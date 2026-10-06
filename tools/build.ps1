param([string]$ArduinoCli = '', [string]$Port = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $ArduinoCli) {
    $candidate = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($candidate) { $ArduinoCli = $candidate.Source }
    else { $ArduinoCli = Join-Path $env:LOCALAPPDATA 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe' }
}
if (-not (Test-Path -LiteralPath $ArduinoCli)) { throw 'Arduino CLI not found. Supply -ArduinoCli with its path.' }
$libraries = Join-Path $root '.tools/libraries'
if (-not (Test-Path -LiteralPath (Join-Path $libraries 'Adafruit_GFX_Library-1.12.6/library.properties'))) {
    throw 'Run tools/fetch_sources.py once to fetch the pinned libraries.'
}
$fqbn = 'esp32:esp32:XIAO_ESP32S3_Plus:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=default'
$output = Join-Path $root 'build/firmware'
$sketch = Join-Path $root 'firmware/RoutineDevice'
# GNU ld in the installed ESP32 toolchain cannot write a Unicode output path.
# Keep sources in the workspace and use an ASCII-only intermediate build path.
$sha = [System.Security.Cryptography.SHA256]::Create()
try { $workspaceId = -join ($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($root)) | ForEach-Object { $_.ToString('x2') }) }
finally { $sha.Dispose() }
$buildCache = Join-Path $env:LOCALAPPDATA ('RoutineDeviceBuild/' + $workspaceId.Substring(0,12))
if ($buildCache -match '[^\x00-\x7F]') { throw 'The installed ESP32 linker requires an ASCII-only LOCALAPPDATA build path.' }
New-Item -ItemType Directory -Force -Path $output | Out-Null
Write-Output "Intermediate build path: $buildCache"
& $ArduinoCli compile --fqbn $fqbn --libraries $libraries --build-property 'compiler.cpp.extra_flags=-Wframe-larger-than=4096 -Wstack-usage=6144 -fstack-usage' --build-path $buildCache --output-dir $output $sketch
if ($LASTEXITCODE -ne 0) { throw "Firmware compile failed ($LASTEXITCODE)" }
if ($Port) {
    & $ArduinoCli upload --fqbn $fqbn --port $Port --input-dir $output $sketch
    if ($LASTEXITCODE -ne 0) { throw "Firmware upload failed ($LASTEXITCODE)" }
} else { Write-Output "Compiled: $output (upload only when -Port is supplied)" }
