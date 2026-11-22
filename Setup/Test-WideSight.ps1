[CmdletBinding()]
param([ValidateSet('A','B','C','D','E','F','G','H','I','J')][string]$Map='A',[string]$OutputName='audit-A',[switch]$Rendered,[switch]$Experiment,[switch]$Visual,[switch]$FirstPerson,[switch]$GeometryOnly,[ValidateRange(0,10000)][int]$WeaponRange=0,[int]$SightRange=5000,[int]$Seed=7,[switch]$Swap,[ValidateRange(1,16)][int]$BehaviorConcurrency=1,[string]$FrozenBuild='',[ValidateSet(0,1)][int]$FlankEnabled=1,[switch]$ObserveTactics,[switch]$RoundTripSave,[switch]$Area,[ValidateSet(0,1)][int]$FlankRelaxed=0,[ValidateRange(0,3)][int]$FlankTeams=3,[switch]$FlankTrial,[ValidateSet(0,1)][int]$CoverActions=0,[ValidateSet(0,1)][int]$FlankCoordination=0,[ValidateSet(0,1)][int]$AreaCompositionPlanner=0,[ValidateRange(0,3)][int]$AreaCompositionPlannerTeams=0,[ValidateSet(0,1)][int]$AICoverProbe=0,[ValidateSet(0,1)][int]$CoverApproach=0,[ValidateSet(0,1)][int]$ContactCover=0,[ValidateSet(-1,0,1)][int]$ContactCoverTeam=-1,[ValidateSet(-1,0,1)][int]$TerrainLineBodies=-1,[ValidateSet(-1,0,1)][int]$ReacquireGrace=-1,[ValidateSet(-1,0,1)][int]$CoverFire=-1,[ValidateSet(-1,0,1)][int]$CoverValidation=-1,[ValidateSet(-1,0,1)][int]$CoverPeekSight=-1,[ValidateSet(0,1)][int]$PerceptionOpt=0,[ValidateSet(0,1)][int]$LeaderGuard=0,[ValidateRange(0,10)][int]$LeaderHealth=0,[int]$SupplyReturnLiving=-1,[int]$SupplyResumeLiving=-1,[double]$SupplyAwayRatio=-1,[double]$SupplyHeal=-1,[double]$SupplyResumeHealth=-1,[ValidateSet(-1,0,1)][int]$AreaHire=-1,[ValidateSet(-1,0,1)][int]$AreaIncome=-1,[int]$AreaHireReserve=-1,[double]$AreaHireInterval=-1,[double]$AreaHireStop=-1,[ValidateSet(-1,0,1)][int]$AreaStrategy=-1,[double]$AreaAttackWeight=-1,[double]$AreaBaseShare=-1,[ValidateSet(-1,0,1)][int]$AreaBaseRule=-1,[double]$AreaBaseShareLow=-1,[double]$AreaBaseShareHigh=-1,[ValidateSet(-1,0,1)][int]$AreaFrontBalance=-1,[double]$AreaFrontGain=-1,[double]$AreaBaseInfiltrate=-1,[double]$AreaFocus=-1,[double]$AreaBaseFlank=-1,[int]$AreaBaseFlankSide=-1,[int]$AreaBaseReturnLiving=-1,[int]$AreaTrialTeam=-1,[int]$AreaIncomeBase=-1,[int]$AreaIncomePerPoint=-1,[double]$AreaIncomeRamp=-1,[int]$AreaStartCredits=-1,[int]$AreaHirePurpose=-1,[int]$AreaPlan=-1,[double]$AreaHeavyShare=-1,[ValidateSet(-1,0,1)][int]$ReturnFire=-1,[int]$AreaIncomeUrbanBase=-1,[int]$AreaIncomeUrbanPerPoint=-1,[ValidateRange(10,120)][int]$Fps=60,[ValidateSet(-1,0,1)][int]$Suppression=-1,[double]$SuppressionPin=-1,[double]$SuppressionSpread=-1,[double]$SuppressionPerShot=-1,[double]$SuppressionSupport=-1,[ValidateSet(-1,0,1)][int]$AreaWarScore=-1,[int]$AreaWarTarget=-1,[ValidateSet(-1,0,1)][int]$RangeProfile=-1,[int]$RangeSight=-1,[int]$RangeEngage=-1,[int]$RangeEngageSupport=-1,[int]$RangeReach=-1,[ValidateSet(-1,0,1)][int]$Stances=-1,[ValidateSet(-1,0,1)][int]$CoverArrival=-1,[int]$HeavyMembers=-1,[ValidateRange(-1,10)][int]$RifleMembers=-1,[ValidateSet(-1,0,1)][int]$AllyPass=-1,[double]$HeavyHealth=-1,[double]$HeavySpeed=-1,[int]$HeavyCost=-1,[double]$HeavyDamage=-1,[int]$AreaScoutKeep=-1,[ValidateSet(-1,0,1)][int]$AreaIntel=-1,[ValidateSet(-1,0,1)][int]$ScoutRetreat=-1,[int]$AreaATKeep=-1,[int]$AreaTankKeep=-1,[int]$AreaACKeep=-1,[int]$AreaMortarKeep=-1,[int]$AreaSPGKeep=-1,[ValidateRange(-1,10)][int]$AreaExoKeep=-1,[ValidateSet(-1,0,1)][int]$ArmourAI=-1,[ValidateSet(-1,0,1)][int]$ArmourAITeam=-1,[ValidateSet(-1,0,1)][int]$AreaComposition=-1,[ValidateSet(-1,0,1)][int]$AreaGroups=-1,[ValidateSet(-1,0,1)][int]$DefendEngage=-1,[ValidateSet(-1,0,1)][int]$SymmetricStart=-1,[switch]$SymmetryAudit,[string]$Trace='',[ValidateSet(-1,0,1)][int]$SightPairReuse=-1,[double]$SightScanInterval=-1,[ValidateSet(-1,0,1)][int]$SightHitStats=-1,[ValidateSet(-1,0,1)][int]$SightParallel=-1,[ValidateSet(-1,0,1)][int]$SightSlowFar=-1,[ValidateSet(-1,0,1)][int]$SightTeamMask=-1,[ValidateSet(-1,0,1)][int]$HireSightRefresh=-1,[switch]$ReuseIdentical,[int]$ThinkCap=-1,[ValidateSet(-1,0,1)][int]$Routes=-1,[ValidateSet(-1,0,1)][int]$AreaDepot=-1,[double]$AreaDepotRaid=-1,[ValidateSet(-1,0,1)][int]$AreaPointSupply=-1,[switch]$PlayerRouteTest,[switch]$HeightTest,[switch]$SectorAudit,[ValidateSet(0,1)][int]$SectorAI=1,[ValidateSet(-1,0,1)][int]$SectorContact=-1,[ValidateSet(-1,0,1)][int]$SectorRoute=-1,[ValidateSet(-1,0,1)][int]$SectorFlank=-1,[ValidateSet(-1,0,1)][int]$SectorFront=-1,[ValidateSet(-1,0,1)][int]$SectorSlotReserve=-1,[ValidateSet(-1,0,1)][int]$WaypointPass=-1,[ValidateSet(-1,0,1)][int]$SectorMoveFire=-1,[ValidateSet(-1,0,1)][int]$CoverHuddle=-1,[ValidateSet(-1,0,1)][int]$SquadCoverCheck=-1,[ValidateSet(-1,0,1)][int]$Bounding=-1,[ValidateSet(-1,0,1)][int]$PinnedFlank=-1,[ValidateSet(-1,0,1)][int]$ArrivalThreat=-1,[ValidateSet(-1,0,1)][int]$SectorCoverRoute=-1,[ValidateSet(-1,0,1)][int]$VATFarLight=-1,[ValidateSet(-1,0,1)][int]$FollowerAnimSkip=-1,[int]$FollowerAnimBudget=-1,[ValidateSet(-1,0,1)][int]$FollowerNoOverlap=-1,[double]$CorpseLifetime=-1,[ValidateRange(-1,100)][int]$AreaMaxSquads=-1,[ValidateSet(-1,0,1)][int]$VehicleOrderHere=-1,[ValidateSet(-1,0,1)][int]$VehicleHitCover=-1,[ValidateSet(-1,0,1)][int]$TankGunFlash=-1,[ValidateSet(-1,0,1)][int]$ArtilleryFlash=-1,[int]$SPGRange=-1,[ValidateSet(-1,0,1)][int]$ArtilleryPosts=-1,[int]$AreaSPGCost=-1,[ValidateSet(-1,0,1)][int]$ObservedArmour=-1,[ValidateSet(-1,0,1)][int]$CoverStrict=-1,[ValidateSet(-1,0,1)][int]$ShotDirection=-1,[ValidateSet(-1,0,1)][int]$DangerRoute=-1,[ValidateSet(-1,0,1)][int]$CloseIn=-1,[ValidateSet(-1,0,1)][int]$SectorCoverWalk=-1,[ValidateSet(-1,0,1)][int]$UnderFireCover=-1,[ValidateSet(-1,0,1)][int]$NoCoverFallback=-1,[ValidateSet(-1,0,1)][int]$CoverPriority=-1,[ValidateSet(-1,0,1)][int]$FollowSightParallel=-1,[ValidateSet(-1,0,1)][int]$HitReveal=-1,[ValidateSet(-1,0,1)][int]$HitFace=-1,[ValidateSet(-1,0,1)][int]$ArrivalWalk=-1,[ValidateSet(-1,0,1)][int]$CoverOutward=-1,[ValidateSet(-1,0,1)][int]$CoverDense=-1,[ValidateSet(-1,0,1)][int]$CoverIndoor=-1,[ValidateRange(-1,10)][int]$AreaSupplyKeep=-1,[ValidateSet(-1,0,1)][int]$FrontLine=-1,[ValidateRange(-1,12)][int]$FrontSquads=-1,[ValidateSet(-1,0,1)][int]$FrontArmy=-1,[ValidateSet(-1,0,1)][int]$FrontAuto=-1,[ValidateSet(-1,0,1)][int]$FrontGap=-1,[ValidateSet(-1,0,1)][int]$FrontHold=-1,[ValidateSet(-1,0,1)][int]$GroupFireSupport=-1,[ValidateSet(-1,0,1)][int]$SurpriseGroup=-1,[ValidateRange(-1,6)][int]$SurpriseSquads=-1,[ValidateSet(-1,0,1)][int]$ObservedTactics=-1,[ValidateSet(-1,0,1)][int]$SituationLog=-1,[ValidateRange(-1,4)][int]$Strategy=-1,[ValidateSet(-1,0,1)][int]$StrategyTeam=-1,[ValidateSet(-1,0,1)][int]$StrategyPass=-1,[ValidateSet(-1,0,1)][int]$AreaAllTypes=-1,[ValidateRange(-1,9)][int]$AreaWithout=-1,[double]$FrontShare=-1,[ValidateRange(-1,63)][int]$CorpseSteps=-1,[int]$StateTrace=-1,[string]$StateTraceDetail='',[int]$RoundTripAt=-1,[ValidateSet(-1,0,1)][int]$RouteTieSymmetric=-1,[ValidateSet(-1,0,1)][int]$AttackFocus=-1,[ValidateSet(-1,0,1)][int]$FocusTarget=-1,[ValidateSet(-1,0,1)][int]$SectorSymmetric=-1,[ValidateRange(-1,2)][int]$StanceMove=-1,[ValidateSet(-1,0,1)][int]$ArtilleryRaidChase=-1,[ValidateRange(-1,500)][int]$ArtilleryRaidSneak=-1,[switch]$Sound,[string]$ExtraArgs='',[double]$ArtilleryStandoff=-1,[ValidateSet(-1,0,1)][int]$SoldierWatch=-1,[ValidateSet(-1,0,1)][int]$SoldierWatchTeam=-1,[ValidateSet(-1,0,1,2)][int]$ThreatCover=-1,[ValidateSet(-1,0,1)][int]$ThreatCoverTeam=-1,[ValidateSet(-1,0,1,2,3)][int]$ThreatCoverRelocate=-1,[ValidateSet(-1,0,1)][int]$ThreatCoverCrouch=-1,[ValidateSet(-1,0,1)][int]$ThreatCoverCrouchTeam=-1,[ValidateSet(-1,0,1)][int]$SoldierMove=-1,[ValidateSet(-1,0,1)][int]$SoldierMoveTeam=-1,[switch]$SoldierOrderTest,[double]$SoldierAIFocus=-1,[string]$SoldierAIFocusAt='',[ValidateSet(-1,0,1)][int]$SetupPhase=-1,[ValidateSet(-1,0,1)][int]$OrderSync=-1,[ValidateRange(-1,25)][int]$StartSquads=-1,[ValidateRange(-1,3)][int]$ArtilleryRaidGroup=-1,[ValidateRange(-1,4)][int]$ArtilleryRaidScreen=-1,[ValidateSet(-1,0,1)][int]$AreaExoEarly=-1)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
if($OutputName -notmatch '^[A-Za-z0-9_-]+$'){throw 'Simple output name required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$snapshot=$null
if($FrozenBuild){$snapshot=Get-Content -LiteralPath $FrozenBuild -Raw|ConvertFrom-Json}
# 2026-11-18: snapshots made by New-BattleBuildSnapshot.ps1 carry their own project copy; UE then runs from that copy and
# the checks below look at the copied DLL/Config/map, so builds and Config edits in the working tree do not affect the run.
$projDir=if($snapshot -and $snapshot.projectCopy){$snapshot.projectCopy}else{Join-Path $root 'Game/FutureBattle'}
$project=Join-Path $projDir 'FutureBattle.uproject'
if ($Map -eq 'H' -and !$FrozenBuild) {
    & (Join-Path $PSScriptRoot 'Initialize-MapH.ps1') -Project $project
}
$out=Join-Path $PSScriptRoot ('Benchmarks/SightTrial-20261111/'+$OutputName)
if(Test-Path $out){throw "Output exists: $out"}
New-Item -ItemType Directory -Path $out | Out-Null
$dll=Join-Path $projDir 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$mapFile=Join-Path $projDir "Content/Prototype/Lvl_BattleWide_$Map.umap"
$hashes=@()
if(!$FrozenBuild){$hashes=@(Get-ChildItem (Join-Path $root 'Game/FutureBattle/Source'),(Join-Path $root 'Game/FutureBattle/Config') -File -Recurse | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})}
if($FrozenBuild){
 if((Get-FileHash $dll).Hash -ne $snapshot.dll -or (Get-FileHash $snapshot.sourceArchive).Hash -ne $snapshot.sourceArchiveSha256){throw 'Frozen build or source archive changed'}
 foreach($file in $snapshot.files){if($file.path -notmatch '[\\/]Source[\\/]' -and (Get-FileHash $file.path).Hash -ne $file.sha256){throw 'Runtime configuration changed since snapshot'}}
 $expectedMap=@($snapshot.maps|Where-Object{$_.path -eq $mapFile})
 if($expectedMap.Count -ne 1 -or (Get-FileHash $mapFile).Hash -ne $expectedMap[0].sha256){throw 'Map changed since snapshot'}
 # C++ edits may continue, but this run executes the pinned DLL and unchanged runtime configuration.
 $hashes=$snapshot.files
}
$savedSettings=@(Get-ChildItem (Join-Path $projDir 'Saved/Config/WindowsEditor') -Filter '*.ini' -File | ForEach-Object {@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}})
$log=Join-Path $out 'run.log'
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleRangeAware=1',"-BattleSightRange=$SightRange",('-abslog="'+$log+'"'))
if (($Map -eq 'H' -or $Map -eq 'I' -or $Map -eq 'J') -and $Area) {
    $a+="-BattleSectorAI=$SectorAI"
    if ($SectorContact -ge 0) { $a+="-BattleSectorContact=$SectorContact" }
    if ($SectorRoute -ge 0) { $a+="-BattleSectorRoute=$SectorRoute" }
    if ($SectorFlank -ge 0) { $a+="-BattleSectorFlank=$SectorFlank" }
    if ($SectorFront -ge 0) { $a+="-BattleSectorFront=$SectorFront" }
    if ($SectorSlotReserve -ge 0) { $a+="-BattleSectorSlotReserve=$SectorSlotReserve" }
    if (!$PSBoundParameters.ContainsKey('CoverApproach')) { $CoverApproach=1 }
    if (!$PSBoundParameters.ContainsKey('LeaderGuard')) { $LeaderGuard=1 }
}
$a+=@("-BattleSquadFlank=$FlankEnabled","-BattleCoverActions=$CoverActions","-BattleFlankCoordination=$FlankCoordination","-BattleAICoverProbe=$AICoverProbe","-BattleCoverApproach=$CoverApproach","-BattleLeaderGuard=$LeaderGuard")
if($LeaderHealth -gt 0){$a+="-BattleLeaderHealth=$LeaderHealth"}
# Supply trials: -1 = not passed (Config default).
if($SupplyReturnLiving -ge 0){$a+="-BattleSupplyReturnLiving=$SupplyReturnLiving"};if($SupplyResumeLiving -ge 0){$a+="-BattleSupplyResumeLiving=$SupplyResumeLiving"}
if($SupplyAwayRatio -ge 0){$a+=("-BattleSupplyAwayRatio="+$SupplyAwayRatio.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SupplyHeal -ge 0){$a+=("-BattleSupplyHeal="+$SupplyHeal.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($SupplyResumeHealth -ge 0){$a+=("-BattleSupplyResumeHealth="+$SupplyResumeHealth.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($ReacquireGrace -ge 0){$a+="-BattleReacquireGrace=$ReacquireGrace"}
if($CoverFire -ge 0){$a+="-BattleCoverFire=$CoverFire"}
if($TerrainLineBodies -ge 0){$a+="-BattleTerrainLineBodies=$TerrainLineBodies"}
if($ContactCover){$a+="-BattleContactCover=1";if($ContactCoverTeam -ge 0){$a+="-BattleContactCoverTeam=$ContactCoverTeam"}}
if($CoverValidation -ge 0){$a+="-BattleCoverValidation=$CoverValidation"}
if($CoverPeekSight -ge 0){$a+="-BattleCoverPeekSight=$CoverPeekSight"}
if($PerceptionOpt -eq 1){$a+='-BattleReuseSightContext=1','-BattlePerceptionGrid=1','-BattleReduceHiddenBones=1'}
# Routes (map F, BattleRoutes.cpp): -1 = not passed (on where the map defines routes).
if($Routes -ge 0){$a+="-BattleRoutes=$Routes"}
# Depots in the team AI (map F, BattleAreaDepot.cpp), trial, default off: 1 turns it on (with -AreaTrialTeam, for that team only). -AreaDepotRaid replaces the raid weight (default 2).
if($AreaPointSupply -ge 0){$a+="-BattleAreaPointSupply=$AreaPointSupply"};if($AreaDepot -ge 0){$a+="-BattleAreaDepot=$AreaDepot"};if($AreaDepotRaid -ge 0){$a+="-BattleAreaDepotRaid=$AreaDepotRaid"}
# Player route choice self-test (map F, BattlePlayerRoutes.cpp): three squads of team 0 walk to the centre over the three routes; read BATTLE_PLAYER_ROUTE_RESULT in run.log. Not with -RoundTripSave (a load clears the choices).
if($PlayerRouteTest){$a+="-BattlePlayerRouteTest"}
# The player's order to one soldier (2026-11-20): walk, hold, send back, cleared by the squad's next order (BATTLE_SOLDIER_ORDER_RESULT).
if($SoldierOrderTest){$a+="-BattleSoldierOrderTest";if($Rendered){$a+="-BattleSoldierOrderShots=$(Join-Path $out 'soldier-order')"}}
# Individual AI only near the player (2026-11-20, 0 = everyone): radius in m; -SoldierAIFocusAt "X,Y" (m) fixes the point for headless runs.
if($SoldierAIFocus -ge 0){$a+="-BattleSoldierAIFocus=$SoldierAIFocus"}; if($SoldierAIFocusAt){$a+="-BattleSoldierAIFocusAt=$SoldierAIFocusAt"}
# Height differences self-test (map G, BattleTerrainTest.cpp): three squads of team 0 walk onto a hilltop, onto an upper floor and across a ridge; read BATTLE_HEIGHT_RESULT in run.log.
if($HeightTest){$a+="-BattleHeightTest"}
# Sector orders audit (BattleSectorAudit.cpp, a map with sectors); -SectorAI 0 runs map H with the ordinary AI orders for comparison.
if($SectorAudit){$a+="-BattleSectorAudit"}
if ($WaypointPass -ge 0) { $a+="-BattleWaypointPass=$WaypointPass" }
if ($SectorMoveFire -ge 0) { $a+="-BattleSectorMoveFire=$SectorMoveFire" }
if ($CoverHuddle -ge 0) { $a+="-BattleCoverHuddle=$CoverHuddle" }
if ($SquadCoverCheck -ge 0) { $a+="-BattleSquadCoverCheck=$SquadCoverCheck" }
if ($Bounding -ge 0) { $a+="-BattleBounding=$Bounding" }
if ($PinnedFlank -ge 0) { $a+="-BattlePinnedFlank=$PinnedFlank" }
if ($ArrivalThreat -ge 0) { $a+="-BattleArrivalThreat=$ArrivalThreat" }
if ($SectorCoverRoute -ge 0) { $a+="-BattleSectorCoverRoute=$SectorCoverRoute" }
if ($CoverStrict -ge 0) { $a+="-BattleCoverStrict=$CoverStrict" }
if ($ShotDirection -ge 0) { $a+="-BattleShotDirection=$ShotDirection" }
if ($DangerRoute -ge 0) { $a+="-BattleDangerRoute=$DangerRoute" }
if ($CloseIn -ge 0) { $a+="-BattleCloseIn=$CloseIn" }
if ($SectorCoverWalk -ge 0) { $a+="-BattleSectorCoverWalk=$SectorCoverWalk" }
if ($UnderFireCover -ge 0) { $a+="-BattleUnderFireCover=$UnderFireCover" }
if ($NoCoverFallback -ge 0) { $a+="-BattleNoCoverFallback=$NoCoverFallback" }
if ($CoverPriority -ge 0) { $a+="-BattleCoverPriority=$CoverPriority" }
if ($SituationLog -ge 0) { $a+="-BattleSituationLog=$SituationLog" }
if ($Strategy -ge 0) { $a+="-BattleStrategy=$Strategy" }
if ($StrategyTeam -ge 0) { $a+="-BattleStrategyTeam=$StrategyTeam" }
if ($StrategyPass -ge 0) { $a+="-BattleStrategyPass=$StrategyPass" }
if ($AreaAllTypes -ge 0) { $a+="-BattleAreaAllTypes=$AreaAllTypes" }
if ($AreaWithout -ge 0) { $a+="-BattleAreaWithout=$AreaWithout" }
if ($FrontShare -ge 0) { $a+=("-BattleFrontShare="+$FrontShare.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if ($CorpseSteps -ge 0) { $a+="-BattleCorpseSteps=$CorpseSteps" }
if ($StateTrace -ge 0) { $a+="-BattleStateTrace=$StateTrace" }
if ($StateTraceDetail) { if($StateTraceDetail -notmatch '^[0-9.]+-[0-9.]+$'){throw 'StateTraceDetail must be <from>-<to> seconds'}; $a+="-BattleStateTraceDetail=$StateTraceDetail" }
if ($HitReveal -ge 0) { $a+="-BattleHitReveal=$HitReveal" }
if ($HitFace -ge 0) { $a+="-BattleHitFace=$HitFace" }
if ($ArrivalWalk -ge 0) { $a+="-BattleArrivalWalk=$ArrivalWalk" }
if ($CoverOutward -ge 0) { $a+="-BattleCoverOutward=$CoverOutward" }
if ($CoverDense -ge 0) { $a+="-BattleCoverDense=$CoverDense" }
if ($CoverIndoor -ge 0) { $a+="-BattleCoverIndoor=$CoverIndoor" }
if ($AttackFocus -ge 0) { $a+="-BattleAttackFocus=$AttackFocus" }
if ($SoldierWatch -ge 0) { $a+="-BattleSoldierWatch=$SoldierWatch" }; if ($SoldierWatchTeam -ge 0) { $a+="-BattleSoldierWatchTeam=$SoldierWatchTeam" }
if ($FocusTarget -ge 0) { $a+="-BattleFocusTarget=$FocusTarget" }
if ($SectorSymmetric -ge 0) { $a+="-BattleSectorSymmetric=$SectorSymmetric" }
$a+=@("-BattleAreaCompositionPlanner=$AreaCompositionPlanner","-BattleAreaCompositionPlannerTeams=$AreaCompositionPlannerTeams")
# Individual AI stage A (2026-11-20, trial, default 0): threats of each soldier, cover against all of them, leave a flanked place. -ThreatCoverTeam limits it to one team.
if ($ThreatCover -ge 0) { $a+="-BattleThreatCover=$ThreatCover" }; if ($ThreatCoverTeam -ge 0) { $a+="-BattleThreatCoverTeam=$ThreatCoverTeam" }
# Stage A follow-up (2026-11-21, trial, default 0): look for the next cover place before leaving a broken one; 1 = none: leave, 2 = none: stay.
if ($ThreatCoverRelocate -ge 0) { $a+="-BattleThreatCoverRelocate=$ThreatCoverRelocate" }
if ($ThreatCoverCrouch -ge 0) { $a+="-BattleThreatCoverCrouch=$ThreatCoverCrouch" }; if ($ThreatCoverCrouchTeam -ge 0) { $a+="-BattleThreatCoverCrouchTeam=$ThreatCoverCrouchTeam" }
# Individual AI watch (2026-11-22, trial, default 0): a soldier standing still with nothing to fire at faces what he knows of the enemy.
# Individual AI stage B1 (2026-11-20, trial, default 0): a soldier in cover walks to a clearly better cover place inside the squad's sector. -SoldierMoveTeam limits it to one team.
if ($SoldierMove -ge 0) { $a+="-BattleSoldierMove=$SoldierMove" }; if ($SoldierMoveTeam -ge 0) { $a+="-BattleSoldierMoveTeam=$SoldierMoveTeam" }
if($AreaHire -ge 0){$a+="-BattleAreaHire=$AreaHire"}
if($AreaIncome -ge 0){$a+="-BattleAreaIncome=$AreaIncome"}
if($AreaStrategy -ge 0){$a+="-BattleAreaStrategy=$AreaStrategy"};if($AreaAttackWeight -ge 0){$a+=("-BattleAreaAttackWeight="+$AreaAttackWeight.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseShare -ge 0){$a+=("-BattleAreaBaseShare="+$AreaBaseShare.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaBaseRule -ge 0){$a+="-BattleAreaBaseRule=$AreaBaseRule"};if($AreaBaseShareLow -ge 0){$a+=("-BattleAreaBaseShareLow="+$AreaBaseShareLow.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseShareHigh -ge 0){$a+=("-BattleAreaBaseShareHigh="+$AreaBaseShareHigh.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaHeavyShare -ge 0){$a+=("-BattleAreaHeavyShare="+$AreaHeavyShare.ToString([Globalization.CultureInfo]::InvariantCulture))};if($ReturnFire -ge 0){$a+="-BattleReturnFire=$ReturnFire"};if($AreaIncomeUrbanBase -ge 0){$a+="-BattleAreaIncomeUrbanBase=$AreaIncomeUrbanBase"};if($AreaIncomeUrbanPerPoint -ge 0){$a+="-BattleAreaIncomeUrbanPerPoint=$AreaIncomeUrbanPerPoint"};if($AreaPlan -ge 0){$a+="-BattleAreaPlan=$AreaPlan"};if($AreaHirePurpose -ge 0){$a+="-BattleAreaHirePurpose=$AreaHirePurpose"};if($AreaIncomeBase -ge 0){$a+="-BattleAreaIncomeBase=$AreaIncomeBase"};if($AreaIncomePerPoint -ge 0){$a+="-BattleAreaIncomePerPoint=$AreaIncomePerPoint"};if($AreaIncomeRamp -ge 0){$a+=("-BattleAreaIncomeRamp="+$AreaIncomeRamp.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaStartCredits -ge 0){$a+="-BattleAreaStartCredits=$AreaStartCredits"};if($AreaBaseFlankSide -ge 0){$a+="-BattleAreaBaseFlankSide=$AreaBaseFlankSide"};if($AreaBaseReturnLiving -ge 0){$a+="-BattleAreaBaseReturnLiving=$AreaBaseReturnLiving"};if($AreaBaseFlank -ge 0){$a+=("-BattleAreaBaseFlank="+$AreaBaseFlank.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaTrialTeam -ge 0){$a+="-BattleAreaTrialTeam=$AreaTrialTeam"};if($AreaFocus -ge 0){$a+=("-BattleAreaFocus="+$AreaFocus.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseInfiltrate -ge 0){$a+=("-BattleAreaBaseInfiltrate="+$AreaBaseInfiltrate.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaFrontBalance -ge 0){$a+="-BattleAreaFrontBalance=$AreaFrontBalance"};if($AreaFrontGain -ge 0){$a+=("-BattleAreaFrontGain="+$AreaFrontGain.ToString([Globalization.CultureInfo]::InvariantCulture))}
if($AreaMaxSquads -ge 0){$a+="-BattleAreaMaxSquads=$AreaMaxSquads"};if($VehicleOrderHere -ge 0){$a+="-BattleVehicleOrderHere=$VehicleOrderHere"};if($VehicleHitCover -ge 0){$a+="-BattleVehicleHitCover=$VehicleHitCover"};if($TankGunFlash -ge 0){$a+="-BattleTankGunFlash=$TankGunFlash"};if($ArtilleryFlash -ge 0){$a+="-BattleArtilleryFlash=$ArtilleryFlash"};if($SPGRange -gt 0){$a+="-BattleSPGRange=$SPGRange"};if($ArtilleryPosts -ge 0){$a+="-BattleArtilleryPosts=$ArtilleryPosts"};if($ArtilleryRaidChase -ge 0){$a+="-BattleArtilleryRaidChase=$ArtilleryRaidChase"};if($ArtilleryRaidSneak -ge 0){$a+="-BattleArtilleryRaidSneak=$ArtilleryRaidSneak"};if($ArtilleryStandoff -ge 0){$a+=("-BattleArtilleryStandoff="+$ArtilleryStandoff.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaSPGCost -gt 0){$a+="-BattleAreaSPGCost=$AreaSPGCost"};if($ObservedArmour -ge 0){$a+="-BattleObservedArmour=$ObservedArmour"};if($ObservedTactics -ge 0){$a+="-BattleObservedTactics=$ObservedTactics"};if($SetupPhase -ge 0){$a+="-BattleSetupPhase=$SetupPhase"};if($OrderSync -ge 0){$a+="-BattleOrderSync=$OrderSync"};if($StartSquads -ge 0){$a+="-BattleStartSquads=$StartSquads"};if($AreaHireReserve -ge 0){$a+="-BattleAreaHireReserve=$AreaHireReserve"};if($AreaHireInterval -ge 0){$a+=("-BattleAreaHireInterval="+$AreaHireInterval.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaHireStop -ge 0){$a+=("-BattleAreaHireStop="+$AreaHireStop.ToString([Globalization.CultureInfo]::InvariantCulture))};if($ArtilleryRaidGroup -ge 0){$a+="-BattleArtilleryRaidGroup=$ArtilleryRaidGroup"};if($AreaExoEarly -ge 0){$a+="-BattleAreaExoEarly=$AreaExoEarly"};if($ArtilleryRaidScreen -ge 0){$a+="-BattleArtilleryRaidScreen=$ArtilleryRaidScreen"}
if($AreaWarScore -ge 0){$a+="-BattleAreaWarScore=$AreaWarScore"};if($AreaWarTarget -ge 0){$a+="-BattleAreaWarTarget=$AreaWarTarget"}
# Range profile (map E default ON; see BattleRanges.h): -1 = not passed. Values in cm.
if($RangeProfile -ge 0){$a+="-BattleRangeProfile=$RangeProfile"};if($RangeSight -ge 0){$a+="-BattleRangeSight=$RangeSight"};if($RangeEngage -ge 0){$a+="-BattleRangeEngage=$RangeEngage"}
if($RangeEngageSupport -ge 0){$a+="-BattleRangeEngageSupport=$RangeEngageSupport"};if($RangeReach -ge 0){$a+="-BattleRangeReach=$RangeReach"}
if($Stances -ge 0){$a+="-BattleStances=$Stances"};if($CoverArrival -ge 0){$a+="-BattleCoverArrival=$CoverArrival"}
# Suppression trial: -1 = not passed (default OFF).
if($SuppressionSupport -ge 0){$a+=("-BattleSuppressionSupport="+$SuppressionSupport.ToString([Globalization.CultureInfo]::InvariantCulture))};if($Suppression -ge 0){$a+="-BattleSuppression=$Suppression"};if($SuppressionPin -ge 0){$a+=("-BattleSuppressionPin="+$SuppressionPin.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SuppressionSpread -ge 0){$a+=("-BattleSuppressionSpread="+$SuppressionSpread.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SuppressionPerShot -ge 0){$a+=("-BattleSuppressionPerShot="+$SuppressionPerShot.ToString([Globalization.CultureInfo]::InvariantCulture))}
# Scout pairs kept by the AI (trial team only with -AreaTrialTeam); -1 = not passed (default AreaScoutKeep).
if($AreaScoutKeep -ge 0){$a+="-BattleAreaScoutKeep=$AreaScoutKeep"}
# Intel trial (AI acts on known enemy positions) and scout retreat switch; -1 = not passed.
if($AreaIntel -ge 0){$a+="-BattleAreaIntel=$AreaIntel"};if($AreaATKeep -ge 0){$a+="-BattleAreaATKeep=$AreaATKeep"};if($AreaTankKeep -ge 0){$a+="-BattleAreaTankKeep=$AreaTankKeep"};if($AreaACKeep -ge 0){$a+="-BattleAreaACKeep=$AreaACKeep"};if($AreaMortarKeep -ge 0){$a+="-BattleAreaMortarKeep=$AreaMortarKeep"};if($AreaSPGKeep -ge 0){$a+="-BattleAreaSPGKeep=$AreaSPGKeep"};if($AreaExoKeep -ge 0){$a+="-BattleAreaExoKeep=$AreaExoKeep"};if($ArmourAI -ge 0){$a+="-BattleArmourAI=$ArmourAI"};if($ArmourAITeam -ge 0){$a+="-BattleArmourAITeam=$ArmourAITeam"};if($AreaComposition -ge 0){$a+="-BattleAreaComposition=$AreaComposition"};if($AreaGroups -ge 0){$a+="-BattleAreaGroups=$AreaGroups"};if($DefendEngage -ge 0){$a+="-BattleDefendEngage=$DefendEngage"};if($SymmetricStart -ge 0){$a+="-BattleSymmetricStart=$SymmetricStart"};if($SymmetryAudit){$a+='-BattleSymmetryAudit'};if($ScoutRetreat -ge 0){$a+="-BattleScoutRetreat=$ScoutRetreat"}
if($AreaSupplyKeep -ge 0){$a+="-BattleAreaSupplyKeep=$AreaSupplyKeep"};if($FrontLine -ge 0){$a+="-BattleFrontLine=$FrontLine"};if($FrontSquads -ge 0){$a+="-BattleFrontSquads=$FrontSquads"};if($FrontArmy -ge 0){$a+="-BattleFrontArmy=$FrontArmy"};if($FrontAuto -ge 0){$a+="-BattleFrontAuto=$FrontAuto"};if($FrontGap -ge 0){$a+="-BattleFrontGap=$FrontGap"};if($FrontHold -ge 0){$a+="-BattleFrontHold=$FrontHold"};if($GroupFireSupport -ge 0){$a+="-BattleGroupFireSupport=$GroupFireSupport"};if($SurpriseGroup -ge 0){$a+="-BattleSurpriseGroup=$SurpriseGroup"};if($SurpriseSquads -ge 0){$a+="-BattleSurpriseSquads=$SurpriseSquads"};if($StanceMove -ge 0){$a+="-BattleStanceMove=$StanceMove"}
# Heavy infantry number trials (2026-11-16): override members, health and speed scale, purchase cost and damage per shot.
foreach($hv in @(@("Members",$HeavyMembers),@("Health",$HeavyHealth),@("Speed",$HeavySpeed),@("Cost",$HeavyCost),@("Damage",$HeavyDamage))){if($hv[1] -ge 0){$a+=("-BattleHeavy"+$hv[0]+"="+([double]$hv[1]).ToString([Globalization.CultureInfo]::InvariantCulture))}}
if($RifleMembers -ge 2){$a+="-BattleRifleMembers=$RifleMembers"}
if($AllyPass -ge 0){$a+="-BattleAllyPass=$AllyPass"}
if($CorpseLifetime -ge 0){$a+="-BattleCorpseLifetime=$CorpseLifetime"}
$a+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams")
if($FlankTrial){$a+='-BattleFlankTrial'}
if($ObserveTactics){$a+='-BattleTacticalObserve'}
if($Area){$a+='-BattleArea'}
if($RoundTripSave){if(!$Experiment){throw 'RoundTripSave requires Experiment'};$a+='-BattleVerifyMixedSave'}
if($RoundTripAt -gt 0){if(!$RoundTripSave){throw 'RoundTripAt requires RoundTripSave'};$a+="-BattleVerifySaveAt=$RoundTripAt"}
if ($RouteTieSymmetric -ge 0) { $a+="-BattleRouteTieSymmetric=$RouteTieSymmetric" }
if($WeaponRange){$a+="-BattleWeaponRange=$WeaponRange"}
if($Swap -and !$Experiment){$a+='-BattleTrialSwap'}
if($Visual){$a+='-BattleSightVisual';if($FirstPerson){$a+='-BattleFirstPerson'}}elseif($Experiment){$a+=@(('-BattleSightExperiment="'+(Join-Path $out 'metrics.json')+'"'),"-BattleTrialSeed=$Seed",'-UseFixedTimeStep',"-FPS=$Fps");if($Swap){$a+='-BattleTrialSwap'}}else{$a+='-BattleWideMapAudit'}
if($GeometryOnly){$a+='-BattleWideMapGeometryOnly'}
if($Rendered){$a+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$a+='-nullrhi'}
# Sound on (2026-11-22, BattleAudio): drop -nosound, e.g. to show that the sounds leave the match unchanged; extra game
# arguments (space separated) such as -ExtraArgs '-BattleSFX=0 -BattleFX=0'.
if($Sound){$a=@($a|Where-Object{$_ -ne '-nosound'})}
if($ExtraArgs){$a+=@($ExtraArgs -split '\s+'|Where-Object{$_})}
# Unreal Insights trace (e.g. -Trace cpu,frame): written next to the run log; named CPU events for a game-thread breakdown.
if($Trace){if($Trace -notmatch '^[A-Za-z0-9,]+$'){throw 'Trace channels: letters, digits and commas'};$a+=@("-trace=$Trace",('-tracefile="'+(Join-Path $out 'trace.utrace')+'"'),'-statnamedevents')}
# Line-of-sight pair reuse trial (performance); -1 = not passed (default OFF).
if($SightPairReuse -ge 0){$a+="-BattleSightPairReuse=$SightPairReuse"}
if($SightHitStats -ge 0){$a+="-BattleSightHitStats=$SightHitStats"};if($SightParallel -ge 0){$a+="-BattleSightParallel=$SightParallel"};if($SightTeamMask -ge 0){$a+="-BattleSightTeamMask=$SightTeamMask"}
# Sight scan interval trial (seconds for observers that saw no enemy at their last scan); -1 = not passed (default 0.4 s).
if($SightScanInterval -ge 0){$a+=("-BattleSightScanInterval="+$SightScanInterval.ToString([Globalization.CultureInfo]::InvariantCulture))}
# Unknown far enemies on alternate scan pulses (trial, 2026-11-17); -1 = not passed (off).
if($SightSlowFar -ge 0){$a+="-BattleSightSlowFar=$SightSlowFar"}
# Full sight update right after a hire or scout handover (2026-11-18); -1 = not passed (default 0 = off).
# Cap on squad decisions per step (trial 2026-11-18); -1 = not passed (default 0 = no cap).
if($ThinkCap -ge 0){$a+="-BattleThinkCap=$ThinkCap"}
if($HireSightRefresh -ge 0){$a+="-BattleHireSightRefresh=$HireSightRefresh"}
# Far VAT followers without a per-body tick or capsule attachment (trial 2026-11-18, changes match results); -1 = not passed (default 0 = off).
if($VATFarLight -ge 0){$a+="-BattleVATFarLight=$VATFarLight"}
# Near followers evaluate their skeleton every 1-4 frames by mesh LOD (trial 2026-11-18, display only); -1 = not passed (default 0 = off).
if($FollowerAnimSkip -ge 0){$a+="-BattleFollowerAnimSkip=$FollowerAnimSkip"}
# Cap on follower skeleton evaluations per frame (trial 2026-11-18, display only); -1 = not passed (default 0 = no cap).
if($FollowerAnimBudget -ge 0){$a+="-BattleFollowerAnimBudget=$FollowerAnimBudget"}
# Sight tests of the Follow loop traced in parallel before it (trial 2026-11-18, may change results); -1 = not passed (default 0).
# Follower capsules without overlap events (trial 2026-11-18); -1 = not passed (default 0).
if($FollowSightParallel -ge 0){$a+="-BattleFollowSightParallel=$FollowSightParallel"}
if($FollowerNoOverlap -ge 0){$a+="-BattleFollowerNoOverlap=$FollowerNoOverlap"}
if($Rendered -and $BehaviorConcurrency -ne 1){throw 'Rendered measurements must run alone'}
# Identical-run key (2026-11-18): DLL, map, runtime Config/.uproject contents and the game arguments without per-output
# paths. Written for every frozen-build NullRHI experiment so later runs with the same key can reuse its result.
$reuseKey=$null;$reusedFrom=$null
if($snapshot -and $Experiment -and !$Rendered -and !$Trace){
 $runtime=@($hashes|Where-Object{$_.path -notmatch '[\\/]Source[\\/]'}|ForEach-Object{($_.path -replace '^.*[\\/](Config[\\/].*|FutureBattle\.uproject)$','$1' -replace '\\','/')+'='+$_.sha256}|Sort-Object)
 $keyArgs=@($a|Select-Object -Skip 1|Where-Object{$_ -notmatch '^-(abslog|BattleSightExperiment|BattleWideScreens|tracefile)='})
 $keyText=(@('dll='+(Get-FileHash $dll).Hash,'map='+(Get-FileHash $mapFile).Hash)+$runtime+$keyArgs) -join "`n"
 $reuseKey=[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($keyText))) -replace '-',''
 Set-Content (Join-Path $out 'reuse-key.txt') $reuseKey -Encoding ascii
 if($ReuseIdentical){
  foreach($d in Get-ChildItem (Split-Path $out) -Directory){
   if($d.FullName -eq $out){continue}
   $k=Join-Path $d.FullName 'reuse-key.txt';$r=Join-Path $d.FullName 'result.json'
   if(!(Test-Path $k) -or !(Test-Path $r) -or !(Test-Path (Join-Path $d.FullName 'metrics.json'))){continue}
   if((Get-Content $k -Raw).Trim() -ne $reuseKey){continue}
   $prev=Get-Content $r -Raw|ConvertFrom-Json
   if($prev.passed -and !$prev.reusedFrom){$reusedFrom=$d.Name;break}
  }
 }
}
@{compositionPlanner=@{stage=2;purchase="planner_when_enabled";enabled=$AreaCompositionPlanner;teams=$AreaCompositionPlannerTeams};machine=(Get-MeasurementMachine);dll=(Get-FileHash $dll).Hash;map=(Get-FileHash $mapFile).Hash;files=$hashes;source_snapshot=$snapshot;savedSettings=$savedSettings;arguments=$a;behaviorConcurrency=$BehaviorConcurrency;timingComparable=(($BehaviorConcurrency -eq 1) -and !$reusedFrom);reuseKey=$reuseKey;reusedFrom=$reusedFrom} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
if($reusedFrom){
 # Same build, map, Config and arguments already played and passed: copy its outcome instead of playing again.
 $src=Join-Path (Split-Path $out) $reusedFrom
 Copy-Item (Join-Path $src 'metrics.json'),(Join-Path $src 'run.log') $out
 $compositionSourceManifest=Get-Content (Join-Path $src 'manifest.json') -Raw | ConvertFrom-Json
 $compositionReuseManifest=Get-Content (Join-Path $out 'manifest.json') -Raw | ConvertFrom-Json
 $compositionReuseManifest | Add-Member -NotePropertyName compositionResolvedLog -NotePropertyValue @($compositionSourceManifest.compositionResolvedLog) -Force
 $compositionReuseManifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
 $prev=Get-Content (Join-Path $src 'result.json') -Raw|ConvertFrom-Json
 @{passed=$true;exitCode=$prev.exitCode;dll=$prev.dll;reusedFrom=$reusedFrom} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
 "REUSED identical run $reusedFrom"
 Get-Content $log | Where-Object {$_ -match 'WIDE_AUDIT|SIGHT_EXPERIMENT|SIGHT_VISUAL'} | Write-Output
 return
}
$p=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $a -WindowStyle Hidden -PassThru
$timeout=if($Experiment){2700000}else{210000}
if(!$p.WaitForExit($timeout)){Stop-Process -Id $p.Id;throw "Timeout: $log"}
$t=Get-Content $log -Raw
# Planner diagnostics: preserve actual resolved definitions and the first pre-purchase input for each observed team.
$compositionManifest=Get-Content (Join-Path $out 'manifest.json') -Raw | ConvertFrom-Json
$compositionResolved=@($t -split "`n" | Where-Object {$_ -match 'BATTLE_COMPOSITION_CONFIG|BATTLE_COMPOSITION_DEF|BATTLE_COMPOSITION_RULES'})
foreach($compositionTeam in 0,1){$compositionResolved+=@($t -split "`n" | Where-Object {$_ -match "BATTLE_COMPOSITION_INPUT .*team=$compositionTeam "} | Select-Object -First 1)}
$compositionManifest | Add-Member -NotePropertyName compositionResolvedLog -NotePropertyValue $compositionResolved -Force
$compositionManifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$pass=$p.ExitCode -eq 0 -and $t -match '(WIDE_AUDIT|SIGHT_EXPERIMENT|SIGHT_VISUAL)_RESULT failures=0' -and $t -notmatch 'WIDE_AUDIT_CHECK FAIL|Fatal error:|Assertion failed:'
# The soldier order self-test must finish and pass too (a missing result line is a failure).
if($SoldierOrderTest){$pass=$pass -and $t -match 'BATTLE_SOLDIER_ORDER_RESULT failures=0'}
if($RoundTripSave){$pass=$pass -and $t -match 'BATTLE_MIXED_SAVE_SOAK_RESULT failures=0'}
if($snapshot -and (Get-FileHash $dll).Hash -ne $snapshot.dll){$pass=$false}
if($snapshot){
 foreach($file in $snapshot.files){if($file.path -notmatch '[\\/]Source[\\/]' -and (Get-FileHash $file.path).Hash -ne $file.sha256){$pass=$false}}
 if((Get-FileHash $mapFile).Hash -ne $expectedMap[0].sha256 -or (Get-FileHash $snapshot.sourceArchive).Hash -ne $snapshot.sourceArchiveSha256){$pass=$false}
}
@{passed=$pass;exitCode=$p.ExitCode;dll=(Get-FileHash $dll).Hash} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$t -split "`n" | Where-Object {$_ -match 'WIDE_AUDIT|SIGHT_EXPERIMENT|SIGHT_VISUAL'} | Write-Output
if(!$pass){throw "Failed: $log"}

