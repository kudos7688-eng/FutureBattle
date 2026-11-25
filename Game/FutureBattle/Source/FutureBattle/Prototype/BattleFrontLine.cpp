// Front line and front-holding group (2026-11-20, user proposal: keep the sectors continuous to make a front line, hold it with one
// front-holding group as the base, and send raid and surprise groups from it; ROADMAP 2026-11-20 "front-holding group"). Group stage 4,
// first part. Trial -BattleFrontLine=1 (default 0; -BattleAreaTrialTeam limits it to one team), maps with sectors (H, I) only.
//  Line: for every sector row (the 14 rows across the axis between the homes) one column, counted from the team's own home edge: the most
//   forward column where at least two own infantry soldiers stand or an own point (observed) lies, never past the column before two
//   known enemies in one sector (own sight, scout reports) or a point observed as the enemy's. Rows without own soldiers take their neighbours'. Then
//   neighbouring rows differ by at most one column (salients are pulled back), so the line has no gaps.
//  Front-holding group: -BattleFrontSquads=n (default 4) AI-led rifle and heavy squads outside other groups. The rows are split into
//   one band per present squad (in the order of the rows they stand in, so they do not cross). Each squad holds the sector with the
//   most cover on the line in its band or one column behind it; with no known enemy within three columns ahead of its band it takes
//   the next column forward, and the line follows (continuity keeps the bands together).
//  Front army (2026-11-21, user proposal after test play: spread squads used cover better than crowded ones; trial -BattleFrontArmy=1,
//   needs -BattleFrontLine=1): the front-holding group becomes the front army and takes every free AI-led rifle and heavy squad (the
//   attack group fills first in UpdateGroups, the surprise group may take front squads while at least FrontArmyKeep stay). More squads
//   than rows share rows in different sectors (no two posts in one sector); a sector with a point not observed as own scores higher.
// Inputs: own soldiers' positions, known enemy positions (AreaIntelView), observed ownership, the fixed sector kinds. Not saved.
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleMapI.h"
#include "BattleSectors.h"
#include "BattleSquadTactics.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "FutureBattle.h"

namespace BattleFrontValues
{
 constexpr float Interval=2.f, LogInterval=30.f;
 constexpr int32 ClearAhead=3; // columns ahead of a band without known enemies before its squad steps forward
 int32 Arg(const TCHAR* Name,int32 Default){int32 V=Default;FParse::Value(FCommandLine::Get(),Name,V);return V;}
 constexpr int32 FrontArmyKeep=4; // squads the front army keeps when the surprise group takes from it
 bool Army(){static const int32 On=Arg(TEXT("BattleFrontArmy="),1);return On!=0;} // default on since 2026-11-21 (user decision)
 // One-sector gaps count as held (user proposal 2026-11-21 after test play; -BattleFrontGap=0 turns it off).
 // The AI fills the front-holding group by itself (-BattleFrontAuto, default 1 by user decision 2026-11-21 "brush it up from there",
 // although the one-sided screen ga1 on map I lost 180-320 points of lead against the earlier allocation). 0: only squads the player
 // drops on the front army tab hold the line; the AI teams keep the earlier allocation.
 bool AutoFill(){static const int32 On=Arg(TEXT("BattleFrontAuto="),1);return On!=0;}
 // Trial -BattleFrontShare=<0..1> (default 1 = as before; ROADMAP stage 5, Docs/design-groups-strategy-link.md, the next step after ga1):
 // the AI fills its front army only up to this share of the squads it could put there (its members + the free ones). Squads already
 // in the group (including those the player dropped there) are never taken out for the share. One team with -BattleAreaTrialTeam.
 double Share(int32 Team)
 {
  static const double V=[]{float X=1.f;FParse::Value(FCommandLine::Get(),TEXT("BattleFrontShare="),X);return FMath::IsFinite(X)?double(FMath::Clamp(X,0.f,1.f)):1.;}();
  const int32 Trial=BattleTactics::AreaStrategy().TrialTeam;
  return Trial<0||Trial==Team?V:1.;
 }
 bool Gap(){static const int32 On=Arg(TEXT("BattleFrontGap="),1);return On!=0;}
 // The front army holds one line as a group (user report 2026-11-21 after test play: its squads moved each on their own and went back
 // and forth, because the line followed where own soldiers stood and each band stepped forward by itself). The group keeps its line
 // (FBattleGroup::FrontHold): rows push and fall back by the damage the group takes; the group steps the rows on its own pace (StepEvery),
 // each row waiting only for its own squads. Posts are fixed places on the line (row and depth), also while squads are
 // away for supply. -BattleFrontHold=0: the earlier line from own positions. Needs the front army.
 bool Hold(){static const int32 On=Arg(TEXT("BattleFrontHold="),1);return On!=0&&Army();}
 // Opening line (user proposal 2026-11-22: the first line sat next to the home, so the opening was slow): with Hold() the group's first
 // line lies this percentage of the way from the own home column to the enemy home column, short of known enemies, and the group moves
 // it by the situation from there. -BattleFrontOpen=0: the first line is where the own soldiers stand (the earlier start).
 constexpr int32 ReleaseMargin=1, ReleaseMax=4; constexpr float ReleaseHold=20.f, ExcessHold=15.f; // squads above the need released (5b)
 int32 OpenPercent(){static const int32 V=FMath::Clamp(Arg(TEXT("BattleFrontOpen="),40),0,80);return V;}
 // Reserve behind the front (trial -BattleFrontReserve=1, default 0; user request 2026-11-22, 6c below). Needs Hold().
 bool Reserve(){static const int32 On=Arg(TEXT("BattleFrontReserve="),0);return On!=0&&Hold();}
 // Second test play 2026-11-21 19:10 "too slow": half of the squads counted as off their posts (35 m from the sector centre), so the line
 // stepped only every 60 s. Now on the post sector or within one sector of its centre, and shorter waits.
 // The group sets the pace (user proposal 2026-11-22: squads were swayed too much by the others): every StepEvery seconds each row
 // that may go steps, whether or not the other rows' squads have arrived. A row waits only for its own squads: it does not step while
 // its foremost squad is more than RowLag columns behind the row's line, so the line never runs away from the squads holding it.
 constexpr float StepEvery=20.f; constexpr int32 RowLag=1;
 constexpr double OnPost=5000.; // within this of the post (cm) counts as on post (log only)
 // The group's aim is to raise its line (user 2026-11-21 19:15 "push the line forward on purpose, as a group"), judged by its own losses
 // rather than by known enemy numbers (user 19:20 "weigh the damage over the enemy count"; seen enemies depend on sight, own damage is
 // sure). Per row, over its front squads posted in rows +-1: the damage of the last 5 s against their health (UBattleSquadTactics).
 //  - Quiet (no such squad damaged): the row steps forward, unless known enemies just ahead are at least Swamp times the own soldiers.
 //  - Light damage (below LightDamage): steps forward only where own soldiers near the line are PushRatio times the known enemies ahead.
 //  - Heavy damage (BackDamage or more): the row falls back one column, at most once per BackHold seconds.
 constexpr double LightDamage=.03, BackDamage=.10, PushRatio=1.5, Swamp=2.; constexpr int32 SwampMin=4; constexpr float BackHold=10.f;
 // Sector kinds by the cover they give a squad that waits there (river sectors are not held).
 double Cover(char Kind){switch(Kind){case 'H':case 'W':return 3.;case 'T':case '^':return 2.;case 'J':case 'B':case 'S':return 1.;case 'R':case 'M':case 'L':return -100.;default:return 0.;}}
 // Squads to places by walking distance D[squad][place] (square): the matching whose longest walk is the least, and among those the one
 // with the least total walk. Returns the place of each squad. Used once when the front army first takes its places (25 squads: fast).
 TArray<int32> AssignMinTime(const TArray<TArray<double>>& D)
 {
  const int32 N=D.Num();TArray<int32> Out;Out.Init(INDEX_NONE,N);if(!N)return Out;
  // 1. The least limit under which every squad still gets a place of its own (augmenting paths).
  TArray<double> All;for(const auto& Row:D)All.Append(Row);All.Sort();
  auto Fits=[&](double T)
  {
   TArray<int32> Of;Of.Init(INDEX_NONE,N);TArray<bool> Seen;
   TFunction<bool(int32)> Try=[&](int32 I)->bool
   {for(int32 J=0;J<N;++J)if(D[I][J]<=T&&!Seen[J]){Seen[J]=true;if(Of[J]==INDEX_NONE||Try(Of[J])){Of[J]=I;return true;}}return false;};
   for(int32 I=0;I<N;++I){Seen.Init(false,N);if(!Try(I))return false;}
   return true;
  };
  int32 Lo=0,Hi=All.Num()-1;while(Lo<Hi){const int32 Mid=(Lo+Hi)/2;if(Fits(All[Mid]))Hi=Mid;else Lo=Mid+1;}
  const double Limit=All[Lo]+1.;
  // 2. The least total walk under that limit (Hungarian method, 1-based).
  constexpr double Inf=1.e30;
  TArray<double> U,V,MinV;TArray<int32> P,Way;U.Init(0.,N+1);V.Init(0.,N+1);P.Init(0,N+1);Way.Init(0,N+1);
  auto Cost=[&](int32 I,int32 J){const double X=D[I-1][J-1];return X<=Limit?X:X+1.e9;};
  for(int32 I=1;I<=N;++I)
  {
   P[0]=I;int32 J0=0;MinV.Init(Inf,N+1);TArray<bool> Used;Used.Init(false,N+1);
   do
   {
    Used[J0]=true;const int32 I0=P[J0];double Delta=Inf;int32 J1=0;
    for(int32 J=1;J<=N;++J)if(!Used[J]){const double Cur=Cost(I0,J)-U[I0]-V[J];if(Cur<MinV[J]){MinV[J]=Cur;Way[J]=J0;}if(MinV[J]<Delta){Delta=MinV[J];J1=J;}}
    for(int32 J=0;J<=N;++J)if(Used[J]){U[P[J]]+=Delta;V[J]-=Delta;}else MinV[J]-=Delta;
    J0=J1;
   }while(P[J0]!=0);
   do{const int32 J1=Way[J0];P[J0]=P[J1];J0=J1;}while(J0);
  }
  for(int32 J=1;J<=N;++J)if(P[J]>0)Out[P[J]-1]=J-1;
  return Out;
 }
}

bool UBattleMatch::FrontLineOn(int32 Team) const
{
 // Default on since 2026-11-21 (user decision after test play). Only an explicit -BattleFrontLine=1 is a trial that
 // -BattleAreaTrialTeam limits to one team (2026-11-22 fix: a one-sided trial of another switch left the other team without its
 // front army on the sector maps); without the argument both teams have it.
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontLine="),-1);
 if(On==0||!bArea||Team<0||Team>1||!BattleFieldLayout::Of(GetWorld()).bSectors)return false;
 if(On<0)return true;
 const int32 Trial=BattleTactics::AreaStrategy().TrialTeam;
 return Trial<0||Trial==Team;
}

FIntPoint UBattleMatch::FrontCell(int32 Team,int32 Column,int32 Row) const
{
 // Column 0 is the edge of the team's own home side. The wide maps are 180-degree symmetric, so the team on the high X side counts
 // its rows from the other edge too (area AI report 2026-11-22: with only the columns turned, the east team's rows were mirrored, not
 // rotated, so row-order choices such as gaps, supports and ties fell on the other side for it).
 const bool bLow=AreaHome(Team).X<0;
 return FIntPoint(bLow?Column:BattleSectors::Columns-1-Column,bLow?Row:BattleSectors::Rows-1-Row);
}

void UBattleMatch::UpdateFrontLine(int32 Team)
{
 using namespace BattleFrontValues;
 if(Team<0||Team>1)return;
 if(!FrontLineOn(Team))
 {
  if(!FrontLine[Team].IsEmpty()){FrontLine[Team].Reset();TArray<int32> IDs;Groups.GetKeys(IDs);for(int32 ID:IDs)if(Groups[ID].Team==Team&&Groups[ID].Kind==1)DissolveGroup(ID,TEXT("front_off"));}
  return;
 }
 if(Elapsed<FrontLineAt[Team])return;
 FrontLineAt[Team]=Elapsed+Interval;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return;
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 const bool bLow=AreaHome(Team).X<0;
 auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 auto RowOf=[&](const FIntPoint& C){return bLow?C.Y:Rows-1-C.Y;}; // the team's own row of a grid cell (FrontCell)
 const int32 HomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(Team))),0,Cols-1);
 const int32 EnemyHomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(1-Team))),0,Cols-1);
 // 1. Own infantry per sector, and the most forward column per row with two or more of them (or an own point).
 TMap<FIntPoint,int32> OwnCount;
 for(const auto& P:Store->GetSquads())
 {
  const auto& S=P.Value;
  if(S.Team!=Team||BattleSquadType::IsVehicle(S.SquadType)||S.SquadType==BattleSquadType::Scout)continue;
  for(int32 UID:S.Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive())
   if(const FIntPoint C=BattleSectors::Cell(R->Position(UID));BattleSectors::Valid(C))++OwnCount.FindOrAdd(C);
 }
 TArray<int32> Reach,EnemyCol;Reach.Init(-1,Rows);EnemyCol.Init(Cols,Rows);
 for(const auto& P:OwnCount)if(P.Value>=2)Reach[RowOf(P.Key)]=FMath::Max(Reach[RowOf(P.Key)],ColOf(P.Key));
 for(int32 I=0;I<Objectives.Num();++I)
 {
  const FIntPoint C=BattleSectors::Cell(Objectives[I].Position);if(!BattleSectors::Valid(C))continue;
  const int32 Owner=ObservedOwner(Team,I);
  if(Owner==Team)Reach[RowOf(C)]=FMath::Max(Reach[RowOf(C)],ColOf(C));
  else if(Owner==1-Team)EnemyCol[RowOf(C)]=FMath::Min(EnemyCol[RowOf(C)],ColOf(C));
 }
 // 2. Known enemies: two or more in a sector (one soldier slipping through does not pull the line back to the home).
 TMap<FIntPoint,int32> EnemyCount;
 for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))++EnemyCount.FindOrAdd(C);
 for(const auto& P:EnemyCount)if(P.Value>=2)EnemyCol[RowOf(P.Key)]=FMath::Min(EnemyCol[RowOf(P.Key)],ColOf(P.Key));
 // 2b. A row held less deep than both neighbours (the one neighbour on an edge row) counts as held up to the shallower neighbour when no
 //  known enemy stands in it up to there (user proposal 2026-11-21: with the line strictly joined, one empty sector held the advance back).
 if(Gap())
 {
  TArray<int32> Filled=Reach;
  for(int32 Y=0;Y<Rows;++Y)if(Reach[Y]>=0)
  {
   const int32 A=Y>0?Reach[Y-1]:-1,B=Y+1<Rows?Reach[Y+1]:-1;
   const int32 N=Y==0?B:Y==Rows-1?A:FMath::Min(A,B);
   if(N>Reach[Y]&&EnemyCol[Y]>N)Filled[Y]=N;
  }
  Reach=Filled;
 }
 // 3. Rows without own soldiers: the nearer known neighbour's column (the lower one on a tie); none at all: just ahead of the home.
 TArray<int32> Line=Reach;
 for(int32 Y=0;Y<Rows;++Y)if(Line[Y]<0)
 {
  int32 Best=-1;
  for(int32 D=1;D<Rows&&Best<0;++D)
  {
   const int32 A=Y-D>=0?Reach[Y-D]:-1,B=Y+D<Rows?Reach[Y+D]:-1;
   Best=A>=0&&B>=0?FMath::Min(A,B):FMath::Max(A,B);
  }
  Line[Y]=Best;
 }
 for(int32 Y=0;Y<Rows;++Y)
 {
  Line[Y]=FMath::Max(Line[Y],HomeCol+1);
  // Not into or past a known enemy: the column before it (but never behind the home).
  Line[Y]=FMath::Clamp(FMath::Min(Line[Y],EnemyCol[Y]-1),HomeCol,FMath::Max(HomeCol,EnemyHomeCol-2));
 }
 // 4. Continuity: neighbouring rows differ by one column at most (forward salients are pulled back).
 for(int32 Pass=0;Pass<Rows;++Pass)
 {
  bool bChanged=false;
  for(int32 Y=0;Y<Rows;++Y)
  {
   int32 Cap=Line[Y];
   if(Y>0)Cap=FMath::Min(Cap,Line[Y-1]+1);
   if(Y+1<Rows)Cap=FMath::Min(Cap,Line[Y+1]+1);
   if(Cap<Line[Y]){Line[Y]=Cap;bChanged=true;}
  }
  if(!bChanged)break;
 }
 FrontLine[Team]=Line;

 // 5. The front-holding group: formed from free AI-led rifle and heavy squads, filled up to its number.
 int32 FrontID=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==1&&!P.Value.bPlayer)FrontID=P.Key;
 // Hold(): the group takes the squads its line needs (FBattleGroup::FrontNeed, counted with the places in 6) and no more
 // (user proposal 2026-11-22); before the first count, two per row.
 const int32 Need=Hold()&&FrontID!=INDEX_NONE?Groups[FrontID].FrontNeed:0;
 const int32 Want=Need>0?Need:Army()?2*Rows:FMath::Clamp(Arg(TEXT("BattleFrontSquads="),4),2,12);
 const int32 Have=FrontID!=INDEX_NONE?Groups[FrontID].Squads.Num():0;
 // A squad the front army may take: AI-led (delegated, for the player's team), rifle or heavy, active, outside groups and missions.
 // The reserve (6c) is these squads too.
 auto IsFree=[&](int32 SID,const auto& S)
 {
  if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(SID)!=INDEX_NONE)return false;
  if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Heavy)return false;
  if(Area.HirePurpose.Contains(SID)||IntelPurpose.Contains(SID)||ArmyPinned.Contains(SID))return false;
  // The real orders never send heavy squads on the home assault (UpdateTeamOrders), so their base slot does not hold them (Codex 2026-11-21).
  bool bBase=false;AreaStrategyTarget(SID,Team,bBase);return !(bBase&&S.SquadType!=BattleSquadType::Heavy);
 };
 // After a release the fill waits ReleaseHold seconds (Codex review: a need going 16 -> 18 right after a release took the same squads back).
 if(Have<Want&&AutoFill()&&(FrontID==INDEX_NONE||Elapsed-Groups[FrontID].FrontReleaseAt>=ReleaseHold))
 {
  const FVector LineMid=BattleSectors::Center(FrontCell(Team,Line[Rows/2],Rows/2));
  struct FFree{int32 SID;double D;};TArray<FFree> Free;
  for(const auto& P:Store->GetSquads())if(IsFree(P.Key,P.Value))Free.Add({P.Key,FVector::DistSquared2D(R->Position(P.Value.LeaderUnitID),LineMid)});
  Free.Sort([](const FFree& A,const FFree& B){return A.D!=B.D?A.D<B.D:A.SID<B.SID;});
  const int32 ShareCap=FMath::CeilToInt(Share(Team)*double(Have+Free.Num())-1e-9);
  TArray<int32> Pick;for(const auto& F:Free)if(Have+Pick.Num()<FMath::Min(Want,ShareCap))Pick.Add(F.SID);
  if(FrontID==INDEX_NONE)
  {
   if(Pick.Num()>=2)
   {
    FrontID=FormGroup(Team,Pick,false);
    if(auto* G=Groups.Find(FrontID)){G->Kind=1;G->Phase=2;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_GROUP group=%d team=%d squads=%d time=%.1f"),FrontID,Team,G->Squads.Num(),Elapsed);}
   }
  }
  else if(Pick.Num())
  {
   auto& G=Groups[FrontID];
   for(int32 SID:Pick){G.Squads.Add(SID);SquadGroup.Add(SID,FrontID);}
   G.Squads.Sort();
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_JOIN group=%d team=%d joined=%d squads=%d time=%.1f ids=%s"),FrontID,Team,Pick.Num(),G.Squads.Num(),Elapsed,*FString::JoinBy(Pick,TEXT(","),[](int32 I){return FString::FromInt(I);}));
  }
 }
 // 5b. Squads above the line's need go back unassigned as a reserve (user proposal 2026-11-22: count the squads the line needs, keep
 //  those in the front army and leave the rest free to be used where needed). Unassigned, they serve the team's other orders and the
 //  player, and the fill above takes free squads back when the need grows. Released first: squads away for supply, then those furthest
 //  behind the line; the front places go last. Only above the need by more than ReleaseMargin, at most every ReleaseHold seconds,
 //  so a need that moves by one does not send squads to and fro. Also squads the player dropped on the front army tab.
 // The excess has to last ExcessHold seconds (FBattleGroup::FrontExcessSince), so a need that dips for a moment releases nobody.
 if(auto* G=FrontID!=INDEX_NONE?Groups.Find(FrontID):nullptr;G&&!(Need>0&&AutoFill()&&Have>Need+ReleaseMargin))G->FrontExcessSince=-1.f;
 else if(G&&G->FrontExcessSince<0.f)G->FrontExcessSince=Elapsed;
 if(Need>0&&AutoFill()&&Have>Need+ReleaseMargin)if(auto* G=Groups.Find(FrontID);G&&Elapsed-G->FrontReleaseAt>=ReleaseHold
  &&((G->FrontReleaseAt<0.f&&Elapsed<30.f)||Elapsed-G->FrontExcessSince>=ExcessHold))
 {
  struct FOut{int32 SID;double S;};TArray<FOut> Out;
  for(int32 SID:G->Squads)if(const auto* S=Store->FindSquad(SID))
  {
   double Rank=0.;
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))Rank=1000.;
   else if(const FVector* Post=FrontPosts.Find(SID))
   {
    const FIntPoint C=BattleSectors::Cell(*Post);
    Rank=G->FrontHold.IsValidIndex(RowOf(C))?double(G->FrontHold[RowOf(C)]-ColOf(C)):0.;
   }
   else Rank=500.;
   Out.Add({SID,Rank});
  }
  Out.Sort([](const FOut& A,const FOut& B){return A.S!=B.S?A.S>B.S:A.SID>B.SID;});
  // The first release (right after the group forms) takes every squad above the need at once, and the rest then take their first
  // places afresh by walking time (their posts of the first two seconds are dropped).
  const bool bFirst=G->FrontReleaseAt<0.f&&Elapsed<30.f;
  int32 Released=0;FString List;
  for(const FOut& O:Out)if(G->Squads.Num()>FMath::Max(1,Need)&&(bFirst||Released<ReleaseMax))
  {
   G->Squads.Remove(O.SID);SquadGroup.Remove(O.SID);ArmyPinned.Remove(O.SID);FrontPosts.Remove(O.SID);
   ++Released;List+=FString::Printf(TEXT("%s%d"),List.IsEmpty()?TEXT(""):TEXT(","),O.SID);
  }
  if(Released)
  {
   G->FrontReleaseAt=Elapsed;FrontReleased[Team]+=Released;
   if(bFirst)for(int32 SID:G->Squads)FrontPosts.Remove(SID);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_RELEASE team=%d group=%d need=%d released=%d squads=%s left=%d time=%.1f"),Team,FrontID,Need,Released,*List,G->Squads.Num(),Elapsed);
  }
 }

 // 6. Bands and posts. Squads away at home for supply leave their band to the others.
 TMap<int32,FIntPoint> Previous; // the front army keeps a squad on its last post when that sector is still wanted
 for(auto It=FrontPosts.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||S->Team==Team){if(S)Previous.Add(It.Key(),BattleSectors::Cell(It.Value()));It.RemoveCurrent();}
 FString Posts;
 TArray<int32> OldHold; // the group's line before this update's step: a squad's place behind it is kept across the step
 if(auto* G=FrontID!=INDEX_NONE?Groups.Find(FrontID):nullptr)
 {
  // 6a. The group's own line (Hold()).
  if(Hold())
  {
   auto& H=G->FrontHold;
   if(H.Num()!=Rows)
   {
    H=Line;
    // The opening raises rows up to Open, short of known enemies by two columns; a row the own soldiers already hold further on keeps
    // it (the reach line itself stops a column short of known enemies).
    const int32 Open=HomeCol+FMath::FloorToInt((EnemyHomeCol-HomeCol)*OpenPercent()/100.);
    for(int32 Y=0;Y<Rows;++Y)H[Y]=FMath::Clamp(FMath::Max(H[Y],FMath::Min(Open,EnemyCol[Y]-2)),HomeCol,FMath::Max(HomeCol,EnemyHomeCol-2));
    for(int32 Pass=0;Pass<Rows;++Pass)for(int32 Y=0;Y<Rows;++Y){if(Y>0)H[Y]=FMath::Min(H[Y],H[Y-1]+1);if(Y+1<Rows)H[Y]=FMath::Min(H[Y],H[Y+1]+1);}
    G->bFrontDrawn=false;G->FrontStepAt=Elapsed;
    FString L;for(int32 Y=0;Y<Rows;++Y)L+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),H[Y]);
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_HOLD team=%d group=%d start open=%d line=%s time=%.1f"),Team,FrontID,Open,*L,Elapsed);
   }
   OldHold=H;
   RunFrontOrderScript(Team,H,HomeCol,EnemyHomeCol);
   TMap<FIntPoint,int32> EnemyAll;for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))++EnemyAll.FindOrAdd(C);
   auto Near=[&](const TMap<FIntPoint,int32>& M,int32 Y,int32 C0,int32 C1)
   {int32 N=0;for(int32 Z=FMath::Max(0,Y-1);Z<FMath::Min(Rows,Y+2);++Z)for(int32 C=FMath::Max(0,C0);C<=FMath::Min(Cols-1,C1);++C)if(const int32* V=M.Find(FrontCell(Team,C,Z)))N+=*V;return N;};
   // Damage per row: the 5 s damage and the health of the front squads posted within one row of it, and how many were damaged.
   TArray<double> Damage,Health;TArray<int32> Hurt;Damage.Init(0.,Rows);Health.Init(0.,Rows);Hurt.Init(0,Rows);
   // Continuity: neighbouring rows differ by one column at most. Front plan (-BattleFrontPlan=1): a focus-section row may stand up to
   // three columns ahead of a neighbour outside the section, but not past its target point's column (empty InFocus: the plain rule).
   TArray<uint8> InFocus;int32 TargetCol=-1;FrontPlanFocusRows(Team,InFocus,TargetCol);
   auto Smooth=[&](TArray<int32>& A)
   {
    auto Cap=[&](int32 Y,int32 Z){int32 V=A[Z]+1;if(InFocus.Num()==Rows&&InFocus[Y]&&!InFocus[Z]&&TargetCol>=0)V=FMath::Max(V,FMath::Min(A[Z]+3,TargetCol));
     // Section orders: at the border of an advancing or falling back section the neighbouring section does not pull the rows
     // (first matches 2026-11-24: a three-column allowance let a falling back section drag the next sections back with it).
     if((SectionOrderOfRow(Team,Y)||SectionOrderOfRow(Team,Z))&&FrontRoleOfRow(Team,Y)!=FrontRoleOfRow(Team,Z))V=TNumericLimits<int32>::Max();
     return V;};
    for(int32 Pass=0;Pass<Rows;++Pass)for(int32 Y=0;Y<Rows;++Y){if(Y>0)A[Y]=FMath::Min(A[Y],Cap(Y,Y-1));if(Y+1<Rows)A[Y]=FMath::Min(A[Y],Cap(Y,Y+1));}
   };
   if(const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>())
    for(int32 SID:G->Squads)if(const FIntPoint* Last=Previous.Find(SID))if(const auto* O=Tactics->Find(SID))
     for(int32 Y=FMath::Max(0,RowOf(*Last)-1);Y<FMath::Min(Rows,RowOf(*Last)+2);++Y){Damage[Y]+=O->RecentDamage;Health[Y]+=O->Health;Hurt[Y]+=O->RecentDamage>0.f?1:0;}
   auto Share=[&](int32 Y){return Health[Y]>0.?Damage[Y]/Health[Y]:0.;};
   // A line the player drew (SetDrawnFrontLine) stays where it was drawn until the player draws again or hands it back.
   if(!G->bFrontDrawn)
   {
    int32 Back=0;
    if(Elapsed-G->FrontBackAt>=BackHold)for(int32 Y=0;Y<Rows;++Y)if(Share(Y)>=BackDamage&&H[Y]>HomeCol)
    {
     if(FrontRoleOfRow(Team,Y)==TEXT('H')){++FrontPlanState[Team].HoldBlocks;continue;} // stage 4: a hold section does not fall back
     --H[Y];++Back;
    }
    if(Back)
    {
     G->FrontBackAt=Elapsed;
     Smooth(H);
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_BACK team=%d group=%d rows=%d time=%.1f"),Team,FrontID,Back,Elapsed);
    }
    for(int32 Y=0;Y<Rows;++Y)H[Y]=FMath::Clamp(H[Y],HomeCol,FMath::Max(HomeCol,EnemyHomeCol-2));
   }
   int32 OnPosts=0,Posted=0;
   for(int32 SID:G->Squads)if(const FIntPoint* Last=Previous.Find(SID))if(const auto* S=Store->FindSquad(SID))
   {
    if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
    ++Posted;if(FVector::DistSquared2D(R->Position(S->LeaderUnitID),BattleSectors::Center(*Last))<=FMath::Square(OnPost))++OnPosts;
   }
   // The foremost column reached by each row's squads posted on the line or one column behind it (-1: none there; support places
   // further back and empty rows do not hold the row's step).
   TArray<int32> Reached;Reached.Init(-1,Rows);
   for(int32 SID:G->Squads)if(const FIntPoint* Last=Previous.Find(SID))if(const auto* S=Store->FindSquad(SID))
    if(Reached.IsValidIndex(RowOf(*Last))&&ColOf(*Last)>=OldHold[RowOf(*Last)]-1&&!(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY"))))Reached[RowOf(*Last)]=FMath::Max(Reached[RowOf(*Last)],ColOf(BattleSectors::Cell(R->Position(S->LeaderUnitID))));
   const float Since=Elapsed-G->FrontStepAt;
   if(!G->bFrontDrawn&&Posted&&Since>=StepEvery)
   {
    TArray<int32> Next=H;int32 Moved=0,Pushed=0,Ordered=0;
    for(int32 Y=0;Y<Rows;++Y)
    {
     // Section orders: the player's advance / fall back move the row toward the target line, one column per step.
     if(const TCHAR Ord=SectionOrderOfRow(Team,Y))
     {
      const int32 T=Area.FrontTarget.IsValidIndex(Y)?Area.FrontTarget[Y]:-1;
      if(Ord==TEXT('A')&&T>H[Y]&&H[Y]+1<EnemyHomeCol-1&&(Reached[Y]<0||Reached[Y]>=H[Y]-RowLag)){++Next[Y];++Moved;++Ordered;}
      else if(Ord==TEXT('B')&&H[Y]>(T>=0?FMath::Max(HomeCol,T):HomeCol)){--Next[Y];++Moved;++Ordered;} // stops on the target line
      continue;
     }
     if(!(H[Y]+1<EnemyHomeCol-1&&(Reached[Y]<0||Reached[Y]>=H[Y]-RowLag)))continue;
     if(FrontRoleOfRow(Team,Y)==TEXT('H')){++FrontPlanState[Team].HoldSteps;continue;} // stage 4: a hold section does not push out
     const int32 Own=Near(OwnCount,Y,H[Y]-1,H[Y]+1),Enemy=Near(EnemyAll,Y,H[Y],H[Y]+2);
     const bool bQuiet=Hurt[Y]==0&&!(Enemy>=SwampMin&&Enemy>=Swamp*Own);
     const bool bLight=Share(Y)<LightDamage&&Own>=PushRatio*Enemy;
     if(bQuiet||bLight){++Next[Y];++Moved;if(Enemy>0)++Pushed;}
    }
    Smooth(Next);
    if(Next!=H)
    {
     H=Next;G->FrontStepAt=Elapsed;
     FString L;for(int32 Y=0;Y<Rows;++Y)L+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),H[Y]);
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_STEP team=%d group=%d on_post=%d/%d rows=%d pushed=%d line=%s time=%.1f"),Team,FrontID,OnPosts,Posted,Moved,Pushed,*L,Elapsed);
     if(Ordered)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_ORDER_STEP team=%d rows=%d line=%s time=%.1f"),Team,Ordered,*L,Elapsed);
    }
   }
   // Front plan: an own point (observed) is the floor of its row - the line passes at or ahead of it and does not fall back behind it.
   if(!G->bFrontDrawn&&FrontPlanOn(Team))for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)==Team)
    if(const FIntPoint C=BattleSectors::Cell(Objectives[I].Position);BattleSectors::Valid(C))
     H[RowOf(C)]=FMath::Clamp(FMath::Max(H[RowOf(C)],ColOf(C)),HomeCol,FMath::Max(HomeCol,EnemyHomeCol-2));
   Line=H;FrontLine[Team]=Line;
  }
  struct FMember{int32 SID;double Row;};TArray<FMember> Present;
  for(int32 SID:G->Squads)if(const auto* S=Store->FindSquad(SID))
  {
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
   Present.Add({SID,bLow?R->Position(S->LeaderUnitID).Y:-R->Position(S->LeaderUnitID).Y}); // side to side in the team's own row order
  }
  Present.Sort([](const FMember& A,const FMember& B){return A.Row!=B.Row?A.Row<B.Row:A.SID<B.SID;});
  const int32 K=Present.Num();
  // Bands: one per squad of the group with Hold() (a squad away for supply keeps its band empty, so the others keep their posts).
  const int32 NB=Hold()?FMath::Max(K,G->Squads.Num()):K;
  // Front army: sectors with a point not observed as own (it takes the point as it holds the line), and one post per sector.
  TSet<FIntPoint> Used,Points;TArray<FIntPoint> Cells;TArray<bool> Pushes;
  if(Army())for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)!=Team)if(const FIntPoint C=BattleSectors::Cell(Objectives[I].Position);BattleSectors::Valid(C))Points.Add(C);
  // Hold(): places on the group's line chosen from cover (user proposals 2026-11-22: "decide from cover what shape the line should take;
  // when one-sector gaps still hold the line, the fields need no soldiers"; earlier the second squad of a row kept its old sector until it
  // fell three columns behind, then ran past the first one, so squads 8 and 14 went back and forth).
  //  - Each row's front place: its line column, or the column right behind it when that one gives more cover (houses, woods, hills).
  //  - A row whose front place is open ground (fields) stays empty when the rows on both sides (the one row beside an edge row) hold
  //    covered places and no enemy is known near it; never two empty rows side by side. An empty row is taken again only when an enemy is
  //    known right in front of it, while leaving a row needs a wider clear zone, so a sighting coming and going does not move squads to and fro.
  //  - The other squads: support places one to three columns behind the line, best cover first, rows facing known enemies first, at most
  //    two per row. All places move with the line, so the squads keep their rows and none runs past another.
  TArray<int32> SlotRow;FString Gaps;int32 FrontCount=0;
  // Where each squad's last place went with the line (its last place shifted by the row's step).
  TSet<FIntPoint> Kept;
  if(Hold())for(int32 SID:G->Squads)if(const FIntPoint* Last=Previous.Find(SID);Last&&OldHold.IsValidIndex(RowOf(*Last))&&Line.IsValidIndex(RowOf(*Last)))
   Kept.Add(FrontCell(Team,FMath::Clamp(ColOf(*Last)+Line[RowOf(*Last)]-OldHold[RowOf(*Last)],0,Cols-1),RowOf(*Last)));
  if(Hold())
  {
   auto Known=[&](int32 Y,int32 C0,int32 C1)
   {int32 N=0;for(int32 Z=FMath::Max(0,Y-1);Z<FMath::Min(Rows,Y+2);++Z)for(int32 C=FMath::Max(0,C0);C<=FMath::Min(Cols-1,C1);++C)if(const int32* V=EnemyCount.Find(FrontCell(Team,C,Z)))N+=*V;return N;};
   // Impassable sectors (mountain M, lake L on map J; strategy branch 2026-11-22) are never places: -1000.
   auto CoverAt=[&](int32 C,int32 Y)
   {
    const FIntPoint Cell=FrontCell(Team,C,Y);if(!BattleSectors::Valid(Cell))return -100.;
    const char Kind=BattleFieldLayout::SectorKind(GetWorld(),Cell);return BattleSectors::Impassable(Kind)&&Kind!='R'?-1000.:Cover(Kind);
   };
   TArray<int32> FrontCol;TArray<double> FrontCover;FrontCol.Init(0,Rows);FrontCover.Init(0.,Rows);
   for(int32 Y=0;Y<Rows;++Y)
   {
    FrontCol[Y]=FMath::Max(HomeCol,Line[Y]);
    if(Line[Y]-1>=HomeCol&&CoverAt(Line[Y]-1,Y)>CoverAt(FrontCol[Y],Y)+.6)FrontCol[Y]=Line[Y]-1;
    FrontCover[Y]=CoverAt(FrontCol[Y],Y);
   }
   TArray<uint8> Empty;Empty.Init(0,Rows);
   const bool bOldGaps=G->FrontGap.Num()==Rows;
   for(int32 Y=0;Y<Rows&&Rows>1;++Y)if(FrontCover[Y]<=0.)
   {
    const bool bA=Y==0||(FrontCover[Y-1]>0.&&!Empty[Y-1]),bB=Y==Rows-1||FrontCover[Y+1]>0.;
    const bool bClear=bOldGaps&&G->FrontGap[Y]?Known(Y,Line[Y],Line[Y]+2)==0:Known(Y,Line[Y]-1,Line[Y]+4)==0;
    // A row with no passable front place is left whatever its neighbours.
    if((bA&&bB&&bClear)||FrontCover[Y]<=-1000.){Empty[Y]=1;Gaps+=FString::Printf(TEXT("%s%d"),Gaps.IsEmpty()?TEXT(""):TEXT(","),Y+1);}
   }
   // Stage 4: a may-fall-back section holds every second row (half a squad per row), unless an enemy is known right in front of the row.
   if(FrontRolesOn(Team))
   {
    const FFrontPlanTeam& FP=FrontPlanState[Team];int32 Thin=0;
    for(int32 Y=0;Y<Rows;++Y)if(!Empty[Y]&&FrontRoleOfRow(Team,Y)==TEXT('R')&&FP.SectionFirst.IsValidIndex(FP.SectionOfRow[Y])&&(Y-FP.SectionFirst[FP.SectionOfRow[Y]])%2==1&&Known(Y,Line[Y],Line[Y]+1)==0)
    {Empty[Y]=1;++Thin;Gaps+=FString::Printf(TEXT("%s%d"),Gaps.IsEmpty()?TEXT(""):TEXT(","),Y+1);}
    FrontPlanState[Team].ThinNow=Thin;
   }
   G->FrontGap=Empty;
   // The line's need: a front place per held row, a support place per row with a known enemy in front of it, and two more.
   {
    int32 Contact=0;
    for(int32 Y=0;Y<Rows;++Y){int32 N=0;for(int32 C=Line[Y];C<=FMath::Min(Cols-1,Line[Y]+3);++C)if(const int32* V=EnemyCount.Find(FrontCell(Team,C,Y)))N+=*V;Contact+=N>0?1:0;}
    int32 Held=0;for(int32 Y=0;Y<Rows;++Y)Held+=Empty[Y]?0:1;
    int32 HoldSecs=0;if(FrontRolesOn(Team))for(const TCHAR Role:FrontPlanState[Team].Role)HoldSecs+=Role==TEXT('H')?1:0; // stage 4: a reserve squad per hold section
    G->FrontNeed=FMath::Clamp(Held+Contact+2+HoldSecs,2,2*Rows);
   }
   // Front places (with fewer squads than rows, the rows spread evenly among the squads).
   TArray<int32> FrontRows;for(int32 Y=0;Y<Rows;++Y)if(!Empty[Y])FrontRows.Add(Y);
   if(NB<FrontRows.Num()){TArray<int32> Spread;for(int32 I=0;I<NB;++I)Spread.Add(FrontRows[(2*I+1)*FrontRows.Num()/(2*NB)]);FrontRows=Spread;}
   for(int32 Y:FrontRows){const FIntPoint Cell=FrontCell(Team,FrontCol[Y],Y);Used.Add(Cell);Cells.Add(Cell);Pushes.Add(false);SlotRow.Add(Y);}
   FrontCount=Cells.Num();
   // Support places.
   struct FSpot{double S;int32 C,Y;};TArray<FSpot> Spots;
   for(int32 Y=0;Y<Rows;++Y)for(int32 D=1;D<=3;++D)if(const int32 C=Line[Y]-D;C>=HomeCol&&!Used.Contains(FrontCell(Team,C,Y))&&CoverAt(C,Y)>-1000.&&FrontRoleOfRow(Team,Y)!=TEXT('R'))
    Spots.Add({CoverAt(C,Y)+(D==1?.5:D==3?-1.:0.)+.5*FMath::Min(Known(Y,Line[Y],Line[Y]+3),4)+(Empty[Y]?.5:0.)+(FrontRoleOfRow(Team,Y)==TEXT('H')?2.:0.)
     +(Kept.Contains(FrontCell(Team,C,Y))?1.5:0.),C,Y}); // a support place a squad already holds stays (map I 2026-11-22: supports hopped rows with each sighting)
   Spots.Sort([](const FSpot& A,const FSpot& B){return A.S!=B.S?A.S>B.S:A.Y!=B.Y?A.Y<B.Y:A.C>B.C;});
   // Support squads per row: one each first (rows facing enemies first by the score), then a second, then any (Codex review: the cap
   // of two let one row take three squads before the others had one).
   TArray<int32> PerRow;PerRow.Init(0,Rows);
   for(int32 Pass=0;Pass<3&&Cells.Num()<NB;++Pass)for(const FSpot& P:Spots)if(Cells.Num()<NB)
   {
    const FIntPoint Cell=FrontCell(Team,P.C,P.Y);if(Used.Contains(Cell)||(Pass<2&&PerRow[P.Y]>Pass))continue;
    Used.Add(Cell);Cells.Add(Cell);Pushes.Add(false);SlotRow.Add(P.Y);++PerRow[P.Y];
   }
   // Still short (a line next to the home): any free sector further behind, then shared places.
   for(int32 Y=0;Y<Rows&&Cells.Num()<NB;++Y)for(int32 C=Line[Y]-4;C>=0&&Cells.Num()<NB;--C)
    if(const FIntPoint Cell=FrontCell(Team,C,Y);BattleSectors::Valid(Cell)&&!Used.Contains(Cell)&&CoverAt(C,Y)>-1000.){Used.Add(Cell);Cells.Add(Cell);Pushes.Add(false);SlotRow.Add(Y);}
   for(int32 I=0;Cells.Num()<NB&&Cells.Num()>0;++I){const FIntPoint Copy=Cells[I];const int32 CopyRow=SlotRow[I];Cells.Add(Copy);Pushes.Add(false);SlotRow.Add(CopyRow);} // copies: TArray::Add rejects its own elements (Codex review)
  }
  else for(int32 I=0;I<NB;++I)
  {
   const int32 Y0=I*Rows/NB,Y1=FMath::Max(Y0+1,(I+1)*Rows/NB);
   // The band's line: its rearmost row. Step forward when no enemy is known within ClearAhead columns ahead of the band (rows +-1).
   int32 Col=Cols;for(int32 Y=Y0;Y<Y1;++Y)Col=FMath::Min(Col,Line[Y]);
   int32 Nearest=Cols;for(int32 Y=FMath::Max(0,Y0-1);Y<FMath::Min(Rows,Y1+1);++Y)Nearest=FMath::Min(Nearest,EnemyCol[Y]);
   const bool bPush=!Hold()&&Nearest-Col>ClearAhead&&Col+1<EnemyHomeCol-1; // Hold(): the group steps the whole line (6a)
   const int32 Target=bPush?Col+1:Col;
   const double Mid=(Y0+Y1-1)*.5;
   double BestScore=-1e9;FIntPoint Best=FrontCell(Team,Target,FMath::RoundToInt(Mid));
   // A pushing band picks among its forward column only (Codex review 2026-11-21: cover behind outscored the step forward, so it never moved).
   // The front army may go two columns back when the forward sectors of a shared row are taken (a pushing band pays for each step back).
   for(int32 Y=Y0;Y<Y1;++Y)for(int32 C=FMath::Max(HomeCol,Army()?Target-2:bPush?Target:Target-1);C<=Target;++C)
   {
    const FIntPoint Cell=FrontCell(Team,C,Y);if(!BattleSectors::Valid(Cell)||Used.Contains(Cell))continue;
    // Map J (2026-11-22): never a post in a mountain or a lake (an order there is refused); rivers already score -100.
    if(BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Cell))&&BattleFieldLayout::SectorKind(GetWorld(),Cell)!='R')continue;
    double CellScore=Cover(BattleFieldLayout::SectorKind(GetWorld(),Cell))+(C==Target?1.5:0.)-.3*FMath::Abs(Y-Mid);
    if(Army())CellScore+=(Points.Contains(Cell)?2.:0.)-(C<Target-1?1.:0.)-(bPush&&C<Target?3.:0.);
    if(CellScore>BestScore){BestScore=CellScore;Best=Cell;}
   }
   // No candidate (a band of mountain rows on map J): the nearest passable sector of the band towards the own home.
   // Only when the default would be a mountain or a lake, so maps without them keep the earlier choice exactly.
   if(BestScore<=-1e9+1&&(BattleFieldLayout::SectorKind(GetWorld(),Best)=='M'||BattleFieldLayout::SectorKind(GetWorld(),Best)=='L'))
    for(int32 C=Target-1;C>=0&&BestScore<=-1e9+1;--C)for(int32 Y=Y0;Y<Y1;++Y)
    {const FIntPoint Cell=FrontCell(Team,C,Y);if(BattleSectors::Valid(Cell)&&!Used.Contains(Cell)&&!BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),Cell))){BestScore=0;Best=Cell;break;}}
   Used.Add(Best);Cells.Add(Best);Pushes.Add(bPush);
  }
  // Squads to posts. The front army: a squad keeps its last post when that sector is still one of them, the rest go to the nearest free
  // post (user report 2026-11-21: squads went back and forth because bands followed the squads' row order, which changes as they move).
  TArray<int32> Owner;Owner.Init(INDEX_NONE,NB);
  bool bAnyLast=false;for(const auto& M:Present)bAnyLast|=Previous.Contains(M.SID);
  if(Hold()&&!bAnyLast&&K>0)
  {
   // First posting by walking time (user proposal 2026-11-22: which squad should go where to be in place soonest). The front places
   // come first in Cells. Squads and places are matched so that the last squad arrives as early as possible (the longest walk is the
   // least), and under that limit so that the walks are the shortest in sum (such matchings also keep the paths from crossing).
   TArray<TArray<double>> D;D.SetNum(K);
   for(int32 I=0;I<K;++I)if(const auto* S=Store->FindSquad(Present[I].SID))
   {const FVector P=R->Position(S->LeaderUnitID);for(int32 J=0;J<K;++J)D[I].Add(FVector::Dist2D(P,BattleSectors::Center(Cells[J])));}
    else D[I].Init(0.,K);
   const TArray<int32> Slot=AssignMinTime(D);
   for(int32 I=0;I<K;++I)if(Slot.IsValidIndex(I)&&Slot[I]!=INDEX_NONE)Owner[Slot[I]]=I;
  }
  else if(Hold())
  {
   // A squad keeps its row (cost: rows apart first), then the place nearest to where its last place went with the line: the last place
   // shifted by the row's step (Codex review: by the last place itself, the front squad took the support place it was standing next to
   // after a step, and the support squad ran past it to the front).
   TArray<bool> Placed;Placed.Init(false,K);
   struct FPair{double D;int32 I,J;};TArray<FPair> Pairs;
   for(int32 I=0;I<K;++I)if(const auto* S=Store->FindSquad(Present[I].SID))
   {
    const FIntPoint* Last=Previous.Find(Present[I].SID);const bool bLead=FrontLeads.Contains(Present[I].SID);
    const int32 LastRow=Last?RowOf(*Last):0;
    const int32 Shift=Last&&OldHold.IsValidIndex(LastRow)&&Line.IsValidIndex(LastRow)?Line[LastRow]-OldHold[LastRow]:0;
    const FVector Expected=Last?BattleSectors::Center(FrontCell(Team,FMath::Clamp(ColOf(*Last)+Shift,0,Cols-1),LastRow)):FVector::ZeroVector;
    // Stage 7 reservation destination: a bought squad sent to a section takes a post there first.
    const int32* Wish=Last?nullptr:FrontSectionWish.Find(Present[I].SID);
    const auto& WishPlan=FrontPlanState[Team];
    for(int32 J=0;J<NB;++J)
     Pairs.Add({Last?1.e13*FMath::Abs(SlotRow[J]-LastRow)+(bLead!=(J<FrontCount)?1.e11:0.)+FVector::DistSquared2D(Expected,BattleSectors::Center(Cells[J]))
      :1.e15+(Wish&&!(WishPlan.SectionOfRow.IsValidIndex(SlotRow[J])&&WishPlan.SectionOfRow[SlotRow[J]]==*Wish)?1.e14:0.)+FVector::DistSquared2D(R->Position(S->LeaderUnitID),BattleSectors::Center(Cells[J])),I,J});
   }
   Pairs.Sort([](const FPair& A,const FPair& B){return A.D!=B.D?A.D<B.D:A.I!=B.I?A.I<B.I:A.J<B.J;});
   for(const auto& Pair:Pairs)if(!Placed[Pair.I]&&Owner[Pair.J]==INDEX_NONE){Owner[Pair.J]=Pair.I;Placed[Pair.I]=true;}
  }
  else if(Army())
  {
   TArray<bool> Placed;Placed.Init(false,K);
   for(int32 I=0;I<K;++I)if(const FIntPoint* Last=Previous.Find(Present[I].SID))
    for(int32 J=0;J<NB;++J)if(Owner[J]==INDEX_NONE&&Cells[J]==*Last){Owner[J]=I;Placed[I]=true;break;}
   struct FPair{double D;int32 I,J;};TArray<FPair> Pairs;
   // Hold(): a squad that had a post keeps its row when the line moves (test play 19:10: after each step squads were sent across the
   // map, e.g. row 7 to row 1, because the nearest-post choice went by where they stood while still on the way). Cost: rows apart first.
   for(int32 I=0;I<K;++I)if(!Placed[I])if(const auto* S=Store->FindSquad(Present[I].SID))
   {
    const FVector P=R->Position(S->LeaderUnitID);const FIntPoint* Last=Hold()?Previous.Find(Present[I].SID):nullptr;
    for(int32 J=0;J<NB;++J)if(Owner[J]==INDEX_NONE)
     Pairs.Add({Last?1.e12*FMath::Abs(Cells[J].Y-Last->Y)+FVector::DistSquared2D(BattleSectors::Center(*Last),BattleSectors::Center(Cells[J])):1.e14+FVector::DistSquared2D(P,BattleSectors::Center(Cells[J])),I,J});
   }
   Pairs.Sort([](const FPair& A,const FPair& B){return A.D!=B.D?A.D<B.D:A.I!=B.I?A.I<B.I:A.J<B.J;});
   for(const auto& Pair:Pairs)if(!Placed[Pair.I]&&Owner[Pair.J]==INDEX_NONE){Owner[Pair.J]=Pair.I;Placed[Pair.I]=true;}
  }
  else for(int32 J=0;J<K;++J)Owner[J]=J;
  // Front or support: a squad keeps its role across steps (map I 2026-11-22: where a row's front place went back a column into cover,
  // the support squad behind took it and the front squad moved to another row).
  if(Hold()){for(int32 SID:G->Squads)FrontLeads.Remove(SID);for(int32 J=0;J<FrontCount&&J<NB;++J)if(Owner[J]!=INDEX_NONE)FrontLeads.Add(Present[Owner[J]].SID);}
  for(int32 J=0;J<NB;++J)if(Owner[J]!=INDEX_NONE)
  {
   const int32 SID=Present[Owner[J]].SID;
   FrontPosts.Add(SID,BattleSectors::Center(Cells[J]));
   FrontSectionWish.Remove(SID); // the first post was given
   Posts+=FString::Printf(TEXT(" %d:%c%d%s"),SID,TCHAR('A'+Cells[J].X),1+Cells[J].Y,Pushes[J]?TEXT("+"):TEXT(""));
  }
  if(!Gaps.IsEmpty())Posts+=FString::Printf(TEXT(" gaps=%s"),*Gaps); // rows left empty (1-based)
 }
 // 6c. Reserve behind the front (trial -BattleFrontReserve=1, user request 2026-11-22 after test play: unassigned squads went back and
 //  forth in place, as the team's point allocation gave them another point now and then). The free squads the front army could take (the
 //  fill above takes them first when the need grows; an attack group may take them too) wait behind its line instead: each keeps a row of
 //  its own (a new one takes the nearest row among those with the fewest reserve squads) and waits in the sector with more cover two or
 //  three columns behind the line in that row, so it moves only when the line steps there.
 for(auto It=FrontReservePosts.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||S->Team==Team)It.RemoveCurrent();
 // Measurement (log only, with and without the reserve): how the free squads move, sampled at each update; a reversal is a step against
 // the squad's previous step (both over 3 m). A squad that leaves the free squads starts afresh when it comes back.
 if(Hold()&&FrontID!=INDEX_NONE)
 {
  TSet<int32> Seen;
  for(const auto& P:Store->GetSquads())if(IsFree(P.Key,P.Value))
  {
   Seen.Add(P.Key);const FVector Now=R->Position(P.Value.LeaderUnitID);
   if(const FVector* Was=FreeLastPos.Find(P.Key))
   {
    const FVector Step(Now.X-Was->X,Now.Y-Was->Y,0.);
    if(Step.SizeSquared2D()>FMath::Square(300.))
    {
     FreeMoved[Team]+=Step.Size2D();
     if(const FVector* Prev=FreeLastStep.Find(P.Key);Prev&&FVector::DotProduct(*Prev,Step)<0.)++FreeReversals[Team];
     FreeLastStep.Add(P.Key,Step);
    }
   }
   FreeLastPos.Add(P.Key,Now);
  }
  for(auto It=FreeLastPos.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||(S->Team==Team&&!Seen.Contains(It.Key()))){FreeLastStep.Remove(It.Key());It.RemoveCurrent();}
  // Crowding (log only, crowd=): the team's infantry squads whose leader shares a sector with another of them, summed per update.
  TMap<FIntPoint,int32> PerCell;
  // Across the map (log only, lat_front= / lat_other=): the same squads per row of the team's own order, front army apart from the rest.
  if(LatFront[Team].Num()!=Rows){LatFront[Team].Init(0,Rows);LatOther[Team].Init(0,Rows);}
  for(const auto& P:Store->GetSquads())if(P.Value.Team==Team&&P.Value.CommandStatus==EBattleCommandStatus::Active&&!BattleSquadType::IsVehicle(P.Value.SquadType)&&!BattleSquadType::IsArtillery(P.Value.SquadType))
   if(const auto* L=Store->Find(P.Value.LeaderUnitID);L&&L->IsAlive())
   {
    const FIntPoint C=BattleSectors::Cell(R->Position(P.Value.LeaderUnitID));++PerCell.FindOrAdd(C);
    if(BattleSectors::Valid(C)&&LatFront[Team].IsValidIndex(RowOf(C)))++(GroupOf(P.Key)==FrontID&&FrontID!=INDEX_NONE?LatFront[Team]:LatOther[Team])[RowOf(C)];
   }
  for(const auto& C:PerCell)SectorCrowd[Team]+=FMath::Max(0,C.Value-1);
  // Width (log only, lanes=): the rows (across the map) with an own infantry squad at the line or up to two columns behind it, summed per update.
  if(Line.Num()==Rows){TSet<int32> Lanes;for(const auto& C:PerCell){const int32 Y=RowOf(C.Key),X=ColOf(C.Key);if(Line.IsValidIndex(Y)&&X>=Line[Y]-2&&X<=Line[Y]+1)Lanes.Add(Y);}FrontLanes[Team]+=Lanes.Num();}
 }
 if(Reserve()&&FrontID!=INDEX_NONE&&Line.Num()==Rows)
 {
  // The waiting column of each row (INDEX_NONE: none passable there).
  TArray<int32> WaitCol;WaitCol.Init(INDEX_NONE,Rows);
  for(int32 Y=0;Y<Rows;++Y)
  {
   double Best=-1.e9;
   for(int32 C=FMath::Max(0,Line[Y]-2);C>=0;--C)
   {
    const FIntPoint Cell=FrontCell(Team,C,Y);if(!BattleSectors::Valid(Cell))continue;
    const char Kind=BattleFieldLayout::SectorKind(GetWorld(),Cell);if(BattleSectors::Impassable(Kind)&&Kind!='R')continue;
    const double WaitScore=Cover(Kind)+(C==Line[Y]-2?.3:0.)-(C<Line[Y]-3?10.*(Line[Y]-3-C):0.); // further back only where nothing nearer is passable
    if(WaitScore>Best){Best=WaitScore;WaitCol[Y]=C;}
    if(C<=Line[Y]-3&&Best>-50.)break;
   }
  }
  TArray<int32> Waiting;for(const auto& P:Store->GetSquads())if(IsFree(P.Key,P.Value))Waiting.Add(P.Key);
  Waiting.Sort();
  for(auto It=FrontReserveRow.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||(S->Team==Team&&!Waiting.Contains(It.Key())))It.RemoveCurrent();
  TArray<int32> Count;Count.Init(0,Rows);
  for(int32 SID:Waiting)if(const int32* Row=FrontReserveRow.Find(SID))
  {if(WaitCol.IsValidIndex(*Row)&&WaitCol[*Row]!=INDEX_NONE)++Count[*Row];else FrontReserveRow.Remove(SID);}
  FString ReserveList;
  for(int32 SID:Waiting)
  {
   if(!FrontReserveRow.Contains(SID))if(const auto* S=Store->FindSquad(SID))
   {
    const FVector P=R->Position(S->LeaderUnitID);double Best=1.e30;int32 BestRow=INDEX_NONE;
    for(int32 Y=0;Y<Rows;++Y)if(WaitCol[Y]!=INDEX_NONE)
    {const double Cost=1.e12*Count[Y]+FVector::DistSquared2D(P,BattleSectors::Center(FrontCell(Team,WaitCol[Y],Y)));if(Cost<Best){Best=Cost;BestRow=Y;}}
    if(BestRow!=INDEX_NONE){FrontReserveRow.Add(SID,BestRow);++Count[BestRow];}
   }
   if(const int32* Row=FrontReserveRow.Find(SID))
   {
    const FIntPoint Cell=FrontCell(Team,WaitCol[*Row],*Row);FrontReservePosts.Add(SID,BattleSectors::Center(Cell));
    ReserveList+=FString::Printf(TEXT("%s%d:%c%d"),ReserveList.IsEmpty()?TEXT(""):TEXT(","),SID,TCHAR('A'+Cell.X),1+Cell.Y);
   }
  }
  if(!ReserveList.IsEmpty())Posts+=FString::Printf(TEXT(" reserve=%s"),*ReserveList);
 }
 // 7. The surprise group (trial -BattleSurpriseGroup=1).
 UpdateSurpriseGroup(Team,FrontID);
 if(Elapsed>=FrontLineLogAt[Team])
 {
  FrontLineLogAt[Team]=Elapsed+LogInterval;
  FString Cells;for(int32 Y=0;Y<Rows;++Y)Cells+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),Line[Y]);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_LINE time=%.0f team=%d line=%s group=%d free_rev=%d free_moved_m=%.0f crowd=%d lanes=%d road=%d lat_front=%s lat_other=%s posts=%s"),Elapsed,Team,*Cells,FrontID,FreeReversals[Team],FreeMoved[Team]/100.,SectorCrowd[Team],FrontLanes[Team],RoadSteps[Team],*FString::JoinBy(LatFront[Team],TEXT(","),[](int32 V){return FString::FromInt(V);}),*FString::JoinBy(LatOther[Team],TEXT(","),[](int32 V){return FString::FromInt(V);}),Posts.IsEmpty()?TEXT(" none"):*Posts);
 }
}

int32 UBattleMatch::SetDrawnFrontLine(int32 Team,const TArray<FVector>& Points)
{
 using namespace BattleFrontValues;
 if(Team<0||Team>1||!FrontLineOn(Team)||!Hold())return -1;
 int32 FrontID=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==1&&!P.Value.bPlayer)FrontID=P.Key;
 auto* G=Groups.Find(FrontID);if(!G)return -1;
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 if(Points.Num()<2)
 {
  if(FrontSectionOrdersOn(Team))Area.FrontTarget.Reset();
  if(G->bFrontDrawn){G->bFrontDrawn=false;G->FrontStepAt=Elapsed;}
  FrontLineAt[Team]=Elapsed;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT FrontLine team=%d handed_back time=%.1f"),Team,Elapsed);
  return 0;
 }
 const bool bLow=AreaHome(Team).X<0;
 auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 const int32 HomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(Team))),0,Cols-1);
 const int32 EnemyHomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(1-Team))),0,Cols-1);
 TArray<int32> H=G->FrontHold.Num()==Rows?G->FrontHold:FrontLine[Team].Num()==Rows?FrontLine[Team]:TArray<int32>();
 if(H.Num()!=Rows)H.Init(HomeCol+1,Rows);
 int32 Set=0;TArray<int32> DrawnCol;DrawnCol.Init(-1,Rows);
 for(int32 Y=0;Y<Rows;++Y)
 {
  // Where the drawn line crosses the middle of the row (the mean of all its crossings).
  const double RowY=BattleSectors::Center(FrontCell(Team,0,Y)).Y;double SumX=0;int32 N=0;
  for(int32 I=0;I+1<Points.Num();++I)
  {
   const FVector& A=Points[I];const FVector& B=Points[I+1];
   if(FMath::IsNearlyEqual(A.Y,B.Y)||RowY<FMath::Min(A.Y,B.Y)||RowY>FMath::Max(A.Y,B.Y))continue;
   SumX+=FMath::Lerp(A.X,B.X,(RowY-A.Y)/(B.Y-A.Y));++N;
  }
  if(!N)continue;
  const FIntPoint C=BattleSectors::Cell(FVector(SumX/N,RowY,0.));
  H[Y]=FMath::Clamp(ColOf(FIntPoint(FMath::Clamp(C.X,0,Cols-1),C.Y)),HomeCol,FMath::Max(HomeCol,EnemyHomeCol-1));++Set;DrawnCol[Y]=H[Y];
 }
 if(!Set)return -2;
 if(FrontSectionOrdersOn(Team))
 {
  // Section orders: the drawn line is the target of the advancing / falling back sections; the front army's line does not jump.
  if(Area.FrontTarget.Num()!=Rows)Area.FrontTarget.Init(-1,Rows);
  FString T;
  for(int32 Y=0;Y<Rows;++Y){if(DrawnCol[Y]>=0)Area.FrontTarget[Y]=FMath::Min(DrawnCol[Y],FMath::Max(HomeCol,EnemyHomeCol-2));T+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),Area.FrontTarget[Y]);}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT FrontTarget team=%d rows=%d target=%s time=%.1f"),Team,Set,*T,Elapsed);
  return Set;
 }
 G->FrontHold=H;G->bFrontDrawn=true;FrontLine[Team]=H;FrontLineAt[Team]=Elapsed;
 FString L;for(int32 Y=0;Y<Rows;++Y)L+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),H[Y]);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT FrontLine team=%d drawn rows=%d line=%s time=%.1f"),Team,Set,*L,Elapsed);
 return Set;
}

bool UBattleMatch::FrontReserveOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const FVector* Post=FrontReservePosts.Find(SquadID);if(!Post)return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);if(!S||S->Team!=Team||(Team==0&&!S->bPlayerDelegated))return false;
 Center=FVector(Post->X,Post->Y,AreaHome(Team).Z);Order=EBattleOrder::AttackMove; // like the front posts: fights what comes and holds there
 return true;
}

bool UBattleMatch::FrontGroupOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const FVector* Post=FrontPosts.Find(SquadID);if(!Post)return false;
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);if(!S||S->Team!=Team)return false;
 // An attack-move order: the squad fights what comes within range on the way and holds the sector (as the other groups do).
 Center=FVector(Post->X,Post->Y,AreaHome(Team).Z);Order=EBattleOrder::AttackMove;
 return true;
}

// Surprise group (2026-11-20, user request; trial -BattleSurpriseGroup=1, needs -BattleFrontLine=1). Once the team has a front-holding
// group, every SurpriseInterval seconds without one it forms a surprise group of -BattleSurpriseSquads=n (default 2) free AI-led squads:
// armoured ones first (tank, armoured car, exoskeleton soldiers; user proposal 2026-11-20: speed and armour carry it through), at
// least one of them, the rest full-strength rifle or heavy squads. It takes the outer row (north or south edge) with fewer known enemies ahead of the line, gathers just behind the
// line there (phase 0), moves along that row two columns at a time (phase 1), and from two columns before the enemy home strikes the
// enemy home (phase 2). It ends when it lost more than half its soldiers or AssembleMax+Max seconds after it formed; its squads then return to the
// normal allocation. Inputs: own positions, known enemies (AreaIntelView), the line, fixed homes.
namespace BattleSurpriseValues
{
 constexpr float Interval=240.f, Max=420.f, AssembleMax=60.f, FirstAt=120.f /* the first one after the opening */, LagWait=40.f;
 constexpr double LossEnd=.5;
 constexpr float ArmourHealth=.6f; // the least health share of an armoured squad taken into a surprise group
}

// Fixed map geometry only. H uses the same 10 m bank strips as its generator; I has two rotated river rectangles.
// Treat even a direct bridge crossing as a staged crossing, so the group never advances its goal halfway over the water.
namespace BattleSurpriseRoute
{
 bool Intersects(const FVector& A,const FVector& B,double X0,double X1,double Y0,double Y1)
 {
  double Lo=0.,Hi=1.;
  auto Clip=[&](double P,double D,double Min,double Max){if(FMath::Abs(D)<1.e-9)return P>=Min&&P<=Max;double T0=(Min-P)/D,T1=(Max-P)/D;if(T0>T1)Swap(T0,T1);Lo=FMath::Max(Lo,T0);Hi=FMath::Min(Hi,T1);return Lo<=Hi;};
  return Clip(A.X,B.X-A.X,X0,X1)&&Clip(A.Y,B.Y-A.Y,Y0,Y1);
 }
 bool Bridge(const UWorld* World,const FVector& From,const FVector& Goal,int32 Row,FVector& Entry,FVector& Exit)
 {
  const auto& Def=BattleFieldLayout::Of(World);double Best=DBL_MAX;
  const double EdgeY=BattleSectors::Center(FIntPoint(0,Row)).Y;
  auto Offer=[&](double X0,double X1,double Y)
  {
   const double Direction=Goal.X>=From.X?1.:-1.;
   const FVector A(Direction>0?X0:X1,Y,Goal.Z),B(Direction>0?X1:X0,Y,Goal.Z);
   const double Cost=FVector::Dist2D(From,A)+FVector::Dist2D(B,Goal)+FMath::Abs(Y-EdgeY);
   if(Cost<Best){Best=Cost;Entry=A;Exit=B;}
  };
  // Map L has the rivers of map I.
  if(FCString::Strcmp(Def.Name,TEXT("Lvl_BattleWide_I"))==0||FCString::Strcmp(Def.Name,TEXT("Lvl_BattleWide_L"))==0)
  {
   using namespace BattleMapI;
   for(double Sign:{-1.,1.})
   {
    const double X0=Sign>0?RiverX0:-RiverX1,X1=Sign>0?RiverX1:-RiverX0,Y0=Sign>0?RiverY0:-RiverY1,Y1=Sign>0?RiverY1:-RiverY0;
    if(Intersects(From,Goal,X0,X1,Y0,Y1))Offer(X0-1000.,X1+1000.,Sign*BridgeY);
   }
  }
  else if(FCString::Strcmp(Def.Name,TEXT("Lvl_BattleWide_H"))==0)
  {
   bool bCross=false;for(int32 Y=-34500;Y<35000&&!bCross;Y+=1000){const double X=BattleSectors::RiverX(Y);bCross=Intersects(From,Goal,X-500.,X+500.,Y-500.,Y+500.);}
   if(bCross)for(int32 Y:BattleSectors::Crossings)if(!BattleSectors::IsFord(Y)){const double X=BattleSectors::RiverX(Y);Offer(X-2500.,X+2500.,Y);}
  }
  return Best<DBL_MAX;
 }
}

bool UBattleMatch::UpdateSurpriseBridge(FBattleGroup& G,const FVector& Lead)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 // The guide: the living squad nearest the lead, armour not in Recover first (Codex review 2026-11-21: a guide in Recover drove the
 // bridge legs from behind the river).
 auto PickGuide=[&](const FVector& Near)
 {
  int32 Best=INDEX_NONE,Rank=INT32_MAX;double BestD=DBL_MAX;
  for(int32 SID:G.Squads)if(const auto* S=Store->FindSquad(SID))if(const auto* U=Store->Find(S->LeaderUnitID);U&&U->IsAlive())
  {
   const bool bArmour=S->SquadType==BattleSquadType::Tank||S->SquadType==BattleSquadType::ArmoredCar||S->SquadType==BattleSquadType::Exo;
   const int32 K=(S->TacticalPlan.Action==EBattleSquadTactic::Recover?2:0)+(bArmour?0:1);
   const double D=FVector::DistSquared2D(R->Position(S->LeaderUnitID),Near);
   if(K<Rank||(K==Rank&&D<BestD)){Rank=K;BestD=D;Best=SID;}
  }
  return Best;
 };
 if(G.SurpriseBridgeStage==0)
 {
  FVector Entry,Exit;if(!BattleSurpriseRoute::Bridge(GetWorld(),Lead,G.Rally,FrontCell(G.Team,0,G.SurpriseRow).Y,Entry,Exit))return false; // the grid row of the team's row
  G.SurpriseBridgeGoal=G.Rally;G.SurpriseBridgeEntry=Entry;G.SurpriseBridgeExit=Exit;G.SurpriseBridgeStage=1;G.SurpriseBridgeLeader=PickGuide(Lead);G.SurpriseBridgeAt=Elapsed;
  G.Rally=Entry;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_BRIDGE group=%d team=%d stage=entry entry=(%.0f,%.0f) exit=(%.0f,%.0f) goal=(%.0f,%.0f) time=%.1f"),G.ID,G.Team,Entry.X,Entry.Y,Exit.X,Exit.Y,G.SurpriseBridgeGoal.X,G.SurpriseBridgeGoal.Y,Elapsed);
  return true;
 }
 // Keep one guide through the turn; changing to the most forward column on the far bank can skip the entrance.
 // A guide that died, left or went into Recover is replaced by the best one near the lead.
 {
  const auto* S=Store->FindSquad(G.SurpriseBridgeLeader);const auto* U=S?Store->Find(S->LeaderUnitID):nullptr;
  if(!S||!U||!U->IsAlive()||!G.Squads.Contains(G.SurpriseBridgeLeader)||S->TacticalPlan.Action==EBattleSquadTactic::Recover)
  {
   const int32 Old=G.SurpriseBridgeLeader;G.SurpriseBridgeLeader=PickGuide(Lead);
   if(G.SurpriseBridgeLeader!=Old)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_BRIDGE group=%d team=%d stage=guide old=%d new=%d time=%.1f"),G.ID,G.Team,Old,G.SurpriseBridgeLeader,Elapsed);
  }
 }
 const auto* S=Store->FindSquad(G.SurpriseBridgeLeader);const auto* U=S?Store->Find(S->LeaderUnitID):nullptr;
 const FVector P=S&&U&&U->IsAlive()&&G.Squads.Contains(G.SurpriseBridgeLeader)?R->Position(S->LeaderUnitID):Lead;
 // Infantry may finish in cover up to 14 m from a non-sector order. Nearness alone must not count the opposite bank.
 const double Direction=G.SurpriseBridgeExit.X>G.SurpriseBridgeEntry.X?1.:-1.;
 auto Passed=[&](const FVector& Q)
 {
  if(FVector::Dist2D(Q,G.Rally)>(G.SurpriseBridgeStage==3?2500.:1600.))return false;
  if(G.SurpriseBridgeStage==1&&(Q.X-G.Rally.X)*Direction>600.)return false;
  if(G.SurpriseBridgeStage==2&&(Q.X-G.Rally.X)*Direction< -600.)return false;
  return true;
 };
 if(!Passed(P))
 {
  // An armoured guide held at the bridge by its own rules (groupui-surprise5-I-s7: both groups stood 300 s in the crossing leg) hands
  // the leg to another squad not in Recover that reached the waypoint, after LagWait seconds on this leg.
  if(Elapsed-G.SurpriseBridgeAt<BattleSurpriseValues::LagWait)return true;
  int32 Alt=INDEX_NONE;
  for(int32 SID:G.Squads)if(SID!=G.SurpriseBridgeLeader)if(const auto* O=Store->FindSquad(SID))if(const auto* OU=Store->Find(O->LeaderUnitID);OU&&OU->IsAlive()&&O->TacticalPlan.Action!=EBattleSquadTactic::Recover&&Passed(R->Position(O->LeaderUnitID))){Alt=SID;break;}
  if(Alt==INDEX_NONE)return true;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_BRIDGE group=%d team=%d stage=guide_stalled old=%d new=%d time=%.1f"),G.ID,G.Team,G.SurpriseBridgeLeader,Alt,Elapsed);
  G.SurpriseBridgeLeader=Alt;
 }
 G.SurpriseBridgeAt=Elapsed;
 if(G.SurpriseBridgeStage==1){G.SurpriseBridgeStage=2;G.Rally=G.SurpriseBridgeExit;}
 else if(G.SurpriseBridgeStage==2)
 {
  G.Rally=G.SurpriseBridgeGoal;G.SurpriseBridgeStage=0;
  // A gathering leg can span both rivers on I. Route the remaining segment before returning to the edge.
  if(UpdateSurpriseBridge(G,P))return true;
  G.SurpriseBridgeStage=3;
 }
 else {G.SurpriseBridgeStage=0;G.SurpriseBridgeLeader=INDEX_NONE;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_BRIDGE group=%d team=%d stage=%s rally=(%.0f,%.0f) time=%.1f"),G.ID,G.Team,G.SurpriseBridgeStage==2?TEXT("cross"):G.SurpriseBridgeStage==3?TEXT("return"):TEXT("done"),G.Rally.X,G.Rally.Y,Elapsed);
 return G.SurpriseBridgeStage!=0;
}

bool UBattleMatch::SurpriseOn(int32 Team) const
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleSurpriseGroup="),0);
 return On!=0&&FrontLineOn(Team);
}

void UBattleMatch::UpdateSurpriseGroup(int32 Team,int32 FrontID)
{
 using namespace BattleSurpriseValues;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 // Living members of all its squads, vehicles included (a group may be armour only).
 auto AllLiving=[Store](const FBattleGroup& Group){int32 N=0;for(int32 SID:Group.Squads)if(const auto* S=Store->FindSquad(SID))for(int32 UID:S->Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive())++N;return N;};
 int32 SID_=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==2)SID_=P.Key;
 if(!SurpriseOn(Team)){if(SID_!=INDEX_NONE)DissolveGroup(SID_,TEXT("surprise_off"));return;}
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 const bool bLow=AreaHome(Team).X<0;auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 auto RowOf=[&](const FIntPoint& C){return bLow?C.Y:Rows-1-C.Y;};
 const int32 EnemyHomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(1-Team))),0,Cols-1);
 const auto& Line=FrontLine[Team];if(Line.Num()!=Rows)return;
 // Sector centres are on dry banks on H and I, even in an R parcel. Keep the edge row; the river geometry below routes crossings.
 // Form: a front-holding group exists, none running, the interval passed.
 if(SID_==INDEX_NONE)
 {
  if(FrontID==INDEX_NONE||Elapsed<SurpriseNextAt[Team]||Elapsed<FirstAt)return;
  const int32 Want=FMath::Clamp(BattleFrontValues::Arg(TEXT("BattleSurpriseSquads="),2),2,6); // an AI group needs two squads
  // The edge row with fewer known enemies ahead of the line (rows 0-1 or Rows-2..Rows-1).
  int32 Ahead[2]={0,0};
  for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))
  {if(RowOf(C)<=1&&ColOf(C)>Line[0])++Ahead[0];if(RowOf(C)>=Rows-2&&ColOf(C)>Line[Rows-1])++Ahead[1];}
  const int32 Row=Ahead[0]<Ahead[1]?0:Ahead[1]<Ahead[0]?Rows-1:0; // own rows, so a tie takes the same side of both teams
  const FVector Start=BattleSectors::Center(FrontCell(Team,FMath::Max(0,Line[Row]-1),Row));
  struct FFree{int32 SID;double D;bool bArmour;};TArray<FFree> Free;
  // Why armour was not free (2026-11-21 groupui-surprise2-I-s7: one group in 900 s): in a group, on a hire/intel errand, on a base assault, not full strength.
  int32 Skip[4]={0,0,0,0};
  // Front army: the surprise group draws on it while it keeps FrontArmyKeep squads.
  int32 FrontSpare=BattleFrontValues::Army()&&Groups.Contains(FrontID)?Groups[FrontID].Squads.Num()-BattleFrontValues::FrontArmyKeep:0;
  for(const auto& P:Store->GetSquads())
  {
   const auto& S=P.Value;
   if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated))continue;
   const bool bArmour=S.SquadType==BattleSquadType::Tank||S.SquadType==BattleSquadType::ArmoredCar||S.SquadType==BattleSquadType::Exo;
   if(!bArmour&&S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Heavy)continue;
   if(const int32 Old=GroupOf(P.Key);Old!=INDEX_NONE&&!(Old==FrontID&&FrontSpare>0)){Skip[0]+=bArmour;continue;}
   if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key)||ArmyPinned.Contains(P.Key)){Skip[1]+=bArmour;continue;}
   // The base slot holds only the types the real orders send on the home assault (UpdateTeamOrders: never heavy squads, armoured cars or
   // exoskeleton soldiers; Codex review 2026-11-21: exoskeletons 72 and 77 of groupui-surprise2-I-s7 were kept out for nothing).
   bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);
   if(S.SquadType==BattleSquadType::Heavy||S.SquadType==BattleSquadType::ArmoredCar||S.SquadType==BattleSquadType::Exo)bBase=false;
   if(bBase){Skip[2]+=bArmour;continue;}
   // Full-strength squads only: the walk is long. Armour (one crew) also needs ArmourHealth of its health, not to leave half-wrecked.
   int32 Living=0;float HP=0.f,MaxHP=0.f;for(int32 UID:S.Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive()){++Living;HP+=U->HP;MaxHP+=Store->MaxHealthOf(UID);}
   if(Living<S.Members.Num()||(bArmour&&HP<ArmourHealth*MaxHP)){Skip[3]+=bArmour;continue;}
   if(FrontID!=INDEX_NONE&&GroupOf(P.Key)==FrontID)--FrontSpare;
   Free.Add({P.Key,FVector::DistSquared2D(R->Position(S.LeaderUnitID),Start),bArmour});
  }
  // No free armour: a healthy tank of the team's AI attack group may move over when that group keeps two squads (Codex review
  // 2026-11-21: with one tank kept, the attack group always took it first). FormGroup takes it out of its old group.
  if(!Free.ContainsByPredicate([](const FFree& F){return F.bArmour;}))for(const auto& GP:Groups)
  {
   const auto& Old=GP.Value;if(Old.Team!=Team||Old.bPlayer||Old.Kind!=0||Old.Squads.Num()<=2)continue;
   for(int32 SID:Old.Squads)if(const auto* S=Store->FindSquad(SID);S&&S->SquadType==BattleSquadType::Tank&&S->CommandStatus==EBattleCommandStatus::Active&&(Team!=0||S->bPlayerDelegated))
   {
    float HP=0.f,MaxHP=0.f;for(int32 UID:S->Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive()){HP+=U->HP;MaxHP+=Store->MaxHealthOf(UID);}
    if(MaxHP>0.f&&HP>=ArmourHealth*MaxHP){Free.Add({SID,FVector::DistSquared2D(R->Position(S->LeaderUnitID),Start),true});break;}
   }
  }
  // Armour first (nearest first), then infantry; none without armour.
  Free.Sort([](const FFree& A,const FFree& B){return A.bArmour!=B.bArmour?A.bArmour:A.D!=B.D?A.D<B.D:A.SID<B.SID;});
  if(Free.Num()<FMath::Max(2,Want)||!Free[0].bArmour)
  {
   if(Elapsed>=SurpriseSkipLogAt[Team])
   {
    SurpriseSkipLogAt[Team]=Elapsed+30.f;int32 FreeArmour=0;for(const auto& F:Free)FreeArmour+=F.bArmour;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_WAIT team=%d free=%d free_armour=%d armour_grouped=%d armour_errand=%d armour_base=%d armour_depleted=%d time=%.1f"),Team,Free.Num(),FreeArmour,Skip[0],Skip[1],Skip[2],Skip[3],Elapsed);
   }
   return;
  }
  TArray<int32> Pick;for(const auto& F:Free)if(Pick.Num()<Want)Pick.Add(F.SID);
  const int32 ID=FormGroup(Team,Pick,false);auto* G=Groups.Find(ID);if(!G)return;
  G->Kind=2;G->Phase=0;G->PhaseAt=Elapsed;G->Rally=Start;G->SurpriseRow=Row;G->StartLiving=AllLiving(*G);G->SurpriseEndAt=Elapsed+AssembleMax+Max;
  // The most forward armoured squad leads from the start too (Codex review 2026-11-21: it was the most forward squad of any type).
  FVector Lead=R->Position(Store->FindSquad(Pick[0])->LeaderUnitID);
  for(int32 I=0;I<Pick.Num();++I)if(const auto* S=Store->FindSquad(Pick[I]);S&&Free[I].bArmour){const FVector P=R->Position(S->LeaderUnitID);if((bLow?P.X:-P.X)>(bLow?Lead.X:-Lead.X))Lead=P;}
  G->SurpriseNextCol=-1;
  UpdateSurpriseBridge(*G,Lead);
  SurpriseNextAt[Team]=Elapsed+Interval;
  int32 Armour=0;FString Ids;for(int32 I=0;I<Pick.Num();++I){Armour+=Free[I].bArmour;Ids+=FString::Printf(TEXT("%s%d"),I?TEXT(","):TEXT(""),Pick[I]);}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_FORM group=%d team=%d squads=%d armour=%d ids=%s row=%d start=%c%d time=%.1f"),ID,Team,G->Squads.Num(),Armour,*Ids,Row,TCHAR('A'+BattleSectors::Cell(Start).X),1+BattleSectors::Cell(Start).Y,Elapsed);
  return;
 }
 auto& G=Groups[SID_];
 const int32 Living=AllLiving(G);
 if(G.StartLiving>0&&Living<G.StartLiving*(1.-LossEnd)){DissolveGroup(SID_,TEXT("surprise_losses"));SurpriseNextAt[Team]=Elapsed+Interval;return;}
 // One limit for the whole group (Codex review 2026-11-21: it was per phase, so a strike restarted it), and every end waits Interval before
 // the next group (the time-out did not, so the squads were taken again at once).
 if(Elapsed>G.SurpriseEndAt){DissolveGroup(SID_,TEXT("surprise_time"));SurpriseNextAt[Team]=Elapsed+Interval;return;}
 // The group's columns: its rearmost and its leading squad's (2026-11-20 first match: stepping from the rearmost one let an armoured
 // squad held back by its own rules stop the whole group until the time ran out).
 int32 Col=Cols,Lead=-1;int32 N=0,Ready=0;FString Where;FVector LeadPosition=FVector::ZeroVector;double LeadX=-DBL_MAX;
 // The armour sets the pace when it is with the group (2026-11-21 groupui-surprise3-I-*: stepping and the bridge legs followed the
 // fastest squad, a rifle squad, which crossed and struck alone while the tank backed off known anti-tank soldiers behind the river;
 // Codex analysis). An armoured squad driving home for repair does not count, so the others go on without it.
 int32 ALead=-1,ALeadSID=INDEX_NONE,LeadSID=INDEX_NONE,NRecover=0;FVector ALeadPosition=FVector::ZeroVector;double ALeadX=-DBL_MAX;
 for(int32 SID:G.Squads)if(const auto* S=Store->FindSquad(SID))
 {
  const FVector P=R->Position(S->LeaderUnitID);const int32 C=ColOf(BattleSectors::Cell(P));
  const bool bRecover=S->TacticalPlan.Action==EBattleSquadTactic::Recover;
  Where+=FString::Printf(TEXT(" %d:t%d:c%d%s"),SID,int32(S->SquadType),C,bRecover?TEXT("r"):TEXT(""));
  // Squads in Recover (repair trip, backing off) leave the pace, the waiting and the assembly count (Codex review 2026-11-21).
  if(bRecover){++NRecover;continue;}
  if((bLow?P.X:-P.X)>LeadX){LeadX=bLow?P.X:-P.X;LeadPosition=P;LeadSID=SID;}
  Col=FMath::Min(Col,C);Lead=FMath::Max(Lead,C);++N;Ready+=FVector::Dist2D(P,G.Rally)<=2500.;
  const bool bArmour=S->SquadType==BattleSquadType::Tank||S->SquadType==BattleSquadType::ArmoredCar||S->SquadType==BattleSquadType::Exo;
  const auto* U=Store->Find(S->LeaderUnitID);
  if(bArmour&&U&&U->IsAlive()&&(bLow?P.X:-P.X)>ALeadX){ALeadX=bLow?P.X:-P.X;ALeadPosition=P;ALead=C;ALeadSID=SID;}
 }
 if(!N)return; // all in Recover: keep the stop until someone comes back
 // Armour sets the pace only while it is with the group, not more than three columns behind the stop (it stalled at home after a
 // repair in groupui-surprise4-I-s7 and pulled the whole group back).
 const int32 StopCol=G.SurpriseNextCol>=0?G.SurpriseNextCol:Lead;
 if(ALead>=0&&ALead>=StopCol-3){Lead=ALead;LeadPosition=ALeadPosition;LeadSID=ALeadSID;}
 if(Elapsed>=G.SurpriseLogAt)
 {
  G.SurpriseLogAt=Elapsed+30.f;const FIntPoint RC=BattleSectors::Cell(G.Rally);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_STATE group=%d team=%d phase=%d rally=%c%d bridge_stage=%d living=%d lead_sid=%d lead_col=%d min_col=%d stop_col=%d guide_sid=%d recover=%d squads=%s time=%.1f"),SID_,Team,int32(G.Phase),TCHAR('A'+RC.X),1+RC.Y,int32(G.SurpriseBridgeStage),Living,LeadSID,Lead,Col,G.SurpriseNextCol,G.SurpriseBridgeLeader,NRecover,*Where,Elapsed);
 }
 auto Enter=[&](uint8 To,const TCHAR* Why){G.Phase=To;G.PhaseAt=Elapsed;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_PHASE group=%d team=%d phase=%d column=%d living=%d reason=%s time=%.1f"),SID_,Team,int32(To),Col,Living,Why,Elapsed);};
 // Finish every bridge leg before assembly timeout, lag handling or the strike can replace its waypoint.
 if(G.Phase<2&&G.SurpriseBridgeStage!=0){UpdateSurpriseBridge(G,LeadPosition);return;}
 if(G.Phase==0)
 {
  if(UpdateSurpriseBridge(G,LeadPosition))return;
  if(Ready>=N||Elapsed-G.PhaseAt>AssembleMax)Enter(1,Ready>=N?TEXT("assembled"):TEXT("assemble_timeout"));
  return;
 }
 if(G.Phase==1)
 {
  // The lead waits for squads more than three columns behind, at most LagWait seconds each time.
  bool bWait=false;
  if(Lead-Col>3){if(G.SurpriseWaitAt<0)G.SurpriseWaitAt=Elapsed;bWait=Elapsed-G.SurpriseWaitAt<LagWait;}
  else G.SurpriseWaitAt=-1.f;
  // Strike with the escort close, or after the wait (Codex review 2026-11-21: the tank struck 3-6 columns ahead of the infantry).
  if(Lead>=EnemyHomeCol-2&&!bWait){Enter(2,TEXT("strike"));return;}
  // Next stop: two columns ahead of the leading squad along the edge row, using a bridge when the segment crosses water. A stop once
  // given is kept when the lead falls back (groupui-surprise4-I-s7/s3: Lead+2 rebuilt each tick pulled the group back with a retreat);
  // while waiting it does not move forward.
  int32 Next=FMath::Min(Lead+2,EnemyHomeCol-2);
  if(G.SurpriseNextCol>=0)Next=bWait?FMath::Max(Lead,FMath::Min(Next,G.SurpriseNextCol)):FMath::Max(Next,G.SurpriseNextCol);
  else if(bWait)Next=Lead;
  G.SurpriseNextCol=Next;
  G.Rally=BattleSectors::Center(FrontCell(Team,Next,G.SurpriseRow));
  UpdateSurpriseBridge(G,LeadPosition);
 }
}

bool UBattleMatch::SurpriseGroupOrder(int32 GroupID,int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 // Gathering and moving: a Move order (no stopping to fight on the way; the squads' own rules still answer fire); the strike: an
 // attack-move order on the enemy home.
 const FBattleGroup* Found=Groups.Find(GroupID);if(!Found)return false;const FBattleGroup& G=*Found;
 if(G.Phase<2){Center=G.Rally;Order=G.Phase==0?EBattleOrder::AttackMove:EBattleOrder::Move;return true;}
 Center=AreaHome(1-Team);Order=EBattleOrder::AttackMove;return true;
}

bool UBattleMatch::SurpriseMoving(int32 SquadID) const
{
 const int32 ID=GroupOf(SquadID);if(ID==INDEX_NONE)return false;
 const FBattleGroup* G=Groups.Find(ID);return G&&G->Kind==2&&G->Phase==1;
}

// ---- Front plan, stage 1: sections and their log (Docs/design-front-strategy.md 2.2 and 7; trial -BattleFrontPlanLog=1, default 0) ----
// Log only. Rows are the team's own rows (FrontCell), so both teams of a 180-degree symmetric map get the same sections.
bool UBattleMatch::FrontPlanLogOn()
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontPlanLog="),0);
 return On!=0;
}

void UBattleMatch::BuildFrontSections(int32 Team)
{
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 FFrontPlanTeam& P=FrontPlanState[Team];
 const bool bLow=AreaHome(Team).X<0;
 auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 const int32 HomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(Team))),0,Cols-1);
 const int32 EnemyHomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(1-Team))),0,Cols-1);
 // Mountains and lakes block a crossing between two rows; rivers do not (they run across the axis and have bridges and fords).
 auto Open=[&](int32 C,int32 Y){const FIntPoint Cell=FrontCell(Team,C,Y);if(!BattleSectors::Valid(Cell))return false;const char K=BattleFieldLayout::SectorKind(GetWorld(),Cell);return K!='M'&&K!='L';};
 const int32 C0=FMath::Min(HomeCol+1,EnemyHomeCol-1),C1=FMath::Max(HomeCol+1,EnemyHomeCol-1),Span=C1-C0+1;
 TArray<int32> Cross;Cross.Init(0,Rows-1);
 for(int32 Y=0;Y+1<Rows;++Y)for(int32 C=C0;C<=C1;++C)Cross[Y]+=Open(C,Y)&&Open(C,Y+1)?1:0;
 // Segments: cut below row Y where at most a quarter of the span crosses to row Y+1 (a mountain wall between two roads).
 TArray<int32> First;First.Add(0);
 for(int32 Y=0;Y+1<Rows;++Y)if(Cross[Y]*4<=Span)First.Add(Y+1);
 auto Len=[&](int32 I){return (I+1<First.Num()?First[I+1]:Rows)-First[I];};
 // Split a segment at its row boundary with the least crossing (the middle on ties).
 auto Split=[&](int32 I)
 {
  const int32 A=First[I],B=A+Len(I);int32 Best=-1;
  for(int32 Y=A+1;Y<B;++Y)if(Best<0||Cross[Y-1]<Cross[Best-1]||(Cross[Y-1]==Cross[Best-1]&&FMath::Abs(2*Y-A-B)<FMath::Abs(2*Best-A-B)))Best=Y;
  if(Best>0)First.Insert(Best,I+1);
 };
 for(int32 I=0;I<First.Num();++I)while(Len(I)>5)Split(I);
 // At most 5 sections: merge the neighbouring pair with the fewest rows. One-row sections join their smaller neighbour.
 while(First.Num()>5){int32 Best=1;for(int32 I=1;I<First.Num();++I)if(Len(I-1)+Len(I)<Len(Best-1)+Len(Best))Best=I;First.RemoveAt(Best);}
 for(int32 I=0;I<First.Num()&&First.Num()>3;)
  if(Len(I)==1){const int32 J=I==0?1:I+1==First.Num()?I:(Len(I-1)<=Len(I+1)?I:I+1);First.RemoveAt(J);I=0;}else ++I;
 // At least 3 sections: split the longest.
 while(First.Num()<3){int32 Best=0;for(int32 I=1;I<First.Num();++I)if(Len(I)>Len(Best))Best=I;if(Len(Best)<2)break;Split(Best);}
 P.SectionFirst=First;P.Sections=First.Num();P.SectionOfRow.Init(0,Rows);
 for(int32 I=0;I<First.Num();++I)for(int32 Y=First[I];Y<First[I]+Len(I);++Y)P.SectionOfRow[Y]=int8(I);
 P.SampleAt.Reset();P.Damage.Reset();P.MeanCol.Reset();
 FString Ranges,Cut;
 for(int32 I=0;I<First.Num();++I)Ranges+=FString::Printf(TEXT("%s%d-%d"),I?TEXT(","):TEXT(""),First[I]+1,First[I]+Len(I));
 for(int32 Y=0;Y+1<Rows;++Y)if(Cross[Y]*4<=Span)Cut+=FString::Printf(TEXT("%s%d:%d"),Cut.IsEmpty()?TEXT(""):TEXT(","),Y+1,Cross[Y]);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_SECTIONS team=%d sections=%d rows=%s span=%d walls=%s"),Team,P.Sections,*Ranges,Span,Cut.IsEmpty()?TEXT("none"):*Cut);
}

void UBattleMatch::FrontPlanSample(int32 Team)
{
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 FFrontPlanTeam& P=FrontPlanState[Team];
 if(P.SectionOfRow.Num()!=Rows)BuildFrontSections(Team);
 const int32 NS=P.Sections;if(NS<=0)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 if(!Store||!R)return;
 const bool bLow=AreaHome(Team).X<0;
 auto ColOf=[&](const FIntPoint& C){return bLow?C.X:Cols-1-C.X;};
 auto RowOf=[&](const FIntPoint& C){return bLow?C.Y:Rows-1-C.Y;};
 const int32 HomeCol=FMath::Clamp(ColOf(BattleSectors::Cell(AreaHome(Team))),0,Cols-1);
 const TArray<int32>& Line=FrontLine[Team];
 // Own infantry squads by the section their leader stands in (out of the home column); the damage of the last 5 s is spread over the
 // 2 s between samples (RecentDamage x Interval / 5).
 TArray<double> Dmg,Col;TArray<int32> Count;Dmg.Init(0.,NS);Col.Init(0.,NS);Count.Init(0,NS);
 P.Hp.Init(0.,NS);P.SquadsN.Init(0,NS);P.KnownN.Init(0,NS);P.CoverN.Init(0,NS);P.OwnMask.Init(0u,NS);P.AheadMask.Init(0u,NS);P.Push.Init(0.,NS);P.Dmg.Init(0.,NS);P.Soldiers.Init(0,NS);
 for(const auto& Pair:Store->GetSquads())
 {
  const auto& S=Pair.Value;
  if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||BattleSquadType::IsVehicle(S.SquadType)||S.SquadType==BattleSquadType::Scout)continue;
  const FIntPoint C=BattleSectors::Cell(R->Position(S.LeaderUnitID));if(!BattleSectors::Valid(C)||ColOf(C)<=HomeCol)continue;
  const int32 K=P.SectionOfRow[RowOf(C)];++P.SquadsN[K];
  for(int32 UID:S.Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive())++P.Soldiers[K];
  if(const auto* O=Tactics?Tactics->Find(Pair.Key):nullptr){Dmg[K]+=O->RecentDamage*BattleFrontValues::Interval/5.;P.Hp[K]+=O->Health;}
 }
 for(int32 Y=0;Y<Rows;++Y){Col[P.SectionOfRow[Y]]+=Line[Y];++Count[P.SectionOfRow[Y]];}
 for(int32 K=0;K<NS;++K)Col[K]/=FMath::Max(1,Count[K]);
 P.SampleAt.Add(Elapsed);P.Damage.Append(Dmg);P.MeanCol.Append(Col);
 // Keep 65 s of samples.
 int32 Drop=0;while(Drop<P.SampleAt.Num()&&P.SampleAt[Drop]<Elapsed-65.f)++Drop;
 if(Drop){P.SampleAt.RemoveAt(0,Drop);P.Damage.RemoveAt(0,Drop*NS);P.MeanCol.RemoveAt(0,Drop*NS);}
 // Known enemies from the line's column minus one to four ahead (AreaIntelView, cached this frame by UpdateFrontLine); sectors with
 // cover on the line and two columns ahead; points: own ones in the section, and not-own ones up to two columns ahead of the line.
 for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))
 {const int32 Y=RowOf(C);if(ColOf(C)>=Line[Y]-1&&ColOf(C)<=Line[Y]+4)++P.KnownN[P.SectionOfRow[Y]];}
 for(int32 Y=0;Y<Rows;++Y)for(int32 C=Line[Y];C<=FMath::Min(Cols-1,Line[Y]+2);++C)
  if(const FIntPoint Cell=FrontCell(Team,C,Y);BattleSectors::Valid(Cell)&&BattleFrontValues::Cover(BattleFieldLayout::SectorKind(GetWorld(),Cell))>0.)++P.CoverN[P.SectionOfRow[Y]];
 for(int32 I=0;I<Objectives.Num()&&I<32;++I)
 {
  const FIntPoint C=BattleSectors::Cell(Objectives[I].Position);if(!BattleSectors::Valid(C))continue;
  const int32 Y=RowOf(C),K=P.SectionOfRow[Y];
  if(ObservedOwner(Team,I)==Team)P.OwnMask[K]|=1u<<I;
  else if(ColOf(C)>Line[Y]-1&&ColOf(C)<=Line[Y]+2)P.AheadMask[K]|=1u<<I;
 }
 const int32 N=P.SampleAt.Num();int32 Old=0;while(Old+1<N&&P.SampleAt[Old+1]<=Elapsed-60.f)++Old;
 for(int32 K=0;K<NS;++K)
 {
  double D60=0;for(int32 J=0;J<N;++J)D60+=P.Damage[J*NS+K];
  P.Push[K]=N?P.MeanCol[(N-1)*NS+K]-P.MeanCol[Old*NS+K]:0.;
  P.Dmg[K]=P.Hp[K]>0.?D60/P.Hp[K]:0.;
 }
 P.StatAt=Elapsed;
 if(FrontRolesOn(Team))UpdateFrontRoles(Team);
 if(Elapsed<P.NextLog)return;
 P.NextLog=Elapsed+BattleFrontValues::LogInterval;
 auto Letters=[&](uint32 M){FString Out;for(int32 I=0;I<32;++I)if((M>>I)&1)Out.AppendChar(TCHAR('A'+I));return Out.IsEmpty()?FString(TEXT("-")):Out;};
 for(int32 K=0;K<NS;++K)
 {
  const int32 A=P.SectionFirst[K],B=K+1<NS?P.SectionFirst[K+1]:Rows;
  FString L;for(int32 Y=A;Y<B;++Y)L+=FString::Printf(TEXT("%s%d"),Y>A?TEXT(","):TEXT(""),Line[Y]);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_SECTION team=%d t=%.1f sec=%d rows=%d-%d line=%s push=%+.2f over=%.0f dmg=%.3f hp=%.0f squads=%d known=%d cover=%d own=%s ahead=%s"),
   Team,Elapsed,K+1,A+1,B,*L,P.Push[K],N?P.SampleAt[N-1]-P.SampleAt[Old]:0.f,P.Dmg[K],P.Hp[K],P.SquadsN[K],P.KnownN[K],P.CoverN[K],*Letters(P.OwnMask[K]),*Letters(P.AheadMask[K]));
 }
}

// ---- Front plan, stage 2: the focus section (Docs/design-front-strategy.md 2.1, 3.1, 4; trial -BattleFrontPlan=1, default 0) ----
bool UBattleMatch::FrontPlanOn(int32 Team) const
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontPlan="),0);
 static const int32 Only=BattleFrontValues::Arg(TEXT("BattleFrontPlanTeam="),-1);
 // Needs the front army's own line (FrontHold).
 return On!=0&&(Only<0||Only==Team)&&FrontLineOn(Team)&&BattleFrontValues::Hold();
}

void UBattleMatch::FrontPlanFocusRows(int32 Team,TArray<uint8>& InFocus,int32& TargetCol) const
{
 InFocus.Reset();TargetCol=-1;
 if(Team<0||Team>1||!FrontPlanOn(Team))return;
 const FFrontPlanTeam& P=FrontPlanState[Team];
 const int32 F=StrategyFocusPoint(Team);
 if(F==INDEX_NONE||ObservedOwner(Team,F)==Team||P.SectionOfRow.Num()!=BattleSectors::Rows)return;
 constexpr int32 Cols=BattleSectors::Columns,Rows=BattleSectors::Rows;
 const bool bLow=AreaHome(Team).X<0;
 const FIntPoint C=BattleSectors::Cell(Objectives[F].Position);if(!BattleSectors::Valid(C))return;
 const int32 K=P.SectionOfRow[bLow?C.Y:Rows-1-C.Y];
 InFocus.Init(0,Rows);for(int32 Y=0;Y<Rows;++Y)InFocus[Y]=P.SectionOfRow[Y]==K?1:0;
 TargetCol=bLow?C.X:Cols-1-C.X;
}

void UBattleMatch::UpdateFrontPlan(int32 Team)
{
 if(!bArea||Team<0||Team>1||!FrontPlanOn(Team)||Objectives.IsEmpty()||Phase!=EBattleMatchPhase::Combat)return;
 const FFrontPlanTeam& P=FrontPlanState[Team];
 const int32 NS=P.Sections;
 if(NS<=0||P.StatAt<0.f||P.Push.Num()!=NS)return;
 auto Fix=[](auto& A,auto V){if(A.Num()!=2)A.Init(V,2);};
 Fix(Area.StrategyPhase,0);Fix(Area.StrategyFocus,-1);Fix(Area.StrategyFocusAt,-1.f);Fix(Area.StrategyReviewAt,-1.f);Fix(Area.StrategyAvoid,-1);Fix(Area.StrategyAvoidUntil,-1.f);
 int32& Focus=Area.StrategyFocus[Team];float& FocusAt=Area.StrategyFocusAt[Team];float& ReviewAt=Area.StrategyReviewAt[Team];int32& SPhase=Area.StrategyPhase[Team];
 // Stage 7: with a focus section of the player's only those sections are candidates (also during the opening); otherwise the sections
 // the player left to the AI. A section the player set to keep, hold or fall back is never the focus.
 const bool bPlayer=PlayerRolesOn(Team);
 bool bPlayerF=false;if(bPlayer)for(int32 K=0;K<NS;++K)bPlayerF|=PlayerSectionRole(K)==1;
 auto Candidate=[&](int32 K){if(!bPlayer)return true;const uint8 R=PlayerSectionRole(K);return bPlayerF?R==1:R==0;};
 // The opening (no push history yet): no focus, so S6 and the attack group keep their default target (the nearest point not own).
 if(Elapsed<StrategyOpenSeconds()*MatchTimeLimit()/600.f&&!bPlayerF)return;
 const bool bValid=Objectives.IsValidIndex(Focus);
 const bool bTaken=bValid&&ObservedOwner(Team,Focus)==Team;
 if(Elapsed<ReviewAt&&bValid&&!bTaken)return;
 constexpr int32 Rows=BattleSectors::Rows;
 const bool bLow=AreaHome(Team).X<0;
 auto SectionOf=[&](int32 Point)
 {const FIntPoint C=BattleSectors::Cell(Objectives[Point].Position);return BattleSectors::Valid(C)&&P.SectionOfRow.Num()==Rows?int32(P.SectionOfRow[bLow?C.Y:Rows-1-C.Y]):-1;};
 const int32 FocusSec=bValid&&!bTaken?SectionOf(Focus):-1;
 // Score (Docs/design-front-strategy.md 3.1): push over 60 s - 2 x damage share + 0.5 x the best point value ahead - known enemies
 // ahead per squad (6) + 1 for the current focus section (no switching on a small difference).
 const FVector Home=AreaHome(Team);
 int32 Best=-1,Next=-1,BestPoint=INDEX_NONE;double BestScore=0,NextScore=0;
 for(int32 K=0;K<NS;++K)
 {
  if(P.AheadMask[K]==0u||!Candidate(K))continue;
  double Value=0;int32 Point=INDEX_NONE;double PointD=DBL_MAX;
  for(int32 I=0;I<Objectives.Num()&&I<32;++I)if((P.AheadMask[K]>>I)&1)
  {
   Value=FMath::Max(Value,double(ObjectiveValue(I)));
   const double D=FVector::DistSquared2D(Objectives[I].Position,Home);if(D<PointD){PointD=D;Point=I;}
  }
  const double SecScore=P.Push[K]-2.*P.Dmg[K]+.5*Value-P.KnownN[K]/6.+(K==FocusSec?1.:0.);
  if(Best<0||SecScore>BestScore){Next=Best;NextScore=BestScore;Best=K;BestScore=SecScore;BestPoint=Point;}
  else if(Next<0||SecScore>NextScore){Next=K;NextScore=SecScore;}
 }
 // The focus stays at least the hold time unless its point was taken or it has no candidate left.
 const TCHAR* Why=bTaken?TEXT("taken"):!bValid?TEXT("first"):TEXT("review");
 if(bValid&&!bTaken&&Best!=FocusSec&&Elapsed-FocusAt<StrategyFocusHoldSeconds()&&FocusSec>=0&&P.AheadMask[FocusSec]!=0u&&Candidate(FocusSec)){Best=FocusSec;BestPoint=Focus;Why=TEXT("hold");}
 // Within the focus section keep the current target while it is still ahead of the line.
 if(Best>=0&&Best==FocusSec&&bValid&&!bTaken&&Focus<32&&((P.AheadMask[Best]>>Focus)&1))BestPoint=Focus;
 const int32 Old=Focus;
 if(BestPoint!=Focus){Focus=BestPoint;FocusAt=Elapsed;}
 SPhase=Focus!=INDEX_NONE?1:0;
 ReviewAt=Elapsed+60.f;
 if(FrontPlanFocus2On(Team))
 {
  // Stage 5 (3.1): a second focus only while the first pushes (its section moved forward over 60 s and the AI attack group lost under
  // 30% of its soldiers) and at least three free AI rifle squads remain. The best other section with a point ahead, its point nearest
  // the own home; kept at least the focus hold time unless taken or the first stops pushing.
  FFrontPlanTeam& FP=FrontPlanState[Team];
  const int32 FocusSecNow=Objectives.IsValidIndex(Focus)?SectionOf(Focus):-1;
  int32 AIGroup=INDEX_NONE;for(const auto& G:Groups)if(G.Value.Team==Team&&!G.Value.bPlayer&&G.Value.Kind==0&&!G.Value.bFocus2)AIGroup=AIGroup==INDEX_NONE?G.Key:FMath::Min(AIGroup,G.Key);
  double Loss=0.;if(const FBattleGroup* G1=Groups.Find(AIGroup);G1&&G1->StartLiving>0)Loss=1.-double(GroupLiving(*G1))/G1->StartLiving;
  // Stage 7: a second focus section of the player's is taken without the pushing condition; with one focus of the player's the AI may
  // add a second among the sections left to it.
  int32 PlayerF2=-1;if(bPlayerF)for(int32 K=0;K<NS;++K)if(PlayerSectionRole(K)==1&&K!=FocusSecNow)PlayerF2=K;
  auto Candidate2=[&](int32 K){if(PlayerF2>=0)return K==PlayerF2;if(!bPlayer)return true;const uint8 R=PlayerSectionRole(K);return R==0||R==1;};
  const bool bPushing=PlayerF2>=0||(FocusSecNow>=0&&P.Push.IsValidIndex(FocusSecNow)&&P.Push[FocusSecNow]>0.&&Loss<.3);
  const bool bTaken2=Objectives.IsValidIndex(FP.Focus2)&&ObservedOwner(Team,FP.Focus2)==Team;
  const int32 Old2=FP.Focus2;const TCHAR* Why2=TEXT("none");
  if(!bPushing){if(FP.Focus2>=0){FP.Focus2=-1;Why2=TEXT("first_not_pushing");}}
  else if(FP.Focus2<0||bTaken2||Elapsed-FP.Focus2At>=StrategyFocusHoldSeconds()||SectionOf(FP.Focus2)==FocusSecNow||!Candidate2(SectionOf(FP.Focus2)))
  {
   int32 Pick=-1;double PickScore=0;int32 PickPoint=INDEX_NONE;
   for(int32 K=0;K<NS;++K)if(K!=FocusSecNow&&P.AheadMask[K]!=0u&&Candidate2(K))
   {
    double Value=0;int32 Point=INDEX_NONE;double PointD=DBL_MAX;
    for(int32 I=0;I<Objectives.Num()&&I<32;++I)if((P.AheadMask[K]>>I)&1){Value=FMath::Max(Value,double(ObjectiveValue(I)));const double D=FVector::DistSquared2D(Objectives[I].Position,Home);if(D<PointD){PointD=D;Point=I;}}
    const double S2=P.Push[K]-2.*P.Dmg[K]+.5*Value-P.KnownN[K]/6.+(Objectives.IsValidIndex(FP.Focus2)&&K==SectionOf(FP.Focus2)?1.:0.);
    if(Pick<0||S2>PickScore){Pick=K;PickScore=S2;PickPoint=Point;}
   }
   const bool bTroops=FP.FreeRifles>=3||Old2>=0; // the running second army keeps its squads
   const int32 New2=Pick>=0&&bTroops?PickPoint:-1;
   if(New2!=FP.Focus2){FP.Focus2=New2;FP.Focus2At=Elapsed;Why2=bTaken2?TEXT("taken"):New2<0?(bTroops?TEXT("no_candidate"):TEXT("no_troops")):TEXT("pick");if(New2>=0)++FP.Focus2Picks;}
  }
  if(FP.Focus2!=Old2)
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_FOCUS2 team=%d t=%.1f focus2=%d old=%d reason=%s first=%d push=%.2f loss=%.2f free_rifles=%d picks=%d"),
    Team,Elapsed,FP.Focus2,Old2,Why2,Focus,FocusSecNow>=0&&P.Push.IsValidIndex(FocusSecNow)?P.Push[FocusSecNow]:0.,Loss,FP.FreeRifles,FP.Focus2Picks);
 }
 if(Focus!=Old||bTaken)
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_PLAN team=%d t=%.1f focus=%d sec=%d old=%d reason=%s score=%.2f push=%.2f dmg=%.3f known=%d next=%d nextScore=%.2f"),
   Team,Elapsed,Focus,Best>=0?Best+1:0,Old,Why,Best>=0?BestScore:0.,Best>=0?P.Push[Best]:0.,Best>=0?P.Dmg[Best]:0.,Best>=0?P.KnownN[Best]:0,Next>=0?Next+1:0,Next>=0?NextScore:0.);
}

// ---- Front plan, stage 4: section roles (Docs/design-front-strategy.md 2.3, 3.2-3.4; trial -BattleFrontPlanFlex=1, default 0) ----
bool UBattleMatch::FrontPlanFlexOn(int32 Team) const
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontPlanFlex="),0);
 return On!=0&&FrontPlanOn(Team);
}

TCHAR UBattleMatch::FrontRoleOfRow(int32 Team,int32 Row) const
{
 if(Team<0||Team>1)return 0;
 const FFrontPlanTeam& P=FrontPlanState[Team];
 if(!P.SectionOfRow.IsValidIndex(Row)||!P.Role.IsValidIndex(P.SectionOfRow[Row])||!FrontRolesOn(Team))return 0;
 return P.Role[P.SectionOfRow[Row]];
}

void UBattleMatch::UpdateFrontRoles(int32 Team)
{
 FFrontPlanTeam& P=FrontPlanState[Team];
 const int32 NS=P.Sections;if(NS<=0)return;
 if(P.Role.Num()!=NS){P.Role.Init(TEXT('K'),NS);P.RoleAt.Init(Elapsed,NS);}
 // Posture: behind while the own points (observed) are fewer than the points observed as the enemy's, for 30 s (3.2). The score gap
 // part of the design needs the enemy's score, which the side does not observe, so only the points count.
 int32 OwnPts=0,EnemyPts=0;for(int32 I=0;I<Objectives.Num();++I){const int32 O=ObservedOwner(Team,I);OwnPts+=O==Team;EnemyPts+=O==1-Team;}
 if(OwnPts<EnemyPts){if(P.BehindSince<0.f)P.BehindSince=Elapsed;}else P.BehindSince=-1.f;
 const bool bWasBehind=P.bBehind;
 P.bBehind=P.BehindSince>=0.f&&Elapsed-P.BehindSince>=30.f;
 TArray<uint8> InFocus;int32 TargetCol=-1;FrontPlanFocusRows(Team,InFocus,TargetCol);
 const int32 N=P.SampleAt.Num();
 auto DamageOver=[&](int32 K,float Seconds){double D=0;for(int32 J=0;J<N;++J)if(P.SampleAt[J]>Elapsed-Seconds)D+=P.Damage[J*NS+K];return D;};
 bool bChanged=false;
 for(int32 K=0;K<NS;++K)
 {
  bool bFocus=false;for(int32 Y=0;Y<InFocus.Num();++Y)bFocus|=InFocus[Y]&&P.SectionOfRow[Y]==K;
  if(FrontPlanFocus2On(Team)&&Objectives.IsValidIndex(P.Focus2))
   if(const FIntPoint C2=BattleSectors::Cell(Objectives[P.Focus2].Position);BattleSectors::Valid(C2))
    bFocus|=P.SectionOfRow[AreaHome(Team).X<0?C2.Y:BattleSectors::Rows-1-C2.Y]==K;
  const TCHAR Was=P.Role[K];TCHAR Now=Was;
  if(bFocus)Now=TEXT('F');
  else if(P.OwnMask[K]!=0u)
  {
   // Hold (3.3): an own point and 15% of the section's health lost in 20 s, or known enemies ahead 1.2 times the own soldiers there.
   // Back to keep after 30 s without damage.
   const bool bPressed=(P.Hp[K]>0.&&DamageOver(K,20.f)/P.Hp[K]>=.15)||(P.KnownN[K]>=4&&P.KnownN[K]>=1.2*P.Soldiers[K]);
   if(bPressed)Now=TEXT('H');
   else if(Was==TEXT('H'))Now=DamageOver(K,30.f)>0.?TEXT('H'):TEXT('K');
   else Now=TEXT('K');
  }
  else
  {
   // May fall back (3.2): only while behind; a role lasts at least 90 s unless the posture changes.
   const TCHAR Want=P.bBehind?TEXT('R'):TEXT('K');
   Now=Want==Was||(Was!=TEXT('R')&&Was!=TEXT('K'))||P.bBehind!=bWasBehind||Elapsed-P.RoleAt[K]>=90.f?Want:Was;
  }
  if(!FrontPlanFlexOn(Team)&&Now!=TEXT('F'))Now=TEXT('K'); // stage 7 without stage 4: the AI sets only focus and keep
  // Stage 7: the player's role wins (fall back never with an own point inside).
  if(PlayerRolesOn(Team))if(const uint8 Pick=PlayerSectionRole(K))Now=Pick==1?TEXT('F'):Pick==3?TEXT('H'):Pick==5&&FrontSectionOrdersOn(Team)?TEXT('A'):Pick==6&&FrontSectionOrdersOn(Team)?TEXT('B'):Pick==4&&P.OwnMask[K]==0u?TEXT('R'):TEXT('K');
  if(Now!=Was){P.Role[K]=Now;P.RoleAt[K]=Elapsed;++P.RoleChanges;bChanged=true;}
 }
 if(bChanged||Elapsed>=P.RoleLogAt)
 {
  P.RoleLogAt=Elapsed+BattleFrontValues::LogInterval;
  FString Roles;for(int32 K=0;K<NS;++K)Roles.AppendChar(P.Role[K]);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_ROLES team=%d t=%.1f roles=%s behind=%d points=%d/%d changes=%d hold_blocks=%d hold_steps=%d thin_rows=%d"),
   Team,Elapsed,*Roles,P.bBehind?1:0,OwnPts,EnemyPts,P.RoleChanges,P.HoldBlocks,P.HoldSteps,P.ThinNow);
 }
}

// ---- Front plan, stage 5: the second focus (Docs/design-front-strategy.md 3.1; trial -BattleFrontPlanFocus2=1, default 0) ----
bool UBattleMatch::FrontPlanFocus2On(int32 Team) const
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontPlanFocus2="),0);
 return On!=0&&FrontPlanOn(Team);
}

int32 UBattleMatch::GroupFocusPoint(const FBattleGroup& G) const
{
 if(G.bFocus2&&G.Team>=0&&G.Team<2)return FrontPlanState[G.Team].Focus2>=0?FrontPlanState[G.Team].Focus2:INDEX_NONE;
 // Stage 7: the second following player group takes the second focus when there is one.
 if(PlayerGroupFollowsFocus(G)&&Objectives.IsValidIndex(FrontPlanState[G.Team].Focus2))
 {
  int32 Rank=0;for(const auto& P:Groups)if(P.Key<G.ID&&PlayerGroupFollowsFocus(P.Value))++Rank;
  if(Rank==1)return FrontPlanState[G.Team].Focus2;
 }
 return StrategyFocusPoint(G.Team);
}

bool UBattleMatch::PlayerGroupFollowsFocus(const FBattleGroup& G) const
{
 return G.bPlayer&&G.Kind==0&&G.bFollowFocus&&PlayerRolesOn(G.Team)&&GroupAIRun(G);
}

bool UBattleMatch::SetGroupFollowFocus(int32 GroupID,bool bFollow)
{
 FBattleGroup* G=Groups.Find(GroupID);
 if(!G||G->Team!=0||!G->bPlayer||G->Kind!=0)return false;
 G->bFollowFocus=bFollow;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_FOLLOW_FOCUS group=%d follow=%d t=%.1f"),GroupID,bFollow?1:0,Elapsed);
 return true;
}

// ---- Front plan, stage 7: the player's section roles (Docs/design-front-strategy.md 6; trial -BattleFrontPlanPlayer=1, default 0) ----
bool UBattleMatch::FrontPlanPlayerOn()
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontPlanPlayer="),0);
 return On!=0;
}

bool UBattleMatch::PlayerRolesOn(int32 Team) const
{
 return Team==0&&FrontPlanPlayerOn()&&FrontPlanOn(0);
}

bool UBattleMatch::FrontRolesOn(int32 Team) const
{
 return FrontPlanFlexOn(Team)||PlayerRolesOn(Team);
}

uint8 UBattleMatch::PlayerSectionRole(int32 Section) const
{
 return Area.PlayerSectionRole.IsValidIndex(Section)?Area.PlayerSectionRole[Section]:0;
}

const TCHAR* UBattleMatch::SectionRoleJa(uint8 PlayerRole)
{
 static const TCHAR* Names[]={TEXT("AIに任せる"),TEXT("重点"),TEXT("保つ"),TEXT("保持"),TEXT("下がってよい"),TEXT("前進（線まで）"),TEXT("後退")};
 return PlayerRole<UE_ARRAY_COUNT(Names)?Names[PlayerRole]:TEXT("?");
}

bool UBattleMatch::CyclePlayerSectionRole(int32 Section)
{
 const FFrontPlanTeam& P=FrontPlanState[0];
 if(!PlayerRolesOn(0)||Section<0||Section>=P.Sections){LastMessage=TEXT("前線の区間がまだありません");return false;}
 if(Area.PlayerSectionRole.Num()!=P.Sections)Area.PlayerSectionRole.SetNumZeroed(P.Sections);
 // At most two focus sections (one without the second focus trial); never "may fall back" with an own point inside (decision 3).
 int32 Focus=0;for(int32 K=0;K<P.Sections;++K)Focus+=K!=Section&&Area.PlayerSectionRole[K]==1;
 const int32 MaxFocus=FrontPlanFocus2On(0)?2:1;
 const bool bOwn=P.OwnMask.IsValidIndex(Section)&&P.OwnMask[Section]!=0u;
 uint8 Role=Area.PlayerSectionRole[Section];
 const int32 NR=FrontSectionOrdersOn(0)?7:5; // section orders add advance and fall back
 if(Role>=NR)Role=0;
 for(int32 N=0;N<NR;++N){Role=uint8((Role+1)%NR);if((Role==1&&Focus>=MaxFocus)||(Role==4&&bOwn))continue;break;}
 Area.PlayerSectionRole[Section]=Role;
 // The next plan update reviews the focus at once.
 if(Area.StrategyReviewAt.Num()==2)Area.StrategyReviewAt[0]=Elapsed;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_PLAYER_ROLE t=%.1f section=%d role=%d"),Elapsed,Section+1,int32(Role));
 LastMessage=FString::Printf(TEXT("区間%d：%s"),Section+1,SectionRoleJa(Role));
 if(Role==1&&!(P.AheadMask.IsValidIndex(Section)&&P.AheadMask[Section]!=0u))LastMessage+=TEXT("（前に拠点が見えるまで攻撃軍は向かわない）");
 if(Role==5)LastMessage+=Area.FrontTarget.IsEmpty()?TEXT("（「前線を引く」で目標の線を引くと、その線まで1区画ずつ進む）"):TEXT("（引いた線まで1区画ずつ進む）");
 if(Role==6)LastMessage+=TEXT("（1区画ずつ下がる。目標の線があればそこで止まり、無ければ自陣まで。自軍の拠点より後ろへは下がらない）");
 return true;
}

bool UBattleMatch::FrontSectionOrdersOn(int32 Team) const
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontSectionOrders="),0);
 return On!=0&&Team==0&&PlayerRolesOn(0);
}

TCHAR UBattleMatch::SectionOrderOfRow(int32 Team,int32 Row) const
{
 if(!FrontSectionOrdersOn(Team))return 0;
 const TCHAR R=FrontRoleOfRow(Team,Row);
 return R==TEXT('A')||R==TEXT('B')?R:0;
}

void UBattleMatch::RunFrontOrderScript(int32 Team,const TArray<int32>& Hold,int32 HomeCol,int32 EnemyHomeCol)
{
 // Test hook -BattleFrontOrderScript=T:roles:offset (e.g. 120:5,3,6:3): once at match time T, the player's section roles (by section)
 // and a target line offset columns ahead of the line for advancing sections and behind it for falling back ones.
 if(FrontOrderScriptDone||!FrontSectionOrdersOn(Team))return;
 static const FString Spec=[]{FString V;FParse::Value(FCommandLine::Get(),TEXT("BattleFrontOrderScript="),V,false);return V;}();
 if(Spec.IsEmpty())return;
 TArray<FString> Parts;Spec.ParseIntoArray(Parts,TEXT(":"));
 if(Parts.Num()!=3||Elapsed<FCString::Atof(*Parts[0]))return;
 const FFrontPlanTeam& P=FrontPlanState[Team];
 if(P.Sections<=0||P.SectionOfRow.Num()!=Hold.Num())return;
 FrontOrderScriptDone=true;
 TArray<FString> Roles;Parts[1].ParseIntoArray(Roles,TEXT(","));
 Area.PlayerSectionRole.SetNumZeroed(P.Sections);
 for(int32 K=0;K<P.Sections&&K<Roles.Num();++K)Area.PlayerSectionRole[K]=uint8(FMath::Clamp(FCString::Atoi(*Roles[K]),0,6));
 const int32 Off=FCString::Atoi(*Parts[2]);
 Area.FrontTarget.Init(-1,Hold.Num());
 FString T;
 for(int32 Y=0;Y<Hold.Num();++Y)
 {
  const uint8 R=P.SectionOfRow.IsValidIndex(Y)?PlayerSectionRole(P.SectionOfRow[Y]):0;
  if(R==5||R==6)Area.FrontTarget[Y]=FMath::Clamp(Hold[Y]+(R==5?Off:-Off),HomeCol,FMath::Max(HomeCol,EnemyHomeCol-2));
  T+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),Area.FrontTarget[Y]);
 }
 if(Area.StrategyReviewAt.Num()==2)Area.StrategyReviewAt[0]=Elapsed;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_ORDER_SCRIPT team=%d t=%.1f roles=%s target=%s"),Team,Elapsed,*Parts[1],*T);
}
