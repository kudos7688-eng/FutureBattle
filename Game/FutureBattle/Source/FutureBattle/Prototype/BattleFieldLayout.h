#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "BattleSectors.h"
#include "BattleMapI.h"
#include "BattleMapL.h"
#include "BattleMapJ.h"
#include "BattleMapK.h"

namespace BattleFieldLayout
{
	inline FVector Expand(FVector P,bool Expanded) {return Expanded?P*FVector(4./3.,1.5,1):P;}
	inline FVector Objective(int32 Index,bool Expanded=false) { return Expand(Index==0?FVector(-7000,12000,0):Index==1?FVector::ZeroVector:FVector(7000,-12000,0),Expanded); }

	// Map definitions (2026-11-18). Everything that differs by map lives in one table entry: to add a map, add an entry to Maps()
	// and a generator branch in BattleMapCommandlet (and the map letter in the Setup scripts that take -Map). Code asks the definition (Of(World) / ByName(Map)) instead of testing map names.
	// Arena = the 80m x 60m prototype arena and any other level; Field500 = the fixed 500-soldier field; Wide = the area-battle maps.
	enum class EMapKind : uint8 { Arena, Field500, Wide };
	// Road network (BattleRoutes.cpp): the three main routes and the connectors between them.
	enum class ERouteLane : uint8 { Centre, North, South, Connector };
	struct FRouteEdge { int32 A=0,B=0; ERouteLane Lane=ERouteLane::Connector; };
	struct FMapDef
	{
		const TCHAR* Name=TEXT("");
		EMapKind Kind=EMapKind::Arena;
		// Map D: map A scaled by Scale, with the extra objective cover its tests and audit rely on.
		bool bExpanded=false;
		// Rule set introduced with map E: range profile, stances, cover-first arrival, partial paths, own re-paths, denser cover slots,
		// the larger income. Independent of the geometry below, so a new map can take it without being map E.
		bool bUrban=false;
		bool bSectors=false;
		// 2026-11-22 (user decision, roadmap stage 1): the sector AI combination is on by default on this map (see SectorComboSwitch).
		bool bSectorCombo=false;
		// Kind of a sector on a map with sectors, in the letters of BattleSectors.h (R river, B bridge approach, S ford, J junction or
		// plaza, H house, W warehouse, F field, T wood, ^ hill). Null on maps without sectors.
		char (*SectorKind)(FIntPoint)=nullptr;
		FVector Scale=FVector::OneVector;
		// Half size of the playable floor (X, Y) in cm.
		FVector2D HalfExtent=FVector2D(4000,3000);
		float HomeDistance=10000;
		// X of the first starting column of the wide maps.
		double SpawnX=25000;
		// Objectives of the wide maps. Index I and Num-1-I are 180-degree counterparts, which the team order rotation relies on.
		TArray<FVector> Objectives;
		// Per objective, both empty on maps without them. Owner: the side that holds it when the match starts (0 = the team of the
		// west home, 1 = of the east home, -1 = nobody). Value: multiplies its income and its end score (a depot is worth more).
		TArray<int32> ObjectiveOwners;
		TArray<float> ObjectiveValues;
		// Multiplies the income of every objective (not the base income). Map F has nine objectives against five on map E, so it
		// pays half per objective; with the full amount each team ended with about 800 unspent resources (mapf1-F-*).
		float PointIncomeScale=1.f;
		// Junctions and road sections of the map's routes; empty = squads go straight to their goal.
		TArray<FVector> RouteNodes;
		TArray<FRouteEdge> RouteEdges;
		// A* node budget of the vehicle navmesh; 0 = the engine default (2048). On map E a tank's path to a goal beyond about 550m
		// came back partial with the default (Setup/Benchmarks/MapStats/mapstats-E-r1), so maps E and F raise it like the infantry one.
		int32 VehicleSearchNodes=0;
		// Lookouts (ROADMAP 2026-11-14): public places where a still soldier sees far (UBattleTuning::LookoutSightRange).
		TArray<FVector> Lookouts;
		// Half size of the built-up centre (slow for vehicles, framed on the minimap); zero = none.
		FVector2D CityHalf=FVector2D::ZeroVector;
		float MatchSeconds=180;
		float CameraMaxHeight=4500;
		// Largest |X|, |Y| a saved position may have.
		FVector2D SaveLimit=FVector2D(32000,22000);
		// Ground heights (BattleTerrain.h): hills, stairs and upper floors. Empty on the flat maps A-F.
		BattleTerrain::FHeights Heights;
	};
	inline const TArray<FMapDef>& Maps()
	{
		static const TArray<FMapDef> Table=[]
		{
			TArray<FMapDef> T;
			{FMapDef D;D.Name=TEXT("Lvl_BattlePrototype");T.Add(D);}
			{FMapDef D;D.Name=TEXT("Lvl_BattleField500");D.Kind=EMapKind::Field500;D.HalfExtent=FVector2D(12000,9000);D.CameraMaxHeight=30000;T.Add(D);}
			// A-D: 600m x 400m (D: 800m x 600m), three objectives on a diagonal, lookouts at the field edges beside the centre.
			for(const TCHAR* Name:{TEXT("Lvl_BattleWide_A"),TEXT("Lvl_BattleWide_B"),TEXT("Lvl_BattleWide_C"),TEXT("Lvl_BattleWide_D")})
			{
				FMapDef D;D.Name=Name;D.Kind=EMapKind::Wide;D.bExpanded=FCString::Strcmp(Name,TEXT("Lvl_BattleWide_D"))==0;
				if(D.bExpanded){D.Scale=FVector(4./3.,1.5,1);D.HalfExtent=FVector2D(40000,30000);D.HomeDistance=36000;D.CameraMaxHeight=130000;D.SaveLimit=FVector2D(42000,32000);}
				else{D.HalfExtent=FVector2D(30000,20000);D.HomeDistance=27000;D.CameraMaxHeight=100000;}
				for(int32 I=0;I<3;++I)D.Objectives.Add(Objective(I,D.bExpanded));
				D.Lookouts={Expand(FVector(0,17000,0),D.bExpanded),Expand(FVector(0,-17000,0),D.bExpanded)};
				D.MatchSeconds=600;T.Add(D);
			}
			// E (2026-11-14): 1000m x 700m, urban centre (500m x 300m) with five objectives at its crossings, open flanks, lookouts at
			// the city edges overlooking the flanks.
			FMapDef Urban;Urban.Kind=EMapKind::Wide;Urban.bUrban=true;
			Urban.HalfExtent=FVector2D(50000,35000);Urban.HomeDistance=42000;Urban.SpawnX=39000;
			Urban.Lookouts={FVector(0,16500,0),FVector(0,-16500,0)};
			Urban.CityHalf=FVector2D(25000,15000);Urban.MatchSeconds=900;Urban.CameraMaxHeight=160000;Urban.SaveLimit=FVector2D(52000,37000);
			Urban.VehicleSearchNodes=32768;
			{
				FMapDef D=Urban;D.Name=TEXT("Lvl_BattleWide_E");
				D.Objectives={FVector(-13000,7500,0),FVector(-13000,-7500,0),FVector::ZeroVector,FVector(13000,7500,0),FVector(13000,-7500,0)};
				T.Add(D);
			}
			// F (2026-11-18): the size, homes and city objectives of E; plus a flank objective and a depot for each side, and roads.
			// Objectives: 0 A, 1 D, 2 north flank, 3 west depot, 4 C, 5 east depot, 6 south flank, 7 B, 8 E.
			{
				FMapDef D=Urban;D.Name=TEXT("Lvl_BattleWide_F");
				D.Objectives={FVector(-13000,7500,0),FVector(-13000,-7500,0),FVector(-15000,25000,0),FVector(-33000,-20000,0),FVector::ZeroVector,
					FVector(33000,20000,0),FVector(15000,-25000,0),FVector(13000,7500,0),FVector(13000,-7500,0)};
				D.ObjectiveOwners={-1,-1,-1,0,-1,1,-1,-1,-1};
				D.ObjectiveValues={1,1,1,2,1,2,1,1,1};
				D.PointIncomeScale=.5f;
				// Roads: the avenue (centre), the flank roads at y = +-250m (north, south), and connectors: the streets and edges of the
				// city, the x = 0 / +-130m / +-250m roads from the city to the flank roads, the x = +-380m road in front of each home,
				// and a spur to each depot. The same lines are kept free of scattered cover in BattleMapLayoutF.inl.
				auto Node=[&D](double X,double Y){const FVector P(X,Y,0);for(int32 I=0;I<D.RouteNodes.Num();++I)if(D.RouteNodes[I].Equals(P,1))return I;return D.RouteNodes.Add(P);};
				auto Edge=[&](double X0,double Y0,double X1,double Y1,ERouteLane Lane){FRouteEdge E;E.A=Node(X0,Y0);E.B=Node(X1,Y1);E.Lane=Lane;D.RouteEdges.Add(E);};
				const double AvenueX[]={-42000,-38000,-25000,-13000,0,13000,25000,38000,42000};
				for(int32 I=0;I<8;++I)Edge(AvenueX[I],0,AvenueX[I+1],0,ERouteLane::Centre);
				const double RoadX[]={-38000,-25000,-13000,0,13000,25000,38000};
				for(int32 I=0;I<6;++I){Edge(RoadX[I],25000,RoadX[I+1],25000,ERouteLane::North);Edge(RoadX[I],-25000,RoadX[I+1],-25000,ERouteLane::South);}
				const double CityX[]={-25000,-13000,0,13000,25000},RowY[]={25000,15000,7500,0,-7500,-15000,-25000};
				for(double Y:{15000.,7500.,-7500.,-15000.})for(int32 I=0;I<4;++I)Edge(CityX[I],Y,CityX[I+1],Y,ERouteLane::Connector);
				for(double X:CityX)for(int32 I=0;I<6;++I)Edge(X,RowY[I],X,RowY[I+1],ERouteLane::Connector);
				for(double S:{-1.,1.})
				{
					Edge(S*38000,25000,S*38000,0,ERouteLane::Connector);Edge(S*38000,0,S*38000,-25000,ERouteLane::Connector);
					Edge(S*33000,S*20000,S*38000,S*25000,ERouteLane::Connector);Edge(S*33000,S*20000,S*25000,S*15000,ERouteLane::Connector);Edge(S*33000,S*20000,S*25000,S*25000,ERouteLane::Connector);
				}
				T.Add(D);
			}
			// G (2026-11-18): map F with height differences: hills on the flanks and the approaches, two-storey houses that can be
			// entered (stairs, an upper floor with windows), more cover beside the roads. Objectives, roads and rules as on F.
			{
				FMapDef D=T.Last();D.Name=TEXT("Lvl_BattleWide_G");
				D.Heights=BattleTerrain::MakeG(D.HalfExtent);
				T.Add(D);
			}
			// I (2026-11-19): designed from 50m sectors (BattleMapI.h): a town on both sides of the middle, fields, woods, a river with
			// a bridge and a ford on each side, hills of 5m and 7m. Eight objectives, each the middle of one sector; the depots (0, 7)
			// start owned and are worth double. Roads on sector borders: the avenue, a north and a south road, seven cross roads.
			{
				FMapDef D=Urban;D.Name=TEXT("Lvl_BattleWide_I");
				// Sector orders as on map H (the same 20 x 14 grid of 50m, the same origin): capture and hold by sector, objectives held by
				// the whole sector. Map I's own kinds are given in the letters of BattleSectors.h.
				D.bSectors=true;D.bSectorCombo=true;
				D.SectorKind=[](FIntPoint C)->char
				{
					switch(BattleMapI::Kind(C.X,C.Y))
					{
					case TEXT('h'):return '^';case TEXT('P'):case TEXT('O'):return 'J';case TEXT('D'):case TEXT('W'):return 'W';
					case TEXT('H'):return 'H';case TEXT('T'):return 'T';case TEXT('R'):return 'R';default:return 'F';
					}
				};
				D.Objectives=BattleMapI::Objectives();
				D.ObjectiveOwners={0,-1,-1,-1,-1,-1,-1,1};
				D.ObjectiveValues={2,1,1,1,1,1,1,2};
				D.PointIncomeScale=.5f;
				D.CityHalf=FVector2D(25000,20000);
				// Lookouts: the knolls inside the town's edge.
				D.Lookouts={FVector(22500,7500,500),FVector(-22500,-7500,500)};
				auto Node=[&D](double X,double Y){const FVector P(X,Y,0);for(int32 I=0;I<D.RouteNodes.Num();++I)if(D.RouteNodes[I].Equals(P,1))return I;return D.RouteNodes.Add(P);};
				auto Edge=[&](double X0,double Y0,double X1,double Y1,ERouteLane Lane){FRouteEdge E;E.A=Node(X0,Y0);E.B=Node(X1,Y1);E.Lane=Lane;D.RouteEdges.Add(E);};
				const double AvenueX[]={-42000,-35000,-25000,-10000,0,10000,25000,35000,42000};
				for(int32 I=0;I<8;++I)Edge(AvenueX[I],0,AvenueX[I+1],0,ERouteLane::Centre);
				const double RoadX[]={-35000,-25000,-10000,0,10000,25000,35000};
				for(int32 I=0;I<6;++I){Edge(RoadX[I],20000,RoadX[I+1],20000,ERouteLane::North);Edge(RoadX[I],-20000,RoadX[I+1],-20000,ERouteLane::South);}
				for(double X:RoadX){Edge(X,20000,X,0,ERouteLane::Connector);Edge(X,0,X,-20000,ERouteLane::Connector);}
				D.Heights=BattleMapI::MakeHeights(D.HalfExtent);
				T.Add(D);
			}
			// L (2026-11-25, Docs/design-terrain-field.md): map I on rolling ground (BattleMapL.h). The same sectors, roads, river,
			// houses, objectives, rules and sector AI combination as I; the ground is a height field, the hills are smooth, the homes flat.
			{
				FMapDef D=T.Last();D.Name=TEXT("Lvl_BattleWide_L");
				D.Heights=BattleMapL::MakeHeights(D.HalfExtent);
				D.Objectives=BattleMapL::Objectives(D.Heights.Field);
				// Lookouts: the tops of the knolls (as on I), at the height of the ground there.
				D.Lookouts={FVector(22500,7500,0),FVector(-22500,-7500,0)};
				for(FVector& P:D.Lookouts)P.Z=D.Heights.Field.Height(P.X,P.Y);
				T.Add(D);
			}
			// J (2026-11-22, Docs/design-map-j-terrain.md): the grid of I with two mountain ranges (impassable, they block sight) that split
			// the field into a north lane, a valley with the town and a lake, and a south lane, joined by passes; varied buildings (office
			// towers, apartment blocks, a church, factories, ruins, car parks, parks, shops). Eight objectives as on I (0 and 7 depots).
			{
				FMapDef D=Urban;D.Name=TEXT("Lvl_BattleWide_J");
				D.bSectors=true;
				D.SectorKind=[](FIntPoint C)->char
				{
					switch(BattleMapJ::Kind(C.X,C.Y))
					{
					case TEXT('h'):return '^';case TEXT('P'):case TEXT('O'):case TEXT('Y'):return 'J';
					case TEXT('D'):case TEXT('W'):case TEXT('K'):return 'W';
					case TEXT('H'):case TEXT('B'):case TEXT('A'):case TEXT('C'):case TEXT('Q'):case TEXT('U'):return 'H';
					case TEXT('T'):case TEXT('G'):return 'T';case TEXT('M'):return 'M';case TEXT('L'):return 'L';default:return 'F';
					}
				};
				D.Objectives=BattleMapJ::Objectives();
				D.ObjectiveOwners={0,-1,-1,-1,-1,-1,-1,1};
				D.ObjectiveValues={2,1,1,1,1,1,1,2};
				D.PointIncomeScale=.5f;
				D.CityHalf=FVector2D(35000,15000);
				// Lookouts: the ridges at the outer edge of the south lane (east) and the north lane (west).
				D.Lookouts={FVector(15000,-30000,700),FVector(-15000,30000,700)};
				auto Node=[&D](double X,double Y){const FVector P(X,Y,0);for(int32 I=0;I<D.RouteNodes.Num();++I)if(D.RouteNodes[I].Equals(P,1))return I;return D.RouteNodes.Add(P);};
				auto Edge=[&](double X0,double Y0,double X1,double Y1,ERouteLane Lane){FRouteEdge E;E.A=Node(X0,Y0);E.B=Node(X1,Y1);E.Lane=Lane;D.RouteEdges.Add(E);};
				const double AvenueX[]={-42000,-25000,-17500,0,17500,25000,42000};
				for(int32 I=0;I<6;++I)Edge(AvenueX[I],0,AvenueX[I+1],0,ERouteLane::Centre);
				const double LaneX[]={-40000,-25000,-17500,0,17500,25000,40000};
				for(int32 I=0;I<6;++I){Edge(LaneX[I],25000,LaneX[I+1],25000,ERouteLane::North);Edge(LaneX[I],-25000,LaneX[I+1],-25000,ERouteLane::South);}
				// Lanes join the valley only through the passes (x = +-17500) and beside the homes (x = +-40000).
				for(double X:{-40000.,-17500.,17500.,40000.}){Edge(X,25000,X,0,ERouteLane::Connector);Edge(X,0,X,-25000,ERouteLane::Connector);}
				Edge(-42000,0,-40000,0,ERouteLane::Centre);Edge(40000,0,42000,0,ERouteLane::Centre);
				D.Heights=BattleMapJ::MakeHeights(D.HalfExtent);
				T.Add(D);
			}
			// K (2026-11-22, Docs/design-map-k.md): a dense small town (600m x 400m, columns 4..15, rows 3..10 of the grid of I and J) ringed
			// by mountains, for close fights that come from the ground. No field, wood, hill or water; six objectives; the homes of J.
			{
				FMapDef D=Urban;D.Name=TEXT("Lvl_BattleWide_K");
				D.bSectors=true;
				D.SectorKind=[](FIntPoint C)->char
				{
					switch(BattleMapK::Kind(C.X,C.Y))
					{
					case TEXT('P'):case TEXT('O'):case TEXT('Y'):return 'J';
					case TEXT('W'):case TEXT('K'):return 'W';
					case TEXT('H'):case TEXT('B'):case TEXT('A'):case TEXT('C'):case TEXT('Q'):case TEXT('U'):return 'H';
					case TEXT('G'):return 'T';case TEXT('M'):return 'M';default:return 'F';
					}
				};
				D.Objectives=BattleMapK::Objectives();
				D.ObjectiveOwners={-1,-1,-1,-1,-1,-1};
				D.ObjectiveValues={1,1,1,1,1,1};
				D.PointIncomeScale=.7f;
				D.CityHalf=FVector2D(30000,20000);
				// Lookouts: the parks at the north-east and south-west corners of the town.
				D.Lookouts={FVector(27500,17500,0),FVector(-27500,-17500,0)};
				auto Node=[&D](double X,double Y){const FVector P(X,Y,0);for(int32 I=0;I<D.RouteNodes.Num();++I)if(D.RouteNodes[I].Equals(P,1))return I;return D.RouteNodes.Add(P);};
				auto Edge=[&](double X0,double Y0,double X1,double Y1,ERouteLane Lane){FRouteEdge E;E.A=Node(X0,Y0);E.B=Node(X1,Y1);E.Lane=Lane;D.RouteEdges.Add(E);};
				// Routes follow the roads of BattleMapK::Roads(). Centre: the avenue with its two turns.
				const double Avenue[][2]={{-42000,0},{-35000,0},{-35000,-5000},{-30000,-5000},{-10000,-5000},{-10000,0},{10000,0},{10000,5000},{30000,5000},{35000,5000},{35000,0},{42000,0}};
				for(int32 I=0;I+1<UE_ARRAY_COUNT(Avenue);++I)Edge(Avenue[I][0],Avenue[I][1],Avenue[I+1][0],Avenue[I+1][1],ERouteLane::Centre);
				const double North[][2]={{-30000,15000},{-10000,15000},{-10000,10000},{10000,10000},{10000,15000},{30000,15000}};
				for(int32 I=0;I+1<UE_ARRAY_COUNT(North);++I)
				{
					Edge(North[I][0],North[I][1],North[I+1][0],North[I+1][1],ERouteLane::North);
					Edge(-North[I][0],-North[I][1],-North[I+1][0],-North[I+1][1],ERouteLane::South);
				}
				// Connectors: beside the town's ends (x = +-300m) and through its middle (x = +-100m).
				Edge(30000,15000,30000,5000,ERouteLane::Connector);Edge(30000,5000,30000,-15000,ERouteLane::Connector);
				Edge(-30000,-15000,-30000,-5000,ERouteLane::Connector);Edge(-30000,-5000,-30000,15000,ERouteLane::Connector);
				Edge(10000,10000,10000,5000,ERouteLane::Connector);Edge(10000,0,10000,-10000,ERouteLane::Connector);
				Edge(-10000,-10000,-10000,-5000,ERouteLane::Connector);Edge(-10000,0,-10000,10000,ERouteLane::Connector);
				D.Heights=BattleMapK::MakeHeights(D.HalfExtent);
				T.Add(D);
			}
            {
                FMapDef D=Urban; D.Name=TEXT("Lvl_BattleWide_H"); D.bSectors=true; D.SectorKind=&BattleSectors::Kind;
                D.PointIncomeScale=.5f; D.Lookouts.Reset(); D.CityHalf=FVector2D::ZeroVector;
                for(FIntPoint Cell : {FIntPoint(3,11),FIntPoint(6,9),FIntPoint(8,7),FIntPoint(9,2),
                                      FIntPoint(10,11),FIntPoint(11,6),FIntPoint(13,4),FIntPoint(16,2)})
                    D.Objectives.Add(BattleSectors::Center(Cell));
                D.ObjectiveOwners={0,-1,-1,-1,-1,-1,-1,1};
                D.ObjectiveValues={2,1,1,1,1,1,1,2};
                // The main avenue connects both homes. Farm and depot roads reach the two outer bridges.
                auto Road=[&](ERouteLane Lane,std::initializer_list<FIntPoint> Junctions){
                    int Previous=INDEX_NONE;
                    for(FIntPoint At:Junctions){
                        const int Current=D.RouteNodes.AddUnique(FVector(100*At.X,100*At.Y,0));
                        if(Previous!=INDEX_NONE)D.RouteEdges.Add({Previous,Current,Lane});
                        Previous=Current;
                    }
                };
#include "BattleSectorRoads.inl"
                for(const auto& House:BattleSectors::Houses())BattleTerrain::AddHouse(D.Heights,House);
                for(const auto& Hill:BattleSectors::Hills())BattleTerrain::AddHill(D.Heights,Hill);
                const int32 East=D.Heights.Patches.Num();
                for(int32 I=0;I<East;++I){auto Patch=D.Heights.Patches[I].Rotated();D.Heights.Patches.Add(Patch);}
                D.Heights.Build(D.HalfExtent);T.Add(MoveTemp(D));
            }
            return T;
		}();
		return Table;
	}
	// Definition of a map by name (a world's map name may carry a prefix, so the end of the name is compared). Unknown maps get the
	// arena definition.
	inline const FMapDef& ByName(const FString& Map)
	{
		for(const FMapDef& D:Maps())if(Map.EndsWith(D.Name))return D;
		return Maps()[0];
	}
	// True for the maps a mixed save may name (exact name).
	inline bool IsMixedSaveMap(const FString& Map)
	{
		for(const FMapDef& D:Maps())if(D.Kind!=EMapKind::Arena&&Map==D.Name)return true;
		return false;
	}
	inline const FMapDef& Of(const UWorld* World)
	{
		if(!World)return Maps()[0];
		// Asked many times per frame (also from worker threads): remember the answer for the last world.
		thread_local TWeakObjectPtr<const UWorld> Last;thread_local const FMapDef* Def=nullptr;
		if(!Def||Last!=World){Def=&ByName(World->GetMapName());Last=World;}
		return *Def;
	}

	// Kind of a sector of the world's map (see FMapDef::SectorKind); 'F' where the map has no sectors or the cell is outside.
	inline char SectorKind(const UWorld* World,FIntPoint Cell) {const FMapDef& D=Of(World);return D.SectorKind&&BattleSectors::Valid(Cell)?D.SectorKind(Cell):'F';}
	inline bool IsExpanded(const UWorld* World) {return Of(World).bExpanded;}
	inline bool IsUrbanMap(const FString& Map) {return ByName(Map).bUrban;}
	inline bool IsUrban(const UWorld* World) {return Of(World).bUrban;}
	// 2026-11-22 (user decision, roadmap stage 1): the sector AI combination -BattleSectorContact/-Route/-Flank/-Front/-MoveFire/
	// -CoverRoute, -BattleCoverHuddle, -BattlePinnedFlank and -BattleArrivalThreat is on by default on maps with bSectorCombo
	// (map I) and off elsewhere. The switches are read without a world, so the leader runtime records the map's default when it
	// starts on a world (UBattleLeaderRuntime::Start). An explicit -Battle<Name>=0/1 still wins on every map.
	inline bool& SectorComboDefault() {static bool bOn=false;return bOn;}
	inline bool SectorComboSwitch(int32 Arg) {return Arg>=0?Arg!=0:SectorComboDefault();}
	inline bool IsSightField(const UWorld* World) {return Of(World).Kind==EMapKind::Wide;}
	inline bool IsLarge(const UWorld* World) {return Of(World).Kind!=EMapKind::Arena;}
	// Objectives of the wide maps by map name (also used to validate saves). Other names get the three objectives of map A.
	inline int32 ObjectiveCountForMap(const FString& Map) {const FMapDef& D=ByName(Map);return D.Kind==EMapKind::Wide?D.Objectives.Num():3;}
	inline FVector ObjectiveForMap(const FString& Map,int32 Index)
	{
		const FMapDef& D=ByName(Map);
		return D.Kind==EMapKind::Wide?D.Objectives[FMath::Clamp(Index,0,D.Objectives.Num()-1)]:Objective(Index);
	}
	// Starting owner and worth of an objective (see FMapDef); -1 and 1 where the map sets none.
	inline int32 ObjectiveOwnerSide(const FMapDef& D,int32 Index) {return D.ObjectiveOwners.IsValidIndex(Index)?D.ObjectiveOwners[Index]:INDEX_NONE;}
	inline float ObjectiveValue(const FMapDef& D,int32 Index) {return D.ObjectiveValues.IsValidIndex(Index)?D.ObjectiveValues[Index]:1.f;}
	inline FVector WideSpawnFor(const UWorld* World,int32 Team,int32 Group)
	{
		const FMapDef& D=Of(World);
		return FVector((Team?1:-1)*(D.SpawnX+(Group%5)*450),(Group/5-2)*3500,100)*D.Scale;
	}
	inline TArray<FVector> Lookouts(const UWorld* World) {return Of(World).Lookouts;}
	inline FVector2D FieldHalfExtent(const UWorld* World) {return Of(World).HalfExtent;}
	inline float HomeDistance(const UWorld* World) {return Of(World).HomeDistance;}
	inline FVector Spawn(int32 Team, int32 Index)
	{
		const int32 Group=Index/5;
		return FVector((Team ? 1 : -1)*(9000+Group%5*400), (Group/5-4.5)*1400+(Index%5-2)*100, 100);
	}
	inline FVector Goal(int32 Index)
	{
		const int32 Group=Index/5;
		return FVector(500+(Group/3/4)*180,(Group%3-1)*4500+(Group/3%4-1.5)*600+(Index%5-2)*100,0);
	}
}
