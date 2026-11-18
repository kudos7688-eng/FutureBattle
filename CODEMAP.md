# コード索引（どこに何があるか）

`Setup/Export-CodeMap.py` が生成する。**手で編集しない**。機能の表とモジュールの説明は `Setup/CodeMap-Features.md` に書き、作り直す。
実装を探すときは、まずこの表で入口を決め、関数の行は [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) を Grep で引く（全文は読まない）。
行番号は生成時点のもの。ずれていたら作り直す。起動引数は [SWITCHES.md](SWITCHES.md)。

対象は `Game/FutureBattle/Source/FutureBattle/Prototype/`（132 モジュール、27110 行）。同じ階層の `Variant_*` と `FutureBattle*.cpp/.h` は UE テンプレート由来で、ゲームのルールは置いていない。

## 毎フレームの流れ（どこから呼ばれるか）

入口は `ABattleGameMode::Tick` (BattleGameMode.cpp:169) 。ここで2つの経路に分かれる。

- **混成（隊長Actor＋Mass部下。広域・エリア戦の通常経路）**：`UBattleLeaderRuntime::Step` (BattleLeaderRuntime.cpp:1016) がすべてを回す。順に、試合の進行 `UBattleMatch::Update` (BattleMatch.cpp:151) → 部隊の判断 `UBattleSquadTactics::Step` (BattleSquadTactics.cpp:97) → 認識 `UBattlePerception::Update` (BattlePerception.cpp:57) → 隊長の判断 `UBattleLeaderRuntime::Think` (BattleLeaderRuntime.cpp:524) → 兵ごとの移動・射撃 `UBattleLeaderRuntime::Follow` (BattleLeaderRuntime.cpp:693) → 表示 `UBattleLeaderRuntime::RefreshDisplay` (BattleLeaderRuntime.cpp:999) 。
- **旧全Actor（80m×60m の試作アリーナ）**：GameMode が直接 `UBattleMatch::Update` (BattleMatch.cpp:151) 、 `UBattleSquadTactics::Step` (BattleSquadTactics.cpp:97) 、 `ABattleGameMode::RefreshVisibility` (BattleGameMode.cpp:209) 、 `UBattleSquadAI::Update` (BattleSquadAI.cpp:32) を呼ぶ。兵ごとの実行は `ABattleAIController::Tick` (BattleAIController.cpp:152) 。

エリア戦のときは `UBattleMatch::Update` (BattleMatch.cpp:151) の中で `UBattleMatch::UpdateArea` (BattleArea.cpp:189) を毎フレーム、 `UBattleMatch::UpdateAreaHiring` (BattleArea.cpp:120) と `UBattleMatch::UpdateEnemyOrders` (BattleMatch.cpp:262) を5秒ごとに呼ぶ。起動時の組み立て（どのモードか、どの試験か）は `ABattleGameMode::BeginPlay` (BattleGameMode.cpp:51) と `UBattleLeaderRuntime::Start` (BattleLeaderRuntime.cpp:65) 。

`UBattleLeaderRuntime` と `UBattleMatch` と `UBattleSquadTactics` と `UBattleSquadTrial` は、1つのクラスを複数の .cpp に分けて書いている。クラス名でファイルを決めつけず、下の表か [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) で引く。

## 機能 → 入口

| 機能 | 入口（ここから読む） | 関連 |
|---|---|---|
| 正規状態・UnitID・部隊 | `UBattleState::RegisterUnit` (BattleState.cpp:87) 、 `UBattleState::CreateSquad` (BattleState.cpp:156) 、 `UBattleState::ApplyDamage` (BattleState.cpp:408) | 構造体は `BattleState.h`（FBattleUnitState、FBattleSquadState）。調整値は同じ .h の `UBattleTuning`（ini で上書き） |
| 兵科（歩兵・重火器・偵察・対戦車・戦車）の差 | `BattleSquadType::Members` (BattleState.cpp:506) から `BattleSquadType::Weapon` (BattleState.cpp:544) までの一群、 `UBattleState::SetSquadType` (BattleState.cpp:549) | 射程は `BattleRanges::Get` (BattleRanges.cpp:6) |
| 命令（移動・攻撃移動・防御・待機） | 入力 `ABattleController::IssueOrderAt` (BattleController.cpp:608) → 状態 `UBattleState::SetSquadOrder` (BattleState.cpp:187) → 混成 `UBattleLeaderRuntime::IssueOrder` (BattleLeaderRuntime.cpp:472) | 委任は `UBattleState::SetSquadDelegated` (BattleState.cpp:174) 、 `UBattleMatch::SetAllyDelegated` (BattleMatch.cpp:143) |
| 入力・RTS/TPS 切替・ゲームパッド | `ABattleController::SetupInputComponent` (BattleController.cpp:58) 、 `ABattleController::InputKey` (BattleController.cpp:397) 、 `ABattleController::ToggleMode` (BattleController.cpp:299) 、 `ABattleController::ControlUnit` (BattleController.cpp:272) | ゲームパッドは `ABattleController::UpdateGamepad` (BattleController.cpp:1054) 。隊長交代は `UBattleState::HandOverLeader` (BattleState.cpp:474) |
| HUD・ミニマップ・部隊一覧・通知 | `ABattleHUD::DrawHUD` (BattleHUD.cpp:19) 、 `ABattleHUD::DrawMinimap` (BattleHUD.cpp:413) 、 `ABattleHUD::DrawSquadList` (BattleHUD.cpp:504) 、 `ABattleHUD::DrawNoticeLog` (BattleHUD.cpp:569) | 通知の中身は `ABattleController::UpdateNoticeLog` (BattleController.cpp:862) 、 `ABattleController::PushNotice` (BattleController.cpp:846) 。日本語化は `BattleUIText::Ja` (BattleUIText.cpp:75) |
| 認識（見えた敵の記憶） | `UBattlePerception::Update` (BattlePerception.cpp:57) 、 `UBattlePerception::GetContacts` (BattlePerception.cpp:36) | 視線は `UBattleLeaderRuntime::CanSee` (BattleLeaderRuntime.cpp:292) 、 `UBattleLeaderRuntime::TraceSight` (BattleLeaderRuntime.cpp:323) 。試験用の視界切替は `BattleSightExperiment` |
| 隊長の判断と兵の実行（混成） | `UBattleLeaderRuntime::Think` (BattleLeaderRuntime.cpp:524) 、 `UBattleLeaderRuntime::Follow` (BattleLeaderRuntime.cpp:693) 、 `UBattleLeaderRuntime::Fire` (BattleLeaderRuntime.cpp:860) | 兵単位の実行は `FBattleMixedDriver::Tick` (BattleMixedDriver.cpp:126) 、 `FBattleMixedDriver::TryTacticalAction` (BattleMixedDriver.cpp:239) |
| 移動・経路・混雑 | `UBattleLeaderRuntime::StableMove` (BattleStableMovement.cpp:227) 、 `UBattleLeaderRuntime::BuildMovementPlan` (BattleStableMovement.cpp:56) | 到着地点の重なりは `UBattleLeaderRuntime::ReassignOccupiedArrival` (BattleContactMovement.cpp:16) 。目的地の補正は `BattleNavigation::ResolveGoal` (BattleNavigation.cpp:7) |
| 遮蔽（場所の予約） | `UBattleCover::BuildSlots` (BattleCover.cpp:14) 、 `UBattleCover::Reserve` (BattleCover.cpp:187) | 露出の計算は `BattleTactics::ExposureAt` (BattleTactics.cpp:236) |
| 遮蔽の動作（接近・のぞき撃ち） | `UBattleLeaderRuntime::RequestCoverAction` (BattleCoverAction.cpp:57) 、 `UBattleLeaderRuntime::StepCoverAction` (BattleCoverAction.cpp:285) | 切替は `BattleTactics::CoverActionsEnabled` (BattleTactics.cpp:185) ほか |
| 姿勢（立ち・しゃがみ） | `UBattleLeaderRuntime::SetStance` (BattleStance.cpp:50) 、 `UBattleLeaderRuntime::UpdateAIStance` (BattleStance.cpp:84) | 数値は `BattleStance::EyeOffset` (BattleStance.cpp:24) ほか |
| 制圧（撃たれて動けない） | `UBattleLeaderRuntime::AddSuppression` (BattleSuppression.cpp:45) 、 `UBattleLeaderRuntime::IsPinned` (BattleSuppression.cpp:33) | 調整値は `BattleSuppression::Tuning` (BattleSuppression.cpp:25) |
| 射撃の散らばり | `BattleTactics::SpreadDegrees` (BattleTactics.cpp:191) 、 `BattleTactics::ShotDirection` (BattleTactics.cpp:199) | 照準の遅れは `UBattleState::UpdateAim` (BattleState.cpp:307) |
| 部隊の戦術（撤退・集合・援護・迂回） | `UBattleSquadTactics::Step` (BattleSquadTactics.cpp:97) 、 `UBattleSquadTactics::Evaluate` (BattleSquadRetreat.cpp:79) 、 `UBattleSquadTactics::Apply` (BattleSquadRetreat.cpp:10) | 撤退 `UBattleSquadTactics::RetreatGoal` (BattleSquadRetreat.cpp:26) 、援護 `UBattleSquadTactics::AcceptSupport` (BattleSquadSupport.cpp:71) 、迂回 `UBattleSquadTactics::TryFlank` (BattleSquadFlank.cpp:45) |
| 陣営全体の判断（援護・迂回の割り振り） | `UBattleTeamAI::Step` (BattleTeamAI.cpp:15) | |
| 試合の進行・勝敗・制限時間 | `UBattleMatch::InitializeMatch` (BattleMatch.cpp:15) 、 `UBattleMatch::Update` (BattleMatch.cpp:151) 、 `UBattleMatch::Finish` (BattleMatch.cpp:480) | 拠点の占領は `UBattleMatch::UpdateObjectives` (BattleMatch.cpp:203) |
| 雇用（旧・固定の価格） | `UBattleMatch::HireSquad` (BattleMatch.cpp:56) 、 `UBattleMatch::CurrentSquadPrice` (BattleMatch.cpp:51) | 実体を出すのは `UBattleLeaderRuntime::SpawnPurchasedSquad` (BattleMixedRecruitment.cpp:40) |
| エリア戦の雇用（AI が何を雇うか） | `UBattleMatch::UpdateAreaHiring` (BattleArea.cpp:120) 、 `UBattleMatch::AreaHireNeed` (BattleArea.cpp:266) 、 `UBattleMatch::HireAreaSquad` (BattleArea.cpp:81) | 調整値は `BattleTactics::AreaHire` (BattleTactics.cpp:46) |
| エリア戦の収入・拠点 HP | `UBattleMatch::UpdateArea` (BattleArea.cpp:189) （収入の支払いもここ）、 `UBattleMatch::DamageBase` (BattleArea.cpp:62) | 調整値は `BattleTactics::AreaIncome` (BattleTactics.cpp:109) 。状態は `BattleAreaState.h` |
| エリア戦の AI 戦略（どこを攻めるか） | `UBattleMatch::UpdateEnemyOrders` (BattleMatch.cpp:262) → `UBattleMatch::UpdateTeamOrders` (BattleMatch.cpp:347) 、 `UBattleMatch::AreaStrategyTarget` (BattleMatch.cpp:280) 、 `UBattleMatch::AreaIntent` (BattleArea.cpp:294) | 調整値は `BattleTactics::AreaStrategy` (BattleTactics.cpp:63) 。前線は `UBattleMatch::AreaFronts` (BattleArea.cpp:343) |
| 偵察・情報（見えていない区域の扱い） | `UBattleMatch::UpdateScoutReports` (BattleScoutReport.cpp:45) 、 `UBattleMatch::AreaIntelView` (BattleAreaIntel.cpp:19) | 偵察兵の配置は `UBattleMatch::AreaScoutPost` (BattleArea.cpp:383) 。昇格は `UBattleLeaderRuntime::PromoteScout` (BattleMixedRecruitment.cpp:114) |
| 補給（弾薬） | `UBattleMatch::Resupply` (BattleMatch.cpp:461) 、 `BattleTactics::Supply` (BattleTactics.cpp:26) | 往復時間の記録（評価用）は `UBattleSquadTactics::SupplyTripEnd` (BattleSquadTactics.cpp:71) |
| 補充（倒れた部下の補充） | `UBattleLeaderRuntime::ReplaceFollower` (BattleMixedRecruitment.cpp:12) 、 `UBattleState::ReplaceDeadFollower` (BattleState.cpp:102) | |
| 戦果点 | `UBattleMatch::UpdateWarScore` (BattleWarScore.cpp:52) 、 `UBattleMatch::WarScoreBreakdown` (BattleWarScore.cpp:75) | 調整値は `BattleWarScore::Tuning` (BattleWarScore.cpp:19) |
| 支援砲撃（旧アリーナ） | `UBattleMatch::RequestSupport` (BattleSupport.cpp:8) 、 `UBattleMatch::UpdateSupport` (BattleSupport.cpp:36) | 混成には未接続 |
| 車両・戦車砲・対戦車 | `UBattleLeaderRuntime::ConfigureVehicle` (BattleVehicle.cpp:50) 、 `UBattleLeaderRuntime::SteerVehicle` (BattleVehicle.cpp:107) 、 `UBattleLeaderRuntime::FireTankGun` (BattleTankGun.cpp:114) | 爆発は `UBattleLeaderRuntime::Explode` (BattleAntiTank.cpp:6) |
| 保存・復元 | `UBattleMatch::SaveMatch` (BattleSave.cpp:245) 、 `UBattleMatch::LoadMatch` (BattleSave.cpp:253) 、 `UBattleMatch::CaptureSnapshot` (BattleSave.cpp:113) 、 `UBattleMatch::RestoreSnapshot` (BattleSave.cpp:161) | 検証は `UBattleSave::Validate` (BattleSave.cpp:23) 、 `UBattleSave::ValidateMixed` (BattleMixedSaveValidation.cpp:7) 。戦術状態は `UBattleSquadTactics::CaptureSave` (BattleTacticsSave.cpp:4) |
| 表示（混成の体・遠距離 VAT） | `UBattleLeaderRuntime::CreateBody` (BattleMixedBodies.cpp:17) 、 `UBattleLeaderRuntime::RefreshDisplay` (BattleLeaderRuntime.cpp:999) 、 `ABattleVATDisplay::Tick` (BattleVATDisplay.cpp:294) | 復元時は `UBattleLeaderRuntime::RestoreMixedBodies` (BattleMixedBodies.cpp:67) |
| マップの定義・配置 | `BattleFieldLayout.h`（マップごとの表 `Maps()`） | 生成は `UBattleMapCommandlet::Main` (BattleMapCommandlet.cpp:35) （既存の手編集を `-Force` で上書きしない） |
| 通り道（道路網に沿った移動の指示） | `UBattleMatch::RouteStep` (BattleRoutes.cpp:43) 、 `UBattleMatch::RouteFlankSide` (BattleRoutes.cpp:33) | 呼び出しは `UBattleMatch::UpdateTeamOrders` (BattleMatch.cpp:347) 。道路網は `FMapDef::RouteNodes/RouteEdges` |
| 高低差（丘・階段・2階。マップG） | `UBattleLeaderRuntime::TerrainStep` (BattleStableMovement.cpp:30) 、 `UBattleMatch::UpdateTerrainTest` (BattleTerrainTest.cpp:21) | 高さの表は `BattleTerrain.h`（`FHeights::Ground`、家と丘の位置 `HousesG/HillsG`）と `FMapDef::Heights`。形は `BattleMapLayoutG.inl`。隊長はエンジンの歩行、多数兵は表に沿う。自己試験 `Test-WideSight.ps1 -Map G -HeightTest` |
| マップの数値監査（試合なし） | `UBattleLeaderRuntime::RunMapStats` (BattleMapStats.cpp:38) | `Setup/Test-MapStats.ps1`。拠点ごとの持ち主・価値は `FMapDef::ObjectiveOwners/ObjectiveValues` |
| 計測の記録 | `UBattleMetrics::BeginFrame` (BattleMetrics.cpp:21) 、 `UBattleMetrics::Finish` (BattleMetrics.cpp:75) | 段取りは MEASURE.md |
| ネットワーク試作 | `ABattleNetGameMode::BeginPlay` (BattleNetwork.cpp:36) 、 `ABattleNetController::ServerOrder_Implementation` (BattleNetwork.cpp:177) | |
| 基本回帰（BATTLE_SMOKE_RESULT） | `ABattleGameMode::RunSmokeTest` (BattleGameMode.cpp:229) 、 `ABattleGameMode::FinishSmokeTest` (BattleGameMode.cpp:331) | 個別の試験は各 `*Smoke.cpp` の `Run…` |


## モジュール一覧（本体）

行数は .h + .cpp。型は宣言順で先頭8個まで。

| モジュール | 種類 | 行数 | 宣言している型 | 役割 |
|---|---|---|---|---|
| `BattleAIController` | .cpp | 249 |  | 旧全Actor経路の兵の実行（移動・再試行）。混成では使わない |
| `BattleAnimTasks` | .h/.cpp | 11 + 27 | FBattleAnimTasks | アニメーション更新の並列実行の設定を兵ごとに切り替え、元の設定を覚えておく |
| `BattleAntiTank` | .cpp | 35 |  | 対戦車弾の爆発（UBattleLeaderRuntime の一部） |
| `BattleArea` | .cpp | 413 |  | エリア戦の本体：初期化、収入、拠点、AI の雇用と意図（UBattleMatch の一部） |
| `BattleAreaDepot` | .cpp | 49 |  | 集積所の重み：価値が1より大きい拠点（マップFの集積所）への割り振りの重みを、守る・取り返す・襲うの状況で変える（UBattleMatch の一部） |
| `BattleAreaIntel` | .cpp | 81 |  | エリア戦の情報：観測に基づく区域の見え方と脅威（UBattleMatch の一部） |
| `BattleAreaState` | .h | 35 | FBattleAreaState | エリア戦の保存される状態（拠点 HP、収入、観測） |
| `BattleArmourAI` | .cpp | 239 |  | Armour AI, stage 3 (2026-11-18, user request: improve the AI that handles tanks, armoured cars and the other new squad types at the |
| `BattleArtillery` | .cpp | 284 |  | Indirect fire, stage 1 (ROADMAP 2026-11-14 heavy equipment order: ... armoured car -> indirect fire; first values 2026-11-18, numbers |
| `BattleContactMovement` | .cpp | 130 |  | 停止中の味方との接触・到着地点の重なりの解消（UBattleLeaderRuntime の一部） |
| `BattleController` | .h/.cpp | 200 + 1122 | ABattleCameraPawn, FBattleNoticeEntry, FBattleNoticeLayout, FBattleCommandBarLayout, ABattleController | プレイヤーの入力、RTS/TPS 切替、選択、命令、通知、ゲームパッド |
| `BattleCover` | .h/.cpp | 39 + 200 | FBattleCoverReservation, UBattleCover | 遮蔽の場所の一覧と予約 |
| `BattleCoverAction` | .cpp | 381 |  | 遮蔽への接近とのぞき撃ちの動作（UBattleLeaderRuntime の一部） |
| `BattleExo` | .cpp | 198 |  | Armoured exoskeleton soldier (squad type 8, 2026-11-18): one armoured, shielded, fast soldier with two weapons. |
| `BattleFieldLayout` | .h | 182 | BattleFieldLayout, FMapDef | マップごとの定義（広さ、拠点、試合時間）を1つの表にまとめたもの |
| `BattleFieldReplay` | .cpp | 206 |  | 広域試験の記録と再生（UBattleSquadTrial の一部） |
| `BattleFieldTurn` | .cpp | 168 |  | 広域試験 T2b（UBattleSquadTrial の一部） |
| `BattleFollowerBones` | .h/.cpp | 10 + 39 | BattleFollowerBones | 霧で隠れた部下のボーン表示の切替 |
| `BattleGameMode` | .h/.cpp | 97 + 363 | ABattleGameMode | モードの統合、起動時の組み立て、毎フレームの入口、基本回帰 |
| `BattleGroups` | .cpp | 447 |  | Squad groups, stage 1 (2026-11-18, user proposal: several squads act together as one group; ROADMAP 2026-11-18 "squad group layer"). |
| `BattleHUD` | .h/.cpp | 23 + 619 | ABattleHUD | 画面表示（HUD、ミニマップ、部隊一覧、通知、命令の印） |
| `BattleLeaderRuntime` | .h/.cpp | 430 + 1114 | FBattleFollowerFragment, FBattleSquadSummary, FBattleSquadListLayout, FBattleMinimapLayout, FBattleLeaderBody, FBattleLeaderPlan, UBattleLeaderRuntime | 混成の中心：起動、視線、隊長の判断、兵の移動と射撃、表示の更新 |
| `BattleMapStats` | .cpp | 186 |  | マップの統計（区域ごとの見通し・遮蔽、経路の長さ）を JSON に書き出す。試合は行わない（UBattleLeaderRuntime の一部） |
| `BattleMatch` | .h/.cpp | 267 + 490 | FAreaIntent, FBattleObjective, FBattleStrike, FPlayerRoute, UBattleMatch | 試合の進行、雇用、拠点の占領、陣営の命令、補給、勝敗 |
| `BattleMetrics` | .h/.cpp | 65 + 142 | FBattleMetricFrame, UBattleMetrics, FBattleScopedMetric | 計測値の記録（1フレームの時間、判断の回数） |
| `BattleMixedBodies` | .cpp | 172 |  | 混成の兵の体の生成・破棄・復元（UBattleLeaderRuntime の一部） |
| `BattleMixedDriver` | .h/.cpp | 50 + 339 | FBattleMixedUnit, FBattleMixedDriver, MixedNavigation | 混成の兵1人ぶんの実行（旧 AIController と同じ役を Actor なしで行う） |
| `BattleMixedRecruitment` | .cpp | 128 |  | 補充と購入部隊の出現、偵察兵の昇格（UBattleLeaderRuntime の一部） |
| `BattleMixedSquadAI` | .cpp | 165 |  | 部隊 AI の混成版の更新（UBattleSquadAI の一部） |
| `BattleMoveCourse` | .h/.cpp | 43 + 159 | UBattleMoveCourse, FRunner | 移動の計測コース |
| `BattleMuzzleFlash` | .h/.cpp | 10 + 92 | BattleMuzzleFlash | 銃口の閃光（表示だけ。発光する楕円体の使い回し） |
| `BattleNavigation` | .h/.cpp | 11 + 44 | BattleNavigation | 目的地の補正と隊形の位置 |
| `BattleNetwork` | .h/.cpp | 148 + 337 | FBattleNetUnit, FBattleNetFrame, ABattleNetController, ABattleNetGameMode, ABattleNetHUD | ネットワーク試作（GameMode、Controller、HUD） |
| `BattleNetworkSerialization` | .cpp | 31 |  | ネットワーク用の兵の直列化 |
| `BattlePerception` | .h/.cpp | 69 + 395 | FBattleContact, FBattleObserver, FBattleReport, UBattlePerception | 認識：見えた敵の記憶、保存と復元 |
| `BattlePhase3` | .h/.cpp | 90 + 424 | UBattleMixedAvoidance, UBattlePhase3, FPhase3Scope | Phase3 の大規模試験経路（通常の混成とは別の処理） |
| `BattlePlayerRoutes` | .cpp | 185 |  | 通り道のプレイヤー指定：部隊ごとに 自動・中央・北・南 を選び、手動の遠い命令と委任中の移動をその道に通す。自動試験 `-BattlePlayerRouteTest`（UBattleMatch の一部） |
| `BattleRanges` | .h/.cpp | 19 + 50 | BattleRanges | 認識・交戦の距離（マップごとに決まる） |
| `BattleReplayData` | .h | 15 | FBattleReplayHeader, FBattleReplayUnit | 再生用データの型（試験専用） |
| `BattleRoutes` | .cpp | 89 |  | 通り道：道路網のあるマップ（F）で、遠い目的地へ交差点を順に指示する。本拠地へ向かう部隊は観測した敵の少ない側面路を選ぶ（UBattleMatch の一部） |
| `BattleSave` | .h/.cpp | 76 + 260 | FBattleSavedUnit, FBattleSavedDamage, UBattleSave | 保存形式、試合の保存と復元、検証 |
| `BattleScoutReport` | .cpp | 160 |  | 偵察報告（UBattleMatch の一部） |
| `BattleSquadAI` | .h/.cpp | 35 + 197 | FBattleSquadPlan, UBattleSquadAI | 旧全Actor経路の部隊 AI |
| `BattleSquadFlank` | .cpp | 189 |  | 迂回の判断（UBattleSquadTactics の一部） |
| `BattleSquadRetreat` | .cpp | 234 |  | 撤退の判断と適用（UBattleSquadTactics の一部。Evaluate と Apply はここ） |
| `BattleSquadSupport` | .cpp | 119 |  | 援護の受け渡し（UBattleSquadTactics の一部） |
| `BattleSquadTactics` | .h/.cpp | 100 + 151 | FBattleSquadObservation, UBattleSquadTactics | 部隊の戦術の土台：観測、毎フレームの更新、記録 |
| `BattleStableMovement` | .cpp | 429 |  | 混成の移動計画と実行（UBattleLeaderRuntime の一部） |
| `BattleStance` | .h/.cpp | 28 + 98 | BattleStance | 姿勢（立ち・しゃがみ）の数値と切替 |
| `BattleStanceAnimImport` | .cpp | 43 |  | 姿勢アニメーションの取り込み（エディタ用。宣言は BattleVATBake.h） |
| `BattleState` | .h/.cpp | 723 + 563 | BattleSquadType, FBattleDecisionProfile, FBattleSquadTacticalPlan, FBattleSquadState, FBattlePerceptionProfile, FBattleUnitState, UBattleTuning, UBattleState | 正規状態：兵・部隊の記録、命令、弾薬、被害、調整値 UBattleTuning、兵科の差 |
| `BattleSupport` | .cpp | 60 |  | 旧アリーナの支援砲撃（UBattleMatch の一部） |
| `BattleSuppression` | .h/.cpp | 16 + 73 | BattleSuppression | 制圧の値と効果 |
| `BattleTacticalAI` | .cpp | 124 |  | 旧全Actor経路の兵の戦術行動（ABattleAIController の一部） |
| `BattleTactics` | .h/.cpp | 109 + 249 | FBattleExposure, BattleTactics | 調整値の束（補給、雇用、戦略、収入）と試験切替、散らばり、露出の計算 |
| `BattleTacticsSave` | .cpp | 23 |  | 戦術状態と陣営 AI の保存・復元 |
| `BattleTankGun` | .cpp | 143 |  | 戦車砲の照準と発射（UBattleLeaderRuntime の一部） |
| `BattleTeamAI` | .h/.cpp | 24 + 138 | UBattleTeamAI | 陣営全体の判断 |
| `BattleTerrain` | .h | 176 | BattleTerrain, FHeightPatch, FHeights, FHouseShape | 地面の高さの表（マップG）：丘・階段・2階の矩形と、その場所の高さを返す `FHeights::Ground`。家と丘の位置（`HousesG/HillsG`）は生成と共通 |
| `BattleUIText` | .h/.cpp | 11 + 134 | BattleUIText | 画面の文言の日本語化 |
| `BattleUnit` | .h/.cpp | 123 + 423 | FBattleSightContext, ABattleUnit, ABattleAIController | 兵の Actor（表示と直接操作。正規状態は BattleState 側） |
| `BattleVATBake` | .h/.cpp | 8 + 194 |  | VAT の焼き込み・検証・修復（エディタ用） |
| `BattleVATData` | .h | 40 | FBattleVATClip, FBattleVATMesh, UBattleVATData | VAT のデータ型 |
| `BattleVATDisplay` | .h/.cpp | 82 + 400 | UBattleVATSkeletalMeshComponent, FBattleVATEntry, ABattleVATDisplay | 遠距離の兵を VAT で描く表示 |
| `BattleVATMaterial` | .cpp | 123 |  | VAT 用マテリアルの作成（エディタ用。宣言は BattleVATBake.h） |
| `BattleVehicle` | .cpp | 161 |  | 車両の設定、操舵、砲塔、車両どうしの視認（UBattleLeaderRuntime の一部） |
| `BattleVehicleNavMesh` | .h | 13 | ABattleVehicleNavMesh | 車両専用のナビメッシュの型（歩兵用と分けるため） |
| `BattleVehicleTactics` | .cpp | 61 |  | Vehicle tactics, stage 2 (2026-11-18, user request "tactics first"): where an armoured car goes and what it avoids. |
| `BattleViewVisibility` | .h/.cpp | 21 + 50 | FT5Visibility | 視点ごとの描画の切替（T5 試験用） |
| `BattleWarScore` | .h/.cpp | 18 + 79 | BattleWarScore | 戦果点（UBattleMatch の一部） |

## モジュール一覧（試験・計測・生成）

名前が Smoke / Trial / Soak / Benchmark / Probe / Pilot / Checks / Test / Audit / Diagnostic / Experiment / Validation / Shot / Visual / Preview / Commandlet で終わるもの。

| モジュール | 種類 | 行数 | 宣言している型 | 役割 |
|---|---|---|---|---|
| `BattleAbilitySmoke` | .cpp | 125 |  |  |
| `BattleAnimTaskTrial` | .h/.cpp | 29 + 85 | ABattleAnimTaskTrial |  |
| `BattleAreaSmoke` | .cpp | 760 |  |  |
| `BattleBenchmark` | .cpp | 194 |  |  |
| `BattleConeTrialCommandlet` | .h/.cpp | 12 + 48 | UBattleConeTrialCommandlet | Closest distance between a forward ray and the capsule's vertical centre segment. |
| `BattleContactTrial` | .cpp | 101 |  | Fixed contact fixtures retain all 500 bodies, collision, and the normal executor. |
| `BattleContextTrial` | .cpp | 52 |  | Test-only full-state fingerprints. Never enabled during performance runs. |
| `BattleCornerTrial` | .cpp | 159 |  |  |
| `BattleCoverActionSmoke` | .cpp | 131 |  |  |
| `BattleDelegationSmoke` | .cpp | 121 |  |  |
| `BattleDisplayLODTrial` | .cpp | 83 |  |  |
| `BattleExoShot` | .cpp | 72 |  | Exoskeleton look check (2026-11-18): -BattleExoShot -BattleArea -BattleWideScreens=<prefix> on a wide mixed map. |
| `BattleFieldTrial` | .cpp | 430 |  |  |
| `BattleFieldViewTrial` | .cpp | 248 |  |  |
| `BattleFlankBayTrial` | .cpp | 68 |  |  |
| `BattleGridTrial` | .cpp | 86 |  | Test-only synchronous mixed Actor/Mass fixture. Never used for performance. |
| `BattleHiddenBoneTrial` | .h/.cpp | 27 + 74 | ABattleHiddenBoneTrial |  |
| `BattleLargeFieldTest` | .cpp | 83 |  |  |
| `BattleLeaderSmoke` | .cpp | 142 |  |  |
| `BattleMapCommandlet` | .h/.cpp | 14 + 500 | UBattleMapCommandlet | Reproducible editor-only arena authoring; never overwrites a map unless -Force is passed. |
| `BattleMassTrial` | .h/.cpp | 49 + 86 | UBattleTrialMovement, ABattleMassTrialUnit, FBattleTrialMotion, UBattleMassTrial | Flat-ground benchmark adapter. Navigation, RVO and swept capsule collisions |
| `BattleMatchSmoke` | .cpp | 58 |  |  |
| `BattleMatchSoak` | .cpp | 60 |  |  |
| `BattleMaterialTrial` | .cpp | 106 |  |  |
| `BattleMixedFastSmoke` | .cpp | 41 |  |  |
| `BattleMixedSaveSmoke` | .cpp | 121 |  |  |
| `BattleMixedSaveValidation` | .cpp | 112 |  |  |
| `BattleMovementTrial` | .cpp | 112 |  |  |
| `BattleMovingSquadTrial` | .cpp | 222 |  |  |
| `BattleNetworkBenchmark` | .cpp | 119 |  |  |
| `BattleNetworkPilot` | .cpp | 140 |  |  |
| `BattleNetworkPilotProbe` | .cpp | 104 |  |  |
| `BattleNetworkProbe` | .cpp | 83 |  |  |
| `BattleObjectiveSmoke` | .cpp | 69 |  |  |
| `BattleOrderSmoke` | .cpp | 154 |  |  |
| `BattlePerceptionOptimizationSmoke` | .cpp | 150 |  |  |
| `BattlePerceptionSmoke` | .cpp | 171 |  |  |
| `BattlePhase3Checks` | .cpp | 89 |  |  |
| `BattleRenderDiagnostic` | .h/.cpp | 37 + 217 | FT5RenderDiagnostic | Test-only state. Components are owned by a world Actor, never combat units. |
| `BattleSaveSmoke` | .cpp | 81 |  |  |
| `BattleScaleSmoke` | .cpp | 84 |  |  |
| `BattleScopeShot` | .cpp | 48 |  | 構え・スコープの撮影試験（UBattleLeaderRuntime の一部） |
| `BattleSightExperiment` | .h/.cpp | 56 + 296 | FBattleSightExperiment |  |
| `BattleSightTrial` | .cpp | 77 |  | Diagnostic fixture: retain 500 bodies, isolate one observer and one target. |
| `BattleSightVisual` | .cpp | 37 |  | 視界の目視確認用の表示（UBattleLeaderRuntime の一部） |
| `BattleSquadFlankSmoke` | .cpp | 181 |  |  |
| `BattleSquadRetreatSmoke` | .cpp | 137 |  |  |
| `BattleSquadSmoke` | .cpp | 215 |  |  |
| `BattleSquadTacticsSmoke` | .cpp | 148 |  |  |
| `BattleSquadTrial` | .h/.cpp | 164 + 181 | FBattleTrialSoldier, UBattleSquadTrial, FT2bUnit, FMember, FFieldUnit |  |
| `BattleStanceShot` | .cpp | 101 |  | 姿勢の撮影試験（UBattleLeaderRuntime の一部） |
| `BattleSupportSmoke` | .cpp | 75 |  |  |
| `BattleTacticalSmoke` | .cpp | 182 |  |  |
| `BattleTacticsSmoke` | .cpp | 111 |  |  |
| `BattleTeamSupportSmoke` | .cpp | 134 |  |  |
| `BattleTerrainTest` | .cpp | 85 |  | 高低差の自己試験 `-BattleHeightTest`：丘の上・2階・尾根越えへ3部隊を歩かせ、着いた高さと浮き沈みを調べる（UBattleMatch の一部） |
| `BattleThirdPersonSmoke` | .cpp | 110 |  |  |
| `BattleVATBakeCommandlet` | .h/.cpp | 12 + 51 | UBattleVATBakeCommandlet |  |
| `BattleWeaponPreview` | .cpp | 60 |  | 武器の見た目の確認 |
| `BattleWeaponRangeTrial` | .cpp | 47 |  |  |
| `BattleWideMapAudit` | .cpp | 198 |  |  |
| `BattleWideMixedSmoke` | .cpp | 208 |  |  |
