# Run in a separate PowerShell process. This TEST-only console cannot open COM.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Add-Type @'
using System;
using System.Collections.Generic;
using System.Text;
public sealed class RoutineDeviceConsole : IDisposable {
 public static SortedDictionary<ulong,byte[]> Archives=new SortedDictionary<ulong,byte[]>();
 public static bool CorruptRead=false, IncompleteList=false, DelayUntilStatus=false;
 public static int SnapshotRequests=0, StatusRequests=0;
 bool awaitingStatus=false;
 string reply="";
 public RoutineDeviceConsole(string port){if(port!="TEST")throw new Exception("Offline test console only");}
 public string ReadExisting(){if(awaitingStatus)return "";var result=reply;reply="";return result;}
 public void Dispose(){}
 public void WriteLine(string command){
  if(command=="status"){StatusRequests++;awaitingStatus=false;return;}
  awaitingStatus=DelayUntilStatus;
  var parts=command.Split(' ');
  if(command=="storage snapshot"){SnapshotRequests++;foreach(var pair in Archives)reply="STORAGE SNAPSHOT "+pair.Key+" "+pair.Value.Length+"\r\n";return;}
  if(command=="storage list"){
   var s=new StringBuilder();foreach(var pair in Archives)s.Append("STORAGE ARCHIVE "+pair.Key+" "+pair.Value.Length+"\r\n");
   s.Append("STORAGE LIST END "+(Archives.Count+(IncompleteList?1:0))+"\r\n");reply=s.ToString();return;
  }
  if(parts.Length==5&&parts[0]=="storage"&&parts[1]=="read"){
   var bytes=Archives[ulong.Parse(parts[2])];int offset=int.Parse(parts[3]),count=int.Parse(parts[4]);var s=new StringBuilder();
   for(int i=0;i<count;i++)s.Append((CorruptRead&&offset+i==0?(byte)(bytes[offset+i]^1):bytes[offset+i]).ToString("x2"));
   reply="STORAGE DATA "+offset+" "+s+"\r\n";return;
  }
  throw new Exception("Unexpected test command");
 }
 public static byte[] Rebase(byte[] source,ulong generation){
  var result=(byte[])source.Clone();Array.Copy(BitConverter.GetBytes(generation),0,result,6,8);
  uint crc=0xffffffff;for(int i=0;i<result.Length-4;i++){crc^=result[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}
  Array.Copy(BitConverter.GetBytes(~crc),0,result,result.Length-4,4);return result;
 }
}
'@
$directory=Join-Path $root ('build/backup-tool-tests/'+[Guid]::NewGuid().ToString('N'))
$tool=Join-Path $root 'tools/backup-state.ps1'
$one=[RoutineDeviceConsole]::Rebase([IO.File]::ReadAllBytes((Join-Path $root 'tests/fixtures/state-v1-reward.bin')),7)
$two=[RoutineDeviceConsole]::Rebase([IO.File]::ReadAllBytes((Join-Path $root 'tests/fixtures/state-v2-collection.bin')),9)
[RoutineDeviceConsole]::Archives.Add(7,$one);[RoutineDeviceConsole]::Archives.Add(9,$two)
& $tool -Port TEST -All -Directory $directory | Out-Null
if (@(Get-ChildItem -LiteralPath $directory -Filter 'history-*.bin').Count -ne 2) { throw 'Expected two archives' }
$first=Join-Path $directory 'history-7.bin'
$mtime=(Get-Item -LiteralPath $first).LastWriteTimeUtc
& $tool -Port TEST -All -Directory $directory | Out-Null
if ((Get-Item -LiteralPath $first).LastWriteTimeUtc -ne $mtime) { throw 'Matching backup was rewritten' }
$receipts=@(Get-ChildItem -LiteralPath $directory -Filter 'receipt-*.json')
if ($receipts.Count -ne 2) { throw 'Expected separate verified receipts' }
if (@((Get-Content -LiteralPath $receipts[0].FullName -Raw | ConvertFrom-Json).archives).Count -ne 2) { throw 'Receipt lacks archives' }
[IO.File]::WriteAllText($first,'different local evidence')
$rejected=$false
try { & $tool -Port TEST -All -Directory $directory | Out-Null } catch { $rejected=$_.Exception.Message -match 'Existing PC backup differs' }
if (-not $rejected -or [IO.File]::ReadAllText($first) -ne 'different local evidence') { throw 'Conflicting PC backup was not preserved' }
[RoutineDeviceConsole]::CorruptRead=$true;$rejected=$false
try { & $tool -Port TEST -All -Directory ($directory+'-corrupt') | Out-Null } catch { $rejected=$_.Exception.Message -match 'checksum/header mismatch' }
if (-not $rejected) { throw 'Corrupt device bytes accepted' }
[RoutineDeviceConsole]::CorruptRead=$false;[RoutineDeviceConsole]::IncompleteList=$true;$rejected=$false
try { & $tool -Port TEST -All -Directory ($directory+'-incomplete') | Out-Null } catch { $rejected=$_.Exception.Message -match 'Incomplete archive listing' }
if (-not $rejected) { throw 'Incomplete listing accepted' }
[RoutineDeviceConsole]::IncompleteList=$false;[RoutineDeviceConsole]::DelayUntilStatus=$true
$snapshots=[RoutineDeviceConsole]::SnapshotRequests
& $tool -Port TEST -All -Directory ($directory+'-delayed') | Out-Null
if ([RoutineDeviceConsole]::SnapshotRequests -ne $snapshots+1 -or [RoutineDeviceConsole]::StatusRequests -lt 2) { throw 'Delayed response recovery replayed snapshot or did not probe' }
Write-Output 'PASS: offline multi-archive export, legacy schemas, matching-file reuse, conflicting-file preservation, corrupt bytes, incomplete listing rejection and delayed USB response recovery without snapshot replay'
