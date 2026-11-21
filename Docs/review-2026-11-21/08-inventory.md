# FutureBattle 総点検08：試験切替・未使用コード・試験範囲・資料

2026-11-21。読取対象HEAD：`9e9e7660da0f4c248d2e0652b6617564ae4f26aa`。開始時の `git status --short -uno` に変更表示なし（ユーザーのglobal ignoreを読めない警告あり）。ビルド、UE起動、試合、スモーク、生成器は実行していない。書き込みは本報告だけ。STATUSの整理・更新も行っていない。

短縮した `Battle*.cpp/.h` は `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。行番号は今回読んだファイルの番号。削減行数は物理行数（コメント・括弧を含む）で、速度改善量ではない。複数用途の行は全行削除と数えない。

## 1. 結論と先行点検の扱い

優先すべきは切替の一括削除ではなく、**試験が保証する範囲の明示、失敗ロードと任務遷移の回帰追加、現在地の資料修正**である。確実な未使用通常関数は2件・宣言込み20行。削除候補の大きな実験はFollowSightParallelとReduceAnimTasksだが、比較用入口も含めた廃止が必要。効果が小さいVATFarLightには表示が試合を変える問題の再現用途が残る。

出発点は01のF02/F03/F04/F09と未使用整理、02の切替境界、03の観測固定試験、04の状態同値と統計同値の区別、05の失敗原子性・プレイヤー経路、06の実購入後の意味、07の後続メモである。**02の全280項目台帳とD10は[02-defaults.md](02-defaults.md)参照とし、再作成しない。** 本報告はその台帳に重ねる採否・保守上の分類であり、モード指定、数値調整、診断、資産インポートまでを「未採用の戦術」と数えない。

先行報告の更新状況も区別した。

| 対象 | 今回の扱い・根拠 | 確信度 |
|---|---|---|
| 01 F01／02 D03のPlanner識別 | `BattlePerception.cpp:64–65`にはPlanner条件がある。未修正扱いしない。陣営マスクによる隔離は06 C07の別問題 | 高 |
| 04 R01の実時間率 | `Setup/Compare-MatchResults.py:10–12`で3項目は除外済み。必須schema・完走・全状態を見ないR02は残る | 高 |
| 05 S01の失敗ロード | stagingとValidatedPathsは存在する。しかし `BattleMixedBodies.cpp:75–77` のResetは経路検証・stagingより前に残る。全面的に修正済みとも、stagingがないとも書かない | 高 |
| 旧peek保存失敗 | `STATUS.md:45`に修正済みと原因がある。新しいPeekRangeCheck試験切替とは別件。MEASUREの「未調査」は古い | 高 |

## 2. 切替の採否分類

### 分類の基準

- **A：評価済み・判断待ち**。少数試行や古い条件での評価も明記する。「既定化可能」の認定ではない。
- **B：評価済み・効果なし／悪化／利益が小さいため削除候補**。全用途で無意味とは断定しない。削除量と既定不変の条件は後表。
- **C：未評価**。機構スモークの存在と、戦術・性能の評価を分ける。比較条件の変更で再評価が必要なものも併記。
- **D：Setupスクリプト・STATUSに利用参照が見つからない**。「誰も使っていない」という実利用の断定はしない。設定専用口・内部バッチによる利用を除外する。
- **E：採用済み**。OFF分岐の常設価値を再検討できる。保存互換・旧基準・切り分けの用途があるものは保持する。

以下の名前は原則 `Battle` 接頭辞を省略する。関連する数値・Teams・Team・Keep等は親機能と一緒に扱う。既定値の正確な解決規則は02台帳と実装が根拠であり、Setup未指定＝直接起動未指定ではない。

### A／B：評価があるもの（記録の存在は高、一般化は別）

| 切替・系列 | 分類と判断 | 評価記録／実装の入口 | 確信度 |
|---|---|---|---|
| FollowerAnimSkip、FollowerAnimBudget | A。既定OFF。見た目確認待ち。BudgetはSkipを暗黙ON。Play-AreaだけSkip=1 | `STATUS.md:13`、`PERFORMANCE.md:14–18`、`ROADMAP.md:73`、`BattleVATDisplay.cpp:40–91`、`Setup/Play-Area.ps1:22`。旧E条件で約0.8〜0.9ms改善、同じ記録の試合 | 高 |
| FollowerNoOverlap | A。既定OFF。旧Eで約0.3ms、試合一致の記録。全表示・生成経路での採用判断は未了 | `STATUS.md:803–826`、`PERFORMANCE.md:15`、`BattleLeaderRuntime.cpp:810–816`、`BattleMixedBodies.cpp:18` | 高 |
| WaypointPass | A。Iの詰まり改善。H/A/E/Fを測ってから判断という承認済み順序 | `ROADMAP.md:44,73,78`、`BattleStableMovement.cpp`（02台帳の入口）。共通移動なので既定化は全マップの試合を変え得る | 高 |
| SectorContact／SectorRoute／SectorFlank／SectorFront／SectorSlotReserve／SectorMoveFire／CoverHuddle／SquadCoverCheck／Bounding／PinnedFlank／ArrivalThreat／SectorCoverRoute | A。ただし組合せ評価であり個別の採否は未確定。PinnedFlankは発動0の試行もありC相当の穴。BoundingはF03を先に検証 | `ROADMAP.md:77`、`STATUS.md:215–245`、`BattleSectorRoute.cpp:1`、`BattleLeaderRuntime.cpp:608–797,868–900`、`Setup/Play-Area.ps1:15–19`。PlayのSectorTrialsはSquadCoverCheck/Boundingを外している | 高 |
| CoverStrict／ShotDirection／DangerRoute／CloseIn／SectorCoverWalk／UnderFireCover | A（改善未確認の部分は保留）。基準変更、単独と組合せ、修正前後を分ける。直ちに全部削除とはしない | `STATUS.md:113–188`の区画AI比較、`BattleCoverAction.cpp:248–249`、`BattleLeaderRuntime.cpp:608–797`、`Setup/Play-Area.ps1:19`。特に歩く・詰める枝は低発動の記録 | 高 |
| SightTeamMask | A。視線の意味が変わる。単純な性能最適化扱いは不可 | `ROADMAP.md:73,85`、`STATUS.md:163–180`、`BattleLeaderRuntime.cpp:318,386` | 高 |
| DefendEngage | Aだが「切替のまま」が決定済み。再び即決を求める判断待ちには戻さない | `ROADMAP.md:73`、`STATUS.md:1281–1297`、`BattleTactics.h:115`。旧3seedの改善と採用は別 | 高 |
| AreaComposition | A。旧3seedの+202は相手が歩兵だけ、車両Nav修正前の制限もある。引き算評価後に判断 | `ROADMAP.md:34,80`、`STATUS.md:1270,1321–1325`、`BattleArmourAI.cpp:234–245` | 高 |
| FollowSightParallel | B。既定OFF、旧E比較で効果なし・試合変更。知覚のSightParallelとは別物 | `STATUS.md:803–826`、`BattleLeaderRuntime.cpp:801–809,817–863`。先読みはループ前の位置で判定 | 高 |
| ReduceAnimTasks | B。既定OFF、不採用の記録。旧1000体で約0.72〜0.94ms悪化 | `Docs/journal/HANDOFF-2026-11-04_11-13.md:586`、`BattleLeaderRuntime.cpp:94–97,1318`、`BattleAnimTasks.cpp:6–27` | 高 |
| VATFarLight | B。ただし再現標本として当面保持する価値あり。旧Eで約0.1ms、9846→9900発。表示と試合の結合は未解明 | `STATUS.md:914–916,254`、`BattleVATDisplay.cpp:30–39,102–108,250–265,312`、`ROADMAP.md:33` | 高 |
| AreaDepot、AreaDepotRaid | B（現行値）。Fの最初の2組で悪化。集積所という機能自体が不要という証拠ではない | `STATUS.md:886–902`、`BattleAreaDepot.cpp:20–49`。新しい値を試す計画なら残す | 高 |
| NoCoverFallback | B寄りの保留。効果小という要約。射撃位置探索と伏せの複合機能で、修正前の評価から無効と断定しない | `STATUS.md:20,82–102`、`BattleLeaderRuntime.cpp:918–956`。削除より現行版の機能分離評価が先 | 高 |
| LeaderGuard | Bの履歴はあるが削除保留。過去は効果小で不採用、現在はH/IのPlayが明示ONにしている | `Docs/journal/HANDOFF-2026-11-04_11-13.md:5`、`Setup/Play-Area.ps1:11`、`BattleStableMovement.cpp:238`、`BattleLeaderRuntime.cpp:921–952` | 高 |
| ThinkCap | C主体。開始ピークで上限8を試した記録はあるが、主因解消後の一斉命令用途の評価は未実施 | `STATUS.md:1028`、`BattleLeaderRuntime.cpp:1353–1361`。判断時刻が変わるので同値最適化とはしない | 高 |
| SightPairReuse／SightSlowFar | A相当の過去比較あり・採用未了。性能と試合結果を分けて再判断。PairReuseはSightParallelを無効にする条件 | `Docs/journal/STATUS-2026-11-17.md:142,189`、`BattleLeaderRuntime.cpp:108–110,341–352`、`BattlePerception.cpp:162` | 高 |

### C：未評価・評価条件不足として残すもの

| 切替・系列 | 根拠と必要な評価 | 確信度 |
|---|---|---|
| PeekRangeCheck、WeaponOrderGate、ArmourSectorGate、ArtilleryObserved、SPGAmmo、BlastOcclusion、ArtilleryClusters、ExoShieldSuppression、GroupStrength | `STATUS.md:41–42`は「どれも試合では未評価」。入口は `BattleCoverAction.cpp:22`、`BattleWeaponOrders.cpp:20`、`BattleArmourAI.cpp:58`、`BattleArtillery.cpp:198–313`、`BattleGroups.cpp:474`。機構試験があっても、主砲漏れ・実観測・群参加を保証しない。既定0を保持 | 高 |
| AreaCompositionPlanner、AreaCompositionPlannerTeams | `STATUS.md:24–28`でON試合未実施。`BattleComposition.cpp:8–13`、`BattleArea.cpp:145`、`BattleCompositionRuntime.cpp:56`。純粋Decide／codec試験と実購入・命令・収入窓を分離して評価する | 高 |
| AIBlindFire | `ROADMAP.md:73,83`、`BattleBlindFire.cpp:1–32`。プレイヤーの描画・発射試験の成功をAI採用評価へ転用しない | 高 |
| AreaPointSupply | `ROADMAP.md:84`、`BattleTactics.cpp:39`、`Setup/Play-Area.ps1:22`。PlayはON、直接起動はOFF。勝敗への影響未計測 | 高 |
| AreaIntel、ScoutRetreat、Area*Keepの兵種構成、AreaWarScore／WarTarget等 | `ROADMAP.md:10,34`、`STATUS.md:9`、`BattleTactics.cpp:94–101`、`BattleWarScore.cpp:1`。実装・局所試験はあるが、全兵種の引き算評価を完了していない。Keepは調整値であり、一律削除する実験枝ではない | 高 |
| AICoverProbe、ContactCover／ContactCoverTeam、CoverActions、FlankRelaxed／FlankTeams、FlankCoordination、ReturnFire | 試行・プレイ使用はあるが、現在のI／新既定／保存条件での個別採否は未確定。`BattleTactics.cpp:8–10,165–198`、`Setup/Play-Area.ps1:22`、`Docs/journal/PROTOTYPE-2026-11-04_11-13.md:25`、`STATUS.md:1433`。旧片側試験はS4/S6偏りの影響がある | 中（現条件での評価網羅性） |
| AreaHirePurpose／AreaPlan／AreaBaseRule／AreaBaseInfiltrateと関連係数、SupplyReturnLiving／SupplyResumeLiving／SupplyAwayRatio | 過去試験あり。ただし用途や時期が異なる。供給A/Dは悪化、Cは効果なしの履歴、EH/ESA等の片側結果は偏り修正前という制限。現行条件ではC＝再評価待ちとして扱う。`Docs/journal/HANDOFF-2026-11-04_11-13.md:7`、`STATUS.md:1433`、`BattleTactics.cpp:27–102`。調整口を削って既定値に固定する提案はしない | 中 |

### E：採用済みとOFF経路の用途

| 切替・系列 | 現在の扱い／OFFを残す意味 | 根拠・確信度 |
|---|---|---|
| ReuseSightContext、ReduceHiddenBones、PerceptionGrid | 採用済み。長期的にはOFFを常設する価値は低下。ただし旧監査はON/OFFを比べるので、その代替試験を決めてから廃止 | `DefaultGame.ini:84–86`、`BattleLeaderRuntime.cpp:98–105`、`Setup/Legacy/README.md:15`。高 |
| SightParallel、見回し0.4秒 | 採用済み。並列不具合切り分けの直列経路と、PairReuse併用条件があるため今は保持 | `STATUS.md:10`、`BattlePerception.cpp:162`、02の既定ON表。高 |
| ReacquireGrace、CoverFire、CoverValidation、CoverPeekSight | 採用済み。旧動作への比較スイッチを常設する価値は薄くなっている。保存FeatureMask・局所スモーク・旧比較の移行が必要 | `BattleTactics.cpp:140–163`、`BattleCoverActionSmoke.cpp:183–185`、`BattleSave.cpp:223–238`。高 |
| AreaHire／AreaIncome／AreaStrategy／AreaFrontBalance、AreaFocus、FS13BR5のFlank/Side/ReturnLiving | 採用済み。FS13BR5は再評価で効果確認できずともユーザーが維持。無効化して「不要な効果を掃除」してはいけない。経済の比較条件としてOFFに用途あり | `STATUS.md:9,1431`、`BattleTactics.h:64–95`、`BattleTactics.cpp:50–128`。高 |
| AreaGroups、SymmetricStart | 全マップ／市街地マップで採用済み。旧基準復元と片側比較のためOFF保持。Groups=0は既存群の運用停止ではない（02 D02） | `ROADMAP.md:73,81,88`、`STATUS.md:52–56`、`BattleTactics.h:105–108`。高 |
| CoverPriority | 11-21採用済み。まだ直近の変更なので比較のためOFFを保持。要約の採用済み行を優先 | `STATUS.md:20`、`BattleLeaderRuntime.cpp:918–952`。高 |
| ArmourAI、StanceAnim、BlindFireExpose、DisplayLOD、StableMovement／ContactAvoidance／RangeAware | 採用済み。旧経路・表現fallback・機能境界の試験に用途あり。特にStanceAnimだけで全旧表示に戻るとは限らない（02 D09）。今すぐOFF枝を消す候補にはしない | `BattleTactics.h:101`、`BattleStance.cpp:35–39`、`BattleBlindFire.cpp:1`、02の既定ON表、`Docs/journal/HANDOFF-2026-11-04_11-13.md:49,57,631–635,684`。高 |
| Routes、CoverApproach、Stances、CoverArrival／RangeProfile等のマップ依存系列 | 採用済みの条件付き機能。A/Dの非適用枝とH/I/E等の適用枝が両方生きている。「既定ONだからOFF不要」に分類して削れない | `BattleTactics.cpp:171–175`、`BattleStance.cpp:17–31`、`BattleFieldLayout.h`、02台帳。高 |
| HireSightRefresh、EngineMoveSweep | **逆極性**。追加更新／エンジンSweepを省く方が採用済み（引数は既定0）。=1が旧経路。旧基準・表示と試合の結合の調査が残るため保持 | `STATUS.md:1022`、`BattleMixedRecruitment.cpp:109–113`、`BattleStableMovement.cpp:483–485`、`Docs/journal/STATUS-2026-11-17.md:114`。高 |
| CorpseLifetime、RifleMembers、LeaderHealth、SupplyHeal | 採用済みの数値設定。0、10人、倍率1などを比較用に残す意味がある。特にCorpseLifetime=0は表示と試合の差の再現条件 | `STATUS.md:19,254`、`ROADMAP.md:93–95`、`BattleTactics.cpp:23,27–37`。高 |

**OFF枝が薄いことと、現在到達不能であることは違う。** Eの削除は設定API・保存受入条件・旧比較を変える。既定のゲーム内判断が同じでも、昔の起動コマンドを黙って無視するようにしてはいけない。受け付けなくなる引数は明示警告と移行表が必要。

### D：参照なしを確認した入口と、誤検出の除外

検索範囲はSource/Config、Setupの`.ps1/.py`（Legacy含む）、ルート`.cmd`、STATUSおよびjournalのSTATUS。生成されたSWITCHESの掲載だけは使用実績に数えない。保存された実行ログ、外部シェル履歴、別ワークツリーの呼出し、Blueprintバイナリは使用者不在の根拠にしていない。

| 入口 | 判断 | 根拠・確信度 |
|---|---|---|
| BlindFireClipTop、BlindFireClipSide | D候補。コメントではアニメ選定用の差替え口。Setup/STATUSに完全名参照なし。今後の選定で使わないなら読取2式だけ廃止できる | `BattleBlindFire.cpp:42–44`。参照なしは高、実験終了・利用者不在は低。既定名を残せば通常表示の選択は変わらない |
| ReplayArm、ReplayFeature | 文字列引数への参照はないが、**死んだ機能ではない**。ReplayBatchが内部でA/B/C/D・AllOn等を設定する | `BattleFieldReplay.cpp:58–68,198–205`、`Setup/Legacy/Test-RenderReplay.ps1:34`。高。単独指定口2行の廃止とリプレイ本体削除を混同しない |
| AimAssist、RangeReachSupport、SuppressionDecay、AreaHirePurposeFront、AreaBaseLowHP、AreaWarPointSeconds、AreaWarResourceValue | Setup/STATUSの完全名参照なし。ただし各設定は実行時に読まれ、調整口。未使用コードとはしない | `BattleController.cpp:1107`、`BattleRanges.cpp:21`、`BattleSuppression.cpp:15`、`BattleTactics.cpp:61,80`、`BattleWarScore.cpp:11`。高 |

### 削除候補のコード量と既定不変の根拠

「消す」は**既定側へ定数畳み込み、参照・診断・引数入口を一緒に整理する**意味。今回削除はしていない。行数は対象本体か安全に識別できる最小範囲であり、波及差分の確定見積りではない。

| 候補 | 対象行数 | 既定試合を変えない根拠／注意 | 効果・危険度・確信度 |
|---|---:|---|---|
| FollowSightParallelの実験本体 | `BattleLeaderRuntime.cpp:805–809` 5行＋`:817–863` 47行＝52行。既定側のCanSee呼出へ置換するので純減はこれ未満。別途h:605–608、cpp:1368、ScoutReport:58–59等 | 既定0ではprefetchはReset直後returnし、FollowCanSeeはCanSeeへ落ちる。呼出時刻・順序を保持すれば既定経路は同じ。ON比較の再現入口は失う | 保守負担中減、危険度中、静的根拠高。実測同値は未実施 |
| ReduceAnimTasks | `BattleAnimTasks.h`11＋`.cpp`27＝38行、runtimeの読取4行（94–97）、呼出・メンバは別途 | 既定0ではApplyを呼ばない。ONの旧実験以外の呼出なし。FBattleLeaderBodyの非反射メンバ・includeを一緒に除去。共通HiddenBonesは残す | 保守負担小減、危険度低〜中、根拠高 |
| VATFarLightの専用部分 | cpp:35–39の5＋102–108の7＋250–255の6＋264/312各1＝20行。宣言・専用時計・ログ整理は別 | 既定0では分離・tick停止・AdvanceFarは実行しない。通常VAT/死亡復帰を残す。ただし既定のattach/tickコードまで巻き込むと不変を保証できない | 保守負担小減、危険度中、根拠高。再現用途があるので優先度低 |
| AreaDepot（現行方式を廃止する場合） | `BattleAreaDepot.cpp`49行全体＋宣言・呼出の整理 | Mode既定0で重み変更前return（:25）。ただしRaid読取・Def取得までは通る。既定のPointWeightを変えず呼出除去可能 | 保守負担小減、危険度低〜中、根拠高。新しい係数評価を続けるなら残す |
| MoveSelected、RefreshVehicles | 下節の20行 | 反射宣言も呼出しもない。既定・試験双方の動作を変える呼出がない | 保守負担小減、危険度低、根拠高 |
| BlindFireClipTop/Side差替え口 | cpp:44の1物理行（2読取式） | TopName/SideNameの既定代入・ロードは残す。既定のアセット選択不変 | 保守負担ごく小、危険度低、不要判定の確信度低 |

LeaderGuard、NoCoverFallback、EのOFF枝は現時点で削除提案に含めない。LeaderGuardはPlayで使用中、NoCoverFallbackは修正前後が混在、Eは比較・保存用途が残る。性能改善の見積りはしていない。

## 3. 未使用・到達不能コード

### I01【高】通常関数2件は宣言と定義以外に参照がない

- `BattleController.cpp:569–579` の `MoveSelected`（11行）、`BattleController.h:210`（1行）。現在の右クリックはInputKey／経路描画からIssueOrderAtへ進み、この関数のバインドはない。UFUNCTIONではない。
- `BattleVehicle.cpp:42–48` の `RefreshVehicles`（7行）、`BattleLeaderRuntime.h:206`（1行）。車両登録の本経路はConfigureVehicle、復元ではVehicleIDsをResetして構成し直す。こちらもUFUNCTIONではない。

Source全体の名前検索は各2件（宣言・定義）のみ。削除候補は計20行。効果は入口の混乱削減、危険度低、既定試合は静的には不変。単に名前の出現が少ないRPCの`*_Implementation`、仮想override、コンソール用`BattleVerify`は除外した（`BattleController.h:181`はUFUNCTION(Exec)）。

### I02【高：条件付き到達不能】F03は機能不全であり、不要な枝ではない

`BattleLeaderRuntime.cpp:868`が有効な遮蔽行動を実行してreturnするため、`:898–900`でBounding移動担当を遮蔽から出す枝は、その遮蔽状態では届かない（01 F03、02 D07）。接近中のCoverApproaches側は到達し得る。分岐全体を削除すると予定している交互前進の意味を失う。優先順位を直して回帰を追加する問題であり、死んだコードとしての行数削減には算入しない。

### I03【高：C++読取なし／削除可否は中以下】反射フィールド2項目

`BattleState.h:215`の`UnitArchetypeID`と`:216`の`LeaderAbilityScale`はSource/Configでは宣言だけ。後者は01 F09への追加棚卸し。名前から期待されるアーキタイプ／判断能力への接続はC++にない。一方UPROPERTYであり、状態を丸ごとコピー・保存・反射比較する経路では名前が出現しなくても値が運ばれる。Blueprint・既存保存・アセット利用を読取検索だけで否定しない。まず「予約項目、ゲームの能力調整には未接続」と文書化。2行だから安全に消せるとはしない。

### I04【中：未使用候補】Setup/Pythonは参照グラフだけで削除しない

完全なファイル名を他のSetupスクリプト・`.cmd`・Markdown（本レビュー群を除く）で検索すると、以下に外部参照がなかった。すべて自分の`:1`から始まる単独CLI候補である。これは呼出し履歴の監査ではない。特に拡張子なしの文中表記・動的に組み立てた名前・手動実行は検索をすり抜ける。

| 系統 | 完全名の外部参照なし候補（括弧は全体の物理行数） | 扱い |
|---|---|---|
| 性能解析 | `Setup/Analyze-SpikeCompetition.py`(72)、`Analyze-SpikeTaskWindows.py`(38)、`Check-SpikeRoot.py`(33)、`Report-SpikeRootEvidence.py`(62)、`Verify-FrameSpikes.py`(28)、`Build-SpikeEtwDecoder.ps1`(7)、`Build-SpikeTraceDecoder.ps1`(17)、`Export-SpikeRoot.ps1`(23)、`Measure-SpikeRoot.ps1`(91) | 証拠再解析用の可能性。旧調査のrun/manifestから使用者を確認し、終わっていればLegacyへ分類。削除は保留 |
| VAT／戦術／Phase3 | `Setup/Verify-VATDisplay.py`(71)、`Verify-VATAdoption.py`(48)、`Summarize-VATDisplay.py`(22)、`Plot-LeaderPhase3.py`(44)、`Summarize-TacticsTrial.py`(76)、`Verify-AreaTrial.py`(68)、`Analyze-TrialDetail.ps1`(40) | 入力結果を読むCLIは他コードから呼ばれなくても有効。ゲーム内死コードとは分離 |
| 資産作成 | `Setup/Create-AC01.py`(483)、`Finalize-AC01.py`(33)、`Inspect-RobotRig.py`(9)、`Inspect-AC01T2-Unreal.py`(31)、`Export-RobotRig.py`(32)、`Verify-AC01.py`(54) | 資産の作成・検査履歴を保存する用途。モデル資産が現在使われることと作成スクリプトの再実行頻度は別 |
| 試験入口 | `Setup/Test-CoverActions.ps1`(22)、`Test-Delegation.ps1`(43)、`Test-FlankBay.ps1`(22)、`Test-SightTrial.ps1`(30)、`Test-VATAdoptionLeader.ps1`(26)、`Test-VATBake.ps1`(14) | 削除禁止寄り。例えばTest-CoverActionsはSTATUSの拡張子なし表記と実fixtureがある。参照なしを誰も使わない証拠にすると有用な試験を消す |

全体行数を示したのは保守規模の目安であり、削除可能行数として合算しない。整理だけなら試合結果不変、危険度低。ただし過去の証拠再現を壊す削除の危険度は中。

`Setup/Legacy/README.md:3–7`は62本を再検証のため保存していると明示する。`Legacy/Test-RenderReplay.ps1:8`の相対パスは現配置からはSetup/Gameを指すが、「必要なものをSetupへ戻して使う」と明記済み。これを新規の無断破損・誰も使わない不要物とは報告しない。

### 誤って死コードにしなかったもの

- `DefaultGame.ini`のゲーム調整キーについて、Prototype中に対応名がない独自キーは見つからなかった。`ProjectID/ProjectName`はエンジン項目。DefaultEngine/Input等もエンジン・プラグインによる消費があり、ゲームC++だけに名前がないことは死んだiniの証拠ではない。**削除を勧められる未参照ini項目は今回なし**。
- 旧Actor、Phase3、Benchmark、ネットワーク、生成commandletは通常Areaで通らなくても専用入口がある。01の旧経路の重複を死コードと取り違えない。
- `BattleFieldReplay.cpp:202–203`の内部バッチ、`BattleCompositionSmoke.cpp:14–24`の合成入力、試験専用失敗注入 `BattleMixedBodies.cpp:113–114` は通常戦闘で無効でも試験で到達する。

## 4. スモークが保証する範囲と穴

### T01【高】「基本回帰All」は混成Areaの全試験ではない

`Setup/Test-Battle.ps1:2,8,14`は旧Prototypeマップ＋BattleSmokeTestを起動する。`BattleGameMode.cpp:155–157,229–369`からOrder→Perception→Tactics→Tactical→Squad→Ability→Match→Objective→Save→Supportへ連なる旧20体系の試験。`BattleCompositionSmoke`等の混成試験までAllに含むわけではない。AGENTSの基本回帰義務は妥当だが、それだけで挙動変更の安全性が保証されたと扱わない。

| 入口／fixture | 覆う機能 | 覆わない機能・条件 | 根拠（すべて確信度高） |
|---|---|---|---|
| Test-Battle / GameMode・Order・Perception等 | UnitID/所属、制御切替時のHP/弾薬/命令、LOSと壁、発射制限、Nav命令、旧保存・戦術・拠点・支援 | 混成Actor/Massの全遷移、H/I区画、Planner実購入、砲兵、操作座標・ゲームパッド実機 | `Setup/Test-Battle.ps1:8`、`BattleGameMode.cpp:229–369`、`BattleOrderSmoke.cpp:148`、`BattlePerceptionSmoke.cpp:12,167` |
| Benchmark-Battle / BattleBenchmark | 指定体数、活動・判断・射撃、計測JSON等の成立 | 戦術の期待値、全試合完走同値、混成Areaの任務・保存 | `BattleBenchmark.cpp:135–186`。結果は`BATTLE_BENCH_RESULT success=`であり、failures=0スモークではない |
| Test-MixedSave / BattleMixedSaveSmoke | 500場で正規状態・body・保存codec、壊れたID/設定拒否、経路失敗、Actor/Mass staging失敗、直接操作保存、復元後2秒の移動/装填 | 非空の制圧・姿勢等の失敗前後、手描き経路・最終Defend、長時間の判断列同値、Planner ON | `Setup/Test-MixedSave.ps1:2,13–14`、`BattleMixedSaveSmoke.cpp:75–124` |
| Test-Area / BattleAreaSmoke | 固定Aで補充・収入・資源・勝敗、兵種購入・装備・主砲/砲撃、群・装甲選択の保存、v3/v4互換、UIメソッド | H/I区画経路、操作イベント列、自然な戦略試合、全9切替ONの試験。条件分岐内の検査は起動条件次第 | `Setup/Test-Area.ps1:2,13–15`、`BattleAreaSmoke.cpp:40,355–439,699–778,850–934,1142–1155` |
| Area内のBattleCompositionSmoke | 固定入力の貯蓄60秒、予算到達、失敗後2判断待ち、緊急割込み、順序不変、合成v4 codec＋次のDecide一致 | 実際のセンサー取得→収入窓→購入→命令更新、C01〜C06、非空実戦の連続往復 | `BattleAreaSmoke.cpp:40`、`BattleCompositionSmoke.cpp:14–24,28–100,105–139`。独立のBattleCompositionSmoke起動引数があるとは扱わない |
| Test-CoverActions / BattleCoverActionSmoke | C/DでActor/Massの接近・隠れ・覗き・解除、照準遅延、制圧の局所式、cover保存 | Boundingの担当交代、区画補給復帰。ブラインドファイアと体姿勢の画像依存段階はRendered経路。クリップ読込・方向選択のassert自体は通常経路にもある | `Setup/Test-CoverActions.ps1:2,12–13`、`BattleCoverActionSmoke.cpp:133–202,206–236,268–272` |
| SectorSmoke / SectorAudit | 3隊の区画命令、到着配置が区画内、区画移動FireBack解除、実試合中の監査 | 補給完了後の元任務復元、全武器の交戦許可、交互前進の実移動 | `BattleAreaSmoke.cpp:1162–1191`、`Setup/Test-WideSight.ps1:2`のSectorAudit。Test-Areaとは別の起動条件 |
| Test-SquadRetreat / TeamSupport / SquadFlank | 混成の撤退・再編・援護・迂回の専用シナリオ | 手動区画任務を含むHOME SUPPLY、兵種購入目的との複合遷移 | `BattleSquadRetreatSmoke.cpp:13`、`BattleTeamSupportSmoke.cpp:14`、`BattleSquadFlankSmoke.cpp:16` |
| Test-WideSight -Experiment -RoundTripSave | 80秒の保存チェック、試合結果・診断、完走 | 無往復と同一判断列であること、巻戻しと別プロセスでの全状態一致 | `BattleSightExperiment.cpp:40–60`、04 R03〜R15、06後続メモ |
| TPS/表示/VAT専用試験 | 各fixtureの制御・敵表示・衝突・表示遷移、画像生成 | 全キーの押下/解除競合、解像度別のクリック対応、VAT死体の近景復帰、実機の照準感度 | `MEASURE.md:49–62`、`STATUS.md:187–201`、07の操作・HUDメモ |

### T02【高】ON枝を検査するコードがあっても、標準ランナーからONにできない

`Test-Area.ps1:2,14–15`にはPlanner/Teams、CoverActions、Flank等はあるが、11-21追加のPeekRangeCheck、WeaponOrderGate、ArmourSectorGate、ArtilleryObserved、SPGAmmo、BlastOcclusion、ArtilleryClusters、ExoShieldSuppression、GroupStrengthを渡す引数はない。Test-MixedSaveも同様。Test-WideSightの巨大なparamにもこれらはない。ゲームの直接CLIには実装されている。

例えばAreaSmokeのSPGAmmo検査は`:860`付近のON条件内、ArtilleryObservedは`:893`付近。OFF条件での`failures=0`をONの検証済みと読めない。将来ランナーへ明示パラメータとmanifest実効値を配線し、まずOFF・単独ON・必要な交差組合せを走らせる。全部同時ONだけでは原因の切り分けができない。効果大、危険度低（試験のみ）、既定試合不変。

### T03【高】失敗原子性のassert対象が不足している

`BattleMixedSaveSmoke.cpp:82–102`のRejectはMatchID/HP/Actor/Credit/body数を見る。AreaSmokeの群・CarChoices・ルート選択確認（`:722–758`）も有用だが、`BattleMixedBodies.cpp:75–77`で消えるCoverApproaches、FirePositions、SuppressionZeroAt、Stances、StanceLockedUntilを非空で比較しない。stagingがあるため安全、という推論は成立しない。

### T04【高】経路消失は、現在のスモークが「成功」と要求する箇所もある

`BattleAreaSmoke.cpp:758`は成功ロード後のPlayerRoutes空をPASS条件にしている。`PROTOTYPE.md:22`はレーン選択が保存されないと説明するが、折れ線の最終目的地・最終命令まで失う影響は十分説明していない（05 S02）。要求を決めずにテストだけ増やしても、損失を仕様として固定する。保存する仕様へ進む場合は、この既存assertも改訂対象。

### 不具合ごとの捕捉試験設計（全件未実装・未実行）

表の期待値は修正後に守りたい契約。現行仕様で自明でないものは仕様決定が必要と記した。各ケースは最終勝敗だけでなく、問題が起きる直後の状態をassertする。

| 指摘 | 既存試験が逃す理由 | 入力・操作 | 期待値／根拠・確信度 |
|---|---|---|---|
| F02 区画任務消失 | SectorSmokeは発令直後、Retreat系は区画との直積なし | H/I、手動区画Defend/AttackMove、欠員でHOME SUPPLY、必要人数・HP回復→再開。AI更新を一旦止めて再開直後を見る | bSectorMission=true、元Order/Goal保持、到着位置も同じ区画。手動／委任、8人／10人を分ける。`BattleSquadRetreat.cpp:130–139`、`BattleState.cpp:212–217`。高 |
| F03 交互前進 | cover単体は覗き遷移、Boundingとの交代を通らない | CoverActions=1、Bounding=1、隊員をCoverPhase=2/3/4と接近中で用意。4秒待機後、偶奇の前進担当へ切替、Followを継続 | 前進担当は予約解除して位置が変わる、援護担当は保持、7秒交代。解除回数だけでなく経路進捗を見る。`BattleLeaderRuntime.cpp:868,898–900`。高 |
| F04 主砲の命令違反 | Areaの主砲試験は射程・損害・発射成立、OrderGate試験は砲兵中心 | 戦車と観測済み敵2体。WeaponOrderGate=0/1別プロセス。Move、Support指定敵、Defend内外を順に発令してThink→Follow→StepTankGun | ONのMoveで発射/消費なし、Supportは指定対象のみ、Defendは許可範囲。OFFは旧結果。直接操作の発射許可も別case。`BattleTankGun.cpp:30,85,114`、`BattleAreaSmoke.cpp:355–439,850–855`。高 |
| F05 区画外装甲再選択 | 歩兵の区画試験と装甲選択試験が別 | 区画内歩兵・区画外車両の両方を既知にし、ArmourAI=1、ArmourSectorGateを切替 | ONでは後段PickArmourTargetsも許可集合内。TargetIDだけでなくbSectorContact/移動判定も整合。`BattleLeaderRuntime.cpp:612,640,719`、`BattleArmourAI.cpp:58–83`。高 |
| S01 失敗ロード | 比較項目不足 | 制圧・姿勢ロック・遮蔽接近・射撃位置を有効にしたlive状態。別の有効保存から経路不能とstaging位置0/1/2/末尾で失敗注入 | false後に全正規・実行状態、残り期限、body、予約、選択、次tick結果が不変。検証段階拒否とstaging拒否を分ける。`BattleMixedBodies.cpp:75–77,83–119`。高 |
| S02 プレイヤー経路 | 経路なし往復、または空化が期待値 | F/Gでレーン経由の最終Defend、Gで3点折れ線、H/Iの地点移動。中継点間で保存→往復／別プロセス再開 | 保存する仕様なら残点列・現在index・最終Goal/Order保持、終点でDefend。保存しない仕様なら明示的に安全停止して利用者に告知し、途中点を最終命令と誤認させない。`BattlePlayerRoutes.cpp:43`、`BattlePlayerPath.cpp`、`BattleAreaSmoke.cpp:758`、05 S02。高 |
| C01 防衛購入の恒久化 | 合成DecideはMission=101の決定まで | Planner ON・HirePurpose OFF、本拠地被弾→2回購入→30秒超被弾なし→UpdateTeamOrders/Groups | 仕様で定めた防衛必要数・解除期限を超えた購入隊が通常任務へ戻る。既存P1恒久目的は別caseで保持。`BattleCompositionRuntime.cpp:112–136`、`BattleMatch.cpp:367–428`、`BattleCompositionSmoke.cpp:65`。高 |
| C02 砲兵等の配置消失 | 候補能力と生成成功を見ても実命令を見ない | Planner＋P1で通常拠点目的、Mortar/SPG/Tank/Scoutを個別に購入し次の戦略更新まで進める | 対象拠点は保ちつつ兵種固有の後方/監視位置へ。群参加は任務整合に従う。`BattleMatch.cpp:370–390,401–428`、`BattleGroups.cpp:242–243`。高 |
| C03 個体記録容量 | Records=100は小隊上限、個体1000とは別 | 個体記録を997/999/1000、Active小隊には空き、予算十分。多人数候補と1人車両候補を同時に欲しい状態 | 生成できない兵種を除外し、買える1人候補を選ぶ。1000では早期Wait、資源/履歴不変、周期的失敗ループなし。`BattleCompositionPlanner.cpp:28`、`BattleMixedRecruitment.cpp:41–57`、06 C03。高 |
| C04 AreaIntel目的の脱落 | Plannerと旧経路の目標付与を比較しない | AreaIntel=1、HirePurpose=0、味方拠点への観測脅威を与え、Planner OFF/ONで同兵種を買える入力 | ONも観測した防衛先を適切に継承し、脅威解消後に解除。非観測真値だけ変えて目的不変。`BattleArea.cpp:183–191`、`BattleCompositionRuntime.cpp:113–114,136`、06 C04。高 |
| C05 到着締切 | 合成入力はStopAt=600000、購入停止と終戦を分けない | 終戦600秒、HireStop=60、Now=530秒、到着30秒、購入可能 | 購入停止前530秒に買え、到着560秒<終戦600秒なら到着遅延理由で落とさない。Now=540秒では購入停止。`BattleCompositionRuntime.cpp:109`、Plannerの到着判定、06 C05。高（仕様は購入停止と到着期限を分離） |
| C06 古い緊急度 | 試験は脅威が増すケース中心 | 同じMissionのSaveをUrgency=3で開始→現在Held=0へ低下→買えるAT候補Urgency=2を追加、2判断進める | 現在の緊急度で割込み、元60秒期限を不当に延長しない。同Mission維持条件を固定する。`BattleCompositionPlanner.cpp:137–149`、`BattleCompositionSmoke.cpp:59–70`、06 C06。高 |

追加の横断試験も必要。

| 対象 | 入力と期待値 | 根拠・確信度 |
|---|---|---|
| 観測境界（03 O01〜O07、06 C07） | 接触ID/位置/時刻/ObservedSquadTypeを固定し、未観測敵のHP・弾・現在兵種・存在・拠点所有だけを変更。判断が不変。Planner Teams=0/1/2/3、GroupStrength=0も比較 | `BattlePerception.cpp:64–65`、`BattleAreaIntel.cpp:19–52`、03後続メモ。高 |
| 保存v4・収入窓 | 実際の収入支払、補充支出、手動購入、貯蓄・Retry・Switch・SaveBlocked、非空VehicleSeenMSを作って保存。codec一致だけでなくCapture→Decide列を比較 | `BattleCompositionSmoke.cpp:105–139`、`BattleCompositionRuntime.cpp:56–91`、06後続メモ。高 |
| 旧保存・実効設定 | Planner ONでv0〜3を読む→CompositionTeams=0、次保存v3。省略/明示/Clamp後同値/大小文字/重複引数、別マップ・異なる設定を個別に検査 | `BattleCompositionRuntime.cpp:24–41`、`BattleSave.cpp:223–238`、05 S06/S07。高 |
| 予約・探索予算 | 同slot二度Reserve、期限ちょうど、所有者交代後の旧Release。24/40/80の論理試行数と物理Nav回数を別assert | `BattleCover.cpp:176–200`、`BattleCoverAction.cpp:386–387`、`BattleStableMovement.cpp:151,195,205`、07後続メモ。高 |
| 同値比較 | 同一条件の独立2実行、80秒往復なし/あり、巻戻し、別プロセス再開。最初に違うtickのID順状態・判断列を比較。Reuseコピーは反復に数えない | `Setup/Compare-MatchResults.py:20–23,40–55`、`Setup/Test-WideSight.ps1:143`以降、04 R02。高 |

## 5. 資料とコードの食い違い

### M01【高】STATUSの要約が直近項目を反映していない

| 要約の記載 | 現在の根拠 | 影響 |
|---|---|---|
| `STATUS.md:9` peek失敗とA基準ずれが未調査 | `STATUS.md:45`で旧peek検査修正済み、基準ずれは比較条件混在と整理 | 解決した調査を再実行し、別の保存問題と混同する |
| `STATUS.md:11` HireSightRefresh=0の判断待ち | 同`:13,1022`で採用済み、`BattleMixedRecruitment.cpp:112`既定0 | 同じ表内で採否が二重化 |
| `STATUS.md:12` AreaGroups既定0、A/Dだけ採用待ち | `ROADMAP.md:13,73,81`、`STATUS.md:52–56`で全マップON | 比較の基準と片側解釈を誤る |
| `STATUS.md:14` H・区画命令が未着手 | H/I資産定義、`BattleAreaSmoke.cpp:1162`、`Setup/Play-Area.ps1:9–19` | 次工程を誤る |
| `STATUS.md:12,15,18` main未取り込み・重複した系統行 | 直近の統合記録と現Sourceの装甲/アニメ実装 | 「作業系統ごとの現在」を歴史として読まざるを得ない |

要約の日付も11-18のまま。履歴本文を削らず、現状表だけ決定・採用日・次の評価へ更新する改善案。効果大、危険度低、試合不変。本依頼では更新しない。

### M02【高】CODEMAPの行番号が一部古い。関数改名とは区別する

| 索引 | 実際の定義 | 根拠 |
|---|---|---|
| `CODEMAP.md:24` UpdateCompositionHiring:55 | Runtime:56 | `CODEMAP_SYMBOLS.md:24`も旧番号 |
| 同ValidateSave:4 | CompositionSave:5 | `CODEMAP_SYMBOLS.md:25` |
| 同RunCompositionSmoke:8 | CompositionSmoke:11 | `CODEMAP_SYMBOLS.md:26` |
| `CODEMAP.md:13,25,32` Perception Update:57／GetContacts:36 | Perception:58／37 | `CODEMAP_SYMBOLS.md:94` |
| `CODEMAP.md:36` UpdateBodyAnim:412 | Unit:422 | `CODEMAP_SYMBOLS.md:153` |

存在しない改名先を推測しない。今回の不一致は同じ関数の行移動。`Setup/Export-CodeMap.py:25–28`の出力先と`:46–70`の走査を確認したが、実行すると既存ファイルへ書くため再生成していない。ソース変更後に両索引を同時生成する運用の不足。効果小〜中、危険度低、試合不変。

### M03【高】PROTOTYPEの右クリック説明がH/Iの実際の起動と違う

`PROTOTYPE.md:22`は右クリック移動、短いドラッグはクリック同様と一律に説明。`Setup/Play-Area.ps1:10–13`はH/IでSectorMove=1。`BattleController.cpp:639–645`は歩兵の通常Moveクリックを区画AttackMoveへ変換する。`STATUS.md:239`では12px以上のドラッグは地点移動と説明される。区画移動／地点移動／T/G指定／車両例外を操作表に分ける必要がある。

また`PROTOTYPE.md:24`にM/Nの区画格子・霧切替がない（`BattleController.cpp:101–102,591–604`）。購入表`:23`は2〜6中心で、7装甲車・8迫撃砲・9自走砲・0外骨格の一覧性が不足する（実バインド`:108–113`）。マップ一覧`:9`はHを省くがランナーはHを受理する（`:2`、Hは比較用というROADMAPの説明を添えるべき）。効果中、危険度低、試合不変。

### M04【高】MEASUREの既知障害と「一致」の説明に更新が必要

- `MEASURE.md:34`の`Invalid cover peek distance`未調査は`STATUS.md:45`と矛盾する。原因が同じとは限らない新しい失敗まで既知として除外しない。
- `MEASURE.md:21`の比較は「時間の値を除く全項目」、実体は`Compare-MatchResults.py:15–16`の名前規則でトップレベルを除外し、`:40–55`でmetricsだけを比較する。manifest・result.passed・必須終戦項目・完全状態は検査しない。空のJSON同士もこの実装上は差なしになる。比較器の修正済み実時間率を再指摘するのではなく、合格名を「非時間metrics一致」に限定する。
- `MEASURE.md:7`の`ListAgents/SendMessage`はセッション環境依存の名称。リポジトリのPowerShellコマンドではない。現在利用可能な連絡手段と対応づける説明が必要。今回の読取レビューをロック待ちで止める理由にはならない。

効果大、危険度低、試合不変。SWITCHESの推定値問題は02 D10を参照するだけとし、本報告では生成器の再監査を行わない。

### M05【高】PERFORMANCEの「今の既定」は現在の試合条件ではない

`PERFORMANCE.md:14–22`の基準は11-18・E・30FPS、該当commitと実験条件が書かれた有効な過去記録。しかし`ROADMAP.md:73,88`、`STATUS.md:55`で11-20にGroups/SymmetricStartの既定が変わり、さらにCoverPriority採用がある。「現在11.8ms」「現在の60FPS達成」の根拠にはならない。旧表は消さず測定時点を表題に入れ、現HEADの基準は未測定と分ける。効果中、危険度低、試合不変。

### M06【高】設計書冒頭と統合済み検証記録の時点が違う

`Docs/design-composition-ai.md:3–5`は購入接続済みだがビルド・実行未実施とする。一方`STATUS.md:24–28`にはその後のビルド・Test-Battle/MixedSave/Area成功、OFFの比較一致がある。**ON実戦未評価は依然正しい**。設計書の「この実装作業では未実行」と「統合後の現在未検証」を分ける。

同書`:15`は下の段階1本文を履歴と明記しているため、`:148,159,209`の将来形を独立した実装欠落とは数えない。現在の最新追記と06のC01〜C06の差を追う。効果中、危険度低、試合不変。

### M07【高】引継ぎの固定作業ルートは別ワークツリーを指す

`CLAUDE_HANDOFF.md:33–36`と末尾の開始文は`C:\Users\nanek\Documents\FPS\FutureBattle`を固定指定。本レビューの作業ツリーは`FutureBattle-codex`。通常スクリプトはPSScriptRootから対象projectを解決する（`Setup/Test-Area.ps1:6–7`等）。引継ぎのSet-Locationをそのまま実行すると別のDLL・Config・STATUSを扱う。作業対象の明示パスで置換する手順にする。冒頭`:3`で歴史と明記した11-12の進捗・射程・210件は、現在の機能欠落として数えない。効果大、危険度低、試合不変。

### M08【高：不一致を認定しなかった範囲】AGENTSと計測機記録

`AGENTS.md:14`はT5以降の各計測に開始前machine・JSON深さ5以上を要求する。現行Area/MixedSave/CoverActionsは呼出・Depth 8あり。`Setup/Legacy/Test-T5.ps1:4,18,24`も取得・Depth 5がある。Legacy配置のままではhelperの相対参照が解決しないが、`Legacy/README.md:5`で必要ファイルをSetupへ戻す手順が明示されている。未修正の要件違反とは認定しない。

旧`Setup/Legacy/Test-RenderReplay.ps1:23`にはmachineがないが、これはT1の古い入口であり、T5以降の違反という証拠には使わない。今後の新規計測へ転用するなら現行手順に合わせる。旧証拠へ後付けしない。エンジン版はAGENTS・現行スクリプトともUE_5.8で一致し、5.7へ戻す必要はない。文書化の効果は誤った再構築の防止、危険度低、試合不変。

## 6. 改善の優先順

| 順 | 改善 | 効果 | 危険度 | 既定の試合結果が変わるか |
|---|---|---|---|---|
| 1 | 失敗原子性S01の非空状態テスト＋失敗前Resetの修正 | ロード失敗後に試合を壊さない | 中 | 正常な新規試合は不変を狙う。失敗ロード後の挙動は修正される |
| 2 | F02/F03/F04/F05、C01〜C06の上表テスト | 層をまたぐ欠落を再発防止 | 試験低、実装修正中 | 試験追加自体は不変。区画復帰修正は既定操作にも影響。Planner/Bounding/武器Gateは既定OFFを維持して評価 |
| 3 | 標準ランナーの試験切替配線、実効設定・必須チェック数・SKIP理由の記録 | OFF合格をON合格と誤認しない | 低〜中 | 通常ゲーム不変。試験の起動条件は意図的に変わる |
| 4 | STATUS要約、操作説明、MEASURE、索引、引継ぎパスを同期 | 誤った次工程・別ツリー操作を防ぐ | 低 | 不変 |
| 5 | metrics一致と完走・全状態一致を別判定、再利用結果を独立反復から除外 | 採否・性能評価の信頼度を上げる | 低〜中 | 比較ツールのみなら不変 |
| 6 | 非反射未使用2関数20行を削除 | 保守上の入口を減らす | 低 | 静的には不変。将来実施時は通常ビルド・適切な回帰 |
| 7 | FollowSightParallel・ReduceAnimTasksを廃止候補にし、過去再現用の扱いを決定 | 使わない実験経路の維持負担を減らす | 中 | 既定側へ畳み込めば不変を狙える。旧ON実験は再現不能になる |
| 8 | AnimSkip/Budget/NoOverlapの視覚確認と現行条件再測定 | 既存の有効候補を判断できる | 中 | Skip等は過去試行で不変だが現行全条件を保証しない。採用と掃除は別差分 |
| 9 | VATFarLight再現用途、AreaDepot再設計、EのOFF枝の寿命を決める | 実験が永続的に増えるのを防ぐ | 中 | 既定側保持なら不変。比較条件・保存受入互換は変わり得る |
| 10 | UPROPERTY・単独CLIの所有者／保存互換調査 | 誤削除防止、分類精度向上 | 文書化低、削除中 | 文書化不変。アセット・反射調査前の削除は保証不可 |

将来計測する場合だけMEASURE/AGENTSの測定機記録を開始前に取得する。性能は描画あり単独、戦術評価は現行既定・固定FPS・seed・swap・保存条件を揃える。Experimentが追加LOSや診断を行う費用（07）を通常プレイの費用と混ぜない。本報告の行数・机上試験案は実測結果ではない。

## 7. 最後の点検（操作・HUD）へのメモ

1. **入力の意味とモード境界**：`BattleController.cpp:405–420,632–651,1067–1145`。H/Iのクリック・12pxドラッグ・T/G・車両例外、RTSのF/Vと直接操作のF/LB、右クリック構えとRTS命令、Tab復帰時の押下状態を追う。`BattlePlayerPath.cpp:271–295`の24隊表示上限・描画中末尾・画面外の線も見る。
2. **保存で消える利用者意図**：`BattlePlayerRoutes.cpp`、`BattleAreaSmoke.cpp:758`、05 S02。手描き最終Defend、選択集合、レーン、群選択、カメラのどれを保持する契約か。単に「選択は保存されない」で目的地損失まで説明できているか。
3. **一覧の表示とクリック**：`BattleController.cpp:829–848,1013`、`BattleHUD.cpp:546`。ソート・フィルタ・スクロール中の死亡/補充/群編成で表示行と命令先が一致するか。1920×1080だけでなく小さいウィンドウ、UIスケール、パッドカーソルで確認する。
4. **情報漏洩と霧**：`BattleHUD.cpp:75,493,840–862`、`BattleSectorView.cpp:48,100–102`。上帯・ミニマップ・区画情報の偵察除外条件、N/Mで非表示でも更新される意味、停止・倍速・再表示時の記憶寿命を確かめる。07 N01/N02を参照。
5. **表示切替と兵の状態**：`BattleVATDisplay.cpp:240–278,331–345`。近景→遠景→近景、死亡途中、保存復元、slot再利用。STATUSで未通過のcorpse_returnsを特に確認。AnimSkip/Budgetは骨間引きだけでなく、銃口・左右の武器・被弾・覗き・姿勢遷移が自然か。
6. **ブラインドファイアの試験条件**：`BattleCoverActionSmoke.cpp:206–236`はスクリーンショットprefixによる経路分岐がある。NullRHI既定の合格から、頭・体の露出や体アニメ全段階まで合格としない。描画試験と実入力の両方が要る。
7. **パッド・照準・通知**：`BattleController.cpp:1107–1145`のエイム補助と上下符号、PROTOTYPEの実機未確認、F8/F9失敗メッセージ、購入不能・区画命令拒否・複数隊一部拒否の伝達。スモークがGamepadToKeyの対応を呼ぶことと、実機のpress/release・軸・UI操作が正しいことは別。

以上は読取による指摘と次の検証設計であり、修正・試験合格・性能改善を報告するものではない。
