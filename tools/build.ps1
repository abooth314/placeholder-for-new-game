param(
    [switch]$CoreOnly,
    [ValidateRange(1,4)][int]$Jobs = 4,
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [string]$SourceDirectory = '',
    [string]$BuildDirectory = '',
    [string]$InstallDirectory = '',
    [string]$ToolchainDirectory = '',
    [string]$Target = '',
    [switch]$Configure,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$repo = Split-Path -Parent $PSScriptRoot
$source = if ($SourceDirectory) { [IO.Path]::GetFullPath($SourceDirectory) } else { $repo }
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $repo $(if ($CoreOnly) { 'out/core' } else { 'out/build' }) }
$install = if ($InstallDirectory) { [IO.Path]::GetFullPath($InstallDirectory) } else { Join-Path $repo 'dist' }
if (!$ToolchainDirectory) { $ToolchainDirectory = Join-Path $repo '.tools' }
$tools = Get-CurvedTools $ToolchainDirectory (Join-Path $PSScriptRoot 'setup-toolchain.ps1')
$client = if ($CoreOnly) { 'OFF' } else { 'ON' }

function Test-CompatibleCache {
    if ($Configure -or !(Test-Path -LiteralPath (Join-Path $build 'build.ninja')) -or !(Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))) { return $false }
    $cache = @{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt')) {
        if ($line -match '^([^#/:][^:]*):[^=]+=(.*)$') { $cache[$matches[1]] = $matches[2] }
    }
    $desired = [ordered]@{CMAKE_HOME_DIRECTORY=$source; CMAKE_COMMAND=$tools.cmake; CMAKE_C_COMPILER=$tools.cc; CMAKE_CXX_COMPILER=$tools.cxx;
        CMAKE_GENERATOR='Ninja'; CMAKE_MAKE_PROGRAM=$tools.ninja; CMAKE_BUILD_TYPE=$Configuration; CURVED_BUILD_CLIENT=$client}
    foreach ($key in $desired.Keys) {
        if (!$cache.ContainsKey($key)) { return $false }
        if ($key -in @('CMAKE_HOME_DIRECTORY','CMAKE_COMMAND','CMAKE_C_COMPILER','CMAKE_CXX_COMPILER','CMAKE_MAKE_PROGRAM')) {
            if (![string]::Equals($cache[$key].Replace('\','/').TrimEnd('/'), $desired[$key].Replace('\','/').TrimEnd('/'), [StringComparison]::OrdinalIgnoreCase)) { return $false }
        } elseif ($cache[$key] -cne $desired[$key]) { return $false }
    }
    return $true
}

$writer = Enter-CurvedLock (Join-Path $build '.curved-build.lock')
$oldPath = $env:PATH
$started = [DateTime]::UtcNow
$logRoot = Join-Path $build ('logs/' + $started.ToString('yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$receipt = [ordered]@{schema=1; source=$source; build=$build; configuration=$Configuration; client=[bool](!$CoreOnly); target=$Target;
    jobs=$Jobs; configure_ran=$false; success=$false; started_utc=$started.ToString('o')}
try {
    $env:PATH = "$($tools.bin);$(Split-Path -Parent $tools.ninja);$oldPath"
    if (!(Test-CompatibleCache)) {
        $receipt.configure_ran = $true
        $arguments = @('-S',$source,'-B',$build,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$($tools.ninja)","-DCMAKE_C_COMPILER=$($tools.cc)","-DCMAKE_CXX_COMPILER=$($tools.cxx)","-DCMAKE_BUILD_TYPE=$Configuration","-DCURVED_BUILD_CLIENT=$client")
        $code = Invoke-CurvedNative $tools.cmake $arguments $source (Join-Path $logRoot 'configure')
        if ($code -ne 0) { throw "CMake configure failed ($code); see $logRoot" }
    } else { Write-Host '[Build] Reusing compatible Ninja configuration; checking dependencies.' }
    $arguments = @('--build', $build, '--parallel', "$Jobs")
    if ($Target) { $arguments += @('--target', $Target) }
    $code = Invoke-CurvedNative $tools.cmake $arguments $source (Join-Path $logRoot 'build')
    if ($code -ne 0) { throw "Build failed ($code); see $logRoot" }
    if (!$CoreOnly -and !$NoInstall) {
        $code = Invoke-CurvedNative $tools.cmake @('--install',$build,'--prefix',$install) $source (Join-Path $logRoot 'install')
        if ($code -ne 0) { throw "Install failed ($code); see $logRoot" }
        foreach ($dll in @('libc++.dll','libunwind.dll')) { Copy-CurvedRuntimeFile (Join-Path $tools.bin $dll) (Join-Path $install $dll) }
        $notices = Join-Path $install 'notices/toolchain'
        Copy-CurvedRuntimeFile (Join-Path $tools.directory 'llvm-mingw-20260616-ucrt-x86_64/LICENSE.TXT') (Join-Path $notices 'LLVM-LICENSE.txt')
        foreach ($notice in Get-ChildItem -LiteralPath (Join-Path $tools.directory 'llvm-mingw-20260616-ucrt-x86_64/x86_64-w64-mingw32/share/mingw32') -File -Filter 'COPYING*') { Copy-CurvedRuntimeFile $notice.FullName (Join-Path $notices $notice.Name) }
    }
    $receipt.success = $true
} finally {
    $receipt.elapsed_seconds = ([DateTime]::UtcNow - $started).TotalSeconds
    $receipt.logs = $logRoot
    try { Write-CurvedJson (Join-Path $build 'last-build.json') $receipt }
    finally { $env:PATH = $oldPath; $writer.Dispose() }
}
