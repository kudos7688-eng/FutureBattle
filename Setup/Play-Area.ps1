[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I')][string]$Map='A',[switch]$SectorAI,[switch]$SectorTrials,[switch]$TestAfterPlay,[switch]$Arms)
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
    # Grouped mortars and guns support the group's squads in trouble (trial, groupui play 2026-11-21).
    $extra += '-BattleGroupFireSupport=1'
    if ($SectorAI) { $extra += '-BattleSectorAI=1' }
    # Sector AI trials (default off in matches): fire back when shot, sector route, flank and front stages, pass a crowded corner,
    # fire on the move until inside the sector (no stop: -BattleSquadCoverCheck and -BattleBounding left out, user 11-20), line up
    # along cover walls, flank when pinned, face the known enemy on arrival, march through cover, cover must hide 3 of 5 body
    # samples and no standing in the open, soldiers do not block each other's sight, cover faces where shots came from (user 11-20).
    if ($SectorTrials) { $extra += @('-BattleSectorContact=1','-BattleSectorRoute=1','-BattleSectorFlank=1','-BattleSectorFront=1','-BattleWaypointPass=1','-BattleSectorMoveFire=1','-BattleCoverHuddle=1','-BattlePinnedFlank=1','-BattleArrivalThreat=1','-BattleSectorCoverRoute=1','-BattleCoverStrict=1','-BattleSightTeamMask=1','-BattleShotDirection=1','-BattleDangerRoute=1','-BattleCloseIn=1','-BattleSectorCoverWalk=1','-BattleUnderFireCover=1','-BattleNoCoverFallback=1','-BattleCoverPriority=1') }
}
# Infantry arms trial (Docs/design-infantry-arms.md, default off): pistols, 2 hand grenades and melee for every foot soldier,
# elite platoons (= key) and sniper teams (] key) for the player, one of each kept by the AI (G grenade, V strike, F pistol in direct control).
if ($Arms) { $extra += @('-BattleSidearms=1','-BattleGrenades=2','-BattleMelee=1','-BattleElite=1','-BattleAreaEliteKeep=1','-BattleSniperTeam=1','-BattleAreaSniperKeep=1','-BattleArmsAssault=1') }
$editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$playArgs = @("/Game/Prototype/Lvl_BattleWide_$Map", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-BattleWideMixed', '-BattleArea', '-BattleCoverActions=1', '-BattleFlankCoordination=1', '-BattleAreaPointSupply=1', '-BattleFollowerAnimSkip=1') + $extra
if ($TestAfterPlay -and $Map -eq 'H') {
    $game = Start-Process $editor -ArgumentList (@('"' + $project + '"') + $playArgs) -PassThru
    $game.WaitForExit()
    if ($game.ExitCode -ne 0) { throw "Map H play exited with code $($game.ExitCode)" }
    & (Join-Path $PSScriptRoot 'Test-MapHAfterPlay.ps1')
} else {
    & $editor $project @playArgs
}
