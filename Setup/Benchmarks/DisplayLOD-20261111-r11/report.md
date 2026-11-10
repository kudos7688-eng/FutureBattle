# ISM＋VAT 段階B：検証・性能報告

**結論：RTSはGT平均を約9.65ms削減。TPSは約4.21ms、FPSは約1.52ms削減したが、全視点60FPS達成には至らない。感覚・戦闘の同入力一致と既存回帰を確認。通常採用・画質許容差の拡大は行わず、既定OFFの試験専用で停止する。**

|視点|GT平均 OFF→ON|削減量|ON p95（2反復範囲）|ON 16.67ms超過率|
|---|---:|---:|---:|---:|
|RTS|20.678→11.028ms|9.650ms / 46.7%|14.141–14.452ms|0.089–0.094%|
|TPS|21.156→16.946ms|4.210ms / 19.9%|19.493–22.817ms|39.4–46.7%|
|FPS|20.790→19.265ms|1.525ms / 7.3%|20.566–26.316ms|96.4–99.4%|

同DLL・既存3最適化ON・ReduceAnimTasks OFF・Leader1000体・高HP・Open・seed1・1080p・warmup10秒＋60秒・OFF/ON各2反復。GT平均は走行平均2本の単純平均、p95/超過率は各走行の範囲。過去DLLの結果と加算/混合していない。RTSでも超過frameは残り、常時60FPS保証ではない。

RT/GPU平均はRTS 3.060/2.636→1.891/1.475ms、TPS 3.168/2.555→2.809/2.009ms、FPS 3.169/2.404→3.095/2.212ms。遠表示対象は平均900/547.5/371.0体（RTS/TPS/FPS）。視点別の対象人数と可視対象数が異なり、RTSの削減率をFPSへ転用できない。

[12走行・GT/p95/超過率・認識等の内訳](performance-tables.md)、[RT/GPU・対象人数・scope補足](supplement-tables.md)。性能は既存の低設定（影/Lumen/反射OFF）。影あり1000体性能は未測定。開発機 i7-14700KF / RTX5070Ti / 約64GB RAM、基準PC換算未取得。


通常既定OFF、Phase3専用。既存3最適化ON、ReduceAnimTasks OFF。通常採用・画質許容差の拡大は未承認。

## 実装と資産

近い部下は元のSkeletalMesh、遠い既存LOD2の部下だけ通常ISM＋頂点VATへ切り替える。元LOD2のgeometry/UV/材質を派生資産へ複製。身体・2武器、待機/歩行/リロードを60Hz、位置/法線/接線のRGBA16Fへ焼く。死亡は現在位相の固定と既存の傾斜、武器非表示。操作可能Actorは対象外。Capsule・正規状態・認識・戦闘・移動は変更しない。

部下の元Tick順序で時計を進め、遠表示中だけ骨評価とSkeletal描画を止める。Component自体とCapsuleへの接続は残す。近表示復帰では現在時刻の骨を評価してから身体・武器を表示する。遠表示を同じ位相に丸めたり、アニメ評価全体をGTへ移したりはしない。

RTS/TPS/FPSの切替は設計Aの48/32/24px相当＋20%ヒステリシス（1080p・FOV65/90/90で遠表示への切替約67.8/64.8/86.4m、復帰45.2/43.2/57.6m）。表示品質採用基準として承認された値ではなく、今回の試験値。

生成資産18件はContent/Prototype/DisplayLODTrialだけ。元資産は上書きしない。Epic同梱AnimToTextureのstatic変換補助のみ使用し、時刻・武器補正のサンプリングは専用baker。Editor限定plugin依存。元Quinn/animationはUE由来、武器は既存Kenney CC0、出典・条件は段階A設計書とThirdPartyの記録を継承。新しい外部素材のダウンロードなし。

GPUテクスチャの非圧縮texel計算は約265.1MiB（9枚、mipなし）。ディスク資産約43.0MiB。実resident VRAM増分は未測定。保存時の位置丸め最大は身体0.037653cm、carbine0.087796cm、support0.084462cm。これはサンプル点だけで、時間補間誤差を含む総誤差ではない。元CPU skinのpacked tangent精度、VATの補間/丸めによる法線・silhouette差もあり、pixel一致・完全な遠表示の姿勢一致は主張しない。

## 測定条件上の注意

前回AnimTasksを含む既存ベンチは保存済み低設定で、影/Lumen/反射OFFだった。性能比較はその既存条件を維持し、影ありQAは別走行（40体、影/Lumen/反射を高設定）。QAのGameUserSettingsは起動前に退避し終了後にbyte単位で復元する。高画質1000体の性能は未測定。品質低下を新たに導入して短縮した測定ではないが、影あり通常プレイへ性能値を一般化できない。

## 修正過程（最終結果と混合しない）

初期候補で資産メタデータのUV channel保存漏れ、表示時計のTick順序差を検出して修正。さらにUEのISMレイトレーシングWPO設定sinkが全component再登録をGT taskへ遅延予約し、初回並列アニメ評価と競合する経路を確認。設定・再登録完了を兵士生成前へ移動。NullRHIでは不要なray設定を変更しない。個体ごとに位相を保持するためray WPOはcomponent単位に有効、最大250体/chunk（UEの256上限内）。試験終了時に設定を戻す。

r1〜r10は実装途中の検証で、最終性能・回帰結果へ加算しない。最新候補の検証出力はr11。未完了の検証を成功とは扱わない。

## 結果

既存210 PASS・BATTLE_SMOKE_RESULT failures=0、OFF固定戦闘40/40一致、Phase3 OFF/ON各25 PASS、3視点1000体の各1349frame感覚/戦闘状態一致を確認。性能12走行完了。別RTS traceの内訳も集計済み。開発機のみ、基準PC換算・1660 Super実機は未取得。
### 確定候補DLLでの小規模QA

DLL SHA256: 72BD51BE335008EE5E78E277C4807C96EAE664EF9170FFC35E3060536CAF22A6。
Build-Editor.ps1成功。機能試験OFF/ON各25 PASS。通常低設定/影あり設定×RTS/TPS/FPSの6走行で、各12960比較・不一致0・enter108/return108。時計は全期間、骨・武器transformは近表示/復帰時、3clipと遠近死亡を検査。遠表示中の全GPU頂点を骨と完全一致検査したという意味ではない。

RTSのquality画像119/121（動作中切替、時刻は2frame異なる）、299/301（凍結姿勢で切替）を確認。身体/武器/影の消失、Tポーズ、死亡姿勢の再開は見られない。TPS149/FPS181では2武器と生存/死亡の混在を確認。拡大fixtureで強制切替した画像であり、通常RTS/TPS/FPSのカメラ距離そのものではない。通常視点は別の1000体replayのframes.pngで確認済み。微小な輪郭/法線/陰影差を許容した採用判定ではない。

### 同入力比較の範囲

既存RecordContextReplayでUnitState/SquadState、位置/向き/速度、隊長plan/経路/RVO入出力/発砲/死亡/判断、全Observer（発見時刻・記憶・共有contact）とPendingReports/Counts/scan時刻を毎frame fingerprint化してOFF/ON比較。表示の骨データを正規状態へ混ぜていない。試験入力での一致であり、全マップ/全seedの数学的証明ではない。

実時間の性能走行はframe cadenceが変わるため、発砲数や共有処理量まで同じになる保証はない。固定入力一致と性能差を別に報告する。認識・視線等の観測差を新しい認識最適化の効果とは扱わない。

### 画質差・確認の限界

3視点1000体replayのOFF/ON画面を確認。近いSkeletal表示・一人称武器・HUD/小隊ラベルは保持。RTSの遠い部下は小さく、画像だけで全頂点の差を識別できない。TPS/FPSで背景上部が黒いのはOFFにもある既存画面で、VAT追加で生じた差ではない。

固定姿勢の遠/近切替でも微小な法線・陰影・輪郭差はあり得る。身体/武器の位置サンプル丸め差は測ったが、全連続時刻のVAT補間誤差最大、GPU出力全頂点との厳密比較、時間方向の画素差、全clipループ端・全カメラ/zoomの網羅は未測定/未検証。画面全体のpixel一致、画質許容差内という採用判定はしない。影ありQAは特定の光源/40体fixtureに限る。

同じ描画品質を許容するかの判断を先取りせず、今回の方式は既定OFF・試験専用のまま残す。表示以外の最適化や更新頻度低下は追加しない。

### 画像の参照先

- [RTS・動作中の近表示](quality-RTS/frames-vat-119.png) / [遠VAT表示](quality-RTS/frames-vat-121.png)（2frame時間差あり）
- [RTS・死亡固定の遠VAT](quality-RTS/frames-vat-299.png) / [近表示へ復帰](quality-RTS/frames-vat-301.png)
- [TPS・2武器](quality-TPS/frames-vat-149.png) / [死亡固定](quality-TPS/frames-vat-299.png)
- [FPS・生存/死亡混在](quality-FPS/frames-vat-181.png) / [死亡固定の復帰](quality-FPS/frames-vat-301.png)
- 通常カメラ1000体：[RTS OFF](replay1000-RTS-0/frames.png) / [ON](replay1000-RTS-1/frames.png)、[TPS OFF](replay1000-TPS-0/frames.png) / [ON](replay1000-TPS-1/frames.png)、[FPS OFF](replay1000-FPS-0/frames.png) / [ON](replay1000-FPS-1/frames.png)

画像はエンジンが描画した1080pの出力。拡大fixtureでは実プレイ距離より近くで遠VATを表示し継ぎ目を確認した。通常カメラ画像では敵未発見時に非表示であること、一人称武器と小隊表示が残ることを確認。全画面pixel一致とはしない。

## 変更ファイルと再現手順

- 既存C++：BattleLeaderRuntime.cpp/.h（試験フラグ・表示hook）、BattlePhase3.cpp（切替hook・描画設定ログ）、FutureBattle.Build.cs（Editor baker依存）。FutureBattle.uprojectはEpic同梱AnimToTextureをEditor限定有効化。
- 新規C++：BattleVATDisplay.cpp/.h、BattleVATData.h、BattleVATBake.cpp/.h、BattleVATBakeCommandlet.cpp/.h、BattleVATMaterial.cpp。
- 派生資産：Content/Prototype/DisplayLODTrialの18件。通常起動では読み込まない。
- 試験専用：Test-VATBake.ps1、Test-VATDisplay.ps1、Measure-VATDisplay.ps1、Report-VATDisplay.py、Summarize-VATDisplay.py、Verify-VATDisplay.py、Export-VATTrace.ps1、Analyze-VATTrace.py。
- 文書：STATUS.md、ROADMAP.md、HANDOFF.md、PERFORMANCE.md、今回の設計/実測記録。既存の汚れた変更は保持し、commit/resetはしていない。

[完全な今回実装差分](../DisplayLOD-20261111-source/implementation.patch)、[整合検証](verification.json)、[OFF40/210のmanifest](../DisplayLOD-20261111-off/manifest.json)。各走行manifestにGet-MeasurementMachineの実行前取得、DLL/ソース/設定/マップ/資産hashとコマンド引数を保存。GPUは実ログのRTX5070Tiと照合済み。

再実行はBuild-Editor.ps1後、Measure-VATDisplay.ps1のFunctional/Pose/Quality/Replay/Perf/Traceを新しいPrefixへ順番に実行する。他のUE計測/ビルドを重ねない。既定DisplayLOD=0、共通ReuseSightContext/ReduceHiddenBones/PerceptionGrid=1、ReduceAnimTasks=0。保護16と元資産/設定のhash照合、Report/Verifyで集計する。

## 残る制限と次の判断

高速接近で多数を同時に近表示へ戻す場合の専用burst性能、影あり1000体性能、実resident VRAM増分、全時刻のVAT頂点誤差上限は未測定。今回の範囲外のmap/seed/長時間/基準PCは未検証。ゲーム状態の一致は固定入力検証の範囲に限定する。

次に優先する候補はTPS/FPSで残る近表示Skeletal処理の観測と、完全に同じ評価入力を共有できる人数の調査。共有の実装や削減量は未確認/未測定。切替距離を近づける、影を省略する、評価頻度を落とす変更は今回行わず、画質を変える案として別承認が必要。現在のVAT方式も通常採用せず、品質差・高画質性能を判断材料にする。新たな最適化は追加しない。

### 視点で効果が違う理由（コードと対象数からの推定）

可視フラグが立つ遠表示対象は平均RTS491.4/TPS199.1/FPS73.9体。既存の非表示骨更新削減が共通ONなので、fog非表示の遠方兵はOFFでも骨評価の一部を省いている。RTSは部下900体全体をVATへ移せる一方、FPSは近い可視部下のSkeletal処理が多く残るため、追加効果が小さいと考えられる。可視フラグ数は画面内pixel数ではなく、この人数差だけから厳密なms寄与を算出したものではない。

## 残るGT内訳と最終整合確認

[排他的trace内訳](trace-tables.md)、[trace会計検証](trace-verification.json)。RTS別traceではアニメTick＋GT反映6.044→0.081ms、描画更新2.938→0.551ms、新しいISM/VAT更新0.141ms。ONでGameMode6.901ms、その他2.206ms、CharacterMovement1.226msが残る。両traceの分類合計はengine tickと一致（会計誤差0、負のself time 0）。通常12走行にtrace値を混合しない。TPS/FPS詳細traceは未測定。

最終verification.jsonは28走行・同一DLL/ソース/資産/設定/マップ、210 PASS、OFF40/40、Phase3各25、6回の切替QA、3視点の全frame状態一致を確認。元Content/Configと保護16のhash不変。保存GameUserSettingsも開始前のbaselineとSHA256一致で復元済み。Build成功ログは../DisplayLOD-20261111-source/build-candidate7.txt。UnrealEditor/Insightsの走行は終了。追加最適化・通常採用へは進まない。
