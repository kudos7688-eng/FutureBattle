#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleState.h"
#include "BattleMatch.generated.h"
class UBattleSave;

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
	void InitializeMatch();
	static int32 SquadPrice(EBattleTraining Training);
	bool HireSquad(int32 Team, EBattleTraining Training);
	bool StartCombat();
	void Finish(int32 WinningTeam, const FString& Reason);
	void Update(float DeltaSeconds);
	void UpdateObjectives(float DeltaSeconds);
	void AdvanceObjectiveControl(float DeltaSeconds, TFunctionRef<int32(const FBattleObjective&)> Presence);
	static bool InObjective(FVector Position, const FBattleObjective& Point);
	int32 ChooseObjective(int32 Group, int32 Team) const;
	void CheckTimeLimit();
	void UpdateEnemyOrders();
	void UpdateTeamOrders(int32 Team);
	int32 Resupply(int32 UnitID, int32 RequestedRounds = 10);
	UBattleSave* CaptureSnapshot() const;
	bool RestoreSnapshot(const UBattleSave* Snapshot);
	bool SaveMatch(const FString& Slot = TEXT("FutureBattleQuick01"));
	bool LoadMatch(const FString& Slot = TEXT("FutureBattleQuick01"));
	bool RequestSupport(int32 ObserverID);
	void UpdateSupport(float DeltaSeconds);
	bool IsEnabled() const { return Phase != EBattleMatchPhase::Disabled; }
};
