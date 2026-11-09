#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace curved {
struct BehaviorDef;
struct PolicyObservation {
    std::uint64_t ref;
    double distance, health, max_health;
    bool ally, enemy, healer;
};
enum class PolicyIntent { Default, Guard, Advance, Flank, Hold };
struct PolicyDecision {
    PolicyIntent intent = PolicyIntent::Default;
    std::uint64_t target = 0;
};
struct PolicyContext {
    std::uint64_t self;
    double health, max_health;
    bool ignore_injured;
    std::vector<PolicyObservation> nearby;
    bool guard_capability = true;
};
class LuaPolicy {
  public:
    explicit LuaPolicy(const BehaviorDef &definition);
    ~LuaPolicy();
    LuaPolicy(const LuaPolicy &) = delete;
    LuaPolicy &operator=(const LuaPolicy &) = delete;
    PolicyDecision plan(const PolicyContext &context);
    std::size_t memory_bytes() const;
    static void validate(const BehaviorDef &definition);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace curved
