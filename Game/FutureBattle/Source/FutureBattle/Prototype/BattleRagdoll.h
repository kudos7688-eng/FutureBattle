#pragma once
class USkeletalMeshComponent;

// Corpse fall (2026-11-21): the Mannequin death clips are the first second of a fall made for a ragdoll; their last frame still
// stands bent over (pelvis 85 cm up). When the clip ends the body falls under physics against the static world only (nothing
// queries it, no unit collides with it), then holds where it came to rest: the tick stops first, so the pose is never evaluated
// again, then physics stops. Display only. -BattleRagdoll=0 holds the clip's last frame instead (the earlier look).
namespace BattleRagdoll
{
 bool Enabled();
 constexpr double SettleSeconds=2.5;
 bool Start(USkeletalMeshComponent* Mesh); // false: no physics asset / bodies, the caller freezes the clip as before
 void Settle(USkeletalMeshComponent* Mesh);
}
