// Offline asset import (2026-11-19, user request): cover clips of the "TPS Multiplayer Shooter with cover" sample for blind fire
// (BattleBlindFire.cpp). Staging: the sample's files are copied unchanged to their own paths
// (/Game/Multiplayercovershooter/Animations/UE5_Anims/ and its skeleton /Game/Multiplayercovershooter/Characters/Mannequins/Meshes/SK_Mannequin)
// only as input; each clip is duplicated to /Game/Characters/Mannequins/Anims/Rifle/Cover/, moved to the project's UE5 mannequin
// skeleton (same bone names) and stripped of notifies. Existing assets are never written. The staging copy is deleted afterwards
// and never committed.
// Each clip (imported or kept) is then measured on the project's body mesh: head and hand heights, the rifle direction
// (right hand -> left hand) and the root drift, so the blind fire clips can be chosen from numbers (log BATTLE_COVER_CLIP).
// Run: FutureBattle.uproject -run=BattleVATBake -ImportCoverAnims
#include "BattleVATBake.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
 const TCHAR* const CoverClips[]={
  TEXT("AS_coverlo_R_idle"),TEXT("AS_CoverLo_Idle2CoverR"),TEXT("AS_CoverLo_CoverL"),
  TEXT("AS_CoverLo_CoverR2AimU"),TEXT("AS_CoverLo_AimU_down"),TEXT("AS_CoverLo_AimU2CoverR"),TEXT("AS_CoverLo_CoverL2AimU"),TEXT("AS_CoverLo_AimU2CoverL"),
  TEXT("AS_CoverLo_AimR"),TEXT("AS_CoverLo_CoverR2AimR"),TEXT("AS_CoverLo_AimR2CoverR"),TEXT("AS_CoverLo_AimL"),TEXT("AS_CoverLo_CoverL2AimL"),TEXT("AS_CoverLo_AimL2CoverL"),
  TEXT("AS_cover_hi_r_idle"),TEXT("AS_CoverHi_Idle2CoverR"),TEXT("AS_CoverHi_CoverL"),
  TEXT("AS_CoverHi_AimR"),TEXT("AS_CoverHi_CoverR2AimR"),TEXT("AS_CoverHi_AimR2CoverR"),TEXT("AS_CoverHi_AimL"),TEXT("AS_CoverHi_CoverL2AimL"),TEXT("AS_CoverHi_AimL2CoverL")};
}

int32 ImportBattleCoverAnims()
{
 auto* Target=LoadObject<USkeleton>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin"));
 auto* BodyMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
 if(!Target || !BodyMesh){UE_LOG(LogTemp,Error,TEXT("COVER_ANIM_IMPORT missing target skeleton or body"));return 1;}
 int32 Failures=0;
 TArray<UAnimSequence*> Clips;
 for(const TCHAR* Name:CoverClips)
 {
  const FString Path=FString(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Cover/"))+Name;
  if(FPackageName::DoesPackageExist(Path))
  {
   UE_LOG(LogTemp,Display,TEXT("COVER_ANIM_IMPORT exists %s (kept)"),*Path);
   if(auto* Kept=LoadObject<UAnimSequence>(nullptr,*Path))Clips.Add(Kept);
   continue;
  }
  const FString Source=FString(TEXT("/Game/Multiplayercovershooter/Animations/UE5_Anims/"))+Name;
  auto* Src=LoadObject<UAnimSequence>(nullptr,*Source);
  if(!Src){UE_LOG(LogTemp,Error,TEXT("COVER_ANIM_IMPORT missing source %s"),*Source);++Failures;continue;}
  UPackage* Package=CreatePackage(*Path);
  auto* Dst=DuplicateObject<UAnimSequence>(Src,Package,Name);
  Dst->SetFlags(RF_Public|RF_Standalone);
  const int32 Notifies=Dst->Notifies.Num();
  Dst->Notifies.Empty();
  if(!Dst->ReplaceSkeleton(Target,false)){UE_LOG(LogTemp,Error,TEXT("COVER_ANIM_IMPORT skeleton replace failed %s"),Name);++Failures;continue;}
  Dst->RefreshCacheData();
  Dst->PostEditChange();
  Dst->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Dst);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  const bool Saved=UPackage::SavePackage(Package,Dst,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
  UE_LOG(LogTemp,Display,TEXT("COVER_ANIM_IMPORT %s saved=%d length=%.3f keys=%d skeleton=%s dropped_notifies=%d"),*Path,Saved,Dst->GetPlayLength(),Dst->GetNumberOfSampledKeys(),*GetPathNameSafe(Dst->GetSkeleton()),Notifies);
  if(!Saved){++Failures;continue;}
  Clips.Add(Dst);
 }
 // Measure on the project's body (mesh space: the body faces +Y, Z up from the feet).
 auto* World=GEditor->GetEditorWorldContext().World();auto* Host=World->SpawnActor<AActor>();
 auto* Body=NewObject<USkeletalMeshComponent>(Host);Host->AddInstanceComponent(Body);Host->SetRootComponent(Body);
 Body->SetSkeletalMesh(BodyMesh);Body->SetUpdateAnimationInEditor(true);
 Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Body->RegisterComponent();
 for(auto* Clip:Clips)
 {
  Body->PlayAnimation(Clip,true);
  FString Line;
  for(const float Fraction:{0.f,.5f,1.f})
  {
   const float Time=Clip->GetPlayLength()*Fraction;
   Body->SetPosition(Time,false);Body->TickAnimation(0,false);Body->RefreshBoneTransforms(nullptr);
   const FVector Head=Body->GetBoneLocation(TEXT("head"),EBoneSpaces::ComponentSpace);
   const FVector HandR=Body->GetBoneLocation(TEXT("hand_r"),EBoneSpaces::ComponentSpace);
   const FVector HandL=Body->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace);
   const FVector Pelvis=Body->GetBoneLocation(TEXT("pelvis"),EBoneSpaces::ComponentSpace);
   // Rifle yaw relative to the body's facing (+Y = 0 deg, positive = to the body's right), pitch up positive.
   const FVector Gun=(HandL-HandR).GetSafeNormal();
   const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(-Gun.X,Gun.Y)),Pitch=FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Gun.Z,-1.f,1.f)));
   Line+=FString::Printf(TEXT(" t%.0f[head_z=%.0f hand_r_z=%.0f hand_l_z=%.0f gun_yaw=%.0f gun_pitch=%.0f pelvis=%.0f,%.0f,%.0f]"),Fraction*100,Head.Z,HandR.Z,HandL.Z,Yaw,Pitch,Pelvis.X,Pelvis.Y,Pelvis.Z);
  }
  UE_LOG(LogTemp,Display,TEXT("BATTLE_COVER_CLIP %s length=%.2f%s"),*Clip->GetName(),Clip->GetPlayLength(),*Line);
 }
 Host->Destroy();
 return Failures;
}
#endif
