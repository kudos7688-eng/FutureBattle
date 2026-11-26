#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleTeamAI.generated.h"

// Team coordination consumes squad reports, never enemy state/body data.
// BattleMatch owns objective allocation; BattleSquadTactics owns local execution.
UCLASS()
class UBattleTeamAI : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 void Step();
 void Reset() { NextAssignment=0; RetryAfter.Reset(); }
 bool IsEnabled() const { return bEnabled; }
private:
 bool bEnabled=true;
 double NextAssignment=0;
 TMap<int32,double> RetryAfter;
};
