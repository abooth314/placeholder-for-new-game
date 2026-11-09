#pragma once
#include "math/hyperbolic.hpp"
#include "sim/limits.hpp"
#include "sim/values.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace curved {
using RefId = std::uint64_t;
struct Handle {
    RefId ref = 0;
    std::uint32_t generation = 0, epoch = 0;
    explicit operator bool() const {
        return ref != 0;
    }
    bool operator==(const Handle &) const = default;
};
enum class Controller { SquadAI, Player };
enum class ActorState {
    Gathering,
    Advancing,
    Holding,
    Retreating,
    Melee,
    Firing,
    Healing,
    Bombarding,
    Guarding,
    Flanking,
    Player
};
const char *state_name(ActorState state);
struct Actor {
    Handle handle;
    std::string base;
    int faction = 0;
    std::uint32_t squad = 0;
    Controller controller = Controller::SquadAI;
    ActorValues values;
    h3::Vec position, move_goal;
    double yaw = 0, cooldown = 0, walk = 0, flash = 0;
    ActorState state = ActorState::Gathering;
    Handle target, guard;
    bool moving = false, manual_intent = false;
    std::uint64_t think_at = 0, path_at = 0;
    std::vector<int> route;
    std::size_t waypoint = 0;
};
class Entities {
  public:
    explicit Entities(std::uint32_t epoch);
    Handle create(std::string base, int faction, h3::Vec position, double health, double speed);
    Actor *get(Handle handle);
    const Actor *get(Handle handle) const;
    Handle resolve(RefId ref) const;
    bool destroy(Handle handle);
    std::size_t size() const {
        return refs_.size();
    }
    std::vector<Handle> live() const;
    std::uint32_t epoch() const {
        return epoch_;
    }

  private:
    struct Slot {
        std::uint32_t generation = 1;
        std::optional<Actor> actor;
    };
    std::array<Slot, limits::actors> slots_;
    std::map<RefId, std::size_t> refs_;
    std::uint32_t epoch_, next_ = 1;
};
} // namespace curved
