[CmdletBinding()]
# Map statistics (BattleMapStats.cpp): sight lengths, clearance, cover density and routes of a wide map as built.
# No match is played. Output: Setup/Benchmarks/MapStats/<OutputName>/stats.json (+ run.log, manifest.json, result.json).
#   .\Setup\Test-MapStats.ps1 -Map E -OutputName mapstats-E-r1
param([ValidatePattern('^[A-Z]$')][string]$Map='E',[Parameter(Mandatory)][string]$OutputName)
$ErrorActionPreference='Stop'
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output name required'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project=Join-Path $root 'Game/FutureBattle/FutureBattle.uproject'
$mapFile=Join-Path $root "Game/FutureBattle/Content/Prototype/Lvl_BattleWide_$Map.umap"
if(!(Test-Path $mapFile)){throw "Map not found: $mapFile"}
$out=Join-Path $PSScriptRoot "Benchmarks/MapStats/$OutputName"
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out|Out-Null
$dll=Join-Path $root 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log';$stats=Join-Path $out 'stats.json'
$arguments=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-nullrhi','-BattleWideMixed','-BattleArea',('-BattleMapStats="'+$stats+'"'),('-abslog="'+$log+'"'))
@{machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;arguments=$arguments;map=(Get-FileHash $mapFile).Hash}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(300000)){Stop-Process -Id $p.Id;throw "Timed out: $log"}
$t=Get-Content $log -Raw
$pass=$p.ExitCode -eq 0 -and $t -match 'BATTLE_MAP_STATS_RESULT failures=0' -and $t -notmatch 'Fatal error:|Assertion failed:' -and (Test-Path $stats)
@{passed=$pass;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash}|ConvertTo-Json|Set-Content (Join-Path $out 'result.json')
$t -split "`n"|Where-Object{$_ -match 'BATTLE_MAP_STATS'}|Write-Output
if(!$pass){throw "Map statistics failed: $log"}
Write-Output $stats
