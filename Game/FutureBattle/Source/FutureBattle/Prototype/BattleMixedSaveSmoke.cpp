#include "BattleLeaderRuntime.h"
#include "BattleSave.h"
#include "BattleMatch.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleSquadTactics.h"
#include "BattleTeamAI.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunMixedSaveSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK)++SmokeFailures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_SAVE_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMixedSaveSmoke,Delay,false);};
 auto Resource=[&](int32 ID)->FBattleUnitState& {return *const_cast<FBattleUnitState*>(Store->Find(ID));};
 auto Plan=[&](int32 SID)->FBattleSquadTacticalPlan& {return const_cast<FBattleSquadState*>(Store->FindSquad(SID))->TacticalPlan;};
 const double Now=GetWorld()->GetTimeSeconds();
 // Rifle squad size N (8 since 2026-11-19): squad k has unit IDs (k-1)*N+1..k*N, leader first; team 1 starts at squad 26.
 const int32 N=BattleSquadType::RifleMembers(),Total=50*N,Enemy=25*N+1,LastLeader=49*N+1,Follower=N;
 if(SmokeStage==0)
 {
  Mode->bCombatPaused=true;Mode->CombatStartsAt=0;PC->EnterTactical();T->Reset();T->ActiveSeconds=10;
  auto Place=[&](int32 ID,FVector P)
  {
   auto& B=Bodies.FindChecked(ID);
   if(B.Actor.IsValid()){B.Actor->SetActorLocation(P);B.Actor->SetActorRotation(FRotator::ZeroRotator);B.Actor->GetCharacterMovement()->StopMovementImmediately();}
   else{B.Capsule->SetWorldLocation(P);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=P;F.Forward=FVector::ForwardVector;}
  };
  for(const auto& Pair:Store->GetUnits())
  {
   Place(Pair.Key,FVector(8000+(Pair.Key%20)*120,-7000+(Pair.Key/20)*120,100));
   if(Pair.Value.Team==1){Resource(Pair.Key).Ammo=0;Resource(Pair.Key).Reserve=0;Resource(Pair.Key).Perception.SightRange=100;}
  }
  for(int32 SID=1;SID<=3;++SID)
  {
   for(int32 I=0;I<N;++I)Place((SID-1)*N+I+1,FVector(-9000-(I/5)*120,-7000-(SID-1)*650-(I%5)*100,100));
   IssueOrder(SID,EBattleOrder::AttackMove,FVector(-6000,-7000-(SID-1)*650,0),true);Store->SetSquadDelegated(SID,true);
  }
  Place(Enemy,FVector(-6000,-7000,100));Resource(Enemy).HP=10000;
  Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  for(int32 SID=1;SID<=3;++SID)T->Samples.Add(SID,T->Observe(*Store->FindSquad(SID)));
  Check(Sense->FindContact(1,Enemy)&&Sense->FindContact(1,Enemy)->bVisible,TEXT("fixture has real acquired contact"));
  Check(T->Apply(1,EBattleSquadTactic::Retreat,EBattleOrder::Move,FVector(-10000,-7000,0),TEXT("SAVE RETREAT")),TEXT("active retreat fixture"));
  Check(T->Apply(2,EBattleSquadTactic::Support,EBattleOrder::Hold,Position(N+1),TEXT("SAVE SUPPORT")),TEXT("active support fixture"));
  Plan(2).SupportSquad=1;Plan(2).SupportTarget=Enemy;Plan(2).SupportUntil=18;Plan(2).LastSupportShot=10;Plan(2).bSupportConfirmed=true;
  Check(T->Apply(3,EBattleSquadTactic::Flank,EBattleOrder::Move,FVector(-8500,-8300,0),TEXT("SAVE SOLO FLANK")),TEXT("active flank fixture"));
  Plan(3).FlankTarget=Enemy;Plan(3).FlankContact=Position(Enemy);Plan(3).FlankExposure=.6f;
  Plan(1).Age=3.25;Plan(1).RetrySeconds=1.5;Plan(3).Age=2.5;
  T->RecordDamage(1,25);
  Resource(Follower).HP=55;Resource(Follower).Ammo=5;Resource(Follower).Reserve=17;Resource(Follower).ReloadEndTime=Now+1.5;Resource(Follower).NextShotTime=Now+.5;
  Store->ApplyDamage(LastLeader,0,100000);PC->SetSelection(Actor(1));Match->DelegationClock=2.5;
  auto* Saved=Match->CaptureSnapshot();FString Error;
  const bool Valid=Saved&&Saved->Validate(Error);Check(Valid,TEXT("mixed snapshot validates"));
  if(!Valid){UE_LOG(LogFutureBattle,Error,TEXT("MIXED_SAVE_VALIDATION %s"),*Error);SmokeStage=3;Later(.1f);return;}
  Check(Saved->Version==2&&Saved->Units.Num()==Total&&Saved->Squads.Num()==50,TEXT("canonical 50 squads x rifle squad size records"));
  int32 Actors=0;for(const auto& E:Saved->Units)Actors+=E.State.bActorRepresentation;
  Check(Actors==50,TEXT("50 Actor leaders and the rest Mass followers encoded"));
  Check(Match->SaveMatch(TEXT("FutureBattleSmokeOnly")),TEXT("separate mixed smoke slot written"));
  Resource(Follower).HP=1;Resource(Follower).Ammo=0;Match->Credits[0]=7;
  Check(Match->LoadMatch(TEXT("FutureBattleSmokeOnly")),TEXT("serialized mixed snapshot restored"));
  UE_LOG(LogFutureBattle,Display,TEXT("MIXED_SAVE_LOAD_MESSAGE %s"),*Match->LastMessage);
  Check(Store->Find(Follower)->HP==55&&Store->Find(Follower)->Ammo==5&&Store->Find(Follower)->Reserve==17,TEXT("follower resources survive body replacement"));
  Check(FMath::IsNearlyEqual(Store->Find(Follower)->ReloadEndTime-Now,1.5,.01)&&FMath::IsNearlyEqual(Store->Find(Follower)->NextShotTime-Now,.5,.01),TEXT("relative reload and shot timers survive"));
  Check(Bodies.Num()==Total&&Mode->Units.Num()==50&&Store->GetUnits().Num()==Total,TEXT("representation counts preserved"));
  Check(Plan(1).Action==EBattleSquadTactic::Retreat&&Plan(2).Action==EBattleSquadTactic::Support&&Plan(3).Action==EBattleSquadTactic::Flank,TEXT("three active tactical actions restored"));
  Check(Plan(2).SupportSquad==1&&Plan(2).SupportTarget==Enemy&&Plan(3).FlankTarget==Enemy&&Plan(1).Age==3.25f&&Plan(1).RetrySeconds==1.5f,TEXT("coordination references and elapsed timers restored"));
  Check(T->Clock()==10&&T->Observe(*Store->FindSquad(1)).RecentDamage==25&&Match->DelegationClock==2.5f,TEXT("decision clocks and recent damage preserved"));
  Check(Store->FindSquad(50)->CommandStatus==EBattleCommandStatus::Disabled&&!Store->Find(LastLeader)->IsAlive()&&!CanControlLeader(LastLeader),TEXT("dead leader stays disabled"));
  bool Transforms=true;for(const auto& E:Saved->Units)if(!Position(E.State.UnitID).Equals(E.Position,.01))
  {Transforms=false;UE_LOG(LogFutureBattle,Display,TEXT("MIXED_SAVE_POSITION id=%d saved=%s restored=%s"),E.State.UnitID,*E.Position.ToCompactString(),*Position(E.State.UnitID).ToCompactString());}
  Check(Transforms,TEXT("all canonical IDs retain their transforms"));
  Check(Plans.FindChecked(1).ArrivalGoals.Num()==N&&Plans.FindChecked(3).ArrivalGoals.Num()==N,TEXT("active routes and arrival reservations rebuilt"));
  auto Reject=[&](TFunctionRef<void(UBattleSave&)> Corrupt,const TCHAR* Label)
  {
   auto* Bad=DuplicateObject<UBattleSave>(Saved,this);Corrupt(*Bad);
   const FGuid Before=Match->MatchID;const float HP=Store->Find(Follower)->HP;auto* Body=Actor(1);const int32 Credit=Match->Credits[0];
   Check(!Match->RestoreSnapshot(Bad)&&Match->MatchID==Before&&Store->Find(Follower)->HP==HP&&Actor(1)==Body&&Match->Credits[0]==Credit&&Bodies.Num()==Total,Label);
  };
  Reject([](UBattleSave& S){S.Units[1].State.UnitID=S.Units[0].State.UnitID;},TEXT("duplicate identity rejected atomically"));
  Reject([](UBattleSave& S){S.SavedPlans[2].SupportSquad=26;},TEXT("enemy support reference rejected atomically"));
  Reject([](UBattleSave& S){S.Units[1].State.bActorRepresentation=true;},TEXT("follower Actor type corruption rejected atomically"));
  Reject([](UBattleSave& S){S.MapName=TEXT("OtherMap");},TEXT("foreign map rejected atomically"));
  Reject([](UBattleSave& S){S.WeaponRange+=100;},TEXT("changed range rules rejected atomically"));
  Reject([](UBattleSave& S){S.CombatStateVersion=3;},TEXT("unknown combat extension rejected"));
  Reject([](UBattleSave& S){S.Units[0].State.Shield=1;},TEXT("shield on rifle soldier rejected"));
  Reject([](UBattleSave& S){S.Units[0].State.LauncherWait=-1;},TEXT("negative launcher timer rejected"));
  Reject([](UBattleSave& S){S.SavedArtilleryShells.Add(FBattleSavedArtilleryShell());},TEXT("invalid shell shooter rejected"));
  Reject([](UBattleSave& S){S.Version=1;},TEXT("legacy schema cannot replace mixed match"));
  Reject([](UBattleSave& S){S.Squads[0].Destination=FVector(31000,21000,0);S.SavedPlans[1].Destination=S.Squads[0].Destination;},TEXT("unreachable saved route rejected before body replacement"));
  RestoreStageFailureAfter=2;
  Reject([](UBattleSave&){},TEXT("failure after staging Actor and Mass bodies rolls back atomically"));
  RestoreStageFailureAfter=INDEX_NONE;
  Check(!Match->SaveMatch(TEXT("UnexpectedSlot")),TEXT("unapproved slot does not write"));
  // Capture and restore actual direct control through the same public APIs used by F8/F9.
  PC->ControlUnit(Actor(1));auto* Direct=Match->CaptureSnapshot();
  Check(Direct&&Direct->ControlledID==1&&Match->RestoreSnapshot(Direct)&&PC->GetPawn()==Actor(1),TEXT("TPS control restored to recreated leader"));
  PC->EnterTactical();Check(PC->GetPawn()!=Actor(1),TEXT("restored leader returns to RTS"));
  auto* Legacy=DuplicateObject<UBattleSave>(Saved,this);Legacy->CombatStateVersion=0;Legacy->SavedArtillery.Reset();Legacy->SavedArtilleryShells.Reset();
  Legacy->SavedGroups.Reset();Legacy->SavedSquadGroup.Reset();Legacy->SavedNextGroupID=1;Legacy->SavedCarChoices.Reset();
  Check(Match->RestoreSnapshot(Legacy)&&Artillery.IsEmpty()&&ArtilleryShells.IsEmpty(),TEXT("pre-extension mixed snapshot remains loadable"));
  Check(Match->RestoreSnapshot(Saved),TEXT("restore fixture for live continuation"));
  PC->QuickSaveSlot=TEXT("FutureBattleSmokeOnly");PC->SaveMatch();Resource(Follower).HP=1;PC->LoadMatch();
  Check(Store->Find(Follower)->HP==55&&Match->LastMessage==TEXT("Match restored"),TEXT("F8/F9 controller callbacks reach mixed save/load using isolated test slot"));
  PC->QuickSaveSlot=TEXT("FutureBattleQuick01");
  Mode->bCombatPaused=false;Match->DirectorClock=0;Match->DelegationClock=0;
  SmokePosition=Position(1);SmokeStage=1;Later(2);return;
 }
 if(SmokeStage==1)
 {
  Check(FVector::Dist2D(Position(1),SmokePosition)>50,TEXT("restored retreat continues through real Actor movement"));
  Check(Store->Find(Follower)->ReloadEndTime==0&&Store->Find(Follower)->Ammo>5,TEXT("restored follower reload completes"));
  Check(Bodies.Num()==Total&&Mode->Units.Num()==50,TEXT("live continuation preserves mixed representation"));
  FString LiveError;auto* Live=Match->CaptureSnapshot();
  Check(Live&&Live->Validate(LiveError)&&Match->SaveMatch(TEXT("FutureBattleSmokeOnly")),TEXT("live mixed combat remains serializable after clocks advance"));
  if(!LiveError.IsEmpty())UE_LOG(LogFutureBattle,Error,TEXT("MIXED_SAVE_LIVE_VALIDATION %s"),*LiveError);
  Mode->bCombatPaused=true;SmokeStage=3;
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-restored.png"),true,false);
  Later(1);return;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_SAVE_RESULT failures=%d"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(true,SmokeFailures?1:0);
}
