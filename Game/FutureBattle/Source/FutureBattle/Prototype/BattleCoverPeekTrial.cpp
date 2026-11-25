#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattlePerception.h"
#include "BattleStance.h"
#include "BattleTactics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunCoverPeekTrial()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("BattleCoverPeekTrial")))return;
 auto* TrialWorld=GetWorld();auto* TrialStore=TrialWorld->GetSubsystem<UBattleState>();auto* TrialSense=TrialWorld->GetSubsystem<UBattlePerception>();
 const double TrialNow=TrialWorld->GetTimeSeconds();
 auto TrialLater=[&](){FTimerHandle TrialTimer;TrialWorld->GetTimerManager().SetTimer(TrialTimer,this,&UBattleLeaderRuntime::RunCoverPeekTrial,1.f/30.f,false);};
 auto TrialPlace=[&](int32 TrialID,FVector TrialAt)
 {
  auto& TrialBody=Bodies.FindChecked(TrialID);
  if(TrialBody.Actor.IsValid())TrialBody.Actor->SetActorLocation(TrialAt,false,nullptr,ETeleportType::TeleportPhysics);
  else{TrialBody.Capsule->SetWorldLocation(TrialAt);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(TrialBody.Entity).Position=TrialAt;}
 };
 auto TrialCount=[&](const TCHAR* TrialKey){return SightExperiment.CoverDiag.FindRef(FName(TrialKey));};
 const TCHAR* TrialName=PeekTrialCase==0?TEXT("visible"):PeekTrialCase==1?TEXT("lost"):TEXT("undetected");
 Mode->bCombatPaused=true;
 if(PeekTrialStage==0)
 {
  PeekTrialAt=TrialNow;PeekTrialLast=TrialNow;PeekTrialReady=false;
  if(PeekTrialCase==0)
  {
   FVector TrialGround(-9595,-4000,0);FHitResult TrialFloor;
   PeekTrialFixture=TrialWorld->LineTraceSingleByObjectType(TrialFloor,TrialGround+FVector(0,0,500),TrialGround-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic));
   if(PeekTrialFixture)TrialGround.Z=TrialFloor.ImpactPoint.Z;
   PeekTrialAnchor=TrialGround+FVector(0,0,90);
   auto TrialWall=[&](FVector TrialCenter,FVector TrialExtent)
   {
    auto* TrialWallActor=TrialWorld->SpawnActor<AActor>();if(!TrialWallActor){PeekTrialFixture=false;return;}
    auto* TrialBox=NewObject<UBoxComponent>(TrialWallActor);TrialWallActor->SetRootComponent(TrialBox);TrialWallActor->AddInstanceComponent(TrialBox);
    TrialBox->SetBoxExtent(TrialExtent);TrialBox->SetCollisionObjectType(ECC_WorldStatic);TrialBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);TrialBox->SetCollisionResponseToAllChannels(ECR_Block);
    TrialBox->SetCanEverAffectNavigation(false);TrialBox->SetWorldLocation(TrialCenter);TrialBox->SetMobility(EComponentMobility::Static);TrialBox->RegisterComponent();
   };
   TrialWall(TrialGround+FVector(95,0,60),FVector(15,200,60));
   TrialWall(TrialGround+FVector(750,500,125),FVector(30,250,125));
   PeekTrialFixture&=SightExperiment.Enabled && BattleStance::Enabled(TrialWorld) && Bodies.Contains(2) && Bodies.Contains(251);
  }
  if(!PeekTrialFixture)
  {
   UE_LOG(LogFutureBattle,Error,TEXT("BATTLE_COVERPEEK_CHECK name=fixture ok=0"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERPEEK_RESULT failures=1"));FPlatformMisc::RequestExitWithStatus(false,1);return;
  }
  for(auto& TrialPair:TrialStore->Units)
  {
   TrialPair.Value.Ammo=0;TrialPair.Value.Reserve=0;TrialPair.Value.Perception.ShareRange=0;
  }
  CancelCoverAction(2);CoverPeekShots.Remove(2);SightExperiment.PeekWhy.Reset();SightExperiment.PeekScans.Reset();SightExperiment.PeekAimUntil.Reset();SightExperiment.PeekIntervalUntil.Reset();SightExperiment.CoverDiag.Reset();
  TrialSense->Reset();
  TrialPlace(2,PeekTrialAnchor);TrialPlace(251,PeekTrialAnchor+FVector(1500,0,0));
  auto& TrialShooter=TrialStore->Units.FindChecked(2);auto& TrialEnemy=TrialStore->Units.FindChecked(251);
  TrialShooter.HP=10000;TrialEnemy.HP=10000;TrialShooter.Ammo=30;TrialShooter.Reserve=90;TrialShooter.ReloadEndTime=0;TrialShooter.NextShotTime=0;TrialShooter.bWeaponAimInitialized=false;
  TrialShooter.Order=EBattleOrder::Hold;TrialStore->Squads.FindChecked(TrialShooter.SquadID).CommandStatus=EBattleCommandStatus::Active;
  auto& TrialFollower=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Bodies.FindChecked(2).Entity);TrialFollower.Forward=FVector(1,0,0);Bodies.FindChecked(2).Capsule->SetWorldRotation(FRotator::ZeroRotator);
  SetStance(2,BattleStance::Stand,TrialNow,true);SetStance(251,BattleStance::Stand,TrialNow,true);
  PeekTrialStage=1;TrialLater();return;
 }
 auto& TrialShooter=TrialStore->Units.FindChecked(2);
 if(PeekTrialStage==1)
 {
  // Establish an actual own sighting before the first two scenes. The third starts with memory only.
  if(PeekTrialCase<2)
  {
   TrialSense->Update(Mode->Units,TrialNow,0,0);
   const auto* TrialContact=TrialSense->FindContact(2,251);
   PeekTrialReady=TrialContact && TrialContact->bVisible && !TrialContact->bShared;
   if(!PeekTrialReady && TrialNow-PeekTrialAt<6.){TrialLater();return;}
  }
  else
  {
   auto& TrialObserver=TrialSense->Observers.FindOrAdd(2);TrialObserver.ProfileRevision=TrialShooter.PerceptionRevision;
   FBattleContact TrialMemory;TrialMemory.TargetID=251;TrialMemory.SourceUnitID=2;TrialMemory.LastKnownPosition=PeekTrialAnchor+FVector(1500,0,0);TrialMemory.LastSeenTime=TrialNow;TrialMemory.Confidence=1;TrialMemory.bShared=true;
   TrialObserver.Contacts.Add(251,TrialMemory);PeekTrialReady=true;
  }
  if(PeekTrialCase==1)
  {
   TrialPlace(251,PeekTrialAnchor+FVector(1500,1000,0));
   // A normal scan must lose the sighting; never fabricate the hidden enemy's new known position.
   TrialSense->Update(Mode->Units,TrialNow);
   const auto* TrialLost=TrialSense->FindContact(2,251);
   PeekTrialReady&=TrialLost && !TrialLost->bVisible && TrialLost->LastKnownPosition.Equals(PeekTrialAnchor+FVector(1500,0,0),1.);
  }
  PeekTrialReady&=CoverLineClear(2,PeekTrialAnchor,PeekTrialAnchor+FVector(1500,0,0));
  TrialShooter.CoverPhase=2;TrialShooter.CoverAnchor=PeekTrialAnchor;TrialShooter.CoverPeek=PeekTrialAnchor;TrialShooter.CoverTargetID=251;TrialShooter.CoverSlot=FreeCoverSlot;TrialShooter.CoverOrderRevision=TrialShooter.OrderRevision;TrialShooter.CoverSeconds=0;
  SetStance(2,BattleStance::Crouch,TrialNow,true);PeekTrialAt=TrialNow;PeekTrialLast=TrialNow;PeekTrialStage=2;TrialLater();return;
 }
 const float TrialDt=float(TrialNow-PeekTrialLast);PeekTrialLast=TrialNow;
 TrialSense->Update(Mode->Units,TrialNow,0,0);StepCoverAction(2,TrialDt,TrialNow);
 const bool bTrialEnded=TrialCount(TEXT("peekwhy_arrived"))>0 && TrialShooter.CoverPhase!=3 && TrialShooter.CoverPhase!=4;
 if(!bTrialEnded && TrialNow-PeekTrialAt<6.){TrialLater();return;}
 const int32 TrialFired=TrialCount(TEXT("peekwhy_shot_frames_fired")),TrialBlind=TrialCount(TEXT("peekwhy_end_blind_0"));
 int32 TrialUnseen=0;for(int32 TrialClass=0;TrialClass<5;++TrialClass)TrialUnseen+=TrialCount(*FString::Printf(TEXT("peekwhy_zero_frames_%s"),FBattleSightExperiment::PeekClass(TrialClass)));
 const bool bTrialOK=PeekTrialReady && bTrialEnded && (PeekTrialCase==0?TrialFired>0:TrialBlind==1 && TrialUnseen>0 && TrialCount(TEXT("peekwhy_zero_frames_aimturn"))==0 && TrialCount(TEXT("peekwhy_zero_frames_interval"))==0);
 if(PeekTrialCase<2){PeekTrialFailures+=!bTrialOK;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERPEEK_CHECK name=%s ok=%d arrived=%d fired=%d blind=%d unseen=%d ready=%d"),TrialName,bTrialOK,TrialCount(TEXT("peekwhy_arrived")),TrialFired,TrialBlind,TrialUnseen,PeekTrialReady);}
 else UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERPEEK_CHECK name=undetected info arrived=%d fired=%d blind=%d detection_seconds=%.3f ended=%d"),TrialCount(TEXT("peekwhy_arrived")),TrialFired,TrialBlind,TrialShooter.Perception.DetectionSeconds,bTrialEnded);
 for(const auto& TrialCounter:SightExperiment.CoverDiag)if(TrialCounter.Key.ToString().StartsWith(TEXT("peekwhy_")))UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERPEEK_DIAG name=%s %s=%d"),TrialName,*TrialCounter.Key.ToString(),TrialCounter.Value);
 if(++PeekTrialCase<3){PeekTrialStage=0;TrialLater();return;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERPEEK_RESULT failures=%d"),PeekTrialFailures);FPlatformMisc::RequestExitWithStatus(false,PeekTrialFailures?1:0);
}
