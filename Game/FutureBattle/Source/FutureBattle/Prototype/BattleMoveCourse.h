#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleMoveCourse.generated.h"

class ABattleUnit;
class ABattleGameMode;
class FJsonObject;

// Benchmark-only parallel lanes on the flat deployment strips of the existing map.
UCLASS()
class UBattleMoveCourse : public UObject
{
	GENERATED_BODY()
public:
	static FVector SpawnPosition(int32 Team, int32 Index, int32 PerTeam);
	static void BuildGates(UWorld& World);
	void Start(ABattleGameMode& Mode, bool bUseGate);
	void Update(ABattleGameMode& Mode, float Dt);
	void Report(FJsonObject& Out) const;
	float Seconds() const { return Elapsed; }
	bool HasMoved() const { return Arrived > 0 || Crossed > 0 || bResumed; }
private:
	struct FRunner
	{
		FVector Goal, Approach, Exit, Previous;
		int32 Stage = 0, Retries = 0;
		float NextRetry = 0;
		double DistanceLeft = 0;
		bool bCrossed = false;
	};
	TArray<FRunner> Runners;
	bool bGate = false, bFPS = false, bReturned = false, bHandoffOK = true, bResumed = false;
	int32 Rejected = 0, Arrived = 0, Crossed = 0, RetryCount = 0;
	float Elapsed = 0, ReturnAt = 0;
	FVector ReturnPosition, LastProbePosition, TacticalCameraPosition;
	double ProbeSpeedBeforeSwitch = 0., ProbeFacingError = 0.;
	int32 ProbeIndex = 0;
	bool bPlayerMoved = false;
	double MaxSwitchJump = 0;
	void Issue(ABattleUnit& Unit, const FRunner& Runner);
};
