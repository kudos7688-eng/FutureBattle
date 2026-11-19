#pragma once
#include "CoreMinimal.h"

// Cover clip table (2026-11-19, user request; display only: no rule, sight line or timing reads it).
// Maps the cover action state (FBattleUnitState::CoverPhase, CoverAnchor, CoverPeek and the threat) to the cover shooter
// sample clips that the body animation import (BattleBodyAnimImport.cpp, blindfire branch) copies to
// /Game/Characters/Mannequins/Anims/Rifle/Cover/. Wiring into ABattleUnit::UpdateBodyAnim (leaders) and
// UBattleLeaderRuntime::FollowerClip (near followers) belongs to the body animation work; this header only holds the data.
// Design notes, open questions and the measurement still needed: Docs/COVER_CLIPS.md.
//
// CoverPhase: 0 none, 1 moving to the hidden anchor, 2 hidden, 3 moving to the peek point, 4 peeking (firing window).
namespace BattleCoverClips
{
 inline const TCHAR* Folder(){return TEXT("/Game/Characters/Mannequins/Anims/Rifle/Cover/");}

 // Low: the cover hides a crouched soldier but not a standing one. High: it hides a standing soldier too.
 enum class EHeight : uint8 {Low,High};
 // Which way the soldier comes out to fire. Up = over the top of low cover (the "stand up to fire" kind, CoverPeek == CoverAnchor).
 enum class ESide : uint8 {Left,Right,Up};

 struct FSet
 {
  const TCHAR* Hidden;   // phase 2, loops (back to the cover, looking toward the side it will come out of)
  const TCHAR* Enter;    // one-shot on reaching the anchor (phase 1 -> 2); nullptr = cross-fade straight into Hidden
  const TCHAR* ToAim;    // one-shot while leaving to fire (phase 3, and 2 -> 4 when the peek point is the anchor)
  const TCHAR* Aim;      // phase 4, loops
  const TCHAR* FromAim;  // one-shot when the firing window ends (4 -> 1/2)
 };

 // Every name below is in the import table of BattleBodyAnimImport.cpp (23 clips). Clips only in the UE4 cover/ folder
 // (AS_CoverLo_AimU, AS_coverlo_R_aim, AS_CoverHi_CoverL2CoverR, AS_CoverHi_WalkL1) are not used.
 // High cover has no "up": an Up request on high cover falls back to Right (see Pick).
 inline FSet Pick(EHeight Height,ESide Side)
 {
  if(Height==EHeight::High && Side==ESide::Up)Side=ESide::Right;
  if(Height==EHeight::Low)
  {
   switch(Side)
   {
    case ESide::Left: return {TEXT("AS_CoverLo_CoverL"),nullptr,TEXT("AS_CoverLo_CoverL2AimL"),TEXT("AS_CoverLo_AimL"),TEXT("AS_CoverLo_AimL2CoverL")};
    case ESide::Right:return {TEXT("AS_coverlo_R_idle"),TEXT("AS_CoverLo_Idle2CoverR"),TEXT("AS_CoverLo_CoverR2AimR"),TEXT("AS_CoverLo_AimR"),TEXT("AS_CoverLo_AimR2CoverR")};
    // Over the top. The hidden pose stays on the right. Phase 4 has no cover clip (nullptr): the ordinary standing rifle body
    // shows, as the rules fire from the standing eye. AimU_down was measured (BATTLE_BODY_CLIP 2026-11-20): a standing body
    // (head 158 cm) holding the rifle above the head (hand 168 cm) and pointing down, so it is the over-the-top blind fire
    // pose (upper body only, over the hidden pose; BattleBlindFire.cpp), not an aimed peek. CoverR2AimU ends standing (head 159).
    default:          return {TEXT("AS_coverlo_R_idle"),TEXT("AS_CoverLo_Idle2CoverR"),TEXT("AS_CoverLo_CoverR2AimU"),nullptr,TEXT("AS_CoverLo_AimU2CoverR")};
   }
  }
  if(Side==ESide::Left)return {TEXT("AS_CoverHi_CoverL"),nullptr,TEXT("AS_CoverHi_CoverL2AimL"),TEXT("AS_CoverHi_AimL"),TEXT("AS_CoverHi_AimL2CoverL")};
  return {TEXT("AS_cover_hi_r_idle"),TEXT("AS_CoverHi_Idle2CoverR"),TEXT("AS_CoverHi_CoverR2AimR"),TEXT("AS_CoverHi_AimR"),TEXT("AS_CoverHi_AimR2CoverR")};
 }

 // Side from the geometry the cover action already stores: the peek point's offset from the anchor, seen from the anchor
 // looking at the threat. Peek within 50 cm of the anchor = the stand-up kind (Up).
 inline ESide SideOf(const FVector& Anchor,const FVector& Peek,const FVector& Threat)
 {
  const FVector2D Offset(Peek-Anchor),ToThreat(Threat-Anchor);
  if(Offset.SizeSquared()<50.f*50.f || ToThreat.IsNearlyZero())return ESide::Up;
  return FVector2D::CrossProduct(ToThreat,Offset)>0.f?ESide::Right:ESide::Left; // UE: X forward, Y right, Z up
 }

 // Height: Up (stand-up kind) is low cover by construction (RequestCoverAction checks it). For a side peek the caller decides
 // once when the action starts: high when a standing soldier at the anchor has no terrain line to the threat
 // (!BattleTactics::HasTerrainLine(World, Anchor + standing eye, ThreatEye)); one trace per cover action, never per frame.
 inline EHeight HeightOf(ESide Side,bool bStandingLineBlocked)
 {
  return Side==ESide::Up || !bStandingLineBlocked?EHeight::Low:EHeight::High;
 }

 // Mesh yaw offset (degrees) added to the body's facing while a clip of this phase plays. The game keeps the body facing the
 // threat. Measured 2026-11-20 (BATTLE_BODY_CLIP chest_yaw against the square standing MM_Rifle_Fire = 0; the ordinary rifle
 // stance is +34): the clips turn the body themselves. Hidden: R idle +94 (low) / +92 (high), CoverL -130 / -151, i.e. already
 // side-on or back to the cover; aim: AimR +23 / +66, AimL +39 / +24, all facing the threat. So no extra turn (the initial
 // guess, hidden 180, would have turned the hidden body toward the threat).
 inline float YawOffset(uint8)
 {
  return 0.f;
 }
}
