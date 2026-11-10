[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$Rendered)
$ErrorActionPreference = 'Stop'
$Units = 500
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $PSScriptRoot ('Battle-LargeField-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + $Units + '.log')
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattleField500', '-game', '-unattended', '-nosound', '-nosplash', "-BattleScaleUnits=$Units", '-BattleLargeFieldTest', ('-abslog="' + $log + '"'))
if ($Rendered) {
    $folder = Join-Path $PSScriptRoot 'ScaleRuns'
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    $screenshot = Join-Path $folder ([IO.Path]::GetFileNameWithoutExtension($log))
    $arguments += @('-RenderOffscreen', '-ResX=1280', '-ResY=720', ('-BattleScaleScreenshot="' + $screenshot + '"'))
} else { $arguments += '-nullrhi' }
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(180000)) { Stop-Process -Id $process.Id; throw "Scale test timed out: $log" }
$text = Get-Content -LiteralPath $log -Raw
$text -split "`n" | Where-Object { $_ -match 'BATTLE_(TEST|LARGE_FIELD_RESULT)|LARGE_FIELD' } | Write-Output
if ($process.ExitCode -ne 0 -or $text -notmatch 'BATTLE_LARGE_FIELD_RESULT failures=0' -or $text -match 'BATTLE_TEST FAIL|Fatal error:|Assertion failed:') { throw "Scale test failed: $log" }
if ($Rendered -and (!(Test-Path -LiteralPath ($screenshot + '-fps.png')) -or !(Test-Path -LiteralPath ($screenshot + '-overview.png')))) { throw "Scale screenshots missing: $screenshot" }
if ($Rendered -and !(Test-Path -LiteralPath ($screenshot + '-combat.png'))) { throw 'Combat screenshot missing' }
Write-Output "Large field test passed: $log"



