# コード索引（どこに何があるか）

`Setup/Export-CodeMap.py` が生成する。**手で編集しない**。機能の表とモジュールの説明は `Setup/CodeMap-Features.md` に書き、作り直す。
実装を探すときは、まずこの表で入口を決め、関数の行は [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) を Grep で引く（全文は読まない）。
行番号は生成時点のもの。ずれていたら作り直す。起動引数は [SWITCHES.md](SWITCHES.md)。

対象は `Game/FutureBattle/Source/FutureBattle/Prototype/`（182 モジュール、48902 行）。同じ階層の `Variant_*` と `FutureBattle*.cpp/.h` は UE テンプレート由来で、ゲームのルールは置いていない。

## 毎フレームの流れ（どこから呼ばれるか）

入口は `ABattleGameMode::Tick` (BattleGameMode.cpp:169) 。ここで2つの経路に分かれる。

- **混成（隊長Actor＋Mass部下。広域・エリア戦の通常経路）**：`UBattleLeaderRuntime::Step` (BattleLeaderRuntime.cpp:1733) がすべてを回す。順に、試合の進行 `UBattleMatch::Update` (BattleMatch.cpp:152) → 部隊の判断 `UBattleSquadTactics::Step` (BattleSquadTactics.cpp:107) → 認識 `UBattlePerception::Update` (BattlePerception.cpp:101) → 隊長の判断 `UBattleLeaderRuntime::Think` (BattleLeaderRuntime.cpp:667) → 兵ごとの移動・射撃 `UBattleLeaderRuntime::Follow` (BattleLeaderRuntime.cpp:930) → 表示 `UBattleLeaderRuntime::RefreshDisplay` (BattleLeaderRuntime.cpp:1714) 。
- **旧全Actor（80m×60m の試作アリーナ）**：GameMode が直接 `UBattleMatch::Update` (BattleMatch.cpp:152) 、 `UBattleSquadTactics::Step` (BattleSquadTactics.cpp:107) 、 `ABattleGameMode::RefreshVisibility` (BattleGameMode.cpp:209) 、 `UBattleSquadAI::Update` (BattleSquadAI.cpp:33) を呼ぶ。兵ごとの実行は `ABattleAIController::Tick` (BattleAIController.cpp:153) 。

エリア戦のときは `UBattleMatch::Update` (BattleMatch.cpp:152) の中で `UBattleMatch::UpdateArea` (BattleArea.cpp:497) を毎フレーム、 `UBattleMatch::UpdateAreaHiring` (BattleArea.cpp:366) と `UBattleMatch::UpdateEnemyOrders` (BattleMatch.cpp:285) を5秒ごとに呼ぶ。起動時の組み立て（どのモードか、どの試験か）は `ABattleGameMode::BeginPlay` (BattleGameMode.cpp:51) と `UBattleLeaderRuntime::Start` (BattleLeaderRuntime.cpp:72) 。

`UBattleLeaderRuntime` と `UBattleMatch` と `UBattleSquadTactics` と `UBattleSquadTrial` は、1つのクラスを複数の .cpp に分けて書いている。クラス名でファイルを決めつけず、下の表か [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) で引く。

## 機能 → 入口

| 機能 | 入口（ここから読む） | 関連 |
|---|---|---|
| 軍共有の既知敵への射撃（段階1） | `UBattlePerception::BuildTeamKnownTargets` (BattleKnownTargets.cpp:16) 、 `UBattleLeaderRuntime::SelectKnownFire` (BattleKnownTargetFire.cpp:102) 、 `UBattleLeaderRuntime::RunKnownTargetFireTrial` (BattleKnownTargetFireTrial.cpp:10) | `-BattleKnownTargetFire=1`（既定0）。通常射撃と既存の覗き、SpotTrackの限定例外、光線予算。移動追加なし |
| 覗き0発の診断・固定場面 | `UBattleLeaderRuntime::RecordPeekFrame` (BattlePeekDiagnostics.cpp:10) 、 `UBattleLeaderRuntime::RunCoverPeekTrial` (BattleCoverPeekTrial.cpp:14) | 記録だけの `peekwhy_*`。`Test-CoverPeek.ps1` は目撃済み／見失いを検査、未発見は記録のみ |
| 編成AI・候補選択と貯蓄購入 | `UBattleMatch::UpdateCompositionHiring` (BattleCompositionRuntime.cpp:93) 、 `BattleComposition::Decide` (BattleCompositionPlanner.cpp:13) | Planner/PlannerTeams は既定0。ONは不足・緊急性で選択し貯蓄／購入。検査は `UBattleLeaderRuntime::RunCompositionSmoke` (BattleCompositionSmoke.cpp:10) |
| 盾被弾制圧・群の兵種別戦力（試験） | `UBattleLeaderRuntime::NoteUnderFire` (BattleLeaderRuntime.cpp:2105) 、 `UBattleState::AdvanceShields` (BattleState.cpp:410) 、 `UBattleMatch::GroupPower` (BattleGroups.cpp:708) 、 `UBattleMatch::GroupPointPower` (BattleGroups.cpp:716) | `BattleExoShieldSuppression` / `BattleGroupStrength` は既定0。観測兵種は `UBattlePerception::Update` (BattlePerception.cpp:101) で取得、重みはUBattleTuning |
| 間接射撃の試験切替 | `UBattleMatch::ArtilleryHomeObserved` (BattleAreaIntel.cpp:105) 、 `UBattleMatch::AreaArtilleryClusters` (BattleArtillery.cpp:456) 、 `UBattleLeaderRuntime::FireArtillery` (BattleArtillery.cpp:701) 、 `UBattleLeaderRuntime::ArtilleryBlastTransmission` (BattleArtillery.cpp:797) | 自走砲弾は `UBattleState::UseSPGAmmo` (BattleState.cpp:683) / `UBattleState::RefillSPGAmmo` (BattleState.cpp:691) 。4切替は既定0 |
| 歩兵の装備（副武装・手りゅう弾・近接・精鋭小隊・新しい武器種。試験） | `UBattleLeaderRuntime::StepArms` (BattleArms.cpp:225) 、 `UBattleLeaderRuntime::ThrowGrenade` (BattleArms.cpp:666) 、 `UBattleLeaderRuntime::FireLauncherRound` (BattleArms.cpp:692) 、 `UBattleLeaderRuntime::MeleeStrike` (BattleArms.cpp:774) 、 `UBattleLeaderRuntime::UseSidearm` (BattleArms.cpp:825) 、 `UBattleLeaderRuntime::ArmsRoleTarget` (BattleArms.cpp:844) | 切替は `BattleArms`（`-BattleSidearms`・`-BattleGrenades`・`-BattleMelee`・`-BattleElite`・`-BattleAreaEliteKeep`、既定0）。武器の値は `BattleWeaponDef`、精鋭小隊は `UBattleLeaderRuntime::IssueEliteKit` (BattleArms.cpp:197) と `FBattleSquadState::Grade`。設計は Docs/design-infantry-arms.md |
| 区画の任務に合わせた躍進・守る側の対応・援護射撃（試験） | `UBattleLeaderRuntime::StepArmsRush` (BattleArmsSector.cpp:100) 、 `UBattleLeaderRuntime::ArmsRushTarget` (BattleArmsSector.cpp:33) 、 `UBattleLeaderRuntime::StartArmsRetreat` (BattleArmsSector.cpp:60) 、 `UBattleLeaderRuntime::StepArmsCoverCalls` (BattleArmsSector.cpp:274) 、 `UBattleLeaderRuntime::ArmsCoverFireAt` (BattleArmsSector.cpp:340) | `-BattleArmsSectorTask`／`-BattleArmsRushWatch`／`-BattleArmsCoverFire`（Docs/design-infantry-arms.md） |
| 詰め寄り・突撃（歩兵の装備の続き。試験） | `UBattleLeaderRuntime::StartAssault` (BattleArms.cpp:442) 、 `UBattleLeaderRuntime::StartCharge` (BattleArms.cpp:483) 、 `UBattleLeaderRuntime::StepCharge` (BattleArms.cpp:557) | 切替は `-BattleArmsAssault=1/2`・`-BattleArmsCharge=1`（既定0）。どちらも `CoverApproaches` の印（`bAssault`・`bCharge`）で動き、歩きは `UBattleLeaderRuntime::StepCoverApproach` (BattleCoverAction.cpp:503) 。設計は Docs/design-infantry-arms.md |
| 正規状態・UnitID・部隊 | `UBattleState::RegisterUnit` (BattleState.cpp:99) 、 `UBattleState::CreateSquad` (BattleState.cpp:168) 、 `UBattleState::ApplyDamage` (BattleState.cpp:455) | 構造体は `BattleState.h`（FBattleUnitState、FBattleSquadState）。調整値は同じ .h の `UBattleTuning`（ini で上書き） |
| 兵科（歩兵・重火器・偵察・対戦車・戦車）の差 | `BattleSquadType::Members` (BattleState.cpp:601) から `BattleSquadType::Weapon` (BattleState.cpp:652) までの一群、 `UBattleState::SetSquadType` (BattleState.cpp:657) | 射程は `BattleRanges::Get` (BattleRanges.cpp:7) |
| 命令（移動・攻撃移動・防御・待機） | 入力 `ABattleController::IssueOrderAt` (BattleController.cpp:984) → 状態 `UBattleState::SetSquadOrder` (BattleState.cpp:199) → 混成 `UBattleLeaderRuntime::IssueOrder` (BattleLeaderRuntime.cpp:559) | 委任は `UBattleState::SetSquadDelegated` (BattleState.cpp:186) 、 `UBattleMatch::SetAllyDelegated` (BattleMatch.cpp:144) |
| 入力・RTS/TPS 切替・ゲームパッド | `ABattleController::SetupInputComponent` (BattleController.cpp:113) 、 `ABattleController::InputKey` (BattleController.cpp:630) 、 `ABattleController::ToggleMode` (BattleController.cpp:531) 、 `ABattleController::ControlUnit` (BattleController.cpp:500) | ゲームパッドは `ABattleController::UpdateGamepad` (BattleController.cpp:1632) 。隊長交代は `UBattleState::HandOverLeader` (BattleState.cpp:565) |
| HUD・ミニマップ・部隊一覧・通知 | `ABattleHUD::DrawHUD` (BattleHUD.cpp:33) 、 `ABattleHUD::DrawMinimap` (BattleHUD.cpp:664) 、 `ABattleHUD::DrawSquadList` (BattleHUD.cpp:765) 、 `ABattleHUD::DrawNoticeLog` (BattleHUD.cpp:884) | 通知の中身は `ABattleController::UpdateNoticeLog` (BattleController.cpp:1360) 、 `ABattleController::PushNotice` (BattleController.cpp:1344) 。日本語化は `BattleUIText::Ja` (BattleUIText.cpp:80) 。兵種アイコン・敵の最終確認地点は `ABattleHUD::DrawSquadIcons` (BattleHUD.cpp:1124) 。射程の円は `ABattleHUD::DrawRangeRings` (BattleHUD.cpp:972) ・ミニマップは `ABattleHUD::DrawMinimapRangeRings` (BattleHUD.cpp:1067) 、敵の円は L キー（ `ABattleController::ToggleEnemyRanges` (BattleController.cpp:948) ）（射程は `BattleRanges::Engage` (BattleRanges.cpp:35) ）。被弾方向は `ABattleHUD::DrawHitDirection` (BattleHUD.cpp:1245) （記録は `UBattleState::ApplyDamageFrom` (BattleState.cpp:496) ）、画面外マーカーは `ABattleHUD::DrawOffscreenMarkers` (BattleHUD.cpp:1283) 、撃破ログは `ABattleHUD::DrawKillLog` (BattleHUD.cpp:1350) （記録は `UBattleState::NoteKill` (BattleState.cpp:503) ）。作戦画面の戦場の霧・区画の格子と区画の状態は `ABattleHUD::DrawFieldOverlay` (BattleHUD.cpp:1444) 、 `ABattleHUD::DrawSectorPanel` (BattleHUD.cpp:1634) （計算は `BattleSectorView`） |
| 認識（見えた敵の記憶） | `UBattlePerception::Update` (BattlePerception.cpp:101) 、 `UBattlePerception::GetContacts` (BattlePerception.cpp:44) | 視線は `UBattleLeaderRuntime::CanSee` (BattleLeaderRuntime.cpp:355) 、 `UBattleLeaderRuntime::TraceSight` (BattleLeaderRuntime.cpp:386) 。試験用の視界切替は `BattleSightExperiment` |
| 隊長の判断と兵の実行（混成） | `UBattleLeaderRuntime::Think` (BattleLeaderRuntime.cpp:667) 、 `UBattleLeaderRuntime::Follow` (BattleLeaderRuntime.cpp:930) 、 `UBattleLeaderRuntime::Fire` (BattleLeaderRuntime.cpp:1428) | 兵単位の実行は `FBattleMixedDriver::Tick` (BattleMixedDriver.cpp:126) 、 `FBattleMixedDriver::TryTacticalAction` (BattleMixedDriver.cpp:241) |
| 移動・経路・混雑 | `UBattleLeaderRuntime::StableMove` (BattleStableMovement.cpp:536) 、 `UBattleLeaderRuntime::BuildMovementPlan` (BattleStableMovement.cpp:99) | 到着地点の重なりは `UBattleLeaderRuntime::ReassignOccupiedArrival` (BattleContactMovement.cpp:16) 。目的地の補正は `BattleNavigation::ResolveGoal` (BattleNavigation.cpp:7) |
| 遮蔽（場所の予約） | `UBattleCover::BuildSlots` (BattleCover.cpp:28) 、 `UBattleCover::Reserve` (BattleCover.cpp:280) | 露出の計算は `BattleTactics::ExposureAt` (BattleTactics.cpp:357) |
| 体のアニメーション | `ABattleUnit::UpdateBodyAnim` (BattleUnit.cpp:499) 、 `FBattleBodyAnimProxy::Evaluate` (BattleBodyAnim.cpp:228) | 部下は `UBattleLeaderRuntime::FollowerClip` (BattleStance.cpp:123) 、 `UBattleLeaderRuntime::StepBodyTimers` (BattleStance.cpp:152) |
| 効果音・エフェクト（表示と音だけ） | `BattleAudio::Shot` (BattleAudio.cpp:199) 、 `BattleAudio::Explosion` (BattleAudio.cpp:245) 、 `BattleAudio::TickPlayer` (BattleAudio.cpp:302) 、 `BattleEffects::Explosion` (BattleEffects.cpp:92) | 呼び出しは `UBattleLeaderRuntime::Fire` (BattleLeaderRuntime.cpp:1428) （射撃・着弾・かすめる弾）、 `UBattleLeaderRuntime::Explode` (BattleAntiTank.cpp:8) （対戦車弾・戦車砲弾・外骨格兵の発射筒の爆発）、 `UBattleLeaderRuntime::FireArtillery` (BattleArtillery.cpp:701) ・ `UBattleLeaderRuntime::StepArtilleryShells` (BattleArtillery.cpp:781) ・ `UBattleLeaderRuntime::ArtilleryImpact` (BattleArtillery.cpp:807) （砲撃）、 `ABattleController::PlayerTick` (BattleController.cpp:1158) （作戦画面の聞く位置・装填/構え/スコープ・拠点と勝敗の合図）。`-BattleSFX=0`／`-BattleFX=0` で止まる。素材と出典は `Docs/ASSET_SOURCES_AUDIO_FX.md` |
| 死体の消滅（表示だけ） | `UBattleLeaderRuntime::StepCorpses` (BattleCorpses.cpp:46) 、 `UBattleLeaderRuntime::HideCorpse` (BattleCorpses.cpp:73) | 登録は OnDeath の `UBattleLeaderRuntime::AddCorpse` (BattleCorpses.cpp:39) |
| 死体の姿勢（死亡アニメ・遠景VATとの切替） | `UBattleLeaderRuntime::DeathClip` (BattleCorpses.cpp:84) 、 `ABattleVATDisplay::ShowNear` (BattleVATDisplay.cpp:221) 、 `ABattleVATDisplay::Select` (BattleVATDisplay.cpp:293) | 死ぬと遠景表示から外れて死亡クリップを流し、隠れるまで遠景に戻らない。クリップの後は物理で倒れて止まる（ `BattleRagdoll::Start` (BattleRagdoll.cpp:13) 、 `BattleRagdoll::Settle` (BattleRagdoll.cpp:36) 。部下は `UBattleLeaderRuntime::StepBodyTimers` (BattleStance.cpp:152) 、隊長は `ABattleUnit::Tick` (BattleUnit.cpp:212) ）。見た目の確認は `UBattleLeaderRuntime::RunDeathShot` (BattleDeathShot.cpp:22)（Setup/Test-DeathShot.ps1） |
| ブラインドファイア | `UBattleLeaderRuntime::BlindFireOrigin` (BattleBlindFire.cpp:53) 、 `UBattleLeaderRuntime::Fire` (BattleLeaderRuntime.cpp:1428) | AIの試験は `UBattleLeaderRuntime::TryAIBlindFire` (BattleBlindFire.cpp:137) |
| 遮蔽の動作（接近・のぞき撃ち） | `UBattleLeaderRuntime::RequestCoverAction` (BattleCoverAction.cpp:203) 、 `UBattleLeaderRuntime::StepCoverAction` (BattleCoverAction.cpp:645) | 切替は `BattleTactics::CoverActionsEnabled` (BattleTactics.cpp:306) ほか |
| プレイヤーの壁の遮蔽（壁に押し当てる・C） | `UBattleLeaderRuntime::StepPlayerWallCover` (BattleWallCover.cpp:90) 、 `UBattleLeaderRuntime::TryWallCover` (BattleWallCover.cpp:42) | 呼び出しは `ABattleController::PlayerTick` (BattleController.cpp:1158) 。中身は普通の遮蔽の動作（決まった場所なし `FreeCoverSlot`）。切替 `-BattlePlayerWallCover`（既定1、AI には効かない） |
| 姿勢（立ち・しゃがみ） | `UBattleLeaderRuntime::SetStance` (BattleStance.cpp:77) 、 `UBattleLeaderRuntime::UpdateAIStance` (BattleStance.cpp:291) | 数値は `BattleStance::EyeOffset` (BattleStance.cpp:34) ほか |
| 制圧（撃たれて動けない） | `UBattleLeaderRuntime::AddSuppression` (BattleSuppression.cpp:48) 、 `UBattleLeaderRuntime::IsPinned` (BattleSuppression.cpp:36) | 調整値は `BattleSuppression::Tuning` (BattleSuppression.cpp:28) |
| 兵ごとの脅威と遮蔽の取り直し（個人AI段階A） | `UBattleLeaderRuntime::GatherThreats` (BattleThreatCover.cpp:34) 、 `UBattleLeaderRuntime::CheckCoverThreats` (BattleThreatCover.cpp:134) | 試験切替 `-BattleThreatCover`（既定0）。遮蔽を選ぶ側は `UBattleLeaderRuntime::RequestCoverAction` (BattleCoverAction.cpp:203) の2段の候補 |
| 止まった兵の見張りの向き（個人AI） | `UBattleLeaderRuntime::SoldierWatchDirection` (BattleSoldierWatch.cpp:74) | `-BattleSoldierWatch`（既定1、11-22 ユーザー決定。=0 で切る）。`Follow` の向きの分岐で `-BattleAttackFocus` の後ろ。視野は前方120度 |
| 歩いている兵の視線（個人AI、近い戦い） | `UBattleLeaderRuntime::WalkLookDirection` (BattleSoldierWatch.cpp:98) 、 `UBattleLeaderRuntime::KnownEnemyDirection` (BattleSoldierWatch.cpp:32) | 試験切替 `-BattleWalkLook`（既定0）。体は進む向きのまま、視線（`Sight` の向き、`WalkLooks`）だけを知っている敵か小隊で分けた向きへ振る。`Follow` の向きの分岐の最後。診断 `hitfov_*`（視野の外から撃たれた兵の状態）は `DiagnoseHitSight` |
| 兵が自分で撃つ相手を選ぶ（個人AI、近い戦い） | `UBattleLeaderRuntime::OwnTarget` (BattleSoldierFire.cpp:39) | 試験切替 `-BattleOwnTarget=1/2`（既定0）。小隊の目標を撃てない兵が、自分で見ている交戦距離内の敵兵を撃つ（撃たれた方向の敵が先、次に近い敵）。`Follow` の `CanEngage` の直後で選び、狙いと射撃だけを変える。値2は、撃つ相手がいないとき撃たれた方向へ2秒向く（`-BattleHitFace` と同じ分岐）。数は `own_target_*` |
| 撃ち返す兵が遮蔽に入る（個人AI） | `UBattleLeaderRuntime::OwnTargetCover` (BattleSoldierFire.cpp:30) | 試験切替 `-BattleOwnTargetCover=1/2`（既定0、`-BattleOwnTarget` が要る）。自分で選んだ相手を撃つ兵が、その相手に対する近くの遮蔽を頼む（`Follow` の遮蔽探しの直前で `RequestCoverAction` を、その相手を目標にして呼ぶ）。値1は歩いていない兵だけ、値2は歩いている兵も相手が50m以内なら止まって入る。近く（約6m）に無ければ、歩いていない兵は15m以内の遮蔽へ歩く（`StartCoverApproach`、`OwnCoverWalks`、数は `own_cover_walk`）。数は `own_cover_*` と `own_target_open_{standing,walking}_frames` |
| その場のしゃがみ繰り返しを減らす（個人AI） | `BattleStance::Steady` (BattleStance.cpp:65) | `-BattleStanceSteady`（**既定1**、2026-11-24 ユーザー決定。`=0` で元に戻る）。開けた所で、しゃがんで敵が見えなくなり立ち上がった場所（`CrouchBlindAt`、2.5m以内）では立ったまま戦う（`UpdateAIStance`、数は `steady_stay_standing_frames`）。遮蔽の覗きが一発も撃たずに終わったら、次は0.8秒でなく3秒しゃがんで待つ（`QuietPeeks`、`CoverHideSeconds`、数は `steady_long_hide`）。評価用に、通り道の小隊の兵を歩き／戦闘なしで停止／戦闘で停止（低い・立ち・立ちで敵が見えない）に分けて数える（`route_*_frames`、`Follow`） |
| 区画の中で兵が遮蔽を移る（個人AI段階B1） | `UBattleLeaderRuntime::StepSoldierMove` (BattleSoldierMove.cpp:48) 、 `UBattleLeaderRuntime::RateCoverPlace` (BattleSoldierMove.cpp:19) | 試験切替 `-BattleSoldierMove`（既定0）。呼び出しは `UBattleLeaderRuntime::StepCoverAction` (BattleCoverAction.cpp:645) の隠れている間（1秒に1回・1フレーム4人まで） |
| 個人AIをプレイヤーの周りだけで動かす | `UBattleLeaderRuntime::SoldierAIFocus` (BattleSoldierMove.cpp:111) | 試験切替 `-BattleSoldierAIFocus=<m>`（既定0＝全員）、`-BattleSoldierAIFocusAt=X,Y`。段階A（`UBattleLeaderRuntime::CheckCoverThreats` (BattleThreatCover.cpp:134) ほか）とB1（`UBattleLeaderRuntime::StepSoldierMove` (BattleSoldierMove.cpp:48)）の入口で見る |
| プレイヤーが兵1人に出す移動の命令（Ctrl＋クリックで選ぶ・右クリックで移動先・Hで戻す） | `UBattleLeaderRuntime::IssueSoldierOrder` (BattleSoldierOrder.cpp:20) 、 `UBattleLeaderRuntime::StepSoldierOrder` (BattleSoldierOrder.cpp:86) 、 `ABattleController::SelectSoldierAt` (BattleSoldierOrder.cpp:142) 、直接操作では `ABattleController::CommandSoldierFromView` (BattleSoldierOrder.cpp:173) （G） | 兵の状態 `bSoldierOrder`。`UBattleLeaderRuntime::Follow` (BattleLeaderRuntime.cpp:930) の `bOwnOrder`。自動試験 `-BattleSoldierOrderTest`（`UBattleLeaderRuntime::StepSoldierOrderTest` (BattleSoldierOrder.cpp:245)） |
| 射撃の散らばり | `BattleTactics::SpreadDegrees` (BattleTactics.cpp:312) 、 `BattleTactics::ShotDirection` (BattleTactics.cpp:320) | 照準の遅れは `UBattleState::UpdateAim` (BattleState.cpp:330) 。プレイヤーの直接操作だけの倍率は `ABattleUnit::PlayerSpreadScale` (BattleUnit.cpp:164) |
| 部隊の戦術（撤退・集合・援護・迂回） | `UBattleSquadTactics::Step` (BattleSquadTactics.cpp:107) 、 `UBattleSquadTactics::Evaluate` (BattleSquadRetreat.cpp:136) 、 `UBattleSquadTactics::Apply` (BattleSquadRetreat.cpp:11) | 撤退 `UBattleSquadTactics::RetreatGoal` (BattleSquadRetreat.cpp:55) 、援護 `UBattleSquadTactics::AcceptSupport` (BattleSquadSupport.cpp:75) 、迂回 `UBattleSquadTactics::TryFlank` (BattleSquadFlank.cpp:47) |
| 陣営全体の判断（援護・迂回の割り振り） | `UBattleTeamAI::Step` (BattleTeamAI.cpp:16) | |
| 試合の進行・勝敗・制限時間 | `UBattleMatch::InitializeMatch` (BattleMatch.cpp:15) 、 `UBattleMatch::Update` (BattleMatch.cpp:152) 、 `UBattleMatch::Finish` (BattleMatch.cpp:625) | 拠点の占領は `UBattleMatch::UpdateObjectives` (BattleMatch.cpp:226) |
| 雇用（旧・固定の価格） | `UBattleMatch::HireSquad` (BattleMatch.cpp:56) 、 `UBattleMatch::CurrentSquadPrice` (BattleMatch.cpp:51) | 実体を出すのは `UBattleLeaderRuntime::SpawnPurchasedSquad` (BattleMixedRecruitment.cpp:45) |
| エリア戦の雇用（AI が何を雇うか） | `UBattleMatch::UpdateAreaHiring` (BattleArea.cpp:366) 、 `UBattleMatch::AreaHireNeed` (BattleArea.cpp:630) 、 `UBattleMatch::HireAreaSquad` (BattleArea.cpp:144) | 調整値は `BattleTactics::AreaHire` (BattleTactics.cpp:158) |
| プレイヤーの購入画面（B キー、2026-11-23） | `FBattleBuyPanelLayout::Make` (BattleBuyPanel.cpp:75) 、 `ABattleController::BuyPanelInput` (BattleBuyPanel.cpp:185) 、 `ABattleController::HandleBuyPanelPad` (BattleBuyPanel.cpp:216) 、 `ABattleHUD::DrawBuyPanel` (BattleHUD.cpp:577) | 行を押すと `ABattleController::BuyArea` (BattleController.cpp:260) （数字キーと同じ。買えなければ予約 `UBattleMatch::ReserveHire` (BattleArea.cpp:194) 、列を買うのは `UBattleMatch::UpdateHireQueue` (BattleArea.cpp:328) 、取り消しは `ABattleController::CancelReservationOfType` (BattleBuyPanel.cpp:173) ）。B は `ABattleController::BuyKey` (BattleBuyPanel.cpp:157) （エリア戦以外は支援要請）。画面の確認は `Setup/Test-BuyPanelShot.ps1` |
| 上部バーのボタン列と終了の確認（Esc・F10、2026-11-25） | `FBattleQuitLayout::Make` (BattleQuitMenu.cpp:16) 、 `ABattleController::HandleQuitMenu` (BattleQuitMenu.cpp:78) （ `ABattleController::InputKey` (BattleController.cpp:630) の最初）、 `ABattleHUD::DrawQuitMenu` (BattleHUD.cpp:516) | ボタンの動作は `ABattleController::RunTopTool` (BattleQuitMenu.cpp:58) （キーと同じ関数）。終了は `ABattleController::QuitGameNow` (BattleQuitMenu.cpp:52) 。画面の確認は `Setup/Test-QuitMenuShot.ps1` |
| エリア戦の収入・拠点 HP | `UBattleMatch::UpdateArea` (BattleArea.cpp:497) （収入の支払いもここ）、 `UBattleMatch::DamageBase` (BattleArea.cpp:69) | 調整値は `BattleTactics::AreaIncome` (BattleTactics.cpp:224) 。状態は `BattleAreaState.h` |
| エリア戦の AI 戦略（どこを攻めるか） | `UBattleMatch::UpdateEnemyOrders` (BattleMatch.cpp:285) → `UBattleMatch::UpdateTeamOrders` (BattleMatch.cpp:373) 、 `UBattleMatch::AreaStrategyTarget` (BattleMatch.cpp:303) 、 `UBattleMatch::AreaIntent` (BattleArea.cpp:658) | 調整値は `BattleTactics::AreaStrategy` (BattleTactics.cpp:176) 。前線は `UBattleMatch::AreaFronts` (BattleArea.cpp:710) 。戦略層の状況図（`Docs/design-strategy-layer.md`）は `UBattleMatch::BuildSituation` (BattleStrategy.cpp:125) 。集中先は前線の計画が決め `UBattleMatch::StrategyFocusPoint` (BattleStrategy.cpp:263) で読む（ `FAreaIntent::FocusPoint` で S6 と攻撃群へ。旧 S2〜S4 は 11-25 に削除） |
| 偵察・情報（見えていない区域の扱い） | `UBattleMatch::UpdateScoutReports` (BattleScoutReport.cpp:49) 、 `UBattleMatch::AreaIntelView` (BattleAreaIntel.cpp:20) | 偵察兵の配置は `UBattleMatch::AreaScoutPost` (BattleArea.cpp:750) 。昇格は `UBattleLeaderRuntime::PromoteScout` (BattleMixedRecruitment.cpp:127) |
| 補給（弾薬） | `UBattleMatch::Resupply` (BattleMatch.cpp:579) 、 `BattleTactics::Supply` (BattleTactics.cpp:135) | 往復時間の記録（評価用）は `UBattleSquadTactics::SupplyTripEnd` (BattleSquadTactics.cpp:81) 。補給・再編・襲撃・交戦の後に任務へ戻るのは `UBattleSquadTactics::ResumeMission` (BattleSquadRetreat.cpp:32) （区画属性を保つ、B06）。エリア戦の本拠地・味方の拠点での弾の補充は `UBattleMatch::UpdateArea` (BattleArea.cpp:497) 、直接操作中に部下から弾を受け取るのは `ABattleController::UpdatePlayerAmmo` (BattleController.cpp:1101) |
| 補充（倒れた部下の補充） | `UBattleLeaderRuntime::ReplaceFollower` (BattleMixedRecruitment.cpp:13) 、 `UBattleState::ReplaceDeadFollower` (BattleState.cpp:114) | |
| 戦果点 | `UBattleMatch::UpdateWarScore` (BattleWarScore.cpp:52) 、 `UBattleMatch::WarScoreBreakdown` (BattleWarScore.cpp:75) | 調整値は `BattleWarScore::Tuning` (BattleWarScore.cpp:19) |
| 支援砲撃（旧アリーナ） | `UBattleMatch::RequestSupport` (BattleSupport.cpp:8) 、 `UBattleMatch::UpdateSupport` (BattleSupport.cpp:45) | 混成には未接続 |
| 車両・戦車砲・対戦車 | `UBattleLeaderRuntime::ConfigureVehicle` (BattleVehicle.cpp:42) 、 `UBattleLeaderRuntime::SteerVehicle` (BattleVehicle.cpp:109) 、 `UBattleLeaderRuntime::FireTankGun` (BattleTankGun.cpp:134) | 爆発は `UBattleLeaderRuntime::Explode` (BattleAntiTank.cpp:8) |
| 武器の命令・任務による交戦許可 | `BattleTactics::OrderAllowsTarget` (BattleWeaponOrders.cpp:20) 、 `UBattleLeaderRuntime::WeaponOrderAllows` (BattleWeaponOrders.cpp:29) | 副武器・間接射撃の試験は BattleWeaponOrderGate、装甲AIの候補制限は BattleArmourSectorGate（いずれも既定0） |
| 表示（混成の体・遠距離 VAT） | `UBattleLeaderRuntime::CreateBody` (BattleMixedBodies.cpp:18) 、 `UBattleLeaderRuntime::RefreshDisplay` (BattleLeaderRuntime.cpp:1714) 、 `ABattleVATDisplay::Tick` (BattleVATDisplay.cpp:316) | 一戦中の保存・読込は 11-25 に削除 |
| 歩兵の体のメッシュ（Quinn／RB01、試験切替 -BattleSoldierBody） | `BattleSoldierBody::LoadMesh` (BattleSoldierBody.cpp:27) 、 `BattleSoldierBody::VATRoot` (BattleSoldierBody.cpp:33) | 隊長は `ABattleUnit::InitializeUnit` (BattleUnit.cpp:94) 、部下は `UBattleLeaderRuntime::CreateBody` (BattleMixedBodies.cpp:18) 、遠景は `ABattleVATDisplay::Initialize` (BattleVATDisplay.cpp:158) 。RB01 の取り込みは Setup/Import-RB01.ps1、VAT は Setup/Test-VATBake.ps1 -RB01 |
| マップの定義・配置 | `BattleFieldLayout.h`（マップごとの表 `Maps()`） | 生成は `UBattleMapCommandlet::Main` (BattleMapCommandlet.cpp:121) （既存の手編集を `-Force` で上書きしない） |
| 通り道（道路網に沿った移動の指示） | `UBattleMatch::RouteStep` (BattleRoutes.cpp:56) 、 `UBattleMatch::RouteFlankSide` (BattleRoutes.cpp:36) | 呼び出しは `UBattleMatch::UpdateTeamOrders` (BattleMatch.cpp:373) 。道路網は `FMapDef::RouteNodes/RouteEdges` |
| 高低差（丘・階段・2階。マップG） | `UBattleLeaderRuntime::TerrainStep` (BattleStableMovement.cpp:62) 、 `UBattleMatch::UpdateTerrainTest` (BattleTerrainTest.cpp:41) | 高さの表は `BattleTerrain.h`（`FHeights::Ground`、家と丘の位置 `HousesG/HillsG`）と `FMapDef::Heights`。形は `BattleMapLayoutG.inl`。隊長はエンジンの歩行、多数兵は表に沿う。自己試験 `Test-WideSight.ps1 -Map G -HeightTest` |
| 地面全体の起伏（高さの格子。マップL） | `BattleTerrain::FHeightField::Height` 、 `BattleMapL::MakeField` | 2.5m 格子の地面の高さ（`FHeights::Field`。他のマップは空で高さ0）。地面の網目と塗りは生成時に `BuildGroundTiles`・`BuildTerrainL`（`BattleMapCommandlet.cpp`）、物を地面に載せるのは同じファイルの `Box`。設計は `Docs/design-terrain-field.md` |
| マップの数値監査（試合なし） | `UBattleLeaderRuntime::RunMapStats` (BattleMapStats.cpp:38) | `Setup/Test-MapStats.ps1`。拠点ごとの持ち主・価値は `FMapDef::ObjectiveOwners/ObjectiveValues` |
| 計測の記録 | `UBattleMetrics::BeginFrame` (BattleMetrics.cpp:21) 、 `UBattleMetrics::Finish` (BattleMetrics.cpp:75) | 段取りは MEASURE.md |
| ネットワーク試作 | `ABattleNetGameMode::BeginPlay` (BattleNetwork.cpp:36) 、 `ABattleNetController::ServerOrder_Implementation` (BattleNetwork.cpp:177) | |
| 基本回帰（BATTLE_SMOKE_RESULT） | `ABattleGameMode::RunSmokeTest` (BattleGameMode.cpp:229) 、 `ABattleGameMode::FinishSmokeTest` (BattleGameMode.cpp:338) | 個別の試験は各 `*Smoke.cpp` の `Run…` |


## モジュール一覧（本体）

行数は .h + .cpp。型は宣言順で先頭8個まで。

| モジュール | 種類 | 行数 | 宣言している型 | 役割 |
|---|---|---|---|---|
| `BattleAIController` | .cpp | 252 |  | 旧全Actor経路の兵の実行（移動・再試行）。混成では使わない |
| `BattleATHunt` | .cpp | 164 |  | 試験切替：対戦車兵の距離、装甲車が見た対戦車兵の排除要請と砲撃要請（目標選びは BattleArtillery の PickArtilleryAim）、装甲車が対戦車兵から離れる距離、装甲車が歩兵の前に出る盾（UBattleMatch の一部） |
| `BattleAnimTasks` | .h/.cpp | 11 + 27 | FBattleAnimTasks | アニメーション更新の並列実行の設定を兵ごとに切り替え、元の設定を覚えておく |
| `BattleAntiTank` | .cpp | 43 |  | 対戦車弾の爆発（UBattleLeaderRuntime の一部） |
| `BattleArea` | .cpp | 781 |  | エリア戦の本体：初期化、収入、拠点、AI の雇用と意図（UBattleMatch の一部） |
| `BattleAreaDepot` | .cpp | 49 |  | 集積所の重み：価値が1より大きい拠点（マップFの集積所）への割り振りの重みを、守る・取り返す・襲うの状況で変える（UBattleMatch の一部） |
| `BattleAreaIntel` | .cpp | 113 |  | エリア戦の情報：観測に基づく区域の見え方と脅威（UBattleMatch の一部） |
| `BattleAreaState` | .h | 76 | FBattleHireReservation, FBattleAreaState | エリア戦の状態（拠点 HP、収入、観測） |
| `BattleArmourAI` | .cpp | 397 |  | 兵種を扱うAI：効かない車両を狙わない、対戦車兵の弾の温存、知っている敵車両、戦車の待機位置、兵種の構成（UBattleLeaderRuntime と UBattleMatch の一部） |
| `BattleArms` | .h/.cpp | 83 + 996 | BattleArms | 歩兵の装備：副武装（拳銃）・手りゅう弾・擲弾銃の弾・近接攻撃・衛生兵の手当て・精鋭小隊の役割と自分の目標、追加の武器種の値（UBattleLeaderRuntime の一部と BattleArms 名前空間） |
| `BattleArmsSector` | .cpp | 356 |  | 区画の任務と歩兵の装備（試験、既定0）：躍進は攻める任務だけ `-BattleArmsSectorTask`、守る側が走り込みを見つけて迎え撃つ・手りゅう弾・下がる・逆襲 `-BattleArmsRushWatch`、躍進への周りの小隊の援護射撃（制圧が効く） `-BattleArmsCoverFire` |
| `BattleArtillery` | .cpp | 1251 |  | 迫撃砲と自走砲：間接射撃の目標選び、着弾、陣地の位置（UBattleLeaderRuntime と UBattleMatch の一部） |
| `BattleAudio` | .h/.cpp | 37 + 365 | BattleAudio | 効果音（音だけ。銃声・着弾・かすめる弾・爆発・発射・砲弾の飛来・装填/構え・拠点と勝敗の合図。素材は Docs/ASSET_SOURCES_AUDIO_FX.md。`-BattleSFX=0` で止まる） |
| `BattleBaseGarrison` | .cpp | 102 |  | 基地の守備隊（試験 `-BattleBaseGarrison=1`、既定0、`Docs/design-base-garrison.md`）：開幕に両軍の本拠地へ専属の小銃小隊2個（`Area.HirePurpose`=102、`IsBaseGarrison`）、基地の範囲に生きた守備隊が居なくなり相手の歩兵が占領し続けると負け（`UpdateBaseGarrison`）。固定場面は BattleBaseGarrisonSmoke |
| `BattleBlindFire` | .h/.cpp | 47 + 145 | BattleBlindFire | 遮蔽の陰からのブラインドファイア（弾の出る位置・AIの試験・アニメ。UBattleLeaderRuntime の一部） |
| `BattleBodyAnim` | .h/.cpp | 129 + 371 | BattleBodyAnim, EClip, FBattleBodyLayer, FBattleBodyAnimProxy, UBattleBodyAnimInstance | 体のアニメーション：隊長の下半身と上半身の重ね・反動・被弾・死亡、部下の伏せと死亡のクリップ |
| `BattleBodyAnimImport` | .cpp | 134 |  | 体のアニメーション（遮蔽・伏せ・しゃがみの向き別）の取り込みと計測（エディタ用。宣言は BattleVATBake.h） |
| `BattleBuyPanel` | .cpp | 362 |  | プレイヤーの購入画面（B キー）：兵種の一覧の配置、クリック・パッドの操作、画面の確認 `-BattleBuyPanelShot`（ABattleController の一部） |
| `BattleComposition` | .h/.cpp | 74 + 127 | BattleComposition, FSettings, FTypeDef, FTypeInput, FInput, FCandidate, FDecision | 編成AIの兵種定義・解決済み入力・診断 |
| `BattleCompositionPlanner` | .cpp | 208 |  | 編成AIの純粋な候補選択・貯蓄遷移・状態検証。整数比較、乱数なし |
| `BattleCompositionRuntime` | .cpp | 193 |  | 編成AIと観測・収入窓・既存購入実行の接続、設定キー |
| `BattleCompositionState` | .h | 36 | FCompositionTeamState | 編成AIの陣営状態 |
| `BattleContactMovement` | .cpp | 162 |  | 停止中の味方との接触・到着地点の重なりの解消（UBattleLeaderRuntime の一部） |
| `BattleController` | .h/.cpp | 387 + 1738 | ABattleCameraPawn, FBattleNoticeEntry, FBattleNoticeLayout, FBattleCommandBarLayout, FBattleQuitLayout, FBattlePlanPanelLayout, FBattleBuyPanelLayout, ABattleController | プレイヤーの入力、RTS/TPS 切替、選択、命令、通知、ゲームパッド |
| `BattleCorpses` | .cpp | 87 |  | 死体の消滅（表示だけ。`-BattleCorpseLifetime=秒`、既定20、0で残す。UBattleLeaderRuntime の一部） |
| `BattleCover` | .h/.cpp | 45 + 303 | FBattleCoverReservation, UBattleCover | 遮蔽の場所の一覧と予約 |
| `BattleCoverAction` | .cpp | 869 |  | 遮蔽への接近とのぞき撃ちの動作（UBattleLeaderRuntime の一部）。撃てる遮蔽の判定 `UBattleLeaderRuntime::CoverFirePoint`（over/lean）と値2の step `CoverStepFirePoint`（試験 `-BattleCoverFirePoint`、既定0） |
| `BattleCoverClips` | .h | 79 | BattleCoverClips, FSet | 遮蔽の状態 → 遮蔽アニメの対応表（データだけ。「アセット参考資料の検討」セッション作） |
| `BattleCoverFire` | .h | 8 | BattleCoverFire | 区画の到着地点が撃てる遮蔽か（over/lean）の判定 `BattleCoverFire::ArrivalFirePoint` の公開（中身は BattleStableMovement.cpp、試験 `-BattleCoverFirePoint`） |
| `BattleEffects` | .h/.cpp | 19 + 103 | BattleEffects | 爆発の効果（表示だけ。Lyra の Niagara。`-BattleFX=0` で止まる。着弾の効果は未実装） |
| `BattleExo` | .cpp | 436 |  | 外骨格兵：盾の回復、副武器（肩の対戦車火器）、専用の部隊判断と軍の割り当て、見た目（AC01の体と武器＝ApplyExoSuit） |
| `BattleFXCheck` | .h/.cpp | 15 + 221 | BattleFXCheck | 効果音・エフェクトの確認（試験だけ。`-BattleFXCheck`、Setup/Test-SoundFX.ps1。止めた戦闘の前で爆発を撮影し、続けて射撃・着弾・爆発・発射・合図の音を出して、スピーカーを鳴らさずに音の混ざりを WAV に録る） |
| `BattleFieldLayout` | .h | 336 | BattleFieldLayout, FMapDef | マップごとの定義（広さ、拠点、試合時間）を1つの表にまとめたもの |
| `BattleFieldReplay` | .cpp | 206 |  | 広域試験の記録と再生（UBattleSquadTrial の一部） |
| `BattleFieldTurn` | .cpp | 168 |  | 広域試験 T2b（UBattleSquadTrial の一部） |
| `BattleFollowerBones` | .h/.cpp | 10 + 39 | BattleFollowerBones | 霧で隠れた部下のボーン表示の切替 |
| `BattleFrontLine` | .cpp | 1485 |  | Surprise bridge waypoints: UpdateSurpriseBridge, BattleSurpriseRoute::Bridge / Intersects; fixed H/I map geometry, entrance / exit / edge goal, retained on same-time restore |
| `BattleGameMode` | .h/.cpp | 96 + 373 | ABattleGameMode | モードの統合、起動時の組み立て、毎フレームの入口、基本回帰 |
| `BattleGroups` | .cpp | 747 |  | 部隊群：編成と解散、役割、集結・前進・保持の段階、AIの群の運用、群に従う命令（UBattleMatch の一部） |
| `BattleHUD` | .h/.cpp | 74 + 1722 | ABattleHUD | 画面表示（HUD、ミニマップ、部隊一覧、通知、命令の印） |
| `BattleKnownTargetFire` | .cpp | 191 |  | 軍共有候補の上位3件・本人確認・狙いの射線・光線予算・通常／覗き射撃（UBattleLeaderRuntimeの一部、段階1） |
| `BattleKnownTargetMove` | .cpp | 309 |  | 既知敵の位置取り（値2）：観測から移動先を選び、予算・予約・命令制限を確認して短距離移動 |
| `BattleKnownTargets` | .h/.cpp | 53 + 63 | BattleKnownFire, FTarget, FTeam, FResult, FSearch, FBudget | 軍共有の観測表と50m格子。知覚結果の適用後だけ集約し、元の観測時刻・期限・追跡試料を保持（既定0） |
| `BattleLeaderRuntime` | .h/.cpp | 1049 + 2130 | FBattleBodyInit, FBattleBrokenCover, FBattleFollowerFragment, FBattleSquadSummary, FBattleSquadListRow, FBattleSquadListLayout, FBattleMinimapLayout, FBattleLeaderBody ほか7 | 混成の中心：起動、視線、隊長の判断、兵の移動と射撃、表示の更新 |
| `BattleMapI` | .h | 112 | BattleMapI | マップI（区画を前提にした設計）の表：50mの区画 20×14 の種類 `Kind`、道路 `Roads`、川・橋・浅瀬の位置、家 `Houses`・丘 `Hills`、高さの表 `MakeHeights`、拠点 `Objectives`。配置は `BattleMapLayoutI.inl`（生成と共有） |
| `BattleMapJ` | .h | 143 | BattleMapJ, FMirror | マップJ（11-22、`Docs/design-map-j-terrain.md`）の区画の表：山 `M`（通れない・見通しを遮る）・湖 `L`（通れない）と、ビル・集合住宅・教会・工場・廃墟・駐車場・公園・商店の区画。区画ごとの配置の候補と8通りの向き（`Variant`／`Mirror`）、入れる家の位置（`HouseOf`）、拠点・丘・道。配置は `BattleMapLayoutJ.inl`、生成は `BattleMapCommandlet`（`-WideField=J`、`Setup/Initialize-MapJ.ps1`） |
| `BattleMapK` | .h | 119 | BattleMapK | マップK（11-22、`Docs/design-map-k.md`）の区画の表：密な小さい市街地（街は列4〜15・行3〜10、外側は山）、道 `Roads`、拠点6つ、入れる家 `HouseOf`。配置はマップJ の `BattleMapLayoutJ.inl` を名前空間だけ差し替えて使い、K だけの高い塀・追加の遮蔽は `bDenseCover`、道のトラックは `BattleMapCommandlet`（`-WideField=K`、`Setup/Initialize-MapK.ps1`、区画図 `Setup/Export-MapK.py`） |
| `BattleMapL` | .h | 124 | BattleMapL | マップL（11-25、`Docs/design-terrain-field.md`）：マップIの区画・道・川・家・拠点を起伏のある地面に置いた地図。地面の形 `Raw`（起伏・丘・川の谷・本拠地の平地）、家と倉庫の平らな土台 `Pads`、高さの格子 `MakeField`、高さの表 `MakeHeights`、地面の高さの拠点 `Objectives` |
| `BattleMapStats` | .cpp | 191 |  | マップの統計（区域ごとの見通し・遮蔽、経路の長さ）を JSON に書き出す。試合は行わない（UBattleLeaderRuntime の一部） |
| `BattleMatch` | .h/.cpp | 626 + 635 | FAreaIntent, FBattleObjective, FBattleStrike, FPlayerRoute, UBattleMatch, FFrontPlanTeam, FGroupStrength, FFireReveal | 試合の進行、雇用、拠点の占領、陣営の命令、補給、勝敗 |
| `BattleMetrics` | .h/.cpp | 65 + 142 | FBattleMetricFrame, UBattleMetrics, FBattleScopedMetric | 計測値の記録（1フレームの時間、判断の回数） |
| `BattleMixedBodies` | .cpp | 67 |  | 混成の兵の体の生成・破棄（UBattleLeaderRuntime の一部） |
| `BattleMixedDriver` | .h/.cpp | 50 + 341 | FBattleMixedUnit, FBattleMixedDriver, MixedNavigation | 混成の兵1人ぶんの実行（旧 AIController と同じ役を Actor なしで行う） |
| `BattleMixedRecruitment` | .cpp | 141 |  | 補充と購入部隊の出現、偵察兵の昇格（UBattleLeaderRuntime の一部） |
| `BattleMixedSquadAI` | .cpp | 166 |  | 部隊 AI の混成版の更新（UBattleSquadAI の一部） |
| `BattleMoveCourse` | .h/.cpp | 43 + 159 | UBattleMoveCourse, FRunner | 移動の計測コース |
| `BattleMuzzleFlash` | .h/.cpp | 10 + 92 | BattleMuzzleFlash | 銃口の閃光（表示だけ。発光する楕円体の使い回し） |
| `BattleNavigation` | .h/.cpp | 11 + 44 | BattleNavigation | 目的地の補正と隊形の位置 |
| `BattleNetwork` | .h/.cpp | 148 + 337 | FBattleNetUnit, FBattleNetFrame, ABattleNetController, ABattleNetGameMode, ABattleNetHUD | ネットワーク試作（GameMode、Controller、HUD） |
| `BattleNetworkSerialization` | .cpp | 31 |  | ネットワーク用の兵の直列化 |
| `BattlePeekDiagnostics` | .cpp | 36 |  | 覗き1回のフレームを排他的に分類する記録専用の計数（peekwhy） |
| `BattlePerception` | .h/.cpp | 154 + 641 | FBattleContact, FBattleObserver, FBattleReport, FBattleSquadKnowledge, FBattleSquadContact, FBattleSquadIntel, FBattleSquadReport, UBattlePerception | 認識：見えた敵の記憶。分隊単位の知識（`-BattleSquadKnowledge=1`〔試験、既定0〕：分隊の表と分隊間の報告は `Update` の末尾、読み口は `Knows`・`FindSquadContact`・`GetSquadContacts`。段階1の表は `BuildSquadKnowledge`。`Docs/design-squad-knowledge.md`） |
| `BattlePhase3` | .h/.cpp | 90 + 425 | UBattleMixedAvoidance, UBattlePhase3, FPhase3Scope | Phase3 の大規模試験経路（通常の混成とは別の処理） |
| `BattlePlayerPath` | .cpp | 329 |  | プレイヤーの入力の補助：命令先の点（2階は壁越しでも指せる、Alt で1階）`PickOrderPoint`、右ドラッグで引いた道筋の命令 `PlayerPathOrder`、選んだ部隊の道筋の予測表示 `DrawOrderWays`／`PlayerRoutePreview`、壁の乗り越え `TryVault`、作戦カメラの速さ `PanSpeed`（ABattleController・UBattleMatch・ABattleHUD の一部） |
| `BattlePlayerRoutes` | .cpp | 215 |  | 通り道のプレイヤー指定：部隊ごとに 自動・中央・北・南 を選び、手動の遠い命令と委任中の移動をその道に通す。自動試験 `-BattlePlayerRouteTest`（UBattleMatch の一部） |
| `BattleQuitMenu` | .cpp | 168 |  | 上部バーのボタン列（主なキーと同じ操作）と終了の確認画面（Esc・F10）の配置と操作（ABattleController の一部） |
| `BattleRagdoll` | .h/.cpp | 14 + 43 | BattleRagdoll | 死体が死亡クリップの後に物理で倒れて止まる（表示だけ。`-BattleRagdoll=0` でクリップの最終フレームのまま） |
| `BattleRanges` | .h/.cpp | 19 + 56 | BattleRanges | 認識・交戦の距離（マップごとに決まる） |
| `BattleReplayData` | .h | 15 | FBattleReplayHeader, FBattleReplayUnit | 再生用データの型（試験専用） |
| `BattleRoutes` | .cpp | 179 |  | 通り道：道路網のあるマップ（F）で、遠い目的地へ交差点を順に指示する。本拠地へ向かう部隊は観測した敵の少ない側面路を選ぶ（UBattleMatch の一部） |
| `BattleScoutReport` | .cpp | 180 |  | 偵察報告（UBattleMatch の一部） |
| `BattleSectorLanes` | .cpp | 121 |  | Trial -BattleSectorLanes=1 (2026-11-25, default 0): spread entering squads over authored lanes and keep their choice. |
| `BattleSectorRoute` | .cpp | 239 |  | 区画経由の移動 `-BattleSectorRoute`・側面 `-BattleSectorFlank`・前線 `-BattleSectorFront`（区画のあるマップ、AIの歩兵）：道路網の次の交差点の代わりに、その周りで遮蔽の多い区画を制圧・維持しながら進む。側面は観測した敵の正面を避けた区画、前線は自軍の区画のうち敵に接する区画（UBattleMatch の一部） |
| `BattleSectorSight` | .cpp | 70 |  | 区画内・区画間の戦い（`-BattleSectorSight`、11-23 から既定1＝通常小隊だけ。`=2` は兵種ごとの決まり `SectorSightRole`〔試験〕、`=0` で元の動き。`Docs/design-sector-combat.md`）：区画のあるマップで、通常小隊は別の区画（20m より遠い）の敵を探す光線を隊長だけが出し、隊員は隊長が今見ている敵を発射直前の視線判定で撃つ（UBattleLeaderRuntime の一部。走査の側は `UBattlePerception::Update` の `SkipCross`） |
| `BattleSectorView` | .h/.cpp | 71 + 191 | FBattleFogGrid, FBattleSectorInfo, BattleSectorView | 作戦画面の表示だけ（規則・AIは読まない）：自軍の視界の格子 `FBattleFogGrid`（12.5m、自軍の兵から静的な物体への光線）と区画ごとの状態 `BattleSectorView::Gather`（自軍の人数・観測した敵・視界の割合・拠点・命令中の部隊・遮蔽位置の数） |
| `BattleSectors` | .h | 70 | BattleSectors | マップHの50m区画の格子（20×14）：区画の番号と中心、区画の種類の表、川・橋・浅瀬の位置 |
| `BattleSoldierBody` | .h/.cpp | 13 + 36 | BattleSoldierBody | 歩兵の体のメッシュの選択（Quinn／RB01、試験切替 -BattleSoldierBody）と、それに合う遠景VATの置き場所 |
| `BattleSoldierFire` | .cpp | 62 |  | 個人AI `-BattleOwnTarget`（試験切替）：小隊の目標を撃てない兵が、自分で見ている敵兵を撃つ（UBattleLeaderRuntime の一部） |
| `BattleSoldierMove` | .cpp | 124 |  | 個人AI段階B1 `-BattleSoldierMove`：区画の中で、隠れている兵が近くのもっと良い遮蔽を採点して自分で移る（UBattleLeaderRuntime の一部） |
| `BattleSoldierOrder` | .cpp | 351 |  | プレイヤーが兵1人に出す移動の命令：命令と歩き（UBattleLeaderRuntime）、Ctrl＋クリックの選択と右クリックの命令（ABattleController）、印（ABattleHUD）、自動試験 `-BattleSoldierOrderTest` |
| `BattleSoldierWatch` | .cpp | 117 |  | 個人AI `-BattleSoldierWatch`：止まっていて撃つ相手のいない兵が、知っている最も近い敵へ、無ければ小隊で分けた前方へ向く（視野120度。UBattleLeaderRuntime の一部） |
| `BattleSquadAI` | .h/.cpp | 35 + 198 | FBattleSquadPlan, UBattleSquadAI | 旧全Actor経路の部隊 AI |
| `BattleSquadFlank` | .cpp | 204 |  | 迂回の判断（UBattleSquadTactics の一部） |
| `BattleSquadRetreat` | .cpp | 527 |  | 撤退の判断と適用（UBattleSquadTactics の一部。Evaluate と Apply はここ） |
| `BattleSquadSupport` | .cpp | 124 |  | 援護の受け渡し（UBattleSquadTactics の一部） |
| `BattleSquadTactics` | .h/.cpp | 119 + 161 | FBattleSquadObservation, UBattleSquadTactics | 部隊の戦術の土台：観測、毎フレームの更新、記録 |
| `BattleStableMovement` | .cpp | 841 |  | 混成の移動計画と実行（UBattleLeaderRuntime の一部） |
| `BattleStance` | .h/.cpp | 49 + 324 | BattleStance | 姿勢（立ち・しゃがみ）の数値と切替 |
| `BattleStanceAnimImport` | .cpp | 43 |  | 姿勢アニメーションの取り込み（エディタ用。宣言は BattleVATBake.h） |
| `BattleState` | .h/.cpp | 898 + 699 | FBattleWeaponDef, BattleTactics, BattleSquadType, BattleRosterLimits, FBattleDecisionProfile, FBattleSquadTacticalPlan, FBattleSquadState, FBattlePerceptionProfile ほか6 | 正規状態：兵・部隊の記録、命令、弾薬、被害、調整値 UBattleTuning、兵科の差 |
| `BattleStateTrace` | .cpp | 94 |  | 段階2の切り分け（`Docs/design-divergence-trace.md`）：`-BattleStateTrace=<N>` で N フレームごとに状態のかたまり（位置・体力・遮蔽・認識・計画・試合・射撃）の要約を `BATTLE_STATE_TRACE` に書く。読むだけで試合は変えない。比べるのは `Setup/Find-Divergence.py`（UBattleLeaderRuntime の一部） |
| `BattleStrategy` | .cpp | 269 |  | 戦略層（`Docs/design-strategy-layer.md`、試験切替 `-BattleSituationLog`、既定0）：戦線＝拠点のボロノイ分割＋両本拠地の150m。状況図 `BuildSituation`（自軍の戦力、知っている敵＝見えている・偵察の報告・記憶を90秒で軽く、拠点を最後に見た時刻、状態 不明／敵優勢／拮抗／自軍優勢）、評価用の記録 `BATTLE_SITUATION`（本当の敵の数は記録の中だけ）、前線の計画と共用の集中先の時刻（`FBattleAreaState::Strategy*` に保存。旧 S2〜S4 と `-BattleStrategy=2/3/4`・`-BattleStrategyPass` は 11-25 に削除）（UBattleMatch の一部） |
| `BattleSupplyCar` | .cpp | 94 |  | 補給車（兵種9）：積んだ弾・本拠地での積み直し・AIの命令（群の役割5、拠点の後ろ）・弾の少ない部隊の行き先（UBattleMatch の一部） |
| `BattleSupport` | .cpp | 69 |  | 旧アリーナの支援砲撃（UBattleMatch の一部） |
| `BattleSuppression` | .h/.cpp | 18 + 101 | BattleSuppression | 制圧の値と効果 |
| `BattleTacticalAI` | .cpp | 124 |  | 旧全Actor経路の兵の戦術行動（ABattleAIController の一部） |
| `BattleTactics` | .h/.cpp | 263 + 370 | FBattleExposure, BattleTactics | 調整値の束（補給、雇用、戦略、収入）と試験切替、散らばり、露出の計算 |
| `BattleTankGun` | .cpp | 207 |  | 戦車砲の照準と発射（UBattleLeaderRuntime の一部） |
| `BattleTeamAI` | .h/.cpp | 21 + 144 | UBattleTeamAI | 陣営全体の判断 |
| `BattleTerrain` | .h | 245 | BattleTerrain, FHeightPatch, FHeightField, FHeights, FHouseShape | 地面の高さの表：丘・階段・2階の矩形（マップG〜K）と地面全体の高さの格子 `FHeightField`（マップL）、その場所の高さを返す `FHeights::Ground`。家と丘の位置（`HousesG/HillsG`）は生成と共通 |
| `BattleThreatCover` | .cpp | 270 |  | 個人AI段階A `-BattleThreatCover`：兵ごとの脅威（自分の目撃4秒＋撃たれた方向）、全員から隠れる遮蔽、見破られた遮蔽を出て探し直す（UBattleLeaderRuntime の一部） |
| `BattleUIText` | .h/.cpp | 11 + 142 | BattleUIText | 画面の文言の日本語化 |
| `BattleUnit` | .h/.cpp | 220 + 861 | FBattleSightContext, ABattleUnit, ABattleAIController | 兵の Actor（表示と直接操作。正規状態は BattleState 側） |
| `BattleVATBake` | .h/.cpp | 9 + 200 |  | VAT の焼き込み・検証・修復（エディタ用） |
| `BattleVATData` | .h | 40 | FBattleVATClip, FBattleVATMesh, UBattleVATData | VAT のデータ型 |
| `BattleVATDisplay` | .h/.cpp | 95 + 431 | UBattleVATSkeletalMeshComponent, FBattleVATEntry, ABattleVATDisplay | 遠距離の兵を VAT で描く表示 |
| `BattleVATMaterial` | .cpp | 125 |  | VAT 用マテリアルの作成（エディタ用。宣言は BattleVATBake.h） |
| `BattleVehicle` | .cpp | 163 |  | 車両の設定、操舵、砲塔、車両どうしの視認（UBattleLeaderRuntime の一部） |
| `BattleVehicleNavMesh` | .h | 13 | ABattleVehicleNavMesh | 車両専用のナビメッシュの型（歩兵用と分けるため） |
| `BattleVehicleTactics` | .cpp | 72 |  | 車両の戦術：車両への脅威の見積もり、装甲車の行き先（UBattleMatch の一部） |
| `BattleViewVisibility` | .h/.cpp | 21 + 50 | FT5Visibility | 視点ごとの描画の切替（T5 試験用） |
| `BattleWallCover` | .cpp | 122 |  | プレイヤーの壁の遮蔽 `-BattlePlayerWallCover`（既定1）：壁に押し当てる・C でその場の遮蔽に入る、低い壁はしゃがむ・高い壁は立つ、構えて身を出す、離れて出る（UBattleLeaderRuntime の一部） |
| `BattleWarScore` | .h/.cpp | 18 + 79 | BattleWarScore | 戦果点（UBattleMatch の一部） |
| `BattleWeaponOrders` | .cpp | 37 |  | 命令・防衛範囲・支援対象・区画任務の交戦許可と武器/装甲AIの試験切替 |

## モジュール一覧（試験・計測・生成）

名前が Smoke / Trial / Soak / Benchmark / Probe / Pilot / Checks / Test / Audit / Diagnostic / Experiment / Validation / Shot / Visual / Preview / Commandlet で終わるもの。

| モジュール | 種類 | 行数 | 宣言している型 | 役割 |
|---|---|---|---|---|
| `BattleAbilitySmoke` | .cpp | 125 |  |  |
| `BattleAnimTaskTrial` | .h/.cpp | 29 + 85 | ABattleAnimTaskTrial |  |
| `BattleAreaSmoke` | .cpp | 1114 |  |  |
| `BattleBaseGarrisonSmoke` | .cpp | 91 |  | Fixed scenes only: positions and casualties are controlled without running ordinary combat between checks. |
| `BattleBenchmark` | .cpp | 194 |  |  |
| `BattleCompositionSmoke` | .cpp | 142 |  | 編成AIの選択・貯蓄のスモーク検査（UBattleLeaderRuntimeの一部） |
| `BattleConeTrialCommandlet` | .h/.cpp | 12 + 48 | UBattleConeTrialCommandlet | Closest distance between a forward ray and the capsule's vertical centre segment. |
| `BattleContactTrial` | .cpp | 101 |  | Fixed contact fixtures retain all 500 bodies, collision, and the normal executor. |
| `BattleContextTrial` | .cpp | 52 |  | Test-only full-state fingerprints. Never enabled during performance runs. |
| `BattleCornerTrial` | .cpp | 159 |  |  |
| `BattleCoverActionSmoke` | .cpp | 568 |  |  |
| `BattleCoverFirePointTrial` | .cpp | 77 |  |  |
| `BattleCoverPeekTrial` | .cpp | 117 |  | 低壁越しの覗きの固定場面3つ（目撃済み・見失い・未発見、試験の印があるときだけ） |
| `BattleDeathShot` | .cpp | 123 |  | Death look check (2026-11-21): -BattleDeathShot -BattleArea -BattleWideScreens=<prefix> on a wide mixed map. |
| `BattleDelegationSmoke` | .cpp | 110 |  |  |
| `BattleDisplayLODTrial` | .cpp | 83 |  |  |
| `BattleExoShot` | .cpp | 121 |  | Exoskeleton look check (2026-11-18): -BattleExoShot -BattleArea -BattleWideScreens=<prefix> on a wide mixed map. |
| `BattleFieldTrial` | .cpp | 430 |  |  |
| `BattleFieldViewTrial` | .cpp | 248 |  |  |
| `BattleFlankBayTrial` | .cpp | 68 |  |  |
| `BattleGridTrial` | .cpp | 86 |  | Test-only synchronous mixed Actor/Mass fixture. Never used for performance. |
| `BattleHiddenBoneTrial` | .h/.cpp | 27 + 74 | ABattleHiddenBoneTrial |  |
| `BattleKnownTargetFireTrial` | .cpp | 246 |  | 軍共有射撃の固定場面。実発砲・候補の切替・鮮度・追跡・予算・命令を検査、BATTLE_KNOWN_FIRE_RESULT |
| `BattleKnownTargetMoveTrial` | .cpp | 144 |  | 値2の固定場面14項目。値1の27項目に追加して実行 |
| `BattleLargeFieldTest` | .cpp | 83 |  |  |
| `BattleLeaderSmoke` | .cpp | 143 |  |  |
| `BattleMapCommandlet` | .h/.cpp | 14 + 934 | UBattleMapCommandlet | Reproducible editor-only arena authoring; never overwrites a map unless -Force is passed. |
| `BattleMassTrial` | .h/.cpp | 49 + 86 | UBattleTrialMovement, ABattleMassTrialUnit, FBattleTrialMotion, UBattleMassTrial | Flat-ground benchmark adapter. Navigation, RVO and swept capsule collisions |
| `BattleMatchSmoke` | .cpp | 58 |  |  |
| `BattleMatchSoak` | .cpp | 52 |  |  |
| `BattleMaterialTrial` | .cpp | 106 |  |  |
| `BattleMixedFastSmoke` | .cpp | 61 |  |  |
| `BattleMovementTrial` | .cpp | 112 |  |  |
| `BattleMovingSquadTrial` | .cpp | 222 |  |  |
| `BattleNetworkBenchmark` | .cpp | 119 |  |  |
| `BattleNetworkPilot` | .cpp | 143 |  |  |
| `BattleNetworkPilotProbe` | .cpp | 104 |  |  |
| `BattleNetworkProbe` | .cpp | 83 |  |  |
| `BattleObjectiveSmoke` | .cpp | 70 |  |  |
| `BattleOrderSmoke` | .cpp | 154 |  |  |
| `BattleOwnRouteSmoke` | .cpp | 194 |  |  |
| `BattlePerceptionOptimizationSmoke` | .cpp | 150 |  |  |
| `BattlePerceptionSmoke` | .cpp | 160 |  |  |
| `BattlePhase3Checks` | .cpp | 89 |  |  |
| `BattleRenderDiagnostic` | .h/.cpp | 37 + 217 | FT5RenderDiagnostic | Test-only state. Components are owned by a world Actor, never combat units. |
| `BattleScaleSmoke` | .cpp | 84 |  |  |
| `BattleScopeShot` | .cpp | 48 |  | 構え・スコープの撮影試験（UBattleLeaderRuntime の一部） |
| `BattleSectorAudit` | .cpp | 111 |  | 区画命令の検証 `-BattleSectorAudit`（区画のあるマップ、描画なし）：全区画へ制圧命令を出して受け付けを数え、8部隊の到着位置が区画内か、240秒後に維持へ移ったかを報告する。規則は変えない |
| `BattleSightExperiment` | .h/.cpp | 68 + 295 | FBattleSightExperiment, FPeekScan |  |
| `BattleSightTrial` | .cpp | 77 |  | Diagnostic fixture: retain 500 bodies, isolate one observer and one target. |
| `BattleSightVisual` | .cpp | 37 |  | 視界の目視確認用の表示（UBattleLeaderRuntime の一部） |
| `BattleSquadFlankSmoke` | .cpp | 181 |  |  |
| `BattleSquadRetreatSmoke` | .cpp | 195 |  |  |
| `BattleSquadSmoke` | .cpp | 215 |  |  |
| `BattleSquadTacticsSmoke` | .cpp | 179 |  |  |
| `BattleSquadTrial` | .h/.cpp | 164 + 181 | FBattleTrialSoldier, UBattleSquadTrial, FT2bUnit, FMember, FFieldUnit |  |
| `BattleStanceShot` | .cpp | 101 |  | 姿勢の撮影試験（UBattleLeaderRuntime の一部） |
| `BattleSupportSmoke` | .cpp | 68 |  |  |
| `BattleTacticalSmoke` | .cpp | 182 |  |  |
| `BattleTacticsSmoke` | .cpp | 111 |  |  |
| `BattleTeamSupportSmoke` | .cpp | 134 |  |  |
| `BattleTerrainTest` | .cpp | 171 |  | 高低差の自己試験 `-BattleHeightTest`：丘の上・2階・尾根越えへ3部隊を歩かせ、着いた高さと浮き沈みを調べる（UBattleMatch の一部） |
| `BattleThirdPersonSmoke` | .cpp | 110 |  |  |
| `BattleVATBakeCommandlet` | .h/.cpp | 12 + 53 | UBattleVATBakeCommandlet |  |
| `BattleWeaponPreview` | .cpp | 60 |  | 武器の見た目の確認 |
| `BattleWeaponRangeTrial` | .cpp | 47 |  |  |
| `BattleWideMapAudit` | .cpp | 198 |  |  |
| `BattleWideMixedSmoke` | .cpp | 208 |  |  |
