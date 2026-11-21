#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleSquadTactics.h"
#include "BattleSave.generated.h"

USTRUCT()
struct FBattleSavedUnit
{
	GENERATED_BODY()
	UPROPERTY() FBattleUnitState State;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	// Stage A (-BattleThreatCover): the cover place he left because it was seen into, and the seconds left (at most 10) before
	// he may take it again. INDEX_NONE (also in saves made before 2026-11-21) = none. Slot numbers follow the order the places
	// were built in, so the place itself is saved too and decides on load (the number is only tried first).
	UPROPERTY() int32 BrokenCoverSlot = INDEX_NONE;
	UPROPERTY() float BrokenCoverLeft = 0;
	UPROPERTY() FVector BrokenCoverAt = FVector::ZeroVector;
};

// Versioned, single-map match snapshot. Timers are relative to the save instant.
USTRUCT()
struct FBattleSavedDamage
{
 GENERATED_BODY()
 UPROPERTY() int32 SquadID=INDEX_NONE;
 UPROPERTY() double Bucket=0;
 UPROPERTY() float Damage=0;
};

UCLASS()
class UBattleSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
 UPROPERTY() FBattleAreaState Area;
 UPROPERTY() FString MapName;
 UPROPERTY() FName Ruleset;
 UPROPERTY() float BaseSightRange=0;
 UPROPERTY() float WeaponRange=0;
 UPROPERTY() bool bRangeAware=true;
 UPROPERTY() int32 TacticalFeatures=0;
 UPROPERTY() float DelegationClock=0;
 UPROPERTY() TMap<int32,FBattleSquadTacticalPlan> SavedPlans;
 UPROPERTY() double TacticalClock=0;
 UPROPERTY() TMap<int32,FBattleSquadObservation> TacticalSamples;
 UPROPERTY() TMap<int32,double> NextSamples;
 UPROPERTY() TArray<FBattleSavedDamage> DamageBuckets;
 UPROPERTY() double TeamNextAssignment=0;
 UPROPERTY() TMap<int32,double> TeamRetryAfter;
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
 bool ValidateMixed(FString& Error) const;
};
