#pragma once
#include "CoreMinimal.h"

class UWorld;

// Battle effects (2026-11-22): explosions (anti-tank rounds, the exoskeleton launcher, tank and artillery shells; the
// explosion carries its own flash of light). Display only: nothing here writes to the battle. Off when nothing is rendered
// (-nullrhi) or with -BattleFX=0. Sources: Docs/ASSET_SOURCES_AUDIO_FX.md. Bullet impacts have a sound (BattleAudio) but no
// effect yet: the TPS impact effect tried first did not show (2026-11-22).
namespace BattleEffects
{
 // True while explosions are shown as effects; the old wire spheres of the blast radius are drawn only when this is false.
 bool Active(UWorld* World);
 // Loads the effects at the start of a battle (otherwise the first use loads them).
 void Preload(UWorld* World);
 // Returns true when an effect was started (false: not rendering, switched off, far, behind the camera, or over the
 // per-frame budget). Size 1 = an anti-tank round; about 2.5 = a self-propelled gun shell.
 bool Explosion(UWorld* World,const FVector& At,float Size);
}
