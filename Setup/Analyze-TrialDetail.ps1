# Detail view for the suppression and war score trials (Run-EconomyMatrix.ps1 outputs econ-<condition>-<map>-s<seed>[-swap]-<tag>).
# Per condition, summed over runs: fire and hit totals, suppression counters from cover_diagnostics, and the war score record.
# Failed or missing runs are excluded and counted. Use Analyze-EconomyMatrix.ps1 for survival, losses and the paired comparison.
[CmdletBinding()]
param([string[]]$Conditions=@('HI','P','PS','PP','W'),[string]$Tag='p2',[string]$Prefix='econ')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
$keys='suppression_events','suppressed_shots','peek_hold_suppressed','peek_end_suppressed','reached_peek','cover_shots','request_success'
foreach($c in $Conditions){
 $n=0;$fail=0;$shots=0;$hits=0;$diag=@{};foreach($k in $keys){$diag[$k]=0}
 $warPoint=0;$warLoss=0;$loss=0;$sameWinner=0;$warRuns=0;$estErr=0;$durations=@()
 foreach($d in Get-ChildItem $root -Directory -Filter "$Prefix-$c-*-$Tag"){
  $res=Get-Content (Join-Path $d.FullName 'result.json') -Raw -ErrorAction SilentlyContinue|ConvertFrom-Json
  $mp=Join-Path $d.FullName 'metrics.json'
  if(!$res -or !$res.passed -or !(Test-Path $mp)){$fail++;continue}
  $m=Get-Content $mp -Raw -Encoding UTF8|ConvertFrom-Json;$n++
  $shots+=$m.shots;$hits+=$m.physical_unit_hits;$durations+=[double]$m.match_seconds
  foreach($k in $keys){$v=$m.cover_diagnostics.$k;if($v){$diag[$k]+=[int]$v}}
  $l=$m.leader_survival
  $l0=$l.team0.leader_combat_deaths+$l.team0.follower_combat_deaths+$l.team0.leader_loss_followers
  $l1=$l.team1.leader_combat_deaths+$l.team1.follower_combat_deaths+$l.team1.leader_loss_followers
  $loss+=$l0+$l1
  $warPoint+=[int]$m.ally_war_point+[int]$m.enemy_war_point;$warLoss+=[int]$m.ally_war_loss+[int]$m.enemy_war_loss
  if(([int]$m.ally_war_point+[int]$m.enemy_war_point+[int]$m.ally_war_loss+[int]$m.enemy_war_loss) -gt 0){
   $warRuns++
   # Old formula recomputed from the same end state, to see how often the war score changes the winner.
   $pts0=@($m.final_objectives|Where-Object{$_.owner -eq 0}).Count;$pts1=@($m.final_objectives|Where-Object{$_.owner -eq 1}).Count
   $old0=$m.ally_alive+$m.ally_resources+20*$pts0;$old1=$m.enemy_alive+$m.enemy_resources+20*$pts1
   $oldWinner=if($old0 -eq $old1){-1}elseif($old0 -gt $old1){0}else{1}
   if($oldWinner -eq $m.winner){$sameWinner++}
   # Estimate error: what each side believed of the opponent's earned score against the truth.
   $estErr+=[math]::Abs([int]$m.ally_war_estimate_of_opponent-([int]$m.enemy_war_point+[int]$m.enemy_war_loss))+[math]::Abs([int]$m.enemy_war_estimate_of_opponent-([int]$m.ally_war_point+[int]$m.ally_war_loss))
  }
 }
 if($n -eq 0){"$c : no completed runs (failed/missing=$fail)";continue}
 $rate=if($shots){100.0*$hits/$shots}else{0}
 ('{0} (n={1}, failed={2}): shots/run={3:N0} hits/run={4:N0} hitRate={5:N2}% loss/run={6:N0} meanMatchSec={7:N0}' -f $c,$n,$fail,($shots/$n),($hits/$n),$rate,($loss/$n),(($durations|Measure-Object -Average).Average))
 ('   suppression: events/run={0:N0} suppressedShots/run={1:N0} peekHeld/run={2:N0} peekDucked/run={3:N0} | cover: requests ok/run={4:N0} reachedPeek/run={5:N0} coverShots/run={6:N0}' -f ($diag['suppression_events']/$n),($diag['suppressed_shots']/$n),($diag['peek_hold_suppressed']/$n),($diag['peek_end_suppressed']/$n),($diag['request_success']/$n),($diag['reached_peek']/$n),($diag['cover_shots']/$n))
 if($warRuns){('   war score: pointScore/run={0:N0} lossScore/run={1:N0} (losses counted by the experiment/run={2:N0}) sameWinnerAsOldFormula={3}/{4} meanEstimateError/side={5:N1}' -f ($warPoint/$warRuns),($warLoss/$warRuns),($loss/$n),$sameWinner,$warRuns,($estErr/(2*$warRuns)))}
}
