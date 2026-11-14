#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleSquadTactics.h"
#include "BattleVATDisplay.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace { bool WideRestartExpected=false; int32 WidePreviousFailures=0; }

// Unmodified starting forces and rules: delegate allies, let the actual match reach a result.
void UBattleLeaderRuntime::RunWideSoak()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(WideStage++==0)
 {
  PC->SelectAll(); PC->DelegateSelected(); PC->SetSelection(Actor(1));
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())) { Camera->SetActorLocation(FVector::ZeroVector); Camera->Zoom(30000); }
  Mode->bCombatPaused=false; Mode->CombatStartsAt=0; WideStart=Position(2);
 }
 int32 Living[2]={0,0}; int32 Shots=0,MassShots=0;
 for(const auto& U:Store->GetUnits()) { if(U.Value.IsAlive()) ++Living[U.Value.Team]; Shots+=U.Value.ShotSequence; if(!U.Value.bActorRepresentation) MassShots+=U.Value.ShotSequence; }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WIDE_SOAK time=%.2f alive=%d/%d shots=%d massShots=%d score=%.2f/%.2f"),Match->Elapsed,Living[0],Living[1],Shots,MassShots,Match->Score[0],Match->Score[1]);
 if(Match->Phase==EBattleMatchPhase::Finished || Match->Elapsed>185)
 {
  const bool OK=Match->Phase==EBattleMatchPhase::Finished && Mode->bBattleEnded && Shots>0 && MassShots>0 && Bodies.Num()==500 && Mode->Units.Num()==50 && FVector::Dist2D(WideStart,Position(2))>50;
  Mode->bCombatPaused=true;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WIDE_SOAK_RESULT failures=%d winner=%d reason=%s"),OK?0:1,Match->Winner,*Mode->Result);
  FString Prefix; FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-result.png"),true,false);
  FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[OK](){FPlatformMisc::RequestExitWithStatus(false,OK?0:1);}),1.f,false); return;
 }
 FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWideSoak,5.f,false);
}

void UBattleLeaderRuntime::RunWideSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Text){ if(!OK) ++SmokeFailures; UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WIDE_TEST %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Text); };
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 FString Prefix; FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 auto Later=[this](float Seconds){ FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWideSmoke,Seconds,false); };
 auto Place=[&](int32 ID,FVector P,FVector Direction)
 {
  auto& B=Bodies.FindChecked(ID); auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);
  F.Position=P; F.Forward=Direction; B.Capsule->SetWorldLocation(P);
 };
 if(WideRestartExpected)
 {
  SmokeFailures+=WidePreviousFailures;
  Check(Bodies.Num()==500 && Mode->Units.Num()==50 && Store->GetSquads().Num()==50,TEXT("F5 reload recreates exactly 50 Actors and 450 Mass without duplicates"));
  Check(Mode->bCombatPaused && !Mode->bBattleEnded && Match->Elapsed==0 && Match->Score[0]==0 && Match->Phase==EBattleMatchPhase::Combat,TEXT("F5 resets objective match clock score and pause state"));
  Check(!Store->FindSquad(1)->bPlayerDelegated && Store->Find(1)->HP==Store->MaxHealthOf(1),TEXT("F5 resets command ownership and casualties"));
  WideRestartExpected=false;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WIDE_RESULT failures=%d"),SmokeFailures);
  FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0); return;
 }
 switch(WideStage++)
 {
 case 0:
 {
  bool Bounded=true;
  for(int32 Heading=0;Heading<8;++Heading) for(int32 X=-32;X<=32;++X) for(int32 Y=-32;Y<=32;++Y)
  {
   const FVector D=FVector::ForwardVector.RotateAngleAxis(Heading*45.f,FVector::UpVector);
   const FVector Result=LimitedSteering(D,FVector(X,Y,0),.2f);
   Bounded&=FVector::DotProduct(Result,D)>=.9805 && FMath::IsNearlyEqual(Result.Size2D(),1.,.001);
  }
  Check(Bounded,TEXT("dense crowd steering preserves at least 98 percent forward component across all headings"));
  Check(LimitedSteering(FVector::ForwardVector,FVector(-32,0,0),.2f).Equals(FVector::ForwardVector),TEXT("backward crowd pressure cannot reverse a move command"));
  Check(LimitedSteering(FVector::ZeroVector,FVector(0,32,0),.2f).IsZero(),TEXT("separation alone cannot start movement without a route direction"));
  bool SingleSteering=true; for(ABattleUnit* Unit:Mode->Units) SingleSteering&=Unit->GetCharacterMovement()->GetRVOAvoidanceWeight()==1.f;
  Check(SingleSteering,TEXT("wide Actor leaders do not stack RVO velocity correction on shared steering"));
  Check(bWideField && Bodies.Num()==500 && Mode->Units.Num()==50 && Store->GetUnits().Num()==500,TEXT("wide runtime owns 500 canonical units and 50 unit Actors"));
  int32 Mass=0,Teams[2]={0,0},Actors[2]={0,0}; bool NavOK=true;
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
  for(const auto& B:Bodies)
  {
   const auto* U=Store->Find(B.Key); ++Teams[U->Team]; Actors[U->Team]+=B.Value.Actor.IsValid();
   Mass+=B.Value.Entity.IsSet() && Manager->IsEntityValid(B.Value.Entity);
   FNavLocation N; NavOK &= Nav && Nav->ProjectPointToNavigation(Position(B.Key),N,FVector(100,100,400)) && FVector::Dist2D(Position(B.Key),N.Location)<30;
  }
  Check(Mass==450 && Teams[0]==250 && Teams[1]==250 && Actors[0]==25 && Actors[1]==25,TEXT("each army has 25 Actor leaders plus 225 real Mass entities"));
  Check(NavOK,TEXT("all 500 spawn positions project onto wide map navigation"));
  Check(Match->Objectives.Num()==3 && Match->Objectives[0].Position.Equals(FVector(0,-4500,0)) && Match->Objectives[2].Position.Equals(FVector(0,4500,0)),TEXT("wide three-point match initializes correct objective positions"));
  Check(Mode->bCombatPaused && Match->Elapsed==0,TEXT("wide battle starts paused"));
  Check(Summaries().Num()==25 && CanControlLeader(1) && !CanControlLeader(2) && !CanControlLeader(251),TEXT("only 25 owned leaders are selectable and controllable"));
  PC->SelectAll(); int32 Manual=0,AI=0; PC->GetDelegationCounts(Manual,AI);
  Check(PC->GetSelection().Num()==25 && Manual==25 && AI==0,TEXT("Q selects 25 manual allied squads"));
  PC->DelegateSelected(); PC->GetDelegationCounts(Manual,AI);
  Check(Manual==0 && AI==25,TEXT("J delegates all 25 allied squads"));
  PC->TakeCommandSelected(); PC->SetSelection(Actor(1)); PC->DelegateSelected();
  const int32 Untouched=Store->FindSquad(2)->OrderRevision;
  Mode->CombatStartsAt=0; Mode->bCombatPaused=false; Match->Update(.1f); Mode->bCombatPaused=true;
  Check(Store->FindSquad(1)->Order==EBattleOrder::AttackMove && Store->FindSquad(2)->OrderRevision==Untouched,TEXT("director commands delegated squad and preserves manual squad"));
  Check(Store->FindSquad(26)->Order==EBattleOrder::AttackMove,TEXT("enemy director issues wide objective orders"));
  Check(Store->Find(2)->Order==EBattleOrder::AttackMove && Store->Find(10)->OrderRevision>0,TEXT("commander order reaches all nine Mass members"));
  const int32 Rev=Store->FindSquad(1)->OrderRevision;
  Check(!IssueOrder(1,EBattleOrder::Move,FVector(1000000,1000000,0)) && Store->FindSquad(1)->bPlayerDelegated && Store->FindSquad(1)->OrderRevision==Rev,TEXT("unreachable order preserves delegation and previous execution"));
  Check(IssueOrder(1,EBattleOrder::Move,FVector(-7000,-5600,0)) && !Store->FindSquad(1)->bPlayerDelegated,TEXT("accepted manual order takes command"));
  PC->DelegateSelected(); PC->HoldSelected();
  Check(!Store->FindSquad(1)->bPlayerDelegated && Store->Find(2)->Order==EBattleOrder::Hold,TEXT("H cancels delegation and holds Mass members"));
  PC->DelegateSelected(); const int32 Keep=Store->FindSquad(1)->OrderRevision; PC->TakeCommandSelected();
  Check(!Store->FindSquad(1)->bPlayerDelegated && Store->FindSquad(1)->OrderRevision==Keep,TEXT("K keeps executing order when returning to manual"));
  const auto Before=*Store->Find(1);
  Check(PC->ControlUnit(Actor(1)),TEXT("E can control Actor leader on the wide map"));
  PC->EnterTactical();
  Check(Store->Find(1)->HP==Before.HP && Store->Find(1)->Ammo==Before.Ammo && Store->Find(1)->UnitID==Before.UnitID,TEXT("TPS RTS switching preserves canonical resources and identity"));
  // Physical Mass firing, then restore locations. No visible contact is fabricated.
  const FVector A=Position(2),B=Position(252),AF=Forward(2),BF=Forward(252);
  Place(2,FVector(-700,-7600,100),FVector::ForwardVector); Place(252,FVector(-500,-7600,100),-FVector::ForwardVector);
  auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>(); Sense->Reset(); const double Now=GetWorld()->GetTimeSeconds();
  Sense->Update(Mode->Units,Now); Check(!Sense->FindContact(2,252),TEXT("Mass detection delay is retained on wide field"));
  Sense->Update(Mode->Units,Now+.7);
  Check(Sense->FindContact(2,252) && Sense->FindContact(2,252)->bVisible,TEXT("Mass observer discovers Mass target after detection delay"));
  const int32 Ammo=Store->Find(2)->Ammo; const float HP=Store->Find(252)->HP;
  Check(Fire(2,Position(2)+FVector(0,0,60),FVector::ForwardVector,false) && Store->Find(2)->Ammo==Ammo-1 && Store->Find(252)->HP<HP,TEXT("wide Mass hit consumes ammunition and damages physical Mass target"));
  const auto Points=Match->Objectives;
  Place(2,Match->Objectives[0].Position+FVector(0,0,100),FVector::ForwardVector);
  Mode->bCombatPaused=false; Match->UpdateObjectives(8); Match->UpdateObjectives(1); Mode->bCombatPaused=true;
  Check(Match->Objectives[0].Owner==0 && Match->Score[0]>0,TEXT("Mass-only presence captures and scores a wide objective"));
  Match->Objectives=Points; Match->Score[0]=Match->Score[1]=0;
  Place(2,A,AF); Place(252,B,BF); Sense->Reset();
  const auto Obs=Tactics->Observe(*Store->FindSquad(1));
  Check(Obs.Living==10 && Obs.Ammo==Store->Find(1)->Ammo+Store->Find(2)->Ammo+Store->Find(3)->Ammo+Store->Find(4)->Ammo+Store->Find(5)->Ammo+Store->Find(6)->Ammo+Store->Find(7)->Ammo+Store->Find(8)->Ammo+Store->Find(9)->Ammo+Store->Find(10)->Ammo,TEXT("ST1 aggregation includes all ten mixed members"));
  TMap<int32,FVector> Goals; for(int32 ID:Store->FindSquad(1)->Members) Goals.Add(ID,Position(ID));
  PC->DelegateSelected();
  Check(Store->SetSquadTacticalPlan(1,EBattleSquadTactic::Retreat,EBattleOrder::Move,Position(1),Goals,3,TEXT("INTEGRATION FIXTURE")),TEXT("ST1 plan authority accepts mixed squad"));
  const int32 PlanRev=Store->FindSquad(1)->OrderRevision;
  Mode->bCombatPaused=false; Match->DelegationClock=5; Match->Update(.1f); Mode->bCombatPaused=true;
  Check(Store->FindSquad(1)->OrderRevision==PlanRev && Store->FindSquad(1)->TacticalPlan.Action==EBattleSquadTactic::Retreat,TEXT("wide director does not overwrite tactical execution"));
  const double Clock=Tactics->Clock(); const float Elapsed=Match->Elapsed; const FVector Frozen=Position(2);
  Step(2);
  Check(Tactics->Clock()==Clock && Match->Elapsed==Elapsed && Position(2)==Frozen,TEXT("P pause freezes Mass movement match and tactical clocks"));
  PC->HoldSelected();
  Check(IssueOrder(1,EBattleOrder::Move,FVector(-7000,-5600,0)),TEXT("wide navigation accepts shared march route"));
  WideStart=Position(2); SmokePosition=Position(1); Mode->bCombatPaused=false;
  Later(6); return;
 }
 case 1:
 {
  Mode->bCombatPaused=true;
  Check(FVector::Dist2D(WideStart,Position(2))>50 && FVector::Dist2D(SmokePosition,Position(1))>50,TEXT("Actor leader and Mass follower both advance during live world ticks"));
  Check(Plans.FindChecked(1).Path.Num()>1 && Mode->Units.Num()==50 && Bodies.Num()==500,TEXT("shared navigation does not promote Mass followers into Actors"));
  Check(Tactics->Find(26)!=nullptr,TEXT("mixed enemy squad produces live ST1 samples"));
  PC->SelectAll(); PC->DelegateSelected(); PC->SetSelection(Actor(1));
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())) { Camera->SetActorLocation(FVector::ZeroVector); Camera->Zoom(30000); }
  Later(1); return;
 }
 case 2:
 {
  if(!Prefix.IsEmpty())
  {
   int32 W,H; PC->GetViewportSize(W,H); int32 EW=1280,EH=720;
   FParse::Value(FCommandLine::Get(),TEXT("ResX="),EW); FParse::Value(FCommandLine::Get(),TEXT("ResY="),EH);
   Check(W==EW && H==EH,TEXT("rendered wide viewport matches requested dimensions"));
   int32 Far=0; if(VAT) for(const auto& B:Bodies) Far+=VAT->IsFar(B.Key);
   int32 DisplayLOD=1; FParse::Value(FCommandLine::Get(),TEXT("BattleDisplayLOD="),DisplayLOD);
   Check(DisplayLOD==0?!VAT:VAT && Far>0,TEXT("wide VAT setting controls distant Mass presentation"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WIDE_VAT far=%d actors=%d mass=%d"),Far,Mode->Units.Num(),Bodies.Num()-Mode->Units.Num());
   FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-rts.png"),true,false);
  }
  Later(1); return;
 }
 case 3:
  Check(PC->ControlUnit(Actor(1)),TEXT("delegated mixed squad leader remains directly controllable"));
  Check(Store->ConsumeShot(1,GetWorld()->GetTimeSeconds(),.1) && Store->StartReload(1,GetWorld()->GetTimeSeconds()),TEXT("direct leader can start reloading while AI is paused"));
  Later(GetDefault<UBattleTuning>()->ReloadTime(Store->Find(1)->Weapon)+.3f); return;
 case 4:
  Check(Store->Find(1)->ReloadEndTime==0 && Store->Find(1)->Ammo==GetDefault<UBattleTuning>()->Capacity(Store->Find(1)->Weapon),TEXT("direct leader reload completes while P pauses AI"));
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-tps.png"),true,false);
  Later(1); return;
 case 5:
 {
  Store->ApplyDamage(1,1,100000);
  int32 Alive=0; for(int32 ID:Store->FindSquad(1)->Members) Alive+=Store->Find(ID)->IsAlive();
  Check(Alive==0 && PC->IsTactical() && !Store->FindSquad(1)->bPlayerDelegated && Store->FindSquad(1)->CommandStatus==EBattleCommandStatus::Disabled,TEXT("leader loss stops entire mixed squad and returns player to RTS"));
  Check(!IssueOrder(1,EBattleOrder::Move,FVector::ZeroVector) && !Match->SetAllyDelegated(1,true),TEXT("disabled squad rejects commands and delegation"));
  Check(Summaries().Num()==25 && !Summaries()[0].bActive && Store->FindSquad(2)->CommandStatus==EBattleCommandStatus::Active,TEXT("stopped squad remains in list without disabling others"));
  Match->Score[0]=99; Match->Objectives[0].Owner=0; Mode->bCombatPaused=false; Match->UpdateObjectives(1);
  Check(Match->Phase==EBattleMatchPhase::Finished && Match->Winner==0 && Mode->bBattleEnded,TEXT("wide score victory ends the integrated match"));
  Check(!IssueOrder(2,EBattleOrder::Move,FVector::ZeroVector) && !CanControlLeader(11),TEXT("finished match rejects new orders and direct control"));
  Match->Phase=EBattleMatchPhase::Combat; Mode->bBattleEnded=false; Match->Score[0]=Match->Score[1]=0; Match->Elapsed=180; Match->CheckTimeLimit();
  Check(Match->Phase==EBattleMatchPhase::Finished && Match->Winner==INDEX_NONE,TEXT("wide match time limit produces a draw at equal score"));
  Match->Phase=EBattleMatchPhase::Combat; Mode->bBattleEnded=false; Match->Elapsed=0;
  for(ABattleUnit* Unit:Mode->Units) if(Unit->GetState()->Team==1) Store->ApplyDamage(Unit->GetUnitID(),0,100000);
  Step(.1f);
  Check(Match->Phase==EBattleMatchPhase::Finished && Match->Winner==0,TEXT("elimination counts both Actor and Mass casualties"));
  WideRestartExpected=true; WidePreviousFailures=SmokeFailures;
  auto* View=GetWorld()->GetGameViewport()->Viewport;
  PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::F5,IE_Pressed,FPlatformTime::Cycles64()));
  PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::F5,IE_Released,FPlatformTime::Cycles64()));
  return;
 }
 }
}
