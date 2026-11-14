#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunSightVisual()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSightVisual,.7f,false);};
 Mode->bCombatPaused=true;
 const int32 Distances[]={5000,10000,15000};
 if(SightVisualStage%2==0)
 {
  const FVector Origin(-25000,-17000,100);
  Actor(1)->SetActorLocation(Origin,false,nullptr,ETeleportType::TeleportPhysics);
  Actor(1)->GetCharacterMovement()->StopMovementImmediately();
  // Use an allied body: FPS intentionally hides enemies without a sensor report.
  Actor(11)->SetActorLocation(Origin+FVector(Distances[SightVisualStage/2],200,0),false,nullptr,ETeleportType::TeleportPhysics);
  Actor(11)->GetCharacterMovement()->StopMovementImmediately();
  PC->ControlUnit(Actor(1));PC->SetControlRotation(FRotator::ZeroRotator);
  ++SightVisualStage;Later();return;
 }
 const FVector P=Position(11);
 FVector2D Low,High;const bool Projected=PC->ProjectWorldLocationToScreen(P-FVector(0,0,90),Low) && PC->ProjectWorldLocationToScreen(P+FVector(0,0,90),High);
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 const int32 D=Distances[SightVisualStage/2]/100;
 UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_VISUAL distance_m=%d view=%s height_180cm_px=%.3f fov=%.2f projected=%d"),D,Actor(1)->UsesThirdPersonView()?TEXT("TPS"):TEXT("FPS"),FMath::Abs(High.Y-Low.Y),PC->PlayerCameraManager->GetFOVAngle(),Projected);
 if(!Projected)++SmokeFailures;
 if(!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+FString::Printf(TEXT("-%dm.png"),D),true,false);
 if(++SightVisualStage<6){Later();return;}
 FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[this](){UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_VISUAL_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);}),1.f,false);
}
