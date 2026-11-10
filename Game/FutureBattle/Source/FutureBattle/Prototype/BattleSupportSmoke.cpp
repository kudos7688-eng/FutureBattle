#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

void ABattleGameMode::RunSupportSmokeTest()
{
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController()); PC->ResetMatchControl();
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>(); auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>(); Perception->Reset();
	for (ABattleUnit* Unit : Units) if (Unit) { Store->ApplyDamage(Unit->GetUnitID(), 1 - Unit->GetState()->Team, 100000); Unit->ApplyDeath(); if (Unit->SavedAI) Unit->SavedAI->Destroy(); Unit->Destroy(); }
	Units.Reset(); Match->Phase = EBattleMatchPhase::Combat; Match->Elapsed = 0; Match->DirectorClock = Match->SupplyClock = 0;
	Match->Credits[0] = Match->Credits[1] = 5000; Match->Score[0] = Match->Score[1] = 0; Match->Strikes.Reset();
	Match->SupportUsed[0] = Match->SupportUsed[1] = 0; Match->SupportCooldown[0] = Match->SupportCooldown[1] = 0;
	bCombatPaused = false; bBattleEnded = false; Result.Reset();
	Match->HireSquad(0, EBattleTraining::Regular); Match->HireSquad(1, EBattleTraining::Regular);
	if (Units.Num() != 10) { CheckSmoke(false, TEXT("Support fixture deployed")); CompleteSmokeTest(); return; }
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		ABattleUnit* Unit = Units[Index]; Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); Unit->GetCharacterMovement()->StopMovementImmediately();
		Unit->SetActorLocation(FVector(2000 + (Index % 5) * 120, 2200 - (Index / 5) * 250, 95), false, nullptr, ETeleportType::TeleportPhysics);
	}
	const int32 ObserverID = Units[0]->GetUnitID(), FriendID = Units[1]->GetUnitID(), TargetID = Units[5]->GetUnitID(), VictimID = Units[6]->GetUnitID(), ShieldID = Units[7]->GetUnitID();
	const FVector KnownPosition(-1200, -2200, 95);
	Units[0]->SetActorLocation(KnownPosition + FVector(-500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
	Units[1]->SetActorLocation(KnownPosition + FVector(-200, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
	Units[6]->SetActorLocation(KnownPosition + FVector(200, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
	Units[7]->SetActorLocation(KnownPosition + FVector(0, 200, 0), false, nullptr, ETeleportType::TeleportPhysics);
	Perception->Reset(); const int32 Credits = Match->Credits[0];
	CheckSmoke(!Match->RequestSupport(ObserverID) && Match->Credits[0] == Credits && Match->Strikes.IsEmpty(), TEXT("Support cannot target unknown enemies or charge for a rejected request"));
	FBattleObserver Observer; Observer.ProfileRevision = Store->Find(ObserverID)->PerceptionRevision;
	FBattleContact Contact; Contact.TargetID = TargetID; Contact.SourceUnitID = FriendID; Contact.LastKnownPosition = KnownPosition; Contact.LastSeenTime = -1.; Contact.Confidence = .2f; Contact.bShared = true;
	Observer.Contacts.Add(TargetID, Contact); TMap<int32, FBattleObserver> Known; Known.Add(ObserverID, Observer);
	Perception->Restore(Known, {}, GetWorld()->GetTimeSeconds());
	CheckSmoke(!Match->RequestSupport(ObserverID) && Match->Credits[0] == Credits, TEXT("Low-confidence shared reports cannot authorize fire support"));
	Known.FindChecked(ObserverID).Contacts.FindChecked(TargetID).Confidence = .8f; Perception->Restore(Known, {}, GetWorld()->GetTimeSeconds());
	CheckSmoke(Match->RequestSupport(ObserverID) && Match->Credits[0] == Credits - 400 && Match->SupportUsed[0] == 1 && Match->Strikes[0].Position == KnownPosition, TEXT("Reliable shared position schedules a paid strike at the old observation rather than the moved enemy"));
	CheckSmoke(!Match->RequestSupport(ObserverID) && Match->Credits[0] == Credits - 400 && Match->Strikes.Num() == 1, TEXT("Cooldown rejects duplicate support without extra payment"));
	auto* Wall = GetWorld()->SpawnActor<AStaticMeshActor>(KnownPosition + FVector(0, 100, 70), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->SetActorScale3D(FVector(1, .3, 4)); Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	auto* Save = Match->CaptureSnapshot(); FString Error;
	CheckSmoke(Save && Save->Validate(Error) && Save->Strikes.Num() == 1 && Save->AllySupportUsed == 1 && Save->AllySupportCooldown == 20, TEXT("Pending impact delay cooldown and consumed mission count are included in save validation"));
	Match->UpdateSupport(2.f);
	CheckSmoke(Store->Find(VictimID)->HP == 100 && Match->Strikes.Num() == 1 && Match->Strikes[0].Remaining == 1, TEXT("Delayed support causes no early damage"));
	bCombatPaused = true; Match->UpdateSupport(10.f);
	CheckSmoke(Match->Strikes[0].Remaining == 1 && Match->SupportCooldown[0] == 18, TEXT("Pausing freezes both support arrival and rearm timers"));
	CheckSmoke(Match->RestoreSnapshot(Save) && Match->Strikes.Num() == 1 && Match->Strikes[0].Remaining == 3 && Match->SupportCooldown[0] == 20 && Match->SupportUsed[0] == 1, TEXT("Restoring an in-flight strike neither duplicates it nor refunds its mission"));
	for (ABattleUnit* Unit : Units) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
	auto* Bad = DuplicateObject<UBattleSave>(Save, GetTransientPackage()); Bad->Strikes[0].Remaining = -1;
	CheckSmoke(!Match->RestoreSnapshot(Bad) && Match->Strikes.Num() == 1 && Match->Strikes[0].Remaining == 3, TEXT("Corrupt support timer rejects the complete snapshot without altering the current mission"));
	Match->UpdateSupport(3.01f);
	CheckSmoke(Store->Find(VictimID)->HP < 100 && Store->Find(TargetID)->HP == 100 && Match->Strikes.IsEmpty(), TEXT("Impact damages enemies near the recorded point and does not follow the moved target"));
	CheckSmoke(Store->Find(ShieldID)->HP == 100 && Store->Find(FriendID)->HP == 100, TEXT("Physical cover blocks blast damage and friendly units remain protected"));
	Match->UpdateSupport(20.f); Match->RequestSupport(ObserverID); Match->UpdateSupport(20.f); Match->RequestSupport(ObserverID);
	Match->UpdateSupport(20.f); const int32 AfterThree = Match->Credits[0];
	CheckSmoke(Match->SupportUsed[0] == 3 && !Match->RequestSupport(ObserverID) && Match->Credits[0] == AfterThree, TEXT("Three-mission cap remains enforced after cooldowns expire"));
	Match->SupportUsed[0] = 0; Match->Credits[0] = 399;
	CheckSmoke(!Match->RequestSupport(ObserverID) && Match->Credits[0] == 399 && Match->SupportUsed[0] == 0, TEXT("Insufficient support funds do not spend a mission"));
	Match->Credits[0] = 1000; Perception->Restore(Known, {}, GetWorld()->GetTimeSeconds());
	const float HP = Store->Find(VictimID)->HP; Match->RequestSupport(ObserverID);
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Match, VictimID, HP, Wall]()
	{
		const auto* S = GetWorld()->GetSubsystem<UBattleState>()->Find(VictimID);
		CheckSmoke(Match->Strikes.IsEmpty() && S && S->HP < HP, TEXT("World tick executes the delayed strike once after three seconds"));
		Wall->Destroy(); CompleteSmokeTest();
	}), 3.3f, false);
}
