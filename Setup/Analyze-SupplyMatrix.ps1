# Summarises Run-SupplyMatrix.ps1 outputs (Benchmarks/SightTrial-20261111/supply-<condition>-<map>-s<seed>[-swap]-<tag>).
# One line per condition, summed over seeds, maps and side swaps. Failed or missing runs are listed and excluded.
[CmdletBinding()]
param([string[]]$Conditions=@('base','A','B','C','D','all'),[string]$Tag='r1')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
foreach($c in $Conditions){
 $a=@{N=0;Fail=@();Ret=0;Res=0;Abort=0;NoStart=0;Trips=@();Lost=0;OpenEnd=0;Other=0;Front=0;Rec=0;Eng=0;All=0;LD=0;FD=0;LLF=0;Comp=0;Credits=0;Shots=0;Alive=0;W0=0;Diff=0}
 foreach($d in Get-ChildItem $root -Directory -Filter "supply-$c-*-$Tag"){
  $res=Get-Content (Join-Path $d.FullName 'result.json') -Raw -ErrorAction SilentlyContinue|ConvertFrom-Json
  $mp=Join-Path $d.FullName 'metrics.json'
  if(!$res -or !$res.passed -or !(Test-Path $mp)){$a.Fail+=$d.Name;continue}
  $m=Get-Content $mp -Raw -Encoding UTF8|ConvertFrom-Json; $e=$m.tactical_events; $s=$m.tactical_squad_seconds; $sc=$m.supply_cycle; $l=$m.leader_survival
  $a.N++; $a.Ret+=[int]$e.HomeSupplyReturn; $a.Res+=[int]$e.HomeSupplyResume; $a.Abort+=[int]$e.HomeSupplyAbortEmergency; $a.NoStart+=[int]$e.HomeSupplyEndWithoutStart
  $a.Trips+=@($sc.resumed_trip_seconds); $a.Lost+=$sc.open_trips_leader_lost; $a.OpenEnd+=$sc.open_trips_in_progress_at_end; $a.Other+=$sc.open_trips_replaced_by_other_plan
  $a.Front+=$sc.front_person_seconds_team0+$sc.front_person_seconds_team1
  $a.Rec+=[double]$s.'RECOVER / DEFEND'; $a.Eng+=[double]$s.ENGAGE; foreach($p in $s.PSObject.Properties){$a.All+=[double]$p.Value}
  foreach($t in 'team0','team1'){$a.LD+=$l.$t.leader_combat_deaths;$a.FD+=$l.$t.follower_combat_deaths;$a.LLF+=$l.$t.leader_loss_followers}
  $a.Comp+=$m.completed_replacements; $a.Credits+=$m.ally_resources+$m.enemy_resources; $a.Shots+=$m.shots; $a.Alive+=$m.ally_alive+$m.enemy_alive
  $a.W0+=[int]($m.winner -eq 0); $a.Diff+=[math]::Abs($m.ally_score-$m.enemy_score)
 }
 if($a.N -eq 0){"$c : no completed runs; failed/missing=$($a.Fail -join ',')";continue}
 $avgTrip=if($a.Trips.Count){($a.Trips|Measure-Object -Average).Average}else{0}
 $loss=$a.LD+$a.FD+$a.LLF
 ('{0} (n={1}): returns={2} resumed={3} abortEmergency={4} openLeaderLost={5} openAtEnd={6} otherPlan={7} noStart={8} avgTripSec={9:N0} recoverTime={10:P0} engageTime={11:P0} frontPersonSec/run={12:N0} loss={13} (leader={14} follower={15} withLeader={16}) replacements/run={17:N0} creditsLeft/run={18:N0} shots/run={19:N0} aliveEnd/run={20:N0} team0Wins={21} mean|diff|={22:N1}' -f
  $c,$a.N,$a.Ret,$a.Res,$a.Abort,$a.Lost,$a.OpenEnd,$a.Other,$a.NoStart,$avgTrip,($a.Rec/[math]::Max(1,$a.All)),($a.Eng/[math]::Max(1,$a.All)),($a.Front/$a.N),$loss,$a.LD,$a.FD,$a.LLF,($a.Comp/$a.N),($a.Credits/$a.N),($a.Shots/$a.N),($a.Alive/$a.N),$a.W0,($a.Diff/$a.N))
 if($a.Fail.Count){"   failed/missing: $($a.Fail -join ',')"}
}
