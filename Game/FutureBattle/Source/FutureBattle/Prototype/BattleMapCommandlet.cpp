#include "BattleMapCommandlet.h"
#include "BattleGameMode.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "BattleFieldLayout.h"
#include "NavigationPath.h"

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
	FString Wide;FParse::Value(*Params,TEXT("WideField="),Wide);
	if(!Wide.IsEmpty() && Wide!=TEXT("A") && Wide!=TEXT("B") && Wide!=TEXT("C") && Wide!=TEXT("D")) return 6;
	const bool Expanded=Wide==TEXT("D");
	const bool Sight=!Wide.IsEmpty();
	const bool Large = Sight || FParse::Param(*Params,TEXT("LargeField"));
	const FString Name=Sight?TEXT("Lvl_BattleWide_")+Wide:Large?TEXT("Lvl_BattleField500"):TEXT("Lvl_BattlePrototype");
	const FString Asset=TEXT("/Game/Prototype/")+Name;
	const FString MapFile=FPaths::ProjectContentDir()/TEXT("Prototype")/(Name+TEXT(".umap"));
	if(Sight && FPaths::FileExists(MapFile)){UE_LOG(LogTemp,Error,TEXT("Sight trial map already exists; refusing replacement even with Force"));return 1;}
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
		Location=BattleFieldLayout::Expand(Location,Expanded);Size=BattleFieldLayout::Expand(Size,Expanded);
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Actor->SetActorLabel(Label);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->GetStaticMeshComponent()->SetMaterial(0, Mat);
		Actor->GetStaticMeshComponent()->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
		Actor->SetActorScale3D(Size / 100.f);
		return Actor;
	};
	if (Sight)
	{
		Box(TEXT("WideFloor"),FVector(0,0,-60),FVector(60000,40000,120),FloorMat);
		for(int32 Sign:{-1,1})
		{
			Box(TEXT("WideBoundary"),FVector(0,Sign*20000,180),FVector(60000,100,360),WallMat);
			Box(TEXT("WideBoundary"),FVector(Sign*30000,0,180),FVector(100,40000,360),WallMat);
			Box(TEXT("HomeMarker"),FVector(Sign*27000,0,2),FVector(3000,3000,3),Sign<0?AllyMat:EnemyMat,false);
			// Every obstacle is generated with its rotated partner. Corridors remain wide.
			for(const FVector P:{FVector(1500,0,0),FVector(-3500,6000,0),FVector(10000,6000,0)})
				Box(TEXT("SightScreen"),FVector(P.X*Sign,P.Y*Sign,250),FVector(1600,1000,500),WallMat);
			for(int32 Column=0;Column<3;++Column) for(int32 Row=0;Row<3;++Row)
			{
				const FVector P(Sign*(10000+Column*5000),Sign*(-11000+Row*9500),250);
				Box(TEXT("WideWarehouse"),P,FVector(1800,2500,500),WallMat);
			}
			if(Wide==TEXT("B")) for(int32 Row=0;Row<4;++Row)
				Box(TEXT("DenseScreen"),FVector(Sign*(4500+(Row%2)*7000),Sign*(-15000+Row*8500),250),FVector(2200,1200,500),WallMat);
		}
		for(int32 I=0;I<3;++I) Box(TEXT("WideObjective"),BattleFieldLayout::Objective(I)+FVector(0,0,2),FVector(1400,1400,3),AllyMat,false);
		if(Expanded)for(int32 I=0;I<3;++I)for(int32 Sign:{-1,1})
		{
			const FVector Origin=BattleFieldLayout::Objective(I)+FVector(0,3000*Sign,0);
			Box(TEXT("ObjectiveFrontCover"),Origin+FVector(1300*Sign,0,65),FVector(100,1400,130),WallMat);
			Box(TEXT("ObjectiveApproachCover"),Origin+FVector(2400*Sign,1200*Sign,150),FVector(1400,120,300),WallMat);
		}
		if(Wide==TEXT("C")) for(int32 Sign:{-1,1})
		{
			// Isolated, point-symmetric local bays retain A's objectives and routes.
			// A low frontal wall exposes the head; its open end permits a short flank.
			const FVector Origin(0,Sign*16000,0);
			Box(TEXT("FlankBayFront"),Origin+FVector(-200*Sign,0,65),FVector(100,1400,130),WallMat);
			Box(TEXT("FlankBayApproach"),Origin+FVector(-1700*Sign,800*Sign,150),FVector(1800,120,300),WallMat);
			Box(TEXT("FlankBayTarget"),Origin+FVector(0,0,2),FVector(150,150,3),EnemyMat,false);
			Box(TEXT("FlankBayStart"),Origin+FVector(-3000*Sign,0,2),FVector(200,200,3),AllyMat,false);
			Box(TEXT("FlankBaySide"),Origin+FVector(0,2000*Sign,2),FVector(200,200,3),AllyMat,false);
		}
	}
	else if (Large)
	{
		Box(TEXT("FieldFloor"),FVector(0,0,-60),FVector(24000,18000,120),FloorMat);
		for (int32 Sign : {-1,1})
		{
			Box(TEXT("FieldBoundary"),FVector(0,Sign*9000,180),FVector(24000,100,360),WallMat);
			Box(TEXT("FieldBoundary"),FVector(Sign*12000,0,180),FVector(100,18000,360),WallMat);
			Box(TEXT("Deployment"),FVector(Sign*9800,0,2),FVector(2200,14000,3),Sign<0 ? AllyMat : EnemyMat,false);
			// Staggered buildings block long sightlines without closing the three routes.
			for (int32 Row=-2; Row<=2; ++Row)
			{
				Box(TEXT("Warehouse"),FVector(Sign*4700,Row*3000,250),FVector(1000,1500,500),WallMat);
				Box(TEXT("Bunker"),FVector(Sign*2200,Row*3000+700,180),FVector(700,900,360),WallMat);
				Box(TEXT("LowCover"),FVector(Sign*6500,Row*3000+1100,60),FVector(1200,100,120),WallMat);
			}
		}
		for (int32 Lane=-1; Lane<=1; ++Lane)
		{
			Box(TEXT("SectorMarking"),FVector(0,Lane*4500,2),FVector(1400,1400,3),AllyMat,false);
			Box(TEXT("SectorCover"),FVector(-900,Lane*4500+1100,150),FVector(500,600,300),WallMat);
			Box(TEXT("SectorCover"),FVector(900,Lane*4500-1100,150),FVector(500,600,300),WallMat);
		}
	}
	else
	{
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
	}
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
	Builder->X = Expanded?79800:Sight ? 59800 : Large ? 23800 : 7800; Builder->Y = Expanded?59800:Sight ? 39800 : Large ? 17800 : 5800; Builder->Z = 1200;
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
		if (!Nav->ProjectPointToNavigation((Sight ? FVector(-27000,0,0) : Large ? FVector(-9000,0,0) : FVector(-1800,-2100,0)), TestPoint, FVector(100, 100, 200)))
		{
			UE_LOG(LogTemp, Error, TEXT("Arena navigation failed validation; map not saved. Bounds: %s"), *Bounds->GetComponentsBoundingBox().ToString());
			return 5;
		}
		if(Sight)
		{
			TArray<FVector> Points={FVector(-27000,0,0),FVector(27000,0,0),BattleFieldLayout::Objective(0),BattleFieldLayout::Objective(1),BattleFieldLayout::Objective(2)};
			for(auto& P:Points)P=BattleFieldLayout::Expand(P,Expanded);
			int32 Failures=0;
			for(int32 I=0;I<Points.Num();++I) for(int32 J=I+1;J<Points.Num();++J)
			{
				auto* Path=Nav->FindPathToLocationSynchronously(World,Points[I],Points[J]);
				const bool Reach=Path && Path->IsValid() && !Path->IsPartial();
				const bool Blocked=World->LineTraceTestByChannel(Points[I]+FVector(0,0,170),Points[J]+FVector(0,0,170),ECC_Visibility);
				const bool RequireBlocked=(I==0 && J==1) || (I>=2) || (I==0 && (J==3 || J==4)) || (I==1 && (J==2 || J==3));
				Failures+=!Reach || (RequireBlocked && !Blocked);
				UE_LOG(LogTemp,Display,TEXT("WIDE_MAP_PAIR from=%d to=%d reachable=%d length_cm=%.1f blocked=%d required=%d"),I,J,Reach,Reach?Path->GetPathLength():-1,Blocked,RequireBlocked);
			}
			UE_LOG(LogTemp,Display,TEXT("WIDE_MAP_RESULT failures=%d layout=fixed-v1 rotation=180"),Failures);
			if(Failures) return 7;
		}
	}
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, Asset)) return 4;
	UE_LOG(LogTemp, Display, TEXT("BATTLE_MAP_CREATED %s"), *MapFile);
	return 0;
#else
	return 1;
#endif
}
