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
 // Trial -BattleSpotTrack (BattleTactics.h): where the enemy is now, for some seconds after its observer lost sight of it; for
 // looking, walking and cover only (MoveAt), never to fire at. Not saved, not copied to reports or the squad table.
 bool bTracked = false;
 FVector TrackedPosition = FVector::ZeroVector;
 const FVector& MoveAt() const { return bTracked ? TrackedPosition : LastKnownPosition; }
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

// Squad knowledge, stage 1 (Docs/design-squad-knowledge.md): what a squad knows, as one table - enemy -> the members' contacts
// for him (seen or reported), in member order. A read-only index of the per-soldier contacts; no rule changes. The pointers are
// valid only until the contacts change (the next perception update, a hint, a restore), so build it and use it in one call.
struct FBattleSquadSighting { int32 MemberID = INDEX_NONE; const FBattleContact* Contact = nullptr; };
struct FBattleSquadKnowledge
{
	TMap<int32, TArray<FBattleSquadSighting, TInlineAllocator<4>>> ByTarget;
	// The contact of the first member (in member order, living or not) who knows the enemy.
	const FBattleContact* First(int32 TargetID) const { const auto* L = ByTarget.Find(TargetID); return L && L->Num() ? (*L)[0].Contact : nullptr; }
};

// Squad knowledge, stage 2 (trial -BattleSquadKnowledge=1, default 0; Docs/design-squad-knowledge.md section 9).
// One enemy known to a squad. Contact holds the newest information: bVisible = a living member sees him now,
// bShared = the newest information came from another squad's report.
USTRUCT()
struct FBattleSquadContact
{
	GENERATED_BODY()
	UPROPERTY() FBattleContact Contact;
	UPROPERTY() int32 SeerID = INDEX_NONE; // the living member with the lowest ID who sees him now
};

USTRUCT()
struct FBattleSquadIntel
{
	GENERATED_BODY()
	UPROPERTY() TMap<int32, FBattleSquadContact> Known;
};

USTRUCT()
struct FBattleSquadReport
{
	GENERATED_BODY()
	UPROPERTY() int32 SourceSquadID = INDEX_NONE;
	UPROPERTY() int32 ReceiverSquadID = INDEX_NONE;
	UPROPERTY() int32 SourceRevision = 0;
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
	// [zone][role][seen]: zone 0 = same sector or within 20 m, 1 = neighbouring sector, 2 = farther; role 1 = rifle squad member who is not the leader.
	int64 RayZone[3][2][2] = {};
	int64 SectorSightSkipped = 0; // -BattleSectorSight: cross-sector candidates a rifle squad member left to his lookout
	// Trial -BattleSectorSightHit: until the given time the observer scans for this cross-sector enemy himself (he was hit by him).
	void AllowCrossSector(int32 ObserverID, int32 TargetID, double Until) { double& Held = CrossSectorLook.FindOrAdd((uint64(uint32(ObserverID)) << 32) | uint32(TargetID)); Held = FMath::Max(Held, Until); }
	TMap<uint64, double> CrossSectorLook;
	double CrossSectorUntil(int32 ObserverID, int32 TargetID) const { return CrossSectorLook.FindRef((uint64(uint32(ObserverID)) << 32) | uint32(TargetID)); }
	bool bSlowUnknownFar = false; // Diagnostic only: unknown targets beyond 1200cm on alternate visits.
	void Update(const TArray<TObjectPtr<ABattleUnit>>& Units, double Now, int32 Slice = 0, int32 SliceCount = 1);
	const TMap<int32, FBattleContact>* GetContacts(int32 ObserverID) const;
	const FBattleContact* FindContact(int32 ObserverID, int32 TargetID) const;
	// Trial -BattleSquadKnowledge=1 (default 0, mixed runtime only; -BattleSquadKnowledgeTeam=t limits it to one team): a soldier's
	// own table holds only what he saw himself; what his squad saw and heard is in the squad's table, without delay inside the
	// squad, and reports travel from squad to squad (one per enemy). Firing still needs the soldier's own sighting.
	static int32 SquadKnowledgeMode(int32 Team);
	const TMap<int32, FBattleSquadContact>* GetSquadContacts(int32 SquadID) const;
	const FBattleSquadContact* FindSquadContact(int32 SquadID, int32 TargetID) const;
	// What the soldier knows of the target: his own contact, or what his squad knows when that is newer (returned as heard:
	// not visible, shared). With the trial off it is his own contact, as FindContact.
	bool Knows(int32 ObserverID, int32 SquadID, int32 TargetID, FBattleContact& Out) const;
	int64 SquadReportsQueued = 0, SquadReportsDelivered = 0, SquadReportsPeak = 0; // BATTLE_SQUAD_KNOWLEDGE
	mutable int64 SquadKnowsHeard = 0; // Knows() answered from the squad's table
	void BuildSquadKnowledge(const TArray<int32>& Members, FBattleSquadKnowledge& Out) const;
	void CollectVisibleTargets(const TArray<TObjectPtr<ABattleUnit>>& Units, int32 Team, TSet<int32>& Targets) const;
	// Scout reports: an enemy near a reporting scout learns its approximate position (a remembered, never visible, contact).
	void AddHint(int32 ObserverID, int32 TargetID, const FVector& Position, double Now, float Confidence);
	// Trial -BattleHitReveal: the observer was just shot by TargetID and has a clear line to him. The detection delay counts as
	// already spent and the observer scans on the next frame; the contact still forms only through the normal scan (line of sight).
	void RevealShooter(int32 ObserverID, int32 TargetID, double Now, float DetectionSeconds);
	void Reset() { Observers.Reset(); PendingReports.Reset(); PendingCounts.Reset(); NextScanTimes.Reset(); LastScanTimes.Reset(); CrossSectorLook.Reset(); SquadIntel.Reset(); SquadReports.Reset(); SquadReportKeys.Reset(); }
	void Capture(TMap<int32, FBattleObserver>& OutObservers, TArray<FBattleReport>& OutReports, double Now) const;
	void Restore(const TMap<int32, FBattleObserver>& SavedObservers, const TArray<FBattleReport>& SavedReports, double Now);
private:
	friend class UBattlePhase3; // Test-only exact state/order audit.
	friend class UBattleLeaderRuntime; // RunAreaSmoke: scoped visible-contact fixture, without save/load semantics.
	UPROPERTY() TMap<int32, FBattleObserver> Observers;
	UPROPERTY() TArray<FBattleReport> PendingReports;
	TMap<FIntVector, int32> PendingCounts;
	UPROPERTY() TMap<int32, FBattleSquadIntel> SquadIntel;
	UPROPERTY() TArray<FBattleSquadReport> SquadReports;
	TSet<FIntVector> SquadReportKeys; // (source squad, receiver squad, target) waiting in SquadReports
	TMap<int32, double> NextScanTimes, LastScanTimes;
};
