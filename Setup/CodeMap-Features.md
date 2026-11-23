## 毎フレームの流れ（どこから呼ばれるか）

入口は @ABattleGameMode::Tick 。ここで2つの経路に分かれる。

- **混成（隊長Actor＋Mass部下。広域・エリア戦の通常経路）**：@UBattleLeaderRuntime::Step がすべてを回す。順に、試合の進行 @UBattleMatch::Update → 部隊の判断 @UBattleSquadTactics::Step → 認識 @UBattlePerception::Update → 隊長の判断 @UBattleLeaderRuntime::Think → 兵ごとの移動・射撃 @UBattleLeaderRuntime::Follow → 表示 @UBattleLeaderRuntime::RefreshDisplay 。
- **旧全Actor（80m×60m の試作アリーナ）**：GameMode が直接 @UBattleMatch::Update 、 @UBattleSquadTactics::Step 、 @ABattleGameMode::RefreshVisibility 、 @UBattleSquadAI::Update を呼ぶ。兵ごとの実行は @ABattleAIController::Tick 。

エリア戦のときは @UBattleMatch::Update の中で @UBattleMatch::UpdateArea を毎フレーム、 @UBattleMatch::UpdateAreaHiring と @UBattleMatch::UpdateEnemyOrders を5秒ごとに呼ぶ。起動時の組み立て（どのモードか、どの試験か）は @ABattleGameMode::BeginPlay と @UBattleLeaderRuntime::Start 。

`UBattleLeaderRuntime` と `UBattleMatch` と `UBattleSquadTactics` と `UBattleSquadTrial` は、1つのクラスを複数の .cpp に分けて書いている。クラス名でファイルを決めつけず、下の表か [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) で引く。

## 機能 → 入口

| 機能 | 入口（ここから読む） | 関連 |
|---|---|---|
| 編成AI・候補選択と貯蓄購入 | @UBattleMatch::UpdateCompositionHiring 、 @BattleComposition::Decide 、 @BattleComposition::ValidateSave | Planner/PlannerTeams は既定0。ONは不足・緊急性で選択し貯蓄／購入。CombatStateVersion=4、旧保存はPlanner無効。検査は @UBattleLeaderRuntime::RunCompositionSmoke |
| 盾被弾制圧・群の兵種別戦力（試験） | @UBattleLeaderRuntime::NoteUnderFire 、 @UBattleState::AdvanceShields 、 @UBattleMatch::GroupPower 、 @UBattleMatch::GroupPointPower | `BattleExoShieldSuppression` / `BattleGroupStrength` は既定0。観測兵種は @UBattlePerception::Update で取得、重みはUBattleTuning |
| 間接射撃の試験切替 | @UBattleMatch::ArtilleryHomeObserved 、 @UBattleMatch::AreaArtilleryClusters 、 @UBattleLeaderRuntime::FireArtillery 、 @UBattleLeaderRuntime::ArtilleryBlastTransmission | 自走砲弾は @UBattleState::UseSPGAmmo / @UBattleState::RefillSPGAmmo 。4切替は既定0、CombatStateVersion=3で弾数と切替を保存 |
| 正規状態・UnitID・部隊 | @UBattleState::RegisterUnit 、 @UBattleState::CreateSquad 、 @UBattleState::ApplyDamage | 構造体は `BattleState.h`（FBattleUnitState、FBattleSquadState）。調整値は同じ .h の `UBattleTuning`（ini で上書き） |
| 兵科（歩兵・重火器・偵察・対戦車・戦車）の差 | @BattleSquadType::Members から @BattleSquadType::Weapon までの一群、 @UBattleState::SetSquadType | 射程は @BattleRanges::Get |
| 命令（移動・攻撃移動・防御・待機） | 入力 @ABattleController::IssueOrderAt → 状態 @UBattleState::SetSquadOrder → 混成 @UBattleLeaderRuntime::IssueOrder | 委任は @UBattleState::SetSquadDelegated 、 @UBattleMatch::SetAllyDelegated |
| 入力・RTS/TPS 切替・ゲームパッド | @ABattleController::SetupInputComponent 、 @ABattleController::InputKey 、 @ABattleController::ToggleMode 、 @ABattleController::ControlUnit | ゲームパッドは @ABattleController::UpdateGamepad 。隊長交代は @UBattleState::HandOverLeader |
| HUD・ミニマップ・部隊一覧・通知 | @ABattleHUD::DrawHUD 、 @ABattleHUD::DrawMinimap 、 @ABattleHUD::DrawSquadList 、 @ABattleHUD::DrawNoticeLog | 通知の中身は @ABattleController::UpdateNoticeLog 、 @ABattleController::PushNotice 。日本語化は @BattleUIText::Ja 。兵種アイコン・敵の最終確認地点は @ABattleHUD::DrawSquadIcons 。射程の円は @ABattleHUD::DrawRangeRings ・ミニマップは @ABattleHUD::DrawMinimapRangeRings 、敵の円は L キー（ @ABattleController::ToggleEnemyRanges ）（射程は @BattleRanges::Engage ）。被弾方向は @ABattleHUD::DrawHitDirection （記録は @UBattleState::ApplyDamageFrom ）、画面外マーカーは @ABattleHUD::DrawOffscreenMarkers 、撃破ログは @ABattleHUD::DrawKillLog （記録は @UBattleState::NoteKill ）。作戦画面の戦場の霧・区画の格子と区画の状態は @ABattleHUD::DrawFieldOverlay 、 @ABattleHUD::DrawSectorPanel （計算は `BattleSectorView`） |
| 認識（見えた敵の記憶） | @UBattlePerception::Update 、 @UBattlePerception::GetContacts | 視線は @UBattleLeaderRuntime::CanSee 、 @UBattleLeaderRuntime::TraceSight 。試験用の視界切替は @BattleSightExperiment |
| 隊長の判断と兵の実行（混成） | @UBattleLeaderRuntime::Think 、 @UBattleLeaderRuntime::Follow 、 @UBattleLeaderRuntime::Fire | 兵単位の実行は @FBattleMixedDriver::Tick 、 @FBattleMixedDriver::TryTacticalAction |
| 移動・経路・混雑 | @UBattleLeaderRuntime::StableMove 、 @UBattleLeaderRuntime::BuildMovementPlan | 到着地点の重なりは @UBattleLeaderRuntime::ReassignOccupiedArrival 。目的地の補正は @BattleNavigation::ResolveGoal |
| 遮蔽（場所の予約） | @UBattleCover::BuildSlots 、 @UBattleCover::Reserve | 露出の計算は @BattleTactics::ExposureAt |
| 体のアニメーション | @ABattleUnit::UpdateBodyAnim 、 @FBattleBodyAnimProxy::Evaluate | 部下は @UBattleLeaderRuntime::FollowerClip 、 @UBattleLeaderRuntime::StepBodyTimers |
| 効果音・エフェクト（表示と音だけ） | @BattleAudio::Shot 、 @BattleAudio::Explosion 、 @BattleAudio::TickPlayer 、 @BattleEffects::Explosion | 呼び出しは @UBattleLeaderRuntime::Fire （射撃・着弾・かすめる弾）、 @UBattleLeaderRuntime::Explode （対戦車弾・戦車砲弾・外骨格兵の発射筒の爆発）、 @UBattleLeaderRuntime::FireArtillery ・ @UBattleLeaderRuntime::StepArtilleryShells ・ @UBattleLeaderRuntime::ArtilleryImpact （砲撃）、 @ABattleController::PlayerTick （作戦画面の聞く位置・装填/構え/スコープ・拠点と勝敗の合図）。`-BattleSFX=0`／`-BattleFX=0` で止まる。素材と出典は `Docs/ASSET_SOURCES_AUDIO_FX.md` |
| 死体の消滅（表示だけ） | @UBattleLeaderRuntime::StepCorpses 、 @UBattleLeaderRuntime::HideCorpse | 登録は OnDeath の @UBattleLeaderRuntime::AddCorpse |
| 死体の姿勢（死亡アニメ・遠景VATとの切替） | @UBattleLeaderRuntime::DeathClip 、 @ABattleVATDisplay::ShowNear 、 @ABattleVATDisplay::Select | 死ぬと遠景表示から外れて死亡クリップを流し、隠れるまで遠景に戻らない。クリップの後は物理で倒れて止まる（ @BattleRagdoll::Start 、 @BattleRagdoll::Settle 。部下は @UBattleLeaderRuntime::StepBodyTimers 、隊長は @ABattleUnit::Tick ）。見た目の確認は @UBattleLeaderRuntime::RunDeathShot（Setup/Test-DeathShot.ps1） |
| ブラインドファイア | @UBattleLeaderRuntime::BlindFireOrigin 、 @UBattleLeaderRuntime::Fire | AIの試験は @UBattleLeaderRuntime::TryAIBlindFire |
| 遮蔽の動作（接近・のぞき撃ち） | @UBattleLeaderRuntime::RequestCoverAction 、 @UBattleLeaderRuntime::StepCoverAction | 切替は @BattleTactics::CoverActionsEnabled ほか |
| 姿勢（立ち・しゃがみ） | @UBattleLeaderRuntime::SetStance 、 @UBattleLeaderRuntime::UpdateAIStance | 数値は @BattleStance::EyeOffset ほか |
| 制圧（撃たれて動けない） | @UBattleLeaderRuntime::AddSuppression 、 @UBattleLeaderRuntime::IsPinned | 調整値は @BattleSuppression::Tuning |
| 兵ごとの脅威と遮蔽の取り直し（個人AI段階A） | @UBattleLeaderRuntime::GatherThreats 、 @UBattleLeaderRuntime::CheckCoverThreats | 試験切替 `-BattleThreatCover`（既定0）。遮蔽を選ぶ側は @UBattleLeaderRuntime::RequestCoverAction の2段の候補 |
| 止まった兵の見張りの向き（個人AI） | @UBattleLeaderRuntime::SoldierWatchDirection | `-BattleSoldierWatch`（既定1、11-22 ユーザー決定。=0 で切る）。`Follow` の向きの分岐で `-BattleAttackFocus` の後ろ。視野は前方120度 |
| 歩いている兵の視線（個人AI、近い戦い） | @UBattleLeaderRuntime::WalkLookDirection 、 @UBattleLeaderRuntime::KnownEnemyDirection | 試験切替 `-BattleWalkLook`（既定0）。体は進む向きのまま、視線（`Sight` の向き、`WalkLooks`）だけを知っている敵か小隊で分けた向きへ振る。`Follow` の向きの分岐の最後。診断 `hitfov_*`（視野の外から撃たれた兵の状態）は `DiagnoseHitSight` |
| 兵が自分で撃つ相手を選ぶ（個人AI、近い戦い） | @UBattleLeaderRuntime::OwnTarget | 試験切替 `-BattleOwnTarget=1/2`（既定0）。小隊の目標を撃てない兵が、自分で見ている交戦距離内の敵兵を撃つ（撃たれた方向の敵が先、次に近い敵）。`Follow` の `CanEngage` の直後で選び、狙いと射撃だけを変える。値2は、撃つ相手がいないとき撃たれた方向へ2秒向く（`-BattleHitFace` と同じ分岐）。数は `own_target_*` |
| 撃ち返す兵が遮蔽に入る（個人AI） | @UBattleLeaderRuntime::OwnTargetCover | 試験切替 `-BattleOwnTargetCover=1/2`（既定0、`-BattleOwnTarget` が要る）。自分で選んだ相手を撃つ兵が、その相手に対する近くの遮蔽を頼む（`Follow` の遮蔽探しの直前で `RequestCoverAction` を、その相手を目標にして呼ぶ）。値1は歩いていない兵だけ、値2は歩いている兵も相手が50m以内なら止まって入る。近く（約6m）に無ければ、歩いていない兵は15m以内の遮蔽へ歩く（`StartCoverApproach`、`OwnCoverWalks`、数は `own_cover_walk`）。数は `own_cover_*` と `own_target_open_{standing,walking}_frames` |
| 区画の中で兵が遮蔽を移る（個人AI段階B1） | @UBattleLeaderRuntime::StepSoldierMove 、 @UBattleLeaderRuntime::RateCoverPlace | 試験切替 `-BattleSoldierMove`（既定0）。呼び出しは @UBattleLeaderRuntime::StepCoverAction の隠れている間（1秒に1回・1フレーム4人まで） |
| 個人AIをプレイヤーの周りだけで動かす | @UBattleLeaderRuntime::SoldierAIFocus | 試験切替 `-BattleSoldierAIFocus=<m>`（既定0＝全員）、`-BattleSoldierAIFocusAt=X,Y`。段階A（@UBattleLeaderRuntime::CheckCoverThreats ほか）とB1（@UBattleLeaderRuntime::StepSoldierMove）の入口で見る |
| プレイヤーが兵1人に出す移動の命令（Ctrl＋クリックで選ぶ・右クリックで移動先・Hで戻す） | @UBattleLeaderRuntime::IssueSoldierOrder 、 @UBattleLeaderRuntime::StepSoldierOrder 、 @ABattleController::SelectSoldierAt 、直接操作では @ABattleController::CommandSoldierFromView （G） | 兵の状態 `bSoldierOrder`（保存される）。@UBattleLeaderRuntime::Follow の `bOwnOrder`。自動試験 `-BattleSoldierOrderTest`（@UBattleLeaderRuntime::StepSoldierOrderTest） |
| 射撃の散らばり | @BattleTactics::SpreadDegrees 、 @BattleTactics::ShotDirection | 照準の遅れは @UBattleState::UpdateAim 。プレイヤーの直接操作だけの倍率は @ABattleUnit::PlayerSpreadScale |
| 部隊の戦術（撤退・集合・援護・迂回） | @UBattleSquadTactics::Step 、 @UBattleSquadTactics::Evaluate 、 @UBattleSquadTactics::Apply | 撤退 @UBattleSquadTactics::RetreatGoal 、援護 @UBattleSquadTactics::AcceptSupport 、迂回 @UBattleSquadTactics::TryFlank |
| 陣営全体の判断（援護・迂回の割り振り） | @UBattleTeamAI::Step | |
| 試合の進行・勝敗・制限時間 | @UBattleMatch::InitializeMatch 、 @UBattleMatch::Update 、 @UBattleMatch::Finish | 拠点の占領は @UBattleMatch::UpdateObjectives |
| 雇用（旧・固定の価格） | @UBattleMatch::HireSquad 、 @UBattleMatch::CurrentSquadPrice | 実体を出すのは @UBattleLeaderRuntime::SpawnPurchasedSquad |
| エリア戦の雇用（AI が何を雇うか） | @UBattleMatch::UpdateAreaHiring 、 @UBattleMatch::AreaHireNeed 、 @UBattleMatch::HireAreaSquad | 調整値は @BattleTactics::AreaHire |
| プレイヤーの購入画面（B キー、2026-11-23） | @FBattleBuyPanelLayout::Make 、 @ABattleController::BuyPanelInput 、 @ABattleController::HandleBuyPanelPad 、 @ABattleHUD::DrawBuyPanel | 行を押すと @ABattleController::BuyArea （数字キーと同じ）。B は @ABattleController::BuyKey （エリア戦以外は支援要請）。画面の確認は `Setup/Test-BuyPanelShot.ps1` |
| エリア戦の収入・拠点 HP | @UBattleMatch::UpdateArea （収入の支払いもここ）、 @UBattleMatch::DamageBase | 調整値は @BattleTactics::AreaIncome 。状態は `BattleAreaState.h` |
| エリア戦の AI 戦略（どこを攻めるか） | @UBattleMatch::UpdateEnemyOrders → @UBattleMatch::UpdateTeamOrders 、 @UBattleMatch::AreaStrategyTarget 、 @UBattleMatch::AreaIntent | 調整値は @BattleTactics::AreaStrategy 。前線は @UBattleMatch::AreaFronts 。戦略層（試験、`Docs/design-strategy-layer.md`）の状況図は @UBattleMatch::BuildSituation 、集中先は @UBattleMatch::UpdateStrategy （ `FAreaIntent::FocusPoint` で S6 と攻撃群へ） |
| 偵察・情報（見えていない区域の扱い） | @UBattleMatch::UpdateScoutReports 、 @UBattleMatch::AreaIntelView | 偵察兵の配置は @UBattleMatch::AreaScoutPost 。昇格は @UBattleLeaderRuntime::PromoteScout |
| 補給（弾薬） | @UBattleMatch::Resupply 、 @BattleTactics::Supply | 往復時間の記録（評価用）は @UBattleSquadTactics::SupplyTripEnd 。エリア戦の本拠地・味方の拠点での弾の補充は @UBattleMatch::UpdateArea 、直接操作中に部下から弾を受け取るのは @ABattleController::UpdatePlayerAmmo |
| 補充（倒れた部下の補充） | @UBattleLeaderRuntime::ReplaceFollower 、 @UBattleState::ReplaceDeadFollower | |
| 戦果点 | @UBattleMatch::UpdateWarScore 、 @UBattleMatch::WarScoreBreakdown | 調整値は @BattleWarScore::Tuning |
| 支援砲撃（旧アリーナ） | @UBattleMatch::RequestSupport 、 @UBattleMatch::UpdateSupport | 混成には未接続 |
| 車両・戦車砲・対戦車 | @UBattleLeaderRuntime::ConfigureVehicle 、 @UBattleLeaderRuntime::SteerVehicle 、 @UBattleLeaderRuntime::FireTankGun | 爆発は @UBattleLeaderRuntime::Explode |
| 武器の命令・任務による交戦許可 | @BattleTactics::OrderAllowsTarget 、 @UBattleLeaderRuntime::WeaponOrderAllows | 副武器・間接射撃の試験は BattleWeaponOrderGate、装甲AIの候補制限は BattleArmourSectorGate（いずれも既定0） |
| 保存・復元 | @UBattleMatch::SaveMatch 、 @UBattleMatch::LoadMatch 、 @UBattleMatch::CaptureSnapshot 、 @UBattleMatch::RestoreSnapshot | 検証は @UBattleSave::Validate 、 @UBattleSave::ValidateMixed 。戦術状態は @UBattleSquadTactics::CaptureSave |
| プレイヤー経路の任意保存（S02） | @BattleTactics::SaveRoutes 、 @UBattleSave::Serialize 、 @UBattleSave::Validate | `-BattleSaveRoutes=1`（既定0）。PlayerRoutesVersion=1で最終命令・目的地・残り中継点・レーン・進行・相対期限を保存。OFF／経路なしは新規タグなし。検査は @UBattleLeaderRuntime::RunMixedSaveSmoke 内で一時切替 |
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
BattleBlindFire | 遮蔽の陰からのブラインドファイア（弾の出る位置・AIの試験・アニメ。UBattleLeaderRuntime の一部）
BattleBodyAnimImport | 体のアニメーション（遮蔽・伏せ・しゃがみの向き別）の取り込みと計測（エディタ用。宣言は BattleVATBake.h）
BattleCoverClips | 遮蔽の状態 → 遮蔽アニメの対応表（データだけ。「アセット参考資料の検討」セッション作）
BattleBuyPanel | プレイヤーの購入画面（B キー）：兵種の一覧の配置、クリック・パッドの操作、画面の確認 `-BattleBuyPanelShot`（ABattleController の一部）
BattleBodyAnim | 体のアニメーション：隊長の下半身と上半身の重ね・反動・被弾・死亡、部下の伏せと死亡のクリップ
BattleCorpses | 死体の消滅（表示だけ。`-BattleCorpseLifetime=秒`、既定20、0で残す。UBattleLeaderRuntime の一部）
BattleRagdoll | 死体が死亡クリップの後に物理で倒れて止まる（表示だけ。`-BattleRagdoll=0` でクリップの最終フレームのまま）
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
BattleSupplyCar | 補給車（兵種9）：積んだ弾・本拠地での積み直し・AIの命令（群の役割5、拠点の後ろ）・弾の少ない部隊の行き先（UBattleMatch の一部）
BattleFrontLine | 前線・前線維持群 `-BattleFrontLine`・奇襲群 `-BattleSurpriseGroup`（区画のあるマップ）：区画の行ごとの前線の列（自軍の兵・観測した拠点・知っている敵、隣の行と1列以内）、維持群の帯と維持する区画、押し上げ、奇襲群（装甲優先・端の行を進んで敵本拠地へ）（UBattleMatch の一部。表示は ABattleHUD::DrawFieldOverlay）
BattleSquadSupport | 援護の受け渡し（UBattleSquadTactics の一部）
BattleSquadTactics | 部隊の戦術の土台：観測、毎フレームの更新、記録
BattleStableMovement | 混成の移動計画と実行（UBattleLeaderRuntime の一部）
BattleStance | 姿勢（立ち・しゃがみ）の数値と切替
BattleStanceAnimImport | 姿勢アニメーションの取り込み（エディタ用。宣言は BattleVATBake.h）
BattleStanceShot | 姿勢の撮影試験（UBattleLeaderRuntime の一部）
BattleScopeShot | 構え・スコープの撮影試験（UBattleLeaderRuntime の一部）
BattleAudio | 効果音（音だけ。銃声・着弾・かすめる弾・爆発・発射・砲弾の飛来・装填/構え・拠点と勝敗の合図。素材は Docs/ASSET_SOURCES_AUDIO_FX.md。`-BattleSFX=0` で止まる）
BattleEffects | 爆発の効果（表示だけ。Lyra の Niagara。`-BattleFX=0` で止まる。着弾の効果は未実装）
BattleFXCheck | 効果音・エフェクトの確認（試験だけ。`-BattleFXCheck`、Setup/Test-SoundFX.ps1。止めた戦闘の前で爆発を撮影し、続けて射撃・着弾・爆発・発射・合図の音を出して、スピーカーを鳴らさずに音の混ざりを WAV に録る）
BattleMuzzleFlash | 銃口の閃光（表示だけ。発光する楕円体の使い回し）
BattleState | 正規状態：兵・部隊の記録、命令、弾薬、被害、調整値 UBattleTuning、兵科の差
BattleSupport | 旧アリーナの支援砲撃（UBattleMatch の一部）
BattleSuppression | 制圧の値と効果
BattleSoldierFire | 個人AI `-BattleOwnTarget`（試験切替）：小隊の目標を撃てない兵が、自分で見ている敵兵を撃つ（UBattleLeaderRuntime の一部）
BattleSoldierWatch | 個人AI `-BattleSoldierWatch`：止まっていて撃つ相手のいない兵が、知っている最も近い敵へ、無ければ小隊で分けた前方へ向く（視野120度。UBattleLeaderRuntime の一部）
BattleThreatCover | 個人AI段階A `-BattleThreatCover`：兵ごとの脅威（自分の目撃4秒＋撃たれた方向）、全員から隠れる遮蔽、見破られた遮蔽を出て探し直す（UBattleLeaderRuntime の一部）
BattleSoldierMove | 個人AI段階B1 `-BattleSoldierMove`：区画の中で、隠れている兵が近くのもっと良い遮蔽を採点して自分で移る（UBattleLeaderRuntime の一部）
BattleSoldierOrder | プレイヤーが兵1人に出す移動の命令：命令と歩き（UBattleLeaderRuntime）、Ctrl＋クリックの選択と右クリックの命令（ABattleController）、印（ABattleHUD）、自動試験 `-BattleSoldierOrderTest`
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
BattleMapI | マップI（区画を前提にした設計）の表：50mの区画 20×14 の種類 `Kind`、道路 `Roads`、川・橋・浅瀬の位置、家 `Houses`・丘 `Hills`、高さの表 `MakeHeights`、拠点 `Objectives`。配置は `BattleMapLayoutI.inl`（生成と共有）
BattleSectorAudit | 区画命令の検証 `-BattleSectorAudit`（区画のあるマップ、描画なし）：全区画へ制圧命令を出して受け付けを数え、8部隊の到着位置が区画内か、240秒後に維持へ移ったかを報告する。規則は変えない
BattleSectors | マップHの区画の表（Codex）：50mの区画 20×14 の種類 `Kind`、位置から区画 `Cell`・区画の中心 `Center`、川の線 `RiverX`、家・丘の一覧。区画の種類をマップごとに引くのは `BattleFieldLayout::SectorKind`（H・I）
BattlePlayerPath | プレイヤーの入力の補助：命令先の点（2階は壁越しでも指せる、Alt で1階）`PickOrderPoint`、右ドラッグで引いた道筋の命令 `PlayerPathOrder`、選んだ部隊の道筋の予測表示 `DrawOrderWays`／`PlayerRoutePreview`、壁の乗り越え `TryVault`、作戦カメラの速さ `PanSpeed`（ABattleController・UBattleMatch・ABattleHUD の一部）
BattleRoutes | 通り道：道路網のあるマップ（F）で、遠い目的地へ交差点を順に指示する。本拠地へ向かう部隊は観測した敵の少ない側面路を選ぶ（UBattleMatch の一部）
BattleStateTrace | 段階2の切り分け（`Docs/design-divergence-trace.md`）：`-BattleStateTrace=<N>` で N フレームごとに状態のかたまり（位置・体力・遮蔽・認識・計画・試合・射撃）の要約を `BATTLE_STATE_TRACE` に書く。読むだけで試合は変えない。比べるのは `Setup/Find-Divergence.py`（UBattleLeaderRuntime の一部）
BattleStrategy | 戦略層（`Docs/design-strategy-layer.md`、試験切替 `-BattleSituationLog`・`-BattleStrategy`、既定0）：戦線＝拠点のボロノイ分割＋両本拠地の150m。状況図 `BuildSituation`（自軍の戦力、知っている敵＝見えている・偵察の報告・記憶を90秒で軽く、拠点を最後に見た時刻、状態 不明／敵優勢／拮抗／自軍優勢）、評価用の記録 `BATTLE_SITUATION`（本当の敵の数は記録の中だけ）、段階 S2 の集中先 `UpdateStrategy`（局面と時刻は `FBattleAreaState::Strategy*` に保存）（UBattleMatch の一部）
BattleMapJ | マップJ（11-22、`Docs/design-map-j-terrain.md`）の区画の表：山 `M`（通れない・見通しを遮る）・湖 `L`（通れない）と、ビル・集合住宅・教会・工場・廃墟・駐車場・公園・商店の区画。区画ごとの配置の候補と8通りの向き（`Variant`／`Mirror`）、入れる家の位置（`HouseOf`）、拠点・丘・道。配置は `BattleMapLayoutJ.inl`、生成は `BattleMapCommandlet`（`-WideField=J`、`Setup/Initialize-MapJ.ps1`）
BattleMapK | マップK（11-22、`Docs/design-map-k.md`）の区画の表：密な小さい市街地（街は列4〜15・行3〜10、外側は山）、道 `Roads`、拠点6つ、入れる家 `HouseOf`。配置はマップJ の `BattleMapLayoutJ.inl` を名前空間だけ差し替えて使い、K だけの高い塀・追加の遮蔽は `bDenseCover`、道のトラックは `BattleMapCommandlet`（`-WideField=K`、`Setup/Initialize-MapK.ps1`、区画図 `Setup/Export-MapK.py`）
BattleSectorView | 作戦画面の表示だけ（規則・AIは読まない）：自軍の視界の格子 `FBattleFogGrid`（12.5m、自軍の兵から静的な物体への光線）と区画ごとの状態 `BattleSectorView::Gather`（自軍の人数・観測した敵・視界の割合・拠点・命令中の部隊・遮蔽位置の数）
BattleSectorRoute | 区画経由の移動 `-BattleSectorRoute`・側面 `-BattleSectorFlank`・前線 `-BattleSectorFront`（区画のあるマップ、AIの歩兵）：道路網の次の交差点の代わりに、その周りで遮蔽の多い区画を制圧・維持しながら進む。側面は観測した敵の正面を避けた区画、前線は自軍の区画のうち敵に接する区画（UBattleMatch の一部）
BattleSectors | マップHの50m区画の格子（20×14）：区画の番号と中心、区画の種類の表、川・橋・浅瀬の位置
BattleFrontLine | Surprise bridge waypoints: UpdateSurpriseBridge, BattleSurpriseRoute::Bridge / Intersects; fixed H/I map geometry, entrance / exit / edge goal, retained on same-time restore

BattleWeaponOrders | 命令・防衛範囲・支援対象・区画任務の交戦許可と武器/装甲AIの試験切替
BattleComposition | 編成AIの兵種定義・解決済み入力・診断
BattleCompositionPlanner | 編成AIの純粋な候補選択・貯蓄遷移・状態検証。整数比較、乱数なし
BattleCompositionRuntime | 編成AIと観測・収入窓・既存購入実行の接続、設定キー
BattleCompositionState | CombatStateVersion=4で保存する編成AIの陣営状態
BattleCompositionSave | 編成AI保存の版・陣営・時刻・参照先検証
BattleCompositionSmoke | 編成AIの選択・貯蓄・保存往復のスモーク検査（UBattleLeaderRuntimeの一部）
BattleSectorSight | 区画内・区画間の戦い（試験切替 `-BattleSectorSight`、既定0、`Docs/design-sector-combat.md`）：区画のあるマップで、通常小隊は別の区画（20m より遠い）の敵を探す光線を隊長だけが出し、隊員は隊長が今見ている敵を発射直前の視線判定で撃つ（UBattleLeaderRuntime の一部。走査の側は `UBattlePerception::Update` の `SkipCross`）
