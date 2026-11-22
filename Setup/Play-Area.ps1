[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I','J')][string]$Map='A',[switch]$SectorAI,[switch]$SectorTrials,[switch]$TestAfterPlay,[ValidateRange(0,4)][int]$Strategy=0,[switch]$Setup,[ValidateSet(0,30,50)][int]$CloseRange=0,[ValidateSet(0,1,2)][int]$OwnTarget=0,[ValidateSet(0,1,2)][int]$WalkLook=0)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$extra = @()
# Strategy layer (Docs/design-strategy-layer.md; trial, default 0): both AI teams, and the squads the player delegates (J), plan by
# the situation map; the top bar shows the own plan. 2 = recon -> focus attack, 3 = + hold and counter, 4 = + purchases for the focus.
if ($Strategy -gt 0) { $extra += "-BattleStrategy=$Strategy" }
# Setup before the start (user decision 2026-11-22, trial): 15 rifle squads instead of 25 and 250 resources instead of 100 (the 10 squads'
# worth plus the usual 100), the match waits paused for purchases and orders; P starts it. Purchases come before both teams' orders.
if ($Setup) { $extra += @('-BattleSetupPhase=1','-BattleStartSquads=15','-BattleAreaStartCredits=250') }
# Close fights to look at (ROADMAP 2026-11-22, stage 1 of fights inside a sector): sight and engagement cut to 30 or 50 m, so squads
# meet inside one sector. For watching the current AI at close range, not a play condition.
if ($CloseRange -gt 0) { $cm = $CloseRange * 100; $extra += @("-BattleRangeSight=$cm","-BattleRangeEngage=$cm","-BattleRangeEngageSupport=$cm") }
# Individual AI trials (default 0, ROADMAP 2026-11-22 stage 2): -OwnTarget 2 = a soldier who cannot fire at the squad's target fires at
# an enemy he sees himself and turns toward where he was hit from; -WalkLook 1 = a soldier on the move turns his head to the enemy he
# knows of or to his share of the ground around the way. Both teams.
if ($OwnTarget -gt 0) { $extra += "-BattleOwnTarget=$OwnTarget" }
if ($WalkLook -gt 0) { $extra += "-BattleWalkLook=$WalkLook" }
if ($Map -eq 'H') {
    & (Join-Path $PSScriptRoot 'Initialize-MapH.ps1') -Project $project
}
if ($Map -eq 'J') {
    & (Join-Path $PSScriptRoot 'Initialize-MapJ.ps1') -Project $project
}
# Maps with sectors (H, I): the same play condition.
if ($Map -eq 'H' -or $Map -eq 'I' -or $Map -eq 'J') {
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
