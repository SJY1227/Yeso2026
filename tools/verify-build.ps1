param([string]$Cache='',[string]$Output='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if (-not $Cache) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $id=-join ($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root)) | ForEach-Object { $_.ToString('x2') }) } finally { $sha.Dispose() }
    $Cache=Join-Path $env:LOCALAPPDATA ('RoutineDeviceBuild/'+$id.Substring(0,12))
}
if (-not $Output) { $Output=Join-Path $root 'build/build-verification.json' }
$source=Join-Path $root 'firmware/RoutineDevice'
$checked=@()
Get-ChildItem -LiteralPath $source -Recurse -File | Where-Object { $_.Extension -in '.h','.cpp','.ino' } | ForEach-Object {
    $relative=$_.FullName.Substring($source.Length+1)
    $compiled=Join-Path (Join-Path $Cache 'sketch') $relative
    if ($_.Extension -eq '.ino') { $compiled+='.cpp' }
    if (-not (Test-Path -LiteralPath $compiled)) { throw "Source missing from build cache: $relative" }
    $original=[IO.File]::ReadAllText($_.FullName).Replace("`r`n","`n")
    $copied=[IO.File]::ReadAllText($compiled).Replace("`r`n","`n")
    $copied=[regex]::Replace($copied,'(?m)^#line[^\n]*\n','')
    if ($_.Extension -eq '.ino') {
        # Arduino adds its header and setup/loop prototypes to this entry point.
        # Refuse any other unexplained difference, including new generated prototypes.
        $copied=[regex]::Replace($copied,'\A#include <Arduino\.h>\n','')
        $copied=[regex]::Replace($copied,'(?m)^void (setup|loop)\(\);\n','')
        $original=$original.TrimEnd("`n");$copied=$copied.TrimEnd("`n")
    }
    if ($original -cne $copied) { throw "Source differs from build cache; rebuild first: $relative" }
    $checked+=@{file=$relative;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$packPath=Join-Path $root 'assets/packed/ui.pak';$bytes=[IO.File]::ReadAllBytes($packPath)
$descriptor=Get-Content -LiteralPath (Join-Path $source 'src/generated/AssetPack.h') -Raw
$payload=[uint32]([regex]::Match($descriptor,'kAssetPayloadBytes = (\d+)').Groups[1].Value)
$crc=[Convert]::ToUInt32([regex]::Match($descriptor,'kAssetCrc = 0x([a-f0-9]+)').Groups[1].Value,16)
if ($bytes.Length -ne $payload+16 -or [Text.Encoding]::ASCII.GetString($bytes,0,8) -ne 'RDAS0001' -or [BitConverter]::ToUInt32($bytes,8) -ne $payload -or [BitConverter]::ToUInt32($bytes,12) -ne $crc) { throw 'Asset header and compiled descriptor differ' }
$app=Join-Path $root 'build/firmware/RoutineDevice.ino.bin'
$cachedApp=Join-Path $Cache 'RoutineDevice.ino.bin'
if ((Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $cachedApp -Algorithm SHA256).Hash) { throw 'Exported firmware differs from build cache' }
$manifest=@{
    generatedUtc=[DateTime]::UtcNow.ToString('o')
    firmware=@{bytes=(Get-Item -LiteralPath $app).Length;sha256=(Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash.ToLowerInvariant()}
    assets=@{bytes=$bytes.Length;crc=$crc.ToString('x8');sha256=(Get-FileHash -LiteralPath $packPath -Algorithm SHA256).Hash.ToLowerInvariant()}
    sources=$checked
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Output -Encoding utf8
Write-Output ("PASS: $($checked.Count) source files match build cache; asset header/descriptor match; release hashes saved to $Output")
