#pragma once
#include "CoreMinimal.h"
class UWorld;
class UNavigationSystemV1;
namespace BattleCoverFire
{
 bool ArrivalFirePoint(UWorld* FireWorld,UNavigationSystemV1* FireNav,const FVector& FireGround,const FVector& FireDirection);
}
