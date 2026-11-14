[CmdletBinding()]
param([ValidateSet(0,1)][int]$Flank=0,[ValidateSet(-1,1)][int]$Sign=1,[int]$Seed=7,[string]$Name='bay',[switch]$Rendered)
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Simple name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out=Join-Path $PSScriptRoot "Benchmarks/FlankStages-20261112/$Name"
if(Test-Path $out){throw 'Output exists'}
New-Item -ItemType Directory -Path $out|Out-Null
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_C','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleFlankBayTrial','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0','-BattleSightRange=10000','-BattleWeaponRange=5000',"-BayFlank=$Flank","-BaySign=$Sign","-BattleTrialSeed=$Seed",'-UseFixedTimeStep','-FPS=60',('-BayOutput="'+(Join-Path $out 'metrics.json')+'"'),('-abslog="'+$log+'"'))
if($Rendered){$a+=@('-RenderOffscreen','-BayRendered','-ForceRes','-ResX=1280','-ResY=720')}else{$a+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;map=(Get-FileHash (Join-Path $root 'Game/FutureBattle/Content/Prototype/Lvl_BattleWide_C.umap')).Hash;arguments=$a}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id;throw 'Bay timeout'}
$t=Get-Content $log -Raw
$ok=$p.ExitCode -eq 0 -and $t -match 'BATTLE_BAY_RESULT failures=0' -and $t -notmatch 'Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
if(!$ok){throw "Bay failed: $log"}
Get-Content (Join-Path $out 'metrics.json')
