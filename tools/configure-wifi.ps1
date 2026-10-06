param([Parameter(Mandatory=$true)][string]$Port)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
# Prompt locally: do not put passwords in command history, source, or chat.
$networkName = Read-Host 'Wi-Fi SSID (2.4 GHz)'
$securePassword = Read-Host 'Wi-Fi password (leave empty for an open network)' -AsSecureString
$passwordPointer = [IntPtr]::Zero
$serial = $null
try {
    $passwordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
    $networkPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPointer)
    $ssidBytes = [Text.Encoding]::UTF8.GetBytes($networkName)
    if ($ssidBytes.Length -lt 1 -or $ssidBytes.Length -gt 32 -or $networkName.Contains([char]0)) { throw 'SSID must be 1..32 UTF-8 bytes without NUL.' }
    if ($networkPassword.Length -ne 0 -and ($networkPassword.Length -lt 8 -or $networkPassword.Length -gt 63 -or $networkPassword -match '[^\x20-\x7E]')) {
        throw 'Use an empty password or 8..63 printable ASCII characters.'
    }
    $passwordBytes = [Text.Encoding]::ASCII.GetBytes($networkPassword)
    $ssidHex = -join ($ssidBytes | ForEach-Object { $_.ToString('x2') })
    $passwordHex = -join ($passwordBytes | ForEach-Object { $_.ToString('x2') })
    $serial = [RoutineDeviceConsole]::new($Port)
    [void]$serial.ReadExisting()
    $serial.WriteLine('wifi-set ' + $ssidHex + ' ' + $passwordHex)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 35) {
        $reply = $serial.ReadExisting()
        if ($reply) { Write-Host -NoNewline $reply }
        Start-Sleep -Milliseconds 50
    }
} finally {
    if ($passwordPointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPointer) }
    if ($passwordBytes) { [Array]::Clear($passwordBytes, 0, $passwordBytes.Length) }
    $networkPassword = $passwordHex = $null
    $securePassword.Dispose()
    if ($serial) { $serial.Dispose() }
}
