# 兵種の引き算の評価（ROADMAP 段階3の詳細設計）

2026-11-22 0時30分 Claude（ロードマップに従う改善活動）。**設計のみ。未実装・未計測。** 出どころは ROADMAP 11-17（「兵種ごとの評価は、全兵種がそろった状態から1種類ずつ抜く引き算の片側試験で行う」ユーザー決定）と 11-20「今後の実装計画」段階3。数値は叩き台。

## 1. 何を確かめるか

- 各兵種に役割があるか（抜くと負けるか）、強すぎ・弱すぎが無いか。
- AI が買う既定の数（`-BattleAreaComposition` を既定にするか）を決める材料。
- 外骨格兵・自走砲が強すぎる見込み（ROADMAP 11-18）を数字で確かめ、値を調整する（数値の調整は Claude に任されている）。

## 2. 今の引数では測れない点（先に直す）

**問題**：兵種の保有数の引数（`-BattleAreaTankKeep` など）と `-BattleAreaComposition` は、`-BattleAreaTrialTeam` を付けると**試験側だけ**に効く（`AreaIntent` の `bTrial`、`BattleTactics::AreaComposition(Team)`）。相手は設定の既定（特殊な兵種を買わない）に戻る。そのため片側試験では「全兵種 − X」対「歩兵だけ」になり、「全兵種 − X」対「全兵種」が作れない。

**直し方（試験切替を2つ足す。どちらも既定0、既定の試合は不変）**

| 引数 | 効く範囲 | 中身 |
|---|---|---|
| `-BattleAreaAllTypes=1` | **両軍**（`-BattleAreaTrialTeam` に関係なく） | 「全兵種」の構成で買う：`AreaComposition` の順序（装甲車1→2、迫撃砲班1→2、6回目の購入から戦車1〔敵車両を知れば2〕、9回目から自走砲1、敵車両の数だけ対戦車班）に、外骨格兵1（6回目から）、補給車1、偵察組2、重装歩兵の割合0.2を足したもの |
| `-BattleAreaWithout=<兵種番号>` | 試験側（`-BattleAreaTrialTeam` があればその側、無ければ両軍） | その兵種の保有数を0にする（重装歩兵は割合を0）。小銃（0）は抜けない |

置き場所：`UpdateAreaHiring`（BattleArea.cpp:141）で `BattleTactics::AreaComposition(Team)` を見ている所に「`AllTypes` なら両軍で `AreaComposition` と追加の保有数を適用」を足し、最後に `Without` を当てる。`AreaIntent` の保有数の枝（試験側だけの上書き）は今のまま。`-BattleAreaAllTypes` があるときは保有数の CLI 上書きを使わない（混ぜると意味が分からなくなるため。起動時に警告を出す）。

**注意**：小隊の上限は25のまま（`-AreaMaxSquads` を変えない）。特殊な兵種は小銃の枠を置き換える。抜いた側はその分だけ小銃が増える。これが「引き算」の意味（同じ資源で、その兵種の代わりに小銃を持つ）。編成AI（`-AreaCompositionPlanner`）は上限25ではほとんど買わない（181判断中178が上限待ち、STATUS 11-21 11時10分）ので、この評価では使わない。

## 3. 条件

| 条件名（`Run-EconomyMatrix` に足す） | 引数 |
|---|---|
| `ALL`（基準。両軍） | `-AreaAllTypes 1` |
| `W1` 重装歩兵なし | `-AreaAllTypes 1 -AreaWithout 1` |
| `W2` 偵察組なし | `-AreaAllTypes 1 -AreaWithout 2` |
| `W3` 対戦車班なし | `-AreaAllTypes 1 -AreaWithout 3` |
| `W4` 戦車なし | `-AreaAllTypes 1 -AreaWithout 4` |
| `W5` 装甲車なし | `-AreaAllTypes 1 -AreaWithout 5` |
| `W6` 迫撃砲班なし | `-AreaAllTypes 1 -AreaWithout 6` |
| `W7` 自走砲なし | `-AreaAllTypes 1 -AreaWithout 7` |
| `W8` 外骨格兵なし | `-AreaAllTypes 1 -AreaWithout 8` |
| `W9` 補給車なし | `-AreaAllTypes 1 -AreaWithout 9` |

- 実行：`Run-StrategyScreen.ps1 -Conditions W1,...,W9 -Baseline ALL -Maps E` と `-Maps I`（試験側の得点差 −「ALL 対 ALL」でのその側の得点差、を seed ごとに出す。9 seed の逐次打ち切り、30FPS で選別 → 60FPS で確認）。
- 遮蔽行動ON（`-CoverActions 1 -FlankCoordination 1`）は `Run-EconomyMatrix` が既に付けている。
- 所要時間の目安：1マップあたり最大で 9条件×36試合＋基準36試合 ≈ 360試合。12並列・30FPS で1組（12試合）約6分 → 最大約3時間。打ち切りで多くは半分ほどになる見込み。ロックを長く持つので、夜間などほかの作業が少ないときにまとめて回す。

## 4. 読む値

1. **得点の差（主）**：W_X の試験側の得点差の増減。
   - 大きく**悪くなる**（−2se より下）：その兵種に役割がある。
   - 大きく**良くなる**（+2se より上）：その兵種は今の値・使い方では損（弱すぎるか、AI の使い方が悪い）。
   - 差が無い：役割が小さい、または他の兵種で置き換えが利く。
2. **兵種ごとの効率（副）**：`ALL` 対 `ALL` の試合の `metrics.json` の `ally_type_activity`／`enemy_type_activity`（兵種ごとの延べ時間・射撃・与ダメージ・撃破・被ダメージ・死亡）から、費用あたりの撃破と与ダメージ、撃破÷死亡を出す。小銃を1として比べる。
   - 費用あたりの撃破が小銃の2倍を超え、かつ W_X で大きく悪くなる兵種は**強すぎ**の候補（外骨格兵・自走砲の見込みを確かめる）。
   - 延べ時間が短い兵種（買ってもすぐ失う、または本拠地で修理ばかり）は使い方の問題として個別に見る。
3. **集計スクリプト**：`Setup/Analyze-Subtraction.py`（新規）。上の2つを1つの表にまとめる。

## 5. 決め方

- 強すぎ：費用を上げる（例：外骨格兵 30→40）か、値を下げる。直したら、その兵種の W_X だけ測り直す。
- 弱すぎ・損：まず AI の使い方（戦術層の割り振り、`BattleArmourAI` の規則）を見る。使い方に問題が無ければ費用を下げる。
- 偵察組（W2）は戦略層（`-BattleStrategy`、`Docs/design-strategy-layer.md`）が無いと価値が出ない（11-17）。段階3では参考値とし、戦略層の S2 の後に `-Strategy 2` を両軍に付けて測り直す。
- 既定にするか（`-BattleAreaComposition` の既定化、AI が外骨格兵・補給車を買う既定）はユーザーが決める。そのときの材料として、上の表と「ALL 対 既定（歩兵だけ）」の片側試験を添える。

## 6. 先に済ませておくこと

| 事項 | 理由 | 担当・状況 |
|---|---|---|
| 段階1（区画戦の土台）の組み合わせの既定 | マップI の既定の動きが変わると測り直しになる | 区画AI（11-22 0時：`-BattleArrivalWalk` を既定にする作業中） |
| 段階2（計測の信頼性） | 表示の変更で試合が変わると、比べる基準がずれる | 未着手 |
| 自走砲への対抗手段（撃つと位置が知られる・射撃陣地・陣地への襲撃） | 自走砲は相手に狙う手段が無いと強すぎて当然 | Codex委任（ユーザー承認 11-21、既定0の試験切替）。評価試合は未実施 |
| 戦車と歩兵の同行 | 戦車が単独で動くと W4 の差が「使い方」で決まってしまう | 未着手（部隊群の段階4で群に戦車を入れる形は既にある） |

## 7. 実装の順

1. `-BattleAreaAllTypes`・`-BattleAreaWithout` を足す（BattleArea.cpp・BattleTactics）。既定0の試合が一致することを確かめる（`Compare-MatchResults`）。`ALL` 対 `ALL` を1試合見て、全兵種が実際に買われていることをログ（`BATTLE_AREA_HIRE` の兵種）で確かめる。
2. `Run-EconomyMatrix` に条件 `ALL`・`W1`〜`W9` を足す。`Analyze-Subtraction.py` を書く。
3. マップE → マップI の順に `Run-StrategyScreen`。結果を STATUS に表で残し、強すぎ・弱すぎの兵種の値を直して、その兵種だけ測り直す。
