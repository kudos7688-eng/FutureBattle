#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace BattleKitLook
{
 struct FPropsResult {int32 Cubes=0, Instances=0, Missing=0;};
 // Called once by Apply, BEFORE surface materials are replaced. Never changes the source geometry.
 FPropsResult Dress(UWorld* World);
}
