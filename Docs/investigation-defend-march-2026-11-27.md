# 防衛進軍の門と個人配置先（a679d57f からの修正）

## 657ae7b9 の結果を受けた到着済み兵の修正

ユーザー提供の凍結 coverlean-657ae7b・K・30FPS・両軍値2/DefendMarch=1：seed 7 の gate_pass=1,031,487、gate_at_post=782,854、defend_march_searches=60、started=0。seed 43 も at_post=667,618、searches=9、started=0。これにより、旧記録の gate_march_defend の大半は持ち場に着いている兵と確定した。下記の「特定不能」はこの結果を得る前の記録。

- 値2かつ DefendMarch=1 だけ、個人 ArrivalGoals（無ければUnit.Destination）から1m以内で、従来 at_post に該当する兵を Follow の進軍分岐から通常の StartKnownMove へ渡す。Waypoint・到着予約・命令は変えない。bMarch/bDefendMarch はfalse。防衛円と区画任務の制約、人数・露出・予算・移動期限・本人確認を通常の停止中と共有する。個人配置先が防衛円外でも防衛円を広げないため、この変更後も候補が成立しない場合がある。
- 通常の値2の Commit>0 は EndKnownMove で自分の遮蔽状態・FirePositionsを解除し、Follow/StableMove が元の個人配置へ戻す構造。今回の兵には bDefendPost を持たせ、Commit=0 でも初弾／到着後5秒／情報失効で終了・解除する。残った遮蔽が帰還を妨げないようにする。帰還待ちの間は次の既知敵への位置取りを始めない。通常の戦闘停止・混雑・再配置は引き続き適用され、一定秒数内の必着を保証する変更ではない。
- 記録 `known_move_defend_post_searches` は予算予約成功後、`_started` は移動開始、`_arrived` は射撃位置到着、`_fired` は到着後の初弾（別目標を含む、既存 march_fired と同じ）。`_resumed` は通常の持ち場移動へ戻した回数、`_resume_arrived` はその時点の個人配置先から1m以内、`_resume_interrupted` は死亡・命令／任務変更など、`_resume_stuck` は10秒間40cm以上の接近が無かった場合に1回。`_gate_returning` は帰還中の再探索拒否。`_resumed` だけで帰還成功と数えない。
- Follow から通常探索に振り分けた到着済み兵は、従来の進軍門を呼ばないため gate_at_post に計上されない。旧版との at_post の大小を探索成功の比較に使わない。
- 固定場面に `move_defend_post_limits`、`move_defend_post_arrive_fire`、`move_defend_post_resume`、無効時の `move_defend_post_disabled` を追加。最終Waypointを残し、命令目的地と個人配置先を200cmずらした兵で探索・実発砲・個人配置への帰還・状態解放を検査する。既存項目は変更しない。Commit=0/12とDefendMarch=0/1で実行する想定。

進軍中の target_range は軍共有表を「交戦距離＋MoveRange」の格子範囲で列挙した後、進軍だけ出発点から交戦距離内に絞るため発生する。target_order は WeaponOrderAllows の制限（非区画Defendの防衛円、区画Missionの別区画拒否と既存の撃ち返し例外など）。seed 7の80,607/70,111は探索数ではなく敵候補ごとの拒否件数。どの命令条件が最多かはこの合算記録からは判別できない。安く直せる契約違反は見つからず、ルールは変えない。

ビルド・UE・試合は依頼により未実施。CRLFと差分、既存固定項目の維持、追加識別子・初期化を静的確認し、コード案内と切替表を再生成する。DefendMarch=0の同一性はコード上の非発動を確認する範囲で、修正後のIDENTICALは未確認。STATUS.md / Docs/journal/ は変更しない。掲示板はUE同梱Pythonで既存申請を参照した（新しい試合の実行・申請なし）。

ビルド・UE・試合は実行していない。以下はコードと既存 metrics.json の調査。STATUS.md / Docs/journal は依頼により変更しない。

## 既存結果から確定できること

`Setup/Benchmarks/SightTrial-20261111/` の結果:

| 計数 | km6-K-km-s7 | km6-K-kmd0-s7 | km6-K-km-s43 |
|---|---:|---:|---:|
| known_move_gate_march_defend | 782854 | 2013462 | 667618 |
| known_move_searches | 8024 | 8319 | 9023 |
| known_move_reject_mission_defend_march | 46 | 0 | 5 |
| known_move_gate_no_candidate | 129596 | 99277 | 125286 |
| known_move_reject_mission_squad_radius | 5313 | 3164 | 2888 |
| known_move_reject_cover_fire_point | 1255 | 1864 | 2246 |
| known_move_budget_deferred | 185726 | 175044 | 215729 |

防衛開始は両シードとも0。一方、defend_march の任務拒否46/5は、**防衛進軍として探索に入った候補があった**証拠。門で全員を落としていたわけではない。その他の断りは攻撃移動・停止中との合算であり、防衛の内訳に転用できない。開始0の支配的原因と78万回の条件別実数は、既存記録だけでは特定不能。今回の記録を使う次の実行で確定する。

## 門の78万回は歩行中の人数ではない

- Follow の `bOnRoute` は共有 Path または個人 Corridors の Waypoint が有効ならtrue。速度・到着済みかは見ない。
- StableMove は個人 ArrivalGoals から20cm以内で `bArrived=true` にして停止するが、最終 Waypoint を無効化しない。したがって停止済みの兵も毎フレーム Follow の「進軍」分岐に入り得る。
- KnownMoveDefendMarching は個人配置先から1m超を要求する。この停止済み状態を正しく拒否する。ここを無条件に緩めると、到着済みの兵まで防衛進軍として扱うため、距離の門は維持する。
- 他の拒否はプラン／Body無し、Defend以外、bCoverStop、戦術Engage、経路無し。Followで歩行条件を計算した後にStableMoveが走るので、門は移動後の距離・Waypointを読む。最終到着のフレームでも拒否され得る。
- bArrivalWalk は、同じ任務区画内、戦術Mission、個人ArrivalTails有り、配置先まで1.5m超、個人経路有りなどが条件。小隊目的地への到着後も個人配置へ歩くための仕組みであり、Defendでも利用される。

## マップKで作られる命令の形

- `BattleMatch.cpp` の UpdateTeamOrders は所有拠点・防衛持ち場などへDefendを出す。ただし遠距離の道路中継点や区画中継点はAttackMoveに置き換わる。全行程がDefendではない。
- 区画任務は目的区画の中心をGoalにし、IssueOrderで高さとNav投影を解決する。Destination無しを表す任意値はなく、受理された命令は目的地を持つ。
- 通常のwide経路では各兵のUnit.DestinationとSquad.Destinationは同じGoal。Phase3の隊形補正が有効なら個別Goalを作る。**実際の歩行の配置先は別のPlan.ArrivalGoals**で、隊長自身もその一員として割り当てられる。隊長の現在位置は共有経路の出発点に使われるが、兵は隊長の個人配置先へ歩くのではない。
- DefenseRadiusは設定・状態の既定とも600cm（DefaultGame.ini:21）。BuildMovementPlanの通常配置格子は非区画Defendだけ防衛半径で制限する。区画任務は区画内の配置を選び、遮蔽候補は中心から3600cm以内かつ同じ区画。個人配置先が6mの円外にあるのは正常。
- したがって「拠点へ向かう歩き」に加えて「同じ区画の中で中心から離れた配置へ歩く」状態が生じる。ただし、その割合は既存の合算記録からは出せない。

## 判定の修正

旧判定の帯は出発点→**小隊Destination**。個人配置先が別方向の場合、その配置先への道の近くでも帯外かつ6m円外として拒否していた。今回、出発点→**個人ArrivalGoals（無ければUnit.Destination）**へ変更し、エピソード開始時のDefendPostとして固定した。途中の再配置で帯が回転して移動範囲が拡がることはない。

片側600cmの線分帯（後方・終点延長なし）または小隊の防衛円、出発点からMoveRange、小隊中心からMoveSquadRadiusを維持。候補・Nav投影後・覗き点・経路頂点・実行中の位置に適用する。曲がった道を帯として追加探索する変更ではない。敵への接近、露出、経路、人数、期限、予算、WeaponOrderAllowsは維持。

固定場面の従来例は個人配置先・兵Destination・小隊Destinationを同一の30m先にし、半径3400cmで射撃位置を広く含めていた。今回の違いを検出できなかった。新規の2項目は試合と同じ状態表現を手で組み立てる幾何・門の検査であり、Kの命令AIやNavによる配置生成を実行する試験ではない。

## 記録の読み方

- `known_move_defend_gate_{disabled,invalid,order,cover_stop,engage,no_route,at_post,pass}`：Followの防衛進軍分岐を1回通るごとの最初の拒否／通過。既存gate_march_defendは失敗分の合計として残す。`not_following`も定義するが、通常Followの記録呼出しではtrueを渡すため発生しない。
- `known_move_defend_march_searches`：敵候補有り、Nav／遮蔽準備済み、予算予約成功後の探索回数。
- `known_move_defend_march_gate_{active,cooldown,end_cooldown,search_interval,cover,ammo,plan,members,no_candidate,setup,no_place,not_allowed}`：開始前の待ち／拒否。cooldownとその内訳など重複する親子の計数を足し合わせない。
- `known_move_defend_march_target_{stale,range,order}`：敵候補を落とした理由。探索回数ではなく、表から読んだ敵の件数。
- `known_move_defend_march_budget_deferred`：初回または探索内の再予約を待った回数。
- `known_move_defend_march_reject_*` / `known_move_defend_march_prefilter_*`：StartKnownMove内の既存断りを防衛進軍だけに分離。mission_squad_radius、mission_defend_march、anchor_nav、occupied、commit_time、anchor_range、anchor_danger、more_exposed、cover_fire_point、free_fire_line、peek_mission、peek_danger、path_invalid、path_mission、path_length、path_capsule等。1探索に最大8候補を試すので探索回数と一致しない。cover_fire_point内部のover/lean別の光線失敗は既存の全体計数のまま。
- `known_move_defend_march_candidate_*` / `pick_*` / `fire_*`：候補種別と選択の補助記録。既存のstarted/arrived/fired/resumedは維持。

## 不変条件・確認状況

- 親切替0・値1ではこの移動追加に入らない。値2かつDefendMarch=0ではDefendMarchがfalseとなり、追加記録の防衛探索分と個人配置先の帯を使わない。候補の評価順・乱数・予算消費・命令状態を書き換えない。既存の到着後／攻撃移動の制約はそのまま。
- 追加検査 `move_defend_march_personal_post`：同一区画、半径6m、小隊中心と個人配置先の相違、共有経路は完了・個人の末尾は有効。個人帯の許可、停止中の旧範囲では拒否、逆方向拒否、開始後の帯固定、小隊半径超過拒否。DefendMarch=0では門が通らないことも検査。
- `move_defend_march_arrived_gate`：個人配置へ到着してもWaypointが有効な状態で門を拒否し、at_postを計数する。
- 既存の実射撃・復帰・停止中拒否の検査は変更せず、スクリプトの期待項目に上記2件を追加。
- 実行試験・警告を含むコンパイルは未実施。CRLF、差分の空白、追加名の重複・初期化、生成表と試験項目の登録を静的確認。Export-CodeMap / Export-Switchesを再生成。生成時の既存注意「BattleSectorLanesの説明無し」は残る。
