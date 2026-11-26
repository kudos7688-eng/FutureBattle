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
#include "BattleStance.h"
#include "BattleUnit.h"

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
 static const int32 Mode=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCoverRelocate="),V);return FMath::Clamp(V,0,3);}();
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
   const double D=FVector::DistSquared2D(Pos,K.MoveAt());if(D>FMath::Square(double(Reach)))continue;
   // Narrowed: an enemy he sees now and that faces him (within 30 degrees); its facing is what he sees of it.
   if(bNarrow && !(K.bVisible && FVector::DotProduct(Forward(C.Key).GetSafeNormal2D(),(Pos-K.MoveAt()).GetSafeNormal2D())>.866f))continue;
   Near.Add({D,K.MoveAt()});
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

bool UBattleLeaderRuntime::ThreatCoverCrouch(int32 Team)
{
 // -BattleThreatCoverCrouchTeam=0/1 limits it to one team (one-sided trials); -1 = both.
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCoverCrouch="),V);return V==1;}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleThreatCoverCrouchTeam="),V);return V==0||V==1?V:-1;}();
 return bOn && (Only<0 || Only==Team);
}

float UBattleLeaderRuntime::ThreatSeen(int32 ID,FVector Anchor,FVector Threat) const
{
 // How much of the soldier at a cover place a threat sees (the fraction compared with CoverExposureLimit). By default the
 // standing body's five points (150/120/115/115/55 cm above the ground). -BattleThreatCoverCrouch=1 (2026-11-21, trial):
 // with stances on, a stage A soldier is judged with the crouched body's points (110/80/55 cm and 90 cm to either side),
 // the body that sight traces and shots meet while he is hidden there (a hidden soldier crouches). Judged from the place,
 // not from his stance now: the stance is not saved, and a place he searches for is judged as he will be there. Used for
 // the target and the other threats alike, in breaks and in choosing places (RequestCoverAction, ContactFirePoint,
 // StartCoverApproach).
 const FVector From=Threat+FVector(0,0,60);
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* Body=Bodies.Find(ID);const bool bPlayer=Body && Body->Actor.IsValid() && Body->Actor->IsPlayerControlled(); // not the player's own cover (C key)
 if(!U || !ThreatCoverCrouch(U->Team) || bPlayer || !BattleStance::Enabled(GetWorld()) || !ThreatCoverEnabled(U->Team) || !SoldierAIFocus(ID))
  return BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),From).Fraction();
 const FVector Right=FVector::CrossProduct(FVector::UpVector,(From-Anchor).GetSafeNormal2D());
 int32 Seen=0;
 for(const FVector& O:{FVector(0,0,20),FVector(0,0,-10),FVector(0,0,-35),Right*25.f,-Right*25.f})Seen+=BattleTactics::HasTerrainLine(GetWorld(),Anchor+O,From);
 return Seen/5.f;
}

bool UBattleLeaderRuntime::ExposedToThreats(int32 ID,FVector Anchor,const TArray<FVector,TInlineAllocator<3>>& Threats) const
{
 // Index 0 is the target, already checked by the caller; a place is flanked when a threat sees more than the limit.
 for(int32 I=1;I<Threats.Num();++I)
  if(ThreatSeen(ID,Anchor,Threats[I])>CoverExposureLimit())return true;
 return false;
}

int32 UBattleLeaderRuntime::ExposingEnemy(int32 ID,FVector Anchor,double Now) const
{
 // -BattleThreatCoverRelocate=3: the nearest enemy (his target included) that he sees himself now, that sees into the place,
 // and that he can fire at from the place itself: within the range the cover action keeps (TargetRange) and fires at (Engage,
 // Reach while under fire), with a clear line from the place. Observed information only (his own sightings, as GatherThreats).
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return INDEX_NONE;
 const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID);if(!Contacts)return INDEX_NONE;
 const float Range=FMath::Min(TargetRange(ID),UnderFire(ID)?BattleRanges::Reach(U->Weapon):BattleRanges::Engage(U->Weapon));
 int32 Best=INDEX_NONE;double BestD=TNumericLimits<double>::Max();
 for(const auto& C:*Contacts)
 {
  const auto& K=C.Value;if(K.bShared || !K.bVisible || Now-K.LastSeenTime>.5)continue;
  const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
  const double D=FVector::DistSquared2D(Anchor,K.MoveAt());if(D>FMath::Square(double(Range)) || D>=BestD)continue;
  if(ThreatSeen(ID,Anchor,K.MoveAt())<=CoverExposureLimit())continue;
  if(!CoverLineClear(ID,Anchor,K.MoveAt()))continue;
  Best=C.Key;BestD=D;
 }
 return Best;
}

bool UBattleLeaderRuntime::BrokenCoverSlot(int32 ID,int32 Slot,double Now) const
{
 const auto* B=BrokenCover.Find(ID);if(!B || Now>=B->Until)return false;
 const auto& Slots=GetWorld()->GetSubsystem<UBattleCover>()->GetSlots();
 return Slots.IsValidIndex(Slot) && FVector::DistSquared(Slots[Slot],B->At)<1.;
}

void UBattleLeaderRuntime::MarkBrokenCover(int32 ID,int32 Slot,double Until)
{
 const auto& Slots=GetWorld()->GetSubsystem<UBattleCover>()->GetSlots();
 if(Slots.IsValidIndex(Slot))BrokenCover.Add(ID,FBattleBrokenCover{Slot,Slots[Slot],Until});
}

bool UBattleLeaderRuntime::CheckCoverThreats(int32 ID,FVector Primary,double Now,float Dt)
{
 // A soldier whose search found the frame's budget full searches again on the next frame (not at his next phase), so soldiers
 // sharing a phase do not leave the same ones waiting every second. The break is already confirmed: no new threat check, so
 // the retry costs only the search and is not counted as another break.
 const bool* Waiting=RelocateWaiting.Find(ID);const bool bWaited=Waiting!=nullptr;const bool bWaitedOthersSee=bWaited && *Waiting;
 RelocateWaiting.Remove(ID);
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);
 if(!U || U->CoverPhase!=2 || U->CoverSlot==FreeCoverSlot || !ThreatCoverEnabled(U->Team) || !SoldierAIFocus(ID))return false;
 // Once a second per soldier, at a phase fixed by the unit ID (no state, so a saved and loaded match checks at the same times).
 const double Phase=double(ID%16)/16.;
 if(!bWaited && FMath::FloorToDouble(Now+Phase)==FMath::FloorToDouble(Now-Dt+Phase))return false;
 auto& Diag=SightExperiment;Diag.Cover(bWaited?TEXT("threat_relocate_retry"):TEXT("threat_cover_check"));
 // Cost of the check (the individual AI is not in custom_ai_ms_per_second): timed only in evaluation runs.
 const double CheckStart=Diag.Enabled?FPlatformTime::Seconds():0.;
 ON_SCOPE_EXIT{if(Diag.Enabled)Diag.ThreatCheckMS.Add((FPlatformTime::Seconds()-CheckStart)*1000.);};
 const int32 Relocate=ThreatCoverRelocate();
 bool bOthersSee=bWaitedOthersSee;
 if(!bWaited)
 {
  TArray<FVector,TInlineAllocator<3>> Threats;GatherThreats(ID,Primary,Now,Threats);
  const FVector Anchor=U->CoverAnchor;
  // The target too: it may have moved round the place since the soldier chose it.
  const bool ByTarget=ThreatSeen(ID,Anchor,Primary)>CoverExposureLimit();
  int32 Breaker=INDEX_NONE;
  if(!ByTarget)for(int32 I=1;I<Threats.Num();++I)
   if(ThreatSeen(ID,Anchor,Threats[I])>CoverExposureLimit()){Breaker=I;break;}
  const bool ByOther=Breaker!=INDEX_NONE;
  if(!ByTarget && !ByOther){U->CoverFirstBreak=0;return false;}
  // -BattleThreatCoverRelocate=2: while another threat sees into the place he keeps down (no peek toward the target), at every
  // check, also before a break is confirmed.
  bOthersSee=Relocate>0 && (ByOther || ExposedToThreats(ID,Anchor,Threats));
  if(Relocate==2 && bOthersSee && !U->bSoldierOrder)U->CoverSeconds=FMath::Max(U->CoverSeconds,1.1f); // not under the player's order to him
  // Narrowed: one break may be a glimpse; he leaves when the next check (1 s later) finds the place broken again.
  if(ThreatCoverMode(U->Team)==2)
  {
   if(U->CoverFirstBreak<=0 || Now-U->CoverFirstBreak>2.5){U->CoverFirstBreak=Now;Diag.Cover(TEXT("threat_cover_first_break"));return false;}
   U->CoverFirstBreak=0;
  }
  Diag.Cover(ByTarget?TEXT("threat_cover_broken_by_target"):TEXT("threat_cover_broken_by_other"));
  // Evaluation only: the angle between the target and the threat that broke the place, seen from the place (is it really a
  // flank, or another enemy of the same front?), and how far that threat was.
  // Evaluation only: the break is judged with the standing body's five points (150/120/115/115/55 cm), but a hidden soldier
  // is crouched (top 110 cm, seen and aimed at 80 cm). Counted: breaks where the crouched body's points (110/80/55 and 90 cm
  // to either side) would still be hidden within the same limit.
  if(Diag.Enabled && !ThreatCoverCrouch(U->Team) && StanceOf(ID)==BattleStance::Crouch) // with the crouch trial this is the judgment itself
  {
   const FVector From=(ByTarget?Primary:Threats[Breaker])+FVector(0,0,60);
   const FVector Right=FVector::CrossProduct(FVector::UpVector,(From-Anchor).GetSafeNormal2D());
   int32 Seen=0;
   for(const FVector& O:{FVector(0,0,20),FVector(0,0,-10),FVector(0,0,-35),Right*25.f,-Right*25.f})Seen+=BattleTactics::HasTerrainLine(GetWorld(),Anchor+O,From);
   Diag.Cover(Seen/5.f>CoverExposureLimit()?TEXT("threat_break_crouch_seen"):TEXT("threat_break_crouch_hidden"));
  }
  if(ByOther && Diag.Enabled)
  {
   const double Cos=FVector::DotProduct((Primary-Anchor).GetSafeNormal2D(),(Threats[Breaker]-Anchor).GetSafeNormal2D());
   Diag.Cover(Cos>.7071?TEXT("threat_break_angle_0_45"):Cos>0.?TEXT("threat_break_angle_45_90"):Cos>-.7071?TEXT("threat_break_angle_90_135"):TEXT("threat_break_angle_135_180"));
   const double Dist=FVector::Dist2D(Anchor,Threats[Breaker]);
   Diag.Cover(Dist<1500.?TEXT("threat_break_dist_lt15m"):Dist<3000.?TEXT("threat_break_dist_15_30m"):TEXT("threat_break_dist_gt30m"));
  }
  UE_LOG(LogFutureBattle,Verbose,TEXT("BATTLE_THREAT_COVER_BROKEN unit=%d slot=%d threats=%d by=%s"),ID,U->CoverSlot,Threats.Num(),ByTarget?TEXT("target"):TEXT("other"));
 }
 else if(U->bSoldierOrder)return false; // ordered while he waited: he holds; his next check decides
 MarkBrokenCover(ID,U->CoverSlot,Now+10.);
 // -BattleThreatCoverRelocate: the next place first (the broken one is excluded above). Not under the player's order to him
 // (he holds where he was sent). The search changes nothing when it fails, so the state before it is put back as it was.
 if(Relocate>0 && !U->bSoldierOrder)
 {
  // Seen by another threat too (also when the target sees in): the next place must hide him from every threat.
  // Searches have their own budget (2 soldiers a frame, apart from B1's 4). A soldier who stays (2) searches again at each
  // confirmed break: with -BattleThreatCover=2 every second check, as the confirmation starts over after each one.
  if(RelocateFrame!=GFrameCounter){RelocateFrame=GFrameCounter;RelocatesThisFrame=0;}
  const bool bBudget=RelocatesThisFrame<2;
  bool bFound=false;FBattleUnitState Before;
  if(bBudget)
  {
   ++RelocatesThisFrame;Diag.Cover(TEXT("threat_relocate_try"));
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
   const double SearchStart=Diag.Enabled?FPlatformTime::Seconds():0.;
   bFound=RequestCoverAction(ID,Primary,Before.CoverTargetID,Radius,bOthersSee,Accept);
   if(Diag.Enabled)Diag.RelocateSearchMS.Add((FPlatformTime::Seconds()-SearchStart)*1000.);
  }
  else
  {
   // No search this frame: not a failed search. He keeps his place and searches on the next frame with the break already
   // confirmed, so a full frame never makes him leave without having looked. He keeps down until then (no peek starts in
   // this frame). The first break is kept for the case the retry is lost (a load): his next check then confirms it.
   BrokenCover.Remove(ID);U->CoverFirstBreak=Now;RelocateWaiting.Add(ID,bOthersSee);U->CoverSeconds=FMath::Max(U->CoverSeconds,.25f);
   Diag.Cover(TEXT("threat_relocate_frame_full"));return false;
  }
  if(bFound)
  {
   // A longer walk keeps its cover timer while it lasts and is not ended halfway (as for stage B1); no B1 move right after it.
   U->SoldierMoveUntil=Now+FVector::Dist2D(Position(ID),U->CoverAnchor)/250.+1.5;U->CoverFirstBreak=0;U->SoldierMoveAfter=Now+8.;
   U->DecisionReason=TEXT("COVER / LEAVING A FLANKED PLACE");CoverApproaches.Remove(ID);
   Diag.Cover(TEXT("threat_relocate_found"));
   UE_LOG(LogFutureBattle,Verbose,TEXT("BATTLE_THREAT_COVER_RELOCATE unit=%d from=%d to=%d"),ID,Before.CoverSlot,U->CoverSlot);
   return false; // still in a cover action: on the way to the new place
  }
  if(bBudget)*U=Before; // the search changes nothing when it fails
  if(Relocate==2){BrokenCover.Remove(ID);Diag.Cover(TEXT("threat_relocate_none_stay"));return false;} // he stays (kept down above)
  if(Relocate==3)
  {
   // He stays and fights from the place: it is seen into anyway, so he peeks where he is (no step out) at the nearest enemy
   // that sees in and that he can hit from there (his target when that is the one). With none such (seen into from where he
   // was hit, or no line from the place) he keeps his usual peek at his target. When the target changes, no peek starts and
   // no B1 move is chosen in this frame, whose contact is the old target's.
   BrokenCover.Remove(ID);
   const int32 Enemy=ExposingEnemy(ID,U->CoverAnchor,Now);
   if(Enemy==INDEX_NONE)Diag.Cover(TEXT("threat_fire_none"));
   else
   {
    if(Enemy!=U->CoverTargetID){U->CoverTargetID=Enemy;U->SoldierMoveAfter=FMath::Max(U->SoldierMoveAfter,Now+1.);Diag.Cover(TEXT("threat_turn_to_other"));}
    else Diag.Cover(TEXT("threat_fire_in_place"));
    U->CoverPeek=U->CoverAnchor;U->CoverSeconds=FMath::Max(U->CoverSeconds,.25f);
   }
   Diag.Cover(TEXT("threat_relocate_none_stay"));return false;
  }
  Diag.Cover(TEXT("threat_relocate_none_leave"));
 }
 CancelCoverAction(ID);CoverProbeAfter.Remove(ID);
 return true;
}
