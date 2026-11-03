# FutureBattle 開発引き継ぎ書

作成日時：2026年10月31日 10:57 JST

FutureBattleは、Unreal Engine 5を使用するWindows向けFPS×RTSゲームです。環境構築後に別プロジェクトへ引き継ぐため、仕様、現在の資産、環境設定、再開手順をまとめています。

**2026-11-04更新：UE 5.8.4でC++ Third Person作成・Development Editor / Win64ビルド・Play確認済み。Git初期コミットはユーザー希望で後回し。以下の2026-10-31時点の記録は履歴です。最新状況は末尾を参照してください。** Visual Studio Communityは導入処理中、UEはインストール完了の登録がありません。ゲーム本体のコード・`.uproject`・マップはまだ作成していません。環境構築が完了したら、末尾の完了確認欄を更新して引き継いでください。

## 最初に読む資料

- `README.md`：ユーザー提供の開発仕様書v0.1。ゲーム仕様の原本。
- `HANDOFF.md`：本書。開始時点の資料と状況が異なる場合、環境の最新状態はこちらを参照し、実機でも再確認する。
- `NEXT_PROJECT_PROMPT.txt`：次のプロジェクトに貼り付ける依頼文。
- `Setup/ENVIRONMENT.txt`：環境構築の補足記録。
- `START_HERE.txt`：環境構築前の記録。最新の導入状況として扱わない。

元のZIPはREADMEとPC確認スクリプトのみを含んでいました。完成済みゲームや既存UEプロジェクトはありません。

## 開発するゲーム

SF銃撃戦を中心に、俯瞰でのRTS指揮と味方ユニットのFPS直接操作を融合します。最終的に500～2,000体規模を目指しますが、最初は20体で動作検証します。初期対象はWindows・シングルプレイです。

| 項目 | 仕様の要点 |
| --- | --- |
| ファストマッチ | 初期予算、雇用、拠点収入、補給、増援、勝利条件を備えた1戦完結型 |
| キャンペーン | 領土、資源、研究、経験、部隊損耗を永続化 |
| キャンペーンA | 国家・全軍を操作する戦略シム |
| キャンペーンB | 上級司令部の作戦を受ける部隊指揮シム。配下をFPS操作可能 |
| キャンペーンC | 個人と少数精鋭を育成するヒーローシム。FPS主体 |
| B/Cの発言力 | 戦功で獲得し、条件を満たした補給・支援などをポイント消費で確定取得。初期版に承認抽選は入れない |
| 戦闘 | 射線、高低差、遮蔽、側面、制圧、士気、兵科連携を重視 |
| 情報 | 偵察・通信による共有。未発見敵の正確な位置を表示しない |
| テストモード | 条件を固定したAI・バランス・負荷試験 |

全ユニットに永続UnitIDを持たせ、正規データと表示Actorを分離します。操作対象を切り替えてもHP・弾薬・命令を保持します。描画LODとシミュレーションLODも分離します。

技術方針はUE5＋C++＋Blueprint/UMG。Mass Entity/MassGameplayは大規模化時の候補であり、導入・動作検証済みではありません。AIは戦略／作戦／分隊／個体の階層を想定します。将来のマルチプレイはサーバー権威型とし、未発見敵情報の配信を制限します。

## 実装順序と最初の完了条件

1. **環境確認**：UEのC++ Third Personテンプレートを作成し、Development Editor / Win64でビルド。エディタのPlayで動作確認し、初期コミットを残す。
2. **20体**：FPS移動・射撃、AI索敵・射撃、RTS選択・移動命令、味方への操作切替。切替前後のUnitID・HP・弾薬・命令の保持を確認する。
3. **100体**：分隊命令、遮蔽・側面、偵察・共有、砲撃、拠点占領。
4. **ファストマッチ**：予算・雇用・増援・勝敗・保存、繰り返し試験。
5. **ネットワーク実証**：2人・20～100体で同期を検証。
6. **500～2,000体**：Mass、LOD、経路・戦闘の最適化。Unreal Insightsで計測。
7. **キャンペーン**：戦略層、A/B/C固定選択、発言力、戦闘結果引き継ぎ。
8. **PvP/PvE拡張**。

巨大な一括実装は避け、動く段階ごとに保存します。生成コードは実機でビルド・実行して確認します。価格・性能値は仮値としてデータ駆動で調整できるようにします。

## 配置と移行対象

現在の資料・Gitリポジトリ：

```text
C:\Users\nanek\Documents\新しいフォルダー (2)\FutureBattle
```

このフォルダーは開始資料の置き場です。UEプロジェクトの作成先は未確定。仕様書の推奨例は `C:\Dev\FutureBattle` ですが、まだ作成・移行していません。

同じPC上で別のCodexプロジェクトへ引き継ぐ場合、Git、Visual Studio、UEなどの導入を最初からやり直さず、実機状態を確認してください。別のPCでは開発ツールを別途導入する必要があります。

資料一式のZIPは、仕様書、引き継ぎ書、確認・構築スクリプトを含みます。ゲーム本体、Git履歴、インストーラー、ビルド出力、Launcherログ、アカウント情報は含めません。未加工のLauncherログには署名付きURLなどが含まれるため、引き継ぎ資料に追加しないでください。

## PCと開発環境

| 項目 | 確認結果 |
| --- | --- |
| OS | Windows 11 Pro / 10.0.26300 |
| CPU | Intel Core i7-14700KF、20コア・28論理プロセッサ |
| RAM | 約63.8GB |
| GPU | NVIDIA GeForce RTX 5070 Ti。VRAM容量は未確認。WMIの4GB表示は採用しない |
| 空き容量 | 初回確認時C約335GB、D約528GB、E約246GB。導入で変動するため再確認 |
| 既存Build Tools | Visual Studio Build Tools 2022 17.14.42、`C:\BuildTools` |
| 既存MSVC | 実バージョン14.44.35229、フォルダー名14.44.35207 |
| 既存Windows SDK | 10.0.26100.0 |
| Git | 2.56.0.windows.4、`C:\Program Files\Git\cmd\git.exe`。導入・実行確認済み |
| Visual Studio Community | 2022 17.14系を導入中。NativeGame＋推奨コンポーネントを指定。完了未確認 |
| Epic Games Launcher | 公式20.3.3 MSIの検証・導入成功。その後の初回更新と起動、ユーザーのログイン完了 |
| UE | 5.7.4を選択。インストール完了未確認。直近の画面確認では一時停止0% |

**成功した検証**：既存Build ToolsでC++20のテストプログラムをコンパイルし、Windows APIにリンク、実行に成功。28論理プロセッサを取得。

**まだ成功を確認していない検証**：新規Community側のコンパイラ、UEプロジェクトのビルド、エディタ起動、Play、パッケージ化、負荷計測。

10:57 JSTの読み取り確認では、`vswhere`が完了済みとして列挙したのは既存Build Toolsのみ。Communityインストーラーのプロセスは稼働していました。Launcher登録ファイルのInstallationListは空でした。これらは作成時点の結果であり、受領時に更新してください。

## 導入設定と通信トラブル

Launcher実行ファイル：

```text
C:\Program Files\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe
```

UEの選択済み保存先は `C:\Program Files\Epic Games\UE_5.7`。本体、テンプレート、エンジンソースを選択。MetaHumanコアデータ、エディタシンボル、Android/iOS/Linux/TVOS/Windows ARM64は未選択でした。

- wingetのDelivery Optimizationによる取得が進まなかったため、公式URLから直接取得。配布元SHA256とAuthenticode署名を検証して導入しました。
- LauncherのMSIは最初に管理者権限不足1925／終了コード1603で失敗。管理者として再実行し終了コード0で成功しました。
- UEのmanifest取得が30秒でタイムアウトしたため、Launcherを終了し、ユーザー側設定に以下を追記して再起動しました。

```ini
[HTTP]
HttpTotalTimeout=600
HttpConnectionTimeout=60
HttpActivityTimeout=120
```

設定先：`C:\Users\nanek\AppData\Local\EpicGamesLauncher\Saved\Config\Windows\Engine.ini`

既存ファイルがある場合は `Setup/Launcher-Engine.ini.before-timeout` に退避するスクリプトです。今回のファイル一覧では退避ファイルは確認されていません。復旧時にバックアップの存在を決めつけないでください。タイムアウト延長による本体取得の成功は未確認です。

UEの初期化0%が続き、一時停止・再開を試みましたが、最後の確認では一時停止のままでした。その後ユーザーから「すこしまってあげて」と指示があり、追加操作を停止しました。引き継ぎ前に最新状態を読み取り、操作の重複や、進行中インストーラーの再起動を避けてください。

## スクリプトの用途

| ファイル | 用途・注意点 |
| --- | --- |
| `Check-PC.ps1` | ユーザー提供のPC情報確認。CIM権限が必要な場合がある |
| `Setup/Check-Environment.ps1` | VS、Git、SDK、Launcher、UE登録・主要ファイルの確認。独自配置は `-EngineRoot` で指定。エディタの実行成功を証明するものではない |
| `Setup/Check-Compiler.cmd` | vswhereでC++ツールがある最新VSを選び、テストをビルド・実行。生成物はSetup/Outputへ保存 |
| `Setup/CompilerSmoke.cpp` | Windows APIとC++20ビルドの確認用。ゲームコードではない |
| `Setup/Install-Tools.ps1` | GitとVSの公式取得元・ハッシュ・導入引数。インストール中は再実行しない。既存ファイルのハッシュ不一致時は停止する |
| `Setup/Install-Launcher.ps1` | MSIの検証と管理者インストール。Windows Installer使用中の1618は待機して再試行 |
| `Setup/Set-LauncherTimeout.ps1` | 今回のPC専用パスを含む設定変更。既存HTTP節があると停止するため、設定済みの今は再実行しない |

構築スクリプトは今回使用した配布版・ユーザーパスを含み、汎用の無条件再実行用ではありません。別PC・将来の再構築ではバージョンと配布元を確認してください。

## Gitの状態

`main`ブランチで初期化済みですが、コミットは0件です。ファイルは未追跡の状態。リモートも未設定です。ユーザー名・メールは開始時に取得できておらず、本人の情報を確認せず作成しないでください。

`.gitignore`はUE生成物、IDE生成物、確認用出力、取得したインストーラー、セットアップログ、設定バックアップを除外します。動作確認後、仕様・設定・ソースを初期コミットしてください。

## 引き継ぎ前に完了させる確認

1. 進行中のVisual Studioインストーラーの終了を確認する。終了コード0または3010を確認し、3010なら再起動の要否をユーザーに伝える。新しい導入を重ねて起動しない。
2. `vswhere`でCommunityとC++ツールの登録を確認する。`Setup/Check-Compiler.cmd`の実行先と結果を記録する。
3. UEの現在の表示を確認する。一時停止のままなら、ユーザーの待機指示が解除されたことを確認して再開する。設定変更や一時停止を短い間隔で繰り返さず、取得時間を確保する。
4. UE 5.7.4の導入が終わったら `Setup/Check-Environment.ps1` とBuild.versionでバージョン・配置を確認する。
5. 短い英数字パスにFutureBattleのC++ Third Personテンプレートを作成する。既存フォルダーを上書きしない。
6. Development Editor / Win64でビルドし、エディタ起動、Play、基本移動を確認する。成功ログとプロジェクトの絶対パスを記録する。
7. 本書の完了確認欄と `Setup/ENVIRONMENT.txt` を更新し、次のプロジェクトへ渡す。

## 環境構築完了時の追記欄

| 確認事項 | 現在の記録 |
| --- | --- |
| 確認日時 | 2026-11-04 00:10 JST |
| Visual Studioの製品・バージョン・パス | Community 2022 17.14.44 / C:\Program Files\Microsoft Visual Studio\2022\Community |
| CommunityでのC++確認結果 | C++20のビルド・リンク・実行成功 |
| UEのバージョン・パス | ユーザー選択：5.8.4 / C:\Program Files\Epic Games\UE_5.8 |
| FutureBattle.uprojectの絶対パス | C:\Users\nanek\Documents\FPS\FutureBattle\Game\FutureBattle\FutureBattle.uproject |
| Development Editor / Win64ビルド | 成功。初回51.17秒、再確認1.97秒 |
| エディタ起動・Play | 成功。Wキー短押しの反応・Spaceジャンプ・Escape終了を確認 |
| 再起動の要否 | vswhere: isRebootRequired=false |
| Git初期コミット | ユーザー希望により名前・メール設定とともに後回し |

## 未決定事項

陣営・世界観の詳細、戦場寸法、兵科性能、最大同時プレイヤー数、ネットワークサービス、キャンペーン時間進行、A/B/Cの詳細UIは未決定です。20体試作に必要な範囲から決め、全体仕様の拡張を先行させないでください。

## 公式参照先

- [UE 5.7とVisual Studioの対応](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.7)
- [EpicのUEインストール手順](https://dev.epicgames.com/documentation/en-us/unreal-engine/install-unreal-engine)
- [EpicのHTTPモジュール資料](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/HTTP/FHttpModule)

## 2026-11-03 再開時の実機確認

- 現在の資料配置：C:\Users\nanek\Documents\FPS\FutureBattle。添付ZIPを展開。元の資料は変更していない。
- ユーザー指定によりUE 5.8.4を使用する。配置：C:\Program Files\Epic Games\UE_5.8。
- Build.version：5.8.4 / CL 58902052。エディタ実行ファイル、Build.bat、Third Personテンプレートの存在を確認。
- LauncherInstalled.datのInstallationListは空。Launcherによるインストール完全性の確認は未完了。
- UE 5.7は本体未確認（.egstoreのみ）。再インストールは実施していない。
- Visual Studio Community 2022 17.14.44：導入完了、isRebootRequired=false。
- Communityのx64開発環境でCheck-Compiler.cmdを実行し、C++20コンパイル、Windows APIリンク、実行成功（28論理プロセッサ）。
- Windows SDK：10.0.26100.0。Git：2.56.0.windows.4。
- Check-Environment.ps1のvswhere出力をUTF-8で解析するよう修正。-EngineRoot指定による環境確認成功。
- UEエディタを起動。WindowsファイアウォールのUnreal Insights用通信確認画面でユーザー操作待ち。
- FutureBattle.uproject作成、Development Editor / Win64ビルド、Play確認はまだ未実施。
- Gitのuser.name / user.emailは現在の作業場所で未設定。本人情報を推測せず、初期コミットは未実施。
- 検証ログ：Setup/Compiler-2026-11-03.log、Setup/Environment-2026-11-03.log（Git対象外）。


## 2026-11-04 00:10 JST 環境検証完了

- C++ Third Person / Desktop / MaximumでFutureBattleを作成。
- プロジェクト：C:\Users\nanek\Documents\FPS\FutureBattle\Game\FutureBattle\FutureBattle.uproject
- UE 5.8.4でDevelopment Editor / Win64ビルド成功。初回51.17秒。
- UEビルドが選択したツールチェーン：C:\BuildTools\VC\Tools\MSVC\14.44.35207（コンパイラ14.44.35229）とSDK 10.0.26100.0。Community単独のC++20検証も別途成功済み。
- Lvl_ThirdPersonのPlay起動成功。W短押しの反応、Spaceでのジャンプ、Escapeでの正常終了を画面で確認。長時間・性能・パッケージ化検証は未実施。
- Setup/Build-Editor.ps1で再ビルド確認成功（up to date、終了コード0）。ソース変更後の通常ビルドはエディタを閉じて実行する。
- 起動はSetup/Open-Editor.ps1を使用可能。EngineAssociationのGUIDがこのPCのUE_5.8に対応することを確認。他PCでは再関連付けが必要。
- 初回ビルドログ：Setup/Initial-Build-Editor.log。再確認ログ：Setup/Build-Editor.log。Playログ：Game/FutureBattle/Saved/Logs/FutureBattle.log。すべてGit対象外。
- Git mainは初期化済み。名前・メール設定とコミットはユーザーの「後で」に従い未実施。
- エディタはPlay停止後、マップを開いた状態で残している。
- 次工程：20体のFPS/RTS試作。永続UnitIDと正規データをActorから分離し、操作切替時にHP・弾薬・命令を保持する。今回は環境準備までで、ゲーム独自機能は未実装。


## 2026-11-04 Git初期保存

ユーザーからGit表示名とメールアドレスの指定を受領し、このリポジトリのローカル設定に反映。以前の保留は解除。検証済みC++ Third Personテンプレート・設定・素材・資料・構築スクリプトを初期コミットに保存する。生成物とローカルログは.gitignoreで除外。リモートへの送信は実施していない。


## 2026-11-04 20体試作の実装（手動Play確認待ち）

- UE標準機能を利用し、Source/FutureBattle/PrototypeにFPS×RTS試作を追加。
- 専用マップ：/Game/Prototype/Lvl_BattlePrototype。起動マップに設定。味方10体・敵10体。
- 正規データはUBattleStateに分離。操作切替時にUnitID、HP、弾倉、予備弾、命令、目的地、リロード時間を保持。
- FPS移動/照準/射撃/リロード、AI索敵射撃、NavMesh移動、俯瞰選択/移動/停止、味方への操作切替、未発見敵非表示、簡易戦闘終了を実装。
- UnitIDの永続性は一戦内。保存読込とキャンペーンをまたぐID保存は未実装。
- Development Editor / Win64ビルド成功。通常のDevelopment / Win64ゲームターゲットも成功（約54秒）。パッケージ化は未実施。
- Setup/Test-Battle.ps1の実ゲームワールド統合テスト：25項目PASS、failures=0。ログ：Setup/Battle-Smoke-20261104-003553.log。
- Map生成時のNavMesh不備を修正。コマンドレットでBrushを正しく作成し、生成後の座標投影を検証。マップ保存成功。
- エディタで専用マップを開いたところ、UnrealEditor本体のWindowsファイアウォール確認が表示された。ユーザーに操作を依頼済み。手動のPlay・選択・入力・HUDの見え方はまだ検証していない。
- 次の再開時：確認画面の処理完了を確認してPlay。PでAI停止→クリック/矩形選択→右クリック移動→EでFPS→射撃/リロード→Tabで戻る、別ユニットへ切替、F5のリセットを実画面で確認。必要に応じて修正し、再ビルドと統合テストを実施。
- 操作説明・制限・再検証手順はPROTOTYPE.md。

## 2026-11-04 00:56 JST 実画面検証・修正完了

- ユーザーがファイアウォール確認を処理。エディタPlayでFPS切替・射撃・弾薬保持を確認し、その後の独立ゲームウィンドウで一連の操作を検証。
- 実画面でUnit02クリック選択、味方10体の矩形選択、右クリックによる10体の移動と実移動、FPS切替、射撃、リロード、俯瞰復帰、F5リセットを確認。
- 実測例：Unit01のHP80・弾薬27/90・MOVE命令をFPSへ引き継ぎ、1発射撃後26/90、リロード後30/86となり、俯瞰へ戻ってもHP80とMOVE命令を保持。
- 仮想マウスではUEのIsMouseAttached判定がfalseになり、GetMousePositionが失敗する問題を修正。通常取得が失敗した場合はViewport内の有効な座標を使用。クリックの押下/解放はInputKeyで処理し、短いクリックにも対応。
- 初期カメラを味方部隊中心に変更。FPS射撃のトレーサー開始点を目の前からずらし、重複する巨大なワールド文字を非表示にした。
- F5とUE既定のshadercomplexity切替の重複をDefaultInput.iniで解除。F5で表示モードを変えず初期状態に戻ることを実画面で確認。
- 最終Development Editor / Win64とDevelopment / Win64ビルド成功。
- 最終統合テスト：Setup/Battle-Smoke-20261104-005512.log、25項目PASS、failures=0。
- Setup/Play-Prototype.ps1を追加。UE実行環境を使って独立した試作ゲームウィンドウを起動できる（配布用パッケージではない）。
- 現在は独立ゲームウィンドウを新しい戦闘の俯瞰モード・AI停止状態で残した。Pで再開可能。
