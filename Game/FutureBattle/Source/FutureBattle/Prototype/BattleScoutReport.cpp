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
#include "BattleRanges.h"
#include "BattleTactics.h"
#include "Engine/StaticMeshActor.h"
#include "NavigationSystem.h"
#include "FutureBattle.h"

namespace
{
 // Trial switch -BattleScoutRetreat=0/1 (default: UBattleTuning::bScoutRetreat).
 bool ScoutRetreatEnabled()
 {
  static const bool bOn=[]{int32 V=GetDefault<UBattleTuning>()->bScoutRetreat?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleScoutRetreat="),V);return V!=0;}();
  return bOn;
 }
}

bool UBattleMatch::IsScoutLongContact(int32 ObserverID,const FBattleContact& Contact) const
{
 if(BattleTactics::ObservedTactics())return Contact.bLongRangeObservation;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ObserverID);
 const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 // Long sight: a scout pair member or a soldier at a lookout (BattleLeaderRuntime::HasLongSight).
 if(!U||!R->HasLongSight(ObserverID)) return false;
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
 // Evaluation log (not AI input): tank positions and speeds every 10 s of match time.
 if(R->VehicleIDs.Num()&&FMath::FloorToInt(Now/10.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/10.))
  for(int32 VID:R->VehicleIDs)if(const auto* A=R->Actor(VID))UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TANK_SAMPLE t=%.0f unit=%d team=%d pos=%s speed=%.0f"),Now,VID,Store->Find(VID)->Team,*A->GetActorLocation().ToCompactString(),A->GetVelocity().Size2D());
 if(R->SightPairTraced+R->SightPairReused>0&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_PAIR t=%.0f traced=%lld reused=%lld"),Now,R->SightPairTraced,R->SightPairReused);
 if(R->MoveSweeps>0&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MOVE_SWEEP t=%.0f moves=%lld hit_soldier=%lld hit_world=%lld engine_shortened=%lld"),Now,R->MoveSweeps,R->MoveHitSoldier,R->MoveHitWorld,R->MoveShortened);
 if(Perception->SlowScans+Perception->NormalScans>0&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_SCAN t=%.0f slow=%lld normal=%lld"),Now,Perception->SlowScans,Perception->NormalScans);
 if(Perception->ParallelUpdates>0&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_PARALLEL t=%.0f updates=%lld traces=%lld"),Now,Perception->ParallelUpdates,Perception->ParallelTraces);
 if(Perception->RayWindow>=0&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_RAYS t=%.0f own_seen=%lld own_blocked=%lld squad_seen=%lld squad_blocked=%lld shared_seen=%lld shared_blocked=%lld unknown_seen=%lld unknown_blocked=%lld known_pairs=%lld"),Now,
   Perception->RayOwn[1],Perception->RayOwn[0],Perception->RaySquad[1],Perception->RaySquad[0],Perception->RayShared[1],Perception->RayShared[0],Perception->RayUnknown[1],Perception->RayUnknown[0],Perception->RayKnownPairs);
 if(R->VehicleIDs.Num()&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TANK_COST t=%.0f sight_traces=%lld sight_reused=%lld sight_ms=%.1f orders=%d order_ms=%.1f order_skips=%lld gun_shots=%lld gun_hits=%lld gun_vehicle_hits=%lld gun_kills=%lld gun_base_hits=%lld gun_picks=%lld gun_ms=%.1f"),Now,R->TankSightTraces,R->TankSightReused,R->TankSightMs,R->TankOrders,R->TankOrderMs,R->TankOrderSkips,
   R->TankGunShots,R->TankGunHits,R->TankGunVehicleHits,R->TankGunKills,R->TankGunBaseHits,R->TankGunPicks,R->TankGunMs);
 // Indirect fire (mortar teams, self-propelled guns), evaluation only: logged once any artillery unit has picked or fired.
 if(R->ArtilleryPicks+R->ArtilleryShotsMortar+R->ArtilleryShotsSPG>0&&FMath::FloorToInt(Now/60.)!=FMath::FloorToInt((Now-GetWorld()->GetDeltaSeconds())/60.))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_COST t=%.0f mortar_shots=%lld spg_shots=%lld impacts=%lld hits=%lld kills=%lld base_hits=%lld friendly_holds=%lld picks=%lld pick_ms=%.1f in_flight=%d"),Now,
   R->ArtilleryShotsMortar,R->ArtilleryShotsSPG,R->ArtilleryImpacts,R->ArtilleryHits,R->ArtilleryKills,R->ArtilleryBaseHits,R->ArtilleryFriendlyHolds,R->ArtilleryPicks,R->ArtilleryMs,R->ArtilleryShells.Num());
 for(auto It=ScoutReports.CreateIterator();It;++It)
 {const auto* S=Store->FindSquad(It.Key());const auto& V=It.Value();
  // A load to an earlier match time drops reports from the abandoned future.
  if(!S||S->CommandStatus!=EBattleCommandStatus::Active||V.Started>Now||V.AlertAt>Now||V.ReportedAt>Now||V.RetreatUntil>Now+600.)It.RemoveCurrent();}
 TArray<int32> Squads;Store->GetSquads().GetKeys(Squads);Squads.Sort();
 for(int32 SID:Squads)
 {
  const auto* S=Store->FindSquad(SID);
  // Observers: scout pairs, and any squad with a member at a lookout (or a report of its own still running).
  if(S->CommandStatus!=EBattleCommandStatus::Active)continue;
  const bool bScoutPair=S->SquadType==BattleSquadType::Scout;
  if(!bScoutPair&&!ScoutReports.Contains(SID)){bool AtLookout=false;for(int32 ID:S->Members){const auto* U=Store->Find(ID);if(U&&U->IsAlive()&&R->AtLookout(ID)){AtLookout=true;break;}}if(!AtLookout)continue;}
  auto& Rep=ScoutReports.FindOrAdd(SID);
  auto Restart=[&Rep](){Rep.Started=-1;Rep.Stage=0;};
  int32 Living=0;bool Pinned=false;float HP=0;TMap<int32,FVector> Long;FVector Nearest=FVector::ZeroVector;double NearestD=DBL_MAX;
  const FVector Eye=R->Position(S->LeaderUnitID);
  for(int32 ID:S->Members)
  {
   const auto* U=Store->Find(ID);if(!U||!U->IsAlive())continue;++Living;HP+=U->HP;
   if(BattleSuppression::Tuning().bEnabled&&R->Suppression(ID,WorldNow)>=BattleSuppression::Tuning().PinLevel)Pinned=true;
   if(const auto* Contacts=Perception->GetContacts(ID))
    for(const auto& C:*Contacts)if(C.Value.bVisible)
    {
     if(IsScoutLongContact(ID,C.Value))Long.Add(C.Key,C.Value.LastKnownPosition);
     const double D=FVector::DistSquared2D(C.Value.LastKnownPosition,Eye);if(D<NearestD){NearestD=D;Nearest=C.Value.LastKnownPosition;}
    }
  }
  // Hit: health lost or a member lost (own squad state only). Logged with the report's progress for the timing analysis.
  const bool Hit=Rep.LastHP>=0.f&&HP<Rep.LastHP-.5f;Rep.LastHP=HP;
  // Warning: an enemy seen within ScoutWarnScale x the normal sight of a soldier (before he can see the pair: 130 m on maps A-D).
  const float Warn=GetDefault<UBattleTuning>()->EffectiveSightRange()*FMath::Max(0.f,T->ScoutWarnScale);
  const bool Near=NearestD<=FMath::Square(Warn);
  if(Hit||Near)Rep.LastDanger=Now;
  if(Hit&&bScoutPair)
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SCOUT_HIT team=%d squad=%d report_age=%.1f stage=%d living=%d time=%.1f"),S->Team,SID,Rep.Started>=0?Now-Rep.Started:-1.,Rep.Stage,Living,Now);
  }
  // Retreat (AI-led pairs, on a warning or a hit): ScoutRetreatDistance away from the nearest visible enemy (towards home when none is seen),
  // never past the home; the pair then watches from there (BattleMatch.cpp) and the report pauses while it moves.
  if(bScoutPair&&(Hit||Near)&&ScoutRetreatEnabled()&&Now>=Rep.RetreatUntil&&IsAIScoutLed(SID))
  {
   const FVector ToHome=AreaHome(S->Team)-Eye;
   const FVector Away=NearestD<DBL_MAX?(Eye-Nearest).GetSafeNormal2D():ToHome.GetSafeNormal2D();
   FVector Back=Eye+(Away.IsNearlyZero()?ToHome.GetSafeNormal2D():Away)*FMath::Min<double>(FMath::Max(0.f,T->ScoutRetreatDistance),ToHome.Size2D());
   if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){FNavLocation L;if(Nav->ProjectPointToNavigation(Back,L,FVector(1500,1500,500)))Back=L.Location;}
   Rep.RetreatPoint=Back;Rep.RetreatUntil=Now+FMath::Max(1.f,T->ScoutRetreatSeconds);++Rep.Burns;
   if(Rep.Started>=0&&Rep.PauseStart<0)Rep.PauseStart=Now;
   R->IssueOrder(SID,EBattleOrder::Move,Back,true);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SCOUT_RETREAT team=%d squad=%d living=%d hit=%d nearest_m=%.0f time=%.1f"),S->Team,SID,Living,Hit?1:0,NearestD<DBL_MAX?FMath::Sqrt(NearestD)/100.:-1.,Now);
  }
  // A lost member or a pinned observer restarts the report (ROADMAP rule).
  if(Living<Rep.Living||Pinned){Restart();Rep.PauseStart=-1;}
  Rep.Living=Living;
  if(Pinned)continue;
  // While retreating the report is paused, not restarted; it resumes from where it stood once the move ends.
  if(Now<Rep.RetreatUntil)continue;
  if(Rep.PauseStart>=0){const double Paused=Now-Rep.PauseStart;if(Rep.Started>=0){Rep.Started+=Paused;Rep.LastLong=Now;}Rep.PauseStart=-1;}
  if(!Long.IsEmpty()){Rep.LastLong=Now;if(Rep.Started<0){Rep.Started=Now;Rep.Stage=0;}}
  else if(Rep.Started>=0&&Now-Rep.LastLong>5.)Restart();
  if(Rep.Started<0)continue;
  if(Rep.Stage==0&&Now-Rep.Started>=T->ScoutAlertSeconds&&!Long.IsEmpty())
  {
   FVector Sum=FVector::ZeroVector;for(const auto& L:Long)Sum+=(L.Value-Eye).GetSafeNormal2D();
   Rep.Direction=Sum.GetSafeNormal2D();Rep.AlertAt=Now;Rep.Stage=1;++Rep.AlertSequence;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SCOUT_ALERT team=%d squad=%d contacts=%d time=%.1f"),S->Team,SID,Long.Num(),Now);
  }
  if(Rep.Stage==1&&Now-Rep.Started>=T->ScoutReportSeconds&&!Long.IsEmpty())
  {
   Rep.Positions.Reset();for(const auto& L:Long)Rep.Positions.Add(L.Value);
   if(BattleTactics::ObservedTactics())
   {
    Rep.bHomeObserved=false;
    if(BaseActors.IsValidIndex(1-S->Team)&&BaseActors[1-S->Team])
     for(int32 HomeScoutID:S->Members)if(const auto* HomeScout=Store->Find(HomeScoutID);HomeScout&&HomeScout->IsAlive()&&R->HasLongSight(HomeScoutID)&&
      AreaVisible(HomeScoutID,BaseActors[1-S->Team]->GetActorLocation(),BaseActors[1-S->Team])){Rep.bHomeObserved=true;break;}
   }
   Rep.ReportedAt=Now;Rep.ReportFrom=Eye;++Rep.Sequence;Rep.Started=Now;Rep.Stage=0; // keep watching: the next report runs from now
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

bool UBattleMatch::IsAIScoutLed(int32 SquadID) const
{
 // The enemy is always AI-led; own squads only when delegated to the AI (the player's manual squads never retreat by themselves).
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);
 return S&&(S->Team==1||S->bPlayerDelegated);
}
