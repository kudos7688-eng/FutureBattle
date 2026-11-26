// Individual AI, watch (2026-11-22, -BattleSoldierWatch, default 1 by the user's decision of 2026-11-22; =0 turns it off): where a soldier who stands still with nothing to fire
// at looks. Sight has a forward field of view (FieldOfView, 120 degrees), and such a soldier kept facing the way he walked in, so
// enemies to the side shot him unseen (map I, 30 FPS: about 1100 hits a match on soldiers standing in the open whose shooter saw
// them while they did not see the shooter). Observed information only: his own contacts (sightings and reports) and the map
// (where the enemy's home is). Part of UBattleLeaderRuntime.
// Walking look (2026-11-22, trial -BattleWalkLook, default 0): the same for a soldier on the move. His body keeps to his way and
// he turns his head: only his view (Sight) changes. In close fights (sight and engagement cut to 50 m) soldiers were hit from
// outside their view three times as often as in the usual long fights, and twice as often while walking.
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

int32 UBattleLeaderRuntime::WalkLookMode(int32 Team)
{
 static const int32 Value=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleWalkLook="),V);return FMath::Clamp(V,0,2);}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleWalkLookTeam="),V);return V==0||V==1?V:-1;}();
 return Only<0 || Only==Team ? Value : 0;
}

// The enemy a soldier with nothing to fire at knows of best, from his own contacts only. Returns 1 the squad's target when he
// knows where it is, 2 the nearest enemy he sees himself now, 3 the nearest he knows of (memory and reports), 0 none.
// Within (cm, 0 = no limit of its own): only enemies that near (the walking look: no further than he can see).
int32 UBattleLeaderRuntime::KnownEnemyDirection(int32 ID,int32 TargetID,const FVector& Pos,double Now,FVector& Out,double Within) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return 0;
 const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID);if(!Contacts)return 0;
 // A contact he still holds: seen now, or within his memory (expired ones are dropped only at his next perception scan).
 const double Memory=U->Perception.MemorySeconds;
 auto Held=[&](const FBattleContact& K){return K.bVisible || K.bTracked || Now-K.LastSeenTime<=Memory;};
 // 1. The squad's target, when he knows where it is (out of sight now): he keeps looking there.
 const auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();FBattleContact KnownOfTarget;
 if(const FBattleContact* K=Sense->Knows(ID,U->SquadID,TargetID,KnownOfTarget)?&KnownOfTarget:nullptr; K && Held(*K))
  if(const auto* T=Store->Find(TargetID); T && T->IsAlive() && T->Team!=U->Team)
  if(Within<=0. || FVector::DistSquared2D(Pos,K->MoveAt())<=FMath::Square(Within))
  {
   Out=(K->MoveAt()-Pos).GetSafeNormal2D();
   if(!Out.IsNearlyZero())return 1;
  }
 // 2. Else the nearest enemy he sees himself now, and 3. else the nearest he knows of (his memory and his comrades' reports,
 // still held), within his weapon's reach: he looks where it is or was last known. One he sees comes first, so he does not
 // turn away from it toward one he only heard of (also in the moments after a load, before his squad picks its target again).
 const double Reach=FMath::Square(Within>0.?FMath::Min(Within,double(BattleRanges::Reach(U->Weapon))):double(BattleRanges::Reach(U->Weapon)));
 double BestSeen=Reach,BestHeld=Reach;FVector AtSeen=FVector::ZeroVector,AtHeld=FVector::ZeroVector;
 for(const auto& C:*Contacts)
 {
  if(!Held(C.Value))continue;
  const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
  const double D=FVector::DistSquared2D(Pos,C.Value.MoveAt());if(D<=1.)continue;
  if(C.Value.bVisible && !C.Value.bShared && D<BestSeen){BestSeen=D;AtSeen=C.Value.MoveAt();}
  if(D<BestHeld){BestHeld=D;AtHeld=C.Value.MoveAt();}
 }
 // -BattleSquadKnowledge=1: what his comrades saw and what the squad heard is in the squad's table.
 if(const auto* SquadKnown=Sense->GetSquadContacts(U->SquadID))for(const auto& C:*SquadKnown)
 {
  if(!Held(C.Value.Contact))continue;
  const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
  const double D=FVector::DistSquared2D(Pos,C.Value.Contact.LastKnownPosition);
  if(D>1. && D<BestHeld){BestHeld=D;AtHeld=C.Value.Contact.LastKnownPosition;}
 }
 if(BestSeen<Reach){Out=(AtSeen-Pos).GetSafeNormal2D();if(!Out.IsNearlyZero())return 2;}
 if(BestHeld<Reach){Out=(AtHeld-Pos).GetSafeNormal2D();if(!Out.IsNearlyZero())return 3;}
 return 0;
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
 // 1-3. The enemy he knows of (KnownEnemyDirection).
 static const TCHAR* const Keys[]={nullptr,TEXT("watch_target_frames"),TEXT("watch_seen_frames"),TEXT("watch_contact_frames")};
 if(const int32 Kind=KnownEnemyDirection(ID,TargetID,Pos,Now,Out,0.)){Diag.Cover(Keys[Kind]);return true;}
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

bool UBattleLeaderRuntime::WalkLookDirection(int32 ID,int32 TargetID,const FVector& Pos,const FVector& Way,double Now,FVector& Out)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return false;
 const int32 LookMode=WalkLookMode(U->Team);if(!LookMode)return false;
 // As the watch: not before his first perception scan (match start or a load).
 if(!GetWorld()->GetSubsystem<UBattlePerception>()->LastScanTimes.Contains(ID))return false;
 const auto* S=Store->FindSquad(U->SquadID);const FVector Ahead=Way.GetSafeNormal2D();if(!S || Ahead.IsNearlyZero())return false;
 auto& Diag=SightExperiment;
 // The squad shares the ground around its way between its members: ahead, 50 degrees and 100 degrees to either side (with 120
 // degrees each, the squad sees 320 degrees). Fixed by the member's place in the squad, so the view does not sweep.
 static const float Offsets[]={0.f,50.f,-50.f,100.f,-100.f};
 const float Offset=Offsets[FMath::Max(0,S->Members.IndexOfByKey(ID))%5];
 // An enemy he knows of within his sight comes first (the squad's target, one he sees, one reported): he looks there, sees it
 // and can fire on the move. =2: the members who look ahead keep looking ahead, where the squad is walking into.
 static const TCHAR* const Keys[]={nullptr,TEXT("walk_look_target_frames"),TEXT("walk_look_seen_frames"),TEXT("walk_look_contact_frames")};
 if(LookMode<2 || Offset!=0.f) if(const int32 Kind=KnownEnemyDirection(ID,TargetID,Pos,Now,Out,U->Perception.SightRange)){Diag.Cover(Keys[Kind]);return true;}
 Out=Ahead.RotateAngleAxis(Offset,FVector::UpVector);
 Diag.Cover(Offset==0.f?TEXT("walk_look_ahead_frames"):TEXT("walk_look_side_frames"));
 return true;
}
