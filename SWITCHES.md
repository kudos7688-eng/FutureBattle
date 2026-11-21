# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値は C++ と DefaultGame.ini の静的読取りによる。解決できない値は「未解決」と記す。
UBattleTuning の Config 値は DefaultGame.ini を優先する。既定は読取り先の初期値で、マップ・陣営・実行時の変更や後処理まで保証しない。Param / Strifind の off は引数なしを示す。読取り箇所で値が異なる場合は場所を併記する。

計 333 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleAIBlindFire` | Value | 0 | BattleBlindFire.cpp:27 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:10 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:1172 |  |
| `-BattleAllyPass` | Value | 0 | BattleStableMovement.cpp:31 |  |
| `-BattleArea` | Param | off | BattleLeaderRuntime.cpp:75, BattleState.cpp:576 |  |
| `-BattleAreaACKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:97 |  |
| `-BattleAreaATKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:95 |  |
| `-BattleAreaAttackWeight` | Value | 2.0 | BattleTactics.cpp:75 |  |
| `-BattleAreaBaseFlank` | Value | 1.3f | BattleTactics.cpp:88 |  |
| `-BattleAreaBaseFlankSide` | Value | true | BattleTactics.cpp:90 |  |
| `-BattleAreaBaseInfiltrate` | Value | 0 | BattleTactics.cpp:84 |  |
| `-BattleAreaBaseLowHP` | Value | 500.f | BattleTactics.cpp:80 |  |
| `-BattleAreaBaseReturnLiving` | Value | 5 | BattleTactics.cpp:91 |  |
| `-BattleAreaBaseRule` | Value | 0 | BattleTactics.cpp:77 |  |
| `-BattleAreaBaseShare` | Value | 0.2 | BattleTactics.cpp:76 |  |
| `-BattleAreaBaseShareHigh` | Value | 0.3f | BattleTactics.cpp:79 |  |
| `-BattleAreaBaseShareLow` | Value | 0.f | BattleTactics.cpp:78 |  |
| `-BattleAreaComposition` | Value | false | BattleArmourAI.cpp:63 |  |
| `-BattleAreaCompositionPlanner` | Value | 0 | BattleComposition.cpp:12 |  |
| `-BattleAreaCompositionPlannerTeams` | Value | 0 | BattleComposition.cpp:13 |  |
| `-BattleAreaDepot` | Value | 0 | BattleAreaDepot.cpp:22 |  |
| `-BattleAreaDepotRaid` | Value | 未解決 | BattleAreaDepot.cpp:23 |  |
| `-BattleAreaExoKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:101 |  |
| `-BattleAreaFocus` | Value | 1.0 | BattleTactics.cpp:86 |  |
| `-BattleAreaFrontBalance` | Value | True | BattleTactics.cpp:81 |  |
| `-BattleAreaFrontGain` | Value | 1.0 | BattleTactics.cpp:82 |  |
| `-BattleAreaGroups` | Value | -1 | BattleGroups.cpp:76 |  |
| `-BattleAreaHeavyShare` | Value | -1.f | BattleTactics.cpp:93 |  |
| `-BattleAreaHire` | Value | True | BattleTactics.cpp:55 |  |
| `-BattleAreaHireInterval` | Value | 0.f | BattleTactics.cpp:57 |  |
| `-BattleAreaHirePurpose` | Value | 0 | BattleTactics.cpp:60 |  |
| `-BattleAreaHirePurposeFront` | Value | 10 | BattleTactics.cpp:61 |  |
| `-BattleAreaHireReserve` | Value | 0 | BattleTactics.cpp:57 |  |
| `-BattleAreaHireStop` | Value | 0.f | BattleTactics.cpp:58 |  |
| `-BattleAreaIncome` | Value | True | BattleTactics.cpp:119 |  |
| `-BattleAreaIncomeBase` | Value | 未解決 | BattleTactics.cpp:122 |  |
| `-BattleAreaIncomePerPoint` | Value | 未解決 | BattleTactics.cpp:122 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:127 |  |
| `-BattleAreaIncomeUrbanBase` | Value | -1 | BattleTactics.cpp:125 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | -1 | BattleTactics.cpp:125 |  |
| `-BattleAreaIntel` | Value | false | BattleAreaIntel.cpp:14 |  |
| `-BattleAreaMaxSquads` | Value | 25 | BattleTactics.cpp:59 |  |
| `-BattleAreaMortarKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:98 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:92 |  |
| `-BattleAreaPointSupply` | Value | False | BattleTactics.cpp:39 |  |
| `-BattleAreaScoutKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:94 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:46, BattleMixedBodies.cpp:114 |  |
| `-BattleAreaSPGCost` | Strifind/Value | 35 | BattleArtillery.cpp:65, BattleHUD.cpp:388 |  |
| `-BattleAreaSPGKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:99 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:128 |  |
| `-BattleAreaStrategy` | Value | True | BattleTactics.cpp:74 |  |
| `-BattleAreaTankKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:96 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:102 |  |
| `-BattleAreaWarPointSeconds` | Value | 5.f | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | Value | 1 | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | Value | 0 | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | Value | 0 | BattleWarScore.cpp:12 |  |
| `-BattleArmourAI` | Value | true | BattleArmourAI.cpp:56 |  |
| `-BattleArmourAITeam` | Value | -1 | BattleArmourAI.cpp:57 |  |
| `-BattleArmourSectorGate` | Value | 0 | BattleWeaponOrders.cpp:16 |  |
| `-BattleArrivalThreat` | Value | 0 | BattleLeaderRuntime.cpp:1441 |  |
| `-BattleArtilleryClusters` | Value | 0 | BattleArtillery.cpp:105 |  |
| `-BattleArtilleryFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:463, BattleArtillery.cpp:52 |  |
| `-BattleArtilleryObserved` | Value | 0 | BattleArtillery.cpp:28 |  |
| `-BattleArtilleryPosts` | Strifind/Value | 0 | BattleAreaSmoke.cpp:462, BattleArtillery.cpp:58 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:54, BattleLeaderRuntime.cpp:71 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleBlastOcclusion` | Value | 0 | BattleArtillery.cpp:99 |  |
| `-BattleBlindFire` | Value | 1 | BattleBlindFire.cpp:26 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireClipSide` | Value | "AS_CoverLo_AimR" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireClipTop` | Value | "AS_CoverLo_AimU_down" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireExpose` | Value | 1 | BattleBlindFire.cpp:77 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireSpread` | Value | 4.f | BattleBlindFire.cpp:30 |  |
| `-BattleBodyAnim` | Value | 1 | BattleBodyAnim.cpp:59 |  |
| `-BattleBounding` | Value | 0 | BattleLeaderRuntime.cpp:1406 |  |
| `-BattleCloseIn` | Value | 0 | BattleLeaderRuntime.cpp:1431 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:83 |  |
| `-BattleContactCover` | Value | 0 | BattleTactics.cpp:186 |  |
| `-BattleContactCoverTeam` | Value | -1 | BattleTactics.cpp:180 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:84 |  |
| `-BattleCorpseLifetime` | Value | 20.f | BattleCorpses.cpp:25 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:192 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:211 |  |
| `-BattleCoverApproach` | Value | -1 | BattleTactics.cpp:174 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:131 |  |
| `-BattleCoverFire` | Value | True | BattleTactics.cpp:150 |  |
| `-BattleCoverHuddle` | Value | 0 | BattleStableMovement.cpp:166 |  |
| `-BattleCoverPeekSight` | Value | True | BattleTactics.cpp:162 |  |
| `-BattleCoverPriority` | Value | 1 | BattleLeaderRuntime.cpp:1416 |  |
| `-BattleCoverStrict` | Value | 0 | BattleLeaderRuntime.cpp:1446 |  |
| `-BattleCoverValidation` | Value | True | BattleTactics.cpp:156 |  |
| `-BattleDangerRoute` | Value | 0 | BattleSectorRoute.cpp:28 |  |
| `-BattleDefendEngage` | Value | false | BattleGroups.cpp:49 |  |
| `-BattleDelegationScreens` | Value | 空文字列 | BattleDelegationSmoke.cpp:90 |  |
| `-BattleDiagnosticOmit` | Value | 空文字列 | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:120, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:485 |  |
| `-BattleExoPlaceholder` | Param | off | BattleExo.cpp:108 |  |
| `-BattleExoShieldSuppression` | Value | 0 | BattleSuppression.cpp:81 |  |
| `-BattleExoShot` | Param | off | BattleLeaderRuntime.cpp:221 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:71 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value | 空文字列 | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value | 空文字列 | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 7 | BattleSquadTrial.cpp:19 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:37 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:213 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:239 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:128, BattleSquadRetreat.cpp:18 |  |
| `-BattleFollowerAnimBudget` | Value | 0 | BattleVATDisplay.cpp:48 |  |
| `-BattleFollowerAnimSkip` | Value | 0 | BattleVATDisplay.cpp:53 |  |
| `-BattleFollowerNoOverlap` | Value | 0 | BattleLeaderRuntime.cpp:811 |  |
| `-BattleGroupStrength` | Value | 0 | BattleGroups.cpp:476 |  |
| `-BattleHeavyCost` | Value | 未解決 | BattleState.cpp:564 |  |
| `-BattleHeavyDamage` | Value | 未解決 | BattleState.cpp:564 |  |
| `-BattleHeavyHealth` | Value | 未解決 | BattleState.cpp:563 |  |
| `-BattleHeavyMembers` | Value | 未解決 | BattleState.cpp:563 |  |
| `-BattleHeavySpeed` | Value | 未解決 | BattleState.cpp:564 |  |
| `-BattleHeightTest` | Param | off | BattleTerrainTest.cpp:27 |  |
| `-BattleHireSightRefresh` | Value | 0 | BattleMixedRecruitment.cpp:112 |  |
| `-BattleHitDirection` | Value | 1 | BattleHUD.cpp:671 |  |
| `-BattleKillLog` | Value | 1 | BattleHUD.cpp:776 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:16 |  |
| `-BattleLeaderHealth` | Value | 3.0 | BattleTactics.cpp:23 |  |
| `-BattleLeaderNoControl` | Value | 未解決 | BattleLeaderRuntime.cpp:137 |  |
| `-BattleLeaderQA` | Value | 空文字列 | BattleLeaderRuntime.cpp:253 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:251 |  |
| `-BattleLeaderUnits` | Value | 未解決 | BattleLeaderRuntime.cpp:89 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:663 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:100 |  |
| `-BattleMapStats` | Value | 空文字列 | BattleLeaderRuntime.cpp:229, BattleMapStats.cpp:40 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value | 空文字列 | BattleMatchSoak.cpp:56 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:18 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:14 |  |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:74 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:215 |  |
| `-BattleMixedSaveSmoke` | Param | off | BattleArtillery.cpp:87, BattleLeaderRuntime.cpp:217, BattleMixedBodies.cpp:114, BattleSave.cpp:72 |  |
| `-BattleMoveCourse` | Value | 空文字列 | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:86 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:85 |  |
| `-BattleMovingCapture` | Value | 空文字列 | BattleSquadTrial.cpp:18 |  |
| `-BattleNetBenchmark` | Param | off | BattleNetwork.cpp:33, BattleNetworkBenchmark.cpp:28, BattleNetworkPilot.cpp:139 |  |
| `-BattleNetBenchmarkPilot` | Param | off | BattleNetworkBenchmark.cpp:30 |  |
| `-BattleNetHoldProbe` | Param | off | BattleNetworkProbe.cpp:9 |  |
| `-BattleNetLeaveAfter` | Value | 0 | BattleNetworkProbe.cpp:10 |  |
| `-BattleNetPilotProbe` | Param | off | BattleNetworkPilot.cpp:139, BattleNetworkPilotProbe.cpp:7 |  |
| `-BattleNetProbe` | Param | off | BattleNetworkProbe.cpp:21 |  |
| `-BattleNetReport` | Value | 空文字列 | BattleNetworkBenchmark.cpp:112 |  |
| `-BattleNetScreenshot` | Value | 空文字列 | BattleNetworkBenchmark.cpp:115 |  |
| `-BattleNetSeconds` | Value | 20 | BattleNetworkBenchmark.cpp:73 |  |
| `-BattleNetUnits` | Value | 20 | BattleNetwork.cpp:27, BattleNetworkBenchmark.cpp:83, BattleNetworkPilotProbe.cpp:22, BattleNetworkProbe.cpp:23 |  |
| `-BattleNoAnimationLOD` | Param | off | BattleBenchmark.cpp:157, BattleBenchmark.cpp:158, BattleNetworkBenchmark.cpp:78, BattleUnit.cpp:656 |  |
| `-BattleNoCoverFallback` | Value | 0 | BattleLeaderRuntime.cpp:1411 |  |
| `-BattleNoMuzzleFlash` | Param | off | BattleMuzzleFlash.cpp:65 |  |
| `-BattleNoStagger` | Param | off | BattleBenchmark.cpp:54 |  |
| `-BattleObservedArmour` | Strifind/Value | 0 | BattleAreaSmoke.cpp:459, BattleArmourAI.cpp:34 |  |
| `-BattleOffscreenMarkers` | Value | 1 | BattleHUD.cpp:709 |  |
| `-BattleOpponentPreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattlePeekRangeCheck` | Value | 0 | BattleCoverAction.cpp:24 | trial: projected peeks must be 100..450 cm from their anchor |
| `-BattlePerceptionGrid` | Value | True | BattleLeaderRuntime.cpp:104 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value | 空文字列 | BattlePhase3.cpp:45, BattleUnit.cpp:38, BattleVATDisplay.cpp:393, BattleVATDisplay.cpp:396 |  |
| `-BattlePinnedFlank` | Value | 0 | BattleLeaderRuntime.cpp:1436 |  |
| `-BattlePlayerRouteTest` | Param | off | BattlePlayerRoutes.cpp:128 |  |
| `-BattlePreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:80 |  |
| `-BattleRangeEngage` | Value | 15000.f | BattleRanges.cpp:20 |  |
| `-BattleRangeEngageSupport` | Value | 25000.f | BattleRanges.cpp:20 |  |
| `-BattleRangeProfile` | Value | -1 | BattleRanges.cpp:14 |  |
| `-BattleRangeReach` | Value | 50000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeReachSupport` | Value | 60000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeSight` | Value | 20000.f | BattleRanges.cpp:18 |  |
| `-BattleReacquireGrace` | Value | True | BattleTactics.cpp:143 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:93 |  |
| `-BattleReduceHiddenBones` | Value | True | BattleLeaderRuntime.cpp:97 |  |
| `-BattleReplayArm` | Value | 空文字列 | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value | 空文字列 | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value | 空文字列 | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value | 空文字列 | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:168 |  |
| `-BattleReuseSightContext` | Value | True | BattleLeaderRuntime.cpp:101 |  |
| `-BattleRifleMembers` | Value | 未解決 | BattleState.cpp:576 |  |
| `-BattleRoutes` | Value | -1 | BattleRoutes.cpp:17 |  |
| `-BattleSaveReloadBoundary` | Value | 0 | BattleArtillery.cpp:81 |  |
| `-BattleSaveRoutes` | Value | 0 | BattleSave.cpp:65 |  |
| `-BattleScaleScreenshot` | Value | 空文字列 | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0（BattleController.cpp:53） / 20（BattleGameMode.cpp:79） | BattleController.cpp:53, BattleGameMode.cpp:79 |  |
| `-BattleScopeShot` | Param | off | BattleLeaderRuntime.cpp:225 |  |
| `-BattleScoutRetreat` | Value | true | BattleScoutReport.cpp:20 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSectorAI` | Value | 0 | BattleMatch.cpp:446, BattleMatch.cpp:461, BattleMatch.cpp:472 |  |
| `-BattleSectorAudit` | Param | off | BattleSectorAudit.cpp:28 |  |
| `-BattleSectorContact` | Value | 0 | BattleLeaderRuntime.cpp:1386 |  |
| `-BattleSectorCoverRoute` | Value | 0 | BattleSectorRoute.cpp:22 |  |
| `-BattleSectorCoverWalk` | Value | 0 | BattleLeaderRuntime.cpp:1426 |  |
| `-BattleSectorFlank` | Value | 0 | BattleSectorRoute.cpp:120 |  |
| `-BattleSectorFront` | Value | 0 | BattleSectorRoute.cpp:126 |  |
| `-BattleSectorMove` | Value | 0 | BattleLeaderRuntime.cpp:1391 |  |
| `-BattleSectorMoveFire` | Value | 0 | BattleLeaderRuntime.cpp:1396 |  |
| `-BattleSectorRoute` | Value | 0 | BattleSectorRoute.cpp:16 |  |
| `-BattleSectorSlotReserve` | Value | 1 | BattleStableMovement.cpp:79 |  |
| `-BattleSectorSmoke` | Param | off | BattleLeaderRuntime.cpp:209 |  |
| `-BattleSectorViewShot` | Value | 空文字列 | BattleHUD.cpp:822 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSenseMode` | Value | 空文字列 | BattleBenchmark.cpp:29 |  |
| `-BattleShotDirection` | Value | 0 | BattleLeaderRuntime.cpp:1451 |  |
| `-BattleSightExperiment` | Value | 未解決 | BattleLeaderRuntime.cpp:77 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:382 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:342, BattlePerception.cpp:164 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:163 |  |
| `-BattleSightRange` | Value | 空文字列 | BattleState.cpp:18 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:112 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:112 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:109 |  |
| `-BattleSightTeamMask` | Value | 0 | BattleLeaderRuntime.cpp:387 |  |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:233 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:227 |  |
| `-BattleSmokeSuite` | Value | 空文字列 | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209, BattleTacticalSmoke.cpp:175 |  |
| `-BattleSmokeTest` | Param | off | BattleGameMode.cpp:142 |  |
| `-BattleSPGAmmo` | Value | 0 | BattleArtillery.cpp:93 |  |
| `-BattleSPGRange` | Strifind/Value | 100000.f | BattleAreaSmoke.cpp:464, BattleArtillery.cpp:72 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSquadCoverCheck` | Value | 0 | BattleLeaderRuntime.cpp:1401 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value | 空文字列 | BattleGameMode.cpp:64, BattleUnit.cpp:101 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:243 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:82 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:38 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:23 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:223 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:126 |  |
| `-BattleSTScreens` | Value | 空文字列 | BattleSquadTacticsSmoke.cpp:127 |  |
| `-BattleSupplyAwayRatio` | Value | 1.0 | BattleTactics.cpp:36 |  |
| `-BattleSupplyHeal` | Value | 10.0 | BattleTactics.cpp:36 |  |
| `-BattleSupplyResumeHealth` | Value | 0.9 | BattleTactics.cpp:37 |  |
| `-BattleSupplyResumeLiving` | Value | 10 | BattleTactics.cpp:35 |  |
| `-BattleSupplyReturnLiving` | Value | 8 | BattleTactics.cpp:35 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:241 |  |
| `-BattleSuppression` | Value | 0 | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionDecay` | Value | .25f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionPerShot` | Value | .12f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionPin` | Value | .5f | BattleSuppression.cpp:16 |  |
| `-BattleSuppressionRadius` | Value | 200.f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionSpread` | Value | 1.5f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionSupport` | Value | 1.f | BattleSuppression.cpp:17 |  |
| `-BattleSymmetricStart` | Value | -1 | BattleGroups.cpp:58 |  |
| `-BattleSymmetryAudit` | Param | off | BattleCover.cpp:56 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTankGunFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:461, BattleArtillery.cpp:46 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:12 |  |
| `-BattleTerrainLineBodies` | Value | 1 | BattleTactics.cpp:231 |  |
| `-BattleThinkCap` | Value | 0 | BattleLeaderRuntime.cpp:1324 |  |
| `-BattleTracerShot` | Param | off | BattleStanceShot.cpp:87 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:207 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:16, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:79, BattleMatch.cpp:27, BattleMatch.cpp:271, BattleMatch.cpp:290, BattleWideMapAudit.cpp:116 |  |
| `-BattleUnderFireCover` | Value | 0 | BattleLeaderRuntime.cpp:1421 |  |
| `-BattleUnits` | Value | 20（BattleBenchmark.cpp:36） / 1000（BattleSquadTrial.cpp:28） | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVATFarLight` | Value | 0 | BattleVATDisplay.cpp:37 |  |
| `-BattleVehicleHitCover` | Strifind/Value | 0 | BattleAreaSmoke.cpp:460, BattleArtillery.cpp:40 |  |
| `-BattleVehicleOrderHere` | Value | 0 | BattleArtillery.cpp:34 |  |
| `-BattleVerifyMixedSave` | Param | off | BattleSave.cpp:445, BattleSightExperiment.cpp:40 |  |
| `-BattleWaypointPass` | Value | 0 | BattleStableMovement.cpp:364 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponOrderGate` | Value | 0 | BattleWeaponOrders.cpp:10 |  |
| `-BattleWeaponPreview` | Value | 空文字列 | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value | 空文字列 | BattleState.cpp:43 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:219 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:231 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:73 |  |
| `-BattleWideScreens` | Value | 空文字列 | BattleAreaSmoke.cpp:1181, BattleContactTrial.cpp:97, BattleExoShot.cpp:78, BattleMixedFastSmoke.cpp:34, BattleMixedSaveSmoke.cpp:286, BattleMovementTrial.cpp:108, BattleScopeShot.cpp:45, BattleSightVisual.cpp:30, BattleSquadFlankSmoke.cpp:165, BattleSquadRetreatSmoke.cpp:20, BattleStanceShot.cpp:96, BattleTeamSupportSmoke.cpp:122, BattleWideMapAudit.cpp:40, BattleWideMapAudit.cpp:159, BattleWideMapAudit.cpp:190, BattleWideMixedSmoke.cpp:39, BattleWideMixedSmoke.cpp:53 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:245 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:247 |  |
| `-BayFlank` | Value | 0 | BattleFlankBayTrial.cpp:17 |  |
| `-BayOutput` | Value | 空文字列 | BattleFlankBayTrial.cpp:62 |  |
| `-BayRendered` | Param | off | BattleFlankBayTrial.cpp:66 |  |
| `-BaySign` | Value | 1 | BattleFlankBayTrial.cpp:17 |  |
| `-CoverScreenshot` | Value | 空文字列 | BattleCoverActionSmoke.cpp:32, BattleCoverActionSmoke.cpp:246 |  |
| `-noraytracing` | Param | off | BattleRenderDiagnostic.cpp:41 |  |
| `-nullrhi` | Param | off | BattleBenchmark.cpp:162, BattleBenchmark.cpp:187, BattleNetworkBenchmark.cpp:85, BattleNetworkBenchmark.cpp:116, BattlePhase3.cpp:176 |  |
| `-P3AnimTaskAudit` | Param | off | BattlePhase3.cpp:106 |  |
| `-P3BoneAudit` | Param | off | BattlePhase3.cpp:101 |  |
| `-P3Capture` | Param | off | BattlePhase3.cpp:182 |  |
| `-P3ContextAudit` | Param | off | BattlePhase3.cpp:50 |  |
| `-P3ContextReplay` | Param | off | BattlePhase3.cpp:51 |  |
| `-P3Decision` | Value | "Leader" | BattlePhase3.cpp:63 |  |
| `-P3Duration` | Value | 60 | BattlePhase3.cpp:65 |  |
| `-P3Functional` | Param | off | BattlePhase3.cpp:354 |  |
| `-P3GridAudit` | Param | off | BattlePhase3.cpp:52 |  |
| `-P3GridFixture` | Param | off | BattlePhase3.cpp:100 |  |
| `-P3Layout` | Value | "Open" | BattlePhase3.cpp:64 |  |
| `-P3Seed` | Value | 1 | BattlePhase3.cpp:65 |  |
| `-P3Sense` | Value | "A" | BattlePhase3.cpp:47 |  |
| `-P3SenseAudit` | Param | off | BattlePhase3.cpp:49 |  |
| `-P3Stable` | Param | off | BattlePhase3.cpp:66 |  |
| `-P3VATAudit` | Param | off | BattleLeaderRuntime.cpp:123, BattleVATDisplay.cpp:139 |  |
| `-P3View` | Value | "RTS" | BattlePhase3.cpp:64 |  |
| `-P3Warmup` | Value | 10 | BattlePhase3.cpp:65 |  |
| `-PktLag` | Value | 0 | BattleNetworkBenchmark.cpp:80 |  |
| `-PktLoss` | Value | 0 | BattleNetworkBenchmark.cpp:80 |  |
| `-ResX` | Value | 1920（BattleLeaderSmoke.cpp:102） / 1280（BattleWideMixedSmoke.cpp:167） | BattleLeaderSmoke.cpp:102, BattleWideMixedSmoke.cpp:167 |  |
| `-ResY` | Value | 1080（BattleLeaderSmoke.cpp:102） / 720（BattleWideMixedSmoke.cpp:167） | BattleLeaderSmoke.cpp:102, BattleWideMixedSmoke.cpp:167 |  |
| `-T2a-Allocation` | Value | "Off" | BattleSquadTrial.cpp:20 |  |
| `-T2a-Headless` | Param | off | BattleSquadTrial.cpp:22 |  |
| `-T2a-Metrics` | Param | off | BattleSquadTrial.cpp:21 |  |
| `-T2b-Headless` | Param | off | BattleFieldTurn.cpp:15 |  |
| `-T2b-Metrics` | Param | off | BattleFieldTurn.cpp:14 |  |
| `-T2b-TurnRate` | Value | 未解決 | BattleFieldTurn.cpp:19 |  |
| `-T2b-TurnToFace` | Value | "Off" | BattleFieldTurn.cpp:11 |  |
| `-T5-CaptureFrame` | Value | 450 | BattleFieldViewTrial.cpp:210 |  |
| `-T5-Conditions` | Value | 30（BattleFieldViewTrial.cpp:61） / 未解決（BattleRenderDiagnostic.cpp:90） | BattleFieldViewTrial.cpp:61, BattleRenderDiagnostic.cpp:90 |  |
| `-T5-Filters` | Value | 空文字列 | BattleRenderDiagnostic.cpp:84 |  |
| `-T5-Hysteresis` | Value | 10. | BattleRenderDiagnostic.cpp:64 |  |
| `-T5-LODPolicy` | Value | 未解決 | BattleRenderDiagnostic.cpp:61 |  |
| `-T5-LODStress` | Param | off | BattleRenderDiagnostic.cpp:63 |  |
| `-T5-Profile` | Value | "Current" | BattleRenderDiagnostic.cpp:37 |  |
| `-T5-Replay` | Param | off | BattleFieldReplay.cpp:57 |  |
| `-T5-ShadowQA` | Param | off | BattleFieldViewTrial.cpp:157 |  |
| `-T5-Stage` | Value | 空文字列 | BattleRenderDiagnostic.cpp:35 |  |
| `-T5-VisualQA` | Param | off | BattleDisplayLODTrial.cpp:29, BattleFieldViewTrial.cpp:157, BattleFieldViewTrial.cpp:173, BattleFieldViewTrial.cpp:211, BattleFieldViewTrial.cpp:243 |  |
| `-TPSVisibilityScreens` | Value | 空文字列 | BattleThirdPersonSmoke.cpp:87 |  |
