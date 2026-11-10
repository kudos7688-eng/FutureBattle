#pragma once

#include "BattleUnit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MassEntityQuery.h"
#include "BattleMassTrial.generated.h"

// Flat-ground benchmark adapter. Navigation, RVO and swept capsule collisions
// remain active; the player always uses the normal CharacterMovement tick.
UCLASS()
class UBattleTrialMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	FVector PlanStep(float Dt);
	void ApplyStep(const FVector& Delta, float Dt);
};

UCLASS()
class ABattleMassTrialUnit : public ABattleUnit
{
	GENERATED_BODY()
public:
	ABattleMassTrialUnit(const FObjectInitializer& Init);
};

USTRUCT()
struct FBattleTrialMotion : public FMassFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<UBattleTrialMovement> Movement;
	FVector Delta = FVector::ZeroVector;
	bool bManaged = false;
};

UCLASS()
class UBattleMassTrial : public UObject
{
	GENERATED_BODY()
public:
	void Start(UWorld& World, const TArray<TObjectPtr<ABattleUnit>>& Units);
	void Update(float Dt);
	void Stop();
	int32 ActivePeak = 0;
private:
	TSharedPtr<FMassEntityManager> Manager;
	TUniquePtr<FMassEntityQuery> Query;
	TArray<FMassEntityHandle> Entities;
};
