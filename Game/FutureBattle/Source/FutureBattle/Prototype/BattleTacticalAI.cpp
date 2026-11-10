#include "BattleUnit.h"
#include "BattleState.h"
#include "BattlePerception.h"
#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleNavigation.h"
#include "FutureBattle.h"
#include "BattleMetrics.h"

void ABattleAIController::ClearCoverPlan()
{
	if (const auto* Unit = Cast<ABattleUnit>(GetPawn()))
	{
		GetWorld()->GetSubsystem<UBattleCover>()->Release(Unit->GetUnitID());
		GetWorld()->GetSubsystem<UBattleState>()->SetTacticalState(Unit->GetUnitID(), INDEX_NONE, TEXT(""));
	}
	CurrentCoverSlot = INDEX_NONE; bCoverForReload = false;
}

bool ABattleAIController::TryTacticalAction(ABattleUnit* Unit, const FBattleContact* Threat, double Now, bool bMustReturn)
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
			if (BattleNavigation::ResolveGoal(Unit, Cover->GetSlots()[Candidate.Key], Resolved) && BattleNavigation::IsPathWithinDefense(Unit, Resolved))
			{ BestSlot = Candidate.Key; BestScore = Candidate.Value; break; }
		}
		if (BestSlot != INDEX_NONE && BestSlot != CurrentCoverSlot && Cover->Reserve(BestSlot, Unit->GetUnitID(), Now))
		{
			SuspendMovement(); CurrentCoverSlot = BestSlot;
			TacticalCommitUntil = Now + FMath::Max(.5f, Tuning->TacticalCommitSeconds);
			bRelaxed = false; NextMoveRequest = 0.;
			Store->SetMoveRetries(Unit->GetUnitID(), 0);
			Store->SetTacticalState(Unit->GetUnitID(), BestSlot, bNeedsReload ? TEXT("SEEK SAFE RELOAD") : TEXT("SEEK COVER + FIRE"));
			UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_TACTIC Unit=%d Slot=%d Goal=%s Score=%.2f Current=%.2f Reload=%d Threat=%s"), Unit->GetUnitID(), BestSlot, *Cover->GetSlots()[BestSlot].ToCompactString(), BestScore, CurrentScore, bNeedsReload, *Threat->LastKnownPosition.ToCompactString());
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
			UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_COVER_ARRIVAL Unit=%d Slot=%d Pos=%s Exposure=%d"), Unit->GetUnitID(), CurrentCoverSlot, *Unit->GetActorLocation().ToCompactString(), BattleTactics::ExposureFrom(Unit, ThreatEye).VisibleSamples);
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
