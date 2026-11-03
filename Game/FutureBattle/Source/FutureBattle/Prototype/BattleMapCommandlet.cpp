#include "BattleMapCommandlet.h"
#include "BattleGameMode.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/BrushComponent.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#endif

UBattleMapCommandlet::UBattleMapCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UBattleMapCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const FString MapFile = FPaths::ProjectContentDir() / TEXT("Prototype/Lvl_BattlePrototype.umap");
	if (FPaths::FileExists(MapFile) && !FParse::Param(*Params, TEXT("Force"))) { UE_LOG(LogTemp, Error, TEXT("Map already exists; use -Force only to regenerate the test arena.")); return 1; }
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!World) return 2;
	World->GetWorldSettings()->DefaultGameMode = ABattleGameMode::StaticClass();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return 3;
	auto Material = [&](const FString& Name, FLinearColor Color)
	{
		const FString PackageName = TEXT("/Game/Prototype/Materials/") + Name;
		if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *(PackageName + TEXT(".") + Name))) return Existing;
		UPackage* Package = CreatePackage(*PackageName);
		UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
		UMaterialExpressionConstant3Vector* RGB = NewObject<UMaterialExpressionConstant3Vector>(Mat);
		RGB->Constant = Color;
		Mat->GetExpressionCollection().AddExpression(RGB);
		Mat->GetEditorOnlyData()->BaseColor.Expression = RGB;
		Mat->PostEditChange();
		FAssetRegistryModule::AssetCreated(Mat);
		Package->MarkPackageDirty();
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPaths::ProjectContentDir() / TEXT("Prototype/Materials/") + Name + TEXT(".uasset");
		UPackage::SavePackage(Package, Mat, *Filename, Args);
		return Mat;
	};
	UMaterial* FloorMat = Material(TEXT("M_ArenaFloor"), FLinearColor(.14f, .19f, .24f));
	UMaterial* WallMat = Material(TEXT("M_ArenaCover"), FLinearColor(.32f, .4f, .46f));
	UMaterial* AllyMat = Material(TEXT("M_DeploymentAlly"), FLinearColor(.04f, .5f, .4f));
	UMaterial* EnemyMat = Material(TEXT("M_DeploymentEnemy"), FLinearColor(.65f, .12f, .07f));
	auto Box = [&](const TCHAR* Label, FVector Location, FVector Size, UMaterial* Mat, bool bCollision = true)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Actor->SetActorLabel(Label);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->GetStaticMeshComponent()->SetMaterial(0, Mat);
		Actor->GetStaticMeshComponent()->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
		Actor->SetActorScale3D(Size / 100.f);
		return Actor;
	};
	Box(TEXT("ArenaFloor"), FVector(0, 0, -60), FVector(8000, 6000, 120), FloorMat);
	Box(TEXT("NorthWall"), FVector(0, 3000, 180), FVector(8000, 100, 360), WallMat);
	Box(TEXT("SouthWall"), FVector(0, -3000, 180), FVector(8000, 100, 360), WallMat);
	Box(TEXT("EastWall"), FVector(4000, 0, 180), FVector(100, 6000, 360), WallMat);
	Box(TEXT("WestWall"), FVector(-4000, 0, 180), FVector(100, 6000, 360), WallMat);
	Box(TEXT("AllyDeployment"), FVector(-2200, 0, 2), FVector(750, 1750, 3), AllyMat, false);
	Box(TEXT("EnemyDeployment"), FVector(2200, 0, 2), FVector(750, 1750, 3), EnemyMat, false);
	// Full-height cover forces flanking; open north/south lanes support navigation tests.
	Box(TEXT("CentralCover"), FVector(0, 0, 150), FVector(220, 650, 300), WallMat);
	Box(TEXT("WestCover"), FVector(-850, 900, 140), FVector(180, 600, 280), WallMat);
	Box(TEXT("EastCover"), FVector(850, -900, 140), FVector(180, 600, 280), WallMat);
	Box(TEXT("WestLowCover"), FVector(-900, -850, 60), FVector(500, 100, 120), WallMat);
	Box(TEXT("EastLowCover"), FVector(900, 850, 60), FVector(500, 100, 120), WallMat);
	for (int32 Y = -2400; Y <= 2400; Y += 600) Box(TEXT("LaneMarking"), FVector(0, Y, 1), FVector(7400, 8, 1), WallMat, false);
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2200), FRotator(-50, -35, 0));
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->GetLightComponent()->SetIntensity(4.f);
	ASkyLight* Sky = World->SpawnActor<ASkyLight>();
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->SetIntensity(1.f);
	Sky->GetLightComponent()->SetLightColor(FLinearColor(.65f, .78f, 1));
	ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 150), FRotator::ZeroRotator);
	Bounds->SetActorLabel(TEXT("ArenaNavigationBounds"));
	UCubeBuilder* Builder = NewObject<UCubeBuilder>(Bounds);
	Builder->X = 7800; Builder->Y = 5800; Builder->Z = 1200;
	UActorFactory::CreateBrushForVolumeActor(Bounds, Builder);
	Bounds->GetBrushComponent()->BuildSimpleBrushCollision();
	Bounds->PostEditChange();
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		// Commandlets do not tick the editor loading loop that normally releases this lock.
		Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		Nav->OnNavigationBoundsUpdated(Bounds);
		Nav->Tick(.1f);
		Nav->Build();
		FNavLocation TestPoint;
		if (!Nav->ProjectPointToNavigation(FVector(-1800, -2100, 0), TestPoint, FVector(100, 100, 200)))
		{
			UE_LOG(LogTemp, Error, TEXT("Arena navigation failed validation; map not saved. Bounds: %s"), *Bounds->GetComponentsBoundingBox().ToString());
			return 5;
		}
	}
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, TEXT("/Game/Prototype/Lvl_BattlePrototype"))) return 4;
	UE_LOG(LogTemp, Display, TEXT("BATTLE_MAP_CREATED %s"), *MapFile);
	return 0;
#else
	return 1;
#endif
}
