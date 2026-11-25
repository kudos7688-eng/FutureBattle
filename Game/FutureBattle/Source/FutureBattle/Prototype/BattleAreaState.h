#pragma once
#include "CoreMinimal.h"
#include "BattleAreaState.generated.h"

// Purchase reservation (Docs/design-front-strategy.md 5.1, -BattleHireReserve=1). Resources are only spent when it is bought.
USTRUCT()
struct FBattleHireReservation
{
 GENERATED_BODY()
 UPROPERTY() int32 Team=0;
 UPROPERTY() int32 Type=0;
 // Destination (stage 6: section / group); -1 = none.
 UPROPERTY() int32 Dest=-1;
 // Match time (Elapsed) of the reservation.
 UPROPERTY() float At=0;
 UPROPERTY() bool bByAI=false;
};

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
 // Purchase reservations of both teams in reservation order (absent in older saves = empty; BattleArea.cpp UpdateHireQueue).
 UPROPERTY() TArray<FBattleHireReservation> HireQueue;
 // Stage 7 (Docs/design-front-strategy.md 6; absent in older saves = defaults): the player's role per front plan section of team 0
 // (0 the AI decides, 1 focus, 2 keep, 3 hold, 4 may fall back; -BattleFrontPlanPlayer=1) and the player's hiring delegation
 // (0 manual, 1 refill only, 2 all; -BattleHireReserve=1), the resources the AI leaves unspent and the squad types it may not buy (bit per type).
 UPROPERTY() TArray<uint8> PlayerSectionRole;
 UPROPERTY() uint8 PlayerHireDelegate=0;
 UPROPERTY() int32 PlayerHireReserve=0;
 UPROPERTY() int32 PlayerHireDeny=0;
 // Section orders (-BattleFrontSectionOrders=1; absent in older saves = empty): the player's target column per own row of team 0, -1 none.
 UPROPERTY() TArray<int32> FrontTarget;
 // War score trial (BattleWarScore.h; absent in older saves = 0). Per team: score from held central points and from enemy losses,
 // each observer's estimate of the OPPONENT's point score (from last observed ownership), and the shared scoring clock.
 UPROPERTY() TArray<int32> WarPoint={0,0};
 UPROPERTY() TArray<int32> WarLoss={0,0};
 UPROPERTY() TArray<int32> WarPointEstimate={0,0};
 UPROPERTY() float WarClock=0;
 // Rounds left on each supply vehicle (squad type 9; BattleSupplyCar.cpp). A vehicle without an entry is full (UBattleTuning::SupplyCarStock).
 UPROPERTY() TMap<int32,int32> SupplyStock;
 // Focus state, written by the front plan (BattleFrontLine.cpp, -BattleFrontPlan; absent in older saves = no focus). Per team: phase (0 deploy, 1 focus),
 // the focus objective (-1 none), when the focus began, the next review, and a focus the AI attack group failed at with its end time.
 // Match times (Elapsed), as HomeHitAt.
 UPROPERTY() TArray<int32> StrategyPhase={0,0};
 UPROPERTY() TArray<int32> StrategyFocus={-1,-1};
 UPROPERTY() TArray<float> StrategyFocusAt={-1,-1};
 UPROPERTY() TArray<float> StrategyReviewAt={-1,-1};
 UPROPERTY() TArray<int32> StrategyAvoid={-1,-1};
 UPROPERTY() TArray<float> StrategyAvoidUntil={-1,-1};
 // (Stage S3/S4 fields StrategyHoldMask, StrategyThreatSince, StrategyCounter, StrategyCounterUntil and StrategyHired were removed
 // with the older strategy layer on 2026-11-25; older saves that carry them load with the fields skipped.)
};
