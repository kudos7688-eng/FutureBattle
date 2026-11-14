[CmdletBinding()]
param([ValidateSet('Full','Performance')][string]$Phase='Full',[string]$Prefix='matrix-r1',[ValidateRange(1,4)][int]$Concurrency=1)
$ErrorActionPreference='Stop'
if($Prefix -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple prefix required'}
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$expected=(Get-FileHash $dll).Hash
$jobs=@()
if($Phase -eq 'Full'){
 foreach($map in @('A','B')){foreach($range in @(5000,7500,10000,15000)){foreach($seed in @(7,19,43)){foreach($swap in @($false,$true)){
  $jobs+=@{Map=$map;SightRange=$range;Seed=$seed;Swap=$swap;Rendered=$false;OutputName=('{0}-{1}-{2}-s{3}-{4}' -f $Prefix,$map,$range,$seed,[int]$swap)}
 }}}}
}else{
 foreach($range in @(5000,10000,15000)){foreach($rep in 1..3){$jobs+=@{Map='B';SightRange=$range;Seed=7;Swap=$false;Rendered=$true;OutputName=('{0}-B-{1}-r{2}' -f $Prefix,$range,$rep)}}}
}
$completed=0
if($Concurrency -gt 1){
 if($Phase -ne 'Full'){throw 'Only headless behavior trials may run concurrently'}
 $runner=Join-Path $PSScriptRoot 'Test-WideSight.ps1'
 $jobs | ForEach-Object -Parallel {
  $ErrorActionPreference='Stop'
  $job=$_
  if((Get-FileHash $using:dll).Hash -ne $using:expected){throw 'DLL changed during matrix'}
  & $using:runner @job -Experiment -BehaviorConcurrency $using:Concurrency
 } -ThrottleLimit $Concurrency
 foreach($job in $jobs){$result=Join-Path $PSScriptRoot ('Benchmarks/SightTrial-20261111/'+$job.OutputName+'/result.json');if(!(Test-Path $result) -or !(Get-Content $result -Raw | ConvertFrom-Json).passed){throw "Missing/failed result: $result"}}
 Write-Output "SIGHT_MATRIX_COMPLETE phase=$Phase runs=$($jobs.Count) concurrency=$Concurrency timings_not_comparable=true dll=$expected"
 return
}
foreach($job in $jobs){
 if((Get-FileHash $dll).Hash -ne $expected){throw 'DLL changed during matrix'}
 & (Join-Path $PSScriptRoot 'Test-WideSight.ps1') @job -Experiment
 ++$completed
 Write-Output "SIGHT_MATRIX_PROGRESS completed=$completed total=$($jobs.Count) last=$($job.OutputName)"
}
Write-Output "SIGHT_MATRIX_COMPLETE phase=$Phase runs=$completed dll=$expected"
