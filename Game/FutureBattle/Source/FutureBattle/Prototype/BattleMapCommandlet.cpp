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
#include "NavMesh/RecastNavMesh.h"
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
	if(!Wide.IsEmpty() && Wide!=TEXT("A") && Wide!=TEXT("B") && Wide!=TEXT("C") && Wide!=TEXT("D") && Wide!=TEXT("E") && Wide!=TEXT("F")) return 6;
	const bool Expanded=Wide==TEXT("D");
	// Map E (2026-11-14): 1000m x 700m, urban centre with five objectives, open flanks with hedges and low walls.
	// Map F (2026-11-18): the size, homes and city objectives of E; a city of districts with an avenue tanks can use, two flank
	// objectives, two depots and a road network (centre, north and south routes with connectors). See ROADMAP 2026-11-18.
	const bool UrbanF=Wide==TEXT("F");
	const bool Urban=Wide==TEXT("E")||UrbanF;
	const FString WideName=TEXT("Lvl_BattleWide_")+Wide;
	const bool Sight=!Wide.IsEmpty();
	const bool Large = Sight || FParse::Param(*Params,TEXT("LargeField"));
	const FString Name=Sight?TEXT("Lvl_BattleWide_")+Wide:Large?TEXT("Lvl_BattleField500"):TEXT("Lvl_BattlePrototype");
	const FString Asset=TEXT("/Game/Prototype/")+Name;
	const FString MapFile=FPaths::ProjectContentDir()/TEXT("Prototype")/(Name+TEXT(".umap"));
	// Maps E and F may be regenerated with -Force (E is a measurement baseline: do not regenerate it without need). Maps A-D are never replaced.
	if(Sight && !(Urban && FParse::Param(*Params,TEXT("Force"))) && FPaths::FileExists(MapFile)){UE_LOG(LogTemp,Error,TEXT("Sight trial map already exists; refusing replacement even with Force"));return 1;}
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
	if (Urban)
	{
		// Every obstacle is spawned with its 180-degree partner, so the layout is point symmetric.
		auto Pair=[&](const TCHAR* Label,FVector Center,FVector Size,bool bConcealment=false)
		{
			for(int32 Sign:{-1,1})
			{
				AStaticMeshActor* Actor=Box(Label,FVector(Center.X*Sign,Center.Y*Sign,Center.Z),Size,WallMat);
				// Hedges hide soldiers but are meant not to stop bullets (shots will ignore this tag).
				if(bConcealment)Actor->Tags.Add(TEXT("BattleConcealment"));
			}
		};
		Box(TEXT("UrbanFloor"),FVector(0,0,-60),FVector(100000,70000,120),FloorMat);
		for(int32 Sign:{-1,1})
		{
			Box(TEXT("UrbanBoundary"),FVector(0,Sign*35000,180),FVector(100000,100,360),WallMat);
			Box(TEXT("UrbanBoundary"),FVector(Sign*50000,0,180),FVector(100,70000,360),WallMat);
			Box(TEXT("HomeMarker"),FVector(Sign*42000,0,2),FVector(3000,3000,3),Sign<0?AllyMat:EnemyMat,false);
			Box(TEXT("LookoutMarker"),FVector(0,Sign*16500,2),FVector(800,800,3),AllyMat,false);
			// Map F: the depots are objectives and get the objective marker below.
			if(!UrbanF)Box(TEXT("DepotMarker"),FVector(Sign*33000,Sign*15000,2),FVector(1200,1200,3),AllyMat,false);
		}
		for(int32 I=0;I<BattleFieldLayout::ObjectiveCountForMap(WideName);++I) Box(TEXT("UrbanObjective"),BattleFieldLayout::ObjectiveForMap(WideName,I)+FVector(0,0,2),FVector(1400,1400,3),AllyMat,false);
		// Layout v2 (2026-11-15). The first layout used 52m x 29m solid blocks: stepping 2-4m sideways from a corner still left a
		// soldier behind the block, so only about 5% of cover requests succeeded. v2 is built from pieces the cover action and
		// the stances can use: houses and wings a few metres wide, ruins with doorways and window sills, cars, barriers,
		// sandbags, kiosks, low yard walls, and scattered walls, trees, boulders, sheds and farms on the flanks.
		int32 Objects=0;
		// Emits an object with its 180-degree partner. Only positions in the +X half (or on the +Y axis) are accepted, so a loop
		// over the whole plane yields a point-symmetric layout.
		auto Emit=[&](const TCHAR* Label,double X,double Y,double SizeX,double SizeY,double Height,bool bConcealment=false)
		{
			if(!(X>1. || (FMath::Abs(X)<=1. && Y>1.)))return;
			// Two cover heights matter to the stances: low cover is 130cm (hides a crouched soldier, who is 110cm tall, while a
			// standing one sees and fires over it); cars, wrecks and boulders are 140cm (a standing soldier still fires over them).
			if(Height>=100. && Height<=125.)Height=130.; else if(Height>=150. && Height<=165.)Height=140.;
			Pair(Label,FVector(X,Y,Height*.5),FVector(SizeX,SizeY,Height),bConcealment);++Objects;
		};
		if(UrbanF)
		{
#include "BattleMapLayoutF.inl"
		}
		else
		{
		// City: 500m x 300m. Streets (16m) run along x = -130/0/130m and y = -75/0/75m; each block holds 2 x 2 lots split by 8m alleys.
		const double BlockX[4][2]={{-25000,-13800},{-12200,-800},{800,12200},{13800,25000}};
		const double BlockY[4][2]={{-15000,-8300},{-6700,-800},{800,6700},{8300,15000}};
		for(int32 BX=0;BX<4;++BX) for(int32 BY=0;BY<4;++BY) for(int32 LX=0;LX<2;++LX) for(int32 LY=0;LY<2;++LY)
		{
			const double MX=(BlockX[BX][0]+BlockX[BX][1])*.5,MY=(BlockY[BY][0]+BlockY[BY][1])*.5;
			const double X0=LX?MX+400:BlockX[BX][0],X1=LX?BlockX[BX][1]:MX-400,Y0=LY?MY+400:BlockY[BY][0],Y1=LY?BlockY[BY][1]:MY-400;
			const double CX=(X0+X1)*.5,CY=(Y0+Y1)*.5,W=X1-X0,H=Y1-Y0;
			switch((BX+2*BY+LX+3*LY)%4)
			{
			case 0: // house with a walled yard
				Emit(TEXT("House"),CX-W*.2,CY,W*.42,H-600,600);
				for(int32 I=0;I<3;++I)
				{
					Emit(TEXT("YardWall"),CX+W*.12+I*550,CY-H*.5+250,300,60,120);
					Emit(TEXT("YardWall"),CX+W*.12+I*550,CY+H*.5-250,300,60,120);
				}
				Emit(TEXT("YardWall"),CX+W*.5-250,CY,60,500,120);
				Emit(TEXT("Crate"),CX+W*.25,CY+250,130,130,130);Emit(TEXT("Crate"),CX+W*.3,CY-300,130,130,130);
				break;
			case 1: // ruin: outer walls with doorways and window sills, open inside
				{
					const double RW=W-900,RH=H-700,Thick=60;
					for(int32 Side:{-1,1}) for(int32 I=0;I<5;++I)
					{
						if(I==2)continue; // doorway
						Emit(I%2?TEXT("RuinSill"):TEXT("RuinWall"),CX-RW*.5+RW*(I+.5)/5.,CY+Side*RH*.5,RW/5.,Thick,I%2?110:320);
					}
					for(int32 Side:{-1,1}) for(int32 I=0;I<3;++I)
					{
						if(I==1)continue; // doorway
						Emit(TEXT("RuinWall"),CX+Side*RW*.5,CY-RH*.5+RH*(I+.5)/3.,Thick,RH/3.,320);
					}
					Emit(TEXT("RuinPartition"),CX-RW*.17,CY+RH*.12,Thick,RH*.4,320);Emit(TEXT("RuinPartition"),CX+RW*.2,CY-RH*.12,Thick,RH*.4,320);
					Emit(TEXT("Crate"),CX,CY,130,130,130);
				}
				break;
			case 2: // U-shaped building around a courtyard
				Emit(TEXT("Wing"),CX,CY+H*.5-650,W-800,700,700);
				Emit(TEXT("Wing"),CX-W*.5+750,CY-150,700,H-1400,700);Emit(TEXT("Wing"),CX+W*.5-750,CY-150,700,H-1400,700);
				Emit(TEXT("CourtWall"),CX-500,CY-H*.5+500,400,60,120);Emit(TEXT("CourtWall"),CX+500,CY-H*.5+500,400,60,120);
				Emit(TEXT("Car"),CX,CY-200,450,200,160);
				break;
			default: // car park
				for(int32 I=0;I<5;++I)Emit(TEXT("Car"),CX-W*.5+700+I*((W-1400)/4.),CY+(I%2?450:-450),200,450,160);
				Emit(TEXT("Barrier"),CX-900,CY,300,60,120);Emit(TEXT("Barrier"),CX+900,CY,300,60,120);
				Emit(TEXT("Kiosk"),CX,CY+H*.5-450,250,250,260);Emit(TEXT("Kiosk"),CX,CY-H*.5+450,250,250,260);
				break;
			}
		}
		// Streets: a wreck in the middle of every segment (alternating sides, so no street is a clear line from crossing to
		// crossing) and clutter along it. bAlongX = the street runs along X.
		auto Clutter=[&](double X,double Y,bool bAlongX,int32 Kind)
		{
			switch(((Kind%4)+4)%4)
			{
			case 0: Emit(TEXT("Car"),X,Y,bAlongX?450:200,bAlongX?200:450,160);break;
			case 1: Emit(TEXT("Barrier"),X,Y,bAlongX?60:300,bAlongX?300:60,120);break;
			case 2: Emit(TEXT("Sandbags"),X,Y,bAlongX?90:400,bAlongX?400:90,120);break;
			default: Emit(TEXT("Kiosk"),X,Y,250,250,260);break;
			}
		};
		const double StreetX[5]={-25000,-13000,0,13000,25000},StreetY[5]={-15000,-7500,0,7500,15000};
		const double Along[4]={.18,.31,.69,.82};
		for(int32 Row=1;Row<4;++Row) for(int32 Seg=0;Seg<4;++Seg)
		{
			const double Start=StreetX[Seg],Length=StreetX[Seg+1]-StreetX[Seg],Y=StreetY[Row];
			Emit(TEXT("StreetWreck"),Start+Length*.5,Y+((Seg+Row)%2?300:-300),300,1000,300);
			for(int32 I=0;I<4;++I)Clutter(Start+Length*Along[I],Y+((I+Seg+Row)%2?500:-500),true,I+Seg+Row);
		}
		for(int32 Column=1;Column<4;++Column) for(int32 Seg=0;Seg<4;++Seg)
		{
			const double Start=StreetY[Seg],Length=StreetY[Seg+1]-StreetY[Seg],X=StreetX[Column];
			Emit(TEXT("StreetWreck"),X+((Seg+Column)%2?300:-300),Start+Length*.5,1000,300,300);
			for(int32 I=0;I<4;++I)Clutter(X+((I+Seg+Column)%2?500:-500),Start+Length*Along[I],false,I+Seg+Column+1);
		}
		// Objective crossings: low walls on four sides and crates at the corners; the centre stays free for the capture area.
		for(int32 I=0;I<5;++I)
		{
			const FVector O=BattleFieldLayout::ObjectiveForMap(TEXT("Lvl_BattleWide_E"),I);
			for(int32 Side:{-1,1})
			{
				// Inside the 4.5m capture radius, so the soldiers who hold the objective have cover too.
				Emit(TEXT("PlazaWall"),O.X,O.Y+Side*380,300,60,120);Emit(TEXT("PlazaWall"),O.X+Side*380,O.Y,60,300,120);
				for(int32 Other:{-1,1})Emit(TEXT("PlazaCrate"),O.X+Side*650,O.Y+Other*650,140,140,130);
			}
		}
		// Flanks and approaches: scattered cover on a jittered grid (fixed seed). Hedges only conceal; everything else stops bullets.
		FRandomStream Random(20261115);
		auto Scatter=[&](double MinX,double MaxX,double MinY,double MaxY,double Cell,double Chance)
		{
			for(double GX=MinX;GX<MaxX;GX+=Cell) for(double GY=MinY;GY<MaxY;GY+=Cell)
			{
				const double X=GX+Cell*(.25+.5*Random.FRand()),Y=GY+Cell*(.25+.5*Random.FRand());
				const double Roll=Random.FRand(),Kind=Random.FRand();const bool bAlongX=Random.FRand()<.5;
				if(Roll>Chance)continue;
				if(Kind<.25)Emit(TEXT("StoneWall"),X,Y,bAlongX?600:70,bAlongX?70:600,130);
				else if(Kind<.40)Emit(TEXT("Hedge"),X,Y,bAlongX?900:100,bAlongX?100:900,220,true);
				else if(Kind<.55){Emit(TEXT("Tree"),X,Y,70,70,700);Emit(TEXT("Tree"),X+(bAlongX?450:0),Y+(bAlongX?0:450),70,70,700);}
				else if(Kind<.70)Emit(TEXT("Boulder"),X,Y,220,220,160);
				else if(Kind<.80)Emit(TEXT("Shed"),X,Y,500,400,280);
				else if(Kind<.90)Emit(TEXT("Wreck"),X,Y,bAlongX?450:200,bAlongX?200:450,160);
				else{Emit(TEXT("FarmHouse"),X,Y,1100,800,450);Emit(TEXT("FarmWall"),X+900,Y-300,70,700,130);Emit(TEXT("FarmWall"),X+300,Y+700,700,70,130);}
			}
		};
		for(int32 Side:{-1,1})
		{
			const double Near=Side>0?16500:-31500,Far=Side>0?31500:-16500;
			Scatter(-46000,46000,Near,Far,2500,.6);
			// The outermost 30m stays sparse: a long firing lane for heavy weapons later.
			Scatter(-46000,46000,Side>0?31500:-34000,Side>0?34000:-31500,2500,.15);
		}
		// Between each home and the city edge.
		Scatter(26000,37500,-16000,16000,2500,.45);
		// Home yards, lookouts and depots: low sandbag walls.
		Emit(TEXT("HomeYardWall"),36000,3000,100,2400,130);Emit(TEXT("HomeYardWall"),36000,-3000,100,2400,130);
		Emit(TEXT("DepotCrate"),33000,15600,600,400,200);Emit(TEXT("DepotWall"),32200,14400,60,500,120);
		Emit(TEXT("LookoutSandbags"),300,16900,400,90,120);Emit(TEXT("LookoutSandbags"),-300,-16900,400,90,120);
		UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_LAYOUT objects=%d (x2 by rotation) layout=urban-v2"),Objects);
		}
	}
	else if (Sight)
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
	// Map E: keep the navigation volume below the tops of cars, walls and roofs (-20cm..125cm). With the tall volume, walkable
	// islands formed on rooftops and car roofs; destinations projected onto them and the paths came back partial.
	ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, Urban ? 52.5 : 150), FRotator::ZeroRotator);
	Bounds->SetActorLabel(TEXT("ArenaNavigationBounds"));
	UCubeBuilder* Builder = NewObject<UCubeBuilder>(Bounds);
	Builder->X = Urban?99800:Expanded?79800:Sight ? 59800 : Large ? 23800 : 7800; Builder->Y = Urban?69800:Expanded?59800:Sight ? 39800 : Large ? 17800 : 5800; Builder->Z = Urban ? 145 : 1200;
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
		if (!Nav->ProjectPointToNavigation((Urban ? FVector(-42000,0,0) : Sight ? FVector(-27000,0,0) : Large ? FVector(-9000,0,0) : FVector(-1800,-2100,0)), TestPoint, FVector(100, 100, 200)))
		{
			UE_LOG(LogTemp, Error, TEXT("Arena navigation failed validation; map not saved. Bounds: %s"), *Bounds->GetComponentsBoundingBox().ToString());
			return 5;
		}
		if(Urban)
		{
			// The v2 city has over a thousand obstacles: an 840m home-to-home path exceeds the default A* node budget and
			// comes back partial. Raise the budget on this map's navigation data (saved with the map).
			if(auto* Recast=Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance()))
			{
				Recast->DefaultMaxSearchNodes=32768.f;
				const_cast<FNavigationQueryFilter&>(*Recast->GetDefaultQueryFilter()).SetMaxSearchNodes(32768);
				UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_NAV max_search_nodes=%.0f"),Recast->DefaultMaxSearchNodes);
			}
			// Homes and the five objectives must all connect. Direct lines of sight between them are only reported: the v2 city is
			// made of small pieces and low walls, so a clear line at standing height can exist without being a design error.
			TArray<FVector> Points={FVector(-42000,0,0),FVector(42000,0,0)};
			for(int32 I=0;I<BattleFieldLayout::ObjectiveCountForMap(WideName);++I)Points.Add(BattleFieldLayout::ObjectiveForMap(WideName,I));
			int32 Failures=0,OpenLines=0;
			for(int32 I=0;I<Points.Num();++I) for(int32 J=I+1;J<Points.Num();++J)
			{
				auto* Path=Nav->FindPathToLocationSynchronously(World,Points[I],Points[J]);
				const bool Reach=Path && Path->IsValid() && !Path->IsPartial();
				const bool Blocked=World->LineTraceTestByChannel(Points[I]+FVector(0,0,170),Points[J]+FVector(0,0,170),ECC_Visibility);
				Failures+=!Reach;OpenLines+=!Blocked;
				UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_PAIR from=%d to=%d reachable=%d length_cm=%.1f blocked=%d"),I,J,Reach,Reach?Path->GetPathLength():-1,Blocked);
			}
			UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_RESULT failures=%d open_lines=%d layout=%s rotation=180"),Failures,OpenLines,UrbanF?TEXT("districts-v1"):TEXT("urban-v2"));
			if(Failures) return 7;
		}
		else if(Sight)
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
