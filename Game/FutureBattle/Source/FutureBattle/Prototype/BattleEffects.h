#pragma once
#include "CoreMinimal.h"

class UWorld;

// Battle effects (2026-11-22): dust and sparks where bullets hit the ground or walls, and explosions (anti-tank rounds, the
// exoskeleton launcher, tank and artillery shells; the explosion carries its own flash of light). Display only: nothing here
// writes to the battle. Off when nothing is rendered (-nullrhi) or with -BattleFX=0. Sources: Docs/ASSET_SOURCES_AUDIO_FX.md.
namespace BattleEffects
{
 // True while explosions are shown as effects; the old wire spheres of the blast radius are drawn only when this is false.
 bool Active(UWorld* World);
 // Loads the effects at the start of a battle (otherwise the first use loads them).
 void Preload(UWorld* World);
 // Both return true when an effect was started (false: not rendering, switched off, far, behind the camera, or over the
 // per-frame budget).
 bool BulletImpact(UWorld* World,const FVector& At,const FVector& Normal);
 // Size 1 = an anti-tank round; about 2.5 = a self-propelled gun shell.
 bool Explosion(UWorld* World,const FVector& At,float Size);
}
