#include "BattleTeamAI.h"
#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

void UBattleTeamAI::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 int32 Enabled=1; FParse::Value(FCommandLine::Get(),TEXT("BattleTeamSupport="),Enabled); bEnabled=Enabled!=0;
}

void UBattleTeamAI::Step()
{
 auto* World=GetWorld(); auto* T=World->GetSubsystem<UBattleSquadTactics>();
 auto* R=World->GetSubsystem<UBattleLeaderRuntime>(); auto* Store=World->GetSubsystem<UBattleState>();
 const auto* Mode=World->GetAuthGameMode<ABattleGameMode>();
 if((!bEnabled && !T->IsFlankEnabled()) || !T->IsST2Enabled() || !R->IsWideField() || !Mode || Mode->bCombatPaused || Mode->bBattleEnded) return;
 const double Now=T->Clock();
 const auto* RaidMatch=World->GetSubsystem<UBattleMatch>();
 auto Managed=[&](const FBattleSquadState* S)
 {
  if(!S || S->CommandStatus!=EBattleCommandStatus::Active || (S->Team==0 && !S->bPlayerDelegated)) return false;
  // Trial -BattleArtilleryRaidSneak: a raid squad on its way is not taken for flank/support (an active flank is handed off).
  if(RaidMatch && RaidMatch->RaidSneaking(S->SquadID)) return false;
  const auto* Leader=Store->Find(S->LeaderUnitID);
  return Leader && Leader->IsAlive() && Leader->Action!=EBattleAction::DirectControl;
 };
 TArray<int32> IDs; Store->GetSquads().GetKeys(IDs); IDs.Sort();
 for(int32 ID:IDs) if(const auto* S=Store->FindSquad(ID); S->TacticalPlan.Action==EBattleSquadTactic::Flank && !Managed(S))
 { T->Event(TEXT("WideFlankHandoff"));Store->CancelSquadTacticalPlan(ID,TEXT("FLANK / AUTHORITY HANDOFF")); }
 TSet<int32> Covered;
 if(Now>=NextAssignment) for(int32 ID:IDs) T->RefreshFlankTarget(ID);
 // Cheap authority cleanup each tick; acceptance and contact/line checks at 1 Hz.
 for(int32 ID:IDs)
 {
  const auto* S=Store->FindSquad(ID); const auto& P=S->TacticalPlan;
  if(P.Action!=EBattleSquadTactic::Support) continue;
  const auto* Other=Store->FindSquad(P.SupportSquad);
  const auto* O=Other?T->Find(Other->SquadID):nullptr;
  const bool Flanking=Other && Other->TacticalPlan.Action==EBattleSquadTactic::Flank && Other->TacticalPlan.FlankSupport==ID && Other->TacticalPlan.FlankTarget==P.SupportTarget;
  const bool Valid=bEnabled && Managed(S) && Managed(Other) && S->Team==Other->Team &&
   (Other->TacticalPlan.Action==EBattleSquadTactic::Retreat || Flanking) && !Covered.Contains(Other->SquadID) &&
   FVector::Dist2D(R->Position(S->LeaderUnitID),R->Position(Other->LeaderUnitID))<=(Flanking?5000:2000) &&
   O && Now-O->SampleTime<=2.5 && O->Contacts.Contains(P.SupportTarget) && Now<P.SupportUntil;
  const bool Grace=Flanking && T->bCoordination;
  if(!Valid || (Now>=NextAssignment && !(Grace?T->CanMaintainSupport(ID,P.SupportTarget):T->CanSupport(ID,P.SupportTarget))) ||
     (P.Age>=3 && (P.LastSupportShot<0 || Now-P.LastSupportShot>(Grace?3.5:2.5))))
  {
   T->ReleaseSupport(ID,TEXT("SUPPORT ENDED / AUTHORITY, CONTACT, FIRE OR DEADLINE"));
   RetryAfter.Add(ID,Now+5);
  }
  else Covered.Add(Other->SquadID);
 }
 if(Now<NextAssignment) return;
 NextAssignment=Now+1;
 if(bEnabled) for(int32 ID:IDs)
 {
  const auto* Retreat=Store->FindSquad(ID); const auto* O=T->Find(ID);
  if(!Managed(Retreat) || Retreat->TacticalPlan.Action!=EBattleSquadTactic::Retreat || Covered.Contains(ID) || !O || Now-O->SampleTime>2.5) continue;
  TArray<int32> Candidates;
  // Filter by faction before any position/resource query: the coordinator never ranks enemy truth.
  for(int32 Candidate:IDs)
  {
   const auto* S=Store->FindSquad(Candidate);
   if(S->Team==Retreat->Team && Candidate!=ID && Managed(S) && RetryAfter.FindRef(Candidate)<=Now &&
      (S->TacticalPlan.Action==EBattleSquadTactic::Mission || S->TacticalPlan.Action==EBattleSquadTactic::Engage)) Candidates.Add(Candidate);
  }
  Candidates.Sort([&](int32 A,int32 B){
   const double DA=FVector::DistSquared2D(R->Position(Store->FindSquad(A)->LeaderUnitID),R->Position(Retreat->LeaderUnitID));
   const double DB=FVector::DistSquared2D(R->Position(Store->FindSquad(B)->LeaderUnitID),R->Position(Retreat->LeaderUnitID));
   return DA==DB?A<B:DA<DB;
  });
  for(int32 Candidate:Candidates)
  {
   const auto* S=Store->FindSquad(Candidate);
   if(Candidate==ID || !Managed(S) || S->Team!=Retreat->Team || RetryAfter.FindRef(Candidate)>Now ||
      (S->TacticalPlan.Action!=EBattleSquadTactic::Mission && S->TacticalPlan.Action!=EBattleSquadTactic::Engage) ||
      FVector::Dist2D(R->Position(S->LeaderUnitID),R->Position(Retreat->LeaderUnitID))>2000) continue;
   const auto* Report=T->Find(Candidate); if(!Report || Now-Report->SampleTime>2.5) continue;
   TArray<int32> Targets; O->Contacts.GetKeys(Targets); Targets.Sort();
   bool Assigned=false;
   for(int32 Target:Targets) if(Report->Contacts.Contains(Target) && T->AcceptSupport(Candidate,ID,Target))
   {
    Covered.Add(ID); Assigned=true;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TEAM_SUPPORT assigned=%d retreat=%d target=%d"),Candidate,ID,Target);
    break;
   }
   if(Assigned) break;
  }
 }

 // At most one new flank per faction and assignment tick; keep most squads on mission.
 if(T->IsFlankEnabled()) for(int32 Faction=0;Faction<2;++Faction)
 {
  if(!T->IsFlankEnabledFor(Faction)) continue;
  int32 Eligible=0,ActiveFlanks=0;
  for(int32 ID:IDs) if(const auto* S=Store->FindSquad(ID); S->Team==Faction && Managed(S))
  {
   ++Eligible; ActiveFlanks+=S->TacticalPlan.Action==EBattleSquadTactic::Flank;
  }
  if(ActiveFlanks>=FMath::Max(1,Eligible/4)) continue;
  for(int32 ID:IDs)
  {
   const auto* S=Store->FindSquad(ID); const auto* O=T->Find(ID);
   if(S->Team!=Faction || !Managed(S) || !O || Now-O->SampleTime>2.5 ||
      S->FlankCooldown>0 || S->TacticalPlan.Action!=EBattleSquadTactic::Engage) continue;
   // Trial -BattleJoinFight: a squad walking to another squad's fight has no enemy to flank yet (a failed attempt would end this team's search).
   if(S->TacticalPlan.Reason.StartsWith(TEXT("JOIN FIGHT")) || S->TacticalPlan.Reason.StartsWith(TEXT("SEEK ENEMY"))) continue;
   TArray<int32> Partners;
   for(int32 OtherID:IDs)
   {
    const auto* Other=Store->FindSquad(OtherID);
    if(OtherID!=ID && Other->Team==Faction && Managed(Other) && RetryAfter.FindRef(OtherID)<=Now &&
       (Other->TacticalPlan.Action==EBattleSquadTactic::Mission || Other->TacticalPlan.Action==EBattleSquadTactic::Engage) &&
       FVector::Dist2D(R->Position(S->LeaderUnitID),R->Position(Other->LeaderUnitID))<=2000)
      Partners.Add(OtherID);
   }
   Partners.Sort([&](int32 A,int32 B){
    const double DA=FVector::DistSquared2D(R->Position(S->LeaderUnitID),R->Position(Store->FindSquad(A)->LeaderUnitID));
    const double DB=FVector::DistSquared2D(R->Position(S->LeaderUnitID),R->Position(Store->FindSquad(B)->LeaderUnitID));
    return DA==DB?A<B:DA<DB;
   });
   // Select a partner that can cover the same nearest directly observed contact.
   int32 Target=INDEX_NONE;double Distance=GetDefault<UBattleTuning>()->EffectiveWeaponRange();
   for(const auto& C:O->Contacts) if(C.Value.bVisible && !C.Value.bShared)
   {
    const double D=FVector::Dist2D(R->Position(S->LeaderUnitID),C.Value.LastKnownPosition);
    if(D<Distance || (D==Distance && C.Key<Target)){Distance=D;Target=C.Key;}
   }
   int32 Partner=INDEX_NONE;
   for(int32 OtherID:Partners) if(bEnabled && Target!=INDEX_NONE && T->CanSupport(OtherID,Target)){Partner=OtherID;break;}
   if(!T->TryFlank(ID,Partner)) break; // At most four path candidates per team per second.
   const auto& P=Store->FindSquad(ID)->TacticalPlan;
   if(P.FlankSupport!=INDEX_NONE && !T->AcceptSupport(P.FlankSupport,ID,P.FlankTarget))
    T->EvaluateFlank(ID,T->Observe(*Store->FindSquad(ID)));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TEAM_FLANK squad=%d partner=%d active=%d"),ID,Partner,Store->FindSquad(ID)->TacticalPlan.Action==EBattleSquadTactic::Flank);
   break;
  }
 }
}
