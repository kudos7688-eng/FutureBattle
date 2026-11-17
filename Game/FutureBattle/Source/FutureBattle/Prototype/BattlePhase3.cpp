#include "BattlePhase3.h"
#include "BattleVATDisplay.h"
#include "BattleHiddenBoneTrial.h"
#include "BattleAnimTaskTrial.h"
#include "BattleLeaderRuntime.h"
#include "BattleMixedDriver.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleMetrics.h"
#include "BattleCover.h"
#include "BattleSquadAI.h"
#include "MassEntityManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "FutureBattle.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"

FPhase3Scope::FPhase3Scope(UBattlePhase3* In,EPhase3Cost C):P(In),Previous(EPhase3Cost::Count)
{
 if(!P || !P->bEnabled) { P=nullptr; return; }
 const double T=FPlatformTime::Seconds(); Previous=P->ActiveCost;
 if(Previous!=EPhase3Cost::Count) P->Costs[int32(Previous)]+=(T-P->CostStart)*1000;
 P->ActiveCost=C; P->CostStart=T;
}
FPhase3Scope::~FPhase3Scope()
{
 if(!P) return; const double T=FPlatformTime::Seconds();
 P->Costs[int32(P->ActiveCost)]+=(T-P->CostStart)*1000; P->ActiveCost=Previous; P->CostStart=T;
}
bool UBattlePhase3::Configure(UBattleLeaderRuntime& R)
{
 if(!FParse::Value(FCommandLine::Get(),TEXT("BattlePhase3="),Output)) return true;
 Runtime=&R; Mode=R.Mode; bEnabled=true;
 FString SenseMode=TEXT("A"); FParse::Value(FCommandLine::Get(),TEXT("P3Sense="),SenseMode);
 if(SenseMode!=TEXT("A") && SenseMode!=TEXT("B")) return false;
 bSenseBatch=SenseMode==TEXT("B"); bSenseAudit=FParse::Param(FCommandLine::Get(),TEXT("P3SenseAudit"));
 bContextAudit=FParse::Param(FCommandLine::Get(),TEXT("P3ContextAudit"));
 bContextReplay=FParse::Param(FCommandLine::Get(),TEXT("P3ContextReplay"));
 bGridAudit=FParse::Param(FCommandLine::Get(),TEXT("P3GridAudit"));
 if(int32(bSenseAudit)+int32(bContextAudit)+int32(bGridAudit)>1) return false;
 if(bContextReplay) ContextReplayRows.Add(TEXT("frame,time,state_sha1,sense_sha1,shots,deaths,hp,ammo,alive"));
 if(bSenseAudit || bContextAudit || bGridAudit)
 {
  ShadowSense=NewObject<UBattlePerception>(GetWorld());
  auto* Main=GetWorld()->GetSubsystem<UBattlePerception>();
  ShadowSense->bReuseSightContext=bContextAudit?!Main->bReuseSightContext:Main->bReuseSightContext;
  ShadowSense->bUseSpatialGrid=bGridAudit?!Main->bUseSpatialGrid:Main->bUseSpatialGrid;
  SenseEvents.Add(TEXT("mode,kind,observer,target,time,last_seen,source,x,y,z"));
 }
 FString Decision=TEXT("Leader"); FParse::Value(FCommandLine::Get(),TEXT("P3Decision="),Decision);
 FParse::Value(FCommandLine::Get(),TEXT("P3View="),View); FParse::Value(FCommandLine::Get(),TEXT("P3Layout="),Layout);
 FParse::Value(FCommandLine::Get(),TEXT("P3Seed="),Seed); FParse::Value(FCommandLine::Get(),TEXT("P3Warmup="),Warmup); FParse::Value(FCommandLine::Get(),TEXT("P3Duration="),Duration);
 bStable=FParse::Param(FCommandLine::Get(),TEXT("P3Stable")); bIndividual=Decision==TEXT("Individual");
 if((Decision!=TEXT("Individual") && Decision!=TEXT("Leader")) || (View!=TEXT("RTS") && View!=TEXT("TPS") && View!=TEXT("FPS")) ||
 (Layout!=TEXT("Open") && Layout!=TEXT("Cross") && Layout!=TEXT("Gate")) || !GetWorld()->GetMapName().EndsWith(TEXT("Lvl_BattleField500")) || Warmup<0 || Duration<=0) return false;
 return true;
}
FVector UBattlePhase3::SpawnCenter(int32 Team,int32 Group) const
{
 // Fits existing 240m x 180m field, on its authored deployment NavMesh.
 const int32 Lane=Group%10,Depth=Group/10;
 FRandomStream Random(Seed*7919+Group*31+Team*17);
 return FVector((Team?1:-1)*(9100+Depth*550), (Lane-4.5)*1500+Random.FRandRange(-12,12),95);
}
FVector UBattlePhase3::Goal(int32 Team,int32 Group) const
{
 const double Y=(Group%10-4.5)*1500;
 return FVector(Team?-600:600,Layout==TEXT("Cross")?-Y:Layout==TEXT("Gate")?(Group%3-1)*4500:Y,0);
}
void UBattlePhase3::Begin()
{
 ControlledID=Runtime->Bodies.Num()>=100?41:1;
 Avoidance=NewObject<UBattleMixedAvoidance>(GetWorld());
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(auto& Pair:Runtime->Bodies)
 {
  const int32 ID=Pair.Key; auto& Body=Pair.Value;
  AvoidanceIDs.Add(ID,ID); InitialPositions.Add(ID,Runtime->Position(ID));
  auto Driver=MakeShared<FBattleMixedDriver>(); Driver->Proxy.P=this; Driver->Proxy.ID=ID; Drivers.Add(ID,Driver);
  if(Body.Actor.IsValid()) { Body.Actor->GetCharacterMovement()->SetAvoidanceEnabled(false); Body.Actor->GetCharacterMovement()->MaxWalkSpeed=GetDefault<UBattleTuning>()->MoveSpeed; }
  TArray<FVector> TestPath; if(!Path(ID,Store->Find(ID)->Destination,TestPath)) ++Failures;
 }
 GetWorld()->GetSubsystem<UBattleCover>()->BuildSlots(true);
 if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) V->Set(0,ECVF_SetByCode);
 if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) V->Set(0,ECVF_SetByCode);
 Verify();
 if(FParse::Param(FCommandLine::Get(),TEXT("P3GridFixture"))) VerifyGrid();
 if(FParse::Param(FCommandLine::Get(),TEXT("P3BoneAudit")))
 {
  BoneTrial=GetWorld()->SpawnActor<ABattleHiddenBoneTrial>(FVector(0,0,-10000),FRotator::ZeroRotator);
  BoneTrial->Start(this);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("P3AnimTaskAudit")))
 {
  AnimTaskTrial=GetWorld()->SpawnActor<ABattleAnimTaskTrial>(FVector(0,0,-10000),FRotator::ZeroRotator);
  AnimTaskTrial->Start(this);
 }
 if(Failures) { Finish(); return; }
 GetWorld()->GetSubsystem<UBattleMetrics>()->Start();
 Started=GetWorld()->GetTimeSeconds(); LastWall=FPlatformTime::Seconds();
 UE_LOG(LogFutureBattle,Display,TEXT("P3_SENSE mode=%s grid=%d far=0 slices=%d audit=%d"),bSenseBatch?TEXT("B"):TEXT("A"),GetWorld()->GetSubsystem<UBattlePerception>()->bUseSpatialGrid,GetDefault<UBattleTuning>()->PerceptionSlices,bSenseAudit);
 Rows.Add(TEXT("time,view,alive,actors,mass,gt_ms,rt_ms,gpu_ms,wall_ms,gt_wait_ms,rt_wait_ms,perception_ms,decision_ms,path_ms,follow_ms,rvo_ms,shot_ms,display_ms,paths,path_failures,decisions,shots,deaths,rvo_changes,rvo_registered,moved,mean_distance_cm,sight_scan_ms,report_delivery_ms,report_queue_ms,decision_lateness_max_ms,sensor_lateness_max_ms,pending_reports_peak,move_failures,cover_scores,coordination_changes"));
 UE_LOG(LogFutureBattle,Display,TEXT("P3_READY units=%d actors=%d mass=%d individual=%d initial_path_failures=%d"),Runtime->Bodies.Num(),Mode->Units.Num(),Runtime->Bodies.Num()-Mode->Units.Num(),bIndividual,Failures);
}
void UBattlePhase3::ConfigureView()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 if(View!=TEXT("RTS") && PC)
 {
  if(!PC->ControlUnit(Runtime->Actor(ControlledID))) ++Failures;
  else if(View==TEXT("TPS")) { auto* A=Runtime->Actor(ControlledID); A->FirstPersonCamera->SetRelativeLocation(FVector(-350,80,130)); A->GetMesh()->SetOwnerNoSee(false); A->MountWeapon(A->Rifle,A->GetMesh(),A->GetState()->Weapon,false); }
 }
 else if(PC)
 {
  BenchmarkCamera=GetWorld()->SpawnActor<ACameraActor>(FVector(-4500,0,11000),FRotator(-65,0,0));
  BenchmarkCamera->GetCameraComponent()->SetFieldOfView(65); PC->SetViewTarget(BenchmarkCamera);
 }
 bViewSet=true;
}
bool UBattlePhase3::Path(int32 ID,FVector Goal,TArray<FVector>& Points)
{
 FPhase3Scope Timing(this,EPhase3Cost::Path); ++PathRequests;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); FNavLocation Target;
 auto Fail=[&](){++PathFailures;return false;};
 if(!Nav || !Nav->ProjectPointToNavigation(Goal,Target,FVector(100,100,400))) return Fail();
 auto* P=Nav->FindPathToLocationSynchronously(GetWorld(),Runtime->Position(ID),Target.Location);
 if(!P || !P->IsValid() || P->IsPartial()) return Fail();
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(U->SquadID);
 if(U->Order==EBattleOrder::Defend)
 {
  if(FVector::DistSquared2D(Target.Location,S->Destination)>FMath::Square(S->DefenseRadius)) return Fail();
  if(FVector::DistSquared2D(Runtime->Position(ID),S->Destination)<=FMath::Square(S->DefenseRadius))
   for(const FVector& Point:P->PathPoints) if(FVector::DistSquared2D(Point,S->Destination)>FMath::Square(S->DefenseRadius)) return Fail();
 }
 Points=P->PathPoints; return true;
}
void UBattlePhase3::SetPath(int32 ID,const TArray<FVector>& Points) { Paths.Add(ID,Points); Waypoints.Add(ID,1); }
void UBattlePhase3::Stop(int32 ID) { Paths.Remove(ID); Desired.Add(ID,FVector::ZeroVector); }
FVector UBattlePhase3::Velocity(int32 ID) const { const auto& B=Runtime->Bodies.FindChecked(ID); return B.Actor.IsValid()?B.Actor->GetVelocity():B.Velocity; }
void UBattlePhase3::Rotate(int32 ID,FRotator Rotation)
{
 auto& B=Runtime->Bodies.FindChecked(ID);
 if(B.Actor.IsValid()) B.Actor->SetActorRotation(Rotation);
 else { Runtime->Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=Rotation.Vector(); B.Capsule->SetWorldRotation(Rotation); }
}
void UBattlePhase3::OnDeath(int32 ID)
{
 ++Deaths; if(const int32* UID=AvoidanceIDs.Find(ID)) { Avoidance->RemoveAvoidanceObject(*UID); AvoidanceIDs.Remove(ID); }
 Paths.Remove(ID); GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);
}
void UBattlePhase3::Step(float Dt)
{
 if(bFinished) return;
 if(bContextReplay) RecordContextReplay();
 Sample();
 const double Now=GetWorld()->GetTimeSeconds(),Elapsed=Now-Started;
 if(!bViewSet && Elapsed>1) ConfigureView();
 if(!bViewChecked && bViewSet && Elapsed>2)
 {
  bViewChecked=true; auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController()); FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation);
  int32 W,H; PC->GetViewportSize(W,H); const double Offset=FVector::Distance(Eye,Runtime->Position(ControlledID));
  const bool OK=(FParse::Param(FCommandLine::Get(),TEXT("nullrhi")) || (W==1920 && H==1080)) && (View==TEXT("RTS")?PC->IsTactical():!PC->IsTactical() && (View==TEXT("TPS")?Offset>250 && Offset<500:Offset<150));
  if(!OK) ++Failures;
  UE_LOG(LogFutureBattle,Display,TEXT("P3_CAMERA %s view=%s unit=%d offset=%.2f viewport=%dx%d"),OK?TEXT("PASS"):TEXT("FAIL"),*View,ControlledID,Offset,W,H);
  for(const TCHAR* Name:{TEXT("r.ShadowQuality"),TEXT("r.Shadow.Virtual.Enable"),TEXT("r.Lumen.DiffuseIndirect.Allow"),TEXT("r.Lumen.Reflections.Allow"),TEXT("r.Lumen.HardwareRayTracing"),TEXT("r.RayTracing.Scene.BuildMode"),TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO"),TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.SimulationCount"),TEXT("r.SkeletalMeshLODBias"),TEXT("r.ViewDistanceScale"),TEXT("r.ScreenPercentage")})
   if(auto* Var=IConsoleManager::Get().FindConsoleVariable(Name))UE_LOG(LogFutureBattle,Display,TEXT("P3_RENDER_SETTING %s=%s"),Name,*Var->GetString());
 }
 if(!bCaptured && Elapsed>20 && FParse::Param(FCommandLine::Get(),TEXT("P3Capture"))) { bCaptured=true; FScreenshotRequest::RequestScreenshot(Output+TEXT(".png"),true,false); }
 if(Elapsed>=Warmup+Duration) { Finish(); return; }
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const auto* T=GetDefault<UBattleTuning>();
 if(!Mode->bCombatPaused && !Mode->bBattleEnded && Now>=Mode->CombatStartsAt) Store->AdvanceReloads(Now);
 // B mirrors normal GameMode's visibility scheduler, including pre-combat updates.
 if(bSenseBatch) { FPhase3Scope Scope(this,EPhase3Cost::Perception); AdvanceSense(GetWorld()->GetSubsystem<UBattlePerception>(),true,Now,SenseNext,SenseSlice,0); }
 const bool SenseCombat=!Mode->bCombatPaused && !Mode->bBattleEnded && Now>=Mode->CombatStartsAt;
 if(!bSenseBatch && SenseCombat) { FPhase3Scope Scope(this,EPhase3Cost::Perception); AdvanceSense(GetWorld()->GetSubsystem<UBattlePerception>(),false,Now,SenseNext,SenseSlice,0); }
 if(bSenseAudit || bContextAudit || bGridAudit)
 {
  if((bContextAudit || bGridAudit) ? (bSenseBatch || SenseCombat) : (!bSenseBatch || SenseCombat)) AdvanceSense(ShadowSense,(bContextAudit || bGridAudit)?bSenseBatch:!bSenseBatch,Now,ShadowNext,ShadowSlice,1);
  CompareSense(Now);
 }
 if(!Mode->bCombatPaused && !Mode->bBattleEnded && Now>=Mode->CombatStartsAt)
 {
  if(bIndividual)
  {
   GetWorld()->GetSubsystem<UBattleSquadAI>()->UpdateMixed(this,Now);
   for(auto& D:Drivers) if(D.Value->Proxy.IsAlive() && !(Runtime->Actor(D.Key) && Runtime->Actor(D.Key)->IsPlayerControlled())) D.Value->Tick(Dt);
  }
  else for(auto& P:Runtime->Plans) if(Now>=P.Value.NextThink) Runtime->Think(P.Key,Now);
  Desired.Reset();
  // Plan desired velocities first. All participants see the same immutable RVO snapshot.
  {
   FPhase3Scope Scope(this,EPhase3Cost::Follow);
   for(auto& Pair:Runtime->Bodies)
   {
    const int32 ID=Pair.Key; auto& B=Pair.Value; const auto* U=Store->Find(ID); if(!U->IsAlive()) continue;
    FVector Wish=FVector::ZeroVector;
    if(B.Actor.IsValid() && B.Actor->IsPlayerControlled())
    { Wish=Elapsed<23?FVector(T->MoveSpeed,0,0):FVector::ZeroVector; }
    else
    {
     const TArray<FVector>* Route=nullptr; int32* Index=nullptr; bool CanMove=true; float Offset=0;
     if(bIndividual) { Route=Paths.Find(ID); Index=Waypoints.Find(ID); }
     else if(auto* Plan=Runtime->Plans.Find(U->SquadID))
     {
      Route=&Plan->Path; Index=&B.Waypoint;
      CanMove=U->Order!=EBattleOrder::Hold && (U->Order==EBattleOrder::Move || Plan->TargetID==INDEX_NONE);
      const auto* S=Store->FindSquad(U->SquadID); Offset=B.StalledSince>0?0:float(S->Members.IndexOfByKey(ID)%5-2)*80;
     }
     if(CanMove && Route && Index && Route->IsValidIndex(*Index))
     {
      const FVector Segment=((*Route)[*Index]-(*Route)[FMath::Max(0,*Index-1)]).GetSafeNormal2D();
      FVector Goal=(*Route)[*Index]+FVector(-Segment.Y,Segment.X,0)*Offset;
      const FVector Delta=Goal-Runtime->Position(ID); const float Arrival=bIndividual && U->CoverSlot!=INDEX_NONE?25:90;
      if(Delta.Size2D()<Arrival) { ++*Index; if(*Index>=Route->Num() && bIndividual) Paths.Remove(ID); }
      else Wish=Delta.GetSafeNormal2D()*FMath::Min(T->MoveSpeed,float(Delta.Size2D()/FMath::Max(Dt,.001f)));
     }
    }
    Desired.Add(ID,Wish);
   }
  }
  {
   FPhase3Scope Scope(this,EPhase3Cost::Avoidance);
   for(const auto& Pair:Desired)
   {
    FNavAvoidanceData Data; Data.Center=Runtime->Position(Pair.Key); Data.Velocity=Pair.Value; Data.Velocity.Z=0;
    Data.Radius=36*Avoidance->ArtificialRadiusExpansion; Data.HalfHeight=90; Data.Weight=Pair.Value.IsNearlyZero()?1.f:.5f;
    Data.GroupMask=1; Data.GroupsToAvoid=-1; Data.GroupsToIgnore=0; Data.OverrideWeightTime=0; Data.RemainingTimeToLive=Avoidance->DefaultTimeToLive; Data.TestRadius2D=350;
    Avoidance->Publish(AvoidanceIDs.FindChecked(Pair.Key),Data);
   }
   Solved.Reset();
   for(const auto& Pair:Desired)
   {
    const int32 UID=AvoidanceIDs.FindChecked(Pair.Key); auto* Data=Avoidance->GetAvoidanceObjectForUID(UID);
    FVector V=Pair.Value.IsNearlyZero()?FVector::ZeroVector:Avoidance->GetAvoidanceVelocityIgnoringUID(*Data,Avoidance->DeltaTimeToPredict,UID);
    if(!V.Equals(Pair.Value,1)) ++RVOChanges; Solved.Add(Pair.Key,V);
   }
  }
  {
   FPhase3Scope Scope(this,EPhase3Cost::Follow);
   for(const auto& Pair:Solved)
   {
    const int32 ID=Pair.Key; auto& B=Runtime->Bodies.FindChecked(ID); const auto* U=Store->Find(ID); if(!U->IsAlive()) continue;
    const FVector Before=Runtime->Position(ID),V=Pair.Value; FVector Direction=Runtime->Forward(ID);
    if(!V.IsNearlyZero()) Direction=V.GetSafeNormal2D();
    if(B.Actor.IsValid())
    {
     B.Actor->GetCharacterMovement()->MaxWalkSpeed=V.Size2D();
     if(V.IsNearlyZero()) B.Actor->GetCharacterMovement()->StopMovementImmediately(); else B.Actor->AddMovementInput(V.GetSafeNormal2D());
     B.Velocity=B.Actor->GetVelocity();
    }
    else
    {
     FHitResult Hit; B.Capsule->MoveComponent(V*Dt,Direction.Rotation().Quaternion(),true,&Hit);
     auto& F=Runtime->Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);
     F.Position=B.Capsule->GetComponentLocation(); B.Velocity=(F.Position-Before)/FMath::Max(Dt,.001f);
    }
    if(Desired.FindRef(ID).SizeSquared2D()>100 && B.Velocity.Size2D()<10)
    { if(B.StalledSince==0) B.StalledSince=Now; if(!bIndividual && Now-B.StalledSince>2) if(auto* Plan=Runtime->Plans.Find(U->SquadID)) Plan->bReplan=true; }
    else B.StalledSince=0;
    if(!bIndividual && !(B.Actor.IsValid() && B.Actor->IsPlayerControlled()))
    {
     if(auto* Plan=Runtime->Plans.Find(U->SquadID))
     {
      const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,Plan->TargetID);
      if(Contact && Contact->bVisible && !Contact->bShared && Runtime->CanSee(ID,Plan->TargetID))
      {
       Direction=(Contact->LastKnownPosition+FVector(0,0,35)-(Before+FVector(0,0,60))).GetSafeNormal();
       if(U->Ammo==0) Store->StartReload(ID,Now); else Runtime->Fire(ID,Before+FVector(0,0,60),Direction,false);
      }
     }
    }
    if(!bIndividual || B.Velocity.Size2D()>20) Rotate(ID,FRotator(0,Direction.Rotation().Yaw,0));
    Store->UpdateAim(ID,Now,B.Velocity.Size2D(),Runtime->Forward(ID),true);
    if(!B.Actor.IsValid())
    {
     const int32 Pose=U->ReloadEndTime>Now?2:B.Velocity.SizeSquared2D()>400?1:0;
     if(Pose!=B.Pose) { B.Pose=Pose; if(Runtime->VAT)Runtime->VAT->ChangePose(ID,Pose); if(!Runtime->VAT || !Runtime->VAT->IsFar(ID))B.Mesh->PlayAnimation(Runtime->FollowerClip(ID,Pose),true); }
    }
   }
  }
 }
 int32 Alive[2]={}; for(const auto& U:Store->GetUnits()) if(U.Value.IsAlive()) ++Alive[U.Value.Team];
 if(!Alive[0] || !Alive[1]) Mode->bBattleEnded=true;
 { FPhase3Scope Scope(this,EPhase3Cost::Display); Runtime->RefreshDisplay(); ApplyLOD(); }
}
void UBattlePhase3::ApplyLOD()
{
 auto* PC=GetWorld()->GetFirstPlayerController(); FVector Eye; FRotator R; PC->GetPlayerViewPoint(Eye,R);
 const double FOV=PC->PlayerCameraManager->GetFOVAngle(); const double Scale=180.*960./FMath::Tan(FMath::DegreesToRadians(FOV*.5));
 const double Near=Scale/120.,Far=Scale/48.;
 for(const auto& Pair:Runtime->Bodies)
 {
  const double Distance=FVector::Distance(Eye,Runtime->Position(Pair.Key));
  int32 LOD=LODs.Contains(Pair.Key)?LODs[Pair.Key]:Distance<Near?0:Distance<Far?1:2;
  while(LOD<2 && Distance>=(LOD==0?Near:Far)*1.2) ++LOD;
  while(LOD>0 && Distance<(LOD==1?Near:Far)*.8) --LOD;
  const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(Pair.Key);
  if(U->bCanPlayerControl) LOD=0;
  auto* Mesh=Pair.Value.Actor.IsValid()?Pair.Value.Actor->GetMesh():Pair.Value.Mesh.Get(); Mesh->SetForcedLOD(LOD+1); LODs.Add(Pair.Key,LOD);
  if(Runtime->VAT)Runtime->VAT->Select(Pair.Key,LOD,Distance,Scale,View);
 }
}
void UBattlePhase3::Sample()
{
 auto* Telemetry=GetWorld()->GetSubsystem<UBattleMetrics>();
 const auto Metrics=Telemetry->DrainMixedFrame();
 const double Wall=FPlatformTime::Seconds(),Now=GetWorld()->GetTimeSeconds()-Started;
 if(Now>=Warmup && Now<Warmup+Duration)
 {
  auto* Store=GetWorld()->GetSubsystem<UBattleState>(); int32 Alive=0,Actors=0,Moved=0; double Distance=0;
  for(const auto& B:Runtime->Bodies) if(Store->Find(B.Key)->IsAlive()) { ++Alive; Actors+=B.Value.Actor.IsValid(); const double D=FVector::Dist2D(InitialPositions[B.Key],Runtime->Position(B.Key)); Distance+=D; Moved+=D>100; }
  FString Actual=View; if(View!=TEXT("RTS") && Cast<ABattleController>(GetWorld()->GetFirstPlayerController())->IsTactical()) Actual=TEXT("RTS_after_death");
  Rows.Add(FString::Printf(TEXT("%.6f,%s,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%lld,%lld,%lld,%lld"),
   Now,*Actual,Alive,Actors,Alive-Actors,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),(Wall-LastWall)*1000,FPlatformTime::ToMilliseconds(GGameThreadWaitTime),FPlatformTime::ToMilliseconds(GRenderThreadWaitTime),
   Costs[0],Costs[1],Costs[2],Costs[3],Costs[4],Costs[5],Costs[6],PathRequests,PathFailures,Decisions,Shots,Deaths,RVOChanges,AvoidanceIDs.Num(),Moved,Alive?Distance/Alive:0,Metrics.SightScanMS,Metrics.ReportDeliveryMS,Metrics.ReportQueueMS,DecisionLateMS,Telemetry->SensorLatenessMaxMS,Telemetry->PendingReportsPeak,Telemetry->MoveFailures,Telemetry->CoverScores,Telemetry->CoordinationChanges));
 }
 LastWall=Wall; DecisionLateMS=0; for(double& C:Costs) C=0;
}
void UBattlePhase3::Finish()
{
 if(BoneTrial && !BoneTrial->Done) ++Failures;
 if(AnimTaskTrial && !AnimTaskTrial->Done) ++Failures;
 bFinished=true;
 if(bContextReplay) FFileHelper::SaveStringArrayToFile(ContextReplayRows,*(Output+TEXT("-context-state.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 if(bSenseAudit || bContextAudit || bGridAudit)
 {
  FFileHelper::SaveStringArrayToFile(SenseEvents,*(Output+TEXT("-sense.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  UE_LOG(LogFutureBattle,Display,TEXT("P3_SENSE_AUDIT frames=%lld contact_mismatch=%lld timer_mismatch=%lld report_mismatch=%lld"),AuditFrames,ContactMismatchFrames,TimerMismatchFrames,ReportMismatchFrames);
  if(bContextAudit || bGridAudit)
  {
   if(AuditFrames==0 || ContactMismatchFrames || TimerMismatchFrames || ReportMismatchFrames) ++Failures;
   UE_LOG(LogFutureBattle,Display,TEXT("P3_%s_AUDIT frames=%lld contacts=%lld timers=%lld reports=%lld direct=%d/%d shared=%d/%d"),bGridAudit?TEXT("GRID"):TEXT("CONTEXT"),AuditFrames,ContactMismatchFrames,TimerMismatchFrames,ReportMismatchFrames,FirstDirect[0].Num(),FirstDirect[1].Num(),FirstShared[0].Num(),FirstShared[1].Num());
  }
 }
 auto* Work=GetWorld()->GetSubsystem<UBattleMetrics>();
 UE_LOG(LogFutureBattle,Display,TEXT("P3_GRID_WORK observers=%lld candidates=%lld far_skipped=%lld"),Work->PerceptionObservers,Work->PerceptionCandidates,Work->FarCandidatesSkipped);
 UE_LOG(LogFutureBattle,Display,TEXT("P3_BATCH_LATENESS max_ms=%.6f"),BatchLateMS);
 if(FParse::Param(FCommandLine::Get(),TEXT("P3Functional"))) VerifyCombat();
 if(Rows.Num()<2) ++Failures;
 FFileHelper::SaveStringArrayToFile(Rows,*(Output+TEXT(".csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogFutureBattle,Display,TEXT("P3_RESULT failures=%d frames=%d paths=%lld path_failures=%lld decisions=%lld shots=%lld deaths=%lld rvo_changes=%lld"),Failures,Rows.Num()-1,PathRequests,PathFailures,Decisions,Shots,Deaths,RVOChanges);
 FGenericPlatformMisc::RequestExit(false);
}
void UBattlePhase3::Deinitialize()
{ Drivers.Reset(); Super::Deinitialize(); }

void UBattlePhase3::AdvanceSense(UBattlePerception* Sense,bool Batch,double Now,double& Next,int32& Slice,int32 AuditIndex)
{
 auto Update=[&](int32 Part,int32 Count)
 {
  Sense->Update(Mode->Units,Now,Part,Count);
  if(!bSenseAudit && !bContextAudit && !bGridAudit) return;
  for(const auto& Body:Runtime->Bodies) if(const auto* Contacts=Sense->GetContacts(Body.Key)) for(const auto& P:*Contacts)
  {
   const auto& C=P.Value; const FIntPoint Key(Body.Key,P.Key);
   const bool Direct=C.bVisible && !C.bShared;
   if(!Direct && !C.bShared) continue;
   auto& Seen=Direct?FirstDirect[AuditIndex]:FirstShared[AuditIndex];
   if(Seen.Contains(Key)) continue; Seen.Add(Key);
   const bool IsB=AuditIndex==0?bSenseBatch:!bSenseBatch;
   const TCHAR* Label=bGridAudit?(Sense->bUseSpatialGrid?TEXT("ON"):TEXT("OFF")):bContextAudit?(Sense->bReuseSightContext?TEXT("ON"):TEXT("OFF")):(IsB?TEXT("B"):TEXT("A"));
   SenseEvents.Add(FString::Printf(TEXT("%s,%s,%d,%d,%.9f,%.9f,%d,%.9f,%.9f,%.9f"),Label,Direct?TEXT("direct"):TEXT("shared"),Body.Key,P.Key,Now-Started,C.LastSeenTime-Started,C.SourceUnitID,C.LastKnownPosition.X,C.LastKnownPosition.Y,C.LastKnownPosition.Z));
  }
 };
 if(!Batch) { Update(-1,0); return; }
 const int32 Slices=FMath::Clamp(GetDefault<UBattleTuning>()->PerceptionSlices,1,16);
 if(Next<=0.) Next=Now;
 int32 UpdatedSlices=0;
 while(Now>=Next && UpdatedSlices++<Slices)
 {
  if(AuditIndex==0) BatchLateMS=FMath::Max(BatchLateMS,(Now-Next)*1000.);
  Update(Slice%Slices,Slices); ++Slice; Next+=.2/Slices;
 }
 if(Now-Next>.2) Next=Now+.2/Slices;
}

void UBattlePhase3::CompareSense(double Now)
{
 // Diagnostic-only, same world inputs and Now; shadow never supplies AI decisions.
 TMap<int32,FBattleObserver> A,B; TArray<FBattleReport> RA,RB;
 GetWorld()->GetSubsystem<UBattlePerception>()->Capture(A,RA,Now); ShadowSense->Capture(B,RB,Now);
 auto EqualContact=[](const FBattleContact& X,const FBattleContact& Y)
 { return X.TargetID==Y.TargetID && X.LastKnownPosition==Y.LastKnownPosition && X.LastSeenTime==Y.LastSeenTime && X.Confidence==Y.Confidence && X.bVisible==Y.bVisible && X.SourceUnitID==Y.SourceUnitID && X.bShared==Y.bShared; };
 bool Contacts=A.Num()==B.Num(),Timers=Contacts,Reports=RA.Num()==RB.Num();
 for(const auto& P:A)
 {
  const auto* Other=B.Find(P.Key); if(!Other) { Contacts=Timers=false; continue; }
  Contacts &= P.Value.ProfileRevision==Other->ProfileRevision && P.Value.Contacts.Num()==Other->Contacts.Num();
  Timers &= P.Value.DetectionStarted.Num()==Other->DetectionStarted.Num();
  for(const auto& C:P.Value.Contacts) { const auto* Found=Other->Contacts.Find(C.Key); Contacts &= Found && EqualContact(C.Value,*Found); }
  for(const auto& D:P.Value.DetectionStarted) { const auto* Found=Other->DetectionStarted.Find(D.Key); Timers &= Found && D.Value==*Found; }
 }
 if(Reports) for(int32 I=0;I<RA.Num();++I) Reports &= RA[I].ReceiverID==RB[I].ReceiverID && RA[I].SourceRevision==RB[I].SourceRevision && RA[I].ReceiverRevision==RB[I].ReceiverRevision && RA[I].DeliveryTime==RB[I].DeliveryTime && EqualContact(RA[I].Contact,RB[I].Contact);
 if(bContextAudit || bGridAudit)
 {
  auto* Main=GetWorld()->GetSubsystem<UBattlePerception>();
  Timers &= Main->NextScanTimes.OrderIndependentCompareEqual(ShadowSense->NextScanTimes) && Main->LastScanTimes.OrderIndependentCompareEqual(ShadowSense->LastScanTimes);
  Reports &= Main->PendingCounts.OrderIndependentCompareEqual(ShadowSense->PendingCounts);
  TArray<int32> KA,KB; A.GetKeys(KA); B.GetKeys(KB); Contacts &= KA==KB;
  for(const auto& P:A) if(const auto* Other=B.Find(P.Key))
  {
   P.Value.Contacts.GetKeys(KA); Other->Contacts.GetKeys(KB); Contacts &= KA==KB;
   P.Value.DetectionStarted.GetKeys(KA); Other->DetectionStarted.GetKeys(KB); Timers &= KA==KB;
  }
 }
 ++AuditFrames; ContactMismatchFrames+=!Contacts; TimerMismatchFrames+=!Timers; ReportMismatchFrames+=!Reports;
}


