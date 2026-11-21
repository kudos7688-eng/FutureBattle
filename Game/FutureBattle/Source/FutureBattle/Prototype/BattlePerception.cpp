#include "BattlePerception.h"
#include "BattleLeaderRuntime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Async/ParallelFor.h"
#include "BattleUnit.h"
#include "BattleMetrics.h"
#include "BattleTactics.h"
#include "BattleMatch.h"

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
	// -BattleAreaCompositionPlanner reads the identified squad type too (review 2026-11-21 F01): record it when either consumer is on.
	const bool bGroupRecordsType = BattleTactics::GroupStrengthEnabled();
 const auto* CompositionMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 const int32 ObserverCompositionTeams=CompositionMatch?CompositionMatch->CompositionTeams:0;
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
	// Vehicles are seen from farther than an observer's own range (UBattleLeaderRuntime::VehicleSpotRange): the grid search adds them.
	TArray<const FLiveUnit*> VehicleUnits; double VehicleSpot2 = 0.;
	if (Mixed && Leader && Leader->VehicleIDs.Num()) { VehicleSpot2 = FMath::Square(Leader->VehicleSpotRange()); for (const auto& L : Alive) if (Leader->IsVehicle(L.Key)) VehicleUnits.Add(&L.Value); }
	// Never slow known contacts or acquisition already in progress. Short glimpses
	// cannot carry a partly completed timer across skipped far scans.
	// One-team trials (-BattleAreaTrialTeam=t): only that team's observers slow their far scans (candidates are enemies).
	const int32 SlowFarTeam = BattleTactics::AreaStrategy().TrialTeam;
	auto SkipFar = [&](const FBattleObserver& Observer, const FBattleSightContext& Sight, bool bFarPulse, const FLiveUnit& Candidate)
	{
		return bSlowUnknownFar && SliceCount == 0 && !bFarPulse && (SlowFarTeam < 0 || Candidate.State->Team != SlowFarTeam) &&
			FVector::DistSquared(Sight.Position, Candidate.Position) > FMath::Square(1200.f) &&
			!Observer.Contacts.Contains(Candidate.State->UnitID) && !Observer.DetectionStarted.Contains(Candidate.State->UnitID);
	};
	// One candidate of one observer's scan. Visible: -1 = test line of sight here, 0/1 = result already traced (parallel trial).
	auto Consider = [&](FBattleObserver& Observer, const TPair<int32, FLiveUnit>& Pair, const FBattleSightContext& Sight, bool bFarPulse, TSet<int32>& InView, const FLiveUnit& Candidate, int32 Visible)
	{
		ABattleUnit* Unit = Pair.Value.Unit;
		const auto* State = Pair.Value.State;
		const auto& Profile = State->Perception;
		if (Candidate.State->Team == State->Team) return;
		if (Metrics->bRecording) ++Metrics->PerceptionCandidates;
		if (SkipFar(Observer, Sight, bFarPulse, Candidate))
		{ if (Metrics->bRecording) ++Metrics->FarCandidatesSkipped; return; }
		if (Visible >= 0 ? Visible == 0 : Mixed ? !(bReuseSightContext ? (VehicleSpot2 > 0. && FVector::DistSquared(Sight.Position, Candidate.Position) > Sight.RangeSquared && Leader->IsVehicle(Candidate.State->UnitID) ? Leader->CanSeeVehicleFar(Pair.Key, Sight, Candidate.State->UnitID, Now) : Leader->CanSeeWithContext(Sight,Candidate.State->UnitID,Pair.Key)) : Leader->CanSee(Pair.Key,Candidate.State->UnitID)) : !Unit->CanSeeSnapshot(Sight, Candidate.Unit, Candidate.Position, true)) return;
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
  if(BattleTactics::ObservedArmour()||bGroupRecordsType||(State->Team>=0&&State->Team<2&&(ObserverCompositionTeams&(1<<State->Team))!=0))
  {
   const auto* SeenSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(Candidate.State->SquadID);
   Contact.ObservedSquadType=SeenSquad?SeenSquad->SquadType:255;
  }
		Contact.LastKnownPosition = Candidate.Position;
		Contact.LastSeenTime = Now; Contact.Confidence = 1.f; Contact.bVisible = true;
		Contact.SourceUnitID = Pair.Key; Contact.bShared = false;
	};
	// End of one observer's scan: forget detections out of view and age unseen contacts.
	auto Forget = [&](FBattleObserver& Observer, const auto& Profile, const TSet<int32>& InView)
	{
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
	};
	// -BattleSightParallel (performance, default ON since 2026-11-17 by user decision; =0 restores the serial scan): the line traces of all observers scanned this update run in a
	// ParallelFor between gathering and applying, in the same frame and order. Mixed runtime with reused sight contexts only; the
	// sight pair reuse trial writes shared state while tracing, so it keeps the serial path.
	static const bool bParallelTrial = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("BattleSightParallel="), V); return V != 0; }();
	static const bool bPairReuseTrial = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("BattleSightPairReuse="), V); return V != 0; }();
	const bool bParallel = bParallelTrial && !bPairReuseTrial && Mixed && bReuseSightContext;
	struct FSightJob { const TPair<int32, FLiveUnit>* Pair = nullptr; FBattleSightContext Sight; bool bFarPulse = false; int32 First = 0, Num = 0; };
	// Scratch buffers of the scan (game thread only, never re-entered): emptied each update, their memory kept for the next one.
	static TArray<FSightJob> Jobs; static TArray<const FLiveUnit*> JobCandidates; static TSet<int32> InView;
	Jobs.Reset(); JobCandidates.Reset();
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
				// Scan interval (performance; user idea and decision 2026-11-17, default 0.4 s): an observer with no enemy in sight at
				// its last scan (or none within -BattleSightScanNear cm when given) looks again only every s seconds; an observer that
				// sees an enemy keeps 0.2 s. -BattleSightScanInterval=0.2 restores the old uniform 0.2 s.
				// Set by the battle runtime for the match perception only (UBattleLeaderRuntime); other perception instances (tests) keep 0.2 s.
				const double ScanInterval = FMath::Clamp(UnseenScanInterval, .2, 2.); const double ScanNear2 = FMath::Square(FMath::Max(0., ScanNearCm));
				double Step = .2;
				if (ScanInterval > .2 + 1.e-6)
				{
					Step = ScanInterval;
					for (const auto& Contact : Observer.Contacts)
						if (Contact.Value.bVisible && !Contact.Value.bShared && FVector::DistSquared(Pair.Value.Position, Contact.Value.LastKnownPosition) <= ScanNear2) { Step = .2; break; }
					++(Step > .2 ? SlowScans : NormalScans);
				}
				*Due += Step;
				if (*Due <= Now + 1.e-6) *Due = (FMath::FloorToDouble((Now - Phase) / .2) + 1.) * .2 + Phase;
			}
			UpdatedObservers.Add(Pair.Key);
			const double* PreviousScan = LastScanTimes.Find(Pair.Key);
			Metrics->RecordObserverScan(PreviousScan ? Now - *PreviousScan : 0., Lateness);
			LastScanTimes.Add(Pair.Key, Now);
			const bool bFarPulse = FMath::FloorToInt64((Now - Phase + 1.e-6) / .2) % 2 == 0;
			for (auto& Contact : Observer.Contacts) Contact.Value.bVisible = false;
			const FBattleSightContext Sight = Mixed ? Leader->Sight(Pair.Key) : Unit->CaptureSightContext();
			// Scout pairs widen the sight context beyond their profile while still; the grid search follows the context.
			const float GridRange = Sight.RangeSquared > FMath::Square(Profile.SightRange) + 1.f ? FMath::Sqrt(Sight.RangeSquared) : Profile.SightRange;
			const int32 MinX = Cell(Sight.Position.X - GridRange), MaxX = Cell(Sight.Position.X + GridRange);
			const int32 MinY = Cell(Sight.Position.Y - GridRange), MaxY = Cell(Sight.Position.Y + GridRange);
			// Very long-range profiles can cover more cells than units; use the reference then.
			const bool bGridSearch = bUseSpatialGrid && int64(MaxX - MinX + 1) * (MaxY - MinY + 1) <= FMath::Max(64, Alive.Num());
			TArray<const FLiveUnit*, TInlineAllocator<128>> Candidates;
			if (bGridSearch)
			{
				for (int32 X = MinX; X <= MaxX; ++X) for (int32 Y = MinY; Y <= MaxY; ++Y)
					if (const auto* Bucket = Grid.Find(FIntPoint(X, Y))) for (const FLiveUnit* Candidate : *Bucket)
						if (Candidate->State->Team != State->Team && FVector::DistSquared(Sight.Position, Candidate->Position) <= Sight.RangeSquared)
							Candidates.Add(Candidate);
				for (const FLiveUnit* V : VehicleUnits) { const double D = FVector::DistSquared(Sight.Position, V->Position); if (V->State->Team != State->Team && D > Sight.RangeSquared && D <= VehicleSpot2) Candidates.Add(V); }
				// Preserve insertion/tie-breaking order in contacts and shared reports.
				Candidates.Sort([](const FLiveUnit& A, const FLiveUnit& B) { return A.Ordinal < B.Ordinal; });
			}
			if (bParallel)
			{
				// Parallel trial: the rest of this observer's scan runs in the apply stage below, in this same observer order.
				FSightJob& Job = Jobs.AddDefaulted_GetRef();
				Job.Pair = &Pair; Job.Sight = Sight; Job.bFarPulse = bFarPulse; Job.First = JobCandidates.Num();
				if (bGridSearch) JobCandidates.Append(Candidates); else for (const auto& Candidate : Alive) JobCandidates.Add(&Candidate.Value);
				Job.Num = JobCandidates.Num() - Job.First;
				continue;
			}
			InView.Reset();
			if (bGridSearch) for (const FLiveUnit* Candidate : Candidates) Consider(Observer, Pair, Sight, bFarPulse, InView, *Candidate, -1);
			else for (const auto& Candidate : Alive) Consider(Observer, Pair, Sight, bFarPulse, InView, Candidate.Value, -1);
			Forget(Observer, Profile, InView);
		}
		if (bParallel && Jobs.Num())
		{
			// Stage 1 (game thread): the same filtering Consider does before its line of sight test; only candidates that would reach
			// CanSeeWithContext's trace get one. Vehicles beyond the observer's own range keep the serial cached path (CanSeeVehicleFar).
			static TArray<int32> Pre; Pre.SetNumUninitialized(JobCandidates.Num(), EAllowShrinking::No);
			struct FSightTrace { FVector End; int32 Job; FHitResult Hit; uint64 Cycles = 0; };
			static TArray<FSightTrace> Traces; Traces.Reset();
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(BattleSightParallelPrepare);
				for (int32 J = 0; J < Jobs.Num(); ++J)
				{
					const FSightJob& Job = Jobs[J]; const FBattleObserver& Observer = Observers.FindChecked(Job.Pair->Key);
					for (int32 K = Job.First; K < Job.First + Job.Num; ++K)
					{
						const FLiveUnit& Candidate = *JobCandidates[K]; Pre[K] = -1;
						if (Candidate.State->Team == Job.Pair->Value.State->Team || SkipFar(Observer, Job.Sight, Job.bFarPulse, Candidate)) continue;
						if (VehicleSpot2 > 0. && FVector::DistSquared(Job.Sight.Position, Candidate.Position) > Job.Sight.RangeSquared && Leader->IsVehicle(Candidate.State->UnitID)) continue;
						FVector End;
						if (!Leader->PrepareSightTrace(Job.Sight, Candidate.State->UnitID, End)) { Pre[K] = 0; continue; }
						Pre[K] = 2 + Traces.Num(); Traces.Add({End, J});
					}
				}
			}
			// Stage 2 (worker threads): line traces only. Nothing in the game is written; each trace takes the Chaos scene read lock
			// itself (FScopedSceneReadLock in the engine's scene query), as the engine's own async traces do from worker threads.
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(BattleSightParallelTrace);
				const UWorld* World = GetWorld(); const bool bStats = UBattleLeaderRuntime::SightHitStatsEnabled();
				ParallelFor(TEXT("BattleSightTraces"), Traces.Num(), 16, [&](int32 I)
				{
					FSightTrace& T = Traces[I]; const FBattleSightContext& S = Jobs[T.Job].Sight;
					const uint64 T0 = bStats ? FPlatformTime::Cycles64() : 0;
					World->LineTraceSingleByChannel(T.Hit, S.Eye, T.End, ECC_Visibility, S.Query);
					if (bStats) T.Cycles = FPlatformTime::Cycles64() - T0;
				});
			}
			// Stage 3 (game thread): apply in the original observer and candidate order, with the hit policy of CanSeeWithContext.
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(BattleSightParallelApply);
				const bool bStats = UBattleLeaderRuntime::SightHitStatsEnabled();
				for (const FSightJob& Job : Jobs)
				{
					FBattleObserver& Observer = Observers.FindChecked(Job.Pair->Key);
					InView.Reset();
					for (int32 K = Job.First; K < Job.First + Job.Num; ++K)
					{
						const FLiveUnit& Candidate = *JobCandidates[K]; int32 Visible = Pre[K] < 0 ? -1 : 0;
						if (Pre[K] >= 2)
						{
							const FSightTrace& T = Traces[Pre[K] - 2];
							if (bStats) Leader->RecordSightHit(T.Hit, Candidate.State->UnitID, T.Cycles, UBattleLeaderRuntime::SightFromPerception);
							Visible = Leader->SightHitVisible(T.Hit, Candidate.State->UnitID) ? 1 : 0;
						}
						Consider(Observer, *Job.Pair, Job.Sight, Job.bFarPulse, InView, Candidate, Visible);
					}
					Forget(Observer, Job.Pair->Value.State->Perception, InView);
				}
			}
			++ParallelUpdates; ParallelTraces += Traces.Num();
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
			// Contacts beyond the source's own profile range (a still scout's long sight) are not passed on by word of mouth; they travel in scout reports.
			const double LocalRange2 = FMath::Square(SourceState->Perception.SightRange * 1.1);
			for (const auto& Contact : Observers.FindChecked(Source.Key).Contacts) if (Contact.Value.bVisible) if (FVector::DistSquared(Source.Value.Position, Contact.Value.LastKnownPosition) <= LocalRange2) VisibleContacts.Add(&Contact.Value);
			if (VisibleContacts.IsEmpty()) continue;
			// Performance (2026-11-17): receivers come from the sight grid cells around the source instead of every living unit,
			// in the same (ordinal) order, so the reports and their order are unchanged.
			const double Share2 = FMath::Square(SourceState->Perception.ShareRange);
			TArray<const FLiveUnit*, TInlineAllocator<64>> Receivers;
			const int32 ShareCells = FMath::CeilToInt(SourceState->Perception.ShareRange / CellSize);
			if (bUseSpatialGrid && ShareCells <= 4)
			{
				const int32 CX = Cell(Source.Value.Position.X), CY = Cell(Source.Value.Position.Y);
				for (int32 X = CX - ShareCells; X <= CX + ShareCells; ++X) for (int32 Y = CY - ShareCells; Y <= CY + ShareCells; ++Y)
					if (const auto* Bucket = Grid.Find(FIntPoint(X, Y))) for (const FLiveUnit* R : *Bucket)
						if (R->State->Team == SourceState->Team && FVector::DistSquared(Source.Value.Position, R->Position) <= Share2) Receivers.Add(R);
				Receivers.Sort([](const FLiveUnit& A, const FLiveUnit& B) { return A.Ordinal < B.Ordinal; });
			}
			else for (const auto& R : Alive) if (R.Value.State->Team == SourceState->Team && FVector::DistSquared(Source.Value.Position, R.Value.Position) <= Share2) Receivers.Add(&R.Value);
			for (const FLiveUnit* ReceiverUnit : Receivers)
			{
				const int32 ReceiverKey = ReceiverUnit->State->UnitID;
				if (Source.Key == ReceiverKey) continue;
				const struct { int32 Key; const FLiveUnit& Value; } Receiver{ReceiverKey, *ReceiverUnit};
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

void UBattlePerception::RevealShooter(int32 ObserverID, int32 TargetID, double Now, float DetectionSeconds)
{
	auto* Observer = Observers.Find(ObserverID); if (!Observer) return;
	if (const auto* Known = Observer->Contacts.Find(TargetID); Known && Known->bVisible) return;
	double& Started = Observer->DetectionStarted.FindOrAdd(TargetID, Now);
	Started = FMath::Min(Started, Now - FMath::Max(0.f, DetectionSeconds));
	if (double* Due = NextScanTimes.Find(ObserverID)) *Due = FMath::Min(*Due, Now);
}

void UBattlePerception::AddHint(int32 ObserverID, int32 TargetID, const FVector& Position, double Now, float Confidence)
{
	auto* Observer = Observers.Find(ObserverID); if (!Observer) return;
	if (const auto* Existing = Observer->Contacts.Find(TargetID); Existing && (Existing->bVisible || Existing->LastSeenTime > Now - .5)) return;
	FBattleContact Hint; Hint.TargetID = TargetID; Hint.LastKnownPosition = Position; Hint.LastSeenTime = Now;
	Hint.Confidence = FMath::Clamp(Confidence, 0.f, 1.f); Hint.bVisible = false; Hint.SourceUnitID = ObserverID; Hint.bShared = true; // its own (valid, same-team) source for saves
	Observer->Contacts.Add(TargetID, Hint);
}
