#include "BattleBodyAnim.h"
#include "TwoBoneIK.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
 const TCHAR* const ClipPaths[]={
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Bwd"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Left"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Right"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Idle"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Walk_Fwd"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Walk_Bwd"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Walk_Left"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Walk_Right"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Prone_IdleR"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Prone_ForwardR"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Prone_BackwardR"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Prone_LeftR"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Prone_RightR"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Entry"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Exit"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Rifle_Idle2Prone"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Rifle_Prone2Idle"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Rifle_Crouch2Prone"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Rifle_Prone2Crouch"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/Rifle_Prone_Reload"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CU"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CC"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CD"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_02"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_03"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_04"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Back_Med_01"),
  TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01"),TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_02"),
  TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_03"),TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01"),
  TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Left_01"),TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Right_01")};
 static_assert(UE_ARRAY_COUNT(ClipPaths)==int32(BattleBodyAnim::EClip::Count),"one path per clip");
 constexpr float CrossFadeSeconds=.2f,OverlayFadeSeconds=.1f;

 // Which side the shot came from, seen by the body: 0 front, 1 back, 2 left, 3 right.
 int32 Side(const FVector& BodyForward,const FVector& ShotDirection)
 {
  const FVector From=-ShotDirection.GetSafeNormal2D(),Forward=BodyForward.GetSafeNormal2D();
  if(From.IsNearlyZero() || Forward.IsNearlyZero())return 0;
  // Cross(Up, Forward) is the body's right in UE's frame (X forward -> +Y right).
  const float Ahead=FVector::DotProduct(Forward,From),Right=FVector::DotProduct(FVector::CrossProduct(FVector::UpVector,Forward),From);
  if(FMath::Abs(Ahead)>=FMath::Abs(Right))return Ahead>=0?0:1;
  return Right>0?3:2;
 }
}

bool BattleBodyAnim::Enabled()
{
 static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleBodyAnim="),Mode);return Mode!=0;}();
 return bOn;
}

UAnimSequence* BattleBodyAnim::Get(EClip Clip)
{
 if(!Enabled())return nullptr;
 static TStrongObjectPtr<UAnimSequence> Clips[int32(EClip::Count)];static bool bLoaded=false;
 if(!bLoaded)
 {
  bLoaded=true;int32 Missing=0;
  for(int32 I=0;I<int32(EClip::Count);++I){Clips[I].Reset(LoadObject<UAnimSequence>(nullptr,ClipPaths[I]));if(!Clips[I].IsValid()){++Missing;UE_LOG(LogTemp,Warning,TEXT("BATTLE_BODY_ANIM missing %s"),ClipPaths[I]);}}
  UE_LOG(LogTemp,Display,TEXT("BATTLE_BODY_ANIM clips=%d missing=%d"),int32(EClip::Count),Missing);
 }
 return Clips[int32(Clip)].Get();
}

UAnimSequence* BattleBodyAnim::Locomotion(uint8 Stance,const FVector& LocalVelocity,bool bJog)
{
 const int32 First=Stance==2?int32(EClip::ProneIdle):Stance==1?int32(EClip::CrouchIdle):int32(EClip::StandIdle);
 if(LocalVelocity.SizeSquared2D()<=400.)return Get(EClip(First));
 if(Stance==0 && bJog && LocalVelocity.X>FMath::Abs(LocalVelocity.Y))return Get(EClip::StandJog);
 // 1 forward, 2 backward, 3 left, 4 right (the same order for every stance).
 const int32 Dir=FMath::Abs(LocalVelocity.X)>=FMath::Abs(LocalVelocity.Y)?(LocalVelocity.X>=0?1:2):(LocalVelocity.Y<0?3:4);
 UAnimSequence* Clip=Get(EClip(First+Dir));
 return Clip?Clip:Get(EClip(First+1));
}

UAnimSequence* BattleBodyAnim::Transition(uint8 From,uint8 To)
{
 if(From==0&&To==1)return Get(EClip::StandToCrouch);
 if(From==1&&To==0)return Get(EClip::CrouchToStand);
 if(From==0&&To==2)return Get(EClip::StandToProne);
 if(From==2&&To==0)return Get(EClip::ProneToStand);
 if(From==1&&To==2)return Get(EClip::CrouchToProne);
 if(From==2&&To==1)return Get(EClip::ProneToCrouch);
 return nullptr;
}

UAnimSequence* BattleBodyAnim::Death(const FVector& BodyForward,const FVector& ShotDirection,int32 Seed)
{
 switch(Side(BodyForward,ShotDirection))
 {
  case 1:return Get(EClip::DeathBack);
  case 2:return Get(EClip::DeathLeft);
  case 3:return Get(EClip::DeathRight);
  default:return Get(EClip(int32(EClip::DeathFront1)+FMath::Abs(Seed)%3));
 }
}

UAnimSequence* BattleBodyAnim::Hit(const FVector& BodyForward,const FVector& ShotDirection,int32 Seed)
{
 return Side(BodyForward,ShotDirection)==1?Get(EClip::HitBack):Get(EClip(int32(EClip::HitFront1)+FMath::Abs(Seed)%4));
}

UAnimSequence* BattleBodyAnim::CoverClip(const TCHAR* Name)
{
 if(!Enabled() || !Name)return nullptr;
 static TMap<FName,TStrongObjectPtr<UAnimSequence>> Cache;
 const FName Key(Name);
 if(const auto* Found=Cache.Find(Key))return Found->Get();
 UAnimSequence* Clip=LoadObject<UAnimSequence>(nullptr,*(FString(BattleCoverClips::Folder())+Name));
 if(!Clip)UE_LOG(LogTemp,Warning,TEXT("BATTLE_BODY_ANIM missing cover clip %s"),Name);
 Cache.Add(Key,TStrongObjectPtr<UAnimSequence>(Clip));
 return Clip;
}

BattleCoverClips::FSet BattleBodyAnim::CoverSet(UWorld* World,const FBattleUnitState& U,const FVector& Forward)
{
 const FVector Threat=U.CoverAnchor+Forward.GetSafeNormal2D()*1000.f;
 const BattleCoverClips::ESide Side=BattleCoverClips::SideOf(U.CoverAnchor,U.CoverPeek,Threat);
 // CoverAnchor is the capsule centre: the standing eye is 60 cm above it.
 const bool bBlocked=Side!=BattleCoverClips::ESide::Up && !BattleTactics::HasTerrainLine(World,U.CoverAnchor+FVector(0,0,60),Threat+FVector(0,0,60));
 return BattleCoverClips::Pick(BattleCoverClips::HeightOf(Side,bBlocked),Side);
}

void UBattleBodyAnimInstance::SetBase(UAnimSequence* Clip,bool bLoop)
{
 if(Clip==BaseClip){bBaseLoop=bLoop;return;}
 if(BaseClip){PreviousClip=BaseClip;PreviousTime=BaseTime;bPreviousLoop=bBaseLoop;PreviousWeight=1.f;}
 BaseClip=Clip;BaseTime=0.f;bBaseLoop=bLoop;
}

void UBattleBodyAnimInstance::SetUpper(UAnimSequence* Clip,float Weight,float Time)
{
 if(Clip!=UpperClip){UpperClip=Clip;UpperTime=0.f;}
 UpperWeight=Clip?FMath::Clamp(Weight,0.f,1.f):0.f;
 bUpperDriven=Time>=0.f;
 if(bUpperDriven && Clip)UpperTime=FMath::Clamp(Time,0.f,Clip->GetPlayLength());
}

void UBattleBodyAnimInstance::SetAim(float PitchDegrees,float Weight){AimPitch=FMath::Clamp(PitchDegrees,-90.f,90.f);AimWeight=FMath::Clamp(Weight,0.f,1.f);}

void UBattleBodyAnimInstance::PlayOverlay(UAnimSequence* Clip,float Weight)
{
 if(!Clip)return;
 OverlayClip=Clip;OverlayTime=0.f;OverlayWeight=FMath::Clamp(Weight,0.f,1.f);
}

void UBattleBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
 Super::NativeUpdateAnimation(DeltaSeconds);
 auto Advance=[DeltaSeconds](UAnimSequence* Clip,float& Time,bool bLoop)
 {
  if(!Clip)return;
  const float Length=FMath::Max(.001f,Clip->GetPlayLength());
  Time+=DeltaSeconds;
  Time=bLoop?FMath::Fmod(Time,Length):FMath::Min(Time,Length);
 };
 Advance(BaseClip,BaseTime,bBaseLoop);
 if(PreviousWeight>0.f){Advance(PreviousClip,PreviousTime,bPreviousLoop);PreviousWeight=FMath::Max(0.f,PreviousWeight-DeltaSeconds/CrossFadeSeconds);}
 if(!bUpperDriven)Advance(UpperClip,UpperTime,true);
 if(OverlayClip)
 {
  OverlayTime+=DeltaSeconds;
  const float Left=OverlayClip->GetPlayLength()-OverlayTime;
  if(Left<=0.f){OverlayClip=nullptr;OverlayWeight=0.f;}
  else if(Left<OverlayFadeSeconds)OverlayWeight=FMath::Min(OverlayWeight,Left/OverlayFadeSeconds);
 }
}

void FBattleBodyAnimProxy::PreUpdate(UAnimInstance* InAnimInstance,float DeltaSeconds)
{
 FAnimInstanceProxy::PreUpdate(InAnimInstance,DeltaSeconds);
 const auto* I=CastChecked<UBattleBodyAnimInstance>(InAnimInstance);
 Base={I->BaseClip,I->BaseTime,I->bBaseLoop};
 Previous={I->PreviousClip,I->PreviousTime,I->bPreviousLoop};PreviousWeight=I->PreviousClip?I->PreviousWeight:0.f;
 Upper={I->UpperClip,I->UpperTime,!I->bUpperDriven};UpperWeight=I->UpperClip?I->UpperWeight:0.f;
 Overlay={I->OverlayClip,I->OverlayTime,false};OverlayWeight=I->OverlayClip?I->OverlayWeight:0.f;
 AimPitch=I->AimPitch;AimWeight=I->AimWeight;
 RaiseTarget=I->RaiseTarget;RaiseWeight=I->RaiseWeight;
 Aim[0]=BattleBodyAnim::Get(BattleBodyAnim::EClip::AimDown);
 Aim[1]=BattleBodyAnim::Get(BattleBodyAnim::EClip::AimCenter);
 Aim[2]=BattleBodyAnim::Get(BattleBodyAnim::EClip::AimUp);
}

bool FBattleBodyAnimProxy::Evaluate(FPoseContext& Output)
{
 if(!Base.Clip){Output.ResetToRefPose();return true;}
 EvaluateLayers(Output);
 if(RaiseWeight>0.f)RaiseHands(Output.Pose);
 return true;
}

void FBattleBodyAnimProxy::RaiseHands(FCompactPose& Pose) const
{
 const FBoneContainer& Bones=Pose.GetBoneContainer();
 auto Find=[&](const TCHAR* Name){const int32 I=Bones.GetPoseBoneIndexForBoneName(Name);return I==INDEX_NONE?FCompactPoseBoneIndex(INDEX_NONE):Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));};
 const FCompactPoseBoneIndex Arm[2][3]={{Find(TEXT("upperarm_r")),Find(TEXT("lowerarm_r")),Find(TEXT("hand_r"))},{Find(TEXT("upperarm_l")),Find(TEXT("lowerarm_l")),Find(TEXT("hand_l"))}};
 for(const auto& Side:Arm)for(const auto& Bone:Side)if(!Bone.IsValid())return;
 FCSPose<FCompactPose> CS;CS.InitPose(Pose);
 // One offset for both hands: the right hand to the target, the left hand by the same amount, so the grip holds.
 const FVector Offset=(RaiseTarget-CS.GetComponentSpaceTransform(Arm[0][2]).GetLocation())*RaiseWeight;
 TArray<FBoneTransform,TInlineAllocator<6>> Solved;
 for(const auto& Side:Arm)
 {
  FTransform ArmUpper=CS.GetComponentSpaceTransform(Side[0]),ArmLower=CS.GetComponentSpaceTransform(Side[1]),ArmHand=CS.GetComponentSpaceTransform(Side[2]);
  // The elbow bends the way it already does (its current position is the pole); no stretching.
  AnimationCore::SolveTwoBoneIK(ArmUpper,ArmLower,ArmHand,ArmLower.GetLocation(),ArmHand.GetLocation()+Offset,false,1.,1.);
  Solved.Add(FBoneTransform(Side[0],ArmUpper));Solved.Add(FBoneTransform(Side[1],ArmLower));Solved.Add(FBoneTransform(Side[2],ArmHand));
 }
 Solved.Sort([](const FBoneTransform& A,const FBoneTransform& B){return A.BoneIndex<B.BoneIndex;});
 CS.SafeSetCSBoneTransforms(Solved);
 FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS),Pose);
}

void FBattleBodyAnimProxy::EvaluateLayers(FPoseContext& Output)
{
 auto Sample=[](const FBattleBodyLayer& Layer,FPoseContext& Into)
 {
  FAnimationPoseData Data(Into);
  Layer.Clip->GetAnimationPose(Data,FAnimExtractContext(double(Layer.Time),false,FDeltaTimeRecord(),Layer.bLoop));
 };
 auto BlendInto=[](FCompactPose& Into,const FCompactPose& Other,float Weight)
 {
  if(Weight<=0.f)return;
  for(const FCompactPoseBoneIndex Index:Into.ForEachBoneIndex()){FTransform Mixed;Mixed.Blend(Into[Index],Other[Index],Weight);Into[Index]=Mixed;}
 };
 Sample(Base,Output);
 if(PreviousWeight>0.f && Previous.Clip){FPoseContext Old(Output);Sample(Previous,Old);BlendInto(Output.Pose,Old.Pose,PreviousWeight);}
 const bool bAim=AimWeight>0.f && Aim[1];
 if(!bAim && UpperWeight<=0.f && OverlayWeight<=0.f)return;
 // The upper body: aim pose (by pitch), then the upper clip, then the one-shot overlay, each over the previous.
 FPoseContext Top(Output);Top.Pose.CopyBonesFrom(Output.Pose);
 if(bAim)
 {
  FPoseContext Center(Output);Sample({Aim[1],0.f,false},Center);
  UAnimSequence* Toward=AimPitch>=0.f?Aim[2]:Aim[0];
  if(Toward && FMath::Abs(AimPitch)>.5f){FPoseContext Tilt(Output);Sample({Toward,0.f,false},Tilt);BlendInto(Center.Pose,Tilt.Pose,FMath::Abs(AimPitch)/90.f);}
  BlendInto(Top.Pose,Center.Pose,AimWeight);
 }
 if(UpperWeight>0.f && Upper.Clip){FPoseContext Layer(Output);Sample(Upper,Layer);BlendInto(Top.Pose,Layer.Pose,UpperWeight);}
 if(OverlayWeight>0.f && Overlay.Clip){FPoseContext Layer(Output);Sample(Overlay,Layer);BlendInto(Top.Pose,Layer.Pose,OverlayWeight);}
 // Above spine_01 only: spine_01 takes half, everything below it in the hierarchy takes all.
 const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
 const int32 SpinePose=Bones.GetPoseBoneIndexForBoneName(TEXT("spine_01"));
 if(SpinePose==INDEX_NONE){Output.Pose.CopyBonesFrom(Top.Pose);return;}
 const FCompactPoseBoneIndex Spine=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SpinePose));
 TArray<float,TInlineAllocator<128>> Mask;Mask.SetNumZeroed(Output.Pose.GetNumBones());
 for(const FCompactPoseBoneIndex Index:Output.Pose.ForEachBoneIndex())
 {
  if(Index==Spine){Mask[Index.GetInt()]=.5f;continue;}
  const FCompactPoseBoneIndex Parent=Bones.GetParentBoneIndex(Index);
  if(Parent.IsValid() && Mask[Parent.GetInt()]>0.f)Mask[Index.GetInt()]=1.f;
 }
 for(const FCompactPoseBoneIndex Index:Output.Pose.ForEachBoneIndex())
  if(const float W=Mask[Index.GetInt()];W>0.f){FTransform Mixed;Mixed.Blend(Output.Pose[Index],Top.Pose[Index],W);Output.Pose[Index]=Mixed;}
 return;
}
