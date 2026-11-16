#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"

namespace BattleFieldLayout
{
	inline bool IsExpanded(const UWorld* World) {return World && World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_D"));}
	inline FVector Expand(FVector P,bool Expanded) {return Expanded?P*FVector(4./3.,1.5,1):P;}
	// Map E (2026-11-14): 1000m x 700m, urban centre with five objectives and open flanks. Maps A-D keep three objectives.
	inline bool IsUrbanMap(const FString& Map) {return Map.EndsWith(TEXT("Lvl_BattleWide_E"));}
	inline bool IsUrban(const UWorld* World) {return World && IsUrbanMap(World->GetMapName());}
	inline bool IsSightField(const UWorld* World) { return World && (IsExpanded(World) || IsUrban(World) ||World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_A")) || World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_B")) || World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_C"))); }
	inline bool IsLarge(const UWorld* World) { return World && (IsSightField(World) || World->GetMapName().EndsWith(TEXT("Lvl_BattleField500"))); }
	inline FVector Objective(int32 Index,bool Expanded=false) { return Expand(Index==0?FVector(-7000,12000,0):Index==1?FVector::ZeroVector:FVector(7000,-12000,0),Expanded); }
	inline FVector WideSpawn(int32 Team,int32 Group,bool Expanded=false) { return Expand(FVector((Team?1:-1)*(25000+(Group%5)*450),(Group/5-2)*3500,100),Expanded); }
	// Objectives of the wide maps by map name (also used to validate saves). Index I and Count-1-I are 180-degree counterparts,
	// which the team order rotation relies on.
	inline int32 ObjectiveCountForMap(const FString& Map) {return IsUrbanMap(Map)?5:3;}
	inline FVector ObjectiveForMap(const FString& Map,int32 Index)
	{
		if(IsUrbanMap(Map))
		{
			static const FVector Urban[]={FVector(-13000,7500,0),FVector(-13000,-7500,0),FVector::ZeroVector,FVector(13000,7500,0),FVector(13000,-7500,0)};
			return Urban[FMath::Clamp(Index,0,4)];
		}
		return Objective(Index,Map.EndsWith(TEXT("Lvl_BattleWide_D")));
	}
	inline FVector WideSpawnFor(const UWorld* World,int32 Team,int32 Group)
	{
		if(IsUrban(World))return FVector((Team?1:-1)*(39000+(Group%5)*450),(Group/5-2)*3500,100);
		return WideSpawn(Team,Group,IsExpanded(World));
	}
	// Lookouts (ROADMAP 2026-11-14): public places where a still soldier sees far (UBattleTuning::LookoutSightRange). Map E: the city
	// edges overlooking the open flanks (0, +-165 m); maps A-D: the field edges beside the centre (0, +-170 m).
	inline TArray<FVector> Lookouts(const UWorld* World)
	{
		if(IsUrban(World))return {FVector(0,16500,0),FVector(0,-16500,0)};
		if(IsSightField(World))return {Expand(FVector(0,17000,0),IsExpanded(World)),Expand(FVector(0,-17000,0),IsExpanded(World))};
		return {};
	}
	// Half size of the playable floor (X, Y) in cm; used by the minimap.
	inline FVector2D FieldHalfExtent(const UWorld* World) {return IsUrban(World)?FVector2D(50000,35000):IsExpanded(World)?FVector2D(40000,30000):IsSightField(World)?FVector2D(30000,20000):IsLarge(World)?FVector2D(12000,9000):FVector2D(4000,3000);}
	inline float HomeDistance(const UWorld* World) {return IsUrban(World)?42000:IsExpanded(World)?36000:IsSightField(World)?27000:10000;}
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
