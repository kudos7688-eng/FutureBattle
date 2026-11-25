#pragma once
#include "CoreMinimal.h"

// Suppression trial (-BattleSuppression=1, default OFF). A shot whose path passes within Radius of an enemy soldier adds
// suppression to that soldier; it decays linearly. Shared tactical effect for AI and direct control: weapon spread is
// multiplied by 1+SpreadScale*level. AI cognition only: at or above PinLevel an AI soldier in cover stays hidden and an
// exposed peek ducks back (PinLevel>1 disables pinning). Runtime suppression state.
// Overrides: -BattleSuppressionRadius= / -BattleSuppressionPerShot= / -BattleSuppressionDecay= / -BattleSuppressionSpread= / -BattleSuppressionPin=.
// -BattleExoShieldSuppression=1 independently enables these effects for exoskeletons, removes shield immunity,
// and adds one PerShot impulse per direct hit. Shield recovery inhibition uses State.ShieldWait.
// Weapon role (2026-11-16 trial): -BattleSuppressionSupport=<scale> multiplies the suppression of support-gun shots (default 1).
namespace BattleSuppression
{
 struct FTuning { bool bEnabled=false; float Radius=200.f, PerShot=.12f, DecayPerSecond=.25f, SpreadScale=1.5f, PinLevel=.5f, SupportScale=1.f; };
 const FTuning& Tuning();
 // Smoke tests only: toggles the trial inside one process.
 FTuning& MutableTuning();
}
