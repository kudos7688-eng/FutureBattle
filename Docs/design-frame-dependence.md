# 戦闘の計算のフレーム依存：一覧と、戦闘の時計を1系統で渡す入口の見積り（2026-11-25）

調査だけの文書で、コードは変えていない。固定刻みへ移すかどうかはユーザーの判断。
背景は `Docs/review-2026-11-25/05-standard-practice-comparison.md` の ③。

- 調べ方：Codex に読み取り専用で全体を洗わせ（基準は main `4f29df4`）、Claude が主な箇所を main `0d9ecfa` のコードで確かめ直した。**「確認」の列が ○ の行は Claude が行を読んで確かめたもの、△ は Codex の報告のまま**（行番号は `4f29df4` のもので、その後の main の進みで数十行ずれている場合がある。関数名で引くこと）。
- ビルド・試合はしていない。**30FPS と 60FPS の差のうち、どの項目がどれだけ勝敗に効いているかは測っていない。** 下の「秒あたり」は処理能力の上限や式から出した値で、実際の件数ではない。
- コードの場所は `Game/FutureBattle/Source/FutureBattle/Prototype/`。

## 1. 結論

1. 戦闘は World Tick に結びついている。`ABattleGameMode::Tick` が World の `DeltaTime` をそのまま `UBattleLeaderRuntime::Step` へ渡し、`Step` は World の時刻を `Now` として読む。戦闘だけの時計は無い（戦闘中だけ進む `UBattleMatch::Elapsed` と `UBattleSquadTactics` の `ActiveSeconds` は別にあり、時計は実質3本ある）。
2. **30FPS と 60FPS の差は「Dt の切り詰め」では説明できない。** `Min(Dt,.05)` は 1/30 秒でも 1/60 秒でも働かない。差を作るのは次の4種類。
   - **1フレームあたりの件数の予算**（個人の経路探索・遮蔽探索・個人の移動先探し・遮蔽の取り直し）：処理能力が FPS に比例する。
   - **期限の丸め**（`Now>=期限` を Tick ごとに見る）：期限を過ぎた最初の Tick で処理し、遅れは次の期限にも載る。追いつきの実行は無い。
   - **移動と衝突の刻み**（一歩＝速さ×Dt、その場で sweep と slide）：一歩の長さが半分になり、当たる順番と位置が変わる。
   - **フレーム番号を値に使う箇所**（被弾方向の推測の誤差）：FPS が直接結果に入る。
3. 計測の「固定刻み」（`-UseFixedTimeStep -FPS=30`）は World 全体の刻みを決める方式で、戦闘だけを固定の刻みで回して描画を別にする作りではない。試遊（`Setup/Play-Area.ps1`）は固定刻みの引数を渡さないので、描画の速さのまま戦闘が進む。
4. 最小の一歩（挙動を変えずに、時刻・Dt・Tick 番号を1つの構造体で `Step` へ渡す入口を作る）は 1〜2日。固定刻みそのものは別の大きな仕事で、隊長の CharacterMovement・部下のカプセルの即時更新・砲塔の角度（表示の部品）・伏せの切り替え（アニメの状態）が境界になる。

## 2. 一覧

列の意味：**30/60** は 30FPS と 60FPS での違い（秒あたり）。**切り離し** は固定の時計へ移すときに表示・入力・CharacterMovement・UE のナビ/物理から切り離せるか（○ 切り離せる／△ 条件つき／× 今のままでは切り離せない）。

### 2.1 入口・判断・フレームごとの予算

| 場所（関数） | 何がフレームに依存するか | 30/60 | 今出ている症状 | 切り離し | 確認 |
|---|---|---|---|---|---|
| `BattleGameMode.cpp:169–172` `ABattleGameMode::Tick` | World の `DeltaTime` をそのまま `Runtime->Step` へ | Step は 30回/秒・60回/秒 | すべての元 | △ 入口は変えられる。入力と CharacterMovement は別の Tick | ○ |
| `BattleLeaderRuntime.cpp:1626,1639` `Step` | `Now` は World の時刻 | ― | ― | ○ | ○ |
| 同 `:1689` `Step` → `Follow` | 兵の更新だけ `Min(Dt,.05)` | 通常はどちらも切り詰めなし。100ms のフレームでは兵の時間だけ 50ms 捨てる | 遅いフレームで、移動と、砲弾・装填・シールドの進む時間がずれる（実害は未計測） | ○ まず切り詰めをそのまま保つ | ○ |
| 同 `:1648,1654–1655,1691,1693` | `Match->Update`・発射筒・シールド・砲弾・Arms は元の Dt | 同上 | 同上 | ○ | ○ |
| 同 `:1661–1664` `Step` | `-BattleThinkCap=N`（既定0＝上限なし）：1 Step に N 件 | 30N・60N 件/秒 | 既定では無し | ○ | ○ |
| 同 `:632` `Think` | `NextThink=Now+max(.2,SquadDecisionSeconds)`（既定0.5秒）。追いつき無し | 理想はどちらも 2回/秒。遅れが次の期限に載る | ― | ○ | ○ |
| 同 `Start`（`:200,204` 付近） | 最初の判断を `.05×(小隊ID%10)` でずらす | 50ms の位相は 30Hz の格子に乗らない | ― | ○ | △ |
| 同 `:1072–1082` `Follow`（遮蔽探索） | 通常の遮蔽探索は `AICoverProbesPerFrame`（設定値2）件/フレーム。被弾優先の枠は別（4件/フレーム）。隊長優先は枠の外 | 通常 60・120 件/秒、被弾優先 120・240 件/秒 | 混雑時に遮蔽を探すまでの待ちが FPS で変わる | ○ 採る順番が結果に効く | ○ |
| `BattleStableMovement.cpp:643–649` `StableMove` | 個人の経路探索 2件/フレーム。関数内 static の計数なので、**同じプロセスの全小隊・全 World で共有** | **60・120 件/秒** | 塀で分断された小隊が自分の経路を得るまでの待ち（B05 と同じ場所）。30FPS では枠が半分 | △ 時計の移管と、static を World ごとに分けることは別の変更 | ○ |
| `BattleSoldierMove.cpp:56,72` `StepSoldierMove` | 位相つきの1秒の境界を `floor(Now+位相)` で検出。探索 4件/フレーム。枠に落ちるとその境界では再試行しない | 上限 120・240 件/秒 | 既定0の試験切替（`-BattleSoldierMove`）のときだけ | ○ | ○ |
| `BattleThreatCover.cpp:134,205` `CheckCoverThreats` | 同じ1秒の境界。取り直しの探索 2件/フレーム、枠待ちは持ち越す | 上限 60・120 件/秒 | 既定0の試験切替（`-BattleThreatCover`）のときだけ | ○ | ○ |
| `BattleSoldierMove.cpp:117–119` `SoldierAIFocus` | フレームごとのキャッシュ | 更新 30・60回/秒 | 既定0 | △ プレイヤーの視点から決まる値 | ○ |
| `BattleLeaderRuntime.cpp:1797` `RevealShooter` | 推測の誤差の向きを `hash(兵, 射手, GFrameCounter)` で作る | 同じ時刻・同じ被弾でもフレーム番号が違えば推測地点が違う（誤差の半径は距離の10%） | 30FPS の計測と実プレイで、撃たれた方向の推測が別の値になる。該当の機能が有効なとき | ○ Tick 番号か被弾の通し番号に替えられる。替えた時点で結果は変わる | ○ |
| 同 `:302` `GuardFalls` | `NextFallCheck=Now+.25` | 30Hz では 8 Tick＝0.267秒、60Hz では 15 Tick＝0.25秒 | ― | △ 地形と Actor の位置を読む | ○ |

`Step` の中の順番（混成の経路）：試験 → 銃の向きの表示 → 体・死体の表示 → `Now` → 装填 → `Match` → `SquadTactics` → 発射筒・シールド → 外骨格 → 視線のマスク → 知覚 → `Think` → 落下 → 回避用の位置の表 → **兵を Bodies の順に `Follow`（移動と射撃が1人ずつ入り交じる）** → 砲弾 → Arms → 終了判定 → 記録 → 表示。先の兵の移動・戦死は、後の兵が読む状態に効く。時計をそろえても、この順番の依存は残る。

### 2.2 移動・CharacterMovement・表示との境界

| 場所（関数） | 何がフレームに依存するか | 30/60 | 切り離し | 確認 |
|---|---|---|---|---|
| `BattleStableMovement.cpp:605` `StableMove` | 部下（Mass のカプセル）の一歩＝`min(速さ×Dt×…, 残り)` | 一歩は v/30・v/60。直線で当たらなければ秒あたりの距離は同じ | △ カプセルと衝突の場を sim の更新に合わせる | ○ |
| 同 `:610–633` | 一歩ごとに sweep、当たると slide。止まる余白は `Hit.Time-.001`（一歩の長さに比例） | sweep の長さと slide の始まる位置が変わる | △ ほかのカプセルを動かす順番まで固定が要る | ○ |
| 同 `:635` | `Velocity=(位置の差)/max(Dt,.001)` | 当たった Tick の速さの値が変わり、静止の判定・射撃の精度へ波及 | △ 正規の速さを sim の状態として持つ | ○ |
| 同 `:595–600` | 隊長（ACharacter）は `AddMovementInput` だけ。実際に動かすのは CharacterMovement の Tick | 部下の手計算の積分と別の刻み方 | **△ 入力を渡すだけでは固定の sim にならない** | ○ |
| 同 `:637–661` | 40cm 進むと進捗の時刻を更新。1秒で停止扱い、2秒で個人の経路、4秒で小隊の再計画。再試行は3秒・1秒 | 秒数は共通。しきい値を越えたことに気づく遅れと、探索の枠待ちが違う | △ 進捗の入力が物理の位置 | ○ |
| 同 `:461–469` | **診断だけ**：`LastMoveSampleFrame` が連続し 40cm 以内なら `StationaryMoveSeconds+=Dt`、3秒で `BATTLE_MOVE_STALL` | 詰まりの数（評価の指標）が、切り詰めや `Follow` を通らない Tick で変わる | ○ 移動を止める値ではない | ○ |
| `BattleLeaderRuntime.cpp` `Follow`（安定移動でない経路） | `速さ×Dt` のカプセル移動、差分の速さ、2秒の停止判定 | 同上 | △ | ○ |
| `BattleCoverAction.cpp` `StepCoverApproach`・`StepCoverAction` | 接近・突撃の一歩、詰まりの積算、差分の速さ。遮蔽の中の移動は 250cm/秒 | 遮蔽の中の一歩 8.3・4.2cm | △ | △ |
| `BattleArms.cpp` `StepCharge` | 突撃の一歩・sweep・速さ | 接近と近接の成立の時点が変わる | △ | △ |
| `BattleSoldierOrder.cpp` `StepSoldierOrder` | 個人命令の一歩・衝突・停止判定 | 同上 | △ | △ |
| `BattleContactMovement.cpp` | 探索1秒・迂回路6秒・味方の通り抜け5秒などの期限 | 期限が来たときの位置と障害物が違う | △ | △ |
| `BattleVehicle.cpp` `SteerVehicle`・`AimTurret` | World の Dt を 100ms で切り、車体・砲塔を 角速度×Dt で回す | 砲塔 60度/秒なら 2度・1度ずつ | △ 砲塔の角度を正規の状態へ移す | △ |
| `BattleTankGun.cpp:110` `StepTankGun` | **表示の部品（砲塔の Component）の角度**が照準から3度以内で発射 | 2度刻みと1度刻みで、撃てる Tick が変わる | **× 正規の砲塔角と描画の角を分ける必要** | ○ |
| `BattleController.cpp` `PlayerTick` | 入力・壁遮蔽・射撃を描画側の Tick で処理 | 入力と発射の評価 30・60回/秒 | △ 入力を sim の Tick へ取り込む決まりが要る | △ |
| `BattleWallCover.cpp` `StepPlayerWallCover` | 壁へ押す時間を Dt で積算 | 成立の時点が Tick に丸まる | △ | △ |
| `BattlePlayerPath.cpp` `UpdateVault` | `VaultTime+=DeltaTime`、曲線上へ Actor を移す | 途中の位置の標本が 30・60個/秒（射撃・知覚の位置も変わる） | △ | △ |
| `BattleUnit.cpp` `SetSprinting` ほか | **伏せの切り替えのアニメの状態（TransitionUntil）**が移動速度0を決める | 切り替えの終わりを読む Tick が違う | **× 移動禁止の時間をゲームの状態へ移す必要** | △ |

### 2.3 知覚・戦術の期限

| 場所（関数） | 何がフレームに依存するか | 30/60 | 切り離し | 確認 |
|---|---|---|---|---|
| `BattlePerception.cpp:304–336` `Update` | 兵ごとの位相と走査の期限（通常0.2秒、未発見の側は0.4秒）。期限が来た兵をその Tick で走査。**「1フレームに N 人まで」という上限は無い** | 回数（5回/秒・2.5回/秒）は同じ。走査のときの位置が最大 33ms・17ms 単位でずれる | △ sim の位置と衝突の場を同じ時点で読む | ○ |
| 同（遅れの扱い） | `Due+=Step`。遅れすぎたら先の格子へ送る | 長く止まった後、抜けた走査は再現しない | ○ | △ |
| 同（発見・再発見・記憶） | 発見に要る時間・再発見の猶予・記憶の薄れを `Now` で判定 | 条件が続いた秒数と、確かめた Tick の両方に依る | ○ | △ |
| 同（報告の配送） | `Now+ShareDelay` で配送、期限が来た Tick で適用 | 同じ Tick の判断に間に合うかが変わる | ○ | △ |
| 同（並列の視線判定） | 同期の `ParallelFor`。結果は元の順で適用（16 は並列の粒度で、件数の上限ではない） | ― | △ | △ |
| `BattleLeaderRuntime.cpp` `CanSeeWithContext` | 組ごとの視線の結果を0.2秒未満は使い回す | 使い回す標本と失効の Tick が変わる | △ | △ |
| 同 `Think`（遮蔽の確認2秒・躍進4/7秒・詰め寄り8秒・重点3秒・再計画1秒） | 約0.5秒の判断の周期の上で、さらに Tick に丸まる | ― | ○ | △ |
| `BattleSuppression.cpp` | `clamp((ZeroAt-Now)×減衰,0,1)`、射撃ごとに期限を延ばす | 式は秒の基準。撃つ時刻が変わると制圧も変わる | ○ | △ |
| `BattleCoverAction.cpp` `StepCoverAction` | `CoverSeconds-=Dt` で隠れる・覗くの相を進める。余りを次の相へ回さない | 1/30・1/60 ずつ | ○ | △ |
| `BattleCover.cpp` `Reserve` | 遮蔽の予約は `Now+2.5秒` | 空きの判定の Tick が変わる | ○ | △ |
| `BattleArms.cpp:266,285,297` `StepArms` | 近接・突撃の候補は `Now+.25`、手りゅう弾などは `Now+.5`。追いつき無し | **0.25秒の処理は理想値で 3.75回/秒・4回/秒**。0.5秒はどちらも2回/秒 | ○ | ○ |
| `BattleSquadTactics.cpp` `Step` | 自前の `ActiveSeconds` に Dt を足し、1秒ごとに観測。標本の Dt は最大2秒に切る | 通常は1回/秒 | ○ World の時計と別であることを保つ | △ |
| `BattleSquadRetreat.cpp`・`BattleTeamAI.cpp`・`BattleSquadFlank.cpp`・`BattleSquadSupport.cpp` | Age・SafeSeconds・再試行・情報の鮮度（2.5/3.5秒など） | 期限をまたぐかで採否が変わる | ○ | △ |

### 2.4 射撃・砲弾・装填

**砲弾を毎 Tick 積分して飛ばす処理は無い。** 小銃・対戦車弾・主砲・外骨格の発射筒は発射した Tick の線の判定、迫撃砲・自走砲・手りゅう弾は着地点を発射時に決めて残り時間を減らす方式。

| 場所（関数） | 何がフレームに依存するか | 30/60 | 切り離し | 確認 |
|---|---|---|---|---|
| `BattleState.cpp:309–312` `ConsumeShot` | `Now<NextShotTime` なら撃てない。撃てたら `NextShotTime=Now+max(.05,間隔)`。足りない分をまとめて撃つことは無い | **実際の間隔 ≈ ceil(間隔/Dt)×Dt**。0.16秒は 30Hz で5 Tick＝0.167秒、60Hz で10 Tick＝0.167秒。0.05秒は 15発/秒・20発/秒 | ○ | ○ |
| `BattleLeaderRuntime.cpp` `Follow`・`Fire` | 兵ごとの発射をその Step で評価。指切り（`BurstLeft`）と休みも期限 | 更新の機会 30・60回/秒。遮蔽の相との組み合わせが変わる | ○ | △ |
| `BattleState.cpp` `StartReload`・`AdvanceReloads` | 装填の終わりは World の絶対時刻。広域の場では AI が止まっていても装填は進む | 完了を見る間隔 33・17ms | ○ | △ |
| 同 `UpdateAim`、`BattleTactics.cpp` `SpreadDegrees` | 静止し始めた時刻・向きの変化・速さで照準の確定と散らばりを決める | 速さの標本が違うので、同じ乱数でも散らばりの角が違う | △ | △ |
| `BattleState.cpp` `AdvanceShields`・`AdvanceLaunchers` | 待ち時間を Dt で減らす。待ちが終わった Tick は再生せず次の Tick から | 境界の余りを再生へ回さない | ○ | △ |
| `BattleArtillery.cpp:801` `StepArtilleryShells` | `Remaining-=Dt`、0以下で着弾（着弾の Tick の位置で被害） | 着弾が1 Tick ずれると被害の人数が変わる。飛行中の毎 Tick の判定は無い | △ | ○ |
| `BattleArms.cpp:257` `StepArms`（手りゅう弾・榴弾） | 同じく残り時間を Dt で減らす。放物線の検査は発射時に8本（フレームあたりではない） | 爆発の時刻が Tick に丸まる | △ | ○ |
| `BattleTankGun.cpp` `FireTankGun` | 即時の線の判定。装填は `Now+Reload` | 砲塔の角度（上の ×）と Tick の評価が重なる | △ | △ |
| `BattleSupport.cpp` `UpdateSupport` | 支援の待ち・クールダウンを Dt で減らす | 着弾の Tick の対象の位置が違う | △ | △ |

### 2.5 経済・占領・雇用

式は秒の基準で、条件が一定なら秒あたりの量は 30FPS と 60FPS で同じ。違いが出るのは、境界を越えた Tick の位置・戦死・所有の状態。

| 場所（関数） | 方式 | 確認 |
|---|---|---|
| `BattleMatch.cpp` `Update` | `Elapsed+=DeltaSeconds`（戦闘中だけ進む。float の積算で、World の時刻と原点も止まる条件も違う） | ○ |
| 同 `AdvanceObjectiveControl` | 占領の進みと得点を Dt で足す。所有が替わったフレームは収入なし。拠点の収入は5秒ごとの支払い回数を floor で出し、余りを持ち越す | △ |
| `BattleArea.cpp` `UpdateArea` | 拠点の観測は0.2秒ごと（余りは Fmod、1回だけ）。エリアの収入は while で未払い分を処理。補給は約1秒ごとで、時計を0へ戻すので位相がずれる | △ |
| `BattleMatch.cpp` `Update`（委任の命令・エリアの監督） | 5秒で時計を0へ戻す。超えた分を捨てる。雇用と命令もこの周期 | △ |
| `BattleArea.cpp` `UpdateHireQueue` | `floor(Elapsed)` が変わったときに評価、各軍1件で打ち切り（各軍 最大1件/秒。FPS に比例しない） | △ |
| `BattleWarScore.cpp`・`BattleArtillery.cpp` `UpdateDepots`・`BattleGroups.cpp`・`BattleFrontLine.cpp`・`BattleSectorRoute.cpp` | Dt の積算か `Elapsed`・World の秒との比較 | △ |

### 2.6 保存・読み込みと乱数

- 保存は「全部が絶対時刻」でも「全部が残り時間」でもない。射撃・装填・個人の移動の期限は **残り時間** にして保存し、読み込みで今の `Now` を足す（`BattleSave.cpp`、`BattleMixedBodies.cpp` `RestoreMixedBodies`）。知覚の最後に見た時刻・配送の時刻は `Now` との差、**走査の期限（NextScan/LastScan）は消す**。`ActiveSeconds`・`Elapsed`・各時計はそのまま保存する。経路・行動の記憶の一部・指切りの残りは作り直すか空にする。保存の往復だけでは、続きの試合の完全な一致は保証されない。
- 乱数は「全員が1本の `FRandomStream` を順に使う」作りではない。射撃は `UnitID・発射の通し番号・TrialSeed` から毎回 stream を作る（`BattleTactics.cpp` `ShotDirection`）。主砲・砲兵・手りゅう弾・外骨格も ID と通し番号（または残数）。**フレーム番号が直接入るのは被弾方向の推測（`RevealShooter`）だけ**。FPS で結果が変わる主な理由は乱数の消費順ではなく、「撃つかどうか・何発目か・どこへ・誰に当たるか」が変わること。
- 補給後の手りゅう弾は同じ残数がもう一度出るので、同じ種がもう一度使われる（フレームとは無関係。気づいた点として記録）。

### 2.7 表示・音・計測・試験（試合の結果に入らないもの。1行ずつ）

- 表示・音：`BattleAudio`・`BattleEffects`・`BattleVATDisplay`・`BattleUnit` のアニメの間引き・HUD・死体・通知の寿命（フレームごとの予算、ID での間引き、補間）。切り離せる。**例外は上の2件（砲塔の角度で発射、伏せの切り替えで移動禁止）。**
- 計測：`FPlatformTime` は CPU 時間・壁時計の計測と試験の終了条件だけ。通常の戦闘の判断に「実際の CPU 時間が予算を超えたら打ち切る」使い方は見つからなかった。
- 古い経路・試験の経路：全 Actor の旧経路（`BattleAIController`・`BattleMixedDriver`）、`BattlePhase3`、`BattleSquadTrial`（`Now=Frame/60`）、`BattleFieldTrial`・`BattleMovingSquadTrial`（`Frame/30`）、`BattleNetwork`。通常のエリア戦とは別。試験の時計はフレーム数から作るので、World の FPS が違うと試験の中の時間の進みが変わる。

### 2.8 起動と刻み

| 何 | 刻み | 確認 |
|---|---|---|
| `Setup/Test-WideSight.ps1:162`（`-Experiment`） | `-UseFixedTimeStep -FPS=$Fps`（既定60、計測では `-Fps 30` を明示） | ○ |
| `Setup/Test-TerrainShot.ps1`（と、`movestall` ブランチの `Test-OwnRoute.ps1`） | `-UseFixedTimeStep -FPS=30` | △ |
| `Setup/Test-FlankBay.ps1` | `-UseFixedTimeStep -FPS=60` | △ |
| `BattleFieldReplay.cpp`・`BattleFieldViewTrial.cpp` | コードから 30Hz 固定 | △ |
| `Setup/Play-Area.ps1`（試遊） | **固定刻みの引数なし**。Config にも `FixedFrameRate`／`UseFixedTimeStep` の指定なし。描画の速さのまま戦闘が進む | ○ |

## 3. いま出ている、または出うる症状（まとめ）

確かめた事実と、式から言えることを分ける。

- **事実**：30FPS と 60FPS の試合は結果が違う（`MEASURE.md`「FPSが違う結果どうしは比べない」）。同じビルド・同じ条件の描画ありのマップEが 390秒から分かれる記録がある（STATUS 要約、原因は未調査）。
- **式から言えること**（どれだけ効いているかは未計測）：
  1. 個人の経路探索は 60件/秒と 120件/秒。塀で分断された小隊（B05）が自分の経路を得るまでの待ちが、30FPS の計測のほうが長い。混雑が解ける順番も違う。
  2. 遮蔽探索（通常 60・120 件/秒）も同じ。撃たれた兵が遮蔽を探し始める時点が違う。
  3. 一歩の長さが違うので、衝突・slide・速さの値・静止の判定・散らばりが違う。隊長（CharacterMovement）と部下（手計算）は別の刻み方。
  4. 砲塔は 2度刻みと1度刻みで回り、発射の成立の Tick が違う。
  5. 被弾方向の推測の誤差は、フレーム番号が直接入る。
  6. 0.25秒ごとの Arms の処理は 3.75回/秒と4回/秒。
  7. 50ms を超えるフレーム（描画ありで PC が遅いとき。STATUS に 36〜43ms の記録があり、50ms には近い）では、兵の時間だけ捨てられ、装填・砲弾・シールド・Arms とずれる。
  8. 試遊は可変の刻みなので、プレイヤーの PC の速さで AI の探索の処理能力（上の1・2）が変わる。**30FPS で採否を決めた切替が、60FPS 以上の試遊で同じ向きに効く保証は無い。**

## 4. 「戦闘の時計と Tick 番号を1系統で渡す入口」を作る場合

### 4.1 最小の一歩（挙動を変えない）

```cpp
struct FBattleStepTime
{
    double WorldSeconds;   // 今の Now と同じ値
    float  DeltaSeconds;   // 今の Dt と同じ値
    uint64 TickNumber;     // 第1段階では GFrameCounter をそのまま運ぶ
};
```

`ABattleGameMode::Tick` で World の時刻・`DeltaTime`・`GFrameCounter` を1回だけ採り、`UBattleLeaderRuntime::Step(const FBattleStepTime&)` へ渡す。`Step` の中は同じ値を `Now`／`Dt` に使い、**呼ぶ順番・切り詰め・期限の式は変えない**。

| 段階 | 置き換える箇所 | 触るファイル | 見積り |
|---|---|---|---|
| 入口だけ | `Runtime->Step(DeltaTime)` の呼び出し1、`Step` の宣言と定義、`Step` の中の `Now` の取得1 | 新しい小さなヘッダー、`BattleGameMode.cpp`、`BattleLeaderRuntime.h/.cpp` | 実装とレビュー半日〜1日、確認を含め1〜2日 |
| 主な依存へ広げる | 7つの cpp（GameMode・LeaderRuntime・StableMovement・SoldierMove・ThreatCover・Vehicle・ArmourAI）の World の秒 33・World の Dt 3・`GFrameCounter` 15＝**51か所**（表示・記録も混ざった数） | 上に加えて各ファイル | 追加で2〜4日 |
| 戦闘の全体 | Prototype 全体で `GetTimeSeconds` 222・`GetDeltaSeconds` 16・`GFrameCounter` 24（19行）・`Dt` 158 から、表示・試験・記録を除いたもの。入力・保存・旧経路も | 表の各系統 | 設計と試験が別に要る。1〜2日では終わらない |

- 守ること：Tick 番号を0始まりにすると被弾方向の推測とアニメの位相が変わるので、**第1段階は `GFrameCounter` をそのまま運ぶ**。`Step` の外から呼ばれる処理（入力・戦死の通知・保存）が前の Step の時刻を読まないようにする。
- 確かめ方：変更の前後で同じ条件の試合を回し、`Setup/Compare-MatchResults.py` が `IDENTICAL`（時間の値を除く metrics の一致。全状態の一致ではない）。30FPS・マップ2つ・1シードずつで足りる。
- 時期：`BattleLeaderRuntime.h/.cpp` は契約の修理（`FutureBattle-contract`）と B05（`FutureBattle-movestall`）が触っているので、その取り込みの後。
- これだけでは症状は1つも直らない。得られるのは、(a) フレーム番号と時刻の読み方が1か所になり、後の変更が小さくなること、(b) static の計数（全 World 共有）を Runtime の持ち物へ移す足場、(c) 「sim の Tick 番号」を記録に出せること（30FPS と 60FPS の結果を Tick ではなく秒で並べて比べられる）。

### 4.2 その次に固定刻み（例：戦闘30Hz、描画は別）へ進む場合に要ること

1. 時計と停止の決まり：World の時間／戦闘中だけ進む `Elapsed`・`ActiveSeconds`／AI が止まっていても進む装填／UI の実時間、を分けて定義する。
2. 固定の Step の蓄積器：1/30秒ごとに戦闘を回す。描画が遅いときの追いつきの上限と、余りの扱い。
3. フレームごとの予算をすべて sim の Tick の基準へ移す（個人の経路・遮蔽・被弾優先・個人の移動・取り直し・`ThinkCap`）。
4. 入力を sim の Tick へ取り込む（射撃・照準・移動・壁遮蔽・乗り越え・通信の入力に時刻か通し番号）。
5. 正規の位置・速さ・砲塔角を表示から分ける（描画で補間した位置を知覚・射撃・占領へ戻さない）。
6. **CharacterMovement との境界**：隊長は入力を積むだけで、動かすのは UE。1描画フレームに sim を2回回しても、隊長は2回動かない。隊長を手計算の移動へ移すか、CharacterMovement を sim から手で進めるかの選択が要る（直接操作の手触りに関わる）。
7. **Mass のカプセル**：sim の Step ごとに Component の位置を合わせる（視線と sweep は Component を読む）。全兵の移動の順番と結果の反映の順番を固定する。
8. NavMesh の同期の探索：共有経路・地形への投影・起動時の準備を含め、どの Tick で実行するかを固定する。
9. sweep・物理：1描画フレームの中の複数の sim Step で、ほかの Actor がどの時点の状態かを決める。
10. 射撃・知覚・経済の順番：今の「1人ずつ移動と射撃」を保つか、「全員の移動 → 知覚 → 全員の射撃」に改めるか（別のルールの選択。試験切替で評価）。
11. 保存：sim の時刻・Tick・要る残り時間と通し番号を保存し、作り直すものを明示して消す。
12. 確かめ：sim を30Hz に固定して描画だけ 30／60／それ以上・わざと長いフレーム・停止と再開・直接操作・保存の往復で比べる。

障害になるのは 6・7 と、表の × の2件（砲塔の角度、伏せの切り替え）。入口へ固定の Dt を渡すだけでは解けない。全体は数週間の仕事で、固定刻みにしても UE の物理・ナビを含めた完全な決定性までは得られない（同じビルドで 390秒から分かれる件は別に調べる必要がある）。

### 4.3 間を取る案（参考。採否はユーザー）

固定刻みまで行かずに、差の大きい所だけを秒の基準へ寄せる方法がある。どれも試合を変えるので既定0の試験切替で入れ、30FPS と 60FPS の両方で確かめる。

- フレームごとの予算を「秒あたり」の予算に替える（例：個人の経路 60件/秒を、Dt に応じた蓄積で配る）。30FPS と 60FPS で処理能力がそろう。変更は数か所で半日〜1日ずつ。
- 被弾方向の推測の `GFrameCounter` を、被弾の通し番号に替える。1行。
- 試遊も `-UseFixedTimeStep` 相当にする案は勧めない（描画が遅いと試合がゆっくりになり、速いと速く進む）。

## 5. やらない場合に残る問題

- 30FPS の並列の計測で決めた採否が、60FPS 以上の試遊でそのまま成り立つとは言えない（特に、探索の枠に頼る移動の詰まり・遮蔽の取り方）。
- 試遊する PC の速さで AI の探索の処理能力が変わる。
- 遅いフレームで、兵の時間だけが捨てられる。
- 入力だけで試合を再現する記録（リプレイ）は作れない。保存の往復の一致も「時間を除く metrics の一致」までにとどまる。
- 部隊を 500〜2000 に増やすとき、sim の頻度を描画より下げて CPU を減らす手（例：戦闘 10〜30Hz、描画は補間）が使えない。
- 一方、今のままでも次は成り立つ：同じ FPS・同じビルドどうしの比較（今の計測の決まり）、秒あたりの量が FPS に依らない経済・占領・装填の式。

## 6. 次に決めること（ユーザーの判断）

1. 4.1 の入口（1〜2日、挙動は不変）を、契約の修理と B05 の取り込みの後に入れるか。
2. 4.3 の「秒あたりの予算」を試験切替で入れ、30FPS と 60FPS の差が縮むかを1回測るか（先に、今の既定で同じシードの 30FPS と 60FPS の詰まり・遮蔽の数を並べるだけなら、試合2〜4本で済む）。
3. 固定刻み（4.2）は、上の2つの結果を見てから決める。
