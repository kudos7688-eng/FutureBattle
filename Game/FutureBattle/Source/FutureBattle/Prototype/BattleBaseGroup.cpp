// Home-assault group (trial -BattleBaseGroup=1, default 0; user decision 2026-11-26 "本拠地攻撃を群に").
// Before: the squads of the home-assault slots (AreaStrategyTarget's base share) went for the enemy home one by one, each choosing a
// flank road by its own number and the enemies its team saw at that moment; a squad that came home for replacements often went out
// on the other flank, and two thirds of the squads in no group were these lone squads (BATTLE_FREE_SQUAD, map I seed 7, 11-26).
// Now, on maps with sectors, they form one group per team (Kind 3), shown in the army list as "本拠地攻撃":
//  - The flank is chosen once, when the group forms: the edge row (one row in from the map edge) with fewer known enemies ahead of the
//    own home (observed information only; ties take the same own row for both teams). Squads that join later, or come back from the
//    home after replacements, take the same flank.
//  - Phase 0: the squads gather at the rally, a sector of that row two columns out from the own home, until two thirds of the squads
//    present (those not walking home for replacements) are within RallyRadius, or AssembleMax seconds pass.
//  - Phase 1: each squad walks along that row two sectors at a time, then for the enemy home once it is within EnemyHomeReach columns
//    of it. Short legs keep it off the road network (RouteStep leaves goals within 150 m alone).
// A squad leaves when it stops being a home-assault squad (its slot changes); the group ends below two squads (GroupMinimum).
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleSectors.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace BattleBaseGroupValues
{
 constexpr float AssembleMax=60.f;     // s in phase 0 at most
 constexpr double RallyRadius=6000.;   // cm: a squad this near the rally counts as gathered
 constexpr int32 RallyColumns=2;       // the rally's column, counted from the own home's column
 constexpr int32 LegColumns=2;         // columns per leg along the row in phase 1
 constexpr int32 EnemyHomeReach=3;     // columns from the enemy home where the squads turn for it
}

bool UBattleMatch::BaseGroupOn() const
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleBaseGroup="),V);return V!=0;}();
 return On&&bArea&&BattleFieldLayout::Of(GetWorld()).bSectors;
}

int32 UBattleMatch::BaseGroupRowCell(int32 Team,int32 Column,int32 Row,FIntPoint& Out) const
{
 // The passable sector of the column nearest to the row, moving inwards (rivers count as passable: their centres are dry banks).
 constexpr int32 Rows=BattleSectors::Rows;
 const int32 Step=Row<Rows/2?1:-1;
 for(int32 D=0;D<Rows/2;++D)
 {
  const int32 Y=Row+Step*D;const FIntPoint C=FrontCell(Team,FMath::Clamp(Column,0,BattleSectors::Columns-1),Y);
  if(!BattleSectors::Valid(C))continue;
  const char Kind=BattleFieldLayout::SectorKind(GetWorld(),C);
  if(BattleSectors::Impassable(Kind)&&Kind!='R')continue;
  Out=C;return Y;
 }
 return INDEX_NONE;
}

void UBattleMatch::UpdateBaseGroup(int32 Team)
{
 using namespace BattleBaseGroupValues;
 int32 ID=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==3)ID=P.Key;
 if(!BaseGroupOn()){if(ID!=INDEX_NONE)DissolveGroup(ID,TEXT("base_group_off"));return;}
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R||Team<0||Team>1)return;
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 const bool bLow=AreaHome(Team).X<0;
 auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 auto RowOf=[&](const FIntPoint& C){return bLow?C.Y:Rows-1-C.Y;};
 const int32 HomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(Team))),0,Cols-1);
 // The home-assault squads of the team the AI runs (the player's own squads only when delegated), as UpdateTeamOrders sends them.
 TArray<int32> Base;
 for(const auto& P:Store->GetSquads())
 {
  const auto& S=P.Value;
  if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated))continue;
  if(const int32 Old=GroupOf(P.Key);Old!=INDEX_NONE&&Old!=ID)continue;
  // A squad the player put into this group by hand (AssignArmy pins it) stays whatever its slot (Codex review 2026-11-26).
  if(ArmyPinned.Contains(P.Key)){if(ID!=INDEX_NONE&&GroupOf(P.Key)==ID)Base.Add(P.Key);continue;}
  if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key))continue;
  if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Tank)continue;
  if(BattleTactics::BaseGarrison()&&IsBaseGarrison(P.Key))continue;
  bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);
  if(bBase)Base.Add(P.Key);
 }
 if(ID==INDEX_NONE)
 {
  if(Base.Num()<2)return;
  // The edge row with fewer known enemies ahead of the own home (own rows 1 or Rows-2).
  int32 Ahead[2]={0,0};
  for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C)&&ColOf(C)>HomeCol+1)
  {if(RowOf(C)<=2)++Ahead[0];if(RowOf(C)>=Rows-3)++Ahead[1];}
  const int32 Want=Ahead[1]<Ahead[0]?Rows-2:1;
  FIntPoint RallyCell;const int32 Row=BaseGroupRowCell(Team,HomeCol+RallyColumns,Want,RallyCell);if(Row==INDEX_NONE)return;
  ID=FormGroup(Team,Base,false);auto* G=Groups.Find(ID);if(!G)return;
  G->Kind=3;G->Objective=INDEX_NONE;G->Phase=0;G->PhaseAt=Elapsed;G->SurpriseRow=Row;
  G->Rally=BattleSectors::Center(RallyCell);G->Rally.Z=AreaHome(Team).Z;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GROUP group=%d team=%d form squads=%d row=%d side=%s rally=%c%d ahead=%d,%d time=%.1f"),
   ID,Team,G->Squads.Num(),Row,G->Rally.Y>0?TEXT("north"):TEXT("south"),TCHAR('A'+RallyCell.X),1+RallyCell.Y,Ahead[0],Ahead[1],Elapsed);
  return;
 }
 auto& G=Groups.FindChecked(ID);
 // Joiners: new home-assault squads (bought, or back from another errand) take the group's flank.
 TArray<int32> Joined;for(int32 SID:Base)if(!G.Squads.Contains(SID)){G.Squads.Add(SID);SquadGroup.Add(SID,ID);Joined.Add(SID);}
 // Leavers: squads that are no longer home-assault squads go back to the team's other orders.
 TArray<int32> Left;for(int32 SID:G.Squads)if(!Base.Contains(SID))Left.Add(SID);
 for(int32 SID:Left){G.Squads.Remove(SID);SquadGroup.Remove(SID);}
 if(!Joined.IsEmpty()||!Left.IsEmpty())
 {
  G.Squads.Sort();
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GROUP group=%d team=%d members joined=%d left=%d squads=%d time=%.1f"),ID,Team,Joined.Num(),Left.Num(),G.Squads.Num(),Elapsed);
  if(G.Squads.Num()<2){DissolveGroup(ID,TEXT("members_left"));return;}
 }
 if(G.Phase==0)
 {
  int32 Present=0,Ready=0;
  for(int32 SID:G.Squads)if(const auto* S=Store->FindSquad(SID);S&&Store->Find(S->LeaderUnitID))
  {
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
   ++Present;Ready+=FVector::Dist2D(R->Position(S->LeaderUnitID),G.Rally)<=RallyRadius;
  }
  if((Present>0&&Ready*3>=Present*2)||Elapsed-G.PhaseAt>=AssembleMax)
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GROUP group=%d team=%d go ready=%d/%d waited=%.0f time=%.1f"),ID,Team,Ready,Present,Elapsed-G.PhaseAt,Elapsed);
   G.Phase=1;G.PhaseAt=Elapsed;
  }
 }
}

bool UBattleMatch::BaseGroupOrder(int32 GroupID,int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 using namespace BattleBaseGroupValues;
 const auto* G=Groups.Find(GroupID);const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!G||G->Kind!=3||!S||!R||!Store->Find(S->LeaderUnitID))return false;
 Order=EBattleOrder::AttackMove;
 // A squad falling back, regrouping or walking home for replacements keeps its mission: a new goal every update (the leg below moves
 // with the leader) cut those short, so depleted squads never reached the home (map I seeds 7/19: "MEMBERS" fall-backs 6/2 -> 29/56,
 // three of five squads down to one to three soldiers for the rest of the match).
 if(S->bHasMission&&S->TacticalPlan.Action!=EBattleSquadTactic::Mission){Center=S->MissionDestination;return true;}
 if(G->Phase==0){Center=G->Rally;return true;}
 constexpr int32 Cols=BattleSectors::Columns;
 const bool bLow=AreaHome(Team).X<0;auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 const int32 EnemyHomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(1-Team))),0,Cols-1);
 const FVector Leader=R->Position(S->LeaderUnitID);const int32 Col=ColOf(BattleSectors::Cell(Leader));
 // The current leg stands until the leader is within a sector of it, when it is still on the group's row and ahead of the leader.
 if(S->bHasMission&&FVector::Dist2D(Leader,S->MissionDestination)>BattleSectors::Size)
 {
  const FIntPoint M=BattleSectors::Cell(S->MissionDestination);
  if(BattleSectors::Valid(M)&&ColOf(M)>Col&&FMath::Abs(M.Y-FrontCell(Team,0,G->SurpriseRow).Y)<=BattleSectors::Rows/2-1&&ColOf(M)<EnemyHomeCol-EnemyHomeReach+LegColumns)
  {Center=S->MissionDestination;return true;}
 }
 FIntPoint Cell;
 if(Col>=EnemyHomeCol-EnemyHomeReach||BaseGroupRowCell(Team,FMath::Min(Col+LegColumns,EnemyHomeCol-EnemyHomeReach),G->SurpriseRow,Cell)==INDEX_NONE)
 {Center=AreaHome(1-Team)*(25./27.);return true;}
 Center=BattleSectors::Center(Cell);Center.Z=G->Rally.Z;
 return true;
}
