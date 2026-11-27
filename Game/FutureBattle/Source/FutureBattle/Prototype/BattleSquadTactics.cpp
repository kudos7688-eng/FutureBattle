#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleState.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleTeamAI.h"
#include "BattleLeaderRuntime.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/CharacterMovementComponent.h"

void UBattleSquadTactics::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 int32 Enabled = 1; FParse::Value(FCommandLine::Get(), TEXT("BattleSquadTactics="), Enabled);
 bEnabled = Enabled != 0;
 int32 CoverActions=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverActions="),CoverActions);bCoverActions=CoverActions!=0;
 int32 Coordination=0;FParse::Value(FCommandLine::Get(),TEXT("BattleFlankCoordination="),Coordination);bCoordination=Coordination!=0;
 int32 ST2=1; FParse::Value(FCommandLine::Get(),TEXT("BattleSquadST2="),ST2); bST2=ST2!=0;
 int32 Flank=1; FParse::Value(FCommandLine::Get(),TEXT("BattleSquadFlank="),Flank); bFlank=Flank!=0;
 int32 Relaxed=0;FParse::Value(FCommandLine::Get(),TEXT("BattleFlankRelaxed="),Relaxed);bRelaxedFlank=Relaxed!=0;
 FParse::Value(FCommandLine::Get(),TEXT("BattleFlankTeams="),FlankTeams);FlankTeams=FMath::Clamp(FlankTeams,0,3);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_CONFIG relaxed=%d teams=%d"),bRelaxedFlank,FlankTeams);
 bObserve = FParse::Param(FCommandLine::Get(), TEXT("BattleTacticalObserve"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TACTICS_CONFIG enabled=%d st2=%d flank=%d"),bEnabled,bST2,bFlank);
}

void UBattleSquadTactics::Reset()
{
 ActiveSeconds = 0; NextDump = 5; NextSample.Reset(); Samples.Reset(); Events.Reset(); ActionSeconds.Reset();
 SupplyTripStart.Reset(); SupplyTripSeconds.Reset(); JoinFights.Reset(); FieldRecoverGaveUp.Reset(); FieldRecoverSince.Reset(); FieldRecoverArrived.Reset();
 SeekEnemies.Reset(); SeekEnded.Reset(); KnownSpots[0].Reset(); KnownSpots[1].Reset(); KnownSpotsAt[0]=KnownSpotsAt[1]=-100.;
 if(BattleTactics::SectorDefendMarch()>0)
 {
  SectorMarches.Reset();SectorMarchKeepAt.Reset();SectorMarchBaseSlots.Reset();
  for(int32 MarchTeam=0;MarchTeam<2;++MarchTeam){SectorMarchKnown[MarchTeam].Reset();SectorMarchKnownAt[MarchTeam]=-100.;}
 }
 if(BattleTactics::SectorDefend()>0){SectorDefends.Reset();DefendSlotCounts.Reset();DefendSlotsAt=-100.;}
 if(BattleTactics::SectorSeize()>0)
 {
  SeizeSectors.Reset();SeizeEnded.Reset();SeizeSuspended.Reset();
  if(BattleTactics::SectorSeizePoint()>0 || BattleTactics::SectorSeizeCounter()>0)ResetSeizeExtensions();
  for(int32 SeizeTeam=0;SeizeTeam<2;++SeizeTeam)
  {SeizeOwnCells[SeizeTeam].Reset();SeizeKnownCells[SeizeTeam].Reset();SeizeCellsAt[SeizeTeam]=-100.;}
 }
 if(BattleTactics::SpotShare() || BattleTactics::SpotShots() || BattleTactics::SpotLost() || BattleTactics::SpotHQ()>0.f || (BattleTactics::SpotTruth()&1))ResetSpotRelax();
 GetWorld()->GetSubsystem<UBattleTeamAI>()->Reset();
 DamageHistory.Reset(); TotalStepMs = MaxStepMs = 0; StepCount = 0;
}

FBattleSquadObservation UBattleSquadTactics::Observe(const FBattleSquadState& Squad) const
{
 FBattleSquadObservation Out; Out.SampleTime = ActiveSeconds;
 const auto* Store = GetWorld()->GetSubsystem<UBattleState>();
 const auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>();
 const double Now = GetWorld()->GetTimeSeconds();
 if (const auto* History = DamageHistory.Find(Squad.SquadID))
  for (const auto& Hit : *History) if (ActiveSeconds - Hit.Key <= 5.) Out.RecentDamage += Hit.Value;
 // Only own members and their delivered contacts. Never resolve a contact to an enemy body/state.
 // -BattleSquadKnowledge=1: the squad's table already holds the newest contact of each enemy (same rule as the merge below).
 const auto* SquadKnown = Perception->GetSquadContacts(Squad.SquadID);
 if (SquadKnown) for (const auto& Pair : *SquadKnown)
 {
  const auto& C = Pair.Value.Contact;
  if (C.Confidence <= .35f || C.LastKnownPosition.ContainsNaN() || !FMath::IsFinite(C.LastSeenTime) || C.LastSeenTime > Now + .01) continue;
  Out.Contacts.Add(C.TargetID, C);
 }
 for (int32 ID : Squad.Members) if (const auto* U = Store->Find(ID); U && U->IsAlive())
 {
  ++Out.Living; Out.Health += U->HP; Out.MaxHealth += Store->MaxHealthOf(ID); Out.Ammo += U->Ammo; Out.Reserve += U->Reserve;
  Out.Direct += U->Action == EBattleAction::DirectControl;
  Out.Ready += U->Action != EBattleAction::DirectControl && U->Ammo > 0 && U->ReloadEndTime == 0;
  Out.Armed += U->Action != EBattleAction::DirectControl && U->Ammo + U->Reserve > 0;
  Out.Cover += U->CoverSlot != INDEX_NONE; Out.Support += U->Role == EBattleRole::Support;
  Out.Flank += U->Role == EBattleRole::Flank; Out.Failed += U->Action == EBattleAction::MoveFailed;
  if (const auto* Contacts = Perception->GetContacts(ID)) for (const auto& Pair : *Contacts)
  {
   if (SquadKnown && !Pair.Value.bShared) continue; // his own sightings are in the squad's table; a hint (AddHint) is still his own
   const auto& C = Pair.Value;
   if (C.Confidence <= .35f || C.LastKnownPosition.ContainsNaN() || !FMath::IsFinite(C.LastSeenTime) ||
       Now - C.LastSeenTime > U->Perception.MemorySeconds || C.LastSeenTime > Now + .01) continue;
   auto* Existing = Out.Contacts.Find(C.TargetID);
   if (!Existing || C.LastSeenTime > Existing->LastSeenTime ||
       (C.LastSeenTime == Existing->LastSeenTime && (C.bVisible > Existing->bVisible ||
       (C.bVisible == Existing->bVisible && C.SourceUnitID < Existing->SourceUnitID)))) Out.Contacts.Add(C.TargetID, C);
  }
 }
 if(BattleTactics::PointTruth())
 {
  int32 PointContactCount=0;
  for(const auto& PointContact:Out.Contacts)if(PointContact.Value.bPointTruth && PointContact.Value.bVisible && !PointContact.Value.bShared)++PointContactCount;
  if(PointContactCount)GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("PointTruthContacts"),PointContactCount);
 }
 Out.Reason = Out.Living == 0 ? TEXT("NO LIVING MEMBERS") : Out.Failed ? TEXT("LOCAL MOVE FAILED") :
  Out.Contacts.IsEmpty() ? TEXT("NO KNOWN CONTACT / FOLLOW MISSION") : TEXT("KNOWN CONTACT / LOCAL AI");
 return Out;
}

void UBattleSquadTactics::SupplyTripEnd(int32 SquadID, bool bResumed)
{
 // Evaluation only. A trip begun before a save/load has no start time and is counted separately.
 double Start=0;
 if(SupplyTripStart.RemoveAndCopyValue(SquadID,Start)){ if(bResumed) SupplyTripSeconds.Add(ActiveSeconds-Start); }
 else Event(TEXT("HomeSupplyEndWithoutStart"));
}

void UBattleSquadTactics::Event(FName Reason, int32 Count)
{
 if (bObserve || GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsWideField()) Events.FindOrAdd(Reason) += Count;
}

void UBattleSquadTactics::RecordDamage(int32 SquadID, float Damage)
{
 const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
 const auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
 if ((!bEnabled && !bObserve) || !Mode || (Mode->bLeaderCommand && !GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsWideField()) || (!bObserve && !Match->IsEnabled())) return;
 auto& History = DamageHistory.FindOrAdd(SquadID);
 History.RemoveAll([this](const auto& Hit) { return ActiveSeconds - Hit.Key > 5.; });
 // At most one bucket per active second, bounded even while the combat clock is paused.
 const double Bucket = FMath::FloorToDouble(ActiveSeconds);
 if (!History.IsEmpty() && History.Last().Key == Bucket) History.Last().Value += Damage;
 else History.Emplace(Bucket, Damage);
}

void UBattleSquadTactics::Step(float DeltaSeconds)
{
 const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
 const auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
 const bool Wide=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsWideField();
 if ((!bEnabled && !bObserve) || !Mode || (Mode->bLeaderCommand && !Wide) || Mode->bCombatPaused || Mode->bBattleEnded ||
     GetWorld()->GetTimeSeconds() < Mode->CombatStartsAt || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0 ||
     (Match->IsEnabled() && Match->Phase != EBattleMatchPhase::Combat)) return;
 if (!Match->IsEnabled() && !bObserve) return;
 const double Start = FPlatformTime::Seconds();
 ActiveSeconds += DeltaSeconds;
 auto* Store = GetWorld()->GetSubsystem<UBattleState>();
 for (const auto& Pair : Store->GetSquads())
 {
  const auto& Squad = Pair.Value;
  if (Squad.CommandStatus != EBattleCommandStatus::Legacy && !(Wide && Squad.CommandStatus==EBattleCommandStatus::Active)) continue;
  const bool bManaged = (!BattleTactics::BaseGarrison()||!Match->IsBaseGarrison(Pair.Key)) && bEnabled && Match->IsEnabled() && (Squad.Team == 1 || Squad.bPlayerDelegated);
  if (bManaged && !Squad.TacticalPlan.bAutonomous) Store->AdvanceSquadTacticalPlan(Squad.SquadID, DeltaSeconds);
  if (!bManaged && !bObserve) { Samples.Remove(Pair.Key); continue; }
  double* Due = NextSample.Find(Pair.Key);
  if (!Due) Due = &NextSample.Add(Pair.Key, ActiveSeconds + (BattleTactics::PhaseIndex(GetWorld(),Pair.Key,Squad.Team) % 10) * .1);
  if (ActiveSeconds < *Due) continue;
  const float SampleDt=Samples.Contains(Pair.Key)?float(ActiveSeconds-Samples.FindChecked(Pair.Key).SampleTime):0.f;
  *Due = ActiveSeconds + 1.;
  auto Sample = Observe(Squad);
  if(bObserve && !Sample.Contacts.IsEmpty())
  {
   double Nearest=DBL_MAX;
   const FVector Here=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(Squad.LeaderUnitID);
   for(const auto& C:Sample.Contacts) Nearest=FMath::Min(Nearest,FVector::Dist2D(Here,C.Value.LastKnownPosition));
   const auto* Leader=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Actor(Squad.LeaderUnitID);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ST_RANGE squad=%d nearest_cm=%.1f leader=%s speed=%.1f max=%.1f root=%d"),Pair.Key,Nearest,*Here.ToCompactString(),Leader?Leader->GetVelocity().Size2D():0.,Leader?Leader->GetCharacterMovement()->MaxWalkSpeed:0.f,Leader?Leader->IsPlayingRootMotion():false);
  }
  if(bManaged && Wide && Sample.Direct==0) ActionSeconds.FindOrAdd(FName(BattleSquadTacticName(Squad.TacticalPlan.Action)))+=FMath::Min(SampleDt,2.f);
  if(bManaged) Store->Squads.FindChecked(Pair.Key).FlankCooldown=FMath::Max(0.f,Squad.FlankCooldown-FMath::Min(SampleDt,2.f));
  if (bManaged && Wide && bST2) Evaluate(Pair.Key,Sample,FMath::Min(SampleDt,2.f));
  if (bObserve) UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_ST_SAMPLE t=%.2f squad=%d team=%d managed=%d living=%d ready=%d direct=%d hp=%.1f damage5s=%.1f ammo=%d reserve=%d contacts=%d cover=%d support=%d flank=%d failed=%d reason=%s"),
   ActiveSeconds, Pair.Key, Squad.Team, bManaged, Sample.Living, Sample.Ready, Sample.Direct, Sample.Health, Sample.RecentDamage, Sample.Ammo, Sample.Reserve, Sample.Contacts.Num(), Sample.Cover, Sample.Support, Sample.Flank, Sample.Failed, *Sample.Reason);
  Samples.Add(Pair.Key, MoveTemp(Sample));
 }
 if(BattleTactics::SectorSeize()>0 || BattleTactics::SectorDefend()>0)CheckSectorTrials();
 if (Wide && IsST2Enabled()) GetWorld()->GetSubsystem<UBattleTeamAI>()->Step();
 const double Cost = (FPlatformTime::Seconds() - Start) * 1000.;
 ++StepCount; TotalStepMs += Cost; MaxStepMs = FMath::Max(MaxStepMs, Cost);
 if (bObserve && ActiveSeconds >= NextDump) { Dump(); NextDump = ActiveSeconds + 5; }
}

void UBattleSquadTactics::Dump() const
{
 if (!bObserve) return;
 UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_ST_COST t=%.2f steps=%lld avg_ms=%.6f max_ms=%.6f includes_observation_logging=1"), ActiveSeconds, StepCount, StepCount ? TotalStepMs / StepCount : 0, MaxStepMs);
 TArray<FName> Keys; Events.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
 for (FName Key : Keys) UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_ST_EVENT t=%.2f reason=%s count=%lld"), ActiveSeconds, *Key.ToString(), Events.FindChecked(Key));
}

void UBattleSquadTactics::Deinitialize() { Dump(); Super::Deinitialize(); }
