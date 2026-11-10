#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"

namespace BattleFieldLayout
{
	inline bool IsLarge(const UWorld* World) { return World && World->GetMapName().EndsWith(TEXT("Lvl_BattleField500")); }
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
