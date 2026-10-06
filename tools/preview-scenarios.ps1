param([string]$Cxx='C:/msys64/ucrt64/bin/g++.exe',[string]$Python='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if (-not $Python) { $Python=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' }
Push-Location -LiteralPath $root
try {
    $sources=@(foreach ($area in @('application','content','domain','presentation','generated')) {
        $folder='firmware/RoutineDevice/src/'+$area
        Get-ChildItem -LiteralPath $folder -Filter '*.cpp' | ForEach-Object { $folder+'/'+$_.Name }
    })
    New-Item -ItemType Directory -Force -Path build/scenarios | Out-Null
    & $Cxx -std=c++17 -O2 -static -Wall -Wextra -Werror -o build/render-scenarios.exe tools/render_scenarios.cpp $sources
    if ($LASTEXITCODE -ne 0) { throw 'Scenario renderer compile failed' }
    & ./build/render-scenarios.exe build/scenarios
    if ($LASTEXITCODE -ne 0) { throw 'Scenario render failed' }
    & $Python tools/scenario_sheets.py
    if ($LASTEXITCODE -ne 0) { throw 'Scenario sheet generation failed' }
} finally { Pop-Location }
