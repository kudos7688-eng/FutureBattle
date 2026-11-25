#include "BattleLeaderRuntime.h"
#include "BattleCoverFire.h"
#include "BattleGameMode.h"
#include "BattleStance.h"
#include "BattleTactics.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunCoverFirePointTrial()
{
 // No unit is needed: bPeekLine=false never reads the supplied ID.
 Mode->bCombatPaused=true;
 auto* TrialWorld=GetWorld();auto* TrialNav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(TrialWorld);
 int32 TrialFailures=0;
 const TCHAR* TrialNames[]={TEXT("A"),TEXT("B"),TEXT("C"),TEXT("D")};
 const double TrialLengths[]={400.,1200.,300.,1200.};
 const double TrialHeights[]={120.,250.,250.,250.};
 for(int32 TrialIndex=0;TrialIndex<4;++TrialIndex)
 {
  // Same open strip as the movement fixtures, with separate lanes for the four walls.
  FVector TrialGround(-9595,-4000+TrialIndex*4000,0);
  FHitResult TrialFloor;
  const bool bTrialFloor=TrialWorld->LineTraceSingleByObjectType(TrialFloor,TrialGround+FVector(0,0,500),TrialGround-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic));
  if(bTrialFloor)TrialGround.Z=TrialFloor.ImpactPoint.Z;
  // C is 25 cm, D is 200 cm inside its positive-Y edge; its threat is straight ahead of the anchor.
  FVector TrialWallAt=TrialGround+FVector(95,TrialIndex==2?-125:TrialIndex==3?-400:0,TrialHeights[TrialIndex]*.5);
  auto* TrialWall=TrialWorld->SpawnActor<AActor>();
  bool bTrialFixture=bTrialFloor && TrialNav && BattleStance::Enabled(TrialWorld) && TrialWall;
  if(TrialWall)
  {
   auto* TrialBox=NewObject<UBoxComponent>(TrialWall);TrialWall->SetRootComponent(TrialBox);TrialWall->AddInstanceComponent(TrialBox);
   TrialBox->SetBoxExtent(FVector(15,TrialLengths[TrialIndex]*.5,TrialHeights[TrialIndex]*.5));
   TrialBox->SetCollisionObjectType(ECC_WorldStatic);TrialBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);TrialBox->SetCollisionResponseToAllChannels(ECR_Block);
   // Deliberately retain baked navigation: projections stay on the near side, never inside a wall.
   TrialBox->SetCanEverAffectNavigation(false);TrialBox->SetWorldLocation(TrialWallAt);TrialBox->SetMobility(EComponentMobility::Static);TrialBox->RegisterComponent();
  }
  if(TrialNav)for(float TrialOffset:{-120.f,0.f,120.f})
  {
   FNavLocation TrialProjected;const FVector TrialNavAt=TrialGround+FVector(0,TrialOffset,0);
   const bool bTrialProjected=TrialNav->ProjectPointToNavigation(TrialNavAt,TrialProjected,FVector(50,50,150));
   bTrialFixture &= bTrialProjected && FVector::Dist2D(TrialNavAt,TrialProjected.Location)<1. && FMath::Abs(TrialNavAt.Z-TrialProjected.Location.Z)<20.;
  }
  const FVector TrialAnchor=TrialGround+FVector(0,0,90),TrialThreat=TrialGround+FVector(1895,0,90),TrialDirection(1,0,0);
  const bool bTrialHidden=!BattleTactics::HasTerrainLine(TrialWorld,TrialAnchor+FVector(0,0,20),TrialThreat+FVector(0,0,60));
  const bool bTrialSweep=TrialWorld->SweepTestByObjectType(TrialAnchor,TrialAnchor+FVector(200,0,0),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88));
  bTrialFixture &= bTrialHidden && bTrialSweep;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERFP_FIXTURE name=%s ok=%d ground=%s hidden=%d sweep=%d nav=%d"),TrialNames[TrialIndex],bTrialFixture,*TrialGround.ToCompactString(),bTrialHidden,bTrialSweep,TrialNav!=nullptr);
  FVector TrialPeek=TrialAnchor;uint8 TrialKind=0;
  const bool bTrialFire=CoverFirePoint(INDEX_NONE,TrialAnchor,TrialThreat,TrialPeek,TrialKind,3,false);
  const double TrialPeekCM=FVector::Dist2D(TrialAnchor,TrialPeek);
  const bool bTrialExpected=TrialIndex==0 || TrialIndex==2;
  const bool bTrialShape=TrialIndex==0?(TrialKind==1 && TrialPeek.Equals(TrialAnchor,.01)):(TrialIndex==1 || TrialIndex==3)?TrialKind==0:(TrialKind==2 && TrialPeekCM>=100. && TrialPeekCM<=120.);
  const bool bTrialCoverOK=bTrialFixture && bTrialFire==bTrialExpected && bTrialShape;
  TrialFailures+=!bTrialCoverOK;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERFP_CHECK name=%s_cover ok=%d kind=%d peek_cm=%.3f"),TrialNames[TrialIndex],bTrialCoverOK,int32(TrialKind),TrialPeekCM);
  if(TrialIndex==3)
  {
   // Exercise the level 2 fallback directly; retain -BattleCoverFirePoint=1 and the original six checks.
   const float TrialHidden=BattleTactics::ExposureAt(TrialWorld,TrialAnchor,TrialDirection,TrialThreat+FVector(0,0,60)).Fraction();
   FVector TrialStepPeek=TrialAnchor;
   const bool bTrialStep=CoverStepFirePoint(TrialAnchor,TrialThreat,FVector::CrossProduct(TrialDirection,FVector::UpVector),TrialDirection,TrialHidden,TrialStepPeek);
   const double TrialStepCM=FVector::Dist2D(TrialAnchor,TrialStepPeek);
   const bool bTrialStepOK=bTrialFixture && !bTrialFire && TrialHidden<=.4f && bTrialStep && TrialStepCM>120. && TrialStepCM<=450.;
   TrialFailures+=!bTrialStepOK;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERFP_CHECK name=D_step ok=%d kind=3 peek_cm=%.3f"),bTrialStepOK,TrialStepCM);
   continue;
  }
  const bool bTrialArrival=BattleCoverFire::ArrivalFirePoint(TrialWorld,TrialNav,TrialGround,TrialDirection);
  const bool bTrialArrivalOK=bTrialFixture && bTrialArrival==bTrialExpected;TrialFailures+=!bTrialArrivalOK;
  // Arrival returns only a bool; -1 marks kind/distance as unavailable, not inferred from CoverFirePoint.
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERFP_CHECK name=%s_arrival ok=%d kind=-1 peek_cm=-1 accepted=%d"),TrialNames[TrialIndex],bTrialArrivalOK,bTrialArrival);
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVERFP_RESULT failures=%d"),TrialFailures);
 FTimerHandle TrialExitHandle;TrialWorld->GetTimerManager().SetTimer(TrialExitHandle,FTimerDelegate::CreateWeakLambda(this,[TrialFailures](){FPlatformMisc::RequestExitWithStatus(false,TrialFailures?1:0);}),1.f,false);
}
