// Top bar toolbar and quit menu (2026-11-25, user request). The top bar holds a button for each main key (AI pause, speed,
// quit, select all, form / select group, direct control / tactical view, grid, fog, ranges, plan panel); a left click runs the same action as the key, so a
// remote session needs no awkward keys. Quit (its button, Esc or F10) opens a confirmation panel; its quit button, Enter, Y or pad A ends
// the game; its back button, Esc, F10 or pad B closes it. Drawn by ABattleHUD::DrawQuitMenu.
#include "BattleController.h"
#include "FutureBattle.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BattleMatch.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Engine/LocalPlayer.h"
#include "InputKeyEventArgs.h"

FBattleQuitLayout FBattleQuitLayout::Make(FVector2D View)
{
 FBattleQuitLayout L;
 // Top bar (y 16..82): two rows of seven right of the view title and unit counts (drawn at 0.4 W, about 250 px wide).
 constexpr int32 Columns=7;constexpr float Gap=6.f,RowH=27.f;
 const float Left=FMath::Max(float(View.X)*.4f+260.f,300.f),Right=float(View.X)-24.f;
 const float Width=FMath::Max(40.f,(Right-Left-Gap*(Columns-1))/Columns);
 // Row 1: AI pause, speed, plan, (gap), quit at the far right; row 2: select all, form group, select group, control, grid, fog, ranges.
 // (Save and load sat between speed and plan until they were removed on 2026-11-25.)
 const uint8 Rows[2][Columns]={{AIPause,Speed,Plan,ToolCount,ToolCount,ToolCount,Quit},{SelectAll,FormGroup,SelectGroup,Control,Grid,Fog,Ranges}};
 for(int32 R=0;R<2;++R)for(int32 C=0;C<Columns;++C)if(Rows[R][C]!=ToolCount)
 {FTool T;T.Id=Rows[R][C];T.Origin=FVector2D(Left+C*(Width+Gap),20.f+R*(RowH+4.f));T.Size=FVector2D(Width,RowH);L.Tools.Add(T);}
 L.PanelSize=FVector2D(440,170);L.Panel=(View-L.PanelSize)*.5f;
 L.ChoiceSize=FVector2D(170,44);
 L.QuitChoice=L.Panel+FVector2D(30,L.PanelSize.Y-L.ChoiceSize.Y-26);
 L.Back=L.Panel+FVector2D(L.PanelSize.X-30-L.ChoiceSize.X,L.PanelSize.Y-L.ChoiceSize.Y-26);
 return L;
}

void ABattleController::SetQuitMenu(bool bOpen)
{
 if(bQuitMenu==bOpen)return;
 bQuitMenu=bOpen;bTriggerHeld=false;
 // Direct control hides the pointer; show it while the panel is open so its buttons can be clicked.
 // (The look actions ignore the mouse while it is open, so the view stays put behind the panel.)
 if(!bTactical)
 {
  bShowMouseCursor=bOpen;
  if(bOpen){FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);}
  else{FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(false);SetInputMode(Mode);} // as ControlUnit leaves it
  if(UGameViewportClient* Viewport=GetWorld()->GetGameViewport())
  {Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);}
 }
 UE_LOG(LogFutureBattle,Display,TEXT("QUIT_MENU open=%d"),bOpen?1:0);
}

void ABattleController::QuitGameNow()
{
 UE_LOG(LogFutureBattle,Display,TEXT("QUIT_MENU quit"));
 UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);
}

void ABattleController::RunTopTool(int32 Id)
{
 UE_LOG(LogFutureBattle,Display,TEXT("TOP_TOOL id=%d"),Id);
 switch(Id)
 {
 case FBattleQuitLayout::AIPause: ToggleCombat();break;
 case FBattleQuitLayout::Speed: CycleGameSpeed();break;
 case FBattleQuitLayout::Quit: SetQuitMenu(true);break;
 case FBattleQuitLayout::SelectAll: SelectAll();break;
 case FBattleQuitLayout::Control: ToggleMode();break;
 case FBattleQuitLayout::Grid: ToggleSectorGrid();break;
 case FBattleQuitLayout::Fog: ToggleFogView();break;
 case FBattleQuitLayout::Ranges: ToggleEnemyRanges();break;
 case FBattleQuitLayout::Plan: TogglePlanPanel();break;
 case FBattleQuitLayout::FormGroup: FormGroupSelected();break;
 case FBattleQuitLayout::SelectGroup: SelectGroupSelected();break;
 default: break;
 }
}

bool ABattleController::HandleQuitMenu(const FInputKeyEventArgs& Params)
{
 // Releases always pass on, so keys held before the panel opened are not left stuck (a swallowed press's release finds no held key).
 if(Params.Event==IE_Released)return false;
 const bool bPress=Params.Event==IE_Pressed;
 // F10: the same as Esc (a remote desktop may keep Esc for itself).
 if(Params.Key==EKeys::Escape||Params.Key==EKeys::F10){if(bPress)SetQuitMenu(!bQuitMenu);return true;}
 int32 ViewW=0,ViewH=0;GetViewportSize(ViewW,ViewH);
 const auto L=FBattleQuitLayout::Make(FVector2D(ViewW,ViewH));
 FVector2D P;const bool bPointer=GetPointerPosition(P);
 if(!bQuitMenu)
 {
  // The toolbar takes clicks where the pointer shows (tactical view).
  if(bTactical&&bPress&&Params.Key==EKeys::LeftMouseButton&&bPointer)
   if(const int32 Tool=L.ToolAt(P);Tool!=INDEX_NONE){bTriggerHeld=false;RunTopTool(Tool);return true;}
  return false;
 }
 // Open: the panel owns every press (the world gets no orders behind it); analog axes still pass for the camera.
 if(Params.Key.IsAnalog()&&!Params.Key.IsMouseButton())return false;
 if(!bPress)return true;
 if(Params.Key==EKeys::Enter||Params.Key==EKeys::Y||Params.Key==EKeys::Gamepad_FaceButton_Bottom){QuitGameNow();return true;}
 if(Params.Key==EKeys::Gamepad_FaceButton_Right){SetQuitMenu(false);return true;}
 if(Params.Key==EKeys::LeftMouseButton&&bPointer)
 {
  if(FBattleQuitLayout::In(P,L.QuitChoice,L.ChoiceSize))QuitGameNow();
  else if(FBattleQuitLayout::In(P,L.Back,L.ChoiceSize))SetQuitMenu(false);
 }
 return true;
}

// Display check -BattleQuitMenuShot=<prefix> (rendered game, Setup/Test-QuitMenuShot.ps1): once the area match's combat phase runs in the
// tactical view, <prefix>-button.png shows the top bar toolbar; clicks on its grid and speed buttons toggle them (put back after); the panel is opened through the input handler with a left press at the
// button's centre (<prefix>-open.png); a press on the back button closes it, Esc opens it again and a press on the quit button ends
// the game through QuitGameNow. BATTLE_QUIT_SHOT lines report each step.
void ABattleController::UpdateQuitMenuShot()
{
 static const FString Prefix=[]{FString P;FParse::Value(FCommandLine::Get(),TEXT("BattleQuitMenuShot="),P);return P;}();
 if(Prefix.IsEmpty())return;
 static int32 Stage=0;static double Next=0.;
 UWorld* World=GetWorld();auto* Match=World->GetSubsystem<UBattleMatch>();const double T=World->GetTimeSeconds();
 int32 W=0,H=0;GetViewportSize(W,H);const auto L=FBattleQuitLayout::Make(FVector2D(W,H));
 // A press and release of Key at P through the controller's InputKey (the pointer is moved there first).
 auto Press=[&](FVector2D P,FKey Key)
 {
  SetMouseLocation(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y));
  FViewport* View=GetLocalPlayer()&&GetLocalPlayer()->ViewportClient?GetLocalPlayer()->ViewportClient->Viewport:nullptr;
  const bool bTaken=InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),Key,IE_Pressed,FPlatformTime::Cycles64()));
  InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),Key,IE_Released,FPlatformTime::Cycles64()));
  return bTaken;
 };
 if(Stage==0&&T>6.&&Match->bArea&&Match->Phase==EBattleMatchPhase::Combat&&bTactical)
 {
  bool bFits=L.Tools.Num()==FBattleQuitLayout::ToolCount;
  for(const auto& Tool:L.Tools)bFits&=Tool.Origin.X>=16&&Tool.Origin.Y>=16&&Tool.Origin.X+Tool.Size.X<=W-16&&Tool.Origin.Y+Tool.Size.Y<=82&&L.ToolAt(Tool.Origin+Tool.Size*.5f)==Tool.Id;
  bFits&=L.Panel.X>=0&&L.Panel.Y>=0&&L.Panel.X+L.PanelSize.X<=W&&L.Panel.Y+L.PanelSize.Y<=H;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_QUIT_SHOT stage=button view=%dx%d fits=%d"),W,H,bFits?1:0);
  Stage=1;Next=T+.6;
 }
 else if(Stage==1&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-button.png"),true,false);Stage=2;Next=T+1.;}
 else if(Stage==2&&T>=Next)
 {
  // Two toolbar buttons through clicks: the grid toggles and the speed steps (both put back after).
  auto Centre=[&](uint8 Id){const auto* Tool=L.Find(Id);return Tool->Origin+Tool->Size*.5f;};
  const bool bGridBefore=bShowSectorGrid;const int32 SpeedBefore=GameSpeed;
  const bool bGridTaken=Press(Centre(FBattleQuitLayout::Grid),EKeys::LeftMouseButton);const bool bGridToggled=bShowSectorGrid!=bGridBefore;
  const bool bSpeedTaken=Press(Centre(FBattleQuitLayout::Speed),EKeys::LeftMouseButton);const bool bSpeedStepped=GameSpeed!=SpeedBefore;
  Press(Centre(FBattleQuitLayout::Grid),EKeys::LeftMouseButton);
  for(int32 I=0;I<3&&GameSpeed!=SpeedBefore;++I)Press(Centre(FBattleQuitLayout::Speed),EKeys::LeftMouseButton);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_QUIT_SHOT stage=tools grid=%d speed=%d restored=%d"),bGridTaken&&bGridToggled?1:0,bSpeedTaken&&bSpeedStepped?1:0,bShowSectorGrid==bGridBefore&&GameSpeed==SpeedBefore?1:0);
  Stage=21;Next=T+.6;
 }
 else if(Stage==21&&T>=Next)
 {
  const bool bTaken=Press(L.Find(FBattleQuitLayout::Quit)->Origin+L.Find(FBattleQuitLayout::Quit)->Size*.5f,EKeys::LeftMouseButton);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_QUIT_SHOT stage=open taken=%d open=%d"),bTaken?1:0,bQuitMenu?1:0);
  Stage=3;Next=T+.6;
 }
 else if(Stage==3&&T>=Next){FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-open.png"),true,false);Stage=4;Next=T+1.;}
 else if(Stage==4&&T>=Next)
 {
  Press(L.Back+L.ChoiceSize*.5f,EKeys::LeftMouseButton);const bool bClosed=!bQuitMenu;
  Press(FVector2D(W*.5f,H*.5f),EKeys::Escape);const bool bReopened=bQuitMenu;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_QUIT_SHOT stage=back closed=%d esc_reopened=%d"),bClosed?1:0,bReopened?1:0);
  Stage=5;Next=T+.6;
 }
 else if(Stage==5&&T>=Next)
 {
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_QUIT_SHOT stage=quit"));
  Stage=6;Press(L.QuitChoice+L.ChoiceSize*.5f,EKeys::LeftMouseButton);
 }
}
