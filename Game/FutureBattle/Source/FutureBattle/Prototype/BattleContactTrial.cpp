#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Camera/CameraComponent.h"
#include "FutureBattle.h"

// Fixed contact fixtures retain all 500 bodies, collision, and the normal executor.
void UBattleLeaderRuntime::RunContactTrial()
{
 const TCHAR* Names[]={TEXT("hold-pass"),TEXT("occupied-arrival"),TEXT("wait-release")};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunContactTrial,Delay,false);};
 auto Place=[&](int32 ID,FVector Pos)
 {
  auto& B=Bodies.FindChecked(ID);
  if(B.Actor.IsValid()){B.Actor->SetActorLocation(Pos,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else {B.Capsule->SetWorldLocation(Pos);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=Pos;}
  B.Velocity=FVector::ZeroVector;
 };
 if(MoveTrialStage==0)
 {
  Mode->bCombatPaused=true;Mode->CombatStartsAt=0;
  for(const auto& Pair:Store->GetSquads()) IssueOrder(Pair.Key,EBattleOrder::Hold,Position(Pair.Value.LeaderUnitID));
  for(auto Wall:MoveTrialWalls) if(Wall.IsValid()) Wall->Destroy();MoveTrialWalls.Reset();
  for(const auto& Pair:Store->GetUnits()) Place(Pair.Key,FVector(8000+(Pair.Key%20)*120,-7000+(Pair.Key/20)*120,100));
  for(auto& Pair:Plans){Pair.Value=FBattleLeaderPlan();}
  MoveTrialIDs.Reset();MoveTrialStarts.Reset();MoveTrialArrivals.Reset();MoveTrialFrameMS.Reset();ContactBlockerStarts.Reset();
  MoveTrialStall=0;MoveTrialMinSpacing=DBL_MAX;ContactBlockersHeld=true;ContactWaitObserved=false;ContactReleased=false;
  for(int32 I=0;I<10;++I)
  {
   const FVector Pos(-9500+(I%3-1)*100,-1800-(I/3)*100,100);
   Place(I+1,Pos);MoveTrialIDs.Add(I+1);MoveTrialStarts.Add(I+1,Pos);
   Place(I+11,FVector(-9500+(I%5-2)*90,-(I/5)*100,100));
  }
  if(MoveTrialScene==2)
  {
   for(int32 Sign:{-1,1})
   {
    auto* Wall=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(40,650,150));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();Wall->SetActorLocation(FVector(-9500+Sign*160,0,150));MoveTrialWalls.Add(Wall);
   }
   for(int32 I=0;I<10;++I) Place(I+11,FVector(-9500+(I%2?70:-70),-100+(I/2)*100,100));
  }
  const FVector Goal(-9500,MoveTrialScene==1?0:1800,0);
  IssueOrder(1,EBattleOrder::Move,Goal);Think(1,GetWorld()->GetTimeSeconds());
  if(MoveTrialScene==1)
  {
   // A different squad stops at previously reserved slots after path creation.
   for(int32 I=0;I<10;++I) if(const auto* G=Plans.FindChecked(1).ArrivalGoals.Find(I+1)) Place(I+11,FVector(G->X,G->Y,100));
  }
  for(int32 ID=11;ID<=20;++ID) ContactBlockerStarts.Add(ID,Position(ID));
  auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());PC->SetSelection(Actor(1));
  if(auto* C=Cast<ABattleCameraPawn>(PC->GetPawn())){C->SetActorLocation(FVector(-9500,0,0));C->Zoom(5500-C->Camera->GetRelativeLocation().Z);}
  MoveTrialStart=GetWorld()->GetTimeSeconds();MoveTrialStage=1;Mode->bCombatPaused=false;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CONTACT_BEGIN scene=%s"),Names[MoveTrialScene]);Later(.25f);return;
 }
 const double Elapsed=GetWorld()->GetTimeSeconds()-MoveTrialStart;
 if(!ContactReleased)
 {
  ContactBlockersHeld &= Store->FindSquad(2)->Order==EBattleOrder::Hold;
  for(const auto& Pair:ContactBlockerStarts) ContactBlockersHeld &= FVector::Dist2D(Pair.Value,Position(Pair.Key))<5;
 }
 if(MoveTrialScene==2 && !ContactReleased && Elapsed>=8)
 {
  // Explicitly release the standing squad, never move it on behalf of the waiter.
  IssueOrder(2,EBattleOrder::Move,FVector(-9500,3000,0));ContactReleased=true;
 }
 int32 Arrived=0,Waiting=0;
 for(int32 ID:MoveTrialIDs)
 {
  const auto* G=Plans.FindChecked(1).ArrivalGoals.Find(ID);const auto& B=Bodies.FindChecked(ID);
  const bool Done=G && FVector::Dist2D(Position(ID),*G)<100;
  Arrived+=Done;Waiting+=!Done && B.Velocity.Size2D()<10;
  if(Done && !MoveTrialArrivals.Contains(ID)) MoveTrialArrivals.Add(ID,Elapsed);
  if(MoveTrialScene==2 && !ContactReleased && Elapsed>5 && B.bTrafficWait) ContactWaitObserved=true;
  for(int32 Other=ID+1;Other<=20;++Other) MoveTrialMinSpacing=FMath::Min(MoveTrialMinSpacing,FVector::Dist2D(Position(ID),Position(Other)));
 }
 if(Elapsed>2) MoveTrialStall+=Waiting*.25;
 if(Elapsed<30 && (Arrived<10 || (MoveTrialScene==2 && !ContactReleased))){Later(.25f);return;}
 Mode->bCombatPaused=true;
 const bool Passed=Arrived==10 && ContactBlockersHeld && MoveTrialMinSpacing>=68 && Plans.FindChecked(1).PathRequests<=10 && (MoveTrialScene!=2 || ContactWaitObserved);
 if(!Passed) ++SmokeFailures;
 for(int32 ID:MoveTrialIDs)
 {
  const auto& B=Bodies.FindChecked(ID);const FVector G=Plans.FindChecked(1).ArrivalGoals.FindRef(ID);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CONTACT_UNIT scene=%s id=%d position=%s goal=%s distance=%.2f occupied=%d waiting=%d route=%d"),Names[MoveTrialScene],ID,*Position(ID).ToCompactString(),*G.ToCompactString(),FVector::Dist2D(Position(ID),G),B.bArrivalOccupied,B.bContactWaiting,B.ContactRoute.Num());
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CONTACT_WORK scene=%s arrival_reassignments=%d contact_sweeps=%d"),Names[MoveTrialScene],Plans.FindChecked(1).ArrivalReassignments,Plans.FindChecked(1).ContactProbes);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CONTACT_CHECK %s scene=%s arrived=%d/10 seconds=%.2f hold_preserved=%d wait_observed=%d min_spacing=%.2f stall_person_seconds=%.2f path_requests=%d actors=%d bodies=%d"),Passed?TEXT("PASS"):TEXT("FAIL"),Names[MoveTrialScene],Arrived,Elapsed,ContactBlockersHeld,ContactWaitObserved,MoveTrialMinSpacing,MoveTrialStall,Plans.FindChecked(1).PathRequests,Mode->Units.Num(),Bodies.Num());
 MoveTrialFrameMS.Sort();double Sum=0;for(float V:MoveTrialFrameMS) Sum+=V;
 if(!MoveTrialFrameMS.IsEmpty())UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CONTACT_FRAME scene=%s mean_ms=%.3f p95_ms=%.3f"),Names[MoveTrialScene],Sum/MoveTrialFrameMS.Num(),MoveTrialFrameMS[FMath::Min(MoveTrialFrameMS.Num()-1,FMath::FloorToInt(MoveTrialFrameMS.Num()*.95))]);
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-")+Names[MoveTrialScene]+TEXT(".png"),true,false);
 if(++MoveTrialScene<3){MoveTrialStage=0;Later(1);return;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CONTACT_RESULT failures=%d"),SmokeFailures);
 FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[this](){FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);}),1.f,false);
}
