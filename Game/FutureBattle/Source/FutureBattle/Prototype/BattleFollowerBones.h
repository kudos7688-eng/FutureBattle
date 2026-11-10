#pragma once
#include "CoreMinimal.h"
class USkeletalMeshComponent;
// Explicit fog-hidden followers only. No screen visibility or scheduling changes.
namespace BattleFollowerBones
{
 bool Eligible(USkeletalMeshComponent* Mesh);
 void SetVisible(USkeletalMeshComponent* Mesh,bool Show,bool Enabled);
 void PrepareFreeze(USkeletalMeshComponent* Mesh,bool Enabled);
}
