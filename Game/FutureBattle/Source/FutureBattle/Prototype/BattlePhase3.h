#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "BattleState.h"
#include "BattlePhase3.generated.h"

class UBattleLeaderRuntime;
class ABattleGameMode;
struct FBattleMixedDriver;

// Both representations publish to ONE UE RVO solver. No follower Actors are created.
UCLASS()
class UBattleMixedAvoidance : public UAvoidanceManager
{
 GENERATED_BODY()
public:
 void Publish(int32 UID,const FNavAvoidanceData& Data) { UpdateRVO_Internal(UID,Data); }
};

enum class EPhase3Cost : uint8 { Perception, Decision, Path, Follow, Avoidance, Shot, Display, Count };
struct FPhase3Scope;

UCLASS()
class UBattlePhase3 : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool Configure(UBattleLeaderRuntime& Runtime);
 void Begin();
 void Step(float Dt);
 void Finish();
 virtual void Deinitialize() override;
 bool bEnabled=false, bIndividual=false, bFinished=false, bStable=false, bCaptured=false, bViewSet=false, bViewChecked=false;
 FString Output, View=TEXT("RTS"), Layout=TEXT("Open");
 int32 Seed=1, ControlledID=1, Failures=0;
 double Warmup=10, Duration=60, Started=0, LastWall=0;
 FVector SpawnCenter(int32 Team,int32 Group) const;
 FVector Goal(int32 Team,int32 Group) const;
 bool Path(int32 ID,FVector Goal,TArray<FVector>& Points);
 void Rotate(int32 ID,FRotator Rotation);
 FVector Velocity(int32 ID) const;
 void SetPath(int32 ID,const TArray<FVector>& Points);
 void Stop(int32 ID);
 void OnDeath(int32 ID);
 void ApplyLOD();
 void Verify();
 void VerifyCombat();
 void ConfigureView();
 void Sample();
 UBattleLeaderRuntime* Runtime=nullptr;
 ABattleGameMode* Mode=nullptr;
 TMap<int32,TSharedPtr<FBattleMixedDriver>> Drivers;
 TMap<int32,int32> AvoidanceIDs, LODs;
 TMap<int32,FVector> Desired, Solved;
 TMap<int32,TArray<FVector>> Paths;
 TMap<int32,int32> Waypoints;
 TMap<int32,FVector> InitialPositions;
 int64 PathRequests=0, PathFailures=0, Shots=0, Deaths=0, RVOChanges=0, Decisions=0;
 double DecisionLateMS=0;
 double Costs[int32(EPhase3Cost::Count)]={};
 EPhase3Cost ActiveCost=EPhase3Cost::Count;
 double CostStart=0;
 TArray<FString> Rows;
 // Measurement-only perception comparison; never enabled outside BattlePhase3.
 bool bSenseBatch=false, bSenseAudit=false, bContextAudit=false, bContextReplay=false, bGridAudit=false;
 void VerifyGrid();
 TArray<FString> ContextReplayRows;
 void RecordContextReplay();
 double SenseNext=0, ShadowNext=0, BatchLateMS=0;
 int32 SenseSlice=0, ShadowSlice=0;
 int64 AuditFrames=0, ContactMismatchFrames=0, TimerMismatchFrames=0, ReportMismatchFrames=0;
 TSet<FIntPoint> FirstDirect[2], FirstShared[2];
 TArray<FString> SenseEvents;
 void AdvanceSense(class UBattlePerception* Sense, bool Batch, double Now, double& Next, int32& Slice, int32 AuditIndex);
 void CompareSense(double Now);
 UPROPERTY() TObjectPtr<class UBattlePerception> ShadowSense;
 UPROPERTY() TObjectPtr<UBattleMixedAvoidance> Avoidance;
 UPROPERTY() TObjectPtr<class ACameraActor> BenchmarkCamera;
 UPROPERTY() TObjectPtr<class ABattleHiddenBoneTrial> BoneTrial;
 UPROPERTY() TObjectPtr<class ABattleAnimTaskTrial> AnimTaskTrial;
};

// Exclusive nesting: child time is removed from the parent, unlike ordinary inclusive scopes.
struct FPhase3Scope
{
 UBattlePhase3* P; EPhase3Cost Previous;
 FPhase3Scope(UBattlePhase3* In,EPhase3Cost Cost);
 ~FPhase3Scope();
};
