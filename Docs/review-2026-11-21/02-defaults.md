# FutureBattle 総点検 02：既定値の守り

レビュー日：2026-11-21。対象HEAD：`f758d21`。前回 `01-ai-layers.md` の「6. 次回以降の点検／既定値の守り」を出発点に、STATUSの要約・直近項目、CODEMAP、SWITCHES、C++、Config、Saved/Config、SetupのPowerShellを静的に照合した。

**結論：既定OFFの新機能を一律に「無影響」と扱うことはできない。** とくに試験スクリプトの暗黙ON、群OFFでの既存群の運用、PlannerとGroupStrengthの結合、片側指定の適用範囲に注意が必要。既定ONとして採用済みの変更自体は不具合と数えない。比較の基準はこのHEADの採用済み既定であり、全スイッチを0にした状態でも、昔の任意のコミットでもない。

ビルド、UE起動、試合、試験、計測は実施していない。確信度「高」は条件と経路をコードで確認した意味であり、勝敗差を測定した意味ではない。ファイルの短縮名は原則 `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。`DefaultGame.ini` は `Game/FutureBattle/Config/` 配下。起動スクリプトの実行も生成器の実行も行っていない。開始時の `git status --short -uno` は追跡ファイル差分なし（個人git ignoreへのアクセス警告あり）。書き込みは本報告だけ。

## 1. 実効値を読む規則

1. UBattleTuningのC++初期値にiniを適用する。関数ローカルの固定値・構造体既定はiniの同名引数では変更されない。
2. `FParse::Value` が見つけた値を代入し、その後のClamp、`==1`、`!=0`、マップ条件、親機能条件、陣営条件を適用する。スクリプトはこの引数を作る側であり、iniの後に別の第四設定層があるわけではない。
3. `Param` は存在判定。`=0`を付ける方式をOFF契約にせず、引数を省略する。パス引数も`=0`をOFFと扱わない。
4. `-1`は多数の引数で「自動／設定値へ委譲」。`Test-WideSight`の多くの`-1`は**UEへ渡さない**というスクリプト側の値。0と同じではない。
5. キャッシュされた設定は初回参照時に確定する。多くはプロセス単位、Ranges/StancesはWorld単位、SquadTactics等はSubsystem初期化時。新しい試合を同じプロセス内で作っても全設定の再読込にはならない。

Configの5ファイルと実在するSaved/Configのiniを確認した。`[/Script/FutureBattle.BattleTuning]` のプロジェクト設定はDefaultGame.iniだけで、Saved側にその上書きは見つからなかった。表はこの作業ツリーの設定を対象とする。別プロジェクトコピー、ユーザーが後で加える`-ini:`、バイナリアセットの未検証の反射設定まで同じ実効値と保証するものではない。

### 主要起動条件の違い（確信度：高）

| 入口 | C++/iniからの追加・変更 | 根拠 |
|---|---|---|
| ゲーム引数を省略 | CoverActions=0、FlankCoordination=0、SectorAI=0。CoverPriority=1、ArmourAI=1、AreaGroups=1。エリア戦には別途BattleAreaが必要 | BattleLeaderRuntime.cpp:74、BattleTactics.cpp:192、BattleState.h:482,496 |
| Test-WideSightの通常既定 | SightRange=5000、RangeAware=1、SquadFlank=1、CoverActions=0、FlankCoordination=0、AICoverProbe=0、CoverApproach=0、LeaderGuard=0、FlankRelaxed=0、FlankTeams=3、Planner/Teams=0を明示 | Setup/Test-WideSight.ps1:2,32,43,80,105 |
| 同スクリプトのH/I＋Area | SectorAI=1を明示。未指定のCoverApproach/LeaderGuardを1へ変更 | Setup/Test-WideSight.ps1:33–41 |
| 同スクリプトのExperiment | TrialSeed=7、固定時間刻み/FPS=60。RoundTripSaveならVerifyMixedSaveも付く | Setup/Test-WideSight.ps1:2,111,113 |
| Play-Area | CoverActions=1、FlankCoordination=1、AreaPointSupply=1、FollowerAnimSkip=1。H/IではLeaderGuard=1、SectorMove=1。SectorAIは利用者のswitch指定時だけ1 | Setup/Play-Area.ps1:10–21 |
| Test-Area | マップA、AreaSmoke、DisplayLOD=1、CoverActions/FlankCoordination/Planner/Teams=0、FlankRelaxed=0、FlankTeams=3 | Setup/Test-Area.ps1:2,13–15 |
| Test-Battle | 旧Prototypeマップ、SmokeTest、SenseMode=Legacy。通常広域の既定比較を代用しない | Setup/Test-Battle.ps1:2,8–9、BattleBenchmark.cpp:28–34 |

`RangeProfile`が有効な都市ルールマップでは、旧SightRange/WeaponRange引数よりProfileが先に返る（BattleState.cpp:29–38）。従ってTest-WideSightのSightRange=5000を全マップ共通の実視界と解釈してはいけない。

## 2. 指摘（確信度高）

### D01：試験スクリプトの「未指定」がゲームの既定と一致しない

確信度：**高**。

- 根拠：Setup/Test-WideSight.ps1:2,32–43、Setup/Play-Area.ps1:10–21、BattleMatch.cpp:466、BattleTactics.cpp:174。
- 状況→結果：H/Iで`-Area -Experiment`だけを足して基準戦を取ると、ゲームでは既定0のSectorAIとLeaderGuardが1になる。通常の未指定CoverApproach=-1は区画マップで自動ONだが、スクリプトは条件により0または1を明示する。A/DではSightRangeもiniの10000から5000へ変わる。Play-Areaとの比較ではさらに遮蔽・連携・拠点補給等が違う。これを「同じ既定」と扱う比較は無効。
- 改善：ゲーム既定をそのまま渡す基準プリセットと操作確認用プリセットを名前で分け、解決済み値・マップ・起動引数・保存往復の有無を比較キーにする。
- 効果：基準条件の混在を防ぐ。危険度：低（記録・プリセット追加）、中（既存スクリプトの既定変更）。既定の試合の結果が変わるか：ゲーム自体は不変。既存スクリプトの値を変更した試験は変わる。

### D02：AreaGroups=0は既存群のAI運用を止めない

確信度：**高**。

- 根拠：BattleGroups.cpp:72–79,204–211,231,270–273,444–448、BattleMatch.cpp:428、BattleSave.cpp:223–237,285、BattleLeaderRuntime.h:172、BattleSquadTactics.h:65。
- 状況→結果：Plannerを使わないv3保存にAI群がある状態で、同じマップ・他設定のまま`BattleAreaGroups=0`で読み込む。群切替はv3の適合判定に含まれず、群が復元される。0が抑止するのは新規自動編成であり、その後のphase更新・GroupOrderはAreaGroupsを再検査しない。敵群はGroupAIRunが常にtrueを返すため群指令を継続する。新規の群なし試合では同じ問題は起きない。手動で作った委任群も0で運用されるが、こちらはプレイヤー群を独立機能とする意図がコメントにある。
- 改善：まずOFFを「自動編成のみ停止」と明記する。完全旧経路を求める比較では群あり保存の読込を拒否するか、別の試験切替でAI群の運用まで停止する。無条件解散は保存・手動群への影響を確認してから。
- 効果：OFF比較と再開時の意味が揃う。危険度：中。既定の試合の結果が変わるか：文書化・比較時の拒否のみなら変わらない。既存群の扱いを変えれば再開試合は変わる。

### D03：Planner単独ONでは敵車両の識別記憶が作られない

確信度：**高**。

- 根拠：BattleComposition.cpp:11–13、BattleCompositionRuntime.cpp:46,78–86,89–104、BattlePerception.cpp:132–136、BattlePerception.h:13、BattleGroups.cpp:474–477。前回F01を再確認。
- 状況→結果：Planner=1かつTeamsの対象bitを立てても、既定GroupStrength=0では新規観測のObservedSquadTypeが255のまま。PlannerのVehicleSeenMSは車両識別済み接触しか採用せず、対車両不足の評価が欠ける。GroupStrengthを1にする回避策は既定ONの群の戦力評価を両陣営で変更するのでPlannerだけの実験にはならない。Planner=1、Teams=0なら計画器の試合状態自体が初期化されない。
- 改善：観測兵種の取得を群評価と分離し、Planner対象だけに必要な観測出力を有効にする。観測情報の保存規則も合わせる。
- 効果：Planner単独の効果測定が可能。危険度：中。既定の試合の結果が変わるか：Planner対象限定・既定0の修正なら不変。GroupStrengthを既定化する解決は変わる。

### D04：片側指定は共通フィルタではなく、未対応の切替には効かない

確信度：**高**。

- 根拠：BattleTactics.cpp:102、BattleArea.cpp:330–344、BattleGroups.cpp:49–79,476、BattleArmourAI.cpp:31–40、BattleWeaponOrders.cpp:10,16、BattleMatch.cpp:440,455,466、BattleLeaderRuntime.cpp:1449,1505–1507。
- 状況→結果：`BattleAreaTrialTeam=1`とGroupStrength、SectorAI、WeaponOrderGate、ArmourSectorGate、CoverPriorityを併用しても、これらはTeamを読まないため両側へ適用される。ArmourAIには別のArmourAITeam、ContactCoverにはContactCoverTeam、PlannerにはbitmaskのPlannerTeamsが必要。AreaAttackWeight、AreaBaseShare、HireReserveもAreaIntent内で共通値をそのまま使用する。一方S4/S6や各Keepの非対象側はiniへ戻す処理が存在し、前回修正済みのS4/S6漏れを再発と報告する根拠はない。
- AreaGroupsにも注意：引数省略なら両側true、明示`=1`＋TrialTeam=1なら相手側false。採用済み既定値の「明示」と「省略」が片側指定下では同値でない（BattleGroups.cpp:74–79）。明示値を全て並べるmanifest再実行が意味を変え得る。
- 改善：切替ごとにglobal/team-index/team-maskを登録し、未対応の片側指定を試験側で検出する。新しい片側機能は陣営別解決値を明示して追加する。
- 効果：対象外陣営のルール変更を検出。危険度：記録は低、スコープ追加は中。既定の試合の結果が変わるか：記録は不変。スコープ修正は従来の片側試験結果を変えるが、引数なしを維持可能。

### D05：Suppression=0でもExoShieldSuppression=1なら制圧が働く

確信度：**高**。

- 根拠：BattleSuppression.cpp:36–54,64–65,91、BattleLeaderRuntime.cpp:1501、BattleSuppression.h:9–12。
- 状況→結果：一般制圧をOFFにして盾被弾制圧だけをONにすると、外骨格兵ではIsPinned/散らばり倍率/AddSuppressionが生きる。SuppressNearの全兵走査も実行する。一般制圧OFFは全制圧OFFではない。これはコメントにも記載された意図的依存であり、既定両方0の漏れではない。
- 改善：試験・解決値表示を「通常兵の制圧」「外骨格兵の制圧」に分ける。グローバル0を優先するよう勝手に仕様変更しない。
- 効果：OFF条件の誤読を防ぐ。危険度：低。既定の試合の結果が変わるか：文書化のみなら不変。

### D06：WeaponOrderGateとArmourSectorGateは別で、主砲は前者を通らない

確信度：**高**。

- 根拠：BattleLeaderRuntime.cpp:612,640,719,1068–1076、BattleArmourAI.cpp:58–83、BattleWeaponOrders.cpp:20–37、BattleTankGun.cpp:30,85,114、BattleArtillery.cpp:281,313、BattleExo.cpp:301。前回F04/F05を再確認。
- 状況→結果：SectorAIが作った区画制限付き候補でも、ArmourSectorGate=0の装甲再選択が区画外目標を選び直す。WeaponOrderGate=1にしてもその切替の代用にはならず、戦車主砲の目標選択・発射は命令許可判定自体を通らない。Move/Support等の制約が副武器・間接射撃と一致しない。
- 改善：主砲も既存WeaponOrderGateの試験経路へ接続する。通常候補と装甲候補の許可集合を共通化し、区画・兵種優先・発射許可の順を固定する。
- 効果：切替の適用範囲と命令の整合。危険度：中。既定の試合の結果が変わるか：既定0の枝内だけなら不変。既定ONのArmourAI本体を直接直すと車両参加戦は変わり得る。

### D07：CoverActionsがBoundingの退出処理より先に実行される

確信度：**高**。

- 根拠：BattleLeaderRuntime.cpp:868,898–900、BattleCoverAction.cpp:375–490。前回F03を再確認。
- 状況→結果：Boundingで移動側となった兵が遮蔽動作中なら、StepCoverActionのtrueでFollowを抜け、後段のCancelCoverActionへ届かない。Bounding=1だけの比較とCoverActions=1との組合せで、移動側が進む保証が変わる。
- 改善：退出するかを状態実行前に裁定。Bounding既定0の枝で変更を比較する。
- 効果：交互前進の停滞を抑制。危険度：中。既定の試合の結果が変わるか：Bounding=0を保つ修正なら不変。

### D08：CoverFire=0でも追加の射撃数マップを更新する

確信度：**高**。

- 根拠：BattleCoverAction.cpp:405–424,450、:45–49。
- 状況→結果：AIが覗きに到着するとCoverFire=0でもCoverPeekShots.Add、射撃成功時にFindOrAddして加算する。通常診断OFFでも実行する。0では回数による覗き終了は無効で1.2秒に戻るため、このマップ更新だけで勝敗が変わる経路は確認できない。ただし「OFFなら追加配列・キャッシュにも触れない」という強い契約には違反する。CoverPeekSightもAIPeekFire条件付きなので、CoverFire=0で同時に無効になる。
- 改善：回数制御ONまたは診断ONに限って記録する。ただしEndPeekDiagの削除・統計出力の期待値も揃える。
- 効果：OFF時の割当・ハッシュ更新削減。危険度：低〜中。既定の試合の結果が変わるか：論理結果を変える必要はない。ログや実時間は変わる可能性があり未測定。

### D09：StanceAnim=0だけでは旧縮小表示に戻らない

確信度：**高**。

- 根拠：BattleStance.cpp:36–38,54,73–78、BattleBodyAnim.cpp:57–69。
- 状況→結果：StanceAnim=0はCrouchClipやBlindFire::Clipを止めるが、BodyScaleの伏せはBodyAnim::Get(ProneIdle)を別に読む。BodyAnimは既定1で、姿勢遷移クリップもAnimEnabledを通らず取得する。対応アセットが存在する近景兵では伏せの全寸表示・遷移が残る。「=0 restores the squashed placeholder」というコメントは現在の全姿勢には成立しない。
- 改善：両アニメ切替の役割を明記し、旧表示を要求する比較プリセットは両方0とする。単一の親スイッチにする場合は新たな試験として扱う。
- 効果：表示比較の条件を揃える。危険度：文書化は低、状態機械変更は中。既定の試合の結果が変わるか：表示の説明だけなら不変。実際の描画時間・操作への影響は未確認。

### D10：SWITCHESの推定には誤値・誤検出がある

確信度：**高**。

- 根拠：Setup/Export-Switches.py:18–19,84–107,125–132、BattleTactics.cpp:73,81,86、BattleSquadTrial.h:126、BattleHUD.cpp:808,856。
- 状況→結果：生成器の「直前4行」検索と型を区別しないメンバー名検索が、実際の代入元を取り落とす。AreaFocusの記載0.fは実際ini=1、AreaFrontBalanceの記載falseは実際ini=True。Area*Keepの「0 / -1」は実効値ではなく異なる構造体の混合（引数入力は-1→設定0）。FieldSeedの「1 / 0 / 7」は当該試験のSeed=7へ絞る必要がある。SectorViewShotの`Center({X,Y})`は出力パスFStringの初期値ではなく誤推定で、実際は空。
- `BattleConcealment`/`BattleTerrain`はActorタグ、`BattleSightTraces`/`BattleFollowSightTraces`はParallelForの表示名。起動引数ではない（BattleLeaderRuntime.cpp:433,1148,841、BattleTerrain.h:18、BattlePerception.cpp:272）。よって280行のうち4行は切替ではない。
- `BattleBlindFire`/Exposeの既定1や、RangeProfile等の空欄は誤値とは別に「未解決」。AreaGroups=-1も入力初期値としては正しいが実効ONを示さない。BlindFireClipTop/Sideは文字列定数、Supply/Heavy等のヘルパー経由も実値を補う必要がある。
- 改善：FParse到達を確かめた登録表を作り、型・マップ・陣営・iniキー・実行条件を持たせる。抽出不能は推定値で埋めず未解決とする。SWITCHESと起動時の解決済み値を別資料にする。
- 効果：全件表の誤読防止。危険度：低。既定の試合の結果が変わるか：生成器・記録のみなら不変。

### D11：OFFでも共通保存拡張と情報キャッシュは残る（採用済み仕様）

確信度：**高**。

- 根拠：BattleSave.cpp:139–166、BattleSave.h:100–106、BattleAreaIntel.cpp:19–51、BattleGroups.cpp:291、BattleArmourAI.cpp:107–113、STATUS.md「2026-11-21 2時20分」。
- 状況→結果：新しい砲撃試験4個・GroupStrength・ExoShieldSuppressionが全部0でも、混成保存はCombatStateVersion=3を書き、trial flagsと群等を保存する。AreaIntel=0でも既定ON群・砲兵からAreaIntelViewを呼べばOwnAt/EnemyAt/Enemies/EnemyKeysが構築され、車両がいればKnownVehiclesも更新される。AreaIntelの0は情報集約全体を止める契約ではない。ArmourAI=0でも車両の歩兵姿勢・遮蔽除外が残る（BattleLeaderRuntime.cpp:880,907,948）。
- この保存拡張・車両除外はSTATUSに「既定で効く修正」と明記される。未承認の新たな既定漏れと混同しない。Planner OFFは追加v4を選ばないが、UPROPERTY宣言そのものは常在するので「旧保存とバイト列が同一」とは言えない（バイナリ同一性は未検証）。
- 改善：ゲーム結果同一、保存論理互換、保存バイト同一、処理コスト同一を別の受入条件にする。AreaIntel集約を切替の外にある共通機能として文書化する。
- 効果：既定基準と過去互換の混同を防ぐ。危険度：低。既定の試合の結果が変わるか：文書化なら不変。集約を無条件に止める「修正」は群・兵種AIを変える。

## 3. 条件付きの懸念と否定できた疑い

### R01【中：影響の大きさは未測定】片側ContactCoverが反対側の探索時刻に干渉

根拠：BattleLeaderRuntime.cpp:917–929,948–952。ContactCoverTeam自体は全利用箇所で陣営判定されている。だがON側はフレーム予算を使いNextCoverProbeを進めず、OFF側は同じRuntimeのNextCoverProbeを使う。従ってON側が従来消費した共通枠が空き、OFF側の探索機会も変わり得る。これはスイッチの陣営判定漏れではなく共有資源を通じた直接干渉で、被害・移動による通常の対戦相互作用とは区別すべき。CoverPriorityの4枠も両側共通。

改善：試験時に陣営別予算消費を記録し、隔離実験が必要なら新しい既定0切替で予算を陣営別にする。効果：片側試験の解釈改善。危険度：中。既定の試合の結果が変わるか：記録のみは不変、予算分割を既定適用すれば変わる。

### R02【中：将来の同一プロセス試験への懸念】static設定と動的設定が混在

根拠：BattleTactics.cpp:29,52,70、BattleComposition.cpp:8–19、BattleRanges.cpp:8–14、BattleStance.cpp:20–24、BattleSquadTactics.cpp:14–24。Settingsは毎回読むがDefinitionsはstatic。Tuningを後から変えるとRulesKeyは現在のプロパティ値、Definitionsは初回の費用・上限となり得る（BattleCompositionRuntime.cpp:17–23,40）。新しいWorldでもプロセスstaticは残る。

ただしSource内のFCommandLine::Set/Append、ReloadConfigは見つからない。既存スモークのSuppression/WarScore切替はMutableTuningの実体を保存・変更・復帰しており、static constを再読込できると仮定してはいない（BattleCoverActionSmoke.cpp:131、BattleAreaSmoke.cpp:1102）。AimSwitchBaseSeconds、TankSightReuse、ExoShieldの変更も直接読む設定を対象とする。**現行試験がこの理由で誤動作すると確定した例はない。**

改善：A/Bは別プロセス、あるいはWorld所有の解決済み設定へ統一。効果：試験順依存の防止。危険度：別プロセス規約は低、設定寿命の改修は中。既定の試合の結果が変わるか：規約のみ不変、初回値・初期化順を変える改修は要比較。

### OFFの乱数・順序について確認できた範囲

- PlannerのOFFフックはInitializeCompositionの早期return、雇用側はCompositionTeams bit判定より後にだけDecideへ入る（BattleCompositionRuntime.cpp:46、BattleArea.cpp:145）。OFFで候補配列の生成・ソート・乱数消費をする通常試合経路は確認しなかった。AreaSmoke/CompositionSmokeがOFFでもDefinitionsを作るのは明示試験用の別経路。
- 射撃はUnitIDとShotSequenceとTrialSaltからローカルFRandomStreamを作る（BattleTactics.cpp:207–210）。砲撃はIDとA.Shotsからローカル生成（BattleArtillery.cpp:323–324）。これらのOFF分岐から共通乱数ストリームを余分に進める箇所は確認しなかった。別の射撃回数・対象になる間接影響は残るので「乱数消費なし＝試合同一」ではない。
- DangerRouteの被弾キャッシュ追加はDangerRouteOnでガードされる（BattleLeaderRuntime.cpp:1502）。ShotDirection=0でもShotFromが増えるのは既定ON CoverPriorityとのOR条件（:1503）であり、ShotDirectionだけの漏れではない。CoverPriority=0でもShotDirection/UnderFireCoverが1なら残る。
- CoverActions=0はRequestCoverActionの入口とFollowの探索を止める（BattleCoverAction.cpp:81、BattleLeaderRuntime.cpp:948）。ただしCoverArrival=1による到着地点選択、姿勢、既定CoverPriorityの被弾履歴までは止めない。これらを同じ「遮蔽OFF」と呼ばない。

## 4. 既定ONを0に戻した場合の判定

「局所復帰」は該当機能の旧分岐へ戻ること。過去コミット全体・同一seed結果の保証ではない。他の既定ON、マップ仕様、保存された状態は残る。

| 切替（Battle接頭辞省略） | =0の意味と復帰範囲 | 根拠／確信度 |
|---|---|---|
| CoverPriority | 優先枠・短縮retry・失敗記憶の追加を停止。ShotFromは他の試験ONなら残る。局所復帰 | BattleLeaderRuntime.cpp:925,928,951,983,1503／高 |
| ArmourAI | 武器別再選択等を停止。車両の姿勢・遮蔽除外、主砲・間接射撃は残る。完全な旧車両挙動ではない | BattleLeaderRuntime.cpp:880–882,948,1068、BattleArmourAI.cpp:31／高 |
| AreaGroups | 新規自動編成停止。既存・手動群は残る（D02） | BattleGroups.cpp:231,273／高 |
| ReacquireGrace | 再認識猶予と目標保持の特例を停止 | BattleTactics.cpp:143、BattlePerception.cpp:119、BattleLeaderRuntime.cpp:620／高 |
| CoverFire | 1.2秒へ戻る。回数マップは更新継続、CoverPeekSightも実質停止 | BattleCoverAction.cpp:405–450／高 |
| CoverValidation | 射程による遮蔽取消・覗き再検証を停止。物理遮蔽成立検査は残る | BattleCoverAction.cpp:393–401、BattleLeaderRuntime.cpp:937／高 |
| CoverPeekSight | 自身の再目視がないときの早期終了だけ停止 | BattleCoverAction.cpp:411／高 |
| AreaHire / AreaIncome | 自動雇用／周期収入を停止。手動購入、初期資源、補充ルールをすべて停止するものではない | BattleArea.cpp:136–145,206、BattleTactics.cpp:55,119／高 |
| AreaStrategy | 通常配分を旧側へ。自動群編成も抑止、独立の道路や兵種処理は残る | BattleMatch.cpp:281,348、BattleGroups.cpp:218,231／高 |
| AreaFrontBalance / AreaFocus | 対象側のみS4/S6停止。片側指定時の非対象はini既定1を維持 | BattleArea.cpp:330–332／高 |
| AreaBaseFlank / BaseFlankSide / BaseReturnLiving | 単独0では三点セットの旧直行には戻らない。ReturnLiving=0は共通補給閾値へ戻す | BattleTactics.h:94–99、BattleArea.cpp:344／高 |
| ScoutRetreat | 偵察の自動退避を停止。観測・報告は残る | BattleScoutReport.cpp:20／高 |
| SightParallel | 直列走査へ。PairReuse=1またはReuseSightContext=0でも並列は使わない | BattlePerception.cpp:158–160,230–302／高 |
| ReuseSightContext / PerceptionGrid / ReduceHiddenBones | 各最適化の旧枝へ。ReuseSightContext=0はSightParallelの有効性も変える | BattleLeaderRuntime.cpp:98–107、BattlePerception.cpp:160／高 |
| StableMovement / ContactAvoidance / RangeAware / DisplayLOD | 各通常混成の旧移動／接触／射程選択／非VAT枝へ。別機能の0の代用にはならない | BattleLeaderRuntime.cpp:81–84,121–127,991／高 |
| SquadTactics / SquadST2 / SquadFlank / TeamSupport | 各層の有効性を制御。TeamSupport=0でもFlankがONならTeamAI::Stepの迂回割当は動く | BattleSquadTactics.cpp:17–24、BattleTeamAI.cpp:20,56,95／高 |
| CoverArrival / SectorSlotReserve | 到着時の遮蔽候補／既存兵位置予約を停止。区画そのものは通常グリッドと別 | BattleStableMovement.cpp:79–87,131–139／高 |
| StanceAnim / BodyAnim | 独立。StanceAnimだけ0では旧縮小表示に戻らない（D09） | BattleStance.cpp:54,73、BattleBodyAnim.cpp:59／高 |
| BlindFire / BlindFireExpose | 前者0はプレイヤーのブラインド射撃を停止、AI切替は独立。後者0は露出だけ戻し、射撃を停止しない | BattleBlindFire.cpp:26–27,77–90／高 |
| AimAssist / HitDirection / KillLog / OffscreenMarkers | 補助／描画を停止。被弾・撃破の正規記録は別 | BattleController.cpp:1107、BattleHUD.cpp:657,695,762、BattleState.cpp:465,472／高 |
| TerrainLineBodies | 地形線判定の動的body扱いを旧条件へ。地形自体を無効にしない | BattleTactics.cpp:231／高 |
| Routes（自動ON） | 道路中間目標を停止。区画AI・手動命令の一括OFFではない | BattleRoutes.cpp:17–18、BattleMatch.cpp:431,467／高 |
| Stances / RangeProfile / SymmetricStart（都市ルールで自動ON） | 各マップの姿勢／射程profile／対称開始を停止。開始配置は新規試合で比較する | BattleStance.cpp:23–25、BattleRanges.cpp:14–21、BattleGroups.cpp:58–60／高 |
| CoverApproach（区画マップで自動ON） | 単独の接近許可を停止するだけ。ContactCover/区画接触/CoverStop/CoverStrict/SectorCoverWalk/UnderFireCoverで接近するOR経路が残る | BattleLeaderRuntime.cpp:965／高 |

数値は0=旧挙動とは限らない。LeaderHealthは0がClampで1、RifleMembersは0が2、SightScanIntervalは0が0.2になる。RifleMembers=10でもAreaSquadCost=12は戻らない（DefaultGame.ini:102–104）。HireSightRefreshの既定は0で、旧一斉更新は1。CorpseLifetime=0は死体を残す指定。これらはboolのOFF試験と分ける。

## 5. 全280項目の実効値台帳

以下はSWITCHESの全行を一対一で収録し、C++初期値→ini→起動引数→スクリプトの順に追える表である。4件の非引数も削らず区別する。数値は明示した分岐・モードが成立したときの値で、対象機能が起動していない場合は値があっても無作用。脚注の「起動箇所なし」はSetup内に当該Battle名の直接指定を発見しなかった意味であり、外部利用者が渡せないという意味ではない。

Setup欄は送信トークンと所在を示す。`$変数[既定…]`はPowerShellパラメータ初期値で、分岐が成立したときだけ送る（条件は引用行と主要起動条件表参照）。パスやループ変数を固定数値とはしない。動的なHeavy接頭辞連結も別途補う。Legacy内は現行通常試合の既定に混ぜない。スクリプトによる追加の実行条件・パスを隠さないため、詳細な送信式を別表に付ける。

| 切替 | C++初期値・解決元 | ini上書き | 引数なし実効値／明示引数の条件 | Setupの送信式参照 | C++読取位置 |
|---|---|---|---|---|---|
| `-BattleAIBlindFire` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleBlindFire.cpp:27 |
| `-BattleAICoverProbe` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S129](#s129) | BattleTactics.cpp:10 |
| `-BattleAimAssist` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleController.cpp:1107 |
| `-BattleAllyPass` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S178](#s178) | BattleStableMovement.cpp:31 |
| `-BattleArea` | 省略OFF（SightFieldでのみArea有効） | なし | Param（存在）：省略OFF／Param存在でON。ただしSightField限定 | [S037](#s037)、[S042](#s042)、[S055](#s055)、[S064](#s064)、[S183](#s183) | BattleLeaderRuntime.cpp:76, BattleState.cpp:564 |
| `-BattleAreaACKeep` | 引数初期-1；AreaACKeep=0（BattleState.h:349） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S175](#s175) | BattleTactics.cpp:97 |
| `-BattleAreaATKeep` | 引数初期-1；AreaATKeep=0（BattleState.h:339） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S175](#s175) | BattleTactics.cpp:95 |
| `-BattleAreaAttackWeight` | AreaAttackWeight=2.f（BattleState.h:672） | AreaAttackWeight=2.0（DefaultGame.ini:125） | Value（=値）：2.0；指定時は読取値（後処理・モード条件あり） | [S165](#s165) | BattleTactics.cpp:75 |
| `-BattleAreaBaseFlank` | 1.3f | なし | Value（=値）：1.3f；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:88 |
| `-BattleAreaBaseFlankSide` | 1（FAreaStrategyTuning） | なし | Value（=値）：1（片側集中）。非対象側もFAreaIntentの採用済み既定値を保持 | [S167](#s167) | BattleTactics.cpp:90 |
| `-BattleAreaBaseInfiltrate` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:84 |
| `-BattleAreaBaseLowHP` | 500.f | なし | Value（=値）：500.f；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleTactics.cpp:80 |
| `-BattleAreaBaseReturnLiving` | 5 | なし | Value（=値）：5。0は共通SupplyReturnLiving（既定8）へ委譲 | [S167](#s167) | BattleTactics.cpp:91 |
| `-BattleAreaBaseRule` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S166](#s166) | BattleTactics.cpp:77 |
| `-BattleAreaBaseShare` | AreaBaseShare=0.2f（BattleState.h:673） | AreaBaseShare=0.2（DefaultGame.ini:126） | Value（=値）：0.2；指定時は読取値（後処理・モード条件あり） | [S165](#s165) | BattleTactics.cpp:76 |
| `-BattleAreaBaseShareHigh` | 0.3f | なし | Value（=値）：0.3f；指定時は読取値（後処理・モード条件あり） | [S166](#s166) | BattleTactics.cpp:79 |
| `-BattleAreaBaseShareLow` | 0.f | なし | Value（=値）：0.f；指定時は読取値（後処理・モード条件あり） | [S166](#s166) | BattleTactics.cpp:78 |
| `-BattleAreaComposition` | bAreaComposition=false（BattleState.h:490） | なし | Value（=値）：0。AreaTrialTeamに従う。Plannerを有効にするスイッチとは別 | [S175](#s175) | BattleArmourAI.cpp:38 |
| `-BattleAreaCompositionPlanner` | 0 | なし | Value（=値）：0。1のみON、Teams対象bitとAreaHire ONが必要。team0購入はExperiment限定 | [S043](#s043)、[S162](#s162) | BattleComposition.cpp:11 |
| `-BattleAreaCompositionPlannerTeams` | 0 | なし | Value（=値）：0；0=対象なし、1=team0、2=team1、3=両方 | [S043](#s043)、[S162](#s162) | BattleComposition.cpp:12 |
| `-BattleAreaDepot` | 0 | なし | Value（=値）：0。値==1かつエリア・対象陣営・拠点価値定義が必要 | [S142](#s142) | BattleAreaDepot.cpp:22 |
| `-BattleAreaDepotRaid` | 2（DepotRaid定数） | なし | Value（=値）：2（DepotRaid定数）；指定時は読取値（後処理・モード条件あり） | [S142](#s142) | BattleAreaDepot.cpp:23 |
| `-BattleAreaExoKeep` | 引数初期-1；AreaExoKeep=0（BattleState.h:372） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S175](#s175) | BattleTactics.cpp:101 |
| `-BattleAreaFocus` | AreaFocusGain=1.f（BattleState.h:678） | AreaFocusGain=1.0（DefaultGame.ini:131） | Value（=値）：1.0；非対象側はiniへ戻す | [S167](#s167) | BattleTactics.cpp:86 |
| `-BattleAreaFrontBalance` | bAreaFrontBalance=true（BattleState.h:676） | bAreaFrontBalance=True（DefaultGame.ini:129） | Value（=値）：1；非対象側はiniへ戻す | [S167](#s167) | BattleTactics.cpp:81 |
| `-BattleAreaFrontGain` | AreaFrontGain=1.f（BattleState.h:677） | AreaFrontGain=1.0（DefaultGame.ini:130） | Value（=値）：1.0；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:82 |
| `-BattleAreaGroups` | 引数初期-1；bAreaGroups=true（BattleState.h:496） | なし | Value（=値）：1；省略なら設定1を両側。明示値はAreaTrialTeamで限定（D04） | [S175](#s175) | BattleGroups.cpp:76 |
| `-BattleAreaHeavyShare` | 引数初期-1；AreaHeavyShare=.33f（BattleState.h:331） | なし | Value（=値）：.33f；引数初期-1→設定.33、片側指定の非対象も設定値 | [S167](#s167) | BattleTactics.cpp:93 |
| `-BattleAreaHire` | bAreaHire=true（BattleState.h:649） | bAreaHire=True（DefaultGame.ini:103） | Value（=値）：1。自動購入のみ、team0はExperiment限定 | [S163](#s163) | BattleTactics.cpp:55 |
| `-BattleAreaHireInterval` | AreaHireIntervalSeconds=0.f（BattleState.h:657） | なし | Value（=値）：0.f；指定時は読取値（後処理・モード条件あり） | [S168](#s168) | BattleTactics.cpp:57 |
| `-BattleAreaHirePurpose` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:60 |
| `-BattleAreaHirePurposeFront` | 10 | なし | Value（=値）：10；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleTactics.cpp:61 |
| `-BattleAreaHireReserve` | AreaHireReserve=0（BattleState.h:651） | AreaHireReserve=0（DefaultGame.ini:106） | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S168](#s168) | BattleTactics.cpp:57 |
| `-BattleAreaHireStop` | AreaHireStopSeconds=0.f（BattleState.h:658） | なし | Value（=値）：0.f；指定時は読取値（後処理・モード条件あり） | [S168](#s168) | BattleTactics.cpp:58 |
| `-BattleAreaIncome` | bAreaIncome=true（BattleState.h:661） | bAreaIncome=True（DefaultGame.ini:113） | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S164](#s164) | BattleTactics.cpp:119 |
| `-BattleAreaIncomeBase` | AreaBaseIncome=1（BattleState.h:663） | AreaBaseIncome=4（DefaultGame.ini:117） | Value（=値）：4；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:122 |
| `-BattleAreaIncomePerPoint` | AreaPointIncome=1（BattleState.h:664） | AreaPointIncome=4（DefaultGame.ini:118） | Value（=値）：4；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:122 |
| `-BattleAreaIncomeRamp` | 0.f | なし | Value（=値）：0.f；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:127 |
| `-BattleAreaIncomeUrbanBase` | AreaBaseIncomeUrban=-1（BattleState.h:667） | AreaBaseIncomeUrban=-1（DefaultGame.ini:120） | Value（=値）：-1→共通Base=4（都市ルールでも同額）。明示0以上で都市額だけ置換 | [S167](#s167) | BattleTactics.cpp:125 |
| `-BattleAreaIncomeUrbanPerPoint` | AreaPointIncomeUrban=-1（BattleState.h:668） | AreaPointIncomeUrban=-1（DefaultGame.ini:121） | Value（=値）：-1→共通PerPoint=4（都市ルールでも同額）。明示0以上で都市額だけ置換 | [S167](#s167) | BattleTactics.cpp:125 |
| `-BattleAreaIntel` | bAreaIntel=false（BattleState.h:555） | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S175](#s175) | BattleAreaIntel.cpp:14 |
| `-BattleAreaMaxSquads` | AreaMaxActiveSquads=25（BattleState.h:654） | AreaMaxActiveSquads=25（DefaultGame.ini:109） | Value（=値）：25；指定時は読取値（後処理・モード条件あり） | [S168](#s168) | BattleTactics.cpp:59 |
| `-BattleAreaMortarKeep` | 引数初期-1；AreaMortarKeep=0（BattleState.h:425） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S175](#s175) | BattleTactics.cpp:98 |
| `-BattleAreaPlan` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:92 |
| `-BattleAreaPointSupply` | bAreaPointSupply=false（BattleState.h:585） | bAreaPointSupply=False（DefaultGame.ini:36） | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S037](#s037)、[S142](#s142) | BattleTactics.cpp:39 |
| `-BattleAreaScoutKeep` | 引数初期-1；AreaScoutKeep=0（BattleState.h:534） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S174](#s174) | BattleTactics.cpp:94 |
| `-BattleAreaSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S042](#s042) | BattleArea.cpp:45, BattleMixedBodies.cpp:117 |
| `-BattleAreaSPGKeep` | 引数初期-1；AreaSPGKeep=0（BattleState.h:442） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S175](#s175) | BattleTactics.cpp:99 |
| `-BattleAreaStartCredits` | 100 | なし | Value（=値）：100；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:128 |
| `-BattleAreaStrategy` | bAreaStrategy=true（BattleState.h:671） | bAreaStrategy=True（DefaultGame.ini:124） | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S165](#s165) | BattleTactics.cpp:74 |
| `-BattleAreaTankKeep` | 引数初期-1；AreaTankKeep=0（BattleState.h:392） | なし | Value（=値）：0；引数初期-1→設定0。明示0は維持数0、非対象側は設定値 | [S175](#s175) | BattleTactics.cpp:96 |
| `-BattleAreaTrialTeam` | -1 | なし | Value（=値）：-1=対象制限なし、0/1=陣営index。対応切替だけに作用 | [S167](#s167) | BattleTactics.cpp:102 |
| `-BattleAreaWarPointSeconds` | 5秒 | なし | Value（=値）：5秒；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleWarScore.cpp:11 |
| `-BattleAreaWarResourceValue` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleWarScore.cpp:11 |
| `-BattleAreaWarScore` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S169](#s169) | BattleWarScore.cpp:10 |
| `-BattleAreaWarTarget` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S169](#s169) | BattleWarScore.cpp:12 |
| `-BattleArmourAI` | bArmourAI=true（BattleState.h:482） | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S175](#s175) | BattleArmourAI.cpp:31 |
| `-BattleArmourAITeam` | -1 | なし | Value（=値）：-1；-1=両方、0/1=陣営index（maskではない） | [S175](#s175) | BattleArmourAI.cpp:32 |
| `-BattleArmourSectorGate` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleWeaponOrders.cpp:16 |
| `-BattleArrivalThreat` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S152](#s152) | BattleLeaderRuntime.cpp:1474 |
| `-BattleArtilleryClusters` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleArtillery.cpp:41 |
| `-BattleArtilleryObserved` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleArtillery.cpp:23 |
| `-BattleBenchmark` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S001](#s001) | BattleBenchmark.cpp:28, BattleController.cpp:53, BattleLeaderRuntime.cpp:72 |
| `-BattleBenchmarkFPS` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S007](#s007) | BattleBenchmark.cpp:120, BattleBenchmark.cpp:154 |
| `-BattleBlastOcclusion` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleArtillery.cpp:35 |
| `-BattleBlindFire` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleBlindFire.cpp:26 |
| `-BattleBlindFireClipSide` | TEXT("AS_CoverLo_AimR") | なし | Value（=値）：TEXT("AS_CoverLo_AimR")；アセット名。未指定AS_CoverLo_AimR | 直接起動箇所なし | BattleBlindFire.cpp:44 |
| `-BattleBlindFireClipTop` | TEXT("AS_CoverLo_AimU_down") | なし | Value（=値）：TEXT("AS_CoverLo_AimU_down")；アセット名。未指定AS_CoverLo_AimU_down | 直接起動箇所なし | BattleBlindFire.cpp:44 |
| `-BattleBlindFireExpose` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleBlindFire.cpp:77 |
| `-BattleBlindFireSpread` | 4.f | なし | Value（=値）：4.f；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleBlindFire.cpp:30 |
| `-BattleBodyAnim` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleBodyAnim.cpp:59 |
| `-BattleBounding` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S150](#s150) | BattleLeaderRuntime.cpp:1439 |
| `-BattleCloseIn` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S157](#s157) | BattleLeaderRuntime.cpp:1464 |
| `-BattleConcealment` | Actorタグ | 対象外 | 起動引数ではない。指定しても当該機能は切り替わらない | 直接起動箇所なし | BattleLeaderRuntime.cpp:1148, BattleLeaderRuntime.cpp:433, BattleMapCommandlet.cpp:116, BattleMapStats.cpp:91 |
| `-BattleContactAvoidance` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S050](#s050) | BattleLeaderRuntime.cpp:84 |
| `-BattleContactCover` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S137](#s137) | BattleTactics.cpp:186 |
| `-BattleContactCoverTeam` | -1 | なし | Value（=値）：-1=両方、0/1=陣営index。ContactCover=0では無作用 | [S137](#s137) | BattleTactics.cpp:180 |
| `-BattleContactTrial` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S050](#s050) | BattleLeaderRuntime.cpp:85 |
| `-BattleCorpseLifetime` | 20.f | なし | Value（=値）：20.f；指定時は読取値（後処理・モード条件あり） | [S179](#s179) | BattleCorpses.cpp:25 |
| `-BattleCoverActions` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S037](#s037)、[S044](#s044)、[S052](#s052)、[S068](#s068)、[S129](#s129) | BattleSquadTactics.cpp:19, BattleTactics.cpp:192 |
| `-BattleCoverActionSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S052](#s052) | BattleLeaderRuntime.cpp:212 |
| `-BattleCoverApproach` | -1→区画マップでON | なし | Value（=値）：-1→区画マップでON；0でもContactCover等のOR条件で接近（4節） | [S129](#s129) | BattleTactics.cpp:174 |
| `-BattleCoverArrival` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S172](#s172) | BattleStableMovement.cpp:131 |
| `-BattleCoverFire` | bAICoverFire=true（BattleState.h:617） | bAICoverFire=True（DefaultGame.ini:72） | Value（=値）：1；0で1.2秒、回数マップ更新は残る | [S135](#s135) | BattleTactics.cpp:150 |
| `-BattleCoverHuddle` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S148](#s148) | BattleStableMovement.cpp:166 |
| `-BattleCoverPeekSight` | bAICoverPeekSight=true（BattleState.h:625） | bAICoverPeekSight=True（DefaultGame.ini:80） | Value（=値）：1；CoverFire ON時のみ | [S139](#s139) | BattleTactics.cpp:162 |
| `-BattleCoverPriority` | 1 | なし | Value（=値）：1。CoverActions=0では探索しないが被弾履歴は記録 | [S036](#s036)、[S161](#s161) | BattleLeaderRuntime.cpp:1449 |
| `-BattleCoverStrict` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S154](#s154) | BattleLeaderRuntime.cpp:1479 |
| `-BattleCoverValidation` | bAICoverValidation=true（BattleState.h:622） | bAICoverValidation=True（DefaultGame.ini:77） | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S138](#s138) | BattleTactics.cpp:156 |
| `-BattleDangerRoute` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S156](#s156) | BattleSectorRoute.cpp:28 |
| `-BattleDefendEngage` | bDefendEngage=false（BattleState.h:521） | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S175](#s175) | BattleGroups.cpp:49 |
| `-BattleDelegationScreens` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S054](#s054) | BattleDelegationSmoke.cpp:90 |
| `-BattleDiagnosticOmit` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S002](#s002) | BattleBenchmark.cpp:114, BattleBenchmark.cpp:146 |
| `-BattleDisplayLOD` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S031](#s031)、[S038](#s038)、[S042](#s042)、[S065](#s065)、[S067](#s067)、[S111](#s111)、[S118](#s118)、[S119](#s119) | BattleLeaderRuntime.cpp:121, BattleWideMixedSmoke.cpp:170 |
| `-BattleEngineMoveSweep` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleStableMovement.cpp:485 |
| `-BattleExoPlaceholder` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | 直接起動箇所なし | BattleExo.cpp:108 |
| `-BattleExoShieldSuppression` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleSuppression.cpp:81 |
| `-BattleExoShot` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S055](#s055) | BattleLeaderRuntime.cpp:222 |
| `-BattleFastMatch` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S039](#s039)、[S056](#s056) | BattleGameMode.cpp:72, BattleLeaderRuntime.cpp:72 |
| `-BattleFieldDiagnostics` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S016](#s016)、[S025](#s025)、[S027](#s027)、[S028](#s028) | BattleSquadTrial.cpp:17 |
| `-BattleFieldRecord` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S025](#s025) | BattleSquadTrial.cpp:15 |
| `-BattleFieldReplay` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S026](#s026)、[S029](#s029)、[S030](#s030) | BattleSquadTrial.cpp:16 |
| `-BattleFieldSeed` | 7（BattleSquadTrial.h:126） | なし | Value（=値）：7（BattleSquadTrial.h:126）；省略時bField=false。Seed=7でもfield試験の開始指定にはならない | [S014](#s014)、[S025](#s025)、[S026](#s026)、[S027](#s027)、[S028](#s028)、[S029](#s029)、[S030](#s030) | BattleSquadTrial.cpp:19 |
| `-BattleFirstPerson` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S187](#s187) | BattleUnit.cpp:37 |
| `-BattleFlankBayTrial` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S057](#s057) | BattleLeaderRuntime.cpp:214 |
| `-BattleFlankCoordination` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S037](#s037)、[S044](#s044)、[S068](#s068)、[S129](#s129) | BattleSquadTactics.cpp:20 |
| `-BattleFlankRelaxed` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S044](#s044)、[S068](#s068)、[S180](#s180) | BattleSquadTactics.cpp:23 |
| `-BattleFlankSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S089](#s089) | BattleLeaderRuntime.cpp:240 |
| `-BattleFlankTeams` | 3 | なし | Value（=値）：3；bitmask 0..3、既定3 | [S044](#s044)、[S068](#s068)、[S180](#s180) | BattleSquadTactics.cpp:24 |
| `-BattleFlankTrial` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S181](#s181) | BattleSightExperiment.cpp:37, BattleSquadFlank.cpp:128, BattleSquadRetreat.cpp:18 |
| `-BattleFollowerAnimBudget` | 0 | なし | Value（=値）：0。AnimSkip等の親条件はBattleVATDisplay.cpp参照 | [S198](#s198) | BattleVATDisplay.cpp:48 |
| `-BattleFollowerAnimSkip` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S037](#s037)、[S197](#s197) | BattleVATDisplay.cpp:53 |
| `-BattleFollowerNoOverlap` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S200](#s200) | BattleLeaderRuntime.cpp:814 |
| `-BattleFollowSightParallel` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S199](#s199) | BattleLeaderRuntime.cpp:807 |
| `-BattleFollowSightTraces` | ParallelFor名 | 対象外 | 起動引数ではない。指定しても当該機能は切り替わらない | 直接起動箇所なし | BattleLeaderRuntime.cpp:841 |
| `-BattleGroupStrength` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleGroups.cpp:476 |
| `-BattleHeavyCost` | AreaHeavyCost=15（BattleState.h:327） | なし | Value（=値）：15；指定時は読取値（後処理・モード条件あり） | [S176](#s176) | BattleState.cpp:552 |
| `-BattleHeavyDamage` | HeavyDamageScale=1.5f（BattleState.h:329） | なし | Value（=値）：1.5f；指定時は読取値（後処理・モード条件あり） | [S176](#s176) | BattleState.cpp:552 |
| `-BattleHeavyHealth` | HeavyHealthScale=3.f（BattleState.h:325） | なし | Value（=値）：3.f；指定時は読取値（後処理・モード条件あり） | [S176](#s176) | BattleState.cpp:551 |
| `-BattleHeavyMembers` | HeavyMembers=6（BattleState.h:324） | なし | Value（=値）：6；指定時は読取値（後処理・モード条件あり） | [S176](#s176) | BattleState.cpp:551 |
| `-BattleHeavySpeed` | HeavySpeedScale=.714f（BattleState.h:326） | なし | Value（=値）：.714f；指定時は読取値（後処理・モード条件あり） | [S176](#s176) | BattleState.cpp:552 |
| `-BattleHeightTest` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S144](#s144) | BattleTerrainTest.cpp:27 |
| `-BattleHireSightRefresh` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S195](#s195) | BattleMixedRecruitment.cpp:112 |
| `-BattleHitDirection` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleHUD.cpp:657 |
| `-BattleKillLog` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleHUD.cpp:762 |
| `-BattleLargeFieldTest` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S058](#s058) | BattleGameMode.cpp:143 |
| `-BattleLeaderCommand` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S019](#s019)、[S040](#s040)、[S050](#s050)、[S060](#s060)、[S063](#s063)、[S070](#s070)、[S083](#s083)、[S088](#s088)、[S091](#s091)、[S102](#s102)、[S108](#s108)、[S112](#s112)、[S119](#s119) | BattleGameMode.cpp:54 |
| `-BattleLeaderGuard` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S033](#s033)、[S129](#s129) | BattleTactics.cpp:16 |
| `-BattleLeaderHealth` | LeaderHealthMultiplier=3.f（BattleState.h:634） | LeaderHealthMultiplier=3.0（DefaultGame.ini:88） | Value（=値）：3。有限値を1..10へClamp、0は1 | [S130](#s130) | BattleTactics.cpp:23 |
| `-BattleLeaderNoControl` | INDEX_NONE | なし | Value（=値）：INDEX_NONE=-1。制御対象除外のID指定 | [S038](#s038)、[S061](#s061)、[S109](#s109) | BattleLeaderRuntime.cpp:138 |
| `-BattleLeaderQA` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S061](#s061)、[S109](#s109) | BattleLeaderRuntime.cpp:254 |
| `-BattleLeaderSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S062](#s062)、[S110](#s110)、[S120](#s120) | BattleLeaderRuntime.cpp:252 |
| `-BattleLeaderUnits` | MixedFast=20、Wide=500、それ以外40 | なし | Value（=値）：MixedFast=20、Wide=500、それ以外40；指定時は読取値（後処理・モード条件あり） | [S019](#s019)、[S038](#s038)、[S040](#s040)、[S050](#s050)、[S061](#s061)、[S063](#s063)、[S070](#s070)、[S088](#s088)、[S091](#s091)、[S102](#s102)、[S109](#s109)、[S112](#s112)、[S120](#s120) | BattleLeaderRuntime.cpp:90 |
| `-BattleLegacyAnimationLOD` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S012](#s012) | BattleUnit.cpp:653 |
| `-BattleLegacyWeaponVisuals` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S018](#s018) | BattleUnit.cpp:100 |
| `-BattleMapStats` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S064](#s064) | BattleLeaderRuntime.cpp:230, BattleMapStats.cpp:40 |
| `-BattleMassMovement` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S004](#s004) | BattleBenchmark.cpp:39 |
| `-BattleMatchReport` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S056](#s056) | BattleMatchSoak.cpp:56 |
| `-BattleMatchScenario` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S056](#s056) | BattleMatchSoak.cpp:18 |
| `-BattleMatchSoak` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S056](#s056) | BattleMatchSoak.cpp:14 |
| `-BattleMixedFastMatch` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S065](#s065) | BattleLeaderRuntime.cpp:75 |
| `-BattleMixedFastSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S065](#s065) | BattleLeaderRuntime.cpp:216 |
| `-BattleMixedSaveSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S067](#s067) | BattleLeaderRuntime.cpp:218, BattleMixedBodies.cpp:117 |
| `-BattleMoveCourse` | 空（Gate/Openのみ、500/1000体時有効） | なし | Value（=値）：空（Gate/Openのみ、500/1000体時有効）；指定時は読取値（後処理・モード条件あり） | [S005](#s005) | BattleBenchmark.cpp:37 |
| `-BattleMovementScene` | -1 | なし | Value（=値）：-1；指定時は読取値（後処理・モード条件あり） | [S072](#s072) | BattleLeaderRuntime.cpp:87 |
| `-BattleMovementTrial` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S070](#s070) | BattleLeaderRuntime.cpp:86 |
| `-BattleMovingCapture` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S015](#s015)、[S024](#s024)、[S027](#s027)、[S028](#s028)、[S029](#s029)、[S030](#s030)、[S098](#s098) | BattleSquadTrial.cpp:18 |
| `-BattleNetBenchmark` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S074](#s074)、[S075](#s075) | BattleNetwork.cpp:33, BattleNetworkBenchmark.cpp:28, BattleNetworkPilot.cpp:139 |
| `-BattleNetBenchmarkPilot` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S077](#s077) | BattleNetworkBenchmark.cpp:30 |
| `-BattleNetHoldProbe` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S079](#s079) | BattleNetworkProbe.cpp:9 |
| `-BattleNetLeaveAfter` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S079](#s079) | BattleNetworkProbe.cpp:10 |
| `-BattleNetPilotProbe` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S075](#s075) | BattleNetworkPilot.cpp:139, BattleNetworkPilotProbe.cpp:7 |
| `-BattleNetProbe` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S075](#s075) | BattleNetworkProbe.cpp:21 |
| `-BattleNetReport` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S076](#s076) | BattleNetworkBenchmark.cpp:112 |
| `-BattleNetScreenshot` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S078](#s078) | BattleNetworkBenchmark.cpp:115 |
| `-BattleNetSeconds` | 20 | なし | Value（=値）：20秒（ネット試験時） | [S074](#s074) | BattleNetworkBenchmark.cpp:73 |
| `-BattleNetUnits` | 20 | なし | Value（=値）：20；指定時は読取値（後処理・モード条件あり） | [S073](#s073) | BattleNetwork.cpp:27, BattleNetworkBenchmark.cpp:83, BattleNetworkPilotProbe.cpp:22, BattleNetworkProbe.cpp:23 |
| `-BattleNoAnimationLOD` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S011](#s011) | BattleBenchmark.cpp:157, BattleBenchmark.cpp:158, BattleNetworkBenchmark.cpp:78, BattleUnit.cpp:646 |
| `-BattleNoCoverFallback` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S160](#s160) | BattleLeaderRuntime.cpp:1444 |
| `-BattleNoMuzzleFlash` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | 直接起動箇所なし | BattleMuzzleFlash.cpp:65 |
| `-BattleNoStagger` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S010](#s010) | BattleBenchmark.cpp:54 |
| `-BattleOffscreenMarkers` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleHUD.cpp:695 |
| `-BattleOpponentPreset` | Regular（1） | なし | Value（=値）：Regular（1）；指定時は読取値（後処理・モード条件あり） | [S001](#s001) | BattleBenchmark.cpp:48 |
| `-BattlePeekRangeCheck` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleCoverAction.cpp:24 |
| `-BattlePerceptionGrid` | bPerceptionGrid=true（BattleState.h:631） | bPerceptionGrid=True（DefaultGame.ini:85） | Value（=値）：1；通常混成の値。旧BenchmarkはSenseModeの別設定 | [S022](#s022)、[S116](#s116)、[S140](#s140) | BattleLeaderRuntime.cpp:105 |
| `-BattlePerceptionSlices` | 4 | なし | Value（=値）：4；指定時は読取値（後処理・モード条件あり） | [S013](#s013) | BattleBenchmark.cpp:56 |
| `-BattlePhase3` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S019](#s019)、[S032](#s032)、[S063](#s063)、[S112](#s112) | BattlePhase3.cpp:45, BattleUnit.cpp:38, BattleVATDisplay.cpp:393, BattleVATDisplay.cpp:396 |
| `-BattlePinnedFlank` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S151](#s151) | BattleLeaderRuntime.cpp:1469 |
| `-BattlePlayerRouteTest` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S143](#s143) | BattlePlayerRoutes.cpp:127 |
| `-BattlePreset` | Regular（1） | なし | Value（=値）：Regular（1）；指定時は読取値（後処理・モード条件あり） | [S001](#s001) | BattleBenchmark.cpp:48 |
| `-BattleProfileCSV` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S008](#s008) | BattleBenchmark.cpp:133, BattleBenchmark.cpp:144, BattleBenchmark.cpp:145 |
| `-BattleRangeAware` | 1 | なし | Value（=値）：1。値==1でON（2等はONではない） | [S087](#s087)、[S122](#s122) | BattleLeaderRuntime.cpp:81 |
| `-BattleRangeEngage` | 15000cm | なし | Value（=値）：15000cm；指定時は読取値（後処理・モード条件あり） | [S170](#s170) | BattleRanges.cpp:20 |
| `-BattleRangeEngageSupport` | 25000cm | なし | Value（=値）：25000cm；指定時は読取値（後処理・モード条件あり） | [S171](#s171) | BattleRanges.cpp:20 |
| `-BattleRangeProfile` | -1→都市ルールなら1、それ以外0 | なし | Value（=値）：-1→都市ルールなら1、それ以外0；各Range*はこのON枝内でのみ読む | [S170](#s170) | BattleRanges.cpp:14 |
| `-BattleRangeReach` | 50000cm | なし | Value（=値）：50000cm；指定時は読取値（後処理・モード条件あり） | [S171](#s171) | BattleRanges.cpp:21 |
| `-BattleRangeReachSupport` | 60000cm | なし | Value（=値）：60000cm；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleRanges.cpp:21 |
| `-BattleRangeSight` | 20000cm | なし | Value（=値）：20000cm；指定時は読取値（後処理・モード条件あり） | [S170](#s170) | BattleRanges.cpp:18 |
| `-BattleReacquireGrace` | bReacquireGrace=true（BattleState.h:612） | bReacquireGrace=True（DefaultGame.ini:67） | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S134](#s134) | BattleTactics.cpp:143 |
| `-BattleReduceAnimTasks` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S023](#s023)、[S117](#s117) | BattleLeaderRuntime.cpp:94 |
| `-BattleReduceHiddenBones` | bReduceHiddenBones=true（BattleState.h:632） | bReduceHiddenBones=True（DefaultGame.ini:86） | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S021](#s021)、[S115](#s115)、[S140](#s140) | BattleLeaderRuntime.cpp:98 |
| `-BattleReplayArm` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleFieldReplay.cpp:60 |
| `-BattleReplayBatch` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S026](#s026) | BattleFieldReplay.cpp:62 |
| `-BattleReplayFeature` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleFieldReplay.cpp:61 |
| `-BattleReplayReference` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S026](#s026) | BattleFieldReplay.cpp:59 |
| `-BattleReport` | 空→Saved/Benchmarks/Latest.json（Benchmark） | なし | Value（=値）：空→Saved/Benchmarks/Latest.json（Benchmark）；指定時は読取値（後処理・モード条件あり） | [S001](#s001) | BattleBenchmark.cpp:50 |
| `-BattleReturnFire` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S167](#s167) | BattleTactics.cpp:168 |
| `-BattleReuseSightContext` | bReuseSightContext=true（BattleState.h:630） | bReuseSightContext=True（DefaultGame.ini:84） | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S020](#s020)、[S114](#s114)、[S140](#s140) | BattleLeaderRuntime.cpp:102 |
| `-BattleRifleMembers` | RifleMembers=8（BattleState.h:648） | RifleMembers=8（DefaultGame.ini:102） | Value（=値）：Area=8、非Area=10；Areaなら設定8、非Areaは10。引数で2..10、費用は別 | [S177](#s177) | BattleState.cpp:564 |
| `-BattleRoutes` | -1→Areaかつ道路ノード2以上でON | なし | Value（=値）：-1→Areaかつ道路ノード2以上でON；指定時は読取値（後処理・モード条件あり） | [S141](#s141) | BattleRoutes.cpp:17 |
| `-BattleScaleScreenshot` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S059](#s059)、[S081](#s081) | BattleLargeFieldTest.cpp:19, BattleScaleSmoke.cpp:17 |
| `-BattleScaleSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S080](#s080) | BattleGameMode.cpp:147 |
| `-BattleScaleUnits` | Controllerの読取仮値0、GameModeの初期人数20 | なし | Value（=値）：Controllerの読取仮値0、GameModeの初期人数20；指定時は読取値（後処理・モード条件あり） | [S041](#s041)、[S054](#s054)、[S058](#s058)、[S080](#s080) | BattleController.cpp:52, BattleGameMode.cpp:79 |
| `-BattleScopeShot` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S082](#s082) | BattleLeaderRuntime.cpp:226 |
| `-BattleScoutRetreat` | bScoutRetreat=true（BattleState.h:547） | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S175](#s175) | BattleScoutReport.cpp:20 |
| `-BattleSeconds` | 20.f | なし | Value（=値）：20.f；指定時は読取値（後処理・モード条件あり） | [S001](#s001) | BattleBenchmark.cpp:41 |
| `-BattleSectorAI` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S035](#s035)、[S123](#s123) | BattleMatch.cpp:440, BattleMatch.cpp:455, BattleMatch.cpp:466 |
| `-BattleSectorAudit` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S145](#s145) | BattleSectorAudit.cpp:28 |
| `-BattleSectorContact` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S124](#s124) | BattleLeaderRuntime.cpp:1419 |
| `-BattleSectorCoverRoute` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S153](#s153) | BattleSectorRoute.cpp:22 |
| `-BattleSectorCoverWalk` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S158](#s158) | BattleLeaderRuntime.cpp:1459 |
| `-BattleSectorFlank` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S126](#s126) | BattleSectorRoute.cpp:120 |
| `-BattleSectorFront` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S127](#s127) | BattleSectorRoute.cpp:126 |
| `-BattleSectorMove` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S034](#s034) | BattleLeaderRuntime.cpp:1424 |
| `-BattleSectorMoveFire` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S147](#s147) | BattleLeaderRuntime.cpp:1429 |
| `-BattleSectorRoute` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S125](#s125) | BattleSectorRoute.cpp:16 |
| `-BattleSectorSlotReserve` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S128](#s128) | BattleStableMovement.cpp:79 |
| `-BattleSectorSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | 直接起動箇所なし | BattleLeaderRuntime.cpp:210 |
| `-BattleSectorViewShot` | 空文字（FString） | なし | Value（=値）：空文字（FString）；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleHUD.cpp:808, BattleHUD.cpp:856 |
| `-BattleSeed` | 7 | なし | Value（=値）：7；指定時は読取値（後処理・モード条件あり） | [S001](#s001) | BattleBenchmark.cpp:42 |
| `-BattleSenseMode` | 空→Legacy相当 | なし | Value（=値）：空→Legacy相当；指定時は読取値（後処理・モード条件あり） | [S003](#s003)、[S047](#s047) | BattleBenchmark.cpp:29 |
| `-BattleShotDirection` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S155](#s155) | BattleLeaderRuntime.cpp:1484 |
| `-BattleSightExperiment` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S187](#s187) | BattleLeaderRuntime.cpp:78 |
| `-BattleSightHitStats` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S191](#s191) | BattleLeaderRuntime.cpp:383 |
| `-BattleSightPairReuse` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S190](#s190) | BattleLeaderRuntime.cpp:343, BattlePerception.cpp:162 |
| `-BattleSightParallel` | 1 | なし | Value（=値）：1；PairReuse OFF・Mixed・ReuseSightContext ONも必要 | [S191](#s191) | BattlePerception.cpp:161 |
| `-BattleSightRange` | SightRange=10000.f（BattleState.h:318） | SightRange=10000.0（DefaultGame.ini:18） | Value（=値）：Profileなし10000、Profileあり20000（訓練倍率前）；Profile有効なら無視しProfile.Sight（既定20000） | [S052](#s052)、[S057](#s057)、[S086](#s086)、[S107](#s107)、[S113](#s113)、[S122](#s122) | BattleState.cpp:17 |
| `-BattleSightScanInterval` | .4 | なし | Value（=値）：0.4秒。指定時0.2..2秒へClamp | [S192](#s192) | BattleLeaderRuntime.cpp:113 |
| `-BattleSightScanNear` | 1.e7 | なし | Value（=値）：1e7cm。指定時0以上へ補正 | 直接起動箇所なし | BattleLeaderRuntime.cpp:113 |
| `-BattleSightSlowFar` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S193](#s193) | BattleLeaderRuntime.cpp:110 |
| `-BattleSightTeamMask` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S191](#s191) | BattleLeaderRuntime.cpp:388 |
| `-BattleSightTraces` | ParallelFor名 | 対象外 | 起動引数ではない。指定しても当該機能は切り替わらない | 直接起動箇所なし | BattlePerception.cpp:275 |
| `-BattleSightTrial` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S084](#s084) | BattleLeaderRuntime.cpp:234 |
| `-BattleSightVisual` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S187](#s187) | BattleLeaderRuntime.cpp:228 |
| `-BattleSmokeSuite` | 空→全体 | なし | Value（=値）：空→全体；指定時は読取値（後処理・モード条件あり） | [S049](#s049)、[S053](#s053)、[S095](#s095)、[S105](#s105)、[S106](#s106)、[S107](#s107) | BattleAbilitySmoke.cpp:121, BattleGameMode.cpp:156, BattleSquadSmoke.cpp:209, BattleTacticalSmoke.cpp:175 |
| `-BattleSmokeTest` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S046](#s046)、[S053](#s053)、[S094](#s094)、[S105](#s105)、[S106](#s106)、[S107](#s107) | BattleGameMode.cpp:142 |
| `-BattleSPGAmmo` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleArtillery.cpp:29 |
| `-BattleSplitDecisions` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S006](#s006) | BattleBenchmark.cpp:35 |
| `-BattleSquadCoverCheck` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S149](#s149) | BattleLeaderRuntime.cpp:1434 |
| `-BattleSquadFlank` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S052](#s052)、[S057](#s057)、[S088](#s088)、[S129](#s129) | BattleSquadTactics.cpp:22 |
| `-BattleSquadST2` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S091](#s091) | BattleSquadTactics.cpp:21 |
| `-BattleSquadTactics` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S052](#s052)、[S057](#s057)、[S094](#s094) | BattleSquadTactics.cpp:17 |
| `-BattleSquadTrial` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S014](#s014)、[S025](#s025)、[S026](#s026)、[S027](#s027)、[S028](#s028)、[S029](#s029)、[S030](#s030)、[S097](#s097) | BattleGameMode.cpp:64, BattleLeaderRuntime.cpp:73, BattleUnit.cpp:101 |
| `-BattleST2Smoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S092](#s092) | BattleLeaderRuntime.cpp:244 |
| `-BattleStableLoad` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S009](#s009) | BattleBenchmark.cpp:49 |
| `-BattleStableMovement` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S050](#s050)、[S070](#s070) | BattleLeaderRuntime.cpp:83 |
| `-BattleStanceAnim` | 1 | なし | Value（=値）：1；BodyAnimとは独立（D09） | [S100](#s100) | BattleStance.cpp:38 |
| `-BattleStances` | -1→都市ルールならON | なし | Value（=値）：-1→都市ルールならON；指定時は読取値（後処理・モード条件あり） | [S055](#s055)、[S099](#s099)、[S172](#s172) | BattleStance.cpp:23 |
| `-BattleStanceShot` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S099](#s099) | BattleLeaderRuntime.cpp:224 |
| `-BattleSTObserveSeconds` | 8 | なし | Value（=値）：8；指定時は読取値（後処理・モード条件あり） | [S094](#s094) | BattleSquadTacticsSmoke.cpp:126 |
| `-BattleSTScreens` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S096](#s096) | BattleSquadTacticsSmoke.cpp:127 |
| `-BattleSupplyAwayRatio` | SupplyAwayRatio=1.f（BattleState.h:641） | SupplyAwayRatio=1.0（DefaultGame.ini:94） | Value（=値）：1.0；指定時は読取値（後処理・モード条件あり） | [S132](#s132) | BattleTactics.cpp:36 |
| `-BattleSupplyHeal` | SupplyHealPerSecond=0.f（BattleState.h:642） | SupplyHealPerSecond=10.0（DefaultGame.ini:95） | Value（=値）：10.0；指定時は読取値（後処理・モード条件あり） | [S132](#s132) | BattleTactics.cpp:36 |
| `-BattleSupplyResumeHealth` | SupplyResumeHealthRatio=0.f（BattleState.h:643） | SupplyResumeHealthRatio=0.9（DefaultGame.ini:96） | Value（=値）：0.9；指定時は読取値（後処理・モード条件あり） | [S133](#s133) | BattleTactics.cpp:37 |
| `-BattleSupplyResumeLiving` | SupplyResumeLiving=10（BattleState.h:640） | SupplyResumeLiving=10（DefaultGame.ini:93） | Value（=値）：10；指定時は読取値（後処理・モード条件あり） | [S131](#s131) | BattleTactics.cpp:35 |
| `-BattleSupplyReturnLiving` | SupplyReturnLiving=8（BattleState.h:639） | SupplyReturnLiving=8（DefaultGame.ini:92） | Value（=値）：8；指定時は読取値（後処理・モード条件あり） | [S131](#s131) | BattleTactics.cpp:35 |
| `-BattleSupportSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S103](#s103) | BattleLeaderRuntime.cpp:242 |
| `-BattleSuppression` | 0 | なし | Value（=値）：0；0でもExoShieldSuppressionが独立有効（D05） | [S173](#s173) | BattleSuppression.cpp:13 |
| `-BattleSuppressionDecay` | .25/秒 | なし | Value（=値）：.25/秒；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleSuppression.cpp:15 |
| `-BattleSuppressionPerShot` | .12 | なし | Value（=値）：.12；指定時は読取値（後処理・モード条件あり） | [S173](#s173) | BattleSuppression.cpp:14 |
| `-BattleSuppressionPin` | .5 | なし | Value（=値）：.5；指定時は読取値（後処理・モード条件あり） | [S173](#s173) | BattleSuppression.cpp:16 |
| `-BattleSuppressionRadius` | 200cm | なし | Value（=値）：200cm；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleSuppression.cpp:14 |
| `-BattleSuppressionSpread` | 1.5 | なし | Value（=値）：1.5；指定時は読取値（後処理・モード条件あり） | [S173](#s173) | BattleSuppression.cpp:15 |
| `-BattleSuppressionSupport` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S173](#s173) | BattleSuppression.cpp:17 |
| `-BattleSymmetricStart` | 引数初期-1；bSymmetricStart=false（BattleState.h:523） | なし | Value（=値）：-1→都市ルール1、それ以外設定0；引数-1→設定値 OR 都市ルール。0/1明示が優先 | [S175](#s175) | BattleGroups.cpp:58 |
| `-BattleSymmetryAudit` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S175](#s175) | BattleCover.cpp:56 |
| `-BattleTacticalObserve` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S088](#s088)、[S091](#s091)、[S094](#s094)、[S102](#s102)、[S182](#s182) | BattleSquadTactics.cpp:26 |
| `-BattleTeamSupport` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S052](#s052)、[S057](#s057)、[S088](#s088)、[S102](#s102) | BattleTeamAI.cpp:12 |
| `-BattleTerrain` | Actorタグ | 対象外 | 起動引数ではない。指定しても当該機能は切り替わらない | 直接起動箇所なし | BattleTerrain.h:18 |
| `-BattleTerrainLineBodies` | 1 | なし | Value（=値）：1；指定時は読取値（後処理・モード条件あり） | [S136](#s136) | BattleTactics.cpp:231 |
| `-BattleThinkCap` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S194](#s194) | BattleLeaderRuntime.cpp:1356 |
| `-BattleTracerShot` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S101](#s101) | BattleStanceShot.cpp:87 |
| `-BattleTrialSeed` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S057](#s057)、[S187](#s187) | BattleTactics.cpp:207 |
| `-BattleTrialSwap` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S186](#s186)、[S187](#s187) | BattleArea.cpp:16, BattleAreaDepot.cpp:32, BattleLeaderRuntime.cpp:80, BattleMatch.cpp:27, BattleMatch.cpp:271, BattleMatch.cpp:290, BattleWideMapAudit.cpp:116 |
| `-BattleUnderFireCover` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S159](#s159) | BattleLeaderRuntime.cpp:1454 |
| `-BattleUnits` | Benchmark=20、SquadTrial=1000 | なし | Value（=値）：Benchmark=20、SquadTrial=1000；指定時は読取値（後処理・モード条件あり） | [S001](#s001)、[S014](#s014)、[S024](#s024)、[S027](#s027)、[S028](#s028)、[S029](#s029)、[S030](#s030)、[S097](#s097) | BattleBenchmark.cpp:36, BattleSquadTrial.cpp:28 |
| `-BattleVATFarLight` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S196](#s196) | BattleVATDisplay.cpp:37 |
| `-BattleVerifyMixedSave` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S184](#s184) | BattleSave.cpp:326, BattleSightExperiment.cpp:40 |
| `-BattleWaypointPass` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | [S036](#s036)、[S146](#s146) | BattleStableMovement.cpp:364 |
| `-BattleWeaponLoadouts` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S017](#s017)、[S024](#s024)、[S027](#s027)、[S028](#s028)、[S029](#s029)、[S030](#s030) | BattleSquadTrial.cpp:50 |
| `-BattleWeaponOrderGate` | 0 | なし | Value（=値）：0；指定時は読取値（後処理・モード条件あり） | 直接起動箇所なし | BattleWeaponOrders.cpp:10 |
| `-BattleWeaponPreview` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S048](#s048) | BattleWeaponPreview.cpp:19 |
| `-BattleWeaponRange` | WeaponRange=5000.f（BattleState.h:319） | WeaponRange=5000.0（DefaultGame.ini:19） | Value（=値）：Profileなし5000、Profileあり15000（carbine交戦）；Profile有効なら無視しEngageCarbine（既定15000） | [S057](#s057)、[S085](#s085)、[S185](#s185) | BattleState.cpp:42 |
| `-BattleWeaponTrial` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S084](#s084) | BattleLeaderRuntime.cpp:220 |
| `-BattleWideMapAudit` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S187](#s187) | BattleLeaderRuntime.cpp:232 |
| `-BattleWideMapGeometryOnly` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S188](#s188) | BattleWideMapAudit.cpp:157 |
| `-BattleWideMixed` | false（引数省略） | なし | Param（存在）：引数省略でもSightFieldならWide有効 | [S037](#s037)、[S040](#s040)、[S042](#s042)、[S050](#s050)、[S052](#s052)、[S055](#s055)、[S057](#s057)、[S064](#s064)、[S065](#s065)、[S067](#s067)、[S070](#s070)、[S082](#s082)、[S083](#s083)、[S088](#s088)、[S091](#s091)、[S099](#s099)、[S102](#s102)、[S120](#s120)、[S122](#s122) | BattleGameMode.cpp:55, BattleLeaderRuntime.cpp:74 |
| `-BattleWideScreens` | 空文字／省略時は未指定 | なし | Value（=値）：空文字／省略時は未指定；指定時は読取値（後処理・モード条件あり） | [S045](#s045)、[S051](#s051)、[S055](#s055)、[S066](#s066)、[S069](#s069)、[S071](#s071)、[S082](#s082)、[S090](#s090)、[S093](#s093)、[S099](#s099)、[S104](#s104)、[S121](#s121)、[S189](#s189) | BattleAreaSmoke.cpp:1156, BattleContactTrial.cpp:97, BattleExoShot.cpp:78, BattleMixedFastSmoke.cpp:34, BattleMixedSaveSmoke.cpp:124, BattleMovementTrial.cpp:108, BattleScopeShot.cpp:45, BattleSightVisual.cpp:30, BattleSquadFlankSmoke.cpp:165, BattleSquadRetreatSmoke.cpp:20, BattleStanceShot.cpp:96, BattleTeamSupportSmoke.cpp:122, BattleWideMapAudit.cpp:159, BattleWideMapAudit.cpp:190, BattleWideMapAudit.cpp:40, BattleWideMixedSmoke.cpp:39, BattleWideMixedSmoke.cpp:53 |
| `-BattleWideSmoke` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S120](#s120) | BattleLeaderRuntime.cpp:246 |
| `-BattleWideSoak` | false（引数省略） | なし | Param（存在）：省略OFF／存在でON。OFFは省略 | [S089](#s089)、[S092](#s092)、[S103](#s103)、[S120](#s120) | BattleLeaderRuntime.cpp:248 |

### Setup送信式の根拠一覧

全Setupの`.ps1`を対象に、引数リテラルを含む実行コード行を収録（コメントは除外）。これは実行ログではない。if/ループ・モードの条件付き式も含む。各式の変数の初期値は併記し、行の外のブロック条件は引用ファイルで確認する。引数を組み立てる行と、既存配列から除去する行は区別する。主な通常入口の条件は1節で解決済み。

<a id="s001"></a>
**S001 — `Setup/Benchmark-Battle.ps1:52`**

```powershell
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-unattended', '-nosound', '-nosplash', '-BattleBenchmark', "-BattleUnits=$Units", "-BattleSeconds=$Seconds", "-BattlePreset=$Preset", "-BattleOpponentPreset=$OpponentPreset", "-BattleSeed=$Seed", ('-BattleReport="' + $report + '"'), ('-abslog="' + $log + '"'))
```

パラメータ既定：`$Units`=20、`$Seconds`=20、`$Preset`='Regular'、`$OpponentPreset`='Regular'、`$Seed`=7。

<a id="s002"></a>
**S002 — `Setup/Benchmark-Battle.ps1:53`**

```powershell
if ($DiagnosticOmit -ne 'None') { $arguments += "-BattleDiagnosticOmit=$DiagnosticOmit" }
```

パラメータ既定：`$DiagnosticOmit`='None'。

<a id="s003"></a>
**S003 — `Setup/Benchmark-Battle.ps1:54`**

```powershell
$arguments += "-BattleSenseMode=$SenseMode"
```

パラメータ既定：`$SenseMode`='Legacy'。

<a id="s004"></a>
**S004 — `Setup/Benchmark-Battle.ps1:55`**

```powershell
if ($MassMovement) { $arguments += '-BattleMassMovement' }
```

<a id="s005"></a>
**S005 — `Setup/Benchmark-Battle.ps1:56`**

```powershell
if ($MoveCourse) { $arguments += "-BattleMoveCourse=$MoveCourse" }
```

<a id="s006"></a>
**S006 — `Setup/Benchmark-Battle.ps1:57`**

```powershell
if ($SplitDecisions) { $arguments += '-BattleSplitDecisions' }
```

<a id="s007"></a>
**S007 — `Setup/Benchmark-Battle.ps1:58`**

```powershell
if ($FirstPerson) { $arguments += '-BattleBenchmarkFPS' }
```

<a id="s008"></a>
**S008 — `Setup/Benchmark-Battle.ps1:59`**

```powershell
if ($ProfileCSV) { $arguments += '-BattleProfileCSV' }
```

<a id="s009"></a>
**S009 — `Setup/Benchmark-Battle.ps1:60`**

```powershell
if ($StableLoad) { $arguments += '-BattleStableLoad' }
```

<a id="s010"></a>
**S010 — `Setup/Benchmark-Battle.ps1:61`**

```powershell
if ($NoStagger) { $arguments += '-BattleNoStagger' }
```

<a id="s011"></a>
**S011 — `Setup/Benchmark-Battle.ps1:62`**

```powershell
if ($NoAnimationLOD) { $arguments += '-BattleNoAnimationLOD' }
```

<a id="s012"></a>
**S012 — `Setup/Benchmark-Battle.ps1:63`**

```powershell
if ($LegacyAnimationLOD) { $arguments += '-BattleLegacyAnimationLOD' }
```

<a id="s013"></a>
**S013 — `Setup/Benchmark-Battle.ps1:64`**

```powershell
if ($PerceptionSlices -gt 0) { $arguments += "-BattlePerceptionSlices=$PerceptionSlices" }
```

パラメータ既定：`$PerceptionSlices`=0。

<a id="s014"></a>
**S014 — `Setup/Legacy/Test-FieldTrial.ps1:23`**

```powershell
'-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync',"-BattleUnits=$count","-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",
```

<a id="s015"></a>
**S015 — `Setup/Legacy/Test-FieldTrial.ps1:24`**

```powershell
('-BattleMovingCapture="' + $capture + '"'),('-abslog="' + $log + '"'))
```

<a id="s016"></a>
**S016 — `Setup/Legacy/Test-FieldTrial.ps1:25`**

```powershell
if ($Diagnostics) { $argsList += "-BattleFieldDiagnostics" }
```

<a id="s017"></a>
**S017 — `Setup/Legacy/Test-FieldTrial.ps1:26`**

```powershell
if ($WeaponLoadouts) { $argsList += "-BattleWeaponLoadouts" }
```

<a id="s018"></a>
**S018 — `Setup/Legacy/Test-FieldTrial.ps1:27`**

```powershell
if ($LegacyVisuals) { $argsList += "-BattleLegacyWeaponVisuals" }
```

<a id="s019"></a>
**S019 — `Setup/Legacy/Test-PerceptionAudit.ps1:19`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleLeaderUnits=$Units",('-abslog="'+$log+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'),"-P3Decision=$Decision","-P3View=$View","-P3Layout=$Layout","-P3Seed=$Seed","-P3Warmup=$Warmup","-P3Duration=$Duration",'-ResX=1920','-ResY=1080','-Windowed','-ForceRes','-NoVSync')
```

パラメータ既定：`$Units`=500、`$Decision`='Leader'、`$View`='RTS'、`$Layout`='Open'、`$Seed`=1、`$Warmup`=10、`$Duration`=60。

<a id="s020"></a>
**S020 — `Setup/Legacy/Test-PerceptionAudit.ps1:25`**

```powershell
$args+="-BattleReuseSightContext=$ContextReuse"
```

パラメータ既定：`$ContextReuse`=0。

<a id="s021"></a>
**S021 — `Setup/Legacy/Test-PerceptionAudit.ps1:26`**

```powershell
$args+="-BattleReduceHiddenBones=$HiddenBones"
```

パラメータ既定：`$HiddenBones`=0。

<a id="s022"></a>
**S022 — `Setup/Legacy/Test-PerceptionAudit.ps1:27`**

```powershell
$args+="-BattlePerceptionGrid=$PerceptionGrid"
```

パラメータ既定：`$PerceptionGrid`=0。

<a id="s023"></a>
**S023 — `Setup/Legacy/Test-PerceptionAudit.ps1:28`**

```powershell
$args+="-BattleReduceAnimTasks=$AnimTasks"
```

パラメータ既定：`$AnimTasks`=0。

<a id="s024"></a>
**S024 — `Setup/Legacy/Test-RenderReplay.ps1:14`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts',('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))+$extra
```

<a id="s025"></a>
**S025 — `Setup/Legacy/Test-RenderReplay.ps1:29`**

```powershell
Invoke-Trial "seed$seed-$mode" @("-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",'-BattleFieldDiagnostics',('-BattleFieldRecord="'+$record+'"')) 'FIELD_TRIAL mode='
```

<a id="s026"></a>
**S026 — `Setup/Legacy/Test-RenderReplay.ps1:34`**

```powershell
Invoke-Trial "seed$seed-replay" @("-BattleFieldSeed=$seed",'-BattleSquadTrial=Individual','-BattleReplayBatch',('-BattleFieldReplay="'+$b+'"'),('-BattleReplayReference="'+$a+'"')) 'FIELD_REPLAY arm=' 32
```

<a id="s027"></a>
**S027 — `Setup/Legacy/Test-T2a.ps1:25`**

```powershell
$argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts','-BattleFieldDiagnostics',"-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",'-T2a-Metrics',"-T2a-Allocation=$allocation",('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
```

<a id="s028"></a>
**S028 — `Setup/Legacy/Test-T2b.ps1:25`**

```powershell
$argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts','-BattleFieldDiagnostics',"-BattleFieldSeed=$seed","-BattleSquadTrial=$mode",'-T2b-Metrics','-T2a-Allocation=Off',('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
```

<a id="s029"></a>
**S029 — `Setup/Legacy/Test-T5.ps1:29`**

```powershell
$argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts',"-BattleFieldSeed=$seed",'-BattleSquadTrial=Individual','-T5-Replay',"-T5-Conditions=$Conditions",'-T2a-Allocation=Off','-T2b-TurnToFace=Off',('-BattleFieldReplay="'+$record+'"'),('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
```

パラメータ既定：`$Conditions`=30。

<a id="s030"></a>
**S030 — `Setup/Legacy/Test-T5A.ps1:34`**

```powershell
$argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts',"-BattleFieldSeed=$seed",'-BattleSquadTrial=Individual','-T5-Replay',"-T5-Stage=$stage","-T5-Profile=$profile",'-T2a-Allocation=Off','-T2b-TurnToFace=Off',('-BattleFieldReplay="'+$record+'"'),('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
```

<a id="s031"></a>
**S031 — `Setup/Measure-SpikeRoot.ps1:37`**

```powershell
$runArgs+='-BattleDisplayLOD=0'
```

<a id="s032"></a>
**S032 — `Setup/Measure-SpikeRoot.ps1:38`**

```powershell
$runArgs+=@(('-abslog="'+(Join-Path $out 'run.log')+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'))
```

<a id="s033"></a>
**S033 — `Setup/Play-Area.ps1:11`**

```powershell
$extra += '-BattleLeaderGuard=1'
```

<a id="s034"></a>
**S034 — `Setup/Play-Area.ps1:13`**

```powershell
$extra += '-BattleSectorMove=1'
```

<a id="s035"></a>
**S035 — `Setup/Play-Area.ps1:14`**

```powershell
if ($SectorAI) { $extra += '-BattleSectorAI=1' }
```

<a id="s036"></a>
**S036 — `Setup/Play-Area.ps1:19`**

```powershell
if ($SectorTrials) { $extra += @('-BattleSectorContact=1','-BattleSectorRoute=1','-BattleSectorFlank=1','-BattleSectorFront=1','-BattleWaypointPass=1','-BattleSectorMoveFire=1','-BattleCoverHuddle=1','-BattlePinnedFlank=1','-BattleArrivalThreat=1','-BattleSectorCoverRoute=1','-BattleCoverStrict=1','-BattleSightTeamMask=1','-BattleShotDirection=1','-BattleDangerRoute=1','-BattleCloseIn=1','-BattleSectorCoverWalk=1','-BattleUnderFireCover=1','-BattleNoCoverFallback=1','-BattleCoverPriority=1') }
```

<a id="s037"></a>
**S037 — `Setup/Play-Area.ps1:22`**

```powershell
$playArgs = @("/Game/Prototype/Lvl_BattleWide_$Map", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-BattleWideMixed', '-BattleArea', '-BattleCoverActions=1', '-BattleFlankCoordination=1', '-BattleAreaPointSupply=1', '-BattleFollowerAnimSkip=1') + $extra
```

パラメータ既定：`$Map`='A'。

<a id="s038"></a>
**S038 — `Setup/Play-LeaderCommand.ps1:8`**

```powershell
& $editor $project '/Game/Prototype/Lvl_BattlePrototype' -game -windowed -ResX=1280 -ResY=720 -ForceRes -BattleLeaderCommand=1 "-BattleLeaderUnits=$Units" "-BattleLeaderNoControl=$NoControlGroup" "-BattleDisplayLOD=$DisplayLOD"
```

パラメータ既定：`$Units`=40、`$NoControlGroup`=-1、`$DisplayLOD`=1。

<a id="s039"></a>
**S039 — `Setup/Play-Prototype.ps1:12`**

```powershell
if ($FastMatch) { $extra += '-BattleFastMatch' }
```

<a id="s040"></a>
**S040 — `Setup/Play-Prototype.ps1:13`**

```powershell
if ($LargeField -and !$LegacyActors) { $extra += @('-BattleLeaderCommand=1','-BattleWideMixed','-BattleLeaderUnits=500') }
```

<a id="s041"></a>
**S041 — `Setup/Play-Prototype.ps1:14`**

```powershell
elseif ($Units -ne 20) { $extra += "-BattleScaleUnits=$Units" }
```

パラメータ既定：`$Units`=20。

<a id="s042"></a>
**S042 — `Setup/Test-Area.ps1:13`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_A','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleArea','-BattleAreaSmoke',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
```

パラメータ既定：`$DisplayLOD`=1。

<a id="s043"></a>
**S043 — `Setup/Test-Area.ps1:14`**

```powershell
$arguments+=@("-BattleAreaCompositionPlanner=$AreaCompositionPlanner","-BattleAreaCompositionPlannerTeams=$AreaCompositionPlannerTeams")
```

パラメータ既定：`$AreaCompositionPlanner`=0、`$AreaCompositionPlannerTeams`=0。

<a id="s044"></a>
**S044 — `Setup/Test-Area.ps1:15`**

```powershell
$arguments+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams","-BattleCoverActions=$CoverActions","-BattleFlankCoordination=$FlankCoordination")
```

パラメータ既定：`$FlankRelaxed`=0、`$FlankTeams`=3、`$CoverActions`=0、`$FlankCoordination`=0。

<a id="s045"></a>
**S045 — `Setup/Test-Area.ps1:16`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
```

<a id="s046"></a>
**S046 — `Setup/Test-Battle.ps1:8`**

```powershell
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-nullrhi', '-unattended', '-nosound', '-nosplash', '-BattleSmokeTest', ('-abslog="' + $log + '"'))
```

<a id="s047"></a>
**S047 — `Setup/Test-Battle.ps1:9`**

```powershell
$arguments += "-BattleSenseMode=$SenseMode"
```

パラメータ既定：`$SenseMode`='Legacy'。

<a id="s048"></a>
**S048 — `Setup/Test-Battle.ps1:12`**

```powershell
$arguments=@($arguments | Where-Object { $_ -ne "-nullrhi" }) + @("-RenderOffscreen","-ResX=1280","-ResY=720",('-BattleWeaponPreview="'+$prefix+'"'))
```

<a id="s049"></a>
**S049 — `Setup/Test-Battle.ps1:14`**

```powershell
if ($Suite -ne 'All') { $arguments += "-BattleSmokeSuite=$Suite" }
```

パラメータ既定：`$Suite`='All'。

<a id="s050"></a>
**S050 — `Setup/Test-ContactMovement.ps1:13`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleContactTrial','-BattleStableMovement=1',"-BattleContactAvoidance=$Enabled",('-abslog="'+$out+'/run.log"'))
```

パラメータ既定：`$Enabled`=1。

<a id="s051"></a>
**S051 — `Setup/Test-ContactMovement.ps1:14`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+$out+'/screen"'))}else{$arguments+='-nullrhi'}
```

<a id="s052"></a>
**S052 — `Setup/Test-CoverActions.ps1:13`**

```powershell
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleCoverActionSmoke','-BattleCoverActions=1','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0','-BattleSightRange=10000',('-abslog="'+$log+'"'))
```

パラメータ既定：`$Map`='C'。

<a id="s053"></a>
**S053 — `Setup/Test-Delegation.ps1:20`**

```powershell
$arguments = @(('"'+$project+'"'), $map, '-game', '-unattended', '-nosound', '-BattleSmokeTest', '-BattleSmokeSuite=Delegation', ('-abslog="'+$log+'"'))
```

<a id="s054"></a>
**S054 — `Setup/Test-Delegation.ps1:22`**

```powershell
$arguments += @('-BattleScaleUnits=500', '-RenderOffscreen', '-Windowed', '-ForceRes', ('-BattleDelegationScreens="'+(Join-Path $OutputFolder $name)+'"'))
```

パラメータ既定：`$OutputFolder`="$PSScriptRoot/Benchmarks/AllyDelegation-20261111"。

<a id="s055"></a>
**S055 — `Setup/Test-ExoShot.ps1:15`**

```powershell
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleArea','-BattleExoShot','-BattleStances=1','-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'shot')+'"'),('-abslog="'+$log+'"'))
```

パラメータ既定：`$Map`='E'。

<a id="s056"></a>
**S056 — `Setup/Test-FastMatch.ps1:11`**

```powershell
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-nullrhi', '-unattended', '-nosound', '-nosplash', '-BattleFastMatch', '-BattleMatchSoak', "-BattleMatchScenario=$Scenario", ('-BattleMatchReport="' + $report + '"'), ('-abslog="' + $log + '"'))
```

パラメータ既定：`$Scenario`=0。

<a id="s057"></a>
**S057 — `Setup/Test-FlankBay.ps1:13`**

```powershell
$a=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleWide_C','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleFlankBayTrial','-BattleSquadTactics=0','-BattleTeamSupport=0','-BattleSquadFlank=0','-BattleSightRange=10000','-BattleWeaponRange=5000',"-BayFlank=$Flank","-BaySign=$Sign","-BattleTrialSeed=$Seed",'-UseFixedTimeStep','-FPS=60',('-BayOutput="'+(Join-Path $out 'metrics.json')+'"'),('-abslog="'+$log+'"'))
```

パラメータ既定：`$Flank`=0、`$Sign`=1、`$Seed`=7。

<a id="s058"></a>
**S058 — `Setup/Test-LargeField.ps1:8`**

```powershell
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattleField500', '-game', '-unattended', '-nosound', '-nosplash', "-BattleScaleUnits=$Units", '-BattleLargeFieldTest', ('-abslog="' + $log + '"'))
```

<a id="s059"></a>
**S059 — `Setup/Test-LargeField.ps1:13`**

```powershell
$arguments += @('-RenderOffscreen', '-ResX=1280', '-ResY=720', ('-BattleScaleScreenshot="' + $screenshot + '"'))
```

<a id="s060"></a>
**S060 — `Setup/Test-LeaderCommand.ps1:13`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',('-abslog="'+$log+'"'))
```

<a id="s061"></a>
**S061 — `Setup/Test-LeaderCommand.ps1:14`**

```powershell
if($UI) { $args+=@('-RenderOffscreen',"-ResX=$Width","-ResY=$Height",'-Windowed','-ForceRes',"-BattleLeaderUnits=$Units",'-BattleLeaderNoControl=1',('-BattleLeaderQA="'+(Join-Path $out 'list')+'"')) }
```

パラメータ既定：`$Width`=1920、`$Height`=1080、`$Units`=1000。

<a id="s062"></a>
**S062 — `Setup/Test-LeaderCommand.ps1:15`**

```powershell
else { $args+=@('-nullrhi','-BattleLeaderSmoke') }
```

<a id="s063"></a>
**S063 — `Setup/Test-LeaderPhase3.ps1:19`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleLeaderUnits=$Units",('-abslog="'+$log+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'),"-P3Decision=$Decision","-P3View=$View","-P3Layout=$Layout","-P3Seed=$Seed","-P3Warmup=$Warmup","-P3Duration=$Duration",'-ResX=1920','-ResY=1080','-Windowed','-ForceRes','-NoVSync')
```

パラメータ既定：`$Units`=500、`$Decision`='Leader'、`$View`='RTS'、`$Layout`='Open'、`$Seed`=1、`$Warmup`=10、`$Duration`=60。

<a id="s064"></a>
**S064 — `Setup/Test-MapStats.ps1:18`**

```powershell
$arguments=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-nullrhi','-BattleWideMixed','-BattleArea',('-BattleMapStats="'+$stats+'"'),('-abslog="'+$log+'"'))
```

パラメータ既定：`$Map`='E'。

<a id="s065"></a>
**S065 — `Setup/Test-MixedFast.ps1:13`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleMixedFastMatch','-BattleMixedFastSmoke',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
```

パラメータ既定：`$DisplayLOD`=1。

<a id="s066"></a>
**S066 — `Setup/Test-MixedFast.ps1:14`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
```

<a id="s067"></a>
**S067 — `Setup/Test-MixedSave.ps1:13`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleMixedSaveSmoke',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
```

パラメータ既定：`$DisplayLOD`=1。

<a id="s068"></a>
**S068 — `Setup/Test-MixedSave.ps1:14`**

```powershell
$arguments+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams","-BattleCoverActions=$CoverActions","-BattleFlankCoordination=$FlankCoordination")
```

パラメータ既定：`$FlankRelaxed`=0、`$FlankTeams`=3、`$CoverActions`=0、`$FlankCoordination`=0。

<a id="s069"></a>
**S069 — `Setup/Test-MixedSave.ps1:15`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
```

<a id="s070"></a>
**S070 — `Setup/Test-MovementStability.ps1:13`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleLeaderCommand=1','-BattleLeaderUnits=500','-BattleMovementTrial',"-BattleStableMovement=$Stable",('-abslog="'+$out+'/run.log"'))
```

パラメータ既定：`$Stable`=1。

<a id="s071"></a>
**S071 — `Setup/Test-MovementStability.ps1:14`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+$out+'/screen"'))}else{$arguments+='-nullrhi'}
```

<a id="s072"></a>
**S072 — `Setup/Test-MovementStability.ps1:15`**

```powershell
$arguments+="-BattleMovementScene=$Scene"
```

パラメータ既定：`$Scene`=-1。

<a id="s073"></a>
**S073 — `Setup/Test-Network.ps1:14`**

```powershell
$common += "-BattleNetUnits=$Units"
```

パラメータ既定：`$Units`=20。

<a id="s074"></a>
**S074 — `Setup/Test-Network.ps1:20`**

```powershell
$common += @('-BattleNetBenchmark', "-BattleNetSeconds=$Seconds")
```

パラメータ既定：`$Seconds`=20。

<a id="s075"></a>
**S075 — `Setup/Test-Network.ps1:35`**

```powershell
$probeFlag = if ($Benchmark) { '-BattleNetBenchmark' } elseif ($Pilot) { '-BattleNetPilotProbe' } else { '-BattleNetProbe' }
```

<a id="s076"></a>
**S076 — `Setup/Test-Network.ps1:39`**

```powershell
$arguments += ('-BattleNetReport="' + $report + '"')
```

<a id="s077"></a>
**S077 — `Setup/Test-Network.ps1:40`**

```powershell
if ($PilotBenchmark) { $arguments += '-BattleNetBenchmarkPilot' }
```

<a id="s078"></a>
**S078 — `Setup/Test-Network.ps1:41`**

```powershell
if ($Rendered) { $arguments += ('-BattleNetScreenshot="' + [IO.Path]::ChangeExtension($report, '.png') + '"') }
```

<a id="s079"></a>
**S079 — `Setup/Test-NetworkSlots.ps1:24`**

```powershell
$arguments = @(('"' + $project + '"'), "127.0.0.1:$Port", '-game', '-BattleNetHoldProbe', "-BattleNetLeaveAfter=$LeaveAfter", ('-abslog="' + $log + '"')) + $common
```

パラメータ既定：`$Port`=17779。

<a id="s080"></a>
**S080 — `Setup/Test-Scale.ps1:7`**

```powershell
$arguments = @(('"' + $project + '"'), '/Game/Prototype/Lvl_BattlePrototype', '-game', '-unattended', '-nosound', '-nosplash', "-BattleScaleUnits=$Units", '-BattleScaleSmoke', ('-abslog="' + $log + '"'))
```

パラメータ既定：`$Units`=500。

<a id="s081"></a>
**S081 — `Setup/Test-Scale.ps1:12`**

```powershell
$arguments += @('-RenderOffscreen', '-ResX=1280', '-ResY=720', ('-BattleScaleScreenshot="' + $screenshot + '"'))
```

<a id="s082"></a>
**S082 — `Setup/Test-ScopeShot.ps1:15`**

```powershell
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleScopeShot','-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'shot')+'"'),('-abslog="'+$log+'"'))
```

パラメータ既定：`$Map`='E'。

<a id="s083"></a>
**S083 — `Setup/Test-SightTrial.ps1:16`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1','-BattleWideMixed',('-abslog="'+$log+'"'))
```

<a id="s084"></a>
**S084 — `Setup/Test-SightTrial.ps1:17`**

```powershell
$arguments+=if($WeaponProbe){'-BattleWeaponTrial'}else{'-BattleSightTrial'}
```

<a id="s085"></a>
**S085 — `Setup/Test-SightTrial.ps1:18`**

```powershell
if($WeaponRange){$arguments+="-BattleWeaponRange=$WeaponRange"}
```

パラメータ既定：`$WeaponRange`=0。

<a id="s086"></a>
**S086 — `Setup/Test-SightTrial.ps1:19`**

```powershell
if($SightRange){$arguments+="-BattleSightRange=$SightRange"}
```

パラメータ既定：`$SightRange`=0。

<a id="s087"></a>
**S087 — `Setup/Test-SightTrial.ps1:20`**

```powershell
if($RangeAware -ge 0){$arguments+="-BattleRangeAware=$RangeAware"}
```

パラメータ既定：`$RangeAware`=-1。

<a id="s088"></a>
**S088 — `Setup/Test-SquadFlank.ps1:21`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1','-BattleWideMixed','-BattleLeaderUnits=500',"-BattleSquadFlank=$Enabled", "-BattleTeamSupport=$SupportEnabled",'-BattleTacticalObserve',('-abslog="'+$log+'"'))
```

パラメータ既定：`$Enabled`=1、`$SupportEnabled`=1。

<a id="s089"></a>
**S089 — `Setup/Test-SquadFlank.ps1:22`**

```powershell
$arguments+=if($Soak){'-BattleWideSoak'}else{'-BattleFlankSmoke'}
```

<a id="s090"></a>
**S090 — `Setup/Test-SquadFlank.ps1:23`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
```

パラメータ既定：`$Width`=1280、`$Height`=720。

<a id="s091"></a>
**S091 — `Setup/Test-SquadRetreat.ps1:20`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1','-BattleWideMixed','-BattleLeaderUnits=500',"-BattleSquadST2=$Enabled",'-BattleTacticalObserve',('-abslog="'+$log+'"'))
```

パラメータ既定：`$Enabled`=1。

<a id="s092"></a>
**S092 — `Setup/Test-SquadRetreat.ps1:21`**

```powershell
$arguments+=if($Soak){'-BattleWideSoak'}else{'-BattleST2Smoke'}
```

<a id="s093"></a>
**S093 — `Setup/Test-SquadRetreat.ps1:22`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
```

パラメータ既定：`$Width`=1280、`$Height`=720。

<a id="s094"></a>
**S094 — `Setup/Test-SquadTactics.ps1:23`**

```powershell
$arguments = @(('"'+$project+'"'),$map,'-game','-unattended','-nosound','-nosplash','-BattleSmokeTest','-BattleTacticalObserve',"-BattleSquadTactics=$Enabled", "-BattleSTObserveSeconds=$Seconds",('-abslog="'+$log+'"'))
```

パラメータ既定：`$Enabled`=1、`$Seconds`=30。

<a id="s095"></a>
**S095 — `Setup/Test-SquadTactics.ps1:24`**

```powershell
if ($Suite -ne 'All') { $arguments += "-BattleSmokeSuite=$Suite" }
```

パラメータ既定：`$Suite`='SquadTactics'。

<a id="s096"></a>
**S096 — `Setup/Test-SquadTactics.ps1:25`**

```powershell
if ($Rendered) { $arguments += @('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleSTScreens="'+(Join-Path $out 'screen')+'"')) }
```

パラメータ既定：`$Width`=1280、`$Height`=720。

<a id="s097"></a>
**S097 — `Setup/Test-SquadTrial.ps1:21`**

```powershell
'-nosound','-unattended','-nosplash', "-BattleSquadTrial=$mode", "-BattleUnits=$Units", ('-abslog="' + $log + '"'))
```

パラメータ既定：`$Units`=1000。

<a id="s098"></a>
**S098 — `Setup/Test-SquadTrial.ps1:24`**

```powershell
$arguments += @('-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync',('-BattleMovingCapture="' + $capture + '"'))
```

<a id="s099"></a>
**S099 — `Setup/Test-StanceShot.ps1:17`**

```powershell
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleStanceShot','-BattleStances=1','-RenderOffscreen','-ForceRes','-ResX=1280','-ResY=720',('-BattleWideScreens="'+(Join-Path $out 'shot')+'"'),('-abslog="'+$log+'"'))
```

パラメータ既定：`$Map`='E'。

<a id="s100"></a>
**S100 — `Setup/Test-StanceShot.ps1:18`**

```powershell
$a+=('-BattleStanceAnim='+[int][bool]$Anim)
```

<a id="s101"></a>
**S101 — `Setup/Test-StanceShot.ps1:19`**

```powershell
if($Tracer){$a+='-BattleTracerShot'}
```

<a id="s102"></a>
**S102 — `Setup/Test-TeamSupport.ps1:20`**

```powershell
$arguments=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1','-BattleWideMixed','-BattleLeaderUnits=500',"-BattleTeamSupport=$Enabled",'-BattleTacticalObserve',('-abslog="'+$log+'"'))
```

パラメータ既定：`$Enabled`=1。

<a id="s103"></a>
**S103 — `Setup/Test-TeamSupport.ps1:21`**

```powershell
$arguments+=if($Soak){'-BattleWideSoak'}else{'-BattleSupportSmoke'}
```

<a id="s104"></a>
**S104 — `Setup/Test-TeamSupport.ps1:22`**

```powershell
if($Rendered){$arguments+=@('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$arguments+='-nullrhi'}
```

パラメータ既定：`$Width`=1280、`$Height`=720。

<a id="s105"></a>
**S105 — `Setup/Test-TPSAvoidance.ps1:18`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-RenderOffscreen','-ResX=1280','-ResY=720',('-TPSVisibilityScreens="'+(Join-Path $folder 'visibility')+'"'),'-unattended','-nosound','-BattleSmokeTest','-BattleSmokeSuite=TPS',('-abslog="'+$log+'"'))
```

<a id="s106"></a>
**S106 — `Setup/Test-TPSControl.ps1:18`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-nullrhi','-unattended','-nosound','-BattleSmokeTest','-BattleSmokeSuite=TPS',('-abslog="'+$log+'"'))
```

<a id="s107"></a>
**S107 — `Setup/Test-TPSVisibility.ps1:18`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-RenderOffscreen','-ResX=1280','-ResY=720',('-TPSVisibilityScreens="'+(Join-Path $folder 'visibility')+'"'),'-unattended','-nosound','-BattleSmokeTest','-BattleSmokeSuite=TPS','-BattleSightRange=2400',('-abslog="'+$log+'"'))
```

<a id="s108"></a>
**S108 — `Setup/Test-VATAdoptionLeader.ps1:13`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattlePrototype','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',('-abslog="'+$log+'"'))
```

<a id="s109"></a>
**S109 — `Setup/Test-VATAdoptionLeader.ps1:14`**

```powershell
if($UI) { $args+=@('-RenderOffscreen',"-ResX=$Width","-ResY=$Height",'-Windowed','-ForceRes',"-BattleLeaderUnits=$Units",'-BattleLeaderNoControl=1',('-BattleLeaderQA="'+(Join-Path $out 'list')+'"')) }
```

パラメータ既定：`$Width`=1920、`$Height`=1080、`$Units`=1000。

<a id="s110"></a>
**S110 — `Setup/Test-VATAdoptionLeader.ps1:15`**

```powershell
else { $args+=@('-nullrhi','-BattleLeaderSmoke') }
```

<a id="s111"></a>
**S111 — `Setup/Test-VATAdoptionLeader.ps1:16`**

```powershell
if($DisplayOff){$args+='-BattleDisplayLOD=0'}; $m.display_off=[bool]$DisplayOff;
```

<a id="s112"></a>
**S112 — `Setup/Test-VATDisplay.ps1:28`**

```powershell
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleLeaderUnits=$Units",('-abslog="'+$log+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'),"-P3Decision=$Decision","-P3View=$View","-P3Layout=$Layout","-P3Seed=$Seed","-P3Warmup=$Warmup","-P3Duration=$Duration",'-ResX=1920','-ResY=1080','-Windowed','-ForceRes','-NoVSync')
```

パラメータ既定：`$Units`=500、`$Decision`='Leader'、`$View`='RTS'、`$Layout`='Open'、`$Seed`=1、`$Warmup`=10、`$Duration`=60。

<a id="s113"></a>
**S113 — `Setup/Test-VATDisplay.ps1:29`**

```powershell
if($SightRange -gt 0){$args+="-BattleSightRange=$SightRange"}
```

パラメータ既定：`$SightRange`=0。

<a id="s114"></a>
**S114 — `Setup/Test-VATDisplay.ps1:35`**

```powershell
$args+="-BattleReuseSightContext=$ContextReuse"
```

パラメータ既定：`$ContextReuse`=0。

<a id="s115"></a>
**S115 — `Setup/Test-VATDisplay.ps1:36`**

```powershell
$args+="-BattleReduceHiddenBones=$HiddenBones"
```

パラメータ既定：`$HiddenBones`=0。

<a id="s116"></a>
**S116 — `Setup/Test-VATDisplay.ps1:37`**

```powershell
$args+="-BattlePerceptionGrid=$PerceptionGrid"
```

パラメータ既定：`$PerceptionGrid`=0。

<a id="s117"></a>
**S117 — `Setup/Test-VATDisplay.ps1:38`**

```powershell
$args+="-BattleReduceAnimTasks=$AnimTasks"
```

パラメータ既定：`$AnimTasks`=0。

<a id="s118"></a>
**S118 — `Setup/Test-VATDisplay.ps1:39`**

```powershell
$args+="-BattleDisplayLOD=$DisplayLOD"
```

パラメータ既定：`$DisplayLOD`=0。

<a id="s119"></a>
**S119 — `Setup/Test-WideMixed.ps1:23`**

```powershell
$args=@(('"'+$project+'"'),$map,'-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleDisplayLOD=$DisplayLOD",('-abslog="'+$log+'"'))
```

パラメータ既定：`$DisplayLOD`=1。

<a id="s120"></a>
**S120 — `Setup/Test-WideMixed.ps1:24`**

```powershell
if($LegacyLeader){$args+=@('-BattleLeaderUnits=40','-BattleLeaderSmoke')}else{$args+=@('-BattleWideMixed','-BattleLeaderUnits=500');$args+=if($Soak){'-BattleWideSoak'}else{'-BattleWideSmoke'}}
```

<a id="s121"></a>
**S121 — `Setup/Test-WideMixed.ps1:25`**

```powershell
if($Rendered){$args+=@('-RenderOffscreen','-ForceRes',"-ResX=$Width","-ResY=$Height",('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$args+='-nullrhi'}
```

パラメータ既定：`$Width`=1280、`$Height`=720。

<a id="s122"></a>
**S122 — `Setup/Test-WideSight.ps1:33`**

```powershell
$a=@(('"'+$project+'"'),"/Game/Prototype/Lvl_BattleWide_$Map",'-game','-unattended','-nosound','-nosplash','-BattleWideMixed','-BattleRangeAware=1',"-BattleSightRange=$SightRange",('-abslog="'+$log+'"'))
```

パラメータ既定：`$Map`='A'、`$SightRange`=5000。

<a id="s123"></a>
**S123 — `Setup/Test-WideSight.ps1:35`**

```powershell
$a+="-BattleSectorAI=$SectorAI"
```

パラメータ既定：`$SectorAI`=1。

<a id="s124"></a>
**S124 — `Setup/Test-WideSight.ps1:36`**

```powershell
if ($SectorContact -ge 0) { $a+="-BattleSectorContact=$SectorContact" }
```

パラメータ既定：`$SectorContact`=-1。

<a id="s125"></a>
**S125 — `Setup/Test-WideSight.ps1:37`**

```powershell
if ($SectorRoute -ge 0) { $a+="-BattleSectorRoute=$SectorRoute" }
```

パラメータ既定：`$SectorRoute`=-1。

<a id="s126"></a>
**S126 — `Setup/Test-WideSight.ps1:38`**

```powershell
if ($SectorFlank -ge 0) { $a+="-BattleSectorFlank=$SectorFlank" }
```

パラメータ既定：`$SectorFlank`=-1。

<a id="s127"></a>
**S127 — `Setup/Test-WideSight.ps1:39`**

```powershell
if ($SectorFront -ge 0) { $a+="-BattleSectorFront=$SectorFront" }
```

パラメータ既定：`$SectorFront`=-1。

<a id="s128"></a>
**S128 — `Setup/Test-WideSight.ps1:40`**

```powershell
if ($SectorSlotReserve -ge 0) { $a+="-BattleSectorSlotReserve=$SectorSlotReserve" }
```

パラメータ既定：`$SectorSlotReserve`=-1。

<a id="s129"></a>
**S129 — `Setup/Test-WideSight.ps1:44`**

```powershell
$a+=@("-BattleSquadFlank=$FlankEnabled","-BattleCoverActions=$CoverActions","-BattleFlankCoordination=$FlankCoordination","-BattleAICoverProbe=$AICoverProbe","-BattleCoverApproach=$CoverApproach","-BattleLeaderGuard=$LeaderGuard")
```

パラメータ既定：`$FlankEnabled`=1、`$CoverActions`=0、`$FlankCoordination`=0、`$AICoverProbe`=0、`$CoverApproach`=0、`$LeaderGuard`=0。

<a id="s130"></a>
**S130 — `Setup/Test-WideSight.ps1:45`**

```powershell
if($LeaderHealth -gt 0){$a+="-BattleLeaderHealth=$LeaderHealth"}
```

パラメータ既定：`$LeaderHealth`=0。

<a id="s131"></a>
**S131 — `Setup/Test-WideSight.ps1:47`**

```powershell
if($SupplyReturnLiving -ge 0){$a+="-BattleSupplyReturnLiving=$SupplyReturnLiving"};if($SupplyResumeLiving -ge 0){$a+="-BattleSupplyResumeLiving=$SupplyResumeLiving"}
```

パラメータ既定：`$SupplyReturnLiving`=-1、`$SupplyResumeLiving`=-1。

<a id="s132"></a>
**S132 — `Setup/Test-WideSight.ps1:48`**

```powershell
if($SupplyAwayRatio -ge 0){$a+=("-BattleSupplyAwayRatio="+$SupplyAwayRatio.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SupplyHeal -ge 0){$a+=("-BattleSupplyHeal="+$SupplyHeal.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$SupplyAwayRatio`=-1、`$SupplyHeal`=-1。

<a id="s133"></a>
**S133 — `Setup/Test-WideSight.ps1:49`**

```powershell
if($SupplyResumeHealth -ge 0){$a+=("-BattleSupplyResumeHealth="+$SupplyResumeHealth.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$SupplyResumeHealth`=-1。

<a id="s134"></a>
**S134 — `Setup/Test-WideSight.ps1:50`**

```powershell
if($ReacquireGrace -ge 0){$a+="-BattleReacquireGrace=$ReacquireGrace"}
```

パラメータ既定：`$ReacquireGrace`=-1。

<a id="s135"></a>
**S135 — `Setup/Test-WideSight.ps1:51`**

```powershell
if($CoverFire -ge 0){$a+="-BattleCoverFire=$CoverFire"}
```

パラメータ既定：`$CoverFire`=-1。

<a id="s136"></a>
**S136 — `Setup/Test-WideSight.ps1:52`**

```powershell
if($TerrainLineBodies -ge 0){$a+="-BattleTerrainLineBodies=$TerrainLineBodies"}
```

パラメータ既定：`$TerrainLineBodies`=-1。

<a id="s137"></a>
**S137 — `Setup/Test-WideSight.ps1:53`**

```powershell
if($ContactCover){$a+="-BattleContactCover=1";if($ContactCoverTeam -ge 0){$a+="-BattleContactCoverTeam=$ContactCoverTeam"}}
```

パラメータ既定：`$ContactCover`=0、`$ContactCoverTeam`=-1。

<a id="s138"></a>
**S138 — `Setup/Test-WideSight.ps1:54`**

```powershell
if($CoverValidation -ge 0){$a+="-BattleCoverValidation=$CoverValidation"}
```

パラメータ既定：`$CoverValidation`=-1。

<a id="s139"></a>
**S139 — `Setup/Test-WideSight.ps1:55`**

```powershell
if($CoverPeekSight -ge 0){$a+="-BattleCoverPeekSight=$CoverPeekSight"}
```

パラメータ既定：`$CoverPeekSight`=-1。

<a id="s140"></a>
**S140 — `Setup/Test-WideSight.ps1:56`**

```powershell
if($PerceptionOpt -eq 1){$a+='-BattleReuseSightContext=1','-BattlePerceptionGrid=1','-BattleReduceHiddenBones=1'}
```

パラメータ既定：`$PerceptionOpt`=0。

<a id="s141"></a>
**S141 — `Setup/Test-WideSight.ps1:58`**

```powershell
if($Routes -ge 0){$a+="-BattleRoutes=$Routes"}
```

パラメータ既定：`$Routes`=-1。

<a id="s142"></a>
**S142 — `Setup/Test-WideSight.ps1:60`**

```powershell
if($AreaPointSupply -ge 0){$a+="-BattleAreaPointSupply=$AreaPointSupply"};if($AreaDepot -ge 0){$a+="-BattleAreaDepot=$AreaDepot"};if($AreaDepotRaid -ge 0){$a+="-BattleAreaDepotRaid=$AreaDepotRaid"}
```

パラメータ既定：`$AreaPointSupply`=-1、`$AreaDepot`=-1、`$AreaDepotRaid`=-1。

<a id="s143"></a>
**S143 — `Setup/Test-WideSight.ps1:62`**

```powershell
if($PlayerRouteTest){$a+="-BattlePlayerRouteTest"}
```

<a id="s144"></a>
**S144 — `Setup/Test-WideSight.ps1:64`**

```powershell
if($HeightTest){$a+="-BattleHeightTest"}
```

<a id="s145"></a>
**S145 — `Setup/Test-WideSight.ps1:66`**

```powershell
if($SectorAudit){$a+="-BattleSectorAudit"}
```

<a id="s146"></a>
**S146 — `Setup/Test-WideSight.ps1:67`**

```powershell
if ($WaypointPass -ge 0) { $a+="-BattleWaypointPass=$WaypointPass" }
```

パラメータ既定：`$WaypointPass`=-1。

<a id="s147"></a>
**S147 — `Setup/Test-WideSight.ps1:68`**

```powershell
if ($SectorMoveFire -ge 0) { $a+="-BattleSectorMoveFire=$SectorMoveFire" }
```

パラメータ既定：`$SectorMoveFire`=-1。

<a id="s148"></a>
**S148 — `Setup/Test-WideSight.ps1:69`**

```powershell
if ($CoverHuddle -ge 0) { $a+="-BattleCoverHuddle=$CoverHuddle" }
```

パラメータ既定：`$CoverHuddle`=-1。

<a id="s149"></a>
**S149 — `Setup/Test-WideSight.ps1:70`**

```powershell
if ($SquadCoverCheck -ge 0) { $a+="-BattleSquadCoverCheck=$SquadCoverCheck" }
```

パラメータ既定：`$SquadCoverCheck`=-1。

<a id="s150"></a>
**S150 — `Setup/Test-WideSight.ps1:71`**

```powershell
if ($Bounding -ge 0) { $a+="-BattleBounding=$Bounding" }
```

パラメータ既定：`$Bounding`=-1。

<a id="s151"></a>
**S151 — `Setup/Test-WideSight.ps1:72`**

```powershell
if ($PinnedFlank -ge 0) { $a+="-BattlePinnedFlank=$PinnedFlank" }
```

パラメータ既定：`$PinnedFlank`=-1。

<a id="s152"></a>
**S152 — `Setup/Test-WideSight.ps1:73`**

```powershell
if ($ArrivalThreat -ge 0) { $a+="-BattleArrivalThreat=$ArrivalThreat" }
```

パラメータ既定：`$ArrivalThreat`=-1。

<a id="s153"></a>
**S153 — `Setup/Test-WideSight.ps1:74`**

```powershell
if ($SectorCoverRoute -ge 0) { $a+="-BattleSectorCoverRoute=$SectorCoverRoute" }
```

パラメータ既定：`$SectorCoverRoute`=-1。

<a id="s154"></a>
**S154 — `Setup/Test-WideSight.ps1:75`**

```powershell
if ($CoverStrict -ge 0) { $a+="-BattleCoverStrict=$CoverStrict" }
```

パラメータ既定：`$CoverStrict`=-1。

<a id="s155"></a>
**S155 — `Setup/Test-WideSight.ps1:76`**

```powershell
if ($ShotDirection -ge 0) { $a+="-BattleShotDirection=$ShotDirection" }
```

パラメータ既定：`$ShotDirection`=-1。

<a id="s156"></a>
**S156 — `Setup/Test-WideSight.ps1:77`**

```powershell
if ($DangerRoute -ge 0) { $a+="-BattleDangerRoute=$DangerRoute" }
```

パラメータ既定：`$DangerRoute`=-1。

<a id="s157"></a>
**S157 — `Setup/Test-WideSight.ps1:78`**

```powershell
if ($CloseIn -ge 0) { $a+="-BattleCloseIn=$CloseIn" }
```

パラメータ既定：`$CloseIn`=-1。

<a id="s158"></a>
**S158 — `Setup/Test-WideSight.ps1:79`**

```powershell
if ($SectorCoverWalk -ge 0) { $a+="-BattleSectorCoverWalk=$SectorCoverWalk" }
```

パラメータ既定：`$SectorCoverWalk`=-1。

<a id="s159"></a>
**S159 — `Setup/Test-WideSight.ps1:80`**

```powershell
if ($UnderFireCover -ge 0) { $a+="-BattleUnderFireCover=$UnderFireCover" }
```

パラメータ既定：`$UnderFireCover`=-1。

<a id="s160"></a>
**S160 — `Setup/Test-WideSight.ps1:81`**

```powershell
if ($NoCoverFallback -ge 0) { $a+="-BattleNoCoverFallback=$NoCoverFallback" }
```

パラメータ既定：`$NoCoverFallback`=-1。

<a id="s161"></a>
**S161 — `Setup/Test-WideSight.ps1:82`**

```powershell
if ($CoverPriority -ge 0) { $a+="-BattleCoverPriority=$CoverPriority" }
```

パラメータ既定：`$CoverPriority`=-1。

<a id="s162"></a>
**S162 — `Setup/Test-WideSight.ps1:83`**

```powershell
$a+=@("-BattleAreaCompositionPlanner=$AreaCompositionPlanner","-BattleAreaCompositionPlannerTeams=$AreaCompositionPlannerTeams")
```

パラメータ既定：`$AreaCompositionPlanner`=0、`$AreaCompositionPlannerTeams`=0。

<a id="s163"></a>
**S163 — `Setup/Test-WideSight.ps1:84`**

```powershell
if($AreaHire -ge 0){$a+="-BattleAreaHire=$AreaHire"}
```

パラメータ既定：`$AreaHire`=-1。

<a id="s164"></a>
**S164 — `Setup/Test-WideSight.ps1:85`**

```powershell
if($AreaIncome -ge 0){$a+="-BattleAreaIncome=$AreaIncome"}
```

パラメータ既定：`$AreaIncome`=-1。

<a id="s165"></a>
**S165 — `Setup/Test-WideSight.ps1:86`**

```powershell
if($AreaStrategy -ge 0){$a+="-BattleAreaStrategy=$AreaStrategy"};if($AreaAttackWeight -ge 0){$a+=("-BattleAreaAttackWeight="+$AreaAttackWeight.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseShare -ge 0){$a+=("-BattleAreaBaseShare="+$AreaBaseShare.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$AreaStrategy`=-1、`$AreaAttackWeight`=-1、`$AreaBaseShare`=-1。

<a id="s166"></a>
**S166 — `Setup/Test-WideSight.ps1:87`**

```powershell
if($AreaBaseRule -ge 0){$a+="-BattleAreaBaseRule=$AreaBaseRule"};if($AreaBaseShareLow -ge 0){$a+=("-BattleAreaBaseShareLow="+$AreaBaseShareLow.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseShareHigh -ge 0){$a+=("-BattleAreaBaseShareHigh="+$AreaBaseShareHigh.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$AreaBaseRule`=-1、`$AreaBaseShareLow`=-1、`$AreaBaseShareHigh`=-1。

<a id="s167"></a>
**S167 — `Setup/Test-WideSight.ps1:88`**

```powershell
if($AreaHeavyShare -ge 0){$a+=("-BattleAreaHeavyShare="+$AreaHeavyShare.ToString([Globalization.CultureInfo]::InvariantCulture))};if($ReturnFire -ge 0){$a+="-BattleReturnFire=$ReturnFire"};if($AreaIncomeUrbanBase -ge 0){$a+="-BattleAreaIncomeUrbanBase=$AreaIncomeUrbanBase"};if($AreaIncomeUrbanPerPoint -ge 0){$a+="-BattleAreaIncomeUrbanPerPoint=$AreaIncomeUrbanPerPoint"};if($AreaPlan -ge 0){$a+="-BattleAreaPlan=$AreaPlan"};if($AreaHirePurpose -ge 0){$a+="-BattleAreaHirePurpose=$AreaHirePurpose"};if($AreaIncomeBase -ge 0){$a+="-BattleAreaIncomeBase=$AreaIncomeBase"};if($AreaIncomePerPoint -ge 0){$a+="-BattleAreaIncomePerPoint=$AreaIncomePerPoint"};if($AreaIncomeRamp -ge 0){$a+=("-BattleAreaIncomeRamp="+$AreaIncomeRamp.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaStartCredits -ge 0){$a+="-BattleAreaStartCredits=$AreaStartCredits"};if($AreaBaseFlankSide -ge 0){$a+="-BattleAreaBaseFlankSide=$AreaBaseFlankSide"};if($AreaBaseReturnLiving -ge 0){$a+="-BattleAreaBaseReturnLiving=$AreaBaseReturnLiving"};if($AreaBaseFlank -ge 0){$a+=("-BattleAreaBaseFlank="+$AreaBaseFlank.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaTrialTeam -ge 0){$a+="-BattleAreaTrialTeam=$AreaTrialTeam"};if($AreaFocus -ge 0){$a+=("-BattleAreaFocus="+$AreaFocus.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaBaseInfiltrate -ge 0){$a+=("-BattleAreaBaseInfiltrate="+$AreaBaseInfiltrate.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaFrontBalance -ge 0){$a+="-BattleAreaFrontBalance=$AreaFrontBalance"};if($AreaFrontGain -ge 0){$a+=("-BattleAreaFrontGain="+$AreaFrontGain.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$AreaHeavyShare`=-1、`$ReturnFire`=-1、`$AreaIncomeUrbanBase`=-1、`$AreaIncomeUrbanPerPoint`=-1、`$AreaPlan`=-1、`$AreaHirePurpose`=-1、`$AreaIncomeBase`=-1、`$AreaIncomePerPoint`=-1、`$AreaIncomeRamp`=-1、`$AreaStartCredits`=-1、`$AreaBaseFlankSide`=-1、`$AreaBaseReturnLiving`=-1、`$AreaBaseFlank`=-1、`$AreaTrialTeam`=-1、`$AreaFocus`=-1、`$AreaBaseInfiltrate`=-1、`$AreaFrontBalance`=-1、`$AreaFrontGain`=-1。

<a id="s168"></a>
**S168 — `Setup/Test-WideSight.ps1:89`**

```powershell
if($AreaMaxSquads -ge 0){$a+="-BattleAreaMaxSquads=$AreaMaxSquads"};if($AreaHireReserve -ge 0){$a+="-BattleAreaHireReserve=$AreaHireReserve"};if($AreaHireInterval -ge 0){$a+=("-BattleAreaHireInterval="+$AreaHireInterval.ToString([Globalization.CultureInfo]::InvariantCulture))};if($AreaHireStop -ge 0){$a+=("-BattleAreaHireStop="+$AreaHireStop.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$AreaMaxSquads`=-1、`$AreaHireReserve`=-1、`$AreaHireInterval`=-1、`$AreaHireStop`=-1。

<a id="s169"></a>
**S169 — `Setup/Test-WideSight.ps1:90`**

```powershell
if($AreaWarScore -ge 0){$a+="-BattleAreaWarScore=$AreaWarScore"};if($AreaWarTarget -ge 0){$a+="-BattleAreaWarTarget=$AreaWarTarget"}
```

パラメータ既定：`$AreaWarScore`=-1、`$AreaWarTarget`=-1。

<a id="s170"></a>
**S170 — `Setup/Test-WideSight.ps1:92`**

```powershell
if($RangeProfile -ge 0){$a+="-BattleRangeProfile=$RangeProfile"};if($RangeSight -ge 0){$a+="-BattleRangeSight=$RangeSight"};if($RangeEngage -ge 0){$a+="-BattleRangeEngage=$RangeEngage"}
```

パラメータ既定：`$RangeProfile`=-1、`$RangeSight`=-1、`$RangeEngage`=-1。

<a id="s171"></a>
**S171 — `Setup/Test-WideSight.ps1:93`**

```powershell
if($RangeEngageSupport -ge 0){$a+="-BattleRangeEngageSupport=$RangeEngageSupport"};if($RangeReach -ge 0){$a+="-BattleRangeReach=$RangeReach"}
```

パラメータ既定：`$RangeEngageSupport`=-1、`$RangeReach`=-1。

<a id="s172"></a>
**S172 — `Setup/Test-WideSight.ps1:94`**

```powershell
if($Stances -ge 0){$a+="-BattleStances=$Stances"};if($CoverArrival -ge 0){$a+="-BattleCoverArrival=$CoverArrival"}
```

パラメータ既定：`$Stances`=-1、`$CoverArrival`=-1。

<a id="s173"></a>
**S173 — `Setup/Test-WideSight.ps1:96`**

```powershell
if($SuppressionSupport -ge 0){$a+=("-BattleSuppressionSupport="+$SuppressionSupport.ToString([Globalization.CultureInfo]::InvariantCulture))};if($Suppression -ge 0){$a+="-BattleSuppression=$Suppression"};if($SuppressionPin -ge 0){$a+=("-BattleSuppressionPin="+$SuppressionPin.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SuppressionSpread -ge 0){$a+=("-BattleSuppressionSpread="+$SuppressionSpread.ToString([Globalization.CultureInfo]::InvariantCulture))};if($SuppressionPerShot -ge 0){$a+=("-BattleSuppressionPerShot="+$SuppressionPerShot.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$SuppressionSupport`=-1、`$Suppression`=-1、`$SuppressionPin`=-1、`$SuppressionSpread`=-1、`$SuppressionPerShot`=-1。

<a id="s174"></a>
**S174 — `Setup/Test-WideSight.ps1:98`**

```powershell
if($AreaScoutKeep -ge 0){$a+="-BattleAreaScoutKeep=$AreaScoutKeep"}
```

パラメータ既定：`$AreaScoutKeep`=-1。

<a id="s175"></a>
**S175 — `Setup/Test-WideSight.ps1:100`**

```powershell
if($AreaIntel -ge 0){$a+="-BattleAreaIntel=$AreaIntel"};if($AreaATKeep -ge 0){$a+="-BattleAreaATKeep=$AreaATKeep"};if($AreaTankKeep -ge 0){$a+="-BattleAreaTankKeep=$AreaTankKeep"};if($AreaACKeep -ge 0){$a+="-BattleAreaACKeep=$AreaACKeep"};if($AreaMortarKeep -ge 0){$a+="-BattleAreaMortarKeep=$AreaMortarKeep"};if($AreaSPGKeep -ge 0){$a+="-BattleAreaSPGKeep=$AreaSPGKeep"};if($AreaExoKeep -ge 0){$a+="-BattleAreaExoKeep=$AreaExoKeep"};if($ArmourAI -ge 0){$a+="-BattleArmourAI=$ArmourAI"};if($ArmourAITeam -ge 0){$a+="-BattleArmourAITeam=$ArmourAITeam"};if($AreaComposition -ge 0){$a+="-BattleAreaComposition=$AreaComposition"};if($AreaGroups -ge 0){$a+="-BattleAreaGroups=$AreaGroups"};if($DefendEngage -ge 0){$a+="-BattleDefendEngage=$DefendEngage"};if($SymmetricStart -ge 0){$a+="-BattleSymmetricStart=$SymmetricStart"};if($SymmetryAudit){$a+='-BattleSymmetryAudit'};if($ScoutRetreat -ge 0){$a+="-BattleScoutRetreat=$ScoutRetreat"}
```

パラメータ既定：`$AreaIntel`=-1、`$AreaATKeep`=-1、`$AreaTankKeep`=-1、`$AreaACKeep`=-1、`$AreaMortarKeep`=-1、`$AreaSPGKeep`=-1、`$AreaExoKeep`=-1、`$ArmourAI`=-1、`$ArmourAITeam`=-1、`$AreaComposition`=-1、`$AreaGroups`=-1、`$DefendEngage`=-1、`$SymmetricStart`=-1、`$ScoutRetreat`=-1。

<a id="s176"></a>
**S176 — `Setup/Test-WideSight.ps1:102`**

```powershell
foreach($hv in @(@("Members",$HeavyMembers),@("Health",$HeavyHealth),@("Speed",$HeavySpeed),@("Cost",$HeavyCost),@("Damage",$HeavyDamage))){if($hv[1] -ge 0){$a+=("-BattleHeavy"+$hv[0]+"="+([double]$hv[1]).ToString([Globalization.CultureInfo]::InvariantCulture))}}
```

パラメータ既定：`$HeavyMembers`=-1、`$HeavyHealth`=-1、`$HeavySpeed`=-1、`$HeavyCost`=-1、`$HeavyDamage`=-1。

<a id="s177"></a>
**S177 — `Setup/Test-WideSight.ps1:103`**

```powershell
if($RifleMembers -ge 2){$a+="-BattleRifleMembers=$RifleMembers"}
```

パラメータ既定：`$RifleMembers`=-1。

<a id="s178"></a>
**S178 — `Setup/Test-WideSight.ps1:104`**

```powershell
if($AllyPass -ge 0){$a+="-BattleAllyPass=$AllyPass"}
```

パラメータ既定：`$AllyPass`=-1。

<a id="s179"></a>
**S179 — `Setup/Test-WideSight.ps1:105`**

```powershell
if($CorpseLifetime -ge 0){$a+="-BattleCorpseLifetime=$CorpseLifetime"}
```

パラメータ既定：`$CorpseLifetime`=-1。

<a id="s180"></a>
**S180 — `Setup/Test-WideSight.ps1:106`**

```powershell
$a+=@("-BattleFlankRelaxed=$FlankRelaxed","-BattleFlankTeams=$FlankTeams")
```

パラメータ既定：`$FlankRelaxed`=0、`$FlankTeams`=3。

<a id="s181"></a>
**S181 — `Setup/Test-WideSight.ps1:107`**

```powershell
if($FlankTrial){$a+='-BattleFlankTrial'}
```

<a id="s182"></a>
**S182 — `Setup/Test-WideSight.ps1:108`**

```powershell
if($ObserveTactics){$a+='-BattleTacticalObserve'}
```

<a id="s183"></a>
**S183 — `Setup/Test-WideSight.ps1:109`**

```powershell
if($Area){$a+='-BattleArea'}
```

<a id="s184"></a>
**S184 — `Setup/Test-WideSight.ps1:110`**

```powershell
if($RoundTripSave){if(!$Experiment){throw 'RoundTripSave requires Experiment'};$a+='-BattleVerifyMixedSave'}
```

<a id="s185"></a>
**S185 — `Setup/Test-WideSight.ps1:111`**

```powershell
if($WeaponRange){$a+="-BattleWeaponRange=$WeaponRange"}
```

パラメータ既定：`$WeaponRange`=0。

<a id="s186"></a>
**S186 — `Setup/Test-WideSight.ps1:112`**

```powershell
if($Swap -and !$Experiment){$a+='-BattleTrialSwap'}
```

<a id="s187"></a>
**S187 — `Setup/Test-WideSight.ps1:113`**

```powershell
if($Visual){$a+='-BattleSightVisual';if($FirstPerson){$a+='-BattleFirstPerson'}}elseif($Experiment){$a+=@(('-BattleSightExperiment="'+(Join-Path $out 'metrics.json')+'"'),"-BattleTrialSeed=$Seed",'-UseFixedTimeStep',"-FPS=$Fps");if($Swap){$a+='-BattleTrialSwap'}}else{$a+='-BattleWideMapAudit'}
```

パラメータ既定：`$Seed`=7、`$Fps`=60。

<a id="s188"></a>
**S188 — `Setup/Test-WideSight.ps1:114`**

```powershell
if($GeometryOnly){$a+='-BattleWideMapGeometryOnly'}
```

<a id="s189"></a>
**S189 — `Setup/Test-WideSight.ps1:115`**

```powershell
if($Rendered){$a+=@('-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',('-BattleWideScreens="'+(Join-Path $out 'screen')+'"'))}else{$a+='-nullrhi'}
```

<a id="s190"></a>
**S190 — `Setup/Test-WideSight.ps1:119`**

```powershell
if($SightPairReuse -ge 0){$a+="-BattleSightPairReuse=$SightPairReuse"}
```

パラメータ既定：`$SightPairReuse`=-1。

<a id="s191"></a>
**S191 — `Setup/Test-WideSight.ps1:120`**

```powershell
if($SightHitStats -ge 0){$a+="-BattleSightHitStats=$SightHitStats"};if($SightParallel -ge 0){$a+="-BattleSightParallel=$SightParallel"};if($SightTeamMask -ge 0){$a+="-BattleSightTeamMask=$SightTeamMask"}
```

パラメータ既定：`$SightHitStats`=-1、`$SightParallel`=-1、`$SightTeamMask`=-1。

<a id="s192"></a>
**S192 — `Setup/Test-WideSight.ps1:122`**

```powershell
if($SightScanInterval -ge 0){$a+=("-BattleSightScanInterval="+$SightScanInterval.ToString([Globalization.CultureInfo]::InvariantCulture))}
```

パラメータ既定：`$SightScanInterval`=-1。

<a id="s193"></a>
**S193 — `Setup/Test-WideSight.ps1:124`**

```powershell
if($SightSlowFar -ge 0){$a+="-BattleSightSlowFar=$SightSlowFar"}
```

パラメータ既定：`$SightSlowFar`=-1。

<a id="s194"></a>
**S194 — `Setup/Test-WideSight.ps1:127`**

```powershell
if($ThinkCap -ge 0){$a+="-BattleThinkCap=$ThinkCap"}
```

パラメータ既定：`$ThinkCap`=-1。

<a id="s195"></a>
**S195 — `Setup/Test-WideSight.ps1:128`**

```powershell
if($HireSightRefresh -ge 0){$a+="-BattleHireSightRefresh=$HireSightRefresh"}
```

パラメータ既定：`$HireSightRefresh`=-1。

<a id="s196"></a>
**S196 — `Setup/Test-WideSight.ps1:130`**

```powershell
if($VATFarLight -ge 0){$a+="-BattleVATFarLight=$VATFarLight"}
```

パラメータ既定：`$VATFarLight`=-1。

<a id="s197"></a>
**S197 — `Setup/Test-WideSight.ps1:132`**

```powershell
if($FollowerAnimSkip -ge 0){$a+="-BattleFollowerAnimSkip=$FollowerAnimSkip"}
```

パラメータ既定：`$FollowerAnimSkip`=-1。

<a id="s198"></a>
**S198 — `Setup/Test-WideSight.ps1:134`**

```powershell
if($FollowerAnimBudget -ge 0){$a+="-BattleFollowerAnimBudget=$FollowerAnimBudget"}
```

パラメータ既定：`$FollowerAnimBudget`=-1。

<a id="s199"></a>
**S199 — `Setup/Test-WideSight.ps1:136`**

```powershell
if($FollowSightParallel -ge 0){$a+="-BattleFollowSightParallel=$FollowSightParallel"}
```

パラメータ既定：`$FollowSightParallel`=-1。

<a id="s200"></a>
**S200 — `Setup/Test-WideSight.ps1:138`**

```powershell
if($FollowerNoOverlap -ge 0){$a+="-BattleFollowerNoOverlap=$FollowerNoOverlap"}
```

パラメータ既定：`$FollowerNoOverlap`=-1。


## 6. 後続の点検へのメモ

- **観測情報ルール**：BattlePerception.cpp:132の識別取得とPlanner/GroupStrengthの境界。BattleAreaIntel.cpp:34の距離だけのbHomeCovered、:51からKnownVehiclesへ真の車両分類を引く経路。ArtilleryObserved ON/OFFで実観測と「近くに味方」を分ける。WeaponOrderGateが観測制限の代わりになっていないか。
- **再現性**：BattleLeaderRuntime.cpp:918–951の両側共有探索枠、TMapのFollow順、FPS依存枠、ContactCover片側の機会変化。BattleTactics.cpp:207の射撃列とBattleArtillery.cpp:323の砲撃列。Test-WideSight.ps1:33–43の暗黙値、RangeProfileによる旧視界引数の無効化、AreaGroupsの明示1と省略の非同値を比較キーに含める。乱数seed一致だけで同条件としない。
- **保存読み込み**：BattleSave.cpp:223–237の適合検査にない群/装甲/区画/遮蔽切替を列挙し、v3群あり保存＋Groups=0を重点にする。v4 RulesKeyの生引数と実効値の差（case、同値Clamp、重複引数、マップ、static Definitions）を調べる。UPROPERTY追加と「OFFなら保存形不変」の契約を分離する。前回F02の区画任務復帰も継続。
- **性能**：CoverPeekShotsのOFF時更新、AreaIntel OFFでも既定群が行う集約、CoverPriority ON/CoverActions OFFのShotFrom記録、試験専用Definitions構築を分けて計測する。static化だけで全て軽くなると考えず、World切替で保持される表も調べる。計測を実施する回にはMEASURE.mdに従い開始前のGet-MeasurementMachineをmanifest.machineへ保存する。本点検に計測値はない。

実装・Config・STATUS・journalは変更していない。本報告の提案は未実装であり、試験合格・旧挙動との完全同一を認定するものではない。
