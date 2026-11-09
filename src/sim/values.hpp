#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace curved {
enum class Value : std::uint8_t {
    MaxHealth,
    MoveSpeed,
    DamageMultiplier,
    CooldownMultiplier,
    HealMultiplier,
    HealthCurrent,
    Morale,
    CurrentVelocity,
    Count
};
struct Modifier {
    std::string source;
    Value value;
    double additive = 0, multiplier = 1;
    std::uint64_t expires = 0;
};
class ActorValues {
  public:
    ActorValues(double max_health = 100, double speed = 3);
    double effective(Value id) const;
    double base(Value id) const;
    double health() const {
        return health_;
    }
    bool alive() const {
        return health_ > 0;
    }
    double morale = 1, velocity = 0;
    bool add(Modifier m);
    void remove(std::string_view source);
    void expire(std::uint64_t tick);
    bool set_base(Value value, double number);
    double damage(double amount);
    double heal(double amount);
    const std::vector<Modifier> &modifiers() const {
        return modifiers_;
    }
    static std::optional<Value> resolve(std::string_view name);
    static std::string_view name(Value id);
    static bool writable(Value id) {
        return static_cast<unsigned>(id) < 5;
    }

  private:
    void clamp_health();
    std::array<double, 5> base_;
    double health_;
    std::vector<Modifier> modifiers_;
};
} // namespace curved
