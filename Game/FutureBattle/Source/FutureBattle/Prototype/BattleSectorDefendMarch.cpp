#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "BattleCover.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

int32 BattleTactics::SectorDefendMarch()
{
 static const int32 Mode=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorDefendMarch="),Value);return FMath::Clamp(Value,0,1);}();return Mode;
}
int32 BattleTactics::SectorDefendMarchTeam()
{
 static const int32 Team=[](){int32 Value=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorDefendMarchTeam="),Value);return FMath::Clamp(Value,-1,1);}();return Team;
}

bool UBattleSquadTactics::SectorMarchCandidate(int32 SID,FIntPoint& Cell,bool& Arrived) const
{
 if(!BattleTactics::SectorDefendMarch())return false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!IsST2Enabled() || !S || S->Team<0 || S->Team>1 || !Match->bArea || !BattleFieldLayout::Of(GetWorld()).bSectors ||
    (BattleTactics::SectorDefendMarchTeam()>=0 && BattleTactics::SectorDefendMarchTeam()!=S->Team) ||
    S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated) || !S->bSectorMission ||
    S->TacticalPlan.Action!=EBattleSquadTactic::Mission || S->Order==EBattleOrder::Hold)return false;
 const auto* Group=Match->Groups.Find(Match->GroupOf(SID));
 if((Group && Group->Kind!=1) || Match->SurpriseMoving(SID) || Match->RaidSneaking(SID))return false;
 const bool* Base=SectorMarchBaseSlots.Find(SID);if(!Base || *Base)return false;
 Cell=Runtime->SectorCellFor(S->Team,S->Destination);if(!BattleSectors::Valid(Cell))return false;
 int32 Living=0,Inside=0;
 for(int32 ID:S->Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive())
 {
  if(Unit->Action==EBattleAction::DirectControl)return false;
  ++Living;if(Runtime->SectorCellFor(S->Team,Runtime->Position(ID))==Cell)++Inside;
 }
 Arrived=S->Order==EBattleOrder::Defend || Inside*2>Living;
 return Living>0;
}

bool UBattleSquadTactics::SectorMarchEnemy(int32 Team,FIntPoint Cell) const
{
 return Team>=0 && Team<2 && SectorMarchKnown[Team].Contains(Cell);
}

bool UBattleSquadTactics::MarchingSector(int32 SID) const
{
 if(!BattleTactics::SectorDefendMarch())return false;
 FIntPoint Cell;bool Arrived=false;if(!SectorMarchCandidate(SID,Cell,Arrived) || Arrived)return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);const auto* March=SectorMarches.Find(SID);
 return !(March && March->Revision==S->MissionRevision && March->Cell==Cell && Clock()<March->BlockedUntil) && !SectorMarchEnemy(S->Team,Cell);
}

void UBattleSquadTactics::LogSectorMarch(int32 SID,FIntPoint Cell,const TCHAR* What,double Distance) const
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_MARCH t=%.0f squad=%d team=%d to=%d,%d what=%s dist=%.0f"),
  GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,SID,S?S->Team:-1,Cell.X,Cell.Y,What,Distance);
}

bool UBattleSquadTactics::StepSectorMarch(int32 SID,const FBattleSquadObservation& O,bool Visible)
{
 if(!BattleTactics::SectorDefendMarch())return false;
 const auto* MarchSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
 if(!MarchSquad || MarchSquad->Team<0 || MarchSquad->Team>1)return false;
 // Strategic allocation and sight aggregation run on squad evaluation, never per soldier's fire/cover check.
 bool Base=false;
 if(BattleTactics::AreaStrategy().bEnabled)GetWorld()->GetSubsystem<UBattleMatch>()->AreaStrategyTarget(SID,MarchSquad->Team,Base);
 SectorMarchBaseSlots.Add(SID,Base);
 const int32 Team=MarchSquad->Team;
 if(Clock()<SectorMarchKnownAt[Team] || Clock()-SectorMarchKnownAt[Team]>=1.)
 {
  SectorMarchKnownAt[Team]=Clock();SectorMarchKnown[Team].Reset();
  const auto* MarchRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  for(const auto& Spot:KnownSpots[Team])if(Clock()>=Spot.Value.Seen && Clock()-Spot.Value.Seen<=BattleTactics::SeekKnownSeconds())
   SectorMarchKnown[Team].Add(MarchRuntime->SectorCellFor(Team,Spot.Value.At));
 }
 FIntPoint Cell;bool Arrived=false;
 if(!SectorMarchCandidate(SID,Cell,Arrived))
 {
  // Keep the 30 s exclusion across the ordinary Engage/Hold plan it deliberately permits.
  if(auto* Last=SectorMarches.Find(SID))
  {
   if(Last->Revision!=MarchSquad->MissionRevision)SectorMarches.Remove(SID);
   else if(Clock()>=Last->BlockedUntil)
   {
    Last->Since=Clock();
    Last->Distance=FVector::Dist2D(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(MarchSquad->LeaderUnitID),BattleSectors::Center(Last->Cell));
   }
  }
  return false;
 }
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* S=Store->Squads.Find(SID);
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const double Now=Clock();
 const double Distance=FVector::Dist2D(Runtime->Position(S->LeaderUnitID),BattleSectors::Center(Cell));
 auto* Previous=SectorMarches.Find(SID);
 if(Previous && (Previous->Cell!=Cell || Previous->Revision!=S->MissionRevision)){SectorMarches.Remove(SID);Previous=nullptr;}
 if(Arrived)
 {
  if(!Previous)return false; // Already deployed before this trial ever followed the march.
  if(Previous && Previous->Kept){Event(TEXT("MarchArrive"));LogSectorMarch(SID,Cell,TEXT("arrive"),Distance);}
  SectorMarches.Remove(SID);
  // Same in-place conversion as Think, also when an observed enemy is outside the post.
  if(S->Order!=EBattleOrder::Defend)
  {
   S->Order=EBattleOrder::Defend;
   for(int32 ID:S->Members)if(const auto* Unit=Store->Find(ID);Unit && Unit->IsAlive())Store->Units.FindChecked(ID).Order=EBattleOrder::Defend;
  }
  return false;
 }
 if(SectorMarchEnemy(S->Team,Cell))
 {
  auto& March=SectorMarches.FindOrAdd(SID);
  March.Cell=Cell;March.Revision=S->MissionRevision;
  if(!March.EnemyNoted){Event(TEXT("MarchEnemyAtPost"));LogSectorMarch(SID,Cell,TEXT("enemy_at_post"),Distance);March.EnemyNoted=true;}
  March.Since=Now;March.Distance=Distance;return false;
 }
 auto& March=SectorMarches.FindOrAdd(SID);
 if(!Previous){March.Cell=Cell;March.Revision=S->MissionRevision;March.Since=Now;March.Distance=Distance;}
 if(March.EnemyNoted){March.EnemyNoted=false;March.Since=Now;March.Distance=Distance;}
 if(Now<March.BlockedUntil)return false;
 if(March.BlockedUntil>=0.){March.BlockedUntil=-1.;March.Since=Now;March.Distance=Distance;}
 if(Now-March.Since>=20.)
 {
  const bool Stuck=March.Distance-Distance<1000. && O.RecentDamage>0;
  March.Since=Now;March.Distance=Distance;
  if(Stuck){March.BlockedUntil=Now+30.;Event(TEXT("MarchStuck"));LogSectorMarch(SID,Cell,TEXT("stuck"),Distance);return false;}
 }
 if(!Visible)for(const auto& Contact:O.Contacts)if(Contact.Value.bVisible && !Contact.Value.bShared){Visible=true;break;}
 const double* LastKeep=SectorMarchKeepAt.Find(SID);
 if(Visible && (!LastKeep || Now-*LastKeep>=30.))
 {March.Kept=true;SectorMarchKeepAt.Add(SID,Now);Event(TEXT("MarchKeep"));LogSectorMarch(SID,Cell,TEXT("keep"),Distance);}
 return true; // Mission and its route remain; no JOIN/SEEK/SEIZE diversion on the way to this post.
}

bool UBattleLeaderRuntime::SectorMarchUnit(int32 ID) const
{
 if(!BattleTactics::SectorDefendMarch())return false;
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 return Unit && Unit->IsAlive() && !Unit->bSoldierOrder && Unit->Action!=EBattleAction::DirectControl &&
  GetWorld()->GetSubsystem<UBattleSquadTactics>()->MarchingSector(Unit->SquadID);
}

void UBattleLeaderRuntime::EnforceSectorMarch(int32 ID,double Now)
{
 if(!SectorMarchUnit(ID))return;
 const auto* Unit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(KnownMoves.Contains(ID))EndKnownMove(ID,Now,TEXT("known_move_cancel_sector_march"));
 if(Unit->CoverPhase)CancelCoverAction(ID);
 if(CoverApproaches.Contains(ID)){CoverApproaches.Remove(ID);SoldierPaths.Remove(ID);GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);}
 FirePositions.Remove(ID);NoCoverProneUntil.Remove(ID);
 if(auto* Plan=Plans.Find(Unit->SquadID))
 {Plan->bCoverStop=false;Plan->bPinned=false;Plan->bCloseIn=false;Plan->BoundGroup=-1;}
}
