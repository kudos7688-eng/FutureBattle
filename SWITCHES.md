# 起動引数（試験切替）の一覧

`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。
既定値は C++ と DefaultGame.ini の静的読取りによる。解決できない値は「未解決」と記す。
UBattleTuning の Config 値は DefaultGame.ini を優先する。既定は読取り先の初期値で、マップ・陣営・実行時の変更や後処理まで保証しない。Param / Strifind の off は引数なしを示す。読取り箇所で値が異なる場合は場所を併記する。

計 545 種類。

| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |
|---|---|---|---|---|
| `-BattleACShield` | Value | 0 | BattleATHunt.cpp:48 |  |
| `-BattleACStandOff` | Value | 0 | BattleATHunt.cpp:36 |  |
| `-BattleACTankStandOff` | Value | 0 | BattleATHunt.cpp:42 |  |
| `-BattleAIBlindFire` | Value | 0 | BattleBlindFire.cpp:27 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleAICoverProbe` | Value | 0 | BattleTactics.cpp:10 |  |
| `-BattleAimAssist` | Value | 1 | BattleController.cpp:1668 |  |
| `-BattleAllyPass` | Value | 0 | BattleStableMovement.cpp:54 |  |
| `-BattleArea` | Param | off | BattleLeaderRuntime.cpp:84, BattleState.cpp:600 |  |
| `-BattleAreaACKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:225 |  |
| `-BattleAreaAllTypes` | Value | 0 | BattleArmourAI.cpp:80 |  |
| `-BattleAreaATKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:223 |  |
| `-BattleAreaAttackWeight` | Value | 2.0 | BattleTactics.cpp:203 |  |
| `-BattleAreaBaseFlank` | Value | 1.3f | BattleTactics.cpp:216 |  |
| `-BattleAreaBaseFlankSide` | Value | true | BattleTactics.cpp:218 |  |
| `-BattleAreaBaseInfiltrate` | Value | 0 | BattleTactics.cpp:212 |  |
| `-BattleAreaBaseLowHP` | Value | 500.f | BattleTactics.cpp:208 |  |
| `-BattleAreaBaseReturnLiving` | Value | 5 | BattleTactics.cpp:219 |  |
| `-BattleAreaBaseRule` | Value | 0 | BattleTactics.cpp:205 |  |
| `-BattleAreaBaseShare` | Value | 0.2 | BattleTactics.cpp:204 |  |
| `-BattleAreaBaseShareHigh` | Value | 0.3f | BattleTactics.cpp:207 |  |
| `-BattleAreaBaseShareLow` | Value | 0.f | BattleTactics.cpp:206 |  |
| `-BattleAreaComposition` | Value | -1 | BattleArmourAI.cpp:71 |  |
| `-BattleAreaCompositionPlanner` | Value | 0 | BattleComposition.cpp:12 |  |
| `-BattleAreaCompositionPlannerTeams` | Value | 0 | BattleComposition.cpp:13 |  |
| `-BattleAreaDepot` | Value | 0 | BattleAreaDepot.cpp:22 |  |
| `-BattleAreaDepotRaid` | Value | 未解決 | BattleAreaDepot.cpp:23 |  |
| `-BattleAreaEliteKeep` | Value | 2 | BattleArms.cpp:44 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleAreaEnsureTypes` | Value | 0 | BattleArmourAI.cpp:88 |  |
| `-BattleAreaExoEarly` | Value | 0 | BattleArea.cpp:425 |  |
| `-BattleAreaExoKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:229 |  |
| `-BattleAreaFocus` | Value | 1.0 | BattleTactics.cpp:214 |  |
| `-BattleAreaFrontBalance` | Value | True | BattleTactics.cpp:209 |  |
| `-BattleAreaFrontGain` | Value | 1.0 | BattleTactics.cpp:210 |  |
| `-BattleAreaGroups` | Value | -1 | BattleGroups.cpp:109 |  |
| `-BattleAreaHeavyShare` | Value | -1.f | BattleTactics.cpp:221 |  |
| `-BattleAreaHire` | Value | True | BattleTactics.cpp:183 |  |
| `-BattleAreaHireInterval` | Value | 0.f | BattleTactics.cpp:185 |  |
| `-BattleAreaHirePurpose` | Value | 0 | BattleTactics.cpp:188 |  |
| `-BattleAreaHirePurposeFront` | Value | 10 | BattleTactics.cpp:189 |  |
| `-BattleAreaHireReserve` | Value | 0 | BattleTactics.cpp:185 |  |
| `-BattleAreaHireStop` | Value | 0.f | BattleTactics.cpp:186 |  |
| `-BattleAreaIncome` | Value | True | BattleTactics.cpp:249 |  |
| `-BattleAreaIncomeBase` | Value | 未解決 | BattleTactics.cpp:252 |  |
| `-BattleAreaIncomePerPoint` | Value | 未解決 | BattleTactics.cpp:252 |  |
| `-BattleAreaIncomeRamp` | Value | 0.f | BattleTactics.cpp:257 |  |
| `-BattleAreaIncomeUrbanBase` | Value | -1 | BattleTactics.cpp:255 |  |
| `-BattleAreaIncomeUrbanPerPoint` | Value | -1 | BattleTactics.cpp:255 |  |
| `-BattleAreaIntel` | Value | false | BattleAreaIntel.cpp:15 |  |
| `-BattleAreaMaxSquads` | Value | 30 | BattleTactics.cpp:187 |  |
| `-BattleAreaMortarKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:226 |  |
| `-BattleAreaPlan` | Value | 0 | BattleTactics.cpp:220 |  |
| `-BattleAreaPointSupply` | Value | True | BattleTactics.cpp:167 |  |
| `-BattleAreaScoutKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:222 |  |
| `-BattleAreaSmoke` | Param | off | BattleArea.cpp:49 |  |
| `-BattleAreaSniperKeep` | Value | 2 | BattleArms.cpp:47 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleAreaSPGCost` | Value | 35 | BattleArtillery.cpp:243 |  |
| `-BattleAreaSPGKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:227 |  |
| `-BattleAreaStartCredits` | Value | 100 | BattleTactics.cpp:258 |  |
| `-BattleAreaStrategy` | Value | True | BattleTactics.cpp:202 |  |
| `-BattleAreaSupplyEarly` | Value | 0 | BattleArea.cpp:429 |  |
| `-BattleAreaSupplyKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:231 |  |
| `-BattleAreaTankKeep` | Value | -1（= 設定値 0） | BattleTactics.cpp:224 |  |
| `-BattleAreaTrialTeam` | Value | -1 | BattleTactics.cpp:232 |  |
| `-BattleAreaWarPointSeconds` | Value | 5.f | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarResourceValue` | Value | 1 | BattleWarScore.cpp:11 |  |
| `-BattleAreaWarScore` | Value | 0 | BattleWarScore.cpp:10 |  |
| `-BattleAreaWarTarget` | Value | 0 | BattleWarScore.cpp:12 |  |
| `-BattleAreaWithout` | Value | -1 | BattleArmourAI.cpp:94 |  |
| `-BattleArmourAI` | Value | true | BattleArmourAI.cpp:62 |  |
| `-BattleArmourAITeam` | Value | -1 | BattleArmourAI.cpp:63 |  |
| `-BattleArmourSectorGate` | Value | 0 | BattleWeaponOrders.cpp:16 |  |
| `-BattleArmourWait` | Value | 0 | BattleArmourAI.cpp:306, BattlePerception.cpp:109 |  |
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
| `-BattleArrivalThreat` | Value | -1 | BattleLeaderRuntime.cpp:2091 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleArrivalWalk` | Value | 1 | BattleLeaderRuntime.cpp:1876 |  |
| `-BattleArriveWatch` | Value | 0.f | BattleTactics.cpp:34 |  |
| `-BattleArtilleryClusters` | Value | 0 | BattleArtillery.cpp:278 |  |
| `-BattleArtilleryFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:495, BattleArtillery.cpp:57 |  |
| `-BattleArtilleryHomeGap` | Value | 0.f | BattleArtillery.cpp:105 |  |
| `-BattleArtilleryObserved` | Value | 0 | BattleArtillery.cpp:33 |  |
| `-BattleArtilleryPostRisk` | Value | 0.f | BattleArtillery.cpp:175 |  |
| `-BattleArtilleryPosts` | Strifind/Value | 0 | BattleAreaSmoke.cpp:494, BattleArtillery.cpp:212 |  |
| `-BattleArtilleryRaidChase` | Value | 0 | BattleArtillery.cpp:218 |  |
| `-BattleArtilleryRaidGroup` | Value | 0 | BattleArtillery.cpp:230 |  |
| `-BattleArtilleryRaidScreen` | Value | 2 | BattleArtillery.cpp:236 |  |
| `-BattleArtilleryRaidSneak` | Value | 0.f | BattleArtillery.cpp:224 |  |
| `-BattleArtilleryStandoff` | Value | .6f | BattleArtillery.cpp:259 |  |
| `-BattleArtilleryVehicleSplash` | Value | 0.f | BattleArtillery.cpp:90 |  |
| `-BattleATFire` | Value | 0 | BattleATHunt.cpp:30 |  |
| `-BattleATHunt` | Value | 0 | BattleATHunt.cpp:24 |  |
| `-BattleATRange` | Value | 0 | BattleATHunt.cpp:17 |  |
| `-BattleAttackFocus` | Value | 1 | BattleLeaderRuntime.cpp:2037 |  |
| `-BattleBaseGarrison` | Value | 0 | BattleTactics.cpp:129 |  |
| `-BattleBaseGarrisonRadius` | Value | 6000.f | BattleTactics.cpp:133 |  |
| `-BattleBaseGarrisonSeconds` | Value | 30.f | BattleTactics.cpp:137 |  |
| `-BattleBaseGarrisonSmoke` | Param | off | BattleLeaderRuntime.cpp:1748 |  |
| `-BattleBaseGroup` | Value | 0 | BattleBaseGroup.cpp:35 |  |
| `-BattleBaseRifleDamage` | Value | 0.f | BattleTactics.cpp:141 |  |
| `-BattleBenchmark` | Param | off | BattleBenchmark.cpp:28, BattleController.cpp:105, BattleLeaderRuntime.cpp:80 |  |
| `-BattleBenchmarkFPS` | Param | off | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |  |
| `-BattleBlastOcclusion` | Value | 0 | BattleArtillery.cpp:272 |  |
| `-BattleBlindFire` | Value | 1 | BattleBlindFire.cpp:26 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireClipSide` | Value | "AS_CoverLo_AimR" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireClipTop` | Value | "AS_CoverLo_AimU_down" | BattleBlindFire.cpp:44 |  |
| `-BattleBlindFireExpose` | Value | 1 | BattleBlindFire.cpp:77 | Switch → BattleBlindFire.cpp:14（ヘルパー経由） |
| `-BattleBlindFireSpread` | Value | 4.f | BattleBlindFire.cpp:30 |  |
| `-BattleBodyAnim` | Value | 1 | BattleBodyAnim.cpp:59 |  |
| `-BattleBounding` | Value | 0 | BattleLeaderRuntime.cpp:1891 |  |
| `-BattleBurstFire` | Value | 0 | BattleTactics.cpp:109 |  |
| `-BattleBurstPause` | Value | 1.f | BattleTactics.cpp:114 |  |
| `-BattleBuyPanelShot` | Value | 空文字列 | BattleBuyPanel.cpp:244 |  |
| `-BattleCarbineReserve` | Value | -1 | BattleTactics.cpp:151 |  |
| `-BattleCarbineShotRounds` | Value | 1 | BattleTactics.cpp:124 |  |
| `-BattleCarRecoverShots` | Param | off | BattleSupplyCar.cpp:45 |  |
| `-BattleCloseIn` | Value | 0 | BattleLeaderRuntime.cpp:2005 |  |
| `-BattleContactAvoidance` | Value | 1 | BattleLeaderRuntime.cpp:92 |  |
| `-BattleContactCover` | Value | 0 | BattleTactics.cpp:310 |  |
| `-BattleContactCoverTeam` | Value | -1 | BattleTactics.cpp:304 |  |
| `-BattleContactTrial` | Param | off | BattleLeaderRuntime.cpp:93 |  |
| `-BattleCorpseLifetime` | Value | 20.f | BattleCorpses.cpp:35 |  |
| `-BattleCorpseSteps` | Value | 63 | BattleCorpses.cpp:29 |  |
| `-BattleCounterBattery` | Value | 0 | BattleArtillery.cpp:66 |  |
| `-BattleCounterBatteryRest` | Value | 60.f | BattleArtillery.cpp:82 |  |
| `-BattleCounterBatteryShots` | Value | 0 | BattleArtillery.cpp:77 |  |
| `-BattleCoverActions` | Value | 0 | BattleSquadTactics.cpp:19, BattleTactics.cpp:328 |  |
| `-BattleCoverActionSmoke` | Param | off | BattleLeaderRuntime.cpp:228 |  |
| `-BattleCoverApproach` | Value | -1 | BattleTactics.cpp:298 |  |
| `-BattleCoverArrival` | Value | 1 | BattleStableMovement.cpp:180 |  |
| `-BattleCoverDense` | Value | 0 | BattleLeaderRuntime.cpp:2076 |  |
| `-BattleCoverFire` | Value | True | BattleTactics.cpp:274 |  |
| `-BattleCoverFirePoint` | Value | 0 | BattleTactics.cpp:317 | trial, default 0 |
| `-BattleCoverFirePointTrial` | Param | off | BattleLeaderRuntime.cpp:254 |  |
| `-BattleCoverHuddle` | Value | -1 | BattleStableMovement.cpp:303 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleCoverIndoor` | Value | 0 | BattleLeaderRuntime.cpp:2081 |  |
| `-BattleCoverOutward` | Value | 0 | BattleLeaderRuntime.cpp:2086 |  |
| `-BattleCoverPeekLookout` | Value | 1 | BattleCoverAction.cpp:25 | AI only, default 1 (user 11-26); =0 restores own-sighting-only peeks |
| `-BattleCoverPeekShots` | Value | 0 | BattleTactics.cpp:119 |  |
| `-BattleCoverPeekSight` | Value | True | BattleTactics.cpp:286 |  |
| `-BattleCoverPeekTrial` | Param | off | BattleCoverPeekTrial.cpp:16, BattleLeaderRuntime.cpp:226, BattleSightExperiment.cpp:25 |  |
| `-BattleCoverPriority` | Value | 1 | BattleLeaderRuntime.cpp:1990 |  |
| `-BattleCoverSlotsMirror` | Value | 0 | BattleCover.cpp:82 |  |
| `-BattleCoverStrict` | Value | 0 | BattleLeaderRuntime.cpp:2096 |  |
| `-BattleCoverValidation` | Value | True | BattleTactics.cpp:280 |  |
| `-BattleCutOffRoute` | Value | 0 | BattleOwnRouteSmoke.cpp:45, BattleStableMovement.cpp:97 |  |
| `-BattleDangerRoute` | Value | 0 | BattleSectorRoute.cpp:28 |  |
| `-BattleDeathShot` | Param | off | BattleLeaderRuntime.cpp:238 |  |
| `-BattleDefendEngage` | Value | false | BattleGroups.cpp:50 |  |
| `-BattleDelegationScreens` | Value | 空文字列 | BattleDelegationSmoke.cpp:78 |  |
| `-BattleDepotHealth` | Value | 0.f | BattleArtillery.cpp:165 |  |
| `-BattleDepotRebuild` | Value | 120.f | BattleArtillery.cpp:170 |  |
| `-BattleDiagnosticOmit` | Value | 空文字列 | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |  |
| `-BattleDisplayLOD` | Value | 1 | BattleLeaderRuntime.cpp:129, BattleWideMixedSmoke.cpp:170 |  |
| `-BattleElite` | Value | 1 | BattleArms.cpp:43 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleEliteCost` | Value | 26 | BattleArms.cpp:45 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleEngineMoveSweep` | Value | 0 | BattleStableMovement.cpp:752 |  |
| `-BattleExoArmor` | Value | -1.f | BattleState.cpp:610 | ExoTrialValue → BattleState.cpp:608（ヘルパー経由） |
| `-BattleExoBody` | Value | 3 | BattleExo.cpp:123 |  |
| `-BattleExoDamage` | Value | -1.f | BattleState.cpp:611 | ExoTrialValue → BattleState.cpp:608（ヘルパー経由） |
| `-BattleExoHealth` | Value | -1.f | BattleState.cpp:626 | ExoTrialValue → BattleState.cpp:608（ヘルパー経由） |
| `-BattleExoPlaceholder` | Param | off | BattleExo.cpp:145 |  |
| `-BattleExoShield` | Value | -1.f | BattleState.cpp:612 | ExoTrialValue → BattleState.cpp:608（ヘルパー経由） |
| `-BattleExoShieldDelay` | Value | -1.f | BattleSuppression.cpp:90 |  |
| `-BattleExoShieldRegen` | Value | -1.f | BattleState.cpp:614 | ExoTrialValue → BattleState.cpp:608（ヘルパー経由） |
| `-BattleExoShieldSuppression` | Value | 0 | BattleSuppression.cpp:83 |  |
| `-BattleExoShot` | Param | off | BattleLeaderRuntime.cpp:236 |  |
| `-BattleFastMatch` | Param | off | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:80 |  |
| `-BattleFieldDiagnostics` | Param | off | BattleSquadTrial.cpp:17 |  |
| `-BattleFieldRecord` | Value | 空文字列 | BattleSquadTrial.cpp:15 |  |
| `-BattleFieldReplay` | Value | 空文字列 | BattleSquadTrial.cpp:16 |  |
| `-BattleFieldSeed` | Value | 7 | BattleSquadTrial.cpp:19 |  |
| `-BattleFieldSupplyLog` | Param | off | BattleSquadRetreat.cpp:216 |  |
| `-BattleFireTimeLog` | Param | off | BattleLeaderRuntime.cpp:945 |  |
| `-BattleFirstPerson` | Param | off | BattleUnit.cpp:41 |  |
| `-BattleFlankBayTrial` | Param | off | BattleLeaderRuntime.cpp:230 |  |
| `-BattleFlankCoordination` | Value | 0 | BattleSquadTactics.cpp:20 |  |
| `-BattleFlankRelaxed` | Value | 0 | BattleSquadTactics.cpp:23 |  |
| `-BattleFlankSmoke` | Param | off | BattleLeaderRuntime.cpp:260 |  |
| `-BattleFlankTeams` | Value | 3 | BattleSquadTactics.cpp:24 |  |
| `-BattleFlankTrial` | Param | off | BattleSightExperiment.cpp:39, BattleSquadFlank.cpp:130, BattleSquadRetreat.cpp:19 |  |
| `-BattleFocusTarget` | Value | 0 | BattleLeaderRuntime.cpp:2042 |  |
| `-BattleFollowerAnimBudget` | Value | 0 | BattleVATDisplay.cpp:39 |  |
| `-BattleFollowerAnimSkip` | Value | 0 | BattleVATDisplay.cpp:44 |  |
| `-BattleFollowerNoOverlap` | Value | 0 | BattleLeaderRuntime.cpp:927 |  |
| `-BattleFrontArmies` | Value | 0 | BattleFrontArmies.cpp:28 |  |
| `-BattleFrontArmy` | Value | 1 | BattleFrontLine.cpp:33 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontAuto` | Value | 1 | BattleFrontLine.cpp:38 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontGap` | Value | 1 | BattleFrontLine.cpp:48 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontHold` | Value | 1 | BattleFrontLine.cpp:54 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontLine` | Value | -1 | BattleFrontLine.cpp:121 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontOpen` | Value | 40 | BattleFrontLine.cpp:59 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontOrderScript` | Value | 空文字列 | BattleFrontLine.cpp:1516 |  |
| `-BattleFrontPlan` | Value | 0 | BattleFrontLine.cpp:1220 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanFlex` | Value | 0 | BattleFrontLine.cpp:1334 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanFocus2` | Value | 0 | BattleFrontLine.cpp:1401 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanLog` | Value | 0 | BattleFrontLine.cpp:1106 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanPlayer` | Value | 0 | BattleFrontLine.cpp:1434 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPlanTeam` | Value | -1 | BattleFrontLine.cpp:1221 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontPostSector` | Value | 0 | BattleMatch.cpp:580 |  |
| `-BattleFrontReserve` | Value | 0 | BattleFrontLine.cpp:61 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontSectionOrders` | Value | 0 | BattleFrontLine.cpp:1499 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFrontShare` | Value | 1.f | BattleFrontLine.cpp:44 |  |
| `-BattleFrontSquads` | Value | 4 | BattleFrontLine.cpp:229 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleFX` | Value | 1 | BattleEffects.cpp:35 |  |
| `-BattleFXCheck` | Param | off | BattleFXCheck.cpp:186 |  |
| `-BattleGrenadeDamage` | Value | 90.f | BattleArms.cpp:151 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGrenadeRadius` | Value | 500.f | BattleArms.cpp:151 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGrenadeRange` | Value | 3000.f | BattleArms.cpp:152 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGrenades` | Value | 2 | BattleArms.cpp:41 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleGrenadeSquadGap` | Value | 3.f | BattleArms.cpp:161 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleGroupFireSupport` | Value | -1 | BattleArtillery.cpp:423, BattleGroups.cpp:326 | -1 = on for both teams (default, user 2026-11-21) |
| `-BattleGroupSectors` | Value | 0 | BattleGroups.cpp:598 |  |
| `-BattleGroupStrength` | Value | 0 | BattleGroups.cpp:681 |  |
| `-BattleGroupSupplySlot` | Value | 0 | BattleGroups.cpp:101 |  |
| `-BattleHeavyCost` | Value | 未解決 | BattleState.cpp:588 |  |
| `-BattleHeavyDamage` | Value | 未解決 | BattleState.cpp:588 |  |
| `-BattleHeavyHealth` | Value | 未解決 | BattleState.cpp:587 |  |
| `-BattleHeavyMembers` | Value | 未解決 | BattleState.cpp:587 |  |
| `-BattleHeavySpeed` | Value | 未解決 | BattleState.cpp:588 |  |
| `-BattleHeightTest` | Param | off | BattleTerrainTest.cpp:99 |  |
| `-BattleHireDelegate` | Value | 0 | BattleArea.cpp:235 |  |
| `-BattleHireDelegateTeam` | Value | -1 | BattleArea.cpp:236 |  |
| `-BattleHireMirror` | Value | 1 | BattleLeaderRuntime.cpp:2015 | default on (user decision 2026-11-22) |
| `-BattleHireReserve` | Value | 1 | BattleArea.cpp:178 |  |
| `-BattleHireSightRefresh` | Value | 0 | BattleMixedRecruitment.cpp:123 |  |
| `-BattleHitDirection` | Value | 1 | BattleHUD.cpp:1273 |  |
| `-BattleHitFace` | Value | 0 | BattleLeaderRuntime.cpp:1901 |  |
| `-BattleHitReveal` | Value | 0 | BattleLeaderRuntime.cpp:1906 |  |
| `-BattleJoinFight` | Value | 0 | BattleTactics.cpp:94 |  |
| `-BattleJoinFightMax` | Value | 2 | BattleTactics.cpp:104 |  |
| `-BattleJoinFightRange` | Value | 15000.f | BattleTactics.cpp:99 |  |
| `-BattleKillLog` | Value | 1 | BattleHUD.cpp:1378 |  |
| `-BattleKnownMoveMarch` | Value | 1 | BattleKnownTargetMove.cpp:18 |  |
| `-BattleKnownMoveMax` | Value | 0 | BattleKnownTargetMove.cpp:21 |  |
| `-BattleKnownMoveNavBudget` | Value | 32 | BattleKnownTargetMove.cpp:26 |  |
| `-BattleKnownMoveRange` | Value | 2500. | BattleKnownTargetMove.cpp:19 |  |
| `-BattleKnownMoveRayBudget` | Value | 32 | BattleKnownTargetMove.cpp:24 |  |
| `-BattleKnownMoveSquadRadius` | Value | 5000. | BattleKnownTargetMove.cpp:20 |  |
| `-BattleKnownMoveSweepBudget` | Value | 24 | BattleKnownTargetMove.cpp:25 |  |
| `-BattleKnownTargetFire` | Value | 0 | BattleKnownTargetMove.cpp:17, BattleKnownTargets.cpp:12 |  |
| `-BattleKnownTargetFireTrial` | Param | off | BattleKnownTargetMove.cpp:238, BattleLeaderRuntime.cpp:252 |  |
| `-BattleLargeFieldTest` | Param | off | BattleGameMode.cpp:143 |  |
| `-BattleLauncherDamage` | Value | 60.f | BattleArms.cpp:158 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleLeaderCommand` | Value | 0 | BattleGameMode.cpp:54 |  |
| `-BattleLeaderGuard` | Value | 0 | BattleTactics.cpp:16 |  |
| `-BattleLeaderHealth` | Value | 3.0 | BattleTactics.cpp:23 |  |
| `-BattleLeaderNoControl` | Value | 未解決 | BattleLeaderRuntime.cpp:146 |  |
| `-BattleLeaderQA` | Value | 空文字列 | BattleLeaderRuntime.cpp:274 |  |
| `-BattleLeaderSmoke` | Param | off | BattleLeaderRuntime.cpp:272 |  |
| `-BattleLeaderUnits` | Value | 未解決 | BattleLeaderRuntime.cpp:98 |  |
| `-BattleLegacyAnimationLOD` | Param | off | BattleUnit.cpp:807 |  |
| `-BattleLegacyWeaponVisuals` | Param | off | BattleUnit.cpp:106 |  |
| `-BattleMachineRepairRate` | Value | 0.f | BattleArtillery.cpp:155 |  |
| `-BattleMapStats` | Value | 空文字列 | BattleLeaderRuntime.cpp:246, BattleMapStats.cpp:40 |  |
| `-BattleMassMovement` | Param | off | BattleBenchmark.cpp:39 |  |
| `-BattleMatchReport` | Value | 空文字列 | BattleMatchSoak.cpp:48 |  |
| `-BattleMatchScenario` | Value | 0 | BattleMatchSoak.cpp:17 |  |
| `-BattleMatchSoak` | Param | off | BattleMatchSoak.cpp:13 |  |
| `-BattleMedicRate` | Value | 4.f | BattleArms.cpp:170 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleMelee` | Value | 1 | BattleArms.cpp:42 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleMeleeDamage` | Value | 60.f | BattleArms.cpp:164 | FloatSwitch → BattleArms.cpp:35（ヘルパー経由） |
| `-BattleMemorySeconds` | Value | 0.f | BattleTactics.cpp:29 |  |
| `-BattleMissionResumeSmoke` | Param | off | BattleLeaderRuntime.cpp:1813 |  |
| `-BattleMixedFastMatch` | Param | off | BattleLeaderRuntime.cpp:83 |  |
| `-BattleMixedFastSmoke` | Param | off | BattleLeaderRuntime.cpp:232 |  |
| `-BattleMoveCourse` | Value | 空文字列 | BattleBenchmark.cpp:37 |  |
| `-BattleMovementScene` | Value | -1 | BattleLeaderRuntime.cpp:95 |  |
| `-BattleMovementTrial` | Param | off | BattleLeaderRuntime.cpp:94 |  |
| `-BattleMoveProgress` | Value | 0 | BattleStableMovement.cpp:774 |  |
| `-BattleMoveTrace` | Value | 空文字列 | BattleGroups.cpp:95 |  |
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
| `-BattleNoCoverFallback` | Value | 0 | BattleLeaderRuntime.cpp:1896 |  |
| `-BattleNoMuzzleFlash` | Param | off | BattleMuzzleFlash.cpp:65 |  |
| `-BattleNoStagger` | Param | off | BattleBenchmark.cpp:54 |  |
| `-BattleObservedArmour` | Strifind/Value | 0 | BattleAreaSmoke.cpp:491, BattleArmourAI.cpp:34 |  |
| `-BattleObservedTactics` | Strifind/Value | 0 | BattleAreaSmoke.cpp:490, BattleArmourAI.cpp:40 |  |
| `-BattleOffscreenMarkers` | Value | 1 | BattleHUD.cpp:1311 |  |
| `-BattleOpponentPreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattleOrderSync` | Value | 1 | BattleArea.cpp:364 | default on (user decision 2026-11-22) |
| `-BattleOrderTrace` | Value | 0 | BattleMatch.cpp:595 |  |
| `-BattleOwnRouteSmoke` | Param | off | BattleLeaderRuntime.cpp:1816 |  |
| `-BattleOwnTarget` | Value | 0 | BattleSoldierFire.cpp:18 |  |
| `-BattleOwnTargetCover` | Value | 0 | BattleSoldierFire.cpp:32 |  |
| `-BattleOwnTargetTeam` | Value | -1 | BattleSoldierFire.cpp:19 |  |
| `-BattlePeekRangeCheck` | Value | 0 | BattleCoverAction.cpp:30 | trial: projected peeks must be 100..450 cm from their anchor |
| `-BattlePerceptionGrid` | Value | True | BattleLeaderRuntime.cpp:113 |  |
| `-BattlePerceptionSlices` | Value | 4 | BattleBenchmark.cpp:56 |  |
| `-BattlePhase3` | Value | 空文字列 | BattlePhase3.cpp:45, BattleUnit.cpp:42, BattleVATDisplay.cpp:413, BattleVATDisplay.cpp:416 |  |
| `-BattlePinnedFlank` | Value | -1 | BattleLeaderRuntime.cpp:2010 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattlePlanPanelShot` | Value | 空文字列 | BattleController.cpp:1861 |  |
| `-BattlePlayerRouteTest` | Param | off | BattlePlayerRoutes.cpp:127 |  |
| `-BattlePlayerWallCover` | Value | 1 | BattleWallCover.cpp:38 |  |
| `-BattlePreset` | Value | 空文字列 | BattleBenchmark.cpp:48 | ReadTier → BattleBenchmark.cpp:45（ヘルパー経由） |
| `-BattleProfileCSV` | Param | off | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |  |
| `-BattleQuitMenuShot` | Value | 空文字列 | BattleQuitMenu.cpp:114 |  |
| `-BattleRagdoll` | Value | 1 | BattleRagdoll.cpp:9 |  |
| `-BattleRangeAware` | Value | 1 | BattleLeaderRuntime.cpp:89 |  |
| `-BattleRangeEngage` | Value | 15000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeEngageSupport` | Value | 25000.f | BattleRanges.cpp:21 |  |
| `-BattleRangeProfile` | Value | -1 | BattleRanges.cpp:15 |  |
| `-BattleRangeReach` | Value | 50000.f | BattleRanges.cpp:22 |  |
| `-BattleRangeReachSupport` | Value | 60000.f | BattleRanges.cpp:22 |  |
| `-BattleRangeRings` | Value | 1 | BattleHUD.cpp:933 |  |
| `-BattleRangeRingShot` | Value | 空文字列 | BattleHUD.cpp:1008 |  |
| `-BattleRangeSight` | Value | 20000.f | BattleRanges.cpp:19 |  |
| `-BattleReacquireGrace` | Value | True | BattleTactics.cpp:267 |  |
| `-BattleReduceAnimTasks` | Value | 0 | BattleLeaderRuntime.cpp:102 |  |
| `-BattleReduceHiddenBones` | Value | True | BattleLeaderRuntime.cpp:106 |  |
| `-BattleRepairDepot` | Value | 0.f | BattleArtillery.cpp:100 |  |
| `-BattleRepairHoldFire` | Value | 0 | BattleArtillery.cpp:160 |  |
| `-BattleReplayArm` | Value | 空文字列 | BattleFieldReplay.cpp:60 |  |
| `-BattleReplayBatch` | Param | off | BattleFieldReplay.cpp:62 |  |
| `-BattleReplayFeature` | Value | 空文字列 | BattleFieldReplay.cpp:61 |  |
| `-BattleReplayReference` | Value | 空文字列 | BattleFieldReplay.cpp:59 |  |
| `-BattleReport` | Value | 空文字列 | BattleBenchmark.cpp:50 |  |
| `-BattleReturnFire` | Value | 0 | BattleTactics.cpp:292 |  |
| `-BattleReuseSightContext` | Value | True | BattleLeaderRuntime.cpp:110 |  |
| `-BattleRifleMembers` | Value | 未解決 | BattleState.cpp:600 |  |
| `-BattleRouteKeep` | Value | 0 | BattleRoutes.cpp:66 |  |
| `-BattleRouteLoad` | Value | 0 | BattleRoutes.cpp:63 |  |
| `-BattleRouteLog` | Value | 0 | BattleRoutes.cpp:68 |  |
| `-BattleRoutePassed` | Value | 1 | BattleRoutes.cpp:165 |  |
| `-BattleRoutes` | Value | -1 | BattleRoutes.cpp:20 |  |
| `-BattleRouteTieSymmetric` | Value | 0 | BattleRoutes.cpp:46 |  |
| `-BattleScaleScreenshot` | Value | 空文字列 | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |  |
| `-BattleScaleSmoke` | Param | off | BattleGameMode.cpp:147 |  |
| `-BattleScaleUnits` | Value | 0（BattleController.cpp:104） / 20（BattleGameMode.cpp:79） | BattleController.cpp:104, BattleGameMode.cpp:79 |  |
| `-BattleScopeShot` | Param | off | BattleLeaderRuntime.cpp:242 |  |
| `-BattleScoutRetreat` | Value | true | BattleScoutReport.cpp:23 |  |
| `-BattleSeconds` | Value | 20.f | BattleBenchmark.cpp:41 |  |
| `-BattleSectorAI` | Value | 0 | BattleMatch.cpp:531, BattleMatch.cpp:546, BattleMatch.cpp:574 |  |
| `-BattleSectorAudit` | Param | off | BattleSectorAudit.cpp:28 |  |
| `-BattleSectorContact` | Value | -1 | BattleLeaderRuntime.cpp:1866 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorCoverRoute` | Value | -1 | BattleSectorRoute.cpp:22 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorCoverWalk` | Value | 0 | BattleLeaderRuntime.cpp:2000 |  |
| `-BattleSectorFlank` | Value | -1 | BattleSectorRoute.cpp:124 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorFront` | Value | -1 | BattleSectorRoute.cpp:130 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorLaneLoad` | Value | 1500. | BattleSectorLanes.cpp:76 |  |
| `-BattleSectorLanes` | Value | 1 | BattleSectorLanes.cpp:17 |  |
| `-BattleSectorMove` | Value | 0 | BattleLeaderRuntime.cpp:1871 |  |
| `-BattleSectorMoveFire` | Value | -1 | BattleLeaderRuntime.cpp:1881 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorRoute` | Value | -1 | BattleSectorRoute.cpp:16 | -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision) |
| `-BattleSectorSight` | Value | 1 | BattleSectorSight.cpp:15 |  |
| `-BattleSectorSightHit` | Value | 1 | BattleSectorSight.cpp:22 |  |
| `-BattleSectorSightNear` | Value | 2000. | BattleSectorSight.cpp:29 |  |
| `-BattleSectorSightTeam` | Value | -1 | BattleSectorSight.cpp:16 |  |
| `-BattleSectorSlotReserve` | Value | 1 | BattleStableMovement.cpp:118 |  |
| `-BattleSectorSmoke` | Param | off | BattleLeaderRuntime.cpp:224 |  |
| `-BattleSectorSymmetric` | Value | 1 | BattleLeaderRuntime.cpp:2020 | default on since 2026-11-22 (user decision) |
| `-BattleSectorViewShot` | Value | 空文字列 | BattleHUD.cpp:1427 |  |
| `-BattleSeed` | Value | 7 | BattleBenchmark.cpp:42 |  |
| `-BattleSeekKnown` | Value | 0 | BattleTactics.cpp:74 |  |
| `-BattleSeekKnownMax` | Value | 2 | BattleTactics.cpp:89 |  |
| `-BattleSeekKnownRange` | Value | 20000.f | BattleTactics.cpp:84 |  |
| `-BattleSeekKnownSeconds` | Value | 30.f | BattleTactics.cpp:79 |  |
| `-BattleSenseMode` | Value | 空文字列 | BattleBenchmark.cpp:29 |  |
| `-BattleSetupPhase` | Value | 0 | BattleArea.cpp:363 |  |
| `-BattleSFX` | Value | 1 | BattleAudio.cpp:185 |  |
| `-BattleShotDirection` | Value | 0 | BattleLeaderRuntime.cpp:2101 |  |
| `-BattleShotLog` | Param | off | BattleLeaderRuntime.cpp:1507 |  |
| `-BattleSidearms` | Value | 1 | BattleArms.cpp:40 | IntSwitch → BattleArms.cpp:34（ヘルパー経由） |
| `-BattleSightExperiment` | Value | 未解決 | BattleLeaderRuntime.cpp:86 |  |
| `-BattleSightHitStats` | Value | 0 | BattleLeaderRuntime.cpp:409 |  |
| `-BattleSightPairReuse` | Value | 0 | BattleLeaderRuntime.cpp:367, BattlePerception.cpp:241 |  |
| `-BattleSightParallel` | Value | 1 | BattlePerception.cpp:240 |  |
| `-BattleSightRange` | Value | 空文字列 | BattleState.cpp:19 |  |
| `-BattleSightRayStats` | Value | 0 | BattlePerception.cpp:249 |  |
| `-BattleSightScanInterval` | Value | .4 | BattleLeaderRuntime.cpp:121 |  |
| `-BattleSightScanNear` | Value | 1.e7 | BattleLeaderRuntime.cpp:121 |  |
| `-BattleSightSlowFar` | Value | 0 | BattleLeaderRuntime.cpp:118 |  |
| `-BattleSightTeamMask` | Value | 1 | BattleLeaderRuntime.cpp:414 | default 1 since 2026-11-22 (user decision); =0 = own soldiers block their side's sight and bullets again |
| `-BattleSightTrial` | Param | off | BattleLeaderRuntime.cpp:250 |  |
| `-BattleSightVisual` | Param | off | BattleLeaderRuntime.cpp:244 |  |
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
| `-BattleSPGAmmo` | Value | 1 | BattleArtillery.cpp:266 |  |
| `-BattleSPGRange` | Strifind/Value | 100000.f | BattleAreaSmoke.cpp:496, BattleArtillery.cpp:250 |  |
| `-BattleSPGSetup` | Value | -1.f | BattleArtillery.cpp:138 |  |
| `-BattleSPGStayAfterFire` | Value | 0.f | BattleArtillery.cpp:143 |  |
| `-BattleSplitDecisions` | Param | off | BattleBenchmark.cpp:35 |  |
| `-BattleSpotTrack` | Value | 0.f | BattleTactics.cpp:69 |  |
| `-BattleSquadCoverCheck` | Value | 0 | BattleLeaderRuntime.cpp:1886 |  |
| `-BattleSquadFlank` | Value | 1 | BattleSquadTactics.cpp:22 |  |
| `-BattleSquadIcons` | Value | 1 | BattleHUD.cpp:259, BattleHUD.cpp:1123 |  |
| `-BattleSquadKnowledge` | Value | 1 | BattlePerception.cpp:65 | default 1 since 2026-11-26 (user decision); =0: the soldier-to-soldier reports |
| `-BattleSquadKnowledgeCover` | Value | 0 | BattleCoverAction.cpp:505 |  |
| `-BattleSquadKnowledgeTeam` | Value | -1 | BattlePerception.cpp:66 |  |
| `-BattleSquadST2` | Value | 1 | BattleSquadTactics.cpp:21 |  |
| `-BattleSquadTactics` | Value | 1 | BattleSquadTactics.cpp:17 |  |
| `-BattleSquadTrial` | Value | 空文字列 | BattleGameMode.cpp:64, BattleUnit.cpp:107 |  |
| `-BattleST2Smoke` | Param | off | BattleLeaderRuntime.cpp:264 |  |
| `-BattleStableLoad` | Param | off | BattleBenchmark.cpp:49 |  |
| `-BattleStableMovement` | Value | 1 | BattleLeaderRuntime.cpp:91 |  |
| `-BattleStanceAnim` | Value | 1 | BattleStance.cpp:42 |  |
| `-BattleStanceMove` | Value | 1 | BattleStance.cpp:62 |  |
| `-BattleStances` | Value | -1 | BattleStance.cpp:27 |  |
| `-BattleStanceShot` | Param | off | BattleLeaderRuntime.cpp:240 |  |
| `-BattleStanceSteady` | Value | 1 | BattleStance.cpp:67 |  |
| `-BattleStartRunning` | Param | off | BattleLeaderRuntime.cpp:215 |  |
| `-BattleStartSquads` | Value | 25 | BattleArea.cpp:365 |  |
| `-BattleStateTrace` | Value | 0 | BattleStateTrace.cpp:24 |  |
| `-BattleStateTraceDetail` | Value | 空文字列 | BattleStateTrace.cpp:26 |  |
| `-BattleSTObserveSeconds` | Value | 8 | BattleSquadTacticsSmoke.cpp:137 |  |
| `-BattleStrategyFocusHold` | Value | 90.f | BattleStrategy.cpp:259 | Seconds → BattleStrategy.cpp:246（ヘルパー経由） |
| `-BattleStrategyOpen` | Value | 150.f | BattleStrategy.cpp:253 | Seconds → BattleStrategy.cpp:246（ヘルパー経由） |
| `-BattleSTScreens` | Value | 空文字列 | BattleSquadTacticsSmoke.cpp:138 |  |
| `-BattleSupplyAmmoShare` | Value | 0.f | BattleTactics.cpp:146 |  |
| `-BattleSupplyAwayRatio` | Value | 1.0 | BattleTactics.cpp:164 |  |
| `-BattleSupplyCarFirst` | Value | 0 | BattleTactics.cpp:49 |  |
| `-BattleSupplyCarHunt` | Value | 0 | BattleTactics.cpp:44 |  |
| `-BattleSupplyCarPost` | Value | 0 | BattleTactics.cpp:54 |  |
| `-BattleSupplyCarRecover` | Value | 0 | BattleTactics.cpp:64 |  |
| `-BattleSupplyCarTarget` | Value | 0 | BattleTactics.cpp:59 |  |
| `-BattleSupplyHeal` | Value | 10.0 | BattleTactics.cpp:164 |  |
| `-BattleSupplyHuntLog` | Param | off | BattleATHunt.cpp:161 |  |
| `-BattleSupplyResumeHealth` | Value | 0.9 | BattleTactics.cpp:165 |  |
| `-BattleSupplyResumeLiving` | Value | 10 | BattleTactics.cpp:163 |  |
| `-BattleSupplyReturnLiving` | Value | 8 | BattleTactics.cpp:163 |  |
| `-BattleSupportSmoke` | Param | off | BattleLeaderRuntime.cpp:262 |  |
| `-BattleSuppressFire` | Value | 0.f | BattleTactics.cpp:39 |  |
| `-BattleSuppression` | Value | 0 | BattleSuppression.cpp:13 |  |
| `-BattleSuppressionDecay` | Value | .25f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionPerShot` | Value | .12f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionPin` | Value | .5f | BattleSuppression.cpp:16 |  |
| `-BattleSuppressionRadius` | Value | 200.f | BattleSuppression.cpp:14 |  |
| `-BattleSuppressionSpread` | Value | 1.5f | BattleSuppression.cpp:15 |  |
| `-BattleSuppressionSupport` | Value | 1.f | BattleSuppression.cpp:17 |  |
| `-BattleSurpriseGroup` | Value | 0 | BattleFrontLine.cpp:934 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleSurpriseSquads` | Value | 2 | BattleFrontLine.cpp:956 | Arg → BattleFrontLine.cpp:31（ヘルパー経由） |
| `-BattleSymmetricStart` | Value | -1 | BattleGroups.cpp:59 |  |
| `-BattleSymmetryAudit` | Param | off | BattleCover.cpp:104 |  |
| `-BattleTacticalObserve` | Param | off | BattleSquadTactics.cpp:26 |  |
| `-BattleTankGunFlash` | Strifind/Value | 0 | BattleAreaSmoke.cpp:493, BattleArtillery.cpp:51 |  |
| `-BattleTeamSupport` | Value | 1 | BattleTeamAI.cpp:13 |  |
| `-BattleTerrainLineBodies` | Value | 1 | BattleTactics.cpp:367 |  |
| `-BattleTerrainShot` | Param | off | BattleTerrainTest.cpp:44 |  |
| `-BattleThinkCap` | Value | 0 | BattleLeaderRuntime.cpp:1782 |  |
| `-BattleThinkSymmetric` | Value | 0 | BattleGroups.cpp:86 |  |
| `-BattleThreatCover` | Value | 0 | BattleThreatCover.cpp:23 |  |
| `-BattleThreatCoverCrouch` | Value | 0 | BattleThreatCover.cpp:67 |  |
| `-BattleThreatCoverCrouchTeam` | Value | -1 | BattleThreatCover.cpp:68 |  |
| `-BattleThreatCoverRelocate` | Value | 0 | BattleThreatCover.cpp:30 |  |
| `-BattleThreatCoverTeam` | Value | -1 | BattleThreatCover.cpp:24 |  |
| `-BattleTracerShot` | Param | off | BattleStanceShot.cpp:87 |  |
| `-BattleTrialSeed` | Value | 0 | BattleTactics.cpp:343 |  |
| `-BattleTrialSwap` | Param | off | BattleArea.cpp:18, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:88, BattleMatch.cpp:27, BattleMatch.cpp:293, BattleMatch.cpp:312, BattleWideMapAudit.cpp:116 |  |
| `-BattleUnderFireCover` | Value | 0 | BattleLeaderRuntime.cpp:1995 |  |
| `-BattleUnits` | Value | 20（BattleBenchmark.cpp:36） / 1000（BattleSquadTrial.cpp:28） | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |  |
| `-BattleVehicleHitCover` | Strifind/Value | 0 | BattleAreaSmoke.cpp:492, BattleArtillery.cpp:45 |  |
| `-BattleVehicleOrderHere` | Value | 0 | BattleArtillery.cpp:39 |  |
| `-BattleVehicleRoadGoal` | Value | 0 | BattleLeaderRuntime.cpp:602 |  |
| `-BattleWalkLook` | Value | 0 | BattleSoldierWatch.cpp:24 |  |
| `-BattleWalkLookTeam` | Value | -1 | BattleSoldierWatch.cpp:25 |  |
| `-BattleWaypointCrowd` | Value | 0 | BattleStableMovement.cpp:621 |  |
| `-BattleWaypointPass` | Value | 1 | BattleStableMovement.cpp:607 |  |
| `-BattleWeaponLoadouts` | Param | off | BattleSquadTrial.cpp:50 |  |
| `-BattleWeaponOrderGate` | Value | 0 | BattleWeaponOrders.cpp:10 |  |
| `-BattleWeaponPreview` | Value | 空文字列 | BattleWeaponPreview.cpp:19 |  |
| `-BattleWeaponRange` | Value | 空文字列 | BattleState.cpp:44 |  |
| `-BattleWeaponTrial` | Param | off | BattleLeaderRuntime.cpp:234 |  |
| `-BattleWideMapAudit` | Param | off | BattleLeaderRuntime.cpp:248 |  |
| `-BattleWideMapGeometryOnly` | Param | off | BattleWideMapAudit.cpp:157 |  |
| `-BattleWideMixed` | Param | off | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:82 |  |
| `-BattleWideScreens` | Value | 空文字列（BattleAreaSmoke.cpp:1077） / 空文字列（BattleContactTrial.cpp:97） / 空文字列（BattleDeathShot.cpp:42） / 空文字列（BattleExoShot.cpp:107） / 未解決（BattleFXCheck.cpp:197） / 空文字列（BattleMixedFastSmoke.cpp:29） / 空文字列（BattleMovementTrial.cpp:108） / 空文字列（BattleScopeShot.cpp:45） / 空文字列（BattleSightVisual.cpp:30） / 空文字列（BattleSquadFlankSmoke.cpp:165） / 空文字列（BattleSquadRetreatSmoke.cpp:22） / 空文字列（BattleStanceShot.cpp:96） / 空文字列（BattleSupplyCar.cpp:46） / 空文字列（BattleTeamSupportSmoke.cpp:122） / 空文字列（BattleTerrainTest.cpp:90） / 空文字列（BattleWideMapAudit.cpp:40） / 空文字列（BattleWideMapAudit.cpp:159） / 空文字列（BattleWideMapAudit.cpp:190） / 空文字列（BattleWideMixedSmoke.cpp:39） / 空文字列（BattleWideMixedSmoke.cpp:53） | BattleAreaSmoke.cpp:1077, BattleContactTrial.cpp:97, BattleDeathShot.cpp:42, BattleExoShot.cpp:107, BattleFXCheck.cpp:197, BattleMixedFastSmoke.cpp:29, BattleMovementTrial.cpp:108, BattleScopeShot.cpp:45, BattleSightVisual.cpp:30, BattleSquadFlankSmoke.cpp:165, BattleSquadRetreatSmoke.cpp:22, BattleStanceShot.cpp:96, BattleSupplyCar.cpp:46, BattleTeamSupportSmoke.cpp:122, BattleTerrainTest.cpp:90, BattleWideMapAudit.cpp:40, BattleWideMapAudit.cpp:159, BattleWideMapAudit.cpp:190, BattleWideMixedSmoke.cpp:39, BattleWideMixedSmoke.cpp:53 |  |
| `-BattleWideSmoke` | Param | off | BattleLeaderRuntime.cpp:266 |  |
| `-BattleWideSoak` | Param | off | BattleLeaderRuntime.cpp:268 |  |
| `-BayFlank` | Value | 0 | BattleFlankBayTrial.cpp:17 |  |
| `-BayOutput` | Value | 空文字列 | BattleFlankBayTrial.cpp:62 |  |
| `-BayRendered` | Param | off | BattleFlankBayTrial.cpp:66 |  |
| `-BaySign` | Value | 1 | BattleFlankBayTrial.cpp:17 |  |
| `-CoverScreenshot` | Value | 空文字列 | BattleCoverActionSmoke.cpp:54, BattleCoverActionSmoke.cpp:261 |  |
| `-noraytracing` | Param | off | BattleRenderDiagnostic.cpp:41 |  |
| `-nullrhi` | Param | off | BattleBenchmark.cpp:162, BattleBenchmark.cpp:187, BattleController.cpp:1863, BattleNetworkBenchmark.cpp:85, BattleNetworkBenchmark.cpp:116, BattlePhase3.cpp:176 |  |
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
| `-P3VATAudit` | Param | off | BattleLeaderRuntime.cpp:132, BattleVATDisplay.cpp:161 |  |
| `-P3View` | Value | "RTS" | BattlePhase3.cpp:64 |  |
| `-P3Warmup` | Value | 10 | BattlePhase3.cpp:65 |  |
| `-PktLag` | Value | 0 | BattleNetworkBenchmark.cpp:80 |  |
| `-PktLoss` | Value | 0 | BattleNetworkBenchmark.cpp:80 |  |
| `-ResX` | Value | 1920（BattleLeaderSmoke.cpp:103） / 1280（BattleWideMixedSmoke.cpp:167） | BattleLeaderSmoke.cpp:103, BattleWideMixedSmoke.cpp:167 |  |
| `-ResY` | Value | 1080（BattleLeaderSmoke.cpp:103） / 720（BattleWideMixedSmoke.cpp:167） | BattleLeaderSmoke.cpp:103, BattleWideMixedSmoke.cpp:167 |  |
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
