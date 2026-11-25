[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I','J','K')][string]$Map='A',[switch]$SectorAI,[switch]$SectorTrials,[switch]$TestAfterPlay,[ValidateRange(0,4)][int]$Strategy=0,[switch]$Setup,[ValidateSet(0,30,50)][int]$CloseRange=0,[ValidateSet(0,1,2)][int]$OwnTarget=0,[ValidateSet(0,1,2)][int]$OwnTargetCover=0,[ValidateSet(-1,0,1)][int]$StanceSteady=-1,[ValidateSet(0,1)][int]$ArrivalReady=0,[ValidateSet(0,1,2)][int]$WalkLook=0,[switch]$FrontPlanPlayer,[switch]$SectionOrders,[ValidateSet(0,1)][int]$VehicleRoadGoal=0,[switch]$Arms,[switch]$SquadKnowledge)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$extra = @()
# Strategy layer (Docs/design-strategy-layer.md; trial, default 0): both AI teams, and the squads the player delegates (J), plan by
# the situation map; the top bar shows the own plan. 2 = recon -> focus attack, 3 = + hold and counter, 4 = + purchases for the focus.
if ($Strategy -gt 0) { $extra += "-BattleStrategy=$Strategy" }
# Trial (2026-11-23, default 0): a vehicle ordered into a place it cannot enter (a walled sector of map K) goes to the nearest road instead of refusing the order.
if ($VehicleRoadGoal -gt 0) { $extra += "-BattleVehicleRoadGoal=$VehicleRoadGoal" }
# Setup before the start (user decision 2026-11-22, trial): 15 rifle squads instead of 25 and 250 resources instead of 100 (the 10 squads'
# worth plus the usual 100); since 2026-11-24 the match starts running (P pauses it to show the setup panel). Purchases come before both teams' orders.
if ($Setup) { $extra += @('-BattleSetupPhase=1','-BattleStartSquads=15','-BattleAreaStartCredits=250') }
# Close fights to look at (ROADMAP 2026-11-22, stage 1 of fights inside a sector): sight and engagement cut to 30 or 50 m, so squads
# meet inside one sector. For watching the current AI at close range, not a play condition.
if ($CloseRange -gt 0) { $cm = $CloseRange * 100; $extra += @("-BattleRangeSight=$cm","-BattleRangeEngage=$cm","-BattleRangeEngageSupport=$cm") }
# Individual AI trials (default 0, ROADMAP 2026-11-22 stage 2): -OwnTarget 2 = a soldier who cannot fire at the squad's target fires at
# an enemy he sees himself and turns toward where he was hit from; -WalkLook 1 = a soldier on the move turns his head to the enemy he
# knows of or to his share of the ground around the way. Both teams.
if ($OwnTarget -gt 0) { $extra += "-BattleOwnTarget=$OwnTarget" }
# -OwnTargetCover 1/2 (with -OwnTarget): he takes cover near by against the enemy he fires back at (2: also when walking, within 50 m).
if ($OwnTargetCover -gt 0) { $extra += "-BattleOwnTargetCover=$OwnTargetCover" }
# -StanceSteady: fewer crouch/stand changes on the spot (stays standing where crouching hid his enemy; 3 s down after an empty peek).
# Default 1 in the game since 2026-11-24; -StanceSteady 0 restores the old behaviour.
if ($StanceSteady -ge 0) { $extra += "-BattleStanceSteady=$StanceSteady" }
# -ArrivalReady 1: a soldier at his arrival place while his squad is still on its way crouches instead of standing.
if ($ArrivalReady -gt 0) { $extra += "-BattleArrivalReady=$ArrivalReady" }
if ($WalkLook -gt 0) { $extra += "-BattleWalkLook=$WalkLook" }
# Squad knowledge stage 2 (Docs/design-squad-knowledge.md; trial, default 0): both teams keep what they know per squad.
if ($SquadKnowledge) { $extra += '-BattleSquadKnowledge=1' }
# Front plan stage 7 (Docs/design-front-strategy.md 6; trials, default 0): the player's team plans by sections of its front line; U opens the
# plan panel (section roles, purchase reservations, hiring delegation), the top bar shows the own plan. Team 0 only; second focus allowed.
if ($SectionOrders) { $FrontPlanPlayer=$true; $extra += @('-BattleFrontSectionOrders=1') }
if ($FrontPlanPlayer) { $extra += @('-BattleFrontPlan=1','-BattleFrontPlanTeam=0','-BattleFrontPlanPlayer=1','-BattleFrontPlanFocus2=1','-BattleHireReserve=1') }
if ($Map -eq 'H') {
    & (Join-Path $PSScriptRoot 'Initialize-MapH.ps1') -Project $project
}
if ($Map -eq 'J') {
    & (Join-Path $PSScriptRoot 'Initialize-MapJ.ps1') -Project $project
}
if ($Map -eq 'K') {
    & (Join-Path $PSScriptRoot 'Initialize-MapK.ps1') -Project $project
}
# Maps with sectors (H, I): the same play condition.
if ($Map -eq 'H' -or $Map -eq 'I' -or $Map -eq 'J' -or $Map -eq 'K') {
    $extra += '-BattleLeaderGuard=1'
    # Player's move click = sector move that fires on the move; a drawn line = point move (BattlePlayerPath.cpp).
    $extra += '-BattleSectorMove=1'
    # Grouped mortars and guns support the group's squads in trouble (trial, groupui play 2026-11-21).
    $extra += '-BattleGroupFireSupport=1'
    # Free squads the front army could take wait two or three sectors behind its line instead of going to and fro (trial, user 2026-11-22).
    $extra += '-BattleFrontReserve=1'
    # Infantry without anti-tank weapons that fell back from a tank waits there instead of walking back into it (trial, user 2026-11-23).
    $extra += '-BattleArmourWait=1'
    # A group gives its squads one sector each around its rally and its point, from the side first (trial, user 2026-11-23).
    $extra += '-BattleGroupSectors=1'
    # AI orders as sector missions (move by sectors, fire on the move into the sector), as Test-WideSight.ps1 measures them by default
    # (-SectorAI 1). Before 2026-11-24 the play passed it only with -SectorAI, so the play's AI differed from the measured one (user decision).
    $extra += '-BattleSectorAI=1'
    # Sector AI trials (default off in matches): fire back when shot, sector route, flank and front stages, pass a crowded corner,
    # fire on the move until inside the sector (no stop: -BattleSquadCoverCheck and -BattleBounding left out, user 11-20), line up
    # along cover walls, flank when pinned, face the known enemy on arrival, march through cover, cover must hide 3 of 5 body
    # samples and no standing in the open, soldiers do not block each other's sight, cover faces where shots came from (user 11-20).
    if ($SectorTrials) { $extra += @('-BattleSectorContact=1','-BattleSectorRoute=1','-BattleSectorFlank=1','-BattleSectorFront=1','-BattleWaypointPass=1','-BattleSectorMoveFire=1','-BattleCoverHuddle=1','-BattlePinnedFlank=1','-BattleArrivalThreat=1','-BattleSectorCoverRoute=1','-BattleCoverStrict=1','-BattleSightTeamMask=1','-BattleShotDirection=1','-BattleDangerRoute=1','-BattleCloseIn=1','-BattleSectorCoverWalk=1','-BattleUnderFireCover=1','-BattleNoCoverFallback=1','-BattleCoverPriority=1') }
}
# Infantry arms (Docs/design-infantry-arms.md): on by default since 2026-11-24 (user decision) - pistols, 2 hand grenades, melee,
# elite platoons and sniper teams, AI assault bounds and charges, sector-task bounds, rush watch, covering fire. -Arms is kept
# for old shortcuts and adds nothing (Q grenade, V strike, F pistol in direct control).
# Player matches start with the AI running (user request 2026-11-23); tests leave this out and start paused. -Setup starts running too (user 2026-11-24).
$extra += '-BattleStartRunning'
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
