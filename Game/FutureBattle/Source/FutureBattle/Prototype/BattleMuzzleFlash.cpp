// Muzzle flash (2026-11-18): a small pool of glowing ellipsoids (engine sphere + the engine's unlit additive
// material), shown at the muzzle for a few frames. No collision, no navigation, no overlap events: display only.
#include "BattleMuzzleFlash.h"
#include "FutureBattle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

namespace
{
 constexpr int32 PoolSize=48;
 struct FFlashPool
 {
  TWeakObjectPtr<AActor> Host;
  TArray<TWeakObjectPtr<UStaticMeshComponent>> Parts;
  TArray<uint32> Serial; // which use of a part a pending hide belongs to
  int32 Next=0;
 };
 TMap<TWeakObjectPtr<UWorld>,FFlashPool> Pools;

 void Build(UWorld* World,FFlashPool& P)
 {
  P=FFlashPool();
  auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
  FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
  AActor* Host=World->SpawnActor<AActor>(Spawn);
  if(!Host||!Sphere)return;
  UMaterialInstanceDynamic* Glow=Base?UMaterialInstanceDynamic::Create(Base,Host):nullptr;
  if(Glow)
  {
   // Whatever the engine material calls its colour and texture: a hot yellow, untextured.
   TArray<FMaterialParameterInfo> Infos;TArray<FGuid> Ids;FString Names;
   Glow->GetAllVectorParameterInfo(Infos,Ids);
   for(const auto& I:Infos){Glow->SetVectorParameterValueByInfo(I,FLinearColor(14.f,7.f,1.6f));Names+=I.Name.ToString()+TEXT(" ");}
   Infos.Reset();Ids.Reset();Glow->GetAllTextureParameterInfo(Infos,Ids);
   if(auto* White=LoadObject<UTexture>(nullptr,TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
    for(const auto& I:Infos){Glow->SetTextureParameterValueByInfo(I,White);Names+=I.Name.ToString()+TEXT(" ");}
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MUZZLE_FLASH material parameters: %s"),*Names);
  }
  for(int32 I=0;I<PoolSize;++I)
  {
   auto* C=NewObject<UStaticMeshComponent>(Host);
   C->SetStaticMesh(Sphere);if(Glow)C->SetMaterial(0,Glow);
   C->SetMobility(EComponentMobility::Movable);
   C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);
   C->SetCastShadow(false);C->SetVisibility(false);
   C->RegisterComponent();Host->AddInstanceComponent(C);
   P.Parts.Add(C);P.Serial.Add(0);
  }
  P.Host=Host;
 }
}

void BattleMuzzleFlash::Show(UWorld* World,const FVector& Muzzle,const FVector& Direction,float Size,float Seconds)
{
 static const bool bOff=FParse::Param(FCommandLine::Get(),TEXT("BattleNoMuzzleFlash"));
 if(bOff||!World||!FApp::CanEverRender()||Direction.IsNearlyZero()||Muzzle.ContainsNaN())return;
 FFlashPool* P=Pools.Find(World);
 if(!P||!P->Host.IsValid())
 {
  for(auto It=Pools.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
  P=&Pools.FindOrAdd(World);Build(World,*P);
 }
 if(!P->Parts.Num())return;
 const int32 Index=P->Next;P->Next=(P->Next+1)%P->Parts.Num();
 UStaticMeshComponent* C=P->Parts[Index].Get();if(!C)return;
 // Seen from far away (the command view) a 30 cm flash is under a pixel: grow it with the camera distance.
 float S=Size;
 if(const auto* PC=World->GetFirstPlayerController();PC&&PC->PlayerCameraManager)
  S*=FMath::Clamp(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Muzzle)/1500.,1.,4.);
 S*=FMath::FRandRange(.8f,1.2f); // display randomness only (not the battle's random streams)
 const FVector Forward=Direction.GetSafeNormal();
 FRotator Rotation=Forward.Rotation();Rotation.Roll=FMath::FRandRange(0.f,360.f);
 C->SetWorldTransform(FTransform(Rotation,Muzzle+Forward*(10.f*S),FVector(.22f,.08f,.08f)*S));
 C->SetVisibility(true);
 const uint32 Serial=++P->Serial[Index];
 FTimerHandle Handle;
 World->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(C,[World=TWeakObjectPtr<UWorld>(World),Index,Serial]()
 {
  const FFlashPool* Pool=Pools.Find(World);
  if(Pool&&Pool->Serial.IsValidIndex(Index)&&Pool->Serial[Index]==Serial&&Pool->Parts[Index].IsValid())Pool->Parts[Index]->SetVisibility(false);
 }),FMath::Max(.01f,Seconds),false);
}
