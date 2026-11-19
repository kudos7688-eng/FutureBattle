[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H')][string]$Map='A',[switch]$SectorAI,[switch]$TestAfterPlay)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$extra = @()
if ($Map -eq 'H') {
    & (Join-Path $PSScriptRoot 'Initialize-MapH.ps1') -Project $project
    $extra += '-BattleLeaderGuard=1'
    if ($SectorAI) { $extra += '-BattleSectorAI=1' }
}
$editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$playArgs = @("/Game/Prototype/Lvl_BattleWide_$Map", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-BattleWideMixed', '-BattleArea', '-BattleCoverActions=1', '-BattleFlankCoordination=1', '-BattleAreaPointSupply=1') + $extra
if ($TestAfterPlay -and $Map -eq 'H') {
    $game = Start-Process $editor -ArgumentList (@('"' + $project + '"') + $playArgs) -PassThru
    $game.WaitForExit()
    if ($game.ExitCode -ne 0) { throw "Map H play exited with code $($game.ExitCode)" }
    & (Join-Path $PSScriptRoot 'Test-MapHAfterPlay.ps1')
} else {
    & $editor $project @playArgs
}
