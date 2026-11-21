#include "BattleSquadAI.h"
#include "BattleSquadTactics.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "BattleNavigation.h"
#include "FutureBattle.h"
#include "BattleMetrics.h"

void UBattleSquadAI::ReleaseUnit(int32 UnitID)
{
	GetWorld()->GetSubsystem<UBattleState>()->SetCoordination(UnitID, EBattleRole::Independent);
	for (auto& Pair : Plans) Pair.Value.Moved.Remove(UnitID);
	NextUpdate = 0.;
}

void UBattleSquadAI::ReportFailure(int32 UnitID, double Now)
{
 GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("FlankExecutionFailed"));
	ReleaseUnit(UnitID); BlockedUntil.Add(UnitID, Now + 8.);
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SQUAD_FAILED Unit=%d / independent fallback for 8 seconds"), UnitID);
}

void UBattleSquadAI::Reset()
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	for (const auto& Pair : Store->GetUnits()) Store->SetCoordination(Pair.Key, EBattleRole::Independent);
	Plans.Reset(); BlockedUntil.Reset(); NextUpdate = 0.;
}

void UBattleSquadAI::Update(const TArray<TObjectPtr<ABattleUnit>>& Units, double Now)
{
	if (Now < NextUpdate) return;
	FBattleScopedMetric Timing(GetWorld()->GetSubsystem<UBattleMetrics>(), EBattleMetricPhase::Squad);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleSquad);
	const auto* Tuning = GetDefault<UBattleTuning>();
	NextUpdate = Now + FMath::Max(.2f, Tuning->SquadDecisionSeconds);
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>();
	TMap<int32, ABattleUnit*> ByID;
	for (ABattleUnit* Unit : Units) if (Unit) ByID.Add(Unit->GetUnitID(), Unit);
	for (const auto& Pair : Store->GetSquads())
	{
		const auto& Squad = Pair.Value;
		auto Clear = [&]()
		{
			for (int32 ID : Squad.Members) Store->SetCoordination(ID, EBattleRole::Independent);
			Plans.Remove(Squad.SquadID);
		};
		if (Squad.Order != EBattleOrder::AttackMove && Squad.Order != EBattleOrder::Defend) { GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("EvalOrderIneligible")); Clear(); continue; }
		TArray<ABattleUnit*> Members;
		for (int32 ID : Squad.Members)
		{
			ABattleUnit* Unit = ByID.FindRef(ID);
			if (!Unit || !Unit->IsAlive() || !Cast<ABattleAIController>(Unit->GetController()) || !Unit->GetController()->IsActorTickEnabled() ||
				Unit->GetState()->Order != Squad.Order || !Unit->GetState()->Decision.bCoordinate || Unit->GetState()->Perception.ShareRange <= 0.f || BlockedUntil.FindRef(ID) > Now ||
				(Squad.Order == EBattleOrder::Defend && FVector::Dist2D(Unit->GetActorLocation(), Squad.Destination) > Squad.DefenseRadius - 60.f))
			{ Store->SetCoordination(ID, EBattleRole::Independent); continue; }
			Members.Add(Unit);
		}
		if (Members.Num() < 2) { GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("EvalMembersIneligible")); Clear(); continue; }
		// A common target needs at least one current observer. No actor position is used to nominate it.
		TMap<int32, int32> Votes;
		TMap<int32, FBattleContact> Latest;
		TSet<int32> Visible;
		for (ABattleUnit* Unit : Members) if (const auto* Contacts = Perception->GetContacts(Unit->GetUnitID()))
			for (const auto& ContactPair : *Contacts)
			{
				const auto& Contact = ContactPair.Value;
				if (Contact.Confidence <= .35f || (Squad.Order == EBattleOrder::Defend && FVector::Dist2D(Contact.LastKnownPosition, Squad.Destination) > Squad.DefenseRadius)) continue;
				++Votes.FindOrAdd(Contact.TargetID);
				if (Contact.bVisible) Visible.Add(Contact.TargetID);
				if (!Latest.Contains(Contact.TargetID) || Latest[Contact.TargetID].LastSeenTime < Contact.LastSeenTime) Latest.Add(Contact.TargetID, Contact);
			}
		int32 Target = INDEX_NONE, BestVotes = 1;
		for (const auto& Vote : Votes) if (Visible.Contains(Vote.Key) && (Vote.Value > BestVotes || (Vote.Value == BestVotes && (Target == INDEX_NONE || Vote.Key < Target))))
		{ Target = Vote.Key; BestVotes = Vote.Value; }
		if (const auto* Existing = Plans.Find(Squad.SquadID); Existing && Visible.Contains(Existing->TargetID) && Votes.FindRef(Existing->TargetID) >= 2)
		{
			const int32 CurrentVotes = Votes.FindRef(Existing->TargetID);
			if (Now < Existing->TargetCommittedUntil || BestVotes < CurrentVotes + 2) { Target = Existing->TargetID; BestVotes = CurrentVotes; }
		}
		if (Target == INDEX_NONE || BestVotes < 2) { GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("EvalNoCommonContact")); Clear(); continue; }
		const FVector Threat = Latest.FindChecked(Target).LastKnownPosition;
		FBattleSquadPlan& Plan = Plans.FindOrAdd(Squad.SquadID);
		if (Plan.OrderRevision != Squad.OrderRevision || Plan.TargetID != Target || (Now >= Plan.TargetCommittedUntil && FVector::Dist2D(Plan.ThreatAnchor, Threat) > 300.f))
		{
			Plan = FBattleSquadPlan(); Plan.OrderRevision = Squad.OrderRevision; Plan.TargetID = Target; Plan.ThreatAnchor = Threat;
			Plan.TargetCommittedUntil = Now + 2.;
			for (ABattleUnit* Unit : Members) { Plan.FormationAnchor += Unit->GetActorLocation(); Store->SetCoordination(Unit->GetUnitID(), EBattleRole::Independent); }
			Plan.FormationAnchor /= Members.Num();
		}
		TArray<ABattleUnit*> Eligible;
		for (ABattleUnit* Unit : Members)
		{
			const auto* Contact = Perception->FindContact(Unit->GetUnitID(), Target);
			if (!Contact || Contact->Confidence <= .35f || FVector::Dist2D(Unit->GetActorLocation(), Plan.FormationAnchor) > Unit->GetState()->Perception.ShareRange)
			{ Store->SetCoordination(Unit->GetUnitID(), EBattleRole::Independent); continue; }
			Eligible.Add(Unit);
			if (Unit->GetState()->Role == EBattleRole::Independent) Store->SetCoordination(Unit->GetUnitID(), EBattleRole::Support, Target);
		}
		if (Eligible.Num() < 2) { GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("EvalCoordinationOutOfRange")); Clear(); continue; }
		auto CanSupport = [&](ABattleUnit* Unit)
		{
			const auto* S = Unit->GetState(); const auto* Contact = Perception->FindContact(S->UnitID, Target);
			ABattleUnit* Enemy = ByID.FindRef(Target);
			return S->Role != EBattleRole::Flank && Contact && Contact->bVisible && (BattleTactics::ObservedTactics()||(Enemy && Unit->CanSee(Enemy))) &&
				S->Ammo > 0 && S->ReloadEndTime == 0. && S->Action == EBattleAction::Engaging && Unit->GetVelocity().Size2D() < 20.f &&
				S->ShotSequence > 0 && S->NextShotTime > Now - FMath::Max(.5f, Tuning->ShotInterval(S->Weapon, false));
		};
		TArray<ABattleUnit*> Movers;
		for (ABattleUnit* Unit : Eligible) if (Unit->GetState()->Role == EBattleRole::Flank) Movers.Add(Unit);
		bool bCompleted = false;
		for (ABattleUnit* Unit : Movers) if (FVector::Dist2D(Unit->GetActorLocation(), Unit->GetState()->TacticalDestination) < 90.f)
		{
			Store->SetCoordination(Unit->GetUnitID(), EBattleRole::Support, Target); bCompleted = true;
   GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("FlankArrival"));
			if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->MoveArrivals;
		}
		Movers.RemoveAll([](ABattleUnit* Unit) { return Unit->GetState()->Role != EBattleRole::Flank; });
		if (bCompleted && Movers.IsEmpty()) Plan.NextWaveAt = Now + 3.;
		bool bSupport = Eligible.ContainsByPredicate(CanSupport);
		for (ABattleUnit* Mover : Movers)
			bSupport &= Eligible.ContainsByPredicate([&](ABattleUnit* Shooter)
			{
				return CanSupport(Shooter) && FVector::Dist2D(Shooter->GetActorLocation(), Mover->GetActorLocation()) <=
					FMath::Min(Shooter->GetState()->Perception.ShareRange, Mover->GetState()->Perception.ShareRange);
			});
		if (!Movers.IsEmpty())
		{
			if (!bSupport || Now - Plan.WaveStarted > FMath::Max(3.f, Tuning->SquadWaveTimeout))
			{
    GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(bSupport ? TEXT("WaveTimedOut") : TEXT("WaveLostSupport"));
				for (ABattleUnit* Unit : Movers)
				{
					if (!bSupport) Plan.Moved.Remove(Unit->GetUnitID());
					else BlockedUntil.Add(Unit->GetUnitID(), Now + 8.);
					Store->SetCoordination(Unit->GetUnitID(), EBattleRole::Support, Target);
				}
				if (!bSupport) Plan.Waves = FMath::Max(0, Plan.Waves - 1);
				Plan.NextWaveAt = Now + 2.;
			}
			continue;
		}
		if (!bSupport || Now < Plan.NextWaveAt || Plan.Waves >= 2)
  {
   GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(!bSupport ? TEXT("EvalNoActualSupport") : Plan.Waves >= 2 ? TEXT("EvalWaveLimit") : TEXT("EvalWaveCooldown"));
   continue;
  }
		// Keep one proven shooter in place; rotate previously moved soldiers into support next wave.
		ABattleUnit* Anchor = nullptr;
		for (ABattleUnit* Unit : Eligible) if (CanSupport(Unit))
		{
			if (Unit->GetState()->Weapon == EBattleWeapon::SupportGun) { Anchor = Unit; break; }
			if (!Anchor || (Plan.Moved.Contains(Unit->GetUnitID()) && !Plan.Moved.Contains(Anchor->GetUnitID()))) Anchor = Unit;
		}
		int32 Assigned = 0;
		for (ABattleUnit* Unit : Eligible)
		{
			const auto* S = Unit->GetState();
			if (Unit == Anchor || Plan.Moved.Contains(S->UnitID) || S->Ammo <= Tuning->Capacity(S->Weapon) / 4 || S->ReloadEndTime > 0.) continue;
			// Each mover computes from its own delayed observation, never the leader's latest position.
			const FVector Known = Perception->FindContact(S->UnitID, Target)->LastKnownPosition;
			const FVector Forward = (Known - Plan.FormationAnchor).GetSafeNormal2D();
			const FVector Right(-Forward.Y, Forward.X, 0);
			bool bAssigned = false;
			for (float Sign : {1.f, -1.f})
			{
				const float Spacing = Squad.Members.IndexOfByKey(S->UnitID) * 160.f;
				FVector Goal = Known - Forward * FMath::Clamp(Tuning->SquadFlankStandoff, 150.f, 800.f) +
					Right * (Sign * (FMath::Clamp(Tuning->SquadFlankDistance, 150.f, 1200.f) + Spacing));
				if (Squad.Order == EBattleOrder::Defend)
				{
					const FVector Offset = (Goal - Squad.Destination).GetSafeNormal2D() * FMath::Min(FVector::Dist2D(Goal, Squad.Destination), Squad.DefenseRadius - 100.f);
					Goal = Squad.Destination + Offset;
				}
				FVector Resolved;
				const float CoordinationRange = FMath::Min(S->Perception.ShareRange, Anchor->GetState()->Perception.ShareRange);
				if (FVector::Dist2D(Unit->GetActorLocation(), Anchor->GetActorLocation()) > CoordinationRange) continue;
				// A distant engagement starts with a bounded lateral advance, rather than refusing every plan.
				const FVector FromAnchor = Goal - Anchor->GetActorLocation();
				if (FromAnchor.Size2D() > CoordinationRange - 100.f) Goal = Anchor->GetActorLocation() + FromAnchor.GetSafeNormal2D() * FMath::Max(0.f, CoordinationRange - 100.f);
				const FVector TravelVector = Goal - Unit->GetActorLocation();
				if (TravelVector.Size2D() > FMath::Max(500.f, Tuning->SquadFlankMaxTravel)) Goal = Unit->GetActorLocation() + TravelVector.GetSafeNormal2D() * FMath::Max(500.f, Tuning->SquadFlankMaxTravel);
				if (FVector::Dist2D(Goal, Unit->GetActorLocation()) < 150.f || !BattleNavigation::ResolveGoal(Unit, Goal, Resolved) || !BattleNavigation::IsPathWithinDefense(Unit, Resolved)) continue;
				Store->SetCoordination(S->UnitID, EBattleRole::Flank, Target, Resolved);
				Plan.Moved.Add(S->UnitID); ++Assigned; bAssigned = true;
				UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SQUAD_PLAN Squad=%d Wave=%d Unit=%d Target=%d Goal=%s"), Squad.SquadID, Plan.Waves + 1, S->UnitID, Target, *Resolved.ToCompactString());
				break;
			}
			if (bAssigned && Assigned >= FMath::Max(1, Eligible.Num() / 2)) break;
		}
		if (Assigned > 0) { GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("FlankAssigned"), Assigned); ++Plan.Waves; Plan.WaveStarted = Now; }
		else { GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("EvalNoFeasibleMover")); Plan.NextWaveAt = Now + 2.; }
	}
}
