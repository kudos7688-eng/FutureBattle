# 計測中のマシン状態を記録する

Windows PowerShell 5.1 用。既存の計測ランナーは変更せず、起動済みのプロセスに別の PowerShell から監視を付ける。UE・ビルド・試合を起動する機能はない。

## 既存計測に付ける

通常の計測を起動した後、別の PowerShell をこのワークツリーで開く。PID は計測ランナーのログや起動結果と照合して選ぶ。別セッションの Editor と取り違えないこと。

```powershell
Set-Location C:/Users/nanek/Documents/FPS/FutureBattle-codex
Get-Process -Name UnrealEditor* | Select-Object Id, ProcessName, StartTime, MainWindowTitle

# 実際の計測 PID に置き換える
$measurementPid = 12345
$watchScript = Join-Path $PWD 'Setup/Watch-MeasurementMachine.ps1'
$output = Join-Path $PWD ('Setup/Benchmarks/MachineWatch/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.jsonl')
$watchArgs = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -TargetId {1} -OutputPath "{2}"' -f $watchScript, $measurementPid, $output
$watcher = Start-Process powershell.exe -WindowStyle Hidden -ArgumentList $watchArgs -PassThru
$watcher.Id
$output
```

出力フォルダーは自動作成する。既存ファイルは上書きせずエラーにする。監視対象が終了すると監視も自動終了する。途中で止める場合は監視プロセスだけを終了する。各行を即時 flush するため、それまでの完了行は残る。

PID を限定しない場合は、別ウィンドウで次のコマンドを実行する。このコマンド自体が独立した PowerShell プロセスになる。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Setup/Watch-MeasurementMachine.ps1 -TargetName 'UnrealEditor*' -OutputPath Setup/Benchmarks/MachineWatch/run-name.jsonl
```

名前指定は**開始時に一致した全プロセス**を固定して追跡し、全対象が終了すると止まる。後から起動する次の試合は捕捉しない。連続試合のランナーでは各試合の PID ごとに監視を起動する。対象がいない場合は即時エラー。`-TargetId` と `-TargetName` は同時指定できない。PowerShell 内から呼ぶ場合は `-TargetId @(12345, 23456)` で複数指定できる。

開始前の機種情報は既存の `Get-MeasurementMachine` と計測 manifest の `machine` に引き続き記録する（JSON 深さ5以上）。この監視ログは動的な状態の補助証拠であり、manifest や描画 GPU と UE ログの照合の代わりではない。監視を後から付けるため、開始前と最初の約1秒、監視準備中の状態は記録できない。既存結果へ機種情報を後付けしない。

## 要約する

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Setup/Summarize-MeasurementMachine.ps1 -InputPath Setup/Benchmarks/MachineWatch/run-name.jsonl -OutputPrefix Setup/Benchmarks/MachineWatch/run-name-summary -IntervalSeconds 30
```

`<OutputPrefix>.json` と短い `<OutputPrefix>.txt` を作り、文章をコンソールにも出す。既存の要約ファイルは上書きしない。JSON には、各区間の平均・最大・最小・有効数・欠測数、他プロセス上位10、クロック低下サンプルの時刻、電源プラン・前面PID・最小化サンプル数を保存する。

区間は監視開始からの経過秒を使う `[0,30), [30,60), ...`。サンプルの終点時刻で振り分ける。平均は有効サンプルの単純平均で、欠測を0扱いしない。長い取得遅延がある場合の時間加重平均ではない。壊れた行（中断時の末尾など）は飛ばし、JSON の `invalid_lines` に行番号と理由を残す。有効行が0なら失敗する。

クロック低下は `% Processor Performance` が基準の70%未満のサンプル。基準は既定で同じログの有効な正値の90パーセンタイル（5件以上必要）。正常時の別ログなどから基準が分かる場合、`-BaselinePerformancePercent 120` のように指定できる。`-ClockDropRatio 0.7` で閾値を変更できる。全区間が遅いログではログ内基準による検出はできないため、正常時と異常時の両方を保存し、同じ基準を指定して比較する。

## 値の定義と制限

| フィールド | 意味 |
|---|---|
| `timestamp_utc`, `elapsed_seconds`, `interval_seconds` | UTC のサンプル時刻、単調時計による経過・前回からの秒数。UEログとの照合ではタイムゾーンに注意 |
| `metrics.cpu_percent` | `Processor(_Total) / % Processor Time`、全体を100%とするCPU使用率 |
| `top_processes` | CPU時間の差分から求めた上位10、名前・PID・開始時刻・CPU%・WorkingSet。Idleは除外。全論理CPU合計を100%とする |
| `targets` | 対象ごとのCPU秒増分、CPU%、PrivateBytes、WorkingSet、スレッド数、主ウィンドウの最小化状態 |
| `available_bytes`, `committed_bytes`, `commit_limit_bytes` | 物理空き、システムのコミット使用量・上限。単位bytes |
| `hard_faults_per_second` | `Memory / Page Reads/sec`：ハードフォールト解決のディスク読み込み回数/秒。1読込で複数ページを処理でき、個別フォールト件数や対象プロセス固有の件数ではない |
| `pages_input_per_second` | `Memory / Pages Input/sec`：上記読込で入ったページ数/秒。ソフトフォールトを含む `Page Faults/sec` は使わない |
| `disk_*_wait_seconds`, `disk_queue_length` | 全物理ディスクの平均Read/Write応答秒と現在キュー長。CPUのI/O待機率ではない |
| `processor_performance_percent` | `Processor Information(_Total) / % Processor Performance`：公称性能に対する割合。ターボ時は100を超える |
| `processor_frequency_mhz` | `Processor Information(_Total) / Processor Frequency`。機種によって公称値のままのことがある。性能割合と併せて見る |
| `power_plan`, `foreground_pid` | 現在の電源プランGUID/名前、監視セッションの前面ウィンドウPID |
| `gpu` | nvidia-smiが列挙する全GPU。UUID/名前、graphics/SM/memory MHz、P-state、W、℃、GPU使用率、VRAM使用/全量MiB |
| `collection_seconds` | 1回の取得にかかった実時間。監視のCPU時間そのものではない |

全CPUカウンターは、英語名を扱えるPDH APIの永続クエリーで一括取得する。日本語Windowsの表示名には依存しない。プロセス一覧は毎周期1回取得し、スレッドとウィンドウの調査は対象だけに限定する。1秒周期を目標に処理時間を差し引いて待機し、遅延した場合は追いつくための連続取得をせず実際の間隔を記録する。nvidia-smiは毎回1プロセス、600msでタイムアウトし、その子プロセスだけを終了する。監視自身とnvidia-smiにも負荷があるので、比較する両条件で同じ監視を付ける。

取得できない項目は `null` と `reasons` に理由を記録し、次のサンプルへ進む。GPUなし・未対応のGPU項目・権限不足・カウンターなしも欠測扱い。対象の主ウィンドウがない場合、最小化していないという意味のfalseにはしない。対象が複数ウィンドウを持つ場合、最小化判定は.NETが返す主ウィンドウだけ。前面ウィンドウは別のデスクトップ/セッションからは取得できない場合がある。

プロセスの開始時刻とPIDを照合して再利用を区別する。開始・終了がスナップショット間に収まる短命プロセスや、アクセスできないプロセスのCPU消費は捕捉できない。上位10に出なかった期間の他プロセスのCPUは不明であり、要約の `observed_cpu_seconds` は上位10に出たときだけの積算。全実行期間の総CPU時間ではない。

クロック低下だけでは電力・温度制限とは断定できない。アイドル、負荷の変化、最小化・前面の切替、電源プラン、メモリ圧迫も合わせて確認する。CPUの温度や制限フラグはこの仕組みでは直接取得していない。GPU値もUEだけの値ではない。

## UEなしの単体確認

次のスクリプトはスリープする隠れたPowerShellだけを対象にし、約5秒採取する。実行ポリシーは呼び出したプロセスだけに指定し、マシン設定を変えない。

```powershell
$target = Start-Process powershell.exe -WindowStyle Hidden -ArgumentList '-NoProfile -Command "Start-Sleep -Seconds 15"' -PassThru
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Setup/Watch-MeasurementMachine.ps1 -TargetId $target.Id -OutputPath Setup/Benchmarks/MachineWatch/selftest.jsonl -DurationSeconds 5
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Setup/Summarize-MeasurementMachine.ps1 -InputPath Setup/Benchmarks/MachineWatch/selftest.jsonl -OutputPrefix Setup/Benchmarks/MachineWatch/selftest-summary -IntervalSeconds 2
```

`-DurationSeconds` の既定0は時間制限なし（対象終了まで）。対象終了による自動停止を確認するときは、この引数を外す。再実行時は未使用の出力名を指定する。手元で開いたメモ帳なども `-TargetName notepad -DurationSeconds 5` で対象にできる。

### 実施済み確認（2026-11-20）

- Windows PowerShell 5.1.26100.9818で構文確認と、UE以外のスリープするPowerShellを対象に5秒採取。5行をJSONとして読み込み、全体CPU・メモリ・ディスク・CPUクロック指標・電源プラン・NVIDIA GPUの値を確認した。
- 時間制限なしで7秒スリープの対象を追跡し、6サンプル採取後、監視呼び出しから約8秒で自動終了。名前のワイルドカード指定でも2秒採取した。
- 合成データで平均20・最大30・欠測1、他プロセスのCPU積算順、対象プロセス除外、全欠測項目、低クロック2サンプル、壊れた末尾行1件を検証した。
- この実行セッションでは前面ウィンドウを取得できず、隠れた対象にも主ウィンドウがないため、いずれも理由付きnullを確認した。実際のGUI最小化切替とUEとの時刻照合は未実施。

ローカルの検証結果は `Setup/Benchmarks/MachineWatchSelfTest/`。UE・ビルド・試合・gitは実行していない。
