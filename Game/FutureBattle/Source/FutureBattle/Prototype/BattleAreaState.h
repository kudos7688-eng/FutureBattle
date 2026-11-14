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
};
