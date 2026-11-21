#include "BattleGameMode.h"
#include "BattleSquadTactics.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleFieldLayout.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void ABattleGameMode::RunSquadTacticsSmokeTest()
{
 bCombatPaused = true; bBattleEnded = false; CombatStartsAt = 0;
 auto* Store = GetWorld()->GetSubsystem<UBattleState>();
 auto* Tactics = GetWorld()->GetSubsystem<UBattleSquadTactics>();
 auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>();
 auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
 auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 Match->InitializeMatch(); Match->Phase = EBattleMatchPhase::Combat;
 PC->EnterTactical(); ABattleUnit* A = Units[0]; PC->SetSelection(A);
 const int32 SID = A->GetState()->SquadID, AID = A->GetUnitID();
 TMap<int32,FVector> Goals;
 for (int32 ID : Store->FindSquad(SID)->Members) Goals.Add(ID, FindUnitByID(ID)->GetActorLocation());
 const FVector Origin = A->GetActorLocation();
 Store->SetSquadOrder(SID, EBattleOrder::Hold, Origin, Goals);
 const int32 BaseRevision = Store->FindSquad(SID)->OrderRevision;
 CheckSmoke(!Store->SetSquadTacticalPlan(SID, EBattleSquadTactic::Retreat, EBattleOrder::Move, Origin, Goals, 3, TEXT("fixture")), TEXT("Manual squad rejects autonomous tactical execution"));
 PC->DelegateSelected();
 if (Tactics->IsEnabled())
 {
  CheckSmoke(Store->SetSquadTacticalPlan(SID, EBattleSquadTactic::Retreat, EBattleOrder::Move, Origin, Goals, 3, TEXT("FIXTURE RETREAT")), TEXT("Delegated squad accepts validated tactical execution"));
  CheckSmoke(Store->FindSquad(SID)->MissionOrder == EBattleOrder::Hold && Store->FindSquad(SID)->Order == EBattleOrder::Move, TEXT("Mission and executing order are separate"));
  const int32 ExecutionRevision = Store->FindSquad(SID)->OrderRevision;
  bCombatPaused = false; Match->DelegationClock = 5; Match->Update(.1f); bCombatPaused = true;
  CheckSmoke(Store->FindSquad(SID)->OrderRevision == ExecutionRevision && Store->FindSquad(SID)->MissionOrder == EBattleOrder::AttackMove, TEXT("Real objective director updates mission without overwriting retreat"));
  const float Remaining = Store->FindSquad(SID)->TacticalPlan.Remaining;
  const double Clock = Tactics->Clock(); Tactics->Step(2);
  CheckSmoke(Tactics->Clock() == Clock && Store->FindSquad(SID)->TacticalPlan.Remaining == Remaining, TEXT("Paused combat freezes tactical clock and plan lifetime"));
  TMap<int32,FVector> InvalidGoals;
  CheckSmoke(!Store->SetSquadOrder(SID, EBattleOrder::Hold, Origin, InvalidGoals) && Store->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Retreat, TEXT("Rejected order preserves plan and execution"));
  const auto Before = *A->GetState();
  CheckSmoke(PC->ControlUnit(A), TEXT("Direct control can begin during a tactical plan"));
  const auto DirectSample = Tactics->Observe(*Store->FindSquad(SID));
  CheckSmoke(DirectSample.Direct == 1 && DirectSample.Ready == 4, TEXT("Directly controlled soldier is excluded from automatic ready count"));
  bCombatPaused = false; Tactics->Step(.5f); bCombatPaused = true;
  CheckSmoke(PC->GetPawn() == A && A->GetState()->Action == EBattleAction::DirectControl && A->GetState()->HP == Before.HP && A->GetState()->Ammo == Before.Ammo, TEXT("Tactical step preserves player control and resources"));
  PC->EnterTactical();
  CheckSmoke(Store->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Retreat, TEXT("TPS RTS switch preserves squad plan"));
  CheckSmoke(Match->CaptureSnapshot() == nullptr, TEXT("ST1 refuses unsupported active-plan snapshot rather than dropping it"));
  bCombatPaused = false; Tactics->Step(3); bCombatPaused = true;
  CheckSmoke(Store->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Mission && Store->FindSquad(SID)->Order == EBattleOrder::AttackMove, TEXT("Expired plan resumes latest mission"));
  Store->SetSquadTacticalPlan(SID, EBattleSquadTactic::Support, EBattleOrder::Hold, Origin, Goals, 3, TEXT("FIXTURE SUPPORT"));
  const int32 HeldRevision = Store->FindSquad(SID)->OrderRevision;
  PC->TakeCommandSelected();
  CheckSmoke(!Store->FindSquad(SID)->bPlayerDelegated && Store->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Mission && Store->FindSquad(SID)->OrderRevision == HeldRevision, TEXT("K cancels tactical authority while keeping executing order"));
  PC->DelegateSelected();
  Store->SetSquadTacticalPlan(SID, EBattleSquadTactic::Support, EBattleOrder::Hold, Origin, Goals, 3, TEXT("FIXTURE SUPPORT"));
  PC->HoldSelected();
  CheckSmoke(!Store->FindSquad(SID)->bPlayerDelegated && Store->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Mission, TEXT("Accepted manual hold cancels delegation and plan"));
 }
 else
 {
  CheckSmoke(!Store->SetSquadTacticalPlan(SID, EBattleSquadTactic::Retreat, EBattleOrder::Move, Origin, Goals, 3, TEXT("fixture")) && Store->FindSquad(SID)->OrderRevision == BaseRevision, TEXT("Feature OFF rejects plans and leaves executing order untouched"));
  bCombatPaused = false; Match->DelegationClock = 5; Match->Update(.1f); bCombatPaused = true;
  CheckSmoke(Store->FindSquad(SID)->Order == EBattleOrder::AttackMove, TEXT("Feature OFF retains legacy objective orders"));
 }
 // Delivered contact fixture: duplicate observations, delayed report not yet delivered, stale contact.
 Perception->Reset();
 const int32 EnemyID = Units.Last()->GetUnitID();
 const int32 OtherEnemy = Units[Units.Num()-2]->GetUnitID();
 TMap<int32,FBattleObserver> Observers;
 FBattleContact Contact; Contact.TargetID = EnemyID; Contact.SourceUnitID = AID;
 Contact.LastKnownPosition = FVector(900, 1000, 95); Contact.LastSeenTime = -1; Contact.Confidence = .8f;
 for (int32 ID : Store->FindSquad(SID)->Members)
 {
  FBattleObserver Observer; Observer.ProfileRevision = Store->Find(ID)->PerceptionRevision;
  Observer.Contacts.Add(EnemyID, Contact); Observers.Add(ID, Observer);
 }
 FBattleReport Report; Report.ReceiverID = AID; Report.Contact = Contact; Report.Contact.TargetID = OtherEnemy;
 Report.DeliveryTime = 2;
 Perception->Restore(Observers, {Report}, GetWorld()->GetTimeSeconds());
 const auto Known = Tactics->Observe(*Store->FindSquad(SID));
 CheckSmoke(Known.Contacts.Num() == 1 && Known.Contacts.Contains(EnemyID), TEXT("Squad observation deduplicates contacts and excludes undelivered reports"));
 ABattleUnit* Hidden = FindUnitByID(EnemyID); const FVector HiddenPosition = Hidden->GetActorLocation();
 Hidden->SetActorLocation(HiddenPosition + FVector(700, 0, 0)); Store->ApplyDamage(EnemyID, 0, 1);
 const auto Changed = Tactics->Observe(*Store->FindSquad(SID));
 CheckSmoke(Changed.Contacts.Num() == Known.Contacts.Num() && Changed.Contacts.FindChecked(EnemyID).LastKnownPosition.Equals(Contact.LastKnownPosition) && Changed.Reason == Known.Reason, TEXT("Unobserved enemy movement and HP cannot change tactical input or reason"));
 Hidden->SetActorLocation(HiddenPosition);
 for (auto& Pair : Observers) Pair.Value.Contacts.FindChecked(EnemyID).LastSeenTime = -100;
 Perception->Restore(Observers, {}, GetWorld()->GetTimeSeconds());
 CheckSmoke(Tactics->Observe(*Store->FindSquad(SID)).Contacts.IsEmpty(), TEXT("Expired observations cannot become current tactical threats"));
 Perception->Reset();
 if (!BattleFieldLayout::IsLarge(GetWorld()))
 {
  auto* Save = Match->CaptureSnapshot(); FString Error; TArray<uint8> Bytes;
  CheckSmoke(Save && Save->Validate(Error), *FString::Printf(TEXT("Mission snapshot validates: %s"), *Error));
  auto* Decoded = Save && UGameplayStatics::SaveGameToMemory(Save, Bytes) ? Cast<UBattleSave>(BattleSaveValidation::Decode(Bytes)) : nullptr;
  CheckSmoke(Decoded && Decoded->Validate(Error) && Match->RestoreSnapshot(Decoded) && Store->FindSquad(SID)->bHasMission, TEXT("Mission survives real binary save and world restore"));
  A = FindUnitByID(AID); PC->SetSelection(A);
  auto* Old = Match->CaptureSnapshot();
  if (Old) for (auto& Squad : Old->Squads) { Squad.bHasMission = false; Squad.MissionGoals.Reset(); }
  CheckSmoke(Old && Match->RestoreSnapshot(Old) && Store->FindSquad(SID)->bHasMission, TEXT("Legacy missing mission fields migrate from stored own orders"));
  A = FindUnitByID(AID); PC->SetSelection(A);
  auto* Invalid = Match->CaptureSnapshot();
  if (Invalid) Invalid->Squads[0].MissionGoals.Add(999999, FVector::ZeroVector);
  const int32 Rev = Store->FindSquad(SID)->OrderRevision;
  CheckSmoke(Invalid && !Match->RestoreSnapshot(Invalid) && Store->FindSquad(SID)->OrderRevision == Rev, TEXT("Invalid mission save is rejected before world mutation"));
  if (Tactics->IsEnabled())
  {
   auto* BeforeDeath = Match->CaptureSnapshot(); PC->DelegateSelected();
   Store->SetSquadTacticalPlan(SID, EBattleSquadTactic::Support, EBattleOrder::Hold, Origin, Goals, 3, TEXT("DEATH FIXTURE"));
   for (int32 ID : Store->FindSquad(SID)->Members) Store->ApplyDamage(ID, 1, 10000);
   CheckSmoke(Store->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Mission && Tactics->Observe(*Store->FindSquad(SID)).Living == 0, TEXT("Last member death cancels tactical authority immediately"));
   CheckSmoke(Tactics->Observe(*Store->FindSquad(SID)).RecentDamage > 0, TEXT("Recent own damage is recorded without enemy state access"));
   CheckSmoke(BeforeDeath && Match->RestoreSnapshot(BeforeDeath), TEXT("Death fixture restores clean live match"));
   A = FindUnitByID(AID); PC->SetSelection(A);
  }
 }
 // Live observation is explicitly separate from the synthetic command/knowledge checks above.
 Tactics->Reset(); PC->SelectAll(); PC->DelegateSelected(); PC->SetSelection(A);
 bCombatPaused = false; CombatStartsAt = 0; Match->DelegationClock = 5;
 float Duration = 8; FParse::Value(FCommandLine::Get(), TEXT("BattleSTObserveSeconds="), Duration); Duration = FMath::Clamp(Duration, 3.f, 90.f);
 FString Prefix; FParse::Value(FCommandLine::Get(), TEXT("BattleSTScreens="), Prefix);
 FTimerHandle Finish;
 GetWorldTimerManager().SetTimer(Finish, FTimerDelegate::CreateWeakLambda(this,[this,Prefix,SID]()
 {
  auto* T = GetWorld()->GetSubsystem<UBattleSquadTactics>();
  bCombatPaused = true; T->Dump();
  CheckSmoke(T->Find(SID) != nullptr, TEXT("Real world ticks produce squad observations"));
  CheckSmoke(GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID)->TacticalPlan.Action == EBattleSquadTactic::Mission, TEXT("ST1 does not autonomously select ST2 ST3 actions"));
  if (!Prefix.IsEmpty())
  {
   auto* Controller = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
   Controller->EnterTactical();
   for (ABattleUnit* Unit : Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0) { Controller->SetSelection(Unit); break; }
   if (auto* Camera = Cast<ABattleCameraPawn>(Controller->GetPawn())) { Camera->SetActorLocation(FVector::ZeroVector); Camera->Zoom(30000); }
   CheckSmoke(!Controller->GetSelection().IsEmpty(), TEXT("HUD capture has a living selected squad"));
   FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-rts.png"),true,false);
  }
  UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SMOKE_RESULT failures=%d"), SmokeFailures);
  FTimerHandle Exit;
  GetWorldTimerManager().SetTimer(Exit, FTimerDelegate::CreateWeakLambda(this,[this](){ FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0); }), 1.f, false);
 }), Duration, false);
}
