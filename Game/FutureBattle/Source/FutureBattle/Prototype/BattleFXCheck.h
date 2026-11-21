#pragma once
#include "CoreMinimal.h"

class ABattleController;

// Sound and effects check (2026-11-22): -BattleFXCheck -BattleWideScreens=<prefix> on a wide map, rendered and with sound
// (Setup/Test-SoundFX.ps1). Pauses the battle and, in front of a fixed camera, plays rifle and machine-gun shots, bullet
// impacts, explosions of three sizes, the launches, the incoming whistle and the match cues; takes screenshots
// <prefix>-*.png and records the game's audio mix to <prefix>-audio.wav. The speakers stay silent: the main output is
// turned down after the point where the recording is taken. Logs FX_CHECK PASS/FAIL lines and FX_CHECK_RESULT, then quits.
namespace BattleFXCheck
{
 // Every frame from the player controller; does nothing without -BattleFXCheck.
 void Tick(ABattleController* Controller);
}
