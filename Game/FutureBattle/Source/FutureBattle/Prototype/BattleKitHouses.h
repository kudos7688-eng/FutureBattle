#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace BattleKitLook
{
 // Called after surface replacement. Adds collision-free parts and shared wall tints only.
 void DressHouses(UWorld* World);
}
