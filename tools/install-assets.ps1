param([string]$Port='COM5',[string]$Pack=(Join-Path $PSScriptRoot '../assets/packed/ui.pak'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Pack))
if($bytes.Length -lt 16 -or [Text.Encoding]::ASCII.GetString($bytes,0,8) -ne 'RDAS0001'){throw 'Invalid asset pack'}
$crc=[BitConverter]::ToUInt32($bytes,12).ToString('x8')
if(-not ('RoutineAssetCrc' -as [type])){Add-Type @'
public static class RoutineAssetCrc {
 public static uint Compute(byte[] bytes) { uint crc=0xffffffff;foreach(byte b in bytes){crc^=b;for(int i=0;i<8;i++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}return ~crc; }
}
'@}
$console=[RoutineDeviceConsole]::new($Port)
function Wait-Reply([string]$pattern,[int]$seconds=25){
 $timer=[Diagnostics.Stopwatch]::StartNew();$reply='';$queriedAt=0
 while($timer.Elapsed.TotalSeconds -lt $seconds){
  $reply+=$console.ReadExisting()
  if($reply -match 'ASSETS ERR'){throw $reply}
  if($reply -match $pattern){return $reply.Trim()}
  # Some HWCDC replies stay queued until another control request. Query the
  # accepted receive offset; never replay a chunk or a product input.
  if($timer.ElapsedMilliseconds-$queriedAt -ge 500){$console.WriteLine('assets info');$queriedAt=$timer.ElapsedMilliseconds}
  Start-Sleep -Milliseconds 10
 }
 throw "Timed out waiting for $pattern. Let the device's 30-second receive timeout finish before querying assets info; do not replay an input. Received: $reply"
}
try {
 $null=$console.ReadExisting()
 $console.WriteLine('assets info');$info=Wait-Reply 'ASSETS ready=\d bytes=\d+ crc=[a-f0-9]+ received=\d+ active=\d'
 Write-Output $info
 if($info -notmatch "bytes=$($bytes.Length) crc=$crc "){throw 'Firmware and asset pack versions differ'}
 if($info -match 'ready=1'){Write-Output 'Matching asset pack already loaded';return}
 $console.WriteLine("assets begin $($bytes.Length) $crc");$null=Wait-Reply 'ASSETS READY'
 for($offset=0;$offset -lt $bytes.Length;){
  $size=[Math]::Min(4096,$bytes.Length-$offset);$chunk=[byte[]]::new($size);[Array]::Copy($bytes,$offset,$chunk,0,$size)
  $chunkCrc=[RoutineAssetCrc]::Compute($chunk).ToString('x8')
  # Header and body fit together in the firmware's 8KiB receive queue. Sending
  # the body immediately also avoids waiting for a READY reply in binary mode.
  $console.WriteLine("assets chunk $offset $size $chunkCrc")
  $console.WriteBytes($chunk)
  $offset+=$size;$null=Wait-Reply "ASSETS OK $offset(?:\r|\n|$)|ASSETS ready=0 bytes=$($bytes.Length) crc=$crc received=$offset active=1"
  if(($offset % 262144) -eq 0){Write-Output "Assets uploaded: $offset/$($bytes.Length)"}
 }
 $console.WriteLine('assets commit');Write-Output (Wait-Reply "ASSETS COMMIT OK|ASSETS ready=1 bytes=$($bytes.Length) crc=$crc received=$($bytes.Length) active=0" 45)
 $console.WriteLine('assets info');Write-Output (Wait-Reply 'ASSETS ready=\d bytes=\d+ crc=[a-f0-9]+ received=\d+ active=\d')
} finally {$console.Dispose()}
