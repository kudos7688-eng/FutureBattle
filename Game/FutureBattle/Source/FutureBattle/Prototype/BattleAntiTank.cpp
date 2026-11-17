// Anti-tank weapon explosion (2026-11-17, user decision: before vehicles exist, the anti-tank team may fire at infantry).
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleMatch.h"

int32 UBattleLeaderRuntime::Explode(int32 ShooterID,FVector At,int32 ExcludeID,float RadiusIn,float DamageIn)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Shooter=Store->Find(ShooterID);if(!Shooter)return 0;
 const auto* T=GetDefault<UBattleTuning>();const float Radius=FMath::Max(1.f,RadiusIn>=0.f?RadiusIn:T->ATSplashRadius);const float Splash=DamageIn>=0.f?DamageIn:T->ATSplashDamage;
 const int32 Team=Shooter->Team;const auto* ShooterSquad=Store->FindSquad(Shooter->SquadID);
 // Collect first: damage broadcasts deaths, which may change the body map.
 TArray<TPair<int32,float>> Hits;
 for(const auto& B:Bodies)
 {
  if(B.Key==ExcludeID||IsVehicle(B.Key))continue;const auto* U=Store->Find(B.Key);if(!U||!U->IsAlive()||U->Team==Team)continue; // armour: splash does not hurt vehicles
  const double D=FVector::Dist(Position(B.Key),At);if(D>Radius)continue;
  Hits.Add({B.Key,float(Splash*(1.0-D/Radius))});
 }
 Hits.Sort([](const TPair<int32,float>& A,const TPair<int32,float>& B){return A.Key<B.Key;});
 int32 Damaged=0;
 for(const auto& H:Hits)
 {
  const auto* Before=Store->Find(H.Key);if(!Before||!Before->IsAlive())continue;
  const float BeforeHP=Before->HP;const int32 VictimTeam=Before->Team;const auto* VictimSquad=Store->FindSquad(Before->SquadID);const uint8 VictimType=VictimSquad?VictimSquad->SquadType:0;
  if(!Store->ApplyDamage(H.Key,Team,H.Value))continue;++Damaged;
  if(SightExperiment.Started)
  {
   auto& E=SightExperiment;const float Dealt=FMath::Min(BeforeHP,H.Value);const bool Killed=!Store->Find(H.Key)->IsAlive();
   if(ShooterSquad&&ShooterSquad->SquadType<BattleSquadType::Count&&Team>=0&&Team<2){E.TypeDamage[Team][ShooterSquad->SquadType]+=Dealt;E.TypeKills[Team][ShooterSquad->SquadType]+=Killed;}
   if(VictimType<BattleSquadType::Count&&VictimTeam>=0&&VictimTeam<2){E.TypeTaken[VictimTeam][VictimType]+=Dealt;E.TypeDeaths[VictimTeam][VictimType]+=Killed;}
   E.TeamDamage[Team]+=Dealt;E.TeamKills[Team]+=Killed;
  }
 }
 return Damaged;
}
