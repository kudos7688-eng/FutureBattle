#include "BattleLeaderRuntime.h"
#include "BattleCoverFire.h"
#include "BattleGameMode.h"
#include "BattleTactics.h"
#include "BattleSquadTactics.h"
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
// Called only with -BattleCoverFirePoint=1, after the 100 cm wall test in this same direction.
bool ArrivalFirePoint(UWorld* FireWorld,UNavigationSystemV1* FireNav,const FVector& FireGround,const FVector& FireDirection)
{
 const FCollisionObjectQueryParams FireStatic(ECC_WorldStatic);
 const FVector FireStart=FireGround+FVector(0,0,150);
 if(BattleStance::Enabled(FireWorld) && !FireWorld->LineTraceTestByObjectType(FireStart,FireStart+FireDirection*300,FireStatic) &&
    FireWorld->LineTraceTestByObjectType(FireGround+FVector(0,0,110),FireGround+FireDirection*250+FVector(0,0,110),FireStatic))return true;
 if(!FireNav)return false;
 const FVector FireSide=FVector::CrossProduct(FireDirection,FVector::UpVector);
 for(float FireOffset:{-100.f,100.f,-110.f,110.f,-120.f,120.f})
 {
  FNavLocation FireProjected;if(!FireNav->ProjectPointToNavigation(FireGround+FireSide*FireOffset,FireProjected,FVector(50,50,150)))continue;
  const double FireDistance=FVector::Dist2D(FireGround,FireProjected.Location);if(FireDistance<100.||FireDistance>120.)continue;
  if(FireWorld->SweepTestByObjectType(FireGround+FVector(0,0,90),FireProjected.Location+FVector(0,0,90),FQuat::Identity,FireStatic,FCollisionShape::MakeCapsule(36,88)))continue;
  const FVector FireSideEye=FireProjected.Location+FVector(0,0,150);
  if(!FireWorld->LineTraceTestByObjectType(FireSideEye,FireSideEye+FireDirection*300,FireStatic) &&
     !FireWorld->LineTraceTestByObjectType(FireProjected.Location+FVector(0,0,100),FireProjected.Location+FireDirection*250+FVector(0,0,100),FireStatic))return true;
 }
 return false;
}
// Trial -BattleAllyPass=1 (default 0): see FBattleLeaderBody::AllyPassUntil.
bool AllyPassOn(){static const bool V=[]{int32 X=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAllyPass="),X);return X!=0;}();return V;}
}

bool BattleCoverFire::ArrivalFirePoint(UWorld* FireWorld,UNavigationSystemV1* FireNav,const FVector& FireGround,const FVector& FireDirection)
{
 return ::ArrivalFirePoint(FireWorld,FireNav,FireGround,FireDirection);
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

// -BattleCutOffRoute (default 0): 1 = a member who cannot walk straight to his corridor point gets a route of his own inside
// the squad replan (synchronous query, outside the two-per-frame budget; 2026-11-25 map lane). 2 = B05 (Docs/review-2026-11-25):
// a member's own route (StableMove, two queries per frame) is an execution state of its own that a squad replan keeps. It is
// dropped when the squad's order or destination changes, when the member dies or loses his arrival place, and when a new query
// for it fails or is partial; a changed arrival place is asked for again through the same budget on the last leg.
// 3 = 2, and (a) when only the destination moved on (same order) a cut-off member keeps the walked-to part of his own route
// and joins the new arrival place from the first remaining point that has a straight swept walk to it (no path query);
// a marked member who is no longer behind the wall when his turn in the budget comes is not given a route.
static int32 CutOffRouteMode()
{static const int32 Mode=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCutOffRoute="),V);return V;}();return Mode;}

void UBattleLeaderRuntime::BuildMovementPlan(int32 SID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID); auto& P=Plans.FindChecked(SID);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 const bool Sector=(S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission) ||
  (BattleTactics::SectorSeize()>0 && GetWorld()->GetSubsystem<UBattleSquadTactics>()->SeizingSector(SID)) ||
  (BattleTactics::SectorDefend()>0 && GetWorld()->GetSubsystem<UBattleSquadTactics>()->DefendingSector(SID));
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
  bool bSeizeSupportFacing=false;
  if(BattleTactics::SectorSeizePoint()>0 && BattleTactics::SectorSeizePointSupport()>0)
  {
   FVector SupportFocus;bSeizeSupportFacing=GetWorld()->GetSubsystem<UBattleSquadTactics>()->SeizeSupportFocus(SID,SupportFocus);
   if(bSeizeSupportFacing){P.bFocus=true;P.Focus=SupportFocus;}
  }
  if(Sector && (AttackFocusEnabled() || bSeizeSupportFacing))
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
   if(BattleTactics::SectorSeizePoint()>0 && BattleTactics::SectorSeizePointAT()>0 &&
      !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SeizeEscortPlace(SID,S->Destination+Offset))continue;
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
  TArray<FVector> CoverCandidates,HideOnlyCandidates; TSet<int32> CoverAssigned;
  int32 ArrivalSlotFireChecks=0,ArrivalHuddleFireChecks=0;
  const bool bNearEnemyArrival=CoverNearEnemyEnabled() && Sector && CoverArrival;
  FVector EnemyPoint=FVector::ZeroVector;bool bNearEnemyKnown=false;
  if(bNearEnemyArrival)
  {
   const auto* NearMatch=GetWorld()->GetSubsystem<UBattleMatch>();
   const TCHAR* NearSource=TEXT("arrival_near_enemy_plan_home");
   if(P.bFocus){EnemyPoint=P.Focus;bNearEnemyKnown=true;NearSource=TEXT("arrival_near_enemy_plan_focus");}
   else
   {
    TMap<FIntPoint,int32> NearCounts;
    for(const FVector& KnownPoint:NearMatch->RouteKnownEnemies(S->Team))
    {
     const FIntPoint KnownCell=BattleSectors::Cell(KnownPoint);
     if(KnownCell!=Cell && BattleSectors::Valid(KnownCell) && FMath::Abs(KnownCell.X-Cell.X)<=1 && FMath::Abs(KnownCell.Y-Cell.Y)<=1)
      ++NearCounts.FindOrAdd(KnownCell);
    }
    const int32 NearTurn=NearMatch->AreaHome(S->Team).X>0?-1:1;
    FIntPoint NearBestCell(0,0);int32 NearBestCount=0;double NearBestDistance=0.;
    for(const auto& NearPair:NearCounts)
    {
     const double NearDistance=FVector::DistSquared2D(BattleSectors::Center(NearPair.Key),S->Destination);
     if(!bNearEnemyKnown || NearPair.Value>NearBestCount || (NearPair.Value==NearBestCount &&
      (NearDistance<NearBestDistance || (NearDistance==NearBestDistance && (NearTurn*NearPair.Key.X<NearTurn*NearBestCell.X ||
       (NearPair.Key.X==NearBestCell.X && NearTurn*NearPair.Key.Y<NearTurn*NearBestCell.Y))))))
     {bNearEnemyKnown=true;NearBestCell=NearPair.Key;NearBestCount=NearPair.Value;NearBestDistance=NearDistance;}
    }
    if(bNearEnemyKnown){EnemyPoint=BattleSectors::Center(NearBestCell);NearSource=TEXT("arrival_near_enemy_plan_sector");}
    else
    {
     const FVector EnemyHome=NearMatch->AreaHome(1-S->Team);
     const FVector HomeDirection=(EnemyHome-BattleSectors::Center(Cell)).GetSafeNormal2D();
     const double HomeOctant=FMath::RoundToDouble(FMath::Atan2(HomeDirection.Y,HomeDirection.X)/(PI/4.))*(PI/4.);
     const FIntPoint HomeCell=Cell+FIntPoint(FMath::RoundToInt(FMath::Cos(HomeOctant)),FMath::RoundToInt(FMath::Sin(HomeOctant)));
     EnemyPoint=BattleSectors::Valid(HomeCell)?BattleSectors::Center(HomeCell):S->Destination+(EnemyHome-S->Destination).GetSafeNormal2D()*5000.;
    }
   }
   SightExperiment.Cover(NearSource);
  }
  // Keep the arrival facing/replan contract. Only wall/exposure tests use the local enemy point.
  int32 NearEnemyChecks=0;bool bNearEnemyCapRecorded=false;
  TMap<FVector,bool> NearEnemyHiding;TSet<FVector> NearEnemyUnchecked;
  auto NearEnemyHides=[&](const FVector& NearGround)
  {
   if(!bNearEnemyKnown)return true;
   if(const bool* NearCached=NearEnemyHiding.Find(NearGround))return *NearCached;
   if(NearEnemyChecks>=48)
   {
    NearEnemyUnchecked.Add(NearGround);
    if(!bNearEnemyCapRecorded){SightExperiment.Cover(TEXT("arrival_near_enemy_check_cap"));bNearEnemyCapRecorded=true;}
    return true;
   }
   ++NearEnemyChecks;
   const bool bNearHides=BattleTactics::ExposureAt(GetWorld(),NearGround+FVector(0,0,90),
    (EnemyPoint-NearGround).GetSafeNormal2D(),EnemyPoint+FVector(0,0,60)).VisibleSamples<=2;
   NearEnemyHiding.Add(NearGround,bNearHides);
   if(!bNearHides)SightExperiment.Cover(TEXT("arrival_near_enemy_exposed"));
   return bNearHides;
  };
  auto RecordNearEnemySlot=[&](const FVector& NearSlot)
  {
   SightExperiment.Cover(TEXT("arrival_near_enemy_slot"));
   const double NearMetres=FVector::Dist2D(NearSlot,EnemyPoint)/100.;
   SightExperiment.Cover(NearMetres<30.?TEXT("arrival_near_enemy_dist_lt30m"):NearMetres<60.?TEXT("arrival_near_enemy_dist_30_60m"):
    NearMetres<100.?TEXT("arrival_near_enemy_dist_60_100m"):TEXT("arrival_near_enemy_dist_ge100m"));
  };
  // -BattleCoverOutward: the outward directions this sector is defended toward (rear and friendly-held sides left out).
  TArray<FVector> Outward;
  if(CoverArrival && Sector && CoverOutwardEnabled() && !bFocusArrival && !bNearEnemyArrival)
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
  auto WallAhead=[&](const FVector& At,bool* FireAllowed=nullptr)
  {
   const FCollisionObjectQueryParams Static(ECC_WorldStatic);
   bool AnyWall=false;
   auto DirectionWall=[&](const FVector& WallDir)
   {
    if(!GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,100),At+WallDir*250+FVector(0,0,100),Static))return false;
    AnyWall=true;
    if(!FireAllowed)return true;
    if(!ArrivalFirePoint(GetWorld(),Nav,At,WallDir))return false;
    *FireAllowed=true;return true;
   };
   // -BattleAttackFocus: an obstacle toward the focus from this slot, or 30 degrees either side.
   if(bFocusArrival || bNearEnemyArrival)
    if(const FVector ToFocus=((bNearEnemyArrival?EnemyPoint:P.Focus)-At).GetSafeNormal2D();!ToFocus.IsNearlyZero())
    {
     for(const float Deg:{0.f,-30.f,30.f})
      if(DirectionWall(ToFocus.RotateAngleAxis(Deg,FVector::UpVector)))return true;
     return AnyWall;
    }
   if(Outward.IsEmpty()){DirectionWall(Forward);return AnyWall;}
   for(const FVector& Dir:Outward)if(DirectionWall(Dir))return true;
   return AnyWall;
  };
  if(CoverArrival)
  {
   auto* Cover=GetWorld()->GetSubsystem<UBattleCover>(); Cover->BuildSlots();
   for(int32 Slot:Cover->NearbySlots(S->Destination,Sector?3600.f:1400.f,Sector?(CoverDenseEnabled()?512:256):48))
   {
    const FVector At=Cover->GetSlots()[Slot];
    if(Sector && BattleSectors::Cell(At)!=Cell)continue;
    if(BattleTactics::SectorSeizePoint()>0 && BattleTactics::SectorSeizePointAT()>0 &&
       !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SeizeEscortPlace(SID,At))continue;
    if(BattleTactics::SectorDefend()>0 && !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SectorDefendPlace(SID,At))continue;
    // Evaluation (arrival_slot_*): slots in the sector, those without an obstacle 2.5 m ahead, those seen by the nearest threat (-BattleCoverStrict).
    SightExperiment.Cover(TEXT("arrival_slot_in_cell"));
    if(!WallAhead(At)) SightExperiment.Cover(TEXT("arrival_slot_no_wall_ahead"));
    else if(bNearEnemyArrival?!NearEnemyHides(At):!HidesFromThreat(At))
    {if(!bNearEnemyArrival)SightExperiment.Cover(TEXT("arrival_slot_exposed"));}
    else if(BattleTactics::CoverFirePointEnabled())
    {
     // Bound expensive fire-point searches per plan, after the cheap wall and hiding checks.
     if(ArrivalSlotFireChecks>=48){SightExperiment.Cover(TEXT("arrival_slot_fire_check_cap"));HideOnlyCandidates.Add(At);continue;}
     ++ArrivalSlotFireChecks;
     bool SlotFireAllowed=false;WallAhead(At,&SlotFireAllowed);
     if(SlotFireAllowed)CoverCandidates.Add(At);
     else {SightExperiment.Cover(TEXT("arrival_slot_no_fire_point"));HideOnlyCandidates.Add(At);}
    }
    else CoverCandidates.Add(At);
   }
  }
  // Keep firing cover first, then hiding-only slots in their original distance order.
  const int32 FireCoverSlots=CoverCandidates.Num();
  TMap<FVector,int32> NearEnemyOriginalOrder;
  if(bNearEnemyArrival)
   for(const TArray<FVector>* OriginalTier:{&CoverCandidates,&HideOnlyCandidates})
    for(int32 OriginalIndex=0;OriginalIndex<OriginalTier->Num();++OriginalIndex)
     NearEnemyOriginalOrder.FindOrAdd((*OriginalTier)[OriginalIndex],OriginalIndex);
  // -BattleCoverIndoor: stable roof-first ordering within each tier, never across tiers.
  int32 IndoorSlots=0,HideOnlyIndoorSlots=0;
  if(CoverIndoorEnabled() && Sector && (!CoverCandidates.IsEmpty() || !HideOnlyCandidates.IsEmpty()))
  {
   for(TArray<FVector>* CoverTier:{&CoverCandidates,&HideOnlyCandidates})
   {
    TArray<FVector> Indoor,Outdoor;
    for(const FVector& At:*CoverTier)
     (GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,120),At+FVector(0,0,800),FCollisionObjectQueryParams(ECC_WorldStatic))?Indoor:Outdoor).Add(At);
    if(CoverTier==&CoverCandidates)IndoorSlots=Indoor.Num();else HideOnlyIndoorSlots=Indoor.Num();
    *CoverTier=MoveTemp(Indoor);CoverTier->Append(Outdoor);
   }
   const int32 TotalIndoorSlots=IndoorSlots+HideOnlyIndoorSlots;
   SightExperiment.Cover(TotalIndoorSlots==0?TEXT("arrival_indoor_none"):TotalIndoorSlots<5?TEXT("arrival_indoor_lt5"):TEXT("arrival_indoor_ge5"));
  }
  // Unchecked slots come last even when indoor. Keep actual indoor flags instead of reading the old prefix.
  // Fire/hide tiers never mix; unknowns keep their order from before the indoor partition.
  TArray<bool> NearEnemyIndoor;
  if(bNearEnemyArrival)
  {
   for(TArray<FVector>* NearTier:{&CoverCandidates,&HideOnlyCandidates})
   {
    const int32 NearIndoorCount=NearTier==&CoverCandidates?IndoorSlots:HideOnlyIndoorSlots;
    TArray<int32> NearOrder;
    for(int32 NearIndex=0;NearIndex<NearTier->Num();++NearIndex)NearOrder.Add(NearIndex);
    NearOrder.StableSort([&](int32 NearA,int32 NearB)
    {
     const bool bUnknownA=NearEnemyUnchecked.Contains((*NearTier)[NearA]),bUnknownB=NearEnemyUnchecked.Contains((*NearTier)[NearB]);
     if(bUnknownA!=bUnknownB)return !bUnknownA;
     if(bUnknownA)return NearEnemyOriginalOrder.FindChecked((*NearTier)[NearA])<NearEnemyOriginalOrder.FindChecked((*NearTier)[NearB]);
     const bool bIndoorA=NearA<NearIndoorCount,bIndoorB=NearB<NearIndoorCount;
     if(bIndoorA!=bIndoorB)return bIndoorA;
     return FMath::FloorToDouble(FVector::Dist2D((*NearTier)[NearA],EnemyPoint)/200.)<
      FMath::FloorToDouble(FVector::Dist2D((*NearTier)[NearB],EnemyPoint)/200.);
    });
    TArray<FVector> NearSorted;
    for(int32 NearIndex:NearOrder){NearSorted.Add((*NearTier)[NearIndex]);NearEnemyIndoor.Add(NearIndex<NearIndoorCount);}
    *NearTier=MoveTemp(NearSorted);
   }
  }
  CoverCandidates.Append(HideOnlyCandidates);
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
     const int32 CoverCandidateIndex=int32(&At-CoverCandidates.GetData());
     const bool bFireCoverSlot=CoverCandidateIndex<FireCoverSlots;
     const bool bIndoorSlot=bNearEnemyArrival?NearEnemyIndoor[CoverCandidateIndex]:(bFireCoverSlot?CoverCandidateIndex<IndoorSlots:CoverCandidateIndex-FireCoverSlots<HideOnlyIndoorSlots);
     bCoverTried=true;
     if(TailQueries>=24) {bCapHit=true;break;}
     bool Taken=false; for(const auto& R:Reserved) if(FVector::DistSquared2D(R,At)<FMath::Square(TakenRadius)) {Taken=true;break;}
     if(Taken) {++ArrTaken;continue;}
     ++TailQueries;
     auto* Tail=Nav->FindPathToLocationSynchronously(GetWorld(),P.Path.Num()?P.Path.Last():S->Destination,At);
     if(!Tail || !Tail->IsValid() || Tail->IsPartial() || Tail->PathPoints.Num()<2 || Tail->GetPathLength()>(Sector?6500:2500))
     {if(Tail && Tail->IsValid() && !Tail->IsPartial() && Tail->PathPoints.Num()>=2) ++ArrLong; else ++ArrNoPath; continue;}
     P.ArrivalGoals.Add(ID,At); P.ArrivalTails.Add(ID,Tail->PathPoints); Reserved.Add(At); CoverAssigned.Add(ID); Found=true;
     if(bNearEnemyArrival)RecordNearEnemySlot(At);
     SightExperiment.Cover(TEXT("arrival_cover_slot"));
     if(BattleTactics::CoverFirePointEnabled())SightExperiment.Cover(bFireCoverSlot?TEXT("arrival_cover_slot_fire"):TEXT("arrival_cover_slot_hide_only"));
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
   static const int32 CoverHuddleArg=[](){int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverHuddle="),V);return V;}(); // -1 = by map: on for map I, off elsewhere (BattleFieldLayout::SectorComboSwitch, 2026-11-22 user decision)
   const bool CoverHuddle=BattleFieldLayout::SectorComboSwitch(CoverHuddleArg);
   if(!Found && CoverHuddle && Sector && CoverArrival)
    for(const FVector& At:CoverCandidates)
    {
     // Huddle keeps its original firing-cover bases; hiding-only slots are arrival fallbacks.
     if(Found || TailQueries>=40 || int32(&At-CoverCandidates.GetData())>=FireCoverSlots) break;
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
      if(BattleTactics::SectorSeizePoint()>0 && BattleTactics::SectorSeizePointAT()>0 &&
         !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SeizeEscortPlace(SID,Projected.Location))continue;
      if(BattleTactics::SectorDefend()>0 && !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SectorDefendPlace(SID,Projected.Location))continue;
      if(!GetWorld()->LineTraceTestByObjectType(Projected.Location+FVector(0,0,100),Projected.Location-Normal*250+FVector(0,0,100),FCollisionObjectQueryParams(ECC_WorldStatic)) || (bNearEnemyArrival?!NearEnemyHides(Projected.Location):!HidesFromThreat(Projected.Location))) continue;
      if(BattleTactics::CoverFirePointEnabled())
      {
       if(ArrivalHuddleFireChecks>=40){SightExperiment.Cover(TEXT("arrival_huddle_fire_check_cap"));continue;}
       ++ArrivalHuddleFireChecks;
       bool HuddleFireAllowed=false;WallAhead(Projected.Location,&HuddleFireAllowed);
       if(!HuddleFireAllowed){SightExperiment.Cover(TEXT("arrival_huddle_no_fire_point"));continue;}
      }
      ++TailQueries;
      auto* Tail=Nav->FindPathToLocationSynchronously(GetWorld(),P.Path.Num()?P.Path.Last():S->Destination,Projected.Location);
      if(!Tail || !Tail->IsValid() || Tail->IsPartial() || Tail->PathPoints.Num()<2 || Tail->GetPathLength()>6500) continue;
      P.ArrivalGoals.Add(ID,Projected.Location); P.ArrivalTails.Add(ID,Tail->PathPoints); Reserved.Add(Projected.Location); CoverAssigned.Add(ID); Found=true;
      if(bNearEnemyArrival)RecordNearEnemySlot(Projected.Location);
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
      if(BattleTactics::SectorSeizePoint()>0 && BattleTactics::SectorSeizePointAT()>0 &&
         !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SeizeEscortPlace(SID,Projected.Location))continue;
      if(BattleTactics::SectorDefend()>0 && !GetWorld()->GetSubsystem<UBattleSquadTactics>()->SectorDefendPlace(SID,Projected.Location))continue;
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
 // B05: own routes that outlive this replan (mode 2), and the count of those a replan throws away (every mode; counters only).
 struct FKeptRoute{TArray<FVector> Points;int32 Waypoint=1;};
 TMap<int32,FKeptRoute> KeptRoutes,JoinRoutes;
 for(int32 KeepID:S->Members) if(auto* KeepBody=Bodies.Find(KeepID);KeepBody && KeepBody->bOwnRoute)
 {
  const auto* KeepUnit=Store->Find(KeepID);const auto* OldRoute=P.Corridors.Find(KeepID);
  const bool bSameOrder=KeepBody->OwnRouteOrder==uint8(S->Order) && KeepBody->OwnRouteDestination.Equals(S->Destination,1.);
  const bool bUsable=KeepUnit && KeepUnit->IsAlive() && OldRoute && KeepBody->Waypoint>=1 && OldRoute->IsValidIndex(KeepBody->Waypoint) && P.ArrivalGoals.Contains(KeepID);
  if(CutOffRouteMode()>=2 && bSameOrder && bUsable)
  {FKeptRoute Kept;Kept.Points=*OldRoute;Kept.Waypoint=KeepBody->Waypoint;KeptRoutes.Add(KeepID,MoveTemp(Kept));SightExperiment.Cover(TEXT("move_own_route_kept"));}
  else
  {
   if(CutOffRouteMode()>=3 && bUsable && KeepBody->OwnRouteOrder==uint8(S->Order)){FKeptRoute Old;Old.Points=*OldRoute;Old.Waypoint=KeepBody->Waypoint;JoinRoutes.Add(KeepID,MoveTemp(Old));}
   KeepBody->bOwnRoute=false;SightExperiment.Cover(!bSameOrder?TEXT("move_own_route_dropped_order"):!bUsable?TEXT("move_own_route_dropped_unusable"):TEXT("move_own_route_lost_replan"));
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
  // Trial -BattleCutOffRoute=1 (default 0; made default 1 on 2026-11-25 by user decision and taken back the same day: with all five on, map K had 758-856 stalls a match against 244 and three times the deaths, nd-K-*; hand-over from the supply lane, STATUS 11-25): the shared path starts at the
  // centre of the squad, so a member on the other side of a wall (map K: the yard walls, doorways 12m and more apart) walks
  // straight at its next point and presses against the wall. StableMove gives him a route of his own after 2s, but the squad
  // replan that the pressing itself asks for (4s) resets every corridor here and sends him back to the wall. With the switch a
  // member on foot who cannot walk straight to his corridor point gets his own route to his arrival place at once.
  B->bOwnRouteWanted=false;B->ProgressWayDist=1.e30; // -BattleMoveProgress: a new plan measures progress afresh
  if(FKeptRoute* KeptRoute=KeptRoutes.Find(ID)) {B->Waypoint=KeptRoute->Waypoint;Route=MoveTemp(KeptRoute->Points);}
  // Mode 2: a member on foot who cannot walk straight to his corridor point is cut off by a wall. He does not wait for the 2 s
  // stall (side steps along the wall count as progress and keep resetting it: no own route in 56 s in the fixed scene);
  // StableMove asks for his route through its two-per-frame budget.
  else if(CutOffRouteMode()>=2 && Nav && BattleFieldLayout::IsUrban(GetWorld()) && !NavFor(ID) && Route.IsValidIndex(B->Waypoint) && P.ArrivalGoals.Contains(ID) && !ClearMovementSegment(Position(ID),Route[B->Waypoint]))
  {
   // Mode 3 (a): the destination moved on. Up to eight remaining points of the old own route are tried for a straight walk to the new place.
   int32 JoinAt=INDEX_NONE;FKeptRoute* JoinRoute=JoinRoutes.Find(ID);const FVector JoinGoal=P.ArrivalGoals.FindChecked(ID);
   if(JoinRoute)for(int32 JoinI=JoinRoute->Waypoint;JoinI<JoinRoute->Points.Num()&&JoinI<JoinRoute->Waypoint+8;++JoinI)
    if(ClearMovementSegment(JoinRoute->Points[JoinI],JoinGoal)){JoinAt=JoinI;break;}
   // Pushed off the old leg meanwhile: the old route no longer starts where he stands.
   if(JoinAt!=INDEX_NONE && !ClearMovementSegment(Position(ID),JoinRoute->Points[JoinRoute->Waypoint]))JoinAt=INDEX_NONE;
   if(JoinAt!=INDEX_NONE)
   {
    JoinRoute->Points.SetNum(JoinAt+1);JoinRoute->Points.Add(JoinGoal);B->Waypoint=JoinRoute->Waypoint;Route=MoveTemp(JoinRoute->Points);
    B->bOwnRoute=true;B->OwnRouteOrder=uint8(S->Order);B->OwnRouteDestination=S->Destination;B->OwnRouteGoal=JoinGoal;SightExperiment.Cover(TEXT("move_own_route_joined"));
   }
   else{B->bOwnRouteWanted=true;SightExperiment.Cover(TEXT("move_own_route_wanted"));if(JoinRoute)SightExperiment.Cover(TEXT("move_own_route_join_failed"));}
  }
  else if(CutOffRouteMode()==1 && Nav && BattleFieldLayout::IsUrban(GetWorld()) && !NavFor(ID) && Route.IsValidIndex(B->Waypoint) && !ClearMovementSegment(Position(ID),Route[B->Waypoint]))
   if(const FVector* OwnGoal=P.ArrivalGoals.Find(ID))
    if(auto* OwnPath=Nav->FindPathToLocationSynchronously(GetWorld(),Position(ID),*OwnGoal);OwnPath && OwnPath->IsValid() && !OwnPath->IsPartial() && OwnPath->PathPoints.Num()>1)
    {
     TArray<FVector> OwnRoute;for(const auto& OwnPoint:OwnPath->PathPoints)OwnRoute.Add(OwnPoint);
     Route=MoveTemp(OwnRoute);B->Waypoint=1;SightExperiment.Cover(TEXT("move_cutoff_route"));
    }
  if(BattleTactics::MoveTraceSquad(SID))
  {
   FString TraceRoute;for(int32 TraceI=0;TraceI<Route.Num()&&TraceI<5;++TraceI)TraceRoute+=FString::Printf(TEXT(" (%.1f,%.1f)"),Route[TraceI].X,Route[TraceI].Y);
   const FVector* TraceGoal=P.ArrivalGoals.Find(ID);
   UE_LOG(LogTemp,Display,TEXT("BATTLE_MOVE_TRACE_ROUTE squad=%d unit=%d new=%d wp=%d/%d pos=(%.1f,%.1f) goal=(%.1f,%.1f) route:%s"),SID,ID,NewOrder?1:0,B->Waypoint,Route.Num(),Position(ID).X,Position(ID).Y,TraceGoal?TraceGoal->X:0.,TraceGoal?TraceGoal->Y:0.,*TraceRoute);
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
 // Records only (2026-11-25, user: "the problem is a soldier who tries to move and cannot; one who waits in place is fine"):
 // BATTLE_MOVE_STALL_WHY tells whether the stalled soldier pushed on (scale >= 0.15) or held back by the queue rule behind an
 // ally (scale < 0.15), BATTLE_MOVE_STALL_END how long the stall lasted (moved=0: he left the moving state instead).
 bool bStallLogged=false;
 if(SightExperiment.Started && !SightExperiment.Finished)
 {
  if(B.LastMoveSampleFrame+1!=GFrameCounter || FVector::DistSquared2D(Pos,B.MoveSampleOrigin)>FMath::Square(40.))
  {
   if(B.StationaryMoveSeconds>=3.f)UE_LOG(LogTemp,Display,TEXT("BATTLE_MOVE_STALL_END unit=%d seconds=%.1f moved=%d"),ID,B.StationaryMoveSeconds,B.LastMoveSampleFrame+1==GFrameCounter?1:0);
   B.MoveSampleOrigin=Pos;
   B.StationaryMoveSeconds=0;
  }
  B.LastMoveSampleFrame=GFrameCounter;
  const float Previous=B.StationaryMoveSeconds;
  B.StationaryMoveSeconds+=Dt;
  if(Previous<3.f && B.StationaryMoveSeconds>=3.f)
  {
   bStallLogged=true;
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
 // -BattleWaypointPass (2026-11-20; default 1 on every map since 2026-11-22, user decision after roadmap stage 1; =0 for the old rule): a soldier pushed past a corner of his route by the crowd (within 4m of
 // the corner point, already on the far side towards the next point, and the next point reachable in a straight swept line)
 // goes on to the next point instead of turning back to touch the corner. On map I with sector routes the whole west army
 // queued at one shed corner in G8 (850 stalls a match), a fifth of them past the corner and walking back into the queue.
 static const bool WaypointPass=[](){int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleWaypointPass="),V);return V!=0;}();
 if(WaypointPass && !Final && B.Waypoint+1<Route->Num())
 {
  const FVector& Next=(*Route)[B.Waypoint+1];
  if(FVector::DistSquared2D(Pos,Way)<FMath::Square(400.) && FVector::DistSquared2D(Pos,Way)>=FMath::Square(65.) &&
     FVector::DotProduct((Pos-Way).GetSafeNormal2D(),(Next-Way).GetSafeNormal2D())>0.f && ClearMovementSegment(Pos,Next))
  {++B.Waypoint;Way=Next;Final=B.Waypoint==Route->Num()-1;SightExperiment.Cover(TEXT("waypoint_pass"));}
 }
// Trial -BattleWaypointCrowd=1 (2026-11-25, default 0; user request: fewer soldiers getting stuck): every soldier has to come within
 // 65cm of each corner point of his route, and the squads of a group share those points. At the town entrance of map K some
 // 150 soldiers pushed at one point of the road (g9b-K-base: 400 of 783 stalls within 5m of it, 64 in 100 of them walking on
 // against bodies). With the switch a soldier who has not got on for 1.5s within 8m of a corner point goes on to the next
 // point when he can walk to it in a straight swept line - also from the near side of the corner (WaypointPass asks for the
 // far side and 4m).
 static const bool WaypointCrowd=[](){int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleWaypointCrowd="),V);return V!=0;}();
 if(WaypointCrowd && !Final && B.Waypoint+1<Route->Num() && Now-B.LastProgressTime>1.5 && FVector::DistSquared2D(Pos,Way)<FMath::Square(800.))
  if(const FVector& CrowdNext=(*Route)[B.Waypoint+1];ClearMovementSegment(Pos,CrowdNext))
  {++B.Waypoint;Way=CrowdNext;Final=B.Waypoint==Route->Num()-1;B.LastProgressTime=Now;SightExperiment.Cover(TEXT("waypoint_crowd_skip"));}
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
 if(BattleTactics::MoveTraceSquad(U->SquadID))if(const auto* TraceMatch=GetWorld()->GetSubsystem<UBattleMatch>();TraceMatch&&TraceMatch->Elapsed<4.f)
  UE_LOG(LogTemp,Display,TEXT("BATTLE_MOVE_TRACE_STEP unit=%d t=%.3f pos=(%.1f,%.1f) wp=%d way=(%.1f,%.1f) d=(%.3f,%.3f) scale=%.3f blocker=%d passing=%d sep=(%.3f,%.3f) dir=(%.3f,%.3f)"),ID,TraceMatch->Elapsed,Pos.X,Pos.Y,B.Waypoint,Way.X,Way.Y,D.X,D.Y,Scale,Blocker,B.PassingID,Separation.X,Separation.Y,Direction.X,Direction.Y);
 B.bTrafficWait=Scale<.15f;
 if(bStallLogged)UE_LOG(LogTemp,Display,TEXT("BATTLE_MOVE_STALL_WHY unit=%d scale=%.2f blocker=%d passing=%d escape=%d contact_wait=%d"),ID,Scale,Blocker,B.PassingID,Now<B.EscapeUntil?1:0,B.bContactWaiting?1:0);
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
 // Trial -BattleMoveProgress=1 (2026-11-25, default 0; B05 follow-up): progress is getting 40cm closer to the current route point
 // (or a new route point), not any 40cm. The side steps along a wall (escape probe, 120cm) counted as progress, so a member
 // pressed against a wall never reached the 2 s without progress that gives him a route of his own (fixed scene: none in 56 s).
 static const bool bWayProgress=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleMoveProgress="),V);return V!=0;}();
 if(bWayProgress)
 {
  // Measured against the fixed route point (the steering target Way moves in narrow corridors and at the last leg). While he passes
  // an ally, follows a contact detour or walks through allies, any 40cm still counts: those moves lead away from the point on purpose.
  const FVector RoutePoint=(*Route)[B.Waypoint];const double ToWay=FVector::Dist2D(Pos,RoutePoint);
  const bool bDetour=B.PassingID!=INDEX_NONE || !B.ContactRoute.IsEmpty() || bAllyPass;
  const bool bSamePoint=RoutePoint.Equals(B.ProgressWay,1.);
  if(!bSamePoint || ToWay<B.ProgressWayDist-40. || (bDetour && FVector::Dist2D(Pos,B.LastProgressPosition)>40))
  {B.ProgressWay=RoutePoint;B.ProgressWayDist=bSamePoint?FMath::Min(ToWay,B.ProgressWayDist):ToWay;B.LastProgressPosition=Pos;B.LastProgressTime=Now;B.StalledSince=0;}
  else if(Now-B.LastProgressTime>1) {B.StalledSince=B.LastProgressTime;B.bTrafficWait=true;}
 }
 else if(FVector::Dist2D(Pos,B.LastProgressPosition)>40) {B.LastProgressPosition=Pos;B.LastProgressTime=Now;B.StalledSince=0;}
 else if(Now-B.LastProgressTime>1) {B.StalledSince=B.LastProgressTime;B.bTrafficWait=true;}
 // Map E: a member cut off from the shared corridor by walls gets a route of his own to his arrival spot, instead of
 // waiting for a squad replan that starts from the squad centre and does not help him. At most two such queries per frame.
 // B05 (mode 2): an own route kept through replans ends at the place it was built for; when the member's arrival place has moved
 // since and cannot be walked to straight from the last leg, ask again (same budget, same 3 s spacing).
 if(CutOffRouteMode()>=3 && B.bOwnRouteWanted && Now>=B.NextOwnPath && ClearMovementSegment(Pos,Way)){B.bOwnRouteWanted=false;SightExperiment.Cover(TEXT("move_own_route_wanted_cleared"));}
 const bool bOwnGoalMoved=CutOffRouteMode()>=2 && B.bOwnRoute && Now>=B.NextOwnPath && Final && !Goal->Equals(B.OwnRouteGoal,50.) && !ClearMovementSegment(Pos,*Goal);
 if(BattleFieldLayout::IsUrban(GetWorld()) && Now>=B.NextOwnPath && (bOwnGoalMoved || B.bOwnRouteWanted || (Now-B.LastProgressTime>2 && !ClearMovementSegment(Pos,Way))))
 {
  static uint64 QueryFrame=0; static int32 QueriesThisFrame=0;
  if(QueryFrame!=GFrameCounter) {QueryFrame=GFrameCounter;QueriesThisFrame=0;}
  if(QueriesThisFrame<2)
  {
   ++QueriesThisFrame; ++B.OwnRouteQueries; B.NextOwnPath=Now+3; B.bOwnRouteWanted=false;
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   auto* Own=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),Pos,*Goal,NavFor(ID)?Actor(ID):nullptr):nullptr;
   if(Own && Own->IsValid() && Own->PathPoints.Num()>1)
   {
    TArray<FVector> OwnRoute; for(const auto& Point:Own->PathPoints) OwnRoute.Add(Point);
    P.Corridors.Add(ID,MoveTemp(OwnRoute)); B.Waypoint=1; B.LastProgressTime=Now; B.StalledSince=0;
    SightExperiment.Cover(TEXT("move_own_route"));
    if(bOwnGoalMoved)SightExperiment.Cover(TEXT("move_own_route_goal_requery"));
    // B05: a complete route is the member's own execution state (a partial one ends short of his place and is not kept).
    B.bOwnRoute=!Own->IsPartial() && S;
    if(B.bOwnRoute){B.OwnRouteOrder=uint8(S->Order);B.OwnRouteDestination=S->Destination;B.OwnRouteGoal=*Goal;}
    // A partial answer for a moved arrival place would leave him at its end with nothing asking again: back to the squad plan.
    else if(bOwnGoalMoved){P.bReplan=true;SightExperiment.Cover(TEXT("move_own_route_goal_replan"));}
    return true;
   }
   if(B.bOwnRoute){B.bOwnRoute=false;SightExperiment.Cover(TEXT("move_own_route_dropped_no_path"));}
   if(bOwnGoalMoved){P.bReplan=true;SightExperiment.Cover(TEXT("move_own_route_goal_replan"));}
  }
  else SightExperiment.Cover(TEXT("move_own_route_budget_wait"));
 }
 // Congestion is not a navigation failure: do not reset all ten paths every two seconds.
 if(B.StalledSince>0 && Now-B.LastProgressTime>4 && !ClearMovementSegment(Pos,Way) && Now>=P.NextPath)
 {P.bReplan=true;P.NextPath=Now+1;B.LastProgressTime=Now;SightExperiment.Cover(TEXT("move_stall_replan"));}
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
