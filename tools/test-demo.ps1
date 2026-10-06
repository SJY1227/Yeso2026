param([Parameter(Mandatory=$true)][string]$Port,[switch]$LeaveRunning,[switch]$LeaveSummary,[string]$Log='')
$ErrorActionPreference='Stop'
if($LeaveRunning -and $LeaveSummary){throw 'Choose either -LeaveRunning or -LeaveSummary'}
. (Join-Path $PSScriptRoot 'DeviceConsole.ps1')
if(-not $Log){$Log=Join-Path (Split-Path -Parent $PSScriptRoot) '.local/demo-device-test.log'}
$logParent=Split-Path -Parent $Log
if($logParent){New-Item -ItemType Directory -Path $logParent -Force | Out-Null}
$console=$null;$trace=[Text.StringBuilder]::new();$active=$false;$passed=$false
function Query([string]$Command,[string]$Pattern){
    [void]$console.ReadExisting();$console.WriteLine($Command)
    $reply='';$timer=[Diagnostics.Stopwatch]::StartNew();$probedAt=0
    while($timer.Elapsed.TotalSeconds -lt 12){
        $reply+=$console.ReadExisting()
        if($reply -match $Pattern){[void]$trace.AppendLine("COMMAND $Command`n$reply");return $reply}
        if($timer.ElapsedMilliseconds-$probedAt -ge 500){$console.WriteLine('status');$probedAt=$timer.ElapsedMilliseconds}
        Start-Sleep -Milliseconds 15
    }
    [void]$trace.AppendLine("TIMEOUT $Command`n$reply");throw "Missing response for $Command"
}
function Frame([string]$Command,[int]$Screen,[int]$Completed,[int]$Growth,[int]$Phase){
    $reply=Query $Command 'DEMO elapsed=\d+ phase=\d+ completed=\d+ earned=\d+ consumed=\d+ warning=\d summary=\d+/3\r?\n'
    if($reply -notmatch 'Session: demo=1 clock=simulated persistence=ram' -or
       $reply -notmatch "screen=$Screen selection=\d fault=0" -or
       $reply -notmatch "growth=$Growth/40" -or
       $reply -notmatch "phase=$Phase completed=$Completed ") {throw "Unexpected $Command : $reply"}
    return $reply
}
function Start-Demo {
    $script:active=$true
    $null=Query 'demo start' 'DEMO STARTED'
    $null=Query 'motion off' 'OK motion off'
    $null=Frame 'demo advance 3' 1 0 0 1
}
function Complete-Step([int]$Step,[switch]$Reload){
    $null=Frame 'input next' 2 ($Step-1) ($Step-1) 2
    $null=Frame 'input confirm' 3 ($Step-1) ($Step-1) 2
    $phase=if($Step -eq 3){3}else{2}
    $null=Frame 'input confirm' 5 $Step ($Step-1) $phase
    if($Reload){$null=Frame 'demo reload' 5 $Step ($Step-1) $phase}
    $null=Frame 'input confirm' 12 $Step ($Step-1) $phase
    $null=Frame 'input confirm' 13 $Step $Step $phase
    if($Reload){$null=Frame 'demo reload' 2 $Step $Step $phase}
    else{$screen=if($Step -eq 3){6}else{2};$null=Frame 'input confirm' $screen $Step $Step $phase}
}
try{
    $console=[RoutineDeviceConsole]::new($Port)
    $before=Query 'status' 'Companion: [^\r\n]+; item=\d+\r?\n'
    if($before -notmatch 'Session: demo=0 '){throw 'Stop the existing demo before running this test'}
    $baseline=[regex]::Match($before,'Product: generation=\d+ revision=\d+ runs=\d+ events=\d+').Value
    $growth=[regex]::Match($before,'Companion: selected=\d+ stage=\d+ growth=\d+/\d+ notice=\d+ offered=\d+').Value
    if(-not $baseline -or -not $growth){throw 'Missing live baseline'}
    Start-Demo
    foreach($blocked in @('storage snapshot','api sync','load begin 10','assets begin 10 aa','sleep 2','sync')){
        $null=Query $blocked 'DEMO ERR command unavailable'
    }
    $null=Frame 'input confirm' 2 0 0 2
    $null=Frame 'input previous' 2 0 0 2
    $null=Frame 'input confirm' 4 0 0 2
    $null=Frame 'input confirm' 2 0 0 2
    $null=Frame 'input confirm' 3 0 0 2
    $null=Frame 'input previous' 2 0 0 2
    Complete-Step 1 -Reload
    Complete-Step 2
    Complete-Step 3
    $null=Frame 'input confirm' 18 3 3 3
    $summary=Frame 'input confirm' 19 3 3 3
    if($summary -notmatch 'summary=3/3'){throw 'Incorrect daily summary'}
    $null=Frame 'input next' 19 3 3 3
    $null=Frame 'input previous' 19 3 3 3
    $null=Frame 'input confirm' 20 3 3 3
    $null=Frame 'input previous' 19 3 3 3
    $null=Frame 'input confirm' 20 3 3 3
    $null=Frame 'input confirm' 0 3 3 3
    Write-Output 'PASS: 3 routine steps, both cancel paths, food/growth, RAM reload without duplicate consumption, result and daily summary'
    Start-Demo
    $null=Frame 'input confirm' 2 0 0 2
    Complete-Step 1
    $null=Frame 'input previous' 2 1 1 2
    $null=Frame 'input confirm' 4 1 1 2
    $null=Frame 'input next' 4 1 1 2
    $abandoned=Frame 'input confirm' 6 1 1 4
    if($abandoned -notmatch 'earned=1 consumed=1'){throw 'Abandon lost earned food'}
    Write-Output 'PASS: abandonment retains completed step, consumed food and growth'
    Start-Demo
    $null=Frame 'input confirm' 2 0 0 2
    $null=Frame 'input confirm' 3 0 0 2
    $status=Query 'status' 'DEMO elapsed=\d+ [^\r\n]+\r?\n'
    $elapsed=[int][regex]::Match($status,'DEMO elapsed=(\d+)').Groups[1].Value
    $warning=Frame ("demo advance "+(7140-$elapsed)) 2 0 0 2
    if($warning -notmatch 'warning=1'){throw 'Warning missing'}
    $null=Frame 'input confirm' 3 0 0 2
    $expired=Frame 'demo advance 60' 6 0 0 5
    if($expired -notmatch 'earned=0 consumed=0'){throw 'Deadline granted unconfirmed food'}
    Write-Output 'PASS: warning invalidates old confirmation; deadline expires without awarding food'
    $null=Query 'demo stop' 'DEMO STOPPED'
    $active=$false
    $after=Query 'status' 'Companion: [^\r\n]+; item=\d+\r?\n'
    if($after -notmatch 'Session: demo=0 ' -or -not $after.Contains($baseline) -or -not $after.Contains($growth)){throw 'Live state changed across demo'}
    Write-Output 'PASS: live state restored unchanged; network/storage/config commands remained blocked in demo'
    if($LeaveSummary){
        Start-Demo
        $null=Frame 'input confirm' 2 0 0 2
        Complete-Step 1
        Complete-Step 2
        Complete-Step 3
        $null=Frame 'input confirm' 18 3 3 3
        $null=Query 'motion on' 'OK motion on'
        Write-Output 'Ready: daily summary intro in RAM-only demo. Hold D3 to open its letter; next/previous browse, confirm exits to the closing greeting.'
    }elseif($LeaveRunning){
        $active=$true;$null=Query 'demo start' 'DEMO STARTED'
        $null=Query 'motion on' 'OK motion on'
        $null=Frame 'demo advance 3' 1 0 0 1
        Write-Output 'Ready: fresh 3-step demo letter, motion on. Hold D3 for 0.8 seconds to open; reboot restores live records.'
    }
    $passed=$true
}finally{
    if($console){
        if($active -and (-not $passed -or (-not $LeaveRunning -and -not $LeaveSummary))){
            try{$null=Query 'demo stop' 'DEMO STOPPED'}catch{Write-Warning 'Demo cleanup could not be confirmed; reboot exits RAM-only mode.'}
        }
        $console.Dispose()
    }
    $trace.ToString()|Set-Content -LiteralPath $Log -Encoding utf8
}
