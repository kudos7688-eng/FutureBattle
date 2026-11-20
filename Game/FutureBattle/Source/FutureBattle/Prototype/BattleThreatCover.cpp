// Individual AI stage A (2026-11-20, trial -BattleThreatCover=1, default 0): each soldier's own threats, cover that hides
// him from all of them, and leaving a cover place that a threat now sees into. Part of UBattleLeaderRuntime.
#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattlePerception.h"
#include "BattleSectors.h"
#include "BattleSquadTactics.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::ThreatCoverEnabled(int32 Team)
{
 return ThreatCoverMode(Team)>0;
}

int32 UBattleLeaderRuntime::ThreatCoverMode(int32 Team)
{
 // 1 = the first version (every enemy he saw within reach; leave at the first break). 2 = narrowed (2026-11-20 after map I):
 // only enemies within engagement range that he sees facing him, and where he was hit from; leave after two breaks in a row.
 static const int32 Mode=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCover="),V);return FMath::Clamp(V,0,2);}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCoverTeam="),V);return V==0||V==1?V:-1;}();
 return Only<0 || Only==Team ? Mode : 0;
}

int32 UBattleLeaderRuntime::ThreatCoverRelocate()
{
 static const int32 Mode=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCoverRelocate="),V);return FMath::Clamp(V,0,2);}();
 return Mode;
}

int32 UBattleLeaderRuntime::GatherThreats(int32 ID,FVector Primary,double Now,TArray<FVector,TInlineAllocator<3>>& Out) const
{
 Out.Reset();Out.Add(Primary);
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return 1;
 const FVector Pos=Position(ID);const bool bNarrow=ThreatCoverMode(U->Team)==2;
 const float Reach=bNarrow?BattleRanges::Engage(U->Weapon):BattleRanges::Reach(U->Weapon);
 // Observed information only: this soldier's own sightings (not reports from others) of the last 4 s, and the place he was
 // last hit from (5 s). Threats within 5 m of one already listed count once; the nearest two besides the target are kept.
 TArray<TPair<double,FVector>,TInlineAllocator<8>> Near;
 if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
  for(const auto& C:*Contacts)
  {
   const auto& K=C.Value;if(K.bShared || Now-K.LastSeenTime>4.)continue;
   const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
   const double D=FVector::DistSquared2D(Pos,K.LastKnownPosition);if(D>FMath::Square(double(Reach)))continue;
   // Narrowed: an enemy he sees now and that faces him (within 30 degrees); its facing is what he sees of it.
   if(bNarrow && !(K.bVisible && FVector::DotProduct(Forward(C.Key).GetSafeNormal2D(),(Pos-K.LastKnownPosition).GetSafeNormal2D())>.866f))continue;
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
 if(!U || U->CoverPhase!=2 || U->CoverSlot==FreeCoverSlot || !ThreatCoverEnabled(U->Team) || !SoldierAIFocus(ID))return false;
 // Once a second per soldier, at a phase fixed by the unit ID (no state, so a saved and loaded match checks at the same times).
 const double Phase=double(ID%16)/16.;
 if(FMath::FloorToDouble(Now+Phase)==FMath::FloorToDouble(Now-Dt+Phase))return false;
 auto& Diag=SightExperiment;Diag.Cover(TEXT("threat_cover_check"));
 TArray<FVector,TInlineAllocator<3>> Threats;GatherThreats(ID,Primary,Now,Threats);
 const FVector Anchor=U->CoverAnchor;
 // The target too: it may have moved round the place since the soldier chose it.
 const bool ByTarget=BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Primary+FVector(0,0,60)).Fraction()>CoverExposureLimit();
 const bool ByOther=!ByTarget && ExposedToThreats(ID,Anchor,Threats);
 if(!ByTarget && !ByOther){U->CoverFirstBreak=0;return false;}
 // Narrowed: one break may be a glimpse; he leaves when the next check (1 s later) finds the place broken again.
 if(ThreatCoverMode(U->Team)==2)
 {
  if(U->CoverFirstBreak<=0 || Now-U->CoverFirstBreak>2.5){U->CoverFirstBreak=Now;Diag.Cover(TEXT("threat_cover_first_break"));return false;}
  U->CoverFirstBreak=0;
 }
 Diag.Cover(ByTarget?TEXT("threat_cover_broken_by_target"):TEXT("threat_cover_broken_by_other"));
 UE_LOG(LogFutureBattle,Verbose,TEXT("BATTLE_THREAT_COVER_BROKEN unit=%d slot=%d threats=%d by=%s"),ID,U->CoverSlot,Threats.Num(),ByTarget?TEXT("target"):TEXT("other"));
 if(U->CoverSlot!=INDEX_NONE)BrokenCover.Add(ID,TPair<int32,double>(U->CoverSlot,Now+10.));
 // -BattleThreatCoverRelocate: the next place first (the broken one is excluded above). Not under the player's order to him
 // (he holds where he was sent). The search changes nothing when it fails, so the state before it is put back as it was.
 if(const int32 Relocate=ThreatCoverRelocate();Relocate>0 && !U->bSoldierOrder)
 {
  // Seen by another threat too (also when the target sees in): the next place must hide him from every threat.
  const bool bOthersSee=ByOther || (ByTarget && ExposedToThreats(ID,Anchor,Threats));
  // Searches share stage B1's budget (4 soldiers a frame). A soldier who stays (2) searches again on every third check
  // only (a stateless phase, as for the checks themselves).
  if(SoldierMoveFrame!=GFrameCounter){SoldierMoveFrame=GFrameCounter;SoldierMovesThisFrame=0;}
  const bool bBudget=SoldierMovesThisFrame<4;
  const bool bTurn=Relocate!=2 || FMath::FloorToInt(Now+Phase)%3==0;
  bool bFound=false;FBattleUnitState Before;
  if(bBudget && bTurn)
  {
   ++SoldierMovesThisFrame;Diag.Cover(TEXT("threat_relocate_try"));
   Before=*U;U->CoverPhase=0;
   const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Squad=Store->FindSquad(U->SquadID);const auto* Plan=Plans.Find(U->SquadID);
   // The squad's area, as for B1: inside its sector during a sector mission (when he is in it) and within 30 m of its centre;
   // not a place a squadmate or anyone else is already walking to. The squad's limit on how many relocate at once does not
   // apply: the other choice (leaving the place) moves him too.
   const bool bSector=Squad && Squad->bSectorMission && Squad->TacticalPlan.Action==EBattleSquadTactic::Mission && BattleSectors::Cell(Position(ID))==BattleSectors::Cell(Squad->Destination);
   const FIntPoint Sector=Squad?BattleSectors::Cell(Squad->Destination):FIntPoint::ZeroValue;const FVector Center=Plan?Plan->Center:FVector::ZeroVector;
   auto Accept=[&](int32 Slot,const FVector& A)
   {
    if(bSector && BattleSectors::Cell(A)!=Sector)return false;
    if(Plan && FVector::DistSquared2D(A,Center)>FMath::Square(3000.))return false;
    for(const auto& Ap:CoverApproaches)if(Ap.Key!=ID && Ap.Value.Slot==Slot)return false;
    return true;
   };
   const float Radius=FMath::Max(1200.f,GetDefault<UBattleTuning>()->AICoverSearchRadius);
   bFound=RequestCoverAction(ID,Primary,Before.CoverTargetID,Radius,bOthersSee,Accept);
  }
  else Diag.Cover(bBudget?TEXT("threat_relocate_wait_turn"):TEXT("threat_relocate_frame_full"));
  if(bFound)
  {
   // A longer walk keeps its cover timer while it lasts and is not ended halfway (as for stage B1); no B1 move right after it.
   U->SoldierMoveUntil=Now+FVector::Dist2D(Position(ID),U->CoverAnchor)/250.+1.5;U->CoverFirstBreak=0;U->SoldierMoveAfter=Now+8.;
   U->DecisionReason=TEXT("COVER / LEAVING A FLANKED PLACE");CoverApproaches.Remove(ID);
   Diag.Cover(TEXT("threat_relocate_found"));
   UE_LOG(LogFutureBattle,Verbose,TEXT("BATTLE_THREAT_COVER_RELOCATE unit=%d from=%d to=%d"),ID,Before.CoverSlot,U->CoverSlot);
   return false; // still in a cover action: on the way to the new place
  }
  if(bBudget && bTurn)*U=Before; // the search changes nothing when it fails
  if(Relocate==2)
  {
   // He stays; while another threat sees into the place he keeps down (no peek toward the target) until the next check.
   BrokenCover.Remove(ID);if(bOthersSee)U->CoverSeconds=FMath::Max(U->CoverSeconds,1.1f);
   Diag.Cover(TEXT("threat_relocate_none_stay"));return false;
  }
  Diag.Cover(TEXT("threat_relocate_none_leave"));
 }
 CancelCoverAction(ID);CoverProbeAfter.Remove(ID);
 return true;
}
