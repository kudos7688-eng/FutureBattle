// Individual AI stage A (2026-11-20, trial -BattleThreatCover=1, default 0): each soldier's own threats, cover that hides
// him from all of them, and leaving a cover place that a threat now sees into. Part of UBattleLeaderRuntime.
#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattlePerception.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::ThreatCoverEnabled(int32 Team)
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCover="),V);return V!=0;}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCoverTeam="),V);return V==0||V==1?V:-1;}();
 return bOn && (Only<0 || Only==Team);
}

int32 UBattleLeaderRuntime::GatherThreats(int32 ID,FVector Primary,double Now,TArray<FVector,TInlineAllocator<3>>& Out) const
{
 Out.Reset();Out.Add(Primary);
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return 1;
 const FVector Pos=Position(ID);const float Reach=BattleRanges::Reach(U->Weapon);
 // Observed information only: this soldier's own sightings (not reports from others) of the last 4 s, and the place he was
 // last hit from (5 s). Threats within 5 m of one already listed count once; the nearest two besides the target are kept.
 TArray<TPair<double,FVector>,TInlineAllocator<8>> Near;
 if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
  for(const auto& C:*Contacts)
  {
   const auto& K=C.Value;if(K.bShared || Now-K.LastSeenTime>4.)continue;
   const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
   const double D=FVector::DistSquared2D(Pos,K.LastKnownPosition);if(D>FMath::Square(double(Reach)))continue;
   Near.Add({D,K.LastKnownPosition});
  }
 if(FVector From;UnitShotFrom(ID,Now,From))Near.Add({FVector::DistSquared2D(Pos,From),From});
 Near.Sort([](const TPair<double,FVector>& A,const TPair<double,FVector>& B){return A.Key<B.Key;});
 for(const auto& N:Near)
 {
  if(Out.Num()>=3)break;
  bool Same=false;for(const FVector& O:Out)if(FVector::DistSquared2D(O,N.Value)<FMath::Square(500.)){Same=true;break;}
  if(!Same)Out.Add(N.Value);
 }
 return Out.Num();
}

bool UBattleLeaderRuntime::ExposedToThreats(int32 ID,FVector Anchor,const TArray<FVector,TInlineAllocator<3>>& Threats) const
{
 // Index 0 is the target, already checked by the caller; a place is flanked when a threat sees more than the limit.
 for(int32 I=1;I<Threats.Num();++I)
  if(BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Threats[I]+FVector(0,0,60)).Fraction()>CoverExposureLimit())return true;
 return false;
}

bool UBattleLeaderRuntime::CheckCoverThreats(int32 ID,FVector Primary,double Now,float Dt)
{
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);
 if(!U || U->CoverPhase!=2 || U->CoverSlot==FreeCoverSlot || !ThreatCoverEnabled(U->Team))return false;
 // Once a second per soldier, at a phase fixed by the unit ID (no state, so a saved and loaded match checks at the same times).
 const double Phase=double(ID%16)/16.;
 if(FMath::FloorToDouble(Now+Phase)==FMath::FloorToDouble(Now-Dt+Phase))return false;
 auto& Diag=SightExperiment;Diag.Cover(TEXT("threat_cover_check"));
 TArray<FVector,TInlineAllocator<3>> Threats;GatherThreats(ID,Primary,Now,Threats);
 const FVector Anchor=U->CoverAnchor;
 // The target too: it may have moved round the place since the soldier chose it.
 const bool ByTarget=BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Primary+FVector(0,0,60)).Fraction()>CoverExposureLimit();
 const bool ByOther=!ByTarget && ExposedToThreats(ID,Anchor,Threats);
 if(!ByTarget && !ByOther)return false;
 Diag.Cover(ByTarget?TEXT("threat_cover_broken_by_target"):TEXT("threat_cover_broken_by_other"));
 UE_LOG(LogFutureBattle,Verbose,TEXT("BATTLE_THREAT_COVER_BROKEN unit=%d slot=%d threats=%d by=%s"),ID,U->CoverSlot,Threats.Num(),ByTarget?TEXT("target"):TEXT("other"));
 if(U->CoverSlot!=INDEX_NONE)BrokenCover.Add(ID,TPair<int32,double>(U->CoverSlot,Now+10.));
 CancelCoverAction(ID);CoverProbeAfter.Remove(ID);
 return true;
}
