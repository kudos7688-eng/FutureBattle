#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleState.h"
#include "BattleAreaState.h"
#include "BattleMatch.generated.h"
class UBattleSave;
// Area strategy layer (ROADMAP 2026-11-16). The team intent is all the lower layers read: point allocation (S1 with S4/S6) and
// AI purchasing. Stage 1: a fixed intent built from the tuning, so behaviour is unchanged.
struct FAreaIntent
{
 float AttackWeight=2.f,BaseShare=0.2f; bool bFrontBalance=false; float FrontGain=1.f,FocusGain=0.f;
 static constexpr int32 MaxObjectives=8; // three objectives on maps A-D, five on map E
 float PointWeight[MaxObjectives]={1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f}; // per objective index, multiplies the S1 weight
 int32 HireReserve=0;                 // resources kept back before an AI purchase (above pending replacements)
 float FlankOffset=1.3f; bool bFlankSameSide=true; // S7/S8 route of home-assault squads (0 = straight; FS13BR5 default since 2026-11-17)
 int32 BaseReturnLiving=5;            // S8: living count at which home-assault squads return for supply (0 = SupplyReturnLiving; 5 by default since 2026-11-17)
 int32 Phase=0;                       // index of the active phase of the team plan (0 when no plan)
 float HeavyShare=0.f;                // share of AI purchases that are heavy infantry (0 = rifle squads only)
 int32 TankKeep=0;                    // tanks the AI keeps active (same pacing as scouts; 0 = none)
 int32 ATKeep=0;                      // anti-tank teams the AI keeps active (same pacing as scouts; 0 = none)
 int32 ScoutKeep=0;                   // scout pairs the AI keeps active (one more per ScoutAfterHires purchases, up to this; 0 = none)
};
// One sector (the part of the field nearest one objective; a lane on maps A-D) as seen by one team: own soldiers and own front (furthest progress
// from own home, 0..1 along the home-to-home line, 90th percentile), and enemies currently visible to the team (count and the
// progress of the most advanced one, -1 = none seen). Observed information only.
struct FAreaFront { float OwnFront=-1.f,EnemyFront=-1.f; int32 Own=0,EnemySeen=0; };

enum class EBattleMatchPhase : uint8 { Disabled, Deployment, Combat, Finished };

USTRUCT()
struct FBattleObjective
{
	GENERATED_BODY()
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() int32 Owner = INDEX_NONE;
	UPROPERTY() int32 CapturingTeam = INDEX_NONE;
	UPROPERTY() float CaptureProgress = 0.f;
	UPROPERTY() bool bContested = false;
	UPROPERTY() float IncomeClock = 0.f;
};

USTRUCT()
struct FBattleStrike
{
	GENERATED_BODY()
	UPROPERTY() int32 Team = 0;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() float Remaining = 3.f;
};

// Match economy is independent of actors and possession. All purchases pass through this store.
UCLASS()
class UBattleMatch : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	EBattleMatchPhase Phase = EBattleMatchPhase::Disabled;
	FGuid MatchID;
	int32 Credits[2] = { 0, 0 };
	int32 Winner = INDEX_NONE;
	UPROPERTY() TArray<FBattleObjective> Objectives;
	float Score[2] = { 0.f, 0.f };
	float Elapsed = 0.f;
	float DirectorClock = 0.f, SupplyClock = 0.f;
	UPROPERTY() TArray<FBattleStrike> Strikes;
	float SupportCooldown[2] = { 0, 0 };
	int32 SupportUsed[2] = { 0, 0 };
	FString LastMessage;
 bool bArea=false;
 UPROPERTY() FBattleAreaState Area;
 UPROPERTY() TArray<TObjectPtr<AActor>> BaseActors;
 void InitializeArea();
 void UpdateArea(float Dt);
 bool AreaVisible(int32 Observer,FVector Point,AActor* Target=nullptr) const;
 bool DamageBase(AActor* Target,int32 AttackerTeam,float Damage);
 bool CanFireAtBase(int32 UnitID,FVector& Target) const;
 FVector AreaHome(int32 Team) const;
 int32 ObservedOwner(int32 Team,int32 Point) const;
 int32 AreaScore(int32 Team) const;
 // War score trial (BattleWarScore.h). Earned = points + enemy losses; Score adds unspent resources; Estimate is what Observer
 // can know of the opponent's Earned (own losses exactly, points from observed ownership only).
 int32 WarEarned(int32 Team) const;
 int32 WarScore(int32 Team) const;
 int32 WarScoreEstimate(int32 Observer) const;
 void RecordWarLoss(int32 LostTeam);
 void UpdateWarScore(float Dt);
 FString WarScoreBreakdown(int32 Team) const;
 // Area new squad (regular only) into a free slot (<25 active squads) for BattleTactics::AreaHire().Cost resources.
 // bAuto: AI purchase (no selection change; a team-0 squad is delegated). Cancels pending replenishments the spend can no longer cover.
 bool HireAreaSquad(int32 Team,EBattleTraining Training,bool bAuto,uint8 SquadType=0);
 int32 LastHiredSquadID=INDEX_NONE;
 // P1 trial: the observed need a new AI squad would serve (see FAreaHireTuning), or -1 for none.
 int32 AreaHireNeed(int32 Team) const;
 // AI purchase rule, every director tick: own resources minus own pending replenishments >= cost + reserve, and a free slot.
 // Team 1 always; team 0 only in the all-AI sight experiment.
 void UpdateAreaHiring();
 int32 ActiveSquads(int32 Team) const;
 // Active scout pairs of a team (at most UBattleTuning::AreaScoutLimit).
 int32 ActiveScouts(int32 Team) const;
 int32 ActiveOfType(int32 Team,uint8 SquadType) const;
 float MatchTimeLimit() const;
 // Smoke fixtures only (not saved): suspend Area income so exact resource checks stay deterministic.
 bool bAreaIncomeHeld=false;
	void InitializeMatch();
	static int32 SquadPrice(EBattleTraining Training);
 int32 CurrentSquadPrice(EBattleTraining Training) const;
	bool HireSquad(int32 Team, EBattleTraining Training);
	bool StartCombat();
	void Finish(int32 WinningTeam, const FString& Reason);
	void Update(float DeltaSeconds);
	void UpdateObjectives(float DeltaSeconds);
	void AdvanceObjectiveControl(float DeltaSeconds, TFunctionRef<int32(const FBattleObjective&)> Presence);
	static bool InObjective(FVector Position, const FBattleObjective& Point);
	int32 ChooseObjective(int32 Group, int32 Team) const;
	// S1 allocation for one squad from observed point ownership only (Area). bBase = assault the enemy home instead.
	int32 AreaStrategyTarget(int32 SquadID, int32 Team, bool& bBase) const;
	FAreaIntent AreaIntent(int32 Team) const;
	// Heavy infantry objective: the observed own point with the fewest own soldiers within 100 m (hold the thin front), or,
	// with no own point, the point nearest the own home. Observed ownership, own positions and fixed positions only.
	int32 AreaHeavyTarget(int32 Team) const;
	// Observation post of a scout pair: ahead of an own front point (observed ownership and static positions only).
	FVector AreaScoutPost(int32 Team, int32 SquadID) const;
	// Scout reports (ROADMAP 2026-11-14, stage 3): what a scout pair sees beyond normal sight reaches its team only through a timed
	// two-step report (direction after ScoutAlertSeconds, positions and size after ScoutReportSeconds). Transient: a load restarts the timers.
	struct FScoutReport { double Started=-1, LastLong=-1, AlertAt=-1, ReportedAt=-1, NextHint=0, RetreatUntil=-1, PauseStart=-1, LastDanger=-1; FVector ReportFrom=FVector::ZeroVector; float LastHP=-1; int32 Stage=0, Living=0, Sequence=0, AlertSequence=0, Burns=0; FVector Direction=FVector::ZeroVector, Approx=FVector::ZeroVector, RetreatPoint=FVector::ZeroVector; TArray<FVector> Positions; };
	// Scout retreat (2026-11-17): an AI-led pair that is hit falls back ScoutRetreatDistance from the nearest seen enemy, watches from
	// there until ScoutHoldSeconds pass without a hit, then returns to its post shifted sideways (Burns). The report pauses while it moves.
	bool IsAIScoutLed(int32 SquadID) const;
	// Intel trial (-BattleAreaIntel=1, 2026-11-17): the AI acts on what its team knows of enemy positions (own sight now, scout reports).
	// (1) reinforce own points where more enemies are known than own soldiers, (2) attack the points where fewer enemies are known,
	// (3) raise the home-assault share when the enemy home was observed lightly held, (4) buy squads to defend a threatened point
	// (released once the threat is gone and the squad is there). Known positions only; never the hidden truth.
	struct FAreaIntelView { double At=-1; TArray<FVector> Enemies; TArray<int32> EnemyKeys /* unit IDs, below zero for scout report positions */; TArray<int32> OwnAt, EnemyAt; int32 HomeEnemies=0; bool bHomeCovered=false; };
	const FAreaIntelView& AreaIntelView(int32 Team) const;
	bool AreaIntelOn(int32 Team) const;
	int32 AreaIntelThreat(int32 Team) const;   // own point with the largest known enemy surplus (-1 = none)
	bool AreaIntelThreatAt(int32 Team, int32 Point) const;
	void UpdateIntelPurposes();
	TMap<int32, int32> IntelPurpose;           // squad -> point it was bought to defend (transient)
	mutable FAreaIntelView IntelViews[2];
	TMap<int32, FScoutReport> ScoutReports;
	void UpdateScoutReports();
	// True when a scout sees this contact only because of its long still-sight (team displays and plans use the reports instead).
	bool IsScoutLongContact(int32 ObserverID, const struct FBattleContact& Contact) const;
	// Positions from the team's scout reports of the last ScoutReportMemory seconds (keys below zero, never unit IDs).
	void AddScoutReportSightings(int32 Team, TMap<int32, FVector>& Seen) const;
	void AreaFronts(int32 Team, FAreaFront (&Out)[FAreaIntent::MaxObjectives]) const;
	void CheckTimeLimit();
	void UpdateEnemyOrders();
	void UpdateTeamOrders(int32 Team, bool bDelegatedOnly = false);
	bool CanDelegateAllies() const;
	bool SetAllyDelegated(int32 SquadID, bool bDelegated);
	float DelegationClock = 5.f;
	int32 Resupply(int32 UnitID, int32 RequestedRounds = 10);
	UBattleSave* CaptureSnapshot() const;
	bool RestoreSnapshot(const UBattleSave* Snapshot);
	bool SaveMatch(const FString& Slot = TEXT("FutureBattleQuick01"));
	bool LoadMatch(const FString& Slot = TEXT("FutureBattleQuick01"));
	bool RequestSupport(int32 ObserverID);
	void UpdateSupport(float DeltaSeconds);
	bool IsEnabled() const { return Phase != EBattleMatchPhase::Disabled; }
};
