#include "BattleAnimTasks.h"
#include "BattleFollowerBones.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"

void FBattleAnimTasks::Apply(USkeletalMeshComponent* Mesh,bool Enabled)
{
 auto* Current=Mesh->GetSingleNodeInstance();
 if(Current!=Node.Get())
 {
  if(Node.IsValid()) Node->bUseMultiThreadedAnimationUpdate=OriginalParallel;
  Node=Current;
  if(Current) OriginalParallel=Current->bUseMultiThreadedAnimationUpdate;
 }
 if(!Current) return;
 // Only an update-only, explicitly fog-hidden follower. Visible bone evaluation,
 // animation clocks, asset-player arithmetic and component tick frequency stay intact.
 const bool Immediate=Enabled && !Mesh->IsVisible() && Mesh->IsComponentTickEnabled()
  && Mesh->VisibilityBasedAnimTickOption==EVisibilityBasedAnimTickOption::AlwaysTickPose
  && BattleFollowerBones::Eligible(Mesh);
 const bool Parallel=Immediate?false:OriginalParallel;
 if(Current->bUseMultiThreadedAnimationUpdate!=Parallel)
 {
  Mesh->CompleteParallelAnimationEvaluation(true);
  Current->bUseMultiThreadedAnimationUpdate=Parallel;
 }
}
