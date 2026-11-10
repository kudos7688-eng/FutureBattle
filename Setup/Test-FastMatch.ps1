[CmdletBinding()]
param([ValidateRange(0,2)][int]$Scenario = 0, [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$folder = Join-Path $PSScriptRoot 'MatchRuns'
[IO.Directory]::CreateDirectory($folder) | Out-Null
$report = Join-Path $folder "$stamp-scenario$Scenario.json"
$log = Join-Path $PSScriptRoot "Battle-Match-$stamp.log"
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-nullrhi', '-unattended', '-nosound', '-nosplash', '-BattleFastMatch', '-BattleMatchSoak', "-BattleMatchScenario=$Scenario", ('-BattleMatchReport="' + $report + '"'), ('-abslog="' + $log + '"'))
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(240000)) { Stop-Process -Id $process.Id; throw "Match run timed out: $log" }
$content = Get-Content -LiteralPath $log -Raw
if ($content -notmatch 'BATTLE_MATCH_RUN success=1' -or $content -match 'Fatal error:|Assertion failed:') { throw "Match run failed: $log" }
Get-Content -LiteralPath $report
Write-Output "Report: $report"
