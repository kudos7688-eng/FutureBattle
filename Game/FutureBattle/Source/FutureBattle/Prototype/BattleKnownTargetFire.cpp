// Trial -BattleKnownTargetFire=1: observation-ranked candidates, personal sensor, then a line to the selected aim.
// No navigation, movement, squad-plan writes, or random draws. All additional traces pass KnownFireRay.
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleSquadTactics.h"

bool UBattleLeaderRuntime::KnownFireAllowed(int32 ID,const BattleKnownFire::FTarget& Target,double Now)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);
 if(!Unit || !Unit->IsAlive() || Unit->Team<0 || Unit->Team>1 ||
  (Unit->ActiveWeapon()!=EBattleWeapon::Carbine && Unit->ActiveWeapon()!=EBattleWeapon::SupportGun) || Unit->bLauncherSelected || IsVehicle(ID))return false;
 if(!Target.Fresh(Now)){SightExperiment.Cover(TEXT("known_fire_reject_stale"));return false;}
 const FVector Point=Target.At(Now);
 if(Unit->Order==EBattleOrder::Move || Unit->bSoldierOrder || !WeaponOrderAllows(ID,Target.TargetID,Point))
 {SightExperiment.Cover(TEXT("known_fire_reject_order"));return false;}
 const double Range=UnderFire(ID)?BattleRanges::Reach(Unit->Weapon):BattleRanges::Engage(Unit->Weapon);
 if(FVector::DistSquared(Position(ID),Point)>FMath::Square(Range))
 {SightExperiment.Cover(TEXT("known_fire_reject_range"));return false;}
 // Stage 1 never turns an existing cover into protection against a new flank. Keep alternatives within 45 degrees.
 if(Unit->CoverPhase)
 {
  auto& CoverSearch=KnownFireSearches.FindOrAdd(ID);
  if(!CoverSearch.bCoverFacing || CoverSearch.CoverAnchor!=Unit->CoverAnchor || CoverSearch.CoverPeek!=Unit->CoverPeek ||
   CoverSearch.CoverTarget!=Unit->CoverTargetID || CoverSearch.CoverRevision!=Unit->CoverOrderRevision)
  {
   CoverSearch.bCoverFacing=true;CoverSearch.CoverAnchor=Unit->CoverAnchor;CoverSearch.CoverPeek=Unit->CoverPeek;
   CoverSearch.CoverTarget=Unit->CoverTargetID;CoverSearch.CoverRevision=Unit->CoverOrderRevision;CoverSearch.CoverFacing=Forward(ID).GetSafeNormal2D();
  }
  if(FVector::DotProduct(CoverSearch.CoverFacing,(Point-Unit->CoverAnchor).GetSafeNormal2D())<.70710678)
  {SightExperiment.Cover(TEXT("known_fire_reject_cover_direction"));return false;}
 }
 else if(auto* CoverSearch=KnownFireSearches.Find(ID))CoverSearch->bCoverFacing=false;
 return true;
}

void UBattleLeaderRuntime::KnownFireCandidates(int32 ID,double Now,TArray<int32>& Out)
{
 Out.Reset();const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!Unit || Unit->Team<0 || Unit->Team>1)return;
 const auto& Team=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];
 const FVector From=Position(ID),Facing=Forward(ID).GetSafeNormal2D();
 const double Range=UnderFire(ID)?BattleRanges::Reach(Unit->Weapon):BattleRanges::Engage(Unit->Weapon);
 const FIntPoint Min(FMath::FloorToInt((From.X-Range)/5000.),FMath::FloorToInt((From.Y-Range)/5000.));
 const FIntPoint Max(FMath::FloorToInt((From.X+Range)/5000.),FMath::FloorToInt((From.Y+Range)/5000.));
 struct FChoice {int32 Candidate=INDEX_NONE;double Score=0;};TArray<FChoice,TInlineAllocator<4>> Best;
 for(int32 X=Min.X;X<=Max.X;++X)for(int32 Y=Min.Y;Y<=Max.Y;++Y)
  if(const auto* Bucket=Team.Grid.Find(FIntPoint(X,Y)))for(int32 TargetID:*Bucket)
  {
   const auto& Target=Team.Targets.FindChecked(TargetID);if(!KnownFireAllowed(ID,Target,Now))continue;
   SightExperiment.Cover(TEXT("known_fire_candidates"));if(Target.Tracked(Now))SightExperiment.Cover(TEXT("known_fire_tracked_candidates"));
   const FVector Delta=Target.At(Now)-From;
   const double Score=Delta.SizeSquared()*(1.+.25*(1.-FVector::DotProduct(Facing,Delta.GetSafeNormal2D())));
   int32 Place=0;while(Place<Best.Num() && (Best[Place].Score<Score || (Best[Place].Score==Score && Best[Place].Candidate<TargetID)))++Place;
   if(Place<3){Best.Insert(FChoice{TargetID,Score},Place);if(Best.Num()>3)Best.SetNum(3,EAllowShrinking::No);}
  }
 if(Unit->CoverPhase)
 {
  TArray<int32,TInlineAllocator<3>> Preferred;
  // Do not call ArmsRoleTarget/OwnTarget here: their legacy paths read enemy truth.
  if(const auto* Plan=Plans.Find(Unit->SquadID))Preferred.AddUnique(Plan->TargetID);
  if(const auto* Charge=CoverApproaches.Find(ID);Charge && Charge->bCharge)Preferred.Insert(Charge->Target,0);
  if(OwnTargetMode(Unit->Team))
  {
   const auto* OwnContacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID);
   int32 OwnID=INDEX_NONE;double OwnDistance=TNumericLimits<double>::Max();
   if(OwnContacts)for(const auto& OwnPair:*OwnContacts)
   {
    const auto* Observed=Team.Targets.Find(OwnPair.Key);if(!Observed)continue;
    const auto* OwnSeen=&OwnPair.Value;
    if(!OwnSeen->bVisible || OwnSeen->bShared || BattleSquadType::IsVehicle(Observed->ObservedSquadType) ||
     !KnownFireAllowed(ID,*Observed,Now))continue;
    const double OwnCandidateDistance=FVector::DistSquared(From,Observed->At(Now));
    if(OwnCandidateDistance<OwnDistance || (OwnCandidateDistance==OwnDistance && OwnPair.Key<OwnID))
    {OwnID=OwnPair.Key;OwnDistance=OwnCandidateDistance;}
   }
   Preferred.AddUnique(OwnID);
  }
  for(int32 PreferredID:Preferred)if(const auto* PreferredTarget=Team.Targets.Find(PreferredID);PreferredTarget && KnownFireAllowed(ID,*PreferredTarget,Now))Out.AddUnique(PreferredID);
 }
 for(const auto& Choice:Best)if(Out.Num()<3)Out.AddUnique(Choice.Candidate);
}

bool UBattleLeaderRuntime::KnownFireRay(int32 ID,double Now)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);if(!Unit || Unit->Team<0 || Unit->Team>1)return false;
 auto& Budget=KnownFireBudgets[Unit->Team];const int64 Window=FMath::FloorToInt64(Now/.1);
 if(Window!=Budget.Window){Budget.Window=Window;Budget.Used=0;}
 // FIFO carry-over beats new requests. Dead/inactive requesters and those no longer asking expire instead of blocking the team.
 for(int32 At=Budget.Waiting.Num()-1;At>=0;--At)
 {
  const int32 WaitingID=Budget.Waiting[At];const auto* WaitingUnit=Store->Find(WaitingID);
  if(!WaitingUnit || !WaitingUnit->IsAlive() || Now-Budget.RequestedAt.FindRef(WaitingID)>.5)
  {Budget.Waiting.RemoveAt(At);Budget.RequestedAt.Remove(WaitingID);}
 }
 if(Budget.Used>=(BattleKnownFire::MoveEnabled()?BattleKnownFire::MoveRayBudget():32) || (Budget.Waiting.Num() && Budget.Waiting[0]!=ID))
 {
  Budget.Waiting.AddUnique(ID);Budget.RequestedAt.Add(ID,Now);SightExperiment.Cover(TEXT("known_fire_budget_deferred"));return false;
 }
 ++Budget.Used;SightExperiment.Cover(TEXT("known_fire_rays"));return true;
}

BattleKnownFire::FResult UBattleLeaderRuntime::SelectKnownFire(int32 ID,const FVector& Origin,double Now)
{
 using namespace BattleKnownFire;
 FResult Result;if(!Enabled())return Result;
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!Unit || Unit->Team<0 || Unit->Team>1 ||
  (Unit->ActiveWeapon()!=EBattleWeapon::Carbine && Unit->ActiveWeapon()!=EBattleWeapon::SupportGun))return Result;
 auto& Search=KnownFireSearches.FindOrAdd(ID);
 if(Unit->Ammo<=0 || Unit->ReloadEndTime>0 || Unit->NextShotTime>Now)
 {
  if(Unit->Ammo<=0 && Unit->ReloadEndTime==0)GetWorld()->GetSubsystem<UBattleState>()->StartReload(ID,Now);
  // Keep the observed aim between shots, without reusing a fire permit or tracing during cooldown.
  const auto& WaitingTeam=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];
  if(const auto* WaitingTarget=WaitingTeam.Targets.Find(Search.Selected);WaitingTarget && KnownFireAllowed(ID,*WaitingTarget,Now))
  {Result.TargetID=WaitingTarget->TargetID;Result.AimPosition=Search.ConfirmedTarget==WaitingTarget->TargetID?Search.ConfirmedAim:WaitingTarget->At(Now)+FVector(0,0,35);Result.bTracked=WaitingTarget->Tracked(Now);}
  return Result;
 }
 // Retain the candidate cursor while queued, even across the search interval.
 if(Now>=Search.NextAt && !KnownFireBudgets[Unit->Team].Waiting.Contains(ID)){KnownFireCandidates(ID,Now,Search.Candidates);Search.NextAt=Now+.4;Search.Cursor=0;Search.Selected=INDEX_NONE;Search.bPending=true;}
 if(!Search.bPending && Search.Selected==INDEX_NONE)return Result;
 const auto& Team=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];
 auto& Budget=KnownFireBudgets[Unit->Team];
 const bool Recheck=!Search.bPending;
 for(;Recheck?Search.Selected!=INDEX_NONE:Search.Cursor<Search.Candidates.Num();++Search.Cursor)
 {
  const int32 TargetID=Recheck?Search.Selected:Search.Candidates[Search.Cursor];
  // A selected target is rechecked once before a shot. Failed selections wait until the next .4 s search.
  if(Recheck)Search.Selected=INDEX_NONE;
  const auto* Target=Team.Targets.Find(TargetID);
  if(!Target){Result.Reject=EReject::Stale;SightExperiment.Cover(TEXT("known_fire_reject_stale"));continue;}
  if(!KnownFireAllowed(ID,*Target,Now))continue;
  const auto Context=Sight(ID);FVector End;
  SightExperiment.Cover(TEXT("known_fire_sensor_checks"));
  if(!PrepareSightTrace(Context,TargetID,End))
  {
   // Failure classification stays inside the personal sensor. It never changes the observation or candidate order.
   const auto* Live=GetWorld()->GetSubsystem<UBattleState>()->Find(TargetID);
   if(!Context.bAlive || !Live || !Live->IsAlive() || !Bodies.Contains(TargetID))
   {Result.Reject=EReject::Invalid;SightExperiment.Cover(TEXT("known_fire_reject_invalid"));}
   else if(FVector::DistSquared(Position(TargetID),Context.Position)>(IsVehicle(TargetID)?FMath::Max(Context.RangeSquared,FMath::Square(VehicleSpotRange())):Context.RangeSquared))
   {Result.Reject=EReject::Range;SightExperiment.Cover(TEXT("known_fire_reject_range"));}
   else{Result.Reject=EReject::FOV;SightExperiment.Cover(TEXT("known_fire_reject_fov"));}
   continue;
  }
  if(!KnownFireRay(ID,Now)){if(Recheck)Search.Selected=TargetID;Result.Reject=EReject::Deferred;return Result;}
  SightExperiment.Cover(TEXT("known_fire_sensor_rays"));
  if(!SightHitVisible(TraceSight(Context,End,TargetID,SightFromCanSee),TargetID))
  {Result.Reject=EReject::SensorWall;SightExperiment.Cover(TEXT("known_fire_reject_sensor_wall"));continue;}
  // Successful personal sensing permits a current observation; failed sensors never refresh this sample.
  // Keep it local to this firing path: do not rewrite shared contacts or their original expiry.
  const FVector Aim=End; // Confirmed current sensor point includes stance and exposed body parts.
  Search.ConfirmedTarget=TargetID;Search.ConfirmedAim=Aim;
  if(!KnownFireRay(ID,Now)){if(Recheck)Search.Selected=TargetID;Result.Reject=EReject::Deferred;return Result;}
  auto AimContext=Context;AimContext.Eye=Origin;SightExperiment.Cover(TEXT("known_fire_aim_rays"));
  if(!SightHitVisible(TraceSight(AimContext,Aim,TargetID,SightFromCanSee),TargetID))
  {Result.Reject=EReject::AimWall;SightExperiment.Cover(TEXT("known_fire_reject_aim_wall"));continue;}
  Result.TargetID=TargetID;Result.SourceUnitID=Target->SourceUnitID;Result.AimPosition=Aim;Result.CheckedAt=Now;
  Result.Revision=Target->Revision;Result.bTracked=Target->Tracked(Now);Result.Reject=EReject::None;Search.Selected=TargetID;break;
 }
 Search.bPending=false;Budget.Waiting.Remove(ID);Budget.RequestedAt.Remove(ID);return Result;
}

bool UBattleLeaderRuntime::FireKnownTarget(int32 ID,const FVector& Origin,const BattleKnownFire::FResult& Result,bool Peek)
{
 if(!Result.Ready() || !Fire(ID,Origin,(Result.AimPosition-Origin).GetSafeNormal(),false,true))return false;
 SightExperiment.Cover(Peek?TEXT("known_fire_shots_peek"):TEXT("known_fire_shots_open"));
 if(Result.bTracked)SightExperiment.Cover(TEXT("known_fire_tracked_shots"));
 if(BattleKnownFire::MoveEnabled())KnownMoveShot(ID,Result.TargetID,Result.CheckedAt);
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Unit=Store->Find(ID);const auto* Squad=Unit?Store->FindSquad(Unit->SquadID):nullptr;
 if(Squad && Squad->TacticalPlan.Action==EBattleSquadTactic::Support)GetWorld()->GetSubsystem<UBattleSquadTactics>()->RecordSupportShot(Squad->SquadID,Result.TargetID);
 return true;
}

bool UBattleLeaderRuntime::KnownCoverContact(int32 ID,double Now,FBattleContact& Out)
{
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!Unit || Unit->Team<0 || Unit->Team>1)return false;
 const auto& Team=GetWorld()->GetSubsystem<UBattlePerception>()->TeamKnownTargets[Unit->Team];
 const auto* Target=Team.Targets.Find(Unit->CoverTargetID);
 // Contact resolution is also limited to one candidate search every .4 seconds.
 auto& Search=KnownFireSearches.FindOrAdd(ID);
 if(!Target || !KnownFireAllowed(ID,*Target,Now))
 {
  if(Now>=Search.NextAt && !KnownFireBudgets[Unit->Team].Waiting.Contains(ID)){KnownFireCandidates(ID,Now,Search.Candidates);Search.NextAt=Now+.4;Search.Cursor=0;Search.Selected=INDEX_NONE;Search.bPending=true;}
  Target=nullptr;
  for(int32 CandidateID:Search.Candidates)if(const auto* Candidate=Team.Targets.Find(CandidateID);Candidate && KnownFireAllowed(ID,*Candidate,Now)){Target=Candidate;break;}
 }
 if(!Target)return false;
 Out.TargetID=Target->TargetID;Out.LastKnownPosition=Target->At(Now);Out.LastSeenTime=Target->ObservedAt;
 Out.SourceUnitID=Target->SourceUnitID;Out.bShared=true;Out.bVisible=false;return true;
}
