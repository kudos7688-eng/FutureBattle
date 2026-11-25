#include "BattleSquadTactics.h"
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleTactics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "FutureBattle.h"

bool UBattleSquadTactics::Apply(int32 SID, EBattleSquadTactic Action, EBattleOrder Order, FVector Goal, const FString& Reason)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID);
 if(!S) return false;
 const bool LeavingFlank=S->TacticalPlan.Action==EBattleSquadTactic::Flank && Action!=EBattleSquadTactic::Flank;
 TMap<int32,FVector> Goals;
 for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) Goals.Add(ID,Goal);
 if(!Store->SetSquadTacticalPlan(SID,Action,Order,Goal,Goals,60,Reason)) return false;
 if(LeavingFlank && FParse::Param(FCommandLine::Get(),TEXT("BattleFlankTrial")))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_LEAVE t=%.3f squad=%d reason=%s"),GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,SID,*Reason);
 Store->Squads.FindChecked(SID).TacticalPlan.bAutonomous=true;
 Event(FName(BattleSquadTacticName(Action)));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ST2_TRANSITION squad=%d action=%s reason=%s goal=%s"),SID,BattleSquadTacticName(Action),*Reason,*Goal.ToCompactString());
 return true;
}

// B06 (Docs/review-2026-11-25): the one way back from a temporary plan (supply, regroup, raid, shield, contact) to the squad's mission.
// The mission is read now, not copied when the plan began, so a mission update during the plan (deferred by SetSquadOrder) is the
// one resumed; an explicit order during the plan cancels the plan itself and this is never reached. The sector flag of the mission is
// kept (it was dropped before: a sector mission came back as a point order). Once resumed the plan is Mission, so it runs once.
// Returns whether the mission route was accepted (the plan is then over).
bool UBattleSquadTactics::ResumeMission(int32 SID,const TCHAR* Reason,const TCHAR* EventName)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 FBattleMission M;
 if(!Store||!Runtime||!Store->MissionOf(SID,M))return false;
 const auto* S=Store->FindSquad(SID);
 if(!S||S->TacticalPlan.Action==EBattleSquadTactic::Mission)return false;
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const double Now=Match?Match->Elapsed:0.;
 const int32 OrderRev=S->OrderRevision;
 // Validate the route first (inside a plan this only updates the mission fields), then release the plan and execute.
 if(!Runtime->IssueOrder(SID,M.Order,M.Goal,true,M.bSector))
 {
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MISSION_RESUME squad=%d t=%.1f reason=%s order=%d sector=%d rev=%d ok=0"),SID,Now,Reason,int32(M.Order),M.bSector?1:0,M.Revision);
  return false;
 }
 Store->CancelSquadTacticalPlan(SID,Reason);
 const bool bRan=Runtime->IssueOrder(SID,M.Order,M.Goal,true,M.bSector);
 if(EventName)Event(EventName);
 const auto* After=Store->FindSquad(SID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MISSION_RESUME squad=%d t=%.1f reason=%s order=%d sector=%d rev=%d ok=%d order_steps=%d"),SID,Now,Reason,int32(M.Order),
  After&&After->bSectorMission?1:0,After?After->MissionRevision:M.Revision,bRan?1:2,After?After->OrderRevision-OrderRev:0);
 return true;
}
bool UBattleSquadTactics::RetreatGoal(const FBattleSquadState& S,const FBattleSquadObservation& Sample,FVector& Goal)
{
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if(!Nav) return false;
 const FVector Start=Runtime->Position(S.LeaderUnitID);
 FVector Away=FVector::ZeroVector;
 auto Distance=[&](FVector P){ double D=FMath::Max(10000.,double(GetDefault<UBattleTuning>()->EffectiveWeaponRange())*2); for(const auto& C:Sample.Contacts) D=FMath::Min(D,FVector::Dist2D(P,C.Value.LastKnownPosition)); return D; };
 for(const auto& C:Sample.Contacts)
 {
  const FVector D=Start-C.Value.LastKnownPosition;
  if(D.Size2D()<GetDefault<UBattleTuning>()->EffectiveWeaponRange()*1.2) Away+=D.GetSafeNormal2D()/FMath::Max(100.,D.Size2D());
 }
 Away=Away.GetSafeNormal2D(); if(Away.IsNearlyZero()) Away=FVector(S.Team?1:-1,0,0);
 const double Initial=Distance(Start); double Best=-DBL_MAX;
 // Four bounded candidates per squad/retry, never one query per follower.
 for(int32 I=0;I<4;++I)
 {
  const float Angle=I==2?45.f:I==3?-45.f:0.f;
  const FVector Raw=Start+Away.RotateAngleAxis(Angle,FVector::UpVector)*(I==0?1500.f:2500.f);
  FNavLocation P;
  if(!Nav->ProjectPointToNavigation(Raw,P,FVector(150,150,400))) continue;
  const double EndDistance=Distance(P.Location);
  if(!Sample.Contacts.IsEmpty() && EndDistance<Initial+400) continue;
  Event(TEXT("RetreatPathRequest"));
  auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Start,P.Location);
  if(!Path || !Path->IsValid() || Path->IsPartial()) continue;
  bool Dangerous=false; double Length=0;
  for(int32 J=1;J<Path->PathPoints.Num();++J)
  {
   const FVector A=Path->PathPoints[J-1], B=Path->PathPoints[J]; Length+=FVector::Dist2D(A,B);
   for(const auto& C:Sample.Contacts)
   {
    const FVector Threat(C.Value.LastKnownPosition.X,C.Value.LastKnownPosition.Y,0);
    const FVector Closest=FMath::ClosestPointOnSegment(Threat,FVector(A.X,A.Y,0),FVector(B.X,B.Y,0));
    if(FVector::Dist2D(Threat,Closest)<Initial-200) Dangerous=true;
   }
  }
  if(Dangerous || Length>6000) continue;
  // One static-geometry cover trace per candidate, toward the closest known contact.
  const FBattleContact* Nearest=nullptr; double NearDistance=DBL_MAX;
  for(const auto& C:Sample.Contacts)
  {
   const double D=FVector::DistSquared2D(P.Location,C.Value.LastKnownPosition);
   if(D<NearDistance || (D==NearDistance && Nearest && C.Key<Nearest->TargetID)) { NearDistance=D; Nearest=&C.Value; }
  }
  const double Cover=Nearest && GetWorld()->LineTraceTestByObjectType(P.Location+FVector(0,0,60),Nearest->LastKnownPosition+FVector(0,0,60),FCollisionObjectQueryParams(ECC_WorldStatic))?500.:0.;
  const double Score=EndDistance-Length*.2+Cover;
  if(Score>Best) { Best=Score; Goal=P.Location; }
 }
 return Best>-DBL_MAX;
}

void UBattleSquadTactics::Evaluate(int32 SID,const FBattleSquadObservation& O,float Dt)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* S=Store->Squads.Find(SID);
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Mode=GetWorld()->GetAuthGameMode<ABattleGameMode>();
 if(!IsST2Enabled() || !Runtime->IsWideField() || !Mode || Mode->bCombatPaused || Mode->bBattleEnded || !S ||
    S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated) || O.Living==0) return;
 auto& P=S->TacticalPlan;
 // A direct-controlled leader suspends tactical transitions; followers retain the existing plan.
 if(O.Direct>0 || (P.Action!=EBattleSquadTactic::Mission && !P.bAutonomous)) return;
 const auto* T=GetDefault<UBattleTuning>(); const FVector Here=Runtime->Position(S->LeaderUnitID);
 int32 Threats=0; bool Visible=false;
 for(const auto& C:O.Contacts) if(FVector::Dist2D(Here,C.Value.LastKnownPosition)<=T->EffectiveWeaponRange()) { ++Threats; Visible|=C.Value.bVisible && !C.Value.bShared; }
 const float FullHP=FMath::Max(1.f,O.FullHealth(T->Health));
 const bool Depleted=O.Armed==0 || O.Living<=S->Members.Num()*T->SquadRetreatLivingRatio || O.Health/FullHP<T->SquadRetreatHealthRatio;
 // Trial -BattleArtilleryRaidSneak: a raid on its way does not fall back only because it sees many enemies (it is meant to pass them;
 // measured raids looped retreat/regroup every 3 s at the front); ammo, losses, health and heavy recent damage still make it fall back.
 const auto* RaidSneakMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 const bool bRaidSneak=RaidSneakMatch && RaidSneakMatch->RaidSneaking(SID);
 const bool Emergency=Threats>0 && (Depleted || O.RecentDamage>=T->Health*S->Members.Num()*T->SquadRetreatDamageRatio || (!bRaidSneak && Threats>FMath::Max(3.f,O.Armed*BattleSquadType::ScoreCount(S->SquadType)*T->SquadRetreatThreatRatio)));
 const FString Cause=O.Armed==0?TEXT("AMMO EXHAUSTED"):
  O.Living<=S->Members.Num()*T->SquadRetreatLivingRatio?FString::Printf(TEXT("MEMBERS %d/%d"),O.Living,S->Members.Num()):
  O.Health/FullHP<T->SquadRetreatHealthRatio?FString::Printf(TEXT("HEALTH %.0f%%"),O.Health/FullHP*100):
  O.RecentDamage>=T->Health*S->Members.Num()*T->SquadRetreatDamageRatio?FString::Printf(TEXT("DAMAGE 5s %.0f"),O.RecentDamage):
  FString::Printf(TEXT("CONTACTS %d / ARMED %d"),Threats,O.Armed);
 P.Age+=Dt; P.RetrySeconds=FMath::Max(0.f,P.RetrySeconds-Dt);
 P.SafeSeconds=Threats==0 && O.RecentDamage==0?P.SafeSeconds+Dt:0.f;
 FString CauseOverride;
 auto Retreat=[&]()
 {
  FVector Goal;
  if(RetreatGoal(*S,O,Goal)) Apply(SID,EBattleSquadTactic::Retreat,EBattleOrder::Move,Goal,CauseOverride.IsEmpty()?Cause:CauseOverride);
  else { Apply(SID,EBattleSquadTactic::Regroup,EBattleOrder::Hold,Here,TEXT("NO SAFER REACHABLE ROUTE / HOLD")); P.RetrySeconds=T->SquadRetreatRetrySeconds; }
 };
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const auto& Supply=BattleTactics::Supply();
 // S8 trial: S1 home-assault squads return for supply only at BaseReturnLiving living soldiers (own squad state and fixed slot only).
 int32 ReturnLiving=Supply.ReturnLiving;
 const int32 BaseReturnLiving=Match->bArea&&BattleTactics::AreaStrategy().bEnabled?Match->AreaIntent(S->Team).BaseReturnLiving:0;
 if(BaseReturnLiving>0)
 {bool bB=false;Match->AreaStrategyTarget(SID,S->Team,bB);if(bB)ReturnLiving=BaseReturnLiving;}
 // Thresholds are written for 10-soldier squads; smaller squads (heavy infantry, 6) scale them to their size (10 -> unchanged).
 if(S->Members.Num()<10) ReturnLiving=FMath::FloorToInt(ReturnLiving*S->Members.Num()/10.f);
 // Scout pairs are never replenished: they go home for ammunition only, and one survivor is enough to resume.
 const bool bNoReplenish=!BattleSquadType::Replenished(S->SquadType);if(bNoReplenish)ReturnLiving=0;
 // Vehicles (stage 2, 2026-11-18): below VehicleRepairRatio of its health a vehicle drives home to be repaired (home healing) even under
 // fire, since it outruns infantry; an emergency does not turn that trip into a retreat. Own state only.
 const bool bVehicle=Runtime->IsVehicle(S->LeaderUnitID);
 // The exoskeleton soldier does the same (its shield refills in the field, its health only at home).
 const bool bExo=S->SquadType==BattleSquadType::Exo;const bool bMachine=bVehicle||bExo;
 const auto* ExoUnit=bExo?Store->Find(S->LeaderUnitID):nullptr;const float ExoShieldMax=bExo?BattleSquadType::ShieldMax(S->SquadType):0.f;
 const float ShieldRatio=ExoUnit&&ExoShieldMax>0.f?ExoUnit->Shield/ExoShieldMax:1.f;
 const auto* SPGUnit=BattleTactics::SPGAmmoEnabled()&&S->SquadType==BattleSquadType::SPGun?Store->Find(S->LeaderUnitID):nullptr;
 const bool bSPGEmpty=SPGUnit&&SPGUnit->SPGAmmo==0;
 // Trial -BattleSupplyAmmoShare (BattleTactics.h): the squad's own reserve rounds against its living members' full reserve.
 int32 OwnRounds=0,OwnFull=0;
 if(BattleTactics::SupplyAmmoShare()>0.f&&!bMachine)for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())
 {OwnRounds+=U->Reserve;OwnFull+=T->ReserveFor(U->Weapon);}
 const bool bLowAmmo=OwnFull>0&&OwnRounds<=OwnFull*BattleTactics::SupplyAmmoShare();
 const bool bAmmoBack=OwnFull==0||OwnRounds>=OwnFull*.9f;
 bool bRepair=Match->bArea&&bMachine&&Supply.HealPerSecond>0.f&&O.Health/FullHP<GetDefault<UBattleTuning>()->VehicleRepairRatio;
 // Field supply (BattleSupplyCar.cpp): a squad on its way to a supply vehicle of its team, refilled there (UBattleMatch::UpdateArea),
 // resumes its mission once most of its members have ammunition again. Without a vehicle with stock in reach it stops the trip
 // (the next evaluation may send it home as before).
 if(Match->bArea && P.Action==EBattleSquadTactic::Recover && P.Reason.StartsWith(TEXT("FIELD SUPPLY")))
 {
  if(Emergency){Event(TEXT("FieldSupplyAbortEmergency"));Retreat();return;}
  FVector Car;int32 CarID=INDEX_NONE;
  if(!Match->FieldSupplyPoint(SID,Car,&CarID)){Event(TEXT("FieldSupplyLost"));Store->CancelSquadTacticalPlan(SID,TEXT("FIELD SUPPLY / NO VEHICLE"));return;}
  // Log only (-BattleFieldSupplyLog, default off; STATUS 2026-11-25): where the squad on its trip is and what order it holds, every 5 s.
  static const bool bTripLog=FParse::Param(FCommandLine::Get(),TEXT("BattleFieldSupplyLog"));
  if(bTripLog)
  {
   static TMap<int32,double> TripLogAt;double& At=TripLogAt.FindOrAdd(SID,-100.);
   if(Match->Elapsed-At>=5.||Match->Elapsed<At)
   {
    At=Match->Elapsed;const auto* LU=Store->Find(S->LeaderUnitID);
    UE_LOG(LogFutureBattle,Display,TEXT("FIELD_SUPPLY_TRIP t=%.0f squad=%d car=%d dist=%.0f here=%s carpos=%s plan_dest=%s remaining=%.0f age=%.0f order=%d dest=%s mission=%d leader_order=%d leader_action=%d leader_dest=%s retries=%d threats=%d armed=%d living=%d"),
     Match->Elapsed,SID,CarID,FVector::Dist2D(Here,Car),*Here.ToCompactString(),*Car.ToCompactString(),*P.Destination.ToCompactString(),P.Remaining,P.Age,int32(S->Order),*S->Destination.ToCompactString(),int32(S->MissionOrder),
     LU?int32(LU->Order):-1,LU?int32(LU->Action):-1,LU?*LU->Destination.ToCompactString():TEXT("none"),LU?LU->MoveRetries:-1,Threats,O.Armed,O.Living);
   }
  }
  if(O.Armed>=FMath::CeilToInt(O.Living*.9f)&&bAmmoBack)
  {
   ResumeMission(SID,TEXT("FIELD SUPPLY COMPLETE / RESUME"),TEXT("FieldSupplyResume"));
   return;
  }
  // The vehicle moved: follow it.
  if(FVector::Dist2D(P.Destination,Car)>1500.)Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Move,Car,TEXT("FIELD SUPPLY / TO SUPPLY VEHICLE"));
  return;
 }
 if(Match->bArea && P.Action==EBattleSquadTactic::Recover && P.Reason.StartsWith(TEXT("HOME SUPPLY")))
 {
  if(Emergency&&!bMachine){Event(TEXT("HomeSupplyAbortEmergency"));SupplyTripEnd(SID,false);Retreat();return;}
  // Trial -BattleRepairDepot: a machine's trip also ends at the depot (where it is repaired; the other conditions below still apply).
  const bool bDepotTrip=bMachine&&Match->DepotWorking(S->Team);
  // Trial -BattleDepotHealth: the depot broke while the machine drove to it; drive to the home instead.
  if(bMachine&&!bDepotTrip&&BattleTactics::RepairDepot()>0.f&&FVector::Dist2D(P.Destination,Match->RepairPoint(S->Team))<=2000.)
  {
   const FVector HomeAt=Match->AreaHome(S->Team);
   if(Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Move,HomeAt+FVector((HomeAt.X>0?-1:1)*700,(SID%5-2)*600,0),TEXT("HOME SUPPLY / DEPOT BROKEN"))){Event(TEXT("HomeSupplyDepotBroken"));return;}
  }
  const bool Home=FVector::Dist2D(Here,Match->AreaHome(S->Team))<=2500||(bDepotTrip&&FVector::Dist2D(Here,Match->RepairPoint(S->Team))<=2500);
  const bool Manned=bNoReplenish||O.Living>=FMath::Min(S->Members.Num(),Supply.ResumeLiving)||Match->Credits[S->Team]==0;
  const bool Healed=Supply.ResumeHealthRatio<=0.f||O.Health/FullHP>=Supply.ResumeHealthRatio;
  if(Home && !bSPGEmpty && O.Armed>=FMath::CeilToInt(O.Living*.6f) && bAmmoBack && Manned && Healed)
  {
   if(ResumeMission(SID,TEXT("HOME SUPPLY COMPLETE / RESUME"),TEXT("HomeSupplyResume")))SupplyTripEnd(SID,true);
  }
  return;
 }
 if(bRepair&&bVehicle&&BattleTactics::VehicleHitCover())
 {
  FVector RepairHitOrigin;const double RepairNow=GetWorld()->GetTimeSeconds();
  if(Runtime->UnitShotFrom(S->LeaderUnitID,RepairNow,RepairHitOrigin,8.))
  {
   bRepair=false;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_VEHICLE_REPAIR_WAIT squad=%d since_hit=%.3f"),SID,RepairNow-Runtime->ShotFrom.FindChecked(S->LeaderUnitID).Value);
  }
 }
 // Field supply: half or more of the living members out of ammunition (losses that need replacements still go home) and a supply
 // vehicle of the team with stock within FieldSupplyRange: drive to it instead of home.
 if(Match->bArea && !bMachine && !bNoReplenish && Threats==0 && O.RecentDamage==0 && !(O.Living<=ReturnLiving&&Match->Credits[S->Team]>0) && (O.Armed<=O.Living/2||bLowAmmo) &&
    P.Action!=EBattleSquadTactic::Retreat && P.Action!=EBattleSquadTactic::Support && P.Action!=EBattleSquadTactic::Flank && P.Action!=EBattleSquadTactic::Recover)
 {
  FVector Car;
  if(Match->FieldSupplyPoint(SID,Car)&&Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Move,Car,TEXT("FIELD SUPPLY / TO SUPPLY VEHICLE"))){Event(TEXT("FieldSupplyGo"));return;}
 }
 if(bRepair || (Match->bArea && Threats==0 && O.RecentDamage==0 && ((O.Living<=ReturnLiving&&Match->Credits[S->Team]>0)||O.Armed==0||bLowAmmo||bSPGEmpty) && P.Action!=EBattleSquadTactic::Retreat && P.Action!=EBattleSquadTactic::Support && P.Action!=EBattleSquadTactic::Flank))
 {
  // Trial -BattleSupplyAwayRatio<1: keep most of the team on the line; only own-team plan state is read.
  bool Allowed=true;
  if(Supply.AwayRatio<1.f && O.Armed>0&&!bLowAmmo&&!bSPGEmpty)
  {
   int32 Active=0,Away=0;
   for(const auto& Other:Store->GetSquads()) if(Other.Value.Team==S->Team && Other.Value.CommandStatus==EBattleCommandStatus::Active)
   {++Active;Away+=Other.Value.TacticalPlan.Action==EBattleSquadTactic::Recover && Other.Value.TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY"));}
   Allowed=Away<FMath::Max(1,FMath::FloorToInt(Active*Supply.AwayRatio));
   if(!Allowed) Event(TEXT("HomeSupplyDeferredAwayLimit"));
  }
  const FVector Home=Match->AreaHome(S->Team);
  FVector Goal=Home+FVector((Home.X>0?-1:1)*700,(SID%5-2)*600,0);
  // Trial -BattleRepairDepot: a machine going for repair drives to the depot instead (spread a little like the home slots).
  if(bRepair&&Match->DepotWorking(S->Team))Goal=Match->RepairPoint(S->Team)+FVector((SID%3-1)*600,0,0);
  if(Allowed && Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Move,Goal,TEXT("HOME SUPPLY / RETURN WITH LEADER"))){Event(TEXT("HomeSupplyReturn"));
   // Evaluation only: trip reason (all members out of ammo vs losses) and whether the squad holds an S1 home-assault slot.
   bool bBase=false;if(Match->bArea&&BattleTactics::AreaStrategy().bEnabled)Match->AreaStrategyTarget(SID,S->Team,bBase);
   if(bRepair)Event(TEXT("HomeSupplyReturnVehicleRepair"));
   if(O.Armed==0||bLowAmmo)Event(TEXT("HomeSupplyReturnAmmo"));if(bLowAmmo&&O.Armed>0)Event(TEXT("HomeSupplyReturnLowAmmo"));if(bBase){Event(TEXT("HomeSupplyReturnBaseAssault"));if(O.Armed==0)Event(TEXT("HomeSupplyReturnBaseAssaultAmmo"));}
   SupplyTripBegin(SID);return;}
 }
 if(P.Action==EBattleSquadTactic::Retreat)
 {
  int32 Arrived=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive() && FVector::Dist2D(Runtime->Position(ID),P.Destination)<500) ++Arrived;
  const bool AtGoal=FVector::Dist2D(Here,P.Destination)<300 && Arrived>=FMath::CeilToInt(O.Living*.7f);
  bool Unsafe=false; for(const auto& C:O.Contacts) if(FVector::Dist2D(C.Value.LastKnownPosition,P.Destination)<1200) Unsafe=true;
  if(Unsafe && P.Age>=T->SquadRetreatRetrySeconds) { Retreat(); return; }
  if(AtGoal || P.Age>=T->SquadRetreatTimeout)
  {
   Apply(SID,EBattleSquadTactic::Regroup,EBattleOrder::Move,Here,AtGoal?TEXT("ARRIVED / WAIT FOR SAFE REGROUP"):TEXT("RETREAT TIMEOUT / REGROUP AT CURRENT POSITION"));
   P.RetrySeconds=T->SquadRetreatRetrySeconds;
  }
  return;
 }
 if(P.Action==EBattleSquadTactic::Regroup || P.Action==EBattleSquadTactic::Recover)
 {
  // Trial -BattleArtilleryRaidSneak: a regrouping raid goes straight back to its raid when nothing forces it back (exo: shield up again).
  if(bRaidSneak && P.Action==EBattleSquadTactic::Regroup && !Emergency && O.Armed>0 && P.Age>=2.f && (!bExo || ShieldRatio>=T->ExoResumeShield))
  {
   if(ResumeMission(SID,TEXT("RAID / RESUME MISSION"),TEXT("RaidResume")))return;
  }
  if(Threats && P.RetrySeconds<=0 && (Emergency || P.Action==EBattleSquadTactic::Regroup)) { Retreat(); return; }
  // Armour AI: a vehicle resumes above its repair threshold (below it, it drives home; between that and 60% it used to wait forever).
  const float ResumeHealth=(S->SquadType==BattleSquadType::Exo||(bVehicle&&BattleTactics::ArmourAI(S->Team)))?FMath::Min(.6f,GetDefault<UBattleTuning>()->VehicleRepairRatio):.6f;
  const bool ReadyToReturn=O.Armed>=FMath::CeilToInt(O.Living*.6f) && O.Living>S->Members.Num()*T->SquadRetreatLivingRatio && O.Health/FullHP>=ResumeHealth;
  // Exoskeleton: back to the mission as soon as the shield is up again (it does not wait out the regroup time).
  if(bExo && ShieldRatio>=T->ExoResumeShield && O.Health/FullHP>=ResumeHealth && O.RecentDamage==0 && O.Armed>0 && P.Age>=2.f)
  {
   if(ResumeMission(SID,TEXT("SHIELD RESTORED / RESUME MISSION"),TEXT("ExoShieldResume")))return;
  }
  int32 Joined=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive() && FVector::Dist2D(Runtime->Position(ID),P.Destination)<500) ++Joined;
  const bool Cohesive=Joined>=FMath::CeilToInt(O.Living*.7f);
  if(P.SafeSeconds>=T->SquadRegroupSafeSeconds && P.Age>=T->SquadRegroupSafeSeconds)
  {
   // Trial -BattleArmourWait: a tank it saw near it or near the mission point keeps the squad waiting here (BattleArmourAI.cpp).
   if(ReadyToReturn && Cohesive && P.Action==EBattleSquadTactic::Regroup && Match->ArmourWait(SID,S->MissionDestination))
   {
    if(!P.Reason.StartsWith(TEXT("ARMOUR NEAR"))){P.Reason=TEXT("ARMOUR NEAR / WAIT BEHIND");Event(TEXT("ArmourWait"));}
    P.SafeSeconds=0;
   }
   else if(ReadyToReturn && Cohesive)
   {
    // Validate the latest mission before releasing protection from the objective director.
    if(ResumeMission(SID,TEXT("SAFE REGROUP / RESUME MISSION"),TEXT("ResumeMission")))
     {UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ST2_RESUME squad=%d"),SID);}
    else { P.Reason=TEXT("MISSION ROUTE UNREACHABLE / HOLD"); P.SafeSeconds=0; }
   }
   else if(P.Action!=EBattleSquadTactic::Recover && (Cohesive || P.Age>=15))
    Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Defend,Here,Cohesive?TEXT("DEPLETED / DEFEND REGROUP POINT / NO AUTO HEAL"):TEXT("STRAGGLERS / DEFEND REGROUP POINT"));
  }
  return;
 }
 // Exoskeleton: with the shield gone and an enemy in sight it breaks contact; the shield refills out of the fight (own state and own contacts).
 if(bExo && Visible && ShieldRatio<=0.f && P.RetrySeconds<=0 && P.Action!=EBattleSquadTactic::Support)
 {CauseOverride=TEXT("SHIELD DOWN / BREAK CONTACT");Event(TEXT("ExoShieldBreak"));Retreat();S->TacticalPlan.RetrySeconds=FMath::Max(S->TacticalPlan.RetrySeconds,T->SquadRetreatRetrySeconds);return;}
 // Armour AI, stage 3 (BattleArmourAI.cpp): a vehicle backs out of known anti-tank reach or away from a heavy hit; infantry with no
 // anti-tank weapon falls back from a tank it sees. Only while a vehicle is on the field.
 if(Match->bArea && Runtime->VehicleIDs.Num() && P.RetrySeconds<=0 && P.Action!=EBattleSquadTactic::Support && BattleTactics::ArmourAI(S->Team))
 {
  FVector Goal;
  const int32 Reaction=Match->AreaArmourReaction(S->Team,SID,O.RecentDamage/FullHP,Goal,CauseOverride);
  if(Reaction==1 && Apply(SID,EBattleSquadTactic::Retreat,EBattleOrder::Move,Goal,CauseOverride)){Event(TEXT("ArmourStandOff"));P.RetrySeconds=T->SquadRetreatRetrySeconds;return;}
  if(Reaction==2){Event(TEXT("ArmourFallBack"));Retreat();S->TacticalPlan.RetrySeconds=FMath::Max(S->TacticalPlan.RetrySeconds,T->SquadRetreatRetrySeconds);return;}
  CauseOverride.Reset();
 }
 if(Emergency) { if(P.Action==EBattleSquadTactic::Flank)Event(TEXT("WideFlankEmergency"));Retreat(); return; }
 if(P.Action==EBattleSquadTactic::Flank) { EvaluateFlank(SID,O); return; }
 if(P.Action==EBattleSquadTactic::Support) return; // Emergency retreat above overrides team assignment.
 if(P.Action==EBattleSquadTactic::Engage)
 {
  // A moving surprise group (trial -BattleSurpriseGroup=1) goes back to its move as soon as it may (Codex review 2026-11-21: holding at
  // every seen enemy stopped the whole group, the leading armour too).
  const bool bSneak=Match->SurpriseMoving(SID)||Match->RaidSneaking(SID);
  if((P.SafeSeconds>=4 || bSneak) && P.Age>=3)
  {
   ResumeMission(SID,bSneak?TEXT("SURPRISE MOVE / RESUME MISSION"):TEXT("CONTACT LOST / RESUME MISSION"),nullptr);
  }
  return;
 }
 if(Visible && O.Armed>0 && !Match->SurpriseMoving(SID) && !Match->RaidSneaking(SID)) Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("KNOWN ENEMY / HOLD FIRING POSITION"));
}
