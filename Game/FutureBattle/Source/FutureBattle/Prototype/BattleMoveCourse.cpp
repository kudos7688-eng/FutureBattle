#include "BattleMoveCourse.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"

static double LaneCenter(int32 Team) { return Team ? 2450. : -2450.; }

FVector UBattleMoveCourse::SpawnPosition(int32 Team, int32 Index, int32 PerTeam)
{
	const int32 Columns = PerTeam / 25;
	return FVector(LaneCenter(Team) + ((Index / 25) - (Columns - 1) * .5) * 88.,
		-420. - (Index % 25) * 88., 100.);
}

void UBattleMoveCourse::BuildGates(UWorld& World)
{
	// Authored waypoints route both backends through the same 4.4 m opening.
	// Transient blockers avoid editing or rebaking the user's map.
	for (int32 Team : {0, 1}) for (int32 Side : {-1, 1})
	{
		auto* Wall = World.SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(495, 45, 150));
		Box->SetCanEverAffectNavigation(false);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->SetWorldLocation(FVector(LaneCenter(Team) + Side * 715., 0, 150));
		Box->RegisterComponent();
	}
}

void UBattleMoveCourse::Start(ABattleGameMode& Mode, bool bUseGate)
{
	bGate = bUseGate;
	for (ABattleUnit* Unit : Mode.Units)
	{
		auto& Runner = Runners.AddDefaulted_GetRef();
		const auto* State = Unit->GetState();
		Runner.Goal = State->Destination;
		Runner.Previous = Unit->GetActorLocation();
		const double Center = LaneCenter(State->Team);
		const double X = Center + FMath::Clamp((Runner.Goal.X - Center) * .40, -165., 165.);
		Runner.Approach = FVector(X, -140, 0);
		Runner.Exit = FVector(X, 140, 0);
		Runner.Stage = bGate ? 0 : 2;
		Issue(*Unit, Runner);
	}
}

void UBattleMoveCourse::Issue(ABattleUnit& Unit, const FRunner& Runner)
{
	auto* AI = Cast<AAIController>(Unit.GetController());
	const FVector Goal = Runner.Stage == 0 ? Runner.Approach : Runner.Stage == 1 ? Runner.Exit : Runner.Goal;
	if (!AI || AI->MoveToLocation(Goal, 14.f, false, true, true, true, nullptr, false) == EPathFollowingRequestResult::Failed) ++Rejected;
}

void UBattleMoveCourse::Update(ABattleGameMode& Mode, float Dt)
{
	Elapsed += Dt;
	auto* PC = CastChecked<ABattleController>(Mode.GetWorld()->GetFirstPlayerController());
	// Exercise the handoff on an actually moving friendly, even when the queue is dense.
	if (!bFPS && Elapsed >= .45f)
	{
		for (int32 I = 0; I < Mode.Units.Num() / 2; ++I)
			if (Mode.Units[I]->GetVelocity().Size2D() > 100.) { ProbeIndex = I; break; }
	}
	auto& Probe = *Mode.Units[ProbeIndex];
	if (bFPS && !bReturned)
	{
		const double Displacement = FVector::Dist(LastProbePosition, Probe.GetActorLocation());
		bHandoffOK &= Displacement <= Probe.GetCharacterMovement()->MaxWalkSpeed * Dt + 10.;
		bPlayerMoved |= Displacement > 1.;
		Probe.AddMovementInput(FVector::RightVector, 1.f);
	}
	LastProbePosition = Probe.GetActorLocation();
	if (!bFPS && Elapsed >= .45f && Probe.GetVelocity().Size2D() > 100.)
	{
		ProbeSpeedBeforeSwitch = Probe.GetVelocity().Size2D();
		ProbeFacingError = FMath::Abs(FMath::FindDeltaAngleDegrees(Probe.GetActorRotation().Yaw, Probe.GetVelocity().Rotation().Yaw));
		const auto Before = *Probe.GetState();
		const FVector P = Probe.GetActorLocation();
		TacticalCameraPosition = PC->GetPawn()->GetActorLocation();
		bHandoffOK &= PC->ControlUnit(&Probe);
		bHandoffOK &= Probe.GetState()->UnitID == Before.UnitID && Probe.GetState()->HP == Before.HP
			&& Probe.GetState()->Ammo == Before.Ammo && Probe.GetState()->Order == Before.Order && Probe.GetState()->Destination == Before.Destination;
		MaxSwitchJump = FVector::Dist(P, Probe.GetActorLocation());
		bFPS = true;
		ReturnAt = Elapsed + .25f;
	}
	else if (bFPS && !bReturned && Elapsed >= ReturnAt)
	{
		bHandoffOK &= Probe.IsPlayerControlled() && Probe.GetCharacterMovement()->IsComponentTickEnabled();
		const auto Before = *Probe.GetState();
		ReturnPosition = Probe.GetActorLocation();
		PC->EnterTactical();
		PC->GetPawn()->SetActorLocation(TacticalCameraPosition);
		MaxSwitchJump = FMath::Max(MaxSwitchJump, FVector::Dist(ReturnPosition, Probe.GetActorLocation()));
		bHandoffOK &= Probe.GetController() == Probe.SavedAI && Probe.GetState()->HP == Before.HP
			&& Probe.GetState()->Ammo == Before.Ammo && Probe.GetState()->Order == Before.Order && Probe.GetState()->Destination == Before.Destination;
		bReturned = true;
		Issue(Probe, Runners[ProbeIndex]);
	}
	if (bReturned && FVector::Dist2D(ReturnPosition, Probe.GetActorLocation()) > 50.) bResumed = true;
	for (int32 I = 0; I < Runners.Num(); ++I)
	{
		auto& Runner = Runners[I];
		auto& Unit = *Mode.Units[I];
		const FVector Pos = Unit.GetActorLocation();
		if (bGate && !Runner.bCrossed && Runner.Previous.Y <= 0. && Pos.Y > 0.)
		{
			const double T = -Runner.Previous.Y / (Pos.Y - Runner.Previous.Y);
			const double CrossingX = FMath::Lerp(Runner.Previous.X, Pos.X, T);
			if (FMath::Abs(CrossingX - LaneCenter(Unit.GetState()->Team)) < 220.) { Runner.bCrossed = true; ++Crossed; }
		}
		Runner.Previous = Pos;
		if (Runner.Stage == 3 || Unit.IsPlayerControlled()) continue;
		const FVector Goal = Runner.Stage == 0 ? Runner.Approach : Runner.Stage == 1 ? Runner.Exit : Runner.Goal;
		Runner.DistanceLeft = FVector::Dist2D(Pos, Runner.Goal);
		const double Reach = Runner.Stage == 2 ? 90. : 55.;
		if (FVector::DistSquared2D(Pos, Goal) > FMath::Square(Reach))
		{
			// A blocked UE path ends even when its destination has not been reached.
			// Both backends get the same bounded opportunity to resume the order.
			auto* AI = Cast<AAIController>(Unit.GetController());
			if (AI && AI->GetMoveStatus() == EPathFollowingStatus::Idle && Elapsed >= Runner.NextRetry && Runner.Retries < GetDefault<UBattleTuning>()->MaxMoveRetries)
			{
				++Runner.Retries; ++RetryCount;
				Runner.NextRetry = Elapsed + 1.f;
				Issue(Unit, Runner);
			}
			continue;
		}
		Runner.Retries = 0;
		if (++Runner.Stage == 3) ++Arrived;
		else Issue(Unit, Runner);
	}
}

void UBattleMoveCourse::Report(FJsonObject& Out) const
{
	Out.SetStringField(TEXT("course"), bGate ? TEXT("gate_440cm") : TEXT("open"));
	Out.SetNumberField(TEXT("course_game_seconds"), Elapsed);
	Out.SetNumberField(TEXT("arrived"), Arrived);
	Out.SetNumberField(TEXT("course_revision"), 3);
	Out.SetNumberField(TEXT("handoff_speed_cm_s"), ProbeSpeedBeforeSwitch);
	Out.SetNumberField(TEXT("handoff_facing_velocity_angle_deg"), ProbeFacingError);
	Out.SetNumberField(TEXT("course_retries"), RetryCount);
	double DistanceSum = 0.;
	for (const FRunner& Runner : Runners) DistanceSum += Runner.DistanceLeft;
	Out.SetNumberField(TEXT("mean_distance_left_cm"), DistanceSum / Runners.Num());
	Out.SetNumberField(TEXT("crossed_gate"), Crossed);
	Out.SetNumberField(TEXT("requests_rejected"), Rejected);
	Out.SetNumberField(TEXT("switch_jump_cm"), MaxSwitchJump);
	Out.SetBoolField(TEXT("handoff_ok"), bHandoffOK && bPlayerMoved && bReturned && bResumed && MaxSwitchJump < 1.);
}
