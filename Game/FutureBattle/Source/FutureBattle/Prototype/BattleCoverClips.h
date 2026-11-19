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
    // Over the top. The hidden pose stays on the right; the UE5 set has no plain AimU loop, so AimU_down is the candidate
    // (the blind fire default too). If its head height is well below the standing eye (the firing point stays at standing
    // height in the rules), keep the ordinary standing aim for phase 4 instead (Docs/COVER_CLIPS.md, question 1).
    default:          return {TEXT("AS_coverlo_R_idle"),TEXT("AS_CoverLo_Idle2CoverR"),TEXT("AS_CoverLo_CoverR2AimU"),TEXT("AS_CoverLo_AimU_down"),TEXT("AS_CoverLo_AimU2CoverR")};
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
 // threat; the hidden clips have the back to the cover, so they are turned until the measured clip forward lines up with the
 // cover. Initial guesses until the import logs BATTLE_BODY_CLIP (pelvis and gun direction per clip): hidden and enter 180
 // (back to the wall, facing away from the threat), aim 0. Replace with the measured values (Docs/COVER_CLIPS.md, step 2).
 inline float YawOffset(uint8 CoverPhase)
 {
  return CoverPhase==2?180.f:0.f;
 }
}
