#pragma once
#include "CoreMinimal.h"
class UWorld;
struct FHitResult;
namespace BattleKitFX
{
 enum class EKind : uint8 {Explosion,SmokePuff,DustBurst,MuzzleSmoke,Count};
 bool Active(UWorld* World);
 // Size is the full billboard width in cm; drift is cm per WORLD second. Zero size/lifetime uses the kind table.
 bool Puff(UWorld* World,EKind Kind,const FVector& At,float SizeCm=0.f,float Seconds=0.f);
 bool Puff(UWorld* World,EKind Kind,const FVector& At,float SizeCm,float Seconds,
  FVector Drift,FLinearColor Tint=FLinearColor::White,float Delay=0.f);
 void Explosion(UWorld* World,const FVector& At,float Size);
 void Impact(UWorld* World,const FHitResult& Hit,bool bUnitHit);
 void Muzzle(UWorld* World,const FVector& At,float Size);
 void Wreck(UWorld* World,const FVector& At);
 uint64 Started(UWorld* World,EKind Kind);
}
