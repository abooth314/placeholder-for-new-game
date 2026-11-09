# Implementation status

Working title only: Curved Earth / curved-world prototype. Repository and Git
root remain placeholder-for-new-game. CMake project metadata is 0.1.0; this
project does not yet use a numbered-release workflow or published release tags.
Source, authored content, tools, documentation and notices belong in Git.
Downloaded tools/dependencies, distributions, generated validation and personal
settings remain local and ignored. Publishing requires explicit owner approval.

Launch the current source with Playtest_Prototype.bat (private incremental build),
or open the existing distribution with Launch_Prototype.bat / dist/curved_world.exe
from any working directory. Build with tools/build.ps1; test with tools/test.ps1.
BUILD_WORKFLOW.md documents the Mandelrift V2 compilation/playtest adaptation.
The supported path is Windows x64, free LLVM/MinGW, Ninja and CMake; no Visual Studio is used.
All downloads, generated output and evidence stay in this project.

Default controls: WASD move view/unit, wheel zoom, right-drag orbit/look, Q/E
rotate, Space pause, F focus, O overhead, V free flight, R/C rise/drop, M maps,
K diplomacy, Tab council, G inspector, H help. Select a unit and P possesses or
releases it. Left mouse/X uses its compatible native attack/healing capability.
Bindings are editable; the UI displays current bindings. World input is captured
by modals, text fields and rebinding. Settings persist in user/settings.json.

| Reference feature / requested addition | Current native status |
| --- | --- |
| Authoritative Lorentz H³, normal-lifted terrain, physical surface movement, real orbit/free camera | Implemented; finite sampled navigation and tessellation approximations are explicit |
| Four factions, outgoing enemy/neutral/ally, mirrored edits and presets | Implemented, including cached targets and projectile impact eligibility |
| Open map plus constrained waterways/bridges | Implemented as Green March and Three Crossings; recipes are native interpretations, not all twelve HTML map ports |
| Autonomous companies, allies changing retreat/advance, formation depth, reinforcements | Implemented; modest tactics and local separation |
| Melee, ranged projectiles, healing and siege | Implemented through data-defined native primitives |
| Unit possession and return to AI | Implemented for every current actor composition; retains health, equipment, values and native cooldown/lifecycle |
| BaseId/RefId/generation/world epoch; source-owned modifiers | Implemented and independently tested |
| Built-in/external validated pipeline, enabled mod, meaningful Lua policy and authored exception | Implemented; Wardens pack is enabled and appears in the application |
| Procedural solid soldiers, bows/shields/staves/catapults, trees, houses, castles, bridge rails and tethered horses | Implemented recipes; appearance/animations remain prototype quality |
| Synthesized combat effects, ambience, intrinsic audio panning and voice bounds | Implemented; device and buffer evidence distinguished from listening |
| Selection, stats, atlas, maps, diplomacy, council, spawn, pause/speed, console/native inspector, settings | Implemented; compact UI refinements included |
| Eight-class parity (spear, mounted charge, battlemage, champion), ten further maps | Deferred; no claim of full HTML parity |
| Shield effects, advanced flanking/screens, exact finite-radius prop collisions | Deferred or partial; guard/flank intents exist, but decorative props do not block bodies |
| Shadows, rich icons, sophisticated LOD, GPU timers, adaptive resolution, full visual parity | Deferred; simple lighting/fog and explicit detail switch |
| Gamepad input, other operating systems, multiplayer, save-game format, campaign/economy | Deferred / outside this milestone |

The implementation includes executable mathematical/domain tests and
presentation/input/settings/console tests. Native diagnostics render populated
battles, selection/possession, free flight, crossing terrain, diplomacy and
inspection, resize to 960x640 and recreate the graphics context. Evidence and
the measured profile are indexed below.

## Validation and evidence

Profiled distribution binary SHA-256:
303b1fb16d8cda6092bc10028dfc97ba06e70c0ebaab00e698c21eedeb4bcdbe.
Catalog fingerprint: e92f9832af920f8f. Measurements below use this game binary;
build/workflow and documentation changes do not imply native recompilation.

- out/validation/final-tests/junit.xml (also accepted/tests/junit.xml): all 18 required CTest groups passed,
  none disabled/skipped/missing. Geometry includes independently known distances,
  near/far/high-curvature cases, transport and isometries. Domain checks cover
  actual bridge traversal, spatial coverage, all four combat roles, identities,
  modifiers, diplomacy including arrows in flight, allied strength decisions,
  Lua/mod validation, possession/release/death, replay and bounded cleanup.
- out/validation/current/rendered: native diagnostics and nine framebuffer
  captures covering battle, selection, external Warden possession, free flight,
  crossings, directional diplomacy, inspector, compact 960x640 layout and GL
  context recreation. Settings and captured-input consumers are exercised too.
  BMPs are losslessly converted/checked by tools/review-captures.ps1; actual PNGs
  were inspected. Possession/compact/crossings scenes have readable controls.
- Direct native-window review exercised selection/possession, pause/resume,
  unit movement input, directional diplomacy UI and free-camera transfer, then
  closed the application. Controlled tests supply exact attack/heal assertions;
  manual key delivery alone is not evidence of a successful shot.
- out/validation/current/Portable build [Omega] with spaces: standalone launch
  from a different working directory and Unicode/spaced path, without compiler
  paths in PATH; external mod, assets, screenshot/report and audio stream loaded.
  The actual directory uses the Greek Omega character.
- out/validation/current/hardware.json and profile-open / profile-crossings:
  two 30-second real-time populated native battle profiles, described below.
- out/workflow-tests and out/workflow-validation-*.log: build/playtest fixtures
  and retained failure evidence. tools/test-build-workflow.ps1 verifies the
  production wrappers against actual compiler outputs. out/playtest contains
  the independently compiled source-copy runtime, manifests and launch receipts.
- out/playtest/verification.json: the full copied-source game built and ran its
  nine-capture native diagnostic successfully. A second unchanged preparation
  reused the verified bundle with all 318 native object timestamps unchanged.
  Its independently linked executable hash is recorded there (it differs from
  the profiled distribution build); no cross-build binary identity is promised.
- out/playtest/content-update-verification.json: updating this evidence index
  refreshed the full-game bundle without rewriting native objects/executable.
- out/invalid-mod-validation-final.log and out/validation/invalid-mod-d786f569:
  a deliberately unresolved external attack link failed at native startup with
  exit 1 and a persisted error identifying definitions.json and its owning unit.
- out/packages/package.json and out/packages/verification.json: portable ZIP
  identity, extracted-file verification and native launch receipt.

Audio: the current validation opened the native WASAPI device, generated stream
frames and played queued synth events. Voice priority/capping is exercised
(peak 24 voices in the profiles); lower-priority events are deliberately dropped
under load. An earlier WASAPI open failed and the implemented DirectSound
fallback was exercised in earlier evidence. Neither device/stream success nor
the saved synthesized-battlefield.wav means the agent listened to the audio.

Hardware: i9-13980HX, RTX 4090 Laptop GPU, Windows 11 Pro 10.0.26200, NVIDIA
610.88. Both maps: seed 8147, L=48 m, 1440x900, detailed renderer, vsync off,
1x speed, fixed 1/30 s simulation, population cap 240 and initial peak 240 actors.
Casualties/reinforcements continue normally; 240 is not a constant survivor count.

| Map / 30-second observation | Frame ms p50 / p95 / p99 | Simulation tick ms p50 / p95 / p99 | Peak working set |
| --- | --- | --- | --- |
| Green March | 1.47 / 3.93 / 14.31 | 7.20 / 22.34 / 25.99 | 146.7 MiB |
| Three Crossings | 1.56 / 7.70 / 18.13 | 8.07 / 22.21 / 24.68 | 150.2 MiB |

Both profiles reported zero path/Lua errors and zero dropped catch-up steps.
Wall-time clamping was 0.182 s / 0.171 s respectively; simulated time was 29.8 s.
GPU timing is unavailable. These are measurements on one laptop, not a 4K/240 FPS
target, worst-case benchmark or other-platform performance guarantee.

Failures remain in out rather than being erased: initial disconnected bridge
ramps / isolated navigation goals, an initial native audio failure, Unicode
screenshot path conversion, and Windows PowerShell logging/notice-copy issues
were fixed and their affected checks rerun. The final native validation was
rerun after the user's Norton exclusion change. No security settings were
changed by the implementation scripts.

Known practical limits: finite sampled terrain; local-body separation rather
than full rigid-body collision; decorative prop intersections/camera occlusion;
ordered attacks rather than simultaneous damage; no unrestricted modding of
new primitives; no cross-build/platform bitwise replay promise. Procedural audio
has native device/stream evidence but has not been listened to by the agent.

Next development step after this foundation: improve finite-radius obstacle
navigation and company screening around bridge approaches, then add a native
mounted/charge composition and its procedural horse animation. Keep that slice
bounded and validate its geometry, controller transfer and actual rendered use.
