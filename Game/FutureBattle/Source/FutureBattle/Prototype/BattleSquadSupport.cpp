#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "FutureBattle.h"

bool UBattleSquadTactics::CanSupport(int32 SID,int32 Target) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID);
 if(!S || S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated)) return false;
 const auto O=Observe(*S); const auto* T=GetDefault<UBattleTuning>();
 if(O.Direct || O.Ready==0 || O.Living<=S->Members.Num()*T->SquadRetreatLivingRatio ||
    O.Health/FMath::Max(1.f,O.FullHealth(T->Health))<T->SquadRetreatHealthRatio ||
    O.RecentDamage>=T->Health*S->Members.Num()*T->SquadRetreatDamageRatio) return false;
 const auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 for(int32 ID:S->Members)
 {
  const auto* U=Store->Find(ID); const auto* C=Sense->FindContact(ID,Target);
  if(U && U->IsAlive() && U->Ammo>0 && U->ReloadEndTime==0 && C && C->bVisible && !C->bShared &&
     C->Confidence>.35f && !C->LastKnownPosition.ContainsNaN() && C->LastSeenTime<=GetWorld()->GetTimeSeconds()+.01 && GetWorld()->GetTimeSeconds()-C->LastSeenTime<=U->Perception.MemorySeconds &&
     FVector::Dist2D(R->Position(ID),C->LastKnownPosition)<=T->EffectiveWeaponRange() &&
     !GetWorld()->LineTraceTestByObjectType(R->Position(ID)+FVector(0,0,60),C->LastKnownPosition+FVector(0,0,35),FCollisionObjectQueryParams(ECC_WorldStatic))) return true;
 }
 return false;
}

bool UBattleSquadTactics::CanMaintainSupport(int32 SID,int32 Target) const
{
 if(CanSupport(SID,Target)) return true;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);
 if(!bCoordination || !S || S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0&&!S->bPlayerDelegated))return false;
 const auto& P=S->TacticalPlan;
 if(P.Action!=EBattleSquadTactic::Support || P.SupportTarget!=Target || !P.bSupportConfirmed || P.LastSupportShot<0 || Clock()-P.LastSupportShot>3.5)return false;
 const auto O=Observe(*S);const auto* Tune=GetDefault<UBattleTuning>();const auto* C=O.Contacts.Find(Target);
 return !O.Direct && O.Armed>0 && O.Living>S->Members.Num()*Tune->SquadRetreatLivingRatio &&
  O.Health>=O.FullHealth(Tune->Health)*Tune->SquadRetreatHealthRatio && O.RecentDamage<Tune->Health*S->Members.Num()*Tune->SquadRetreatDamageRatio &&
  C && !C->bShared && GetWorld()->GetTimeSeconds()-C->LastSeenTime<=3.5;
}

bool UBattleSquadTactics::TryReplaceFlankSupport(int32 SID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* S=Store->Squads.Find(SID);
 if(!bCoordination || !S || S->TacticalPlan.Action!=EBattleSquadTactic::Flank || S->TacticalPlan.FlankSupportChanges>=1 || S->TacticalPlan.Age>=20)return false;
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto& P=S->TacticalPlan;const int32 Old=P.FlankSupport;
 TArray<int32> Candidates;
 for(const auto& Pair:Store->GetSquads())
 {
  const auto& C=Pair.Value;
  if(C.Team==S->Team && C.SquadID!=SID && C.SquadID!=Old && C.CommandStatus==EBattleCommandStatus::Active &&
    (C.Team!=0||C.bPlayerDelegated) && (C.TacticalPlan.Action==EBattleSquadTactic::Mission||C.TacticalPlan.Action==EBattleSquadTactic::Engage) &&
    FVector::Dist2D(R->Position(C.LeaderUnitID),R->Position(S->LeaderUnitID))<=5000)Candidates.Add(C.SquadID);
 }
 Candidates.Sort([&](int32 A,int32 B){const double DA=FVector::DistSquared2D(R->Position(Store->FindSquad(A)->LeaderUnitID),R->Position(S->LeaderUnitID));const double DB=FVector::DistSquared2D(R->Position(Store->FindSquad(B)->LeaderUnitID),R->Position(S->LeaderUnitID));return DA==DB?A<B:DA<DB;});
 for(int32 I=0;I<FMath::Min(3,Candidates.Num());++I)
 {
  P.FlankSupport=Candidates[I];
  if(AcceptSupport(Candidates[I],SID,P.FlankTarget))
  {
   ++P.FlankSupportChanges;
   Store->Squads.FindChecked(Candidates[I]).TacticalPlan.SupportUntil=Clock()+FMath::Max(0.f,20-P.Age);
   const auto* Previous=Store->FindSquad(Old);
   if(Previous && Previous->TacticalPlan.SupportSquad==SID)ReleaseSupport(Old,TEXT("COVER REPLACED"));
   Event(TEXT("FlankSupportReplaced"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_HANDOFF squad=%d old=%d new=%d age=%.2f"),SID,Old,P.FlankSupport,P.Age);
   return true;
  }
 }
 P.FlankSupport=Old;return false;
}

bool UBattleSquadTactics::AcceptSupport(int32 SID,int32 OtherID,int32 Target)
{
 const auto* Mode=GetWorld()->GetAuthGameMode<ABattleGameMode>();
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* S=Store->Squads.Find(SID); const auto* Other=Store->FindSquad(OtherID);
 if(!IsST2Enabled() || !R->IsWideField() || !Mode || Mode->bCombatPaused || Mode->bBattleEnded ||
    !S || !Other || SID==OtherID || S->Team!=Other->Team ||
    (Other->TacticalPlan.Action!=EBattleSquadTactic::Retreat && Other->TacticalPlan.Action!=EBattleSquadTactic::Flank) ||
    (Other->TacticalPlan.Action==EBattleSquadTactic::Flank && (Other->TacticalPlan.FlankSupport!=SID || Other->TacticalPlan.FlankTarget!=Target)) ||
    (S->TacticalPlan.Action!=EBattleSquadTactic::Mission && S->TacticalPlan.Action!=EBattleSquadTactic::Engage) ||
    Other->CommandStatus!=EBattleCommandStatus::Active || (Other->Team==0 && !Other->bPlayerDelegated) ||
    Store->Find(Other->LeaderUnitID)->Action==EBattleAction::DirectControl ||
    FVector::Dist2D(R->Position(S->LeaderUnitID),R->Position(Other->LeaderUnitID))>(bCoordination&&Other->TacticalPlan.Action==EBattleSquadTactic::Flank?5000:2000) ||
    !CanSupport(SID,Target)) return false;
 // Local emergency assessment has final say, even when the team report was stale.
 Evaluate(SID,Observe(*S),0);
 if(S->TacticalPlan.Action!=EBattleSquadTactic::Mission && S->TacticalPlan.Action!=EBattleSquadTactic::Engage) return false;
 if(!Apply(SID,EBattleSquadTactic::Support,EBattleOrder::Hold,R->Position(S->LeaderUnitID),
    FString::Printf(TEXT("COVER S%d / TARGET %d / WAITING FOR FIRE"),OtherID,Target))) return false;
 auto& P=S->TacticalPlan; P.SupportSquad=OtherID; P.SupportTarget=Target;
 P.SupportUntil=Clock()+(Other->TacticalPlan.Action==EBattleSquadTactic::Flank?20:8);
 return true;
}

void UBattleSquadTactics::ReleaseSupport(int32 SID,const FString& Reason)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* S=Store->Squads.Find(SID);
 if(!S || S->TacticalPlan.Action!=EBattleSquadTactic::Support) return;
 const auto O=Observe(*S);
 // Handoff cancels only coordination, never issues movement to a player-controlled squad.
 if(O.Direct || S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated))
  Store->CancelSquadTacticalPlan(SID,Reason);
 else Apply(SID,EBattleSquadTactic::Engage,EBattleOrder::Hold,GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(S->LeaderUnitID),Reason);
 Event(TEXT("SupportReleased"));
}

void UBattleSquadTactics::RecordSupportShot(int32 SID,int32 Target)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* S=Store->Squads.Find(SID);
 if(!S || S->TacticalPlan.Action!=EBattleSquadTactic::Support || S->TacticalPlan.SupportTarget!=Target) return;
 auto& P=S->TacticalPlan; P.LastSupportShot=Clock();
 if(!P.bSupportConfirmed)
 {
  P.bSupportConfirmed=true;
  P.Reason=FString::Printf(TEXT("COVER S%d / TARGET %d / FIRING"),P.SupportSquad,Target);
  Event(TEXT("SupportConfirmed"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TEAM_SUPPORT confirmed=%d retreat=%d target=%d"),SID,P.SupportSquad,Target);
 }
}
