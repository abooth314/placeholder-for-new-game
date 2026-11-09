param(
    [switch]$PrepareOnly,
    [switch]$Rebuild,
    [ValidateRange(1,4)][int]$Jobs = 4,
    [string[]]$GameArguments = @(),
    [string]$SourceDirectory = '',
    [string]$PlaytestDirectory = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$repo = Split-Path -Parent $PSScriptRoot
$source = if ($SourceDirectory) { [IO.Path]::GetFullPath($SourceDirectory) } else { $repo }
$playtest = if ($PlaytestDirectory) { [IO.Path]::GetFullPath($PlaytestDirectory) } else { Join-Path $repo 'out/playtest' }
$boundary = $repo.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
if (!$playtest.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) { throw 'Playtest outputs must be inside this repository' }
$playtest = Join-CurvedOwnedPath $repo $playtest.Substring($boundary.Length)
if ($source -ine $repo) {
    if (!$source.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) { throw 'Source copies must be inside this repository' }
    $source = Join-CurvedOwnedPath $repo $source.Substring($boundary.Length)
}
foreach ($argument in $GameArguments) {
    if ($argument -in @('--user','--assets','--output')) { throw 'Playtest owns --user, --assets and --output; edit the source tree or use the direct executable for custom paths' }
}
$runName = (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$report = Join-CurvedOwnedPath $playtest ('reports/' + $runName)
[IO.Directory]::CreateDirectory($report) | Out-Null
$currentPath = Join-CurvedOwnedPath $playtest 'current.json'
$setup = Join-Path $PSScriptRoot 'setup-toolchain.ps1'
$tools = Get-CurvedTools (Join-Path $repo '.tools') $setup
$fingerprint = Get-CurvedToolchainFingerprint $tools $setup

function Get-RuntimeManifest([string]$Root) {
    $paths = @()
    foreach ($name in @('curved_world.exe','libc++.dll','libunwind.dll','README.md','THIRD_PARTY_NOTICES.md')) {
        if (!(Test-Path -LiteralPath (Join-CurvedOwnedPath $Root $name) -PathType Leaf)) { throw "Incomplete playtest: $name" }
    }
    # Include every runtime file; an extra/retired asset also invalidates a hit.
    foreach ($directory in @('assets','docs','notices','user')) {
        if (!(Test-Path -LiteralPath (Join-CurvedOwnedPath $Root $directory) -PathType Container)) { throw "Incomplete playtest: $directory" }
    }
    foreach ($item in Get-ChildItem -LiteralPath $Root -Force) {
        if ($item.PSIsContainer) { $paths += @(Get-CurvedTreePaths $Root $item.Name) }
        elseif ($item.Name -cne 'manifest.json') { $paths += $item.Name }
    }
    Get-CurvedFileManifest $Root $paths
}

function Test-Bundle([string]$BuildId, [string]$ExpectedRuntimeHash = '') {
    if ($BuildId -cnotmatch '^build-[a-f0-9]{16}-[a-f0-9]{16}$') { throw 'Invalid playtest build identity' }
    $directory = Join-CurvedOwnedPath $playtest ('bundles/' + $BuildId)
    $manifest = Read-CurvedJson (Join-Path $directory 'manifest.json')
    $actual = @(Get-RuntimeManifest $directory)
    $hash = Get-CurvedManifestHash $actual
    $exe = @($actual | Where-Object { $_.path -ceq 'curved_world.exe' })[0]
    if ($manifest.schema -ne 1 -or $manifest.build_id -cne $BuildId -or $manifest.runtime_sha256 -cne $hash -or
        $manifest.executable_sha256 -cne $exe.sha256 -or
        $BuildId -cne ('build-' + $exe.sha256.Substring(0,16) + '-' + $hash.Substring(0,16)) -or
        ($ExpectedRuntimeHash -and $hash -cne $ExpectedRuntimeHash)) {
        throw "Frozen playtest files changed: $directory. Evidence was preserved; use -Rebuild for a fresh compilation."
    }
    foreach ($dll in @('libc++.dll','libunwind.dll')) {
        if ((Get-CurvedHash (Join-Path $directory $dll)) -cne (Get-CurvedHash (Join-Path $tools.bin $dll))) { throw "Playtest runtime differs from the pinned toolchain: $dll" }
    }
    return [pscustomobject]@{id=$BuildId; directory=$directory; manifest=$manifest}
}

function Copy-DependencyArchives([string]$Snapshot) {
    $lock = Read-CurvedJson (Join-Path $Snapshot 'cmake/dependencies.lock.json')
    foreach ($dependency in $lock.dependencies) {
        if ($dependency.archive -notmatch '^[a-zA-Z0-9_.+-]+$' -or $dependency.sha256 -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid dependency pin' }
        $relative = '.dependencies/downloads/' + $dependency.archive
        $cached = Join-CurvedOwnedPath $repo $relative
        $copy = Join-CurvedOwnedPath $Snapshot $relative
        if (Test-Path -LiteralPath $cached -PathType Leaf) {
            if ((Get-CurvedHash $cached) -cne $dependency.sha256) { throw "Dependency archive checksum failed: $cached" }
            Copy-CurvedRuntimeFile $cached $copy
        }
        # If absent, CMake downloads the pinned URL and verifies its SHA-256 in
        # this private build. Active dependency source/build trees are not shared.
    }
}

function Select-Bundle {
    Write-Host '[Playtest] Hashing current source, content and workflow inputs.'
    $before = @(Get-CurvedSourceManifest $source)
    $sourceHash = Get-CurvedManifestHash $before
    Write-CurvedJson (Join-Path $report 'source-manifest.json') ([ordered]@{source=$source; source_sha256=$sourceHash; files=$before})
    if (!$Rebuild -and (Test-Path -LiteralPath $currentPath -PathType Leaf)) {
        $current = Read-CurvedJson $currentPath
        if ($current.source -ieq $source -and $current.source_sha256 -ceq $sourceHash -and $current.toolchain_sha256 -ceq $fingerprint) {
            Write-Host '[Playtest] Source unchanged; verifying every frozen runtime file.'
            $selected = Test-Bundle $current.build_id $current.runtime_sha256
            if ((Get-CurvedManifestHash @(Get-CurvedSourceManifest $source)) -cne $sourceHash) { throw 'Source changed during verification; run playtest again' }
            return [pscustomobject]@{bundle=$selected; source_hash=$sourceHash; mode='verified-cache'; owner=$current.owner; copied=0; removed=0}
        }
    }
    $ownerName = if ($Rebuild) { 'clean-' + $runName } else { 'incremental-' + $fingerprint.Substring(0,16) }
    $owner = Join-CurvedOwnedPath $playtest ('cache/' + $ownerName)
    $snapshot = Join-CurvedOwnedPath $owner 'source'
    $build = Join-CurvedOwnedPath $owner 'build'
    [IO.Directory]::CreateDirectory($snapshot) | Out-Null
    $ownerPath = Join-Path $owner 'owner.json'
    if (Test-Path -LiteralPath $ownerPath -PathType Leaf) {
        $identity = Read-CurvedJson $ownerPath
        if ($identity.source -ine $source -or $identity.toolchain_sha256 -cne $fingerprint) { throw 'Private build owner does not match this source/toolchain' }
    } else { Write-CurvedJson $ownerPath ([ordered]@{source=$source; toolchain_sha256=$fingerprint}) }
    $compiledPath = Join-Path $owner 'compiled-inputs.json'
    $compiled = @()
    if (Test-Path -LiteralPath $compiledPath -PathType Leaf) { $compiled = @((Read-CurvedJson $compiledPath).inputs) }
    Write-Host '[Playtest] Synchronizing the private source copy; preserving unchanged inputs.'
    $sync = Sync-CurvedSource $source $snapshot $before $compiled
    $snapshotHash = Get-CurvedManifestHash @(Get-CurvedSourceManifest $snapshot)
    if ($snapshotHash -cne $sourceHash -or (Get-CurvedManifestHash @(Get-CurvedSourceManifest $source)) -cne $sourceHash) { throw 'Source changed during copying; no compiler was started' }
    Copy-DependencyArchives $snapshot
    $inputs = @(Get-CurvedInputTimes $snapshot $before)
    # Store the latest attempted inputs before calling the compiler, including
    # failed attempts. Reverting a partial build cannot resurrect stale objects.
    Write-CurvedJson $compiledPath ([ordered]@{state='attempted'; source_sha256=$sourceHash; inputs=$inputs})
    $stage = Join-CurvedOwnedPath $playtest ('staging/' + $runName)
    Write-Host "[Playtest] Building copied source with Ninja (maximum $Jobs jobs)."
    $buildParameters = @{SourceDirectory=$snapshot; BuildDirectory=$build; InstallDirectory=$stage; ToolchainDirectory=$tools.directory; Target='curved_world'; Jobs=$Jobs}
    & (Join-Path $PSScriptRoot 'build.ps1') @buildParameters *>&1 | Tee-Object -FilePath (Join-Path $report 'build.log') | Out-Host
    if (!$?) { throw 'Private source build failed; previous selection and all logs were preserved' }
    if ((Get-CurvedManifestHash @(Get-CurvedSourceManifest $snapshot)) -cne $sourceHash -or
        (Get-CurvedManifestHash @(Get-CurvedSourceManifest $source)) -cne $sourceHash) { throw 'Source changed during compilation; no new playtest was selected' }
    Write-CurvedJson $compiledPath ([ordered]@{state='compiled'; source_sha256=$sourceHash; inputs=$inputs})
    $runtime = @(Get-RuntimeManifest $stage)
    $runtimeHash = Get-CurvedManifestHash $runtime
    $exe = @($runtime | Where-Object { $_.path -ceq 'curved_world.exe' })[0]
    $buildId = 'build-' + $exe.sha256.Substring(0,16) + '-' + $runtimeHash.Substring(0,16)
    Write-CurvedJson (Join-Path $stage 'manifest.json') ([ordered]@{schema=1; build_id=$buildId; source_sha256=$sourceHash;
        toolchain_sha256=$fingerprint; runtime_sha256=$runtimeHash; executable_sha256=$exe.sha256; created_utc=[DateTime]::UtcNow.ToString('o'); files=$runtime})
    $destination = Join-CurvedOwnedPath $playtest ('bundles/' + $buildId)
    [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
    if (Test-Path -LiteralPath $destination) {
        try { $null = Test-Bundle $buildId $runtimeHash }
        catch {
            if (!$Rebuild) { throw }
            $quarantine = Join-CurvedOwnedPath $playtest ('quarantine/' + $buildId + '-' + $runName)
            [IO.Directory]::CreateDirectory((Split-Path -Parent $quarantine)) | Out-Null
            # Preserve a changed bundle rather than overwrite its evidence.
            Move-Item -LiteralPath $destination -Destination $quarantine
            Write-Host "[Playtest] Preserved changed bundle: $quarantine"
        }
    }
    if (!(Test-Path -LiteralPath $destination)) {
        # Both absolute move endpoints are verified descendants of this owner.
        foreach ($entry in $runtime) {
            $path = Join-CurvedOwnedPath $stage $entry.path
            [IO.File]::SetAttributes($path, [IO.File]::GetAttributes($path) -bor [IO.FileAttributes]::ReadOnly)
        }
        Move-Item -LiteralPath $stage -Destination $destination
    }
    $selected = Test-Bundle $buildId $runtimeHash
    Write-CurvedJson $currentPath ([ordered]@{schema=1; source=$source; source_sha256=$sourceHash; toolchain_sha256=$fingerprint;
        build_id=$buildId; runtime_sha256=$runtimeHash; owner=$owner; selected_utc=[DateTime]::UtcNow.ToString('o')})
    return [pscustomobject]@{bundle=$selected; source_hash=$sourceHash; mode='source-build'; owner=$owner; copied=$sync.copied; removed=$sync.removed}
}

function Sync-Profile([string]$Bundle, [string]$Profile) {
    [IO.Directory]::CreateDirectory($Profile) | Out-Null
    $userSource = Join-Path $Bundle 'user'
    $paths = @('enabled-mods.json') + @(Get-CurvedTreePaths $userSource 'mods')
    $desired = @(Get-CurvedFileManifest $userSource $paths)
    $record = Join-CurvedOwnedPath $playtest 'profile-inputs.json'
    $wanted = @{}
    foreach ($entry in $desired) { $wanted[$entry.path] = $true }
    if (Test-Path -LiteralPath $record -PathType Leaf) {
        foreach ($entry in @((Read-CurvedJson $record).files)) {
            if (!$wanted.ContainsKey($entry.path)) {
                $retired = Join-CurvedOwnedPath $Profile $entry.path
                if (Test-Path -LiteralPath $retired -PathType Leaf) { Remove-Item -LiteralPath $retired -Force }
            }
        }
    }
    foreach ($entry in $desired) {
        $to = Join-CurvedOwnedPath $Profile $entry.path
        Copy-CurvedRuntimeFile (Join-CurvedOwnedPath $userSource $entry.path) $to
        [IO.File]::SetAttributes($to, [IO.FileAttributes]::Normal)
    }
    Write-CurvedJson $record ([ordered]@{files=$desired})
}

$prepare = Enter-CurvedLock (Join-CurvedOwnedPath $playtest 'prepare.lock')
try {
    $selection = Select-Bundle
    Write-CurvedJson (Join-Path $report 'preparation.json') ([ordered]@{success=$true; mode=$selection.mode; source_sha256=$selection.source_hash;
        build_id=$selection.bundle.id; executable_sha256=$selection.bundle.manifest.executable_sha256; owner=$selection.owner; copied_files=$selection.copied; removed_files=$selection.removed})
    Write-CurvedJson (Join-CurvedOwnedPath $playtest 'last-preparation.json') ([ordered]@{report=$report; build_id=$selection.bundle.id; mode=$selection.mode})
} catch {
    Write-CurvedJson (Join-Path $report 'failure.json') ([ordered]@{success=$false; message=$_.Exception.Message; at=$_.ScriptStackTrace})
    throw
} finally { $prepare.Dispose() }

$profile = Join-CurvedOwnedPath $playtest 'profile'
$profileLock = Enter-CurvedLock (Join-CurvedOwnedPath $profile 'running.lock')
try {
    Sync-Profile $selection.bundle.directory $profile
    $executable = Join-Path $selection.bundle.directory 'curved_world.exe'
    $arguments = @('--user', $profile, '--output', $report) + $GameArguments
    Write-CurvedJson (Join-Path $report 'launch.json') ([ordered]@{prepared_only=[bool]$PrepareOnly; executable=$executable; arguments=$arguments;
        user_directory=$profile; build_id=$selection.bundle.id; executable_sha256=$selection.bundle.manifest.executable_sha256})
    Write-Host "[Playtest] Version: $($selection.bundle.id)"
    Write-Host "[Playtest] Settings/mod copy: $profile"
    Write-Host "[Playtest] Report: $report"
    if (!$PrepareOnly) {
        $argumentText = ($arguments | ForEach-Object { ConvertTo-CurvedNativeArgument $_ }) -join ' '
        $oldPath = $env:PATH
        $game = $null
        try {
            # Prove the bundle supplies its own non-system runtime DLLs.
            $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
            $game = Start-Process -FilePath $executable -ArgumentList $argumentText -WorkingDirectory $selection.bundle.directory -WindowStyle Normal -PassThru `
                -RedirectStandardOutput (Join-Path $report 'stdout.log') -RedirectStandardError (Join-Path $report 'stderr.log')
            $null = $game.Handle
        } finally { $env:PATH = $oldPath }
        try {
            Write-Host '[Playtest] The game is running; this launcher waits for it to close.'
            $game.WaitForExit()
            $game.Refresh()
            Write-CurvedJson (Join-Path $report 'exit.json') ([ordered]@{exit_code=$game.ExitCode; exited_utc=[DateTime]::UtcNow.ToString('o')})
            if ($game.ExitCode -ne 0) { throw "Game exited with code $($game.ExitCode); see $report/stderr.log" }
        } finally { if ($game) { $game.Dispose() } }
    } else { Write-Host '[Playtest] Prepared only; no game started.' }
} finally { $profileLock.Dispose() }
