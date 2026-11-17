# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値はソースから推定したもの（`FParse::Value` に渡す変数の初期値、または構造体のメンバーの初期値）。空欄は推定できなかったもの。`UPROPERTY(Config)` の値は `Config/DefaultGame.ini` で上書きされることがある。大事なときは「場所」の行と ini を確かめる。`Param` は付けるだけでONになる引数。

計 198 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:9 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:943 |  |
| `-BattleArea` | Param | off | BattleLeaderRuntime.cpp:70 |  |
| `-BattleAreaACKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:92 |  |
| `-BattleAreaATKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:90 |  |
| `-BattleAreaAttackWeight` | Value | T->AreaAttackWeight | BattleTactics.cpp:70 |  |
| `-BattleAreaBaseFlank` | Value | 1.3f | BattleTactics.cpp:83 |  |
| `-BattleAreaBaseFlankSide` | Value | S.bFlankSameSide?1:0（bFlankSameSide=true） | BattleTactics.cpp:85 |  |
| `-BattleAreaBaseInfiltrate` | Value | 0 | BattleTactics.cpp:79 |  |
| `-BattleAreaBaseLowHP` | Value | 500.f | BattleTactics.cpp:75 |  |
| `-BattleAreaBaseReturnLiving` | Value | 5 | BattleTactics.cpp:86 |  |
| `-BattleAreaBaseRule` | Value | 0 | BattleTactics.cpp:72 |  |
| `-BattleAreaBaseShare` | Value | T->AreaBaseShare | BattleTactics.cpp:71 |  |
| `-BattleAreaBaseShareHigh` | Value | 0.3f | BattleTactics.cpp:74 |  |
| `-BattleAreaBaseShareLow` | Value | 0.f | BattleTactics.cpp:73 |  |
| `-BattleAreaFocus` | Value | 0.f | BattleTactics.cpp:81 |  |
| `-BattleAreaFrontBalance` | Value | S.bFrontBalance?1:0（bFrontBalance=false） | BattleTactics.cpp:76 |  |
| `-BattleAreaFrontGain` | Value | 1.f | BattleTactics.cpp:77 |  |
| `-BattleAreaHeavyShare` | Value | 0.f / -1.f（同名のメンバーが複数） | BattleTactics.cpp:88 |  |
| `-BattleAreaHire` | Value | T->bAreaHire?1:0 | BattleTactics.cpp:51 |  |
| `-BattleAreaHireInterval` | Value | T->AreaHireIntervalSeconds | BattleTactics.cpp:53 |  |
| `-BattleAreaHirePurpose` | Value | 0 | BattleTactics.cpp:55 |  |
| `-BattleAreaHirePurposeFront` | Value | 10 | BattleTactics.cpp:56 |  |
| `-BattleAreaHireReserve` | Value | T->AreaHireReserve | BattleTactics.cpp:53 |  |
| `-BattleAreaHireStop` | Value | T->AreaHireStopSeconds | BattleTactics.cpp:54 |  |
| `-BattleAreaIncome` | Value | T->bAreaIncome?1:0 | BattleTactics.cpp:112 |  |
| `-BattleAreaIncomeBase` | Value | FMath::Clamp(T->AreaBaseIncome | BattleTactics.cpp:115 |  |
| `-BattleAreaIncomePerPoint` | Value | FMath::Clamp(T->AreaPointIncome | BattleTactics.cpp:115 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:120 |  |
| `-BattleAreaIncomeUrbanBase` | Value | T->AreaBaseIncomeUrban | BattleTactics.cpp:118 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | T->AreaPointIncomeUrban | BattleTactics.cpp:118 |  |
| `-BattleAreaIntel` | Value | GetDefault<UBattleTuning>()->bAreaIntel?1:0 | BattleAreaIntel.cpp:14 |  |
| `-BattleAreaMortarKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:93 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:87 |  |
| `-BattleAreaScoutKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:89 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:37 |  |
| `-BattleAreaSPGKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:94 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:121 |  |
| `-BattleAreaStrategy` | Value | T->bAreaStrategy?1:0 | BattleTactics.cpp:69 |  |
| `-BattleAreaTankKeep` | Value | 0 / -1（同名のメンバーが複数） | BattleTactics.cpp:91 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:95 |  |
| `-BattleAreaWarPointSeconds` | other |  | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | other |  | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | other |  | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | other |  | BattleWarScore.cpp:12 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:52, BattleLeaderRuntime.cpp:66 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleConcealment` | other |  | BattleLeaderRuntime.cpp:376, BattleLeaderRuntime.cpp:761, BattleMapCommandlet.cpp:98 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:78 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:79 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:173 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:198 |  |
| `-BattleCoverApproach` | Value | 0 | BattleTactics.cpp:167 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:70 |  |
| `-BattleCoverFire` | Value | GetDefault<UBattleTuning>()->bAICoverFire?1:0 | BattleTactics.cpp:143 |  |
| `-BattleCoverPeekSight` | Value | GetDefault<UBattleTuning>()->bAICoverPeekSight?1:0 | BattleTactics.cpp:155 |  |
| `-BattleCoverValidation` | Value | GetDefault<UBattleTuning>()->bAICoverValidation?1:0 | BattleTactics.cpp:149 |  |
| `-BattleDelegationScreens` | Value |  | BattleDelegationSmoke.cpp:89 |  |
| `-BattleDiagnosticOmit` | Value |  | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:115, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:345 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:66 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value |  | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value |  | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 1 / 0 / 7（同名のメンバーが複数） | BattleSquadTrial.cpp:19 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:31 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:200 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:220 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:115, BattleSquadRetreat.cpp:18 |  |
| `-BattleHeavyCost` | other |  | BattleState.cpp:454 |  |
| `-BattleHeavyDamage` | other |  | BattleState.cpp:454 |  |
| `-BattleHeavyHealth` | other |  | BattleState.cpp:453 |  |
| `-BattleHeavyMembers` | other |  | BattleState.cpp:453 |  |
| `-BattleHeavySpeed` | other |  | BattleState.cpp:454 |  |
| `-BattleHireSightRefresh` | Value | 1 | BattleMixedRecruitment.cpp:109 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:15 |  |
| `-BattleLeaderHealth` | Value | GetDefault<UBattleTuning>()->LeaderHealthMultiplier | BattleTactics.cpp:22 |  |
| `-BattleLeaderNoControl` | Value | INDEX_NONE | BattleLeaderRuntime.cpp:132 |  |
| `-BattleLeaderQA` | Value |  | BattleLeaderRuntime.cpp:234 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:232 |  |
| `-BattleLeaderUnits` | Value | bMixedFastMatch?20:bWideField?500:40 | BattleLeaderRuntime.cpp:84 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:315 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:94 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value |  | BattleMatchSoak.cpp:56 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:18 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:14 |  |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:69 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:202 |  |
| `-BattleMixedSaveSmoke` | Param | off | BattleLeaderRuntime.cpp:204, BattleMixedBodies.cpp:114 |  |
| `-BattleMoveCourse` | Value |  | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:81 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:80 |  |
| `-BattleMovingCapture` | Value |  | BattleSquadTrial.cpp:18 |  |
| `-BattleNetBenchmark` | Param | off | BattleNetwork.cpp:33, BattleNetworkBenchmark.cpp:28, BattleNetworkPilot.cpp:136 |  |
| `-BattleNetBenchmarkPilot` | Param | off | BattleNetworkBenchmark.cpp:30 |  |
| `-BattleNetHoldProbe` | Param | off | BattleNetworkProbe.cpp:9 |  |
| `-BattleNetLeaveAfter` | Value | 0 | BattleNetworkProbe.cpp:10 |  |
| `-BattleNetPilotProbe` | Param | off | BattleNetworkPilot.cpp:136, BattleNetworkPilotProbe.cpp:7 |  |
| `-BattleNetProbe` | Param | off | BattleNetworkProbe.cpp:21 |  |
| `-BattleNetReport` | Value |  | BattleNetworkBenchmark.cpp:112 |  |
| `-BattleNetScreenshot` | Value |  | BattleNetworkBenchmark.cpp:115 |  |
| `-BattleNetSeconds` | Value | 20 | BattleNetworkBenchmark.cpp:73 |  |
| `-BattleNetUnits` | Value | 20 | BattleNetwork.cpp:27, BattleNetworkBenchmark.cpp:83, BattleNetworkPilotProbe.cpp:22 ほか1 |  |
| `-BattleNoAnimationLOD` | Param | off | BattleBenchmark.cpp:157, BattleBenchmark.cpp:158, BattleNetworkBenchmark.cpp:78 ほか1 |  |
| `-BattleNoStagger` | Param | off | BattleBenchmark.cpp:54 |  |
| `-BattleOpponentPreset` | other |  | BattleBenchmark.cpp:48 |  |
| `-BattlePerceptionGrid` | Value | GetDefault<UBattleTuning>()->bPerceptionGrid?1:0 | BattleLeaderRuntime.cpp:99 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value |  | BattlePhase3.cpp:45, BattleUnit.cpp:32, BattleVATDisplay.cpp:294 ほか1 |  |
| `-BattlePreset` | other |  | BattleBenchmark.cpp:48 |  |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:75 |  |
| `-BattleRangeEngage` | other |  | BattleRanges.cpp:20 |  |
| `-BattleRangeEngageSupport` | other |  | BattleRanges.cpp:20 |  |
| `-BattleRangeProfile` | other |  | BattleRanges.cpp:14 |  |
| `-BattleRangeReach` | other |  | BattleRanges.cpp:21 |  |
| `-BattleRangeReachSupport` | other |  | BattleRanges.cpp:21 |  |
| `-BattleRangeSight` | other |  | BattleRanges.cpp:18 |  |
| `-BattleReacquireGrace` | Value | GetDefault<UBattleTuning>()->bReacquireGrace?1:0 | BattleTactics.cpp:136 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:88 |  |
| `-BattleReduceHiddenBones` | Value | GetDefault<UBattleTuning>()->bReduceHiddenBones?1:0 | BattleLeaderRuntime.cpp:92 |  |
| `-BattleReplayArm` | Value |  | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value |  | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value |  | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value |  | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:161 |  |
| `-BattleReuseSightContext` | Value | GetDefault<UBattleTuning>()->bReuseSightContext?1:0 | BattleLeaderRuntime.cpp:96 |  |
| `-BattleScaleScreenshot` | Value |  | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0 / 20 | BattleController.cpp:51, BattleGameMode.cpp:79 |  |
| `-BattleScoutRetreat` | Value | GetDefault<UBattleTuning>()->bScoutRetreat?1:0 | BattleScoutReport.cpp:20 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSenseMode` | Value |  | BattleBenchmark.cpp:29 |  |
| `-BattleSightExperiment` | Value |  | BattleLeaderRuntime.cpp:72 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:326 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:289, BattlePerception.cpp:154 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:153 |  |
| `-BattleSightRange` | Value |  | BattleState.cpp:17 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:107 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:107 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:104 |  |
| `-BattleSightTeamMask` | Value | 0 | BattleLeaderRuntime.cpp:331 |  |
| `-BattleSightTraces` | other |  | BattlePerception.cpp:267 |  |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:214 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:210 |  |
| `-BattleSmokeSuite` | Value |  | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209 ほか1 |  |
| `-BattleSmokeTest` | Param | off | BattleGameMode.cpp:142 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value/other |  | BattleGameMode.cpp:64, BattleLeaderRuntime.cpp:67, BattleUnit.cpp:95 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:224 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:77 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:32 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:17 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:208 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:126 |  |
| `-BattleSTScreens` | Value |  | BattleSquadTacticsSmoke.cpp:127 |  |
| `-BattleSupplyAwayRatio` | other |  | BattleTactics.cpp:35 |  |
| `-BattleSupplyHeal` | other |  | BattleTactics.cpp:35 |  |
| `-BattleSupplyResumeHealth` | other |  | BattleTactics.cpp:36 |  |
| `-BattleSupplyResumeLiving` | other |  | BattleTactics.cpp:34 |  |
| `-BattleSupplyReturnLiving` | other |  | BattleTactics.cpp:34 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:222 |  |
| `-BattleSuppression` | other |  | BattleSuppression.cpp:10 |  |
| `-BattleSuppressionDecay` | other |  | BattleSuppression.cpp:12 |  |
| `-BattleSuppressionPerShot` | other |  | BattleSuppression.cpp:11 |  |
| `-BattleSuppressionPin` | other |  | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionRadius` | other |  | BattleSuppression.cpp:11 |  |
| `-BattleSuppressionSpread` | other |  | BattleSuppression.cpp:12 |  |
| `-BattleSuppressionSupport` | other |  | BattleSuppression.cpp:14 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:12 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:188 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:15, BattleLeaderRuntime.cpp:74, BattleMatch.cpp:262 ほか2 |  |
| `-BattleUnits` | Value | 1000 / 20 | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVerifyMixedSave` | Param | off | BattleSave.cpp:228, BattleSightExperiment.cpp:40 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponPreview` | Value |  | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value |  | BattleState.cpp:42 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:206 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:212 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:68 |  |
| `-BattleWideScreens` | Value |  | BattleAreaSmoke.cpp:618, BattleContactTrial.cpp:97, BattleMixedFastSmoke.cpp:34 ほか12 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:226 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:228 |  |
