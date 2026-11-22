// Routes (ROADMAP 2026-11-18): on maps that define a road network (FMapDef::RouteNodes/RouteEdges; map F), a squad ordered to a
// distant goal walks it road by road instead of along the single shortest path: the team AI hands it the next junction, then the
// one after, and the goal itself once it is near. Three main routes (centre avenue, north and south flank roads) with connectors.
//  - Squads bound for an objective take the shortest way over the network.
//  - Squads sent against the enemy home keep off the centre and take the flank road with fewer enemies their own team currently
//    sees near it (observed information only), choosing while still near their own home and then staying on that side.
// -BattleRoutes=0 turns it off (straight to the goal, as on the other maps); default on where the map defines routes.
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool UBattleMatch::RoutesOn() const
{
 // -1 = not passed: on where the map defines routes.
 static const int32 Mode=[]{int32 Routes=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleRoutes="),Routes);return Routes;}();
 return bArea&&Mode!=0&&BattleFieldLayout::Of(GetWorld()).RouteNodes.Num()>1;
}

TArray<FVector> UBattleMatch::RouteKnownEnemies(int32 Team) const
{
 // What the team's soldiers see now (the same source as AreaFronts): positions only, no hidden state.
 TMap<int32,FVector> Seen;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 if(Perception)for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())
  if(const auto* Contacts=Perception->GetContacts(U.Key))
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!IsScoutLongContact(U.Key,C.Value))Seen.Add(C.Key,C.Value.LastKnownPosition);
 AddScoutReportSightings(Team,Seen);
 TArray<FVector> Out;Seen.GenerateValueArray(Out);return Out;
}

int32 UBattleMatch::RouteFlankSide(const FVector& From,const FVector& OwnHome,const TArray<FVector>& KnownEnemies,int32 SquadID) const
{
 // Already out on a flank: stay there. Otherwise the side with fewer known enemies beyond the city edge; ties by squad number.
 const auto& Def=BattleFieldLayout::Of(GetWorld());const double Edge=Def.CityHalf.Y>0?Def.CityHalf.Y:Def.HalfExtent.Y*.4;
 if(FMath::Abs(From.Y)>Edge*.7&&FVector::Dist2D(From,OwnHome)>8000)return From.Y>0?1:-1;
 int32 North=0,South=0;for(const FVector& E:KnownEnemies){North+=E.Y>Edge;South+=E.Y<-Edge;}
 if(North!=South)return North<South?1:-1;
 // Trial -BattleRouteTieSymmetric=1 (default 0; sector lane's note 2026-11-22): the tie went by the squad number for both teams, so on
 // the 180-degree maps both took the same absolute side and their squad numbers differ. With the symmetric start the tie goes by the
 // team's own allocation index and is mirrored for the east team, so both teams split their home assaults the same way.
 static const bool bTieSymmetric=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleRouteTieSymmetric="),V);return V!=0;}();
 if(bTieSymmetric&&BattleTactics::SymmetricStart(GetWorld()))
 {
  const int32 Team=FVector::DistSquared2D(OwnHome,AreaHome(0))<=FVector::DistSquared2D(OwnHome,AreaHome(1))?0:1;
  const int32 Side=(BattleTactics::AllocationIndex(GetWorld(),SquadID,Team)/10)%2?1:-1;
  return OwnHome.X>0?-Side:Side;
 }
 return (SquadID/10)%2?1:-1;
}

FVector UBattleMatch::RouteStep(const FVector& From,const FVector& Goal,int32 FlankSide,const TArray<FVector>& KnownEnemies) const
{
 const auto& Def=BattleFieldLayout::Of(GetWorld());const auto& Nodes=Def.RouteNodes;const int32 N=Nodes.Num();
 // Near the goal the squad goes straight there (the last leg is the tactical layer's business).
 if(N<2||FVector::Dist2D(From,Goal)<15000)return Goal;
 auto Nearest=[&](const FVector& P){int32 Best=0;double BestD=DBL_MAX;for(int32 I=0;I<N;++I){const double D=FVector::DistSquared2D(P,Nodes[I]);if(D<BestD){BestD=D;Best=I;}}return Best;};
 const int32 Start=Nearest(From),End=Nearest(Goal);
 if(Start==End)return Goal;
 // Dijkstra over the few dozen junctions. FlankSide (+1 north, -1 south, 0 none) is set for squads sent against the enemy home:
 // the centre route costs three times its length, the other flank road twice, and each known enemy within 150m of a road
 // section adds a quarter (up to double).
 TArray<double> Cost;Cost.Init(DBL_MAX,N);TArray<int32> Prev;Prev.Init(INDEX_NONE,N);TArray<bool> Done;Done.Init(false,N);
 Cost[Start]=0;
 for(;;)
 {
  int32 U=INDEX_NONE;for(int32 I=0;I<N;++I)if(!Done[I]&&Cost[I]<DBL_MAX&&(U==INDEX_NONE||Cost[I]<Cost[U]))U=I;
  if(U==INDEX_NONE||U==End)break;
  Done[U]=true;
  for(const auto& E:Def.RouteEdges)
  {
   const int32 V=E.A==U?E.B:E.B==U?E.A:INDEX_NONE;if(V==INDEX_NONE||Done[V])continue;
   double W=FVector::Dist2D(Nodes[U],Nodes[V]);
   if(FlankSide)
   {
    if(E.Lane==BattleFieldLayout::ERouteLane::Centre)W*=3;
    else if(E.Lane==(FlankSide>0?BattleFieldLayout::ERouteLane::South:BattleFieldLayout::ERouteLane::North))W*=2;
    const FVector Mid=(Nodes[U]+Nodes[V])*.5;int32 Near=0;for(const FVector& K:KnownEnemies)Near+=FVector::DistSquared2D(K,Mid)<FMath::Square(15000.);
    W*=1.+.25*FMath::Min(4,Near);
   }
   if(Cost[U]+W<Cost[V]){Cost[V]=Cost[U]+W;Prev[V]=U;}
  }
 }
 if(Prev[End]==INDEX_NONE)return Goal;
 TArray<int32> Path;for(int32 I=End;I!=INDEX_NONE;I=Prev[I])Path.Insert(I,0);
 // The next junction worth an order: not one the leader is already at or past (within 60m, or the next one is nearer to the
 // leader than to this one), so the squad keeps walking instead of stopping at every junction.
 for(int32 I=0;I<Path.Num();++I)
 {
  const FVector& Here=Nodes[Path[I]];
  if(FVector::Dist2D(From,Here)<6000)continue;
  if(I+1<Path.Num()&&FVector::Dist2D(From,Nodes[Path[I+1]])<FVector::Dist2D(Here,Nodes[Path[I+1]]))continue;
  // The last junction is skipped when the goal is closer to the leader than that junction is: no walking past the goal and back.
  if(I==Path.Num()-1&&FVector::Dist2D(From,Goal)<=FVector::Dist2D(From,Here))break;
  return Here;
 }
 return Goal;
}
