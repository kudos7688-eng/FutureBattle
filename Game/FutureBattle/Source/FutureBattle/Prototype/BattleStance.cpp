#include "BattleStance.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleUnit.h"
#include "BattleVATDisplay.h"
#include "Animation/AnimSequence.h"
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
float BattleStance::BodyScale(uint8 Stance){return Stance==Crouch&&CrouchClip(false)?1.f:VisualScale(Stance);}

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
  if(B->Actor.IsValid()){B->Actor->VisualStance=Stance;if(auto* Mesh=B->Actor->GetMesh())Mesh->SetRelativeScale3D(Scale);}
  else if(B->Mesh.IsValid()){B->Mesh->SetRelativeScale3D(Scale);if(BattleStance::AnimEnabled())RefreshFollowerPose(ID);}
 }
 SightExperiment.Cover(Stance==BattleStance::Prone?TEXT("stance_to_prone"):Stance==BattleStance::Crouch?TEXT("stance_to_crouch"):TEXT("stance_to_stand"));
 return true;
}

UAnimSequence* UBattleLeaderRuntime::FollowerClip(int32 ID,int32 Pose) const
{
 if(StanceOf(ID)==BattleStance::Crouch)if(auto* Crouch=BattleStance::CrouchClip(Pose==1))return Crouch;
 return Pose==2?ReloadPose.Get():Pose==1?WalkPose.Get():IdlePose.Get();
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
 if(bMoving||!bCombat){SetStance(ID,BattleStance::Stand,Now);return;}
 if(bLostSight)
 {
  // The remembered target is no longer visible from down here: stand up and look for a few seconds.
  if(Current!=BattleStance::Stand&&SetStance(ID,BattleStance::Stand,Now)){StanceLockedUntil.Add(ID,Now+3.);SightExperiment.Cover(TEXT("stance_stood_to_look"));}
  return;
 }
 SetStance(ID,BattleStance::Crouch,Now);
}
