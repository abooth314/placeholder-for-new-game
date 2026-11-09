param([string]$EvidenceDirectory='out/validation/delivery')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$directory=if([IO.Path]::IsPathRooted($EvidenceDirectory)){$EvidenceDirectory}else{Join-Path $root $EvidenceDirectory}
Add-Type -AssemblyName System.Drawing
$checks=@()
foreach($file in Get-ChildItem -LiteralPath $directory -Recurse -File -Filter '*.bmp'){
    $bitmap=[Drawing.Bitmap]::FromFile($file.FullName)
    try{
        $colors=New-Object 'System.Collections.Generic.HashSet[int]'
        for($y=20;$y -lt $bitmap.Height-20;$y+=12){for($x=20;$x -lt $bitmap.Width-20;$x+=12){$null=$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
        if($colors.Count -lt 64 -or $bitmap.Width -lt 960 -or $bitmap.Height -lt 640){throw "Blank/undersized rendered capture: $($file.FullName)"}
        $png=[IO.Path]::ChangeExtension($file.FullName,'.png')
        $bitmap.Save($png,[Drawing.Imaging.ImageFormat]::Png)
        $checks += [ordered]@{source=$file.FullName;png=$png;width=$bitmap.Width;height=$bitmap.Height;sampled_colors=$colors.Count;png_sha256=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash}
    }finally{$bitmap.Dispose()}
}
if($checks.Count -lt 12){throw 'Rendered/portable/profile captures are missing'}
$checks | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $directory 'capture-checks.json') -Encoding utf8
Write-Host "Verified and losslessly converted $($checks.Count) native captures"
