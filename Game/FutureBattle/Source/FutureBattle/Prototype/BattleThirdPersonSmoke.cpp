#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattlePerception.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FutureBattle.h"

void ABattleGameMode::RunThirdPersonSmokeTest()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 if(!PC || Units.IsEmpty()) { CheckSmoke(false,TEXT("TPS fixture initialized")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
 auto* U=Units[0].Get();
 U->SetActorLocation(FVector(-1000,-2200,100));
 const auto Before=*U->GetState();
 const float AvoidanceBefore=U->GetCharacterMovement()->GetRVOAvoidanceWeight();
 auto* Enemy=Units.Last().Get();
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 TMap<int32,FBattleObserver> ObserversBefore; TArray<FBattleReport> ReportsBefore;
 Sense->Capture(ObserversBefore,ReportsBefore,0);
 Enemy->SetRevealed(false);
 CheckSmoke(Enemy->IsHidden() && !Enemy->bRevealed,TEXT("RTS hides unspotted enemy body"));
 CheckSmoke(PC->ControlUnit(U) && U->UsesThirdPersonView(),TEXT("Normal direct control uses TPS"));
 auto* Movement=U->GetCharacterMovement();
 CheckSmoke(Movement->bUseRVOAvoidance && Movement->GetRVOAvoidanceWeight()==1.f,TEXT("Direct control skips velocity avoidance but remains an AI obstacle"));
 U->SetDirectControl(true);
 CheckSmoke(Movement->GetRVOAvoidanceWeight()==1.f,TEXT("Repeated direct-control setup does not lose saved avoidance weight"));
 CheckSmoke(!Enemy->IsHidden() && !Enemy->bRevealed,TEXT("TPS immediately renders unspotted body without revealing marker"));
 Enemy->SetRevealed(false);
 CheckSmoke(!Enemy->IsHidden() && !Enemy->bRevealed,TEXT("Sensor refresh cannot hide TPS unspotted body"));
 PC->SetControlRotation(FRotator::ZeroRotator);
 FMinimalViewInfo View; U->CalcCamera(0,View);
 const FVector Pivot=U->FirstPersonCamera->GetComponentLocation();
 CheckSmoke(View.Location.Equals(Pivot+FVector(-350,65,45),1),TEXT("TPS camera is behind and above shoulder"));
 CheckSmoke(!U->GetMesh()->bOwnerNoSee && U->Rifle->GetAttachParent()==U->GetMesh(),TEXT("TPS body visible and weapon mounted in hand"));
 CheckSmoke(U->GetCapsuleComponent()->GetUnscaledCapsuleRadius()==36 && U->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()==90,TEXT("TPS leaves movement and hit capsule unchanged"));
 CheckSmoke(U->GetCharacterMovement()->AvoidanceConsiderationRadius==350,TEXT("TPS leaves existing avoidance radius unchanged"));
 FVector Start,Direction; U->GetPlayerShot(Start,Direction);
 CheckSmoke(Start.Equals(Pivot,.01) && Direction.IsNormalized(),TEXT("TPS shot starts at soldier eye, not rear camera"));

 auto* Wall=GetWorld()->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
 Box->SetBoxExtent(FVector(12,200,200));
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
 Box->RegisterComponent(); Wall->SetActorLocation(Pivot+FVector(-180,0,0));
 U->CalcCamera(0,View);
 CheckSmoke(View.Location.X>Pivot.X-160 && View.Location.X<Pivot.X,TEXT("Rear wall retracts camera before clipping"));
 // Put cover at the eye while the shoulder camera can look over it.
 Wall->SetActorLocation(Pivot+FVector(60,0,-50)); Box->SetBoxExtent(FVector(10,200,60));
 U->GetPlayerShot(Start,Direction);
 FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(TPSCoverTest),false,U);
 GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Direction*1000,ECC_Visibility,Query);
 CheckSmoke(Hit.GetActor()==Wall,TEXT("Eye-origin shot is blocked by cover even when shoulder camera sees above"));
 Wall->Destroy();
 // A live neighboring capsule still blocks actual swept movement with avoidance bypassed.
 const FVector EnemyBefore=Enemy->GetActorLocation();
 Enemy->SetActorLocation(U->GetActorLocation()+FVector(150,0,0));
 const FVector MoveStart=U->GetActorLocation();
 FHitResult Collision;
 U->SetActorLocation(MoveStart+FVector(250,0,0),true,&Collision,ETeleportType::None);
 CheckSmoke(Collision.bBlockingHit && Collision.GetActor()==Enemy && U->GetActorLocation().X<Enemy->GetActorLocation().X-70,TEXT("Direct movement remains blocked by neighboring soldier capsule"));
 U->SetActorLocation(MoveStart,false,nullptr,ETeleportType::TeleportPhysics);
 Enemy->SetActorLocation(EnemyBefore,false,nullptr,ETeleportType::TeleportPhysics);
 PC->EnterTactical();
 CheckSmoke(Movement->GetRVOAvoidanceWeight()==AvoidanceBefore && Movement->bUseRVOAvoidance,TEXT("RTS return restores original AI avoidance weight"));
 // Do not accidentally enable an avoidance system disabled by a different runtime.
 Movement->SetAvoidanceEnabled(false); Movement->SetRVOAvoidanceWeight(.7f);
 PC->ControlUnit(U); PC->EnterTactical();
 CheckSmoke(!Movement->bUseRVOAvoidance && FMath::IsNearlyEqual(Movement->GetRVOAvoidanceWeight(),.7f),TEXT("Control handoff preserves pre-existing disabled avoidance and custom weight"));
 Movement->SetRVOAvoidanceWeight(AvoidanceBefore); Movement->SetAvoidanceEnabled(true);
 CheckSmoke(Enemy->IsHidden() && !Enemy->bRevealed,TEXT("RTS return immediately hides unspotted body"));
 TMap<int32,FBattleObserver> ObserversAfter; TArray<FBattleReport> ReportsAfter;
 Sense->Capture(ObserversAfter,ReportsAfter,0);
 CheckSmoke(ObserversBefore.Num()==ObserversAfter.Num() && ReportsBefore.Num()==ReportsAfter.Num() && !Sense->FindContact(U->GetUnitID(),Enemy->GetUnitID()),TEXT("View switches do not discover enemy or queue shared reports"));
 const auto& After=*U->GetState();
 CheckSmoke(Before.HP==After.HP && Before.Ammo==After.Ammo && Before.Order==After.Order && Before.Destination==After.Destination,TEXT("TPS RTS switch preserves combat state and order"));
 CheckSmoke(U->GetController()==U->SavedAI && !U->GetMesh()->bOwnerNoSee,TEXT("TPS release restores AI and visible body"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TPS_RESULT failures=%d"),SmokeFailures);
 FString Prefix; FParse::Value(FCommandLine::Get(),TEXT("TPSVisibilityScreens="),Prefix);
 if(!Prefix.IsEmpty())
 {
  // A visible silhouette beyond all observers' range. Sensors and rendering keep separate truth.
  for(ABattleUnit* Other:Units) if(Other!=U && Other!=Enemy) Other->SetActorLocation(FVector(-10000,Other->GetUnitID()*200,100));
  U->SetActorLocation(FVector(-1000,-2200,100)); Enemy->SetActorLocation(FVector(2500,-2200,100));
  PC->ControlUnit(U); PC->SetControlRotation(FRotator::ZeroRotator);
  FTimerHandle Shot; GetWorldTimerManager().SetTimer(Shot,FTimerDelegate::CreateWeakLambda(this,[this,Prefix,PC,Enemy]()
  {
   CheckSmoke(!Enemy->IsHidden() && !Enemy->bRevealed,TEXT("35m enemy rendered in TPS without sensor detection"));
   FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-tps.png"),true,false);
   FTimerHandle Back; GetWorldTimerManager().SetTimer(Back,FTimerDelegate::CreateWeakLambda(this,[this,Prefix,PC,Enemy]()
   {
    PC->EnterTactical();
    CheckSmoke(Enemy->IsHidden() && !Enemy->bRevealed,TEXT("Same enemy hidden after returning to RTS"));
    FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-rts.png"),true,false);
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TPS_VISIBILITY_RESULT failures=%d"),SmokeFailures);
    FTimerHandle Exit; GetWorldTimerManager().SetTimer(Exit,FTimerDelegate::CreateWeakLambda(this,[this](){FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);}),1.f,false);
   }),1.f,false);
  }),1.f,false);
  return;
 }
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
