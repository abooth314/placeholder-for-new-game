#include "sim/world.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace curved {
void World::plan_squad(Squad &s) {
    const auto &h = terrain_.space();
    h3::Vec sum{};
    double hp = 0, max_hp = 0;
    std::size_t count = 0;
    for (auto handle : s.members)
        if (auto *a = entities_.get(handle)) {
            sum = sum + a->position;
            hp += a->values.health();
            max_hp += a->values.effective(Value::MaxHealth);
            ++count;
        }
    if (!count)
        return;
    const double len = std::sqrt(std::max(1e-10, -h3::dot(sum, sum)));
    s.center = terrain_.surface(sum * (1 / len));
    double friendly = 0, hostile = 0, best = std::numeric_limits<double>::infinity();
    const Actor *threat = nullptr;
    for (auto handle : spatial_.query(s.center, 32, h))
        if (auto *a = entities_.get(handle)) {
            const double d = h.dist(s.center, a->position);
            if (d > 32)
                continue;
            const double strength = (.5 + std::sqrt(a->values.health() / 85)) * (d < 20 ? 1 : .5);
            const int r = relation(s.faction, a->faction);
            if (r == 1)
                friendly += strength;
            else if (r == -1) {
                hostile += strength;
                if (d < best) {
                    best = d;
                    threat = a;
                }
            }
        }
    s.ratio = (friendly + 1) / (hostile + 1);
    s.morale = std::clamp(.35 + .32 * s.ratio + .35 * hp / std::max(1.0, max_hp), .15, 1.25);
    const auto old = s.state;
    s.state = ActorState::Advancing;
    if (threat && s.ratio < .58) {
        s.state = ActorState::Retreating;
        s.objective = fortresses_[s.faction].position;
        double nearest = 100;
        for (auto &other : squads_)
            if (other.id != s.id && relation(s.faction, other.faction) == 1 && other.members.size() >= 3) {
                const double d = h.dist(s.center, other.center);
                if (d > 5 && d < nearest && h.dist(other.center, threat->position) > best + 3) {
                    s.objective = other.center;
                    nearest = d;
                }
            }
        if (old != s.state)
            ++stats_.retreats;
    } else if (threat) {
        s.state = s.ratio > 1.2 ? ActorState::Advancing : ActorState::Holding;
        s.objective = threat->position;
    } else {
        best = std::numeric_limits<double>::infinity();
        s.state = count < 3 ? ActorState::Gathering : ActorState::Advancing;
        s.objective = fortresses_[s.faction].position;
        bool enemy = false;
        for (auto &f : fortresses_)
            if (relation(s.faction, f.faction) == -1) {
                const double d = h.dist(s.center, f.position) * (f.ruined ? 2 : 1);
                if (d < best) {
                    best = d;
                    s.objective = f.position;
                    enemy = true;
                }
            }
        if (!enemy)
            s.state = ActorState::Holding;
    }
}
Handle World::nearest_target(const Actor &actor, bool support, double range) const {
    const auto &h = terrain_.space();
    Handle best;
    double score = 1e9;
    for (auto handle : spatial_.query(actor.position, range, h))
        if (auto *a = entities_.get(handle)) {
            if (handle == actor.handle)
                continue;
            const int r = relation(actor.faction, a->faction);
            if (r != (support ? 1 : -1))
                continue;
            const double d = h.dist(actor.position, a->position);
            if (d > range)
                continue;
            double priority = d;
            if (support) {
                const double missing = a->values.effective(Value::MaxHealth) - a->values.health();
                if (missing < .1)
                    continue;
                priority = d - missing * .4;
            }
            if (priority < score) {
                score = priority;
                best = handle;
            }
        }
    return best;
}
void World::route(Actor &a, h3::Vec goal) {
    const auto &h = terrain_.space();
    a.move_goal = goal;
    a.moving = h.dist(a.position, goal) > .35;
    if (!a.moving) {
        a.route.clear();
        return;
    }
    if (terrain_.segment_legal(a.position, goal)) {
        a.route.clear();
        return;
    }
    if (!a.route.empty() && stats_.ticks - a.path_at < 30) {
        const auto end = terrain_.nodes()[a.route.back()].point;
        if (h.dist(end, goal) < 4)
            return;
    }
    const int home_component = terrain_.component(terrain_.homes()[a.faction]);
    int from = terrain_.nearest(a.position, home_component);
    if (!terrain_.segment_legal(a.position, terrain_.nodes()[from].point)) {
        double d = 12;
        for (int i = 0; i < static_cast<int>(terrain_.nodes().size()); ++i) {
            const auto &n = terrain_.nodes()[i];
            const double test = h.dist(a.position, n.point);
            if (n.sample.walkable && terrain_.component(i) == home_component && test < d &&
                terrain_.segment_legal(a.position, n.point)) {
                from = i;
                d = test;
            }
        }
    }
    const int to = terrain_.nearest(goal, home_component);
    a.move_goal = terrain_.nodes()[to].point;
    a.route = terrain_.path(from, to);
    a.waypoint = 0;
    a.path_at = stats_.ticks;
    ++stats_.paths;
    if (a.route.empty()) {
        ++stats_.path_failures;
        a.moving = false;
    }
}
void World::plan_actor(Actor &a) {
    a.think_at = stats_.ticks + 12 + ((a.handle.ref & 0xffffffffU) % 5);
    auto *s = squad(a.squad);
    if (!s)
        return;
    const auto &h = terrain_.space();
    const auto &def = catalog_.units.at(a.base);
    const auto &atk = catalog_.attacks.at(def.attack);
    a.values.morale = std::lerp(a.values.morale, s->morale, .3);
    a.guard = {};
    a.manual_intent = false;
    const bool support = def.has(Capability::Heal);
    a.target = combat ? nearest_target(a, support, std::max(30.0, atk.range + 3)) : Handle{};
    auto *target = entities_.get(a.target);
    a.state = s->state;
    h3::Vec goal = s->objective;
    if (s->state != ActorState::Retreating) {
        if (target) {
            const double distance = h.dist(a.position, target->position);
            const double desired = support                         ? atk.range * .7
                                   : atk.mode == AttackMode::Melee ? atk.range * .85
                                                                   : atk.range * .8;
            if (distance <= atk.range && terrain_.line_of_sight(h.offset(a.position, 0, 1.5, 0),
                                                                h.offset(target->position, 0, 1, 0))) {
                a.moving = false;
                a.route.clear();
                a.state = support                         ? ActorState::Healing
                          : def.has(Capability::Siege)    ? ActorState::Bombarding
                          : atk.mode == AttackMode::Melee ? ActorState::Melee
                                                          : ActorState::Firing;
            } else {
                goal = target->position;
                if (atk.mode != AttackMode::Melee && distance > desired)
                    goal = terrain_.surface(
                        h.interpolate(target->position, a.position, std::min(.8, desired / distance)));
                route(a, goal);
            }
            if (!support && atk.mode != AttackMode::Melee && distance < 5) {
                const auto away = h.log(a.position, target->position);
                goal = terrain_.surface(h.exp(a.position, away * (-5 / std::max(.01, h3::norm(away)))));
                if (terrain_.legal(goal))
                    route(a, goal);
            }
        } else {
            const double d = h.dist(s->center, s->objective);
            if (d > 1) {
                const auto v = h.log(s->center, s->objective);
                const auto f = h.frame(s->center);
                const double yaw = std::atan2(h3::dot(v, f.east), h3::dot(v, f.north));
                const double lateral = (static_cast<int>((a.handle.ref & 0xffffffffU) % 5) - 2) * 1.4;
                goal = terrain_.surface(h.offset(s->objective, lateral, 0, def.formation_depth, yaw));
                if (!terrain_.legal(goal))
                    goal = s->objective;
            }
            route(a, goal);
        }
    } else
        route(a, goal);
    if (!def.behavior.empty() && lua_plans_ >= limits::lua_plans_per_tick)
        ++stats_.lua_budget_skips;
    if (!def.behavior.empty() && lua_plans_ < limits::lua_plans_per_tick) {
        ++lua_plans_;
        ++stats_.lua_calls;
        PolicyContext context{
            a.handle.ref, a.values.health(), a.values.effective(Value::MaxHealth), def.ignore_injured, {}};
        context.guard_capability = def.has(Capability::Guard);
        for (auto handle : spatial_.query(a.position, 24, h))
            if (auto *other = entities_.get(handle)) {
                const double distance = h.dist(a.position, other->position);
                if (distance <= 24 && handle != a.handle) {
                    context.nearby.push_back({handle.ref, distance, other->values.health(),
                                              other->values.effective(Value::MaxHealth),
                                              relation(a.faction, other->faction) == 1,
                                              relation(a.faction, other->faction) == -1,
                                              catalog_.units.at(other->base).has(Capability::Heal)});
                }
            }
        std::sort(context.nearby.begin(), context.nearby.end(), [](auto &x, auto &y) {
            return x.distance < y.distance || (x.distance == y.distance && x.ref < y.ref);
        });
        if (context.nearby.size() > limits::observations)
            context.nearby.resize(limits::observations);
        try {
            auto decision = policies_.at(def.behavior)->plan(context);
            if (decision.intent == PolicyIntent::Guard && def.has(Capability::Guard)) {
                auto handle = entities_.resolve(decision.target);
                if (auto *patient = entities_.get(handle);
                    patient && relation(a.faction, patient->faction) == 1) {
                    a.guard = handle;
                    a.state = ActorState::Guarding;
                    goal = terrain_.surface(h.offset(patient->position, 2.5, 0, 0));
                    if (terrain_.legal(goal))
                        route(a, goal);
                }
            } else if (decision.intent == PolicyIntent::Flank && s->state != ActorState::Retreating) {
                goal = terrain_.surface(h.offset(s->objective, (a.handle.ref % 2 ? 1 : -1) * 7, 0, -2));
                if (terrain_.legal(goal)) {
                    a.state = ActorState::Flanking;
                    route(a, goal);
                }
            } else if (decision.intent == PolicyIntent::Hold) {
                a.moving = false;
                a.route.clear();
            }
        } catch (const std::exception &e) {
            ++stats_.lua_errors;
            if (errors_.size() < 16)
                errors_.push_back(def.behavior + ": " + e.what());
        }
    }
}
void World::move(Actor &a) {
    a.values.velocity = 0;
    if (!a.moving)
        return;
    const auto &h = terrain_.space();
    auto goal = a.move_goal;
    if (!a.route.empty()) {
        while (a.waypoint < a.route.size() &&
               h.dist(a.position, terrain_.nodes()[a.route[a.waypoint]].point) < .28)
            ++a.waypoint;
        if (a.waypoint < a.route.size())
            goal = terrain_.nodes()[a.route[a.waypoint]].point;
        else
            a.route.clear();
    }
    const double remaining = h.dist(a.position, goal);
    if (remaining < .12) {
        a.moving = false;
        return;
    }
    const double speed = a.values.effective(Value::MoveSpeed);
    const auto frame = h.frame(a.position);
    // Local intrinsic separation steers only to legal surface locations. It cannot
    // replace graph routes or push a unit across blocked terrain.
    h3::Vec repulsion{};
    for (auto other : spatial_.query(a.position, 2.2, h))
        if (other != a.handle)
            if (auto *b = entities_.get(other)) {
                const double d = h.dist(a.position, b->position),
                             minimum = catalog_.units.at(a.base).radius + catalog_.units.at(b->base).radius;
                if (d > .03 && d < minimum)
                    repulsion = repulsion + h.log(a.position, b->position) * (-.45 * (minimum - d) / d);
            }
    if (h3::norm(repulsion) > .03) {
        const auto direction = h.log(a.position, goal);
        const auto adjusted = terrain_.surface(
            h.exp(a.position, direction * (1.5 / std::max(.01, h3::norm(direction))) + repulsion));
        if (terrain_.segment_legal(a.position, adjusted))
            goal = adjusted;
    }
    const auto result = terrain_.advance(a.position, goal, std::min(remaining, speed * limits::step));
    const auto previous = a.position;
    if (result.travelled > 1e-7) {
        const auto facing = h.transport(previous, result.point, h.frame(previous, a.yaw).north);
        const auto nextframe = h.frame(result.point);
        a.yaw = std::atan2(h3::dot(facing, nextframe.east), h3::dot(facing, nextframe.north));
        const auto to = h.log(previous, result.point);
        const double desired = std::atan2(h3::dot(to, frame.east), h3::dot(to, frame.north));
        a.yaw += std::atan2(std::sin(desired - a.yaw), std::cos(desired - a.yaw)) * .35;
        a.position = result.point;
        a.walk += result.travelled * 2.2;
        a.values.velocity = result.travelled / limits::step;
    } else {
        a.think_at = 0;
        if (a.controller == Controller::SquadAI && stats_.ticks - a.path_at > 20) {
            a.route.clear();
            a.path_at = 0;
        }
    }
}
} // namespace curved
