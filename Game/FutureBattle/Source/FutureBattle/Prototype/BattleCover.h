#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleCover.generated.h"
class ABattleUnit;

USTRUCT()
struct FBattleCoverReservation
{
	GENERATED_BODY()
	UPROPERTY() int32 UnitID = INDEX_NONE;
	UPROPERTY() double ExpiresAt = 0.;
};

UCLASS()
class UBattleCover : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void BuildSlots(bool bForce = false);
	// Evaluation only (-BattleSymmetryAudit): compares the cover slots, the navigation mesh and lines of sight with their 180-degree
	// counterparts and logs BATTLE_SYMMETRY lines (never read by the AI).
	void AuditSymmetry() const;
	const TArray<FVector>& GetSlots() const { return Slots; }
	TArray<int32> NearbySlots(FVector Origin, float Radius, int32 Limit) const;
	bool IsAvailable(int32 Slot, int32 UnitID, double Now) const;
	bool Reserve(int32 Slot, int32 UnitID, double Now);
	void Release(int32 UnitID);
	void ResetReservations() { Reservations.Reset(); }
	int32 Owner(int32 Slot, double Now) const;
private:
	bool bBuilt = false;
	UPROPERTY() TArray<FVector> Slots;
	UPROPERTY() TMap<int32, FBattleCoverReservation> Reservations;
};
