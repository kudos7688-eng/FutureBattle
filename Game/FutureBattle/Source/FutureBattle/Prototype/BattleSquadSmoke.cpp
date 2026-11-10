#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleSquadAI.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ABattleGameMode::RunSquadSmokeTest()
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Coordinator = GetWorld()->GetSubsystem<UBattleSquadAI>();
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	auto Place = [](ABattleUnit* Unit, FVector Position)
	{
		Unit->GetCharacterMovement()->StopMovementImmediately();
		Unit->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
	};
	auto Profile = [&](ABattleUnit* Unit, float Sight)
	{
		FBattlePerceptionProfile P = Unit->GetState()->Perception;
		P.SightRange = Sight; P.FieldOfView = 120; P.DetectionSeconds = 0; P.ShareRange = Sight > 0 ? 3000 : 0; P.MemorySeconds = 5;
		Store->SetPerceptionProfile(Unit->GetUnitID(), P);
	};
	auto Order = [&](EBattleOrder Command, FVector Goal)
	{
		TMap<int32, FVector> Goals;
		const int32 SquadID = SquadSmokeUnits[0]->GetState()->SquadID;
		for (int32 ID : Store->FindSquad(SquadID)->Members) if (Store->Find(ID)->IsAlive()) Goals.Add(ID, Goal);
		Store->SetSquadOrder(SquadID, Command, Goal, Goals);
		for (int32 I = 0; I < 3; ++I) Cast<ABattleAIController>(SquadSmokeUnits[I]->SavedAI)->BeginOrder();
	};
	auto FindFixture = [&](int32 ID) -> ABattleUnit*
	{
		for (ABattleUnit* Unit : SquadSmokeUnits) if (Unit->GetUnitID() == ID) return Unit;
		return nullptr;
	};
	float Delay = 1.f;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SQUAD_STAGE %d"), SquadSmokeStage);
	switch (SquadSmokeStage++)
	{
	case 0:
	{
		bCombatPaused = true;
		for (ABattleUnit* Unit : Units)
		{
			if (Unit->SavedAI) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
			Unit->SetActorEnableCollision(false); Place(Unit, FVector(12000, Unit->GetUnitID() * 250, 100)); Unit->GetCharacterMovement()->DisableMovement();
		}
		TArray<int32> IDs;
		for (int32 I = 0; I < 4; ++I)
		{
			const int32 Team = I == 3 ? 1 : 0;
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABattleUnit* Unit = GetWorld()->SpawnActor<ABattleUnit>(FVector(Team ? -1000.f : -2500.f, Team ? 1900.f : 1600.f + I * 300.f, 95), FRotator(0, Team ? 180 : 0, 0), Params);
			// Durable target isolates role transitions from a very short firefight.
			auto* Tuning = GetMutableDefault<UBattleTuning>(); const float PreviousHealth = Tuning->Health;
			if (Team) Tuning->Health = 1000.f;
			const int32 ID = Store->RegisterUnit(Team); Tuning->Health = PreviousHealth;
			Unit->InitializeUnit(ID); Units.Add(Unit); SquadSmokeUnits.Add(Unit); Profile(Unit, 4000);
			if (!Team) IDs.Add(ID); else Unit->SavedAI->SetActorTickEnabled(false);
		}
		Store->CreateSquad(0, IDs); Coordinator->Reset(); GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility();
		Order(EBattleOrder::AttackMove, FVector(-1400, 1900, 0));
		CombatStartsAt = 0.; bCombatPaused = false; Delay = 1.3f;
		break;
	}
	case 1:
	{
		int32 Supports = 0, Movers = 0; bool bFocus = true;
		for (int32 I = 0; I < 3; ++I)
		{
			const auto* S = SquadSmokeUnits[I]->GetState();
			if (S->Role == EBattleRole::Support) { if (++Supports == 1) SquadSmokeSupport = S->UnitID; }
			if (S->Role == EBattleRole::Flank) { ++Movers; SquadSmokeMover = S->UnitID; }
			bFocus &= S->CoordinatedTarget == SquadSmokeUnits[3]->GetUnitID();
		}
		CheckSmoke(Supports >= 1 && Movers >= 1 && Supports + Movers == 3, TEXT("Squad keeps support shooters while assigning a separate flanking mover"));
		CheckSmoke(bFocus, TEXT("Squad members prioritize the same personally known enemy"));
		ABattleUnit* Support = FindFixture(SquadSmokeSupport);
		if (Support) { SquadSmokePosition = Support->GetActorLocation(); SquadSmokeAmmo = Support->GetState()->Ammo; }
		Delay = 4.5f;
		break;
	}
	case 2:
	{
		ABattleUnit* Mover = FindFixture(SquadSmokeMover); ABattleUnit* Support = FindFixture(SquadSmokeSupport);
		CheckSmoke(Mover && Mover->GetState()->Role == EBattleRole::Support && Mover->GetActorLocation().X > -1500.f &&
			BattleTactics::ExposureFrom(SquadSmokeUnits[3], Mover->EyePosition()).bFlanked, TEXT("Supported unit physically reaches a side angle satisfying shared flank rules"));
		CheckSmoke(Mover && Mover->GetState()->Ammo < 30 && Mover->CanSee(SquadSmokeUnits[3]), TEXT("Arrived mover turns toward known threat and reacquires through directional vision before firing"));
		CheckSmoke(Support && Support->GetState()->Ammo < SquadSmokeAmmo && FVector::Dist2D(Support->GetActorLocation(), SquadSmokePosition) < 100.f,
			TEXT("Support keeps firing from its position while another member advances"));
		CheckSmoke(Mover && Mover->GetState()->Order == EBattleOrder::AttackMove && Mover->GetState()->Destination.Equals(FVector(-1400, 1900, 0)),
			TEXT("Flank execution preserves original squad order and destination"));
		Delay = 3.f;
		break;
	}
	case 3:
	{
		bool bRotated = false;
		for (int32 I = 0; I < 3; ++I) if (SquadSmokeUnits[I]->GetUnitID() != SquadSmokeMover && SquadSmokeUnits[I]->GetState()->Role == EBattleRole::Flank) bRotated = true;
		CheckSmoke(bRotated && FindFixture(SquadSmokeMover) && FindFixture(SquadSmokeMover)->GetState()->Role == EBattleRole::Support,
			TEXT("Arrived mover provides support as a former support member takes the next movement role"));
		for (int32 I = 0; I < 3; ++I) if (SquadSmokeUnits[I]->GetState()->Role != EBattleRole::Flank) Profile(SquadSmokeUnits[I], 0);
		GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility(); Delay = .8f;
		break;
	}
	case 4:
	{
		bool bNoFlank = true;
		for (int32 I = 0; I < 3; ++I) bNoFlank &= SquadSmokeUnits[I]->GetState()->Role != EBattleRole::Flank;
		CheckSmoke(bNoFlank, TEXT("Loss of current support vision cancels unsupported flanking movement"));
		for (int32 I = 0; I < 3; ++I) { Profile(SquadSmokeUnits[I], 4000); Place(SquadSmokeUnits[I], FVector(-2500, 1600 + I * 300, 95)); }
		Coordinator->Reset(); GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility();
		Order(EBattleOrder::AttackMove, FVector(-1400, 1900, 0)); Delay = 1.3f;
		break;
	}
	case 5:
	{
		ABattleUnit* Mover = nullptr;
		for (int32 I = 0; I < 3; ++I) if (SquadSmokeUnits[I]->GetState()->Role == EBattleRole::Flank) Mover = SquadSmokeUnits[I];
		if (Mover)
		{
			const int32 Ammo = Mover->GetState()->Ammo;
			PC->ControlUnit(Mover);
			CheckSmoke(Mover->GetState()->Role == EBattleRole::Independent && Mover->GetState()->Action == EBattleAction::DirectControl && Mover->GetState()->Ammo == Ammo,
				TEXT("FPS takeover immediately cancels squad movement role without changing ammunition"));
			PC->EnterTactical();
		}
		else CheckSmoke(false, TEXT("FPS role handoff fixture has a mover"));
		Order(EBattleOrder::Hold, SquadSmokeUnits[0]->GetActorLocation());
		break;
	}
	case 6:
	{
		bool bClear = true;
		for (int32 I = 0; I < 3; ++I) bClear &= SquadSmokeUnits[I]->GetState()->Role == EBattleRole::Independent && SquadSmokeUnits[I]->GetState()->CoordinatedTarget == INDEX_NONE;
		CheckSmoke(bClear, TEXT("Hold clears all coordinated targets and roles without stale tactical movement"));
		for (int32 I = 0; I < 3; ++I) { Place(SquadSmokeUnits[I], FVector(-2500, 1600 + I * 300, 95)); SquadSmokeUnits[I]->SetActorRotation(FRotator::ZeroRotator); }
		Place(SquadSmokeUnits[3], FVector(600, 1900, 95));
		Coordinator->Reset(); GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility();
		Order(EBattleOrder::AttackMove, FVector(-1400, 1900, 0)); Delay = 1.3f;
		break;
	}
	case 7:
	{
		bool bBoundedAdvance = false;
		for (int32 I = 0; I < 3; ++I) if (SquadSmokeUnits[I]->GetState()->Role == EBattleRole::Flank)
		{
			SquadSmokeMover = SquadSmokeUnits[I]->GetUnitID(); SquadSmokeRevision = SquadSmokeUnits[I]->GetState()->CoordinationRevision;
			const float Travel = FVector::Dist2D(SquadSmokeUnits[I]->GetState()->TacticalDestination, FVector(-2500, 1600 + I * 300, 95));
			// Initial attack movement advances a few centimeters before the first support decision.
			bBoundedAdvance = Travel > 1000.f && Travel <= 1825.f;
		}
		CheckSmoke(bBoundedAdvance, TEXT("Distant engagement uses a bounded lateral advance instead of rejecting all flank plans"));
		Place(SquadSmokeUnits[3], FVector(-1000, 1400, 95)); Delay = 1.6f;
		break;
	}
	case 8:
	{
		ABattleUnit* Mover = FindFixture(SquadSmokeMover);
		CheckSmoke(Mover && Mover->GetState()->CoordinationRevision > SquadSmokeRevision, TEXT("Observed enemy displacement invalidates the old squad movement plan"));
		ABattleUnit* Victim = SquadSmokeUnits[0];
		Store->ApplyDamage(Victim->GetUnitID(), 1, 10000); Victim->ApplyDeath();
		CheckSmoke(Victim->GetState()->Role == EBattleRole::Independent && Victim->GetState()->CoordinatedTarget == INDEX_NONE, TEXT("Death removes coordinated role and target immediately"));
		for (int32 I = 1; I < 3; ++I) Profile(SquadSmokeUnits[I], 0);
		GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); Delay = .8f;
		break;
	}
	case 9:
	{
		CheckSmoke(SquadSmokeUnits[1]->GetState()->Role == EBattleRole::Independent && SquadSmokeUnits[2]->GetState()->Role == EBattleRole::Independent,
			TEXT("Remaining members drop the old plan after casualty and loss of target knowledge"));
		for (int32 I = 1; I < 3; ++I)
		{ Profile(SquadSmokeUnits[I], 4000); Place(SquadSmokeUnits[I], FVector(-2300, 1750 + I * 100, 95)); SquadSmokeUnits[I]->SetActorRotation(FRotator::ZeroRotator); }
		Place(SquadSmokeUnits[3], FVector(-1600, 1900, 95));
		Coordinator->Reset(); GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility();
		Order(EBattleOrder::Defend, FVector(-2000, 1900, 0)); Delay = 1.3f;
		break;
	}
	case 10:
	{
		ABattleUnit* Mover = nullptr; bool bBounded = true;
		for (int32 I = 1; I < 3; ++I)
		{
			const auto* S = SquadSmokeUnits[I]->GetState();
			if (S->Role == EBattleRole::Flank) { Mover = SquadSmokeUnits[I]; bBounded &= FVector::Dist2D(S->TacticalDestination, FVector(-2000, 1900, 0)) <= 501.f; }
			bBounded &= FVector::Dist2D(SquadSmokeUnits[I]->GetActorLocation(), FVector(-2000, 1900, 0)) <= 600.f;
		}
		CheckSmoke(Mover && bBounded, TEXT("Coordinated defender constrains flank goal and real movement to defense radius"));
		if (Mover)
		{
			SquadSmokeMover = Mover->GetUnitID();
			// Simulate a tactical path becoming unavailable after the squad accepted its plan.
			Store->SetCoordination(Mover->GetUnitID(), EBattleRole::Flank, SquadSmokeUnits[3]->GetUnitID(), FVector(1000000, 1000000, 0));
		}
		Delay = 3.f;
		break;
	}
	case 11:
	{
		ABattleUnit* Mover = FindFixture(SquadSmokeMover);
		CheckSmoke(Mover && Mover->GetState()->Role == EBattleRole::Independent && Mover->GetState()->Order == EBattleOrder::Defend && Mover->GetState()->Action != EBattleAction::Flanking,
			TEXT("Failed flank path returns to independent defense instead of endlessly reassigning movement"));
		FString Suite; FParse::Value(FCommandLine::Get(), TEXT("BattleSmokeSuite="), Suite);
		if (Suite == TEXT("Squad")) CompleteSmokeTest(); else RunAbilitySmokeTest();
		return;
	}
	}
	FTimerHandle Timer; GetWorldTimerManager().SetTimer(Timer, this, &ABattleGameMode::RunSquadSmokeTest, Delay, false);
}
