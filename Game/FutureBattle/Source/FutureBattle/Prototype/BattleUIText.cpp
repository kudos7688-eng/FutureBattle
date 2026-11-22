#include "BattleUIText.h"
#include "Internationalization/Regex.h"

namespace
{
const TMap<FString,FString>& Words()
{
 static const TMap<FString,FString> Map=[]()
 {
  TMap<FString,FString> M;
  // Results.
  M.Add(TEXT("VICTORY"),TEXT("勝利"));M.Add(TEXT("DEFEAT"),TEXT("敗北"));M.Add(TEXT("DRAW"),TEXT("引き分け"));
  M.Add(TEXT("ENEMY BASE DESTROYED"),TEXT("敵本拠地を破壊"));M.Add(TEXT("HOME BASE DESTROYED"),TEXT("自軍本拠地を破壊された"));
  M.Add(TEXT("TIME LIMIT"),TEXT("時間切れ"));M.Add(TEXT("CONTROL SCORE"),TEXT("支配得点"));M.Add(TEXT("FORCE ELIMINATED"),TEXT("自軍全滅"));
  M.Add(TEXT("ENEMY ELIMINATED"),TEXT("敵を全滅"));M.Add(TEXT("ELIMINATION"),TEXT("全滅"));
  // Orders, unit actions, squad tactics.
  M.Add(TEXT("MOVE"),TEXT("移動"));M.Add(TEXT("ATTACK MOVE"),TEXT("攻撃移動"));M.Add(TEXT("DEFEND"),TEXT("防衛"));M.Add(TEXT("HOLD"),TEXT("停止"));
  M.Add(TEXT("DIRECT"),TEXT("直接操作"));M.Add(TEXT("IDLE"),TEXT("待機"));M.Add(TEXT("MOVING"),TEXT("移動中"));M.Add(TEXT("ENGAGING"),TEXT("交戦中"));
  M.Add(TEXT("RELOADING"),TEXT("装填中"));M.Add(TEXT("TAKING COVER"),TEXT("遮蔽へ"));M.Add(TEXT("DEAD"),TEXT("戦死"));M.Add(TEXT("MOVE FAILED"),TEXT("移動失敗"));
  M.Add(TEXT("REGROUPING"),TEXT("集合中"));M.Add(TEXT("RETURNING"),TEXT("帰還中"));M.Add(TEXT("FLANKING"),TEXT("迂回中"));
  M.Add(TEXT("FOLLOW MISSION"),TEXT("任務どおり"));M.Add(TEXT("ENGAGE"),TEXT("交戦"));M.Add(TEXT("SUPPORT"),TEXT("援護"));M.Add(TEXT("FLANK"),TEXT("迂回"));
  M.Add(TEXT("RETREAT"),TEXT("後退"));M.Add(TEXT("REGROUP"),TEXT("集合"));M.Add(TEXT("RECOVER / DEFEND"),TEXT("回復・防衛"));M.Add(TEXT("RECOVER"),TEXT("回復"));
  // Squad list states and reasons (the reason strings stay English in the game state).
  M.Add(TEXT("MANUAL COMMAND"),TEXT("手動指揮"));M.Add(TEXT("WAITING FOR REPORT"),TEXT("報告待ち"));M.Add(TEXT("LeaderDeath"),TEXT("隊長戦死"));
  M.Add(TEXT("NO KNOWN CONTACT"),TEXT("既知の敵なし"));M.Add(TEXT("KNOWN CONTACT"),TEXT("既知の敵あり"));M.Add(TEXT("LOCAL AI"),TEXT("部隊AI"));
  M.Add(TEXT("HOME SUPPLY"),TEXT("本拠地で補給"));M.Add(TEXT("RETURN WITH LEADER"),TEXT("隊長と帰還"));M.Add(TEXT("COMPLETE"),TEXT("完了"));M.Add(TEXT("RESUME"),TEXT("任務に復帰"));
  M.Add(TEXT("AMMO EXHAUSTED"),TEXT("弾切れ"));M.Add(TEXT("NO SAFER REACHABLE ROUTE"),TEXT("安全な経路なし"));
  // One-line notices from the controller.
  M.Add(TEXT("ATTACK MOVE: right-click destination"),TEXT("攻撃移動：右クリックで目的地"));M.Add(TEXT("DEFEND: right-click center"),TEXT("防衛：右クリックで中心"));
  M.Add(TEXT("COVER RELEASED"),TEXT("遮蔽を解除"));M.Add(TEXT("Game speed: standalone only"),TEXT("速度の変更は単独起動のときだけ"));
  M.Add(TEXT("Game speed: tactical view only (direct control runs at x1)"),TEXT("速度の変更は作戦画面だけ（直接操作中はx1）"));
  M.Add(TEXT("Navigation is not ready"),TEXT("経路の準備ができていません"));M.Add(TEXT("SQUAD HOLD POSITION"),TEXT("部隊はその場で停止"));
  M.Add(TEXT("Select a friendly observer first"),TEXT("先に味方の観測者を選んでください"));M.Add(TEXT("Select a friendly unit first"),TEXT("先に味方の兵を選んでください"));
  M.Add(TEXT("Start the battle with ENTER"),TEXT("Enterで戦闘を開始"));M.Add(TEXT("Training is fixed at recruitment"),TEXT("練度は購入時に決まります"));
  M.Add(TEXT("UNIT DOWN / select another ally"),TEXT("兵が倒れた ／ 別の味方を選んでください"));
  M.Add(TEXT("AI COMMAND / P TO RESUME IF PAUSED"),TEXT("AI指揮 ／ 停止中ならPで再開"));M.Add(TEXT("MANUAL COMMAND / CURRENT ORDERS RETAINED"),TEXT("手動指揮 ／ 今の命令を維持"));
  M.Add(TEXT("SELECT FRIENDLY SQUADS FIRST"),TEXT("先に味方の部隊を選んでください"));
  M.Add(TEXT("PRONE / X TO STAND"),TEXT("伏せ ／ Xで立つ"));M.Add(TEXT("CROUCHED / X FOR PRONE"),TEXT("しゃがみ ／ Xで伏せ"));M.Add(TEXT("STANDING / X TO CROUCH"),TEXT("立ち ／ Xでしゃがむ"));M.Add(TEXT("CHANGING STANCE"),TEXT("姿勢を変えている途中"));
  M.Add(TEXT("COVER / FIRE TO PEEK / C OR MOVE TO RELEASE"),TEXT("遮蔽中 ／ 射撃で身を出す ／ Cか移動で解除"));M.Add(TEXT("COVER / FIRE = BLIND FIRE / AIM + FIRE = PEEK / C OR MOVE TO RELEASE"),TEXT("遮蔽中 ／ 射撃で壁越しに撃つ ／ 構えて射撃で身を出す ／ Cか移動で解除"));M.Add(TEXT("BLIND FIRE"),TEXT("壁越し射撃"));M.Add(TEXT("NO NEARBY COVER WITH SAFE PEEK"),TEXT("近くに安全に撃てる遮蔽がない"));
  M.Add(TEXT("AI PAUSED / practice controls freely"),TEXT("AI停止中 ／ 自由に操作を練習できます"));M.Add(TEXT("AI RESUMED"),TEXT("AI再開"));
  // Match and save messages.
  M.Add(TEXT("Player route save requires BattleSaveRoutes=1"),TEXT("経路付き保存を読むには -BattleSaveRoutes=1 が必要です"));
  M.Add(TEXT("Invalid player route extension"),TEXT("保存されたプレイヤー経路の版・件数・時計が不正です"));
  M.Add(TEXT("Invalid player route squad"),TEXT("保存されたプレイヤー経路の部隊が不正です"));
  M.Add(TEXT("Invalid player route state"),TEXT("保存されたプレイヤー経路の命令・座標・期限が不正です"));
  M.Add(TEXT("Invalid player route waypoint"),TEXT("保存されたプレイヤー経路の中継点が不正です"));
  M.Add(TEXT("LAUNCHER EQUIPPED"),TEXT("対戦車火器を装備"));
  M.Add(TEXT("RIFLE EQUIPPED"),TEXT("ライフルを装備"));
  M.Add(TEXT("Invalid training price"),TEXT("練度の購入価格が不正です"));
  M.Add(TEXT("Match roster capacity reached (1000 including losses)"),TEXT("兵の記録数が上限に達しました（損失を含めて1000人）"));
  M.Add(TEXT("Match roster capacity reached (500 including losses)"),TEXT("兵の記録数が上限に達しました（損失を含めて500人）"));
  M.Add(TEXT("Could not write save file"),TEXT("保存ファイルを書き込めませんでした"));
  M.Add(TEXT("Indirect-fire trial configuration does not match"),TEXT("保存時と間接射撃の設定が違います"));
  M.Add(TEXT("Strength/shield trial configuration does not match"),TEXT("保存時と戦力・盾の設定が違います"));
  M.Add(TEXT("Composition planner configuration does not match"),TEXT("保存時と編成計画の設定が違います"));
  M.Add(TEXT("Composition saving price does not match definitions"),TEXT("保存された購入予定額が現在の定義と一致しません"));
  M.Add(TEXT("Mixed snapshot required"),TEXT("混成モードの保存データが必要です"));
  M.Add(TEXT("Player or game mode unavailable; restore aborted"),TEXT("操作担当またはゲームモードが利用できず読込を中止しました"));
  M.Add(TEXT("Navigation unavailable; restore aborted"),TEXT("経路が利用できず読込を中止しました"));
  M.Add(TEXT("Saved route unreachable; restore aborted"),TEXT("保存された経路を通れず読込を中止しました"));
  M.Add(TEXT("Could not stage body host; restore aborted"),TEXT("表示の土台を準備できず読込を中止しました"));
  M.Add(TEXT("Could not stage representations; restore aborted"),TEXT("兵の表示を準備できず読込を中止しました"));
  M.Add(TEXT("Restore failed while creating units"),TEXT("兵の生成中に読込に失敗しました"));
  M.Add(TEXT("RECRUIT"),TEXT("新兵"));
  M.Add(TEXT("REGULAR"),TEXT("正規兵"));
  M.Add(TEXT("VETERAN"),TEXT("熟練兵"));
  M.Add(TEXT("Area battle: new squads disabled"),TEXT("エリア戦：小隊の購入は無効"));M.Add(TEXT("Area battle: regular squads only [2]"),TEXT("エリア戦：購入できるのは正規兵の小隊だけ [2]"));
  M.Add(TEXT("Battle started"),TEXT("戦闘開始"));M.Add(TEXT("Deploy at least one squad before starting"),TEXT("開始前に1部隊以上を展開してください"));
  M.Add(TEXT("Deployment failed; credits retained"),TEXT("展開に失敗（資源は戻しました）"));M.Add(TEXT("Deployment navigation unavailable"),TEXT("展開先の経路がありません"));
  M.Add(TEXT("Direct control outside combat"),TEXT("戦闘中以外は直接操作できません"));M.Add(TEXT("Field capacity reached (100)"),TEXT("戦場の上限に達しました（100）"));
  M.Add(TEXT("Fixed 500-soldier field / no reinforcements"),TEXT("500人の固定戦場 ／ 増援なし"));M.Add(TEXT("Home deployment area is blocked"),TEXT("本拠地の展開区域がふさがっています"));
  M.Add(TEXT("Insufficient credits"),TEXT("資源が足りません"));M.Add(TEXT("Insufficient resources"),TEXT("資源が足りません"));
  M.Add(TEXT("Scout pair limit reached"),TEXT("偵察組の上限に達しました"));
  M.Add(TEXT("Anti-tank team limit reached"),TEXT("対戦車班の上限に達しました"));
  M.Add(TEXT("Tank limit reached"),TEXT("戦車の上限に達しました"));
  M.Add(TEXT("Armoured car limit reached"),TEXT("装甲車の上限に達しました"));
  M.Add(TEXT("Mortar team limit reached"),TEXT("迫撃砲班の上限に達しました"));
  M.Add(TEXT("Self-propelled gun limit reached"),TEXT("自走砲の上限に達しました"));
  M.Add(TEXT("LAUNCHER EMPTY / REFILL AT HOME"),TEXT("対戦車弾なし ／ 本拠地で補充"));M.Add(TEXT("LAUNCHER LOADING"),TEXT("対戦車火器を装填中"));
  M.Add(TEXT("Exoskeleton soldier limit reached"),TEXT("外骨格兵の上限に達しました"));
  M.Add(TEXT("Supply vehicle limit reached"),TEXT("補給車の上限に達しました"));
  M.Add(TEXT("Match restored"),TEXT("試合を読み込みました"));M.Add(TEXT("Match saved [F9 to load]"),TEXT("試合を保存しました [F9で読込]"));
  M.Add(TEXT("Mixed recruitment unavailable"),TEXT("購入できません"));M.Add(TEXT("Navigation unavailable"),TEXT("経路がありません"));
  M.Add(TEXT("No compatible save found"),TEXT("使える保存データがありません"));M.Add(TEXT("No free squad slot (active squad limit)"),TEXT("部隊の空きがありません（稼働部隊の上限）"));
  M.Add(TEXT("No reliable known enemy position / scout or receive a report"),TEXT("確かな敵の位置がありません ／ 偵察するか報告を待ってください"));
  M.Add(TEXT("Recruitment unavailable"),TEXT("今は購入できません"));M.Add(TEXT("Representation staging failed; purchase retained"),TEXT("表示の準備に失敗（購入は有効）"));
  M.Add(TEXT("Save mode, map or rule configuration does not match"),TEXT("保存時とモード・マップ・規則が違います"));
  M.Add(TEXT("Squad record capacity reached (100 including lost squads)"),TEXT("部隊記録の上限に達しました（失った部隊を含めて100）"));
  M.Add(TEXT("Support rearming (20s)"),TEXT("支援攻撃の再装填中（20秒）"));M.Add(TEXT("Support requested / impact in 3s / -400 credits"),TEXT("支援攻撃を要請 ／ 3秒後に着弾 ／ 資源 -400"));
  M.Add(TEXT("Support requires 400 credits"),TEXT("支援攻撃には資源400が必要です"));M.Add(TEXT("Support unavailable"),TEXT("支援攻撃は使えません"));
  M.Add(TEXT("Unsupported save mode or slot"),TEXT("この保存先・モードには対応していません"));M.Add(TEXT("Unsupported or invalid match header"),TEXT("保存データの形式が不正です"));
  return M;
 }();
 return Map;
}
struct FPattern { const TCHAR* Regex; const TCHAR* Format; };
}

static FString JaUncached(const FString& Text);
FString BattleUIText::Ja(const FString& Text)
{
 if(Text.IsEmpty()) return Text;
 if(const FString* Hit=Words().Find(Text)) return *Hit;
 // Performance (2026-11-17): the HUD translates the same reason strings every frame, and a miss compiles up to 14 regular
 // expressions. Results are remembered (game thread only; the table is cleared when it grows large). Same output.
 if(!IsInGameThread()) return JaUncached(Text);
 static TMap<FString,FString> Memo;
 if(const FString* Known=Memo.Find(Text)) return *Known;
 if(Memo.Num()>=4096) Memo.Reset();
 FString Out=JaUncached(Text);
 Memo.Add(Text,Out);
 return Out;
}
static FString JaUncached(const FString& Text)
{
 // Notices with numbers. Groups are inserted with {0}, {1}, {2}; a group that is itself English text is translated.
 static const FPattern Patterns[]={
  {TEXT("^(.*) squad deployed / -(\\d+) credits$"),TEXT("{0}の部隊を配備 ／ 資源 -{1}")},
  {TEXT("^(\\d+) soldiers deployed / -(\\d+) credits$"),TEXT("{0}人を配備 ／ 資源 -{1}")},
  {TEXT("^GAME SPEED x(\\d+) \\[\\.\\]$"),TEXT("速度 x{0} [.]")},
  {TEXT("^DIRECT CONTROL / UNIT (\\d+)$"),TEXT("直接操作 ／ 兵{0}")},
  {TEXT("^ORDER: (\\d+) SQUADS / (\\d+) REJECTED$"),TEXT("命令：{0}部隊 ／ 拒否 {1}")},
  {TEXT("^SQUAD (\\d+) / UNIT (\\d+): MOVE FAILED - issue a new order$"),TEXT("部隊{0} ／ 兵{1}：移動失敗。命令を出し直してください")},
  {TEXT("^(.*) / (\\d+) UNITS / (\\d+) SQUADS UNREACHABLE$"),TEXT("{0} ／ {1}体 ／ {2}部隊が到達不能")},
  {TEXT("^TEST TRAINING: (.*) / (\\d+) UNITS / equipment unchanged$"),TEXT("試験の練度：{0} ／ {1}体 ／ 装備は同じ")},
  {TEXT("^VERIFY: (\\d+) FAILURES / F5 TO RESET$"),TEXT("検証：失敗 {0} ／ F5でやり直し")},
  {TEXT("^New squad deployed / -(\\d+) resources$"),TEXT("新しい小隊を配備 ／ 資源 -{0}")},
  {TEXT("^Heavy infantry deployed / -(\\d+) resources$"),TEXT("重装歩兵隊を配備 ／ 資源 -{0}")},
  {TEXT("^Scout pair deployed / -(\\d+) resources$"),TEXT("偵察組を配備 ／ 資源 -{0}")},
  {TEXT("^Anti-tank team deployed / -(\\d+) resources$"),TEXT("対戦車班を配備 ／ 資源 -{0}")},
  {TEXT("^Tank deployed / -(\\d+) resources$"),TEXT("戦車を配備 ／ 資源 -{0}")},
  {TEXT("^Armoured car deployed / -(\\d+) resources$"),TEXT("装甲車を配備 ／ 資源 -{0}")},
  {TEXT("^Mortar team deployed / -(\\d+) resources$"),TEXT("迫撃砲班を配備 ／ 資源 -{0}")},
  {TEXT("^Self-propelled gun deployed / -(\\d+) resources$"),TEXT("自走砲を配備 ／ 資源 -{0}")},
  {TEXT("^LAUNCHER FIRED / (\\d+) LEFT$"),TEXT("対戦車火器を発射 ／ 残り{0}発")},
  {TEXT("^Exoskeleton soldier deployed / -(\\d+) resources$"),TEXT("外骨格兵を配備 ／ 資源 -{0}")},
  {TEXT("^Supply vehicle deployed / -(\\d+) resources$"),TEXT("補給車を配備 ／ 資源 -{0}")},
  {TEXT("^(\\d+) SQUADS / (.*)$"),TEXT("{0}部隊 ／ {1}")},
  {TEXT("^(.*) / (\\d+)m$"),TEXT("{0} ／ {1}m")},
 };
 for(const auto& P:Patterns)
 {
  FRegexMatcher M(FRegexPattern(P.Regex),Text);
  if(!M.FindNext()) continue;
  FString Out=P.Format;
  for(int32 G=1;G<=3;++G)
  {
   const FString Key=FString::Printf(TEXT("{%d}"),G-1); if(!Out.Contains(Key)) continue;
   const FString Part=M.GetCaptureGroup(G);
   Out.ReplaceInline(*Key,*(Part.IsNumeric()?Part:BattleUIText::Ja(Part)));
  }
  return Out;
 }
 // Segment by segment ("VICTORY / TIME LIMIT", "NO KNOWN CONTACT / FOLLOW MISSION").
 TArray<FString> Parts; Text.ParseIntoArray(Parts,TEXT(" / "),true);
 if(Parts.Num()>1)
 {
  bool bAny=false; for(FString& Part:Parts) if(const FString* Hit=Words().Find(Part)){Part=*Hit;bAny=true;}
  if(bAny) return FString::Join(Parts,TEXT(" ／ "));
 }
 return Text;
}

FString BattleUIText::SaveResult(bool bSuccess,bool bLoading,const FString& Detail)
{
 if(Detail==TEXT("Save file exceeds size limit"))return TEXT("保存ファイルがサイズ上限（1 GiB）を超えています");
 if(Detail==TEXT("Weapon or health inconsistent with squad type"))return TEXT("保存された武器または耐久値が兵種と一致しません");
 if(bSuccess)return bLoading?TEXT("試合を読み込みました（道筋の最終命令・複数選択・作戦カメラは復元対象外）"):
  TEXT("試合を保存しました [F9で読込]（道筋の最終命令・複数選択・作戦カメラは保存対象外）");
 const FString Translated=Ja(Detail);
 if(!Translated.IsEmpty() && Translated!=Detail)return Translated;
 return bLoading?TEXT("読込に失敗しました。詳しい理由はログを確認してください"):
  TEXT("保存に失敗しました。詳しい理由はログを確認してください");
}
