# Codex implementation handoff — <PROJECT_NAME>

You are the implementation agent for an independent, exploratory native game prototype by **Asterhallow Games**. The project name and repository are deliberately not finalized.

**Project variables**
- Display title: `<PROJECT_NAME>`
- Folder/repository slug: `<PROJECT_SLUG>`
- Intended project root and Git root: `<PROJECT_ROOT>`
- GitHub URL (reference only; do not push): `<GITHUB_REPOSITORY_URL>`
- Primary HTML reference, to be inspected read-only: `C:\Users\aboot\Downloads\Hyperwar_Kingdoms_of_the_Curved_Earth.html`
- Optional read-only architecture reference: `C:\Users\aboot\OneDrive\Documents\GitHub\Mandelrift-V2`

## 1. Objective and scope

Build a **working, inspectable native C++ foundation** that faithfully carries forward the HTML experiment's unusual world geometry, autonomous medieval combat, and visual/audio identity, while replacing its browser architecture. Do not merely wrap a web view, transpile the JavaScript wholesale, or draw a conventional flat simulation through a distorted lens.

This is a **prototype, not a finalized game design**. Do not invent a campaign, economy, technology tree, story, progression, fixed player role, multiplayer, monetization, or a general-purpose engine. A continuously running, spectatable battle sandbox with exploration and limited manipulation is sufficient. Keep product assumptions explicit and reversible.

The **first bounded milestone** must launch on Windows x64 and visibly demonstrate: four independently related factions; seeded terrain with genuine hyperbolic geometry and traversable routes; autonomous squads that fight, coordinate, fall back and respond to allies; meaningful melee, ranged, support/healing, and siege behaviors; directional diplomacy changes at runtime; a 3D orbit/RTS camera and free-flight camera; recognizable procedural models and synthesized sound; working selection, inspection, spawning, pause/speed and controls; and one built-in/external-content boundary with a meaningful Lua-authored example mod.

Start with **two contrasting maps** (one open and one with constrained bridges/crossings), four representative combat roles, and the smallest useful set of effects. Add the remaining eight-class/twelve-map parity incrementally, not by making an untestable first milestone. Clearly report exactly which features are implemented, partial, or deferred. Do not silently approximate defining geometry or behavior.

## 2. Inspect the reference before implementation

Read the entire HTML source, trace the actually executed code paths, and record a feature/accuracy inventory. It is roughly organized into embedded `config.js`, `geometry.js`, `terrain.js`, `simulation.js`, `renderer.js`, `audio.js`, `checks.js`, and `app.js` sections; these are logical markers inside one file, **not** a native architecture to copy verbatim.

Facts visible in that version that deserve particular inspection:

- Four factions: Iron Banner, Ashen Court, Verdant Pact, Sunward League; relations are **directional** enemy/neutral/ally values, with optional mirrored edits and presets.
- Eight named roles: swordsman, spearman, archer, knight, catapult, battlemage, cleric, champion. The current definitions and some mechanics are hardcoded in JavaScript; replace this with validated content/capability composition.
- Twelve seeded maps: Green March, Three Rivers, Elderwood, Crownlands, Frostfall Pass, Embercrags, Sable Marsh, Sunscar Dunes, Silverlake, Autumn Reach, Shattered Coast, and Forgotten Crown. The current terrain/navigation is **finite and sampled**, not a limitless exact manifold.
- The spatial core uses the **Lorentz hyperboloid** representation for H³, with a configurable curvature radius (initially 48 in the inspected source), Lorentz inner product, geodesic distance, exponential/logarithmic map, transport, local frames and hyperbolic camera. The **Klein chart** is used for lookup, minimap and rasterization-related representation; it must not become the authoritative metric.
- Terrain is built from a base H² slice, lifted in H³. Inspect whether each movement/path/physics step actually honors the terrain-induced metric instead of assuming ambient geodesic length or an arbitrary map-plane cost.
- Simulation uses **fixed 1/30-second steps**, a staggered planning cadence, squad objectives, local ally/enemy strength, morale, formations, reinforcements, pooled transient objects, fortress rebuilding, and a nearby-index approximation. Verify actual effects, ordering and limitations; do not accept comments or UI descriptions as proof.
- Renderer uses procedural low-poly solid models, hyperbolic camera projection and shader instances, a reduced renderer fallback, a Klein atlas, effects and an inspector. Audio is generated in Web Audio with event priority/voice limiting. Browser-specific mechanisms must be replaced natively.
- Existing source checks are a useful starting point but not independent proof of correctness. Inspect edge cases, shader math, broad-phase conservatism, navigation, projectile paths, resource limits and cleanup.

Preserve an identified, **unchanged** reference copy in project documentation/references if appropriate. Do not edit the Downloads original.

## 3. Architecture and technical choices

Use **C++20 or newer**, **CMake**, an understandable Windows x64 build/run path, and a compact, pinned dependency set. A reasonable starting recommendation is SDL3 for window/input/controller/audio device management, OpenGL for a custom H³-aware renderer, Lua 5.4 for authored behavior, a strict JSON loader for definitions, and Dear ImGui for development inspectors. You may justify an alternative on practical rendering, portability, licensing, or complexity grounds. Pin exact selected versions, document provenance and license notices; do not invent version claims.

Keep separate owners for: (1) H³ math/numerics; (2) terrain/navigation/spatial queries; (3) entity storage and identifiers; (4) actor values/modifiers; (5) combat/effects/timing; (6) tactical AI and commands; (7) content loading and Lua; (8) procedural visuals/assets; (9) graphics; (10) procedural audio; (11) input/UI/inspection; and (12) application coordination. Dependencies should point toward native domain owners, not cyclic singletons. Renderer/UI/audio read authoritative state and events, never decide gameplay outcomes.

Favor compact typed storage for frequently updated components and separate optional data. Avoid monolithic source files, giant transitive headers, circular dependencies, premature ECS frameworks, and generic systems lacking prototype consumers.

## 4. Genuine hyperbolic world and numerical contracts

Implement the Lorentz hyperboloid H³ as authoritative spatial representation, with curvature radius `L` and sectional curvature `-1/L²`. A normalized world point `P = (P0,P1,P2,P3)` must satisfy `-P0² + P1² + P2² + P3² = -1` with `P0 > 0` in the **unit hyperboloid convention**; physical distances multiply by `L`. If choosing radius-scaled coordinates instead, document and consistently apply that convention.

Provide and independently test Lorentz product, normalization, tangent projection, `dist`, `exp`, `log`, geodesic interpolation, parallel transport, stable frame construction, isometries needed by the camera, and bounded numerical tolerances. Include near-coincident, far-separated and high-curvature tests; reject non-finite/invalid inputs. Avoid catastrophic loss of precision near zero distance; document chart boundaries and error behavior.

Build a **fully 3D world**: terrain is a displaced H² surface embedded in H³; trees, roofs, walls, figures, weapons, flight trajectories and free camera occupy positions/orientations in that same H³ world. The ordinary game camera resembles a 2.5D RTS camera solely because gameplay mostly occurs on the surface. From freecam, the world must genuinely have volume and different sightlines. No hidden flat-world gameplay or post-process fisheye substitution.

Keep separate: ambient H³ geodesic distance, **surface-constrained route length**, and screen/chart approximations. Movement on slopes/roads and paths around water/cliffs follow the terrain-induced geometry and legal connections; broad-phase may use a projected chart only with conservative candidate coverage. Projectile trajectories use a documented H³ trajectory/integration and explicit gameplay gravity model; never implement flight as unqualified Euclidean interpolation. Rendering may use triangles and numerical tessellation as controlled approximations of hyperbolic shapes.

Make negative curvature visibly consequential at battlefield scale: separated sightlines, diverging roads, distances, group frontage, navigation and exploration. Provide a small inspector that makes intrinsic distances and curvature visible without turning the UI into a lecture.

## 5. Simulation, world ownership, identities and capabilities

One native simulation owns all live gameplay state. Define simulation phases and ordering, for example: accept validated user/Lua requests; process scheduled effects; perform staggered squad/unit planning; update fixed-step movement, collisions, attacks and projectiles; apply damage/healing/morale/death; publish events; commit structural changes and cleanup. Document which requests take effect in the current or next step, when observers see them, how cancellation works, and which multi-part operations are atomic.

Use stable **namespaced BaseIds** for authored definitions, **RefIds** for independent live instances, and lifetime-validated **native handles** (generation and world epoch or equivalent). Many actors can share one base definition without sharing health, morale, targets, modifiers, or temporary state. Commands targeting live actors validate instance identity; spawns resolve a definition. Public IDs must never expose vector indices or unchecked pointers. Separate faction identity, controller ownership, selection, and target identity.

Make a shared actor model for soldiers, horse-mounted units, casters and support units, with explicit capabilities such as locomotion, melee, ranged/projectile, healing, siege, guarding, and target eligibility. Buildings and other positioned objects share inspection/targeting contracts where useful but retain type-appropriate state. Add no class-name or numeric-ID switches where a compatible capability suffices. Actor controllers choose intents through public native commands; later transferring a supported actor between controllers must preserve native mechanics and instance state (without requiring direct player possession now).

Implement meaningful autonomous behavior: squad gathering, objectives, formation positions, role-sensitive combat, local strength assessment, protection of ranged/support units, flanking where navigable, retreat/regroup thresholds, and continuous reinforcements. Nearby allied forces must change tactical decisions. Keep per-step mechanics native; use less frequent, staggered squad/unit decision phases. Directional faction relations must affect enemy target validity **and** support eligibility immediately after live changes, including existing orders, projectiles/effects where applicable, and cached squad plans. Explicitly decide how neutral units and asymmetric relations work.

Add bounded entity/projectile/effect/sound budgets with documented overflow behavior, counters and cleanup. Fortresses can sustain damage, fall and rebuild through a bounded native lifecycle. Do not let destroyed actors remain addressable as live instances by stale references.

## 6. Actor values and source-owned modifiers

Expose a discoverable, documented native actor-value contract for actual prototype needs: health/current/max, morale, movement limits/speed, supported combat modifiers, etc. Keep weapon base damage/range/cooldown owned by attack definitions rather than cloning them into an independently editable stat store. Categorical attributes such as faction, controller and capabilities are typed properties, not numeric statistics.

For each value, document units, base/default, current/effective distinction, legal range, read/write policy, aggregation order, clamping, and side effects. Distinguish actual current velocity from max movement rate and a movement multiplier. Values used in the UI and Lua must resolve through the same native implementation that drives simulation. Resolve string names to typed identifiers at API boundaries; inner loops use typed access.

Modifiers are tagged by **source identity** (equipment, spell, aura, authored status, or another live source) and follow explicit additive/multiplicative order, stacking, expiry and removal rules. Removing one source must not erase other contributors. Define what reducing/increasing max health does to current health; unrelated stat edits must not heal or revive. Damage/healing/death go through native lifecycle operations, not unconstrained writes.

Tests must prove independent instances from one BaseId, additive-plus-multiplicative composition, removal/expiry of one source while others persist, stale-reference rejection, and visible movement/combat agreeing with inspected effective values.

## 7. Static content, Lua, and practical mods

Use versioned, validated external **data** for factions, units, equipment, attacks, capabilities, map recipes, procedural appearance/audio recipes, scenarios, diplomacy presets and reinforcement choices. Lua owns bounded tactical/scenario policy and composition of supported native capabilities; it must make **meaningful choices**, not just call a C++ function that implements an entire named role. Use the same schema/loaders for shipped content and user mods.

Define stable namespaced IDs, schema/API version, dependencies, deterministic load order, explicit enabled-mod selection, and a small override policy (prefer complete-record replacement in this milestone). Keep user-writable mods outside installed game assets. Validate each candidate catalog completely before publishing; an invalid mod must fail clearly with pack/file/definition diagnostics and must not half-replace the live catalog. Do not promise field merging, hot reload or migrations unless actually implemented. Separate discovery, menu exposure, spawn eligibility, and capability eligibility as explicit authoring policies.

Lua must have no raw native pointers, unchecked handle use, unrestricted OS/filesystem/network access, or competing writable simulation. Provide bounded instruction and memory usage, deterministic scheduling policy where practical, and clear errors. Validate arguments, permissions, capability requirements, command counts, resource limits and target lifetime. Document each exposed function immediately: context, arguments, returns, failure behavior, timing and a working example.

Expose only operations needed by real behavior: bounded nearby/ally/enemy observation, distances/positions, values/capabilities, squad objectives and movement intents, supported attacks/effects, reinforcement/scheduling, and cancellation. Keep high-frequency projectiles, collision, damage and movement in native code. Avoid a Lua callback for every unit and every hit. A truly new primitive mechanic may require a native feature; content-only extensions that reuse an existing one must not require changing C++ branches.

Provide an **independent, enabled example mod** that adds a newly named compatible unit and a real Lua-authored tactical behavior (for example, switching between guarding an injured allied support unit and joining the squad's advance based on bounded observations). Its new definition, appearance, spawn policy and behavior must load through normal paths without a C++ rebuild or identity-specific native change. Test its presence in the application, not merely in an isolated loader test. Show one authored exception to a shared behavior as well.

## 8. Procedural assets, presentation and sound

Preserve the HTML's recognizable **medieval, colorful, stylized low-poly** identity: solid soldiers and horses, identifiable shields/weapons, catapults, castles, houses, forests and atmospheric terrain. This is not a wireframe demo or a screen-space deforming flat map. Model vertices, orientations and camera positions must be embedded/reconstructed in H³ consistently. Use GPU instancing, retention/caching, sensible geodesic tessellation and LOD according to measured costs.

**Procedural generation is required for authored game assets**, not merely map placement. Express compatible unit/prop geometry, materials, patterns, palettes, animation curves, effects, icons and audio as versioned **editable recipes** built from reusable native evaluators. Terrain and environmental shapes follow the same principle. Do not ship prerendered sprite sheets, finished imported models, raster art/baked texture files, or prerecorded music/sounds as game authoring inputs. Licensed outline font files are acceptable; generate glyph atlases at runtime.

Create/rebuild meshes, GPU textures, audio buffers and atlases at loading or change boundaries; keep explicit ownership, cache keys, memory limits, invalidation and cleanup. Do not regenerate unchanged resources every frame. Separate simulation RNG from presentation RNG so changing quality or model recipes never changes battle results. Test replacement/resize/context/resource lifecycle.

Synthesize battlefield sounds natively: distinct impacts, bows/arrows, horse movement, siege, spells, healing, destruction and optional ambience, with controlled variation and bounded voices. Position and attenuate sources by **hyperbolic** listener-relative distance/direction. Support master/effects/ambience volumes and mute. Report whether sound was actually listened to versus merely generating buffers. Do not substitute generic silence for the audio requirement.

## 9. Graphics, camera, UI, input and diagnostics

Use a H³-aware renderer: transform objects to the camera's local orthonormal hyperbolic frame and perform a numerically justified perspective projection. Include depth behavior, clipping, GPU instancing and visible, navigable terrain. Use straightforward lighting/fog/decals or modest shadows before speculative post-processing. Make engine-side rendering tests and actually inspect screenshots of battles, selection and UI states. A successfully linked shader is **not** visual acceptance.

Preserve the reference's useful visual hierarchy and controls: spacious battle view, team statistics, selected-unit card, map chooser, diplomacy matrix, simulation settings and Klein minimap/atlas. Supply elevated ground/orbit camera, overhead/overview, battle-focus, selection-follow, and **real free-flight through H³**. Keep keyboard/mouse actions (WASD, mouse wheel, right-drag, V for freecam, and useful existing shortcuts), with semantic bindings and UI input capture. Displayed shortcuts must match real bindings; manipulating a modal or binding editor must not issue world commands. Stationary pointer movement must not steal keyboard/controller focus. Preserve settings across restart when exposed; plan controller input through the action/focus layer but do **not** claim controller support unless tested.

Add a modest inspector/console backed by native owners, not writable UI mirrors. Useful operations: inspect BaseId/RefId/handle validity, positioned objects, actor values/modifier sources, capabilities, terrain/nav distance and camera state; select/inspect/spawn by ID; issue supported mutations and reject unsupported writes explicitly; view content/Lua errors, entity counts, renderer resource counts and frame/tick diagnostics. Avoid a general-purpose editor.

## 10. Determinism, performance and validation

Use a fixed simulation step decoupled from render frame rate; document catch-up/clamping and speed controls. Record actual reproducibility guarantees for a seed and build, not hypothetical cross-platform bitwise identity. Keep limits centralized. Separate adaptive visual quality from simulation time so an FPS improvement does not secretly mean slower battles or omitted mechanics. Optimize allocations, repeated traversals, spatial queries and retained resources before sacrificing visible behavior.

Write independent, executable tests for geometry identities/known distances, transport, chart stability; surface-constrained routing across river/bridge and blocked slopes; conservative spatial-query coverage; actor identities and cleanup; values/modifiers; diplomacy including asymmetric changes; melee/ranged/healing/siege; group behavior when allies appear/disappear; fortresses/reinforcements; invalid content and Lua budgets; and mod loader validation. Borrow useful HTML checks only after independently determining what they prove.

Test a representative **populated** battle and collect seed, map, faction population, renderer quality/resolution, PC CPU/GPU, timestep, speed, duration, frame-time percentiles, simulation CPU time, GPU time if available, and peak memory/entity/voice budgets. Capture evidence of observed rendered consumers (not just unit tests). Listen to procedural audio when possible. Any aggregate test runner must fail on missing, timed-out or silently skipped cases; save the failing evidence. Distinguish source inspection, tests, manual graphics/audio evaluation, synthetic controller tests and physical-device verification. Do not claim untested platforms or features passed. Do not import Mandelrift's 4K/240 FPS target; establish a measured appropriate baseline and improve from evidence.

## 11. Work plan and safe boundaries

First inspect `<PROJECT_ROOT>` and verify it is the intended Git root and note existing status/changes. Do not overwrite unrelated work. Read the HTML before choosing architecture. Inspect Mandelrift V2 **read-only** only for useful ideas in `docs/ARCHITECTURE.md`, `docs/MODDABLE_ENGINE_ARCHITECTURE_PLAN.md`, `docs/CONTENT_PACKS.md`, `docs/LUA_NATIVE_API.md`, `docs/PROCEDURAL_ASSETS.md`, `docs/AUDIO_ARCHITECTURE.md`, `docs/CONSOLE.md` and `docs/UI_STRATEGY.md`; distinguish implemented contracts from planned designs. **Never edit Mandelrift, copy its entire engine, or inherit its numbering, scope or workflow machinery.**

Progress through small runnable slices, validating each before expanding:

1. **Source inventory and build scaffold:** capture behavior/limitations, establish dependencies, native window, diagnostics and reproducible CMake build.
2. **Geometry/terrain/camera:** prove H³ math with independent tests; show solid procedural terrain, props, orbit/RTS camera and free-flight; establish surface routing on an open and crossing-constrained map.
3. **Autonomous conflict:** native entities, four factions, diplomacy, fixed-step movement/targeting, squads, morale, melee, reinforcements, buildings and selection. Demonstrate that nearby allies change decisions.
4. **Distinct combat roles and feedback:** projectile/ranged, healing/support and siege, effects, procedural sound, native inspection and responsive UI.
5. **Real content boundary:** validated built-in definitions/recipes, Lua tactical/scenario operations, external enabled-mod loading, a working example mod, actor-value/modifier tests and end-to-end combat with that mod.
6. **Polish and evidence:** resource limits, actual rendering/UI/audio review, stability and timing profiles, comparison inventory and concise documentation.

Run/inspect/debug the executable during the process; do not stop at design documents or an unlaunched build. Reuse incremental builds; limit compiler parallelism; ensure a single coordinated build writer. Launch from a directory different from the source/build directory and test paths with spaces (Unicode where practical). Resolve assets relative to a documented asset root/application location, never to a developer's hardcoded current working directory.

Keep all new source, docs, tests, experiments and generated artifacts within `<PROJECT_ROOT>`. **Do not create another repository, stage, commit, push, publish, delete existing project work, or modify other projects without explicit separate authorization.** This instruction authorizes local inspection, implementation, building, running and fixes *inside the selected project only*.

## 12. Deliverables and acceptance criteria

Deliver inside the project:

- A Windows x64 executable and required distributable runtime content/notices, plus tested build/run steps.
- Clean source, pin/dependency manifests, CMake project, repeatable build, tests, built-in content and Lua/recipe files.
- A distinct user mod location, enabled-mod selection, and a genuinely functioning external sample mod.
- Concise `README.md`, enduring project working agreement, ownership/architecture overview, HTML-to-native feature matrix, Lua native API + actor-value reference, content/modding guide and procedural-recipe reference. Each contract has one canonical document; status and experimental evidence are not passed off as normative design.
- Test results, gameplay screenshots and useful profiling evidence, with failures/unknowns preserved and disclosed.

**Minimum foundation acceptance checklist:**

- [ ] A fresh Windows x64 build can launch a native window without invoking a browser; camera can orbit and freely fly through a genuinely 3D H³ scene.
- [ ] Two seeded terrain recipes present distinct traversal constraints; units navigate legal routes across a bridge and do not walk across blocked water or cliffs.
- [ ] Four teams autonomously spawn, march, coordinate and fight. Directional diplomacy changes retargeting and support behavior during combat.
- [ ] At least one melee, one ranged/projectile, one healing/support and one siege capability have visibly distinct native mechanics, usable by data-defined content.
- [ ] A squad advances or regroups differently in a controlled scenario depending on nearby allied strength; this is not merely a cosmetic animation.
- [ ] BaseIds, RefIds and native handles have defined semantics; separate live instances retain independent values; stale handles are rejected.
- [ ] Source-owned modifiers compose and remove correctly, and inspected effective values agree with actual simulation behavior.
- [ ] Built-in and external content use the same validated pipeline; an independent Lua-enabled mod adds a compatible unit and meaningful tactical behavior without rebuilding C++.
- [ ] Models, terrain detail, effects and battlefield audio are generated from procedural recipes and managed by bounded native caches.
- [ ] A working UI offers selection, spawn, speed/pause, diplomacy, map/seed change, cameras and useful native diagnostics.
- [ ] Automated tests plus direct observation confirm the above; reproducibility, numerical approximations, resource limits, and current performance are reported honestly.

At your final handoff, state **what launches and how**, which HTML features are preserved/partial/deferred, what is editable without a rebuild, what needs new native capabilities, exactly which tests and visual/audio reviews actually ran, measured performance, known defects and numerical constraints, the next concrete development step, and confirmation that changes remain **unstaged and uncommitted** unless separately authorized.

**Priority:** mathematically meaningful H³ mechanics and a reliable runnable native foundation; then enjoyable autonomous combat; then faithful visual/audio character, practical content extensibility and measured performance. Do real implementation work, not just a plan.
