# Project working agreement

This is Asterhallow Games' unnamed native curved-world prototype. The current
repository slug is intentional; do not finalize the game's name implicitly.

- Read `docs/ARCHITECTURE.md` and the relevant canonical contract before changes.
- C++20, CMake, LLVM/MinGW and Ninja are the supported Windows path. No paid
  IDE, Visual Studio installation, proprietary SDK, account or store client is
  required to build or run. Pin dependency archives and verify SHA-256 hashes.
- Keep all generated output, downloads, tools and evidence under this root.
- `docs/references/` is an unchanged, read-only reference. Never modify the
  Downloads originals or Mandelrift-V2. Do not transplant its engine.
- Hyperboloid points and physical tangent vectors are authoritative. Distinguish
  ambient geodesics, induced surface routes, and chart/screen approximations.
- Simulation owns outcomes. AI, Lua and possessed actors use the same validated
  commands and capabilities. Renderer/UI/audio are observers.
- Keep modules small and dependencies toward domain owners. No global engine,
  class-name gameplay switches or competing Lua/UI state.
- Game art and audio are procedural editable recipes. No imported finished
  models, raster game art, baked textures or recorded sound.
- Preserve existing work. Do not stage, commit, push or publish unless the user
  separately authorizes it. Do not assign a public source-code license on the
  owner's behalf; third-party notices are separate.
- Build with one coordinated writer, at most four compiler jobs. Run meaningful
  tests and inspect actual rendered frames. Report failures and untested areas.
- `docs/BUILD_WORKFLOW.md` owns build/playtest behavior. Preserve compatible
  Ninja caches, unchanged private-source timestamps and frozen-runtime checks.
  The user explicitly authorized adapting Mandelrift V2's compilation/playtest
  workflow; Mandelrift itself remains read-only and is not a dependency.
- Controller support, other platforms and listening to audio require their own
  evidence. A buffer test or screenshot is not physical-device verification.
