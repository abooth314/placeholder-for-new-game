# Exercise the production wrappers against a tiny real C++ project. All edits,
# compiler failures, corrupted bundles, logs and receipts stay in this fixture.
param()
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$repo = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $repo ('out/workflow-tests/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$live = Join-CurvedOwnedPath $fixture 'live'
$build = Join-CurvedOwnedPath $fixture 'build'
$playtest = Join-CurvedOwnedPath $fixture 'playtest'
[IO.Directory]::CreateDirectory($live) | Out-Null
$script:checks = [Collections.Generic.List[string]]::new()
$shell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'

function Assert-Workflow([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw "Workflow assertion failed: $Message" }
    $script:checks.Add($Message)
    Write-Host "PASS $Message"
}
function Write-Fixture([string]$Relative, [string]$Text) {
    $path = Join-CurvedOwnedPath $live $Relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $path)) | Out-Null
    [IO.File]::WriteAllText($path, $Text, [Text.UTF8Encoding]::new($false))
}
function Invoke-Wrapper([string]$Label, [string]$Script, [string[]]$Arguments, [switch]$Fails) {
    $log = Join-CurvedOwnedPath $fixture ($Label + '.log')
    $argumentText = (@('-NoProfile','-ExecutionPolicy','Bypass','-File',$Script) + $Arguments | ForEach-Object { ConvertTo-CurvedNativeArgument $_ }) -join ' '
    $process = Start-Process -FilePath $shell -ArgumentList $argumentText -WindowStyle Hidden -PassThru -RedirectStandardOutput ($log + '.stdout') -RedirectStandardError ($log + '.stderr')
    try { $null = $process.Handle; $process.WaitForExit(); $process.Refresh(); $code = $process.ExitCode }
    finally { $process.Dispose() }
    [IO.File]::WriteAllText($log, [IO.File]::ReadAllText($log + '.stdout') + "`n" + [IO.File]::ReadAllText($log + '.stderr'))
    Assert-Workflow $(if ($Fails) { $code -ne 0 } else { $code -eq 0 }) "$Label exit status (actual $code)"
    return $log
}
function Run-Build([string]$Label, [string[]]$Extra = @(), [switch]$Fails) {
    $arguments = @('-SourceDirectory',$live,'-BuildDirectory',$build,'-ToolchainDirectory',(Join-Path $repo '.tools'),'-NoInstall') + $Extra
    return Invoke-Wrapper $Label (Join-Path $PSScriptRoot 'build.ps1') $arguments -Fails:$Fails
}
function Run-Playtest([string]$Label, [string[]]$Extra = @(), [switch]$Fails) {
    $arguments = @('-SourceDirectory',$live,'-PlaytestDirectory',$playtest,'-PrepareOnly') + $Extra
    return Invoke-Wrapper $Label (Join-Path $PSScriptRoot 'playtest.ps1') $arguments -Fails:$Fails
}
function Object-Times([string]$Directory) {
    return [pscustomobject]@{
        main=[IO.File]::GetLastWriteTimeUtc((Join-Path $Directory 'CMakeFiles/curved_world.dir/src/main.cpp.obj')).Ticks
        other=[IO.File]::GetLastWriteTimeUtc((Join-Path $Directory 'CMakeFiles/curved_world.dir/src/other.cpp.obj')).Ticks
    }
}
function Assert-Result([string]$Exe, [int]$Expected, [string]$Label) {
    $actual = @(& $Exe)
    Assert-Workflow ($LASTEXITCODE -eq 0 -and $actual.Count -eq 1 -and $actual[0] -ceq "$Expected") $Label
}

try {
    foreach ($directory in @('src','tests','cmake','assets','docs','notices','tools','user/mods/fixture')) {
        [IO.Directory]::CreateDirectory((Join-CurvedOwnedPath $live $directory)) | Out-Null
    }
    foreach ($file in @('build.ps1','build-common.ps1','setup-toolchain.ps1','playtest.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-CurvedOwnedPath $live ('tools/' + $file))
    }
    Write-Fixture 'CMakeLists.txt' @'
cmake_minimum_required(VERSION 3.25)
project(CurvedWorkflowFixture LANGUAGES C CXX)
option(CURVED_BUILD_CLIENT "Fixture option" ON)
include(cmake/options.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cmake/version.txt")
file(APPEND "${CMAKE_BINARY_DIR}/configure-events.txt" "configured\n")
add_executable(curved_world src/main.cpp src/other.cpp)
install(TARGETS curved_world RUNTIME DESTINATION .)
install(FILES README.md THIRD_PARTY_NOTICES.md DESTINATION .)
install(DIRECTORY assets docs notices user DESTINATION .)
'@
    Write-Fixture 'src/main.cpp' "#include <cstdio>`n#include `"value.hpp`"`nint other();`nint main() { std::printf(`"%d\n`", BASE_VALUE + other()); }`n"
    Write-Fixture 'src/other.cpp' "int other() { return 3; }`n"
    Write-Fixture 'src/value.hpp' "#define BASE_VALUE 7`n"
    Write-Fixture 'cmake/options.cmake' "set(FIXTURE_OPTION 1)`n"
    Write-Fixture 'cmake/version.txt' "1`n"
    Write-Fixture 'cmake/dependencies.lock.json' '{"schema":1,"dependencies":[]}'
    Write-Fixture 'README.md' 'Incremental build fixture.'
    Write-Fixture 'THIRD_PARTY_NOTICES.md' 'Fixture; bundled toolchain notices still apply.'
    Write-Fixture 'docs/fixture.md' 'Documentation is a source input.'
    Write-Fixture 'notices/fixture.txt' 'Fixture notice.'
    Write-Fixture 'assets/current.json' '{"value":1}'
    Write-Fixture 'assets/retired.json' '{"value":"retire me"}'
    Write-Fixture 'user/enabled-mods.json' '{"schema":1,"enabled":["fixture"]}'
    Write-Fixture 'user/mods/fixture/pack.json' '{"schema":1,"id":"fixture"}'

    $null = Run-Build '01-first'
    Assert-Workflow ((Read-CurvedJson (Join-Path $build 'last-build.json')).configure_ran) 'first build explicitly configures'
    Assert-Result (Join-Path $build 'curved_world.exe') 10 'first compiled behavior'
    $first = Object-Times $build
    $log = Run-Build '02-unchanged'
    $second = Object-Times $build
    Assert-Workflow (!(Read-CurvedJson (Join-Path $build 'last-build.json')).configure_ran) 'compatible build skips explicit configure'
    Assert-Workflow ($first.main -eq $second.main -and $first.other -eq $second.other -and (Get-Content -LiteralPath $log -Raw) -match 'ninja: no work to do') 'unchanged build preserves both object timestamps'

    Write-Fixture 'src/other.cpp' "int other() { return 4; }`n"
    $null = Run-Build '03-cpp-edit'
    $third = Object-Times $build
    Assert-Workflow ($third.main -eq $second.main -and $third.other -gt $second.other) 'CPP edit rebuilds only its translation unit'
    Assert-Result (Join-Path $build 'curved_world.exe') 11 'CPP edit reaches executable'
    Write-Fixture 'src/value.hpp' "#define BASE_VALUE 9`n"
    $null = Run-Build '04-header-edit'
    $fourth = Object-Times $build
    Assert-Workflow ($fourth.main -gt $third.main -and $fourth.other -eq $third.other) 'header edit rebuilds only its consumer'
    Assert-Result (Join-Path $build 'curved_world.exe') 13 'header edit reaches executable'
    $configureCount = @(Get-Content -LiteralPath (Join-Path $build 'configure-events.txt')).Count
    Write-Fixture 'cmake/options.cmake' "set(FIXTURE_OPTION 2)`n"
    $null = Run-Build '05-cmake-include'
    Assert-Workflow (@(Get-Content -LiteralPath (Join-Path $build 'configure-events.txt')).Count -gt $configureCount -and !(Read-CurvedJson (Join-Path $build 'last-build.json')).configure_ran) 'Ninja regenerates a changed CMake include'
    $configureCount = @(Get-Content -LiteralPath (Join-Path $build 'configure-events.txt')).Count
    Write-Fixture 'cmake/version.txt' "2`n"
    $null = Run-Build '06-configure-dependency'
    Assert-Workflow (@(Get-Content -LiteralPath (Join-Path $build 'configure-events.txt')).Count -gt $configureCount) 'registered data input triggers CMake regeneration'
    $null = Run-Build '07-debug' @('-Configuration','Debug')
    Assert-Workflow ((Read-CurvedJson (Join-Path $build 'last-build.json')).configure_ran) 'changed configuration explicitly reconfigures'
    $null = Run-Build '08-core' @('-CoreOnly')
    Assert-Workflow ((Read-CurvedJson (Join-Path $build 'last-build.json')).configure_ran) 'changed client option explicitly reconfigures'
    $null = Run-Build '09-client'
    $null = Run-Build '10-forced' @('-Configure')
    Assert-Workflow ((Read-CurvedJson (Join-Path $build 'last-build.json')).configure_ran) 'forced configure remains available'
    $lock = Enter-CurvedLock (Join-Path $build '.curved-build.lock')
    try { $log = Run-Build '11-locked' @() -Fails }
    finally { $lock.Dispose() }
    Assert-Workflow ((Get-Content -LiteralPath $log -Raw) -match 'Another writer owns') 'concurrent writer is refused'
    Write-Fixture 'src/other.cpp' "this is a deliberate compiler error`n"
    $null = Run-Build '12-compiler-failure' @() -Fails
    Write-Fixture 'src/other.cpp' "int other() { return 4; }`n"
    $null = Run-Build '13-recovery'
    Assert-Result (Join-Path $build 'curved_world.exe') 13 'failed compile releases lock and recovery succeeds'

    $null = Run-Playtest '14-first-source-copy'
    $current = Read-CurvedJson (Join-Path $playtest 'current.json')
    $privateBuild = Join-Path $current.owner 'build'
    $beforeTimes = Object-Times $privateBuild
    $beforeHash = Get-CurvedHash (Join-Path $privateBuild 'curved_world.exe')
    $beforeBundle = Join-Path $playtest ('bundles/' + $current.build_id)
    $settings = Join-CurvedOwnedPath $playtest 'profile/settings.json'
    [IO.File]::WriteAllText($settings, '{"volume":0.37}')
    $null = Run-Playtest '15-source-cache-hit'
    $last = Read-CurvedJson (Join-Path $playtest 'last-preparation.json')
    $afterTimes = Object-Times $privateBuild
    Assert-Workflow ($last.mode -ceq 'verified-cache' -and $beforeTimes.main -eq $afterTimes.main -and $beforeTimes.other -eq $afterTimes.other) 'unchanged playtest verifies bundle without invoking compiler'
    Write-Fixture 'assets/current.json' '{"value":2}'
    Write-Fixture 'assets/added.json' '{"value":"new"}'
    Write-Fixture 'user/mods/fixture/pack.json' '{"schema":1,"id":"fixture","value":2}'
    Remove-Item -LiteralPath (Join-CurvedOwnedPath $live 'assets/retired.json')
    $null = Run-Playtest '16-assets-only'
    $current = Read-CurvedJson (Join-Path $playtest 'current.json')
    $bundle = Join-Path $playtest ('bundles/' + $current.build_id)
    $afterTimes = Object-Times $privateBuild
    $last = Read-CurvedJson (Join-Path $playtest 'last-preparation.json')
    Assert-Workflow ($beforeHash -ceq (Get-CurvedHash (Join-Path $privateBuild 'curved_world.exe')) -and $beforeTimes.main -eq $afterTimes.main -and $beforeTimes.other -eq $afterTimes.other) 'content-only copy preserves native objects and executable'
    Assert-Workflow ((Get-Content -LiteralPath (Join-Path $last.report 'build.log') -Raw) -match 'ninja: no work to do') 'content-only private build reports no native work'
    Assert-Workflow ((Test-Path -LiteralPath (Join-Path $bundle 'assets/added.json')) -and !(Test-Path -LiteralPath (Join-Path $bundle 'assets/retired.json')) -and (Test-Path -LiteralPath (Join-Path $beforeBundle 'assets/retired.json'))) 'new bundle removes retired assets and preserves previous bundle'
    Assert-Workflow ((Get-Content -LiteralPath (Join-Path $playtest 'profile/mods/fixture/pack.json') -Raw) -ceq '{"schema":1,"id":"fixture","value":2}' -and (Get-Content -LiteralPath $settings -Raw) -ceq '{"volume":0.37}') 'authored mod edits refresh while playtest settings persist'
    Write-Fixture 'src/other.cpp' "int other() { return 6; }`n"
    [IO.File]::SetLastWriteTimeUtc((Join-Path $live 'src/other.cpp'), [DateTime]::new(2000,1,1))
    $null = Run-Playtest '17-backdated-edit'
    Assert-Result (Join-Path $privateBuild 'curved_world.exe') 15 'backdated changed source still recompiles'
    $goodPointer = Get-CurvedHash (Join-Path $playtest 'current.json')
    Write-Fixture 'src/value.hpp' "#define BASE_VALUE 11`n"
    Write-Fixture 'src/other.cpp' "deliberate partial-build failure`n"
    $null = Run-Playtest '18-failed-private-build' @() -Fails
    Assert-Workflow ((Get-CurvedHash (Join-Path $playtest 'current.json')) -ceq $goodPointer) 'failed private build preserves selected version'
    Write-Fixture 'src/value.hpp' "#define BASE_VALUE 9`n"
    Write-Fixture 'src/other.cpp' "int other() { return 4; }`n"
    foreach ($path in @('src/value.hpp','src/other.cpp')) { [IO.File]::SetLastWriteTimeUtc((Join-Path $live $path), [DateTime]::new(2000,1,1)) }
    $null = Run-Playtest '19-failed-edit-reverted'
    Assert-Result (Join-Path $privateBuild 'curved_world.exe') 13 'backdated revert after partial failure rebuilds correct behavior'
    $current = Read-CurvedJson (Join-Path $playtest 'current.json')
    $bundle = Join-Path $playtest ('bundles/' + $current.build_id)
    $tampered = Join-CurvedOwnedPath $bundle 'assets/current.json'
    [IO.File]::SetAttributes($tampered, [IO.FileAttributes]::Normal)
    [IO.File]::WriteAllText($tampered, 'changed cached bytes')
    $log = Run-Playtest '20-tampered-runtime' @() -Fails
    Assert-Workflow ((Get-Content -LiteralPath $log -Raw) -match 'Frozen playtest files changed') 'cached runtime byte changes are rejected'
    $null = Run-Playtest '21-clean-rebuild' @('-Rebuild')
    $current = Read-CurvedJson (Join-Path $playtest 'current.json')
    $bundle = Join-Path $playtest ('bundles/' + $current.build_id)
    Assert-Workflow ((Get-Content -LiteralPath (Join-Path $bundle 'assets/current.json') -Raw) -ceq '{"value":2}') 'explicit clean rebuild recovers a verified bundle'
    Assert-Result (Join-Path $bundle 'curved_world.exe') 13 'source-copy executable runs with bundled runtimes'
    Write-CurvedJson (Join-Path $fixture 'result.json') ([ordered]@{success=$true; checks=$script:checks; fixture=$fixture})
    Write-Host "Build/playtest workflow passed: $fixture"
} catch {
    Write-CurvedJson (Join-Path $fixture 'result.json') ([ordered]@{success=$false; checks=$script:checks; message=$_.Exception.Message; at=$_.ScriptStackTrace})
    throw
}
