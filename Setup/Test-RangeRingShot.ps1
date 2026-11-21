[CmdletBinding()]
# Range ring display check (2026-11-21): a rendered area match with the Play-Area conditions; at 40 s an own squad is selected
# (its rings), from 90 s, once an enemy is seen (240 s at the latest), the own squad nearest a seen enemy with every seen enemy's rings (as with L). Two screenshots, then quit.
# Passes when both BATTLE_RANGE_RING_SHOT lines and both pictures are there.
param([string]$Name='rings-r1',[ValidateSet('A','E','F','G','H','I')][string]$Map='I')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/RangeRingShot-20261121/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$log=Join-Path $out 'run.log'
$shot=Join-Path $out 'shot'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',
 '-BattleWideMixed','-BattleArea','-BattleCoverActions=1','-BattleFlankCoordination=1','-BattleAreaPointSupply=1','-BattleFollowerAnimSkip=1',
 ('-BattleRangeRingShot="'+$shot+'"'),('-abslog="'+$log+'"'))
if($Map -eq 'H' -or $Map -eq 'I'){$a+=@('-BattleLeaderGuard=1','-BattleSectorMove=1','-BattleGroupFireSupport=1')}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(480000)){Stop-Process -Id $p.Id;throw 'Range ring shot timeout'}
$t=Get-Content $log -Raw
$lines=@($t -split "`n"|Where-Object{$_ -match 'BATTLE_RANGE_RING_SHOT'})
$ok=$lines.Count -eq 2 -and (Test-Path "$shot-40s.png") -and (Test-Path "$shot-enemy.png") -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$lines
if(!$ok){throw "Range ring shot failed: $log"}
Write-Output "Range ring shot passed: $out"
