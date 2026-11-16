# Squad type activity (2026-11-16). Reads *_type_activity from metrics.json of Run-EconomyMatrix outputs and prints, per squad
# type (0 rifle squad, 1 heavy infantry), totals over runs and both teams plus rates per 1000 living person-seconds:
# share of time near objectives, shots, damage dealt, kills, damage taken, deaths, and the exchange ratio (damage dealt / taken).
[CmdletBinding()]
param([string[]]$Conditions=@('HV33'),[string]$Tag='r25',[string]$Prefix='econ')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
$names='rifle squad','heavy infantry','scout pair','anti-tank team'
foreach($c in $Conditions){
 $sum=@{};foreach($k in 'person_seconds','point_person_seconds','shots','damage','kills','damage_taken','deaths'){$sum[$k]=New-Object double[] 4}
 $n=0
 foreach($d in Get-ChildItem $root -Directory -Filter "$Prefix-$c-*-$Tag"){
  $mp=Join-Path $d.FullName 'metrics.json';if(!(Test-Path $mp)){"   missing: $($d.Name)";continue}
  $m=Get-Content $mp -Raw -Encoding UTF8|ConvertFrom-Json
  if($null -eq $m.ally_type_activity){"   no type activity: $($d.Name)";continue}
  $n++
  foreach($side in 'ally','enemy'){foreach($row in @($m."$($side)_type_activity")){foreach($k in $sum.Keys){$sum[$k][[int]$row.type]+=[double]$row.$k}}}
 }
 if(!$n){"$c : no runs";continue}
 "$c (runs=$n, both teams)"
 '  type           | person-sec | near point | shots/1k | damage/1k | kills/1k | taken/1k | deaths/1k | dealt/taken'
 for($t=0;$t -lt 4;$t++){
  $ps=[math]::Max(1,$sum['person_seconds'][$t]);$k=1000/$ps
  '  {0,-14} | {1,10:N0} | {2,9:P0} | {3,8:N1} | {4,9:N0} | {5,8:N2} | {6,8:N0} | {7,9:N2} | {8,11:N2}' -f $names[$t],$sum['person_seconds'][$t],($sum['point_person_seconds'][$t]/$ps),($sum['shots'][$t]*$k),($sum['damage'][$t]*$k),($sum['kills'][$t]*$k),($sum['damage_taken'][$t]*$k),($sum['deaths'][$t]*$k),($sum['damage'][$t]/[math]::Max(1,$sum['damage_taken'][$t]))
 }
}
