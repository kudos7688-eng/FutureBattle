// Purchase panel (2026-11-23, user request): B in the area battle's tactical view (pad: D-pad left, or the command bar's purchase button)
// opens a list of the squad types; a click on a row buys it through the same path as its number key. See FBattleBuyPanelLayout.
#include "BattleController.h"
#include "BattleMatch.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "BattleGameMode.h"
#include "FutureBattle.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace
{
 struct FBuyEntry { uint8 Type; const TCHAR* Key; const TCHAR* Name; const TCHAR* Role; };
 // Row order = number-key order (2 .. 0, -); those keys still buy directly.
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
 };
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
 int32 TypeCost(uint8 Type){return Type==BattleSquadType::Rifle?BattleTactics::AreaHire().Cost:BattleSquadType::AreaCost(Type);}
}

int32 FBattleBuyPanelLayout::RowCount(){return UE_ARRAY_COUNT(BuyEntries);}

FBattleBuyPanelLayout FBattleBuyPanelLayout::Make(FVector2D View,const UWorld* World,bool bPad)
{
 FBattleBuyPanelLayout L;
 const auto* M=World?World->GetSubsystem<UBattleMatch>():nullptr;
 if(!M||!M->bArea||!BattleTactics::AreaHire().bEnabled)return L;
 const bool bReserve=UBattleMatch::HireReserveOn();
 const int32 N=RowCount();
 // Title, column names, the rows and the footer; anchored just above the bottom panel, centred.
 const float Height=8.f+CloseSize+4.f+RowHeight+N*RowHeight+6.f+RowHeight+4.f;
 const float Bottom=FBattleCommandBarLayout::Make(View,World).PanelTop-8.f;
 L.Size=FVector2D(FMath::Min(Width,float(View.X)-32.f),Height);
 L.Origin=FVector2D(FMath::Max(16.f,(float(View.X)-L.Size.X)*.5f),FMath::Max(16.f,Bottom-Height));
 L.CloseOrigin=FVector2D(L.Origin.X+L.Size.X-6.f-CloseSize,L.Origin.Y+6.f);
 L.Title=FString::Printf(TEXT("部隊の購入 ／ 資源 %d ／ 部隊 %d/%d"),M->Credits[0],M->ActiveSquads(0),BattleTactics::AreaHire().MaxActive);
 if(bReserve)L.Title+=FString::Printf(TEXT(" ／ 予約 %d/%d"),M->HireQueueOf(0).Num(),UBattleMatch::HireQueueMax);
 float Y=L.Origin.Y+8.f+CloseSize+4.f;
 L.HeaderY=Y+5.f;Y+=RowHeight;
 for(int32 I=0;I<N;++I)
 {
  const FBuyEntry& E=BuyEntries[I];
  FRow R;R.Type=E.Type;R.Key=E.Key;R.Name=E.Name;R.Role=E.Role;
  R.Origin=FVector2D(L.Origin.X+6.f,Y);R.Size=FVector2D(L.Size.X-12.f,RowHeight-2.f);
  R.Members=BattleSquadType::IsVehicle(E.Type)?FString(TEXT("1両")):FString::Printf(TEXT("%d人"),BattleSquadType::Members(E.Type));
  const int32 Cost=TypeCost(E.Type);R.Cost=FString::FromInt(Cost);
  const int32 Have=M->ActiveOfType(0,E.Type),Limit=TypeLimit(E.Type);
  R.Owned=Limit>=0?FString::Printf(TEXT("%d/%d"),Have,Limit):FString::FromInt(Have);
  FString Why;R.Block=M->HireBlock(0,E.Type,Why);
  if(R.Block==0)R.State=TEXT("購入できる");
  else
  {
   // HireBlock's reasons (English, also in the log): resources, the active squad slots, the type's limit, anything else.
   const FString Wait=Why.Contains(TEXT("resources"))?FString::Printf(TEXT("資源 あと%d"),FMath::Max(1,Cost-M->Credits[0]))
    :Why.Contains(TEXT("slot"))?FString(TEXT("部隊数の上限")):Why.Contains(TEXT("limit"))?FString(TEXT("この兵種の上限")):FString(TEXT("今は買えない"));
   R.State=R.Block==1&&bReserve?FString::Printf(TEXT("予約できる（%s）"),*Wait):Wait;
  }
  L.Rows.Add(MoveTemp(R));Y+=RowHeight;
 }
 L.FooterY=Y+6.f+4.f;
 L.Footer=bPad?FString(TEXT("十字↑↓：選ぶ ／ A：購入 ／ B・十字←：閉じる ／ 開いたまま続けて買える"))
  :FString(TEXT("クリックで購入 ／ 開いたまま続けて買える ／ 2〜0・- キーでも購入 ／ B・× で閉じる"));
 if(bReserve)L.Footer+=TEXT(" ／ 買えない兵種は予約に入る");
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
 BuyPanelRow=Row;BuyArea(L.Rows[Row].Type);
 return true;
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
 if(Params.Key!=EKeys::LeftMouseButton||Params.Event!=IE_Pressed)return true;
 bBuyPress=true;bTriggerHeld=false;
 if(L.CloseAt(P)){bBuyPanel=false;return true;}
 if(const int32 Row=L.RowAt(P);Row!=INDEX_NONE){BuyPanelRow=Row;BuyArea(L.Rows[Row].Type);}
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
 if(!bUp&&!bDown&&!bBuy&&!bClose)return false; // sticks, zoom, menus and the rest keep working
 BuyPadKeys.Add(Params.Key);bGamepadActive=true;
 const int32 N=FBattleBuyPanelLayout::RowCount();
 if(bUp)BuyPanelRow=(BuyPanelRow+N-1)%N;
 else if(bDown)BuyPanelRow=(BuyPanelRow+1)%N;
 else if(bBuy)BuyFromPanel(BuyPanelRow);
 else bBuyPanel=false;
 return true;
}

// Display check -BattleBuyPanelShot=<prefix> (rendered game only, Setup/Test-BuyPanelShot.ps1): once the combat phase runs, the panel is
// opened (screenshot <prefix>-open.png), the first row that can be bought is bought through the panel (screenshot <prefix>-bought.png),
// the pad's D-pad down moves the highlight and its B closes the panel. BATTLE_BUY_PANEL_SHOT lines, then quit.
static void UpdateBuyPanelShot(ABattleController* PC)
{
 static const FString Prefix=[]{FString P;FParse::Value(FCommandLine::Get(),TEXT("BattleBuyPanelShot="),P);return P;}();
 if(Prefix.IsEmpty())return;
 static int32 Stage=0;static double Next=0.;
 UWorld* World=PC->GetWorld();auto* Match=World->GetSubsystem<UBattleMatch>();
 const double T=World->GetTimeSeconds();
 if(Stage==0&&T>6.&&Match->bArea&&Match->Phase==EBattleMatchPhase::Combat&&PC->IsTactical())
 {
  if(!PC->IsBuyPanelOpen())PC->ToggleBuyPanel();
  int32 W=0,H=0;PC->GetViewportSize(W,H);
  const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  const float Top=FBattleCommandBarLayout::Make(FVector2D(W,H),World).PanelTop;
  bool bHits=true;for(int32 I=0;I<L.Rows.Num();++I)bHits&=L.RowAt(L.Rows[I].Origin+L.Rows[I].Size*.5f)==I;
  int32 Buyable=0;for(const auto& R:L.Rows)Buyable+=R.Block==0;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=open t=%.1f open=%d view=%dx%d rows=%d buyable=%d hits=%d fits=%d credits=%d"),T,PC->IsBuyPanelOpen()?1:0,W,H,L.Rows.Num(),Buyable,bHits?1:0,
   L.Origin.X>=16.f&&L.Origin.Y>=16.f&&L.Origin.X+L.Size.X<=W-16.f&&L.Origin.Y+L.Size.Y<=Top?1:0,Match->Credits[0]);
  Stage=1;Next=T+.6;
 }
 else if(Stage==1&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-open.png"),true,false);Stage=2;Next=T+1.;}
 else if(Stage==2&&T>=Next)
 {
  int32 W=0,H=0;PC->GetViewportSize(W,H);
  const auto L=FBattleBuyPanelLayout::Make(FVector2D(W,H),World);
  int32 Row=INDEX_NONE;for(int32 I=0;I<L.Rows.Num()&&Row<0;++I)if(L.Rows[I].Block==0)Row=I;
  const int32 Credits=Match->Credits[0],Active=Match->ActiveSquads(0),Have=Row>=0?Match->ActiveOfType(0,L.Rows[Row].Type):-1;
  const bool bCalled=Row>=0&&PC->BuyFromPanel(Row);
  const bool bBought=bCalled&&Match->ActiveSquads(0)==Active+1&&Match->ActiveOfType(0,L.Rows[Row].Type)==Have+1&&Match->Credits[0]<Credits;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=buy t=%.1f row=%d type=%d bought=%d credits=%d->%d active=%d->%d open=%d"),T,Row,Row>=0?int32(L.Rows[Row].Type):-1,bBought?1:0,Credits,Match->Credits[0],Active,Match->ActiveSquads(0),PC->IsBuyPanelOpen()?1:0);
  Stage=3;Next=T+.6;
 }
 else if(Stage==3&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-bought.png"),true,false);Stage=4;Next=T+1.;}
 else if(Stage==4&&T>=Next)
 {
  const int32 Before=PC->GetBuyPanelRow();
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Down,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Down,IE_Released,0.f));
  const int32 After=PC->GetBuyPanelRow();
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Right,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Right,IE_Released,0.f));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BUY_PANEL_SHOT stage=pad t=%.1f row=%d->%d moved=%d closed=%d"),T,Before,After,After==(Before+1)%FBattleBuyPanelLayout::RowCount()?1:0,PC->IsBuyPanelOpen()?0:1);
  Stage=5;Next=T+2.;
 }
 else if(Stage==5&&T>=Next){Stage=6;FPlatformMisc::RequestExit(false,TEXT("BattleBuyPanelShot"));}
}

void ABattleController::UpdateBuyPanel()
{
 UpdateBuyPanelShot(this);
 if(!IsBuyPanelOpen())return;
 if(!GetWorld()->GetSubsystem<UBattleMatch>()->bArea){bBuyPanel=false;return;}
 // A pointer that moves onto a row highlights it (the pad's virtual cursor then buys it with A).
 FVector2D P;
 if(!GetPointerPosition(P)||P==LastBuyPointer)return;
 LastBuyPointer=P;
 int32 W=0,H=0;GetViewportSize(W,H);
 if(const int32 Row=FBattleBuyPanelLayout::Make(FVector2D(W,H),GetWorld()).RowAt(P);Row!=INDEX_NONE)BuyPanelRow=Row;
}
