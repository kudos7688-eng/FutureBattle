// Individual AI, watch (2026-11-22, trial -BattleSoldierWatch=1, default 0): where a soldier who stands still with nothing to fire
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
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierWatch="),V);return V!=0;}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierWatchTeam="),V);return V==0||V==1?V:-1;}();
 return bOn && (Only<0 || Only==Team);
}

bool UBattleLeaderRuntime::SoldierWatchDirection(int32 ID,const FVector& Pos,double Now,FVector& Out)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return false;
 auto& Diag=SightExperiment;
 // 1. The nearest enemy he knows of (his own sightings and his comrades' reports, still in his memory), within his weapon's reach:
 // he looks where it was last known.
 if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
 {
  const double Reach=FMath::Square(double(BattleRanges::Reach(U->Weapon)));double Best=Reach;FVector At=FVector::ZeroVector;
  for(const auto& C:*Contacts)
  {
   const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
   const double D=FVector::DistSquared2D(Pos,C.Value.LastKnownPosition);
   if(D<Best && D>1.e4){Best=D;At=C.Value.LastKnownPosition;}
  }
  if(Best<Reach){Out=(At-Pos).GetSafeNormal2D();if(!Out.IsNearlyZero()){Diag.Cover(TEXT("watch_contact_frames"));return true;}}
 }
 // 2. Nothing known: the squad shares the ground ahead between its members - toward the enemy's home, and 70 degrees to either
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
