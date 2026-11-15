#include "BattleWarScore.h"
#include "BattleMatch.h"
#include "BattleGameMode.h"

BattleWarScore::FTuning& BattleWarScore::MutableTuning()
{
 static FTuning Value=[]()
 {
  FTuning T;int32 Enabled=0;const TCHAR* Line=FCommandLine::Get();
  FParse::Value(Line,TEXT("BattleAreaWarScore="),Enabled);T.bEnabled=Enabled!=0;
  FParse::Value(Line,TEXT("BattleAreaWarPointSeconds="),T.PointSeconds);FParse::Value(Line,TEXT("BattleAreaWarResourceValue="),T.ResourceValue);
  FParse::Value(Line,TEXT("BattleAreaWarTarget="),T.Target);
  T.PointSeconds=FMath::IsFinite(T.PointSeconds)?FMath::Clamp(T.PointSeconds,1.f,600.f):5.f;
  T.ResourceValue=FMath::Clamp(T.ResourceValue,0,100);T.Target=FMath::Clamp(T.Target,0,100000);
  return T;
 }();
 return Value;
}
const BattleWarScore::FTuning& BattleWarScore::Tuning() { return MutableTuning(); }

static void EnsureWarArrays(FBattleAreaState& Area)
{
 if(Area.WarPoint.Num()!=2)Area.WarPoint={0,0};
 if(Area.WarLoss.Num()!=2)Area.WarLoss={0,0};
 if(Area.WarPointEstimate.Num()!=2)Area.WarPointEstimate={0,0};
}

int32 UBattleMatch::WarEarned(int32 Team) const
{
 return (Area.WarPoint.IsValidIndex(Team)?Area.WarPoint[Team]:0)+(Area.WarLoss.IsValidIndex(Team)?Area.WarLoss[Team]:0);
}

int32 UBattleMatch::WarScore(int32 Team) const
{
 return WarEarned(Team)+Credits[Team]*BattleWarScore::Tuning().ResourceValue;
}

int32 UBattleMatch::WarScoreEstimate(int32 Observer) const
{
 // The opponent's loss score equals the observer's own losses, which the observer knows exactly.
 const int32 Opponent=1-Observer;
 return (Area.WarLoss.IsValidIndex(Opponent)?Area.WarLoss[Opponent]:0)+(Area.WarPointEstimate.IsValidIndex(Observer)?Area.WarPointEstimate[Observer]:0);
}

void UBattleMatch::RecordWarLoss(int32 LostTeam)
{
 const auto& W=BattleWarScore::Tuning();
 if(!bArea||!W.bEnabled||Phase!=EBattleMatchPhase::Combat||LostTeam<0||LostTeam>1)return;
 EnsureWarArrays(Area);Area.WarLoss[1-LostTeam]+=W.PerLoss;
}

void UBattleMatch::UpdateWarScore(float Dt)
{
 const auto& W=BattleWarScore::Tuning();
 if(!bArea||!W.bEnabled||Phase!=EBattleMatchPhase::Combat)return;
 EnsureWarArrays(Area);
 Area.WarClock+=Dt;
 while(Area.WarClock>=W.PointSeconds)
 {
  Area.WarClock-=W.PointSeconds;
  for(int32 Team=0;Team<2;++Team)for(int32 Point=0;Point<Objectives.Num();++Point)
  {
   // True ownership is the game rule (like income); the estimate uses only what the observer last saw.
   if(Objectives[Point].Owner==Team)Area.WarPoint[Team]+=W.PerPoint;
   if(ObservedOwner(Team,Point)==1-Team)Area.WarPointEstimate[Team]+=W.PerPoint;
  }
 }
 if(W.Target>0&&(WarEarned(0)>=W.Target||WarEarned(1)>=W.Target))
 {
  const int32 A=WarEarned(0),B=WarEarned(1);Score[0]=WarScore(0);Score[1]=WarScore(1);
  Finish(A==B?INDEX_NONE:A>B?0:1,A==B?TEXT("DRAW / WAR SCORE REACHED"):A>B?TEXT("VICTORY / WAR SCORE REACHED"):TEXT("DEFEAT / WAR SCORE REACHED"));
 }
}

FString UBattleMatch::WarScoreBreakdown(int32 Team) const
{
 return FString::Printf(TEXT("%d = POINTS %d + ENEMY LOSSES %d + RESOURCES %d"),WarScore(Team),Area.WarPoint.IsValidIndex(Team)?Area.WarPoint[Team]:0,
  Area.WarLoss.IsValidIndex(Team)?Area.WarLoss[Team]:0,Credits[Team]*BattleWarScore::Tuning().ResourceValue);
}
