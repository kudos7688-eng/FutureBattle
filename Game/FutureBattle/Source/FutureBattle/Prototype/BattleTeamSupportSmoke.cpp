#include "BattleLeaderRuntime.h"
#include "BattleSquadTactics.h"
#include "BattleTeamAI.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunSupportSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK) ++SmokeFailures; UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPORT_TEST %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 auto* Team=GetWorld()->GetSubsystem<UBattleTeamAI>(); auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSupportSmoke,Delay,false);};
 // Fixture-only resource injection; production code uses the authoritative state API.
 auto Resource=[&](int32 ID)->FBattleUnitState& {return *const_cast<FBattleUnitState*>(Store->Find(ID));};
 auto Plan=[&](int32 SID)->const FBattleSquadTacticalPlan& {return Store->FindSquad(SID)->TacticalPlan;};
 auto Place=[&](int32 ID,FVector Pos)
 {
  auto& B=Bodies.FindChecked(ID);
  if(B.Actor.IsValid()) {B.Actor->SetActorLocation(Pos);B.Actor->SetActorRotation(FRotator(0,90,0));}
  else {B.Capsule->SetWorldLocation(Pos);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=Pos;F.Forward=FVector(0,1,0);}
 };
 auto Reports=[&](){for(int32 SID:{1,2,3}) T->Samples.Add(SID,T->Observe(*Store->FindSquad(SID)));};
 auto Setup=[&]()
 {
  T->Reset();
  for(int32 SID:{1,2,3}) {IssueOrder(SID,EBattleOrder::Hold,Position(Store->FindSquad(SID)->LeaderUnitID));Store->SetSquadDelegated(SID,true);}
  T->Apply(1,EBattleSquadTactic::Retreat,EBattleOrder::Move,Position(1)-FVector(0,2500,0),TEXT("RETREAT WITHOUT WAITING FOR TEAM"));
  Reports();
 };
 if(WideStage==0)
 {
  Mode->CombatStartsAt=0; Mode->bCombatPaused=false; PC->EnterTactical();
  for(const auto& Pair:Store->GetUnits()) Place(Pair.Key,FVector(8000+(Pair.Key%20)*120,-7000+(Pair.Key/20)*120,100));
  for(int32 SID:{1,2,3})
   for(int32 I=0;I<10;++I) Place((SID-1)*10+I+1,FVector(-9600+(SID-1)*500+(I%5)*130,-2300+(SID-1)*600-(I/5)*130,100));
  Place(251,FVector(-9100,900,100)); Resource(251).HP=100000;
  for(const auto& Pair:Store->GetUnits()) if(Pair.Value.Team==1) {Resource(Pair.Key).Ammo=0;Resource(Pair.Key).Reserve=0;}
  const double Now=GetWorld()->GetTimeSeconds(); Sense->Reset(); Sense->Update(Mode->Units,Now-1); Sense->Update(Mode->Units,Now);
  Check(CanSee(11,251) && Sense->FindContact(11,251) && Sense->FindContact(11,251)->bVisible,TEXT("fixture acquires physical target through real perception"));
  Setup(); Team->Step();
  if(!Team->IsEnabled())
  {
   Check(Plan(2).Action!=EBattleSquadTactic::Support && Plan(1).Action==EBattleSquadTactic::Retreat,TEXT("OFF keeps independent ST2 retreat without support"));
   WideStage=3;Later(.1f);return;
  }
  Check(Plan(2).Action==EBattleSquadTactic::Support && Plan(2).SupportSquad==1,TEXT("team assigns nearest eligible squad to retreat"));
  if(Plan(2).Action!=EBattleSquadTactic::Support) { UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPORT_RESULT failures=%d"),SmokeFailures); FPlatformMisc::RequestExitWithStatus(false,1); return; }
  Check(Plan(3).Action!=EBattleSquadTactic::Support,TEXT("one supporter per retreat, no duplicate reservation"));
  Check(!Plan(2).bSupportConfirmed,TEXT("assignment alone does not claim actual covering fire"));
  Check(Plan(1).Action==EBattleSquadTactic::Retreat && Store->FindSquad(1)->Order==EBattleOrder::Move,TEXT("retreat never waits for support confirmation"));
  Think(2,Now);const int32 Ammo=Store->Find(11)->Ammo;Follow(11,.01f,Now);
  Check(Store->Find(11)->Ammo==Ammo-1 && Plan(2).bSupportConfirmed,TEXT("actual visible line and accepted shot confirm support"));
  const int32 Revision=Plan(2).Revision; const double Clock=T->Clock();Mode->bCombatPaused=true;T->Step(10);Team->Step();
  Check(T->Clock()==Clock && Plan(2).Revision==Revision,TEXT("pause freezes team reservations and squad clocks"));Mode->bCombatPaused=false;
  IssueOrder(2,EBattleOrder::AttackMove,Position(11)+FVector(0,500,0),true);
  Check(Plan(2).Action==EBattleSquadTactic::Support && Store->FindSquad(2)->Order==EBattleOrder::Hold,TEXT("objective mission update cannot overwrite team support execution"));
  PC->SetSelection(Actor(11));PC->TakeCommandSelected();Team->Step();
  Check(Plan(2).Action==EBattleSquadTactic::Mission && !Store->FindSquad(2)->bPlayerDelegated,TEXT("manual takeover cancels support authority"));
  Setup();Team->Step();PC->ControlUnit(Actor(11));Team->Step();
  Check(Plan(2).Action==EBattleSquadTactic::Mission && PC->GetPawn()==Actor(11),TEXT("TPS handoff releases coordination without taking control"));PC->EnterTactical();
  Setup();Team->Step(); auto O=T->Observe(*Store->FindSquad(2));O.RecentDamage=500;T->Evaluate(2,O,1);
  Check(Plan(2).Action==EBattleSquadTactic::Retreat || Plan(2).Action==EBattleSquadTactic::Regroup,TEXT("supporter emergency overrides team assignment"));
  Setup();Team->Step();Check(Plan(2).Action==EBattleSquadTactic::Support,TEXT("deadline fixture starts with an accepted assignment"));T->ActiveSeconds=9;Reports();Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("expired reservation releases and cooldown prevents immediate reassignment"));
  Setup();Team->Step();Check(Plan(2).Action==EBattleSquadTactic::Support,TEXT("no-fire fixture starts with an accepted assignment"));T->ActiveSeconds=4;O=T->Observe(*Store->FindSquad(2));T->Evaluate(2,O,4);Reports();Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("no real fire aborts support within bounded grace period"));
  Setup();Team->Step();Check(Plan(2).Action==EBattleSquadTactic::Support,TEXT("ammunition fixture starts with an accepted assignment"));for(int32 ID:Store->FindSquad(2)->Members) {Resource(ID).Ammo=0;Resource(ID).Reserve=0;}
  T->ActiveSeconds=1;Reports();Team->Step();Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("ammunition exhaustion releases support"));
  for(int32 ID:Store->FindSquad(2)->Members) {Resource(ID).Ammo=30;Resource(ID).Reserve=90;}
  Setup();Team->Step();Check(Plan(2).Action==EBattleSquadTactic::Support,TEXT("contact-loss fixture starts with an accepted assignment"));Sense->Reset();T->ActiveSeconds=1;Reports();Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("lost contact releases support, no firing from memory"));
  Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  Setup();Team->Step();T->Apply(1,EBattleSquadTactic::Regroup,EBattleOrder::Hold,Position(1),TEXT("ARRIVED"));Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("retreat completion releases covering squad"));
  Setup();Team->Step();Check(!T->AcceptSupport(1,2,251),TEXT("retreat/support pair cannot form a circular wait"));
  const FVector Enemy=Position(251);Place(251,FVector(6500,6500,100));
  Check(T->CanSupport(2,251),TEXT("eligibility uses observed position, not hidden target movement"));
  Resource(251).HP=1;Setup();Team->Step();
  Check(Plan(2).Action==EBattleSquadTactic::Support && Plan(2).SupportSquad==1 && Plan(2).SupportTarget==251,TEXT("team assignment is unchanged by unobserved enemy position and health"));
  Resource(251).HP=100000;Place(251,Enemy);
  Setup();Store->SetSquadDelegated(2,false);Store->SetSquadDelegated(3,false);Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support && Plan(3).Action!=EBattleSquadTactic::Support && Plan(1).Action==EBattleSquadTactic::Retreat,TEXT("no eligible partner leaves retreat independent"));
  Setup();T->ActiveSeconds=4;Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("stale squad reports cannot allocate support"));
  Setup();const FVector SupportOrigin=Position(11);Place(11,SupportOrigin+FVector(4000,0,0));Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("distant supporter is not summoned away from its mission"));Place(11,SupportOrigin);
  // The same coordinator runs the opposing team with an actual visible contact.
  T->Reset();for(int32 SID:{1,2,3}) Store->SetSquadDelegated(SID,false);
  TMap<int32,FVector> OwnPositions;for(int32 ID=1;ID<=30;++ID){OwnPositions.Add(ID,Position(ID));Place(ID,FVector(8000+(ID%20)*120,-7000+(ID/20)*120,100));}
  Place(251,FVector(-9600,-2300,100));Place(261,FVector(-9100,-1700,100));Place(1,FVector(-9100,900,100));
  Resource(251).HP=100;Resource(261).Ammo=30;Resource(261).Reserve=90;
  Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  IssueOrder(27,EBattleOrder::Hold,Position(261));T->Apply(26,EBattleSquadTactic::Retreat,EBattleOrder::Move,Position(251)-FVector(0,2000,0),TEXT("ENEMY RETREAT"));
  T->Samples.Add(26,T->Observe(*Store->FindSquad(26)));T->Samples.Add(27,T->Observe(*Store->FindSquad(27)));Team->Step();
  Check(Plan(27).Action==EBattleSquadTactic::Support && Plan(27).SupportSquad==26,TEXT("opposing team uses the same coordination and acceptance rules"));
  IssueOrder(26,EBattleOrder::Hold,Position(251));IssueOrder(27,EBattleOrder::Hold,Position(261));Resource(261).Ammo=Resource(261).Reserve=0;
  for(const auto& Pair:OwnPositions) Place(Pair.Key,Pair.Value);Place(251,FVector(-9100,900,100));Resource(251).HP=100000;Place(261,FVector(10000,-5000,100));
  Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  // Mass targets must not be mistaken for static terrain in the acceptance trace.
  Place(251,FVector(10000,-5000,100));Place(252,FVector(-9100,900,100));Resource(252).HP=100000;
  Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  Setup();Team->Step();
  Check(Plan(2).Action==EBattleSquadTactic::Support && Plan(2).SupportTarget==252,TEXT("Mass target permits support without treating its capsule as terrain"));
  // Production world ticks now execute both roles; no synthetic shot event is injected.
  Setup();Team->Step();SmokePosition=Position(1);WideStart=Position(2);
  PC->SetSelection(Actor(11));if(auto* C=Cast<ABattleCameraPawn>(PC->GetPawn())){C->SetActorLocation(Position(11));C->Zoom(7000);}
  WideStage=1;Later(2);return;
 }
 if(WideStage==1)
 {
  Check(Plan(2).Action==EBattleSquadTactic::Support && Plan(2).bSupportConfirmed,TEXT("live ticks sustain actual support fire"));
  Check(Position(1).Y<SmokePosition.Y-50 && Position(2).Y<WideStart.Y-50,TEXT("live Actor and Mass retreat concurrently with support"));
  Check(Bodies.Num()==500 && Mode->Units.Num()==50,TEXT("support preserves 50 Actor and 450 Mass representations"));
  Mode->bCombatPaused=true;FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-support.png"),true,false);
  WideStage=2;Later(.5f);return;
 }
 if(WideStage==2)
 {
  Mode->bCombatPaused=false;Store->ApplyDamage(1,1,100000);Team->Step();
  Check(Plan(2).Action!=EBattleSquadTactic::Support,TEXT("retreat leader death releases partner"));
  Mode->bCombatPaused=true;WideStage=3;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPORT_RESULT failures=%d"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
