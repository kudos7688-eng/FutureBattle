# 区画の攻防と遮蔽（設計）

2026-11-27 作成（系統「区画の攻防と遮蔽」、作業ツリー `FutureBattle-sectorfight`、ブランチ `sectorfight`、掲示板 C54）。対象は区画のあるマップ（H〜L）の AI の歩兵。切替はすべて既定0で、切替0の試合は変えない。

## 1. ユーザーの決定（2026-11-27、原文）

1. 「現状攻撃する際、もう少し相手がいる区画に対して積極的に区画選択を行い、区画を制圧する動きを強めた方がいいと思います。また、維持する際もその場で打ち合うのではなく、区画から区画防衛を行った方がいいと思います。また遮蔽を取る際は、中心から外側、あるいは相手に対して射程を取るように。」
2. 「3点とも設計から」
3. 「設計書ができたら攻撃から実装してください。また相手からできるだけ近い遮蔽および相手に対して遮蔽を取れるように遮蔽を取るようにしてください。」

遮蔽は 3 が新しいので、**「相手にできるだけ近く、相手に対して遮蔽になる地点」**を正とする（1 の「射程を取る」は、距離を空ける意味では採らない）。

## 2. 現状（コードで確かめたこと）

### 2.1 攻撃

- **前線は敵の多い所へは進まない**。到達線は行ごとに「知っている敵が2人以上いる区画」の1列手前まで（`BattleFrontLine.cpp:177・207`）。既定の前線軍の自分の線（`-BattleFrontHold`）は、最初の線を敵の2列手前までにし（同 :326）、その後は20秒ごとに「被害が無く、敵が4人以上かつ味方の2倍以上でない」行だけ1列進む（同 :380〜、Codex の読み取り）。敵のいる区画を目標に取る判断ではない。
- **戦略の目標は拠点で、区画ではない**（`AreaStrategyTarget`、`BattleMatch.cpp:308`）。情報活用を入れても、既知の敵が多い敵側の拠点は重みを下げる。
- **区画経由の移動は敵の区画を避ける**。立ち寄り先から「敵だけがいる区画」を外す（目的地は除く。`BattleSectorRoute.cpp:205`）。
- **敵へ向かうのは「向かう」だけ**（`-BattleSeekKnown=2`、既定。`BattleSquadRetreat.cpp:537〜584`）。手すきの健康な小銃小隊が、軍の目撃マスのうちいちばん近い1点へ攻撃移動する。
  - 行き先は目撃の**点**で、区画の任務ではない（計画は Engage なので、到着時に区画の遮蔽を選ぶ処理 `BuildMovementPlan` の `Sector` が立たない）。
  - 途中で敵を自分で見ると、その場で止まる（`KNOWN ENEMY / HOLD FIRING POSITION`、同 :464〜468）。
  - 敵を見失って4秒たつと**元の任務へ帰る**（`CONTACT LOST / RESUME MISSION`、同 :482〜485）。着いて敵が居なくても10秒で帰る（同 :471）。
  - つまり「敵の居た場所へ行き、撃ち合いが終わったら帰る」で、**区画を取って残る動きは無い**。

### 2.2 維持

- 区画任務で着いた小隊は守りに変わるが、敵を自分で見ると戦術層が `Engage`／`Hold`（その場）へ移り、区画任務の扱い（区画の遮蔽を選ぶ・攻撃の対象の向きに選び直す）が外れる（`BattleSquadRetreat.cpp:585`、`BattleStableMovement.cpp:103`）。これが「その場で撃ち合う」の正体とみる。
- 「守り」に変わる条件は、生きている兵の過半数が区画に入ったことだけ（`BattleLeaderRuntime.cpp:853`）。区画に敵が居ないことは確かめていない。
- 接触した小隊が区画の外へ出る経路がある：側面移動（`BattleSquadFlank.cpp:47`）、遮蔽の後ろへの集合（`BattleCoverAction.cpp:373`）、接触時の近い遮蔽（同 :240、区画の制限なし）。
- 守りの配置（`-BattleDefendSpread`、arms）は小隊の中で兵を左右に散らす仕組みで、区画の単位の守り（どの区画で受け、どこへ下がるか）ではない。11-26 に棚上げ（入れた側が少し損）。arms の今のコードは既定1になっている（11-27 の変更、main には無い）。

### 2.3 遮蔽

- 到着時の遮蔽地点（`UBattleLeaderRuntime::BuildMovementPlan`、`BattleStableMovement.cpp:99〜`）：
  - 候補は目的地の区画の中の遮蔽スロット。
  - 「遮蔽になる」の判定は、攻撃の対象の点（`-BattleAttackFocus`、既定1）の方向±30度の2.5m先に壁があること（同 :216〜221）。対象の点が無いときは歩いてきた向き。
  - 並びは**目的地（区画の中心）に近い順**（`NearbySlots` の順）。**敵からの距離は入っていない**。
  - 敵から実際に隠れるかの検査（`-BattleCoverStrict`、5点中2点以下しか見えない）は既定0。
- 外向きの遮蔽（`-BattleCoverOutward`、既定0）は周り8区画の向きのどれかに壁があればよい形で、11-21 の試験では5方向以上が残って実際の射撃方向と合わず悪化した。

## 3. 攻撃：敵のいる区画を取りに行く `-BattleSectorSeize=1`

「向かう」を土台に、行き先を**区画**にし、**取るまで続け、取ったら残る**。

### 3.1 目標の区画の選び方

- 材料は軍の目撃マス（`UBattleSquadTactics::KnownSpots`、真の位置は使わない）。マスを区画（`BattleSectors::Cell`）ごとにまとめ、「敵の区画」の表を1秒ごとに作る。
- 候補：目撃のある区画で、通れる区画（川・山・湖でない）。
- 点数（高いほど選ぶ）：
  - 近さ：小隊長から区画の中心までの距離（−1点／50m）。
  - 集中：同じ軍の別の小隊がもう取りに向かっている区画は +3点（1小隊まで）。3小隊で埋まり（上限 `-BattleSectorSeizeMax`、既定3）、それ以上は選ばない。
  - 足場：自軍の兵がいる区画に隣り合う区画は +2点（孤立した奥の区画へ1小隊で入らない）。
- 対象の小隊は「向かう」と同じ（健康な小銃小隊、敵が近くに無い、本拠地攻撃の枠・購入の目的つき・奇襲・襲撃を除く。前線軍の小隊は対象）。距離の上限も `-BattleSeekKnownRange` を使う。

### 3.2 動き

1. **向かう**：区画の中心へ攻撃移動（計画 Engage、理由 `SEIZE SECTOR`）。到着の計画は区画任務と同じ扱いにする（区画の中の遮蔽を、攻撃の対象の向きで選ぶ）。
2. **途中の接触**：敵を自分で見たら今までどおり止まって撃つ（`SEIZE SECTOR / FIRE`）。**隊員が最後に自分で敵を見てから4秒たち、直近の被害が無ければ、任務へ帰らずに同じ区画へ向かい直す**（共有・記憶の接触が残っていても待たない）。
3. **制圧**：小隊長が目標の区画に入り、その区画に目撃が無い時間が15秒続いたら「取った」。
4. **残る**：取った区画に30秒とどまる（`-BattleSectorSeizeHold`、既定30）。その間に隣の区画に目撃が出れば、次の目標として 3.1 で選び直す（区画から区画へ進む）。出なければ任務へ帰る。
5. **やめる条件**：向かい始めて180秒、小隊が退却・補給に入る（今の決まりのまま）、目標の区画の目撃が消えて60秒。やめた区画へは同じ小隊は30秒向かわない。取った後に撃ち合いが続いても、取ってから「残る時間＋90秒」で任務へ帰る。

### 3.3 数える記録

`SeizeGo`（向かった）、`SeizeAgain`（撃ち合いの後に向かい直した）、`SeizeEnter`（区画に入った）、`SeizeTaken`（取った）、`SeizeNext`（次の区画へ）、`SeizeEndTimeout`／`SeizeEndGone`／`SeizeEndOther`。行 `BATTLE_SECTOR_SEIZE`。意図した行動は `SeizeAgain`・`SeizeEnter`・`SeizeTaken` で数える。

支援／側面移動で中断した未終了の制圧は、その計画が Engage／Mission に戻った後、人数・HP・武装数・被害なしの条件と期限を再確認して同じ区画へ戻す（自分で見ている場合は FIRE）。成功は SeizeResume、不成立は SeizeEndOther。SUPPORT／FLANK 中は閉じず、退却・補給・再編では閉じる。

### 3.4 次の段（今回は入れない）

- 前線の線を敵の区画の手前で止める決まり（2.1）を、取りに向かっている区画の列までは出す（スポットの緩和の B-3 と同じ所。系統「戦略層」「群制御」と調整が要る）。
- 群（突入群・本拠地攻撃隊）の目標に敵の区画を足す。

## 4. 維持：区画の単位で守る -BattleSectorDefend=1/2

2026-11-27 実装（未ビルド・未実行）。親切替は既定0・0〜2。-BattleSectorDefendTeam=-1（両軍、0/1で片軍）、-BattleSectorDefendBack=30（5〜120秒）。区画マップのエリア戦、AI管理下の区画任務で、到着後に計画 Mission・命令 Defend になった小隊を対象とする。兵種の追加制限はない。直接操作・明示命令の小隊には介入しない。

### 4.1 値1：区画の中で受ける

- Evaluate の補給・退却・再編・盾切れ・対戦車退避・Emergency より後、通常の Engage 処理と新規 JOIN／SEEK／SEIZE より前で区画任務を継続する。接触が無いとき（まだ見ていない、または最後に自分で見てから30秒）は状態を閉じて通常の評価へ戻す（JOIN／SEEK／SEIZE に選ばれてよい。固定すると敵を探しに出る小隊が居なくなるため）。値2の後退は、45秒たって生存者の半数が入区していれば到着とみなし、90秒たっても着かなければ任務へ帰る（DefendFallBackStuck）。後退先の中心は地形の高さへ補正する。
- 避ける「見たので止まる」分岐は末尾の KNOWN ENEMY / HOLD FIRING POSITION。JOIN FIGHT／SEEK ENEMY 内の同文の分岐は、既に Engage の別任務なので対象に含めない。SEIZE の FIRE も別の試験の規則を維持する。
- 対象は CanSupport／AcceptSupport／TryFlank で支援・側面移動に選ばない。区画AIの CoverCheck・PinnedFlank・CloseIn も対象から外す。
- 攻撃の対象の点は既存の PickAttackFocus。Mission を保つので、Think の3秒ごとの確認、45度の向きの変化／対象出現、10秒間隔の遮蔽再計画が接触中も働く。この既存の再計画で DefendReplan を記録する（行ログは出さない）。
- OrderAllowsTarget は区画任務なら元から防衛半径を課さない。一方 Think と WeaponOrderAllows は区画外を別に拒否していたので、対象小隊について外の観測敵も許可する。射程・個人の視認・射撃判定は従来のまま。
- SectorDefendPlace は状態の Cell（状態作成前は任務の目的区画）を SectorCellFor で照合する。接触時の遮蔽の Anchor／Peek、CoverFirePoint／RevalidatePeek、ContactFirePoint、StartCoverApproach、StartHuddle、StartFirePosition、KnownTarget Reach の投影後の地点・覗き地点・経路の検査へ接続した。既存の外向き接触の例外でも区画外へ出ない。
- 到着時の遮蔽スロット選びは既に区画内限定。追加でスロットと Nav 投影後の集合／開けた到着地点を検査する。個人の SoldierMove は既存の区画内 Anchor 制限と、今回制限した ContactFirePoint を通るので専用の分岐を増やさない。
- Follow の入口で、既に進行中の区画外への遮蔽行動・KnownMove を解除する。個人への明示命令は優先する。

### 4.2 値2：区画から区画へ下がる

- 既存の Emergency が成立し、退却理由が DAMAGE 5s ... になった場合だけ置換する。式は Threats>0 かつ O.RecentDamage >= T->Health * S->Members.Num() * T->SquadRetreatDamageRatio。弾切れ・人数・HPの理由が先なら置換しない。対戦車退避なども従来どおり先に処理する。
- 周り8区画から、Valid・Impassableでない・自軍本拠地へ近づく・軍の KnownSpots がない区画を選ぶ。点数は遮蔽スロット数＋自軍兵がいる場合2。点数同点なら本拠地への距離、さらに同点なら自軍の向きで読んだ X、Y の小さい順。
- スロット数は到着処理と同じ UBattleCover::GetSlots を1秒ごとに区画別集計する。選択のために BuildSlots や新しい光線／Nav 検索は行わない。自軍兵と目撃は後退の選択時だけ集計する。
- Apply(Engage, Move, 隣の区画中心, "SECTOR DEFEND / FALL BACK")。Mission は Apply で受理されないため Engage を使用し、DefendingSector により到着計画・攻撃の対象の向き・Follow の区画扱いを拡張する。MissionDestination 等は変更しない。
- 移動中は通常の退却と同じ Move 命令で交戦停止せず、接触遮蔽／Reach の寄り道も禁止する。生存者全員が入区してから Defend（計画は Engage、理由 HOLD BACK）にする。過半数の時点で残りの兵が止まらないための判断。入区不能の兵に対する新しいタイムアウトは設けていない。
- 次の後退は10秒以上あける。10秒未満の同じ DAMAGE 理由はその場の計画を維持する（通常の退却へは落とさない）。健康・補給・対戦車の理由はこの待ち時間でも優先。候補なし／Apply拒否は DefendFallBackNone を記録して通常の退却へ。
- 入区後、最後の被害から Back 秒、被害なしで ResumeMission。被害時刻は既存の DamageHistory の1秒バケット末尾を使うため最大約1秒保守的。復帰先は最新の元任務。SeizeEnded には登録しない。取り返す専用の計画は作らない。

### 4.3 状態と記録

TMap<int32,FSectorDefend> SectorDefends：Cell、Start、LastSeen、LastReplan、BackAt=-1、From。From は直前の後退元。加えて LastDamage・ArrivedAt・OrderRevision を保持し、被害のない時間・入区・明示された別命令を区別する。Reset は Seize と同じ Reset 関数で親切替の内側。

最初の接触で DefendStay。後退していない状態は最後の自分の視認から30秒で DefendEnd として閉じる。後退中は Back の復帰条件を優先する。退却・補給・再編、権限喪失・命令変更でも閉じる。後退時 DefendFallBack、復帰時 DefendReturn。復帰は return だけを記録し、end を重ねない。

行は BATTLE_SECTOR_DEFEND t=... squad=... team=... cell=x,y what=stay|back|return|end from=x,y secs=...。再計画は Event のみ。

実装の主処理は Prototype/BattleSectorDefend.cpp。既存ファイルは呼び出し・条件の拡張。守りの配置（arms）とは別の層。ビルド・UE・試合・固定場面試験は今回の依頼により実施していない。

## 5. 遮蔽：相手に近く、相手に対して遮蔽になる地点 `-BattleCoverNearEnemy=1`

到着時の遮蔽地点（区画任務と 3章の「取りに行く」）の選び方を変える。

1. **相手の点**：攻撃の対象の点（`PickAttackFocus`：目的の区画と周り8区画で敵が最も多い区画の平均位置、無ければ200m以内でいちばん近い既知の敵）。
2. **遮蔽になるか**：今の「対象の方向±30度の2.5m先に壁」に加えて、相手の点から実際に隠れるかを確かめる（`BattleTactics::ExposureAt`、しゃがんだ高さで5点中2点以下）。検査は1計画48スロットまで。
3. **近い順**：2 を通った地点を、**相手の点に近い順**に並べる（今は区画の中心に近い順）。同じ距離（2m 刻み）は今の順。
4. **相手が分からないとき**：敵のいる隣の区画（区画の状態の表 `SectorState` の敵あり）、無ければ敵の本拠地側の隣の区画の中心を相手の点の代わりにする（区画の中心から外側の、敵に近い辺の遮蔽に入る）。
5. 撃てる遮蔽（`-BattleCoverFirePoint`）・屋内優先とは、段（撃てる → 隠れるだけ）の中の並びとして合わせる。

記録：`arrival_near_enemy_plan`（この並びで計画した）、`arrival_near_enemy_slot`（選んだ地点）、`arrival_near_enemy_exposed`（隠れないので外した）、選んだ地点から相手の点までの距離の合計（平均を出す）。

接触時の遮蔽（`RequestCoverAction`）は今回触らない（別の採点。次の段）。

実装の注意（d48a1c37）：敵を知らないとき（4 の本拠地側）は隠れるかの検査をしない。検査の上限48を超えた候補は「不明」として各段の最後に残す（選ばれることがある）。距離は4つの帯（30m 未満・30〜60m・60〜100m・100m 以上）の数で記録する。

## 6. 進め方と評価

順番はユーザー決定のとおり **攻撃 → 遮蔽 → 維持**（遮蔽は攻撃の到着で使うので2番目）。

各切替で、試合数の決まり（`Docs/design-match-budget.md`）の段を守る。

1. ビルド、`Test-Battle`。切替0が同じビルドの基準と `IDENTICAL`（マップK s7、1本）。
2. 切替1の1試合（マップK s7）で、意図した行動の数を見る（0なら直す）。
3. 2 seed（マップK s7・43、両軍あり）を切替0と比べる。見る値：与えた損害・撃破・戦死（陣営ごと）、得点、`ENGAGE` の割合、遮蔽からの射撃、遮蔽なしの被弾、射撃の距離。
4. 既定にするかはユーザー判断。片側試験（`-BattleSectorSeizeTeam`）は、両軍ありで向きがそろってから。

## 7. 触る場所

| 切替 | 場所 |
|---|---|
| `-BattleSectorSeize` | `BattleTactics.h/.cpp`（切替）、新規 `BattleSectorSeize.cpp`（区画の表・目標選び・状態）、`BattleSquadRetreat.cpp`（`Evaluate` の Engage の分岐と「向かう」の前）、`BattleSquadTactics.h/.cpp`（状態・リセット）、`BattleStableMovement.cpp`（到着の計画を区画の扱いに）、`BattleLeaderRuntime.cpp`（攻撃の対象の点を持つ条件） |
| `-BattleCoverNearEnemy` | `BattleStableMovement.cpp`（`BuildMovementPlan` の候補の並びと検査）、`BattleLeaderRuntime.cpp`（相手が分からないときの点） |
| `-BattleSectorDefend` | `BattleSquadRetreat.cpp`（接触時の分岐、退却の行き先）、`BattleKnownTargetMove.cpp`（区画の中に限る） |

## 8. 危ない所

- 「向かう」（既定）と同じ小隊を取り合う。`-BattleSectorSeize=1` のときは「取りに行く」を先に見て、選べなければ「向かう」に落とす。
- 持ち場を空ける：前線軍の小隊も対象なので、前線が薄くなる。上限（1区画3小隊）と180秒の打ち切りで抑えるが、数えて確かめる（前線の持ち場にいる小隊の数）。
- 取りに行った区画で孤立する：足場の加点と、被害による退却（今の決まり）に任せる。
- 敵に近い遮蔽は被弾が増えうる。遮蔽なしの被弾と戦死で見る。
- 区画の境の交差点は東西で読み方が違う（`-BattleSectorSymmetric`）。区画は `SectorCellFor` で読む。
