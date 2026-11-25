# B03・B06 の修正の設計と、文書の訂正案（2026-11-25 08時40分 Claude）

対象：main `1983937`（コードは `437801f` と同じ）。方針は `03-development-policy.md`。
この文書は案で、コードと既存の文書はまだ変えていない。行番号は上のコミットのもの。「確かめた」はコードを読んだという意味で、ビルドと試合はまだ。

## 1. B03：委任購入が「残す資源」を使い込む

### 1.1 今の動き（確かめた）

| 経路 | 場所 | 残す資源の判定 |
|---|---|---|
| 委任なしの AI 購入 | `BattleArea.cpp:451` | `資源−補充の予約 < 購入費＋残す資源` なら買わない（正しい） |
| 委任（`Delegate>0`）の入口 | `BattleArea.cpp:449` | `資源−補充の予約 < 残す資源` だけ。**購入費が入っていない** |
| 委任のすぐ買う経路 | `ReserveHire`（`:184–187`）→ `HireAreaSquad` | 判定なし（`HireBlock` は `資源 < 購入費` だけ、`:123`） |
| 予約の実行 | `UpdateHireQueue`（`:324–333`） | 判定なし。予約の後で残す資源を上げても守られない |

残す資源の値は、プレイヤーが委任しているとき `Area.PlayerHireReserve`、それ以外は `AreaIntent(Team).HireReserve`。

### 1.2 直し方

1. **購入費の関数** `int32 UBattleMatch::HireCost(uint8 Type,uint8 Grade) const`。`:114`・`:141`・`:434` の同じ式を置き換える（結果は変わらない整理）。
2. **残す資源の関数** `int32 UBattleMatch::AIHireKeep(int32 Team) const`。`PlayerDelegating(Team)` なら `Area.PlayerHireReserve`、それ以外は `AreaIntent(Team).HireReserve`。
3. **共通の判定** `bool UBattleMatch::AIHireAffordable(int32 Team,int32 Cost) const`。`資源−補充の予約−Cost >= AIHireKeep(Team)` を返す。補充の予約の数え方は `:355` と同じ式を関数にまとめる。
4. **判定を置く場所（AI の購入だけ。プレイヤーの手動の購入と予約には掛けない）**
   - `ReserveHire`：`bByAI` で `Block==0` でも `AIHireAffordable` が偽なら、すぐ買わずに予約へ回す。待ちの理由は新しい札 `wait=keep`。
   - `UpdateHireQueue`：`Q.bByAI` の予約は、`HireBlock==0` に加えて `AIHireAffordable` が真のときだけ買う。偽ならその予約は待つ（列からは外さない）。
   - `:449` の入口の式はそのまま（残す資源を割っている間は AI が新しい予約を足さない）。
5. **待ちすぎの扱い**：`HireStarved`（資源待ち120秒で後ろを止める）は、残す資源のために待っている AI の予約を数えない。数えると、AI の予約がプレイヤーの予約を止めてしまう。
6. 委任なしの経路（`:451`）は式が同じなので触らない。

### 1.3 既定の試合への影響

`-BattleHireDelegate` は既定0で、プレイヤーが操作しない試合では `PlayerDelegating` も偽。直す所は委任の経路だけなので、**既定の試合は変わらない見込み**（確かめ方は 1.4 の4）。変わったら変更の範囲を疑う。

### 1.4 確かめ方（必要な分だけ）

1. `BattleAreaSmoke` に3つ足す。
   - すぐ買う経路：資源20・残す資源20・購入費12 → 買わずに予約になり、資源は20のまま。
   - 予約の実行：資源が `購入費＋残す資源` に届いた秒に買い、買った後の資源が残す資源以上。
   - 残す資源の変更：AI の予約がある状態で残す資源を上げる → 買わない。下げる → 買う。
2. プレイヤーの手動の予約は残す資源に関係なく買える（今までどおり）。
3. `Test-Battle` と `Test-Area`。
4. 既定の試合が変わらないこと：マップI seed 7 を直す前後で比べて IDENTICAL。
5. 委任ありの試合1本（`-BattleHireDelegate=2`、マップI seed 7）で、`BATTLE_AREA_HIRE auto=1` の全行について `credits >= 残す資源` を数える。直す前の同じ試合での違反の数も並べる。

## 2. B06：補給後に区画任務が点命令になる

### 2.1 今の動き（確かめた）

- 小隊の「任務」は `FBattleSquadState` にすでにある：`bHasMission`・`MissionOrder`・`MissionDestination`・`MissionGoals`・`MissionRevision`・`bSectorMission`。補給・退避などの一時的な行動（`TacticalPlan`）の間も任務は残る（`SetSquadTacticalPlan`、`BattleState.cpp:248–252`）。
- 一時的な行動の間に上の層が任務を更新すると、任務の欄だけ書き換わり、実行は後回しになる（`BattleState.cpp:212–220`、`MissionDeferredByPlan`）。明示の命令（`bMissionUpdate=false`）は一時的な行動を取り消してすぐ実行する（`:221`）。
- 復帰の呼び出しは `BattleSquadRetreat.cpp` に6か所（`:164` 補給車、`:188` 本拠地、`:251` 襲撃、`:261` 外骨格のシールド、`:277–280` 再編、`:314` 交戦の終わり）。どれも `IssueOrder(SID,Order,Goal,true)` を2回呼び、`bSector` を渡さない。`SetSquadOrder` は `bSectorMission=bSector` と書くので（`:214`）、区画任務が点命令になる。
- 時間切れの復帰（`AdvanceSquadTacticalPlan`、`BattleState.cpp:269`）だけは `bSectorMission` を渡していて正しい。
- 推定（未確認）：AI の小隊は、次の割り振りで `BattleMatch.cpp:543` が `bSectorMission` の違いを見て区画命令を出し直すので、点命令の時間は短い。プレイヤーが出した区画命令（委任した部隊）は出し直す層が無く、点命令のまま残る。

### 2.2 直し方

1. **任務の構造体**（`BattleState.h`）。項目は今ある任務の欄と同じものだけにする。
   ```cpp
   struct FBattleMission { EBattleOrder Order; FVector Goal; bool bSector; int32 Revision; };
   bool UBattleState::MissionOf(int32 SquadID,FBattleMission& Out) const; // bHasMission が偽なら false
   ```
2. **復帰の関数を1つ**（`BattleSquadRetreat.cpp`）。
   ```cpp
   bool UBattleSquadTactics::ResumeMission(int32 SID,const TCHAR* Reason,FName EventName);
   ```
   中身：`MissionOf` で**その時点の**任務を読む → `IssueOrder(SID,Order,Goal,true,bSector)` で道が通るか確かめる（一時的な行動の間は実行されない）→ 通れば `CancelSquadTacticalPlan` → もう一度 `IssueOrder` で実行 → 記録。6か所をこの関数の呼び出しに置き換える。補給の統計（`SupplyTripEnd`）などは呼び出し側に残す。
3. **3つの約束と、それを満たす仕組み**
   - 区画属性を保つ：`bSector` を任務から読んで渡す。
   - 補給中に新しい命令が来たら古い任務へ戻さない：補給の開始時に写しを取らず、復帰の時点で任務の欄を読む。任務の更新は欄に入っているので新しい方へ戻る。明示の命令は補給そのものを取り消すので、復帰の関数は呼ばれない。
   - 復帰は一度だけ：復帰すると `TacticalPlan.Action` が `Mission` に戻り、呼び出しの条件（`Recover`・`Regroup`・`Engage`）から外れる。関数の入口でも `Action==Mission` なら何もしない。
4. **記録**（方針 2.6 の指標の最初の分）：`BATTLE_MISSION_RESUME squad= t= reason= order= sector= rev= ok=`。失敗（道が通らない）も `ok=0` で残す。
5. 車両は区画命令を受けない（`IssueOrder` が断る、`BattleLeaderRuntime.cpp:553`）ので、車両の `bSectorMission` は偽のままで動きは変わらない。
6. `IssueOrder(...,true)` の形の呼び出しは試験の外に `BattleScoutReport.cpp` にも1つある。着手時に同じ漏れか確かめる。

### 2.3 既定の試合への影響

区画任務を使うマップ（I は既定、H・J は切替による）では、**既定の試合が変わる見込み**（補給の後に点命令で動く時間がなくなる）。A〜E は区画任務が無いので変わらない見込み。判断1の答えが「切替なしで直してよい」なら切替なしで入れ、違う答えなら試験切替（既定0）にする。

### 2.4 確かめ方（必要な分だけ）

1. `BattleSquadRetreatSmoke`／`BattleAreaSmoke` に3つ足す。
   - 区画任務の小隊を本拠地補給 → 復帰の後も `bSectorMission` が真で、目的の区画が同じ。補給車からの復帰も同じ。
   - 補給中に任務を別の区画へ更新 → 復帰先は新しい区画。補給中の明示の命令 → 補給が終わり、復帰の記録は出ない。
   - 復帰の記録は1回の補給につき1行、`OrderRevision` の増え方は1。
2. `Test-Battle`、`Test-Area`。
3. マップA seed 7 が直す前後で IDENTICAL（区画任務の無いマップは変わらないこと）。
4. マップI・seed 7 と 43（今の基準の2シード、30FPS）：`BATTLE_MISSION_RESUME` の数と `sector=1` の割合、基準との差（得点・与ダメージ・撃破・戦死）を左右別に表にする。発動0回の試合も載せる。これは採否のためではなく、変わり方の記録。

## 3. 文書の訂正案

### 3.1 STATUS の要約（表の行ごと。今のコードと `SWITCHES.md` で確かめた）

| 行 | 今の記述 | 訂正 |
|---|---|---|
| 経済・AI戦略 | 「AIが戦車を買う既定は0」 | 「`-BattleAreaTankKeep` の設定値は0だが、**11-22 から `-BattleAreaComposition` が両チームで既定ON**（`BattleArmourAI.cpp:67–75`）なので、既定の試合でも AI が車両・対戦車班などを買う」 |
| 経済・AI戦略（未対応の欄） | 「保存往復の検証で `Invalid cover peek distance` …（未調査）」 | 「原因の候補が1つ分かった：壁遮蔽（直接操作）の覗き位置80cmを保存の検査（100〜450cm）が拒否する（B01、`Docs/review-2026-11-25/`）。AI だけの試合で出る場合の原因は未確認」 |
| 新機能 | 「戦略層の `-BattleAreaComposition` は既定0」「AIは既定で買わない」「main へは未取り込み」「`-BattleAreaComposition` を既定にするか」 | 「11-22 に両チームで既定ON（ユーザー決定、main 0b122ab）」。判断待ちの欄から外す |
| 新機能（未対応の欄） | 「`-BattleAreaGroups` 既定0 …マップA/D系だけ既定にするかはユーザー判断」 | 「11-20 から全マップで既定ON（ユーザー決定。`BattleGroups.cpp:81–83`）」。判断待ちの欄から外す |
| 現状の負荷問題 | 「`-BattleFollowSightParallel`（試験切替、既定OFF…）」 | 「今の本体に受け手が無い（`Setup/Test-WideSight.ps1:193` の引数だけが残っている。削除の候補）」 |
| 直接操作の表示 | 「`-BattleAreaPointSupply`、既定OFF・`Play-Area.ps1` でON」「拠点での補充を既定にするか」 | 「11-22 から全マップで既定ON（ユーザー決定。`=0` で切れる）」。判断待ちの欄から外す |
| 戦略層・マップJ | 「段階3〜7（購入予約…すべて既定0）」 | 「購入予約 `-BattleHireReserve` は 11-24 からプレイヤー既定1（購入画面の行と同じ記述に合わせる）。ほかは既定0」 |
| 視線判定の負荷対策 | `-BattleSectorSight` 既定1（光線 −71%） | 追記：「これは高速化ではなくルールの変更（隊長が遮蔽の陰だと隊員に見える敵を見つけない場合がある）。既定のまま続けるかは判断待ち（判断3）」 |

表の上に1行足す：「**11-25：Codex レビューと今後の方針は `Docs/review-2026-11-25/03-development-policy.md`。新しい試験切替より先に、契約の修理（B03→B06→B01/B02→B04→B10）と評価の決まりの整理を行う。**」

### 3.2 ROADMAP

| 場所 | 今の記述 | 訂正 |
|---|---|---|
| 10行目（兵種） | 「AIは既定で車両・外骨格・偵察組を買わない。…引き算の評価は**未実施**」 | 「11-22 から `-BattleAreaComposition` が既定ONで、AI は既定で車両・対戦車班などを買う（偵察組・外骨格・補給車は `Keep` の設定が0で買わない。`BattleState.h:603`、`BattleArea.cpp:621`）。引き算の評価は切替（`-BattleAreaAllTypes`・`-BattleAreaWithout`）があり、結果は『兵種の強さ』ではなく『その購入・運用込みの損得』と読む」 |
| 11行目（区画） | 「区画経由・側面・前線・被弾反応・交互前進などは試験切替（既定0）」 | 「区画AIの組み合わせ9個はマップI で既定ON、`-BattleWaypointPass` は全マップで既定ON（11-22、下の表）。交互前進 `-BattleBounding` は既定0で、移動担当が遮蔽から出られない不具合（B07）があり休止」 |
| 20行目（採否の決め方） | 「片側試験で比べる」 | 「同じ条件の元の結果との差（得点・与ダメージ・撃破・戦死）で比べる（`MEASURE.md`）。機能が動くことの確認と通常の試合での有用性を分け、発動0回の試合も集計に入れる。採用の候補だけ未使用の2シードで確かめる」 |
| 20行目に足す | — | 「**切替なしで直す範囲**：（判断1の答えをここに書く）」 |
| 新しい節 | — | 「2026-11-25 契約の修理と評価の整理」：方針 2.1 の順番、2.2 の保存の合格条件、2.4 の観測の原則の文、4 の1週間の並び。本文は方針の文書へのリンクにして写しを作らない |

### 3.3 `Docs/design-front-strategy.md`

「1. なぜ作り直すか」の1つ目の項目を次に替える。

> - 戦略層 S2（集中先の選び直し）が変えていたのは**集中先だけ**で、既定（手前から順に取る）より遠い所を選ぶと手前の自軍の拠点が薄くなる（仕組みの上での理由）。片側試験はマップE で −6.7（3シードで打ち切り）。
> - **根拠から外した数字**：マップJ の −69.6 と、直した後の −89.6。この2つは相手側の前線軍が切れていた試験で、S2 の効き目としては読めない（調査の掲示板 11-22 21時18分）。履歴として残すが、作り直しの根拠にはしない。

2つ目・3つ目の項目（3つの層が同じ兵を取り合う、重点は線の上にしか置けない）はそのまま。これが作り直しの根拠になる。
冒頭の実装の状況で `-BattleHireReserve=1`（既定0）と書いた所に「（11-24 からプレイヤー既定1）」を足す。
訂正の前に、マップE の −6.7 が同じ欠陥（相手側の S4/S6 が切れる）の影響を受けていないかを掲示板と STATUS の記録で確かめる。受けていれば同じ扱いにする。
