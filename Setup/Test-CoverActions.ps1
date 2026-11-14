[CmdletBinding()]
param([string]$Name='cover-r1',[switch]$Rendered,[ValidateSet('C','D')][string]$Map='C')
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
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleCoverActionSmoke','-BattleCoverActions=1','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0','-BattleSightRange=10000',('-abslog="'+$log+'"'))
if($Rendered){$a+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-CoverScreenshot="'+(Join-Path $out 'cover.png')+'"'))}else{$a+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$a}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id;throw 'Cover timeout'}
$t=Get-Content $log -Raw
$ok=$p.ExitCode -eq 0 -and $t -match 'BATTLE_COVER_RESULT failures=0' -and $t -notmatch 'BATTLE_COVER_CHECK FAIL|Fatal error:|Assertion failed:'
@{passed=$ok;exitCode=$p.ExitCode;checks=([regex]::Matches($t,'BATTLE_COVER_CHECK PASS')).Count}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n"|Where-Object{$_ -match 'BATTLE_COVER_(CHECK|RESULT|ACTION)'}
if(!$ok){throw "Cover failed: $log"}
