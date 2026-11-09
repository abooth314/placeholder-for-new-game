#include "sim/values.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace curved {
static constexpr std::array<std::string_view, 8> names = {"health.max",
                                                          "movement.max_speed",
                                                          "combat.damage_multiplier",
                                                          "combat.cooldown_multiplier",
                                                          "support.heal_multiplier",
                                                          "health.current",
                                                          "morale.current",
                                                          "movement.velocity"};
static constexpr double lo[] = {1, 0, 0, .1, 0}, hi[] = {10000, 20, 10, 10, 10};
ActorValues::ActorValues(double hp, double speed) : base_{hp, speed, 1, 1, 1}, health_(hp) {
    if (!std::isfinite(hp) || !std::isfinite(speed) || hp < 1 || hp > 10000 || speed < 0 || speed > 20)
        throw std::invalid_argument("Invalid actor base values");
}
double ActorValues::base(Value id) const {
    return base_.at(static_cast<std::size_t>(id));
}
double ActorValues::effective(Value id) const {
    if (static_cast<unsigned>(id) >= static_cast<unsigned>(Value::Count))
        throw std::invalid_argument("Unknown actor value");
    if (id == Value::HealthCurrent)
        return health_;
    if (id == Value::Morale)
        return morale;
    if (id == Value::CurrentVelocity)
        return velocity;
    const auto i = static_cast<std::size_t>(id);
    double add = 0, mul = 1;
    for (auto &m : modifiers_)
        if (m.value == id) {
            add += m.additive;
            mul *= m.multiplier;
        }
    return std::clamp((base_.at(i) + add) * mul, lo[i], hi[i]);
}
void ActorValues::clamp_health() {
    health_ = std::min(health_, effective(Value::MaxHealth));
}
bool ActorValues::add(Modifier m) {
    if (m.source.empty() || m.source.size() > 96 || !std::isfinite(m.additive) ||
        !std::isfinite(m.multiplier) || std::abs(m.additive) > 10000 || m.multiplier < 0 ||
        m.multiplier > 10 || static_cast<unsigned>(m.value) >= 5)
        return false;
    for (auto &old : modifiers_)
        if (old.source == m.source && old.value == m.value) {
            old = std::move(m);
            clamp_health();
            return true;
        }
    if (modifiers_.size() >= 16)
        return false;
    modifiers_.push_back(std::move(m));
    clamp_health();
    return true;
}
void ActorValues::remove(std::string_view source) {
    std::erase_if(modifiers_, [&](auto &m) { return m.source == source; });
    clamp_health();
}
void ActorValues::expire(std::uint64_t tick) {
    std::erase_if(modifiers_, [&](auto &m) { return m.expires != 0 && m.expires <= tick; });
    clamp_health();
}
bool ActorValues::set_base(Value id, double n) {
    const auto i = static_cast<std::size_t>(id);
    if (i >= 5 || !std::isfinite(n) || n < lo[i] || n > hi[i])
        return false;
    base_[i] = n;
    clamp_health();
    return true;
}
double ActorValues::damage(double n) {
    if (!std::isfinite(n) || n < 0)
        return 0;
    const double applied = std::min(health_, n);
    health_ -= applied;
    return applied;
}
double ActorValues::heal(double n) {
    if (!alive() || !std::isfinite(n) || n < 0)
        return 0;
    const double applied = std::min(n, effective(Value::MaxHealth) - health_);
    health_ += applied;
    return applied;
}
std::optional<Value> ActorValues::resolve(std::string_view n) {
    for (unsigned i = 0; i < names.size(); ++i)
        if (names[i] == n)
            return static_cast<Value>(i);
    return {};
}
std::string_view ActorValues::name(Value id) {
    return names.at(static_cast<unsigned>(id));
}
} // namespace curved
