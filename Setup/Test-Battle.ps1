[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [ValidateSet('All', 'Tactical', 'Squad', 'Ability', 'Match', 'Corner', 'Weapons')][string]$Suite = 'All', [ValidateSet('Legacy','Grid','Even','Far')][string]$SenseMode = 'Legacy')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Game\FutureBattle\FutureBattle.uproject'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $PSScriptRoot ('Battle-Smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
if (!(Test-Path -LiteralPath $editor)) { throw "Missing editor: $editor" }
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-nullrhi', '-unattended', '-nosound', '-nosplash', '-BattleSmokeTest', ('-abslog="' + $log + '"'))
$arguments += "-BattleSenseMode=$SenseMode"
if ($Suite -eq "Weapons") {
    $prefix=[IO.Path]::ChangeExtension($log,$null).TrimEnd(".")
    $arguments=@($arguments | Where-Object { $_ -ne "-nullrhi" }) + @("-RenderOffscreen","-ResX=1280","-ResY=720",('-BattleWeaponPreview="'+$prefix+'"'))
}
if ($Suite -ne 'All') { $arguments += "-BattleSmokeSuite=$Suite" }
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(180000)) {
    Stop-Process -Id $process.Id
    throw "Battle smoke test timed out. See $log"
}
if (!(Test-Path -LiteralPath $log -PathType Leaf)) {
    throw "Battle smoke test produced no UE log (exit code $($process.ExitCode)). No test result is available. Expected: $log"
}
$text = Get-Content -LiteralPath $log -Raw
$text -split "`n" | Where-Object { $_ -match 'BATTLE_(TEST|CORNER|SMOKE_RESULT)' } | Write-Output
if ($text -notmatch 'BATTLE_SMOKE_RESULT failures=0' -or $text -match 'BATTLE_TEST FAIL|Fatal error:|Assertion failed:') {
    throw "Battle smoke test failed. See $log"
}
if ($Suite -eq "Weapons") { foreach ($view in "carbine","support","pair") { if (!(Test-Path "$prefix-$view.png")) { throw "Missing $view preview" } } }
Write-Output "Battle smoke test passed. Log: $log"
