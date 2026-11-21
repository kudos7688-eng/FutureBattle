#pragma once
#include "CoreMinimal.h"

class UWorld;
class ABattleController;

// Battle sounds (2026-11-22): gunfire, bullet impacts, near misses, explosions, launches, the player's weapon handling and
// the match cues. Display only: it reads the battle, never changes it, and draws its variations from its own random stream.
// Silent with -nosound (no audio device) or -BattleSFX=0. Where the sounds come from: Docs/ASSET_SOURCES_AUDIO_FX.md.
namespace BattleAudio
{
 enum class EShot : uint8 { Rifle, Heavy };
 enum class ECue : uint8 { RocketLaunch, MortarLaunch, CannonFire, CaptureFriendly, CaptureEnemy, Victory, Defeat };
 // Seconds before a shell lands at which its whistle starts (the length of the whistle sound).
 constexpr float IncomingLead=1.8f;

 bool Enabled(UWorld* World);
 // Loads the sounds at the start of a battle (otherwise the first shot loads them).
 void Preload(UWorld* World);
 // A shot fired at At. bPlayer: the soldier the player controls (louder, never thinned out).
 void Shot(UWorld* World,const FVector& At,EShot Kind,bool bPlayer);
 // A bullet hitting the ground or a wall (bBody=false) or a soldier / vehicle (bBody=true).
 void Impact(UWorld* World,const FVector& At,bool bBody);
 // A bullet from Start to End by ShooterTeam: a crack where it passes close to the soldier the player controls.
 void ShotPassing(UWorld* World,const FVector& Start,const FVector& End,int32 ShooterTeam,int32 VictimID);
 // An explosion at At; Size 1 = an anti-tank round, about 2.5 = a self-propelled gun shell.
 void Explosion(UWorld* World,const FVector& At,float Size);
 // An artillery shell about to land at At in SecondsToImpact (<= IncomingLead; the whistle starts part-way through).
 void Incoming(UWorld* World,const FVector& At,float SecondsToImpact=IncomingLead);
 // One-off sounds at a place (launches) or, with At = nullptr, for the player alone (match cues).
 void Cue(UWorld* World,ECue Which,const FVector* At=nullptr);
 // Every frame from the player controller: the listener of the command view, the controlled soldier's weapon handling
 // (aim, scope, reload, empty trigger) and the match cues (objectives taken or lost, victory, defeat).
 void TickPlayer(ABattleController* Controller,bool bTriggerHeld);
}
