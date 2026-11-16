#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "BattleState.h"
#include "BattleAnimTasks.h"
#include "BattleUnit.h"
#include "BattleSightExperiment.h"
#include "BattleTactics.h"
#include "BattleLeaderRuntime.generated.h"

class ABattleGameMode;
class UBattleSave;
struct FBattleSavedUnit;
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
 float Width=360, Height=0, RowHeight=56, Header=44;
 int32 VisibleRows=0;
 static FBattleSquadListLayout Make(FVector2D View);
 // Header controls (command UI 2026-11-16): five filter tabs and a sort button on the second header line.
 static constexpr int32 FilterCount=5; static constexpr float TabWidth=50, TabTop=23, TabHeight=18, SortWidth=86;
 int32 FilterTabAt(FVector2D P) const {if(P.Y<Origin.Y+TabTop||P.Y>=Origin.Y+TabTop+TabHeight||P.X<Origin.X+6)return INDEX_NONE;const int32 I=FMath::FloorToInt((P.X-Origin.X-6)/TabWidth);return I<FilterCount?I:INDEX_NONE;}
 bool SortButtonAt(FVector2D P) const {return P.Y>=Origin.Y+TabTop&&P.Y<Origin.Y+TabTop+TabHeight&&P.X>=Origin.X+Width-SortWidth-8&&P.X<Origin.X+Width-8;}
 bool Contains(FVector2D P) const;
 int32 RowAt(FVector2D P) const;
};

// Tactical minimap, shared by Canvas drawing and pointer input. World +X is up and +Y is right, like the tactical camera.
struct FBattleMinimapLayout
{
 FVector2D Origin=FVector2D::ZeroVector, Size=FVector2D::ZeroVector, HalfExtent=FVector2D(30000,20000);
 static FBattleMinimapLayout Make(FVector2D View,const UWorld* World);
 bool Contains(FVector2D P) const { return Size.X>0 && P.X>=Origin.X && P.X<Origin.X+Size.X && P.Y>=Origin.Y && P.Y<Origin.Y+Size.Y; }
 FVector2D ToScreen(FVector World) const { return FVector2D(Origin.X+(World.Y+HalfExtent.Y)/(2*HalfExtent.Y)*Size.X,Origin.Y+(HalfExtent.X-World.X)/(2*HalfExtent.X)*Size.Y); }
 FVector ToWorld(FVector2D P) const { return FVector(HalfExtent.X-(P.Y-Origin.Y)/Size.Y*2*HalfExtent.X,(P.X-Origin.X)/Size.X*2*HalfExtent.Y-HalfExtent.Y,0); }
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
 FVector LastProgressPosition=FVector::ZeroVector;
 double LastProgressTime=0;
 bool bTrafficWait=false, bArrived=false;
 int32 PassingID=INDEX_NONE;

 FVector PassingWaypoint=FVector::ZeroVector, PassingForward=FVector::ForwardVector, PassingOffset=FVector::ZeroVector;
 bool bPassingSecond=false;
 double EscapeUntil=0, NextEscapeProbe=0;
 // Map E: earliest time this member may ask for a route of his own again after being cut off from the shared corridor.
 double NextOwnPath=0;
 FVector EscapeWaypoint=FVector::ZeroVector;
 TArray<FVector> ContactRoute;
 double NextContactProbe=0, ContactRouteUntil=0, NextArrivalProbe=0;
 bool bArrivalOccupied=false, bContactWaiting=false;
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
 TMap<int32,FVector> ArrivalGoals;
 // Map E cover arrival: a member's own short route from the end of the shared path to a cover slot that the
 // destination cannot reach in a straight line. Rebuilt on demand (not saved).
 TMap<int32,TArray<FVector>> ArrivalTails;
 TMap<int32,TArray<FVector>> Corridors;
 int32 PathRequests=0, ArrivalReassignments=0, ContactProbes=0;
 int32 ArrivalRevision=INDEX_NONE;
 bool bNarrowCorridor=false;
};

UCLASS()
class UBattleLeaderRuntime : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool IsEnabled() const { return bEnabled; }
 bool IsWideField() const { return bWideField; }
 bool RangeAwareEnabled() const {return bRangeAwareCombat;}
 // Return fire (trial -BattleReturnFire=1, ROADMAP range table): a soldier hit within the last ReturnFireSeconds may engage a
 // contact it can currently see up to its weapon reach instead of the shorter AI engagement distance; its squad may pick such a target.
 // Only differs from the old rule where the range profile separates reach from engagement (map E).
 void NoteUnderFire(int32 ID){if(BattleTactics::ReturnFireEnabled())UnderFireUntil.Add(ID,GetWorld()->GetTimeSeconds()+5.0);}
 bool UnderFire(int32 ID) const {const double* T=UnderFireUntil.Find(ID);return T&&*T>GetWorld()->GetTimeSeconds();}
 TMap<int32,double> UnderFireUntil;
 // Squad target search range for one member: the AI engagement distance, or its weapon reach while under fire (return fire).
 float TargetRange(int32 ID) const;
 bool bMixedFastMatch=false;
 bool bArea=false;
 FName SnapshotMode() const {return bArea?TEXT("Area"):bMixedFastMatch?TEXT("MixedFast"):TEXT("Wide");}
 bool ReplaceFollower(int32 DeadID);
 // MixedFast or Area. bSelect selects a new team-0 leader (player purchase); OutSquadID receives the new squad.
 bool SpawnPurchasedSquad(int32 Team,EBattleTraining Training,FString& Error,bool bSelect=true,int32* OutSquadID=nullptr,uint8 SquadType=0);
 bool CreateBody(const FBattleSavedUnit& Entry,AActor* Host,FBattleLeaderBody& Body);
 void DestroyBody(FBattleLeaderBody& Body);
 bool RestoreMixedBodies(const UBattleSave* Save,FString& Error);
 void RunMixedSaveSmoke();
 void RunMixedFastSmoke();
 void RunAreaSmoke();
 static FVector LimitedSteering(FVector Desired, FVector Separation, float MaxSide);
 bool Start(ABattleGameMode& Mode);
 void Step(float DeltaTime);
 void RefreshDisplay();
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
 bool RequestCoverAction(int32 ID,FVector Threat,int32 Target=INDEX_NONE,float SearchRadius=-1.f);
 void CancelCoverAction(int32 ID);
 bool StepCoverAction(int32 ID,float Dt,double Now);
 void RunCoverActionSmoke();
 double NextCoverProbe=0;
 uint64 CoverProbeFrame=0;int32 CoverProbesThisFrame=0;
 TMap<int32,double> CoverProbeAfter;
 // Trial -BattleCoverApproach: runtime-only walk toward a hidden cover slot (not saved; cleared on restore).
 struct FCoverApproach{FVector Goal=FVector::ZeroVector;int32 Slot=INDEX_NONE,Target=INDEX_NONE,Revision=0;double Until=0;bool bKeptTargetChange=false;};
 TMap<int32,FCoverApproach> CoverApproaches;
 // -BattleCoverFire: shots fired during the current AI peek (runtime only; a restored peek starts from zero).
 TMap<int32,int32> CoverPeekShots;
 // Evaluation only: records how an AI peek ended (peek_end_*) and its shot count; removes the CoverPeekShots entry.
 void EndPeekDiag(int32 ID,const TCHAR* Cause);
 // Cover validation: a visibility line from a standing position at From to the observed threat position
 // (own capsule ignored; soldiers' bodies are transient and do not count as blocking).
 bool CoverLineClear(int32 ID,FVector From,FVector Threat) const;
 // Re-picks a peek point beside the cover anchor with a clear line to Threat; false keeps the soldier hidden.
 bool RevalidatePeek(int32 ID,FVector Threat);
 // Suppression trial (-BattleSuppression=1, see BattleSuppression.h). Runtime only: time at which each unit's level decays to zero.
 TMap<int32,double> SuppressionZeroAt;
 float Suppression(int32 ID,double Now) const;
 bool IsPinned(int32 ID,double Now) const;
 float SuppressionSpreadScale(int32 ID,double Now) const;
 void AddSuppression(int32 ID,float Amount,double Now);
 // Suppresses living enemies of ShooterTeam within the tuning radius of the travelled bullet path Start..End.
 void SuppressNear(int32 ShooterTeam,FVector Start,FVector End,double Now,float Scale=1.f);
 // Stances (BattleStance.h; map E default). Runtime only: 0 stand, 1 crouch, 2 prone, and the earliest time each may change again.
 TMap<int32,uint8> Stances;
 TMap<int32,double> StanceLockedUntil;
 uint8 StanceOf(int32 ID) const {const uint8* Found=Stances.Find(ID);return Found?*Found:0;}
 bool SetStance(int32 ID,uint8 Stance,double Now,bool bForce=false);
 void UpdateAIStance(int32 ID,double Now,bool bMoving,bool bCombat,bool bLostSight);
 // Concealment (hedges tagged BattleConcealment in the map): blocks sight but shots ignore it. Collected once per world.
 TArray<AActor*> ConcealmentActors;
 bool bConcealmentBuilt=false;
 bool StartCoverApproach(int32 ID,FVector Threat,int32 Target,double Now,float LocalRadius);
 bool StepCoverApproach(int32 ID,float Dt,double Now,int32 Target);
 bool IssueOrder(int32 SquadID,EBattleOrder Order,FVector Goal,bool bMissionUpdate=false);
 bool CanSelectLeader(int32 ID) const;
 bool CanControlLeader(int32 ID) const;
 const TArray<FBattleSquadSummary>& Summaries();
 // Current squad target (a contact the squad engages), or INDEX_NONE. For the command UI squad list.
 int32 SquadTargetID(int32 SquadID) const {const auto* P=Plans.Find(SquadID);return P?P->TargetID:INDEX_NONE;}
 // Squad plan (arrival goals per member) for the command UI ground marks. Read only.
 const FBattleLeaderPlan* PlanOf(int32 SquadID) const {return Plans.Find(SquadID);}
 void OnDeath(int32 ID);
 void RunSmoke();
 void RunUIQA();
 void RunWideSmoke();
 void RunWideSoak();
 void RunST2Smoke();
 void RunSupportSmoke();
 void RunFlankSmoke();
 void RunFlankBayTrial();
 double BayStart=0, BayArrival=-1, BayMovingDamage=0, BayLastHP=0;
 int32 BayStage=0;
 double BayDamageGiven[2]={0,0}, BayMoveDirectDamage=0;
 FVector FlankSmokeGoal=FVector::ZeroVector;
 void RunMovementTrial();
 void RunContactTrial();
 void RunSightTrial();
 void RunWeaponRangeTrial();
 void RunWideMapAudit();
 void RunSightVisual();
 int32 SightVisualStage=0;
 void StepSightExperiment(float DeltaSeconds);
 FBattleSightExperiment SightExperiment;
 int32 MapAuditStage=0;
 double MapAuditStart=0;
 int32 SightTrialCase=0, SightTrialTick=0;
 int32 SightTrialAmmo=0;
 FVector SightTrialStart=FVector::ZeroVector;
 FString MovementLabel(int32 SquadID) const;
 class ABattleVATDisplay* VATDisplay() const {return VAT;}
private:
 friend class UBattlePhase3;
 friend struct FBattleMixedUnit;
 bool bEnabled=false, bSummaryDirty=true, bReduceHiddenBones=false, bReduceAnimTasks=false;
 bool bWideField=false;
 bool bDisplayLODEnabled=true;
 int32 RestoreStageFailureAfter=INDEX_NONE; // Exercised only by BattleMixedSaveSmoke.
 bool bRangeAwareCombat=true; // BattleRangeAware=0 retains the legacy trial baseline.
 bool bStableMovement=true, bMovementTrial=false, bContactTrial=false, bContactAvoidance=true;
 bool ContactBlockersHeld=true, ContactWaitObserved=false, ContactReleased=false;
 TMap<int32,FVector> ContactBlockerStarts;
 int32 MoveTrialScene=0, MoveTrialStage=0;
 int32 MoveTrialOnly=-1;
 double MoveTrialStart=0, MoveTrialStall=0;
 double MoveTrialMaxDeviation=0;
 double MoveTrialMinSpacing=DBL_MAX;
 TArray<float> MoveTrialFrameMS;
 TArray<int32> MoveTrialIDs;
 TMap<int32,FVector> MoveTrialStarts, MoveTrialGoals;
 TMap<int32,double> MoveTrialArrivals;
 TArray<TWeakObjectPtr<AActor>> MoveTrialWalls;
 int32 WideStage=0;
 FVector WideStart=FVector::ZeroVector;
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
 void BuildMovementPlan(int32 SquadID);
 bool StableMove(int32 ID,float Dt,double Now,FVector& Direction);
 bool IsStationaryAlly(int32 Observer,int32 Other) const;
 void ReassignOccupiedArrival(int32 ID,double Now);
 bool ResolveStoppedContact(int32 ID,FVector Way,double Now,FVector& Direction,float& Scale);
 bool ClearMovementSegment(FVector A,FVector B) const;
};
