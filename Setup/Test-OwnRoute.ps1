[CmdletBinding()]
param(
 [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',
 [string]$OutputName='headless',
 [ValidateSet('E','K','I','J')][string]$Map='K',
 # 2 = B05 (own routes survive squad replans). 0 shows the defect: the "survive a squad replan" checks fail.
 [ValidateSet(0,1,2,3)][int]$CutOffRoute=2,
 [ValidateSet(0,1)][int]$MoveProgress=0,
 [int]$Fps=30
)
# B05 fixed scene (BattleOwnRouteSmoke.cpp): one squad split by a wall, a plain move and a supply trip.
# Pass = BATTLE_OWN_ROUTE_SMOKE_RESULT failures=0. Hold the measurement lock (Setup\Lock-Queue.ps1) before running.
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'OutputName must be a simple identifier'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$out=Join-Path $PSScriptRoot ('Benchmarks/OwnRoute-20261125/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out -Force | Out-Null
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$arguments=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleRangeAware=1','-BattleSightRange=5000',
 ('-abslog="'+$log+'"'),'-BattleSectorAI=1','-BattleSquadFlank=1','-BattleCoverActions=1','-BattleFlankCoordination=1','-BattleAICoverProbe=0','-BattleCoverApproach=1','-BattleLeaderGuard=1',
 '-BattleTacticalObserve','-BattleArea',('-BattleSightExperiment="'+(Join-Path $out 'metrics.json')+'"'),'-BattleTrialSeed=7','-UseFixedTimeStep',"-FPS=$Fps",'-nullrhi',
 "-BattleCutOffRoute=$CutOffRoute","-BattleMoveProgress=$MoveProgress",'-BattleOwnRouteSmoke')
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;map=$Map;cutOffRoute=$CutOffRoute;fps=$Fps;arguments=$arguments} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(600000)){Stop-Process -Id $p.Id;throw "Timed out: $log"}
$text=Get-Content $log -Raw
$pass=$p.ExitCode -eq 0 -and $text -match 'BATTLE_OWN_ROUTE_SMOKE_RESULT failures=0' -and $text -notmatch 'BATTLE_OWN_ROUTE_SMOKE FAIL|Fatal error:|Assertion failed:'
@{passed=$pass;exitCode=$p.ExitCode;checks=([regex]::Matches($text,'BATTLE_OWN_ROUTE_SMOKE PASS')).Count;fails=([regex]::Matches($text,'BATTLE_OWN_ROUTE_SMOKE FAIL')).Count;dll=(Get-FileHash $dll).Hash} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$text -split "`n" | Where-Object {$_ -match 'BATTLE_OWN_ROUTE_SMOKE'} | Write-Output
if(!$pass){throw "Test failed: $log"}
Write-Output "Evidence: $out"
