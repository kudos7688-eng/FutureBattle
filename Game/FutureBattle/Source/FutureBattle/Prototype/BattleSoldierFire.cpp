// Individual AI, own target (2026-11-22, trial -BattleOwnTarget=1/2, default 0; -BattleOwnTargetTeam=0/1 for one team): whom a
// soldier fires at when he cannot fire at the squad's target. A soldier fires only at the one target of his squad (Follow:
// P->TargetID), and only when he sees it himself. In close fights (sight and engagement cut to 50 m, map I) half of the soldiers
// hit while standing in the open had nothing they could fire at - the squad had no target, or one he did not know or did not
// see. With this trial he fires at an enemy he sees himself within the engagement distance. =2: hit with nothing to fire at, he
// also turns toward where the shot came from for 2 s (as the trial -BattleHitFace), so that he sees who fires at him.
// Only his aim, his fire and (=2) that turn change: the squad's target, its movement and its cover stay as they are. Observed
// information only: his own sightings and the direction he was hit from. Nothing is saved (chosen anew each frame from his
// contacts). Part of UBattleLeaderRuntime.
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleState.h"
#include "BattleTactics.h"

int32 UBattleLeaderRuntime::OwnTargetMode(int32 Team)
{
 static const int32 Value=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleOwnTarget="),V);return FMath::Clamp(V,0,2);}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleOwnTargetTeam="),V);return V==0||V==1?V:-1;}();
 return Only<0 || Only==Team ? Value : 0;
}

// Trial -BattleOwnTargetCover=1/2 (default 0, 2026-11-22, user: with -BattleOwnTarget more soldiers fire standing in the open): a
// soldier who fires at his own target looks for cover near by against that enemy (the usual cover action, with that enemy as
// its target) and fires from it. =1: only a soldier who is not walking (the squad's route, fire on the move, his arrival walk
// or the player's order). =2: also a walking soldier when that enemy is within 50 m - he leaves the march until the cover
// action ends (the enemy unseen for 5 s, out of range, a new order). Needs -BattleOwnTarget; the same team limit.
// No cover that near (about 6 m): a soldier who is not walking goes to cover up to 15 m away (StartCoverApproach with
// bHoldToo - soldiers of a holding squad too, who are most of those standing) and asks again there.
int32 UBattleLeaderRuntime::OwnTargetCover(int32 Team)
{
 static const int32 Value=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleOwnTargetCover="),V);return FMath::Clamp(V,0,2);}();
 return OwnTargetMode(Team) ? Value : 0;
}

// The enemy soldier he sees himself now (not a report) within the distance at which he may open fire - the same rule as for the
// squad's target: his weapon's engagement distance, its reach while he is under fire. Just hit (2 s): one in the direction the
// shot came from (within 15 degrees) comes first; else the nearest. bAtShooter tells which. INDEX_NONE when he sees none.
int32 UBattleLeaderRuntime::OwnTarget(int32 ID,const FVector& Pos,double Now,bool& bAtShooter) const
{
 bAtShooter=false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return INDEX_NONE;
 const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID);if(!Contacts)return INDEX_NONE;
 const double Limit=bRangeAwareCombat?FMath::Square(double(UnderFire(ID)?BattleRanges::Reach(U->Weapon):BattleRanges::Engage(U->Weapon))):TNumericLimits<double>::Max();
 FVector From;const bool bHit=UnitShotFrom(ID,Now,From,2.);
 const FVector Toward=bHit?(From-Pos).GetSafeNormal2D():FVector::ZeroVector;
 int32 Best=INDEX_NONE;double BestDistance=TNumericLimits<double>::Max();bool bBestShooter=false;
 for(const auto& C:*Contacts)
 {
  if(!C.Value.bVisible || C.Value.bShared)continue;
  const auto* T=Store->Find(C.Key);if(!T || !T->IsAlive() || T->Team==U->Team)continue;
  // Soldiers only: a vehicle is the business of the anti-tank gunner and the squad's vehicle target (BattleArmourAI.cpp).
  if(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(C.Value.ObservedSquadType):IsVehicle(C.Key))continue;
  const double D=FVector::DistSquared(Pos,C.Value.LastKnownPosition);if(D>Limit || D<=1.)continue;
  const bool bShooter=!Toward.IsNearlyZero() && FVector::DotProduct(Toward,(C.Value.LastKnownPosition-Pos).GetSafeNormal2D())>=.966;
  // The one who is firing at him first, then the nearer; the lower ID on a tie, so the choice does not hang on the map's order.
  if(bShooter!=bBestShooter ? bShooter : (D<BestDistance || (D==BestDistance && C.Key<Best)))
  {Best=C.Key;BestDistance=D;bBestShooter=bShooter;}
 }
 bAtShooter=bBestShooter;
 return Best;
}
