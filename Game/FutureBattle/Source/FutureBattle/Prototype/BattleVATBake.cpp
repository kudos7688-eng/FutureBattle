// Offline trial asset generation only. Original assets are never written.
#include "BattleVATBake.h"
#if WITH_EDITOR
#include "BattleVATData.h"
#include "BattleSoldierBody.h"
#include "AnimToTextureBPLibrary.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "StaticMeshAttributes.h"
#include "AssetCompilingManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "StaticMeshResources.h"

namespace BattleVATBake
{
// Quinn bakes into /Game/Prototype/DisplayLODTrial/; with -BattleSoldierBody=1 the RB01 body bakes into its own folder.
const FString& Root(){static const FString R=BattleSoldierBody::VATRoot();return R;}
bool Save(UObject* Object)
{
 auto* P=Object->GetOutermost(); P->MarkPackageDirty();
 FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(P,Object,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A);
}
template<class T> T* NewAsset(const FString& Name)
{
 const FString Path=Root()+Name;
 return NewObject<T>(CreatePackage(*Path),*Name,RF_Public|RF_Standalone);
}
UTexture2D* Texture(const FString& Name,int32 W,int32 H,const TArray<FFloat16Color>& Pixels)
{
 auto* T=NewAsset<UTexture2D>(Name);
 T->Source.Init(W,H,1,1,TSF_RGBA16F,reinterpret_cast<const uint8*>(Pixels.GetData()));
 T->SRGB=false; T->CompressionSettings=TC_HDR; T->MipGenSettings=TMGS_NoMipmaps;
 T->Filter=TF_Nearest; T->NeverStream=true; T->AddressX=TA_Clamp;T->AddressY=TA_Clamp;
 T->PostEditChange();return T;
}
struct FMeshSamples
{
 TArray<FVector3f> Ref, RefN, RefX;
 TArray<int32> SkinIndex;
 TArray<FFloat16Color> P,N,X;
 FBox Bounds=FBox(ForceInit);
 double MaxQuantization=0;
};
}
int32 BakeBattleVAT()
{
 using namespace BattleVATBake;
 if(!UEditorLoadingAndSavingUtils::NewBlankMap(false))return 9;
 // Refuse accidental overwrites even inside the generated asset folder.
 if(FPackageName::DoesPackageExist(Root()+TEXT("DA_BattleVAT")))
 {UE_LOG(LogTemp,Error,TEXT("VAT_BAKE existing output; choose a clean trial folder"));return 10;}
 auto* Skel=BattleSoldierBody::LoadMesh();
 if(!Skel)return 11;
 UE_LOG(LogTemp,Display,TEXT("VAT_BAKE body=%s root=%s"),*Skel->GetPathName(),*Root());
 auto* Data=NewAsset<UBattleVATData>(TEXT("DA_BattleVAT"));
 Data->Provenance=BattleSoldierBody::Version()==1
  ?TEXT("RB01 (Tripo-generated, Art/RB01_Tripo) LOD2 on SK_Mannequin; existing rifle clips and CC0 Kenney weapons. Original materials retained. Explicit 60Hz samples including endpoint. RGBA16F. Trial only.")
  :TEXT("UE5.8.4 bundled Quinn LOD2; existing rifle clips and CC0 Kenney weapons. Original materials retained. Explicit 60Hz samples including endpoint. RGBA16F. Trial only.");
 int32 Rows=0;
 for(const TCHAR* Name:{TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"),TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload")})
 {
  auto* Clip=LoadObject<UAnimSequence>(nullptr,Name);if(!Clip)return 12;
  FBattleVATClip C;C.Clip=Clip;C.Length=Clip->GetPlayLength();C.Start=Rows;
  C.Samples=FMath::CeilToInt(C.Length*Data->SampleRate)+1;Rows+=C.Samples;Data->Clips.Add(C);
 }
 Data->Meshes.SetNum(3);FMeshSamples Samples[3];
 Data->Meshes[0].Mesh=UAnimToTextureBPLibrary::ConvertSkeletalMeshToStaticMesh(Skel,Root()+TEXT("SM_Body"),2);
 for(int32 M=1;M<3;++M)
 {
  const FString Name=M==1?TEXT("SM_Carbine"):TEXT("SM_Support");
  auto* Src=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/Prototype/Weapons/")+Name));if(!Src)return 13;
  Data->Meshes[M].Mesh=DuplicateObject<UStaticMesh>(Src,CreatePackage(*(Root()+Name)),*Name);
 }
 const auto& LOD=Skel->GetResourceForRendering()->LODRenderData[2];
 double MaxMappingError=0;
 for(int32 M=0;M<3;++M)
 {
  auto& V=Data->Meshes[M];auto& S=Samples[M];if(!V.Mesh)return 14;
  auto* Desc=V.Mesh->GetMeshDescription(0);if(!Desc)return 15;
  FStaticMeshAttributes At(*Desc);auto Pos=At.GetVertexPositions();auto Ns=At.GetVertexInstanceNormals();auto Xs=At.GetVertexInstanceTangents();auto UV=At.GetVertexInstanceUVs();
  V.UVChannel=UV.GetNumChannels();UV.SetNumChannels(V.UVChannel+1);
  TMap<int32,int32> SkinToVAT;
  TMap<FVertexInstanceID,int32> Indices;
  for(const auto VI:Desc->VertexInstances().GetElementIDs())
  {
   const FVector3f P=Pos[Desc->GetVertexInstanceVertex(VI)];
   int32 I=S.Ref.Num();
   if(M==0)
   {
    int32 Best=INDEX_NONE;double Score=DBL_MAX;double Distance=0;
    for(uint32 J=0;J<LOD.GetNumVertices();++J)
    {
     const double D=FVector3f::DistSquared(P,LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(J));
     const double Q=D+0.001*(1.-FVector3f::DotProduct(Ns[VI],LOD.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(J)));
     if(Q<Score){Score=Q;Best=J;Distance=FMath::Sqrt(D);}
    }
    MaxMappingError=FMath::Max(MaxMappingError,Distance);
    if(const auto* Existing=SkinToVAT.Find(Best)){Indices.Add(VI,*Existing);continue;}
    SkinToVAT.Add(Best,I);S.SkinIndex.Add(Best);
   }
   S.Ref.Add(P);S.RefN.Add(Ns[VI]);S.RefX.Add(Xs[VI]);Indices.Add(VI,I);
  }
  V.Vertices=S.Ref.Num();V.Width=FMath::RoundUpToPowerOfTwo(V.Vertices);V.Height=Rows;
  if(V.Width>16384 || Rows>16384)return 20;
  for(const auto& Pair:Indices)UV.Set(Pair.Key,V.UVChannel,FVector2f((Pair.Value+.5f)/V.Width,.5f));
  // 0.01 cm (was 0.001 until 2026-11-23: RB01's LOD2 maps at 0.0014 cm). Still well below the 16-bit quantization (0.04-0.09 cm).
  if(MaxMappingError>0.01){UE_LOG(LogTemp,Error,TEXT("VAT_BAKE mapping error %.9f cm"),MaxMappingError);return 16;}
  S.P.SetNumZeroed(V.Width*Rows);S.N.SetNumZeroed(V.Width*Rows);S.X.SetNumZeroed(V.Width*Rows);
  auto& Settings=V.Mesh->GetSourceModel(0).BuildSettings;Settings.bUseFullPrecisionUVs=true;Settings.bRecomputeNormals=false;Settings.bRecomputeTangents=false;Settings.bGenerateLightmapUVs=false;
  V.Mesh->CommitMeshDescription(0);V.Mesh->Build(false);
 }
 auto* World=GEditor->GetEditorWorldContext().World();auto* Host=World->SpawnActor<AActor>();
 auto* Body=NewObject<USkeletalMeshComponent>(Host);Host->AddInstanceComponent(Body);Host->SetRootComponent(Body);
 Body->SetSkeletalMesh(Skel);Body->SetForcedLOD(3);Body->SetUpdateAnimationInEditor(true);
 Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Body->RegisterComponent();
 auto* Attachment=NewObject<UStaticMeshComponent>(Host);Host->AddInstanceComponent(Attachment);Attachment->SetupAttachment(Body,TEXT("hand_r"));Attachment->RegisterComponent();
 const FTransform Hand(FRotator(-11.885756,-81.776576,16.174298),FVector(-16.823813,7.097377,139.904362));
 const FTransform Gun(FRotator(0,180,0),FVector(-14.9,29.,143.5));const FTransform Grip=Gun.GetRelativeTransform(Hand);
 for(const auto& C:Data->Clips)
 {
  Body->PlayAnimation(C.Clip,true);
  for(int32 F=0;F<C.Samples;++F)
  {
   const float Time=FMath::Min(F/Data->SampleRate,C.Length);
   Body->SetPosition(Time,false);Body->TickAnimation(0,false);Body->RefreshBoneTransforms(nullptr);
   TArray<FFinalSkinVertex> Skin;Body->GetCPUSkinnedVertices(Skin,2);
   if(Skin.Num()!=int32(LOD.GetNumVertices()))return 17;
   const FTransform Socket=Body->GetSocketTransform(TEXT("hand_r"),RTS_Component);
   for(int32 M=0;M<3;++M)
   {
    auto& V=Data->Meshes[M];auto& S=Samples[M];FTransform W=Grip;W.SetScale3D(FVector(M==1?.62:1.));W=W*Socket;
    for(int32 I=0;I<V.Vertices;++I)
    {
     FVector3f P,N,X;float Sign=1;
     if(M==0)
     {
      const auto& K=Skin[S.SkinIndex[I]];P=K.Position;N=K.TangentZ.ToFVector3f();X=K.TangentX.ToFVector3f();
      Sign=FVector3f::DotProduct(FVector3f::CrossProduct(N,X),K.GetTangentY())<0?-1:1;
     }
     else {P=FVector3f(W.TransformPosition(FVector(S.Ref[I])));N=FVector3f(W.TransformVectorNoScale(FVector(S.RefN[I])));X=FVector3f(W.TransformVectorNoScale(FVector(S.RefX[I])));}
     const int32 Index=(C.Start+F)*V.Width+I;const FVector3f Delta=P-S.Ref[I];
     S.P[Index]=FFloat16Color(FLinearColor(Delta.X,Delta.Y,Delta.Z,1));S.N[Index]=FFloat16Color(FLinearColor(N.X,N.Y,N.Z,1));S.X[Index]=FFloat16Color(FLinearColor(X.X,X.Y,X.Z,Sign));
     const auto Q=S.P[Index].GetFloats();S.MaxQuantization=FMath::Max(S.MaxQuantization,double(FVector3f::Distance(Delta,FVector3f(Q.R,Q.G,Q.B))));S.Bounds+=FVector(P);
    }
   }
  }
  UE_LOG(LogTemp,Display,TEXT("VAT_BAKE clip=%s samples=%d endpoint=%.9f"),*C.Clip->GetName(),C.Samples,C.Length);
 }
 Host->Destroy();
 for(int32 M=0;M<3;++M)
 {
  auto& V=Data->Meshes[M];auto& S=Samples[M];const FString Prefix=FString::Printf(TEXT("T_VAT%d_"),M);
  V.Position=Texture(Prefix+TEXT("P"),V.Width,Rows,S.P);V.Normal=Texture(Prefix+TEXT("N"),V.Width,Rows,S.N);V.Tangent=Texture(Prefix+TEXT("X"),V.Width,Rows,S.X);
  const FBox Original=V.Mesh->GetBoundingBox();V.Mesh->SetPositiveBoundsExtension((S.Bounds.Max-Original.Max).ComponentMax(FVector::ZeroVector));V.Mesh->SetNegativeBoundsExtension((Original.Min-S.Bounds.Min).ComponentMax(FVector::ZeroVector));
  V.Mesh->PostEditChange();
  UE_LOG(LogTemp,Display,TEXT("VAT_BAKE mesh=%d vertices=%d texture=%dx%d uv=%d mapping_cm=%.9f quantization_cm=%.9f"),M,V.Vertices,V.Width,Rows,V.UVChannel,MaxMappingError,S.MaxQuantization);
 }
 FAssetCompilingManager::Get().FinishAllCompilation();
 for(auto& V:Data->Meshes)if(!Save(V.Position)||!Save(V.Normal)||!Save(V.Tangent)||!Save(V.Mesh))return 18;
 if(!Save(Data))return 19;
 UE_LOG(LogTemp,Display,TEXT("VAT_BAKE_SAMPLES_RESULT failures=0 materials_pending=1"));return 0;
}
int32 ValidateBattleVAT()
{
 auto* D=LoadObject<UBattleVATData>(nullptr,*(BattleVATBake::Root()+TEXT("DA_BattleVAT")));if(!D)return 40;
 auto* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);auto* A=World->SpawnActor<AActor>();auto* M=NewObject<USkeletalMeshComponent>(A);
 auto* Skel=BattleSoldierBody::LoadMesh();M->SetSkeletalMesh(Skel);M->RegisterComponent();M->PlayAnimation(D->Clips[0].Clip,true);M->SetPosition(0,false);M->TickAnimation(0,false);M->RefreshBoneTransforms(nullptr);
 TArray<FFinalSkinVertex> Skin;M->GetCPUSkinnedVertices(Skin,2);
 const auto& LOD=Skel->GetResourceForRendering()->LODRenderData[2];
 for(int32 G=0;G<3;++G)
 {
  const auto& V=D->Meshes[G];FAssetCompilingManager::Get().FinishAllCompilation();const auto& R=V.Mesh->GetRenderData()->LODResources[0];
  TArray64<uint8> Bytes;V.Position->Source.GetMipData(Bytes,0);const auto* Pixels=reinterpret_cast<const FFloat16Color*>(Bytes.GetData());
  double MaxError=0;TSet<int32> Indices;int32 Invalid=0;
  for(int32 I=0;I<R.GetNumVertices();++I)
  {
   const auto P=R.VertexBuffers.PositionVertexBuffer.VertexPosition(I);const auto UV=R.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(I,V.UVChannel);const int32 Index=FMath::FloorToInt(UV.X*V.Width);
   if(Index<0 || Index>=V.Vertices){++Invalid;continue;}Indices.Add(Index);
   if(G==0)
   {
    int32 Best=0;double Dist=DBL_MAX;const auto N=R.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(I);
    for(uint32 J=0;J<LOD.GetNumVertices();++J){double E=FVector3f::DistSquared(P,LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(J))+.001*(1-FVector3f::DotProduct(N,LOD.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(J)));if(E<Dist){Dist=E;Best=J;}}
    const auto Q=Pixels[Index].GetFloats();const FVector3f Result=P+FVector3f(Q.R,Q.G,Q.B);MaxError=FMath::Max(MaxError,double(FVector3f::Distance(Result,Skin[Best].Position)));
   }
  }
  UE_LOG(LogTemp,Display,TEXT("VAT_VALIDATE mesh=%d render_vertices=%d uv_channels=%d unique_indices=%d expected=%d invalid=%d cpu_error_cm=%.9f texture_actual=%dx%d format=%d full_uv=%d"),G,R.GetNumVertices(),R.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(),Indices.Num(),V.Vertices,Invalid,MaxError,V.Position->GetSizeX(),V.Position->GetSizeY(),int32(V.Position->GetPixelFormat()),V.Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs);
 }
 A->Destroy();return 0;
}
#endif
