# Summarises where S1 home-assault squads spend their time (2026-11-15). Reads metrics.json of Run-EconomyMatrix.ps1 outputs
# (Benchmarks/SightTrial-20261111/<prefix>-<condition>-*-<tag>) and prints, per condition, summed over runs and both teams:
#  - person-seconds by progress from own home (0.0) to enemy home (1.0) in tenths,
#  - person-seconds by squad state (squad tactic, home supply trip, no mission),
#  - mean and max of the furthest progress reached per team and run.
[CmdletBinding()]
param([string[]]$Conditions=@('HI'),[string]$Tag='r8',[string]$Prefix='econ')
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
$stateNames='Mission','Engage','Support','Flank','Retreat','Regroup','Recover','SupplyTrip','NoMission'
foreach($c in $Conditions){
 $prog=New-Object double[] 10;$state=New-Object double[] 9;$max=@();$n=0
 foreach($d in Get-ChildItem $root -Directory -Filter "$Prefix-$c-*-$Tag"){
  $mp=Join-Path $d.FullName 'metrics.json';if(!(Test-Path $mp)){"   missing: $($d.Name)";continue}
  $m=Get-Content $mp -Raw -Encoding UTF8|ConvertFrom-Json
  if($null -eq $m.ally_base_assault_progress_seconds){"   no base-assault metrics: $($d.Name)";continue}
  $n++
  foreach($side in 'ally','enemy'){
   $p=@($m."$($side)_base_assault_progress_seconds");$s=@($m."$($side)_base_assault_state_seconds")
   for($i=0;$i -lt 10;$i++){$prog[$i]+=[double]$p[$i]};for($i=0;$i -lt 9;$i++){$state[$i]+=[double]$s[$i]}
   $max+=[double]$m."$($side)_base_assault_max_progress"
  }
 }
 if(!$n){"$c : no runs";continue}
 $tp=($prog|Measure-Object -Sum).Sum;$ts=($state|Measure-Object -Sum).Sum
 "$c (n=$n runs, person-sec/run={0:N0})" -f ($tp/$n)
 '  progress  : '+((0..9|ForEach-Object{'{0:0.0}-{1:0.0} {2:P0}' -f ($_/10),(($_+1)/10),($prog[$_]/[math]::Max(1,$tp))}) -join ' | ')
 '  state     : '+((0..8|ForEach-Object{'{0} {1:P0}' -f $stateNames[$_],($state[$_]/[math]::Max(1,$ts))}) -join ' | ')
 '  maxProgress per team-run: mean={0:N2} max={1:N2}' -f ($max|Measure-Object -Average).Average,($max|Measure-Object -Maximum).Maximum
}
