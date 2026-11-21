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
 bool Gap(){static const int32 On=Arg(TEXT("BattleFrontGap="),1);return On!=0;}
 // The front army holds one line as a group (user report 2026-11-21 after test play: its squads moved each on their own and went back
 // and forth, because the line followed where own soldiers stood and each band stepped forward by itself). The group keeps its line
 // (FBattleGroup::FrontHold): a row falls back only before two or more known enemies; the whole line steps one column forward at once
 // when most squads stand on their posts and no enemy is known ahead. Posts stay laid out for the whole group, also while squads are
 // away for supply. -BattleFrontHold=0: the earlier line from own positions. Needs the front army.
 bool Hold(){static const int32 On=Arg(TEXT("BattleFrontHold="),1);return On!=0&&Army();}
 // Second test play 2026-11-21 19:10 "too slow": half of the squads counted as off their posts (35 m from the sector centre), so the line
 // stepped only every 60 s. Now on the post sector or within one sector of its centre, and shorter waits.
 constexpr float StepHold=10.f, StepLate=30.f; // least time between steps; after StepLate half of the squads on their posts suffice
 constexpr double StepReady=.6, StepReadyLate=.5, OnPost=5000.; // share of squads on their posts; within this of the post (cm)
 // The group's aim is to raise its line (user 2026-11-21 19:15 "push the line forward on purpose, as a group"), judged by its own losses
 // rather than by known enemy numbers (user 19:20 "weigh the damage over the enemy count"; seen enemies depend on sight, own damage is
 // sure). Per row, over its front squads posted in rows +-1: the damage of the last 5 s against their health (UBattleSquadTactics).
 //  - Quiet (no such squad damaged): the row steps forward, unless known enemies just ahead are at least Swamp times the own soldiers.
 //  - Light damage (below LightDamage): steps forward only where own soldiers near the line are PushRatio times the known enemies ahead.
 //  - Heavy damage (BackDamage or more): the row falls back one column, at most once per BackHold seconds.
 constexpr double LightDamage=.03, BackDamage=.10, PushRatio=1.5, Swamp=2.; constexpr int32 SwampMin=4; constexpr float BackHold=10.f;
 // Sector kinds by the cover they give a squad that waits there (river sectors are not held).
 double Cover(char Kind){switch(Kind){case 'H':case 'W':return 3.;case 'T':case '^':return 2.;case 'J':case 'B':case 'S':return 1.;case 'R':return -100.;default:return 0.;}}
}

bool UBattleMatch::FrontLineOn(int32 Team) const
{
 // Default on since 2026-11-21 (user decision after test play).
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontLine="),1);
 if(On==0||!bArea||Team<0||Team>1||!BattleFieldLayout::Of(GetWorld()).bSectors)return false;
 const int32 Trial=BattleTactics::AreaStrategy().TrialTeam;
 return Trial<0||Trial==Team;
}

FIntPoint UBattleMatch::FrontCell(int32 Team,int32 Column,int32 Row) const
{
 // Column 0 is the edge of the team's own home side.
 return FIntPoint(AreaHome(Team).X<0?Column:BattleSectors::Columns-1-Column,Row);
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
 for(const auto& P:OwnCount)if(P.Value>=2)Reach[P.Key.Y]=FMath::Max(Reach[P.Key.Y],ColOf(P.Key));
 for(int32 I=0;I<Objectives.Num();++I)
 {
  const FIntPoint C=BattleSectors::Cell(Objectives[I].Position);if(!BattleSectors::Valid(C))continue;
  const int32 Owner=ObservedOwner(Team,I);
  if(Owner==Team)Reach[C.Y]=FMath::Max(Reach[C.Y],ColOf(C));
  else if(Owner==1-Team)EnemyCol[C.Y]=FMath::Min(EnemyCol[C.Y],ColOf(C));
 }
 // 2. Known enemies: two or more in a sector (one soldier slipping through does not pull the line back to the home).
 TMap<FIntPoint,int32> EnemyCount;
 for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))++EnemyCount.FindOrAdd(C);
 for(const auto& P:EnemyCount)if(P.Value>=2)EnemyCol[P.Key.Y]=FMath::Min(EnemyCol[P.Key.Y],ColOf(P.Key));
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
 const int32 Want=Army()?2*Rows:FMath::Clamp(Arg(TEXT("BattleFrontSquads="),4),2,12);
 int32 FrontID=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==1&&!P.Value.bPlayer)FrontID=P.Key;
 const int32 Have=FrontID!=INDEX_NONE?Groups[FrontID].Squads.Num():0;
 if(Have<Want&&AutoFill())
 {
  const FVector LineMid=BattleSectors::Center(FrontCell(Team,Line[Rows/2],Rows/2));
  struct FFree{int32 SID;double D;};TArray<FFree> Free;
  for(const auto& P:Store->GetSquads())
  {
   const auto& S=P.Value;
   if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(P.Key)!=INDEX_NONE)continue;
   if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Heavy)continue;
   if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key)||ArmyPinned.Contains(P.Key))continue;
   // The real orders never send heavy squads on the home assault (UpdateTeamOrders), so their base slot does not hold them (Codex 2026-11-21).
   bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase&&S.SquadType!=BattleSquadType::Heavy)continue;
   Free.Add({P.Key,FVector::DistSquared2D(R->Position(S.LeaderUnitID),LineMid)});
  }
  Free.Sort([](const FFree& A,const FFree& B){return A.D!=B.D?A.D<B.D:A.SID<B.SID;});
  TArray<int32> Pick;for(const auto& F:Free)if(Have+Pick.Num()<Want)Pick.Add(F.SID);
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
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_JOIN group=%d team=%d joined=%d squads=%d time=%.1f"),FrontID,Team,Pick.Num(),G.Squads.Num(),Elapsed);
  }
 }

 // 6. Bands and posts. Squads away at home for supply leave their band to the others.
 TMap<int32,FIntPoint> Previous; // the front army keeps a squad on its last post when that sector is still wanted
 for(auto It=FrontPosts.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||S->Team==Team){if(S)Previous.Add(It.Key(),BattleSectors::Cell(It.Value()));It.RemoveCurrent();}
 FString Posts;
 if(auto* G=FrontID!=INDEX_NONE?Groups.Find(FrontID):nullptr)
 {
  // 6a. The group's own line (Hold()).
  if(Hold())
  {
   auto& H=G->FrontHold;
   if(H.Num()!=Rows){H=Line;G->bFrontDrawn=false;G->FrontStepAt=Elapsed;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_HOLD team=%d group=%d start time=%.1f"),Team,FrontID,Elapsed);}
   TMap<FIntPoint,int32> EnemyAll;for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))++EnemyAll.FindOrAdd(C);
   auto Near=[&](const TMap<FIntPoint,int32>& M,int32 Y,int32 C0,int32 C1)
   {int32 N=0;for(int32 Z=FMath::Max(0,Y-1);Z<FMath::Min(Rows,Y+2);++Z)for(int32 C=FMath::Max(0,C0);C<=FMath::Min(Cols-1,C1);++C)if(const int32* V=M.Find(FrontCell(Team,C,Z)))N+=*V;return N;};
   // Damage per row: the 5 s damage and the health of the front squads posted within one row of it, and how many were damaged.
   TArray<double> Damage,Health;TArray<int32> Hurt;Damage.Init(0.,Rows);Health.Init(0.,Rows);Hurt.Init(0,Rows);
   if(const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>())
    for(int32 SID:G->Squads)if(const FIntPoint* Last=Previous.Find(SID))if(const auto* O=Tactics->Find(SID))
     for(int32 Y=FMath::Max(0,Last->Y-1);Y<FMath::Min(Rows,Last->Y+2);++Y){Damage[Y]+=O->RecentDamage;Health[Y]+=O->Health;Hurt[Y]+=O->RecentDamage>0.f?1:0;}
   auto Share=[&](int32 Y){return Health[Y]>0.?Damage[Y]/Health[Y]:0.;};
   // A line the player drew (SetDrawnFrontLine) stays where it was drawn until the player draws again or hands it back.
   if(!G->bFrontDrawn)
   {
    int32 Back=0;
    if(Elapsed-G->FrontBackAt>=BackHold)for(int32 Y=0;Y<Rows;++Y)if(Share(Y)>=BackDamage&&H[Y]>HomeCol){--H[Y];++Back;}
    if(Back)
    {
     G->FrontBackAt=Elapsed;
     for(int32 Pass=0;Pass<Rows;++Pass)for(int32 Y=0;Y<Rows;++Y){if(Y>0)H[Y]=FMath::Min(H[Y],H[Y-1]+1);if(Y+1<Rows)H[Y]=FMath::Min(H[Y],H[Y+1]+1);}
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
   const float Since=Elapsed-G->FrontStepAt;const double Ready=Posted?double(OnPosts)/Posted:0.;
   if(!G->bFrontDrawn&&Posted&&Since>=StepHold&&(Ready>=StepReady||(Since>=StepLate&&Ready>=StepReadyLate)))
   {
    TArray<int32> Next=H;int32 Moved=0,Pushed=0;
    for(int32 Y=0;Y<Rows;++Y)if(H[Y]+1<EnemyHomeCol-1)
    {
     const int32 Own=Near(OwnCount,Y,H[Y]-1,H[Y]+1),Enemy=Near(EnemyAll,Y,H[Y],H[Y]+2);
     const bool bQuiet=Hurt[Y]==0&&!(Enemy>=SwampMin&&Enemy>=Swamp*Own);
     const bool bLight=Share(Y)<LightDamage&&Own>=PushRatio*Enemy;
     if(bQuiet||bLight){++Next[Y];++Moved;if(Enemy>0)++Pushed;}
    }
    for(int32 Pass=0;Pass<Rows;++Pass)for(int32 Y=0;Y<Rows;++Y)
    {
     if(Y>0)Next[Y]=FMath::Min(Next[Y],Next[Y-1]+1);
     if(Y+1<Rows)Next[Y]=FMath::Min(Next[Y],Next[Y+1]+1);
    }
    if(Next!=H)
    {
     H=Next;G->FrontStepAt=Elapsed;
     FString L;for(int32 Y=0;Y<Rows;++Y)L+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),H[Y]);
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_STEP team=%d group=%d on_post=%d/%d rows=%d pushed=%d line=%s time=%.1f"),Team,FrontID,OnPosts,Posted,Moved,Pushed,*L,Elapsed);
    }
   }
   Line=H;FrontLine[Team]=Line;
  }
  struct FMember{int32 SID;double Row;};TArray<FMember> Present;
  for(int32 SID:G->Squads)if(const auto* S=Store->FindSquad(SID))
  {
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
   Present.Add({SID,R->Position(S->LeaderUnitID).Y});
  }
  Present.Sort([](const FMember& A,const FMember& B){return A.Row!=B.Row?A.Row<B.Row:A.SID<B.SID;});
  const int32 K=Present.Num();
  // Bands: one per squad of the group with Hold() (a squad away for supply keeps its band empty, so the others keep their posts).
  const int32 NB=Hold()?FMath::Max(K,G->Squads.Num()):K;
  // Front army: sectors with a point not observed as own (it takes the point as it holds the line), and one post per sector.
  TSet<FIntPoint> Used,Points;TArray<FIntPoint> Cells;TArray<bool> Pushes;
  if(Army())for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)!=Team)if(const FIntPoint C=BattleSectors::Cell(Objectives[I].Position);BattleSectors::Valid(C))Points.Add(C);
  for(int32 I=0;I<NB;++I)
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
    double CellScore=Cover(BattleFieldLayout::SectorKind(GetWorld(),Cell))+(C==Target?1.5:0.)-.3*FMath::Abs(Y-Mid);
    if(Army())CellScore+=(Points.Contains(Cell)?2.:0.)-(C<Target-1?1.:0.)-(bPush&&C<Target?3.:0.);
    if(CellScore>BestScore){BestScore=CellScore;Best=Cell;}
   }
   Used.Add(Best);Cells.Add(Best);Pushes.Add(bPush);
  }
  // Squads to posts. The front army: a squad keeps its last post when that sector is still one of them, the rest go to the nearest free
  // post (user report 2026-11-21: squads went back and forth because bands followed the squads' row order, which changes as they move).
  TArray<int32> Owner;Owner.Init(INDEX_NONE,NB);
  if(Army())
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
  for(int32 J=0;J<NB;++J)if(Owner[J]!=INDEX_NONE)
  {
   const int32 SID=Present[Owner[J]].SID;
   FrontPosts.Add(SID,BattleSectors::Center(Cells[J]));
   Posts+=FString::Printf(TEXT(" %d:%c%d%s"),SID,TCHAR('A'+Cells[J].X),1+Cells[J].Y,Pushes[J]?TEXT("+"):TEXT(""));
  }
 }
 // 7. The surprise group (trial -BattleSurpriseGroup=1).
 UpdateSurpriseGroup(Team,FrontID);
 if(Elapsed>=FrontLineLogAt[Team])
 {
  FrontLineLogAt[Team]=Elapsed+LogInterval;
  FString Cells;for(int32 Y=0;Y<Rows;++Y)Cells+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),Line[Y]);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_LINE time=%.0f team=%d line=%s group=%d posts=%s"),Elapsed,Team,*Cells,FrontID,Posts.IsEmpty()?TEXT(" none"):*Posts);
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
 int32 Set=0;
 for(int32 Y=0;Y<Rows;++Y)
 {
  // Where the drawn line crosses the middle of the row (the mean of all its crossings).
  const double RowY=BattleSectors::Center(FIntPoint(0,Y)).Y;double SumX=0;int32 N=0;
  for(int32 I=0;I+1<Points.Num();++I)
  {
   const FVector& A=Points[I];const FVector& B=Points[I+1];
   if(FMath::IsNearlyEqual(A.Y,B.Y)||RowY<FMath::Min(A.Y,B.Y)||RowY>FMath::Max(A.Y,B.Y))continue;
   SumX+=FMath::Lerp(A.X,B.X,(RowY-A.Y)/(B.Y-A.Y));++N;
  }
  if(!N)continue;
  const FIntPoint C=BattleSectors::Cell(FVector(SumX/N,RowY,0.));
  H[Y]=FMath::Clamp(ColOf(FIntPoint(FMath::Clamp(C.X,0,Cols-1),Y)),HomeCol,FMath::Max(HomeCol,EnemyHomeCol-1));++Set;
 }
 if(!Set)return -2;
 G->FrontHold=H;G->bFrontDrawn=true;FrontLine[Team]=H;FrontLineAt[Team]=Elapsed;
 FString L;for(int32 Y=0;Y<Rows;++Y)L+=FString::Printf(TEXT("%s%d"),Y?TEXT(","):TEXT(""),H[Y]);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT FrontLine team=%d drawn rows=%d line=%s time=%.1f"),Team,Set,*L,Elapsed);
 return Set;
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
  if(FCString::Strcmp(Def.Name,TEXT("Lvl_BattleWide_I"))==0)
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
 // Groups survive RestoreSnapshot in the same match. These fields, like Rally, are untouched by a same-time save round trip.
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
  FVector Entry,Exit;if(!BattleSurpriseRoute::Bridge(GetWorld(),Lead,G.Rally,G.SurpriseRow,Entry,Exit))return false;
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
  {if(C.Y<=1&&ColOf(C)>Line[0])++Ahead[0];if(C.Y>=Rows-2&&ColOf(C)>Line[Rows-1])++Ahead[1];}
  const int32 Row=Ahead[0]<Ahead[1]?0:Ahead[1]<Ahead[0]?Rows-1:(Team==0?0:Rows-1);
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

void UBattleMatch::ClampFrontTimers()
{
 // A no-op for the standard save round trip (saved and loaded at the same time); after loading an earlier save the timers set later
 // in the abandoned run would otherwise stall the front line and the surprise group (Codex review 2026-11-21).
 for(int32 T=0;T<2;++T)
 {
  FrontLineAt[T]=FMath::Min(FrontLineAt[T],Elapsed+BattleFrontValues::Interval);
  FrontLineLogAt[T]=FMath::Min(FrontLineLogAt[T],Elapsed+BattleFrontValues::LogInterval);
  SurpriseNextAt[T]=FMath::Min(SurpriseNextAt[T],Elapsed+BattleSurpriseValues::Interval);
 }
 for(auto& P:Groups)if(P.Value.Kind==1){P.Value.FrontStepAt=FMath::Min(P.Value.FrontStepAt,Elapsed);P.Value.FrontBackAt=FMath::Min(P.Value.FrontBackAt,Elapsed);}
 for(auto& P:Groups)if(P.Value.Kind==2)
 {
  auto& G=P.Value;
  G.SurpriseEndAt=FMath::Min(G.SurpriseEndAt,Elapsed+BattleSurpriseValues::AssembleMax+BattleSurpriseValues::Max);
  G.SurpriseLogAt=FMath::Min(G.SurpriseLogAt,Elapsed+30.f);
  if(G.SurpriseWaitAt>Elapsed)G.SurpriseWaitAt=-1.f;
  G.PhaseAt=FMath::Min(G.PhaseAt,Elapsed);
 }
}
