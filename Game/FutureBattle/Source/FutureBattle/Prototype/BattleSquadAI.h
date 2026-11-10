#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleSquadAI.generated.h"
class ABattleUnit;

struct FBattleSquadPlan
{
	int32 OrderRevision = INDEX_NONE;
	int32 TargetID = INDEX_NONE;
	FVector ThreatAnchor = FVector::ZeroVector;
	FVector FormationAnchor = FVector::ZeroVector;
	int32 Waves = 0;
	double WaveStarted = 0.;
	double NextWaveAt = 0.;
	double TargetCommittedUntil = 0.;
	TSet<int32> Moved;
};

// Friendly roles are coordinated here; enemy knowledge stays in each observer's contacts.
UCLASS()
class UBattleSquadAI : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void Update(const TArray<TObjectPtr<ABattleUnit>>& Units, double Now);
 void UpdateMixed(class UBattlePhase3* Phase3,double Now);
	void ReleaseUnit(int32 UnitID);
	void ReportFailure(int32 UnitID, double Now);
	void Reset();
private:
	TMap<int32, FBattleSquadPlan> Plans;
	TMap<int32, double> BlockedUntil;
	double NextUpdate = 0.;
};
