#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "BattleFieldLayout.h"
#include "BattlePerception.h"
#include "BattleGameMode.h"
#include "BattleVATDisplay.h"
#include "BattleSquadTactics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::CancelCoverAction(int32 ID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U)return;
 if(U->CoverPhase==4)EndPeekDiag(ID,U->IsAlive()?TEXT("cancel"):TEXT("died"));
 if(U->CoverPhase){GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_RELEASE unit=%d phase=%d position=%s remaining=%.2f revision=%d/%d"),ID,U->CoverPhase,*Position(ID).ToCompactString(),U->CoverSeconds,U->OrderRevision,U->CoverOrderRevision);}
 U->CoverPhase=0;U->CoverSeconds=0;U->CoverTargetID=INDEX_NONE;U->CoverSlot=INDEX_NONE;
}

void UBattleLeaderRuntime::EndPeekDiag(int32 ID,const TCHAR* Cause)
{
 int32 Shots=0;if(!CoverPeekShots.RemoveAndCopyValue(ID,Shots))return;
 SightExperiment.Cover(*FString::Printf(TEXT("peek_end_%s"),Cause));
 SightExperiment.Cover(Shots<=0?TEXT("peek_end_shots_0"):Shots==1?TEXT("peek_end_shots_1"):TEXT("peek_end_shots_2plus"));
}

bool UBattleLeaderRuntime::CoverLineClear(int32 ID,FVector From,FVector Threat) const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CoverPeekLine),false);
 if(const auto* B=Bodies.Find(ID))Q.AddIgnoredComponent(B->Capsule.Get());
 FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,From+FVector(0,0,60),Threat+FVector(0,0,35),ECC_Visibility,Q);
 return !Hit.bBlockingHit || HitIDs.Contains(Hit.GetComponent());
}

bool UBattleLeaderRuntime::RevalidatePeek(int32 ID,FVector Threat)
{
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);if(!U)return false;
 if(CoverLineClear(ID,U->CoverPeek,Threat))return true;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 const FVector Anchor=U->CoverAnchor,Lateral=FVector::CrossProduct((Threat-Anchor).GetSafeNormal2D(),FVector::UpVector);
 for(float Offset:{-220.f,220.f,-400.f,400.f})
 {
  FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor+Lateral*Offset-FVector(0,0,90),Projected,FVector(50,50,150)))continue;
  const FVector Peek=Projected.Location+FVector(0,0,90);
  if(GetWorld()->SweepTestByObjectType(Anchor,Peek,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88)))continue;
  if(!CoverLineClear(ID,Peek,Threat))continue;
  U->CoverPeek=Peek;SightExperiment.Cover(TEXT("peek_repicked"));return true;
 }
 return false;
}

bool UBattleLeaderRuntime::RequestCoverAction(int32 ID,FVector Threat,int32 Target,float SearchRadius)
{
 if(!bEnabled || !BattleTactics::CoverActionsEnabled() || !Bodies.Contains(ID) || Threat.ContainsNaN())return false;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase)return false;
 const auto* Squad=Store->FindSquad(U->SquadID);if(!Squad || Squad->CommandStatus!=EBattleCommandStatus::Active)return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 const FVector Pos=Position(ID),Eye=Threat+FVector(0,0,60);const double Now=GetWorld()->GetTimeSeconds();
 const FVector Lateral=FVector::CrossProduct((Threat-Pos).GetSafeNormal2D(),FVector::UpVector);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 auto Clear=[&](FVector A,FVector B){return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88));};
 const auto* Tuning=GetDefault<UBattleTuning>();
 // Evaluation-only funnel: furthest stage reached by this request (does not alter the search order or result).
 auto& Diag=SightExperiment;int32 Stage=0;
 static const TCHAR* StageKeys[]={TEXT("request_no_slot_in_radius"),TEXT("request_slot_reserved"),TEXT("request_path_to_anchor_blocked"),TEXT("request_anchor_too_exposed"),TEXT("request_no_valid_peek"),TEXT("request_reserve_failed")};
 Diag.Cover(TEXT("request_total"));
 for(int32 Slot:Cover->NearbySlots(Pos,FMath::Max(0.f,SearchRadius>=0?SearchRadius:Tuning->CoverActionSearchRadius),BattleFieldLayout::IsUrban(GetWorld())?12:FMath::Max(1,Tuning->CoverActionCandidateLimit)))
 {
  const FVector Anchor=Cover->GetSlots()[Slot]+FVector(0,0,90);
  Stage=FMath::Max(Stage,1);if(!Cover->IsAvailable(Slot,ID,Now))continue;
  Stage=FMath::Max(Stage,2);if(!Clear(Pos,Anchor))continue;
  Stage=FMath::Max(Stage,3);
  const float Hidden=BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction();
  if(Hidden>.6f)continue;
  Stage=FMath::Max(Stage,4);
  // Stances: low cover (a wall, a car, sandbags) is used by crouching behind it and standing up to fire, with no step
  // sideways. It qualifies when the threat cannot see the top of a crouched soldier here but a standing one can see the threat.
  if(BattleStance::Enabled(GetWorld()) && !BattleTactics::HasTerrainLine(GetWorld(),Anchor+FVector(0,0,BattleStance::TopOffset(BattleStance::Crouch)),Eye)
     && BattleTactics::HasTerrainLine(GetWorld(),Anchor+FVector(0,0,60),Eye))
  {
   Stage=FMath::Max(Stage,5);
   if(!Cover->Reserve(Slot,ID,Now))continue;
   Diag.Cover(TEXT("request_success"));Diag.Cover(TEXT("request_success_stand_up"));
   U->CoverAnchor=Anchor;U->CoverPeek=Anchor;U->CoverSlot=Slot;U->CoverPhase=1;U->CoverSeconds=3;
   U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=Target;U->DecisionReason=TEXT("COVER / CROUCH BEHIND LOW COVER");
   return true;
  }
  for(float Offset:{-220.f,220.f,-400.f,400.f})
  {
   FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor+Lateral*Offset-FVector(0,0,90),Projected,FVector(50,50,150))){Diag.Cover(TEXT("peek_off_navmesh"));continue;}
   const FVector Peek=Projected.Location+FVector(0,0,90);
   if(!Clear(Anchor,Peek)){Diag.Cover(TEXT("peek_path_blocked"));continue;}
   if(BattleTactics::ExposureAt(GetWorld(),Peek,Forward(ID),Eye).Fraction()<Hidden+.39f){Diag.Cover(TEXT("peek_not_exposed_enough"));continue;}
   if(!BattleTactics::HasTerrainLine(GetWorld(),Peek+FVector(0,0,60),Eye)){Diag.Cover(TEXT("peek_no_line_to_threat"));continue;}
   Stage=FMath::Max(Stage,5);
   if(!Cover->Reserve(Slot,ID,Now))continue;
   Diag.Cover(TEXT("request_success"));
   U->CoverAnchor=Anchor;U->CoverPeek=Peek;U->CoverSlot=Slot;U->CoverPhase=1;U->CoverSeconds=3;
   U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=Target;U->DecisionReason=TEXT("COVER / ENTER PHYSICAL SHELTER");
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_ACTION unit=%d slot=%d hidden=%.2f peek_cm=%.1f"),ID,Slot,Hidden,FVector::Dist2D(Anchor,Peek));return true;
  }
 }
 Diag.Cover(StageKeys[Stage]);
 return false;
}

bool UBattleLeaderRuntime::StartCoverApproach(int32 ID,FVector Threat,int32 Target,double Now,float LocalRadius)
{
 auto& Diag=SightExperiment;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase || Threat.ContainsNaN() || !Bodies.Contains(ID))return false;
 const auto* Squad=Store->FindSquad(U->SquadID);if(!Squad || Squad->Order==EBattleOrder::Hold || Squad->Order==EBattleOrder::Move)return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 const auto* T=GetDefault<UBattleTuning>();
 const FVector Pos=Position(ID),Eye=Threat+FVector(0,0,60);
 const double Near=FMath::Max(0.f,LocalRadius),Base=FVector::Dist2D(Pos,Threat);
 auto Clear=[&](FVector A,FVector B){return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88));};
 const int32 Limit=FMath::Max(1,T->AICoverApproachCandidates);int32 Evaluated=0;
 for(int32 Slot:Cover->NearbySlots(Pos,FMath::Max(0.f,T->AICoverApproachRadius),Limit+32))
 {
  const FVector Anchor=Cover->GetSlots()[Slot]+FVector(0,0,90);
  if(FVector::Dist2D(Pos,Anchor)<=Near)continue; // Already covered by the local cover search.
  if(++Evaluated>Limit)break;
  if(!Cover->IsAvailable(Slot,ID,Now))continue;
  bool Taken=false;for(const auto& Other:CoverApproaches)if(Other.Key!=ID && Other.Value.Slot==Slot){Taken=true;break;}
  if(Taken)continue;
  if(FVector::Dist2D(Anchor,Threat)<Base-T->AICoverApproachMaxAdvance)continue;
  if(Squad->Order==EBattleOrder::Defend && FVector::Dist2D(Anchor,Squad->Destination)>Squad->DefenseRadius)continue;
  if(!Clear(Pos,Anchor))continue;
  if(BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction()>.6f)continue;
  FCoverApproach A;A.Goal=Anchor;A.Slot=Slot;A.Target=Target;A.Revision=U->OrderRevision;A.Until=Now+FMath::Max(0.f,T->AICoverApproachSeconds);
  CoverApproaches.Add(ID,A);Diag.Cover(TEXT("approach_started"));return true;
 }
 Diag.Cover(TEXT("approach_none_found"));return false;
}

bool UBattleLeaderRuntime::StepCoverApproach(int32 ID,float Dt,double Now,int32 Target)
{
 auto* A=CoverApproaches.Find(ID);if(!A)return false;
 auto& Diag=SightExperiment;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,A->Target);
 const FVector Pos=Position(ID);const TCHAR* End=nullptr;
 if(!U || !U->IsAlive())End=TEXT("approach_cancel_died");
 else if(U->OrderRevision!=A->Revision)End=TEXT("approach_cancel_new_order");
 else if(Target==INDEX_NONE)End=TEXT("approach_cancel_disengaged");
 // The squad re-picks its target every think; keep walking while this soldier still sees its own threat.
 else if(Target!=A->Target && !(Contact && Contact->bVisible))End=TEXT("approach_cancel_target_changed");
 else if(!Contact || Now-Contact->LastSeenTime>FMath::Min(5.f,U->Perception.MemorySeconds))End=TEXT("approach_cancel_contact_stale");
 else if(!GetWorld()->GetSubsystem<UBattleCover>()->IsAvailable(A->Slot,ID,Now))End=TEXT("approach_cancel_slot_taken");
 else if(FVector::Dist2D(Pos,A->Goal)<120){End=TEXT("approach_arrived");CoverProbeAfter.Remove(ID);}
 else if(Now>A->Until)End=TEXT("approach_cancel_timeout");
 if(End){Diag.Cover(End);CoverApproaches.Remove(ID);return false;}
 if(Target!=A->Target && !A->bKeptTargetChange){A->bKeptTargetChange=true;Diag.Cover(TEXT("approach_kept_after_target_change"));}
 auto& B=Bodies.FindChecked(ID);const FVector D=(A->Goal-Pos).GetSafeNormal2D();
 if(B.Actor.IsValid()){B.Actor->AddMovementInput(D);B.Velocity=B.Actor->GetVelocity();}
 else
 {
  FHitResult Hit;B.Capsule->MoveComponent(D*FMath::Min(GetDefault<UBattleTuning>()->MoveSpeed*Dt,float(FVector::Dist2D(Pos,A->Goal))),B.Capsule->GetComponentQuat(),true,&Hit);
  auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=B.Capsule->GetComponentLocation();B.Velocity=(F.Position-Pos)/FMath::Max(.001f,Dt);
 }
 return true;
}

bool UBattleLeaderRuntime::StepCoverAction(int32 ID,float Dt,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U || !U->CoverPhase)return false;
 auto* Squad=Store->FindSquad(U->SquadID);auto& B=Bodies.FindChecked(ID);
 const bool Player=B.Actor.IsValid()&&B.Actor->IsPlayerControlled();
 auto& Diag=SightExperiment;
 if(!BattleTactics::CoverActionsEnabled() || !U->IsAlive() || !Squad || Squad->CommandStatus!=EBattleCommandStatus::Active || U->OrderRevision!=U->CoverOrderRevision || (!Player && U->Order==EBattleOrder::Move))
 {Diag.Cover(!U->IsAlive()?TEXT("cancel_died"):(!Squad || Squad->CommandStatus!=EBattleCommandStatus::Active)?TEXT("cancel_squad_inactive"):TEXT("cancel_new_order"));CancelCoverAction(ID);return false;}
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 // Reservations are rebuilt after loading, against saved positions in this same map.
 if(U->CoverSlot==INDEX_NONE)for(int32 Slot:Cover->NearbySlots(U->CoverAnchor,120,4))if(FVector::Dist2D(Cover->GetSlots()[Slot],U->CoverAnchor)<100 && Cover->Reserve(Slot,ID,Now)){U->CoverSlot=Slot;break;}
 if(!Cover->Reserve(U->CoverSlot,ID,Now)){Diag.Cover(TEXT("cancel_slot_lost"));CancelCoverAction(ID);return false;}
 U->CoverSeconds=FMath::Max(0.f,U->CoverSeconds-Dt);
 const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,U->CoverTargetID);
 if(!Player && (!Contact || Now-Contact->LastSeenTime>FMath::Min(5.f,U->Perception.MemorySeconds))){Diag.Cover(U->CoverPhase==1?TEXT("cancel_contact_stale_entering"):TEXT("cancel_contact_stale_in_cover"));CancelCoverAction(ID);return false;}
 // Cover validation (default ON): leave cover once the observed threat is beyond weapon range, and only step out to a
 // peek point with a clear line to the threat's last known position; otherwise stay hidden and check again shortly.
 const bool Validate=!Player && BattleTactics::CoverValidationEnabled();
 if(Validate && FVector::Dist2D(Position(ID),Contact->LastKnownPosition)>TargetRange(ID))
 {Diag.Cover(TEXT("cancel_target_out_of_range"));CancelCoverAction(ID);return false;}
 if(U->CoverPhase==2 && !Player && U->CoverSeconds<=0 && U->ReloadEndTime==0)
 {
  // Suppression trial: a pinned AI soldier stays behind cover and checks again shortly.
  if(IsPinned(ID,Now)){U->CoverSeconds=.5f;Diag.Cover(TEXT("peek_hold_suppressed"));}
  else if(Validate && !RevalidatePeek(ID,Contact->LastKnownPosition)){U->CoverSeconds=.5f;Diag.Cover(TEXT("peek_hold_no_line"));}
  else{U->CoverPhase=3;U->CoverSeconds=3;}
 }
 // -BattleCoverFire=1: an AI peek ends after AICoverShotsPerPeek shots instead of a fixed 1.2s that is shorter than the AI shot interval.
 const bool AIPeekFire=!Player && BattleTactics::CoverFireEnabled();
 if(U->CoverPhase==4)
 {
  const TCHAR* PeekEnd=U->ReloadEndTime>0?TEXT("reload"):(!Player && IsPinned(ID,Now))?TEXT("suppressed"):(AIPeekFire && CoverPeekShots.FindRef(ID)>=FMath::Max(1,GetDefault<UBattleTuning>()->AICoverShotsPerPeek))?TEXT("shots_done"):U->CoverSeconds<=0?TEXT("timer"):
   (Validate && !CoverLineClear(ID,Position(ID),Contact->LastKnownPosition))?TEXT("no_line"):
   // -BattleCoverPeekSight=1: stop exposing once the blind time has passed at the peek point without an own sighting.
   (AIPeekFire && BattleTactics::CoverPeekSightEnabled() && !(Contact->bVisible && !Contact->bShared) &&
    FMath::Clamp(GetDefault<UBattleTuning>()->AICoverPeekSeconds,.1f,3.f)-U->CoverSeconds>=GetDefault<UBattleTuning>()->AICoverPeekBlindSeconds)?TEXT("blind"):nullptr;
  if(PeekEnd){EndPeekDiag(ID,PeekEnd);U->CoverPhase=1;U->CoverSeconds=3;}
 }
 B.Velocity=FVector::ZeroVector;
 if(U->CoverPhase==1 || U->CoverPhase==3)
 {
  const FVector Goal=U->CoverPhase==1?U->CoverAnchor:U->CoverPeek;
  const FVector Before=Position(ID),D=(Goal-Before).GetSafeNormal2D();
  if(FVector::Dist2D(Before,Goal)<40)
  {U->CoverPhase=U->CoverPhase==1?2:4;U->CoverSeconds=U->CoverPhase==2?.8f:AIPeekFire?FMath::Clamp(GetDefault<UBattleTuning>()->AICoverPeekSeconds,.1f,3.f):1.2f;if(U->CoverPhase==4 && !Player)
   {
    CoverPeekShots.Add(ID,0);
    // Evaluation only: what the AI knew and whether its weapon was ready when it reached the peek point.
    Diag.Cover(!Contact?TEXT("peek_start_no_contact"):Contact->bVisible&&!Contact->bShared?TEXT("peek_start_visible"):Now-Contact->LastSeenTime<=GetDefault<UBattleTuning>()->ReacquireGraceSeconds?TEXT("peek_start_seen_within_grace"):TEXT("peek_start_seen_before_grace"));
    if(U->NextShotTime>Now)Diag.Cover(TEXT("peek_start_cooldown_pending"));
   }
   if(!Player)Diag.Cover(U->CoverPhase==2?TEXT("reached_hidden"):TEXT("reached_peek"));if(B.Actor.IsValid())B.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else if(U->CoverSeconds<=0){Diag.Cover(U->CoverPhase==1?TEXT("cancel_move_timeout_to_hidden"):TEXT("cancel_move_timeout_to_peek"));CancelCoverAction(ID);return false;}
  else
  {
   if(B.Actor.IsValid()){B.Actor->AddMovementInput(D);B.Velocity=B.Actor->GetVelocity();}
   else {FHitResult Hit;B.Capsule->MoveComponent(D*FMath::Min(250.f*Dt,float(FVector::Dist2D(Before,Goal))),B.Capsule->GetComponentQuat(),true,&Hit);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=B.Capsule->GetComponentLocation();B.Velocity=(F.Position-Before)/FMath::Max(.001f,Dt);}
  }
 }
 else if(B.Actor.IsValid())B.Actor->GetCharacterMovement()->StopMovementImmediately();
 if(!Player && Contact)
 {
  const FVector Aim=(Contact->LastKnownPosition+FVector(0,0,35)-(Position(ID)+FVector(0,0,60))).GetSafeNormal();
  if(B.Actor.IsValid())B.Actor->SetActorRotation(FRotator(0,Aim.Rotation().Yaw,0));
  else {Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=Aim.GetSafeNormal2D();B.Capsule->SetWorldRotation(FRotator(0,Aim.Rotation().Yaw,0));}
  Store->UpdateAim(ID,Now,B.Velocity.Size2D(),Aim,true);
  const bool InRange=FVector::Dist2D(Position(ID),Contact->LastKnownPosition)<=(UnderFire(ID)?BattleRanges::Reach(U->Weapon):BattleRanges::Engage(U->Weapon));
  if(U->CoverPhase==4 && Contact->bVisible && !Contact->bShared && InRange && CanSee(ID,U->CoverTargetID))
  {
   if(U->Ammo==0){Store->StartReload(ID,Now);Diag.Cover(TEXT("peekwait_reloading"));}
   else
   {
    const bool Shot=Fire(ID,Position(ID)+FVector(0,0,60),Aim,false);
    if(Shot){Diag.Cover(TEXT("cover_shots"));if(++CoverPeekShots.FindOrAdd(ID)==1)Diag.Cover(TEXT("peek_first_shot"));}
    else Diag.Cover(U->ReloadEndTime>0?TEXT("peekwait_reloading"):Now<U->NextShotTime?TEXT("peekwait_cooldown"):TEXT("peekwait_fire_other"));
    if(Shot && Squad->TacticalPlan.Action==EBattleSquadTactic::Support)GetWorld()->GetSubsystem<UBattleSquadTactics>()->RecordSupportShot(Squad->SquadID,U->CoverTargetID);
   }
  }
  // Evaluation only (never AI input): why this AI peek frame could not fire. The extra line-of-sight check runs only in experiments.
  else if(U->CoverPhase==4 && Diag.Enabled)
   Diag.Cover(!Contact->bVisible?(CanSee(ID,U->CoverTargetID)?TEXT("peekwait_detecting"):TEXT("peekwait_no_line_of_sight")):Contact->bShared?TEXT("peekwait_shared_only"):!InRange?TEXT("peekwait_out_of_range"):TEXT("peekwait_ray_blocked"));
 }
 else if(!Player && U->CoverPhase==4)Diag.Cover(TEXT("peekwait_no_contact"));
 if(U->CoverPhase==2 && U->Ammo==0)Store->StartReload(ID,Now);
 if(!B.Actor.IsValid())
 {
  const int32 Pose=U->ReloadEndTime>Now?2:B.Velocity.SizeSquared2D()>400?1:0;
  if(Pose!=B.Pose){B.Pose=Pose;if(VAT)VAT->ChangePose(ID,Pose);if(!VAT || !VAT->IsFar(ID))B.Mesh->PlayAnimation(FollowerClip(ID,Pose),true);}
 }
 U->DecisionReason=U->CoverPhase==2?TEXT("COVER / HIDDEN - FIRE TO PEEK"):U->CoverPhase==4?TEXT("COVER / EXPOSED FIRING WINDOW"):TEXT("COVER / MOVING - FIRE BLOCKED");
 if(!Player)U->Action=U->CoverPhase==2?EBattleAction::TakingCover:U->CoverPhase==4?EBattleAction::Engaging:EBattleAction::Moving;
 // Stances: crouch while hidden behind cover, stand to move and to fire from the peek point.
 // (Direct control too: behind low cover the player is only hidden while crouched.)
 SetStance(ID,U->CoverPhase==2?BattleStance::Crouch:BattleStance::Stand,Now,true);
 return true;
}
