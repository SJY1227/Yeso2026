param([string]$Cxx = 'C:/msys64/ucrt64/bin/g++.exe', [string]$Python = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Python) {
    $Python = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
}
Push-Location -LiteralPath $root
try {
    New-Item -ItemType Directory -Force -Path build, design/previews | Out-Null
    & $Cxx -std=c++17 -O2 -static -Wall -Wextra -Werror -o build/render-preview.exe tools/render_preview.cpp firmware/RoutineDevice/src/diagnostics/HardwareCheck.cpp firmware/RoutineDevice/src/presentation/HomeRenderer.cpp firmware/RoutineDevice/src/presentation/ImageRenderer.cpp firmware/RoutineDevice/src/generated/Assets.cpp firmware/RoutineDevice/src/generated/ThemeAssets.cpp firmware/RoutineDevice/src/presentation/ImageArchive.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Preview compile failed' }
    & (Join-Path $root 'build/render-preview.exe') design/previews
    if ($LASTEXITCODE -ne 0) { throw 'Preview render failed' }
    & $Python -c 'from pathlib import Path; from PIL import Image; [Image.open(p).save(p.with_suffix(".png")) for p in Path("design/previews").glob("*-home.ppm")]'
    if ($LASTEXITCODE -ne 0) { throw 'Preview PNG conversion failed' }
} finally { Pop-Location }
