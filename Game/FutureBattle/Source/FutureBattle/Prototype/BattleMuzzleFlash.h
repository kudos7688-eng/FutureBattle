#pragma once
#include "CoreMinimal.h"

// Display only: a short bright flash at a muzzle (2026-11-18). Never touches the battle state.
// Does nothing when nothing is rendered (-nullrhi) or with -BattleNoMuzzleFlash.
namespace BattleMuzzleFlash
{
 // Size 1 = a rifle (about 22 cm long); Seconds = how long it stays.
 void Show(UWorld* World,const FVector& Muzzle,const FVector& Direction,float Size=1.f,float Seconds=.06f);
}
