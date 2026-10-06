param([Parameter(Mandatory=$true)][string]$Port,[string]$Directory='',[switch]$All)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
if (-not $Directory) { $Directory=Join-Path (Split-Path -Parent $PSScriptRoot) ('.local/backups/state-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$console=$null
function Exchange([string]$Command,[string]$Pattern) {
    [void]$console.ReadExisting();$console.WriteLine($Command)
    $reply='';$timer=[Diagnostics.Stopwatch]::StartNew();$probedAt=0
    while ($timer.Elapsed.TotalSeconds -lt 20) {
        $reply+=$console.ReadExisting()
        if ($reply -match 'STORAGE ERR') { throw 'Device could not export an archive. Existing progress is unchanged.' }
        $match=[regex]::Match($reply,$Pattern)
        if ($match.Success) { $script:lastStorageReply=$reply;return $match }
        # HWCDC can hold a short response after sleep. A read-only status query
        # drains that path without replaying the archive or chunk command.
        if ($timer.ElapsedMilliseconds-$probedAt -ge 500) {
            $console.WriteLine('status');$probedAt=$timer.ElapsedMilliseconds
        }
        Start-Sleep -Milliseconds 10
    }
    throw "Snapshot read timed out at '$Command'; backup has not been accepted."
}
function Verify-Snapshot([byte[]]$Bytes,[uint64]$Generation) {
    if ($Bytes.Length -lt 32 -or $Bytes.Length -gt 65536) { throw 'Invalid snapshot length.' }
    [uint32]$crc=[uint32]::MaxValue
    for ($i=0;$i -lt $Bytes.Length-4;$i++) {
        $crc=$crc -bxor [uint32]$Bytes[$i]
        for ($bit=0;$bit -lt 8;$bit++) { if ($crc -band 1) { $crc=($crc -shr 1) -bxor [uint32]3988292384 } else { $crc=$crc -shr 1 } }
    }
    $crc=$crc -bxor [uint32]::MaxValue
    if ($crc -ne [BitConverter]::ToUInt32($Bytes,$Bytes.Length-4) -or [BitConverter]::ToUInt32($Bytes,0) -ne 0x54534452) { throw 'Backup checksum/header mismatch.' }
    # Schema 1..4 share the generation immediately after magic/version (offset 6).
    $schema=[BitConverter]::ToUInt16($Bytes,4)
    if ($schema -lt 1 -or $schema -gt 4 -or [BitConverter]::ToUInt64($Bytes,6) -ne $Generation) { throw 'Backup version/generation mismatch.' }
}
try {
    $console=[RoutineDeviceConsole]::new($Port)
    $header=Exchange 'storage snapshot' 'STORAGE SNAPSHOT (\d+) (\d+)\r?\n'
    $archives=@(@{Generation=[uint64]$header.Groups[1].Value;Size=[int]$header.Groups[2].Value})
    if ($All) {
        $listed=Exchange 'storage list' 'STORAGE LIST END (\d+)\r?\n'
        $entries=[regex]::Matches($script:lastStorageReply,'STORAGE ARCHIVE (\d+) (\d+)\r?\n')
        if ($entries.Count -ne [int]$listed.Groups[1].Value -or -not $entries.Count) { throw 'Incomplete archive listing.' }
        $archives=@($entries | ForEach-Object { @{Generation=[uint64]$_.Groups[1].Value;Size=[int]$_.Groups[2].Value} } | Sort-Object { $_.Generation })
        if (@($archives | Group-Object { $_.Generation } | Where-Object Count -gt 1).Count) { throw 'Duplicate archive generation.' }
    }
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $manifest=@()
    foreach ($archive in $archives) {
        $generation=$archive.Generation;$size=$archive.Size
        if (-not $generation -or $size -lt 32 -or $size -gt 65536) { throw 'Invalid archive metadata.' }
        $bytes=[byte[]]::new($size)
        for ($offset=0;$offset -lt $size;$offset+=128) {
            $count=[Math]::Min(128,$size-$offset)
            $part=Exchange ("storage read $generation $offset $count") ("STORAGE DATA $offset ([0-9a-f]{"+($count*2)+"})\r?\n")
            $hex=$part.Groups[1].Value
            for ($i=0;$i -lt $count;$i++) { $bytes[$offset+$i]=[Convert]::ToByte($hex.Substring($i*2,2),16) }
        }
        Verify-Snapshot $bytes $generation
        $hasher=[Security.Cryptography.SHA256]::Create()
        try { $hash=-join ($hasher.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) } finally { $hasher.Dispose() }
        $target=Join-Path $Directory ("history-$generation.bin")
        if (Test-Path -LiteralPath $target) {
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) { throw 'Existing PC backup differs; it was not overwritten.' }
        } else {
            $file=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try { $file.Write($bytes,0,$bytes.Length);$file.Flush($true) } finally { $file.Dispose() }
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) { throw 'PC backup read-back mismatch.' }
        }
        $manifest+=@{generation=$generation.ToString();bytes=$size;file=[IO.Path]::GetFileName($target);sha256=$hash}
        Write-Output ("Verified generation $generation ($size bytes): $target SHA256=$hash")
    }
    # A unique receipt cannot overwrite an older backup set's manifest.
    $receipt=Join-Path $Directory ('receipt-'+[Guid]::NewGuid().ToString('N')+'.json')
    @{createdUtc=[DateTime]::UtcNow.ToString('o');archives=$manifest} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $receipt -Encoding utf8
    Write-Output ('Verified '+$archives.Count+' progress archive(s). Wi-Fi/API credentials are excluded; device archives were retained.')
} finally { if ($console) { $console.Dispose() } }
