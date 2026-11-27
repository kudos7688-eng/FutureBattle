#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleCover.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "FutureBattle.h"

// All entry points are also gated at their call sites. Enemy information is KnownSpots or the squad observation.
bool UBattleSquadTactics::SectorDefendEnabled(int32 Team) const
{
 if(BattleTactics::SectorDefend()<=0)return false;
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 return IsST2Enabled() && Team>=0 && Team<2 && Match && Match->bArea &&
  BattleFieldLayout::Of(GetWorld()).bSectors &&
  (BattleTactics::SectorDefendTeam()<0 || BattleTactics::SectorDefendTeam()==Team);
}

bool UBattleSquadTactics::DefendingSector(int32 SID) const
{
 if(BattleTactics::SectorDefend()<=0)return false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);
 if(!S || !SectorDefendEnabled(S->Team) || S->CommandStatus!=EBattleCommandStatus::Active ||
    (S->Team==0 && !S->bPlayerDelegated))return false;
 bool HasLiving=false;
 for(int32 ID:S->Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive())
 {HasLiving=true;if(Unit->Action==EBattleAction::DirectControl)return false;}
 if(!HasLiving)return false;
 if(S->TacticalPlan.Action==EBattleSquadTactic::Mission)return S->bSectorMission && S->Order==EBattleOrder::Defend;
 const auto* Defence=SectorDefends.Find(SID);
 return Defence && Defence->BackAt>=0. && Defence->OrderRevision==S->OrderRevision &&
  S->TacticalPlan.bAutonomous && S->TacticalPlan.Action==EBattleSquadTactic::Engage &&
  S->TacticalPlan.Reason.StartsWith(TEXT("SECTOR DEFEND")) &&
  (S->Order==EBattleOrder::Move || S->Order==EBattleOrder::Defend);
}

bool UBattleSquadTactics::SectorDefendPlace(int32 SID,const FVector& Place) const
{
 if(BattleTactics::SectorDefend()<=0 || !DefendingSector(SID))return true;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto* Defence=SectorDefends.Find(SID);
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 return Runtime->SectorCellFor(S->Team,Place)==(Defence?Defence->Cell:Runtime->SectorCellFor(S->Team,S->Destination));
}

bool UBattleSquadTactics::SectorDefendMoving(int32 SID) const
{
 if(BattleTactics::SectorDefend()<=0 || !DefendingSector(SID))return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 return S->Order==EBattleOrder::Move;
}

void UBattleSquadTactics::LogSectorDefend(int32 SID,const FSectorDefend& Defence,const TCHAR* What) const
{
 if(BattleTactics::SectorDefend()<=0)return;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_DEFEND t=%.0f squad=%d team=%d cell=%d,%d what=%s from=%d,%d secs=%.0f"),
  GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,SID,S?S->Team:-1,Defence.Cell.X,Defence.Cell.Y,What,
  Defence.From.X,Defence.From.Y,Clock()-Defence.Start);
}

void UBattleSquadTactics::EndSectorDefend(int32 SID)
{
 if(BattleTactics::SectorDefend()<=0)return;
 if(const auto* Defence=SectorDefends.Find(SID))
 {LogSectorDefend(SID,*Defence,TEXT("end"));Event(TEXT("DefendEnd"));SectorDefends.Remove(SID);}
}

void UBattleSquadTactics::SectorDefendReplan(int32 SID)
{
 if(BattleTactics::SectorDefend()<=0 || !DefendingSector(SID))return;
 if(auto* Defence=SectorDefends.Find(SID))
 {Defence->LastReplan=Clock();Event(TEXT("DefendReplan"));}
}

bool UBattleSquadTactics::StepSectorDefend(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 if(BattleTactics::SectorDefend()<=0 || !DefendingSector(SID))return false;
 if(!Visible)for(const auto& Contact:O.Contacts)if(Contact.Value.bVisible && !Contact.Value.bShared){Visible=true;break;}
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* S=Store->Squads.Find(SID);
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const double Now=Clock();
 auto* Defence=SectorDefends.Find(SID);
 if(Defence && Defence->OrderRevision!=S->OrderRevision){EndSectorDefend(SID);Defence=nullptr;}
 if(!Defence && Visible && O.Armed>0)
 {
  FSectorDefend Initial;Initial.Cell=Runtime->SectorCellFor(S->Team,S->Destination);Initial.From=Initial.Cell;
  Initial.Start=Now;Initial.LastSeen=Now;Initial.OrderRevision=S->OrderRevision;
  Defence=&SectorDefends.Add(SID,Initial);Event(TEXT("DefendStay"));LogSectorDefend(SID,*Defence,TEXT("stay"));
 }
 if(Defence)
 {
  if(Visible)Defence->LastSeen=Now;
  if(const auto* History=DamageHistory.Find(SID);History && !History->IsEmpty())
   Defence->LastDamage=FMath::Max(Defence->LastDamage,History->Last().Key+1.);
  if(Defence->BackAt>=0.)
  {
   S->TacticalPlan.Remaining=60.f;
   int32 Inside=0;
   for(int32 ID:S->Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive() &&
      Runtime->SectorCellFor(S->Team,Runtime->Position(ID))==Defence->Cell)++Inside;
   if(Inside==O.Living && O.Living>0 && Defence->ArrivedAt<0.)
   {
    // Move suppresses combat stops during the entire deployment, just as ordinary retreat does.
    if(Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Defend,BattleSectors::Center(Defence->Cell),TEXT("SECTOR DEFEND / HOLD BACK")))
    {Defence->ArrivedAt=Now;Defence->OrderRevision=S->OrderRevision;}
   }
   if(Defence->ArrivedAt>=0. && O.RecentDamage==0 &&
      Now-FMath::Max(Defence->ArrivedAt,Defence->LastDamage)>=BattleTactics::SectorDefendBack())
   {
    const FSectorDefend Finished=*Defence;
    if(ResumeMission(SID,TEXT("SECTOR DEFEND / RETURN TO MISSION"),nullptr))
    {Event(TEXT("DefendReturn"));LogSectorDefend(SID,Finished,TEXT("return"));SectorDefends.Remove(SID);}
   }
   return true;
  }
  if(Now-Defence->LastSeen>=30.){EndSectorDefend(SID);return false;}
  return true;
 }
 // Quiet (no contact for 30 s, or none yet): the ordinary evaluation goes on, so SEEK/JOIN/SEIZE may still take the squad.
 return false;
}

bool UBattleSquadTactics::FallBackSector(int32 SID,const FBattleSquadObservation& O)
{
 if(BattleTactics::SectorDefend()<2 || !DefendingSector(SID))return false;
 const double Now=Clock();auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);
 const auto* Previous=SectorDefends.Find(SID);
 if(Previous && Previous->OrderRevision!=S->OrderRevision){EndSectorDefend(SID);Previous=nullptr;}
 // The repeated damage window cannot immediately consume several sectors. Health/ammunition emergencies never enter here.
 if(Previous && Previous->BackAt>=0. && Now-Previous->BackAt<10.)return true;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const FIntPoint Current=Previous?Previous->Cell:Runtime->SectorCellFor(S->Team,S->Destination);
 const FVector Home=Match->AreaHome(S->Team);const double CurrentDistance=FVector::DistSquared2D(BattleSectors::Center(Current),Home);
 RefreshKnownSpots(S->Team);
 TSet<FIntPoint> EnemyCells,OwnCells;
 for(const auto& Spot:KnownSpots[S->Team])
  if(Now>=Spot.Value.Seen && Now-Spot.Value.Seen<=BattleTactics::SeekKnownSeconds())
   EnemyCells.Add(Runtime->SectorCellFor(S->Team,Spot.Value.At));
 for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==S->Team)
  for(int32 ID:Pair.Value.Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive())
   OwnCells.Add(Runtime->SectorCellFor(S->Team,Runtime->Position(ID)));
 // GetSlots is the already built arrival-cover table. No BuildSlots/nav work in this decision.
 if(Now<DefendSlotsAt || Now-DefendSlotsAt>=1.)
 {
  DefendSlotsAt=Now;DefendSlotCounts.Reset();
  for(const FVector& Slot:GetWorld()->GetSubsystem<UBattleCover>()->GetSlots())
   ++DefendSlotCounts.FindOrAdd(BattleSectors::Cell(Slot));
 }
 const int32 Turn=Home.X>0?-1:1;
 FIntPoint BestCell=Current,BestRead=FIntPoint::ZeroValue;int32 BestScore=-1;double BestDistance=DBL_MAX;
 for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
 {
  if(!DX && !DY)continue;
  const FIntPoint Candidate=Current+FIntPoint(DX,DY);
  if(!BattleSectors::Valid(Candidate) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Candidate)) ||
     EnemyCells.Contains(Candidate))continue;
  const double Distance=FVector::DistSquared2D(BattleSectors::Center(Candidate),Home);
  if(Distance>=CurrentDistance)continue;
  const int32 Score=DefendSlotCounts.FindRef(Candidate)+(OwnCells.Contains(Candidate)?2:0);
  const FIntPoint Read(Candidate.X*Turn,Candidate.Y*Turn);
  if(Score<BestScore || (Score==BestScore && (Distance>BestDistance ||
     (Distance==BestDistance && (Read.X>BestRead.X || (Read.X==BestRead.X && Read.Y>=BestRead.Y))))))continue;
  BestCell=Candidate;BestScore=Score;BestDistance=Distance;BestRead=Read;
 }
 if(BestScore<0){Event(TEXT("DefendFallBackNone"));return false;}
 FSectorDefend Next;
 if(Previous)Next=*Previous;
 else {Next.Cell=Current;Next.From=Current;Next.Start=Now;Next.LastSeen=Now;}
 if(!Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Move,BattleSectors::Center(BestCell),TEXT("SECTOR DEFEND / FALL BACK")))
 {Event(TEXT("DefendFallBackNone"));return false;}
 if(BattleTactics::SectorSeize()>0)EndSeize(SID);
 Next.From=Current;Next.Cell=BestCell;Next.BackAt=Now;Next.ArrivedAt=-1.;if(O.RecentDamage>0)Next.LastDamage=Now;Next.OrderRevision=S->OrderRevision;
 SectorDefends.Add(SID,Next);Event(TEXT("DefendFallBack"));LogSectorDefend(SID,Next,TEXT("back"));
 return true;
}

void UBattleSquadTactics::SeizePlanChanged(int32 SID)
{
 if(BattleTactics::SectorSeize()<=0)return;
 const auto* Open=SeizeSectors.Find(SID);if(!Open || Open->EndAt>=0.)return;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);if(!S)return;
 const auto Action=S->TacticalPlan.Action;
 if(Action==EBattleSquadTactic::Support || Action==EBattleSquadTactic::Flank){SeizeSuspended.Add(SID);return;}
 if(Action==EBattleSquadTactic::Engage && S->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR"))){SeizeSuspended.Remove(SID);return;}
 if(SeizeSuspended.Contains(SID) && (Action==EBattleSquadTactic::Engage || Action==EBattleSquadTactic::Mission))return;
 EndSeize(SID);
}

bool UBattleSquadTactics::ResumeSeizeSector(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 if(BattleTactics::SectorSeize()<=0)return false;
 auto* Open=SeizeSectors.Find(SID);if(!Open || Open->EndAt>=0.)return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 if(!S || !SectorSeizeEnabled(S->Team))return false;
 const auto Action=S->TacticalPlan.Action;
 if(Action==EBattleSquadTactic::Support || Action==EBattleSquadTactic::Flank)
 {
  SeizeSuspended.Add(SID);
  if(SeizeSectorKnown(S->Team,Open->Cell,Clock()))Open->LastKnown=Clock();
  Open->QuietSince=-1.;return false;
 }
 if(!SeizeSuspended.Contains(SID))return false;
 if(Action!=EBattleSquadTactic::Engage && Action!=EBattleSquadTactic::Mission){EndSeize(SID);return false;}
 const double Now=Clock();const auto* Tune=GetDefault<UBattleTuning>();
 if(SeizeSectorKnown(S->Team,Open->Cell,Now))Open->LastKnown=Now;
 const bool Healthy=O.RecentDamage==0 && O.Armed>0 && O.Armed>=FMath::CeilToInt(O.Living*.6f) &&
  O.Living>S->Members.Num()*Tune->SquadRetreatLivingRatio &&
  O.Health/FMath::Max(1.f,O.FullHealth(Tune->Health))>=Tune->SquadRetreatHealthRatio;
 if(!Healthy || (Open->TakenAt<0. && Now-Open->Start>=180.) || Now-Open->LastKnown>=60. ||
    (Open->TakenAt>=0. && Now-Open->TakenAt>=BattleTactics::SectorSeizeHold()+90.))
 {EndSeize(SID);SeizeSuspended.Remove(SID);return false;}
 const FVector Goal=Visible?GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID):BattleSectors::Center(Open->Cell);
 if(!Apply(SID,EBattleSquadTactic::Engage,Visible?EBattleOrder::Hold:EBattleOrder::AttackMove,Goal,
    Visible?TEXT("SEIZE SECTOR / FIRE"):TEXT("SEIZE SECTOR / MOVE ON")))
 {EndSeize(SID);SeizeSuspended.Remove(SID);return false;}
 if(Visible)Open->LastSeen=Now;
 Open->QuietSince=-1.;SeizeSuspended.Remove(SID);Event(TEXT("SeizeResume"));return true;
}

void UBattleSquadTactics::CheckSectorTrials()
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(BattleTactics::SectorDefend()>0)
 {
  TArray<int32> Ended;
  for(const auto& Pair:SectorDefends)
  {
   const auto* S=Store->FindSquad(Pair.Key);
   if(!DefendingSector(Pair.Key) || !S || S->OrderRevision!=Pair.Value.OrderRevision)Ended.Add(Pair.Key);
  }
  for(int32 SID:Ended)EndSectorDefend(SID);
 }
 if(BattleTactics::SectorSeize()>0)
  for(const auto& Pair:SeizeSectors)if(Pair.Value.EndAt<0.)
  {
   const auto* S=Store->FindSquad(Pair.Key);
   if(!S || S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated) ||
      S->TacticalPlan.Reason==TEXT("EXPLICIT ORDER"))EndSeize(Pair.Key);
   else SeizePlanChanged(Pair.Key);
  }
}

bool UBattleLeaderRuntime::SectorDefendUnitPlace(int32 ID,const FVector& Place) const
{
 if(BattleTactics::SectorDefend()<=0)return true;
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!Unit || Unit->bSoldierOrder)return true;
 const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 if(!Tactics->DefendingSector(Unit->SquadID))return true;
 return !Tactics->SectorDefendMoving(Unit->SquadID) && Tactics->SectorDefendPlace(Unit->SquadID,Place);
}

void UBattleLeaderRuntime::EnforceSectorDefend(int32 ID,double Now)
{
 if(BattleTactics::SectorDefend()<=0)return;
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 if(!Unit || Unit->bSoldierOrder || !Tactics->DefendingSector(Unit->SquadID))return;
 if(const auto* Episode=KnownMoves.Find(ID);Episode &&
    (!SectorDefendUnitPlace(ID,Episode->Goal) || !SectorDefendUnitPlace(ID,Episode->Peek)))
  EndKnownMove(ID,Now,TEXT("known_move_cancel_mission"));
 if(Unit->CoverPhase && (!SectorDefendUnitPlace(ID,Unit->CoverAnchor) || !SectorDefendUnitPlace(ID,Unit->CoverPeek)))CancelCoverAction(ID);
 if(const auto* Approach=CoverApproaches.Find(ID);Approach && !SectorDefendUnitPlace(ID,Approach->Goal))
 {CoverApproaches.Remove(ID);SoldierPaths.Remove(ID);GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);}
 if(const auto* DefendFire=FirePositions.Find(ID);DefendFire && !SectorDefendUnitPlace(ID,*DefendFire))FirePositions.Remove(ID);
 if(auto* Plan=Plans.Find(Unit->SquadID))
 {
  Plan->bPinned=false;Plan->bCloseIn=false;
  if(Tactics->SectorDefendMoving(Unit->SquadID))Plan->bCoverStop=false;
 }
}
