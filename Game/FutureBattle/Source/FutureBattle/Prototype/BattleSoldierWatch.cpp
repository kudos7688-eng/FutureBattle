// Individual AI, watch (2026-11-22, -BattleSoldierWatch, default 1 by the user's decision of 2026-11-22; =0 turns it off): where a soldier who stands still with nothing to fire
// at looks. Sight has a forward field of view (FieldOfView, 120 degrees), and such a soldier kept facing the way he walked in, so
// enemies to the side shot him unseen (map I, 30 FPS: about 1100 hits a match on soldiers standing in the open whose shooter saw
// them while they did not see the shooter). Observed information only: his own contacts (sightings and reports) and the map
// (where the enemy's home is). Part of UBattleLeaderRuntime.
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleState.h"

bool UBattleLeaderRuntime::SoldierWatchEnabled(int32 Team)
{
 static const bool bOn=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierWatch="),V);return V!=0;}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierWatchTeam="),V);return V==0||V==1?V:-1;}();
 return bOn && (Only<0 || Only==Team);
}

bool UBattleLeaderRuntime::SoldierWatchDirection(int32 ID,int32 TargetID,const FVector& Pos,double Now,FVector& Out)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return false;
 const auto* Perc=GetWorld()->GetSubsystem<UBattlePerception>();
 // Not before his first perception scan (of the match, or after a load): a load rebuilds the squads' plans without targets and
 // restores every contact out of sight, so a soldier who was firing would turn away from his enemy. Until that scan he keeps the
 // facing he had (saved with his body), as in the match that was saved.
 if(!Perc->LastScanTimes.Contains(ID))return false;
 auto& Diag=SightExperiment;
 // A contact he still holds: seen now, or within his memory (expired ones are dropped only at his next perception scan).
 const double Memory=U->Perception.MemorySeconds;
 auto Held=[&](const FBattleContact& K){return K.bVisible || Now-K.LastSeenTime<=Memory;};
 if(const auto* Contacts=Perc->GetContacts(ID))
 {
  // 1. The squad's target, when he knows where it is (out of sight now): he keeps looking there.
  if(const FBattleContact* K=Contacts->Find(TargetID); K && Held(*K))
   if(const auto* T=Store->Find(TargetID); T && T->IsAlive() && T->Team!=U->Team)
   {
    Out=(K->LastKnownPosition-Pos).GetSafeNormal2D();
    if(!Out.IsNearlyZero()){Diag.Cover(TEXT("watch_target_frames"));return true;}
   }
  // 2. Else the nearest enemy he sees himself now, and 3. else the nearest he knows of (his memory and his comrades' reports,
  // still held), within his weapon's reach: he looks where it is or was last known. One he sees comes first, so he does not
  // turn away from it toward one he only heard of (also in the moments after a load, before his squad picks its target again).
  const double Reach=FMath::Square(double(BattleRanges::Reach(U->Weapon)));
  double BestSeen=Reach,BestHeld=Reach;FVector AtSeen=FVector::ZeroVector,AtHeld=FVector::ZeroVector;
  for(const auto& C:*Contacts)
  {
   if(!Held(C.Value))continue;
   const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
   const double D=FVector::DistSquared2D(Pos,C.Value.LastKnownPosition);if(D<=1.)continue;
   if(C.Value.bVisible && !C.Value.bShared && D<BestSeen){BestSeen=D;AtSeen=C.Value.LastKnownPosition;}
   if(D<BestHeld){BestHeld=D;AtHeld=C.Value.LastKnownPosition;}
  }
  if(BestSeen<Reach){Out=(AtSeen-Pos).GetSafeNormal2D();if(!Out.IsNearlyZero()){Diag.Cover(TEXT("watch_seen_frames"));return true;}}
  if(BestHeld<Reach){Out=(AtHeld-Pos).GetSafeNormal2D();if(!Out.IsNearlyZero()){Diag.Cover(TEXT("watch_contact_frames"));return true;}}
 }
 // 4. Nothing known: the squad shares the ground ahead between its members - toward the enemy's home, and 70 degrees to either
 // side (with 120 degrees each, the squad watches 260 degrees). Fixed by the member's place in the squad, so it does not sweep.
 const auto* S=Store->FindSquad(U->SquadID);const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(!S || !Match || !Match->IsEnabled() || (U->Team!=0 && U->Team!=1))return false;
 const FVector Front=(Match->AreaHome(1-U->Team)-Pos).GetSafeNormal2D();if(Front.IsNearlyZero())return false;
 static const float Offsets[]={0.f,70.f,-70.f};
 const int32 Index=FMath::Max(0,S->Members.IndexOfByKey(ID));
 Out=Front.RotateAngleAxis(Offsets[Index%3],FVector::UpVector);
 Diag.Cover(TEXT("watch_sector_frames"));
 return true;
}
