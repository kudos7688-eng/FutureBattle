#include "BattleSquadTrial.h"
#include "BattleRenderDiagnostic.h"
#include "BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Shader.h"
#include "ShaderCompiler.h"
#include "VertexFactory.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"

namespace {
TSharedPtr<FJsonObject> MaterialStats(UMaterialInterface* Material) {
 auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("asset"),Material->GetPathName());
 O->SetStringField(TEXT("base_material"),Material->GetMaterial()->GetPathName());
 O->SetNumberField(TEXT("blend_mode"),int32(Material->GetBlendMode()));
 O->SetNumberField(TEXT("shader_platform"),int32(GMaxRHIShaderPlatform));
 const FMaterialResource* Resource=Material->GetMaterialResource(GMaxRHIShaderPlatform);
 O->SetBoolField(TEXT("shader_map_present"),Resource && Resource->GetGameThreadShaderMap());
 TArray<TSharedPtr<FJsonValue>> Rows;bool HasInstructions=false;
 if(Resource && Resource->GetGameThreadShaderMap()) {
  uint32 VS=0,PS=0;Resource->GetEstimatedNumTextureSamples(VS,PS);
  O->SetNumberField(TEXT("estimated_texture_samples_vs"),VS);O->SetNumberField(TEXT("estimated_texture_samples_ps"),PS);
  TMap<FShaderId,TShaderRef<FShader>> Shaders;Resource->GetGameThreadShaderMap()->GetShaderList(Shaders);
  for(const auto& Pair:Shaders) {
   const FString Name=Pair.Key.Type->GetName();if(!Name.Contains(TEXT("BasePass"))) continue;
   const auto& S=Pair.Value;auto R=MakeShared<FJsonObject>();
   R->SetStringField(TEXT("shader"),Name);R->SetStringField(TEXT("vertex_factory"),Pair.Key.VFType?Pair.Key.VFType->GetName():TEXT("none"));
   R->SetNumberField(TEXT("permutation"),Pair.Key.PermutationId);R->SetNumberField(TEXT("frequency"),int32(S->GetFrequency()));
   R->SetStringField(TEXT("frequency_name"),S->GetFrequency()==SF_Pixel?TEXT("pixel"):S->GetFrequency()==SF_Vertex?TEXT("vertex"):TEXT("other"));
   R->SetNumberField(TEXT("instructions"),S->GetNumInstructions());R->SetNumberField(TEXT("samplers"),S->GetNumTextureSamplers());
   HasInstructions|=S->GetNumInstructions()>0;Rows.Add(MakeShared<FJsonValueObject>(R));
  }
 }
 O->SetBoolField(TEXT("instruction_counts_available"),HasInstructions);
 if(!HasInstructions) O->SetStringField(TEXT("instruction_count_note"),TEXT("No nonzero compiler instruction statistics in the loaded shader map; zero must not be interpreted as zero-cost shading."));
 O->SetArrayField(TEXT("base_pass_shaders"),Rows);return O;
}
}

bool UBattleSquadTrial::ConfigureT5Material() {
#if WITH_EDITOR
 auto& D=*T5Diagnostic;
 UMaterial* Base=LoadObject<UMaterial>(nullptr,TEXT("/Game/Prototype/Materials/M_ArenaCover.M_ArenaCover"));
 if(!Base) return false;
 // In-memory derivative of an existing opaque constant-color asset. Never save or alter the source asset.
 UMaterial* Simple=DuplicateObject<UMaterial>(Base,GetTransientPackage(),TEXT("T5_SimpleFarMaterial"));
 if(!Simple) return false;
 Simple->SetUsageByFlag(MATUSAGE_SkeletalMesh,true);
 Simple->PostEditChange();Simple->CacheShaders(EMaterialShaderPrecompileMode::Default);D.SimpleMaterial.Reset(Simple);
 TSet<UMaterialInterface*> Unique;
 for(auto& M:Members) {
  auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display;auto& Slots=D.OriginalMaterials.AddDefaulted_GetRef();
  for(int32 J=0;J<Mesh->GetNumMaterials();++J) {auto* Mat=Mesh->GetMaterial(J);Slots.Emplace(Mat);if(Mat) Unique.Add(Mat);}
  D.OriginalShadows.Add(Mesh,Mesh->CastShadow);
  TArray<USceneComponent*> Children;Mesh->GetChildrenComponents(true,Children);
  for(auto* Child:Children) if(auto* P=Cast<UPrimitiveComponent>(Child)) D.OriginalShadows.Add(P,P->CastShadow);
 }
 if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
 for(auto* Mat:Unique) D.MaterialStatistics.Add(MakeShared<FJsonValueObject>(MaterialStats(Mat)));
 D.MaterialStatistics.Add(MakeShared<FJsonValueObject>(MaterialStats(Simple)));
 auto* Bias=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"));
 if(!Bias) return false;D.BaseShadowBias=Bias->GetFloat();return true;
#else
 return false;
#endif
}

void UBattleSquadTrial::SelectT5Material() {
 auto& D=*T5Diagnostic;
 for(int32 I=0;I<500;++I) {
  D.FarMaterial[I]=false;D.FarShadow[I]=false;
  auto* Mesh=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;
  for(int32 J=0;J<D.OriginalMaterials[I].Num();++J) Mesh->SetMaterial(J,D.OriginalMaterials[I][J].Get());
 }
 for(auto& Pair:D.OriginalShadows) Pair.Key->SetCastShadow(Pair.Value);
 const double Bias=D.BaseShadowBias+(T5Filter.StartsWith(TEXT("Bias1"))?1.:0.);
 auto* Var=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"));
 Var->Set(float(Bias),ECVF_SetByCode);
 D.ConditionSettings=MakeShared<FJsonObject>();D.ConditionSettings->SetNumberField(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"),Var->GetFloat());
 D.ConditionSettings->SetNumberField(TEXT("base_directional_bias"),D.BaseShadowBias);
 D.ConditionSettings->SetNumberField(TEXT("hysteresis_percent"),D.SelectedHysteresis*100.);
}

void UBattleSquadTrial::ApplyT5Material(int32 I,const FBattleReplayUnit* U,const FBattleReplayCamera& C) {
 auto& D=*T5Diagnostic;auto* Mesh=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;
 const double Distance=FVector::Distance(FVector(U[I].X,U[I].Y,U[I].Z),FVector(C.X,C.Y,C.Z));
 const double ShadowCut=T5Filter==TEXT("Shadow30") || T5Filter==TEXT("Combined")?3000.:T5Filter==TEXT("Shadow60")?6000.:0.;
 const double MaterialCut=T5Filter==TEXT("Material40") || T5Filter==TEXT("Combined")?4000.:0.;
 auto Far=[&](double Cut,bool WasFar){return Cut>0 && Distance>Cut*(WasFar?1.-D.SelectedHysteresis:1.+D.SelectedHysteresis);};
 const bool ShadowFar=Far(ShadowCut,D.FarShadow[I]),MaterialFar=Far(MaterialCut,D.FarMaterial[I]);
 auto Add=[&](const TCHAR* Key,double N){double V=0;D.Last->TryGetNumberField(Key,V);D.Last->SetNumberField(Key,V+N);};
 Add(TEXT("shadow_changes"),ShadowFar!=D.FarShadow[I]);Add(TEXT("material_changes"),MaterialFar!=D.FarMaterial[I]);
 if(ShadowFar!=D.FarShadow[I]) {
  Mesh->SetCastShadow(D.OriginalShadows.FindChecked(Mesh) && !ShadowFar);
  TArray<USceneComponent*> Children;Mesh->GetChildrenComponents(true,Children);
  for(auto* Child:Children) if(auto* P=Cast<UPrimitiveComponent>(Child)) P->SetCastShadow(D.OriginalShadows.FindChecked(P) && !ShadowFar);
 }
 if(MaterialFar!=D.FarMaterial[I]) for(int32 J=0;J<D.OriginalMaterials[I].Num();++J) Mesh->SetMaterial(J,MaterialFar?D.SimpleMaterial.Get():D.OriginalMaterials[I][J].Get());
 D.FarShadow[I]=ShadowFar;D.FarMaterial[I]=MaterialFar;
 Add(TEXT("shadow_state_errors"),bool(Mesh->CastShadow)!=(D.OriginalShadows.FindChecked(Mesh) && !ShadowFar));
 for(int32 J=0;J<D.OriginalMaterials[I].Num();++J) Add(TEXT("material_state_errors"),Mesh->GetMaterial(J)!=(MaterialFar?D.SimpleMaterial.Get():D.OriginalMaterials[I][J].Get()));
 Add(TEXT("shadow_suppressed_bodies"),ShadowFar && ReplayVisible[I]);Add(TEXT("simplified_material_bodies"),MaterialFar && ReplayVisible[I]);
}
