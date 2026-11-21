#include "BattleStance.h"
#include "BattleBlindFire.h"
#include "BattleState.h"
#include "BattleBodyAnim.h"
#include "BattleFollowerBones.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleUnit.h"
#include "BattleVATDisplay.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "FutureBattle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

bool BattleStance::Enabled(const UWorld* World)
{
 // Resolved once per world, like the range profile.
 static TWeakObjectPtr<const UWorld> ResolvedFor;static bool bResolved=false,bEnabled=false;
 if(!bResolved||ResolvedFor.Get()!=World)
 {
  bResolved=true;ResolvedFor=World;int32 Mode=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleStances="),Mode);
  bEnabled=Mode<0?BattleFieldLayout::IsUrban(World):Mode!=0;
 }
 return bEnabled;
}
// A crouched soldier is 110cm tall (eye at 90cm), so the 130cm low walls of map E hide and protect him, while a standing
// soldier (eye at 150cm) sees and fires over them. With the first values (120cm) a 120cm wall hid nobody.
float BattleStance::EyeOffset(uint8 Stance){return Stance==Prone?-50.f:Stance==Crouch?0.f:60.f;}
float BattleStance::AimOffset(uint8 Stance){return Stance==Prone?-60.f:Stance==Crouch?-10.f:35.f;}
float BattleStance::TopOffset(uint8 Stance){return Stance==Prone?-35.f:Stance==Crouch?20.f:95.f;}
float BattleStance::SpreadScale(uint8 Stance){return Stance==Prone?.6f:Stance==Crouch?.8f:1.f;}
float BattleStance::VisualScale(uint8 Stance){return Stance==Prone?.3f:Stance==Crouch?.65f:1.f;}
bool BattleStance::AnimEnabled()
{
 // Default on since 2026-11-17 (user decision); -BattleStanceAnim=0 restores the squashed placeholder.
 static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleStanceAnim="),Mode);return Mode!=0;}();
 return bOn;
}
UAnimSequence* BattleStance::CrouchClip(bool bWalk)
{
 if(!AnimEnabled())return nullptr;
 static TStrongObjectPtr<UAnimSequence> Idle,Walk;static bool bLoaded=false;
 if(!bLoaded)
 {
  bLoaded=true;
  Idle.Reset(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Idle")));
  Walk.Reset(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Walk_Fwd")));
  UE_LOG(LogTemp,Display,TEXT("BATTLE_STANCE_ANIM idle=%d walk=%d"),Idle.IsValid(),Walk.IsValid());
 }
 return bWalk&&Walk.IsValid()?Walk.Get():Idle.Get();
}
// With a clip for the stance (crouch: Lyra; prone: the cover shooter sample, BattleBodyAnim) the body keeps its full size.
float BattleStance::BodyScale(uint8 Stance){return (Stance==Crouch&&CrouchClip(false))||(Stance==Prone&&BattleBodyAnim::Get(BattleBodyAnim::EClip::ProneIdle))?1.f:VisualScale(Stance);}

bool UBattleLeaderRuntime::SetStance(int32 ID,uint8 Stance,double Now,bool bForce)
{
 if(!BattleStance::Enabled(GetWorld()))return false;
 const uint8 Current=StanceOf(ID);
 if(Current==Stance||Stance>BattleStance::Prone)return false;
 if(!bForce&&Now<StanceLockedUntil.FindRef(ID))return false;
 Stances.Add(ID,Stance);
 // Changing takes time; getting down to or up from prone takes longer.
 StanceLockedUntil.Add(ID,Now+((Current==BattleStance::Prone||Stance==BattleStance::Prone)?1.2:.5));
 // Placeholder visual: squash the body mesh vertically (feet stay on the ground). Far VAT instances are not changed.
 if(const auto* B=Bodies.Find(ID))
 {
  const FVector Scale(1,1,BattleStance::BodyScale(Stance));
  // A soldier with his own look keeps the squashed body when prone, unless his mesh plays the shared prone clips (AC01 suit).
  if(B->Actor.IsValid()){B->Actor->VisualStance=Stance;if(auto* Mesh=B->Actor->GetMesh())Mesh->SetRelativeScale3D(B->Actor->HasLook()&&!B->Actor->LookHasProneClip()&&Stance==BattleStance::Prone?FVector(1,1,BattleStance::VisualScale(Stance)):Scale);}
  else if(B->Mesh.IsValid())
  {
   B->Mesh->SetRelativeScale3D(Scale);
   // Body animation: a near follower plays the stance transition once (StepBodyTimers returns him to his loop).
   UAnimSequence* Change=B->Mesh->IsComponentTickEnabled()&&!(VAT&&VAT->IsFar(ID))?BattleBodyAnim::Transition(Current,Stance):nullptr;
   if(Change){B->Mesh->PlayAnimation(Change,false);BodyTimers.Add(ID,{Now+Change->GetPlayLength(),false});}
   else if(BattleStance::AnimEnabled())RefreshFollowerPose(ID);
  }
 }
 SightExperiment.Cover(Stance==BattleStance::Prone?TEXT("stance_to_prone"):Stance==BattleStance::Crouch?TEXT("stance_to_crouch"):TEXT("stance_to_stand"));
 return true;
}

UAnimSequence* UBattleLeaderRuntime::FollowerClip(int32 ID,int32 Pose) const
{
 // Blind fire (BattleBlindFire.cpp): the clip that raises the rifle over or around the cover, while it lasts.
 if(Pose!=2 && IsBlindFiring(ID,GetWorld()->GetTimeSeconds()))
  if(const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID))
  {
   // Over the top the clip is a standing body holding the rifle above the head: shown while the rules expose the head
   // (BattleBlindFire::ExposeEnabled); otherwise a follower (one whole-body clip, no upper layer) keeps its hidden pose.
   if(BattleBlindFire::IsOverTop(*U)){if(BattleBlindFire::ExposeEnabled())if(auto* Top=BattleBlindFire::Clip(true))return Top;}
   else
   {
    // Around the edge: the side's own crouched aim of the cover set (AimL or AimR), else the default side clip.
    if(const auto* B=Bodies.Find(ID);B && B->CoverClipPhase)if(auto* Aim=BattleBodyAnim::CoverClip(B->CoverClipSet.Aim))return Aim;
    if(auto* Blind=BattleBlindFire::Clip(false))return Blind;
   }
  }
 // Cover clips (BattleCoverClips.h): hidden or firing loop while the follower's cover action is in that phase.
 if(Pose!=2)if(const auto* B=Bodies.Find(ID);B && B->CoverClipPhase)
  if(auto* Cover=BattleBodyAnim::CoverClip(B->CoverClipPhase==2?B->CoverClipSet.Hidden:B->CoverClipSet.Aim))return Cover;
 if(StanceOf(ID)==BattleStance::Crouch)if(auto* Crouch=BattleStance::CrouchClip(Pose==1))return Crouch;
 if(StanceOf(ID)==BattleStance::Prone)
  if(auto* Prone=BattleBodyAnim::Get(Pose==2?BattleBodyAnim::EClip::ProneReload:Pose==1?BattleBodyAnim::EClip::ProneFwd:BattleBodyAnim::EClip::ProneIdle))return Prone;
 // Moving without firing: the rifle held low in a jog (ShowsRun); firing on the move keeps the aimed walk.
 if(ShowsRun(ID,Pose,GetWorld()->GetTimeSeconds()))if(auto* Jog=BattleBodyAnim::Get(BattleBodyAnim::EClip::StandJog))return Jog;
 return Pose==2?ReloadPose.Get():Pose==1?WalkPose.Get():IdlePose.Get();
}

void UBattleLeaderRuntime::StepBodyTimers()
{
 const double Now=GetWorld()->GetTimeSeconds();
 if(BattleStance::AnimEnabled() && Now>=NextPoseAudit){NextPoseAudit=Now+1.;AuditFollowerPoses(Now);}
 if(BodyTimers.IsEmpty())return;
 for(auto It=BodyTimers.CreateIterator();It;++It)
 {
  if(Now<It.Value().At)continue;
  const int32 ID=It.Key();const bool bFreeze=It.Value().bFreeze;It.RemoveCurrent();
  const auto* B=Bodies.Find(ID);
  if(!B||B->Actor.IsValid()||!B->Mesh.IsValid())continue;
  if(bFreeze)
  {
   // Frame skipping (-BattleFollowerAnimSkip/-BattleFollowerAnimBudget) may still owe the clip some time: put the death clip on
   // its last frame before the tick stops, or the body would hold a pose halfway down.
   USkeletalMeshComponent* M=B->Mesh.Get();M->HandleExistingParallelEvaluationTask(true,true);
   if(auto* V=Cast<UBattleVATSkeletalMeshComponent>(M))V->EndAnimSkip();
   if(auto* Node=M->GetSingleNodeInstance())if(const auto* Clip=Cast<UAnimSequenceBase>(Node->GetAnimationAsset()))
   {Node->SetPosition(Clip->GetPlayLength(),false);M->TickAnimation(0,false);M->RefreshBoneTransforms(nullptr);}
   BattleFollowerBones::PrepareFreeze(M,bReduceHiddenBones);M->SetComponentTickEnabled(false);
  }
  else RefreshFollowerPose(ID);
 }
}

void UBattleLeaderRuntime::AuditFollowerPoses(double Now)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(const auto& Pair:Bodies)
 {
  const int32 ID=Pair.Key;const auto& B=Pair.Value;
  if(B.Actor.IsValid()||!B.Mesh.IsValid()||BodyTimers.Contains(ID)||!B.Mesh->IsComponentTickEnabled()||(VAT&&VAT->IsFar(ID)))continue;
  const auto* U=Store->Find(ID);if(!U||!U->IsAlive())continue;
  const UAnimationAsset* Shown=B.Mesh->GetSingleNodeInstance()?B.Mesh->GetSingleNodeInstance()->GetAnimationAsset():nullptr;
  UAnimSequence* Want=FollowerClip(ID,B.Pose);
  if(!Want||Shown==Want)continue;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BODY_POSE_RESET unit=%d shown=%s want=%s stance=%d cover=%d"),ID,Shown?*Shown->GetName():TEXT("none"),*Want->GetName(),int32(StanceOf(ID)),int32(U->CoverPhase));
  RefreshFollowerPose(ID);
 }
}

void UBattleLeaderRuntime::RefreshFollowerPose(int32 ID)
{
 const auto* B=Bodies.Find(ID);
 if(!B||B->Actor.IsValid()||!B->Mesh.IsValid()||(VAT&&VAT->IsFar(ID))||!B->Mesh->IsComponentTickEnabled())return;
 B->Mesh->HandleExistingParallelEvaluationTask(true,true);
 B->Mesh->PlayAnimation(FollowerClip(ID,B->Pose),true);
}

void UBattleLeaderRuntime::UpdateAIStance(int32 ID,double Now,bool bMoving,bool bCombat,bool bLostSight)
{
 if(!BattleStance::Enabled(GetWorld()))return;
 const uint8 Current=StanceOf(ID);
 // Pinned by suppression: get as low as possible, whatever the order says (own state only).
 if(IsPinned(ID,Now)){SetStance(ID,BattleStance::Prone,Now);return;}
 if(bMoving||!bCombat){NoCoverProneUntil.Remove(ID);SetStance(ID,BattleStance::Stand,Now);return;}
 if(bLostSight)
 {
  // The remembered target is no longer visible from down here: stand up and look for a few seconds.
  if(Current!=BattleStance::Stand&&SetStance(ID,BattleStance::Stand,Now)){StanceLockedUntil.Add(ID,Now+3.);SightExperiment.Cover(TEXT("stance_stood_to_look"));}
  return;
 }
 // -BattleNoCoverFallback: no cover and no better place found - lie down while he still sees his target.
 if(const double* T=NoCoverProneUntil.Find(ID)){if(*T>Now){SetStance(ID,BattleStance::Prone,Now);return;}NoCoverProneUntil.Remove(ID);}
 SetStance(ID,BattleStance::Crouch,Now);
}
