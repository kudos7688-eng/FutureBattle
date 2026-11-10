#pragma once
#include "CoreMinimal.h"
class USkeletalMeshComponent;
class UAnimSingleNodeInstance;
// Per-mesh ownership of the original engine policy; no shared animation time.
struct FBattleAnimTasks
{
 TWeakObjectPtr<UAnimSingleNodeInstance> Node;
 bool OriginalParallel=true;
 void Apply(USkeletalMeshComponent* Mesh,bool Enabled);
};
