// Player route choice (ROADMAP 2026-11-18, map F): on maps with a road network the player picks, per squad, the main route it takes
// (automatic, centre, north or south; V or the command bar button, for the selected squads).
//  - A manual order to a distant goal is walked in three parts: to the chosen route, along it to the junction nearest the goal,
//    then to the goal. The match hands the squad one junction at a time (UpdatePlayerRoutes) and the player's order last.
//  - A squad delegated to the AI keeps the choice: its AI goals are reached over the chosen route (UBattleMatch::UpdateTeamOrders).
//  - Automatic is the behaviour without a choice: a manual order goes straight to the goal, the AI picks its own way.
// The choice is the player's own decision about static map roads; it reads no enemy information. Not saved: after a load every
// squad is back on automatic and a routed manual order ends at the junction it was walking to.
// -BattlePlayerRouteTest (map F, headless): three squads of team 0 are ordered to the central objective over the three routes
// and BATTLE_PLAYER_ROUTE_RESULT reports whether each kept to its route.
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "NavigationSystem.h"
#include "Misc/CommandLine.h"

namespace
{
 // Squads of the self-test and what their leaders did (transient, test only).
 struct FRouteTestRow { int32 Squad=INDEX_NONE,Lane=0; double MaxY=-DBL_MAX,MinY=DBL_MAX,MinGoal=DBL_MAX; int32 Phase=0; };
 TArray<FRouteTestRow> RouteTestRows;
 int32 RouteTestState=0; // 0 not started, 1 running, 2 reported
 // A fourth squad walks a drawn way (BattlePlayerPath.cpp): a zigzag of four points; the nearest its leader came to each.
 int32 PathTestSquad=INDEX_NONE;TArray<FVector> PathTestPoints;TArray<double> PathTestNearest;
}

int32 UBattleMatch::PlayerRouteLane(int32 SquadID) const
{
 const FPlayerRoute* R=PlayerRoutes.Find(SquadID);return R?R->Lane:0;
}

void UBattleMatch::SetPlayerRouteLane(int32 SquadID,int32 Lane)
{
 FPlayerRoute& R=PlayerRoutes.FindOrAdd(SquadID);
 R.Lane=FMath::Clamp(Lane,0,3);R.Phase=0;R.PhaseGoal=FVector(FLT_MAX);
 // A routed order under way and delegated squads take the new choice at once.
 if(R.bMission)R.Step=FVector(FLT_MAX);
 PlayerRouteClock=1.f;DelegationClock=5.f;
}

void UBattleMatch::ClearPlayerRouteMission(int32 SquadID)
{
 if(FPlayerRoute* R=PlayerRoutes.Find(SquadID)){R->bMission=false;R->bPath=false;R->Waypoints.Reset();}
}

FVector UBattleMatch::PlayerRouteStep(int32 SquadID,const FVector& From,const FVector& Goal)
{
 static const TArray<FVector> None;
 FPlayerRoute* R=PlayerRoutes.Find(SquadID);
 if(!R||R->Lane<1||R->Lane>3)return RouteStep(From,Goal,0,None);
 const auto& Def=BattleFieldLayout::Of(GetWorld());const auto& Nodes=Def.RouteNodes;
 if(FVector::Dist2D(From,Goal)<15000)return Goal;
 // Junctions and sections of the chosen main route (ERouteLane: Centre, North, South in the order of the choice).
 const auto Lane=BattleFieldLayout::ERouteLane(R->Lane-1);
 TArray<int32> On;bool bOnRoad=false;
 for(const auto& E:Def.RouteEdges)if(E.Lane==Lane)
 {
  On.AddUnique(E.A);On.AddUnique(E.B);
  const FVector A(Nodes[E.A].X,Nodes[E.A].Y,0),B(Nodes[E.B].X,Nodes[E.B].Y,0);
  bOnRoad|=FMath::PointDistToSegmentSquared(FVector(From.X,From.Y,0),A,B)<FMath::Square(3000.);
 }
 if(On.Num()<2)return RouteStep(From,Goal,0,None);
 if(!R->PhaseGoal.Equals(Goal,1000)){R->Phase=0;R->PhaseGoal=Goal;}
 // Leaves the route at its junction nearest the goal; joins it where that adds least (to the junction, plus half the way along).
 int32 Exit=On[0],Entry=On[0];double Best=DBL_MAX;
 for(int32 I:On){const double D=FVector::DistSquared2D(Goal,Nodes[I]);if(D<Best){Best=D;Exit=I;}}
 Best=DBL_MAX;
 for(int32 I:On){const double D=FVector::Dist2D(From,Nodes[I])+.5*FVector::Dist2D(Nodes[I],Nodes[Exit]);if(D<Best){Best=D;Entry=I;}}
 if(R->Phase==0)
 {
  // No detour when the goal is nearer than the route itself.
  if(FVector::Dist2D(From,Goal)<=FVector::Dist2D(From,Nodes[Entry]))R->Phase=2;
  else if(bOnRoad||FVector::Dist2D(From,Nodes[Entry])<6000)R->Phase=1;
  else return RouteStep(From,Nodes[Entry],0,None);
 }
 if(R->Phase==1)
 {
  if(FVector::Dist2D(From,Nodes[Exit])<6000)R->Phase=2;
  else
  {
   // Along the route only: from its junction nearest the leader to the exit (breadth-first over the sections of the route).
   int32 Start=On[0];Best=DBL_MAX;for(int32 I:On){const double D=FVector::DistSquared2D(From,Nodes[I]);if(D<Best){Best=D;Start=I;}}
   TMap<int32,int32> Prev;TArray<int32> Open;Open.Add(Start);Prev.Add(Start,INDEX_NONE);
   for(int32 Head=0;Head<Open.Num()&&!Prev.Contains(Exit);++Head)
    for(const auto& E:Def.RouteEdges)if(E.Lane==Lane)
    {
     const int32 U=Open[Head],V=E.A==U?E.B:E.B==U?E.A:INDEX_NONE;
     if(V!=INDEX_NONE&&!Prev.Contains(V)){Prev.Add(V,U);Open.Add(V);}
    }
   TArray<int32> Path;if(Prev.Contains(Exit))for(int32 I=Exit;I!=INDEX_NONE;I=Prev[I])Path.Insert(I,0);
   // The next junction worth an order (the rule of RouteStep): not one the leader is at or already past.
   for(int32 I=0;I<Path.Num();++I)
   {
    const FVector& Here=Nodes[Path[I]];
    if(FVector::Dist2D(From,Here)<6000)continue;
    if(I+1<Path.Num()&&FVector::Dist2D(From,Nodes[Path[I+1]])<FVector::Dist2D(Here,Nodes[Path[I+1]]))continue;
    return Here;
   }
   R->Phase=2;
  }
 }
 return RouteStep(From,Goal,0,None);
}

int32 UBattleMatch::PlayerRouteOrder(int32 SquadID,EBattleOrder Order,const FVector& Goal)
{
 FPlayerRoute* R=PlayerRoutes.Find(SquadID);
 if(R){R->bMission=false;R->bPath=false;R->Waypoints.Reset();}
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const auto* S=Store->FindSquad(SquadID);
 if(!R||R->Lane==0||Order==EBattleOrder::Hold||!RoutesOn()||!Runtime||!Runtime->IsWideField()||!S||!Store->Find(S->LeaderUnitID))return -1;
 R->Phase=0;R->PhaseGoal=Goal;
 const FVector Step=PlayerRouteStep(SquadID,Runtime->Position(S->LeaderUnitID),Goal);
 if(Step.Equals(Goal,1))return -1; // near: an ordinary order
 // A goal off the walkable ground is refused now, not at the last junction.
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Projected;
 if(!Nav||!Nav->ProjectPointToNavigation(Goal,Projected,Runtime->IsVehicle(S->LeaderUnitID)?FVector(600,600,400):FVector(100,100,400)))return 0;
 if(!Runtime->IssueOrder(SquadID,Order==EBattleOrder::Move?EBattleOrder::Move:EBattleOrder::AttackMove,Step))return 0;
 R=PlayerRoutes.Find(SquadID);R->bMission=true;R->Order=Order;R->Goal=Goal;R->Step=Step;
 return 1;
}

void UBattleMatch::UpdatePlayerRoutes(float DeltaSeconds)
{
 static const bool bTest=FParse::Param(FCommandLine::Get(),TEXT("BattlePlayerRouteTest"));
 if(PlayerRoutes.IsEmpty()&&!bTest)return;
 PlayerRouteClock+=DeltaSeconds;if(PlayerRouteClock<1.f)return;
 PlayerRouteClock=0;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(!Runtime||!Runtime->IsWideField())return;
 // Self-test: three rifle squads of team 0, one per route, to the middle of the map.
 const FVector TestGoal=Objectives.IsValidIndex(Objectives.Num()/2)?Objectives[Objectives.Num()/2].Position:FVector::ZeroVector;
 if(bTest&&RouteTestState==0&&Elapsed>=2&&RoutesOn())
 {
  RouteTestState=1;RouteTestRows.Reset();
  TArray<int32> IDs;for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==0&&Pair.Value.SquadType==0&&Pair.Value.CommandStatus==EBattleCommandStatus::Active&&!Runtime->IsVehicle(Pair.Value.LeaderUnitID))IDs.Add(Pair.Key);
  IDs.Sort();
  for(int32 Lane=1;Lane<=3&&Lane<=IDs.Num();++Lane)
  {
   SetPlayerRouteLane(IDs[Lane-1],Lane);
   const int32 Result=PlayerRouteOrder(IDs[Lane-1],EBattleOrder::Move,TestGoal);
   FRouteTestRow Row;Row.Squad=IDs[Lane-1];Row.Lane=Lane;RouteTestRows.Add(Row);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_PLAYER_ROUTE_TEST start squad=%d lane=%d accepted=%d"),IDs[Lane-1],Lane,Result);
  }
  PathTestSquad=INDEX_NONE;PathTestPoints.Reset();PathTestNearest.Reset();
  if(IDs.Num()>3)if(const auto* S=Store->FindSquad(IDs[3]))
  {
   const FVector From=Runtime->Position(S->LeaderUnitID);
   for(const FVector2D Offset:{FVector2D(3500,4000),FVector2D(8000,-3500),FVector2D(12500,3500),FVector2D(17000,0)})PathTestPoints.Add(FVector(From.X+Offset.X,From.Y+Offset.Y,0));
   PathTestNearest.Init(DBL_MAX,PathTestPoints.Num());PathTestSquad=IDs[3];
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_PLAYER_PATH_TEST start squad=%d points=%d accepted=%d"),PathTestSquad,PathTestPoints.Num(),PlayerPathOrder(PathTestSquad,EBattleOrder::Move,PathTestPoints));
  }
  PlayerRouteClock=0;
 }
 for(auto It=PlayerRoutes.CreateIterator();It;++It)
 {
  FPlayerRoute& R=It.Value();
  const auto* S=Store->FindSquad(It.Key());
  if(!S){It.RemoveCurrent();continue;}
  if(!R.bMission)continue;
  const auto* Leader=Store->Find(S->LeaderUnitID);
  if(S->CommandStatus!=EBattleCommandStatus::Active||S->bPlayerDelegated||!Leader||!Leader->IsAlive()){R.bMission=false;continue;}
  const FVector LeaderAt=Runtime->Position(S->LeaderUnitID);
  FVector Step=R.Goal;
  // A drawn way hands on its next point; a route its next junction.
  if(R.bPath){if(!PlayerPathNext(R,LeaderAt,Step))continue;}
  else{if(R.Lane)Step=PlayerRouteStep(It.Key(),LeaderAt,R.Goal);if(Step.Equals(R.Step,1))continue;}
  const bool bFinal=R.bPath?R.Waypoints.IsEmpty():Step.Equals(R.Goal,1);
  // On the way the squad moves (or attack-moves); the player's own order is given with the goal.
  const EBattleOrder Order=bFinal?R.Order:R.Order==EBattleOrder::Move?EBattleOrder::Move:EBattleOrder::AttackMove;
  if(Runtime->IssueOrder(It.Key(),Order,Step))
  {
   R.Step=Step;R.StepDeadline=Elapsed+20.f+float(FVector::Dist2D(LeaderAt,Step)/250.);
   if(R.bPath&&R.Waypoints.Num())R.Waypoints.RemoveAt(0);
   if(bFinal){R.bMission=false;R.bPath=false;}
  }
  else if(R.bPath&&!bFinal)R.Waypoints.RemoveAt(0); // a point that takes no order is skipped
  else if(bFinal){R.bMission=false;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_PLAYER_ROUTE squad=%d final order refused"),It.Key());}
 }
 if(bTest&&RouteTestState==1)
 {
  for(auto& Row:RouteTestRows)if(const auto* S=Store->FindSquad(Row.Squad))if(const auto* Leader=Store->Find(S->LeaderUnitID);Leader&&Leader->IsAlive())
  {
   const FVector P=Runtime->Position(S->LeaderUnitID);
   Row.MaxY=FMath::Max(Row.MaxY,P.Y);Row.MinY=FMath::Min(Row.MinY,P.Y);Row.MinGoal=FMath::Min(Row.MinGoal,FVector::Dist2D(P,TestGoal));
   if(const FPlayerRoute* R=PlayerRoutes.Find(Row.Squad))Row.Phase=FMath::Max(Row.Phase,R->Phase);
  }
  if(const auto* S=Store->FindSquad(PathTestSquad))if(const auto* Leader=Store->Find(S->LeaderUnitID);Leader&&Leader->IsAlive())
   for(int32 I=0;I<PathTestPoints.Num();++I)PathTestNearest[I]=FMath::Min(PathTestNearest[I],FVector::Dist2D(Runtime->Position(S->LeaderUnitID),PathTestPoints[I]));
  if(Elapsed>=420)
  {
   RouteTestState=2;int32 Failures=RouteTestRows.Num()==3?0:1;
   {
    // Drawn way: every point visited (within 25m), in the order drawn (the mission is over).
    int32 Reached=0;FString Nearest;
    for(double D:PathTestNearest){Reached+=D<2500;Nearest+=FString::Printf(TEXT("%s%.0f"),Nearest.IsEmpty()?TEXT(""):TEXT("/"),D/100);}
    const FPlayerRoute* R=PlayerRoutes.Find(PathTestSquad);const bool bDone=R&&!R->bMission;
    const bool bOK=PathTestPoints.Num()==4&&Reached==4&&bDone;Failures+=!bOK;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_PLAYER_PATH_TEST squad=%d points=%d reached=%d nearest_m=%s finished=%d ok=%d"),PathTestSquad,PathTestPoints.Num(),Reached,*Nearest,bDone?1:0,bOK?1:0);
   }
   for(const auto& Row:RouteTestRows)
   {
    // Centre: never out on a flank road. North/south: reached its flank road (y = +-250m on map F) and never the other one.
    const bool bKept=Row.Lane==1?Row.MaxY<15000&&Row.MinY>-15000:Row.Lane==2?Row.MaxY>22000&&Row.MinY>-15000:Row.MinY<-22000&&Row.MaxY<15000;
    const bool bArrived=Row.MinGoal<15000;
    Failures+=!bKept;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_PLAYER_ROUTE_TEST squad=%d lane=%d max_y_m=%.0f min_y_m=%.0f min_goal_m=%.0f phase=%d kept=%d arrived=%d"),Row.Squad,Row.Lane,Row.MaxY/100,Row.MinY/100,Row.MinGoal/100,Row.Phase,bKept?1:0,bArrived?1:0);
   }
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_PLAYER_ROUTE_RESULT failures=%d"),Failures);
  }
 }
}
