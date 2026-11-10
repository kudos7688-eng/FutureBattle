#pragma once
#include "BattlePhase3.h"
#include "BattleUnit.h"
#include "Navigation/PathFollowingComponent.h"

// Value adapter, NOT an Actor. The decision implementation below is ported from
// BattleAIController/BattleTacticalAI; movement is the common mixed executor.
struct FBattleMixedUnit
{
 UBattlePhase3* P=nullptr; int32 ID=INDEX_NONE;
 UWorld* GetWorld() const { return P->GetWorld(); }
 int32 GetUnitID() const { return ID; }
 const FBattleUnitState* GetState() const { return GetWorld()->GetSubsystem<UBattleState>()->Find(ID); }
 bool IsAIActive() const;
 bool CanSee(const FBattleMixedUnit* Other) const;
 bool IsAlive() const { return GetState() && GetState()->IsAlive(); }
 FVector GetActorLocation() const;
 FVector GetActorForwardVector() const;
 FRotator GetActorRotation() const { return GetActorForwardVector().Rotation(); }
 FVector GetVelocity() const { return P->Velocity(ID); }
 FVector EyePosition() const { return GetActorLocation()+FVector(0,0,60); }
 void SetActorRotation(FRotator R) { P->Rotate(ID,R); }
 FBattleSightContext CaptureSightContext() const;
 bool CanSeeSnapshot(const FBattleSightContext&,const FBattleMixedUnit* Other,FVector,bool) const;
 bool FireAt(FVector Start,FVector Direction,bool Player);
};
struct FBattleMixedDriver
{
 FBattleMixedUnit Proxy;
 double NextControllerTick=0,NextDecision=0,NextMoveRequest=0,LastProgressTime=0;
 FVector LastProgressPosition=FVector::ZeroVector,ExecutionGoal=FVector::ZeroVector;
 int32 ActiveRevision=INDEX_NONE,ActiveCoordinationRevision=INDEX_NONE,ActiveDecisionRevision=INDEX_NONE;
 bool bTrackingMove=false,bRelaxed=false,bFailed=false;
 int32 CurrentCoverSlot=INDEX_NONE;
 double NextTacticalDecision=0,TacticalCommitUntil=0,TacticalBlockedUntil=0;
 bool bCoverForReload=false;
 UWorld* GetWorld() const { return Proxy.GetWorld(); }
 void StopMovement() { Proxy.P->Stop(Proxy.ID); }
 void ClearFocus(EAIFocusPriority::Type) {}
 void SetFocalPoint(FVector P) { Proxy.SetActorRotation((P-Proxy.GetActorLocation()).Rotation()); }
 EPathFollowingStatus::Type GetMoveStatus() const;
 EPathFollowingRequestResult::Type MoveToLocation(FVector Goal,float,bool,bool,bool,bool,void*,bool);
 void ResetExecution(); void Tick(float Dt); void SuspendMovement();
 void ExecuteMove(FBattleMixedUnit*,FVector,EBattleAction,double);
 void RetryMove(FBattleMixedUnit*,double,FVector,FName);
 bool TryTacticalAction(FBattleMixedUnit*,const FBattleContact*,double,bool);
 void ClearCoverPlan();
};

namespace MixedNavigation { bool ResolveGoal(FBattleMixedUnit*,FVector,FVector&); bool IsPathWithinDefense(FBattleMixedUnit*,FVector); }
