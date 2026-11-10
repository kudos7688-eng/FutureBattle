#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleVATData.generated.h"
class UStaticMesh;
class UTexture2D;
class UAnimSequence;
USTRUCT()
struct FBattleVATClip
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UAnimSequence> Clip;
 UPROPERTY() float Length=0;
 UPROPERTY() int32 Start=0;
 UPROPERTY() int32 Samples=0;
};
USTRUCT()
struct FBattleVATMesh
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UStaticMesh> Mesh;
 UPROPERTY() TObjectPtr<UTexture2D> Position;
 UPROPERTY() TObjectPtr<UTexture2D> Normal;
 UPROPERTY() TObjectPtr<UTexture2D> Tangent;
 UPROPERTY() int32 Width=0;
 UPROPERTY() int32 Height=0;
 UPROPERTY() int32 UVChannel=0;
 UPROPERTY() int32 Vertices=0;
};
UCLASS()
class UBattleVATData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY() TArray<FBattleVATClip> Clips;
 // Body LOD2, carbine, support weapon. Positions are local ref-pose deltas.
 UPROPERTY() TArray<FBattleVATMesh> Meshes;
 UPROPERTY() float SampleRate=60;
 UPROPERTY() FString Provenance;
};
