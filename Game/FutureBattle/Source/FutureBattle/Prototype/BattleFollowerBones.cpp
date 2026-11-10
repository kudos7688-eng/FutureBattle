#include "BattleFollowerBones.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"

bool BattleFollowerBones::Eligible(USkeletalMeshComponent* Mesh)
{
 auto* Node=Mesh->GetSingleNodeInstance();
 const auto* Clip=Node?Cast<UAnimSequence>(Node->GetCurrentAsset()):nullptr;
 // Stateful post-process, root motion and bone-dependent notifies need their pose.
 return Clip && (!Clip->HasRootMotion() || !Node->ShouldExtractRootMotion()) && Clip->Notifies.IsEmpty() && !Mesh->GetPostProcessInstance()
  && !Mesh->bCastHiddenShadow && Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
}
void BattleFollowerBones::SetVisible(USkeletalMeshComponent* Mesh,bool Show,bool Enabled)
{
 if(Enabled)
 {
  const bool WasReduced=Mesh->VisibilityBasedAnimTickOption==EVisibilityBasedAnimTickOption::AlwaysTickPose;
  const bool Reduce=!Show && Mesh->IsComponentTickEnabled() && Eligible(Mesh);
  Mesh->VisibilityBasedAnimTickOption=Reduce?EVisibilityBasedAnimTickOption::AlwaysTickPose:EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  if(WasReduced && !Reduce && Mesh->IsComponentTickEnabled())
  {
   Mesh->CompleteParallelAnimationEvaluation(true);
   // Evaluate the current animation time before visibility / attachments are exposed.
   // Do not advance animation time a second time.
   Mesh->RefreshBoneTransforms(nullptr);
  }
 }
 Mesh->SetVisibility(Show);
}
void BattleFollowerBones::PrepareFreeze(USkeletalMeshComponent* Mesh,bool Enabled)
{
 if(Enabled && Mesh->VisibilityBasedAnimTickOption==EVisibilityBasedAnimTickOption::AlwaysTickPose)
 {
  Mesh->CompleteParallelAnimationEvaluation(true);
  Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  Mesh->RefreshBoneTransforms(nullptr);
 }
}
