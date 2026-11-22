#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleSquadTactics.h"
#include "BattleSave.generated.h"

namespace BattleSaveValidation
{
 // 1 GiB: refuses absurd files before the codec, far above any real save. The first limit (64 MiB, review 05 S10) rejected
 // legitimate map A saves at the 80 s round trip (2026-11-22; ~1000 soldiers, tagged contact maps).
 constexpr int32 MaxBytes=1024*1024*1024;
 USaveGame* Decode(const TArray<uint8>& Bytes,FString& Error);
 USaveGame* Decode(const TArray<uint8>& Bytes,FString& Error,int32 Limit); // Smoke tests pass a small limit (no 1 GiB buffer).
 USaveGame* Decode(const TArray<uint8>& Bytes);
 double ClockLimit(const FString& MapName);
 // Far beyond a real match, with ample headroom for subsequent increments.
 inline bool Counter(int32 Value){return Value>=0&&Value<=100000000;}
 bool Contact(const FBattleContact& Value,int32 ObserverTeam,const FBattleUnitState* Source,
  const FBattleUnitState* Target,FVector2D Limit);
}

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

// Extension revision 1: relative artillery clocks. Revision 0 is an older save without this state.
USTRUCT()
struct FBattleSavedArtillery
{
 GENERATED_BODY()
 UPROPERTY() bool bSettingUp=false;
 UPROPERTY() double SetupElapsed=0;
 UPROPERTY() double ReloadRemaining=0;
 UPROPERTY() double PickRemaining=0;
 UPROPERTY() int32 Shots=0;
 UPROPERTY() int32 TargetKey=INDEX_NONE; // observed aim identity; older saves did not retain it
 UPROPERTY() FVector Aim=FVector::ZeroVector;
 UPROPERTY() bool bHasTarget=false;
 UPROPERTY() bool bBase=false;
};

USTRUCT()
struct FBattleSavedArtilleryShell
{
 GENERATED_BODY()
 UPROPERTY() int32 ShooterID=INDEX_NONE;
 UPROPERTY() int32 Team=0;
 UPROPERTY() uint8 ShooterType=0;
 UPROPERTY() bool bSPG=false;
 UPROPERTY() FVector At=FVector::ZeroVector;
 UPROPERTY() float Remaining=0;
};

// Revision 2: group decision state, including historical roles for squads that have since left.
// All three time fields are signed offsets from match Elapsed, NOT UWorld::TimeSeconds.
// Keep doubles for exact round trips of float clocks; do not clamp expired deadlines or future phase starts.
USTRUCT()
struct FBattleSavedGroup
{
 GENERATED_BODY()
 UPROPERTY() int32 ID=0;
 UPROPERTY() int32 Team=0;
 UPROPERTY() TArray<int32> Squads;
 UPROPERTY() int32 Objective=INDEX_NONE;
 UPROPERTY() uint8 Phase=0;
 UPROPERTY() double PhaseAge=0;
 UPROPERTY() FVector Rally=FVector::ZeroVector;
 UPROPERTY() bool bPlayer=false;
 UPROPERTY() TMap<int32,uint8> Roles;
 UPROPERTY() int32 Avoid=INDEX_NONE;
 UPROPERTY() double AvoidRemaining=0;
 UPROPERTY() int32 StartLiving=0;
 UPROPERTY() double StartStrength=-1; // additive trial field; older snapshots initialize on next group update
 UPROPERTY() int32 StallPoint=INDEX_NONE;
 UPROPERTY() int32 Stalls=0;
 UPROPERTY() bool bOpened=false;
 UPROPERTY() bool bArrived=false;
 UPROPERTY() double ArrivalAge=0;
 UPROPERTY() int32 Extensions=0;
 // Front-holding and surprise groups (groupui 2026-11-21; additive, older snapshots load as attack groups). Times as offsets from Elapsed.
 UPROPERTY() uint8 Kind=0;
 UPROPERTY() int32 SurpriseRow=0;
 UPROPERTY() uint8 SurpriseBridgeStage=0;
 UPROPERTY() int32 SurpriseBridgeLeader=INDEX_NONE;
 UPROPERTY() double SurpriseBridgeAge=0;
 UPROPERTY() int32 SurpriseNextCol=-1;
 UPROPERTY() TArray<int32> FrontHold;
 UPROPERTY() double FrontStepAge=0;
 UPROPERTY() bool bFrontDrawn=false;
 UPROPERTY() double FrontBackAge=100;
 UPROPERTY() TArray<uint8> FrontGap;
 UPROPERTY() int32 FrontNeed=0;
 UPROPERTY() double FrontReleaseAge=100;
 UPROPERTY() double FrontExcessAge=-1; // -1: no excess
 UPROPERTY() FVector SurpriseBridgeEntry=FVector::ZeroVector;
 UPROPERTY() FVector SurpriseBridgeExit=FVector::ZeroVector;
 UPROPERTY() FVector SurpriseBridgeGoal=FVector::ZeroVector;
 UPROPERTY() bool bSurpriseWaiting=false;
 UPROPERTY() double SurpriseWaitAge=0;
 UPROPERTY() double SurpriseLogRemaining=0;
 UPROPERTY() double SurpriseEndRemaining=0;
};

USTRUCT()
struct FBattleSavedCarChoice
{
 GENERATED_BODY()
 UPROPERTY() int32 Target=INDEX_NONE;
 UPROPERTY() bool bHasTime=false; // runtime At=-1 means no choice has been made
 UPROPERTY() double Age=0;
};

// Independent optional extension; signed deadlines use match Elapsed, not world time.
USTRUCT()
struct FBattleSavedPlayerRoute
{
 GENERATED_BODY()
 UPROPERTY() int32 SquadID=INDEX_NONE;
 UPROPERTY() int32 Lane=0;
 UPROPERTY() int32 Phase=0;
 UPROPERTY() FVector PhaseGoal=FVector(FLT_MAX);
 UPROPERTY() bool bMission=false;
 UPROPERTY() uint8 Order=0;
 UPROPERTY() FVector Goal=FVector::ZeroVector;
 UPROPERTY() FVector Step=FVector::ZeroVector;
 UPROPERTY() bool bPath=false;
 UPROPERTY() TArray<FVector> Waypoints;
 UPROPERTY() double DeadlineRemaining=0;
};

UCLASS()
class UBattleSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
 // Keep the existing mode versions/slot names; absent extension fields default to revision 0.
 UPROPERTY() int32 CombatStateVersion=0;
 UPROPERTY() int32 CompositionTeams=0;
 UPROPERTY() int32 CompositionPolicy=0;
 UPROPERTY() FString CompositionRules;
 UPROPERTY() TArray<FCompositionTeamState> CompositionState;
 // Revision 3: SPG main-gun rounds in State and indirect-fire trial configuration.
 UPROPERTY() int32 ArtilleryTrialFlags=0;
 UPROPERTY() int32 StrengthShieldTrialFlags=-1; // absent in older saves; additive to extension 3
 UPROPERTY() TMap<int32,FBattleSavedGroup> SavedGroups;
 UPROPERTY() TMap<int32,int32> SavedSquadGroup;
 UPROPERTY() int32 SavedNextGroupID=1;
 UPROPERTY() TMap<int32,FBattleSavedCarChoice> SavedCarChoices;
 UPROPERTY() TMap<int32,FBattleSavedArtillery> SavedArtillery;
 UPROPERTY() TArray<FBattleSavedArtilleryShell> SavedArtilleryShells;
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
 // Omitted from the tagged archive when absent, including the version tag itself.
 UPROPERTY() int32 PlayerRoutesVersion=0;
 UPROPERTY() TArray<FBattleSavedPlayerRoute> SavedPlayerRoutes;
 UPROPERTY() float SavedPlayerRouteClock=0;
 virtual void Serialize(FArchive& Ar) override;
	bool Validate(FString& Error) const;
 bool ValidateMixed(FString& Error) const;
};
