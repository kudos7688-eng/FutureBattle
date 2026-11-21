#include "BattleStance.h"
#include "BattleBlindFire.h"
#include "BattleState.h"
#include "BattleBodyAnim.h"
#include "BattleFollowerBones.h"
#include "BattleRagdoll.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleUnit.h"
#include "BattleVATDisplay.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "FutureBattle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
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
int32 BattleStance::MoveMode()
{
 static const int32 Mode=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleStanceMove="),V);return FMath::Clamp(V,0,2);}();
 return Mode;
}
float BattleStance::MoveScale(uint8 Stance){return Stance==Prone?.25f:Stance==Crouch?.55f:1.f;}

bool UBattleLeaderRuntime::SetStance(int32 ID,uint8 Stance,double Now,bool bForce,bool bOnTheMove)
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
   if(Change)
   {
    // On the move (he walks off as the stance changes) the transition ends within MovingTransitionSeconds: at its own speed it
    // slid along the ground. Until it ends his walk does not replace it (the pose change in Follow waits for this timer).
    const float Rate=bOnTheMove?FMath::Max(1.f,float(Change->GetPlayLength()/BattleBodyAnim::MovingTransitionSeconds)):1.f;
    B->Mesh->PlayAnimation(Change,false);if(Rate>1.f)B->Mesh->SetPlayRate(Rate);
    BodyTimers.Add(ID,{Now+Change->GetPlayLength()/Rate,false});
   }
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
 TArray<int32> Falling;
 for(auto It=BodyTimers.CreateIterator();It;++It)
 {
  if(Now<It.Value().At)continue;
  const int32 ID=It.Key();const bool bFreeze=It.Value().bFreeze,bSettle=It.Value().bSettle;It.RemoveCurrent();
  const auto* B=Bodies.Find(ID);
  if(!B||B->Actor.IsValid()||!B->Mesh.IsValid())continue;
  if(bSettle)BattleRagdoll::Settle(B->Mesh.Get());
  else if(bFreeze)
  {
   // Frame skipping (-BattleFollowerAnimSkip/-BattleFollowerAnimBudget) may still owe the clip some time: put the death clip on
   // its last frame before the tick stops, or the body would hold a pose halfway down.
   USkeletalMeshComponent* M=B->Mesh.Get();M->HandleExistingParallelEvaluationTask(true,true);
   if(auto* V=Cast<UBattleVATSkeletalMeshComponent>(M))V->EndAnimSkip();
   if(auto* Node=M->GetSingleNodeInstance())if(const auto* Clip=Cast<UAnimSequenceBase>(Node->GetAnimationAsset()))
   {Node->SetPosition(Clip->GetPlayLength(),false);M->TickAnimation(0,false);M->RefreshBoneTransforms(nullptr);}
   // The clip ends bent over, still standing: the body falls the rest of the way under physics, then holds.
   if(BattleRagdoll::Start(M))Falling.Add(ID);
   else{BattleFollowerBones::PrepareFreeze(M,bReduceHiddenBones);M->SetComponentTickEnabled(false);}
  }
  else RefreshFollowerPose(ID);
 }
 for(int32 ID:Falling)BodyTimers.Add(ID,{Now+BattleRagdoll::SettleSeconds,false,true});
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

// The follower's upper-body bend for the rifle's pitch (the mesh subclass of the default far display; none without it).
static void SetSpinePitch(USkeletalMeshComponent* Mesh,float Pitch){if(auto* V=Cast<UBattleVATSkeletalMeshComponent>(Mesh))V->SpinePitch=Pitch;}

void UBattleLeaderRuntime::AimFollowerRifles(double Now)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const float Dt=GetWorld()->GetDeltaSeconds();
 for(auto& Pair:Bodies)
 {
  const int32 ID=Pair.Key;auto& B=Pair.Value;
  if(B.Actor.IsValid()||!B.Mesh.IsValid()||!B.Weapon.IsValid()||!B.Mesh->IsComponentTickEnabled()||(VAT&&VAT->IsFar(ID)))continue;
  const auto* U=Store->Find(ID);if(!U||!U->IsAlive())continue;
  // Cover clips and blind fire turn the mesh themselves (BattleCoverAction.cpp); leave it to them.
  if(B.CoverClipPhase||B.bBlindPose){if(B.MuzzlePitch!=0.f)SetSpinePitch(B.Mesh.Get(),0.f);B.MuzzleYaw=B.MuzzlePitch=0.f;continue;}
  // Moving into or out of cover without a cover clip yet, the turn is dropped here (the cover code may not set the rotation).
  if(U->CoverPhase)
  {
   if(B.MuzzleYaw!=0.f||B.MuzzlePitch!=0.f){B.MuzzleYaw=B.MuzzlePitch=0.f;SetSpinePitch(B.Mesh.Get(),0.f);B.Mesh->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));}
   continue;
  }
  float WantYaw=0.f,WantPitch=0.f;
  if(Now-LastFireTimes.FindRef(ID,-100.)<BattleBodyAnim::ReadySeconds && !B.bRunPose && B.Pose!=2 && StanceOf(ID)!=BattleStance::Prone)
  {
   // The barrel (the weapon model's local -Y, posed by the last evaluated frame under the current mesh turn) against the
   // facing across and the last shot's pitch up and down.
   const FVector Barrel=-B.Weapon->GetComponentTransform().GetUnitAxis(EAxis::Y);
   const FVector Facing=B.Capsule.IsValid()?B.Capsule->GetForwardVector():Forward(ID);
   const FVector* Shot=FollowerAims.Find(ID);
   // The bend the shown pose was made with (the barrel above is from that pose; evaluation may skip frames, so the angle
   // asked for since then is not on the screen yet and adding to it would overshoot).
   const auto* Bend=Cast<UBattleVATSkeletalMeshComponent>(B.Mesh.Get());
   const float ShotPitch=Shot?FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Shot->Z,-1.,1.))):0.f;
   const float BarrelPitch=FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Barrel.Z,-1.,1.)));
   WantYaw=Barrel.SizeSquared2D()>.01?FMath::Clamp(B.MuzzleYaw+FMath::FindDeltaAngleDegrees(Barrel.Rotation().Yaw,Facing.Rotation().Yaw),-MaxFollowerMuzzleYaw,MaxFollowerMuzzleYaw):B.MuzzleYaw;
   if(Bend)WantPitch=FMath::Clamp(Bend->AppliedSpinePitch+(ShotPitch-BarrelPitch),-MaxFollowerMuzzlePitch,MaxFollowerMuzzlePitch);
  }
  const float Yaw=FMath::FInterpConstantTo(B.MuzzleYaw,WantYaw,Dt,360.f),Pitch=FMath::FInterpConstantTo(B.MuzzlePitch,WantPitch,Dt,180.f);
  if(FMath::IsNearlyEqual(Yaw,B.MuzzleYaw,.05f) && FMath::IsNearlyEqual(Pitch,B.MuzzlePitch,.05f))continue;
  B.MuzzleYaw=Yaw;B.MuzzlePitch=Pitch;
  // Across: the whole mesh turns about the capsule's vertical axis (it hangs 90 cm below the centre, its forward turned by -90).
  // Up and down: only the upper body bends (UBattleVATSkeletalMeshComponent::SpinePitch); the legs and feet keep the clip's pose.
  B.Mesh->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,Yaw-90.f,0));
  SetSpinePitch(B.Mesh.Get(),Pitch);
 }
}

void UBattleLeaderRuntime::ClearFollowerRifleTurn(int32 ID)
{
 auto* B=Bodies.Find(ID);
 if(!B||B->Actor.IsValid()||!B->Mesh.IsValid()||(B->MuzzleYaw==0.f&&B->MuzzlePitch==0.f))return;
 B->MuzzleYaw=B->MuzzlePitch=0.f;SetSpinePitch(B->Mesh.Get(),0.f);
 B->Mesh->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));
}

void UBattleLeaderRuntime::RefreshFollowerPose(int32 ID)
{
 const auto* B=Bodies.Find(ID);
 if(!B||B->Actor.IsValid()||!B->Mesh.IsValid()||(VAT&&VAT->IsFar(ID))||!B->Mesh->IsComponentTickEnabled())return;
 B->Mesh->HandleExistingParallelEvaluationTask(true,true);
 PlayFollowerClip(ID,B->Pose);
}

void UBattleLeaderRuntime::PlayFollowerClip(int32 ID,int32 Pose)
{
 const auto* B=Bodies.Find(ID);
 if(!B||!B->Mesh.IsValid())return;
 UAnimSequence* Clip=FollowerClip(ID,Pose);
 B->Mesh->PlayAnimation(Clip,true);
 // Walking crouched or prone (-BattleStanceMove, or a crouched move of the cover action): the steps keep pace with his speed
 // (his velocity; the stance's share of the walking speed before he has one).
 const uint8 Stance=StanceOf(ID);
 if(Pose==1 && Stance!=BattleStance::Stand && Clip && !B->CoverClipPhase && !B->bBlindPose)
 {
  const float Speed=B->Velocity.SizeSquared2D()>400.?float(B->Velocity.Size2D()):GetDefault<UBattleTuning>()->MoveSpeed*BattleStance::MoveScale(Stance);
  B->Mesh->SetPlayRate(BattleBodyAnim::MatchRate(Clip,Speed));
 }
}

void UBattleLeaderRuntime::UpdateAIStance(int32 ID,double Now,bool bMoving,bool bCombat,bool bLostSight,bool bThreat)
{
 if(!BattleStance::Enabled(GetWorld()))return;
 const uint8 Current=StanceOf(ID);
 // Pinned by suppression: get as low as possible, whatever the order says (own state only). With -BattleStanceMove=2 he
 // crawls on along his way (Follow lets him walk prone); before, he waited until he could stand up.
 if(IsPinned(ID,Now)){SetStance(ID,BattleStance::Prone,Now,false,bMoving&&BattleStance::MoveMode()>=2);return;}
 // Trial -BattleStanceMove=2: on the move under fire or with an enemy in sight he stays crouched (own state and own sight).
 if(bMoving && bThreat && BattleStance::MoveMode()>=2){NoCoverProneUntil.Remove(ID);SetStance(ID,BattleStance::Crouch,Now,false,true);return;}
 if(bMoving||!bCombat){NoCoverProneUntil.Remove(ID);SetStance(ID,BattleStance::Stand,Now,false,bMoving);return;}
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
