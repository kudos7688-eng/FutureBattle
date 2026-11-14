[CmdletBinding()]
param(
 [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',
 [string]$OutputName='wide-headless',
 [switch]$Rendered,
 [switch]$LegacyLeader,
 [switch]$Soak,
 [ValidateSet(0,1)][int]$DisplayLOD=1,
 [int]$Width=1280,[int]$Height=720
)
$ErrorActionPreference='Stop'
if($Soak -and $LegacyLeader){throw 'Soak is only supported for the mixed wide field'}
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'OutputName must be a simple identifier'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$out=Join-Path $PSScriptRoot ('Benchmarks/WideMixed-20261111/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out -Force | Out-Null
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;rendered=[bool]$Rendered;legacyLeader=[bool]$LegacyLeader;soak=[bool]$Soak;width=$Width;height=$Height;displayLOD=$DisplayLOD} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$map=if($LegacyLeader){'/Game/Prototype/Lvl_BattlePrototype'}else{'/Game/Prototype/Lvl_BattleField500'}
$log=Join-Path $out 'run.log'
$args=@(('"'+$project+'"'),$map,'-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
if($LegacyLeader){$args+=@('-BattleLeaderUnits=40','-BattleLeaderSmoke')}else{$args+=@('-BattleWideMixed','-BattleLeaderUnits=500');$args+=if($Soak){'-BattleWideSoak'}else{'-BattleWideSmoke'}}
if($Rendered){$args+=@('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$args+='-nullrhi'}
$p=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $args -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(240000)){Stop-Process -Id $p.Id;throw "Timed out: $log"}
$text=Get-Content $log -Raw
$pass=$p.ExitCode -eq 0 -and $text -match 'BATTLE_(WIDE|WIDE_SOAK|LEADER)_RESULT failures=0' -and $text -notmatch 'BATTLE_(WIDE|LEADER)_TEST FAIL|Fatal error:|Assertion failed:'
@{passed=$pass;exitCode=$p.ExitCode;checks=([regex]::Matches($text,'BATTLE_(WIDE|LEADER)_TEST PASS')).Count;dll=(Get-FileHash $dll).Hash} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$text -split "`n" | Where-Object {$_ -match 'BATTLE_(WIDE|LEADER)_(TEST|RESULT|VAT|SOAK_RESULT)'} | Write-Output
if(!$pass){throw "Test failed: $log"}
if($Rendered -and !$LegacyLeader){$views=if($Soak){@('result')}else{@('rts','tps')};foreach($view in $views){if(!(Test-Path (Join-Path $out "screen-$view.png"))){throw "Missing $view screenshot"}}}
Write-Output "Evidence: $out"
