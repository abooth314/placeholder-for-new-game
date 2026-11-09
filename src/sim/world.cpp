#include "sim/world.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace curved {
static std::atomic<std::uint32_t> next_epoch{1};
World::World(const Catalog &catalog, const Terrain &terrain, bool populate, unsigned initial_limit)
    : catalog_(catalog), terrain_(terrain), entities_(next_epoch++), relations_(catalog.scenario.relations),
      rng_(terrain.seed() ^ 0x9e3779b9U) {
    population_limit = std::clamp(initial_limit, 4U, static_cast<unsigned>(limits::actors));
    for (int i = 0; i < 4; ++i)
        fortresses_[i] = {
            {(static_cast<RefId>(entities_.epoch()) << 32) | 0x80000000U | static_cast<unsigned>(i), 1,
             entities_.epoch()},
            i,
            terrain_.nodes()[terrain_.homes()[i]].point};
    for (auto &[id, b] : catalog.behaviors)
        policies_.emplace(id, std::make_unique<LuaPolicy>(b));
    projectiles_.reserve(limits::projectiles);
    effects_.reserve(limits::effects);
    events_.reserve(limits::events);
    squads_.reserve(limits::squads);
    deaths_.reserve(limits::actors);
    if (populate) {
        std::vector<std::string> available;
        for (auto &[id, u] : catalog.units)
            if (u.spawn && u.reinforce)
                available.push_back(id);
        if (available.empty())
            throw std::runtime_error("Scenario has no spawn-eligible units");
        for (int team = 0; team < 4; ++team)
            for (int i = 0;
                 i < std::min<unsigned>(catalog.scenario.starting_per_faction, population_limit / 4); ++i) {
                const double angle = std::numbers::pi * (.25 + team * .5) + (i % 4 - 1.5) * .07;
                const auto p = terrain_.space().point(22 + (i / 4) * 2, angle);
                spawn(available[i % available.size()], team, terrain_.surface(p));
            }
    }
    spatial_.rebuild(entities_, terrain_.space());
    replan();
}
double World::random() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_ / 4294967296.0;
}
Squad *World::squad(std::uint32_t id) {
    auto it = std::find_if(squads_.begin(), squads_.end(), [&](auto &s) { return s.id == id; });
    return it == squads_.end() ? nullptr : &*it;
}
Handle World::spawn(const std::string &base, int faction, std::optional<h3::Vec> near) {
    const auto it = catalog_.units.find(base);
    if (it == catalog_.units.end() || !it->second.spawn || faction < 0 || faction > 3)
        return {};
    if (entities_.size() >= std::min<std::size_t>(population_limit, limits::actors)) {
        ++stats_.actor_overflow;
        return {};
    }
    const auto &def = it->second;
    const auto &h = terrain_.space();
    auto p = near.value_or(fortresses_[faction].position);
    if (!h3::valid(p))
        return {};
    if (!near) {
        p = terrain_.surface(h.offset(p, (random() - .5) * 12, 0, (random() - .5) * 12));
    }
    p = terrain_.nodes()[terrain_.nearest(p, terrain_.component(terrain_.homes()[faction]))].point;
    Squad *s = nullptr;
    for (auto &candidate : squads_)
        if (candidate.faction == faction && candidate.members.size() < limits::squad_members &&
            h.dist(candidate.center, p) < 24)
            s = &candidate;
    if (!s) {
        if (squads_.size() >= limits::squads) {
            ++stats_.actor_overflow;
            return {};
        }
        squads_.push_back({next_squad_++, faction, {}, p, p});
        s = &squads_.back();
    }
    const auto handle = entities_.create(base, faction, p, def.health, def.speed);
    if (!handle)
        return {};
    auto *a = entities_.get(handle);
    a->squad = s->id;
    a->yaw = std::atan2(-p.x, -p.z);
    a->think_at = stats_.ticks + ((handle.ref & 0xffffffffU) % 12);
    a->cooldown = random() * .5;
    if (!def.equipment.empty())
        for (auto modifier : catalog_.equipment.at(def.equipment).modifiers)
            a->values.add(std::move(modifier));
    s->members.push_back(handle);
    ++faction_stats_[faction].alive;
    ++faction_stats_[faction].spawned;
    stats_.peak_actors = std::max(stats_.peak_actors, entities_.size());
    plans_invalid_ = true;
    return handle;
}
std::vector<Handle> World::spawn_detachment(const std::string &base, int faction, unsigned count) {
    if (count == 0 || count > 12 ||
        entities_.size() + count > std::min<std::size_t>(population_limit, limits::actors) || faction < 0 ||
        faction > 3 || !catalog_.units.contains(base) || !catalog_.units.at(base).spawn)
        return {};
    // Reserve the necessary company capacity before the atomic detachment.
    std::size_t available = (limits::squads - squads_.size()) * limits::squad_members;
    for (auto &s : squads_)
        if (s.faction == faction)
            available += limits::squad_members - s.members.size();
    if (available < count)
        return {};
    const auto prior_rng = rng_, prior_squad = next_squad_;
    std::vector<Handle> made;
    for (unsigned i = 0; i < count; ++i) {
        auto h = spawn(base, faction);
        if (!h) {
            for (auto prior : made) {
                entities_.destroy(prior);
                --faction_stats_[faction].alive;
                --faction_stats_[faction].spawned;
                for (auto &s : squads_)
                    std::erase(s.members, prior);
            }
            rng_ = prior_rng;
            next_squad_ = prior_squad;
            std::erase_if(squads_, [&](auto &s) { return s.id >= prior_squad && s.members.empty(); });
            return {};
        }
        made.push_back(h);
    }
    return made;
}
int World::relation(int from, int to) const {
    if (from < 0 || from > 3 || to < 0 || to > 3)
        return 0;
    return relations_[from][to];
}
bool World::set_relation(int from, int to, int value, bool mirrored) {
    if (from < 0 || from > 3 || to < 0 || to > 3 || from == to || value < -1 || value > 1)
        return false;
    relations_[from][to] = value;
    if (mirrored)
        relations_[to][from] = value;
    for (auto handle : entities_.live()) {
        auto *a = entities_.get(handle);
        if (auto *target = entities_.get(a->target)) {
            const auto mode = catalog_.attacks.at(catalog_.units.at(a->base).attack).mode;
            if (relation(a->faction, target->faction) != (mode == AttackMode::Heal ? 1 : -1))
                a->target = {};
        }
        if (auto *g = entities_.get(a->guard); g && relation(a->faction, g->faction) != 1)
            a->guard = {};
        a->think_at = 0;
    }
    plans_invalid_ = true;
    return true;
}
bool World::set_preset(const std::string &id) {
    auto it =
        std::find_if(catalog_.presets.begin(), catalog_.presets.end(), [&](auto &p) { return p.id == id; });
    if (it == catalog_.presets.end())
        return false;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (i != j)
                set_relation(i, j, it->relations[i][j]);
    return true;
}
bool World::possess(Handle handle) {
    auto *a = entities_.get(handle);
    if (!a || !catalog_.units.at(a->base).has(Capability::Move))
        return false;
    release();
    possessed_ = handle;
    a->controller = Controller::Player;
    a->state = ActorState::Player;
    a->target = {};
    a->guard = {};
    a->route.clear();
    a->moving = false;
    input_ = {};
    return true;
}
void World::release() {
    if (auto *a = entities_.get(possessed_)) {
        a->controller = Controller::SquadAI;
        a->state = ActorState::Gathering;
        a->moving = false;
        a->think_at = 0;
        a->target = {};
        a->route.clear();
        a->manual_intent = false;
    }
    possessed_ = {};
    input_ = {};
}
void World::player_intent(PlayerIntent intent) {
    if (!std::isfinite(intent.east) || !std::isfinite(intent.north)) {
        input_ = {};
        return;
    }
    const double n = std::hypot(intent.east, intent.north);
    if (n > 1) {
        intent.east /= n;
        intent.north /= n;
    }
    input_ = intent;
}
bool World::move_to(Handle handle, h3::Vec target) {
    auto *a = entities_.get(handle);
    if (!a || !catalog_.units.at(a->base).has(Capability::Move) || !h3::valid(target) ||
        !terrain_.legal(target))
        return false;
    route(*a, terrain_.surface(target));
    return true;
}
bool World::use_primary(Handle handle, Handle target) {
    auto *a = entities_.get(handle);
    if (!a || !combat)
        return false;
    const auto old = a->cooldown;
    attack(*a, target);
    return a->cooldown > old;
}
bool World::add_modifier(Handle h, Modifier m) {
    auto *a = entities_.get(h);
    return a && a->values.add(std::move(m));
}
bool World::remove_modifier(Handle h, const std::string &source) {
    auto *a = entities_.get(h);
    if (!a)
        return false;
    a->values.remove(source);
    return true;
}
bool World::set_value(Handle h, Value v, double n) {
    auto *a = entities_.get(h);
    return a && a->values.set_base(v, n);
}
void World::replan() {
    spatial_.rebuild(entities_, terrain_.space());
    for (auto &s : squads_)
        plan_squad(s);
    plans_invalid_ = false;
}
void World::feedback(const std::string &sound, h3::Vec p, Feedback kind, int faction) {
    if (events_.size() < limits::events)
        events_.push_back({sound, p, kind, faction});
    else
        ++stats_.event_overflow;
    if (effects_.size() < limits::effects && kind != Feedback::Hoof)
        effects_.push_back({p, kind, faction, 0,
                            kind == Feedback::Collapse ? 1.8
                            : kind == Feedback::Heal   ? .8
                                                       : .5});
    else if (kind != Feedback::Hoof)
        ++stats_.effect_overflow;
    stats_.peak_effects = std::max(stats_.peak_effects, effects_.size());
}
void World::finish_deaths() {
    for (auto dead : deaths_) {
        entities_.destroy(dead);
        for (auto &s : squads_)
            std::erase(s.members, dead);
    }
    deaths_.clear();
    std::erase_if(squads_, [](auto &s) { return s.members.empty(); });
    if (possessed_ && !entities_.get(possessed_))
        release();
}
void World::step() {
    const auto started = std::chrono::steady_clock::now();
    events_.clear();
    ++stats_.ticks;
    lua_plans_ = 0;
    for (auto &e : effects_)
        e.age += limits::step;
    std::erase_if(effects_, [](auto &e) { return e.age >= e.life; });
    for (auto &f : fortresses_)
        if (f.ruined && time() >= f.rebuild_at) {
            f.ruined = false;
            f.health = f.max_health;
            ++stats_.rebuilds;
            feedback("core:heal", f.position, Feedback::Heal, f.faction);
        }
    reinforce_clock_ += limits::step * std::clamp(reinforce_rate, 0.0, 3.0);
    if (reinforce_clock_ >= catalog_.scenario.reinforce_seconds) {
        reinforce_clock_ -= catalog_.scenario.reinforce_seconds;
        for (int team = 0; team < 4; ++team)
            if (!fortresses_[team].ruined && faction_stats_[team].alive < (population_limit + 3) / 4) {
                double total = 0;
                for (auto &[id, u] : catalog_.units)
                    if (u.spawn && u.reinforce)
                        total += u.weight;
                double choice = random() * total;
                for (auto &[id, u] : catalog_.units)
                    if (total > 0 && u.spawn && u.reinforce && u.weight > 0 && (choice -= u.weight) <= 0) {
                        spawn(id, team);
                        break;
                    }
            }
    }
    spatial_.rebuild(entities_, terrain_.space());
    for (auto &s : squads_)
        if (plans_invalid_ || stats_.ticks % 23 == s.id % 23)
            plan_squad(s);
    plans_invalid_ = false;
    const auto handles = entities_.live();
    for (auto handle : handles) {
        auto *a = entities_.get(handle);
        if (!a)
            continue;
        a->values.expire(stats_.ticks);
        a->cooldown = std::max(0.0, a->cooldown - limits::step);
        a->flash = std::max(0.0, a->flash - limits::step);
        if (a->controller == Controller::Player) {
            a->state = ActorState::Player;
            const auto &h = terrain_.space();
            const double n = std::hypot(input_.east, input_.north);
            a->moving = n > .01;
            if (a->moving) {
                a->move_goal = terrain_.surface(h.offset(a->position, input_.east * 2, 0, input_.north * 2));
                a->route.clear();
            }
        } else if (stats_.ticks >= a->think_at)
            plan_actor(*a);
        move(*a);
    }
    spatial_.rebuild(entities_, terrain_.space());
    if (combat)
        for (auto handle : handles) {
            auto *a = entities_.get(handle);
            if (!a)
                continue;
            if (a->controller == Controller::Player) {
                if (input_.primary || input_.ability)
                    attack(*a, input_.target);
            } else
                attack(*a, a->target);
        }
    step_projectiles();
    finish_deaths();
    stats_.last_tick_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}
std::uint64_t World::digest() const {
    std::uint64_t hash = 1469598103934665603ULL;
    auto add = [&](std::uint64_t v) {
        hash ^= v;
        hash *= 1099511628211ULL;
    };
    add(rng_);
    add(stats_.ticks);
    for (auto &row : relations_)
        for (int r : row)
            add(r + 1);
    for (auto handle : entities_.live()) {
        auto *a = entities_.get(handle);
        add(handle.ref & 0xffffffff);
        for (char ch : a->base)
            add(static_cast<unsigned char>(ch));
        add(a->faction);
        for (double v : {a->position.t, a->position.x, a->position.y, a->position.z, a->values.health(),
                         a->values.morale, a->cooldown}) {
            std::uint64_t bits;
            std::memcpy(&bits, &v, sizeof(bits));
            add(bits);
        }
    }
    for (auto &p : projectiles_) {
        add(p.source & 0xffffffff);
        add(static_cast<std::uint64_t>(p.age * 1e9));
    }
    for (auto &f : fortresses_)
        add(static_cast<std::uint64_t>(f.health * 1e5));
    return hash;
}
} // namespace curved
