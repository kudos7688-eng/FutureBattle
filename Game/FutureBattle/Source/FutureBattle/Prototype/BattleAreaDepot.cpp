// Depots in the team AI (ROADMAP 2026-11-18): on maps whose objectives have a worth (FMapDef::ObjectiveValues; the depots of map F),
// the weight the point allocation (S1) gives an objective worth more than one depends on its situation:
//  - the team's own depot, held and quiet: a light guard (DepotGuard);
//  - own depot with enemies the team currently knows of within 150m: DepotThreat, so squads are sent to defend it;
//  - own depot lost: DepotRetake (on top of the attack weight every point not held gets);
//  - the enemy depot: DepotRaid while the team observes holding more of the ordinary objectives than the enemy, DepotRaidBehind
//    otherwise (no trickle of single squads deep into the enemy rear while behind); once taken it is held like any point (DepotHold).
// The team knows whether it still holds its own depot (its income tells it); everything else is observed information only.
// Trial switch, default off: -BattleAreaDepot=1 turns it on (with -BattleAreaTrialTeam, for that team only).
// -BattleAreaDepotRaid=<weight> replaces DepotRaid (1 = guard and retake only).
#include "BattleMatch.h"
#include "BattleFieldLayout.h"

namespace
{
 // First values (numbers delegated; map F only).
 constexpr float DepotGuard=.5f,DepotThreat=3.f,DepotRetake=2.f,DepotRaid=2.f,DepotRaidBehind=.5f,DepotHold=1.5f;
}

void UBattleMatch::ApplyDepotWeights(int32 Team,bool bTrialTeam,FAreaIntent& Intent) const
{
 static const int32 Mode=[]{int32 Depot=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaDepot="),Depot);return Depot;}();
 static const float Raid=[]{float DepotRaidWeight=DepotRaid;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaDepotRaid="),DepotRaidWeight);return FMath::Clamp(DepotRaidWeight,0.f,10.f);}();
 const auto& Def=BattleFieldLayout::Of(GetWorld());const int32 N=Objectives.Num();
 if(!bArea||Def.ObjectiveValues.IsEmpty()||Mode!=1||!bTrialTeam||N>FAreaIntent::MaxObjectives||Team<0||Team>1)return;
 // Asked once per squad and update: the weights are worked out once per team and match time.
 TArray<float>& Weights=DepotWeights[Team];
 if(DepotWeightsAt[Team]!=Elapsed||Weights.Num()!=N)
 {
  DepotWeightsAt[Team]=Elapsed;Weights.Init(1.f,N);
  // Side 1 is the east home, which belongs to the team that is not the swapped one (UBattleMatch::AreaHome).
  const int32 Side=Team!=int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap")))?1:0;
  int32 Own=0,Enemy=0;
  for(int32 I=0;I<N;++I)if(BattleFieldLayout::ObjectiveValue(Def,I)<=1.f){const int32 Holder=ObservedOwner(Team,I);Own+=Holder==Team;Enemy+=Holder==1-Team;}
  TArray<FVector> Known;bool bKnown=false;
  for(int32 I=0;I<N;++I)if(BattleFieldLayout::ObjectiveValue(Def,I)>1.f)
  {
   if(BattleFieldLayout::ObjectiveOwnerSide(Def,I)==Side)
   {
    if(Objectives[I].Owner!=Team){Weights[I]=DepotRetake;continue;}
    if(!bKnown){Known=RouteKnownEnemies(Team);bKnown=true;}
    bool bThreat=false;for(const FVector& K:Known)bThreat|=FVector::DistSquared2D(K,Objectives[I].Position)<=FMath::Square(15000.);
    Weights[I]=bThreat?DepotThreat:DepotGuard;
   }
   else Weights[I]=ObservedOwner(Team,I)==Team?DepotHold:Own>Enemy?Raid:FMath::Min(Raid,DepotRaidBehind);
  }
 }
 for(int32 I=0;I<N;++I)Intent.PointWeight[I]*=Weights[I];
}
