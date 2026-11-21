#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleLeaderRuntime.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleNavigation.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "BattleCover.h"
#include "BattleSquadAI.h"
#include "BattleMetrics.h"
#include "Navigation/PathFollowingComponent.h"
#include "FutureBattle.h"

ABattleAIController::ABattleAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .05f;
	bAttachToPawn = true;
}

void ABattleAIController::ResetExecution()
{
	ClearCoverPlan();
	NextTacticalDecision = TacticalCommitUntil = TacticalBlockedUntil = 0.;
	NextDecision = NextMoveRequest = 0.;
	LastProgressTime = GetWorld()->GetTimeSeconds();
	bTrackingMove = bRelaxed = bFailed = false;
	if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
	{
		ActiveRevision = Unit->GetState() ? Unit->GetState()->OrderRevision : INDEX_NONE;
		ActiveCoordinationRevision = Unit->GetState() ? Unit->GetState()->CoordinationRevision : INDEX_NONE;
		ActiveDecisionRevision = Unit->GetState() ? Unit->GetState()->DecisionRevision : INDEX_NONE;
		GetWorld()->GetSubsystem<UBattleState>()->SetMoveRetries(Unit->GetUnitID(), 0);
	}
}

void ABattleAIController::OnUnPossess()
{
	if (const auto* Unit = Cast<ABattleUnit>(GetPawn())) GetWorld()->GetSubsystem<UBattleSquadAI>()->ReleaseUnit(Unit->GetUnitID());
	ClearCoverPlan();
	Super::OnUnPossess();
}

void ABattleAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ResetExecution();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void ABattleAIController::BeginOrder()
{
 if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled()) return;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	ResetExecution();
	ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn());
	if (!Unit || !Unit->IsAlive()) return;
	if (Unit->GetState()->Order == EBattleOrder::Hold)
		GetWorld()->GetSubsystem<UBattleState>()->SetAction(Unit->GetUnitID(), EBattleAction::Idle);
	else ExecuteMove(Unit, Unit->GetState()->Destination, EBattleAction::Moving, GetWorld()->GetTimeSeconds());
}

void ABattleAIController::SuspendMovement()
{
	StopMovement();
	bTrackingMove = false;
	LastProgressTime = GetWorld()->GetTimeSeconds();
}

void ABattleAIController::PrepareControlHandoff()
{
	StopMovement(); ClearFocus(EAIFocusPriority::Gameplay); ResetExecution();
}

void ABattleAIController::RetryMove(ABattleUnit* Unit, double Now, FVector Goal, FName Reason)
{
	StopMovement();
	bTrackingMove = false;
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	const int32 Retries = Unit->GetState()->MoveRetries;
	GetWorld()->GetSubsystem<UBattleMetrics>()->RecordMoveRetry(Unit, Goal, Reason, Retries >= FMath::Max(0, GetDefault<UBattleTuning>()->MaxMoveRetries));
	if (Retries >= FMath::Max(0, GetDefault<UBattleTuning>()->MaxMoveRetries))
	{
		bFailed = true;
		if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->MoveFailures;
		Store->SetAction(Unit->GetUnitID(), EBattleAction::MoveFailed);
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_MOVE_FAILED Unit=%d Squad=%d Retries=%d"), Unit->GetUnitID(), Unit->GetState()->SquadID, Retries);
		if (Unit->GetState()->Team == 0)
			if (ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController()))
				PC->ShowNotice(FString::Printf(TEXT("SQUAD %02d / UNIT %02d: MOVE FAILED - issue a new order"), Unit->GetState()->SquadID, Unit->GetUnitID()));
		return;
	}
	Store->SetMoveRetries(Unit->GetUnitID(), Retries + 1);
	// First retry keeps the member's slot. Subsequent retries use the squad center.
	bRelaxed = Retries >= 1;
	NextMoveRequest = Now + .3;
	LastProgressTime = Now;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_MOVE_RETRY Unit=%d Attempt=%d Relaxed=%d"), Unit->GetUnitID(), Retries + 1, bRelaxed);
}

void ABattleAIController::ExecuteMove(ABattleUnit* Unit, FVector Goal, EBattleAction Action, double Now)
{
	if (bFailed) return;
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	if (bRelaxed && CurrentCoverSlot == INDEX_NONE && Unit->GetState()->Role != EBattleRole::Flank)
	{
		if (const auto* Squad = Store->FindSquad(Unit->GetState()->SquadID))
		{
			if (FVector::Dist2D(Unit->GetActorLocation(), Squad->Destination) > 100.f)
			{ Goal = Squad->Destination; Action = EBattleAction::Regrouping; }
			else bRelaxed = false; // At the shared waypoint; re-form at the member goal.
		}
	}
	const bool bCoverMove = CurrentCoverSlot != INDEX_NONE;
	const float ArrivalRadius = bCoverMove ? 25.f : 90.f;
	if (FVector::DistSquared2D(Unit->GetActorLocation(), Goal) < FMath::Square(ArrivalRadius))
	{
		if (bTrackingMove) if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->MoveArrivals;
		StopMovement(); bTrackingMove = false;
		Store->SetAction(Unit->GetUnitID(), EBattleAction::Idle);
		return;
	}
	if (bTrackingMove && !ExecutionGoal.Equals(Goal, 20.f)) { StopMovement(); bTrackingMove = false; }
	if (bTrackingMove)
	{
		Store->SetAction(Unit->GetUnitID(), Action);
		if (FVector::DistSquared2D(Unit->GetActorLocation(), LastProgressPosition) >= FMath::Square(35.f))
		{ LastProgressPosition = Unit->GetActorLocation(); LastProgressTime = Now; }
		if (Now - LastProgressTime >= FMath::Max(.5f, GetDefault<UBattleTuning>()->MoveStuckSeconds))
		{ RetryMove(Unit, Now, Goal, TEXT("NoProgress")); return; }
		if (GetMoveStatus() != EPathFollowingStatus::Idle) return;
		// A path ended before arrival (blocked/aborted); retry with the same bounded budget.
		RetryMove(Unit, Now, Goal, TEXT("PathEnded")); return;
	}
	if (Now < NextMoveRequest) return;
	FVector Resolved;
	if (!BattleNavigation::ResolveGoal(Unit, Goal, Resolved) || !BattleNavigation::IsPathWithinDefense(Unit, Resolved))
	{ RetryMove(Unit, Now, Goal, TEXT("GoalOrPathInvalid")); return; }
	// UE adds the capsule radius (36) to acceptance; keep the total below our 90cm arrival threshold.
	// Cover benefit depends on close placement: do not add capsule radius to its stop distance.
	const auto Result = MoveToLocation(Resolved, bCoverMove ? 15.f : 40.f, !bCoverMove, true, false, true, nullptr, false);
	if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->MoveRequests;
	if (Result == EPathFollowingRequestResult::Failed) { RetryMove(Unit, Now, Goal, TEXT("RequestRejected")); return; }
	ExecutionGoal = Goal;
	LastProgressPosition = Unit->GetActorLocation();
	LastProgressTime = Now;
	bTrackingMove = true;
	Store->SetAction(Unit->GetUnitID(), Action);
}

void ABattleAIController::Tick(float DeltaTime)
{
 if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled()) return;
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleIndividual);
	Super::Tick(DeltaTime);
	ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn());
	ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Unit || !Unit->IsAlive() || !Mode) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Mode->bCombatPaused || Mode->bBattleEnded || Now < Mode->CombatStartsAt)
	{ LastProgressTime = Now; return; }
	auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>();
	FBattleScopedMetric Timing(Metrics, EBattleMetricPhase::Individual);
	const FBattleUnitState* S = Unit->GetState();
	if (ActiveRevision != S->OrderRevision || ActiveDecisionRevision != S->DecisionRevision) { StopMovement(); ResetExecution(); }
	else if (ActiveCoordinationRevision != S->CoordinationRevision)
	{
		const double Scheduled = NextDecision;
		StopMovement(); ResetExecution();
		// Cancel obsolete movement immediately, then keep the member's regular decision phase.
		if (GetDefault<UBattleTuning>()->bStaggerDecisions) NextDecision = Scheduled;
	}
	if (bFailed && S->Role == EBattleRole::Flank)
	{ GetWorld()->GetSubsystem<UBattleSquadAI>()->ReportFailure(S->UnitID, Now); ResetExecution(); }
	if (Now < NextDecision) return;
	// Shift half of due decisions off a visibility frame, at most once per decision.
	if (Mode->bSplitDecisionLoad && NextDecision > 0. && (S->UnitID & 1) && Mode->IsVisibilityFrame(Now))
	{
		NextDecision = -1.;
		return;
	}
	const double ThinkInterval = S->Decision.ThinkSeconds;
	const double Phase = ThinkInterval * ((S->UnitID * 37) % 16) / 16.;
	NextDecision = GetDefault<UBattleTuning>()->bStaggerDecisions ? (FMath::FloorToDouble((Now - Phase) / ThinkInterval) + 1.) * ThinkInterval + Phase : Now + ThinkInterval;
	Metrics->CountDecision();
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto* Squad = Store->FindSquad(S->SquadID);
	const bool bOutsideDefense = S->Order == EBattleOrder::Defend && Squad &&
		FVector::DistSquared2D(Unit->GetActorLocation(), Squad->Destination) > FMath::Square(Squad->DefenseRadius - 60.f);

	// Perception gates every enemy-dependent decision; there is no unseen pursuit.
	ABattleUnit* Target = nullptr;
	const FBattleContact* TacticalThreat = nullptr;
	bool bObservedEngagement=false;
	FVector KnownTargetPosition = FVector::ZeroVector;
	float Best = FMath::Square(GetDefault<UBattleTuning>()->EffectiveWeaponRange());
	const auto* Contacts = GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(Unit->GetUnitID());
	const FBattleSightContext Sight = Unit->CaptureSightContext();
	if (Contacts) for (const auto& Pair : *Contacts)
	{
		const auto& Contact = Pair.Value;
		if (!Contact.bVisible) continue;
		ABattleUnit* Candidate = Mode->FindUnitByID(Contact.TargetID);
		if (!BattleTactics::ObservedTactics()&&(!Candidate || !Unit->CanSeeSnapshot(Sight, Candidate, Candidate->GetActorLocation(), Candidate->IsAlive()))) continue;
		if (S->Order == EBattleOrder::Defend && Squad &&
			FVector::DistSquared2D(Contact.LastKnownPosition, Squad->Destination) > FMath::Square(Squad->DefenseRadius)) continue;
		float Distance = FVector::DistSquared(Unit->GetActorLocation(), Contact.LastKnownPosition);
		if (Distance > FMath::Square(GetDefault<UBattleTuning>()->EffectiveWeaponRange())) continue;
		if (Contact.TargetID == S->CoordinatedTarget) Distance *= .25f;
		if (Distance < Best) { Best = Distance; Target = Candidate;if(BattleTactics::ObservedTactics())bObservedEngagement=true; KnownTargetPosition = Contact.LastKnownPosition; TacticalThreat = &Contact; }
	}
	if (!TacticalThreat && Contacts)
		for (const auto& Pair : *Contacts) if (Pair.Value.Confidence > .35f && (!TacticalThreat || Pair.Value.LastSeenTime > TacticalThreat->LastSeenTime)) TacticalThreat = &Pair.Value;
	bool bSquadMovement = S->Role == EBattleRole::Flank && !bOutsideDefense;
	if (bSquadMovement)
	{
		const auto* Known = GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(S->UnitID, S->CoordinatedTarget);
		if (!Known || Known->Confidence <= .35f)
		{ GetWorld()->GetSubsystem<UBattleSquadAI>()->ReleaseUnit(S->UnitID); ResetExecution(); bSquadMovement = false; }
		else
		{
			if (CurrentCoverSlot != INDEX_NONE) ClearCoverPlan();
			ExecuteMove(Unit, S->TacticalDestination, EBattleAction::Flanking, Now);
			Store->SetTacticalState(S->UnitID, INDEX_NONE, TEXT("FLANK UNDER SUPPORT FIRE"));
		}
	}
	const bool bTacticalAction = bSquadMovement || TryTacticalAction(Unit, TacticalThreat, Now, bOutsideDefense);
	const bool bEngage = (BattleTactics::ObservedTactics()?bObservedEngagement:Target!=nullptr) && (S->Order == EBattleOrder::AttackMove || S->Order == EBattleOrder::Defend) && !bOutsideDefense;
	if (!bTacticalAction && (S->Order == EBattleOrder::Hold || bEngage))
	{
		StopMovement(); bTrackingMove = false;
		if (!bFailed) Store->SetAction(Unit->GetUnitID(), bEngage ? EBattleAction::Engaging : EBattleAction::Idle);
	}
	else if (!bTacticalAction) ExecuteMove(Unit, S->Destination, bOutsideDefense ? EBattleAction::Returning : EBattleAction::Moving, Now);
	// Navigation can leave a soldier facing away from a remembered threat at arrival.
	// Turn toward the observation, then let normal perception reacquire; do not fire from memory.
	if (TacticalThreat && Unit->GetVelocity().Size2D() < 20.f && S->Action != EBattleAction::TakingCover && S->Action != EBattleAction::Flanking)
	{
		const FVector TowardKnown = TacticalThreat->LastKnownPosition - Unit->GetActorLocation();
		if (!TowardKnown.IsNearlyZero()) Unit->SetActorRotation(FRotator(0, FMath::FixedTurn(Unit->GetActorRotation().Yaw, TowardKnown.Rotation().Yaw, 90.f), 0));
	}
	if (Target && !bOutsideDefense && !bSquadMovement && S->Action != EBattleAction::TakingCover && S->Action != EBattleAction::Reloading &&
  (!BattleTactics::ObservedTactics()||Unit->CanSeeSnapshot(Sight,Target,Target->GetActorLocation(),Target->IsAlive())))
	{
		SetFocalPoint(KnownTargetPosition);
		if (S->Ammo == 0) Store->StartReload(Unit->GetUnitID(), Now);
		else Unit->FireAt(Unit->EyePosition(), KnownTargetPosition + FVector(0, 0, 35) - Unit->EyePosition(), false);
	}
	else ClearFocus(EAIFocusPriority::Gameplay);
}
