param([switch]$CoreOnly, [string]$ResultsDirectory = 'out/validation/tests')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$env:PATH = "$(Join-Path $root '.tools/llvm-mingw-20260616-ucrt-x86_64/bin');$env:PATH"
$ctest = Join-Path $root '.tools/cmake-4.1.0-windows-x86_64/bin/ctest.exe'
$build = Join-Path $root $(if ($CoreOnly) { 'out/core' } else { 'out/build' })
if (![IO.Path]::IsPathRooted($ResultsDirectory)) { $ResultsDirectory=Join-Path $root $ResultsDirectory }
New-Item -ItemType Directory -Force -Path $ResultsDirectory | Out-Null
$expected=@('geometry','terrain','values','content','lua','identity','spatial','combat','diplomacy','tactics','possession','replay','bridge_world','budgets')
if (!$CoreOnly) { $expected+=@('camera','recipes','input_settings','console') }
$inventoryText=& $ctest --test-dir $build --show-only=json-v1
if ($LASTEXITCODE -ne 0) { throw 'Test inventory failed' }
$inventory=($inventoryText -join [Environment]::NewLine) | ConvertFrom-Json
foreach($name in $expected) { if($name -notin $inventory.tests.name){throw "Required test is missing: $name"} }
$junit=Join-Path $ResultsDirectory 'junit.xml'
& $ctest --test-dir $build --output-on-failure --no-tests=error --timeout 90 --output-junit $junit
$testExit=$LASTEXITCODE
if (!(Test-Path -LiteralPath $junit)) { throw 'No test receipt was generated' }
[xml]$receipt=Get-Content -LiteralPath $junit -Raw
$cases=@($receipt.testsuite.testcase)
foreach($name in $expected) { if(@($cases | Where-Object {$_.name -eq $name}).Count -ne 1){throw "Missing/duplicate test receipt: $name"} }
if($testExit -ne 0 -or [int]$receipt.testsuite.failures -ne 0 -or [int]$receipt.testsuite.disabled -ne 0 -or $receipt.SelectNodes('//skipped').Count -ne 0){throw 'Native tests failed or skipped a required case'}
Write-Host "Verified $($cases.Count) completed test receipts: $junit"
