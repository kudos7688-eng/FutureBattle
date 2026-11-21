// Battle effects (2026-11-22). Display only: no collision, no navigation, no battle state; the only query is a short
// downward trace that sets an explosion on the ground.
// Explosion: Lyra Starter Game NS_Grenade_Explosion (Niagara). Bullet impact: TPS Multiplayer Shooter
// P_Wep_BulletImpact_Rifle (Cascade). Both copied with their materials and textures by Setup/Import-AudioFXAssets.py;
// sources and terms in Docs/ASSET_SOURCES_AUDIO_FX.md.
#include "BattleEffects.h"
#include "FutureBattle.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

// Helpers live in an unnamed namespace inside BattleEffects so that, in a unity build, they never meet the unnamed-namespace
// helpers of other files (lookup from the BattleEffects functions finds these first).
namespace BattleEffects
{
namespace
{
 struct FFxState
 {
  bool bLoaded=false;
  UNiagaraSystem* Blast=nullptr;
  UParticleSystem* Impact=nullptr;
  uint64 Frame=MAX_uint64;
  int32 Impacts=0, Blasts=0;
  bool bCamera=false;
  FVector Camera=FVector::ZeroVector, Forward=FVector::ForwardVector;
 };
 FFxState& Get(){static FFxState S;return S;}

 bool SwitchedOff()
 {
  static const bool bOff=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleFX="),V);return V==0;}();
  return bOff;
 }

 bool Renders(UWorld* World)
 {
  return !SwitchedOff()&&World&&World->IsGameWorld()&&FApp::CanEverRender();
 }

 void Load(FFxState& S)
 {
  if(S.bLoaded)return;
  S.bLoaded=true;
  S.Blast=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Effects/Particles/Explosion/NS_Grenade_Explosion.NS_Grenade_Explosion"));
  S.Impact=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/Multiplayercovershooter/ParticleSystem/Particles/Effects/Weapons/P_Wep_BulletImpact_Rifle.P_Wep_BulletImpact_Rifle"));
  if(S.Blast)S.Blast->AddToRoot();
  if(S.Impact)S.Impact->AddToRoot();
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FX loaded explosion=%d impact=%d"),S.Blast?1:0,S.Impact?1:0);
 }

 FFxState* Ready(UWorld* World)
 {
  if(!Renders(World))return nullptr;
  FFxState& S=Get();
  Load(S);
  if(S.Frame!=GFrameCounter)
  {
   S.Frame=GFrameCounter;S.Impacts=S.Blasts=0;S.bCamera=false;
   if(const APlayerController* PC=World->GetFirstPlayerController();PC&&PC->PlayerCameraManager)
   {
    S.Camera=PC->PlayerCameraManager->GetCameraLocation();S.Forward=PC->PlayerCameraManager->GetCameraRotation().Vector();S.bCamera=true;
   }
  }
  return S.bCamera?&S:nullptr;
 }

 // Near enough and in front of the camera (a wide cone, everything within Around): nothing is spawned behind the viewer.
 bool InView(const FFxState& S,const FVector& At,double MaxDistance,double Around)
 {
  const FVector To=At-S.Camera;
  const double D=To.Size();
  if(D>MaxDistance)return false;
  return D<Around||FVector::DotProduct(To/D,S.Forward)>.25;
 }
}
}

bool BattleEffects::Active(UWorld* World)
{
 if(!Renders(World))return false;
 Load(Get());
 return Get().Blast!=nullptr;
}

void BattleEffects::Preload(UWorld* World)
{
 if(Renders(World))Load(Get());
}

bool BattleEffects::BulletImpact(UWorld* World,const FVector& At,const FVector& Normal)
{
 FFxState* S=Ready(World);
 if(!S||!S->Impact||At.ContainsNaN())return false;
 // A puff of a few centimetres: only near the camera, and a few per frame when a whole line fires at once.
 if(!InView(*S,At,7000.,600.)||S->Impacts>=4)return false;
 ++S->Impacts;
 const FVector N=Normal.IsNearlyZero()?FVector::UpVector:Normal.GetSafeNormal();
 return UGameplayStatics::SpawnEmitterAtLocation(World,S->Impact,At+N*2.,N.Rotation(),FVector(1.),true,EPSCPoolMethod::AutoRelease,true)!=nullptr;
}

bool BattleEffects::Explosion(UWorld* World,const FVector& At,float Size)
{
 FFxState* S=Ready(World);
 if(!S||!S->Blast||At.ContainsNaN())return false;
 if(!InView(*S,At,150000.,5000.)||S->Blasts>=4)return false;
 ++S->Blasts;
 // Sit on the ground when the burst is just above it (shells aim at soldiers' centres); a hit high on a wall stays there.
 FVector Where=At;
 FHitResult Ground;
 if(World->LineTraceSingleByObjectType(Ground,At+FVector(0,0,50),At-FVector(0,0,300),FCollisionObjectQueryParams(ECC_WorldStatic)))Where=Ground.ImpactPoint;
 return UNiagaraFunctionLibrary::SpawnSystemAtLocation(World,S->Blast,Where,FRotator::ZeroRotator,FVector(FMath::Clamp(Size,.5f,3.f)),true,true,ENCPoolMethod::AutoRelease,true)!=nullptr;
}
