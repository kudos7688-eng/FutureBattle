#pragma once
#include "CoreMinimal.h"
#include "MassEntityManager.h"
#include "BattleReplayData.h"
#include "BattleSquadTrial.generated.h"

class ABattleGameMode;
class ABattleUnit;

USTRUCT()
struct FBattleTrialSoldier : public FMassFragment
{
	GENERATED_BODY()
	int32 UnitID = INDEX_NONE;
	FVector Position = FVector::ZeroVector;
};

// Controlled static lane experiment. Both representations share UBattleState.
UCLASS()
class UBattleSquadTrial : public UObject
{
	GENERATED_BODY()
public:
	void Start(ABattleGameMode& Mode, const FString& Variant);
	void Step(ABattleGameMode& Mode);
	void Stop();
private:

 TSharedPtr<struct FT5RenderDiagnostic> T5Diagnostic;
 bool ConfigureT5Diagnostic();
 void SelectT5LOD();
 bool ConfigureT5Material();
 void SelectT5Material();
 void ApplyT5Material(int32 I,const FBattleReplayUnit* U,const FBattleReplayCamera& C);
 void ApplyT5LODCamera(const FBattleReplayUnit* U,FBattleReplayCamera& C);
 int32 ApplyT5LOD(int32 I,const FBattleReplayUnit* U,const FBattleReplayCamera& C);
 void FinishT5LODFrame();
 void SelectT5Diagnostic();
 void BeginT5Diagnostic(ABattleGameMode& Mode,const FBattleReplayUnit* U,const FBattleReplayCamera& C);
 bool FilterT5Diagnostic(int32 I,const FBattleReplayUnit* U,const FBattleReplayCamera& C,bool& Show);
 void EndT5Diagnostic(ABattleGameMode& Mode,const FBattleReplayUnit* U,const FBattleReplayCamera& C);
 TSharedPtr<class FT5Visibility, ESPMode::ThreadSafe> T5Visibility;
 int64 T5PreviousTimeKey=0;
 bool bT5=false; int32 T5Condition=0,T5Limit=30,T5Anchor=0;
 FString T5View,T5Filter,T5Base;
 TArray<float> T5LastRendered;
 TArray<TSharedPtr<class FJsonValue>> T5Samples;
 TArray<TSharedPtr<class FJsonValue>> T5Assets;
 FBattleReplayCamera T5PreviousCamera{};
 bool StartT5(ABattleGameMode& Mode);
 void SelectT5();
 void StepT5(ABattleGameMode& Mode);
 bool FinishT5();
 void InspectT5Assets();
 bool bT2bMetrics=false, bT2bOn=false, bT2bHeadless=false;
 double T2bRate=540.;
 struct FT2bUnit {
  int32 Target=-1, Start=-1, Aligned=-1, Confirmed=-1;
  FVector Observed=FVector::ZeroVector;
  double InitialAngle=0., Turned=0.;
  bool CandidateFired=false, ValidatedTurn=false;
 };
 TArray<FT2bUnit> T2bUnits;
 TArray<TSharedPtr<class FJsonValue>> T2bTurns, T2bCurve, T2bFirstShots;
 int32 T2bFirstFrame=-1, T2bShotsBefore=0, T2bAliveBefore=0, T2bWindowFrames=0;
 int32 T2bStarted=0, T2bStartThisFrame=0, T2bMaxStarts=0, T2bMaxActive=0;
 int64 T2bWindowShots=0, T2bShotsAfterTurn=0, T2bReadyFov=0, T2bReadyWaiting=0, T2bLostClear=0;
 int32 T2bAngularErrors=0, T2bFireErrors=0;
 double T2bAliveSum=0., T2bShooterSeconds=0., T2bMaxStepAngle=0.;
 bool ConfigureT2b(int32 Count);
 bool T2bActive(int32 I) const;
 void AdvanceT2b(int32 I);
 void BeginT2b(int32 I,int32 Target,FVector Observed);
 void EndT2b(int32 I,const TCHAR* Reason);
 void T2bCandidate(int32 I,int32 Next);
 bool T2bCheck(int32 I,int32 Next,bool Visible,int32 Reject,bool Ready);
 void T2bShot(int32 I,int32 Next);
 void T2bBeforeScan();
 void T2bAfterScan();
 void FinishT2b();
 FString T2aAllocation=TEXT("Off");
 bool bT2aMetrics=false, bT2aHeadless=false;
 int64 T2aDeferred=0, T2aEligible=0, T2aAllocated=0;
 double T2aAliveSum=0; int32 T2aAliveFrames=0;
 struct FT2aWait { int32 Target=-1, Start=-1; };
 TArray<FT2aWait> T2aWaits;
 TArray<TSharedPtr<class FJsonValue>> T2aEvents;
 void T2aEndWait(int32 I, const TCHAR* Reason);
 void FinishT2a();
 bool bReplayBatch=false; int32 ReplayBatchIndex=0; FString ReplaySourceB, ReplayCaptureBase;
 void SelectReplayBatch();
 FString RecordFile, ReplayFile, ReplayReference, ReplayArm, ReplayFeature;
 TArray<uint8> ReplayBytes, ReferenceBytes;
 FBattleReplayHeader ReplayHeader;
 TArray<int32> ReplayPriority;
 TArray<bool> ReplayVisible;
 TArray<uint8> ReplayMasks;
 TMap<class UPrimitiveComponent*,bool> ReplayOriginalShadows;
 int32 ReplayCameraErrors=0;
 int32 ReplayStart=0, ReplayCursor=-120, ReplayAdded=0, ReplayRemoved=0;
 double ReplayAliveSum=0;
 class ACameraActor* ReplayCamera=nullptr;
 void CaptureField(ABattleGameMode& Mode);
 bool SaveFieldRecording();
 bool StartReplay(ABattleGameMode& Mode);
 void StepReplay(ABattleGameMode& Mode);
 bool FinishReplay();
	struct FMember
	{
		ABattleUnit* Actor = nullptr; // owned by Mode.Units
		FMassEntityHandle Entity;
		int32 ID = INDEX_NONE;
		int32 Seen = 0;
		FVector Start = FVector::ZeroVector;
		class USkeletalMeshComponent* Display = nullptr;
	};
	TArray<FMember> Members;
	TSharedPtr<FMassEntityManager> Manager;
	TArray<double> Frames;
	TArray<double> CombatFrames;
	FString Name;
	int32 Frame = -60, Shots = 0, Hits = 0, Rays = 0;
	bool bPilot = false;
	bool bMoving = false;
	bool bField = false;
	int32 Seed = 7;
	struct FFieldUnit
	{
		FVector P, Forward, Slot;
		TArray<FVector> Path;
		int32 Waypoint = 0, Goal = -1, Candidate = -1, Since = 0;
		bool bAlive = true, bMoving = false, bEngaged = false;
	};
	struct FFieldSquad { int32 Exposure = -1, Discovery = -1, Candidate = -1, Since = 0; };
	TArray<FFieldUnit> Soldiers;
	TArray<FFieldSquad> FieldSquads;
	int32 Paths = 0, PathFailures = 0, Loss30 = -1, ContestedFrames = 0, PointChanges = 0;
	double FPS30 = 0;

 enum EFieldTiming { StepTime, FindTime, FindRayTime, RayTime, NavTime, SyncTime, ScanTime, GameTime, RenderTime, RHITime, GPUTime, TimingCount };
 bool bFieldDiagnostics = false;
 double DiagFrame[TimingCount] = {}, DiagTotal[TimingCount] = {}, DiagCombat[TimingCount] = {};
 int32 DiagSamples = 0, DiagCombatSamples = 0;
 int64 RawHits = 0, SharedChecks = 0, SharedDenied[5] = {}, TargetChanges = 0, DuplicateTargetShots = 0, ShotTargetGroups = 0;
 struct FShotBin { int64 Shots=0, RawHits=0, AppliedHits=0; double Distance=0, Spread=0; };
 FShotBin ShotBins[12]; // moving x weapon x distance (<8m, 8-16m, >=16m)
 void FinishFieldDiagnostics();
	int64 Candidates = 0;
	void StepField(ABattleGameMode& Mode);
	bool PlanFieldPath(UWorld* World, int32 Index, FVector Goal);
	void ScanField(ABattleGameMode& Mode);
	void FinishField(ABattleGameMode& Mode);
	struct FExposure { int32 StartFrame = -1; bool bWasOpen = true; bool bDetected = false; };
	TArray<FExposure> Exposures;
	TArray<int32> Latencies;
	int32 Missed = 0, Censored = 0, GeometryErrors = 0, VisibleMass = 0, VisibleActors = 0;
	FString Capture;
	void SetupMovingView(ABattleGameMode& Mode);
	void StepMoving(ABattleGameMode& Mode);
	void FinishMoving(ABattleGameMode& Mode);
	static FVector MovingStart(int32 Team, int32 Lane);
	double LastFrame = 0;
	FVector Position(const FMember& Member) const;
};
