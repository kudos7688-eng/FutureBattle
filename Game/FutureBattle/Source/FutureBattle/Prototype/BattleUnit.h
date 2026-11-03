#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BattleState.h"
#include "BattleUnit.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class ABattleUnit : public ACharacter
{
	GENERATED_BODY()
public:
	ABattleUnit();
	void InitializeUnit(int32 ID);
	virtual void Tick(float DeltaTime) override;
	int32 GetUnitID() const { return UnitID; }
	const FBattleUnitState* GetState() const;
	bool IsAlive() const;
	bool CanSee(const ABattleUnit* Other) const;
	bool FireAt(FVector Start, FVector Direction, bool bPlayerShot);
	void SetDirectControl(bool bDirect);
	void SetRevealed(bool bVisible);
	void ApplyDeath();
	FVector EyePosition() const;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rifle;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> TeamLabel;
	UPROPERTY() TObjectPtr<AAIController> SavedAI;
	bool bRevealed = true;
private:
	UPROPERTY() int32 UnitID = INDEX_NONE;
	bool bDeathApplied = false;
};

UCLASS()
class ABattleAIController : public AAIController
{
	GENERATED_BODY()
public:
	ABattleAIController();
	virtual void Tick(float DeltaTime) override;
private:
	double NextDecision = 0.;
	double NextMoveRequest = 0.;
};
