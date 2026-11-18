[CmdletBinding()]
# Aim / scope view check (2026-11-18): leader 1 under direct control, photographed with the HUD in each view
# (rear camera, aim x1.6, scope x4, scope x8, released). Passes on SCOPE_SHOT_RESULT failures=0.
param([string]$Name='scope-r1',[ValidateSet('A','D','E','F')][string]$Map='E')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/ScopeShot-20261118/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleScopeShot','-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'shot')+'"'),('-abslog="'+$log+'"'))
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$a}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(240000)){Stop-Process -Id $p.Id;throw 'Scope shot timeout'}
$t=Get-Content $log -Raw
$ok=$p.ExitCode -eq 0 -and $t -match 'SCOPE_SHOT_RESULT failures=0' -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n"|Where-Object{$_ -match 'SCOPE_SHOT|BATTLE_MUZZLE_FLASH'}
if(!$ok){throw "Scope shot failed: $log"}
