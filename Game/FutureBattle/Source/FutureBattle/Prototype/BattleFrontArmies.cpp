// Front armies (trial -BattleFrontArmies=N, default 0; user decision 2026-11-27: "which army holds which front and which squads are
// there was hard to see; split the front army, each with its squads and its stretch of the front").
// The front-holding group keeps one continuous line for the team (UpdateFrontLine 1-4 and 6a: the line, its steps and fall-backs by
// row), and its squads are split into N armies, each owning a band of rows:
//  - Bands: the front plan's sections when the team has them (-BattleFrontPlan), else N bands of about equal rows (N = 2..5; 1 means 3).
//  - Each squad of the group belongs to one army. It takes posts in its army's rows (UpdateFrontLine 6: a post in another army's rows
//    costs more than any row distance), so armies do not mix.
//  - An army's need is the number of posts in its rows. A squad joining the group goes to the army short of the most squads.
//  - Every RebalanceHold seconds, one squad moves from the army with the most squads above its need to the army furthest below it
//    (the one whose last post is nearest that army's rows), unless the player put it in its army by hand.
// Log BATTLE_FRONT_ARMIES (every 30 s) and BATTLE_FRONT_ARMY_MOVE. The armies are shown in the army list and on the map's front line.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleSectors.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace BattleFrontArmyValues
{
 constexpr float RebalanceHold=20.f; // s between moves of one squad from army to army
 constexpr float LogEvery=30.f;
}

int32 UBattleMatch::FrontArmiesWanted()
{
 static const int32 V=[]{int32 N=0;FParse::Value(FCommandLine::Get(),TEXT("BattleFrontArmies="),N);return N<=0?0:N==1?3:FMath::Clamp(N,2,5);}();
 return V;
}

int32 UBattleMatch::FrontArmyCount(int32 Team) const
{
 return Team>=0&&Team<2?FrontArmyFirst[Team].Num():0;
}

int32 UBattleMatch::FrontArmyOfRow(int32 Team,int32 Row) const
{
 if(Team<0||Team>1||FrontArmyFirst[Team].IsEmpty())return INDEX_NONE;
 int32 A=0;for(int32 I=0;I<FrontArmyFirst[Team].Num();++I)if(Row>=FrontArmyFirst[Team][I])A=I;
 return A;
}

int32 UBattleMatch::FrontArmyOfSquad(int32 SquadID) const
{
 const int8* A=FrontArmyOf.Find(SquadID);return A?int32(*A):INDEX_NONE;
}

bool UBattleMatch::SetFrontArmy(int32 SquadID,int32 Army)
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);
 if(!S||Army<0||Army>=FrontArmyCount(S->Team))return false;
 if(!AssignArmy(SquadID,1))return false; // into the front-holding group first (pins it there)
 FrontArmyOf.Add(SquadID,int8(Army));FrontArmyPinned.Add(SquadID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_ARMY_MOVE team=%d squad=%d to=%d by=player time=%.1f"),S->Team,SquadID,Army+1,Elapsed);
 return true;
}

void UBattleMatch::UpdateFrontArmies(int32 Team,const FBattleGroup& G,const TArray<int32>& SlotRow,int32 NB,const TMap<int32,FIntPoint>& LastPosts)
{
 using namespace BattleFrontArmyValues;
 const int32 Want=FrontArmiesWanted();
 if(Want<=0||Team<0||Team>1){FrontArmyFirst[Team].Reset();return;}
 constexpr int32 Rows=BattleSectors::Rows;
 // Bands: the front plan's sections when there are two or more, else equal bands (made once per match).
 const auto& Plan=FrontPlanState[Team];
 if(Plan.Sections>=2&&Plan.SectionFirst.Num()==Plan.Sections)FrontArmyFirst[Team]=Plan.SectionFirst;
 else if(FrontArmyFirst[Team].Num()!=Want){FrontArmyFirst[Team].Reset();for(int32 I=0;I<Want;++I)FrontArmyFirst[Team].Add(I*Rows/Want);}
 const int32 NA=FrontArmyFirst[Team].Num();
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const bool bLow=AreaHome(Team).X<0;
 auto RowOf=[&](const FIntPoint& C){return bLow?C.Y:Rows-1-C.Y;};
 // Squads that left the group leave their army; so do squads of an army that no longer exists (the bands may change from equal
 // bands to the front plan's sections once those are made; Codex review 2026-11-27).
 for(auto It=FrontArmyOf.CreateIterator();It;++It)if(const auto* S=Store->FindSquad(It.Key());!S||(S->Team==Team&&(!G.Squads.Contains(It.Key())||It.Value()<0||It.Value()>=NA))){FrontArmyPinned.Remove(It.Key());It.RemoveCurrent();}
 // Needs: posts per army.
 TArray<int32> Slots,Have;Slots.Init(0,NA);Have.Init(0,NA);
 for(int32 J=0;J<NB&&J<SlotRow.Num();++J)if(const int32 A=FrontArmyOfRow(Team,SlotRow[J]);A!=INDEX_NONE)++Slots[A];
 for(int32 SID:G.Squads)if(const int8* A=FrontArmyOf.Find(SID);A&&*A>=0&&*A<NA)++Have[*A];
 FrontArmyNeed[Team]=Slots;
 auto Middle=[&](int32 A){const int32 First=FrontArmyFirst[Team][A],Last=A+1<NA?FrontArmyFirst[Team][A+1]-1:Rows-1;return .5*(First+Last);};
 auto SquadRow=[&](int32 SID)->double
 {
  // The last post (UpdateFrontLine has already cleared this update's FrontPosts; Codex review 2026-11-27).
  if(const FIntPoint* Post=LastPosts.Find(SID))return RowOf(*Post);
  if(const auto* S=Store->FindSquad(SID);S&&R)return RowOf(BattleSectors::Cell(R->Position(S->LeaderUnitID)));
  return Rows*.5;
 };
 // New members: the army short of the most squads (ties: the nearest to the squad).
 for(int32 SID:G.Squads)if(!FrontArmyOf.Contains(SID))
 {
  int32 Best=0;double BestScore=-1e9;const double Row=SquadRow(SID);
  for(int32 A=0;A<NA;++A){const double ArmyScore=(Slots[A]-Have[A])*100.-FMath::Abs(Row-Middle(A));if(ArmyScore>BestScore){BestScore=ArmyScore;Best=A;}}
  // A squad bought for a section (stage 7 reservation destination) joins the army of that section's rows (Codex review 2026-11-27).
  if(const int32* Wish=FrontSectionWish.Find(SID);Wish&&Plan.SectionFirst.IsValidIndex(*Wish))if(const int32 W=FrontArmyOfRow(Team,Plan.SectionFirst[*Wish]);W!=INDEX_NONE)Best=W;
  FrontArmyOf.Add(SID,int8(Best));++Have[Best];
 }
 // One move every RebalanceHold seconds: from the most over-strength army to the most under-strength one.
 if(Elapsed-FrontArmyMoveAt[Team]>=RebalanceHold)
 {
  int32 Short=INDEX_NONE,Over=INDEX_NONE;
  for(int32 A=0;A<NA;++A)
  {
   if(Slots[A]-Have[A]>0&&(Short==INDEX_NONE||Slots[A]-Have[A]>Slots[Short]-Have[Short]))Short=A;
   // Only an army with a squad the player did not place by hand can give one (Codex review 2026-11-27).
   bool bMovable=false;for(int32 SID:G.Squads)if(const int8* Of=FrontArmyOf.Find(SID);Of&&*Of==A&&!FrontArmyPinned.Contains(SID))bMovable=true;
   if(bMovable&&Have[A]-Slots[A]>0&&(Over==INDEX_NONE||Have[A]-Slots[A]>Have[Over]-Slots[Over]))Over=A;
  }
  if(Short!=INDEX_NONE&&Over!=INDEX_NONE)
  {
   int32 Pick=INDEX_NONE;double PickD=1e9;
   for(int32 SID:G.Squads)if(const int8* A=FrontArmyOf.Find(SID);A&&*A==Over&&!FrontArmyPinned.Contains(SID))
   {const double D=FMath::Abs(SquadRow(SID)-Middle(Short));if(D<PickD||(D==PickD&&SID<Pick)){PickD=D;Pick=SID;}}
   if(Pick!=INDEX_NONE)
   {
    FrontArmyOf.Add(Pick,int8(Short));FrontArmyMoveAt[Team]=Elapsed;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_ARMY_MOVE team=%d squad=%d from=%d to=%d need=%d/%d have=%d/%d time=%.1f"),Team,Pick,Over+1,Short+1,Slots[Over],Slots[Short],Have[Over],Have[Short],Elapsed);
   }
  }
 }
 if(Elapsed>=FrontArmyLogAt[Team])
 {
  FrontArmyLogAt[Team]=Elapsed+LogEvery;
  FString Text;
  for(int32 A=0;A<NA;++A)
  {
   TArray<int32> Ids;for(int32 SID:G.Squads)if(const int8* Of=FrontArmyOf.Find(SID);Of&&*Of==A)Ids.Add(SID);
   const int32 First=FrontArmyFirst[Team][A],Last=A+1<NA?FrontArmyFirst[Team][A+1]-1:Rows-1;
   Text+=FString::Printf(TEXT(" %d:rows%d-%d need=%d have=%d ids=%s"),A+1,First+1,Last+1,Slots[A],Ids.Num(),*FString::JoinBy(Ids,TEXT(","),[](int32 I){return FString::FromInt(I);}));
  }
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FRONT_ARMIES time=%.0f team=%d armies=%d%s"),Elapsed,Team,NA,*Text);
 }
}

bool UBattleMatch::FrontArmyMismatch(int32 Team,int32 SquadID,int32 Row) const
{
 if(FrontArmyCount(Team)<2)return false;
 const int32 A=FrontArmyOfSquad(SquadID);return A!=INDEX_NONE&&A!=FrontArmyOfRow(Team,Row);
}
