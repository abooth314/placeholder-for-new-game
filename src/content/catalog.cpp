#include "content/catalog.hpp"
#include "content/lua_policy.hpp"
#include "core/path.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>

namespace curved {
using Json = nlohmann::json;
namespace fs = std::filesystem;
static void keys(const Json &j, std::initializer_list<const char *> allowed) {
    if (!j.is_object())
        throw std::runtime_error("Expected object");
    for (auto it = j.begin(); it != j.end(); ++it) {
        bool found = false;
        for (auto key : allowed)
            if (it.key() == key)
                found = true;
        if (!found)
            throw std::runtime_error("Unknown field: " + it.key());
    }
}
static std::string text_file(const fs::path &file, std::size_t max = 2 * 1024 * 1024) {
    if (!fs::is_regular_file(file) || fs::file_size(file) > max)
        throw std::runtime_error("Missing or oversized file: " + utf8_path(file));
    std::ifstream stream(file, std::ios::binary);
    std::ostringstream s;
    s << stream.rdbuf();
    if (!stream.good() && !stream.eof())
        throw std::runtime_error("Read failed: " + utf8_path(file));
    return s.str();
}
static Json read_json(const fs::path &path) {
    std::vector<std::set<std::string>> stack;
    return Json::parse(text_file(path), [&](int, Json::parse_event_t event, Json &parsed) {
        if (event == Json::parse_event_t::object_start)
            stack.emplace_back();
        if (event == Json::parse_event_t::key && !stack.back().insert(parsed.get<std::string>()).second)
            throw std::runtime_error("Duplicate JSON key: " + parsed.get<std::string>());
        if (event == Json::parse_event_t::object_end)
            stack.pop_back();
        return true;
    });
}
static std::string id(const Json &j) {
    const auto s = j.at("id").get<std::string>();
    const auto p = s.find(':');
    if (p == std::string::npos || p == 0 || p + 1 == s.size() || s.size() > 96 ||
        s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789:_-") != std::string::npos)
        throw std::runtime_error("Invalid namespaced BaseId: " + s);
    return s;
}
static double number(const Json &j, const char *key, double low, double high, double fallback) {
    const double n = j.value(key, fallback);
    if (!std::isfinite(n) || n < low || n > high)
        throw std::runtime_error(std::string("Out-of-range ") + key);
    return n;
}
static std::array<float, 3> triple(const Json &j, const char *key, float low, float high) {
    const auto a = j.at(key).get<std::vector<float>>();
    if (a.size() != 3)
        throw std::runtime_error(std::string(key) + " needs 3 entries");
    for (float n : a)
        if (!std::isfinite(n) || n < low || n > high)
            throw std::runtime_error(std::string("Invalid ") + key);
    return {a[0], a[1], a[2]};
}
unsigned capability_from_name(const std::string &n) {
    static const std::map<std::string, Capability> caps = {
        {"move", Capability::Move}, {"melee", Capability::Melee}, {"projectile", Capability::Projectile},
        {"heal", Capability::Heal}, {"siege", Capability::Siege}, {"guard", Capability::Guard}};
    auto it = caps.find(n);
    if (it == caps.end())
        throw std::runtime_error("Unknown capability: " + n);
    return static_cast<unsigned>(it->second);
}
template <class T>
static void insert(std::map<std::string, T> &map, T def, bool replace, std::size_t budget = 64) {
    const auto key = def.id;
    if (map.contains(key) && !replace)
        throw std::runtime_error("Duplicate definition without replace permission: " + key);
    if (!map.contains(key) && map.size() >= budget)
        throw std::runtime_error("Definition budget exceeded: " + key);
    map.insert_or_assign(key, std::move(def));
}
static auto relations(const Json &j) {
    auto r = j.get<std::array<std::array<int, 4>, 4>>();
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k)
            if (r[i][k] < -1 || r[i][k] > 1 || (i == k && r[i][k] != 1))
                throw std::runtime_error("Invalid directional diplomacy matrix");
    return r;
}
template <class T>
static void insert_records(std::vector<T> &records, T record, bool replace, std::size_t budget) {
    auto found =
        std::find_if(records.begin(), records.end(), [&](const T &old) { return old.id == record.id; });
    if (found != records.end()) {
        if (!replace)
            throw std::runtime_error("Duplicate definition without replace permission: " + record.id);
        *found = std::move(record);
    } else {
        if (records.size() >= budget)
            throw std::runtime_error("Definition budget exceeded: " + record.id);
        records.push_back(std::move(record));
    }
}
Catalog load_catalog(const fs::path &assets, const fs::path &user) {
    Catalog c;
    bool have_scenario = false;
    std::map<std::string, std::string> origins;
    const auto enabled = read_json(user / "enabled-mods.json");
    keys(enabled, {"schema", "enabled"});
    if (enabled.at("schema") != 1)
        throw std::runtime_error("Unsupported enabled-mod schema");
    std::vector<fs::path> dirs = {assets / "core"};
    std::set<std::string> enabled_names;
    for (auto &item : enabled.at("enabled")) {
        auto name = item.get<std::string>();
        if (name.empty() ||
            name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos ||
            !enabled_names.insert(name).second)
            throw std::runtime_error("Invalid/duplicate enabled mod folder");
        dirs.push_back(user / "mods" / name);
    }
    if (dirs.size() > 16)
        throw std::runtime_error("Too many enabled packs");
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto &dir : dirs) {
        std::string context = utf8_path(dir / "pack.json");
        try {
            const auto manifest = read_json(dir / "pack.json");
            keys(manifest, {"schema", "api", "id", "dependencies", "replace", "files"});
            if (manifest.at("schema") != 1 || manifest.at("api") != 1)
                throw std::runtime_error("Unsupported pack schema/API");
            const std::string pack = manifest.at("id");
            if (pack.empty() ||
                pack.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos ||
                std::find(c.packs.begin(), c.packs.end(), pack) != c.packs.end())
                throw std::runtime_error("Invalid/duplicate pack identity");
            for (const auto &dep : manifest.at("dependencies"))
                if (std::find(c.packs.begin(), c.packs.end(), dep.get<std::string>()) == c.packs.end())
                    throw std::runtime_error("Dependency must be loaded first: " + dep.get<std::string>());
            const bool replace = manifest.value("replace", false);
            if (manifest.at("files").size() > 16)
                throw std::runtime_error("Pack file budget exceeded");
            auto inside = [&](const std::string &relative) {
                const fs::path p = fs::u8path(relative);
                if (p.is_absolute())
                    throw std::runtime_error("Absolute content path forbidden");
                const auto base = fs::weakly_canonical(dir), full = fs::weakly_canonical(dir / p);
                const auto rel = full.lexically_relative(base);
                if (rel.empty() || *rel.begin() == "..")
                    throw std::runtime_error("Content path escapes pack");
                return full;
            };
            for (auto &name : manifest.at("files")) {
                const fs::path file = inside(name.get<std::string>());
                context = utf8_path(file);
                const auto data = read_json(file);
                keys(data, {"schema", "factions", "attacks", "units", "visuals", "sounds", "behaviors",
                            "equipment", "maps", "scenario", "presets"});
                if (data.at("schema") != 1)
                    throw std::runtime_error("Unsupported definition schema");
                for (char ch : data.dump()) {
                    hash ^= static_cast<unsigned char>(ch);
                    hash *= 1099511628211ULL;
                }
                auto rows = [&](const char *key) -> Json {
                    auto records = data.contains(key) ? data.at(key) : Json::array();
                    if (!records.is_array() || records.size() > 64)
                        throw std::runtime_error(std::string("Invalid/oversized record collection: ") + key);
                    return records;
                };
                for (const char *category : {"factions", "attacks", "units", "visuals", "sounds", "behaviors",
                                             "equipment", "maps", "presets"})
                    for (const auto &record : rows(category))
                        origins[std::string(category) + "/" + id(record)] =
                            utf8_path(file) + " [" + id(record) + "]";
                for (auto &d : rows("factions")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "name", "short", "color"});
                    context = utf8_path(file) + " [" + id(d) + "]";
                    insert_records(c.factions,
                                   FactionDef{id(d), d.at("name"), d.at("short"), triple(d, "color", 0, 1)},
                                   replace, 4);
                }
                for (auto &d : rows("attacks")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "mode", "damage", "range", "cooldown", "flight_speed", "arc", "splash",
                             "building_multiplier", "sound"});
                    context = utf8_path(file) + " [" + id(d) + "]";
                    const std::string mode = d.at("mode");
                    if (mode != "melee" && mode != "projectile" && mode != "heal")
                        throw std::runtime_error("Unknown attack primitive");
                    insert(c.attacks,
                           AttackDef{id(d), d.at("sound"),
                                     mode == "melee"  ? AttackMode::Melee
                                     : mode == "heal" ? AttackMode::Heal
                                                      : AttackMode::Projectile,
                                     number(d, "damage", 0, 500, 10), number(d, "range", .5, 60, 2),
                                     number(d, "cooldown", .1, 30, 1), number(d, "flight_speed", 5, 80, 26),
                                     number(d, "arc", 0, .5, .1), number(d, "splash", 0, 10, 0),
                                     number(d, "building_multiplier", 0, 10, 1)},
                           replace);
                }
                for (auto &d : rows("units")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "name", "attack", "appearance", "behavior", "equipment", "capabilities",
                             "health", "speed", "formation_depth", "radius", "weight", "discover", "menu",
                             "spawn", "reinforce", "ignore_injured"});
                    context = utf8_path(file) + " [" + id(d) + "]";
                    UnitDef u;
                    u.id = id(d);
                    u.name = d.at("name");
                    if (u.name.empty() || u.name.size() > 64)
                        throw std::runtime_error("Invalid display name");
                    u.attack = d.at("attack");
                    u.appearance = d.at("appearance");
                    u.behavior = d.value("behavior", "");
                    u.equipment = d.value("equipment", "");
                    for (auto &cap : d.at("capabilities"))
                        u.capabilities |= capability_from_name(cap.get<std::string>());
                    u.health = number(d, "health", 1, 10000, 100);
                    u.speed = number(d, "speed", 0, 20, 3);
                    u.formation_depth = number(d, "formation_depth", -20, 5, 0);
                    u.radius = number(d, "radius", .2, 3, .6);
                    u.weight = number(d, "weight", 0, 20, 1);
                    u.discover = d.value("discover", true);
                    u.menu = d.value("menu", true);
                    u.spawn = d.value("spawn", true);
                    u.reinforce = d.value("reinforce", true);
                    u.ignore_injured = d.value("ignore_injured", false);
                    insert(c.units, std::move(u), replace);
                }
                for (auto &d : rows("visuals")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "parts"});
                    context = utf8_path(file) + " [" + id(d) + "]";
                    VisualRecipe recipe{id(d), {}};
                    if (d.at("parts").empty() || d.at("parts").size() > 160)
                        throw std::runtime_error("Mesh part budget exceeded");
                    for (auto &p : d.at("parts")) {
                        keys(p, {"primitive", "center", "size", "color", "tint", "motion", "sides", "end"});
                        const std::string primitive = p.at("primitive");
                        Primitive k;
                        if (primitive == "box")
                            k = Primitive::Box;
                        else if (primitive == "cone")
                            k = Primitive::Cone;
                        else if (primitive == "roof")
                            k = Primitive::Roof;
                        else if (primitive == "beam")
                            k = Primitive::Beam;
                        else if (primitive == "ellipsoid")
                            k = Primitive::Ellipsoid;
                        else
                            throw std::runtime_error("Unknown geometry evaluator");
                        PartRecipe part{k,
                                        triple(p, "center", -20, 20),
                                        triple(p, "size", .005f, 30),
                                        triple(p, "color", 0, 1),
                                        {},
                                        static_cast<float>(number(p, "tint", 0, 1, 0)),
                                        static_cast<float>(number(p, "motion", 0, 3, 0)),
                                        static_cast<int>(number(p, "sides", 3, 16, 7))};
                        if (k == Primitive::Beam)
                            part.end = triple(p, "end", -20, 20);
                        recipe.parts.push_back(part);
                    }
                    insert(c.visuals, std::move(recipe), replace);
                }
                for (auto &d : rows("sounds")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "duration", "frequency", "end_frequency", "noise", "tone", "gain",
                             "priority"});
                    context = utf8_path(file) + " [" + id(d) + "]";
                    insert(c.sounds,
                           SoundRecipe{id(d), number(d, "duration", .02, 2, .2),
                                       number(d, "frequency", 15, 12000, 400),
                                       number(d, "end_frequency", 15, 12000, 120),
                                       number(d, "noise", 0, 1, .3), number(d, "tone", 0, 1, .5),
                                       number(d, "gain", 0, 1, .4),
                                       static_cast<int>(number(d, "priority", 0, 10, 1))},
                           replace, 32);
                }
                for (auto &d : rows("behaviors")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "file"});
                    const auto path = inside(d.at("file"));
                    context = utf8_path(path) + " [" + id(d) + "]";
                    BehaviorDef b{id(d), text_file(path, 65536), utf8_path(path)};
                    LuaPolicy::validate(b);
                    for (char ch : b.source) {
                        hash ^= static_cast<unsigned char>(ch);
                        hash *= 1099511628211ULL;
                    }
                    insert(c.behaviors, std::move(b), replace, 16);
                }
                for (auto &d : rows("equipment")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "modifiers"});
                    EquipmentDef e{id(d), {}};
                    for (auto &m : d.at("modifiers")) {
                        keys(m, {"value", "additive", "multiplier"});
                        auto v = ActorValues::resolve(m.at("value").get<std::string>());
                        if (!v || !ActorValues::writable(*v))
                            throw std::runtime_error("Unknown or read-only equipment actor value");
                        e.modifiers.push_back({"equipment:" + e.id, *v, number(m, "additive", -1000, 1000, 0),
                                               number(m, "multiplier", 0, 10, 1), 0});
                    }
                    if (e.modifiers.size() > 8)
                        throw std::runtime_error("Equipment modifier budget");
                    insert(c.equipment, std::move(e), replace);
                }
                for (auto &d : rows("maps")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "name", "description", "radius", "relief", "rivers", "grass", "water"});
                    insert_records(c.maps,
                                   MapRecipe{id(d), d.at("name"), d.at("description"),
                                             number(d, "radius", 40, 90, 70), number(d, "relief", 0, 8, 3),
                                             static_cast<int>(number(d, "rivers", 0, 3, 0)),
                                             triple(d, "grass", 0, 1), triple(d, "water", 0, 1)},
                                   replace, 12);
                }
                if (data.contains("scenario")) {
                    context = utf8_path(file) + " [scenario]";
                    if (have_scenario && !replace)
                        throw std::runtime_error("Scenario replacement requires replace:true");
                    auto &s = data.at("scenario");
                    keys(s, {"starting_per_faction", "reinforce_seconds", "relations"});
                    c.scenario = {static_cast<int>(number(s, "starting_per_faction", 4, 48, 16)),
                                  number(s, "reinforce_seconds", .5, 30, 2.5), relations(s.at("relations"))};
                    have_scenario = true;
                }
                for (auto &d : rows("presets")) {
                    context = utf8_path(file) + " [" + id(d) + "]";
                    keys(d, {"id", "name", "relations"});
                    insert_records(c.presets,
                                   DiplomacyPreset{id(d), d.at("name"), relations(d.at("relations"))},
                                   replace, 16);
                }
            }
            c.packs.push_back(pack);
        } catch (const std::exception &e) {
            throw std::runtime_error(context + ": " + e.what());
        }
    }
    if (c.factions.size() != 4 || c.maps.empty() || c.maps.size() > 12 || c.units.empty() ||
        c.units.size() > 64 || c.attacks.size() > 64 || c.visuals.size() > 64 || c.sounds.size() > 32 ||
        c.behaviors.size() > 16)
        throw std::runtime_error("Catalog shape/budget invalid");
    for (int i = 0; i < 4; ++i)
        if (c.scenario.relations[i][i] != 1)
            throw std::runtime_error("A validated scenario with self-allied factions is required");
    std::set<std::string> unique;
    for (auto &f : c.factions)
        if (!unique.insert(f.id).second)
            throw std::runtime_error("Duplicate faction");
    unique.clear();
    for (auto &m : c.maps)
        if (!unique.insert(m.id).second)
            throw std::runtime_error("Duplicate map");
    for (auto &[key, u] : c.units) {
        if (!c.attacks.contains(u.attack) || !c.visuals.contains(u.appearance) ||
            (!u.behavior.empty() && !c.behaviors.contains(u.behavior)) ||
            (!u.equipment.empty() && !c.equipment.contains(u.equipment)))
            throw std::runtime_error(origins.at("units/" + key) + ": unresolved definition reference");
        const auto mode = c.attacks.at(u.attack).mode;
        const auto cap = mode == AttackMode::Melee  ? Capability::Melee
                         : mode == AttackMode::Heal ? Capability::Heal
                                                    : Capability::Projectile;
        if (!u.has(cap) || !u.has(Capability::Move) ||
            (u.has(Capability::Siege) && mode != AttackMode::Projectile))
            throw std::runtime_error(origins.at("units/" + key) + ": incompatible capability composition");
    }
    for (auto &[key, a] : c.attacks)
        if (!c.sounds.contains(a.sound))
            throw std::runtime_error(origins.at("attacks/" + key) + ": unresolved sound recipe");
    for (const char *required :
         {"core:castle", "core:tree", "core:house", "core:bridge", "core:projectile", "core:effect"})
        if (!c.visuals.contains(required))
            throw std::runtime_error(std::string("Missing scene recipe: ") + required);
    for (const char *required : {"core:impact", "core:heal", "core:collapse", "core:hoof", "core:magic"})
        if (!c.sounds.contains(required))
            throw std::runtime_error(std::string("Missing event sound: ") + required);
    std::ostringstream fingerprint;
    fingerprint << std::hex << hash;
    c.fingerprint = fingerprint.str();
    return c;
}
} // namespace curved
