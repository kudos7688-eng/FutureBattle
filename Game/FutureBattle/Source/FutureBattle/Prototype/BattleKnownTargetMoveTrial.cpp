// Stage 2 extension. The original 27 stage 1 checks remain in BattleKnownTargetFireTrial.cpp.
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleCover.h"
#include "BattleStance.h"
#include "BattleSectors.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "FutureBattle.h"

int32 UBattleLeaderRuntime::RunKnownTargetMoveTrial()
{
 auto* TestWorld=GetWorld();auto* TestStore=TestWorld->GetSubsystem<UBattleState>();auto* TestSense=TestWorld->GetSubsystem<UBattlePerception>();
 auto& Shooter=TestStore->Units.FindChecked(2);auto& Squad=TestStore->Squads.FindChecked(Shooter.SquadID);
 const FVector Start=Position(2),Enemy=Start+FVector(1500,0,0);double TestNow=TestWorld->GetTimeSeconds()+10.;int32 Failures=0;
 auto Check=[&](const TCHAR* Name,bool OK){Failures+=!OK;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_CHECK name=%s ok=%d"),Name,OK);};
 auto Count=[&](const TCHAR* Key){return SightExperiment.CoverDiag.FindRef(FName(Key));};
 auto Put=[&](int32 TestID,FVector At)
 {
  auto& TestBody=Bodies.FindChecked(TestID);
  if(TestBody.Actor.IsValid())TestBody.Actor->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);
  else{TestBody.Capsule->SetWorldLocation(At);auto& TestFragment=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(TestBody.Entity);TestFragment.Position=At;TestFragment.Forward=FVector::ForwardVector;}
  TestBody.Velocity=FVector::ZeroVector;
 };
 TArray<AActor*> Walls;
 auto Wall=[&](FVector At,FVector Extent)
 {
  auto* WallActor=TestWorld->SpawnActor<AActor>();if(!WallActor)return;
  auto* Box=NewObject<UBoxComponent>(WallActor);WallActor->SetRootComponent(Box);WallActor->AddInstanceComponent(Box);
  Box->SetBoxExtent(Extent);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetCanEverAffectNavigation(false);Box->SetWorldLocation(At);Box->SetMobility(EComponentMobility::Static);Box->RegisterComponent();Walls.Add(WallActor);
 };
 auto Observe=[&]()
 {
  TestSense->Reset();auto& Contact=TestSense->Observers.FindOrAdd(11).Contacts.FindOrAdd(251);
  Contact.TargetID=251;Contact.SourceUnitID=11;Contact.LastKnownPosition=Enemy;Contact.LastSeenTime=TestNow;
  // This fixture is a normal observation by another squad, not an unreported long-range scout contact.
  // FBattleContact defaults to restricted provenance; BuildTeamKnownTargets must keep rejecting that default.
  Contact.bLongRangeObservation=false;Contact.ObservedSquadType=0;
  Contact.bVisible=true;Contact.bShared=false;Contact.KnownExpiresAt=TestNow+20.;Contact.KnownSourceSquad=TestStore->Find(11)->SquadID;TestSense->BuildTeamKnownTargets(TestNow);
 };
 auto Reset=[&]()
 {
  if(KnownMoves.Contains(2))EndKnownMove(2,TestNow,TEXT("known_move_test_reset"));
  CancelCoverAction(2);TestWorld->GetSubsystem<UBattleCover>()->Release(2);CoverApproaches.Remove(2);SoldierPaths.Remove(2);FirePositions.Remove(2);
  KnownFireSearches.Reset();KnownMoveAfter.Reset();KnownMoveCursor.Reset();
  for(auto& FireBudget:KnownFireBudgets)FireBudget=BattleKnownFire::FBudget();
  for(auto& MoveBudget:KnownMoveBudgets)MoveBudget=FKnownMoveBudget();
  for(auto* OldWall:Walls){OldWall->SetActorEnableCollision(false);OldWall->Destroy();}Walls.Reset();
  TestNow+=10.;Put(2,Start);Put(251,Enemy);Put(252,Start+FVector(5000,2000,0));
  Shooter.Order=EBattleOrder::Defend;Shooter.bSoldierOrder=false;Shooter.Ammo=30;Shooter.Reserve=90;Shooter.NextShotTime=0;Shooter.ReloadEndTime=0;
  Shooter.Perception.SightRange=10000;Shooter.Perception.FieldOfView=120;Shooter.Destination=Start;Shooter.CoverPhase=0;
  Squad.Order=EBattleOrder::Defend;Squad.Destination=Start;Squad.DefenseRadius=5000;Squad.bSectorMission=false;Squad.TacticalPlan.Action=EBattleSquadTactic::Mission;
  auto& TestPlan=Plans.FindOrAdd(Shooter.SquadID);TestPlan=FBattleLeaderPlan();TestPlan.Center=Start;
  SetStance(2,BattleStance::Stand,TestNow,true);Observe();
  Wall(Start+FVector(250,0,60),FVector(30,120,150));
  // Each free candidate has low cover: same hidden chest as the start, with a clear standing muzzle.
  Wall(Start+FVector(100,-500,-25),FVector(20,160,65));Wall(Start+FVector(100,500,-25),FVector(20,160,65));
 };
 auto TryStart=[&]()
 {
  const int32 StartRays=Count(TEXT("known_move_rays")),StartSweeps=Count(TEXT("known_move_sweeps")),StartNav=Count(TEXT("known_move_nav"));
  for(int32 Attempt=0;Attempt<8;++Attempt){TestNow+=1.01;Observe();if(StartKnownMove(2,TestNow))return true;}
  int32 TestLiving=0,TestMoving=0;
  for(int32 TestMember:Squad.Members)if(const auto* TestUnit=TestStore->Find(TestMember);TestUnit && TestUnit->IsAlive())
  {++TestLiving;if(TestMember!=2 && (TestUnit->CoverPhase==1 || CoverApproaches.Contains(TestMember)))++TestMoving;}
  const int32 TestLimit=BattleKnownFire::MoveMax()>0?FMath::Min(TestLiving/2,BattleKnownFire::MoveMax()):TestLiving/2;
  const auto* TestKnown=TestSense->TeamKnownTargets[Shooter.Team].Targets.Find(251);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_DIAG move_start_failed now=%.3f allowed=%d known=%d fresh=%d target_allowed=%d living=%d moving=%d limit=%d episode=%d approach=%d cooldown=%.3f cover_built=%d rays=%d sweeps=%d nav=%d"),
   TestNow,KnownMoveAllowed(2),TestKnown!=nullptr,TestKnown && TestKnown->Fresh(TestNow),TestKnown && WeaponOrderAllows(2,251,TestKnown->At(TestNow)),
   TestLiving,TestMoving,TestLimit,KnownMoves.Contains(2),CoverApproaches.Contains(2),KnownMoveAfter.FindRef(2)-TestNow,TestWorld->GetSubsystem<UBattleCover>()->IsBuilt(),
   Count(TEXT("known_move_rays"))-StartRays,Count(TEXT("known_move_sweeps"))-StartSweeps,Count(TEXT("known_move_nav"))-StartNav);
  return false;
 };
 TestWorld->GetSubsystem<UBattleCover>()->BuildSlots();Reset();
 const auto Blocked=SelectKnownFire(2,Start+FVector(0,0,60),TestNow);
 const bool Started=!Blocked.Ready() && TryStart();const int32 BeforeShot=Shooter.ShotSequence;
 bool Within=true,Arrived=false,RecheckedHidden=false,HiddenDidNotFire=false;
 const int32 SensorsBefore=Count(TEXT("known_fire_sensor_checks"));
 if(Started)
 {
  for(int32 Frame=0;Frame<600;++Frame)
  {
   TestNow+=1./30.;Observe();Follow(2,1.f/30.f,TestNow);
   Within &= FVector::Dist2D(Position(2),Start)<=BattleKnownFire::MoveRange()+1.;
   if(const auto* Walk=KnownMoves.Find(2);Walk && Walk->Arrived>=0)Arrived=true;
   if(Arrived && !RecheckedHidden)
   {
    // Keep the shared observation unchanged while the real target is no longer visible.
    RecheckedHidden=true;const int32 HiddenShots=Shooter.ShotSequence;
    Put(251,Start+FVector(50000,0,0));
    for(int32 HiddenFrame=0;HiddenFrame<30;++HiddenFrame){TestNow+=1./30.;Observe();Follow(2,1.f/30.f,TestNow);}
    HiddenDidNotFire=Shooter.ShotSequence==HiddenShots;
    Put(251,Enemy);Observe();
   }
   if(Count(TEXT("known_move_arrived_fired"))>0)break;
  }
 }
 Check(TEXT("move_arrive_personal_fire"),Started && Arrived && Shooter.ShotSequence>BeforeShot && Count(TEXT("known_move_arrived_fired"))==1 && Within && RecheckedHidden && HiddenDidNotFire && Count(TEXT("known_fire_sensor_checks"))>SensorsBefore);
 const int32 Once=Count(TEXT("known_move_arrived_fired"));KnownMoveShot(2,251,TestNow);
 Check(TEXT("move_first_shot_once"),Once==1 && Count(TEXT("known_move_arrived_fired"))==Once);
 Reset();const bool StartForOrder=TryStart();const int32 OrderBefore=Count(TEXT("known_move_cancel_order"));++Shooter.OrderRevision;
 Follow(2,1.f/30.f,TestNow+.01);
 Check(TEXT("move_order_change"),StartForOrder && !KnownMoves.Contains(2) && Count(TEXT("known_move_cancel_order"))==OrderBefore+1 && KnownMoveAfter.FindRef(2)>=TestNow+5.);
 Reset();const int32 HeldBefore=Count(TEXT("known_move_started"));Squad.Order=EBattleOrder::Hold;
 const bool Held=!StartKnownMove(2,TestNow);Squad.Order=EBattleOrder::Defend;Shooter.bSoldierOrder=true;
 const bool Personal=!StartKnownMove(2,TestNow);Shooter.bSoldierOrder=false;Squad.TacticalPlan.Action=EBattleSquadTactic::Retreat;
 const bool Retreat=!StartKnownMove(2,TestNow);Squad.TacticalPlan.Action=EBattleSquadTactic::Recover;
 const bool Supply=!StartKnownMove(2,TestNow);Squad.TacticalPlan.Action=EBattleSquadTactic::Regroup;
 Check(TEXT("move_priority_gates"),Held && Personal && Retreat && Supply && !StartKnownMove(2,TestNow) && Count(TEXT("known_move_started"))==HeldBefore);
 Reset();auto& March=Plans.FindChecked(Shooter.SquadID);March.Path={Start,Start+FVector(2000,0,0)};Bodies.FindChecked(2).Waypoint=1;
 Squad.Order=EBattleOrder::AttackMove;Shooter.Order=EBattleOrder::AttackMove;Shooter.Destination=March.Path[1];
 Check(TEXT("move_march_gate"),!StartKnownMove(2,TestNow));
 Reset();const bool OutsideRange=!KnownMovePlaceAllowed(2,Start,Start+FVector(BattleKnownFire::MoveRange()+1.,0,0));
 Plans.FindChecked(Shooter.SquadID).Center=Start+FVector(BattleKnownFire::MoveSquadRadius()+10.,0,0);
 const bool OutsideSquad=!KnownMovePlaceAllowed(2,Start,Start);Plans.FindChecked(Shooter.SquadID).Center=Start;Squad.DefenseRadius=100;
 const bool OutsideDefend=!KnownMovePlaceAllowed(2,Start,Start+FVector(200,0,0));Squad.DefenseRadius=5000;Squad.bSectorMission=true;
 const FVector SectorStart(10,100,Start.Z),OutsideSector(-10,100,Start.Z);
 Squad.Destination=SectorStart;Plans.FindChecked(Shooter.SquadID).Center=SectorStart;
 const bool SectorRejected=!KnownMovePlaceAllowed(2,SectorStart,OutsideSector);Squad.bSectorMission=false;
 Check(TEXT("move_distance_mission_limits"),OutsideRange && OutsideSquad && OutsideDefend && SectorRejected && KnownMovePlaceAllowed(2,SectorStart,OutsideSector));
 Reset();TArray<int32> Busy;
 for(int32 Member:Squad.Members)if(Member!=2 && TestStore->Find(Member)->IsAlive()){FCoverApproach Reserved;Reserved.Goal=Start+FVector(0,500,0);CoverApproaches.Add(Member,Reserved);Busy.Add(Member);}
 Check(TEXT("move_shared_limit"),!StartKnownMove(2,TestNow));for(int32 Member:Busy)CoverApproaches.Remove(Member);
 Reset();const bool First=TryStart();const int32 Starts=Count(TEXT("known_move_started"));
 const bool NoDouble=First && !StartKnownMove(2,TestNow+2.) && Count(TEXT("known_move_started"))==Starts;
 // An upper-priority approach replacing this episode must keep its own reservation/path.
 if(First){auto& Replacement=CoverApproaches.FindChecked(2);Replacement.Target=252;Replacement.bCharge=true;}
 StepKnownMove(2,1.f/30.f,TestNow+.01);
 Check(TEXT("move_no_double_start"),NoDouble && !KnownMoves.Contains(2) && CoverApproaches.Contains(2) &&
  CoverApproaches.FindChecked(2).Target==252 && SoldierPaths.Contains(2));
 Reset();const bool StaleStart=TryStart();const int32 StaleBefore=Count(TEXT("known_move_cancel_stale"));
 FVector StaleGoal=Start;bool StaleWasFree=false;
 if(StaleStart)
 {
  StaleWasFree=KnownMoves.FindChecked(2).Slot==INDEX_NONE;
  StaleGoal=StaleWasFree?Start:KnownMoves.FindChecked(2).Goal;
  for(int32 WalkFrame=0;WalkFrame<10;++WalkFrame){TestNow+=1./30.;Observe();StepKnownMove(2,1.f/30.f,TestNow);}
 }
 const bool LeftStart=FVector::Dist2D(Position(2),Start)>40.;TestSense->Reset();
 for(int32 Frame=0;Frame<600 && KnownMoves.Contains(2);++Frame){TestNow+=1./30.;StepKnownMove(2,1.f/30.f,TestNow);}
 Check(TEXT("move_stale_return"),StaleStart && LeftStart && !KnownMoves.Contains(2) && Count(TEXT("known_move_cancel_stale"))==StaleBefore+1 &&
  FVector::Dist2D(Position(2),StaleGoal)<40. && !CoverApproaches.Contains(2) && !SoldierPaths.Contains(2));
 Reset();bool Tokens=true;for(int32 Token=0;Token<16;++Token)Tokens &= KnownMoveSpend(2,TestNow,0);
 const bool Deferred=!KnownMoveSpend(2,TestNow,0);auto& TeamBudget=KnownFireBudgets[Shooter.Team];
 Check(TEXT("move_ray_budget"),Tokens && Deferred && TeamBudget.Used==16 && KnownMoveBudgets[Shooter.Team].Rays==16);
 bool MoreTokens=true;for(int32 Token=0;Token<16;++Token)MoreTokens &= KnownFireRay(2,TestNow);
 Check(TEXT("move_shared_ray_budget"),MoreTokens && TeamBudget.Used==32 && !KnownFireRay(2,TestNow));
 Reset();bool Sweeps=true,Navs=true;for(int32 Token=0;Token<16;++Token){Sweeps &= KnownMoveSpend(2,TestNow,1);Navs &= KnownMoveSpend(2,TestNow,2);}
 Check(TEXT("move_geometry_budget"),Sweeps && Navs && !KnownMoveSpend(2,TestNow,1) && !KnownMoveSpend(2,TestNow,2));
 const bool NewCaller=!KnownMoveSpend(11,TestNow+1.01,2);const bool OldCaller=KnownMoveSpend(2,TestNow+1.01,2);
 Check(TEXT("move_budget_fifo"),NewCaller && OldCaller);
 Reset();const bool IntervalStart=TryStart();if(IntervalStart)EndKnownMove(2,TestNow,TEXT("known_move_test_reset"));
 Check(TEXT("move_cooldown"),IntervalStart && !StartKnownMove(2,TestNow+4.99));
 for(auto* TestWall:Walls){TestWall->SetActorEnableCollision(false);TestWall->Destroy();}
 return Failures;
}
