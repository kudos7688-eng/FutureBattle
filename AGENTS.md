# FutureBattle の継続開発指示

- 日本語で対応する。開始時に `STATUS.md` 冒頭の要約と直近の項目を読み、次の作業と既知の制限を確認する。計測・試験の手順は `MEASURE.md`。STATUS が約80KBを超えたら、前日より前の項目を `Docs/journal/` へ移す（消さない）。実装計画は `ROADMAP.md`、起動・操作・検証は `PROTOTYPE.md`、元の全体構想は `README.md`、過去の経緯は `HANDOFF.md`。コードのどこに何があるかは `CODEMAP.md`（機能 → 入口の関数とファイル。`Setup/Export-CodeMap.py` で自動生成、手書き部分は `Setup/CodeMap-Features.md`）で引いてから読む。
- 現在の依頼を優先する。「続きから進めて」などの実装依頼ならSTATUSの次の工程に進む。相談・計画・資料整理の依頼ではゲーム実装を勝手に開始しない。ロードマップ全体を一度に実装しない。
- UE 5.8.4を使用。プロジェクトは `Game/FutureBattle/FutureBattle.uproject`。環境構築・20体試作は完了済み。古い5.7.4指定や未導入の履歴に戻らない。
- UE標準機能・既存テンプレート資産を活用し、独自のゲームルールはC++中心に実装する。新しい外部資産・プラグインの導入時は出典・利用条件・必要性を記録する。
- UnitIDと正規状態を表示Actorから分離する。FPS/RTS切替でHP・弾薬・命令等を失わない。一戦内IDと保存をまたぐ永続IDを混同しない。
- AIは「命令」「認識情報」「判断」「行動実行」「戦術効果」を分離する。敵の真の状態を判断に直結させない。戦術効果は直接操作とAIで共通にし、AI能力・射撃技量・ゲーム内価格を別の調整項目にする。
- ゲーム内の賢さと実行負荷対策を分ける。高コスト兵のために全個体へ高頻度の重い探索を追加しない。
- 変更前に `git status --short -uno` を確認し（`-uno` なしだと計測結果の未追跡ファイルが数千行出る。未追跡は `git status --short -- Game/FutureBattle/Source Game/FutureBattle/Config` のように場所を絞って見る）、ユーザーの未コミット変更を保持する。試作マップの手編集を自動再生成の -Force で上書きしない。
- C++変更後は `Setup/Build-Editor.ps1`、挙動変更後は `Setup/Test-Battle.ps1` を実施し、必要な実画面確認を追加する。成功判定はログの `BATTLE_SMOKE_RESULT failures=0`。ビルド時のEditor DLLロックに注意し、未保存作業を失う強制終了は避ける。
- 作業の区切りごとに、別途依頼されなくてもSTATUSへ実装済み範囲・実施した検証・未解決事項・次の一手を更新する。方針が変わったらROADMAPを更新する。重要な経緯はHANDOFFへ追記し、古い完了待ち記述を最新状態と誤解させない。未実施の確認を完了と書かない。
- チャットの記憶だけに依存せず、ファイルに残した合意と実際のコード・ログを確認して継続する。
- T5以降に実施する各計測では、開始前に `Setup/Get-MeasurementMachine.ps1` の `Get-MeasurementMachine` で測定機情報を取得し、manifestの `machine` に保存する（JSON深さ5以上）。GPU名・VRAM（bytesと取得元）・ドライバ版、CPU名・物理コア数/論理プロセッサ数、物理メモリ容量（bytes）、OS名/版/ビルド/アーキテクチャを含める。複数GPU/CPUは全件保存し、実際の描画GPUはUEログと照合する。既存結果への後付けはしない。VRAM取得不能はnullと理由を残し、32bitのCIM AdapterRAM値を実容量と誤認しない。
