[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $PSScriptRoot ('Battle-Smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
if (!(Test-Path -LiteralPath $editor)) { throw "Missing editor: $editor" }
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-nullrhi', '-unattended', '-nosound', '-nosplash', '-BattleSmokeTest', ('-abslog="' + $log + '"'))
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id
    throw "Battle smoke test timed out. See $log"
}
$text = Get-Content -LiteralPath $log -Raw
$text -split "`n" | Where-Object { $_ -match 'BATTLE_(TEST|SMOKE_RESULT)' } | Write-Output
if ($text -notmatch 'BATTLE_SMOKE_RESULT failures=0' -or $text -match 'BATTLE_TEST FAIL|Fatal error:|Assertion failed:') {
    throw "Battle smoke test failed. See $log"
}
Write-Output "Battle smoke test passed. Log: $log"
