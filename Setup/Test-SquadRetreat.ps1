[CmdletBinding()]
param(
 [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',
 [string]$OutputName='headless',
 [switch]$Rendered,
 [switch]$Soak,
 [ValidateSet(0,1)][int]$Enabled=1,
 [int]$Width=1280,[int]$Height=720
)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'OutputName must be a simple identifier'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$out=Join-Path $PSScriptRoot ('Benchmarks/SquadRetreat-ST2-20261111/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out -Force | Out-Null
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;rendered=[bool]$Rendered;soak=[bool]$Soak;width=$Width;height=$Height;enabled=$Enabled} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$log=Join-Path $out 'run.log'
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1','-BattleWideMixed','-BattleLeaderUnits=500',"-BattleSquadST2=$Enabled",'-BattleTacticalObserve',('-abslog="'+$log+'"'))
$arguments+=if($Soak){'-BattleWideSoak'}else{'-BattleST2Smoke'}
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
$p=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(260000)){Stop-Process -Id $p.Id;throw "Timed out: $log"}
$text=Get-Content $log -Raw
$pass=$p.ExitCode -eq 0 -and $text -match 'BATTLE_(ST2|WIDE_SOAK)_RESULT failures=0' -and $text -notmatch 'BATTLE_ST2_TEST FAIL|Fatal error:|Assertion failed:'
$result=@{passed=$pass;exitCode=$p.ExitCode;checks=([regex]::Matches($text,'BATTLE_ST2_TEST PASS')).Count;dll=(Get-FileHash $dll).Hash}
$result | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$text -split "`n" | Where-Object {$_ -match 'BATTLE_ST2_(TEST|RESULT)|BATTLE_WIDE_SOAK_RESULT'} | Write-Output
if(!$pass){throw "Test failed: $log"}
if($Rendered -and ($Soak -or $Enabled -eq 1)){$view=if($Soak){'result'}else{'retreat'};if(!(Test-Path (Join-Path $out "screen-$view.png"))){throw "Missing $view screenshot"}}
Write-Output "Evidence: $out"
