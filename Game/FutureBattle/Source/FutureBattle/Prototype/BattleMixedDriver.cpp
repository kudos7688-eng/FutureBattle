#include "BattleMixedDriver.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleCover.h"
#include "BattleSquadAI.h"
#include "BattleMetrics.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
FVector FBattleMixedUnit::GetActorLocation() const { return P->Runtime->Position(ID); }
FVector FBattleMixedUnit::GetActorForwardVector() const { return P->Runtime->Forward(ID); }
FBattleSightContext FBattleMixedUnit::CaptureSightContext() const { return P->Runtime->Sight(ID); }
bool FBattleMixedUnit::CanSeeSnapshot(const FBattleSightContext&,const FBattleMixedUnit* Other,FVector,bool) const { return P->Runtime->CanSee(ID,Other->ID); }
bool FBattleMixedUnit::FireAt(FVector Start,FVector Direction,bool Player) { return P->Runtime->Fire(ID,Start,Direction,Player); }
EPathFollowingStatus::Type FBattleMixedDriver::GetMoveStatus() const
{ return Proxy.P->Paths.Contains(Proxy.ID)?EPathFollowingStatus::Moving:EPathFollowingStatus::Idle; }
EPathFollowingRequestResult::Type FBattleMixedDriver::MoveToLocation(FVector Goal,float,bool,bool,bool,bool,void*,bool)
{ TArray<FVector> Points; if(!Proxy.P->Path(Proxy.ID,Goal,Points)) return EPathFollowingRequestResult::Failed; Proxy.P->SetPath(Proxy.ID,Points); return EPathFollowingRequestResult::RequestSuccessful; }
namespace MixedNavigation {
bool ResolveGoal(FBattleMixedUnit* Unit,FVector Goal,FVector& Out)
{ TArray<FVector> Points; if(!Unit->P->Path(Unit->ID,Goal,Points)) return false; Out=Points.Last(); return true; }
bool IsPathWithinDefense(FBattleMixedUnit* Unit,FVector Goal)
{ if(Unit->GetState()->Order!=EBattleOrder::Defend) return true; TArray<FVector> Points; return Unit->P->Path(Unit->ID,Goal,Points); }
}
void FBattleMixedDriver::ResetExecution()
{
	ClearCoverPlan();
	NextTacticalDecision = TacticalCommitUntil = TacticalBlockedUntil = 0.;
	NextDecision = NextMoveRequest = 0.;
	LastProgressTime = GetWorld()->GetTimeSeconds();
	bTrackingMove = bRelaxed = bFailed = false;
	if (FBattleMixedUnit* Unit = &this->Proxy)
	{
		ActiveRevision = Unit->GetState() ? Unit->GetState()->OrderRevision : INDEX_NONE;
		ActiveCoordinationRevision = Unit->GetState() ? Unit->GetState()->CoordinationRevision : INDEX_NONE;
		ActiveDecisionRevision = Unit->GetState() ? Unit->GetState()->DecisionRevision : INDEX_NONE;
		GetWorld()->GetSubsystem<UBattleState>()->SetMoveRetries(Unit->GetUnitID(), 0);
	}
}


void FBattleMixedDriver::SuspendMovement()
{
	StopMovement();
	bTrackingMove = false;
	LastProgressTime = GetWorld()->GetTimeSeconds();
}


void FBattleMixedDriver::RetryMove(FBattleMixedUnit* Unit, double Now, FVector Goal, FName Reason)
{
	StopMovement();
	bTrackingMove = false;
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	const int32 Retries = Unit->GetState()->MoveRetries;
	// MoveRetries and bFailed retain the original bounded retry policy.
	if (Retries >= FMath::Max(0, GetDefault<UBattleTuning>()->MaxMoveRetries))
	{
		bFailed = true;
		if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->MoveFailures;
		Store->SetAction(Unit->GetUnitID(), EBattleAction::MoveFailed);
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
}

void FBattleMixedDriver::ExecuteMove(FBattleMixedUnit* Unit, FVector Goal, EBattleAction Action, double Now)
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
	if (!MixedNavigation::ResolveGoal(Unit, Goal, Resolved) || !MixedNavigation::IsPathWithinDefense(Unit, Resolved))
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


void FBattleMixedDriver::Tick(float DeltaTime)
{

	const double TickNow=GetWorld()->GetTimeSeconds();
 if(TickNow<NextControllerTick) return; NextControllerTick=TickNow+.05;
 FPhase3Scope Timing3(Proxy.P,EPhase3Cost::Decision);

	FBattleMixedUnit* Unit = &this->Proxy;
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
 if(NextDecision>0) Proxy.P->DecisionLateMS=FMath::Max(Proxy.P->DecisionLateMS,(Now-NextDecision)*1000);
	const double ThinkInterval = S->Decision.ThinkSeconds;
	const double Phase = ThinkInterval * ((S->UnitID * 37) % 16) / 16.;
	NextDecision = GetDefault<UBattleTuning>()->bStaggerDecisions ? (FMath::FloorToDouble((Now - Phase) / ThinkInterval) + 1.) * ThinkInterval + Phase : Now + ThinkInterval;
 Metrics->CountDecision();
 ++Unit->P->Decisions;
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto* Squad = Store->FindSquad(S->SquadID);
	const bool bOutsideDefense = S->Order == EBattleOrder::Defend && Squad &&
		FVector::DistSquared2D(Unit->GetActorLocation(), Squad->Destination) > FMath::Square(Squad->DefenseRadius - 60.f);

	// Perception gates every enemy-dependent decision; there is no unseen pursuit.
	FBattleMixedUnit* Target = nullptr;
	const FBattleContact* TacticalThreat = nullptr;
	FVector KnownTargetPosition = FVector::ZeroVector;
	float Best = FMath::Square(GetDefault<UBattleTuning>()->WeaponRange);
	const auto* Contacts = GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(Unit->GetUnitID());
	const FBattleSightContext Sight = Unit->CaptureSightContext();
	if (Contacts) for (const auto& Pair : *Contacts)
	{
		const auto& Contact = Pair.Value;
		if (!Contact.bVisible) continue;
		FBattleMixedUnit* Candidate = (Unit->P->Drivers.Contains(Contact.TargetID)?&Unit->P->Drivers[Contact.TargetID]->Proxy:nullptr);
		if (!Candidate || !Unit->CanSeeSnapshot(Sight, Candidate, Candidate->GetActorLocation(), Candidate->IsAlive())) continue;
		if (S->Order == EBattleOrder::Defend && Squad &&
			FVector::DistSquared2D(Contact.LastKnownPosition, Squad->Destination) > FMath::Square(Squad->DefenseRadius)) continue;
		float Distance = FVector::DistSquared(Unit->GetActorLocation(), Contact.LastKnownPosition);
		if (Distance > FMath::Square(GetDefault<UBattleTuning>()->WeaponRange)) continue;
		if (Contact.TargetID == S->CoordinatedTarget) Distance *= .25f;
		if (Distance < Best) { Best = Distance; Target = Candidate; KnownTargetPosition = Contact.LastKnownPosition; TacticalThreat = &Contact; }
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
	const bool bEngage = Target && (S->Order == EBattleOrder::AttackMove || S->Order == EBattleOrder::Defend) && !bOutsideDefense;
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
	if (Target && !bOutsideDefense && !bSquadMovement && S->Action != EBattleAction::TakingCover && S->Action != EBattleAction::Reloading)
	{
		SetFocalPoint(KnownTargetPosition);
		if (S->Ammo == 0) Store->StartReload(Unit->GetUnitID(), Now);
		else Unit->FireAt(Unit->EyePosition(), KnownTargetPosition + FVector(0, 0, 35) - Unit->EyePosition(), false);
	}
	else ClearFocus(EAIFocusPriority::Gameplay);
}

void FBattleMixedDriver::ClearCoverPlan()
{
	if (const auto* Unit = &this->Proxy)
	{
		GetWorld()->GetSubsystem<UBattleCover>()->Release(Unit->GetUnitID());
		GetWorld()->GetSubsystem<UBattleState>()->SetTacticalState(Unit->GetUnitID(), INDEX_NONE, TEXT(""));
	}
	CurrentCoverSlot = INDEX_NONE; bCoverForReload = false;
}

bool FBattleMixedDriver::TryTacticalAction(FBattleMixedUnit* Unit, const FBattleContact* Threat, double Now, bool bMustReturn)
{
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	UBattleCover* Cover = GetWorld()->GetSubsystem<UBattleCover>();
	const auto* S = Unit->GetState();
	const auto* Tuning = GetDefault<UBattleTuning>();
	if ((S->Order != EBattleOrder::AttackMove && S->Order != EBattleOrder::Defend) || bMustReturn || !Threat)
	{
		if (CurrentCoverSlot != INDEX_NONE) { ClearCoverPlan(); SuspendMovement(); }
		return false;
	}
	if (bFailed && CurrentCoverSlot != INDEX_NONE)
	{
		ResetExecution(); TacticalBlockedUntil = Now + 5.;
		Store->SetTacticalState(Unit->GetUnitID(), INDEX_NONE, TEXT("COVER FAILED: RETURN TO ORDER"));
	}
	if (bFailed || Now < TacticalBlockedUntil) return false;
	Cover->BuildSlots();
	const bool bNeedsReload = S->ReloadEndTime > 0. || (S->Reserve > 0 && S->Ammo <= FMath::Max(1, Tuning->Capacity(S->Weapon) / 4));
	if (CurrentCoverSlot != INDEX_NONE && !Cover->Reserve(CurrentCoverSlot, Unit->GetUnitID(), Now))
	{ ClearCoverPlan(); SuspendMovement(); NextTacticalDecision = 0.; }
	const auto* Squad = Store->FindSquad(S->SquadID);
	const FVector ThreatEye = Threat->LastKnownPosition + FVector(0, 0, 60);
	auto Score = [&](FVector Position, bool bCurrent)
	{
		if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->CoverScores;
		const float Exposure = BattleTactics::ExposureAt(GetWorld(), Position, Unit->GetActorForwardVector(), ThreatEye).Fraction();
		const bool bCanShoot = BattleTactics::HasTerrainLine(GetWorld(), Position + FVector(0, 0, 60), Threat->LastKnownPosition + FVector(0, 0, 35));
		const float CoverWeight = bNeedsReload ? 8.f : 6.f;
		const float FireScore = bNeedsReload ? 0.f : bCanShoot ? 3.f * Threat->Confidence : -4.f;
		const float TravelCost = FVector::Dist2D(Position, Unit->GetActorLocation()) / FMath::Max(1.f, Tuning->CoverSearchRadius);
		const float AwayFromOrder = FMath::Max(0.f, FVector::Dist2D(Position, S->Destination) - FVector::Dist2D(Unit->GetActorLocation(), S->Destination)) / 1800.f;
		return (1.f - Exposure) * CoverWeight + FireScore - TravelCost - AwayFromOrder + (bCurrent ? .5f : 0.f);
	};
	if (Now >= NextTacticalDecision && (CurrentCoverSlot == INDEX_NONE || Now >= TacticalCommitUntil || bNeedsReload != bCoverForReload))
	{
		NextTacticalDecision = Now + S->Decision.CoverThinkSeconds;
		const float CurrentScore = Score(Unit->GetActorLocation(), true);
		float BestScore = CurrentScore + FMath::Max(.05f, Tuning->TacticalSwitchMargin);
		int32 BestSlot = INDEX_NONE;
		TArray<TPair<int32, float>> Ranked;
		for (int32 Slot : Cover->NearbySlots(Unit->GetActorLocation(), FMath::Max(100.f, Tuning->CoverSearchRadius), S->Decision.CoverCandidates))
		{
			if (!Cover->IsAvailable(Slot, Unit->GetUnitID(), Now)) continue;
			const FVector Position = Cover->GetSlots()[Slot] + FVector(0, 0, 92);
			if (S->Order == EBattleOrder::Defend && (!Squad || FVector::Dist2D(Position, Squad->Destination) > Squad->DefenseRadius - 90.f)) continue;
			const float CandidateScore = Score(Position, false);
			if (CandidateScore > BestScore) Ranked.Emplace(Slot, CandidateScore);
		}
		Ranked.Sort([](const auto& A, const auto& B) { return A.Value > B.Value; });
		for (const auto& Candidate : Ranked)
		{
			FVector Resolved;
			if (MixedNavigation::ResolveGoal(Unit, Cover->GetSlots()[Candidate.Key], Resolved) && MixedNavigation::IsPathWithinDefense(Unit, Resolved))
			{ BestSlot = Candidate.Key; BestScore = Candidate.Value; break; }
		}
		if (BestSlot != INDEX_NONE && BestSlot != CurrentCoverSlot && Cover->Reserve(BestSlot, Unit->GetUnitID(), Now))
		{
			SuspendMovement(); CurrentCoverSlot = BestSlot;
			TacticalCommitUntil = Now + FMath::Max(.5f, Tuning->TacticalCommitSeconds);
			bRelaxed = false; NextMoveRequest = 0.;
			Store->SetMoveRetries(Unit->GetUnitID(), 0);
			Store->SetTacticalState(Unit->GetUnitID(), BestSlot, bNeedsReload ? TEXT("SEEK SAFE RELOAD") : TEXT("SEEK COVER + FIRE"));
		}
		else if (CurrentCoverSlot != INDEX_NONE && !bNeedsReload && bCoverForReload)
		{
			// No firing position improves the current one after reload; resume the order.
			ClearCoverPlan(); SuspendMovement();
		}
		bCoverForReload = bNeedsReload;
	}
	if (CurrentCoverSlot != INDEX_NONE)
	{
		const FVector Goal = Cover->GetSlots()[CurrentCoverSlot];
		if (FVector::Dist2D(Unit->GetActorLocation(), Goal) > 25.f)
		{
			ExecuteMove(Unit, Goal, EBattleAction::TakingCover, Now);
			return true;
		}
		if (S->Action == EBattleAction::TakingCover)
		{
			if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->MoveArrivals;
		}
		SuspendMovement();
		if (bNeedsReload) Store->StartReload(Unit->GetUnitID(), Now);
		Store->SetAction(Unit->GetUnitID(), S->ReloadEndTime > 0. ? EBattleAction::Reloading : EBattleAction::Engaging);
		Store->SetTacticalState(Unit->GetUnitID(), CurrentCoverSlot, S->ReloadEndTime > 0. ? TEXT("RELOAD IN COVER") : TEXT("HOLD COVER / STEADY AIM"));
		return true;
	}
	if (bNeedsReload)
	{
		SuspendMovement(); Store->StartReload(Unit->GetUnitID(), Now);
		Store->SetAction(Unit->GetUnitID(), EBattleAction::Reloading);
		Store->SetTacticalState(Unit->GetUnitID(), INDEX_NONE, TEXT("RELOAD HERE: NO BETTER COVER"));
		return true;
	}
	return false;
}

bool FBattleMixedUnit::IsAIActive() const { const auto* A=P->Runtime->Actor(ID); return !A || !A->IsPlayerControlled(); }
bool FBattleMixedUnit::CanSee(const FBattleMixedUnit* Other) const { return P->Runtime->CanSee(ID,Other->ID); }
