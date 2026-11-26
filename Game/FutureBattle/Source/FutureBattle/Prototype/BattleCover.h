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
	// Lazy, geometry-only index used exclusively by known-target movement (mode 2).
	TArray<FBox> KnownMoveGeometry(FVector Origin,double Radius,bool Rebuild=false);
	// Evaluation only (-BattleSymmetryAudit): compares the cover slots, the navigation mesh and lines of sight with their 180-degree
	// counterparts and logs BATTLE_SYMMETRY lines (never read by the AI).
	void AuditSymmetry() const;
	// Builds the slots while the match waits to start, once navigation has finished building (same slots as a later
	// lazy build: soldiers are ignored and only the map and the navmesh decide them). Moves ~100 ms out of the first step.
	void PrebuildSlots();
	bool IsBuilt() const { return bBuilt; }
	const TArray<FVector>& GetSlots() const { return Slots; }
	TArray<int32> NearbySlots(FVector Origin, float Radius, int32 Limit) const;
	bool IsAvailable(int32 Slot, int32 UnitID, double Now) const;
	bool Reserve(int32 Slot, int32 UnitID, double Now);
	void Release(int32 UnitID);
	void ResetReservations() { Reservations.Reset(); UnitToSlot.Reset(); }
	int32 Owner(int32 Slot, double Now) const;
private:
	bool bKnownGeometryBuilt=false;
	TArray<FBox> KnownGeometryBoxes;
	TMap<FIntPoint,TArray<int32>> KnownGeometryCells;
	bool bBuilt = false;
	UPROPERTY() TArray<FVector> Slots;
	UPROPERTY() TMap<int32, FBattleCoverReservation> Reservations;
	// Derived from reservations made through Reserve.
	TMap<int32, int32> UnitToSlot;
	TMap<FIntPoint, TArray<int32>> SlotCells;
	TArray<TArray<int32>> ConflictSlots;
	bool bSlotIndexValid = false;
	void BuildSlotIndex();
};
