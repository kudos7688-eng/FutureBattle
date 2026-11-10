#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "BattleState.h"
#include "BattleAnimTasks.h"
#include "BattleUnit.h"
#include "BattleLeaderRuntime.generated.h"

class ABattleGameMode;
class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

USTRUCT()
struct FBattleFollowerFragment : public FMassFragment
{
 GENERATED_BODY()
 int32 UnitID=INDEX_NONE;
 FVector Position=FVector::ZeroVector;
 FVector Forward=FVector::ForwardVector;
};

enum class EBattleSquadIntent : uint8 { Unspecified };
struct FBattleSquadSummary
{
 int32 SquadID=INDEX_NONE, LeaderUnitID=INDEX_NONE, Revision=0;
 FString Name;
 bool bActive=false, bSelectable=false;
 EBattleSquadIntent Intent=EBattleSquadIntent::Unspecified;
 uint32 ProblemFlags=0;
};

// Shared by Canvas and pointer input; no stale hit rectangles from the previous draw.
struct FBattleSquadListLayout
{
 FVector2D Origin;
 float Width=300, Height=0, RowHeight=28, Header=44;
 int32 VisibleRows=0;
 static FBattleSquadListLayout Make(FVector2D View);
 bool Contains(FVector2D P) const;
 int32 RowAt(FVector2D P) const;
};

struct FBattleLeaderBody
{
 TWeakObjectPtr<ABattleUnit> Actor;
 FMassEntityHandle Entity;
 TWeakObjectPtr<UCapsuleComponent> Capsule;
 TWeakObjectPtr<USkeletalMeshComponent> Mesh;
 TWeakObjectPtr<UStaticMeshComponent> Weapon;
 FVector Velocity=FVector::ZeroVector;
 double StalledSince=0;
 int32 Waypoint=1, Pose=0;
 FBattleAnimTasks AnimTasks;
};
struct FBattleLeaderPlan
{
 int32 OrderRevision=INDEX_NONE, TargetID=INDEX_NONE;
 double NextThink=0, NextPath=0;
 bool bReplan=true;
 TArray<FVector> Path;
 FVector Center=FVector::ZeroVector;
};

UCLASS()
class UBattleLeaderRuntime : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool IsEnabled() const { return bEnabled; }
 bool Start(ABattleGameMode& Mode);
 void Step(float DeltaTime);
 virtual void Deinitialize() override;
 FVector Position(int32 ID) const;
 FVector Forward(int32 ID) const;
 ABattleUnit* Actor(int32 ID) const;
 const TMap<int32,FBattleLeaderBody>& GetBodies() const { return Bodies; }
 FBattleSightContext Sight(int32 ID) const;
 bool CanSee(int32 ObserverID,int32 TargetID) const;
 // Context must belong to the current observer/update; never retained across updates.
 bool CanSeeWithContext(const FBattleSightContext& Context,int32 TargetID) const;
 bool Fire(int32 ID,FVector Start,FVector Direction,bool bPlayerShot);
 bool IssueOrder(int32 SquadID,EBattleOrder Order,FVector Goal);
 bool CanSelectLeader(int32 ID) const;
 bool CanControlLeader(int32 ID) const;
 const TArray<FBattleSquadSummary>& Summaries();
 void OnDeath(int32 ID);
 void RunSmoke();
 void RunUIQA();
 class ABattleVATDisplay* VATDisplay() const {return VAT;}
private:
 friend class UBattlePhase3;
 friend struct FBattleMixedUnit;
 bool bEnabled=false, bSummaryDirty=true, bReduceHiddenBones=false, bReduceAnimTasks=false;
 UPROPERTY() TObjectPtr<class UBattlePhase3> Phase3;
 UPROPERTY() TObjectPtr<class ABattleVATDisplay> VAT;
 int32 SmokeFailures=0, SmokeStage=0, QAStage=0;
 FString QAPrefix;
 FVector SmokePosition=FVector::ZeroVector;
 TSharedPtr<FMassEntityManager> Manager;
 TMap<int32,FBattleLeaderBody> Bodies;
 TMap<int32,FBattleLeaderPlan> Plans;
 TMap<FIntPoint,TArray<int32>> AvoidanceGrid;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,int32> HitIDs;
 TArray<FBattleSquadSummary> CachedSummaries;
 UPROPERTY() TObjectPtr<AActor> DisplayHost;
 UPROPERTY() TObjectPtr<class UAnimSequence> IdlePose;
 UPROPERTY() TObjectPtr<class UAnimSequence> WalkPose;
 UPROPERTY() TObjectPtr<class UAnimSequence> ReloadPose;
 UPROPERTY() TObjectPtr<ABattleGameMode> Mode;
 FDelegateHandle DeathHandle;
 void Think(int32 SquadID,double Now);
 void Follow(int32 ID,float Dt,double Now);
 void RefreshDisplay();
};
