// Stage 2 extension. The original 27 stage 1 checks remain in BattleKnownTargetFireTrial.cpp.
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleCover.h"
#include "BattleStance.h"
#include "BattleSectors.h"
#include "NavigationSystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FutureBattle.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

int32 UBattleLeaderRuntime::RunKnownTargetMoveTrial()
{
 auto* TestWorld=GetWorld();auto* TestStore=TestWorld->GetSubsystem<UBattleState>();auto* TestSense=TestWorld->GetSubsystem<UBattlePerception>();
 auto& Shooter=TestStore->Units.FindChecked(2);auto& Squad=TestStore->Squads.FindChecked(Shooter.SquadID);
 // Follow/UpdateAim use the supplied time, but Fire/ConsumeShot read the world clock.
 // Advance both together: otherwise a turn during marching sets NextShotTime in
 // the simulated future and Fire can never finish that cooldown. Restore on exit.
 TGuardValue<double> MoveTrialClock(TestWorld->TimeSeconds,TestWorld->GetTimeSeconds()+10.);
 double& TestNow=TestWorld->TimeSeconds;
 const FVector Start=Position(2),Enemy=Start+FVector(1500,0,0);int32 Failures=0;
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
 auto Reset=[&](bool OriginalLowWalls=false)
 {
  if(KnownMoves.Contains(2))EndKnownMove(2,TestNow,TEXT("known_move_test_reset"));
  CancelCoverAction(2);TestWorld->GetSubsystem<UBattleCover>()->Release(2);CoverApproaches.Remove(2);SoldierPaths.Remove(2);FirePositions.Remove(2);
  KnownFireSearches.Reset();KnownMoveAfter.Reset();KnownMoveEndCooldown.Reset();KnownMoveCursor.Reset();KnownMoveReservedID=INDEX_NONE;
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
  // Match the planner's projected body centre, not the physical floor at Start.
  // The old top (Start.Z+40) leaves only 8.45 cm for navmesh elevation before
  // the chest ray at the wall's near face (80/1500 of the ray) clears it.
  auto* TrialNav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(TestWorld);
  for(double TrialSide:{-500.,500.})
  {
   FNavLocation TrialProjected;
   if(!TrialNav || !TrialNav->ProjectPointToNavigation(Start+FVector(0,TrialSide,-90),TrialProjected,FVector(50,50,150)))
   {Check(TEXT("move_fixture_nav"),false);continue;}
   const FVector TrialAnchor=TrialProjected.Location+FVector(0,0,90);
   // A 15 cm margin on either side of the chest/muzzle heights. Keep the wall
   // off the lateral walking path (its nearest face is 80 cm away).
   if(OriginalLowWalls)Wall(Start+FVector(100,TrialSide,-25),FVector(20,160,65));
   else Wall(TrialAnchor+FVector(100,0,-22.5),FVector(20,160,67.5));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_FIXTURE side=%.0f start=%s anchor=%s old_top=%.3f old_chest_at_face=%.3f new_top=%.3f"),
    TrialSide,*Start.ToCompactString(),*TrialAnchor.ToCompactString(),Start.Z+40.,FMath::Lerp(TrialAnchor.Z+30.,Enemy.Z+60.,80./(Enemy.X-TrialAnchor.X)),TrialAnchor.Z+45.);
  }
 };
 auto TryStart=[&](bool ShortLived=false)
 {
  const int32 StartRays=Count(TEXT("known_move_rays")),StartSweeps=Count(TEXT("known_move_sweeps")),StartNav=Count(TEXT("known_move_nav"));
  for(int32 Attempt=0;Attempt<24;++Attempt)
  {
   const TMap<FName,int32> BeforeRejects=SightExperiment.CoverDiag;
   TestNow+=1.01;Observe();
   if(ShortLived)TestSense->TeamKnownTargets[Shooter.Team].Targets.FindChecked(251).ExpiresAt=TestNow+.15;
   const bool AttemptStarted=StartKnownMove(2,TestNow);
   for(const auto& Reject:SightExperiment.CoverDiag)
    if(Reject.Key.ToString().StartsWith(TEXT("known_move_reject_")) && Reject.Value>BeforeRejects.FindRef(Reject.Key))
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_REJECT now=%.3f attempt=%d started=%d %s=%d"),TestNow,Attempt,AttemptStarted,*Reject.Key.ToString(),Reject.Value-BeforeRejects.FindRef(Reject.Key));
   if(AttemptStarted)return true;
  }
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
 TestWorld->GetSubsystem<UBattleCover>()->BuildSlots();
 TestWorld->GetSubsystem<UBattleCover>()->KnownMoveGeometry(Start,BattleKnownFire::MoveRange());
 // Replay the original geometry first for diagnosis, then require the repaired
 // fixture to hide the chest AND permit fire. Existing outcome checks stay intact.
 auto ProbeFixture=[&](bool RequireGeometry)
 {
  for(double FixtureSide:{-500.,500.})
  {
   auto* FixtureNav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(TestWorld);FNavLocation FixtureProjected;
   const bool FixtureOnNav=FixtureNav && FixtureNav->ProjectPointToNavigation(Start+FVector(0,FixtureSide,-90),FixtureProjected,FVector(50,50,150));
   bool FixtureHere=false,FixtureThere=false,FixtureFire=false;
   if(FixtureOnNav)
   {
    TestNow+=.2;bKnownMoveDeferred=false;
    const bool ProbeReserved=KnownMoveReserve(2,TestNow);
    if(!ProbeReserved){Check(TEXT("move_fixture_budget"),false);continue;}
    const FVector FixtureAnchor=FixtureProjected.Location+FVector(0,0,90);
    FixtureHere=KnownMoveLine(2,Start+FVector(0,0,30),Enemy+FVector(0,0,60),TestNow);
    FixtureThere=KnownMoveLine(2,FixtureAnchor+FVector(0,0,30),Enemy+FVector(0,0,60),TestNow);
    FixtureFire=KnownMoveLine(2,FixtureAnchor+FVector(0,0,60),Enemy+FVector(0,0,60),TestNow);
    KnownMoveRelease(2);
   }
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_FIXTURE_PROBE repaired=%d side=%.0f nav=%d here=%d there=%d fire=%d deferred=%d"),RequireGeometry,FixtureSide,FixtureOnNav,FixtureHere,FixtureThere,FixtureFire,bKnownMoveDeferred);
   if(RequireGeometry)Check(FixtureSide<0?TEXT("move_fixture_left_hidden_fire"):TEXT("move_fixture_right_hidden_fire"),FixtureOnNav && !FixtureHere && !FixtureThere && FixtureFire && !bKnownMoveDeferred);
  }
 };
 Reset(true);ProbeFixture(false);
 // Run the original candidate selection too: its per-attempt rejection counts
 // distinguish exposure from budget exhaustion or a different map/navmesh.
 const bool OriginalStarted=TryStart();
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_ORIGINAL started=%d"),OriginalStarted);
 Reset();ProbeFixture(true);
 TestNow+=.2;Observe();
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
 if(!BattleKnownFire::MoveMarch())
 {
  Check(TEXT("move_march_gate"),!StartKnownMove(2,TestNow));
  const int32 GateStartsBefore=Count(TEXT("known_move_started")),GateCountBefore=Count(TEXT("known_move_gate_march"));
  TestNow+=1.01;Observe();Follow(2,0.f,TestNow);
  Check(TEXT("move_march_follow_gate"),!KnownMoves.Contains(2) && Count(TEXT("known_move_started"))==GateStartsBefore && Count(TEXT("known_move_gate_march"))>GateCountBefore);
 }
 if(BattleKnownFire::MoveMarch())
 {
  // Exercise the Follow entry gate as well as the planner. Zero dt keeps the initial geometry fixed.
  const FVector MarchDestination=Start+FVector(0,3000,0);
  March.Path={Start,MarchDestination};March.Corridors.Add(2,March.Path);March.ArrivalGoals.Add(2,MarchDestination);Shooter.Destination=MarchDestination;Squad.Destination=MarchDestination;
  const auto SavedOrderRevision=Shooter.OrderRevision;const auto SavedMissionRevision=Squad.MissionRevision;
  const int32 MarchShots=Shooter.ShotSequence,MarchFiredBefore=Count(TEXT("known_move_arrived_fired"));
  const TMap<FName,int32> MarchBeforeDiag=SightExperiment.CoverDiag;
  bool MarchStarted=false,MarchArrived=false,MarchWithin=true;
  for(int32 MarchAttempt=0;MarchAttempt<24 && !MarchStarted;++MarchAttempt)
  {
   const TMap<FName,int32> MarchBeforeAttempt=SightExperiment.CoverDiag;
   TestNow+=1.01;Observe();Follow(2,0.f,TestNow);MarchStarted=KnownMoves.Contains(2);
   for(const auto& MarchReject:SightExperiment.CoverDiag)
    if(MarchReject.Key.ToString().StartsWith(TEXT("known_move_reject_")) && MarchReject.Value>MarchBeforeAttempt.FindRef(MarchReject.Key))
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_REJECT scene=march now=%.3f attempt=%d started=%d %s=%d"),TestNow,MarchAttempt,MarchStarted,*MarchReject.Key.ToString(),MarchReject.Value-MarchBeforeAttempt.FindRef(MarchReject.Key));
  }
  for(int32 MarchFrame=0;MarchFrame<600 && KnownMoves.Contains(2);++MarchFrame)
  {
   TestNow+=1./30.;Observe();Follow(2,1.f/30.f,TestNow);
   if(const auto* MarchEpisode=KnownMoves.Find(2);MarchEpisode && MarchEpisode->Arrived>=0)MarchArrived=true;
   MarchWithin &= FVector::Dist2D(Position(2),Plans.FindChecked(Shooter.SquadID).Center)<=BattleKnownFire::MoveSquadRadius()+1.;
  }
  for(const auto& MarchDiag:SightExperiment.CoverDiag)
   if(MarchDiag.Value>MarchBeforeDiag.FindRef(MarchDiag.Key))
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_MARCH_DIAG %s=%d"),*MarchDiag.Key.ToString(),MarchDiag.Value-MarchBeforeDiag.FindRef(MarchDiag.Key));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_MARCH_RESULT started=%d arrived=%d within=%d shots=%d first_shots=%d phase=%d now=%.3f world_now=%.3f next_shot=%.3f"),
   MarchStarted,MarchArrived,MarchWithin,Shooter.ShotSequence-MarchShots,Count(TEXT("known_move_arrived_fired"))-MarchFiredBefore,int32(Shooter.CoverPhase),TestNow,TestWorld->GetTimeSeconds(),Shooter.NextShotTime);
  Check(TEXT("move_march_arrive_fire"),MarchStarted && MarchArrived && MarchWithin && Shooter.ShotSequence>MarchShots && Count(TEXT("known_move_arrived_fired"))==MarchFiredBefore+1);
  const FVector MarchResumeAt=Position(2);TestSense->Reset();Put(251,Start+FVector(50000,0,0));
  for(int32 ResumeFrame=0;ResumeFrame<30;++ResumeFrame){TestNow+=1./30.;Follow(2,1.f/30.f,TestNow);}
  Check(TEXT("move_march_resume"),!KnownMoves.Contains(2) && !CoverApproaches.Contains(2) && !FirePositions.Contains(2) && !Shooter.CoverPhase &&
   FVector::Dist2D(Position(2),MarchDestination)<FVector::Dist2D(MarchResumeAt,MarchDestination)-40. &&
   Shooter.Order==EBattleOrder::AttackMove && Squad.Order==EBattleOrder::AttackMove && Shooter.OrderRevision==SavedOrderRevision && Squad.MissionRevision==SavedMissionRevision && Squad.Destination==MarchDestination);
  if(BattleKnownFire::MoveCommit()==0.)
  {
  // Legacy stale cancellation is tested only with the actual Commit=0 command line.
  Reset();auto& StaleMarchPlan=Plans.FindChecked(Shooter.SquadID);StaleMarchPlan.Path={Start,MarchDestination};StaleMarchPlan.Corridors.Add(2,StaleMarchPlan.Path);StaleMarchPlan.ArrivalGoals.Add(2,MarchDestination);Bodies.FindChecked(2).Waypoint=1;
  Squad.Order=EBattleOrder::AttackMove;Shooter.Order=EBattleOrder::AttackMove;Shooter.Destination=MarchDestination;Squad.Destination=MarchDestination;
  const bool StaleMarchStarted=TryStart();TestSense->Reset();Put(251,Start+FVector(50000,0,0));
  const int32 StaleMarchShots=Shooter.ShotSequence;const FVector StaleMarchAt=Position(2);
  for(int32 StaleMarchFrame=0;StaleMarchFrame<30;++StaleMarchFrame){TestNow+=1./30.;Follow(2,1.f/30.f,TestNow);}
  Check(TEXT("move_march_stale_resume"),StaleMarchStarted && !KnownMoves.Contains(2) && Shooter.ShotSequence==StaleMarchShots &&
   FVector::Dist2D(Position(2),MarchDestination)<FVector::Dist2D(StaleMarchAt,MarchDestination)-40.);
  }
  Reset();Squad.Order=EBattleOrder::AttackMove;Shooter.Order=EBattleOrder::AttackMove;Shooter.Destination=MarchDestination;
  // A fresh report in the spatial bucket but outside weapon engagement range cannot start a march detour.
  auto& DistantReport=TestSense->TeamKnownTargets[Shooter.Team].Targets.FindChecked(251);DistantReport.LastKnownPosition=Start+FVector(100000,0,0);
  Check(TEXT("move_march_near_gate"),!StartKnownMove(2,TestNow) && !KnownMoves.Contains(2));
  Reset();Squad.Order=EBattleOrder::AttackMove;Shooter.Order=EBattleOrder::AttackMove;Shooter.Destination=MarchDestination;Squad.bSectorMission=true;
  const FVector MarchCellStart(-10,100,Start.Z),MarchCellGoal(10,100,Start.Z);
  Squad.Destination=MarchCellGoal;Plans.FindChecked(Shooter.SquadID).Center=MarchCellStart;
  Check(TEXT("move_march_sector"),KnownMovePlaceAllowed(2,MarchCellStart,MarchCellStart) && KnownMovePlaceAllowed(2,MarchCellStart,MarchCellGoal) &&
   KnownMovePlaceAllowed(2,MarchCellStart,FVector(-10,-10,Start.Z)));
  Plans.FindChecked(Shooter.SquadID).Center=MarchCellStart+FVector(BattleKnownFire::MoveSquadRadius()+10.,0,0);
  Check(TEXT("move_march_squad_radius"),!KnownMovePlaceAllowed(2,MarchCellStart,MarchCellStart));
 }
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
 if(BattleKnownFire::MoveCommit()==0.)
 {
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
 }
 Reset();const bool RayReserved=KnownMoveReserve(2,TestNow);
 bool Tokens=RayReserved;for(int32 Token=0;Token<12;++Token)Tokens &= KnownMoveSpend(2,TestNow,0);
 KnownMoveRelease(2);auto& MoveTeamBudget=KnownMoveBudgets[Shooter.Team];
 Check(TEXT("move_ray_budget"),Tokens && MoveTeamBudget.Rays==12 && MoveTeamBudget.Sweeps==0 && MoveTeamBudget.Nav==0);
 // Movement rays are charged to the shared 32-ray allowance; only 20 remain.
 bool MoreTokens=true;for(int32 Token=0;Token<20;++Token)MoreTokens &= KnownFireRay(2,TestNow);
 Check(TEXT("move_shared_ray_budget"),MoreTokens && KnownFireBudgets[Shooter.Team].Used==32 && !KnownFireRay(2,TestNow));
 Reset();bool Sweeps=KnownMoveReserve(2,TestNow),Navs=Sweeps;
 for(int32 Token=0;Token<24;++Token)Sweeps &= KnownMoveSpend(2,TestNow,1);
 for(int32 Token=0;Token<8;++Token)Navs &= KnownMoveSpend(2,TestNow,2);
 KnownMoveRelease(2);
 Check(TEXT("move_geometry_budget"),Sweeps && Navs && MoveTeamBudget.Sweeps==24 && MoveTeamBudget.Nav==8 && MoveTeamBudget.Rays==0);
 // Failure in each dimension is atomic: no other dimension is charged and no cursor is advanced.
 bool Atomic=true;
 for(int32 Dimension=0;Dimension<3;++Dimension)
 {
  Reset();MoveTeamBudget.Window=FMath::FloorToInt64(TestNow/.1);
  MoveTeamBudget.Rays=Dimension==0?BattleKnownFire::MoveRayBudget():0;
  KnownFireBudgets[Shooter.Team].Window=MoveTeamBudget.Window;KnownFireBudgets[Shooter.Team].Used=MoveTeamBudget.Rays;
  MoveTeamBudget.Sweeps=Dimension==1?BattleKnownFire::MoveSweepBudget():0;
  MoveTeamBudget.Nav=Dimension==2?BattleKnownFire::MoveNavBudget():0;
  const int32 BeforeRays=MoveTeamBudget.Rays,BeforeSweeps=MoveTeamBudget.Sweeps,BeforeNav=MoveTeamBudget.Nav,BeforeShared=KnownFireBudgets[Shooter.Team].Used;
  Atomic &= !StartKnownMove(2,TestNow) && KnownMoveReservedID==INDEX_NONE && KnownMoveCursor.FindRef(2)==0 &&
   MoveTeamBudget.Rays==BeforeRays && MoveTeamBudget.Sweeps==BeforeSweeps && MoveTeamBudget.Nav==BeforeNav && KnownFireBudgets[Shooter.Team].Used==BeforeShared;
 }
 Check(TEXT("move_budget_atomic"),Atomic);
 const bool NewCaller=!KnownMoveReserve(11,TestNow+1.01);const bool OldCaller=KnownMoveReserve(2,TestNow+1.01);
 KnownMoveRelease(2);
 const bool NextCaller=KnownMoveReserve(11,TestNow+1.01);KnownMoveRelease(11);
 Check(TEXT("move_budget_fifo"),NewCaller && OldCaller && NextCaller);
 Reset();const bool IntervalStart=TryStart();if(IntervalStart)EndKnownMove(2,TestNow,TEXT("known_move_test_reset"));
 const int32 EndCooldownBefore=Count(TEXT("known_move_gate_end_cooldown"));
 Check(TEXT("move_cooldown"),IntervalStart && !StartKnownMove(2,TestNow+4.99) && Count(TEXT("known_move_gate_end_cooldown"))==EndCooldownBefore+1);
 Reset();KnownMoveAfter.Add(2,TestNow+1.);const int32 IntervalBefore=Count(TEXT("known_move_gate_search_interval"));
 Check(TEXT("move_interval_diagnostic"),!StartKnownMove(2,TestNow+.5) && Count(TEXT("known_move_gate_search_interval"))==IntervalBefore+1);
 Check(TEXT("move_no_partial_geometry"),Count(TEXT("known_move_reservation_violation"))==0 && KnownMoveReservedID==INDEX_NONE);
 if(BattleKnownFire::MoveCommit()>0.)
 {
  // Expectations come from the requested settings, not the implementation's deadline/getters.
  double ExpectedCommitSeconds=12.,ExpectedWaitSeconds=1.5;
  FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveCommit="),ExpectedCommitSeconds);
  FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveWait="),ExpectedWaitSeconds);
  // Expire the same fresh sample after departure; do not refresh it in the frame loop.
  for(bool CommitMarch:{false,true})for(bool HideAtArrival:{false,true})
  {
   if(CommitMarch && !BattleKnownFire::MoveMarch())continue;
   Reset();
   if(CommitMarch)
   {
    Squad.Order=EBattleOrder::AttackMove;Shooter.Order=EBattleOrder::AttackMove;Shooter.Destination=Start+FVector(0,3000,0);Squad.Destination=Shooter.Destination;
    auto& CommitPlan=Plans.FindChecked(Shooter.SquadID);CommitPlan.Path={Start,Shooter.Destination};Bodies.FindChecked(2).Waypoint=1;
   }
   const bool CommitStarted=TryStart(true);const int32 CommitShots=Shooter.ShotSequence;
   const int32 CommitWalksBefore=Count(TEXT("known_move_commit_walks")),CommitArrivalsBefore=Count(TEXT("known_move_commit_arrived"));
   const int32 CommitFiredBefore=Count(TEXT("known_move_commit_fired")),CommitUnseenBefore=Count(TEXT("known_move_arrived_unseen"));
   FVector CommitShelter=Start;bool CommitArrived=false,CommitNoEarlyShot=true,CommitExpired=false;
   double CommitPeekAt=-1.,CommitReturnAt=-1.;bool CommitWaitExact=true;
   if(CommitStarted)CommitShelter=(KnownMoves.FindChecked(2).Source<3 || KnownMoves.FindChecked(2).bStandUp)?KnownMoves.FindChecked(2).Goal:Start;
   for(int32 CommitFrame=0;CommitFrame<900 && KnownMoves.Contains(2);++CommitFrame)
   {
    TestNow+=1./30.;StepKnownMove(2,1.f/30.f,TestNow);
    if(const auto* CommitEpisode=KnownMoves.Find(2))
    {
     CommitExpired |= !CommitEpisode->Observation.Fresh(TestNow);
     if(CommitEpisode->bAtPeek && CommitPeekAt<0.)CommitPeekAt=TestNow;
     if(CommitEpisode->bReturning && CommitReturnAt<0.)CommitReturnAt=TestNow;
     if(HideAtArrival && CommitPeekAt>=0. && TestNow<=CommitPeekAt+ExpectedWaitSeconds)CommitWaitExact &= !CommitEpisode->bReturning;
     if(CommitEpisode->Arrived>=0)
     {
      if(!CommitArrived && HideAtArrival)Put(251,Start+FVector(50000,0,0));
      CommitArrived=true;
     }
     else CommitNoEarlyShot &= Shooter.ShotSequence==CommitShots;
    }
   }
   const bool CommitCommon=CommitStarted && CommitArrived && CommitExpired && CommitNoEarlyShot && !KnownMoves.Contains(2) &&
    Count(TEXT("known_move_commit_walks"))==CommitWalksBefore+1 && Count(TEXT("known_move_commit_arrived"))==CommitArrivalsBefore+1;
   const TCHAR* CommitCase=CommitMarch?(HideAtArrival?TEXT("move_commit_march_unseen_return"):TEXT("move_commit_march_arrive_fire")):
    HideAtArrival?TEXT("move_commit_unseen_return"):TEXT("move_commit_arrive_fire");
   Check(CommitCase,CommitCommon && (HideAtArrival?
    Shooter.ShotSequence==CommitShots && Count(TEXT("known_move_arrived_unseen"))==CommitUnseenBefore+1 &&
    Count(TEXT("known_move_commit_fired"))==CommitFiredBefore && FVector::Dist2D(Position(2),CommitShelter)<40. && !Shooter.CoverPhase &&
    CommitWaitExact && CommitPeekAt>=0. && CommitReturnAt>CommitPeekAt+ExpectedWaitSeconds && CommitReturnAt<=CommitPeekAt+ExpectedWaitSeconds+1./30.+.0001:
    Shooter.ShotSequence>CommitShots && Count(TEXT("known_move_commit_fired"))==CommitFiredBefore+1));
  }
  Reset();const bool TimeoutStarted=TryStart(true);const int32 TimeoutBefore=Count(TEXT("known_move_commit_timeout")),TimeoutShots=Shooter.ShotSequence;
  bool TimeoutContinued=false,TimeoutValue=false;
  if(TimeoutStarted)
  {
   const double ExpectedDeadline=KnownMoves.FindChecked(2).Observation.ObservedAt+ExpectedCommitSeconds;
   TimeoutValue=FMath::Abs(KnownMoves.FindChecked(2).CommitUntil-ExpectedDeadline)<.0001;
   TestNow=ExpectedDeadline-.01;StepKnownMove(2,0.f,TestNow);
   TimeoutContinued=KnownMoves.Contains(2) && CoverApproaches.Contains(2) && SoldierPaths.Contains(2) && Count(TEXT("known_move_commit_timeout"))==TimeoutBefore;
   TestNow=ExpectedDeadline+.01;StepKnownMove(2,0.f,TestNow);
  }
  Check(TEXT("move_commit_timeout"),TimeoutStarted && TimeoutValue && TimeoutContinued && !KnownMoves.Contains(2) && !CoverApproaches.Contains(2) && !SoldierPaths.Contains(2) &&
   Shooter.ShotSequence==TimeoutShots && Count(TEXT("known_move_commit_timeout"))==TimeoutBefore+1);
  auto ArrivalScene=[&]()
  {
   Reset();
   for(auto* ArrivalWall:Walls){ArrivalWall->SetActorEnableCollision(false);ArrivalWall->Destroy();}Walls.Reset();
   FKnownMove ArrivalEpisode;ArrivalEpisode.Start=Start;ArrivalEpisode.Goal=Start;ArrivalEpisode.Peek=Start;ArrivalEpisode.Last=Start;
   ArrivalEpisode.Observation=TestSense->TeamKnownTargets[Shooter.Team].Targets.FindChecked(251);
   ArrivalEpisode.Revision=Shooter.OrderRevision;ArrivalEpisode.Mission=Squad.MissionRevision;
   ArrivalEpisode.Started=TestNow;ArrivalEpisode.Arrived=TestNow;ArrivalEpisode.CommitUntil=TestNow+BattleKnownFire::MoveCommit();
   KnownMoves.Add(2,ArrivalEpisode);Shooter.bWeaponAimInitialized=false;
   if(auto* ArrivalActor=Actor(2))ArrivalActor->SetActorRotation(FRotator::ZeroRotator);
   else Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Bodies.FindChecked(2).Entity).Forward=FVector::ForwardVector;
  };
  ArrivalScene();Wall(Start+FVector(750,0,60),FVector(30,150,150));
  FVector WallEnd=FVector::ZeroVector;const auto WallSensor=Sight(2);
  const bool WallInView=PrepareSightTrace(WallSensor,251,WallEnd);
  const bool WallOccludes=WallInView && !SightHitVisible(TraceSight(WallSensor,WallEnd,251,SightFromCanSee),251);
  const int32 WallShots=Shooter.ShotSequence,WallRays=Count(TEXT("known_fire_sensor_rays"));
  StepKnownMove(2,0.f,TestNow);
  Check(TEXT("move_commit_sensor_wall"),WallInView && WallOccludes && Shooter.ShotSequence==WallShots &&
   Count(TEXT("known_fire_sensor_rays"))==WallRays+1 && !KnownMoves.FindChecked(2).bArrivalSeen);

  // A side-exposed eye sees around this wall; the actual body/muzzle remains behind it.
  ArrivalScene();auto& MuzzleEpisode=KnownMoves.FindChecked(2);MuzzleEpisode.bAtPeek=true;MuzzleEpisode.CheckUntil=TestNow+BattleKnownFire::MoveWait();
  Shooter.CoverPhase=2;Shooter.CoverAnchor=Start;Shooter.CoverPeek=Start+FVector(0,200,0);
  BlindFireUntil.Add(2,TestNow+10.);BlindFireSide.Add(2,Start+FVector(0,200,60));
  Wall(Start+FVector(250,0,60),FVector(20,70,150));
  const auto MuzzleSensor=Sight(2);FVector MuzzleEnd=FVector::ZeroVector;
  const bool MuzzleInView=PrepareSightTrace(MuzzleSensor,251,MuzzleEnd);
  auto BlockedMuzzleSensor=MuzzleSensor;BlockedMuzzleSensor.Eye=Position(2)+FVector(0,0,BattleStance::EyeOffset(StanceOf(2)));
  const bool SeparateMuzzle=MuzzleInView && SightHitVisible(TraceSight(MuzzleSensor,MuzzleEnd,251,SightFromCanSee),251) &&
   !SightHitVisible(TraceSight(BlockedMuzzleSensor,MuzzleEnd,251,SightFromCanSee),251);
  const int32 MuzzleShots=Shooter.ShotSequence,MuzzleRays=Count(TEXT("known_fire_aim_rays"));
  StepKnownMove(2,0.f,TestNow);
  Check(TEXT("move_commit_muzzle_wall"),SeparateMuzzle && Shooter.ShotSequence==MuzzleShots &&
   KnownMoves.FindChecked(2).bArrivalSeen && Count(TEXT("known_fire_aim_rays"))==MuzzleRays+1);
  BlindFireUntil.Remove(2);BlindFireSide.Remove(2);

  ArrivalScene();Put(251,Enemy+FVector(0,400,0));
  FVector MovedEnd=FVector::ZeroVector;const bool MovedInView=PrepareSightTrace(Sight(2),251,MovedEnd);
  const FVector MovedMuzzle=Position(2)+FVector(0,0,BattleStance::EyeOffset(StanceOf(2)));
  const FVector ExpectedMovedAim=(MovedEnd-MovedMuzzle).GetSafeNormal();const int32 MovedShots=Shooter.ShotSequence;
  StepKnownMove(2,0.f,TestNow);
  Check(TEXT("move_commit_current_aim"),MovedInView && !MovedEnd.Equals(Enemy+FVector(0,0,TargetAimOffset(251)),1.) &&
   KnownMoves.FindChecked(2).Observation.LastKnownPosition==Enemy && KnownMoves.FindChecked(2).bArrivalSeen &&
   Shooter.ShotSequence==MovedShots+1 && Shooter.WeaponAimDirection.Equals(ExpectedMovedAim,.0001));

  // Use real deferral at the head of the FIFO; the next request is less than .5 seconds old.
  for(bool QueueCancel:{false,true})
  {
   ArrivalScene();const double QueuePeekAt=TestNow;Shooter.NextShotTime=TestNow+10.;StepKnownMove(2,0.f,TestNow);Shooter.NextShotTime=0.;
   TestNow=QueuePeekAt+ExpectedWaitSeconds-.01;
   auto& QueuedBudget=KnownFireBudgets[Shooter.Team];QueuedBudget.Window=FMath::FloorToInt64(TestNow/.1);QueuedBudget.Used=BattleKnownFire::MoveRayBudget();
   StepKnownMove(2,0.f,TestNow);
   const bool WasQueued=QueuedBudget.Waiting.Num()==1 && QueuedBudget.Waiting[0]==2 && QueuedBudget.RequestedAt.Contains(2);
   const bool NextDeferred=!KnownFireRay(11,TestNow);
   const bool WaitContinued=KnownMoves.Contains(2) && !KnownMoves.FindChecked(2).bReturning;
   TestNow=QueuePeekAt+ExpectedWaitSeconds+.01;
   if(QueueCancel)++Shooter.OrderRevision;
   StepKnownMove(2,0.f,TestNow);
   const bool ReleasedQueue=!QueuedBudget.Waiting.Contains(2) && !QueuedBudget.RequestedAt.Contains(2);
   const bool EndedAsExpected=QueueCancel?!KnownMoves.Contains(2):KnownMoves.Contains(2) && KnownMoves.FindChecked(2).bReturning;
   TestNow+=.11;const bool NextGranted=KnownFireRay(11,TestNow);
   Check(QueueCancel?TEXT("move_commit_cancel_fifo"):TEXT("move_commit_wait_fifo"),WasQueued && NextDeferred && WaitContinued && ReleasedQueue && EndedAsExpected && NextGranted);
  }

  // Exercise an Actor, not the Mass follower used by the walking fixtures. No world tick is needed for the swept final step.
  ArrivalScene();FActorSpawnParameters SpeedSpawn;SpeedSpawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* SpeedActor=TestWorld->SpawnActor<ABattleUnit>(Start,FRotator::ZeroRotator,SpeedSpawn);
  bool ActorSpeedOK=false;
  if(SpeedActor)
  {
   SpeedActor->InitializeUnit(2);auto& SpeedBody=Bodies.FindChecked(2);
   const auto SavedSpeedActor=SpeedBody.Actor;const auto SavedSpeedCollision=SpeedBody.Capsule->GetCollisionEnabled();
   SpeedBody.Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);SpeedBody.Actor=SpeedActor;
   auto* SpeedMovement=SpeedActor->GetCharacterMovement();const float ExpectedSpeed=GetDefault<UBattleTuning>()->MoveSpeed;
   SpeedMovement->MaxWalkSpeed=ExpectedSpeed;Shooter.NextShotTime=TestNow+10.;
   KnownMoves.FindChecked(2).Peek=Start+FVector(0,2,0);
   StepKnownMove(2,1.f/30.f,TestNow);
   ActorSpeedOK=SpeedMovement->MaxWalkSpeed==ExpectedSpeed && FVector::Dist2D(Position(2),Start+FVector(0,2,0))<1.;
   TestNow+=1./30.;StepKnownMove(2,1.f/30.f,TestNow);
   ActorSpeedOK &= KnownMoves.FindChecked(2).bAtPeek && SpeedMovement->MaxWalkSpeed==ExpectedSpeed;
   TestNow+=BattleKnownFire::MoveWait()+.01;StepKnownMove(2,1.f/30.f,TestNow);
   ActorSpeedOK &= KnownMoves.FindChecked(2).bReturning;
   for(int32 SpeedFrame=0;SpeedFrame<4 && KnownMoves.Contains(2);++SpeedFrame){TestNow+=1./30.;StepKnownMove(2,1.f/30.f,TestNow);}
   ActorSpeedOK &= !KnownMoves.Contains(2) && SpeedMovement->MaxWalkSpeed==ExpectedSpeed && FVector::Dist2D(Position(2),Start)<1.;
   // A cancellation during the final 2 cm must also leave normal movement speed intact.
   FKnownMove CancelSpeedEpisode;CancelSpeedEpisode.Start=Start;CancelSpeedEpisode.Goal=Start;CancelSpeedEpisode.Peek=Start+FVector(0,2,0);
   CancelSpeedEpisode.Observation=TestSense->TeamKnownTargets[Shooter.Team].Targets.FindChecked(251);
   CancelSpeedEpisode.Revision=Shooter.OrderRevision;CancelSpeedEpisode.Mission=Squad.MissionRevision;
   CancelSpeedEpisode.Arrived=TestNow;CancelSpeedEpisode.CommitUntil=TestNow+BattleKnownFire::MoveCommit();KnownMoves.Add(2,CancelSpeedEpisode);
   StepKnownMove(2,1.f/30.f,TestNow);++Shooter.OrderRevision;StepKnownMove(2,0.f,TestNow);
   ActorSpeedOK &= !KnownMoves.Contains(2) && SpeedMovement->MaxWalkSpeed==ExpectedSpeed;
   SpeedBody.Actor=SavedSpeedActor;SpeedBody.Capsule->SetCollisionEnabled(SavedSpeedCollision);SpeedActor->Destroy();
  }
  Check(TEXT("move_commit_actor_speed"),ActorSpeedOK);
  Reset();const bool RefreshStarted=TryStart();bool RefreshKeptPlan=false,TrackKeptPlan=false;
  const int32 RefreshBefore=Count(TEXT("known_move_refreshed"));
  if(RefreshStarted)
  {
   const FVector RefreshGoal=KnownMoves.FindChecked(2).Goal;const double RefreshDeadline=KnownMoves.FindChecked(2).CommitUntil;
   TestNow+=.1;auto& RefreshReport=TestSense->TeamKnownTargets[Shooter.Team].Targets.FindChecked(251);
   RefreshReport.ObservedAt=TestNow;RefreshReport.LastKnownPosition=Enemy+FVector(0,25,0);RefreshReport.ExpiresAt=TestNow+5.;
   StepKnownMove(2,0.f,TestNow);
   RefreshKeptPlan=KnownMoves.Contains(2) && KnownMoves.FindChecked(2).Observation.LastKnownPosition==RefreshReport.LastKnownPosition &&
    KnownMoves.FindChecked(2).Goal==RefreshGoal && KnownMoves.FindChecked(2).CommitUntil==RefreshDeadline;
   TestNow+=.1;RefreshReport.TrackSampleAt=TestNow;RefreshReport.TrackUntil=TestNow+2.;RefreshReport.TrackedPosition=Enemy+FVector(0,50,0);
   StepKnownMove(2,0.f,TestNow);
   TrackKeptPlan=KnownMoves.Contains(2) && KnownMoves.FindChecked(2).Observation.At(TestNow)==RefreshReport.TrackedPosition &&
    KnownMoves.FindChecked(2).Goal==RefreshGoal && KnownMoves.FindChecked(2).CommitUntil==RefreshDeadline;
   StepKnownMove(2,0.f,TestNow); // Republishing the same sample must not count as a refresh.
  }
  Check(TEXT("move_commit_refresh"),RefreshStarted && RefreshKeptPlan && TrackKeptPlan && Count(TEXT("known_move_refreshed"))==RefreshBefore+2 && Count(TEXT("known_move_switched"))==0);
 }
 // Additional scene only; all preceding checks and the stage 1 fixture are unchanged.
 auto* EdgeCover=TestWorld->GetSubsystem<UBattleCover>();
 auto* EdgeCube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 int32 EdgeCrates=0;
 auto SetupEdgeScene=[&]()
 {
  Reset();
  for(auto* EdgeOldWall:Walls){EdgeOldWall->SetActorEnableCollision(false);EdgeOldWall->Destroy();}Walls.Reset();
  Wall(Start+FVector(250,0,60),FVector(30,900,250));
  EdgeCrates=0;
  // Nearby real slots from small crates behind the shooter; they cannot lean past the long wall.
  for(double CrateBack:{-300.,-600.})for(double CrateSide:{-450.,-150.,150.,450.})
  {
   auto* Crate=TestWorld->SpawnActor<AStaticMeshActor>();
   if(!Crate || !EdgeCube)continue;
   Crate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Crate->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
   Crate->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
   Crate->GetStaticMeshComponent()->SetStaticMesh(EdgeCube);Crate->SetActorScale3D(FVector(1.,1.,1.));
   Crate->SetActorLocation(Start+FVector(CrateBack,CrateSide,-40.));
   Crate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);Walls.Add(Crate);++EdgeCrates;
  }
  EdgeCover->BuildSlots(true);EdgeCover->KnownMoveGeometry(Start,BattleKnownFire::MoveRange(),true);
 };
 SetupEdgeScene();
 TArray<int32> EdgeOldSlots=EdgeCover->NearbySlots(Start,float(BattleKnownFire::MoveRange()),24);
 EdgeOldSlots.Sort([&](int32 LeftSlot,int32 RightSlot){const double LeftDistance=FVector::DistSquared2D(EdgeCover->GetSlots()[LeftSlot],Enemy),RightDistance=FVector::DistSquared2D(EdgeCover->GetSlots()[RightSlot],Enemy);return LeftDistance==RightDistance?LeftSlot<RightSlot:LeftDistance<RightDistance;});
 if(EdgeOldSlots.Num()>4)EdgeOldSlots.SetNum(4,EAllowShrinking::No);
 bool OldPlacesBlocked=EdgeCube && EdgeCrates==8 && EdgeOldSlots.Num()==4;
 for(int32 OldSlot:EdgeOldSlots)
 {
  TestNow+=.2;const bool EdgeReserved=KnownMoveReserve(2,TestNow);FVector OldPeek=FVector::ZeroVector;uint8 OldKind=0;
  OldPlacesBlocked &= EdgeReserved && !CoverFirePoint(2,EdgeCover->GetSlots()[OldSlot]+FVector(0,0,90),Enemy,OldPeek,OldKind,3,false,TestNow) && !bKnownMoveDeferred;
  KnownMoveRelease(2);
 }
 for(double OldForward:{0.,.6})for(double OldSide:{-1.,1.})
 {
  TestNow+=.2;const bool OldReserved=KnownMoveReserve(2,TestNow);
  const FVector OldFree=Start+FVector(OldForward,OldSide,0).GetSafeNormal2D()*500.;
  OldPlacesBlocked &= OldReserved && !KnownMoveLine(2,OldFree+FVector(0,0,60),Enemy+FVector(0,0,60),TestNow);KnownMoveRelease(2);
 }
 Check(TEXT("move_edge_old_places_blocked"),OldPlacesBlocked);
 const int32 EdgePicksBefore=Count(TEXT("known_move_pick_edge")),EdgeShotsBefore=Shooter.ShotSequence;
 int32 EdgeArrivalSensors=0;bool EdgeBudgetsOK=true,EdgeNoEarlyShot=true;
 const int32 EdgeFiredBefore=Count(TEXT("known_move_arrived_fired"));
 const bool EdgeStarted=TryStart();bool EdgeArrived=false,EdgeHiddenChecked=false,EdgeHiddenSilent=false;
 bool EdgeGoalMatches=false,EdgeRayClears=false;
 if(const auto* SelectedEdge=KnownMoves.Find(2))
 {
  const FBox ExpectedEdgeBox(Start+FVector(220,-900,-190),Start+FVector(280,900,310));
  const double SelectedEndY=Start.Y+SelectedEdge->EndSign*900.;
  EdgeGoalMatches=SelectedEdge->Source==0 && SelectedEdge->SourceBox.Min.Equals(ExpectedEdgeBox.Min,1.) &&
   SelectedEdge->SourceBox.Max.Equals(ExpectedEdgeBox.Max,1.) && SelectedEdge->FaceAxis==0 && SelectedEdge->FaceSign==-1. &&
   FMath::Abs(SelectedEdge->EndSign)==1. && SelectedEdge->Goal.X<ExpectedEdgeBox.Min.X &&
   FVector::Dist2D(SelectedEdge->Goal,FVector(ExpectedEdgeBox.Min.X,SelectedEndY,SelectedEdge->Goal.Z))<=200.;
  const FVector EdgeRayFrom=SelectedEdge->Peek+FVector(0,0,60),EdgeRayTo=Enemy+FVector(0,0,60);
  EdgeRayClears=EdgeGoalMatches && EdgeRayFrom.X<ExpectedEdgeBox.Min.X && EdgeRayTo.X>ExpectedEdgeBox.Max.X;
  for(double EdgePlaneX:{ExpectedEdgeBox.Min.X,ExpectedEdgeBox.Max.X})
  {
   const double EdgeRayDX=EdgeRayTo.X-EdgeRayFrom.X;
   const double EdgeRayT=FMath::Abs(EdgeRayDX)>1.? (EdgePlaneX-EdgeRayFrom.X)/EdgeRayDX : -1.;
   const FVector EdgeCrossing=FMath::Lerp(EdgeRayFrom,EdgeRayTo,EdgeRayT);
   // Cross outside the selected end at BOTH wall faces, below the top: no over-wall success.
   EdgeRayClears &= EdgeRayT>0. && EdgeRayT<1. && SelectedEdge->EndSign*(EdgeCrossing.Y-SelectedEndY)>1. &&
    EdgeCrossing.Z>ExpectedEdgeBox.Min.Z && EdgeCrossing.Z<ExpectedEdgeBox.Max.Z;
  }
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_EDGE_SELECTED source=%d box_min=%s box_max=%s face_axis=%d face_sign=%.0f end_sign=%.0f goal=%s peek=%s goal_matches=%d ray_clears=%d"),
   int32(SelectedEdge->Source),*SelectedEdge->SourceBox.Min.ToCompactString(),*SelectedEdge->SourceBox.Max.ToCompactString(),SelectedEdge->FaceAxis,SelectedEdge->FaceSign,SelectedEdge->EndSign,
   *SelectedEdge->Goal.ToCompactString(),*SelectedEdge->Peek.ToCompactString(),EdgeGoalMatches,EdgeRayClears);
 }
 Check(TEXT("move_edge_specified_goal"),EdgeStarted && EdgeGoalMatches);
 Check(TEXT("move_edge_specified_peek_ray"),EdgeStarted && EdgeRayClears);
 // Trial-only evidence from the actual body, including the hidden-target interval. No extra traces.
 auto FollowEdge=[&]()
 {
  const uint8 EdgePhaseBefore=Shooter.CoverPhase;
  const int32 EdgeSensorsBefore=Count(TEXT("known_fire_sensor_checks"));
  Follow(2,1.f/30.f,TestNow);
  if(Shooter.CoverPhase!=EdgePhaseBefore || Count(TEXT("known_fire_sensor_checks"))!=EdgeSensorsBefore)
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_EDGE_STEP now=%.3f phase=%d/%d pos=%s peek=%s remaining_cm=%.3f sensor_wall=%d aim_wall=%d range=%d fov=%d shots=%d"),
    TestNow,int32(EdgePhaseBefore),int32(Shooter.CoverPhase),*Position(2).ToCompactString(),*Shooter.CoverPeek.ToCompactString(),
    FVector::Dist2D(Position(2),Shooter.CoverPeek),Count(TEXT("known_fire_reject_sensor_wall")),Count(TEXT("known_fire_reject_aim_wall")),
    Count(TEXT("known_fire_reject_range")),Count(TEXT("known_fire_reject_fov")),Shooter.ShotSequence);
 };
 for(int32 EdgeFrame=0;EdgeFrame<900 && EdgeStarted;++EdgeFrame)
 {
  TestNow+=1./30.;Observe();FollowEdge();
  if(const auto* EdgeEpisode=KnownMoves.Find(2);EdgeEpisode && EdgeEpisode->Arrived>=0)EdgeArrived=true;
  if(!EdgeArrived)EdgeNoEarlyShot &= Shooter.ShotSequence==EdgeShotsBefore;
  EdgeBudgetsOK &= KnownFireBudgets[Shooter.Team].Used<=BattleKnownFire::MoveRayBudget() && MoveTeamBudget.Sweeps<=BattleKnownFire::MoveSweepBudget() && MoveTeamBudget.Nav<=BattleKnownFire::MoveNavBudget();
  if(EdgeArrived && !EdgeHiddenChecked)
  {
   EdgeArrivalSensors=Count(TEXT("known_fire_sensor_checks"));EdgeHiddenChecked=true;const int32 EdgeHiddenShots=Shooter.ShotSequence;Put(251,Start+FVector(50000,0,0));
   for(int32 EdgeHiddenFrame=0;EdgeHiddenFrame<15;++EdgeHiddenFrame){TestNow+=1./30.;Observe();FollowEdge();}
   EdgeHiddenSilent=Shooter.ShotSequence==EdgeHiddenShots;Put(251,Enemy);Observe();
  }
  if(Count(TEXT("known_move_arrived_fired"))>EdgeFiredBefore)break;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_EDGE_ARRIVE started=%d arrived=%d picks=%d/%d no_early=%d hidden_checked=%d hidden_silent=%d sensors=%d/%d shots=%d/%d fired=%d/%d"),EdgeStarted,EdgeArrived,Count(TEXT("known_move_pick_edge")),EdgePicksBefore+1,EdgeNoEarlyShot,EdgeHiddenChecked,EdgeHiddenSilent,Count(TEXT("known_fire_sensor_checks")),EdgeArrivalSensors,Shooter.ShotSequence,EdgeShotsBefore,Count(TEXT("known_move_arrived_fired")),EdgeFiredBefore+1);
 Check(TEXT("move_edge_arrive_personal_fire"),EdgeStarted && EdgeArrived && Count(TEXT("known_move_pick_edge"))==EdgePicksBefore+1 &&
  EdgeNoEarlyShot && EdgeHiddenChecked && EdgeHiddenSilent && Count(TEXT("known_fire_sensor_checks"))>EdgeArrivalSensors && Shooter.ShotSequence>EdgeShotsBefore && Count(TEXT("known_move_arrived_fired"))==EdgeFiredBefore+1);
 Check(TEXT("move_edge_budget"),EdgeBudgetsOK && Count(TEXT("known_move_reservation_violation"))==0 && KnownMoveReservedID==INDEX_NONE &&
  KnownFireBudgets[Shooter.Team].Used<=BattleKnownFire::MoveRayBudget() && MoveTeamBudget.Sweeps<=BattleKnownFire::MoveSweepBudget() && MoveTeamBudget.Nav<=BattleKnownFire::MoveNavBudget());
 // Recreate exactly the same fixture; only edge/opening generation differs.
 SetupEdgeScene();
 {
  TGuardValue<bool> SkipEdgeCandidates(bKnownMoveTrialSkipEdges,true);
  const int32 ControlShots=Shooter.ShotSequence,ControlStarts=Count(TEXT("known_move_started"));
  const int32 ControlSearches=Count(TEXT("known_move_searches")),ControlEdges=Count(TEXT("known_move_candidate_edge"))+Count(TEXT("known_move_candidate_door"));
  bool ControlStill=true,ControlSilent=true,ControlNoMove=true;
  for(int32 ControlFrame=0;ControlFrame<900;++ControlFrame)
  {
   TestNow+=1./30.;Observe();Follow(2,1.f/30.f,TestNow);
   ControlStill &= FVector::Dist2D(Position(2),Start)<1.;
   ControlSilent &= Shooter.ShotSequence==ControlShots;
   ControlNoMove &= !KnownMoves.Contains(2) && !CoverApproaches.Contains(2);
  }
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_EDGE_CONTROL searches=%d starts=%d shots=%d still=%d no_move=%d"),
   Count(TEXT("known_move_searches"))-ControlSearches,Count(TEXT("known_move_started"))-ControlStarts,Shooter.ShotSequence-ControlShots,ControlStill,ControlNoMove);
  Check(TEXT("move_edge_disabled_no_move_fire"),EdgeCube && EdgeCrates==8 && Count(TEXT("known_move_searches"))>ControlSearches &&
   Count(TEXT("known_move_candidate_edge"))+Count(TEXT("known_move_candidate_door"))==ControlEdges && Count(TEXT("known_move_started"))==ControlStarts && ControlNoMove && ControlStill && ControlSilent);
 }
 // Defense marching uses the real Follow gate, with the same hidden-fire fixture.
 Reset();KnownDefendResumes.Reset();
 // The edge scene cached crates and a long wall; rebuild after removing them.
 TestWorld->GetSubsystem<UBattleCover>()->BuildSlots(true);
 TestWorld->GetSubsystem<UBattleCover>()->KnownMoveGeometry(Start,BattleKnownFire::MoveRange(),true);
 const FVector DefendPost=Start+FVector(0,3000,0);
 auto& DefendPlan=Plans.FindChecked(Shooter.SquadID);DefendPlan.Path={Start,DefendPost};
 DefendPlan.Corridors.Add(2,DefendPlan.Path);DefendPlan.ArrivalGoals.Add(2,DefendPost);Bodies.FindChecked(2).Waypoint=1;
 Shooter.Destination=DefendPost;Squad.Destination=DefendPost;Squad.DefenseRadius=3400;
 const int32 DefendRevision=Shooter.OrderRevision,DefendMission=Squad.MissionRevision;
 const int32 DefendStarts=Count(TEXT("known_move_defend_march_started")),DefendArrivals=Count(TEXT("known_move_defend_march_arrived"));
 const int32 DefendFires=Count(TEXT("known_move_defend_march_fired")),DefendResumes=Count(TEXT("known_move_defend_march_resumed"));
 const int32 DefendPostArrivals=Count(TEXT("known_move_defend_march_resume_arrived")),DefendShots=Shooter.ShotSequence;
 const int32 DefendSearches=Count(TEXT("known_move_searches")),DefendRays=Count(TEXT("known_move_rays"));
 const int32 DefendSensors=Count(TEXT("known_fire_sensor_checks"));
 bool DefendStarted=false,DefendNoEarlyShot=true;
 for(int32 DefendAttempt=0;DefendAttempt<24 && !DefendStarted;++DefendAttempt)
 {TestNow+=1.01;Observe();Follow(2,0.f,TestNow);DefendStarted=KnownMoves.Contains(2);}
 if(!BattleKnownFire::MoveDefendMarch())
 {
  Check(TEXT("move_defend_march_disabled"),!DefendStarted && Count(TEXT("known_move_defend_march_started"))==DefendStarts &&
   Count(TEXT("known_move_searches"))==DefendSearches && Count(TEXT("known_move_rays"))==DefendRays && Shooter.ShotSequence==DefendShots && FVector::Dist2D(Position(2),Start)<1.);
  bool DefendDisabledStraight=true;double DefendDisabledRemaining=FVector::Dist2D(Position(2),DefendPost);
  for(int32 DefendDisabledFrame=0;DefendDisabledFrame<900 && DefendDisabledRemaining>100.;++DefendDisabledFrame)
  {
   TestNow+=1./30.;Observe();Follow(2,1.f/30.f,TestNow);
   const double DefendDisabledNext=FVector::Dist2D(Position(2),DefendPost);
   DefendDisabledStraight &= FMath::Abs(Position(2).X-Start.X)<1. && DefendDisabledNext<=DefendDisabledRemaining &&
    !KnownMoves.Contains(2) && !CoverApproaches.Contains(2) && !Shooter.CoverPhase;
   DefendDisabledRemaining=DefendDisabledNext;
  }
  Check(TEXT("move_defend_march_disabled_walk"),DefendDisabledStraight && DefendDisabledRemaining<=100. && FVector::Dist2D(Position(2),Start)>100. &&
   Count(TEXT("known_move_searches"))==DefendSearches && Count(TEXT("known_move_rays"))==DefendRays &&
   Count(TEXT("known_move_defend_march_started"))==DefendStarts && KnownMoveReservedID==INDEX_NONE &&
   Shooter.Order==EBattleOrder::Defend && Squad.Order==EBattleOrder::Defend && Shooter.OrderRevision==DefendRevision &&
   Squad.MissionRevision==DefendMission && Shooter.Destination==DefendPost && Squad.Destination==DefendPost);
 }
 else
 {
  for(int32 DefendFrame=0;DefendFrame<900 && KnownMoves.Contains(2);++DefendFrame)
  {
   TestNow+=1./30.;Observe();Follow(2,1.f/30.f,TestNow);
   if(const auto* DefendEpisode=KnownMoves.Find(2);DefendEpisode && DefendEpisode->Arrived<0)DefendNoEarlyShot &= Shooter.ShotSequence==DefendShots;
  }
  Check(TEXT("move_defend_march_arrive_fire"),DefendStarted && DefendNoEarlyShot && Shooter.ShotSequence>DefendShots && Count(TEXT("known_fire_sensor_checks"))>DefendSensors &&
   Count(TEXT("known_move_defend_march_started"))==DefendStarts+1 && Count(TEXT("known_move_defend_march_arrived"))==DefendArrivals+1 && Count(TEXT("known_move_defend_march_fired"))==DefendFires+1);
  TestSense->Reset();Put(251,Start+FVector(50000,0,0));
  for(int32 DefendResumeFrame=0;DefendResumeFrame<900;++DefendResumeFrame)
  {TestNow+=1./30.;Follow(2,1.f/30.f,TestNow);if(Count(TEXT("known_move_defend_march_resume_arrived"))>DefendPostArrivals)break;}
  Check(TEXT("move_defend_march_resume"),!KnownMoves.Contains(2) && !CoverApproaches.Contains(2) && !Shooter.CoverPhase &&
   Count(TEXT("known_move_defend_march_resumed"))==DefendResumes+1 && Count(TEXT("known_move_defend_march_resume_arrived"))==DefendPostArrivals+1 &&
   FVector::Dist2D(Position(2),DefendPost)<=100. && Shooter.Order==EBattleOrder::Defend && Squad.Order==EBattleOrder::Defend &&
   Shooter.OrderRevision==DefendRevision && Squad.MissionRevision==DefendMission && Shooter.Destination==DefendPost && Squad.Destination==DefendPost);
  // Start outside the assigned sector and defense disk; crossing a cell border is legal only in the strip.
  Reset();KnownDefendResumes.Reset();Squad.bSectorMission=true;Squad.DefenseRadius=400;
  const FVector DefendBoundaryStart(-100,100,Start.Z),DefendBoundaryPost(2000,100,Start.Z);
  Put(2,DefendBoundaryStart);Shooter.Destination=DefendBoundaryPost;Squad.Destination=DefendBoundaryPost;Plans.FindChecked(Shooter.SquadID).Center=DefendBoundaryStart;
  Plans.FindChecked(Shooter.SquadID).Path={DefendBoundaryStart,DefendBoundaryPost};Bodies.FindChecked(2).Waypoint=1;
  Check(TEXT("move_defend_march_sector"),BattleSectors::Cell(DefendBoundaryStart)!=BattleSectors::Cell(DefendBoundaryPost) &&
   KnownMovePlaceAllowed(2,DefendBoundaryStart,DefendBoundaryStart,true) && KnownMovePlaceAllowed(2,DefendBoundaryStart,DefendBoundaryStart+FVector(500,0,0),true) &&
   KnownMovePlaceAllowed(2,DefendBoundaryStart,DefendBoundaryPost,true) && !KnownMovePlaceAllowed(2,DefendBoundaryStart,DefendBoundaryStart+FVector(500,700,0),true));
  Check(TEXT("move_defend_march_reverse_rejected"),!KnownMovePlaceAllowed(2,DefendBoundaryStart,DefendBoundaryStart-FVector(700,0,0),true) &&
   !KnownMovePlaceAllowed(2,DefendBoundaryStart,DefendBoundaryStart-FVector(1,0,0),true));
  // Expired information must eventually release the detour without changing the post.
  Reset();KnownDefendResumes.Reset();auto& DefendStalePlan=Plans.FindChecked(Shooter.SquadID);
  DefendStalePlan.Path={Start,DefendPost};DefendStalePlan.Corridors.Add(2,DefendStalePlan.Path);DefendStalePlan.ArrivalGoals.Add(2,DefendPost);Bodies.FindChecked(2).Waypoint=1;
  Shooter.Destination=DefendPost;Squad.Destination=DefendPost;Squad.DefenseRadius=3400;
  bool DefendStaleStarted=false;
  for(int32 DefendStaleAttempt=0;DefendStaleAttempt<24 && !DefendStaleStarted;++DefendStaleAttempt)
  {TestNow+=1.01;Observe();Follow(2,0.f,TestNow);DefendStaleStarted=KnownMoves.Contains(2);}
  const int32 DefendStaleShots=Shooter.ShotSequence,DefendStaleResumes=Count(TEXT("known_move_defend_march_resumed"));
  const int32 DefendStaleArrivals=Count(TEXT("known_move_defend_march_resume_arrived"));
  const int32 DefendStaleRevision=Shooter.OrderRevision,DefendStaleMission=Squad.MissionRevision;
  TestSense->Reset();Put(251,Start+FVector(50000,0,0));
  for(int32 DefendStaleFrame=0;DefendStaleFrame<2400;++DefendStaleFrame)
  {TestNow+=1./30.;Follow(2,1.f/30.f,TestNow);if(Count(TEXT("known_move_defend_march_resume_arrived"))>DefendStaleArrivals)break;}
  bool DefendStaleSlotsReleased=true;
  for(int32 DefendStaleSlot=0;DefendStaleSlot<EdgeCover->GetSlots().Num();++DefendStaleSlot)
   DefendStaleSlotsReleased &= EdgeCover->Owner(DefendStaleSlot,0.)!=2; // Detect leaked reservations even if their lease expired.
  Check(TEXT("move_defend_march_stale_resume"),DefendStaleSlotsReleased && DefendStaleStarted && !KnownMoves.Contains(2) && Shooter.ShotSequence==DefendStaleShots &&
   Count(TEXT("known_move_defend_march_resumed"))==DefendStaleResumes+1 && Shooter.Destination==DefendPost && Squad.Destination==DefendPost &&
   Count(TEXT("known_move_defend_march_resume_arrived"))==DefendStaleArrivals+1 && FVector::Dist2D(Position(2),DefendPost)<=100. &&
   !KnownDefendResumes.Contains(2) && !CoverApproaches.Contains(2) && !Shooter.CoverPhase && KnownMoveReservedID==INDEX_NONE &&
   Shooter.Order==EBattleOrder::Defend && Squad.Order==EBattleOrder::Defend && Shooter.OrderRevision==DefendStaleRevision && Squad.MissionRevision==DefendStaleMission);
  // A squad target stops this soldier, although his own view is blocked and the route remains.
  // The small defense disk is beyond MoveRange: the marching strip must not admit a detour.
  Reset();KnownDefendResumes.Reset();
  auto& DefendStoppedPlan=Plans.FindChecked(Shooter.SquadID);
  DefendStoppedPlan.Path={Start,DefendPost};DefendStoppedPlan.Corridors.Add(2,DefendStoppedPlan.Path);
  DefendStoppedPlan.ArrivalGoals.Add(2,DefendPost);Bodies.FindChecked(2).Waypoint=1;
  DefendStoppedPlan.TargetID=251;DefendStoppedPlan.bCoverStop=false;
  Shooter.Destination=DefendPost;Squad.Destination=DefendPost;Squad.DefenseRadius=400;
  const int32 DefendStoppedSearches=Count(TEXT("known_move_searches")),DefendStoppedStarts=Count(TEXT("known_move_defend_march_started"));
  const int32 DefendStoppedShots=Shooter.ShotSequence;bool DefendStoppedNoDetour=true;
  for(int32 DefendStoppedAttempt=0;DefendStoppedAttempt<24;++DefendStoppedAttempt)
  {
   TestNow+=1.01;Observe();Follow(2,0.f,TestNow);
   DefendStoppedNoDetour &= !KnownMoves.Contains(2) && !CoverApproaches.Contains(2) && KnownMoveReservedID==INDEX_NONE;
  }
  Check(TEXT("move_defend_march_combat_stop"),DefendStoppedNoDetour && Count(TEXT("known_move_searches"))>DefendStoppedSearches &&
   Count(TEXT("known_move_defend_march_started"))==DefendStoppedStarts && Shooter.ShotSequence==DefendStoppedShots &&
   FVector::Dist2D(Position(2),Start)<1. && !KnownDefendResumes.Contains(2) &&
   !KnownMovePlaceAllowed(2,Start,Start+FVector(0,500,0)));
 }
 for(auto* TestWall:Walls){TestWall->SetActorEnableCollision(false);TestWall->Destroy();}
 return Failures;
}
