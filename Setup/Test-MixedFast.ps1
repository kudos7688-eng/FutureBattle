[CmdletBinding()]
param([string]$OutputName='pilot',[switch]$Rendered,[ValidateSet(0,1)][int]$DisplayLOD=1)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$out=Join-Path $PSScriptRoot "Benchmarks/MixedFast-20261112/$OutputName"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleMixedFastMatch','-BattleMixedFastSmoke',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$arguments;map=(Get-FileHash (Join-Path $root 'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')).Hash} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id;throw "Timed out: $log"}
$t=Get-Content $log -Raw
$pass=$p.ExitCode -eq 0 -and $t -match 'BATTLE_MIXED_FAST_RESULT failures=0' -and $t -notmatch 'BATTLE_MIXED_FAST_CHECK FAIL|Fatal error:|Assertion failed:'
@{passed=$pass;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash;checks=([regex]::Matches($t,'BATTLE_MIXED_FAST_CHECK PASS')).Count}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json')
$t -split "`n"|Where-Object{$_ -match 'BATTLE_MIXED_FAST'}|Write-Output
if(!$pass){throw "Test failed: $log"}
if($Rendered -and !(Test-Path (Join-Path $out 'screen-restored.png'))){throw 'Missing restored screen'}

