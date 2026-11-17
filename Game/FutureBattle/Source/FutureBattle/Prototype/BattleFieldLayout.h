#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"

namespace BattleFieldLayout
{
	inline FVector Expand(FVector P,bool Expanded) {return Expanded?P*FVector(4./3.,1.5,1):P;}
	inline FVector Objective(int32 Index,bool Expanded=false) { return Expand(Index==0?FVector(-7000,12000,0):Index==1?FVector::ZeroVector:FVector(7000,-12000,0),Expanded); }

	// Map definitions (2026-11-18). Everything that differs by map lives in one table entry: to add a map, add an entry to Maps()
	// and a generator branch in BattleMapCommandlet. Code asks the definition (Of(World) / ByName(Map)) instead of testing map names.
	// Arena = the 80m x 60m prototype arena and any other level; Field500 = the fixed 500-soldier field; Wide = the area-battle maps.
	enum class EMapKind : uint8 { Arena, Field500, Wide };
	struct FMapDef
	{
		const TCHAR* Name=TEXT("");
		EMapKind Kind=EMapKind::Arena;
		// Map D: map A scaled by Scale, with the extra objective cover its tests and audit rely on.
		bool bExpanded=false;
		// Rule set introduced with map E: range profile, stances, cover-first arrival, partial paths, own re-paths, denser cover slots,
		// the larger income. Independent of the geometry below, so a new map can take it without being map E.
		bool bUrban=false;
		FVector Scale=FVector::OneVector;
		// Half size of the playable floor (X, Y) in cm.
		FVector2D HalfExtent=FVector2D(4000,3000);
		float HomeDistance=10000;
		// X of the first starting column of the wide maps.
		double SpawnX=25000;
		// Objectives of the wide maps. Index I and Num-1-I are 180-degree counterparts, which the team order rotation relies on.
		TArray<FVector> Objectives;
		// Lookouts (ROADMAP 2026-11-14): public places where a still soldier sees far (UBattleTuning::LookoutSightRange).
		TArray<FVector> Lookouts;
		// Half size of the built-up centre (slow for vehicles, framed on the minimap); zero = none.
		FVector2D CityHalf=FVector2D::ZeroVector;
		float MatchSeconds=180;
		float CameraMaxHeight=4500;
		// Largest |X|, |Y| a saved position may have.
		FVector2D SaveLimit=FVector2D(32000,22000);
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
			{
				FMapDef D;D.Name=TEXT("Lvl_BattleWide_E");D.Kind=EMapKind::Wide;D.bUrban=true;
				D.HalfExtent=FVector2D(50000,35000);D.HomeDistance=42000;D.SpawnX=39000;
				D.Objectives={FVector(-13000,7500,0),FVector(-13000,-7500,0),FVector::ZeroVector,FVector(13000,7500,0),FVector(13000,-7500,0)};
				D.Lookouts={FVector(0,16500,0),FVector(0,-16500,0)};
				D.CityHalf=FVector2D(25000,15000);D.MatchSeconds=900;D.CameraMaxHeight=160000;D.SaveLimit=FVector2D(52000,37000);T.Add(D);
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

	inline bool IsExpanded(const UWorld* World) {return Of(World).bExpanded;}
	inline bool IsUrbanMap(const FString& Map) {return ByName(Map).bUrban;}
	inline bool IsUrban(const UWorld* World) {return Of(World).bUrban;}
	inline bool IsSightField(const UWorld* World) {return Of(World).Kind==EMapKind::Wide;}
	inline bool IsLarge(const UWorld* World) {return Of(World).Kind!=EMapKind::Arena;}
	// Objectives of the wide maps by map name (also used to validate saves). Other names get the three objectives of map A.
	inline int32 ObjectiveCountForMap(const FString& Map) {const FMapDef& D=ByName(Map);return D.Kind==EMapKind::Wide?D.Objectives.Num():3;}
	inline FVector ObjectiveForMap(const FString& Map,int32 Index)
	{
		const FMapDef& D=ByName(Map);
		return D.Kind==EMapKind::Wide?D.Objectives[FMath::Clamp(Index,0,D.Objectives.Num()-1)]:Objective(Index);
	}
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
