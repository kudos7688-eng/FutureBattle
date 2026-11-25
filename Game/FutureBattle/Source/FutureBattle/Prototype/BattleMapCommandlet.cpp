#include "BattleMapCommandlet.h"
#include "BattleGameMode.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "BattleFieldLayout.h"
#include "BattleMapI.h"
#include "BattleMapL.h"
#include "BattleMapJ.h"
#include "BattleMapK.h"
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
#include "NavModifierVolume.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "NavModifierComponent.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#endif

#if WITH_EDITOR
namespace
{
// Map L (2026-11-25): the ground mesh, in tiles of 40 x 40 grid cells (100m), from the height field: the same points and the same
// two triangles per cell as BattleTerrain::FHeightField::Height, so the collision is the table. Paint gives the colour of a grid
// point. Each tile is a static mesh asset under Root with complex collision, tagged as ground (BattleTerrain::Tag). Returns the
// number of tiles, or -1 when a package could not be saved.
int32 BuildGroundTiles(UWorld* World,const BattleTerrain::FHeightField& F,TFunctionRef<FLinearColor(int32,int32)> Paint,UMaterialInterface* Mat,const FString& Root)
{
	constexpr int32 Cells=40;int32 Tiles=0;
	for(int32 TJ=0;TJ*Cells<F.NY-1;++TJ)for(int32 TI=0;TI*Cells<F.NX-1;++TI)
	{
		const int32 I0=TI*Cells,J0=TJ*Cells,I1=FMath::Min(I0+Cells,F.NX-1),J1=FMath::Min(J0+Cells,F.NY-1);
		const FString Name=FString::Printf(TEXT("SM_Ground_%02d_%02d"),TI,TJ);
		UPackage* Package=CreatePackage(*(Root/Name));
		UStaticMesh* Mesh=NewObject<UStaticMesh>(Package,*Name,RF_Public|RF_Standalone);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Mat,TEXT("Ground"),TEXT("Ground")));
		FMeshDescription Desc;FStaticMeshAttributes Attr(Desc);Attr.Register();
		auto Pos=Attr.GetVertexPositions();auto Normals=Attr.GetVertexInstanceNormals();auto Colors=Attr.GetVertexInstanceColors();auto UVs=Attr.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		const FPolygonGroupID Group=Desc.CreatePolygonGroup();Attr.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Ground");
		const int32 W=I1-I0+1;TArray<FVertexInstanceID> Inst;Inst.SetNum(W*(J1-J0+1));
		for(int32 J=J0;J<=J1;++J)for(int32 I=I0;I<=I1;++I)
		{
			const double X=F.Origin.X+I*F.Step,Y=F.Origin.Y+J*F.Step,H=F.At(I,J);
			const FVertexID V=Desc.CreateVertex();Pos[V]=FVector3f(float(X),float(Y),float(H));
			const FVertexInstanceID VI=Desc.CreateVertexInstance(V);
			const int32 XA=FMath::Max(I-1,0),XB=FMath::Min(I+1,F.NX-1),YA=FMath::Max(J-1,0),YB=FMath::Min(J+1,F.NY-1);
			const double DX=(F.At(XB,J)-F.At(XA,J))/((XB-XA)*F.Step),DY=(F.At(I,YB)-F.At(I,YA))/((YB-YA)*F.Step);
			Normals[VI]=FVector3f(FVector(-DX,-DY,1.).GetSafeNormal());
			const FLinearColor C=Paint(I,J);Colors[VI]=FVector4f(C.R,C.G,C.B,1.f);
			UVs.Set(VI,0,FVector2f(float(X/1000.),float(Y/1000.)));
			Inst[(J-J0)*W+(I-I0)]=VI;
		}
		auto At=[&](int32 A,int32 B){return Inst[(B-J0)*W+(A-I0)];};
		for(int32 J=J0;J<J1;++J)for(int32 I=I0;I<I1;++I)
		{
			// Faces up with the winding of the engine's generated boxes (UKismetProceduralMeshLibrary::GenerateBoxMesh): the lower
			// triangle (0,0)-(1,1)-(1,0) and the upper one (0,0)-(0,1)-(1,1), split along the diagonal of FHeightField::Height.
			Desc.CreateTriangle(Group,TArray<FVertexInstanceID>({At(I,J),At(I+1,J+1),At(I+1,J)}));
			Desc.CreateTriangle(Group,TArray<FVertexInstanceID>({At(I,J),At(I,J+1),At(I+1,J+1)}));
		}
		FStaticMeshSourceModel& Source=Mesh->AddSourceModel();
		Source.BuildSettings.bRecomputeNormals=false;Source.BuildSettings.bRecomputeTangents=true;Source.BuildSettings.bGenerateLightmapUVs=false;
		Source.BuildSettings.bRemoveDegenerates=false;
		Mesh->CreateMeshDescription(0,MoveTemp(Desc));Mesh->CommitMeshDescription(0);
		Mesh->CreateBodySetup();
		if(UBodySetup* Body=Mesh->GetBodySetup()){Body->CollisionTraceFlag=CTF_UseComplexAsSimple;Body->bDoubleSidedGeometry=true;}
		Mesh->Build(false);Mesh->PostEditChange();
		FAssetRegistryModule::AssetCreated(Mesh);Package->MarkPackageDirty();
		FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
		const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
		if(!UPackage::SavePackage(Package,Mesh,*File,Args)){UE_LOG(LogTemp,Error,TEXT("TERRAIN_FIELD_SAVE_FAILED %s"),*File);return -1;}
		AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector,FRotator::ZeroRotator);
		Actor->SetActorLabel(TEXT("Ground"));
		Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->Tags.Add(BattleTerrain::Tag());
		++Tiles;
	}
	return Tiles;
}
}
#endif

UBattleMapCommandlet::UBattleMapCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UBattleMapCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString Wide;FParse::Value(*Params,TEXT("WideField="),Wide);
	if(!Wide.IsEmpty() && Wide!=TEXT("A") && Wide!=TEXT("B") && Wide!=TEXT("C") && Wide!=TEXT("D") && Wide!=TEXT("E") && Wide!=TEXT("F") && Wide!=TEXT("G") && Wide!=TEXT("H") && Wide!=TEXT("I") && Wide!=TEXT("J") && Wide!=TEXT("K") && Wide!=TEXT("L")) return 6;
	const bool Expanded=Wide==TEXT("D");
	// Map E (2026-11-14): 1000m x 700m, urban centre with five objectives, open flanks with hedges and low walls.
	// Map F (2026-11-18): the size, homes and city objectives of E; a city of districts with an avenue tanks can use, two flank
	// objectives, two depots and a road network (centre, north and south routes with connectors). See ROADMAP 2026-11-18.
	// Map G (2026-11-18): map F with height differences (hills, two-storey houses that can be entered) and more cover beside the
	// roads. It shares the layout of F (BattleMapLayoutF.inl) and adds BattleMapLayoutG.inl; the heights are in BattleTerrain.h.
	const bool UrbanG=Wide==TEXT("G");
	const bool UrbanF=Wide==TEXT("F")||UrbanG;
	// Map I (2026-11-19): a map designed from 50m sectors (BattleMapI.h, BattleMapLayoutI.inl): town, fields, woods, a river with a
	// bridge and a ford on each side, hills of 5m and 7m. It takes the rules of map E and the height table of map G.
	const bool UrbanI=Wide==TEXT("I");
	// Map L (2026-11-25, BattleMapL.h, Docs/design-terrain-field.md): the layout of map I on rolling ground (a height field and a
	// ground mesh instead of the flat floor). Everything that stands on the ground is placed at the height of the ground there.
	const bool UrbanL=Wide==TEXT("L");
	const bool UrbanH=Wide==TEXT("H");
	// Map J (2026-11-22, BattleMapJ.h, BattleMapLayoutJ.inl): the grid of map I with impassable mountains and lakes and varied buildings.
	const bool UrbanJ=Wide==TEXT("J");
	// Map K (2026-11-22, BattleMapK.h, Docs/design-map-k.md): a dense small town ringed by mountains, built from the sector kinds and
	// layouts of map J (BattleMapLayoutJ.inl is included a second time with BattleMapK in place of BattleMapJ).
	const bool UrbanK=Wide==TEXT("K");
	auto KindJK=[UrbanK](int32 C,int32 R){return UrbanK?BattleMapK::Kind(C,R):BattleMapJ::Kind(C,R);};
	const bool Urban=UrbanH||Wide==TEXT("E")||UrbanF||UrbanI||UrbanJ||UrbanK||UrbanL;
	// Maps with a height table: their houses and hills (east half), for the navigation volumes and the checks below. Map L has no
	// hills as patches (they are part of its height field).
	const bool Terrain=UrbanG||UrbanH||UrbanI||UrbanJ||UrbanK||UrbanL;
	const TArray<BattleTerrain::FHouse> TerrainHouses=UrbanK?BattleMapK::Houses():UrbanJ?BattleMapJ::Houses():(UrbanI||UrbanL)?BattleMapI::Houses():UrbanH?BattleSectors::Houses():UrbanG?BattleTerrain::HousesG():TArray<BattleTerrain::FHouse>();
	const TArray<BattleTerrain::FHill> TerrainHills=UrbanK?BattleMapK::Hills():UrbanJ?BattleMapJ::Hills():UrbanI?BattleMapI::Hills():UrbanH?BattleSectors::Hills():UrbanG?BattleTerrain::HillsG():TArray<BattleTerrain::FHill>();
	const FString WideName=TEXT("Lvl_BattleWide_")+Wide;
	// Map L: the height field of the map definition (null on every other map: their ground is the flat floor at height 0).
	const BattleTerrain::FHeightField* Field=UrbanL?&BattleFieldLayout::ByName(WideName).Heights.Field:nullptr;
	auto GroundZ=[Field](double X,double Y){return Field?Field->Height(X,Y):0.;};
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
	// Map L: how Box places a piece on the rolling ground. FieldStand (default): a piece whose bottom is at the ground (below 5cm) keeps
	// its height above the ground at its middle and reaches 20cm below the lowest ground under it (a foundation, no gap on a slope);
	// a piece longer than 4m on ground that changes by more than 20cm under it is cut into pieces of at most 3m first, so a wall on a
	// hillside follows the hill (FieldSplit=false keeps it whole: the map boundary). A piece above the ground (a tree crown, an upper
	// floor) and every piece with FieldStand=false (a ramp, which Ramp makes thick enough) is only raised by the ground at its middle.
	// Spawned: the actors of the last call (more than one when a piece was cut). Other maps: one actor at the given place.
	bool FieldStand=true,FieldSplit=true;TArray<AStaticMeshActor*> Spawned;
	auto Box = [&](const TCHAR* Label, FVector Location, FVector Size, UMaterial* Mat, bool bCollision = true)
	{
		Location=BattleFieldLayout::Expand(Location,Expanded);Size=BattleFieldLayout::Expand(Size,Expanded);
		Spawned.Reset();
		TArray<TPair<FVector,FVector>> Pieces;Pieces.Emplace(Location,Size);
		if(Field)
		{
			Pieces.Reset();
			const bool bStands=FieldStand&&Location.Z-Size.Z*.5<5.;
			double Low=0,High=0;Field->Range(Location.X-Size.X*.5,Location.Y-Size.Y*.5,Location.X+Size.X*.5,Location.Y+Size.Y*.5,Low,High);
			const bool bAlongX=Size.X>=Size.Y;const double Long=bAlongX?Size.X:Size.Y;
			const int32 N=bStands&&FieldSplit&&Long>400.&&High-Low>20.?FMath::CeilToInt(Long/300.):1;
			for(int32 K=0;K<N;++K)
			{
				FVector At=Location,Extent=Size;
				if(N>1){const double Part=Long/N,From=(bAlongX?Location.X:Location.Y)-Long*.5+Part*(K+.5);(bAlongX?At.X:At.Y)=From;(bAlongX?Extent.X:Extent.Y)=Part;}
				const double G=Field->Height(At.X,At.Y);
				if(bStands)
				{
					double PieceLow=0,PieceHigh=0;Field->Range(At.X-Extent.X*.5,At.Y-Extent.Y*.5,At.X+Extent.X*.5,At.Y+Extent.Y*.5,PieceLow,PieceHigh);
					const double Top=G+At.Z+Extent.Z*.5,Bottom=FMath::Min(G+At.Z-Extent.Z*.5,PieceLow-20.);
					At.Z=(Top+Bottom)*.5;Extent.Z=Top-Bottom;
				}
				else At.Z+=G;
				Pieces.Emplace(At,Extent);
			}
		}
		for(const auto& Piece:Pieces)
		{
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Piece.Key, FRotator::ZeroRotator);
			Actor->SetActorLabel(Label);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Actor->GetStaticMeshComponent()->SetMaterial(0, Mat);
			Actor->GetStaticMeshComponent()->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
			Actor->SetActorScale3D(Piece.Value / 100.f);
			Spawned.Add(Actor);
		}
		return Spawned[0];
	};
	// Map L: the ground mesh (BattleMapL.h) and its paint, in the colours of the materials of map I: grass, fields, woods, the town,
	// the home bases, roads, water, the bridge and the ford, the objective and home markers. Higher ground is a little lighter, and a
	// darker line runs along every 2m of height (a contour line, so the slopes read from the tactical camera).
	auto BuildTerrainL=[&]()->bool
	{
		using namespace BattleMapI;
		UMaterial* GroundMat=nullptr;
		{
			const FString MatName=TEXT("M_GroundVertex"),PackageName=TEXT("/Game/Prototype/Materials/")+MatName;
			GroundMat=LoadObject<UMaterial>(nullptr,*(PackageName+TEXT(".")+MatName));
			if(!GroundMat)
			{
				UPackage* Package=CreatePackage(*PackageName);
				GroundMat=NewObject<UMaterial>(Package,*MatName,RF_Public|RF_Standalone);
				auto* Paint=NewObject<UMaterialExpressionVertexColor>(GroundMat);GroundMat->GetExpressionCollection().AddExpression(Paint);
				GroundMat->GetEditorOnlyData()->BaseColor.Expression=Paint;
				auto* Rough=NewObject<UMaterialExpressionConstant>(GroundMat);Rough->R=.9f;GroundMat->GetExpressionCollection().AddExpression(Rough);
				GroundMat->GetEditorOnlyData()->Roughness.Expression=Rough;
				GroundMat->PostEditChange();FAssetRegistryModule::AssetCreated(GroundMat);Package->MarkPackageDirty();
				FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
				UPackage::SavePackage(Package,GroundMat,*(FPaths::ProjectContentDir()/TEXT("Prototype/Materials/")+MatName+TEXT(".uasset")),Args);
			}
		}
		const BattleTerrain::FHeightField& F=*Field;
		auto Paint=[&](int32 I,int32 J)->FLinearColor
		{
			const double X=F.Origin.X+I*F.Step,Y=F.Origin.Y+J*F.Step,H=F.At(I,J);
			int32 C=0,R=0;SectorAt(X,Y,C,R);const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);
			FLinearColor Col(.16f,.24f,.11f);
			switch(K)
			{
			case TEXT('F'):if(FMath::Abs(X-M.X)<=(Cell-900)*.5&&FMath::Abs(Y-M.Y)<=(Cell-900)*.5)Col=FLinearColor(.3f,.27f,.12f);break;
			case TEXT('T'):Col=FLinearColor(.09f,.17f,.07f);break;
			case TEXT('h'):Col=FLinearColor(.2f,.3f,.14f);break;
			case TEXT('S'):Col=FLinearColor(.14f,.19f,.24f);break;
			case TEXT('H'):case TEXT('P'):case TEXT('O'):case TEXT('W'):case TEXT('D'):Col=FLinearColor(.21f,.21f,.2f);break;
			default:break;
			}
			Col*=float(.85+.3*FMath::Clamp(H/1200.,0.,1.));
			// Contour: this point is on the low side of a 2m line that passes between it and a neighbour.
			const int32 Band=FMath::FloorToInt(H/200.);
			for(const FIntPoint D:{FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)})
			{
				const int32 A=I+D.X,B=J+D.Y;
				if(A>=0&&B>=0&&A<F.NX&&B<F.NY&&FMath::FloorToInt(F.At(A,B)/200.)>Band){Col*=.8f;break;}
			}
			for(const FRoad& Rd:Roads())
			{
				const double Along=Rd.bAlongX?X:Y,Across=Rd.bAlongX?Y:X;
				if(FMath::Abs(Across-Rd.At)<=400.&&Along>=Rd.From-400.&&Along<=Rd.To+400.)Col=FLinearColor(.07f,.08f,.09f);
			}
			for(const FVector& O:BattleMapI::Objectives())if(FMath::Abs(X-O.X)<=700.&&FMath::Abs(Y-O.Y)<=700.)Col=FLinearColor(.04f,.5f,.4f);
			for(const double S:{1.,-1.})
			{
				const double PX=S*X,PY=S*Y;
				if(FMath::Abs(PX-42000.)<=1500.&&FMath::Abs(PY)<=1500.)Col=S<0?FLinearColor(.04f,.5f,.4f):FLinearColor(.65f,.12f,.07f);
				if(PX>=RiverX0&&PX<=RiverX1&&PY>=RiverY0&&PY<=RiverY1)
					Col=FMath::Abs(PY-BridgeY)<=BridgeHalf?FLinearColor(.3f,.22f,.14f):FMath::Abs(PY-FordY)<=FordHalf+100.?FLinearColor(.3f,.27f,.12f):FLinearColor(.05f,.2f,.45f);
			}
			return Col;
		};
		const int32 Tiles=BuildGroundTiles(World,F,Paint,GroundMat,TEXT("/Game/Prototype/Terrain/L"));
		double Low=DBL_MAX,High=-DBL_MAX;for(const float Z:F.Z){Low=FMath::Min(Low,double(Z));High=FMath::Max(High,double(Z));}
		UE_LOG(LogTemp,Display,TEXT("TERRAIN_FIELD_GROUND tiles=%d grid=%dx%d step_cm=%.0f low_cm=%.1f high_cm=%.1f"),Tiles,F.NX,F.NY,F.Step,Low,High);
		return Tiles>0;
	};
	if (Urban)
	{
		// Every obstacle is spawned with its 180-degree partner, so the layout is point symmetric.
		auto Pair=[&](const TCHAR* Label,FVector Center,FVector Size,bool bConcealment=false)
		{
			for(int32 Sign:{-1,1})
			{
				Box(Label,FVector(Center.X*Sign,Center.Y*Sign,Center.Z),Size,WallMat);
				// Hedges hide soldiers but are meant not to stop bullets (shots will ignore this tag).
				if(bConcealment)for(AStaticMeshActor* Actor:Spawned)Actor->Tags.Add(TEXT("BattleConcealment"));
			}
		};
		if(!UrbanH&&!UrbanL)Box(TEXT("UrbanFloor"),FVector(0,0,-60),FVector(100000,70000,120),FloorMat);
		// Map L: the ground is the mesh of the height field, painted per grid point (the roads, fields, water and markers that the
		// other maps lay as thin boxes on the flat floor would not follow the slopes).
		if(UrbanL&&!BuildTerrainL())return 9;
		FieldSplit=false;
		for(int32 Sign:{-1,1})
		{
			Box(TEXT("UrbanBoundary"),FVector(0,Sign*35000,180),FVector(100000,100,360),WallMat);
			Box(TEXT("UrbanBoundary"),FVector(Sign*50000,0,180),FVector(100,70000,360),WallMat);
			if(UrbanL)continue;
			Box(TEXT("HomeMarker"),FVector(Sign*42000,0,2),FVector(3000,3000,3),Sign<0?AllyMat:EnemyMat,false);
			if(!UrbanH&&!UrbanI)Box(TEXT("LookoutMarker"),FVector(0,Sign*16500,2),FVector(800,800,3),AllyMat,false);
			// Map F: the depots are objectives and get the objective marker below.
			if(!UrbanF&&!UrbanH&&!UrbanI)Box(TEXT("DepotMarker"),FVector(Sign*33000,Sign*15000,2),FVector(1200,1200,3),AllyMat,false);
		}
		FieldSplit=true;
		if(!UrbanL)for(int32 I=0;I<BattleFieldLayout::ObjectiveCountForMap(WideName);++I) Box(TEXT("UrbanObjective"),BattleFieldLayout::ObjectiveForMap(WideName,I)+FVector(0,0,2),FVector(1400,1400,3),AllyMat,false);
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
		// Map G: a box that stands on Z0, and a ramp over the rectangle X0..X1, Y0..Y1 that rises from the ground to Top along X
		// (bAlongX) or Y, towards the larger coordinate when bRisesPositive. Each with its 180-degree partner (east half given).
		// bTerrain marks ground to walk on (BattleTerrain.h).
		auto Piece=[&](const TCHAR* Label,double X,double Y,double Z0,double SizeX,double SizeY,double Height,bool bTerrain=false)
		{
			for(int32 Sign:{-1,1})
			{
				Box(Label,FVector(X*Sign,Y*Sign,Z0+Height*.5),FVector(SizeX,SizeY,Height),WallMat);
				if(bTerrain)for(AStaticMeshActor* Actor:Spawned)Actor->Tags.Add(BattleTerrain::Tag());
			}
			++Objects;
		};
		auto Ramp=[&](const TCHAR* Label,double X0,double X1,double Y0,double Y1,bool bAlongX,bool bRisesPositive,double Top)
		{
			const double Run=bAlongX?X1-X0:Y1-Y0,Width=bAlongX?Y1-Y0:X1-X0,Angle=FMath::Atan2(Top,Run),Length=FMath::Sqrt(Run*Run+Top*Top);
			// Thick enough that the underside is below the ground along the whole ramp: nothing to walk under.
			const double Thick=Top/FMath::Cos(Angle)+30;
			for(int32 Sign:{-1,1})
			{
				const double Dir=(bRisesPositive?1:-1)*Sign,CosA=FMath::Cos(Angle),SinA=FMath::Sin(Angle);
				const FVector Normal=bAlongX?FVector(-Dir*SinA,0,CosA):FVector(0,-Dir*SinA,CosA);
				const FVector Mid(Sign*(X0+X1)*.5,Sign*(Y0+Y1)*.5,Top*.5);
				FieldStand=false;AStaticMeshActor* Actor=Box(Label,Mid-Normal*Thick*.5,FVector(Length,Width,Thick),WallMat);FieldStand=true;
				Actor->SetActorRotation(FRotator(FMath::RadiansToDegrees(Angle),bAlongX?(Dir>0?0.:180.):(Dir>0?90.:-90.),0));
				Actor->Tags.Add(BattleTerrain::Tag());
			}
			++Objects;
		};
		if(UrbanI||UrbanL)
		{
#include "BattleMapLayoutI.inl"
		}
		else if(UrbanJ)
		{
#include "BattleMapLayoutJ.inl"
		}
		else if(UrbanK)
		{
// The layouts of map J over the sector table, roads and houses of map K (the log line of the layout says terrain-j-v1).
#define BattleMapJ BattleMapK
#include "BattleMapLayoutJ.inl"
#undef BattleMapJ
			// Sight breakers on the roads of map K (2026-11-22): with the roads open, nine shots in ten of the first matches were fired
			// from 125-150m along a road (mk1-K-*), as on map I. At the middle of every sector edge a road passes, two parked lorries
			// stand staggered, each closing a little more than half of the 9m between the sectors: no straight line along the road
			// passes, a walker goes round them (4m beside each, 10m between the two). None beside a mountain or a home base.
			{
				WallMat=Material(TEXT("M_MapRust"),FLinearColor(.36f,.2f,.1f));
				auto KindAt=[](double X,double Y){return BattleMapK::Kind(FMath::FloorToInt((X+BattleMapK::HalfX)/BattleMapK::Cell),FMath::FloorToInt((Y+BattleMapK::HalfY)/BattleMapK::Cell));};
				auto Closed=[](TCHAR K){return K==TEXT('M')||K==TEXT('S')||K==TEXT(' ');};
				int32 Breakers=0;
				for(const BattleMapK::FRoad& Rd:BattleMapK::Roads())
				{
					const double Mid=(Rd.From+Rd.To)*.5,MX=Rd.bAlongX?Mid:Rd.At,MY=Rd.bAlongX?Rd.At:Mid;
					const bool bSelf=FMath::Abs(MX)<1.&&FMath::Abs(MY)<1.;
					// Emit places the 180-degree partner: take one road of each pair (and the positive half of the road through the middle).
					if(!bSelf&&!(MX>1.||(FMath::Abs(MX)<1.&&MY>1.)))continue;
					for(double P=FMath::FloorToDouble(Rd.From/5000.)*5000.+2500.;P<Rd.To;P+=5000.)
					{
						if(P<Rd.From||(bSelf&&P<0.))continue;
						const double PX=Rd.bAlongX?P:Rd.At,PY=Rd.bAlongX?Rd.At:P;
						if(Closed(KindAt(PX+(Rd.bAlongX?0.:100.),PY+(Rd.bAlongX?100.:0.)))||Closed(KindAt(PX-(Rd.bAlongX?0.:100.),PY-(Rd.bAlongX?100.:0.))))continue;
						const double Sg=((FMath::RoundToInt(P/5000.-.5)+FMath::RoundToInt(Rd.At/5000.))&1)?1.:-1.;
						// Generation 6: one lorry across half the road at each place (4.5m of the 9.4m between the walls), on alternating sides, clear of the doorways of the walls beside the road
						// (those stand 4m to 18m and -6m to -20m along from this point). The staggered pair of generations 2-5 left a tank
						// (vehicle agent radius 2m) no way through once the road had walls on both sides (mk7-K: no tank entered the town).
						{
							// Generation 7: the lorry stands lengthwise at the side of the road (2.2m of the 9.4m, 7.1m left), so that a vehicle agent
							// 6m wide still gets past (generation 6 left 4.9m: with the agent at 5m or 6m no tank entered the town, vw-K-n5/n6).
							const double Along=P-100.,Across=Rd.At+Sg*350.;
							if(Rd.bAlongX)Emit(TEXT("RoadLorry"),Along,Across,450,220,280);else Emit(TEXT("RoadLorry"),Across,Along,220,450,280);
						}
						++Breakers;
					}
				}
				UE_LOG(LogTemp,Display,TEXT("TOWN_K_BREAKERS pairs=%d (x2 by rotation)"),Breakers);
			}
		}
		else if(UrbanH)
#include "BattleMapLayoutH.inl"
		else if(UrbanF)
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
	// Map L: one volume from below the lowest ground to above the highest upper floor; the tops of obstacles are kept out of the
	// navigation by a null-area modifier on every obstacle instead (below).
	double FieldHigh=0;if(Field)for(const float Z:Field->Z)FieldHigh=FMath::Max(FieldHigh,double(Z));
	ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, Field ? (FieldHigh+450.-50.)*.5 : Urban ? 52.5 : 150), FRotator::ZeroRotator);
	Bounds->SetActorLabel(TEXT("ArenaNavigationBounds"));
	UCubeBuilder* Builder = NewObject<UCubeBuilder>(Bounds);
	Builder->X = Urban?99800:Expanded?79800:Sight ? 59800 : Large ? 23800 : 7800; Builder->Y = Urban?69800:Expanded?59800:Sight ? 39800 : Large ? 17800 : 5800; Builder->Z = Field ? FieldHigh+500. : Urban ? 145 : 1200;
	UActorFactory::CreateBrushForVolumeActor(Bounds, Builder);
	Bounds->GetBrushComponent()->BuildSimpleBrushCollision();
	Bounds->PostEditChange();
	// Map G: the ground volume stays below the tops of the obstacles (see above), so every hill, staircase and upper floor gets a
	// volume of its own that reaches just over it. Cover that stands on those is 130cm high, above the volume.
	TArray<ANavMeshBoundsVolume*> TerrainVolumes;
	int32 NoRoof=0;
	if(Field)
	{
		for(TActorIterator<AStaticMeshActor> It(World);It;++It)
		{
			AStaticMeshActor* Actor=*It;UStaticMeshComponent* Mesh=Actor->GetStaticMeshComponent();
			if(BattleTerrain::IsGround(Actor)||!Mesh||Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
			if(Actor->GetActorLabel().StartsWith(TEXT("UrbanBoundary")))continue;
			UNavModifierComponent* Modifier=NewObject<UNavModifierComponent>(Actor,TEXT("NoRoof"));
			Modifier->SetAreaClass(UNavArea_Null::StaticClass());Actor->AddInstanceComponent(Modifier);Modifier->RegisterComponent();++NoRoof;
		}
		UE_LOG(LogTemp,Display,TEXT("TERRAIN_FIELD_NAV no_roof_modifiers=%d volume_top_cm=%.0f"),NoRoof,FieldHigh+450.);
	}
	if(Terrain&&!Field)
	{
		auto Extra=[&](double X0,double X1,double Y0,double Y1,double Z0,double Z1)
		{
			for(int32 Sign:{-1,1})
			{
				ANavMeshBoundsVolume* Volume=World->SpawnActor<ANavMeshBoundsVolume>(FVector(Sign*(X0+X1)*.5,Sign*(Y0+Y1)*.5,(Z0+Z1)*.5),FRotator::ZeroRotator);
				Volume->SetActorLabel(TEXT("TerrainNavigationBounds"));
				UCubeBuilder* Cube=NewObject<UCubeBuilder>(Volume);Cube->X=X1-X0;Cube->Y=Y1-Y0;Cube->Z=Z1-Z0;
				UActorFactory::CreateBrushForVolumeActor(Volume,Cube);Volume->GetBrushComponent()->BuildSimpleBrushCollision();Volume->PostEditChange();
				TerrainVolumes.Add(Volume);
			}
		};
		for(const BattleTerrain::FHouse& B:TerrainHouses)
		{
			const BattleTerrain::FHouseShape S(B);const double F=BattleTerrain::FloorZ;
			Extra(S.W0-20,S.E0+20,S.S0y-20,S.N0y+20,F-30,F+120);
			Extra(S.S0-30,S.S1+30,S.SY-10,S.N0y+10,100,F+120);
		}
		for(const BattleTerrain::FHill& H:TerrainHills)
			Extra(H.X-H.W*.5-H.Run-50,H.X+H.W*.5+H.Run+50,H.Y-H.H*.5-H.Run-50,H.Y+H.H*.5+H.Run+50,100,H.Top+120);
	}
	// Map I: the rivers are holes in the navigation mesh (no collision: sight and shots cross the water). The gaps between the
	// pieces are the bridge (10m, vehicles too) and the ford (3m, too narrow for the vehicle agent).
	if(UrbanI||UrbanL)
	{
		using namespace BattleMapI;
		const double Cuts[4][2]={{RiverY0,BridgeY-BridgeHalf},{BridgeY+BridgeHalf,FordY-FordHalf},{FordY+FordHalf,RiverY1},{0,0}};
		for(int32 I=0;I<3;++I)for(int32 Sign:{-1,1})
		{
			// Map L: from 1m below the lowest ground of the piece to 3m above its highest (map I: -100..220 over the flat floor).
			double Low=0,High=0;if(Field)Field->Range(Sign>0?RiverX0:-RiverX1,Sign>0?Cuts[I][0]:-Cuts[I][1],Sign>0?RiverX1:-RiverX0,Sign>0?Cuts[I][1]:-Cuts[I][0],Low,High);
			ANavModifierVolume* Water=World->SpawnActor<ANavModifierVolume>(FVector(Sign*(RiverX0+RiverX1)*.5,Sign*(Cuts[I][0]+Cuts[I][1])*.5,Field?(Low-100.+High+300.)*.5:60),FRotator::ZeroRotator);
			Water->SetActorLabel(TEXT("RiverNoWalk"));
			UCubeBuilder* WaterCube=NewObject<UCubeBuilder>(Water);WaterCube->X=RiverX1-RiverX0;WaterCube->Y=Cuts[I][1]-Cuts[I][0];WaterCube->Z=Field?High+300.-(Low-100.):320;
			UActorFactory::CreateBrushForVolumeActor(Water,WaterCube);Water->GetBrushComponent()->BuildSimpleBrushCollision();
			Water->SetAreaClass(UNavArea_Null::StaticClass());Water->PostEditChange();
		}
	}
	// Map J: mountains and lakes are holes in the navigation mesh (mountains also stand there as solid rock; lakes have no collision,
	// so sight and shots cross the water). One volume per sector, with its 180-degree partner.
	if(UrbanJ||UrbanK)
	{
		for(int32 Cx=BattleMapJ::Columns/2;Cx<BattleMapJ::Columns;++Cx)for(int32 Rx=0;Rx<BattleMapJ::Rows;++Rx)
		{
			const TCHAR Kx=KindJK(Cx,Rx);if(!BattleMapJ::Impassable(Kx))continue;
			const FVector2D Mx=BattleMapJ::Centre(Cx,Rx);
			for(int32 Sign:{-1,1})
			{
				ANavModifierVolume* NoWalk=World->SpawnActor<ANavModifierVolume>(FVector(Sign*Mx.X,Sign*Mx.Y,60),FRotator::ZeroRotator);
				NoWalk->SetActorLabel(Kx==TEXT('M')?TEXT("MountainNoWalk"):TEXT("LakeNoWalk"));
				UCubeBuilder* NoWalkCube=NewObject<UCubeBuilder>(NoWalk);NoWalkCube->X=BattleMapJ::Cell;NoWalkCube->Y=BattleMapJ::Cell;NoWalkCube->Z=320;
				UActorFactory::CreateBrushForVolumeActor(NoWalk,NoWalkCube);NoWalk->GetBrushComponent()->BuildSimpleBrushCollision();
				NoWalk->SetAreaClass(UNavArea_Null::StaticClass());NoWalk->PostEditChange();
			}
		}
	}
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		// Commandlets do not tick the editor loading loop that normally releases this lock.
		Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		Nav->OnNavigationBoundsUpdated(Bounds);
		for(ANavMeshBoundsVolume* Volume:TerrainVolumes)Nav->OnNavigationBoundsUpdated(Volume);
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
            if(UrbanH)
            {
                const auto& Def=BattleFieldLayout::ByName(WideName);
                int BlockedRoads=0;
                for(const auto& Edge:Def.RouteEdges){
                    const FVector A=Def.RouteNodes[Edge.A],B=Def.RouteNodes[Edge.B];
                    const FVector Direction=(B-A).GetSafeNormal2D();
                    const FVector Across(-Direction.Y,Direction.X,0);
                    const double HalfWidth=(Edge.Lane==BattleFieldLayout::ERouteLane::Centre?BattleSectors::MainWidth:
                        Edge.Lane==BattleFieldLayout::ERouteLane::Connector?BattleSectors::StreetWidth:BattleSectors::SideWidth)*.5;
                    for(double Fraction:{-.85,0.,.85}){
                        const FVector Offset=Across*(HalfWidth*Fraction)+FVector(0,0,100);
                        if(World->LineTraceTestByChannel(A+Offset,B+Offset,ECC_Visibility)){
                            ++BlockedRoads;
                            UE_LOG(LogTemp,Warning,TEXT("SECTOR_ROAD_BLOCKED from=%s to=%s"),*A.ToString(),*B.ToString());
                            break;
                        }
                    }
                }
                Failures+=BlockedRoads;
                UE_LOG(LogTemp,Display,TEXT("SECTOR_ROAD_RESULT blocked=%d segments=%d"),BlockedRoads,Def.RouteEdges.Num());
            }
			UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_RESULT failures=%d open_lines=%d layout=%s rotation=180"),Failures,OpenLines,UrbanL?TEXT("terrain-l-v1"):UrbanK?TEXT("town-k-v1"):UrbanJ?TEXT("terrain-j-v1"):UrbanI?TEXT("sectors-v2"):UrbanH?TEXT("river-town-v2"):UrbanG?TEXT("heights-v1"):UrbanF?TEXT("districts-v1"):TEXT("urban-v2"));
			if(Terrain)
			{
				// The geometry agrees with the height table (a trace down at nine points of every patch; points under a crate or a
				// wall that stands on the patch are skipped), and every upper floor and hilltop is reached from the west home.
				const auto& Heights=BattleFieldLayout::ByName(WideName).Heights;
				int32 Samples=0,Mismatched=0,Unreached=0,Goals=0;
				for(const auto& P:Heights.Patches) for(double U:{.25,.5,.75}) for(double V:{.25,.5,.75})
				{
					const double X=FMath::Lerp(P.Min.X,P.Max.X,U),Y=FMath::Lerp(P.Min.Y,P.Max.Y,V),Z=P.HeightAt(X,Y);FHitResult Hit;
					const bool bHit=World->LineTraceSingleByObjectType(Hit,FVector(X,Y,Z+110),FVector(X,Y,Z-110),FCollisionObjectQueryParams(ECC_WorldStatic));
					if(bHit&&!BattleTerrain::IsGround(Hit))continue;
					++Samples;
					if(!bHit||FMath::Abs(Hit.ImpactPoint.Z-Z)>3){if(++Mismatched<=8)UE_LOG(LogTemp,Warning,TEXT("TERRAIN_MAP_MISMATCH x=%.0f y=%.0f table=%.1f traced=%.1f hit=%d"),X,Y,Z,bHit?Hit.ImpactPoint.Z:-1.,bHit);}
				}
				auto Reach=[&](const FVector& Goal)
				{
					++Goals;auto* Path=Nav->FindPathToLocationSynchronously(World,FVector(-42000,0,0),Goal);
					const bool bOK=Path&&Path->IsValid()&&!Path->IsPartial()&&Path->PathPoints.Num()>1&&FMath::Abs(Path->PathPoints.Last().Z-Goal.Z)<40;
					if(!bOK){++Unreached;UE_LOG(LogTemp,Warning,TEXT("TERRAIN_MAP_UNREACHED x=%.0f y=%.0f z=%.0f end_z=%.1f"),Goal.X,Goal.Y,Goal.Z,Path&&Path->PathPoints.Num()?Path->PathPoints.Last().Z:-1.);}
				};
				for(int32 Sign:{-1,1})
				{
					for(const BattleTerrain::FHouse& B:TerrainHouses){const BattleTerrain::FHouseShape S(B);Reach(FVector(Sign*B.X,Sign*(S.S0y+S.SY)*.5+60,GroundZ(B.X,B.Y)+BattleTerrain::FloorZ));}
					for(const BattleTerrain::FHill& H:TerrainHills)Reach(FVector(Sign*H.X,Sign*H.Y,H.Top));
					// Map L: the tops of the knoll and the ridge (the hills of map I, part of the field).
					if(Field)for(const FVector2D Top:{FVector2D(22500,7500),FVector2D(20000,-29500)})Reach(FVector(Sign*Top.X,Sign*Top.Y,GroundZ(Top.X,Top.Y)));
				}
				// Map L: the ground mesh is the field (a trace down every 10m; points where an obstacle stands are skipped), and no
				// obstacle top carries navigation (a point 40cm above the middle of the top of every obstacle wider than 70cm).
				int32 FieldSamples=0,FieldMismatched=0,Roofs=0,RoofChecked=0;double FieldWorst=0;
				if(Field)
				{
					for(double X=-49500;X<=49500;X+=1000)for(double Y=-34500;Y<=34500;Y+=1000)
					{
						const double Z=GroundZ(X,Y);FHitResult Hit;
						const bool bHit=World->LineTraceSingleByObjectType(Hit,FVector(X,Y,Z+300),FVector(X,Y,Z-300),FCollisionObjectQueryParams(ECC_WorldStatic));
						if(bHit&&!BattleTerrain::IsGround(Hit))continue;
						++FieldSamples;const double Gap=bHit?FMath::Abs(Hit.ImpactPoint.Z-Z):1.e6;FieldWorst=FMath::Max(FieldWorst,Gap);
						if(Gap>3){if(++FieldMismatched<=8)UE_LOG(LogTemp,Warning,TEXT("TERRAIN_FIELD_MISMATCH x=%.0f y=%.0f table=%.1f traced=%.1f hit=%d"),X,Y,Z,bHit?Hit.ImpactPoint.Z:-1.,bHit);}
					}
					for(TActorIterator<AStaticMeshActor> It(World);It;++It)
					{
						if(BattleTerrain::IsGround(*It)||It->GetStaticMeshComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
						FVector C,E;It->GetActorBounds(false,C,E);if(E.X<35||E.Y<35||E.X>3000||E.Y>3000)continue;
						++RoofChecked;FNavLocation On;
						if(Nav->ProjectPointToNavigation(FVector(C.X,C.Y,C.Z+E.Z+40),On,FVector(E.X-30,E.Y-30,60))&&On.Location.Z>C.Z+E.Z-30)
						{if(++Roofs<=8)UE_LOG(LogTemp,Warning,TEXT("TERRAIN_FIELD_ROOF label=%s x=%.0f y=%.0f top=%.0f nav_z=%.0f"),*It->GetActorLabel(),C.X,C.Y,C.Z+E.Z,On.Location.Z);}
					}
					UE_LOG(LogTemp,Display,TEXT("TERRAIN_FIELD_RESULT failures=%d samples=%d mismatched=%d worst_cm=%.2f roofs_checked=%d roofs=%d"),FieldMismatched+Roofs,FieldSamples,FieldMismatched,FieldWorst,RoofChecked,Roofs);
				}
				UE_LOG(LogTemp,Display,TEXT("TERRAIN_MAP_RESULT failures=%d samples=%d mismatched=%d goals=%d unreached=%d volumes=%d"),Mismatched+Unreached,Samples,Mismatched,Goals,Unreached,TerrainVolumes.Num());
				if(Mismatched+Unreached+FieldMismatched+Roofs) return 8;
			}
			if(UrbanI||UrbanL)
			{
				using namespace BattleMapI;
				// River: no ground to stand on in the water; the ford and the bridge are crossed on foot; a tank crosses by the bridge only.
				FNavLocation InWater;const double MidX=(RiverX0+RiverX1)*.5;
				const bool bWet=!Nav->ProjectPointToNavigation(FVector(MidX,15000,GroundZ(MidX,15000)),InWater,FVector(60,60,200));
				auto Cross=[&](double Y){auto* Path=Nav->FindPathToLocationSynchronously(World,FVector(RiverX0-600,Y,GroundZ(RiverX0-600,Y)),FVector(RiverX1+600,Y,GroundZ(RiverX1+600,Y)));return Path&&Path->IsValid()&&!Path->IsPartial()?Path->GetPathLength():-1.f;};
				const float Ford=Cross(FordY),Bridge=Cross(BridgeY),Bank=Cross(15000);
				const bool bRiver=bWet&&Ford>0&&Ford<4000&&Bridge>0&&Bridge<4000&&(Bank<0||Bank>8000);
				UE_LOG(LogTemp,Display,TEXT("SECTOR_MAP_RIVER ok=%d water_blocked=%d ford_cm=%.0f bridge_cm=%.0f bank_to_bank_cm=%.0f"),bRiver?1:0,bWet?1:0,Ford,Bridge,Bank);
				// Cover of every sector: standing places (2.5m grid, on the navigation mesh, ground level) that are shielded within 2.5m
				// from each of the four sides, the test the cover-first arrival uses. Every sector but fields, rivers, hills and the
				// home bases must offer at least six per side.
				int32 Checked=0,Short=0,Least=MAX_int32;int32 FieldSum=0,FieldCount=0;
				for(int32 C=0;C<Columns;++C)for(int32 R=0;R<Rows;++R)
				{
					const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);int32 N[4]={0,0,0,0};
					for(double DX=-2250;DX<=2250;DX+=250)for(double DY=-2250;DY<=2250;DY+=250)
					{
						FNavLocation At;if(!Nav->ProjectPointToNavigation(FVector(M.X+DX,M.Y+DY,GroundZ(M.X+DX,M.Y+DY)),At,FVector(40,40,60)))continue;
						const FVector Eye=At.Location+FVector(0,0,100);const FVector Dirs[4]={FVector(0,1,0),FVector(1,0,0),FVector(0,-1,0),FVector(-1,0,0)};
						for(int32 D=0;D<4;++D)N[D]+=World->LineTraceTestByObjectType(Eye,Eye+Dirs[D]*250,FCollisionObjectQueryParams(ECC_WorldStatic));
					}
					const int32 Min=FMath::Min(FMath::Min(N[0],N[1]),FMath::Min(N[2],N[3]));
					if(K==TEXT('F')){FieldSum+=Min;++FieldCount;continue;}
					if(K==TEXT('R')||K==TEXT('h')||K==TEXT('S'))continue;
					++Checked;Least=FMath::Min(Least,Min);
					if(Min<6){++Short;UE_LOG(LogTemp,Warning,TEXT("SECTOR_MAP_COVER_SHORT c=%d r=%d kind=%c n=%d e=%d s=%d w=%d"),C,R,K,N[0],N[1],N[2],N[3]);}
				}
				UE_LOG(LogTemp,Display,TEXT("SECTOR_MAP_COVER sectors=%d checked=%d short=%d least_per_side=%d fields=%d field_mean_per_side=%.1f"),Columns*Rows,Checked,Short,Least,FieldCount,FieldCount?double(FieldSum)/FieldCount:0.);
				UE_LOG(LogTemp,Display,TEXT("SECTOR_MAP_RESULT failures=%d"),(bRiver?0:1)+Short);
			}
			if(UrbanJ||UrbanK)
			{
				// Map J: no ground in a mountain or a lake sector (25 samples each); the passes and the lanes connect (homes and objectives
				// were checked above); cover towards every side in the sectors that are held (as map I; fields, hills, mountains, lakes and
				// home bases are not counted). A shortage of cover is reported, not a failure (the varied layouts are new, 2026-11-22).
				int32 Blocked=0,Leaks=0,Checked=0,Short=0,Least=MAX_int32;
				for(int32 Cx=0;Cx<BattleMapJ::Columns;++Cx)for(int32 Rx=0;Rx<BattleMapJ::Rows;++Rx)
				{
					const TCHAR Kx=KindJK(Cx,Rx);const FVector2D Mx=BattleMapJ::Centre(Cx,Rx);
					if(BattleMapJ::Impassable(Kx))
					{
						++Blocked;int32 Ground=0;FNavLocation At;
						for(double DX=-2000;DX<=2000;DX+=1000)for(double DY=-2000;DY<=2000;DY+=1000)Ground+=Nav->ProjectPointToNavigation(FVector(Mx.X+DX,Mx.Y+DY,0),At,FVector(40,40,60));
						if(Ground){++Leaks;UE_LOG(LogTemp,Warning,TEXT("TERRAIN_J_LEAK c=%d r=%d kind=%c ground_samples=%d/25"),Cx,Rx,Kx,Ground);}
						continue;
					}
					if(Kx==TEXT('F')||Kx==TEXT('h')||Kx==TEXT('S'))continue;
					int32 N[4]={0,0,0,0};
					for(double DX=-2250;DX<=2250;DX+=250)for(double DY=-2250;DY<=2250;DY+=250)
					{
						FNavLocation At;if(!Nav->ProjectPointToNavigation(FVector(Mx.X+DX,Mx.Y+DY,0),At,FVector(40,40,60)))continue;
						const FVector Eye=At.Location+FVector(0,0,100);const FVector Dirs[4]={FVector(0,1,0),FVector(1,0,0),FVector(0,-1,0),FVector(-1,0,0)};
						for(int32 D=0;D<4;++D)N[D]+=World->LineTraceTestByObjectType(Eye,Eye+Dirs[D]*250,FCollisionObjectQueryParams(ECC_WorldStatic));
					}
					const int32 Min=FMath::Min(FMath::Min(N[0],N[1]),FMath::Min(N[2],N[3]));++Checked;Least=FMath::Min(Least,Min);
					if(Min<6){++Short;UE_LOG(LogTemp,Warning,TEXT("SECTOR_MAP_COVER_SHORT c=%d r=%d kind=%c n=%d e=%d s=%d w=%d"),Cx,Rx,Kx,N[0],N[1],N[2],N[3]);}
				}
				// Across a range: from the north lane to the valley the path must go round by a pass (longer than 1.3 x the straight line).
				auto* Around=Nav->FindPathToLocationSynchronously(World,FVector(2500,27500,0),FVector(2500,7500,0));
				// Map K has no range to go round: its mountains only ring the town (the homes and objectives were reached above).
				const bool bAround=UrbanK||(Around&&Around->IsValid()&&!Around->IsPartial()&&Around->GetPathLength()>1.3*20000.);
				UE_LOG(LogTemp,Display,TEXT("TERRAIN_J_RESULT failures=%d blocked_sectors=%d leaks=%d detour_cm=%.0f detour_ok=%d cover_checked=%d cover_short=%d least_per_side=%d"),
					Leaks+(bAround?0:1),Blocked,Leaks,Around&&Around->IsValid()?Around->GetPathLength():-1.,bAround?1:0,Checked,Short,Least==MAX_int32?0:Least);
				if(Leaks||!bAround) return 7;
			}
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
