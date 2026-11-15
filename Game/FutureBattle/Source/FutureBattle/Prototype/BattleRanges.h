#pragma once
#include "CoreMinimal.h"
#include "BattleState.h"

// Range profile (2026-11-14 decision). Active on map E by default; maps A-D keep the single shared 100m sight / 50m weapon range.
// It separates three distances that used to be one value:
//   Sight  - how far a regular soldier perceives (training still scales it);
//   Engage - how far AI opens fire with a weapon (AI cognition; also what "in weapon range" means for AI decisions);
//   Reach  - how far a bullet travels, identical for AI and direct control, so a player may fire at anything visible.
// -BattleRangeProfile=0/1 forces it off/on for any map. Overrides (cm): -BattleRangeSight= / -BattleRangeEngage= /
// -BattleRangeEngageSupport= / -BattleRangeReach= / -BattleRangeReachSupport=.
// While active it takes precedence over the older trial overrides -BattleSightRange= and -BattleWeaponRange=.
namespace BattleRanges
{
 struct FProfile { bool bActive=false; float Sight=20000.f, EngageCarbine=15000.f, EngageSupport=25000.f, ReachCarbine=50000.f, ReachSupport=60000.f; };
 const FProfile& Get();
 float Engage(EBattleWeapon Weapon);
 float Reach(EBattleWeapon Weapon);
}
