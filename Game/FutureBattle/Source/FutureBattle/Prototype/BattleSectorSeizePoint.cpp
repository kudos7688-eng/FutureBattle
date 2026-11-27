#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystem.h"

int32 BattleTactics::SectorSeizePoint()
{
 static const int32 Mode=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePoint="),Value);return FMath::Clamp(Value,0,1);}();
 return Mode;
}
int32 BattleTactics::SectorSeizePointAT()
{
 static const int32 Limit=[](){int32 Value=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePointAT="),Value);return FMath::Clamp(Value,0,2);}();
 return Limit;
}
int32 BattleTactics::SectorSeizePointTank()
{
 static const int32 Limit=[](){int32 Value=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePointTank="),Value);return FMath::Clamp(Value,0,2);}();
 return Limit;
}
int32 BattleTactics::SectorSeizePointMax()
{
 static const int32 Limit=[](){int32 Value=6;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePointMax="),Value);return FMath::Clamp(Value,2,12);}();
 return Limit;
}
int32 BattleTactics::SectorSeizePointSupport()
{
 static const int32 Limit=[](){int32 Value=2;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePointSupport="),Value);return FMath::Clamp(Value,0,4);}();
 return Limit;
}
int32 BattleTactics::SectorSeizePointMin()
{
 static const int32 Limit=[](){int32 Value=4;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePointMin="),Value);return FMath::Clamp(Value,1,8);}();
 return Limit;
}
float BattleTactics::SectorSeizePointWait()
{
 static const float Seconds=[](){float Value=45.f;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePointWait="),Value);return FMath::IsFinite(Value)?FMath::Clamp(Value,0.f,120.f):45.f;}();
 return Seconds;
}
int32 BattleTactics::SectorSeizeCounter()
{
 static const int32 Mode=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizeCounter="),Value);return FMath::Clamp(Value,0,1);}();
 return Mode;
}
int32 BattleTactics::SectorSeizeCounterMax()
{
 static const int32 Limit=[](){int32 Value=5;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizeCounterMax="),Value);return FMath::Clamp(Value,1,8);}();
 return Limit;
}

void UBattleSquadTactics::ResetSeizeExtensions()
{
 if(BattleTactics::SectorSeize()<=0 || (!BattleTactics::SectorSeizePoint() && !BattleTactics::SectorSeizeCounter()))return;
 SeizeExtras.Reset();SeizeTrialSlots.Reset();
 if(BattleTactics::SectorSeizePoint() && (BattleTactics::SectorSeizePointAT()>0 || BattleTactics::SectorSeizePointTank()>0))SeizeArmourStayAt.Reset();SeizeTrialSlotsAt=-100.;
 if(BattleTactics::SectorSeizePoint()){SeizePointNoAt.Reset();SeizePointNoneAt[0]=SeizePointNoneAt[1]=-100.;}
 for(int32 Team=0;Team<2;++Team)
 {
  SeizePoints[Team]=FSeizePointPlan();SeizeGuards[Team].Reset();
  SeizeTrialOwn[Team].Reset();SeizeTrialKnown[Team].Reset();SeizeTrialAt[Team]=-100.;
 }
}

void UBattleSquadTactics::LogSeizePoint(int32 Team,const TCHAR* What,const FIntPoint* LogCell) const
{
 if(!BattleTactics::SectorSeizePoint() || !SectorSeizeEnabled(Team))return;
 const auto& Focus=SeizePoints[Team];
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();int32 AssignedCount=0;
 for(const auto& Pair:SeizeExtras)
 {
  const auto* Squad=Store->FindSquad(Pair.Key);const auto* Open=SeizeSectors.Find(Pair.Key);
  if(Pair.Value.Point==Focus.Point && Pair.Value.FocusStart==Focus.Start &&
     Squad && Squad->Team==Team && Open && Open->EndAt<0. &&
     Squad->CommandStatus==EBattleCommandStatus::Active &&
     (Squad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Pair.Key)))++AssignedCount;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE_POINT t=%.0f team=%d point=%d cell=%d,%d what=%s squads=%d at=%d tank=%d%s"),
  GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,Team,Focus.Point,LogCell?LogCell->X:Focus.Cell.X,LogCell?LogCell->Y:Focus.Cell.Y,What,
  AssignedCount,SeizeEscortCount(Team,BattleSquadType::AntiTank),SeizeEscortCount(Team,BattleSquadType::Tank),FCString::Strcmp(What,TEXT("taken"))==0?*FString::Printf(TEXT(" clear_secs=%.0f"),Focus.ClearAt>=0.?Clock()-Focus.ClearAt:0.):TEXT(""));
}

void UBattleSquadTactics::UpdateSeizeExtensions(int32 Team)
{
 if((!BattleTactics::SectorSeizePoint() && !BattleTactics::SectorSeizeCounter()) || !SectorSeizeEnabled(Team))return;
 const double Now=Clock();
 if(Now>=SeizeTrialAt[Team] && Now-SeizeTrialAt[Team]<1.)return;
 SeizeTrialAt[Team]=Now;
 ON_SCOPE_EXIT { if(BattleTactics::SectorSeizePoint() && BattleTactics::SectorSeizePointTank()>0)UpdateSeizeTanks(Team); };
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 SeizeTrialOwn[Team].Reset();SeizeTrialKnown[Team].Reset();SeizeGuards[Team].Reset();
 TArray<int32> RifleIDs;
 // No enemy actors, positions, unit counts or capture progress are read here.
 for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==Team)
 {
  bool Living=false;
  for(int32 ID:Pair.Value.Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive())
  {
   Living=true;const FIntPoint OwnCell=BattleSectors::Cell(Runtime->Position(ID));
   if(BattleSectors::Valid(OwnCell))SeizeTrialOwn[Team].Add(OwnCell);
  }
  // Keep soldiers actually holding here, not squads already on a temporary trip away.
  if(Living && (Pair.Value.SquadType==BattleSquadType::Rifle ||
     (BattleTactics::SectorSeizePoint() && BattleTactics::SectorSeizePointAT()>0 && Pair.Value.SquadType==BattleSquadType::AntiTank)) && Pair.Value.CommandStatus==EBattleCommandStatus::Active &&
     Pair.Value.TacticalPlan.Action==EBattleSquadTactic::Mission && Pair.Value.Order==EBattleOrder::Defend)RifleIDs.Add(Pair.Key);
 }
 RifleIDs.Sort();
 for(const auto& Spot:KnownSpots[Team])
  if(Now>=Spot.Value.Seen && Now-Spot.Value.Seen<=BattleTactics::SeekKnownSeconds())
   SeizeTrialKnown[Team].Add(BattleSectors::Cell(Spot.Value.At));
 TSet<FIntPoint> OwnedCells;OwnedCells.Add(BattleSectors::Cell(Match->AreaHome(Team)));
 for(int32 PointIndex=0;PointIndex<Match->Objectives.Num();++PointIndex)if(Match->ObservedOwner(Team,PointIndex)==Team)OwnedCells.Add(BattleSectors::Cell(Match->Objectives[PointIndex].Position));
 for(const FIntPoint& OwnedCell:OwnedCells)
 {
  int32 Kept=0;
  for(int32 RifleID:RifleIDs)
  {
   const auto* Rifle=Store->FindSquad(RifleID);
   const FIntPoint RifleCell=BattleSectors::Cell(Runtime->Position(Rifle->LeaderUnitID));
   if(FMath::Abs(RifleCell.X-OwnedCell.X)>1 || FMath::Abs(RifleCell.Y-OwnedCell.Y)>1)continue;
   SeizeGuards[Team].FindOrAdd(RifleID).Add(OwnedCell);
   if(++Kept>=2)break;
  }
 }
 // Expiry closes the seize reservation even while a support/flank plan owns the squad.
 for(const auto& Pair:SeizeExtras)
 {
  const auto* Squad=Store->FindSquad(Pair.Key);const auto* Open=SeizeSectors.Find(Pair.Key);
  if(!Squad || Squad->Team!=Team || !Open || Open->EndAt>=0. || !SeizeSuspended.Contains(Pair.Key))continue;
  if(BattleTactics::SectorSeizePoint() && Pair.Value.Point!=INDEX_NONE &&
     SeizeExtendedEnd(Pair.Key,Now))
  {EndSeize(Pair.Key,TEXT("SeizeEndPointTimeout"));SeizeSuspended.Remove(Pair.Key);}
  else if(Pair.Value.Point==INDEX_NONE && BattleTactics::SectorSeizeCounter() && Pair.Value.Counter && Open->TakenAt<0. && Now-Open->Start>=300.)
  {EndSeize(Pair.Key,TEXT("SeizeEndTimeout"));SeizeSuspended.Remove(Pair.Key);}
 }
 if(!BattleTactics::SectorSeizePoint())return;
 auto& Focus=SeizePoints[Team];
 AdoptSeizePoint(Team);
 TArray<int32> Assigned;
 for(const auto& Pair:SeizeExtras)
 {
  const auto* Squad=Store->FindSquad(Pair.Key);const auto* Open=SeizeSectors.Find(Pair.Key);
  if(Squad)
  {
   bool Living=false;for(int32 ID:Squad->Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive()){Living=true;break;}
   if(!Living)continue;
  }
  if(Focus.Point!=INDEX_NONE && Pair.Value.Point==Focus.Point &&
     Pair.Value.FocusStart==Focus.Start && Squad && Squad->Team==Team &&
     Squad->CommandStatus==EBattleCommandStatus::Active && Open && Open->EndAt<0. &&
     (Squad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Pair.Key)))Assigned.Add(Pair.Key);
 }
 Assigned.Sort();
 if(Focus.Point!=INDEX_NONE)
 {
  if(!Focus.Taken && !Focus.Broken && Match->Objectives.IsValidIndex(Focus.Point) && Match->ObservedOwner(Team,Focus.Point)==Team)
  {
   Focus.Taken=true;Event(Assigned.IsEmpty()?TEXT("SeizePointOwn"):TEXT("SeizePointTaken"));
   LogSeizePoint(Team,Assigned.IsEmpty()?TEXT("own"):TEXT("taken"));
   for(int32 AssignedID:Assigned)SeizeExtras.FindChecked(AssignedID).CapturedAt=Now;
   return; // Re-select on the next one-second update, even while captured squads remain.
  }
  int32 Assaults=Assigned.Num();
  if(BattleTactics::SectorSeizePointAT()>0 || BattleTactics::SectorSeizePointTank()>0)
   for(int32 EscortID:Assigned)if(SeizeEscort(EscortID))--Assaults;
  if(BattleTactics::SectorSeizePointSupport()>0)
  {
   Assaults=0;
   for(int32 AssignedID:Assigned)if(!SeizeExtras.FindChecked(AssignedID).Support && !SeizeEscort(AssignedID))++Assaults;
  }
  if(!Focus.Taken && !Focus.Broken && Focus.ClearAt>=0. && Assaults<=1 && SeizeSectorKnown(Team,Focus.Cell,Now))
  {
   Focus.Broken=true;Event(TEXT("SeizePointBroken"));LogSeizePoint(Team,TEXT("broken"));
   for(int32 AssignedID:Assigned)
   {
    const auto* BrokenSquad=Store->FindSquad(AssignedID);
    if(SeizeEscort(AssignedID)){StepSeizeEscort(AssignedID);continue;}
    EndSeize(AssignedID,TEXT("SeizeEndPointBroken"));SeizeSuspended.Remove(AssignedID);
    if(BrokenSquad->TacticalPlan.Action==EBattleSquadTactic::Retreat ||
       BrokenSquad->TacticalPlan.Action==EBattleSquadTactic::Regroup || BrokenSquad->TacticalPlan.Action==EBattleSquadTactic::Recover)continue;
    if(!ResumeMission(AssignedID,TEXT("POINT BROKEN / RESUME MISSION"),nullptr))
     Apply(AssignedID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Runtime->Position(BrokenSquad->LeaderUnitID),TEXT("POINT BROKEN / HOLD"));
   }
   return;
  }
  if(Assaults>0 && !Focus.Taken && !Focus.Broken)
  {
   if(Focus.GoAt<0.)
   {
    int32 Assembled=0;
    for(int32 AssignedID:Assigned)
    {
     const auto* Squad=Store->FindSquad(AssignedID);
     if(SeizeEscort(AssignedID) || (BattleTactics::SectorSeizePointSupport()>0 && SeizeExtras.FindChecked(AssignedID).Support))continue;
     if(BattleSectors::Cell(Runtime->Position(Squad->LeaderUnitID))==Focus.Assembly)++Assembled;
    }
    if(Assembled>0 && Focus.FirstAt<0.)Focus.FirstAt=Now;
    if(Assembled>=BattleTactics::SectorSeizePointMin() ||
       (Focus.FirstAt>=0. && Now-Focus.FirstAt>=BattleTactics::SectorSeizePointWait()))
    {
     Focus.GoAt=Now;Event(TEXT("SeizePointGo"));LogSeizePoint(Team,TEXT("go"));
     for(int32 AssignedID:Assigned)
     {
      const auto* Squad=Store->FindSquad(AssignedID);
      // Fire and suspended plans retain priority; their normal resume uses the new assault goal.
      if(!SeizeEscort(AssignedID) && Squad->TacticalPlan.Reason==TEXT("SEIZE SECTOR / ASSEMBLE"))
       Apply(AssignedID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,BattleSectors::Center(Focus.Cell),TEXT("SEIZE SECTOR / ASSAULT POINT"));
     }
    }
   }
   return; // Keep this point even if another point has become nearer.
  }
  if(!Focus.Taken && Focus.Committed && !Focus.Broken)LogSeizePoint(Team,TEXT("drop"));
  if(BattleTactics::SectorSeizePointSupport()>0)
   for(int32 AssignedID:Assigned)if(auto& Detail=SeizeExtras.FindChecked(AssignedID);Detail.Support && Detail.ReleasedAt<0.)Detail.ReleasedAt=Now;
 }
 // Observed enemies first, then non-owned points on the enemy half or near known spots.
 const FVector Home=Match->AreaHome(Team);const int32 Turn=Home.X>0?-1:1;
 const FIntPoint EnemyHome=BattleSectors::Cell(Match->AreaHome(1-Team));
 int32 BestPoint=INDEX_NONE;double BestDistance=DBL_MAX;FIntPoint BestCell=FIntPoint::ZeroValue,BestRead=FIntPoint::ZeroValue;
 bool BestEnemy=false;
 for(int32 PointIndex=0;PointIndex<Match->Objectives.Num();++PointIndex)
 {
  const auto& Objective=Match->Objectives[PointIndex];const FIntPoint Candidate=BattleSectors::Cell(Objective.Position);
  const int32 Observed=Match->ObservedOwner(Team,PointIndex);const bool Enemy=Observed==1-Team;
  if(Observed==Team || Candidate==EnemyHome || !BattleSectors::Valid(Candidate) ||
     BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Candidate)))continue;
  bool NearKnown=false;
  for(int32 DX=-1;DX<=1 && !NearKnown;++DX)for(int32 DY=-1;DY<=1;++DY)
   if(SeizeTrialKnown[Team].Contains(Candidate+FIntPoint(DX,DY))){NearKnown=true;break;}
  const FVector CandidateCenter=BattleSectors::Center(Candidate);
  if(!Enemy && !NearKnown && FVector::DistSquared2D(CandidateCenter,Match->AreaHome(1-Team))>=FVector::DistSquared2D(CandidateCenter,Home))continue;
  double Distance=DBL_MAX;
  for(const FIntPoint& OwnCell:SeizeTrialOwn[Team])Distance=FMath::Min(Distance,FVector::DistSquared2D(BattleSectors::Center(OwnCell),BattleSectors::Center(Candidate)));
  if(Distance==DBL_MAX)continue;
  const FIntPoint ReadCell(Candidate.X*Turn,Candidate.Y*Turn);
  if(BestPoint!=INDEX_NONE && ((!Enemy && BestEnemy) || (Enemy==BestEnemy && (Distance>BestDistance || (Distance==BestDistance &&
     (ReadCell.X>BestRead.X || (ReadCell.X==BestRead.X && ReadCell.Y>=BestRead.Y)))))))continue;
  BestPoint=PointIndex;BestDistance=Distance;BestCell=Candidate;BestRead=ReadCell;BestEnemy=Enemy;
 }
 if(BestPoint==INDEX_NONE && (Now<SeizePointNoneAt[Team] || Now-SeizePointNoneAt[Team]>=10.))
 {
  SeizePointNoneAt[Team]=Now;FString Owners;
  for(int32 OwnerIndex=0;OwnerIndex<Match->Objectives.Num();++OwnerIndex)
   Owners+=FString::Printf(TEXT("%s%d"),OwnerIndex?TEXT(","):TEXT(""),Match->ObservedOwner(Team,OwnerIndex));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE_POINT_NONE t=%.0f team=%d owners=%s"),Match->Elapsed,Team,*Owners);
 }
 // An uncommitted selection stays stable without repeating pick events every second.
 if(Focus.Point==BestPoint && !Focus.Taken && !Focus.Broken && !Focus.Committed)return;
 Focus=FSeizePointPlan();
 if(BestPoint==INDEX_NONE)return;
 Focus.Point=BestPoint;Focus.Cell=BestCell;Focus.Start=Now;
 if(Now<SeizeTrialSlotsAt || Now-SeizeTrialSlotsAt>=1.)
 {
  SeizeTrialSlotsAt=Now;SeizeTrialSlots.Reset();
  for(const FVector& Slot:GetWorld()->GetSubsystem<UBattleCover>()->GetSlots())++SeizeTrialSlots.FindOrAdd(BattleSectors::Cell(Slot));
 }
 int32 MostSlots=-1;FIntPoint AssemblyRead=FIntPoint::ZeroValue;
 for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
 {
  if(!DX && !DY)continue;
  const FIntPoint Candidate=BestCell+FIntPoint(DX,DY);
  if(!BattleSectors::Valid(Candidate) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Candidate)) ||
     SeizeTrialKnown[Team].Contains(Candidate) || FVector::DistSquared2D(BattleSectors::Center(Candidate),Home)>=FVector::DistSquared2D(BattleSectors::Center(BestCell),Home))continue;
  const int32 Slots=SeizeTrialSlots.FindRef(Candidate);const FIntPoint ReadCell(Candidate.X*Turn,Candidate.Y*Turn);
  if(Slots<MostSlots || (Slots==MostSlots && (ReadCell.X>AssemblyRead.X || (ReadCell.X==AssemblyRead.X && ReadCell.Y>=AssemblyRead.Y))))continue;
  MostSlots=Slots;AssemblyRead=ReadCell;Focus.Assembly=Candidate;
 }
 Event(TEXT("SeizePointPick"));LogSeizePoint(Team,TEXT("pick"));
 AdoptSeizePoint(Team);
}

void UBattleSquadTactics::SeizePointNo(int32 SID,int32 Team,const TCHAR* Why)
{
 if(!BattleTactics::SectorSeizePoint() || !SectorSeizeEnabled(Team))return;
 const auto& NoFocus=SeizePoints[Team];
 if(NoFocus.Point==INDEX_NONE || NoFocus.Taken || NoFocus.Broken)return;
 const double NoNow=Clock();const double* PreviousNo=SeizePointNoAt.Find(SID);
 if(PreviousNo && NoNow>=*PreviousNo && NoNow-*PreviousNo<10.)return;
 SeizePointNoAt.Add(SID,NoNow);Event(Why);
}

void UBattleSquadTactics::AdoptSeizePoint(int32 Team)
{
 if(!BattleTactics::SectorSeizePoint() || !SectorSeizeEnabled(Team))return;
 auto& AdoptFocus=SeizePoints[Team];
 const auto* AdoptMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 if(AdoptFocus.Point==INDEX_NONE || AdoptFocus.Taken || AdoptFocus.Broken ||
    AdoptMatch->ObservedOwner(Team,AdoptFocus.Point)==Team)return;
 const auto* AdoptStore=GetWorld()->GetSubsystem<UBattleState>();
 const auto* AdoptRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const double AdoptNow=Clock();TArray<int32> AdoptIDs;int32 AdoptCount=0;
 for(const auto& Reservation:SeizeSectors)
 {
  const auto* AdoptSquad=AdoptStore->FindSquad(Reservation.Key);
  if(!AdoptSquad || AdoptSquad->Team!=Team || AdoptSquad->CommandStatus!=EBattleCommandStatus::Active || Reservation.Value.EndAt>=0.)continue;
  bool AdoptLiving=false;
  for(int32 MemberID:AdoptSquad->Members)if(const auto* Member=AdoptStore->Find(MemberID);Member && Member->IsAlive()){AdoptLiving=true;break;}
  if(!AdoptLiving)continue;
  const auto* AdoptExtra=SeizeExtras.Find(Reservation.Key);
  if(AdoptExtra && AdoptExtra->Point==AdoptFocus.Point && AdoptExtra->FocusStart==AdoptFocus.Start &&
     (AdoptSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Reservation.Key)) && !SeizeEscort(Reservation.Key))++AdoptCount;
  if((!AdoptExtra || (AdoptExtra->Point==INDEX_NONE && !AdoptExtra->Counter)) && Reservation.Value.Cell==AdoptFocus.Cell &&
     Reservation.Value.TakenAt<0. && AdoptSquad->SquadType==BattleSquadType::Rifle &&
     AdoptSquad->TacticalPlan.Action==EBattleSquadTactic::Engage && AdoptSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) &&
     !SeizeSuspended.Contains(Reservation.Key))AdoptIDs.Add(Reservation.Key);
 }
 AdoptIDs.Sort();
 for(int32 AdoptID:AdoptIDs)
 {
  if(!SeizeGuardAllows(AdoptID,Team,AdoptFocus.Cell)){SeizePointNo(AdoptID,Team,TEXT("SeizePointNo_guard"));continue;}
  const auto* AdoptSquad=AdoptStore->FindSquad(AdoptID);
  const auto* AdoptSample=Find(AdoptID);const auto* AdoptTune=GetDefault<UBattleTuning>();
  if(AdoptSample && (AdoptSample->Armed<FMath::CeilToInt(AdoptSample->Living*.6f) ||
     AdoptSample->Living<=AdoptSquad->Members.Num()*AdoptTune->SquadRetreatLivingRatio ||
     AdoptSample->Health/FMath::Max(1.f,AdoptSample->FullHealth(AdoptTune->Health))<AdoptTune->SquadRetreatHealthRatio))
  {SeizePointNo(AdoptID,Team,TEXT("SeizePointNo_unhealthy"));continue;}
  const FVector AdoptHere=AdoptRuntime->Position(AdoptSquad->LeaderUnitID);
  if(FVector::Dist2D(AdoptHere,BattleSectors::Center(AdoptFocus.Cell))>BattleTactics::SeekKnownRange())
  {SeizePointNo(AdoptID,Team,TEXT("SeizePointNo_range"));continue;}
  if(AdoptCount>=BattleTactics::SectorSeizePointMax()){SeizePointNo(AdoptID,Team,TEXT("SeizePointNo_full"));continue;}
  const auto* AdoptEnded=SeizeEnded.Find(AdoptID);const double* AdoptEndAt=AdoptEnded?AdoptEnded->Find(AdoptFocus.Cell):nullptr;
  if(AdoptEndAt && AdoptNow>=*AdoptEndAt && AdoptNow-*AdoptEndAt<30.)
  {SeizePointNo(AdoptID,Team,TEXT("SeizePointNo_ended30s"));continue;}
  if(!StartSeizeExtended(AdoptID,AdoptFocus.Cell,TEXT("SEIZE SECTOR / ASSEMBLE"),true))continue;
  ++AdoptCount;Event(TEXT("SeizePointAdopt"));
 }
}

bool UBattleSquadTactics::SeizeCounterCell(int32 Team,FIntPoint Cell,int32* Count) const
{
 if(Count)*Count=0;
 if(!BattleTactics::SectorSeizeCounter() || !SectorSeizeEnabled(Team) || !SeizeSectorKnown(Team,Cell,Clock(),Count))return false;
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const FVector Center=BattleSectors::Center(Cell);
 if(FVector::DistSquared2D(Center,Match->AreaHome(Team))<FVector::DistSquared2D(Center,Match->AreaHome(1-Team)))return true;
 const auto Near=[&](FVector At){const FIntPoint PointCell=BattleSectors::Cell(At);return FMath::Abs(Cell.X-PointCell.X)<=1 && FMath::Abs(Cell.Y-PointCell.Y)<=1;};
 if(Near(Match->AreaHome(Team)))return true;
 for(int32 PointIndex=0;PointIndex<Match->Objectives.Num();++PointIndex)if(Match->ObservedOwner(Team,PointIndex)==Team && Near(Match->Objectives[PointIndex].Position))return true;
 return false;
}

bool UBattleSquadTactics::SeizeGuardAllows(int32 SID,int32 Team,FIntPoint Cell) const
{
 const auto* KeptFor=SeizeGuards[Team].Find(SID);if(!KeptFor)return true;
 // A counterattack at this very point may use its last defenders; other nearby points still retain theirs.
 const bool Counter=SeizeCounterCell(Team,Cell);
 for(const FIntPoint& OwnedCell:*KeptFor)if(!Counter || OwnedCell!=Cell)return false;
 return true;
}

bool UBattleSquadTactics::PickSeizeExtended(int32 SID,const FBattleSquadState& S,const FVector& Here,const FIntPoint* Around,FIntPoint& Out,bool CounterOnly)
{
 UpdateSeizeExtensions(S.Team);
 const double Now=Clock();const auto* Ended=SeizeEnded.Find(SID);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const auto& Focus=SeizePoints[S.Team];
 if(BattleTactics::SectorSeizePoint() && BattleTactics::SectorSeizePointAT()>0 && S.SquadType==BattleSquadType::AntiTank && !CounterOnly)
 {
  if(Around || Focus.Point==INDEX_NONE || Focus.Taken || Focus.Broken || !BattleSectors::Valid(Focus.Assembly) ||
     SeizeGuards[S.Team].Contains(SID) || SeizeEscortCount(S.Team,BattleSquadType::AntiTank)>=BattleTactics::SectorSeizePointAT() ||
     FVector::Dist2D(Here,BattleSectors::Center(Focus.Cell))>BattleTactics::SeekKnownRange())return false;
  const double* ATEnded=Ended?Ended->Find(Focus.Cell):nullptr;
  if(ATEnded && Now>=*ATEnded && Now-*ATEnded<30.)return false;
  Out=Focus.Cell;return true;
 }
 const int32 Turn=Match->AreaHome(S.Team).X>0?-1:1;
 TSet<FIntPoint> Candidates=SeizeTrialKnown[S.Team];
 if(BattleTactics::SectorSeizePoint() && Focus.Point!=INDEX_NONE)Candidates.Add(Focus.Cell);
 double Best=-DBL_MAX;FIntPoint BestRead=FIntPoint::ZeroValue;bool Found=false;
 const TCHAR* PointNoReason=nullptr;bool PointScored=false;
 for(const FIntPoint& Cell:Candidates)
 {
  if(!BattleSectors::Valid(Cell) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Cell)))continue;
  const bool Point=BattleTactics::SectorSeizePoint() && Focus.Point!=INDEX_NONE && !Focus.Broken && !Focus.Taken && Focus.Cell==Cell;
  int32 Count=0;const bool Counter=SeizeCounterCell(S.Team,Cell,&Count);
  if(CounterOnly && !Counter)continue;
  if(S.SquadType!=BattleSquadType::Rifle && !(Counter && SeizeFreeSquad(SID)))continue;
  if(Around){if(!Point && (Cell==*Around || FMath::Abs(Cell.X-Around->X)>1 || FMath::Abs(Cell.Y-Around->Y)>1))continue;}
  else if(Cell==BattleSectors::Cell(Here) && !Point && !Counter)continue;
  if(const double* At=Ended?Ended->Find(Cell):nullptr;At && Now>=*At && Now-*At<30.){if(Point)PointNoReason=TEXT("SeizePointNo_ended30s");continue;}
  if(!Point && !SeizeSectorKnown(S.Team,Cell,Now))continue;
  // Purpose-bound defenders are borrowed only for these two extensions, never for an ordinary sight target.
  if((Match->Area.HirePurpose.Contains(SID) && !Point && !Counter) ||
     (!(Counter && SeizeFreeSquad(SID)) && !SeizeGuardAllows(SID,S.Team,Cell))){if(Point)PointNoReason=TEXT("SeizePointNo_guard");continue;}
  const bool FreeCounter=Counter && SeizeFreeSquad(SID);
  const double Distance=FVector::Dist2D(Here,BattleSectors::Center(Cell));if(!FreeCounter && Distance>BattleTactics::SeekKnownRange()){if(Point)PointNoReason=TEXT("SeizePointNo_range");continue;}
  int32 Limit=BattleTactics::SectorSeizeMax();
  if(Point)Limit=BattleTactics::SectorSeizePointMax();
  if(Counter){const int32 CounterLimit=FMath::Min(BattleTactics::SectorSeizeCounterMax(),FMath::Max(2,Count*2));Limit=Point?FMath::Max(Limit,CounterLimit):CounterLimit;}
  int32 Allies=SeizeSectorAllies(SID,S.Team,Cell);
  if(Point || Counter)
  {
   // Temporary support/flank does not release a point/counter reservation.
   Allies=0;const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
   for(const auto& Pair:SeizeSectors)
   {
    const auto* Other=Store->FindSquad(Pair.Key);
    const auto* Detail=BattleTactics::SectorSeizePointSupport()>0?SeizeExtras.Find(Pair.Key):nullptr;
    const bool SamePoint=Point && Detail && Detail->Point==Focus.Point && Detail->FocusStart==Focus.Start;
    if(Pair.Key!=SID && !SeizeEscort(Pair.Key) && Pair.Value.EndAt<0. && (Pair.Value.Cell==Cell || SamePoint) && Other && Other->Team==S.Team &&
       Other->CommandStatus==EBattleCommandStatus::Active &&
       (Other->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Pair.Key)))++Allies;
   }
  }
  if(Point && Allies>=BattleTactics::SectorSeizePointMax()){PointNoReason=TEXT("SeizePointNo_full");continue;}
  if(Counter)
  {
   int32 FreeAllies=0;const auto* CounterStore=GetWorld()->GetSubsystem<UBattleState>();
   for(const auto& Reservation:SeizeSectors)
   {
    const auto* CounterSquad=CounterStore->FindSquad(Reservation.Key);const auto* CounterExtra=SeizeExtras.Find(Reservation.Key);
    if(Reservation.Key==SID || Reservation.Value.EndAt>=0. || Reservation.Value.Cell!=Cell || !CounterSquad || CounterSquad->Team!=S.Team ||
       CounterSquad->CommandStatus!=EBattleCommandStatus::Active || !CounterExtra || !CounterExtra->FreeCounter || !SeizeFreeSquad(Reservation.Key) ||
       !(CounterSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Reservation.Key)))continue;
    bool CounterLiving=false;for(int32 MemberID:CounterSquad->Members)if(const auto* Member=CounterStore->Find(MemberID);Member && Member->IsAlive()){CounterLiving=true;break;}
    if(CounterLiving)++FreeAllies;
   }
   const int32 CounterLimit=FMath::Min(BattleTactics::SectorSeizeCounterMax(),FMath::Max(2,Count*2));
   if(FreeCounter){if(FreeAllies>=CounterLimit){if(Point)PointNoReason=TEXT("SeizePointNo_full");continue;}} // Guards already en route do not consume the free allocation.
   else if(S.Order==EBattleOrder::Defend && S.TacticalPlan.Action==EBattleSquadTactic::Mission && FreeAllies>=CounterLimit){if(Point)PointNoReason=TEXT("SeizePointNo_full");continue;}
   else if(Allies>=Limit){if(Point)PointNoReason=TEXT("SeizePointNo_full");continue;}
  }
  else if(Allies>=Limit)continue;
  bool Footing=false;
  for(int32 DX=-1;DX<=1 && !Footing;++DX)for(int32 DY=-1;DY<=1;++DY)
   if((DX || DY) && SeizeTrialOwn[S.Team].Contains(Cell+FIntPoint(DX,DY))){Footing=true;break;}
  const double Score=-Distance/5000.+(Allies>0?3.:0.)+(Footing?2.:0.)+(Point?6.:0.)+(Counter?5.:0.)+(FreeCounter?10.:0.);
  if(Point)PointScored=true;
  const FIntPoint ReadCell(Cell.X*Turn,Cell.Y*Turn);
  if(Found && (Score<Best || (Score==Best && (ReadCell.X>BestRead.X || (ReadCell.X==BestRead.X && ReadCell.Y>=BestRead.Y)))))continue;
  Found=true;Best=Score;BestRead=ReadCell;Out=Cell;
 }
 if(PointScored && (!Found || Out!=Focus.Cell))PointNoReason=TEXT("SeizePointNo_score");
 if(PointNoReason)SeizePointNo(SID,S.Team,PointNoReason);
 return Found;
}

bool UBattleSquadTactics::BeginSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible,int32 Threats)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);
 EndSeize(SID);const double Now=Clock();
 if(const auto* Last=SeizeSectors.Find(SID);Last && Last->EndAt>=0. && Now-Last->EndAt<5.)return false;
 const auto* Tune=GetDefault<UBattleTuning>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const bool BorrowAT=BattleTactics::SectorSeizePoint() && BattleTactics::SectorSeizePointAT()>0 && S->SquadType==BattleSquadType::AntiTank;
 if(BorrowAT && (Match->GroupOf(SID)!=INDEX_NONE || Match->ATHunts.Contains(SID) || Match->SupplyHunts.Contains(SID) ||
    Match->FrontPosts.Contains(SID) || Match->PlayerRoutes.Contains(SID) || !O.Contacts.IsEmpty() || S->TacticalPlan.Action!=EBattleSquadTactic::Mission ||
    S->TacticalPlan.Reason.Contains(TEXT("EXPLICIT ORDER"))))return false;
 if(Visible || Threats!=0 || O.RecentDamage!=0 || (S->SquadType!=BattleSquadType::Rifle && !BorrowAT) ||
    O.Armed<FMath::CeilToInt(O.Living*.6f) || O.Living<=S->Members.Num()*Tune->SquadRetreatLivingRatio ||
    O.Health/FMath::Max(1.f,O.FullHealth(Tune->Health))<Tune->SquadRetreatHealthRatio || Match->SurpriseMoving(SID) || Match->RaidSneaking(SID))
 {
  if(S->SquadType==BattleSquadType::Rifle && (O.RecentDamage!=0 || O.Armed<FMath::CeilToInt(O.Living*.6f) ||
     O.Living<=S->Members.Num()*Tune->SquadRetreatLivingRatio ||
     O.Health/FMath::Max(1.f,O.FullHealth(Tune->Health))<Tune->SquadRetreatHealthRatio))
   SeizePointNo(SID,S->Team,TEXT("SeizePointNo_unhealthy"));
  return false;
 }
 const auto* Group=Match->Groups.Find(Match->GroupOf(SID));
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const FVector Here=Runtime->Position(S->LeaderUnitID);
 const bool Guard=(S->bSectorMission || BorrowAT) && S->Order==EBattleOrder::Defend && S->TacticalPlan.Action==EBattleSquadTactic::Mission &&
  Runtime->SectorCellFor(S->Team,Here)==Runtime->SectorCellFor(S->Team,S->Destination) && O.Contacts.IsEmpty();
 if((Group && Group->Kind!=1) || (Match->Area.HirePurpose.Contains(SID) && !Guard) || Match->IntelPurpose.Contains(SID))return false;
 const bool* Base=Match->SectorTrialBaseSlots.Find(SID);if(!Base || *Base)return false;
 FIntPoint Cell;if(!PickSeizeExtended(SID,*S,Here,nullptr,Cell))return false;
 return StartSeizeExtended(SID,Cell,TEXT("SEIZE SECTOR / MOVE TO ENEMY SECTOR"));
}

bool UBattleSquadTactics::SeizePointWaiting(int32 SID) const
{
 const auto* Extra=SeizeExtras.Find(SID);const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 if(!BattleTactics::SectorSeizePoint() || !Extra || Extra->Point==INDEX_NONE || !S)return false;
 if(Extra->ClearAt>=0.)return false;
 if(BattleTactics::SectorSeizePointSupport()>0 && Extra->Support)return false;
 const auto& Focus=SeizePoints[S->Team];
 return Focus.Point==Extra->Point && Extra->FocusStart==Focus.Start && Focus.GoAt<0. && BattleSectors::Valid(Focus.Assembly);
}

FVector UBattleSquadTactics::SeizeExtendedGoal(int32 SID,const TCHAR*& Reason) const
{
 const auto& Open=SeizeSectors.FindChecked(SID);const auto* Extra=SeizeExtras.Find(SID);
 if(BattleTactics::SectorSeizePoint() && Extra && Extra->Point!=INDEX_NONE)
 {
  if(BattleTactics::SectorSeizePointSupport()>0 && Extra->Support)
  {Reason=TEXT("SEIZE SECTOR / SUPPORT SECTOR");return BattleSectors::Center(Open.Cell);}
  if(SeizePointWaiting(SID))
  {
   const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);Reason=TEXT("SEIZE SECTOR / ASSEMBLE");
   return BattleSectors::Center(SeizePoints[S->Team].Assembly);
  }
  if(Extra->Escort){Reason=TEXT("SEIZE SECTOR / ESCORT MOVE");return Extra->EscortGoal;}
  if(Extra->ClearAt>=0.){Reason=TEXT("SEIZE SECTOR / CLEAR POINT");return Extra->ClearGoal.IsNearlyZero()?BattleSectors::Center(Open.Cell):Extra->ClearGoal;}
  Reason=TEXT("SEIZE SECTOR / ASSAULT POINT");
 }
 return BattleSectors::Center(Open.Cell);
}

bool UBattleSquadTactics::StartSeizeExtended(int32 SID,FIntPoint Cell,const TCHAR* SightReason,bool Adopt)
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const double Now=Clock();auto& Focus=SeizePoints[S->Team];
 if(BattleTactics::SectorSeizePoint() && BattleTactics::SectorSeizePointAT()>0 && S->SquadType==BattleSquadType::AntiTank &&
    Focus.Point!=INDEX_NONE && Cell==Focus.Cell && !Focus.Taken && !Focus.Broken)
 {
  const auto* ATBase=Match->SectorTrialBaseSlots.Find(SID);
  if(S->TacticalPlan.Action!=EBattleSquadTactic::Mission || Match->GroupOf(SID)!=INDEX_NONE ||
     Match->ATHunts.Contains(SID) || Match->SupplyHunts.Contains(SID) || Match->PlayerRoutes.Contains(SID) ||
     !ATBase || *ATBase || SeizeGuards[S->Team].Contains(SID) ||
     FVector::Dist2D(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID),BattleSectors::Center(Cell))>BattleTactics::SeekKnownRange() ||
     SeizeEscortCount(S->Team,BattleSquadType::AntiTank)>=BattleTactics::SectorSeizePointAT())return false;
 }
 FSeizeExtra Extra;Extra.Counter=SeizeCounterCell(S->Team,Cell);
 const bool FreeRole=SeizeFreeSquad(SID);
 const bool GuardRole=!FreeRole && S->Order==EBattleOrder::Defend && S->TacticalPlan.Action==EBattleSquadTactic::Mission;
 Extra.FreeCounter=Extra.Counter && FreeRole;Extra.GuardCounter=Extra.Counter && GuardRole;
 FVector Goal=BattleSectors::Center(Cell);const TCHAR* Reason=SightReason;
 if(BattleTactics::SectorSeizePoint() && Focus.Point!=INDEX_NONE && !Focus.Broken && !Focus.Taken && Focus.Cell==Cell)
 {
  Extra.Point=Focus.Point;Extra.FocusStart=Focus.Start;Extra.PointCell=Focus.Cell;Extra.AssaultOrdinal=Focus.AssaultJoined;Extra.SweepIndex=Extra.AssaultOrdinal%5;
  if(Focus.GoAt<0. && BattleSectors::Valid(Focus.Assembly)){Goal=BattleSectors::Center(Focus.Assembly);Reason=TEXT("SEIZE SECTOR / ASSEMBLE");}
  else Reason=TEXT("SEIZE SECTOR / ASSAULT POINT");
 }
 if(Extra.Point!=INDEX_NONE &&
    ((S->SquadType==BattleSquadType::AntiTank && BattleTactics::SectorSeizePointAT()>0) ||
     ((S->SquadType==BattleSquadType::Tank || S->SquadType==BattleSquadType::ArmoredCar) && BattleTactics::SectorSeizePointTank()>0)))
 {
  Extra.Escort=true;Extra.EscortAssembly=Focus.Assembly;Extra.Counter=false;Extra.FreeCounter=false;Extra.GuardCounter=false;
  if(!SeizeEscortGoal(SID,Focus.Cell,Focus.Assembly,Extra.EscortGoal) ||
     !GetWorld()->GetSubsystem<UBattleState>()->MissionOf(SID,Extra.EscortMission))return false;
  if(Focus.GoAt>=0.){Goal=Extra.EscortGoal;Reason=TEXT("SEIZE SECTOR / ESCORT MOVE");}
 }
 if(Extra.Point!=INDEX_NONE && !Extra.Escort && !Extra.FreeCounter && BattleTactics::SectorSeizePointSupport()>0 &&
    Focus.AssaultJoined>=BattleTactics::SectorSeizePointMin() && Focus.SupportJoined<BattleTactics::SectorSeizePointSupport())
 {
  FIntPoint SupportCell=Focus.SupportCell;
  if(!BattleSectors::Valid(SupportCell) && !PickSeizeSupport(S->Team,SupportCell))return false;
  Extra.Support=true;Cell=SupportCell;Extra.Counter=SeizeCounterCell(S->Team,Cell);
  Extra.FreeCounter=false;Extra.GuardCounter=Extra.Counter && S->Order==EBattleOrder::Defend && S->TacticalPlan.Action==EBattleSquadTactic::Mission;
  Goal=BattleSectors::Center(Cell);Reason=TEXT("SEIZE SECTOR / SUPPORT SECTOR");
 }
 if(Adopt && Extra.Point!=INDEX_NONE && !Extra.Support && !Extra.Escort &&
    BattleSectors::Cell(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID))==Focus.Cell)
 {Extra.ClearAt=Now;Goal=BattleSectors::Center(Focus.Cell);Reason=TEXT("SEIZE SECTOR / CLEAR POINT");}
 if(!Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,Goal,Reason))return false;
 EndSeek(SID);
 FSeizeSector Open;Open.Cell=Cell;Open.Start=Now;Open.LastKnown=Now;
 if(Extra.ClearAt>=0.)Open.EnterAt=Now;
 SeizeSectors.Add(SID,Open);SeizeExtras.Add(SID,Extra);
 if(Extra.Point!=INDEX_NONE)
 {
  Focus.Committed=true;
  if(Extra.ClearAt>=0.)
  {
   if(Focus.ClearAt<0.){Focus.ClearAt=Now;LogSeizePoint(S->Team,TEXT("clear"));}
   Event(TEXT("SeizeEnter"));Event(TEXT("SeizePointEnter"));Event(TEXT("SeizePointClear"));
  }
  if(Extra.Support){++Focus.SupportJoined;Focus.SupportCell=Cell;Event(TEXT("SeizePointSupportGo"));LogSeizePoint(S->Team,TEXT("support"),&Cell);}
  if(!Extra.Support && !Extra.Escort)++Focus.AssaultJoined;
  if(Extra.Escort)Event(S->SquadType==BattleSquadType::AntiTank?TEXT("SeizePointATGo"):TEXT("SeizePointTankGo"));
  if(Extra.Support || Extra.Escort){} // Escorts and supports never open the rifle assault gate.
  else if(SeizePointWaiting(SID))Event(TEXT("SeizePointAssemble"));
  else if(Focus.GoAt<0.){Focus.GoAt=Now;Event(TEXT("SeizePointGo"));LogSeizePoint(S->Team,TEXT("go"));}
 }
 if(Extra.Counter){Event(TEXT("SeizeCounterGo"));if(Extra.FreeCounter)Event(TEXT("SeizeCounterFree"));else if(Extra.GuardCounter)Event(TEXT("SeizeCounterGuard"));}
 Event(TEXT("SeizeGo"));int32 Known=0;SeizeSectorKnown(S->Team,Cell,Now,&Known);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE t=%.0f squad=%d team=%d cell=%d,%d dist=%.0f known=%d allies=%d kind=%s%s"),
  Match->Elapsed,SID,S->Team,Cell.X,Cell.Y,FVector::Dist2D(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID),BattleSectors::Center(Cell)),
  Known,SeizeSectorAllies(SID,S->Team,Cell),Extra.Support?TEXT("support"):Extra.Point!=INDEX_NONE?TEXT("point"):Extra.Counter?TEXT("counter"):TEXT("sight"),
  BattleTactics::SectorSeizeCounter()?(FreeRole?TEXT(" role=free"):GuardRole?TEXT(" role=guard"):TEXT(" role=other")):TEXT(""));
 return true;
}

const TCHAR* UBattleSquadTactics::SeizeExtendedEnd(int32 SID,double Now) const
{
 const auto* Open=SeizeSectors.Find(SID);if(!Open || Open->EndAt>=0.)return TEXT("SeizeEndLost");
 const auto* Extra=SeizeExtras.Find(SID);
 if(BattleTactics::SectorSeizePoint() && Extra && Extra->Point!=INDEX_NONE)
 {
  if(BattleTactics::SectorSeizePointSupport()>0 && Extra->Support && Open->TakenAt>=0. &&
     (Extra->ReleasedAt>=0. || Extra->CapturedAt>=0.))
  {
   const double Released=Extra->ReleasedAt>=0.?Extra->ReleasedAt:Extra->CapturedAt;
   if(Now-FMath::Max(Released,Open->TakenAt)>=BattleTactics::SectorSeizeHold()+90.)return TEXT("SeizeEndHeldLong");
  }
  if(Extra->Support)
  {
   const auto* SupportSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
   if(SupportSquad)
   {
    const auto& SupportPlan=SeizePoints[SupportSquad->Team];
    if(SupportPlan.Point==Extra->Point && SupportPlan.Start==Extra->FocusStart && SupportPlan.ClearAt>=0. && !SupportPlan.Taken && !SupportPlan.Broken)return nullptr;
   }
  }
  return (Extra->ClearAt>=0.?Now-Extra->ClearAt>=240.:Now-Open->Start>=360.)?TEXT("SeizeEndPointTimeout"):nullptr;
 }
 const double Timeout=BattleTactics::SectorSeizeCounter() && Extra && Extra->Counter?300.:180.;
 if(Open->TakenAt<0. && Now-Open->Start>=Timeout)return TEXT("SeizeEndTimeout");
 if(Open->TakenAt>=0. && Now-Open->TakenAt>=BattleTactics::SectorSeizeHold()+90.)return TEXT("SeizeEndHeldLong");
 if(Open->EnterAt<0. && Now-Open->LastKnown>=60.)return TEXT("SeizeEndGone");
 return nullptr;
}

bool UBattleSquadTactics::SeizePointMember(int32 SID) const
{
 if(BattleTactics::SectorSeizePoint()<=0)return false;
 const auto* MemberOpen=SeizeSectors.Find(SID);
 const auto* MemberExtra=SeizeExtras.Find(SID);
 const auto* MemberSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 return MemberOpen && MemberOpen->EndAt<0. && MemberExtra && MemberExtra->Point!=INDEX_NONE &&
  MemberSquad && MemberSquad->TacticalPlan.Action==EBattleSquadTactic::Engage &&
  MemberSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR"));
}

bool UBattleSquadTactics::SeizeClearing(int32 SID) const
{
 if(BattleTactics::SectorSeizePoint()<=0)return false;
 const auto* ClearingOpen=SeizeSectors.Find(SID);
 const auto* ClearingExtra=SeizeExtras.Find(SID);
 const auto* ClearingSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 return ClearingOpen && ClearingOpen->EndAt<0. && ClearingExtra && !ClearingExtra->Support &&
  ClearingSquad && ClearingSquad->TacticalPlan.Action==EBattleSquadTactic::Engage &&
  ClearingSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR / CLEAR POINT"));
}

bool UBattleSquadTactics::ResumeSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible,bool Healthy)
{
 if(!Visible)for(const auto& Contact:O.Contacts)if(Contact.Value.bVisible && !Contact.Value.bShared){Visible=true;break;}
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);UpdateSeizeExtensions(S->Team);
 auto* Open=SeizeSectors.Find(SID);const double Now=Clock();
 const auto* Extra=SeizeExtras.Find(SID);
 const bool Point=BattleTactics::SectorSeizePoint() && Extra && Extra->Point!=INDEX_NONE;
 const TCHAR* End=SeizeExtendedEnd(SID,Now);
 if(!Healthy || End || (!Point && Now-Open->LastKnown>=60.))
 {EndSeize(SID,End?End:TEXT("SeizeEndOther"));SeizeSuspended.Remove(SID);return false;}
 const TCHAR* Reason=TEXT("SEIZE SECTOR / MOVE ON");FVector Goal=SeizeExtendedGoal(SID,Reason);
 if(Visible){Goal=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID);Reason=TEXT("SEIZE SECTOR / FIRE");}
 if(!Apply(SID,EBattleSquadTactic::Engage,Visible?EBattleOrder::Hold:EBattleOrder::AttackMove,Goal,Reason))
 {EndSeize(SID);SeizeSuspended.Remove(SID);return false;}
 // A resumed FIRE plan must not inherit time spent supporting or flanking.
 if(Point)
 {
  auto& ResumedClear=SeizeExtras.FindChecked(SID);
  ResumedClear.FireSeconds=-1.;ResumedClear.FireCheckedAt=-1.;ResumedClear.FireWasActive=false;ResumedClear.Closing=false;
 }
 if(Visible)Open->LastSeen=Now;
 Open->QuietSince=-1.;SeizeSuspended.Remove(SID);Event(TEXT("SeizeResume"));return true;
}

bool UBattleSquadTactics::StepSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 if(BattleTactics::SectorSeize()<=0)return false;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* S=Store->Squads.Find(SID);
 if(!S || !SectorSeizeEnabled(S->Team))return false;
 if(!Visible)for(const auto& Contact:O.Contacts)if(Contact.Value.bVisible && !Contact.Value.bShared){Visible=true;break;}
 auto& Plan=S->TacticalPlan;
 if(Plan.Action!=EBattleSquadTactic::Engage || !Plan.Reason.StartsWith(TEXT("SEIZE SECTOR")))return false;
 UpdateSeizeExtensions(S->Team);
 if(Plan.Action!=EBattleSquadTactic::Engage || !Plan.Reason.StartsWith(TEXT("SEIZE SECTOR")))return true;
 const double Now=Clock();
 const FVector Here=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID);
 auto* Seize=SeizeSectors.Find(SID);
 auto* Extra=SeizeExtras.Find(SID);
 if(SeizeEscort(SID))return StepSeizeEscort(SID);
 const bool Point=BattleTactics::SectorSeizePoint()>0 && Extra && Extra->Point!=INDEX_NONE;
 const bool Support=Point && BattleTactics::SectorSeizePointSupport()>0 && Extra->Support;
 if(Point && !Support && Seize && Seize->EndAt<0. && Seize->EnterAt<0. && BattleSectors::Cell(Here)==Seize->Cell)
 {
  Seize->EnterAt=Now;Extra->ClearAt=Now;Event(TEXT("SeizeEnter"));Event(TEXT("SeizePointEnter"));Event(TEXT("SeizePointClear"));
  auto& ClearingFocus=SeizePoints[S->Team];
  if(ClearingFocus.Point==Extra->Point && ClearingFocus.Start==Extra->FocusStart && ClearingFocus.ClearAt<0.)
  {ClearingFocus.ClearAt=Now;LogSeizePoint(S->Team,TEXT("clear"));}
 }
 // Point: 360 s approach, replaced by 240 s after entry; counter: 300 s before taking.
 const bool Known=Seize && SeizeSectorKnown(S->Team,Seize->Cell,Now);
 if(Seize)
 {
  if(Known){Seize->LastKnown=Now;Seize->QuietSince=-1.;}
  if(BattleSectors::Cell(Here)!=Seize->Cell)Seize->QuietSince=-1.;
 }
 const TCHAR* End=SeizeExtendedEnd(SID,Now);
 if(End)
 {
  if(!Seize || Seize->EndAt>=0.)
  {
   FSeizeSector Lost;Lost.Cell=BattleSectors::Cell(Plan.Destination);Lost.Start=Now;
   SeizeSectors.Add(SID,Lost);
  }
  EndSeize(SID,End);
  if(!ResumeMission(SID,TEXT("SEIZE ENDED / RESUME MISSION"),nullptr))
   Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE ENDED / HOLD"));
  return true;
 }
 // Ordinary plans last 60 s. This plan instead owns the bounded lifetime above.
 Plan.Remaining=60.f;
 if(Support && (Extra->ReleasedAt>=0. || Extra->CapturedAt>=0.) && Seize->TakenAt<0.)
 {
  EndSeize(SID,TEXT("SeizeEndOther"));
  if(!ResumeMission(SID,TEXT("POINT SUPPORT OVER / RESUME MISSION"),nullptr))
   Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("POINT SUPPORT OVER / HOLD"));
  return true;
 }
 if(Point && Extra->CapturedAt<0. && GetWorld()->GetSubsystem<UBattleMatch>()->Objectives.IsValidIndex(Extra->Point) &&
    GetWorld()->GetSubsystem<UBattleMatch>()->ObservedOwner(S->Team,Extra->Point)==S->Team)Extra->CapturedAt=Now;
 if(Point && !Support && Extra->CapturedAt>=0. && Seize->TakenAt<0.)
 {
  if(Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE SECTOR / HOLD TAKEN")))
  {Seize->TakenAt=Now;Event(TEXT("SeizeTaken"));LogSeizeSector(SID,*Seize,TEXT("SeizeTaken"));if(Extra->Counter)Event(TEXT("SeizeCounterTaken"));}
  else return true;
 }
 if(Point && !Support && Extra->ClearAt>=0. && Extra->CapturedAt<0.)return StepSeizeClear(SID,O,Visible);
 if(Visible && O.Armed>0)
 {
  if(Plan.Reason!=TEXT("SEIZE SECTOR / FIRE") &&
     Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE SECTOR / FIRE")))
   Event(TEXT("SeizeContact"));
  Seize->QuietSince=-1.;Seize->LastSeen=Now;
  return true;
 }
 const TCHAR* MoveReason=TEXT("SEIZE SECTOR / MOVE ON");
 const FVector Center=SeizeExtendedGoal(SID,MoveReason);
 if(Plan.Reason==TEXT("SEIZE SECTOR / FIRE"))
 {
  // Four seconds since a member last saw an enemy himself, and no damage (SafeSeconds also waits for shared and remembered
  // contacts to fade, which kept the squad in place; Codex review 2026-11-27).
  if(Now-Seize->LastSeen>=4. && O.RecentDamage==0 && Plan.Age>=3 &&
     Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,Center,MoveReason))
   Event(TEXT("SeizeAgain"));
  return true;
 }
 if(SeizePointWaiting(SID))return true;
 // Retry a rejected go order, or leave assembly if another squad captured the point first.
 if(Point && Plan.Reason==TEXT("SEIZE SECTOR / ASSEMBLE"))
  Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,Center,TEXT("SEIZE SECTOR / ASSAULT POINT"));
 if(BattleSectors::Cell(Here)==Seize->Cell)
 {
  if(Seize->EnterAt<0.){Seize->EnterAt=Now;Event(TEXT("SeizeEnter"));}
  if(Known)Seize->QuietSince=-1.;
  else if(Seize->QuietSince<0.)Seize->QuietSince=Now;
  if((!Point || Support) && Seize->TakenAt<0. && Seize->QuietSince>=0. && Now-Seize->QuietSince>=15. &&
     Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE SECTOR / HOLD TAKEN")))
  {
   // Hold keeps the soldiers where they are; Defend can rebuild routes using DefenseRadius.
   Seize->TakenAt=Now;Event(TEXT("SeizeTaken"));LogSeizeSector(SID,*Seize,TEXT("SeizeTaken"));
   if(BattleTactics::SectorSeizeCounter()>0 && Extra && Extra->Counter)Event(TEXT("SeizeCounterTaken"));
   if(Support)Event(TEXT("SeizePointSupportTaken"));
  }
 }
 else Seize->QuietSince=-1.;
 if(Seize->TakenAt>=0.)
 {
  // A contact after capture also resumes the move; hold again once back inside.
  if(BattleSectors::Cell(Here)==Seize->Cell && Plan.Reason!=TEXT("SEIZE SECTOR / HOLD TAKEN"))
   Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE SECTOR / HOLD TAKEN"));
  // Assaults take only observed ownership; supports wait for the focus to finish.
  double HoldFrom=Seize->TakenAt;
  if(Support)
  {
   if(Extra->ReleasedAt<0. && Extra->CapturedAt<0.)return true;
   HoldFrom=FMath::Max(Seize->TakenAt,Extra->ReleasedAt>=0.?Extra->ReleasedAt:Extra->CapturedAt);
  }
  else if(Point)
  {
   HoldFrom=FMath::Max(Seize->TakenAt,Extra->CapturedAt);
   if(Now-HoldFrom<BattleTactics::SectorSeizeHold())return true;
  }
  FIntPoint Next;
  if(PickSeizeExtended(SID,*S,Here,&Seize->Cell,Next))
  {
   // Apply first; a failed route must not close the current hold.
   const FSeizeSector Previous=*Seize;
   if(StartSeizeExtended(SID,Next,TEXT("SEIZE SECTOR / NEXT")))
   {
    SeizeEnded.FindOrAdd(SID).Add(Previous.Cell,Now);Event(TEXT("SeizeEndNext"));
    LogSeizeSector(SID,Previous,TEXT("SeizeEndNext"));Event(TEXT("SeizeNext"));return true;
   }
  }
  if(Now-HoldFrom>=BattleTactics::SectorSeizeHold())
  {
   EndSeize(SID,TEXT("SeizeEndHeld"));
   if(!ResumeMission(SID,TEXT("SECTOR TAKEN / RESUME MISSION"),nullptr))
    Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SECTOR TAKEN / HOLD"));
  }
 }
 return true;
}

bool UBattleSquadTactics::PickSeizeSupport(int32 Team,FIntPoint& Out) const
{
 const auto& Focus=SeizePoints[Team];const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const int32 Turn=Match->AreaHome(Team).X>0?-1:1;
 bool Found=false;int32 BestKnown=-1,BestNear=MAX_int32,BestSlots=-1;FIntPoint BestRead=FIntPoint::ZeroValue;
 const FIntPoint Anchor=BattleSectors::Valid(Focus.Assembly)?Focus.Assembly:BattleSectors::Cell(Match->AreaHome(Team));
 for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
 {
  if(!DX && !DY)continue;
  const FIntPoint Candidate=Focus.Cell+FIntPoint(DX,DY);
  if(Candidate==Focus.Assembly || !BattleSectors::Valid(Candidate) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Candidate)))continue;
  int32 Known=0;SeizeSectorKnown(Team,Candidate,Clock(),&Known);
  const int32 Near=FMath::Max(FMath::Abs(Candidate.X-Anchor.X),FMath::Abs(Candidate.Y-Anchor.Y));
  const int32 Slots=SeizeTrialSlots.FindRef(Candidate);const FIntPoint ReadCell(Candidate.X*Turn,Candidate.Y*Turn);
  const bool Smaller=ReadCell.X<BestRead.X || (ReadCell.X==BestRead.X && ReadCell.Y<BestRead.Y);
  bool Better=!Found || Known>BestKnown;
  if(Found && Known==BestKnown)
   Better=Known>0?Smaller:Near<BestNear || (Near==BestNear && (Slots>BestSlots || (Slots==BestSlots && Smaller)));
  if(!Better)continue;
  Found=true;BestKnown=Known;BestNear=Near;BestSlots=Slots;BestRead=ReadCell;Out=Candidate;
 }
 return Found;
}

bool UBattleSquadTactics::SeizeSupportFocus(int32 SID,FVector& Out) const
{
 if(!BattleTactics::SectorSeizePoint() || !BattleTactics::SectorSeizePointSupport())return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto* Extra=SeizeExtras.Find(SID);const auto* Open=SeizeSectors.Find(SID);
 if(!S || !SectorSeizeEnabled(S->Team) || !Extra || !Extra->Support || !Open || Open->EndAt>=0. ||
    S->TacticalPlan.Action!=EBattleSquadTactic::Engage || !S->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")))return false;
 Out=BattleSectors::Center(Extra->PointCell);return true;
}

bool UBattleSquadTactics::SeizeSupportTarget(int32 SID,const FVector& At) const
{
 FVector Focus;return SeizeSupportFocus(SID,Focus) && BattleSectors::Cell(At)==BattleSectors::Cell(Focus);
}

bool UBattleSquadTactics::SeizeFreeSquad(int32 SID) const
{
 if(!BattleTactics::SectorSeizeCounter())return false;
 const auto* CounterState=GetWorld()->GetSubsystem<UBattleState>();const auto* CounterSquad=CounterState->FindSquad(SID);
 const auto* CounterMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 if(!CounterSquad || !SectorSeizeEnabled(CounterSquad->Team) || CounterSquad->CommandStatus!=EBattleCommandStatus::Active ||
    (CounterSquad->Team==0 && !CounterSquad->bPlayerDelegated) || CounterMatch->GroupOf(SID)!=INDEX_NONE ||
    CounterMatch->FrontPosts.Contains(SID) || CounterMatch->Area.HirePurpose.Contains(SID) || CounterMatch->IntelPurpose.Contains(SID) ||
    CounterMatch->SurpriseMoving(SID) || CounterMatch->RaidSneaking(SID) || BattleSquadType::IsVehicle(CounterSquad->SquadType) ||
    BattleSquadType::IsArtillery(CounterSquad->SquadType))return false;
 const bool* ConfirmedBase=CounterMatch->SectorTrialBaseSlots.Find(SID);
 if(!ConfirmedBase || *ConfirmedBase)return false;
 for(int32 MemberID:CounterSquad->Members)if(const auto* Member=CounterState->Find(MemberID);Member && Member->IsAlive() && Member->Action==EBattleAction::DirectControl)return false;
 return true;
}

bool UBattleSquadTactics::BeginFreeCounter(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 if(!SeizeFreeSquad(SID) || Visible || O.RecentDamage!=0 || O.Direct>0)return false;
 if(BattleTactics::SectorSeizePoint() && BattleTactics::SectorSeizePointAT()>0)
 {
  const auto* FreeATSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
  if(FreeATSquad && FreeATSquad->SquadType==BattleSquadType::AntiTank)
  {
   const auto* FreeATMatch=GetWorld()->GetSubsystem<UBattleMatch>();
   if(!O.Contacts.IsEmpty() || FreeATMatch->ATHunts.Contains(SID) || FreeATMatch->SupplyHunts.Contains(SID))return false;
  }
 }
 const auto* CounterSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto CounterAction=CounterSquad->TacticalPlan.Action;
 if((CounterAction!=EBattleSquadTactic::Mission && CounterAction!=EBattleSquadTactic::Engage) ||
    CounterSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(SID))return false;
 const auto* CounterTune=GetDefault<UBattleTuning>();const double CounterNow=Clock();
 if(O.Armed<FMath::CeilToInt(O.Living*.6f) || O.Living<=CounterSquad->Members.Num()*CounterTune->SquadRetreatLivingRatio ||
    O.Health/FMath::Max(1.f,O.FullHealth(CounterTune->Health))<CounterTune->SquadRetreatHealthRatio)return false;
 const FVector CounterHere=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(CounterSquad->LeaderUnitID);
 for(const auto& CounterContact:O.Contacts)
 {
  if(CounterContact.Value.bVisible && !CounterContact.Value.bShared)return false;
  if(FVector::Dist2D(CounterHere,CounterContact.Value.LastKnownPosition)<=CounterTune->EffectiveWeaponRange() &&
     !SeizeCounterCell(CounterSquad->Team,BattleSectors::Cell(CounterContact.Value.LastKnownPosition)))return false;
 }
 if(const auto* PreviousCounter=SeizeSectors.Find(SID);PreviousCounter &&
    (PreviousCounter->EndAt<0. || CounterNow-PreviousCounter->EndAt<5.))return false;
 FIntPoint CounterCell;
 if(!PickSeizeExtended(SID,*CounterSquad,CounterHere,nullptr,CounterCell,true))return false;
 if(!StartSeizeExtended(SID,CounterCell,TEXT("SEIZE SECTOR / COUNTER")))return false;
 if(auto* PreviousJoin=JoinFights.Find(SID))PreviousJoin->EndAt=CounterNow;
 return true;
}

bool UBattleSquadTactics::StepSeizeClear(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 auto& ClearExtra=SeizeExtras.FindChecked(SID);auto& ClearOpen=SeizeSectors.FindChecked(SID);
 const auto* ClearState=GetWorld()->GetSubsystem<UBattleState>();const auto* ClearSquad=ClearState->FindSquad(SID);
 const auto* ClearRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const double ClearNow=Clock();
 const FVector ClearHere=ClearRuntime->Position(ClearSquad->LeaderUnitID);
 const bool ClearArmourCovered=SeizeArmourCover(SID);
 // Use delivered positions only, with stable ties (contact ID / spot coordinates).
 FVector ClearTarget=FVector::ZeroVector;FIntPoint TargetSpot=FIntPoint::ZeroValue;
 bool HasTarget=false,HasVisibleTarget=false;double NearestTarget=DBL_MAX;int32 NearestContact=MAX_int32;
 for(const auto& ClearContact:O.Contacts)
 {
  const auto& SeenContact=ClearContact.Value;
  if(ClearArmourCovered && (BattleSquadType::IsVehicle(SeenContact.ObservedSquadType) ||
     SeizeKnownVehicle(ClearSquad->Team,ClearContact.Key,SeenContact.LastKnownPosition,false)))continue;
  if(!SeenContact.bVisible || SeenContact.bShared || BattleSectors::Cell(SeenContact.LastKnownPosition)!=ClearOpen.Cell)continue;
  const double ContactDistance=FVector::DistSquared2D(ClearHere,SeenContact.LastKnownPosition);
  if(ContactDistance>NearestTarget || (ContactDistance==NearestTarget && ClearContact.Key>=NearestContact))continue;
  NearestTarget=ContactDistance;NearestContact=ClearContact.Key;ClearTarget=SeenContact.LastKnownPosition;HasTarget=true;HasVisibleTarget=true;
  TargetSpot=FIntPoint(FMath::FloorToInt32(ClearTarget.X/5000.),FMath::FloorToInt32(ClearTarget.Y/5000.));
 }
 if(!HasTarget)
 {
  bool BestReserved=true;
  for(const auto& ClearKnown:KnownSpots[ClearSquad->Team])
  {
   if(ClearNow<ClearKnown.Value.Seen || ClearNow-ClearKnown.Value.Seen>BattleTactics::SeekKnownSeconds() || BattleSectors::Cell(ClearKnown.Value.At)!=ClearOpen.Cell)continue;
   if(ClearArmourCovered)
   {
    bool ClearVehicleSpot=SeizeKnownVehicle(ClearSquad->Team,INDEX_NONE,ClearKnown.Value.At,true);
    for(const auto& ClearVehicleContact:O.Contacts)
     if(BattleSquadType::IsVehicle(ClearVehicleContact.Value.ObservedSquadType) &&
        FMath::FloorToInt32(ClearVehicleContact.Value.LastKnownPosition.X/5000.)==ClearKnown.Key.X &&
        FMath::FloorToInt32(ClearVehicleContact.Value.LastKnownPosition.Y/5000.)==ClearKnown.Key.Y)
     {ClearVehicleSpot=true;break;}
    if(ClearVehicleSpot)continue;
   }
   bool Reserved=false;
   for(const auto& OtherClear:SeizeExtras)
   {
    const auto* OtherOpen=SeizeSectors.Find(OtherClear.Key);const auto* OtherSquad=ClearState->FindSquad(OtherClear.Key);
    if(OtherClear.Key!=SID && OtherSquad && OtherSquad->CommandStatus==EBattleCommandStatus::Active && OtherSquad->Team==ClearSquad->Team && OtherOpen && OtherOpen->EndAt<0. &&
       OtherClear.Value.Point==ClearExtra.Point && OtherClear.Value.FocusStart==ClearExtra.FocusStart && !OtherClear.Value.Support &&
       OtherClear.Value.ClearHasSpot && OtherClear.Value.ClearSpot==ClearKnown.Key && OtherSquad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")))
    {Reserved=true;break;}
   }
   const double SpotDistance=FVector::DistSquared2D(ClearHere,ClearKnown.Value.At);
   if(HasTarget && (Reserved>BestReserved || (Reserved==BestReserved && (SpotDistance>NearestTarget ||
      (SpotDistance==NearestTarget && (ClearKnown.Key.X>TargetSpot.X || (ClearKnown.Key.X==TargetSpot.X && ClearKnown.Key.Y>=TargetSpot.Y)))))))continue;
   HasTarget=true;BestReserved=Reserved;NearestTarget=SpotDistance;ClearTarget=ClearKnown.Value.At;TargetSpot=ClearKnown.Key;
  }
 }
 // Charge only the interval whose preceding evaluation left the squad firing.
 if(ClearExtra.FireSeconds>=0. && ClearExtra.FireCheckedAt>=0. && ClearExtra.FireWasActive &&
    ClearSquad->TacticalPlan.Reason==TEXT("SEIZE SECTOR / FIRE") && !ClearExtra.Closing)
  ClearExtra.FireSeconds+=FMath::Max(0.,ClearNow-ClearExtra.FireCheckedAt);
 ClearExtra.FireCheckedAt=ClearNow;
 ON_SCOPE_EXIT
 {
  ClearExtra.FireWasActive=ClearSquad->TacticalPlan.Reason==TEXT("SEIZE SECTOR / FIRE");
 };
 if(!HasTarget || (ClearExtra.FireSeconds>=0. && ClearExtra.FireSpot!=TargetSpot)){ClearExtra.FireSeconds=-1.;ClearExtra.Closing=false;}
 if(HasVisibleTarget && ClearExtra.FireSeconds<0.){ClearExtra.FireSeconds=0.;ClearExtra.FireSpot=TargetSpot;}
 if(HasTarget && ClearExtra.FireSeconds>=20.)ClearExtra.Closing=true;
 if(Visible && O.Armed>0)
 {
  ClearOpen.LastSeen=ClearNow;
  if(!ClearExtra.Closing || !HasTarget || FVector::DistSquared2D(ClearHere,ClearTarget)<=FMath::Square(1500.))
  {
   if(ClearSquad->TacticalPlan.Reason!=TEXT("SEIZE SECTOR / FIRE"))
    if(Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,ClearHere,TEXT("SEIZE SECTOR / FIRE")))Event(TEXT("SeizeContact"));
   return true;
  }
 }
 else if(ClearSquad->TacticalPlan.Reason==TEXT("SEIZE SECTOR / FIRE") && ClearNow-ClearOpen.LastSeen<2.)return true;
 if(ClearNow-ClearExtra.ClearPickAt<3.)return true;
 ClearExtra.ClearPickAt=ClearNow;
 const TCHAR* ClearReason=TEXT("SEIZE SECTOR / CLEAR POINT");const TCHAR* ClearEvent=nullptr;
 FVector ClearGoal;
 if(HasTarget)
 {
  ClearGoal=ClearTarget;
  if(ClearExtra.Closing)
  {
   if(FVector::DistSquared2D(ClearHere,ClearTarget)>FMath::Square(1500.))ClearGoal+=(ClearHere-ClearTarget).GetSafeNormal2D()*1500.;
   else ClearGoal=ClearHere;
   ClearReason=TEXT("SEIZE SECTOR / CLEAR POINT / CLOSE");ClearEvent=TEXT("SeizePointClose");
  }
  else ClearEvent=TEXT("SeizePointClearSpot");
 }
 else
 {
  const auto* ClearMatch=GetWorld()->GetSubsystem<UBattleMatch>();
  if(!ClearMatch->Objectives.IsValidIndex(ClearExtra.Point) || ClearMatch->ObservedOwner(ClearSquad->Team,ClearExtra.Point)==ClearSquad->Team)return true;
  if(ClearExtra.SweepAt>=0. && (FVector::DistSquared2D(ClearHere,ClearExtra.ClearGoal)<=FMath::Square(4000.) || ClearNow-ClearExtra.SweepAt>=20.))
  {ClearExtra.SweepIndex=(ClearExtra.SweepIndex+1)%5;ClearExtra.SweepAt=-1.;}
  auto* ClearNav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!ClearNav)return true;
  bool SweepFound=false;
  static const FVector SweepDirections[]={FVector(1,1,0),FVector(-1,1,0),FVector(-1,-1,0),FVector(1,-1,0)};
  for(int32 SweepAttempt=0;SweepAttempt<5;++SweepAttempt)
  {
   ClearGoal=ClearExtra.SweepIndex==0?ClearMatch->Objectives[ClearExtra.Point].Position:
    BattleSectors::Center(ClearOpen.Cell)+SweepDirections[ClearExtra.SweepIndex-1].GetSafeNormal2D()*1500.;
   BattleFieldLayout::Of(GetWorld()).Heights.Lift(ClearGoal);FNavLocation SweepProjected;
   if(ClearNav->ProjectPointToNavigation(ClearGoal,SweepProjected,FVector(100,100,250)) && BattleSectors::Cell(SweepProjected.Location)==ClearOpen.Cell)
   {ClearGoal=SweepProjected.Location;SweepFound=true;break;}
   ClearExtra.SweepIndex=(ClearExtra.SweepIndex+1)%5;ClearExtra.SweepAt=-1.;
  }
  if(!SweepFound)return true;
  ClearEvent=TEXT("SeizePointClearSweep");
 }
 // Keep close goals inside the sector; retain the successful Nav projection for a sweep.
 if(HasTarget)
 {
  const FVector ClearCenter=BattleSectors::Center(ClearOpen.Cell);
  ClearGoal.X=FMath::Clamp(ClearGoal.X,ClearCenter.X-2450.,ClearCenter.X+2450.);
  ClearGoal.Y=FMath::Clamp(ClearGoal.Y,ClearCenter.Y-2450.,ClearCenter.Y+2450.);
  BattleFieldLayout::Of(GetWorld()).Heights.Lift(ClearGoal);
 }
 if(ClearSquad->Order==EBattleOrder::AttackMove && ClearSquad->TacticalPlan.Reason==ClearReason && ClearExtra.ClearGoal.Equals(ClearGoal,1.))
 {
  if(!HasTarget && ClearExtra.SweepAt<0.)ClearExtra.SweepAt=ClearNow;
  ClearExtra.ClearHasSpot=HasTarget;ClearExtra.ClearSpot=TargetSpot;return true;
 }
 if(Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,ClearGoal,ClearReason))
 {
  ClearExtra.ClearGoal=ClearGoal;ClearExtra.ClearHasSpot=HasTarget;ClearExtra.ClearSpot=TargetSpot;
  if(!HasTarget && ClearExtra.SweepAt<0.)ClearExtra.SweepAt=ClearNow;
  if(HasTarget)ClearExtra.SweepAt=-1.;
  Event(ClearEvent);
 }
 return true;
}


bool UBattleSquadTactics::SeizeEscort(int32 SID) const
{
 if(!BattleTactics::SectorSeizePoint())return false;
 const auto* EscortDetail=SeizeExtras.Find(SID);
 if(!EscortDetail || !EscortDetail->Escort)return false;
 const auto* EscortSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 return EscortSquad && (EscortSquad->SquadType==BattleSquadType::AntiTank?
  BattleTactics::SectorSeizePointAT()>0:BattleTactics::SectorSeizePointTank()>0);
}

int32 UBattleSquadTactics::SeizeEscortCount(int32 Team,uint8 Type) const
{
 if(!BattleTactics::SectorSeizePoint())return 0;
 const auto* EscortStore=GetWorld()->GetSubsystem<UBattleState>();const auto& EscortFocus=SeizePoints[Team];
 int32 EscortCount=0;
 for(const auto& EscortPair:SeizeExtras)
 {
  const auto* EscortSquad=EscortStore->FindSquad(EscortPair.Key);
  if(!EscortSquad || EscortSquad->Team!=Team || EscortSquad->SquadType!=Type ||
     EscortSquad->CommandStatus!=EBattleCommandStatus::Active || !SeizeEscort(EscortPair.Key) || !SeizePointMember(EscortPair.Key) ||
     EscortPair.Value.Point!=EscortFocus.Point || EscortPair.Value.FocusStart!=EscortFocus.Start)continue;
  for(int32 EscortUnitID:EscortSquad->Members)if(const auto* EscortUnit=EscortStore->Find(EscortUnitID);EscortUnit && EscortUnit->IsAlive())
  {++EscortCount;break;}
 }
 return EscortCount;
}

bool UBattleSquadTactics::SeizeEscortGoal(int32 SID,FIntPoint Cell,FIntPoint Assembly,FVector& Goal) const
{
 if(!BattleSectors::Valid(Assembly))return false;
 const auto* EscortStore=GetWorld()->GetSubsystem<UBattleState>();const auto* EscortSquad=EscortStore->FindSquad(SID);
 const auto* EscortRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 auto* EscortNav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if(!EscortSquad || !EscortNav || Cell==Assembly)return false;
 const ANavigationData* EscortNavData=EscortRuntime->NavFor(EscortSquad->LeaderUnitID);
 if(BattleSquadType::IsVehicle(EscortSquad->SquadType) && !EscortNavData)return false;
 const FVector EscortCenter=BattleSectors::Center(Cell);
 const FVector EscortBack=(BattleSectors::Center(Assembly)-EscortCenter).GetSafeNormal2D();
 const FVector EscortEntry=EscortCenter+FVector(FMath::Sign(EscortBack.X)*1250.,FMath::Sign(EscortBack.Y)*1250.,0.);
 const FVector EscortSide(-EscortBack.Y,EscortBack.X,0);
 // A bounded search on the unit's Nav data: vehicle clearance excludes buildings/narrow infantry cover.
 // Stay within 1000..1500 cm of the entry boundary; never fall back to the point centre.
 for(int32 EscortAttempt=0;EscortAttempt<9;++EscortAttempt)
 {
  const int32 EscortOffset=EscortAttempt==0?0:((EscortAttempt+1)/2)*(EscortAttempt%2?1:-1);
  FVector EscortWant=EscortEntry+EscortSide*(EscortOffset*400.);
  BattleFieldLayout::Of(GetWorld()).Heights.Lift(EscortWant);
  FNavLocation EscortProjected;
  if(!EscortNav->ProjectPointToNavigation(EscortWant,EscortProjected,FVector(100,100,250),EscortNavData) ||
     BattleSectors::Cell(EscortProjected.Location)!=Cell)continue;
  const FVector EscortLocal=EscortProjected.Location-EscortCenter;
  const double EscortDepthX=2500.-EscortLocal.X*FMath::Sign(EscortBack.X);
  const double EscortDepthY=2500.-EscortLocal.Y*FMath::Sign(EscortBack.Y);
  if((!FMath::IsNearlyZero(EscortBack.X) && (EscortDepthX<1000. || EscortDepthX>1500.)) ||
     (!FMath::IsNearlyZero(EscortBack.Y) && (EscortDepthY<1000. || EscortDepthY>1500.)))continue;
  Goal=EscortProjected.Location;return true;
 }
 return false;
}

void UBattleSquadTactics::UpdateSeizeTanks(int32 Team)
{
 if(!BattleTactics::SectorSeizePoint() || BattleTactics::SectorSeizePointTank()<=0 || !SectorSeizeEnabled(Team))return;
 const auto* EscortStore=GetWorld()->GetSubsystem<UBattleState>();const auto* EscortMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 const auto* EscortRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto& EscortFocus=SeizePoints[Team];
 // End old escorts even when no rifle remains to evaluate the old concentration.
 TArray<int32> EscortExisting;
 for(const auto& EscortPair:SeizeExtras)
  if(const auto* EscortSquad=EscortStore->FindSquad(EscortPair.Key);EscortSquad && EscortSquad->Team==Team &&
     SeizeEscort(EscortPair.Key) && SeizePointMember(EscortPair.Key))EscortExisting.Add(EscortPair.Key);
 EscortExisting.Sort();
 for(int32 EscortID:EscortExisting)StepSeizeEscort(EscortID);
 if(EscortFocus.Point==INDEX_NONE || EscortFocus.Taken || EscortFocus.Broken || !BattleSectors::Valid(EscortFocus.Assembly))return;
 bool EscortHasRifle=false;
 for(const auto& EscortPair:SeizeExtras)
 {
  const auto* EscortSquad=EscortStore->FindSquad(EscortPair.Key);
  if(!EscortSquad || EscortSquad->Team!=Team || EscortSquad->SquadType!=BattleSquadType::Rifle ||
     EscortSquad->CommandStatus!=EBattleCommandStatus::Active || (Team==0 && !EscortSquad->bPlayerDelegated) ||
     EscortPair.Value.Point!=EscortFocus.Point || EscortPair.Value.FocusStart!=EscortFocus.Start || !SeizePointMember(EscortPair.Key))continue;
  for(int32 EscortUnitID:EscortSquad->Members)if(const auto* EscortUnit=EscortStore->Find(EscortUnitID);EscortUnit && EscortUnit->IsAlive())
  {EscortHasRifle=true;break;}
 }
 if(!EscortHasRifle)return;
 const FVector EscortAssembly=BattleSectors::Center(EscortFocus.Assembly);const auto* EscortTune=GetDefault<UBattleTuning>();
 TArray<int32> EscortTanks,EscortCars;bool EscortAnyTank=false;
 for(const auto& EscortPair:EscortStore->GetSquads())
 {
  const auto& EscortSquad=EscortPair.Value;
  if(EscortSquad.Team!=Team || (EscortSquad.SquadType!=BattleSquadType::Tank && EscortSquad.SquadType!=BattleSquadType::ArmoredCar))continue;
  const auto* EscortUnit=EscortStore->Find(EscortSquad.LeaderUnitID);
  if(!EscortUnit || !EscortUnit->IsAlive())continue;
  if(EscortSquad.SquadType==BattleSquadType::Tank)EscortAnyTank=true;
  const auto* EscortSample=Find(EscortPair.Key);const bool* EscortBase=EscortMatch->SectorTrialBaseSlots.Find(EscortPair.Key);
  if(EscortUnit->Action==EBattleAction::DirectControl || EscortSquad.CommandStatus!=EBattleCommandStatus::Active || (Team==0 && !EscortSquad.bPlayerDelegated) ||
     EscortSquad.TacticalPlan.Action!=EBattleSquadTactic::Mission || EscortSquad.TacticalPlan.Reason.Contains(TEXT("EXPLICIT ORDER")) ||
     !EscortSquad.bHasMission || EscortMatch->GroupOf(EscortPair.Key)!=INDEX_NONE || !EscortBase || *EscortBase ||
     EscortMatch->Area.HirePurpose.Contains(EscortPair.Key) || EscortMatch->IntelPurpose.Contains(EscortPair.Key) ||
     EscortMatch->FrontPosts.Contains(EscortPair.Key) || EscortMatch->PlayerRoutes.Contains(EscortPair.Key) ||
     EscortMatch->ATHunts.Contains(EscortPair.Key) || EscortMatch->SupplyHunts.Contains(EscortPair.Key) ||
     EscortMatch->SurpriseMoving(EscortPair.Key) || EscortMatch->RaidSneaking(EscortPair.Key) ||
     !EscortSample || Clock()-EscortSample->SampleTime>2.5 || EscortSample->Direct>0 || EscortSample->Living==0 ||
     EscortSample->Armed==0 || EscortSample->RecentDamage>0 || EscortSquad.TacticalPlan.RetrySeconds>0 ||
     EscortSample->Health/FMath::Max(1.f,EscortSample->FullHealth(EscortTune->Health))<
       FMath::Max(EscortTune->VehicleRepairRatio,EscortTune->SquadRetreatHealthRatio))continue;
  if(const auto* EscortOld=SeizeSectors.Find(EscortPair.Key);EscortOld && (EscortOld->EndAt<0. || Clock()-EscortOld->EndAt<30.))continue;
  (EscortSquad.SquadType==BattleSquadType::Tank?EscortTanks:EscortCars).Add(EscortPair.Key);
 }
 auto EscortNearer=[&](int32 LeftID,int32 RightID)
 {
  const double LeftDistance=FVector::DistSquared2D(EscortRuntime->Position(EscortStore->FindSquad(LeftID)->LeaderUnitID),EscortAssembly);
  const double RightDistance=FVector::DistSquared2D(EscortRuntime->Position(EscortStore->FindSquad(RightID)->LeaderUnitID),EscortAssembly);
  return LeftDistance<RightDistance || (LeftDistance==RightDistance && LeftID<RightID);
 };
 EscortTanks.Sort(EscortNearer);EscortCars.Sort(EscortNearer);
 const int32 EscortAssigned=SeizeEscortCount(Team,BattleSquadType::Tank)+SeizeEscortCount(Team,BattleSquadType::ArmoredCar);
 int32 EscortSlots=BattleTactics::SectorSeizePointTank()-EscortAssigned;
 const auto& EscortCandidates=EscortAnyTank?EscortTanks:EscortCars;
 if(!EscortAnyTank)EscortSlots=FMath::Min(EscortSlots,1-EscortAssigned);
 for(int32 EscortID:EscortCandidates)
 {
  if(EscortSlots<=0)break;
  if(StartSeizeExtended(EscortID,EscortFocus.Cell,TEXT("SEIZE SECTOR / ASSEMBLE")))--EscortSlots;
 }
}

bool UBattleSquadTactics::StepSeizeEscort(int32 SID)
{
 if(!SeizeEscort(SID) || !SeizePointMember(SID))return false;
 const auto* EscortStore=GetWorld()->GetSubsystem<UBattleState>();const auto* EscortSquad=EscortStore->FindSquad(SID);
 auto* EscortRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto* EscortMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 auto& EscortDetail=SeizeExtras.FindChecked(SID);const auto& EscortFocus=SeizePoints[EscortSquad->Team];
 // Never overwrite direct control, a new explicit command, or safety transitions.
 if(EscortSquad->CommandStatus!=EBattleCommandStatus::Active || (EscortSquad->Team==0 && !EscortSquad->bPlayerDelegated))return true;
 for(int32 EscortUnitID:EscortSquad->Members)if(const auto* EscortUnit=EscortStore->Find(EscortUnitID);EscortUnit && EscortUnit->IsAlive() &&
    EscortUnit->Action==EBattleAction::DirectControl)return true;
 bool EscortRifles=false;
 for(const auto& EscortPair:SeizeExtras)
 {
  const auto* RifleSquad=EscortStore->FindSquad(EscortPair.Key);
  if(!RifleSquad || RifleSquad->Team!=EscortSquad->Team || RifleSquad->SquadType!=BattleSquadType::Rifle ||
     RifleSquad->CommandStatus!=EBattleCommandStatus::Active || (RifleSquad->Team==0 && !RifleSquad->bPlayerDelegated) ||
     EscortPair.Value.Point!=EscortDetail.Point || EscortPair.Value.FocusStart!=EscortDetail.FocusStart || !SeizePointMember(EscortPair.Key))continue;
  for(int32 RifleUnitID:RifleSquad->Members)if(const auto* RifleUnit=EscortStore->Find(RifleUnitID);RifleUnit && RifleUnit->IsAlive())
  {EscortRifles=true;break;}
 }
 const bool EscortVehicle=BattleSquadType::IsVehicle(EscortSquad->SquadType);
 const bool EscortOver=EscortFocus.Point!=EscortDetail.Point || EscortFocus.Start!=EscortDetail.FocusStart ||
  EscortFocus.Taken || EscortFocus.Broken || EscortDetail.CapturedAt>=0. ||
  EscortMatch->ObservedOwner(EscortSquad->Team,EscortDetail.Point)==EscortSquad->Team ||
  (EscortVehicle && !EscortRifles) || SeizeExtendedEnd(SID,Clock())!=nullptr;
 if(EscortOver)
 {
  // Tactical plans keep mission fields separately. Restore the saved vehicle mission before releasing the plan.
  if(EscortVehicle && !EscortRuntime->IssueOrder(SID,EscortDetail.EscortMission.Order,EscortDetail.EscortMission.Goal,true,EscortDetail.EscortMission.bSector))
   return true; // Retry without sending the vehicle deeper into the point.
  if(!ResumeMission(SID,TEXT("POINT ESCORT OVER / RESUME MISSION"),nullptr))return true;
  EndSeize(SID,TEXT("SeizeEndOther"));return true;
 }
 const bool EscortWaiting=EscortFocus.GoAt<0.;
 const FVector EscortGoal=EscortWaiting?BattleSectors::Center(EscortDetail.EscortAssembly):EscortDetail.EscortGoal;
 const FVector EscortHere=EscortRuntime->Position(EscortSquad->LeaderUnitID);
 const bool EscortArrived=FVector::DistSquared2D(EscortHere,EscortGoal)<=FMath::Square(250.);
 if(!EscortWaiting && EscortArrived)EscortDetail.EscortAtPost=true;
 const TCHAR* EscortReason=EscortWaiting?TEXT("SEIZE SECTOR / ASSEMBLE"):
  !EscortVehicle?TEXT("SEIZE SECTOR / AT OVERWATCH"):
  EscortDetail.EscortAtPost?TEXT("SEIZE SECTOR / TANK OVERWATCH"):TEXT("SEIZE SECTOR / ESCORT MOVE");
 // AT retains its sector arrival-cover route. Vehicles hold once at the post, without a defence-radius route.
 const EBattleOrder EscortOrder=(EscortWaiting?EscortArrived:EscortVehicle && EscortDetail.EscortAtPost)?EBattleOrder::Hold:EBattleOrder::AttackMove;
 if(EscortSquad->Order!=EscortOrder || EscortSquad->TacticalPlan.Reason!=EscortReason || !EscortSquad->Destination.Equals(EscortGoal,1.))
  Apply(SID,EBattleSquadTactic::Engage,EscortOrder,EscortGoal,EscortReason);
 GetWorld()->GetSubsystem<UBattleState>()->Squads.FindChecked(SID).TacticalPlan.Remaining=60.f;
 return true;
}

bool UBattleSquadTactics::SeizeArmourCover(int32 SID) const
{
 if(!BattleTactics::SectorSeizePoint() || (BattleTactics::SectorSeizePointAT()==0 && BattleTactics::SectorSeizePointTank()==0) ||
    !SeizePointMember(SID))return false;
 const auto* EscortStore=GetWorld()->GetSubsystem<UBattleState>();const auto* EscortRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* CoveredSquad=EscortStore->FindSquad(SID);const auto& CoveredDetail=SeizeExtras.FindChecked(SID);
 const auto& CoveredFocus=SeizePoints[CoveredSquad->Team];
 if(CoveredFocus.Point!=CoveredDetail.Point || CoveredFocus.Start!=CoveredDetail.FocusStart || CoveredFocus.Taken || CoveredFocus.Broken)return false;
 for(const auto& EscortPair:SeizeExtras)
 {
  const auto* EscortSquad=EscortStore->FindSquad(EscortPair.Key);
  if(!EscortSquad || EscortSquad->Team!=CoveredSquad->Team ||
     (EscortSquad->SquadType!=BattleSquadType::AntiTank && EscortSquad->SquadType!=BattleSquadType::Tank) ||
     !SeizeEscort(EscortPair.Key) || !SeizePointMember(EscortPair.Key) ||
     EscortSquad->CommandStatus!=EBattleCommandStatus::Active || (EscortSquad->Team==0 && !EscortSquad->bPlayerDelegated) ||
     EscortPair.Value.Point!=CoveredDetail.Point || EscortPair.Value.FocusStart!=CoveredDetail.FocusStart)continue;
  for(int32 EscortUnitID:EscortSquad->Members)
  {
   const auto* EscortUnit=EscortStore->Find(EscortUnitID);if(!EscortUnit || !EscortUnit->IsAlive())continue;
   const FVector EscortAt=EscortRuntime->Position(EscortUnitID);const FIntPoint EscortCell=BattleSectors::Cell(EscortAt);
   if(EscortCell==CoveredFocus.Cell || EscortCell==CoveredFocus.Assembly)return true;
   const FVector EscortA=BattleSectors::Center(CoveredFocus.Assembly),EscortB=BattleSectors::Center(CoveredFocus.Cell);
   const FVector EscortAB=EscortB-EscortA;
   const double EscortAlong=FVector::DotProduct(EscortAt-EscortA,EscortAB)/EscortAB.SizeSquared2D();
   if(EscortAlong>=0. && EscortAlong<=1. &&
      FVector::DistSquared2D(EscortAt,EscortA+EscortAB*EscortAlong)<=FMath::Square(1250.))return true;
  }
 }
 return false;
}

bool UBattleSquadTactics::SeizeArmourStay(int32 SID)
{
 if(!SeizeArmourCover(SID))return false;
 const double StayNow=Clock();const double* StayLast=SeizeArmourStayAt.Find(SID);
 if(!StayLast || StayNow<*StayLast || StayNow-*StayLast>=30.)
 {SeizeArmourStayAt.Add(SID,StayNow);Event(TEXT("SeizePointArmourStay"));}
 return true;
}

bool UBattleSquadTactics::SeizeKnownVehicle(int32 Team,int32 Key,const FVector& At,bool Spot) const
{
 // Read the already delivered vehicle table. KnownEnemyVehicles/AreaIntelView would refresh it as a side effect.
 const auto* EscortMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 const float EscortMemory=FMath::Max(1.f,GetDefault<UBattleTuning>()->VehicleMemory);
 for(const auto& EscortKnown:EscortMatch->KnownVehicles[Team])
 {
  if(EscortKnown.Value.At>EscortMatch->Elapsed || EscortMatch->Elapsed-EscortKnown.Value.At>EscortMemory)continue;
  if(!Spot && EscortKnown.Key==Key)return true;
  if(Spot && FMath::FloorToInt32(At.X/5000.)==FMath::FloorToInt32(EscortKnown.Value.Position.X/5000.) &&
     FMath::FloorToInt32(At.Y/5000.)==FMath::FloorToInt32(EscortKnown.Value.Position.Y/5000.))return true;
 }
 return false;
}

bool UBattleSquadTactics::SeizeEscortVehicleTarget(int32 SID,int32 TargetID,uint8 SeenType) const
{
 if(!BattleTactics::SectorSeizePoint() || BattleTactics::SectorSeizePointAT()<=0 || !SeizeEscort(SID) || !SeizePointMember(SID))return false;
 const auto* EscortSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 return EscortSquad && EscortSquad->SquadType==BattleSquadType::AntiTank &&
  (BattleSquadType::IsVehicle(SeenType) || SeizeKnownVehicle(EscortSquad->Team,TargetID,FVector::ZeroVector,false));
}

bool UBattleSquadTactics::SeizeEscortPlace(int32 SID,const FVector& At) const
{
 if(!BattleTactics::SectorSeizePoint() || BattleTactics::SectorSeizePointAT()<=0 || !SeizeEscort(SID) || !SeizePointMember(SID))return true;
 const auto* EscortSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto& EscortDetail=SeizeExtras.FindChecked(SID);
 if(!EscortSquad || EscortSquad->SquadType!=BattleSquadType::AntiTank ||
    BattleSectors::Cell(EscortSquad->Destination)!=EscortDetail.PointCell)return true;
 if(BattleSectors::Cell(At)!=EscortDetail.PointCell)return false;
 const FVector EscortLocal=At-BattleSectors::Center(EscortDetail.PointCell);
 const FIntPoint EscortSide=EscortDetail.EscortAssembly-EscortDetail.PointCell;
 const double EscortDepthX=2500.-EscortLocal.X*FMath::Sign(EscortSide.X);
 const double EscortDepthY=2500.-EscortLocal.Y*FMath::Sign(EscortSide.Y);
 return (EscortSide.X==0 || (EscortDepthX>=1000. && EscortDepthX<=1500.)) &&
  (EscortSide.Y==0 || (EscortDepthY>=1000. && EscortDepthY<=1500.));
}
