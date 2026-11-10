#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleSquadAI.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ABattleGameMode::RunAbilitySmokeTest()
{
	bCombatPaused = true;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>();
	auto Place = [](ABattleUnit* Unit, FVector P) { Unit->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics); };
	for (ABattleUnit* Unit : Units)
	{
		if (Unit->SavedAI) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
		Unit->SetActorEnableCollision(false); Place(Unit, FVector(14000, Unit->GetUnitID() * 250, 100)); Unit->GetCharacterMovement()->DisableMovement();
	}
	TArray<TObjectPtr<ABattleUnit>> Fixture;
	TArray<int32> Members;
	for (int32 I = 0; I < 4; ++I)
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Unit = GetWorld()->SpawnActor<ABattleUnit>(FVector(I == 3 ? -800 : -2300, 2000, 95), FRotator::ZeroRotator, Params);
		auto* Tuning = GetMutableDefault<UBattleTuning>(); const float PreviousHealth = Tuning->Health;
		if (I == 3) Tuning->Health = 1000;
		const int32 ID = Store->RegisterUnit(I == 3 ? 1 : 0); Tuning->Health = PreviousHealth;
		Unit->InitializeUnit(ID); Unit->SavedAI->SetActorTickEnabled(false); Unit->SetActorEnableCollision(I == 3);
		Fixture.Add(Unit); Units.Add(Unit);
		if (I < 3) { Members.Add(ID); Store->SetTrainingPreset(ID, static_cast<EBattleTraining>(I)); }
	}
	Store->CreateSquad(0, Members);
	ABattleUnit* Recruit = Fixture[0]; ABattleUnit* Regular = Fixture[1]; ABattleUnit* Veteran = Fixture[2]; ABattleUnit* Enemy = Fixture[3];
	const FBattleUnitState R = *Recruit->GetState(), N = *Regular->GetState(), V = *Veteran->GetState();
	CheckSmoke(R.HireCost < N.HireCost && N.HireCost < V.HireCost && R.Perception.DetectionSeconds > N.Perception.DetectionSeconds && N.Perception.DetectionSeconds > V.Perception.DetectionSeconds,
		TEXT("Training presets provide explicit provisional costs and progressively shorter acquisition delay"));
	CheckSmoke(R.Decision.ThinkSeconds > N.Decision.ThinkSeconds && N.Decision.ThinkSeconds > V.Decision.ThinkSeconds &&
		R.Decision.CoverCandidates < N.Decision.CoverCandidates && N.Decision.CoverCandidates < V.Decision.CoverCandidates && !R.Decision.bCoordinate && V.Decision.bCoordinate,
		TEXT("Decision cadence candidate budget and coordination vary independently from weapon equipment"));
	CheckSmoke(R.HP == N.HP && N.HP == V.HP && R.Ammo == V.Ammo && R.Reserve == V.Reserve && R.SpreadSkillScale == V.SpreadSkillScale,
		TEXT("Training comparison keeps health ammunition and shooting skill identical"));
	Store->SetSpreadSkill(Recruit->GetUnitID(), .75f); Store->SetTrainingPreset(Recruit->GetUnitID(), EBattleTraining::Veteran);
	CheckSmoke(Recruit->GetState()->SpreadSkillScale == .75f && Recruit->GetState()->Ammo == R.Ammo && Recruit->GetState()->HP == R.HP,
		TEXT("Changing training preserves separately configured shooting skill and current resources"));
	Store->SetSpreadSkill(Recruit->GetUnitID(), 1.f); Store->SetTrainingPreset(Recruit->GetUnitID(), EBattleTraining::Recruit);
	CheckSmoke(!Store->SetTrainingPreset(Recruit->GetUnitID(), static_cast<EBattleTraining>(255)), TEXT("Invalid training identifier cannot corrupt current capability state"));
	Place(Enemy, FVector(400, 2000, 95));
	CheckSmoke(!Recruit->CanSee(Enemy) && !Regular->CanSee(Enemy) && Veteran->CanSee(Enemy), TEXT("Long-range target is visible only to the preset whose sight range reaches it"));
	Place(Enemy, Recruit->GetActorLocation() + FRotator(0, 70, 0).Vector() * 900.f);
	CheckSmoke(!Recruit->CanSee(Enemy) && !Regular->CanSee(Enemy) && Veteran->CanSee(Enemy), TEXT("Peripheral target obeys each preset's field of view"));
	// Fixed-seed placements share geometry, equipment and update times; record direct acquisition only.
	for (int32 I = 0; I < 3; ++I)
	{ auto P = Fixture[I]->GetState()->Perception; P.ShareRange = 0; Store->SetPerceptionProfile(Fixture[I]->GetUnitID(), P); }
	FRandomStream Random(617);
	double Sum[3] = {0., 0., 0.}; bool bOrdered = true;
	const double Base = GetWorld()->GetTimeSeconds();
	for (int32 Trial = 0; Trial < 64; ++Trial)
	{
		Place(Enemy, FVector(-800, 2000 + Random.FRandRange(-180, 180), 95)); Perception->Reset();
		double First[3] = {-1., -1., -1.}; const double T = Base + Trial * 3.;
		for (int32 Step = 0; Step <= 30; ++Step)
		{
			Perception->Update(Fixture, T + Step * .05);
			for (int32 I = 0; I < 3; ++I) if (First[I] < 0.)
				if (const auto* Contact = Perception->FindContact(Fixture[I]->GetUnitID(), Enemy->GetUnitID()); Contact && Contact->bVisible) First[I] = Step * .05;
		}
		bOrdered &= First[2] >= 0. && First[2] < First[1] && First[1] < First[0];
		for (int32 I = 0; I < 3; ++I) Sum[I] += First[I];
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_ABILITY_COMPARE trials=64 seed=617 direct_acquisition_seconds recruit=%.3f regular=%.3f veteran=%.3f / equipment=equal"), Sum[0] / 64., Sum[1] / 64., Sum[2] / 64.);
	CheckSmoke(bOrdered, TEXT("64 fixed-seed equal-equipment placements reproduce preset acquisition differences"));
	Place(Enemy, FVector(-800, 2000, 95));
	auto* Wall = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-1550, 2000, 150), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->SetActorScale3D(FVector(1, 4, 3)); Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	CheckSmoke(!Veteran->CanSee(Enemy), TEXT("Highest-cost preset still cannot see through physical cover")); Wall->Destroy();
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	const FVector Goal(-1700, 2000, 0); Store->SetOrder(Recruit->GetUnitID(), EBattleOrder::AttackMove, Goal);
	Store->SetCoordination(Recruit->GetUnitID(), EBattleRole::Flank, Enemy->GetUnitID(), Goal);
	const int32 PerceptionRevision = Recruit->GetState()->PerceptionRevision;
	PC->SetSelection(Recruit); PC->CycleTrainingSelected();
	CheckSmoke(Recruit->GetState()->Training == EBattleTraining::Regular && Regular->GetState()->Training == EBattleTraining::Regular && Veteran->GetState()->Training == EBattleTraining::Regular,
		TEXT("Selecting one member cycles the training preset of its living squad"));
	CheckSmoke(Recruit->GetState()->Role == EBattleRole::Independent && Recruit->GetState()->PerceptionRevision > PerceptionRevision && Recruit->GetState()->Destination.Equals(Goal) && Recruit->GetState()->Order == EBattleOrder::AttackMove,
		TEXT("Training change invalidates old perception and roles while preserving original order"));
	PC->ControlUnit(Recruit); Store->SetTrainingPreset(Recruit->GetUnitID(), EBattleTraining::Veteran); PC->EnterTactical();
	CheckSmoke(Recruit->GetState()->Training == EBattleTraining::Veteran && Recruit->GetState()->HP == R.HP && Recruit->GetState()->Ammo == R.Ammo,
		TEXT("Training and resources survive AI/FPS ownership changes"));
	GetWorld()->GetSubsystem<UBattleSquadAI>()->Reset(); Perception->Reset();
	for (int32 I = 0; I < 3; ++I)
	{
		ABattleUnit* Unit = Fixture[I]; Store->SetTrainingPreset(Unit->GetUnitID(), static_cast<EBattleTraining>(I));
		auto P = Unit->GetState()->Perception; P.ShareRange = 0; Store->SetPerceptionProfile(Unit->GetUnitID(), P);
		Unit->SetActorEnableCollision(true); Place(Unit, FVector(-2300, 1600 + I * 200, 95)); Unit->SetActorRotation(FRotator::ZeroRotator);
		Unit->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Store->SetOrder(Unit->GetUnitID(), EBattleOrder::Hold, Unit->GetActorLocation());
		Cast<ABattleAIController>(Unit->SavedAI)->BeginOrder(); Unit->SavedAI->SetActorTickEnabled(true);
	}
	Place(Enemy, FVector(-800, 1800, 95)); RefreshVisibility(); bCombatPaused = false; CombatStartsAt = 0.;
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Recruit, Veteran]()
	{
		CheckSmoke(Veteran->GetState()->Ammo < 30 && Recruit->GetState()->Ammo == 30, TEXT("Actual veteran AI fires after acquisition while recruit is still detecting"));
		FTimerHandle Later;
		GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateWeakLambda(this, [this, Recruit]()
		{
			CheckSmoke(Recruit->GetState()->Ammo < 30, TEXT("Actual recruit AI eventually acquires and fires under the same equipment rules"));
			FString Suite; FParse::Value(FCommandLine::Get(), TEXT("BattleSmokeSuite="), Suite);
			if (Suite == TEXT("Ability")) CompleteSmokeTest(); else RunMatchSmokeTest();
		}), 1.3f, false);
	}), .7f, false);
}
