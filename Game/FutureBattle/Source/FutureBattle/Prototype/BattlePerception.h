#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlePerception.generated.h"

class ABattleUnit;

USTRUCT()
struct FBattleContact
{
	GENERATED_BODY()
	UPROPERTY() int32 TargetID = INDEX_NONE;
 UPROPERTY() uint8 ObservedSquadType = 255; // unknown until actually identified; never refreshed from unseen truth
	UPROPERTY() FVector LastKnownPosition = FVector::ZeroVector;
	UPROPERTY() double LastSeenTime = 0.;
	UPROPERTY() float Confidence = 0.f;
	UPROPERTY() bool bVisible = false;
	UPROPERTY() int32 SourceUnitID = INDEX_NONE;
	UPROPERTY() bool bShared = false;
 bool bLongRangeObservation = true; // Unknown provenance stays restricted until a fresh sensor observation; copied with reports.
 bool bLocalShareEligible = false; // Measured at acquisition, not from the sender's later position.
};

USTRUCT()
struct FBattleObserver
{
	GENERATED_BODY()
	UPROPERTY() TMap<int32, FBattleContact> Contacts;
	UPROPERTY() TMap<int32, double> DetectionStarted;
	UPROPERTY() int32 ProfileRevision = INDEX_NONE;
};

USTRUCT()
struct FBattleReport
{
	GENERATED_BODY()
	UPROPERTY() int32 ReceiverID = INDEX_NONE;
	UPROPERTY() int32 SourceRevision = 0;
	UPROPERTY() int32 ReceiverRevision = 0;
	UPROPERTY() double DeliveryTime = 0.;
	UPROPERTY() FBattleContact Contact;
};

UCLASS()
class UBattlePerception : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	// Opt-in while comparing against the exhaustive reference. No perception rules change.
	bool bUseSpatialGrid = false;
	bool bReuseSightContext = false; // Opt-in: observer-local, synchronous mixed-runtime scan only.
	bool bEvenSchedule = false;
	int64 SlowScans = 0, NormalScans = 0; // scan interval counters (BATTLE_SIGHT_SCAN)
	double UnseenScanInterval = .2, ScanNearCm = 1.e7; // seconds between scans of an observer that saw no enemy (match: 0.4, set by UBattleLeaderRuntime)
	int64 ParallelUpdates = 0, ParallelTraces = 0; // parallel sight trial counters (BATTLE_SIGHT_PARALLEL)
	// Evaluation only (-BattleSightRayStats=1, default 0; nothing in the match reads these): where the parallel scan's rays go.
	// [0] = ray blocked, [1] = target seen. Own: the observer saw the target himself within 0.45 s. Squad: a squad mate did.
	// Shared: known only from a report. Unknown: no contact. RayKnownPairs: distinct (squad, target) among Own+Squad rays per 0.2 s.
	int64 RayOwn[2] = {0, 0}, RaySquad[2] = {0, 0}, RayShared[2] = {0, 0}, RayUnknown[2] = {0, 0}, RayKnownPairs = 0, RayWindow = -1;
	TSet<uint64> RayWindowPairs;
	bool bSlowUnknownFar = false; // Diagnostic only: unknown targets beyond 1200cm on alternate visits.
	void Update(const TArray<TObjectPtr<ABattleUnit>>& Units, double Now, int32 Slice = 0, int32 SliceCount = 1);
	const TMap<int32, FBattleContact>* GetContacts(int32 ObserverID) const;
	const FBattleContact* FindContact(int32 ObserverID, int32 TargetID) const;
	void CollectVisibleTargets(const TArray<TObjectPtr<ABattleUnit>>& Units, int32 Team, TSet<int32>& Targets) const;
	// Scout reports: an enemy near a reporting scout learns its approximate position (a remembered, never visible, contact).
	void AddHint(int32 ObserverID, int32 TargetID, const FVector& Position, double Now, float Confidence);
	// Trial -BattleHitReveal: the observer was just shot by TargetID and has a clear line to him. The detection delay counts as
	// already spent and the observer scans on the next frame; the contact still forms only through the normal scan (line of sight).
	void RevealShooter(int32 ObserverID, int32 TargetID, double Now, float DetectionSeconds);
	void Reset() { Observers.Reset(); PendingReports.Reset(); PendingCounts.Reset(); NextScanTimes.Reset(); LastScanTimes.Reset(); }
	void Capture(TMap<int32, FBattleObserver>& OutObservers, TArray<FBattleReport>& OutReports, double Now) const;
	void Restore(const TMap<int32, FBattleObserver>& SavedObservers, const TArray<FBattleReport>& SavedReports, double Now);
private:
	friend class UBattlePhase3; // Test-only exact state/order audit.
	friend class UBattleLeaderRuntime; // RunAreaSmoke: scoped visible-contact fixture, without save/load semantics.
	UPROPERTY() TMap<int32, FBattleObserver> Observers;
	UPROPERTY() TArray<FBattleReport> PendingReports;
	TMap<FIntVector, int32> PendingCounts;
	TMap<int32, double> NextScanTimes, LastScanTimes;
};
