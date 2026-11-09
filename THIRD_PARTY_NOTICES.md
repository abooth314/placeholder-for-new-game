# Third-party provenance

The Windows build uses **LLVM/MinGW 20260616 (UCRT x64)**, **CMake 4.1.0** and
**Ninja 1.13.1**, pinned with archive hashes in `tools/setup-toolchain.ps1`.
Upstream: <https://github.com/mstorsjo/llvm-mingw/releases/tag/20260616>,
<https://github.com/Kitware/CMake/releases/tag/v4.1.0>,
<https://github.com/ninja-build/ninja/releases/tag/v1.13.1>.
LLVM and its libc++/libunwind runtimes use Apache-2.0 with LLVM exceptions;
MinGW runtime headers/libraries have their respective permissive/public-domain
notices (see the toolchain's licenses directory). CMake is BSD-3-Clause and
Ninja is Apache-2.0. LLVM/MinGW runtime notices are copied into
`dist/notices/toolchain`; build-tool notices remain with their archives in
`.tools`. CMake and Ninja are not runtime dependencies.
No Microsoft compiler, Visual Studio license, proprietary SDK or paid engine
is used by this build. OpenGL is supplied by the operating system/GPU driver.

Runtime library source pins and SHA-256 values are canonical in
`cmake/dependencies.lock.json`:

| Component | Selected version | License | Provenance |
| --- | --- | --- | --- |
| SDL | 3.4.18 | Zlib | https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18 |
| nlohmann/json | 3.12.0 | MIT | https://github.com/nlohmann/json/releases/tag/v3.12.0 |
| Lua | 5.4.9 | MIT | https://www.lua.org/ftp/ |
| Dear ImGui | 1.92.5 | MIT | https://github.com/ocornut/imgui/releases/tag/v1.92.5 |

Selected versions are deliberate pins, not claims to be the newest releases.
SDL, Lua and ImGui are linked statically; libc++ and libunwind DLLs accompany
the executable. ImGui's embedded default font and its bundled stb code retain
the notices in the ImGui source distribution. Glyph atlases are built at
runtime. No finished external game models, textures, images or recorded audio
are authoring inputs.

The owner's supplied HTML/handoff are retained as unchanged reference material,
not third-party library source. No source license is inferred for the new game.
