# Inspected HTML inventory

Inspected source: `references/Hyperwar_Kingdoms_of_the_Curved_Earth.html`.
The companion handoff is preserved alongside it. Hashes are in
`references/SHA256SUMS.txt`. Read the entire source, including the executed
config, geometry, terrain, simulation, renderer, audio, checks and app sections.
This inventory is source inspection, not browser or native acceptance evidence.

| Owner / source lines | Executed behavior and important limits |
| --- | --- |
| Config 105–136 | Four directional factions; eight hardcoded classes and weights. Quality presets also change particle budgets; population preset values are not applied by `setQuality`. |
| Geometry 138–225 | Unit Lorentz H³, L=48; physical exp/log vectors; chord/asinh distance; transport; normal H² lift; frames; reflected isometry; camera frame; Klein chart. `fromKlein` silently clamps and several functions mask invalid values instead of rejecting them. |
| Terrain 227–782 | Twelve seeded recipes; intrinsic radial rings with capped angular density. Hills/water/roads use H² distances. Graph cost integrates `dh² + cosh²(h/L) ds_H2²` using one Simpson panel with linear edge heights; actual movement samples triangle-interpolated heights, so these are related approximations, not identical exact integrals. Forests/props block nodes; component repairs may open raised causeways. |
| Terrain sampling/navigation | Klein bins select triangles and barycentrically interpolate height. Nearest-vertex dual mask defines walkability. Path caching is bounded; A* uses ambient distance as a lower bound. `advance` corrects local speed and checks intermediate legality for large steps. `clearance` is accepted but unused. Triangle sampling and graph edge legality are not an independent proof of arbitrary-radius collision clearance. |
| Simulation 784–1796 | 30 Hz app stepping; 0.75 s squad plans, ~0.35–0.53 s actor thinking; squad strength/morale, role-specific formation depths, retreat to allied companies, cavalry flanking and charge, spear counter, champion cleave, healer shields. Many mechanics branch on numeric class identity. A chart hash is rebuilt every 0.12 s; padding is heuristic for moving entries. Native foundation will rebuild at phase boundaries to remove that staleness issue. |
| Combat | Melee, arrows, fireballs, healing/shields, catapult splash and fortress assaults. Projectile path is a base H² geodesic plus a parabolic normal-height profile, not Newtonian ambient gravity. Swept unit tests use closest points on H³ geodesic segments; terrain is checked at the step endpoint. Fortresses rebuild in 32 s. |
| Identity/lifecycle | Monotonic numeric IDs and map lookups; pooled actors/projectiles/effects. Dead actors remain in lookup until corpse cleanup, but most callers check death. No generation/epoch handles, native actor-value/modifier contract, static content loader, Lua or external mod boundary. |
| Diplomacy | Outgoing enemy=-1, neutral=0, ally=1; self always allied; neutral never auto-retaliates. Live edits clear invalid enemy targets, invalidate planning; heal and projectile impact eligibility rechecked at use. Four presets. |
| Renderer 1798–2392 | Solid tangent-space procedural meshes exponentiated into H³, static terrain, GPU instances, camera-local Lorentz perspective, depth using projected Klein z, fog, inexpensive contact shadows and LOD. Normals/light are local approximations. Water covers triangles with any wet node. Reduced canvas fallback uses CPU solid meshes and static caches; no GPU timing there. Resource disposal exists; context loss falls back, without full context restoration. |
| Audio 2394–2490 | Web Audio oscillators/noise, 22 voices, interval gating and distance/pan from H³ listener frame, effects/ambience/master/mute. Presentation randomness is mostly separate, but `_effect` consumes simulation RNG, so changing effect budgets can alter later simulation randomness. |
| Checks 2492–2713 | Useful mathematical identities, current-route traversal, counters and directional fixture. Checks are same-source assertions; shader readiness checks backend state, not pixels. Missing consumers can emit NOT RUN without incrementing total. They do not prove every seed/map, broad-phase motion coverage, combat parity, performance or heard audio. |
| App 2715–3021 | Fixed 1/30 accumulator; raw delta capped at .1 s, backlog at .4 s, max 12 steps. Orbit/overview/focus/follow/free H³ cameras, selection, map/seed chooser, atlas, diplomacy, settings, spawn and keyboard controls. Settings do not persist; some shortcut handlers remain active in modals. No player possession. |

Native scope is the handoff's bounded two-map/four-role foundation plus the
user's explicit unit-possession addition. Remaining classes, ten further maps,
controller delivery, broad platform support and advanced presentation are
incremental work. `STATUS.md` reports the actual delivered subset and evidence.
