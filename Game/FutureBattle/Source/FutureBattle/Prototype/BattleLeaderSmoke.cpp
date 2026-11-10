#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleMatch.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK) ++SmokeFailures; UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_TEST %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 if(SmokeStage++==0)
 {
  Check(Bodies.Num()==40 && Mode->Units.Num()==4,TEXT("four leaders and 36 Mass followers"));
  int32 Mass=0; for(const auto& B:Bodies) if(B.Value.Entity.IsSet()) ++Mass;
  Check(Mass==36,TEXT("followers have actual Mass entities"));
  Check(Store->Find(1)->bActorRepresentation && !Store->Find(2)->bActorRepresentation && !Store->Find(2)->bCanPlayerControl,TEXT("representation and control fixed at formation"));
  Check(CanSelectLeader(1) && !CanSelectLeader(2) && !CanSelectLeader(21),TEXT("only owned eligible leaders selectable"));
  Check(Summaries().Num()==2 && Summaries()[0].Name==TEXT("第1小隊"),TEXT("summary includes own squads only and stable names"));
  Check(Summaries()[0].Intent==EBattleSquadIntent::Unspecified && Summaries()[0].ProblemFlags==0,TEXT("intent and problem slots remain empty"));
  const auto Layout=FBattleSquadListLayout::Make(FVector2D(1920,1080));
  Check(Layout.VisibleRows==23 && Layout.RowAt(Layout.Origin+FVector2D(10,45))==0 && Layout.RowAt(Layout.Origin+FVector2D(10,5))==INDEX_NONE,TEXT("list rows and header hit regions"));
  Check(FBattleSquadListLayout::Make(FVector2D(1280,720)).VisibleRows==15,TEXT("list resize to 720p"));
  Check(!GetWorld()->GetSubsystem<UBattleMatch>()->SaveMatch(TEXT("LeaderUnsupported")),TEXT("unsupported save is rejected"));
  SmokePosition=Position(2); IssueOrder(1,EBattleOrder::Move,SmokePosition+FVector(650,0,0));
  Mode->bCombatPaused=false; Mode->CombatStartsAt=0;
  FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSmoke,4.f,false); return;
 }
 Mode->bCombatPaused=true;
 Check(FVector::Dist2D(SmokePosition,Position(2))>50,TEXT("Mass follower moves on shared route in live ticks"));
 Check(Plans.FindChecked(1).Path.Num()>1,TEXT("commander owns a NavMesh route"));
 Check(Mode->Units.Num()==4,TEXT("movement did not promote followers to Actors"));
 // Place two Mass bodies in a clear short-range lane; exercise actual sensing and physical hits.
 auto Place=[&](int32 ID,FVector P,FVector Facing) { auto& B=Bodies.FindChecked(ID); auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity); F.Position=P; F.Forward=Facing; B.Capsule->SetWorldLocation(P); };
 Place(2,FVector(-700,-2100,95),FVector::ForwardVector); Place(22,FVector(-500,-2100,95),-FVector::ForwardVector);
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>(); const double Now=GetWorld()->GetTimeSeconds(); Sense->Reset();
 Check(CanSee(2,22),TEXT("Mass sensors see a visible physical Mass target"));
 Sense->Update(Mode->Units,Now); Check(!Sense->FindContact(2,22),TEXT("detection delay is not bypassed"));
 Sense->Update(Mode->Units,Now+.7); Check(Sense->FindContact(2,22) && Sense->FindContact(2,22)->bVisible,TEXT("Mass contact acquired after original detection time"));
 const int32 Rounds=Store->Find(2)->Ammo; const float HP=Store->Find(22)->HP;
 Check(Fire(2,Position(2)+FVector(0,0,60),FVector::ForwardVector,false),TEXT("Mass uses common shot cadence"));
 Check(Store->Find(2)->Ammo==Rounds-1 && Store->Find(22)->HP<HP,TEXT("Mass shot consumes one round and damages physical target"));
 Check(!Fire(2,Position(2),FVector::ForwardVector,false),TEXT("second shot within interval is rejected"));
 auto* Wall=NewObject<UBoxComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Wall); Wall->SetBoxExtent(FVector(25,100,200)); Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Wall->SetCollisionResponseToAllChannels(ECR_Block); Wall->RegisterComponent(); Wall->SetWorldLocation(FVector(-600,-2100,100));
 Check(!CanSee(2,22),TEXT("Mass perception respects a blocking wall"));
 const FVector Known=Sense->FindContact(2,22)->LastKnownPosition;
 Place(22,FVector(-400,-2100,95),-FVector::ForwardVector); Sense->Update(Mode->Units,Now+.9);
 Check(Sense->FindContact(2,22) && !Sense->FindContact(2,22)->bVisible && Sense->FindContact(2,22)->LastKnownPosition==Known,TEXT("occluded contact retains last known position not live position"));
 Plans.FindChecked(1).TargetID=22; const int32 BeforeWallShot=Store->Find(2)->Ammo; Follow(2,.01f,Now+1);
 Check(Store->Find(2)->Ammo==BeforeWallShot,TEXT("follower cannot fire from remembered or blocked contact")); Wall->DestroyComponent();
 Check(PC && PC->ControlUnit(Actor(1)),TEXT("manual leader possession"));
 Check(Store->FindSquad(1)->CommandStatus==EBattleCommandStatus::Active,TEXT("possession does not disable command"));
 const int32 OtherLeader=11;
 Place(22,FVector(1300,-2100,95),-FVector::ForwardVector);
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); Match->Phase=EBattleMatchPhase::Combat; FBattleObjective Objective; Objective.Position=Position(2); Match->Objectives={Objective}; Mode->bCombatPaused=false; Match->UpdateObjectives(1.f);
 Check(Match->Objectives[0].CapturingTeam==0,TEXT("Mass follower participates in objective presence")); Mode->bCombatPaused=true;
 const FVector Before=Position(2); const int32 Ammo=Store->Find(2)->Ammo;
 Store->ApplyDamage(1,1,100000);
 Check(PC->IsTactical() && PC->GetSelection().IsEmpty(),TEXT("leader death returns to RTS without auto-selection"));
 int32 Alive=0; for(int32 ID:Store->FindSquad(1)->Members) if(Store->Find(ID)->IsAlive()) ++Alive;
 Mode->bCombatPaused=false; Match->UpdateObjectives(1.f); Mode->bCombatPaused=true;
 Check(Match->Objectives[0].CapturingTeam==INDEX_NONE,TEXT("disabled Mass squad no longer captures objectives")); Match->Phase=EBattleMatchPhase::Disabled;
 Check(Alive==0 && Store->FindSquad(1)->CommandStatus==EBattleCommandStatus::Disabled,TEXT("leader loss kills all squad members"));
 Check(Store->Find(2)->DeathCause==TEXT("LeaderLoss"),TEXT("cascade retains a distinct death cause"));
 Check(!Store->DisableLeaderSquad(1) && !Store->ApplyDamage(1,1,100000),TEXT("duplicate death is idempotent"));
 Check(!CanSelectLeader(1) && !PC->ControlUnit(Actor(1)) && !IssueOrder(1,EBattleOrder::Move,FVector::ZeroVector),TEXT("disabled squad rejects selection control and orders"));
 Follow(2,.1f,GetWorld()->GetTimeSeconds());
 Check(Position(2)==Before && Store->Find(2)->Ammo==Ammo && !Fire(2,Before,FVector::ForwardVector,false),TEXT("disabled follower cannot move or fire"));
 Check(Summaries().Num()==2 && !Summaries()[0].bActive && Summaries()[1].bActive,TEXT("stopped row retained without disabling other squad"));
 PC->ToggleMode(); Check(PC->IsTactical(),TEXT("Tab cannot resurrect last controlled leader"));
 PC->SetSelection(Actor(OtherLeader)); Check(PC->IsTactical(),TEXT("selection alone does not possess"));
 PC->ControlSelected(); Check(PC->GetPawn()==Actor(OtherLeader),TEXT("explicit E action controls manually selected leader"));
 Store->ApplyDamage(22,0,100000);
 Check(PC->GetPawn()==Actor(OtherLeader) && Store->FindSquad(3)->CommandStatus==EBattleCommandStatus::Active,TEXT("follower death neither switches control nor disables its squad"));
 Store->ApplyDamage(21,0,100000);
 Check(PC->GetPawn()==Actor(OtherLeader) && Store->FindSquad(3)->CommandStatus==EBattleCommandStatus::Disabled,TEXT("uncontrolled leader death does not move current control"));
 Check(!Summaries()[0].bSelectable,TEXT("summary selection state agrees with authority"));
 Mode->bBattleEnded=true; Check(!IssueOrder(2,EBattleOrder::Move,FVector::ZeroVector) && !CanControlLeader(11),TEXT("ended battle rejects new orders and possession"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_RESULT failures=%d"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}

void UBattleLeaderRuntime::RunUIQA()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Check=[&](bool OK,const TCHAR* Label){ if(!OK) ++SmokeFailures; UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_UI %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label); };
 int32 W,H; PC->GetViewportSize(W,H); const auto Layout=FBattleSquadListLayout::Make(FVector2D(W,H));
 if(QAStage==0)
 {
  int32 ExpectedW=1920, ExpectedH=1080; FParse::Value(FCommandLine::Get(),TEXT("ResX="),ExpectedW); FParse::Value(FCommandLine::Get(),TEXT("ResY="),ExpectedH);
  Check(W==ExpectedW && H==ExpectedH,TEXT("actual viewport matches requested resolution"));
  Store->ApplyDamage(21,1,100000); // Third friendly row; retained after leader death.
  PC->SetMouseLocation(int32(Layout.Origin.X+25),int32(Layout.Origin.Y+Layout.Header+10));
  auto* View=GetWorld()->GetGameViewport()->Viewport;
  PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftMouseButton,IE_Pressed,FPlatformTime::Cycles64()));
  PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftMouseButton,IE_Released,FPlatformTime::Cycles64()));
  Check(PC->IsTactical() && PC->IsSelected(Actor(1)),TEXT("list click selects without possessing"));
  PC->SelectSquadListRow(Layout.Origin+FVector2D(25,Layout.Header+Layout.RowHeight+10));
  Check(PC->IsSelected(Actor(1)) && !CanSelectLeader(11),TEXT("control-disabled row cannot replace selection"));
  PC->SelectSquadListRow(Layout.Origin+FVector2D(25,Layout.Header+2*Layout.RowHeight+10));
  Check(PC->IsSelected(Actor(1)) && !CanSelectLeader(21),TEXT("stopped row cannot replace selection"));
  FScreenshotRequest::RequestScreenshot(QAPrefix+TEXT("-top.png"),true,false);
 }
 else if(QAStage==1)
 {
  const FVector Camera=CastChecked<ABattleCameraPawn>(PC->GetPawn())->Camera->GetRelativeLocation(); SmokePosition=Camera;
  auto* View=GetWorld()->GetGameViewport()->Viewport;
  for(int32 I=0;I<30;++I) PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseScrollDown,IE_Pressed,FPlatformTime::Cycles64()));
  Check(PC->SquadListScroll==FMath::Max(0,Summaries().Num()-Layout.VisibleRows),TEXT("wheel reaches final row"));
  Check(CastChecked<ABattleCameraPawn>(PC->GetPawn())->Camera->GetRelativeLocation()==Camera,TEXT("list wheel does not move tactical camera"));
  FScreenshotRequest::RequestScreenshot(QAPrefix+TEXT("-bottom.png"),true,false);
 }
 else if(QAStage==2)
 {
  Check(CastChecked<ABattleCameraPawn>(PC->GetPawn())->Camera->GetRelativeLocation()==SmokePosition,TEXT("deferred input does not zoom behind list"));
  const int32 Index=FMath::Min(Layout.VisibleRows-1,Summaries().Num()-1);
  PC->SelectSquadListRow(Layout.Origin+FVector2D(25,Layout.Header+Index*Layout.RowHeight+10));
  const int32 ID=Summaries().Last().LeaderUnitID;
  Check(PC->IsTactical() && PC->IsSelected(Actor(ID)),TEXT("last visible row maps to last squad"));
  PC->ControlSelected(); Check(PC->GetPawn()==Actor(ID),TEXT("E enters selected leader"));
  Store->ApplyDamage(ID,1,100000); Check(PC->IsTactical() && PC->GetSelection().IsEmpty(),TEXT("selected leader death returns to RTS without transfer"));
  FScreenshotRequest::RequestScreenshot(QAPrefix+TEXT("-stopped.png"),true,false);
 }
 else
 {
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_UI_RESULT failures=%d"),SmokeFailures);
  FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0); return;
 }
 ++QAStage; FTimerHandle Timer; GetWorld()->GetTimerManager().SetTimer(Timer,this,&UBattleLeaderRuntime::RunUIQA,2.f,false);
}
