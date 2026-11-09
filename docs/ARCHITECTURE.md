# Native ownership

This is an exploratory battle sandbox, with a provisional display label and no
campaign, economy, progression, networking or fixed player faction. The current
milestone has two terrain recipes and four native combat compositions, plus an
external sample pack. The user's addition is possession of any compatible actor.

| Owner | Source | Responsibility |
| --- | --- | --- |
| H³ numerics | src/math | Unit hyperboloid, physical tangent vectors, frames, exp/log, transport, isometries |
| Terrain/navigation | src/world/terrain.* | Continuous displaced H² recipe, finite sampled legal graph, surface-length integration, components, routes |
| Spatial queries | src/world/spatial.* | Conservative chart candidates, rebuilt at simulation phase boundaries |
| Live identities/storage | src/sim/entities.* | Bounded slots, opaque lifetime-validated handles, independent actor instances |
| Actor values | src/sim/values.* | Effective values, lifecycle health, source-owned modifiers |
| Combat/timing | src/sim/combat.cpp, limits.hpp | Native attacks, healing, H³ projectiles, damage, death and budgets |
| Tactical commands | src/sim/tactics.cpp, world.cpp | Squads, ally strength, retreat, formations, movement, controller transfer |
| Authored content | src/content | Strict JSON catalog, transactional validation, sandboxed Lua policies |
| Procedural assets | src/assets, src/audio/synth.* | Reusable native mesh and waveform evaluators |
| Graphics/camera | src/view | Retained GPU resources, camera-relative H³ instancing/projection, real free flight |
| Audio device/mixer | src/audio/audio.* | Synth cache, bounded prioritized voices, intrinsic distance/pan, SDL stream |
| Input/UI/console | src/app/input.*, src/ui | Semantic actions, capture/rebinding, native inspection and validated requests |
| Coordination/settings | src/app/main.cpp, settings.* | Asset paths, window/context, fixed-step accumulator, lifecycle, preferences, diagnostics |

Domain code does not depend on SDL, ImGui or OpenGL. JSON and Lua headers stay
at their loading/interpreter boundaries. Presentation reads domain state and
published events. UI issues public commands; it does not calculate hits, healing
or movement. Lua returns bounded intents to the native tactical owner.

Catalogs are immutable for a world's lifetime. The application constructs a
complete replacement terrain/world before switching maps. Graphics resources
are rebuilt for the new terrain; unit/sound recipes are cached at startup.
There is no hot reload, general editor, ECS framework or singleton engine.

Read the canonical contracts for details rather than inferring them from this
overview: GEOMETRY.md, SIMULATION.md, ACTOR_VALUES.md, CONTENT.md, LUA_API.md,
RECIPES.md. REFERENCE_INVENTORY.md is source inspection; STATUS.md and out/
contain implementation status and evidence, not new normative contracts.
