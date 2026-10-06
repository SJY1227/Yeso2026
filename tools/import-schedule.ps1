param([Parameter(Mandatory=$true)][string]$Port,[Parameter(Mandatory=$true)][string]$Path,[string]$Python='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
if (-not $Python) { $Python=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' }
$binary=Join-Path ([IO.Path]::GetTempPath()) ('routine-schedule-'+[guid]::NewGuid().ToString('N')+'.bin')
$console=$null
function Exchange([string]$Command,[string]$Pattern) {
    [void]$console.ReadExisting()
    $console.WriteLine($Command)
    $reply='';$timer=[Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 10) {
        $reply+=$console.ReadExisting()
        if ($reply -match 'LOAD ERR') { throw $reply }
        if ($reply -match $Pattern) { return $reply }
        Start-Sleep -Milliseconds 20
    }
    throw "No acknowledgement for schedule transfer. State was not assumed committed. $reply"
}
try {
    & $Python (Join-Path $PSScriptRoot 'compile_schedule.py') $Path $binary
    if ($LASTEXITCODE -ne 0) { throw 'Invalid schedule JSON' }
    $bytes=[IO.File]::ReadAllBytes($binary)
    $console=[RoutineDeviceConsole]::new($Port)
    [void](Exchange ('load begin '+$bytes.Length) 'LOAD OK 0\r?\n')
    for ($offset=0;$offset -lt $bytes.Length;$offset+=64) {
        $last=[Math]::Min($offset+63,$bytes.Length-1)
        $hex=-join ($bytes[$offset..$last] | ForEach-Object { $_.ToString('x2') })
        [void](Exchange ("load data $offset $hex") ("LOAD OK "+($last+1)+"\r?\n"))
    }
    $result=Exchange 'load commit' 'LOAD RESULT (\d+)'
    if ($result -match 'LOAD RESULT ([01])\r?\n') { Write-Output 'Schedule saved (or identical schedule already present).' }
    else { throw "Schedule rejected; existing records preserved. $result" }
} finally {
    if ($console) { $console.Dispose() }
    if (Test-Path -LiteralPath $binary) { Remove-Item -LiteralPath $binary }
}
