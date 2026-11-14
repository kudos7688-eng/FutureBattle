#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleTeamAI.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "FutureBattle.h"
#include "BattleMatch.h"

bool UBattleSquadTactics::SoloFlankRisk(const FBattleSquadObservation& O,FVector Start,double Length,double Exposure) const
{
 const auto* T=GetDefault<UBattleTuning>();
 if(!bRelaxedFlank || O.Living<7 || O.Armed<FMath::CeilToInt(O.Living*.6f) || O.Direct ||
    O.Health<O.FullHealth(T->Health)*.7f || O.RecentDamage>O.Living*T->Health*.1f) return false;
 int32 Nearby=0;for(const auto& C:O.Contacts)if(FVector::Dist2D(Start,C.Value.LastKnownPosition)<=T->EffectiveWeaponRange())++Nearby;
 // A short exposed crossing is allowed only for a healthy, locally unoutnumbered squad.
 return Nearby<=O.Living && (Length<=2000 || (Length<=3000 && Exposure<=.8));
}

bool UBattleSquadTactics::ReassessSoloFlank(int32 SID,const FBattleSquadObservation& O)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto& P=Store->Squads.FindChecked(SID).TacticalPlan;
 const auto* C=O.Contacts.Find(P.FlankTarget);
 if(!C || C->bShared || GetWorld()->GetTimeSeconds()-C->LastSeenTime>2.5) return false;
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const FVector Start=R->Position(Store->FindSquad(SID)->LeaderUnitID);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 Event(TEXT("FlankReassessmentPath"));auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Start,P.Destination);
 if(!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength()>3000)return false;
 int32 Visible=0,Total=0;
 for(int32 J=1;J<Path->PathPoints.Num();++J)
 {
  const FVector A=Path->PathPoints[J-1],B=Path->PathPoints[J];
  if(GetWorld()->SweepTestByObjectType(A+FVector(0,0,100),B+FVector(0,0,100),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(40,88)))return false;
  const int32 N=FMath::Clamp(FMath::CeilToInt(FVector::Dist2D(A,B)/500.),1,10);
  for(int32 K=0;K<=N;++K)
  {
   const FVector At=FMath::Lerp(A,B,double(K)/N);
   for(const auto& Threat:O.Contacts)if(FVector::Dist2D(At,Threat.Value.LastKnownPosition)<FMath::Min(600.,FVector::Dist2D(Start,Threat.Value.LastKnownPosition)*.65))return false;
   ++Total;Visible+=!GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,170),C->LastKnownPosition+FVector(0,0,70),FCollisionObjectQueryParams(ECC_WorldStatic));
  }
 }
 return SoloFlankRisk(O,Start,Path->GetPathLength(),Total?double(Visible)/Total:1.);
}

bool UBattleSquadTactics::TryFlank(int32 SID,int32 Supporter)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* S=Store->Squads.Find(SID); auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Mode=GetWorld()->GetAuthGameMode<ABattleGameMode>();
 if(!IsST2Enabled() || !bFlank || !R->IsWideField() || !Mode || Mode->bCombatPaused || Mode->bBattleEnded ||
    !S || !IsFlankEnabledFor(S->Team) || S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated) ||
    S->FlankCooldown>0 || S->TacticalPlan.Action!=EBattleSquadTactic::Engage) return false;
 S->FlankCooldown=15; // Rate-limit failed attempts as well as accepted plans.
 Event(TEXT("FlankAttempt"));
 const auto O=Observe(*S); const auto* Tune=GetDefault<UBattleTuning>();
 if(O.Direct || O.Ready<FMath::CeilToInt(O.Living*.6f)) {Event(TEXT("FlankRejectReadiness"));return false;}
 if(O.Living<=S->Members.Num()*.6 || O.Health<(O.Living>0?O.FullHealth(Tune->Health):Tune->Health)*.7) {Event(TEXT("FlankRejectAttrition"));return false;}
 if(O.RecentDamage>(bRelaxedFlank?O.Living*Tune->Health*.1f:0.f)) {Event(TEXT("FlankRejectRecentDamage"));return false;}
 const FVector Start=R->Position(S->LeaderUnitID);
 const FBattleContact* Target=nullptr; double Nearest=Tune->EffectiveWeaponRange();
 for(const auto& C:O.Contacts) if(C.Value.bVisible && !C.Value.bShared)
 {
  const double D=FVector::Dist2D(Start,C.Value.LastKnownPosition);
  if(D<Nearest || (D==Nearest && Target && C.Key<Target->TargetID)) {Nearest=D;Target=&C.Value;}
 }
 if(!Target || Nearest<800) {Event(Target?TEXT("FlankRejectTooClose"):TEXT("FlankRejectNoDirectTarget"));return false;}
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); if(!Nav) return false;
 const FVector Threat=Target->LastKnownPosition;
 const FVector Away=(Start-Threat).GetSafeNormal2D();
 const auto* SupportSquad=Store->FindSquad(Supporter);
 const bool Supported=GetWorld()->GetSubsystem<UBattleTeamAI>()->IsEnabled() && SupportSquad && SupportSquad->Team==S->Team && Supporter!=SID &&
  (SupportSquad->TacticalPlan.Action==EBattleSquadTactic::Mission || SupportSquad->TacticalPlan.Action==EBattleSquadTactic::Engage) &&
  FVector::Dist2D(Start,R->Position(SupportSquad->LeaderUnitID))<=2000 && CanSupport(Supporter,Target->TargetID);
 double Best=-DBL_MAX; FVector BestGoal; double BestExposure=1; bool BestSolo=false;
 // At most two candidates per side, independent of the number of soldiers.
 for(int32 Side:{-1,1}) for(float Angle:{30.f,55.f})
 {
  const FVector Raw=Threat+Away.RotateAngleAxis(Side*Angle,FVector::UpVector)*FMath::Clamp(Nearest*.85,800.,3500.);
  FNavLocation Goal;
  if(!Nav->ProjectPointToNavigation(Raw,Goal,FVector(150,150,400))) {Event(TEXT("FlankCandidateOffNav"));continue;}
  if(GetWorld()->LineTraceTestByObjectType(Goal.Location+FVector(0,0,170),Threat+FVector(0,0,70),FCollisionObjectQueryParams(ECC_WorldStatic))) {Event(TEXT("FlankCandidateBlockedFiringPosition"));continue;}
  Event(TEXT("FlankPathRequest"));
  auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Start,Goal.Location);
  if(!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength()>5000) {Event(TEXT("FlankCandidateUnreachableOrLong"));continue;}
  int32 Visible=0,Total=0; bool Unsafe=false;
  for(int32 J=1;J<Path->PathPoints.Num();++J)
  {
   const FVector A=Path->PathPoints[J-1],B=Path->PathPoints[J];
   if(GetWorld()->SweepTestByObjectType(A+FVector(0,0,100),B+FVector(0,0,100),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(40,88))) Unsafe=true;
   const int32 SampleCount=FMath::Clamp(FMath::CeilToInt(FVector::Dist2D(A,B)/500.),1,100);
   for(int32 K=0;K<=SampleCount;++K)
   {
    const FVector P=FMath::Lerp(A,B,double(K)/SampleCount);
    for(const auto& C:O.Contacts)
     if(FVector::Dist2D(P,C.Value.LastKnownPosition)<FMath::Min(600.,FVector::Dist2D(Start,C.Value.LastKnownPosition)*.65)) Unsafe=true;
    ++Total; Visible+=!GetWorld()->LineTraceTestByObjectType(P+FVector(0,0,170),Threat+FVector(0,0,70),FCollisionObjectQueryParams(ECC_WorldStatic));
   }
  }
  const double Exposure=Total?double(Visible)/Total:1.;
  if(Unsafe) {Event(TEXT("FlankCandidateCollisionOrNearThreat"));continue;}
  const bool Solo=Exposure<=.65 || SoloFlankRisk(O,Start,Path->GetPathLength(),Exposure);
  if(!Supported && !Solo) {Event(TEXT("FlankCandidateExposedWithoutSupport"));continue;}
  // Prefer a reachable short lateral move over a large angle that requires
  // sustained fire support for most of the twenty-second action deadline.
  const double Score=Angle*10-Path->GetPathLength()*.8-Exposure*1500;
  if(Score>Best) {Best=Score;BestGoal=Goal.Location;BestExposure=Exposure;BestSolo=Solo;}
 }
 if(Best==-DBL_MAX) {Event(TEXT("FlankNoSafeRoute"));return false;}
 if(!Apply(SID,EBattleSquadTactic::Flank,EBattleOrder::Move,BestGoal,TEXT("FLANK / KNOWN CONTACT / BOUNDED ROUTE"))) return false;
 auto& P=S->TacticalPlan;
 P.FlankTarget=Target->TargetID; P.FlankContact=Threat;
 P.FlankSupport=BestSolo?INDEX_NONE:Supporter;
 P.FlankExposure=BestExposure;
 P.Reason=FString::Printf(TEXT("FLANK T%d / %s / EXP %.0f%%"),P.FlankTarget,P.FlankSupport==INDEX_NONE?TEXT("SOLO"):TEXT("COVER REQUIRED"),100*BestExposure);
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleFlankTrial")))
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_BEGIN t=%.3f squad=%d team=%d target=%d support=%d exposure=%.4f x=%.1f y=%.1f gx=%.1f gy=%.1f tx=%.1f ty=%.1f living=%d hp=%.1f"),GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,SID,S->Team,P.FlankTarget,P.FlankSupport,BestExposure,Start.X,Start.Y,BestGoal.X,BestGoal.Y,Threat.X,Threat.Y,O.Living,O.Health);
 return true;
}

bool UBattleSquadTactics::RefreshFlankTarget(int32 SID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* S=Store->Squads.Find(SID);
 if(!S || S->TacticalPlan.Action!=EBattleSquadTactic::Flank) return false;
 auto& P=S->TacticalPlan; auto* Cover=Store->Squads.Find(P.FlankSupport);
 if(!Cover || Cover->TacticalPlan.Action!=EBattleSquadTactic::Support || Cover->TacticalPlan.SupportSquad!=SID) return false;
 if(CanSupport(P.FlankSupport,P.FlankTarget)) return true;
 const auto O=Observe(*S); int32 Target=INDEX_NONE; double Best=DBL_MAX;
 const double Now=GetWorld()->GetTimeSeconds();
 // Cover the same local engagement, using own observations only. Keep the
 // original route, deadline and last actual shot; switching cannot extend them.
 for(const auto& C:O.Contacts)
 {
  const double D=FVector::DistSquared2D(C.Value.LastKnownPosition,P.FlankContact);
  if(C.Value.bShared || C.Value.Confidence<=.35f || Now-C.Value.LastSeenTime>2.5 || D>FMath::Square(1000.) ||
     !CanSupport(P.FlankSupport,C.Key)) continue;
  if(D<Best || (D==Best && C.Key<Target)) {Best=D;Target=C.Key;}
 }
 if(Target==INDEX_NONE) return false;
 P.FlankTarget=Target; Cover->TacticalPlan.SupportTarget=Target;
 Cover->TacticalPlan.Reason=FString::Printf(TEXT("COVER S%d / TARGET %d / LOCAL RETARGET"),SID,Target);
 Event(TEXT("FlankSupportRetarget")); return true;
}

void UBattleSquadTactics::EvaluateFlank(int32 SID,const FBattleSquadObservation& O)
{
 RefreshFlankTarget(SID);
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* S=Store->Squads.Find(SID);
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); auto& P=S->TacticalPlan;
 const auto* Contact=O.Contacts.Find(P.FlankTarget);
 FString End;
 if(!Contact || FVector::Dist2D(Contact->LastKnownPosition,P.FlankContact)>1000) End=TEXT("FLANK ABORT / CONTACT STALE OR MOVED");
 else if(P.Age>=20 || O.Failed>0) End=TEXT("FLANK ABORT / TIMEOUT OR ROUTE FAILURE");
 else if(P.FlankSupport!=INDEX_NONE)
 {
  const auto* Cover=Store->FindSquad(P.FlankSupport);
  const bool Authority=Cover && Cover->TacticalPlan.Action==EBattleSquadTactic::Support && Cover->TacticalPlan.SupportSquad==SID;
  const bool Available=Authority && (bCoordination?CanMaintainSupport(P.FlankSupport,P.FlankTarget):CanSupport(P.FlankSupport,P.FlankTarget));
  const bool Recent=Authority && ((bCoordination?Cover->TacticalPlan.Age:P.Age)<=3 || (Cover->TacticalPlan.bSupportConfirmed && Clock()-Cover->TacticalPlan.LastSupportShot<=(bCoordination?3.5:2.5)));
  if(!Authority || !Available || !Recent)
  {
   Event(!Authority?TEXT("FlankSupportLossRole"):!Available?TEXT("FlankSupportLossFireEligibility"):TEXT("FlankSupportLossShotTimeout"));
   if(TryReplaceFlankSupport(SID)) { P.Reason=TEXT("FLANK / REPLACEMENT COVER"); }
   else if(bRelaxedFlank && ReassessSoloFlank(SID,O))
   {
    P.FlankSupport=INDEX_NONE;P.Reason=TEXT("FLANK / SUPPORT LOST / SOLO RISK ACCEPTED");Event(TEXT("FlankContinuedSolo"));
   }
   else End=TEXT("FLANK ABORT / REQUIRED FIRE SUPPORT LOST");
   if(bObserve)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_SUPPORT_LOSS squad=%d partner=%d role=%s reason=%s eligible=%d recent=%d known=%d visible=%d"),SID,P.FlankSupport,Cover?BattleSquadTacticName(Cover->TacticalPlan.Action):TEXT("MISSING"),Cover?*Cover->TacticalPlan.Reason:TEXT("MISSING"),Available,Recent,Contact!=nullptr,Contact&&Contact->bVisible);
  }
 }
 if(End.IsEmpty())
 {
  int32 Arrived=0;
  for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive() && FVector::Dist2D(R->Position(ID),P.Destination)<500) ++Arrived;
  if(FVector::Dist2D(R->Position(S->LeaderUnitID),P.Destination)<300 && Arrived>=FMath::CeilToInt(O.Living*.7))
   End=TEXT("FLANK ARRIVED / ENGAGE");
 }
 if(!End.IsEmpty())
 {
  const float Age=P.Age;
  Event(End.StartsWith(TEXT("FLANK ARRIVED"))?TEXT("WideFlankArrived"):
   End.Contains(TEXT("CONTACT"))?TEXT("WideFlankLostContact"):
   End.Contains(TEXT("SUPPORT"))?TEXT("WideFlankLostSupport"):TEXT("WideFlankTimeoutOrRouteFailure"));
  Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,R->Position(S->LeaderUnitID),End);
  S->FlankCooldown=15;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_END squad=%d seconds=%.2f reason=%s"),SID,Age,*End);
 }
}

