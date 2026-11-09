param([int]$ProfileSeconds=30, [string]$EvidenceDirectory='out/validation')
$ErrorActionPreference='Stop'
if($ProfileSeconds -lt 5 -or $ProfileSeconds -gt 120){throw 'Profile duration must be 5..120 seconds'}
$root=Split-Path -Parent $PSScriptRoot
$evidence=if([IO.Path]::IsPathRooted($EvidenceDirectory)){$EvidenceDirectory}else{Join-Path $root $EvidenceDirectory}
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$exe=Join-Path $root 'dist/curved_world.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build the client first'}
$binaryHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$hardware=[ordered]@{
    cpu=@(Get-CimInstance Win32_Processor | Select-Object -ExpandProperty Name)
    adapters=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    windows=(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,OSArchitecture)
    executable_sha256=$binaryHash
}
$hardware | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'hardware.json') -Encoding utf8
# Keep the compiler directory out of PATH during distributable launch checks.
$env:PATH=($env:PATH -split ';' | Where-Object {$_ -and $_ -notmatch 'llvm-mingw|placeholder-for-new-game[\\/]\.tools'}) -join ';'

function Seed-User([string]$directory) {
    New-Item -ItemType Directory -Force -Path (Join-Path $directory 'mods') | Out-Null
    if(!(Test-Path -LiteralPath (Join-Path $directory 'enabled-mods.json'))){Copy-Item -LiteralPath (Join-Path $root 'user/enabled-mods.json') -Destination $directory}
    if(!(Test-Path -LiteralPath (Join-Path $directory 'mods/wardens'))){Copy-Item -LiteralPath (Join-Path $root 'user/mods/wardens') -Destination (Join-Path $directory 'mods') -Recurse}
}
function Run-Native([string]$label,[string]$program,[string[]]$arguments,[string]$workingDirectory,[string]$resultDirectory) {
    New-Item -ItemType Directory -Force -Path $resultDirectory | Out-Null
    $stdout=Join-Path $resultDirectory 'stdout.log'
    $stderr=Join-Path $resultDirectory 'stderr.log'
    $start=Get-Date
    $process=Start-Process -FilePath $program -ArgumentList $arguments -WorkingDirectory $workingDirectory -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    while(!$process.WaitForExit(1000)){
        if(((Get-Date)-$start).TotalSeconds -gt ($ProfileSeconds+120)){Stop-Process -Id $process.Id -Force;throw "$label timed out; logs preserved in $resultDirectory"}
    }
    $process.Refresh()
    if($process.ExitCode -ne 0){Get-Content -LiteralPath $stderr -Tail 20;throw "$label exited $($process.ExitCode)"}
    $reportPath=Join-Path $resultDirectory 'report.json'
    if(!(Test-Path -LiteralPath $reportPath)){throw "$label did not produce its required report"}
    $report=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if(!$report.success -or $report.lua_errors -ne 0 -or $report.path_failures -ne 0){throw "$label has a failed native receipt"}
    [ordered]@{label=$label;program=$program;arguments=$arguments;working_directory=$workingDirectory;executable_sha256=$binaryHash;exit_code=$process.ExitCode;wall_seconds=((Get-Date)-$start).TotalSeconds} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $resultDirectory 'invocation.json') -Encoding utf8
    Write-Host "PASS $label"
    return $report
}
$working=Join-Path $evidence 'Different working directory'
New-Item -ItemType Directory -Force -Path $working | Out-Null
$rendered=Join-Path $evidence 'rendered'
Seed-User (Join-Path $rendered 'user')
$report=Run-Native 'Rendered controls and resource lifecycle' $exe @('--diagnostic','--warmup-ticks','180','--map','0','--no-vsync','--user',('"'+(Join-Path $rendered 'user')+'"'),'--output',('"'+$rendered+'"')) $working $rendered
if($report.captures.Count -ne 9 -or !$report.context_recreation -or !$report.settings_roundtrip -or !$report.synthetic_ui_capture){throw 'Rendered diagnostic omitted a required consumer'}
if(!$report.audio.device_open -or $report.audio.played_events -lt 1 -or $report.audio.generated_frames -lt 1){throw 'Native audio device/stream was not exercised'}
foreach($name in $report.captures){if(!(Test-Path -LiteralPath (Join-Path $rendered ($name+'.bmp')))){throw "Missing rendered capture $name"}}

foreach($map in @(0,1)){
    $profile=Join-Path $evidence $(if($map -eq 0){'profile-open'}else{'profile-crossings'})
    Seed-User (Join-Path $profile 'user')
    $report=Run-Native "Populated map $map profile" $exe @('--profile-seconds',"$ProfileSeconds",'--fill-population','--population','240','--map',"$map",'--width','1440','--height','900','--no-vsync','--user',('"'+(Join-Path $profile 'user')+'"'),'--output',('"'+$profile+'"')) $working $profile
    if($report.peak_actors -lt 240 -or $report.wall_duration_s -lt $ProfileSeconds -or $report.projectile_shots -lt 1 -or $report.heals -lt 1){throw "Map $map profile was not a populated combat consumer"}
}
$portable=Join-Path $evidence ('Portable build '+[char]0x03A9+' with spaces')
New-Item -ItemType Directory -Force -Path $portable | Out-Null
foreach($name in @('curved_world.exe','libc++.dll','libunwind.dll','assets','notices','README.md','THIRD_PARTY_NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root ('dist/'+$name)) -Destination $portable -Recurse -Force}
Seed-User (Join-Path $portable 'user')
$portableResult=Join-Path $portable 'native evidence'
$report=Run-Native 'Standalone Unicode/spaced-path launch' (Join-Path $portable 'curved_world.exe') @('--frames','120','--warmup-ticks','90','--map','1','--no-vsync','--output',('"'+$portableResult+'"')) $working $portableResult
if('wardens' -notin $report.enabled_packs){throw 'Portable executable did not load the external mod'}
Write-Host "Native evidence completed: $evidence"
