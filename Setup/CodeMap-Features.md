## 毎フレームの流れ（どこから呼ばれるか）

入口は @ABattleGameMode::Tick 。ここで2つの経路に分かれる。

- **混成（隊長Actor＋Mass部下。広域・エリア戦の通常経路）**：@UBattleLeaderRuntime::Step がすべてを回す。順に、試合の進行 @UBattleMatch::Update → 部隊の判断 @UBattleSquadTactics::Step → 認識 @UBattlePerception::Update → 隊長の判断 @UBattleLeaderRuntime::Think → 兵ごとの移動・射撃 @UBattleLeaderRuntime::Follow → 表示 @UBattleLeaderRuntime::RefreshDisplay 。
- **旧全Actor（80m×60m の試作アリーナ）**：GameMode が直接 @UBattleMatch::Update 、 @UBattleSquadTactics::Step 、 @ABattleGameMode::RefreshVisibility 、 @UBattleSquadAI::Update を呼ぶ。兵ごとの実行は @ABattleAIController::Tick 。

エリア戦のときは @UBattleMatch::Update の中で @UBattleMatch::UpdateArea を毎フレーム、 @UBattleMatch::UpdateAreaHiring と @UBattleMatch::UpdateEnemyOrders を5秒ごとに呼ぶ。起動時の組み立て（どのモードか、どの試験か）は @ABattleGameMode::BeginPlay と @UBattleLeaderRuntime::Start 。

`UBattleLeaderRuntime` と `UBattleMatch` と `UBattleSquadTactics` と `UBattleSquadTrial` は、1つのクラスを複数の .cpp に分けて書いている。クラス名でファイルを決めつけず、下の表か [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) で引く。

## 機能 → 入口

| 機能 | 入口（ここから読む） | 関連 |
|---|---|---|
| 正規状態・UnitID・部隊 | @UBattleState::RegisterUnit 、 @UBattleState::CreateSquad 、 @UBattleState::ApplyDamage | 構造体は `BattleState.h`（FBattleUnitState、FBattleSquadState）。調整値は同じ .h の `UBattleTuning`（ini で上書き） |
| 兵科（歩兵・重火器・偵察・対戦車・戦車）の差 | @BattleSquadType::Members から @BattleSquadType::Weapon までの一群、 @UBattleState::SetSquadType | 射程は @BattleRanges::Get |
| 命令（移動・攻撃移動・防御・待機） | 入力 @ABattleController::IssueOrderAt → 状態 @UBattleState::SetSquadOrder → 混成 @UBattleLeaderRuntime::IssueOrder | 委任は @UBattleState::SetSquadDelegated 、 @UBattleMatch::SetAllyDelegated |
| 入力・RTS/TPS 切替・ゲームパッド | @ABattleController::SetupInputComponent 、 @ABattleController::InputKey 、 @ABattleController::ToggleMode 、 @ABattleController::ControlUnit | ゲームパッドは @ABattleController::UpdateGamepad 。隊長交代は @UBattleState::HandOverLeader |
| HUD・ミニマップ・部隊一覧・通知 | @ABattleHUD::DrawHUD 、 @ABattleHUD::DrawMinimap 、 @ABattleHUD::DrawSquadList 、 @ABattleHUD::DrawNoticeLog | 通知の中身は @ABattleController::UpdateNoticeLog 、 @ABattleController::PushNotice 。日本語化は @BattleUIText::Ja |
| 認識（見えた敵の記憶） | @UBattlePerception::Update 、 @UBattlePerception::GetContacts | 視線は @UBattleLeaderRuntime::CanSee 、 @UBattleLeaderRuntime::TraceSight 。試験用の視界切替は @BattleSightExperiment |
| 隊長の判断と兵の実行（混成） | @UBattleLeaderRuntime::Think 、 @UBattleLeaderRuntime::Follow 、 @UBattleLeaderRuntime::Fire | 兵単位の実行は @FBattleMixedDriver::Tick 、 @FBattleMixedDriver::TryTacticalAction |
| 移動・経路・混雑 | @UBattleLeaderRuntime::StableMove 、 @UBattleLeaderRuntime::BuildMovementPlan | 到着地点の重なりは @UBattleLeaderRuntime::ReassignOccupiedArrival 。目的地の補正は @BattleNavigation::ResolveGoal |
| 遮蔽（場所の予約） | @UBattleCover::BuildSlots 、 @UBattleCover::Reserve | 露出の計算は @BattleTactics::ExposureAt |
| 遮蔽の動作（接近・のぞき撃ち） | @UBattleLeaderRuntime::RequestCoverAction 、 @UBattleLeaderRuntime::StepCoverAction | 切替は @BattleTactics::CoverActionsEnabled ほか |
| 姿勢（立ち・しゃがみ） | @UBattleLeaderRuntime::SetStance 、 @UBattleLeaderRuntime::UpdateAIStance | 数値は @BattleStance::EyeOffset ほか |
| 制圧（撃たれて動けない） | @UBattleLeaderRuntime::AddSuppression 、 @UBattleLeaderRuntime::IsPinned | 調整値は @BattleSuppression::Tuning |
| 射撃の散らばり | @BattleTactics::SpreadDegrees 、 @BattleTactics::ShotDirection | 照準の遅れは @UBattleState::UpdateAim 。プレイヤーの直接操作だけの倍率は @ABattleUnit::PlayerSpreadScale |
| 部隊の戦術（撤退・集合・援護・迂回） | @UBattleSquadTactics::Step 、 @UBattleSquadTactics::Evaluate 、 @UBattleSquadTactics::Apply | 撤退 @UBattleSquadTactics::RetreatGoal 、援護 @UBattleSquadTactics::AcceptSupport 、迂回 @UBattleSquadTactics::TryFlank |
| 陣営全体の判断（援護・迂回の割り振り） | @UBattleTeamAI::Step | |
| 試合の進行・勝敗・制限時間 | @UBattleMatch::InitializeMatch 、 @UBattleMatch::Update 、 @UBattleMatch::Finish | 拠点の占領は @UBattleMatch::UpdateObjectives |
| 雇用（旧・固定の価格） | @UBattleMatch::HireSquad 、 @UBattleMatch::CurrentSquadPrice | 実体を出すのは @UBattleLeaderRuntime::SpawnPurchasedSquad |
| エリア戦の雇用（AI が何を雇うか） | @UBattleMatch::UpdateAreaHiring 、 @UBattleMatch::AreaHireNeed 、 @UBattleMatch::HireAreaSquad | 調整値は @BattleTactics::AreaHire |
| エリア戦の収入・拠点 HP | @UBattleMatch::UpdateArea （収入の支払いもここ）、 @UBattleMatch::DamageBase | 調整値は @BattleTactics::AreaIncome 。状態は `BattleAreaState.h` |
| エリア戦の AI 戦略（どこを攻めるか） | @UBattleMatch::UpdateEnemyOrders → @UBattleMatch::UpdateTeamOrders 、 @UBattleMatch::AreaStrategyTarget 、 @UBattleMatch::AreaIntent | 調整値は @BattleTactics::AreaStrategy 。前線は @UBattleMatch::AreaFronts |
| 偵察・情報（見えていない区域の扱い） | @UBattleMatch::UpdateScoutReports 、 @UBattleMatch::AreaIntelView | 偵察兵の配置は @UBattleMatch::AreaScoutPost 。昇格は @UBattleLeaderRuntime::PromoteScout |
| 補給（弾薬） | @UBattleMatch::Resupply 、 @BattleTactics::Supply | 往復時間の記録（評価用）は @UBattleSquadTactics::SupplyTripEnd 。エリア戦の本拠地・味方の拠点での弾の補充は @UBattleMatch::UpdateArea 、直接操作中に部下から弾を受け取るのは @ABattleController::UpdatePlayerAmmo |
| 補充（倒れた部下の補充） | @UBattleLeaderRuntime::ReplaceFollower 、 @UBattleState::ReplaceDeadFollower | |
| 戦果点 | @UBattleMatch::UpdateWarScore 、 @UBattleMatch::WarScoreBreakdown | 調整値は @BattleWarScore::Tuning |
| 支援砲撃（旧アリーナ） | @UBattleMatch::RequestSupport 、 @UBattleMatch::UpdateSupport | 混成には未接続 |
| 車両・戦車砲・対戦車 | @UBattleLeaderRuntime::ConfigureVehicle 、 @UBattleLeaderRuntime::SteerVehicle 、 @UBattleLeaderRuntime::FireTankGun | 爆発は @UBattleLeaderRuntime::Explode |
| 保存・復元 | @UBattleMatch::SaveMatch 、 @UBattleMatch::LoadMatch 、 @UBattleMatch::CaptureSnapshot 、 @UBattleMatch::RestoreSnapshot | 検証は @UBattleSave::Validate 、 @UBattleSave::ValidateMixed 。戦術状態は @UBattleSquadTactics::CaptureSave |
| 表示（混成の体・遠距離 VAT） | @UBattleLeaderRuntime::CreateBody 、 @UBattleLeaderRuntime::RefreshDisplay 、 @ABattleVATDisplay::Tick | 復元時は @UBattleLeaderRuntime::RestoreMixedBodies |
| マップの定義・配置 | `BattleFieldLayout.h`（マップごとの表 `Maps()`） | 生成は @UBattleMapCommandlet::Main （既存の手編集を `-Force` で上書きしない） |
| 通り道（道路網に沿った移動の指示） | @UBattleMatch::RouteStep 、 @UBattleMatch::RouteFlankSide | 呼び出しは @UBattleMatch::UpdateTeamOrders 。道路網は `FMapDef::RouteNodes/RouteEdges` |
| 高低差（丘・階段・2階。マップG） | @UBattleLeaderRuntime::TerrainStep 、 @UBattleMatch::UpdateTerrainTest | 高さの表は `BattleTerrain.h`（`FHeights::Ground`、家と丘の位置 `HousesG/HillsG`）と `FMapDef::Heights`。形は `BattleMapLayoutG.inl`。隊長はエンジンの歩行、多数兵は表に沿う。自己試験 `Test-WideSight.ps1 -Map G -HeightTest` |
| マップの数値監査（試合なし） | @UBattleLeaderRuntime::RunMapStats | `Setup/Test-MapStats.ps1`。拠点ごとの持ち主・価値は `FMapDef::ObjectiveOwners/ObjectiveValues` |
| 計測の記録 | @UBattleMetrics::BeginFrame 、 @UBattleMetrics::Finish | 段取りは MEASURE.md |
| ネットワーク試作 | @ABattleNetGameMode::BeginPlay 、 @ABattleNetController::ServerOrder_Implementation | |
| 基本回帰（BATTLE_SMOKE_RESULT） | @ABattleGameMode::RunSmokeTest 、 @ABattleGameMode::FinishSmokeTest | 個別の試験は各 `*Smoke.cpp` の `Run…` |

## モジュールの説明

BattleAIController | 旧全Actor経路の兵の実行（移動・再試行）。混成では使わない
BattleAnimTasks | アニメーション更新の並列実行の設定を兵ごとに切り替え、元の設定を覚えておく
BattleAntiTank | 対戦車弾の爆発（UBattleLeaderRuntime の一部）
BattleArea | エリア戦の本体：初期化、収入、拠点、AI の雇用と意図（UBattleMatch の一部）
BattleAreaIntel | エリア戦の情報：観測に基づく区域の見え方と脅威（UBattleMatch の一部）
BattleAreaState | エリア戦の保存される状態（拠点 HP、収入、観測）
BattleArmourAI | 兵種を扱うAI：効かない車両を狙わない、対戦車兵の弾の温存、知っている敵車両、戦車の待機位置、兵種の構成（UBattleLeaderRuntime と UBattleMatch の一部）
BattleArtillery | 迫撃砲と自走砲：間接射撃の目標選び、着弾、陣地の位置（UBattleLeaderRuntime と UBattleMatch の一部）
BattleContactMovement | 停止中の味方との接触・到着地点の重なりの解消（UBattleLeaderRuntime の一部）
BattleExo | 外骨格兵：盾の回復、副武器（肩の対戦車火器）、専用の部隊判断と軍の割り当て、見た目（AC01の体と武器＝ApplyExoSuit）
BattleController | プレイヤーの入力、RTS/TPS 切替、選択、命令、通知、ゲームパッド
BattleCover | 遮蔽の場所の一覧と予約
BattleCoverAction | 遮蔽への接近とのぞき撃ちの動作（UBattleLeaderRuntime の一部）
BattleFieldLayout | マップごとの定義（広さ、拠点、試合時間）を1つの表にまとめたもの
BattleGroups | 部隊群：編成と解散、役割、集結・前進・保持の段階、AIの群の運用、群に従う命令（UBattleMatch の一部）
BattleFieldReplay | 広域試験の記録と再生（UBattleSquadTrial の一部）
BattleFieldTurn | 広域試験 T2b（UBattleSquadTrial の一部）
BattleFollowerBones | 霧で隠れた部下のボーン表示の切替
BattleGameMode | モードの統合、起動時の組み立て、毎フレームの入口、基本回帰
BattleHUD | 画面表示（HUD、ミニマップ、部隊一覧、通知、命令の印）
BattleLeaderRuntime | 混成の中心：起動、視線、隊長の判断、兵の移動と射撃、表示の更新
BattleMatch | 試合の進行、雇用、拠点の占領、陣営の命令、補給、勝敗
BattleMetrics | 計測値の記録（1フレームの時間、判断の回数）
BattleMixedBodies | 混成の兵の体の生成・破棄・復元（UBattleLeaderRuntime の一部）
BattleMixedDriver | 混成の兵1人ぶんの実行（旧 AIController と同じ役を Actor なしで行う）
BattleMixedRecruitment | 補充と購入部隊の出現、偵察兵の昇格（UBattleLeaderRuntime の一部）
BattleMixedSquadAI | 部隊 AI の混成版の更新（UBattleSquadAI の一部）
BattleMoveCourse | 移動の計測コース
BattleNavigation | 目的地の補正と隊形の位置
BattleNetwork | ネットワーク試作（GameMode、Controller、HUD）
BattleNetworkSerialization | ネットワーク用の兵の直列化
BattlePerception | 認識：見えた敵の記憶、保存と復元
BattlePhase3 | Phase3 の大規模試験経路（通常の混成とは別の処理）
BattleRanges | 認識・交戦の距離（マップごとに決まる）
BattleReplayData | 再生用データの型（試験専用）
BattleSave | 保存形式、試合の保存と復元、検証
BattleScoutReport | 偵察報告（UBattleMatch の一部）
BattleSightVisual | 視界の目視確認用の表示（UBattleLeaderRuntime の一部）
BattleSquadAI | 旧全Actor経路の部隊 AI
BattleSquadFlank | 迂回の判断（UBattleSquadTactics の一部）
BattleSquadRetreat | 撤退の判断と適用（UBattleSquadTactics の一部。Evaluate と Apply はここ）
BattleSquadSupport | 援護の受け渡し（UBattleSquadTactics の一部）
BattleSquadTactics | 部隊の戦術の土台：観測、毎フレームの更新、記録
BattleStableMovement | 混成の移動計画と実行（UBattleLeaderRuntime の一部）
BattleStance | 姿勢（立ち・しゃがみ）の数値と切替
BattleStanceAnimImport | 姿勢アニメーションの取り込み（エディタ用。宣言は BattleVATBake.h）
BattleStanceShot | 姿勢の撮影試験（UBattleLeaderRuntime の一部）
BattleScopeShot | 構え・スコープの撮影試験（UBattleLeaderRuntime の一部）
BattleMuzzleFlash | 銃口の閃光（表示だけ。発光する楕円体の使い回し）
BattleState | 正規状態：兵・部隊の記録、命令、弾薬、被害、調整値 UBattleTuning、兵科の差
BattleSupport | 旧アリーナの支援砲撃（UBattleMatch の一部）
BattleSuppression | 制圧の値と効果
BattleTacticalAI | 旧全Actor経路の兵の戦術行動（ABattleAIController の一部）
BattleTactics | 調整値の束（補給、雇用、戦略、収入）と試験切替、散らばり、露出の計算
BattleTacticsSave | 戦術状態と陣営 AI の保存・復元
BattleTankGun | 戦車砲の照準と発射（UBattleLeaderRuntime の一部）
BattleTeamAI | 陣営全体の判断
BattleUIText | 画面の文言の日本語化
BattleUnit | 兵の Actor（表示と直接操作。正規状態は BattleState 側）
BattleVATBake | VAT の焼き込み・検証・修復（エディタ用）
BattleVATData | VAT のデータ型
BattleVATDisplay | 遠距離の兵を VAT で描く表示
BattleVATMaterial | VAT 用マテリアルの作成（エディタ用。宣言は BattleVATBake.h）
BattleVehicle | 車両の設定、操舵、砲塔、車両どうしの視認（UBattleLeaderRuntime の一部）
BattleVehicleTactics | 車両の戦術：車両への脅威の見積もり、装甲車の行き先（UBattleMatch の一部）
BattleVehicleNavMesh | 車両専用のナビメッシュの型（歩兵用と分けるため）
BattleViewVisibility | 視点ごとの描画の切替（T5 試験用）
BattleWarScore | 戦果点（UBattleMatch の一部）
BattleWeaponPreview | 武器の見た目の確認
BattleMapStats | マップの統計（区域ごとの見通し・遮蔽、経路の長さ）を JSON に書き出す。試合は行わない（UBattleLeaderRuntime の一部）
BattleAreaDepot | 集積所の重み：価値が1より大きい拠点（マップFの集積所）への割り振りの重みを、守る・取り返す・襲うの状況で変える（UBattleMatch の一部）
BattlePlayerRoutes | 通り道のプレイヤー指定：部隊ごとに 自動・中央・北・南 を選び、手動の遠い命令と委任中の移動をその道に通す。自動試験 `-BattlePlayerRouteTest`（UBattleMatch の一部）
BattleTerrain | 地面の高さの表（マップG）：丘・階段・2階の矩形と、その場所の高さを返す `FHeights::Ground`。家と丘の位置（`HousesG/HillsG`）は生成と共通
BattleTerrainTest | 高低差の自己試験 `-BattleHeightTest`：丘の上・2階・尾根越えへ3部隊を歩かせ、着いた高さと浮き沈みを調べる（UBattleMatch の一部）
BattlePlayerPath | プレイヤーの入力の補助：命令先の点（2階は壁越しでも指せる、Alt で1階）`PickOrderPoint`、右ドラッグで引いた道筋の命令 `PlayerPathOrder`、選んだ部隊の道筋の予測表示 `DrawOrderWays`／`PlayerRoutePreview`、壁の乗り越え `TryVault`、作戦カメラの速さ `PanSpeed`（ABattleController・UBattleMatch・ABattleHUD の一部）
BattleRoutes | 通り道：道路網のあるマップ（F）で、遠い目的地へ交差点を順に指示する。本拠地へ向かう部隊は観測した敵の少ない側面路を選ぶ（UBattleMatch の一部）
