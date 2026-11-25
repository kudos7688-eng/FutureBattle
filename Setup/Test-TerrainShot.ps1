[CmdletBinding()]
# Terrain look and height check (2026-11-25, map L, Docs/design-terrain-field.md): a rendered area match with the Play-Area conditions
# and -BattleHeightTest (three squads of team 0 walk up the west knoll, onto an upper floor and onto the west ridge; every soldier of
# the team must keep to the ground). -BattleTerrainShot takes seven pictures (the map from above, the knoll, the ridge, the river
# valley, the first squad on its way three times) and quits after the height test has reported (about 300 s of match time, fixed
# 30 FPS steps). Passes when BATTLE_HEIGHT_RESULT failures=0 and all seven pictures are there.
param([string]$Name='terrain-shot-r1',[ValidateSet('L','G','I')][string]$Map='L')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/TerrainShot-20261125/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$log=Join-Path $out 'run.log'
$shot=Join-Path $out 'shot'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',
 '-BattleWideMixed','-BattleArea','-BattleCoverActions=1','-BattleFlankCoordination=1','-BattleAreaPointSupply=1','-BattleFollowerAnimSkip=1',
 '-BattleLeaderGuard=1','-BattleSectorMove=1','-BattleGroupFireSupport=1','-BattleSectorAI=1','-BattleStartRunning','-UseFixedTimeStep','-FPS=30',
 '-BattleHeightTest','-BattleTerrainShot',('-BattleWideScreens="'+$shot+'"'),('-abslog="'+$log+'"'))
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(1200000)){Stop-Process -Id $p.Id;throw 'Terrain shot timeout'}
$t=Get-Content $log -Raw
$lines=@($t -split "`n"|Where-Object{$_ -match 'BATTLE_HEIGHT_TEST|BATTLE_HEIGHT_RESULT|TERRAIN_SHOT'})
$pictures=@(Get-ChildItem $out -Filter 'shot-*.png' -File)
$ok=($t -match 'BATTLE_HEIGHT_RESULT failures=0') -and $pictures.Count -eq 7 -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode;pictures=$pictures.Count}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$lines
if(!$ok){throw "Terrain shot failed: $log"}
Write-Output "Terrain shot passed: $out"
