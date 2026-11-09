#include "content/catalog.hpp"
#include "content/lua_policy.hpp"
#include "math/hyperbolic.hpp"
#include "sim/values.hpp"
#include "sim/world.hpp"
#include "world/spatial.hpp"
#include "world/terrain.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <numbers>
#include <stdexcept>

using namespace curved;
namespace fs = std::filesystem;
static int assertions = 0;
static void require(bool v, const char *message) {
    ++assertions;
    if (!v)
        throw std::runtime_error(message);
}
static void near(double a, double b, double tolerance, const char *message) {
    require(std::abs(a - b) < tolerance, message);
}
template <class F> static void rejects(F f, const char *message) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception &) {
        threw = true;
    }
    require(threw, message);
}
static Catalog content(const fs::path &root) {
    return load_catalog(root / "assets", root / "user");
}
static void geometry() {
    for (double L : {8.0, 48.0, 160.0}) {
        h3::Space h(L);
        const auto o = h3::origin();
        for (double r : {1e-8, .03, 12.0, L * 5}) {
            auto p = h.point(r, .7);
            near(h.dist(o, p), r, 2e-7, "known radial distance");
            near(h3::dot(p, p), -1, 2e-10, "unit hyperboloid");
            auto v = h.log(o, p);
            near(h3::norm(v), r, 2e-7, "log magnitude in metres");
            near(h.dist(h.exp(o, v), p), 0, 1e-6, "exp/log inverse");
            auto q = h.interpolate(o, p, .37);
            near(h.dist(o, q), r * .37, 2e-7, "geodesic interpolation has physical fraction");
        }
        auto a = h.point(L * .7, 0, 3), b = h.point(L * .9, std::numbers::pi / 2, 0);
        // Independent hyperbolic right-triangle cosine law, including normal lift.
        const double known = L * std::acosh(std::cosh(.7) * std::cosh(.9) * std::cosh(3 / L));
        near(h.dist(a, b), known, 1e-8, "right triangle cosine law");
        auto f = h.frame(a, .31);
        for (auto v : {f.east, f.up, f.north}) {
            near(h3::dot(a, v), 0, 1e-10, "frame tangent");
            near(h3::norm(v), 1, 1e-10, "frame unit");
            auto w = h.transport(a, b, v);
            near(h3::dot(b, w), 0, 1e-9, "transport tangent");
            near(h3::norm(w), 1, 1e-9, "transport norm");
            auto back = h.transport(b, a, w);
            near(h3::norm(back - v), 0, 1e-7, "transport reversible");
        }
        near(h3::dot(f.east, f.north), 0, 1e-10, "frame orthogonal");
        near(h.dist(h.isometry(a, a, b), b), 0, 1e-7, "isometry maps origin pair");
        auto q = h.point(12, -.8, 2);
        near(h.dist(h.isometry(q, a, b), h.isometry(a, a, b)), h.dist(q, a), 1e-7,
             "isometry preserves distances");
        auto k = h.klein(a);
        near(h.dist(h.ground(a), h.from_klein(k[0], k[1])), 0, 1e-8, "Klein round trip");
        near(h.height(h.lift(o, 9)), 9, 1e-10, "normal height");
        rejects([&] { h.from_klein(1, 0); }, "chart boundary rejected");
        rejects([&] { h.dist(o, {0, 0, 0, 0}); }, "invalid point rejected");
        rejects([&] { h.exp(o, {0, std::numeric_limits<double>::infinity(), 0, 0}); }, "nonfinite rejected");
    }
    rejects([] { h3::Space h(0); }, "invalid curvature rejected");
}
static void terrain(const fs::path &root) {
    auto c = content(root);
    for (auto &recipe : c.maps) {
        Terrain t(recipe, 8147);
        auto &h = t.space();
        int bridges = 0, water = 0, cliffs = 0;
        for (auto &n : t.nodes()) {
            bridges += n.sample.bridge;
            water += n.sample.water;
            cliffs += !n.sample.walkable && !n.sample.water;
        }
        require(recipe.rivers == 0 || bridges > 20, "crossing map has bridge nodes");
        require(recipe.rivers == 0 || water > 100, "crossing map has blocked water");
        require(cliffs > 0, "blocked slopes/boundary exist");
        for (int i = 0; i < 4; ++i) {
            auto route = t.path(t.homes()[i], t.homes()[(i + 1) % 4]);
            require(route.size() > 2, "fortresses connected");
            require(t.route_length(route) + 1e-7 >=
                        h.dist(t.nodes()[route.front()].point, t.nodes()[route.back()].point),
                    "surface route no shorter than ambient geodesic");
            auto p = t.nodes()[route[0]].point;
            for (std::size_t j = 1; j < route.size(); ++j) {
                auto goal = t.nodes()[route[j]].point;
                int count = 0;
                while (h.dist(p, goal) > .002 && count++ < 200) {
                    auto next = t.advance(p, goal, .24);
                    require(next.travelled > 1e-7, "legal route never stalls");
                    require(t.legal(next.point), "movement never enters blocked water/cliff");
                    require(next.travelled <= .24001, "surface speed bound");
                    near(h.height(next.point), t.sample(h.ground(next.point)).height, 1e-8,
                         "movement grounded");
                    p = next.point;
                }
                require(h.dist(p, goal) < .002, "edge arrival");
            }
        }
        if (recipe.rivers) {
            const auto forbidden = h.from_klein(0, .2);
            require(!t.legal(forbidden), "river away from bridges blocked");
        }
    }
}
static void values() {
    ActorValues a(100, 3), b(100, 3);
    a.damage(35);
    near(b.health(), 100, 1e-9, "instances independent");
    require(a.add({"kit", Value::MoveSpeed, .5, 1.2, 0}), "equipment modifier added");
    require(a.add({"spell", Value::MoveSpeed, .5, .8, 10}), "spell added");
    near(a.effective(Value::MoveSpeed), 3.84, 1e-9, "additive then multiplicative");
    a.remove("kit");
    near(a.effective(Value::MoveSpeed), 2.8, 1e-9, "removal preserves other contributor");
    a.add({"kit", Value::MoveSpeed, .5, 1.2, 0});
    a.expire(10);
    near(a.effective(Value::MoveSpeed), 4.2, 1e-9, "expiry preserves permanent contributor");
    a.remove("kit");
    a.set_base(Value::MaxHealth, 50);
    near(a.health(), 50, 1e-9, "lower max clamps current");
    a.set_base(Value::MaxHealth, 150);
    near(a.health(), 50, 1e-9, "increased max does not heal");
    a.set_base(Value::MoveSpeed, 5);
    near(a.health(), 50, 1e-9, "unrelated edit does not heal");
    a.damage(500);
    a.heal(50);
    near(a.health(), 0, 1e-9, "healing does not revive");
    require(!a.add({"bad", Value::MoveSpeed, 0, std::numeric_limits<double>::quiet_NaN(), 0}),
            "invalid modifier rejected");
    near(a.effective(Value::HealthCurrent), a.health(), 1e-9, "read-only health resolves to lifecycle owner");
    require(!a.set_base(Value::HealthCurrent, 99), "read-only value rejects base writes");
}
static void catalog_tests(const fs::path &root) {
    auto c = content(root);
    require(c.units.contains("wardens:lantern_warden"), "external mod unit loaded");
    require(c.packs.size() == 2, "explicit enabled mod loaded");
    require(c.units.at("wardens:road_vanguard").ignore_injured, "authored exception loaded");
    const auto fixture = root / "out/tests/invalid_content";
    fs::create_directories(fixture / "mods/broken");
    std::ofstream(fixture / "enabled-mods.json") << R"({"schema":1,"enabled":["broken"]})";
    std::ofstream(fixture / "mods/broken/pack.json")
        << R"({"schema":1,"api":1,"id":"broken","dependencies":["core"],"files":["defs.json"]})";
    std::ofstream(fixture / "mods/broken/defs.json")
        << R"({"schema":1,"units":[{"id":"broken:oops","name":"Bad","attack":"missing:attack","appearance":"core:soldier","capabilities":["move","melee"]}]})";
    rejects([&] { c = load_catalog(root / "assets", fixture); }, "invalid candidate rejected");
    try {
        load_catalog(root / "assets", fixture);
    } catch (const std::exception &error) {
        const std::string message = error.what();
        require(message.find("defs.json") != std::string::npos &&
                    message.find("broken:oops") != std::string::npos,
                "cross-reference errors identify owning file and definition");
    }
    require(c.units.contains("wardens:lantern_warden"), "invalid load leaves published catalog intact");
    std::ofstream(fixture / "mods/broken/defs.json") << R"({"schema":1,"schema":2})";
    rejects([&] { load_catalog(root / "assets", fixture); }, "duplicate JSON key rejected");
    std::ofstream(fixture / "mods/broken/pack.json")
        << R"({"schema":1,"api":1,"id":"broken","dependencies":["core"],"files":["../escape.json"]})";
    rejects([&] { load_catalog(root / "assets", fixture); }, "path escape rejected");
    std::ifstream builtin(root / "assets/core/definitions.json");
    nlohmann::json original;
    builtin >> original;
    auto unit = original["units"][0];
    unit["health"] = 217;
    auto faction = original["factions"][0];
    faction["short"] = "Reforged";
    auto map = original["maps"][0];
    map["name"] = "Reforged March";
    auto preset = original["presets"][0];
    preset["name"] = "Modded armistice";
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            preset["relations"][i][j] = i == j ? 1 : 0;
    auto scenario = original["scenario"];
    scenario["starting_per_faction"] = 8;
    scenario["relations"] = preset["relations"];
    nlohmann::json replacement = {{"schema", 1},   {"units", {unit}},     {"factions", {faction}},
                                  {"maps", {map}}, {"presets", {preset}}, {"scenario", scenario}};
    std::ofstream(fixture / "mods/broken/defs.json") << replacement.dump();
    std::ofstream(fixture / "mods/broken/pack.json")
        << R"({"schema":1,"api":1,"id":"broken","dependencies":["core"],"replace":true,"files":["defs.json"]})";
    auto replaced = load_catalog(root / "assets", fixture);
    require(replaced.units.at("core:swordsman").health == 217 &&
                replaced.factions[0].short_name == "Reforged",
            "whole-record unit/faction replacements preserve stable identities");
    require(replaced.maps[0].name == "Reforged March" && replaced.presets[0].name == "Modded armistice",
            "map/preset replacement preserves record order");
    require(replaced.scenario.starting_per_faction == 8, "scenario replaced as one complete record");
    std::ofstream(fixture / "mods/broken/pack.json")
        << R"({"schema":1,"api":1,"id":"broken","dependencies":["core"],"replace":false,"files":["defs.json"]})";
    rejects([&] { load_catalog(root / "assets", fixture); },
            "all replacement categories require explicit permission");
}
static void lua_tests(const fs::path &root) {
    auto c = content(root);
    LuaPolicy p(c.behaviors.at("wardens:guardian_policy"));
    PolicyContext ctx{
        11, 100, 100, false, {{12, 9, 30, 78, true, false, true}, {13, 5, 100, 100, false, true, false}}};
    auto d = p.plan(ctx);
    require(d.intent == PolicyIntent::Guard && d.target == 12, "Lua chooses injured allied support");
    ctx.nearby[0].health = 78;
    require(p.plan(ctx).intent == PolicyIntent::Advance, "Lua returns to advance when support healthy");
    ctx.ignore_injured = true;
    require(p.plan(ctx).intent == PolicyIntent::Flank, "authored exception changes decision");
    LuaPolicy runaway({"test:runaway", "return function() while true do end end", "test"});
    rejects([&] { runaway.plan(ctx); }, "runtime instruction budget enforced");
    rejects([&] { LuaPolicy p2({"test:loop", "while true do end", "test"}); },
            "load-time instruction budget enforced");
    LuaPolicy memory({"test:memory", "return function() local a=string.rep('x',4000000) end", "test"});
    rejects([&] { memory.plan(ctx); }, "memory budget enforced without host crash");
    LuaPolicy forbidden({"test:forbidden", "return function() os.execute('bad') end", "test"});
    rejects([&] { forbidden.plan(ctx); }, "OS unavailable");
    require(p.memory_bytes() < 2 * 1024 * 1024, "policy within memory budget");
}
static void identity(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    auto w = std::make_unique<World>(c, t, false);
    w->reinforce_rate = 0;
    auto a = w->spawn("core:swordsman", 0, t.surface(h3::origin())),
         b = w->spawn("core:swordsman", 0, t.surface(h3::origin()));
    require(a.ref != b.ref, "independent RefIds share BaseId");
    w->damage_actor(a, 25);
    near(w->inspect(b)->values.health(), 115, 1e-9, "live instance values independent");
    w->damage_actor(a, 1000);
    require(!w->inspect(a) && !w->resolve(a.ref), "dead actor immediately not addressable");
    w->step();
    auto new_actor = w->spawn("core:swordsman", 0, t.surface(h3::origin()));
    require(new_actor.ref != a.ref && new_actor.generation != a.generation,
            "slot reuse increments generation and independent RefId");
    require(!w->set_value(a, Value::MoveSpeed, 12), "stale mutation rejected");
    auto other = std::make_unique<World>(c, t, false);
    require(!other->inspect(new_actor), "foreign epoch rejected");
}
static void spatial() {
    h3::Space h(32);
    Entities e(912);
    for (int i = 0; i < 200; ++i)
        e.create("test:unit", i % 4, h.point((i * 37) % 100, i * 2.399963, (i % 7 - 3) * 3), 100, 3);
    SpatialIndex index;
    index.rebuild(e, h);
    for (int i = 0; i < 30; ++i) {
        auto p = h.point(i * 2.1, i * .7, 5);
        const double range = 3 + i % 24;
        const auto candidates = index.query(p, range, h);
        for (auto handle : e.live())
            if (h.dist(p, e.get(handle)->position) <= range)
                require(std::find(candidates.begin(), candidates.end(), handle) != candidates.end(),
                        "conservative chart candidates cover every intrinsic neighbor");
    }
}
static void combat_tests(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    const auto p = t.surface(h3::origin());
    auto &h = t.space();
    for (auto base : {"core:swordsman", "core:archer", "core:cleric", "core:catapult"}) {
        auto w = std::make_unique<World>(c, t, false);
        w->reinforce_rate = 0;
        auto a = w->spawn(base, 0, p);
        w->possess(a);
        w->combat = false;
        for (int i = 0; i < 20; ++i)
            w->step();
        const bool heal = std::string(base) == "core:cleric";
        const double distance = std::string(base) == "core:swordsman" ? 1.8 : 12;
        auto b = w->spawn("core:swordsman", heal ? 0 : 1, t.surface(h.offset(p, distance, 0, 0)));
        if (heal)
            w->damage_actor(b, 50);
        w->replan();
        w->combat = true;
        const double before = w->inspect(b)->values.health();
        require(w->use_primary(a, b), "primary native capability executes");
        for (int i = 0; i < 100; ++i)
            w->step();
        if (heal)
            require(w->inspect(b) && w->inspect(b)->values.health() > before,
                    "healing consumer restored health");
        else
            require(!w->inspect(b) || w->inspect(b)->values.health() < before,
                    "melee/projectile/siege consumer damaged target");
        if (std::string(base) == "core:archer" || std::string(base) == "core:catapult")
            require(w->stats().shots > 0 && w->stats().hits > 0, "projectile flight and impact exercised");
    }
    auto w = std::make_unique<World>(c, t, false);
    w->reinforce_rate = 0;
    auto fortress = w->fortresses()[1].handle;
    require(w->damage_fortress(fortress, 10000, 0), "fortress damage uses enemy eligibility");
    require(w->fortresses()[1].ruined, "fortress fell");
    for (int i = 0; i < 962; ++i)
        w->step();
    require(!w->fortresses()[1].ruined && w->stats().rebuilds == 1, "bounded native rebuild lifecycle");
}
static void diplomacy(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    auto w = std::make_unique<World>(c, t, false);
    w->reinforce_rate = 0;
    const auto p = t.surface(h3::origin());
    auto a = w->spawn("core:archer", 0, p);
    w->possess(a);
    w->combat = false;
    for (int i = 0; i < 20; ++i)
        w->step();
    auto b = w->spawn("core:swordsman", 1, t.surface(t.space().offset(p, 14, 0, 0)));
    w->replan();
    w->combat = true;
    require(w->use_primary(a, b) && !w->projectiles().empty(), "projectile in flight before diplomacy edit");
    const auto hp = w->inspect(b)->values.health();
    w->set_relation(0, 1, 0);
    require(w->relation(1, 0) == -1, "asymmetric outgoing diplomacy preserved");
    require(!w->inspect(a)->target, "invalid existing target cleared immediately");
    for (int i = 0; i < 100; ++i)
        w->step();
    near(w->inspect(b)->values.health(), hp, 1e-9, "in-flight projectile respects changed diplomacy");
    w->set_relation(0, 1, 1);
    require(w->relation(0, 1) == 1 && w->relation(1, 0) == -1, "asymmetric ally allowed");
    w->set_relation(2, 3, 1, true);
    require(w->relation(3, 2) == 1, "mirrored changes explicit");
    require(!w->set_relation(0, 0, -1), "own faction always allied");
}
static void tactics(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    auto w = std::make_unique<World>(c, t, false);
    w->reinforce_rate = 0;
    w->combat = false;
    const auto p = t.surface(h3::origin());
    auto &h = t.space();
    for (int i = 0; i < 2; ++i)
        w->spawn("core:swordsman", 0, p);
    for (int i = 0; i < 12; ++i)
        w->spawn("core:swordsman", 1, t.surface(h.offset(p, 10, 0, 0)));
    w->replan();
    auto state = [&]() {
        for (auto &s : w->squads())
            if (s.faction == 0)
                return s.state;
        return ActorState::Player;
    };
    require(state() == ActorState::Retreating, "outnumbered company retreats");
    w->set_relation(0, 2, 1);
    std::vector<Handle> allies;
    for (int i = 0; i < 18; ++i)
        allies.push_back(w->spawn("core:swordsman", 2, t.surface(h.offset(p, -7, 0, 0))));
    w->replan();
    require(state() != ActorState::Retreating, "nearby allied force changes tactical decision");
    for (auto ally : allies)
        w->damage_actor(ally, 1000);
    w->replan();
    require(state() == ActorState::Retreating, "removing allied support reverses decision");
    auto lua_world = std::make_unique<World>(c, t, false);
    lua_world->reinforce_rate = 0;
    auto ward = lua_world->spawn("wardens:lantern_warden", 0, p);
    auto patient = lua_world->spawn("core:cleric", 0, t.surface(h.offset(p, 7, 0, 0)));
    lua_world->damage_actor(patient, 35);
    for (int i = 0; i < 14; ++i)
        lua_world->step();
    require(lua_world->inspect(ward) && lua_world->inspect(ward)->guard == patient,
            "enabled mod behavior changes real actor intent");
    lua_world->set_relation(0, 1, 1);
    require(lua_world->stats().lua_calls > 0 && lua_world->stats().lua_errors == 0,
            "native world executes bounded Lua policy");
}
static void possession(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    auto w = std::make_unique<World>(c, t, false);
    w->reinforce_rate = 0;
    w->combat = false;
    auto a = w->spawn("wardens:lantern_warden", 0, t.surface(h3::origin()));
    w->damage_actor(a, 20);
    w->add_modifier(a, {"spell:haste", Value::MoveSpeed, 1, 1.2, 300});
    const auto hp = w->inspect(a)->values.health();
    const auto speed = w->inspect(a)->values.effective(Value::MoveSpeed);
    require(w->possess(a), "can possess compatible live unit");
    const auto before = w->inspect(a)->position;
    w->player_intent({1, 0, false, false, {}});
    for (int i = 0; i < 30; ++i)
        w->step();
    require(t.space().dist(before, w->inspect(a)->position) > speed * .94,
            "possessed movement uses effective actor speed");
    near(w->inspect(a)->values.velocity, speed, .03, "inspected actual velocity matches movement limit");
    near(w->inspect(a)->values.health(), hp, 1e-9, "controller transfer preserves health");
    w->release();
    require(w->inspect(a)->controller == Controller::SquadAI && w->inspect(a)->handle == a,
            "release returns same instance to AI");
    require(w->inspect(a)->values.modifiers().size() == 2,
            "equipment and spell preserved through possession");
    w->possess(a);
    w->damage_actor(a, 10000);
    w->step();
    require(!w->possessed() && !w->inspect(a), "death safely releases possession");
}
static void replay(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    auto a = std::make_unique<World>(c, t), b = std::make_unique<World>(c, t);
    for (int i = 0; i < 180; ++i) {
        a->step();
        b->step();
        require(a->digest() == b->digest(), "same-build seeded fixed-step replay agrees across world epochs");
    }
    require(a->stats().peak_actors >= 64, "populated battle exercised");
    require(a->stats().lua_calls > 0, "mod present in populated battle");
    for (auto handle : a->actors())
        require(t.legal(a->inspect(handle)->position), "all populated actors on legal surface");
}
static void bridge_world(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[1], 8147);
    auto w = std::make_unique<World>(c, t, false);
    w->reinforce_rate = 0;
    w->combat = false;
    auto handle = w->spawn("core:swordsman", 0, t.nodes()[t.homes()[0]].point);
    require(w->possess(handle), "bridge traversal uses a possessed native body");
    const auto route = t.path(t.homes()[0], t.homes()[1]);
    auto &h = t.space();
    int bridges = 0;
    for (std::size_t i = 1; i < route.size(); ++i) {
        const auto goal = t.nodes()[route[i]].point;
        int steps = 0;
        while (h.dist(w->inspect(handle)->position, goal) > .18 && steps++ < 500) {
            const auto *a = w->inspect(handle);
            const auto v = h.log(a->position, goal);
            const auto f = h.frame(a->position);
            const double x = h3::dot(v, f.east), z = h3::dot(v, f.north),
                         n = std::max(.001, std::hypot(x, z));
            w->player_intent({x / n, z / n, false, false, {}});
            w->step();
            const auto p = w->inspect(handle)->position;
            const auto sample = t.sample(p);
            require(sample.walkable && !sample.water && h.height(p) >= .35,
                    "native body stays above blocked water on legal terrain");
            require(w->inspect(handle)->values.velocity <=
                        w->inspect(handle)->values.effective(Value::MoveSpeed) + .01,
                    "native bridge movement obeys effective limit");
            bridges += sample.bridge;
        }
        require(h.dist(w->inspect(handle)->position, goal) <= .18,
                "native body reaches each constrained waypoint");
    }
    require(bridges > 10, "native actor actually traversed bridge interiors");
    require(w->stats().path_failures == 0, "no disconnected navigation goals");
}
static void budgets(const fs::path &root) {
    auto c = content(root);
    Terrain t(c.maps[0], 8147);
    auto w = std::make_unique<World>(c, t, false, 40);
    w->reinforce_rate = 0;
    const auto p = t.surface(h3::origin());
    for (int i = 0; i < 40; ++i)
        require(static_cast<bool>(w->spawn("core:swordsman", i % 4, p)), "fill requested actor limit");
    require(!w->spawn("core:swordsman", 0, p), "actor overflow rejected");
    const auto before = w->digest();
    require(w->spawn_detachment("core:archer", 0, 8).empty(),
            "detachment rejected atomically when budget insufficient");
    require(w->digest() == before && w->actors().size() == 40, "failed detachment changes no gameplay state");
    for (auto handle : w->actors())
        w->damage_actor(handle, 1000);
    w->step();
    require(w->actors().empty() && !w->possessed(), "cleanup reclaims all actors and control");
    auto reinforcements = std::make_unique<World>(c, t, false, 40);
    reinforcements->damage_fortress(reinforcements->fortresses()[1].handle, 10000, 0);
    for (int i = 0; i < 90; ++i)
        reinforcements->step();
    require(reinforcements->factions()[0].spawned > 0 && reinforcements->factions()[1].spawned == 0,
            "native reinforcements respect ruined fortress");
}
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Expected test name and project root");
        const std::string test = argv[1];
        const fs::path root = argv[2];
        if (test == "geometry")
            geometry();
        else if (test == "terrain")
            terrain(root);
        else if (test == "values")
            values();
        else if (test == "content")
            catalog_tests(root);
        else if (test == "lua")
            lua_tests(root);
        else if (test == "identity")
            identity(root);
        else if (test == "spatial")
            spatial();
        else if (test == "combat")
            combat_tests(root);
        else if (test == "diplomacy")
            diplomacy(root);
        else if (test == "tactics")
            tactics(root);
        else if (test == "possession")
            possession(root);
        else if (test == "replay")
            replay(root);
        else if (test == "bridge_world")
            bridge_world(root);
        else if (test == "budgets")
            budgets(root);
        else
            throw std::runtime_error("Unknown test case");
        std::cout << "PASS " << test << " (" << assertions << " assertions)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << " (after " << assertions << " assertions)\n";
        return 1;
    }
}
