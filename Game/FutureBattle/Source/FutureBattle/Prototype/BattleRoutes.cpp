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

FVector UBattleMatch::RouteStep(const FVector& From,const FVector& Goal,int32 FlankSide,const TArray<FVector>& KnownEnemies,TArray<int32>* Load,int32 SquadID) const
{
 // Trial -BattleRouteLoad=<percent> (2026-11-25, default 0; user: "can the squads be led along routes so that they do not crowd"):
 // every squad took the shortest way over the roads, so a whole group walked the same road to the same doorway (map K: some 150
 // soldiers pushing at one point of the town entrance). With the switch each road section costs that many percent more for
 // every squad of the team already sent over it in this round of orders (Load, one count per section, filled in squad order),
 // so later squads take a parallel road once the nearest one is full enough to make the detour the shorter wait.
 static const double LoadShare=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleRouteLoad="),V);return FMath::Clamp(V,0,400)/100.;}();
 const bool bLoad=Load&&LoadShare>0.;
 // -BattleRouteKeep=1 (default 0): only the keeping of the chosen way (below), without the load; -BattleRouteLoad keeps it too.
 static const bool bRouteKeep=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleRouteKeep="),V);return V!=0;}();
 const bool bCommit=(bLoad||bRouteKeep)&&SquadID!=INDEX_NONE;
 static const bool bRouteLog=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleRouteLog="),V);return V!=0;}();
 // A way kept for another goal or another kind of order is forgotten first, also when this order needs no way at all.
 if(bCommit)if(const FRouteCommit* Old=RouteCommits.Find(SquadID);Old&&(FVector::Dist2D(Old->Goal,Goal)>3000.||Old->FlankSide!=FlankSide))RouteCommits.Remove(SquadID);
 if(bRouteLog&&SquadID!=INDEX_NONE)if(const FRouteCommit* OldSeen=RouteSeen.Find(SquadID);OldSeen&&(FVector::Dist2D(OldSeen->Goal,Goal)>3000.||OldSeen->FlankSide!=FlankSide))RouteSeen.Remove(SquadID);
 const auto& Def=BattleFieldLayout::Of(GetWorld());const auto& Nodes=Def.RouteNodes;const int32 N=Nodes.Num();
 // Near the goal the squad goes straight there (the last leg is the tactical layer's business).
 if(N<2||FVector::Dist2D(From,Goal)<15000){if(bCommit)RouteCommits.Remove(SquadID);return Goal;}
 auto Nearest=[&](const FVector& P){int32 Best=0;double BestD=DBL_MAX;for(int32 I=0;I<N;++I){const double D=FVector::DistSquared2D(P,Nodes[I]);if(D<BestD){BestD=D;Best=I;}}return Best;};
 const int32 Start=Nearest(From),End=Nearest(Goal);
 if(Start==End){if(bCommit)RouteCommits.Remove(SquadID);return Goal;}
 // Dijkstra over the few dozen junctions. FlankSide (+1 north, -1 south, 0 none) is set for squads sent against the enemy home:
 // the centre route costs three times its length, the other flank road twice, and each known enemy within 150m of a road
 // section adds a quarter (up to double).
 // The way once chosen is kept (-BattleRouteLoad; -BattleRouteKeep=1 alone. User 2026-11-25: squads have changed their way in the
 // middle of a road - map K 16-18 times a match, mostly because the nearest junction changes as the squad walks). Its junctions
 // are kept until the goal moves by more than 30m or the kind of order changes (checked above, before the short ways out);
 // junctions reached or passed are dropped, the last of them remembered (Passed); a squad that has left the road section it
 // walks by more than 60m, or is more than 250m from its first junction, chooses again. (Not saved: in-match saving is being removed, user 2026-11-25.)
 TArray<int32> Path;int32 PassedNode=INDEX_NONE;
 if(bCommit)
  if(FRouteCommit* Commit=RouteCommits.Find(SquadID))
  {
   while(Commit->Nodes.Num()&&(FVector::Dist2D(From,Nodes[Commit->Nodes[0]])<6000||(Commit->Nodes.Num()>1&&FVector::Dist2D(From,Nodes[Commit->Nodes[1]])<FVector::Dist2D(Nodes[Commit->Nodes[0]],Nodes[Commit->Nodes[1]]))))
   {Commit->Passed=Commit->Nodes[0];Commit->Nodes.RemoveAt(0);}
   bool bOnWay=Commit->Nodes.Num()>0;
   if(bOnWay)
   {
    const FVector NextAt=Nodes[Commit->Nodes[0]];
    bOnWay=Nodes.IsValidIndex(Commit->Passed)?FVector::Dist2D(From,FMath::ClosestPointOnSegment(FVector(From.X,From.Y,0),Nodes[Commit->Passed],NextAt))<=6000.:FVector::Dist2D(From,NextAt)<=25000.;
   }
   if(bOnWay){Path=Commit->Nodes;PassedNode=Commit->Passed;}else RouteCommits.Remove(SquadID);
  }
 if(Path.IsEmpty())
 {
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
   if(bLoad)if(const int32 EdgeIndex=int32(&E-Def.RouteEdges.GetData());Load->IsValidIndex(EdgeIndex))W*=1.+LoadShare*(*Load)[EdgeIndex];
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
 for(int32 I=End;I!=INDEX_NONE;I=Prev[I])Path.Insert(I,0);
 // Records only, -BattleRouteLog=1 (default 0): BATTLE_ROUTE_CHANGE when a squad with the same goal is sent to a way that leaves
 // out the junction it was walking to (junctions already reached or passed do not count).
 if(bRouteLog&&SquadID!=INDEX_NONE)
 {
  FRouteCommit& Seen=RouteSeen.FindOrAdd(SquadID);
  if(Seen.Nodes.Num())
  {
   while(Seen.Nodes.Num()&&(FVector::Dist2D(From,Nodes[Seen.Nodes[0]])<6000||(Seen.Nodes.Num()>1&&FVector::Dist2D(From,Nodes[Seen.Nodes[1]])<FVector::Dist2D(Nodes[Seen.Nodes[0]],Nodes[Seen.Nodes[1]]))))Seen.Nodes.RemoveAt(0);
   if(Seen.Nodes.Num()&&!Path.Contains(Seen.Nodes[0]))
    UE_LOG(LogTemp,Display,TEXT("BATTLE_ROUTE_CHANGE squad=%d at=(%.0f,%.0f) was=(%.0f,%.0f) now=(%.0f,%.0f) goal=(%.0f,%.0f) flank=%d"),SquadID,From.X,From.Y,Nodes[Seen.Nodes[0]].X,Nodes[Seen.Nodes[0]].Y,Path.Num()>1?Nodes[Path[1]].X:0.,Path.Num()>1?Nodes[Path[1]].Y:0.,Goal.X,Goal.Y,FlankSide);
  }
  Seen.Goal=Goal;Seen.FlankSide=FlankSide;Seen.Nodes=Path;
 }
 if(bCommit)
 {
  FRouteCommit NewCommit;NewCommit.Goal=Goal;NewCommit.FlankSide=FlankSide;NewCommit.Nodes=Path;RouteCommits.Add(SquadID,MoveTemp(NewCommit));
  UE_LOG(LogTemp,Display,TEXT("BATTLE_ROUTE_COMMIT squad=%d nodes=%d second=(%.0f,%.0f) goal=(%.0f,%.0f)"),SquadID,Path.Num(),Path.Num()>1?Nodes[Path[1]].X:0.,Path.Num()>1?Nodes[Path[1]].Y:0.,Goal.X,Goal.Y);
 }
 }
 if(bLoad)
 {
  // Every road section still ahead counts, the one the squad is walking included (from the junction passed last).
  if(Load->Num()!=Def.RouteEdges.Num())Load->Init(0,Def.RouteEdges.Num());
  auto CountSection=[&](int32 NodeA,int32 NodeB)
  {
   for(int32 EdgeIndex=0;EdgeIndex<Def.RouteEdges.Num();++EdgeIndex)
    if(const auto& PathEdge=Def.RouteEdges[EdgeIndex];(PathEdge.A==NodeA&&PathEdge.B==NodeB)||(PathEdge.B==NodeA&&PathEdge.A==NodeB)){++(*Load)[EdgeIndex];return;}
  };
  if(PassedNode!=INDEX_NONE&&Path.Num())CountSection(PassedNode,Path[0]);
  for(int32 I=0;I+1<Path.Num();++I)CountSection(Path[I],Path[I+1]);
 }
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
