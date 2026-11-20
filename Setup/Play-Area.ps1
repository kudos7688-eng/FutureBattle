[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I')][string]$Map='A',[switch]$SectorAI,[switch]$SectorTrials,[switch]$TestAfterPlay)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$extra = @()
if ($Map -eq 'H') {
    & (Join-Path $PSScriptRoot 'Initialize-MapH.ps1') -Project $project
}
# Maps with sectors (H, I): the same play condition.
if ($Map -eq 'H' -or $Map -eq 'I') {
    $extra += '-BattleLeaderGuard=1'
    # Player's move click = sector move that fires on the move; a drawn line = point move (BattlePlayerPath.cpp).
    $extra += '-BattleSectorMove=1'
    if ($SectorAI) { $extra += '-BattleSectorAI=1' }
    # Sector AI trials (default off in matches): fire back when shot, sector route, flank and front stages, pass a crowded corner,
    # fire on the move until inside the sector, gather behind cover, and stop in cover only when half the squad can hide.
    if ($SectorTrials) { $extra += @('-BattleSectorContact=1','-BattleSectorRoute=1','-BattleSectorFlank=1','-BattleSectorFront=1','-BattleWaypointPass=1','-BattleSectorMoveFire=1','-BattleCoverHuddle=1','-BattleSquadCoverCheck=1') }
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
