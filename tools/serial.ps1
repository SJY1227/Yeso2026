param(
    [Parameter(Mandatory=$true)][string]$Port,
    [string[]]$Commands = @('help'),
    [ValidateRange(1,30)][int]$ListenSeconds = 3
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
$serial = $null
$response = [System.Text.StringBuilder]::new()
try {
    $serial = [RoutineDeviceConsole]::new($Port)
    foreach ($line in $Commands) { $serial.WriteLine($line) }
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $ListenSeconds) {
        [void]$response.Append($serial.ReadExisting())
        Start-Sleep -Milliseconds 20
    }
    Write-Output $response.ToString()
} finally {
    if ($serial) { $serial.Dispose() }
}
