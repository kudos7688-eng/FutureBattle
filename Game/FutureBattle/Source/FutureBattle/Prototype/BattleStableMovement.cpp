#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "BattleCover.h"
#include "BattleStance.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

namespace
{
void SweepSoldier(UWorld* World,const UPrimitiveComponent* Ignore,FVector From,FVector Delta,FHitResult& Result,float HalfHeight=90,const TArray<TWeakObjectPtr<UPrimitiveComponent>>* AlsoIgnore=nullptr)
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleMassMovement)); Query.AddIgnoredComponent(Ignore);
 if(AlsoIgnore)for(const auto& C:*AlsoIgnore)if(C.IsValid())Query.AddIgnoredComponent(C.Get()); // -BattleAllyPass: stopped allies
 TArray<FHitResult> Hits;
 World->SweepMultiByChannel(Hits,From,From+Delta,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,HalfHeight),Query);
 Result=FHitResult();
 for(const auto& Candidate:Hits) if(Candidate.bBlockingHit)
 {
  // Match UE movement's handling of initial contacts when moving away from them.
  if(Candidate.bStartPenetrating && FVector::DotProduct(Delta,Candidate.ImpactNormal)>0) continue;
  // Map with heights: the ground pieces are followed by height (TerrainStep), they do not stop a soldier.
  if(BattleTerrain::IsGround(Candidate)) continue;
  Result=Candidate;break;
 }
}
// Trial -BattleAllyPass=1 (default 0): see FBattleLeaderBody::AllyPassUntil.
bool AllyPassOn(){static const bool V=[]{int32 X=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAllyPass="),X);return X!=0;}();return V;}
}

bool UBattleLeaderRuntime::TerrainStep(const FVector& From,FVector& Step) const
{
 const auto& Heights=BattleFieldLayout::Of(GetWorld()).Heights;
 if(Heights.IsEmpty())return true;
 double Here=0,There=0,Slope=0;
 Heights.Ground(From.X,From.Y,From.Z-100,Here,nullptr,1.e9);
 if(!Heights.Ground(From.X+Step.X,From.Y+Step.Y,Here,There,&Slope))return false;
 Step.Z=There+BattleTerrain::CentreAbove(Slope)-From.Z;return true;
}

bool UBattleLeaderRuntime::ClearMovementSegment(FVector A,FVector B) const
{
 if(const auto& Heights=BattleFieldLayout::Of(GetWorld()).Heights;!Heights.IsEmpty())
 {
  // Map with heights: each end at capsule height above the level it is on (the ends arrive as feet or as capsule centres);
  // the ground pieces themselves do not block.
  for(FVector* P:{&A,&B}){double Level=0,Slope=0;Heights.Ground(P->X,P->Y,P->Z-50,Level,&Slope,1.e9);P->Z=Level+BattleTerrain::CentreAbove(Slope)+8;}
  TArray<FHitResult> Hits;GetWorld()->SweepMultiByObjectType(Hits,A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,75));
  for(const auto& Hit:Hits)if(Hit.bBlockingHit&&!BattleTerrain::IsGround(Hit))return false;
  return true;
 }
 // This runtime currently uses the flat wide field. Query static geometry, not nearby soldiers.
 A.Z=FMath::Max(A.Z,100.); B.Z=FMath::Max(B.Z,100.);
 return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,75));
}

void UBattleLeaderRuntime::BuildMovementPlan(int32 SID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID); auto& P=Plans.FindChecked(SID);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 const bool Sector=S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission;
 const auto Cell=BattleSectors::Cell(S->Destination);
 const bool NewOrder=P.ArrivalRevision!=S->OrderRevision;
 if(NewOrder || P.ArrivalGoals.IsEmpty())
 {
  P.ArrivalGoals.Reset(); P.ArrivalTails.Reset(); P.ArrivalRevision=S->OrderRevision;
  TArray<FVector> Reserved;
  for(const auto& Other:Plans) if(Other.Key!=SID)
   if(const auto* OS=Store->FindSquad(Other.Key); OS && OS->Team==S->Team && OS->Order!=EBattleOrder::Hold && OS->CommandStatus==EBattleCommandStatus::Active)
    for(const auto& G:Other.Value.ArrivalGoals) if(Store->Find(G.Key)->IsAlive()) Reserved.Add(G.Value);
  // Sector missions (2026-11-20): a squad that already holds the sector (order Hold, so its arrival places are no longer
  // reserved above) still stands in its cover slots. Without this, the next squad sent into the sector was given those same
  // slots and its soldiers stood beside the occupied one for good (map I: 850 move stalls a match at one shed corner in G8
  // with -BattleSectorRoute=1). Reserve where the team's other soldiers in the sector stand, and the places of holding squads.
  // -BattleSectorSlotReserve=0 turns it off for comparison. Maps without sectors are unchanged.
  static const bool SectorSlotReserve=[](){int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSlotReserve="),V);return V!=0;}();
  if(Sector && SectorSlotReserve)
   for(const auto& Other:Plans) if(Other.Key!=SID)
    if(const auto* OS=Store->FindSquad(Other.Key); OS && OS->Team==S->Team)
    {
     for(int32 MID:OS->Members) if(const auto* M=Store->Find(MID); M && M->IsAlive())
      if(const FVector At=Position(MID); BattleSectors::Cell(At)==Cell) Reserved.Add(At);
     if(OS->Order==EBattleOrder::Hold)
      for(const auto& G:Other.Value.ArrivalGoals) if(Store->Find(G.Key)->IsAlive() && BattleSectors::Cell(G.Value)==Cell) Reserved.Add(G.Value);
    }
  FVector Forward=P.Path.Num()>1?(P.Path.Last()-P.Path[P.Path.Num()-2]).GetSafeNormal2D():FVector::ForwardVector;
  // Trial -BattleArrivalThreat=1: face the known enemy nearest to the sector (within 200 m) instead of the way in.
  if(Sector && ArrivalThreatEnabled())
  {
   double Best=FMath::Square(20000.);bool bNear=false;FVector Near=FVector::ZeroVector;
   for(const FVector& E:GetWorld()->GetSubsystem<UBattleMatch>()->RouteKnownEnemies(S->Team))
    if(const double D=FVector::DistSquared2D(E,S->Destination);D<Best){Best=D;Near=E;bNear=true;}
   if(bNear && FVector::Dist2D(Near,S->Destination)>100.){Forward=(Near-S->Destination).GetSafeNormal2D();SightExperiment.Cover(TEXT("arrival_threat_facing"));}
   P.ArrivalFacing=bNear?Forward:FVector::ZeroVector;
  }
  // Trial -BattleAttackFocus=1: a sector squad with a focus chooses its arrival places against it (instead of the way in or the
  // -BattleArrivalThreat direction); the direction is kept for the squad's re-planning check.
  bool bFocusArrival=false;
  if(Sector && AttackFocusEnabled())
  {
   const FVector Dir=P.bFocus?(P.Focus-S->Destination).GetSafeNormal2D():FVector::ZeroVector;
   P.FocusPlanDir=Dir;
   if(!Dir.IsNearlyZero()){Forward=Dir;P.ArrivalFacing=Dir;bFocusArrival=true;SightExperiment.Cover(TEXT("arrival_focus_facing"));}
  }
  // Trial -BattleCoverStrict=1: arrival places must hide from the known enemy nearest to the sector (at most 2 of 5 samples).
  bool bStrictThreat=false;FVector StrictEye=FVector::ZeroVector;
  if(Sector && CoverStrictEnabled())
  {
   double Best=FMath::Square(20000.);
   for(const FVector& E:GetWorld()->GetSubsystem<UBattleMatch>()->RouteKnownEnemies(S->Team))
    if(const double D=FVector::DistSquared2D(E,S->Destination);D<Best){Best=D;StrictEye=E+FVector(0,0,60);bStrictThreat=true;}
  }
  auto HidesFromThreat=[&](const FVector& Ground){return !bStrictThreat || BattleTactics::ExposureAt(GetWorld(),Ground+FVector(0,0,90),Forward,StrictEye).VisibleSamples<=2;};
  const FVector Right(-Forward.Y,Forward.X,0);
  const bool Rally=S->TacticalPlan.bAutonomous &&
   (S->TacticalPlan.Action==EBattleSquadTactic::Retreat || S->TacticalPlan.Action==EBattleSquadTactic::Regroup || S->TacticalPlan.Action==EBattleSquadTactic::Recover || S->TacticalPlan.Action==EBattleSquadTactic::Flank);
  TArray<FVector> Candidates;
  for(int32 Row=-12;Row<=12;++Row) for(int32 Col=-12;Col<=12;++Col)
  {
   const FVector Offset=Forward*(Row*200)+Right*(Col*200);
   // Converging squads fill their own approach half-plane rather than crossing
   // the rally point to claim a slot behind the opposing incoming stream.
   if(Sector && BattleSectors::Cell(S->Destination+Offset)!=Cell)continue;
   if(!Sector && FVector::DotProduct(Offset,Forward)>20) continue;
   if(Rally && Offset.Size2D()>450) continue;
   if(!Sector && S->Order==EBattleOrder::Defend && Offset.Size2D()>S->DefenseRadius-80) continue;
   Candidates.Add(S->Destination+Offset);
  }
  // Fill from the approach side, so converging squads don't seek slots beyond each other.
  auto Score=[&](FVector V){return FMath::FloorToDouble(FVector::Dist2D(V,S->Destination)/450.)*100000.+FVector::Dist2D(V,P.Center);};
  const double Turn=SectorTurn(S->Team); // -BattleSectorSymmetric: ties in the team's own frame
  Candidates.Sort([&](const FVector& A,const FVector& B){const double SA=Score(A),SB=Score(B);return SA!=SB?SA<SB:A.X!=B.X?Turn*A.X<Turn*B.X:Turn*A.Y<Turn*B.Y;});
  // Map E: arrive behind cover. The grid below only accepts spots the destination reaches in a straight line, which is
  // exactly what excludes the far side of a car or a wall. So most members first try cover slots near the destination that
  // are shielded from the direction of advance, and walk there along a short route of their own. The first three (never
  // the leader) still take open spots on the objective, so that it can be captured and held.
  // -BattleCoverArrival=0 turns it off for comparison.
  static const bool CoverArrivalAllowed=[](){int32 Value=1;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverArrival="),Value);return Value!=0;}();
  const bool CoverArrival=CoverArrivalAllowed && BattleFieldLayout::IsUrban(GetWorld()) && !Rally && Nav;
  TArray<FVector> CoverCandidates; TSet<int32> CoverAssigned;
  // -BattleCoverOutward: the outward directions this sector is defended toward (rear and friendly-held sides left out).
  TArray<FVector> Outward;
  if(CoverArrival && Sector && CoverOutwardEnabled() && !bFocusArrival)
  {
   const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
   const FVector Mid=BattleSectors::Center(Cell);
   const FVector ToHome=(Match->AreaHome(S->Team)-Mid).GetSafeNormal2D();
   for(int32 DX=-1;DX<=1;++DX) for(int32 DY=-1;DY<=1;++DY) if(DX||DY)
   {
    const FIntPoint N=Cell+FIntPoint(DX,DY);
    if(!BattleSectors::Valid(N)){SightExperiment.Cover(TEXT("arrival_outward_skip_edge"));continue;}
    const uint8 Held=(S->Team==0||S->Team==1)?Match->SectorState[S->Team].FindRef(N):0;
    if(Held==1){SightExperiment.Cover(TEXT("arrival_outward_skip_friendly"));continue;}
    const FVector Dir=FVector(DX,DY,0).GetSafeNormal2D();
    if(!(Held&2) && !ToHome.IsNearlyZero() && FVector::DotProduct(Dir,ToHome)>.707f){SightExperiment.Cover(TEXT("arrival_outward_skip_rear"));continue;}
    Outward.Add(Dir);
   }
   SightExperiment.Cover(Outward.IsEmpty()?TEXT("arrival_outward_none_old_rule"):Outward.Num()<=2?TEXT("arrival_outward_dirs_le2"):Outward.Num()<=4?TEXT("arrival_outward_dirs_le4"):TEXT("arrival_outward_dirs_gt4"));
  }
  auto WallAhead=[&](const FVector& At)
  {
   const FCollisionObjectQueryParams Static(ECC_WorldStatic);
   // -BattleAttackFocus: an obstacle toward the focus from this slot, or 30 degrees either side.
   if(bFocusArrival)
    if(const FVector ToFocus=(P.Focus-At).GetSafeNormal2D();!ToFocus.IsNearlyZero())
    {
     for(const float Deg:{0.f,-30.f,30.f})
      if(GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,100),At+ToFocus.RotateAngleAxis(Deg,FVector::UpVector)*250+FVector(0,0,100),Static))return true;
     return false;
    }
   if(Outward.IsEmpty())return GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,100),At+Forward*250+FVector(0,0,100),Static);
   for(const FVector& Dir:Outward)if(GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,100),At+Dir*250+FVector(0,0,100),Static))return true;
   return false;
  };
  if(CoverArrival)
  {
   auto* Cover=GetWorld()->GetSubsystem<UBattleCover>(); Cover->BuildSlots();
   for(int32 Slot:Cover->NearbySlots(S->Destination,Sector?3600.f:1400.f,Sector?(CoverDenseEnabled()?512:256):48))
   {
    const FVector At=Cover->GetSlots()[Slot];
    if(Sector && BattleSectors::Cell(At)!=Cell)continue;
    // Evaluation (arrival_slot_*): slots in the sector, those without an obstacle 2.5 m ahead, those seen by the nearest threat (-BattleCoverStrict).
    SightExperiment.Cover(TEXT("arrival_slot_in_cell"));
    if(!WallAhead(At)) SightExperiment.Cover(TEXT("arrival_slot_no_wall_ahead"));
    else if(HidesFromThreat(At)) CoverCandidates.Add(At);
    else SightExperiment.Cover(TEXT("arrival_slot_exposed"));
   }
  }
  // -BattleCoverIndoor: in a sector, slots under a roof or upper floor first (stable order otherwise).
  int32 IndoorSlots=0;
  if(CoverIndoorEnabled() && Sector && !CoverCandidates.IsEmpty())
  {
   TArray<FVector> Indoor,Outdoor;
   for(const FVector& At:CoverCandidates)
    (GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,120),At+FVector(0,0,800),FCollisionObjectQueryParams(ECC_WorldStatic))?Indoor:Outdoor).Add(At);
   IndoorSlots=Indoor.Num();
   SightExperiment.Cover(IndoorSlots==0?TEXT("arrival_indoor_none"):IndoorSlots<5?TEXT("arrival_indoor_lt5"):TEXT("arrival_indoor_ge5"));
   CoverCandidates=MoveTemp(Indoor);CoverCandidates.Append(Outdoor);
  }
  const double TakenRadius=Sector && CoverDenseEnabled()?130.:190.;
  int32 Queries=0, OpenMembers=0, TailQueries=0;
  if(CoverArrival) SightExperiment.Cover(CoverCandidates.Num()==0?TEXT("arrival_plan_candidates_0"):CoverCandidates.Num()<5?TEXT("arrival_plan_candidates_lt5"):CoverCandidates.Num()<20?TEXT("arrival_plan_candidates_lt20"):TEXT("arrival_plan_candidates_ge20"));
  for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive())
  {
   bool Found=false;
   // Evaluation (arrival_open_*): why this member found no cover slot.
   bool bCoverTried=false,bCapHit=false;int32 ArrTaken=0,ArrNoPath=0,ArrLong=0;
   if(CoverArrival && (Sector || ID==S->LeaderUnitID || OpenMembers++>=3))
    for(const FVector& At:CoverCandidates)
    {
     const bool bIndoorSlot=int32(&At-CoverCandidates.GetData())<IndoorSlots;
     bCoverTried=true;
     if(TailQueries>=24) {bCapHit=true;break;}
     bool Taken=false; for(const auto& R:Reserved) if(FVector::DistSquared2D(R,At)<FMath::Square(TakenRadius)) {Taken=true;break;}
     if(Taken) {++ArrTaken;continue;}
     ++TailQueries;
     auto* Tail=Nav->FindPathToLocationSynchronously(GetWorld(),P.Path.Num()?P.Path.Last():S->Destination,At);
     if(!Tail || !Tail->IsValid() || Tail->IsPartial() || Tail->PathPoints.Num()<2 || Tail->GetPathLength()>(Sector?6500:2500))
     {if(Tail && Tail->IsValid() && !Tail->IsPartial() && Tail->PathPoints.Num()>=2) ++ArrLong; else ++ArrNoPath; continue;}
     P.ArrivalGoals.Add(ID,At); P.ArrivalTails.Add(ID,Tail->PathPoints); Reserved.Add(At); CoverAssigned.Add(ID); Found=true;
     SightExperiment.Cover(TEXT("arrival_cover_slot"));
     if(bIndoorSlot)SightExperiment.Cover(TEXT("arrival_indoor_slot"));
     break;
    }
   // Trial -BattleCoverHuddle=1 (2026-11-20, default 0; sector missions where arrivals use cover): a member left without a cover
   // slot lines up along the same wall as a slot instead of taking an open spot - 1 m or 2 m to either side of the slot along
   // the obstacle face, at the slot's distance from it, where the obstacle still stands in front (within 2.5 m) - so the squad
   // stands shoulder to shoulder in the lee of walls and houses. (The first version stood 1.3 m behind the slot: a queue in
   // the open, user 11-20.)
   if(CoverArrival && !Found && (Sector || bCoverTried))
    SightExperiment.Cover(CoverCandidates.Num()==0?TEXT("arrival_open_no_candidates"):bCapHit?TEXT("arrival_open_query_cap"):ArrNoPath+ArrLong==0?TEXT("arrival_open_all_taken"):ArrLong>=ArrNoPath?TEXT("arrival_open_path_too_long"):TEXT("arrival_open_no_path"));
   static const bool CoverHuddle=[](){int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverHuddle="),V);return V!=0;}();
   if(!Found && CoverHuddle && Sector && CoverArrival)
    for(const FVector& At:CoverCandidates)
    {
     if(Found || TailQueries>=40) break;
     FHitResult Face;
     if(!GetWorld()->LineTraceSingleByObjectType(Face,At+FVector(0,0,100),At+Forward*250+FVector(0,0,100),FCollisionObjectQueryParams(ECC_WorldStatic))) continue;
     const FVector Normal=FVector(Face.ImpactNormal.X,Face.ImpactNormal.Y,0).GetSafeNormal();
     if(Normal.IsNearlyZero()) continue;
     const FVector Along(-Normal.Y,Normal.X,0);
     for(const float Side:{100.f,-100.f,200.f,-200.f})
     {
      const FVector Want=At+Along*Side;
      if(BattleSectors::Cell(Want)!=Cell) continue;
      bool Taken=false; for(const auto& R:Reserved) if(FVector::DistSquared2D(R,Want)<FMath::Square(90.)) {Taken=true;break;}
      if(Taken) continue;
      FNavLocation Projected; if(!Nav->ProjectPointToNavigation(Want,Projected,FVector(30,30,200)) || FVector::Dist2D(Projected.Location,Want)>40) continue;
      if(!GetWorld()->LineTraceTestByObjectType(Projected.Location+FVector(0,0,100),Projected.Location-Normal*250+FVector(0,0,100),FCollisionObjectQueryParams(ECC_WorldStatic)) || !HidesFromThreat(Projected.Location)) continue;
      ++TailQueries;
      auto* Tail=Nav->FindPathToLocationSynchronously(GetWorld(),P.Path.Num()?P.Path.Last():S->Destination,Projected.Location);
      if(!Tail || !Tail->IsValid() || Tail->IsPartial() || Tail->PathPoints.Num()<2 || Tail->GetPathLength()>6500) continue;
      P.ArrivalGoals.Add(ID,Projected.Location); P.ArrivalTails.Add(ID,Tail->PathPoints); Reserved.Add(Projected.Location); CoverAssigned.Add(ID); Found=true;
      SightExperiment.Cover(TEXT("arrival_huddle"));
      break;
     }
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

bool UBattleLeaderRuntime::ObservedAvoidancePosition(int32 ObserverID,int32 OtherID,FVector& OutPosition) const
{
 const auto* AvoidStore=GetWorld()->GetSubsystem<UBattleState>();
 const auto* AvoidObserver=AvoidStore->Find(ObserverID);const auto* AvoidOther=AvoidStore->Find(OtherID);
 if(!AvoidObserver)return false;
 if(AvoidOther&&AvoidOther->Team==AvoidObserver->Team)
 {if(!AvoidOther->IsAlive())return false;OutPosition=Position(OtherID);return true;}
 const auto* AvoidContact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ObserverID,OtherID);
 if(!AvoidContact||!AvoidContact->bVisible||AvoidContact->bShared)return false;
 OutPosition=AvoidContact->LastKnownPosition;return true;
}

void UBattleLeaderRuntime::BuildObservedAvoidance(int32 ObserverID,TMap<FIntPoint,int32>& OutGrid,TArray<TArray<int32>>& OutBuckets,TMap<int32,FVector>& OutPositions) const
{
 const auto* AvoidStore=GetWorld()->GetSubsystem<UBattleState>();const auto* AvoidObserver=AvoidStore->Find(ObserverID);if(!AvoidObserver)return;
 const FVector AvoidOrigin=Position(ObserverID);const FIntPoint AvoidCell(FMath::FloorToInt(AvoidOrigin.X/150.),FMath::FloorToInt(AvoidOrigin.Y/150.));
 auto AddObserved=[&](int32 AvoidID,const FVector& AvoidAt)
 {
  const FIntPoint ObservedCell(FMath::FloorToInt(AvoidAt.X/150.),FMath::FloorToInt(AvoidAt.Y/150.));
  if(FMath::Abs(ObservedCell.X-AvoidCell.X)>1||FMath::Abs(ObservedCell.Y-AvoidCell.Y)>1||OutPositions.Contains(AvoidID))return;
  OutPositions.Add(AvoidID,AvoidAt);int32* ObservedBucket=OutGrid.Find(ObservedCell);
  if(!ObservedBucket){const int32 ObservedIndex=OutBuckets.AddDefaulted();ObservedBucket=&OutGrid.Add(ObservedCell,ObservedIndex);}
  OutBuckets[*ObservedBucket].Add(AvoidID);
 };
 // The live grid supplies allies only. Enemy candidates come from the observer's own sensor snapshot, even after unseen death.
 for(int32 AvoidX=-1;AvoidX<=1;++AvoidX)for(int32 AvoidY=-1;AvoidY<=1;++AvoidY)
  if(const int32* AllyBucket=AvoidanceGrid.Find(AvoidCell+FIntPoint(AvoidX,AvoidY)))for(int32 AllyID:AvoidanceBuckets[*AllyBucket])
   if(AllyID!=ObserverID)if(const auto* AvoidAlly=AvoidStore->Find(AllyID);AvoidAlly&&AvoidAlly->Team==AvoidObserver->Team&&AvoidAlly->IsAlive())AddObserved(AllyID,Position(AllyID));
 if(const auto* AvoidContacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ObserverID))
 {
  TArray<int32> ObservedKeys;
  for(const auto& NearbyContact:*AvoidContacts)if(NearbyContact.Value.bVisible&&!NearbyContact.Value.bShared&&FVector::DistSquared2D(AvoidOrigin,NearbyContact.Value.LastKnownPosition)<=FMath::Square(450.))ObservedKeys.Add(NearbyContact.Key);
  ObservedKeys.Sort();
  for(int32 ObservedID:ObservedKeys){const auto& ObservedContact=AvoidContacts->FindChecked(ObservedID);if(ObservedContact.bVisible&&!ObservedContact.bShared)AddObserved(ObservedID,ObservedContact.LastKnownPosition);}
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
 if(!Goal || !Route || B.Waypoint<1 || !Route->IsValidIndex(B.Waypoint))
 {
  if(Goal && FVector::DistSquared2D(Position(ID),*Goal)<FMath::Square(20.))
  {B.bArrived=true;Stop();return true;}
  B.bTrafficWait=true;
  // A reserved destination can survive a failed route build. Retry that route
  // through the squad's existing path budget, keeping its arrival reservation.
  P.bReplan=true;
  if(!Goal)P.ArrivalRevision=INDEX_NONE;
  Stop();return false;
 }
 const FVector Pos=Position(ID);
 // -BattleAllyPass: walking through stopped allies for a few seconds after a hopeless wait (off by default).
 const bool bAllyPass=AllyPassOn()&&Now<B.AllyPassUntil;
 if(!bAllyPass&&B.AllyPassUntil>0)EndAllyPass(ID);
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
 if(SightExperiment.Started && !SightExperiment.Finished)
 {
  if(B.LastMoveSampleFrame+1!=GFrameCounter || FVector::DistSquared2D(Pos,B.MoveSampleOrigin)>FMath::Square(40.))
  {
   B.MoveSampleOrigin=Pos;
   B.StationaryMoveSeconds=0;
  }
  B.LastMoveSampleFrame=GFrameCounter;
  const float Previous=B.StationaryMoveSeconds;
  B.StationaryMoveSeconds+=Dt;
  if(Previous<3.f && B.StationaryMoveSeconds>=3.f)
  {
   UE_LOG(LogTemp,Display,TEXT("BATTLE_MOVE_STALL unit=%d team=%d squad=%d pos=%s waypoint=%s"),ID,U->Team,U->SquadID,*Pos.ToString(),*Way.ToString());
   // Detail (2026-11-20, diagnosis of the map I sector jams): what the soldier walks to, whether the way is clear of walls,
   // and who stands within 1.5m (same squad / other squad of the team / enemy; and how many of them stand still).
   int32 Own=0,Team=0,Enemy=0,Still=0;
   for(const auto& Pair:Bodies)if(Pair.Key!=ID)if(const auto* O=Store->Find(Pair.Key);O&&O->IsAlive()&&FVector::DistSquared2D(Position(Pair.Key),Pos)<FMath::Square(150.))
   {(O->Team!=U->Team?Enemy:O->SquadID==U->SquadID?Own:Team)++;Still+=Pair.Value.Velocity.Size2D()<20;}
   UE_LOG(LogTemp,Display,TEXT("BATTLE_MOVE_STALL_DETAIL unit=%d squad=%d order=%d action=%d sector=%d final=%d wp=%d/%d goal=%s clear=%d own=%d team=%d enemy=%d still=%d cover=%d"),
    ID,U->SquadID,int32(S->Order),int32(S->TacticalPlan.Action),S->bSectorMission?1:0,Final?1:0,B.Waypoint,Route->Num(),Goal?*Goal->ToString():TEXT("-"),
    ClearMovementSegment(Pos,Way)?1:0,Own,Team,Enemy,Still,U->CoverPhase?1:0);
  }
 }
 // Trial -BattleWaypointPass=1 (2026-11-20, default 0): a soldier pushed past a corner of his route by the crowd (within 4m of
 // the corner point, already on the far side towards the next point, and the next point reachable in a straight swept line)
 // goes on to the next point instead of turning back to touch the corner. On map I with sector routes the whole west army
 // queued at one shed corner in G8 (850 stalls a match), a fifth of them past the corner and walking back into the queue.
 static const bool WaypointPass=[](){int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleWaypointPass="),V);return V!=0;}();
 if(WaypointPass && !Final && B.Waypoint+1<Route->Num())
 {
  const FVector& Next=(*Route)[B.Waypoint+1];
  if(FVector::DistSquared2D(Pos,Way)<FMath::Square(400.) && FVector::DistSquared2D(Pos,Way)>=FMath::Square(65.) &&
     FVector::DotProduct((Pos-Way).GetSafeNormal2D(),(Next-Way).GetSafeNormal2D())>0.f && ClearMovementSegment(Pos,Next))
  {++B.Waypoint;Way=Next;Final=B.Waypoint==Route->Num()-1;SightExperiment.Cover(TEXT("waypoint_pass"));}
 }
 if(!Final && FVector::Dist2D(Pos,Way)<65) {++B.Waypoint;Way=(*Route)[B.Waypoint];Final=B.Waypoint==Route->Num()-1;}
 FVector D=(Way-Pos).GetSafeNormal2D(); const FVector Right(-D.Y,D.X,0);
 const bool Confined=NarrowHere;
 const bool bObservedAvoidance=BattleTactics::ObservedTactics();
 TMap<FIntPoint,int32> ObservedGrid;TArray<TArray<int32>> ObservedBuckets;TMap<int32,FVector> ObservedPositions;
 if(bObservedAvoidance)BuildObservedAvoidance(ID,ObservedGrid,ObservedBuckets,ObservedPositions);
 const auto& MovementGrid=bObservedAvoidance?ObservedGrid:AvoidanceGrid;
 const auto& MovementBuckets=bObservedAvoidance?ObservedBuckets:AvoidanceBuckets;
 FVector ObservedPassingAt=FVector::ZeroVector;
 if(bObservedAvoidance&&B.PassingID!=INDEX_NONE&&!ObservedAvoidancePosition(ID,B.PassingID,ObservedPassingAt))B.PassingID=INDEX_NONE;
 if(B.PassingID!=INDEX_NONE)
 {
  const FVector Delta=(bObservedAvoidance?ObservedPassingAt:Position(B.PassingID))-Pos;
  const auto* PassingUnit=Store->Find(B.PassingID);
  const auto* PassingPlan=PassingUnit&&(!bObservedAvoidance||PassingUnit->Team==U->Team)?Plans.Find(PassingUnit->SquadID):nullptr;
  if(Confined && PassingUnit && PassingUnit->Team==U->Team && PassingPlan && PassingPlan->Path.Num()>1 && FVector::DotProduct((PassingPlan->Path.Last()-PassingPlan->Path[0]).GetSafeNormal2D(),D)>.5) B.PassingID=INDEX_NONE;
  if((!bObservedAvoidance&&!PassingUnit->IsAlive()) || FVector::DotProduct(Delta,D)<-80 || Delta.Size2D()>350 ||
     (B.bPassingSecond && FVector::Dist2D(Pos,B.PassingWaypoint)<40)) B.PassingID=INDEX_NONE;
 }
 FVector Separation=FVector::ZeroVector; float Scale=1;
 int32 Blocker=INDEX_NONE; double BlockerDistance=DBL_MAX;
 const FIntPoint Cell(FMath::FloorToInt(Pos.X/150.),FMath::FloorToInt(Pos.Y/150.)); int32 Checked=0;
 for(int32 X=-1;X<=1 && Checked<32;++X) for(int32 Y=-1;Y<=1 && Checked<32;++Y)
  if(const int32* NearbyBucket=MovementGrid.Find(Cell+FIntPoint(X,Y))) for(int32 Other:MovementBuckets[*NearbyBucket])
  {
   if(Other==ID || (!bObservedAvoidance&&!Store->Find(Other)->IsAlive())) continue;
   if(bAllyPass&&(!bObservedAvoidance||(Store->Find(Other)&&Store->Find(Other)->Team==U->Team))&&IsStationaryAlly(ID,Other)) continue;
   if(++Checked>32) break;
   const FVector Delta=(bObservedAvoidance?ObservedPositions.FindChecked(Other):Position(Other))-Pos;
   const double Along=FVector::DotProduct(Delta,D), Side=FVector::DotProduct(Delta,Right), Dist=Delta.Size2D();
   if(Dist>1 && Dist<110) Separation-=Delta.GetSafeNormal2D()*(110-Dist)/110;
   if(Along>0 && Along<155 && FMath::Abs(Side)<76)
   {
    Scale=FMath::Min(Scale,float(FMath::Clamp((Along-78)/70.,0.,1.)));
    const auto* OtherUnit=Store->Find(Other); const auto* OtherPlan=OtherUnit&&OtherUnit->Team==U->Team?Plans.Find(OtherUnit->SquadID):nullptr;
    const bool SameFlow=(!bObservedAvoidance||(OtherUnit&&OtherUnit->Team==U->Team))&&(FVector::DotProduct(Bodies.FindChecked(Other).Velocity.GetSafeNormal2D(),D)>.5 || (OtherPlan && OtherPlan->Path.Num()>1 && FVector::DotProduct((OtherPlan->Path.Last()-OtherPlan->Path[0]).GetSafeNormal2D(),D)>.5));
    // Queue behind a moving ally in a tight corridor instead of overtaking into oncoming traffic.
    if(SameFlow && (Confined || Bodies.FindChecked(Other).Velocity.Size2D()>20)) continue;
    if(Dist<BlockerDistance) {Blocker=Other;BlockerDistance=Dist;}
   }
  }
 if(Blocker!=INDEX_NONE && B.PassingID==INDEX_NONE)
 {
  const FVector BlockerAt=bObservedAvoidance?ObservedPositions.FindChecked(Blocker):Position(Blocker);
  const float Sign=FVector::DotProduct(BlockerAt-Pos,Right)>10?-1.f:1.f;
  for(float SideSign:{Sign,-Sign})
  {
   for(float Distance:{110.f,80.f})
   {
    const FVector Offset=Right*SideSign*Distance, Bypass=(bObservedAvoidance?BlockerAt:Position(Blocker))+Offset+D*20;
    if(!ClearMovementSegment(Pos,Bypass)) continue;
    B.PassingID=Blocker;B.PassingWaypoint=Bypass;if(bObservedAvoidance)ObservedPassingAt=BlockerAt;
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
  {B.PassingWaypoint=(bObservedAvoidance?ObservedPassingAt:Position(B.PassingID))+B.PassingOffset+B.PassingForward*160;B.bPassingSecond=true;}
  if(ClearMovementSegment(Pos,B.PassingWaypoint))
  {D=(B.PassingWaypoint-Pos).GetSafeNormal2D();Separation=FVector::ZeroVector;Scale=FMath::Max(Scale,.6f);}
  else B.PassingID=INDEX_NONE;
 }
 // A stopped cluster can block a two-point pass. Probe at most twelve short,
 // fully swept alternatives once per second, without another NavMesh search.
 if(!bAllyPass && Now>=B.EscapeUntil && Now>=B.NextEscapeProbe && Now-B.LastProgressTime>1)
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
 if(bContactAvoidance && !bAllyPass && (Blocker!=INDEX_NONE || Now-B.LastProgressTime>1 || !B.ContactRoute.IsEmpty() || B.bContactWaiting))
  if(ResolveStoppedContact(ID,Way,Now,D,Scale)) Separation=FVector::ZeroVector;
 // -BattleAllyPass: no detour around the stopped cluster and no progress for 3 s: pass through the stopped allies.
 if(AllyPassOn()&&!bAllyPass&&B.bContactWaiting&&Now-B.LastProgressTime>3)
 {BeginAllyPass(ID,Now);B.bContactWaiting=false;Scale=1;Separation=FVector::ZeroVector;D=(Way-Pos).GetSafeNormal2D();}
 Direction=LimitedSteering(D,Separation,GetDefault<UBattleTuning>()->WideAvoidanceMaxSide);
 B.bTrafficWait=Scale<.15f;
 // Heavy infantry moves slower (squad type speed scale); rifle squads keep the configured speed.
 const float TypeSpeed=S?BattleSquadType::SpeedAt(S->SquadType,GetWorld(),Pos):1.f;
 const float Speed=GetDefault<UBattleTuning>()->MoveSpeed*TypeSpeed;
 if(TypeSpeed!=1.f&&B.Actor.IsValid())B.Actor->GetCharacterMovement()->MaxWalkSpeed=Speed;
 // -BattleStanceMove=2: walking crouched or crawling, at that stance's share of the speed.
 const float StanceScale=BattleStance::MoveMode()>=2?BattleStance::MoveScale(StanceOf(ID)):1.f;
 if(StanceScale<1.f)SightExperiment.Cover(StanceOf(ID)==BattleStance::Prone?TEXT("stance_move_prone_frames"):TEXT("stance_move_crouch_frames"));
 if(B.Actor.IsValid())
 {
  // Vehicles turn at their own rate and drive along the hull.
  if(IsVehicle(ID)&&Scale>.01f)SteerVehicle(ID,Direction,Scale);
  if(Scale<=.01f) Stop(); else B.Actor->AddMovementInput(Direction,Scale*StanceScale);
  B.Velocity=B.Actor->GetVelocity();
 }
 else
 {
  FHitResult Hit; const FVector Before=B.Capsule->GetComponentLocation();
  FVector Step=Direction*FMath::Min(Speed*Dt*Scale*StanceScale,float(FVector::Dist2D(Way,Pos)));
  // Map with heights: the step follows the ground; a place that cannot be stepped onto stops it.
  if(!TerrainStep(Before,Step))Step=FVector::ZeroVector;
  // MoveComponent ignores its owner. Mass capsules share DisplayHost, so explicitly
  // sweep against sibling capsules too, excluding only the moving component.
  auto SweepMove=[&](FVector Delta,FHitResult& Result)
  {
   const FVector From=B.Capsule->GetComponentLocation();
   { TRACE_CPUPROFILER_EVENT_SCOPE(BattleSweepSoldier); SweepSoldier(GetWorld(),B.Capsule.Get(),From,Delta,Result,90,bAllyPass?&B.AllyPassIgnored:nullptr); }
   const float Fraction=Result.bBlockingHit?FMath::Max(0.f,Result.Time-.001f):1.f;
   // Performance (2026-11-17, user decision): the own sweep above already stops at soldiers and the world, so the capsule moves
   // without the second engine sweep (it ignores the shared owner and only sees the world; it never shortened a move by 0.5 cm in
   // 7.5 million moves on map E; tiny position differences change matches; 0.3 ms). -BattleEngineMoveSweep=1 restores it.
   static const bool bEngineSweep=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleEngineMoveSweep="),V);return V!=0;}();
   { TRACE_CPUPROFILER_EVENT_SCOPE(BattleMoveCapsule); B.Capsule->MoveComponent(Delta*Fraction,Direction.Rotation().Quaternion(),bEngineSweep); }
   // Evaluation (2026-11-17, collision cost): what stopped the own sweep (a soldier or the world), and (with the engine sweep)
   // whether the engine's sweep shortened the move further.
   ++MoveSweeps;
   if(Result.bBlockingHit){if(HitIDs.FindRef(Result.GetComponent())>0)++MoveHitSoldier;else ++MoveHitWorld;}
   const double Wanted=(Delta*Fraction).Size2D(),Moved=FVector::Dist2D(From,B.Capsule->GetComponentLocation());
   if(Wanted-Moved>.5)++MoveShortened;
  };
  SweepMove(Step,Hit);
  if(Hit.bBlockingHit)
  {
   FVector Slide=FVector::VectorPlaneProject(Step*(1-Hit.Time),Hit.Normal);
   if(!TerrainStep(B.Capsule->GetComponentLocation(),Slide))Slide=FVector::ZeroVector;
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
   auto* Own=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),Pos,*Goal,NavFor(ID)?Actor(ID):nullptr):nullptr;
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
