#include "BattleLeaderRuntime.h"
#include "BattleSquadTactics.h"
#include "BattleGameMode.h"
#include "BattleFieldLayout.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

// B05 self-test (-BattleOwnRouteSmoke on an urban area map, e.g. map K; Docs/review-2026-11-25, Setup/Test-OwnRoute.ps1).
// A fixed scene: one squad stands on both sides of a wall and is sent to a place beyond it. The members behind the wall are cut
// off from the shared route and get routes of their own (StableMove). The test forces squad replans while they walk and checks
// that, with -BattleCutOffRoute=2, the own routes survive them, the members arrive, a new order drops the own routes, and the
// own-route queries stay few. Scene 0 uses a plain move order, scene 1 a supply trip (a temporary Recover plan).
// With -BattleCutOffRoute=0 the "survives a squad replan" checks fail: that is the defect B05 describes.
// The test moves soldiers and gives orders, so it changes the match (test runs only).
namespace
{
 struct FOwnRouteSmoke
 {
  int32 Stage=0,Scene=0,SID=INDEX_NONE,Failures=0;
  bool bSite=false,bDone=false;
  double StageAt=0,Limit=0,NextForce=0,Detour=0;
  FVector East=FVector::ZeroVector,West=FVector::ZeroVector,Dest=FVector::ZeroVector;
  TArray<int32> Cut;                  // members placed behind the wall
  TMap<int32,bool> Had;               // had an own route last frame
  TMap<int32,int32> QueriesAtStart;
  int32 Lost=0,Gained=0,CheckedReplans=0,ForcedReplans=0,UsedSquads=0;
  // A forced replan that has not run yet: the plan's request count when it was asked, and the own routes at that time.
  bool bPending=false;int32 PendingRequests=0;TMap<int32,FVector> PendingEnds;
  int32 OrderRequests=0;
 };
 FOwnRouteSmoke GOwnRouteSmoke;
}

void UBattleLeaderRuntime::StepOwnRouteSmoke(double Now)
{
 auto& T=GOwnRouteSmoke;
 if(T.bDone || Now<Mode->CombatStartsAt+3.)return;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 int32 Mode2=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCutOffRoute="),Mode2);
 auto Check=[&](bool bOk,const FString& What){if(!bOk)++T.Failures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_OWN_ROUTE_SMOKE %s scene=%d %s"),bOk?TEXT("PASS"):TEXT("FAIL"),T.Scene,*What);};
 auto Finish=[&]()
 {
  T.bDone=true;SmokeFailures+=T.Failures;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_OWN_ROUTE_SMOKE_RESULT failures=%d mode=%d scenes=%d"),T.Failures,Mode2,T.Scene);
  FPlatformMisc::RequestExitWithStatus(false,T.Failures?1:0);
 };
 auto Place=[&](int32 ID,FVector At)
 {
  auto& Body=Bodies.FindChecked(ID);
  if(Body.Actor.IsValid()){Body.Actor->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);Body.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else{Body.Capsule->SetWorldLocation(At);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Body.Entity).Position=At;}
 };
 auto OnMesh=[&](FVector At,FVector& Out){FNavLocation Loc;if(!Nav||!Nav->ProjectPointToNavigation(At,Loc,FVector(30,30,300))||FVector::Dist2D(Loc.Location,At)>30.)return false;Out=Loc.Location;return true;};
 auto Walk=[&](FVector From,FVector To,double& Length)
 {auto* Path=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),From,To):nullptr;if(!Path||!Path->IsValid()||Path->IsPartial()||Path->PathPoints.Num()<2)return false;Length=Path->GetPathLength();return true;};
 if(T.Stage==0)
 {
  if(!T.bSite)
  {
   // A wall with open ground on both sides and a way round of 12 to 90 m: first the place seen on map K (STATUS 2026-11-25,
   // squad 19 at the x=0 wall), then a scan of the map.
   auto Site=[&](FVector E,FVector W)
   {
    FVector EM,WM,DM;double Round=0,Onward=0;
    if(!OnMesh(E,EM)||!OnMesh(W,WM)||ClearMovementSegment(EM+FVector(0,0,90),WM+FVector(0,0,90))||!Walk(EM,WM,Round)||Round<1200.||Round>9000.)return false;
    for(const FVector Off:{FVector(-4000,0,0),FVector(-3000,1500,0),FVector(-3000,-1500,0),FVector(-2000,0,0)})
     if(FNavLocation Loc;Nav->ProjectPointToNavigation(W+Off,Loc,FVector(300,300,300))&&Walk(WM,Loc.Location,Onward)&&Onward<9000.)
     {DM=Loc.Location;T.East=EM;T.West=WM;T.Dest=DM;T.Detour=Round+Onward;return true;}
    return false;
   };
   T.bSite=Site(FVector(150,-3150,10),FVector(-150,-3150,10));
   for(double X=-12000.;!T.bSite&&X<=12000.;X+=150.)for(double Y=-9000.;!T.bSite&&Y<=9000.;Y+=450.)T.bSite=Site(FVector(X+150,Y,10),FVector(X-150,Y,10));
   Check(T.bSite&&BattleFieldLayout::IsUrban(GetWorld()),FString::Printf(TEXT("scene: a wall with a way round (east=%s west=%s dest=%s walk=%.0f)"),*T.East.ToString(),*T.West.ToString(),*T.Dest.ToString(),T.Detour));
   if(!T.bSite){Finish();return;}
  }
  // A living rifle squad on foot of team 0 with at least six members, not used by an earlier scene.
  T.SID=INDEX_NONE;int32 Skip=T.UsedSquads;
  TArray<int32> SquadIDs;for(const auto& Pair:Store->GetSquads())SquadIDs.Add(Pair.Key);SquadIDs.Sort();
  for(int32 Candidate:SquadIDs)
  {
   const auto* CS=Store->FindSquad(Candidate);
   if(!CS||CS->Team!=0||CS->CommandStatus!=EBattleCommandStatus::Active||CS->SquadType!=BattleSquadType::Rifle||IsVehicle(CS->LeaderUnitID))continue;
   int32 Living=0;for(int32 MID:CS->Members)if(const auto* MU=Store->Find(MID);MU&&MU->IsAlive()&&Bodies.Contains(MID))++Living;
   if(Living<6||!Store->Find(CS->LeaderUnitID)||!Store->Find(CS->LeaderUnitID)->IsAlive())continue;
   if(Skip-->0)continue;
   T.SID=Candidate;break;
  }
  if(T.SID==INDEX_NONE){Check(false,TEXT("scene: a rifle squad of six or more"));Finish();return;}
  ++T.UsedSquads;
  const auto* S=Store->FindSquad(T.SID);
  // Three members behind the wall (east), the leader and the rest in front of it (west), so the squad centre is in front.
  T.Cut.Reset();T.Had.Reset();T.QueriesAtStart.Reset();T.Lost=T.Gained=T.CheckedReplans=T.ForcedReplans=0;T.bPending=false;
  TArray<FVector> EastSpots,WestSpots;
  for(int32 Row=0;Row<4;++Row)for(int32 Col=-2;Col<=2;++Col)
  {
   FVector Spot;
   if(OnMesh(T.East+FVector(40+Row*110,Col*110,0),Spot)&&ClearMovementSegment(Spot+FVector(0,0,90),T.East+FVector(0,0,90)))EastSpots.Add(Spot);
   if(OnMesh(T.West-FVector(250+Row*110,Col*110,0),Spot)&&ClearMovementSegment(Spot+FVector(0,0,90),T.West+FVector(0,0,90)))WestSpots.Add(Spot);
  }
  int32 EastUsed=0,WestUsed=0;bool bPlaced=true;
  for(int32 MID:S->Members)if(const auto* MU=Store->Find(MID);MU&&MU->IsAlive()&&Bodies.Contains(MID))
  {
   const bool bBehind=MID!=S->LeaderUnitID&&T.Cut.Num()<3;
   auto& Spots=bBehind?EastSpots:WestSpots;int32& Used=bBehind?EastUsed:WestUsed;
   if(!Spots.IsValidIndex(Used)){bPlaced=false;break;}
   Place(MID,Spots[Used++]+FVector(0,0,90));
   if(bBehind)T.Cut.Add(MID);
  }
  Check(bPlaced&&T.Cut.Num()==3,TEXT("scene: the squad stands on both sides of the wall"));
  if(!bPlaced){Finish();return;}
  for(int32 MID:T.Cut){T.Had.Add(MID,false);T.QueriesAtStart.Add(MID,Bodies.FindChecked(MID).OwnRouteQueries);Bodies.FindChecked(MID).bOwnRoute=false;}
  bool bOrdered=false;
  if(T.Scene==0)bOrdered=IssueOrder(T.SID,EBattleOrder::Move,T.Dest);
  else bOrdered=Tactics&&Tactics->Apply(T.SID,EBattleSquadTactic::Recover,EBattleOrder::Move,T.Dest,TEXT("HOME SUPPLY / SMOKE"));
  Check(bOrdered&&FVector::Dist2D(S->Destination,T.Dest)<300.,T.Scene==0?TEXT("move order accepted"):TEXT("supply trip accepted"));
  T.Dest=S->Destination;
  T.StageAt=Now;T.NextForce=Now+6.;T.Limit=Now+25.+2.5*T.Detour/FMath::Max(1.f,GetDefault<UBattleTuning>()->MoveSpeed);T.Stage=1;
  return;
 }
 const auto* S=Store->FindSquad(T.SID);auto* P=Plans.Find(T.SID);
 if(!S||!P){Check(false,TEXT("the squad exists"));Finish();return;}
 if(T.Stage==1)
 {
  const bool bSameOrder=S->Order==EBattleOrder::Move&&S->Destination.Equals(T.Dest,1.);
  // Own routes gained and lost, frame by frame.
  int32 Arrived=0,Living=0,Own=0;
  for(int32 MID:T.Cut)if(const auto* MU=Store->Find(MID);MU&&MU->IsAlive())
  {
   const auto& Body=Bodies.FindChecked(MID);++Living;
   if(Body.bOwnRoute&&!T.Had[MID])++T.Gained;
   if(!Body.bOwnRoute&&T.Had[MID]&&bSameOrder)
   {++T.Lost;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_OWN_ROUTE_SMOKE_LOST scene=%d unit=%d t=%.1f pos=%s"),T.Scene,MID,Now-T.StageAt,*Position(MID).ToString());}
   T.Had[MID]=Body.bOwnRoute;Own+=Body.bOwnRoute;
   if(const FVector* Goal=P->ArrivalGoals.Find(MID);Goal&&FVector::Dist2D(Position(MID),*Goal)<300.)++Arrived;
  }
  // A forced squad replan has run: every own route of that moment is still there and ends where it ended.
  if(T.bPending&&P->PathRequests!=T.PendingRequests)
  {
   T.bPending=false;int32 Kept=0;
   for(const auto& End:T.PendingEnds)
   {
    const auto* Route=P->Corridors.Find(End.Key);
    if(Bodies.FindChecked(End.Key).bOwnRoute&&Route&&Route->Num()>1&&Route->Last().Equals(End.Value,1.))++Kept;
   }
   ++T.CheckedReplans;
   Check(Kept==T.PendingEnds.Num(),FString::Printf(TEXT("own routes survive a squad replan (%d of %d, t=%.1f)"),Kept,T.PendingEnds.Num(),Now-T.StageAt));
  }
  // Every 3 s while a cut-off member walks a route of his own: ask for a squad replan, as a stalled member does.
  if(!T.bPending&&Now>=T.NextForce&&bSameOrder&&Arrived<Living)
  {
   T.NextForce=Now+3.;T.PendingEnds.Reset();
   for(int32 MID:T.Cut)if(const auto* Route=P->Corridors.Find(MID);Route&&Route->Num()>1&&Bodies.FindChecked(MID).bOwnRoute&&Store->Find(MID)->IsAlive())T.PendingEnds.Add(MID,Route->Last());
   if(T.PendingEnds.Num()){T.bPending=true;T.PendingRequests=P->PathRequests;P->bReplan=true;P->NextPath=0;++T.ForcedReplans;}
  }
  const bool bAll=Living>0&&Arrived==Living;
  if(!bAll&&Now<T.Limit)return;
  Check(bSameOrder,TEXT("the order stayed in force during the walk"));
  Check(bAll,FString::Printf(TEXT("the cut-off members reach their places (%d of %d in %.1f s, limit %.1f s)"),Arrived,Living,Now-T.StageAt,T.Limit-T.StageAt));
  Check(T.Gained>0,FString::Printf(TEXT("cut-off members get routes of their own (%d)"),T.Gained));
  Check(T.CheckedReplans>0,FString::Printf(TEXT("squad replans were forced during the walk (%d checked of %d asked)"),T.CheckedReplans,T.ForcedReplans));
  Check(T.Lost==0,FString::Printf(TEXT("no own route is lost while the order holds (%d lost)"),T.Lost));
  int32 Queries=0;for(int32 MID:T.Cut)Queries+=Bodies.FindChecked(MID).OwnRouteQueries-T.QueriesAtStart[MID];
  Check(Queries<=T.Cut.Num()*4,FString::Printf(TEXT("own-route queries stay few (%d for %d members)"),Queries,T.Cut.Num()));
  // A new order to another place: the own routes are dropped and the squad plans again.
  FNavLocation Other;const bool bOther=Nav&&Nav->ProjectPointToNavigation(T.Dest+FVector(0,2000,0),Other,FVector(400,400,300));
  for(int32 MID:T.Cut)if(Store->Find(MID)->IsAlive()){auto& Body=Bodies.FindChecked(MID);if(!Body.bOwnRoute){Body.bOwnRoute=true;Body.OwnRouteOrder=uint8(S->Order);Body.OwnRouteDestination=S->Destination;}}
  T.OrderRequests=P->PathRequests;
  Check(bOther&&IssueOrder(T.SID,EBattleOrder::Move,Other.Location),TEXT("a new order to another place is accepted"));
  T.Dest=S->Destination;T.StageAt=Now;T.Limit=Now+5.;T.Stage=2;
  return;
 }
 if(T.Stage==2)
 {
  if(P->PathRequests==T.OrderRequests&&Now<T.Limit)return;
  int32 StillOwn=0,Planned=0,Living=0;
  for(int32 MID:T.Cut)if(Store->Find(MID)->IsAlive())
  {
   ++Living;StillOwn+=Bodies.FindChecked(MID).bOwnRoute;
   if(const auto* Route=P->Corridors.Find(MID);Route&&Route->Num()>1&&FVector::Dist2D(Route->Last(),T.Dest)<1500.)++Planned;
  }
  Check(P->PathRequests!=T.OrderRequests&&StillOwn==0&&Planned==Living,FString::Printf(TEXT("a new order drops the own routes and plans again (own=%d planned=%d of %d)"),StillOwn,Planned,Living));
  if(++T.Scene<2){T.Stage=0;return;}
  Finish();
 }
}
