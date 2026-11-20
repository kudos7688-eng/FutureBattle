# FutureBattle 総点検 06：編成AI

レビュー日：2026-11-21。確認HEAD：`0f33c827767df26947bf27d75d8f83ccc24fbb02`。UE 5.8.4。静的レビューのみ。

## 結論と範囲

**最初のON試合では、購入の成功だけでなく、買った部隊の任務・配置・群参加まで追う必要がある。** 既存の購入実行・予算保護・整数比較は利用できている。一方、緊急防衛の恒久拘束、個体記録容量を知らない再試行、旧Intel購入任務の脱落、終了時刻の扱い、保存署名と診断の不足が残る。クラッシュや勝敗差を実証した報告ではない。

先行[01](01-ai-layers.md)〜[05](05-save.md)の指摘・後続点検メモと、[設計書](../design-composition-ai.md)を出発点にした。01 F06、02の切替境界、03の観測入力、04の整数比較・時計・共有状態、05 S07と保存検証を再確認した。STATUS冒頭・直近項目とCODEMAPから入口を確認した。設計書の冒頭追記を最新仕様とし、その下の「未実装」等の履歴は現在の欠落と数えない。

以下の短縮ファイル名は `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。確信度「高」は条件とコード経路を確認した意味であり、頻度・実害の大きさを測った意味ではない。開始時と作成直前の `git status --short -uno` は追跡変更なし（個人git ignoreの読取警告あり）。ビルド、UE起動、試合、試験、計測、状態を変えるgit操作は行っていない。書き込みは本報告だけ。

## 1. 判断までの経路

1. `BattleArea.cpp:136–145`：Combat・雇用有効を確認。IntelPurposeの更新後、Team 0→1の順。通常プレイのTeam 0は購入しない。対象bitだけ `UpdateCompositionHiring` へ分岐し、OFF側は旧処理を続ける。
2. `BattleCompositionRuntime.cpp:62–91`：20秒以上の経済窓を更新し、自軍の生存観測者が持つ識別済み接触を集約。専用のVehicleSeenMSを期限で掃除する。
3. 同 `:92–117`：成功購入累計、AreaIntent、自動目標、観測車両、任務、購入間隔、残り時間を入力へ解決。Captureの旧履歴・車両値は本番で上書きする。
4. `BattleCompositionPlanner.cpp:28–83`：権限・総枠・記録枠・任務・間隔を確認。前線3枠を残して特殊兵種目標を割当、小銃／重装の残り枠を作る。候補は10兵種、現在は任務1個を共有する。
5. 同 `:85–165`：緊急度→整数スコア→待ち回数→TypeID→MissionIDで比較。貯蓄の期限、割込み、通常変更、再開禁止を処理してWait=0／Save=1／Buy=2を返す。
6. `BattleCompositionRuntime.cpp:133–139` → `BattleArea.cpp:96–134`：Buyだけ既存HireAreaSquadへ。生成成功後に資源・Purchased・HireSpent・LastAIHireAtを更新し、任務を記録する。CompletePurchaseは購入数を再加算しない。

## 2. 確信度高の指摘

### C01：緊急防衛購入が恒久任務になり、後続購入も本拠地へ拘束する（01 F06再確認）

**確信度：高。優先度：高。** 根拠：`BattleCompositionRuntime.cpp:112–114,135–136`、`BattleMatch.cpp:367–390,398–428`、`BattleGroups.cpp:242`、`BattleAreaIntel.cpp:69–82`。

状況→結果：HirePurposeを使わない通常のPlannerで本拠地が被弾する。以後30秒、各判断のMissionは101となり、その間の成功購入すべてにHirePurpose=101を付ける。P1のAreaHireNeedにある「既に防衛隊がいるなら追加しない」条件もこの枝にはない。被弾が止まってもHirePurposeを解除する経路はなく、部隊は防衛を継続し、自動群にも入らない。被弾継続なら拘束対象が増える。購入理由が一時的なのに実行任務は恒久的である。

設計冒頭は「任務継続は既存purpose方式」と追記済みなので、完全な無断実装とは扱わない。しかし緊急割込みの解消後に前線へ戻れない影響は残る。IntelPurposeの到着・脅威解消による解除はHirePurposeには適用されない。

改善：購入理由と拘束する任務を分離し、Plannerの緊急防衛だけに必要数・期限・脅威解消時の解除を設ける。効果：防衛隊の過剰購入・戦力の固定を防ぐ。危険度：中（保存と命令優先順位）。**既定試合：Planner OFF経路を維持すれば不変。既存P1全体の解除規則まで変更するとOFF＋P1の試合も変わる。**

### C02：拠点任務を付けると、砲兵・偵察・車両の配置機能まで無効になる

**確信度：高。優先度：高。** 根拠：`BattleCompositionPlanner.cpp:60,79`、`BattleCompositionRuntime.cpp:114,136`、`BattleMatch.cpp:370–390,401–428`、`BattleGroups.cpp:242–243`。

状況→結果：P1で通常拠点番号を返し、Plannerが迫撃砲／自走砲を購入する。候補の任務不適合チェックは100/101だけなので購入できる。HirePurposeがあると `bArtillery=bStrategy&&!Purpose&&IsArtillery(...)` がfalseになり、AreaArtilleryPostで拠点から後方へ離す処理を通らず、拠点そのものを目指す。偵察の監視所・退避、装甲車の脅威回避先、戦車の観測射撃地点、対戦車の配置、外骨格の予備配置も同じ `!Purpose` に阻まれる。自動群への参加も止まる。

これは旧P1にも存在する相互作用だが、全兵種を同時比較するPlannerが使うと露出する。任務対象を固定することと、その兵種に適した配置を禁止することは別である。群役割不足の採点が高くても、その購入は群へ加入できない場合がある。

改善：任務の拠点を固定したまま、兵種固有の配置を適用する。群参加の可否は任務との整合として別判定にする。効果：砲兵の前進し過ぎと購入能力の使い残しを減らす。危険度：中〜高。**既定試合：Planner由来の任務だけに限定すればOFF不変。BattleMatchの共通 `!Purpose` を一律変更するとOFFのP1／IntelPurpose部隊にも影響する。**

### C03：個体記録上限に達すると、同じ購入失敗を繰り返し、小人数の買える候補も拾わない

**確信度：高。優先度：高。** 根拠：`BattleComposition.h:35–43`、`BattleCompositionPlanner.cpp:28,75–81,153–172`、`BattleMixedRecruitment.cpp:49–51`、`BattleCompositionRuntime.cpp:135–139`。

状況→結果：死亡記録込みの個体数が995、部隊記録は100未満、Activeにも余裕がある。小銃8人が上位ならPlannerはBuyを返すが、実行側の1000人記録上限で失敗する。2判断待って再選択しても同じ不足・スコアなら再び小銃を選ぶ。1人の戦車や3人の迫撃砲が予算・目標を満たしても、個体容量は候補除外条件にないためそちらへ移れない。記録数が減らない条件では、周期的な失敗が試合終了まで続き得る。

資金・Purchasedを失う不具合ではなく、回復不能な容量不足を一時的なspawn失敗として扱う問題。上限のチェックそのものは実行側にあるので配列破壊を主張しない。

改善：残り個体記録枠を入力し、必要人数が入らない兵種を除外する。失敗を「一時的な配置失敗」「容量」「設定」に分類し、再試行方針を分ける。効果：実行不能な購入の反復を止め、代替候補を選べる。危険度：低〜中。**既定試合：Planner限定ならOFF不変。**

### C04：旧AreaIntelの脅威拠点向け購入がPlanner ONで脱落する

**確信度：高。優先度：中。** 根拠：`BattleArea.cpp:141–145,189–195`、`BattleCompositionRuntime.cpp:112–136`、`BattleAreaIntel.cpp:62–82`。

状況→結果：AreaIntel=1、HirePurpose=0、観測された自軍拠点の脅威があるが本拠地被弾はない。旧経路はAreaIntelThreatの拠点をIntelPurposeへ記録して防衛購入を行う。Planner側はMission=-1で購入し、IntelNeedを取得せずIntelPurposeにも入れない。UpdateIntelPurposesを先に呼んでいても、新規購入への付与は代替されない。

自動目標生成と購入選択の差だけを測るつもりでAreaIntelを共通ONにすると、購入部隊の防衛任務も比較条件として変わる。現行設計追記は必要任務・本拠地被害しか列挙しておらず、旧機能のこの扱いを明示していない。

改善：観測脅威拠点をPlannerの任務入力へ接続し、旧IntelPurposeの解除条件を維持するか、互換対象外と明記して別比較にする。効果：ON時の既存防衛機能の脱落を防ぐ。危険度：中。**既定試合：Planner限定ならOFF不変。**

### C05：到着締切に購入停止時刻を使い、Stopを指定すると余分に早く購入を止める

**確信度：高。優先度：中。** 根拠：`BattleCompositionRuntime.cpp:109,126`、`BattleCompositionPlanner.cpp:28,81,99–101`、`BattleArea.cpp:183–186`。設計第6節。

状況→結果：試合終了600秒、HireStop=60秒、現在530秒、到着見積り20秒、現金で購入可能。購入自体は540秒より前、到着550秒は試合終了前だが、Plannerは `530+20>=540` により `too_late_to_arrive` で除外する。貯蓄も購入待ち＋到着を540秒までに収める。設計の「購入停止時刻」と「試合終了」を分けた制約より厳しく、旧経路より早い停止になる。Stop既定0では両時刻が一致するため差は出ない。

改善：BuyAtの締切とArrivalAtの締切を別入力にし、前者を終了−Stop、後者を試合終了または任務期限にする。効果：終盤の買える・間に合う候補を落とさない。危険度：低〜中。**既定試合：Planner OFF不変。Planner ONでもStop=0の通常値は同値を維持可能。**

### C06：貯蓄開始時の緊急度が下がった後も、割込み判定の閾値として残る

**確信度：高（条件付きの論理経路）。優先度：中。** 根拠：`BattleCompositionPlanner.cpp:115,132–150,159–164`、`BattleCompositionRuntime.cpp:112–114`、`BattleArea.cpp:303–321`。

状況→結果：同じMissionの装甲車をUrgency=3で貯蓄開始し、その後本拠地脅威が解消して装甲車の現在Urgencyが0へ低下する（装甲車の既定群役割は255）。続いて対戦車候補がUrgency=2となり、買えるとする。比較は `Best->Urgency > Next.SaveUrgency`（2>3でfalse）。通常変更の条件も `Best->Urgency==Held->Urgency`（2==0でfalse）なのでHeldを選び続ける。現在の低緊急目標が、より高い緊急候補を遮る。

Missionが101→-1へ変わる通常ケースではHeldが無効になり、この例にはならない。成立する具体条件はP1有効で既存の防衛目的部隊がいる場合：本拠地被弾中でもAreaHireNeedは通常拠点を返し得るため、被弾解消後も同じ拠点任務を維持できる。装甲車Keep・解禁・前線最低量・正の収入など他の候補条件を満たす必要がある。試合中の頻度は未測定。

改善：開始時緊急度は診断履歴として保持しつつ、割込みの比較には現在のHeldの緊急度を使う方針を定義する。効果：古い脅威の優先順位による拘束を防ぐ。危険度：中（割込みとヒステリシス）。**既定試合：Planner OFF不変。**

### C07：F01の修正は識別を直すが、Teamsによる観測状態の隔離はしていない

**確信度：高。優先度：中。** 根拠：コミット `dc50d4d`、`BattlePerception.cpp:64–65,110–138`、`BattleComposition.cpp:11–13`、`BattleComposition.h:18`、`BattleAreaIntel.cpp:39`、`BattleSave.cpp:276–280`。

状況→結果：Planner=1／Teams=2／GroupStrength=0でも、`bRecordSquadType` は `Settings().bEnabled` だけを見るのでTeam 0の接触にも兵種を書き込む。Teams=0でも、旧保存を読んで試合のCompositionTeams=0になった後でも同じ。購入マスクは正しいが「OFF側の状態に一切触らない」という強い契約は成立しない。

**修正の中心部分は正しい。** 視認・検出遅延の条件を通った後に兵種を記録するため、未観測敵の種類をPlannerが直接読む修正ではない。これによりGroupStrength=0でも、通常の識別済み車両接触をRuntimeが採用できる。既定Planner=0／GroupStrength=0では以前と同じく識別を書かない。

現時点でOFF側のこの追加値を読む行動消費者は、GroupStrength有効時のAreaIntelとPlannerであり、GroupStrength=0かつPlanner非対象なら直ちにOFF側の購入が変わる証拠はない。追加の参照処理・保存する接触値は変わる。したがって「F01未修正」でも「修正だけで相手の戦術が変わる」でもない。

改善：観測者の陣営と試合の実効CompositionTeamsを確認し、GroupStrengthとのORで記録する。効果：片側と旧保存後の状態境界を明確化。危険度：低〜中（初期化順・共有報告・旧保存）。**既定試合：Planner OFFを維持しGroupStrengthの既存条件を変えなければ不変。**

### C08：保存署名は同値設定を拒否し、全実効ルールの一致も保証しない（05 S07再確認）

**確信度：高。優先度：中。** 根拠：`BattleCompositionRuntime.cpp:13–41`、`BattleSave.cpp:222–238`、`BattleState.cpp:564`。

状況→結果：小銃人数が既定8の保存を、次回 `-BattleRifleMembers=8` または `=08` で読むと、実効値が同じでも生引数が署名に加わるため拒否する。Clamp後に同じ値になる表記も同じ問題。全CPF_Config値を含むので、ゲーム判断と無関係なConfig変更も互換性へ混ざる。これは設計冒頭で保守的キーと明記された制約だが、第8節の実効値契約はまだ満たしていない。

一方、署名対象はArea/Heavy/Rifle/Supply/LeaderHealth/Armour/Group/SPGAmmoのprefixに限定される。例えば `BattleTrialSwap` は対象外で、AreaHome (`BattleArea.cpp:16`) と到着見積りの基点を変える。マップ名が同じだけではこの左右条件の一致を保証しない。署名が同じことを完全継続の根拠にできない。05 S06の共通ルール漏れも併せて参照。

同名引数は先頭だけを採用する35〜37行が存在する。古い重複引数順の指摘を未修正として再掲しない。Definitionsがstaticなので、同プロセスでTuningだけを書き換えて再試合する用途も、署名と実行表の一致を別途確認すべきである。

改善：陣営別・Clamp後の実効値とマップ／配置条件を正規化し、署名の版を持たせる。効果：不要な拒否と別ルールの受理を減らす。危険度：中（既存v4移行）。**既定試合：署名検査のみならPlanner OFFと同条件の新規試合は不変。旧v4の扱いは明示移行が必要。**

### C09：保存検証は履歴の範囲を見るが、自軍記録・収入との整合までは見ない

**確信度：高。優先度：低〜中。** 根拠：`BattleCompositionPlanner.cpp:180–188`、`BattleCompositionSave.cpp:29–35`、`BattleCompositionRuntime.cpp:69–73,92,100–117`。

状況→結果：個々のPurchasedが0〜100で総数がOwnRecords以下なら、購入したことのない兵種へ履歴を移しても通る。さらに開始部隊を含むOwnRecordsを上限とするだけなので、購入ゼロの試合に架空の購入数6を入れて戦車解禁を早める値も、そのほかの条件を満たせば拒否できない。WindowIncome／WindowHireSpentが現在の累積値より大きい保存も、各値の範囲内なら通り、次の窓で負の差分から誤った純増推定を作れる。

これは通常のCaptureが自発的に壊すという指摘ではなく、壊れた保存の受理条件。価格変更・兵種変更・旧形式移行を考慮せず単純な総費用一致を要求するのも危険。敵の観測記憶を現在の敵兵種と照合する修正は不要であり不適切。

改善：購入由来を識別できる自軍履歴との照合、少なくともWindowIncome/HireSpent<=保存累積値の単調性を検査する。効果：架空解禁・異常な収入窓を拒否。危険度：中（履歴の契約とfixture）。**既定試合：正規の保存を変えずv4検証に限定すればOFF不変。**

### C10：初回ON試合のログだけでは、貯蓄状態と失敗理由を再構成できない

**確信度：高。優先度：中。** 根拠：`BattleComposition.cpp:99–107`、`BattleCompositionRuntime.cpp:128–139`、`BattleCompositionPlanner.cpp:168–172`、`BattleSightExperiment.cpp:40–60`、`Setup/Test-WideSight.ps1:180–186`。

状況→結果：実行失敗時のHireAreaSquadのLastMessageは `spawn_failed_backoff` に集約され、実際の失敗メッセージをログへ出す前にPreviousMessageへ戻す。容量／Nav／配置／表示生成失敗をEXECUTEから区別できない。INPUTにはEffective、RoleEffective、FrontEffective、到着締切、窓基点がなく、CANDIDATEにはWaitingがない。DECISIONにはSaveStarted／SaveUrgency／Switch／Retry／SaveBlockedがなく、保存前後の内部遷移を全比較できない。

また80秒の `BATTLE_MIXED_SAVE_SOAK_RESULT failures=0` は構造検証・一部値比較であり、CompositionState全フィールドの等値検査は含まない。Test-Areaには実状態比較 (`BattleAreaSmoke.cpp:203–208`) があるが、別の実行経路。manifestのcompositionResolvedLogは設定と最初のINPUTだけで、判断履歴の保存先ではない。

改善：失敗理由を消す前に記録し、入力・状態の安定順ハッシュと貯蓄遷移フィールドを診断に追加。Soakで非空CompositionStateの完全比較を行う。効果：最初の分岐・実行失敗を特定できる。危険度：低（ログ量・試験時間）。**既定試合：診断のみならOFFのゲーム結果は不変。ログを増やしたON計測の実時間は変わり得る。**

## 3. 確信度中：評価モデルの制限と条件付きリスク

### C11：役割不足・前線最低量は、任務へ参加可能な戦力とは一致しない

**確信度：中（計算方式は確定、実害の大きさは未測定）。** 根拠：`BattleComposition.cpp:55–78`、`BattleCompositionPlanner.cpp:53–80`、`BattleGroups.cpp:123–128,241–255`。

状況→結果：本拠地に固定された小銃隊や、任務の異なる隊もFrontEffectiveへ入る。群外の部隊もEmpty Groupの既定RoleでRoleEffectiveに算入する。逆に群の弱い小銃隊は支援役として計上するが、小銃候補のRoleDeficitは既定Role=0だけを見る。Heavy/SPGには役割定義がある一方、現在の自動群加入の兵種一覧にはない。群の不足を埋めたつもりでも実際の群へ補充できない場合がある。

追記仕様は「兵種別目標を役割へ集約する近似」「新しい群探索はしない」と明示しており、群別最適化の未実装自体は予定どおり。問題はこの値を「最低前線を維持できる」「群の空き役割を埋めた」という診断として読むこと。全員帰還中でもRetreatの割引は75%なので、前線に現在誰もいないことを表さない。

改善：まず総稼働量・任務利用可能量・実群充足を別の診断値にし、購入判断への反映は次のpolicyで評価する。効果：スコアの意味を明確にし、拘束戦力の過大評価を抑える。危険度：診断は低、判断変更は中。**既定試合：Planner内ならOFF不変。群加入ルール自体の変更はOFFにも影響する。**

### C12：20秒窓と観測時刻の境界では、貯蓄の可否が実態より遅れて変わる

**確信度：中（遅れは仕様、勝敗への害は推測）。** 根拠：`BattleCompositionRuntime.cpp:66–91,110`、`BattlePerception.cpp:10–34`、`BattleCompositionPlanner.cpp:94–101,125–126`。

状況→結果：収入源を失った直後も、次の20秒以上の窓更新までは過去の正のNetPerMinuteでSaveし得る。逆に補充支出の大きな窓の直後に補充が終わっても、次の窓までは貯蓄しない。これは予測であり、期限内に必ず購入できる保証ではない。判断周期との位相によって窓は厳密に20秒ではなく20秒以上になる。

観測はWorldの年齢をFloorしてElapsedの整数msへ移す。通常は元観測時刻を保持し、古い記憶を集約時刻まで若返らせてはいない。ただし別World原点での復元・float Elapsedの丸め・追加走査により、ms境界と次の候補が変わる余地がある（04 R04/R17）。保存されたVehicleSeenMSそのものの欠落ではない。

改善：窓の始終・収入差・購入差・その他支出・観測最終時刻を記録し、時刻境界fixtureで検証する。予測の平滑化変更は別policyにする。効果：一時的なSave/Waitを不具合と誤認せず、実際の予測誤差を評価できる。危険度：診断は低、時計・推定変更は中。**既定試合：Planner診断ならOFF不変。共通Perception時計を変える案はOFFにも影響する。**

## 4. 設計との対応・境界条件で確認できた保護

| 項目 | 実装と判定 |
|---|---|
| Buy/Save/Wait | 全候補を比較し、買えない上位がSave不適格なら次候補へ進む。旧60%境界はない。保持目標は通常候補より優先。空の有効集合はWaitであり、nullのBestを無条件参照しない（Planner:111–158） |
| 純増計算 | `OtherSpent=max(0,WindowCredits+IncomeDelta-HireDelta−Credits)`、純増=`IncomeDelta−OtherSpent`。手動購入もHireSpentに含まれるので補充支出と誤認しない。購入後Creditsだけが低下した条件で二重課金の推定にはならない（Runtime:69–73、Area:121–124） |
| 0除算 | WindowMS>=20000のみ除算。NetPerMinute<=0はSaveDelayの除算前に拒否。目標の分母はmax(1,target)、人数・最大HP・速度にも下限あり。通常実効入力から確定する0除算は見つからない |
| 予算 | FreeはCredits−Pending−Reserveを0へ丸める。Saveは別財布を作らず、Buyにも同じFreeを使う。緊急時も留保を破らない。実行側はさらにCredits・上限を検査する。通常のPlanner購入なら補充予約取消は0を期待できる |
| 目標 | 特殊兵種はPacedTargetと兵種上限、残り枠で制限。前線最低3枠を先に残す。重装は残り歩兵枠×HeavyShareの切上げ、小銃は残り。明示Keep=0とSupplyのAI除外は維持。全兵種を必ず買う保証はない |
| 設計上の優先順位差 | 本文5.1は緊急任務→前線最低→通常目標。実装は前線枠を先に確保し、FrontShortならAT・戦車等の非前線候補を全除外（Planner:36,80）。最新追記の「前線3枠」優先の具体化。装甲が迫る全滅直後にもATへ切り替わるとは限らない |
| 全滅直後 | Combatが継続して入口を通る限りFrontShortで小銃・重装・外骨格だけを候補にする。外骨格はKeep/解禁を満たす必要。CanSaveはFrontShortでfalseなので、現金不足ならWait。実際の試合終了条件でCombatを抜ければ購入しない。全滅から必ず再建する仕様ではない |
| 上限 | Active総上限・100部隊記録は早期Wait、兵種上限は候補除外。傷んだ隊をEffectiveで割り引いてもActive枠は空かない。個体1000記録はC03。最大枠100への変更でも100部隊記録の共通上限は別に残る |
| 間隔・期限 | 間隔／Retryの早期Wait中は候補・貯蓄取消の評価を飛ばす（Planner:32）。次の購入可能判断で再検査する。期限を過ぎていても早期Wait中にSaveTypeが残ることを即破損としない |
| 再試行 | 失敗でSerial+2、次の2判断Wait、3判断目で再開。期限切れ目標はSerial+3までSave不可、既に買えるならBuy可。ゲーム秒ではなく雇用判断回数。スモークにこの境界の合成検査あり |
| 貯蓄変更 | 500点差が2判断続く通常変更と元期限維持を実装。Mission変更時は一旦無効化するが、未失効の元期限をSelectionDeadlineへ使う。無条件に毎回60秒延長する実装ではない |
| 到着モデル | 直線距離×1.25／速度＋5秒。Nav到達可能性・街中車速・渋滞・必要戦闘時間は見ない。Missionなしは原点であり、実際に割り当てられる拠点や群目標ではない。追記仕様の近似で、到着保証ではない |
| 解禁履歴 | 本番はPurchased成功累計。Captureに残るID>50推定は診断用LegacyBoughtで、本番の解禁値はRuntime:116–117で上書き。両陣営の初期ID構成を変えた比較では、旧方式との解禁条件が同値かを確認する |
| 定義共通化 | 費用・人数は既存関数を使う。上限一覧・役割表・自動目標式は実行側や旧AreaCompositionと重複が残る。AllowedGroupRolesは診断・検査に使うが選択の適合条件には未使用。群任務探索・新役割・補給AIは未実装で予定範囲外 |

上表の追加診断・境界fixtureは効果＝契約の見える化、危険度＝低、既定試合の結果＝不変。目標・能力・到着近似を変える案はPlanner ONのpolicy変更として扱い、旧経路の共通関数を変更しない限りOFFは不変。

## 5. Teams・乱数・処理順の隔離

**購入経路のマスクは正しいが、対戦相手の結果・ID・乱数列まで固定する仕組みではない。**

| 条件 | 確認結果 |
|---|---|
| Planner=0、Teams=任意 | InitializeCompositionは状態を作らない。新規試合の両側は旧購入。既定の0/0を維持 |
| Planner=1、Teams=0 | 警告、購入は両側旧方式。C07の全陣営識別追加は残る |
| Teams=1 / 2 / 3 | 購入対象はTeam 0 / Team 1 / 両側。Settings.Appliesは有効範囲とbitを検査。実行はCompositionTeamsを見る |
| 通常プレイのTeam 0 | bitが立っても自動購入しない。手動の成功購入はPurchasedに入る。ExperimentだけTeam 0も自動購入 |
| AreaTrialTeam | Plannerマスクとは独立。旧Compositionと明示Keep・P1の対象は従来のTrialTeam判定を使う（ArmourAI:36–40、Area:330–342、Runtime:112）。片側Planner比較のために追加指定しない |
| 純粋Decide | グローバル乱数なし、整数全順序。定義順を変えても目標割当・候補最終選択を安定IDで決める。集約も自軍ID／接触ID順 |

ただし**共有ID割当による相手側乱数への波及はある（確信度：高）**。`BattleMixedRecruitment.cpp:82–91` は両軍共通StoreへRegisterUnit/CreateLeaderSquadし、`BattleState.cpp:101,497` は共通NextID/NextSquadIDを進める。ON側が8人小銃の代わりに1人戦車を買う、またはSaveして購入を遅らせると、その後のOFF側の購入兵のUnitIDも変わる。通常射撃seedはUnitIDを含む (`BattleTactics.cpp:207–208`) ため、共通乱数を消費しなくても新規兵の散布列は変わる。部隊IDを使う同点・配置、共有100部隊／1000人記録の残枠にも波及する。HireSightRefresh=1なら購入時の全陣営認識走査も変わる（Recruitment:103）。既定は0。

これはマスク漏れとは別の既存シミュレーション構造であり、Planner単独で「OFF側の乱数・処理順が完全不変」と認定できない。購入→兵数／ID→反復集合の変更と、戦場を介する正当な敵味方相互作用を分けて記録する。

改善案：まず同じOFF側入力を与える購入判断fixtureと、実戦の片側比較を分ける。効果＝選択器の差を識別、危険度＝低、既定結果＝不変。陣営別IDや論理出生番号で乱数を分離する変更は危険度＝高、効果＝実験上の乱数対応改善だが、**OFFを含む既存試合の散布・保存・順序を変えるため独立の既定0試験が必要**。今回のPlanner修正に混ぜない。

## 6. 観測境界と保存往復

本番Plannerは敵の現在HP・兵種・生死を引いて候補を採点していない。Runtime:76の全Units走査は自軍観測者の抽出、Capture:53–75は自軍の稼働量。全軍のRecordsは購入容量という制約であり敵戦力の採点ではない。Runtime:84–88はObservedSquadType・Confidence・LastSeenTimeを読む。未観測死亡を理由に車両記憶を消すこともない。03の許容判定を維持する。

Capture:80–87は旧KnownVehiclesを数えるが、本番はRuntime:107で専用記憶数へ上書きし、旧AreaCompositionのKnownEnemyVehiclesも呼ばない。AreaHireNeedの敵入力は接触の既知位置と観測済み本拠地HP。AreaIntentは公開時刻・自軍設定と観測所有を使う。旧AreaIntel等に残る03の真値問題を、Plannerの専用車両入力にそのまま転記しない。

長距離偵察接触は `IsScoutLongContact` で除外し、PlannerはScoutReportsを直接読まない。したがって「偵察報告だけで遠方車両を検知したら必ずATを買う」は現実装の受入条件ではない。新たに接続するなら報告の兵種・観測時刻の契約が必要。効果＝情報利用拡張、危険度＝中、Planner限定ならOFF不変。

v4は選択対象2陣営分の状態、policy、RulesKey、マスクを保存する（Save:141–144）。Elapsed・DirectorClock・Areaも保存し、SaveStarted/Deadline/Window/VehicleSeenMSは同じElapsed座標で復元するため、World時刻を加算してはいけない。無効側の状態は空との完全比較、配列長・型・期限順・範囲・観測IDの敵陣営性を検査する。敵の現在兵種とは比較しない。保存時の期限切れを一律拒否せず次のDecideで処理するのも妥当。

旧v0〜3ロードはその試合のPlannerを明示無効化し、次の保存はv3になる。起動引数がONでも自動変換されない。v4は現在起動マスク・署名・貯蓄価格を適用前に照合する。ただしC08/C09と、04の追加認識走査・時計／状態再構築は残る。**CompositionStateがcodecで一致すること、次回入力が一致すること、試合全体が一致することは別の条件**である。

## 7. 最初のON試合で見るログと期待値（実行していない）

スクリプト引数は `-AreaCompositionPlanner 1 -AreaCompositionPlannerTeams 2`、UEへ届く引数は `-BattleAreaCompositionPlanner=1 -BattleAreaCompositionPlannerTeams=2`。直接UE起動で短いスクリプト名を渡しても読み取られない。主比較は両軍Composition=1、AreaTrialTeam未指定、Planner片側。GroupStrength=0を維持してF01修正を確認する。遮蔽・連携は設計どおり `-CoverActions 1 -FlankCoordination 1` を共通にする。

| ログ行／証拠 | 期待値・読み方 |
|---|---|
| `BATTLE_COMPOSITION_CONFIG`（Composition:114–115） | teams=2ならteam0=0,team1=1,purchase=planner。teams=0の警告が出たらON試合として扱わない。stage=2は段階ラベルで、購入未接続という意味ではない |
| `BATTLE_COMPOSITION_DEF`（:116） | type=0〜9の10件、価格・人数・上限を実効設定と照合。type9はai_eligible=0。role=255の兵種を自動群加入可と解釈しない |
| `BATTLE_COMPOSITION_RULES`（Runtime:53） | policy=1、32桁key、teams=2。同一条件の新規・再開で一致。キー一致だけで全ルール一致とはしない（C08） |
| `BATTLE_COMPOSITION_INPUT`（Runtime:128） | 対象陣営のみ、通常約5秒周期。free=max(0,credits−pending−reserve)。GroupStrength=0でも実際に識別した車両があれば、その後の判断でobserved_vehicles>0。未観測車両を置いただけなら0。初期Active上限でWaitが続くのは正常 |
| `BATTLE_COMPOSITION_CANDIDATE`（Runtime:131） | 候補評価した判断は10件。早期Waitでは0件。type9はnot_ai_eligible、Keep=0/未解禁はzero_or_locked_target、上限はtype_limit。deficit/role_deficitは0〜1000、urgencyは0〜3。score=4×deficit+3×role_deficit−clamp(arrival_ms/60,0,1000) |
| `BATTLE_COMPOSITION_DECISION`（Runtime:132） | action=0 Wait／1 Save／2 Buy。Buy/Saveのtypeは0〜8。最初の20秒窓前はnet_per_minute=0で新規Saveなし。収入停止／保留時もSaveしない。Buyはcost<=free。既存目標の保持があるため毎回CANDIDATE先頭を買うとは限らない |
| Saveの連続行 | 費用未達で期限内なら同じtype/mission/deadline_msを維持。Free=23→24の60%境界だけで変更しない。通常の目標切替でも元期限を延ばさない。開始／切替内部値は現ログで不足（C10） |
| `reason=expired; ...` | 同じ兵種への新規Saveは次3判断禁止。現金で買える場合は即Buyも許可。別兵種へその判断で進める。購入間隔の早期Waitが期限処理を遅らせる場合あり |
| `BATTLE_COMPOSITION_EXECUTE`（Runtime:138） | Buy一件につき一件。success=1ならsquadが有効、reason=purchased。success=0ならsquad=-1、reason=spawn_failed_backoff、次2判断はWait。失敗の原文は今は出ない |
| `BATTLE_AREA_HIRE`（Area:131） | EXECUTE成功とteam/squadが一致、auto=1、costがDEFと一致、購入前後のcredits差がcost。通常Plannerでcancelled_replacements=0。OFF側にもこの旧共通ログは出るがCOMPOSITION_DECISIONは出ない |
| `BATTLE_AREA_HIRE_PURPOSE` / `BATTLE_AREA_INTEL_HIRE` | 現Planner経路はこれらを出さない。出ないことを任務未付与と解釈しない。DECISIONのmission>=0と成功EXECUTEからC01/C02対象を追う。現在は任務解除・実命令確認を追加する必要あり |
| `BATTLE_GROUP_FORM/JOIN/PHASE`（Groups:189,265,278） | 購入後の運用を確認。加入兵種・既存群枠・任務拘束で加入しないこともある。JOINは人数だけなので購入SIDとの対応はこのログだけでは不足 |
| `BATTLE_COMPOSITION_LOAD legacy save: ...`（Save:280） | 新規v4の往復試合では出ないこと。出たら以後Planner無効なので、ON継続成功と数えない |
| `BATTLE_MIXED_SAVE_SOAK_RESULT failures=0`（SightExperiment:60） | RoundTripSave時に80秒付近で必要。ただしPlanner全状態・判断列の一致証明ではない。別途非空Save中の全状態比較が必要 |
| `SIGHT_EXPERIMENT_RESULT failures=0 ...`（:293） | 結果書出し・完走を確認。winner/seconds、プロセスexitCode、result.json、manifestも照合。これだけでPlannerの意味的正しさは判定しない |
| `BATTLE_AREA_CHECK PASS ...planner...` / `BATTLE_AREA_RESULT failures=0` | Test-Areaの合成選択・codec・実状態往復確認。標準ではPlanner OFFでも純関数試験を行う。ON実戦と区別する |
| `BATTLE_SMOKE_RESULT failures=0` | 将来修正時の基本回帰に必要な別試験。今回実施していないし、ON試合の成功を代用する行でもない |

収入窓の机上期待例：20秒で収入20、購入12、補充5ならNetPerMinute=45。手動購入をさらに10増やしても、それをHireSpentに含めるのでNetPerMinuteは45のまま、Freeは10減る。負の純増なら値は負でよく、Saveだけを止める。収入窓・予約・留保を別々にログ照合する。

最初の実戦だけでは全遷移が起こらない。意図的に作るべき条件は、40費用にFree=0/23/24/39/40、収入ゼロ／負、補充予約と手動購入、Save中の脅威発生と解消、同じ任務での緊急度低下、兵種上限・総枠・100部隊・残り個体枠、全滅直後、終了／購入停止の境界、生成失敗、期限切れと3判断待ち、非空貯蓄を別プロセスで復元、敵真値だけ変更して観測固定、マスク0/1/2/3である。現行スモークで覆う条件も、Runtime→実購入→任務まで接続した試験とは区別する。

## 8. 後続の点検メモ

### 性能

- `BattleCompositionRuntime.cpp:76–91`：自軍全兵×接触の列挙と各Sort。毎候補のNavはないが、陣営共有の識別済み記憶を生成時刻を変えず再利用できるか。観測者死亡・長距離偵察除外・最終観測時刻を保持すること。
- `BattleComposition.cpp:50–78,83–87`：毎判断の部隊／Memberソート・HP取得・群役割照会。直後に上書きされるKnownVehicles集計の削除は診断Captureとの契約を確認してから。
- `BattleCompositionRuntime.cpp:128–138`：毎判断INPUT＋10候補＋DECISIONの文字列・IO。ログ有無を区別して購入判断時間・p95・最大を測る。RulesKeyの全Config反射とハッシュは毎候補ではなく初期化／保存・復元側。
- `BattleMixedRecruitment.cpp:57–103`：出現候補探索・表示生成・ConfigureVehicle・任意の全視界更新。Planner選択時間と購入実行の停止時間を分ける。C03で不可避な容量失敗を先に弾ける。
- `BattlePerception.cpp:65,135–138`：F01後の全陣営兵種照会と毎Updateの設定解析。陣営別条件化・キャッシュ化で、旧保存による実効マスク変更を見落とさない。
- 計測する回はMEASUREに従い、開始前Get-MeasurementMachineの結果をmanifest.machineへ深さ5以上で保存し描画GPUをUEログと照合する。本報告に性能実測値はない。

### 切替の棚卸し・試験の穴

- `BattleComposition.cpp:8–13` と `Runtime:44–60`：起動設定と試合の実効状態を別に出す。Planner=1/Teams=0、通常Team 0、AreaHire=0、非Combat、旧保存後を試験表へ。
- `BattleAreaSmoke.cpp:40–89,199–210`、`BattleCompositionSmoke.cpp:18–139`：合成入力の成功を実戦経路の成功に数えない。現行試験の収入は主にNetPerMinute直代入。20秒窓の実支出、C01〜C06、C07のセンサー実取得、個体容量の代替選択が不足。
- `BattleSightExperiment.cpp:40–60`：非空のCompositionState、VehicleSeenMS、Window、Purchased、Switch/Retry/SaveBlockedを全比較し、保存直後だけでなく次の判断列も比較する。04の試合全体の非保存状態は別の失敗要因として残す。
- `Setup/Test-WideSight.ps1:83,160,180–186`：引数・manifest配線は存在する。H/Iの暗黙SectorAI等を先行02と合わせて固定し、比較でAreaTrialTeamを流用しない。ReuseIdenticalによる既存結果のコピーを独立反復に数えない。
- 目標式の二重管理（ArmourAI:234–245／CompositionRuntime:98–104）、Definitionsの上限・役割と実雇用／群加入一覧、署名対象のprefixと実効スイッチ全表を照合する。
- 優先順はC01/C02/C03の実購入後の意味、C06の固定入力遷移、C10の診断を先に固め、その後ON新規試合・非空保存往復・片側比較。既定化や勝敗評価はその先とする。提案はすべて未実装・未実行。
