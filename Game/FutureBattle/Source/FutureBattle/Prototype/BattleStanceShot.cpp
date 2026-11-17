// Stance visuals check (2026-11-17): -BattleStanceShot -BattleWideScreens=<prefix> on a wide mixed map with stances on.
// Combat is paused; a leader (Actor) and one of his Mass followers are put in each stance in turn and photographed from
// 4 m in front. Logs the clip each body plays and its vertical scale. Run with and without -BattleStanceAnim=1.
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleStance.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattleVATDisplay.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/HUD.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

namespace
{
 FString ClipOf(const USkeletalMeshComponent* Mesh)
 {
  const auto* Node=Mesh?Mesh->GetSingleNodeInstance():nullptr;
  return Node&&Node->GetAnimationAsset()?Node->GetAnimationAsset()->GetName():TEXT("none");
 }
}

void UBattleLeaderRuntime::RunStanceShot()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunStanceShot,Delay,false);};
 Mode->bCombatPaused=true;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(StanceShotStage==0)
 {
  // The first living Mass follower of leader 1's team that has a near body.
  for(const auto& Pair:Bodies)
   if(!Pair.Value.Actor.IsValid()&&Pair.Value.Mesh.IsValid()&&Store->Find(Pair.Key)&&Store->Find(Pair.Key)->Team==Store->Find(1)->Team&&(StanceShotFollower==0||Pair.Key<StanceShotFollower))
    StanceShotFollower=Pair.Key;
  if(!Actor(1)||StanceShotFollower==0){UE_LOG(LogFutureBattle,Error,TEXT("STANCE_SHOT no leader or follower"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
  UE_LOG(LogFutureBattle,Display,TEXT("STANCE_SHOT enabled=%d anim=%d leader=1 follower=%d"),BattleStance::Enabled(GetWorld()),BattleStance::AnimEnabled(),StanceShotFollower);
 }
 const uint8 Order[]={BattleStance::Stand,BattleStance::Crouch,BattleStance::Prone,BattleStance::Stand};
 const TCHAR* Names[]={TEXT("stand"),TEXT("crouch"),TEXT("prone"),TEXT("stand2")};
 const int32 Step=StanceShotStage/3,Phase=StanceShotStage%3; // per stance: set, shoot leader, shoot follower
 if(Step>=4)
 {
  UE_LOG(LogFutureBattle,Display,TEXT("STANCE_SHOT_RESULT failures=%d"),SmokeFailures);
  FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);return;
 }
 const double Now=GetWorld()->GetTimeSeconds();
 if(Phase==0)
 {
  SetStance(1,Order[Step],Now,true);SetStance(StanceShotFollower,Order[Step],Now,true);
  ++StanceShotStage;Later(1.f);return;
 }
 const int32 ID=Phase==1?1:StanceShotFollower;
 const USkeletalMeshComponent* Mesh=Phase==1?Actor(1)->GetMesh():Bodies.FindChecked(ID).Mesh.Get();
 // Camera 4 m in front of the soldier at chest height, looking back at him.
 const FVector P=Position(ID);
 const FVector Forward=(Phase==1?Actor(1)->GetActorForwardVector():Bodies.FindChecked(ID).Capsule->GetForwardVector()).GetSafeNormal2D();
 // Slightly to the side so the body is not hidden behind its own rifle.
 const FVector Side=FVector::CrossProduct(FVector::UpVector,Forward);
 const FVector Eye=P+Forward*300+Side*120+FVector(0,0,30);
 static TWeakObjectPtr<ACameraActor> Camera;
 if(!Camera.IsValid())
 {
  Camera=GetWorld()->SpawnActor<ACameraActor>();
  auto& PP=Camera->GetCameraComponent()->PostProcessSettings;PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=0;
  Camera->GetCameraComponent()->SetFieldOfView(60);
 }
 Camera->SetActorLocationAndRotation(Eye,(P-FVector(0,0,20)-Eye).Rotation());
 PC->SetViewTarget(Camera.Get());
 PC->bAutoManageActiveCameraTarget=false;if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
 const FString Clip=ClipOf(Mesh);
 const bool Far=VAT&&Phase==2&&VAT->IsFar(ID);
 UE_LOG(LogFutureBattle,Display,TEXT("STANCE_SHOT stance=%s body=%s id=%d clip=%s scale_z=%.2f far=%d"),Names[Step],Phase==1?TEXT("leader"):TEXT("follower"),ID,*Clip,Mesh->GetRelativeScale3D().Z,Far);
 // With the trial on, a crouch must play the crouch clip at full height (the shot taken a frame later still shows it).
 if(BattleStance::Enabled(GetWorld())&&BattleStance::AnimEnabled()&&Order[Step]==BattleStance::Crouch&&!Far&&(!Clip.Contains(TEXT("Crouch"))||Mesh->GetRelativeScale3D().Z<.99))++SmokeFailures;
 if(!BattleStance::AnimEnabled()&&Clip.Contains(TEXT("Crouch")))++SmokeFailures;
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 // Capture after the camera has settled (temporal anti-aliasing needs a few frames at the new position).
 const FString File=Prefix+FString::Printf(TEXT("-%s-%s.png"),Names[Step],Phase==1?TEXT("leader"):TEXT("follower"));
 if(!Prefix.IsEmpty()){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[File](){FScreenshotRequest::RequestScreenshot(File,false,false);}),.5f,false);}
 ++StanceShotStage;Later(1.f);
}
