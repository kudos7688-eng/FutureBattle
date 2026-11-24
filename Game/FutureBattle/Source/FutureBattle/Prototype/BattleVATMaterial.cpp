#include "BattleVATBake.h"
#if WITH_EDITOR
#include "BattleVATData.h"
#include "BattleSoldierBody.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionPreviousFrameSwitch.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionAdd.h"
#include "AssetCompilingManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "StaticMeshAttributes.h"

namespace BattleVATMaterial
{
template<class T> T* Node(UMaterial* M){auto* N=NewObject<T>(M);M->GetExpressionCollection().AddExpression(N);return N;}
FExpressionInput Input(UMaterialExpression* E){FExpressionInput I;I.Connect(0,E);return I;}
void Arg(UMaterialExpressionCustom* C,const TCHAR* Name,FExpressionInput I){FCustomInput A;A.InputName=Name;A.Input=I;C->Inputs.Add(A);}
UMaterialExpression* Sample(UMaterial* M,UTexture2D* T,int32 UV,int32 Width,int32 Base)
{
 auto* Tex=Node<UMaterialExpressionTextureObject>(M);Tex->Texture=T;Tex->SamplerType=SAMPLERTYPE_LinearColor;
 auto* Coord=Node<UMaterialExpressionTextureCoordinate>(M);Coord->CoordinateIndex=UV;
 auto* C=Node<UMaterialExpressionCustom>(M);C->Inputs.Reset();C->OutputType=CMOT_Float4;
 C->Code=FString::Printf(TEXT("int x=(int)floor(UV.x*%d);return lerp(T.Load(int3(x,(int)A,0)),T.Load(int3(x,(int)B,0)),F);"),Width);
 Arg(C,TEXT("T"),Input(Tex));Arg(C,TEXT("UV"),Input(Coord));
 for(int32 I=0;I<3;++I){auto* D=Node<UMaterialExpressionPerInstanceCustomData>(M);D->DataIndex=Base+I;Arg(C,I==0?TEXT("A"):I==1?TEXT("B"):TEXT("F"),Input(D));}
 return C;
}
UMaterialExpression* XYZ(UMaterial* M,UMaterialExpression* E)
{
 auto* C=Node<UMaterialExpressionCustom>(M);C->Inputs.Reset();C->OutputType=CMOT_Float3;C->Code=TEXT("return V.xyz;");Arg(C,TEXT("V"),Input(E));return C;
}
UMaterialExpression* WorldVector(UMaterial* M,UMaterialExpression* E)
{
 auto* T=Node<UMaterialExpressionTransform>(M);T->TransformSourceType=TRANSFORMSOURCE_Instance;T->TransformType=TRANSFORM_World;T->Input=Input(E);return T;
}
void Modify(UMaterial* M,const FBattleVATMesh& V)
{
 check(!M->bUseMaterialAttributes && M->bTangentSpaceNormal);
 auto* E=M->GetEditorOnlyData();
 auto* Switch=Node<UMaterialExpressionPreviousFrameSwitch>(M);
 Switch->CurrentFrame=Input(XYZ(M,Sample(M,V.Position,V.UVChannel,V.Width,0)));
 Switch->PreviousFrame=Input(XYZ(M,Sample(M,V.Position,V.UVChannel,V.Width,3)));
 auto* Delta=WorldVector(M,Switch);
 if(E->WorldPositionOffset.Expression){auto* Add=Node<UMaterialExpressionAdd>(M);Add->A=E->WorldPositionOffset;Add->B=Input(Delta);Delta=Add;}
 E->WorldPositionOffset.Connect(0,Delta);
 auto* X=Node<UMaterialExpressionVertexInterpolator>(M);X->Input=Input(Sample(M,V.Tangent,V.UVChannel,V.Width,0));
 auto* Z=Node<UMaterialExpressionVertexInterpolator>(M);Z->Input=Input(Sample(M,V.Normal,V.UVChannel,V.Width,0));
 auto* Combine=Node<UMaterialExpressionCustom>(M);Combine->Inputs.Reset();Combine->OutputType=CMOT_Float3;
 Combine->Code=TEXT("float3 z=normalize(Z.xyz);float3 x=normalize(X.xyz-z*dot(X.xyz,z));float3 y=cross(z,x)*X.w;return normalize(x*N.x+y*N.y+z*N.z);");
 Arg(Combine,TEXT("X"),Input(X));Arg(Combine,TEXT("Z"),Input(Z));
 FExpressionInput Normal=E->Normal;
 if(!Normal.Expression){auto* Default=Node<UMaterialExpressionConstant3Vector>(M);Default->Constant=FLinearColor(0,0,1);Normal=Input(Default);}
 Arg(Combine,TEXT("N"),Normal);E->Normal.Connect(0,WorldVector(M,Combine));M->bTangentSpaceNormal=false;
 M->bUsedWithInstancedStaticMeshes=true;M->PostEditChange();
}
}
int32 BuildBattleVATMaterials()
{
 using namespace BattleVATMaterial;
 const FString Root=BattleSoldierBody::VATRoot(); // Quinn's folder, or RB01's with -BattleSoldierBody=1
 auto* Data=LoadObject<UBattleVATData>(nullptr,*(Root+TEXT("DA_BattleVAT")));if(!Data)return 30;
 TArray<UObject*> Outputs;
 for(int32 Index=0;Index<Data->Meshes.Num();++Index)
 {
  auto& V=Data->Meshes[Index];TMap<UMaterialInterface*,UMaterialInterface*> Copies;
  TFunction<UMaterialInterface*(UMaterialInterface*)> Clone=[&](UMaterialInterface* Original)->UMaterialInterface*
  {
   if(auto* Found=Copies.Find(Original))return *Found;
   const FString Name=FString::Printf(TEXT("VAT%d_%s"),Index,*Original->GetName());const FString Path=Root+Name;
   if(FPackageName::DoesPackageExist(Path))return nullptr;
   auto* Copy=DuplicateObject<UMaterialInterface>(Original,CreatePackage(*Path),*Name);Copies.Add(Original,Copy);
   if(auto* MI=Cast<UMaterialInstanceConstant>(Copy))
   {auto* Parent=Clone(CastChecked<UMaterialInstanceConstant>(Original)->Parent);if(!Parent)return nullptr;MI->SetParentEditorOnly(Parent);MI->PostEditChange();}
   else if(auto* M=Cast<UMaterial>(Copy))
   {if(M->bUseMaterialAttributes || !M->bTangentSpaceNormal)return nullptr;Modify(M,V);}
   else return nullptr;
   Outputs.Add(Copy);return Copy;
  };
  for(int32 Slot=0;Slot<V.Mesh->GetStaticMaterials().Num();++Slot)
  {auto* M=Clone(V.Mesh->GetMaterial(Slot));if(!M){UE_LOG(LogTemp,Error,TEXT("VAT_MATERIAL failed mesh=%d slot=%d"),Index,Slot);return 31;}V.Mesh->SetMaterial(Slot,M);}
  V.Mesh->PostEditChange();Outputs.Add(V.Mesh);
 }
 FAssetCompilingManager::Get().FinishAllCompilation();
 for(auto* O:Outputs)
 {
  auto* P=O->GetOutermost();P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;
  if(!UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A))return 32;
 }
 UE_LOG(LogTemp,Display,TEXT("VAT_MATERIAL_RESULT failures=0 assets=%d"),Outputs.Num());return 0;
}
int32 RepairBattleVATMetadata()
{
 auto* D=LoadObject<UBattleVATData>(nullptr,*(BattleSoldierBody::VATRoot()+TEXT("DA_BattleVAT")));if(!D)return 41;
 TSet<UObject*> Outputs;Outputs.Add(D);
 for(auto& V:D->Meshes)
 {
  V.Height=V.Position->Source.GetSizeY();V.Width=V.Position->Source.GetSizeX();V.Vertices=0;
  FStaticMeshAttributes A(*V.Mesh->GetMeshDescription(0));V.UVChannel=A.GetVertexInstanceUVs().GetNumChannels()-1;
  TArray64<uint8> Bytes;V.Position->Source.GetMipData(Bytes,0);auto* P=reinterpret_cast<const FFloat16Color*>(Bytes.GetData());
  for(int32 I=0;I<V.Width;++I)if(P[I].A.GetFloat()>0)++V.Vertices;
  for(const auto& Slot:V.Mesh->GetStaticMaterials())
  {
   auto* M=Slot.MaterialInterface->GetMaterial();if(Outputs.Contains(M))continue;
   for(auto E:M->GetExpressions())if(auto* C=Cast<UMaterialExpressionCustom>(E))
    if(C->Code.StartsWith(TEXT("int x=(int)floor(UV.x*")))
     for(auto& Input:C->Inputs)if(Input.InputName==TEXT("UV"))CastChecked<UMaterialExpressionTextureCoordinate>(Input.Input.Expression)->CoordinateIndex=V.UVChannel;
   M->PostEditChange();Outputs.Add(M);
  }
  UE_LOG(LogTemp,Display,TEXT("VAT_METADATA mesh=%s uv=%d vertices=%d height=%d"),*V.Mesh->GetName(),V.UVChannel,V.Vertices,V.Height);
 }
 FAssetCompilingManager::Get().FinishAllCompilation();
 for(auto* O:Outputs){auto* P=O->GetOutermost();P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;if(!UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A))return 42;}
 return 0;
}
#endif
