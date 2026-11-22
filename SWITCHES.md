# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値は C++ と DefaultGame.ini の静的読取りによる。解決できない値は「未解決」と記す。
UBattleTuning の Config 値は DefaultGame.ini を優先する。既定は読取り先の初期値で、マップ・陣営・実行時の変更や後処理まで保証しない。Param / Strifind の off は引数なしを示す。読取り箇所で値が異なる場合は場所を併記する。

計 375 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleAIBlindFire` | Value | 0 | BattleBlindFire.cpp:27 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:10 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:1405 |  |
| `-BattleAllyPass` | Value | 0 | BattleStableMovement.cpp:32 |  |
| `-BattleArea` | Param | off | BattleLeaderRuntime.cpp:77, BattleState.cpp:581 |  |
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
| `-BattleAreaComposition` | Value | false | BattleArmourAI.cpp:69 |  |
| `-BattleAreaCompositionPlanner` | Value | 0 | BattleComposition.cpp:12 |  |
| `-BattleAreaCompositionPlannerTeams` | Value | 0 | BattleComposition.cpp:13 |  |
| `-BattleAreaDepot` | Value | 0 | BattleAreaDepot.cpp:22 |  |
| `-BattleAreaDepotRaid` | Value | 未解決 | BattleAreaDepot.cpp:23 |  |
| `-BattleAreaEliteKeep` | Value | 0 | BattleArms.cpp:41 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
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
| `-BattleAreaIncome` | Value | True | BattleTactics.cpp:121 |  |
| `-BattleAreaIncomeBase` | Value | 未解決 | BattleTactics.cpp:124 |  |
| `-BattleAreaIncomePerPoint` | Value | 未解決 | BattleTactics.cpp:124 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:129 |  |
| `-BattleAreaIncomeUrbanBase` | Value | -1 | BattleTactics.cpp:127 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | -1 | BattleTactics.cpp:127 |  |
| `-BattleAreaIntel` | Value | false | BattleAreaIntel.cpp:15 |  |
| `-BattleAreaMaxSquads` | Value | 30 | BattleTactics.cpp:59 |  |
| `-BattleAreaMortarKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:98 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:92 |  |
| `-BattleAreaPointSupply` | Value | False | BattleTactics.cpp:39 |  |
| `-BattleAreaScoutKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:94 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:47, BattleMixedBodies.cpp:114 |  |
| `-BattleAreaSniperKeep` | Value | 0 | BattleArms.cpp:44 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleAreaSPGCost` | Strifind/Value | 35 | BattleArtillery.cpp:67, BattleController.cpp:44 |  |
| `-BattleAreaSPGKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:99 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:130 |  |
| `-BattleAreaStrategy` | Value | True | BattleTactics.cpp:74 |  |
| `-BattleAreaSupplyKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:103 |  |
| `-BattleAreaTankKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:96 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:104 |  |
| `-BattleAreaWarPointSeconds` | Value | 5.f | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | Value | 1 | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | Value | 0 | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | Value | 0 | BattleWarScore.cpp:12 |  |
| `-BattleArmourAI` | Value | true | BattleArmourAI.cpp:62 |  |
| `-BattleArmourAITeam` | Value | -1 | BattleArmourAI.cpp:63 |  |
| `-BattleArmourSectorGate` | Value | 0 | BattleWeaponOrders.cpp:16 |  |
| `-BattleArmsAssault` | Value | 0 | BattleArms.cpp:46 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleArmsCharge` | Value | 0 | BattleArms.cpp:47 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleArmsSmoke` | Param | off | BattleArms.cpp:48 |  |
| `-BattleArrivalThreat` | Value | 0 | BattleLeaderRuntime.cpp:1608 |  |
| `-BattleArrivalWalk` | Value | 1 | BattleLeaderRuntime.cpp:1469 |  |
| `-BattleArtilleryClusters` | Value | 0 | BattleArtillery.cpp:107 |  |
| `-BattleArtilleryFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:470, BattleArtillery.cpp:54 |  |
| `-BattleArtilleryObserved` | Value | 0 | BattleArtillery.cpp:30 |  |
| `-BattleArtilleryPosts` | Strifind/Value | 0 | BattleAreaSmoke.cpp:469, BattleArtillery.cpp:60 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:102, BattleLeaderRuntime.cpp:73 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleBlastOcclusion` | Value | 0 | BattleArtillery.cpp:101 |  |
| `-BattleBlindFire` | Value | 1 | BattleBlindFire.cpp:26 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireClipSide` | Value | "AS_CoverLo_AimR" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireClipTop` | Value | "AS_CoverLo_AimU_down" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireExpose` | Value | 1 | BattleBlindFire.cpp:77 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireSpread` | Value | 4.f | BattleBlindFire.cpp:30 |  |
| `-BattleBodyAnim` | Value | 1 | BattleBodyAnim.cpp:59 |  |
| `-BattleBounding` | Value | 0 | BattleLeaderRuntime.cpp:1484 |  |
| `-BattleCloseIn` | Value | 0 | BattleLeaderRuntime.cpp:1583 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:85 |  |
| `-BattleContactCover` | Value | 0 | BattleTactics.cpp:188 |  |
| `-BattleContactCoverTeam` | Value | -1 | BattleTactics.cpp:182 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:86 |  |
| `-BattleCorpseLifetime` | Value | 20.f | BattleCorpses.cpp:25 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:194 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:213 |  |
| `-BattleCoverApproach` | Value | -1 | BattleTactics.cpp:176 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:132 |  |
| `-BattleCoverDense` | Value | 0 | BattleLeaderRuntime.cpp:1593 |  |
| `-BattleCoverFire` | Value | True | BattleTactics.cpp:152 |  |
| `-BattleCoverHuddle` | Value | 0 | BattleStableMovement.cpp:218 |  |
| `-BattleCoverIndoor` | Value | 0 | BattleLeaderRuntime.cpp:1598 |  |
| `-BattleCoverOutward` | Value | 0 | BattleLeaderRuntime.cpp:1603 |  |
| `-BattleCoverPeekSight` | Value | True | BattleTactics.cpp:164 |  |
| `-BattleCoverPriority` | Value | 1 | BattleLeaderRuntime.cpp:1568 |  |
| `-BattleCoverStrict` | Value | 0 | BattleLeaderRuntime.cpp:1613 |  |
| `-BattleCoverValidation` | Value | True | BattleTactics.cpp:158 |  |
| `-BattleDangerRoute` | Value | 0 | BattleSectorRoute.cpp:28 |  |
| `-BattleDeathShot` | Param | off | BattleLeaderRuntime.cpp:225 |  |
| `-BattleDefendEngage` | Value | false | BattleGroups.cpp:49 |  |
| `-BattleDelegationScreens` | Value | 空文字列 | BattleDelegationSmoke.cpp:90 |  |
| `-BattleDiagnosticOmit` | Value | 空文字列 | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:122, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleElite` | Value | 0 | BattleArms.cpp:40 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleEliteCost` | Value | 0 | BattleArms.cpp:42 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:582 |  |
| `-BattleExoBody` | Value | 3 | BattleExo.cpp:116 |  |
| `-BattleExoPlaceholder` | Param | off | BattleExo.cpp:108 |  |
| `-BattleExoShieldSuppression` | Value | 0 | BattleSuppression.cpp:81 |  |
| `-BattleExoShot` | Param | off | BattleLeaderRuntime.cpp:223 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:73 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value | 空文字列 | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value | 空文字列 | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 7 | BattleSquadTrial.cpp:19 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:39 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:215 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:243 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:128, BattleSquadRetreat.cpp:18 |  |
| `-BattleFollowerAnimBudget` | Value | 0 | BattleVATDisplay.cpp:48 |  |
| `-BattleFollowerAnimSkip` | Value | 0 | BattleVATDisplay.cpp:53 |  |
| `-BattleFollowerNoOverlap` | Value | 0 | BattleLeaderRuntime.cpp:819 |  |
| `-BattleFrontArmy` | Value | 1 | BattleFrontLine.cpp:33 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontAuto` | Value | 1 | BattleFrontLine.cpp:38 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontGap` | Value | 1 | BattleFrontLine.cpp:39 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontHold` | Value | 1 | BattleFrontLine.cpp:45 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontLine` | Value | 1 | BattleFrontLine.cpp:64 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontSquads` | Value | 4 | BattleFrontLine.cpp:163 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleGrenadeDamage` | Value | 90.f | BattleArms.cpp:141 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleGrenadeRadius` | Value | 500.f | BattleArms.cpp:141 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleGrenadeRange` | Value | 3000.f | BattleArms.cpp:142 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleGrenades` | Value | 0 | BattleArms.cpp:38 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleGrenadeSquadGap` | Value | 3.f | BattleArms.cpp:151 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleGroupFireSupport` | Value | 1 | BattleArtillery.cpp:202, BattleGroups.cpp:290 | default 1 (user 2026-11-21) |
| `-BattleGroupStrength` | Value | 0 | BattleGroups.cpp:533 |  |
| `-BattleHeavyCost` | Value | 未解決 | BattleState.cpp:569 |  |
| `-BattleHeavyDamage` | Value | 未解決 | BattleState.cpp:569 |  |
| `-BattleHeavyHealth` | Value | 未解決 | BattleState.cpp:568 |  |
| `-BattleHeavyMembers` | Value | 未解決 | BattleState.cpp:568 |  |
| `-BattleHeavySpeed` | Value | 未解決 | BattleState.cpp:569 |  |
| `-BattleHeightTest` | Param | off | BattleTerrainTest.cpp:27 |  |
| `-BattleHireSightRefresh` | Value | 0 | BattleMixedRecruitment.cpp:120 |  |
| `-BattleHitDirection` | Value | 1 | BattleHUD.cpp:1076 |  |
| `-BattleHitFace` | Value | 0 | BattleLeaderRuntime.cpp:1494 |  |
| `-BattleHitReveal` | Value | 0 | BattleLeaderRuntime.cpp:1499 |  |
| `-BattleKillLog` | Value | 1 | BattleHUD.cpp:1181 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLauncherDamage` | Value | 60.f | BattleArms.cpp:148 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:16 |  |
| `-BattleLeaderHealth` | Value | 3.0 | BattleTactics.cpp:23 |  |
| `-BattleLeaderNoControl` | Value | 未解決 | BattleLeaderRuntime.cpp:139 |  |
| `-BattleLeaderQA` | Value | 空文字列 | BattleLeaderRuntime.cpp:257 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:255 |  |
| `-BattleLeaderUnits` | Value | 未解決 | BattleLeaderRuntime.cpp:91 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:698 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:102 |  |
| `-BattleMapStats` | Value | 空文字列 | BattleLeaderRuntime.cpp:233, BattleMapStats.cpp:40 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value | 空文字列 | BattleMatchSoak.cpp:56 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:18 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:14 |  |
| `-BattleMedicRate` | Value | 4.f | BattleArms.cpp:160 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleMelee` | Value | 0 | BattleArms.cpp:39 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleMeleeDamage` | Value | 60.f | BattleArms.cpp:154 | FloatSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:76 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:217 |  |
| `-BattleMixedSaveSmoke` | Param | off | BattleArtillery.cpp:89, BattleLeaderRuntime.cpp:219, BattleMixedBodies.cpp:114, BattleSave.cpp:93 |  |
| `-BattleMoveCourse` | Value | 空文字列 | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:88 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:87 |  |
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
| `-BattleNoAnimationLOD` | Param | off | BattleBenchmark.cpp:157, BattleBenchmark.cpp:158, BattleNetworkBenchmark.cpp:78, BattleUnit.cpp:691 |  |
| `-BattleNoCoverFallback` | Value | 0 | BattleLeaderRuntime.cpp:1489 |  |
| `-BattleNoMuzzleFlash` | Param | off | BattleMuzzleFlash.cpp:65 |  |
| `-BattleNoStagger` | Param | off | BattleBenchmark.cpp:54 |  |
| `-BattleObservedArmour` | Strifind/Value | 0 | BattleAreaSmoke.cpp:466, BattleArmourAI.cpp:34 |  |
| `-BattleObservedTactics` | Strifind/Value | 0 | BattleAreaSmoke.cpp:465, BattleArmourAI.cpp:40 |  |
| `-BattleOffscreenMarkers` | Value | 1 | BattleHUD.cpp:1114 |  |
| `-BattleOpponentPreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattlePeekRangeCheck` | Value | 0 | BattleCoverAction.cpp:24 | trial: projected peeks must be 100..450 cm from their anchor |
| `-BattlePerceptionGrid` | Value | True | BattleLeaderRuntime.cpp:106 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value | 空文字列 | BattlePhase3.cpp:45, BattleUnit.cpp:40, BattleVATDisplay.cpp:407, BattleVATDisplay.cpp:410 |  |
| `-BattlePinnedFlank` | Value | 0 | BattleLeaderRuntime.cpp:1588 |  |
| `-BattlePlayerRouteTest` | Param | off | BattlePlayerRoutes.cpp:128 |  |
| `-BattlePreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleRagdoll` | Value | 1 | BattleRagdoll.cpp:9 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:82 |  |
| `-BattleRangeEngage` | Value | 15000.f | BattleRanges.cpp:20 |  |
| `-BattleRangeEngageSupport` | Value | 25000.f | BattleRanges.cpp:20 |  |
| `-BattleRangeProfile` | Value | -1 | BattleRanges.cpp:14 |  |
| `-BattleRangeReach` | Value | 50000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeReachSupport` | Value | 60000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeRings` | Value | 1 | BattleHUD.cpp:736 |  |
| `-BattleRangeRingShot` | Value | 空文字列 | BattleHUD.cpp:811 |  |
| `-BattleRangeSight` | Value | 20000.f | BattleRanges.cpp:18 |  |
| `-BattleReacquireGrace` | Value | True | BattleTactics.cpp:145 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:95 |  |
| `-BattleReduceHiddenBones` | Value | True | BattleLeaderRuntime.cpp:99 |  |
| `-BattleReplayArm` | Value | 空文字列 | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value | 空文字列 | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value | 空文字列 | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value | 空文字列 | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:170 |  |
| `-BattleReuseSightContext` | Value | True | BattleLeaderRuntime.cpp:103 |  |
| `-BattleRifleMembers` | Value | 未解決 | BattleState.cpp:581 |  |
| `-BattleRoutes` | Value | -1 | BattleRoutes.cpp:17 |  |
| `-BattleSaveReloadBoundary` | Value | 0 | BattleArtillery.cpp:83 |  |
| `-BattleSaveRoutes` | Value | 0 | BattleSave.cpp:86 |  |
| `-BattleScaleScreenshot` | Value | 空文字列 | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0（BattleController.cpp:101） / 20（BattleGameMode.cpp:79） | BattleController.cpp:101, BattleGameMode.cpp:79 |  |
| `-BattleScopeShot` | Param | off | BattleLeaderRuntime.cpp:229 |  |
| `-BattleScoutRetreat` | Value | true | BattleScoutReport.cpp:22 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSectorAI` | Value | 0 | BattleMatch.cpp:451, BattleMatch.cpp:466, BattleMatch.cpp:477 |  |
| `-BattleSectorAudit` | Param | off | BattleSectorAudit.cpp:28 |  |
| `-BattleSectorContact` | Value | 0 | BattleLeaderRuntime.cpp:1459 |  |
| `-BattleSectorCoverRoute` | Value | 0 | BattleSectorRoute.cpp:22 |  |
| `-BattleSectorCoverWalk` | Value | 0 | BattleLeaderRuntime.cpp:1578 |  |
| `-BattleSectorFlank` | Value | 0 | BattleSectorRoute.cpp:120 |  |
| `-BattleSectorFront` | Value | 0 | BattleSectorRoute.cpp:126 |  |
| `-BattleSectorMove` | Value | 0 | BattleLeaderRuntime.cpp:1464 |  |
| `-BattleSectorMoveFire` | Value | 0 | BattleLeaderRuntime.cpp:1474 |  |
| `-BattleSectorRoute` | Value | 0 | BattleSectorRoute.cpp:16 |  |
| `-BattleSectorSlotReserve` | Value | 1 | BattleStableMovement.cpp:80 |  |
| `-BattleSectorSmoke` | Param | off | BattleLeaderRuntime.cpp:211 |  |
| `-BattleSectorViewShot` | Value | 空文字列 | BattleHUD.cpp:1230 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSenseMode` | Value | 空文字列 | BattleBenchmark.cpp:29 |  |
| `-BattleShotDirection` | Value | 0 | BattleLeaderRuntime.cpp:1618 |  |
| `-BattleSidearms` | Value | 0 | BattleArms.cpp:37 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleSightExperiment` | Value | 未解決 | BattleLeaderRuntime.cpp:79 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:386 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:346, BattlePerception.cpp:170 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:169 |  |
| `-BattleSightRange` | Value | 空文字列 | BattleState.cpp:19 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:114 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:114 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:111 |  |
| `-BattleSightTeamMask` | Value | 0 | BattleLeaderRuntime.cpp:391 |  |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:237 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:231 |  |
| `-BattleSmokeSuite` | Value | 空文字列 | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209, BattleTacticalSmoke.cpp:175 |  |
| `-BattleSmokeTest` | Param | off | BattleGameMode.cpp:142 |  |
| `-BattleSniperCost` | Value | 0 | BattleArms.cpp:45 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleSniperTeam` | Value | 0 | BattleArms.cpp:43 | IntSwitch → BattleArms.cpp:33（ヘルパー経由） |
| `-BattleSPGAmmo` | Value | 0 | BattleArtillery.cpp:95 |  |
| `-BattleSPGRange` | Strifind/Value | 100000.f | BattleAreaSmoke.cpp:471, BattleArtillery.cpp:74 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSquadCoverCheck` | Value | 0 | BattleLeaderRuntime.cpp:1479 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadIcons` | Value | 1 | BattleHUD.cpp:215, BattleHUD.cpp:926 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value | 空文字列 | BattleGameMode.cpp:64, BattleUnit.cpp:103 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:247 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:84 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:41 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:26 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:227 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:126 |  |
| `-BattleSTScreens` | Value | 空文字列 | BattleSquadTacticsSmoke.cpp:127 |  |
| `-BattleSupplyAwayRatio` | Value | 1.0 | BattleTactics.cpp:36 |  |
| `-BattleSupplyHeal` | Value | 10.0 | BattleTactics.cpp:36 |  |
| `-BattleSupplyResumeHealth` | Value | 0.9 | BattleTactics.cpp:37 |  |
| `-BattleSupplyResumeLiving` | Value | 10 | BattleTactics.cpp:35 |  |
| `-BattleSupplyReturnLiving` | Value | 8 | BattleTactics.cpp:35 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:245 |  |
| `-BattleSuppression` | Value | 0 | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionDecay` | Value | .25f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionPerShot` | Value | .12f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionPin` | Value | .5f | BattleSuppression.cpp:16 |  |
| `-BattleSuppressionRadius` | Value | 200.f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionSpread` | Value | 1.5f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionSupport` | Value | 1.f | BattleSuppression.cpp:17 |  |
| `-BattleSurpriseGroup` | Value | 0 | BattleFrontLine.cpp:513 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleSurpriseSquads` | Value | 2 | BattleFrontLine.cpp:534 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleSymmetricStart` | Value | -1 | BattleGroups.cpp:58 |  |
| `-BattleSymmetryAudit` | Param | off | BattleCover.cpp:77 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTankGunFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:468, BattleArtillery.cpp:48 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:12 |  |
| `-BattleTerrainLineBodies` | Value | 1 | BattleTactics.cpp:233 |  |
| `-BattleThinkCap` | Value | 0 | BattleLeaderRuntime.cpp:1382 |  |
| `-BattleTracerShot` | Param | off | BattleStanceShot.cpp:87 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:209 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:17, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:81, BattleMatch.cpp:27, BattleMatch.cpp:271, BattleMatch.cpp:290, BattleWideMapAudit.cpp:116 |  |
| `-BattleUnderFireCover` | Value | 0 | BattleLeaderRuntime.cpp:1573 |  |
| `-BattleUnits` | Value | 20（BattleBenchmark.cpp:36） / 1000（BattleSquadTrial.cpp:28） | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVATFarLight` | Value | 0 | BattleVATDisplay.cpp:37 |  |
| `-BattleVehicleHitCover` | Strifind/Value | 0 | BattleAreaSmoke.cpp:467, BattleArtillery.cpp:42 |  |
| `-BattleVehicleOrderHere` | Value | 0 | BattleArtillery.cpp:36 |  |
| `-BattleVerifyMixedSave` | Param | off | BattleSave.cpp:496, BattleSightExperiment.cpp:40 |  |
| `-BattleWaypointPass` | Value | 0 | BattleStableMovement.cpp:453 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponOrderGate` | Value | 0 | BattleWeaponOrders.cpp:10 |  |
| `-BattleWeaponPreview` | Value | 空文字列 | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value | 空文字列 | BattleState.cpp:44 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:221 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:235 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:75 |  |
| `-BattleWideScreens` | Value | 空文字列 | BattleAreaSmoke.cpp:1217, BattleContactTrial.cpp:97, BattleDeathShot.cpp:42, BattleExoShot.cpp:78, BattleMixedFastSmoke.cpp:34, BattleMixedSaveSmoke.cpp:305, BattleMovementTrial.cpp:108, BattleScopeShot.cpp:45, BattleSightVisual.cpp:30, BattleSquadFlankSmoke.cpp:165, BattleSquadRetreatSmoke.cpp:20, BattleStanceShot.cpp:96, BattleTeamSupportSmoke.cpp:122, BattleWideMapAudit.cpp:40, BattleWideMapAudit.cpp:159, BattleWideMapAudit.cpp:190, BattleWideMixedSmoke.cpp:39, BattleWideMixedSmoke.cpp:53 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:249 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:251 |  |
| `-BayFlank` | Value | 0 | BattleFlankBayTrial.cpp:17 |  |
| `-BayOutput` | Value | 空文字列 | BattleFlankBayTrial.cpp:62 |  |
| `-BayRendered` | Param | off | BattleFlankBayTrial.cpp:66 |  |
| `-BaySign` | Value | 1 | BattleFlankBayTrial.cpp:17 |  |
| `-CoverScreenshot` | Value | 空文字列 | BattleCoverActionSmoke.cpp:35, BattleCoverActionSmoke.cpp:249 |  |
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
| `-P3VATAudit` | Param | off | BattleLeaderRuntime.cpp:125, BattleVATDisplay.cpp:139 |  |
| `-P3View` | Value | "RTS" | BattlePhase3.cpp:64 |  |
| `-P3Warmup` | Value | 10 | BattlePhase3.cpp:65 |  |
| `-PktLag` | Value | 0 | BattleNetworkBenchmark.cpp:80 |  |
| `-PktLoss` | Value | 0 | BattleNetworkBenchmark.cpp:80 |  |
| `-ResX` | Value | 1920（BattleLeaderSmoke.cpp:104） / 1280（BattleWideMixedSmoke.cpp:167） | BattleLeaderSmoke.cpp:104, BattleWideMixedSmoke.cpp:167 |  |
| `-ResY` | Value | 1080（BattleLeaderSmoke.cpp:104） / 720（BattleWideMixedSmoke.cpp:167） | BattleLeaderSmoke.cpp:104, BattleWideMixedSmoke.cpp:167 |  |
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
