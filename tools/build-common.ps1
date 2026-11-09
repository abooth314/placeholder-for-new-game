# Shared Windows PowerShell 5.1-compatible build/playtest plumbing.
# Adapted to this prototype from the read-only Mandelrift V2 workflow.
# A cmd launcher can inherit PowerShell 7's module search path. Select this
# host's own built-ins explicitly, as V2 does, before resolving their commands.
Import-Module ([IO.Path]::Combine($PSHOME, 'Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1')) -ErrorAction Stop
Import-Module ([IO.Path]::Combine($PSHOME, 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')) -ErrorAction Stop
Set-StrictMode -Version Latest

function Write-CurvedJson([string]$Path, $Value) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 16) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

function Read-CurvedJson([string]$Path) {
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-CurvedHash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-CurvedManifestHash($Entries) {
    $lines = @($Entries | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.length, $_.sha256 })
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n") + "`n")
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hasher.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $hasher.Dispose() }
}

function Join-CurvedOwnedPath([string]$Root, [string]$Relative) {
    $boundary = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $path = [IO.Path]::GetFullPath((Join-Path $boundary $Relative))
    if (!$path.StartsWith($boundary + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Path escapes its owner: $path" }
    $ancestor = $path
    while ($ancestor -and $ancestor.Length -ge $boundary.Length) {
        if (Test-Path -LiteralPath $ancestor) {
            if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Cannot snapshot a linked path: $ancestor" }
        }
        $ancestor = Split-Path -Parent $ancestor
    }
    return $path
}

function Get-CurvedTreePaths([string]$Root, [string]$Relative, [switch]$AllowMissing) {
    $start = Join-CurvedOwnedPath $Root $Relative
    if (!(Test-Path -LiteralPath $start -PathType Container)) {
        if ($AllowMissing) { return }
        throw "Required source directory is missing: $start"
    }
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($start)
    while ($pending.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Cannot snapshot a linked path: $($item.FullName)" }
            if ($item.PSIsContainer) {
                if ($item.Name -ne '__pycache__') { $pending.Push($item.FullName) }
            } elseif ($item.Extension -notin @('.pyc', '.pyo')) { $item.FullName.Substring($prefix.Length).Replace('\', '/') }
        }
    }
}

function Get-CurvedFileManifest([string]$Root, [string[]]$Paths) {
    $ordered = [string[]]@($Paths)
    [Array]::Sort($ordered, [StringComparer]::Ordinal)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $ordered) {
        if (!$seen.Add($relative)) { throw "Duplicate manifest path: $relative" }
        $path = Join-CurvedOwnedPath $Root $relative
        $item = Get-Item -LiteralPath $path -Force
        [pscustomobject]@{path=$relative.Replace('\', '/'); length=$item.Length; sha256=(Get-CurvedHash $path)}
    }
}

function Get-CurvedSourceManifest([string]$Root, [switch]$AllowMissing) {
    $paths = @()
    foreach ($name in @('CMakeLists.txt', 'README.md', 'THIRD_PARTY_NOTICES.md', 'user/enabled-mods.json')) {
        if (!$AllowMissing -or (Test-Path -LiteralPath (Join-CurvedOwnedPath $Root $name) -PathType Leaf)) { $paths += $name }
    }
    foreach ($name in @('AGENTS.md', '.clang-format', '.editorconfig', '.gitattributes', '.gitignore', 'Launch_Prototype.bat', 'Playtest_Prototype.bat')) {
        if (Test-Path -LiteralPath (Join-CurvedOwnedPath $Root $name) -PathType Leaf) { $paths += $name }
    }
    foreach ($directory in @('src', 'tests', 'cmake', 'assets', 'notices', 'docs', 'tools', 'user/mods')) { $paths += @(Get-CurvedTreePaths $Root $directory -AllowMissing:$AllowMissing) }
    Get-CurvedFileManifest $Root $paths
}

function Sync-CurvedSource([string]$Source, [string]$Destination, $Expected, $CompiledInputs) {
    foreach ($directory in @('src', 'tests', 'cmake', 'assets', 'notices', 'docs', 'tools', 'user/mods')) {
        if (Test-Path -LiteralPath (Join-CurvedOwnedPath $Source $directory) -PathType Container) {
            [IO.Directory]::CreateDirectory((Join-CurvedOwnedPath $Destination $directory)) | Out-Null
        }
    }
    $old = @{}
    foreach ($entry in @(Get-CurvedSourceManifest $Destination -AllowMissing)) { $old[$entry.path] = $entry }
    $compiled = @{}
    foreach ($entry in @($CompiledInputs)) { $compiled[$entry.path] = $entry }
    $wanted = @{}
    foreach ($entry in $Expected) { $wanted[$entry.path] = $true }
    $removed = 0
    foreach ($entry in @($old.Values)) {
        if (!$wanted.ContainsKey($entry.path)) {
            $retired = Join-CurvedOwnedPath $Destination $entry.path
            # Delete only a resolved file in the owned private source tree.
            Remove-Item -LiteralPath $retired -Force
            $removed++
        }
    }
    $copied = 0
    foreach ($entry in $Expected) {
        $to = Join-CurvedOwnedPath $Destination $entry.path
        $previous = $old[$entry.path]
        if ($previous -and $previous.length -eq $entry.length -and $previous.sha256 -ceq $entry.sha256) {
            $prior = $compiled[$entry.path]
            if ($prior -and $prior.sha256 -ceq $entry.sha256 -and $prior.writeTimeUtcTicks -gt 0) {
                [IO.File]::SetLastWriteTimeUtc($to, [DateTime]::new([long]$prior.writeTimeUtcTicks, [DateTimeKind]::Utc))
            }
            continue
        }
        $from = Join-CurvedOwnedPath $Source $entry.path
        [IO.Directory]::CreateDirectory((Split-Path -Parent $to)) | Out-Null
        if (Test-Path -LiteralPath $to -PathType Leaf) { [IO.File]::SetAttributes($to, [IO.FileAttributes]::Normal) }
        [IO.File]::Copy($from, $to, $true)
        [IO.File]::SetAttributes($to, [IO.FileAttributes]::Normal)
        # Changed bytes must reach Ninja even after a backdated edit or a
        # failed build followed by a revert. Unchanged bytes keep their times.
        [IO.File]::SetLastWriteTimeUtc($to, [DateTime]::UtcNow)
        $copied++
    }
    return [pscustomobject]@{copied=$copied; removed=$removed}
}

function Get-CurvedInputTimes([string]$Root, $Entries) {
    foreach ($entry in $Entries) {
        [pscustomobject]@{path=$entry.path; length=$entry.length; sha256=$entry.sha256;
            writeTimeUtcTicks=[IO.File]::GetLastWriteTimeUtc((Join-CurvedOwnedPath $Root $entry.path)).Ticks}
    }
}

function Enter-CurvedLock([string]$Path) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    try { return [IO.File]::Open($Path, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch [IO.IOException] { throw "Another writer owns $Path. Wait for that build or playtest preparation to finish." }
}

function Copy-CurvedRuntimeFile([string]$Source, [string]$Destination) {
    if ((Test-Path -LiteralPath $Destination -PathType Leaf) -and (Get-CurvedHash $Source) -ceq (Get-CurvedHash $Destination)) { return }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Destination)) | Out-Null
    if (Test-Path -LiteralPath $Destination -PathType Leaf) { [IO.File]::SetAttributes($Destination, [IO.FileAttributes]::Normal) }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Get-CurvedTools([string]$ToolchainDirectory, [string]$SetupScript) {
    $directory = [IO.Path]::GetFullPath($ToolchainDirectory)
    $bin = Join-Path $directory 'llvm-mingw-20260616-ucrt-x86_64/bin'
    $tools = [ordered]@{directory=$directory; bin=$bin;
        cc=(Join-Path $bin 'x86_64-w64-mingw32-clang.exe'); cxx=(Join-Path $bin 'x86_64-w64-mingw32-clang++.exe');
        cmake=(Join-Path $directory 'cmake-4.1.0-windows-x86_64/bin/cmake.exe'); ninja=(Join-Path $directory 'ninja-1.13.1/ninja.exe')}
    if (@('cc', 'cxx', 'cmake', 'ninja') | Where-Object { !(Test-Path -LiteralPath $tools[$_] -PathType Leaf) }) {
        & $SetupScript -DestinationDirectory $directory
        if (!$?) { throw 'Free toolchain setup failed' }
    }
    return [pscustomobject]$tools
}

function Get-CurvedToolchainFingerprint($Tools, [string]$SetupScript) {
    $paths = @($Tools.cc, $Tools.cxx, $Tools.cmake, $Tools.ninja, $SetupScript)
    foreach ($name in @('clang-22.exe', 'libclang-cpp.dll', 'libc++.dll', 'libunwind.dll')) { $paths += Join-Path $Tools.bin $name }
    $entries = foreach ($path in $paths) {
        [pscustomobject]@{path=[IO.Path]::GetFullPath($path).Replace('\', '/'); length=(Get-Item -LiteralPath $path).Length; sha256=(Get-CurvedHash $path)}
    }
    return Get-CurvedManifestHash $entries
}

function ConvertTo-CurvedNativeArgument([string]$Value) {
    if ($Value -ne '' -and $Value -notmatch '[\s"]') { return $Value }
    $quoted = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $quoted = [regex]::Replace($quoted, '(\\+)$', '$1$1')
    return '"' + $quoted + '"'
}

function Invoke-CurvedNative([string]$Executable, [string[]]$Arguments, [string]$Directory, [string]$LogPrefix) {
    # Read both streams concurrently. Windows PowerShell 5.1 otherwise promotes
    # redirected native stderr (even a CMake warning) to a terminating error.
    [IO.Directory]::CreateDirectory((Split-Path -Parent $LogPrefix)) | Out-Null
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.Arguments = ($Arguments | ForEach-Object { ConvertTo-CurvedNativeArgument $_ }) -join ' '
    $start.WorkingDirectory = $Directory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    $outLog = [IO.StreamWriter]::new($LogPrefix + '.stdout.log', $false, [Text.UTF8Encoding]::new($false))
    $errLog = [IO.StreamWriter]::new($LogPrefix + '.stderr.log', $false, [Text.UTF8Encoding]::new($false))
    try {
        $null = $process.Start()
        $outTask = $process.StandardOutput.ReadLineAsync()
        $errTask = $process.StandardError.ReadLineAsync()
        while ($null -ne $outTask -or $null -ne $errTask) {
            $read = $false
            if ($null -ne $outTask -and $outTask.IsCompleted) {
                $line = $outTask.GetAwaiter().GetResult()
                if ($null -eq $line) { $outTask = $null }
                else { $outLog.WriteLine($line); $outLog.Flush(); Write-Host $line; $outTask = $process.StandardOutput.ReadLineAsync() }
                $read = $true
            }
            if ($null -ne $errTask -and $errTask.IsCompleted) {
                $line = $errTask.GetAwaiter().GetResult()
                if ($null -eq $line) { $errTask = $null }
                else { $errLog.WriteLine($line); $errLog.Flush(); Write-Host $line; $errTask = $process.StandardError.ReadLineAsync() }
                $read = $true
            }
            if (!$read) { Start-Sleep -Milliseconds 15 }
        }
        $process.WaitForExit()
        return $process.ExitCode
    } finally { $outLog.Dispose(); $errLog.Dispose(); $process.Dispose() }
}
