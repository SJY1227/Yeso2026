param([Parameter(Mandatory=$true)][string]$Port)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
# Do not alter modem control lines on reopen; that would reset the board.
function Open-DeviceConsole {
    return [RoutineDeviceConsole]::new($Port)
}
function Read-DeviceStatus($console) {
    [void]$console.ReadExisting()
    $data = ''
    $console.WriteLine('status')
    $limit = [Diagnostics.Stopwatch]::StartNew()
    $probedAt = 0
    while ($limit.Elapsed.TotalSeconds -lt 5) {
        $data += $console.ReadExisting()
        if ($data -match 'requests=(\d+)' -and $data -match 'last-error=(\d+) wake-cause=(\d+)') { return $data }
        # Read-only probes recover delayed HWCDC output; never repeat sleep.
        if ($limit.ElapsedMilliseconds - $probedAt -ge 500) {
            $console.WriteLine('status'); $probedAt = $limit.ElapsedMilliseconds
        }
        Start-Sleep -Milliseconds 50
    }
    throw 'No status after reconnect. Check the USB connection and diagnostic firmware.'
}
$console = $null
try {
    $console = Open-DeviceConsole
    Start-Sleep -Milliseconds 1500
    $initial = Read-DeviceStatus $console
    Write-Output $initial
    [void]($initial -match 'requests=(\d+)')
    $requestCount = [uint32]$Matches[1]
    foreach ($iteration in 1..3) {
        $console.WriteLine('sleep 2')
        Start-Sleep -Milliseconds 500
        $console.Close(); $console.Dispose(); $console = $null
        Start-Sleep -Seconds 3
        $retryTimer = [Diagnostics.Stopwatch]::StartNew()
        while (-not $console -and $retryTimer.Elapsed.TotalSeconds -lt 6) {
            try { $console = Open-DeviceConsole } catch { Start-Sleep -Milliseconds 250 }
        }
        if (-not $console) { throw 'USB did not re-enumerate after light sleep.' }
        $after = Read-DeviceStatus $console
        Write-Output $after
        [void]($after -match 'requests=(\d+)')
        $nextCount = [uint32]$Matches[1]
        if ($nextCount -ne $requestCount + 1) { throw 'Wake did not request exactly one new time sync, or the board reset.' }
        if ($after -notmatch 'last-error=0 wake-cause=4;') { throw 'Expected a successful timer wake (cause 4).' }
        $requestCount = $nextCount
        Write-Output "PASS: timer wake $iteration, retained RAM, new time-sync request, USB reopened without board reset"
    }
} finally {
    if ($console) { $console.Dispose() }
}
