# Economy and front-line timeline (2026-11-16). Reads metrics.json of Run-EconomyMatrix.ps1 outputs
# (Benchmarks/SightTrial-20261111/<prefix>-<condition>-*-<tag>) and prints, per condition, the mean per team and run:
#  - at each full minute: resources held, cumulative income, new-squad spend, replacement spend
#    (= StartCredits + income - held - squad spend), living soldiers;
#  - every 60 s: own front and most advanced visible enemy (progress 0 = own home, 1 = enemy home, mean over sectors (3 lanes on A-D, 5 on E) that have
#    a value), own soldiers and visible enemies summed over the sectors.
[CmdletBinding()]
param([string[]]$Conditions=@('HI'),[string]$Tag='r17',[string]$Prefix='econ',[int]$StartCredits=100)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot 'Benchmarks/SightTrial-20261111'
foreach($c in $Conditions){
 $e=New-Object 'double[,]' 10,5;$f=New-Object 'double[,]' 20,6;$n=0
 foreach($d in Get-ChildItem $root -Directory -Filter "$Prefix-$c-*-$Tag"){
  $mp=Join-Path $d.FullName 'metrics.json';if(!(Test-Path $mp)){continue}
  $m=Get-Content $mp -Raw -Encoding UTF8|ConvertFrom-Json
  if($null -eq $m.ally_economy_per_minute){"   no timeline: $($d.Name)";continue}
  foreach($side in 'ally','enemy'){
   $n++
   $rows=@($m."$($side)_economy_per_minute")
   for($i=0;$i -lt 10;$i++){$r=@($rows[$i]);$e[$i,0]+=$r[0];$e[$i,1]+=$r[1];$e[$i,2]+=$r[2];$e[$i,3]+=$StartCredits+$r[1]-$r[0]-$r[2];$e[$i,4]+=$r[3]}
   $fr=@($m."$($side)_front_per_30s")
   if($fr.Count -eq 20){for($s=0;$s -lt 20;$s++){$r=@($fr[$s])
    $of=@();$ef=@();$own=0;$seen=0
    for($l=0;$l -lt [int]($r.Count/4);$l++){if($r[4*$l] -ge 0){$of+=$r[4*$l]};if($r[4*$l+1] -ge 0){$ef+=$r[4*$l+1]};$own+=$r[4*$l+2];$seen+=$r[4*$l+3]}
    if($of.Count){$f[$s,0]+=($of|Measure-Object -Average).Average;$f[$s,1]++}
    if($ef.Count){$f[$s,2]+=($ef|Measure-Object -Average).Average;$f[$s,3]++}
    $f[$s,4]+=$own;$f[$s,5]+=$seen}}
  }
 }
 if(!$n){"$c : no runs";continue}
 "$c (team-runs=$n)"
 '  min | held | income | squadSpend | replaceSpend | living'
 for($i=0;$i -lt 10;$i++){'  {0,3} | {1,4:N0} | {2,6:N0} | {3,10:N0} | {4,12:N0} | {5,6:N0}' -f ($i+1),($e[$i,0]/$n),($e[$i,1]/$n),($e[$i,2]/$n),($e[$i,3]/$n),($e[$i,4]/$n)}
 '  sec | ownFront | enemyFront(seen) | own | enemySeen'
 for($s=1;$s -lt 20;$s+=2){$k1=$f.GetValue($s,1);if($k1 -lt 1){$k1=1};$k3=$f.GetValue($s,3);if($k3 -lt 1){$k3=1};'  {0,3} | {1,8:N2} | {2,16:N2} | {3,3:N0} | {4,9:N0}' -f (($s+1)*30),($f[$s,0]/$k1),($f[$s,2]/$k3),($f[$s,4]/$n),($f[$s,5]/$n)}
}
