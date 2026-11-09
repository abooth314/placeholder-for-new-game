#pragma once
#include <cstddef>
namespace curved::limits {
inline constexpr std::size_t actors = 384, projectiles = 128, effects = 256, events = 128, squads = 64;
inline constexpr std::size_t squad_members = 12, observations = 24, lua_plans_per_tick = 16;
inline constexpr double step = 1.0 / 30;
inline constexpr unsigned max_catchup = 12;
} // namespace curved::limits
