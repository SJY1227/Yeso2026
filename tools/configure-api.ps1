param(
    [Parameter(Mandatory=$true)][string]$Port,
    [Parameter(Mandatory=$true)][string]$BaseUrl,
    [Parameter(Mandatory=$true)][string]$RootCaPath,
    [ValidateRange(0,86400)][uint32]$WarningLeadSeconds=0,
    [ValidateSet('deadline-wins','allow-at-deadline')][string]$DeadlineRule='deadline-wins',
    [switch]$Pair
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
$uri=[Uri]$BaseUrl
if ($uri.Scheme -ne 'https' -or $uri.UserInfo -or $uri.Query -or $uri.Fragment -or $uri.AbsolutePath -ne '/') {
    throw 'BaseUrl must be an HTTPS origin without a path, credentials, query, or fragment.'
}
$ca=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $RootCaPath))
$configuration=@{baseUrl=$BaseUrl.TrimEnd('/');rootCaPem=$ca;warningLeadSeconds=$WarningLeadSeconds;deadlineRule=$DeadlineRule}
$bytes=[Text.Encoding]::UTF8.GetBytes(($configuration | ConvertTo-Json -Compress))
if ($bytes.Length -gt 8192) { throw 'API configuration exceeds the device limit (8192 bytes).' }
$console=$null
function Exchange([string]$Command,[string]$Pattern) {
    [void]$console.ReadExisting()
    $console.WriteLine($Command)
    $reply='';$timer=[Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 12) {
        $reply+=$console.ReadExisting()
        if ($reply -match 'API ERR') { throw 'Device rejected configuration; inspect api status. No credentials are printed.' }
        if ($reply -match $Pattern) { return }
        Start-Sleep -Milliseconds 20
    }
    throw 'No acknowledgement. Query api status before retrying; do not assume the last operation failed.'
}
try {
    $console=[RoutineDeviceConsole]::new($Port)
    Exchange ('api config begin '+$bytes.Length) 'API CONFIG 0\r?\n'
    for ($offset=0;$offset -lt $bytes.Length;$offset+=64) {
        $last=[Math]::Min($offset+63,$bytes.Length-1)
        $hex=-join ($bytes[$offset..$last] | ForEach-Object { $_.ToString('x2') })
        Exchange ("api config data $offset $hex") ('API CONFIG '+($last+1)+'\r?\n')
    }
    Exchange 'api config commit' 'API CONFIG OK\r?\n'
    Write-Output 'Verified HTTPS configuration saved.'
    if ($Pair) {
        # Enter the single-use code locally; keep it out of shell history and logs.
        $secret=Read-Host '10-digit app pairing code (including leading zeros)' -AsSecureString
        $pointer=[IntPtr]::Zero
        try {
            $pointer=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($secret)
            $code=[Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
            if ($code -notmatch '^\d{10}$' -or $code -match '[^0-9]') { throw 'Expected exactly ten ASCII digits.' }
            Exchange ('api pair '+$code) 'API pairing requested\r?\n'
            Write-Output 'Pairing requested. This acknowledgement does not mean server registration succeeded.'
        } finally {
            if ($pointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer) }
            $code=$null
            $secret.Dispose()
        }
    }
    $console.WriteLine('api status')
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 2) { $reply=$console.ReadExisting();if ($reply) { Write-Host -NoNewline $reply } }
} finally { if ($console) { $console.Dispose() } }
