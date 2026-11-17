[CmdletBinding()]
# Stance visuals check (2026-11-17): close-up screenshots of a leader and a Mass follower standing, crouched and prone on
# map E (stances on by default there). -Anim adds -BattleStanceAnim=1 (Lyra rifle crouch clips); without it the
# placeholder (squashed body) is shown.
param([string]$Name='stance-r1',[switch]$Anim,[ValidateSet('A','D','E')][string]$Map='E')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/StanceAnim-20261117/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleStanceShot','-BattleStances=1','-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'shot')+'"'),('-abslog="'+$log+'"'))
if($Anim){$a+='-BattleStanceAnim=1'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$a}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(240000)){Stop-Process -Id $p.Id;throw 'Stance shot timeout'}
$t=Get-Content $log -Raw
$ok=$p.ExitCode -eq 0 -and $t -match 'STANCE_SHOT_RESULT failures=0' -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n"|Where-Object{$_ -match 'STANCE_SHOT|BATTLE_STANCE_ANIM'}
if(!$ok){throw "Stance shot failed: $log"}
