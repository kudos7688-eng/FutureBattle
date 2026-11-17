[CmdletBinding()]
param([string]$Build='area-r6',[string]$Prefix='area-full-r6',[switch]$SkipRendered)
$ErrorActionPreference='Stop'
$snapshot=Join-Path $PSScriptRoot "Benchmarks/BuildSnapshots/$Build/manifest.json"
$runner=Join-Path $PSScriptRoot 'Test-WideSight.ps1'
$jobs=@(foreach($map in 'A','B'){foreach($swap in 0,1){
 Start-Job -ScriptBlock {param($runner,$snapshot,$name,$map,$swap)
  & $runner -Map $map -OutputName $name -Swap:([bool]$swap) -Experiment -Area -SightRange 10000 -WeaponRange 5000 -BehaviorConcurrency 4 -FrozenBuild $snapshot -RoundTripSave:($map -eq 'A' -and $swap -eq 0)
 } -ArgumentList $runner,$snapshot,"$Prefix-$map-$swap",$map,$swap
}})
$jobs|Wait-Job|Receive-Job
foreach($job in $jobs){if($job.State -ne 'Completed' -or $job.ChildJobs[0].Error.Count){throw 'Area comparison failed'}}
$jobs|Remove-Job
if(!$SkipRendered){& $runner -Map B -OutputName "$Prefix-rendered-B" -Experiment -Area -SightRange 10000 -WeaponRange 5000 -Rendered -FrozenBuild $snapshot -RoundTripSave}
