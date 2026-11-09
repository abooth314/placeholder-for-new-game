param([string]$CacheDirectory = '', [string]$DestinationDirectory = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$destination = if ($DestinationDirectory) { [IO.Path]::GetFullPath($DestinationDirectory) } else { Join-Path $root '.tools' }
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$packages = @(
    @{ Name='llvm-mingw-20260616-ucrt-x86_64.zip'; Folder='llvm-mingw-20260616-ucrt-x86_64'; Url='https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-ucrt-x86_64.zip'; Hash='b9b68a4d276e16fa25802aaba458e4638f64b3884c290aaccdc2d87083b6ca35'; Probe='bin/x86_64-w64-mingw32-clang++.exe' },
    @{ Name='cmake-4.1.0-windows-x86_64.zip'; Folder='cmake-4.1.0-windows-x86_64'; Url='https://github.com/Kitware/CMake/releases/download/v4.1.0/cmake-4.1.0-windows-x86_64.zip'; Hash='5c09d23786daa89ac433a1a8237f2bdee9f420fb0f2b8a9aded7894d532c0076'; Probe='bin/cmake.exe' },
    @{ Name='ninja-1.13.1-win.zip'; Folder='ninja-1.13.1'; Url='https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip'; Hash='26a40fa8595694dec2fad4911e62d29e10525d2133c9a4230b66397774ae25bf'; Probe='ninja.exe' }
)
foreach ($package in $packages) {
    $archive = Join-Path $destination $package.Name
    $folder = Join-Path $destination $package.Folder
    if (!(Test-Path -LiteralPath $archive)) {
        $cached = if ($CacheDirectory) { Join-Path $CacheDirectory $package.Name } else { '' }
        if ($cached -and (Test-Path -LiteralPath $cached)) { Copy-Item -LiteralPath $cached -Destination $archive }
        else {
            & curl.exe --fail --location --retry 3 --output "$archive.partial" $package.Url
            if ($LASTEXITCODE -ne 0) { throw "Download failed: $($package.Name)" }
            if ((Get-FileHash -LiteralPath "$archive.partial" -Algorithm SHA256).Hash -ne $package.Hash) { throw "Checksum failed: $($package.Name)" }
            Move-Item -LiteralPath "$archive.partial" -Destination $archive
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $package.Hash) { throw "Checksum failed: $($package.Name)" }
    if (!(Test-Path -LiteralPath (Join-Path $folder $package.Probe))) {
        if ($package.Folder -like 'ninja-*') { New-Item -ItemType Directory -Force -Path $folder | Out-Null; & tar.exe -xf $archive -C $folder }
        else { & tar.exe -xf $archive -C $destination }
        if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $($package.Name)" }
    }
    Write-Host "Verified $($package.Folder)"
}
