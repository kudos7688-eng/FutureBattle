#pragma once
#include "CoreMinimal.h"

struct FBattleUnitState;
class UAnimSequence;

// Blind fire (2026-11-19, user request): a soldier hidden behind cover (CoverPhase 2) fires over a low cover or around the
// edge of a wide one without leaving it. The body stays crouched where it was, so sight and hits on him are unchanged; only
// the shot leaves from above or beside the cover, with a wide spread (the shooter cannot look along the barrel).
// - Player: firing without aiming blind-fires; aiming and firing peeks as before. -BattleBlindFire=0 restores the old peek.
// - AI: trial -BattleAIBlindFire=1 (default 0): a pinned soldier fires blind at the last known position instead of waiting.
// - Display: the clips imported from the cover shooter sample (/Game/Characters/Mannequins/Anims/Rifle/Cover/, see
//   BattleCoverAnimImport.cpp) while the blind fire lasts; without them the crouch clip stays.
namespace BattleBlindFire
{
 bool PlayerEnabled();
 bool AIEnabled();
 // Spread multiplier of a blind shot (replaces the stance and aim multipliers). -BattleBlindFireSpread=, default 4.
 float SpreadScale();
 // How long the blind fire clip stays after a shot.
 constexpr double ClipHoldSeconds=.6;
 // Low cover (the soldier stands up in place to fire, so the peek point is the anchor): blind fire goes over the top.
 // Otherwise it goes around the edge toward the peek point.
 bool IsOverTop(const FBattleUnitState& U);
 // nullptr when the clip is not imported or -BattleStanceAnim=0.
 UAnimSequence* Clip(bool bOverTop);
}
