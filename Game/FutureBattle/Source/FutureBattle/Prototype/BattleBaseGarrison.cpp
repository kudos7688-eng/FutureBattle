#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleTactics.h"
#include "BattleGameMode.h"
#include "Components/CapsuleComponent.h"
#include "FutureBattle.h"

bool UBattleMatch::IsBaseGarrison(int32 SquadID) const
{
 return BattleTactics::BaseGarrison()&&bArea&&Area.HirePurpose.FindRef(SquadID)==102;
}
int32 UBattleMatch::GarrisonLiving(int32 Team,bool bInsideOnly) const
{
 if(!BattleTactics::BaseGarrison()||!bArea)return 0;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 int32 Living=0;
 for(const auto& Entry:Store->GetUnits())if(Entry.Value.Team==Team&&Entry.Value.IsAlive()&&IsBaseGarrison(Entry.Value.SquadID))
  if(!bInsideOnly||FVector::Dist2D(Runtime->Position(Entry.Key),AreaHome(Team))<=BattleTactics::BaseGarrisonRadius())++Living;
 return Living;
}
void UBattleLeaderRuntime::InitializeBaseGarrison()
{
 if(!BattleTactics::BaseGarrison()||!bArea)return;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 Match->Area.bGarrisonReady=false;Match->Area.GarrisonHeld={0,0};
 const int32 SavedNextUnit=Store->NextID,SavedNextSquad=Store->NextSquadID;
 TArray<int32> Created;FString Error;bool Success=true;
 for(int32 Team=0;Team<2&&Success;++Team)for(int32 Slot=0;Slot<BattleTactics::BaseGarrisonSquads&&Success;++Slot)
 {
  int32 GuardID=INDEX_NONE;
  Success=SpawnPurchasedSquad(Team,EBattleTraining::Regular,Error,false,&GuardID,BattleSquadType::Rifle,0);
  if(!Success)break;
  Created.Add(GuardID);Match->Area.HirePurpose.Add(GuardID,102);
  const auto* Guard=Store->FindSquad(GuardID);
  for(int32 Member:Guard->Members)if(FVector::Dist2D(Position(Member),Match->AreaHome(Team))>BattleTactics::BaseGarrisonRadius())
  {Success=false;Error=TEXT("Deployment outside base radius");}
  // Keep the initial post inside the base; manual orders revoke delegation as usual.
  if(Success&&!IssueOrder(GuardID,EBattleOrder::Defend,Position(Guard->LeaderUnitID),true))
  {Success=false;Error=TEXT("Initial defend order refused");}
 }
 if(!Success)
 {
  // Remove all staged squads and bodies without deaths, score changes or one-sided survivors.
  for(int32 GuardID:Created)
  {
   const TArray<int32> Members=Store->FindSquad(GuardID)->Members;
   for(int32 Member:Members)
   {
    if(auto* Body=Bodies.Find(Member))
    {
     if(Body->Actor.IsValid()){Mode->Units.Remove(Body->Actor.Get());HitIDs.Remove(Body->Actor->GetCapsuleComponent());}
     else HitIDs.Remove(Body->Capsule.Get());
     DestroyBody(*Body);Bodies.Remove(Member);
    }
    Store->Units.Remove(Member);
   }
   Store->Squads.Remove(GuardID);Plans.Remove(GuardID);Match->Area.HirePurpose.Remove(GuardID);
  }
  Store->NextID=SavedNextUnit;Store->NextSquadID=SavedNextSquad;bSummaryDirty=true;
  UE_LOG(LogFutureBattle,Error,TEXT("BATTLE_BASE_GARRISON_FAIL reason=%s"),*Error);return;
 }
 Match->Area.bGarrisonReady=true;
 for(int32 Team=0;Team<2;++Team)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON team=%d squads=%d,%d"),Team,Created[Team*2],Created[Team*2+1]);
}
void UBattleMatch::UpdateBaseGarrison(float Dt)
{
 if(!BattleTactics::BaseGarrison()||!bArea||!Area.bGarrisonReady||Phase!=EBattleMatchPhase::Combat||!FMath::IsFinite(Dt)||Dt<=0)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const float Duration=BattleTactics::BaseGarrisonSeconds();
 float Before[2]={Area.GarrisonHeld[0],Area.GarrisonHeld[1]};bool Done[2]={false,false};
 for(int32 Team=0;Team<2;++Team)
 {
  // The other team's own garrison never counts as an occupier (it belongs to its base).
  bool Occupied=false;
  for(const auto& Entry:Store->GetUnits())if(Entry.Value.Team==1-Team&&Entry.Value.IsAlive())
   if(const auto* Squad=Store->FindSquad(Entry.Value.SquadID);Squad&&!BattleSquadType::IsVehicle(Squad->SquadType)&&!IsBaseGarrison(Squad->SquadID)&&
      FVector::Dist2D(Runtime->Position(Entry.Key),AreaHome(Team))<=BattleTactics::BaseGarrisonRadius()){Occupied=true;break;}
  float& Held=Area.GarrisonHeld[Team];
  if(!Occupied)
  {
   if(Held>0){Held=0;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON_CLOCK team=%d state=reset t=%.1f"),Team,Held);}
   continue;
  }
  if(GarrisonLiving(Team,true)>0)continue;
  if(Held==0)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON_CLOCK team=%d state=start t=%.1f"),Team,Held);
  Held=FMath::Min(Duration,Held+Dt);Done[Team]=Held>=Duration;
  if(Done[Team])UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON_CLOCK team=%d state=done t=%.1f"),Team,Held);
 }
 if(!Done[0]&&!Done[1])return;
 int32 Victor=Done[0]?1:0;
 if(Done[0]&&Done[1])
 {
  // Smaller remaining duration wins first; compare the pre-clamp clocks, never team iteration order.
  if(Before[0]!=Before[1])Victor=Before[0]>Before[1]?1:0;
  else
  {
   Score[0]=AreaScore(0);Score[1]=AreaScore(1);
   Victor=FMath::IsNearlyEqual(Score[0],Score[1],.01f)?INDEX_NONE:Score[0]>Score[1]?0:1;
  }
 }
 Finish(Victor,Victor==INDEX_NONE?TEXT("DRAW / BASE OCCUPATION SCORE"):Victor==0?TEXT("VICTORY / ENEMY BASE OCCUPIED"):TEXT("DEFEAT / HOME BASE OCCUPIED"));
}
