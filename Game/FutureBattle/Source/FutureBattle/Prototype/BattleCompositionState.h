#pragma once
#include "CoreMinimal.h"
#include "BattleCompositionState.generated.h"

// Combat extension 4. Times are integer milliseconds on the saved match Elapsed clock, NOT world time.
USTRUCT()
struct FCompositionTeamState
{
 GENERATED_BODY()
 UPROPERTY() int32 Serial=0;
 UPROPERTY() TArray<int32> Purchased={0,0,0,0,0,0,0,0,0,0};
 UPROPERTY() TArray<int32> Waiting={0,0,0,0,0,0,0,0,0,0};
 UPROPERTY() TArray<int32> SaveBlockedUntil={0,0,0,0,0,0,0,0,0,0};
 UPROPERTY() int32 SaveType=-1;
 UPROPERTY() int32 SaveMission=-1;
 UPROPERTY() int32 SaveCost=0;
 UPROPERTY() int32 SaveUrgency=0;
 UPROPERTY() int32 SaveStartedMS=0;
 UPROPERTY() int32 SaveDeadlineMS=0;
 UPROPERTY() int32 SwitchType=-1;
 UPROPERTY() int32 SwitchCount=0;
 UPROPERTY() int32 RetryUntil=0;
 UPROPERTY() int32 LastSuccess=0;
 UPROPERTY() FString Reason;
 // Fixed >=20s sampling window: actual own income minus non-hire spending (primarily replacements).
 UPROPERTY() int32 WindowAtMS=0;
 UPROPERTY() int32 WindowCredits=0;
 UPROPERTY() int32 WindowIncome=0;
 UPROPERTY() int32 WindowHireSpent=0;
 UPROPERTY() int32 NetPerMinute=0;
 // Identified vehicles only, from perception records. Never look up enemy live type/HP to update this.
 UPROPERTY() TMap<int32,int32> VehicleSeenMS;
};
