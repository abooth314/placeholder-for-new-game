# Curved-world native prototype

An independent, exploratory C++20 battle sandbox by **Asterhallow Games**.
The title is provisional; the repository remains `placeholder-for-new-game`.

Windows x64 uses the free LLVM/MinGW toolchain, CMake and Ninja. Visual Studio,
an IDE subscription, a proprietary game engine and paid SDKs are unnecessary.
The original HTML is a reference, not the runtime.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1
.\Playtest_Prototype.bat
```

The build script downloads hash-verified tools into `.tools` and dependencies
into `.dependencies`; the first build needs internet access. Subsequent builds
reuse them. Normal output is `out/build`, with a runnable distribution in `dist`.
Compatible CMake configurations are reused; Ninja checks dependencies and only
recompiles changed source/header consumers. Compiler jobs are capped at four.

`Playtest_Prototype.bat` follows **Mandelrift V2's** source-copy workflow: it
builds a private source snapshot, retains incremental objects, and launches a
verified frozen runtime with separate persistent settings. Unchanged source
reuses a hash-verified bundle without recompiling. `-PrepareOnly` prepares it
without opening the game; `-Rebuild` makes a fresh private compilation. Details
and workflow tests: `docs/BUILD_WORKFLOW.md`.

`Launch_Prototype.bat` opens the existing distribution (builds it if absent).
For a portable package, run `tools/package.ps1` after a successful build and use
the executable in the ZIP without installing developer tools.

Select a soldier, archer, cleric or catapult and press **P** to possess it.
**WASD** moves the unit; **left mouse/X** uses its attack, healing or siege
capability. **P** returns it to AI, retaining its health, equipment and cooldown.
The same transfer works for compatible modded units. Controls can be rebound.

Controls, implementation status and measured evidence are documented in
`docs/STATUS.md`. Architecture: `docs/ARCHITECTURE.md`. Geometry and simulation:
`docs/GEOMETRY.md`, `docs/SIMULATION.md`. Content: `docs/CONTENT.md`. Native API
and actor values: `docs/LUA_API.md`, `docs/ACTOR_VALUES.md`. Procedural recipes:
`docs/RECIPES.md`. Dependency provenance: `THIRD_PARTY_NOTICES.md` and
`cmake/dependencies.lock.json`.

No project license has been chosen. Third-party components retain their own
licenses, distributed with the executable. All project changes remain local
until the owner explicitly authorizes publication. Generated builds, validation
output, downloaded tools and personal settings are excluded from source commits.
