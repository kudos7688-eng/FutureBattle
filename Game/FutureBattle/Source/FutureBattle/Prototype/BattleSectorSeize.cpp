#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "FutureBattle.h"

bool UBattleSquadTactics::SectorSeizeEnabled(int32 Team) const
{
 if(BattleTactics::SectorSeize()<=0)return false;
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 return BattleTactics::SeekKnown()>0 && Team>=0 && Team<2 && Match && Match->bArea &&
  BattleFieldLayout::Of(GetWorld()).bSectors &&
  (BattleTactics::SectorSeizeTeam()<0 || BattleTactics::SectorSeizeTeam()==Team);
}

bool UBattleSquadTactics::SeizingSector(int32 SID) const
{
 if(BattleTactics::SectorSeize()<=0)return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 if(!S || !SectorSeizeEnabled(S->Team))return false;
 const auto* Seize=SeizeSectors.Find(SID);
 if(BattleTactics::SectorSeizePoint()>0 && Seize && Seize->EndAt<0. && S->TacticalPlan.Action==EBattleSquadTactic::Engage &&
    S->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")))
  if(const auto* ClearDetail=SeizeExtras.Find(SID);ClearDetail && !ClearDetail->Support && ClearDetail->ClearAt>=0. && ClearDetail->CapturedAt<0.)return true;
 return Seize && Seize->EndAt<0. && S->TacticalPlan.Action==EBattleSquadTactic::Engage &&
  (S->Order==EBattleOrder::AttackMove || S->Order==EBattleOrder::Defend) &&
  S->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")) &&
  !S->TacticalPlan.Reason.Contains(TEXT("/ FIRE"));
}

bool UBattleSquadTactics::SeizeSectorKnown(int32 Team,FIntPoint Cell,double Now,int32* Count) const
{
 if(BattleTactics::SectorSeize()<=0)return false;
 if(Count)*Count=0;
 if(!SectorSeizeEnabled(Team))return false;
 int32 Found=0;
 for(const auto& Spot:KnownSpots[Team])
  if(Now>=Spot.Value.Seen && Now-Spot.Value.Seen<=BattleTactics::SeekKnownSeconds() &&
     BattleSectors::Cell(Spot.Value.At)==Cell)++Found;
 if(Count)*Count=Found;
 return Found>0;
}

int32 UBattleSquadTactics::SeizeSectorAllies(int32 SID,int32 Team,FIntPoint Cell) const
{
 if(BattleTactics::SectorSeize()<=0)return 0;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 int32 Allies=0;
 for(const auto& Other:SeizeSectors)
 {
  if(Other.Key==SID || Other.Value.EndAt>=0. || Other.Value.Cell!=Cell)continue;
  const auto* Squad=Store->FindSquad(Other.Key);
  if(Squad && Squad->Team==Team && Squad->CommandStatus==EBattleCommandStatus::Active &&
     Squad->TacticalPlan.Action==EBattleSquadTactic::Engage &&
     Squad->TacticalPlan.Reason.StartsWith(TEXT("SEIZE SECTOR")))++Allies;
 }
 return Allies;
}

bool UBattleSquadTactics::PickSeizeSector(int32 SID,const FBattleSquadState& S,const FVector& Here,
 double Now,const FIntPoint* Around,FIntPoint& Out)
{
 if(BattleTactics::SectorSeize()<=0 || !SectorSeizeEnabled(S.Team))return false;
 if(BattleTactics::SectorSeizePoint()>0 || BattleTactics::SectorSeizeCounter()>0)
  return PickSeizeExtended(SID,S,Here,Around,Out);
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 // Only our soldiers' true positions; enemy candidates come exclusively from KnownSpots.
 if(Now<SeizeCellsAt[S.Team] || Now-SeizeCellsAt[S.Team]>=1.)
 {
  SeizeCellsAt[S.Team]=Now;SeizeOwnCells[S.Team].Reset();SeizeKnownCells[S.Team].Reset();
  for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==S.Team)
   for(int32 ID:Pair.Value.Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive())
    if(const FIntPoint OwnCell=BattleSectors::Cell(Runtime->Position(ID));BattleSectors::Valid(OwnCell))
     SeizeOwnCells[S.Team].Add(OwnCell);
  for(const auto& Spot:KnownSpots[S.Team])
   if(Now>=Spot.Value.Seen && Now-Spot.Value.Seen<=BattleTactics::SeekKnownSeconds())
    SeizeKnownCells[S.Team].Add(BattleSectors::Cell(Spot.Value.At));
 }
 const auto* Ended=SeizeEnded.Find(SID);
 const FIntPoint HereCell=BattleSectors::Cell(Here);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const int32 Turn=Match->AreaHome(S.Team).X>0?-1:1;
 double Best=-DBL_MAX;FIntPoint BestRead=FIntPoint::ZeroValue;bool Found=false;
 for(const FIntPoint& Cell:SeizeKnownCells[S.Team])
 {
  if(!BattleSectors::Valid(Cell) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Cell)))continue;
  if(Around)
  {
   if(Cell==*Around || FMath::Abs(Cell.X-Around->X)>1 || FMath::Abs(Cell.Y-Around->Y)>1)continue;
  }
  else if(Cell==HereCell)continue;
  if(const double* At=Ended?Ended->Find(Cell):nullptr;At && Now>=*At && Now-*At<30.)continue;
  if(!SeizeSectorKnown(S.Team,Cell,Now))continue;
  const double Distance=FVector::Dist2D(Here,BattleSectors::Center(Cell));
  if(Distance>BattleTactics::SeekKnownRange())continue;
  const int32 Allies=SeizeSectorAllies(SID,S.Team,Cell);
  if(Allies>=BattleTactics::SectorSeizeMax())continue;
  bool Footing=false;
  for(int32 DX=-1;DX<=1 && !Footing;++DX)for(int32 DY=-1;DY<=1;++DY)
   if((DX || DY) && SeizeOwnCells[S.Team].Contains(Cell+FIntPoint(DX,DY))){Footing=true;break;}
  const double Score=-Distance/5000.+(Allies>0?3.:0.)+(Footing?2.:0.);
  const FIntPoint Read(Cell.X*Turn,Cell.Y*Turn);
  if(Found && (Score<Best || (Score==Best && (Read.X>BestRead.X || (Read.X==BestRead.X && Read.Y>=BestRead.Y)))))continue;
  Found=true;Best=Score;BestRead=Read;Out=Cell;
 }
 return Found;
}

void UBattleSquadTactics::LogSeizeSector(int32 SID,const FSeizeSector& Seize,const TCHAR* Why) const
{
 if(BattleTactics::SectorSeize()<=0)return;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE_END t=%.0f squad=%d team=%d cell=%d,%d why=%s entered=%d taken=%d secs=%.0f"),
  Match->Elapsed,SID,S?S->Team:-1,Seize.Cell.X,Seize.Cell.Y,Why,
  Seize.EnterAt>=0.?1:0,Seize.TakenAt>=0.?1:0,Clock()-Seize.Start);
}

void UBattleSquadTactics::EndSeize(int32 SID,const TCHAR* Why)
{
 if(BattleTactics::SectorSeize()<=0)return;
 if(auto* Seize=SeizeSectors.Find(SID);Seize && Seize->EndAt<0.)
 {
  Seize->EndAt=Clock();SeizeEnded.FindOrAdd(SID).Add(Seize->Cell,Seize->EndAt);
  Event(Why);LogSeizeSector(SID,*Seize,Why);
 }
}

bool UBattleSquadTactics::BeginSeizeSector(int32 SID,const FBattleSquadObservation& O,bool Visible,int32 Threats)
{
 if(BattleTactics::SectorSeize()<=0)return false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const auto* S=Store->FindSquad(SID);
 if(!S || !SectorSeizeEnabled(S->Team))return false;
 if(BattleTactics::SectorSeizePoint()>0 || BattleTactics::SectorSeizeCounter()>0)
  return BeginSeizeExtended(SID,O,Visible,Threats);
 EndSeize(SID);
 const double Now=Clock();
 if(const auto* Last=SeizeSectors.Find(SID);Last && Last->EndAt>=0. && Now-Last->EndAt<5.)return false;
 const auto* T=GetDefault<UBattleTuning>();
 const float FullHP=FMath::Max(1.f,O.FullHealth(T->Health));
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(Visible || Threats!=0 || O.RecentDamage!=0 || S->SquadType!=BattleSquadType::Rifle ||
    O.Armed<FMath::CeilToInt(O.Living*.6f) || O.Living<=S->Members.Num()*T->SquadRetreatLivingRatio ||
    O.Health/FullHP<T->SquadRetreatHealthRatio || Match->SurpriseMoving(SID) || Match->RaidSneaking(SID))return false;
 const auto* Group=Match->Groups.Find(Match->GroupOf(SID));
 if((Group && Group->Kind!=1) || Match->Area.HirePurpose.Contains(SID) || Match->IntelPurpose.Contains(SID))return false;
 bool Base=false;if(BattleTactics::AreaStrategy().bEnabled)Match->AreaStrategyTarget(SID,S->Team,Base);
 if(Base)return false;
 const FVector Here=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID);
 FIntPoint Cell;
 if(!PickSeizeSector(SID,*S,Here,Now,nullptr,Cell))return false;
 const FVector Center=BattleSectors::Center(Cell);
 if(!Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,Center,TEXT("SEIZE SECTOR / MOVE TO ENEMY SECTOR")))return false;
 EndSeek(SID);
 FSeizeSector Seize;Seize.Cell=Cell;Seize.Start=Now;Seize.LastKnown=Now;SeizeSectors.Add(SID,Seize);
 Event(TEXT("SeizeGo"));
 int32 Known=0;SeizeSectorKnown(S->Team,Cell,Now,&Known);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_SEIZE t=%.0f squad=%d team=%d cell=%d,%d dist=%.0f known=%d allies=%d"),
  Match->Elapsed,SID,S->Team,Cell.X,Cell.Y,FVector::Dist2D(Here,Center),Known,SeizeSectorAllies(SID,S->Team,Cell));
 return true;
}

bool UBattleSquadTactics::StepSeizeSector(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 if(BattleTactics::SectorSeize()<=0)return false;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* S=Store->Squads.Find(SID);
 if(!S || !SectorSeizeEnabled(S->Team))return false;
 if(BattleTactics::SectorSeizePoint()>0 || BattleTactics::SectorSeizeCounter()>0)
  return StepSeizeExtended(SID,O,Visible);
 auto& Plan=S->TacticalPlan;
 if(Plan.Action!=EBattleSquadTactic::Engage || !Plan.Reason.StartsWith(TEXT("SEIZE SECTOR")))return false;
 const double Now=Clock();
 const FVector Here=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID);
 auto* Seize=SeizeSectors.Find(SID);
 // Check deadlines even during contact, so repeated firing cannot extend the 180 s limit.
 const bool Known=Seize && SeizeSectorKnown(S->Team,Seize->Cell,Now);
 if(Seize)
 {
  if(Known){Seize->LastKnown=Now;Seize->QuietSince=-1.;}
  if(BattleSectors::Cell(Here)!=Seize->Cell)Seize->QuietSince=-1.;
 }
 const TCHAR* End=!Seize || Seize->EndAt>=0.?TEXT("SeizeEndLost"):
  Seize->TakenAt<0. && Now-Seize->Start>=180.?TEXT("SeizeEndTimeout"):
  // After the sector was taken a renewed fire fight may not keep the squad for good (Codex review 2026-11-27).
  Seize->TakenAt>=0. && Now-Seize->TakenAt>=BattleTactics::SectorSeizeHold()+90.?TEXT("SeizeEndHeldLong"):
  Seize->EnterAt<0. && Now-Seize->LastKnown>=60.?TEXT("SeizeEndGone"):nullptr;
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
 if(Visible && O.Armed>0)
 {
  if(Plan.Reason!=TEXT("SEIZE SECTOR / FIRE") &&
     Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE SECTOR / FIRE")))
   Event(TEXT("SeizeContact"));
  Seize->QuietSince=-1.;Seize->LastSeen=Now;
  return true;
 }
 const FVector Center=BattleSectors::Center(Seize->Cell);
 if(Plan.Reason==TEXT("SEIZE SECTOR / FIRE"))
 {
  // Four seconds since a member last saw an enemy himself, and no damage (SafeSeconds also waits for shared and remembered
  // contacts to fade, which kept the squad in place; Codex review 2026-11-27).
  if(Now-Seize->LastSeen>=4. && O.RecentDamage==0 && Plan.Age>=3 &&
     Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,Center,TEXT("SEIZE SECTOR / MOVE ON")))
   Event(TEXT("SeizeAgain"));
  return true;
 }
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
  }
 }
 else Seize->QuietSince=-1.;
 if(Seize->TakenAt>=0.)
 {
  // A contact after capture also resumes the move; hold again once back inside.
  if(BattleSectors::Cell(Here)==Seize->Cell && Plan.Reason!=TEXT("SEIZE SECTOR / HOLD TAKEN"))
   Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SEIZE SECTOR / HOLD TAKEN"));
  FIntPoint Next;
  if(PickSeizeSector(SID,*S,Here,Now,&Seize->Cell,Next) &&
     Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::AttackMove,BattleSectors::Center(Next),TEXT("SEIZE SECTOR / NEXT")))
  {
   // Close the previous sector too, preventing an immediate loop back.
   EndSeize(SID,TEXT("SeizeEndNext"));
   FSeizeSector Following;Following.Cell=Next;Following.Start=Now;Following.LastKnown=Now;
   SeizeSectors.Add(SID,Following);Event(TEXT("SeizeNext"));return true;
  }
  if(Now-Seize->TakenAt>=BattleTactics::SectorSeizeHold())
  {
   EndSeize(SID,TEXT("SeizeEndHeld"));
   if(!ResumeMission(SID,TEXT("SECTOR TAKEN / RESUME MISSION"),nullptr))
    Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("SECTOR TAKEN / HOLD"));
  }
 }
 return true;
}
