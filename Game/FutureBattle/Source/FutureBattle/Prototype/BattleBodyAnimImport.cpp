// Offline asset import (2026-11-19, user request): body clips for blind fire, prone, stance transitions and walking in every
// direction (BattleBlindFire.cpp, BattleBodyAnim.cpp), from the asset samples in C:\Users\nanek\Documents\FPS\アセット:
// - "TPS Multiplayer Shooter with cover": cover and prone clips (UE5_Anims, UE5 mannequin bone names).
// - Lyra: the crouch walks backward and to the sides (the forward walk was imported with the stance clips, BattleStanceAnimImport.cpp).
// Staging: the sample files (and the skeleton each references) are copied unchanged to their own /Game paths only as input;
// each clip is duplicated to the project folder, moved to the project's UE5 mannequin skeleton, stripped of notifies and
// root-locked (the capsule moves the body; a clip must never drift the mesh away from it). Existing assets are never written.
// The staging copies are deleted afterwards and never committed.
// Every clip of the table and the death and hit clips already in the project are then measured on the project's body mesh
// (log BATTLE_BODY_CLIP: head and hand heights, rifle direction, pelvis drift), so the clips can be checked from numbers.
// Run: FutureBattle.uproject -run=BattleVATBake -ImportBodyAnims
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
 struct FClipImport{const TCHAR* Source;const TCHAR* Target;const TCHAR* Name;};
 const TCHAR* const TPS=TEXT("/Game/Multiplayercovershooter/Animations/UE5_Anims/");
 const TCHAR* const TPSMontages=TEXT("/Game/Multiplayercovershooter/Animations/UE5_Anims/Montages/");
 const TCHAR* const Lyra=TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/");
 const TCHAR* const Cover=TEXT("/Game/Characters/Mannequins/Anims/Rifle/Cover/");
 const TCHAR* const Prone=TEXT("/Game/Characters/Mannequins/Anims/Rifle/Prone/");
 const TCHAR* const Crouch=TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/");
 const FClipImport Imports[]={
  {TPS,Cover,TEXT("AS_coverlo_R_idle")},{TPS,Cover,TEXT("AS_CoverLo_Idle2CoverR")},{TPS,Cover,TEXT("AS_CoverLo_CoverL")},
  {TPS,Cover,TEXT("AS_CoverLo_CoverR2AimU")},{TPS,Cover,TEXT("AS_CoverLo_AimU_down")},{TPS,Cover,TEXT("AS_CoverLo_AimU2CoverR")},
  {TPS,Cover,TEXT("AS_CoverLo_CoverL2AimU")},{TPS,Cover,TEXT("AS_CoverLo_AimU2CoverL")},
  {TPS,Cover,TEXT("AS_CoverLo_AimR")},{TPS,Cover,TEXT("AS_CoverLo_CoverR2AimR")},{TPS,Cover,TEXT("AS_CoverLo_AimR2CoverR")},
  {TPS,Cover,TEXT("AS_CoverLo_AimL")},{TPS,Cover,TEXT("AS_CoverLo_CoverL2AimL")},{TPS,Cover,TEXT("AS_CoverLo_AimL2CoverL")},
  {TPS,Cover,TEXT("AS_cover_hi_r_idle")},{TPS,Cover,TEXT("AS_CoverHi_Idle2CoverR")},{TPS,Cover,TEXT("AS_CoverHi_CoverL")},
  {TPS,Cover,TEXT("AS_CoverHi_AimR")},{TPS,Cover,TEXT("AS_CoverHi_CoverR2AimR")},{TPS,Cover,TEXT("AS_CoverHi_AimR2CoverR")},
  {TPS,Cover,TEXT("AS_CoverHi_AimL")},{TPS,Cover,TEXT("AS_CoverHi_CoverL2AimL")},{TPS,Cover,TEXT("AS_CoverHi_AimL2CoverL")},
  {TPS,Prone,TEXT("Prone_IdleR")},{TPS,Prone,TEXT("Prone_ForwardR")},{TPS,Prone,TEXT("Prone_BackwardR")},
  {TPS,Prone,TEXT("Prone_LeftR")},{TPS,Prone,TEXT("Prone_RightR")},
  {TPS,Prone,TEXT("Rifle_Idle2Prone")},{TPS,Prone,TEXT("Rifle_Prone2Idle")},{TPS,Prone,TEXT("Rifle_Crouch2Prone")},{TPS,Prone,TEXT("Rifle_Prone2Crouch")},
  {TPSMontages,Prone,TEXT("Rifle_Prone_Reload")},
  {Lyra,Crouch,TEXT("MM_Rifle_Crouch_Walk_Bwd")},{Lyra,Crouch,TEXT("MM_Rifle_Crouch_Walk_Left")},{Lyra,Crouch,TEXT("MM_Rifle_Crouch_Walk_Right")}};
 // Already in the project (Lyra), measured only.
 const TCHAR* const Measured[]={
  TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01"),TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_02"),
  TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_03"),TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01"),
  TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Left_01"),TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Right_01"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Back_Med_01"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Entry"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/MM_Rifle_Crouch_Exit"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CU"),
  TEXT("/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CC"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CD")};
}

int32 ImportBattleBodyAnims()
{
 auto* Target=LoadObject<USkeleton>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin"));
 auto* BodyMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
 if(!Target || !BodyMesh){UE_LOG(LogTemp,Error,TEXT("BODY_ANIM_IMPORT missing target skeleton or body"));return 1;}
 int32 Failures=0;
 TArray<UAnimSequence*> Clips;
 for(const auto& Entry:Imports)
 {
  const FString Path=FString(Entry.Target)+Entry.Name;
  if(FPackageName::DoesPackageExist(Path))
  {
   UE_LOG(LogTemp,Display,TEXT("BODY_ANIM_IMPORT exists %s (kept)"),*Path);
   if(auto* Kept=LoadObject<UAnimSequence>(nullptr,*Path))Clips.Add(Kept);
   continue;
  }
  const FString Source=FString(Entry.Source)+Entry.Name;
  auto* Src=LoadObject<UAnimSequence>(nullptr,*Source);
  if(!Src){UE_LOG(LogTemp,Error,TEXT("BODY_ANIM_IMPORT missing source %s"),*Source);++Failures;continue;}
  UPackage* Package=CreatePackage(*Path);
  auto* Dst=DuplicateObject<UAnimSequence>(Src,Package,Entry.Name);
  Dst->SetFlags(RF_Public|RF_Standalone);
  const int32 Notifies=Dst->Notifies.Num();
  Dst->Notifies.Empty();
  Dst->bForceRootLock=true;
  if(!Dst->ReplaceSkeleton(Target,false)){UE_LOG(LogTemp,Error,TEXT("BODY_ANIM_IMPORT skeleton replace failed %s"),Entry.Name);++Failures;continue;}
  Dst->RefreshCacheData();
  Dst->PostEditChange();
  Dst->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Dst);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  const bool Saved=UPackage::SavePackage(Package,Dst,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
  UE_LOG(LogTemp,Display,TEXT("BODY_ANIM_IMPORT %s saved=%d length=%.3f keys=%d skeleton=%s dropped_notifies=%d"),*Path,Saved,Dst->GetPlayLength(),Dst->GetNumberOfSampledKeys(),*GetPathNameSafe(Dst->GetSkeleton()),Notifies);
  if(!Saved){++Failures;continue;}
  Clips.Add(Dst);
 }
 for(const TCHAR* Path:Measured)if(auto* Clip=LoadObject<UAnimSequence>(nullptr,Path))Clips.Add(Clip);else UE_LOG(LogTemp,Warning,TEXT("BODY_ANIM_IMPORT not found for measuring %s"),Path);
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
  UE_LOG(LogTemp,Display,TEXT("BATTLE_BODY_CLIP %s length=%.2f root_lock=%d%s"),*Clip->GetName(),Clip->GetPlayLength(),Clip->bForceRootLock,*Line);
 }
 Host->Destroy();
 return Failures;
}
#endif
