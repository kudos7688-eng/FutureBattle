#include "BattleAnimTaskTrial.h"
#include "BattleFollowerBones.h"
#include "BattlePhase3.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "FutureBattle.h"

ABattleAnimTaskTrial::ABattleAnimTaskTrial()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
void ABattleAnimTaskTrial::Start(UBattlePhase3* InPhase)
{
 Phase=InPhase;
 auto* Asset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
 for(const TCHAR* Path:{TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload")}) Clips.Add(LoadObject<UAnimSequence>(nullptr,Path));
 BoneUpdates.Init(0,18); Policies.SetNum(18);
 for(int32 Pair=0;Pair<9;++Pair) for(int32 Variant=0;Variant<2;++Variant)
 {
  const int32 Index=Meshes.Num(); auto* M=NewObject<USkeletalMeshComponent>(this); AddInstanceComponent(M);
  M->SetSkeletalMesh(Asset); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetupAttachment(RootComponent);
  M->RegisterComponent(); M->SetForcedLOD(Pair%3+1); M->PlayAnimation(Clips[Pair/3],true);
  M->SetRelativeLocation(FVector(0,Pair*160,0));
  M->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateLambda([this,Index](){++BoneUpdates[Index];}));
  Meshes.Add(M);
  auto* W=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(W); W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  W->SetupAttachment(M,TEXT("hand_r")); W->RegisterComponent(); Weapons.Add(W);
  if(Variant) UE_LOG(LogFutureBattle,Display,TEXT("ANIM_TASK_FIXTURE pair=%d eligible=%d notifies=%d rootmotion=%d postprocess=%d"),Pair,BattleFollowerBones::Eligible(M),Clips[Pair/3]->Notifies.Num(),Clips[Pair/3]->HasRootMotion(),M->GetPostProcessInstance()!=nullptr);
 }
}
void ABattleAnimTaskTrial::Compare(int32 Pair)
{
 auto* A=Meshes[Pair*2].Get(); auto* B=Meshes[Pair*2+1].Get();
 A->CompleteParallelAnimationEvaluation(true); B->CompleteParallelAnimationEvaluation(true);
 bool Equal=A->GetSingleNodeInstance()->GetCurrentTime()==B->GetSingleNodeInstance()->GetCurrentTime();
 if(Shown)
 {
  const auto& X=A->GetComponentSpaceTransforms(); const auto& Y=B->GetComponentSpaceTransforms();
  Equal &= X.Num()==Y.Num();
  for(int32 I=0;I<FMath::Min(X.Num(),Y.Num());++I) Equal &= X[I].Equals(Y[I],1.e-6);
  Equal &= A->GetSocketTransform(TEXT("hand_r")).Equals(B->GetSocketTransform(TEXT("hand_r")),1.e-6);
  Equal &= Weapons[Pair*2]->GetComponentTransform().Equals(Weapons[Pair*2+1]->GetComponentTransform(),1.e-6);
 }
 ++Checks;
 if(!Equal) { if(Errors<8) UE_LOG(LogFutureBattle,Error,TEXT("ANIM_TASK_POSE_MISMATCH frame=%d pair=%d shown=%d times=%.9f/%.9f"),Frame,Pair,Shown,A->GetSingleNodeInstance()->GetCurrentTime(),B->GetSingleNodeInstance()->GetCurrentTime()); ++Errors; }
}
void ABattleAnimTaskTrial::Tick(float Dt)
{
 Super::Tick(Dt); if(!Phase || Done) return;
 ++Frame;
 for(auto& M:Meshes) M->CompleteParallelAnimationEvaluation(true);
 // Move and turn both parents while hidden, change clip, and cross LODs.
 if(Frame==30 || Frame==240) { Shown=false; for(int32 I=0;I<Meshes.Num();++I) BattleFollowerBones::SetVisible(Meshes[I],false,true); }
 if(Frame==90) for(int32 I=0;I<Meshes.Num();++I) { Meshes[I]->PlayAnimation(Clips[(I/6+1)%3],true); BattleFollowerBones::SetVisible(Meshes[I],Shown,true); }
 if(Frame==140 || Frame==150) for(int32 I=0;I<2;++I)
 {
  Meshes[I]->bCastHiddenShadow=Frame==140;
  BattleFollowerBones::SetVisible(Meshes[I],Shown,true);
 }
 if(Frame==120) { SetActorRotation(FRotator(0,45,0)); for(auto& M:Meshes) M->SetForcedLOD(1); }
 if(Frame==180 || Frame==330)
 {
  Shown=true; for(int32 I=0;I<Meshes.Num();++I) BattleFollowerBones::SetVisible(Meshes[I],true,true);
  ++Reveals;
 }
 if(Frame==300) for(int32 I=0;I<Meshes.Num();++I) { BattleFollowerBones::PrepareFreeze(Meshes[I],true); Meshes[I]->SetComponentTickEnabled(false); }
 for(int32 I=0;I<Meshes.Num();++I) Policies[I].Apply(Meshes[I],I%2==1 && Frame!=100);
 if(Frame==100 || Frame==140 || Frame==180)
 {
  const int32 Limit=Frame==140?2:Meshes.Num();
  for(int32 I=0;I<Limit;++I) if(!Meshes[I]->GetSingleNodeInstance()->bUseMultiThreadedAnimationUpdate) ++Errors;
 }
 if(Frame>2) for(int32 Pair=0;Pair<9;++Pair) Compare(Pair);
 if(!Shown && Frame>60 && Frame<90) for(int32 Pair=0;Pair<9;++Pair) if(Meshes[Pair*2]->GetSingleNodeInstance()->bUseMultiThreadedAnimationUpdate && !Meshes[Pair*2+1]->GetSingleNodeInstance()->bUseMultiThreadedAnimationUpdate && BoneUpdates[Pair*2]==0 && BoneUpdates[Pair*2+1]==0) ++ReducedTicks;
 for(int32& Count:BoneUpdates) Count=0;
 if(Frame>=360)
 {
  Done=true; if(!ReducedTicks || Reveals!=2) ++Errors;
  if(Errors) ++Phase->Failures;
  UE_LOG(LogFutureBattle,Display,TEXT("P3_ANIM_TASK_AUDIT checks=%d errors=%d reduced_ticks=%d reveals=%d"),Checks,Errors,ReducedTicks,Reveals);
 }
}
