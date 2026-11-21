// Corpse removal (display only). -BattleCorpseLifetime=S (default 20 since 2026-11-20, user decision; 0 keeps bodies): S seconds after death a body sinks 1.2 m into the ground
// over 1.5 s and is then hidden. The unit record (UBattleState), its position and its collision are left alone: score,
// save, replacement and AI read the record, never the body. Yet the match is not identical (map E seed 7: 566/618 -> 532/614,
// same winner; an unknown display->match coupling, like -BattleVATFarLight). The user accepted that.
#include "BattleLeaderRuntime.h"
#include "BattleBodyAnim.h"
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

// Stage 2 diagnosis (ROADMAP 2026-11-20 stage 2: find the display->match coupling). -BattleCorpseSteps=<mask> (default 63 = all, as before)
// leaves out single display operations of the corpse removal, to see which one changes the match: 1 sink the body, 2 hide a leader's
// actor, 4 stop a leader's mesh tick, 8 take a follower out of the VAT presenter, 16 hide a follower's mesh and stop its tick, 32 hide
// a follower's weapon. Bookkeeping (lists, log) is unchanged.
static int32 CorpseSteps()
{
 static const int32 Mask=[]{int32 V=63;FParse::Value(FCommandLine::Get(),TEXT("BattleCorpseSteps="),V);return V&63;}();
 return Mask;
}

float UBattleLeaderRuntime::CorpseLifetime()
{
 static const float Seconds=[]{float V=20.f;FParse::Value(FCommandLine::Get(),TEXT("BattleCorpseLifetime="),V);return FMath::Max(0.f,V);}();
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
  if(Body && !Far && (CorpseSteps()&1))Body->SetRelativeLocation(C.BaseOffset-FVector(0,0,CorpseSinkDepth*Alpha));
  if(Alpha>=1.){HideCorpse(ID);Done.Add(ID);}
 }
 for(int32 ID:Done)Corpses.Remove(ID);
}

void UBattleLeaderRuntime::HideCorpse(int32 ID)
{
 auto* B=Bodies.Find(ID);if(!B)return;
 HiddenCorpses.Add(ID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_CORPSE_HIDDEN id=%d actor=%d hidden=%d"),ID,B->Actor.IsValid()?1:0,HiddenCorpses.Num());
 if(B->Actor.IsValid()){B->Actor->HideCorpse((CorpseSteps()&2)!=0,(CorpseSteps()&4)!=0);return;}
 if(VAT&&(CorpseSteps()&8))VAT->RemoveUnit(ID);
 if(B->Mesh.IsValid()&&(CorpseSteps()&16)){B->Mesh->SetVisibility(false,true);B->Mesh->SetComponentTickEnabled(false);}
 if(B->Weapon.IsValid()&&(CorpseSteps()&32))B->Weapon->SetVisibility(false);
}

UAnimSequence* UBattleLeaderRuntime::DeathClip(int32 ID) const
{
 return BattleBodyAnim::Death(Forward(ID),LastShotDirs.FindRef(ID),ID);
}
