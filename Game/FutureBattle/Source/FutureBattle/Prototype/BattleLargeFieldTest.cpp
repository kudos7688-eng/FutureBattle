#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleFieldLayout.h"
#include "BattleMatch.h"
#include "FutureBattle.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ABattleGameMode::RunLargeFieldTest()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if (!PC || Units.Num()!=500) { CheckSmoke(false,TEXT("500 field initialized")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
 FString Prefix; FParse::Value(FCommandLine::Get(),TEXT("BattleScaleScreenshot="),Prefix);
 float Delay=1.f;
 switch(ScaleSmokeStage++)
 {
 case 0:
 {
  CheckSmoke(bCombatPaused && bBenchmarkOrdersReady && Store->GetSquads().Num()==100,TEXT("500 soldiers / 100 squads start paused with enemy orders ready"));
  auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
  CheckSmoke(Match->Phase==EBattleMatchPhase::Combat && Match->Objectives.Num()==3 && Match->Objectives[0].Position.Equals(FVector(0,-4500,0)) && Match->Objectives[2].Position.Equals(FVector(0,4500,0)),TEXT("Large field connects three control points to playable match"));
  CheckSmoke(!Match->HireSquad(0,EBattleTraining::Regular) && !Match->RequestSupport(Units[0]->GetUnitID()),TEXT("Fixed forces reject reinforcements and support strikes"));
  const FVector AllyStart=Units[0]->GetActorLocation(), EnemyStart=Units[250]->GetActorLocation();
  Units[0]->SetActorLocation(FVector(0,0,95)); bCombatPaused=false;
  Match->UpdateObjectives(8.1f); Match->UpdateObjectives(1.f);
  CheckSmoke(Match->Objectives[1].Owner==0 && FMath::IsNearlyEqual(Match->Score[0],1.f),TEXT("Soldier standing on new central point captures and scores"));
  Units[250]->SetActorLocation(FVector(100,0,95)); Match->UpdateObjectives(2.f);
  CheckSmoke(Match->Objectives[1].bContested && FMath::IsNearlyEqual(Match->Score[0],1.f),TEXT("Opponent entering new point contests and stops score"));
  Units[0]->SetActorLocation(AllyStart); Units[250]->SetActorLocation(EnemyStart); bCombatPaused=true;
  for(auto& Point:Match->Objectives) { const FVector P=Point.Position; Point=FBattleObjective(); Point.Position=P; }
  Match->Score[0]=Match->Score[1]=0;
  int32 Paths=0; auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
  for (int32 I=0; I<Units.Num(); ++I) {
   auto* U=Units[I].Get(); ScaleSmokeStarts.Add(U->GetActorLocation());
   FVector End=U->GetActorLocation(); End.X=-End.X;
   auto* Path=Nav ? Nav->FindPathToLocationSynchronously(GetWorld(),U->GetActorLocation(),End,U) : nullptr;
   if(Path && Path->IsValid() && !Path->IsPartial()) ++Paths;
  }
  UE_LOG(LogFutureBattle,Display,TEXT("LARGE_FIELD cross-field paths=%d/500"),Paths);
  CheckSmoke(Paths==500,TEXT("Every soldier has a full navigation path across the 240m field"));
  CheckSmoke(!Units[0]->CanSee(Units[250]),TEXT("Opposing deployments begin beyond detection range"));
  for(int32 I=0; I<250; I+=5) { PC->SetSelection(Units[I]); FVector Goal=BattleFieldLayout::Goal(I); Goal.X=-Goal.X; PC->IssueOrderAt(Goal,EBattleOrder::AttackMove); }
  int32 Orders=0; for(int32 I=0; I<250; ++I) if(Units[I]->GetState()->Order==EBattleOrder::AttackMove) ++Orders;
  CheckSmoke(Orders==250,TEXT("All 50 friendly squads accept distributed three-lane orders"));
  auto* Cam=Cast<ABattleCameraPawn>(PC->GetPawn()); if(Cam){Cam->SetActorLocation(FVector::ZeroVector);Cam->Zoom(30000);}
  Delay=.5f; break;
 }
 case 1:
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-overview.png"),true,false);
  Delay=10.f; break;
 case 2:
 {
  int32 Moved=0; for(int32 I=0; I<250; ++I) if(FVector::Dist2D(Units[I]->GetActorLocation(),ScaleSmokeStarts[I])>2000) ++Moved;
  UE_LOG(LogFutureBattle,Display,TEXT("LARGE_FIELD moved over 20m=%d/250"),Moved);
  CheckSmoke(Moved>=200,TEXT("At least 80 percent of friendlies actually travel over 20m"));
  CheckSmoke(PC->ControlUnit(Units[4]),TEXT("Large field permits FPS possession of support soldier"));
  Delay=.5f; break;
 }
 case 3:
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-fps.png"),true,false);
  break;
 case 4:
  PC->EnterTactical();
  CheckSmoke(Units[4]->GetController()==Units[4]->SavedAI,TEXT("Returning from FPS restores AI control"));
  bCombatPaused=false; CombatStartsAt=0; Delay=35.f; break;
 default:
 {
  int32 Shots=0,Alive=0; for(ABattleUnit* U:Units){Shots+=U->GetState()->ShotSequence; Alive+=U->IsAlive()?1:0;}
  CheckSmoke(Shots>0,TEXT("Separated armies navigate into contact and fire"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LARGE_FIELD_RESULT failures=%d units=%d shots=%d alive=%d"),SmokeFailures,Units.Num(),Shots,Alive);
  if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-combat.png"),true,false);
  FTimerHandle Exit; const uint8 Code=SmokeFailures?1:0;
  GetWorldTimerManager().SetTimer(Exit,FTimerDelegate::CreateLambda([Code](){FPlatformMisc::RequestExitWithStatus(false,Code);}),3.f,false); return;
 }
 }
 FTimerHandle Timer; GetWorldTimerManager().SetTimer(Timer,this,&ABattleGameMode::RunLargeFieldTest,Delay,false);
}
