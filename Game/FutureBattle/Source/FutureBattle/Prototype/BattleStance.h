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
}
