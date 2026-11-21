# FutureBattle AI層の総点検 01

レビュー日：2026-11-21。対象HEAD：`e3f9d98f520887546bfc24cb7abd547f4ae9ea26`。

## 対象・根拠・制限

`STATUS.md` の要約と直近項目、`CODEMAP.md`、`CODEMAP_SYMBOLS.md` の順に入口を確認し、実装と参照元を静的に追った。以下の `Battle*.cpp:行` / `.h:行` は、特記しない限り `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。索引と実ファイルで行番号がずれる箇所は実ファイルを採用した（例：`UpdateCompositionHiring` は56行）。

通常の広域・エリア混成経路を主対象とし、旧全Actor経路、Phase3比較経路、試験切替も区別した。UEテンプレートの Variant_* はゲーム戦術AIの層に数えていない。ビルド、UE起動、試合、試験、計測は一切実施していない。確信度「高」はコード上の経路・条件を確認できたという意味であり、発生率や勝敗への大きさを実測した意味ではない。

開始時の `git status --short -uno` に追跡ファイルの変更表示はなかった。gitの個人ignoreファイルへのアクセス警告は出た。指定された本報告以外のファイルは編集していない。STATUSの古い要約には群制御OFF、保存問題未解決などの記述が残るが、直近項目と実装を優先した。現在の群制御は `BattleState.h:496` の `bAreaGroups=true`、編成Plannerは既定OFF。保存の過去の不具合を今回新たに再現したとは扱わない。

## 1. AIの層と更新の地図

独立したSector用Subsystemが全戦場を指揮している構成ではない。Areaが戦略任務を決め、Groupsが所属部隊の目標を差し替え、道路・区画処理が中間目的地へ変換する。SquadTacticsが一時的な撤退・補給・援護・迂回を管理し、LeaderRuntimeが部隊の目標と移動計画を作り、Followが個人の行動を実行する。

### 通常混成経路の実行順

`BattleGameMode.cpp:169` → `BattleLeaderRuntime.cpp:1321` `Step`。

1. `Match::Update` → プレイヤー経路、味方委任命令、拠点ルール、Area観測・補給・偵察、雇用・敵命令。
2. `SquadTactics::Step` → 分隊ごとの観測標本・戦術評価 → `TeamAI::Step`。
3. ランチャー・盾更新、外骨格副武器の選択、視線マスク更新。
4. `Perception::Update`。
5. 期限または命令Revisionが変わった分隊の `Think`。
6. 近接回避グリッド再構築 → 必要なら射撃視線先読み → 全兵の `Follow` → 飛翔砲弾の更新。

根拠：`BattleLeaderRuntime.cpp:1338`, `:1348`, `:1350`, `:1358`, `:1365`。上位層・外骨格副武器はこのフレームのPerception更新より前の認識を読む。必ずしも誤りではないが、更新順の変更は挙動変更になる。

| 層・入口（実装位置） | 実際の頻度・条件 | 入力 | 書き込み・出力 |
|---|---|---|---|
| 戦略・経済：`Update` (`BattleMatch.cpp:152`)、`UpdateAreaHiring` (`BattleArea.cpp:136`)、`UpdateTeamOrders` (`BattleMatch.cpp:348`) | Updateは毎フレーム。味方委任と敵Directorは別時計で各5秒。開始・委任による早期更新あり | 自軍資源・部隊、観測拠点所有者、AreaIntent、購入目的、兵種、群、観測敵 | 購入、MissionOrder/Destination、HirePurpose。命令はIssueOrder経由 |
| エリア観測：`UpdateArea` (`BattleArea.cpp:206`)、`AreaVisible` (`:48`) | 拠点・敵本拠地観測0.2秒。収入は支払い時計。占領判定は毎フレーム | 自軍センサーと地形、観測成功時のみ対象所有者・本拠地HP。占領・収入ルールは正規状態 | KnownOwners / SeenAt / KnownEnemyBaseHP、資源・補給状態 |
| 陣営情報集約：`AreaIntelView` (`BattleAreaIntel.cpp:19`) | 呼び出し時に遅延更新、陣営ごと1秒キャッシュ。AreaIntel切替OFFでも群・砲兵等から使う | 自軍位置、個人接触、偵察報告。bHomeCoveredには距離だけの経路あり | OwnAt / EnemyAt / Enemies、既知車両キャッシュ、砲兵クラスタ |
| 偵察：`UpdateScoutReports` (`BattleScoutReport.cpp:45`) | 毎フレーム呼ぶ。報告・警報に個別時計、AddHintは2秒間隔 (`:146`) | 自軍HP・位置・監視所、長距離の観測記憶 | ScoutReports、警報・退避期限、味方へのHint |
| 群：`UpdateGroups` (`BattleGroups.cpp:213`)、`GroupOrder` (`:444`) | 陣営命令更新の都度、通常各5秒 | 自軍所属・生存人数・補給状態、観測所有者、AreaIntelView | Groups / SquadGroup、役割、集合・前進・保持・予備、上位のCenter/Orderを差し替え |
| 道路・区画：`RouteStep` (`BattleRoutes.cpp:43`)、`SectorStage` (`BattleSectorRoute.cpp:162`) | 陣営命令更新時、対象分隊ごと。SectorFrontの「2秒」は最短更新間隔で、通常入口が5秒のため自律2秒更新ではない | 静的道路・区画・遮蔽、自軍位置、RouteKnownEnemies、被弾記録 | 次の中間目標、区画任務フラグ、SectorState / SectorFrontCells。SectorViewはHUD側でありこのAIの入力ではない |
| 編成：`UpdateCompositionHiring` (`BattleCompositionRuntime.cpp:56`) → `Capture` (`BattleComposition.cpp:39`) → `Decide` (`BattleCompositionPlanner.cpp:13`) | Planner適用陣営の雇用更新時、通常5秒。資源純増の推定窓20秒 | 自軍編成・HP・弾薬・購入数、観測兵種、役割不足、残り時間。純関数DecideはWorld参照なし | SaveType/Deadline/Retry等のCompositionState、Buy/Save/Wait、既存HireAreaSquadの実行 |
| 分隊戦術：`Step` (`BattleSquadTactics.cpp:97`) → `Observe` (`:38`) → `Evaluate` (`BattleSquadRetreat.cpp:79`) | Stepは毎フレーム。分隊標本・評価は1秒、初期位相あり。非自律計画の期限は毎フレーム | 自軍HP・弾薬・被害、接触スナップショット、元任務 | TacticalPlan、現在のOrder/Destination、補給往復・撤退・集合・交戦 |
| 部隊間の援護・迂回：`TeamAI::Step` (`BattleTeamAI.cpp:15`) | 権限の掃除は毎フレーム、割当1秒。新規迂回は陣営ごと1件/更新 | 自軍戦術計画、分隊観測、共通接触、位置 | Support/Flank、対象・期限・クールダウン。Groupsとは別の調整層 |
| 認識：`Perception::Update` (`BattlePerception.cpp:57`) | 毎フレーム入口。個人走査は通常0.2秒、未発見時0.4秒。初回位相あり。配信キューは毎フレーム | センサー段階では実体位置・衝突を測定。認識成功後にスナップショット化 | Contacts、DetectionStarted、LastSeenTime、bVisible、PendingReports |
| 隊長判断：`Think` (`BattleLeaderRuntime.cpp:588`) | 通常0.5秒、命令Revision変更なら前倒し。再経路探索は要求時・最短1秒。設定は `BattleState.h:686`、`DefaultGame.ini:140` | 部隊員の直接観測、任務、戦術状態、予約・地形。装甲再選択は別経路 | Plans.TargetID/ArmourTargetID、Path、ArrivalGoals、区画接触・交互前進フラグ |
| 個人実行：`Follow` (`BattleLeaderRuntime.cpp:864`)、`StableMove` (`BattleStableMovement.cpp:299`) | 全生存兵、毎フレーム。Dtは上限0.05秒 (`BattleLeaderRuntime.cpp:1369`) | 隊長計画、個人接触、自己の弾薬・姿勢・被弾、地形 | 移動、向き、Aim/Action、射撃、遮蔽探索。近接回避は格子・最大32候補 |
| 遮蔽・覗き：`RequestCoverAction` / `StepCoverApproach` / `StepCoverAction` (`BattleCoverAction.cpp:79`, `:320`, `:375`) | 探索は予算・再試行待ち。状態実行は毎フレーム。通常枠に加えCoverPriorityは4件/フレームの優先枠 | 観測脅威、自己の命令Revision、Nav・露出・予約 | CoverAnchor/Peek/Phase/TargetID、予約、移動、射撃、姿勢 |
| 兵種別目標：`PickArmourTargets` (`BattleArmourAI.cpp:45`) | Thinkと同時。ArmourAI既定ONだが車両存在時のみ | 部隊接触、武器・弾薬、車両分類 | 通常TargetIDを再選択、対戦車目標 |
| 戦車主砲：`TankGunTarget` / `StepTankGun` (`BattleTankGun.cpp:30`, `:85`) | 実行は毎フレーム、再選択0.5秒 | 自身・陣営の既知位置、車両分類、射線 | TankGuns、砲塔方向、主砲発射 |
| 間接射撃：`StepArtillery` / `PickArtilleryAim` (`BattleArtillery.cpp:251`, `:175`) | 静止時毎フレーム、選択は既定0.75秒 (`BattleState.h:467`) | 自身・陣営の既知位置、味方位置、設置・装填状態 | Artillery、Aim、飛翔弾、着弾の共通効果 |
| 外骨格副武器：`StepExoLaunchers` (`BattleExo.cpp:277`) | 選択0.5秒、描画更新はそれ以前に毎フレーム | 個人の直接観測、弾数、敵集団・車両 | ランチャー発射、弾薬・待機 |
| 補給ルール：`UpdateArea` (`BattleArea.cpp:242`) | 1秒ごと判定、10秒で弾薬補充・部下補充。帰還の判断自体はSquadTactics | 自軍位置・資源・生存、補給班。拠点補給は実所有者・争奪状態というゲームルール | AmmoTimers / Replacements、弾薬・HP・補充。SupplyTeamNearは分隊全体を検索 |

### 旧経路と試験経路

- 旧全Actor：`BattleGameMode.cpp:169` がMatch/Tactics/Perception/SquadAIを回す。`BattleSquadAI.cpp:32` のUpdateは0.5秒、接触の投票で共通目標・援護役割を決める。`BattleAIController.cpp:152` のTickは個別ThinkSecondsで判断する。混成有効なら同Tickは154行でreturnするので、通常混成と二重実行する関数ではない。
- Phase3：`BattleLeaderRuntime.cpp:1327` で `Phase3->Step` に入りreturn。bIndividual時は `BattlePhase3.cpp:200` → `BattleMixedSquadAI.cpp:7` のUpdateMixed、`BattleMixedDriver.cpp:126` の個人実行。それ以外は `BattlePhase3.cpp:203` で通常のThinkを再利用する。通常広域のStep後半とは別のスケジューラであり、全て別実装という意味ではない。
- `BattleSupport.cpp:8` の旧支援砲撃は `BattleMatch.cpp:168` の非Large経路に接続されている。通常の迫撃砲・自走砲とは別。旧関数を「通常で呼ばれない」だけで未使用と断定して削除しない。

## 2. 確認できた問題（確信度高を先に記載）

### F01【高】編成Plannerの車両認識がGroupStrengthに依存

- 根拠：`BattlePerception.h:13`、`BattlePerception.cpp:132`、`BattleGroups.cpp:474`、`BattleCompositionRuntime.cpp:78`。
- 状況→結果：新規試合でPlannerをON、GroupStrengthを既定0のままにする。センサーは敵を目視してもObservedSquadTypeを書かず初期値255。Plannerはこの兵種が車両である接触だけをVehicleSeenMSへ入れるため、車両を見てもObservedVehiclesが増えない。AT不足の緊急性、ATKeep、戦車目標数の反応が働かない。別の保存から既に識別済み値を読んだ場合は別条件。
- 改善：兵種識別をセンサーの独立出力とし、少なくともPlanner対象陣営では記録する。群の重み付けONを回避策にすると別の群行動まで変わるため結合を切る。
- 効果：Planner単独試験が成立する。危険度：中（接触保存との整合）。既定結果：Planner既定OFF経路を守れば不変。まずPlanner内、または新切替既定0で修正を評価する。

### F02【高】本拠地補給から再開すると区画任務が落ちる

- 根拠：`BattleSquadRetreat.cpp:130`〜`:139`、`BattleLeaderRuntime.h:440`、`BattleState.cpp:212`〜`:217`。
- 状況→結果：bSectorMission=trueの部隊がHOME SUPPLYを完了する。再開時の2回のIssueOrderはいずれもbSectorを省略し既定false。最初の呼び出しがRecover計画により実行を保留しても、SetSquadOrderはその前にbSectorMission=falseを書いている。次の再開命令もfalseとなり、区画目標制限・到着配置が通常の点命令に変わる。次の戦略更新で再設定される場合も、その間は異なる挙動になる。手動由来の区画任務では自動修復を期待できない。
- 改善：元任務をOrder/Goalだけでなく区画フラグ込みの値として保持して再開する。到達可否の検証と適用を分け、同じIssueOrderを2回呼ぶ構造も整理する。
- 効果：任務種別の保持、再開時の重複同期経路探索の削減。危険度：中。既定結果：区画任務を使う試合は変わり得るため、修正切替は既定0から。

### F03【高】交互前進の「遮蔽から出る」が早期returnの後ろ

- 根拠：`BattleLeaderRuntime.cpp:868`、`:898`〜`:900`、`BattleCoverAction.cpp:375`〜`:490`。
- 状況→結果：Boundingで移動担当に切り替わった兵が有効なCoverPhaseを持つ。Followは最初にStepCoverActionを実行しtrueでreturnする。後方のbBoundMove判定・CancelCoverActionへ到達しない。覆いから出す明示処理は、StepCoverAction自身が解除した場合以外は現在遮蔽中の兵に効かない。遮蔽接近中の兵はこの早期return対象ではないので解除できる。
- 改善：計画による遮蔽退出の可否を状態実行の前に裁定する。直接操作・命令取消の優先順位も同じ入口で決める。
- 効果：交互前進の停止を防ぐ。危険度：中。既定結果：Bounding既定0では不変。既存Bounding ONの試験内で直し、既定化と分離する。

### F04【高】戦車主砲はWeaponOrderGateをONにしても命令制限を通らない

- 根拠：`BattleLeaderRuntime.cpp:1068`〜`:1076`、`BattleTankGun.cpp:30`, `:85`, `:114`、`BattleWeaponOrders.cpp:20`。
- 状況→結果：戦車にMove命令、あるいは指定対象だけのSupportを与えても、TankGunTargetは既知候補から独立に選択しStepTankGunが発射する。これらの入口・FireTankGunにOrderAllowsTarget/WeaponOrderAllowsはない。WeaponOrderGateの実装がある間接射撃 (`BattleArtillery.cpp:281`, `:313`)・外骨格 (`BattleExo.cpp:301`) とも不一致。
- 改善：主砲にも候補選択時と発射直前の交戦許可を導入する。プレイヤーの直接発射とAI命令による発射の権限は明示して共通の損害実行へ渡す。
- 効果：Move/Support/Defendの意味が武器で変わらない。危険度：中。既定結果：戦車参加時は変わり得る。既存WeaponOrderGate既定0の適用範囲を拡張して比較する。

### F05【高】装甲再選択が区画で除外した対象を復活させる

- 根拠：`BattleLeaderRuntime.cpp:612`, `:640`, `:719`、`BattleArmourAI.cpp:58`〜`:83`。
- 状況→結果：区画任務で区画外の車両・歩兵が見えている。通常Thinkの対象は区画制限を通るが、ArmourSectorGate=0では後段PickArmourTargetsが別のフィルタで選び直す。区画判定はなく、車両を見ていればP.TargetIDを上書きする。bSectorContact等は再選択前に算出済みなので、目標と移動判断の整合も崩れる。
- 改善：既存ArmourSectorGate=1の許可候補方式を基点に、通常射撃・装甲射撃が同じ候補集合と観測時点を使う構造にする。
- 効果：上位任務の制約を保持。危険度：中。既定結果：変わり得る。既存切替既定0を維持して評価する。対策コードは既に存在しており「未実装」とはしない。

### F06【高】Plannerの一時的な本拠地防衛購入が恒久任務になる

- 根拠：`BattleCompositionRuntime.cpp:113`〜`:136`、`BattleMatch.cpp:367`〜`:371`, `:428`、`BattleGroups.cpp:242`、`BattleAreaIntel.cpp:69`。
- 状況→結果：Planner ON、Purpose不要、直近30秒に本拠地被弾 → Input.Mission=101。その間に買う部隊は全てArea.HirePurposeに101を保存する。脅威が消えてもこのマップのエントリを解除する経路は確認できず、以後通常配分や群への参加より防衛目的が優先される。IntelPurposeには到着・脅威解消で解除する実装があるが、HirePurposeには適用されない。旧P1の「購入目的維持」は意図的でも、Plannerの緊急購入まで恒久化することは別途判断が必要。
- 改善：購入理由と現在の任務拘束を分離。EmergencyDefenseに期限・解除条件を持たせ、旧P1の固定目的と区別する。
- 効果：一度の被弾による後続購入部隊の拘束を防ぐ。危険度：中。既定結果：Planner既定OFFでは不変。Planner内の試験として実施。

### F07【高】本拠地「観測済み」の集約に遮蔽・視野角を見ない経路

- 根拠：`BattleAreaIntel.cpp:33`〜`:44`、`BattleArtillery.cpp:237`〜`:245`、`BattleTankGun.cpp:67`〜`:75`。
- 状況→結果：味方が敵本拠地の視界半径内にいるだけで、壁の向こう・背面でもbHomeCovered=true。既定の間接射撃はそれを本拠地照準の許可として使い、自身のAreaVisibleがfalseでも砲撃候補にする。戦車はさらに発射側射線を検査するが、情報の既知判定は同じ集約を参照する。
- 改善：到達可能な視界範囲と実観測を別フィールドにする。ArtilleryObservedの実観測方式を評価し、通常の戦略層で使うbHomeCoveredの意味も明示する。
- 効果：認識ルールの整合。危険度：中〜高（攻勢配分にも関係）。既定結果：変わる可能性あり。既存ArtilleryObserved既定0でまず砲撃を比較し、戦略側は別の既定0切替で評価。

### F08【高】砲兵の安全確認を上位8候補の重複が使い切る

- 根拠：`BattleArtillery.cpp:134`〜`:136`, `:215`〜`:235`。
- 状況→結果：同じ密集集団の兵ごとに同じ重心・人数の候補を作る。上位8候補が味方近接で全て不許可、9番目以降に安全な別集団がある場合でもINDEX_NONEとなる。候補8件の上限は集団8個の意味になっていない。
- 改善：既存ArtilleryClusters切替による同一集合の重複除去を評価する。ただし同切替は候補上限解除・陣営共有・発射時安全再検査も含むため、速度だけの変更ではない。
- 効果：安全な目標の取り逃し抑制、同集団の再計算減。危険度：中。既定結果：砲兵がいる試合で変わり得る。既存切替既定0を維持する。

### F09【高：コード内】能力調整用フィールドがC++では使われていない

- 根拠：`BattleState.h:216` の `LeaderAbilityScale = 1.f`。Source/Configへの参照検索では宣言以外なし。
- 状況→結果：C++の判断能力をこの値で調整したつもりでも、判断頻度・戦術能力・費用のいずれにも読まれない。UPROPERTYであるため、バイナリアセット・反射経由の利用が絶対にないとまでは断言しない。
- 改善：将来用であれば明記する。不要なら保存・反射互換を調べて廃止する。名前から効果を期待させるだけの値を増やさない。
- 効果：調整時の誤解を防ぐ。危険度：文書化は低、削除は中。既定結果：文書化は不変。能力に接続する実装は挙動変更なので既定0で導入する。

### F10【中：設計上の推測】群・兵種・購入目的・撤退で「本拠地攻撃担当」の定義がずれる

- 根拠：`BattleMatch.cpp:365`〜`:428`、`BattleSquadRetreat.cpp:111`〜`:117`。
- 状況→結果：補給帰還の閾値を決めるため、戦術層がAreaStrategyTargetを再実行してbBaseを読む。しかし実際の任務は、その後に購入目的、兵種役割、GroupOrderで差し替えられる。固定slotでは本拠地攻撃担当だが現在は別任務の部隊、またはPurpose=100で攻撃担当になった部隊の帰還閾値が、実任務と一致しない可能性がある。どちらを意図した調整かは静的コードだけでは確定できない。
- 改善：最終的な任務種別を値として渡し、戦術層が戦略配分を再計算しない。固定slot依存を維持するならその意味を明文化する。
- 効果：責務・閾値の一貫性、反復集計の削減。危険度：中。既定結果：変わり得るため試験切替既定0が必要。

### 競合として誤認しない保護

`BattleState.cpp:214` は戦略のMission更新を一時戦術計画中に保留し、元任務だけを更新する。このため「5秒ごとの群命令が毎回撤退を解除する」とは判断しない。手動命令は `BattleLeaderRuntime.cpp:583` で委任解除、PlayerRoutesの手動任務は `BattleMatch.cpp:396` で戦略更新から除外される。一方、F02のように保留前に書かれるメタデータやF05の下位目標差し替えにはこの保護が及ばない。

## 3. 効率：頻度と規模の概算

以下は実測ではない。N=生存兵数、Q=保存されている分隊数、O=拠点数、K=対象層が知る敵数、A=砲兵数、C=遮蔽候補総数、R=予約数、F=FPS。説明用にN=400、Q=50、O=9、K=100、F=60を置く。死亡分隊のレコードが残る検索は生存数より大きくなる。全探索が即トレースと同数になるわけではなく、距離・所属・FOV等の早期除外がある。

| 指摘・確信度 | 根拠／状況→処理量 | 改善、効果・危険度・既定結果 |
|---|---|---|
| P01 高：遮蔽予約の更新が二重全走査 | `BattleCoverAction.cpp:387` が遮蔽中毎フレームReserve。`BattleCover.cpp:176` は予約全件を検査し、`:191`→`:197` は同じUnitIDを全件探索して解除する。遮蔽兵B人なら概ね2BRFのレコード走査。B=R=100なら約120万走査/秒 | UnitID→slot索引、同slot更新の専用経路、近傍slot索引。期限・死亡・140cm近接競合の意味を保持。効果大の候補、危険度中。完全に同じ競合結果なら既定結果不変を狙える |
| P02 高：射撃待機中も毎フレームLOS | `BattleLeaderRuntime.cpp:887`〜`:889` でCanEngage用LOS、実射撃のクールダウン・弾薬拒否は`:1126`。覗きも `BattleCoverAction.cpp:444`。100人が対象を維持すると最大約6000回/秒の射線確認（対象者のみ） | CanEngageは姿勢・照準・移動にも使うため、単に装填中をスキップすると挙動が変わる。行動用LOSと発射直前LOSを分離し計測。効果大候補、危険度高、更新頻度変更は既定0切替が必要 |
| P03 高：長距離センサーの全兵走査と毎フレーム再構築 | `BattlePerception.cpp:65`〜`:81` はAlive/Gridを局所コンテナで毎回構築。混成ではActor分を作ってからResetして再構築。`:218`で格子セル数が多いと全Alive候補へ戻る。全員がこの経路なら毎秒2.5〜5N²候補訪問、例40万〜80万。LOSはその一部 | 混成判定を先にする、scratchの再利用、広い視界用の階層格子。順番・位相・候補採用が同じなら結果不変を狙える。危険度中。センサー頻度を落とす案は別途既定0 |
| P04 高：知識配信の三重積 | `BattlePerception.cpp:344`〜`:385` は更新送信者×近傍受信者×直接可視接触。PendingCountsで待機重複は防いでいる。400人×5Hz×受信者20×接触30なら約120万照会/秒。ShareCells>4では受信者抽出も全Aliveへ戻る | 到達範囲・同一時刻の接触集合を再利用、配信順を保つキュー索引。効果は密度依存、危険度中。配信量やDelay変更は結果変更、既定0が必要 |
| P05 高：毎フレームの部隊一覧確保・ソート | `BattleTeamAI.cpp:28` は1Hzのreturn (`:54`) より前でGetKeys/Sort。`BattleScoutReport.cpp:75` も毎フレーム。Q=50で各約3000キー抽出/秒＋ソート、双方で約6000。後者は非偵察部隊の全員について監視所確認もする | 権限掃除を残し、部隊集合変更時だけID一覧を更新。再利用配列は可変状態の参照を長期保持しない。効果小〜中、危険度低〜中、順番維持なら不変 |
| P06 高：戦略の陣営共通計算が分隊ごとに反復 | `BattleMatch.cpp:287`, `:291`, `:298`、`BattleGroups.cpp:244`、`BattleSquadRetreat.cpp:115`。AreaIntent・重み・前線均衡の自軍集計をAreaStrategyTargetのたびに作る。前線均衡ONなら1回O(NO)、全Q部隊でO(QNO)。例5秒更新1巡で最大18万の位置対拠点比較。帰還判定の1Hz呼び出しは別 | 同じ入力時点の陣営配分スナップショットを作りslotだけ個別計算。効果中、危険度中。途中で任務・所属が更新される箇所を跨ぐキャッシュは結果変更の恐れがあり既定0で比較 |
| P07 高：砲兵・主砲・外骨格で敵集団を再計算 | `BattleArtillery.cpp:215`〜`:229` はO(K²)を各砲0.75秒。A=8、K=100で約10.7万距離比較/秒＋ソート＋最大8Nの味方検査/選択。`BattleTankGun.cpp:50`〜`:58` は砲ごと0.5秒、`BattleExo.cpp:298`〜`:305` は射手ごと0.5秒に類似処理 | 陣営観測の同一版で集団特徴を共有。ただし自分の直接接触の優先、爆風半径、重心か敵位置か、最小人数が異なる。効果中〜大、危険度中。既存Clustersは結果も変えるので既定0。完全同値の集計再利用と分ける |
| P08 高：遮蔽候補はLimit適用前に全件検索・ソート | `BattleCover.cpp:159`〜`:164` はO(C)+O(k log k)後に件数を切る。`BattleCoverAction.cpp:95` の都市候補12は探索全体の上限ではない。各候補でSweep/露出検査/最大4覗き位置のNav投影等。優先枠は4F、通常予算ONならさらに既定2Fの最大要求枠（60fpsで最大360要求/秒、再試行条件により実際は減る） | 静的遮蔽格子と上位k選択。距離同点の現行順を保持すること。効果大候補、危険度中。同値なら不変、探索件数や順序を変えるなら既定0 |
| P09 高：到着配置のNavが分隊一括で集中 | `BattleLeaderRuntime.cpp:755`〜`:797`、`BattleStableMovement.cpp:112`, `:137`, `:151`。再計画ごと625格子候補を生成、遮蔽候補は通常48/区画256、遮蔽末尾経路だけでも最大24回の同期探索。50部隊同時ならその部分だけ最大1200回。別の末尾経路・IssueOrderでの検証もある | 既存ThinkCapの評価、計画の段階分割、同一Nav条件の検証結果再利用。初回位相はあるが命令更新がNextThink=0にする (`BattleLeaderRuntime.cpp:582`)。危険度中〜高。分散すると命令が効くフレームが変わるため既定0 |
| P10 高：拠点観測と補給探索 | `BattleArea.cpp:217`〜`:220` の観測は5Hz、O(N(O+1))、例最大2万候補/秒。全員視認不可の場合はbreakが効かない。`BattleMatch.cpp:214` の占領はO(NO)毎フレーム、例21.6万候補/秒。`BattleArea.cpp:277`→`:79`のSupplyTeamNearは各分隊から分隊検索、O(Q²)/秒 | 占領はルールなので周期を落とさず、同フレームの区画所属集計を共有。補給班一覧を型別に索引。観測は距離候補抽出までの最適化。危険度低〜中、結果不変を狙える。観測周期変更は既定0 |

既に対策がある箇所もある：認識走査は全員毎フレームではない、共有報告にはPendingCounts、通常近接回避には格子と32件上限、砲兵再選択には間隔、個人迂回の同期経路探索にはフレーム予算がある (`BattleStableMovement.cpp:508`〜`:516`)。これらを無視して「常時全N²」とは評価しない。

## 4. 重複・無駄・未使用の整理

| 指摘・確信度 | 根拠と具体例 | 共通化・扱い／効果・危険度・既定結果 |
|---|---|---|
| D01 高：旧ActorとPhase3の分隊投票処理が重複 | `BattleSquadAI.cpp:41`〜`:85` と `BattleMixedSquadAI.cpp:16`〜`:60`。部隊員選別、Votes/Latest/Visible、既存目標保持をほぼ同形で実装 | Actor/Proxyアクセスだけアダプタにし、接触スナップショットから役割計画を返す共通評価へ。効果は修正漏れ防止、危険度中。既定結果は不変を目標。通常広域Thinkを無理に同一化しない |
| D02 高：兵種目標数の式が旧編成とPlannerに重複 | `BattleArmourAI.cpp:234` と `BattleCompositionRuntime.cpp:98`〜`:104`。購入数6/9/12等の段階と兵種上限を双方で管理 | 入力を解決済み自軍購入数・観測車両数・明示overrideにして共通純関数へ。認識収集は分離を維持。効果は調整漏れ防止、危険度低〜中、同値なら不変 |
| D03 高：局地戦力集計をThink内で重複 | `BattleLeaderRuntime.cpp:656`〜`:665` と `:695`〜`:704`。部隊の射程内直接接触集合と30m以内の友軍人数を遮蔽停止・接近で再集計 | 同じThink内の必要時に一度だけ算出し、判断条件は別に残す。効果小〜中、危険度低、同値なら不変 |
| D04 高：命令フィルタが各武器・目標選択に散在 | `BattleWeaponOrders.cpp:20`, `BattleLeaderRuntime.cpp:608`, `BattleArmourAI.cpp:64`, `BattleExo.cpp:301`, `BattleArtillery.cpp:281`、主砲はF04の欠落 | 許可理由つき候補評価を共通化。任務と射程、識別と実弾トレースを混ぜない。効果大、危険度中〜高、挙動を揃える変更は既定0 |
| D05 高：軍事状態を表示用文字列で判定 | `BattleGroups.cpp:285`、`BattleSquadRetreat.cpp:130`, `:151` はReason.StartsWith("HOME SUPPLY")で補給中か判断 | TacticalPlanに補給目的のenumを持たせ、Reasonはログ専用。文言変更が群・補給判断を変えることを防ぐ。危険度中（保存移行）、判定を同値にすれば既定結果不変 |
| D06 高：補給再開で同一注文を2回検証 | `BattleSquadRetreat.cpp:139` → `BattleLeaderRuntime.cpp:547`〜`:555`。最初はMission更新保留、取消後にもう一度実行 | 検証済み計画の適用APIへ分離。同期Navの重複削減。危険度中、同値維持が必要。F02の修正とは別差分で比較 |
| D07 高：到着配置への射影前・後の値、可視状態と射撃可否の責務が混在 | `BattleCoverAction.cpp:60`〜`:74`, `:128`〜`:140`、`BattleLeaderRuntime.cpp:887`。PeekRangeCheckは既存の既定0試験 | 覗き候補の投影・距離・移動可能性を一つの結果型にする。選択時と再検証で同じ仕様を使う。効果は境界条件の揃え、危険度中、範囲制限変更は既定0 |

未使用・定数分岐の確認結果：

- F09のLeaderAbilityScaleはSource/Config内で参照なし。ただし反射・アセット未調査なので即削除対象とはしない。
- `BattleComposition.h:23` のAllowedGroupRolesは `BattleComposition.cpp:30` で設定され、`:116` のログと `BattleAreaSmoke.cpp:71` の検査で使う。現状Plannerが候補の可否を決める入力には使っていない。「全く未使用」ではなく診断・将来拡張の定義。
- `BattleLeaderRuntime.cpp:900` の `U->CoverPhase` を伴う退出はF03の早期returnにより実質的に狙った状況で到達できない。`CoverApproaches.Contains` 側は生きているため、分岐全体をdead codeとはしない。
- 主要AIクラスの定義名を宣言・参照と照合した範囲で、確実に呼ばれていない関数は特定できなかった。UpdateMixedや旧支援砲撃には別経路の呼び出しがある。文字列一致だけではUE反射やdelegate接続の不使用を証明できない。
- 固定初期値のままのフィールドと、起動時static constで解決した試験切替は別物。後者は引数により結果が変わるため、常に同じ分岐として除去しない。

## 5. 改善の優先順

挙動を変える改善は、既存の該当試験切替の中で直すか、新しい既定0切替を設ける。今回はいずれも提案のみ。性能改善でも評価順・更新時刻・同点選択を変えれば結果が変わる。

| 優先 | 作業 | 効果 | 危険度 | 既定試合への影響と導入方法 |
|---|---|---|---|---|
| 1 | F01の認識依存を切る | 未評価Plannerの基本入力を正常化 | 中 | Planner OFFでは不変。Planner対象内で評価、GroupStrength既定化で代用しない |
| 2 | F02の元任務復元 | 補給後の区画任務消失を防ぐ | 中 | 区画使用時に変わる。修正切替既定0 |
| 3 | F03の状態実行前の裁定 | 遮蔽中の交互前進が実行できる | 中 | Bounding既定0を維持してON経路を修正 |
| 4 | F04/F05/D04の命令許可統一 | 武器・兵種による任務逸脱を減らす | 中〜高 | WeaponOrderGate/ArmourSectorGate既定0で比較。主砲も対象にする |
| 5 | F06の緊急任務解除、F10の任務種別整理 | 新規購入の拘束・層間の意味ずれを防ぐ | 中 | Planner内変更と通常戦術変更を分け、後者は新切替既定0 |
| 6 | P01/P08の遮蔽索引 | 毎フレーム全予約走査と全slotソートを削減 | 中 | 同値を目標。候補順・予約競合が変わるなら既定0 |
| 7 | P03/P05/P10の確保と索引 | 継続的な小コストと重複集計を減らす | 低〜中 | 更新時点・順序維持で不変を目標 |
| 8 | P09の計画生成の集中対策 | 開始・命令変更時の負荷ピークを減らす | 中〜高 | ThinkCap等既定0。後回しにする分隊の公平性も確認 |
| 9 | F07/F08/P07の砲兵改善 | 認識と候補品質、敵集団計算の共有 | 中〜高 | ArtilleryObserved/Clusters既定0、観測修正と計算最適化を分ける |
| 10 | P02の行動用LOS・発射用LOS分離 | 高頻度トレース削減 | 高 | 行動も依存するため新切替既定0で比較 |
| 11 | D01/D02/D03/D05の共通化、F09の明文化 | 修正漏れ・意図誤認の防止 | 低〜中 | 同値の整理を基本とし、保存変更は別途往復検証 |

## 6. 次回以降の点検で優先する場所

### 既定値の守り

- `BattleTactics.cpp`、`BattleGroups.cpp:72`、`BattleArmourAI.cpp:28`、`BattleLeaderRuntime.cpp:1417`以降、`BattleWeaponOrders.cpp:8`、`BattleComposition.cpp:8`、`DefaultGame.ini`、Setupの起動スクリプト。C++初期値、ini、引数、片側試験の対象陣営の順に実効値を並べる。STATUSの旧要約から既定値を推定しない。
- Planner ON/GroupStrength OFF、SectorAI ON/ArmourSectorGate OFF、WeaponOrderGate ON/主砲、CoverActions ON/Bounding ONの組合せを重点にする。
- F02は区画AIの自動命令だけでなく手動区画命令の補給再開も対象。既定試合とユーザー操作の経路を分ける。

### 観測情報ルール

- `BattleAreaIntel.cpp:34` の距離だけのbHomeCovered（F07）。`BattleArmourAI.cpp:107` のNoteKnownVehicles、`BattleArtillery.cpp:167`, `:186`、`BattleTankGun.cpp:53` のIsVehicle/VehicleTypeが、現在の実体分類をいつ読んでいるか。認識時の識別情報と、古い接触に対する真値照会を区別する。
- `BattleLeaderRuntime.cpp:643`〜`:647` は対象位置を「最初に見つけた接触」から読む。選択を成立させた新しい可視接触と同じとは限らない。時刻・bVisible・bSharedを含む接触選択の一貫性を調べる（現象は未再現）。
- `BattleLeaderRuntime.cpp:1051` のSideExposure/TargetAimOffsetは射撃照準の現在姿勢利用。センサー測定・弾道衝突に必要な真値と、AIの戦術判断に入れる真値を別々に監査する。
- `BattleMatch.cpp:214` の占領、`BattleArea.cpp:203` の収入、`:274`の補給はゲームルール。正規状態を読んでいるだけでAI違反と決めない。そこから判断層へ戻る値の公開範囲を追う。

### 再現性

- `BattleLeaderRuntime.cpp:1358`, `:1369` のTMap順と射撃中の死亡、`BattleCover.cpp:163` の距離同点に明示tie-breakがないソート、`BattleAreaIntel.cpp:39` のSeen.Addによる同じ敵の最後の書き込み、`BattleTeamAI.cpp:28`等の明示ソートの境界。
- ThinkCapによる先頭優先、FPS依存の遮蔽予算（2件＋優先4件/フレーム）、`Follow`へのDt上限0.05と世界時計、5秒時計を0へ戻す処理。30/60FPSで判断・移動・予約失効が同じかを別レビューにする。
- 認識ParallelForは適用順を保っている一方、FollowSightParallelは全員が動く前の位置で判定する別仕様 (`BattleLeaderRuntime.cpp:801`)。単なる同値高速化として既定化しない。

### 保存・読み込み

- `BattleSave.cpp:128`, `:214`、`BattleTacticsSave.cpp:4`、`BattleCompositionSave.cpp:4`、`BattleCompositionPlanner.cpp:176`。Mission/実行Order/区画フラグ、CoverPhase・予約再取得、群の時刻、砲弾・主砲・ランチャーの選択・射撃番号、Planner観測時刻を横断する。
- `BattleCoverAction.cpp:386` の予約再構築は同時復元順で競合する可能性。`BattleSectorRoute.cpp:137` の世界時計キャッシュ、`BattleAreaIntel.cpp:22` の試合時計キャッシュ、NextThink/NextPick/NextSampleが復元後の時計へ戻されるか。
- F09のUPROPERTY削除は保存・反射互換を確認するまで行わない。過去のInvalid cover peek distanceの修正済み履歴と、新規Peek投影範囲問題を混同しない。

### 性能

- P01/P02/P08/P09を最初の計測候補とする。認識、Follow、Cover、Nav、描画を別区間で測る。実験用 `BattleStableMovement.cpp:337`〜`:357` の全兵診断走査や、`BattleCoverAction.cpp:455` の追加LOSは試験ON時だけなので通常コストと混ぜない。
- `BattleCompositionRuntime.cpp:128`〜`:138` の全候補ログは5秒ごと、`BattleScoutReport.cpp:51`以降には車両等の診断ログ。試験でログ量とIOの影響を区別する。
- 新たな計測時にはMEASURE.mdに従い、開始前のGet-MeasurementMachine結果をmanifest.machineへ保存し、描画GPUをUEログと照合する。本点検では計測していないため、CPU/GPUやms単位の改善量を推定値で埋めない。

## 完了範囲

層の地図、呼び出し頻度、入力・出力、10件の問題、10項目の負荷候補、重複・未使用候補、優先順位、次回監査メモを記載した。指摘は静的レビューの結果であり、修正・試験合格・勝敗改善を報告するものではない。
