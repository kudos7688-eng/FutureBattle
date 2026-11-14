[CmdletBinding()]
param([string]$Build='st4a-input-r8',[string]$Prefix='st4a-final-r8')
$ErrorActionPreference='Stop'
$snapshot=Join-Path $PSScriptRoot "Benchmarks/BuildSnapshots/$Build/manifest.json"
$runner=Join-Path $PSScriptRoot 'Test-WideSight.ps1'
foreach($flank in 0,1){
 $jobs=@(foreach($map in 'A','B'){foreach($swap in 0,1){
  Start-Job -ScriptBlock {param($runner,$snapshot,$name,$map,$swap,$flank)
   & $runner -Map $map -OutputName $name -Swap:([bool]$swap) -Experiment -SightRange 10000 -WeaponRange 5000 -FlankEnabled $flank -BehaviorConcurrency 4 -FrozenBuild $snapshot
  } -ArgumentList $runner,$snapshot,"$Prefix-f$flank-$map-$swap",$map,$swap,$flank
 }})
 $jobs|Wait-Job|Receive-Job
 foreach($job in $jobs){if($job.State -ne 'Completed' -or $job.ChildJobs[0].Error.Count){throw 'Tactical matrix failed'}}
 $jobs|Remove-Job
}
foreach($repeat in 1..3){ & $runner -Map B -OutputName "$Prefix-render-$repeat" -Experiment -SightRange 10000 -WeaponRange 5000 -Rendered -FrozenBuild $snapshot }
