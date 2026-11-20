# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値はソースから推定したもの（`FParse::Value` に渡す変数の初期値、または構造体のメンバーの初期値）。空欄は推定できなかったもの。`UPROPERTY(Config)` の値は `Config/DefaultGame.ini` で上書きされることがある。大事なときは「場所」の行と ini を確かめる。`Param` は付けるだけでONになる引数。

計 268 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleAIBlindFire` | other |  | BattleBlindFire.cpp:27 |  |
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:10 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:1107 |  |
| `-BattleAllyPass` | Value | 0 | BattleStableMovement.cpp:31 |  |
| `-BattleArea` | Param/other | off | BattleLeaderRuntime.cpp:76, BattleState.cpp:555 |  |
| `-BattleAreaACKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:97 |  |
| `-BattleAreaATKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:95 |  |
| `-BattleAreaAttackWeight` | Value | T->AreaAttackWeight | BattleTactics.cpp:75 |  |
| `-BattleAreaBaseFlank` | Value | 1.3f | BattleTactics.cpp:88 |  |
| `-BattleAreaBaseFlankSide` | Value | S.bFlankSameSide?1:0（bFlankSameSide=true） | BattleTactics.cpp:90 |  |
| `-BattleAreaBaseInfiltrate` | Value | 0 | BattleTactics.cpp:84 |  |
| `-BattleAreaBaseLowHP` | Value | 500.f | BattleTactics.cpp:80 |  |
| `-BattleAreaBaseReturnLiving` | Value | 5 | BattleTactics.cpp:91 |  |
| `-BattleAreaBaseRule` | Value | 0 | BattleTactics.cpp:77 |  |
| `-BattleAreaBaseShare` | Value | T->AreaBaseShare | BattleTactics.cpp:76 |  |
| `-BattleAreaBaseShareHigh` | Value | 0.3f | BattleTactics.cpp:79 |  |
| `-BattleAreaBaseShareLow` | Value | 0.f | BattleTactics.cpp:78 |  |
| `-BattleAreaComposition` | Value | GetDefault<UBattleTuning>()->bAreaComposition?1:0 | BattleArmourAI.cpp:38 |  |
| `-BattleAreaDepot` | Value | 0 | BattleAreaDepot.cpp:22 |  |
| `-BattleAreaDepotRaid` | Value | DepotRaid | BattleAreaDepot.cpp:23 |  |
| `-BattleAreaExoKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:101 |  |
| `-BattleAreaFocus` | Value | 0.f | BattleTactics.cpp:86 |  |
| `-BattleAreaFrontBalance` | Value | S.bFrontBalance?1:0（bFrontBalance=false） | BattleTactics.cpp:81 |  |
| `-BattleAreaFrontGain` | Value | 1.f | BattleTactics.cpp:82 |  |
| `-BattleAreaGroups` | Value | -1 | BattleGroups.cpp:74 |  |
| `-BattleAreaHeavyShare` | Value | 0.f / -1.f（同名のメンバーが複数） | BattleTactics.cpp:93 |  |
| `-BattleAreaHire` | Value | T->bAreaHire?1:0 | BattleTactics.cpp:55 |  |
| `-BattleAreaHireInterval` | Value | T->AreaHireIntervalSeconds | BattleTactics.cpp:57 |  |
| `-BattleAreaHirePurpose` | Value | 0 | BattleTactics.cpp:60 |  |
| `-BattleAreaHirePurposeFront` | Value | 10 | BattleTactics.cpp:61 |  |
| `-BattleAreaHireReserve` | Value | T->AreaHireReserve | BattleTactics.cpp:57 |  |
| `-BattleAreaHireStop` | Value | T->AreaHireStopSeconds | BattleTactics.cpp:58 |  |
| `-BattleAreaIncome` | Value | T->bAreaIncome?1:0 | BattleTactics.cpp:119 |  |
| `-BattleAreaIncomeBase` | Value | FMath::Clamp(T->AreaBaseIncome | BattleTactics.cpp:122 |  |
| `-BattleAreaIncomePerPoint` | Value | FMath::Clamp(T->AreaPointIncome | BattleTactics.cpp:122 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:127 |  |
| `-BattleAreaIncomeUrbanBase` | Value | T->AreaBaseIncomeUrban | BattleTactics.cpp:125 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | T->AreaPointIncomeUrban | BattleTactics.cpp:125 |  |
| `-BattleAreaIntel` | Value | GetDefault<UBattleTuning>()->bAreaIntel?1:0 | BattleAreaIntel.cpp:14 |  |
| `-BattleAreaMaxSquads` | Value | T->AreaMaxActiveSquads | BattleTactics.cpp:59 |  |
| `-BattleAreaMortarKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:98 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:92 |  |
| `-BattleAreaPointSupply` | other |  | BattleTactics.cpp:39 |  |
| `-BattleAreaScoutKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:94 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:42 |  |
| `-BattleAreaSPGKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:99 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:128 |  |
| `-BattleAreaStrategy` | Value | T->bAreaStrategy?1:0 | BattleTactics.cpp:74 |  |
| `-BattleAreaTankKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:96 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:102 |  |
| `-BattleAreaWarPointSeconds` | other |  | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | other |  | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | other |  | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | other |  | BattleWarScore.cpp:12 |  |
| `-BattleArmourAI` | Value | GetDefault<UBattleTuning>()->bArmourAI?1:0 | BattleArmourAI.cpp:31 |  |
| `-BattleArmourAITeam` | Value | -1 | BattleArmourAI.cpp:32 |  |
| `-BattleArrivalThreat` | Value | 0 | BattleLeaderRuntime.cpp:1436 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:53, BattleLeaderRuntime.cpp:72 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleBlindFire` | other |  | BattleBlindFire.cpp:26 |  |
| `-BattleBlindFireClipSide` | Value | TEXT("AS_CoverLo_AimR") | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireClipTop` | Value | TEXT("AS_CoverLo_AimU_down") | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireExpose` | other |  | BattleBlindFire.cpp:77 |  |
| `-BattleBlindFireSpread` | Value | 4.f | BattleBlindFire.cpp:30 |  |
| `-BattleBodyAnim` | Value | 1 | BattleBodyAnim.cpp:59 |  |
| `-BattleBounding` | Value | 0 | BattleLeaderRuntime.cpp:1411 |  |
| `-BattleCloseIn` | Value | 0 | BattleLeaderRuntime.cpp:1426 |  |
| `-BattleConcealment` | other |  | BattleLeaderRuntime.cpp:433, BattleLeaderRuntime.cpp:1121, BattleMapCommandlet.cpp:116 ほか1 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:84 |  |
| `-BattleContactCover` | Value | 0 | BattleTactics.cpp:186 |  |
| `-BattleContactCoverTeam` | Value | -1 | BattleTactics.cpp:180 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:85 |  |
| `-BattleCorpseLifetime` | Value | 20.f | BattleCorpses.cpp:25 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:192 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:212 |  |
| `-BattleCoverApproach` | Value | -1 | BattleTactics.cpp:174 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:131 |  |
| `-BattleCoverFire` | Value | GetDefault<UBattleTuning>()->bAICoverFire?1:0 | BattleTactics.cpp:150 |  |
| `-BattleCoverHuddle` | Value | 0 | BattleStableMovement.cpp:166 |  |
| `-BattleCoverPeekSight` | Value | GetDefault<UBattleTuning>()->bAICoverPeekSight?1:0 | BattleTactics.cpp:162 |  |
| `-BattleCoverStrict` | Value | 0 | BattleLeaderRuntime.cpp:1441 |  |
| `-BattleCoverValidation` | Value | GetDefault<UBattleTuning>()->bAICoverValidation?1:0 | BattleTactics.cpp:156 |  |
| `-BattleDangerRoute` | Value | 0 | BattleSectorRoute.cpp:28 |  |
| `-BattleDefendEngage` | Value | GetDefault<UBattleTuning>()->bDefendEngage?1:0 | BattleGroups.cpp:47 |  |
| `-BattleDelegationScreens` | Value |  | BattleDelegationSmoke.cpp:90 |  |
| `-BattleDiagnosticOmit` | Value |  | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:121, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:485 |  |
| `-BattleExoPlaceholder` | Param | off | BattleExo.cpp:107 |  |
| `-BattleExoShot` | Param | off | BattleLeaderRuntime.cpp:222 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:72 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value |  | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value |  | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 1 / 0 / 7（同名のメンバーが複数） | BattleSquadTrial.cpp:19 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:37 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:214 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:240 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:128, BattleSquadRetreat.cpp:18 |  |
| `-BattleFollowerAnimBudget` | Value | 0 | BattleVATDisplay.cpp:48 |  |
| `-BattleFollowerAnimSkip` | Value | 0 | BattleVATDisplay.cpp:53 |  |
| `-BattleFollowerNoOverlap` | Value | 0 | BattleLeaderRuntime.cpp:803 |  |
| `-BattleFollowSightParallel` | Value | 0 | BattleLeaderRuntime.cpp:796 |  |
| `-BattleFollowSightTraces` | other |  | BattleLeaderRuntime.cpp:830 |  |
| `-BattleHeavyCost` | other |  | BattleState.cpp:543 |  |
| `-BattleHeavyDamage` | other |  | BattleState.cpp:543 |  |
| `-BattleHeavyHealth` | other |  | BattleState.cpp:542 |  |
| `-BattleHeavyMembers` | other |  | BattleState.cpp:542 |  |
| `-BattleHeavySpeed` | other |  | BattleState.cpp:543 |  |
| `-BattleHeightTest` | Param | off | BattleTerrainTest.cpp:27 |  |
| `-BattleHireSightRefresh` | Value | 0 | BattleMixedRecruitment.cpp:112 |  |
| `-BattleHitDirection` | Value | 1 | BattleHUD.cpp:657 |  |
| `-BattleKillLog` | Value | 1 | BattleHUD.cpp:762 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:16 |  |
| `-BattleLeaderHealth` | Value | GetDefault<UBattleTuning>()->LeaderHealthMultiplier | BattleTactics.cpp:23 |  |
| `-BattleLeaderNoControl` | Value | INDEX_NONE | BattleLeaderRuntime.cpp:138 |  |
| `-BattleLeaderQA` | Value |  | BattleLeaderRuntime.cpp:254 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:252 |  |
| `-BattleLeaderUnits` | Value | bMixedFastMatch?20:bWideField?500:40 | BattleLeaderRuntime.cpp:90 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:643 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:100 |  |
| `-BattleMapStats` | Value |  | BattleLeaderRuntime.cpp:230, BattleMapStats.cpp:40 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value |  | BattleMatchSoak.cpp:56 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:18 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:14 |  |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:75 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:216 |  |
| `-BattleMixedSaveSmoke` | Param | off | BattleLeaderRuntime.cpp:218, BattleMixedBodies.cpp:115 |  |
| `-BattleMoveCourse` | Value |  | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:87 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:86 |  |
| `-BattleMovingCapture` | Value |  | BattleSquadTrial.cpp:18 |  |
| `-BattleNetBenchmark` | Param | off | BattleNetwork.cpp:33, BattleNetworkBenchmark.cpp:28, BattleNetworkPilot.cpp:139 |  |
| `-BattleNetBenchmarkPilot` | Param | off | BattleNetworkBenchmark.cpp:30 |  |
| `-BattleNetHoldProbe` | Param | off | BattleNetworkProbe.cpp:9 |  |
| `-BattleNetLeaveAfter` | Value | 0 | BattleNetworkProbe.cpp:10 |  |
| `-BattleNetPilotProbe` | Param | off | BattleNetworkPilot.cpp:139, BattleNetworkPilotProbe.cpp:7 |  |
| `-BattleNetProbe` | Param | off | BattleNetworkProbe.cpp:21 |  |
| `-BattleNetReport` | Value |  | BattleNetworkBenchmark.cpp:112 |  |
| `-BattleNetScreenshot` | Value |  | BattleNetworkBenchmark.cpp:115 |  |
| `-BattleNetSeconds` | Value | 20 | BattleNetworkBenchmark.cpp:73 |  |
| `-BattleNetUnits` | Value | 20 | BattleNetwork.cpp:27, BattleNetworkBenchmark.cpp:83, BattleNetworkPilotProbe.cpp:22 ほか1 |  |
| `-BattleNoAnimationLOD` | Param | off | BattleBenchmark.cpp:157, BattleBenchmark.cpp:158, BattleNetworkBenchmark.cpp:78 ほか1 |  |
| `-BattleNoMuzzleFlash` | Param | off | BattleMuzzleFlash.cpp:65 |  |
| `-BattleNoStagger` | Param | off | BattleBenchmark.cpp:54 |  |
| `-BattleOffscreenMarkers` | Value | 1 | BattleHUD.cpp:695 |  |
| `-BattleOpponentPreset` | other |  | BattleBenchmark.cpp:48 |  |
| `-BattlePeekRangeCheck` | Value | 0 | BattleCoverAction.cpp:24 | trial: projected peeks must be 100..450 cm from their anchor |
| `-BattlePerceptionGrid` | Value | GetDefault<UBattleTuning>()->bPerceptionGrid?1:0 | BattleLeaderRuntime.cpp:105 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value |  | BattlePhase3.cpp:45, BattleUnit.cpp:38, BattleVATDisplay.cpp:393 ほか1 |  |
| `-BattlePinnedFlank` | Value | 0 | BattleLeaderRuntime.cpp:1431 |  |
| `-BattlePlayerRouteTest` | Param | off | BattlePlayerRoutes.cpp:127 |  |
| `-BattlePreset` | other |  | BattleBenchmark.cpp:48 |  |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:81 |  |
| `-BattleRangeEngage` | other |  | BattleRanges.cpp:20 |  |
| `-BattleRangeEngageSupport` | other |  | BattleRanges.cpp:20 |  |
| `-BattleRangeProfile` | other |  | BattleRanges.cpp:14 |  |
| `-BattleRangeReach` | other |  | BattleRanges.cpp:21 |  |
| `-BattleRangeReachSupport` | other |  | BattleRanges.cpp:21 |  |
| `-BattleRangeSight` | other |  | BattleRanges.cpp:18 |  |
| `-BattleReacquireGrace` | Value | GetDefault<UBattleTuning>()->bReacquireGrace?1:0 | BattleTactics.cpp:143 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:94 |  |
| `-BattleReduceHiddenBones` | Value | GetDefault<UBattleTuning>()->bReduceHiddenBones?1:0 | BattleLeaderRuntime.cpp:98 |  |
| `-BattleReplayArm` | Value |  | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value |  | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value |  | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value |  | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:168 |  |
| `-BattleReuseSightContext` | Value | GetDefault<UBattleTuning>()->bReuseSightContext?1:0 | BattleLeaderRuntime.cpp:102 |  |
| `-BattleRifleMembers` | other |  | BattleState.cpp:555 |  |
| `-BattleRoutes` | Value | -1 | BattleRoutes.cpp:17 |  |
| `-BattleScaleScreenshot` | Value |  | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0 / 20 | BattleController.cpp:52, BattleGameMode.cpp:79 |  |
| `-BattleScopeShot` | Param | off | BattleLeaderRuntime.cpp:226 |  |
| `-BattleScoutRetreat` | Value | GetDefault<UBattleTuning>()->bScoutRetreat?1:0 | BattleScoutReport.cpp:20 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSectorAI` | Value | 0 | BattleMatch.cpp:440, BattleMatch.cpp:455, BattleMatch.cpp:466 |  |
| `-BattleSectorAudit` | Param | off | BattleSectorAudit.cpp:28 |  |
| `-BattleSectorContact` | Value | 0 | BattleLeaderRuntime.cpp:1391 |  |
| `-BattleSectorCoverRoute` | Value | 0 | BattleSectorRoute.cpp:22 |  |
| `-BattleSectorCoverWalk` | Value | 0 | BattleLeaderRuntime.cpp:1421 |  |
| `-BattleSectorFlank` | Value | 0 | BattleSectorRoute.cpp:120 |  |
| `-BattleSectorFront` | Value | 0 | BattleSectorRoute.cpp:126 |  |
| `-BattleSectorMove` | Value | 0 | BattleLeaderRuntime.cpp:1396 |  |
| `-BattleSectorMoveFire` | Value | 0 | BattleLeaderRuntime.cpp:1401 |  |
| `-BattleSectorRoute` | Value | 0 | BattleSectorRoute.cpp:16 |  |
| `-BattleSectorSlotReserve` | Value | 1 | BattleStableMovement.cpp:79 |  |
| `-BattleSectorSmoke` | Param | off | BattleLeaderRuntime.cpp:210 |  |
| `-BattleSectorViewShot` | Value/other | Center({X,Y}) | BattleHUD.cpp:808, BattleHUD.cpp:856 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSenseMode` | Value |  | BattleBenchmark.cpp:29 |  |
| `-BattleShotDirection` | Value | 0 | BattleLeaderRuntime.cpp:1446 |  |
| `-BattleSightExperiment` | Value |  | BattleLeaderRuntime.cpp:78 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:383 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:343, BattlePerception.cpp:154 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:153 |  |
| `-BattleSightRange` | Value |  | BattleState.cpp:17 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:113 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:113 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:110 |  |
| `-BattleSightTeamMask` | Value | 0 | BattleLeaderRuntime.cpp:388 |  |
| `-BattleSightTraces` | other |  | BattlePerception.cpp:267 |  |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:234 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:228 |  |
| `-BattleSmokeSuite` | Value |  | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209 ほか1 |  |
| `-BattleSmokeTest` | Param | off | BattleGameMode.cpp:142 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSquadCoverCheck` | Value | 0 | BattleLeaderRuntime.cpp:1406 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value/other |  | BattleGameMode.cpp:64, BattleLeaderRuntime.cpp:73, BattleUnit.cpp:101 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:244 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:83 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:38 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:23 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:224 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:126 |  |
| `-BattleSTScreens` | Value |  | BattleSquadTacticsSmoke.cpp:127 |  |
| `-BattleSupplyAwayRatio` | other |  | BattleTactics.cpp:36 |  |
| `-BattleSupplyHeal` | other |  | BattleTactics.cpp:36 |  |
| `-BattleSupplyResumeHealth` | other |  | BattleTactics.cpp:37 |  |
| `-BattleSupplyResumeLiving` | other |  | BattleTactics.cpp:35 |  |
| `-BattleSupplyReturnLiving` | other |  | BattleTactics.cpp:35 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:242 |  |
| `-BattleSuppression` | other |  | BattleSuppression.cpp:10 |  |
| `-BattleSuppressionDecay` | other |  | BattleSuppression.cpp:12 |  |
| `-BattleSuppressionPerShot` | other |  | BattleSuppression.cpp:11 |  |
| `-BattleSuppressionPin` | other |  | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionRadius` | other |  | BattleSuppression.cpp:11 |  |
| `-BattleSuppressionSpread` | other |  | BattleSuppression.cpp:12 |  |
| `-BattleSuppressionSupport` | other |  | BattleSuppression.cpp:14 |  |
| `-BattleSymmetricStart` | Value | -1 | BattleGroups.cpp:56 |  |
| `-BattleSymmetryAudit` | Param | off | BattleCover.cpp:56 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:12 |  |
| `-BattleTerrain` | other |  | BattleTerrain.h:18 |  |
| `-BattleTerrainLineBodies` | Value | 1 | BattleTactics.cpp:231 |  |
| `-BattleThinkCap` | Value | 0 | BattleLeaderRuntime.cpp:1328 |  |
| `-BattleTracerShot` | Param | off | BattleStanceShot.cpp:87 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:207 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:15, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:80 ほか4 |  |
| `-BattleUnderFireCover` | Value | 0 | BattleLeaderRuntime.cpp:1416 |  |
| `-BattleUnits` | Value | 1000 / 20 | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVATFarLight` | Value | 0 | BattleVATDisplay.cpp:37 |  |
| `-BattleVerifyMixedSave` | Param | off | BattleSave.cpp:253, BattleSightExperiment.cpp:40 |  |
| `-BattleWaypointPass` | Value | 0 | BattleStableMovement.cpp:364 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponPreview` | Value |  | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value |  | BattleState.cpp:42 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:220 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:232 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:74 |  |
| `-BattleWideScreens` | Value |  | BattleAreaSmoke.cpp:836, BattleContactTrial.cpp:97, BattleExoShot.cpp:78 ほか14 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:246 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:248 |  |
