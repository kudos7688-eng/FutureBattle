# 頻発するフレームスパイクの追加調査（2026-11-11）

今回最も絞れた候補は、**GTの実行コアとCPU上での実行効率の変動**。特にtrace-r2ではp95超124フレーム中110フレームでGTのEコア実行が1ms以上あり、アニメ・移動の呼出回数が同じまま各処理が同時に伸長した。一方、trace-r1の大半はPコア主体であり、Eコアだけを全走行の根因とはしない。外部負荷、共有コア・キャッシュ・周波数、内部仕事量の寄与が残る。

**GTのOS実行待ち、アニメworkerの開始待ちが頻発スパイクの主因であるという証拠は弱くなった。** workerが実行中に受ける資源競合まで否定したものではない。最適化・ゲーム挙動・通常既定・資産の変更は行っていない。

## 条件と測定

ユーザー指定で表示LOD作業の完了を待ち、その最終DLL `72BD51BE335008EE5E78E277C4807C96EAE664EF9170FFC35E3060536CAF22A6` を固定した。先行した旧DLLのcontrol-r1と起動前失敗は `../SpikeRoot-20261111` に保存し、今回には混合しない。

1000体・Leader・RTS・高HP・Open・seed1・1080p・B感覚、既存3最適化ON、AnimTasks OFF、DisplayLOD OFF。warmup10秒＋測定60秒。元の影/Lumen/反射OFF設定を維持。control-r1→trace-r1→trace-r2→control-r2の順。通常2反復、UE CPU/frame/bookmark/log/task＋ETW同時計測2反復。4走行とも同じ管理者権限、優先度・affinity・worker数は変更なし。各開始前にGet-MeasurementMachineを取得し各manifest.machineへ保存した。

|走行|frames|GT平均 ms|GT p95 ms|GT最大 ms|RT平均 ms|GPU平均 ms|
|---|---:|---:|---:|---:|---:|---:|
|control-r1|2757|21.761|27.915|58.925|3.258|2.572|
|control-r2|2920|20.546|22.923|39.469|2.951|2.634|
|trace-r1|2549|23.539|25.849|40.136|3.381|2.613|
|trace-r2|2492|24.079|27.448|58.982|3.455|2.640|

通常平均の2本平均21.153msに対しtrace平均23.809ms（+2.656ms、約12.6%）。これは計測負荷と走行間変動を合わせた差で、純粋なtrace費用の推定値ではない。p95も一方向に変化せず、traceを通常性能へ混合・補正しない。全走行GT16.67ms超100%、死亡0。全ms列の統計は [summary.json](summary.json)、wallを含む表は [measurement-table.md](measurement-table.md)。

## スパイク分類と頻度

各走行自身のp95を厳密に超えるフレームを対象とする。p95超は定義上ほぼ5%なので、回数は独立した異常発生確率とは解釈しない。

|走行|p95超/60秒|孤立フレーム|GCと重なる|GT Eコア実行1ms以上（GC除外）|残るP主体|
|---|---:|---:|---:|---:|---:|
|trace-r1|127（2.12/秒）|75|1|6|120|
|trace-r2|124（2.07/秒）|37|1|110|13|

control-r1/r2は137/146フレーム、孤立60/79。trace-r2は連続した山が多い。Eコア実行が1ms以上の全フレーム数は42/403であり、Eコアに乗るだけで必ずp95超になるわけではない。分類は観測上の重なりであって、各原因に帰属した件数ではない。該当CSV行・world時刻・OS内訳は各走行の `spike-classification.csv`、連続区間は `e-core-episodes.csv`。

秒位相10分割と50ms位相5分割を比較した。秒位相の最大率はr1の0.3–0.4秒（7.84%）、r2の0.8–0.9秒（7.72%）で一致しない。50ms位相には偏りがあるが両走行の山を一意に説明しない（[period-phase.csv](period-phase.csv)）。**不規則な単発・連続群が中心で、毎秒同じ時刻の処理が根因とは確認できない。** 正確なバッチdeadline時刻との同期は未取得。

trace-r1では127件中83件が初発砲前。shots差分とGTの相関はr1=-0.262、r2=0.015、死亡は全走行0。発砲数増加を共通根因にはできない。認識の上位−非上位差はr1=-0.015/r2=+0.490ms、followは+1.271/+1.294ms、RVOは+0.480/+0.253ms。戦況や移動内部の仕事量の影響までは除外していない。値と定義は [evidence.json](evidence.json)。

## OS実行待ちと実行中時間の切り分け

CSVのGTはviewport境界、OS集計は対応するengine tick境界。相関r1=0.97746/r2=0.93597、両方末尾2tick除外で整列。個別フレーム境界の差は残るため、GT差をそのままOS内訳へ厳密に割り振らない。

|engine tick内のp95超−非超の平均差 ms|trace-r1|trace-r2|
|---|---:|---:|
|tick全体|+3.444|+8.662|
|GT running（CPU割当中）|+3.339|+8.432|
|GT Ready（実行可能だが未実行）|+0.006|+0.142|
|GT blocked（待機→Ready）|+0.100|+0.087|
|GT Eコア上実行|+0.044|+11.789|

E/P時間はrunningの内訳であり別加算しない。GT runningは割込時間を含む上限で、DPC/ISRとの重なりの上位−非上位差は0.055/0.025ms（全frame最大0.790/1.008ms）。頻発する数msの伸びを主に割込で説明する結果ではない。

GT Eコア時間とGTの相関はr1=0.059/r2=0.807。r2のp95超ではEコア平均12.285ms、非超では0.496ms。単なるReady待ちではなく、実際にEコアで実行しながら各scopeが伸びている。`scheduler-frames.csv`、`scheduler-summary.json`、`competition-summary.json` が根拠。

## 同じ呼出回数でもアニメ・移動が伸びる証拠

各traceで8スパイクと近傍比較frameを選び、前後1frameを含む48frameずつを全thread出力。GTの親子scopeを排他的に集計した。全96frameの集計誤差は0（小数精度内）。各走行 `paired-evidence.csv` / `scope-os-evidence.csv` / `scope-leaves.csv` に全値がある。

**r2 frames.csv行1542、world47.655918秒と行1545を比較：**

|GT内訳|近傍通常 ms|スパイク ms|スパイク中Eコア ms|
|---|---:|---:|---:|
|GameMode（follow/RVO等を含む）|7.823|18.719|16.657|
|アニメTick|3.102|8.862|8.749|
|アニメ完了|3.003|7.779|7.058|
|CharacterMovement|1.071|2.859|2.537|

呼出回数は両方アニメTick1000、完了900、CharacterMovement100。MoveComponentも1346→1314で増えていない。Ready待ちは各scope0.009–0.379msと小さい。呼出数増加よりも、実行コアと実行効率の変化を強く疑う根拠になる。ただし内部ループ回数・cache miss・周波数は未測定。

**r1行1226、world39.708123秒と行1230：** Eコア時間0のままGameMode9.071→12.700ms、アニメ完了3.173→5.144ms。各呼出数は同じ、MoveComponent614→612、両scopeのReady/blockedは0。Eコア以外でもCPU割当中の伸びが残る。

一方、r1行1212対1209ではMoveComponent609→1354と仕事量の増加もある。すべてを同じ機序に分類せず、移動更新が多いframeを別に扱う必要がある。前後frameは `scope-frames.csv` に保存した。

## worker競合の切り分け

UEのTaskTraceでは旧TaskGraphの名前がGraphTaskになるため、名前だけでアニメを数えず、選択区間のCPU scope・OS thread・開始終了時刻へ対応させた。アニメscopeとの対応はr1=86,399/86,400、r2=86,400/86,400。1件未対応は欠落として残す。各アンカーframeはworker評価900件、GT完了900件。`selected-animation-tasks.csv`、`selected-task-evidence.csv` が根拠。

- worker評価のScheduled→Started平均：選択frame全体で0.000829–0.002179ms。frame内p95最大0.0109ms、単一task最大0.0928ms。数msのworker開始待ちは観測されない。
- r2行1542のGT完了task queue平均11.257ms、近傍3.944ms。一方worker評価queue平均0.002179/0.001091ms。GTが遅い状況で完了taskがGT待ちになる説明と整合し、worker開始待ちとは区別する。
- 全worker合計Ready CPU-msは、上位で4.689/5.283、非上位で5.291/5.710と増えていない。workerのblocked合計はフレームが長いほど増えるので、混雑の証拠に使わない。
- 実行中workerの資源競合は未除外。GTと同一物理コアのSMT相方でworkerが動いた時間は上位0.411/0.686ms、非上位0.337/0.365ms。強い共通説明ではないが、cache・電力・周波数など全CPUにまたがる競合はこの指標だけでは分からない。

taskの実行時間合計は並列・親子重複を含み、GT時間へ加算しない。全task件数は上位/非上位でr1=3568.9/3531.9、r2=3542.5/3534.6と近い。taskの内部仕事量が同一という証明ではない。

## 外部負荷とその他の周期処理

同一物理コアのSMT相方で外部processが動く時間は、上位で1.035/1.648ms、非上位で0.077/0.050ms。外部CPU占有はフレーム長で割った「使用コア相当数」でも増加する。WmiPrvSEは上位0.543/0.525対非上位0.158/0.150、ChatGPTは0.459/0.666対0.104/0.114。これらは同時発生であり、どのprocessがスパイクを生んだかの因果は未確定。アプリ停止やaffinity変更は実施しない。`external-processes.csv` / `external-frame-occupancy.csv` / `core-sharing-frames.csv` を保存。

GCの大きな区間は各走行1回、world約61.15秒。r1 CPU trace72.190197–72.207276秒で17.080ms、r2 72.716409–72.736724秒で20.315ms。その後に約2msのincremental purgeが3frameあり、「完全に1frameだけ」とはしない。大きなGCはp95超各1件に寄与するが頻発全体を説明しない。全tickの `gc-events.csv` が根拠。

選択したスパイク周辺で、streaming workerのMipCalc最大0.128/0.121ms、r2 GT streaming管理最大0.099ms。shader compiler workerは小さいpolling scopeであり、代表区間で数msのcompile burstを示す証拠なし。r1/r2 `selected-background-scope-max.csv` にCPU threadだけの最大値を保存。r2のGCは選択上位8区間に入らないため、全tickGC出力で別途確認した。

UEログのshader/streaming初期化は測定開始前。r1 run.logの1137–1196行、1655行（texture pool）、1694–1695行（結果）を参照。r2も1694–1695行に結果。ログにないことだけで処理なしとは判定しない。全IO経路、全workerの全60秒のscopeを分類し尽くしたわけではない。

## pending reportsとdeadline

pending_reports_peakは累積最大値。今回も差分とGTの相関は0.032/-0.019と弱い。現在queue長ではないため、現在の渋滞や原因方向は分からない。deadline latenessは全走行最大42.652/45.066msのみで、発生frameを特定できない。GT長時間化→次batchの遅延・catch-upという説明も残る。因果を断定せず、必要ならframe ID/current_queue/各sliceの投入・配送・deadline・latenessを既定OFFの計測で記録する案を残す。今回はゲームへの計測コード追加もしていない。

## 候補の優先順位、次に必要な検証と対策案（未実施）

1. **CPUの実行コア・実行効率の変動**：r2で最も強い。同じ仕事量の近傍比較に加え、light trace＋ETWを各2回、周波数・電力・ハードウェアカウンターと対応させる。実験的なPコア限定/OSスケジューリング比較は、現条件を変更するため別の明示的な試験条件として扱う。結果なしでaffinity固定を採用しない。
2. **外部負荷や共有資源の競合**：両traceで外部負荷・SMT共有の増加あり。外部負荷を抑えた条件を別に記録し、同DLL各2回で比較するのが候補。未実施。特定の常駐ソフトを原因と決めず、安定した専用測定環境を対策候補とする。
3. **移動内部の仕事量やバッチの偏り**：一部frameではMoveComponent増加、別のframeは同数で伸長。follow内の移動件数、sweep/overlap数、RVO候補数、batch期限をframe単位で観測する。仕事量が原因と確認されてから、意味を維持する計算削減案を検討する。今回未実装。
4. **worker開始待ち／GTのOS Ready待ち**：今回の代表区間では主因の可能性が低下。worker実行中の資源競合は1/2と合わせて調べる。タスク集約やGT移管を今回の結果だけで実装しない。
5. **GC**：単発大スパイクへの寄与は確認。長時間走行で周期と確保量を確認した後、割当削減等を検討する。streaming/shaderについては今回の選択区間から共通根因を支持しない。

## 検証と制限

- ETW lost events/buffersとも0、CPUごとのCSwitch連続性不一致0。GT時間会計最大誤差1.14e-10ms。Windows CSwitch v5はTDH schemaが未提供だったため、[Microsoft PerfViewの固定prefix offsets](https://github.com/microsoft/perfview/blob/main/src/TraceEvent/Parsers/KernelTraceEventParser.cs)と連続性検証で解読した（`etw-schema.txt`）。[Ready/CSwitchの状態定義](https://learn.microsoft.com/en-us/windows/win32/etw/cswitch)を使用。独立したWPA集計との照合は未実施。
- DLL、190source、5config、18asset、mapは最終基準manifestと一致。保護16はsource検証対象に含まれ変更なし。SavedのEngine/GameUserSettingsは全4本同一、走行前後も同一。CrashReportClientの自動生成設定だけ差分があるためSaved全体不変とは記載しない。全走行P3_RESULT failures=0。 [verification.json](verification.json)、[checks.json](checks.json)。
- 開発機i7-14700KF/RTX5070Tiのみ。基準PC換算・1660 Super級実機未取得。foreground/offscreen、通常常駐process、管理者権限の環境差も一般化しない。
- traceの負荷混入、GTとtick境界差、実時間走行の戦況差、各条件2回だけという制限あり。旧DLLの35走行への原因割合の転用はしない。
- worker命令数/cache miss/周波数、OSがEコアへ移した理由、外部負荷の因果、全IO経路、deadline時系列は未確認。単一の根因確定とはしない。

使用した追加物はSetup以下の測定・offline集計/decoderと文書のみ。ゲームソース・資産・通常設定の変更、最適化、追加のゲームビルドは実施していない。
