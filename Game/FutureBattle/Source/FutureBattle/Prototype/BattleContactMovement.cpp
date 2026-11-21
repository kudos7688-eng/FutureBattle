#include "BattleLeaderRuntime.h"
#include "BattleTerrain.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"

bool UBattleLeaderRuntime::IsStationaryAlly(int32 ID,int32 Other) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);const auto* O=Store->Find(Other);
 const auto* B=Bodies.Find(Other);const auto* S=O?Store->FindSquad(O->SquadID):nullptr;const auto* P=S?Plans.Find(S->SquadID):nullptr;
 return ID!=Other && U && O && O->IsAlive() && O->Team==U->Team && B && S && B->Velocity.SizeSquared2D()<400 &&
  // Arrival within an executing Move order still uses the established queue/
  // formation avoidance. Do not freeze that flow as a stationary hold cluster.
  (S->Order==EBattleOrder::Hold || O->Action==EBattleAction::DirectControl || (P && P->TargetID!=INDEX_NONE && S->Order!=EBattleOrder::Move));
}

void UBattleLeaderRuntime::ReassignOccupiedArrival(int32 ID,double Now)
{
 auto& B=Bodies.FindChecked(ID);if(Now<B.NextArrivalProbe)return;
 B.NextArrivalProbe=Now+1;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);const auto* S=Store->FindSquad(U->SquadID);auto& P=Plans.FindChecked(S->SquadID);
 const auto* Old=P.ArrivalGoals.Find(ID);if(!Old || FVector::Dist2D(Position(ID),*Old)>900)return;
 auto Occupied=[&](FVector Goal)
 {
  for(const auto& Other:Bodies) if(IsStationaryAlly(ID,Other.Key) && FVector::DistSquared2D(Position(Other.Key),Goal)<10000) return true;
  return false;
 };
 B.bArrivalOccupied=Occupied(*Old);
 // A free slot may still be surrounded by stationary soldiers. After sustained
 // contact waiting, prefer a reachable free slot rather than shifting the holders.
 const bool AccessBlocked=B.bContactWaiting && Now-B.LastProgressTime>3;
 if(!B.bArrivalOccupied && !AccessBlocked)return;
 const bool Rally=S->TacticalPlan.bAutonomous && (S->TacticalPlan.Action==EBattleSquadTactic::Retreat || S->TacticalPlan.Action==EBattleSquadTactic::Regroup || S->TacticalPlan.Action==EBattleSquadTactic::Recover);
 const FVector Forward=P.Path.Num()>1?(P.Path.Last()-P.Path[P.Path.Num()-2]).GetSafeNormal2D():FVector::ForwardVector,Right(-Forward.Y,Forward.X,0);
 TArray<FVector> Candidates;
 for(int32 Row=-6;Row<=0;++Row) for(int32 Col=-6;Col<=6;++Col)
 {
  const FVector Offset=Forward*(Row*200)+Right*(Col*200);
  if(Rally && Offset.Size2D()>(ID==S->LeaderUnitID?250:450))continue;
  if(S->Order==EBattleOrder::Defend && Offset.Size2D()>S->DefenseRadius-80)continue;
  Candidates.Add(S->Destination+Offset);
 }
 const FVector Reference=AccessBlocked?Position(ID):*Old;
 const double Turn=SectorTurn(S->Team); // -BattleSectorSymmetric: ties in the team's own frame
 Candidates.Sort([&](const FVector& A,const FVector& C){const double DA=FVector::DistSquared2D(A,Reference),DC=FVector::DistSquared2D(C,Reference);return DA!=DC?DA<DC:A.X!=C.X?Turn*A.X<Turn*C.X:Turn*A.Y<Turn*C.Y;});
 auto Reserved=[&](FVector Goal)
 {
  for(const auto& Pair:Plans)
  {
   const auto* OS=Store->FindSquad(Pair.Key);if(!OS || OS->Team!=S->Team || OS->Order==EBattleOrder::Hold || OS->CommandStatus!=EBattleCommandStatus::Active)continue;
   for(const auto& Slot:Pair.Value.ArrivalGoals) if(Slot.Key!=ID && Store->Find(Slot.Key)->IsAlive() && FVector::DistSquared2D(Slot.Value,Goal)<FMath::Square(190.))return true;
  }
  return false;
 };
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());int32 Queries=0;
 for(const FVector& Candidate:Candidates)
 {
  if(Occupied(Candidate) || Reserved(Candidate))continue;
  if(++Queries>12)break;
  FNavLocation Projected;if(!Nav || !Nav->ProjectPointToNavigation(Candidate,Projected,FVector(30,30,200)) || !ClearMovementSegment(S->Destination,Projected.Location))continue;
  if(Rally && FVector::Dist2D(Projected.Location,S->Destination)>(ID==S->LeaderUnitID?250:450))continue;
  if(S->Order==EBattleOrder::Defend && FVector::Dist2D(Projected.Location,S->Destination)>S->DefenseRadius-80)continue;
  if(Occupied(Projected.Location) || Reserved(Projected.Location))continue;
  if(AccessBlocked)
  {
   if(FVector::DistSquared2D(Projected.Location,*Old)<10000)continue;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(ArrivalAccess));Q.AddIgnoredComponent(B.Actor.IsValid()?B.Actor->GetCapsuleComponent():B.Capsule.Get());
   FVector End=Projected.Location;End.Z=Position(ID).Z;TArray<FHitResult> Hits;
   ++P.ContactProbes;GetWorld()->SweepMultiByChannel(Hits,Position(ID),End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,75),Q);
   bool Blocked=false;for(const auto& Hit:Hits)if(Hit.bBlockingHit && !(Hit.bStartPenetrating && FVector::DotProduct(End-Position(ID),Hit.ImpactNormal)>0) && !BattleTerrain::IsGround(Hit)){Blocked=true;break;}
   if(Blocked)continue;
  }
  P.ArrivalGoals[ID]=Projected.Location;++P.ArrivalReassignments;
  B.bArrivalOccupied=false;B.bArrived=false;B.ContactRoute.Reset();B.bContactWaiting=false;B.NextContactProbe=0;B.EscapeUntil=0;B.PassingID=INDEX_NONE;
  return; // Only this soldier's slot changes; the squad route and mission remain intact.
 }
}

bool UBattleLeaderRuntime::ResolveStoppedContact(int32 ID,FVector Way,double Now,FVector& Direction,float& Scale)
{
 auto& B=Bodies.FindChecked(ID);const FVector Pos=Position(ID);
 if(Now>B.ContactRouteUntil) B.ContactRoute.Reset();
 while(!B.ContactRoute.IsEmpty() && FVector::Dist2D(Pos,B.ContactRoute[0])<25)B.ContactRoute.RemoveAt(0);
 if(!B.ContactRoute.IsEmpty())
 {
  Direction=(B.ContactRoute[0]-Pos).GetSafeNormal2D();Scale=1;B.PassingID=INDEX_NONE;B.EscapeUntil=0;return true;
 }
 if(Now<B.NextContactProbe){if(B.bContactWaiting){Scale=0;B.EscapeUntil=0;B.PassingID=INDEX_NONE;}return B.bContactWaiting;}
 B.NextContactProbe=Now+1;B.bContactWaiting=false;
 const FVector Forward=(Way-Pos).GetSafeNormal2D(),Right(-Forward.Y,Forward.X,0);
 double Left=0,RightEdge=0,Front=0;bool Blocked=false;
 for(const auto& Other:Bodies) if(IsStationaryAlly(ID,Other.Key))
 {
  const FVector Delta=Position(Other.Key)-Pos;const double Along=FVector::DotProduct(Delta,Forward),Side=FVector::DotProduct(Delta,Right);
  if(Along<-100 || Along>500 || FMath::Abs(Side)>450)continue;
  Left=FMath::Min(Left,Side);RightEdge=FMath::Max(RightEdge,Side);Front=FMath::Max(Front,Along);
  if(Along>0 && Along<200 && FMath::Abs(Side)<80)Blocked=true;
 }
 if(!Blocked)return false;
 const UPrimitiveComponent* Ignore=B.Actor.IsValid()?B.Actor->GetCapsuleComponent():B.Capsule.Get();
 auto Clear=[&](FVector A,FVector C)
 {
  ++Plans.FindChecked(GetWorld()->GetSubsystem<UBattleState>()->Find(ID)->SquadID).ContactProbes;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(StoppedContact));Q.AddIgnoredComponent(Ignore);
  TArray<FHitResult> Hits;GetWorld()->SweepMultiByChannel(Hits,A,C,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,75),Q);
  for(const auto& Hit:Hits)if(Hit.bBlockingHit && !(Hit.bStartPenetrating && FVector::DotProduct(C-A,Hit.ImpactNormal)>0) && !BattleTerrain::IsGround(Hit))return false;
  return true;
 };
 // Near the goal, a gap between standing soldiers can be shorter than going
 // around the whole cluster. Test bounded two-segment routes through that gap.
 if(FVector::Dist2D(Pos,Way)<900)
 {
  FVector End=Way;End.Z=Pos.Z;
  for(float Radius:{120.f,240.f,360.f})for(float Angle:{0.f,30.f,-30.f,60.f,-60.f,90.f,-90.f,120.f,-120.f,180.f})
  {
   const FVector Mid=Pos+Forward.RotateAngleAxis(Angle,FVector::UpVector)*Radius;
   if(!Clear(Pos,Mid) || !Clear(Mid,End))continue;
   B.ContactRoute={Mid,End};B.ContactRouteUntil=Now+6;B.PassingID=INDEX_NONE;B.EscapeUntil=0;
   Direction=(Mid-Pos).GetSafeNormal2D();Scale=1;return true;
  }
 }
 // At most six further three-segment candidates per stalled soldier per second; no NavMesh search.
 for(float Back:{0.f,-100.f,-200.f})for(double Side:{RightEdge+100,Left-100})
 {
  const FVector A=Pos+Forward*Back,C=A+Right*Side,D=Pos+Forward*(Front+100)+Right*Side;
  if((Back!=0 && !Clear(Pos,A)) || !Clear(A,C) || !Clear(C,D))continue;
  B.ContactRoute.Reset();if(Back!=0)B.ContactRoute.Add(A);B.ContactRoute.Add(C);B.ContactRoute.Add(D);
  B.ContactRouteUntil=Now+6;B.PassingID=INDEX_NONE;B.EscapeUntil=0;
  Direction=(B.ContactRoute[0]-Pos).GetSafeNormal2D();Scale=1;return true;
 }
 B.bContactWaiting=true;B.EscapeUntil=0;B.PassingID=INDEX_NONE;Scale=0;return true;
}

// Trial -BattleAllyPass=1 (2026-11-19, user request; off by default). A soldier that waited 3 s in front of a stopped ally cluster
// that ResolveStoppedContact found no way around walks through the stopped allies within 8 m for 5 s: its own sweep (Mass) or its
// capsule's movement (Actor leader) ignores their bodies. Walls, the ground, moving soldiers and enemies still block it. Only the
// passing soldier changes; the stopped allies keep their places. Found with map H's sector audit (8-soldier squads, squad 8).
void UBattleLeaderRuntime::BeginAllyPass(int32 ID,double Now)
{
 EndAllyPass(ID);
 auto& B=Bodies.FindChecked(ID);const FVector Pos=Position(ID);
 UPrimitiveComponent* Own=B.Actor.IsValid()?static_cast<UPrimitiveComponent*>(B.Actor->GetCapsuleComponent()):static_cast<UPrimitiveComponent*>(B.Capsule.Get());
 for(const auto& Other:Bodies)
 {
  if(!IsStationaryAlly(ID,Other.Key)||FVector::DistSquared2D(Position(Other.Key),Pos)>FMath::Square(800.))continue;
  UPrimitiveComponent* C=Other.Value.Actor.IsValid()?static_cast<UPrimitiveComponent*>(Other.Value.Actor->GetCapsuleComponent()):static_cast<UPrimitiveComponent*>(Other.Value.Capsule.Get());
  if(!C)continue;
  B.AllyPassIgnored.Add(C);
  if(B.Actor.IsValid()&&Own)Own->IgnoreComponentWhenMoving(C,true);
 }
 B.AllyPassUntil=Now+5;
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 UE_LOG(LogTemp,Display,TEXT("BATTLE_ALLY_PASS unit=%d squad=%d team=%d ignored=%d t=%.1f pos=%s"),ID,U?U->SquadID:-1,U?U->Team:-1,B.AllyPassIgnored.Num(),Now,*Pos.ToCompactString());
}

void UBattleLeaderRuntime::EndAllyPass(int32 ID)
{
 auto* B=Bodies.Find(ID);if(!B)return;
 if(B->Actor.IsValid())
  if(UPrimitiveComponent* Own=B->Actor->GetCapsuleComponent())
   for(const auto& C:B->AllyPassIgnored)if(C.IsValid())Own->IgnoreComponentWhenMoving(C.Get(),false);
 B->AllyPassIgnored.Reset();B->AllyPassUntil=0;
}
