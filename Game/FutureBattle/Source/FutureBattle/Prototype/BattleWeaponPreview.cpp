#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleTactics.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/HUD.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABattleGameMode::RunWeaponPreview()
{
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	if (!PC || Units.Num()<5) { CheckSmoke(false,TEXT("Weapon fixture present")); CompleteSmokeTest(); return; }
	auto* Carbine = Units[0].Get();
	auto* Support = Units[4].Get();
	FString Prefix;
	FParse::Value(FCommandLine::Get(),TEXT("BattleWeaponPreview="),Prefix);
	auto Capture = [&](const TCHAR* Name){ if (!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+Name,true,false); };
	const double Now = GetWorld()->GetTimeSeconds();
	float Wait = .45f;
	switch (WeaponPreviewStage++)
	{
	case 0:
		CheckSmoke(PC->ControlUnit(Carbine) && Carbine->Rifle->GetStaticMesh()->GetName()==TEXT("SM_Carbine"),TEXT("Carbine mesh and FPS possession"));
		break;
	case 1: Capture(TEXT("-carbine.png")); break;
	case 2:
		CheckSmoke(PC->ControlUnit(Support) && Support->Rifle->GetStaticMesh()->GetName()==TEXT("SM_Support"),TEXT("Support mesh and FPS possession"));
		break;
	case 3: Capture(TEXT("-support.png")); break;
	case 4:
		CheckSmoke(Support->FireAt(Support->EyePosition(),FVector::UpVector,true) && !Support->FireAt(Support->EyePosition(),FVector::UpVector,true) &&
			FMath::IsNearlyEqual(Support->GetState()->NextShotTime-Now,.09,.001),TEXT("Support FPS uses live fire and its own cooldown"));
		PC->EnterTactical();
		Wait = .2f;
		break;
	case 5:
		CheckSmoke(Support->FireAt(Support->EyePosition(),FVector::UpVector,false) && FMath::IsNearlyEqual(Support->GetState()->NextShotTime-Now,.45,.001),TEXT("AI shot uses support cadence after FPS release"));
		CheckSmoke(Store->StartReload(Support->GetUnitID(),Now) && FMath::IsNearlyEqual(Support->GetState()->ReloadEndTime-Now,3.2,.001),TEXT("Support reload has its own timer"));
		Wait = 3.4f;
		break;
	case 6:
	{
		CheckSmoke(Support->GetState()->Ammo==60 && Support->GetState()->Reserve==88,TEXT("Support reload conserves two fired rounds across possession"));
		Carbine->SetActorLocationAndRotation(FVector(-2000,0,95),FRotator::ZeroRotator);
		Support->SetActorLocationAndRotation(FVector(-2000,125,95),FRotator::ZeroRotator);
		Units[2]->SetActorLocation(FVector(-2450,0,95));
		const FVector Target(-1990,62,125), Eye=Target+FVector(185,270,60);
		auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Target-Eye).Rotation());
		PC->SetViewTarget(Camera); PC->GetHUD()->bShowHUD=false;
		break;
	}
	case 7: Capture(TEXT("-pair.png")); break;
	default: CompleteSmokeTest(); return;
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next,this,&ABattleGameMode::RunWeaponPreview,Wait,false);
}
