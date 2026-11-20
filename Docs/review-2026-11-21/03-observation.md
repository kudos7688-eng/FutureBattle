# FutureBattle 総点検 03：観測情報ルール

レビュー日：2026-11-21。対象HEAD：`3d1150c206c8e9a8c6fffe69a5f071e6f81dda91`。

## 対象・結論・制限

先行 `01-ai-layers.md` と `02-defaults.md`、両者の後続点検メモの観測情報項、STATUS冒頭の要約・直近項目、CODEMAPの機能入口を出発点に静的追跡した。以下の短縮ファイル名は `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。行番号は今回の実ファイルによる。通常の広域・エリア混成経路に加え、旧全Actor・Phase3のMixedDriver経路を区別して確認した。UEテンプレートのVariant_*はFutureBattle分隊AIの対象外。

**接触の作成・共有・忘却は基本的にスナップショット方式だが、その利用側の装甲分類、対戦車脅威評価、本拠地観測扱い、群の占領判断に境界違反が残る。** 敵位置の全面的な追跡や敵HPの数値を通常の歩兵戦術へ直接入れる経路は確認しなかった。一方、敵の死亡やランチャー残弾の消失を、観測記憶とは別に判断へ入れる具体的経路がある。

先行01 F01／02 D03の「Planner単独では観測兵種が記録されない」は、今回のHEADでは**修正済み**。`dc50d4d` により `BattlePerception.cpp:64–65,135–138` がGroupStrengthまたはPlanner有効時に識別を記録する。古い行番号・結論を再掲して未修正扱いしない。Plannerの全動作を検証済みという意味ではない。

ビルド、UE起動、試合、試験、計測、生成スクリプトは実行していない。確信度「高」はコードの条件・値の流れを確認した意味であり、現象の実機再現や勝敗差の測定ではない。開始時 `git status --short -uno` は追跡変更表示なし（個人ignoreへのアクセス警告あり）。書き込みはこの報告だけ。STATUS・journalも更新しない。

修正提案はすべて未実装。「既定結果が変わる」は、結果が変わり得るコード上の経路を指す。勝者まで変わると断定しない。挙動を変える提案は新しい試験切替を**既定0**で追加するか、既存の既定0の試験枝に限定する。先行02のとおり、ゲーム引数省略、Test-WideSight、Play-Areaは同一の既定条件ではない。

## 1. 判断層ごとの入口と敵情報の取得元

| 層 | 主な入口・値の流れ | 判定・参照先 |
|---|---|---|
| センサー | Perception::Update `BattlePerception.cpp:58` → 視線・検出待ち → `:133–142` の位置・時刻・識別コピー | 測定時の敵実体参照は許容。忘却 `:145–155` は敵HPを見ない |
| 共有報告 | `BattlePerception.cpp:383–386` でContactをコピー → `:317–336` で遅延配信 | 本文は観測値。送受信者の現在位置・生存確認は味方通信ルールで許容 |
| 戦略 | `BattleMatch.cpp:281–345,348–440`、AreaIntent、ObservedOwner、KnownEnemyBaseHP、AreaIntel | 基本は観測値。bHomeCoveredはO04、兵種別委譲先はO01/O02 |
| 雇用 | `BattleArea.cpp:136–196,296–322` → 自軍資源・購入履歴・AreaHireNeed・AreaComposition | 敵車両数はKnownEnemyVehicles経由でO01/O03。自軍被弾・補充・収入は許容 |
| 群 | `BattleGroups.cpp:291,372–405,512–542` → AreaIntel、ObservedOwner、観測EnemyTypes | 重み付き敵戦力は観測兵種。現在CapturingTeamの読み直しはO05 |
| 区画・道路 | `BattleRoutes.cpp:21–30,43–89` → Contactsと偵察報告、`BattleSectorRoute.cpp:134–175,204–209` | 敵の区画・前線は既知位置から作る。被弾起点の入力はO10、隊長の接触取り違えはO07 |
| 分隊戦術・撤退 | `BattleSquadTactics.cpp:38–64` → 自軍HP/弾薬/被害とContactコピー、`BattleSquadRetreat.cpp:33–72,79` | 基本は許容。装甲反応への委譲はO02 |
| 援護・迂回・TeamAI | `BattleSquadSupport.cpp:13–22,30–38`、`BattleSquadFlank.cpp:24–40,62–69,162–168`、`BattleTeamAI.cpp:38–44,79–82,123–125` | 味方状態と共通接触、観測位置への地形線。敵の現在Squad/HPは見ない |
| 隊長Think | `BattleLeaderRuntime.cpp:608–647` → 自隊の直接可視Contact、装甲再選択 `:638,722` | 距離・区画は観測位置。分類はO01、選択したContactと位置の不一致はO07 |
| 個人Follow | `BattleLeaderRuntime.cpp:879–889,937–960,1043–1091` | 弾温存の分類はO01/O03。射線と照準の真値は下記境界評価、回避はO09 |
| 遮蔽・姿勢 | `BattleCoverAction.cpp:79,204,235,271,375–450`、`BattleStance.cpp:152–168` | ThreatとContact.LastKnownPosition、自己状態、地形が主入力。被弾元の精度はO10 |
| 装甲・対戦車 | `BattleArmourAI.cpp:45–83,107–123,128–166,188–245`、`BattleVehicleTactics.cpp:11–60` | 現在の実体分類・生存・装備照会あり。O01/O02/O03 |
| 戦車主砲 | `BattleTankGun.cpp:30–82` → 自身の直接接触＋AreaIntel、車両優先、射線 | 分類O01、本拠地O04、古い接触への姿勢照会O06 |
| 砲兵 | `BattleArtillery.cpp:80–118,160–248,267–279` → 自身の接触＋AreaIntel、集団・陣地選択 | 陣地の距離計算は観測位置。歩兵の選別O01、本拠地O04 |
| 外骨格 | `BattleExo.cpp:277–312,364–384` → 直接接触の集団・車両優先、AreaIntelから配置 | 現在車両分類O01。射撃後の命中・ダメージはルール |
| 偵察 | `BattleScoutReport.cpp:85–150` → 自軍被害、Contacts、長距離報告、発信によるHint | 報告位置はコピー。Hintは明示の探知ルールとして許容。配信制限の整合はO11 |
| 編成Planner | `BattleCompositionRuntime.cpp:75–114` → ObservedSquadType/LastSeenTime、自軍状態 → `BattleCompositionPlanner.cpp:13` | 本番の車両入力は観測記憶。Capture内の旧KnownVehicles入力は直後に上書き。詳細は許容項 |
| 旧Actor／Phase3混成 | `BattleAIController.cpp:199–231`／`BattleMixedDriver.cpp:177–207`、`BattleSquadAI.cpp:104–137`／`BattleMixedSquadAI.cpp:79–111` | 接触候補の後で敵Actor/Proxyによる現在視線を評価し、停止・援護可否に反映。O08 |

## 2. 確信度高：違反が確認できた経路

### O01：識別を観測に保存せず、敵IDから現在の車両分類を再取得

確信度：**高**。判定：**違反**。可視フラグがあることと、識別の値を観測したことは同義ではない。

共通の実体参照は `BattleLeaderRuntime.h:190–192` のIsVehicle/HasMainGunと `BattleVehicle.cpp:36–39` のVehicleType（敵ID → Store->Find → 敵SquadID → FindSquad → SquadType）。IsVehicleは観測表ではなく生存車両のVehicleIDsを見る。`BattleVehicle.cpp:42–47` で生存者だけ登録し、死亡時に `BattleLeaderRuntime.cpp:485` で直ちに削除する。

| 判断側の参照箇所 | 具体的な状況 → 結果 |
|---|---|
| `BattleArmourAI.cpp:67,75` PickArmourTargets | 最後の走査では可視だった敵を現在の車両/装甲係数で採点 → 小銃目標の除外・距離重み、対戦車目標が変わる |
| `BattleArmourAI.cpp:165` AreaArmourReaction | 可視Contactの敵を現在の車両種とArmorSmallArmsで検査 → 歩兵の退避判断を変える |
| `BattleLeaderRuntime.cpp:882–883` Follow | 選択済み敵の現在IsVehicleで兵士扱いか決める → 対戦車弾を温存するか変わる |
| `BattleArmourAI.cpp:112–113` NoteKnownVehicles | AreaIntelの正IDを現在IsVehicleで識別 → 既知車両表へ登録。集約時刻ElapsedをK.Atに書き、元の観測時刻を失う |
| `BattleTankGun.cpp:53–56` | 自分の直接視界外でもAreaIntelに残る正IDを車両判定 → 単独車両に1000点、歩兵は最小集団数条件。未観測の車両死亡で候補が消える／歩兵側へ移る |
| `BattleArtillery.cpp:167,186` | 既知の敵から現在IsVehicleで車両を除く → 砲撃する歩兵集団の人数・重心が変わる。Clusters ON/OFFの両経路にある |
| `BattleExo.cpp:302–304` | 直接可視フラグの残る敵を現在IsVehicleで採点、周囲も同様に分類 → ランチャー温存・車両優先・集団人数が変わる |
| `BattleExo.cpp:374–381` | AreaIntelの正IDを現在車両判定 → 外骨格の配置先に+4点する拠点が変わる |

`bVisible` は走査間の連続視認を保証しない。`BattlePerception.cpp:191,214` の間は前回走査の値が残り、AreaIntelは `BattleAreaIntel.cpp:22` によりさらに1秒キャッシュされる。この窓で敵が隠れて死亡すれば、再観測なしで分類が変わる。兵種自体を戦闘中に自由に変更できると仮定する必要はない。車両登録の死亡時削除だけで成立する。

偵察報告は `BattleScoutReport.cpp:37–41` で負IDの位置列になるため、上表の正ID限定の枝はその報告から敵Squadを解決しない。全偵察報告が直接真値化される、という指摘ではない。

修正案：Contact、AreaIntelの要素、武器候補に、同じ観測由来の位置・識別・時刻・信頼度を保持する。車両不明は不明のまま扱い、未知を現在の登録表で補完しない。観測時に `ObservedSquadType` を取得する条件も全利用側に合わせる（Planner/GroupStrengthを強制ONにする回避は不可）。KnownVehiclesの時刻はLastSeenTimeから変換する。

効果：未観測の撃破・分類変化の伝播を止め、兵種別判断を同じ認識に揃える。危険度：**高**（目標・撤退・弾薬・購入が連動、保存項目も関係）。既定結果：通常の車両なし歩兵戦でこの分類による差は限定されるが、既定ON ArmourAIを使う車両参加戦は変わる。新しい観測分類切替を**既定0**。

### O02：装甲の脅威評価が敵の生存と実残弾に追随

確信度：**高**。判定：**違反**。

根拠：`BattleArmourAI.cpp:143–145`、`BattleVehicleTactics.cpp:20–25`、`BattleState.h:268`。

状況→結果：AreaIntelの接触位置が残っている間、敵の正IDをStore->Findして `IsAlive()` と `CarriesAntiTank()` を読む。CarriesAntiTankは兵種識別ではなく `Weapon==AntiTank || Launcher>0`。見えない外骨格が最後のランチャーを使い切ると、通常銃のままなら「対戦車脅威なし」へ変わる。見えない敵が死んだ場合も同様。車両の後退方向・要否、装甲車の拠点評価、戦車の待機位置を変える（`BattleArmourAI.cpp:149–156,195,214`、`BattleVehicleTactics.cpp:46`）。HasMainGunも現在の生存車両登録＋敵Squadの種別で評価する。

位置そのものはV.Enemiesのままなので、これは隠れた敵への位置追尾ではなく、**観測位置と現在の生存・能力の混合**。自軍のDamageRatioから「大きな被弾で後退」は許容であり、そこまで除く必要はない。

修正案：識別時の脅威能力を保存し、残弾の確定情報が観測できない限り「対戦車能力を持つ可能性」を期限付きで維持する。死亡も視認・報告イベントがない限り削除理由にしない。

効果：見えない敵の弾切れ・死亡を使う安全判断を防ぐ。危険度：**中〜高**（待機位置・後退頻度が変わる）。既定結果：車両参加戦は変わり得る。歩兵のみの通常戦は該当車両判断なし。修正切替は**既定0**。

### O03：「全戦場に車両が存在するか」がAIと記憶の有効性を決める

確信度：**高**。判定：**違反**。負荷回避用の全体ガードが判断の意味を変えている。

根拠：`BattleLeaderRuntime.cpp:638,722,879–883`、`BattleArmourAI.cpp:110,118,132`、車両死亡時の削除 `BattleLeaderRuntime.cpp:485`。

状況→結果：自軍に車両がなく、敵の最後の車両が視界外で死ぬとVehicleIDsが空になる。KnownVehiclesに有効期限内の記憶が残っていても `KnownEnemyVehicles` は118行で0を返す。FollowのbArmourAIもfalseとなり、対戦車弾の温存が解除される。旧AreaCompositionの購入数とAreaATTargetもこの0を読む（`BattleArmourAI.cpp:224,239–245`）。別の未知の車両が存在すれば空にならず、同じ観測内容でも判断が変わる。

修正案：自軍に関連武器があるか、観測車両記憶があるか、という自軍/観測由来の条件で処理を省略する。KnownEnemyVehiclesは実体登録表の空判定を通さず観測表の期限だけを読む。

効果：未観測の最後の車両撃破を全軍へ知らせる経路を除く。危険度：**中**。既定結果：車両なしで始まり終わる試合は不変を狙えるが、車両消失を含む試合は変わる。切替**既定0**。現在のPlannerのVehicleSeenMSはこのガードを使わない。

### O04：距離だけで「敵本拠地を観測した」とする（先行F07）

確信度：**高**。判定：**違反**。敵の真値を直接コピーする形でなくても、未観測域を観測済みの空域として判断する問題。

根拠：`BattleAreaIntel.cpp:34,42–44` → `BattleMatch.cpp:339`、`BattleArtillery.cpp:242`、`BattleTankGun.cpp:72`。

状況→結果：味方が本拠地からSightRange内にいるだけで、壁・視野角を検査せずbHomeCovered=true。偵察の場合も、別方向の敵を報告した地点が敵本拠地から長距離視界内ならtrueになる。既知HomeEnemiesが少ないとAreaIntel戦略は攻撃比率を増やす。砲兵は実観測がなくても本拠地を砲撃候補にする。主砲はその後に独自の射線確認があるため壁を無条件に撃ち抜くわけではないが、本拠地認識の許可は距離だけで代替される。

修正案：幾何的な到達範囲と、視野・遮蔽を通した観測の時刻/範囲を別に記録する。実観測の出発点は `BattleAreaIntel.cpp:35–37,86–93` のArtilleryHomeObserved。ただし本拠地一点の可視だけで「周辺守備兵も少ない」と確定せず、敵不在の確度を別に持つ。偵察報告には観測方向・範囲を保存する。

効果：壁の向こうを偵察済み扱いする戦略・砲撃を防ぐ。危険度：**中〜高**。既定結果：AreaIntelの戦略利用は既定OFF、砲兵/主砲は存在時に変わり得る。ArtilleryObservedは**既定0を維持**。それをONにしても主砲・戦略のbHomeCoveredは修正されないため、共通修正も別の**既定0**枝が必要。WeaponOrderGateは命令許可であり観測制限の代替ではない。

### O05：群の「押している」判定が現在の占領状態を読む

確信度：**高**。判定：**違反**（ゲームルールによる占領更新そのものとは別）。

根拠：`BattleGroups.cpp:392–405`、観測保存 `BattleArea.cpp:217–218`、占領ルール `BattleMatch.cpp:214–246`。

状況→結果：1秒以内に拠点を見たbSeenを条件に、現在の `Objectives[...].CapturingTeam` を読む。観測時に保存されるのはOwnerとSeenAtだけ。視線が切れた直後、敵の侵入で占領が止まった場合でも、再観測なしでbPushing・許容損失率・前進時間延長が変わる。自軍が占領域にいることだけで、隠れた敵による中断を知ってよいとは保証できない。

修正案：拠点の観測更新時にCapturingTeam/Contestedもスナップショットへ保存し、群はそれを参照する。占領表示が全軍に公開される正式ルールなら、その公開イベントを独立APIとして定義する。現在の「見た時刻だけをゲートに真値を読む」構造は避ける。

効果：占領ルールから未観測の敵介入が漏れるのを防ぐ。危険度：**中**（群の粘り・撤退と保存形式）。既定結果：**変わり得る**。AreaGroupsは現在既定ONで、歩兵だけの試合も対象。修正切替**既定0**。

### O06：主砲の候補選択に、見えていない対象の現在姿勢が混ざる

確信度：**高**。判定：**違反**（照準だけに閉じていない）。

根拠：`BattleTankGun.cpp:44–48,61–65`、`BattleBlindFire.cpp:94–96`。

状況→結果：AreaIntelから得た古い正IDの位置に `TargetAimOffset(C.Key)` を足す。これは現在のStance/頭部露出を読む。過去に見た座標で敵の現在の伏せ・しゃがみ・ブラインドファイア状態を反映したAimを作り、射線検査で候補を採用するか、次候補へ移るかを決める。敵が移動済みなら、古い座標＋現在姿勢という、どの観測にも存在しない標本になる。

修正案：記憶・陣営報告だけの候補評価では観測時の照準高さまたは不確かな標準高さを使う。自分の現在の測定が成功した対象についてだけ、射撃実行で現在の露出点を使う。物理トレースの実体照会は残す。

効果：隠れた姿勢による目標選択の変化を防ぐ。危険度：**中**（射線が通る候補が変わる）。既定結果：主砲使用戦で変わり得る、通常の歩兵戦は対象外。切替**既定0**。

## 3. 確信度高：真値漏洩とは区別する観測整合の問題

### O07：選択した敵IDに対し、最初の別Contactから位置を取り直す

確信度：**高**（コード経路）。判定：**真値参照違反ではないが、観測の一貫性の欠陥**。

根拠：`BattleLeaderRuntime.cpp:608–616,624–631,643,647,666–667,685,692,709,715`。集約側にも `BattleAreaIntel.cpp:39`、`BattleRoutes.cpp:28`、`BattleArea.cpp:388` のSeen.Addによる後勝ちがある。

状況→結果：隊員Bの新しい可視Contactが目標選択を成立させても、後でMembersの先頭Aの古い/共有ContactがあればそれをTargetAtにする。643/647行は時刻・可視・共有・生存を比較しない。区画外判定、遮蔽の脅威方向、PinnedAt、CloseInAt、到着配置の向きが、選択を成立させた情報と食い違う。AreaIntel等も同じIDを見た複数隊員から最新を選ぶ保証がない。古い情報の利用自体は許容だが、新しい根拠を捨てる理由がない。

修正案：選択と同時にContact全体/観測元を保持する。陣営集約も `BattleSquadTactics.cpp:60–63` のようにLastSeenTime、可視、SourceUnitID等の安定比較で選ぶ。観測位置・種別を別々のContactから取らない。

効果：観測時点の混合と列挙順依存を減らす。危険度：**中**。既定結果：集約修正は既定群にも影響し得る。隊長の詳細戦術は関連切替ON時が中心。修正切替**既定0**。現象の実機再現は未実施。

## 4. 確信度中：センサー・実行・判断の境界

### O08：即時射線確認の結果が停止・援護判断にも戻る

確信度：**中**（値の流れは確認、違反判定は即時センサーの契約による）。判定：**測定自体は許容、判断入力の境界は要明文化**。

根拠：通常Follow `BattleLeaderRuntime.cpp:854–862,887–889,911–912,1088–1091`。旧Actor `BattleAIController.cpp:203–210,228–231`、MixedDriver `BattleMixedDriver.cpp:181–188,206–207`。旧分隊の援護可否 `BattleSquadAI.cpp:104–110,123–137`、混成版 `BattleMixedSquadAI.cpp:79–85,98–111`。

状況→結果：ContactのbVisibleが残る間にも敵Actorの現在位置・生存を再評価してCanSeeを得る。旧経路はその成否でTargetの採否、停止、援護喪失を決める。通常Followの並列先読みも、先に撃破された敵を現在IsAliveで即座に不射撃へする。敵HPを数値として戦略へ渡すわけではなく、Contactを死亡確認済みに書き換えるわけでもない。

センサーの追加測定として定義すれば、視線を再確認すること自体はユーザーの許容範囲。単なる発射可否検査と位置づけるなら、その失敗を「死亡」「援護不能」と判断層へ戻すのは境界越えになる。現在は失敗理由（遮蔽/見失い/消失/射程）がboolに潰れ、敵死亡を視認したかは区別しない。通常Followの移動停止は主にP.TargetIDで決まり、このboolだけで即時に追撃へ切り替わるとは断定しない。

修正案：即時センサー結果を時刻・理由付きの観測として出し、発射可否と戦術入力を分ける。観測不能は「死亡」ではなく「見失い」。効果：視線更新頻度と判断の契約を明確化。危険度：**中〜高**。既定結果：通常/旧経路とも変更範囲次第で変わる。挙動変更は**既定0**、APIの明文化だけなら不変。

### O09：近接回避が未観測の敵位置・速度・死亡を読んで経路を選ぶ

確信度：**中**（敵も含む参照は確定、物理実行の許容範囲が設計上の論点）。判定：**戦術入力としては違反、局所衝突センサーとしては条件付き許容**。

根拠：全生存者の格子 `BattleLeaderRuntime.cpp:1365–1366`、単純Followの分離 `:1005–1013`、StableMove `BattleStableMovement.cpp:375–428`。

状況→結果：格子に敵味方が入り、Team/Contactによる観測制限なしで位置から減速・分離する。StableMoveは敵にもBodiesの現在Velocityを使ってSameFlowを判定し、Blockerを選び、その実位置の左右へ持続するPassingWaypointを作る。PassingIDの現在死亡・位置で解除/更新する。壁越し・視野外でも距離条件だけで影響し得る（具体的配置での再現は未実施）。敵の分隊経路を読むOtherPlanは味方限定なので、そこは漏洩扱いしない。

実際のSweep・衝突解決はゲームルールとして許容。しかし事前の左右迂回選択まで「接触測定」と扱うなら、壁越しや高さ違いの候補を含めない条件が必要。

修正案：味方は現状の真値利用を維持し、敵は短距離の物理接触/近接センサー結果へ制限する。物理SweepをContactsだけで置き換えて衝突を失わせない。効果：未観測の敵の進行方向を使う先回り回避を防ぐ。危険度：**高**（詰まり・移動安定性）。既定結果：歩兵の通常移動でも変わり得る。試験切替**既定0**。

### O10：被弾の「方向」から、敵射撃原点の正確な座標が遮蔽判断へ届く

確信度：**中**（座標の伝播は確定、被弾知覚の仕様として許す精度が未定）。判定：**被弾検知は許容、未観測射手の距離まで確定する利用は違反候補**。

根拠：`BattleLeaderRuntime.cpp:1208` のApplyDamageFrom(Start) → `BattleState.cpp:465–469` のPendingHitFrom → `BattleLeaderRuntime.cpp:1503` のShotFrom → `:666,957–960`。`BattleLeaderRuntime.cpp:1487–1490` は分隊内で最新の被弾元を集約する。

状況→結果：見えない射手から被弾すると射撃時の正確なStartが記録され、ShotDirection/UnderFireCoverでContact位置を置き換える。これはその後の射手移動を追尾する値ではないが、単なる到来方向より強い情報で、遮蔽候補の露出線の起点になる。砲撃/爆風のFromは爆心（`BattleArtillery.cpp:384`、`BattleAntiTank.cpp:25`）なので、常に敵砲の位置が分かるわけではない。

修正案：Damageイベントから知覚用の方位・誤差・時刻を作り、未識別射手は距離不明とする。味方間の共有もその測定値をコピーする。効果：隠れた射手の正確な起点を使う遮蔽最適化を抑える。危険度：**中**。既定結果：CoverPriority=1は現在記録するが、単独ではこの起点への置換は行わない。ShotDirection/UnderFireCoverの既定0を維持して枝内で評価すれば通常既定は不変を狙える。新しい精度変更も**既定0**。

### O11：長距離偵察の配信制限が、現在の味方状態と受信者距離で判定される

確信度：**中**（条件差は確定、報告待ちを迂回する発生頻度は未検証）。判定：**敵真値参照ではないが、観測の出所・配信ルールに欠陥候補**。

根拠：`BattleScoutReport.cpp:25–31`、`BattleAreaIntel.cpp:39`、`BattleRoutes.cpp:28`、`BattleCompositionRuntime.cpp:83–88`、`BattlePerception.cpp:353–355`。

状況→結果：IsScoutLongContactはContact取得時の測定方式ではなく、今の観測者のHasLongSightと位置で判定する。長距離で見た後に偵察兵が動き始め、次の走査前に集約すると、同じbVisible接触が長距離でなくなって報告待ちから外れる可能性がある。通常共有はSightRangeの1.1倍まで許すが、長距離判定は1.0倍なので境界帯が一致しない。Plannerは共有Contactも受け取り、受信者に対してIsScoutLongContactを評価するため、出所の長距離制限を再現しない。

修正案：Contactに取得時のセンサー種別と配信可否を持たせ、共有コピーでも維持する。生存/自軍位置の照会を禁止する問題ではない。効果：偵察報告の待ち時間と共有の意味を揃える。危険度：**中**。既定結果：偵察・監視所を使う戦いでは変わり得る。修正切替**既定0**。Plannerのみの試験で直す場合も共通Perceptionを無条件変更しない。

## 5. 許容と判断した真値参照・否定できた疑い

| 箇所・確信度 | 状況 → 結果、判定理由 |
|---|---|
| `BattlePerception.cpp:69–77,101–142,145–155`／高 | 生存者・現在位置・種別をセンサー候補として測定し、視認＋検出待ち成功後だけContactへ保存。未観測記憶の忘却は年齢だけ。**許容**。見えない敵のHP低下でConfidenceを変える処理はない |
| `BattlePerception.cpp:317–336,383–386`／高 | 共有本文はコピー。配信時に対象の現在位置・HPを引かず、送受信者の生存・Revision・通信距離と元LastSeenTimeを確認。**許容**。受信時刻への若返りはない |
| `BattleScoutReport.cpp:139–150`、`BattlePerception.cpp:396–402`／高 | 偵察位置報告は観測位置配列。発信中の偵察兵を周囲の敵が探知するHintは、距離・格子丸め・0.5信頼度・非可視の明示ルール。**センサーとして許容**。F07の「通常視界で見たことにする」とは異なる。Hintから真の射手位置を追尾しない |
| `BattleSquadTactics.cpp:47–63`、`BattleSquadRetreat.cpp:33–72`／高 | 自軍HP/弾薬/被害とContactから退避する。**許容**。敵HPを読むのは装甲反応のIsAlive等O02に限定して指摘し、分隊戦術全体が真値判断とは扱わない |
| `BattleCoverAction.cpp:86,208,241,279,389–450`、`BattleTacticalAI.cpp:44–53`／高 | Threat/最後の観測位置から地形の露出・覗き・射程を評価。敵Actorの現在座標で隠れ場所を再評価する経路はここにはない。**許容**。O10の入力元は別問題 |
| `BattleLeaderRuntime.cpp:1049–1051`／高 | CanEngageの即時視線確認後、現在SideExposure/TargetAimOffsetで照準する。位置が変わるのは向き・Aim・発射に使用。ここからThinkのTargetAtや遮蔽Threatへ書き戻す経路はない。**照準として許容**。車体も射撃方向へ向く `:1072–1074` が、未観測の別目標の選択ではない |
| `BattleExo.cpp:309–312`／高 | 自身の直接可視Contactから選んだ候補に、肩からの射線検査と現在の照準高さを使う。**発射可否として許容**。分類採点はO01。主砲O06は陣営の古い接触を使い複数候補の選択へ戻すため区別する |
| `BattleTankGun.cpp:94–110,124–134`、`BattleArtillery.cpp:368–390`、`BattleAntiTank.cpp:6–35`、`BattleLeaderRuntime.cpp:1187–1283`／高 | 現在位置・HP・種別で命中/爆風/装甲/ダメージを処理し、結果を計測・表示へ記録。**ゲームルールとして許容**。Fireのboolは最終的に発射成功で、敵の残HPを隊長の評価点へ返していない。被弾元の知覚出力だけO10 |
| `BattleArea.cpp:217–220`／高 | AreaVisible成功時にKnownOwners/KnownEnemyBaseHPを更新。**許容**。通常戦略の低HP判定 `BattleMatch.cpp:335–336` と雇用 `BattleArea.cpp:321` はこの保存値を読む |
| `BattleArea.cpp:61`、`BattleTankGun.cpp:69`、`BattleArtillery.cpp:239`／高 | 敵本拠地の現在BaseHP>0を発射候補の存在条件に使う。ただしHPの大小で戦術採点せず、0は `BattleArea.cpp:70` で即試合終了。固定本拠地位置も公知のマップ情報。**現行ルールでは許容**。隠れたHP低下を攻撃配分へ戻す経路ではない。将来、破壊後も継戦するなら見直す |
| `BattleMatch.cpp:214–246`、`BattleArea.cpp:199–204,223–242,273–282`／高 | 占領・収入・補給は真の占領者/接近/自軍残弾で処理。**許容**。実収入をPlannerが `BattleCompositionRuntime.cpp:64–72` で推定するのも自軍経済の観測。敵所有者を直接読んで推定していない |
| `BattleCompositionRuntime.cpp:75–91,106–107`／高 | 観測種別と元観測年齢からVehicleSeenMSを更新。敵実体を引かず、未観測死亡で即消さない。Capture `BattleComposition.cpp:80–87` が読む旧KnownVehiclesの数は本番では107行で上書きされる。**本番の車両入力は許容**。診断Capture単体はO01/O03由来のキャッシュを表示し得る |
| `BattleComposition.cpp:47`、`BattleCompositionPlanner.cpp:28`／高 | 全軍の部隊レコード数を100件の容量条件に使う。敵戦力を採点する入力ではなく、購入可能性という実装/ゲーム上の制約。**許容**。敵編成を推測する用途へ流用しない |
| `BattleArea.cpp:17`、`BattleMatch.cpp:268–278`／高 | 非AreaではObservedOwnerがObjectives.Ownerを返す旧ルール。旧アリーナの拠点所有は公開ステータスとして命令を選ぶ。**公開ルールとして許容**。Areaで敵所有者を見ずに更新する経路と混同しない |
| `BattleScoutReport.cpp:51–70`、`BattleLeaderRuntime.cpp:1211–1278`、`BattleContextTrial.cpp:18–24`／高 | 真の位置/速度/被害/戦術状態をログ・比較ハッシュに使用。確認した参照では試験の真値を通常目標選択の入力へ戻していない。**診断として許容**。試験fixtureの状態注入まで禁止すべきという意味ではない |

### 死亡・HPの情報が伝わる経路のまとめ

通常のContactにはHPがなく、未観測の対象死亡でContactsを一斉削除する処理もない。Observersから死んだ**観測者自身**を消す `BattlePerception.cpp:85–86` は、自軍の観測能力喪失であり敵死亡通知ではない。

問題になる経路は、(1) VehicleIDsの死亡時削除→分類と全体ガード（O01/O03）、(2) AreaIntelのIDからIsAlive/Launcherを読む（O02）、(3) 即時射線・近接回避で現在の実体消失を参照する（O08/O09）。HPの数値低下そのものを通常の撤退点数へ使う経路は確認しなかった。味方のHP低下と被弾元情報はO10の境界として別扱いにする。

## 6. 修正順と後続の点検へのメモ

優先順は、O01/O02/O03の分類・脅威・記憶を一つの観測モデルに揃え、O04/O05の拠点観測を分離し、O06/O07の同一標本性を直す。O08〜O11はセンサーの出力契約を先に定義する。複数の修正を一度に既定化せず、観測値を固定して未観測の敵真値だけを変える比較で、判断が不変か確かめる（今回は実行していない）。

### 再現性

- `BattleLeaderRuntime.cpp:1340–1350`：Match/分隊戦術/外骨格副武器は当該フレームのPerceptionより前。`bVisible`を連続可視とみなすテストを避け、走査前後・死亡前後・1秒Intelキャッシュの境界で比較する。
- `BattleLeaderRuntime.cpp:643,647`、`BattleAreaIntel.cpp:39`、`BattleRoutes.cpp:28`：Members/TMapの列挙順と接触選択。敵IDが同じでも観測元・時刻・姿勢を揃えなければ同条件ではない。
- `BattleTankGun.cpp:44–65`、`BattleArtillery.cpp:160–170,267–279`：自己接触優先と陣営集約、負IDの偵察報告、集団重複、キャッシュ構築順。同じ敵が直接観測と偵察位置の双方に入り得る。これを真値漏洩と混同せず、人数過大評価・砲撃採点を調べる。
- `BattleLeaderRuntime.cpp:1368–1369`：先読み射線と同フレームの先行兵による撃破。O08の即時生存チェックを外す場合、発射列・死体への弾消費が変わる。
- `BattlePhase3.cpp:398–399`：EqualContactはObservedSquadTypeを比較項目に含めていない。位置・時刻が一致しただけで識別まで同一と保証しない。
- 前回02の起動条件差を継続。Planner/GroupStrength/ArmourAI/ArtilleryObserved、隊長・個人・Perceptionの更新間隔とキャッシュ時刻を比較キーへ含める。

### 保存・読み込み

- `BattlePerception.cpp:10–34`：LastSeenTime/配信時刻は相対化、復元時bVisible=false、走査時計はReset。識別が正しく残っても復元直後の選択結果が同じとは限らない。
- `BattleSave.cpp:112`：識別値の範囲とID/陣営関係は検証される。観測対象の現在兵種と一致することを検証条件に足さない（古い正当な識別記憶を破壊する）。死んだ敵を指す記憶・配信待ち報告も対象にする。
- `BattleCompositionRuntime.cpp:85–91`、`BattleCompositionSave.cpp`：World時刻とElapsedの換算、VehicleSeenMSの期限、保存往復での観測時刻の若返りを調べる。
- `BattleArmourAI.cpp:113` のK.Atは観測時刻ではなく集約時刻。KnownVehicles/IntelViews/EnemyTypes、砲兵クラスタ、主砲Aim、ShotFrom、PassingIDが保存・再構築される順序を追う。再構築に敵現在分類を使うとロードだけで判断が変わる。
- 拠点のCapturedTeam等の観測スナップショット追加時はKnownOwners/SeenAtと同じ版・寿命を扱う。群の現在真値を保存して「観測」として復元しない。

### 性能

- O01/O02を直す際、各AI層で敵IDに対して追加のLOSを毎回行わない。Perception更新で一度取得した識別をコピーし、同一陣営・同一版のIntelを共有する。賢さと処理頻度の調整を分離する。
- `BattleAreaIntel.cpp:19–52`：AreaIntel OFFでも群・砲兵から集約される。1秒キャッシュの費用だけでなく、NoteKnownVehiclesの副作用とArtilleryObservedの追加トレースを別計上する。
- `BattleArtillery.cpp:140–170,215–226`、`BattleTankGun.cpp:50–58`、`BattleExo.cpp:298–305`：観測集団の二重走査を共有する場合も、入力の識別・時刻・自己接触優先を変えない。負ID報告の重複除去は挙動変更なので別実験。
- `BattleStableMovement.cpp:387–428`：近接センサー化で全個体へ重い視線探索を追加しない。格子・候補上限・局所Sweepを活かし、物理回避と敵戦術情報を分離する。
- 計測を行う後続作業ではMEASURE.mdに従い、開始前にGet-MeasurementMachineの結果をmanifest.machineへ保存する。本報告には実測値・試験合格の主張はない。

本報告以外のソース、Config、既存資料、STATUS、journalは変更していない。
