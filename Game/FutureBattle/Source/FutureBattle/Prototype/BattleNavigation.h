#pragma once
#include "CoreMinimal.h"
class ABattleUnit;

// Complete paths only. Defense paths starting inside the radius must stay inside.
namespace BattleNavigation
{
	bool ResolveGoal(ABattleUnit* Unit, const FVector& Requested, FVector& Resolved);
	bool IsPathWithinDefense(ABattleUnit* Unit, const FVector& Goal);
	bool FormationGoal(ABattleUnit* Unit, const FVector& Center, const FVector& Offset, FVector& Resolved);
}
