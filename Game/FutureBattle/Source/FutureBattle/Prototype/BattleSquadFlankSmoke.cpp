#include "BattleLeaderRuntime.h"
#include "BattleSquadTactics.h"
#include "BattleTeamAI.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunFlankSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK) ++SmokeFailures; UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_TEST %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();auto* Team=GetWorld()->GetSubsystem<UBattleTeamAI>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunFlankSmoke,Delay,false);};
 auto Resource=[&](int32 ID)->FBattleUnitState& {return *const_cast<FBattleUnitState*>(Store->Find(ID));};
 auto Plan=[&](int32 SID)->FBattleSquadTacticalPlan& {return const_cast<FBattleSquadState*>(Store->FindSquad(SID))->TacticalPlan;};
 auto Place=[&](int32 ID,FVector Pos){
  auto& B=Bodies.FindChecked(ID);
  if(B.Actor.IsValid()){B.Actor->SetActorLocation(Pos,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->SetActorRotation(FRotator::ZeroRotator);}
  else{B.Capsule->SetWorldLocation(Pos);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=Pos;F.Forward=FVector::ForwardVector;}
 };
 auto Acquire=[&](){const double Now=GetWorld()->GetTimeSeconds();Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);};
 auto Setup=[&](){
  T->Reset();
  for(int32 SID:{1,2}){IssueOrder(SID,EBattleOrder::Hold,Position(Store->FindSquad(SID)->LeaderUnitID));Store->SetSquadDelegated(SID,true);const_cast<FBattleSquadState*>(Store->FindSquad(SID))->FlankCooldown=0;}
  T->Apply(1,EBattleSquadTactic::Engage,EBattleOrder::Hold,Position(1),TEXT("FLANK FIXTURE"));
  // The production coordinator receives squad reports before assigning support.
  for(int32 SID:{1,2})T->Samples.Add(SID,T->Observe(*Store->FindSquad(SID)));
 };
 if(WideStage==0){
  Mode->CombatStartsAt=0;Mode->bCombatPaused=false;PC->EnterTactical();
  // Isolate local tactics from objective reassignment; movement/perception still tick normally.
  auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();Match->DirectorClock=-1000;Match->DelegationClock=-1000;
  for(const auto& Pair:Store->GetUnits()){
   Place(Pair.Key,FVector(7000+(Pair.Key%20)*120,3000+(Pair.Key/20)*120,100));
   auto P=Pair.Value.Perception;P.SightRange=0;P.ShareRange=0;Store->SetPerceptionProfile(Pair.Key,P);
   if(Pair.Value.Team==1){Resource(Pair.Key).Ammo=0;Resource(Pair.Key).Reserve=0;}
  }
  for(const auto& Pair:Store->GetSquads()) IssueOrder(Pair.Key,EBattleOrder::Hold,Position(Pair.Value.LeaderUnitID));
  for(int32 SID:{1,2})for(int32 I=0;I<10;++I){
   const int32 ID=(SID-1)*10+I+1;Place(ID,FVector(-9000-(I/5)*120,-7000-(SID-1)*500+(I%5)*80,100));
   Store->SetTrainingPreset(ID,EBattleTraining::Regular);auto P=Resource(ID).Perception;P.ShareRange=0;Store->SetPerceptionProfile(ID,P);
  }
  Place(251,FVector(-6000,-7000,100));Resource(251).HP=100000;Acquire();Setup();
  Check(Sense->FindContact(1,251) && Sense->FindContact(1,251)->bVisible,TEXT("fixture acquires target with actual perception"));
  if(!T->IsFlankEnabled())
  {
   Check(!T->TryFlank(1,2) && Plan(1).Action==EBattleSquadTactic::Engage,TEXT("OFF preserves engagement without flank assignment"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_RESULT failures=%d"),SmokeFailures);
   FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);return;
  }
  auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-7800,-6050,250),FRotator::ZeroRotator);
  Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
  Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Wall->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
  Wall->SetActorScale3D(FVector(2,9,5));Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));Wall->SetCanBeDamaged(false);
  const bool Solo=T->TryFlank(1,INDEX_NONE);
  Check(Solo && Plan(1).FlankSupport==INDEX_NONE,TEXT("covered route permits solo flank without reserving a supporter"));
  Check(!Solo || (Plan(1).FlankExposure<=.65 && Store->FindSquad(1)->Order==EBattleOrder::Move),TEXT("solo route has bounded exposure and does not stop to shoot"));
  const FVector ObservedGoal=Plan(1).Destination;
  Place(251,FVector(10000,8000,100));Resource(251).HP=1;Setup();
  Check(T->TryFlank(1,INDEX_NONE) && Plan(1).Destination.Equals(ObservedGoal,1),TEXT("unobserved enemy movement and health do not change flank decision"));
  Place(251,FVector(-6000,-7000,100));Resource(251).HP=100000;
  Check(!T->TryFlank(1,INDEX_NONE),TEXT("active flank and cooldown prevent duplicate replanning"));
  if(!Team->IsEnabled())
  {
   Setup();for(int32 SID:{1,2})T->Samples.Add(SID,T->Observe(*Store->FindSquad(SID)));Team->Step();
   Check(Plan(1).Action==EBattleSquadTactic::Flank && Plan(1).FlankSupport==INDEX_NONE,TEXT("team coordinator permits covered solo flank with team support OFF"));
   Wall->Destroy();UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_RESULT failures=%d"),SmokeFailures);
   FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);return;
  }
  Wall->Destroy();Setup();
  Check(!T->TryFlank(1,INDEX_NONE),TEXT("exposed unsupported route is rejected"));
  Setup();Mode->bCombatPaused=true;Check(!T->TryFlank(1,2),TEXT("paused combat cannot accept a flank"));Mode->bCombatPaused=false;
  Setup();for(int32 ID:Store->FindSquad(1)->Members)Resource(ID).Ammo=0;
  Check(!T->TryFlank(1,2),TEXT("unready squad cannot accept a flank even with support"));
  for(int32 ID:Store->FindSquad(1)->Members)Resource(ID).Ammo=GetDefault<UBattleTuning>()->Capacity(Resource(ID).Weapon);
  auto Supported=[&](){Setup();const bool OK=T->TryFlank(1,2);return OK && T->AcceptSupport(2,1,Plan(1).FlankTarget);};
  Check(Supported() && Plan(1).FlankSupport==2,TEXT("exposed route requires eligible same-team covering squad"));
  if(Plan(1).Action!=EBattleSquadTactic::Flank){UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Check(!Plan(2).bSupportConfirmed,TEXT("flank assignment does not fabricate covering shots"));
  const double Now=GetWorld()->GetTimeSeconds();Think(2,Now);Follow(11,.01f,Now);
  Check(Plan(2).bSupportConfirmed,TEXT("shared execution confirms real support fire for flank"));
  const FVector SparePosition=Position(252); const double Deadline=Plan(2).SupportUntil,LastShot=Plan(2).LastSupportShot;
  const FVector Route=Plan(1).Destination;
  Place(251,FVector(10000,8000,100));Place(252,FVector(-6000,-5500,100));Acquire();
  Check(!T->RefreshFlankTarget(1),TEXT("support cannot switch to a different engagement outside ten metres"));
  Place(252,FVector(-6000,-6800,100));Acquire();
  Check(T->RefreshFlankTarget(1) && Plan(1).FlankTarget==252 && Plan(2).SupportTarget==252,TEXT("lost target switches to locally observed shootable contact"));
  Check(Plan(1).Destination.Equals(Route) && Plan(2).SupportUntil==Deadline && Plan(2).LastSupportShot==LastShot,TEXT("retarget preserves route deadline and actual shot history"));
  Sense->Reset();Check(!T->RefreshFlankTarget(1),TEXT("retarget cannot use unobserved enemy bodies"));
  Place(252,SparePosition);Place(251,FVector(-6000,-7000,100));Acquire();
  Check(Supported(),TEXT("support loss scenario restores original target"));
  Store->SetSquadDelegated(2,false);T->EvaluateFlank(1,T->Observe(*Store->FindSquad(1)));
  Check(Plan(1).Action==EBattleSquadTactic::Engage,TEXT("loss of required support aborts exposed route"));
  Check(Supported(),TEXT("relaxed support-loss fixture starts under strict policy"));
  const FVector OriginalGoal=Plan(1).Destination;const float OriginalAge=Plan(1).Age;
  T->bRelaxedFlank=true;
  auto Risk=T->Observe(*Store->FindSquad(1));
  Check(T->SoloFlankRisk(Risk,Position(1),1500,1),TEXT("relaxed policy allows short exposed movement without support"));
  Check(!T->SoloFlankRisk(Risk,Position(1),3100,.1),TEXT("relaxed policy retains bounded solo path length"));
  Risk.RecentDamage=Risk.Living*GetDefault<UBattleTuning>()->Health*.2f;
  Check(!T->SoloFlankRisk(Risk,Position(1),1500,1),TEXT("recent heavy damage rejects unsupported continuation"));
  Store->SetSquadDelegated(2,false);T->EvaluateFlank(1,T->Observe(*Store->FindSquad(1)));
  Check(Plan(1).Action==EBattleSquadTactic::Flank && Plan(1).FlankSupport==INDEX_NONE,TEXT("support loss reassesses remaining route and continues solo"));
  Check(Plan(1).Destination.Equals(OriginalGoal)&&Plan(1).Age==OriginalAge,TEXT("solo continuation preserves destination and deadline"));
  Sense->Reset();Check(!T->ReassessSoloFlank(1,T->Observe(*Store->FindSquad(1))),TEXT("solo reassessment cannot inspect unobserved enemy truth"));Acquire();
  T->bRelaxedFlank=false;Setup();T->FlankTeams=2;
  Check(!T->TryFlank(1,2)&&!T->IsFlankEnabledFor(0)&&T->IsFlankEnabledFor(1),TEXT("team mask disables only assigned faction including direct entry"));
  T->FlankTeams=3;
  T->bCoordination=true;
  Check(Supported(),TEXT("coordination fixture starts with accepted flank"));
  Resource(11).NextShotTime=0;Think(2,Now);Follow(11,.01f,Now);
  Check(Plan(2).bSupportConfirmed,TEXT("grace period requires a real support shot"));
  for(int32 ID:Store->FindSquad(2)->Members)Resource(ID).ReloadEndTime=Now+2;
  Check(!T->CanSupport(2,251)&&T->CanMaintainSupport(2,251),TEXT("short reload retains recently confirmed support"));
  Plan(2).LastSupportShot=T->Clock()-4;
  Check(!T->CanMaintainSupport(2,251),TEXT("support grace expires after actual shot gap"));
  for(int32 ID:Store->FindSquad(2)->Members)Resource(ID).ReloadEndTime=0;
  TArray<FVector> SparePositions;for(int32 ID:Store->FindSquad(3)->Members){SparePositions.Add(Position(ID));Place(ID,FVector(-9000,-8000+(ID-21)*80,100));Store->SetTrainingPreset(ID,EBattleTraining::Regular);}
  IssueOrder(3,EBattleOrder::Hold,Position(21));Store->SetSquadDelegated(3,true);Acquire();
  Check(Supported(),TEXT("replacement fixture starts with original supporter"));
  const FVector BeforeGoal=Plan(1).Destination;Plan(1).Age=7;
  Store->SetSquadDelegated(2,false);
  T->EvaluateFlank(1,T->Observe(*Store->FindSquad(1)));
  Check(Plan(1).Action==EBattleSquadTactic::Flank&&Plan(1).FlankSupport==3&&Plan(3).Action==EBattleSquadTactic::Support,TEXT("eligible observed nearby squad replaces lost support"));
  Check(Plan(1).FlankSupportChanges==1&&Plan(1).Age==7&&Plan(1).Destination.Equals(BeforeGoal)&&Plan(3).SupportUntil<=T->Clock()+13.01,TEXT("replacement preserves flank route and original deadline"));
  Check(!Plan(3).bSupportConfirmed&&!T->TryReplaceFlankSupport(1),TEXT("replacement neither fabricates shots nor permits repeated handoff"));
  T->ReleaseSupport(3,TEXT("FIXTURE END"));Store->SetSquadDelegated(3,false);
  for(int32 ID:Store->FindSquad(3)->Members){Place(ID,SparePositions[ID-21]);auto P=Resource(ID).Perception;P.SightRange=0;P.ShareRange=0;Store->SetPerceptionProfile(ID,P);}
  IssueOrder(3,EBattleOrder::Hold,Position(21));Acquire();T->bCoordination=false;
  Check(Supported(),TEXT("timeout scenario starts with accepted flank"));
  Plan(1).Age=20;T->EvaluateFlank(1,T->Observe(*Store->FindSquad(1)));
  Check(Plan(1).Action==EBattleSquadTactic::Engage && Store->FindSquad(1)->FlankCooldown>0,TEXT("timeout releases flank with retry cooldown"));
  Check(Supported(),TEXT("contact-loss scenario starts with accepted flank"));
  Sense->Reset();T->EvaluateFlank(1,T->Observe(*Store->FindSquad(1)));
  Check(Plan(1).Action==EBattleSquadTactic::Engage,TEXT("missing known contact aborts without enemy truth lookup"));Acquire();
  Check(Supported(),TEXT("emergency scenario starts with accepted flank"));
  auto O=T->Observe(*Store->FindSquad(1));O.RecentDamage=500;T->Evaluate(1,O,1);
  Check(Plan(1).Action==EBattleSquadTactic::Retreat || Plan(1).Action==EBattleSquadTactic::Regroup,TEXT("local emergency overrides flanking assignment"));
  Check(Supported(),TEXT("manual takeover scenario starts with accepted flank"));
  PC->SetSelection(Actor(1));PC->TakeCommandSelected();Team->Step();
  Check(!Store->FindSquad(1)->bPlayerDelegated && Plan(1).Action==EBattleSquadTactic::Mission && Plan(2).Action!=EBattleSquadTactic::Support,TEXT("manual takeover cancels flank and partner coordination"));
  Check(Supported(),TEXT("TPS takeover scenario starts with accepted flank"));
  PC->ControlUnit(Actor(1));Team->Step();
  Check(PC->GetPawn()==Actor(1) && Plan(1).Action==EBattleSquadTactic::Mission && Plan(2).Action!=EBattleSquadTactic::Support,TEXT("TPS handoff releases flank coordination without taking control"));PC->EnterTactical();
  Check(Supported(),TEXT("live movement scenario starts with accepted flank"));
  SmokePosition=Position(1);WideStart=Position(2);FlankSmokeGoal=Plan(1).Destination;
  PC->SetSelection(Actor(1));if(auto* C=Cast<ABattleCameraPawn>(PC->GetPawn())){C->SetActorLocation(Position(1));C->Zoom(7000);}
  WideStage=1;Later(2);return;
 }
 if(WideStage==1){
  Check(FVector::Dist2D(Position(1),SmokePosition)>100 && FVector::Dist2D(Position(2),WideStart)>100,TEXT("live Actor and Mass advance during support"));
  Check(Plan(2).bSupportConfirmed,TEXT("live support remains confirmed while flank advances"));
  Check(Bodies.Num()==500 && Mode->Units.Num()==50,TEXT("flank preserves mixed representation counts"));
  Mode->bCombatPaused=true;const float Age=Plan(1).Age;T->Step(10);Check(Plan(1).Age==Age,TEXT("pause freezes flank deadline"));
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-flank.png"),true,false);
  WideStage=2;Later(.5f);return;
 }
 if(WideStage==2){Mode->bCombatPaused=false;WideStage=3;Later(12);return;}
 int32 Arrived=0;for(int32 ID:Store->FindSquad(1)->Members)Arrived+=FVector::Dist2D(Position(ID),FlankSmokeGoal)<500;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_ARRIVAL action=%s age=%.2f leader_cm=%.2f nearby=%d goal=%s position=%s"),BattleSquadTacticName(Plan(1).Action),Plan(1).Age,FVector::Dist2D(Position(1),FlankSmokeGoal),Arrived,*FlankSmokeGoal.ToCompactString(),*Position(1).ToCompactString());
 Check(Plan(1).Action==EBattleSquadTactic::Engage && FVector::Dist2D(Position(1),FlankSmokeGoal)<300 && Arrived>=7,TEXT("live flank reaches goal and returns to engagement"));
 Place(1,FVector(-9000,-7000,100));Acquire();Setup();
 const bool DeathPlan=T->TryFlank(1,2) && T->AcceptSupport(2,1,Plan(1).FlankTarget);
 Check(DeathPlan,TEXT("leader-death scenario starts with active flank and partner"));
 Mode->bCombatPaused=false;Store->ApplyDamage(1,1,100000);Team->Step();
 Check(Store->FindSquad(1)->CommandStatus!=EBattleCommandStatus::Active && Plan(2).Action!=EBattleSquadTactic::Support,TEXT("leader death cancels flank and releases partner"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_RESULT failures=%d"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}

