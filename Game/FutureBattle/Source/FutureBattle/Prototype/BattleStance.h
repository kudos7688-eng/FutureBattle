#pragma once
#include "CoreMinimal.h"

class UWorld;

// Stances (2026-11-14 decision; map E default, -BattleStances=0/1 forces off/on). Standing, crouching and prone change
// physical heights measured from the body centre (90cm above the ground): where the soldier's eye is, where others see and
// aim at him, and above which height a bullet passes over him. So a 1.2m wall hides and protects a crouched soldier.
// Shared tactical effect: weapon spread shrinks when crouched or prone (AI and direct control alike).
// AI cognition: stand while moving, crouch when stopped in combat, stand up for a few seconds when the target is lost from
// the lower eye height, go prone when pinned by suppression. Visuals are a placeholder (the body is squashed vertically).
// Runtime only: not saved, a loaded battle starts standing.
namespace BattleStance
{
 enum : uint8 { Stand=0, Crouch=1, Prone=2 };
 bool Enabled(const UWorld* World);
 float EyeOffset(uint8 Stance);    // 60 / 0 / -50
 float AimOffset(uint8 Stance);    // 35 / -10 / -60
 float TopOffset(uint8 Stance);    // 95 / 20 / -35
 float SpreadScale(uint8 Stance);  // 1 / 0.8 / 0.6
 float VisualScale(uint8 Stance);  // 1 / 0.65 / 0.3
 // Stance visuals (2026-11-17, default on by user decision; -BattleStanceAnim=0 turns it off): a crouch plays Lyra's rifle crouch clips
 // (/Game/Characters/Mannequins/Anims/Rifle/Crouch/) at full body scale instead of the squashed body. Prone has no clip
 // and keeps the squash. Display only: heights, sight and hits are unchanged. Far VAT followers still show the standing clip.
 bool AnimEnabled();
 class UAnimSequence* CrouchClip(bool bWalk); // nullptr when the trial is off or the asset is missing
 float BodyScale(uint8 Stance);               // VisualScale, but 1 for a crouch shown by the clip
 // Moving in a low stance (2026-11-22, user decision): -BattleStanceMove=N.
 //  0: moving stands every soldier up (the rule before).
 //  1 (default): the directly controlled soldier keeps his crouch or prone stance while he walks (X key), at MoveScale of it.
 //     AI soldiers are unchanged (they stand up to move), so matches without a player are unchanged.
 //  2 (trial): AI soldiers too: on the move under fire or with an enemy in sight a soldier stays crouched (until 3 s after the
 //     last threat), and a pinned soldier crawls on along his way (before, he waited prone until he could stand), each at
 //     MoveScale of his stance.
 // Heights, sight, hits and spread follow the stance as when standing still (a crouched walker hides behind a 1.2 m wall).
 int32 MoveMode();
 // Trial -BattleStanceSteady=1 (default 0, 2026-11-23, user: soldiers crouch and stand again and again on the spot, which looks
 // bad). Two sources of that (map I default match: 19,800 crouches in 15 min): (1) in the open, a soldier crouched, lost his
 // enemy from the lower eye, stood up to look, saw him and crouched again (9,350 "stood to look"); with the trial, once crouching
 // hid his enemy at a place he stays standing there (within 2.5 m). (2) Behind cover, he stood up to peek every 0.8 s hidden,
 // and 72% of the peeks ended with no shot (no enemy seen); with the trial, after a peek with no shot he stays down 3 s.
 bool Steady();
 float MoveScale(uint8 Stance);    // 1 / 0.55 / 0.25 of the walking speed
}
