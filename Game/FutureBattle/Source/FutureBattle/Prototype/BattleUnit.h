#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BattleState.h"
#include "CollisionQueryParams.h"
#include "BattleCoverClips.h"
#include "BattleUnit.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UAnimSequence;
class UStaticMesh;
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
	// Display only: where a tracer starts. The muzzle of a visible weapon model, else a point ahead of the eye the shot is traced from.
	static FVector TracerStart(const UStaticMeshComponent* Model, const FVector& Eye, const FVector& Direction);
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
	void SetSprinting(bool bRequested);
	void ReloadIfEmpty();
	void SetAnimationDetail(bool bDirect);
	void SetRevealed(bool bVisible);
	void ApplyDeath();
	// Corpse removal (display only): hides the body for good; SetRevealed no longer shows it.
	void HideCorpse(bool bHideActor=true,bool bStopMeshTick=true);
	FVector EyePosition() const;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rifle;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> TeamLabel;
	UPROPERTY() TObjectPtr<AAIController> SavedAI;
	// Direct control view (display only; the shot's spread and range do not change):
	// 0 none, 1 aim over the shoulder (x1.6), 2 scope x4, 3 scope x8. Set every frame by the controller.
	void SetAimLevel(int32 Level) { AimLevel = FMath::Clamp(Level, 0, 3); }
	int32 GetAimLevel() const { return AimLevel; }
	float AimMagnification() const { return AimZoom; }
	bool ScopeView() const { return ScopeAlpha > .5f; }
	// Spread multiplier for the player's own shot (UBattleTuning): below 1 once the view has settled on an aim level.
	float AimSpreadScale() const;
	// The whole multiplier of a possessed soldier's shot: UBattleTuning::PlayerSpreadScale x AimSpreadScale(). 1 for everybody else.
	float PlayerSpreadScale() const;
	bool bRevealed = true;
	// Stance shown by the body (set by UBattleLeaderRuntime::SetStance); with -BattleStanceAnim=1 a crouch plays Lyra's clip.
	uint8 VisualStance = 0;
	// A squad type's own look (the exoskeleton soldier's AC01 suit): its own clips and its own weapon model held at a bone
	// (Grip = the model relative to that bone). There is no reload clip: the pose stays. Set by UBattleLeaderRuntime::ApplySquadLook.
	void SetLook(UAnimSequence* Idle, UAnimSequence* Move, UAnimSequence* CrouchIdle, UAnimSequence* CrouchMove, UStaticMesh* Weapon, FName Bone, const FTransform& Grip);
	bool HasLook() const { return LookIdle != nullptr; }
	UAnimSequence* GetLookMove() const { return LookMove; }
	// Look check shots (BattleExoShot.cpp): the body tilt drawn as if moving at this local velocity (zero = the real one).
	void SetLookShotVelocity(const FVector& Local) { LookShotVelocity = Local; LookVelShown = Local; UpdateLookTilt(true); }
	// The look's mesh shares the mannequin skeleton (the AC01 suit does), so the shared prone clips (BattleBodyAnim) play on it
	// at full size instead of the squashed body.
	bool LookHasProneClip() const;
	// Recoil display of the body (BattleBodyAnim): set by the shared fire path for every shot of this soldier.
	void NoteVisualShot(double Time) { LastVisualShot = Time; }
	// The AI's current aim (BattleLeaderRuntime, every step) for the body display: WeaponAimDirection changes only on a large
	// turn (UBattleState::UpdateAim, the aim-switch delay), so the body would point the rifle at a stale pitch.
	void NoteDisplayAim(const FVector& Direction, double Time) { DisplayAim = Direction; DisplayAimTime = Time; }
private:
	friend class UBattleLeaderRuntime; // the cover smoke drives the body display (BattleCoverActionSmoke.cpp)
	// Body animation (BattleBodyAnim.h): lower body by stance, direction and transition, upper-body layers and overlays.
	void UpdateBodyAnim(class UBattleBodyAnimInstance& Body, const FBattleUnitState& S);
	class UBattleBodyAnimInstance* BodyAnim() const;
	uint8 BodyStance = 0;
	UPROPERTY() TObjectPtr<UAnimSequence> TransitionClip;
	double TransitionUntil = 0., ShownShot = -10.;
	float ShownHP = -1.f;
	// Cover clips (BattleCoverClips.h): the set picked when the cover action starts, the one-shot between phases, mesh yaw.
	uint8 ShownCoverPhase = 0;
	BattleCoverClips::FSet CoverClipSet{};
	UPROPERTY() TObjectPtr<UAnimSequence> CoverOneShot;
	double CoverOneShotUntil = 0.;
	float CoverOneShotYaw = 0.f, MeshYawShown = 0.f;
	void SetMeshYaw(float Yaw);
	// Blind fire around an edge (BattleBlindFire::SideLeanBack): the body is drawn leaning out to the edge while that part of
	// it shows; the offset of the mesh from its place in the capsule, eased in and out (display only).
	FVector MeshLeanShown = FVector::ZeroVector;
	void SetMeshLean(const FVector& WorldOffset, float DeltaSeconds);
	// A soldier with its own look (the exo suit's glide) tilts its whole body from the feet toward where it moves: the eased
	// velocity sets the lean, the difference to the real one adds a little more when starting and a little back when braking.
	FVector LookVelShown = FVector::ZeroVector, LookShotVelocity = FVector::ZeroVector;
	FQuat MeshTiltShown = FQuat::Identity;
	void UpdateLookTilt(bool bUpright);
	UPROPERTY() TObjectPtr<UAnimSequence> LookIdle;
	UPROPERTY() TObjectPtr<UAnimSequence> LookMove;
	UPROPERTY() TObjectPtr<UAnimSequence> LookCrouchIdle;
	UPROPERTY() TObjectPtr<UAnimSequence> LookCrouchMove;
	UPROPERTY() TObjectPtr<UStaticMesh> LookWeapon;
	FName LookBone;
	FTransform LookGrip;
	UPROPERTY() int32 UnitID = INDEX_NONE;
	bool bDeathApplied = false;
	// Corpse fall (BattleRagdoll): 0 until the death clip ends at RagdollAt, 1 falling until RagdollAt, 2 done.
	uint8 RagdollStage = 2;
	double RagdollAt = 0.;
	bool bCorpseHidden = false;
	bool bThirdPersonView = true;
	bool bDirectAvoidanceOverride = false;
	float WalkSpeedBeforeSprint = 0.f;
	float SavedAvoidanceWeight = .5f;
	bool bWeaponVisuals = true, bTrialPose = false;
	UPROPERTY() TObjectPtr<UStaticMesh> CarbineModel;
	UPROPERTY() TObjectPtr<UStaticMesh> SupportModel;
	UPROPERTY() TObjectPtr<class UAnimSequence> RifleIdle;
	UPROPERTY() TObjectPtr<UAnimSequence> RifleWalk;
	UPROPERTY() TObjectPtr<UAnimSequence> RifleReload;
	UPROPERTY() TObjectPtr<UAnimSequence> CurrentPose;
	double LastVisualShot = -10.;
	float ShownAimPitch = 0.f;
	double LastHitReact = -10.;
	FVector DisplayAim = FVector::ZeroVector;
	double DisplayAimTime = -10.;
	int32 AimLevel = 0;
	float AimZoom = 1.f, ScopeAlpha = 0.f;
	bool bScopeHidden = false;
	// The rear camera pulled in by a wall to within the body (CalcCamera): the body is hidden like through a scope, rather than
	// showing its legs with the camera inside the chest.
	bool bCameraInBody = false;
	void UpdateAimView();
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
