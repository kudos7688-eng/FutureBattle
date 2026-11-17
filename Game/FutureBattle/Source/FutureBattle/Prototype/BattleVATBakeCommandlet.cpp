#include "BattleVATBakeCommandlet.h"
#include "BattleVATData.h"
#include "BattleVATBake.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#endif
UBattleVATBakeCommandlet::UBattleVATBakeCommandlet(){IsClient=true;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UBattleVATBakeCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 if(FParse::Param(*Params,TEXT("Materials"))) return BuildBattleVATMaterials();
 if(FParse::Param(*Params,TEXT("Validate"))) return ValidateBattleVAT();
 if(FParse::Param(*Params,TEXT("RepairMetadata"))) return RepairBattleVATMetadata();
 if(FParse::Param(*Params,TEXT("ImportStanceAnims"))) return ImportBattleStanceAnims();
 if(!FParse::Param(*Params,TEXT("Inspect"))) return BakeBattleVAT();
 auto* Skel=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
 if(!Skel) return 1;
 for(int32 I=0;I<Skel->GetResourceForRendering()->LODRenderData.Num();++I)
  UE_LOG(LogTemp,Display,TEXT("VAT_INSPECT lod=%d vertices=%d"),I,Skel->GetResourceForRendering()->LODRenderData[I].GetNumVertices());
 TArray<UMaterialInterface*> Mats;for(const auto& M:Skel->GetMaterials()) Mats.AddUnique(M.MaterialInterface);
 for(const TCHAR* Name:{TEXT("SM_Carbine"),TEXT("SM_Support")})
 {
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Game/Prototype/Weapons/"))+Name));
  if(!Mesh) return 2;for(const auto& M:Mesh->GetStaticMaterials()) Mats.AddUnique(M.MaterialInterface);
 }
 for(auto* MI:Mats)
 {
  auto* M=MI->GetMaterial();UE_LOG(LogTemp,Display,TEXT("VAT_MATERIAL %s base=%s attributes=%d tangent_normal=%d"),*MI->GetPathName(),*M->GetPathName(),M->bUseMaterialAttributes,M->bTangentSpaceNormal);
  for(const auto& E:M->GetExpressions())
  {
   FString Inputs;for(int32 I=0;I<E->GetInputsView().Num();++I) Inputs+=FString::Printf(TEXT(" %d:%s"),I,*E->GetInputName(I).ToString());
   UE_LOG(LogTemp,Display,TEXT("VAT_EXPR %s %s"),*E->GetClass()->GetName(),*Inputs);
  }
 }
 for(const TCHAR* Name:{TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload")})
 {
  auto* A=LoadObject<UAnimSequence>(nullptr,Name);if(!A)return 3;
  UE_LOG(LogTemp,Display,TEXT("VAT_CLIP %s length=%.9f keys=%d rate=%.9f notifies=%d"),Name,A->GetPlayLength(),A->GetNumberOfSampledKeys(),A->RateScale,A->Notifies.Num());
 }
 return 0;
#else
 return 1;
#endif
}

