// Display only: bounded world-local billboards and timers. No battle subsystems, traces or random streams.
#include "BattleKitFX.h"
#include "BattleKitLook.h"
#include "FutureBattle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MaterialBillboardComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace BattleKitFX
{
namespace
{
 constexpr int32 KitFXLimit=96,KitFXPerFrame=12,KitFXWreckLimit=32;
 struct FKindSpec {const TCHAR* Name;float Size,Seconds,Rise,Distance;};
 const FKindSpec KindSpecs[]={
  {TEXT("Explosion"),600.f,1.4f,15.f,60000.f},
  {TEXT("SmokePuff"),300.f,7.f,65.f,15000.f},
  {TEXT("DustBurst"),60.f,.6f,12.f,15000.f},
  {TEXT("MuzzleSmoke"),30.f,.5f,25.f,4000.f}
 };
 constexpr int32 KindCount=int32(EKind::Count);
 static_assert(UE_ARRAY_COUNT(KindSpecs)==KindCount,"Update flipbook kinds");
 struct FKitMaterials
 {
  bool bLoaded=false;int32 Loaded=0;
  TStrongObjectPtr<UMaterialInterface> Bases[KindCount];
 };
 FKitMaterials& KitMaterials(){static FKitMaterials Value;return Value;}
 void LoadKitMaterials()
 {
  FKitMaterials& Cache=KitMaterials();if(Cache.bLoaded)return;Cache.bLoaded=true;
  int32 Count=0;
  for(int32 I=0;I<KindCount;++I)
  {
   const FString Path=FString::Printf(TEXT("/Game/Kit/Effects/MI_Kit_FX_%s.MI_Kit_FX_%s"),KindSpecs[I].Name,KindSpecs[I].Name);
   Cache.Bases[I].Reset(LoadObject<UMaterialInterface>(nullptr,*Path,nullptr,LOAD_NoWarn));
   if(Cache.Bases[I].IsValid())++Count;
  }
  Cache.Loaded=Count;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KITFX loaded=%d"),Count);
 }
 struct FKitSprite
 {
  TWeakObjectPtr<UMaterialBillboardComponent> Board;
  TWeakObjectPtr<UMaterialInstanceDynamic> Material;
  EKind Kind=EKind::Count;
  FVector At=FVector::ZeroVector,Drift=FVector::ZeroVector;
  double Start=0.,End=0.;bool bLive=false,bShown=false;
 };
 struct FKitWreck {FVector At;double End,Next;};
 struct FKitPool
 {
  TWeakObjectPtr<AActor> Host;
  TArray<FKitSprite> Sprites;
  TArray<FKitWreck> Wrecks;
  uint64 Frame=MAX_uint64,TickFrame=MAX_uint64,Counts[KindCount]={};
  int32 Emitted=0;
 };
 TMap<TWeakObjectPtr<UWorld>,TUniquePtr<FKitPool>> KitPools;
 bool CameraPosition(UWorld* World,FVector& Out)
 {
  const auto* PC=World->GetFirstPlayerController();
  if(!PC||!PC->PlayerCameraManager)return false;
  Out=PC->PlayerCameraManager->GetCameraLocation();return !Out.ContainsNaN();
 }
 void AdvanceKit(UWorld* World);
 FKitPool* ReadyKit(UWorld* World)
 {
  if(!Active(World))return nullptr;
  LoadKitMaterials();if(KitMaterials().Loaded==0)return nullptr;
  for(auto It=KitPools.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
  auto& Owned=KitPools.FindOrAdd(World);
  if(!Owned)Owned=MakeUnique<FKitPool>();
  FKitPool& Pool=*Owned;
  if(!Pool.Host.IsValid())
  {
   Pool=FKitPool();
   FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
   AActor* Host=World->SpawnActor<AActor>(Spawn);if(!Host)return nullptr;
   Pool.Host=Host;
   FTimerHandle Timer;
   FTimerManagerTimerParameters Parameters;Parameters.bLoop=true;Parameters.bMaxOncePerFrame=true;
   World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(Host,[WeakWorld=TWeakObjectPtr<UWorld>(World)]()
   {if(UWorld* LiveWorld=WeakWorld.Get())AdvanceKit(LiveWorld);}),1.f/60.f,Parameters);
  }
  return &Pool;
 }
 void AdvanceKit(UWorld* World)
 {
  if(!Active(World))return;
  auto* Owned=KitPools.Find(World);if(!Owned||!Owned->IsValid())return;
  FKitPool& Pool=**Owned;if(Pool.TickFrame==GFrameCounter)return;Pool.TickFrame=GFrameCounter;
  const double Now=World->GetTimeSeconds();FVector Camera;const bool bCamera=CameraPosition(World,Camera);
  for(FKitSprite& Sprite:Pool.Sprites)
  {
   if(!Sprite.bLive)continue;
   auto* Board=Sprite.Board.Get();auto* Material=Sprite.Material.Get();
   if(!Board||!Material){Sprite.bLive=false;continue;}
   if(Now>=Sprite.End){Board->SetVisibility(false);Sprite.bLive=false;continue;}
   if(Now<Sprite.Start){Board->SetVisibility(false);continue;}
   if(!Sprite.bShown)
   {
    if(Pool.Frame!=GFrameCounter){Pool.Frame=GFrameCounter;Pool.Emitted=0;}
    if(Pool.Emitted>=KitFXPerFrame)continue;
    ++Pool.Emitted;Sprite.bShown=true;
   }
   const double Age=Now-Sprite.Start,Alpha=FMath::Clamp(Age/(Sprite.End-Sprite.Start),0.,1.);
   const FVector Position=Sprite.At+Sprite.Drift*Age;
   Board->SetWorldLocation(Position);
   Material->SetScalarParameterValue(TEXT("Frame"),float(63.*Alpha));
   Material->SetScalarParameterValue(TEXT("Fade"),float(FMath::Min(1.,(1.-Alpha)*5.)));
   Board->SetVisibility(bCamera&&FVector::DistSquared(Camera,Position)<=FMath::Square(double(KindSpecs[int32(Sprite.Kind)].Distance)));
  }
  for(int32 I=Pool.Wrecks.Num()-1;I>=0;--I)
  {
   FKitWreck& W=Pool.Wrecks[I];
   if(Now>=W.End){Pool.Wrecks.RemoveAtSwap(I);continue;}
   if(Now<W.Next)continue;
   // No catch-up burst after a long frame: move along the 0.7-world-second schedule once.
   W.Next+=(FMath::FloorToDouble((Now-W.Next)/.7)+1.)*.7;
   Puff(World,EKind::SmokePuff,W.At,330.f,7.f,FVector(0,0,KindSpecs[int32(EKind::SmokePuff)].Rise),FLinearColor(.16f,.16f,.16f,1));
  }
 }
}
}

bool BattleKitFX::Active(UWorld* World)
{
 if(!BattleKitLook::Active(World))return false;
 static const bool Enabled=[]{int32 Option=1;FParse::Value(FCommandLine::Get(),TEXT("BattleKitFX="),Option);return Option!=0;}();
 return Enabled;
}

bool BattleKitFX::Puff(UWorld* World,EKind Kind,const FVector& At,float SizeCm,float Seconds)
{
 if(!Active(World)||int32(Kind)>=KindCount)return false;
 return Puff(World,Kind,At,SizeCm,Seconds,FVector(0,0,KindSpecs[int32(Kind)].Rise));
}

bool BattleKitFX::Puff(UWorld* World,EKind Kind,const FVector& At,float SizeCm,float Seconds,FVector Drift,FLinearColor Tint,float Delay)
{
 if(!Active(World))return false;
 const int32 KindIndex=int32(Kind);
 if(KindIndex<0||KindIndex>=KindCount||At.ContainsNaN()||Drift.ContainsNaN()||!FMath::IsFinite(SizeCm)||
  !FMath::IsFinite(Seconds)||!FMath::IsFinite(Delay)||Delay<0.f||Delay>8.f||
  !FMath::IsFinite(Tint.R)||!FMath::IsFinite(Tint.G)||!FMath::IsFinite(Tint.B)||!FMath::IsFinite(Tint.A))return false;
 const FKindSpec& Spec=KindSpecs[KindIndex];
 FVector Camera;if(!CameraPosition(World,Camera)||FVector::DistSquared(Camera,At)>FMath::Square(double(Spec.Distance)))return false;
 FKitPool* Pool=ReadyKit(World);if(!Pool||!KitMaterials().Bases[KindIndex].IsValid())return false;
 if(Pool->Frame!=GFrameCounter){Pool->Frame=GFrameCounter;Pool->Emitted=0;}
 if(Pool->Emitted>=KitFXPerFrame)return false;
 const double Now=World->GetTimeSeconds();int32 Chosen=INDEX_NONE;
 for(int32 I=0;I<Pool->Sprites.Num();++I)
  if(!Pool->Sprites[I].bLive||Pool->Sprites[I].End<=Now){Chosen=I;break;}
 if(Chosen==INDEX_NONE&&Pool->Sprites.Num()<KitFXLimit)Chosen=Pool->Sprites.AddDefaulted();
 if(Chosen==INDEX_NONE)
 {
  double Best=-1.;
  for(int32 I=0;I<Pool->Sprites.Num();++I)
  {
   const FKitSprite& Existing=Pool->Sprites[I];
   const double Age=FMath::Max(0.,Now-Existing.Start);
   const double Score=FVector::DistSquared(Camera,Existing.At+Existing.Drift*Age)+Age*1000000.;
   if(Score>Best){Best=Score;Chosen=I;}
  }
 }
 FKitSprite& Sprite=Pool->Sprites[Chosen];
 auto* Board=Sprite.Board.Get();
 if(!Board)
 {
  Board=NewObject<UMaterialBillboardComponent>(Pool->Host.Get(),NAME_None,RF_Transient);
  Board->SetCanEverAffectNavigation(false);Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Board->SetGenerateOverlapEvents(false);Board->SetCastShadow(false);Board->SetMobility(EComponentMobility::Movable);
  Board->SetVisibility(false);Pool->Host->AddInstanceComponent(Board);Board->RegisterComponent();Sprite.Board=Board;Sprite.Material.Reset();Sprite.Kind=EKind::Count;
 }
 Board->SetVisibility(false);
 if(!Sprite.Material.IsValid())
 {
  Sprite.Material=UMaterialInstanceDynamic::Create(KitMaterials().Bases[KindIndex].Get(),Board);
  if(!Sprite.Material.IsValid()){Sprite.bLive=false;return false;}
  Sprite.Material->SetFlags(RF_Transient);
 }
 // All four instances share M_Kit_FX_Flipbook: copy Sheet and its uniform parameters into the same MID.
 // A busy reused slot therefore keeps ONE dynamic material instead of allocating one on every change of kind.
 if(Sprite.Kind!=Kind)Sprite.Material->CopyMaterialUniformParameters(KitMaterials().Bases[KindIndex].Get());
 Sprite.Kind=Kind;Sprite.At=At;Sprite.Drift=Drift;
 Sprite.Start=Now+Delay;Sprite.End=Sprite.Start+FMath::Clamp(Seconds>0.f?Seconds:Spec.Seconds,.05f,10.f);Sprite.bLive=true;Sprite.bShown=Delay==0.f;
 auto* Material=Sprite.Material.Get();Material->SetScalarParameterValue(TEXT("Frame"),0.f);
 Material->SetScalarParameterValue(TEXT("Fade"),1.f);Material->SetVectorParameterValue(TEXT("Tint"),Tint);
 const float Width=FMath::Clamp(SizeCm>0.f?SizeCm:Spec.Size,1.f,3000.f);
 Board->Elements.Reset();Board->AddElement(Material,nullptr,false,Width*.5f,Width*.5f,nullptr);
 Board->SetWorldLocation(At);Board->SetVisibility(Delay==0.f);
 ++Pool->Emitted;++Pool->Counts[KindIndex];return true;
}

void BattleKitFX::Explosion(UWorld* World,const FVector& At,float Size)
{
 if(!Active(World)||!FMath::IsFinite(Size)||At.ContainsNaN())return;
 const float Scale=FMath::Clamp(Size,.5f,3.f);
 const auto& Fire=KindSpecs[int32(EKind::Explosion)];const auto& Smoke=KindSpecs[int32(EKind::SmokePuff)];
 Puff(World,EKind::Explosion,At,Fire.Size*Scale,Fire.Seconds,FVector(0,0,Fire.Rise));
 for(int32 I=0;I<3;++I)
  Puff(World,EKind::SmokePuff,At+FVector((I-1)*45.*Scale,0,50.*Scale),Smoke.Size*Scale,6.f+I,FVector(0,0,Smoke.Rise),FLinearColor(.45f,.45f,.45f,1),.25f+.2f*I);
 Puff(World,EKind::DustBurst,At+FVector(0,0,10),300.f*Scale,1.f,FVector(0,0,KindSpecs[int32(EKind::DustBurst)].Rise));
}

void BattleKitFX::Impact(UWorld* World,const FHitResult& Hit,bool bUnitHit)
{
 if(!Active(World)||bUnitHit||!Hit.bBlockingHit)return;
 const auto* Part=Hit.GetComponent();if(!Part||Part->Mobility==EComponentMobility::Movable)return;
 Puff(World,EKind::DustBurst,Hit.ImpactPoint+Hit.ImpactNormal*3.f,60.f,.6f,Hit.ImpactNormal*12.f);
}

void BattleKitFX::Muzzle(UWorld* World,const FVector& At,float Size)
{
 if(!Active(World)||!FMath::IsFinite(Size))return;
 FVector Camera;if(!CameraPosition(World,Camera)||FVector::DistSquared(Camera,At)>FMath::Square(4000.))return;
 const float Scale=FMath::Clamp(Size,1.f,6.f);
 const auto& Smoke=KindSpecs[int32(EKind::MuzzleSmoke)];
 Puff(World,EKind::MuzzleSmoke,At,Smoke.Size*Scale,Smoke.Seconds,FVector(0,0,Smoke.Rise));
 if(Size>=2.5f)Puff(World,EKind::DustBurst,At,80.f*Scale,.7f,FVector(0,0,12));
}

void BattleKitFX::Wreck(UWorld* World,const FVector& At)
{
 if(!Active(World)||At.ContainsNaN())return;
 FKitPool* Pool=ReadyKit(World);if(!Pool)return;
 const double Now=World->GetTimeSeconds();
 for(const FKitWreck& Existing:Pool->Wrecks)if(Existing.End>Now&&FVector::DistSquared(Existing.At,At)<1.)return;
 if(Pool->Wrecks.Num()>=KitFXWreckLimit)
 {
  int32 Oldest=0;for(int32 I=1;I<Pool->Wrecks.Num();++I)if(Pool->Wrecks[I].End<Pool->Wrecks[Oldest].End)Oldest=I;
  Pool->Wrecks.RemoveAtSwap(Oldest);
 }
 Pool->Wrecks.Add({At,Now+45.,Now});
}

uint64 BattleKitFX::Started(UWorld* World,EKind Kind)
{
 if(!Active(World)||int32(Kind)>=KindCount)return 0;
 const auto* Pool=KitPools.Find(World);return Pool&&Pool->IsValid()?(*Pool)->Counts[int32(Kind)]:0;
}
