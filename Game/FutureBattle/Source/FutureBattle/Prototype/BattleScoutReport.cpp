// Scout reports (ROADMAP 2026-11-14, user decisions; stage 3 of the scout pair, 2026-11-17).
// What a still scout pair sees beyond normal sight is not team knowledge at once. A report runs while the pair holds such contacts:
// after ScoutAlertSeconds the team gets the direction only, after ScoutReportSeconds the positions and the size (a snapshot that
// team plans and the map use for ScoutReportMemory seconds). Losing a member or being pinned by suppression restarts the report.
// While a report runs, enemy soldiers within ScoutHintRadius learn the scout's approximate position (a remembered contact on a grid).
// The player's own direct view is not filtered: what the camera shows is seen at once.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleSuppression.h"
#include "FutureBattle.h"

bool UBattleMatch::IsScoutLongContact(int32 ObserverID,const FBattleContact& Contact) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ObserverID);
 const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
 if(!S||S->SquadType!=BattleSquadType::Scout) return false;
 const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 return FVector::DistSquared2D(R->Position(ObserverID),Contact.LastKnownPosition)>FMath::Square(U->Perception.SightRange);
}

void UBattleMatch::AddScoutReportSightings(int32 Team,TMap<int32,FVector>& Seen) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const float Memory=GetDefault<UBattleTuning>()->ScoutReportMemory;
 int32 Key=-1;
 for(const auto& P:ScoutReports)
 {
  const auto* S=Store->FindSquad(P.Key);if(!S||S->Team!=Team||P.Value.ReportedAt<0||Elapsed-P.Value.ReportedAt>Memory)continue;
  for(const FVector& Where:P.Value.Positions)Seen.Add(Key--,Where);
 }
}

void UBattleMatch::UpdateScoutReports()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();const auto* T=GetDefault<UBattleTuning>();
 if(!R||!Perception)return;
 const double Now=Elapsed,WorldNow=GetWorld()->GetTimeSeconds();
 for(auto It=ScoutReports.CreateIterator();It;++It)
 {const auto* S=Store->FindSquad(It.Key());const auto& V=It.Value();
  // A load to an earlier match time drops reports from the abandoned future.
  if(!S||S->CommandStatus!=EBattleCommandStatus::Active||V.Started>Now||V.AlertAt>Now||V.ReportedAt>Now)It.RemoveCurrent();}
 TArray<int32> Squads;Store->GetSquads().GetKeys(Squads);Squads.Sort();
 for(int32 SID:Squads)
 {
  const auto* S=Store->FindSquad(SID);
  if(S->SquadType!=BattleSquadType::Scout||S->CommandStatus!=EBattleCommandStatus::Active)continue;
  auto& Rep=ScoutReports.FindOrAdd(SID);
  auto Restart=[&Rep](){Rep.Started=-1;Rep.Stage=0;};
  int32 Living=0;bool Pinned=false;TMap<int32,FVector> Long;
  for(int32 ID:S->Members)
  {
   const auto* U=Store->Find(ID);if(!U||!U->IsAlive())continue;++Living;
   if(BattleSuppression::Tuning().bEnabled&&R->Suppression(ID,WorldNow)>=BattleSuppression::Tuning().PinLevel)Pinned=true;
   if(const auto* Contacts=Perception->GetContacts(ID))
    for(const auto& C:*Contacts)if(C.Value.bVisible&&IsScoutLongContact(ID,C.Value))Long.Add(C.Key,C.Value.LastKnownPosition);
  }
  // A lost member or a pinned observer restarts the report.
  if(Living<Rep.Living||Pinned)Restart();
  Rep.Living=Living;
  if(Pinned)continue;
  if(!Long.IsEmpty()){Rep.LastLong=Now;if(Rep.Started<0){Rep.Started=Now;Rep.Stage=0;}}
  else if(Rep.Started>=0&&Now-Rep.LastLong>5.)Restart();
  if(Rep.Started<0)continue;
  const FVector Eye=R->Position(S->LeaderUnitID);
  if(Rep.Stage==0&&Now-Rep.Started>=T->ScoutAlertSeconds&&!Long.IsEmpty())
  {
   FVector Sum=FVector::ZeroVector;for(const auto& L:Long)Sum+=(L.Value-Eye).GetSafeNormal2D();
   Rep.Direction=Sum.GetSafeNormal2D();Rep.AlertAt=Now;Rep.Stage=1;++Rep.AlertSequence;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SCOUT_ALERT team=%d squad=%d contacts=%d time=%.1f"),S->Team,SID,Long.Num(),Now);
  }
  if(Rep.Stage==1&&Now-Rep.Started>=T->ScoutReportSeconds&&!Long.IsEmpty())
  {
   Rep.Positions.Reset();for(const auto& L:Long)Rep.Positions.Add(L.Value);
   Rep.ReportedAt=Now;++Rep.Sequence;Rep.Started=Now;Rep.Stage=0; // keep watching: the next report runs from now
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SCOUT_REPORT team=%d squad=%d enemies=%d time=%.1f"),S->Team,SID,Rep.Positions.Num(),Now);
  }
  // Enemies near a reporting scout learn its approximate position (grid cell centre, refreshed every 2 s).
  const float Grid=FMath::Max(100.f,T->ScoutHintGrid);
  Rep.Approx=FVector((FMath::FloorToFloat(Eye.X/Grid)+.5f)*Grid,(FMath::FloorToFloat(Eye.Y/Grid)+.5f)*Grid,Eye.Z);
  if(WorldNow>=Rep.NextHint)
  {
   Rep.NextHint=WorldNow+2.;
   for(const auto& U:Store->GetUnits())if(U.Value.Team!=S->Team&&U.Value.IsAlive()&&FVector::DistSquared2D(R->Position(U.Key),Eye)<=FMath::Square(T->ScoutHintRadius))
    Perception->AddHint(U.Key,S->LeaderUnitID,Rep.Approx,WorldNow,.5f);
  }
 }
}
