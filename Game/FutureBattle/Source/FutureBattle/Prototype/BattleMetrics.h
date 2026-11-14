#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleMetrics.generated.h"
class FJsonObject;
class ABattleUnit;

enum class EBattleMetricPhase : uint8 { Individual, Perception, Squad, SightScan, ReportDelivery, ReportQueue };

struct FBattleMetricFrame
{
	double WallMS = 0.;
	double GameThreadMS = 0., RenderThreadMS = 0., RHIThreadMS = 0.;
	double GameThreadWaitMS = 0., RenderThreadWaitMS = 0.;
	double IndividualMS = 0.;
	double PerceptionMS = 0.;
	double SquadMS = 0.;
	double SightScanMS = 0., ReportDeliveryMS = 0., ReportQueueMS = 0.;
	int32 Decisions = 0, SenseObservers = 0;
};

UCLASS()
class UBattleMetrics : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void Start();
 // Opt-in mixed benchmark only; legacy BeginFrame/Finish behavior is unchanged.
 FBattleMetricFrame DrainMixedFrame() { const auto Result=Current; Current=FBattleMetricFrame(); return Result; }
	void BeginFrame();
	void RecordTime(EBattleMetricPhase Phase, double Seconds);
	void RecordMoveRetry(ABattleUnit* Unit, FVector Goal, FName Reason, bool bFailed);
	TSharedPtr<FJsonObject> Finish();
	bool bRecording = false;
	double StartedAt = 0.;
	int64 SightChecks = 0, SightRays = 0, TerrainRays = 0, Paths = 0, Decisions = 0, CoverScores = 0;
	int64 CoordinationChanges = 0, MoveFailures = 0, Shots = 0, Hits = 0, PerceptionUpdates = 0;
	int64 MoveRequests = 0, MoveArrivals = 0;
	int64 PendingReportsPeak = 0;
 int64 ReportsDelivered = 0;
	int64 PerceptionObservers = 0, PerceptionCandidates = 0, FarCandidatesSkipped = 0;
	// Evaluation only: a contact seen within 1s re-enters view and restarts the detection timer.
	int64 ReacquireKnownStarted = 0, ReacquireKnownCompleted = 0;
	double ReacquireBlackoutSumSeconds = 0., ReacquireBlackoutMaxSeconds = 0.;
	double SensorIntervalSumMS = 0., SensorIntervalMaxMS = 0., SensorLatenessMaxMS = 0.;
	int64 SensorIntervalSamples = 0;
	void RecordObserverScan(double IntervalSeconds, double LateSeconds);
	void CountDecision() { if (bRecording) { ++Decisions; ++Current.Decisions; } }
private:
	FBattleMetricFrame Current;
	TArray<FBattleMetricFrame> Frames;
	double LastFrameAt = 0.;
	TMap<FName, int64> MoveRetryCauses;
	TArray<TSharedPtr<FJsonObject>> MoveFailureSamples;
};

struct FBattleScopedMetric
{
	UBattleMetrics* Metrics;
	EBattleMetricPhase Phase;
	double Started;
	FBattleScopedMetric(UBattleMetrics* InMetrics, EBattleMetricPhase InPhase)
		: Metrics(InMetrics), Phase(InPhase), Started(InMetrics && InMetrics->bRecording ? FPlatformTime::Seconds() : 0.) {}
	~FBattleScopedMetric() { if (Started > 0.) Metrics->RecordTime(Phase, FPlatformTime::Seconds() - Started); }
};
