#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleNavigation.h"
#include "FutureBattle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

void ABattleGameMode::CompleteSmokeTest()
{
	bCombatPaused = true;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SMOKE_RESULT failures=%d"), SmokeFailures);
	if (bSmokeMode) FPlatformMisc::RequestExitWithStatus(false, SmokeFailures ? 1 : 0);
	else if (ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController()))
		PC->ShowNotice(FString::Printf(TEXT("VERIFY: %d FAILURES / F5 TO RESET"), SmokeFailures));
}

void ABattleGameMode::RunOrderSmokeTest()
{
	ABattleUnit* A = Units[5];
	ABattleUnit* Enemy = Units[15];
	ABattleAIController* AI = Cast<ABattleAIController>(A->SavedAI);
	ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	if (!AI || !PC || !A->IsAlive() || !Enemy->IsAlive())
	{ CheckSmoke(false, TEXT("Order smoke fixture is alive and controllable")); CompleteSmokeTest(); return; }
	auto Place = [](ABattleUnit* Unit, FVector Position)
	{
		Unit->GetCharacterMovement()->StopMovementImmediately();
		Unit->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
	};
	auto SquadOrder = [&](EBattleOrder Order, FVector Center, FVector Goal)
	{
		TMap<int32, FVector> Goals;
		for (int32 ID : Store->FindSquad(A->GetState()->SquadID)->Members)
			if (Store->Find(ID)->IsAlive()) Goals.Add(ID, Goal);
		Store->SetSquadOrder(A->GetState()->SquadID, Order, Center, Goals);
		AI->BeginOrder();
	};
	float Delay = 1.2f;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_ORDER_STAGE %d Pos=%s Action=%s Retries=%d Goal=%s"), OrderSmokeStage, *A->GetActorLocation().ToCompactString(), BattleActionName(A->GetState()->Action), A->GetState()->MoveRetries, *A->GetState()->Destination.ToCompactString());
	switch (OrderSmokeStage++)
	{
	case 0:
	{
		// Test command semantics independently of configurable acquisition latency.
		FBattlePerceptionProfile Profile = A->GetState()->Perception;
		Profile.DetectionSeconds = 0.f; Profile.FieldOfView = 360.f;
		Store->SetPerceptionProfile(A->GetUnitID(), Profile);
		// Isolate one fresh member and one target; all tests below advance the real world.
		for (int32 Index = 0; Index < Units.Num(); ++Index)
		{
			ABattleUnit* Unit = Units[Index];
			if (Unit->SavedAI) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
			if (Unit != A && Unit != Enemy)
			{ Unit->SetActorEnableCollision(false); Place(Unit, FVector(10000, Index * 250, 100)); Unit->GetCharacterMovement()->DisableMovement(); }
		}
		AI->SetActorTickEnabled(true); CombatStartsAt = 0.; bCombatPaused = false;
		Place(A, FVector(-2200, -2100, 95)); Place(Enemy, FVector(-1400, -2100, 95));
		OrderSmokePosition = A->GetActorLocation(); OrderSmokeAmmo = A->GetState()->Ammo;
		SquadOrder(EBattleOrder::AttackMove, FVector(-800, -2100, 0), FVector(-800, -2100, 0));
		break;
	}
	case 1:
		CheckSmoke(A->GetState()->Action == EBattleAction::Engaging && FVector::Dist2D(A->GetActorLocation(), OrderSmokePosition) < 100.f && A->GetState()->Ammo < OrderSmokeAmmo,
			TEXT("Attack move stops and fires on a visible enemy during world ticks"));
		Place(Enemy, FVector(3000, 2100, 95));
		OrderSmokePosition = A->GetActorLocation();
		break;
	case 2:
		CheckSmoke(A->GetState()->Order == EBattleOrder::AttackMove && A->GetState()->Action == EBattleAction::Moving && FVector::Dist2D(A->GetActorLocation(), OrderSmokePosition) > 150.f,
			TEXT("Attack move resumes original destination after losing sight"));
		PC->SetSelection(A); PC->HoldSelected(); OrderSmokePosition = A->GetActorLocation();
		break;
	case 3:
		CheckSmoke(A->GetState()->Order == EBattleOrder::Hold && FVector::Dist2D(A->GetActorLocation(), OrderSmokePosition) < 40.f,
			TEXT("Hold cancels attack move without resuming stale movement"));
		// A path can become invalid after an accepted order; execution must report it.
		SquadOrder(EBattleOrder::Move, FVector(1000000, 1000000, 0), FVector(1000000, 1000000, 0));
		Delay = 2.5f;
		break;
	case 4:
		CheckSmoke(A->GetState()->Action == EBattleAction::MoveFailed && A->GetState()->MoveRetries == GetDefault<UBattleTuning>()->MaxMoveRetries,
			TEXT("Unreachable execution fails after bounded retries"));
		OrderSmokePosition = A->GetActorLocation();
		Store->SetOrder(A->GetUnitID(), EBattleOrder::Move, OrderSmokePosition + FVector(500, 0, -95)); AI->BeginOrder();
		break;
	case 5:
		CheckSmoke(A->GetState()->Action != EBattleAction::MoveFailed && A->GetState()->MoveRetries == 0 && FVector::Dist2D(A->GetActorLocation(), OrderSmokePosition) > 150.f,
			TEXT("New valid order clears failure and physically moves again"));
		Place(A, FVector(-2800, -2100, 95)); Place(Enemy, FVector(-1500, -2100, 95));
		SquadOrder(EBattleOrder::Defend, FVector(-1800, -2100, 0), FVector(-1800, -2100, 0));
		Delay = .6f;
		break;
	case 6:
		CheckSmoke(A->GetState()->Action == EBattleAction::Returning && A->GetActorLocation().X > -2750.f,
			TEXT("Defender outside radius returns before engaging visible enemy"));
		Delay = 2.5f;
		break;
	case 7:
		CheckSmoke(FVector::Dist2D(A->GetActorLocation(), FVector(-1800, -2100, 0)) < 600.f && A->GetState()->Action == EBattleAction::Engaging,
			TEXT("Defender engages only after returning within its radius"));
		Place(Enemy, FVector(-700, -2100, 95)); OrderSmokeAmmo = A->GetState()->Ammo;
		break;
	case 8:
		CheckSmoke(A->CanSee(Enemy) && A->GetState()->Ammo == OrderSmokeAmmo && FVector::Dist2D(A->GetActorLocation(), FVector(-1800, -2100, 0)) < 600.f,
			TEXT("Visible enemy outside defense radius is not pursued or fired upon"));
		PC->ControlUnit(A); Place(A, FVector(-2800, -2100, 95)); PC->EnterTactical();
		Delay = .6f;
		break;
	case 9:
		CheckSmoke(A->GetState()->Order == EBattleOrder::Defend && A->GetState()->Action == EBattleAction::Returning,
			TEXT("FPS release outside defense radius resumes return order"));
		Place(Enemy, FVector(3000, 2100, 95)); Place(A, FVector(-2800, -2100, 95));
		A->GetCharacterMovement()->MaxWalkSpeed = 0.f;
		SquadOrder(EBattleOrder::Move, FVector(-2000, -2100, 0), FVector(-1600, -2100, 0));
		Delay = 6.2f; // Includes retry backoff and the next decision tick before observing REGROUPING.
		break;
	case 10:
		CheckSmoke(A->GetState()->MoveRetries >= 2 && A->GetState()->Action == EBattleAction::Regrouping,
			TEXT("Stalled movement retries and relaxes formation toward squad center"));
		A->GetCharacterMovement()->MaxWalkSpeed = GetDefault<UBattleTuning>()->MoveSpeed;
		Delay = 4.f;
		break;
	case 11:
		CheckSmoke(FVector::Dist2D(A->GetActorLocation(), FVector(-1600, -2100, 0)) < 100.f && A->GetState()->Action == EBattleAction::Idle,
			TEXT("After obstruction clears member regroups then reaches its own slot"));
		Place(A, FVector(-2800, -2100, 95)); A->GetCharacterMovement()->MaxWalkSpeed = 0.f;
		SquadOrder(EBattleOrder::Move, FVector(-2000, -2100, 0), FVector(-1600, -2100, 0));
		Delay = 10.5f;
		break;
	case 12:
		CheckSmoke(A->GetState()->Action == EBattleAction::MoveFailed && A->GetState()->MoveRetries == GetDefault<UBattleTuning>()->MaxMoveRetries,
			TEXT("Persistent physical stall reports failure instead of infinite repathing"));
		Delay = 1.f;
		break;
	case 13:
	{
		CheckSmoke(A->GetState()->Action == EBattleAction::MoveFailed && A->GetState()->MoveRetries == GetDefault<UBattleTuning>()->MaxMoveRetries,
			TEXT("Failed movement remains stopped until a new order or control return"));
		A->GetCharacterMovement()->MaxWalkSpeed = GetDefault<UBattleTuning>()->MoveSpeed;
		FVector Resolved;
		const FVector EdgeCenter(-3700, -2100, 0), BlockedOffset(-500, 0, 0);
		CheckSmoke(!BattleNavigation::ResolveGoal(A, EdgeCenter + BlockedOffset, Resolved) &&
			BattleNavigation::FormationGoal(A, EdgeCenter, BlockedOffset, Resolved) && FVector::Dist2D(Resolved, EdgeCenter) < 200.f,
			TEXT("Blocked formation slot contracts to reachable ground near squad center"));
		RunPerceptionSmokeTest(); return;
	}
	default: CheckSmoke(false, TEXT("Unexpected order smoke stage")); CompleteSmokeTest(); return;
	}
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, this, &ABattleGameMode::RunOrderSmokeTest, Delay, false);
}
