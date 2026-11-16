#include "BattlePerception.h"
#include "BattleLeaderRuntime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleUnit.h"
#include "BattleMetrics.h"
#include "BattleTactics.h"

void UBattlePerception::Capture(TMap<int32, FBattleObserver>& OutObservers, TArray<FBattleReport>& OutReports, double Now) const
{
	OutObservers = Observers; OutReports = PendingReports;
	for (auto& Observer : OutObservers)
	{
		for (auto& Contact : Observer.Value.Contacts) Contact.Value.LastSeenTime -= Now;
		for (auto& Started : Observer.Value.DetectionStarted) Started.Value -= Now;
	}
	for (auto& Report : OutReports) { Report.DeliveryTime -= Now; Report.Contact.LastSeenTime -= Now; }
}

void UBattlePerception::Restore(const TMap<int32, FBattleObserver>& SavedObservers, const TArray<FBattleReport>& SavedReports, double Now)
{
	Observers = SavedObservers; PendingReports = SavedReports; PendingCounts.Reset();
	NextScanTimes.Reset(); LastScanTimes.Reset();
	for (auto& Observer : Observers)
	{
		for (auto& Contact : Observer.Value.Contacts) { Contact.Value.LastSeenTime += Now; Contact.Value.bVisible = false; }
		for (auto& Started : Observer.Value.DetectionStarted) Started.Value += Now;
	}
	for (auto& Report : PendingReports)
	{
		Report.DeliveryTime += Now; Report.Contact.LastSeenTime += Now;
		++PendingCounts.FindOrAdd(FIntVector(Report.Contact.SourceUnitID, Report.ReceiverID, Report.Contact.TargetID));
	}
}

const TMap<int32, FBattleContact>* UBattlePerception::GetContacts(int32 ObserverID) const
{
	const auto* Observer = Observers.Find(ObserverID);
	return Observer ? &Observer->Contacts : nullptr;
}

const FBattleContact* UBattlePerception::FindContact(int32 ObserverID, int32 TargetID) const
{
	const auto* Contacts = GetContacts(ObserverID);
	return Contacts ? Contacts->Find(TargetID) : nullptr;
}

void UBattlePerception::CollectVisibleTargets(const TArray<TObjectPtr<ABattleUnit>>& Units, int32 Team, TSet<int32>& Targets) const
{
	Targets.Reset(); Targets.Reserve(Units.Num() / 2);
	for (ABattleUnit* Observer : Units) if (Observer)
		if (const auto* State = Observer->GetState(); State && State->IsAlive() && State->Team == Team)
			if (const auto* Contacts = GetContacts(State->UnitID))
				for (const auto& Pair : *Contacts) if (Pair.Value.bVisible) Targets.Add(Pair.Key);
}

void UBattlePerception::Update(const TArray<TObjectPtr<ABattleUnit>>& Units, double Now, int32 Slice, int32 SliceCount)
{
	auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>();
	FBattleScopedMetric Timing(Metrics, EBattleMetricPhase::Perception);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattlePerception);
	if (Metrics->bRecording) ++Metrics->PerceptionUpdates;
	// Cache immutable values for this synchronous update, not between sensor ticks.
	struct FLiveUnit { ABattleUnit* Unit; const FBattleUnitState* State; FVector Position; int32 Ordinal; };
	TMap<int32, FLiveUnit> Alive;
	for (ABattleUnit* Unit : Units) if (Unit) if (const auto* State = Unit->GetState(); State && State->IsAlive())
		Alive.Add(Unit->GetUnitID(), {Unit, State, Unit->GetActorLocation(), Alive.Num()});
	auto* Leader=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const bool Mixed=Leader->IsEnabled();
 if(Mixed)
 {
  Alive.Reset(); auto* Store=GetWorld()->GetSubsystem<UBattleState>();
  for(const auto& B:Leader->GetBodies()) if(const auto* S=Store->Find(B.Key); S && S->IsAlive())
   Alive.Add(B.Key,{Leader->Actor(B.Key),S,Leader->Position(B.Key),Alive.Num()});
 }
 // Rebuild from this synchronous snapshot: moving/spawned units never use stale cells.
	constexpr double CellSize = 1200.;
	auto Cell = [](double Coordinate) { return FMath::FloorToInt(Coordinate / CellSize); };
	TMap<FIntPoint, TArray<const FLiveUnit*>> Grid;
	if (bUseSpatialGrid) for (const auto& Pair : Alive)
		Grid.FindOrAdd(FIntPoint(Cell(Pair.Value.Position.X), Cell(Pair.Value.Position.Y))).Add(&Pair.Value);
	for (auto It = Observers.CreateIterator(); It; ++It) if (!Alive.Contains(It.Key()))
	{ NextScanTimes.Remove(It.Key()); LastScanTimes.Remove(It.Key()); It.RemoveCurrent(); }
	TSet<int32> UpdatedObservers;
	{
		FBattleScopedMetric ScanTiming(Metrics, EBattleMetricPhase::SightScan);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleSightScan);
		for (const auto& Pair : Alive)
		{
			ABattleUnit* Unit = Pair.Value.Unit;
			const auto* State = Pair.Value.State;
			const auto& Profile = State->Perception;
			auto& Observer = Observers.FindOrAdd(Pair.Key);
			if (Observer.ProfileRevision != State->PerceptionRevision)
			{ Observer.Contacts.Reset(); Observer.DetectionStarted.Reset(); Observer.ProfileRevision = State->PerceptionRevision; }
			if (SliceCount > 1 && Pair.Key % SliceCount != Slice) continue;
			const double Phase = double((int64(Pair.Key) * 137) % 503) / 503. * .2;
			double Lateness = 0.;
			if (SliceCount == 0) // Each world frame selects individual due observers, not whole batches.
			{
				double* Due = NextScanTimes.Find(Pair.Key);
				if (!Due)
				{
					NextScanTimes.Add(Pair.Key, (FMath::FloorToDouble((Now - Phase) / .2) + 1.) * .2 + Phase);
					continue;
				}
				if (Now + 1.e-6 < *Due) continue;
				Lateness = FMath::Max(0., Now - *Due);
				*Due += .2;
				if (*Due <= Now + 1.e-6) *Due = (FMath::FloorToDouble((Now - Phase) / .2) + 1.) * .2 + Phase;
			}
			UpdatedObservers.Add(Pair.Key);
			const double* PreviousScan = LastScanTimes.Find(Pair.Key);
			Metrics->RecordObserverScan(PreviousScan ? Now - *PreviousScan : 0., Lateness);
			LastScanTimes.Add(Pair.Key, Now);
			const bool bFarPulse = FMath::FloorToInt64((Now - Phase + 1.e-6) / .2) % 2 == 0;
			for (auto& Contact : Observer.Contacts) Contact.Value.bVisible = false;
			TSet<int32> InView;
			const FBattleSightContext Sight = Mixed ? Leader->Sight(Pair.Key) : Unit->CaptureSightContext();
			auto Consider = [&](const FLiveUnit& Candidate)
			{
				if (Candidate.State->Team == State->Team) return;
				if (Metrics->bRecording) ++Metrics->PerceptionCandidates;
				// Never slow known contacts or acquisition already in progress. Short glimpses
				// cannot carry a partly completed timer across skipped far scans.
				if (bSlowUnknownFar && SliceCount == 0 && !bFarPulse &&
					FVector::DistSquared(Sight.Position, Candidate.Position) > FMath::Square(1200.f) &&
					!Observer.Contacts.Contains(Candidate.State->UnitID) && !Observer.DetectionStarted.Contains(Candidate.State->UnitID))
				{ if (Metrics->bRecording) ++Metrics->FarCandidatesSkipped; return; }
				if (Mixed ? !(bReuseSightContext ? Leader->CanSeeWithContext(Sight,Candidate.State->UnitID) : Leader->CanSee(Pair.Key,Candidate.State->UnitID)) : !Unit->CanSeeSnapshot(Sight, Candidate.Unit, Candidate.Position, true)) return;
				const int32 TargetID = Candidate.State->UnitID;
				InView.Add(TargetID);
				double* Started = Observer.DetectionStarted.Find(TargetID);
				if (!Started)
				{
					// Trial -BattleReacquireGrace=1: a target this observer itself saw moments ago is recognised again
					// without repeating the whole detection delay (own non-shared memory only, never the live target state).
					double StartAt = Now;
					if (BattleTactics::ReacquireGraceEnabled())
						if (const auto* Known = Observer.Contacts.Find(TargetID); Known && !Known->bShared && Now - Known->LastSeenTime <= FMath::Max(0.f, GetDefault<UBattleTuning>()->ReacquireGraceSeconds) + 1.e-6)
							StartAt = Now - Profile.DetectionSeconds;
					Observer.DetectionStarted.Add(TargetID, StartAt); Started = Observer.DetectionStarted.Find(TargetID);
					// Evaluation only: a contact this observer saw itself within 1s must wait for detection again.
					if (Metrics->bRecording) if (const auto* Known = Observer.Contacts.Find(TargetID); Known && !Known->bShared && Now - Known->LastSeenTime <= 1. + 1.e-6) ++Metrics->ReacquireKnownStarted;
				}
				if (Now - *Started + 1.e-6 < Profile.DetectionSeconds) return;
				if (Metrics->bRecording && *Started < Now - 1.e-6)
					if (const auto* Known = Observer.Contacts.Find(TargetID); Known && !Known->bShared && Known->LastSeenTime < *Started - 1.e-6 && *Started - Known->LastSeenTime <= 1. + 1.e-6)
					{
						const double Blackout = Now - Known->LastSeenTime;
						++Metrics->ReacquireKnownCompleted; Metrics->ReacquireBlackoutSumSeconds += Blackout; Metrics->ReacquireBlackoutMaxSeconds = FMath::Max(Metrics->ReacquireBlackoutMaxSeconds, Blackout);
					}
				auto& Contact = Observer.Contacts.FindOrAdd(TargetID);
				Contact.TargetID = TargetID;
				Contact.LastKnownPosition = Candidate.Position;
				Contact.LastSeenTime = Now; Contact.Confidence = 1.f; Contact.bVisible = true;
				Contact.SourceUnitID = Pair.Key; Contact.bShared = false;
			};
			// Scout pairs widen the sight context beyond their profile while still; the grid search follows the context.
			const float GridRange = Sight.RangeSquared > FMath::Square(Profile.SightRange) + 1.f ? FMath::Sqrt(Sight.RangeSquared) : Profile.SightRange;
			const int32 MinX = Cell(Sight.Position.X - GridRange), MaxX = Cell(Sight.Position.X + GridRange);
			const int32 MinY = Cell(Sight.Position.Y - GridRange), MaxY = Cell(Sight.Position.Y + GridRange);
			// Very long-range profiles can cover more cells than units; use the reference then.
			if (bUseSpatialGrid && int64(MaxX - MinX + 1) * (MaxY - MinY + 1) <= FMath::Max(64, Alive.Num()))
			{
				TArray<const FLiveUnit*, TInlineAllocator<128>> Candidates;
				for (int32 X = MinX; X <= MaxX; ++X) for (int32 Y = MinY; Y <= MaxY; ++Y)
					if (const auto* Bucket = Grid.Find(FIntPoint(X, Y))) for (const FLiveUnit* Candidate : *Bucket)
						if (Candidate->State->Team != State->Team && FVector::DistSquared(Sight.Position, Candidate->Position) <= Sight.RangeSquared)
							Candidates.Add(Candidate);
				// Preserve insertion/tie-breaking order in contacts and shared reports.
				Candidates.Sort([](const FLiveUnit& A, const FLiveUnit& B) { return A.Ordinal < B.Ordinal; });
				for (const FLiveUnit* Candidate : Candidates) Consider(*Candidate);
			}
			else for (const auto& Candidate : Alive) Consider(Candidate.Value);

			for (auto It = Observer.DetectionStarted.CreateIterator(); It; ++It) if (!InView.Contains(It.Key())) It.RemoveCurrent();
			for (auto It = Observer.Contacts.CreateIterator(); It; ++It)
			{
				auto& Contact = It.Value();
				if (Contact.bVisible) continue;
				const double Age = FMath::Max(0., Now - Contact.LastSeenTime);
				// Do not consult an unseen target's current position or health to update memory.
				if (Profile.MemorySeconds <= 0.f || Age >= Profile.MemorySeconds) { It.RemoveCurrent(); continue; }
				Contact.Confidence = FMath::Clamp(1.f - static_cast<float>(Age / Profile.MemorySeconds), 0.f, 1.f);
			}
		}

	}
	// Deliver snapshots, never a reference that tracks the target's live position.
	{
		FBattleScopedMetric DeliveryTiming(Metrics, EBattleMetricPhase::ReportDelivery);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleReportDelivery);
		auto RemovePendingKey = [&](const FBattleReport& Report)
		{
			const FIntVector Key(Report.Contact.SourceUnitID, Report.ReceiverID, Report.Contact.TargetID);
			if (int32* Count = PendingCounts.Find(Key)) if (--*Count <= 0) PendingCounts.Remove(Key);
		};
		for (auto It = PendingReports.CreateIterator(); It; ++It)
		{
			const auto& Report = *It;
			const auto* Sender = Alive.Find(Report.Contact.SourceUnitID);
			const auto* Receiver = Alive.Find(Report.ReceiverID);
			if (!Sender || !Receiver || Sender->State->PerceptionRevision != Report.SourceRevision ||
				Receiver->State->PerceptionRevision != Report.ReceiverRevision ||
				FVector::DistSquared(Sender->Position, Receiver->Position) > FMath::Square(Sender->State->Perception.ShareRange))
			{ RemovePendingKey(Report); It.RemoveCurrentSwap(EAllowShrinking::No); continue; }
			if (Now < Report.DeliveryTime) continue;
			const float Memory = Receiver->State->Perception.MemorySeconds;
			if (Memory > 0.f && Now - Report.Contact.LastSeenTime < Memory)
			{
				auto& Contacts = Observers.FindChecked(Report.ReceiverID).Contacts;
				const auto* Existing = Contacts.Find(Report.Contact.TargetID);
				if (!Existing || (!Existing->bVisible && Existing->LastSeenTime < Report.Contact.LastSeenTime))
				{
					FBattleContact Copy = Report.Contact; Copy.bVisible = false; Copy.bShared = true;
					Copy.Confidence = FMath::Clamp(1.f - static_cast<float>((Now - Copy.LastSeenTime) / Memory), 0.f, 1.f);
					Contacts.Add(Copy.TargetID, Copy);
     if(Metrics->bRecording) ++Metrics->ReportsDelivered;
				}
			}
			RemovePendingKey(Report); It.RemoveCurrentSwap(EAllowShrinking::No);
		}
	}
	// Avoid a linear search through all pending reports for every sender/receiver/contact triple.
	{
		FBattleScopedMetric QueueTiming(Metrics, EBattleMetricPhase::ReportQueue);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleReportQueue);
		for (const auto& Source : Alive)
		{
			if (!UpdatedObservers.Contains(Source.Key)) continue;
			const auto* SourceState = Source.Value.State;
			if (SourceState->Perception.ShareRange <= 0.f) continue;
			TArray<const FBattleContact*, TInlineAllocator<32>> VisibleContacts;
			for (const auto& Contact : Observers.FindChecked(Source.Key).Contacts) if (Contact.Value.bVisible) VisibleContacts.Add(&Contact.Value);
			if (VisibleContacts.IsEmpty()) continue;
			for (const auto& Receiver : Alive)
			{
				if (Source.Key == Receiver.Key || SourceState->Team != Receiver.Value.State->Team ||
					FVector::DistSquared(Source.Value.Position, Receiver.Value.Position) > FMath::Square(SourceState->Perception.ShareRange)) continue;
				const auto& ReceiverContacts = Observers.FindChecked(Receiver.Key).Contacts;
				for (const FBattleContact* Contact : VisibleContacts)
				{
					const auto* Existing = ReceiverContacts.Find(Contact->TargetID);
					if (Existing && Existing->bVisible) continue;
					const FIntVector Key(Source.Key, Receiver.Key, Contact->TargetID);
					if (PendingCounts.Contains(Key)) continue;
					FBattleReport Report;
					Report.ReceiverID = Receiver.Key; Report.SourceRevision = SourceState->PerceptionRevision;
					Report.ReceiverRevision = Receiver.Value.State->PerceptionRevision;
					Report.Contact = *Contact; Report.DeliveryTime = Now + SourceState->Perception.ShareDelay;
					PendingReports.Add(Report);
					PendingCounts.Add(Key, 1);
				}
			}
		}
	}
	if (Metrics->bRecording) Metrics->PendingReportsPeak = FMath::Max(Metrics->PendingReportsPeak, static_cast<int64>(PendingReports.Num()));
}
