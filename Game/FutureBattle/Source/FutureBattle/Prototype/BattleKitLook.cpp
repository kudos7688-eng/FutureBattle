// Material kit trial (default off). No gameplay queries, random numbers or battle-state writes.
#include "BattleKitLook.h"
#include "BattleKitProps.h"
#include "BattleBodyAnim.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RHI.h"
#include "UObject/StrongObjectPtr.h"

namespace BattleKitLook
{
namespace
{
 struct FOptions
 {
  int32 Look=0, Surfaces=1, Sky=1, Weapons=1, Props=1;
  FVector Offset=FVector::ZeroVector;
  FRotator Rotation=FRotator::ZeroRotator;
  FOptions()
  {
   FParse::Value(FCommandLine::Get(),TEXT("BattleKitLook="),Look);
   FParse::Value(FCommandLine::Get(),TEXT("BattleKitSurfaces="),Surfaces);
   FParse::Value(FCommandLine::Get(),TEXT("BattleKitSky="),Sky);
   FParse::Value(FCommandLine::Get(),TEXT("BattleKitWeapons="),Weapons);
   FParse::Value(FCommandLine::Get(),TEXT("BattleKitProps="),Props);
   FString Text;
   if(FParse::Value(FCommandLine::Get(),TEXT("BattleKitGrip="),Text,false))
   {
    TArray<FString> Parts;Text.ParseIntoArray(Parts,TEXT(","),false);
    double Values[6]={};bool bValid=Parts.Num()==6;
    for(int32 I=0;bValid&&I<6;++I){bValid=Parts[I].IsNumeric();Values[I]=FCString::Atod(*Parts[I]);}
    if(bValid){Offset=FVector(Values[0],Values[1],Values[2]);Rotation=FRotator(Values[3],Values[4],Values[5]);}
    else UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_KITLOOK invalid BattleKitGrip; expected x,y,z,pitch,yaw,roll (finite numbers)"));
   }
  }
 };
 const FOptions& Options(){static const FOptions Value;return Value;}

 struct FSurface {const TCHAR* Original;const TCHAR* Kit;};
 const FSurface Surfaces[]={
  {TEXT("M_ArenaFloor"),TEXT("Asphalt")},{TEXT("M_ArenaCover"),TEXT("PlasterWhite")},
  {TEXT("M_MapBrick"),TEXT("BrickRed")},{TEXT("M_MapBridge"),TEXT("StoneBlock")},
  {TEXT("M_MapChurch"),TEXT("StoneBlock")},{TEXT("M_MapField"),TEXT("FarmSoil")},
  {TEXT("M_MapFlats"),TEXT("PlasterGrey")},{TEXT("M_MapGrass"),TEXT("Grass")},
  {TEXT("M_MapHill"),TEXT("GrassSparse")},{TEXT("M_MapHouse"),TEXT("PlasterPainted")},
  {TEXT("M_MapLane"),TEXT("DirtPath")},{TEXT("M_MapPave"),TEXT("Pavement")},
  {TEXT("M_MapRoad"),TEXT("Asphalt")},{TEXT("M_MapRock"),TEXT("RockFace")},
  {TEXT("M_MapRockTop"),TEXT("RockGround")},{TEXT("M_MapRuin"),TEXT("ConcreteDirty")},
  {TEXT("M_MapRust"),TEXT("MetalRust")},{TEXT("M_MapTower"),TEXT("PlasterPainted")},
  {TEXT("M_MapTrunk"),TEXT("Bark")},{TEXT("M_FordGravel"),TEXT("Gravel")},
  {TEXT("M_SectorField"),TEXT("FarmSoil")},{TEXT("M_SectorRoad"),TEXT("Asphalt")},
  {TEXT("M_SectorWood"),TEXT("ForestFloor")}
 };
 // EBattleWeapon order. Corrections are in the hand/camera parent space: cm and pitch/yaw/roll degrees.
 struct FWeaponSpec {const TCHAR* Name;FVector Offset;FRotator Rotation;};
 const FWeaponSpec WeaponSpecs[]={
  {TEXT("Carbine"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("SupportGun"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("AntiTank"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("Mortar"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("Pistol"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("SMG"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("Shotgun"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("Marksman"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("GrenadeLauncher"),FVector::ZeroVector,FRotator::ZeroRotator},
  {TEXT("Sniper"),FVector::ZeroVector,FRotator::ZeroRotator}
 };
 constexpr int32 WeaponCount=UE_ARRAY_COUNT(WeaponSpecs);
 static_assert(WeaponCount==int32(EBattleWeapon::Sniper)+1,"Update the kit weapon table");
 struct FAssets
 {
  bool bLoaded=false;
  TStrongObjectPtr<UStaticMesh> Meshes[WeaponCount];
  int32 Loaded=0, Missing=0;
 };
 FAssets& Assets(){static FAssets Value;return Value;}
 void LoadWeapons()
 {
  FAssets& Cache=Assets();if(Cache.bLoaded)return;Cache.bLoaded=true;
  for(int32 I=0;I<WeaponCount;++I)
  {
   const FString Path=FString::Printf(TEXT("/Game/Kit/Weapons/SM_Kit_%s.SM_Kit_%s"),WeaponSpecs[I].Name,WeaponSpecs[I].Name);
   Cache.Meshes[I].Reset(LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn));
   if(Cache.Meshes[I].IsValid())++Cache.Loaded;else ++Cache.Missing;
  }
 }

 bool AddSky(UWorld* World,int32& Missing)
 {
  if(TActorIterator<ASkyAtmosphere> Existing(World);Existing)return true;
  FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
  if(!World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn))return false;
  if(!TActorIterator<AExponentialHeightFog>(World))
   if(auto* Fog=World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn))
    Fog->GetComponent()->SetFogDensity(.001f);
  // Optional stock engine cloud material, no external content or plugin dependency.
  if(!TActorIterator<AVolumetricCloud>(World))
  {
   auto* CloudMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"),nullptr,LOAD_NoWarn);
   if(CloudMaterial)
   {
    if(auto* Cloud=World->SpawnActor<AVolumetricCloud>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn))
     if(auto* Component=Cloud->FindComponentByClass<UVolumetricCloudComponent>())Component->SetMaterial(CloudMaterial);
   }
   else ++Missing;
  }
  for(TActorIterator<ADirectionalLight> It(World);It;++It)
   if(auto* Light=Cast<UDirectionalLightComponent>(It->GetLightComponent()))Light->SetAtmosphereSunLight(true);
  bool bHasSkyLight=false;
  for(TActorIterator<ASkyLight> It(World);It;++It)
  {It->GetLightComponent()->SetRealTimeCaptureEnabled(true);bHasSkyLight=true;}
  if(!bHasSkyLight)
   if(auto* Light=World->SpawnActor<ASkyLight>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn))
   {
    Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Light->GetLightComponent()->SetRealTimeCaptureEnabled(true);
   }
  return true;
 }
}
}

// Look check (display only): -BattleKitShot=<file prefix> saves <prefix>-N.png for each camera given with
// -BattleKitShotCams="x,y,z,pitch,yaw;x,y,z,pitch,yaw;..." (cm, degrees), from 8 seconds after the start, 2 seconds apart,
// then quits. Without cameras: one picture of the player's own view. The view target is the only thing it changes.
namespace BattleKitLook
{
namespace
{
 void StartShots(UWorld* World)
 {
  FString Prefix;if(!FParse::Value(FCommandLine::Get(),TEXT("BattleKitShot="),Prefix)||Prefix.IsEmpty())return;
  FString CamText;FParse::Value(FCommandLine::Get(),TEXT("BattleKitShotCams="),CamText,false);
  TArray<FString> CamList;CamText.ParseIntoArray(CamList,TEXT(";"),true);
  TArray<FTransform> Cams;
  for(const FString& One:CamList)
  {
   TArray<FString> V;One.ParseIntoArray(V,TEXT(","),false);if(V.Num()!=5)continue;
   Cams.Add(FTransform(FRotator(FCString::Atod(*V[3]),FCString::Atod(*V[4]),0.),FVector(FCString::Atod(*V[0]),FCString::Atod(*V[1]),FCString::Atod(*V[2]))));
  }
  const int32 Count=FMath::Max(1,Cams.Num());
  TWeakObjectPtr<UWorld> WeakWorld(World);
  for(int32 Index=0;Index<=Count;++Index)
  {
   FTimerHandle Handle;
   World->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([WeakWorld,Cams,Prefix,Index,Count]()
   {
    UWorld* W=WeakWorld.Get();if(!W)return;
    if(APlayerController* Viewer=W->GetFirstPlayerController())if(AHUD* Hud=Viewer->GetHUD())Hud->bShowHUD=false;
    if(Index>0)FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s-%d.png"),*Prefix,Index-1),false,false);
    if(Index==Count){FTimerHandle Quit;W->GetTimerManager().SetTimer(Quit,FTimerDelegate::CreateLambda([](){FPlatformMisc::RequestExit(false);}),1.f,false);return;}
    if(Cams.IsValidIndex(Index))
     if(APlayerController* PC=W->GetFirstPlayerController())
     {
      FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
      if(ACameraActor* Cam=W->SpawnActor<ACameraActor>(Cams[Index].GetLocation(),Cams[Index].Rotator(),Spawn))
      {
       if(UCameraComponent* Lens=Cam->GetCameraComponent()){Lens->bConstrainAspectRatio=false;Lens->SetFieldOfView(70.f);}
       PC->SetViewTarget(Cam);
      }
     }
   }),8.f+2.f*Index,false);
  }
 }
}
}

bool BattleKitLook::Active(UWorld* World)
{
 // Test renderability before even parsing the kit options (including on a CDO / dedicated server).
 return World&&World->IsGameWorld()&&FApp::CanEverRender()&&!GUsingNullRHI&&Options().Look==1;
}

UStaticMesh* BattleKitLook::Weapon(UWorld* World,EBattleWeapon Kind)
{
 if(!Active(World)||Options().Weapons==0)return nullptr;
 const int32 Index=int32(Kind);if(Index<0||Index>=WeaponCount)return nullptr;
 LoadWeapons();return Assets().Meshes[Index].Get();
}

bool BattleKitLook::IsWeapon(UWorld* World,const UStaticMesh* Mesh)
{
 if(!Mesh||!Active(World)||Options().Weapons==0)return false;
 for(const auto& Entry:Assets().Meshes)if(Entry.Get()==Mesh)return true;
 return false;
}

FTransform BattleKitLook::Grip(EBattleWeapon Kind,bool bFirstPerson)
{
 // RifleGrip is the old mesh ORIGIN in hand_r space, not the hand's grip point. Recover the hand origin
 // in the old carbine's coordinates using its .62 scale, then move the kit's grip origin to that point.
 // RifleGrip reference Hand/Gun gives local grip ~= (-3.1029,35.3268,-5.7994) cm; FPS ~= (68.9026,13.0762,-19.5956).
 FTransform OldHand=BattleBodyAnim::RifleGrip();OldHand.SetScale3D(FVector(.62));
 const FVector OldLocalGrip=OldHand.InverseTransformPosition(FVector::ZeroVector);
 FTransform Base=bFirstPerson?FTransform(FRotator(0,90,0),FVector(47,15,-16),FVector(.62)):OldHand;
 const FVector At=Base.TransformPosition(OldLocalGrip);
 const int32 Index=int32(Kind);
 const FVector PerWeapon=Index>=0&&Index<WeaponCount?WeaponSpecs[Index].Offset:FVector::ZeroVector;
 const FRotator PerRotation=Index>=0&&Index<WeaponCount?WeaponSpecs[Index].Rotation:FRotator::ZeroRotator;
 return FTransform(((PerRotation+Options().Rotation).Quaternion()*Base.GetRotation()).GetNormalized(),At+PerWeapon+Options().Offset,FVector(1.));
}

bool BattleKitLook::AnimateFirstPerson(UStaticMeshComponent* Model,double Kick,double Dip)
{
 if(!Model||!Active(Model->GetWorld())||Options().Weapons==0)return false;
 for(int32 I=0;I<WeaponCount;++I)
  if(Assets().Meshes[I].IsValid()&&Assets().Meshes[I].Get()==Model->GetStaticMesh())
  {
   FTransform Pose=Grip(static_cast<EBattleWeapon>(I),true);
   Pose.AddToTranslation(FVector(-4.*Kick,0.,-6.*Dip));
   FRotator Turn=Pose.Rotator();Turn.Roll+=4.*Kick-25.*Dip;Pose.SetRotation(Turn.Quaternion());
   Model->SetRelativeTransform(Pose);return true;
  }
 return false;
}

void BattleKitLook::Apply(UWorld* World)
{
 // The look check also works with the switch off (the same pictures before and after).
 if(!Active(World))
 {
  static bool bShotsOff=false;
  if(!bShotsOff&&World&&World->IsGameWorld()&&FApp::CanEverRender()&&!GUsingNullRHI){bShotsOff=true;StartShots(World);}
  return;
 }
 static TSet<TWeakObjectPtr<UWorld>> Applied;
 for(auto It=Applied.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
 if(Applied.Contains(World))return;Applied.Add(World);
 StartShots(World);
 // Dressing classifies the ORIGINAL material names; surface replacement must follow it.
 const FPropsResult Dressing=Options().Props!=0?Dress(World):FPropsResult{};
 int32 Changed=0,Missing=Dressing.Missing;
 if(Options().Surfaces!=0)
 {
  // Cache failures as well as successes: missing counts distinct asset paths, not every affected cube.
  TMap<FString,TStrongObjectPtr<UMaterialInterface>> Materials;
  for(TActorIterator<AStaticMeshActor> It(World);It;++It)
  {
   // Never touch runtime-created markers, bodies, or actors. RF_WasLoaded survives level loading.
   if(!It->HasAnyFlags(RF_WasLoaded)||It->HasAnyFlags(RF_Transient))continue;
   TInlineComponentArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
   for(UStaticMeshComponent* Component:Components)
   {
    if(!Component->HasAnyFlags(RF_WasLoaded)||Component->HasAnyFlags(RF_Transient))continue;
    const UMaterialInterface* Original=Component->GetMaterial(0);if(!Original)continue;
    for(const FSurface& Entry:Surfaces)
    {
     if(Original->GetFName()!=FName(Entry.Original))continue;
     const FString Key(Entry.Kit);
     auto* Cached=Materials.Find(Key);
     if(!Cached)
     {
      const FString Path=FString::Printf(TEXT("/Game/Kit/Surfaces/MI_Kit_%s.MI_Kit_%s"),Entry.Kit,Entry.Kit);
      auto* Material=LoadObject<UMaterialInterface>(nullptr,*Path,nullptr,LOAD_NoWarn);
      Cached=&Materials.Add(Key,TStrongObjectPtr<UMaterialInterface>(Material));if(!Material)++Missing;
     }
     if(Cached->IsValid()){Component->SetMaterial(0,Cached->Get());++Changed;}
     break;
    }
   }
  }
 }
 const bool bSky=Options().Sky!=0&&AddSky(World,Missing);
 int32 Loaded=0;
 if(Options().Weapons!=0){LoadWeapons();Loaded=Assets().Loaded;Missing+=Assets().Missing;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KITLOOK surfaces=%d missing=%d sky=%d weapons=%d props=%d instances=%d"),Changed,Missing,bSky?1:0,Loaded,Dressing.Cubes,Dressing.Instances);
}
