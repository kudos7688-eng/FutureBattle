// Death look check (2026-11-21): -BattleDeathShot -BattleArea -BattleWideScreens=<prefix> on a wide mixed map.
// Combat is paused. A starting follower of team 0 drawn by the far display (VAT) from a high camera is killed: it must
// leave the far display and play a death clip (the far display has none), then hold its last frame with the body's tick off and
// stay out of the far display. The leader (an actor) is killed next. Close-ups while dying and dead. Setup/Test-DeathShot.ps1 runs it.
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattleVATDisplay.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/HUD.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunDeathShot()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunDeathShot,Delay,false);};
 auto Finish=[&](){UE_LOG(LogFutureBattle,Display,TEXT("DEATH_SHOT_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);};
 auto Check=[&](bool bOk,const TCHAR* What){UE_LOG(LogFutureBattle,Display,TEXT("DEATH_SHOT %s: %s"),bOk?TEXT("PASS"):TEXT("FAIL"),What);if(!bOk)++SmokeFailures;};
 static TWeakObjectPtr<ACameraActor> Camera;
 auto Look=[&](const FVector& Eye,const FVector& At)
 {
  if(!Camera.IsValid())
  {
   Camera=GetWorld()->SpawnActor<ACameraActor>();
   auto& PP=Camera->GetCameraComponent()->PostProcessSettings;PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=0;
   Camera->GetCameraComponent()->SetFieldOfView(60);
  }
  Camera->SetActorLocationAndRotation(Eye,(At-Eye).Rotation());
  PC->SetViewTarget(Camera.Get());PC->bAutoManageActiveCameraTarget=false;if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
 };
 auto Shot=[&](const TCHAR* Name)
 {
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+FString::Printf(TEXT("-%d-%s.png"),DeathShotStage,Name),false,false);
 };
 auto Clip=[&](int32 ID,float* Time=nullptr)->const UAnimationAsset*
 {
  const auto* B=Bodies.Find(ID);auto* Node=B&&B->Mesh.IsValid()?B->Mesh->GetSingleNodeInstance():nullptr;
  if(Time)*Time=Node?Node->GetCurrentTime():-1.f;
  return Node?Node->GetAnimationAsset():nullptr;
 };
 auto Close=[&](int32 ID){const FVector P=Position(ID);Look(P+FVector(-260,-260,170),P+FVector(0,0,-40));};
 Mode->bCombatPaused=true;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const int32 Follower=DeathShotIDs[1],Leader=DeathShotIDs[0];
 switch(DeathShotStage++)
 {
 case 0:
 {
  // A starting squad of team 0: its followers are registered with the far display (hired ones are not).
  for(const auto& Pair:Bodies)
  {
   const auto* U=Store->Find(Pair.Key);const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
   if(!Pair.Value.Actor.IsValid()&&U&&U->Team==0&&U->IsAlive()&&S&&S->LeaderUnitID!=Pair.Key&&!IsVehicle(S->LeaderUnitID)&&Actor(S->LeaderUnitID)){DeathShotIDs[1]=Pair.Key;DeathShotIDs[0]=S->LeaderUnitID;break;}
  }
  if(!DeathShotIDs[1]){Check(false,TEXT("a team 0 follower with a leader exists"));Finish();return;}
  const FVector P=Position(DeathShotIDs[1]);Look(P+FVector(-9000,0,9000),P);
  Later(2.f);return;
 }
 case 1:
 {
  const bool bFar=VAT&&VAT->IsFar(Follower);
  UE_LOG(LogFutureBattle,Display,TEXT("DEATH_SHOT follower=%d leader=%d far_before=%d"),Follower,Leader,bFar);
  // The check is about the far display: without a far follower it would pass whatever OnDeath does.
  Check(bFar,TEXT("the follower is drawn by the far display before it dies"));
  Shot(TEXT("high"));
  Store->ApplyDamage(Follower,1,100000.f);
  Check(!VAT||!VAT->IsFar(Follower),TEXT("a dying follower leaves the far display"));
  Check(Clip(Follower)&&Clip(Follower)==DeathClip(Follower),TEXT("a dying follower plays its death clip"));
  Close(Follower);Later(.3f);return;
 }
 case 2: Shot(TEXT("follower_dying"));Later(.6f);return;
 case 3: Shot(TEXT("follower_falling"));Later(3.f);return;
 case 4:
 {
  float T=0;const auto* A=Clip(Follower,&T);const auto* B=Bodies.Find(Follower);
  UE_LOG(LogFutureBattle,Display,TEXT("DEATH_SHOT follower clip=%s time=%.3f length=%.3f tick=%d"),A?*A->GetName():TEXT("none"),T,A?CastChecked<UAnimSequence>(A)->GetPlayLength():0.f,B&&B->Mesh.IsValid()&&B->Mesh->IsComponentTickEnabled());
  Check(A&&FMath::IsNearlyEqual(T,CastChecked<UAnimSequence>(A)->GetPlayLength(),.05f),TEXT("the corpse holds the last frame of its death clip"));
  Check(B&&B->Mesh.IsValid()&&!B->Mesh->IsComponentTickEnabled(),TEXT("the corpse's body no longer ticks"));
  Shot(TEXT("follower_dead"));
  Store->ApplyDamage(Leader,1,100000.f);
  Close(Leader);Later(.4f);return;
 }
 case 5: Shot(TEXT("leader_dying"));Later(3.f);return;
 case 6:
 {
  Shot(TEXT("leader_dead"));
  const FVector P=Position(Follower);Look(P+FVector(-9000,0,9000),P);
  Later(1.f);return;
 }
 default:
  Check(!VAT||!VAT->IsFar(Follower),TEXT("a corpse stays out of the far display seen from high up"));
  Shot(TEXT("high_after"));
  {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[this](){UE_LOG(LogFutureBattle,Display,TEXT("DEATH_SHOT_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);}),.5f,false);}
  return;
 }
}
