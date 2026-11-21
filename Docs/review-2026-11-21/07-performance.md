# FutureBattle 総点検 07：CPU性能

レビュー日：2026-11-21。対象HEAD：`b1dbdaa5cf0c096fb32750b821ae42f8f6d74b30`。

## 範囲と結論

優先するのは、遮蔽予約の近傍索引、HUDの重複集計と一時領域、移動・VATの一時配列である。遮蔽候補の上位k化とLOSの間引きは、そのままでは同値最適化にならない。到着配置は「同じ問い合わせを再利用する」変更と「計画を翌フレームに送る」変更を分ける。

01〜06の報告、特に各報告末尾の性能メモ、STATUSの要約・直近項目、PERFORMANCE、MEASURE、CODEMAPから入口を確認し、通常混成経路を中心に静的点検した。01 P01〜P10の一覧は再掲せず、指定4件の設計をA01〜A04、新規・追加の具体化をN01以降に記す。短縮ファイル名は `Game/FutureBattle/Source/FutureBattle/Prototype/` 配下。行番号は現HEADの実ファイルによる。

**ビルド、UE起動、試合、試験、計測は一切実施していない。処理回数はすべて机上概算で、ms短縮量・割当回数の実測ではない。** 確信度「高」は呼出経路・条件の確認を示し、効果量の確定を意味しない。見込み効果の大/中/小は当該負荷が発生する条件での相対評価。小項目を多数集めてもトレースやNavの大項目を上回るとは限らない。

開始時の `git status --short -uno` は追跡差分なし（個人git ignoreのアクセス警告あり）。書き込みは本報告だけ。STATUS/journalも更新していない。

### 先行点検から引き継ぐ条件

- 02：ゲーム引数省略、Test-WideSight、Play-Areaは同じ既定ではない。特にCoverActionsのゲーム既定は0、Play-Areaでは1。A01/A02の遮蔽中経路は後者等で効く。都市のCoverArrivalは別の既定ON経路。H/Iのスクリプト設定も固定して比較する。
- 03：敵識別・Intel共有の改善で新しいLOSを各AI層へ追加しない。現在の観測・自己接触優先と、物理判定の真値を区別する。HUDの可視敵集計もAI用Intelの単純流用は不可。
- 04：Bodies/Plans列挙順、近接回避32件上限、同点・浮動小数加算順、復元後の走査予定が同値条件。`Setup/Compare-MatchResults.py:10`では実時間率3項目の除外は既に修正済み。古いR01を未修正として再掲しない。ただし統計一致は全状態一致を保証しない点は残る。
- 05：復元のNav・一時メモリは通常フレームと別評価。現在の `BattleMixedBodies.cpp:79,105,164` は検証済み主経路を既に再利用している。「同じ主経路を必ず二度探索する」とは扱わない。
- 06：Planner識別取得は修正済み。新たに毎Updateへ入った設定解析と、ON時の集計・ログを分ける。AreaIntel OFFでも既定群から集約される性質は01 P06等の範囲なので新規件数に数えない。
- PERFORMANCEの11.61〜12.25ms等は11-18のマップE・30FPS・歩兵の旧条件。11-20追加の霧・区画表示、現HEAD、400人60FPSの実測値ではない。過去のHUD 1.04msを現在のHUD費用と認定しない。

## 1. 上位4候補の具体設計（確信度高）

### A01：P01の深化――予約の逆引きと静的な競合隣接表

**根拠：** `BattleCover.cpp:159,169,177,188,197`、`BattleCover.h:32`、`BattleCoverAction.cpp:383–387`。

**状況→追加で分かった処理量：** IsAvailableは各予約にOwnerを呼び、Ownerが同じReservationsを再Findし、さらにStateの生存をFindする。したがってP01の走査件数に加え、最大BRF回の予約ハッシュ検索と生存検索がある。遮蔽兵B=予約R=100、60FPSなら各最大60万回/秒。Releaseには期限切れ予約も残り、生存予約数だけでRを見積もると過小になる。

**データ構造と変更先：** Slotsを正規の番号付き配列のまま維持し、BuildSlots完了時に `ConflictSlots[Slot] : TArray<int32>` を作る。140cm幅の2Dセルで周囲3×3セルを抽出し、現行と同じ `DistSquared2D < Square(140.f)` を満たすslotだけを登録（自身を含む）。床の高さが違っても現行は2D判定なので、Zで分割しない。`UnitToSlot : TMap<int32,int32>` を追加し、Reserve/Release/ResetReservations/BuildSlotsの全書込箇所で維持する。

1. IsAvailableはConflictSlotsだけを調べる。Reservationを一度Findし、期限 `<=Now`・現在の生死・自己所有除外を同じ順の意味で判定。世界状態をフレーム始めに固定しない。
2. Reserveは最初に本人の生存と空きを確認し、失敗時は旧予約を保持。成功時だけ旧slotを逆引きして削除し、新slotを設定する。同slotでも他者との競合検査を省かず、期限を毎回厳密に `Now+2.5` へ更新する。
3. 期限切れslotを別人が上書きする際、旧所有者の逆索引を対応確認して削除する。そうしないと旧所有者の後のReleaseが新所有者を消す。死亡、復元、強制再構築でも両表を同期する。単一所有不変条件を検証し、導入時に破れていれば全走査へ戻す。

**同値性：保証可能な設計。** 競合の返値は「一人でも有効な他者が近いか」という副作用のないORなので、内部の隣接列挙順は採用者を変えない。Reserveを呼ぶ兵の順序と時刻、失敗時の旧予約保持を維持する。期限切れを早期に捨てる必要はない。予約TMapの内部配置を外部の順序として利用していない範囲で、同slotへのremove/addを値更新へ変えられる。復元時に兵をソートして取得し直す変更は混ぜない。

**概算：** 平均競合隣接数d=5ならB×F×d=3万slot照会/秒＋6000逆引き程度。dは仮定であり実マップの分布を測る。構築費と隣接表メモリは増えるが、Reserveのたびの全R走査はなくなる。

**見込み効果：大、危険度：中。** 計測は後述M1のBattleFollow/BattleThink、追加区間CoverReserve/CoverIsAvailable。M2で死亡直後・同slot更新・期限ちょうど・上書き後Release・2階・保存復元の予約取得列を比較。CoverActions=0の基準だけでは評価しない。

### A02：P02の深化――行動用LOSと発射準備を分けても更新時刻を保存する

**根拠：** `BattleLeaderRuntime.cpp:887–889,985–992,1043,1088–1091,1125–1126`、`BattleCoverAction.cpp:408–444`、`BattleState.cpp:299–343`。

CanEngageは発射だけでなく移動停止・照準方向・本拠地射撃への分岐・Actionを決める。さらにFireはConsumeShotの拒否より前にUpdateAimを呼ぶ。UpdateAim自身がAimStableSince/WeaponAimDirection/NextShotTimeを書き換える。このため「弾が出ないフレームのFireごと消す」「NextShotTime判定を冒頭へ移す」も無条件に安全ではない。

**同値側の設計：** `FEngagementContext` に当該呼出内のStore/Sense/U/S/B/P、Contact、位置、SightContextを持たせ、Follow→判定ヘルパー間の重複取得だけを省く（N06）。位置・姿勢を変更した後には読み直す。LOS自体を減らすなら、同一呼出の完全に同じEye/End/Query・衝突シーン版で、間に移動・回転・死亡・Collision変更がない重複だけを共有する。現在の主経路にそのような大量の重複があるとは確認していない。`CoverLineClear`は遮蔽解除用の「誰かの体なら通過扱い」、CanSeeはFOV/距離/対象Hitも見るため共用不可（CoverAction:51、Runtime:369）。

**結果が変わる側の設計：** 個人ごとに `{TargetID, LastActionLOS, NextActionSenseAt, OrderRevision}` を置き、行動用LOSを固定ゲーム時刻間隔で更新。射撃可能時だけ、その時点の新しいSightContextで発射用LOSを同期確認する。標的・命令・姿勢・遮蔽phase変更で無効化。既存FollowSightParallelの移動前先読みを同値キャッシュとして使わない。元のAim更新、弾切れ時StartReload、Peekの解除判定を独立して残す。

**変化する場所：** 行動LOSの古い値により、停止/再移動、敵への向き、Engaging表示、本拠地への切替が最大更新間隔ぶん遅れる。CanEngageを新しい射撃LOSで上書きすればそのフレームからさらに行動が変わる。位相分散も最初の観測時刻を変える。遮蔽phase4のCanSeeだけ削減する案でも、LOS成功時のみ実行されていたFire内の二回目のUpdateAimと診断カウンタを含めた証明が要るため、本点検では同値と認定しない。

**概算（実測ではない）：** 射線確認対象100人、60FPSで6000候補評価/秒。行動5Hz＋仮に発射試行1Hzなら約600評価/秒という上限モデル。ただしFOV等で実トレース前に落ちる数、Peek解除の別トレース、命令による臨時更新は別。6000本が実際に600本になると約束しない。

**見込み効果：大、危険度：高。** 頻度変更は新規試験切替既定0。M1でBattleFollowとSightHitStatsのCanSee由来を調べ、M3で移動停止・装填・射撃列・遮蔽phase・勝敗を評価。取得だけの整理はM2。同値版の見込みは小〜中に留まる。

### A03：P08の深化――格子抽出は同値にできるが、同点を含むtop-kは別扱い

**根拠：** `BattleCover.cpp:25–55,159–165`、`BattleCoverAction.cpp:95`、`BattleStableMovement.cpp:137`。

BuildSlots内の100cm格子は重複slot生成を防ぐローカル変数であり、NearbySlots用に残っていない。**既存の格子対策を再提案するのではなく、問い合わせ用索引を別途保持する設計**である。

**同値側：** `SlotCells : TMap<FIntPoint,TArray<int32>>` をBuildSlotsで作り、各slotの番号を保持する。問い合わせ矩形と交差する全セルを列挙し、現行の2D距離式と半径境界でフィルタする。候補番号を昇順に戻してから、現行の距離比較・同じSortを呼び、最後にLimitを適用する。これによりSortへの入力列を現行全走査と一致させる。距離値の事前計算も同じ精度・式を使い、NaN等は従来経路へフォールバック。Limit<=0で早く返す場合も副作用・診断がないことを確認する。

**同点の落とし穴：** 現行は距離だけのSortで、SlotIDのtie-breakはない。安定ソートや `(距離, SlotID)` のheapを導入すると、同距離候補の順序が変わり得る。境界同点を全採用してから別の部分配列をSortしても、現行の全候補配列へのSortと同じ同点順とは限らない。

**top-kを同値にする条件付き案：** 距離がすべて有限で重複距離がない問い合わせだけ、max-heap k件＋選んだk件の距離昇順へ進む。採用候補内または境界だけでなく、保守的に候補全体の同距離有無を検査し、同点があれば現行入力順の全Sortへ戻す。数値比較を厳密一致とし、epsilonでまとめない。重複検出の費用が削減を相殺する可能性がある。まず格子抽出版を先行させる。

**結果が変わる案：** 常時 `(距離,SlotID)` でtop-kするなら、初めに通る遮蔽、覗き候補、予約の採用、到着点まで変わる。順序を規格化する価値はあるが新規切替既定0。

**概算：** C=5000、近傍k=200、要求r=100/秒という仮定では現行は50万距離フィルタ/秒＋約15万比較/秒規模。格子は近傍セル内m件に対するr×m判定と番号順復元に置換する。k=200→limit12のheapは約r×k×log2(12)の比較規模だが同点fallback率次第。C/k/rは仮の説明値で、400人から一意に決まらない。

**見込み効果：大、危険度：中（同点規則変更は高）。** M1のBattleThink/BattleFollow内にNearbySlots区間と抽出前後件数を追加。M2では候補配列を切り詰め後まで順序付き比較し、半径境界・同距離多発・複数階・Limit 0/12/48/256を含める。top-k規則変更はM3。

### A04：P09の深化――反復失敗のメモ化と論理予算の維持

**根拠：** `BattleLeaderRuntime.cpp:755–797`、`BattleStableMovement.cpp:67–73,112–157,193–219,253–258`。

**追加の上限：** 遮蔽候補の末尾経路は24回で打切るが、一般候補の直線が塞がると同じTailQueriesを40まで使う。これはCoverHuddle=0でも発生する。50分隊同時なら末尾経路だけ最大2000同期探索/更新波、一般候補の投影はQueries上限80×50=4000/波。さらに主経路と、東側の対称主経路、通路のSweepがある。全候補625個をNav投影するわけではない。

失敗した遮蔽候補はReservedへ入らず、後続memberが同じ始点・終点で再び試す。単なる成功経路共有より、失敗結果の再利用に余地がある。

**同値を狙う設計：** BuildMovementPlan一回の呼出内で、候補slot/一般候補番号→投影結果・末尾経路結果（成功/partial/点列/長さ）を保存。問い合わせキーは同じ始終点の正確な値、NavData/agent/context/filter、投影extent、Nav版を含める。近似量子化で同一視しない。Reserved照会は毎回現在値で行い、member順・候補順・採用順は変更しない。

重要なのは、**キャッシュヒットも従来同様にQueries/TailQueriesを消費すること**。物理探索数を減らしたからといって後続候補を追加すると、24/40/80件予算が別の意味になり結果が変わる。物理問合せ数と論理試行数を別カウンタにする。診断の論理失敗/採用イベントも元の順で残す。

静的Navが同一の間の純粋な問合せとして成立することが同値保証の前提。実行中のNav更新、問合せContextの違い、エンジンのpath更新通知などまで無条件に同じとは断定しない。初期構築中は再利用しない。呼出間共有へ広げるならNav版の無効化・World切替・復元まで必要で、最初の実装範囲にしない。通路のClearMovementSegmentも同じ静的シーン版・正確な端点だけ再利用できる。

**集中を分散する設計：** 既存ThinkCapを基点に、分隊IDとOrderRevision、次member/candidate、Reservedの扱いを持つ継続計画を作り、フレーム当たり固定件数で進める。完成時に命令revisionを再確認。予算を実時間msにするとマシン負荷が試合へ入るので採用しない。ただし、開始/確定時刻、競合した予約を取る順、後続Thinkの入力は変わる。既存計画のまま移動を続けるか停止するかも仕様として決め、新規切替既定0（ThinkCapは既存既定0）で評価する。

**概算：** 仮に5秒ごとに全隊のArrivalが作り直されるなら、上記上限は平均400末尾探索/秒、800投影/秒。ただし1フレーム集中時2000/4000というピークは平均で消えない。通常は `NewOrder || ArrivalGoals.IsEmpty()` の条件付きで、0.5秒ごとの全Thinkをこの負荷として掛けない。

**見込み効果：大、危険度：中（呼出内同値）／高（分散）。** M1でBattleThinkの開始・命令変更波とp95/最大。現行PathMSは主経路探索の計時で、BuildMovementPlanの末尾全体は含まないため追加NavTail/ArrivalProjection区間が必要。M2＋移動・到着専用試験、分散版はM3を使う。

## 2. 新規・追加具体化の候補（確信度高、優先順）

### N01：作戦HUDの霧は表示を隠しても同期トレースを続ける

- **根拠：** `BattleHUD.cpp:833–842,858–866,876,891`、`BattleSectorView.cpp:46–89`、`BattleSectorView.h:28–34`。
- **状況→処理量：** Tactical＋混成＋SightFieldで、Fog/区画の表示フラグより先にFog.Update(...,400)が走る。最大400本/フレーム＝2.4万本/秒（60FPS）。地形高さを求め、各rayで625cm刻みのセルMarkも行う。仮に射程100mかつ全線開通なら16回Mark/ray、最大38.4万Mark/秒。casterは自軍が占めるセルごと1人に圧縮済みで、全400人×全ray/フレームではない。
- **同値案：** ray数ごとのsin/cos方向表、同じcaster周回中の静的地形終点、出力scratchを再利用する。最初の生成も現行と同じ数式・精度。ray実行順と400本予算、Mark時刻は保持。霧も区画も隠れている時は画面投影・Dark/Tris生成をスキップできるが、ミニマップと記憶に使うFog.Updateは継続する。
- **変更案：** 予算削減、非表示中のFog更新停止、非同期結果の翌フレーム適用は表示の発見/失効時刻とCycleSecondsを変える。AIに入力しなくても操作情報が変わるため、表示用試験切替既定0にする。旧値を単に引き延ばさない。
- **見込み効果：大（ray削減）／中（同値整理）、危険度：中。** M1のBattleHUD_DrawFieldOverlay、CSV BattleHUD。FogUpdate/投影/生成に区間を追加。M4でFogのみ・区画のみ・両方隠す・ミニマップ・カメラ移動を比較。描画なし試合ではこの費用を測れない。

### N02：HUDが同じ接触を重複集計し、毎フレーム多数の一時コンテナを作る

- **根拠：** `BattleHUD.cpp:75–78,493–504,862,878,893,900`、`BattleSectorView.cpp:92–103,119–147`。
- **状況→処理量：** 上帯のKnown TSet、ミニマップのKnown TMap、H/IのGather内Known TMapがそれぞれ自軍接触を読む。自軍200人×平均30接触×60FPSなら各36万接触訪問/秒、3経路で108万/秒（全てがAddされるわけではない）。Screen/On/Dark/Count/Trisがフレームローカル。GatherのOut.Resetは280区画分の内部Squads配列を破棄するため、外側の容量だけ保持しても内側の確保は残る。
- **同値案：** 一回のDrawHUD内にHUD専用snapshotを構築し、同一フィルタのミニマップとGatherへ渡す。可視優先→新しいLastSeenTime→完全同点は最初の接触、という現規則と入力列挙順を維持する。上帯は長距離偵察を除外していないので、上帯用の可視ID集合は別の射影として同じ走査中に作る。AI用AreaIntelは1秒キャッシュ・フィルタ・副作用が異なるので代用不可。
- Screen等はHUD所有の再利用領域にし、必要長だけ初期化。区画配列は要素を保持して数値をリセットし、各Squads.Resetで内側容量も保持。フレーム間のContactポインタ保持はしない。カメラ不変でも霧の実時間失効があるので結果丸ごとの長期キャッシュは不可。
- **見込み効果：中、危険度：中。** M1のDrawFieldOverlay/DrawMinimapとCSV BattleHUD、追加KnownEnemies区間。M2の自動戦闘＋M4の描画順・完全同点接触・偵察報告・復元を確認。机上の訪問数削減と実malloc数は分ける。

### N03：近接回避格子のResetでセル内TArrayを毎フレーム作り直す

- **根拠：** `BattleLeaderRuntime.cpp:1365–1366`、`BattleStableMovement.cpp:387–428`。認識格子（01 P03）とは別の近接移動用格子。
- **状況→処理量：** TMapをResetし、各生存兵をセル配列へAddする。外側の容量を保持しても、値のTArrayの要素寿命は終了する。400人60FPSで2.4万Add/秒。1セル1〜数人なら最大400セル配列/フレーム、2.4万セル配列生成/秒規模。実ヒープ呼出回数は増量規則とセル分布に依存。
- **同値案：** `Cell→bucket番号` と再利用bucket pool、世代/使用リストを持つ。毎フレーム前回使用bucketだけReset、現行Bodies順で現在セルへAddし直す。検索は同じ3×3セル順、配列中のID順を保持し、32件上限前にソートしない。移動後の位置を見て格子を途中更新する変更も混ぜない。未使用セル索引は世代で無効化して肥大化を防ぐ。
- **見込み効果：中、危険度：中。** M1のBattleRuntimeStepでBattleFollow直前（現行は専用scopeなし）にAvoidanceGridBuildを追加。M2で混雑・補充・死亡・保存往復の最初の位置差を比較し、M移動試験を併用。TInlineAllocatorへの変更だけでも小セルを救えるが構造体サイズ増加との比較が必要。

### N04：Mass移動のSweepMultiが呼出ごとにHit配列を作る

- **根拠：** `BattleStableMovement.cpp:14–28,51–52,477–500`、`BattleContactMovement.cpp:66,103`。
- **状況→処理量：** 移動部下の通常Sweep、衝突時のSlideでローカルTArray<FHitResult>を生成する。400人=50隊長＋350部下が全員移動する例なら主Sweep最大2.1万/秒、全員Slideも行えば4.2万/秒。全線hitなしなら配列が確保されない可能性があるため、「毎回malloc」と断定しない。高低差のClearMovementSegmentにも別のMulti配列がある。
- **同値案：** 呼出スタックに対応した再利用scratchをSweepSoldierへ引数で渡す。元のHit順と「離脱方向の初期貫通/地面を飛ばし、最初のblockingを採る」規則を保持する。SingleSweepへの置換は地面や初期貫通の次のhitを失うので不可。再入・delegateから同じscratchが上書きされない寿命設計を置く。UE APIが通常TArray型を要求する場合、単にInlineAllocator型へ変えて渡せるとは限らない。
- **見込み効果：中、危険度：低〜中。** M1のBattleSweepSoldier/BattleMoveCapsule、将来の割当計測でヒットが多い区間を抽出。M2とTest-MovementStability/Test-ContactMovement、斜面・初期重なり・壁滑りを確認。既定で既に無効な二重engine sweepを「未対策」として再提案しない。

### N05：VATは遠距離兵の数だけでなく全instance枠を毎フレーム初期化する

- **根拠：** `BattleVATDisplay.cpp:323–345`。
- **状況→処理量：** `TArray<TArray<FTransform>> Transforms`を毎Tick生成し、全GroupのGetInstanceCount分をゼロscaleで埋め、FarかつVisibleのslotを上書きする。部下350人×体/武器各1枠なら700transform初期化/フレーム＝4.2万/秒、さらに表示中Far分の行列計算・SetCustomData・BatchUpdate。実枠数は死亡・表示group構成等に依存する。
- **同値案：** Group構成変更時だけ外側を調整し、各配列の容量を保持、毎Tick同じゼロ埋めと上書きを行う。体/武器2回の `Runtime->GetBodies().FindChecked(E.ID)` と同一Capsule transform取得を一度へまとめる。まずCPUの確保・再検索だけ削減し、BatchUpdate/previous frame更新の順は保つ。
- **変更注意：** 全ゼロ初期化を消すなら近景移行・死亡武器・非表示・slot再使用のたびに必ずゼロ化する必要がある。FarLightやアニメ間引きの既存試験は本提案とは別。描画更新を間引く案は見た目が変わるので既定0で比較。
- **見込み効果：中、危険度：低〜中。** M1のBattleVATDisplay、M4とMEASURE §2 Test-VATDisplay。通常/監査ONを分け、Far=0でも枠を持つ状態、死亡・近遠往復・復元を含める。

### N06：Follow周辺のSubsystem/状態/Bodyの再取得を短い範囲で統合できる

- **根拠：** `BattleLeaderRuntime.cpp:266–283,323–328,369–378,866–884,985,1125,1358`、`BattleStableMovement.cpp:301–304`、`BattleCoverAction.cpp:377–389`。
- **状況→処理量：** FollowでStore/U/S/B/Pを得た後、StepCoverActionやStableMoveが取得し直す。Position/ForwardもBodies.Findを行う。例えば対象全員ならFollowのStoreとStepCoverActionのStoreだけで4.8万Subsystem照会/秒。全員StableMoveへ入ればさらに2.4万/秒。1358の同一Squad Find×2は短絡条件を通った分、最大3000重複Find/秒。985のContains→StepCoverApproach内Find、FireのActor(ID)反復にも同種の余地がある。
- **同値案：** World寿命のサービス参照を安全に初期化、またはStep/Followごとのcontext引数に束ねる。Findの返値を局所で1回保持し、既に得たBodyから位置を読む純粋ヘルパーを追加。値の読取時刻は変えず、State/Bodies/PlansのAdd/Remove、Fire→OnDeath、命令変更や復元を跨ぐ参照は再取得する。フレーム冒頭の全員位置snapshotへ置換しない。
- **見込み効果：小、危険度：低（サービス参照）〜中（要素参照）。** M1のBattleFollow/Thinkで件数とinclusive時間、M2で死亡・隊長交代。GetSubsystemは通常のlookupであり、呼出回数が多いだけで高コストの生成処理と扱わない。トレース削減より後の優先度。

### N07：HUDの毎フレーム文字列・幾何生成と、必要な値コピーを区別する

- **根拠：** `BattleHUD.cpp:48–59,93–120,142–156,546,789–799`、`BattleController.cpp:1013`、`BattlePlayerPath.cpp:271–295`、`BattleCoverAction.cpp:47,485`。
- **状況→処理量：** HUDは既定描画時にFString::Printfや連結を行う。拠点9個ならラベル更新だけで最大540回/秒、拠点輪郭32辺×9×60で最大1.728万辺評価/秒（端点投影は短絡前最大3.456万/秒）。1080pのscopeは約332行×2矩形×60＝約4万DrawRect呼出/秒。これはGPU draw call数と同義ではない。通知5行なら最大300行/秒の複数文字列生成。遮蔽のDecisionReasonもB=100で6000代入/秒だが同長のFString代入が毎回ヒープ確保するとは限らない。
- **同値案：** 表示に使う整数値・丸め後秒・通知ID/操作兵ID・言語が変わった時だけ文字列を作り、色/フェードは毎回計算する。輪郭の静的world頂点を同じ式で生成して保持し、投影はカメラ更新に追従。scopeの同形の幾何も画面サイズ変更時に再生成する。描画の順・透明度・境界を保持する範囲なら同値。描画形状を別のマスク方式へ変える案は画面差があり既定0で評価する。
- `auto Point=Objectives[Index]`は直後に観測Ownerへ置換するためのコピー。参照化して正規状態を上書きしてはいけない。`const auto Rows=SquadListRows()`も戻り値TArrayの受取り（Controller.h:77）であり、それだけで余計な二重コピーとは言えない。負荷は関数内の一覧生成/ソートにある。HUDとクリックで同じ行順を保つ版付き一覧を使う案は有用だが、死亡・カテゴリ・距離順・フィルタ変更を全て無効化条件に含める。
- PlayerPathのDraftコピーは末尾にpointer位置を足すため。表示用span＋末尾1点、Now/Laterのscratch再利用で削減できる。選択24隊なら最大1440回/秒の経路表示組立て。道路のLanePathは小グラフ探索であり同期Nav探索ではない。
- **見込み効果：中（HUD全体）／小（文字列単独）、危険度：低〜中。** M1のDrawHUD/DrawSquadList/DrawNoticeLog/DrawOrderMarks。DrawOrderWaysと通知文字列には追加scopeを推奨。M4で文字列・丸め境界・画面サイズ・選択行の一致を確認。UpdateAimや戦術Reasonのenum化を性能変更へ混ぜない。

### N08：Planner OFFでも設定の文字列解析が認識更新に入る

- **根拠：** `BattlePerception.cpp:65`、`BattleComposition.cpp:8–13`、`BattleCompositionRuntime.cpp:128–138`、`BattleCoverAction.cpp:45–49,423,450`。
- **状況→処理量：** GroupStrength=0の既定では、毎Perception UpdateにSettingsが2回FParseする。60FPSで120回のコマンドライン検索/秒。Planner自体の候補選択は5秒周期なので毎フレーム10候補計算と誤認しない。両陣営ONなら通常INPUT＋最大10候補＋DECISIONで最大約4.8行/秒、購入成功時EXECUTEは追加。01 P07等と重複する敵集団集計は新規に数えない。
- **同値案：** 起動設定はWorld/初期化時に解決して保持。旧保存でCompositionTeamsを無効化する「実効状態」とは別にする。現行のbRecordSquadTypeは起動bEnabledを見るため、最適化だけで実効maskへ変えない。02 D08の追加具体化として、CoverFire=0かつ診断OFFではCoverPeekShotsの記録とEndPeekDiagのPrintfを省ける。診断ONの数値を変えず、削除時の掃除も揃える。
- **UE_LOGの判定：** 確認した通常Step/Follow/移動/HUD/VATの範囲では、既定で無条件に毎フレーム1行出すログは見つからない。ScoutReport:52は車両10秒、:54–69は60秒、CoverRelease/Actionはイベント時、StableMovement:337–357の詳細は実験時の条件付き。毎フレームの文字列処理と毎フレームIOを混同しない。戦闘イベント集中時のログ増量は別に測る。
- **見込み効果：小、危険度：低〜中。** M1のBattlePerceptionUpdate/MatchUpdate/Follow。解析・ログ整形の追加scopeとログbytes/ゲーム秒。M2でPlanner=0、1/Teams=0、旧保存後の識別条件と診断列を確認。ログを消して速くなった比較をAI選択器の高速化と呼ばない。

## 3. 条件付き候補（確信度中／大きさは推測）

### N09：小さいParallelForの費用は粒度16/8だけでは判定できない

**根拠：** `BattlePerception.cpp:251–301`、`BattleLeaderRuntime.cpp:823–848`。

認識は事前filter済みTraces.Numに対し最小batch16で並列化し、最後に元のobserver/candidate順で適用している。Follow先読みはbatch8、既定OFF。同じParallelForでも後者は移動前の世界を読む仕様変更で、同値比較の対照にしてはいけない。

少数trace時に起動・待機費が支配する可能性はあるが、UE側のタスク分割・直列実行最適化もあり「16は小さすぎる」は未立証。N=400でもdue観測者の位相とFOVによってTracesは0〜多数と変動する。60FPSなら入口は最大60回/秒で、400タスク×60と数えない。

**設計案：** count閾値未満は同じTraces配列をforで埋め、以上はParallelFor。Prepare/Applyは変えず、同期joinしてから適用。閾値は件数の固定設定とし、実時間の速さを見て未来の更新を飛ばす方式にしない。同じ衝突シーンを読む前提では同値を狙えるが、エンジン側の更新と競合していないかも検証する。

**見込み効果：小〜中、危険度：中。** M1のBattleSightParallelPrepare/Trace/ApplyとCSV BattleSightScanでtrace件数別の時間、worker実行とGT待機を比較。M2で接触・識別・配信列の一致。閾値とbatchを一度に変更しない。証拠がない段階で並列化を撤去しない。

### N10：配列線形探索の置換は8人隊では優先度が低い

**根拠：** `BattleLeaderRuntime.cpp:564,898,996`、`BattleStableMovement.cpp:267`、`BattleState.cpp:116,494,532`。

Members.IndexOfByKeyは8人隊なら平均約4.5比較。仮に400人が毎フレーム1回実行しても約10.8万の整数比較/秒。996は非Stable経路、267は計画生成、898はBounding条件付きなので、全てを既定で常時加算してはいけない。Members.Containsは登録・交代等のイベントにもあり、全件が毎フレームではない。CoverAssigned.Contains等はTSetであり配列線形探索ではない。

**設計案：** まず索引を持つforでMembersを走査する箇所はそのindexを使う。継続的に必要ならUnitID→member ordinalを部隊構造変更時に更新する。死者を除いた連番へ詰めると隊形・偶奇担当が変わるので禁止。ReplaceDeadFollower/復元でも元のMembers順を保持する。小配列をTMap化するとハッシュ・メモリ費が増えて逆効果の可能性がある。

**見込み効果：小、危険度：低〜中。** M1のBattleThink/Followで当該呼出件数をまず確認、M2の部下補充・隊長交代でordinal一致。仮の上限だけで最適化の先頭に置かない。

### N11：保存復元は単発のNav・オブジェクト・一時メモリのピークとして扱う

**根拠：** `BattleMixedBodies.cpp:79–105,161–186`、`BattleSave.cpp:349–355`、05の資源上限点検。

主経路ValidatedPathsは再利用済み。一方、復元後のBuildMovementPlanで到着点・通路を構築し直すため、50隊再開ならA04相当の集中があり得る。車両ConfigureVehicle前の事前検証は歩兵Nav、後の計画とはquery条件が異なるので、同じ終点というだけでメモ化できない。Save読込/検証/codec・stagingの費用は毎フレームではなくロード1回ごとに評価する。

**設計案：** A04の呼出内再利用から始め、検証済み点列のPへのコピーは所有権を移せる箇所だけMoveTempを検討。失敗時に元状態を残すstagingを削って節約しない。05が挙げた上限Reports100万・砲弾10万を400人生存の通常保存の実サイズと混同しない。`BattleState.cpp:266`のMissionGoals値コピーもSetSquadOrderによる自己alias更新を避ける意味があり、安易なconst参照化はしない。

**見込み効果：中（ロード停止）、危険度：中〜高。** M2＋MEASURE §2 Test-MixedSave、M1の往復80秒付近を別区間として記録。ロード専用scopeとピークメモリ計測を追加する。現行の集計はロードでリセットされる項目がある（04 R12）ため、通常フレーム平均だけでは判断不可。

## 4. 同値性と実装優先順

「保証可能」は設計上の条件を満たせば論理結果を保てるという分類であり、今回は実装・試験による保証を得ていない。固定tickで同じ入力・Nav/衝突準備状態が前提。実時間依存のHUDの寿命や非固定FPSの実プレイまで、CPU処理時間を短くしても全く同じ世界になるとは主張しない。

| 優先 | 結果を変えない最適化候補 | 見込み効果 | 危険度 | 同値の必須条件／確認 |
|---|---|---|---|---|
| 1 | A01 予約逆索引・競合隣接表 | 大 | 中 | 期限・生死を呼出時に判定、取得順保持。M1/M2 |
| 2 | A03 格子抽出→元のslot順→元の全Sort | 大 | 中 | 同点を含む入力列一致、slot生成は不変。M1/M2 |
| 3 | N01/N02 HUD共通集計・scratch・静的幾何 | 中 | 中 | 観測フィルタ・描画順・霧更新時刻保持。M1/M4 |
| 4 | N03/N04 移動scratchとセルbucket再利用 | 中 | 中 | 32候補上限前の順序、hit選択維持。M1/M2/移動試験 |
| 5 | N05 VAT配列再利用・同じBody取得の統合 | 中 | 低〜中 | 全ゼロ枠とprevious-frame更新を維持。M1/M4 |
| 6 | A04 同一呼出内のNav失敗/投影再利用 | 大 | 中 | Nav版固定、論理24/40/80予算を維持。M1/M2/移動試験 |
| 7 | A03 距離完全一意時のみtop-k | 中〜大 | 中 | 同点時は現行全Sort、fallback率測定。M1/M2 |
| 8 | N06/N08 短命context・起動設定解決・診断OFF時処理 | 小 | 低〜中 | 要素寿命・旧保存の実効状態を混同しない。M1/M2 |
| 9 | N07 文言/一覧/経路の生成再利用 | 小〜中 | 中 | 全表示入力の無効化とクリックの行順一致。M1/M4 |
| 10 | N09 少数traceの直列fallback | 小〜中 | 中 | 同じ問い合わせと適用順、同期完了。M1/M2 |
| 11 | N10 ordinal、N11ロード内コピー整理 | 小／中 | 中 | Members順、所有権・失敗原子性を維持。M2 |

| 優先 | 結果が変わる最適化（既定0の試験が必要） | 見込み効果 | 危険度 | 変化するもの／確認 |
|---|---|---|---|---|
| 1 | A04 ThinkCap・到着計画のフレーム分割 | 大（ピーク） | 高 | 命令反映時刻、予約取得順、移動開始。M1/M3 |
| 2 | A02 行動LOS低頻度化・射撃LOS分離 | 大 | 高 | 停止/照準/装填/本拠地射撃分岐。M1/M3 |
| 3 | N01 霧の予算削減・非同期化・非表示中停止 | 大（HUD） | 中 | 情報が出る/消える時刻。AI不変でも操作表示は変化。M1/M4 |
| 4 | A03 常時top-k＋新tie-break | 大 | 高 | 同距離候補の遮蔽採用順・到着配置。M1/M3 |
| 5 | N05/N07 表示の間引き・描画方式変更 | 中 | 中 | pose/霧/輪郭/文字の表示タイミングまたは画素。M1/M4 |

A02の取得統合はN06として同値側に含めるが、待機フレームのLOS本数を大きく減らす同値案は現時点で確立していない。案を無理に「同値」と分類するより、効果と挙動差を独立に測る。

## 5. 将来の検証・計測手順（今回すべて未実施）

### M1：CPU費用とピーク

MEASURE §0/§1「性能」/「性能の計測の決まり」を使う。凍結ビルド、単独描画、同じマップ/seed/FPS/保存往復/カメラ/表示条件で比較し、トレースあり/なしを分ける。入口は `Setup/Test-WideSight.ps1 -Map E -Area -ObserveTactics -Experiment -Rendered -Fps 30 -OutputName <未使用名>`、内訳用は `-Trace 'cpu,frame'`。現HEADで新しい基準を作り、同条件の60FPSも別対で取る。遮蔽案はCoverActions=1を両側に明示した対を追加し、H/I・混成兵種・RTS/TPSは別条件にする。スクリプト既定の暗黙ONは02と現ソースで照合する。

開始前にGet-MeasurementMachineを取得しmanifest.machineへJSON深さ5以上で保存。全GPUの名前/VRAM bytesと取得元/ドライバ、全CPUの名前/物理・論理コア、RAM bytes、OS名/版/ビルド/architectureを記録し、描画GPUはUEログと照合。取得不能VRAMはnullと理由、32bit AdapterRAMを実容量に使わない。既存結果へ追記しない。実行共有ロックやPC競合の確認もMEASUREに従う。今回の読取点検には実行ロックは取得していない。

見る値は `metrics.json` の `frame_wall_ms`、`game_thread_cpu_ms`、`game_thread_wait_ms`、認識/判断時間とp95/最大。GT/RT/RHIは並行しているので足し算しない（BattleMetrics.cpp:25）。PERFORMANCEの約0.5msのばらつきより小さい差は単発で認定しない。再利用結果 `timingComparable=false` は除外、並列の描画なし勝敗試験をCPU基準にしない。

区間は開始直後、平常の移動、交戦密集、5秒命令波、雇用・死亡、保存復元、試合終了を分ける。各区間で生存数・予約数・trace/Nav実回数を添え、異なる試合展開の全体平均差をアルゴリズム短縮と呼ばない。

| 対象 | 現在あるInsightsイベント／CSV stat | 不足している内訳（追加提案、現在は存在しない） |
|---|---|---|
| 認識 | BattlePerceptionUpdate、BattleSightParallelPrepare/Trace/Apply／BattlePerception、BattleSightScan、BattleReportDelivery、BattleReportQueue | trace件数別、設定解析、格子の割当 |
| 判断・Nav | BattleThink、BattleMatchUpdate、BattleSquadTacticsStep | NearbySlots、CoverReserve、ArrivalProjection/NavTail、Nav物理呼出/論理試行数 |
| 実行 | BattleFollow、BattleSweepSoldier、BattleMoveCapsule | 遮蔽phase別LOS、AvoidanceGridBuild、scratch割当 |
| 表示 | BattleRefreshDisplay、BattleVATDisplay、BattleHUD_DrawHUD/DrawMinimap/DrawSquadList/DrawNoticeLog/DrawOrderMarks/DrawFieldOverlay／BattleHUD、BattleVisibility、BattleUnitTick、BattlePlayerTick | FogUpdate、KnownEnemies、DrawOrderWays、文字列組立て、VAT配列初期化 |
| ロード | 通常の試合トレースで時刻を照合 | 読込/検証/staging/Arrival再構築、メモリピーク |

CSVはソースに存在するstat名を挙げたもので、Test-WideSightが専用列を自動出力するという意味ではない。同スクリプトのTrace配線は116–117行。必要なら将来の計測ハーネスでCSV捕捉を明示する。`SCENE_QUERY_STAT(BattleFogView)`等はscene queryのタグであり、その名の時間列が既存metricsにあると解釈しない。malloc数/byteの断定には別途Memory Insights等の割当捕捉が必要で、通常cpu,frameだけの計測とは分ける。

### M2：同値最適化

MEASURE §1「結果を変えない最適化の確認」のCompare-MatchResultsで前後同条件を比較し、完走とIDENTICALを確認。C++修正時はBuild-Editor→Test-Battleの `BATTLE_SMOKE_RESULT failures=0`、変更範囲ごとの§2専用試験も必要。移動はTest-MovementStability/Test-ContactMovement、保存はTest-MixedSave、補充はTest-Area等。

ただしIDENTICALは統計一致なので、同点候補の配列・予約取得列・ArrivalGoals/点列・射撃の時刻とID・正規状態/継続状態の最初の差を別途確認する（04 R02）。診断比較では物理探索を省いた件数差と論理判断の差を区別する。新しい診断の記録は性能計測と分ける。保存あり前後と保存なし前後は各々同条件で比べ、保存往復そのものが完全同値という要求を混ぜない。

### M3：結果が変わる最適化

MEASURE §1の凍結ビルド→Run-StrategyScreen/Run-EconomyMatrix→Analyze-EconomyMatrixを用い、30FPSは選別、60FPSと保存往復で確認する。グローバルな件数予算・trace方式を片側化できるとは限らず、AreaTrialTeamを付けるだけで片側試験が成立すると扱わない。通常の両側適用対、切替に明示された陣営範囲、比較対象の意味を記録する。勝敗だけでなく行動差・予約待ち・射撃頻度を読む。既定化は別判断。

### M4：HUD・描画側CPU

MEASURE §2の操作/表示試験（Test-TPSControl/Test-TPSVisibility/Test-TPSAvoidance/Test-VATDisplay）と実画面確認を使う。RTS、TPS、FPS、scope、解像度、霧/区画、選択0/1/24隊、停止/倍速/再開、通知多発を分ける。性能用のカメラ経路を固定し、画面比較は同じゲーム状態・同じ表示時計で行う。描画なしではHUDが検証できず、画像を生成しただけでも表示確認完了にはならない。

## 6. 後続の点検メモ

### 切替の棚卸し・試験の穴・資料の食い違い

- `BattleCover.cpp:188`と`BattleCoverAction.cpp:386–387`：ロード復元時の二度Reserve、期限ちょうど、slot上書き後の旧Unit Releaseを回帰表へ。A01の索引が保存項目ではなく再構築物であることも明示する。
- `BattleStableMovement.cpp:151,195,205`：24/40/80の論理予算と実Nav呼出を別の検査にする。ThinkCap/FollowSightParallel/EngineMoveSweep/SightPairReuseを全て「結果不変の性能切替」と一括分類しない。
- `BattleCoverAction.cpp:455–457`、`BattleStableMovement.cpp:337–357`：Experimentが追加LOS・診断走査を行う。計測コマンドのExperimentありと通常プレイを同じ仕事量と見なさない。
- `BattlePerception.cpp:65`、`BattleComposition.cpp:8`：起動設定cacheと旧保存による実効Planner無効化の契約をテストする。01/02の識別未修正記述は現状態ではない。
- `Setup/Compare-MatchResults.py:10–18`：率の除外は修正済みだが、全状態・実効設定・必須完走schemaの保証は別。変更後の物理query数まで一致を要求する項目と、判断同値の項目を整理する。
- PERFORMANCEの「現在」は11-18の条件、MEASUREのpeek保存問題未調査記述とSTATUS直近の修正済み記述が一致しない。新規基準は現HEAD・起動条件・描画条件を添え、今回の机上400/50/60を測定値として転載しない。
- 05で懸念された復元処理は現HEADでstaging/ValidatedPaths再利用がある。修正済み範囲と残るBuildMovementPlan/NavData条件を分け、古い行番号だけで未修正認定しない。

### 操作・HUD

- `BattleHUD.cpp:840–842,862`：霧と区画を隠しても霧更新と全頂点投影が続く。非表示中の探索記憶、再表示、ミニマップとの整合を仕様として確認する。
- `BattleSectorView.cpp:48,102`：CycleSecondsは実時間、同時刻接触は最初のもの。CPU速度の変化、停止/倍速時の霧寿命、完全同点の採用を確認する。単純なゲーム時刻への置換も表示仕様変更。
- `BattleHUD.cpp:75,493`と`BattleSectorView.cpp:100`：上帯・地図の偵察除外条件の違い。集計共通化で未報告の敵を表示しないこと。
- `BattleController.cpp:829–848,1013`、`BattleHUD.cpp:546`：部隊一覧cache導入時は描画行とクリック行の一致、ソート中の死亡・補充・委任切替を確認する。
- `BattlePlayerPath.cpp:271–295`：24隊上限、線を引いている途中の末尾、カメラの外へ出る線の分割。Navと無関係な描画組立てを「経路探索が遅い」と誤分類しない。
- `BattleVATDisplay.cpp:331–345`：遠距離→近距離、非表示、死亡武器、slot再使用、復元のゼロtransformとprevious frameを確認する。アニメ間引きの既存試験と配列再利用を別差分にする。

本報告の最適化はすべて設計案であり、実装・速度改善・同値試験の合格を報告するものではない。
