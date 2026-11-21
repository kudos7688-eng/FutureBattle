# 計測・試験の手順（1ページ版）

2026-11-18 作成。計測を始める前にこれだけ読めばよいようにまとめた。詳しい経緯は `PERFORMANCE.md` と `Docs/journal/` にある。手順が変わったら、この文書を書き換える（追記で長くしない）。

## 0. 始める前に（毎回）

1. **ロック**（2026-11-22 から、ユーザー決定）：ビルド・UEの起動（試験・計測・試合）・Source/Config/DLL/マップの変更の前に、ファイルの順番待ちの列に並ぶ。`Setup\Lock-Queue.ps1 -Join -Who <セッション名> -Purpose <用途> -Minutes <見込み>` → `Setup\Lock-Queue.ps1 -Wait -Who <セッション名>`（Claude は `run_in_background` で呼ぶ。自分の番で終わる）→ 作業 → `Setup\Lock-Queue.ps1 -Release -Who <セッション名>`（次の人が自動で持ち主になる）。今の持ち主と列は `Setup\Lock-Queue.ps1 -Status`（列のファイルは作業ツリーの外の `FPS\.lock\queue.json` で、どの作業ツリーからも同じ）。チャットでの受け渡しは要らない（急ぎの相談だけ `SendMessage`）。持ち主が見込み＋30分を過ぎ、UE・ビルドのプロセスが無ければ、次の人に渡る。ユーザーのプレイの確認は `-Urgent` で列の先頭に入る。設計は `Docs/design-lock-queue.md`。ワークツリーが別でもCPUは共有なので、実行を重ねない。
2. `git status --short -uno` と、実行中の `UnrealEditor` を確認する。ほかのセッションのプロセスは止めない。
3. **凍結ビルドは写しから起動する**（11-18 から）。`New-BattleBuildSnapshot.ps1` は `<凍結ビルド>/Project` に DLL・Config・Content などの写しを作り（約200MB・約5秒）、試合はその写しから起動する。計測中にビルドや Config・マップを編集しても、計測は止まらず、不合格にもならない。ただし、写しを持たない古い凍結ビルド（または `-NoProjectCopy` で作ったもの）は従来どおりで、計測中に Config を変えると起動拒否か不合格になる。
4. 所要時間を見積もってユーザーに伝える（目安は下の表）。
5. 起動引数の名前と既定値は `SWITCHES.md` で引く。ソースを変えた後は、`& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -I Setup/Export-Switches.py` で作り直す。Claude は `/measure` スキル（`FPS/.claude/skills/measure/`）で、この手順を同じ順に進められる。

## 1. 目的別の入口

| 目的 | 手順 | 合格・読む値 |
|---|---|---|
| C++を変えた後の基本回帰 | `powershell -NoProfile -ExecutionPolicy Bypass -File Setup\Build-Editor.ps1` → `Setup\Test-Battle.ps1` | ログに `BATTLE_SMOKE_RESULT failures=0`。プロセスの終了だけで合格にしない |
| 変えた箇所ごとの専用試験 | 下の「専用試験」の表 | 各スクリプトの `BATTLE_*_RESULT failures=0` |
| AI・経済の勝ち負けの比較（並列・描画なし） | `Setup\New-BattleBuildSnapshot.ps1 -Name <未使用名>` で固定ビルドを作る → `Setup\Run-EconomyMatrix.ps1`（総当たり）または `Setup\Run-StrategyScreen.ps1`（片側試験を2段で絞る） → `Setup\Analyze-EconomyMatrix.ps1` | 試合ごとの `result.json` の `passed`、`metrics.json` の得点・勝敗 |
| 性能（1フレームの時間） | `Setup\Test-WideSight.ps1 -Map E -Area -ObserveTactics -Experiment -Rendered -Fps 30 -OutputName <未使用名>`。内訳が要るときだけ `-Trace 'cpu,frame'` を足す | `metrics.json` の1フレームの平均/p95/最大、認識、描画スレッド |
| 結果を変えない最適化の確認 | 変更前後で同じ条件の試合を回し、`python.exe -I Setup/Compare-MatchResults.py <変更前> <変更後> ...` で比べる（`metrics.json` のうち時間の値を除く項目だけを比べる。11-18 から。manifest・`result.json` の合否・保存の中身は比べないので、完走と `passed` は別に確かめる） | すべて `IDENTICAL`（終了コード0）。意味は「非時間の metrics が一致」 |
| マップの形を数字で見る（試合はしない） | `Setup\Test-MapStats.ps1 -Map E -OutputName <未使用名>` | `Setup/Benchmarks/MapStats/<名前>/stats.json`：区域ごとの視線の長さ・空き幅・遮蔽の密度、本拠地からの経路の長さ、戦車の到達、見張り所と拠点から見える割合 |

### 勝ち負けの比較の決まり

- **標準は60FPS・保存往復あり・12並列**。12戦で約10.5分（マップA/D、600秒）。
- `-Fps 30` は約1.7倍速いが結果が変わる。**一次選別だけ**に使い、採否は60FPSで確かめる。FPSが違う結果どうしは比べない。
- **`Run-StrategyScreen` はこの決まりを自動で行う**（11-18 2時から）：既定で30FPSで選別し（`-Fps 30`）、通過した条件だけを60FPSで確かめる（`-ConfirmFps 60`、出力タグ `<Tag>f60`）。両方に通った条件だけを、採用を検討する価値があるとみなす。確認を省く `-NoConfirm` は、選別だけで済むときに限る。
  - seedは9本（7, 19, 43, 61, 89, 101, 131, 157, 173）を並列数の分ずつ回す。平均±2se（se は seed 間の標準偏差と `-SeedSd`〔既定60〕の大きいほう ÷ √n）で、明らかに悪ければ打ち切り、3seed以上で明らかに良ければ早めに通過させる。3seedの時点で基準以下なら打ち切る従来の規則も残っている。
  - `-DryRun` を付けると、試合をせずに今ある結果だけで判定する（集計のやり直しに使う）。詳しくは STATUS の 11-18 2時00分の項。
- `-NoRoundTripSave` は速くならず結果も変わるので使わない。
- 片側試験の評価は「試験側の得点差 −（同じseedの既定どうしでのその側の得点差）」。seedの組ごとの側の偏りは±15〜35、12戦の組のぶれは±50ほどある。
- 片側試験の偏り（`-AreaTrialTeam` で相手側の S4/S6 が切れていた）は 11-18 0時50分に修正した。それより前の勝敗を問う片側試験の結果とは比べない。
- 保存往復の検証で `Invalid cover peek distance` によって試合が失敗した件は、`e1b7885` で直した（距離0の正当な遮蔽を旧検査が拒否していた）。ほかの理由で失敗することはあるので、完走数を必ず確かめる。同じ文言でも、新しい失敗を既知として除外しない。
- 途中で止まった総当たりは、同じコマンドをもう一度実行すれば、出力が既にある試合を飛ばして再開する。
- **同じ試合の再利用（11-18 から既定）**：`Run-EconomyMatrix`/`Run-StrategyScreen` は、同じ凍結ビルド・同じ条件で合格済みの試合があれば、試合をせずに結果を写す（`reusedFrom` が付き、`timingComparable=false`）。勝ち負けの比較には使えるが、時間の比較には使わない。毎回試合をするときは `-NoReuse` を付ける。詳しくは STATUS の 11-18 1時05分の項。

### 性能の計測の決まり

- **描画ありは単独で実行する**（`Test-WideSight` は `-Rendered` と並列の組み合わせを拒否する）。並列の描画なしの試合をFPSの計測として扱わない。
- トレースあり/なしを分けて記録する（トレースありは約0.9ms遅い）。トレースは数GBになるので、コミットしない。
- 測定機の情報は `Test-WideSight` が `manifest.json` の `machine` に自動で保存する。ほかのスクリプトで計測するときは `Setup\Get-MeasurementMachine.ps1` の `Get-MeasurementMachine` を呼び、JSONの深さ5以上で保存する。後から付け足さない。
- 現在の基準値は `PERFORMANCE.md` の冒頭にある。

## 2. 専用試験（変えた箇所ごと）

| 変えた箇所 | 試験（`Setup\` 配下） |
|---|---|
| エリア・補充・資源・本拠地・勝敗 | `Test-Area.ps1`（`BATTLE_AREA_RESULT`） |
| 混成の保存・読込 | `Test-MixedSave.ps1`（`BATTLE_MIXED_SAVE_RESULT`） |
| 混成の購入・予算 | `Test-MixedFast.ps1` |
| 移動・衝突・到着予約 | `Test-MovementStability.ps1`、`Test-ContactMovement.ps1` |
| 撤退・援護・迂回 | `Test-SquadRetreat.ps1`、`Test-TeamSupport.ps1`、`Test-SquadFlank.ps1` |
| 直接操作・敵の表示・回避 | `Test-TPSControl.ps1`、`Test-TPSVisibility.ps1`、`Test-TPSAvoidance.ps1` |
| 隊長/Mass・VAT | `Test-LeaderCommand.ps1`、`Test-LeaderPhase3.ps1`、`Test-VATDisplay.ps1` |

表示を変えたときは、実画面での確認も必要。画像ができたことと、目で見て確認したことを区別して書く。

## 3. 出力の場所

| 何 | 場所 |
|---|---|
| 試合ごとの結果 | `Setup/Benchmarks/SightTrial-20261111/<OutputName>/`（`result.json`、`metrics.json`、`manifest.json`） |
| 固定ビルド | `Setup/Benchmarks/BuildSnapshots/<Name>/manifest.json` |
| 総当たりの標準出力 | `Setup/<接頭辞>-<条件>.txt`（例 `Econ-econ-...txt`）。**2日より古いものは `Setup\Move-OldRuns.ps1` で `Setup/Runs/<YYYY-MM>/` へ移す**（txt・png・json・log・out が対象。git の対象外。Setup 内に読み戻すスクリプトはない） |
| ビルドのログ | `Setup/Build-Editor.log`（毎回上書き） |

`OutputName` やタグには、まだ使っていない名前を付ける。既存の証拠を上書きしない。

11-04〜11-11 の個別の調査で使った試験・計測スクリプト（T2/T5 系、認識監査、GT の内訳など62本）は `Setup/Legacy/` へ移した。使うときは `Setup/Legacy/README.md` を読む。

リポジトリ直下の `.ignore` により、ripgrep（Claude の Grep）は `Setup/Benchmarks/` や `Setup/Runs/` などの計測結果を検索しない。結果の中を検索するときは、パスを指定して `rg --no-ignore` か `grep -r` を使う。

## 4. してはいけないこと

- ほかのセッションの計測中に、ビルド・試験・Config変更をする。
- ビルドに失敗した後、古いDLLのまま試験を続ける。
- 30FPSの選別だけで既定の採否を決める（決めるのはユーザー。材料として60FPSの確認の結果を添える。`-NoConfirm` で回した結果は選別の結果として書く）。
- 未実施の確認を「合格」と書く。
