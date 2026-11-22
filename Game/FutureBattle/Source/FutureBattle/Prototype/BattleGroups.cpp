// Squad groups, stage 1 (2026-11-18, user proposal: several squads act together as one group; ROADMAP 2026-11-18 "squad group layer").
// A decision layer between the team (which objectives matter) and the squad (its own fight): a group has members, one objective and a
// phase. Assemble: members gather at a rally point short of the objective. Advance: once most of them are there (or after
// GroupAssembleMax seconds) they go in together: infantry to the point, a tank to an overwatch post close behind them, a mortar team
// to its firing post. Hold: after the team observes the point as its own, the group stays GroupHoldSeconds, then takes the next one.
//  AI groups: -BattleAreaGroups=0/1 (UBattleTuning::bAreaGroups, default 1 since 2026-11-20; the trial team with -BattleAreaTrialTeam): one attack
//   group per team from its AI-led squads (the enemy team; own squads delegated to the AI).
//  Player groups: the player forms a group from the selected squads and selects a whole group (ABattleController); orders then go to
//   all its squads as before. A player group whose living squads are all delegated to the AI is run like an AI group.
// Groups and their decision history are saved by combat extension 2. Inputs: own squads' state and positions, observed ownership, fixed map positions.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleRanges.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "NavigationSystem.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
 // Stage 3d: a group that is pushing stays. Pushing = the team sees the point being captured by itself, or it has PushRatio times the
 // known enemies in soldiers around the point (at least PushMin). The stall clock is then extended PushExtend seconds at a time, up to
 // PushMax seconds after the arrival, and the loss limit drops to PushLossAbort.
 constexpr double PushRatio=1.5;
 constexpr int32 PushMin=8;
 constexpr float PushExtend=30.f;
 constexpr float PushMax=480.f;
 constexpr double PushLossAbort=.4;
 constexpr float AvoidCap=3600.f;    // a failed point is avoided GroupAvoidSeconds x 2^(failures in a row - 1), up to this
}

bool BattleTactics::DefendEngage(int32 Team)
{
 static const bool bOn=[]{int32 V=GetDefault<UBattleTuning>()->bDefendEngage?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleDefendEngage="),V);return V!=0;}();
 const int32 Trial=AreaStrategy().TrialTeam;
 return bOn&&(Trial<0||Trial==Team);
}

bool BattleTactics::SymmetricStart(const UWorld* World)
{
 // -1 = not on the command line: on by default on the maps with the urban rule set (E to I; user decision 2026-11-20), off on A/D
 // unless UBattleTuning::bSymmetricStart. An explicit -BattleSymmetricStart=0/1 wins on every map.
 static const int32 Arg=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSymmetricStart="),V);return V;}();
 if(Arg>=0)return Arg!=0;
 return GetDefault<UBattleTuning>()->bSymmetricStart||BattleFieldLayout::Of(World).bUrban;
}

int32 BattleTactics::AllocationIndex(const UWorld* World,int32 SquadID,int32 Team)
{
 if(!World||!SymmetricStart(World))return SquadID;
 // The squad's number within its team (squads of the team with a lower or equal ID; IDs are never reused, so it never changes).
 int32 Index=0;
 for(const auto& P:World->GetSubsystem<UBattleState>()->GetSquads())Index+=P.Value.Team==Team&&P.Key<=SquadID;
 return FMath::Max(1,Index);
}

bool BattleTactics::AreaGroups(int32 Team)
{
 // On by default for both teams. Only an explicit -BattleAreaGroups=1 is a trial that -BattleAreaTrialTeam limits to one team, so a
 // one-sided trial of another switch keeps the groups on both sides.
 static const int32 Arg=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaGroups="),V);return V;}();
 if(Arg<0)return GetDefault<UBattleTuning>()->bAreaGroups;
 const int32 Trial=AreaStrategy().TrialTeam;
 return Arg!=0&&(Trial<0||Trial==Team);
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
 return Type==BattleSquadType::Tank?2:BattleSquadType::IsArtillery(Type)?3:Type==BattleSquadType::AntiTank?4:Type==BattleSquadType::Heavy?1:Type==BattleSquadType::Rifle?0:Type==BattleSquadType::Supply?5:255;
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

// The fewest squads a group keeps: a player group and the front army one, other AI groups two.
static int32 GroupMinimum(const UBattleMatch::FBattleGroup& G){return G.bPlayer||G.Kind==1?1:2;}

int32 UBattleMatch::ArmyOf(int32 SquadID) const
{
 const auto* G=Groups.Find(GroupOf(SquadID));
 return !G?0:G->Kind==1?1:2;
}

bool UBattleMatch::AssignArmy(int32 SquadID,int32 Army,int32 TargetGroup)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||S->Team!=0||S->CommandStatus!=EBattleCommandStatus::Active||Army<0||Army>2)return false;
 ArmyPinned.Add(SquadID);
 if(Army!=0&&ArmyOf(SquadID)==Army&&(TargetGroup==INDEX_NONE||TargetGroup==GroupOf(SquadID)))return true;
 if(const int32 Old=GroupOf(SquadID);Old!=INDEX_NONE)
 {
  auto& O=Groups.FindChecked(Old);O.Squads.Remove(SquadID);SquadGroup.Remove(SquadID);
  if(O.Squads.Num()<GroupMinimum(O))DissolveGroup(Old,TEXT("members_left"));else if(O.Kind==0)AssignGroupRoles(O);
 }
 int32 To=INDEX_NONE;
 if(Army==1)
 {
  for(const auto& P:Groups)if(P.Value.Team==0&&P.Value.Kind==1&&!P.Value.bPlayer)To=P.Key;
  if(To==INDEX_NONE)
  {
   // The front army may start from one squad the player drops in; UpdateFrontLine gives it its posts.
   FBattleGroup G;G.Team=0;G.Kind=1;G.Phase=2;G.PhaseAt=Elapsed;G.ID=NextGroupID++;G.Rally=AreaHome(0);To=G.ID;Groups.Add(To,MoveTemp(G));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_GROUP group=%d team=0 squads=0 player=1 time=%.1f"),To,Elapsed);
  }
 }
 else if(Army==2)
 {
  if(const auto* T=Groups.Find(TargetGroup);T&&T->Team==0&&T->Kind!=1)To=TargetGroup;
  else if(TargetGroup!=NewAttackGroup)for(const auto& P:Groups)if(P.Value.Team==0&&P.Value.Kind==0&&P.Value.bPlayer&&(To==INDEX_NONE||P.Key<To))To=P.Key;
  if(To==INDEX_NONE)To=FormGroup(0,{SquadID},true);
 }
 if(To!=INDEX_NONE&&!Groups[To].Squads.Contains(SquadID))
 {
  auto& G=Groups[To];G.Squads.Add(SquadID);G.Squads.Sort();SquadGroup.Add(SquadID,To);
  if(G.Kind==0)AssignGroupRoles(G);
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMY_ASSIGN squad=%d army=%d group=%d time=%.1f"),SquadID,Army,To,Elapsed);
 return Army==0||To!=INDEX_NONE;
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
  if(O.Squads.Num()<GroupMinimum(O))DissolveGroup(Old,TEXT("members_left"));
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
  if(G->Squads.Num()<GroupMinimum(*G))DissolveGroup(ID,TEXT("members_lost"));
 }
 // 2. AI formation (trial): one attack group per team from its AI-led rifle squads outside the home-assault slots, the nearest to the
 //    rally point first, with one tank, one mortar team, one anti-tank team and one supply vehicle when the team has them.
 int32 AIGroup=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&!P.Value.bPlayer&&P.Value.Kind==0&&!P.Value.bFocus2)AIGroup=AIGroup==INDEX_NONE?P.Key:FMath::Min(AIGroup,P.Key);
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
    if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key)||ArmyPinned.Contains(P.Key)||(BattleTactics::ArtilleryPosts()&&ArtilleryRaids.Contains(P.Key)))continue;
    // Group fire support (trial -BattleGroupFireSupport=1): the AI attack group also takes one self-propelled gun.
    static const bool bFire=[]{int32 X=1;FParse::Value(FCommandLine::Get(),TEXT("BattleGroupFireSupport="),X);return X!=0;}(); // default 1 (user 2026-11-21)
    if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Tank&&S.SquadType!=BattleSquadType::Mortar&&S.SquadType!=BattleSquadType::AntiTank&&S.SquadType!=BattleSquadType::Supply&&!(bFire&&(BattleTactics::AreaStrategy().TrialTeam<0||BattleTactics::AreaStrategy().TrialTeam==Team)&&S.SquadType==BattleSquadType::SPGun))continue;
    bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase)continue;
    Free.Add({P.Key,S.SquadType,FVector::DistSquared2D(R->Position(S.LeaderUnitID),Rally)});
   }
   Free.Sort([](const FFree& A,const FFree& B){return A.D!=B.D?A.D<B.D:A.SID<B.SID;});
   // What the group already has (a new group: nothing), then the nearest free squads up to its numbers.
   TArray<int32> Pick;int32 Rifles=0;bool bTank=false,bMortar=false,bAT=false,bSupply=false,bGun=false;const int32 Want=FMath::Clamp(T->GroupRifleSquads,2,6);
   if(AIGroup!=INDEX_NONE)for(int32 SID:Groups[AIGroup].Squads)if(const auto* S=Store->FindSquad(SID))
   {Rifles+=S->SquadType==BattleSquadType::Rifle;bTank|=S->SquadType==BattleSquadType::Tank;bMortar|=S->SquadType==BattleSquadType::Mortar;bAT|=S->SquadType==BattleSquadType::AntiTank;bSupply|=S->SquadType==BattleSquadType::Supply;bGun|=S->SquadType==BattleSquadType::SPGun;}
   for(const auto& F:Free)
   {
    if(F.Type==BattleSquadType::Rifle){if(Rifles<Want){Pick.Add(F.SID);++Rifles;}}
    else if(F.Type==BattleSquadType::Tank){if(!bTank){Pick.Add(F.SID);bTank=true;}}
    else if(F.Type==BattleSquadType::Mortar){if(!bMortar){Pick.Add(F.SID);bMortar=true;}}
    else if(F.Type==BattleSquadType::Supply){if(!bSupply){Pick.Add(F.SID);bSupply=true;}}
    else if(F.Type==BattleSquadType::SPGun){if(!bGun){Pick.Add(F.SID);bGun=true;}}
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
 // 2b. Front plan stage 5: the second attack army (rifle squads only) for the second focus; ends when the second focus ends.
 if(FrontPlanFocus2On(Team))
 {
  FFrontPlanTeam& FP=FrontPlanState[Team];
  int32 F2Group=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&!P.Value.bPlayer&&P.Value.Kind==0&&P.Value.bFocus2)F2Group=P.Key;
  const bool bWant=Objectives.IsValidIndex(FP.Focus2)&&ObservedOwner(Team,FP.Focus2)!=Team;
  if(F2Group!=INDEX_NONE&&!bWant)DissolveGroup(F2Group,TEXT("focus2_end"));
  struct FFree2{int32 SID;double D;};TArray<FFree2> Free2;
  const FVector Rally2=bWant?GroupRally(Team,FP.Focus2):AreaHome(Team);
  for(const auto& P:Store->GetSquads())
  {
   const auto& S=P.Value;
   if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(P.Key)!=INDEX_NONE||S.SquadType!=BattleSquadType::Rifle)continue;
   if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key)||ArmyPinned.Contains(P.Key))continue;
   bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase)continue;
   Free2.Add({P.Key,FVector::DistSquared2D(R->Position(S.LeaderUnitID),Rally2)});
  }
  // Only squads above the front army's open need count as free (the front army fills after this, UpdateFrontLine).
  int32 FrontShort=0;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==1&&!P.Value.bPlayer)FrontShort=FMath::Max(0,P.Value.FrontNeed-P.Value.Squads.Num());
  FP.FreeRifles=FMath::Max(0,Free2.Num()-FrontShort);
  if(bWant)
  {
   Free2.Sort([](const FFree2& A,const FFree2& B){return A.D!=B.D?A.D<B.D:A.SID<B.SID;});
   if(FP.FreeRifles<Free2.Num())Free2.SetNum(FP.FreeRifles);
   const int32 Want2=FMath::Clamp(T->GroupRifleSquads,3,6);
   const int32 Have2=F2Group!=INDEX_NONE&&Groups.Contains(F2Group)?Groups[F2Group].Squads.Num():0;
   TArray<int32> Pick2;for(const auto& F:Free2)if(Have2+Pick2.Num()<Want2)Pick2.Add(F.SID);
   if(F2Group==INDEX_NONE||!Groups.Contains(F2Group))
   {
    if(Pick2.Num()>=3)if(auto* G2=Groups.Find(FormGroup(Team,Pick2,false)))
    {
     G2->bFocus2=true;G2->bOpened=true;G2->Objective=FP.Focus2;G2->Rally=Rally2;G2->Phase=0;G2->PhaseAt=Elapsed;
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_FOCUS2_GROUP team=%d group=%d point=%d squads=%d time=%.1f"),Team,G2->ID,FP.Focus2,G2->Squads.Num(),Elapsed);
    }
   }
   else if(Pick2.Num())
   {
    auto& G2=Groups[F2Group];
    for(int32 SID:Pick2){G2.Squads.Add(SID);SquadGroup.Add(SID,F2Group);}
    G2.Squads.Sort();
   }
  }
 }
 // 3. Phases of the groups the AI runs.
 Groups.GetKeys(IDs);IDs.Sort();
 for(int32 ID:IDs)
 {
  auto* G=Groups.Find(ID);if(!G||G->Team!=Team||!GroupAIRun(*G))continue;
  if(G->Kind!=0)continue; // front-holding and surprise groups: UpdateFrontLine / UpdateSurpriseGroup
  auto Enter=[&](uint8 To,const TCHAR* Why)
  {
   G->Phase=To;G->PhaseAt=Elapsed;
   if(To==1){AssignGroupRoles(*G);G->StartLiving=GroupLiving(*G);if(BattleTactics::GroupStrengthEnabled())G->StartStrength=GroupPower(*G).Total();G->bArrived=false;}
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_PHASE group=%d team=%d phase=%d objective=%d squads=%d reason=%s time=%.1f"),ID,Team,int32(To),G->Objective,G->Squads.Num(),Why,Elapsed);
  };
  // Squads away at home for supply do not hold the group up.
  int32 Present=0,Ready=0;FVector Center=FVector::ZeroVector;
  for(int32 SID:G->Squads)
  {
   const auto* S=Store->FindSquad(SID);if(!S)continue;
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
   // The supply vehicle keeps its own place (BattleSupplyCar.cpp) and does not hold the group up either.
   if(S->SquadType==BattleSquadType::Supply)continue;
   const FVector P=R->Position(S->LeaderUnitID);++Present;Center+=P;
   Ready+=FVector::Dist2D(P,G->Rally)<=FMath::Max(500.f,T->GroupRallyRadius);
  }
  if(Present)Center/=Present;
  // Stage 3: what the group decides as a whole. Own squads' numbers, observed ownership, known enemies (AreaIntelView).
  const auto& V=AreaIntelView(Team);const FVector Home=AreaHome(Team),EnemyHome=AreaHome(1-Team);
  int32 Full=0;const int32 Living=GroupLiving(*G,&Full);
  const bool Weighted=BattleTactics::GroupStrengthEnabled();
  FGroupStrength Power,FullPower;
  if(Weighted){Power=GroupPower(*G);FullPower=GroupPower(*G,true);if(G->StartStrength<0)G->StartStrength=Power.Total();}
  auto ThreatPoint=[&](){
   // Strategy layer S3 (BattleStrategy.cpp): a held point (own, "enemy ahead" for 20 s by the situation map) comes first.
   if(const int32 Held=StrategyHoldPoint(Team);Held!=INDEX_NONE)return Held;
   if(!Weighted)return AreaIntelThreat(Team);
   int32 Best=INDEX_NONE;double BestRisk=-1;
   for(int32 I=0;I<Objectives.Num();++I)if(GroupPointThreat(Team,I))
   {const auto E=GroupPointPower(Team,I,true),O=GroupPointPower(Team,I,false);const double Risk=E.Total()+E.Unknown*FMath::Max(0.f,T->GroupUnknownRisk)-O.Total();if(Risk>BestRisk||Best==INDEX_NONE){BestRisk=Risk;Best=I;}}
   return Best;
  };
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
   // Strategy layer S2 (BattleStrategy.cpp): the team's focus first, while it is not own and the group is not avoiding it.
   if(!G->bPlayer)if(const int32 F=GroupFocusPoint(*G);F!=INDEX_NONE&&ObservedOwner(Team,F)!=Team&&!(bAvoid&&F==G->Avoid))return F;
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
   const int32 Threat=ThreatPoint();if(Threat!=INDEX_NONE&&Threat>=0)return Threat;
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
    if(Best!=INDEX_NONE){G->Objective=Best;G->Rally=GroupRally(Team,Best);Enter(Weighted&&!Power.SuperiorTo(GroupPointPower(Team,Best,true),BattleGroupValues::CommitRatio)?0:1,TEXT("opening_rush"));continue;}
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
   // Strategy layer S2: a group still gathering turns to a new team focus (an advance under way finishes first).
   // Not more often than every 30 s (group lane 2026-11-22: switching a gathering group's goal makes it go back and forth).
   if(!G->bPlayer&&Elapsed-G->PhaseAt>=30.f)if(const int32 F=GroupFocusPoint(*G);F!=INDEX_NONE&&F!=G->Objective&&ObservedOwner(Team,F)!=Team&&!(Elapsed<G->AvoidUntil&&F==G->Avoid)){Attack(F,TEXT("strategy_focus"));continue;}
   const bool bAssembled=Present>=2&&Ready>=FMath::CeilToInt(Present*FMath::Clamp(T->GroupReadyShare,.1f,1.f));
   const float Waited=Elapsed-G->PhaseAt;
   if(!bAssembled&&Waited<FMath::Max(0.f,T->GroupAssembleMax))continue;
   // Commit: go in only with enough soldiers for the enemies the team knows of at the point; otherwise wait for more, then stand back.
   const int32 Known=V.EnemyAt.IsValidIndex(G->Objective)?V.EnemyAt[G->Objective]:0;
   if(Weighted?Power.SuperiorTo(GroupPointPower(Team,G->Objective,true),BattleGroupValues::CommitRatio):Living>=Known*BattleGroupValues::CommitRatio)Enter(1,bAssembled?TEXT("assembled"):TEXT("assemble_timeout"));
   else if(Waited>=FMath::Max(T->GroupAssembleMax,BattleGroupValues::WaitMax))
   {
    Failed();
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_HOLD_BACK group=%d team=%d objective=%d living=%d known=%d time=%.1f"),ID,Team,G->Objective,Living,Known,Elapsed);
    if(!Reserve(TEXT("outnumbered")))Enter(Weighted?0:1,TEXT("no_reserve_point"));
   }
  }
  else if(G->Phase==1)
  {
   // The stall clock runs from the arrival at the point, not from the decision to go (the walk may take longer than the clock).
   if(!G->bArrived)
   {
    bool bNear=false;
    for(int32 SID:G->Squads)if(const auto* S=Store->FindSquad(SID);S&&(!Weighted||GroupSquadPresent(*S)))bNear|=FVector::Dist2D(R->Position(S->LeaderUnitID),Objectives[G->Objective].Position)<=BattleGroupValues::ArriveRadius;
    if(bNear){G->bArrived=true;G->PhaseAt=Elapsed;G->ArrivedAt=Elapsed;G->Extensions=0;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_ARRIVED group=%d team=%d objective=%d time=%.1f"),ID,Team,G->Objective,Elapsed);}
   }
   // The trial substitutes observed ownership for the live capture marker; known local superiority remains a separate reason.
   const int32 Slot=Team*Objectives.Num()+G->Objective;
   const bool bSeen=Area.SeenAt.IsValidIndex(Slot)&&Area.SeenAt[Slot]>=0.f&&Elapsed-Area.SeenAt[Slot]<=1.f;
   const int32 OwnThere=V.OwnAt.IsValidIndex(G->Objective)?V.OwnAt[G->Objective]:0,KnownThere=V.EnemyAt.IsValidIndex(G->Objective)?V.EnemyAt[G->Objective]:0;
   FGroupStrength OwnPower,EnemyPower;
   if(Weighted){OwnPower=GroupPointPower(Team,G->Objective,false);EnemyPower=GroupPointPower(Team,G->Objective,true);}
   const bool bPushing=G->bArrived&&(Weighted?
    (OwnPower.Total()>0&&((bSeen&&(BattleTactics::ObservedTactics()?ObservedOwner(Team,G->Objective):Objectives[G->Objective].CapturingTeam)==Team)||(OwnPower.Infantry>=BattleGroupValues::PushMin&&OwnPower.SuperiorTo(EnemyPower,BattleGroupValues::PushRatio)))):
    ((bSeen&&(BattleTactics::ObservedTactics()?ObservedOwner(Team,G->Objective):Objectives[G->Objective].CapturingTeam)==Team)||(OwnThere>=BattleGroupValues::PushMin&&OwnThere>=KnownThere*BattleGroupValues::PushRatio)));
   const double LossLimit=bPushing?BattleGroupValues::PushLossAbort:BattleGroupValues::LossAbort;
   const bool bLosses=Weighted?(G->StartStrength>0&&Power.Total()<G->StartStrength*LossLimit):(G->StartLiving>0&&Living<G->StartLiving*LossLimit);
   if(bPushing&&!bOwn&&!bLosses&&Elapsed-G->PhaseAt>=FMath::Max(10.f,T->GroupAdvanceMax)&&Elapsed-G->ArrivedAt<BattleGroupValues::PushMax)
   {
    G->PhaseAt+=BattleGroupValues::PushExtend;++G->Extensions;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_PUSH group=%d team=%d objective=%d own=%d known=%d capturing=%d extensions=%d time=%.1f"),ID,Team,G->Objective,OwnThere,KnownThere,bSeen?(BattleTactics::ObservedTactics()?ObservedOwner(Team,G->Objective):Objectives[G->Objective].CapturingTeam):-2,G->Extensions,Elapsed);
   }
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
   if(const int32 Threat=ThreatPoint();Threat!=INDEX_NONE&&Threat>=0&&Threat!=G->Objective){Reserve(TEXT("defend_threat"));continue;}
   const int32 Next=AttackPoint();
   if(Next!=INDEX_NONE)Attack(Next,TEXT("next_objective"));else G->PhaseAt=Elapsed;
  }
  else
  {
   // Reserve at an own point: follow the threat; attack again after ReserveMin when its point is not threatened and a point is open.
   if(!bOwn){Attack(G->Objective,TEXT("reserve_point_lost"));continue;}
   const int32 Want=ReservePoint();
   const bool bThreatHere=GroupPointThreat(Team,G->Objective);
   if(Want!=INDEX_NONE&&Want!=G->Objective&&GroupPointThreat(Team,Want)&&!bThreatHere){Reserve(TEXT("defend_threat"));continue;}
   if(bThreatHere||Elapsed-G->PhaseAt<BattleGroupValues::ReserveMin)continue;
   // Not below strength: squads that lost soldiers go home for replacements by their own rules first.
   if(Weighted?(Power.Total()<=0||Power.Total()<FullPower.Total()*BattleGroupValues::ReadyStrength):(Full>0&&Living<Full*BattleGroupValues::ReadyStrength))continue;
   const int32 Next=AttackPoint();
   if(Next!=INDEX_NONE)Attack(Next,TEXT("attack_again"));
  }
 }
 // 4. Front line and the front-holding group (trial -BattleFrontLine=1, BattleFrontLine.cpp).
 const float FrontWas=FrontLineAt[Team];
 UpdateFrontLine(Team);
 // Front plan log (-BattleFrontPlanLog=1): only in a frame where the line was just rebuilt, so the known-enemy view it reads is the one
 // UpdateFrontLine refreshed (calling AreaIntelView at another time would move its once-per-second refresh and change the match).
 if((FrontPlanLogOn()||FrontPlanOn(Team))&&FrontLineAt[Team]!=FrontWas&&FrontLine[Team].Num()==BattleSectors::Rows)FrontPlanSample(Team);
}

bool UBattleMatch::GroupOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const int32 ID=GroupOf(SquadID);const auto* G=ID!=INDEX_NONE?Groups.Find(ID):nullptr;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);
 // The front army holds posts squad by squad: one squad the player commands by hand does not stop the others (2026-11-21).
 if(G&&S&&G->Kind==1)return G->Team==Team&&(Team!=0||S->bPlayerDelegated)&&FrontGroupOrder(SquadID,Team,Center,Order); // BattleFrontLine.cpp
 if(G&&S&&G->Kind==2)return G->Team==Team&&GroupAIRun(*G)&&SurpriseGroupOrder(ID,SquadID,Team,Center,Order);
 if(!G||!S||G->Team!=Team||!Objectives.IsValidIndex(G->Objective)||!GroupAIRun(*G))return false;
 if(BattleTactics::GroupStrengthEnabled()&&!GroupSquadPresent(*S))return false;
 const uint8 Role=GroupRole(*G,SquadID);if(Role==255||Role==5)return false; // 5 supply: SupplyCarOrder (BattleSupplyCar.cpp)
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

bool BattleTactics::GroupStrengthEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleGroupStrength="),V);return V!=0;}();
 return On;
}
void UBattleMatch::FGroupStrength::Add(uint8 Type)
{
 if(Type>=BattleSquadType::Count){++Unknown;return;}
 const auto* T=GetDefault<UBattleTuning>();
 auto Weight=[&](const TArray<float>& Values){return Values.IsValidIndex(Type)&&FMath::IsFinite(Values[Type])?double(FMath::Clamp(Values[Type],0.f,100.f)):0.;};
 Infantry+=Weight(T->GroupAntiInfantry);Armour+=Weight(T->GroupAntiArmour);
 if(BattleSquadType::IsVehicle(Type))++Hard;else ++Soft;
}
double UBattleMatch::FGroupStrength::Against(const FGroupStrength& Other) const
{
 // No identified target mix: use anti-infantry power, without pretending unknown reports are infantry.
 const double Count=Other.Soft+Other.Hard;
 return Count>0?(Infantry*Other.Soft+Armour*Other.Hard)/Count:Infantry;
}
bool UBattleMatch::FGroupStrength::SuperiorTo(const FGroupStrength& Other,double Ratio) const
{
 const float Risk=GetDefault<UBattleTuning>()->GroupUnknownRisk;
 const double Uncertainty=Other.Unknown*(FMath::IsFinite(Risk)?FMath::Clamp(Risk,0.f,100.f):1.5f);
 return Total()>0&&Against(Other)>=Ratio*(Other.Against(*this)+Uncertainty);
}
bool UBattleMatch::GroupSquadPresent(const FBattleSquadState& Squad)
{
 return Squad.CommandStatus==EBattleCommandStatus::Active&&
  !(Squad.TacticalPlan.Action==EBattleSquadTactic::Recover&&Squad.TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")));
}
UBattleMatch::FGroupStrength UBattleMatch::GroupPower(const FBattleGroup& Group,bool bFull) const
{
 FGroupStrength Result;const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 TArray<int32> Squads=Group.Squads;Squads.Sort();
 for(int32 SID:Squads)if(const auto* S=Store->FindSquad(SID);S&&GroupSquadPresent(*S))
  for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&(bFull||U->IsAlive()))Result.Add(S->SquadType);
 return Result;
}
UBattleMatch::FGroupStrength UBattleMatch::GroupPointPower(int32 Team,int32 Point,bool bEnemy) const
{
 FGroupStrength Result;if(!Objectives.IsValidIndex(Point))return Result;
 const FVector At=Objectives[Point].Position;
 if(bEnemy)
 {
  const auto& V=AreaIntelView(Team);TArray<int32> Order;
  for(int32 I=0;I<V.Enemies.Num();++I)Order.Add(I);
  Order.Sort([&](int32 A,int32 B){return V.EnemyKeys[A]<V.EnemyKeys[B];});
  const double Radius=FMath::Max(0.f,GetDefault<UBattleTuning>()->AreaIntelRadius);
  for(int32 I:Order)if(FVector::DistSquared2D(V.Enemies[I],At)<=Radius*Radius)
  {const auto* Type=V.EnemyTypes.Find(V.EnemyKeys[I]);Result.Add(Type?*Type:255);}
 }
 else
 {
  const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  TArray<int32> IDs;Store->GetUnits().GetKeys(IDs);IDs.Sort();
  for(int32 ID:IDs)
  {
   const auto* U=Store->Find(ID);const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
   if(U&&U->IsAlive()&&U->Team==Team&&S&&GroupSquadPresent(*S)&&FVector::DistSquared2D(R->Position(ID),At)<=FMath::Square(10000.))Result.Add(S->SquadType);
  }
 }
 return Result;
}
bool UBattleMatch::GroupPointThreat(int32 Team,int32 Point) const
{
 if(StrategyHolds(Team,Point))return true; // strategy layer S3: a held point counts as threatened (-BattleStrategy=3 only)
 if(!BattleTactics::GroupStrengthEnabled())return AreaIntelThreatAt(Team,Point);
 if(ObservedOwner(Team,Point)!=Team)return false;
 const auto Own=GroupPointPower(Team,Point,false),Enemy=GroupPointPower(Team,Point,true);
 return (Enemy.Total()>0||Enemy.Unknown>0)&&!Own.SuperiorTo(Enemy,1.);
}
