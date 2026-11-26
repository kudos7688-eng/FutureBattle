# Measurement lock queue (2026-11-22, Docs/design-lock-queue.md; user decision "入れてよい"). Builds, UE runs (tests, matches,
# benchmarks) and Source/Config/DLL/map changes share one CPU and go one at a time. Instead of handing the lock over in chat, every
# session uses this file queue. The queue lives outside the work trees, so every worktree sees the same one:
#   C:\Users\nanek\Documents\FPS\.lock\queue.json  (holder + waiting list)   queue-log.jsonl (history)
# Usage (PowerShell 5.1; Claude runs -Wait with run_in_background and is notified when it is its turn):
#   .\Lock-Queue.ps1 -Join -Who <session name> -Purpose <what> -Minutes <estimate> [-Urgent] [-Share]
#   .\Lock-Queue.ps1 -Wait -Who <session name>          # returns when <session name> holds the lock (exit 0), or times out (exit 3)
#   .\Lock-Queue.ps1 -Release -Who <session name>       # hands the lock to the next in the queue
#   .\Lock-Queue.ps1 -Leave -Who <session name>         # leaves the queue without taking the lock
#   .\Lock-Queue.ps1 -Status
# Rules kept from MEASURE.md: never stop another session's processes; the file is a marker, not an enforcement. A holder that is
# more than 30 min past its estimate while no UE / build process runs is treated as gone and the lock passes on (logged as stale).
# -Urgent (the user's own play test) joins at the front of the queue; it does not interrupt the holder.
#
# Match slots (2026-11-25, Docs/design-match-budget.md 8.1; user decision). Headless 30 FPS matches started from a frozen build
# (New-BattleBuildSnapshot) are not affected by builds or by other matches, so they no longer take the work queue above. They take
# match slots: up to 12 matches in total, from any number of sessions, alongside the holder of the work queue.
#   .\Lock-Queue.ps1 -Join -Who <name> -Matches <n> -Purpose <what> -Minutes <estimate>   # starts at once when n slots are free
#   .\Lock-Queue.ps1 -Wait -Who <name> [-For Work|Matches]   # also returns when <name>'s matches may start ("ACQUIRED_MATCHES").
#        Without -For: waits for the match slots when <name> is waiting for them, else for the work queue.
#   .\Lock-Queue.ps1 -Release -Who <name> -ToMatches <n> [-Minutes <m>] [-Purpose <what>]  # give up the work queue, keep n slots
#   .\Lock-Queue.ps1 -ReleaseMatches -Who <name>       # matches finished
# The work queue stays one at a time: builds, regression tests, merges into main, map generation. -Alone (performance and rendered
# measurements) joins the work queue and runs with no matches alongside: no new matches start while it waits or holds, and its
# -Wait returns only when the running matches have ended (a session cannot be -Alone and hold match slots at once). Match slots are kept in matches.json; queue.json keeps its old shape
# because -Wait loops started before this change still rewrite it (an -Alone entry is stored as share='alone'). -Share is kept as a mark
# only (matches run alongside every holder that is not -Alone).
[CmdletBinding(DefaultParameterSetName='Status')]
param(
 [Parameter(ParameterSetName='Join',Mandatory)][switch]$Join,
 [Parameter(ParameterSetName='Wait',Mandatory)][switch]$Wait,
 [Parameter(ParameterSetName='Release',Mandatory)][switch]$Release,
 [Parameter(ParameterSetName='Leave',Mandatory)][switch]$Leave,
 [Parameter(ParameterSetName='ReleaseMatches',Mandatory)][switch]$ReleaseMatches,
 [Parameter(ParameterSetName='Status')][switch]$Status,
 [Parameter(ParameterSetName='Join',Mandatory)][Parameter(ParameterSetName='Wait',Mandatory)][Parameter(ParameterSetName='Release',Mandatory)][Parameter(ParameterSetName='Leave',Mandatory)][Parameter(ParameterSetName='ReleaseMatches',Mandatory)][string]$Who,
 [Parameter(ParameterSetName='Join')][Parameter(ParameterSetName='Release')][string]$Purpose='',
 [Parameter(ParameterSetName='Join')][Parameter(ParameterSetName='Release')][ValidateRange(1,600)][int]$Minutes=30,
 [Parameter(ParameterSetName='Join')][switch]$Urgent,
 [Parameter(ParameterSetName='Join')][switch]$Share,
 [Parameter(ParameterSetName='Join')][switch]$Alone,
 [Parameter(ParameterSetName='Join')][ValidateRange(0,64)][int]$Matches=0,
 [Parameter(ParameterSetName='Release')][ValidateRange(0,64)][int]$ToMatches=0,
 [Parameter(ParameterSetName='Wait')][ValidateSet('Auto','Work','Matches')][string]$For='Auto',
 [Parameter(ParameterSetName='Wait')][ValidateRange(5,600)][int]$PollSeconds=30,
 [Parameter(ParameterSetName='Wait')][ValidateRange(1,1440)][int]$TimeoutMinutes=720,
 [string]$Dir='C:\Users\nanek\Documents\FPS\.lock'
)
$ErrorActionPreference='Stop'
# Session names are Japanese: print UTF-8 so that callers (Claude's Bash, terminals) read them back correctly.
try{[Console]::OutputEncoding=New-Object System.Text.UTF8Encoding($false)}catch{}
if(!(Test-Path -LiteralPath $Dir)){New-Item -ItemType Directory -Path $Dir -Force | Out-Null}
$queueFile=Join-Path $Dir 'queue.json'
$logFile=Join-Path $Dir 'queue-log.jsonl'
$mutexFile=Join-Path $Dir 'queue.mutex'
$slotFile=Join-Path $Dir 'matches.json'
$MaxMatches=12   # total headless matches at a time (MEASURE.md); fixed, not a per-call option
$utf8=New-Object System.Text.UTF8Encoding($false)

function Now-Iso { [DateTimeOffset]::Now.ToString('yyyy-MM-ddTHH:mm:sszzz') }

# Every read-modify-write holds an exclusive handle on queue.mutex (other callers retry for up to 60 s).
function Invoke-Locked([scriptblock]$Body){
 $deadline=[DateTime]::Now.AddSeconds(60);$handle=$null
 while($null -eq $handle){
  try{$handle=[System.IO.File]::Open($mutexFile,[System.IO.FileMode]::OpenOrCreate,[System.IO.FileAccess]::ReadWrite,[System.IO.FileShare]::None)}
  catch [System.IO.IOException]{if([DateTime]::Now -gt $deadline){throw 'Lock-Queue: queue file busy for 60 s'};Start-Sleep -Milliseconds 200}
 }
 try{& $Body}finally{$handle.Dispose()}
}
function Read-Queue {
 if(!(Test-Path -LiteralPath $queueFile)){return [pscustomobject]@{holder=$null;queue=@()}}
 $text=[System.IO.File]::ReadAllText($queueFile,$utf8)
 if([string]::IsNullOrWhiteSpace($text)){return [pscustomobject]@{holder=$null;queue=@()}}
 $q=$text|ConvertFrom-Json
 return [pscustomobject]@{holder=$q.holder;queue=@($q.queue|Where-Object{$_})}
}
function Write-Queue($Q){
 $json=[pscustomobject]@{holder=$Q.holder;queue=@($Q.queue)}|ConvertTo-Json -Depth 5
 [System.IO.File]::WriteAllText($queueFile,$json,$utf8)
}
function Write-Log([string]$Event,$Entry,[string]$Note=''){
 $line=[pscustomobject]@{time=(Now-Iso);event=$Event;who=$Entry.who;purpose=$Entry.purpose;minutes=$Entry.minutes;note=$Note}|ConvertTo-Json -Compress
 [System.IO.File]::AppendAllText($logFile,$line+"`n",$utf8)
}
function Busy-Processes {
 @(Get-Process -ErrorAction SilentlyContinue|Where-Object{$_.ProcessName -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|ShaderCompileWorker|link|cl)$'})
}
# The next in line becomes the holder when nobody holds the lock. Returns the updated queue.
function Advance($Q){
 if($null -eq $Q.holder -and @($Q.queue).Count -gt 0){
  $next=@($Q.queue)[0];$rest=@($Q.queue|Select-Object -Skip 1)
  $Q.holder=[pscustomobject]@{who=$next.who;purpose=$next.purpose;minutes=$next.minutes;since=(Now-Iso);share=$next.share}
  $Q.queue=$rest;Write-Log 'acquire' $Q.holder
 }
 return $Q
}
# A holder 30 min past its estimate with no UE / build process running is treated as gone.
function Drop-Stale($Q){
 if($null -ne $Q.holder){
  $since=[DateTimeOffset]::Parse($Q.holder.since);$limit=$since.AddMinutes([double]$Q.holder.minutes+30)
  if([DateTimeOffset]::Now -gt $limit -and (Busy-Processes).Count -eq 0){Write-Log 'stale' $Q.holder ("past "+$limit.ToString('HH:mm'));$Q.holder=$null}
 }
 return $Q
}
# ---- match slots (matches.json: running = matches that may run now, waiting = first come first served) ----
function Read-Slots {
 $empty=[pscustomobject]@{running=@();waiting=@()}
 if(!(Test-Path -LiteralPath $slotFile)){return $empty}
 $text=[System.IO.File]::ReadAllText($slotFile,$utf8)
 if([string]::IsNullOrWhiteSpace($text)){return $empty}
 $s=$text|ConvertFrom-Json
 return [pscustomobject]@{running=@($s.running|Where-Object{$_});waiting=@($s.waiting|Where-Object{$_})}
}
function Write-Slots($S){
 $json=[pscustomobject]@{running=@($S.running);waiting=@($S.waiting)}|ConvertTo-Json -Depth 5
 [System.IO.File]::WriteAllText($slotFile,$json,$utf8)
}
function Is-Alone($Entry){ $null -ne $Entry -and "$($Entry.share)" -eq 'alone' }
function Slots-Used($S){ $n=0;foreach($r in @($S.running)){$n+=[int]$r.matches};return $n }
function Match-Processes { @(Get-Process -ErrorAction SilentlyContinue|Where-Object{$_.ProcessName -match '^(UnrealEditor|UnrealEditor-Cmd)$'}) }
# Starts waiting matches in order while slots are free. Nothing starts while an -Alone entry holds or waits in the work
# queue (so a measurement is never starved by matches; the matches already running finish first).
function Advance-Slots($Q,$S){
 $kept=@()
 foreach($r in @($S.running)){
  $limit=([DateTimeOffset]::Parse($r.since)).AddMinutes([double]$r.minutes+30)
  if([DateTimeOffset]::Now -gt $limit -and (Match-Processes).Count -eq 0){Write-Log 'mstale' $r ("past "+$limit.ToString('HH:mm'))}else{$kept+=$r}
 }
 $S.running=$kept
 if((Is-Alone $Q.holder) -or @($Q.queue|Where-Object{Is-Alone $_}).Count -gt 0){return $S}
 $rest=@();$blocked=$false
 foreach($w in @($S.waiting)){
  if(!$blocked -and ((Slots-Used $S)+[int]$w.matches) -le $MaxMatches){
   $run=[pscustomobject]@{who=$w.who;purpose=$w.purpose;minutes=$w.minutes;matches=$w.matches;since=(Now-Iso)}
   $S.running=@($S.running)+@($run);Write-Log 'macquire' $run ("matches="+$w.matches)
  }else{$blocked=$true;$rest+=$w}
 }
 $S.waiting=$rest
 return $S
}
function Show-Slots($S){
 "matches: {0}/{1} slots in use" -f (Slots-Used $S),$MaxMatches
 foreach($r in @($S.running)){$since=[DateTimeOffset]::Parse($r.since);"  running: {0}  {1} matches  since {2}  until ~{3}  ({4})" -f $r.who,$r.matches,$since.ToString('HH:mm'),$since.AddMinutes([double]$r.minutes).ToString('HH:mm'),$r.purpose}
 foreach($w in @($S.waiting)){"  waiting: {0}  {1} matches  ~{2} min  ({3})  joined {4}" -f $w.who,$w.matches,$w.minutes,$w.purpose,([DateTimeOffset]::Parse($w.joined)).ToString('HH:mm')}
}
function Show($Q){
 if($null -eq $Q.holder){'holder: (free)'}
 else{
  $since=[DateTimeOffset]::Parse($Q.holder.since);$end=$since.AddMinutes([double]$Q.holder.minutes)
  "holder: {0}  since {1}  until ~{2}  ({3}){4}" -f $Q.holder.who,$since.ToString('HH:mm'),$end.ToString('HH:mm'),$Q.holder.purpose,$(if(Is-Alone $Q.holder){'  [alone: no matches alongside]'}elseif($Q.holder.share){'  [share: short headless runs of the next may run alongside]'}else{''})
 }
 $i=0;foreach($e in @($Q.queue)){$i++;"  {0}. {1}  ~{2} min  ({3})  joined {4}{5}" -f $i,$e.who,$e.minutes,$e.purpose,([DateTimeOffset]::Parse($e.joined)).ToString('HH:mm'),($(if($e.urgent){'  [urgent]'}else{''})+$(if(Is-Alone $e){'  [alone]'}else{''}))}
 if($i -eq 0){'  (queue empty)'}
 Show-Slots (Read-Slots)
}

switch($PSCmdlet.ParameterSetName){
 'Join' {
  Invoke-Locked {
   $q=Drop-Stale (Read-Queue)
   if($Matches -gt 0){
    if($Alone -or $Urgent){throw 'Lock-Queue: -Matches cannot be combined with -Alone or -Urgent'}
    if($Matches -gt $MaxMatches){throw "Lock-Queue: at most $MaxMatches matches at a time"}
    if(@(@($q.holder)+@($q.queue)|Where-Object{$_ -and $_.who -eq $Who -and (Is-Alone $_)}).Count){throw "Lock-Queue: $Who is -Alone in the work queue; it cannot hold match slots at the same time"}
    $s=Read-Slots
    if(@($s.running|Where-Object{$_.who -eq $Who}).Count){"ALREADY_RUNNING_MATCHES $Who"}
    elseif(@($s.waiting|Where-Object{$_.who -eq $Who}).Count){"ALREADY_QUEUED_MATCHES $Who"}
    else{
     $entry=[pscustomobject]@{who=$Who;purpose=$Purpose;minutes=$Minutes;matches=$Matches;joined=(Now-Iso)}
     $s.waiting=@($s.waiting)+@($entry);Write-Log 'mjoin' $entry ("matches="+$Matches)
    }
    $s=Advance-Slots $q $s;Write-Slots $s
    if(@($s.running|Where-Object{$_.who -eq $Who}).Count){"ACQUIRED_MATCHES $Who"}else{"QUEUED_MATCHES $Who"}
    Show $q;return
   }
   if($q.holder -and $q.holder.who -eq $Who){"ALREADY_HOLDER $Who";return}
   if($Alone){$own=Read-Slots;if(@(@($own.running)+@($own.waiting)|Where-Object{$_.who -eq $Who}).Count){throw "Lock-Queue: $Who holds or waits for match slots; release them (-ReleaseMatches / -Leave) before joining -Alone"}}
   if(@($q.queue|Where-Object{$_.who -eq $Who}).Count){"ALREADY_QUEUED $Who"}
   else{
    $entry=[pscustomobject]@{who=$Who;purpose=$Purpose;minutes=$Minutes;joined=(Now-Iso);urgent=[bool]$Urgent;share=$(if($Alone){'alone'}else{[bool]$Share})}
    $q.queue=if($Urgent){@($entry)+@($q.queue)}else{@($q.queue)+@($entry)}
    Write-Log 'join' $entry $(if($Urgent){'urgent'}elseif($Alone){'alone'}else{''})
   }
   $q=Advance $q;Write-Queue $q
   $s=Read-Slots
   if($q.holder.who -eq $Who -and (Is-Alone $q.holder) -and @($s.running).Count -gt 0){"QUEUED $Who (alone: waiting for the running matches to end; use -Wait)"}
   elseif($q.holder.who -eq $Who){"ACQUIRED $Who"}else{"QUEUED $Who"}
   Show $q
  }
 }
 'Wait' {
  $deadline=[DateTime]::Now.AddMinutes($TimeoutMinutes)
  while($true){
   $state=Invoke-Locked {
    $q=Advance (Drop-Stale (Read-Queue));Write-Queue $q
    $s=Advance-Slots $q (Read-Slots);Write-Slots $s
    $isHolder=[bool]($q.holder -and $q.holder.who -eq $Who);$inQueue=@($q.queue|Where-Object{$_.who -eq $Who}).Count -gt 0
    $runs=@($s.running|Where-Object{$_.who -eq $Who}).Count -gt 0;$waits=@($s.waiting|Where-Object{$_.who -eq $Who}).Count -gt 0
    # What this call waits for: the match slots when the session is waiting for them, else the work queue (-For overrides).
    $target=$For
    if($target -eq 'Auto'){$target=if($waits){'Matches'}elseif($isHolder -or $inQueue){'Work'}elseif($runs){'Matches'}else{'None'}}
    if($target -eq 'Matches'){if($runs){'matches'}elseif($waits){'waiting'}else{'absent'}}
    elseif($target -eq 'Work'){
     if($isHolder){if((Is-Alone $q.holder) -and @($s.running).Count -gt 0){'waiting'}else{'held'}}elseif($inQueue){'waiting'}else{'absent'}
    }
    else{'absent'}
   }
   if($state -eq 'held'){"ACQUIRED $Who";Invoke-Locked {Show (Read-Queue)};exit 0}
   if($state -eq 'matches'){"ACQUIRED_MATCHES $Who";Invoke-Locked {Show (Read-Queue)};exit 0}
   if($state -eq 'absent'){"NOT_IN_QUEUE $Who (run -Join first)";exit 2}
   if([DateTime]::Now -gt $deadline){"TIMEOUT $Who";exit 3}
   Start-Sleep -Seconds $PollSeconds
  }
 }
 'Release' {
  Invoke-Locked {
   $q=Read-Queue
   $s=Read-Slots
   if($ToMatches -gt $MaxMatches){throw "Lock-Queue: at most $MaxMatches matches at a time"}
   if($ToMatches -gt 0 -and @(@($s.running)+@($s.waiting)|Where-Object{$_.who -eq $Who}).Count){throw "Lock-Queue: $Who already holds or waits for match slots; -ReleaseMatches (or -Leave) first, or release without -ToMatches"}
   if($ToMatches -gt 0 -and (Is-Alone $q.holder) -and $q.holder.who -eq $Who){throw "Lock-Queue: an -Alone holder cannot move to match slots; -Release, then -Join -Matches"}
   if($q.holder -and $q.holder.who -eq $Who){
    $old=$q.holder;Write-Log 'release' $old $(if($ToMatches -gt 0){"to matches=$ToMatches"}else{''});$q.holder=$null
    if($ToMatches -gt 0){
     # The former holder goes to the front of the waiting matches: it was in line before everybody who joined during its hold.
     $entry=[pscustomobject]@{who=$Who;purpose=$(if($Purpose){$Purpose}else{$old.purpose});minutes=$Minutes;matches=$ToMatches;joined=$old.since}
     $s.waiting=@($entry)+@($s.waiting);Write-Log 'mjoin' $entry ("matches="+$ToMatches)
     $s=Advance-Slots $q $s
    }
    $q=Advance $q;Write-Queue $q;$s=Advance-Slots $q $s;Write-Slots $s
    "RELEASED $Who"
    if($ToMatches -gt 0){if(@($s.running|Where-Object{$_.who -eq $Who}).Count){"ACQUIRED_MATCHES $Who"}else{"QUEUED_MATCHES $Who (use -Wait)"}}
   }
   elseif(@($s.running|Where-Object{$_.who -eq $Who}).Count){
    foreach($r in @($s.running|Where-Object{$_.who -eq $Who})){Write-Log 'mrelease' $r ("matches="+$r.matches)}
    $s.running=@($s.running|Where-Object{$_.who -ne $Who});$s=Advance-Slots $q $s;Write-Slots $s;"RELEASED_MATCHES $Who"
   }
   else{"NOT_HOLDER $Who"}
   Show $q
  }
 }
 'Leave' {
  Invoke-Locked {
   $q=Read-Queue;$before=@($q.queue).Count;$q.queue=@($q.queue|Where-Object{$_.who -ne $Who})
   $s=Read-Slots;$waitingBefore=@($s.waiting).Count;$s.waiting=@($s.waiting|Where-Object{$_.who -ne $Who})
   $leftSlots=@($s.waiting).Count -lt $waitingBefore
   if(@($q.queue).Count -lt $before){Write-Log 'leave' ([pscustomobject]@{who=$Who;purpose='';minutes=0});Write-Queue $q;"LEFT $Who"}elseif(!$leftSlots){"NOT_QUEUED $Who"}
   if($leftSlots){Write-Log 'mleave' ([pscustomobject]@{who=$Who;purpose='';minutes=0})}
   # Leaving the work queue may remove a waiting -Alone entry, which lets waiting matches start.
   $s=Advance-Slots $q $s;Write-Slots $s
   if($leftSlots){"LEFT_MATCHES $Who"}
   Show $q
  }
 }
 'ReleaseMatches' {
  Invoke-Locked {
   $q=Read-Queue;$s=Read-Slots
   if(@($s.running|Where-Object{$_.who -eq $Who}).Count){
    foreach($r in @($s.running|Where-Object{$_.who -eq $Who})){Write-Log 'mrelease' $r ("matches="+$r.matches)}
    $s.running=@($s.running|Where-Object{$_.who -ne $Who});$s=Advance-Slots $q $s;Write-Slots $s;"RELEASED_MATCHES $Who"
   }else{"NOT_RUNNING_MATCHES $Who"}
   Show $q
  }
 }
 default { Invoke-Locked {Show (Drop-Stale (Read-Queue))} }
}
