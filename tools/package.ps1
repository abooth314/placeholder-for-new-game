$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $root 'dist/curved_world.exe'
$hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLower()
$packages=Join-Path $root 'out/packages'
$stage=Join-Path $packages ('staging-'+$hash.Substring(0,12)+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach($name in @('curved_world.exe','libc++.dll','libunwind.dll','assets','notices','docs','README.md','THIRD_PARTY_NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root ('dist/'+$name)) -Destination $stage -Recurse -Force}
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'user/mods') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'user/enabled-mods.json') -Destination (Join-Path $stage 'user') -Force
Copy-Item -LiteralPath (Join-Path $root 'user/mods/wardens') -Destination (Join-Path $stage 'user/mods') -Recurse -Force
$archive=Join-Path $packages 'CurvedWorldPrototype-Windows-x64.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
[ordered]@{archive=$archive;sha256=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash;executable_sha256=$hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $packages 'package.json') -Encoding utf8
Write-Host "Created $archive"
