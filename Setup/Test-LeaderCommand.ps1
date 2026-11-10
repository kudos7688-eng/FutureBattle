[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputFolder,[switch]$UI,[ValidateSet(500,1000)][int]$Units=1000,[int]$Width=1920,[int]$Height=1080)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$out=[IO.Path]::GetFullPath($OutputFolder)
if(Test-Path (Join-Path $out 'manifest.json')) { throw 'Use a fresh output folder' }
if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { throw 'An Unreal process is already running' }
[IO.Directory]::CreateDirectory($out)|Out-Null
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$m=@{test='Leader 2A+2B functional verification';performance_measurement=$false;machine=(Get-MeasurementMachine);dll_sha256=(Get-FileHash $dll).Hash;created=(Get-Date -Format o);ui=[bool]$UI;status='running'}
$log=Join-Path $out 'run.log'
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',('-abslog="'+$log+'"'))
if($UI) { $args+=@('-RenderOffscreen',"-ResX=$Width","-ResY=$Height",'-Windowed','-ForceRes',"-BattleLeaderUnits=$Units",'-BattleLeaderNoControl=1',('-BattleLeaderQA="'+(Join-Path $out 'list')+'"')) }
else { $args+=@('-nullrhi','-BattleLeaderSmoke') }
$m.arguments=$args; $m|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json')
$proc=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $args -WindowStyle Hidden -PassThru
if(!$proc.WaitForExit(180000)) { Stop-Process -Id $proc.Id; throw "Timed out: $log" }
$text=Get-Content $log -Raw
$pattern=if($UI){'BATTLE_LEADER_UI_RESULT failures=0'}else{'BATTLE_LEADER_RESULT failures=0'}
$ok=$text -match $pattern -and $text -notmatch 'BATTLE_LEADER_(TEST|UI) FAIL|Fatal error:|Assertion failed:'
if($UI) { foreach($name in 'top','bottom','stopped') { $ok=$ok -and (Test-Path (Join-Path $out "list-$name.png")) } }
$m.status=if($ok){'passed'}else{'failed'}; $m|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json')
$text -split "`n" | Where-Object {$_ -match 'BATTLE_LEADER_(TEST|UI|RESULT)'} | Write-Output
if(!$ok) { throw "Leader verification failed: $log" }
