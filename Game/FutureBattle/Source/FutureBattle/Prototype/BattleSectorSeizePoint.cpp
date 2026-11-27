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

int32 BattleTactics::SectorSeizePoint()
{
 static const int32 Mode=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSeizePoint="),Value);return FMath::Clamp(Value,0,1);}();
 return Mode;
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
 SeizeExtras.Reset();SeizeTrialSlots.Reset();SeizeTrialSlotsAt=-100.;
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
  if(Pair.Value.Point==Focus.Point && (!BattleTactics::SectorSeizePointSupport() || Pair.Value.FocusStart==Focus.Start) &&
     Squad && Squad->Team==Team && Open && Open->EndAt<0. &&
     Squad->CommandStatus==EBattleCommandStatus::Active &&
     (Squad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Pair.Key)))++AssignedCount;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE_POINT t=%.0f team=%d point=%d cell=%d,%d what=%s squads=%d"),
  GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,Team,Focus.Point,LogCell?LogCell->X:Focus.Cell.X,LogCell?LogCell->Y:Focus.Cell.Y,What,
  AssignedCount);
}

void UBattleSquadTactics::UpdateSeizeExtensions(int32 Team)
{
 if((!BattleTactics::SectorSeizePoint() && !BattleTactics::SectorSeizeCounter()) || !SectorSeizeEnabled(Team))return;
 const double Now=Clock();
 if(Now>=SeizeTrialAt[Team] && Now-SeizeTrialAt[Team]<1.)return;
 SeizeTrialAt[Team]=Now;
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
  if(Living && Pair.Value.SquadType==BattleSquadType::Rifle && Pair.Value.CommandStatus==EBattleCommandStatus::Active &&
     Pair.Value.TacticalPlan.Action==EBattleSquadTactic::Mission && Pair.Value.Order==EBattleOrder::Defend)RifleIDs.Add(Pair.Key);
 }
 RifleIDs.Sort();
 for(const auto& Spot:KnownSpots[Team])
  if(Now>=Spot.Value.Seen && Now-Spot.Value.Seen<=BattleTactics::SeekKnownSeconds())
   SeizeTrialKnown[Team].Add(BattleSectors::Cell(Spot.Value.At));
 TSet<FIntPoint> OwnedCells;OwnedCells.Add(BattleSectors::Cell(Match->AreaHome(Team)));
 for(const auto& Point:Match->Objectives)if(Point.Owner==Team)OwnedCells.Add(BattleSectors::Cell(Point.Position));
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
  if(BattleTactics::SectorSeizePoint() && Pair.Value.Point!=INDEX_NONE && Now-Open->Start>=360.)
  {EndSeize(Pair.Key,TEXT("SeizeEndPointTimeout"));SeizeSuspended.Remove(Pair.Key);}
  else if(Pair.Value.Point==INDEX_NONE && BattleTactics::SectorSeizeCounter() && Pair.Value.Counter && Open->TakenAt<0. && Now-Open->Start>=300.)
  {EndSeize(Pair.Key,TEXT("SeizeEndTimeout"));SeizeSuspended.Remove(Pair.Key);}
 }
 if(!BattleTactics::SectorSeizePoint())return;
 auto& Focus=SeizePoints[Team];
 TArray<int32> Assigned;
 for(const auto& Pair:SeizeExtras)
 {
  const auto* Squad=Store->FindSquad(Pair.Key);const auto* Open=SeizeSectors.Find(Pair.Key);
  if(BattleTactics::SectorSeizePointSupport()>0 && Squad)
  {
   bool Living=false;for(int32 ID:Squad->Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive()){Living=true;break;}
   if(!Living)continue;
  }
  if(Focus.Point!=INDEX_NONE && Pair.Value.Point==Focus.Point &&
     (!BattleTactics::SectorSeizePointSupport() || Pair.Value.FocusStart==Focus.Start) && Squad && Squad->Team==Team &&
     Squad->CommandStatus==EBattleCommandStatus::Active && Open && Open->EndAt<0. &&
     (Squad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Pair.Key)))Assigned.Add(Pair.Key);
 }
 Assigned.Sort();
 if(Focus.Point!=INDEX_NONE)
 {
  if(!Focus.Taken && Match->Objectives.IsValidIndex(Focus.Point) && Match->Objectives[Focus.Point].Owner==Team)
  {
   Focus.Taken=true;Event(TEXT("SeizePointTaken"));LogSeizePoint(Team,TEXT("taken"));
   for(int32 AssignedID:Assigned)SeizeExtras.FindChecked(AssignedID).CapturedAt=Now;
  }
  int32 Assaults=Assigned.Num();
  if(BattleTactics::SectorSeizePointSupport()>0)
  {
   Assaults=0;
   for(int32 AssignedID:Assigned)if(!SeizeExtras.FindChecked(AssignedID).Support)++Assaults;
  }
  if(Assaults>0 && !Focus.Taken)
  {
   if(Focus.GoAt<0.)
   {
    int32 Assembled=0;
    for(int32 AssignedID:Assigned)
    {
     const auto* Squad=Store->FindSquad(AssignedID);
     if(BattleTactics::SectorSeizePointSupport()>0 && SeizeExtras.FindChecked(AssignedID).Support)continue;
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
      if(Squad->TacticalPlan.Reason==TEXT("SEIZE SECTOR / ASSEMBLE"))
       Apply(AssignedID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,BattleSectors::Center(Focus.Cell),TEXT("SEIZE SECTOR / ASSAULT POINT"));
     }
    }
   }
   return; // Keep this point even if another point has become nearer.
  }
  if(!Focus.Taken && Focus.Committed)LogSeizePoint(Team,TEXT("drop"));
  if(BattleTactics::SectorSeizePointSupport()>0)
   for(int32 AssignedID:Assigned)if(auto& Detail=SeizeExtras.FindChecked(AssignedID);Detail.Support && Detail.ReleasedAt<0.)Detail.ReleasedAt=Now;
 }
 // Select once per second from public enemy ownership, excluding the enemy home sector.
 const FVector Home=Match->AreaHome(Team);const int32 Turn=Home.X>0?-1:1;
 const FIntPoint EnemyHome=BattleSectors::Cell(Match->AreaHome(1-Team));
 int32 BestPoint=INDEX_NONE;double BestDistance=DBL_MAX;FIntPoint BestCell=FIntPoint::ZeroValue,BestRead=FIntPoint::ZeroValue;
 for(int32 PointIndex=0;PointIndex<Match->Objectives.Num();++PointIndex)
 {
  const auto& Objective=Match->Objectives[PointIndex];const FIntPoint Candidate=BattleSectors::Cell(Objective.Position);
  if(Objective.Owner!=1-Team || Candidate==EnemyHome || !BattleSectors::Valid(Candidate) ||
     BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Candidate)))continue;
  double Distance=DBL_MAX;
  for(const FIntPoint& OwnCell:SeizeTrialOwn[Team])Distance=FMath::Min(Distance,FVector::DistSquared2D(BattleSectors::Center(OwnCell),BattleSectors::Center(Candidate)));
  if(Distance==DBL_MAX)continue;
  const FIntPoint ReadCell(Candidate.X*Turn,Candidate.Y*Turn);
  if(BestPoint!=INDEX_NONE && (Distance>BestDistance || (Distance==BestDistance &&
     (ReadCell.X>BestRead.X || (ReadCell.X==BestRead.X && ReadCell.Y>=BestRead.Y)))))continue;
  BestPoint=PointIndex;BestDistance=Distance;BestCell=Candidate;BestRead=ReadCell;
 }
 // An uncommitted selection stays stable without repeating pick events every second.
 if(Focus.Point==BestPoint && !Focus.Taken && !Focus.Committed)return;
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
 for(const auto& Point:Match->Objectives)if(Point.Owner==Team && Near(Point.Position))return true;
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

bool UBattleSquadTactics::PickSeizeExtended(int32 SID,const FBattleSquadState& S,const FVector& Here,const FIntPoint* Around,FIntPoint& Out)
{
 UpdateSeizeExtensions(S.Team);
 const double Now=Clock();const auto* Ended=SeizeEnded.Find(SID);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const auto& Focus=SeizePoints[S.Team];
 const int32 Turn=Match->AreaHome(S.Team).X>0?-1:1;
 TSet<FIntPoint> Candidates=SeizeTrialKnown[S.Team];
 if(BattleTactics::SectorSeizePoint() && Focus.Point!=INDEX_NONE)Candidates.Add(Focus.Cell);
 double Best=-DBL_MAX;FIntPoint BestRead=FIntPoint::ZeroValue;bool Found=false;
 for(const FIntPoint& Cell:Candidates)
 {
  if(!BattleSectors::Valid(Cell) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Cell)))continue;
  const bool Point=BattleTactics::SectorSeizePoint() && Focus.Point!=INDEX_NONE && Focus.Cell==Cell;
  int32 Count=0;const bool Counter=SeizeCounterCell(S.Team,Cell,&Count);
  if(Around){if(Cell==*Around || FMath::Abs(Cell.X-Around->X)>1 || FMath::Abs(Cell.Y-Around->Y)>1)continue;}
  else if(Cell==BattleSectors::Cell(Here) && !Point && !Counter)continue;
  if(const double* At=Ended?Ended->Find(Cell):nullptr;At && Now>=*At && Now-*At<30.)continue;
  if(!Point && !SeizeSectorKnown(S.Team,Cell,Now))continue;
  // Purpose-bound defenders are borrowed only for these two extensions, never for an ordinary sight target.
  if((Match->Area.HirePurpose.Contains(SID) && !Point && !Counter) || !SeizeGuardAllows(SID,S.Team,Cell))continue;
  const double Distance=FVector::Dist2D(Here,BattleSectors::Center(Cell));if(Distance>BattleTactics::SeekKnownRange())continue;
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
    if(Pair.Key!=SID && Pair.Value.EndAt<0. && (Pair.Value.Cell==Cell || SamePoint) && Other && Other->Team==S.Team &&
       Other->CommandStatus==EBattleCommandStatus::Active &&
       (Other->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) || SeizeSuspended.Contains(Pair.Key)))++Allies;
   }
  }
  if(Allies>=Limit)continue;
  bool Footing=false;
  for(int32 DX=-1;DX<=1 && !Footing;++DX)for(int32 DY=-1;DY<=1;++DY)
   if((DX || DY) && SeizeTrialOwn[S.Team].Contains(Cell+FIntPoint(DX,DY))){Footing=true;break;}
  const double Score=-Distance/5000.+(Allies>0?3.:0.)+(Footing?2.:0.)+(Point?6.:0.)+(Counter?5.:0.);
  const FIntPoint ReadCell(Cell.X*Turn,Cell.Y*Turn);
  if(Found && (Score<Best || (Score==Best && (ReadCell.X>BestRead.X || (ReadCell.X==BestRead.X && ReadCell.Y>=BestRead.Y)))))continue;
  Found=true;Best=Score;BestRead=ReadCell;Out=Cell;
 }
 return Found;
}

bool UBattleSquadTactics::BeginSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible,int32 Threats)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);
 EndSeize(SID);const double Now=Clock();
 if(const auto* Last=SeizeSectors.Find(SID);Last && Last->EndAt>=0. && Now-Last->EndAt<5.)return false;
 const auto* Tune=GetDefault<UBattleTuning>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(Visible || Threats!=0 || O.RecentDamage!=0 || S->SquadType!=BattleSquadType::Rifle ||
    O.Armed<FMath::CeilToInt(O.Living*.6f) || O.Living<=S->Members.Num()*Tune->SquadRetreatLivingRatio ||
    O.Health/FMath::Max(1.f,O.FullHealth(Tune->Health))<Tune->SquadRetreatHealthRatio || Match->SurpriseMoving(SID) || Match->RaidSneaking(SID))return false;
 const auto* Group=Match->Groups.Find(Match->GroupOf(SID));
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const FVector Here=Runtime->Position(S->LeaderUnitID);
 const bool Guard=S->bSectorMission && S->Order==EBattleOrder::Defend && S->TacticalPlan.Action==EBattleSquadTactic::Mission &&
  Runtime->SectorCellFor(S->Team,Here)==Runtime->SectorCellFor(S->Team,S->Destination) && O.Contacts.IsEmpty();
 if((Group && Group->Kind!=1) || (Match->Area.HirePurpose.Contains(SID) && !Guard) || Match->IntelPurpose.Contains(SID))return false;
 bool Base=false;if(BattleTactics::AreaStrategy().bEnabled)Match->AreaStrategyTarget(SID,S->Team,Base);if(Base)return false;
 FIntPoint Cell;if(!PickSeizeExtended(SID,*S,Here,nullptr,Cell))return false;
 return StartSeizeExtended(SID,Cell,TEXT("SEIZE SECTOR / MOVE TO ENEMY SECTOR"));
}

bool UBattleSquadTactics::SeizePointWaiting(int32 SID) const
{
 const auto* Extra=SeizeExtras.Find(SID);const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 if(!BattleTactics::SectorSeizePoint() || !Extra || Extra->Point==INDEX_NONE || !S)return false;
 if(BattleTactics::SectorSeizePointSupport()>0 && Extra->Support)return false;
 const auto& Focus=SeizePoints[S->Team];
 return Focus.Point==Extra->Point && Focus.GoAt<0. && BattleSectors::Valid(Focus.Assembly);
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
  Reason=TEXT("SEIZE SECTOR / ASSAULT POINT");
 }
 return BattleSectors::Center(Open.Cell);
}

bool UBattleSquadTactics::StartSeizeExtended(int32 SID,FIntPoint Cell,const TCHAR* SightReason)
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const double Now=Clock();auto& Focus=SeizePoints[S->Team];
 FSeizeExtra Extra;Extra.Counter=SeizeCounterCell(S->Team,Cell);
 FVector Goal=BattleSectors::Center(Cell);const TCHAR* Reason=SightReason;
 if(BattleTactics::SectorSeizePoint() && Focus.Point!=INDEX_NONE && Focus.Cell==Cell)
 {
  Extra.Point=Focus.Point;
  if(BattleTactics::SectorSeizePointSupport()>0)
  {Extra.FocusStart=Focus.Start;Extra.PointCell=Focus.Cell;}
  if(Focus.GoAt<0. && BattleSectors::Valid(Focus.Assembly)){Goal=BattleSectors::Center(Focus.Assembly);Reason=TEXT("SEIZE SECTOR / ASSEMBLE");}
  else Reason=TEXT("SEIZE SECTOR / ASSAULT POINT");
 }
 if(Extra.Point!=INDEX_NONE && BattleTactics::SectorSeizePointSupport()>0 &&
    Focus.AssaultJoined>=BattleTactics::SectorSeizePointMin() && Focus.SupportJoined<BattleTactics::SectorSeizePointSupport())
 {
  FIntPoint SupportCell=Focus.SupportCell;
  if(!BattleSectors::Valid(SupportCell) && !PickSeizeSupport(S->Team,SupportCell))return false;
  Extra.Support=true;Cell=SupportCell;Extra.Counter=SeizeCounterCell(S->Team,Cell);
  Goal=BattleSectors::Center(Cell);Reason=TEXT("SEIZE SECTOR / SUPPORT SECTOR");
 }
 if(!Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,Goal,Reason))return false;
 EndSeek(SID);
 FSeizeSector Open;Open.Cell=Cell;Open.Start=Now;Open.LastKnown=Now;SeizeSectors.Add(SID,Open);SeizeExtras.Add(SID,Extra);
 if(Extra.Point!=INDEX_NONE)
 {
  Focus.Committed=true;
  if(BattleTactics::SectorSeizePointSupport()>0)
  {
   if(Extra.Support){++Focus.SupportJoined;Focus.SupportCell=Cell;Event(TEXT("SeizePointSupportGo"));LogSeizePoint(S->Team,TEXT("support"),&Cell);}
   else ++Focus.AssaultJoined;
  }
  if(Extra.Support){} // Support leaves immediately and never opens the assault gate.
  else if(SeizePointWaiting(SID))Event(TEXT("SeizePointAssemble"));
  else if(Focus.GoAt<0.){Focus.GoAt=Now;Event(TEXT("SeizePointGo"));LogSeizePoint(S->Team,TEXT("go"));}
 }
 if(Extra.Counter)Event(TEXT("SeizeCounterGo"));
 Event(TEXT("SeizeGo"));int32 Known=0;SeizeSectorKnown(S->Team,Cell,Now,&Known);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE t=%.0f squad=%d team=%d cell=%d,%d dist=%.0f known=%d allies=%d kind=%s"),
  Match->Elapsed,SID,S->Team,Cell.X,Cell.Y,FVector::Dist2D(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID),BattleSectors::Center(Cell)),
  Known,SeizeSectorAllies(SID,S->Team,Cell),Extra.Support?TEXT("support"):Extra.Point!=INDEX_NONE?TEXT("point"):Extra.Counter?TEXT("counter"):TEXT("sight"));
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
  return Now-Open->Start>=360.?TEXT("SeizeEndPointTimeout"):nullptr;
 }
 const double Timeout=BattleTactics::SectorSeizeCounter() && Extra && Extra->Counter?300.:180.;
 if(Open->TakenAt<0. && Now-Open->Start>=Timeout)return TEXT("SeizeEndTimeout");
 if(Open->TakenAt>=0. && Now-Open->TakenAt>=BattleTactics::SectorSeizeHold()+90.)return TEXT("SeizeEndHeldLong");
 if(Open->EnterAt<0. && Now-Open->LastKnown>=60.)return TEXT("SeizeEndGone");
 return nullptr;
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
 const double Now=Clock();
 const FVector Here=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID);
 auto* Seize=SeizeSectors.Find(SID);
 // Point: 360 s overall; counter: 300 s before taking; sight: legacy deadlines.
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
 auto* Extra=SeizeExtras.Find(SID);
 const bool Point=BattleTactics::SectorSeizePoint()>0 && Extra && Extra->Point!=INDEX_NONE;
 const bool Support=Point && BattleTactics::SectorSeizePointSupport()>0 && Extra->Support;
 if(Support && (Extra->ReleasedAt>=0. || Extra->CapturedAt>=0.) && Seize->TakenAt<0.)
 {
  EndSeize(SID,TEXT("SeizeEndOther"));
  if(!ResumeMission(SID,TEXT("POINT SUPPORT OVER / RESUME MISSION"),nullptr))
   Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("POINT SUPPORT OVER / HOLD"));
  return true;
 }
 if(Point && !Support && Seize->EnterAt<0. && BattleSectors::Cell(Here)==Seize->Cell)
 {Seize->EnterAt=Now;Event(TEXT("SeizeEnter"));Event(TEXT("SeizePointEnter"));}
 if(Point && Extra->CapturedAt<0. && GetWorld()->GetSubsystem<UBattleMatch>()->Objectives.IsValidIndex(Extra->Point) &&
    GetWorld()->GetSubsystem<UBattleMatch>()->Objectives[Extra->Point].Owner==S->Team)Extra->CapturedAt=Now;
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
  if(Seize->TakenAt<0. && Seize->QuietSince>=0. && Now-Seize->QuietSince>=15. &&
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
  // Hold for public ownership (at most 120 s); then start the ordinary stay/next phase.
  double HoldFrom=Seize->TakenAt;
  if(Support)
  {
   if(Extra->ReleasedAt<0. && Extra->CapturedAt<0.)return true;
   HoldFrom=FMath::Max(Seize->TakenAt,Extra->ReleasedAt>=0.?Extra->ReleasedAt:Extra->CapturedAt);
  }
  else if(Point)
  {
   if(Extra->CapturedAt<0. && Now-Seize->TakenAt<120.)return true;
   HoldFrom=Extra->CapturedAt>=0.?FMath::Max(Seize->TakenAt,Extra->CapturedAt):Seize->TakenAt+120.;
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
