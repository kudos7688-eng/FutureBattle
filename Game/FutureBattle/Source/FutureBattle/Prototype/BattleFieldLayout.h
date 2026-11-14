#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"

namespace BattleFieldLayout
{
	inline bool IsExpanded(const UWorld* World) {return World && World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_D"));}
	inline FVector Expand(FVector P,bool Expanded) {return Expanded?P*FVector(4./3.,1.5,1):P;}
	inline bool IsSightField(const UWorld* World) { return World && (IsExpanded(World) || World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_A")) || World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_B")) || World->GetMapName().EndsWith(TEXT("Lvl_BattleWide_C"))); }
	inline bool IsLarge(const UWorld* World) { return World && (IsSightField(World) || World->GetMapName().EndsWith(TEXT("Lvl_BattleField500"))); }
	inline FVector Objective(int32 Index,bool Expanded=false) { return Expand(Index==0?FVector(-7000,12000,0):Index==1?FVector::ZeroVector:FVector(7000,-12000,0),Expanded); }
	inline FVector WideSpawn(int32 Team,int32 Group,bool Expanded=false) { return Expand(FVector((Team?1:-1)*(25000+(Group%5)*450),(Group/5-2)*3500,100),Expanded); }
	inline float HomeDistance(const UWorld* World) {return IsExpanded(World)?36000:IsSightField(World)?27000:10000;}
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
