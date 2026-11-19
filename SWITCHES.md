# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値はソースから推定したもの（`FParse::Value` に渡す変数の初期値、または構造体のメンバーの初期値）。空欄は推定できなかったもの。`UPROPERTY(Config)` の値は `Config/DefaultGame.ini` で上書きされることがある。大事なときは「場所」の行と ini を確かめる。`Param` は付けるだけでONになる引数。

計 237 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:10 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:1081 |  |
| `-BattleArea` | Param/other | off | BattleLeaderRuntime.cpp:73, BattleState.cpp:519 |  |
| `-BattleAreaACKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:96 |  |
| `-BattleAreaATKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:94 |  |
| `-BattleAreaAttackWeight` | Value | T->AreaAttackWeight | BattleTactics.cpp:74 |  |
| `-BattleAreaBaseFlank` | Value | 1.3f | BattleTactics.cpp:87 |  |
| `-BattleAreaBaseFlankSide` | Value | S.bFlankSameSide?1:0（bFlankSameSide=true） | BattleTactics.cpp:89 |  |
| `-BattleAreaBaseInfiltrate` | Value | 0 | BattleTactics.cpp:83 |  |
| `-BattleAreaBaseLowHP` | Value | 500.f | BattleTactics.cpp:79 |  |
| `-BattleAreaBaseReturnLiving` | Value | 5 | BattleTactics.cpp:90 |  |
| `-BattleAreaBaseRule` | Value | 0 | BattleTactics.cpp:76 |  |
| `-BattleAreaBaseShare` | Value | T->AreaBaseShare | BattleTactics.cpp:75 |  |
| `-BattleAreaBaseShareHigh` | Value | 0.3f | BattleTactics.cpp:78 |  |
| `-BattleAreaBaseShareLow` | Value | 0.f | BattleTactics.cpp:77 |  |
| `-BattleAreaComposition` | Value | GetDefault<UBattleTuning>()->bAreaComposition?1:0 | BattleArmourAI.cpp:38 |  |
| `-BattleAreaDepot` | Value | 0 | BattleAreaDepot.cpp:22 |  |
| `-BattleAreaDepotRaid` | Value | DepotRaid | BattleAreaDepot.cpp:23 |  |
| `-BattleAreaExoKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:100 |  |
| `-BattleAreaFocus` | Value | 0.f | BattleTactics.cpp:85 |  |
| `-BattleAreaFrontBalance` | Value | S.bFrontBalance?1:0（bFrontBalance=false） | BattleTactics.cpp:80 |  |
| `-BattleAreaFrontGain` | Value | 1.f | BattleTactics.cpp:81 |  |
| `-BattleAreaGroups` | Value | GetDefault<UBattleTuning>()->bAreaGroups?1:0 | BattleGroups.cpp:68 |  |
| `-BattleAreaHeavyShare` | Value | 0.f / -1.f（同名のメンバーが複数） | BattleTactics.cpp:92 |  |
| `-BattleAreaHire` | Value | T->bAreaHire?1:0 | BattleTactics.cpp:55 |  |
| `-BattleAreaHireInterval` | Value | T->AreaHireIntervalSeconds | BattleTactics.cpp:57 |  |
| `-BattleAreaHirePurpose` | Value | 0 | BattleTactics.cpp:59 |  |
| `-BattleAreaHirePurposeFront` | Value | 10 | BattleTactics.cpp:60 |  |
| `-BattleAreaHireReserve` | Value | T->AreaHireReserve | BattleTactics.cpp:57 |  |
| `-BattleAreaHireStop` | Value | T->AreaHireStopSeconds | BattleTactics.cpp:58 |  |
| `-BattleAreaIncome` | Value | T->bAreaIncome?1:0 | BattleTactics.cpp:118 |  |
| `-BattleAreaIncomeBase` | Value | FMath::Clamp(T->AreaBaseIncome | BattleTactics.cpp:121 |  |
| `-BattleAreaIncomePerPoint` | Value | FMath::Clamp(T->AreaPointIncome | BattleTactics.cpp:121 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:126 |  |
| `-BattleAreaIncomeUrbanBase` | Value | T->AreaBaseIncomeUrban | BattleTactics.cpp:124 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | T->AreaPointIncomeUrban | BattleTactics.cpp:124 |  |
| `-BattleAreaIntel` | Value | GetDefault<UBattleTuning>()->bAreaIntel?1:0 | BattleAreaIntel.cpp:14 |  |
| `-BattleAreaMortarKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:97 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:91 |  |
| `-BattleAreaPointSupply` | other |  | BattleTactics.cpp:39 |  |
| `-BattleAreaScoutKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:93 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:42 |  |
| `-BattleAreaSPGKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:98 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:127 |  |
| `-BattleAreaStrategy` | Value | T->bAreaStrategy?1:0 | BattleTactics.cpp:73 |  |
| `-BattleAreaTankKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:95 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:101 |  |
| `-BattleAreaWarPointSeconds` | other |  | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | other |  | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | other |  | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | other |  | BattleWarScore.cpp:12 |  |
| `-BattleArmourAI` | Value | GetDefault<UBattleTuning>()->bArmourAI?1:0 | BattleArmourAI.cpp:31 |  |
| `-BattleArmourAITeam` | Value | -1 | BattleArmourAI.cpp:32 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:52, BattleLeaderRuntime.cpp:69 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleConcealment` | other |  | BattleLeaderRuntime.cpp:424, BattleLeaderRuntime.cpp:967, BattleMapCommandlet.cpp:106 ほか1 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:81 |  |
| `-BattleContactCover` | Value | 0 | BattleTactics.cpp:185 |  |
| `-BattleContactCoverTeam` | Value | -1 | BattleTactics.cpp:179 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:82 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:191 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:209 |  |
| `-BattleCoverApproach` | Value | -1 | BattleTactics.cpp:173 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:94 |  |
| `-BattleCoverFire` | Value | GetDefault<UBattleTuning>()->bAICoverFire?1:0 | BattleTactics.cpp:149 |  |
| `-BattleCoverPeekSight` | Value | GetDefault<UBattleTuning>()->bAICoverPeekSight?1:0 | BattleTactics.cpp:161 |  |
| `-BattleCoverValidation` | Value | GetDefault<UBattleTuning>()->bAICoverValidation?1:0 | BattleTactics.cpp:155 |  |
| `-BattleDefendEngage` | Value | GetDefault<UBattleTuning>()->bDefendEngage?1:0 | BattleGroups.cpp:46 |  |
| `-BattleDelegationScreens` | Value |  | BattleDelegationSmoke.cpp:90 |  |
| `-BattleDiagnosticOmit` | Value |  | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:118, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:388 |  |
| `-BattleExoPlaceholder` | Param | off | BattleExo.cpp:45 |  |
| `-BattleExoShot` | Param | off | BattleLeaderRuntime.cpp:219 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:69 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value |  | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value |  | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 1 / 0 / 7（同名のメンバーが複数） | BattleSquadTrial.cpp:19 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:35 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:211 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:237 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:128, BattleSquadRetreat.cpp:18 |  |
| `-BattleFollowerAnimBudget` | Value | 0 | BattleVATDisplay.cpp:48 |  |
| `-BattleFollowerAnimSkip` | Value | 0 | BattleVATDisplay.cpp:53 |  |
| `-BattleFollowerNoOverlap` | Value | 0 | BattleLeaderRuntime.cpp:707 |  |
| `-BattleFollowSightParallel` | Value | 0 | BattleLeaderRuntime.cpp:700 |  |
| `-BattleFollowSightTraces` | other |  | BattleLeaderRuntime.cpp:734 |  |
| `-BattleHeavyCost` | other |  | BattleState.cpp:507 |  |
| `-BattleHeavyDamage` | other |  | BattleState.cpp:507 |  |
| `-BattleHeavyHealth` | other |  | BattleState.cpp:506 |  |
| `-BattleHeavyMembers` | other |  | BattleState.cpp:506 |  |
| `-BattleHeavySpeed` | other |  | BattleState.cpp:507 |  |
| `-BattleHeightTest` | Param | off | BattleTerrainTest.cpp:27 |  |
| `-BattleHireSightRefresh` | Value | 0 | BattleMixedRecruitment.cpp:111 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:16 |  |
| `-BattleLeaderHealth` | Value | GetDefault<UBattleTuning>()->LeaderHealthMultiplier | BattleTactics.cpp:23 |  |
| `-BattleLeaderNoControl` | Value | INDEX_NONE | BattleLeaderRuntime.cpp:135 |  |
| `-BattleLeaderQA` | Value |  | BattleLeaderRuntime.cpp:251 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:249 |  |
| `-BattleLeaderUnits` | Value | bMixedFastMatch?20:bWideField?500:40 | BattleLeaderRuntime.cpp:87 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:445 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:98 |  |
| `-BattleMapStats` | Value |  | BattleLeaderRuntime.cpp:227, BattleMapStats.cpp:40 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value |  | BattleMatchSoak.cpp:56 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:18 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:14 |  |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:72 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:213 |  |
| `-BattleMixedSaveSmoke` | Param | off | BattleLeaderRuntime.cpp:215, BattleMixedBodies.cpp:115 |  |
| `-BattleMoveCourse` | Value |  | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:84 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:83 |  |
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
| `-BattleOpponentPreset` | other |  | BattleBenchmark.cpp:48 |  |
| `-BattlePerceptionGrid` | Value | GetDefault<UBattleTuning>()->bPerceptionGrid?1:0 | BattleLeaderRuntime.cpp:102 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value |  | BattlePhase3.cpp:45, BattleUnit.cpp:36, BattleVATDisplay.cpp:382 ほか1 |  |
| `-BattlePlayerRouteTest` | Param | off | BattlePlayerRoutes.cpp:127 |  |
| `-BattlePreset` | other |  | BattleBenchmark.cpp:48 |  |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:78 |  |
| `-BattleRangeEngage` | other |  | BattleRanges.cpp:20 |  |
| `-BattleRangeEngageSupport` | other |  | BattleRanges.cpp:20 |  |
| `-BattleRangeProfile` | other |  | BattleRanges.cpp:14 |  |
| `-BattleRangeReach` | other |  | BattleRanges.cpp:21 |  |
| `-BattleRangeReachSupport` | other |  | BattleRanges.cpp:21 |  |
| `-BattleRangeSight` | other |  | BattleRanges.cpp:18 |  |
| `-BattleReacquireGrace` | Value | GetDefault<UBattleTuning>()->bReacquireGrace?1:0 | BattleTactics.cpp:142 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:91 |  |
| `-BattleReduceHiddenBones` | Value | GetDefault<UBattleTuning>()->bReduceHiddenBones?1:0 | BattleLeaderRuntime.cpp:95 |  |
| `-BattleReplayArm` | Value |  | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value |  | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value |  | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value |  | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:167 |  |
| `-BattleReuseSightContext` | Value | GetDefault<UBattleTuning>()->bReuseSightContext?1:0 | BattleLeaderRuntime.cpp:99 |  |
| `-BattleRifleMembers` | other |  | BattleState.cpp:519 |  |
| `-BattleRoutes` | Value | -1 | BattleRoutes.cpp:17 |  |
| `-BattleScaleScreenshot` | Value |  | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0 / 20 | BattleController.cpp:51, BattleGameMode.cpp:79 |  |
| `-BattleScopeShot` | Param | off | BattleLeaderRuntime.cpp:223 |  |
| `-BattleScoutRetreat` | Value | GetDefault<UBattleTuning>()->bScoutRetreat?1:0 | BattleScoutReport.cpp:20 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSectorAI` | Value | 0 | BattleMatch.cpp:445, BattleMatch.cpp:456 |  |
| `-BattleSectorAudit` | Param | off | BattleSectorAudit.cpp:28 |  |
| `-BattleSectorContact` | Value | 0 | BattleLeaderRuntime.cpp:1184 |  |
| `-BattleSectorFlank` | Value | 0 | BattleSectorRoute.cpp:34 |  |
| `-BattleSectorFront` | Value | 0 | BattleSectorRoute.cpp:40 |  |
| `-BattleSectorRoute` | Value | 0 | BattleSectorRoute.cpp:15 |  |
| `-BattleSectorSmoke` | Param | off | BattleLeaderRuntime.cpp:207 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSenseMode` | Value |  | BattleBenchmark.cpp:29 |  |
| `-BattleSightExperiment` | Value |  | BattleLeaderRuntime.cpp:75 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:374 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:337, BattlePerception.cpp:154 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:153 |  |
| `-BattleSightRange` | Value |  | BattleState.cpp:17 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:110 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:110 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:107 |  |
| `-BattleSightTeamMask` | Value | 0 | BattleLeaderRuntime.cpp:379 |  |
| `-BattleSightTraces` | other |  | BattlePerception.cpp:267 |  |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:231 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:225 |  |
| `-BattleSmokeSuite` | Value |  | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209 ほか1 |  |
| `-BattleSmokeTest` | Param | off | BattleGameMode.cpp:142 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value/other |  | BattleGameMode.cpp:64, BattleLeaderRuntime.cpp:70, BattleUnit.cpp:99 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:241 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:80 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:32 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:17 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:221 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:126 |  |
| `-BattleSTScreens` | Value |  | BattleSquadTacticsSmoke.cpp:127 |  |
| `-BattleSupplyAwayRatio` | other |  | BattleTactics.cpp:36 |  |
| `-BattleSupplyHeal` | other |  | BattleTactics.cpp:36 |  |
| `-BattleSupplyResumeHealth` | other |  | BattleTactics.cpp:37 |  |
| `-BattleSupplyResumeLiving` | other |  | BattleTactics.cpp:35 |  |
| `-BattleSupplyReturnLiving` | other |  | BattleTactics.cpp:35 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:239 |  |
| `-BattleSuppression` | other |  | BattleSuppression.cpp:10 |  |
| `-BattleSuppressionDecay` | other |  | BattleSuppression.cpp:12 |  |
| `-BattleSuppressionPerShot` | other |  | BattleSuppression.cpp:11 |  |
| `-BattleSuppressionPin` | other |  | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionRadius` | other |  | BattleSuppression.cpp:11 |  |
| `-BattleSuppressionSpread` | other |  | BattleSuppression.cpp:12 |  |
| `-BattleSuppressionSupport` | other |  | BattleSuppression.cpp:14 |  |
| `-BattleSymmetricStart` | Value | GetDefault<UBattleTuning>()->bSymmetricStart?1:0 | BattleGroups.cpp:53 |  |
| `-BattleSymmetryAudit` | Param | off | BattleCover.cpp:56 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:12 |  |
| `-BattleTerrain` | other |  | BattleTerrain.h:18 |  |
| `-BattleTerrainLineBodies` | Value | 1 | BattleTactics.cpp:230 |  |
| `-BattleThinkCap` | Value | 0 | BattleLeaderRuntime.cpp:1121 |  |
| `-BattleTracerShot` | Param | off | BattleStanceShot.cpp:87 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:206 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:15, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:77 ほか4 |  |
| `-BattleUnits` | Value | 1000 / 20 | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVATFarLight` | Value | 0 | BattleVATDisplay.cpp:37 |  |
| `-BattleVerifyMixedSave` | Param | off | BattleSave.cpp:230, BattleSightExperiment.cpp:40 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponPreview` | Value |  | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value |  | BattleState.cpp:42 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:217 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:229 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:71 |  |
| `-BattleWideScreens` | Value |  | BattleAreaSmoke.cpp:794, BattleContactTrial.cpp:97, BattleExoShot.cpp:68 ほか14 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:243 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:245 |  |
