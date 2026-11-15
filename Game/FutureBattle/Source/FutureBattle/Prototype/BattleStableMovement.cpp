#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "BattleCover.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

namespace
{
void SweepSoldier(UWorld* World,const UPrimitiveComponent* Ignore,FVector From,FVector Delta,FHitResult& Result,float HalfHeight=90)
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleMassMovement)); Query.AddIgnoredComponent(Ignore);
 TArray<FHitResult> Hits;
 World->SweepMultiByChannel(Hits,From,From+Delta,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,HalfHeight),Query);
 Result=FHitResult();
 for(const auto& Candidate:Hits) if(Candidate.bBlockingHit)
 {
  // Match UE movement's handling of initial contacts when moving away from them.
  if(Candidate.bStartPenetrating && FVector::DotProduct(Delta,Candidate.ImpactNormal)>0) continue;
  Result=Candidate;break;
 }
}
}

bool UBattleLeaderRuntime::ClearMovementSegment(FVector A,FVector B) const
{
 // This runtime currently uses the flat wide field. Query static geometry, not nearby soldiers.
 A.Z=FMath::Max(A.Z,100.); B.Z=FMath::Max(B.Z,100.);
 return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,75));
}

void UBattleLeaderRuntime::BuildMovementPlan(int32 SID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID); auto& P=Plans.FindChecked(SID);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 const bool NewOrder=P.ArrivalRevision!=S->OrderRevision;
 if(NewOrder || P.ArrivalGoals.IsEmpty())
 {
  P.ArrivalGoals.Reset(); P.ArrivalTails.Reset(); P.ArrivalRevision=S->OrderRevision;
  TArray<FVector> Reserved;
  for(const auto& Other:Plans) if(Other.Key!=SID)
   if(const auto* OS=Store->FindSquad(Other.Key); OS && OS->Team==S->Team && OS->Order!=EBattleOrder::Hold && OS->CommandStatus==EBattleCommandStatus::Active)
    for(const auto& G:Other.Value.ArrivalGoals) if(Store->Find(G.Key)->IsAlive()) Reserved.Add(G.Value);
  const FVector Forward=P.Path.Num()>1?(P.Path.Last()-P.Path[P.Path.Num()-2]).GetSafeNormal2D():FVector::ForwardVector;
  const FVector Right(-Forward.Y,Forward.X,0);
  const bool Rally=S->TacticalPlan.bAutonomous &&
   (S->TacticalPlan.Action==EBattleSquadTactic::Retreat || S->TacticalPlan.Action==EBattleSquadTactic::Regroup || S->TacticalPlan.Action==EBattleSquadTactic::Recover || S->TacticalPlan.Action==EBattleSquadTactic::Flank);
  TArray<FVector> Candidates;
  for(int32 Row=-12;Row<=12;++Row) for(int32 Col=-12;Col<=12;++Col)
  {
   const FVector Offset=Forward*(Row*200)+Right*(Col*200);
   // Converging squads fill their own approach half-plane rather than crossing
   // the rally point to claim a slot behind the opposing incoming stream.
   if(FVector::DotProduct(Offset,Forward)>20) continue;
   if(Rally && Offset.Size2D()>450) continue;
   if(S->Order==EBattleOrder::Defend && Offset.Size2D()>S->DefenseRadius-80) continue;
   Candidates.Add(S->Destination+Offset);
  }
  // Fill from the approach side, so converging squads don't seek slots beyond each other.
  auto Score=[&](FVector V){return FMath::FloorToDouble(FVector::Dist2D(V,S->Destination)/450.)*100000.+FVector::Dist2D(V,P.Center);};
  Candidates.Sort([&](const FVector& A,const FVector& B){const double SA=Score(A),SB=Score(B);return SA!=SB?SA<SB:A.X!=B.X?A.X<B.X:A.Y<B.Y;});
  // Map E: arrive behind cover. The grid below only accepts spots the destination reaches in a straight line, which is
  // exactly what excludes the far side of a car or a wall. So most members first try cover slots near the destination that
  // are shielded from the direction of advance, and walk there along a short route of their own. The first three (never
  // the leader) still take open spots on the objective, so that it can be captured and held.
  // -BattleCoverArrival=0 turns it off for comparison.
  static const bool CoverArrivalAllowed=[](){int32 Value=1;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverArrival="),Value);return Value!=0;}();
  const bool CoverArrival=CoverArrivalAllowed && BattleFieldLayout::IsUrban(GetWorld()) && !Rally && Nav;
  TArray<FVector> CoverCandidates; TSet<int32> CoverAssigned;
  if(CoverArrival)
  {
   auto* Cover=GetWorld()->GetSubsystem<UBattleCover>(); Cover->BuildSlots();
   for(int32 Slot:Cover->NearbySlots(S->Destination,1400.f,48))
   {
    const FVector At=Cover->GetSlots()[Slot];
    if(GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,100),At+Forward*250+FVector(0,0,100),FCollisionObjectQueryParams(ECC_WorldStatic))) CoverCandidates.Add(At);
   }
  }
  int32 Queries=0, OpenMembers=0, TailQueries=0;
  for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive())
  {
   bool Found=false;
   if(CoverArrival && (ID==S->LeaderUnitID || OpenMembers++>=3))
    for(const FVector& At:CoverCandidates)
    {
     if(TailQueries>=24) break;
     bool Taken=false; for(const auto& R:Reserved) if(FVector::DistSquared2D(R,At)<FMath::Square(190.)) {Taken=true;break;}
     if(Taken) continue;
     ++TailQueries;
     auto* Tail=Nav->FindPathToLocationSynchronously(GetWorld(),P.Path.Num()?P.Path.Last():S->Destination,At);
     if(!Tail || !Tail->IsValid() || Tail->IsPartial() || Tail->PathPoints.Num()<2 || Tail->GetPathLength()>2500) continue;
     P.ArrivalGoals.Add(ID,At); P.ArrivalTails.Add(ID,Tail->PathPoints); Reserved.Add(At); CoverAssigned.Add(ID); Found=true;
     SightExperiment.Cover(TEXT("arrival_cover_slot"));
     break;
    }
   // Bounded shared allocation. Each living member reserves a distinct destination.
   for(const FVector& Candidate:Candidates)
     {
      if(Found || Queries>=80) break;
      if(Rally && ID==S->LeaderUnitID && FVector::Dist2D(Candidate,S->Destination)>250) continue;
      bool Occupied=false; for(const auto& R:Reserved) if(FVector::DistSquared2D(R,Candidate)<FMath::Square(190.)) {Occupied=true;break;}
      if(Occupied) continue;
      ++Queries; FNavLocation Projected;
      if(!Nav || !Nav->ProjectPointToNavigation(Candidate,Projected,FVector(30,30,200))) continue;
      TArray<FVector> OpenTail;
      if(!ClearMovementSegment(S->Destination,Projected.Location))
      {
       // Map E: clutter (low walls, cars) breaks the straight line to most spots around a destination. Accept a spot that a
       // short walk reaches instead of rejecting it; without this, squads were left with no free arrival spot and re-planned
       // every second.
       if(!CoverArrival || TailQueries>=40) continue;
       ++TailQueries;
       const FVector PathEnd=P.Path.Num()?FVector(P.Path.Last()):S->Destination;
       auto* Tail=Nav->FindPathToLocationSynchronously(GetWorld(),PathEnd,Projected.Location);
       if(!Tail || !Tail->IsValid() || Tail->IsPartial() || Tail->PathPoints.Num()<2 || Tail->GetPathLength()>FVector::Dist2D(PathEnd,Projected.Location)*2.+400.) continue;
       OpenTail=Tail->PathPoints;
      }
      if(Rally && FVector::Dist2D(Projected.Location,S->Destination)>450) continue;
      if(Rally && ID==S->LeaderUnitID && FVector::Dist2D(Projected.Location,S->Destination)>250) continue;
      for(const auto& R:Reserved) if(FVector::DistSquared2D(R,Projected.Location)<FMath::Square(190.)) {Occupied=true;break;}
      if(Occupied) continue;
      P.ArrivalGoals.Add(ID,Projected.Location); Reserved.Add(Projected.Location); Found=true;
      if(OpenTail.Num()>1) {P.ArrivalTails.Add(ID,OpenTail); CoverAssigned.Add(ID);}
      if(CoverArrival) SightExperiment.Cover(TEXT("arrival_open_slot"));
     }
   // Map E: never leave a member without an arrival spot (that forced a full reassignment every second). As a last resort
   // he walks to the end of the shared path and local avoidance sorts out the crowding.
   if(CoverArrival && !Found)
   {
    P.ArrivalGoals.Add(ID,P.Path.Num()?FVector(P.Path.Last()):S->Destination); CoverAssigned.Add(ID);
    SightExperiment.Cover(TEXT("arrival_fallback"));
   }
  }
  // Preserve front-to-back ordering: a front row must not stop on the rear row's route.
  TArray<int32> Members;TArray<FVector> Goals;
  // Members sent to a cover slot keep it: their own route leads to that exact spot.
  for(const auto& Pair:P.ArrivalGoals) if(!CoverAssigned.Contains(Pair.Key)) {Members.Add(Pair.Key);Goals.Add(Pair.Value);}
  auto Key=[&](FVector V){return FMath::RoundToDouble(FVector::DotProduct(V-S->Destination,Forward)/80.);};
  Members.Sort([&](int32 A,int32 B){const double KA=Key(Position(A)),KB=Key(Position(B));return KA!=KB?KA>KB:FVector::DotProduct(Position(A),Right)<FVector::DotProduct(Position(B),Right);});
  Goals.Sort([&](const FVector& A,const FVector& B){const double KA=Key(A),KB=Key(B);return KA!=KB?KA>KB:FVector::DotProduct(A,Right)<FVector::DotProduct(B,Right);});
  for(int32 I=0;I<Members.Num();++I) P.ArrivalGoals[Members[I]]=Goals[I];
  if(BattleTactics::LeaderGuardEnabled() && !Rally && !CoverArrival && Goals.Num()>1 && P.ArrivalGoals.Contains(S->LeaderUnitID))
  {
   // Trial: the AI leader stops on the rearmost reserved slot. Goals are sorted front to back.
   int32 RearID=S->LeaderUnitID;
   for(const auto& Pair:P.ArrivalGoals) if(Pair.Value==Goals.Last()) {RearID=Pair.Key;break;}
   Swap(P.ArrivalGoals[S->LeaderUnitID],P.ArrivalGoals[RearID]);
  }
  if(Rally && P.ArrivalGoals.Contains(S->LeaderUnitID))
  {
   // ST2 also requires the leader within 3m, independently of follower cohesion.
   int32 ClosestID=S->LeaderUnitID;
   for(const auto& Pair:P.ArrivalGoals) if(FVector::DistSquared2D(Pair.Value,S->Destination)<FVector::DistSquared2D(P.ArrivalGoals[ClosestID],S->Destination)) ClosestID=Pair.Key;
   Swap(P.ArrivalGoals[S->LeaderUnitID],P.ArrivalGoals[ClosestID]);
  }
 }
 P.Corridors.Reset();
 P.bNarrowCorridor=false;
 for(int32 I=1;I<P.Path.Num();++I)
 {
  const FVector Segment=(P.Path[I]-P.Path[I-1]).GetSafeNormal2D(), Side(-Segment.Y,Segment.X,0);
  if(!ClearMovementSegment(P.Path[I-1]+Side*120,P.Path[I]+Side*120) || !ClearMovementSegment(P.Path[I-1]-Side*120,P.Path[I]-Side*120)) P.bNarrowCorridor=true;
 }
 for(int32 ID:S->Members) if(auto* B=Bodies.Find(ID))
 {
  B->bArrived=false; B->bTrafficWait=false; B->StalledSince=0; B->PassingID=INDEX_NONE;
  B->EscapeUntil=0;B->NextEscapeProbe=0;
  B->ContactRoute.Reset();B->NextContactProbe=0;B->ContactRouteUntil=0;B->NextArrivalProbe=0;B->bArrivalOccupied=false;B->bContactWaiting=false;
  B->LastProgressTime=GetWorld()->GetTimeSeconds(); B->LastProgressPosition=Position(ID);
  auto& Route=P.Corridors.Add(ID,P.Path);
  const int32 Slot=S->Members.IndexOfByKey(ID);
  for(int32 I=1;I<Route.Num();++I)
  {
   const FVector D=(P.Path[I]-P.Path[I-1]).GetSafeNormal2D(), Right(-D.Y,D.X,0);
   // Keep right for opposing streams; shrink to one lane if the static corridor is narrow.
   for(float Width:{50.f+(Slot%3)*90.f,50.f,0.f})
   {
    const FVector A=P.Path[I-1]+Right*Width, End=P.Path[I]+Right*Width;
    if(ClearMovementSegment(A,End) && ClearMovementSegment(P.Path[I],End)) {if(I==1) Route[0]=A;Route[I]=End;break;}
   }
  }
  // Map E cover arrival: continue from the end of the shared path along the member's own route to his cover slot.
  if(const auto* Tail=P.ArrivalTails.Find(ID); Tail && Tail->Num()>1)
  {
   if(Route.IsEmpty()) Route=*Tail;
   else { Route.Last()=(*Tail)[0]; for(int32 I=1;I<Tail->Num();++I) Route.Add((*Tail)[I]); }
  }
  B->Waypoint=1;
  if(!NewOrder && Route.Num()>1)
  {
   // Rejoin the closest remaining corridor segment instead of walking back to the leader.
   double Best=DBL_MAX;
   for(int32 I=1;I<Route.Num();++I)
   {
    const FVector Closest=FMath::ClosestPointOnSegment(Position(ID),Route[I-1],Route[I]);
    const double Distance=FVector::DistSquared2D(Position(ID),Closest);
    if(Distance<=Best && ClearMovementSegment(Position(ID),Route[I])) {Best=Distance;B->Waypoint=I;}
   }
  }
 }
}

bool UBattleLeaderRuntime::StableMove(int32 ID,float Dt,double Now,FVector& Direction)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); const auto* S=Store->FindSquad(U->SquadID);
 auto& B=Bodies.FindChecked(ID); auto& P=Plans.FindChecked(U->SquadID);
 if(bContactAvoidance) ReassignOccupiedArrival(ID,Now);
 const auto* Goal=P.ArrivalGoals.Find(ID); const auto* Route=P.Corridors.Find(ID);
 B.bTrafficWait=false; B.bArrived=false;
 auto Stop=[&](){if(B.Actor.IsValid()) B.Actor->GetCharacterMovement()->StopMovementImmediately();B.Velocity=FVector::ZeroVector;};
 if(B.bArrivalOccupied){B.bTrafficWait=true;Stop();return false;}
 if(!Goal || !Route || !Route->IsValidIndex(B.Waypoint))
 {
  B.bTrafficWait=!Goal;
  // Missing slots remain a visible wait; retry through the existing one-second
  // shared path budget instead of leaving a restored/crowded squad stuck forever.
  if(!Goal){P.bReplan=true;P.ArrivalRevision=INDEX_NONE;}
  Stop();return false;
 }
 const FVector Pos=Position(ID);
 bool Final=B.Waypoint==Route->Num()-1;
 FVector Way=(*Route)[B.Waypoint];
 const FVector Segment=(Way-(*Route)[B.Waypoint-1]).GetSafeNormal2D(), SegmentRight(-Segment.Y,Segment.X,0);
 const bool NarrowHere=P.bNarrowCorridor && !ClearMovementSegment(Pos-SegmentRight*120,Pos+SegmentRight*120);
 if(Final && !NarrowHere && (!P.bNarrowCorridor || FVector::Dist2D(Pos,*Goal)<1000) && ClearMovementSegment(Pos,*Goal)) Way=*Goal;
 else if(Final && P.bNarrowCorridor)
 {
  // Join the lane before entering the pinch; aiming only at the distant endpoint
  // leaves the front rows from opposing formations on the same center line.
  const FVector Closest=FMath::ClosestPointOnSegment(Pos,(*Route)[B.Waypoint-1],Way);
  Way=Closest+Segment*FMath::Min(250.,FVector::Dist2D(Closest,Way));
 }
 if(Final && FVector::Dist2D(Pos,*Goal)<20)
 {B.bArrived=true;Stop();return true;}
 if(!Final && FVector::Dist2D(Pos,Way)<65) {++B.Waypoint;Way=(*Route)[B.Waypoint];Final=B.Waypoint==Route->Num()-1;}
 FVector D=(Way-Pos).GetSafeNormal2D(); const FVector Right(-D.Y,D.X,0);
 const bool Confined=NarrowHere;
 if(B.PassingID!=INDEX_NONE)
 {
  const FVector Delta=Position(B.PassingID)-Pos;
  const auto* PassingUnit=Store->Find(B.PassingID);
  const auto* PassingPlan=Plans.Find(PassingUnit->SquadID);
  if(Confined && PassingUnit->Team==U->Team && PassingPlan && PassingPlan->Path.Num()>1 && FVector::DotProduct((PassingPlan->Path.Last()-PassingPlan->Path[0]).GetSafeNormal2D(),D)>.5) B.PassingID=INDEX_NONE;
  if(!PassingUnit->IsAlive() || FVector::DotProduct(Delta,D)<-80 || Delta.Size2D()>350 ||
     (B.bPassingSecond && FVector::Dist2D(Pos,B.PassingWaypoint)<40)) B.PassingID=INDEX_NONE;
 }
 FVector Separation=FVector::ZeroVector; float Scale=1;
 int32 Blocker=INDEX_NONE; double BlockerDistance=DBL_MAX;
 const FIntPoint Cell(FMath::FloorToInt(Pos.X/150.),FMath::FloorToInt(Pos.Y/150.)); int32 Checked=0;
 for(int32 X=-1;X<=1 && Checked<32;++X) for(int32 Y=-1;Y<=1 && Checked<32;++Y)
  if(const auto* Nearby=AvoidanceGrid.Find(Cell+FIntPoint(X,Y))) for(int32 Other:*Nearby)
  {
   if(Other==ID || !Store->Find(Other)->IsAlive()) continue;
   if(++Checked>32) break;
   const FVector Delta=Position(Other)-Pos;
   const double Along=FVector::DotProduct(Delta,D), Side=FVector::DotProduct(Delta,Right), Dist=Delta.Size2D();
   if(Dist>1 && Dist<110) Separation-=Delta.GetSafeNormal2D()*(110-Dist)/110;
   if(Along>0 && Along<155 && FMath::Abs(Side)<76)
   {
    Scale=FMath::Min(Scale,float(FMath::Clamp((Along-78)/70.,0.,1.)));
    const auto* OtherUnit=Store->Find(Other); const auto* OtherPlan=OtherUnit->Team==U->Team?Plans.Find(OtherUnit->SquadID):nullptr;
    const bool SameFlow=FVector::DotProduct(Bodies.FindChecked(Other).Velocity.GetSafeNormal2D(),D)>.5 || (OtherPlan && OtherPlan->Path.Num()>1 && FVector::DotProduct((OtherPlan->Path.Last()-OtherPlan->Path[0]).GetSafeNormal2D(),D)>.5);
    // Queue behind a moving ally in a tight corridor instead of overtaking into oncoming traffic.
    if(SameFlow && (Confined || Bodies.FindChecked(Other).Velocity.Size2D()>20)) continue;
    if(Dist<BlockerDistance) {Blocker=Other;BlockerDistance=Dist;}
   }
  }
 if(Blocker!=INDEX_NONE && B.PassingID==INDEX_NONE)
 {
  const float Sign=FVector::DotProduct(Position(Blocker)-Pos,Right)>10?-1.f:1.f;
  for(float SideSign:{Sign,-Sign})
  {
   for(float Distance:{110.f,80.f})
   {
    const FVector Offset=Right*SideSign*Distance, Bypass=Position(Blocker)+Offset+D*20;
    if(!ClearMovementSegment(Pos,Bypass)) continue;
    B.PassingID=Blocker;B.PassingWaypoint=Bypass;
    B.PassingForward=D;B.PassingOffset=Offset;B.bPassingSecond=false;break;
   }
   if(B.PassingID!=INDEX_NONE) break;
  }
 }
 // Short, persistent two-point bypass: a routing choice, not unbounded repulsion.
 // Steering remains capped relative to this route and every physical move is swept.
 if(B.PassingID!=INDEX_NONE)
 {
  if(!B.bPassingSecond && FVector::Dist2D(Pos,B.PassingWaypoint)<40)
  {B.PassingWaypoint=Position(B.PassingID)+B.PassingOffset+B.PassingForward*160;B.bPassingSecond=true;}
  if(ClearMovementSegment(Pos,B.PassingWaypoint))
  {D=(B.PassingWaypoint-Pos).GetSafeNormal2D();Separation=FVector::ZeroVector;Scale=FMath::Max(Scale,.6f);}
  else B.PassingID=INDEX_NONE;
 }
 // A stopped cluster can block a two-point pass. Probe at most twelve short,
 // fully swept alternatives once per second, without another NavMesh search.
 if(Now>=B.EscapeUntil && Now>=B.NextEscapeProbe && Now-B.LastProgressTime>1)
 {
  B.NextEscapeProbe=Now+1;
  const UPrimitiveComponent* Ignore=B.Actor.IsValid()?B.Actor->GetCapsuleComponent():B.Capsule.Get();
  const FVector Desired=(Way-Pos).GetSafeNormal2D();
  for(float Angle:{0.f,30.f,-30.f,60.f,-60.f,90.f,-90.f,120.f,-120.f,150.f,-150.f,180.f})
  {
   const FVector Delta=Desired.RotateAngleAxis(Angle,FVector::UpVector)*120;
   FHitResult Probe;SweepSoldier(GetWorld(),Ignore,Pos,Delta,Probe,75);
   if(Probe.bBlockingHit) continue;
   B.EscapeWaypoint=Pos+Delta;B.EscapeUntil=Now+1;B.PassingID=INDEX_NONE;break;
  }
 }
 if(Now<B.EscapeUntil)
 {
  if(FVector::Dist2D(Pos,B.EscapeWaypoint)<20) B.EscapeUntil=0;
  else {D=(B.EscapeWaypoint-Pos).GetSafeNormal2D();Separation=FVector::ZeroVector;Scale=1;}
 }
 if(bContactAvoidance && (Blocker!=INDEX_NONE || Now-B.LastProgressTime>1 || !B.ContactRoute.IsEmpty() || B.bContactWaiting))
  if(ResolveStoppedContact(ID,Way,Now,D,Scale)) Separation=FVector::ZeroVector;
 Direction=LimitedSteering(D,Separation,GetDefault<UBattleTuning>()->WideAvoidanceMaxSide);
 B.bTrafficWait=Scale<.15f;
 const float Speed=GetDefault<UBattleTuning>()->MoveSpeed;
 if(B.Actor.IsValid())
 {
  if(Scale<=.01f) Stop(); else B.Actor->AddMovementInput(Direction,Scale);
  B.Velocity=B.Actor->GetVelocity();
 }
 else
 {
  FHitResult Hit; const FVector Before=B.Capsule->GetComponentLocation();
  const FVector Step=Direction*FMath::Min(Speed*Dt*Scale,float(FVector::Dist2D(Way,Pos)));
  // MoveComponent ignores its owner. Mass capsules share DisplayHost, so explicitly
  // sweep against sibling capsules too, excluding only the moving component.
  auto SweepMove=[&](FVector Delta,FHitResult& Result)
  {
   const FVector From=B.Capsule->GetComponentLocation();
   SweepSoldier(GetWorld(),B.Capsule.Get(),From,Delta,Result);
   const float Fraction=Result.bBlockingHit?FMath::Max(0.f,Result.Time-.001f):1.f;
   B.Capsule->MoveComponent(Delta*Fraction,Direction.Rotation().Quaternion(),true);
  };
  SweepMove(Step,Hit);
  if(Hit.bBlockingHit)
  {
   const FVector Slide=FVector::VectorPlaneProject(Step*(1-Hit.Time),Hit.Normal);
   if(!Slide.IsNearlyZero()) {FHitResult SlideHit;SweepMove(Slide,SlideHit);}
  }
  auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity); F.Position=B.Capsule->GetComponentLocation();
  B.Velocity=(F.Position-Before)/FMath::Max(Dt,.001f);
 }
 if(FVector::Dist2D(Pos,B.LastProgressPosition)>40) {B.LastProgressPosition=Pos;B.LastProgressTime=Now;B.StalledSince=0;}
 else if(Now-B.LastProgressTime>1) {B.StalledSince=B.LastProgressTime;B.bTrafficWait=true;}
 // Map E: a member cut off from the shared corridor by walls gets a route of his own to his arrival spot, instead of
 // waiting for a squad replan that starts from the squad centre and does not help him. At most two such queries per frame.
 if(BattleFieldLayout::IsUrban(GetWorld()) && Now-B.LastProgressTime>2 && Now>=B.NextOwnPath && !ClearMovementSegment(Pos,Way))
 {
  static uint64 QueryFrame=0; static int32 QueriesThisFrame=0;
  if(QueryFrame!=GFrameCounter) {QueryFrame=GFrameCounter;QueriesThisFrame=0;}
  if(QueriesThisFrame<2)
  {
   ++QueriesThisFrame; B.NextOwnPath=Now+3;
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   auto* Own=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),Pos,*Goal):nullptr;
   if(Own && Own->IsValid() && Own->PathPoints.Num()>1)
   {
    TArray<FVector> OwnRoute; for(const auto& Point:Own->PathPoints) OwnRoute.Add(Point);
    P.Corridors.Add(ID,MoveTemp(OwnRoute)); B.Waypoint=1; B.LastProgressTime=Now; B.StalledSince=0;
    SightExperiment.Cover(TEXT("move_own_route"));
    return true;
   }
  }
 }
 // Congestion is not a navigation failure: do not reset all ten paths every two seconds.
 if(B.StalledSince>0 && Now-B.LastProgressTime>4 && !ClearMovementSegment(Pos,Way) && Now>=P.NextPath)
 {P.bReplan=true;P.NextPath=Now+1;B.LastProgressTime=Now;}
 return true;
}

FString UBattleLeaderRuntime::MovementLabel(int32 SID) const
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID); const auto* P=Plans.Find(SID);
 if(!bWideField || !bStableMovement || !S || !P || S->Order==EBattleOrder::Hold) return FString();
 if(P->bReplan) return TEXT("MOVE: ROUTE PENDING");
 if(P->Path.Num()<2) return TEXT("MOVE: NO SHARED ROUTE");
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 int32 Arrived=0,Waiting=0,Occupied=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive())
 {
  if(!P->ArrivalGoals.Contains(ID)) return TEXT("MOVE: NO FREE ARRIVAL SLOT");
  if(const auto* B=Bodies.Find(ID)) {Arrived+=B->bArrived;Waiting+=B->bTrafficWait;Occupied+=B->bArrivalOccupied;}
 }
 if(Occupied) return FString::Printf(TEXT("MOVE: OCCUPIED SLOTS %d / WAIT %d"),Occupied,Waiting);
 return FString::Printf(TEXT("MOVE: ARRIVED %d / TRAFFIC WAIT %d"),Arrived,Waiting);
}
