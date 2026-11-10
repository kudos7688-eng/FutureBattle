#pragma once
#include "CoreMinimal.h"
struct FBattleUnitState;
class ABattleUnit;

struct FBattleExposure
{
	int32 VisibleSamples = 0;
	int32 TotalSamples = 5;
	bool bFlanked = false;
	float Fraction() const { return static_cast<float>(VisibleSamples) / TotalSamples; }
	bool HasCover() const { return VisibleSamples < TotalSamples; }
};

namespace BattleTactics
{
	float SpreadDegrees(const FBattleUnitState& State, double Now);
	FVector ShotDirection(const FBattleUnitState& State, FVector Aim, double Now);
	FBattleExposure ExposureFrom(const ABattleUnit* Target, FVector AttackOrigin);
	FBattleExposure ExposureAt(UWorld* World, FVector Position, FVector Forward, FVector AttackOrigin);
	bool HasTerrainLine(UWorld* World, FVector From, FVector To);
}
