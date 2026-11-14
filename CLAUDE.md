# Claude 向け開発指示

共通ルールの正本は [AGENTS.md](AGENTS.md)。開始時に必ず読み、[STATUS.md](STATUS.md) の最新状況を確認する。実務手順は [CLAUDE_HANDOFF.md](CLAUDE_HANDOFF.md)、計画は [ROADMAP.md](ROADMAP.md)、操作は [PROTOTYPE.md](PROTOTYPE.md)。過去のチャットを読めることを前提にしない。

- 日本語で対応し、現在の依頼を優先する。資料整理だけの依頼から実装を始めない。
- UE 5.8.4 / Windows / C++。既存プロジェクトは `Game/FutureBattle/FutureBattle.uproject`。セットアップやマップ生成を最初からやり直さない。
- 変更前に `git status` と実行中のUE・計測プロセスを確認する。未コミット変更を削除・巻き戻し・一括整形しない。他の開発者/AIの測定中はSource/Config/DLL/マップを変更せず、追加のゲーム・ビルド・性能測定を重ねない。
- C++変更後は `Setup/Build-Editor.ps1`、挙動変更後は `Setup/Test-Battle.ps1` と該当する専用試験を実施する。表示変更には実画面確認も必要。未保存Editorや他作業のプロセスを強制終了しない。
- 正規状態/UnitIDと表示を分離し、AI判断には観測情報を使う。試験用の敵真値を判断へ戻さない。
- 作業区切りでSTATUSに変更範囲・検証・制限・次の一手を記録する。方針変更はROADMAP、重要な経緯はHANDOFFへ残す。未実施を合格と書かない。

この入口は共通文書を置き換えない。最新仕様・承認範囲は都度STATUS/ROADMAPと実コード・ログを照合する。
