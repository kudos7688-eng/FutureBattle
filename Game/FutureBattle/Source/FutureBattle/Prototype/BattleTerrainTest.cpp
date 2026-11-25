// Height differences (map G, BattleTerrain.h): self-test -BattleHeightTest (headless, a map with heights). Three rifle squads of
// team 0 are ordered (Move) onto a hilltop, onto the upper floor of a house and across the ridge to its far side. After 300s
// BATTLE_HEIGHT_TEST reports per squad how many members stand on the level of their goal and how many soldiers of the whole
// team are off the ground (above or below the level they stand on), and BATTLE_HEIGHT_RESULT counts the failures. The match goes
// on around them (they may be shot later), so the best second counts, not the last.
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

namespace
{
 struct FHeightTestRow { int32 Squad=INDEX_NONE; FVector Goal=FVector::ZeroVector; const TCHAR* Name=TEXT(""); bool bAccepted=false; double MaxFeet=0; int32 Members=0,BestOnLevel=0,BestNear=0; };
 TArray<FHeightTestRow> HeightTestRows;
 int32 HeightTestState=0; // 0 not started, 1 running, 2 reported
 float HeightTestClock=0;
}

// Sector orders audit (BattleSectorAudit.cpp, -BattleSectorAudit): shares this per-tick entry.
void BattleSectorAudit(UBattleMatch& Match,float DeltaSeconds);

namespace
{
 // Terrain look check (2026-11-25, map L, Docs/design-terrain-field.md): -BattleTerrainShot -BattleWideScreens=<prefix>, rendered,
 // with -BattleHeightTest. Fixed views of the rolling ground at the start (the whole map from above, the knoll, the ridge, the river
 // valley), then the first height-test squad on its way up the knoll, close; quits once the height test has reported.
 // Setup/Test-TerrainShot.ps1 runs it. Display only: the match itself is not changed.
 int32 TerrainShotStage=0;bool bTerrainShotAimed=false;float TerrainShotWait=0;
 TWeakObjectPtr<ACameraActor> TerrainShotCamera;
}

void UBattleMatch::UpdateTerrainTest(float DeltaSeconds)
{
 BattleSectorAudit(*this,DeltaSeconds);
 static const bool bShot=FParse::Param(FCommandLine::Get(),TEXT("BattleTerrainShot"));
 if(bShot&&(TerrainShotWait-=DeltaSeconds)<=0)
 {
  TerrainShotWait=.5f;
  auto* PC=GetWorld()->GetFirstPlayerController();auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  const auto& Field=BattleFieldLayout::Of(GetWorld()).Heights;
  auto G=[&](double X,double Y){return Field.Base(X,Y);};
  // Views: eye, look-at, name, earliest match second. The squad views follow the leader of the first height-test squad.
  struct FView { FVector Eye,At;const TCHAR* Name;float From; };
  const FView Views[]={
   {FVector(0,0,110000),FVector(0,1,0),TEXT("map"),1.f},
   {FVector(8000,-6000,G(8000,-6000)+1500),FVector(22500,7500,G(22500,7500)+300),TEXT("knoll"),1.f},
   {FVector(20000,-13000,G(20000,-13000)+1200),FVector(20000,-29500,G(20000,-29500)+400),TEXT("ridge"),1.f},
   {FVector(6300,3000,G(6300,3000)+900),FVector(6300,22000,G(6300,22000)),TEXT("valley"),1.f},
   {FVector::ZeroVector,FVector::ZeroVector,TEXT("squad-a"),45.f},
   {FVector::ZeroVector,FVector::ZeroVector,TEXT("squad-b"),90.f},
   {FVector::ZeroVector,FVector::ZeroVector,TEXT("squad-c"),150.f},
  };
  if(TerrainShotStage<UE_ARRAY_COUNT(Views)&&PC&&Elapsed>=Views[TerrainShotStage].From)
  {
   FView V=Views[TerrainShotStage];
   if(V.Eye.IsZero())
   {
    // The leader of the first height-test squad (west knoll), seen from behind and above.
    const auto* S=HeightTestRows.Num()&&Runtime?GetWorld()->GetSubsystem<UBattleState>()->FindSquad(HeightTestRows[0].Squad):nullptr;
    if(!S){++TerrainShotStage;return;}
    const FVector P=Runtime->Position(S->LeaderUnitID),Way=(HeightTestRows[0].Goal-P).GetSafeNormal2D();
    V.At=P;V.Eye=P-Way*900+FVector(-Way.Y,Way.X,0)*300+FVector(0,0,450);
   }
   if(!bTerrainShotAimed)
   {
    if(!TerrainShotCamera.IsValid())
    {
     TerrainShotCamera=GetWorld()->SpawnActor<ACameraActor>();
     auto& PP=TerrainShotCamera->GetCameraComponent()->PostProcessSettings;PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=0;
     TerrainShotCamera->GetCameraComponent()->SetFieldOfView(60);
     TerrainShotCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
    }
    // Straight down for the map view (pitch -90, north up).
    const FRotator Aim=V.Name==FString(TEXT("map"))?FRotator(-90,90,0):(V.At-V.Eye).Rotation();
    TerrainShotCamera->SetActorLocationAndRotation(V.Eye,Aim);
    PC->SetViewTarget(TerrainShotCamera.Get());PC->bAutoManageActiveCameraTarget=false;if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
    bTerrainShotAimed=true;TerrainShotWait=1.f;
   }
   else
   {
    FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
    if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+FString::Printf(TEXT("-%d-%s.png"),TerrainShotStage,V.Name),false,false);
    UE_LOG(LogFutureBattle,Display,TEXT("TERRAIN_SHOT view=%s second=%.0f eye=%s at=%s"),V.Name,Elapsed,*V.Eye.ToCompactString(),*V.At.ToCompactString());
    ++TerrainShotStage;bTerrainShotAimed=false;
   }
  }
  // After the last view and the height test's report: quit.
  if(TerrainShotStage>=UE_ARRAY_COUNT(Views)&&HeightTestState==2){UE_LOG(LogFutureBattle,Display,TEXT("TERRAIN_SHOT done"));FPlatformMisc::RequestExitWithStatus(false,0);}
 }
 static const bool bTest=FParse::Param(FCommandLine::Get(),TEXT("BattleHeightTest"));
 if(!bTest||HeightTestState==2)return;
 HeightTestClock+=DeltaSeconds;if(HeightTestClock<1.f)return;
 HeightTestClock=0;
 const auto& Heights=BattleFieldLayout::Of(GetWorld()).Heights;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(!Runtime||!Runtime->IsWideField()||Heights.IsEmpty())return;
 // The level a soldier stands on and how far his capsule centre is from where it belongs above it (leaders: 90cm half height).
 auto Feet=[&](int32 ID,double& Level,double& Off)
 {
  const FVector P=Runtime->Position(ID);double Slope=0;
  Heights.Ground(P.X,P.Y,P.Z-95,Level,&Slope,1.e9);
  Off=P.Z-Level-95;
 };
 if(HeightTestState==0&&Elapsed>=2)
 {
  HeightTestState=1;HeightTestRows.Reset();
  TArray<int32> IDs;for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==0&&Pair.Value.SquadType==0&&Pair.Value.CommandStatus==EBattleCommandStatus::Active&&!Runtime->IsVehicle(Pair.Value.LeaderUnitID))IDs.Add(Pair.Key);
  IDs.Sort();
  // West-half counterparts of the first hill and the last house of the east half.
  const auto& Hill=BattleTerrain::HillsG()[0];const auto& House=BattleTerrain::HousesG().Last();
  FVector Goals[3]={FVector(-Hill.X,-Hill.Y,Hill.Top),FVector(-House.X,-House.Y+200,BattleTerrain::FloorZ),FVector(-Hill.X,-Hill.Y+Hill.H*.5+Hill.Run+800,0)};
  const TCHAR* Names[3]={TEXT("hilltop"),TEXT("upper_floor"),TEXT("across_ridge")};
  // Map L (rolling ground, BattleMapL.h): the top of the west knoll, the upper floor of the last house of map I (west half, on its
  // pad) and the crest of the west ridge. The off-ground count below then checks every soldier of the team on the slopes.
  if(!Heights.Field.IsEmpty())
  {
   const auto& HouseL=BattleMapI::Houses().Last();
   Goals[0]=FVector(-22500,-7500,Heights.Base(-22500,-7500));
   Goals[1]=FVector(-HouseL.X,-HouseL.Y+200,Heights.Base(-HouseL.X,-HouseL.Y)+BattleTerrain::FloorZ);
   Goals[2]=FVector(-20000,29500,Heights.Base(-20000,29500));
   Names[2]=TEXT("ridge_top");
  }
  for(int32 I=0;I<3&&I<IDs.Num();++I)
  {
   FHeightTestRow Row;Row.Squad=IDs[I];Row.Goal=Goals[I];Row.Name=Names[I];
   Row.bAccepted=Runtime->IssueOrder(IDs[I],EBattleOrder::Move,Goals[I]);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HEIGHT_TEST start squad=%d goal=%s x_m=%.0f y_m=%.0f z_cm=%.0f accepted=%d"),IDs[I],Names[I],Goals[I].X/100,Goals[I].Y/100,Goals[I].Z,Row.bAccepted?1:0);
   HeightTestRows.Add(Row);
  }
  return;
 }
 if(HeightTestState!=1)return;
 for(auto& Row:HeightTestRows)if(const auto* S=Store->FindSquad(Row.Squad))for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())
  {double Level,Off;Feet(ID,Level,Off);Row.MaxFeet=FMath::Max(Row.MaxFeet,Level);}
 for(auto& Row:HeightTestRows)
 {
  int32 Alive=0,OnLevel=0,Near=0;
  if(const auto* S=Store->FindSquad(Row.Squad))for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())
  {
   double Level,Off;Feet(ID,Level,Off);++Alive;const bool bNear=FVector::Dist2D(Runtime->Position(ID),Row.Goal)<2500;
   Near+=bNear;OnLevel+=bNear&&FMath::Abs(Level-Row.Goal.Z)<=20;
  }
  Row.Members=FMath::Max(Row.Members,Alive);Row.BestOnLevel=FMath::Max(Row.BestOnLevel,OnLevel);Row.BestNear=FMath::Max(Row.BestNear,Near);
 }
 if(Elapsed<300)return;
 HeightTestState=2;int32 Failures=HeightTestRows.Num()==3?0:1;
 for(const auto& Row:HeightTestRows)
 {
  // Hilltop and upper floor: at some moment at least half of the squad stood near the goal on its level. Across the ridge:
  // at least half reached the far side on the ground and somebody was up on the ridge on the way.
  const bool bOK=Row.bAccepted&&Row.Members>0&&Row.BestOnLevel*2>=Row.Members&&(Row.Goal.Z>0||Row.MaxFeet>100);
  Failures+=!bOK;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HEIGHT_TEST squad=%d goal=%s members=%d best_on_level=%d best_near=%d highest_cm=%.0f ok=%d"),Row.Squad,Row.Name,Row.Members,Row.BestOnLevel,Row.BestNear,Row.MaxFeet,bOK?1:0);
 }
 // Every soldier of the team: capsule centre within 15cm of where it belongs above the level he stands on.
 int32 Soldiers=0,OffGround=0;double Worst=0;
 for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==0&&!Runtime->IsVehicle(Pair.Value.LeaderUnitID))for(int32 ID:Pair.Value.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())
 {double Level,Off;Feet(ID,Level,Off);++Soldiers;if(FMath::Abs(Off)>15){++OffGround;}Worst=FMath::Abs(Off)>FMath::Abs(Worst)?Off:Worst;}
 Failures+=OffGround>0;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HEIGHT_TEST soldiers=%d off_ground=%d worst_cm=%.1f"),Soldiers,OffGround,Worst);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HEIGHT_RESULT failures=%d"),Failures);
}
