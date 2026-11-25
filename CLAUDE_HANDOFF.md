# Claude 開発・テスト引継ぎ書

作成日：2026-11-12（日本時間）。同じWindows作業コピーをClaudeから扱うための手順。以下の進捗は作成時点の記録であり、再開時には最新のSTATUSと実行中プロセスを確認する。

## 1. 最初に読む資料

|資料|役割|
|---|---|
|[AGENTS.md](AGENTS.md)|全AI共通の開発・検証ルール。必読|
|[STATUS.md](STATUS.md)|現在地、既知の制限、次の一手。冒頭の「要約」の表から読む。直近の項目だけを置き、古い項目は `Docs/journal/` にある|
|[MEASURE.md](MEASURE.md)|計測・試験の手順（1ページ）。計測の前に読む|
|[ROADMAP.md](ROADMAP.md)|承認済み方針・段階と範囲|
|[PROTOTYPE.md](PROTOTYPE.md)|起動、操作、試験の案内|
|[PERFORMANCE.md](PERFORMANCE.md)|性能の現在の基準値だけ（11-11 までの経緯は `Docs/journal/PERFORMANCE-2026-11-04_11-11.md`）|
|[HANDOFF.md](HANDOFF.md)|過去の作業経緯|
|[README.md](README.md)|元の全体構想。全機能が実装済みという意味ではない|

STATUS/ROADMAP/PROTOTYPEは追記型で、古い「未実装」「既定OFF」「50m」等が残る。日付だけでなく、冒頭の最新決定と対象モードを優先する。Setup/ENVIRONMENT.txt冒頭のUE 5.7未導入記録も履歴。矛盾があれば実コード・直近の検証資料で照合し、推測で完了扱いしない。

## 2. 作成時点の現在地

- UE 5.8.4の環境と20体試作は完成済み。工程1〜8の試作実装と最終area-r6検証が完了。認識100m、射程50m（訓練倍率あり）。
- 混成広域は隊長Actor＋Mass部下、500体では50 Actor＋450 Mass。Mass部下の遠距離VATは通常ON。旧全Actor・隔離隊長・Phase3とは処理/試験条件を区別する。
- RTS/TPS操作、AI委任、撤退・集合・援護・迂回、混成FastMatch、1エリア戦の補充/勝敗が実装されている。一戦中のセーブ・ロード（混成保存）は 2026-11-25 のユーザー決定で削除した。
- area-r6は基本210、Area24、混成Fast15、混成保存35が成功。A/B×陣営入替4戦＋描画B1戦が600秒完走。これは当該ビルドの記録で、以後の変更の合格を保証しない。[最終報告](Setup/Benchmarks/SightTrial-20261111/report-area-final.md)。
- **flank-effect-r1比較は本書作成時に実行中であることをプロセスで確認した。** 従来/緩和/片軍のみ迂回×A/B×seed7/19/43×配置入替の48戦。予備4戦の結果を全体結論にしない。
- 次の承認済み工程は、現行比較の終了・集計・証拠固定後に「方向性のある遮蔽と短い側方経路」の局所検証。その後にカバー動作/照準切替、連携改善、全体マップ拡張という順序。現行A/Bを保持した別環境で段階的に進める。

残る制限：通常戦闘の迂回中断と帰還後の移動待ち、購入/補充兵のskeletal fallback、混成Fastの死亡込み登録500上限、混成への旧支援砲撃未接続。配布用cook、全視点60FPS、基準PC性能、キャンペーンの完成は宣言していない。開発機での結果を基準PCへ転用しない。

## 3. 環境と開始前確認

作業ルートは、既定では main の `C:\Users\nanek\Documents\FPS\FutureBattle`。別の作業ツリー（`FutureBattle-codex`・`FutureBattle-suppression` など）で作業するときは、下の `Set-Location` をその作業ツリーの場所に置き換える。スクリプトは自分の置き場所から対象のプロジェクトを決めるので、別の作業ツリーから main の場所に移ると、別の DLL・Config・STATUS を扱ってしまう。以下はWindows PowerShell/PowerShellから実行する。Bashの構文と混ぜない。Claude側にはローカルファイルの読み書きとPowerShell実行権限が必要で、通常のチャットへこの文書を渡しただけではゲームを実行できない。

```powershell
Set-Location 'C:\Users\nanek\Documents\FPS\FutureBattle'
Get-Content AGENTS.md
Get-Content STATUS.md -TotalCount 100
git status --short -uno
git ls-files --others --exclude-standard Game/FutureBattle/Source Game/FutureBattle/Config
Get-CimInstance Win32_Process |
    Where-Object { $_.Name -match 'UnrealEditor|powershell|pwsh' } |
    Select-Object ProcessId, Name, CommandLine
```

`-uno`は概要用。変更対象の未追跡ファイルも確認し、必要時は通常の `git status --short` を使う。作成時点で多数の未コミット実装・マップ・測定証拠があるため、HEADだけを最新成果と扱わない。`git reset --hard`、`git clean`、無断のcheckout復元・stash・一括addを行わない。

UE配置は `C:\Program Files\Epic Games\UE_5.8`、プロジェクトは `Game/FutureBattle/FutureBattle.uproject`。Visual Studio 2022/MSVC/Windows SDKは構築済み。新PCへ移す場合は再確認するが、このPCで再インストールを始めない。スクリプトによって `-EngineRoot` 対応の有無が異なり、UEパス固定のものもある。

別PCにGit cloneしただけでは未追跡実装・資産・無視対象DLL・測定証拠が揃うとは限らない。移行時は必要なSource/Config/Contentと使用する証拠一式を確認し、同じUE版でビルドする。本書は全成果物の配布パッケージではない。

## 4. ビルドと最小回帰

**進行中の比較/計測が終了し、他作業が同じDLLを使用していないことを確認してから実行する。** Editorを閉じる必要がある場合は保存して正常終了する。資料変更だけならビルド・ゲーム試験は不要。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup\Build-Editor.ps1
if ($LASTEXITCODE -ne 0) { throw 'Build failed; inspect Setup/Build-Editor.log' }
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup\Test-Battle.ps1
if ($LASTEXITCODE -ne 0) { throw 'Battle smoke failed; inspect the printed log path' }
```

`ExecutionPolicy Bypass`はこの子プロセスだけの指定で、端末の永続設定変更は不要。Build-EditorはFutureBattleEditor/Win64/Developmentをビルドし、Source一覧の変化も処理する。ログは `Setup/Build-Editor.log`（次回上書き）。必要な証拠は実行ごとの出力先へ保存する。

Test-Battleは通常NullRHIで実行し、`Setup/Battle-Smoke-日時.log`へ記録する。**`BATTLE_SMOKE_RESULT failures=0`が必須**。FAIL、Fatal error、Assertion failed、timeoutがあれば失敗。プロセス終了だけで合格にしない。試験件数は将来変わり得るため、210という数だけを合格判定に使わない。

`Build-Editor.ps1`には `exit` があるため、複数手順を連続実行する親シェルからは上例のように子PowerShellで呼ぶ。ビルド失敗後に古いDLLで試験を続行しない。

## 5. 変更箇所に応じた試験

スクリプトの `param` と末尾の判定を読んでから、必要な試験を選ぶ。全ベンチを毎回実行する必要はない。各実行に未使用の英数字/ハイフン/アンダースコアのOutputNameを付け、既存証拠を上書きしない。

|変更対象|主な専用試験（Setup配下）|
|---|---|
|Area補充・資源・本拠地・勝敗|`Test-Area.ps1`|
|混成購入・予算|`Test-MixedFast.ps1`|
|混成広域全般|`Test-WideMixed.ps1`|
|共有移動・衝突・到着予約|`Test-MovementStability.ps1`、`Test-ContactMovement.ps1`|
|撤退・援護・迂回|`Test-SquadRetreat.ps1`、`Test-TeamSupport.ps1`、`Test-SquadFlank.ps1`|
|直接操作・敵表示・回避|`Test-TPSControl.ps1`、`Test-TPSVisibility.ps1`、`Test-TPSAvoidance.ps1`|
|隊長/Mass・VAT|`Test-LeaderCommand.ps1`、`Test-LeaderPhase3.ps1`、`Test-VATDisplay.ps1`（条件を確認）|

例：Areaを変更した場合の描画回帰。

```powershell
$runName = 'claude-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
.\Setup\Test-Area.ps1 -OutputName $runName -Rendered
```

Areaの成功ログは `BATTLE_AREA_RESULT failures=0`。他試験は各スクリプトが要求する結果行・FAIL不在・画像・result.jsonを確認する。`-Rendered`は通常オフスクリーン描画で、表示された窓を手動操作する試験とは異なる。画像が生成されたことと目視確認済みを区別する。Claude側で画像を開けない場合はその検証を未実施として記録する。NullRHI合格から描画品質やGPU性能を推定しない。

一戦通しの確認と固定機構試験も分ける。通常A/Bの双方、配置入替、複数seedを変更範囲に合わせて選ぶ。旧全ActorやPhase3の合格を混成Areaの合格として代用しない。

## 6. 手動起動と操作

```powershell
.\Setup\Play-Area.ps1 -Map A
# Bマップは -Map B。別モードを試す前に前のゲームを終了する。
# 混成購入戦: .\Setup\Play-MixedFast.ps1 -Map A
# 従来の広域入口: .\Play-TestField.cmd
# Editor: .\Setup\Open-Editor.ps1
```

AreaはQで全選択→JでAI委任→Pで開始。RTSはWASD/ホイール、右クリック移動、T→右クリック攻撃移動、G→右クリック防衛、H停止。Eで選択隊長をTPS操作し、TabでRTSへ戻る。TPSはWASD/マウス/左クリック射撃/Rリロード。Kで手動指揮に戻す。混成Fastは1/2/3で購入、Enterで開始。

一戦中のセーブ・ロード（旧 F8 保存／F9 読込）は無い（2026-11-25 に削除）。

## 7. コードの入口

主なゲーム実装は `Game/FutureBattle/Source/FutureBattle/Prototype/`。機能ごとの入口と各ファイルの役割は [CODEMAP.md](CODEMAP.md)、関数の行は [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md)（どちらも `Setup/Export-CodeMap.py` で自動生成）。下の表は概略で、詳しい方は CODEMAP を正とする。

|ファイル群|確認する内容|
|---|---|
|BattleGameMode、BattleState、BattleUnit|モード統合、正規状態/ID、Actor|
|BattleController、BattleHUD|入力、RTS/TPS、UI|
|BattlePerception、BattleTacticalAI、BattleSquadAI|認識と既存AI|
|BattleTeamAI、BattleSquadTactics、BattleSquadRetreat、BattleSquadSupport、BattleSquadFlank|全体/部隊判断、撤退、援護、迂回|
|BattleMixedDriver、BattleMixedBodies、BattleLeaderRuntime|Actor/Mass混成の実行・登録|
|BattleStableMovement、BattleContactMovement|共有移動、混雑・接触|
|BattleCover、BattleFieldLayout、BattleMapCommandlet|遮蔽、配置、マップ生成。既存手編集資産を保持|
|BattleArea、BattleAreaState、BattleMixedRecruitment、BattleMatch|Areaと補充、試合/経済|
|BattleVATDisplay|VAT描画と表示登録|
|各 *Smoke.cpp / *Trial.cpp|専用試験・計測用fixture|

調整値は `Game/FutureBattle/Config/DefaultGame.ini` 等。保存済み画質設定も結果に影響するため、Default設定だけを条件としない。既存資産を再生成する `-Force` を安易に使わない。

## 8. 比較測定と同時作業

1. 作業開始前に担当範囲と実行中プロセスを確認する。同じ作業コピーでCodex/Claudeが同じソースやDLLを並行更新しない。実行中なら読取・資料整理に留める。
2. C++変更→ビルド→専用/基本回帰の後、必要なら `Setup/New-BattleBuildSnapshot.ps1 -Name <未使用名>` でSource/Config ZIP・DLL hash・マップhashを固定する。snapshotは別checkoutや専用実行DLLの代わりではない。
3. 計測開始前にGet-MeasurementMachineを呼び、manifest.machineへJSON深さ5以上で保存する。対応済みスクリプトを優先する。GPU/VRAM取得元、CPU、RAM、OSと実描画GPUを記録する。後付けしない。
4. 引数、seed、map、配置側、DLL/Source/Config/資産hash、解像度/画質、描画有無、並列数、開始終了、exit codeを証拠に残す。途中変更を検出した結果は同一比較に混ぜない。
5. 描画性能は単独実行。既存Matrixの並列NullRHI挙動試験を、そのままFPS測定として扱わない。trace負荷あり/なしも分ける。

現行比較の入口は `Setup/Run-FlankEffectMatrix.ps1`、実行器は `Setup/Test-WideSight.ps1`、解析は `Setup/Analyze-FlankEffect.py`。結果は `Setup/Benchmarks/SightTrial-20261111/`、固定情報は `Setup/Benchmarks/BuildSnapshots/flank-effect-r1/manifest.json`。**既に実行中ならMatrixを重複起動しない。** 完走数・各result/manifest・ログを確認してから解析する。停止した場合も成功済みと失敗/途中結果を分け、既存出力を削除して再利用しない。

## 9. 作業終了時に残すもの

- 依頼と実装範囲、変更したファイル、変更理由。
- ビルド/試験コマンドと結果、実行ログ・画像・manifestの場所、対象DLL/ソース。
- 未実施、失敗、既知の制限、次の具体的な一手。
- STATUSの最新節。方針を変更した場合はROADMAP、重要な経緯はHANDOFFも更新。

既存変更を含めた一括コミット/pushを引継ぎ手順として行わない。依頼範囲外の環境変更・追加最適化・長期ロードマップの一括実装へ進めない。

## 10. Claudeへ渡す開始文

```text
C:\Users\nanek\Documents\FPS\FutureBattle を作業ルートにしてください。
CLAUDE.md、AGENTS.md、STATUS.md冒頭、CLAUDE_HANDOFF.mdを読み、
計画はROADMAP.md、操作はPROTOTYPE.mdで確認してください。
日本語で対応し、未コミット変更を保持してください。
まず実行中のUE/比較計測と対象ファイルの状態を確認し、他作業に干渉しないでください。
今回の依頼：［ここにテスト/修正/開発の具体的な依頼を書く］
変更範囲に応じたビルド・回帰・描画確認を行い、結果と未実施事項をSTATUSへ残してください。
```

本書作成では文書とスクリプトの照合、実行中プロセスの読取確認を行った。ゲーム実装変更、ビルド、ゲーム試験の再実行は行っていない。
