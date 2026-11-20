// Corpse removal (display only). Trial -BattleCorpseLifetime=S: S seconds after death a body sinks 1.2 m into the ground
// over 1.5 s and is then hidden. The unit record (UBattleState), its position and its collision are left alone: score,
// save, replacement and AI read the record, never the body, so the match is unchanged.
#include "BattleLeaderRuntime.h"
#include "FutureBattle.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattleVATDisplay.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
 constexpr double CorpseSinkSeconds=1.5;
 constexpr double CorpseSinkDepth=120.;
}

float UBattleLeaderRuntime::CorpseLifetime()
{
 static const float Seconds=[]{float V=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleCorpseLifetime="),V);return FMath::Max(0.f,V);}();
 return Seconds;
}

void UBattleLeaderRuntime::AddCorpse(int32 ID)
{
 if(CorpseLifetime()<=0.f || !Bodies.Contains(ID) || HiddenCorpses.Contains(ID))return;
 FCorpse C;C.SinkAt=GetWorld()->GetTimeSeconds()+CorpseLifetime();
 Corpses.Add(ID,C);
}

void UBattleLeaderRuntime::StepCorpses()
{
 if(Corpses.IsEmpty())return;
 const double Now=GetWorld()->GetTimeSeconds();
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 TArray<int32> Done;
 for(auto& Pair:Corpses)
 {
  const int32 ID=Pair.Key;auto& C=Pair.Value;
  const auto* U=Store->Find(ID);auto* B=Bodies.Find(ID);
  if(!U || U->IsAlive() || !B){Done.Add(ID);continue;}
  if(Now<C.SinkAt)continue;
  USceneComponent* Body=B->Actor.IsValid()?static_cast<USceneComponent*>(B->Actor->GetMesh()):static_cast<USceneComponent*>(B->Mesh.Get());
  // A far follower is drawn by the VAT presenter, which does not follow the mesh: hide it at once (it is far away).
  const bool Far=!B->Actor.IsValid() && VAT && VAT->IsFar(ID);
  if(!C.bSinking)
  {
   C.bSinking=true;
   if(Body)C.BaseOffset=Body->GetRelativeLocation();
  }
  const double Alpha=Far?1.:FMath::Clamp((Now-C.SinkAt)/CorpseSinkSeconds,0.,1.);
  if(Body && !Far)Body->SetRelativeLocation(C.BaseOffset-FVector(0,0,CorpseSinkDepth*Alpha));
  if(Alpha>=1.){HideCorpse(ID);Done.Add(ID);}
 }
 for(int32 ID:Done)Corpses.Remove(ID);
}

void UBattleLeaderRuntime::HideCorpse(int32 ID)
{
 auto* B=Bodies.Find(ID);if(!B)return;
 HiddenCorpses.Add(ID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CORPSE_HIDDEN id=%d actor=%d hidden=%d"),ID,B->Actor.IsValid()?1:0,HiddenCorpses.Num());
 if(B->Actor.IsValid()){B->Actor->HideCorpse();return;}
 if(VAT)VAT->RemoveUnit(ID);
 if(B->Mesh.IsValid()){B->Mesh->SetVisibility(false,true);B->Mesh->SetComponentTickEnabled(false);}
 if(B->Weapon.IsValid())B->Weapon->SetVisibility(false);
}
