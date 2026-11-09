# Build and private playtest workflow

This is the canonical build contract. It adapts the current **Mandelrift V2**
build/playtest approach to this independent project: compatible CMake caches,
Ninja dependency checks, a private source copy, preserved unchanged timestamps,
and verified frozen runtime bundles. It does not depend on a Mandelrift checkout.

Windows PowerShell 5.1, the free pinned LLVM/MinGW UCRT x64 toolchain, CMake and
Ninja are sufficient. An IDE, Microsoft compiler/SDK installation, account,
Visual Studio license and Python installation are not required. The initial
download needs internet access. Archive hashes and notices are retained locally.

## Normal development

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1
```

The supported build is Release in out/build and installs into dist. CoreOnly
uses out/core without building the SDL client. Jobs is 1..4, default 4. Additional
options are Configuration Release/Debug, Configure, Target and NoInstall.
SourceDirectory, BuildDirectory, InstallDirectory and ToolchainDirectory support
private builds and fixtures; they do not select a system compiler implicitly.

The wrapper checks CMakeCache.txt against the source path, CMake and compiler
paths, Ninja executable/generator, configuration and client option. It explicitly
configures only when these differ, configuration is missing, or Configure was
requested. It **always asks Ninja to check the normal build**. Ninja owns source,
header and included-CMake dependencies and CMake auto-regeneration. The JSON
dependency lock is registered as a CMake configure dependency too. Never skip
Ninja just because an executable exists or the Git status looks unchanged.

A no-op prints "ninja: no work to do" and leaves native object/executable times
unchanged. An edited CPP recompiles that translation unit; edited headers rebuild
their consumers. Changing data, Lua, documentation or recipes needs no native
compilation. Installing updates runtime content. Because a CMake install overlays
an existing directory, use playtest or a fresh package for a clean asset tree
after deleting/renaming assets.

One FileShare.None lock owns each build directory across configure, compilation
and installation. Conflicting writers fail instead of using the same objects.
Every invocation writes last-build.json and separate streamed stdout/stderr logs
under its build directory. Native exit status determines success; a warning on
stderr is not a failed build. Environment PATH is restored afterward.

## Source-copy playtest

```powershell
.\Playtest_Prototype.bat
# Or prepare without opening a window:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/playtest.ps1 -PrepareOnly
# Force a fresh private build while keeping previous evidence/settings:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/playtest.ps1 -Rebuild
```

The launcher hashes current source, CMake/lock files, tests, tools, docs, recipes,
notices, enabled-mod selection and authored mods. Generated output, .git,
downloaded tools/dependencies and personal settings are excluded. The toolchain
identity includes its selected compiler drivers/library, build executables,
runtime DLLs, their paths, and the pinned setup script.

On a source change, it synchronizes a private source tree under
out/playtest/cache and compiles **that copy** with its own Ninja/CMake outputs.
The fixed paths and unchanged file timestamps let Ninja retain existing objects.
Changed bytes receive a fresh compiler-visible timestamp even if an editor
backdated the original. Deleted authored files are removed from the private copy.
An attempted-input receipt is written before compilation so a partially failed
build followed by a revert cannot reuse stale objects. The private copy/cache is
retained for inspection and reuse.

The private build shares only this repository's pinned toolchain and verified
download archives. Dependency source/build trees and native objects are private.
Only the game target and its dependencies are built; run tools/test.ps1 separately
for the full native test suite. A new staging directory receives a complete
install, runtime DLLs and notices, preventing retired assets from lingering.

Source is rehashed after copying and after compilation. Concurrent source edits
abort selection. A successful runtime is frozen into a bundle addressed by its
executable/runtime hashes, with a complete manifest. The current.json selection
changes only after verification. Failure keeps the previous selection and logs.

If source and toolchain are unchanged, playtest verifies every runtime file's
actual bytes and reuses the bundle without invoking the compiler or CMake.
Modified/missing/extra runtime files fail verification. Rebuild creates a new
private compilation; if an identically addressed old bundle was altered, it is
preserved under quarantine before the verified replacement is selected.

Authored enabled mods refresh from the copied source into out/playtest/profile.
Settings persist there across versions. Make authoring edits in repository user/,
not in generated profile copies. Other user data is preserved. Launch/report
paths and compiler output are recorded under out/playtest/reports. A profile
lock prevents two launched playtests writing the same settings concurrently.
The launcher waits for the game to close and propagates failure. Runtime PATH
contains only Windows system paths, so the bundle must supply its own runtimes.

For a short native smoke run from PowerShell:

```powershell
& .\tools\playtest.ps1 -GameArguments @('--frames','120','--no-vsync')
```

GameArguments cannot replace managed user/assets/output paths. All playtest
outputs stay inside this project. Linked inputs/outputs and escaping manifest
paths are rejected. Older bundles, failed stages and logs are retained; no other
checkout or process is modified.

## Workflow verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-build-workflow.ps1
```

This uses the production wrappers and a tiny actual C++ fixture, not mocked
compiler results. It checks no-op object times; selective CPP/header recompiles;
CMake include/data regeneration; configuration/client changes; forced configure;
writer exclusion; failure/recovery; copied-source caching; asset deletion and
mod refresh without recompilation; preserved settings; backdated edits; failed
partial-build reverts; runtime corruption rejection and clean recovery. Fixtures
and all failing logs remain under out/workflow-tests.
