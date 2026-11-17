// Offline asset import (2026-11-17, user request): Lyra's rifle crouch clips for the stance visuals (-BattleStanceAnim=1).
// The Lyra source files are copied unchanged to their Lyra path (/Game/Characters/Heroes/Mannequin/...) only as a staging
// input; this function duplicates each clip to /Game/Characters/Mannequins/Anims/Rifle/Crouch/, moves it to the project's
// UE5 mannequin skeleton (same bone names), and drops the Lyra-only notifies (footstep effects) whose classes are not in
// this project. (Lyra's animation modifier entries stay as unresolved editor-only data; they never run at game time.) Existing assets are never written. Run: FutureBattle.uproject -run=BattleVATBake -ImportStanceAnims
#include "BattleVATBake.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

int32 ImportBattleStanceAnims()
{
 auto* Target=LoadObject<USkeleton>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin"));
 if(!Target){UE_LOG(LogTemp,Error,TEXT("STANCE_ANIM_IMPORT missing target skeleton"));return 1;}
 int32 Failures=0;
 for(const TCHAR* Name:{TEXT("MM_Rifle_Crouch_Idle"),TEXT("MM_Rifle_Crouch_Walk_Fwd"),TEXT("MM_Rifle_Crouch_Entry"),TEXT("MM_Rifle_Crouch_Exit")})
 {
  const FString Source=FString(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/"))+Name;
  const FString Path=FString(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Crouch/"))+Name;
  auto* Src=LoadObject<UAnimSequence>(nullptr,*Source);
  if(!Src){UE_LOG(LogTemp,Error,TEXT("STANCE_ANIM_IMPORT missing source %s"),*Source);++Failures;continue;}
  if(FPackageName::DoesPackageExist(Path)){UE_LOG(LogTemp,Display,TEXT("STANCE_ANIM_IMPORT exists %s (kept)"),*Path);continue;}
  UPackage* Package=CreatePackage(*Path);
  auto* Dst=DuplicateObject<UAnimSequence>(Src,Package,Name);
  Dst->SetFlags(RF_Public|RF_Standalone);
  const int32 Notifies=Dst->Notifies.Num();
  Dst->Notifies.Empty();
  if(!Dst->ReplaceSkeleton(Target,false)){UE_LOG(LogTemp,Error,TEXT("STANCE_ANIM_IMPORT skeleton replace failed %s"),Name);++Failures;continue;}
  Dst->RefreshCacheData();
  Dst->PostEditChange();
  Dst->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Dst);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  const bool Saved=UPackage::SavePackage(Package,Dst,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
  UE_LOG(LogTemp,Display,TEXT("STANCE_ANIM_IMPORT %s saved=%d length=%.3f keys=%d skeleton=%s dropped_notifies=%d"),*Path,Saved,Dst->GetPlayLength(),Dst->GetNumberOfSampledKeys(),*GetPathNameSafe(Dst->GetSkeleton()),Notifies);
  if(!Saved)++Failures;
 }
 return Failures;
}
#endif
