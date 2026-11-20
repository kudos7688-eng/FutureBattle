# 遮蔽の状態 → 遮蔽アニメの対応表（2026-11-19）

> **2026-11-20 0時40分 追記：この写しは古い。** 正本は `blindfire` ブランチの `Game/FutureBattle/Source/FutureBattle/Prototype/BattleCoverClips.h` と `Docs/COVER_CLIPS.md`。取り込み後の計測（`FutureBattle-blindfire/Game/FutureBattle/Saved/Logs/ImportBodyAnims2.log` の `BATTLE_BODY_CLIP`）で、未決事項の1と2は次のように決まった。
> - 向きの補正は全部0度。隠れる姿勢のクリップは自分で体を横へ回している（肩の線の向き：低・右 +94°、低・左 −130°、高・右 +92°、高・左 −151°）。撃つクリップは敵の方を向く（+23〜+66°。立った射撃姿勢が0°、普通の構えが+34°）。180度にすると隠れた体が敵を向いてしまう。
> - 低い遮蔽の「上」で撃つクリップは使わない（段階4は今の立ち構え）。AimU_down は立った体（頭158cm）で銃を頭上（手168cm）に掲げて下へ向ける姿勢で、上から撃つブラインドファイアに使う。CoverR2AimU は立ち姿（頭159cm）で終わるので、出るクリップとしてそのまま使う。
> - 残る確認：肩の線の向きだけでは、隠れた体が遮蔽に背を向けているか、遮蔽の方を向いているかは区別できない（低・右の銃の向きは前方9°）。画面で見て、不自然なら隠れる姿勢だけ回す。

ユーザーの依頼で、アセット（TPS遮蔽シューター）の遮蔽アニメを使うための対応表を作った。**表示だけの話で、規則・視線・時間は変えない。**

## 分担

| 担当 | 範囲 |
|---|---|
| 「カバーアクション現状確認とTPS視点実装」（作業ツリー `FutureBattle-blindfire`、ブランチ `blindfire`） | 取り込み（`BattleBodyAnimImport.cpp`、23本を `/Game/Characters/Mannequins/Anims/Rifle/Cover/` へ）、再生（隊長は `ABattleUnit::UpdateBodyAnim`、部下は `UBattleLeaderRuntime::FollowerClip`）、ブラインドファイア、クリップの計測ログ `BATTLE_BODY_CLIP` |
| 「アセット参考資料の検討」（このメモ） | 遮蔽の状態 → クリップの対応表（[BattleCoverClips.h](BattleCoverClips.h)）と、その判断の根拠・未決事項 |

`BattleCoverClips.h` は計測中に Source を変えないため、ここ（Docs）に置いた。`blindfire` 側が `Game/FutureBattle/Source/FutureBattle/Prototype/` へ写してつなぐ。ヘッダーだけで完結し、ワールドも UObject も読まない。

## 遮蔽の段階（`FBattleUnitState::CoverPhase`、`BattleCoverAction.cpp`）

| 段階 | 意味 | 今の姿勢 |
|---|---|---|
| 0 | 遮蔽の動作なし | 通常 |
| 1 | 隠れる位置（`CoverAnchor`）へ移動中 | 立ち（移動） |
| 2 | 隠れている（0.8秒以上） | しゃがみ |
| 3 | のぞく位置（`CoverPeek`）へ移動中 | 立ち（移動） |
| 4 | のぞいて撃つ（AIは `AICoverPeekSeconds`、プレイヤーは1.2秒） | 立ち |

遮蔽の取り方は3種類ある（`RequestCoverAction`）。

- **立ち上がって撃つ**（低い遮蔽。`CoverPeek == CoverAnchor`）：しゃがめば隠れ、立てば敵が見える場所。→ 側は **上（Up）**、高さは **低**。
- **横へ出て撃つ**（左右へ2.2mか4m出る）：→ 側は、隠れる位置から敵を見て、のぞく位置が右か左か（`SideOf`）。高さは、隠れる位置で立った目から敵への線が地形でふさがれていれば **高**、通っていれば **低**（`HeightOf`。動作の開始時に1回だけ線を引く。毎フレームは引かない）。
- **接触時の射撃位置**（`-BattleContactCover`）：横へ出る型と同じ扱い。

## 対応表（`BattleCoverClips::Pick`）

| 高さ・側 | 隠れる（段階2、繰り返し） | 隠れ始め（1→2、1回） | 出る（段階3、1回） | 撃つ（段階4、繰り返し） | 戻る（4→、1回） |
|---|---|---|---|---|---|
| 低・右 | AS_coverlo_R_idle | AS_CoverLo_Idle2CoverR | AS_CoverLo_CoverR2AimR | AS_CoverLo_AimR | AS_CoverLo_AimR2CoverR |
| 低・左 | AS_CoverLo_CoverL | （そのまま重ねて切替） | AS_CoverLo_CoverL2AimL | AS_CoverLo_AimL | AS_CoverLo_AimL2CoverL |
| 低・上 | AS_coverlo_R_idle | AS_CoverLo_Idle2CoverR | AS_CoverLo_CoverR2AimU | AS_CoverLo_AimU_down（仮） | AS_CoverLo_AimU2CoverR |
| 高・右 | AS_cover_hi_r_idle | AS_CoverHi_Idle2CoverR | AS_CoverHi_CoverR2AimR | AS_CoverHi_AimR | AS_CoverHi_AimR2CoverR |
| 高・左 | AS_CoverHi_CoverL | （そのまま重ねて切替） | AS_CoverHi_CoverL2AimL | AS_CoverHi_AimL | AS_CoverHi_AimL2CoverL |

- 高い遮蔽に「上」は無いので右として扱う（起こらない組み合わせだが、安全のため）。
- 23本すべてが取り込み表にある名前。UE4 の `cover/` にしか無い4本（AS_CoverLo_AimU、AS_coverlo_R_aim、AS_CoverHi_CoverL2CoverR、AS_CoverHi_WalkL1）は使わない。
- 遮蔽に沿って横に歩く4本（AS_CoverLo_WalkL、AS_CoverLo_WalkR2_new、AS_CoverHi_WalkL、AS_new_CoverHi_WalkR）は、ゲームに「遮蔽に付いたまま横へ動く」状態が無いので使わない（段階3の横への移動は2.2〜4mあり、出る1回のクリップと通常の移動で足りる）。
- 段階1（隠れる位置への移動）は通常の移動のクリップのまま。到着したら「隠れ始め」を1回流し、終わったら「隠れる」へ。
- 部下（Mass の近い体）は1本の全身クリップなので、1回だけのクリップは使わず、段階2は「隠れる」、段階4は「撃つ」、それ以外は今のまま。遠い VAT 表示は変えない。

## 向きの補正（`BattleCoverClips::YawOffset`）

ゲームでは、隠れている体も敵の方を向いている。隠れるクリップは壁に背を付けるので、メッシュだけを回す。**初期値は推測**で、段階2は180度（敵に背を向けて壁に寄りかかる）、撃つクリップは0度。`blindfire` 側の取り込みログ `BATTLE_BODY_CLIP`（骨盤の位置・銃の向き）が出たら、クリップごとの値に置き換える。

## 未決事項

1. **低・上で撃つクリップ**：規則では段階4の射撃位置は立った目の高さ。UE5 の組に純粋な AimU の繰り返しが無く、AimU_down を仮に当てた。計測で頭の高さが立った目より大きく低ければ、段階4は今の通常の立ち構えのままにする（見た目と当たり判定が食い違うため）。UE4 の AS_CoverLo_AimU を使う場合は骨格の変換が要り、こちらの担当。
2. **向きの補正**：上のとおり、計測待ち。
3. **左右の偏り**：低・左と高・左には「隠れ始め」が無い。右と見た目の差が気になれば、右の Idle2CoverR を左右反転して作る（ミラーデータテーブルが要る。今は作らない）。
4. **性能**：表を引くのは段階が変わったときだけで、毎フレームの追加の線引きは無い。高さの判定は遮蔽の動作1回につき線1本。

## 次の一手

1. `blindfire` 側：このヘッダーを Source へ写し、隊長の `UpdateBodyAnim` と部下の `FollowerClip` で段階に応じて引く。
2. 取り込み後の `BATTLE_BODY_CLIP` を見て、`YawOffset` と未決事項1を決める（こちらが値を出す）。
3. 画面での確認（ユーザー）。
