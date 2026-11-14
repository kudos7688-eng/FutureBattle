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

UENUM()
enum class EBattleSquadTactic : uint8 { Mission, Engage, Support, Flank, Retreat, Regroup, Recover };

inline const TCHAR* BattleSquadTacticName(EBattleSquadTactic Tactic)
{
 switch (Tactic) {
 case EBattleSquadTactic::Engage: return TEXT("ENGAGE");
 case EBattleSquadTactic::Support: return TEXT("SUPPORT");
 case EBattleSquadTactic::Flank: return TEXT("FLANK");
 case EBattleSquadTactic::Retreat: return TEXT("RETREAT");
 case EBattleSquadTactic::Regroup: return TEXT("REGROUP");
 case EBattleSquadTactic::Recover: return TEXT("RECOVER / DEFEND");
 default: return TEXT("FOLLOW MISSION"); }
}

USTRUCT()
struct FBattleSquadTacticalPlan
{
 GENERATED_BODY()
 UPROPERTY() EBattleSquadTactic Action = EBattleSquadTactic::Mission;
 UPROPERTY() FVector Destination = FVector::ZeroVector;
 UPROPERTY() float Remaining = 0;
 UPROPERTY() int32 Revision = 0;
 UPROPERTY() FString Reason;
 UPROPERTY() int32 SupportSquad = INDEX_NONE;
 UPROPERTY() int32 SupportTarget = INDEX_NONE;
 UPROPERTY() double SupportUntil = 0;
 UPROPERTY() double LastSupportShot = -1;
 UPROPERTY() bool bSupportConfirmed = false;
 UPROPERTY() int32 FlankTarget = INDEX_NONE;
 UPROPERTY() int32 FlankSupport = INDEX_NONE;
 UPROPERTY() int32 FlankSupportChanges = 0;
 UPROPERTY() FVector FlankContact = FVector::ZeroVector;
 UPROPERTY() float FlankExposure = 0;
 // Combat-clock timers owned by ST2, independent of the objective director.
 UPROPERTY() bool bAutonomous = false;
 UPROPERTY() float Age = 0;
 UPROPERTY() float SafeSeconds = 0;
 UPROPERTY() float RetrySeconds = 0;
};

USTRUCT(BlueprintType)
struct FBattleSquadState
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere) bool bPlayerDelegated = false;
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
 // Intent is distinct from the currently executing Order/Destination.
 UPROPERTY() bool bHasMission = false;
 UPROPERTY() EBattleOrder MissionOrder = EBattleOrder::Hold;
 UPROPERTY() FVector MissionDestination = FVector::ZeroVector;
 UPROPERTY() TMap<int32, FVector> MissionGoals;
 UPROPERTY() int32 MissionRevision = 0;
 // ST1 does not generate autonomous plans. ST2 will add selection and versioned plan saves.
 UPROPERTY(Transient) FBattleSquadTacticalPlan TacticalPlan;
 UPROPERTY() float FlankCooldown = 0;
};

// Authoritative match data. Possession and Actor lifetime never create/reset this record.
USTRUCT(BlueprintType)
struct FBattlePerceptionProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SightRange = 10000.f;
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
 UPROPERTY() bool bRetired=false;
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
 UPROPERTY() bool bWeaponAimInitialized=false;
 UPROPERTY() FVector WeaponAimDirection=FVector::ForwardVector;
 // Physical cover movement: enter, hidden, emerge, exposed. Relative timers survive saving.
 UPROPERTY() uint8 CoverPhase=0;
 UPROPERTY() FVector CoverAnchor=FVector::ZeroVector;
 UPROPERTY() FVector CoverPeek=FVector::ZeroVector;
 UPROPERTY() float CoverSeconds=0;
 UPROPERTY() int32 CoverOrderRevision=0;
 UPROPERTY() int32 CoverTargetID=INDEX_NONE;
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
	UPROPERTY(Config, EditAnywhere) float SightRange = 10000.f;
	UPROPERTY(Config, EditAnywhere) float WeaponRange = 5000.f;
	float EffectiveWeaponRange() const;
 float EffectiveSightRange() const;
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
	// Shared physical weapon constraint for direct control and AI: large aim turns delay the next shot.
	UPROPERTY(Config, EditAnywhere) float AimSwitchMinDegrees = 45.f;
	UPROPERTY(Config, EditAnywhere) float AimSwitchBaseSeconds = .15f;
	UPROPERTY(Config, EditAnywhere) float AimSwitchHalfTurnSeconds = .35f;
	float AimSwitchDelay(float Degrees) const { return Degrees >= AimSwitchMinDegrees ? FMath::Max(0.f, AimSwitchBaseSeconds + AimSwitchHalfTurnSeconds * Degrees / 180.f) : 0.f; }
	// Shared cover-action geometry (C key and AI use the same search).
	UPROPERTY(Config, EditAnywhere) float CoverActionSearchRadius = 350.f;
	UPROPERTY(Config, EditAnywhere) int32 CoverActionCandidateLimit = 6;
	// AI cognition only: how often non-player units look for a cover action.
	UPROPERTY(Config, EditAnywhere) float AICoverProbeInterval = .25f;
	UPROPERTY(Config, EditAnywhere) float AICoverRetrySeconds = 5.f;
	// Used only with -BattleAICoverProbe=1 (trial): probes per frame for all AI and AI-only search radius.
	UPROPERTY(Config, EditAnywhere) int32 AICoverProbesPerFrame = 2;
	UPROPERTY(Config, EditAnywhere) float AICoverSearchRadius = 600.f;
	// Used only with -BattleCoverApproach=1 (trial): move to a hidden cover slot when none is within AICoverSearchRadius.
	UPROPERTY(Config, EditAnywhere) float AICoverApproachRadius = 1500.f;
	UPROPERTY(Config, EditAnywhere) float AICoverApproachSeconds = 6.f;
	UPROPERTY(Config, EditAnywhere) float AICoverApproachMaxAdvance = 200.f;
	UPROPERTY(Config, EditAnywhere) int32 AICoverApproachCandidates = 8;
	// AI/perception continuity (adopted default ON 2026-11-13; -BattleReacquireGrace=0 restores the old behaviour):
	// own contacts seen within ReacquireGraceSeconds are re-detected immediately, and a squad keeps its previous target
	// for TargetHoldSeconds after a member's own sighting. Firing still requires a current line of sight.
	UPROPERTY(Config, EditAnywhere) bool bReacquireGrace = true;
	UPROPERTY(Config, EditAnywhere) float ReacquireGraceSeconds = 1.f;
	UPROPERTY(Config, EditAnywhere) float TargetHoldSeconds = 1.f;
	// AI cover firing window (adopted ON 2026-11-14; -BattleCoverFire=0 restores the fixed 1.2s peek): AI stays at the peek point
	// long enough for AICoverShotsPerPeek shots (AI interval 1.4s plus aim delay), capped at AICoverPeekSeconds (<=3 for saves).
	UPROPERTY(Config, EditAnywhere) bool bAICoverFire = true;
	UPROPERTY(Config, EditAnywhere) float AICoverPeekSeconds = 2.8f;
	UPROPERTY(Config, EditAnywhere) int32 AICoverShotsPerPeek = 2;
	// AI cover validation (ON; -BattleCoverValidation=0 restores the old behaviour): start cover only against a contact within
	// weapon range, and before/while peeking require a clear line to the contact's last known position (observed data only).
	UPROPERTY(Config, EditAnywhere) bool bAICoverValidation = true;
	// Blind peek exit (adopted ON 2026-11-14; -BattleCoverPeekSight=0 restores the full window): an AI firing-window peek ends early
	// ("blind") once AICoverPeekBlindSeconds have passed at the peek point without an own sighting of the cover target (observed data only).
	UPROPERTY(Config, EditAnywhere) bool bAICoverPeekSight = true;
	UPROPERTY(Config, EditAnywhere) float AICoverPeekBlindSeconds = 1.f;
	// Leader-mode perception optimizations (adopted ON 2026-11-14; results verified identical): observer sight context reuse,
	// candidate grid, and skipping bone refresh of fog-hidden followers. -BattleReuseSightContext= / -BattlePerceptionGrid= /
	// -BattleReduceHiddenBones= (0/1) override.
	UPROPERTY(Config, EditAnywhere) bool bReuseSightContext = true;
	UPROPERTY(Config, EditAnywhere) bool bPerceptionGrid = true;
	UPROPERTY(Config, EditAnywhere) bool bReduceHiddenBones = true;
	// Squad leader HP = Health x this (1..10). Adopted default 3 (2026-11-13); -BattleLeaderHealth=N overrides for trials.
	UPROPERTY(Config, EditAnywhere) float LeaderHealthMultiplier = 3.f;
	// Area home supply cycle (defaults reproduce the 2026-11-12 behaviour). Trials override with -BattleSupply*=.
	// Return home when living <= SupplyReturnLiving; resume when living >= SupplyResumeLiving (or no resources) and,
	// if SupplyResumeHealthRatio > 0, squad HP ratio reaches it. At most SupplyAwayRatio of a team's active squads
	// may be on a supply trip (1 = no limit; out-of-ammo squads always may). SupplyHealPerSecond heals at home (0 = none).
	UPROPERTY(Config, EditAnywhere) int32 SupplyReturnLiving = 8;
	UPROPERTY(Config, EditAnywhere) int32 SupplyResumeLiving = 10;
	UPROPERTY(Config, EditAnywhere) float SupplyAwayRatio = 1.f;
	UPROPERTY(Config, EditAnywhere) float SupplyHealPerSecond = 0.f;
	UPROPERTY(Config, EditAnywhere) float SupplyResumeHealthRatio = 0.f;
	// Area new-squad hiring (adopted 2026-11-14; -BattleAreaHire=0 disables): a team with fewer than 25 active squads may buy
	// a fresh 10-soldier regular squad at home for AreaSquadCost resources. AI buys only when resources minus pending
	// replenishments >= AreaSquadCost + AreaHireReserve.
	UPROPERTY(Config, EditAnywhere) bool bAreaHire = true;
	UPROPERTY(Config, EditAnywhere) int32 AreaSquadCost = 15;
	UPROPERTY(Config, EditAnywhere) int32 AreaHireReserve = 10;
	// AI purchase pacing (0 = off, the first implementation): at least AreaHireIntervalSeconds between a team's AI purchases,
	// and no AI purchase in the last AreaHireStopSeconds of the time limit. Trials: -BattleAreaHireReserve= / -Interval= / -Stop=.
	UPROPERTY(Config, EditAnywhere) float AreaHireIntervalSeconds = 0.f;
	UPROPERTY(Config, EditAnywhere) float AreaHireStopSeconds = 0.f;
	// Area resource income (adopted 2026-11-14; -BattleAreaIncome=0 disables): every AreaIncomeSeconds each team gains
	// AreaBaseIncome plus AreaPointIncome per central point it actually owns at that moment.
	UPROPERTY(Config, EditAnywhere) bool bAreaIncome = true;
	UPROPERTY(Config, EditAnywhere) float AreaIncomeSeconds = 20.f;
	UPROPERTY(Config, EditAnywhere) int32 AreaBaseIncome = 1;
	UPROPERTY(Config, EditAnywhere) int32 AreaPointIncome = 1;
	// Area AI point allocation S1 (adopted 2026-11-14; -BattleAreaStrategy=0 restores the old round-robin): each squad keeps a fixed
	// golden-ratio slot; points not observed as own weigh AreaAttackWeight, owned ones 1; the top AreaBaseShare of slots assault the enemy home.
	UPROPERTY(Config, EditAnywhere) bool bAreaStrategy = true;
	UPROPERTY(Config, EditAnywhere) float AreaAttackWeight = 2.f;
	UPROPERTY(Config, EditAnywhere) float AreaBaseShare = 0.2f;
	// Used only with -BattleLeaderGuard=1 (trial, AI cognition): AI leader cover retry interval.
	UPROPERTY(Config, EditAnywhere) float LeaderGuardCoverRetrySeconds = 2.f;	UPROPERTY(Config, EditAnywhere) float FlankAngleDegrees = 60.f;
	UPROPERTY(Config, EditAnywhere) float CoverSearchRadius = 900.f;
	UPROPERTY(Config, EditAnywhere) int32 CoverCandidateLimit = 12;
	UPROPERTY(Config, EditAnywhere) float TacticalDecisionSeconds = 1.f;
	UPROPERTY(Config, EditAnywhere) float TacticalCommitSeconds = 2.f;
	UPROPERTY(Config, EditAnywhere) float TacticalSwitchMargin = .35f;
	UPROPERTY(Config, EditAnywhere) float SquadDecisionSeconds = .5f;
 UPROPERTY(Config, EditAnywhere) float SquadRetreatHealthRatio = .45f;
 UPROPERTY(Config, EditAnywhere) float SquadRetreatLivingRatio = .4f;
 UPROPERTY(Config, EditAnywhere) float SquadRetreatDamageRatio = .25f;
 UPROPERTY(Config, EditAnywhere) float SquadRetreatThreatRatio = 2.f;
 UPROPERTY(Config, EditAnywhere) float SquadRegroupSafeSeconds = 5.f;
 UPROPERTY(Config, EditAnywhere) float SquadRetreatTimeout = 12.f;
 UPROPERTY(Config, EditAnywhere) float SquadRetreatRetrySeconds = 5.f;
 UPROPERTY(Config, EditAnywhere) float WideAvoidanceMaxSide = .2f;
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
 bool ReplaceDeadFollower(int32 OldID,int32 NewID,FVector Position);
	int32 CreateSquad(int32 Team, const TArray<int32>& Members);
	int32 CreateLeaderSquad(int32 Team, const TArray<int32>& Members, int32 LeaderID, bool bAllowControl);
	// Maximum HP of a unit: Health, times the trial -BattleLeaderHealth multiplier for a squad leader.
	float MaxHealthOf(int32 UnitID) const;
	bool DisableLeaderSquad(int32 SquadID);
	bool CanSelectLeader(int32 UnitID, int32 PlayerTeam = 0) const;
	FBattleUnitDied OnUnitDied;
	bool SetSquadDelegated(int32 ID, bool bDelegated);
	const FBattleSquadState* FindSquad(int32 ID) const { return Squads.Find(ID); }
 friend class UBattleSquadTactics;
 friend class UBattleLeaderRuntime;
	const TMap<int32, FBattleSquadState>& GetSquads() const { return Squads; }
	bool SetSquadOrder(int32 ID, EBattleOrder Order, FVector Destination, const TMap<int32, FVector>& Goals, bool bMissionUpdate = false);
 bool SetSquadTacticalPlan(int32 ID, EBattleSquadTactic Action, EBattleOrder Order, FVector Destination,
  const TMap<int32, FVector>& Goals, float Lifetime, const FString& Reason);
 void CancelSquadTacticalPlan(int32 ID, const FString& Reason);
 void AdvanceSquadTacticalPlan(int32 ID, float DeltaSeconds);
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
	// Heals a living unit up to MaxHealthOf(ID); returns the amount applied.
	float Heal(int32 ID, float Amount);
	void RestoreRecords(const TArray<FBattleUnitState>& SavedUnits, const TArray<FBattleSquadState>& SavedSquads, bool PreservePlans=false);
	const TMap<int32, FBattleUnitState>& GetUnits() const { return Units; }
private:
	UPROPERTY() TMap<int32, FBattleUnitState> Units;
	UPROPERTY() TMap<int32, FBattleSquadState> Squads;
	int32 NextSquadID = 1;
	int32 NextID = 1;
};
