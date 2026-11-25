#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

void ABattleGameMode::RunPerceptionSmokeTest()
{
	RunPerceptionOptimizationSmokeTest();
	bCombatPaused = true;
	ABattleUnit* A = Units[5]; ABattleUnit* B = Units[6]; ABattleUnit* C = Units[7]; ABattleUnit* Enemy = Units[15];
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	UBattlePerception* Sense = GetWorld()->GetSubsystem<UBattlePerception>();
	TArray<TObjectPtr<ABattleUnit>> Fixture = { A, B, C, Enemy };
	for (ABattleUnit* Unit : Fixture)
	{
		if (Unit->SavedAI) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
		Unit->GetCharacterMovement()->StopMovementImmediately();
		Unit->SetActorEnableCollision(true);
	}
	auto Place = [](ABattleUnit* Unit, const FVector& Position) { Unit->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics); };
	Place(A, FVector(-2000, -2100, 95)); Place(B, FVector(-2500, -2300, 95)); Place(C, FVector(-3000, -2300, 95));
	Place(Enemy, FVector(-1000, -2100, 95)); A->SetActorRotation(FRotator::ZeroRotator);
	FBattlePerceptionProfile Profile;
	Profile.SightRange = 1500.f; Profile.FieldOfView = 90.f; Profile.DetectionSeconds = .5f;
	Profile.MemorySeconds = 2.f; Profile.ShareRange = 700.f; Profile.ShareDelay = .5f;
	Store->SetPerceptionProfile(A->GetUnitID(), Profile);
	FBattlePerceptionProfile Blind = Profile; Blind.SightRange = 0.f; Blind.MemorySeconds = 3.f;
	Store->SetPerceptionProfile(B->GetUnitID(), Blind); Store->SetPerceptionProfile(C->GetUnitID(), Blind);
	Blind.ShareRange = 0.f; Store->SetPerceptionProfile(Enemy->GetUnitID(), Blind);
	CheckSmoke(A->CanSee(Enemy), TEXT("Per-unit field of view detects a forward target"));
	Place(Enemy, FVector(-3000, -2100, 95));
	CheckSmoke(!A->CanSee(Enemy), TEXT("Rear target is outside directional field of view"));
	FBattlePerceptionProfile Wide = Profile; Wide.FieldOfView = 360.f; Store->SetPerceptionProfile(A->GetUnitID(), Wide);
	CheckSmoke(A->CanSee(Enemy), TEXT("Changing only field of view changes rear-target detection"));
	Store->SetPerceptionProfile(A->GetUnitID(), Profile);
	Place(Enemy, FVector(-1000, -1100, 95));
	CheckSmoke(A->CanSee(Enemy), TEXT("Target on 45-degree half-angle boundary is in view"));
	Place(Enemy, FVector(-1000, -1090, 95));
	CheckSmoke(!A->CanSee(Enemy), TEXT("Target just beyond field-of-view boundary is excluded"));
	Place(Enemy, FVector(-499, -2100, 95));
	CheckSmoke(!A->CanSee(Enemy), TEXT("Target just beyond per-unit sight range is excluded"));
	Place(Enemy, FVector(-1000, -2100, 95));
	FBattlePerceptionProfile Short = Profile; Short.SightRange = 500.f; Store->SetPerceptionProfile(A->GetUnitID(), Short);
	CheckSmoke(!A->CanSee(Enemy), TEXT("Changing only sight range changes detection"));
	Store->SetPerceptionProfile(A->GetUnitID(), Profile);

	// Deterministic sensor-clock boundary checks using real actors, geometry, and traces.
	Sense->Reset(); const double T = GetWorld()->GetTimeSeconds();
	auto Contact = [&](ABattleUnit* Observer) { return Sense->FindContact(Observer->GetUnitID(), Enemy->GetUnitID()); };
	Sense->Update(Fixture, T); Sense->Update(Fixture, T + .49);
	CheckSmoke(!Contact(A), TEXT("Enemy is not acquired before configured detection time"));
	Sense->Update(Fixture, T + .5);
	CheckSmoke(Contact(A) && Contact(A)->bVisible && Contact(A)->Confidence == 1.f, TEXT("Enemy is acquired at detection-time boundary"));
	TSet<int32> TeamVisible; Sense->CollectVisibleTargets(Fixture, A->GetState()->Team, TeamVisible);
	CheckSmoke(TeamVisible.Contains(Enemy->GetUnitID()), TEXT("Team display projection includes a living member's current sighting"));
	CheckSmoke(!Contact(B), TEXT("Squadmate does not receive newly seen enemy instantly"));
	const FVector SeenPosition = Enemy->GetActorLocation();
	const int32 Revision = A->GetState()->PerceptionRevision;
	ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	PC->ControlUnit(A); PC->EnterTactical();
	CheckSmoke(A->GetState()->PerceptionRevision == Revision && Contact(A) && Contact(A)->LastKnownPosition == SeenPosition,
		TEXT("Perception ability and contact memory survive FPS ownership changes"));
	Place(Enemy, FVector(-3000, -2100, 95));
	Sense->Update(Fixture, T + .6);
	CheckSmoke(Contact(A) && !Contact(A)->bVisible && Contact(A)->LastKnownPosition == SeenPosition && Contact(A)->Confidence < 1.f,
		TEXT("Lost enemy retains last observation with decaying confidence, not live position"));
	Sense->Update(Fixture, T + .99);
	CheckSmoke(!Contact(B), TEXT("Shared report remains unavailable before transmission delay"));
	Sense->Update(Fixture, T + 1.0);
	CheckSmoke(Contact(B) && Contact(B)->bShared && !Contact(B)->bVisible && Contact(B)->SourceUnitID == A->GetUnitID() && Contact(B)->LastKnownPosition == SeenPosition,
		TEXT("Delayed shared contact contains original source and observation snapshot"));
	Sense->CollectVisibleTargets(Fixture, A->GetState()->Team, TeamVisible);
	CheckSmoke(!TeamVisible.Contains(Enemy->GetUnitID()), TEXT("Team display projection excludes both memories and shared reports"));
	Sense->Update(Fixture, T + 1.6);
	CheckSmoke(!Contact(C), TEXT("Shared memories are not automatically relayed beyond sender range"));
	Sense->Update(Fixture, T + 2.51);
	CheckSmoke(!Contact(A) && Contact(B), TEXT("Different memory durations expire independently using observation age"));
	Sense->Update(Fixture, T + 3.51);
	CheckSmoke(!Contact(B), TEXT("Shared memory expires without refreshing its timestamp on receipt"));

	Sense->Reset(); Place(Enemy, SeenPosition);
	Sense->Update(Fixture, T + 10.); Sense->Update(Fixture, T + 10.3);
	Place(Enemy, FVector(-3000, -2100, 95)); Sense->Update(Fixture, T + 10.4);
	Place(Enemy, SeenPosition); Sense->Update(Fixture, T + 10.5); Sense->Update(Fixture, T + 10.9);
	CheckSmoke(!Contact(A), TEXT("Interrupted glimpse resets detection timer"));
	Sense->Update(Fixture, T + 11.);
	CheckSmoke(Contact(A) && Contact(A)->bVisible, TEXT("Continuous reacquisition completes after a fresh delay"));
	AStaticMeshActor* Cover = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-1500, -2100, 160), FRotator::ZeroRotator);
	Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Cover->SetActorScale3D(FVector(1, 4, 3.2)); Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Sense->Update(Fixture, T + 11.1);
	CheckSmoke(Contact(A) && !Contact(A)->bVisible && !A->CanSee(Enemy), TEXT("New cover immediately removes visible status while retaining memory"));
	Cover->Destroy();

	Sense->Reset(); Place(B, FVector(-3200, -2500, 95));
	Sense->Update(Fixture, T + 20.); Sense->Update(Fixture, T + 20.5); Sense->Update(Fixture, T + 21.1);
	CheckSmoke(Contact(A) && !Contact(B), TEXT("Ally outside sharing range receives no report"));
	Place(B, FVector(-2500, -2300, 95)); Sense->Update(Fixture, T + 22.);
	Place(B, FVector(-3200, -2500, 95)); Sense->Update(Fixture, T + 22.6);
	CheckSmoke(!Contact(B), TEXT("Report is cancelled if receiver leaves range before delivery"));
	Place(B, FVector(-2500, -2300, 95)); Sense->Update(Fixture, T + 23.); Sense->Update(Fixture, T + 23.5);
	CheckSmoke(Contact(B) && Contact(B)->bShared && Contact(B)->LastSeenTime >= T + 23.,
		TEXT("Returning receiver can receive a fresh report after an earlier transmission was cancelled"));

	// Runtime capability changes must not inherit partially elapsed acquisition.
	Sense->Reset(); Place(B, FVector(-2500, -2300, 95)); Sense->Update(Fixture, T + 30.);
	Sense->Update(Fixture, T + 30.5);
	Store->SetPerceptionProfile(A->GetUnitID(), Short); Sense->Update(Fixture, T + 31.1);
	CheckSmoke(!Contact(A) && !Contact(B), TEXT("Capability revision clears old contacts and cancels pending outgoing reports"));
	Store->SetPerceptionProfile(A->GetUnitID(), Profile);
	Sense->Reset();
	for (int32 Step = 0; Step < 24; ++Step) Sense->Update(Fixture, T + 35. + Step * .025, (A->GetUnitID() + Step) % 8, 8);
	CheckSmoke(!Contact(A), TEXT("Eight sensor slices do not bypass the configured acquisition delay"));
	Sense->Update(Fixture, T + 35.6, A->GetUnitID() % 8, 8);
	CheckSmoke(Contact(A) && Contact(A)->bVisible, TEXT("Eight sensor slices still revisit each observer every 0.2 seconds"));
	Sense->Reset(); Sense->Update(Fixture, T + 40.); Sense->Update(Fixture, T + 40.5);
	Place(Enemy, FVector(-3000, -2100, 95)); Store->ApplyDamage(Enemy->GetUnitID(), 0, 10000.f); Enemy->ApplyDeath();
	Sense->Update(Fixture, T + 40.6);
	CheckSmoke(Contact(A) && !Contact(A)->bVisible && Contact(A)->LastKnownPosition == SeenPosition,
		TEXT("Unseen enemy death does not reveal new information or erase remembered location"));
	Sense->Reset();
	// Also prove delayed acquisition gates actual AI shots and tactical rendering on world ticks.
	ABattleUnit* LiveEnemy = Units[16];
	LiveEnemy->SetActorEnableCollision(true);
	Place(A, FVector(-2200, -2100, 95)); Place(LiveEnemy, FVector(-1000, -2100, 95));
	A->SetActorRotation(FRotator::ZeroRotator);
	Profile.DetectionSeconds = 1.f; Profile.ShareRange = 0.f;
	Store->SetPerceptionProfile(A->GetUnitID(), Profile);
	Store->SetPerceptionProfile(LiveEnemy->GetUnitID(), Blind);
	Store->SetOrder(A->GetUnitID(), EBattleOrder::Hold, A->GetActorLocation());
	Cast<ABattleAIController>(A->SavedAI)->BeginOrder(); A->SavedAI->SetActorTickEnabled(true);
	bCombatPaused = false; RefreshVisibility();
	const int32 AmmoBefore = A->GetState()->Ammo;
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, A, LiveEnemy, AmmoBefore]()
	{
		CheckSmoke(A->GetState()->Ammo == AmmoBefore && !LiveEnemy->bRevealed, TEXT("Before detection delay actual AI does not fire and enemy stays hidden"));
		FTimerHandle AcquiredTimer;
		GetWorldTimerManager().SetTimer(AcquiredTimer, FTimerDelegate::CreateWeakLambda(this, [this, A, LiveEnemy, AmmoBefore]()
		{
			CheckSmoke(A->GetState()->Ammo < AmmoBefore && LiveEnemy->bRevealed, TEXT("After detection delay actual AI fires and tactical enemy becomes visible"));
			LiveEnemy->SetActorLocation(FVector(-3200, -2100, 95), false, nullptr, ETeleportType::TeleportPhysics);
			RefreshVisibility();
			const int32 AmmoAfter = A->GetState()->Ammo;
			FTimerHandle LostTimer;
			GetWorldTimerManager().SetTimer(LostTimer, FTimerDelegate::CreateWeakLambda(this, [this, A, LiveEnemy, AmmoAfter]()
			{
				const auto* Remembered = GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(A->GetUnitID(), LiveEnemy->GetUnitID());
				CheckSmoke(A->GetState()->Ammo == AmmoAfter && !LiveEnemy->bRevealed && Remembered && !Remembered->bVisible,
					TEXT("Remembered target outside vision is hidden and cannot drive AI fire"));
				RunTacticsSmokeTest();
			}), .7f, false);
		}), 1.2f, false);
	}), .5f, false);
}
