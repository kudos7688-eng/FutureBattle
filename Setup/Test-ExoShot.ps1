[CmdletBinding()]
# Exoskeleton look check (2026-11-18): close-up screenshots of one exoskeleton soldier per team (front, side, back, bazooka in hand front and back, shield gone,
# crouched, prone three times (the first may still show the get-down clip), the other team's). Rendered off screen; look at the PNG files afterwards (a generated file is not a visual check).
param([string]$Name='exo-r1',[ValidateSet('A','D','E')][string]$Map='E')
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/ExoShot-20261118/$Name"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleArea','-BattleExoShot','-BattleStances=1','-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'shot')+'"'),('-abslog="'+$log+'"'))
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$a}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(240000)){Stop-Process -Id $p.Id;throw 'Exoskeleton shot timeout'}
$t=Get-Content $log -Raw
$ok=$p.ExitCode -eq 0 -and $t -match 'EXO_SHOT_RESULT failures=0' -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n"|Where-Object{$_ -match 'EXO_SHOT'}
if(!$ok){throw "Exoskeleton shot failed: $log"}
