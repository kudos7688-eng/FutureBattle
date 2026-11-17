// Vehicle tactics, stage 2 (2026-11-18, user request "tactics first"): where an armoured car goes and what it avoids.
// Known enemies only (UBattleMatch::AreaIntelView: what own soldiers see now plus scout and lookout reports). A known enemy is told apart
// as an anti-tank soldier or a tank only while it is a seen unit (report positions, keys below zero, carry no type).
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleRanges.h"
#include "BattleFieldLayout.h"
#include "FutureBattle.h"

bool UBattleMatch::AreaArmourThreat(int32 Team,const FVector& P,bool bTankGuns) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return false;
 const auto& V=AreaIntelView(Team);
 const double Margin=FMath::Max(0.f,GetDefault<UBattleTuning>()->ACThreatMargin);
 const double AT=BattleRanges::Reach(EBattleWeapon::AntiTank)+Margin,Gun=R->TankGunEngage()+Margin;
 for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)
 {
  const int32 Key=V.EnemyKeys[I];if(Key<0)continue;
  const auto* U=Store->Find(Key);if(!U||!U->IsAlive())continue;
  double Reach=0;
  if(U->CarriesAntiTank())Reach=AT;
  else if(bTankGuns&&R->HasMainGun(Key))Reach=Gun;
  if(Reach>0&&FVector::DistSquared2D(P,V.Enemies[I])<=Reach*Reach)return true;
 }
 return false;
}

int32 UBattleMatch::AreaCarTarget(int32 Team,int32 SquadID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||Objectives.IsEmpty())return INDEX_NONE;
 const FVector Here=R->Position(S->LeaderUnitID);
 const bool bUrbanMap=BattleFieldLayout::IsUrban(GetWorld());
 auto InCity=[&](const FVector& P){return bUrbanMap&&FMath::Abs(P.X)<25000&&FMath::Abs(P.Y)<15000;};
 const auto& V=AreaIntelView(Team);
 // Score: +3 for a point the team does not hold (observed), +2 when known enemies are around it, -0.5 per 100 m of distance (quick
 // reaction); points in the city or under a known anti-tank threat only when nothing else is left (-100 / -50).
 auto ScoreOf=[&](int32 I)->double
 {
  const FVector P=Objectives[I].Position;
  double Sc=(ObservedOwner(Team,I)!=Team?3.:0.)+(V.EnemyAt.IsValidIndex(I)&&V.EnemyAt[I]>0?2.:0.)-FVector::Dist2D(Here,P)/10000.*.5;
  if(InCity(P))Sc-=100.;
  if(AreaArmourThreat(Team,P))Sc-=50.;
  return Sc;
 };
 int32 Best=INDEX_NONE;double BestScore=-DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I){const double Sc=ScoreOf(I);if(Sc>BestScore){BestScore=Sc;Best=I;}}
 auto& C=CarChoices.FindOrAdd(SquadID);
 // Keep the current point for ACChoiceHold seconds unless it became unsafe (a known threat) or another point is clearly better.
 if(C.Target!=INDEX_NONE&&Objectives.IsValidIndex(C.Target)&&C.Target!=Best)
 {
  const double Current=ScoreOf(C.Target);
  const bool bUnsafe=Current<=-50.;
  if(!bUnsafe&&(Elapsed-C.At<FMath::Max(0.f,GetDefault<UBattleTuning>()->ACChoiceHold)||BestScore<Current+1.))return C.Target;
 }
 if(C.Target!=Best){C.Target=Best;C.At=Elapsed;}
 return Best;
}
