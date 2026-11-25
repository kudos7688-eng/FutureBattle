# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値は C++ と DefaultGame.ini の静的読取りによる。解決できない値は「未解決」と記す。
UBattleTuning の Config 値は DefaultGame.ini を優先する。既定は読取り先の初期値で、マップ・陣営・実行時の変更や後処理まで保証しない。Param / Strifind の off は引数なしを示す。読取り箇所で値が異なる場合は場所を併記する。

計 495 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleACShield` | Value | 0 | BattleATHunt.cpp:47 |  |
| `-BattleACStandOff` | Value | 0 | BattleATHunt.cpp:35 |  |
| `-BattleACTankStandOff` | Value | 0 | BattleATHunt.cpp:41 |  |
| `-BattleAIBlindFire` | Value | 0 | BattleBlindFire.cpp:27 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:10 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:1638 |  |
| `-BattleAllyPass` | Value | 0 | BattleStableMovement.cpp:51 |  |
| `-BattleArea` | Param | off | BattleLeaderRuntime.cpp:82, BattleState.cpp:592 |  |
| `-BattleAreaACKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:113 |  |
| `-BattleAreaAllTypes` | Value | 0 | BattleArmourAI.cpp:80 |  |
| `-BattleAreaATKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:111 |  |
| `-BattleAreaAttackWeight` | Value | 2.0 | BattleTactics.cpp:91 |  |
| `-BattleAreaBaseFlank` | Value | 1.3f | BattleTactics.cpp:104 |  |
| `-BattleAreaBaseFlankSide` | Value | true | BattleTactics.cpp:106 |  |
| `-BattleAreaBaseInfiltrate` | Value | 0 | BattleTactics.cpp:100 |  |
| `-BattleAreaBaseLowHP` | Value | 500.f | BattleTactics.cpp:96 |  |
| `-BattleAreaBaseReturnLiving` | Value | 5 | BattleTactics.cpp:107 |  |
| `-BattleAreaBaseRule` | Value | 0 | BattleTactics.cpp:93 |  |
| `-BattleAreaBaseShare` | Value | 0.2 | BattleTactics.cpp:92 |  |
| `-BattleAreaBaseShareHigh` | Value | 0.3f | BattleTactics.cpp:95 |  |
| `-BattleAreaBaseShareLow` | Value | 0.f | BattleTactics.cpp:94 |  |
| `-BattleAreaComposition` | Value | -1 | BattleArmourAI.cpp:71 |  |
| `-BattleAreaCompositionPlanner` | Value | 0 | BattleComposition.cpp:12 |  |
| `-BattleAreaCompositionPlannerTeams` | Value | 0 | BattleComposition.cpp:13 |  |
| `-BattleAreaDepot` | Value | 0 | BattleAreaDepot.cpp:22 |  |
| `-BattleAreaDepotRaid` | Value | 未解決 | BattleAreaDepot.cpp:23 |  |
| `-BattleAreaEliteKeep` | Value | 2 | BattleArms.cpp:44 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleAreaEnsureTypes` | Value | 0 | BattleArmourAI.cpp:88 |  |
| `-BattleAreaExoEarly` | Value | 0 | BattleArea.cpp:424 |  |
| `-BattleAreaExoKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:117 |  |
| `-BattleAreaFocus` | Value | 1.0 | BattleTactics.cpp:102 |  |
| `-BattleAreaFrontBalance` | Value | True | BattleTactics.cpp:97 |  |
| `-BattleAreaFrontGain` | Value | 1.0 | BattleTactics.cpp:98 |  |
| `-BattleAreaGroups` | Value | -1 | BattleGroups.cpp:83 |  |
| `-BattleAreaHeavyShare` | Value | -1.f | BattleTactics.cpp:109 |  |
| `-BattleAreaHire` | Value | True | BattleTactics.cpp:71 |  |
| `-BattleAreaHireInterval` | Value | 0.f | BattleTactics.cpp:73 |  |
| `-BattleAreaHirePurpose` | Value | 0 | BattleTactics.cpp:76 |  |
| `-BattleAreaHirePurposeFront` | Value | 10 | BattleTactics.cpp:77 |  |
| `-BattleAreaHireReserve` | Value | 0 | BattleTactics.cpp:73 |  |
| `-BattleAreaHireStop` | Value | 0.f | BattleTactics.cpp:74 |  |
| `-BattleAreaIncome` | Value | True | BattleTactics.cpp:137 |  |
| `-BattleAreaIncomeBase` | Value | 未解決 | BattleTactics.cpp:140 |  |
| `-BattleAreaIncomePerPoint` | Value | 未解決 | BattleTactics.cpp:140 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:145 |  |
| `-BattleAreaIncomeUrbanBase` | Value | -1 | BattleTactics.cpp:143 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | -1 | BattleTactics.cpp:143 |  |
| `-BattleAreaIntel` | Value | false | BattleAreaIntel.cpp:15 |  |
| `-BattleAreaMaxSquads` | Value | 30 | BattleTactics.cpp:75 |  |
| `-BattleAreaMortarKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:114 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:108 |  |
| `-BattleAreaPointSupply` | Value | True | BattleTactics.cpp:55 |  |
| `-BattleAreaScoutKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:110 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:48, BattleMixedBodies.cpp:115 |  |
| `-BattleAreaSniperKeep` | Value | 2 | BattleArms.cpp:47 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleAreaSPGCost` | Value | 35 | BattleArtillery.cpp:243 |  |
| `-BattleAreaSPGKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:115 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:146 |  |
| `-BattleAreaStrategy` | Value | True | BattleTactics.cpp:90 |  |
| `-BattleAreaSupplyEarly` | Value | 0 | BattleArea.cpp:428 |  |
| `-BattleAreaSupplyKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:119 |  |
| `-BattleAreaTankKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:112 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:120 |  |
| `-BattleAreaWarPointSeconds` | Value | 5.f | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | Value | 1 | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | Value | 0 | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | Value | 0 | BattleWarScore.cpp:12 |  |
| `-BattleAreaWithout` | Value | -1 | BattleArmourAI.cpp:94 |  |
| `-BattleArmourAI` | Value | true | BattleArmourAI.cpp:62 |  |
| `-BattleArmourAITeam` | Value | -1 | BattleArmourAI.cpp:63 |  |
| `-BattleArmourSectorGate` | Value | 0 | BattleWeaponOrders.cpp:16 |  |
| `-BattleArmourWait` | Value | 0 | BattleArmourAI.cpp:298, BattlePerception.cpp:156 |  |
| `-BattleArmsAssault` | Value | 1 | BattleArms.cpp:50 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsCharge` | Value | 1 | BattleArms.cpp:51 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsChargeFallen` | Value | 1 | BattleArms.cpp:58 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsChargeSlide` | Value | 1 | BattleArms.cpp:59 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsCoverFire` | Value | 1 | BattleArms.cpp:55 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsRushWatch` | Value | 1 | BattleArms.cpp:54 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsSectorTask` | Value | 1 | BattleArms.cpp:52 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsSlide` | Value | 1 | BattleArms.cpp:57 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArmsSmoke` | Param | off | BattleArms.cpp:56 |  |
| `-BattleArmsTeam` | Value | -1 | BattleArms.cpp:60 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleArrivalReady` | Value | 0 | BattleStance.cpp:72 |  |
| `-BattleArrivalThreat` | Value | -1 | BattleLeaderRuntime.cpp:1933 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleArrivalWalk` | Value | 1 | BattleLeaderRuntime.cpp:1718 |  |
| `-BattleArtilleryClusters` | Value | 0 | BattleArtillery.cpp:291 |  |
| `-BattleArtilleryFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:532, BattleArtillery.cpp:57 |  |
| `-BattleArtilleryHomeGap` | Value | 0.f | BattleArtillery.cpp:105 |  |
| `-BattleArtilleryObserved` | Value | 0 | BattleArtillery.cpp:33 |  |
| `-BattleArtilleryPostRisk` | Value | 0.f | BattleArtillery.cpp:175 |  |
| `-BattleArtilleryPosts` | Strifind/Value | 0 | BattleAreaSmoke.cpp:531, BattleArtillery.cpp:212 |  |
| `-BattleArtilleryRaidChase` | Value | 0 | BattleArtillery.cpp:218 |  |
| `-BattleArtilleryRaidGroup` | Value | 0 | BattleArtillery.cpp:230 |  |
| `-BattleArtilleryRaidScreen` | Value | 2 | BattleArtillery.cpp:236 |  |
| `-BattleArtilleryRaidSneak` | Value | 0.f | BattleArtillery.cpp:224 |  |
| `-BattleArtilleryStandoff` | Value | .6f | BattleArtillery.cpp:259 |  |
| `-BattleArtilleryVehicleSplash` | Value | 0.f | BattleArtillery.cpp:90 |  |
| `-BattleATFire` | Value | 0 | BattleATHunt.cpp:29 |  |
| `-BattleATHunt` | Value | 0 | BattleATHunt.cpp:23 |  |
| `-BattleATRange` | Value | 0 | BattleATHunt.cpp:16 |  |
| `-BattleAttackFocus` | Value | 1 | BattleLeaderRuntime.cpp:1879 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:104, BattleLeaderRuntime.cpp:78 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleBlastOcclusion` | Value | 0 | BattleArtillery.cpp:285 |  |
| `-BattleBlindFire` | Value | 1 | BattleBlindFire.cpp:26 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireClipSide` | Value | "AS_CoverLo_AimR" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireClipTop` | Value | "AS_CoverLo_AimU_down" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireExpose` | Value | 1 | BattleBlindFire.cpp:77 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireSpread` | Value | 4.f | BattleBlindFire.cpp:30 |  |
| `-BattleBodyAnim` | Value | 1 | BattleBodyAnim.cpp:59 |  |
| `-BattleBounding` | Value | 0 | BattleLeaderRuntime.cpp:1733 |  |
| `-BattleBuyPanelShot` | Value | 空文字列 | BattleBuyPanel.cpp:244 |  |
| `-BattleCarbineReserve` | Value | -1 | BattleTactics.cpp:39 |  |
| `-BattleCarbineShotRounds` | Value | 1 | BattleTactics.cpp:29 |  |
| `-BattleCloseIn` | Value | 0 | BattleLeaderRuntime.cpp:1847 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:90 |  |
| `-BattleContactCover` | Value | 0 | BattleTactics.cpp:204 |  |
| `-BattleContactCoverTeam` | Value | -1 | BattleTactics.cpp:198 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:91 |  |
| `-BattleCorpseLifetime` | Value | 20.f | BattleCorpses.cpp:35 |  |
| `-BattleCorpseSteps` | Value | 63 | BattleCorpses.cpp:29 |  |
| `-BattleCounterBattery` | Value | 0 | BattleArtillery.cpp:66 |  |
| `-BattleCounterBatteryRest` | Value | 60.f | BattleArtillery.cpp:82 |  |
| `-BattleCounterBatteryShots` | Value | 0 | BattleArtillery.cpp:77 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:217 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:224 |  |
| `-BattleCoverApproach` | Value | -1 | BattleTactics.cpp:192 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:161 |  |
| `-BattleCoverDense` | Value | 0 | BattleLeaderRuntime.cpp:1918 |  |
| `-BattleCoverFire` | Value | True | BattleTactics.cpp:168 |  |
| `-BattleCoverFirePoint` | Value | 0 | BattleTactics.cpp:211 | trial, default 0: over low cover or a 100..120 cm step past an edge |
| `-BattleCoverHuddle` | Value | -1 | BattleStableMovement.cpp:266 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleCoverIndoor` | Value | 0 | BattleLeaderRuntime.cpp:1923 |  |
| `-BattleCoverOutward` | Value | 0 | BattleLeaderRuntime.cpp:1928 |  |
| `-BattleCoverPeekSight` | Value | True | BattleTactics.cpp:180 |  |
| `-BattleCoverPriority` | Value | 1 | BattleLeaderRuntime.cpp:1832 |  |
| `-BattleCoverStrict` | Value | 0 | BattleLeaderRuntime.cpp:1938 |  |
| `-BattleCoverValidation` | Value | True | BattleTactics.cpp:174 |  |
| `-BattleCutOffRoute` | Value | 0 | BattleStableMovement.cpp:407 |  |
| `-BattleDangerRoute` | Value | 0 | BattleSectorRoute.cpp:28 |  |
| `-BattleDeathShot` | Param | off | BattleLeaderRuntime.cpp:236 |  |
| `-BattleDefendEngage` | Value | false | BattleGroups.cpp:50 |  |
| `-BattleDelegationScreens` | Value | 空文字列 | BattleDelegationSmoke.cpp:90 |  |
| `-BattleDepotHealth` | Value | 0.f | BattleArtillery.cpp:165 |  |
| `-BattleDepotRebuild` | Value | 120.f | BattleArtillery.cpp:170 |  |
| `-BattleDiagnosticOmit` | Value | 空文字列 | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:127, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleElite` | Value | 1 | BattleArms.cpp:43 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleEliteCost` | Value | 26 | BattleArms.cpp:45 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:652 |  |
| `-BattleExoArmor` | Value | -1.f | BattleState.cpp:602 | ExoTrialValue → BattleState.cpp:600（ヘルパー経由） |
| `-BattleExoBody` | Value | 3 | BattleExo.cpp:123 |  |
| `-BattleExoDamage` | Value | -1.f | BattleState.cpp:603 | ExoTrialValue → BattleState.cpp:600（ヘルパー経由） |
| `-BattleExoHealth` | Value | -1.f | BattleState.cpp:618 | ExoTrialValue → BattleState.cpp:600（ヘルパー経由） |
| `-BattleExoPlaceholder` | Param | off | BattleExo.cpp:145 |  |
| `-BattleExoShield` | Value | -1.f | BattleState.cpp:604 | ExoTrialValue → BattleState.cpp:600（ヘルパー経由） |
| `-BattleExoShieldDelay` | Value | -1.f | BattleSuppression.cpp:90 |  |
| `-BattleExoShieldRegen` | Value | -1.f | BattleState.cpp:606 | ExoTrialValue → BattleState.cpp:600（ヘルパー経由） |
| `-BattleExoShieldSuppression` | Value | 0 | BattleSuppression.cpp:83 |  |
| `-BattleExoShot` | Param | off | BattleLeaderRuntime.cpp:234 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:78 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value | 空文字列 | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value | 空文字列 | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 7 | BattleSquadTrial.cpp:19 |  |
| `-BattleFieldSupplyLog` | Param | off | BattleSquadRetreat.cpp:177 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:41 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:226 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:254 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:128, BattleSquadRetreat.cpp:18 |  |
| `-BattleFocusTarget` | Value | 0 | BattleLeaderRuntime.cpp:1884 |  |
| `-BattleFollowerAnimBudget` | Value | 0 | BattleVATDisplay.cpp:39 |  |
| `-BattleFollowerAnimSkip` | Value | 0 | BattleVATDisplay.cpp:44 |  |
| `-BattleFollowerNoOverlap` | Value | 0 | BattleLeaderRuntime.cpp:877 |  |
| `-BattleFrontArmy` | Value | 1 | BattleFrontLine.cpp:33 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontAuto` | Value | 1 | BattleFrontLine.cpp:38 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontGap` | Value | 1 | BattleFrontLine.cpp:48 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontHold` | Value | 1 | BattleFrontLine.cpp:54 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontLine` | Value | -1 | BattleFrontLine.cpp:121 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontOpen` | Value | 40 | BattleFrontLine.cpp:59 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontOrderScript` | Value | 空文字列 | BattleFrontLine.cpp:1484 |  |
| `-BattleFrontPlan` | Value | 0 | BattleFrontLine.cpp:1203 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanFlex` | Value | 0 | BattleFrontLine.cpp:1317 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanFocus2` | Value | 0 | BattleFrontLine.cpp:1384 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanLog` | Value | 0 | BattleFrontLine.cpp:1089 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanPlayer` | Value | 0 | BattleFrontLine.cpp:1417 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanTeam` | Value | -1 | BattleFrontLine.cpp:1204 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPostSector` | Value | 0 | BattleMatch.cpp:531 |  |
| `-BattleFrontReserve` | Value | 0 | BattleFrontLine.cpp:61 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontSectionOrders` | Value | 0 | BattleFrontLine.cpp:1468 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontShare` | Value | 1.f | BattleFrontLine.cpp:44 |  |
| `-BattleFrontSquads` | Value | 4 | BattleFrontLine.cpp:229 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFX` | Value | 1 | BattleEffects.cpp:35 |  |
| `-BattleFXCheck` | Param | off | BattleFXCheck.cpp:186 |  |
| `-BattleGrenadeDamage` | Value | 90.f | BattleArms.cpp:153 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGrenadeRadius` | Value | 500.f | BattleArms.cpp:153 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGrenadeRange` | Value | 3000.f | BattleArms.cpp:154 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGrenades` | Value | 2 | BattleArms.cpp:41 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleGrenadeSquadGap` | Value | 3.f | BattleArms.cpp:163 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGroupFireSupport` | Value | -1 | BattleArtillery.cpp:436, BattleGroups.cpp:298 | -1 = on for both teams (default, user 2026-11-21) |
| `-BattleGroupSectors` | Value | 0 | BattleGroups.cpp:570 |  |
| `-BattleGroupStrength` | Value | 0 | BattleGroups.cpp:652 |  |
| `-BattleGroupSupplySlot` | Value | 0 | BattleGroups.cpp:75 |  |
| `-BattleHeavyCost` | Value | 未解決 | BattleState.cpp:580 |  |
| `-BattleHeavyDamage` | Value | 未解決 | BattleState.cpp:580 |  |
| `-BattleHeavyHealth` | Value | 未解決 | BattleState.cpp:579 |  |
| `-BattleHeavyMembers` | Value | 未解決 | BattleState.cpp:579 |  |
| `-BattleHeavySpeed` | Value | 未解決 | BattleState.cpp:580 |  |
| `-BattleHeightTest` | Param | off | BattleTerrainTest.cpp:27 |  |
| `-BattleHireDelegate` | Value | 0 | BattleArea.cpp:234 |  |
| `-BattleHireDelegateTeam` | Value | -1 | BattleArea.cpp:235 |  |
| `-BattleHireMirror` | Value | 1 | BattleLeaderRuntime.cpp:1857 | default on (user decision 2026-11-22) |
| `-BattleHireReserve` | Value | 1 | BattleArea.cpp:177 |  |
| `-BattleHireSightRefresh` | Value | 0 | BattleMixedRecruitment.cpp:122 |  |
| `-BattleHitDirection` | Value | 1 | BattleHUD.cpp:1250 |  |
| `-BattleHitFace` | Value | 0 | BattleLeaderRuntime.cpp:1743 |  |
| `-BattleHitReveal` | Value | 0 | BattleLeaderRuntime.cpp:1748 |  |
| `-BattleKillLog` | Value | 1 | BattleHUD.cpp:1355 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLauncherDamage` | Value | 60.f | BattleArms.cpp:160 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:16 |  |
| `-BattleLeaderHealth` | Value | 3.0 | BattleTactics.cpp:23 |  |
| `-BattleLeaderNoControl` | Value | 未解決 | BattleLeaderRuntime.cpp:144 |  |
| `-BattleLeaderQA` | Value | 空文字列 | BattleLeaderRuntime.cpp:268 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:266 |  |
| `-BattleLeaderUnits` | Value | 未解決 | BattleLeaderRuntime.cpp:96 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:807 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:106 |  |
| `-BattleMachineRepairRate` | Value | 0.f | BattleArtillery.cpp:155 |  |
| `-BattleMapStats` | Value | 空文字列 | BattleLeaderRuntime.cpp:244, BattleMapStats.cpp:40 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value | 空文字列 | BattleMatchSoak.cpp:56 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:18 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:14 |  |
| `-BattleMedicRate` | Value | 4.f | BattleArms.cpp:172 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleMelee` | Value | 1 | BattleArms.cpp:42 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleMeleeDamage` | Value | 60.f | BattleArms.cpp:166 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleMissionResumeSmoke` | Param | off | BattleLeaderRuntime.cpp:1658 |  |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:81 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:228 |  |
| `-BattleMixedSaveSmoke` | Param | off | BattleArtillery.cpp:272, BattleLeaderRuntime.cpp:230, BattleMixedBodies.cpp:115, BattleSave.cpp:101 |  |
| `-BattleMoveCourse` | Value | 空文字列 | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:93 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:92 |  |
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
| `-BattleNoAnimationLOD` | Param | off | BattleBenchmark.cpp:157, BattleBenchmark.cpp:158, BattleNetworkBenchmark.cpp:78, BattleUnit.cpp:800 |  |
| `-BattleNoCoverFallback` | Value | 0 | BattleLeaderRuntime.cpp:1738 |  |
| `-BattleNoMuzzleFlash` | Param | off | BattleMuzzleFlash.cpp:65 |  |
| `-BattleNoStagger` | Param | off | BattleBenchmark.cpp:54 |  |
| `-BattleObservedArmour` | Strifind/Value | 0 | BattleAreaSmoke.cpp:528, BattleArmourAI.cpp:34 |  |
| `-BattleObservedTactics` | Strifind/Value | 0 | BattleAreaSmoke.cpp:527, BattleArmourAI.cpp:40 |  |
| `-BattleOffscreenMarkers` | Value | 1 | BattleHUD.cpp:1288 |  |
| `-BattleOpponentPreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattleOrderSync` | Value | 1 | BattleArea.cpp:363 | default on (user decision 2026-11-22) |
| `-BattleOwnTarget` | Value | 0 | BattleSoldierFire.cpp:18 |  |
| `-BattleOwnTargetCover` | Value | 0 | BattleSoldierFire.cpp:32 |  |
| `-BattleOwnTargetTeam` | Value | -1 | BattleSoldierFire.cpp:19 |  |
| `-BattlePeekRangeCheck` | Value | 0 | BattleCoverAction.cpp:25 | trial: projected peeks must be 100..450 cm from their anchor |
| `-BattlePerceptionGrid` | Value | True | BattleLeaderRuntime.cpp:111 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value | 空文字列 | BattlePhase3.cpp:45, BattleUnit.cpp:42, BattleVATDisplay.cpp:421, BattleVATDisplay.cpp:424 |  |
| `-BattlePinnedFlank` | Value | -1 | BattleLeaderRuntime.cpp:1852 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattlePlayerRouteTest` | Param | off | BattlePlayerRoutes.cpp:128 |  |
| `-BattlePlayerWallCover` | Value | 1 | BattleWallCover.cpp:38 |  |
| `-BattlePreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleQuitMenuShot` | Value | 空文字列 | BattleQuitMenu.cpp:115 |  |
| `-BattleRagdoll` | Value | 1 | BattleRagdoll.cpp:9 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:87 |  |
| `-BattleRangeEngage` | Value | 15000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeEngageSupport` | Value | 25000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeProfile` | Value | -1 | BattleRanges.cpp:15 |  |
| `-BattleRangeReach` | Value | 50000.f | BattleRanges.cpp:22 |  |
| `-BattleRangeReachSupport` | Value | 60000.f | BattleRanges.cpp:22 |  |
| `-BattleRangeRings` | Value | 1 | BattleHUD.cpp:910 |  |
| `-BattleRangeRingShot` | Value | 空文字列 | BattleHUD.cpp:985 |  |
| `-BattleRangeSight` | Value | 20000.f | BattleRanges.cpp:19 |  |
| `-BattleReacquireGrace` | Value | True | BattleTactics.cpp:161 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:100 |  |
| `-BattleReduceHiddenBones` | Value | True | BattleLeaderRuntime.cpp:104 |  |
| `-BattleRepairDepot` | Value | 0.f | BattleArtillery.cpp:100 |  |
| `-BattleRepairHoldFire` | Value | 0 | BattleArtillery.cpp:160 |  |
| `-BattleReplayArm` | Value | 空文字列 | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value | 空文字列 | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value | 空文字列 | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value | 空文字列 | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:186 |  |
| `-BattleReuseSightContext` | Value | True | BattleLeaderRuntime.cpp:108 |  |
| `-BattleRifleMembers` | Value | 未解決 | BattleState.cpp:592 |  |
| `-BattleRoutes` | Value | -1 | BattleRoutes.cpp:20 |  |
| `-BattleRouteTieSymmetric` | Value | 0 | BattleRoutes.cpp:46 |  |
| `-BattleSaveReloadBoundary` | Value | 0 | BattleArtillery.cpp:266 |  |
| `-BattleSaveRoutes` | Value | 0 | BattleSave.cpp:94 |  |
| `-BattleScaleScreenshot` | Value | 空文字列 | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0（BattleController.cpp:103） / 20（BattleGameMode.cpp:79） | BattleController.cpp:103, BattleGameMode.cpp:79 |  |
| `-BattleScopeShot` | Param | off | BattleLeaderRuntime.cpp:240 |  |
| `-BattleScoutRetreat` | Value | true | BattleScoutReport.cpp:22 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSectorAI` | Value | 0 | BattleMatch.cpp:499, BattleMatch.cpp:514, BattleMatch.cpp:525 |  |
| `-BattleSectorAudit` | Param | off | BattleSectorAudit.cpp:28 |  |
| `-BattleSectorContact` | Value | -1 | BattleLeaderRuntime.cpp:1708 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorCoverRoute` | Value | -1 | BattleSectorRoute.cpp:22 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorCoverWalk` | Value | 0 | BattleLeaderRuntime.cpp:1842 |  |
| `-BattleSectorFlank` | Value | -1 | BattleSectorRoute.cpp:124 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorFront` | Value | -1 | BattleSectorRoute.cpp:130 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorMove` | Value | 0 | BattleLeaderRuntime.cpp:1713 |  |
| `-BattleSectorMoveFire` | Value | -1 | BattleLeaderRuntime.cpp:1723 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorRoute` | Value | -1 | BattleSectorRoute.cpp:16 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorSight` | Value | 1 | BattleSectorSight.cpp:14 |  |
| `-BattleSectorSightHit` | Value | 1 | BattleSectorSight.cpp:21 |  |
| `-BattleSectorSightNear` | Value | 2000. | BattleSectorSight.cpp:28 |  |
| `-BattleSectorSightTeam` | Value | -1 | BattleSectorSight.cpp:15 |  |
| `-BattleSectorSlotReserve` | Value | 1 | BattleStableMovement.cpp:99 |  |
| `-BattleSectorSmoke` | Param | off | BattleLeaderRuntime.cpp:222 |  |
| `-BattleSectorSymmetric` | Value | 1 | BattleLeaderRuntime.cpp:1862 | default on since 2026-11-22 (user decision) |
| `-BattleSectorViewShot` | Value | 空文字列 | BattleHUD.cpp:1404 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSenseMode` | Value | 空文字列 | BattleBenchmark.cpp:29 |  |
| `-BattleSetupPhase` | Value | 0 | BattleArea.cpp:362 |  |
| `-BattleSFX` | Value | 1 | BattleAudio.cpp:185 |  |
| `-BattleShotDirection` | Value | 0 | BattleLeaderRuntime.cpp:1943 |  |
| `-BattleShotLog` | Param | off | BattleLeaderRuntime.cpp:1357 |  |
| `-BattleSidearms` | Value | 1 | BattleArms.cpp:40 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleSightExperiment` | Value | 未解決 | BattleLeaderRuntime.cpp:84 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:400 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:360, BattlePerception.cpp:275 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:274 |  |
| `-BattleSightRange` | Value | 空文字列 | BattleState.cpp:19 |  |
| `-BattleSightRayStats` | Value | 0 | BattlePerception.cpp:283 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:119 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:119 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:116 |  |
| `-BattleSightTeamMask` | Value | 1 | BattleLeaderRuntime.cpp:405 | default 1 since 2026-11-22 (user decision); =0 = own soldiers block their side's sight and bullets again |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:248 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:242 |  |
| `-BattleSituationLog` | Value | 0 | BattleStrategy.cpp:31 |  |
| `-BattleSmokeSuite` | Value | 空文字列 | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209, BattleTacticalSmoke.cpp:175 |  |
| `-BattleSmokeTest` | Param | off | BattleGameMode.cpp:142 |  |
| `-BattleSniperCost` | Value | 14 | BattleArms.cpp:48 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleSniperTeam` | Value | 1 | BattleArms.cpp:46 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleSoldierAIFocus` | Value | 0 | BattleSoldierMove.cpp:113 |  |
| `-BattleSoldierAIFocusAt` | Value | 空文字列 | BattleSoldierMove.cpp:115 |  |
| `-BattleSoldierBody` | Value | 0 | BattleSoldierBody.cpp:19 |  |
| `-BattleSoldierMove` | Value | 0 | BattleSoldierMove.cpp:14 |  |
| `-BattleSoldierMoveTeam` | Value | -1 | BattleSoldierMove.cpp:15 |  |
| `-BattleSoldierOrderShots` | Value | 空文字列 | BattleSoldierOrder.cpp:279 |  |
| `-BattleSoldierOrderTest` | Param | off | BattleSoldierOrder.cpp:247 |  |
| `-BattleSoldierWatch` | Value | 1 | BattleSoldierWatch.cpp:17 |  |
| `-BattleSoldierWatchTeam` | Value | -1 | BattleSoldierWatch.cpp:18 |  |
| `-BattleSPGAmmo` | Value | 1 | BattleArtillery.cpp:279 |  |
| `-BattleSPGRange` | Strifind/Value | 100000.f | BattleAreaSmoke.cpp:533, BattleArtillery.cpp:250 |  |
| `-BattleSPGSetup` | Value | -1.f | BattleArtillery.cpp:138 |  |
| `-BattleSPGStayAfterFire` | Value | 0.f | BattleArtillery.cpp:143 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSquadCoverCheck` | Value | 0 | BattleLeaderRuntime.cpp:1728 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadIcons` | Value | 1 | BattleHUD.cpp:260, BattleHUD.cpp:1100 |  |
| `-BattleSquadKnowledge` | Value | 0 | BattlePerception.cpp:112 |  |
| `-BattleSquadKnowledgeCover` | Value | 0 | BattleCoverAction.cpp:420 |  |
| `-BattleSquadKnowledgeTeam` | Value | -1 | BattlePerception.cpp:113 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value | 空文字列 | BattleGameMode.cpp:64, BattleUnit.cpp:107 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:258 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:89 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:42 |  |
| `-BattleStanceMove` | Value | 1 | BattleStance.cpp:62 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:27 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:238 |  |
| `-BattleStanceSteady` | Value | 1 | BattleStance.cpp:67 |  |
| `-BattleStartRunning` | Param | off | BattleLeaderRuntime.cpp:213 |  |
| `-BattleStartSquads` | Value | 25 | BattleArea.cpp:364 |  |
| `-BattleStateTrace` | Value | 0 | BattleStateTrace.cpp:24 |  |
| `-BattleStateTraceDetail` | Value | 空文字列 | BattleStateTrace.cpp:26 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:126 |  |
| `-BattleStrategyFocusHold` | Value | 90.f | BattleStrategy.cpp:259 | Seconds → BattleStrategy.cpp:246（ヘルパー経由） |
| `-BattleStrategyOpen` | Value | 150.f | BattleStrategy.cpp:253 | Seconds → BattleStrategy.cpp:246（ヘルパー経由） |
| `-BattleSTScreens` | Value | 空文字列 | BattleSquadTacticsSmoke.cpp:127 |  |
| `-BattleSupplyAmmoShare` | Value | 0.f | BattleTactics.cpp:34 |  |
| `-BattleSupplyAwayRatio` | Value | 1.0 | BattleTactics.cpp:52 |  |
| `-BattleSupplyHeal` | Value | 10.0 | BattleTactics.cpp:52 |  |
| `-BattleSupplyResumeHealth` | Value | 0.9 | BattleTactics.cpp:53 |  |
| `-BattleSupplyResumeLiving` | Value | 10 | BattleTactics.cpp:51 |  |
| `-BattleSupplyReturnLiving` | Value | 8 | BattleTactics.cpp:51 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:256 |  |
| `-BattleSuppression` | Value | 0 | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionDecay` | Value | .25f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionPerShot` | Value | .12f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionPin` | Value | .5f | BattleSuppression.cpp:16 |  |
| `-BattleSuppressionRadius` | Value | 200.f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionSpread` | Value | 1.5f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionSupport` | Value | 1.f | BattleSuppression.cpp:17 |  |
| `-BattleSurpriseGroup` | Value | 0 | BattleFrontLine.cpp:896 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleSurpriseSquads` | Value | 2 | BattleFrontLine.cpp:918 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleSymmetricStart` | Value | -1 | BattleGroups.cpp:59 |  |
| `-BattleSymmetryAudit` | Param | off | BattleCover.cpp:77 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTankGunFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:530, BattleArtillery.cpp:51 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:13 |  |
| `-BattleTerrainLineBodies` | Value | 1 | BattleTactics.cpp:256 |  |
| `-BattleThinkCap` | Value | 0 | BattleLeaderRuntime.cpp:1627 |  |
| `-BattleThreatCover` | Value | 0 | BattleThreatCover.cpp:23 |  |
| `-BattleThreatCoverCrouch` | Value | 0 | BattleThreatCover.cpp:67 |  |
| `-BattleThreatCoverCrouchTeam` | Value | -1 | BattleThreatCover.cpp:68 |  |
| `-BattleThreatCoverRelocate` | Value | 0 | BattleThreatCover.cpp:30 |  |
| `-BattleThreatCoverTeam` | Value | -1 | BattleThreatCover.cpp:24 |  |
| `-BattleTracerShot` | Param | off | BattleStanceShot.cpp:87 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:232 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:18, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:86, BattleMatch.cpp:27, BattleMatch.cpp:293, BattleMatch.cpp:312, BattleWideMapAudit.cpp:116 |  |
| `-BattleUnderFireCover` | Value | 0 | BattleLeaderRuntime.cpp:1837 |  |
| `-BattleUnits` | Value | 20（BattleBenchmark.cpp:36） / 1000（BattleSquadTrial.cpp:28） | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVehicleHitCover` | Strifind/Value | 0 | BattleAreaSmoke.cpp:529, BattleArtillery.cpp:45 |  |
| `-BattleVehicleOrderHere` | Value | 0 | BattleArtillery.cpp:39 |  |
| `-BattleVehicleRoadGoal` | Value | 0 | BattleLeaderRuntime.cpp:577 |  |
| `-BattleVerifyMixedSave` | Param | off | BattleSave.cpp:546, BattleSightExperiment.cpp:42 |  |
| `-BattleVerifySaveAt` | Value | 80.f | BattleSightExperiment.cpp:41 |  |
| `-BattleWalkLook` | Value | 0 | BattleSoldierWatch.cpp:24 |  |
| `-BattleWalkLookTeam` | Value | -1 | BattleSoldierWatch.cpp:25 |  |
| `-BattleWaypointPass` | Value | 1 | BattleStableMovement.cpp:520 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponOrderGate` | Value | 0 | BattleWeaponOrders.cpp:10 |  |
| `-BattleWeaponPreview` | Value | 空文字列 | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value | 空文字列 | BattleState.cpp:44 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:232 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:246 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:80 |  |
| `-BattleWideScreens` | Value | 空文字列（BattleAreaSmoke.cpp:1309） / 空文字列（BattleContactTrial.cpp:97） / 空文字列（BattleDeathShot.cpp:42） / 空文字列（BattleExoShot.cpp:107） / 未解決（BattleFXCheck.cpp:197） / 空文字列（BattleMixedFastSmoke.cpp:34） / 空文字列（BattleMixedSaveSmoke.cpp:306） / 空文字列（BattleMovementTrial.cpp:108） / 空文字列（BattleScopeShot.cpp:45） / 空文字列（BattleSightVisual.cpp:30） / 空文字列（BattleSquadFlankSmoke.cpp:165） / 空文字列（BattleSquadRetreatSmoke.cpp:22） / 空文字列（BattleStanceShot.cpp:96） / 空文字列（BattleTeamSupportSmoke.cpp:122） / 空文字列（BattleWideMapAudit.cpp:40） / 空文字列（BattleWideMapAudit.cpp:159） / 空文字列（BattleWideMapAudit.cpp:190） / 空文字列（BattleWideMixedSmoke.cpp:39） / 空文字列（BattleWideMixedSmoke.cpp:53） | BattleAreaSmoke.cpp:1309, BattleContactTrial.cpp:97, BattleDeathShot.cpp:42, BattleExoShot.cpp:107, BattleFXCheck.cpp:197, BattleMixedFastSmoke.cpp:34, BattleMixedSaveSmoke.cpp:306, BattleMovementTrial.cpp:108, BattleScopeShot.cpp:45, BattleSightVisual.cpp:30, BattleSquadFlankSmoke.cpp:165, BattleSquadRetreatSmoke.cpp:22, BattleStanceShot.cpp:96, BattleTeamSupportSmoke.cpp:122, BattleWideMapAudit.cpp:40, BattleWideMapAudit.cpp:159, BattleWideMapAudit.cpp:190, BattleWideMixedSmoke.cpp:39, BattleWideMixedSmoke.cpp:53 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:260 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:262 |  |
| `-BayFlank` | Value | 0 | BattleFlankBayTrial.cpp:17 |  |
| `-BayOutput` | Value | 空文字列 | BattleFlankBayTrial.cpp:62 |  |
| `-BayRendered` | Param | off | BattleFlankBayTrial.cpp:66 |  |
| `-BaySign` | Value | 1 | BattleFlankBayTrial.cpp:17 |  |
| `-CoverScreenshot` | Value | 空文字列 | BattleCoverActionSmoke.cpp:55, BattleCoverActionSmoke.cpp:269 |  |
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
| `-P3VATAudit` | Param | off | BattleLeaderRuntime.cpp:130, BattleVATDisplay.cpp:161 |  |
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
