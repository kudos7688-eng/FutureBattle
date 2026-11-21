// Intel trial (-BattleAreaIntel=1, 2026-11-17, user request "AI must act on what scouts find"). The team's knowledge of enemy
// positions is what its soldiers see now (scouts' long-range sightings excluded) plus its scout reports of the last
// ScoutReportMemory seconds. It drives (1) reinforcing threatened own points, (2) attacking where fewer enemies are known,
// (3) the home-assault share when the enemy home was observed lightly held, and (4) purchases to defend a threatened point.
// AI cognition only: positions of enemies the team has not seen or been told about are never used.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "FutureBattle.h"

bool UBattleMatch::AreaIntelOn(int32 Team) const
{
 static const bool bOn=[]{int32 V=GetDefault<UBattleTuning>()->bAreaIntel?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIntel="),V);return V!=0;}();
 const int32 Trial=BattleTactics::AreaStrategy().TrialTeam;
 return bArea&&bOn&&(Trial<0||Trial==Team);
}

const UBattleMatch::FAreaIntelView& UBattleMatch::AreaIntelView(int32 Team) const
{
 FAreaIntelView& V=IntelViews[FMath::Clamp(Team,0,1)];
 if(V.At>=0&&V.At<=Elapsed&&Elapsed-V.At<1.&&(!(BattleTactics::TankGunFlash()||BattleTactics::ArtilleryFlash())||Elapsed<V.FlashExpiresAt))return V; // refreshed once per match second or at flash expiry
 V=FAreaIntelView();V.At=Elapsed;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();const auto* T=GetDefault<UBattleTuning>();
 const int32 N=Objectives.Num();V.OwnAt.Init(0,N);V.EnemyAt.Init(0,N);
 const FVector EnemyHome=AreaHome(1-Team);
 TMap<int32,FVector> Seen;
 for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())
 {
  const FVector P=R->Position(U.Key);
  for(int32 I=0;I<N;++I)if(FVector::DistSquared2D(P,Objectives[I].Position)<=FMath::Square(10000.)){++V.OwnAt[I];break;}
  // The enemy home is covered when a soldier's own sight (a still scout's long sight counts only through its reports) reaches it.
  if(!V.bHomeCovered&&FVector::DistSquared2D(P,EnemyHome)<=FMath::Square(U.Value.Perception.SightRange))V.bHomeCovered=true;
  if(BattleTactics::ArtilleryObservedEnabled()&&BaseActors.IsValidIndex(1-Team)&&BaseActors[1-Team]&&
     (V.HomeObserver==INDEX_NONE||U.Key<V.HomeObserver)&&AreaVisible(U.Key,EnemyHome,BaseActors[1-Team]))
  {V.HomeObserver=U.Key;V.HomeObservedAt=Elapsed;V.HomeReportFrom=P;}
  if(const auto* Contacts=Perception?Perception->GetContacts(U.Key):nullptr)
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!IsScoutLongContact(U.Key,C.Value)){Seen.Add(C.Key,C.Value.LastKnownPosition);if(BattleTactics::GroupStrengthEnabled()&&C.Value.ObservedSquadType<BattleSquadType::Count)V.EnemyTypes.Add(C.Key,C.Value.ObservedSquadType);}
 }
 AddScoutReportSightings(Team,Seen);
 if(BattleTactics::TankGunFlash()||BattleTactics::ArtilleryFlash())
 {
  auto& Flashes=FireReveals[FMath::Clamp(Team,0,1)];
  for(auto FlashIt=Flashes.CreateIterator();FlashIt;++FlashIt)
   if(FlashIt.Value().At>Elapsed||Elapsed-FlashIt.Value().At>=FlashIt.Value().Memory)FlashIt.RemoveCurrent();
  TArray<int32> FlashKeys;Flashes.GetKeys(FlashKeys);FlashKeys.Sort();
  for(int32 FlashKey:FlashKeys)
  {
   const auto& Flash=Flashes[FlashKey];
   V.FlashExpiresAt=FMath::Min(V.FlashExpiresAt,Flash.At+Flash.Memory);
   if(!Seen.Contains(FlashKey))Seen.Add(FlashKey,Flash.Position);
  }
 }
 for(const auto& P:ScoutReports)
  if(const auto* S=Store->FindSquad(P.Key);S&&S->Team==Team&&P.Value.ReportedAt>=0&&Elapsed-P.Value.ReportedAt<=T->ScoutReportMemory&&
     FVector::DistSquared2D(P.Value.ReportFrom,EnemyHome)<=FMath::Square(BattleSquadType::StillSightRange(BattleSquadType::Scout)))V.bHomeCovered=true;
 const double Radius2=FMath::Square(FMath::Max(0.f,T->AreaIntelRadius));
 for(const auto& E:Seen)
 {
  V.Enemies.Add(E.Value);V.EnemyKeys.Add(E.Key);
  for(int32 I=0;I<N;++I)if(FVector::DistSquared2D(E.Value,Objectives[I].Position)<=Radius2)++V.EnemyAt[I];
  if(FVector::DistSquared2D(E.Value,EnemyHome)<=FMath::Square(15000.))++V.HomeEnemies;
 }
 NoteKnownVehicles(FMath::Clamp(Team,0,1));
 return V;
}

bool UBattleMatch::AreaIntelThreatAt(int32 Team,int32 Point) const
{
 const auto& V=AreaIntelView(Team);
 return V.EnemyAt.IsValidIndex(Point)&&ObservedOwner(Team,Point)==Team&&V.EnemyAt[Point]-V.OwnAt[Point]>=FMath::Max(1,GetDefault<UBattleTuning>()->AreaIntelThreatMin);
}

int32 UBattleMatch::AreaIntelThreat(int32 Team) const
{
 const auto& V=AreaIntelView(Team);int32 Best=-1,BestSurplus=0;
 for(int32 I=0;I<V.EnemyAt.Num();++I)if(AreaIntelThreatAt(Team,I)){const int32 Surplus=V.EnemyAt[I]-V.OwnAt[I];if(Surplus>BestSurplus){BestSurplus=Surplus;Best=I;}}
 return Best;
}

void UBattleMatch::UpdateIntelPurposes()
{
 // A squad bought to defend a point returns to the normal spread once it is there and the point is no longer threatened.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 for(auto It=IntelPurpose.CreateIterator();It;++It)
 {
  const auto* S=Store->FindSquad(It.Key());
  if(!S||S->CommandStatus!=EBattleCommandStatus::Active||!Objectives.IsValidIndex(It.Value())){It.RemoveCurrent();continue;}
  const bool There=FVector::DistSquared2D(R->Position(S->LeaderUnitID),Objectives[It.Value()].Position)<=FMath::Square(10000.);
  if(There&&!AreaIntelThreatAt(S->Team,It.Value()))
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_INTEL_RELEASE team=%d squad=%d point=%d time=%.1f"),S->Team,It.Key(),It.Value(),Elapsed);
   It.RemoveCurrent();
  }
 }
}

bool UBattleMatch::ArtilleryHomeObserved(int32 Team) const
{
 if(Team<0||Team>1||!BaseActors.IsValidIndex(1-Team)||!BaseActors[1-Team])return false;
 const auto& V=AreaIntelView(Team);
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(V.HomeObserver);
 // Revalidate at launch: a dead/moved observer or newly blocked LOS cannot authorize a retained aim.
 return V.HomeObservedAt>=0&&Elapsed>=V.HomeObservedAt&&Elapsed-V.HomeObservedAt<1.&&
  U&&U->IsAlive()&&U->Team==Team&&AreaVisible(V.HomeObserver,AreaHome(1-Team),BaseActors[1-Team]);
}
