# 試験の対応表：何をどの試験で判断するか

2026-11-25 作成（系統「評価の道具」。下調べは Codex の読み取り調査、要点は Claude がコードで確認。main `fefc4bb` 時点）。
対象は `Game/FutureBattle/Source/FutureBattle/Prototype/*Smoke*.cpp` 25本と `Setup/Test-*.ps1` 40本（`Setup/Legacy/` は除く）。手順は `MEASURE.md`、通しの試遊は `Docs/playtest-walkthrough.md`。試験を足した・入口を変えたら、この表の行を直す。

## 1. 4つの合格条件と、それぞれを判断する試験

採否は AI 同士の勝敗だけで決めない。次の4つを別々の合格条件にする（`Docs/review-2026-11-25/05-standard-practice-comparison.md`）。

| 合格条件 | 問うこと | 判断に使う試験 | 使わないもの |
|---|---|---|---|
| **契約** | 購入・命令・権限が約束どおり動くか（勝敗と関係なく正しさが決まる） | `Test-Battle`（基本回帰）、`Test-Area`、`Test-MixedFast`、`Test-Delegation`、`Test-SquadTactics`、`Test-CoverActions`、B06 の自己試験 `-BattleMissionResumeSmoke` | 長い試合の得点（契約の合否は得点で決めない） |
| **局所の挙動** | 短い固定場面で、1つの行動が成立するか（着く・隠れる・撃つ・退く） | `Test-MovementStability`、`Test-ContactMovement`、`Test-CoverActions`、`Test-SquadRetreat`、`Test-TeamSupport`、`Test-SquadFlank`、`Test-SightTrial`、`Test-FlankBay`、`Test-LeaderPhase3` | 「長い試合で発動回数が増えた」だけ（診断の数が動いたことは、ねらった行動が成立した証拠ではない。11-22 の歩行中の視線の教訓） |
| **通常の試合** | 先に決めた試合の全部で、基準（切替OFF）との差がプラスか | `Test-WideSight -Area -Experiment -Fps 30`（`Run-EconomyMatrix`・`Run-StrategyScreen` が束ねる）→ `Analyze-MatchDelta.py`（条件の一致・発動回数・得点と損害の内訳）、`Compare-MatchResults.py`（結果を変えない変更の確認） | 発動0回の試合を除いた集計、得点だけの比較、FPS や側が違う結果どうしの比較 |
| **操作の体験** | プレイヤーが指示を出せるか・意図を読めるか・FPS 中に指揮へ戻りたくなるか | `Docs/playtest-walkthrough.md`（ユーザーの試遊）。自動で先に確かめられるのは入力と表示の成立だけ：`Test-BuyPanelShot`、`Test-QuitMenuShot`、`Test-RangeRingShot`、`Test-TPSControl`／`Visibility`／`Avoidance`、`Test-ScopeShot`、`Test-StanceShot`、`Test-DeathShot`、`Test-ExoShot`、`Test-SoundFX` | 自動試験の合格（画像ができたこと・入力が届いたことは、見やすさ・分かりやすさの確認ではない） |

進め方：**契約 → 局所の挙動 → 通常の試合 → 操作の体験** の順に確かめ、前が成立していないものを後ろの数字で評価しない（例：補給へ着けない状態で補給車の購入量を試合で比べない）。

合格の行の読み方の注意：`failures=0` の意味は試験ごとに違う。`SIGHT_EXPERIMENT_RESULT failures=0`（通常の試合）と `BATTLE_MAP_STATS_RESULT failures=0`、`BATTLE_BAY_RESULT failures=0` は、主に「完走して結果を書けた」で、行動の良し悪しは判定していない。`BATTLE_MOVE_STALL` は記録で、出ても不合格にはならない。

## 2. 試験ごとの対応（契＝契約、局＝局所の挙動、試＝通常の試合、操＝操作の体験）

分類は検査の中身からの割り当て。操の印は「入力・表示が成立する」までで、人が見て確かめたことを意味しない。

### 2.1 `Setup/Test-*.ps1`

| 試験 | 契 | 局 | 試 | 操 | 判断すること | 合格の行 | マップ・長さ |
|---|---|---|---|---|---|---|---|
| Test-Battle | ○ | ○ | | △ | 基本回帰：命令の実行、知覚、戦術効果、協同、訓練、購入・拠点、支援攻撃（Smoke を順に。保存の Smoke は 11-25 に削除） | `BATTLE_SMOKE_RESULT failures=0` | 試作の小マップ、上限180秒 |
| Test-Area | ○ | ○ | | △ | エリア戦：補充・資源、購入と予約、B03（残す資源）、車両・砲兵・補給車の規則、収入と勝敗、購入の入力。編成AI のスモークも中で走る | `BATTLE_AREA_RESULT failures=0` | マップA 固定、上限180秒 |
| Test-MixedFast | ○ | ○ | | | 混成の購入・予算：課金は一度、人数、不正な購入の拒否、有料の補給、読込後の新ID | `BATTLE_MIXED_FAST_RESULT failures=0` | 500体の野原、約10秒 |
| Test-Delegation | ○ | ○ | | △ | 委任：選んだ小隊だけ委任、手動小隊の命令保持、直接操作で権限を奪われない、ボタン（720p・1080p） | `BATTLE_DELEGATION_RESULT failures=0` | 小マップ・野原、4ケース |
| Test-SquadTactics | ○ | ○ | | △ | 任務と一時の計画の分離、計画中の任務更新、計画の満了で復帰、観測の重複排除（守備の散開は検査しない） | `BATTLE_SMOKE_RESULT failures=0` | 小マップ／`-LargeField` |
| Test-CoverActions | ○ | ○ | | △ | 兵1人の遮蔽：保護位置へ着く、覗いて撃つ、制圧で隠れる、ブラインドファイア、壁遮蔽の出入り | `BATTLE_COVER_RESULT failures=0` | マップC／D、上限180秒 |
| Test-MovementStability | | ○ | | | 移動の4場面（密集・狭路・対向流・集合）：全員が1m以内に着く、到着枠の間隔、経路要求数、兵どうしの間隔 | `BATTLE_MOVE_RESULT failures=0` | 野原、各30秒まで |
| Test-ContactMovement | ○ | ○ | | | 接触の3場面：止まっている小隊の横を通る、到着の予約を別小隊が占めている、狭路で待って通る。止まっている側を勝手に動かさない | `BATTLE_CONTACT_RESULT failures=0` | 野原（場面の中に壁を作る）、各30秒まで |
| Test-SquadRetreat | ○ | ○ | (△) | | 撤退：被害・弾切れ・劣勢で退く、集合を待つ、安全になったら任務へ戻る、弾や HP を作らない。**B06 の自己試験は含まない** | `BATTLE_ST2_RESULT failures=0`（`-Soak` は `BATTLE_WIDE_SOAK_RESULT`） | 野原、約10秒（Soak 180秒） |
| Test-TeamSupport | ○ | ○ | (△) | | 小隊間の援護：近い適格な小隊を割り当て、重複なし、実弾で確認、期限・弾切れ・見失いで解除 | `BATTLE_SUPPORT_RESULT failures=0` | 野原、約7秒 |
| Test-SquadFlank | ○ | ○ | (△) | | 迂回：遮蔽のある経路は単独、露出する経路は援護が要る、未観測の真値を使わない、到着後に交戦へ戻る | `BATTLE_FLANK_RESULT failures=0` | 野原、約19秒 |
| Test-SightTrial | ○ | ○ | | | 視界と射程の境界：4つの距離、訓練別の視界、射程外で撃たない、弾1発の消費 | `BATTLE_SIGHT_RESULT failures=0 cases=8`／`BATTLE_WEAPON_RESULT failures=0 cases=12` | 野原、約30秒 |
| Test-FlankBay | | ○ | | | 45秒の固定の交戦（正面と側面）の数字を取る。優劣は別に集計する | `BATTLE_BAY_RESULT failures=0`（結果を書けた、の意味） | マップC、60FPS 固定 |
| Test-LeaderCommand／Test-VATAdoptionLeader | ○ | ○ | | △ | 隊長と Mass：選択と操作は隊長だけ、共有経路で Mass が動く、隊長の死亡、一覧のクリック | `BATTLE_LEADER_RESULT failures=0`／`BATTLE_LEADER_UI_RESULT failures=0` | 小マップ |
| Test-LeaderPhase3／Test-VATDisplay | ○ | ○ | | △ | 固定編成の経路・回避・LOD、Actor と Mass の射撃の一致、骨・アニメ・VAT の監査、1フレームの時間の記録 | `P3_RESULT failures=0` | 野原、10秒＋60秒 |
| Test-WideMixed | ○ | ○ | (△) | △ | 混成500体：選択・委任・命令の権限、共通の射撃、Mass の占領、操作の切替、隊長の死亡、やり直し | `BATTLE_WIDE_RESULT failures=0`（`-Soak` は別） | 野原、上限240秒 |
| Test-Scale／Test-LargeField | ○ | ○ | | △ | 多数の Actor：ID、命令の分散、移動、視点の切替で弾が増えない、3拠点の占領 | `BATTLE_SCALE_RESULT failures=0`／`BATTLE_LARGE_FIELD_RESULT failures=0` | 小マップ／野原 |
| Test-SquadTrial | ○ | ○ | | △ | 表現の3方式（Actor・個人・小隊）で発射数・命中数が一致する | `SQUAD_TRIAL … success=1` | 小マップ、600フレーム |
| Test-FastMatch | ○ | | ○ | | 古い形の試合：完走、資金が負にならない、ID の重複なし | `BATTLE_MATCH_RUN success=1` | 小マップ、200秒 |
| Test-Network／Test-NetworkSlots | ○ | | | △ | 接続、2陣営の割り当て、3人目の拒否、退出で停止、再接続、相手に見せない情報 | `BATTLE_NET_RESULT success=1`／`BATTLE_NET_SLOTS success=1 …` | 小マップ |
| Test-TPSControl／Visibility／Avoidance | ○ | ○ | | △ | 直接操作：カメラ、壁でカメラが下がる、目からの射撃、未発見の敵の表示と RTS へ戻すと隠す、HP・弾・命令の引き継ぎ | `BATTLE_TPS_RESULT failures=0`／`BATTLE_TPS_VISIBILITY_RESULT failures=0`＋子試験 | 小マップ・野原 |
| Test-BuyPanelShot | ○ | | | △ | 購入画面：行の当たり、購入、パッド、予約と買えるようになった後の配備、精鋭・狙撃の行 | `BATTLE_BUY_PANEL_SHOT` の各条件＋画像 | A・E〜K（既定 I）、描画あり |
| Test-QuitMenuShot | ○ | | | △ | 上部のボタン列、速度の切替、終了の確認 | `BATTLE_QUIT_SHOT` の各条件＋`QUIT_MENU quit` | 既定 I、描画あり |
| Test-RangeRingShot | | | | △ | 射程の円の画像を作る（中身は目で見る） | `BATTLE_RANGE_RING_SHOT` 2行＋画像 | 既定 I |
| Test-ScopeShot／StanceShot／DeathShot／ExoShot | (△) | △ | | △ | 構えとスコープの倍率・散らばり、姿勢のクリップ、死亡の表示、外骨格の部品と武器の構え | `SCOPE_`／`STANCE_`／`DEATH_`／`EXO_SHOT_RESULT failures=0` | 既定 E、描画あり |
| Test-SoundFX | | | | △ | 音と爆発の効果が出る（14種の音、3サイズの爆発） | `FX_CHECK_RESULT failures=0` | 既定 E、描画・音あり |
| Test-MapStats | | (△) | | | マップの形の数字（視線・通路の幅・遮蔽の密度・経路・車両の到達）。行動の試験ではない | `BATTLE_MAP_STATS_RESULT failures=0`＋`stats.json` | 引数、試合なし |
| Test-VATBake | | | | (△) | VAT の資産を作る・検証する工程。試験ではない | 終了コード0 | コマンドレット |
| Test-WideSight | (△) | (△) | ○ | (△) | 既定：地形・配置の対称・経路の監査。`-Experiment`：**通常の試合**（完走と結果の出力。得点・損害・補給などは `metrics.json` に記録するだけで、良し悪しは `Analyze-MatchDelta.py` で読む）。`-RoundTripSave` は 11-25 に廃止（指定すると止まる）。`-ExtraArgs` で自己試験の引数を渡せるが、その結果行は合否に入らない | `WIDE_AUDIT_RESULT`／`SIGHT_EXPERIMENT_RESULT`／`SIGHT_VISUAL_RESULT failures=0` | A〜L、試合は 600／900秒 |
| Test-MapHAfterPlay | | | ○ | | マップH の試遊の後の回帰（`Test-WideSight` の1試合、入力が同じなら再利用） | 子試験＋`result.json` の `passed` | マップH |

○＝その合格条件の判断に使う、△＝一部だけ（入力・表示の成立まで）、(△)＝枝や引数しだい。

### 2.2 スモーク（`Prototype/*Smoke*.cpp`）と入口

| スモーク | 入口の試験と引数 | 契 | 局 | 操 | 確かめること（要点） |
|---|---|---|---|---|---|
| BattleOrderSmoke | Test-Battle（All） | ○ | ○ | | 攻撃移動の停止と再開、Hold で古い移動を取り消す、到達できない命令の有限の再試行、防衛の半径へ戻る |
| BattlePerceptionSmoke／PerceptionOptimizationSmoke | Test-Battle（All） | ○ | ○ | △ | 視野・距離の境界、認識の遅れ、記憶、共有の遅れと範囲、未認識の敵の表示、格子と全探索の一致 |
| BattleTacticsSmoke | Test-Battle（All） | ○ | ○ | | 戦術効果の共通計算：照準の縮小、AI と直接操作で同じ損害、遮蔽の正面と側面 |
| BattleTacticalSmoke | Test-Battle `-Suite Tactical` | ○ | ○ | | 兵の遮蔽選び：到達できる候補、予約の競合、複数の兵が別の場所、守る範囲の外の遮蔽を拒否 |
| BattleSquadSmoke | Test-Battle `-Suite Squad` | ○ | ○ | | 援護役と移動役、迂回の到着、援護射撃の継続、元の命令の保持 |
| BattleAbilitySmoke | Test-Battle `-Suite Ability` | ○ | ○ | | 訓練別の価格・認識の遅れ・判断の頻度、射撃技量の独立、壁を透視しない |
| BattleMatchSmoke → ObjectiveSmoke → SupportSmoke | Test-Battle `-Suite Match` | ○ | ○ | △ | 購入と開始の条件、占領と得点、拠点の補給（古い形）、有料の支援攻撃 |
| BattleSquadTacticsSmoke | Test-SquadTactics | ○ | ○ | △ | 任務と計画の分離、観測（2.1 と同じ） |
| BattleDelegationSmoke | Test-Delegation | ○ | ○ | △ | 委任と権限（2.1 と同じ） |
| BattleAreaSmoke（中で BattleCompositionSmoke） | Test-Area `-BattleAreaSmoke` | ○ | ○ | △ | エリア戦の規則、編成AI の貯蓄・購入 |
| BattleAreaSmoke の `RunSectorSmoke` | **入口の試験なし**（引数 `-BattleSectorSmoke`、区画のあるマップ） | ○ | ○ | | 3小隊への区画の命令、到着の割り当てが区画の中、次の命令で反撃移動が解ける（実際の到着は待たない）。合格の行 `BATTLE_SECTOR_RESULT failures=0 orders=3` |
| BattleMixedFastSmoke | Test-MixedFast | ○ | ○ | △ | 2.1 と同じ |
| BattleCoverActionSmoke | Test-CoverActions | ○ | ○ | △ | 2.1 と同じ |
| BattleSquadRetreatSmoke（ST2） | Test-SquadRetreat `-BattleST2Smoke` | ○ | ○ | | 2.1 と同じ |
| BattleSquadRetreatSmoke の `RunMissionResumeSmoke`（B06） | **入口の試験なし**（引数 `-BattleMissionResumeSmoke`。`Test-WideSight -ExtraArgs` で渡せるが合否に入らない） | ○ | | | 一時の計画から戻るとき、命令・目的地・区画の属性を保ち、命令の版がちょうど1増える。2回目は何もしない。計画中に更新された任務へ戻る。Hold の後は戻らない。合格の行 `BATTLE_MISSION_RESUME_SMOKE_RESULT failures=0` |
| BattleTeamSupportSmoke／SquadFlankSmoke | Test-TeamSupport／Test-SquadFlank | ○ | ○ | | 2.1 と同じ |
| BattleLeaderSmoke | Test-LeaderCommand ほか | ○ | ○ | △ | 2.1 と同じ |
| BattleThirdPersonSmoke | Test-TPSControl ほか | ○ | ○ | △ | 2.1 と同じ |
| BattleScaleSmoke | Test-Scale | ○ | ○ | △ | 2.1 と同じ |
| BattleWideMixedSmoke | Test-WideMixed（`-Soak` は撤退・援護・迂回の Soak からも） | ○ | ○ | △ | 2.1 と同じ |

ファイルの単位では、25本すべてにどれかの `Test-*.ps1` からの入口がある。入口が無いのは上の太字の2つ（同じファイルの中の別の自己試験）。

## 3. 足すべき小さな固定場面3つ：今あるもので足りる所と無い所

作るのは各担当の系統。ここは一覧まで（新しい試験の土台は作らず、今のスモークの仕組みに載せる）。

| 固定場面 | 今ある試験が確かめていること | 無いもの | 担当の系統 |
|---|---|---|---|
| **壁で分断された小隊** | `Test-MovementStability`：まとまった隊形から出て全員が着く（狭路の壁は経路網に影響しない壁、`BattleMovementTrial.cpp:38`）。`Test-ContactMovement`：止まった小隊・予約の占有・狭路の待ち。`BattleOrderSmoke`：古い Actor の形での物理の停止→再集合。通常の試合の `BATTLE_MOVE_STALL`・`move_own_route`・`move_cutoff_route` は数の記録だけ | 同じ小隊の一部を**本物の壁（経路網に効く壁）の反対側**に置き、小隊の再計画を何回か起こして、壁へ戻らずに合流まで終わることを合否にする場面。`-BattleCutOffRoute=1` をどの `Test-*.ps1` も渡していない。通常の移動と補給の戻りの両方 | 移動の詰まり（C24） |
| **遮蔽へ置く守備小隊** | `Test-CoverActions`：兵1人（直接操作の Actor と Mass の部下1人）へ遮蔽の要求を直接出し、保護位置へ着く・覗く・隠れる・保存。`BattleTacticalSmoke`：複数の兵が別の遮蔽を予約、守る範囲の外を拒否（古い Actor の形）。`RunSectorSmoke`：区画の命令の受け付けと到着の割り当て（到着・滞在は待たない。入口の試験も無い） | 守る命令から、小隊が2〜3組に分かれて遮蔽を選び、**全員が着いて一定の時間そこに留まる**ことを合否にする場面。遮蔽の取り合い、置けないときの扱い、命令し直したときの置き直し。配置の本体（`BattleHotSectors.cpp`）は main に無く arms の作業ツリーにある | 武器と高級小隊（arms、守りの配置） |
| **補給から戻る小隊** | B06 の自己試験：復帰の関数の契約（命令＋目的地＋区画の属性、一度だけ、新しい任務を優先、Hold の後は戻らない）。`Test-SquadRetreat`：弾切れの**観測を渡して**撤退→集合→回復、弾を作らない、戻る。`Test-Area`：補給で戻る計画が元の目的地を保つ、補給車の価格・範囲（20m は可、40m・敵は不可）・空なら本拠地へ・積み直し。通常の試合：補給の回数・往復の時間の記録（`supply_cycle`） | **実際に弾を減らす → 歩く → 補給が成立 → 最新の命令＋目的地＋区画の属性へ一度だけ戻る → 元の区画へ着く**、を本拠地・補給車・味方の拠点（`-BattleAreaPointSupply`）の3通りで通す場面。B06 の自己試験を合否に入れる入口（`Test-SquadRetreat` に引数の枝を足すか、`Test-Area` と同じ形の専用の入口） | 契約の修理（入口）／弾薬と補給（往復の場面） |

## 4. 手順の文書に載っていない試験

`MEASURE.md`「2. 専用試験」の表にあるのは13本。次は表に無い（1章の入口の表にある `Test-Battle`・`Test-WideSight`・`Test-MapStats` を除く）ので、変えた箇所に当たるときはこの文書で探す：`Test-CoverActions`（遮蔽の動作）、`Test-Delegation`、`Test-SquadTactics`、`Test-WideMixed`、`Test-Scale`、`Test-LargeField`、`Test-SightTrial`、`Test-FlankBay`、`Test-SquadTrial`、`Test-FastMatch`、`Test-Network`、`Test-NetworkSlots`、`Test-VATAdoptionLeader`、`Test-VATBake`、`Test-MapHAfterPlay`、画面の撮影の9本（`Test-BuyPanelShot`・`QuitMenuShot`・`RangeRingShot`・`ScopeShot`・`StanceShot`・`DeathShot`・`ExoShot`・`SoundFX`）。

変えた箇所から引く早見：

| 変えた箇所 | まず回す試験（契約・局所） | 試合で見るとき |
|---|---|---|
| 購入・予約・資源・補給車・勝敗 | `Test-Area`、購入画面は `Test-BuyPanelShot` | 発動：`BATTLE_AREA_HIRE`、`BATTLE_HIRE_RESERVE op=buy`、`BATTLE_COMPOSITION_EXECUTE success=1` |
| 撤退・補給の戻り・任務の復帰 | `Test-SquadRetreat`、`-BattleMissionResumeSmoke`（結果行を目で確かめる） | 発動：`BATTLE_MISSION_RESUME`、`supply_cycle` |
| 移動・経路・詰まり | `Test-MovementStability`、`Test-ContactMovement` | `BATTLE_MOVE_STALL`、`move_own_route`、`move_cutoff_route`（数の記録） |
| 遮蔽・躍進・突撃・手りゅう弾 | `Test-CoverActions`、`-BattleArmsSmoke` | 発動：`BATTLE_ARMS_ASSAULT`／`CHARGE`／`GRENADE`／`MELEE` |
| 前線・区間・重点・群 | `-BattleFrontOrderScript`（区間の前進・後退の自動確認） | 発動：`BATTLE_FRONT_FOCUS2_GROUP`、`BATTLE_FRONT_ORDER_STEP`、`BATTLE_FRONT_PLAN` |
| 直接操作・表示 | `Test-TPSControl`／`Visibility`／`Avoidance`、撮影の試験 | 試遊（`Docs/playtest-walkthrough.md`） |
