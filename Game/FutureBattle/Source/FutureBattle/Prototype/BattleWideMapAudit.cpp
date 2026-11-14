#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleController.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleTactics.h"
#include "BattleCover.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

namespace { bool MapRestart=false;int32 MapFailures=0; }
void UBattleLeaderRuntime::RunWideMapAudit()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Later=[&](float Seconds){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWideMapAudit,Seconds,false);};
 auto Check=[&](bool OK,const TCHAR* Name){if(!OK)++MapFailures;UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Name);};
 if(MapRestart)
 {
  Check(Mode->Units.Num()==50 && Bodies.Num()==500 && Match->Objectives.Num()==3 && Match->Elapsed==0,TEXT("F5 restores fresh mixed trial map"));
  MapRestart=false;UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_RESULT failures=%d"),MapFailures);
  FPlatformMisc::RequestExitWithStatus(false,MapFailures?1:0);return;
 }
 if(MapAuditStage>=10)
 {
  if(MapAuditStage==12){UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_RESULT failures=%d scope=geometry_only"),MapFailures);FPlatformMisc::RequestExitWithStatus(false,MapFailures?1:0);return;}
  const int32 Sign=MapAuditStage==10?1:-1;
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())){Camera->SetActorLocation(FVector(-1000*Sign,17000*Sign,0));Camera->Zoom(9000-Camera->Camera->GetRelativeLocation().Z);}
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+(Sign>0?TEXT("-bay-north.png"):TEXT("-bay-south.png")),true,false);
  ++MapAuditStage;Later(1.f);return;
 }
 if(MapAuditStage==0)
 {
  MapFailures=0;Check(BattleFieldLayout::IsSightField(GetWorld()) && Bodies.Num()==500 && Mode->Units.Num()==50,TEXT("new map starts mixed runtime"));
  TArray<AStaticMeshActor*> Geometry;for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It) Geometry.Add(*It);
  int32 Missing=0;
  for(auto* A:Geometry)
  {
   const FVector P=A->GetActorLocation();const FVector Mirror(-P.X,-P.Y,P.Z);
   const bool Found=Geometry.ContainsByPredicate([&](const auto* B){return B->GetActorLocation().Equals(Mirror,1) && B->GetActorScale3D().Equals(A->GetActorScale3D(),.001);});
   Missing+=!Found;
  }
  Check(Missing==0,TEXT("geometry point symmetry within 1cm"));
  if(BattleFieldLayout::IsExpanded(GetWorld()))for(int32 I=0;I<3;++I)for(int32 Sign:{-1,1})
  {
   const FVector Origin=BattleFieldLayout::Objective(I,true)+FVector(0,4500*Sign,0);
   const FVector Target=Origin+BattleFieldLayout::Expand(FVector(1100*Sign,0,100),true);
   const FVector Front=Origin+BattleFieldLayout::Expand(FVector(3100*Sign,0,160),true);
   const FVector Side=Target+BattleFieldLayout::Expand(FVector(0,1800*Sign,60),true);
   const auto F=BattleTactics::ExposureAt(GetWorld(),Target,FVector(Sign,0,0),Front);
   const auto S=BattleTactics::ExposureAt(GetWorld(),Target,FVector(Sign,0,0),Side);
   Check(F.VisibleSamples>0&&F.VisibleSamples<S.VisibleSamples,TEXT("expanded objective directional cover rewards side angle"));
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   auto* Path=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),Front-FVector(0,0,160),Side-FVector(0,0,160)):nullptr;
   const bool Reach=Path&&Path->IsValid()&&!Path->IsPartial();bool Clear=Reach;
   if(Reach)for(int32 J=1;J<Path->PathPoints.Num();++J)Clear&=!GetWorld()->SweepTestByObjectType(Path->PathPoints[J-1]+FVector(0,0,100),Path->PathPoints[J]+FVector(0,0,100),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,90));
   Check(Reach&&Path->GetPathLength()<=5000,TEXT("expanded objective keeps a local flank within 50m"));
   Check(Clear,TEXT("expanded objective flank clears full soldier capsule"));
   auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
   Check(!Cover->NearbySlots(Target+FVector(0,787.5*Sign,0),350,6).IsEmpty(),TEXT("expanded frontal cover exposes usable action slots near its end"));
   UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_OBJECTIVE_FLANK point=%d side=%d front=%d side_samples=%d path_cm=%.1f"),I,Sign,F.VisibleSamples,S.VisibleSamples,Reach?Path->GetPathLength():-1);
  }
  if(GetWorld()->GetMapName().EndsWith(TEXT("Lvl_BattleWide_C")))
  {
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   for(int32 Sign:{-1,1})
   {
    const FVector Origin(0,Sign*16000,0);
    const FVector Target=Origin+FVector(0,0,100);
    const FVector Front=Origin+FVector(-3000*Sign,0,160);
    const FVector Side=Origin+FVector(0,2000*Sign,160);
    const auto FrontExposure=BattleTactics::ExposureAt(GetWorld(),Target,FVector(-Sign,0,0),Front);
    const auto SideExposure=BattleTactics::ExposureAt(GetWorld(),Target,FVector(-Sign,0,0),Side);
    Check(FrontExposure.VisibleSamples>0 && FrontExposure.VisibleSamples<5,TEXT("bay frontal cover protects body but permits return fire"));
    Check(SideExposure.VisibleSamples==5 && SideExposure.VisibleSamples>FrontExposure.VisibleSamples,TEXT("bay side position defeats frontal physical cover"));
    Check(BattleTactics::HasTerrainLine(GetWorld(),Target+FVector(0,0,60),Front),TEXT("bay defender can fire over low cover"));
    const FVector Start=Front-FVector(0,0,160),End=Side-FVector(0,0,160);
    auto* Path=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),Start,End):nullptr;
    const bool Reach=Path && Path->IsValid() && !Path->IsPartial();
    Check(Reach && Path->GetPathLength()<=5000,TEXT("bay short flank is reachable within existing 50m path budget"));
    bool Clear=Reach;int32 Exposed=0,Total=0;
    if(Reach)for(int32 J=1;J<Path->PathPoints.Num();++J)
    {
     const FVector A=Path->PathPoints[J-1],B=Path->PathPoints[J];
     Clear&=!GetWorld()->SweepTestByObjectType(A+FVector(0,0,100),B+FVector(0,0,100),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,90));
     const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector::Dist(A,B)/200));
     for(int32 K=0;K<Count;++K){++Total;Exposed+=BattleTactics::HasTerrainLine(GetWorld(),FMath::Lerp(A,B,(K+.5)/Count)+FVector(0,0,160),Target+FVector(0,0,60));}
    }
    Check(Clear,TEXT("bay flank path clears full soldier capsule"));
    Check(Total>0 && Exposed<Total,TEXT("bay approach provides some actual line-of-sight protection"));
    UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_FLANK_BAY side=%d front_samples=%d side_samples=%d path_cm=%.1f exposed=%d samples=%d"),Sign,FrontExposure.VisibleSamples,SideExposure.VisibleSamples,Reach?Path->GetPathLength():-1,Exposed,Total);
   }
  }
  for(const auto& U:Store->GetUnits()) if(U.Value.Team==0)
  {
   const FVector P=Position(U.Key);bool Found=false;
   for(const auto& V:Store->GetUnits()) if(V.Value.Team==1 && V.Value.Weapon==U.Value.Weapon && V.Value.bActorRepresentation==U.Value.bActorRepresentation && Position(V.Key).Equals(FVector(-P.X,-P.Y,P.Z),1)) {Found=true;break;}
   Missing+=!Found;
  }
  Check(Missing==0,TEXT("spawn point symmetry within 1cm"));
  bool SymmetricOrders=true;
  for(int32 G=0;G<25;++G) SymmetricOrders&=(Match->Objectives[Match->ChooseObjective(G,0)].Position+Match->Objectives[Match->ChooseObjective(G,1)].Position).IsNearlyZero(1);
  Check(SymmetricOrders,TEXT("neutral objective assignments are point symmetric"));
  const bool Swap=FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"));
  Match->Objectives[Swap?2:0].Owner=0;Match->Objectives[Swap?0:2].Owner=1;
  SymmetricOrders=true;
  for(int32 G=0;G<25;++G) SymmetricOrders&=(Match->Objectives[Match->ChooseObjective(G,0)].Position+Match->Objectives[Match->ChooseObjective(G,1)].Position).IsNearlyZero(1);
  Check(SymmetricOrders,TEXT("captured objective assignments are point symmetric"));
  for(auto& P:Match->Objectives) P.Owner=INDEX_NONE;
  for(int32 I=0;I<3;++I)
  {
   Check(Match->Objectives[I].Position.Equals(BattleFieldLayout::Objective(I,BattleFieldLayout::IsExpanded(GetWorld())),1),TEXT("objective coordinates connected"));
   int32 Visible=0,Samples=0,NearBlocked=0;
   const FVector P=Match->Objectives[I].Position;
   for(int32 X=-10;X<=10;++X)for(int32 Y=-10;Y<=10;++Y)if(X*X+Y*Y<=100 && (X || Y))
   {++Samples;const bool Clear=!GetWorld()->LineTraceTestByObjectType(P+FVector(0,0,170),P+FVector(X*1000,Y*1000,170),FCollisionObjectQueryParams(ECC_WorldStatic));Visible+=Clear;NearBlocked+=!Clear && X*X+Y*Y<=25;}
   Check(NearBlocked>0,TEXT("objective has a blind direction within 50m"));
   UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_VISIBILITY point=%d visible=%d samples=%d fraction=%.4f grid_cm=1000 eye_cm=170"),I,Visible,Samples,double(Visible)/Samples);
   // Two authored via-points establish distinct candidate approaches; report exposure separately.
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   for(int32 Sign:{-1,1})
   {
    const double Direction=I==2?-1.:1.;const FVector Home(-BattleFieldLayout::HomeDistance(GetWorld())*Direction,0,0);
    const FVector Via=Sign==1 ? P+FVector(0,6000*Direction,0) : (Home+P)*.5+FVector(0,-4000*Direction,0);
    auto* A=Nav->FindPathToLocationSynchronously(GetWorld(),Home,Via);
    auto* B=Nav->FindPathToLocationSynchronously(GetWorld(),Via,P);
    const bool OK=A && B && A->IsValid() && B->IsValid() && !A->IsPartial() && !B->IsPartial();
    Check(OK,TEXT("alternative approach via-point reachable"));
    UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_APPROACH point=%d side=%d length_cm=%.1f"),I,Sign,OK?A->GetPathLength()+B->GetPathLength():-1);
    if(OK)
    {
     int32 Exposed=0,Total=0;
     for(auto* Route:{A,B}) for(int32 J=1;J<Route->PathPoints.Num();++J)
     {
      const FVector From=Route->PathPoints[J-1],To=Route->PathPoints[J];
      const int32 N=FMath::Max(1,FMath::CeilToInt(FVector::Dist(From,To)/500));
      for(int32 K=0;K<N;++K){const FVector Q=FMath::Lerp(From,To,(K+.5)/N);++Total;Exposed+=!GetWorld()->LineTraceTestByObjectType(Q+FVector(0,0,170),P+FVector(0,0,170),FCollisionObjectQueryParams(ECC_WorldStatic));}
     }
     UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_EXPOSURE point=%d side=%d visible_samples=%d total=%d fraction=%.4f"),I,Sign,Exposed,Total,double(Exposed)/FMath::Max(1,Total));
    }
   }
   Check(IssueOrder(I+1,EBattleOrder::Move,P),TEXT("ten-person squad accepts objective move"));
  }
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())){Camera->SetActorLocation(FVector::ZeroVector);const float Height=BattleFieldLayout::IsExpanded(GetWorld())?130000:100000;Camera->Zoom(Height);Check(Camera->Camera->GetRelativeLocation().Z==Height,TEXT("RTS camera supports whole field"));}
  if(FParse::Param(FCommandLine::Get(),TEXT("BattleWideMapGeometryOnly")))
  {
   FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
   if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-overview.png"),true,false);
   if(GetWorld()->GetMapName().EndsWith(TEXT("Lvl_BattleWide_C"))){MapAuditStage=10;Later(1.f);return;}
   UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_RESULT failures=%d scope=geometry_only"),MapFailures);
   FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExitWithStatus(false,MapFailures?1:0);}),1.f,false);return;
  }
  if(GetWorld()->GetMapName().EndsWith(TEXT("Lvl_BattleWide_C")))for(int32 SID:{1,2})
  {
   const int32 Sign=SID==1?1:-1;const FVector Origin(0,Sign*16000,0);
   const auto* Squad=Store->FindSquad(SID);int32 Index=0;
   for(int32 ID:Squad->Members)
   {
    const FVector P=Origin+FVector((-3000-(Index/5)*120)*Sign,(Index%5-2)*100,100);++Index;
    auto& B=Bodies.FindChecked(ID);
    if(B.Actor.IsValid())B.Actor->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
    else {B.Capsule->SetWorldLocation(P);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=P;}
   }
   Check(IssueOrder(SID,EBattleOrder::Move,Origin+FVector(0,2000*Sign,0)),TEXT("ten-person squad accepts short bay flank"));
  }
  Mode->bCombatPaused=false;Mode->CombatStartsAt=0;MapAuditStart=GetWorld()->GetTimeSeconds();MapAuditStage=1;Later(.5f);return;
 }
 if(MapAuditStage==1)
 {
  int32 Arrived=0;
  for(int32 SID=1;SID<=3;++SID) for(int32 ID:Store->FindSquad(SID)->Members)
   if(const auto* G=Plans.FindChecked(SID).ArrivalGoals.Find(ID);G && FVector::Dist2D(Position(ID),*G)<100)++Arrived;
  const double Elapsed=GetWorld()->GetTimeSeconds()-MapAuditStart;
  if(Arrived<30 && Elapsed<(BattleFieldLayout::IsExpanded(GetWorld())?180:140)){Later(.5f);return;}
  Check(Arrived==30,TEXT("three ten-person squads reach reserved objective slots within 1m"));
  UE_LOG(LogFutureBattle,Display,TEXT("WIDE_AUDIT_ARRIVAL count=%d elapsed=%.2f"),Arrived,Elapsed);
  Mode->bCombatPaused=true;
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-overview.png"),true,false);
  MapAuditStage=2;Later(1);return;
 }
 MapRestart=true;
 auto* View=GetWorld()->GetGameViewport()->Viewport;
 PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::F5,IE_Pressed,FPlatformTime::Cycles64()));
 PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::F5,IE_Released,FPlatformTime::Cycles64()));
}
