# Summarises Run-EconomyMatrix.ps1 outputs (Benchmarks/SightTrial-20261111/econ-<condition>-<map>-s<seed>[-swap]-<tag>).
# One line per condition, summed over seeds, maps and side swaps, plus a paired comparison of each condition with -Baseline.
# Failed or missing runs are listed and excluded.
[CmdletBinding()]
param([string[]]$Conditions=@('HI','H','I','none'),[string]$Tag='r1',[string]$Baseline='none',[string]$Prefix='econ',[ValidateRange(-1,1)][int]$TrialTeam=-1)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
$runs=@{}
foreach($c in $Conditions){
 $a=@{N=0;Fail=@();Alive=0;Credits=0;Income=0;Hires=0;HireTimes=@();Late=0;Comp=0;Loss=0;LD=0;Front=0;Shots=0;W0=0;W1=0;Draw=0;Base=0;Diff=0;Score=0;Points=0;BaseDmg=0;HomeRange=0}
 foreach($d in Get-ChildItem $root -Directory -Filter "$Prefix-$c-*-$Tag"){
  $res=Get-Content (Join-Path $d.FullName 'result.json') -Raw -ErrorAction SilentlyContinue|ConvertFrom-Json
  $mp=Join-Path $d.FullName 'metrics.json'
  if(!$res -or !$res.passed -or !(Test-Path $mp)){$a.Fail+=$d.Name;continue}
  $m=Get-Content $mp -Raw -Encoding UTF8|ConvertFrom-Json;$l=$m.leader_survival;$sc=$m.supply_cycle
  $key=$d.Name.Substring("$Prefix-$c-".Length);$runs["$c|$key"]=$m
  $a.N++;$a.Alive+=$m.ally_alive+$m.enemy_alive;$a.Credits+=$m.ally_resources+$m.enemy_resources
  $a.Income+=[int]$m.ally_income+[int]$m.enemy_income;$a.Hires+=[int]$m.ally_hired_squads+[int]$m.enemy_hired_squads
  $times=@($m.ally_hire_times)+@($m.enemy_hire_times)|Where-Object{$_ -ne $null};$a.HireTimes+=$times;$a.Late+=@($times|Where-Object{$_ -ge 480}).Count
  $a.Comp+=$m.completed_replacements
  foreach($t in 'team0','team1'){$a.LD+=$l.$t.leader_combat_deaths;$a.Loss+=$l.$t.leader_combat_deaths+$l.$t.follower_combat_deaths+$l.$t.leader_loss_followers}
  $a.Front+=$sc.front_person_seconds_team0+$sc.front_person_seconds_team1;$a.Shots+=$m.shots
  $a.W0+=[int]($m.winner -eq 0);$a.W1+=[int]($m.winner -eq 1);$a.Draw+=[int]($m.winner -eq -1);$a.Base+=[int]($m.reason -match 'BASE')
  $a.Diff+=[math]::Abs($m.ally_score-$m.enemy_score);$a.Score+=$m.ally_score+$m.enemy_score
  $a.Points+=@($m.final_objectives|Where-Object{$_.owner -ge 0}).Count
  $a.BaseDmg+=(1000-[double]$m.ally_base_hp)+(1000-[double]$m.enemy_base_hp)
  $a.HomeRange+=[double]$m.ally_at_opponent_home_range_seconds+[double]$m.enemy_at_opponent_home_range_seconds
 }
 if($a.N -eq 0){"$c : no completed runs; failed/missing=$($a.Fail -join ',')";continue}
 $first=if($a.HireTimes.Count){($a.HireTimes|Measure-Object -Average).Average}else{0}
 ('{0} (n={1}): aliveEnd/run={2:N0} creditsLeft/run={3:N0} income/run={4:N0} hires/run={5:N1} meanHireSec={6:N0} hiresAfter480s={7} replacements/run={8:N0} loss/run={9:N0} leaderDeaths/run={10:N1} frontPersonSec/run={11:N0} shots/run={12:N0} wins0/1/draw={13}/{14}/{15} baseKills={16} scoreSum/run={17:N0} mean|diff|={18:N1} ownedPointsEnd/run={19:N2} baseDamage/run={20:N0} atEnemyHomeRangeSec/run={21:N0}' -f
  $c,$a.N,($a.Alive/$a.N),($a.Credits/$a.N),($a.Income/$a.N),($a.Hires/$a.N),$first,$a.Late,($a.Comp/$a.N),($a.Loss/$a.N),($a.LD/$a.N),($a.Front/$a.N),($a.Shots/$a.N),$a.W0,$a.W1,$a.Draw,$a.Base,($a.Score/$a.N),($a.Diff/$a.N),($a.Points/$a.N),($a.BaseDmg/$a.N),($a.HomeRange/$a.N))
 if($a.Fail.Count){"   failed/missing: $($a.Fail -join ',')"}
}
# Paired comparison (same map/seed/side) against the baseline condition.
foreach($c in $Conditions|Where-Object{$_ -ne $Baseline}){
 $pairs=0;$dAlive=0;$better=0;$dFront=0;$dLoss=0
 foreach($k in $runs.Keys|Where-Object{$_ -like "$c|*"}){
  $b=$runs["$Baseline|"+$k.Split('|')[1]];if(!$b){continue};$m=$runs[$k];$pairs++
  $x=$m.ally_alive+$m.enemy_alive;$y=$b.ally_alive+$b.enemy_alive;$dAlive+=$x-$y;$better+=[int]($x -gt $y)
  $dFront+=($m.supply_cycle.front_person_seconds_team0+$m.supply_cycle.front_person_seconds_team1)-($b.supply_cycle.front_person_seconds_team0+$b.supply_cycle.front_person_seconds_team1)
 }
 if($pairs){('{0} vs {1}: pairs={2} aliveEnd diff/run={3:N1} (higher in {4}/{2}) frontPersonSec diff/run={5:N0}' -f $c,$Baseline,$pairs,($dAlive/$pairs),$better,($dFront/$pairs))}
}
# One-team trials (Run-EconomyMatrix -TrialTeam): the trial team's wins, score lead and survivor lead per run.
if($TrialTeam -ge 0){
 $me=if($TrialTeam){'enemy'}else{'ally'};$them=if($TrialTeam){'ally'}else{'enemy'}
 foreach($c in $Conditions){
  $ks=@($runs.Keys|Where-Object{$_ -like "$c|*"});if(!$ks.Count){continue}
  $w=0;$l=0;$ds=0;$da=0
  foreach($k in $ks){$m=$runs[$k];$w+=[int]($m.winner -eq $TrialTeam);$l+=[int]($m.winner -eq 1-$TrialTeam);$ds+=$m."$($me)_score"-$m."$($them)_score";$da+=$m."$($me)_alive"-$m."$($them)_alive"}
  ('{0} trial team {1}: wins {2} losses {3} of {4}, score lead/run={5:N1}, survivor lead/run={6:N1}' -f $c,$TrialTeam,$w,$l,$ks.Count,($ds/$ks.Count),($da/$ks.Count))
 }
}
