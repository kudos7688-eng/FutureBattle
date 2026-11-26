#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleFieldLayout.h"
#include "BattlePerception.h"
#include "BattleStance.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunKnownTargetFireTrial()
{
 Mode->bCombatPaused=true;
 auto* TrialWorld=GetWorld();auto* TrialStore=TrialWorld->GetSubsystem<UBattleState>();auto* TrialSense=TrialWorld->GetSubsystem<UBattlePerception>();
 const double TrialNow=TrialWorld->GetTimeSeconds();int32 TrialFailures=0;
 auto TrialCheck=[&](const TCHAR* TrialName,bool TrialOK)
 {TrialFailures+=!TrialOK;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_CHECK name=%s ok=%d"),TrialName,TrialOK);};
 if(FCString::Strcmp(BattleFieldLayout::Of(TrialWorld).Name,TEXT("Lvl_BattleWide_C"))!=0 || !BattleKnownFire::Enabled() || !SightExperiment.Enabled || !Bodies.Contains(2) || !Bodies.Contains(11) || !Bodies.Contains(251) || !Bodies.Contains(252))
 {UE_LOG(LogFutureBattle,Error,TEXT("BATTLE_KNOWN_FIRE_RESULT failures=1 fixture"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
 // Map C, same open ground and low-wall anchor as RunCoverPeekTrial; all scene walls are relative to it.
 // Without BattleArea, 25 squads of 10 per team give allied IDs 2/11 and enemy IDs 251/252.
 FVector TrialGround(-9595,-4000,0);FHitResult TrialFloor;
 const bool TrialHasFloor=TrialWorld->LineTraceSingleByObjectType(TrialFloor,TrialGround+FVector(0,0,500),TrialGround-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic));
 if(TrialHasFloor)TrialGround.Z=TrialFloor.ImpactPoint.Z;
 const FVector TrialStart=TrialGround+FVector(0,0,90),TrialFirst=TrialStart+FVector(1500,0,0),TrialSecond=TrialStart+FVector(1800,600,0);
 auto TrialPlace=[&](int32 TrialID,const FVector& TrialAt)
 {
  auto& TrialBody=Bodies.FindChecked(TrialID);
  if(TrialBody.Actor.IsValid())TrialBody.Actor->SetActorLocation(TrialAt,false,nullptr,ETeleportType::TeleportPhysics);
  else{TrialBody.Capsule->SetWorldLocation(TrialAt);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(TrialBody.Entity).Position=TrialAt;}
 };
 for(auto& TrialPair:TrialStore->Units)
 {
  TrialPair.Value.Ammo=0;TrialPair.Value.Reserve=0;TrialPair.Value.Perception.ShareRange=0;
  // Keep unrelated bodies from blocking the fixture's rays or moving its shooter through separation.
  if(Bodies.Contains(TrialPair.Key))TrialPlace(TrialPair.Key,FVector(8000+(TrialPair.Key%20)*120,-7000+(TrialPair.Key/20)*120,100));
 }
 auto& TrialShooter=TrialStore->Units.FindChecked(2);auto& TrialSquad=TrialStore->Squads.FindChecked(TrialShooter.SquadID);
 auto& TrialObserver=TrialStore->Units.FindChecked(11);
 TrialCheck(TEXT("fixture"),TrialHasFloor && TrialObserver.SquadID!=TrialShooter.SquadID && TrialObserver.Team==TrialShooter.Team && BattleStance::Enabled(TrialWorld) && BattleTactics::CoverActionsEnabled());
 TArray<AActor*> TrialWalls;
 auto TrialWall=[&](const FVector& TrialAt,const FVector& TrialExtent)
 {
  auto* TrialWallActor=TrialWorld->SpawnActor<AActor>();if(!TrialWallActor)return;
  auto* TrialBox=NewObject<UBoxComponent>(TrialWallActor);TrialWallActor->SetRootComponent(TrialBox);TrialWallActor->AddInstanceComponent(TrialBox);
  TrialBox->SetBoxExtent(TrialExtent);TrialBox->SetCollisionObjectType(ECC_WorldStatic);TrialBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);TrialBox->SetCollisionResponseToAllChannels(ECR_Block);
  TrialBox->SetCanEverAffectNavigation(false);TrialBox->SetWorldLocation(TrialAt);TrialBox->SetMobility(EComponentMobility::Static);TrialBox->RegisterComponent();TrialWalls.Add(TrialWallActor);
 };
 auto TrialReset=[&]()
 {
  for(auto* TrialOldWall:TrialWalls){TrialOldWall->SetActorEnableCollision(false);TrialOldWall->Destroy();}TrialWalls.Reset();
  CancelCoverAction(2);CoverPeekShots.Remove(2);TrialSense->Reset();KnownFireSearches.Reset();
  for(auto& TrialBudget:KnownFireBudgets)TrialBudget=BattleKnownFire::FBudget();
  TrialPlace(2,TrialStart);TrialPlace(251,TrialFirst);TrialPlace(252,TrialSecond);
  TrialPlace(11,TrialStart+FVector(0,-1500,0));
  for(int32 TrialID:{2,11,251,252}){auto& TrialUnit=TrialStore->Units.FindChecked(TrialID);TrialUnit.HP=100000;TrialUnit.CoverPhase=0;TrialUnit.Perception.MemorySeconds=20;SetStance(TrialID,BattleStance::Stand,TrialNow,true);}
  TrialShooter.Weapon=EBattleWeapon::Carbine;TrialShooter.bSidearmSelected=false;TrialShooter.bLauncherSelected=false;
  TrialShooter.Ammo=30;TrialShooter.Reserve=90;TrialShooter.ReloadEndTime=0;TrialShooter.NextShotTime=0;TrialShooter.bWeaponAimInitialized=false;
  Bodies.FindChecked(2).Velocity=FVector::ZeroVector;
  TrialShooter.Order=EBattleOrder::Hold;TrialShooter.bSoldierOrder=false;TrialShooter.Perception.SightRange=10000;TrialShooter.Perception.FieldOfView=120;
  TrialSquad.Order=EBattleOrder::Hold;TrialSquad.CommandStatus=EBattleCommandStatus::Active;TrialSquad.bSectorMission=false;TrialSquad.TacticalPlan.Action=EBattleSquadTactic::Mission;
  Plans.FindOrAdd(TrialShooter.SquadID)=FBattleLeaderPlan();
  Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Bodies.FindChecked(2).Entity).Forward=FVector::ForwardVector;
  Bodies.FindChecked(2).Capsule->SetWorldRotation(FRotator::ZeroRotator);WalkLooks.Remove(2);
 };
 auto TrialObserve=[&](int32 TrialTarget,const FVector& TrialPosition,double TrialAge,bool TrialTracked=false)
 {
  // Scoped observation fixture: the shooter never receives an own visible contact. Source is another squad.
  FBattleContact TrialContact;TrialContact.TargetID=TrialTarget;TrialContact.SourceUnitID=11;TrialContact.KnownSourceSquad=TrialObserver.SquadID;
  TrialContact.ObservedSquadType=0;TrialContact.LastKnownPosition=TrialPosition;TrialContact.LastSeenTime=TrialNow-TrialAge;
  TrialContact.KnownExpiresAt=TrialContact.LastSeenTime+20;TrialContact.bLongRangeObservation=false;TrialContact.bVisible=TrialAge==0;
  TrialContact.bTracked=TrialTracked;TrialContact.TrackedPosition=TrialSecond;TrialContact.KnownTrackSampleAt=TrialNow;
  TrialSense->Observers.FindOrAdd(11).Contacts.Add(TrialTarget,TrialContact);TrialSense->BuildTeamKnownTargets(TrialNow);
 };
 auto TrialPick=[&](){return SelectKnownFire(2,Position(2)+FVector(0,0,BattleStance::EyeOffset(StanceOf(2))),TrialNow);};
 auto TrialCount=[&](const TCHAR* TrialKey){return SightExperiment.CoverDiag.FindRef(FName(TrialKey));};

 TrialReset();TrialObserve(251,TrialFirst,0);
 const auto TrialOpen=TrialPick();const int32 TrialOpenShots=TrialShooter.ShotSequence;
 // Follow must select and actually fire; reset only the selection cache, not the observation fixture.
 KnownFireSearches.Reset();Follow(2,1.f/30.f,TrialNow);
 TrialCheck(TEXT("other_squad_open"),TrialOpen.Ready() && TrialOpen.TargetID==251 && TrialOpen.SourceUnitID==11 && !TrialSense->FindContact(2,251) && TrialShooter.ShotSequence==TrialOpenShots+1 && Position(2).Equals(TrialStart,.01));

 TrialReset();TrialObserve(251,TrialFirst,0);
 TrialWall(TrialGround+FVector(95,0,60),FVector(15,200,60));
 TrialShooter.CoverPhase=4;TrialShooter.CoverAnchor=TrialStart;TrialShooter.CoverPeek=TrialStart;TrialShooter.CoverTargetID=251;
 TrialShooter.CoverSlot=FreeCoverSlot;TrialShooter.CoverOrderRevision=TrialShooter.OrderRevision;TrialShooter.CoverSeconds=1;
 const int32 TrialPeekShots=TrialShooter.ShotSequence;const int32 TrialPeekBefore=TrialCount(TEXT("known_fire_shots_peek"));
 StepCoverAction(2,1.f/30.f,TrialNow);
 TrialCheck(TEXT("other_squad_peek"),TrialShooter.ShotSequence==TrialPeekShots+1 && TrialCount(TEXT("known_fire_shots_peek"))==TrialPeekBefore+1 && TrialShooter.CoverPhase==4 && Position(2).Equals(TrialStart,.01) && !TrialSense->FindContact(2,251));

 TrialReset();TrialObserve(251,TrialFirst,0);TrialObserve(252,TrialSecond,0);
 TrialWall(TrialGround+FVector(750,0,150),FVector(30,130,150));
 const int32 TrialWallBefore=TrialCount(TEXT("known_fire_reject_sensor_wall"));const auto TrialAlternative=TrialPick();
 const bool TrialAlternativeShot=FireKnownTarget(2,TrialStart+FVector(0,0,60),TrialAlternative,false);
 TrialCheck(TEXT("blocked_first_second_fires"),TrialAlternative.Ready() && TrialAlternative.TargetID==252 && TrialAlternativeShot && TrialCount(TEXT("known_fire_reject_sensor_wall"))==TrialWallBefore+1 && Position(2).Equals(TrialStart,.01));

 TrialReset();TrialObserve(251,TrialFirst,0);TrialObserve(252,TrialSecond,0);
 TrialWall(TrialGround+FVector(750,0,150),FVector(30,130,150));
 TrialShooter.CoverPhase=4;TrialShooter.CoverAnchor=TrialStart;TrialShooter.CoverPeek=TrialStart;TrialShooter.CoverTargetID=251;
 TrialShooter.CoverSlot=FreeCoverSlot;TrialShooter.CoverOrderRevision=TrialShooter.OrderRevision;TrialShooter.CoverSeconds=1;
 const int32 TrialSecondPeekShots=TrialShooter.ShotSequence;StepCoverAction(2,1.f/30.f,TrialNow);
 TrialCheck(TEXT("blocked_first_peek_second"),TrialShooter.CoverPhase==4 && TrialShooter.ShotSequence==TrialSecondPeekShots+1 && KnownFireSearches.FindChecked(2).Selected==252 && Position(2).Equals(TrialStart,.01));

 TrialReset();TrialObserve(251,TrialFirst,0);
 SetStance(251,BattleStance::Crouch,TrialNow,true);const auto TrialCrouchAim=TrialPick();
 KnownFireSearches.Reset();SetStance(251,BattleStance::Prone,TrialNow,true);const auto TrialProneAim=TrialPick();
 TrialCheck(TEXT("confirmed_stance_aim"),TrialCrouchAim.Ready() && TrialProneAim.Ready() &&
  TrialCrouchAim.AimPosition.Equals(TrialFirst+FVector(0,0,BattleStance::AimOffset(BattleStance::Crouch)),.01) &&
  TrialProneAim.AimPosition.Equals(TrialFirst+FVector(0,0,BattleStance::AimOffset(BattleStance::Prone)),.01));

 TrialReset();TrialObserve(251,TrialFirst,6);TrialSense->BuildTeamKnownTargets(TrialNow);
 const int32 TrialStaleRays=TrialCount(TEXT("known_fire_rays"));const int32 TrialStaleShots=TrialShooter.ShotSequence;
 const auto TrialStale=TrialPick();Follow(2,1.f/30.f,TrialNow);
 TrialCheck(TEXT("stale_no_revival"),!TrialStale.Ready() && TrialSense->TeamKnownTargets[TrialShooter.Team].Targets.Num()==0 && TrialCount(TEXT("known_fire_rays"))==TrialStaleRays && TrialShooter.ShotSequence==TrialStaleShots);

 TrialReset();TrialPlace(251,TrialSecond);TrialPlace(252,TrialStart+FVector(5000,3000,0));
 TrialObserve(251,TrialFirst,1,BattleTactics::SpotTrack()>0);
 const auto TrialTracking=TrialPick();const FVector TrialExpected=TrialSecond+FVector(0,0,35);
 TrialCheck(TEXT("track_aim"),TrialTracking.Ready() && TrialTracking.AimPosition.Equals(TrialExpected,.01) && TrialTracking.bTracked==(BattleTactics::SpotTrack()>0) && FireKnownTarget(2,TrialStart+FVector(0,0,60),TrialTracking,false));
 const auto TrialHeldAim=TrialPick();
 TrialCheck(TEXT("confirmed_aim_held"),!TrialHeldAim.Ready() && TrialHeldAim.AimPosition.Equals(TrialExpected,.01));
 TrialWall(TrialGround+FVector(750,250,150),FVector(30,400,150));KnownFireSearches.Reset();TrialShooter.NextShotTime=0;
 const auto TrialTrackBlocked=TrialPick();
 TrialCheck(TEXT("track_still_needs_sensor"),!TrialTrackBlocked.Ready() && TrialTrackBlocked.Reject==BattleKnownFire::EReject::SensorWall);

 TrialReset();TrialObserve(251,TrialFirst,0);auto& TrialBudget=KnownFireBudgets[TrialShooter.Team];
 bool TrialBudgetFilled=true;for(int32 TrialRay=0;TrialRay<32;++TrialRay)
 {
  const bool TrialToken=KnownFireRay(2,TrialNow);TrialBudgetFilled &= TrialToken;
  if(TrialToken){SightExperiment.Cover(TEXT("known_fire_sensor_rays"));TraceSight(Sight(2),TrialFirst+FVector(0,0,35),251,SightFromCanSee);}
 }
 const auto TrialDeferred=TrialPick();
 TrialCheck(TEXT("budget_deferred"),TrialBudgetFilled && !TrialDeferred.Ready() && TrialDeferred.Reject==BattleKnownFire::EReject::Deferred && TrialBudget.Used==32 && TrialBudget.Waiting.Contains(2));
 // New callers cannot jump ahead of an older deferred soldier, even across windows.
 const bool TrialNewCaller=KnownFireRay(11,TrialNow+.11);
 const auto TrialResumed=SelectKnownFire(2,TrialStart+FVector(0,0,60),TrialNow+.11);
 const bool TrialNextCaller=KnownFireRay(11,TrialNow+.11);
 if(TrialNextCaller){SightExperiment.Cover(TEXT("known_fire_sensor_rays"));TraceSight(Sight(11),TrialFirst+FVector(0,0,35),251,SightFromCanSee);}
 const bool TrialFIFOImmediate=!TrialNewCaller && TrialResumed.Ready() && TrialNextCaller && TrialBudget.Used==3;
 // A suspended second candidate must not restart at the first after the search interval.
 TrialReset();TrialObserve(251,TrialFirst,0);TrialObserve(252,TrialSecond,0);
 TrialWall(TrialGround+FVector(750,0,150),FVector(30,130,150));
 TrialBudget.Window=FMath::FloorToInt64(TrialNow/.1);TrialBudget.Used=31;
 const auto TrialPartial=TrialPick();const int32 TrialPartialCursor=KnownFireSearches.FindChecked(2).Cursor;
 const auto TrialLateResume=SelectKnownFire(2,TrialStart+FVector(0,0,60),TrialNow+.41);
 TrialCheck(TEXT("budget_fifo"),TrialFIFOImmediate && TrialPartial.Reject==BattleKnownFire::EReject::Deferred && TrialPartialCursor==1 &&
  TrialLateResume.Ready() && TrialLateResume.TargetID==252 && TrialBudget.Used==2);

 TrialReset();TrialObserve(251,TrialFirst,0);TrialSquad.Order=EBattleOrder::Move;
 const int32 TrialOrderRays=TrialCount(TEXT("known_fire_rays"));const auto TrialOrder=TrialPick();
 TrialCheck(TEXT("move_order"),!TrialOrder.Ready() && TrialCount(TEXT("known_fire_rays"))==TrialOrderRays);
 TrialReset();TrialObserve(251,TrialFirst,0);TrialSquad.TacticalPlan.Action=EBattleSquadTactic::Support;TrialSquad.TacticalPlan.SupportTarget=252;
 TrialCheck(TEXT("support_target"),!TrialPick().Ready());
 TrialReset();TrialObserve(251,TrialFirst,0);TrialShooter.Weapon=EBattleWeapon::AntiTank;
 const bool TrialWrongWeapon=!TrialPick().Ready();
 TrialShooter.Weapon=EBattleWeapon::Carbine;TrialShooter.Ammo=0;
 const int32 TrialReloadRays=TrialCount(TEXT("known_fire_rays"));const auto TrialEmpty=TrialPick();
 TrialCheck(TEXT("weapon_gate"),TrialWrongWeapon && !TrialEmpty.Ready() && TrialShooter.ReloadEndTime>TrialNow && TrialCount(TEXT("known_fire_rays"))==TrialReloadRays);

 TrialReset();TrialObserve(251,TrialFirst,0);
 TrialSense->Observers.FindChecked(11).Contacts.FindChecked(251).bLongRangeObservation=true;TrialSense->BuildTeamKnownTargets(TrialNow);
 TrialCheck(TEXT("unreported_scout"),!TrialPick().Ready() && TrialSense->TeamKnownTargets[TrialShooter.Team].Targets.Num()==0);
 TrialReset();TrialObserve(251,TrialFirst,0);
 TrialSense->Observers.FindChecked(11).Contacts.FindChecked(251).LastKnownPosition=TrialSecond;
 TrialSense->BuildTeamKnownTargets(TrialNow);TrialWall(TrialGround+FVector(750,250,150),FVector(30,100,150));
 const auto TrialAimWall=TrialPick();
 TrialCheck(TEXT("old_aim_wall_ignored"),TrialAimWall.Ready() && TrialAimWall.AimPosition.Equals(TrialFirst+FVector(0,0,35),.01));
 // Sensor at eye height can see above this obstacle, while the lower muzzle cannot.
 TrialReset();TrialObserve(251,TrialFirst,0);
 TrialWall(TrialGround+FVector(750,0,108),FVector(30,130,8));
 const auto TrialMuzzleWall=SelectKnownFire(2,TrialStart,TrialNow);
 TrialCheck(TEXT("aim_wall_separate"),!TrialMuzzleWall.Ready() && TrialMuzzleWall.Reject==BattleKnownFire::EReject::AimWall);

 TrialReset();TrialObserve(251,TrialFirst,0);
 auto& TrialDelivered=TrialSense->Observers.FindChecked(11).Contacts.FindChecked(251);
 TrialDelivered.bShared=true;TrialDelivered.bVisible=false;TrialDelivered.KnownExpiresAt=TrialNow+.1;
 TrialSense->BuildTeamKnownTargets(TrialNow);
 const auto TrialReport=TrialPick();KnownFireSearches.Reset();TrialSense->BuildTeamKnownTargets(TrialNow+.2);
 TrialCheck(TEXT("delivered_original_expiry"),TrialReport.Ready() && TrialSense->TeamKnownTargets[TrialShooter.Team].Targets.Num()==0);

 TrialReset();TrialObserve(251,TrialFirst,0);
 Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Bodies.FindChecked(2).Entity).Forward=-FVector::ForwardVector;
 const int32 TrialFOVRays=TrialCount(TEXT("known_fire_rays"));const auto TrialFOV=TrialPick();
 TrialCheck(TEXT("sensor_fov"),TrialFOV.Reject==BattleKnownFire::EReject::FOV && TrialCount(TEXT("known_fire_rays"))==TrialFOVRays);
 TrialReset();TrialObserve(251,TrialFirst,0);TrialShooter.Perception.SightRange=100;
 const int32 TrialRangeRays=TrialCount(TEXT("known_fire_rays"));const auto TrialRange=TrialPick();
 TrialCheck(TEXT("sensor_range"),TrialRange.Reject==BattleKnownFire::EReject::Range && TrialCount(TEXT("known_fire_rays"))==TrialRangeRays);

 TrialReset();TrialObserve(251,TrialFirst,0);TrialWall(TrialGround+FVector(750,0,150),FVector(30,130,150));
 TrialPick();const int32 TrialIntervalRays=TrialCount(TEXT("known_fire_rays"));TrialPick();
 TrialCheck(TEXT("search_interval"),TrialCount(TEXT("known_fire_rays"))==TrialIntervalRays);
 TrialReset();TrialObserve(251,TrialFirst,0);const auto TrialSelected=TrialPick();
 TrialWall(TrialGround+FVector(750,0,150),FVector(30,130,150));const auto TrialRechecked=TrialPick();
 TrialCheck(TEXT("selected_rechecked"),TrialSelected.Ready() && !TrialRechecked.Ready() && TrialRechecked.Reject==BattleKnownFire::EReject::SensorWall);

 TrialReset();TrialObserve(251,TrialFirst,0);TrialObserve(252,TrialSecond,0);
 TrialWall(TrialGround+FVector(750,150,150),FVector(30,600,150));
 const int32 TrialBlockedShots=TrialShooter.ShotSequence;
 for(int32 TrialFrame=0;TrialFrame<3;++TrialFrame)Follow(2,1.f/30.f,TrialNow+.4*TrialFrame);
 TrialCheck(TEXT("all_blocked_no_move"),TrialShooter.ShotSequence==TrialBlockedShots && Position(2).Equals(TrialStart,.01) && !CoverApproaches.Contains(2) && !FirePositions.Contains(2));

 TrialReset();TrialObserve(251,TrialFirst,0);TrialObserve(252,TrialSecond,0);
 FBattleContact TrialOwn=TrialSense->Observers.FindChecked(11).Contacts.FindChecked(251);TrialOwn.SourceUnitID=2;
 TrialSense->Observers.FindOrAdd(2).Contacts.Add(251,TrialOwn);
 TrialShooter.CoverPhase=4;TrialShooter.CoverAnchor=TrialStart;TrialShooter.CoverPeek=TrialStart;TrialShooter.CoverTargetID=251;
 TrialShooter.CoverSlot=FreeCoverSlot;TrialShooter.CoverOrderRevision=TrialShooter.OrderRevision;TrialShooter.CoverSeconds=1;
 const int32 TrialLegacyRays=TrialCount(TEXT("known_fire_rays")),TrialLegacyShots=TrialShooter.ShotSequence;
 StepCoverAction(2,1.f/30.f,TrialNow);
 const bool TrialLegacyPriority=TrialShooter.ShotSequence==TrialLegacyShots+1 && TrialCount(TEXT("known_fire_rays"))==TrialLegacyRays;
 // Even if legacy aiming turns the body, the cover's initial facing remains the reference.
 Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Bodies.FindChecked(2).Entity).Forward=FVector(0,1,0);
 TrialObserve(252,TrialStart+FVector(0,1800,0),0);
 const bool TrialSideRejected=!KnownFireAllowed(2,TrialSense->TeamKnownTargets[TrialShooter.Team].Targets.FindChecked(252),TrialNow);
 TrialCheck(TEXT("legacy_peek_priority"),TrialLegacyPriority && TrialSideRejected);

 // Close target and minimum skill spread keep this counter fixture independent of the shot sequence.
 TrialReset();TrialPlace(251,TrialStart+FVector(300,0,0));TrialObserve(251,TrialFirst,0);
 const float TrialSavedSkill=TrialShooter.SpreadSkillScale;TrialShooter.SpreadSkillScale=.25f;
 const auto TrialMetricTarget=TrialPick();
 const double TrialDamageBefore=SightExperiment.KnownFireDamage;
 const float TrialHPBefore=TrialStore->Find(251)->HP;
 const int32 TrialHitsBefore=TrialCount(TEXT("known_fire_hits"));
 const bool TrialMetricShot=FireKnownTarget(2,TrialStart+FVector(0,0,60),TrialMetricTarget,false);
 const float TrialLostHP=TrialHPBefore-TrialStore->Find(251)->HP;
 TrialCheck(TEXT("known_damage_counter"),TrialMetricShot && TrialLostHP>0 &&
  FMath::IsNearlyEqual(SightExperiment.KnownFireDamage-TrialDamageBefore,double(TrialLostHP),.001) && TrialCount(TEXT("known_fire_hits"))==TrialHitsBefore+1);
 TrialShooter.NextShotTime=0;
 const double TrialKnownDamageAfter=SightExperiment.KnownFireDamage;
 const int32 TrialKnownHitsAfter=TrialCount(TEXT("known_fire_hits"));
 const bool TrialNormalShot=Fire(2,TrialStart+FVector(0,0,60),(TrialMetricTarget.AimPosition-(TrialStart+FVector(0,0,60))).GetSafeNormal(),false);
 TrialCheck(TEXT("ordinary_not_known"),TrialNormalShot && SightExperiment.KnownFireDamage==TrialKnownDamageAfter && TrialCount(TEXT("known_fire_hits"))==TrialKnownHitsAfter);
 TrialShooter.SpreadSkillScale=TrialSavedSkill;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_DIAG known_fire_damage=%.6f"),SightExperiment.KnownFireDamage);

 if(BattleKnownFire::MoveEnabled())
 {
  TrialFailures+=RunKnownTargetMoveTrial();
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_DIAG known_move_distance_cm=%.6f"),SightExperiment.KnownMoveDistance);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_DIAG known_move_time=%.6f"),SightExperiment.KnownMoveSeconds);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_DIAG known_move_first_shot_time=%.6f"),SightExperiment.KnownMoveFirstShotSeconds);
 }
 for(const auto& TrialDiag:SightExperiment.CoverDiag)if(TrialDiag.Key.ToString().StartsWith(TEXT("known_fire_")) || TrialDiag.Key.ToString().StartsWith(TEXT("known_move_")))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_DIAG %s=%d"),*TrialDiag.Key.ToString(),TrialDiag.Value);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_FIRE_RESULT failures=%d"),TrialFailures);
 FPlatformMisc::RequestExitWithStatus(false,TrialFailures?1:0);
}
