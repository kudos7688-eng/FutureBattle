// Purchase panel (2026-11-23, user request): B in the area battle's tactical view (pad: D-pad left, or the command bar's purchase button)
// opens a list of the squad types; a click on a row buys it through the same path as its number key, or reserves it when it cannot be
// bought now (-BattleHireReserve, default 1). See FBattleBuyPanelLayout.
#include "BattleController.h"
#include "BattleMatch.h"
#include "BattleArms.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "BattleGameMode.h"
#include "FutureBattle.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace
{
 struct FBuyEntry { uint8 Type; const TCHAR* Key; const TCHAR* Name; const TCHAR* Role; uint8 Grade=0; };
 // Row order = number-key order (2 .. 0, -); those keys still buy directly. The elite platoon and the sniper team (BattleArms.cpp,
 // Grade 1 / 2) have no key and show only with their trial switches (-BattleElite=1 / -BattleSniperTeam=1).
 const FBuyEntry BuyEntries[]=
 {
  {BattleSquadType::Rifle,TEXT("2"),TEXT("小銃小隊"),TEXT("歩兵の主力")},
  {BattleSquadType::Heavy,TEXT("3"),TEXT("重装歩兵"),TEXT("硬く火力が高い・遅い")},
  {BattleSquadType::Scout,TEXT("4"),TEXT("偵察組"),TEXT("止まると遠くまで見る")},
  {BattleSquadType::AntiTank,TEXT("5"),TEXT("対戦車班"),TEXT("車両を倒す")},
  {BattleSquadType::Tank,TEXT("6"),TEXT("戦車"),TEXT("厚い装甲と主砲")},
  {BattleSquadType::ArmoredCar,TEXT("7"),TEXT("装甲車"),TEXT("速い・機関銃・軽い装甲")},
  {BattleSquadType::Mortar,TEXT("8"),TEXT("迫撃砲班"),TEXT("知った位置へ曲射")},
  {BattleSquadType::SPGun,TEXT("9"),TEXT("自走砲"),TEXT("遠くへ曲射する車両")},
  {BattleSquadType::Exo,TEXT("0"),TEXT("外骨格兵"),TEXT("盾と対戦車弾を持つ1人")},
  {BattleSquadType::Supply,TEXT("-"),TEXT("補給車"),TEXT("近くの部隊に弾を補給")},
  {BattleSquadType::Rifle,TEXT(""),TEXT("精鋭小隊"),TEXT("8人の役割分担・手りゅう弾"),1},
  {BattleSquadType::Scout,TEXT(""),TEXT("狙撃班"),TEXT("狙撃手と観測手の2人"),2},
 };
 bool Shown(const FBuyEntry& E){return E.Grade==1?BattleArms::EliteEnabled():E.Grade==2?BattleArms::SniperEnabled():true;}
 // Per-type limit of the own side (UBattleMatch::HireBlock); -1 = only the active squad limit.
 int32 TypeLimit(uint8 Type)
 {
  const auto* T=GetDefault<UBattleTuning>();
  switch(Type)
  {
  case BattleSquadType::Scout: return FMath::Max(0,T->AreaScoutLimit);
  case BattleSquadType::AntiTank: return FMath::Max(0,T->AreaATLimit);
  case BattleSquadType::Tank: return FMath::Max(0,T->AreaTankLimit);
  case BattleSquadType::ArmoredCar: return FMath::Max(0,T->AreaACLimit);
  case BattleSquadType::Mortar: return FMath::Max(0,T->AreaMortarLimit);
  case BattleSquadType::SPGun: return FMath::Max(0,T->AreaSPGLimit);
  case BattleSquadType::Exo: return FMath::Max(0,T->AreaExoLimit);
  case BattleSquadType::Supply: return FMath::Max(0,T->AreaSupplyLimit);
  default: return -1;
  }
 }
 int32 TypeCost(uint8 Type,uint8 Grade=0){return Grade==1?BattleArms::EliteCost():Grade==2?BattleArms::SniperCost():Type==BattleSquadType::Rifle?BattleTactics::AreaHire().Cost:BattleSquadType::AreaCost(Type);}
}

int32 FBattleBuyPanelLayout::RowCount(){int32 N=0;for(const auto& E:BuyEntries)N+=Shown(E);return N;}

const TCHAR* FBattleBuyPanelLayout::TypeName(int32 Type)
{
 for(const FBuyEntry& E:BuyEntries)if(E.Type==Type)return E.Name;
 return TEXT("部隊");
}

FString FBattleBuyPanelLayout::WaitText(const UBattleMatch* Match,uint8 Type,const FString& Why)
{
 // UBattleMatch::HireBlock's and ReserveHire's reasons (English, also in the log). "slot" before "limit": the slot reason says "limit" too.
 if(Why.Contains(TEXT("resources")))return FString::Printf(TEXT("資源 あと%d"),FMath::Max(1,TypeCost(Type)-(Match?Match->Credits[0]:0)));
 if(Why.Contains(TEXT("slot")))return TEXT("部隊数の上限");
 if(Why.Contains(TEXT("limit")))return TEXT("この兵種の上限");
 if(Why.Contains(TEXT("same type")))return TEXT("同じ兵種の予約の後");
 if(Why.Contains(TEXT("starved")))return TEXT("先の予約の資源をためている");
 return TEXT("今は買えない");
}

FBattleBuyPanelLayout FBattleBuyPanelLayout::Make(FVector2D View,const UWorld* World,bool bPad)
{
 FBattleBuyPanelLayout L;
 const auto* M=World?World->GetSubsystem<UBattleMatch>():nullptr;
 if(!M||!M->bArea||!BattleTactics::AreaHire().bEnabled)return L;
 L.bReserve=UBattleMatch::HireReserveOn();
 const int32 N=RowCount();
 const TArray<int32> Mine=L.bReserve?M->HireQueueOf(0):TArray<int32>();
 const int32 Starved=L.bReserve?M->HireStarved(0):-1;
 // Title, column names, the rows, the own reservations in order (one line kept even when empty, so the panel does not jump) and the
 // footer; anchored just above the bottom panel, centred.
 const int32 QueueLines=L.bReserve?FMath::Max(1,FMath::DivideAndRoundUp(Mine.Num(),ChipsPerLine)):0;
 const float QueueHeight=L.bReserve?6.f+20.f+QueueLines*(ChipHeight+4.f):0.f;
 const float Height=8.f+CloseSize+4.f+RowHeight+N*RowHeight+QueueHeight+6.f+RowHeight+4.f;
 const float Bottom=FBattleCommandBarLayout::Make(View,World).PanelTop-8.f;
 L.Size=FVector2D(FMath::Min(Width,float(View.X)-32.f),Height);
 L.Origin=FVector2D(FMath::Max(16.f,(float(View.X)-L.Size.X)*.5f),FMath::Max(16.f,Bottom-Height));
 L.CloseOrigin=FVector2D(L.Origin.X+L.Size.X-6.f-CloseSize,L.Origin.Y+6.f);
 L.Title=FString::Printf(TEXT("部隊の購入 ／ 資源 %d ／ 部隊 %d/%d"),M->Credits[0],M->ActiveSquads(0),BattleTactics::AreaHire().MaxActive);
 if(L.bReserve)L.Title+=FString::Printf(TEXT(" ／ 予約 %d/%d"),Mine.Num(),UBattleMatch::HireQueueMax);
 float Y=L.Origin.Y+8.f+CloseSize+4.f;
 L.HeaderY=Y+5.f;Y+=RowHeight;
 // Elite platoons are rifle squads and sniper teams scout pairs (Grade 1 / 2): each row counts only its own grade.
 auto Graded=[M](uint8 Type,uint8 Grade){int32 C=0;for(const auto& P:M->GetWorld()->GetSubsystem<UBattleState>()->GetSquads())C+=P.Value.Team==0&&P.Value.CommandStatus==EBattleCommandStatus::Active&&P.Value.SquadType==Type&&P.Value.Grade==Grade;return C;};
 for(const FBuyEntry& E:BuyEntries)
 {
  if(!Shown(E))continue;
  FRow R;R.Type=E.Type;R.Grade=E.Grade;R.Key=E.Key;R.Name=E.Name;R.Role=E.Role;
  R.Origin=FVector2D(L.Origin.X+6.f,Y);R.Size=FVector2D(L.Size.X-12.f,RowHeight-2.f);
  R.Members=BattleSquadType::IsVehicle(E.Type)?FString(TEXT("1両")):FString::Printf(TEXT("%d人"),BattleSquadType::Members(E.Type));
  const int32 Cost=TypeCost(E.Type,E.Grade);R.Cost=FString::FromInt(Cost);
  const bool bGraded=E.Grade>0||((E.Type==BattleSquadType::Rifle||E.Type==BattleSquadType::Scout)&&(BattleArms::EliteEnabled()||BattleArms::SniperEnabled()));
  const int32 Have=bGraded?Graded(E.Type,E.Grade):M->ActiveOfType(0,E.Type),Limit=E.Grade==2?BattleArms::SniperLimit:E.Grade==1?-1:TypeLimit(E.Type);
  R.Owned=Limit>=0?FString::Printf(TEXT("%d/%d"),Have,Limit):FString::FromInt(Have);
  if(E.Grade==0)for(const int32 Q:Mine)R.Reserved+=M->Area.HireQueue[Q].Type==E.Type;
  FString Why;R.Block=M->HireBlock(0,E.Type,Why,E.Grade);
  // The elite platoon and the sniper team (Grade 1 / 2) are bought directly, never reserved.
  const bool bReserveRow=L.bReserve&&E.Grade==0;
  // What a click does now (UBattleMatch::ReserveHire): buy, or queue behind the same type / behind a reservation saving resources.
  if(R.Block==0)R.State=!bReserveRow?FString(TEXT("購入できる")):R.Reserved>0?FString(TEXT("予約に入る（同じ兵種の後）")):Starved>=0?FString(TEXT("予約に入る（資源をためている）")):FString(TEXT("購入できる"));
  else{const FString Wait=E.Grade>0&&Why.Contains(TEXT("resources"))?FString::Printf(TEXT("資源 あと%d"),FMath::Max(1,Cost-M->Credits[0])):WaitText(M,E.Type,Why);R.State=R.Block==1&&bReserveRow?FString::Printf(TEXT("予約できる（%s）"),*Wait):Wait;}
  L.Rows.Add(MoveTemp(R));Y+=RowHeight;
 }
 if(L.bReserve)
 {
  Y+=6.f;L.QueueY=Y+3.f;
  L.QueueTitle=Mine.IsEmpty()?FString(TEXT("予約なし（今は買えない兵種をクリックすると予約。買えるようになると自動で配備）"))
   :FString::Printf(TEXT("予約の順番 %d/%d（上から順に、買えるようになると自動で配備 ／ ↑：先へ ／ ×：取り消し）"),Mine.Num(),UBattleMatch::HireQueueMax);
  Y+=20.f;
  const float ChipWidth=(L.Size.X-12.f-(ChipsPerLine-1)*6.f)/ChipsPerLine;
  for(int32 K=0;K<Mine.Num();++K)
  {
   const auto& Q=M->Area.HireQueue[Mine[K]];
   FChip C;C.Index=K;C.bByAI=Q.bByAI;
   C.Origin=FVector2D(L.Origin.X+6.f+(K%ChipsPerLine)*(ChipWidth+6.f),Y+(K/ChipsPerLine)*(ChipHeight+4.f));C.Size=FVector2D(ChipWidth,ChipHeight);
   C.CancelOrigin=FVector2D(C.Origin.X+ChipWidth-ButtonSize-1.f,C.Origin.Y+1.f);C.UpOrigin=FVector2D(C.CancelOrigin.X-ButtonSize-2.f,C.Origin.Y+1.f);
   FString Why;const int32 B=M->HireBlock(0,uint8(Q.Type),Why);
   const FString Wait=B==0?FString(Starved>=0&&Mine[K]!=Starved?TEXT("資源をためている"):TEXT("まもなく配備")):B==1?WaitText(M,uint8(Q.Type),Why):FString(TEXT("今は買えない"));
   C.Text=FString::Printf(TEXT("%d.%s%s %s"),K+1,TypeName(Q.Type),Q.bByAI?TEXT("（AI）"):TEXT(""),*Wait);
   L.Chips.Add(MoveTemp(C));
  }
  Y+=QueueLines*(ChipHeight+4.f);
 }
 L.FooterY=Y+6.f+4.f;
 if(L.bReserve)
  L.Footer=bPad?FString(TEXT("十字↑↓：選ぶ ／ A：購入（買えなければ予約） ／ X：その兵種の予約を取り消す ／ B・十字←：閉じる"))
   :FString(TEXT("クリック：購入（買えなければ予約） ／ 右クリック：その兵種の予約を1つ取り消す ／ 2〜0・- キーでも同じ ／ B・× で閉じる"));
 else
  L.Footer=bPad?FString(TEXT("十字↑↓：選ぶ ／ A：購入 ／ B・十字←：閉じる ／ 開いたまま続けて買える"))
   :FString(TEXT("クリックで購入 ／ 開いたまま続けて買える ／ 2〜0・- キーでも購入 ／ B・× で閉じる"));
 return L;
}

void ABattleController::ToggleBuyPanel()
{
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(!bTactical||!Match->IsEnabled()||!Match->bArea)return;
 if(!bBuyPanel&&!BattleTactics::AreaHire().bEnabled){ShowNotice(TEXT("この試合では部隊を購入できません"));return;}
 bBuyPanel=!bBuyPanel;bBuyPress=false;LastBuyPointer=FVector2D(-1,-1);
 if(bBuyPanel)bPlanPanel=false; // one panel at a time
}

void ABattleController::BuyKey()
{
 // The support request never works on the large fields of the area battle (UBattleMatch::RequestSupport), so B is free there.
 if(GetWorld()->GetSubsystem<UBattleMatch>()->bArea){ToggleBuyPanel();return;}
 RequestSupport();
}

bool ABattleController::BuyFromPanel(int32 Row)
{
 int32 W=0,H=0;GetViewportSize(W,H);
 const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),GetWorld());
 if(!L.Rows.IsValidIndex(Row))return false;
 BuyPanelRow=Row;BuyArea(L.Rows[Row].Type,L.Rows[Row].Grade);
 return true;
}

bool ABattleController::CancelReservationOfType(uint8 Type)
{
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(!Match->bArea)return false;
 const TArray<int32> Mine=Match->HireQueueOf(0);
 for(int32 K=Mine.Num()-1;K>=0;--K)
  if(Match->Area.HireQueue[Mine[K]].Type==Type&&Match->CancelHire(0,K))
  {ShowNotice(FString::Printf(TEXT("予約を取り消しました：%s ／ 予約 %d/%d"),FBattleBuyPanelLayout::TypeName(Type),Mine.Num()-1,UBattleMatch::HireQueueMax));return true;}
 ShowNotice(FString::Printf(TEXT("%s の予約はありません"),FBattleBuyPanelLayout::TypeName(Type)));
 return false;
}

bool ABattleController::BuyPanelInput(const FInputKeyEventArgs& Params)
{
 const bool bMouse=Params.Key==EKeys::LeftMouseButton||Params.Key==EKeys::RightMouseButton||Params.Key==EKeys::MouseScrollUp||Params.Key==EKeys::MouseScrollDown;
 if(!bMouse)return false;
 if(Params.Key==EKeys::LeftMouseButton&&Params.Event==IE_Released&&bBuyPress){bBuyPress=false;bTriggerHeld=false;return true;}
 if(!IsBuyPanelOpen())return false;
 // A right drag begun on the field ends where it is released, even over the panel.
 if(Params.Key==EKeys::RightMouseButton&&Params.Event==IE_Released&&bPathDraw)return false;
 FVector2D P;int32 W=0,H=0;GetViewportSize(W,H);
 if(!GetPointerPosition(P))return false;
 const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),GetWorld());
 if(!L.Contains(P))return false;
 if(Params.Event!=IE_Pressed)return true; // releases and the wheel inside the panel do nothing
 // Right click on a row: that type's newest reservation is cancelled.
 if(Params.Key==EKeys::RightMouseButton){if(const int32 Row=L.RowAt(P);Row!=INDEX_NONE){BuyPanelRow=Row;CancelReservationOfType(L.Rows[Row].Type);}return true;}
 if(Params.Key!=EKeys::LeftMouseButton)return true;
 bBuyPress=true;bTriggerHeld=false;
 if(L.CloseAt(P)){bBuyPanel=false;return true;}
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(const int32 C=L.ChipButtonAt(P,true);C!=INDEX_NONE)
 {
  const TArray<int32> Mine=Match->HireQueueOf(0);
  const int32 Type=Mine.IsValidIndex(L.Chips[C].Index)?Match->Area.HireQueue[Mine[L.Chips[C].Index]].Type:-1;
  if(Match->CancelHire(0,L.Chips[C].Index))ShowNotice(FString::Printf(TEXT("予約を取り消しました：%s ／ 予約 %d/%d"),FBattleBuyPanelLayout::TypeName(Type),Mine.Num()-1,UBattleMatch::HireQueueMax));
  return true;
 }
 if(const int32 C=L.ChipButtonAt(P,false);C!=INDEX_NONE){if(L.Chips[C].Index>0)Match->MoveHire(0,L.Chips[C].Index,-1);return true;}
 if(const int32 Row=L.RowAt(P);Row!=INDEX_NONE){BuyPanelRow=Row;BuyArea(L.Rows[Row].Type,L.Rows[Row].Grade);}
 return true;
}

bool ABattleController::HandleBuyPanelPad(const FInputKeyEventArgs& Params)
{
 if(!Params.Key.IsGamepadKey()||Params.Key.IsAnalog())return false;
 // Keep ownership of a button the panel took through its release (also after the panel closed).
 if(BuyPadKeys.Contains(Params.Key)){if(Params.Event==IE_Released)BuyPadKeys.Remove(Params.Key);return true;}
 if(!IsBuyPanelOpen()||Params.Event!=IE_Pressed)return false;
 const bool bUp=Params.Key==EKeys::Gamepad_DPad_Up,bDown=Params.Key==EKeys::Gamepad_DPad_Down,bBuy=Params.Key==EKeys::Gamepad_FaceButton_Bottom;
 const bool bClose=Params.Key==EKeys::Gamepad_FaceButton_Right||Params.Key==EKeys::Gamepad_DPad_Left;
 const bool bCancel=Params.Key==EKeys::Gamepad_FaceButton_Left&&UBattleMatch::HireReserveOn();
 if(!bUp&&!bDown&&!bBuy&&!bClose&&!bCancel)return false; // sticks, zoom, menus and the rest keep working
 BuyPadKeys.Add(Params.Key);bGamepadActive=true;
 const int32 N=FBattleBuyPanelLayout::RowCount();
 if(bUp)BuyPanelRow=(BuyPanelRow+N-1)%N;
 else if(bDown)BuyPanelRow=(BuyPanelRow+1)%N;
 else if(bBuy)BuyFromPanel(BuyPanelRow);
 else if(bCancel)CancelReservationOfType(BuyEntries[FMath::Clamp(BuyPanelRow,0,N-1)].Type);
 else bBuyPanel=false;
 return true;
}

// Display check -BattleBuyPanelShot=<prefix> (rendered game only, Setup/Test-BuyPanelShot.ps1): once the combat phase runs, the panel is
// opened (screenshot <prefix>-open.png) and the first row that can be bought is bought through it (<prefix>-bought.png). With reservations
// (-BattleHireReserve, default 1) the own resources are then set low (test only) and a tank is reserved through the panel
// (<prefix>-reserved.png); resources back and the match running, the queue buys it with its notice (<prefix>-reserve-bought.png).
// The elite platoon and sniper team rows (when shown) are then bought through the panel (<prefix>-elite.png, <prefix>-sniper.png).
// Then the pad's D-pad down moves the highlight and its B closes the panel. BATTLE_BUY_PANEL_SHOT lines, then quit.
static void UpdateBuyPanelShot(ABattleController* PC)
{
 static const FString Prefix=[]{FString P;FParse::Value(FCommandLine::Get(),TEXT("BattleBuyPanelShot="),P);return P;}();
 if(Prefix.IsEmpty())return;
 static int32 Stage=0,TankRow=INDEX_NONE,TanksBefore=0,BoughtBefore=0,CreditsBack=0;static double Next=0.;
 // The graded rows (elite platoon / sniper team, when shown) are bought after the reservation stages.
 const int32 GradedStage=BattleArms::EliteEnabled()?11:BattleArms::SniperEnabled()?13:8;
 UWorld* World=PC->GetWorld();auto* Match=World->GetSubsystem<UBattleMatch>();
 const double T=World->GetTimeSeconds();
 int32 W=0,H=0;PC->GetViewportSize(W,H);
 if(Stage==0&&T>6.&&Match->bArea&&Match->Phase==EBattleMatchPhase::Combat&&PC->IsTactical())
 {
  if(!PC->IsBuyPanelOpen())PC->ToggleBuyPanel();
  PC->bGamepadActive=false; // the pictures show the mouse labels (the pad stage below switches to the pad)
  const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  const float Top=FBattleCommandBarLayout::Make(FVector2D(W,H),World).PanelTop;
  bool bHits=true;for(int32 I=0;I<L.Rows.Num();++I)bHits&=L.RowAt(L.Rows[I].Origin+L.Rows[I].Size*.5f)==I;
  int32 Buyable=0;for(const auto& R:L.Rows)Buyable+=R.Block==0;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=open t=%.1f open=%d view=%dx%d rows=%d buyable=%d hits=%d fits=%d credits=%d reserve=%d"),T,PC->IsBuyPanelOpen()?1:0,W,H,L.Rows.Num(),Buyable,bHits?1:0,
   L.Origin.X>=16.f&&L.Origin.Y>=16.f&&L.Origin.X+L.Size.X<=W-16.f&&L.Origin.Y+L.Size.Y<=Top?1:0,Match->Credits[0],L.bReserve?1:0);
  Stage=1;Next=T+.6;
 }
 else if(Stage==1&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-open.png"),true,false);Stage=2;Next=T+1.;}
 else if(Stage==2&&T>=Next)
 {
  const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  int32 Row=INDEX_NONE;for(int32 I=0;I<L.Rows.Num()&&Row<0;++I)if(L.Rows[I].Block==0&&L.Rows[I].Reserved==0)Row=I;
  const int32 Credits=Match->Credits[0],Active=Match->ActiveSquads(0),Have=Row>=0?Match->ActiveOfType(0,L.Rows[Row].Type):-1;
  const bool bCalled=Row>=0&&PC->BuyFromPanel(Row);
  const bool bBought=bCalled&&Match->ActiveSquads(0)==Active+1&&Match->ActiveOfType(0,L.Rows[Row].Type)==Have+1&&Match->Credits[0]<Credits;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=buy t=%.1f row=%d type=%d bought=%d credits=%d->%d active=%d->%d open=%d pad_labels=%d"),T,Row,Row>=0?int32(L.Rows[Row].Type):-1,bBought?1:0,Credits,Match->Credits[0],Active,Match->ActiveSquads(0),PC->IsBuyPanelOpen()?1:0,PC->bGamepadActive?1:0);
  Stage=3;Next=T+.6;
 }
 else if(Stage==3&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-bought.png"),true,false);Stage=UBattleMatch::HireReserveOn()?4:GradedStage;Next=T+1.;}
 else if(Stage==4&&T>=Next)
 {
  // Reserve a tank with too few resources (test only: the own resources are lowered here and given back in stage 6).
  CreditsBack=Match->Credits[0];Match->Credits[0]=FMath::Min(Match->Credits[0],5);
  const auto Before=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  for(int32 I=0;I<Before.Rows.Num();++I)if(Before.Rows[I].Type==BattleSquadType::Tank)TankRow=I;
  const int32 Queued=Match->HireQueueOf(0).Num();TanksBefore=Match->ActiveOfType(0,BattleSquadType::Tank);BoughtBefore=Match->HireQueueBought[0];
  const bool bCalled=TankRow>=0&&PC->BuyFromPanel(TankRow);
  const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  const bool bChip=L.Chips.Num()==Queued+1&&L.ChipButtonAt(L.Chips.Last().CancelOrigin+FVector2D(L.ButtonSize*.5f),true)==L.Chips.Num()-1&&L.ChipButtonAt(L.Chips.Last().UpOrigin+FVector2D(L.ButtonSize*.5f),false)==L.Chips.Num()-1;
  const float Top=FBattleCommandBarLayout::Make(FVector2D(W,H),World).PanelTop;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=reserve t=%.1f row=%d called=%d queued=%d->%d chips=%d chip_hits=%d reserved_in_row=%d fits=%d tanks=%d notice=\"%s\""),T,TankRow,bCalled?1:0,Queued,Match->HireQueueOf(0).Num(),L.Chips.Num(),bChip?1:0,
   L.Rows.IsValidIndex(TankRow)?L.Rows[TankRow].Reserved:-1,L.Origin.Y>=16.f&&L.Origin.Y+L.Size.Y<=Top?1:0,Match->ActiveOfType(0,BattleSquadType::Tank),*PC->Notice);
  Stage=5;Next=T+.6;
 }
 else if(Stage==5&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-reserved.png"),true,false);Stage=6;Next=T+1.;}
 else if(Stage==6&&T>=Next)
 {
  // Resources back and the match running: the queue buys the tank within a match second (UBattleMatch::UpdateHireQueue).
  Match->Credits[0]=FMath::Max(CreditsBack,BattleSquadType::AreaCost(BattleSquadType::Tank));
  if(auto* Mode=World->GetAuthGameMode<ABattleGameMode>())Mode->bCombatPaused=false;
  Stage=7;Next=T+2.5;
 }
 else if(Stage==7&&T>=Next)
 {
  if(auto* Mode=World->GetAuthGameMode<ABattleGameMode>())Mode->bCombatPaused=true;
  const bool bBought=Match->HireQueueBought[0]>BoughtBefore&&Match->ActiveOfType(0,BattleSquadType::Tank)==TanksBefore+1&&Match->HireQueueOf(0).IsEmpty();
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=reservebuy t=%.1f bought=%d queued=%d tanks=%d->%d bought_count=%d->%d notice=\"%s\""),T,bBought?1:0,Match->HireQueueOf(0).Num(),
   TanksBefore,Match->ActiveOfType(0,BattleSquadType::Tank),BoughtBefore,Match->HireQueueBought[0],*PC->Notice);
  FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-reserve-bought.png"),true,false);
  Stage=GradedStage;Next=T+1.;
 }
 else if((Stage==11||Stage==13)&&T>=Next)
 {
  // The elite platoon (stage 11) and sniper team (13) rows: the row under its centre (the mouse hit test) is bought through the panel
  // with enough resources (test only), and one squad of that grade appears. Pictures <prefix>-elite.png / -sniper.png.
  const uint8 Grade=Stage==11?1:2;const uint8 Type=Grade==1?BattleSquadType::Rifle:BattleSquadType::Scout;
  Match->Credits[0]=FMath::Max(Match->Credits[0],(Grade==1?BattleArms::EliteCost():BattleArms::SniperCost())+5);
  const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  int32 Row=INDEX_NONE;for(int32 I=0;I<L.Rows.Num();++I)if(L.Rows[I].Grade==Grade)Row=I;
  const int32 Hit=Row>=0?L.RowAt(L.Rows[Row].Origin+L.Rows[Row].Size*.5f):INDEX_NONE;
  auto Count=[&]{int32 C=0;for(const auto& P:World->GetSubsystem<UBattleState>()->GetSquads())C+=P.Value.Team==0&&P.Value.CommandStatus==EBattleCommandStatus::Active&&P.Value.SquadType==Type&&P.Value.Grade==Grade;return C;};
  const int32 Before=Count(),Credits=Match->Credits[0];
  const bool bCalled=Hit>=0&&Hit==Row&&PC->BuyFromPanel(Hit);
  const int32 After=Count();
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=%s t=%.1f row=%d hit=%d called=%d bought=%d squads=%d->%d credits=%d->%d state=\"%s\" notice=\"%s\""),Grade==1?TEXT("elite"):TEXT("sniper"),T,Row,Hit==Row&&Row>=0?1:0,bCalled?1:0,
   After==Before+1&&Match->Credits[0]<Credits?1:0,Before,After,Credits,Match->Credits[0],L.Rows.IsValidIndex(Row)?*L.Rows[Row].State:TEXT(""),*PC->Notice);
  Stage=Stage+1;Next=T+.6;
 }
 else if((Stage==12||Stage==14)&&T>=Next)
 {
  FScreenshotRequest::RequestScreenshot(Prefix+(Stage==12?TEXT("-elite.png"):TEXT("-sniper.png")),true,false);
  Stage=Stage==12&&BattleArms::SniperEnabled()?13:8;Next=T+1.;
 }
 else if(Stage==8&&T>=Next)
 {
  const int32 Before=PC->GetBuyPanelRow();
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Down,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Down,IE_Released,0.f));
  const int32 After=PC->GetBuyPanelRow();
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Right,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Right,IE_Released,0.f));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=pad t=%.1f row=%d->%d moved=%d closed=%d"),T,Before,After,After==(Before+1)%FBattleBuyPanelLayout::RowCount()?1:0,PC->IsBuyPanelOpen()?0:1);
  Stage=9;Next=T+2.;
 }
 else if(Stage==9&&T>=Next){Stage=10;FPlatformMisc::RequestExit(false,TEXT("BattleBuyPanelShot"));}
}

void ABattleController::UpdateBuyPanel()
{
 UpdateBuyPanelShot(this);
 UpdateQuitMenuShot();
 UpdatePlanPanelShot();
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 // A reservation bought by the queue (UBattleMatch::UpdateHireQueue) is announced, the panel open or not.
 if(Match->bArea)
 {
  if(Match->HireQueueBought[0]>SeenReservationBuys)
   ShowNotice(FString::Printf(TEXT("%s：%s を配備しました"),Match->HireQueueBoughtByAI[0]?TEXT("AI の予約"):TEXT("予約していた部隊"),FBattleBuyPanelLayout::TypeName(Match->HireQueueBoughtType[0])));
  SeenReservationBuys=Match->HireQueueBought[0];
 }
 if(!IsBuyPanelOpen())return;
 if(!Match->bArea){bBuyPanel=false;return;}
 // A pointer that moves onto a row highlights it (the pad's virtual cursor then buys it with A).
 FVector2D P;
 if(!GetPointerPosition(P)||P==LastBuyPointer)return;
 LastBuyPointer=P;
 int32 W=0,H=0;GetViewportSize(W,H);
 if(const int32 Row=FBattleBuyPanelLayout::Make(FVector2D(W,H),GetWorld()).RowAt(P);Row!=INDEX_NONE)BuyPanelRow=Row;
}
