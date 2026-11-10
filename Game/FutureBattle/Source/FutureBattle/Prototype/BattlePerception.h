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
	UPROPERTY() FVector LastKnownPosition = FVector::ZeroVector;
	UPROPERTY() double LastSeenTime = 0.;
	UPROPERTY() float Confidence = 0.f;
	UPROPERTY() bool bVisible = false;
	UPROPERTY() int32 SourceUnitID = INDEX_NONE;
	UPROPERTY() bool bShared = false;
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
	bool bSlowUnknownFar = false; // Diagnostic only: unknown targets beyond 1200cm on alternate visits.
	void Update(const TArray<TObjectPtr<ABattleUnit>>& Units, double Now, int32 Slice = 0, int32 SliceCount = 1);
	const TMap<int32, FBattleContact>* GetContacts(int32 ObserverID) const;
	const FBattleContact* FindContact(int32 ObserverID, int32 TargetID) const;
	void CollectVisibleTargets(const TArray<TObjectPtr<ABattleUnit>>& Units, int32 Team, TSet<int32>& Targets) const;
	void Reset() { Observers.Reset(); PendingReports.Reset(); PendingCounts.Reset(); NextScanTimes.Reset(); LastScanTimes.Reset(); }
	void Capture(TMap<int32, FBattleObserver>& OutObservers, TArray<FBattleReport>& OutReports, double Now) const;
	void Restore(const TMap<int32, FBattleObserver>& SavedObservers, const TArray<FBattleReport>& SavedReports, double Now);
private:
	friend class UBattlePhase3; // Test-only exact state/order audit.
	UPROPERTY() TMap<int32, FBattleObserver> Observers;
	UPROPERTY() TArray<FBattleReport> PendingReports;
	TMap<FIntVector, int32> PendingCounts;
	TMap<int32, double> NextScanTimes, LastScanTimes;
};
