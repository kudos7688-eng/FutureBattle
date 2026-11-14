#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"

void UBattleLeaderRuntime::RunMovementTrial()
{
 const TCHAR* Names[]={TEXT("dense"),TEXT("narrow"),TEXT("counterflow"),TEXT("gather")};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMovementTrial,Delay,false);};
 auto Place=[&](int32 ID,FVector Pos)
 {
  auto& B=Bodies.FindChecked(ID);
  if(B.Actor.IsValid()) {B.Actor->SetActorLocation(Pos,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else {B.Capsule->SetWorldLocation(Pos);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=Pos;}
 };
 if(MoveTrialStage==0)
 {
  Mode->bCombatPaused=true; Mode->CombatStartsAt=0;
  for(const auto& Pair:Store->GetSquads()) IssueOrder(Pair.Key,EBattleOrder::Hold,Position(Pair.Value.LeaderUnitID));
  for(auto Wall:MoveTrialWalls) if(Wall.IsValid()) Wall->Destroy(); MoveTrialWalls.Reset();
  // Park all 500 bodies away from the fixture, preserving representation and collision.
  for(const auto& Pair:Store->GetUnits()) Place(Pair.Key,FVector(8000+(Pair.Key%20)*120,-7000+(Pair.Key/20)*120,100));
  for(auto& Pair:Plans) {Pair.Value.ArrivalGoals.Reset();Pair.Value.Path.Reset();Pair.Value.Corridors.Reset();Pair.Value.NextPath=0;Pair.Value.PathRequests=0;}
  MoveTrialIDs.Reset(); MoveTrialGoals.Reset(); MoveTrialStarts.Reset(); MoveTrialArrivals.Reset(); MoveTrialFrameMS.Reset(); MoveTrialStall=0;MoveTrialMaxDeviation=0;MoveTrialMinSpacing=DBL_MAX;
  const int32 Squads=MoveTrialScene==3?10:MoveTrialScene==0?4:2;
  if(MoveTrialScene==1 || MoveTrialScene==2)
   for(int32 Sign:{-1,1})
   {
    auto* Wall=GetWorld()->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(40,650,150));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();Wall->SetActorLocation(FVector(-9500+Sign*160,0,150));MoveTrialWalls.Add(Wall);
    DrawDebugBox(GetWorld(),Wall->GetActorLocation(),Box->GetUnscaledBoxExtent(),FColor::Orange,false,31,0,5);
   }
  for(int32 I=0;I<Squads;++I)
  {
   const int32 SID=I+1; const auto* S=Store->FindSquad(SID);
   FVector Start,Goal,Forward(0,1,0);
   if(MoveTrialScene==3)
   {const float Angle=I*2*PI/Squads;Start=FVector(-9500+FMath::Cos(Angle)*1700,FMath::Sin(Angle)*1700,100);Goal=FVector(-9500,0,0);Forward=(Goal-Start).GetSafeNormal2D();}
   else if(MoveTrialScene==2 && I==1) {Start=FVector(-9500,1800,100);Goal=FVector(-9500,-1800,0);Forward=-Forward;}
   else {Start=FVector(-9500+(MoveTrialScene==0?(I%2)*350:0),-1800-(MoveTrialScene==0?I/2:I)*450,100);Goal=FVector(-9500+(MoveTrialScene==0?(I%2)*350:0),1800+(MoveTrialScene==0?I/2:I)*350,0);}
   const FVector Right(-Forward.Y,Forward.X,0);
   for(int32 Slot=0;Slot<10;++Slot)
   {
    const int32 ID=S->Members[Slot];const FVector Pos=Start+Right*((Slot%3-1)*100)-Forward*((Slot/3)*100);
    Place(ID,Pos); MoveTrialIDs.Add(ID);MoveTrialStarts.Add(ID,Pos);MoveTrialGoals.Add(ID,Goal);
   }
   if(!IssueOrder(SID,EBattleOrder::Move,Goal)) ++SmokeFailures;
  }
  auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());PC->SetSelection(Actor(1));
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())) {Camera->SetActorLocation(FVector(-9500,0,0));Camera->Zoom(5500-Camera->Camera->GetRelativeLocation().Z);}
  MoveTrialStart=GetWorld()->GetTimeSeconds(); Mode->bCombatPaused=false; MoveTrialStage=1;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_BEGIN scene=%s stable=%d actors=%d bodies=%d active=%d"),Names[MoveTrialScene],bStableMovement,Mode->Units.Num(),Bodies.Num(),MoveTrialIDs.Num());
  Later(.25f);return;
 }
 const double Elapsed=GetWorld()->GetTimeSeconds()-MoveTrialStart; int32 Arrived=0,Stalled=0; double Distance=0,Deviation=0;
 for(int32 ID:MoveTrialIDs)
 {
  const auto& B=Bodies.FindChecked(ID); const auto* S=Store->FindSquad(Store->Find(ID)->SquadID);const auto& P=Plans.FindChecked(S->SquadID);
  FVector Goal=MoveTrialGoals.FindChecked(ID);
  if(bStableMovement && P.ArrivalGoals.Contains(ID)) Goal=P.ArrivalGoals.FindChecked(ID);
  else if(P.Path.Num()>1) {const FVector Dir=(P.Path.Last()-P.Path[P.Path.Num()-2]).GetSafeNormal2D();Goal=P.Path.Last()+FVector(-Dir.Y,Dir.X,0)*float(S->Members.IndexOfByKey(ID)%5-2)*80.f;}
  const double D=FVector::Dist2D(Position(ID),Goal);Distance+=D;
  const FVector Closest=FMath::ClosestPointOnSegment(Position(ID),MoveTrialStarts.FindChecked(ID),MoveTrialGoals.FindChecked(ID));Deviation=FMath::Max(Deviation,FVector::Dist2D(Position(ID),Closest));
  const bool Done=D<100; // Same 1m destination tolerance for both executors.
  if(Done && !MoveTrialArrivals.Contains(ID)) MoveTrialArrivals.Add(ID,Elapsed);
  Arrived+=Done;
  if(!Done && B.Velocity.Size2D()<10 && Elapsed>2) ++Stalled;
 }
 MoveTrialStall+=Stalled*.25;
 for(int32 A=0;A<MoveTrialIDs.Num();++A) for(int32 B=A+1;B<MoveTrialIDs.Num();++B)
  MoveTrialMinSpacing=FMath::Min(MoveTrialMinSpacing,FVector::Dist2D(Position(MoveTrialIDs[A]),Position(MoveTrialIDs[B])));
 MoveTrialMaxDeviation=FMath::Max(MoveTrialMaxDeviation,Deviation);
 if(Elapsed<30 && Arrived<MoveTrialIDs.Num()) {Later(.25f);return;}
 Mode->bCombatPaused=true;int32 Paths=0;for(const auto& Pair:Plans) Paths+=Pair.Value.PathRequests;
 double ArrivalSum=0;for(const auto& A:MoveTrialArrivals) ArrivalSum+=A.Value;
 MoveTrialFrameMS.Sort(); double FrameSum=0;for(float MS:MoveTrialFrameMS) FrameSum+=MS;
 if(!MoveTrialFrameMS.IsEmpty()) UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_FRAME scene=%s count=%d mean_ms=%.3f p95_ms=%.3f"),Names[MoveTrialScene],MoveTrialFrameMS.Num(),FrameSum/MoveTrialFrameMS.Num(),MoveTrialFrameMS[FMath::Min(MoveTrialFrameMS.Num()-1,FMath::FloorToInt(MoveTrialFrameMS.Num()*.95))]);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_METRIC scene=%s stable=%d arrived=%d total=%d seconds=%.2f mean_arrival=%.2f mean_distance=%.2f max_deviation=%.2f stall_person_seconds=%.2f path_requests=%d"),Names[MoveTrialScene],bStableMovement,Arrived,MoveTrialIDs.Num(),Elapsed,MoveTrialArrivals.Num()?ArrivalSum/MoveTrialArrivals.Num():-1,Distance/MoveTrialIDs.Num(),MoveTrialMaxDeviation,MoveTrialStall,Paths);
 int32 Printed=0;
 for(int32 ID:MoveTrialIDs) if(Printed<8 && !MoveTrialArrivals.Contains(ID))
 {const auto& B=Bodies.FindChecked(ID);const auto& P=Plans.FindChecked(Store->Find(ID)->SquadID);const FVector Goal=P.ArrivalGoals.Contains(ID)?P.ArrivalGoals.FindChecked(ID):MoveTrialGoals.FindChecked(ID);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_STUCK id=%d pos=%s goal=%s waypoint=%d path=%d wait=%d passing=%d bypass=%s"),ID,*Position(ID).ToCompactString(),*Goal.ToCompactString(),B.Waypoint,P.Path.Num(),B.bTrafficWait,B.PassingID,*B.PassingWaypoint.ToCompactString());++Printed;}
 if(Bodies.Num()!=500 || Mode->Units.Num()!=50) ++SmokeFailures;
 if(bStableMovement)
 {
  // Fixed fixtures require every mover to reach its reserved slot within 30 seconds.
  bool SlotsValid=true;
  TArray<FVector> Slots;
  for(int32 ID:MoveTrialIDs)
  {
   const auto& Plan=Plans.FindChecked(Store->Find(ID)->SquadID);
   const FVector* Slot=Plan.ArrivalGoals.Find(ID);
   if(!Slot) {SlotsValid=false;continue;}
   for(const FVector& Other:Slots) if(FVector::Dist2D(*Slot,Other)<189) SlotsValid=false;
   Slots.Add(*Slot);
  }
  const bool Passed=Arrived==MoveTrialIDs.Num() && SlotsValid && Paths<=MoveTrialIDs.Num() && MoveTrialMinSpacing>=68;
  if(!Passed) ++SmokeFailures;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_CHECK %s scene=%s all_arrived=%d distinct_slots=%d bounded_shared_paths=%d min_spacing=%.2f"),Passed?TEXT("PASS"):TEXT("FAIL"),Names[MoveTrialScene],Arrived==MoveTrialIDs.Num(),SlotsValid,Paths<=MoveTrialIDs.Num(),MoveTrialMinSpacing);
 }
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-")+Names[MoveTrialScene]+TEXT(".png"),true,false);
 if(++MoveTrialScene<4 && MoveTrialOnly<0) {MoveTrialStage=0;Later(1);return;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_RESULT failures=%d"),SmokeFailures);
 FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[this](){FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);}),1.f,false);
}
