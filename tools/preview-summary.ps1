param([string]$Cxx='C:/msys64/ucrt64/bin/g++.exe',[string]$Python='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $Python){$Python=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'}
Push-Location -LiteralPath $root
try{
    $sources=@(foreach($area in @('application','content','domain','presentation','generated')){
        $folder='firmware/RoutineDevice/src/'+$area
        Get-ChildItem -LiteralPath $folder -Filter '*.cpp' | ForEach-Object {$folder+'/'+$_.Name}
    })
    & $Cxx -std=c++17 -O2 -static -Wall -Wextra -Werror -o build/summary-tests.exe tests/summary_tests.cpp $sources
    if($LASTEXITCODE -ne 0){throw 'Summary renderer compile failed'}
    New-Item -ItemType Directory -Force build/summary-preview | Out-Null
    & ./build/summary-tests.exe build/summary-preview
    if($LASTEXITCODE -ne 0){throw 'Summary tests/render failed'}
    & $Python tools/summary_sheets.py
    if($LASTEXITCODE -ne 0){throw 'Summary comparison generation failed'}
}finally{Pop-Location}
