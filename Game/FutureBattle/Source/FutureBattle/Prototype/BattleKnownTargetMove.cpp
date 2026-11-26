// Level 2 only: observation-ranked short paths. No enemy bodies are read by planning.
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleCover.h"
#include "BattleRanges.h"
#include "BattleSectors.h"
#include "BattleMatch.h"
#include "BattleStance.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool BattleKnownFire::MoveEnabled(){static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownTargetFire="),V);return V==2;}();return On;}
bool BattleKnownFire::MoveMarch(){static const bool On=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveMarch="),V);return V!=0;}();return On;}
double BattleKnownFire::MoveRange(){static const double Range=[]{double V=2500.;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveRange="),V);return FMath::Clamp(V,0.,25000.);}();return Range;}
double BattleKnownFire::MoveSquadRadius(){static const double Radius=[]{double V=5000.;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveSquadRadius="),V);return FMath::Clamp(V,0.,50000.);}();return Radius;}
int32 BattleKnownFire::MoveMax(){static const int32 Maximum=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveMax="),V);return FMath::Max(0,V);}();return Maximum;}

bool UBattleLeaderRuntime::KnownMoveMarching(int32 ID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;const auto* Plan=Unit?Plans.Find(Unit->SquadID):nullptr;
 return Squad && Plan && Squad->Order==EBattleOrder::AttackMove && !Plan->bCoverStop &&
  Squad->TacticalPlan.Action!=EBattleSquadTactic::Engage && FVector::DistSquared2D(Position(ID),Unit->Destination)>FMath::Square(100.);
}

bool UBattleLeaderRuntime::KnownMoveAllowed(int32 ID,bool CountGate)
{
 if(!BattleKnownFire::MoveEnabled())return false;
 auto RejectGate=[&](const TCHAR* Reason){if(CountGate)SightExperiment.Cover(Reason);return false;};
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;const auto* Body=Bodies.Find(ID);
 if(!Unit || !Unit->IsAlive() || !Squad || !Body || Unit->Team<0 || Unit->Team>1)return RejectGate(TEXT("known_move_gate_invalid"));
 if(Unit->bSoldierOrder || Unit->Order==EBattleOrder::Hold || Unit->Order==EBattleOrder::Move ||
  Squad->Order==EBattleOrder::Hold || Squad->Order==EBattleOrder::Move || Squad->CommandStatus!=EBattleCommandStatus::Active)return RejectGate(TEXT("known_move_gate_order"));
 if(Squad->TacticalPlan.Action==EBattleSquadTactic::Retreat)return RejectGate(TEXT("known_move_gate_retreat"));
 if(Squad->TacticalPlan.Action==EBattleSquadTactic::Recover)return RejectGate(TEXT("known_move_gate_supply"));
 if(Squad->TacticalPlan.Action==EBattleSquadTactic::Regroup)return RejectGate(TEXT("known_move_gate_regroup"));
 if(Squad->TacticalPlan.Action!=EBattleSquadTactic::Mission && Squad->TacticalPlan.Action!=EBattleSquadTactic::Engage)return RejectGate(TEXT("known_move_gate_tactic"));
 if(ArmsDashing(ID))return RejectGate(TEXT("known_move_gate_charge"));
 if(IsVehicle(ID) || Unit->bLauncherSelected || (Unit->ActiveWeapon()!=EBattleWeapon::Carbine && Unit->ActiveWeapon()!=EBattleWeapon::SupportGun))return RejectGate(TEXT("known_move_gate_weapon"));
 if(Body->Actor.IsValid() && Body->Actor->IsPlayerControlled())return RejectGate(TEXT("known_move_gate_player"));
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const auto* Group=Match->Groups.Find(Match->GroupOf(Unit->SquadID));
 return !Group || Group->Phase!=0 || RejectGate(TEXT("known_move_gate_assembly"));
}

bool UBattleLeaderRuntime::KnownMovePlaceAllowed(int32 ID,FVector Start,FVector Place) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;const auto* Plan=Unit?Plans.Find(Unit->SquadID):nullptr;
 if(!Squad || !Plan || Place.ContainsNaN() || FVector::DistSquared2D(Start,Place)>FMath::Square(BattleKnownFire::MoveRange()) ||
  FVector::DistSquared2D(Plan->Center,Place)>FMath::Square(BattleKnownFire::MoveSquadRadius()))return false;
 // Freeze the departure cell for the episode; the squad radius still follows its live centre.
 const auto* Episode=KnownMoves.Find(ID);
 const bool March=BattleKnownFire::MoveMarch() && (Episode?Episode->bMarch:KnownMoveMarching(ID));
 if(Squad->bSectorMission && BattleSectors::Cell(Place)!=BattleSectors::Cell(Squad->Destination) &&
  !(March && BattleSectors::Cell(Place)==BattleSectors::Cell(Start)))return false;
 if(Squad->Order==EBattleOrder::Defend && FVector::DistSquared2D(Place,Squad->Destination)>FMath::Square(double(Squad->DefenseRadius)))return false;
 return true;
}

// Kind 0: actual ray (also charged to stage 1), 1: sweep, 2: navigation projection/path query.
// FIFO persists across the one-second search interval. Geometry may defer; it never grants a clear line on exhaustion.
bool UBattleLeaderRuntime::KnownMoveSpend(int32 ID,double Now,uint8 Kind)
{
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!Unit || Unit->Team<0 || Unit->Team>1)return false;
 auto& Budget=KnownMoveBudgets[Unit->Team];const int64 Window=FMath::FloorToInt64(Now/.1);
 if(Budget.Window!=Window){Budget.Window=Window;Budget.Rays=0;Budget.Sweeps=0;Budget.Nav=0;}
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(int32 Index=Budget.Waiting.Num()-1;Index>=0;--Index)
 {
  const int32 WaitID=Budget.Waiting[Index];const auto* WaitUnit=Store->Find(WaitID);
  if(!WaitUnit || !WaitUnit->IsAlive() || Now-Budget.Asked.FindRef(WaitID)>1.5)
  {Budget.Waiting.RemoveAt(Index);Budget.Asked.Remove(WaitID);}
 }
 int32& Used=Kind==0?Budget.Rays:Kind==1?Budget.Sweeps:Budget.Nav;
 if(Used>=16 || (Budget.Waiting.Num() && Budget.Waiting[0]!=ID) || (Kind==0 && !KnownFireRay(ID,Now)))
 {
  Budget.Waiting.AddUnique(ID);Budget.Asked.Add(ID,Now);bKnownMoveDeferred=true;SightExperiment.Cover(TEXT("known_move_budget_deferred"));SightExperiment.Cover(Kind==0?TEXT("known_move_reject_budget_ray"):Kind==1?TEXT("known_move_reject_budget_sweep"):TEXT("known_move_reject_budget_nav"));return false;
 }
 ++Used;SightExperiment.Cover(Kind==0?TEXT("known_move_rays"):Kind==1?TEXT("known_move_sweeps"):TEXT("known_move_nav"));return true;
}

bool UBattleLeaderRuntime::KnownMoveLine(int32 ID,FVector From,FVector To,double Now)
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(KnownMoveLine),false);FHitResult Hit;
 for(int32 Pass=0;Pass<=8;++Pass)
 {
  if(!KnownMoveSpend(ID,Now,0))return false;
  GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Query);
  if(!Hit.bBlockingHit)return true;
  if(!Hit.GetComponent() || Hit.GetComponent()->GetCollisionObjectType()!=ECC_Pawn){SightExperiment.Cover(TEXT("known_move_reject_line_terrain"));return false;}
  Query.AddIgnoredComponent(Hit.GetComponent());Hit=FHitResult();
 }
 SightExperiment.Cover(TEXT("known_move_reject_line_pawn_limit"));return false;
}

bool UBattleLeaderRuntime::StartKnownMove(int32 ID,double Now)
{
 if(!KnownMoveAllowed(ID,true))return false;
 if(KnownMoves.Contains(ID) || CoverApproaches.Contains(ID)){SightExperiment.Cover(TEXT("known_move_gate_active"));return false;}
 if(Now<KnownMoveAfter.FindRef(ID)){SightExperiment.Cover(TEXT("known_move_gate_cooldown"));return false;}
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Unit=Store->Units.Find(ID);
 if(Unit->CoverPhase==1 || Unit->CoverPhase==3){SightExperiment.Cover(TEXT("known_move_gate_cover"));return false;}
 if(Unit->Ammo<=0 || Unit->ReloadEndTime>0 || Now<Unit->NextShotTime){SightExperiment.Cover(TEXT("known_move_gate_ammo"));return false;}
 const auto* Squad=Store->FindSquad(Unit->SquadID);const auto* Plan=Plans.Find(Unit->SquadID);if(!Plan){SightExperiment.Cover(TEXT("known_move_gate_plan"));return false;}
 // March=0 retains the original in-flight gate. The caller also retains its route/arrival gate.
 if(!BattleKnownFire::MoveMarch() && Squad->Order==EBattleOrder::AttackMove && Plan->Path.IsValidIndex(Bodies.FindChecked(ID).Waypoint) &&
  !Plan->bCoverStop && Squad->TacticalPlan.Action!=EBattleSquadTactic::Engage && FVector::DistSquared2D(Position(ID),Unit->Destination)>FMath::Square(100.)){SightExperiment.Cover(TEXT("known_move_gate_march"));return false;}
 const bool March=BattleKnownFire::MoveMarch() && KnownMoveMarching(ID);
 KnownMoveAfter.Add(ID,Now+1.);
 int32 Living=0,Moving=0;
 for(int32 Member:Squad->Members)if(const auto* Other=Store->Find(Member);Other && Other->IsAlive())
 {++Living;if(Member!=ID && (Other->CoverPhase==1 || CoverApproaches.Contains(Member)))++Moving;}
 const int32 Limit=BattleKnownFire::MoveMax()>0?FMath::Min(Living/2,BattleKnownFire::MoveMax()):Living/2;
 if(Moving>=Limit){SightExperiment.Cover(TEXT("known_move_squad_busy"));SightExperiment.Cover(TEXT("known_move_gate_members"));return false;}
 const FVector Start=Position(ID);const auto& Team=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];
 const double Reach=BattleRanges::Engage(Unit->Weapon),SearchRange=Reach+BattleKnownFire::MoveRange();
 TArray<int32> Observed,Candidates;
 const FIntPoint Low(FMath::FloorToInt((Start.X-SearchRange)/5000.),FMath::FloorToInt((Start.Y-SearchRange)/5000.));
 const FIntPoint High(FMath::FloorToInt((Start.X+SearchRange)/5000.),FMath::FloorToInt((Start.Y+SearchRange)/5000.));
 for(int32 X=Low.X;X<=High.X;++X)for(int32 Y=Low.Y;Y<=High.Y;++Y)
  if(const auto* Bucket=Team.Grid.Find(FIntPoint(X,Y)))for(int32 TargetID:*Bucket)
  {
   const auto& Target=Team.Targets.FindChecked(TargetID);if(!Target.Fresh(Now))continue;Observed.Add(TargetID);
   if((!March || FVector::DistSquared(Start,Target.At(Now))<=FMath::Square(Reach)) && WeaponOrderAllows(ID,TargetID,Target.At(Now)))Candidates.Add(TargetID);
  }
 Candidates.Sort([&](int32 Left,int32 Right){const double L=FVector::DistSquared2D(Start,Team.Targets.FindChecked(Left).At(Now)),R=FVector::DistSquared2D(Start,Team.Targets.FindChecked(Right).At(Now));return L==R?Left<Right:L<R;});
 if(Candidates.Num()>2)Candidates.SetNum(2,EAllowShrinking::No);
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if(Candidates.IsEmpty()){SightExperiment.Cover(TEXT("known_move_gate_no_candidate"));return false;}
 if(!Nav || !Cover->IsBuilt()){SightExperiment.Cover(TEXT("known_move_reject_setup"));SightExperiment.Cover(TEXT("known_move_gate_setup"));return false;} // slot prebuild is not hidden work in a per-soldier budget
 SightExperiment.Cover(TEXT("known_move_searches"));
 bKnownMoveDeferred=false;
 const auto& EntryBudget=KnownMoveBudgets[Unit->Team];
 const int32 SpentBefore=EntryBudget.Window==FMath::FloorToInt64(Now/.1)?EntryBudget.Rays+EntryBudget.Sweeps+EntryBudget.Nav:0;
 auto FinishBudget=[&]()
 {
  auto& MoveBudget=KnownMoveBudgets[Unit->Team];
  if(!bKnownMoveDeferred){MoveBudget.Waiting.Remove(ID);MoveBudget.Asked.Remove(ID);}
  else if(MoveBudget.Rays+MoveBudget.Sweeps+MoveBudget.Nav>SpentBefore)
  {
   // A serviced search yields to the next soldier even if its geometry exhausted this slice.
   MoveBudget.Waiting.Remove(ID);MoveBudget.Waiting.Add(ID);
  }
  KnownFireBudgets[Unit->Team].Waiting.Remove(ID);KnownFireBudgets[Unit->Team].RequestedAt.Remove(ID);
 };
 struct FPlace {FVector At;int32 Slot;};TArray<FPlace> Places;
 for(int32 Slot:Cover->NearbySlots(Start,float(BattleKnownFire::MoveRange()),4))Places.Add({Cover->GetSlots()[Slot]+FVector(0,0,90),Slot});
 // Two free positions per search, at most 5 m sideways, without random draws.
 const int32 Turn=KnownMoveCursor.FindRef(ID);
 const FVector Side=FVector::CrossProduct((Team.Targets.FindChecked(Candidates[0]).At(Now)-Start).GetSafeNormal2D(),FVector::UpVector);
 const double Radius=FMath::Min(BattleKnownFire::MoveRange(),500.);
 Places.Add({Start+Side*Radius,INDEX_NONE});Places.Add({Start-Side*Radius,INDEX_NONE});
 int32 PlaceCount=0;
 for(int32 PlaceIndex=0;PlaceIndex<Places.Num();++PlaceIndex)
 {
  if(bKnownMoveDeferred)break;
  const int32 PlaceAt=(Turn+PlaceIndex)%Places.Num();const FPlace& Place=Places[PlaceAt];
  KnownMoveCursor.Add(ID,(PlaceAt+1)%Places.Num());
  if(++PlaceCount>6)break;
  FVector Anchor=Place.At;const bool Free=Place.Slot==INDEX_NONE;
  SightExperiment.Cover(Free?TEXT("known_move_candidate_free"):TEXT("known_move_candidate_slot"));
  if(!KnownMovePlaceAllowed(ID,Start,Anchor)){SightExperiment.Cover(TEXT("known_move_reject_anchor_mission"));continue;}
  if(FVector::Dist2D(Start,Anchor)<150.){SightExperiment.Cover(TEXT("known_move_reject_too_close"));continue;}
  if(!Free && !Cover->IsAvailable(Place.Slot,ID,Now)){SightExperiment.Cover(TEXT("known_move_reject_slot_busy"));continue;}
  bool Occupied=false;
  for(const auto& Walk:CoverApproaches)if(Walk.Key!=ID && ((Walk.Value.Slot==Place.Slot && !Free) || FVector::DistSquared2D(Walk.Value.Goal,Anchor)<FMath::Square(120.))){Occupied=true;break;}
  if(!Occupied)for(const auto& Settled:FirePositions)if(Settled.Key!=ID && FVector::DistSquared2D(Settled.Value,Anchor)<FMath::Square(120.)){Occupied=true;break;}
  if(!Occupied)for(const auto& Friendly:Store->GetUnits())if(Friendly.Key!=ID && Friendly.Value.Team==Unit->Team && Friendly.Value.IsAlive() && FVector::DistSquared2D(Position(Friendly.Key),Anchor)<FMath::Square(120.)){Occupied=true;break;}
  if(Occupied){SightExperiment.Cover(TEXT("known_move_reject_occupied"));continue;}
  if(!KnownMoveSpend(ID,Now,2))break;
  FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor-FVector(0,0,90),Projected,FVector(50,50,150))){SightExperiment.Cover(TEXT("known_move_reject_anchor_nav"));continue;}
  Anchor=Projected.Location+FVector(0,0,90);if(!KnownMovePlaceAllowed(ID,Start,Anchor)){SightExperiment.Cover(TEXT("known_move_reject_projected_mission"));continue;}
  for(int32 TargetID:Candidates)
  {
   if(bKnownMoveDeferred)break;
   const auto& Target=Team.Targets.FindChecked(TargetID);const FVector Threat=Target.At(Now);FVector Peek=Anchor;uint8 Kind=0;
   if(FVector::DistSquared2D(Anchor,Threat)>FMath::Square(Reach)){SightExperiment.Cover(TEXT("known_move_reject_anchor_range"));continue;}
   // Conservative observed danger: no approach toward ANY nearby known enemy, including enemies forbidden by the mission.
   auto Safe=[&](FVector Point)
   {for(int32 ThreatID:Observed){const FVector Seen=Team.Targets.FindChecked(ThreatID).At(Now);if(FVector::DistSquared2D(Point,Seen)+1.<FVector::DistSquared2D(Start,Seen))return false;}return true;};
   if(!Safe(Anchor)){SightExperiment.Cover(TEXT("known_move_reject_anchor_danger"));continue;}
   bool MoreExposed=false,StandUp=false;
   for(int32 DangerID:Candidates)
   {
    const FVector DangerEye=Team.Targets.FindChecked(DangerID).At(Now)+FVector(0,0,60);
    const bool HereVisible=KnownMoveLine(ID,Start+FVector(0,0,30),DangerEye,Now);
    const bool ThereVisible=KnownMoveLine(ID,Anchor+FVector(0,0,30),DangerEye,Now);
    static const bool MoveTrialDiag=FParse::Param(FCommandLine::Get(),TEXT("BattleKnownTargetFireTrial"));
    if(SightExperiment.Enabled && MoveTrialDiag)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_EXPOSURE unit=%d slot=%d start=%s raw=%s anchor=%s threat_eye=%s here=%d there=%d deferred=%d"),ID,Place.Slot,*Start.ToCompactString(),*Place.At.ToCompactString(),*Anchor.ToCompactString(),*DangerEye.ToCompactString(),HereVisible,ThereVisible,bKnownMoveDeferred);
    if(DangerID==TargetID)StandUp=!ThereVisible;
    if(bKnownMoveDeferred || (ThereVisible && !HereVisible)){MoreExposed=true;break;}
   }
   if(MoreExposed){SightExperiment.Cover(bKnownMoveDeferred?TEXT("known_move_reject_exposure_deferred"):TEXT("known_move_reject_more_exposed"));continue;}
   const bool CanFire=Free?KnownMoveLine(ID,Anchor+FVector(0,0,60),Threat+FVector(0,0,60),Now):CoverFirePoint(ID,Anchor,Threat,Peek,Kind,3,false,Now);
   if(bKnownMoveDeferred){SightExperiment.Cover(TEXT("known_move_reject_fire_deferred"));continue;}
   if(!CanFire){SightExperiment.Cover(Free?TEXT("known_move_reject_free_fire_line"):TEXT("known_move_reject_cover_fire_point"));continue;}
   if(!KnownMovePlaceAllowed(ID,Start,Peek)){SightExperiment.Cover(TEXT("known_move_reject_peek_mission"));continue;}
   if(!Safe(Peek)){SightExperiment.Cover(TEXT("known_move_reject_peek_danger"));continue;}
   if(FVector::DistSquared2D(Peek,Threat)>FMath::Square(Reach)){SightExperiment.Cover(TEXT("known_move_reject_peek_range"));continue;}
   SightExperiment.Cover(Free?TEXT("known_move_fire_free"):Kind==1?TEXT("known_move_fire_over"):TEXT("known_move_fire_lean"));
   if(!KnownMoveSpend(ID,Now,1))break;
   if(GetWorld()->SweepTestByObjectType(Anchor,Anchor,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88))){SightExperiment.Cover(TEXT("known_move_reject_anchor_capsule"));continue;}
   if(!KnownMoveSpend(ID,Now,2))break;
   UNavigationPath* Route=Nav->FindPathToLocationSynchronously(GetWorld(),Start-FVector(0,0,90),Projected.Location);
   if(!Route || !Route->IsValid() || Route->IsPartial() || Route->PathPoints.Num()<2){SightExperiment.Cover(TEXT("known_move_reject_path_invalid"));continue;}
   double Length=0.;bool PathAllowed=true;
   for(int32 Index=1;Index<Route->PathPoints.Num();++Index)
   {Length+=FVector::Dist(Route->PathPoints[Index-1],Route->PathPoints[Index]);PathAllowed &= KnownMovePlaceAllowed(ID,Start,Route->PathPoints[Index]+FVector(0,0,90));}
   if(!PathAllowed){SightExperiment.Cover(TEXT("known_move_reject_path_mission"));continue;}
   if(Length>1.5*FVector::Dist2D(Start,Anchor)){SightExperiment.Cover(TEXT("known_move_reject_path_length"));continue;}
   // The mesh can lag dynamic geometry. Validate every segment, with the same bounded sweep budget.
   for(int32 SegmentIndex=1;SegmentIndex<Route->PathPoints.Num();++SegmentIndex)
   {
    if(!KnownMoveSpend(ID,Now,1)){PathAllowed=false;break;}
    if(GetWorld()->SweepTestByObjectType(Route->PathPoints[SegmentIndex-1]+FVector(0,0,90),Route->PathPoints[SegmentIndex]+FVector(0,0,90),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88))){PathAllowed=false;break;}
   }
   if(!PathAllowed){SightExperiment.Cover(bKnownMoveDeferred?TEXT("known_move_reject_path_deferred"):TEXT("known_move_reject_path_capsule"));continue;}
   // Reserve before publishing the approach; CancelCoverAction releases the old reservation first.
   const int32 OldSlot=Unit->CoverSlot;
   CancelCoverAction(ID);
   if(!Free && !Cover->Reserve(Place.Slot,ID,Now)){SightExperiment.Cover(TEXT("known_move_reject_reservation"));if(OldSlot>=0)Cover->Reserve(OldSlot,ID,Now);continue;}
   FKnownMove Episode;Episode.Start=Start;Episode.Goal=Anchor;Episode.Last=Start;Episode.Peek=Peek;Episode.Observation=Target;Episode.Mission=Squad->MissionRevision;
   Episode.bMarch=March;Episode.Revision=Unit->OrderRevision;Episode.Started=Now;Episode.Slot=Place.Slot;Episode.bStandUp=StandUp;KnownMoves.Add(ID,Episode);
   FCoverApproach Approach;Approach.Goal=Anchor;Approach.Target=TargetID;Approach.Slot=Place.Slot;Approach.Revision=Unit->OrderRevision;
   Approach.Until=Now+FMath::Max(8.,Length/100.+3.);Approach.bFree=Free;CoverApproaches.Add(ID,Approach);
   auto& Path=SoldierPaths.FindOrAdd(ID);Path=FSoldierPath();Path.Points=Route->PathPoints;
   FirePositions.Remove(ID);KnownFireSearches.Remove(ID);
   Unit->DecisionReason=TEXT("KNOWN TARGET / MOVING TO FIRE");SightExperiment.Cover(TEXT("known_move_started"));
   if(Target.Tracked(Now))SightExperiment.Cover(TEXT("known_move_tracked_plans"));
   FinishBudget();return true;
  }
 }
 FinishBudget();return false;
}

void UBattleLeaderRuntime::EndKnownMove(int32 ID,double Now,const TCHAR* Reason)
{
 const auto* Episode=KnownMoves.Find(ID);if(!Episode)return;
 SightExperiment.Cover(Reason);
 if(SightExperiment.Enabled && Episode->Arrived<0){SightExperiment.KnownMoveDistance+=Episode->Distance;SightExperiment.KnownMoveSeconds+=Now-Episode->Started;}
 if(SightExperiment.Enabled)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_END unit=%d target=%d reason=%s distance_cm=%.3f time=%.3f"),ID,Episode->Observation.TargetID,Reason,Episode->Distance,Now-Episode->Started);
 const auto* EndingUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* EndingApproach=CoverApproaches.Find(ID);
 const bool OwnApproach=EndingApproach && EndingApproach->Target==Episode->Observation.TargetID &&
  EndingApproach->Revision==Episode->Revision && EndingApproach->Slot==Episode->Slot &&
  EndingApproach->Goal==Episode->Goal && !EndingApproach->bCharge && !EndingApproach->bAssault;
 if(Episode->Arrived<0 && (OwnApproach || (!EndingApproach && (!EndingUnit || !EndingUnit->CoverPhase))))GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);
 else if(Episode->Arrived>=0 && (Episode->bMarch || FCString::Strncmp(Reason,TEXT("known_move_cancel_"),18)==0))
 {
  const auto* EndedUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
  if(EndedUnit && EndedUnit->CoverOrderRevision==Episode->Revision && ((Episode->bMarch && EndedUnit->CoverAnchor==Episode->Goal) || (EndedUnit->CoverTargetID==Episode->Observation.TargetID && EndedUnit->CoverPeek==Episode->Peek)))CancelCoverAction(ID);
  if(const auto* EndedPosition=FirePositions.Find(ID);EndedPosition && *EndedPosition==Episode->Goal)FirePositions.Remove(ID);
 }
 if(OwnApproach){CoverApproaches.Remove(ID);SoldierPaths.Remove(ID);}
 if(Episode->bMarch && (!EndingApproach || OwnApproach))SoldierPaths.Remove(ID); // discard the detour; normal movement rebuilds the mission route
 if(!EndingApproach || OwnApproach)
  if(auto* EndBody=Bodies.Find(ID);EndBody && EndBody->Actor.IsValid())EndBody->Actor->GetCharacterMovement()->StopMovementImmediately();
 KnownMoves.Remove(ID);KnownMoveAfter.Add(ID,Now+5.);
}

bool UBattleLeaderRuntime::StepKnownMove(int32 ID,float Dt,double Now)
{
 auto* Episode=KnownMoves.Find(ID);if(!Episode)return false;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Unit=Store->Units.Find(ID);
 const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;
 if(!Unit || !Unit->IsAlive()){EndKnownMove(ID,Now,TEXT("known_move_cancel_died"));return false;}
 if(!KnownMoveAllowed(ID) || Unit->OrderRevision!=Episode->Revision || !Squad || Squad->MissionRevision!=Episode->Mission)
 {EndKnownMove(ID,Now,TEXT("known_move_cancel_order"));return false;}
 auto* Approach=CoverApproaches.Find(ID);
 const auto& Team=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];const auto* Current=Team.Targets.Find(Episode->Observation.TargetID);
 const bool Fresh=Current && Current->Fresh(Now) && WeaponOrderAllows(ID,Current->TargetID,Current->At(Now));
 if(Episode->bMarch && (!Fresh || Episode->bFired))
 {EndKnownMove(ID,Now,!Fresh?TEXT("known_move_cancel_stale"):TEXT("known_move_finished"));return false;}
 if(Episode->bMarch && (!KnownMovePlaceAllowed(ID,Episode->Start,Position(ID)) || !KnownMovePlaceAllowed(ID,Episode->Start,Episode->Peek)))
 {EndKnownMove(ID,Now,TEXT("known_move_cancel_mission"));return false;}
 if(Episode->Arrived>=0)
 {
  if(Episode->bMarch)
  {
   if(Now-Episode->Arrived>5.){EndKnownMove(ID,Now,TEXT("known_move_finished"));return false;}
   // Stay at the certified point until the first shot. Do not execute the squad route in this frame.
   if(!StepCoverAction(ID,Dt,Now))
   {
    const FVector MarchMuzzle=Position(ID)+FVector(0,0,BattleStance::EyeOffset(StanceOf(ID)));
    const auto MarchFire=SelectKnownFire(ID,MarchMuzzle,Now);
    if(MarchFire.TargetID!=INDEX_NONE)
    {
     const FVector MarchAim=(MarchFire.AimPosition-MarchMuzzle).GetSafeNormal();
     Store->UpdateAim(ID,Now,0.f,MarchAim,true);
     if(auto* MarchBody=Bodies.Find(ID);MarchBody && MarchBody->Actor.IsValid())MarchBody->Actor->NoteDisplayAim(MarchAim,Now);
    }
    if(MarchFire.Ready())FireKnownTarget(ID,MarchMuzzle,MarchFire,false);
    Store->SetAction(ID,Unit->ReloadEndTime>0?EBattleAction::Reloading:MarchFire.TargetID!=INDEX_NONE?EBattleAction::Engaging:EBattleAction::Idle);
   }
   return true;
  }
  if(!Fresh || Now-Episode->Arrived>5.)EndKnownMove(ID,Now,!Fresh?TEXT("known_move_cancel_stale"):TEXT("known_move_finished"));
  return false;
 }
 if(!Approach || Approach->Target!=Episode->Observation.TargetID || Approach->Revision!=Episode->Revision ||
  Approach->Slot!=Episode->Slot || Approach->Goal!=Episode->Goal || Approach->bCharge || Approach->bAssault || !SoldierPaths.Contains(ID)){EndKnownMove(ID,Now,TEXT("known_move_cancel_interrupted"));return false;}
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();
 if(Episode->Slot!=INDEX_NONE && !Cover->Reserve(Episode->Slot,ID,Now)){EndKnownMove(ID,Now,TEXT("known_move_cancel_reservation"));return false;}
 if(!KnownMovePlaceAllowed(ID,Episode->Start,Approach->Goal) || !KnownMovePlaceAllowed(ID,Episode->Start,Position(ID))){EndKnownMove(ID,Now,TEXT("known_move_cancel_mission"));return false;}
 if(!Fresh && !Episode->bReturning)
 {
  Episode->bReturning=true;SightExperiment.Cover(TEXT("known_move_cancel_stale"));
  // A chosen cover remains the nearest certified shelter on this route. Free positions return along their cached route.
  if(Episode->Slot==INDEX_NONE)
  {
   auto& ReturnPath=SoldierPaths.FindChecked(ID);TArray<FVector> Reverse;Reverse.Add(Position(ID)-FVector(0,0,90));
   for(int32 Index=FMath::Min(ReturnPath.Next-1,ReturnPath.Points.Num()-1);Index>=0;--Index)Reverse.Add(ReturnPath.Points[Index]);
   ReturnPath.Points=MoveTemp(Reverse);ReturnPath.Next=1;Approach->Goal=Episode->Start;Episode->Goal=Episode->Start;
  }
 }
 const FVector Pos=Position(ID);Episode->Distance+=FVector::Dist2D(Pos,Episode->Last);
 Episode->Stuck=FVector::DistSquared2D(Pos,Episode->Last)<1.?Episode->Stuck+Dt:0.f;Episode->Last=Pos;
 if(Now>Approach->Until || Episode->Stuck>2.){EndKnownMove(ID,Now,Episode->Stuck>2.?TEXT("known_move_cancel_stuck"):TEXT("known_move_cancel_timeout"));return false;}
 if(FVector::Dist2D(Pos,Approach->Goal)<40.)
 {
  const FCoverApproach Done=*Approach;CoverApproaches.Remove(ID);SoldierPaths.Remove(ID);Episode->Arrived=Now;KnownMoveAfter.Add(ID,Now+5.);
  KnownFireSearches.Remove(ID);SightExperiment.Cover(TEXT("known_move_arrived"));
  if(SightExperiment.Enabled){SightExperiment.KnownMoveDistance+=Episode->Distance;SightExperiment.KnownMoveSeconds+=Now-Episode->Started;}
  if(SightExperiment.Enabled)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_ARRIVED unit=%d target=%d known_move_distance_cm=%.3f known_move_time=%.3f"),ID,Done.Target,Episode->Distance,Now-Episode->Started);
  if(auto* Body=Bodies.Find(ID);Body && Body->Actor.IsValid())Body->Actor->GetCharacterMovement()->StopMovementImmediately();
  if(Episode->bReturning){if(Episode->Slot!=INDEX_NONE)SetStance(ID,BattleStance::Crouch,Now,true);Cover->Release(ID);EndKnownMove(ID,Now,TEXT("known_move_returned"));return true;}
  const FVector Watch=(Episode->Observation.At(Now)-Pos).GetSafeNormal2D();
  auto& ArrivedBody=Bodies.FindChecked(ID);ArrivedBody.Velocity=FVector::ZeroVector;
  if(ArrivedBody.Actor.IsValid())ArrivedBody.Actor->SetActorRotation(Watch.Rotation());
  else Manager->GetFragmentDataChecked<FBattleFollowerFragment>(ArrivedBody.Entity).Forward=Watch;
  if(!Done.bFree || Episode->bStandUp)
  {
   Unit->CoverAnchor=Done.Goal;Unit->CoverPeek=Episode->Peek;Unit->CoverSlot=Done.bFree?FreeCoverSlot:Done.Slot;Unit->CoverTargetID=Done.Target;
   Unit->CoverOrderRevision=Unit->OrderRevision;Unit->CoverPhase=2;Unit->CoverSeconds=0;
  }
  else FirePositions.Add(ID,Done.Goal);
  // The next Follow/StepCoverAction runs the stage 1 personal sensor from the arrived position, never this planning ray.
  return true;
 }
 auto& Path=SoldierPaths.FindChecked(ID);
 while(Path.Next<Path.Points.Num()-1 && FVector::DistSquared2D(Pos,Path.Points[Path.Next])<FMath::Square(40.))++Path.Next;
 if(!Path.Points.IsValidIndex(Path.Next)){EndKnownMove(ID,Now,TEXT("known_move_cancel_path"));return false;}
 const FVector Way=Path.Points[Path.Next],Direction=(Way-Pos).GetSafeNormal2D();auto& Body=Bodies.FindChecked(ID);
 const float Speed=GetDefault<UBattleTuning>()->MoveSpeed*BattleSquadType::SpeedAt(Squad->SquadType,GetWorld(),Pos);
 if(Body.Actor.IsValid()){Body.Actor->GetCharacterMovement()->MaxWalkSpeed=Speed;Body.Actor->AddMovementInput(Direction);Body.Velocity=Body.Actor->GetVelocity();}
 else
 {
  FVector Delta=Direction*FMath::Min(Speed*Dt,float(FVector::Dist2D(Pos,Way)));if(!TerrainStep(Pos,Delta))Delta=FVector::ZeroVector;
  FHitResult Hit;Body.Capsule->MoveComponent(Delta,Direction.Rotation().Quaternion(),true,&Hit);
  auto& Fragment=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Body.Entity);Fragment.Position=Body.Capsule->GetComponentLocation();Fragment.Forward=Direction;
  Body.Velocity=(Fragment.Position-Pos)/FMath::Max(Dt,.001f);
 }
 return true;
}

void UBattleLeaderRuntime::KnownMoveShot(int32 ID,int32 Target,double Now)
{
 auto* Episode=KnownMoves.Find(ID);if(!Episode || Episode->Arrived<0 || Episode->bFired)return;
 if(Target!=Episode->Observation.TargetID){SightExperiment.Cover(TEXT("known_move_arrived_other_shot"));if(Episode->bMarch)Episode->bFired=true;return;}
 Episode->bFired=true;SightExperiment.Cover(TEXT("known_move_arrived_fired"));
 if(SightExperiment.Enabled)SightExperiment.KnownMoveFirstShotSeconds+=Now-Episode->Arrived;
 if(SightExperiment.Enabled)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_SHOT unit=%d target=%d known_move_first_shot_time=%.6f"),ID,Target,Now-Episode->Arrived);
}
