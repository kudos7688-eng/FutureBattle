#pragma once
#include "CoreMinimal.h"
#include "BattleAreaState.generated.h"

USTRUCT()
struct FBattleAreaState
{
 GENERATED_BODY()
 UPROPERTY() TArray<float> BaseHP={1000,1000};
 UPROPERTY() TMap<int32,float> Replacements;
 UPROPERTY() TMap<int32,float> AmmoTimers;
 // Three central points per observing team. -2 means never observed.
 UPROPERTY() TArray<int32> KnownOwners={-2,-2,-2,-2,-2,-2};
 UPROPERTY() TArray<float> SeenAt={-1,-1,-1,-1,-1,-1};
 UPROPERTY() TArray<float> KnownEnemyBaseHP={-1,-1};
 UPROPERTY() float ObserveClock=0;
 UPROPERTY() float SupplyClock=0;
 // Resources spent on new squads per team (absent in pre-hiring saves = 0). Hired squads = team squads - 25.
 UPROPERTY() TArray<int32> HireSpent={0,0};
 // Resource income received per team and the shared payment clock (absent in pre-income saves = 0).
 UPROPERTY() TArray<int32> Income={0,0};
 UPROPERTY() float IncomeClock=0;
 // Match time of each team's last AI purchase (-1000 = none), for the AI purchase interval.
 UPROPERTY() TArray<float> LastAIHireAt={-1000,-1000};
 // P1 purpose purchase trial (2026-11-16): mission of each AI-bought squad, 0..N-1 = objective index (take/hold),
  // 100 = enemy home assault, 101 = own home defence; and the match time the own home last took damage (-1000 = never).
 UPROPERTY() TMap<int32,int32> HirePurpose;
 UPROPERTY() TArray<float> HomeHitAt={-1000,-1000};
 // War score trial (BattleWarScore.h; absent in older saves = 0). Per team: score from held central points and from enemy losses,
 // each observer's estimate of the OPPONENT's point score (from last observed ownership), and the shared scoring clock.
 UPROPERTY() TArray<int32> WarPoint={0,0};
 UPROPERTY() TArray<int32> WarLoss={0,0};
 UPROPERTY() TArray<int32> WarPointEstimate={0,0};
 UPROPERTY() float WarClock=0;
 // Rounds left on each supply vehicle (squad type 9; BattleSupplyCar.cpp). A vehicle without an entry is full (UBattleTuning::SupplyCarStock).
 UPROPERTY() TMap<int32,int32> SupplyStock;
};
