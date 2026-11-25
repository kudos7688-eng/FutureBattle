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

- **改善かどうかは、元の結果からプラスになったかで決める**（2026-11-22 ユーザー決定）。偏りを直して勝敗をそろえたうえでの片側試験では判断しない。同じマップ・seed・条件で、変更なし（切替OFF）の元の結果と変更あり（切替ON）を回し、得点・撃破・損失・ねらった行動の差を項目ごとに出す。プラスになったときだけ改善と書く。下の片側試験の決まりは過去の記録を読むためのもので、新しい判断には使わない。
- **得点だけで決めない。与えた損害と戦死もあわせて見る**（2026-11-24 ユーザー決定）。エリア戦の得点は「残した資源＋生存者＋終了時の拠点」で、相手が分隊を買い直さずに済むと相手の得点が上がる。撃ち合いが強くなったのに得点差が下がることがある（11-24 の段階2：小銃分隊の与えた損害 +17%、得点差 −71。内訳は残した資源 −59）。
  - 報告には、得点差の変化に加えて、**軍ごとの与えた損害・撃破・戦死**（元 → 変更後）を必ず並べる。
  - 得点と撃ち合いの向きが食い違ったら、得点を **残した資源・生存者・拠点** に分け、雇用の支出と倒した隊長の数を見てから結論を書く。食い違いを書かずに「改善」「悪化」と書かない。
  - 集計は `python -I Setup/Analyze-MatchDelta.py <元> <変更後> [<元> <変更後> ...]`（結果フォルダの名前を組で渡す。ビルドもロックも要らない）。項目ごとの差の後に、次の3つが出る（2026-11-25 から）。
    - `CONDITIONS`：組の2試合のビルド・DLL・マップ・シード・FPS・描画・試験した側・保存の確認・`passed` を並べ、違い（`!! MISMATCH`）と不明（`?? unknown`）に印を付ける。片方だけにある起動引数も出す。**出るのは起動引数で、実際に効いた設定の一式ではない**（省略した引数の既定値やマップごとの値は manifest に無い）。`not frozen` は凍結ビルドでなく作業ツリーから起動した試合。
    - `ACTIVATIONS`：ねらった行動の記録の行（`BATTLE_AREA_HIRE`、`BATTLE_COMPOSITION_EXECUTE success=1`、`BATTLE_HIRE_RESERVE op=buy`、`BATTLE_ARMS_ASSAULT`／`CHARGE`／`GRENADE`／`MELEE`、`BATTLE_FRONT_FOCUS2_GROUP`、`BATTLE_FRONT_ORDER_STEP`、`BATTLE_MISSION_RESUME`）を `run.log` からチーム別に数える。0 のときは **`0`（起き得たが起きなかった）／`no subject`（対象なし：切替が無い・予約が無いなど）／`0 (subject unknown)`（ログからは分からない）／`no record`（`run.log` が無い）** を区別する。指標を足すときは `--count 名前=正規表現[@対象があることを示す行の正規表現]`。ログを読まないときは `--no-log`。
    - `ALL MATCHES`：渡した組を、結果の向きに関係なく全部1行ずつ出す（読めない組も `NOT READ` で残る）。**発動0回の組も集計から外さない**。
- **採用の確認に使うシードは 239 と 277**（2026-11-25 に決めた。これまでの全結果の manifest に無い未使用のシード。使用済みは 3・7・11・19・43・61・89・101・131・157・173・191・211）。使い方：まず既存の2シード（7・43 など）で基準との差と発動回数を見て、**採用の候補になったものだけ**を 239・277 で確かめる。選別や調整には使わない（調整に使ったシードは確認にならない）。全マップ・左右の総当たりは義務にしない。測っていないマップ・側は表に書く。
- **負荷テスト（性能・1フレームの時間）以外の試験は、すべて30FPS（`-Fps 30`）・並列で行う**（2026-11-22 ユーザー決定。選別・回帰・挙動の確認・片側試験・既定にするかの判断を含む）。60FPSは負荷テストだけ。30FPSは60FPSより約1.7倍速いが結果は変わるので、FPSが違う結果どうしは比べない（基準の試合も30FPSで回し直す）。
- **試合の本数は判断に必要な最小限にする**（11-22 ユーザー指示）。12は並列の上限で、目安ではない。シード間で向きがそろい差が揺れより大きければ2シードで止め、そろわないときだけ足す。ほかのセッションが確かめた効果は測り直さない。
- **段階の順と申請**（2026-11-25 ユーザー決定。`Docs/design-match-budget.md`）：① 固定場面の試験（1〜2分。`Docs/test-map.md`）で成立を確かめる → ② **1試合**で発動回数を見る（`Analyze-MatchDelta.py` の `ACTIVATIONS`）→ ③ 既存の2シードで基準との差 → ④ 採用の候補だけ 239・277。②で発動0回なら③へ進まず、前提の切替・マップを直すか、「通常の試合では起きない」と書いて止める。使える固定場面が無いときは、試合を増やす前に固定場面を作る。**③以上を回す前に掲示板へ申請を1行書く**：`python -I Setup/Board.py plan --who <系統> --question "<問い>" --why "<既存の結果で答えられない理由>" --runs "<条件の数×シード、マップ、合計の本数>" --stop "<やめる条件>" --keys <切替名,マップ> [--stage 2|3|4]`。書くと、同じ切替・同じ語の過去の結果と他の系統の申請が出るので、先に読む。16本以上の総当たりは、条件の表を STATUS か設計文書に書いてから。
- **共用の基準（台帳）**（同）：main の既定の基準は、マップ I・J・K × シード 7・43 と H × 7 の7本を、既定が変わるたびに1回だけ回して台帳（`FPS\.lock\baselines.json`）に置く（持ち主は当面、系統「試合数の統制」）。**自分で基準を回す前に `python -I Setup/Baseline.py show`**。比べるときは `python -I Setup/Analyze-MatchDelta.py --baseline <試験の結果>...`（基準を台帳から map・seed で引き、試験の DLL が基準と同じか・同等の登録があるかを `BASELINE` の節に出す）。
  - 系統の作業ツリーの DLL は main と違う。**差分が「既定0の試験切替を足しただけ」のときに限り**、切替 OFF の1本（台帳にある map・seed、基準と同じ引数、凍結ビルド）を回して `python -I Setup/Baseline.py equiv --who <系統> --switch-only <その結果>` で同等を登録し、残りの基準は台帳を**参照**する（写して自分の実測と書かない）。1マップ1シードの一致は残りの同等の証明ではないので、既定の経路・Config・マップに触れた差分は、基準を自分の DLL で全部回す。記録の引数を足した試合は基準と引数が違うので、同等の確認には使えない。
  - 台帳に無いマップ・シード・条件（見通し50m など）の基準は今までどおり自分で回し、結果の名前を `Board.py note` に残す。
- **比較に使う試合は凍結ビルドから起動する**（同）。作業ツリーから直接起動した試合は、再利用の鍵が付かず、Config の一時変更などの条件が manifest に残らない（11-22〜25 は45%が直接起動だった）。動作を見るだけの1本は直接起動でよい。**凍結ビルドの片付け**：`Setup/Benchmarks/BuildSnapshots/<名前>/Project`（約200MB の写し）は、3日より古く、台帳の今の基準でもないものを消してよい（`manifest.json` と `source.zip` は残す。結果からビルドをたどれる）。消すのは自分の系統が作ったものだけ。
- **記録だけを足したビルドは、回し直す前に1本で一致を見る**（Claude の判断で追加、ユーザー未確認）：新しい記録の行を足しただけなら、前と同じ条件の1本を回して `Compare-MatchResults.py` で `IDENTICAL` を確かめ、残りのシードは前の結果を使う（新しい記録はその1本で読む）。
- **既定を変えるコミットと main への取り込みは1日2回、6時と18時**（同。2026-11-25 の18時から）。その回の最後に、持ち主が基準7本を取り直して台帳を更新する。例外は、ユーザーの試遊に要るものとユーザーが最優先と指示したもの（`-Urgent`）。例外で既定の試合が変わったときは、`Board.py note` に「基準が古くなった」と書く（次の定時まで、台帳の基準と今の main は一致しない）。文書・`Setup/` の解析スクリプトだけのコミットは定時を待たない。
- 並列は12まで（描画なしの試合だけ。60FPSの固定刻みと同じく、30FPSの固定刻みでも結果は並列の数に左右されない）。保存往復は、保存・読み込みに触れる変更のときに付ける。
- 11-22 より前の記録（標準60FPS・12並列、30FPSは一次選別だけ）の結果は60FPSのものなので、新しい30FPSの結果と混ぜない。
- **`Run-StrategyScreen`**（11-18 2時から）：既定で30FPSで選別し（`-Fps 30`）、通過した条件だけを60FPSで確かめる（`-ConfirmFps 60`）作りだが、11-22 の決定により **`-NoConfirm` を付けて30FPSだけで回す**。
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
- 既定の採否を自分で決める（決めるのはユーザー。材料は30FPS・並列の結果でよい。11-22 ユーザー決定）。
- 負荷テスト以外を60FPSや1本ずつで回す（時間の無駄。11-22 ユーザー決定）。
- 未実施の確認を「合格」と書く。
