#include "BattleMetrics.h"
#include "Dom/JsonObject.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "RenderTimer.h"

void UBattleMetrics::Start()
{
	Frames.Reset(); Current = FBattleMetricFrame();
	SightChecks = SightRays = TerrainRays = Paths = Decisions = CoverScores = 0;
	CoordinationChanges = MoveFailures = Shots = Hits = PerceptionUpdates = 0;
	MoveRequests = MoveArrivals = 0;
	PendingReportsPeak = 0; PerceptionObservers = PerceptionCandidates = FarCandidatesSkipped = 0;
	SensorIntervalSumMS = SensorIntervalMaxMS = SensorLatenessMaxMS = 0.; SensorIntervalSamples = 0;
	MoveRetryCauses.Reset(); MoveFailureSamples.Reset();
	StartedAt = LastFrameAt = FPlatformTime::Seconds(); bRecording = true;
}

void UBattleMetrics::BeginFrame()
{
	if (!bRecording) return;
	const double Now = FPlatformTime::Seconds();
	Current.WallMS = (Now - LastFrameAt) * 1000.;
	// UE publishes these at viewport draw. Sample the latest completed frame;
	// thread CPU times overlap and must not be added to wall-frame or custom-AI time.
	Current.GameThreadMS = FPlatformTime::ToMilliseconds(GGameThreadTime);
	Current.RenderThreadMS = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	Current.RHIThreadMS = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	Current.GameThreadWaitMS = FPlatformTime::ToMilliseconds(GGameThreadWaitTime);
	Current.RenderThreadWaitMS = FPlatformTime::ToMilliseconds(GRenderThreadWaitTime);
	Frames.Add(Current); Current = FBattleMetricFrame(); LastFrameAt = Now;
}

void UBattleMetrics::RecordObserverScan(double IntervalSeconds, double LateSeconds)
{
	if (!bRecording) return;
	++PerceptionObservers; ++Current.SenseObservers;
	if (IntervalSeconds > 0.)
	{ ++SensorIntervalSamples; SensorIntervalSumMS += IntervalSeconds * 1000.; SensorIntervalMaxMS = FMath::Max(SensorIntervalMaxMS, IntervalSeconds * 1000.); }
	SensorLatenessMaxMS = FMath::Max(SensorLatenessMaxMS, LateSeconds * 1000.);
}

void UBattleMetrics::RecordTime(EBattleMetricPhase Phase, double Seconds)
{
	if (!bRecording) return;
	switch (Phase)
	{
	case EBattleMetricPhase::Individual: Current.IndividualMS += Seconds * 1000.; break;
	case EBattleMetricPhase::Perception: Current.PerceptionMS += Seconds * 1000.; break;
	case EBattleMetricPhase::Squad: Current.SquadMS += Seconds * 1000.; break;
	case EBattleMetricPhase::SightScan: Current.SightScanMS += Seconds * 1000.; break;
	case EBattleMetricPhase::ReportDelivery: Current.ReportDeliveryMS += Seconds * 1000.; break;
	case EBattleMetricPhase::ReportQueue: Current.ReportQueueMS += Seconds * 1000.; break;
	}
}

void UBattleMetrics::RecordMoveRetry(ABattleUnit* Unit, FVector Goal, FName Reason, bool bFailed)
{
	if (!bRecording || !Unit || !Unit->GetState()) return;
	++MoveRetryCauses.FindOrAdd(Reason);
	if (!bFailed || MoveFailureSamples.Num() >= 128) return;
	const auto* S = Unit->GetState(); auto Sample = MakeShared<FJsonObject>();
	Sample->SetNumberField(TEXT("unit_id"), S->UnitID); Sample->SetNumberField(TEXT("squad_id"), S->SquadID);
	Sample->SetStringField(TEXT("cause"), Reason.ToString()); Sample->SetStringField(TEXT("order"), BattleOrderName(S->Order));
	Sample->SetStringField(TEXT("position"), Unit->GetActorLocation().ToString()); Sample->SetStringField(TEXT("goal"), Goal.ToString());
	Sample->SetNumberField(TEXT("distance_cm"), FVector::Dist2D(Unit->GetActorLocation(), Goal));
	int32 Neighbors = 0;
	if (const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Other : Mode->Units) if (Other && Other != Unit && Other->IsAlive() && FVector::DistSquared2D(Unit->GetActorLocation(), Other->GetActorLocation()) < FMath::Square(150.f)) ++Neighbors;
	Sample->SetNumberField(TEXT("neighbors_within_150cm"), Neighbors); MoveFailureSamples.Add(Sample);
}

TSharedPtr<FJsonObject> UBattleMetrics::Finish()
{
	BeginFrame(); bRecording = false;
	auto Root = MakeShared<FJsonObject>();
	const double Duration = FMath::Max(.001, FPlatformTime::Seconds() - StartedAt);
	Root->SetNumberField(TEXT("duration_seconds"), Duration); Root->SetNumberField(TEXT("frames"), Frames.Num());
	auto Distribution = [&](const TCHAR* Name, auto Value, bool bPositiveOnly = false)
	{
		TArray<double> Samples; double Sum = 0.;
		for (const auto& Frame : Frames) { const double Sample = Value(Frame); if (bPositiveOnly && Sample <= 0.) continue; Samples.Add(Sample); Sum += Sample; }
		Samples.Sort(); auto Stats = MakeShared<FJsonObject>();
		Stats->SetNumberField(TEXT("mean"), Samples.IsEmpty() ? 0. : Sum / Samples.Num());
		Stats->SetNumberField(TEXT("p95"), Samples.IsEmpty() ? 0. : Samples[FMath::Clamp(FMath::CeilToInt(Samples.Num() * .95) - 1, 0, Samples.Num() - 1)]);
		Stats->SetNumberField(TEXT("max"), Samples.IsEmpty() ? 0. : Samples.Last());
		Root->SetObjectField(Name, Stats);
	};
	Distribution(TEXT("frame_wall_ms"), [](const FBattleMetricFrame& F) { return F.WallMS; });
	Distribution(TEXT("game_thread_cpu_ms"), [](const FBattleMetricFrame& F) { return F.GameThreadMS; });
	Distribution(TEXT("render_thread_cpu_ms"), [](const FBattleMetricFrame& F) { return F.RenderThreadMS; });
	Distribution(TEXT("rhi_thread_cpu_ms"), [](const FBattleMetricFrame& F) { return F.RHIThreadMS; });
	Distribution(TEXT("game_thread_wait_ms"), [](const FBattleMetricFrame& F) { return F.GameThreadWaitMS; });
	Distribution(TEXT("render_thread_wait_ms"), [](const FBattleMetricFrame& F) { return F.RenderThreadWaitMS; });
	Root->SetBoolField(TEXT("engine_thread_samples_available"), Frames.ContainsByPredicate([](const FBattleMetricFrame& F) { return F.GameThreadMS > 0; }));
	Root->SetStringField(TEXT("engine_thread_timing_scope"), TEXT("Latest completed UE viewport frame; CPU times exclude idle; threads overlap; not GPU duration; zero may mean unavailable with nullrhi"));
	Distribution(TEXT("individual_ai_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.IndividualMS; });
	Distribution(TEXT("perception_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.PerceptionMS; });
	Distribution(TEXT("perception_scan_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.SightScanMS; });
	Distribution(TEXT("report_delivery_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.ReportDeliveryMS; });
	Distribution(TEXT("report_queue_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.ReportQueueMS; });
	Distribution(TEXT("squad_ai_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.SquadMS; });
	Distribution(TEXT("combined_ai_ms_per_frame"), [](const FBattleMetricFrame& F) { return F.IndividualMS + F.PerceptionMS + F.SquadMS; });
	Distribution(TEXT("active_ai_frame_ms"), [](const FBattleMetricFrame& F) { return F.IndividualMS + F.PerceptionMS + F.SquadMS; }, true);
	double TotalAI = 0.; for (const auto& Frame : Frames) TotalAI += Frame.IndividualMS + Frame.PerceptionMS + Frame.SquadMS;
	Root->SetNumberField(TEXT("custom_ai_ms_per_second"), TotalAI / Duration);
	Root->SetStringField(TEXT("timing_scope"), TEXT("Custom controller, perception subsystem, squad subsystem; excludes UE movement, animation, rendering and HUD"));
	Distribution(TEXT("decisions_per_frame"), [](const FBattleMetricFrame& F) { return static_cast<double>(F.Decisions); });
	auto Counters = MakeShared<FJsonObject>();
	Counters->SetNumberField(TEXT("sight_checks"), SightChecks); Counters->SetNumberField(TEXT("sight_rays"), SightRays);
	Counters->SetNumberField(TEXT("terrain_rays"), TerrainRays); Counters->SetNumberField(TEXT("path_queries"), Paths);
	Counters->SetNumberField(TEXT("decisions"), Decisions); Counters->SetNumberField(TEXT("cover_scores"), CoverScores);
	Counters->SetNumberField(TEXT("coordination_changes"), CoordinationChanges); Counters->SetNumberField(TEXT("move_failures"), MoveFailures);
	Counters->SetNumberField(TEXT("shots"), Shots); Counters->SetNumberField(TEXT("hits"), Hits); Counters->SetNumberField(TEXT("perception_updates"), PerceptionUpdates);
	Counters->SetNumberField(TEXT("move_requests"), MoveRequests); Counters->SetNumberField(TEXT("move_arrivals"), MoveArrivals);
	Counters->SetNumberField(TEXT("pending_reports_peak"), PendingReportsPeak);
	Counters->SetNumberField(TEXT("perception_observers"), PerceptionObservers);
	Counters->SetNumberField(TEXT("perception_candidates"), PerceptionCandidates);
	Counters->SetNumberField(TEXT("far_candidates_skipped"), FarCandidatesSkipped);
	Root->SetObjectField(TEXT("counts"), Counters);
	auto SensorTiming = MakeShared<FJsonObject>();
	SensorTiming->SetNumberField(TEXT("interval_mean_ms"), SensorIntervalSamples ? SensorIntervalSumMS / SensorIntervalSamples : 0.);
	SensorTiming->SetNumberField(TEXT("interval_max_ms"), SensorIntervalMaxMS);
	SensorTiming->SetNumberField(TEXT("deadline_lateness_max_ms"), SensorLatenessMaxMS);
	Root->SetObjectField(TEXT("sensor_timing"), SensorTiming);
	Distribution(TEXT("observers_per_frame"), [](const FBattleMetricFrame& F) { return static_cast<double>(F.SenseObservers); });
	auto RetryCauses = MakeShared<FJsonObject>(); for (const auto& Pair : MoveRetryCauses) RetryCauses->SetNumberField(Pair.Key.ToString(), Pair.Value);
	Root->SetObjectField(TEXT("move_retry_causes"), RetryCauses);
	TArray<TSharedPtr<FJsonValue>> Failures; for (const auto& Sample : MoveFailureSamples) Failures.Add(MakeShared<FJsonValueObject>(Sample));
	Root->SetArrayField(TEXT("move_failure_samples"), Failures);
	Root->SetNumberField(TEXT("decisions_per_second"), Decisions / Duration);
	Root->SetNumberField(TEXT("sight_rays_per_second"), SightRays / Duration);
	Root->SetNumberField(TEXT("path_queries_per_second"), Paths / Duration);
	return Root;
}
