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
// Inputs: own soldiers' positions, known enemy positions (AreaIntelView), observed ownership, the fixed sector kinds. Not saved.
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleSectors.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "FutureBattle.h"

namespace BattleFrontValues
{
 constexpr float Interval=2.f, LogInterval=30.f;
 constexpr int32 ClearAhead=3; // columns ahead of a band without known enemies before its squad steps forward
 int32 Arg(const TCHAR* Name,int32 Default){int32 V=Default;FParse::Value(FCommandLine::Get(),Name,V);return V;}
 // Sector kinds by the cover they give a squad that waits there (river sectors are not held).
 double Cover(char Kind){switch(Kind){case 'H':case 'W':return 3.;case 'T':case '^':return 2.;case 'J':case 'B':case 'S':return 1.;case 'R':return -100.;default:return 0.;}}
}

bool UBattleMatch::FrontLineOn(int32 Team) const
{
 static const int32 On=BattleFrontValues::Arg(TEXT("BattleFrontLine="),0);
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
 const int32 Want=FMath::Clamp(Arg(TEXT("BattleFrontSquads="),4),2,12);
 int32 FrontID=INDEX_NONE;for(const auto& P:Groups)if(P.Value.Team==Team&&P.Value.Kind==1&&!P.Value.bPlayer)FrontID=P.Key;
 const int32 Have=FrontID!=INDEX_NONE?Groups[FrontID].Squads.Num():0;
 if(Have<Want)
 {
  const FVector LineMid=BattleSectors::Center(FrontCell(Team,Line[Rows/2],Rows/2));
  struct FFree{int32 SID;double D;};TArray<FFree> Free;
  for(const auto& P:Store->GetSquads())
  {
   const auto& S=P.Value;
   if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(P.Key)!=INDEX_NONE)continue;
   if(S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Heavy)continue;
   if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key))continue;
   bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase)continue;
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
 for(auto It=FrontPosts.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||S->Team==Team)It.RemoveCurrent();
 FString Posts;
 if(const auto* G=FrontID!=INDEX_NONE?Groups.Find(FrontID):nullptr)
 {
  struct FMember{int32 SID;double Row;};TArray<FMember> Present;
  for(int32 SID:G->Squads)if(const auto* S=Store->FindSquad(SID))
  {
   if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))continue;
   Present.Add({SID,R->Position(S->LeaderUnitID).Y});
  }
  Present.Sort([](const FMember& A,const FMember& B){return A.Row!=B.Row?A.Row<B.Row:A.SID<B.SID;});
  const int32 K=Present.Num();
  for(int32 I=0;I<K;++I)
  {
   const int32 Y0=I*Rows/K,Y1=FMath::Max(Y0+1,(I+1)*Rows/K);
   // The band's line: its rearmost row. Step forward when no enemy is known within ClearAhead columns ahead of the band (rows +-1).
   int32 Col=Cols;for(int32 Y=Y0;Y<Y1;++Y)Col=FMath::Min(Col,Line[Y]);
   int32 Nearest=Cols;for(int32 Y=FMath::Max(0,Y0-1);Y<FMath::Min(Rows,Y1+1);++Y)Nearest=FMath::Min(Nearest,EnemyCol[Y]);
   const bool bPush=Nearest-Col>ClearAhead&&Col+1<EnemyHomeCol-1;
   const int32 Target=bPush?Col+1:Col;
   const double Mid=(Y0+Y1-1)*.5;
   double BestScore=-1e9;FIntPoint Best=FrontCell(Team,Target,FMath::RoundToInt(Mid));
   for(int32 Y=Y0;Y<Y1;++Y)for(int32 C=FMath::Max(HomeCol,Target-1);C<=Target;++C)
   {
    const FIntPoint Cell=FrontCell(Team,C,Y);if(!BattleSectors::Valid(Cell))continue;
    const double CellScore=Cover(BattleFieldLayout::SectorKind(GetWorld(),Cell))+(C==Target?1.5:0.)-.3*FMath::Abs(Y-Mid);
    if(CellScore>BestScore){BestScore=CellScore;Best=Cell;}
   }
   FrontPosts.Add(Present[I].SID,BattleSectors::Center(Best));
   Posts+=FString::Printf(TEXT(" %d:%c%d%s"),Present[I].SID,TCHAR('A'+Best.X),1+Best.Y,bPush?TEXT("+"):TEXT(""));
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
// enemy home (phase 2). It ends when it lost more than half its soldiers or SurpriseMax seconds passed; its squads then return to the
// normal allocation. Inputs: own positions, known enemies (AreaIntelView), the line, fixed homes.
namespace BattleSurpriseValues
{
 constexpr float Interval=240.f, Max=420.f, AssembleMax=60.f, FirstAt=120.f; // the first one after the opening
 constexpr double LossEnd=.5;
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
 // Form: a front-holding group exists, none running, the interval passed.
 if(SID_==INDEX_NONE)
 {
  if(FrontID==INDEX_NONE||Elapsed<SurpriseNextAt[Team]||Elapsed<FirstAt)return;
  const int32 Want=FMath::Clamp(BattleFrontValues::Arg(TEXT("BattleSurpriseSquads="),2),1,6);
  // The edge row with fewer known enemies ahead of the line (rows 0-1 or Rows-2..Rows-1).
  int32 Ahead[2]={0,0};
  for(const FVector& E:AreaIntelView(Team).Enemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))
  {if(C.Y<=1&&ColOf(C)>Line[0])++Ahead[0];if(C.Y>=Rows-2&&ColOf(C)>Line[Rows-1])++Ahead[1];}
  const int32 Row=Ahead[0]<Ahead[1]?0:Ahead[1]<Ahead[0]?Rows-1:(Team==0?0:Rows-1);
  const FVector Start=BattleSectors::Center(FrontCell(Team,FMath::Max(0,Line[Row]-1),Row));
  struct FFree{int32 SID;double D;bool bArmour;};TArray<FFree> Free;
  for(const auto& P:Store->GetSquads())
  {
   const auto& S=P.Value;
   if(S.Team!=Team||S.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!S.bPlayerDelegated)||GroupOf(P.Key)!=INDEX_NONE)continue;
   const bool bArmour=S.SquadType==BattleSquadType::Tank||S.SquadType==BattleSquadType::ArmoredCar||S.SquadType==BattleSquadType::Exo;
   if(!bArmour&&S.SquadType!=BattleSquadType::Rifle&&S.SquadType!=BattleSquadType::Heavy)continue;
   if(Area.HirePurpose.Contains(P.Key)||IntelPurpose.Contains(P.Key))continue;
   bool bBase=false;AreaStrategyTarget(P.Key,Team,bBase);if(bBase)continue;
   // Full-strength squads only: the walk is long.
   int32 Living=0;for(int32 UID:S.Members)if(const auto* U=Store->Find(UID);U&&U->IsAlive())++Living;
   if(Living<S.Members.Num())continue;
   Free.Add({P.Key,FVector::DistSquared2D(R->Position(S.LeaderUnitID),Start),bArmour});
  }
  // Armour first (nearest first), then infantry; none without armour.
  Free.Sort([](const FFree& A,const FFree& B){return A.bArmour!=B.bArmour?A.bArmour:A.D!=B.D?A.D<B.D:A.SID<B.SID;});
  if(Free.Num()<FMath::Max(2,Want)||!Free[0].bArmour)return;
  TArray<int32> Pick;for(const auto& F:Free)if(Pick.Num()<Want)Pick.Add(F.SID);
  const int32 ID=FormGroup(Team,Pick,false);auto* G=Groups.Find(ID);if(!G)return;
  G->Kind=2;G->Phase=0;G->PhaseAt=Elapsed;G->Rally=Start;G->SurpriseRow=Row;G->StartLiving=AllLiving(*G);
  SurpriseNextAt[Team]=Elapsed+Interval;
  int32 Armour=0;for(int32 I=0;I<Pick.Num();++I)Armour+=Free[I].bArmour;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_FORM group=%d team=%d squads=%d armour=%d row=%d start=%c%d time=%.1f"),ID,Team,G->Squads.Num(),Armour,Row,TCHAR('A'+BattleSectors::Cell(Start).X),1+BattleSectors::Cell(Start).Y,Elapsed);
  return;
 }
 auto& G=Groups[SID_];
 const int32 Living=AllLiving(G);
 if(G.StartLiving>0&&Living<G.StartLiving*(1.-LossEnd)){DissolveGroup(SID_,TEXT("surprise_losses"));SurpriseNextAt[Team]=Elapsed+Interval;return;}
 if(G.Phase>0&&Elapsed-G.PhaseAt>Max){DissolveGroup(SID_,TEXT("surprise_time"));return;}
 // The group's column: its rearmost squad's.
 int32 Col=Cols;FVector Center=FVector::ZeroVector;int32 N=0,Ready=0;
 for(int32 SID:G.Squads)if(const auto* S=Store->FindSquad(SID))
 {const FVector P=R->Position(S->LeaderUnitID);Col=FMath::Min(Col,ColOf(BattleSectors::Cell(P)));Center+=P;++N;Ready+=FVector::Dist2D(P,G.Rally)<=2500.;}
 if(!N)return;
 auto Enter=[&](uint8 To,const TCHAR* Why){G.Phase=To;G.PhaseAt=Elapsed;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SURPRISE_PHASE group=%d team=%d phase=%d column=%d living=%d reason=%s time=%.1f"),SID_,Team,int32(To),Col,Living,Why,Elapsed);};
 if(G.Phase==0)
 {
  if(Ready>=N||Elapsed-G.PhaseAt>AssembleMax)Enter(1,Ready>=N?TEXT("assembled"):TEXT("assemble_timeout"));
  return;
 }
 if(G.Phase==1)
 {
  if(Col>=EnemyHomeCol-2){Enter(2,TEXT("strike"));return;}
  // Next stop: two columns ahead along the edge row (the inner neighbour row when the edge sector is river).
  const int32 Next=FMath::Min(Col+2,EnemyHomeCol-2);
  FIntPoint Cell=FrontCell(Team,Next,G.SurpriseRow);
  if(BattleFieldLayout::SectorKind(GetWorld(),Cell)=='R')Cell.Y+=G.SurpriseRow==0?1:-1;
  G.Rally=BattleSectors::Center(Cell);
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

