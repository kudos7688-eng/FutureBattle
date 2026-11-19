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
//   BattleBodyAnimImport.cpp) while the blind fire lasts; without them the crouch clip stays.
namespace BattleBlindFire
{
 bool PlayerEnabled();
 bool AIEnabled();
 // Spread multiplier of a blind shot (replaces the stance and aim multipliers). -BattleBlindFireSpread=, default 4.
 float SpreadScale();
 // Counterplay (2026-11-20, user decision): over the top the head comes up over the cover while the blind fire clip shows
 // (ClipHoldSeconds after each shot). For that time sight aims at, and bullets stop at, the head instead of the crouched
 // body (heights above the capsule centre, which stands 90 cm above the ground). Around the edge the body stays hidden.
 // The head that shows also looks (user decision, 2026-11-20): the shooter's eye is at the head for that time, so he can
 // spot over the cover what a standing soldier would (the standing eye, 60 cm above the capsule centre).
 // -BattleBlindFireExpose=0 restores the fully hidden blind fire (the hands then lift the rifle alone).
 bool ExposeEnabled();
 constexpr float ExposedAimOffset=60.f,ExposedTopOffset=80.f,ExposedEyeOffset=60.f;
 // How long the blind fire clip stays after a shot.
 constexpr double ClipHoldSeconds=.6;
 // Low cover (the soldier stands up in place to fire, so the peek point is the anchor): blind fire goes over the top.
 // Otherwise it goes around the edge toward the peek point.
 bool IsOverTop(const FBattleUnitState& U);
 // nullptr when the clip is not imported or -BattleStanceAnim=0.
 UAnimSequence* Clip(bool bOverTop);
 // Over the top the clip shows only above spine_01 over the hidden pose, so its chest turns with the hidden hips. Mesh yaw
 // (degrees, right positive) that brings the hips to the over-top clip's own facing, so the rifle points over the cover:
 // measured hip yaw (BATTLE_BODY_CLIP 2026-11-20) of AS_CoverLo_AimU_down 64 minus that of the hidden clip.
 float OverTopYaw(const TCHAR* HiddenClip);
 // Display: world Z of the top of the cover below an over-the-top origin (one line straight down, static geometry only);
 // without a hit, 45 cm above the body's capsule centre (about the top of the 130 cm low walls).
 float CoverTop(UWorld* World,const FVector& Origin,const FVector& Body);
}
