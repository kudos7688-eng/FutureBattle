#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleSave.generated.h"

USTRUCT()
struct FBattleSavedUnit
{
	GENERATED_BODY()
	UPROPERTY() FBattleUnitState State;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
};

// Versioned, single-map match snapshot. Timers are relative to the save instant.
UCLASS()
class UBattleSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FGuid MatchID;
	UPROPERTY() uint8 Phase = 0;
	UPROPERTY() int32 AllyCredits = 0;
	UPROPERTY() int32 EnemyCredits = 0;
	UPROPERTY() float AllyScore = 0;
	UPROPERTY() float EnemyScore = 0;
	UPROPERTY() float Elapsed = 0;
	UPROPERTY() float DirectorClock = 0;
	UPROPERTY() float SupplyClock = 0;
	UPROPERTY() int32 Winner = INDEX_NONE;
	UPROPERTY() FString Result;
	UPROPERTY() bool bPaused = false;
	UPROPERTY() int32 ControlledID = INDEX_NONE;
	UPROPERTY() int32 SelectedID = INDEX_NONE;
	UPROPERTY() FRotator ViewRotation = FRotator::ZeroRotator;
	UPROPERTY() TArray<FBattleObjective> Objectives;
	UPROPERTY() TArray<FBattleSavedUnit> Units;
	UPROPERTY() TArray<FBattleSquadState> Squads;
	UPROPERTY() TMap<int32, FBattleObserver> Observers;
	UPROPERTY() TArray<FBattleReport> Reports;
	UPROPERTY() TArray<FBattleStrike> Strikes;
	UPROPERTY() float AllySupportCooldown = 0;
	UPROPERTY() float EnemySupportCooldown = 0;
	UPROPERTY() int32 AllySupportUsed = 0;
	UPROPERTY() int32 EnemySupportUsed = 0;
	bool Validate(FString& Error) const;
};
