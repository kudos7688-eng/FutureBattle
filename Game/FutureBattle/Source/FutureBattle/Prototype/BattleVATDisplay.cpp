#include "BattleVATDisplay.h"
#include "BattleVATData.h"
#include "BattleLeaderRuntime.h"
#include "BattleFollowerBones.h"
#include "BattleStance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "BattlePhase3.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "Camera/PlayerCameraManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#endif

// Trial -BattleVATFarLight=1 (2026-11-18, default 0): a far follower's skeletal mesh stops ticking and is detached from its
// capsule while the VAT instance stands in for it. The presenter advances the clip time in one loop, and moving the capsule
// no longer recomputes the hidden mesh's transform. Measured on map E (rendered, 310 far followers): about 0.1 ms per frame,
// and the match result changes (9,846 -> 9,900 shots), so it stays off. Suspected cause, not verified: enabling and
// disabling component ticks changes the order in which later-registered ticks (hired leaders' movement) run.
static bool FarLight()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleVATFarLight="),V);return V!=0;}();
 return bOn;
}
// Trial -BattleFollowerAnimSkip=1 (2026-11-18, default 0, display only): a near follower's skeleton is evaluated every
// frame only at mesh LOD 0; every 2nd frame at LOD 1, every 3rd beyond, every 4th while not on screen (staggered by unit).
// The engine's own update-rate optimisation shares one rate per owning actor, and all followers share one host actor.
// Trial -BattleFollowerAnimBudget=K (2026-11-18, default 0 = none, display only): at most about K follower skeleton
// evaluations per frame. When the skip rule above would evaluate more, every interval is stretched by the same whole
// factor (up to 8x; LOD 0 by half of it), so the cost stays level as the number of near followers grows. Implies the skip rule.
static int32 AnimBudget()
{
 static const int32 K=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleFollowerAnimBudget="),V);return FMath::Max(0,V);}();
 return K;
}
static bool AnimSkip()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleFollowerAnimSkip="),V);return V!=0 || AnimBudget()>0;}();
 return bOn;
}
void UBattleVATSkeletalMeshComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
#if WITH_EDITOR
 if(GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->IsCompiling())return;
#endif
 if(VATFar && Presenter.IsValid())
 {if(ShouldTickPose() && ShouldTickAnimation())Presenter->Advance(UnitID,Dt);}
 else
 {
  if(AnimSkip())
  {
   // Evaluate every Nth frame and hold the pose in between (no interpolation: that would keep the per-frame work).
   // The clip time still advances by the full elapsed time on the frames that do evaluate.
   const int32 LOD=GetPredictedLODLevel();
   const int32 Base=!bRecentlyRendered?4:LOD<=0?1:LOD==1?2:3;
   int32 Stretch=1;
   if(AnimBudget()>0 && Presenter.IsValid())
   {
    // Demand = evaluations per frame the skip rule alone would do, measured over the previous frame.
    auto* P=Presenter.Get();
    if(P->AnimDemandFrame!=GFrameCounter)
    {
     P->AnimStretch=FMath::Clamp(FMath::CeilToInt32(P->AnimDemand/float(AnimBudget())),1,8);
     P->StatAnimStretch+=P->AnimStretch;++P->StatAnimStretchFrames;
     P->AnimDemand=0;P->AnimDemandFrame=GFrameCounter;
    }
    P->AnimDemand+=1.f/Base;Stretch=P->AnimStretch;
   }
   const int32 N=Base==1?FMath::Max(1,Stretch/2):Base*Stretch;
   const bool bUpdate=N<=1 || (GFrameCounter+uint64(UnitID))%uint64(N)==0;
   if(ShouldTickPose())PendingDt+=Dt;
   EnableExternalTickRateControl(true);EnableExternalEvaluationRateLimiting(N>1);EnableExternalInterpolation(false);
   EnableExternalUpdate(bUpdate);SetExternalDeltaTime(PendingDt);
   if(bUpdate)PendingDt=0;
   if(Presenter.IsValid()){++Presenter->StatAnimTicks;Presenter->StatAnimEvals+=bUpdate;}
  }
  Super::TickComponent(Dt,Type,Function);HasNativeTick|=PoseTickedThisFrame();NativeTickFrame=GFrameCounter;
 }
}
// Hands the body back to the engine's own per-frame update (far display, death, or a pose that must be current now).
void UBattleVATSkeletalMeshComponent::EndAnimSkip()
{
 EnableExternalTickRateControl(false);EnableExternalEvaluationRateLimiting(false);EnableExternalUpdate(false);PendingDt=0;
}
// One advance per frame for a far body whose own tick is off. A body that enters or leaves the far display earlier in
// the frame than the presenter's tick (Phase 3 selects from the game mode tick) still gets exactly one advance.
void ABattleVATDisplay::AdvanceFar(FBattleVATEntry& E,float Dt)
{
 if(E.AdvancedFrame==GFrameCounter)return;
 E.AdvancedFrame=GFrameCounter;
 const auto* M=E.Body.Get();
 if(!E.Dead && M && M->ShouldTickPose() && M->ShouldTickAnimation())FAnimationRuntime::AdvanceTime(true,(E.Rate*Data->Clips[E.Pose].Clip->RateScale)*Dt,E.Time,Data->Clips[E.Pose].Length);
}
bool UBattleVATSkeletalMeshComponent::ShouldCreateRenderState()const{return !VATFar && Super::ShouldCreateRenderState();}
// Runs after the pose is evaluated and before the engine flips it to the shown buffer; the rifle and other socket children
// follow afterwards (UpdateChildTransforms). Only a newly made pose is bent (bNeedToFlipSpaceBaseBuffers): a held pose
// (skipped frame) was bent when it was made, and bending it again would add up. Single-buffered bones are not bent: on a
// skipped frame the engine copies the unbent cached pose straight into the shown buffer (the bend would flicker).
void UBattleVATSkeletalMeshComponent::FinalizeBoneTransform()
{
 if(bNeedToFlipSpaceBaseBuffers)AppliedSpinePitch=0.f;
 if(bNeedToFlipSpaceBaseBuffers && bDoubleBufferedComponentSpaceTransforms && FMath::Abs(SpinePitch)>.01f && !VATFar && GetSkeletalMeshAsset())
 {
  if(BendMesh!=GetSkeletalMeshAsset())
  {
   BendMesh=GetSkeletalMeshAsset();
   const FReferenceSkeleton& Ref=GetSkeletalMeshAsset()->GetRefSkeleton();
   BendRoot=Ref.FindBoneIndex(TEXT("spine_01"));
   BendBones.Init(false,Ref.GetNum());
   // Parents come before their children in the reference skeleton.
   if(BendRoot!=INDEX_NONE)
   {
    BendBones[BendRoot]=true;
    for(int32 I=BendRoot+1;I<Ref.GetNum();++I){const int32 P=Ref.GetParentIndex(I);if(P!=INDEX_NONE && BendBones[P])BendBones[I]=true;}
   }
  }
  TArray<FTransform>& Pose=GetEditableComponentSpaceTransforms();
  if(BendRoot!=INDEX_NONE && Pose.IsValidIndex(BendRoot))
  {
   // The mesh's forward is +Y and its right is +X: a turn about +X by a positive angle raises the forward toward +Z.
   const FQuat Bend(FVector::XAxisVector,FMath::DegreesToRadians(SpinePitch));
   const FVector Pivot=Pose[BendRoot].GetLocation();
   for(const FBoneIndexType I:FillComponentSpaceTransformsRequiredBones)
    if(BendBones.IsValidIndex(I) && BendBones[I] && Pose.IsValidIndex(I))
    {
     FTransform& T=Pose[I];
     T.SetLocation(Pivot+Bend.RotateVector(T.GetLocation()-Pivot));
     T.SetRotation((Bend*T.GetRotation()).GetNormalized());
    }
   AppliedSpinePitch=SpinePitch;
  }
 }
 Super::FinalizeBoneTransform();
}
void ABattleVATDisplay::Advance(int32 ID,float Dt)
{
 auto& E=Entries.FindChecked(ID);
 if(!E.Dead)FAnimationRuntime::AdvanceTime(true,(E.Rate*Data->Clips[E.Pose].Clip->RateScale)*Dt,E.Time,Data->Clips[E.Pose].Length);
}

ABattleVATDisplay::ABattleVATDisplay()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void ABattleVATDisplay::PrepareRendering()
{
 // UE caps independent ISM ray tracing WPO evaluations at 256 per component.
 // Keep every instance independent, including its phase, rather than sharing a pose.
 auto* RayWPO=IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO"));
 auto* RayCount=IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.SimulationCount"));
 if(!GUsingNullRHI){SavedRayWPO=RayWPO->GetInt();SavedRayInstances=RayCount->GetInt();RayWPO->Set(-1,ECVF_SetByCode);RayCount->Set(-1,ECVF_SetByCode);RaySettingsChanged=true;}
 if(!GUsingNullRHI)
 {
  IConsoleManager::Get().CallAllConsoleVariableSinks();
  // The ISM WPO sink queues FGlobalComponentReregisterContext on the GT.
  // Drain it during initialization, before skeletal tick workers can start.
  FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
 }
}
bool ABattleVATDisplay::Initialize(UBattleLeaderRuntime* InRuntime)
{
 Runtime=InRuntime;
 Audit=FParse::Param(FCommandLine::Get(),TEXT("P3VATAudit"));
 Data=LoadObject<UBattleVATData>(nullptr,TEXT("/Game/Prototype/DisplayLODTrial/DA_BattleVAT"));
 if(!Data || Data->Meshes.Num()!=3 || Data->Clips.Num()!=3)return false;
#if WITH_EDITOR
 // Complete startup asset rebuilds before any parallel pose task can reference them.
 FAssetCompilingManager::Get().FinishAllCompilation();
#endif
 int32 Followers=0;for(const auto& B:Runtime->GetBodies())Followers+=!B.Value.Actor.IsValid();
 for(int32 Chunk=0;Chunk<FMath::DivideAndRoundUp(Followers,250);++Chunk)for(const auto& V:Data->Meshes)
 {
  auto* G=NewObject<UInstancedStaticMeshComponent>(this);AddInstanceComponent(G);G->SetupAttachment(RootComponent);
  G->SetStaticMesh(V.Mesh);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCanEverAffectNavigation(false);
  G->SetGenerateOverlapEvents(false);
  G->SetMobility(EComponentMobility::Movable);G->SetEvaluateWorldPositionOffsetInRayTracing(true);G->NumCustomDataFloats=6;G->RegisterComponent();Groups.Add(G);
 }
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 TArray<int32> IDs;Runtime->GetBodies().GetKeys(IDs);IDs.Sort();
 for(int32 ID:IDs)
 {
  const auto& B=Runtime->GetBodies().FindChecked(ID);if(B.Actor.IsValid())continue;
  auto* Trial=CastChecked<UBattleVATSkeletalMeshComponent>(B.Mesh.Get());Trial->Presenter=this;Trial->UnitID=ID;
  FBattleVATEntry E;E.ID=ID;E.Body=B.Mesh;E.Weapon=B.Weapon;E.BodyRelative=B.Mesh->GetRelativeTransform();E.WeaponRelative=B.Weapon->GetRelativeTransform();
  E.BodyGroup=(Entries.Num()/250)*3;E.WeaponGroup=E.BodyGroup+(Store->Find(ID)->Weapon==EBattleWeapon::SupportGun?2:1);
  const FTransform Hidden(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector);
  E.Slot=Groups[E.BodyGroup]->AddInstance(Hidden);E.WeaponSlot=Groups[E.WeaponGroup]->AddInstance(Hidden);Entries.Add(ID,E);
  AddTickPrerequisiteComponent(B.Mesh.Get());
  if(Audit)
  {
   auto* Ref=NewObject<USkeletalMeshComponent>(this);AddInstanceComponent(Ref);Ref->SetSkeletalMesh(B.Mesh->GetSkeletalMeshAsset());Ref->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Ref->SetupAttachment(B.Capsule.Get());Ref->SetRelativeTransform(E.BodyRelative);Ref->RegisterComponent();Ref->PlayAnimation(Data->Clips[0].Clip,true);
   Ref->SetVisibility(false);Ref->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   auto* W=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(W);W->SetStaticMesh(B.Weapon->GetStaticMesh());W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   W->SetupAttachment(Ref,TEXT("hand_r"));W->SetRelativeTransform(E.WeaponRelative);W->RegisterComponent();W->SetVisibility(false);
   AuditIndices.Add(ID,AuditBodies.Num());AuditBodies.Add(Ref);AuditWeapons.Add(W);AddTickPrerequisiteComponent(Ref);
  }
 }
#if WITH_EDITOR
 FAssetCompilingManager::Get().FinishAllCompilation();
 if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
#endif
 UE_LOG(LogTemp,Display,TEXT("VAT_DISPLAY enabled=1 followers=%d scope=mass_followers groups=%d max_instances=250 far_light=%d ray_wpo=%d ray_simulation_count=%d"),Entries.Num(),Groups.Num(),int32(FarLight()),IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO"))->GetInt(),IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.SimulationCount"))->GetInt());return true;
}
void ABattleVATDisplay::ResetBodies()
{
 // Loading can occur inside a GameMode tick task. Keep the existing owner of
 // render CVars; restarting PrepareRendering would recursively drain that queue.
 for(const auto& Pair:Entries)if(Pair.Value.Body.IsValid())RemoveTickPrerequisiteComponent(Pair.Value.Body.Get());
 Entries.Reset();
 for(auto& G:Groups)if(G)G->DestroyComponent();Groups.Reset();
}
void ABattleVATDisplay::RemoveUnit(int32 ID)
{
 if(auto* E=Entries.Find(ID))
 {
  if(E->Body.IsValid())RemoveTickPrerequisiteComponent(E->Body.Get());
  const FTransform Hidden(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector);
  Groups[E->BodyGroup]->UpdateInstanceTransform(E->Slot,Hidden,true,true,true);
  Groups[E->WeaponGroup]->UpdateInstanceTransform(E->WeaponSlot,Hidden,true,true,true);
  Entries.Remove(ID);
 }
}
bool ABattleVATDisplay::IsFar(int32 ID)const{const auto* E=Entries.Find(ID);return E && E->Far;}
void ABattleVATDisplay::SetVisible(int32 ID,bool Visible){if(auto* E=Entries.Find(ID))E->Visible=Visible;}
void ABattleVATDisplay::ChangePose(int32 ID,int32 Pose)
{
 if(auto* E=Entries.Find(ID)){if(!E->Far){E->Body->HandleExistingParallelEvaluationTask(true,true);CastChecked<UBattleVATSkeletalMeshComponent>(E->Body.Get())->PendingDt=0;}E->Pose=Pose;if(E->Far){E->Time=0;E->Rate=1;}}
 if(const auto* I=AuditIndices.Find(ID)){AuditBodies[*I]->HandleExistingParallelEvaluationTask(true,true);AuditBodies[*I]->PlayAnimation(Data->Clips[Pose].Clip,true);}
}
void ABattleVATDisplay::ShowNear(int32 ID){if(auto* E=Entries.Find(ID);E && E->Far && !Audit)Leave(*E);}
void ABattleVATDisplay::Freeze(int32 ID)
{
 if(auto* E=Entries.Find(ID))
 {
  E->Dead=true;
  if(E->Far){E->NearRotation=E->BodyRelative.GetRotation();E->BodyRelative.SetRotation(FRotator(0,-90,85).Quaternion());}
  if(const auto* I=AuditIndices.Find(ID)){auto* M=AuditBodies[*I].Get();M->HandleExistingParallelEvaluationTask(true,true);M->SetComponentTickEnabled(false);M->SetRelativeRotation(FRotator(0,-90,85));}
 }
}
void ABattleVATDisplay::Enter(FBattleVATEntry& E)
{
 auto* M=E.Body.Get();auto* W=E.Weapon.Get();
 if(!CastChecked<UBattleVATSkeletalMeshComponent>(M)->HasNativeTick)return;
 if(!AssetsReady && AssetCheckFrame!=GFrameCounter)
 {
  AssetCheckFrame=GFrameCounter;AssetsReady=true;
  for(const auto& V:Data->Meshes)
  {
#if WITH_EDITOR
   AssetsReady &= !V.Mesh->IsCompiling() && !V.Position->IsCompiling() && !V.Normal->IsCompiling() && !V.Tangent->IsCompiling();
   for(const auto& Slot:V.Mesh->GetStaticMaterials())AssetsReady &= Slot.MaterialInterface && !Slot.MaterialInterface->IsCompiling();
   if(Audit && AuditFrame==120)UE_LOG(LogTemp,Display,TEXT("VAT_ASSET_READY mesh=%s mesh_compiling=%d texture_compiling=%d/%d/%d material_complete=%d"),*V.Mesh->GetName(),V.Mesh->IsCompiling(),V.Position->IsCompiling(),V.Normal->IsCompiling(),V.Tangent->IsCompiling(),V.Mesh->GetMaterial(0)->IsComplete());
#endif
  }
 }
 if(!AssetsReady)return;
 M->HandleExistingParallelEvaluationTask(true,true);
 auto* Node=M->GetSingleNodeInstance();if(!Node || !BattleFollowerBones::Eligible(M))return;
 E.Time=Node->GetCurrentTime();E.Rate=Node->GetPlayRate()*M->GlobalAnimRateScale;
 // Frame skipping (trial): the clip time is behind by the frames not yet evaluated. Catch up, then let the far display
 // advance it every frame again.
 if(auto* Skip=CastChecked<UBattleVATSkeletalMeshComponent>(M);Skip->IsUsingExternalTickRateControl())
 {
  if(Skip->PendingDt>0 && M->IsComponentTickEnabled())FAnimationRuntime::AdvanceTime(true,(E.Rate*Data->Clips[E.Pose].Clip->RateScale)*Skip->PendingDt,E.Time,Data->Clips[E.Pose].Length);
  Skip->EndAnimSkip();
 }
 if(!Audit)Runtime->ClearFollowerRifleTurn(E.ID); // a near follower's rifle turn and tilt are not carried into the far body
 E.BodyRelative=M->GetRelativeTransform();E.WeaponRelative=W->GetRelativeTransform();E.NearRotation=E.BodyRelative.GetRotation();
 // A body still playing its death clip ticks: death comes from Freeze, not from the tick. The far display has no death
 // clips, so a corpse lies tipped over there (as one that died far away).
 E.Dead=E.Dead || !M->IsComponentTickEnabled();
 if(E.Dead && !Audit){E.BodyRelative.SetRotation(FRotator(0,-90,85).Quaternion());++StatCorpseEnters;}
 W->UnregisterComponent();W->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
 auto* Trial=CastChecked<UBattleVATSkeletalMeshComponent>(M);Trial->VATFar=true;M->SetVisibility(false);M->MarkRenderStateDirty();
 if(FarLight())
 {
  // The body's own tick already advanced the clip this frame: the presenter must not advance it again.
  if(Trial->NativeTickFrame==GFrameCounter)E.AdvancedFrame=GFrameCounter;
  M->SetComponentTickEnabled(false);M->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
 }
 E.Far=true;E.PreviousFrames=Frames(E);
 if(Audit)++AuditEnters;
}
void ABattleVATDisplay::Leave(FBattleVATEntry& E)
{
 auto* M=E.Body.Get();auto* W=E.Weapon.Get();const auto& B=Runtime->GetBodies().FindChecked(E.ID);
 M->HandleExistingParallelEvaluationTask(true,true);
 // The body does not tick again this frame: take this frame's advance before handing the time back to it.
 if(FarLight())AdvanceFar(E,GetWorld()->GetDeltaSeconds());
 if(M->GetAttachParent()!=B.Capsule.Get())M->AttachToComponent(B.Capsule.Get(),FAttachmentTransformRules::KeepRelativeTransform);
 M->SetRelativeTransform(E.BodyRelative);CastChecked<UBattleVATSkeletalMeshComponent>(M)->VATFar=false;
 M->SetVisibility(false);
 // A corpse returns lying on the last frame of its death clip, not on the clip it moved with (which it went on playing).
 UAnimSequence* Death=E.Dead && !Audit?Runtime->DeathClip(E.ID):nullptr;
 if(Death)
 {
  M->SetRelativeRotation(E.NearRotation);M->SetRelativeScale3D(FVector(1.));
  M->PlayAnimation(Death,false);M->SetPlayRate(1.f);M->SetPosition(Death->GetPlayLength(),false);++StatCorpseReturns;
 }
 else {M->PlayAnimation(BattleStance::AnimEnabled()?Runtime->FollowerClip(E.ID,E.Pose):Data->Clips[E.Pose].Clip.Get(),true);M->SetPlayRate(E.Rate/M->GlobalAnimRateScale);M->SetPosition(E.Time,false);}
 M->TickAnimation(0,false);M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 M->RefreshBoneTransforms(nullptr);M->SetComponentTickEnabled(!E.Dead);
 W->AttachToComponent(M,FAttachmentTransformRules::KeepRelativeTransform,TEXT("hand_r"));W->SetRelativeTransform(E.WeaponRelative);W->RegisterComponent();W->UpdateComponentToWorld();
 BattleFollowerBones::SetVisible(M,E.Visible,true);W->SetVisibility(E.Visible && !E.Dead);E.Far=false;
 M->MarkRenderStateDirty();
 if(Audit)++AuditReturns;
}
void ABattleVATDisplay::Select(int32 ID,int32 LOD,double Distance,double Scale,const FString& View)
{
 auto* E=Entries.Find(ID);if(!E)return;
 if(Audit)
 {
  // Trial fixture only: exercise all followers across a display boundary repeatedly.
  LOD=2;Distance=(AuditFrame/60)%2==0?100000.:0.;E->Body->SetForcedLOD(3);
  AuditBodies[AuditIndices.FindChecked(ID)]->SetForcedLOD(3);
 }
 const double Threshold=Scale/(View==TEXT("RTS")?48.:View==TEXT("TPS")?32.:24.);
 const bool Far=LOD==2 && (E->Initialized?(E->Far?Distance>=Threshold*.8:Distance>=Threshold*1.2):Distance>=Threshold);
 E->Initialized=true;
 // Corpses stay near (their death clip on its last frame, the body's tick off) until they are hidden.
 if(Far && !E->Far && !(E->Dead && !Audit))Enter(*E);else if(!Far && E->Far)Leave(*E);
}
FVector3f ABattleVATDisplay::Frames(const FBattleVATEntry& E)const
{
 const auto& C=Data->Clips[E.Pose];const float F=FMath::Clamp(E.Time*Data->SampleRate,0.f,float(C.Samples-1));
 const int32 A=FMath::FloorToInt(F),B=FMath::Min(A+1,C.Samples-1);
 // Last interval can be shorter than 1/60 s.
 const float TA=FMath::Min(A/Data->SampleRate,C.Length),TB=FMath::Min(B/Data->SampleRate,C.Length);
 return FVector3f(C.Start+A,C.Start+B,TB>TA?FMath::Clamp((E.Time-TA)/(TB-TA),0.f,1.f):0.f);
}
void ABattleVATDisplay::Tick(float Dt)
{
 Super::Tick(Dt);if(!Data)return;
 const double Start=FPlatformTime::Seconds();int32 FarCount=0,ShownCount=0;
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleVATDisplay);
 const auto* Phase=GetWorld()->GetSubsystem<UBattlePhase3>();
 // Far bodies no longer tick: advance their clip time here, under the same conditions as the component tick did.
 if(FarLight())for(auto& Pair:Entries)if(Pair.Value.Far)AdvanceFar(Pair.Value,Dt);
 if(!Phase->bEnabled)
 {
  // Ordinary leader play retains native skeletal LOD selection. Only replace
  // an already-coarse LOD; restore immediately when the distance gate closes.
  auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
  if(PC && PC->PlayerCameraManager)
  {
   FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
   int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
   const double Scale=180.*FMath::Max(Width,1)*.5/FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5));
   for(auto& Pair:Entries)
   {
    const auto& E=Pair.Value;
    const int32 LOD=E.Far?2:E.Body->GetPredictedLODLevel();
    Select(E.ID,LOD>=2?2:LOD,FVector::Distance(Eye,Runtime->Position(E.ID)),Scale,PC->IsTactical()?TEXT("RTS"):TEXT("FPS"));
   }
  }
 }
 if(Transforms.Num()!=Groups.Num())Transforms.SetNum(Groups.Num());
 for(int32 I=0;I<Groups.Num();++I)
 {
  Transforms[I].SetNum(Groups[I]->GetInstanceCount(),EAllowShrinking::No);
  for(FTransform& InstanceTransform:Transforms[I])InstanceTransform=FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector);
 }
 for(auto& Pair:Entries)
 {
  auto& E=Pair.Value;if(!E.Far)continue;
  ++FarCount;ShownCount+=E.Visible;
  const FVector3f F=Frames(E);
  FTransform CapsuleTransform;
  bool bHaveCapsuleTransform=false;
  for(int32 G:{E.BodyGroup,E.WeaponGroup})
  {
   const int32 Slot=G==E.BodyGroup?E.Slot:E.WeaponSlot;const float Values[]={F.X,F.Y,F.Z,E.PreviousFrames.X,E.PreviousFrames.Y,E.PreviousFrames.Z};
   Groups[G]->SetCustomData(Slot,MakeArrayView(Values),false);
   if(E.Visible && (G==E.BodyGroup || !E.Dead))
   {
    if(!bHaveCapsuleTransform){CapsuleTransform=Runtime->GetBodies().FindChecked(E.ID).Capsule->GetComponentTransform();bHaveCapsuleTransform=true;}
    Transforms[G][Slot]=E.BodyRelative*CapsuleTransform;
   }
  }
  E.PreviousFrames=F;
 }
 for(int32 I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,Transforms[I],true,true,false);
 const double Elapsed=GetWorld()->GetTimeSeconds()-Phase->Started;
 if(!Phase->bEnabled || (Elapsed>=Phase->Warmup && Elapsed<Phase->Warmup+Phase->Duration)){++StatFrames;StatFar+=FarCount;StatShown+=ShownCount;StatMS+=(FPlatformTime::Seconds()-Start)*1000;}
 if(Audit)
 {
  ++AuditFrame;
  if(AuditFrame==70)
  {
   const FVector Center=Runtime->Position(2)+FVector(0,150,30),Eye=Center+FVector(-600,-900,450);
   auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Center-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(50);
   GetWorld()->GetFirstPlayerController()->SetViewTarget(Camera);
#if WITH_EDITOR
   if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
#endif
   UE_LOG(LogTemp,Display,TEXT("VAT_QA_CAMERA close_fixture view_verification_already_completed=1"));
  }
  for(const auto& Pair:Entries)
  {
   const auto& E=Pair.Value;const int32 I=AuditIndices.FindChecked(E.ID);auto* Ref=AuditBodies[I].Get();Ref->HandleExistingParallelEvaluationTask(true,true);
   auto* M=E.Body.Get();if(!E.Far)M->HandleExistingParallelEvaluationTask(true,true);
   const float T=E.Far?E.Time:M->GetSingleNodeInstance()->GetCurrentTime(),RT=Ref->GetSingleNodeInstance()->GetCurrentTime();
   bool Bones=true,Weapon=true;
   if(!E.Far && (E.Visible || E.Dead))
   {
    const auto& X=M->GetComponentSpaceTransforms();const auto& Y=Ref->GetComponentSpaceTransforms();Bones=X.Num()==Y.Num();
    for(int32 J=0;J<FMath::Min(X.Num(),Y.Num());++J)Bones &= X[J].Equals(Y[J],1.e-6);
    Weapon=E.Weapon->GetComponentTransform().Equals(AuditWeapons[I]->GetComponentTransform(),1.e-6);
   }
   ++AuditChecks;
   if(T!=RT || !Bones || !Weapon){if(AuditErrors<8)UE_LOG(LogTemp,Error,TEXT("VAT_AUDIT_MISMATCH frame=%d id=%d far=%d time=%.9f/%.9f bones=%d weapon=%d"),AuditFrame,E.ID,E.Far,T,RT,Bones,Weapon);++AuditErrors;}
   AuditCSV+=FString::Printf(TEXT("%d,%d,%d,%d,%d,%.9f,%.9f,%d,%d,%d\n"),AuditFrame,E.ID,E.Far,E.Dead,E.Pose,T,RT,T==RT,Bones,Weapon);
  }
  if(AuditFrame==90)
  {
   // Display-only fixture: stop game changes, then exercise all clips independently.
   GetWorld()->GetAuthGameMode<ABattleGameMode>()->bCombatPaused=true;
   for(auto& Pair:Entries){auto& E=Pair.Value;const int32 Pose=E.ID%3;ChangePose(E.ID,Pose);if(!E.Far)E.Body->PlayAnimation(Data->Clips[Pose].Clip,true);}
  }
  if(AuditFrame==150 || AuditFrame==240)
  {
   for(auto& Pair:Entries)
   {
    auto& E=Pair.Value;if((E.ID%2==1)!=(AuditFrame==150))continue;
    Freeze(E.ID);
    if(!E.Far){BattleFollowerBones::PrepareFreeze(E.Body.Get(),true);E.Body->SetComponentTickEnabled(false);E.Body->SetRelativeRotation(FRotator(0,-90,85));E.Weapon->SetVisibility(false);}
   }
  }
  if(AuditFrame==119 || AuditFrame==121 || AuditFrame==149 || AuditFrame==181 || AuditFrame==299 || AuditFrame==301)
  {FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattlePhase3="),Prefix);FScreenshotRequest::RequestScreenshot(Prefix+FString::Printf(TEXT("-vat-%03d.png"),AuditFrame),true,false);}
  if(AuditFrame==360)
  {
   FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattlePhase3="),Prefix);FFileHelper::SaveStringToFile(AuditCSV,*(Prefix+TEXT("-vat-audit.csv")));
   if(AuditErrors || !AuditEnters || !AuditReturns)++GetWorld()->GetSubsystem<UBattlePhase3>()->Failures;
   UE_LOG(LogTemp,Display,TEXT("P3_VAT_AUDIT checks=%d errors=%d enters=%d returns=%d"),AuditChecks,AuditErrors,AuditEnters,AuditReturns);
  }
 }
}
void ABattleVATDisplay::EndPlay(const EEndPlayReason::Type Reason)
{
 UE_LOG(LogTemp,Display,TEXT("VAT_DISPLAY_STATS frames=%lld far_mean=%.6f visible_far_mean=%.6f presenter_ms=%.6f anim_skip=%d anim_ticks=%lld anim_evals=%lld anim_budget=%d anim_stretch_mean=%.3f corpse_enters=%d corpse_returns=%d"),StatFrames,StatFrames?double(StatFar)/StatFrames:0.,StatFrames?double(StatShown)/StatFrames:0.,StatFrames?StatMS/StatFrames:0.,int32(AnimSkip()),StatAnimTicks,StatAnimEvals,AnimBudget(),StatAnimStretchFrames?double(StatAnimStretch)/StatAnimStretchFrames:1.,StatCorpseEnters,StatCorpseReturns);
 Super::EndPlay(Reason);
 if(RaySettingsChanged)
 {
  IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO"))->Set(SavedRayWPO,ECVF_SetByCode);
  IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.InstancedStaticMeshes.SimulationCount"))->Set(SavedRayInstances,ECVF_SetByCode);
 }
}
