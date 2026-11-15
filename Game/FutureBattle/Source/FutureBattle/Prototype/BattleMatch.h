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
 float PointWeight[3]={1.f,1.f,1.f}; // per objective index, multiplies the S1 weight
 int32 HireReserve=0;                 // resources kept back before an AI purchase (above pending replacements)
 float FlankOffset=0.f; bool bFlankSameSide=false; // S7/S8 route of home-assault squads (0 = straight)
 int32 BaseReturnLiving=0;            // S8: living count at which home-assault squads return for supply (0 = SupplyReturnLiving)
 int32 Phase=0;                       // index of the active phase of the team plan (0 when no plan)
};
// One lane (the third of the field around one objective) as seen by one team: own soldiers and own front (furthest progress
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
 // Area new squad (regular only) into a free slot (<25 active squads) for BattleTactics::AreaHire().Cost resources.
 // bAuto: AI purchase (no selection change; a team-0 squad is delegated). Cancels pending replenishments the spend can no longer cover.
 bool HireAreaSquad(int32 Team,EBattleTraining Training,bool bAuto);
 int32 LastHiredSquadID=INDEX_NONE;
 // P1 trial: the observed need a new AI squad would serve (see FAreaHireTuning), or -1 for none.
 int32 AreaHireNeed(int32 Team) const;
 // AI purchase rule, every director tick: own resources minus own pending replenishments >= cost + reserve, and a free slot.
 // Team 1 always; team 0 only in the all-AI sight experiment.
 void UpdateAreaHiring();
 int32 ActiveSquads(int32 Team) const;
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
	void AreaFronts(int32 Team, FAreaFront (&Out)[3]) const;
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
