#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleState.generated.h"

UENUM(BlueprintType)
enum class EBattleWeapon : uint8 { Carbine, SupportGun };

inline EBattleWeapon WeaponForSlot(int32 Slot)
{
	return Slot % 5 == 4 ? EBattleWeapon::SupportGun : EBattleWeapon::Carbine;
}

UENUM(BlueprintType)
enum class EBattleOrder : uint8 { Hold, Move, AttackMove, Defend };

UENUM(BlueprintType)
enum class EBattleRole : uint8 { Independent, Support, Flank };

UENUM(BlueprintType)
enum class EBattleTraining : uint8 { Recruit, Regular, Veteran };

inline const TCHAR* BattleTrainingName(EBattleTraining Training)
{
	return Training == EBattleTraining::Recruit ? TEXT("RECRUIT") : Training == EBattleTraining::Veteran ? TEXT("VETERAN") : TEXT("REGULAR");
}

USTRUCT(BlueprintType)
struct FBattleDecisionProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float ThinkSeconds = .2f;
	UPROPERTY(EditAnywhere) float CoverThinkSeconds = 1.f;
	UPROPERTY(EditAnywhere) int32 CoverCandidates = 12;
	UPROPERTY(EditAnywhere) bool bCoordinate = true;
};

inline const TCHAR* BattleRoleName(EBattleRole Role)
{
	return Role == EBattleRole::Support ? TEXT("SUPPORT") : Role == EBattleRole::Flank ? TEXT("FLANK") : TEXT("INDEPENDENT");
}

UENUM(BlueprintType)
enum class EBattleAction : uint8 { Idle, Moving, Engaging, Returning, Regrouping, TakingCover, Flanking, Reloading, MoveFailed, DirectControl, Dead };

inline const TCHAR* BattleOrderName(EBattleOrder Order)
{
	switch (Order) {
	case EBattleOrder::Move: return TEXT("MOVE");
	case EBattleOrder::AttackMove: return TEXT("ATTACK MOVE");
	case EBattleOrder::Defend: return TEXT("DEFEND");
	default: return TEXT("HOLD"); }
}

inline const TCHAR* BattleActionName(EBattleAction Action)
{
	switch (Action) {
	case EBattleAction::Moving: return TEXT("MOVING");
	case EBattleAction::Engaging: return TEXT("ENGAGING");
	case EBattleAction::Returning: return TEXT("RETURNING");
	case EBattleAction::Regrouping: return TEXT("REGROUPING");
	case EBattleAction::TakingCover: return TEXT("TAKING COVER");
	case EBattleAction::Flanking: return TEXT("FLANKING");
	case EBattleAction::Reloading: return TEXT("RELOADING");
	case EBattleAction::MoveFailed: return TEXT("MOVE FAILED");
	case EBattleAction::DirectControl: return TEXT("DIRECT");
	case EBattleAction::Dead: return TEXT("DEAD");
	default: return TEXT("IDLE"); }
}

UENUM()
enum class EBattleCommandStatus : uint8 { Legacy, Active, Disabled };

DECLARE_MULTICAST_DELEGATE_OneParam(FBattleUnitDied, int32);

USTRUCT(BlueprintType)
struct FBattleSquadState
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere) int32 SquadID = INDEX_NONE;
	UPROPERTY() int32 LeaderUnitID = INDEX_NONE;
	UPROPERTY() EBattleCommandStatus CommandStatus = EBattleCommandStatus::Legacy;
	UPROPERTY() FString DisplayName;
	UPROPERTY() int32 CommandRevision = 0;
	UPROPERTY() FName StopReason;
	UPROPERTY(VisibleAnywhere) int32 Team = 0;
	UPROPERTY(VisibleAnywhere) TArray<int32> Members;
	UPROPERTY(VisibleAnywhere) EBattleOrder Order = EBattleOrder::Hold;
	UPROPERTY(VisibleAnywhere) FVector Destination = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) float DefenseRadius = 600.f;
	UPROPERTY(VisibleAnywhere) int32 OrderRevision = 0;
};

// Authoritative match data. Possession and Actor lifetime never create/reset this record.
USTRUCT(BlueprintType)
struct FBattlePerceptionProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SightRange = 2400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FieldOfView = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DetectionSeconds = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MemorySeconds = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShareRange = 1600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShareDelay = .8f;
};

USTRUCT(BlueprintType)
struct FBattleUnitState
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 UnitID = INDEX_NONE;
	// Frozen by CreateLeaderSquad; never changed by possession.
	UPROPERTY() bool bLeaderManaged = false;
	UPROPERTY() bool bCanPlayerControl = false;
	UPROPERTY() bool bActorRepresentation = true;
	UPROPERTY() FName UnitArchetypeID = TEXT("Regular");
	UPROPERTY() float LeaderAbilityScale = 1.f;
	UPROPERTY() FName DeathCause;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Team = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SquadID = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EBattleAction Action = EBattleAction::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HP = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EBattleWeapon Weapon = EBattleWeapon::Carbine;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Ammo = 30;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Reserve = 90;
	// Derived member order/formation goal; arrival never cancels the squad order.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EBattleOrder Order = EBattleOrder::Hold;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Destination = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) int32 OrderRevision = 0;
	UPROPERTY(VisibleAnywhere) int32 MoveRetries = 0;
	UPROPERTY(VisibleAnywhere) FBattlePerceptionProfile Perception;
	UPROPERTY(VisibleAnywhere) int32 PerceptionRevision = 0;
	UPROPERTY(VisibleAnywhere) double AimStableSince = -1.;
	UPROPERTY(VisibleAnywhere) FVector AimAnchor = FVector::ForwardVector;
	UPROPERTY(VisibleAnywhere) bool bMovingShot = false;
	UPROPERTY(VisibleAnywhere) float SpreadSkillScale = 1.f;
	UPROPERTY(VisibleAnywhere) int32 ShotSequence = 0;
	UPROPERTY(VisibleAnywhere) int32 CoverSlot = INDEX_NONE;
	UPROPERTY(VisibleAnywhere) FString DecisionReason;
	UPROPERTY(VisibleAnywhere) EBattleRole Role = EBattleRole::Independent;
	UPROPERTY(VisibleAnywhere) int32 CoordinatedTarget = INDEX_NONE;
	UPROPERTY(VisibleAnywhere) FVector TacticalDestination = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere) int32 CoordinationRevision = 0;
	UPROPERTY(VisibleAnywhere) EBattleTraining Training = EBattleTraining::Regular;
	UPROPERTY(VisibleAnywhere) FBattleDecisionProfile Decision;
	UPROPERTY(VisibleAnywhere) int32 DecisionRevision = 0;
	UPROPERTY(VisibleAnywhere) int32 HireCost = 180;
	UPROPERTY() double NextShotTime = 0.;
	UPROPERTY() double ReloadEndTime = 0.;
	bool IsAlive() const { return HP > 0.f; }
};

// Tune the prototype without recompiling via DefaultGame.ini.
UCLASS(Config=Game, DefaultConfig)
class UBattleTuning : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere) float Health = 100.f;
	UPROPERTY(Config, EditAnywhere) int32 Magazine = 30;
	UPROPERTY(Config, EditAnywhere) int32 ReserveAmmo = 90;
	UPROPERTY(Config, EditAnywhere) float ShotDamage = 10.f;
	UPROPERTY(Config, EditAnywhere) float PlayerShotInterval = 0.16f;
	UPROPERTY(Config, EditAnywhere) float AIShotInterval = 1.4f;
	UPROPERTY(Config, EditAnywhere) float ReloadSeconds = 1.8f;
	UPROPERTY(Config, EditAnywhere) int32 SupportMagazine = 60;
	UPROPERTY(Config, EditAnywhere) float SupportReloadSeconds = 3.2f;
	UPROPERTY(Config, EditAnywhere) float SupportPlayerInterval = .09f;
	UPROPERTY(Config, EditAnywhere) float SupportAIInterval = .45f;
	UPROPERTY(Config, EditAnywhere) float SupportMovingSpreadScale = 1.5f;
	int32 Capacity(EBattleWeapon W) const { return W == EBattleWeapon::SupportGun ? SupportMagazine : Magazine; }
	float ReloadTime(EBattleWeapon W) const { return W == EBattleWeapon::SupportGun ? SupportReloadSeconds : ReloadSeconds; }
	float ShotInterval(EBattleWeapon W, bool bPlayer) const
	{
		if (W == EBattleWeapon::SupportGun) return bPlayer ? SupportPlayerInterval : SupportAIInterval;
		return bPlayer ? PlayerShotInterval : AIShotInterval;
	}
	UPROPERTY(Config, EditAnywhere) float SightRange = 2400.f;
	UPROPERTY(Config, EditAnywhere) float WeaponRange = 5000.f;
	UPROPERTY(Config, EditAnywhere) float MoveSpeed = 420.f;
	UPROPERTY(Config, EditAnywhere) float DefenseRadius = 600.f;
	UPROPERTY(Config, EditAnywhere) float MoveStuckSeconds = 2.f;
	UPROPERTY(Config, EditAnywhere) int32 MaxMoveRetries = 3;
	UPROPERTY(Config, EditAnywhere) float FieldOfView = 120.f;
	UPROPERTY(Config, EditAnywhere) float DetectionSeconds = .6f;
	UPROPERTY(Config, EditAnywhere) float MemorySeconds = 5.f;
	UPROPERTY(Config, EditAnywhere) float ShareRange = 1600.f;
	UPROPERTY(Config, EditAnywhere) float ShareDelay = .8f;
	UPROPERTY(Config, EditAnywhere) float StandingSpreadDegrees = 1.2f;
	UPROPERTY(Config, EditAnywhere) float MovingSpreadDegrees = 4.f;
	UPROPERTY(Config, EditAnywhere) float SettledSpreadDegrees = .35f;
	UPROPERTY(Config, EditAnywhere) float AimSettleSeconds = .8f;
	UPROPERTY(Config, EditAnywhere) float AimResetDegrees = 3.f;
	UPROPERTY(Config, EditAnywhere) float AimStationarySpeed = 20.f;
	UPROPERTY(Config, EditAnywhere) float FlankAngleDegrees = 60.f;
	UPROPERTY(Config, EditAnywhere) float CoverSearchRadius = 900.f;
	UPROPERTY(Config, EditAnywhere) int32 CoverCandidateLimit = 12;
	UPROPERTY(Config, EditAnywhere) float TacticalDecisionSeconds = 1.f;
	UPROPERTY(Config, EditAnywhere) float TacticalCommitSeconds = 2.f;
	UPROPERTY(Config, EditAnywhere) float TacticalSwitchMargin = .35f;
	UPROPERTY(Config, EditAnywhere) float SquadDecisionSeconds = .5f;
	UPROPERTY(Config, EditAnywhere) float SquadFlankDistance = 600.f;
	UPROPERTY(Config, EditAnywhere) float SquadFlankStandoff = 250.f;
	UPROPERTY(Config, EditAnywhere) float SquadFlankMaxTravel = 1800.f;
	UPROPERTY(Config, EditAnywhere) float SquadWaveTimeout = 8.f;
	UPROPERTY(Config, EditAnywhere) float AIDecisionSeconds = .2f;
	UPROPERTY(Config, EditAnywhere) int32 RecruitCost = 100;
	UPROPERTY(Config, EditAnywhere) int32 RegularCost = 180;
	UPROPERTY(Config, EditAnywhere) int32 VeteranCost = 300;
	UPROPERTY(Config, EditAnywhere) bool bStaggerDecisions = true;
	UPROPERTY(Config, EditAnywhere) int32 PerceptionSlices = 4;
};

UCLASS()
class UBattleState : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	int32 RegisterUnit(int32 Team, float InitialHealth = 0.f, EBattleWeapon Weapon = EBattleWeapon::Carbine);
	int32 CreateSquad(int32 Team, const TArray<int32>& Members);
	int32 CreateLeaderSquad(int32 Team, const TArray<int32>& Members, int32 LeaderID, bool bAllowControl);
	bool DisableLeaderSquad(int32 SquadID);
	bool CanSelectLeader(int32 UnitID, int32 PlayerTeam = 0) const;
	FBattleUnitDied OnUnitDied;
	const FBattleSquadState* FindSquad(int32 ID) const { return Squads.Find(ID); }
	const TMap<int32, FBattleSquadState>& GetSquads() const { return Squads; }
	bool SetSquadOrder(int32 ID, EBattleOrder Order, FVector Destination, const TMap<int32, FVector>& Goals);
	void SetAction(int32 ID, EBattleAction Action);
	void SetMoveRetries(int32 ID, int32 Retries);
	bool SetPerceptionProfile(int32 ID, const FBattlePerceptionProfile& Profile);
	void UpdateAim(int32 ID, double Now, float Speed, FVector Direction, bool bGrounded);
	bool SetSpreadSkill(int32 ID, float Scale);
	void SetTacticalState(int32 ID, int32 CoverSlot, const FString& Reason);
	void SetCoordination(int32 ID, EBattleRole Role, int32 TargetID = INDEX_NONE, FVector Goal = FVector::ZeroVector);
	bool SetTrainingPreset(int32 ID, EBattleTraining Training);
	const FBattleUnitState* Find(int32 ID) const { return Units.Find(ID); }
	bool SetOrder(int32 ID, EBattleOrder Order, FVector Destination);
	bool ConsumeShot(int32 ID, double Now, double Interval);
	bool StartReload(int32 ID, double Now);
	void AdvanceReloads(double Now);
	bool ApplyDamage(int32 ID, int32 AttackingTeam, float Damage);
	int32 AddReserve(int32 ID, int32 Rounds);
	void RestoreRecords(const TArray<FBattleUnitState>& SavedUnits, const TArray<FBattleSquadState>& SavedSquads);
	const TMap<int32, FBattleUnitState>& GetUnits() const { return Units; }
private:
	UPROPERTY() TMap<int32, FBattleUnitState> Units;
	UPROPERTY() TMap<int32, FBattleSquadState> Squads;
	int32 NextSquadID = 1;
	int32 NextID = 1;
};
