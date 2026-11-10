#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"
class USkeletalMeshComponent;
class UStaticMeshComponent;
// Test-only state. Components are owned by a world Actor, never combat units.
struct FT5RenderDiagnostic {
 FString Stage,Profile;
 TArray<TPair<FString,FString>> Cases;
 int32 Run=1,Requested=500,Registrations=0,Wakes=0,PreviousShown=0;
 bool Keep[500]={},Inside[500]={},Required[500]={},PreviousInside[500]={},WasSleeping[500]={};
 struct FScreenSet {uint32 Bits[16]={};};
 TMap<FString,TArray<FScreenSet>> ReferenceScreen;
 bool Started=false;
 float SleepPoseTime[500]={};
 int32 SleepFrame[500]={};
 TArray<USkeletalMeshComponent*> Extra;
 TArray<UStaticMeshComponent*> ExtraWeapons;
 class UInstancedStaticMeshComponent* Markers=nullptr;
 int32 LastSeen[500]={}; FVector LastKnown[500]={};
 TArray<FVector> MarkerPositions;
 TSharedPtr<FJsonObject> Settings=MakeShared<FJsonObject>();
 TSharedPtr<FJsonObject> Last=MakeShared<FJsonObject>();
 TArray<TArray<TStrongObjectPtr<UMaterialInterface>>> OriginalMaterials;
 TStrongObjectPtr<UMaterialInterface> SimpleMaterial;
 TMap<class UPrimitiveComponent*,bool> OriginalShadows;
 TArray<TSharedPtr<FJsonValue>> MaterialStatistics;
 TSharedPtr<FJsonObject> ConditionSettings=MakeShared<FJsonObject>();
 bool FarShadow[500]={},FarMaterial[500]={};
 double BaseShadowBias=0.,SelectedHysteresis=.1;
 FString LODPolicy=TEXT("Dist20_50");
 bool LODStress=false;int32 LODProbe=-1;
 int32 PreviousLOD[500]={},RequestedLOD[500]={};
 int32 Errors=0;
};
