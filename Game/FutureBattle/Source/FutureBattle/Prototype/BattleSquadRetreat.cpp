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
 const bool Emergency=Threats>0 && (Depleted || O.RecentDamage>=T->Health*S->Members.Num()*T->SquadRetreatDamageRatio || Threats>FMath::Max(3.f,O.Armed*T->SquadRetreatThreatRatio));
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
 const bool bRepair=Match->bArea&&bVehicle&&Supply.HealPerSecond>0.f&&O.Health/FullHP<GetDefault<UBattleTuning>()->VehicleRepairRatio;
 if(Match->bArea && P.Action==EBattleSquadTactic::Recover && P.Reason.StartsWith(TEXT("HOME SUPPLY")))
 {
  if(Emergency&&!bVehicle){Event(TEXT("HomeSupplyAbortEmergency"));SupplyTripEnd(SID,false);Retreat();return;}
  const bool Home=FVector::Dist2D(Here,Match->AreaHome(S->Team))<=2500;
  const bool Manned=bNoReplenish||O.Living>=FMath::Min(S->Members.Num(),Supply.ResumeLiving)||Match->Credits[S->Team]==0;
  const bool Healed=Supply.ResumeHealthRatio<=0.f||O.Health/FullHP>=Supply.ResumeHealthRatio;
  if(Home && O.Armed>=FMath::CeilToInt(O.Living*.6f) && Manned && Healed)
  {
   const auto Order=S->MissionOrder;const FVector Goal=S->MissionDestination;
   if(Runtime->IssueOrder(SID,Order,Goal,true)){Store->CancelSquadTacticalPlan(SID,TEXT("HOME SUPPLY COMPLETE / RESUME"));Runtime->IssueOrder(SID,Order,Goal,true);Event(TEXT("HomeSupplyResume"));SupplyTripEnd(SID,true);}
  }
  return;
 }
 if(bRepair || (Match->bArea && Threats==0 && O.RecentDamage==0 && ((O.Living<=ReturnLiving&&Match->Credits[S->Team]>0)||O.Armed==0) && P.Action!=EBattleSquadTactic::Retreat && P.Action!=EBattleSquadTactic::Support && P.Action!=EBattleSquadTactic::Flank))
 {
  // Trial -BattleSupplyAwayRatio<1: keep most of the team on the line; only own-team plan state is read.
  bool Allowed=true;
  if(Supply.AwayRatio<1.f && O.Armed>0)
  {
   int32 Active=0,Away=0;
   for(const auto& Other:Store->GetSquads()) if(Other.Value.Team==S->Team && Other.Value.CommandStatus==EBattleCommandStatus::Active)
   {++Active;Away+=Other.Value.TacticalPlan.Action==EBattleSquadTactic::Recover && Other.Value.TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY"));}
   Allowed=Away<FMath::Max(1,FMath::FloorToInt(Active*Supply.AwayRatio));
   if(!Allowed) Event(TEXT("HomeSupplyDeferredAwayLimit"));
  }
  const FVector Home=Match->AreaHome(S->Team);
  const FVector Goal=Home+FVector((Home.X>0?-1:1)*700,(SID%5-2)*600,0);
  if(Allowed && Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Move,Goal,TEXT("HOME SUPPLY / RETURN WITH LEADER"))){Event(TEXT("HomeSupplyReturn"));
   // Evaluation only: trip reason (all members out of ammo vs losses) and whether the squad holds an S1 home-assault slot.
   bool bBase=false;if(Match->bArea&&BattleTactics::AreaStrategy().bEnabled)Match->AreaStrategyTarget(SID,S->Team,bBase);
   if(bRepair)Event(TEXT("HomeSupplyReturnVehicleRepair"));
   if(O.Armed==0)Event(TEXT("HomeSupplyReturnAmmo"));if(bBase){Event(TEXT("HomeSupplyReturnBaseAssault"));if(O.Armed==0)Event(TEXT("HomeSupplyReturnBaseAssaultAmmo"));}
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
  if(Threats && P.RetrySeconds<=0 && (Emergency || P.Action==EBattleSquadTactic::Regroup)) { Retreat(); return; }
  // Armour AI: a vehicle resumes above its repair threshold (below it, it drives home; between that and 60% it used to wait forever).
  const float ResumeHealth=bVehicle&&BattleTactics::ArmourAI(S->Team)?FMath::Min(.6f,GetDefault<UBattleTuning>()->VehicleRepairRatio):.6f;
  const bool ReadyToReturn=O.Armed>=FMath::CeilToInt(O.Living*.6f) && O.Living>S->Members.Num()*T->SquadRetreatLivingRatio && O.Health/FullHP>=ResumeHealth;
  int32 Joined=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive() && FVector::Dist2D(Runtime->Position(ID),P.Destination)<500) ++Joined;
  const bool Cohesive=Joined>=FMath::CeilToInt(O.Living*.7f);
  if(P.SafeSeconds>=T->SquadRegroupSafeSeconds && P.Age>=T->SquadRegroupSafeSeconds)
  {
   if(ReadyToReturn && Cohesive)
   {
    const auto Order=S->MissionOrder; const FVector Goal=S->MissionDestination;
    // Validate the latest mission before releasing protection from the objective director.
    if(Runtime->IssueOrder(SID,Order,Goal,true))
    {
     Store->CancelSquadTacticalPlan(SID,TEXT("SAFE REGROUP / RESUME MISSION"));
     Runtime->IssueOrder(SID,Order,Goal,true); Event(TEXT("ResumeMission"));
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ST2_RESUME squad=%d"),SID);
    }
    else { P.Reason=TEXT("MISSION ROUTE UNREACHABLE / HOLD"); P.SafeSeconds=0; }
   }
   else if(P.Action!=EBattleSquadTactic::Recover && (Cohesive || P.Age>=15))
    Apply(SID,EBattleSquadTactic::Recover,EBattleOrder::Defend,Here,Cohesive?TEXT("DEPLETED / DEFEND REGROUP POINT / NO AUTO HEAL"):TEXT("STRAGGLERS / DEFEND REGROUP POINT"));
  }
  return;
 }
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
  if(P.SafeSeconds>=4 && P.Age>=3)
  {
   const auto Order=S->MissionOrder; const FVector Goal=S->MissionDestination;
   if(Runtime->IssueOrder(SID,Order,Goal,true)) { Store->CancelSquadTacticalPlan(SID,TEXT("CONTACT LOST / RESUME MISSION")); Runtime->IssueOrder(SID,Order,Goal,true); }
  }
  return;
 }
 if(Visible && O.Armed>0) Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,Here,TEXT("KNOWN ENEMY / HOLD FIRING POSITION"));
}
