// Supply vehicle (squad type 9; 2026-11-20, user decision: the 3-soldier supply team is replaced by one vehicle, so it is limited to
// where vehicles go and becomes a target). It carries UBattleTuning::SupplyCarStock rounds. Own squads whose leader stays within
// SupplyTeamRadius of it get the home ammunition refill (UBattleMatch::UpdateArea) and the rounds given leave its stock; at home it
// loads again. An AI-run vehicle drives home while low, keeps behind its group's post (group role 5) or behind the own point nearest
// the enemy home. A squad short of ammunition drives to a vehicle with stock within FieldSupplyRange instead of going home
// (BattleSquadRetreat.cpp, "FIELD SUPPLY"). Inputs: own squads' state and positions, observed ownership, known enemies (AreaArmourThreat).
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "FutureBattle.h"

namespace
{
 // P moved Back toward the own home, then further back in 20 m steps (up to 6) while a known anti-tank soldier reaches it.
 FVector SupplyBack(const UBattleMatch& M,int32 Team,const FVector& P,double Back)
 {
  const FVector Home=M.AreaHome(Team);const FVector Dir=(Home-P).GetSafeNormal2D();const double Length=FVector::Dist2D(Home,P);
  double Step=FMath::Clamp(Back,0.,FMath::Max(0.,Length-3000.));
  for(int32 I=0;I<6&&Step+2000.<Length&&M.AreaArmourThreat(Team,P+Dir*Step,false);++I)Step+=2000.;
  return P+Dir*Step;
 }
}

int32 UBattleMatch::SupplyStockMax() const { return FMath::Clamp(GetDefault<UBattleTuning>()->SupplyCarStock,0,100000); }
int32 UBattleMatch::SupplyStockOf(int32 SquadID) const { const int32* S=Area.SupplyStock.Find(SquadID); return S?*S:SupplyStockMax(); }
void UBattleMatch::SpendSupply(int32 SquadID,int32 Rounds) { Area.SupplyStock.Add(SquadID,FMath::Max(0,SupplyStockOf(SquadID)-FMath::Max(0,Rounds))); }
void UBattleMatch::RestockSupplyCar(int32 SquadID)
{
 if(!Area.SupplyStock.Contains(SquadID))return;
 UE_LOG(LogFutureBattle,Display,TEXT("AREA_SUPPLY_RESTOCK time=%.0f squad=%d stock=%d"),Elapsed,SquadID,Area.SupplyStock[SquadID]);
 Area.SupplyStock.Remove(SquadID); // no entry = full
}

int32 UBattleMatch::SupplyCarNear(int32 Team,const FVector& Position,float Radius) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return INDEX_NONE;
 const double Reach=Radius>0.f?Radius:FMath::Clamp(GetDefault<UBattleTuning>()->SupplyTeamRadius,100.f,20000.f);
 int32 Best=INDEX_NONE;double BestD=Reach;
 for(const auto& Pair:Store->GetSquads())
 {
  const auto& S=Pair.Value;if(S.Team!=Team||S.SquadType!=BattleSquadType::Supply||S.CommandStatus!=EBattleCommandStatus::Active||SupplyStockOf(Pair.Key)<=0)continue;
  const auto* Leader=Store->Find(S.LeaderUnitID);if(!Leader||!Leader->IsAlive())continue;
  const double D=FVector::Dist2D(R->Position(S.LeaderUnitID),Position);
  if(D<=BestD){BestD=D;Best=Pair.Key;}
 }
 return Best;
}

bool UBattleMatch::SupplyCarOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||S->SquadType!=BattleSquadType::Supply||S->Team!=Team)return false;
 const auto* T=GetDefault<UBattleTuning>();
 const FVector Home=AreaHome(Team),Here=R->Position(S->LeaderUnitID);
 const int32 Stock=SupplyStockOf(SquadID),Max=SupplyStockMax();
 // Loading: low on stock, or at home and not yet full. The waiting place is beside the home, toward the field.
 if(Max>0&&(Stock<FMath::CeilToInt(Max*FMath::Clamp(T->SupplyCarRestockRatio,0.f,1.f))||(Stock<Max&&FVector::Dist2D(Here,Home)<=2500.)))
 {Center=Home+FVector((Home.X>0?-1:1)*700,(SquadID%5-2)*600,0);Order=EBattleOrder::Move;return true;}
 const double Back=FMath::Max(0.f,T->SupplyCarBack);
 // With its group (when the AI runs it): the rally point while gathering, behind the support post while advancing, behind the point while holding.
 if(const int32 GID=GroupOf(SquadID);GID!=INDEX_NONE)
  if(const auto* G=Groups.Find(GID);G&&GroupAIRun(*G)&&Objectives.IsValidIndex(G->Objective))
  {
   const FVector Post=G->Phase==0?G->Rally:G->Phase==1?GroupSupportPost(Team,G->Objective):Objectives[G->Objective].Position;
   Center=G->Phase==0?Post:SupplyBack(*this,Team,Post,Back);Order=EBattleOrder::Defend;return true;
  }
 // Otherwise behind the own point (observed) nearest the enemy home; none: at home.
 int32 Best=INDEX_NONE;double BestD=DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)==Team)
 {const double D=FVector::Dist2D(Objectives[I].Position,AreaHome(1-Team));if(D<BestD){BestD=D;Best=I;}}
 Center=Best!=INDEX_NONE?SupplyBack(*this,Team,Objectives[Best].Position,Back):Home+FVector((Home.X>0?-1:1)*700,(SquadID%5-2)*600,0);
 Order=EBattleOrder::Defend;return true;
}

bool UBattleMatch::FieldSupplyPoint(int32 SquadID,FVector& Where,int32* CarID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||!bArea||S->SquadType==BattleSquadType::Supply||BattleSquadType::IsVehicle(S->SquadType))return false;
 const FVector Here=R->Position(S->LeaderUnitID);const float Range=FMath::Max(0.f,GetDefault<UBattleTuning>()->FieldSupplyRange);
 if(Range<=0.f)return false;
 // The squad's own group's vehicle first, then the nearest one in range.
 int32 Car=INDEX_NONE;
 if(const int32 GID=GroupOf(SquadID);GID!=INDEX_NONE)if(const auto* G=Groups.Find(GID))
  for(int32 Other:G->Squads)if(const auto* O=Store->FindSquad(Other);O&&O->SquadType==BattleSquadType::Supply&&O->CommandStatus==EBattleCommandStatus::Active&&SupplyStockOf(Other)>0)
   if(FVector::Dist2D(R->Position(O->LeaderUnitID),Here)<=Range){Car=Other;break;}
 if(Car==INDEX_NONE)Car=SupplyCarNear(S->Team,Here,Range);
 // A vehicle low enough to be going home to load is not sent for (it still supplies squads beside it).
 if(Car==INDEX_NONE||SupplyStockOf(Car)<FMath::CeilToInt(SupplyStockMax()*FMath::Clamp(GetDefault<UBattleTuning>()->SupplyCarRestockRatio,0.f,1.f)))return false;
 Where=R->Position(Store->FindSquad(Car)->LeaderUnitID);if(CarID)*CarID=Car;
 return true;
}
