[CmdletBinding()]
param([string]$Build='flank-effect-r1',[string]$Prefix='flank-effect-r1',[switch]$Pilot)
$ErrorActionPreference='Stop'
$snapshot=Join-Path $PSScriptRoot "Benchmarks/BuildSnapshots/$Build/manifest.json"
$runner=Join-Path $PSScriptRoot 'Test-WideSight.ps1'
$cases=@()
foreach($map in 'A','B'){foreach($seed in 7,19,43){foreach($swap in 0,1){
 foreach($variant in 'strict','relaxed','on0','on1'){
  if($Pilot -and ($map -ne 'A' -or $seed -ne 7 -or $swap -ne 0)){continue}
  $name="$Prefix-$map-s$seed-x$swap-$variant"
  $result=Join-Path $PSScriptRoot "Benchmarks/SightTrial-20261111/$name/result.json"
  if(Test-Path $result){
   $r=Get-Content $result -Raw|ConvertFrom-Json
   $m=Get-Content $snapshot -Raw|ConvertFrom-Json
   if(!$r.passed -or $r.dll -ne $m.dll){throw "Existing failed/mismatched result: $name"}
   continue
  }
  $cases+=@{map=$map;seed=$seed;swap=$swap;variant=$variant;name=$name}
 }
}}}
for($i=0;$i -lt $cases.Count;$i+=4){
 $jobs=@(foreach($c in $cases[$i..([Math]::Min($i+3,$cases.Count-1))]){
  Start-Job -ScriptBlock {param($runner,$snapshot,$c)
   $ErrorActionPreference='Stop'
   $relaxed=if($c.variant -eq 'strict'){0}else{1}
   $teams=if($c.variant -eq 'on0'){1}elseif($c.variant -eq 'on1'){2}else{3}
   & $runner -Map $c.map -Seed $c.seed -Swap:([bool]$c.swap) -OutputName $c.name -Experiment -Area -SightRange 10000 -WeaponRange 5000 -BehaviorConcurrency 4 -FrozenBuild $snapshot -FlankRelaxed $relaxed -FlankTeams $teams -FlankTrial
  } -ArgumentList $runner,$snapshot,$c
 })
 $jobs|Wait-Job|Out-Null
 foreach($job in $jobs){
  $output=@(Receive-Job $job)
  if($job.State -ne 'Completed' -or $job.ChildJobs[0].Error.Count){throw "Flank trial failed: $($job.ChildJobs[0].Error)"}
  $output|Where-Object {$_ -match 'SIGHT_EXPERIMENT_RESULT'}|Write-Output
 }
 $jobs|Remove-Job
 Write-Output "Completed batch through $([Math]::Min($i+4,$cases.Count))/$($cases.Count) new runs"
}
