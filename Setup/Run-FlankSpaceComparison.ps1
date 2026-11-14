[CmdletBinding()]
param([string]$Build='mixed-save-r3',[string]$Prefix='flank-space-r3')
$ErrorActionPreference='Stop'
$snapshot=Join-Path $PSScriptRoot "Benchmarks/BuildSnapshots/$Build/manifest.json"
$runner=Join-Path $PSScriptRoot 'Test-WideSight.ps1'
foreach($range in 5000,10000){
 $jobs=@(foreach($map in 'A','B'){foreach($swap in 0,1){
  Start-Job -ScriptBlock {param($runner,$snapshot,$name,$map,$swap,$range)
   & $runner -Map $map -OutputName $name -Swap:([bool]$swap) -Experiment -SightRange $range -WeaponRange 5000 -FlankEnabled 1 -ObserveTactics -BehaviorConcurrency 4 -FrozenBuild $snapshot
  } -ArgumentList $runner,$snapshot,"$Prefix-s$range-$map-$swap",$map,$swap,$range
 }})
 $jobs|Wait-Job|Receive-Job
 foreach($job in $jobs){if($job.State -ne 'Completed' -or $job.ChildJobs[0].Error.Count){throw 'Flank space comparison failed'}}
 $jobs|Remove-Job
}
