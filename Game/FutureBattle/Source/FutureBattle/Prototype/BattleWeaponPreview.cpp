#include "BattleGameMode.h"
#include "BattleKitLook.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleTactics.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/HUD.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "EngineUtils.h"

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
	if (BattleKitLook::Active(GetWorld()))
	{
		// Separate display fixture: never change Weapon/Ammo or run the legacy cadence checks on a swapped mesh.
		// Four frames per weapon: mount FPS, capture, mount hand_r, capture. No shots or random draws.
		const int32 KitStage = WeaponPreviewStage++;
		if (KitStage >= 40) { Carbine->RemountWeapon(); CompleteSmokeTest(); return; }
		const EBattleWeapon KitKind = static_cast<EBattleWeapon>(KitStage / 4);
		const int32 KitView = KitStage % 4;
		if (KitView == 0)
		{
			CheckSmoke(PC->ControlUnit(Carbine), TEXT("Kit preview possession"));
			PC->SetViewTarget(Carbine);
			Carbine->MountWeapon(Carbine->Rifle, Carbine->FirstPersonCamera, KitKind, true);
			UStaticMesh* ExpectedKit = BattleKitLook::Weapon(GetWorld(), KitKind);
			CheckSmoke(ExpectedKit && Carbine->Rifle->GetStaticMesh() == ExpectedKit && Carbine->Rifle->GetRelativeScale3D().Equals(FVector(1.)),
				*FString::Printf(TEXT("Kit weapon %d loaded and mounted at full size"), int32(KitKind)));
		}
		else if (KitView == 2)
		{
			PC->EnterTactical();
			Carbine->MountWeapon(Carbine->Rifle, Carbine->GetMesh(), KitKind, false);
			CheckSmoke(Carbine->Rifle->GetStaticMesh() == BattleKitLook::Weapon(GetWorld(), KitKind) &&
				Carbine->Rifle->GetAttachSocketName() == FName(TEXT("hand_r")) &&
				Carbine->Rifle->GetRelativeTransform().Equals(BattleKitLook::Grip(KitKind, false)),
				*FString::Printf(TEXT("Kit weapon %d hand grip transform"), int32(KitKind)));
			const FVector KitTarget = Carbine->GetActorLocation() + FVector(0,0,35);
			const FVector KitEye = KitTarget + FVector(185,220,45);
			// Reuse one camera for the whole sequence; the view target keeps it alive between frames.
			ACameraActor* KitCamera = nullptr;
			for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
				if (It->ActorHasTag(TEXT("BattleKitPreviewCamera"))) { KitCamera = *It; break; }
			if (!KitCamera)
			{
				KitCamera = GetWorld()->SpawnActor<ACameraActor>(KitEye,(KitTarget-KitEye).Rotation());
				if (KitCamera) KitCamera->Tags.Add(TEXT("BattleKitPreviewCamera"));
			}
			if (KitCamera) PC->SetViewTarget(KitCamera);
			CheckSmoke(KitCamera != nullptr, TEXT("Kit preview third person camera"));
		}
		else Capture(*FString::Printf(TEXT("-kit-%02d-%s.png"),int32(KitKind),KitView == 1 ? TEXT("fps") : TEXT("hand")));
		FTimerHandle KitNext;
		GetWorldTimerManager().SetTimer(KitNext,this,&ABattleGameMode::RunWeaponPreview,.75f,false);
		return;
	}
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
