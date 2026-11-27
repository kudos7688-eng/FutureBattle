// Level 2 only: observation-ranked short paths. No enemy bodies are read by planning.
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleCover.h"
#include "BattleRanges.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
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

// Default 2 (user decision 2026-11-27): known-target fire plus moves to a firing position; =1 fires from place only, =0 restores the old play.
bool BattleKnownFire::MoveEnabled(){static const bool On=[]{int32 V=2;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownTargetFire="),V);return V==2;}();return On;}
double BattleKnownFire::MoveCommit(){static const double Seconds=[]{double V=12.;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveCommit="),V);return FMath::IsFinite(V)?FMath::Clamp(V,0.,60.):12.;}();return Seconds;}
double BattleKnownFire::MoveWait(){static const double Seconds=[]{double V=1.5;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveWait="),V);return FMath::IsFinite(V)?FMath::Clamp(V,.1,5.):1.5;}();return Seconds;}
bool BattleKnownFire::MoveMarch(){static const bool On=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveMarch="),V);return V!=0;}();return On;}
bool BattleKnownFire::MoveDefendMarch(){static const bool On=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveDefendMarch="),V);return V!=0;}();return On;}
double BattleKnownFire::MoveDefendReach(){static const double Radius=[]{double V=1500.;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveDefendReach="),V);return FMath::IsFinite(V)?FMath::Clamp(V,0.,50000.):1500.;}();return Radius;}

double UBattleLeaderRuntime::KnownMoveDefendReach(int32 ID) const
{
 if(!BattleKnownFire::MoveEnabled())return 0.;
 const auto* ReachUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* ReachSquad=ReachUnit?GetWorld()->GetSubsystem<UBattleState>()->FindSquad(ReachUnit->SquadID):nullptr;
 return ReachSquad && ReachSquad->Order==EBattleOrder::Defend?
  (KnownMoveTrialDefendReach>=0.?KnownMoveTrialDefendReach:BattleKnownFire::MoveDefendReach()):0.;
}

FVector UBattleLeaderRuntime::KnownMoveReachCenter(int32 ID) const
{
 const auto* CenterStore=GetWorld()->GetSubsystem<UBattleState>();const auto* CenterUnit=CenterStore->Find(ID);
 const auto* CenterSquad=CenterUnit?CenterStore->FindSquad(CenterUnit->SquadID):nullptr;
 if(CenterSquad && CenterSquad->DefenseRadius>0.)return CenterSquad->Destination;
 const auto* CenterPlan=CenterUnit?Plans.Find(CenterUnit->SquadID):nullptr;
 const auto* CenterAssigned=CenterPlan?CenterPlan->ArrivalGoals.Find(ID):nullptr;
 return CenterAssigned?*CenterAssigned:CenterUnit?CenterUnit->Destination:FVector::ZeroVector;
}

// Called only for movement candidates or the current episode's observed target.
bool UBattleLeaderRuntime::KnownMoveTargetAllows(int32 ID,int32 TargetID,const FVector& Observed) const
{
 if(WeaponOrderAllows(ID,TargetID,Observed))return true;
 const double ExtraReach=KnownMoveDefendReach(ID);if(ExtraReach<=0.)return false;
 const auto* ReachStore=GetWorld()->GetSubsystem<UBattleState>();const auto* ReachUnit=ReachStore->Find(ID);
 const auto* ReachSquad=ReachUnit?ReachStore->FindSquad(ReachUnit->SquadID):nullptr;
 if(!ReachUnit || !ReachUnit->IsAlive() || !ReachSquad || ReachSquad->CommandStatus!=EBattleCommandStatus::Active)return false;
 // Never bypass the sector or support-target gates. Only enlarge the defense disk.
 if(ReachSquad->bSectorMission && ReachSquad->TacticalPlan.Action==EBattleSquadTactic::Mission)return false;
 const FVector ReachPost=KnownMoveReachCenter(ID);
 const double TargetRadius=FMath::Max(0.,double(ReachSquad->DefenseRadius))+ExtraReach+BattleRanges::Engage(ReachUnit->Weapon);
 if(FVector::DistSquared2D(Observed,ReachPost)>FMath::Square(TargetRadius))return false;
 // The observed distance was checked above. Evaluate the remaining order gates
 // at the defense center without copying the squad and its member/route arrays.
 return BattleTactics::OrderAllowsTarget(*ReachSquad,TargetID,ReachSquad->Destination);
}

double BattleKnownFire::MoveRange(){static const double Range=[]{double V=2500.;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveRange="),V);return FMath::Clamp(V,0.,25000.);}();return Range;}
double BattleKnownFire::MoveSquadRadius(){static const double Radius=[]{double V=5000.;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveSquadRadius="),V);return FMath::Clamp(V,0.,50000.);}();return Radius;}
int32 BattleKnownFire::MoveMax(){static const int32 Maximum=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveMax="),V);return FMath::Max(0,V);}();return Maximum;}


int32 BattleKnownFire::MoveRayBudget(){static const int32 Cap=[]{int32 V=32;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveRayBudget="),V);return FMath::Clamp(V,0,4096);}();return Cap;}
int32 BattleKnownFire::MoveSweepBudget(){static const int32 Cap=[]{int32 V=24;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveSweepBudget="),V);return FMath::Clamp(V,0,4096);}();return Cap;}
int32 BattleKnownFire::MoveNavBudget(){static const int32 Cap=[]{int32 V=32;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownMoveNavBudget="),V);return FMath::Clamp(V,0,4096);}();return Cap;}

bool UBattleLeaderRuntime::KnownMoveMarching(int32 ID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;const auto* Plan=Unit?Plans.Find(Unit->SquadID):nullptr;
 return Squad && Plan && Squad->Order==EBattleOrder::AttackMove && !Plan->bCoverStop &&
  Squad->TacticalPlan.Action!=EBattleSquadTactic::Engage && FVector::DistSquared2D(Position(ID),Unit->Destination)>FMath::Square(100.);
}

// The same personal-post threshold as the marching gate, without changing route state.
bool UBattleLeaderRuntime::KnownMoveDefendAtPost(int32 ID) const
{
 if(!BattleKnownFire::MoveEnabled() || !BattleKnownFire::MoveDefendMarch())return false;
 const auto* PostStore=GetWorld()->GetSubsystem<UBattleState>();const auto* PostUnit=PostStore->Find(ID);
 const auto* PostSquad=PostUnit?PostStore->FindSquad(PostUnit->SquadID):nullptr;
 const auto* PostPlan=PostUnit?Plans.Find(PostUnit->SquadID):nullptr;const auto* PostBody=Bodies.Find(ID);
 if(!PostSquad || !PostPlan || !PostBody || PostSquad->Order!=EBattleOrder::Defend || PostPlan->bCoverStop ||
  PostSquad->TacticalPlan.Action==EBattleSquadTactic::Engage)return false;
 const auto* PostRoute=PostPlan->Corridors.Find(ID);const auto* PostGoal=PostPlan->ArrivalGoals.Find(ID);
 return (PostPlan->Path.IsValidIndex(PostBody->Waypoint) || (PostRoute && PostRoute->IsValidIndex(PostBody->Waypoint))) &&
  FVector::DistSquared2D(Position(ID),PostGoal?*PostGoal:PostUnit->Destination)<=FMath::Square(100.);
}

bool UBattleLeaderRuntime::KnownMoveDefendMarching(int32 ID,bool FollowingMarch,bool CountGate)
{
 auto DefendGate=[&](const TCHAR* DefendReason)
 {if(CountGate)SightExperiment.Cover(DefendReason);return false;};
 if(!FollowingMarch)return DefendGate(TEXT("known_move_defend_gate_not_following"));
 if(!BattleKnownFire::MoveEnabled() || !BattleKnownFire::MoveDefendMarch())return DefendGate(TEXT("known_move_defend_gate_disabled"));
 const auto* DefendStore=GetWorld()->GetSubsystem<UBattleState>();const auto* DefendUnit=DefendStore->Find(ID);
 const auto* DefendSquad=DefendUnit?DefendStore->FindSquad(DefendUnit->SquadID):nullptr;
 const auto* DefendPlan=DefendUnit?Plans.Find(DefendUnit->SquadID):nullptr;
 const auto* DefendBody=Bodies.Find(ID);
 if(!DefendSquad || !DefendPlan || !DefendBody)return DefendGate(TEXT("known_move_defend_gate_invalid"));
 if(DefendSquad->Order!=EBattleOrder::Defend)return DefendGate(TEXT("known_move_defend_gate_order"));
 if(DefendPlan->bCoverStop)return DefendGate(TEXT("known_move_defend_gate_cover_stop"));
 if(DefendSquad->TacticalPlan.Action==EBattleSquadTactic::Engage)return DefendGate(TEXT("known_move_defend_gate_engage"));
 const auto* DefendRoute=DefendPlan->Corridors.Find(ID);const auto* DefendArrival=DefendPlan->ArrivalGoals.Find(ID);
 if(!DefendPlan->Path.IsValidIndex(DefendBody->Waypoint) && !(DefendRoute && DefendRoute->IsValidIndex(DefendBody->Waypoint)))
  return DefendGate(TEXT("known_move_defend_gate_no_route"));
 // StableMove keeps its final waypoint when arrived: bOnRoute alone is not marching.
 if(!(FVector::DistSquared2D(Position(ID),DefendArrival?*DefendArrival:DefendUnit->Destination)>FMath::Square(100.)))
  return DefendGate(TEXT("known_move_defend_gate_at_post"));
 if(CountGate)SightExperiment.Cover(TEXT("known_move_defend_gate_pass"));
 return true;
}

// Diagnostics only: follow the unchanged mission until arrival, interruption or lack of progress.
void UBattleLeaderRuntime::TrackKnownDefendResume(int32 ID,double Now)
{
 auto* Resume=KnownDefendResumes.Find(ID);if(!Resume)return;
 const auto* ResumeStore=GetWorld()->GetSubsystem<UBattleState>();const auto* ResumeUnit=ResumeStore->Find(ID);
 const auto* ResumeSquad=ResumeUnit?ResumeStore->FindSquad(ResumeUnit->SquadID):nullptr;
 if(!ResumeUnit || !ResumeUnit->IsAlive() || !ResumeSquad || ResumeUnit->OrderRevision!=Resume->Revision ||
  ResumeSquad->MissionRevision!=Resume->Mission || ResumeSquad->Order!=EBattleOrder::Defend || ResumeUnit->Destination!=Resume->Goal)
 {SightExperiment.Cover(Resume->bDefendPost?TEXT("known_move_defend_post_resume_interrupted"):TEXT("known_move_defend_march_resume_interrupted"));KnownDefendResumes.Remove(ID);return;}
 if(KnownMoves.Contains(ID))return;
 const auto* ResumePlan=Plans.Find(ResumeUnit->SquadID);
 const FVector* AssignedPost=ResumePlan?ResumePlan->ArrivalGoals.Find(ID):nullptr;
 const FVector CurrentPost=AssignedPost?*AssignedPost:Resume->Goal;
 if(CurrentPost!=Resume->ArrivalGoal){Resume->ArrivalGoal=CurrentPost;Resume->Best=FVector::Dist2D(Position(ID),CurrentPost);Resume->ProgressAt=Now;}
 const double Remaining=FVector::Dist2D(Position(ID),CurrentPost);
 if(Remaining<=100.){SightExperiment.Cover(Resume->bDefendPost?TEXT("known_move_defend_post_resume_arrived"):TEXT("known_move_defend_march_resume_arrived"));KnownDefendResumes.Remove(ID);return;}
 if(Remaining<Resume->Best-40.){Resume->Best=Remaining;Resume->ProgressAt=Now;}
 if(!Resume->bReportedStuck && Now-Resume->ProgressAt>10.)
 {Resume->bReportedStuck=true;SightExperiment.Cover(Resume->bDefendPost?TEXT("known_move_defend_post_resume_stuck"):TEXT("known_move_defend_march_resume_stuck"));}
}

bool UBattleLeaderRuntime::KnownMoveAllowed(int32 ID,bool CountGate)
{
 if(BattleTactics::SectorDefendMarch()>0 && SectorMarchUnit(ID))return false;
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

const TCHAR* UBattleLeaderRuntime::KnownMovePlaceRejection(int32 ID,FVector Start,FVector Place,bool DefendMarchAtStart) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;const auto* Plan=Unit?Plans.Find(Unit->SquadID):nullptr;
 if(!Squad || !Plan || Place.ContainsNaN())return TEXT("known_move_reject_mission_invalid");
 if(BattleTactics::SectorDefend()>0 && !SectorDefendUnitPlace(ID,Place))return TEXT("known_move_reject_mission_sector");
 if(FVector::DistSquared2D(Start,Place)>FMath::Square(BattleKnownFire::MoveRange()))return TEXT("known_move_reject_mission_range");
 if(FVector::DistSquared2D(Plan->Center,Place)>FMath::Square(BattleKnownFire::MoveSquadRadius()))return TEXT("known_move_reject_mission_squad_radius");
 const double DefendReach=KnownMoveDefendReach(ID);
 const FVector ReachPost=KnownMoveReachCenter(ID);
 const bool InReach=DefendReach>0. && FVector::DistSquared2D(Place,ReachPost)<=FMath::Square(DefendReach);
 const auto* Episode=KnownMoves.Find(ID);
 const bool DefendMarch=Episode?Episode->bDefendMarch:DefendMarchAtStart;
 const bool March=(BattleKnownFire::MoveMarch() && (Episode?Episode->bMarch:KnownMoveMarching(ID))) || DefendMarch;
 // Reach=0 preserves the old strip. Reach>0 uses the defense/reach union, with no backward extension.
 // Keep the departure fixed throughout the detour; otherwise repeated checks permit drift.
 if(DefendMarch && (Squad->DefenseRadius>0. || DefendReach>0.))
 {
  const FVector Departure=Episode?Episode->Start:Start;
  const FVector* AssignedPost=Plan->ArrivalGoals.Find(ID);
  const FVector Post=Episode?Episode->DefendPost:AssignedPost?*AssignedPost:Unit->Destination;
  const FVector Axis=(Post-Departure)*FVector(1,1,0),Offset=(Place-Departure)*FVector(1,1,0);
  const double AxisSquared=Axis.SizeSquared(),Along=AxisSquared>1.?FVector::DotProduct(Offset,Axis)/AxisSquared:-1.;
  const bool InStrip=Along>=0. && Along<=1. && (Offset-Axis*Along).SizeSquared()<=FMath::Square(600.);
  const bool InDefense=FVector::DistSquared2D(Place,Squad->Destination)<=FMath::Square(double(Squad->DefenseRadius));
  if(!InDefense && !(DefendReach>0.?(InReach && Along>=0.):InStrip))return TEXT("known_move_reject_mission_defend_march");
 }
 if(!March && Squad->bSectorMission && BattleSectors::Cell(Place)!=BattleSectors::Cell(Squad->Destination))return TEXT("known_move_reject_mission_sector");
 if(!DefendMarch && !InReach && Squad->Order==EBattleOrder::Defend && FVector::DistSquared2D(Place,Squad->Destination)>FMath::Square(double(Squad->DefenseRadius)))return TEXT("known_move_reject_mission_defend");
 return nullptr;
}

bool UBattleLeaderRuntime::KnownMovePlaceAllowed(int32 ID,FVector Start,FVector Place,bool DefendMarchAtStart) const
{
 return KnownMovePlaceRejection(ID,Start,Place,DefendMarchAtStart)==nullptr;
}

// Split at circle/forward-plane boundaries: inside each interval all range
// predicates are constant. No traces, navigation, or sampling-distance loophole.
bool UBattleLeaderRuntime::KnownMoveSegmentAllowed(int32 ID,FVector Start,FVector From,FVector To,bool DefendMarchAtStart) const
{
 const double SegmentReach=KnownMoveDefendReach(ID);if(SegmentReach<=0.)return true;
 const auto* SegmentUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* SegmentSquad=SegmentUnit?GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SegmentUnit->SquadID):nullptr;
 if(!SegmentSquad || !KnownMovePlaceAllowed(ID,Start,From,DefendMarchAtStart) || !KnownMovePlaceAllowed(ID,Start,To,DefendMarchAtStart))return false;
 TArray<double,TInlineAllocator<8>> Cuts;Cuts.Add(0.);Cuts.Add(1.);
 const FVector SegmentDelta=(To-From)*FVector(1,1,0);const double SegmentLength2=SegmentDelta.SizeSquared();
 auto CutCircle=[&](FVector CircleCenter,double CircleRadius)
 {
  if(SegmentLength2<=1.e-12)return;
  const FVector CircleOffset=(From-CircleCenter)*FVector(1,1,0);
  const double CircleB=FVector::DotProduct(CircleOffset,SegmentDelta);
  const double CircleD=CircleB*CircleB-SegmentLength2*(CircleOffset.SizeSquared()-CircleRadius*CircleRadius);
  if(CircleD<0.)return;
  for(double CircleSign:{-1.,1.}){const double CircleT=(-CircleB+CircleSign*FMath::Sqrt(CircleD))/SegmentLength2;if(CircleT>0. && CircleT<1.)Cuts.Add(CircleT);}
 };
 CutCircle(SegmentSquad->Destination,FMath::Max(0.,double(SegmentSquad->DefenseRadius)));
 CutCircle(KnownMoveReachCenter(ID),SegmentReach);
 const auto* SegmentEpisode=KnownMoves.Find(ID);const auto* SegmentPlan=Plans.Find(SegmentUnit->SquadID);
 if(SegmentPlan && (SegmentEpisode?SegmentEpisode->bDefendMarch:DefendMarchAtStart))
 {
  const auto* SegmentAssigned=SegmentPlan->ArrivalGoals.Find(ID);
  const FVector SegmentPost=SegmentEpisode?SegmentEpisode->DefendPost:SegmentAssigned?*SegmentAssigned:SegmentUnit->Destination;
  const FVector SegmentDeparture=SegmentEpisode?SegmentEpisode->Start:Start;
  const FVector SegmentAxis=(SegmentPost-SegmentDeparture)*FVector(1,1,0);
  const double PlaneRate=FVector::DotProduct(SegmentDelta,SegmentAxis);
  if(FMath::Abs(PlaneRate)>1.e-12){const double PlaneT=-FVector::DotProduct(From-SegmentDeparture,SegmentAxis)/PlaneRate;if(PlaneT>0. && PlaneT<1.)Cuts.Add(PlaneT);}
 }
 Cuts.Sort();
 for(int32 CutIndex=1;CutIndex<Cuts.Num();++CutIndex)
  if(!KnownMovePlaceAllowed(ID,Start,FMath::Lerp(From,To,(Cuts[CutIndex-1]+Cuts[CutIndex])*.5),DefendMarchAtStart))return false;
 return true;
}

// One complete position/target transaction: exposure 4 + over 2 + lean 6 rays,
// anchor projection 1 + lean 6 + path 1 nav, lean 6 + anchor 1 + path <=17 sweeps.
// Rays also reserve the shared firing allowance. Modes 0/1 retain their original limits.
bool UBattleLeaderRuntime::KnownMoveReserve(int32 ID,double Now)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 if(!Unit || Unit->Team<0 || Unit->Team>1)return false;
 auto& Budget=KnownMoveBudgets[Unit->Team];const int64 Window=FMath::FloorToInt64(Now/.1);
 if(Budget.Window!=Window){Budget.Window=Window;Budget.Rays=0;Budget.Sweeps=0;Budget.Nav=0;}
 for(int32 Index=Budget.Waiting.Num()-1;Index>=0;--Index)
 {
  const int32 WaitID=Budget.Waiting[Index];const auto* WaitUnit=Store->Find(WaitID);
  if(!WaitUnit || !WaitUnit->IsAlive() || Now-Budget.Asked.FindRef(WaitID)>1.5)
  {Budget.Waiting.RemoveAt(Index);Budget.Asked.Remove(WaitID);}
 }
 auto& FireBudget=KnownFireBudgets[Unit->Team];
 if(FireBudget.Window!=Window){FireBudget.Window=Window;FireBudget.Used=0;}
 for(int32 FireIndex=FireBudget.Waiting.Num()-1;FireIndex>=0;--FireIndex)
 {
  const int32 FireWaitID=FireBudget.Waiting[FireIndex];const auto* FireWaitUnit=Store->Find(FireWaitID);
  if(!FireWaitUnit || !FireWaitUnit->IsAlive() || Now-FireBudget.RequestedAt.FindRef(FireWaitID)>.5)
  {FireBudget.Waiting.RemoveAt(FireIndex);FireBudget.RequestedAt.Remove(FireWaitID);}
 }
 const bool RayRoom=FireBudget.Used+12<=BattleKnownFire::MoveRayBudget();
 const bool SweepRoom=Budget.Sweeps+24<=BattleKnownFire::MoveSweepBudget();
 const bool NavRoom=Budget.Nav+8<=BattleKnownFire::MoveNavBudget();
 if(!RayRoom || !SweepRoom || !NavRoom || (Budget.Waiting.Num() && Budget.Waiting[0]!=ID) || (FireBudget.Waiting.Num() && FireBudget.Waiting[0]!=ID))
 {
  Budget.Waiting.AddUnique(ID);Budget.Asked.Add(ID,Now);bKnownMoveDeferred=true;
  SightExperiment.Cover(TEXT("known_move_budget_deferred"));
  SightExperiment.Cover(!RayRoom?TEXT("known_move_reserve_ray"):!SweepRoom?TEXT("known_move_reserve_sweep"):!NavRoom?TEXT("known_move_reserve_nav"):TEXT("known_move_reserve_fifo"));
  return false;
 }
 Budget.Rays+=12;Budget.Sweeps+=24;Budget.Nav+=8;FireBudget.Used+=12;
 KnownMoveReservedID=ID;KnownMoveReservedWindow=Window;KnownMoveRemainingRays=12;KnownMoveRemainingSweeps=24;KnownMoveRemainingNav=8;
 Budget.Waiting.Remove(ID);Budget.Asked.Remove(ID);bKnownMoveDeferred=false;
 return true;
}

void UBattleLeaderRuntime::KnownMoveRelease(int32 ID)
{
 if(KnownMoveReservedID!=ID)return;
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(Unit && Unit->Team>=0 && Unit->Team<2)
 {
  auto& Budget=KnownMoveBudgets[Unit->Team];
  if(Budget.Window==KnownMoveReservedWindow){Budget.Rays-=KnownMoveRemainingRays;Budget.Sweeps-=KnownMoveRemainingSweeps;Budget.Nav-=KnownMoveRemainingNav;}
  auto& FireBudget=KnownFireBudgets[Unit->Team];
  if(FireBudget.Window==KnownMoveReservedWindow)FireBudget.Used-=KnownMoveRemainingRays;
 }
 KnownMoveReservedID=INDEX_NONE;
}

bool UBattleLeaderRuntime::KnownMoveSpend(int32 ID,double Now,uint8 Kind)
{
 // Geometry is only entered after a complete reservation. Exhaustion is a bug, not a retry policy.
 int32& Remaining=Kind==0?KnownMoveRemainingRays:Kind==1?KnownMoveRemainingSweeps:KnownMoveRemainingNav;
 if(KnownMoveReservedID!=ID || KnownMoveReservedWindow!=FMath::FloorToInt64(Now/.1) || Remaining<=0)
 {bKnownMoveDeferred=true;SightExperiment.Cover(TEXT("known_move_reservation_violation"));return false;}
 --Remaining;if(Kind==0)SightExperiment.Cover(TEXT("known_fire_rays"));SightExperiment.Cover(Kind==0?TEXT("known_move_rays"):Kind==1?TEXT("known_move_sweeps"):TEXT("known_move_nav"));return true;
}

bool UBattleLeaderRuntime::KnownMoveLine(int32 ID,FVector From,FVector To,double Now)
{
 if(!KnownMoveSpend(ID,Now,0))return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(KnownMoveLine),false);FHitResult Hit;
 // Ignore Pawn object responses in the query, instead of spending up to nine rays skipping bodies.
 FCollisionResponseParams Responses;Responses.CollisionResponse.SetResponse(ECC_Pawn,ECR_Ignore);
 GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Query,Responses);
 if(Hit.bBlockingHit)SightExperiment.Cover(TEXT("known_move_reject_line_terrain"));
 return !Hit.bBlockingHit;
}

bool UBattleLeaderRuntime::StartKnownMove(int32 ID,double Now,bool FollowingMarch)
{
 const bool DefendMarch=KnownMoveDefendMarching(ID,FollowingMarch);
 const bool DefendAtPost=KnownMoveDefendAtPost(ID);
 // Finish returning to the personal post before beginning another detour.
 if(const auto* PostResume=KnownDefendResumes.Find(ID);PostResume && PostResume->bDefendPost)
 {SightExperiment.Cover(TEXT("known_move_defend_post_gate_returning"));return false;}
 // Preserve existing totals; the parallel namespace counts only marching defenders.
 auto RecordMove=[&](const TCHAR* MoveMetric)
 {
  SightExperiment.Cover(MoveMetric);
  if(DefendMarch && SightExperiment.Enabled && !FString(MoveMetric).StartsWith(TEXT("known_move_defend_")))
   SightExperiment.Cover(*FString(MoveMetric).Replace(TEXT("known_move_"),TEXT("known_move_defend_march_")));
 };
 if(!KnownMoveAllowed(ID,true)){if(DefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_gate_not_allowed"));return false;}
 if(KnownMoves.Contains(ID) || CoverApproaches.Contains(ID)){RecordMove(TEXT("known_move_gate_active"));return false;}
 if(Now<KnownMoveAfter.FindRef(ID))
 {RecordMove(TEXT("known_move_gate_cooldown"));RecordMove(Now<KnownMoveEndCooldown.FindRef(ID)?TEXT("known_move_gate_end_cooldown"):TEXT("known_move_gate_search_interval"));return false;}
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Unit=Store->Units.Find(ID);
 if(Unit->CoverPhase==1 || Unit->CoverPhase==3){RecordMove(TEXT("known_move_gate_cover"));return false;}
 if(Unit->Ammo<=0 || Unit->ReloadEndTime>0 || Now<Unit->NextShotTime){RecordMove(TEXT("known_move_gate_ammo"));return false;}
 const auto* Squad=Store->FindSquad(Unit->SquadID);const auto* Plan=Plans.Find(Unit->SquadID);if(!Plan){RecordMove(TEXT("known_move_gate_plan"));return false;}
 // March=0 retains the original in-flight gate. The caller also retains its route/arrival gate.
 if(!BattleKnownFire::MoveMarch() && Squad->Order==EBattleOrder::AttackMove && Plan->Path.IsValidIndex(Bodies.FindChecked(ID).Waypoint) &&
  !Plan->bCoverStop && Squad->TacticalPlan.Action!=EBattleSquadTactic::Engage && FVector::DistSquared2D(Position(ID),Unit->Destination)>FMath::Square(100.)){RecordMove(TEXT("known_move_gate_march"));RecordMove(TEXT("known_move_gate_march_attack_move"));return false;}
 // Follow supplies its actual route/arrival movement gate; preserve it in the episode.
 const bool March=(BattleKnownFire::MoveMarch() && KnownMoveMarching(ID)) || DefendMarch;
 KnownMoveAfter.Add(ID,Now+1.);
 int32 Living=0,Moving=0;
 for(int32 Member:Squad->Members)if(const auto* Other=Store->Find(Member);Other && Other->IsAlive())
 {++Living;if(Member!=ID && (Other->CoverPhase==1 || CoverApproaches.Contains(Member)))++Moving;}
 const int32 Limit=BattleKnownFire::MoveMax()>0?FMath::Min(Living/2,BattleKnownFire::MoveMax()):Living/2;
 if(Moving>=Limit){RecordMove(TEXT("known_move_squad_busy"));RecordMove(TEXT("known_move_gate_members"));return false;}
 const double CommitSeconds=BattleKnownFire::MoveCommit();
 if(CommitSeconds>0. && SightExperiment.Enabled)
  for(const TCHAR* CommitMetric:{TEXT("known_move_commit_walks"),TEXT("known_move_commit_arrived"),TEXT("known_move_commit_fired"),TEXT("known_move_commit_timeout"),TEXT("known_move_refreshed"),TEXT("known_move_switched"),TEXT("known_move_arrived_unseen")})SightExperiment.CoverDiag.FindOrAdd(FName(CommitMetric));
 const FVector Start=Position(ID);
 const double WalkSpeed=CommitSeconds>0.?FMath::Max(1.,double(GetDefault<UBattleTuning>()->MoveSpeed*BattleSquadType::SpeedAt(Squad->SquadType,GetWorld(),Start))):1.;
 const auto& Team=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];
 const double Reach=BattleRanges::Engage(Unit->Weapon),SearchRange=Reach+BattleKnownFire::MoveRange();
 TArray<int32> Observed,Candidates;
 const FIntPoint Low(FMath::FloorToInt((Start.X-SearchRange)/5000.),FMath::FloorToInt((Start.Y-SearchRange)/5000.));
 const FIntPoint High(FMath::FloorToInt((Start.X+SearchRange)/5000.),FMath::FloorToInt((Start.Y+SearchRange)/5000.));
 for(int32 X=Low.X;X<=High.X;++X)for(int32 Y=Low.Y;Y<=High.Y;++Y)
  if(const auto* Bucket=Team.Grid.Find(FIntPoint(X,Y)))for(int32 TargetID:*Bucket)
  {
   const auto& Target=Team.Targets.FindChecked(TargetID);if(!Target.Fresh(Now)){if(DefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_target_stale"));continue;}Observed.Add(TargetID);
   const double CandidateReach=KnownMoveDefendReach(ID);
   const FVector CandidatePost=KnownMoveReachCenter(ID);
   const bool ReachTarget=CandidateReach>0. && FVector::DistSquared2D(CandidatePost,Target.At(Now))<=
    FMath::Square(FMath::Max(0.,double(Squad->DefenseRadius))+CandidateReach+Reach);
   if(March && !ReachTarget && !(FVector::DistSquared(Start,Target.At(Now))<=FMath::Square(Reach)))
   {if(DefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_target_range"));continue;}
   if(!KnownMoveTargetAllows(ID,TargetID,Target.At(Now)))
   {if(DefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_target_order"));continue;}
   if(KnownMoveDefendReach(ID)>0. && (!WeaponOrderAllows(ID,TargetID,Target.At(Now)) ||
    (March && FVector::DistSquared(Start,Target.At(Now))>FMath::Square(Reach))))SightExperiment.Cover(TEXT("known_move_defend_reach_targets"));
   Candidates.Add(TargetID);
  }
 Candidates.Sort([&](int32 Left,int32 Right){const double L=FVector::DistSquared2D(Start,Team.Targets.FindChecked(Left).At(Now)),R=FVector::DistSquared2D(Start,Team.Targets.FindChecked(Right).At(Now));return L==R?Left<Right:L<R;});
 if(Candidates.Num()>2)Candidates.SetNum(2,EAllowShrinking::No);
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if(Candidates.IsEmpty()){RecordMove(TEXT("known_move_gate_no_candidate"));return false;}
 if(!Nav || !Cover->IsBuilt()){RecordMove(TEXT("known_move_reject_setup"));RecordMove(TEXT("known_move_gate_setup"));return false;} // slot prebuild is not hidden work in a per-soldier budget
 if(!KnownMoveReserve(ID,Now)){if(DefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_budget_deferred"));KnownMoveAfter.Add(ID,Now+.1);return false;}
 RecordMove(TEXT("known_move_searches"));
 if(DefendAtPost)SightExperiment.Cover(TEXT("known_move_defend_post_searches"));
 auto FinishBudget=[&](){KnownMoveRelease(ID);};
 // Source: edge / inferred opening / ordinary slot / ring. Geometry queries do not trace.
 struct FPlace {FVector At;int32 Slot;uint8 Source;FBox SourceBox;int32 FaceAxis;double FaceSign,EndSign;};TArray<FPlace> Places;
 const FVector ObservedGoal=Team.Targets.FindChecked(Candidates[0]).At(Now);
 const auto& Heights=BattleFieldLayout::Of(GetWorld()).Heights;
 const TArray<FBox> Boxes=Cover->KnownMoveGeometry(Start,BattleKnownFire::MoveRange()+200.);
 auto AddPlace=[&](FVector Point,int32 PlaceSlot,uint8 SourceKind,FBox CandidateBox=FBox(ForceInit),int32 CandidateFaceAxis=INDEX_NONE,double CandidateFaceSign=0.,double CandidateEndSign=0.)
 {
  if(FVector::DistSquared2D(Start,Point)>FMath::Square(BattleKnownFire::MoveRange()))return;
  for(const FPlace& Existing:Places)if(FVector::DistSquared(Existing.At,Point)<FMath::Square(60.))return;
  Places.Add({Point,PlaceSlot,SourceKind,CandidateBox,CandidateFaceAxis,CandidateFaceSign,CandidateEndSign});
 };
 // Corners sit 50 cm inside an end, 95 cm off its face: the existing 1-1.2 m lean can clear it.
 for(int32 EdgeBoxIndex=0;!bKnownMoveTrialSkipEdges && EdgeBoxIndex<FMath::Min(24,Boxes.Num());++EdgeBoxIndex)
 {
  const FBox& Box=Boxes[EdgeBoxIndex];
  for(int32 FaceAxis=0;FaceAxis<2;++FaceAxis)for(double FaceSign:{-1.,1.})for(double EndSign:{-1.,1.})
  {
   const int32 AlongAxis=1-FaceAxis;const FVector BoxCentre=Box.GetCenter(),BoxExtent=Box.GetExtent();
   if(BoxExtent[AlongAxis]<100.)continue;
   FVector EdgeAt=BoxCentre;EdgeAt.Z=Start.Z;
   EdgeAt[FaceAxis]+=FaceSign*(BoxExtent[FaceAxis]+95.);EdgeAt[AlongAxis]+=EndSign*(BoxExtent[AlongAxis]-50.);
   // Shift the anchor along the face so an oblique observed ray grazes the end within lean reach.
   const double FacePlane=BoxCentre[FaceAxis]+FaceSign*BoxExtent[FaceAxis],EndPlane=BoxCentre[AlongAxis]+EndSign*BoxExtent[AlongAxis];
   const double Across=ObservedGoal[FaceAxis]-FacePlane;
   if(FMath::Abs(Across)>100.)EdgeAt[AlongAxis]+=(ObservedGoal[AlongAxis]-EndPlane)*(EdgeAt[FaceAxis]-FacePlane)/Across;
   // Keep the near face. Opposite faces would require walking through/around the whole obstacle.
   if((Start[FaceAxis]-BoxCentre[FaceAxis])*FaceSign<0.)continue;
   // Match ring heights after XY is final. With no field, preserve the starting floor
   // as rings do; Nav projection and the full path/sweeps still validate connectivity.
   if(!Heights.Field.IsEmpty())EdgeAt.Z=Heights.Base(EdgeAt.X,EdgeAt.Y)+90.;
   if(Box.Max.Z<EdgeAt.Z-30. || Box.Min.Z>EdgeAt.Z+60.)continue;
   bool Opening=false;
   if(BoxExtent[FaceAxis]<=100.)for(const FBox& Adjacent:Boxes)
   {
    const double Gap=EndSign>0.?Adjacent.Min[AlongAxis]-Box.Max[AlongAxis]:Box.Min[AlongAxis]-Adjacent.Max[AlongAxis];
    if(Gap>=90. && Gap<=600. && FMath::Abs(Adjacent.GetCenter()[FaceAxis]-BoxCentre[FaceAxis])<10. && Adjacent.GetExtent()[FaceAxis]<=100. && Adjacent.Min.Z<=EdgeAt.Z+60. && Adjacent.Max.Z>=EdgeAt.Z+60.){Opening=true;break;}
   }
   AddPlace(EdgeAt,INDEX_NONE,Opening?1:0,Box,FaceAxis,FaceSign,EndSign);
  }
 }
 for(int32 Slot:Cover->NearbySlots(Start,float(BattleKnownFire::MoveRange()),24))AddPlace(Cover->GetSlots()[Slot]+FVector(0,0,90),Slot,2);
 const FVector Toward=(ObservedGoal-Start).GetSafeNormal2D(),Side=FVector::CrossProduct(Toward,FVector::UpVector);
 for(double RingRadius:{500.,1000.,1500.,BattleKnownFire::MoveRange()})
 {
  if(RingRadius>BattleKnownFire::MoveRange() || RingRadius<=0.)continue;
  for(double ForwardPart:{.6,0.,-.6})for(double SideSign:{-1.,1.})
  {
   FVector RingAt=Start+(Toward*ForwardPart+Side*SideSign).GetSafeNormal2D()*(RingRadius*.999999);
   if(!Heights.Field.IsEmpty())RingAt.Z=Heights.Base(RingAt.X,RingAt.Y)+90.;
   AddPlace(RingAt,INDEX_NONE,3);
  }
 }
 Places.StableSort([&](const FPlace& Left,const FPlace& Right)
 {
  const int32 LeftRank=Left.Source<2?0:int32(Left.Source),RightRank=Right.Source<2?0:int32(Right.Source);
  if(CommitSeconds>0.)
  {
   const auto& RankTarget=Team.Targets.FindChecked(Candidates[0]);
   const double WalkRoom=RankTarget.ObservedAt+CommitSeconds-Now;
   const bool LeftFits=FVector::Dist2D(Start,Left.At)/WalkSpeed<=WalkRoom,RightFits=FVector::Dist2D(Start,Right.At)/WalkSpeed<=WalkRoom;
   if(LeftFits!=RightFits)return LeftFits;
  }
  if(LeftRank!=RightRank)return LeftRank<RightRank;
  const double LeftThreat=FVector::DistSquared2D(Left.At,ObservedGoal),RightThreat=FVector::DistSquared2D(Right.At,ObservedGoal);
  if(LeftThreat!=RightThreat)return LeftThreat<RightThreat;
  return FVector::DistSquared2D(Left.At,Start)<FVector::DistSquared2D(Right.At,Start);
 });
 if(Places.IsEmpty()){RecordMove(TEXT("known_move_gate_no_place"));FinishBudget();return false;}
 const int32 CycleSize=Places.Num()*Candidates.Num();
 int32 Turn=KnownMoveCursor.FindRef(ID)%CycleSize;
 // Up to eight transactions, with a fresh atomic reservation for each. FIFO still wins over batching.
 for(int32 Attempt=0;Attempt<FMath::Min(8,CycleSize);++Attempt)
 {
  if(Attempt>0){FinishBudget();if(!KnownMoveReserve(ID,Now)){if(DefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_budget_deferred"));break;}}
  const int32 PlaceAt=Turn/Candidates.Num(),TargetID=Candidates[Turn%Candidates.Num()];const FPlace& Place=Places[PlaceAt];
  Turn=(Turn+1)%CycleSize;KnownMoveCursor.Add(ID,Turn);
  KnownMoveAfter.Add(ID,Now+(Turn==0?1.:.1));
  FVector Anchor=Place.At;const bool Free=Place.Slot==INDEX_NONE;
  auto RejectPath=[&](const TCHAR* PathReason)
  {
   RecordMove(PathReason);
   if(SightExperiment.Enabled && Place.Source<2)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_EDGE_PATH_REJECT unit=%d reason=%s start=%s raw=%s projected=%s box_min=%s box_max=%s face_axis=%d face_sign=%.0f end_sign=%.0f"),
    ID,PathReason,*Start.ToCompactString(),*Place.At.ToCompactString(),*Anchor.ToCompactString(),*Place.SourceBox.Min.ToCompactString(),*Place.SourceBox.Max.ToCompactString(),Place.FaceAxis,Place.FaceSign,Place.EndSign);
  };
  RecordMove(Free?TEXT("known_move_candidate_free"):TEXT("known_move_candidate_slot"));
  if(Place.Source!=2)RecordMove(Place.Source==0?TEXT("known_move_candidate_edge"):Place.Source==1?TEXT("known_move_candidate_door"):TEXT("known_move_candidate_ring"));
  if(const TCHAR* MissionRejection=KnownMovePlaceRejection(ID,Start,Anchor,DefendMarch)){RecordMove(TEXT("known_move_reject_anchor_mission"));RecordMove(MissionRejection);continue;}
  if(FVector::Dist2D(Start,Anchor)<150.){RecordMove(TEXT("known_move_reject_too_close"));continue;}
  if(!Free && !Cover->IsAvailable(Place.Slot,ID,Now)){RecordMove(TEXT("known_move_reject_slot_busy"));continue;}
  bool Occupied=false;
  for(const auto& Walk:CoverApproaches)if(Walk.Key!=ID && ((Walk.Value.Slot==Place.Slot && !Free) || FVector::DistSquared2D(Walk.Value.Goal,Anchor)<FMath::Square(120.))){Occupied=true;break;}
  if(!Occupied)for(const auto& Settled:FirePositions)if(Settled.Key!=ID && FVector::DistSquared2D(Settled.Value,Anchor)<FMath::Square(120.)){Occupied=true;break;}
  if(!Occupied)for(const auto& Friendly:Store->GetUnits())if(Friendly.Key!=ID && Friendly.Value.Team==Unit->Team && Friendly.Value.IsAlive() && FVector::DistSquared2D(Position(Friendly.Key),Anchor)<FMath::Square(120.)){Occupied=true;break;}
  if(Occupied){RecordMove(TEXT("known_move_reject_occupied"));continue;}
  if(!KnownMoveSpend(ID,Now,2))break;
  FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor-FVector(0,0,90),Projected,FVector(50,50,150))){RejectPath(TEXT("known_move_reject_anchor_nav"));continue;}
  Anchor=Projected.Location+FVector(0,0,90);if(!KnownMovePlaceAllowed(ID,Start,Anchor,DefendMarch)){RecordMove(TEXT("known_move_reject_projected_mission"));continue;}
  do
  {
   if(bKnownMoveDeferred)break;
   const auto& Target=Team.Targets.FindChecked(TargetID);const FVector Threat=Target.At(Now);FVector Peek=Anchor;uint8 Kind=0;
   // This absolute deadline cannot be prolonged by observation refreshes during the walk.
   const double CommitDeadline=CommitSeconds>0.?Target.ObservedAt+CommitSeconds:0.;
   if(CommitSeconds>0. && Now+FVector::Dist2D(Start,Anchor)/WalkSpeed>CommitDeadline){RecordMove(TEXT("known_move_reject_commit_time"));continue;}
   if(FVector::DistSquared2D(Anchor,Threat)>FMath::Square(Reach)){RecordMove(TEXT("known_move_reject_anchor_range"));continue;}
   // Conservative sampled envelope: keep every possible lean/projection, reject only solid geometry.
   // CoverFirePoint accepts at most 120 cm in XY after projection, and projects +/-150 cm in Z.
   const bool UsesCover=Place.Source<3;
   bool TerrainBlocked=false,BoxBlocked=false;
   const FVector FilterFrom=Anchor+FVector(0,0,60),FilterTo=Threat+FVector(0,0,60);
   for(int32 Sample=1;Sample<=8 && !TerrainBlocked && !BoxBlocked;++Sample)
   {
    const double Fraction=double(Sample)/9.,Margin=UsesCover?120.*(1.-Fraction):0.,Vertical=UsesCover?150.*(1.-Fraction):0.;
    const FVector Mid=FMath::Lerp(FilterFrom,FilterTo,Fraction),Span(Margin+2.,Margin+2.,Vertical+2.);
    // Only the exact height field (no box approximation of hills or upper floors).
    if(!Heights.Field.IsEmpty() && !UsesCover && Heights.Field.Height(Mid.X,Mid.Y)>Mid.Z+2.)TerrainBlocked=true;
    for(const FBox& Solid:Boxes)
     if(Solid.IsInside(Mid+Span) && Solid.IsInside(Mid-Span)){BoxBlocked=true;break;}
   }
   if(TerrainBlocked || BoxBlocked){RecordMove(TerrainBlocked?TEXT("known_move_prefilter_terrain"):TEXT("known_move_prefilter_box"));continue;}
   // Allow at most min(5 m, 10% of current separation) closer to every observed enemy.
   auto Safe=[&](FVector Point)
   {for(int32 ThreatID:Observed){const FVector Seen=Team.Targets.FindChecked(ThreatID).At(Now);const double Separation=FVector::Dist2D(Start,Seen);if(FVector::Dist2D(Point,Seen)<Separation-FMath::Min(500.,Separation*.1))return false;}return true;};
   if(!Safe(Anchor)){RecordMove(TEXT("known_move_reject_anchor_danger"));continue;}
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
   if(MoreExposed){RecordMove(bKnownMoveDeferred?TEXT("known_move_reject_exposure_deferred"):TEXT("known_move_reject_more_exposed"));continue;}
   const bool CanFire=Place.Source==3?KnownMoveLine(ID,Anchor+FVector(0,0,60),Threat+FVector(0,0,60),Now):CoverFirePoint(ID,Anchor,Threat,Peek,Kind,3,false,Now);
   if(bKnownMoveDeferred){RecordMove(TEXT("known_move_reject_fire_deferred"));continue;}
   if(!CanFire){RecordMove(Place.Source==3?TEXT("known_move_reject_free_fire_line"):TEXT("known_move_reject_cover_fire_point"));continue;}
   if(!KnownMovePlaceAllowed(ID,Start,Peek,DefendMarch) || !KnownMoveSegmentAllowed(ID,Start,Anchor,Peek,DefendMarch)){RecordMove(TEXT("known_move_reject_peek_mission"));continue;}
   if(!Safe(Peek)){RecordMove(TEXT("known_move_reject_peek_danger"));continue;}
   if(FVector::DistSquared2D(Peek,Threat)>FMath::Square(Reach)){RecordMove(TEXT("known_move_reject_peek_range"));continue;}
   RecordMove(Place.Source==3?TEXT("known_move_fire_free"):Kind==1?TEXT("known_move_fire_over"):TEXT("known_move_fire_lean"));
   if(!KnownMoveSpend(ID,Now,1))break;
   if(GetWorld()->SweepTestByObjectType(Anchor,Anchor,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88))){RecordMove(TEXT("known_move_reject_anchor_capsule"));continue;}
   if(!KnownMoveSpend(ID,Now,2))break;
   UNavigationPath* Route=Nav->FindPathToLocationSynchronously(GetWorld(),Start-FVector(0,0,90),Projected.Location);
   if(!Route || !Route->IsValid() || Route->IsPartial() || Route->PathPoints.Num()<2){RejectPath(TEXT("known_move_reject_path_invalid"));continue;}
   double Length=0.;bool PathAllowed=true;
   for(int32 Index=1;Index<Route->PathPoints.Num();++Index)
   {Length+=FVector::Dist(Route->PathPoints[Index-1],Route->PathPoints[Index]);PathAllowed &= KnownMovePlaceAllowed(ID,Start,Route->PathPoints[Index]+FVector(0,0,90),DefendMarch) &&
     KnownMoveSegmentAllowed(ID,Start,Route->PathPoints[Index-1]+FVector(0,0,90),Route->PathPoints[Index]+FVector(0,0,90),DefendMarch);}
   if(!PathAllowed){RejectPath(TEXT("known_move_reject_path_mission"));continue;}
   if(CommitSeconds>0. && Now+Length/WalkSpeed>CommitDeadline){RecordMove(TEXT("known_move_reject_commit_time"));continue;}
   if(Length>1.5*FVector::Dist2D(Start,Anchor)){RejectPath(TEXT("known_move_reject_path_length"));continue;}
   // A finite transaction must also bound path complexity, independently of token availability.
   if(Route->PathPoints.Num()>18){RejectPath(TEXT("known_move_reject_path_complexity"));continue;}
   // The mesh can lag dynamic geometry. Validate every segment, with the same bounded sweep budget.
   for(int32 SegmentIndex=1;SegmentIndex<Route->PathPoints.Num();++SegmentIndex)
   {
    if(!KnownMoveSpend(ID,Now,1)){PathAllowed=false;break;}
    if(GetWorld()->SweepTestByObjectType(Route->PathPoints[SegmentIndex-1]+FVector(0,0,90),Route->PathPoints[SegmentIndex]+FVector(0,0,90),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88))){PathAllowed=false;break;}
   }
   if(!PathAllowed){RejectPath(bKnownMoveDeferred?TEXT("known_move_reject_path_deferred"):TEXT("known_move_reject_path_capsule"));continue;}
   // Reserve before publishing the approach; CancelCoverAction releases the old reservation first.
   const int32 OldSlot=Unit->CoverSlot;
   CancelCoverAction(ID);
   if(!Free && !Cover->Reserve(Place.Slot,ID,Now)){RecordMove(TEXT("known_move_reject_reservation"));if(OldSlot>=0)Cover->Reserve(OldSlot,ID,Now);continue;}
   FKnownMove Episode;Episode.Start=Start;Episode.Goal=Anchor;Episode.Last=Start;Episode.Peek=Peek;Episode.Observation=Target;Episode.Mission=Squad->MissionRevision;
   Episode.Source=Place.Source;Episode.SourceBox=Place.SourceBox;Episode.FaceAxis=Place.FaceAxis;Episode.FaceSign=Place.FaceSign;Episode.EndSign=Place.EndSign;
   if(DefendMarch){const FVector* EpisodePost=Plan->ArrivalGoals.Find(ID);Episode.DefendPost=EpisodePost?*EpisodePost:Unit->Destination;}
   const double SelectedReach=KnownMoveDefendReach(ID);
   const FVector SelectedPost=KnownMoveReachCenter(ID);
   Episode.bDefendReach=SelectedReach>0.;
   if(Episode.bDefendReach &&
    ((FVector::DistSquared2D(Anchor,Squad->Destination)>FMath::Square(double(Squad->DefenseRadius)) && FVector::DistSquared2D(Anchor,SelectedPost)<=FMath::Square(SelectedReach)) ||
     (FVector::DistSquared2D(Peek,Squad->Destination)>FMath::Square(double(Squad->DefenseRadius)) && FVector::DistSquared2D(Peek,SelectedPost)<=FMath::Square(SelectedReach))))SightExperiment.Cover(TEXT("known_move_defend_reach_places"));
   Episode.bDefendPost=DefendAtPost || (Episode.bDefendReach && !DefendMarch);Episode.bDefendMarch=DefendMarch;Episode.bMarch=March;Episode.Revision=Unit->OrderRevision;Episode.Started=Now;Episode.Slot=Place.Slot;Episode.bStandUp=StandUp || Place.Source<2;
   Episode.CommitUntil=CommitDeadline;
   if(CommitSeconds>0. || Episode.bDefendReach)for(int32 ReturnIndex=Route->PathPoints.Num()-1;ReturnIndex>=0;--ReturnIndex)Episode.ReturnPoints.Add(Route->PathPoints[ReturnIndex]+FVector(0,0,90));
   KnownMoves.Add(ID,Episode);
   if(DefendMarch){if(KnownDefendResumes.Remove(ID))RecordMove(TEXT("known_move_defend_march_resume_interrupted"));SightExperiment.Cover(TEXT("known_move_defend_march_started"));}
   if(DefendAtPost)SightExperiment.Cover(TEXT("known_move_defend_post_started"));
   FCoverApproach Approach;Approach.Goal=Anchor;Approach.Target=TargetID;Approach.Slot=Place.Slot;Approach.Revision=Unit->OrderRevision;
   Approach.Until=CommitSeconds>0.?CommitDeadline:Now+FMath::Max(8.,Length/100.+3.);Approach.bFree=Free;CoverApproaches.Add(ID,Approach);
   auto& Path=SoldierPaths.FindOrAdd(ID);Path=FSoldierPath();Path.Points=Route->PathPoints;
   FirePositions.Remove(ID);KnownFireSearches.Remove(ID);
   RecordMove(Place.Source==0?TEXT("known_move_pick_edge"):Place.Source==1?TEXT("known_move_pick_door"):Place.Source==2?TEXT("known_move_pick_slot"):TEXT("known_move_pick_ring"));
   Unit->DecisionReason=TEXT("KNOWN TARGET / MOVING TO FIRE");SightExperiment.Cover(TEXT("known_move_started"));
   if(Target.Tracked(Now))RecordMove(TEXT("known_move_tracked_plans"));
   FinishBudget();return true;
  }while(false);
  if(Turn==0)break;
 }
 FinishBudget();return false;
}

void UBattleLeaderRuntime::EndKnownMove(int32 ID,double Now,const TCHAR* Reason)
{
 const auto* Episode=KnownMoves.Find(ID);if(!Episode)return;
 // Arrival owns the fire request until it fires, returns, or is cancelled (including death).
 if((Episode->CommitUntil>0. || Episode->bDefendReach) && Episode->Arrived>=0)
  for(auto& EndingBudget:KnownFireBudgets){EndingBudget.Waiting.Remove(ID);EndingBudget.RequestedAt.Remove(ID);}
 SightExperiment.Cover(Reason);
 if(SightExperiment.Enabled && Episode->Arrived<0){SightExperiment.KnownMoveDistance+=Episode->Distance;SightExperiment.KnownMoveSeconds+=Now-Episode->Started;}
 if(SightExperiment.Enabled)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_END unit=%d target=%d reason=%s distance_cm=%.3f time=%.3f"),ID,Episode->Observation.TargetID,Reason,Episode->Distance,Now-Episode->Started);
 const auto* EndingUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* EndingApproach=CoverApproaches.Find(ID);
 const bool OwnApproach=EndingApproach && EndingApproach->Target==Episode->Observation.TargetID &&
  EndingApproach->Revision==Episode->Revision && EndingApproach->Slot==Episode->Slot &&
  EndingApproach->Goal==Episode->Goal && !EndingApproach->bCharge && !EndingApproach->bAssault;
 if(Episode->Arrived<0 && (OwnApproach || (!EndingApproach && (!EndingUnit || !EndingUnit->CoverPhase))))GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);
 else if(Episode->Arrived>=0 && (Episode->CommitUntil>0. || Episode->bMarch || Episode->bDefendPost || FCString::Strncmp(Reason,TEXT("known_move_cancel_"),18)==0))
 {
  const auto* EndedUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
  if(EndedUnit && EndedUnit->CoverOrderRevision==Episode->Revision && (((Episode->bMarch || Episode->bDefendPost) && EndedUnit->CoverAnchor==Episode->Goal) || (EndedUnit->CoverTargetID==Episode->Observation.TargetID && EndedUnit->CoverPeek==Episode->Peek)))CancelCoverAction(ID);
  if(const auto* EndedPosition=FirePositions.Find(ID);EndedPosition && *EndedPosition==Episode->Goal)FirePositions.Remove(ID);
 }
 if(OwnApproach){CoverApproaches.Remove(ID);SoldierPaths.Remove(ID);}
 if((Episode->bMarch || Episode->bDefendPost) && (!EndingApproach || OwnApproach))SoldierPaths.Remove(ID); // discard the detour; normal movement rebuilds the mission route
 if(!EndingApproach || OwnApproach)
  if(auto* EndBody=Bodies.Find(ID);EndBody && EndBody->Actor.IsValid())EndBody->Actor->GetCharacterMovement()->StopMovementImmediately();
 if((Episode->bDefendMarch || Episode->bDefendPost) && EndingUnit && EndingUnit->IsAlive() && EndingUnit->OrderRevision==Episode->Revision)
 {
  const auto* ResumeSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(EndingUnit->SquadID);
  if(ResumeSquad && ResumeSquad->Order==EBattleOrder::Defend && ResumeSquad->MissionRevision==Episode->Mission)
  {
   FKnownDefendResume Resume;Resume.Goal=EndingUnit->Destination;Resume.ArrivalGoal=Resume.Goal;Resume.Mission=Episode->Mission;Resume.Revision=Episode->Revision;
   Resume.bDefendPost=Episode->bDefendPost;
   Resume.Best=FVector::Dist2D(Position(ID),Resume.Goal);Resume.ProgressAt=Now;KnownDefendResumes.Add(ID,Resume);
   SightExperiment.Cover(Episode->bDefendPost?TEXT("known_move_defend_post_resumed"):TEXT("known_move_defend_march_resumed"));
  }
 }
 KnownMoves.Remove(ID);KnownMoveAfter.Add(ID,Now+5.);KnownMoveEndCooldown.Add(ID,Now+5.);
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
 const bool Fresh=Current && Current->Fresh(Now) && KnownMoveTargetAllows(ID,Current->TargetID,Current->At(Now));
 const bool Commit=Episode->CommitUntil>0.;
 if(Commit && Current && Current->Fresh(Now) && (Current->ObservedAt>Episode->Observation.ObservedAt ||
  (Current->ObservedAt==Episode->Observation.ObservedAt && Current->TrackSampleAt>Episode->Observation.TrackSampleAt)))
 {Episode->Observation=*Current;SightExperiment.Cover(TEXT("known_move_refreshed"));}
 if(Commit && !KnownMoveTargetAllows(ID,Episode->Observation.TargetID,Episode->Observation.At(Now)))
 {EndKnownMove(ID,Now,TEXT("known_move_cancel_order"));return false;}
 if((Episode->bMarch || Episode->bDefendPost) && ((!Fresh && !Commit) || Episode->bFired))
 {EndKnownMove(ID,Now,!Fresh && !Commit?TEXT("known_move_cancel_stale"):TEXT("known_move_finished"));return false;}
 if(Episode->bMarch && (!KnownMovePlaceAllowed(ID,Episode->Start,Position(ID)) || !KnownMovePlaceAllowed(ID,Episode->Start,Episode->Peek)))
 {EndKnownMove(ID,Now,TEXT("known_move_cancel_mission"));return false;}
 if((Commit || Episode->bDefendReach) && (!KnownMovePlaceAllowed(ID,Episode->Start,Position(ID)) || !KnownMovePlaceAllowed(ID,Episode->Start,Episode->Goal) || !KnownMovePlaceAllowed(ID,Episode->Start,Episode->Peek)))
 {EndKnownMove(ID,Now,TEXT("known_move_cancel_mission"));return false;}
 if(Episode->Arrived>=0)
 {
  if(Commit || Episode->bDefendReach)return StepKnownMoveArrival(ID,Dt,Now);
  if(Episode->bMarch || Episode->bDefendPost)
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
 if(Commit && Now>Episode->CommitUntil)
 {SightExperiment.Cover(TEXT("known_move_commit_timeout"));EndKnownMove(ID,Now,TEXT("known_move_cancel_timeout"));return false;}
 if(Commit && !Fresh && !Episode->bCommittedWalk)
 {Episode->bCommittedWalk=true;SightExperiment.Cover(TEXT("known_move_commit_walks"));}
 if(!Fresh && !Commit && !Episode->bReturning)
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
  if(Episode->bDefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_arrived"));
  if(Episode->bDefendPost)SightExperiment.Cover(TEXT("known_move_defend_post_arrived"));
  if(Episode->bCommittedWalk)SightExperiment.Cover(TEXT("known_move_commit_arrived"));
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

// A committed arrival owns its peek/return movement and never lends stale data to ordinary firing.
bool UBattleLeaderRuntime::StepKnownMoveArrival(int32 ID,float Dt,double Now)
{
 auto& Episode=KnownMoves.FindChecked(ID);auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto& Unit=Store->Units.FindChecked(ID);
 auto& Body=Bodies.FindChecked(ID);auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();
 auto ReleaseFireQueue=[&](){auto& ArrivalBudget=KnownFireBudgets[Unit.Team];ArrivalBudget.Waiting.Remove(ID);ArrivalBudget.RequestedAt.Remove(ID);};
 if(Episode.bFired){EndKnownMove(ID,Now,TEXT("known_move_finished"));return true;}
 if(Episode.Slot!=INDEX_NONE && !Cover->Reserve(Episode.Slot,ID,Now)){EndKnownMove(ID,Now,TEXT("known_move_cancel_reservation"));return false;}
 // Only the previously certified peek and reverse route are used; no new navigation or geometry search.
 auto WalkTo=[&](FVector WalkGoal)
 {
  if(FVector::Dist2D(Position(ID),WalkGoal)<1.)return true;
  const FVector WalkBefore=Position(ID),WalkDirection=(WalkGoal-WalkBefore).GetSafeNormal2D();
  const auto* WalkSquad=Store->FindSquad(Unit.SquadID);
  const float WalkRate=GetDefault<UBattleTuning>()->MoveSpeed*BattleSquadType::SpeedAt(WalkSquad->SquadType,GetWorld(),WalkBefore);
  if(Body.Actor.IsValid())
  {
   if(FVector::Dist2D(WalkBefore,WalkGoal)<40.)
   {
    // Match precise cover peeking: clamp the swept final step, never persist a reduced speed.
    Body.Actor->GetCharacterMovement()->StopMovementImmediately();FHitResult WalkHit;
    Body.Actor->AddActorWorldOffset(WalkDirection*FMath::Min(WalkRate*Dt,float(FVector::Dist2D(WalkBefore,WalkGoal))),true,&WalkHit);
    Body.Velocity=(Position(ID)-WalkBefore)/FMath::Max(Dt,.001f);
   }
   else{Body.Actor->AddMovementInput(WalkDirection);Body.Velocity=Body.Actor->GetVelocity();}
  }
  else
  {
   FVector WalkDelta=WalkDirection*FMath::Min(WalkRate*Dt,float(FVector::Dist2D(WalkBefore,WalkGoal)));if(!TerrainStep(WalkBefore,WalkDelta))WalkDelta=FVector::ZeroVector;
   FHitResult WalkHit;Body.Capsule->MoveComponent(WalkDelta,WalkDirection.Rotation().Quaternion(),true,&WalkHit);
   auto& WalkFragment=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Body.Entity);WalkFragment.Position=Body.Capsule->GetComponentLocation();WalkFragment.Forward=WalkDirection;
   Body.Velocity=(WalkFragment.Position-WalkBefore)/FMath::Max(Dt,.001f);
  }
  Store->SetAction(ID,EBattleAction::Moving);return false;
 };
 if(Episode.bReturning)
 {
  if(Now>Episode.ReturnUntil){EndKnownMove(ID,Now,TEXT("known_move_cancel_return_timeout"));return false;}
  // Cover anchors are the certified shelter. A free firing position retraces the cached approach.
  const bool HasShelter=Episode.Source<3 || Episode.bStandUp;
  const FVector ReturnGoal=HasShelter?Episode.Goal:Episode.ReturnPoints.IsValidIndex(Episode.ReturnNext)?Episode.ReturnPoints[Episode.ReturnNext]:Episode.Start;
  if(!KnownMovePlaceAllowed(ID,Episode.Start,ReturnGoal)){EndKnownMove(ID,Now,TEXT("known_move_cancel_mission"));return false;}
  if(!WalkTo(ReturnGoal))return true;
  if(!HasShelter && ++Episode.ReturnNext<Episode.ReturnPoints.Num())return true;
  SetStance(ID,BattleStance::Crouch,Now,true);EndKnownMove(ID,Now,TEXT("known_move_returned"));return true;
 }
 if(!Episode.bAtPeek)
 {
  SetStance(ID,BattleStance::Stand,Now,true);
  if(!WalkTo(Episode.Peek))
  {
   if(Now-Episode.Arrived>5.){ReleaseFireQueue();Episode.bReturning=true;Episode.ReturnUntil=Now+12.;SightExperiment.Cover(TEXT("known_move_arrived_unseen"));}
   return true;
  }
  Episode.bAtPeek=true;Episode.CheckUntil=Now+BattleKnownFire::MoveWait();
  if(Body.Actor.IsValid())Body.Actor->GetCharacterMovement()->StopMovementImmediately();
  Body.Velocity=FVector::ZeroVector;Unit.CoverPhase=Episode.Source<3 || Episode.bStandUp?4:0;
 }
 if(Episode.bDefendReach && Episode.CommitUntil>0. && Now>Episode.CommitUntil)
 {ReleaseFireQueue();EndKnownMove(ID,Now,TEXT("known_move_cancel_timeout"));return false;}
 if(Now>Episode.CheckUntil)
 {ReleaseFireQueue();Episode.bReturning=true;Episode.ReturnUntil=Now+12.;if(!Episode.bArrivalSeen)SightExperiment.Cover(TEXT("known_move_arrived_unseen"));return true;}
 if(Episode.bDefendReach && !KnownMoveTargetAllows(ID,Episode.Observation.TargetID,Episode.Observation.At(Now)))
 {ReleaseFireQueue();EndKnownMove(ID,Now,TEXT("known_move_cancel_order"));return false;}
 const FVector ObservedPoint=Episode.Observation.At(Now),WatchDirection=(ObservedPoint-Position(ID)).GetSafeNormal2D();
 if(Body.Actor.IsValid())Body.Actor->SetActorRotation(WatchDirection.Rotation());
 else Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Body.Entity).Forward=WatchDirection;
 Store->SetAction(ID,EBattleAction::Idle);
 if(Now<Episode.NextCheck || Unit.NextShotTime>Now || Unit.ReloadEndTime>0 || IsPinned(ID,Now))return true;
 if(Unit.Ammo<=0){Store->StartReload(ID,Now);return true;}
 Episode.NextCheck=Now+.4;
 if(FVector::DistSquared(Position(ID),ObservedPoint)>FMath::Square(BattleRanges::Engage(Unit.Weapon)))return true;
 // Enemy truth is accessed only by the personal sensor; observation coordinates never authorize a shot.
 const auto Sensor=Sight(ID);FVector SensorEnd=FVector::ZeroVector;
 SightExperiment.Cover(TEXT("known_fire_sensor_checks"));
 if(!PrepareSightTrace(Sensor,Episode.Observation.TargetID,SensorEnd)){ReleaseFireQueue();return true;}
 if(!KnownFireRay(ID,Now)){Episode.NextCheck=Now+.1;return true;}
 SightExperiment.Cover(TEXT("known_fire_sensor_rays"));
 if(!SightHitVisible(TraceSight(Sensor,SensorEnd,Episode.Observation.TargetID,SightFromCanSee),Episode.Observation.TargetID)){ReleaseFireQueue();return true;}
 Episode.bArrivalSeen=true;
 if(!KnownFireRay(ID,Now)){Episode.NextCheck=Now+.1;return true;}
 ReleaseFireQueue();
 const FVector Muzzle=Position(ID)+FVector(0,0,BattleStance::EyeOffset(StanceOf(ID)));
 auto AimSensor=Sensor;AimSensor.Eye=Muzzle;SightExperiment.Cover(TEXT("known_fire_aim_rays"));
 if(!SightHitVisible(TraceSight(AimSensor,SensorEnd,Episode.Observation.TargetID,SightFromCanSee),Episode.Observation.TargetID))return true;
 BattleKnownFire::FResult Confirmed;Confirmed.TargetID=Episode.Observation.TargetID;Confirmed.SourceUnitID=Episode.Observation.SourceUnitID;
 Confirmed.AimPosition=SensorEnd;Confirmed.CheckedAt=Now;Confirmed.Revision=Episode.Observation.Revision;
 Confirmed.bTracked=Episode.Observation.Tracked(Now);Confirmed.Reject=BattleKnownFire::EReject::None;
 const FVector ConfirmedDirection=(SensorEnd-Muzzle).GetSafeNormal();Store->UpdateAim(ID,Now,0.f,ConfirmedDirection,true);
 if(Body.Actor.IsValid())Body.Actor->NoteDisplayAim(ConfirmedDirection,Now);
 FireKnownTarget(ID,Muzzle,Confirmed,Unit.CoverPhase==4);Store->SetAction(ID,EBattleAction::Engaging);return true;
}

void UBattleLeaderRuntime::KnownMoveShot(int32 ID,int32 Target,double Now)
{
 auto* Episode=KnownMoves.Find(ID);if(!Episode || Episode->Arrived<0 || Episode->bFired)return;
 if(Episode->bDefendMarch)SightExperiment.Cover(TEXT("known_move_defend_march_fired"));
 if(Episode->bDefendPost)SightExperiment.Cover(TEXT("known_move_defend_post_fired"));
 if(Target!=Episode->Observation.TargetID){SightExperiment.Cover(TEXT("known_move_arrived_other_shot"));if(Episode->bMarch || Episode->bDefendPost)Episode->bFired=true;return;}
 Episode->bFired=true;SightExperiment.Cover(TEXT("known_move_arrived_fired"));
 if(Episode->bDefendReach)SightExperiment.Cover(TEXT("known_move_defend_reach_fired"));
 if(Episode->bCommittedWalk)SightExperiment.Cover(TEXT("known_move_commit_fired"));
 if(SightExperiment.Enabled)SightExperiment.KnownMoveFirstShotSeconds+=Now-Episode->Arrived;
 if(SightExperiment.Enabled)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KNOWN_MOVE_SHOT unit=%d target=%d known_move_first_shot_time=%.6f"),ID,Target,Now-Episode->Arrived);
}
