#include "sim/world.hpp"
#include <algorithm>
#include <cmath>

namespace curved {
bool World::damage_actor(Handle target, double damage, int source) {
    auto *a = entities_.get(target);
    if (!a || !std::isfinite(damage) || damage < 0 || damage > 100000 ||
        (source >= 0 && relation(source, a->faction) != -1))
        return false;
    a->values.damage(damage);
    a->flash = .2;
    if (!a->values.alive()) {
        --faction_stats_[a->faction].alive;
        ++faction_stats_[a->faction].losses;
        if (source >= 0 && source < 4)
            ++faction_stats_[source].kills;
        feedback("core:impact", a->position, Feedback::Impact, a->faction);
        deaths_.push_back(target);
        plans_invalid_ = true;
    }
    return true;
}
bool World::damage_fortress(Handle target, double damage, int source) {
    if (!std::isfinite(damage) || damage < 0 || damage > 100000)
        return false;
    for (auto &f : fortresses_)
        if (f.handle == target && !f.ruined && relation(source, f.faction) == -1) {
            f.health = std::max(0.0, f.health - damage);
            if (f.health == 0) {
                f.ruined = true;
                f.rebuild_at = time() + 32;
                ++stats_.ruins;
                feedback("core:collapse", f.position, Feedback::Collapse, f.faction);
                plans_invalid_ = true;
            }
            return true;
        }
    return false;
}
void World::attack(Actor &a, Handle requested) {
    if (!combat || a.cooldown > 0)
        return;
    const auto &def = catalog_.units.at(a.base);
    const auto &atk = catalog_.attacks.at(def.attack);
    const auto &h = terrain_.space();
    const bool support = atk.mode == AttackMode::Heal;
    Handle target = requested ? requested : a.target;
    auto *other = entities_.get(target);
    if (!other || relation(a.faction, other->faction) != (support ? 1 : -1) ||
        h.dist(a.position, other->position) > atk.range) {
        target = nearest_target(a, support, atk.range);
        other = entities_.get(target);
    }
    Fortress *fort = nullptr;
    if (!support && (!other || def.has(Capability::Siege)))
        for (auto &f : fortresses_)
            if (!f.ruined && relation(a.faction, f.faction) == -1 &&
                h.dist(a.position, f.position) <= atk.range + 5) {
                fort = &f;
                break;
            }
    if (support) {
        if (!other || other->values.health() >= other->values.effective(Value::MaxHealth) - .1 ||
            !terrain_.line_of_sight(h.offset(a.position, 0, 1.6, 0), h.offset(other->position, 0, 1, 0)))
            return;
        const double healed = other->values.heal(atk.damage * a.values.effective(Value::HealMultiplier));
        if (healed <= 0)
            return;
        faction_stats_[a.faction].healing += healed;
        ++stats_.heals;
        a.target = target;
        feedback(atk.sound, other->position, Feedback::Heal, a.faction);
    } else {
        if (!other && !fort)
            return;
        const auto aim = fort ? fort->position : other->position;
        if (atk.mode != AttackMode::Projectile || !def.has(Capability::Siege))
            if (!terrain_.line_of_sight(h.offset(a.position, 0, 1.6, 0), h.offset(aim, 0, fort ? 3 : 1, 0)))
                return;
        const double damage = atk.damage * a.values.effective(Value::DamageMultiplier);
        if (atk.mode == AttackMode::Melee) {
            if (fort)
                damage_fortress(fort->handle, damage * atk.building_multiplier, a.faction);
            else
                damage_actor(target, damage, a.faction);
            ++stats_.melee;
            feedback(atk.sound, h.offset(aim, 0, 1, 0), Feedback::Melee, a.faction);
        } else {
            if (projectiles_.size() >= limits::projectiles) {
                ++stats_.projectile_overflow;
                return;
            }
            const auto from = h.offset(a.position, 0, def.has(Capability::Siege) ? 2.4 : 1.65, 0),
                       to = h.offset(aim, 0, fort ? 3.0 : 1.0, 0);
            const double d = h.dist(from, to);
            projectiles_.push_back({from, from, h.ground(from), h.ground(to), h.height(from), h.height(to),
                                    std::clamp(d / atk.flight_speed, .18, 3.5), 0, std::max(1.0, d * atk.arc),
                                    damage, atk.splash, atk.building_multiplier, a.faction, a.handle.ref,
                                    def.has(Capability::Siege)});
            ++stats_.shots;
            stats_.peak_projectiles = std::max(stats_.peak_projectiles, projectiles_.size());
            feedback(atk.sound, from, def.has(Capability::Siege) ? Feedback::Siege : Feedback::Arrow,
                     a.faction);
        }
        const auto v = h.log(a.position, aim);
        const auto f = h.frame(a.position);
        if (h3::norm(v) > .01)
            a.yaw = std::atan2(h3::dot(v, f.east), h3::dot(v, f.north));
        a.target = fort ? Handle{} : target;
    }
    a.cooldown = atk.cooldown * a.values.effective(Value::CooldownMultiplier);
    a.flash = .3;
}
void World::impact(const Projectile &p) {
    const auto &h = terrain_.space();
    if (!combat)
        return;
    if (p.splash > .1) {
        for (auto target : spatial_.query(p.position, p.splash + 2, h))
            if (auto *a = entities_.get(target); a && relation(p.faction, a->faction) == -1) {
                const double d = h.dist(h.offset(a->position, 0, .9, 0), p.position);
                if (d < p.splash) {
                    damage_actor(target, p.damage * (.4 + .6 * (1 - d / p.splash)), p.faction);
                    ++stats_.hits;
                }
            }
    } else {
        Handle target;
        double best = 1.5;
        for (auto candidate : spatial_.query(p.position, 3, h))
            if (auto *a = entities_.get(candidate); a && relation(p.faction, a->faction) == -1) {
                const double d = h.dist(h.offset(a->position, 0, 1, 0), p.position);
                if (d < best) {
                    best = d;
                    target = candidate;
                }
            }
        if (target) {
            damage_actor(target, p.damage, p.faction);
            ++stats_.hits;
        }
    }
    for (auto &f : fortresses_)
        if (relation(p.faction, f.faction) == -1 && !f.ruined &&
            h.dist(f.position, p.position) < 5 + std::max(0.0, p.splash))
            damage_fortress(f.handle, p.damage * p.building_multiplier, p.faction);
    feedback("core:impact", p.position, p.siege ? Feedback::Siege : Feedback::Impact, p.faction);
}
void World::step_projectiles() {
    const auto &h = terrain_.space();
    for (std::size_t i = 0; i < projectiles_.size();) {
        auto &p = projectiles_[i];
        p.previous = p.position;
        const double begin = p.age / p.duration;
        p.age = std::min(p.duration, p.age + limits::step);
        const double end = p.age / p.duration;
        auto point = [&](double t) {
            return h.lift(h.interpolate(p.from_base, p.to_base, t),
                          std::lerp(p.from_height, p.to_height, t) + 4 * p.arc * t * (1 - t));
        };
        const auto destination = point(end);
        const int samples = std::max(1, static_cast<int>(std::ceil(h.dist(p.position, destination) / .25)));
        bool hit = false;
        for (int sample = 1; sample <= samples && !hit; ++sample) {
            const double t = std::lerp(begin, end, static_cast<double>(sample) / samples);
            p.position = point(t);
            if (t > .02 && h.height(p.position) < terrain_.sample(h.ground(p.position)).height + .08)
                hit = true;
            if (p.age > .06) {
                for (auto target : spatial_.query(p.position, 3, h))
                    if (auto *a = entities_.get(target); a && relation(p.faction, a->faction) == -1) {
                        const auto center = h.offset(a->position, 0, 1, 0);
                        if (h.dist(center, p.position) <
                            std::max(.45, catalog_.units.at(a->base).radius * .7) + (p.siege ? .2 : .08)) {
                            hit = true;
                            break;
                        }
                    }
                for (auto &f : fortresses_)
                    if (!f.ruined && relation(p.faction, f.faction) == -1 &&
                        h.dist(f.position, p.position) < 5 && h.height(p.position) < h.height(f.position) + 7)
                        hit = true;
            }
        }
        if (hit || p.age >= p.duration) {
            impact(p);
            projectiles_[i] = projectiles_.back();
            projectiles_.pop_back();
        } else
            ++i;
    }
}
} // namespace curved
