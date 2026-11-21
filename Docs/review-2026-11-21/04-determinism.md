# FutureBattle 総点検 04：再現性（決定性）

レビュー日：2026-11-21。対象HEAD：`9333675be8ef2cc1cbb10990a86a9efa966c97fa`。静的レビューのみ。

## 対象と結論

先行 `01-ai-layers.md`、`02-defaults.md`、`03-observation.md` の報告と「後続の点検／再現性」を出発点に、STATUS冒頭・直近項目、CODEMAP、MEASURE、実装・比較スクリプトを確認した。以下の `Battle*.cpp:行` / `.h:行` は原則 `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。先行報告とHEADが異なるため現在のコードを優先する。特に先行01 F01／02 D03のPlanner識別問題は、現在の `BattlePerception.cpp:64–65` ではPlannerも識別取得条件に入っている。

**同一条件での新規試合の反復、保存なし対保存往復、30対60fpsは、別々の保証である。現状のIDENTICALはどれについても内部状態の完全一致を証明しない。** 保存往復で判断・移動・姿勢・予約を作り直す経路は確定しており、厳密な継続再現の前提を満たしていない。また比較器には、実時間由来の値が比較に残る誤検出経路もある。

- 同じ整数キーのTMapは、同じ操作履歴なら通常同じ列挙になる。TMapが存在するだけで毎回ランダムになるとは判定しない。本報告では、補充・削除・再構築で履歴が変わる入口と、順序を結果へ変換する利用側を示す。
- 同点比較の不足も、同一配列・同一実装なら毎回揺れるという意味ではない。入力列が変わった場合の非同値と、最適化・エンジン変更で選択が変わる余地である。
- 確信度「高」はコードの条件・伝播を確認した意味。具体的seedでの発生、頻度、勝敗差は未検証。「中」は境界値・エンジン・環境次第の推測を含む。
- ビルド、UE起動、試合、試験、計測、生成器は実行していない。開始時の `git status --short -uno` に追跡ファイルの変更表示なし（個人ignoreへのアクセス警告あり）。書き込みは本ファイルだけ。STATUS、journalは変更しない。

MEASUREは標準60fps・保存往復ありとし、30fpsは一次選別だけ、保存なしでは結果が変わるとも明記している。STATUS直近にも既存の往復差の記録がある。これらは今回実測した証拠ではなく、現在の要求「往復でも不変」と既存運用の差を示す資料として扱う。過去のIDENTICALには「変更前の往復あり対変更後の往復あり」もあり、「往復なし対あり」の同値を意味しない。

## 1. 確信度高の指摘

### R01：IDENTICALに実時間由来の3項目が残る（誤検出）

**確信度：高。** 根拠：`Setup/Compare-MatchResults.py:10–14,39–40`、`BattleMetrics.cpp:79,138–140`、`BattleSightExperiment.cpp:164,291`。

状況→結果：`decisions_per_second`、`sight_rays_per_second`、`path_queries_per_second` は `FPlatformTime::Seconds()-StartedAt` で割った値。比較器はduration自体と名前に `_ms` を含むキー等を除くが、この3項目を除かない。非零の件数が一致しても、CPU負荷・並列実行・保存IO等で実時間が違えば **DIFFERENT** となる。Experiment側でこの3項目を試合時間に置換／削除する処理はない。

IDENTICAL検出：すり抜けではなく偽陽性。「勝敗・弾数が同じでも非決定的な試合」と誤解させる。現在のソースの指摘であり、過去の凍結DLL・出力が同じフィールドを出していたかは本レビューで認定しない。

改善：実時間の率を性能セクションに隔離し、ゲーム時間で割った率と区別する。比較用schemaに除外項目を明示する。**効果：性能揺れの誤検出を除去。危険度：低。既定試合の結果：不変、比較結果のみ変わる。ゲーム挙動切替は不要。**

### R02：比較は統計の一致であり、完全状態・条件の一致を検査しない

**確信度：高。** 根拠：`Setup/Compare-MatchResults.py:17–20,39–47`、`BattleSightExperiment.cpp:165–177,231–247,287–290`、`Setup/Test-WideSight.ps1:33–43,110–113,143–145`。

状況→結果：比較器はmetrics.jsonだけを読み、manifest、起動引数、実効設定、ビルド／Contentを照合しない。最終個体別HP・弾薬・位置・命令・Contacts・次回判断・予約・全射撃イベント列もmetricsにはない。同じ集計・距離列になる別の内部状態はIDENTICALを通る。JSON辞書の順序は無視される一方、距離配列の順序は比較される。したがって射手IDが入れ替わって同じ距離を撃つ例は隠れ、同じ距離の集合でも並びだけ違えば差になる。

`is_timing` はトップレベル名に `_ms` を含むものを一括除外する。将来 `deadline_ms` 等のゲーム時計を足すと検査対象から落ちる。`a.get(k)` と `b.get(k)` の比較では「片方にキーなし」と「片方にnull」も一致扱いになる。空のJSONオブジェクト2つも比較器単体ではIDENTICALになり、完走・schemaの必須キー検証はない。

IDENTICAL検出：内部差・未出力の設定差はすり抜ける。伝播して `shots`、`shot_distances_cm`、`winner`、得点、`counts` 等が変わった場合だけ検出できる。`mixed_save_round_trip_verified` がある通常出力では往復の有無自体も差になるため、現行比較器で無加工の「なし対あり」をそのまま同値要求するのも不適切。

改善：実効ルールとマップ／資産・実行条件の署名、必須schema・完走検証、UnitID順の正規状態と隠れた継続状態のフレーム別ハッシュ、射手ID・対象ID・時刻付きイベント列を追加する。診断・性能・試合状態を比較モードで分け、往復有無は意図的に異なる入力として許す。**効果：最初の分岐点を検出し、誤った同条件認定を防ぐ。危険度：低〜中（記録量、署名仕様）。既定試合：読み取り記録のみなら不変。既存のIDENTICAL判定は厳しくなる。**

### R03：補充後のBodies順が、保存復元でID順へ変わる

**確信度：高。** 根拠：`BattleMixedRecruitment.cpp:30–37`、`BattleSave.cpp:192–207`、`BattleMixedBodies.cpp:112–123,131,147–165`、`BattleState.cpp:52–65`、`BattleLeaderRuntime.cpp:1358–1369`、`BattlePerception.cpp:75–84,230–231`。

状況→結果：死んだ部下を補充すると旧BodiesキーをRemoveし、新しい大きなUnitIDをAddする。空きスロット再利用により、新IDが古い兵の位置で列挙され得る。保存はUnitsをID昇順にSortし、復元はその順でStagedへAddする。非retiredの同じ兵集合でも、往復前後でBodiesの順が変わる。

Followはその順で移動・射撃する。Aの先行射撃でBが死亡するとBはそのフレームに撃てず、逆順では逆の結果になり得る。移動中の衝突・味方遮蔽も先行兵の更新後を読む。認識候補のOrdinalもBodies順から作るため、OrdinalでSortする既存の最適化はID順への正規化ではない。Plansも死亡等で除去・追加される側と、復元時の分隊昇順再作成で判断順が変わり得る。

IDENTICAL検出：発射数・距離・撃破・経路・予算割当に伝播すれば検出。順序だけ、または未記録状態だけの差なら通る。保存往復を両比較群で同様に行う試験は、この不連続を共通に含み得る。

改善：ゲーム処理順をUnitID／SquadIDの明示列として固定し、保存復元も同じ順序契約にする。**効果：コンテナ履歴から射撃・移動・判断を分離。危険度：高（戦闘の先後が変わる）。既定試合：変わり得るため試験切替既定0。純粋な決定性修正でも、現状たまたま安定した補充順を変えると基準結果は変わる。**

### R04：認識の走査予定を捨て、復元時だけ全員を追加走査する

**確信度：高。** 根拠：`BattlePerception.cpp:10–34,181–207,317–340`、`BattlePerception.h:55`、`BattleSave.cpp:314–316`、`BattleTacticsSave.cpp:14–15`。

状況→結果：CaptureはContacts、DetectionStarted、PendingReportsを保存するがNextScanTimes／LastScanTimesを保存しない。Restoreは両時計をResetし、ContactsをbVisible=falseへ変える。その直後の `Update(Mode->Units,Now)` は既定SliceCount=1なので、通常混成のSliceCount=0による期限判定を通さず全員を走査・報告配信する。次の通常UpdateはNextScanTimesがないため次の0.2秒位相を設定してcontinueする。

次回走査まで0.1秒残っていた兵でもロード時に敵を再取得し得る。逆に以前の可視対象が現在壁の裏なら即時に不可視となる。通常は0.4秒間隔で待つ未発見兵も走査が前倒しされる。戦術SamplesのContactもbVisible=falseになるが、その標本はPerceptionの追加走査では直接更新されない。

IDENTICAL検出：`counts.perception_updates/perception_observers/reports_delivered` 等に追加仕事が現れ得る。目標・射撃が変われば他の比較キーにも出る。次回予定そのものや同じ集計内の観測差は通る。Soakはこの一致を検証しない（R12）。

改善：次回走査までの残りと可視スナップショットを保持し、復元を新たな観測イベントにしない。実際のロード環境で再検証が必要なら「厳密継続」と明示的な再評価ロードを分ける。**効果：認識位相・配信順の維持。危険度：中〜高。既定試合：往復ありは変わる。試験切替既定0、旧保存は不足情報ありと区別。**

### R05：移動・照準・隊長計画は保存されず、同じ座標から再開しても別状態

**確信度：高。** 根拠：`BattleLeaderRuntime.h:73–103,106–138`、`BattleMixedBodies.cpp:75,81–105,115,131–138,161–186`、`BattleMixedBodies.cpp:37`、`BattleTactics.cpp:196–210`。

状況→結果：速度、進捗時計、Waypoint、PassingID／迂回地点、ContactRoute、NextOwnPath、到着予約・末尾経路、P.TargetID／NextThink／NextPath等はFBattleLeaderBody／FBattleLeaderPlan内にある。復元はこれらを新しい構造体の既定値にし、現在位置からNav経路と到着配置を再計画する。保存するSavedPlansは分隊戦術計画であり、このLeaderPlan全体ではない。

狭路で迂回中の兵は迂回を忘れ、速度0・詰まり履歴なしへ戻る。隊長は次フレームに期限0で再判断する。`AimStableSince=-1` にするため、静止して照準を安定させていた兵の散布も変わる。Forward→Rotation→Vectorの変換もあり、方位ベクトルのビット保存ではない。

IDENTICAL検出：距離・弾数・path_requests・到着人数へ伝播すれば出る。速度、経路、照準安定時刻そのものは出ず、Soakの位置許容差0.01cmでは隠れる。

改善：継続に必要な移動・計画・照準状態を論理IDと相対時計で保存し、単なる表示ハンドルだけ再作成する。**効果：移動と発射の連続性。危険度：高（Nav版・保存互換）。既定試合：往復ありで変わるため試験切替既定0。保存なしも含め状態所有権を変える実装は別に同値確認が必要。**

### R06：制圧と姿勢を消し、遮蔽予約を後から取り直す

**確信度：高。** 根拠：`BattleMixedBodies.cpp:75–77,132,137`、`BattleSuppression.cpp:30–45`、`BattleLeaderRuntime.h:419`、`BattleStance.cpp:57–65`、`BattleCoverAction.cpp:383–400`、`BattleCover.cpp:168–193`。

状況→結果：制圧が残る兵をロードするとSuppressionZeroAtをResetするため制圧0になり、Pinned／散布補正が解除される。StancesとStanceLockedUntilもResetされ、姿勢は立ちへ戻る。姿勢は見た目だけでなく目線・被弾処理に使われる。CoverPhase／Anchor／Peekは保存してもCoverSlotとReservationsは破棄し、各兵のStepCoverAction時に近傍slotをReserveする。

復元中の新規遮蔽探索と既存CoverPhase兵の予約再取得が競合した場合、先に予約した兵が勝つ。通常はFreeCoverSlotだった兵もINDEX_NONEへ変わるので、元がslot非依存の遮蔽だった場合、対応slotを発見できず `cancel_slot_lost` になる可能性がある（分岐は確定、該当配置は未再現）。

IDENTICAL検出：姿勢遷移・遮蔽取消は `cover_diagnostics`、制圧変化は命中・射撃へ波及して出る。姿勢／制圧／予約の直接ハッシュはなく、最終集計が同じなら通る。

改善：制圧残量、姿勢と遷移残り、slot種別と予約所有・期限を保存し、復元トランザクションで全予約を先に戻す。**効果：ロードによる戦術状態の解除を防ぐ。危険度：高（slot識別、失効時刻、旧保存）。既定試合：Stances有効都市戦・CoverActions有効戦・制圧ON戦の往復で変わる。試験切替既定0。**

### R07：主砲・偵察・被弾等は保存されず、同プロセスの未来の値が残る

**確信度：高。** 根拠：`BattleLeaderRuntime.h:156,161,199,231–232,330–336,350–361,501–503`、`BattleMatch.h:165,226,249–251,277–279`、`BattleSave.cpp:128–211,270–315`、`BattleMixedBodies.cpp:168–183`。

保存しないだけでなく、Restoreで一律初期化／時刻変換していない状態がある。即時往復では元の値が偶然残るため、別プロセスからの読込や過去への巻き戻しと挙動が違う。

| 状態と利用箇所 | 具体的な状況 → 結果 |
|---|---|
| TankGuns（`BattleTankGun.cpp:35–40,87,119–123`） | 保存後に主砲を撃ち、古い保存をロード → Shots、ReadyAt、NextPick、Aimがロード前のまま。次弾の乱数列・発射可否が保存時と違う。新WorldではShots=0から。ReadyAtの「未来すぎるならNow」補正は保存時残り時間を復元せず、Shots／NextPickも戻さない |
| ScoutReports（`BattleScoutReport.cpp:83,99–150`） | 報告待ち・退避・NextHint・ReportedAtを保持したままElapsedだけ巻き戻る → 未来報告を参照、退避／発信期限が伸びる。新Worldでは報告履歴がなく、即時往復と一致しない |
| KnownVehicles／IntelPurpose（`BattleArmourAI.cpp:107–123`、`BattleAreaIntel.cpp:19–52`） | 過去ロード後も保存後の車両記憶・購入任務キャッシュが残り得る。IntelViewsだけは305行で明示初期化されるので、全キャッシュが同じ扱いではない |
| ShotFrom、UnderFireUntil、SectorHitUntil、CoverProbeAfter／FailedAt、ExoNextPick | 過去のHP・位置へ戻しても、直前に受けた弾の起点・反撃期限・遮蔽再試行待ち・副武器選択期限は残る。CoverPriorityの優先枠選択にも入る |
| BlindFireUntil／Top／Side、SectorFrontAt等 | 露出点・区画キャッシュの時計がWorld時刻のまま。新Worldロードと同World巻き戻しで期限と内容が揃わない |

IDENTICAL検出：主砲等が実際に撃ち、命中・購入・移動が変われば出る。80秒で即保存即ロードする標準往復では、残存値が保存値と同じため未保存をすり抜ける。特に主砲の乱数位置はこの検査では保存されたように見える。

改善：全runtime状態を「保存必須」「入力から完全再構築」「表示のみ」に分類し、復元時に置換する。主砲は砲兵と同様、発射番号・残り時計・目標を保存する。**効果：再起動／巻き戻しの一致、未来知識の混入防止。危険度：高。既定試合：関連兵種・機能を使うロードで変わる。試験切替既定0。全消去だけではR04〜R06と同様に継続を壊すので解決にならない。**

### R08：1秒Intelキャッシュの寿命も判断状態である

**確信度：高。** 根拠：`BattleAreaIntel.cpp:21–23,39–52`、`BattleSave.cpp:304–305`、`BattleArtillery.cpp:160–170,267–276`、`BattleLeaderRuntime.cpp:1340–1350`。

状況→結果：通常は前回集約から1秒未満なら古いVを返す。保存直前に0.8秒寿命が残っていても、復元でVを空にして次の利用時に最新観測から作り直す。群や砲兵がその窓に判断すると、保存なしとは異なる敵集合・クラスタ・拠点周辺人数を読む。Match／Tacticsは当フレームのPerceptionより先なので、更新順とキャッシュの初回呼び出し位置も意味を持つ。

IDENTICAL検出：戦術イベント・購入・射撃へ伝播すれば出る。Vの時刻／要素／クラスタは直接比較されず、判断が変わらなければ通る。

改善：周期のある知識ビューを版・次回更新込みで継続状態として扱うか、保存なし側も含め全利用者が同じ明示時刻に更新する。**効果：ロードだけ情報が新しくなる差を除く。危険度：中。既定試合：往復ありは変わり得る。周期統一は保存なしにも影響するので試験切替既定0。**

### R09：接触の先勝ち・後勝ち・負ID付与に列挙順が入る

**確信度：高。** 根拠：`BattleLeaderRuntime.cpp:608–616,643,647`、`BattleAreaIntel.cpp:29–48`、`BattleRoutes.cpp:24–30`、`BattleArea.cpp:383–388`、`BattleScoutReport.cpp:37–41,94–95,139`、`BattlePerception.cpp:325,332–340`。

状況→結果：同一敵に対する新旧Contactを複数隊員が持つと、ThinkでIDを選んだ後のTargetAtはMembersの最初のContactになる。陣営集約Seen.Addは最後の観測者の位置が勝つ。順序を入れ替えると同じContact集合でも位置が変わり、区画／迂回／砲撃の候補が変わる。Members順は保存されるため、これ単独で毎回揺れるとはしない。

共有報告は同じLastSeenTimeなら既存を上書きしないため先着が勝つ。PendingReportsのRemoveCurrentSwapは以降の配信順に影響し、元の走査順が変われば同時刻報告の採用元も変わる。偵察報告はScoutReports列挙＋Positions列挙のたびに-1,-2…を振り直す。負IDは安定した敵識別ではなく、主砲／砲兵の同点キーにもなるため、報告の増減だけで同点優先が変わり得る。

IDENTICAL検出：観測元／採用位置の差は直接出ない。射撃距離・経路・共有件数などへ伝播すると検出。

改善：時刻・直接／共有・SourceUnitID等による全順序でContact全体を選び、選んだ標本を目標決定から実行へ渡す。偵察点には報告元・報告番号・点番号から安定キーを付ける。**効果：同一観測集合の順序非依存化。危険度：中〜高。既定試合：群・偵察を使う既定戦でも変わり得る。試験切替既定0。決定性修正でも従来の安定した先勝ち／後勝ちを変更する。**

### R10：近接回避は上限32件・同距離先勝ち・逐次位置更新に依存

**確信度：高。** 根拠：`BattleLeaderRuntime.cpp:1365–1369`、`BattleStableMovement.cpp:384–428`、単純移動 `BattleLeaderRuntime.cpp:1005–1013`。

状況→結果：同じ格子付近に32人超がいると、Bodies由来のバケット順で最初の32候補を選ぶ。最寄り順ではない。Blockerの距離同点は先勝ち、Separationは列挙順に浮動小数を加算する。さらに格子への所属はフレーム開始時、位置・Velocityは先行Followで更新済みのものを読む。R03の順変更で近傍集合、迂回相手、分離方向、詰まりの解除が変わる。

IDENTICAL検出：移動後の射撃・到着・経路診断に出得るが、兵の軌跡そのものは比較しない。短い迂回差が終盤までに消えれば通り得る。

改善：候補を距離＋IDで決め、集計順を固定する。全員がフレーム開始時の位置・速度を読む方式は別の挙動変更として扱う。**効果：密集時の履歴依存を抑制。危険度：高（交通安定性）。既定試合：変わる可能性が高く試験切替既定0。近傍上限撤廃や全個体LOS追加は不要。**

### R11：フレーム件数予算は30/60fpsで秒当たり処理量が変わる

**確信度：高。** 根拠：`BattleLeaderRuntime.cpp:918–952,1353–1361`、`BattleStableMovement.cpp:508–520`。

状況→結果：通常遮蔽枠（予算ON時、設定既定2）と優先被弾枠4はGFrameCounterでリセット。両方飽和すれば60fpsでは最大360要求/秒、30fpsでは180要求/秒となる（LeaderFirstの枠外分、再試行条件による未使用分は別）。個人Nav探索も2件/フレーム。ThinkCap=Nも同時期限到来をN件ずつ先頭優先で後送りする。片側ContactCoverをONにすると、共有枠を経由して反対側の処理機会も変わる。

固定60Hzのまま描画が遅くなっても、ゲーム1秒当たりのフレーム数が保たれるならこの理由だけで変わるわけではない。依存先はシミュレーションStep数。固定刻みを外した実時間駆動、30/60の切替、同じプロセス内で複数Worldがstatic QueryFrameを共有する場合は条件が違う。

IDENTICAL検出：cover_diagnosticsの待ち／探索数、counts.decisions/path_queries、移動・発射へ出る。未出力の待ち列だけなら通る。

改善：ゲーム内固定tickに対する予算・安定した待ち列・公平なカーソルを定義し、描画tickから分離する。**効果：処理密度の明確化、先頭固定優遇の抑制。危険度：高（行動が効く時刻）。既定試合：60Hzでも待ち順変更で変わるため試験切替既定0。現状ThinkCapは既定0のまま評価。**

### R12：往復Soak合格は継続同値ではなく、計測値もロードで部分リセットされる

**確信度：高。** 根拠：`BattleSightExperiment.cpp:40–60,231–237,264–284,288–289`、`BattleTacticsSave.cpp:14–17`、`BattleSquadTactics.cpp:30–33`、`BattleMixedBodies.cpp:131,163–165`、`BattlePhase3.cpp:398–409`。

状況→結果：Soakは80秒で同プロセスに即時復元し、数・ID・HP・弾薬・位置と一部戦術／経済項目を確認する。速度・照準・走査位相・主砲番号・全戦術フィールド等は比較しない。R04〜R07があってもfailures=0を返し得る。Phase3のEqualContactにもObservedSquadTypeがないので、識別だけ異なる対照は一致扱いになる。

さらにTacticsのResetはEvents／ActionSeconds／SupplyTripStartを消す。Plans再作成でPathRequestsも初期化される。E.Shots等は同じSightExperimentに残るため、metricsには「全試合」と「ロード後区間」の統計が混在する。ゲームが完全に復元できたとしても、保存なし対ありでtactical_events／tactical_squad_seconds／supply_cycle／path_requestsが変わり得る。

IDENTICAL検出：部分リセットは偽のゲーム差として検出され得る。一方、Soak合格自体は隠れた状態差をすり抜ける。R01とR02とは独立した計測の契約問題。

改善：保存検査に継続状態比較・翌tick以降のイベント比較・別プロセス復元を追加し、試合全体の診断カウンタは復元処理から分離する。**効果：往復保証と統計の意味を揃える。危険度：低〜中。既定試合：診断だけなら不変、metricsの値・合否は変わる。ゲーム挙動修正は各項の既定0切替。**

### R13：期限の「Now＋間隔」と余りの破棄は30/60fpsで別のスケジュール

**確信度：高。** 根拠：`BattleMatch.cpp:156,161–175`、`BattleLeaderRuntime.cpp:596`、`BattleState.cpp:299–305,309–313`、`BattleSquadTactics.cpp:107–120`、`BattleTankGun.cpp:40,119`、`BattleExo.cpp:281–282`、`BattleSightExperiment.cpp:66–69`。

状況→結果：期限を越えた最初のフレームに1回処理し、次をNow+間隔とする。小銃の発射も同様で、取りこぼした時間分を複数発実行しない。端数・丸め・初期位相が違う30/60Hzでは、判断・射撃・標本採取が異なる瞬間になる。Area Directorと味方Delegationは5秒超過時に時計を0へ戻し、余りを捨てるので遅れが積み重なる。非Area DirectorはFmod、Area収入は `BattleArea.cpp:229` のwhileであり全時計が同じ処理ではない。

Elapsedはfloat (`BattleMatch.h:86–87`) なので600秒間に1/30と1/60を加算した結果も同じビット列ではない。占領判定の境界通過、盾・装填・砲弾の期限、時間切れフレームも変わる。同じ固定60Hz反復での自発的な揺れとは区別する。

IDENTICAL検出：match_seconds、hire_times、first_shot_seconds、shots、各時間積算等に直接出る。未記録の期限差だけなら通る。

改善：基準シミュレーションtickと整数deadlineを定義し、繰り返し時計は余り保持／追いつき方針を統一する。30/60の描画だけ変えてシミュレーションを60Hz固定にするのが比較しやすい。**効果：時間量子化の契約を明確化。危険度：高（発射率・収入・判断頻度）。既定試合：変わり得るため試験切替既定0。30Hzの結果を60Hzの採否に直接使わない現行規則は妥当。**

### R14：FollowだけDtを0.05に切るため、低fpsでは時計が分離する

**確信度：高。** 根拠：`BattleLeaderRuntime.cpp:1331,1340–1347,1369–1371`、`BattleCoverAction.cpp:388`、`BattleMatch.cpp:156`。

状況→結果：Dt=0.1秒のフレームでもFollowの移動・遮蔽残秒は0.05秒だけ進む。一方Match/Tactics、盾・ランチャー、飛翔砲弾は元Dt、NextShot・予約・認識はWorld Nowを使う。移動が遅いまま装填や予約失効は進む。通常の固定30／60fpsのDtは両方0.05未満なので、**このClamp自体は30対60差の原因ではない**。固定10fps、非固定の50ms超フレーム等が条件。

IDENTICAL検出：射撃・移動・遮蔽取消・試合時間集計に伝播すれば出る。各時計の差そのものは比較されない。

改善：同一シミュレーション時間で全層をサブステップするか、全ゲーム時計に同じclamp方針を適用する。単にFollowの上限を外すとSweep・移動安定性を変える。**効果：低fps時の時計乖離を防ぐ。危険度：高。既定60Hz：clamp対象外なのでこの枝だけなら不変を狙えるが、スケジューラ全体変更は別。低fps挙動は変わるので試験切替既定0。**

### R15：意図的に保存しないプレイヤー経路も、厳密な継続要求とは両立しない

**確信度：高。** 根拠：`BattleSave.cpp:270–271`、`BattleMatch.cpp:157`、`Setup/Test-WideSight.ps1:61`。

状況→結果：道路の北／南指定やプレイヤー経路がある試合をロードするとPlayerRoutesをResetする。保存した通常Order／Destinationが同じでも以後の経由点は失われる。標準の無操作Experimentでは通常この条件がないため、保存往復試験を通っても操作中の再現は保証されない。

IDENTICAL検出：経路を実際に使う比較で射撃距離等が変われば出る。標準試合だけではすり抜ける。

改善：保存対象へ含めるか、厳密再現保証の範囲外であることを保存schema・試験条件に明記する。**効果：ユーザー命令の継続性。危険度：中。既定無操作試合：不変、手動経路ありロードは変わる。挙動修正は試験切替既定0。**

## 2. 確信度中：条件付きの決定性リスク

### R16：同点ソートと遮蔽slot生成の入力順

**確信度：中（比較不足の存在は高、同条件反復での発生は未確認）。**

| 箇所 | 具体的な状況 → 結果 |
|---|---|
| `BattleCover.cpp:28,49–53,163–164` | StaticMeshActor列挙順で100cm未満の候補を先勝ちで間引き、slot番号を付ける。等距離slotを距離だけでSortしてLimitで切る。Actor生成／ロード順や候補追加により、採用位置と同点境界が変わる余地 |
| `BattleCoverAction.cpp:310–315` | 同点の射撃位置をScoreだけでSort、先頭4件だけSweep。等点位置の並びが変わると「有効な位置あり／なし」まで変わり得る |
| `BattleTacticalAI.cpp:71–76`、`BattleMixedDriver.cpp:288` | 旧Actor／Phase3個人経路の遮蔽採点にslotキーのtie-breakなし。最初の到達可能な同点slotが変わる |
| `BattleStableMovement.cpp:235–236` | 進行方向・横方向が同じ兵、または同じXYで高さの違う目標で最終ID／Z比較なし。到着割当の対応が変わる余地 |
| `BattleArea.cpp:427` | 敵本拠地から等距離の監視所の最終座標／安定ID比較なし。入力順変更で偵察配置が変わる余地 |

IDENTICAL検出：slot・割当は直接出ない。移動・発射が変われば検出。現在の同じ配列を同じSortで処理するだけなら安定していることもある。

改善：距離／点数の後に安定IDまたはXYZを比較し、slot生成前の障害物・候補順も定義する。StableSortだけではTMap等から来る不安定な入力順を温存する。**効果：同点と候補上限の一意化。危険度：中。既定試合：同点があれば変わり得るため試験切替既定0。純粋な決定性修正でも旧基準とIDENTICALとは限らない。**

### R17：浮動小数の加算順と相対時計変換の境界

**確信度：中（非正規化された加算は確認、勝敗分岐は推測）。** 根拠：`BattleArtillery.cpp:185–186,218–226`、`BattleScoutReport.cpp:133–140`、`BattleStableMovement.cpp:395`、`BattleSave.cpp:153–168,195,291–299`、`BattlePerception.cpp:15–18,27–32`、`BattleCompositionRuntime.cpp:62,85–91`。

状況→結果：既定Clusters=0の砲兵重心はKnown由来の順にSumする。候補最終SortにIDがあっても、その前のSum／距離Scoreの下位bit差は消えない。偵察方向ベクトル合計、回避Separationも同様。入力順が変わり、射程・遮蔽・スコア同点の境界に近い場合、分岐が変わり得る。Clusters=1側はSoldiersをID順にしてから加算する (`BattleArtillery.cpp:143–150`) が、重複除去等も変える機能なので単純な同値修正としてONにしない。

時間はWorld Now、float Elapsed、double ActiveSeconds、整数msが混在。異なるWorld原点で `Saved=Time-Now` → `NewNow+Saved` を行うと丸めが変わり得る。PlannerはAgeをmsへFloorして期限判定するため、整数ms境界を跨ぐと記憶の寿命／不足判定が1ms変わる余地がある。一方、群はdoubleでfloat値の差を保存し同じElapsedから戻すため、「相対化は必ず誤差を生む」「群時計は必ず壊れる」とはしない。即時往復の同じNowで差が出ることも未確認。

IDENTICAL検出：shot_distances_cm等は数値の完全比較なので微小差でも出る場合がある。期限・重心そのものは未出力なら通る。すべての比較をepsilon化すると真の初期分岐も隠す。

改善：安定ID順の加算、ゲーム時計の整数tick化、期限の残tick保存、必要箇所だけ決定的な量子化を導入。**効果：境界の予測可能性向上。危険度：中〜高。既定試合：下位bitの変更でも分岐が変わる可能性、試験切替既定0。完全同値と許容誤差の比較は別レポートにする。**

### R18：Nav準備完了が実時間依存なら、固定刻みだけでは初期条件が固定されない

**確信度：中、環境依存の推測。** 根拠：`BattleSightExperiment.cpp:31–36`、`BattleCover.cpp:14–19,147–156`、`BattleLeaderRuntime.cpp:1332–1333,768–770`、`BattleVehicle.cpp:126–134`。

状況→結果：ExperimentはWorld時刻10秒で開始し、Nav構築完了を開始条件にしない。PrebuildSlotsは未完成Navを避けるが、通常のBuildSlotsはNavが存在すればbBuilt=trueとして投影を始める。固定刻み・描画なしでゲーム時計だけ速く進み、バックグラウンドNavが遅れると、初回投影の失敗や不完全なslot集合が固定される可能性がある。同期FindPath呼び出しであっても、参照するNavDataの準備状態は別問題。STATUSの「30FPSで事前に経路網ができない」という記録と整合する候補だが、今回その原因を実証してはいない。

IDENTICAL検出：経路要求・最大経路長・戦闘へ伝播すれば出る。Nav版・slot集合の署名は比較されず、潜在差なら通る。

改善：実験開始を必要NavDataの準備完了に結びつけ、完了後にシミュレーションの時刻／位相を定義する。静的slotとNav版の署名を記録する。**効果：PC負荷や起動速度から初期条件を分離。危険度：中。既定試合：開始位相・slotが変わる場合があるので試験切替既定0。単に待ち秒数を伸ばすだけでは保証にならない。**

### R19：衝突候補の等距離、表示再作成、異なるCPU／ビルドは未保証

**確信度：中（エンジン側の順序差は推測）。** 根拠：`BattleMixedBodies.cpp:112–131`、`BattleLeaderRuntime.cpp:841–850,1155–1161`、`BattleLeaderRuntime.h:300,594`。

状況→結果：ロードで全Body／衝突Componentを再生成する。重なった2体や壁と兵が同距離にあるLineTraceSingleのhit選択について、このプロジェクトはUnitIDによる二次裁定をしていない。エンジンの登録順・物理候補順が変われば、HitIDsで解決される被害者が変わる可能性がある。Chaos内部の等距離処理・実際の並行更新は今回未監査なので、アドレス順に必ず左右されるとは断定しない。

CPU／コンパイラ／UE版を跨ぐ三角関数・正規化・FMA・Nav・Chaosのビット決定性も、このゲームのIDベース乱数だけでは保証されない。同じマップ名でContentが違う場合も同条件ではない。

IDENTICAL検出：被害先が変わり集計へ出れば検出。最終集計まで同じなら通る。乱数描画やMatchID等まで含めた保存ファイルのバイト一致は、そもそも試合結果の比較とは別契約。

改善：実行環境・資産署名を固定し、衝突同点fixtureと復元後のhit列比較を追加する。必要なら同点候補の安定ID裁定を共通ルール化。**効果：エンジン境界の保証範囲を明確化。危険度：記録は低、命中裁定変更は高。既定試合：記録だけ不変、裁定は変わり得るため試験切替既定0。**

## 3. 乱数・並列・ポインタ：否定できた疑いと残る制限

| 対象・確信度 | 確認結果と条件、IDENTICALへの現れ方 |
|---|---|
| 通常銃の乱数／高 | `BattleTactics.cpp:207–210` はUnitID＋ShotSequence＋TrialSeedから毎射撃FRandomStreamを作る。共通乱数の消費順ではない。ShotSequenceはUnitStateに保存される。ただし誰が撃てたか・照準安定度が変われば以後の弾列は変わる（R03/R05）。距離・弾数に出るが乱数seed自体は比較しない |
| 砲兵・主砲・外骨格／高 | `BattleArtillery.cpp:323–324`、`BattleTankGun.cpp:122–123`、`BattleExo.cpp:320–327` はローカルStream。主砲Shotsは未保存（R07）、砲兵Shotsは保存済み。外骨格のSequenceは発射前のLauncher残数であり、補給で同じ残数へ戻れば同じ散布列を再利用する。これら3系統には通常銃のTrialSaltがない。seedを変えても独立に全武器の乱数が変わる仕様ではないが、同seed反復の非決定性ではない |
| FMath共通乱数／高（確認したC++範囲） | Prototype内の `BattleMuzzleFlash.cpp:80,82` がサイズ・RollにFRandRangeを使う。`:54` はNoCollision。確認範囲で戦闘の乱数へ結びつかない。画像の完全一致を求めれば揺れるが、metricsのIDENTICALには出ない。Variant_*のGameMode／CombatEnemy等にもRandがあるが通常Battle経路と区別する |
| Perception ParallelFor／高（適用順） | `BattlePerception.cpp:275–302` は各Traces[I]だけを書き、待ち合わせ後に元Job／候補順でConsiderする。完了順にContactsをAddしたり浮動小数をreduceする実装ではない。ただし「元順」はBodies由来（R03）。エンジン衝突の保証はR19 |
| FollowSightParallel／高 | `BattleLeaderRuntime.cpp:801–804,841–860` もindex別出力→順序適用。データ競合によるランダム合成の証拠はない。ただし全員が動く前の射線を使うため逐次Followと別仕様。先行射撃後のIsAlive検査は残す。境界で先行兵が遮蔽を横切る条件ならON/OFFで射撃が変わり、shots等に出得る。既定0維持、単なる同値高速化としない |
| ポインタ／ハッシュ／高（検索範囲） | HitIDsは弱Componentポインタ→UnitIDの照会に使う。Perceptionの候補ポインタはOrdinalで比較。確認したゲーム採点にポインタ大小やアドレス値を乱数seedにする箇所は見つからない。アセットキャッシュや材質コピーのポインタTMapの存在だけをゲームの順序不具合に数えない |
| FPlatformTime／高（主要通常経路） | Sight／Tank／Artillery／Tacticsの利用は計測。経過実時間でAI探索を途中打切りする通常の予算は確認しなかった。実時間依存はR01の統計、R18の非同期準備を分離。旧Benchmark終了判定 `BattleBenchmark.cpp:138` のwall clockは通常Experimentと別 |
| GUID／高 | `BattleMatch.cpp:18` のNewGuidは試合識別。新規試合ごとの値は異なるが、確認範囲で判断／散布seedに使わない。metricsにも出ない。バイト単位保存比較の際は識別メタデータとして分離する |
| 明示順序の既存実装／高 | TeamAI `:28,68–71,80,116–126`、Groups `:219,247,270,507,520,528`、補充キュー `BattleArea.cpp:127,258,265`、爆風 `BattleAntiTank.cpp:19`／`BattleArtillery.cpp:378`、主砲候補 `BattleTankGun.cpp:58` はID等で順序を定義。Planner `BattleCompositionPlanner.cpp:38–42,85–92` はUrgency→Score→Waiting→Type→Missionで全順序、入力集約もID順。これらまで一律に「TMapだから非決定的」とはしない |

この節の改善方針：既存のローカルStreamと順序適用は維持し、発射番号・ゲーム時計の保存を補う。**効果：並列化の誤診を避け、修正を原因へ限定。危険度：低（監査／記録）、高（処理時点変更）。既定結果：記録だけ不変。乱数salt統一・Follow先読み・命中裁定の変更は結果が変わるため既定0の独立試験。**

## 4. 30fpsと60fpsの違いを読む表

| 経路 | 30/60差の原因 | 同じ固定60Hzで描画時間だけ違う場合 |
|---|---|---|
| 遮蔽・個人経路・ThinkCap | 秒当たり枠数と待ち順（R11） | 描画が遅いだけでは枠数差を生まない |
| 発射・判断・認識・報告 | 期限を越えるtick、位相、Now+間隔、余り（R04/R13） | 同じ初期時刻とtick列なら原理上同じ予定 |
| 移動・回避・物理・占領 | 1stepの移動距離、逐次更新、境界検査の機会（R10/R13） | エンジン側の非同期／準備状態は別途固定が必要 |
| Dt上限0.05 | 30/60では上限に当たらない（R14） | 固定刻みが守られるなら壁時計の50ms超とは無関係 |
| 起動待ち10ゲーム秒 | 600対300フレーム、Nav準備に使える実時間が違う可能性（R18） | 同じFPSでも起動時負荷で準備状態が変わり得る |
| 観測・時間統計 | 5Hz採取のNow+0.2、float積算、フレーム単位診断（R12/R13） | wall由来3率は必ず別扱い（R01） |

30/60差を修正することと、同一60Hz反復の決定性を直すことは分ける。採否は現行規則どおり60fpsを基準とし、30fpsでのみ一致・改善しても保証としない。

## 5. 改善の優先順と、将来の検証設計（今回は未実施）

1. **比較の契約を先に直す（R01/R02/R12）**。性能率を分離し、完走・実効ルール署名を検査。統計一致と状態一致を別の合格名にする。ゲームを変えずに証拠の信頼度を上げられる。
2. **継続状態一覧を確定する（R04〜R08/R15）**。正規状態に移すべき時計・発射番号・移動計画と、真に再構築可能なキャッシュを分ける。「キャッシュだから非保存可」ではなく、再構築時点で判断が変わらないことを条件にする。
3. **順序修正を独立に評価する（R03/R09/R10/R16/R17）**。全IDソートを一度に入れず、実行順、観測統合、近傍選択、同点、加算の順で切り分ける。いずれも純粋な決定性修正で既存結果が変わることがある。挙動変更切替は既定0。
4. **時間と起動条件を固定する（R11/R13/R14/R18/R19）**。ゲームtick、Nav準備、資産・ビルドを固定する。性能都合で判断を後送りする変更は、同値最適化と呼ばない。

将来実行する対照は、(a)同一凍結ビルド・60fpsを独立に2回、(b)往復なし対80秒即時往復、(c)別プロセスから同保存、(d)保存後に戦闘を進めてから巻き戻し、(e)同じ論理状態を別挿入／削除履歴で構築、(f)同点・32人超密集・補充後・制圧中・姿勢遷移中・予約競合・主砲装填中、を分ける。開始条件署名を照合し、最初に違うtickで状態差を出す。

再利用済み結果のコピーは独立反復ではない。`Test-WideSight.ps1:143` 以降とMEASUREのReuseIdentical／reusedFrom規則を確認し、反復検証時はキャッシュを使わず実行した証拠を残す。今回これらを起動した事実はない。

## 6. 後続の点検メモ

### 保存・読み込み

- `BattleSave.cpp:128–318` と `BattleMixedBodies.cpp:68–187`：保存されるUBattleStateだけでなく、R05〜R08の全WorldSubsystem／Body状態の所有権表を作る。TankGuns、ScoutReports、被弾／露出／再試行時計は即時往復で残存値が隠れる代表例。
- `BattleMixedBodies.cpp:75–77` は経路検証・staging成功より前に状態をResetする。失敗ロードが現在試合を変えないかは保存監査で別途確認する。今回は失敗注入を行っていない。
- FreeCoverSlot、予約期限、姿勢・照準、移動速度・Waypoint、Nav版、無効な旧兵IDの参照を確認する。再構築可能キャッシュには「いつ再構築するか」も含める。
- `BattleSave.cpp:223–238` の適合検査とv4 RulesKeyの実効値／生引数差、v3で未検査の群・装甲・区画・遮蔽切替は先行02から継続。旧保存の不足情報を黙って現行完全保存と扱わない。
- `BattlePerception.h:55` の既定SliceCount=1、通常Stepの0、復元の追加走査を明示して検証する。死亡した敵を指す正当な古い記憶を「現在存在しない」だけで破棄しない。

### 編成AI

- `BattleCompositionPlanner.cpp:38–42,85–92` の整数採点／tie-break、`BattleComposition.cpp:50,59,83` とRuntime `:76,80` のID順は維持する。現時点で純粋Decide内部の共通乱数・アドレス依存は見つからない。
- `BattleCompositionRuntime.cpp:62,85–91`：World観測年齢→Elapsed由来msへの換算、Floor境界、保存されたVehicleSeenMSの期限を確認。float Elapsedと貯蓄期限が境界で噛み合うか。
- 先行01 F01／02 D03は現HEADで識別取得条件が変わったため、古い未修正指摘をそのまま転載しない。購入目的101の恒久化、入力側AreaIntent／群のキャッシュ時刻は別問題として継続する。
- Planner状態・入力・候補列を診断ハッシュに含める。最終購入数だけ同じでも、Save／Waitの内部遷移や次回購入時刻が違う可能性がある。

### 性能

- Perception ParallelForは順序適用を保持。FollowSightParallelは同じ保証でなく、移動前の世界を読む変更。両者を分けて計測・同値検証する。
- Bodies／Plansの安定順一覧は集合変更時に更新する方式も検討できるが、現行順序を変える初回修正と、同じ順序の一覧を再利用する最適化を別差分にする。
- 遮蔽探索の上位k化、近傍候補の最適化、砲兵集約共有は、同点・加算順・自己観測優先・予約取得時点を維持する。フレーム予算をwall time予算へ置き換えると固定60Hzでもマシン負荷で結果が変わるため避ける。
- 実時間率3項目の誤検出とゲーム内期限の除外漏れを先に整理。計測時はMEASUREに従いGet-MeasurementMachineを開始前に取得しmanifest.machineへ保存する。本報告には計測値・試験合格・実機再現の主張はない。

本報告は静的に確認できた経路と条件付きの残余リスクを整理したものであり、エンジン／アセットを含む全経路の決定性を証明するものではない。ソース、Config、既存資料、STATUS、journalは変更していない。
