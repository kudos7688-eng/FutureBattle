#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleCover.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ABattleGameMode::RunTacticalSmokeTest()
{
	ABattleUnit* A = Units[2]; ABattleUnit* B = Units[3]; ABattleUnit* Enemy = Units[19];
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Cover = GetWorld()->GetSubsystem<UBattleCover>();
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	auto* AI = Cast<ABattleAIController>(A->SavedAI);
	const double Now = GetWorld()->GetTimeSeconds();
	auto Place = [](ABattleUnit* Unit, FVector Position)
	{
		Unit->GetCharacterMovement()->StopMovementImmediately();
		Unit->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
	};
	auto Order = [&](EBattleOrder Command, FVector Goal)
	{
		Store->SetOrder(A->GetUnitID(), Command, Goal); AI->BeginOrder();
	};
	float Delay = 1.f;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_TACTICAL_STAGE %d Pos=%s Action=%s Slot=%d Ammo=%d Reserve=%d Reason=%s"), TacticalSmokeStage,
		*A->GetActorLocation().ToCompactString(), BattleActionName(A->GetState()->Action), A->GetState()->CoverSlot, A->GetState()->Ammo, A->GetState()->Reserve, *A->GetState()->DecisionReason);
	switch (TacticalSmokeStage++)
	{
	case 0:
	{
		bCombatPaused = true;
		for (ABattleUnit* Unit : Units)
		{
			if (Unit->SavedAI) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
			Unit->GetCharacterMovement()->StopMovementImmediately(); Unit->SetActorEnableCollision(false);
			if (Unit != A && Unit != B && Unit != Enemy)
			{
				// Hiding/collision disable does not remove a living unit from perception or RVO.
				Place(Unit, FVector(10000, Unit->GetUnitID() * 250, 100)); Unit->GetCharacterMovement()->DisableMovement();
			}
		}
		for (ABattleUnit* Unit : {A, B, Enemy})
		{
			Unit->SetActorEnableCollision(true); Unit->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			FBattlePerceptionProfile P = Unit->GetState()->Perception;
			P.SightRange = 3000; P.FieldOfView = 360; P.DetectionSeconds = 0; P.MemorySeconds = 15; P.ShareRange = 0;
			Store->SetPerceptionProfile(Unit->GetUnitID(), P);
		}
		Place(A, FVector(-2100, -2500, 95)); Place(B, FVector(-2100, -1700, 95)); Place(Enemy, FVector(-700, -2100, 95));
		auto Obstacle = [&](FVector Center, FVector Scale)
		{
			auto* Wall = GetWorld()->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->SetActorScale3D(Scale); Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		};
		Obstacle(FVector(-1600, -2100, 70), FVector(1, 4, 1.4));
		Obstacle(FVector(-1600, -2600, 150), FVector(1, 2, 3));
		Cover->BuildSlots(true);
		const auto Nearby = Cover->NearbySlots(A->GetActorLocation(), 900, 32);
		CheckSmoke(!Nearby.IsEmpty(), TEXT("Cover candidates have navigable capsule clearance near real obstacles"));
		if (!Nearby.IsEmpty())
		{
			const int32 Slot = Nearby[0];
			CheckSmoke(Cover->Reserve(Slot, A->GetUnitID(), Now) && !Cover->Reserve(Slot, B->GetUnitID(), Now), TEXT("Two living units cannot reserve the same cover location"));
			CheckSmoke(Cover->Reserve(Slot, B->GetUnitID(), Now + 3.), TEXT("Expired cover lease is reclaimable without waiting for former owner"));
			Cover->Release(A->GetUnitID()); Cover->Release(B->GetUnitID());
		}
		GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility();
		Order(EBattleOrder::AttackMove, FVector(-1000, -2100, 0));
		Store->SetOrder(B->GetUnitID(), EBattleOrder::AttackMove, FVector(-1000, -2100, 0));
		Cast<ABattleAIController>(B->SavedAI)->BeginOrder();
		AI->SetActorTickEnabled(true); B->SavedAI->SetActorTickEnabled(true); bCombatPaused = false; CombatStartsAt = 0.;
		Delay = .6f;
		break;
	}
	case 1:
	{
		const int32 SA = A->GetState()->CoverSlot, SB = B->GetState()->CoverSlot;
		CheckSmoke(SA != INDEX_NONE && SB != INDEX_NONE && SA != SB && FVector::Dist2D(Cover->GetSlots()[SA], Cover->GetSlots()[SB]) >= 140.f,
			TEXT("Concurrent attackers choose separated cover reservations during world ticks"));
		CheckSmoke(A->GetState()->Order == EBattleOrder::AttackMove && A->GetState()->Action == EBattleAction::TakingCover,
			TEXT("Cover movement preserves attack order and exposes its execution action"));
		TacticalSmokeSlot = SA; TacticalSmokeAmmo = A->GetState()->Ammo;
		Delay = .7f;
		break;
	}
	case 2:
		CheckSmoke(A->GetState()->CoverSlot == TacticalSmokeSlot && TacticalSmokeSlot != INDEX_NONE, TEXT("Cover commitment prevents rapid plan reversal while moving"));
		Delay = 1.5f;
		break;
	case 3:
	{
		CheckSmoke(A->GetState()->CoverSlot != INDEX_NONE && A->GetState()->Action == EBattleAction::Engaging && A->GetState()->Ammo < TacticalSmokeAmmo,
			TEXT("Attacker reaches cover then resumes live fire"));
		TacticalSmokeSlot = A->GetState()->CoverSlot;
		const int32 Ammo = A->GetState()->Ammo; const FVector Goal = A->GetState()->Destination;
		PC->ControlUnit(A);
		CheckSmoke(A->GetState()->CoverSlot == INDEX_NONE && Cover->Owner(TacticalSmokeSlot, Now) == INDEX_NONE && A->GetState()->Ammo == Ammo && A->GetState()->Destination.Equals(Goal),
			TEXT("FPS takeover releases reservation while preserving ammo and original order"));
		PC->EnterTactical();
		B->SavedAI->SetActorTickEnabled(false); Cast<ABattleAIController>(B->SavedAI)->StopMovement();
		const int32 BSlot = B->GetState()->CoverSlot;
		Store->ApplyDamage(B->GetUnitID(), 1, 1000); B->ApplyDeath();
		CheckSmoke(BSlot != INDEX_NONE && Cover->Owner(BSlot, Now) == INDEX_NONE, TEXT("Death immediately releases occupied cover"));
		// Consume ammunition without damaging the fixture; cooldown stays behind world time.
		double ShotTime = FMath::Max(Now, A->GetState()->NextShotTime);
		while (A->GetState()->Ammo > 5) { ShotTime += .001; Store->ConsumeShot(A->GetUnitID(), ShotTime, .001); }
		TacticalSmokeReserve = A->GetState()->Reserve;
		Delay = .6f;
		break;
	}
	case 4:
		CheckSmoke(A->GetState()->CoverSlot != INDEX_NONE && A->GetState()->Action == EBattleAction::TakingCover && A->GetState()->ReloadEndTime == 0.,
			TEXT("Low-ammo unit moves toward safer cover before beginning reload"));
		Delay = 4.5f;
		break;
	case 5:
		CheckSmoke(A->GetState()->Reserve < TacticalSmokeReserve && A->GetState()->Ammo > 15, TEXT("Safe reload completes and transfers reserve ammunition during world ticks"));
		Order(EBattleOrder::Hold, A->GetActorLocation()); TacticalSmokePosition = A->GetActorLocation();
		break;
	case 6:
		CheckSmoke(A->GetState()->CoverSlot == INDEX_NONE && FVector::Dist2D(A->GetActorLocation(), TacticalSmokePosition) < 40.f,
			TEXT("Hold cancels tactical reservation and prevents autonomous cover relocation"));
		Place(A, FVector(-2100, -2200, 95)); TacticalSmokePosition = A->GetActorLocation();
		Order(EBattleOrder::Move, FVector(-2400, -2200, 0));
		break;
	case 7:
		CheckSmoke(A->GetState()->CoverSlot == INDEX_NONE && FVector::Dist2D(A->GetActorLocation(), TacticalSmokePosition) > 150.f,
			TEXT("Plain move remains destination-first despite nearby threat and cover"));
		{
			const FVector Center(-2450, -2100, 0);
			TMap<int32, FVector> Goals;
			for (int32 ID : Store->FindSquad(A->GetState()->SquadID)->Members) if (Store->Find(ID)->IsAlive()) Goals.Add(ID, Center);
			Store->SetSquadOrder(A->GetState()->SquadID, EBattleOrder::Defend, Center, Goals); AI->BeginOrder();
		}
		break;
	case 8:
		CheckSmoke(A->GetState()->CoverSlot == INDEX_NONE && FVector::Dist2D(A->GetActorLocation(), Store->FindSquad(A->GetState()->SquadID)->Destination) <= 510.f,
			TEXT("Defender rejects attractive cover outside the permitted defense area"));
		{
			FBattlePerceptionProfile P = A->GetState()->Perception; P.SightRange = 0;
			Store->SetPerceptionProfile(A->GetUnitID(), P); GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); RefreshVisibility();
		}
		TacticalSmokeAmmo = A->GetState()->Ammo; TacticalSmokePosition = A->GetActorLocation();
		Order(EBattleOrder::AttackMove, FVector(-2100, -2100, 0)); Delay = 2.f;
		break;
	case 9:
		CheckSmoke(A->GetState()->CoverSlot == INDEX_NONE && A->GetState()->Ammo == TacticalSmokeAmmo && FVector::Dist2D(A->GetActorLocation(), TacticalSmokePosition) > 100.f,
			TEXT("Unknown enemy cannot drive cover planning or shooting despite true proximity"));
		{
			FBattlePerceptionProfile P = A->GetState()->Perception; P.SightRange = 3000;
			Store->SetPerceptionProfile(A->GetUnitID(), P); GetWorld()->GetSubsystem<UBattlePerception>()->Reset();
		}
		Place(A, FVector(-2100, -2500, 95)); RefreshVisibility();
		Order(EBattleOrder::AttackMove, FVector(-1000, -2100, 0)); Delay = .5f;
		break;
	case 10:
		CheckSmoke(A->GetState()->CoverSlot != INDEX_NONE, TEXT("Cover stall fixture has an active reserved movement plan"));
		A->GetCharacterMovement()->DisableMovement(); Delay = 11.f;
		break;
	case 11:
		CheckSmoke(A->GetState()->CoverSlot == INDEX_NONE && A->GetState()->DecisionReason.Contains(TEXT("COVER FAILED")) && A->GetState()->Order == EBattleOrder::AttackMove,
			TEXT("Blocked cover movement releases lease and falls back to original order with cooldown"));
		A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		{
			FString Suite; FParse::Value(FCommandLine::Get(), TEXT("BattleSmokeSuite="), Suite);
			if (Suite == TEXT("Tactical")) CompleteSmokeTest(); else RunSquadSmokeTest();
		}
		return;
	}
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, this, &ABattleGameMode::RunTacticalSmokeTest, Delay, false);
}
