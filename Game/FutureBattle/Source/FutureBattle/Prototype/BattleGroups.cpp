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

// Stage 3 values (2026-11-18, first values; to move into UBattleTuning once settled).
namespace BattleGroupValues
{
 constexpr double CommitRatio=1.2;   // advance only with this many times the living soldiers of the enemies known at the point
 constexpr float WaitMax=120.f;      // seconds a gathered group waits for better odds before it goes into reserve
 constexpr float ReserveMin=45.f;    // seconds a group stays in reserve at an own point before it attacks again
 constexpr double DeepRatio=1.3;     // after a stall, another point only when it is not this much farther from the own home
 constexpr double StageMax=2.5;      // an own point within this x the rally distance of the objective is the rally point
 constexpr double LossAbort=.6;      // an advance ends when the group's living soldiers fall below this share of those it began with
 constexpr double ReadyStrength=.8;  // a group in reserve attacks again only with this share of its full numbers
 constexpr float OpeningSeconds=120.f; // a group formed this early goes straight for the first contested point (no gathering)
 constexpr double OwnSideRatio=.75;  // a point nearer the own home than this x its distance to the enemy home is an own-side point
 constexpr double ArriveRadius=10000.; // an advance has arrived when a squad of the group is this near the point (the stall clock starts)
 constexpr float TravelMax=300.f;      // an advance that has not arrived after this long is given up
 constexpr float AvoidCap=3600.f;    // a failed point is avoided GroupAvoidSeconds x 2^(failures in a row - 1), up to this
}

bool BattleTactics::DefendEngage(int32 Team)
{
 static const bool bOn=[]{int32 V=GetDefault<UBattleTuning>()->bDefendEngage?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleDefendEngage="),V);return V!=0;}();
 const int32 Trial=AreaStrategy().TrialTeam;
 return bOn&&(Trial<0||Trial==Team);
}

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

int32 UBattleMatch::GroupFocus(int32 Team,const FVector& From,int32 Avoid) const
{
 // The group's objective: a point the team does not observe as its own, the nearest to From (the group's own position, or its home);
 // INDEX_NONE when the team observes every point as its own.
 int32 Best=INDEX_NONE;double BestD=DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I)
 {
  if(ObservedOwner(Team,I)==Team||I==Avoid)continue;
  const double D=FVector::DistSquared2D(Objectives[I].Position,From);if(D<BestD){BestD=D;Best=I;}
 }
 // Only the avoided point is left: better that than nothing.
 if(Best==INDEX_NONE&&Objectives.IsValidIndex(Avoid)&&ObservedOwner(Team,Avoid)!=Team)Best=Avoid;
 return Best;
}

FVector UBattleMatch::GroupSupportPost(int32 Team,int32 Point) const
{
 // GroupSupportBack x the rifle engagement distance short of the point on the own side (within range of the point, outside its centre).
 if(!Objectives.IsValidIndex(Point))return AreaHome(Team);
 const auto* T=GetDefault<UBattleTuning>();
 const FVector P=Objectives[Point].Position,Home=AreaHome(Team);const double Length=FVector::Dist2D(Home,P);
 const double Range=BattleRanges::Get().bActive?BattleRanges::Engage(EBattleWeapon::Carbine):T->EffectiveWeaponRange();
 FVector Post=P+(Home-P).GetSafeNormal2D()*FMath::Clamp(double(T->GroupSupportBack)*Range,0.,FMath::Max(0.,Length-3000.));
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){FNavLocation L;if(Nav->ProjectPointToNavigation(Post,L,FVector(1000,1000,500)))Post=L.Location;}
 return Post;
}

uint8 UBattleMatch::GroupRole(const FBattleGroup& G,int32 SquadID) const
{
 if(const uint8* Role=G.Roles.Find(SquadID))return *Role;
 // A squad that joined after the roles were given: by its type.
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);const uint8 Type=S?S->SquadType:BattleSquadType::Rifle;
 return Type==BattleSquadType::Tank?2:BattleSquadType::IsArtillery(Type)?3:Type==BattleSquadType::AntiTank?4:Type==BattleSquadType::Heavy?1:Type==BattleSquadType::Rifle?0:255;
}

void UBattleMatch::AssignGroupRoles(FBattleGroup& G) const
{
 // Own squads' type and living numbers only. Rifle squads assault; with three or more the weakest supports. Heavy infantry supports.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 G.Roles.Reset();
 struct FRifle{int32 SID;int32 Living;};TArray<FRifle> Rifles;
 for(int32 SID:G.Squads)
 {
  const auto* S=Store->FindSquad(SID);if(!S)continue;
  if(S->SquadType!=BattleSquadType::Rifle){FBattleGroup None;G.Roles.Add(SID,GroupRole(None,SID));continue;}
  int32 Living=0;for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())++Living;
  Rifles.Add({SID,Living});
 }
 Rifles.Sort([](const FRifle& A,const FRifle& B){return A.Living!=B.Living?A.Living>B.Living:A.SID<B.SID;});
 for(int32 I=0;I<Rifles.Num();++I)G.Roles.Add(Rifles[I].SID,Rifles.Num()>=3&&I==Rifles.Num()-1?1:0);
}

FVector UBattleMatch::GroupRally(int32 Team,int32 Point) const
{
 // GroupRallyBack (GroupRallyBackProfile with the range profile) short of the point on the own side, on walkable ground.
 if(!Objectives.IsValidIndex(Point))return AreaHome(Team);
 const auto* T=GetDefault<UBattleTuning>();
 const FVector P=Objectives[Point].Position,Home=AreaHome(Team);const double Length=FVector::Dist2D(Home,P);
 const double Back=FMath::Clamp(double(BattleRanges::Get().bActive?T->GroupRallyBackProfile:T->GroupRallyBack),0.,FMath::Max(0.,Length-3000.));
 FVector Rally=P+(Home-P).GetSafeNormal2D()*Back;
 // An own point (observed) nearer the own home than the objective and close to it: gather there, where the squads also hold ground.
 {
  int32 Stage=INDEX_NONE;double StageD=Back*BattleGroupValues::StageMax;
  for(int32 I=0;I<Objectives.Num();++I)
  {
   if(I==Point||ObservedOwner(Team,I)!=Team||FVector::Dist2D(Objectives[I].Position,Home)>=Length)continue;
   const double D=FVector::Dist2D(Objectives[I].Position,P);if(D<StageD){StageD=D;Stage=I;}
  }
  if(Stage!=INDEX_NONE)Rally=Objectives[Stage].Position;
 }
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

int32 UBattleMatch::GroupLiving(const FBattleGroup& G,int32* Full) const
{
 // Living soldiers of the group's squads on foot (vehicles do not count), and their full number.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();int32 Living=0,All=0;
 for(int32 SID:G.Squads)if(const auto* S=Store->FindSquad(SID);S&&!BattleSquadType::IsVehicle(S->SquadType))
 {All+=S->Members.Num();for(int32 UID:S->Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive())++Living;}
 if(Full)*Full=All;
 return Living;
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
 int32 AIGroup=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&!P.Value.bPlayer)AIGroup=AIGroup==INDEX_NONE?P.Key:FMath::Min(AIGroup,P.Key);
 if(BattleTactics::AreaGroups(Team)&&bStrategy)
 {
  const int32 Focus=AIGroup!=INDEX_NONE?Groups[AIGroup].Objective:GroupFocus(Team,AreaHome(Team));
  if(Focus!=INDEX_NONE||AIGroup!=INDEX_NONE)
  {
   const FVector Rally=AIGroup!=INDEX_NONE?Groups[AIGroup].Rally:GroupRally(Team,Focus);
   struct FFree{int32 SID;uint8 Type;double D;};TArray<FFree> Free;
   for(const auto& P:Store->GetSquads())
   {
    const auto& S=P.Value;
    if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(P.Key)!=INDEX_NONE)continue;
    if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key))continue;
    if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Tank&&S.SquadType!=BattleSquadType::Mortar&&S.SquadType!=BattleSquadType::AntiTank)continue;
    bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase)continue;
    Free.Add({P.Key,S.SquadType,FVector::DistSquared2D(R->Position(S.LeaderUnitID),Rally)});
   }
   Free.Sort([](const FFree& A,const FFree& B){return A.D!=B.D?A.D<B.D:A.SID<B.SID;});
   // What the group already has (a new group: nothing), then the nearest free squads up to its numbers.
   TArray<int32> Pick;int32 Rifles=0;bool bTank=false,bMortar=false,bAT=false;const int32 Want=FMath::Clamp(T->GroupRifleSquads,2,6);
   if(AIGroup!=INDEX_NONE)for(int32 SID:Groups[AIGroup].Squads)if(const auto* S=Store->FindSquad(SID))
   {Rifles+=S->SquadType==BattleSquadType::Rifle;bTank|=S->SquadType==BattleSquadType::Tank;bMortar|=S->SquadType==BattleSquadType::Mortar;bAT|=S->SquadType==BattleSquadType::AntiTank;}
   for(const auto& F:Free)
   {
    if(F.Type==BattleSquadType::Rifle){if(Rifles<Want){Pick.Add(F.SID);++Rifles;}}
    else if(F.Type==BattleSquadType::Tank){if(!bTank){Pick.Add(F.SID);bTank=true;}}
    else if(F.Type==BattleSquadType::Mortar){if(!bMortar){Pick.Add(F.SID);bMortar=true;}}
    else if(!bAT){Pick.Add(F.SID);bAT=true;}
   }
   if(AIGroup==INDEX_NONE){if(Rifles>=2)FormGroup(Team,Pick,false);}
   else if(Pick.Num())
   {
    auto& G=Groups[AIGroup];
    for(int32 SID:Pick){G.Squads.Add(SID);SquadGroup.Add(SID,AIGroup);}
    G.Squads.Sort();
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_JOIN group=%d team=%d joined=%d squads=%d time=%.1f"),AIGroup,Team,Pick.Num(),G.Squads.Num(),Elapsed);
   }
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
   if(To==1){AssignGroupRoles(*G);G->StartLiving=GroupLiving(*G);G->bArrived=false;}
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
  // Stage 3: what the group decides as a whole. Own squads' numbers, observed ownership, known enemies (AreaIntelView).
  const auto& V=AreaIntelView(Team);const FVector Home=AreaHome(Team),EnemyHome=AreaHome(1-Team);
  int32 Full=0;const int32 Living=GroupLiving(*G,&Full);
  // A failed attack is remembered: the point is avoided longer each time the group fails at it in a row.
  auto Failed=[&]()
  {
   if(G->StallPoint==G->Objective)++G->Stalls;else{G->StallPoint=G->Objective;G->Stalls=1;}
   G->Avoid=G->Objective;
   G->AvoidUntil=Elapsed+FMath::Min(BattleGroupValues::AvoidCap,FMath::Max(0.f,T->GroupAvoidSeconds)*float(1<<FMath::Min(G->Stalls-1,5)));
  };
  // The next point to attack: not observed as own, the nearest to the own home; not the avoided one while it is avoided, and
  // not one much deeper than it (a group that failed at the front does not go for the enemy's rear).
  auto AttackPoint=[&]()->int32
  {
   const bool bAvoid=Elapsed<G->AvoidUntil&&Objectives.IsValidIndex(G->Avoid);
   const double Limit=bAvoid?FVector::Dist2D(Objectives[G->Avoid].Position,Home)*BattleGroupValues::DeepRatio:DBL_MAX;
   int32 Best=INDEX_NONE;double BestD=DBL_MAX;
   for(int32 I=0;I<Objectives.Num();++I)
   {
    if(ObservedOwner(Team,I)==Team||(bAvoid&&I==G->Avoid))continue;
    const double D=FVector::Dist2D(Objectives[I].Position,Home);if(D>Limit)continue;
    if(D<BestD){BestD=D;Best=I;}
   }
   return Best;
  };
  // The own point to stand at in reserve: the one with the largest known enemy surplus, else the own point nearest the enemy home.
  auto ReservePoint=[&]()->int32
  {
   const int32 Threat=AreaIntelThreat(Team);if(Threat!=INDEX_NONE&&Threat>=0)return Threat;
   int32 Best=INDEX_NONE;double BestD=DBL_MAX;
   for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)==Team){const double D=FVector::Dist2D(Objectives[I].Position,EnemyHome);if(D<BestD){BestD=D;Best=I;}}
   return Best;
  };
  auto Attack=[&](int32 Point,const TCHAR* Why){G->Objective=Point;G->Rally=GroupRally(Team,Point);Enter(0,Why);};
  auto Reserve=[&](const TCHAR* Why)->bool
  {
   const int32 Point=ReservePoint();if(Point==INDEX_NONE)return false;
   G->Objective=Point;G->Rally=Objectives[Point].Position;Enter(3,Why);return true;
  };
  // Opening (stage 3c): the first minutes decide who takes the contested points, and gathering first costs about 90 s. A group formed
  // early skips the own-side points (the team's other squads take those) and goes straight for the nearest point that is not on
  // its own side. Fixed map positions and observed ownership only.
  if(!G->bPlayer&&!G->bOpened)
  {
   G->bOpened=true;
   if(Elapsed<BattleGroupValues::OpeningSeconds)
   {
    int32 Best=INDEX_NONE;double BestD=DBL_MAX;
    for(int32 I=0;I<Objectives.Num();++I)
    {
     const FVector P=Objectives[I].Position;const double D=FVector::Dist2D(P,Home);
     if(ObservedOwner(Team,I)==Team||D<FVector::Dist2D(P,EnemyHome)*BattleGroupValues::OwnSideRatio)continue;
     if(D<BestD){BestD=D;Best=I;}
    }
    if(Best!=INDEX_NONE){G->Objective=Best;G->Rally=GroupRally(Team,Best);Enter(1,TEXT("opening_rush"));continue;}
   }
  }
  if(!Objectives.IsValidIndex(G->Objective))
  {
   const int32 Next=AttackPoint();
   if(Next!=INDEX_NONE){Attack(Next,TEXT("new_objective"));continue;}
   if(!Reserve(TEXT("no_attack_point"))&&!G->bPlayer)DissolveGroup(ID,TEXT("no_objective"));
   continue;
  }
  const bool bOwn=ObservedOwner(Team,G->Objective)==Team;
  if(G->Phase==0)
  {
   if(bOwn){Enter(2,TEXT("already_own"));continue;}
   const bool bAssembled=Present>=2&&Ready>=FMath::CeilToInt(Present*FMath::Clamp(T->GroupReadyShare,.1f,1.f));
   const float Waited=Elapsed-G->PhaseAt;
   if(!bAssembled&&Waited<FMath::Max(0.f,T->GroupAssembleMax))continue;
   // Commit: go in only with enough soldiers for the enemies the team knows of at the point; otherwise wait for more, then stand back.
   const int32 Known=V.EnemyAt.IsValidIndex(G->Objective)?V.EnemyAt[G->Objective]:0;
   if(Living>=Known*BattleGroupValues::CommitRatio)Enter(1,bAssembled?TEXT("assembled"):TEXT("assemble_timeout"));
   else if(Waited>=FMath::Max(T->GroupAssembleMax,BattleGroupValues::WaitMax))
   {
    Failed();
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_HOLD_BACK group=%d team=%d objective=%d living=%d known=%d time=%.1f"),ID,Team,G->Objective,Living,Known,Elapsed);
    if(!Reserve(TEXT("outnumbered")))Enter(1,TEXT("no_reserve_point"));
   }
  }
  else if(G->Phase==1)
  {
   // The stall clock runs from the arrival at the point, not from the decision to go (the walk may take longer than the clock).
   if(!G->bArrived)
   {
    bool bNear=false;
    for(int32 SID:G->Squads)if(const auto* S=Store->FindSquad(SID))bNear|=FVector::Dist2D(R->Position(S->LeaderUnitID),Objectives[G->Objective].Position)<=BattleGroupValues::ArriveRadius;
    if(bNear){G->bArrived=true;G->PhaseAt=Elapsed;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_ARRIVED group=%d team=%d objective=%d time=%.1f"),ID,Team,G->Objective,Elapsed);}
   }
   const bool bLosses=G->StartLiving>0&&Living<G->StartLiving*BattleGroupValues::LossAbort;
   if(bOwn){if(G->StallPoint==G->Objective){G->StallPoint=INDEX_NONE;G->Stalls=0;}Enter(2,TEXT("point_taken"));}
   else if(bLosses||Elapsed-G->PhaseAt>=(G->bArrived?FMath::Max(10.f,T->GroupAdvanceMax):BattleGroupValues::TravelMax))
   {
    if(bLosses)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_LOSSES group=%d team=%d objective=%d living=%d began=%d after=%.0f time=%.1f"),ID,Team,G->Objective,Living,G->StartLiving,Elapsed-G->PhaseAt,Elapsed);
    // Stalled: the point is not taken. It is avoided for a while; another front point if there is one, else the group stands in
    // reserve at an own point (it used to go on to the enemy's rear points and back, holding nothing).
    Failed();
    const int32 Next=AttackPoint();
    if(Next!=INDEX_NONE)Attack(Next,TEXT("stalled_switch"));
    else if(!Reserve(TEXT("stalled_reserve")))Enter(0,TEXT("stalled_regroup"));
   }
  }
  else if(G->Phase==2)
  {
   if(!bOwn){Attack(G->Objective,TEXT("point_lost"));continue;}
   if(Elapsed-G->PhaseAt<FMath::Max(0.f,T->GroupHoldSeconds))continue;
   // A threatened own point comes first; then the next point to attack; with neither, the group stays.
   if(const int32 Threat=AreaIntelThreat(Team);Threat!=INDEX_NONE&&Threat>=0&&Threat!=G->Objective){Reserve(TEXT("defend_threat"));continue;}
   const int32 Next=AttackPoint();
   if(Next!=INDEX_NONE)Attack(Next,TEXT("next_objective"));else G->PhaseAt=Elapsed;
  }
  else
  {
   // Reserve at an own point: follow the threat; attack again after ReserveMin when its point is not threatened and a point is open.
   if(!bOwn){Attack(G->Objective,TEXT("reserve_point_lost"));continue;}
   const int32 Want=ReservePoint();
   const bool bThreatHere=AreaIntelThreatAt(Team,G->Objective);
   if(Want!=INDEX_NONE&&Want!=G->Objective&&AreaIntelThreatAt(Team,Want)&&!bThreatHere){Reserve(TEXT("defend_threat"));continue;}
   if(bThreatHere||Elapsed-G->PhaseAt<BattleGroupValues::ReserveMin)continue;
   // Not below strength: squads that lost soldiers go home for replacements by their own rules first.
   if(Full>0&&Living<Full*BattleGroupValues::ReadyStrength)continue;
   const int32 Next=AttackPoint();
   if(Next!=INDEX_NONE)Attack(Next,TEXT("attack_again"));
  }
 }
}

bool UBattleMatch::GroupOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const int32 ID=GroupOf(SquadID);const auto* G=ID!=INDEX_NONE?Groups.Find(ID):nullptr;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);
 if(!G||!S||G->Team!=Team||!Objectives.IsValidIndex(G->Objective)||!GroupAIRun(*G))return false;
 const uint8 Role=GroupRole(*G,SquadID);if(Role==255)return false;
 const bool bMortar=Role==3;
 if(G->Phase==0)
 {
  // Gather at the rally point, fighting what comes within range on the way and there (a Defend order would only watch the rally
  // point itself); a mortar team keeps its standoff from known enemies.
  Center=bMortar?AreaArtilleryPost(Team,SquadID,G->Rally):G->Rally;Order=EBattleOrder::AttackMove;return true;
 }
 // Always an attack-move order: at a point the team holds, the group's squads stay there and fight what comes within range (a Defend
 // order only watches DefenseRadius around the place).
 const FVector Point=Objectives[G->Objective].Position;const bool bOwn=ObservedOwner(Team,G->Objective)==Team;
 Order=EBattleOrder::AttackMove;
 // Support and guard squads fire from the support post while the group advances, then move up to the point.
 if((Role==1||Role==4)&&!bOwn&&G->Phase==1){Center=GroupSupportPost(Team,G->Objective);return true;}
 if(Role==2)
 {
  // Close behind its infantry instead of the lone overwatch distance; a post near the current mission is kept.
  Center=AreaTankPost(Team,G->Objective,GetDefault<UBattleTuning>()->GroupTankBack);
  if(S->bHasMission&&FVector::Dist2D(S->MissionDestination,Center)<2500.)Center=S->MissionDestination;
 }
 else Center=bMortar?AreaArtilleryPost(Team,SquadID,Point):Point;
 return true;
}
