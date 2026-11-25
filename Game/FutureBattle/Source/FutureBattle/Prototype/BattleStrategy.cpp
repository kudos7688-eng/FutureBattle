// Strategy layer (ROADMAP stage 4, Docs/design-strategy-layer.md). Stage S1: the team's situation map (theatres = the objectives'
// Voronoi cells plus the two home zones) built from what the team knows - own soldiers, enemies seen now, scout reports, and
// remembered contacts weighted by age - and the evaluation log BATTLE_SITUATION (-BattleSituationLog=1), which adds the true enemy
// count of each theatre for evaluation only. Nothing here changes the match: no cache is refreshed (AreaIntelView is not called)
// and no decision reads the situation yet.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace BattleSituationValues
{
 constexpr double HomeZone=15000.;       // cm: within 150 m of a home is that home's zone
 constexpr double FreshAge=2.;           // s: a contact seen this recently counts fully (bVisible is false right after a restore)
 constexpr double MemoryAge=90.;         // s: remembered contacts and scout reports weigh 1 - age / MemoryAge (report delay + walk)
 constexpr float MemoryConfidence=.35f;  // remembered contacts above this confidence count (the minimap's remembered dots)
 constexpr double UnknownAge=60.;        // s: neither the point nor an enemy in the theatre seen for this long = unknown
 constexpr double EnemyAhead=1.2,OwnAhead=1.5;
 constexpr double EnemyAheadMin=3.;      // known enemies (fresh + weighted memory) needed for "enemy ahead"
 constexpr double LogEvery=30.;          // s of match time between BATTLE_SITUATION blocks
}

bool UBattleMatch::SituationLogOn()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSituationLog="),V);return V!=0;}();
 return bOn;
}

int32 UBattleMatch::TheatreOf(int32 Team,const FVector& P) const
{
 const int32 N=Objectives.Num();if(N<1||Team<0||Team>1)return INDEX_NONE;
 if(FVector::DistSquared2D(P,AreaHome(1-Team))<=FMath::Square(BattleSituationValues::HomeZone))return N;
 if(FVector::DistSquared2D(P,AreaHome(Team))<=FMath::Square(BattleSituationValues::HomeZone))return N+1;
 // Sector maps: by path over passable sectors (mountains, lakes and rivers are not crossed; BuildSectorTheatres).
 if(BattleFieldLayout::Of(GetWorld()).bSectors)
 {
  BuildSectorTheatres();
  const FIntPoint Cell=BattleSectors::Cell(P);
  if(BattleSectors::Valid(Cell))if(const int32 Th=SectorTheatres.IsValidIndex(Cell.Y*BattleSectors::Columns+Cell.X)?SectorTheatres[Cell.Y*BattleSectors::Columns+Cell.X]:-1;Th>=0&&Th<N)return Th;
 }
 // As AreaFronts: lanes by Y on three-objective maps (A-D), else the nearest objective.
 auto Dist=[&](int32 I){return N==3?FMath::Abs(P.Y-Objectives[I].Position.Y):FVector::DistSquared2D(P,Objectives[I].Position);};
 int32 Best=0;double BestD=Dist(0);
 for(int32 I=1;I<N;++I){const double D=Dist(I);if(D<BestD){BestD=D;Best=I;}}
 return Best;
}

namespace
{
 // Breadth-first distances in sector steps from Start over open sectors (4 neighbours, diagonals when both sides are open);
 // MAX_int32 where there is no path. Closed (optional, one flag per sector) marks open sectors not to enter (threatened passes, 3-4).
 void SectorSteps(const UWorld* World,FIntPoint Start,const TArray<bool>* Closed,TArray<int32>& Out)
 {
  using namespace BattleSectors;
  auto Index=[](FIntPoint C){return C.Y*Columns+C.X;};
  auto Open=[&](FIntPoint C){return Valid(C)&&!Impassable(BattleFieldLayout::SectorKind(World,C))&&!(Closed&&(*Closed)[Index(C)]);};
  Out.Init(MAX_int32,Columns*Rows);if(!Valid(Start))return;
  TArray<FIntPoint> Queue;Queue.Add(Start);Out[Index(Start)]=0;
  for(int32 Head=0;Head<Queue.Num();++Head)
  {
   const FIntPoint Here=Queue[Head];const int32 D=Out[Index(Here)];
   for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
   {
    if(!DX&&!DY)continue;const FIntPoint To(Here.X+DX,Here.Y+DY);if(!Open(To))continue;
    if(DX&&DY&&(!Open(FIntPoint(Here.X+DX,Here.Y))||!Open(FIntPoint(Here.X,Here.Y+DY))))continue;
    if(Out[Index(To)]<=D+1)continue;
    Out[Index(To)]=D+1;Queue.Add(To);
   }
  }
 }
}

void UBattleMatch::BuildSectorTheatres() const
{
 const auto& Def=BattleFieldLayout::Of(GetWorld());
 if(!Def.bSectors){SectorTheatres.Reset();TheatreNeighbours.Reset();StrategyPasses.Reset();PassOfCell.Reset();return;}
 if(SectorTheatresMap==Def.Name&&SectorTheatres.Num()==BattleSectors::Columns*BattleSectors::Rows)return;
 using namespace BattleSectors;
 const int32 N=Objectives.Num();const int32 Cells=Columns*Rows;
 SectorTheatres.Init(-1,Cells);TheatreNeighbours.Init(0,FMath::Max(0,N));TheatreSteps.Init(MAX_int32,FMath::Max(0,N*N));HomeTheatreSteps.Init(MAX_int32,FMath::Max(0,2*N));SectorTheatresMap=Def.Name;
 StrategyPasses.Reset();PassOfCell.Init(-1,Cells);
 auto Index=[](FIntPoint C){return C.Y*Columns+C.X;};
 auto Open=[&](FIntPoint C){return Valid(C)&&!Impassable(BattleFieldLayout::SectorKind(GetWorld(),C));};
 // Passes (2, 3-4): an open sector closed on both sides along exactly one axis; its exits are the open sectors across the other.
 for(int32 Y=0;Y<Rows;++Y)for(int32 X=0;X<Columns;++X)
 {
  const FIntPoint C(X,Y);if(!Open(C)||StrategyPasses.Num()>=100)continue;
  const bool bClosedX=!Open(FIntPoint(X-1,Y))&&!Open(FIntPoint(X+1,Y)),bClosedY=!Open(FIntPoint(X,Y-1))&&!Open(FIntPoint(X,Y+1));
  if(bClosedX==bClosedY)continue; // neither: open ground; both: an enclosed sector, not a way through
  const int32 P=StrategyPasses.Add(C);PassOfCell[Index(C)]=int8(P);
  const FIntPoint ExitA=bClosedX?FIntPoint(X,Y-1):FIntPoint(X-1,Y),ExitB=bClosedX?FIntPoint(X,Y+1):FIntPoint(X+1,Y);
  for(const FIntPoint E:{ExitA,ExitB})if(Open(E)&&PassOfCell[Index(E)]<0)PassOfCell[Index(E)]=int8(P);
 }
 if(N<1||N>16)return;
 // Breadth-first distances in sector steps from every objective's sector.
 TArray<TArray<int32>> Steps;Steps.SetNum(N);
 for(int32 I=0;I<N;++I)SectorSteps(GetWorld(),Cell(Objectives[I].Position),nullptr,Steps[I]);
 // Path steps between objectives and from both homes (3-3).
 for(int32 I=0;I<N;++I)
 {
  for(int32 J=0;J<N;++J)if(const FIntPoint To=Cell(Objectives[J].Position);Valid(To))TheatreSteps[I*N+J]=Steps[I][Index(To)];
  for(int32 T=0;T<2;++T)if(const FIntPoint To=Cell(AreaHome(T));Valid(To))HomeTheatreSteps[T*N+I]=Steps[I][Index(To)];
 }
 for(int32 C=0;C<Cells;++C)
 {
  int32 Best=-1,BestSteps=MAX_int32;
  for(int32 I=0;I<N;++I)if(Steps[I][C]<BestSteps){BestSteps=Steps[I][C];Best=I;}
  SectorTheatres[C]=int8(Best); // -1 for mountains, lakes and cut-off sectors: TheatreOf falls back to the nearest objective
 }
 // Theatres touch where two open sectors side by side belong to different theatres.
 for(int32 X=0;X<Columns;++X)for(int32 Y=0;Y<Rows;++Y)
 {
  const int32 A=SectorTheatres[Index(FIntPoint(X,Y))];if(A<0)continue;
  for(const FIntPoint To:{FIntPoint(X+1,Y),FIntPoint(X,Y+1)})
   if(Valid(To))if(const int32 B=SectorTheatres[Index(To)];B>=0&&B!=A){TheatreNeighbours[A]|=uint16(1u<<B);TheatreNeighbours[B]|=uint16(1u<<A);}
 }
}

void UBattleMatch::BuildSituation(int32 Team,FBattleSituation& Out) const
{
 Out=FBattleSituation();
 const int32 N=Objectives.Num();if(N<1||Team<0||Team>1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();const auto* T=GetDefault<UBattleTuning>();
 if(!Store||!R)return;
 using namespace BattleSituationValues;
 const double Now=GetWorld()->GetTimeSeconds();
 Out.At=Elapsed;Out.Theatres.SetNum(N+2);
 // Sector maps: the known enemies at each pass are counted with the theatres (3-4).
 if(BattleFieldLayout::Of(GetWorld()).bSectors)BuildSectorTheatres();
 Out.PassThreat.Init(0.,StrategyPasses.Num());
 TArray<FGroupStrength> OwnPower;OwnPower.SetNum(N+2);
 // Known enemies by unit ID: the freshest sighting among the team's soldiers (soldiers in ID order, so ties keep the same one).
 struct FKnown{FVector At=FVector::ZeroVector;double Age=0;uint8 Type=255;};
 TMap<int32,FKnown> Known;
 TArray<int32> IDs;Store->GetUnits().GetKeys(IDs);IDs.Sort();
 for(int32 ID:IDs)
 {
  const auto* U=Store->Find(ID);if(!U||U->Team!=Team||!U->IsAlive())continue;
  const FVector P=R->Position(ID);
  if(const auto* S=Store->FindSquad(U->SquadID);S&&GroupSquadPresent(*S))
   if(const int32 Th=TheatreOf(Team,P);Th!=INDEX_NONE){++Out.Theatres[Th].Own;OwnPower[Th].Add(S->SquadType);}
  const auto* Contacts=Perception?Perception->GetContacts(ID):nullptr;if(!Contacts)continue;
  for(const auto& C:*Contacts)
  {
   const FBattleContact& K=C.Value;
   // Long sight enters the team's knowledge only through the scouts' reports (as AreaIntelView).
   if(IsScoutLongContact(ID,K))continue;
   const double Age=K.bVisible?0.:FMath::Max(0.,Now-K.LastSeenTime);
   if(!K.bVisible&&(K.Confidence<=MemoryConfidence||Age>=MemoryAge))continue;
   const FKnown* Old=Known.Find(C.Key);
   if(!Old||Age<Old->Age){FKnown New;New.At=K.LastKnownPosition;New.Age=Age;New.Type=K.ObservedSquadType;Known.Add(C.Key,New);}
  }
 }
 // An enemy of unknown type counts as one rifleman (one seen soldier is one soldier; no guess at what else is there).
 const double RifleWeight=T->GroupAntiInfantry.IsValidIndex(BattleSquadType::Rifle)&&FMath::IsFinite(T->GroupAntiInfantry[BattleSquadType::Rifle])?
  double(FMath::Clamp(T->GroupAntiInfantry[BattleSquadType::Rifle],0.f,100.f)):1.;
 auto AddEnemy=[&](const FVector& At,double Age,uint8 Type)
 {
  const int32 Th=TheatreOf(Team,At);if(Th==INDEX_NONE)return;
  auto& X=Out.Theatres[Th];
  const double W=Age<=FreshAge?1.:FMath::Max(0.,1.-Age/MemoryAge);if(W<=0)return;
  if(Age<=FreshAge)++X.Fresh;else X.Memory+=W;
  FGroupStrength One;One.Add(Type);X.KnownPower+=W*(One.Total()+One.Unknown*RifleWeight);
  if(X.EnemySeenAge<0||Age<X.EnemySeenAge)X.EnemySeenAge=Age;
  if(Out.PassThreat.Num())
   if(const FIntPoint PassCell=BattleSectors::Cell(At);BattleSectors::Valid(PassCell))
    if(const int32 PassIndex=PassOfCell.IsValidIndex(PassCell.Y*BattleSectors::Columns+PassCell.X)?PassOfCell[PassCell.Y*BattleSectors::Columns+PassCell.X]:-1;
       Out.PassThreat.IsValidIndex(PassIndex))Out.PassThreat[PassIndex]+=W;
 };
 TArray<int32> Keys;Known.GetKeys(Keys);Keys.Sort();
 for(int32 Key:Keys){const FKnown& K=Known[Key];AddEnemy(K.At,K.Age,K.Type);}
 // Scout reports of the last ScoutReportMemory seconds: positions only (no identity), aged from the report.
 TArray<int32> Scouts;ScoutReports.GetKeys(Scouts);Scouts.Sort();
 for(int32 SID:Scouts)
 {
  const FScoutReport& Rep=ScoutReports[SID];const auto* S=Store->FindSquad(SID);
  if(!S||S->Team!=Team||Rep.ReportedAt<0||Elapsed-Rep.ReportedAt>T->ScoutReportMemory)continue;
  for(const FVector& At:Rep.Positions)AddEnemy(At,FMath::Max(0.,Elapsed-Rep.ReportedAt),255);
 }
 // Points: when the team last observed each point (Area.SeenAt, from the existing 0.2 s point checks) and its observed owner.
 for(int32 I=0;I<N+2;++I)
 {
  auto& X=Out.Theatres[I];X.OwnPower=OwnPower[I].Total();
  if(I<N)
  {
   const int32 Slot=Team*N+I;
   if(Area.SeenAt.IsValidIndex(Slot)&&Area.SeenAt[Slot]>=0)X.PointSeenAge=FMath::Max(0.,Elapsed-Area.SeenAt[Slot]);
   X.Owner=ObservedOwner(Team,I);
  }
  const double KnownCount=X.Fresh+X.Memory;
  const bool bPointStale=I>=N||X.PointSeenAge<0||X.PointSeenAge>UnknownAge,bEnemyStale=X.EnemySeenAge<0||X.EnemySeenAge>UnknownAge;
  if(bPointStale&&bEnemyStale)X.State=0;
  else if(KnownCount>=EnemyAheadMin&&X.KnownPower>=X.OwnPower*EnemyAhead)X.State=1;
  else if(X.OwnPower>0&&X.OwnPower>=X.KnownPower*OwnAhead)X.State=3;
  else X.State=2;
 }
}

void UBattleMatch::UpdateSituationLog()
{
 if(!SituationLogOn()||!bArea||Phase!=EBattleMatchPhase::Combat||Objectives.IsEmpty())return;
 const int32 Bucket=FMath::FloorToInt(Elapsed/BattleSituationValues::LogEvery);
 if(Bucket==SituationLogBucket)return;
 SituationLogBucket=Bucket;if(Bucket<1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return;
 const int32 N=Objectives.Num();
 static const TCHAR* States[]={TEXT("unknown"),TEXT("enemy"),TEXT("even"),TEXT("own")};
 TArray<int32> IDs;Store->GetUnits().GetKeys(IDs);IDs.Sort();
 for(int32 Team=0;Team<2;++Team)
 {
  FBattleSituation S;BuildSituation(Team,S);if(S.Theatres.Num()!=N+2)continue;
  // Evaluation only: the true enemy soldiers in each theatre, counted here and written to the log; no function returns them.
  TArray<int32> TrueCount;TrueCount.Init(0,N+2);TArray<FGroupStrength> TruePower;TruePower.SetNum(N+2);
  for(int32 ID:IDs)
  {
   const auto* U=Store->Find(ID);if(!U||U->Team==Team||!U->IsAlive())continue;
   const int32 Th=TheatreOf(Team,R->Position(ID));if(Th==INDEX_NONE)continue;
   ++TrueCount[Th];if(const auto* Sq=Store->FindSquad(U->SquadID))TruePower[Th].Add(Sq->SquadType);
  }
  for(int32 I=0;I<N+2;++I)
  {
   const auto& X=S.Theatres[I];
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SITUATION team=%d t=%.0f theatre=%d kind=%s state=%s own=%d ownP=%.1f fresh=%d mem=%.1f knownP=%.1f seenAge=%.0f pointAge=%.0f owner=%d | true=%d trueP=%.1f"),
    Team,Elapsed,I,I<N?TEXT("point"):I==N?TEXT("enemyhome"):TEXT("ownhome"),States[FMath::Min<int32>(X.State,3)],X.Own,X.OwnPower,X.Fresh,X.Memory,X.KnownPower,
    X.EnemySeenAge,X.PointSeenAge,X.Owner,TrueCount[I],TruePower[I].Total());
  }
 }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Values shared by the front plan (BattleFrontLine.cpp): the opening before the first focus and the least time a focus is kept.
// The older strategy layer S2-S4 (-BattleStrategy=2/3/4, -BattleStrategyPass) was removed on 2026-11-25 (user decision,
// Docs/review-2026-11-25); its last results can be reproduced from main b470127.
namespace BattleStrategyValues
{
 float Seconds(const TCHAR* Name,float Default,float Min)
 {
  float V=Default;FParse::Value(FCommandLine::Get(),Name,V);
  return FMath::IsFinite(V)?FMath::Max(Min,V):Default;
 }
}

float UBattleMatch::StrategyOpenSeconds()
{
 static const float V=BattleStrategyValues::Seconds(TEXT("BattleStrategyOpen="),150.f,0.f);
 return V;
}

float UBattleMatch::StrategyFocusHoldSeconds()
{
 static const float V=BattleStrategyValues::Seconds(TEXT("BattleStrategyFocusHold="),90.f,0.f);
 return V;
}

int32 UBattleMatch::StrategyFocusPoint(int32 Team) const
{
 // The front plan (BattleFrontLine.cpp, -BattleFrontPlan=1) writes its focus section's target point here too.
 if(!bArea||Team<0||Team>1||!FrontPlanOn(Team)||!Area.StrategyFocus.IsValidIndex(Team))return INDEX_NONE;
 const int32 F=Area.StrategyFocus[Team];
 return Objectives.IsValidIndex(F)?F:INDEX_NONE;
}
