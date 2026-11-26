# 防衛進軍の門と個人配置先（a679d57f からの修正）

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
