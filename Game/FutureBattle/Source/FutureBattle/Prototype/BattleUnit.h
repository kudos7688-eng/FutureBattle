#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BattleState.h"
#include "CollisionQueryParams.h"
#include "BattleUnit.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FBattleContact;
class UBattleMetrics;

// A synchronous sensor-query snapshot. Never retain it across world ticks.
struct FBattleSightContext
{
	FVector Position, Eye, Forward;
	float RangeSquared = 0, HalfAngleCos = 1;
	bool bAlive = false, bOmnidirectional = false;
	UBattleMetrics* Metrics = nullptr;
	FCollisionQueryParams Query;
};

UCLASS()
class ABattleUnit : public ACharacter
{
	GENERATED_BODY()
public:
	ABattleUnit(const FObjectInitializer& Init = FObjectInitializer::Get());
	void InitializeUnit(int32 ID);
	void MountWeapon(UStaticMeshComponent* Model, USceneComponent* Parent, EBattleWeapon Weapon, bool bFirstPerson) const;
	bool HasWeaponVisuals() const { return bWeaponVisuals; }
	virtual void Tick(float DeltaTime) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	bool UsesThirdPersonView() const { return bThirdPersonView; }
	void GetPlayerShot(FVector& Start, FVector& Direction);
	bool FireFromPlayerView();
	int32 GetUnitID() const { return UnitID; }
	const FBattleUnitState* GetState() const;
	bool IsAlive() const;
	bool CanSee(const ABattleUnit* Other) const;
	FBattleSightContext CaptureSightContext() const;
	bool CanSeeSnapshot(const FBattleSightContext& Context, const ABattleUnit* Other, const FVector& Position, bool bAlive) const;
	bool FireAt(FVector Start, FVector Direction, bool bPlayerShot);
	void SetDirectControl(bool bDirect);
	void SetAnimationDetail(bool bDirect);
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
	bool bThirdPersonView = true;
	bool bDirectAvoidanceOverride = false;
	float SavedAvoidanceWeight = .5f;
	bool bWeaponVisuals = true, bTrialPose = false;
	UPROPERTY() TObjectPtr<UStaticMesh> CarbineModel;
	UPROPERTY() TObjectPtr<UStaticMesh> SupportModel;
	UPROPERTY() TObjectPtr<class UAnimSequence> RifleIdle;
	UPROPERTY() TObjectPtr<UAnimSequence> RifleWalk;
	UPROPERTY() TObjectPtr<UAnimSequence> RifleReload;
	UPROPERTY() TObjectPtr<UAnimSequence> CurrentPose;
	double LastVisualShot = -10.;
	void UpdateWeaponPose(const FBattleUnitState& S);
};

UCLASS()
class ABattleAIController : public AAIController
{
	GENERATED_BODY()
public:
	ABattleAIController();
	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	// Also used for explicit orders while AI decisions are paused for practice.
	void BeginOrder();
	void SuspendMovement();
	void PrepareControlHandoff();
private:
	double NextDecision = 0.;
	double NextMoveRequest = 0.;
	double LastProgressTime = 0.;
	FVector LastProgressPosition = FVector::ZeroVector;
	FVector ExecutionGoal = FVector::ZeroVector;
	int32 ActiveRevision = INDEX_NONE;
	int32 ActiveCoordinationRevision = INDEX_NONE;
	int32 ActiveDecisionRevision = INDEX_NONE;
	bool bTrackingMove = false;
	bool bRelaxed = false;
	bool bFailed = false;
	void ResetExecution();
	void ExecuteMove(ABattleUnit* Unit, FVector Goal, EBattleAction Action, double Now);
	void RetryMove(ABattleUnit* Unit, double Now, FVector Goal, FName Reason);
	bool TryTacticalAction(ABattleUnit* Unit, const FBattleContact* Threat, double Now, bool bMustReturn);
	void ClearCoverPlan();
	int32 CurrentCoverSlot = INDEX_NONE;
	double NextTacticalDecision = 0.;
	double TacticalCommitUntil = 0.;
	double TacticalBlockedUntil = 0.;
	bool bCoverForReload = false;
};
