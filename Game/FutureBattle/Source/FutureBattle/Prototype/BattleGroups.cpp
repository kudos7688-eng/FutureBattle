// Squad groups, stage 1 (2026-11-18, user proposal: several squads act together as one group; ROADMAP 2026-11-18 "squad group layer").
// A decision layer between the team (which objectives matter) and the squad (its own fight): a group has members, one objective and a
// phase. Assemble: members gather at a rally point short of the objective. Advance: once most of them are there (or after
// GroupAssembleMax seconds) they go in together: infantry to the point, a tank to an overwatch post close behind them, a mortar team
// to its firing post. Hold: after the team observes the point as its own, the group stays GroupHoldSeconds, then takes the next one.
//  AI groups: trial -BattleAreaGroups=0/1 (UBattleTuning::bAreaGroups, default 0; the trial team with -BattleAreaTrialTeam): one attack
//   group per team from its AI-led squads (the enemy team; own squads delegated to the AI).
//  Player groups: the player forms a group from the selected squads and selects a whole group (ABattleController); orders then go to
//   all its squads as before. A player group whose living squads are all delegated to the AI is run like an AI group.
// Groups are not saved (a load drops them). Inputs: own squads' state and positions, observed ownership, fixed map positions.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleRanges.h"
#include "BattleTactics.h"
#include "NavigationSystem.h"
#include "FutureBattle.h"

bool BattleTactics::AreaGroups(int32 Team)
{
 static const bool bOn=[]{int32 V=GetDefault<UBattleTuning>()->bAreaGroups?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaGroups="),V);return V!=0;}();
 const int32 Trial=AreaStrategy().TrialTeam;
 return bOn&&(Trial<0||Trial==Team);
}

int32 UBattleMatch::GroupOf(int32 SquadID) const
{
 const int32* G=SquadGroup.Find(SquadID);
 return G&&Groups.Contains(*G)?*G:INDEX_NONE;
}

void UBattleMatch::DissolveGroup(int32 GroupID,const TCHAR* Why)
{
 const auto* G=Groups.Find(GroupID);if(!G)return;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_END group=%d team=%d squads=%d reason=%s time=%.1f"),GroupID,G->Team,G->Squads.Num(),Why,Elapsed);
 for(int32 SID:G->Squads)if(const int32* Of=SquadGroup.Find(SID);Of&&*Of==GroupID)SquadGroup.Remove(SID);
 Groups.Remove(GroupID);
}

int32 UBattleMatch::GroupFocus(int32 Team,const FVector& From) const
{
 // The group's objective: a point the team does not observe as its own, the nearest to From (the group's own position, or its home);
 // INDEX_NONE when the team observes every point as its own.
 int32 Best=INDEX_NONE;double BestD=DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I)
 {
  if(ObservedOwner(Team,I)==Team)continue;
  const double D=FVector::DistSquared2D(Objectives[I].Position,From);if(D<BestD){BestD=D;Best=I;}
 }
 return Best;
}

FVector UBattleMatch::GroupRally(int32 Team,int32 Point) const
{
 // GroupRallyBack (GroupRallyBackProfile with the range profile) short of the point on the own side, on walkable ground.
 if(!Objectives.IsValidIndex(Point))return AreaHome(Team);
 const auto* T=GetDefault<UBattleTuning>();
 const FVector P=Objectives[Point].Position,Home=AreaHome(Team);const double Length=FVector::Dist2D(Home,P);
 const double Back=FMath::Clamp(double(BattleRanges::Get().bActive?T->GroupRallyBackProfile:T->GroupRallyBack),0.,FMath::Max(0.,Length-3000.));
 FVector Rally=P+(Home-P).GetSafeNormal2D()*Back;
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){FNavLocation L;if(Nav->ProjectPointToNavigation(Rally,L,FVector(1500,1500,500)))Rally=L.Location;}
 return Rally;
}

int32 UBattleMatch::FormGroup(int32 Team,const TArray<int32>& Squads,bool bPlayer)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R||Team<0||Team>1)return INDEX_NONE;
 FBattleGroup G;G.Team=Team;G.bPlayer=bPlayer;
 for(int32 SID:Squads)if(const auto* S=Store->FindSquad(SID);S&&S->Team==Team&&S->CommandStatus==EBattleCommandStatus::Active)G.Squads.AddUnique(SID);
 if(G.Squads.Num()<(bPlayer?1:2))return INDEX_NONE;
 G.Squads.Sort();
 // A squad belongs to one group: leaving an old group may end it.
 for(int32 SID:G.Squads)if(const int32 Old=GroupOf(SID);Old!=INDEX_NONE)
 {
  auto& O=Groups.FindChecked(Old);O.Squads.Remove(SID);SquadGroup.Remove(SID);
  if(O.Squads.Num()<(O.bPlayer?1:2))DissolveGroup(Old,TEXT("members_left"));
 }
 G.ID=NextGroupID++;
 FVector Center=FVector::ZeroVector;for(int32 SID:G.Squads)Center+=R->Position(Store->FindSquad(SID)->LeaderUnitID);Center/=G.Squads.Num();
 G.Objective=bArea&&!Objectives.IsEmpty()?GroupFocus(Team,AreaHome(Team)):INDEX_NONE;
 G.Rally=G.Objective!=INDEX_NONE?GroupRally(Team,G.Objective):Center;G.Phase=0;G.PhaseAt=Elapsed;
 for(int32 SID:G.Squads)SquadGroup.Add(SID,G.ID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_FORM group=%d team=%d player=%d squads=%d objective=%d rally=%s time=%.1f"),G.ID,Team,bPlayer?1:0,G.Squads.Num(),G.Objective,*G.Rally.ToCompactString(),Elapsed);
 const int32 ID=G.ID;Groups.Add(ID,MoveTemp(G));
 return ID;
}

bool UBattleMatch::GroupAIRun(const FBattleGroup& G) const
{
 // The enemy team's groups always; an own group only while every active squad of it is delegated to the AI.
 if(G.Team!=0)return true;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();int32 Active=0;
 for(int32 SID:G.Squads)if(const auto* S=Store->FindSquad(SID);S&&S->CommandStatus==EBattleCommandStatus::Active){if(!S->bPlayerDelegated)return false;++Active;}
 return Active>0;
}

void UBattleMatch::UpdateGroups(int32 Team)
{
 if(!bArea||Phase!=EBattleMatchPhase::Combat||Objectives.IsEmpty()||Team<0||Team>1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto* T=GetDefault<UBattleTuning>();
 if(!Store||!R)return;
 const bool bStrategy=BattleTactics::AreaStrategy().bEnabled;
 TArray<int32> IDs;Groups.GetKeys(IDs);IDs.Sort();
 // 1. Members: stopped squads (leader lost) leave; a group below its minimum ends.
 for(int32 ID:IDs)
 {
  auto* G=Groups.Find(ID);if(!G||G->Team!=Team)continue;
  for(int32 I=G->Squads.Num()-1;I>=0;--I)
   if(const auto* S=Store->FindSquad(G->Squads[I]);!S||S->CommandStatus!=EBattleCommandStatus::Active){SquadGroup.Remove(G->Squads[I]);G->Squads.RemoveAt(I);}
  if(G->Squads.Num()<(G->bPlayer?1:2))DissolveGroup(ID,TEXT("members_lost"));
 }
 // 2. AI formation (trial): one attack group per team from its AI-led rifle squads outside the home-assault slots, the nearest to the
 //    rally point first, with one tank and one mortar team when the team has them.
 bool bHasAIGroup=false;for(const auto& P:Groups)bHasAIGroup|=P.Value.Team==Team&&!P.Value.bPlayer;
 if(BattleTactics::AreaGroups(Team)&&bStrategy&&!bHasAIGroup)
 {
  const int32 Focus=GroupFocus(Team,AreaHome(Team));
  if(Focus!=INDEX_NONE)
  {
   const FVector Rally=GroupRally(Team,Focus);
   struct FFree{int32 SID;uint8 Type;double D;};TArray<FFree> Free;
   for(const auto& P:Store->GetSquads())
   {
    const auto& S=P.Value;
    if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(P.Key)!=INDEX_NONE)continue;
    if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key))continue;
    if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Tank&&S.SquadType!=BattleSquadType::Mortar)continue;
    bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase)continue;
    Free.Add({P.Key,S.SquadType,FVector::DistSquared2D(R->Position(S.LeaderUnitID),Rally)});
   }
   Free.Sort([](const FFree& A,const FFree& B){return A.D!=B.D?A.D<B.D:A.SID<B.SID;});
   TArray<int32> Pick;int32 Rifles=0;bool bTank=false,bMortar=false;const int32 Want=FMath::Clamp(T->GroupRifleSquads,2,6);
   for(const auto& F:Free)
   {
    if(F.Type==BattleSquadType::Rifle){if(Rifles<Want){Pick.Add(F.SID);++Rifles;}}
    else if(F.Type==BattleSquadType::Tank){if(!bTank){Pick.Add(F.SID);bTank=true;}}
    else if(!bMortar){Pick.Add(F.SID);bMortar=true;}
   }
   if(Rifles>=2)FormGroup(Team,Pick,false);
  }
 }
 // 3. Phases of the groups the AI runs.
 Groups.GetKeys(IDs);IDs.Sort();
 for(int32 ID:IDs)
 {
  auto* G=Groups.Find(ID);if(!G||G->Team!=Team||!GroupAIRun(*G))continue;
  auto Enter=[&](uint8 To,const TCHAR* Why)
  {
   G->Phase=To;G->PhaseAt=Elapsed;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_PHASE group=%d team=%d phase=%d objective=%d squads=%d reason=%s time=%.1f"),ID,Team,int32(To),G->Objective,G->Squads.Num(),Why,Elapsed);
  };
  // Squads away at home for supply do not hold the group up.
  int32 Present=0,Ready=0;FVector Center=FVector::ZeroVector;
  for(int32 SID:G->Squads)
  {
   const auto* S=Store->FindSquad(SID);if(!S)continue;
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
   const FVector P=R->Position(S->LeaderUnitID);++Present;Center+=P;
   Ready+=FVector::Dist2D(P,G->Rally)<=FMath::Max(500.f,T->GroupRallyRadius);
  }
  if(Present)Center/=Present;
  if(!Objectives.IsValidIndex(G->Objective))
  {
   const int32 Next=GroupFocus(Team,Present?Center:AreaHome(Team));
   if(Next==INDEX_NONE){if(!G->bPlayer){DissolveGroup(ID,TEXT("no_objective"));}continue;}
   G->Objective=Next;G->Rally=GroupRally(Team,Next);Enter(0,TEXT("new_objective"));continue;
  }
  const bool bOwn=ObservedOwner(Team,G->Objective)==Team;
  if(G->Phase==0)
  {
   if(bOwn){Enter(2,TEXT("already_own"));continue;}
   if(Present>=2&&Ready>=FMath::CeilToInt(Present*FMath::Clamp(T->GroupReadyShare,.1f,1.f)))Enter(1,TEXT("assembled"));
   else if(Elapsed-G->PhaseAt>=FMath::Max(0.f,T->GroupAssembleMax))Enter(1,TEXT("assemble_timeout"));
  }
  else if(G->Phase==1){if(bOwn)Enter(2,TEXT("point_taken"));}
  else if(Elapsed-G->PhaseAt>=FMath::Max(0.f,T->GroupHoldSeconds)||!bOwn)
  {
   // The next point: the nearest one the team does not hold, from where the group stands. It gathers again short of it.
   const int32 Next=bOwn?GroupFocus(Team,Present?Center:AreaHome(Team)):G->Objective;
   if(Next==INDEX_NONE){G->PhaseAt=Elapsed;continue;}
   G->Objective=Next;G->Rally=GroupRally(Team,Next);Enter(0,bOwn?TEXT("next_objective"):TEXT("point_lost"));
  }
 }
}

bool UBattleMatch::GroupOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const int32 ID=GroupOf(SquadID);const auto* G=ID!=INDEX_NONE?Groups.Find(ID):nullptr;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);
 if(!G||!S||G->Team!=Team||!Objectives.IsValidIndex(G->Objective)||!GroupAIRun(*G))return false;
 const bool bMortar=BattleSquadType::IsArtillery(S->SquadType);
 if(G->Phase==0)
 {
  // Gather at the rally point; a mortar team keeps its standoff from known enemies.
  Center=bMortar?AreaArtilleryPost(Team,SquadID,G->Rally):G->Rally;Order=EBattleOrder::Defend;return true;
 }
 const FVector Point=Objectives[G->Objective].Position;const bool bOwn=ObservedOwner(Team,G->Objective)==Team;
 Order=bOwn?EBattleOrder::Defend:EBattleOrder::AttackMove;
 if(S->SquadType==BattleSquadType::Tank)
 {
  // Close behind its infantry instead of the lone overwatch distance; a post near the current mission is kept.
  Center=AreaTankPost(Team,G->Objective,GetDefault<UBattleTuning>()->GroupTankBack);
  if(S->bHasMission&&FVector::Dist2D(S->MissionDestination,Center)<2500.)Center=S->MissionDestination;
 }
 else Center=bMortar?AreaArtilleryPost(Team,SquadID,Point):Point;
 return true;
}
