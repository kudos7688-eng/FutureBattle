#include "BattleLeaderRuntime.h"
#include "BattleSquadTactics.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"
#include "Components/CapsuleComponent.h"

void UBattleLeaderRuntime::RunST2Smoke()
{
 auto Check=[&](bool OK,const TCHAR* Text){ if(!OK) ++SmokeFailures; UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ST2_TEST %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Text); };
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController()); auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto S=[&](){return Store->FindSquad(1);};
 auto Later=[&](float Delay){FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunST2Smoke,Delay,false);};
 FString Prefix; FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 if(WideStage==0)
 {
  Mode->CombatStartsAt=0; Mode->bCombatPaused=false; PC->EnterTactical(); PC->SetSelection(Actor(1));
  auto O=T->Observe(*S()); const FVector Here=Position(1);
  FBattleContact Contact; Contact.TargetID=251; Contact.LastKnownPosition=Here+FVector(1500,0,0); Contact.Confidence=1; Contact.bVisible=true; Contact.LastSeenTime=GetWorld()->GetTimeSeconds();
  O.Contacts.Add(251,Contact);
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Mission,TEXT("manual squad keeps command authority"));
  PC->DelegateSelected();
  if(!T->IsST2Enabled())
  {
   O.RecentDamage=1000; T->Evaluate(1,O,1);
   Check(S()->TacticalPlan.Action==EBattleSquadTactic::Mission,TEXT("ST2 OFF preserves ST1 mission-only behavior"));
   WideStage=3; Later(.1f); return;
  }
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Engage && S()->Order==EBattleOrder::Hold,TEXT("advantageous contact holds firing position instead of retreating"));
  O.Ready=0; T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Engage,TEXT("temporary reload with reserve is not ammunition exhaustion"));
  O.Ready=9; O.RecentDamage=400; T->Evaluate(1,O,1);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Retreat && S()->Order==EBattleOrder::Move,TEXT("recent damage selects reachable retreat overriding combat stop"));
  Check(S()->TacticalPlan.Destination.X<Here.X-400,TEXT("retreat endpoint increases separation from known threat"));
  if(bStableMovement)
  {
   Plans.FindChecked(1).NextPath=0; Think(1,GetWorld()->GetTimeSeconds());
   const auto& Goals=Plans.FindChecked(1).ArrivalGoals;
   Check(Goals.Contains(1) && FVector::Dist2D(Goals.FindChecked(1),S()->Destination)<=250,TEXT("stable retreat reserves leader arrival inside ST2 leader radius"));
   bool Cohesive=Goals.Num()==S()->Members.Num();
   for(const auto& Goal:Goals) Cohesive&=FVector::Dist2D(Goal.Value,S()->Destination)<=450;
   Check(Cohesive,TEXT("stable retreat reserves every follower inside ST2 regroup radius"));
  }
  const FVector Retreat=S()->Destination;
  IssueOrder(1,EBattleOrder::AttackMove,FVector(-7000,-5600,0),true);
  Check(S()->Destination==Retreat && S()->MissionDestination!=Retreat,TEXT("director updates mission without overwriting retreat execution"));
  Check(!IssueOrder(1,EBattleOrder::Move,FVector(50000,50000,0)) && S()->TacticalPlan.Action==EBattleSquadTactic::Retreat && S()->bPlayerDelegated,TEXT("unreachable manual command retains retreat and delegation"));
  const float Age=S()->TacticalPlan.Age; const double Clock=T->Clock(); Mode->bCombatPaused=true; T->Step(2);
  Check(T->Clock()==Clock && S()->TacticalPlan.Age==Age,TEXT("P freezes tactical timers")); Mode->bCombatPaused=false;
  PC->ControlUnit(Actor(1)); auto Direct=O; Direct.Direct=1; T->Evaluate(1,Direct,8);
  Check(PC->GetPawn()==Actor(1) && S()->TacticalPlan.Age==Age,TEXT("direct control suspends autonomous transitions without losing retreat")); PC->EnterTactical();
  PC->TakeCommandSelected(); const int32 Rev=S()->OrderRevision; T->Evaluate(1,O,10);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Mission && S()->OrderRevision==Rev,TEXT("K cancels tactical authority and preserves executing order"));
  PC->HoldSelected(); PC->DelegateSelected();
  O.RecentDamage=0; O.Armed=0; O.Ammo=O.Reserve=0; T->Evaluate(1,O,1);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Retreat,TEXT("exhausted ammunition under threat triggers retreat"));
  T->Evaluate(1,O,20);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Regroup,TEXT("retreat timeout regroups instead of resuming attack"));
  O.Contacts.Reset(); T->Evaluate(1,O,2);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Regroup,TEXT("regroup does not immediately counterattack"));
  const auto Before=*Store->Find(1); T->Evaluate(1,O,5);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Recover && S()->Order==EBattleOrder::Defend,TEXT("irrecoverable ammunition loss adopts defensive holding role"));
  Check(Store->Find(1)->HP==Before.HP && Store->Find(1)->Ammo==Before.Ammo && Store->Find(1)->Reserve==Before.Reserve,TEXT("regroup never heals or creates ammunition"));
  IssueOrder(1,EBattleOrder::AttackMove,FVector(-7000,-5600,0),true);
  O=T->Observe(*S()); O.Contacts.Reset(); T->Evaluate(1,O,6);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Mission && S()->Order==EBattleOrder::AttackMove,TEXT("safe and combat-capable squad resumes latest mission"));
  // Navigation failure fixture: start outside the existing NavMesh, then restore the real position.
  Actor(1)->SetActorLocation(FVector(-50000,-50000,100)); O.Contacts.Add(251,Contact); O.Contacts[251].LastKnownPosition=Position(1)+FVector(1000,0,0); O.RecentDamage=400;
  T->Evaluate(1,O,1);
  Check(S()->TacticalPlan.Action==EBattleSquadTactic::Regroup && S()->TacticalPlan.Reason.Contains(TEXT("NO SAFER")),TEXT("no reachable retreat candidate holds with explicit reason"));
  const int32 FailedRevision=S()->OrderRevision; T->Evaluate(1,O,1);
  Check(S()->OrderRevision==FailedRevision,TEXT("failed route search respects retry cooldown")); Actor(1)->SetActorLocation(Here);
  PC->HoldSelected(); PC->DelegateSelected(); O=T->Observe(*S());
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Mission,TEXT("unseen enemies do not trigger tactical decisions"));
  O.Contacts.Add(251,Contact); O.RecentDamage=400; T->Evaluate(1,O,1); const FVector KnownGoal=S()->Destination;
  PC->HoldSelected(); PC->DelegateSelected(); const FVector Enemy=Position(251); Actor(251)->SetActorLocation(FVector(6000,5000,100)); T->Evaluate(1,O,1);
  Check(S()->Destination.Equals(KnownGoal,1),TEXT("identical observations ignore hidden enemy true-position changes")); Actor(251)->SetActorLocation(Enemy);
  // Additional boundaries use declared observation fixtures, without reading enemy truth.
  PC->HoldSelected(); PC->DelegateSelected(); O=T->Observe(*S()); O.Contacts.Add(251,Contact); O.Health=300;
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Retreat,TEXT("low surviving-member health triggers retreat"));
  PC->HoldSelected(); PC->DelegateSelected(); O=T->Observe(*S()); O.Contacts.Add(251,Contact); O.Living=3; O.Armed=3; O.Health=300;
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Retreat,TEXT("severe member losses trigger retreat without requiring recent damage"));
  PC->HoldSelected(); PC->DelegateSelected(); O=T->Observe(*S());
  for(int32 I=0;I<25;++I) { auto C=Contact; C.TargetID=251+I; O.Contacts.Add(C.TargetID,C); }
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Retreat,TEXT("observed numerical disadvantage triggers retreat"));
  auto EnemyO=T->Observe(*Store->FindSquad(26)); auto EC=Contact; EC.TargetID=1; EC.LastKnownPosition=Position(251)-FVector(1500,0,0); EnemyO.Contacts.Add(1,EC); EnemyO.RecentDamage=400;
  T->Evaluate(26,EnemyO,1); Check(Store->FindSquad(26)->TacticalPlan.Action==EBattleSquadTactic::Retreat && Store->FindSquad(26)->Destination.X>Position(251).X+400,TEXT("enemy uses the same retreat rule with mirrored threat direction"));
  IssueOrder(26,EBattleOrder::Hold,Position(251));
  PC->HoldSelected(); PC->DelegateSelected(); O=T->Observe(*S()); O.Contacts.Add(251,Contact); O.RecentDamage=400; T->Evaluate(1,O,1);
  const FVector OldGoal=S()->Destination; O.Contacts[251].LastKnownPosition=OldGoal; T->Evaluate(1,O,6);
  Check(!S()->Destination.Equals(OldGoal,100) || S()->TacticalPlan.Action==EBattleSquadTactic::Regroup,TEXT("new known danger at retreat endpoint replans or holds safely"));
  PC->HoldSelected(); PC->DelegateSelected(); O=T->Observe(*S()); O.Contacts.Add(251,Contact); O.RecentDamage=400; T->Evaluate(1,O,1);
  // Actual world ticks must execute the decision on both representations.
  WideStart=Position(2); SmokePosition=Position(1); WideStage=1; Later(4); return;
 }
 if(WideStage==1)
 {
  Mode->bCombatPaused=true;
  Check(Position(1).X<SmokePosition.X-50 && Position(2).X<WideStart.X-50,TEXT("live Actor and Mass movement executes retreat away from known threat"));
  Check(Bodies.Num()==500 && Mode->Units.Num()==50,TEXT("ST2 retains exactly 50 Actor and 450 Mass representations"));
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())) {Camera->SetActorLocation(Position(1));Camera->Zoom(8000);}
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-retreat.png"),true,false);
  WideStage=2; Later(1); return;
 }
 if(WideStage==2)
 {
  Mode->bCombatPaused=false;
  auto O=T->Observe(*S()); O.Contacts.Reset(); O.RecentDamage=0;
  auto PlaceFollowers=[&](bool Gathered)
  {
   for(int32 ID:S()->Members) if(ID!=S()->LeaderUnitID)
   {
    auto& B=Bodies.FindChecked(ID); const FVector P=Position(1)+FVector(Gathered?float((ID%3)*90):1500.f,float((ID%4)*70),0);
    B.Capsule->SetWorldLocation(P); Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=P;
   }
  };
  // Current-position goal exercises arrival/cohesion through the production evaluator.
  PlaceFollowers(false);
  T->Apply(1,EBattleSquadTactic::Retreat,EBattleOrder::Move,Position(1),TEXT("ARRIVAL FIXTURE"));
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Retreat,TEXT("leader arrival alone cannot complete regroup while members lag"));
  PlaceFollowers(true);
  T->Evaluate(1,O,1); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Regroup,TEXT("arrival and member cohesion transition into regroup"));
  T->Evaluate(1,O,6); Check(S()->TacticalPlan.Action==EBattleSquadTactic::Mission,TEXT("regroup safe interval returns capable force to mission"));
  PC->HoldSelected(); Check(!S()->bPlayerDelegated && S()->TacticalPlan.Action==EBattleSquadTactic::Mission,TEXT("accepted H stops autonomous execution"));
  PC->DelegateSelected(); O.Contacts.Add(251,FBattleContact()); O.Contacts[251].LastKnownPosition=Position(1)+FVector(1000,0,0); O.RecentDamage=400; T->Evaluate(1,O,1);
  Store->ApplyDamage(1,1,100000);
  Check(S()->CommandStatus==EBattleCommandStatus::Disabled && S()->TacticalPlan.Action==EBattleSquadTactic::Mission,TEXT("leader death cancels autonomous plan and disables squad"));
  Mode->bCombatPaused=true; WideStage=3; Later(.1f); return;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ST2_RESULT failures=%d"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
