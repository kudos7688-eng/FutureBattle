#include "BattleLeaderRuntime.h"
#include "BattleMuzzleFlash.h"
#include "BattleFollowerBones.h"
#include "BattleVATDisplay.h"
#include "BattlePhase3.h"
#include "BattleGameMode.h"
#include "BattleFieldLayout.h"
#include "BattleMatch.h"
#include "BattleSquadTactics.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattleSuppression.h"
#include "BattleStance.h"
#include "BattleBlindFire.h"
#include "BattleBodyAnim.h"
#include "EngineUtils.h"
#include "BattleMetrics.h"
#include "MassEntitySubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "BattleCover.h"
#include "BattleSectors.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "FutureBattle.h"

FVector UBattleLeaderRuntime::LimitedSteering(FVector Desired,FVector Separation,float MaxSide)
{
 const FVector Forward=Desired.GetSafeNormal2D();
 if(Forward.IsNearlyZero()) return FVector::ZeroVector;
 const FVector Side(-Forward.Y,Forward.X,0);
 // Preserve the commanded forward direction; crowd size cannot amplify the sideways correction.
 const float Offset=FMath::Clamp(float(FVector::DotProduct(Separation,Side)*.65),-FMath::Clamp(MaxSide,0.f,.5f),FMath::Clamp(MaxSide,0.f,.5f));
 return (Forward+Side*Offset).GetSafeNormal2D();
}

FBattleMinimapLayout FBattleMinimapLayout::Make(FVector2D View,const UWorld* World)
{
 // Bottom left, above the order bar. Only the wide maps have one; too small a window gets none.
 FBattleMinimapLayout M; if(!BattleFieldLayout::IsSightField(World)) return M;
 M.HalfExtent=BattleFieldLayout::FieldHalfExtent(World);
 const float MapBottom=FBattleCommandBarLayout::Make(View,World).PanelTop-22.f;
 const float Height=FMath::Min(300.f,MapBottom-290.f); if(Height<140.f) return M;
 M.Size=FVector2D(Height*M.HalfExtent.Y/M.HalfExtent.X,Height); M.Origin=FVector2D(20,MapBottom-Height); return M;
}
FBattleSquadListLayout FBattleSquadListLayout::Make(FVector2D View,const UWorld* World)
{
 FBattleSquadListLayout L; L.Width=FMath::Min(360.f,float(View.X)-32.f);
 L.Origin=FVector2D(View.X-L.Width-16,104); L.Height=FMath::Max(0.f,FMath::Min(720.f,FBattleCommandBarLayout::Make(View,World).PanelTop-110.f));
 L.VisibleRows=FMath::Max(0,FMath::FloorToInt((L.Height-52)/L.RowHeight)); return L;
}
bool FBattleSquadListLayout::Contains(FVector2D P) const { return Height>0 && P.X>=Origin.X && P.X<Origin.X+Width && P.Y>=Origin.Y && P.Y<Origin.Y+Height; }
int32 FBattleSquadListLayout::RowAt(FVector2D P) const
{
 if(!Contains(P) || P.Y<Origin.Y+Header) return INDEX_NONE;
 const int32 R=FMath::FloorToInt((P.Y-Origin.Y-Header)/RowHeight); return R<VisibleRows ? R:INDEX_NONE;
}

bool UBattleLeaderRuntime::Start(ABattleGameMode& InMode)
{
 if(bEnabled) return false;
 // Explicit opt-in isolated runtime. Legacy trials, saves and network modes are not silently mixed.
 if(InMode.GetClass()!=ABattleGameMode::StaticClass() || GetWorld()->GetNetMode()!=NM_Standalone || FParse::Param(FCommandLine::Get(),TEXT("BattleBenchmark")) || FParse::Param(FCommandLine::Get(),TEXT("BattleFastMatch")) ||
   FCString::Stristr(FCommandLine::Get(),TEXT("BattleSquadTrial="))) return false;
 bWideField=FParse::Param(FCommandLine::Get(),TEXT("BattleWideMixed")) || BattleFieldLayout::IsSightField(GetWorld());
 bMixedFastMatch=bWideField && FParse::Param(FCommandLine::Get(),TEXT("BattleMixedFastMatch"));
 bArea=BattleFieldLayout::IsSightField(GetWorld())&&FParse::Param(FCommandLine::Get(),TEXT("BattleArea"));
 if(bArea&&bMixedFastMatch)return false;
 SightExperiment.Enabled=FParse::Value(FCommandLine::Get(),TEXT("BattleSightExperiment="),SightExperiment.Output);
 if(SightExperiment.Enabled && !BattleFieldLayout::IsSightField(GetWorld())) return false;
 const bool Swap=BattleFieldLayout::IsSightField(GetWorld()) && FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"));
 int32 RangeAware=1;FParse::Value(FCommandLine::Get(),TEXT("BattleRangeAware="),RangeAware);bRangeAwareCombat=RangeAware==1;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_RANGE_AWARE enabled=%d"),bRangeAwareCombat);
 int32 Stable=1; FParse::Value(FCommandLine::Get(),TEXT("BattleStableMovement="),Stable); bStableMovement=Stable!=0;
 int32 Contact=1;FParse::Value(FCommandLine::Get(),TEXT("BattleContactAvoidance="),Contact);bContactAvoidance=Contact!=0;
 bContactTrial=FParse::Param(FCommandLine::Get(),TEXT("BattleContactTrial"));
 bMovementTrial=bContactTrial || FParse::Param(FCommandLine::Get(),TEXT("BattleMovementTrial"));
 FParse::Value(FCommandLine::Get(),TEXT("BattleMovementScene="),MoveTrialOnly);
 if(MoveTrialOnly>=0 && MoveTrialOnly<4) MoveTrialScene=MoveTrialOnly;
 if(bWideField && !BattleFieldLayout::IsLarge(GetWorld())) return false;
 int32 Count=bMixedFastMatch?20:bWideField?500:40; FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderUnits="),Count);
 if(Count<20 || Count>1000 || Count%20) return false;
 if(bWideField && Count!=(bMixedFastMatch?20:500)) return false;
 Mode=&InMode; bEnabled=true;
 int32 AnimTasks=0; FParse::Value(FCommandLine::Get(),TEXT("BattleReduceAnimTasks="),AnimTasks);
 if(AnimTasks!=0 && AnimTasks!=1) return false;
 bReduceAnimTasks=AnimTasks==1;
 UE_LOG(LogFutureBattle,Display,TEXT("ANIM_TASKS enabled=%d scope=hidden_update_only"),AnimTasks);
 int32 HiddenBones=GetDefault<UBattleTuning>()->bReduceHiddenBones?1:0; FParse::Value(FCommandLine::Get(),TEXT("BattleReduceHiddenBones="),HiddenBones);
 if(HiddenBones!=0 && HiddenBones!=1) return false;
 bReduceHiddenBones=HiddenBones==1;
 UE_LOG(LogFutureBattle,Display,TEXT("HIDDEN_BONES enabled=%d scope=fog_hidden_followers"),HiddenBones);
 int32 ReuseSight=GetDefault<UBattleTuning>()->bReuseSightContext?1:0; FParse::Value(FCommandLine::Get(),TEXT("BattleReuseSightContext="),ReuseSight);
 if(ReuseSight!=0 && ReuseSight!=1) return false;
 GetWorld()->GetSubsystem<UBattlePerception>()->bReuseSightContext=ReuseSight==1;
 int32 Grid=GetDefault<UBattleTuning>()->bPerceptionGrid?1:0; FParse::Value(FCommandLine::Get(),TEXT("BattlePerceptionGrid="),Grid);
 if(Grid!=0 && Grid!=1) return false;
 GetWorld()->GetSubsystem<UBattlePerception>()->bUseSpatialGrid=Grid==1;
 // Trial -BattleSightSlowFar=1 (2026-11-17, default OFF): unknown enemies beyond 12 m are considered on every other 0.2 s scan
 // pulse only (the 2026-11-04 "Far" sense mode, tested then only on the dense 500 map). Known contacts and started detections never wait.
 int32 SlowFar=0; FParse::Value(FCommandLine::Get(),TEXT("BattleSightSlowFar="),SlowFar);
 GetWorld()->GetSubsystem<UBattlePerception>()->bSlowUnknownFar=SlowFar==1;
 // Scan interval of observers that saw no enemy (user decision 2026-11-17: 0.4 s; -BattleSightScanInterval=0.2 = old uniform 0.2 s).
 {double Interval=.4,Near=1.e7;FParse::Value(FCommandLine::Get(),TEXT("BattleSightScanInterval="),Interval);FParse::Value(FCommandLine::Get(),TEXT("BattleSightScanNear="),Near);
  auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();Sense->UnseenScanInterval=FMath::Clamp(Interval,.2,2.);Sense->ScanNearCm=FMath::Max(0.,Near);
  UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_SCAN_INTERVAL unseen=%.2f near_cm=%.0f"),Sense->UnseenScanInterval,Sense->ScanNearCm);}
 UE_LOG(LogFutureBattle,Display,TEXT("PERCEPTION_GRID enabled=%d far=%d scope=existing_candidate_grid"),Grid,SlowFar==1);
 UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_CONTEXT_REUSE enabled=%d scope=observer_scan"),ReuseSight);
 Phase3=GetWorld()->GetSubsystem<UBattlePhase3>();
 if(!Phase3->Configure(*this)) return false;
 if(bWideField && Phase3->bEnabled) return false;
 int32 DisplayLOD=1; FParse::Value(FCommandLine::Get(),TEXT("BattleDisplayLOD="),DisplayLOD);
 if(DisplayLOD!=0 && DisplayLOD!=1)return false;
 bDisplayLODEnabled=DisplayLOD==1;
 if(!Phase3->bEnabled && FParse::Param(FCommandLine::Get(),TEXT("P3VATAudit")))return false;
 UE_LOG(LogFutureBattle,Display,TEXT("VAT_DISPLAY requested=%d default=1 scope=mass_followers"),DisplayLOD);
 if(DisplayLOD==1){VAT=GetWorld()->SpawnActor<ABattleVATDisplay>();VAT->PrepareRendering();}
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 DeathHandle=Store->OnUnitDied.AddUObject(this,&UBattleLeaderRuntime::OnDeath);
 Manager=GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager().AsShared();
 const auto Archetype=Manager->CreateArchetype({FBattleFollowerFragment::StaticStruct()});
 DisplayHost=GetWorld()->SpawnActor<AActor>();
 auto* Root=NewObject<USceneComponent>(DisplayHost); DisplayHost->SetRootComponent(Root); DisplayHost->AddInstanceComponent(Root); Root->RegisterComponent();
 auto* MeshAsset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
 auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"));
 IdlePose=Idle;
 WalkPose=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"));
 ReloadPose=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"));
 int32 NoControl=INDEX_NONE; FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderNoControl="),NoControl);
 for(int32 Team=0;Team<2;++Team) for(int32 Group=0;Group<Count/20;++Group)
 {
  TArray<int32> IDs;
  const int32 Side=Swap?1-Team:Team;
  // Symmetric start trial: the east side's rows are the west side's rotated 180 degrees (north-south reversed), on the area maps.
  const FVector RowSign(1.,bArea&&Side&&BattleTactics::SymmetricStart(GetWorld())?-1.:1.,1.);
  const FVector Center=RowSign*(bMixedFastMatch?FVector((Team?1:-1)*(BattleFieldLayout::HomeDistance(GetWorld())-(BattleFieldLayout::IsSightField(GetWorld())?1000:500)),0,100):BattleFieldLayout::IsSightField(GetWorld())?BattleFieldLayout::WideSpawnFor(GetWorld(),Side,Group):bWideField?FVector((Team?1:-1)*(9000+(Group%5)*400),(Group/5-2)*2800,100):Phase3->bEnabled?Phase3->SpawnCenter(Team,Group):FVector(Team ? 1800:-1800, (Group-(Count/20-1)*.5)*800, 95));
  ABattleUnit* Leader=nullptr;
  for(int32 Slot=0;Slot<BattleSquadType::RifleMembers();++Slot)
  {
   const int32 ID=Store->RegisterUnit(Team,Phase3->bStable?10000.f:0.f,WeaponForSlot(Slot)); IDs.Add(ID);
   const FVector P=Center+FVector((Slot/5)*-130*(Side?-1:1),(Slot%5-2)*110*(BattleFieldLayout::IsSightField(GetWorld()) && Side?-1:1),0);
   FBattleLeaderBody B;
   if(Slot==0)
   {
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Leader=GetWorld()->SpawnActor<ABattleUnit>(P,FRotator(0,Side?180:0,0),Params);
    if(!Leader) return false;
    Leader->InitializeUnit(ID); Mode->Units.Add(Leader); B.Actor=Leader;
    Leader->GetCharacterMovement()->bOrientRotationToMovement=false;
    // The shared runtime produces input in GameMode::Tick. Explicitly order its
    // consumer afterwards; a throttled controller prerequisite can otherwise
    // reorder movement ticks and leave every third frame without input.
    Leader->GetCharacterMovement()->AddTickPrerequisiteActor(Mode);
    // The mixed runtime already steers both representations. Do not apply a second RVO correction.
    if(bWideField) Leader->GetCharacterMovement()->SetRVOAvoidanceWeight(1.f);
    HitIDs.Add(Leader->GetCapsuleComponent(),ID);
   }
   else
   {
    B.Entity=Manager->CreateEntity(Archetype); auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);
    F.UnitID=ID; F.Position=P; F.Forward=FVector(Side?-1:1,0,0);
    auto* Capsule=NewObject<UCapsuleComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Capsule);
    Capsule->InitCapsuleSize(36,90); Capsule->SetCollisionObjectType(ECC_Pawn);
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Capsule->SetCollisionResponseToAllChannels(ECR_Block);
    Capsule->SetCanEverAffectNavigation(false); Capsule->SetupAttachment(Root);
    if(FollowerOverlapsOff())Capsule->SetGenerateOverlapEvents(false);
    Capsule->RegisterComponent(); Capsule->SetWorldLocation(P);
    B.Capsule=Capsule; HitIDs.Add(Capsule,ID);
    auto* Mesh=DisplayLOD==1?NewObject<USkeletalMeshComponent>(DisplayHost,UBattleVATSkeletalMeshComponent::StaticClass()):NewObject<USkeletalMeshComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Mesh);
    Mesh->SetSkeletalMesh(MeshAsset); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetupAttachment(Capsule);
    Mesh->SetRelativeLocation(FVector(0,0,-90)); Mesh->SetRelativeRotation(FRotator(0,-90,0));
    if(FollowerOverlapsOff())Mesh->SetGenerateOverlapEvents(false);
    Mesh->RegisterComponent();
    Mesh->PlayAnimation(Idle,true); B.Mesh=Mesh;
    auto* Weapon=NewObject<UStaticMeshComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Weapon);
    Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision); if(FollowerOverlapsOff())Weapon->SetGenerateOverlapEvents(false); Weapon->RegisterComponent(); Leader->MountWeapon(Weapon,Mesh,Store->Find(ID)->Weapon,false); B.Weapon=Weapon;
   }
   Bodies.Add(ID,B);
  }
  const int32 SID=Store->CreateLeaderSquad(Team,IDs,IDs[0],Team==0 && Group!=NoControl);
  if(SID==INDEX_NONE) return false;
  FBattleLeaderPlan Plan; Plan.Center=Center; Plan.NextThink=GetWorld()->GetTimeSeconds()+.05*(SID%10); Plans.Add(SID,Plan);
  IssueOrder(SID,bWideField?EBattleOrder::Hold:Phase3->bEnabled?EBattleOrder::AttackMove:Team?EBattleOrder::AttackMove:EBattleOrder::Hold,bWideField?Center:Phase3->bEnabled?Phase3->Goal(Team,Group):FVector(Team?600:Center.X,Center.Y,0));
 }
 if(DisplayLOD==1 && !VAT->Initialize(this))return false;
 Mode->CombatStartsAt=GetWorld()->GetTimeSeconds()+3.;
 if(Phase3->bEnabled) Phase3->Begin();
 if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->BindLeaderDeaths();
 if(bWideField)
 {
  auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); Match->InitializeMatch(); Match->Phase=EBattleMatchPhase::Combat; Match->DirectorClock=5;
  Mode->bScaleTest=true; Mode->ScaleUnitCount=Count; Mode->bCombatPaused=true;
  if(bMixedFastMatch){Match->Phase=EBattleMatchPhase::Deployment;Match->Credits[0]=Match->Credits[1]=3000-Match->CurrentSquadPrice(EBattleTraining::Regular);}
  if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->SetSelection(Actor(1));
  // Map E: long paths through the dense city need a larger A* node budget than the engine default, or they come back partial.
  if(BattleFieldLayout::IsUrban(GetWorld()))
   if(auto* NavSystem=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    if(auto* Recast=Cast<ARecastNavMesh>(NavSystem->GetDefaultNavDataInstance()))
    {Recast->DefaultMaxSearchNodes=32768.f;const_cast<FNavigationQueryFilter&>(*Recast->GetDefaultQueryFilter()).SetMaxSearchNodes(32768);}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WIDE_MIXED_READY squads=50 actors=50 mass=%d objectives=%d paused=1"),50*(BattleSquadType::RifleMembers()-1),Match->Objectives.Num());
  if(FParse::Param(FCommandLine::Get(),TEXT("BattleSectorSmoke")))
  {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSectorSmoke,6.f,false);}
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleCoverActionSmoke")))
  {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunCoverActionSmoke,4.f,false);}
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleFlankBayTrial")))
  {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunFlankBayTrial,4.f,false);}
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleMixedFastSmoke")))
  {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMixedFastSmoke,4.f,false);}
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleMixedSaveSmoke")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMixedSaveSmoke,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleWeaponTrial")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWeaponRangeTrial,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleExoShot")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunExoShot,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleDeathShot")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunDeathShot,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleStanceShot")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunStanceShot,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleScopeShot")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunScopeShot,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleSightVisual")))
  { FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSightVisual,4.f,false); }
  else if(FString StatsFile;FParse::Value(FCommandLine::Get(),TEXT("BattleMapStats="),StatsFile))
  { bMovementTrial=true;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMapStats,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleWideMapAudit")))
  { bMovementTrial=true;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWideMapAudit,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleSightTrial")))
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSightTrial,4.f,false); }
  else if(bContactTrial)
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunContactTrial,4.f,false); }
  else if(bMovementTrial)
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMovementTrial,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleFlankSmoke")))
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunFlankSmoke,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleSupportSmoke")))
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSupportSmoke,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleST2Smoke")))
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunST2Smoke,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleWideSmoke")))
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWideSmoke,4.f,false); }
  else if(FParse::Param(FCommandLine::Get(),TEXT("BattleWideSoak")))
  { FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunWideSoak,4.f,false); }
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_READY total=%d actors=%d mass=%d"),Count,Mode->Units.Num(),Count-Mode->Units.Num());
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleLeaderSmoke")))
 { Mode->bCombatPaused=true; FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSmoke,3.f,false); }
 if(FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderQA="),QAPrefix))
 { Mode->bCombatPaused=true; FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunUIQA,5.f,false); }
 return true;
}
void UBattleLeaderRuntime::Deinitialize()
{
 LogSightHitStats(TEXT("deinitialize"));
 if(bEnabled)
 {
  if(auto* Store=GetWorld()->GetSubsystem<UBattleState>()) Store->OnUnitDied.Remove(DeathHandle);
  if(Manager) for(auto& P:Bodies) if(P.Value.Entity.IsSet() && Manager->IsEntityValid(P.Value.Entity)) Manager->DestroyEntity(P.Value.Entity);
  Bodies.Reset(); Manager.Reset();
 }
 Super::Deinitialize();
}
ABattleUnit* UBattleLeaderRuntime::Actor(int32 ID) const { const auto* B=Bodies.Find(ID); return B?B->Actor.Get():nullptr; }
FVector UBattleLeaderRuntime::Position(int32 ID) const
{
 const auto* B=Bodies.Find(ID); if(!B) return FVector::ZeroVector;
 if(B->Actor.IsValid())return B->Actor->GetActorLocation();
 // An Actor body that was destroyed (it fell out of the world) has no Mass entity: its last ground position, not a crash.
 if(!B->Entity.IsSet() || !Manager->IsEntityValid(B->Entity))return LastGround.FindRef(ID);
 return Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B->Entity).Position;
}
FVector UBattleLeaderRuntime::Forward(int32 ID) const
{
 const auto* B=Bodies.Find(ID); if(!B) return FVector::ForwardVector;
 if(B->Actor.IsValid())return B->Actor->CaptureSightContext().Forward;
 if(!B->Entity.IsSet() || !Manager->IsEntityValid(B->Entity))return FVector::ForwardVector;
 return Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B->Entity).Forward;
}
void UBattleLeaderRuntime::GuardFalls(double Now)
{
 if(Now<NextFallCheck)return;
 NextFallCheck=Now+.25;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(auto& Body:Bodies)
 {
  ABattleUnit* A=Body.Value.Actor.Get();const auto* U=Store->Find(Body.Key);
  if(!A || !U || !U->IsAlive())continue;
  const FVector P=A->GetActorLocation();
  if(P.Z>-200.){if(const auto* Move=A->GetCharacterMovement();!Move || Move->IsMovingOnGround())LastGround.Add(Body.Key,P);continue;}
  if(P.Z>-500.)continue;
  if(const FVector* Ground=LastGround.Find(Body.Key))
  {
   A->SetActorLocation(*Ground+FVector(0,0,20),false,nullptr,ETeleportType::TeleportPhysics);
   if(auto* Move=A->GetCharacterMovement())Move->StopMovementImmediately();
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FALL_RECOVER unit=%d squad=%d from=%s to=%s"),Body.Key,U->SquadID,*P.ToCompactString(),*Ground->ToCompactString());
   SightExperiment.Cover(TEXT("fall_recovered"));
  }
  else
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FALL_DEATH unit=%d squad=%d at=%s"),Body.Key,U->SquadID,*P.ToCompactString());
   Store->ApplyDamage(Body.Key,-1,U->HP+U->Shield+1.f);SightExperiment.Cover(TEXT("fall_death"));
  }
 }
}
FBattleSightContext UBattleLeaderRuntime::Sight(int32 ID) const
{
 // Stances lower the eye (60cm above the body centre when standing); a head shown by an over-the-top blind fire looks
 // from its own height (BattleBlindFire::ExposedEyeOffset).
 const float EyeZ=IsHeadExposed(ID,GetWorld()->GetTimeSeconds())?BattleBlindFire::ExposedEyeOffset:BattleStance::EyeOffset(StanceOf(ID));
 // Scout pairs see ScoutSightRange while still (moving: their normal profile).
 const float Still=ScoutStillSight(ID);
 // Trial -BattleSightTeamMask=1: the observer's own side does not block its sight (its soldiers' bodies carry the side's mask bit).
 const FMaskFilter TeamMask=SightTeamMaskFor(ID);
 // Leaning out past an edge for a blind shot, he looks from there.
 FVector Leaning;const bool bLeaning=SideExposure(ID,GetWorld()->GetTimeSeconds(),Leaning);
 if(auto* A=Actor(ID)) {auto Context=A->CaptureSightContext();Context.Eye.Z+=EyeZ-60.f;if(bLeaning)Context.Eye=Leaning;if(Still>0.f)Context.RangeSquared=FMath::Max(Context.RangeSquared,FMath::Square(Still));Context.Query.IgnoreMask=TeamMask;return Context;}
 FBattleSightContext C; const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 C.Position=Position(ID); C.Eye=bLeaning?Leaning:C.Position+FVector(0,0,EyeZ); C.Forward=Forward(ID); C.bAlive=U && U->IsAlive();
 if(U) { C.RangeSquared=FMath::Square(U->Perception.SightRange); C.HalfAngleCos=FMath::Cos(FMath::DegreesToRadians(U->Perception.FieldOfView*.5)); C.bOmnidirectional=U->Perception.FieldOfView>=360; }
 if(Still>0.f) C.RangeSquared=FMath::Max(C.RangeSquared,FMath::Square(Still));
 C.Metrics=GetWorld()->GetSubsystem<UBattleMetrics>(); C.Query=FCollisionQueryParams(SCENE_QUERY_STAT(LeaderSight),false);
 if(const auto* B=Bodies.Find(ID)) C.Query.AddIgnoredComponent(B->Capsule.Get());
 C.Query.IgnoreMask=TeamMask; return C;
}
bool UBattleLeaderRuntime::CanSee(int32 ID,int32 Target) const
{
 const auto C=Sight(ID); FVector End;
 return PrepareSightTrace(C,Target,End) && SightHitVisible(TraceSight(C,End,Target,SightFromCanSee),Target);
}
bool UBattleLeaderRuntime::CanSeeWithContext(const FBattleSightContext& C,int32 Target,int32 Observer) const
{
 // Keep reference arithmetic, target reads, query and hit policy unchanged.
 // Only the observer Context construction is hoisted by the perception caller.
 FVector End; if(!PrepareSightTrace(C,Target,End)) return false;
 // Trial -BattleSightPairReuse=1 (performance, default OFF): two soldiers looking at each other trace nearly the same line. A result
 // traced in the other direction within SightPairSeconds is reused while neither has moved more than 1 m since.
 static const bool bPairReuse=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSightPairReuse="),V);return V!=0;}();
 const bool bPair=bPairReuse&&Observer>=0&&Observer!=Target;
 const FVector P=bPair?Position(Target):FVector::ZeroVector;
 const double Now=bPair?GetWorld()->GetTimeSeconds():0.;
 const uint64 Key=bPair?(uint64(uint32(FMath::Min(Observer,Target)))<<32)|uint32(FMath::Max(Observer,Target)):0;
 const bool bObserverLow=Observer<Target;
 if(bPair)
  if(const auto* E=SightPairs.Find(Key);E&&E->bLowObserver!=bObserverLow&&Now>=E->At&&Now-E->At<.2-1.e-6&&
     FVector::DistSquared(E->Low,bObserverLow?C.Position:P)<=1.e4&&FVector::DistSquared(E->High,bObserverLow?P:C.Position)<=1.e4)
  {++SightPairReused;return E->bVisible;}
 const bool bVisible=SightHitVisible(TraceSight(C,End,Target,SightFromPerception),Target);
 if(bPair)
 {
  ++SightPairTraced;
  if(Now>=SightPairsPruneAt){SightPairsPruneAt=Now+1.;for(auto It=SightPairs.CreateIterator();It;++It)if(Now-It.Value().At>.5)It.RemoveCurrent();}
  SightPairs.Add(Key,{Now,bObserverLow?C.Position:P,bObserverLow?P:C.Position,bObserverLow,bVisible});
 }
 return bVisible;
}
FHitResult UBattleLeaderRuntime::TraceSight(const FBattleSightContext& C,const FVector& End,int32 Target,ESightHitSource Source) const
{
 const bool bStats=SightHitStatsEnabled();const uint64 T0=bStats?FPlatformTime::Cycles64():0;
 FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,C.Eye,End,ECC_Visibility,C.Query);
 if(bStats) RecordSightHit(Hit,Target,FPlatformTime::Cycles64()-T0,Source);
 return Hit;
}
bool UBattleLeaderRuntime::PrepareSightTrace(const FBattleSightContext& C,int32 Target,FVector& End) const
{
 // The one copy of the checks before a unit sight trace (alive, range, field of view); vehicles are seen out to VehicleSpotRange.
 const auto* T=GetWorld()->GetSubsystem<UBattleState>()->Find(Target);
 if(!C.bAlive || !T || !T->IsAlive() || !Bodies.Contains(Target)) return false;
 const FVector P=Position(Target), D=(P-C.Position).GetSafeNormal2D();
 if(FVector::DistSquared(P,C.Position)>(IsVehicle(Target)?FMath::Max(C.RangeSquared,FMath::Square(VehicleSpotRange())):C.RangeSquared) || (!C.bOmnidirectional && !D.IsNearlyZero() && FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)) return false;
 End=P+FVector(0,0,TargetAimOffset(Target));
 // A soldier blind-firing around an edge is seen by the part of him that shows past it.
 FVector Shown;if(SideExposure(Target,GetWorld()->GetTimeSeconds(),Shown))End=Shown;
 return true;
}
bool UBattleLeaderRuntime::SightHitStatsEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSightHitStats="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::SightTeamMaskEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSightTeamMask="),V);return V!=0;}();
 return bOn;
}
FMaskFilter UBattleLeaderRuntime::SightTeamMaskFor(int32 ObserverID) const
{
 if(!SightTeamMaskEnabled()) return 0;
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ObserverID);
 return U&&U->Team>=0&&U->Team<2?FMaskFilter(1<<U->Team):FMaskFilter(0);
}
void UBattleLeaderRuntime::RefreshSightTeamMasks()
{
 // Every body registered for hits (spawned, hired, replaced or restored) gets its side's bit; unchanged masks return at once.
 if(!SightTeamMaskEnabled()) return;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(const auto& P:HitIDs) if(UPrimitiveComponent* C=P.Key.Get()) if(const auto* U=Store->Find(P.Value); U&&U->Team>=0&&U->Team<2)
  C->SetMaskFilterOnBodyInstance(FMaskFilter(1<<U->Team));
}
namespace
{
 const TCHAR* SightHitKindNames[]={TEXT("clear"),TEXT("target"),TEXT("soldier"),TEXT("vehicle"),TEXT("floor"),TEXT("concealment"),TEXT("static_mesh"),TEXT("instanced_mesh"),TEXT("unit_part"),TEXT("other"),TEXT("soldier_target_side"),TEXT("soldier_observer_side")};
 const TCHAR* SightHitSourceNames[]={TEXT("perception"),TEXT("cansee"),TEXT("vehicle_far"),TEXT("area")};
 // Actor names without the numeric suffix (Wall_12 -> Wall).
 FString SightHitActorGroup(const AActor* A)
 {
  if(!A) return TEXT("(none)");
  FString N=A->GetActorNameOrLabel();int32 End=N.Len();
  while(End>0&&(FChar::IsDigit(N[End-1])||N[End-1]==TEXT('_')))--End;
  return End>0?N.Left(End):N;
 }
}
void UBattleLeaderRuntime::RecordSightHit(const FHitResult& Hit,int32 Target,uint64 Cycles,ESightHitSource Source) const
{
 // Game thread only (the parallel trial records in its apply stage). Kinds: see SightHitKindNames.
 int32 Kind=0;const UPrimitiveComponent* C=Hit.GetComponent();const AActor* A=Hit.GetActor();
 if(Hit.bBlockingHit)
 {
  const FString Group=SightHitActorGroup(A);
  if(const int32* ID=HitIDs.Find(Hit.GetComponent()))
  {
   // Another soldier: on the target's side (the observer's enemies) or the observer's side (target unknown: plain soldier).
   Kind=*ID==Target?1:IsVehicle(*ID)?3:2;
   const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* B=Store->Find(*ID);const auto* T=Target>=0?Store->Find(Target):nullptr;
   if(Kind==2&&B&&T) Kind=B->Team==T->Team?10:11;
  }
  else if(!C) Kind=9;
  else if(A&&A->ActorHasTag(TEXT("BattleConcealment"))) Kind=5;
  else if(C->GetClass()->GetName().Contains(TEXT("Landscape"))||Group.Contains(TEXT("Floor"))) Kind=4;
  else if(A&&(Cast<ABattleUnit>(A)||A==DisplayHost.Get())) Kind=8;
  else if(C->IsA<UInstancedStaticMeshComponent>()) Kind=7;
  else if(C->IsA<UStaticMeshComponent>()) Kind=6;
  else Kind=9;
  ++SightHitClasses.FindOrAdd(C?C->GetClass()->GetName():FString(TEXT("(none)")));
  if(A&&A->Tags.Num()) {for(const FName& Tag:A->Tags) ++SightHitTags.FindOrAdd(Tag.ToString());} else ++SightHitTags.FindOrAdd(TEXT("(none)"));
  ++SightHitActors.FindOrAdd(FString(SightHitKindNames[Kind])+TEXT(":")+Group);
 }
 ++SightHitKinds[Kind].Count;SightHitKinds[Kind].Cycles+=Cycles;
 ++SightHitSources[Source].Count;SightHitSources[Source].Cycles+=Cycles;
}
void UBattleLeaderRuntime::LogSightHitStats(const TCHAR* Why) const
{
 if(!SightHitStatsEnabled()||bSightHitsLogged) return;
 int64 N=0;uint64 Cy=0;for(const auto& K:SightHitKinds){N+=K.Count;Cy+=K.Cycles;}
 if(N==0) return; // other worlds (editor, previews) trace nothing
 bSightHitsLogged=true;
 const double Ms=FPlatformTime::ToMilliseconds64(Cy);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_HITS when=%s traces=%lld ms=%.1f us_per_trace=%.3f"),Why,N,Ms,N?Ms*1000./N:0.);
 for(int32 K=0;K<SightHitKindCount;++K) if(SightHitKinds[K].Count)
 {
  const double KMs=FPlatformTime::ToMilliseconds64(SightHitKinds[K].Cycles);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_HITS_KIND kind=%s traces=%lld share=%.4f ms=%.1f us_per_trace=%.3f"),SightHitKindNames[K],SightHitKinds[K].Count,double(SightHitKinds[K].Count)/FMath::Max<int64>(1,N),KMs,KMs*1000./SightHitKinds[K].Count);
 }
 for(int32 S=0;S<SightSourceCount;++S) if(SightHitSources[S].Count)
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_HITS_SOURCE source=%s traces=%lld ms=%.1f"),SightHitSourceNames[S],SightHitSources[S].Count,FPlatformTime::ToMilliseconds64(SightHitSources[S].Cycles));
 auto Top=[](const TMap<FString,int64>& Map,const TCHAR* Tag,int32 Limit)
 {
  TArray<TPair<FString,int64>> Rows;for(const auto& P:Map)Rows.Add(P);
  Rows.Sort([](const TPair<FString,int64>& L,const TPair<FString,int64>& R){return L.Value!=R.Value?L.Value>R.Value:L.Key<R.Key;});
  for(int32 I=0;I<FMath::Min(Limit,Rows.Num());++I) UE_LOG(LogFutureBattle,Display,TEXT("%s name=%s hits=%lld"),Tag,*Rows[I].Key,Rows[I].Value);
 };
 Top(SightHitClasses,TEXT("BATTLE_SIGHT_HITS_CLASS"),20);Top(SightHitTags,TEXT("BATTLE_SIGHT_HITS_TAG"),20);Top(SightHitActors,TEXT("BATTLE_SIGHT_HITS_ACTOR"),40);
}
bool UBattleLeaderRuntime::CanSelectLeader(int32 ID) const { return Actor(ID) && GetWorld()->GetSubsystem<UBattleState>()->CanSelectLeader(ID); }
bool UBattleLeaderRuntime::CanControlLeader(int32 ID) const { return CanSelectLeader(ID) && Mode && !Mode->bBattleEnded; }
const TArray<FBattleSquadSummary>& UBattleLeaderRuntime::Summaries()
{
 if(bSummaryDirty)
 {
  CachedSummaries.Reset(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
  for(const auto& P:Store->GetSquads()) if(P.Value.Team==0 && P.Value.CommandStatus!=EBattleCommandStatus::Legacy)
  { FBattleSquadSummary S; S.SquadID=P.Key; S.LeaderUnitID=P.Value.LeaderUnitID; S.Name=P.Value.DisplayName; S.Revision=P.Value.CommandRevision; S.bActive=P.Value.CommandStatus==EBattleCommandStatus::Active; S.bSelectable=CanSelectLeader(S.LeaderUnitID); CachedSummaries.Add(S); }
  CachedSummaries.Sort([](const auto& A,const auto& B){return A.SquadID<B.SquadID;}); bSummaryDirty=false;
 }
 return CachedSummaries;
}
void UBattleLeaderRuntime::OnDeath(int32 ID)
{
 CancelCoverAction(ID);
 VehicleIDs.Remove(ID); // a destroyed vehicle is a wreck: no armour or spotting rules
 // Stances are for the living: restore the normal body shape before the death pose is applied.
 SetStance(ID,BattleStance::Stand,GetWorld()->GetTimeSeconds(),true);Stances.Remove(ID);StanceLockedUntil.Remove(ID);NoCoverProneUntil.Remove(ID);CoverProbeFailedAt.Remove(ID);
 if(Phase3 && Phase3->bEnabled) Phase3->OnDeath(ID);
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); if(!U) return;
 // War score trial: every loss (shot or stopped by the leader's death) scores for the other team.
 if(bArea)GetWorld()->GetSubsystem<UBattleMatch>()->RecordWarLoss(U->Team);
 const auto* S=Store->FindSquad(U->SquadID);
 if(S && S->LeaderUnitID==ID)
 {
  // Scout pairs: the surviving member takes command instead of the squad stopping.
  int32 Heir=INDEX_NONE;
  if(BattleSquadType::LeaderHandover(S->SquadType)) for(int32 M:S->Members) if(M!=ID){const auto* MU=Store->Find(M);if(MU&&MU->IsAlive()){Heir=M;break;}}
  if(Heir==INDEX_NONE||!PromoteScout(S->SquadID,Heir)) { Store->DisableLeaderSquad(S->SquadID); Plans.Remove(S->SquadID); }
 }
 if(bArea&&S&&S->CommandStatus!=EBattleCommandStatus::Active)
 {
  auto* M=GetWorld()->GetSubsystem<UBattleMatch>();
  for(auto It=M->Area.Replacements.CreateIterator();It;++It)if(const auto* U2=Store->Find(It.Key());U2&&U2->SquadID==S->SquadID)It.RemoveCurrent();
  M->Area.AmmoTimers.Remove(S->SquadID);
 }
 if(auto* B=Bodies.Find(ID))
 {
  B->Velocity=FVector::ZeroVector;
  if(B->Actor.IsValid()) B->Actor->ApplyDeath();
  else
  {
   ClearFollowerRifleTurn(ID);
   B->Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); if(VAT){VAT->ShowNear(ID);VAT->Freeze(ID);}
   if(!VAT || !VAT->IsFar(ID))
   {
    B->Weapon->SetVisibility(false);
    // Body animation: a near follower plays a death clip and freezes on its last frame (StepBodyTimers); otherwise it tips over.
    UAnimSequence* Death=B->Mesh.IsValid()?DeathClip(ID):nullptr;
    if(Death){B->Mesh->SetRelativeScale3D(FVector(1.));B->Mesh->PlayAnimation(Death,false);BodyTimers.Add(ID,{GetWorld()->GetTimeSeconds()+Death->GetPlayLength(),true});}
    else {BattleFollowerBones::PrepareFreeze(B->Mesh.Get(),bReduceHiddenBones); B->Mesh->SetComponentTickEnabled(false); B->Mesh->SetRelativeRotation(FRotator(0,-90,85));}
   }
  }
  AddCorpse(ID);
 }
 bSummaryDirty=true;
}
bool UBattleLeaderRuntime::IssueOrder(int32 SID,EBattleOrder Order,FVector Goal,bool bMissionUpdate,bool bSector)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID);
 if(!Mode || Mode->bBattleEnded || !S || S->CommandStatus!=EBattleCommandStatus::Active || Goal.ContainsNaN()) return false;
 const FVector AskedGoal=Goal;
 if(!bMissionUpdate)SectorFireBack.Remove(SID);
 if(bSector)
 {
  const FIntPoint Cell=BattleSectors::Cell(Goal);
  if(!BattleFieldLayout::Of(GetWorld()).bSectors || !BattleSectors::Valid(Cell) || IsVehicle(S->LeaderUnitID))return false;
  Goal=BattleSectors::Center(Cell);
  BattleFieldLayout::Of(GetWorld()).Heights.Ground(Goal.X,Goal.Y,0,Goal.Z,nullptr,1.e9);
 }
 if(bWideField && Order!=EBattleOrder::Hold)
 {
  // A goal under a hill (given at height 0) is the place on the hill above it (map I: hills above the projection's reach).
  BattleFieldLayout::Of(GetWorld()).Heights.Lift(Goal);
  // A vehicle's order (with its path searches, partial routes into the city explore the whole reachable area) is timed for BATTLE_TANK_COST.
  const bool bVehicleOrder=IsVehicle(S->LeaderUnitID);const double OrderStart=bVehicleOrder?FPlatformTime::Seconds():0.;
  ON_SCOPE_EXIT{if(bVehicleOrder){TankOrderMs+=(FPlatformTime::Seconds()-OrderStart)*1000.;++TankOrders;}};
  const FVector RequestedGoal=Goal;auto Refuse=[&]{if(bVehicleOrder){auto& V=VehicleOrders.FindOrAdd(SID);V.Refused=RequestedGoal;V.RetryAt=GetWorld()->GetTimeSeconds()+5.;}};
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); FNavLocation Projected;
  // Vehicles use their own navigation data (wide agent) for the goal and the route.
  const ANavigationData* VehicleNav=NavFor(S->LeaderUnitID);AActor* VehicleContext=VehicleNav?Actor(S->LeaderUnitID):nullptr;
  const bool Projectable=Nav&&Nav->ProjectPointToNavigation(Goal,Projected,VehicleNav?FVector(600,600,400):FVector(100,100,400),VehicleNav);
  if(!Projectable){if(VehicleNav||NavFor(S->LeaderUnitID)!=nullptr||IsVehicle(S->LeaderUnitID))UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TANK_ORDER_FAIL squad=%d reason=project nav=%s goal=%s"),SID,VehicleNav?*VehicleNav->GetName():TEXT("none"),*Goal.ToCompactString());Refuse();return false;}
  auto* Route=Nav->FindPathToLocationSynchronously(GetWorld(),Position(S->LeaderUnitID),Projected.Location,VehicleContext);
  // A tank whose goal lies where it cannot fit (the narrow streets of map E) drives to the end of the partial route instead.
  const bool VehiclePartial=VehicleNav&&Route&&Route->IsValid()&&Route->IsPartial()&&Route->PathPoints.Num()>1;
  // Trial -BattleVehicleOrderHere=1: a vehicle whose goal is where it already stands (an artillery post that equals its position,
  // map I measurement 2026-11-21) gets no route from the navigation (zero length). Accept the order as arrived instead of refusing it
  // and retrying the same goal every 5 s.
  bool bVehicleHere=false;
  if(!VehiclePartial&&(!Route || !Route->IsValid() || Route->IsPartial())&&bVehicleOrder&&VehicleNav&&BattleTactics::VehicleOrderHere()&&
     FVector::Dist2D(Position(S->LeaderUnitID),Projected.Location)<=BattleSquadType::VehicleRadius(VehicleType(S->LeaderUnitID))+200.f)
  {bVehicleHere=true;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_VEHICLE_ORDER_HERE squad=%d goal=%s"),SID,*Projected.Location.ToCompactString());}
  if(!bVehicleHere&&!VehiclePartial&&(!Route || !Route->IsValid() || Route->IsPartial())){if(IsVehicle(S->LeaderUnitID))UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_TANK_ORDER_FAIL squad=%d reason=route valid=%d from=%s goal=%s projected=%s"),SID,Route&&Route->IsValid()?1:0,*Position(S->LeaderUnitID).ToCompactString(),*Goal.ToCompactString(),*Projected.Location.ToCompactString());Refuse();return false;}
  Goal=VehiclePartial?FVector(Route->PathPoints.Last()):Projected.Location;
 }
 TMap<int32,FVector> Goals;
 for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive())
 {
  FVector MemberGoal=Goal;
  if(Phase3 && Phase3->bEnabled)
  {
   const FVector D=(Goal-Position(ID)).GetSafeNormal2D(); const FVector Offset=FVector(-D.Y,D.X,0)*float(S->Members.IndexOfByKey(ID)%5-2)*80;
   auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   // Same formation relaxation policy as BattleNavigation::FormationGoal.
   // Resolve against geometry before assigning the new order (not the old defense radius).
   for(float Scale:{1.f,.5f,0.f})
   {
    FNavLocation Projected;
    if(!Nav || !Nav->ProjectPointToNavigation(Goal+Offset*Scale,Projected,FVector(100,100,400))) continue;
    auto* Route=Nav->FindPathToLocationSynchronously(GetWorld(),Position(ID),Projected.Location);
    if(Route && Route->IsValid() && !Route->IsPartial()) { MemberGoal=Projected.Location; break; }
   }
  }
  Goals.Add(ID,MemberGoal);
 }
 const int32 Revision=S->OrderRevision;
 const bool OK=Store->SetSquadOrder(SID,Order,Goal,Goals,bMissionUpdate,bSector);
 if(OK)
 {
  if(S->OrderRevision!=Revision) { auto& P=Plans.FindOrAdd(SID); P.bReplan=true; P.NextThink=0; }
  if(bWideField && !bMissionUpdate && S->Team==0) Store->SetSquadDelegated(SID,false);
  if(IsVehicle(S->LeaderUnitID)){auto& V=VehicleOrders.FindOrAdd(SID);V.Requested=AskedGoal;V.MissionRevision=S->MissionRevision;V.RetryAt=0;}
 }
 return OK;
}
void UBattleLeaderRuntime::Think(int32 SID,double Now)
{
 FBattleScopedMetric TrialTiming(SightExperiment.Enabled?GetWorld()->GetSubsystem<UBattleMetrics>():nullptr,EBattleMetricPhase::Squad);
 FPhase3Scope Timing(Phase3,EPhase3Cost::Decision);
 if(Phase3 && Phase3->bEnabled) ++Phase3->Decisions;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID); auto& P=Plans.FindChecked(SID);
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 if(Phase3 && Phase3->bEnabled && P.NextThink>0) Phase3->DecisionLateMS=FMath::Max(Phase3->DecisionLateMS,(Now-P.NextThink)*1000);
 P.NextThink=Now+FMath::Max(.2f,GetDefault<UBattleTuning>()->SquadDecisionSeconds);
 const int32 PreviousTarget=P.TargetID;
 // Trial -BattleDefendEngage=1: a defending squad targets any enemy within weapon range (BattleGroups.cpp).
 const bool Sector=S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission;
 const bool bArmourSectorGate=BattleTactics::ArmourSectorGateEnabled();
 TSet<int32> AllowedArmourTargets;
 // Trial -BattleSectorContact=1: shot at on the way or while holding, the squad also answers enemies outside its sector.
 const bool bShotAt=Sector && (SectorContactEnabled() || SectorMoveFireEnabled() || SectorFireBack.Contains(SID)) && SquadShotAt(*S,Now);
 const FIntPoint SectorCell=Sector?BattleSectors::Cell(S->Destination):FIntPoint(-1,-1);
 auto OutsideOK=[&](int32 ID,const FVector& At){return bShotAt && FVector::DistSquared(Position(ID),At)<=FMath::Square(TargetRange(ID));};
 FVector SelectedTargetAt=FVector::ZeroVector;
 P.TargetID=INDEX_NONE; double Best=DBL_MAX;
 P.Center=FVector::ZeroVector; int32 Count=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) { P.Center+=Position(ID); ++Count; } if(Count) P.Center/=Count;
 if(S->Order!=EBattleOrder::Move) for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) if(const auto* Contacts=Sense->GetContacts(ID))
  for(const auto& C:*Contacts) if(C.Value.bVisible && !C.Value.bShared)
  {
   if(bRangeAwareCombat && FVector::DistSquared(Position(ID),C.Value.LastKnownPosition)>FMath::Square(TargetRange(ID))) continue;
   if(Sector && BattleSectors::Cell(C.Value.LastKnownPosition)!=SectorCell && !OutsideOK(ID,C.Value.LastKnownPosition)) continue;
   if(!BattleTactics::OrderAllowsTarget(*S,C.Key,C.Value.LastKnownPosition))continue;
   if(bArmourSectorGate)AllowedArmourTargets.Add(C.Key);
   const double D=FVector::DistSquared(C.Value.LastKnownPosition,P.Center);
   if(D<Best || (D==Best && C.Key<P.TargetID)) { Best=D; P.TargetID=C.Key;if(BattleTactics::ObservedTactics())SelectedTargetAt=C.Value.LastKnownPosition; }
  }
 // Trial -BattleReacquireGrace=1: keep the previous target briefly while a member's own (non-shared) sighting is
 // fresh. Firing still requires a current line of sight (CanEngage/CanSee); this only avoids dropping the engagement.
 if(P.TargetID==INDEX_NONE && PreviousTarget!=INDEX_NONE && S->Order!=EBattleOrder::Move && BattleTactics::ReacquireGraceEnabled())
 {
  const float Hold=FMath::Max(0.f,GetDefault<UBattleTuning>()->TargetHoldSeconds);
  if(S->TacticalPlan.Action!=EBattleSquadTactic::Support || PreviousTarget==S->TacticalPlan.SupportTarget)
   for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) if(const auto* C=Sense->FindContact(ID,PreviousTarget))
   {
    if(C->bShared || Now-C->LastSeenTime>Hold+1.e-6) continue;
    if(bRangeAwareCombat && FVector::DistSquared(Position(ID),C->LastKnownPosition)>FMath::Square(TargetRange(ID))) continue;
    if(Sector && BattleSectors::Cell(C->LastKnownPosition)!=SectorCell && !OutsideOK(ID,C->LastKnownPosition)) continue;
    if(!BattleTactics::OrderAllowsTarget(*S,PreviousTarget,C->LastKnownPosition))continue;
    if(bArmourSectorGate)AllowedArmourTargets.Add(PreviousTarget);
    P.TargetID=PreviousTarget;if(BattleTactics::ObservedTactics())SelectedTargetAt=C->LastKnownPosition;SightExperiment.Cover(TEXT("squad_target_held"));break;
   }
 }
 // With the trial on, reprioritize before deriving the sector-contact and cover decisions from the chosen target.
 if(bArmourSectorGate)
 {
  P.ArmourTargetID=INDEX_NONE;
  if((BattleTactics::ObservedArmour()?HasArmourKnowledge(S->Team):VehicleIDs.Num()>0)&&S->Order!=EBattleOrder::Move&&BattleTactics::ArmourAI(S->Team))PickArmourTargets(SID,P,&AllowedArmourTargets,BattleTactics::ObservedTactics()?&SelectedTargetAt:nullptr);
 }
 P.bSectorContact=false;
 if(bShotAt && P.TargetID!=INDEX_NONE)
 {
  if(BattleTactics::ObservedTactics())P.bSectorContact=BattleSectors::Cell(SelectedTargetAt)!=SectorCell;
  else for(int32 ID:S->Members)if(const auto* C=Sense->FindContact(ID,P.TargetID)){P.bSectorContact=BattleSectors::Cell(C->LastKnownPosition)!=SectorCell;break;}
  if(P.bSectorContact)SightExperiment.Cover(TEXT("sector_contact_target"));
 }
 FVector TargetAt=FVector::ZeroVector;bool bTargetKnown=false;
 if(BattleTactics::ObservedTactics()){TargetAt=SelectedTargetAt;bTargetKnown=P.TargetID!=INDEX_NONE;}
 else if(P.TargetID!=INDEX_NONE)for(int32 ID:S->Members)if(const auto* C=Sense->FindContact(ID,P.TargetID)){TargetAt=C->LastKnownPosition;bTargetKnown=true;break;}
 // Trial -BattleSquadCoverCheck=1: a squad that fires on the move decides as a whole whether to stop in cover here - only when
 // it can hide and is at least even with the enemies it sees; otherwise it keeps firing on the move.
 if(SquadCoverCheckEnabled() && Sector && (SectorMoveFireEnabled() || SectorFireBack.Contains(SID)) && bTargetKnown &&
    BattleSectors::Cell(P.Center)!=SectorCell)
 {
  if(Now>=P.CoverCheckAt)
  {
   P.CoverCheckAt=Now+2.;
   int32 Own=0;TSet<int32> Seen;
   for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive())
   {
    ++Own;
    if(const auto* Contacts=Sense->GetContacts(ID))for(const auto& C:*Contacts)
     if(C.Value.bVisible && !C.Value.bShared && FVector::DistSquared(Position(ID),C.Value.LastKnownPosition)<=FMath::Square(TargetRange(ID)))Seen.Add(C.Key);
   }
   for(const auto& B:Bodies)if(const auto* O=Store->Find(B.Key);O && O->IsAlive() && O->Team==S->Team && O->SquadID!=SID &&
      FVector::DistSquared2D(Position(B.Key),P.Center)<FMath::Square(3000.))++Own;
   const bool bEven=Own>=Seen.Num();
   FVector HideFrom=TargetAt;if(FVector From;ShotDirectionEnabled() && SquadShotFrom(SID,Now,From) && ShotFromOther(P.Center,TargetAt,From))HideFrom=From;
   const bool bWas=P.bCoverStop;P.bCoverStop=bEven && SquadCanHide(SID,HideFrom,Now);
   SightExperiment.Cover(P.bCoverStop?TEXT("squad_cover_stop"):bEven?TEXT("squad_cover_move_on"):TEXT("squad_cover_outnumbered"));
   if(P.bCoverStop!=bWas)P.bReplan=true;
  }
 }
 else P.bCoverStop=false;
 // Trial -BattleBounding=1: a squad stopping in cover on the way moves on by halves.
 if(BoundingEnabled() && P.bCoverStop)
 {
  if(P.BoundSwitchAt==0){P.BoundGroup=-1;P.BoundSwitchAt=Now+4.;}
  else if(Now>=P.BoundSwitchAt){P.BoundGroup=P.BoundGroup==1?0:1;P.BoundSwitchAt=Now+7.;SightExperiment.Cover(TEXT("bounding_switch"));}
 }
 else {P.BoundGroup=-1;P.BoundSwitchAt=0;}
 // Trial -BattlePinnedFlank=1: shot at on the way for 20 s without getting 10 m closer to the sector.
 if(PinnedFlankEnabled() && Sector && bShotAt && BattleSectors::Cell(P.Center)!=SectorCell)
 {
  const double D=FVector::Dist2D(P.Center,S->Destination);
  if(P.PinnedSince==0 || D<P.PinnedBestDist-1000.){P.PinnedSince=Now;P.PinnedBestDist=D;}
  else if(!P.bPinned && bTargetKnown && Now-P.PinnedSince>=20.){P.bPinned=true;P.PinnedAt=TargetAt;SightExperiment.Cover(TEXT("squad_pinned"));}
 }
 else if(!bShotAt){P.PinnedSince=0;P.bPinned=false;}
 // Trial -BattleCloseIn=1: in its sector, a squad trading fire at long range while at least even moves a sector closer.
 if(CloseInEnabled() && Sector && bTargetKnown && !P.bCloseIn && BattleSectors::Cell(P.Center)==SectorCell && Now>=P.FarCheckAt)
 {
  P.FarCheckAt=Now+2.;
  bool bFar=FVector::DistSquared2D(P.Center,TargetAt)>FMath::Square(9000.);
  if(bFar)
  {
   int32 Own=0;TSet<int32> Seen;
   for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive())
   {
    ++Own;
    if(const auto* Contacts=Sense->GetContacts(ID))for(const auto& C:*Contacts)
     if(C.Value.bVisible && !C.Value.bShared && FVector::DistSquared(Position(ID),C.Value.LastKnownPosition)<=FMath::Square(TargetRange(ID)))Seen.Add(C.Key);
   }
   for(const auto& B:Bodies)if(const auto* O=Store->Find(B.Key);O && O->IsAlive() && O->Team==S->Team && O->SquadID!=SID &&
      FVector::DistSquared2D(Position(B.Key),P.Center)<FMath::Square(3000.))++Own;
   bFar=Own>=Seen.Num();
   if(!bFar)SightExperiment.Cover(TEXT("close_in_outnumbered"));
  }
  if(!bFar)P.FarSince=0;
  else if(P.FarSince==0)P.FarSince=Now;
  else if(Now-P.FarSince>=8.){P.bCloseIn=true;P.CloseInAt=TargetAt;SightExperiment.Cover(TEXT("close_in_wanted"));}
 }
 // Trial -BattleArrivalThreat=1: in the sector, a target far off the direction the arrival places face picks them again.
 if(ArrivalThreatEnabled() && Sector && bTargetKnown && BattleSectors::Cell(P.Center)==SectorCell && Now>=P.FacingCheckAt)
 {
  P.FacingCheckAt=Now+3.;
  if(!P.ArrivalFacing.IsNearlyZero() && FVector::DotProduct((TargetAt-S->Destination).GetSafeNormal2D(),P.ArrivalFacing)<.707f)
  {P.ArrivalGoals.Reset();P.bReplan=true;SightExperiment.Cover(TEXT("arrival_refacing"));}
 }
 // Armour AI (BattleArmourAI.cpp): while the squad sees a vehicle, small arms skip targets they cannot hurt and anti-tank gunners get a vehicle.
 if(!bArmourSectorGate)
 {
  P.ArmourTargetID=INDEX_NONE;
  if((BattleTactics::ObservedArmour()?HasArmourKnowledge(S->Team):VehicleIDs.Num()>0) && S->Order!=EBattleOrder::Move && BattleTactics::ArmourAI(S->Team)) PickArmourTargets(SID,P);
 }
 // Evaluation only: how long a squad goes without a visible target, and whether a member had just seen it.
 if(SightExperiment.Enabled && S->Order!=EBattleOrder::Move)
 {
  auto& E=SightExperiment;
  if(PreviousTarget!=INDEX_NONE && P.TargetID==INDEX_NONE)
  {
   E.Cover(TEXT("squad_target_lost"));E.SquadTargetLostAt.Add(SID,Now);
   bool Recent=false;
   for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) if(const auto* C=Sense->FindContact(ID,PreviousTarget)) if(!C->bShared && Now-C->LastSeenTime<=1.) {Recent=true;break;}
   if(Recent)E.Cover(TEXT("squad_target_lost_member_saw_within_1s"));
  }
  else if(PreviousTarget==INDEX_NONE && P.TargetID!=INDEX_NONE)
   if(const double* Lost=E.SquadTargetLostAt.Find(SID))
   {
    const double Gap=Now-*Lost;E.SquadTargetLostAt.Remove(SID);
    E.Cover(Gap<=.51?TEXT("squad_target_gap_le_0_5s"):Gap<=1.01?TEXT("squad_target_gap_0_5_1s"):Gap<=2.01?TEXT("squad_target_gap_1_2s"):Gap<=5.01?TEXT("squad_target_gap_2_5s"):TEXT("squad_target_gap_gt_5s"));
   }
 }
 if(Sector && S->Order==EBattleOrder::AttackMove && P.TargetID==INDEX_NONE)
 {
  int32 Present=0;
  for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive() && BattleSectors::Cell(Position(ID))==BattleSectors::Cell(S->Destination))++Present;
  if(Present*2>Count)
  {
   // Keep the assigned places and mission revision when the deployed squad begins holding.
   Store->Squads.FindChecked(SID).Order=EBattleOrder::Defend;
   for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive())Store->Units.FindChecked(ID).Order=EBattleOrder::Defend;
  }
 }
 const bool NewOrder=P.OrderRevision!=S->OrderRevision;
 if(NewOrder) { P.OrderRevision=S->OrderRevision; P.bReplan=true; }
 if(P.bReplan && Now>=P.NextPath && S->Order!=EBattleOrder::Hold)
 {
  P.NextPath=Now+1.; P.bReplan=false; P.Path.Reset();
  ++P.PathRequests;
  P.Center=FVector::ZeroVector; int32 N=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) { P.Center+=Position(ID); ++N; } if(N) P.Center/=N;
  FPhase3Scope PathTiming(Phase3,EPhase3Cost::Path);
  if(Phase3 && Phase3->bEnabled) ++Phase3->PathRequests;
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
  FNavLocation From,To;
  const FVector Origin=bWideField && !bStableMovement?Position(S->LeaderUnitID):P.Center;
  const ANavigationData* VehicleNav=NavFor(S->LeaderUnitID);AActor* VehicleContext=VehicleNav?Actor(S->LeaderUnitID):nullptr;const FVector VehicleExtent=VehicleNav?FVector(600,600,400):INVALID_NAVEXTENT;
  if(Nav && (Nav->ProjectPointToNavigation(Origin,From,VehicleExtent,VehicleNav) || (bWideField && Nav->ProjectPointToNavigation(Position(S->LeaderUnitID),From,VehicleExtent,VehicleNav))) && Nav->ProjectPointToNavigation(S->Destination,To,VehicleExtent,VehicleNav))
  {
   const double PathStart=SightExperiment.Enabled?FPlatformTime::Seconds():0;
   auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),From.Location,To.Location,VehicleContext);
   if(SightExperiment.Enabled){SightExperiment.PathMS.Add((FPlatformTime::Seconds()-PathStart)*1000);if(Path && Path->IsValid())SightExperiment.MaxPathLength=FMath::Max(SightExperiment.MaxPathLength,double(Path->GetPathLength()));}
   // Map E: a partial path still moves the squad as far as the route goes, instead of leaving it standing.
   if(Path && Path->IsValid() && Path->IsPartial() && BattleFieldLayout::IsUrban(GetWorld())) SightExperiment.Cover(TEXT("path_partial_used"));
   if(Path && Path->IsValid() && (!Path->IsPartial() || BattleFieldLayout::IsUrban(GetWorld()) || VehicleNav)) P.Path=Path->PathPoints;
   // Symmetric start trial (-BattleSymmetricStart=1): the navigation mesh is not a mirror image of itself, so the same trip takes
   // other streets from the east than from the west (up to 34 m apart on map E). An east-side squad on foot takes the 180-degree
   // image of the route a west-side squad would take, when every point of it lies on the mesh.
   if(bArea && !VehicleNav && P.Path.Num()>1 && BattleTactics::SymmetricStart(GetWorld()) && GetWorld()->GetSubsystem<UBattleMatch>()->AreaHome(S->Team).X>0)
   {
    FNavLocation MirrorFrom,MirrorTo;
    if(Nav->ProjectPointToNavigation(FVector(-From.Location.X,-From.Location.Y,From.Location.Z),MirrorFrom,FVector(100,100,300)) && Nav->ProjectPointToNavigation(FVector(-To.Location.X,-To.Location.Y,To.Location.Z),MirrorTo,FVector(100,100,300)))
     if(auto* West=Nav->FindPathToLocationSynchronously(GetWorld(),MirrorFrom.Location,MirrorTo.Location);West && West->IsValid() && !West->IsPartial() && West->PathPoints.Num()>1)
     {
      TArray<FVector> Image;bool bOnMesh=true;
      for(const FVector& Q:West->PathPoints)
      {
       FNavLocation On;
       if(!Nav->ProjectPointToNavigation(FVector(-Q.X,-Q.Y,Q.Z),On,FVector(60,60,300))){bOnMesh=false;break;}
       Image.Add(On.Location);
      }
      if(bOnMesh){Image[0]=From.Location;Image.Last()=To.Location;P.Path=MoveTemp(Image);SightExperiment.Cover(TEXT("path_mirrored"));}
      else SightExperiment.Cover(TEXT("path_mirror_off_mesh"));
     }
   }
   if(S->TacticalPlan.bAutonomous && S->TacticalPlan.Action==EBattleSquadTactic::Regroup && FVector::Dist2D(From.Location,To.Location)<50)
    P.Path={From.Location,To.Location}; // Followers can converge even when the leader is already at the rally point.
  }
  if(bWideField && bStableMovement) BuildMovementPlan(SID);
  else for(int32 ID:S->Members) if(auto* B=Bodies.Find(ID)) B->Waypoint=1;
 }
}
// Trial -BattleFollowerNoOverlap=1 (2026-11-18, performance, default 0): follower capsules, bodies and weapons do not
// generate overlap events. Nothing in the game listens for them; every capsule move otherwise runs an overlap query.
bool UBattleLeaderRuntime::FollowerOverlapsOff()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleFollowerNoOverlap="),V);return V!=0;}();
 return bOn;
}
void UBattleLeaderRuntime::Follow(int32 ID,float Dt,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); if(!U || !U->IsAlive()) return;
 auto& B=Bodies.FindChecked(ID);
 if(StepCoverAction(ID,Dt,Now))return;
 if(B.Actor.IsValid() && B.Actor->IsPlayerControlled())
 {
  // Direct control with -BattleStanceMove=0: walking stands the soldier up again. By default he keeps his stance and walks at
  // its speed (ABattleUnit::SetSprinting).
  if(BattleStance::MoveMode()==0 && StanceOf(ID)!=BattleStance::Stand && B.Actor->GetVelocity().SizeSquared2D()>2500.)SetStance(ID,BattleStance::Stand,Now,true,true);
  return;
 }
 const auto* S=Store->FindSquad(U->SquadID); auto* P=Plans.Find(U->SquadID); if(!P || S->CommandStatus!=EBattleCommandStatus::Active) return;
 FVector Pos=Position(ID), Direction=Forward(ID); B.Velocity=FVector::ZeroVector;
 // Armour AI (BattleArmourAI.cpp): an anti-tank gunner fires at the squad's vehicle target; with few rounds left and enemy vehicles
 // known to his team he does not spend them on soldiers. Vehicles take no cover actions and no stances.
 const bool bArmourAI=(BattleTactics::ObservedArmour()?HasArmourKnowledge(U->Team):VehicleIDs.Num()>0) && BattleTactics::ArmourAI(U->Team);
 const bool bVehicleBody=IsVehicle(ID);
 const int32 TargetID=bArmourAI && U->Weapon==EBattleWeapon::AntiTank && P->ArmourTargetID!=INDEX_NONE ? P->ArmourTargetID : P->TargetID;
 const auto* ArmourContact=BattleTactics::ObservedArmour()?GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,TargetID):nullptr;
 const bool bSaveRounds=bArmourAI && bArea && U->Weapon==EBattleWeapon::AntiTank && TargetID!=INDEX_NONE && !(BattleTactics::ObservedArmour()?(ArmourContact&&BattleSquadType::IsVehicle(ArmourContact->ObservedSquadType)):IsVehicle(TargetID)) &&
  U->Ammo+U->Reserve<=GetDefault<UBattleTuning>()->ATKeepRounds && GetWorld()->GetSubsystem<UBattleMatch>()->KnownEnemyVehicles(U->Team)>0;
 const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,TargetID);
 // Regrouping does not replenish ammunition, but may load existing reserve without an enemy target.
 if(bWideField && U->Ammo==0 && U->Reserve>0 && U->ReloadEndTime==0) Store->StartReload(ID,Now);
 const bool ObservedEngage=Contact && Contact->bVisible && !Contact->bShared && !bSaveRounds &&
  (!bRangeAwareCombat || FVector::DistSquared(Pos,Contact->LastKnownPosition)<=FMath::Square(UnderFire(ID)?BattleRanges::Reach(U->Weapon):BattleRanges::Engage(U->Weapon))) &&
  (S->TacticalPlan.Action!=EBattleSquadTactic::Support || TargetID==S->TacticalPlan.SupportTarget);
 const bool CanEngage=ObservedEngage&&CanSee(ID,TargetID);
 // Evaluation only: an engagement that only the return-fire rule allows (beyond the AI engagement distance, within reach).
 if(CanEngage&&SightExperiment.Enabled&&bRangeAwareCombat&&UnderFire(ID)&&FVector::DistSquared(Pos,Contact->LastKnownPosition)>FMath::Square(BattleRanges::Engage(U->Weapon)))SightExperiment.Cover(TEXT("return_fire_engage"));
 // Trial -BattleSectorContact=1: against an enemy outside the sector, a soldier stops only in cover (or on his way to it);
 // in the open he keeps going to the sector and fires on the move.
 const bool bOutsideContact=P->bSectorContact && P->TargetID!=INDEX_NONE && S->Order!=EBattleOrder::Move && !bVehicleBody;
 // -BattleSectorMove (the player's sector move) and -BattleSectorMoveFire (every sector mission): fire on the move until the
 // soldier is inside the sector (no stop, no cover walk).
 // -BattleBounding: this soldier's half moves on now; he leaves his cover or the place he walks to.
 const bool bBoundMove=P->bCoverStop && P->BoundGroup>=0 && S->Members.IndexOfByKey(ID)%2==P->BoundGroup && !bVehicleBody &&
  S->bSectorMission && BattleSectors::Cell(Pos)!=BattleSectors::Cell(S->Destination);
 if(bBoundMove && (U->CoverPhase || CoverApproaches.Contains(ID))){CancelCoverAction(ID);CoverApproaches.Remove(ID);}
 if(bBoundMove)FirePositions.Remove(ID);
 const bool bMoveFire=bBoundMove || ((SectorMoveFireEnabled() || (!SectorFireBack.IsEmpty() && SectorFireBack.Contains(U->SquadID))) && !bVehicleBody &&
  S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission && !P->bCoverStop &&
  BattleSectors::Cell(Pos)!=BattleSectors::Cell(S->Destination) && !U->CoverPhase && !CoverApproaches.Contains(ID));
 // -BattleArrivalWalk (and evaluation): his own arrival place in the sector, still more than 1.5 m away, on a route that goes past
 // the end of the shared path (the tail to that place).
 const FVector* ArrivalGoal=(bWideField && bStableMovement && P->ArrivalTails.Contains(ID))?P->ArrivalGoals.Find(ID):nullptr;
 const TArray<FVector>* OwnRoute=bWideField && bStableMovement?P->Corridors.Find(ID):nullptr;
 const bool bTailLeft=OwnRoute && !P->Path.IsValidIndex(B.Waypoint) && OwnRoute->IsValidIndex(B.Waypoint);
 const bool bWalkIn=ArrivalGoal && !bVehicleBody && S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission &&
  BattleSectors::Cell(Pos)==BattleSectors::Cell(S->Destination) && FVector::DistSquared2D(Pos,*ArrivalGoal)>FMath::Square(150.) &&
  !U->CoverPhase && !CoverApproaches.Contains(ID);
 const bool bArrivalWalk=ArrivalWalkEnabled() && bWalkIn && S->Order!=EBattleOrder::Hold && OwnRoute && OwnRoute->IsValidIndex(B.Waypoint);
 const bool bOnRoute=P->Path.IsValidIndex(B.Waypoint) || (ArrivalWalkEnabled() && bTailLeft);
 const bool StopForCombat=!bMoveFire && !bArrivalWalk && (P->TargetID!=INDEX_NONE || (bArmourAI && P->ArmourTargetID!=INDEX_NONE)) && S->Order!=EBattleOrder::Move &&
  !(bOutsideContact && !U->CoverPhase && !CoverApproaches.Contains(ID) && !(P->bCoverStop && FirePositions.Contains(ID)));
 const bool CombatContact=StopForCombat || bOutsideContact;
 // Evaluation (arrival_walk_*, frames): stopped for combat short of his place / standing at the shared end with his tail left /
 // walking in under the trial.
 if(SightExperiment.Enabled)
 {
  if(bWalkIn && StopForCombat)SightExperiment.Cover(TEXT("arrival_walk_stopped_short_frames"));
  if(bTailLeft && !StopForCombat && S->Order!=EBattleOrder::Hold && !ArrivalWalkEnabled())SightExperiment.Cover(TEXT("arrival_walk_tail_cut_frames"));
  if(bArrivalWalk && (P->TargetID!=INDEX_NONE || (bArmourAI && P->ArmourTargetID!=INDEX_NONE)) && S->Order!=EBattleOrder::Move)SightExperiment.Cover(TEXT("arrival_walk_fire_on_move_frames"));
  if(ArrivalGoal && FVector::DistSquared2D(Pos,*ArrivalGoal)<=FMath::Square(150.))SightExperiment.Cover(TEXT("arrival_walk_at_place_frames"));
 }
 if(BattleStance::Enabled(GetWorld()) && !bVehicleBody)
 {
  const bool WillMove=(S->Order!=EBattleOrder::Hold && !StopForCombat && bOnRoute) || CoverApproaches.Contains(ID);
  const bool LostSight=!CanEngage && Contact && !Contact->bVisible && Now-Contact->LastSeenTime<=4.;
  // Trial -BattleStanceMove=2: a threat is being under fire or seeing any enemy himself (a Move order has no squad target).
  const bool bThreat=BattleStance::MoveMode()>=2 && WillMove && (UnderFire(ID) || (Contact && Contact->bVisible && !Contact->bShared) || SeesEnemy(ID));
  UpdateAIStance(ID,Now,WillMove,StopForCombat,LostSight,bThreat);
  // Getting up takes time: no walking until the soldier is standing. With -BattleStanceMove=2 he walks in his stance instead
  // (crouched or crawling, at its speed below).
  if(WillMove && StanceOf(ID)!=BattleStance::Stand && BattleStance::MoveMode()<2){if(SightExperiment.Enabled)CoverGate.Add(ID,8);Store->SetAction(ID,EBattleAction::Idle);return;}
 }
 // -BattleContactCover (this soldier's team): per-frame probe budget, 2 s retry, walks to cover and to free firing positions.
 const bool bContactCover=BattleTactics::ContactCover(U->Team) && !bVehicleBody;
 const bool ProbeBudget=BattleTactics::AICoverProbeBudgetEnabled() || bContactCover;
 if(ProbeBudget && CoverProbeFrame!=GFrameCounter){CoverProbeFrame=GFrameCounter;CoverProbesThisFrame=0;}
 const auto* CoverTuning=GetDefault<UBattleTuning>();
 // Trial -BattleLeaderGuard=1: an AI leader (one per squad) does not wait for the shared probe slot.
 const bool LeaderFirst=BattleTactics::LeaderGuardEnabled() && ID==S->LeaderUnitID;
 // -BattleCoverPriority: just hit, he searches without waiting for the shared slot.
 if(PriorityProbeFrame!=GFrameCounter){PriorityProbeFrame=GFrameCounter;PriorityProbesThisFrame=0;}
 const bool HitFirst=CoverPriorityEnabled() && HitWithin(ID,Now,5.) && PriorityProbesThisFrame<4;
 // -BattleCoverPriority: a soldier hit after a failed search searches again 2 s after that failure, not after the full wait.
 double RetryAt=CoverProbeAfter.FindRef(ID);
 if(HitFirst)if(const double* Failed=CoverProbeFailedAt.Find(ID))RetryAt=FMath::Min(RetryAt,*Failed+2.);
 const bool ProbeSlot=LeaderFirst || HitFirst || (ProbeBudget?CoverProbesThisFrame<FMath::Max(1,CoverTuning->AICoverProbesPerFrame):Now>=NextCoverProbe);
 if(BattleTactics::CoverActionsEnabled() && Contact && Contact->bVisible)
 {
  auto& Diag=SightExperiment;
  if(!StopForCombat)Diag.Cover(TEXT("ai_visible_moving_frames"));
  else{Diag.Cover(TEXT("ai_eligible_frames"));if(Now<CoverProbeAfter.FindRef(ID))Diag.Cover(TEXT("ai_wait_retry_frames"));else if(!ProbeSlot)Diag.Cover(TEXT("ai_wait_budget_frames"));}
 }
 // Cover validation (default ON): never take cover from a contact that this soldier cannot shoot at from here.
 const bool CoverInRange=!BattleTactics::CoverValidationEnabled() || (Contact && FVector::DistSquared2D(Pos,Contact->LastKnownPosition)<=FMath::Square(CoverTuning->EffectiveWeaponRange()));
 // A mortar gunner stays at his emplacement (moving would undo the set-up).
 const bool bSeenCover=CombatContact && Contact && Contact->bVisible && CoverInRange;
 // Evaluation only: which gate of the cover search this soldier is at (read when he is hit in the open).
 if(SightExperiment.Enabled && !bVehicleBody)CoverGate.Add(ID,!Contact||!Contact->bVisible?0:!CombatContact?1:bMoveFire?2:!CoverInRange?3:
  U->Weapon==EBattleWeapon::Mortar?6:Now<RetryAt?5:!ProbeSlot?4:7);
 // -BattleUnderFireCover: just hit and standing (not walking his path): look for cover from a target he knows of, even unseen.
 FVector HitFrom;
 const bool bHitCover=UnderFireCoverEnabled() && !bSeenCover && !bMoveFire && !bVehicleBody && S->Order!=EBattleOrder::Move && !U->CoverPhase &&
  !CoverApproaches.Contains(ID) && UnitShotFrom(ID,Now,HitFrom) && Contact && Now-Contact->LastSeenTime<=FMath::Min(5.f,U->Perception.MemorySeconds) &&
  FVector::DistSquared2D(Pos,Contact->LastKnownPosition)<=FMath::Square(TargetRange(ID)) && !(S->Order!=EBattleOrder::Hold && !StopForCombat && P->Path.IsValidIndex(B.Waypoint));
 if(BattleTactics::CoverActionsEnabled() && (bSeenCover || bHitCover) && !bMoveFire && ProbeSlot && Now>=RetryAt && U->Weapon!=EBattleWeapon::Mortar && !bVehicleBody)
 {
  NoCoverProneUntil.Remove(ID); // a new search: lie down again only if it also finds nothing
  if(LeaderFirst){}else if(HitFirst){++PriorityProbesThisFrame;SightExperiment.Cover(TEXT("cover_priority_probe"));}else if(ProbeBudget)++CoverProbesThisFrame;else NextCoverProbe=Now+FMath::Max(0.f,CoverTuning->AICoverProbeInterval);
  CoverProbeAfter.Add(ID,Now+FMath::Max(0.f,LeaderFirst?CoverTuning->LeaderGuardCoverRetrySeconds:(bContactCover||HitFirst)?2.f:CoverTuning->AICoverRetrySeconds));
  // Map E: cover is plentiful but small, so AI looks a little further for it (6.5m instead of 3.5m; AI cognition only).
  const float LocalRadius=(ProbeBudget||LeaderFirst)?CoverTuning->AICoverSearchRadius:BattleFieldLayout::IsUrban(GetWorld())?650.f:CoverTuning->CoverActionSearchRadius;
  // -BattleShotDirection: take cover from where this soldier was just hit from, when that is not his target.
  FVector CoverThreat=Contact->LastKnownPosition;
  if(FVector From;ShotDirectionEnabled() && UnitShotFrom(ID,Now,From))
  {if(ShotFromOther(Pos,CoverThreat,From)){CoverThreat=From;SightExperiment.Cover(TEXT("cover_from_shot_direction"));}else SightExperiment.Cover(TEXT("cover_shot_by_target"));}
  if(bHitCover){SightExperiment.Cover(Contact->bVisible?TEXT("hit_cover_request_seen_far"):TEXT("hit_cover_request_unseen"));if(ShotFromOther(Pos,CoverThreat,HitFrom))CoverThreat=HitFrom;}
  if(RequestCoverAction(ID,CoverThreat,TargetID,LocalRadius)){CoverApproaches.Remove(ID);CoverProbeFailedAt.Remove(ID);if(bOutsideContact)SightExperiment.Cover(TEXT("sector_contact_cover_here"));if(bHitCover)SightExperiment.Cover(TEXT("hit_cover_here"));return;}
  // Trial -BattleCoverApproach=1: no usable cover here, so walk to a hidden slot beyond the local search radius.
  const bool bStrictSector=CoverStrictEnabled() && S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission;
  // -BattleSectorCoverWalk: inside his own sector, no cover here = walk to cover in the sector.
  const bool bSectorWalk=SectorCoverWalkEnabled() && S->bSectorMission && S->TacticalPlan.Action==EBattleSquadTactic::Mission && BattleSectors::Cell(Pos)==BattleSectors::Cell(S->Destination);
  bool bFireTried=false;
  if((BattleTactics::CoverApproachEnabled(GetWorld()) || bContactCover || bOutsideContact || P->bCoverStop || bStrictSector || bSectorWalk || bHitCover) && !CoverApproaches.Contains(ID))
  {
   bool bFound=StartCoverApproach(ID,CoverThreat,TargetID,Now,LocalRadius);
   if(bSectorWalk && bFound)SightExperiment.Cover(TEXT("sector_walk_to_cover"));
   if(bHitCover && bFound)SightExperiment.Cover(TEXT("hit_cover_walk"));
   // -BattleSquadCoverCheck: the squad stops in cover, so a member without a slot of his own gathers right behind one.
   if(!bFound && (P->bCoverStop || bSectorWalk || bHitCover)){bFound=StartHuddle(ID,CoverThreat,TargetID,Now);if(bSectorWalk && bFound)SightExperiment.Cover(TEXT("sector_walk_to_huddle"));if(bHitCover && bFound)SightExperiment.Cover(TEXT("hit_cover_huddle"));}
   if(!bFound && (bContactCover || bOutsideContact || bStrictSector || bSectorWalk || bHitCover)){bFireTried=true;bFound=StartFirePosition(ID,CoverThreat,TargetID,Now);if(bSectorWalk)SightExperiment.Cover(bFound?TEXT("sector_walk_to_fire_position"):TEXT("sector_walk_none"));if(bHitCover)SightExperiment.Cover(bFound?TEXT("hit_cover_fire_position"):TEXT("hit_cover_none"));}
   if(bOutsideContact)SightExperiment.Cover(bFound?TEXT("sector_contact_cover_walk"):TEXT("sector_contact_keep_moving"));
  }
  // -BattleNoCoverFallback: no cover found and nothing else started - a less exposed place nearby, else lie down.
  if(NoCoverFallbackEnabled() && bSeenCover && !CoverApproaches.Contains(ID) && !FirePositions.Contains(ID))
  {
   if(!bFireTried && StartFirePosition(ID,CoverThreat,TargetID,Now)){CoverApproaches.FindChecked(ID).bFallback=true;SightExperiment.Cover(TEXT("no_cover_fire_position"));}
   else if(bFirePositionHereGood)SightExperiment.Cover(TEXT("no_cover_here_is_good"));
   else if(BattleStance::Enabled(GetWorld())){NoCoverProneUntil.Add(ID,Now+5.);SightExperiment.Cover(TEXT("no_cover_prone"));}
  }
  if(CoverPriorityEnabled()){if(CoverApproaches.Contains(ID))CoverProbeFailedAt.Remove(ID);else CoverProbeFailedAt.Add(ID,Now);}
 }
 const bool Approaching=CoverApproaches.Contains(ID) && StepCoverApproach(ID,Dt,Now,CombatContact?TargetID:INDEX_NONE);
 if(StopForCombat || S->Order==EBattleOrder::Hold) B.bTrafficWait=false;
 const bool bDriving=S->Order!=EBattleOrder::Hold && !StopForCombat && bOnRoute;
 if(bDriving && !Approaching && FirePositions.Num())FirePositions.Remove(ID); // back on the march: the place is free again
 // -BattleContactCover: a soldier walking to cover finishes that walk before he follows the squad's path again.
 if(S->Order!=EBattleOrder::Hold && !StopForCombat && bOnRoute && !(Approaching && (bContactCover || bOutsideContact)))
 {
  if(bWideField && bStableMovement) StableMove(ID,Dt,Now,Direction);
  else
  {
  const FVector Segment=(P->Path[B.Waypoint]-P->Path[B.Waypoint-1]).GetSafeNormal2D();
  const int32 Slot=S->Members.IndexOfByKey(ID);
  // Shrink to the corridor center after a collision, then ask the commander to replan.
  const float Width=B.StalledSince>0?0.f:float(Slot%5-2)*80.f;
  const FVector Way=P->Path[B.Waypoint]+FVector(-Segment.Y,Segment.X,0)*Width;
  FVector D=(Way-Pos).GetSafeNormal2D();
 const bool bObservedAvoidance=BattleTactics::ObservedTactics();
 TMap<FIntPoint,int32> ObservedGrid;TArray<TArray<int32>> ObservedBuckets;TMap<int32,FVector> ObservedPositions;
 if(bObservedAvoidance)BuildObservedAvoidance(ID,ObservedGrid,ObservedBuckets,ObservedPositions);
 const auto& MovementGrid=bObservedAvoidance?ObservedGrid:AvoidanceGrid;
 const auto& MovementBuckets=bObservedAvoidance?ObservedBuckets:AvoidanceBuckets;
  // A shared corridor, with local spacing; no per-follower NavMesh query.
  FVector Separation=FVector::ZeroVector;
  const FIntPoint Cell(FMath::FloorToInt(Pos.X/150.),FMath::FloorToInt(Pos.Y/150.));
  int32 Checked=0;
  for(int32 X=-1;X<=1 && Checked<32;++X) for(int32 Y=-1;Y<=1 && Checked<32;++Y)
   if(const int32* NearbyBucket=MovementGrid.Find(Cell+FIntPoint(X,Y))) for(int32 Other:MovementBuckets[*NearbyBucket])
   {
    if(Other==ID || (!bObservedAvoidance&&!Store->Find(Other)->IsAlive())) continue;
    if(++Checked>32) break;
    const FVector Away=Pos-(bObservedAvoidance?ObservedPositions.FindChecked(Other):Position(Other)); const double Dist=Away.Size2D();
    if(Dist>1 && Dist<95) Separation+=Away.GetSafeNormal2D()*(95-Dist)/95;
   }
  Direction=bWideField?LimitedSteering(D,Separation,GetDefault<UBattleTuning>()->WideAvoidanceMaxSide):(D+Separation*.65).GetSafeNormal2D();
  // Heavy infantry moves slower (squad type speed scale); rifle squads keep the configured speed.
  const float TypeSpeed=BattleSquadType::SpeedAt(S->SquadType,GetWorld(),Pos);
  const float Speed=GetDefault<UBattleTuning>()->MoveSpeed*TypeSpeed;
  if(TypeSpeed!=1.f&&B.Actor.IsValid())B.Actor->GetCharacterMovement()->MaxWalkSpeed=Speed;
  // -BattleStanceMove=2: walking crouched or crawling, at that stance's share of the speed.
  const float StanceScale=BattleStance::MoveMode()>=2?BattleStance::MoveScale(StanceOf(ID)):1.f;
  if(StanceScale<1.f)SightExperiment.Cover(StanceOf(ID)==BattleStance::Prone?TEXT("stance_move_prone_frames"):TEXT("stance_move_crouch_frames"));
  if(B.Actor.IsValid())
  {
   // Vehicles turn at their own rate and drive along the hull.
   float VehicleScale=1.f;if(IsVehicle(ID))SteerVehicle(ID,Direction,VehicleScale);
   B.Actor->AddMovementInput(Direction,VehicleScale*StanceScale); B.Velocity=B.Actor->GetVelocity();
   if(B.Velocity.Size2D()<10) { if(B.StalledSince==0) B.StalledSince=Now; }
   else B.StalledSince=0;
   if(B.StalledSince>0 && Now-B.StalledSince>2) { P->bReplan=true; B.StalledSince=Now; }
  }
  else
  {
   FHitResult Hit; const FVector Before=B.Capsule->GetComponentLocation();
   B.Capsule->MoveComponent(Direction*FMath::Min(Speed*StanceScale*Dt,float(FVector::Dist2D(Way,Pos))),Direction.Rotation().Quaternion(),true,&Hit);
   auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity); F.Position=B.Capsule->GetComponentLocation(); B.Velocity=(F.Position-Before)/FMath::Max(Dt,.001f);
   if(Hit.bBlockingHit && B.StalledSince==0) B.StalledSince=Now;
   else if(!Hit.bBlockingHit) B.StalledSince=0;
   if(B.StalledSince>0 && Now-B.StalledSince>2) { P->bReplan=true; B.StalledSince=Now; }
  }
  if(FVector::Dist2D(Position(ID),Way)<90) ++B.Waypoint;
  }
 }
 else if(B.Actor.IsValid() && !Approaching) B.Actor->GetCharacterMovement()->StopMovementImmediately();
 // Indirect fire (mortar gunner, self-propelled gun; BattleArtillery.cpp): set-up while still, target from known positions, firing.
 FVector ArtilleryDirection=FVector::ZeroVector;
 const bool bArtilleryAim=bArea && IsArtillery(ID) && StepArtillery(ID,Now,B.Velocity.Size2D()>20.f,ArtilleryDirection);
 FVector BaseTarget;const bool BaseEngage=!CanEngage&&S->Order!=EBattleOrder::Move&&GetWorld()->GetSubsystem<UBattleMatch>()->CanFireAtBase(ID,BaseTarget);
 // Stances: fire from the current eye height, and aim at the height the visible target presents.
 const FVector Muzzle(0,0,BattleStance::EyeOffset(StanceOf(ID)));
 if(BaseEngage)Direction=(BaseTarget-(Pos+Muzzle)).GetSafeNormal();
 else if(CanEngage)
 {
  // A target leaning out past an edge for a blind shot is aimed at where he shows (he is seen there, PrepareSightTrace).
  FVector Shown;
  Direction=((SideExposure(TargetID,Now,Shown)?Shown:Contact->LastKnownPosition+FVector(0,0,TargetAimOffset(TargetID)))-(Pos+Muzzle)).GetSafeNormal();
 }
 else if(S->TacticalPlan.Action==EBattleSquadTactic::Flank)
 {
  // Movement has already consumed its steering vector. Keep watch toward known
  // contact information while moving sideways; never resolve the enemy body here.
  const auto* Known=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,S->TacticalPlan.FlankTarget);
  Direction=((Known?Known->LastKnownPosition:S->TacticalPlan.FlankContact)-Position(ID)).GetSafeNormal2D();
 }
 else if(FVector From;HitFaceEnabled() && !bArtilleryAim && !IsVehicle(ID) && HitWithin(ID,Now,2.) && UnitShotFrom(ID,Now,From) && FVector::DistSquared2D(From,Pos)>1.e4)
 {Direction=(From-Pos).GetSafeNormal2D();SightExperiment.Cover(TEXT("hit_face_turned"));}
 // A vehicle turns its turret to the target; its hull keeps the driving heading (SteerVehicle).
 // The main gun's target (stage 2) takes the turret; the machine gun keeps firing along Direction at its own target.
 // A mortar gunner without a direct target faces his indirect-fire target; a self-propelled gun turns its gun to it.
 if(bArtilleryAim && !CanEngage && !BaseEngage && !IsVehicle(ID)) Direction=ArtilleryDirection;
 if(B.Actor.IsValid())
 {
  if(IsVehicle(ID))
  {
   FVector GunDirection=FVector::ZeroVector; const bool bGun=!bArtilleryAim && TankGunTarget(ID,Now,GunDirection);
   // Armour AI: a stopped vehicle turns its hull, and with it its forward view, to what it fires at, or to the nearest known enemy.
   if(bArmourAI && bVehicleBody && !bDriving)
   {
    FVector Face=bArtilleryAim?ArtilleryDirection:bGun?GunDirection:(CanEngage||BaseEngage)?Direction.GetSafeNormal2D():FVector::ZeroVector;
    if(Face.IsNearlyZero()) Face=VehicleWatchDirection(ID);
    FaceVehicle(ID,Face);
   }
   if(bArtilleryAim) AimTurret(ID,ArtilleryDirection); else { AimTurret(ID,bGun?GunDirection:Direction); StepTankGun(ID,Now); }
  }
  else B.Actor->SetActorRotation(FRotator(0,Direction.Rotation().Yaw,0));
 }
 else
 {
  Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=Direction.GetSafeNormal2D();
  B.Capsule->SetWorldRotation(FRotator(0,Direction.Rotation().Yaw,0));
  const int32 Pose=U->ReloadEndTime>Now?2:B.Velocity.SizeSquared2D()>400?1:0;
  const bool Run=ShowsRun(ID,Pose,Now);
  // A stance transition still playing (SetStance; BodyTimers) is not cut by the walk: its timer puts the pose's clip on after it
  // (he stood up from a crouch in one frame when he walked off).
  const FBodyTimer* Change=BodyTimers.Find(ID);
  const bool bChanging=Change && !Change->bFreeze && !Change->bSettle;
  if(Pose!=B.Pose || Run!=B.bRunPose) { B.Pose=Pose; B.bRunPose=Run; if(VAT)VAT->ChangePose(ID,Pose); if((!VAT || !VAT->IsFar(ID)) && !bChanging)PlayFollowerClip(ID,Pose); }
  // Walking crouched or prone: the steps keep pace as his speed changes (traffic ahead, the start of the walk).
  else if(Pose==1 && !bChanging && StanceOf(ID)!=BattleStance::Stand && (!VAT || !VAT->IsFar(ID)) && !B.CoverClipPhase && !B.bBlindPose)
   if(UAnimSingleNodeInstance* Node=B.Mesh->GetSingleNodeInstance())
    if(const auto* Clip=Cast<UAnimSequence>(Node->GetAnimationAsset()))Node->SetPlayRate(BattleBodyAnim::MatchRate(Clip,float(B.Velocity.Size2D())));
 }
 Store->UpdateAim(ID,Now,B.Velocity.Size2D(),Direction,true);
 if(B.Actor.IsValid())B.Actor->NoteDisplayAim(Direction,Now);
 if(BaseEngage)Fire(ID,Pos+Muzzle,Direction,false);
 if(CanEngage) { if(U->Ammo==0) Store->StartReload(ID,Now); else if(Fire(ID,Pos+Muzzle,Direction,false) && S->TacticalPlan.Action==EBattleSquadTactic::Support) GetWorld()->GetSubsystem<UBattleSquadTactics>()->RecordSupportShot(S->SquadID,TargetID); }
 Store->SetAction(ID,U->ReloadEndTime>0?EBattleAction::Reloading:(BattleTactics::ObservedTactics()?ObservedEngage:CanEngage)?EBattleAction::Engaging:B.Velocity.SizeSquared2D()>1?EBattleAction::Moving:EBattleAction::Idle);
}
bool UBattleLeaderRuntime::Fire(int32 ID,FVector Start,FVector Direction,bool bPlayerShot)
{
 FPhase3Scope Timing(Phase3,EPhase3Cost::Shot);
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
 if(!S || S->CommandStatus!=EBattleCommandStatus::Active || !U->IsAlive() || Mode->bBattleEnded || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
 // A mortar has no direct fire (its shells go through FireArtillery, BattleArtillery.cpp).
 if(U->Weapon==EBattleWeapon::Mortar) return false;
 const double Now=GetWorld()->GetTimeSeconds(); const auto* T=GetDefault<UBattleTuning>();
 bool bBlind=false;
 if(U->CoverPhase && U->CoverPhase!=4)
 {
  // Blind fire (BattleBlindFire.cpp): the player fires without aiming, or an AI soldier in the trial; the shot leaves above or
  // beside the cover while the body stays hidden. Aiming and firing (or no free origin) peeks as before.
  const bool bWantBlind=U->CoverPhase==2 && U->Weapon!=EBattleWeapon::AntiTank && !U->bLauncherSelected &&
   (bPlayerShot?BattleBlindFire::PlayerEnabled() && Actor(ID) && Actor(ID)->GetAimLevel()==0:bAIBlindShot);
  FVector Origin;
  if(bWantBlind && BlindFireOrigin(ID,Start+Direction.GetSafeNormal()*BattleRanges::Reach(U->Weapon),Origin))
  {
   const FVector Aim=Start+Direction.GetSafeNormal()*BattleRanges::Reach(U->Weapon);
   Start=Origin;Direction=(Aim-Origin).GetSafeNormal();bBlind=true;
   BlindFireLook.Add(ID,Origin+Direction.GetSafeNormal2D()*200.f);
   if(BattleBlindFire::IsOverTop(*U))BlindFireTop.Add(ID,BattleBlindFire::CoverTop(GetWorld(),Origin,Position(ID)));
   else BlindFireSide.Add(ID,Origin);
  }
  else
  {
   if(bPlayerShot && U->CoverPhase==2){auto& Cover=Store->Units.FindChecked(ID);Cover.CoverPhase=3;Cover.CoverSeconds=3;}
   return false;
  }
 }
 if(bPlayerShot && U->bLauncherSelected)
  return FireExoLauncher(ID,Start+Direction.GetSafeNormal()*BattleRanges::Reach(EBattleWeapon::AntiTank),Now);
 Store->UpdateAim(ID,Now,Actor(ID)?Actor(ID)->GetVelocity().Size2D():Bodies.FindChecked(ID).Velocity.Size2D(),Direction,Actor(ID)?Actor(ID)->GetCharacterMovement()->IsMovingOnGround():true);
 if(ABattleUnit* Shooter=Actor(ID);Shooter && !bPlayerShot)Shooter->NoteDisplayAim(Direction,Now);
 if(!Store->ConsumeShot(ID,Now,T->ShotInterval(U->Weapon,bPlayerShot))) return false;
 if(SightExperiment.Started)
 {
  auto& E=SightExperiment;++E.Shots;
  if(const auto* ShooterSquad=Store->FindSquad(U->SquadID); ShooterSquad && ShooterSquad->SquadType<BattleSquadType::Count && U->Team>=0 && U->Team<2) ++E.TypeShots[U->Team][ShooterSquad->SquadType];
  const auto* P=Plans.Find(U->SquadID);const auto* C=P?GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,P->TargetID):nullptr;
  const double D=C?FVector::Dist(Position(ID),C->LastKnownPosition):-1;
  E.ShotDistances.Add(D);if(E.FirstShot<0){E.FirstShot=GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed;E.FirstShotDistance=D;}
 }
 if(Phase3 && Phase3->bEnabled) ++Phase3->Shots;
 const float SuppressedScale=SuppressionSpreadScale(ID,Now);
 if(SuppressedScale>1.25f)SightExperiment.Cover(TEXT("suppressed_shots"));
 // The player's own shot is tighter while he aims or looks through the scope (never the AI's shots).
 /* tighter for the player at all times (PlayerSpreadScale), more so while aiming */ const float AimScale=bPlayerShot&&Actor(ID)?Actor(ID)->PlayerSpreadScale():1.f;
 // Blind fire: the shooter cannot see along the barrel, so neither stance nor the player's aim tightens it.
 if(bBlind){BlindFireUntil.Add(ID,Now+BattleBlindFire::ClipHoldSeconds);SightExperiment.Cover(bPlayerShot?TEXT("blind_shots_player"):TEXT("blind_shots_ai"));}
 Direction=BattleTactics::ShotDirection(*U,Direction,Now,bBlind?SuppressedScale*BattleBlindFire::SpreadScale():SuppressedScale*BattleStance::SpreadScale(StanceOf(ID))*AimScale);
 auto Q=Sight(ID).Query; FHitResult Hit; const FVector End=Start+Direction.GetSafeNormal()*BattleRanges::Reach(U->Weapon);
 // Concealment (hedges) hides soldiers from sight but does not stop bullets.
 if(!bConcealmentBuilt)
 {
  bConcealmentBuilt=true;
  for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("BattleConcealment")))ConcealmentActors.Add(*It);
 }
 if(ConcealmentActors.Num())Q.AddIgnoredActors(ConcealmentActors);
 GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q);
 // Stances: a bullet above a crouched or prone soldier passes over him and flies on.
 for(int32 Pass=0;Pass<4&&Hit.bBlockingHit;++Pass)
 {
  const int32 Over=HitIDs.FindRef(Hit.GetComponent());const uint8 OverStance=Over>0?StanceOf(Over):0;
  if(OverStance==BattleStance::Stand||Hit.ImpactPoint.Z<=Position(Over).Z+(Over>0?TargetTopOffset(Over):BattleStance::TopOffset(OverStance)))break;
  SightExperiment.Cover(TEXT("shot_passed_over"));Q.AddIgnoredComponent(Hit.GetComponent());
  Hit=FHitResult();GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q);
 }
 SuppressNear(U->Team,Start,Hit.bBlockingHit?FVector(Hit.ImpactPoint):End,Now,U->Weapon==EBattleWeapon::SupportGun?BattleSuppression::Tuning().SupportScale:1.f);
 int32 Victim=HitIDs.FindRef(Hit.GetComponent());
 // Blind fire around an edge: the part of a soldier that shows past it is hit when the bullet's line passes through it
 // before reaching what the trace hit (BattleBlindFire::SideExposedRadius); it is not a collision body.
 bool bShownPartHit=false;
 if(BlindFireSide.Num())
 {
  const FVector To=Hit.bBlockingHit?FVector(Hit.ImpactPoint):End;double Nearest=FVector::Dist(Start,To);
  for(const auto& Side:BlindFireSide)
  {
   FVector Shown;if(Side.Key==ID || !SideExposure(Side.Key,Now,Shown))continue;
   const FVector Closest=FMath::ClosestPointOnSegment(Shown,Start,To);
   if(FVector::DistSquared(Closest,Shown)>FMath::Square(BattleBlindFire::SideExposedRadius))continue;
   if(const double Along=FVector::Dist(Start,Closest);Along<Nearest){Nearest=Along;Victim=Side.Key;bShownPartHit=true;}
  }
  if(bShownPartHit)SightExperiment.Cover(TEXT("blind_side_part_hit"));
 }
 // Display only (BattleBodyAnim): the hit and death clips face the side the shot came from.
 if(Victim>0)LastShotDirs.Add(Victim,Direction.GetSafeNormal2D());
 LastFireTimes.Add(ID,Now);
 if(!Actor(ID))FollowerAims.Add(ID,Direction.GetSafeNormal());
 if(auto* Shooter=Actor(ID))Shooter->NoteVisualShot(Now);
 // Per-shot damage by the shooter's squad type (heavy infantry may hit harder; rifle squads 1x).
 // An anti-tank round hits for ATDamage (and ATBaseDamage on an enemy home); its explosion follows below.
 const bool bAT=U->Weapon==EBattleWeapon::AntiTank;
 const float ShotDmg=bAT?T->ATDamage:[&]{const auto* SS=Store->FindSquad(U->SquadID);return T->ShotDamage*BattleSquadType::DamageScale(SS?SS->SquadType:0);}();
 if(bArea && !bShownPartHit)GetWorld()->GetSubsystem<UBattleMatch>()->DamageBase(Hit.GetActor(),U->Team,bAT?T->ATBaseDamage:ShotDmg);
 if(SightExperiment.Started && Victim>0) ++SightExperiment.Hits;
 const auto* BeforeVictim=Store->Find(Victim);
 const float BeforeHP=BeforeVictim?BeforeVictim->HP:0;
 const int32 VictimSquad=BeforeVictim?BeforeVictim->SquadID:INDEX_NONE;
 // Evaluation only (never AI input): leader exposure before the hit is applied.
 const auto* VictimSquadState=VictimSquad!=INDEX_NONE?Store->FindSquad(VictimSquad):nullptr;
 const bool VictimLeader=VictimSquadState && VictimSquadState->LeaderUnitID==Victim;
 const int32 VictimTeam=BeforeVictim?BeforeVictim->Team:0;
 const bool VictimCover=BeforeVictim && BeforeVictim->CoverPhase!=0;
 // Evaluation only: victim's cover state before the hit (0 none, 1 entering, 2 hidden, 3 moving to peek, 4 peek; 9 approaching cover).
 static const TCHAR* const HitKeys[]={TEXT("victim_hit_no_cover"),TEXT("victim_hit_cover_entering"),TEXT("victim_hit_cover_hidden"),TEXT("victim_hit_cover_to_peek"),TEXT("victim_hit_cover_peek"),TEXT("victim_hit_approaching_cover")};
 static const TCHAR* const DeathKeys[]={TEXT("victim_death_no_cover"),TEXT("victim_death_cover_entering"),TEXT("victim_death_cover_hidden"),TEXT("victim_death_cover_to_peek"),TEXT("victim_death_cover_peek"),TEXT("victim_death_approaching_cover")};
 const int32 VictimCoverState=!BeforeVictim?0:BeforeVictim->CoverPhase>0?FMath::Min<int32>(BeforeVictim->CoverPhase,4):CoverApproaches.Contains(Victim)?5:0;
 const EBattleAction VictimAction=BeforeVictim?BeforeVictim->Action:EBattleAction::Idle;
 int32 LeaderRank=-1,SquadLiving=0;
 if(SightExperiment.Started && VictimLeader)
 {
  LeaderRank=0;const double LeaderDistance=FVector::DistSquared2D(Position(Victim),Start);
  for(int32 Member:VictimSquadState->Members) if(const auto* M=Store->Find(Member); M && M->IsAlive())
  {++SquadLiving;if(Member!=Victim && FVector::DistSquared2D(Position(Member),Start)<LeaderDistance)++LeaderRank;}
 }
 // Armour: a vehicle takes only anti-tank hits (rifles and machine guns do nothing to it).
 const bool Damaged=Victim>0 && ArmorScale(Victim,U->Weapon)>0.f && Store->ApplyDamageFrom(Victim,U->Team,ShotDmg*ArmorScale(Victim,U->Weapon),Start,ID,EBattleKillKind::Shot);
 if(SightExperiment.Started && Damaged && BeforeVictim)
 {
  auto& E=SightExperiment;const bool Killed=BeforeHP>0 && !Store->Find(Victim)->IsAlive();
  // Squad type activity: damage and kills by the shooter's squad type, damage taken and deaths by the victim's.
  { const float Dealt=FMath::Min(BeforeHP,ShotDmg*ArmorScale(Victim,U->Weapon)); const auto* ShooterSquad=Store->FindSquad(U->SquadID);
    if(ShooterSquad && ShooterSquad->SquadType<BattleSquadType::Count && U->Team>=0 && U->Team<2){E.TypeDamage[U->Team][ShooterSquad->SquadType]+=Dealt;E.TypeKills[U->Team][ShooterSquad->SquadType]+=Killed;}
    if(VictimSquadState && VictimSquadState->SquadType<BattleSquadType::Count && VictimTeam>=0 && VictimTeam<2){E.TypeTaken[VictimTeam][VictimSquadState->SquadType]+=Dealt;E.TypeDeaths[VictimTeam][VictimSquadState->SquadType]+=Killed;} }
  (VictimLeader?E.LeaderHits:E.FollowerHits)[VictimTeam]++;
  E.Cover(HitKeys[VictimCoverState]);if(Killed)E.Cover(DeathKeys[VictimCoverState]);
  // Evaluation only: what a soldier hit outside cover was doing (walking, shooting, other), and for a sector squad whether
  // he was still on the way or already inside the sector (where moving hits come from; 11-20).
  if(VictimCoverState==0 && !IsVehicle(Victim))
  {
   const bool bWalking=VictimAction==EBattleAction::Moving || VictimAction==EBattleAction::Flanking || VictimAction==EBattleAction::Returning || VictimAction==EBattleAction::Regrouping || VictimAction==EBattleAction::TakingCover;
   E.Cover(bWalking?TEXT("victim_open_walking"):VictimAction==EBattleAction::Engaging?TEXT("victim_open_engaging"):TEXT("victim_open_other"));
   if(!bWalking)
   {
    const auto* VP=Plans.Find(VictimSquad);const int32 VT=VP?VP->TargetID:INDEX_NONE;
    const auto* VC=VT!=INDEX_NONE?GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(Victim,VT):nullptr;
    E.Cover(VT==INDEX_NONE?TEXT("victim_still_no_target"):!VC?TEXT("victim_still_target_unknown"):!VC->bVisible?TEXT("victim_still_target_not_seen"):
     FVector::DistSquared2D(Position(Victim),VC->LastKnownPosition)>FMath::Square(GetDefault<UBattleTuning>()->EffectiveWeaponRange())?TEXT("victim_still_target_seen_far"):TEXT("victim_still_target_seen_near"));
    {
     const auto* Perc=GetWorld()->GetSubsystem<UBattlePerception>();
     const auto* SC=Perc->FindContact(Victim,ID);
     if(SC && SC->bVisible)E.Cover(TEXT("victim_shooter_seen"));
     else
     {
      E.Cover(!SC?TEXT("victim_shooter_unknown"):SC->bShared?TEXT("victim_shooter_known_shared"):TEXT("victim_shooter_known_memory"));
      const double Dist=FVector::Dist2D(Position(Victim),Start);
      E.Cover(Dist<5000.?TEXT("victim_shooter_unseen_lt50m"):Dist<10000.?TEXT("victim_shooter_unseen_lt100m"):Dist<15000.?TEXT("victim_shooter_unseen_lt150m"):TEXT("victim_shooter_unseen_ge150m"));
      if(Dist>BeforeVictim->Perception.SightRange)E.Cover(TEXT("victim_shooter_unseen_beyond_sight"));
      const uint8 St=StanceOf(ID);
      E.Cover(St==BattleStance::Prone?TEXT("victim_shooter_unseen_prone"):St==BattleStance::Crouch?TEXT("victim_shooter_unseen_crouch"):TEXT("victim_shooter_unseen_stand"));
      if(U->CoverPhase)E.Cover(TEXT("victim_shooter_unseen_in_cover"));
      const auto* VS=Perc->FindContact(ID,Victim);
      E.Cover(VS && VS->bVisible?TEXT("victim_shooter_unseen_sees_victim"):TEXT("victim_shooter_unseen_blind_fire"));
     }
    }
    if(VC && VC->bVisible && FVector::DistSquared2D(Position(Victim),VC->LastKnownPosition)<=FMath::Square(GetDefault<UBattleTuning>()->EffectiveWeaponRange()))
    {
     static const TCHAR* GateKeys[]={TEXT("victim_near_gate_not_visible"),TEXT("victim_near_gate_not_stopped"),TEXT("victim_near_gate_move_fire"),TEXT("victim_near_gate_out_of_range"),
      TEXT("victim_near_gate_budget"),TEXT("victim_near_gate_retry"),TEXT("victim_near_gate_mortar"),TEXT("victim_near_gate_probing"),TEXT("victim_near_gate_standing_up")};
     static const TCHAR* RetryKeys[]={TEXT("victim_near_retry_no_slot"),TEXT("victim_near_retry_slot_reserved"),TEXT("victim_near_retry_path_blocked"),
      TEXT("victim_near_retry_too_exposed"),TEXT("victim_near_retry_no_peek"),TEXT("victim_near_retry_reserve_failed")};
     const uint8* G=CoverGate.Find(Victim);E.Cover(G&&*G<9?GateKeys[*G]:TEXT("victim_near_gate_none"));
     if(G&&(*G==5||*G==7)){const int8* St=LastCoverStage.Find(Victim);E.Cover(!St?TEXT("victim_near_retry_never"):*St<0?TEXT("victim_near_retry_after_success"):RetryKeys[FMath::Clamp<int32>(*St,0,5)]);}
    }
   }
   if(VictimSquadState && VictimSquadState->bSectorMission)
   {
    const bool bInside=BattleSectors::Cell(Position(Victim))==BattleSectors::Cell(VictimSquadState->Destination);
    E.Cover(bInside?(bWalking?TEXT("victim_open_sector_inside_walking"):TEXT("victim_open_sector_inside_still")):(bWalking?TEXT("victim_open_sector_enroute_walking"):TEXT("victim_open_sector_enroute_still")));
   }
   else E.Cover(TEXT("victim_open_no_sector_mission"));
  }
  // Evaluation only: how much of a standing body at the victim's place the shooter could see, and hits at a settled firing position.
  if(!IsVehicle(Victim))
  {
   const float Seen=BattleTactics::ExposureAt(GetWorld(),Position(Victim),Forward(Victim),Start).Fraction();
   E.Cover(Seen>.81f?TEXT("victim_hit_body_open"):Seen>.41f?TEXT("victim_hit_body_part_covered"):TEXT("victim_hit_body_mostly_covered"));
   if(const FVector* Settled=FirePositions.Find(Victim);Settled && FVector::DistSquared2D(*Settled,Position(Victim))<FMath::Square(150.))E.Cover(TEXT("victim_hit_at_fire_position"));
  }
  if(Killed && !VictimLeader) E.FollowerCombatDeaths[VictimTeam]++;
  if(Killed && VictimLeader)
  {
   E.LeaderCombatDeaths[VictimTeam]++;E.LeaderLossFollowers[VictimTeam]+=FMath::Max(0,SquadLiving-1);
   E.LeaderDeathRanks.Add(LeaderRank);E.LeaderDeathsInCover+=VictimCover;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_DEATH t=%.2f squad=%d team=%d rank=%d living=%d cover=%d"),GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,VictimSquad,VictimTeam,LeaderRank,SquadLiving,VictimCover);
  }
 }
 if(SightExperiment.Started)
 {
  // Evaluation only: attribute fire to the closest objective within 100m of the shooter.
  auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();int32 Nearest=INDEX_NONE;double Best=10000;
  for(int32 I=0;I<Match->Objectives.Num();++I){const double D=FVector::Dist2D(Start,Match->Objectives[I].Position);if(D<Best){Best=D;Nearest=I;}}
  if(Nearest!=INDEX_NONE){++SightExperiment.ObjectiveShots[Nearest];SightExperiment.ObjectiveDamage[Nearest]+=Damaged?FMath::Min(BeforeHP,ShotDmg):0;if(SightExperiment.ObjectiveFirstShot[Nearest]<0)SightExperiment.ObjectiveFirstShot[Nearest]=Match->Elapsed;}
 }
 if(BayStage==1 && Damaged){BayDamageGiven[U->Team]+=FMath::Min(BeforeHP,ShotDmg);if(VictimSquad==2 && BayArrival<0)BayMoveDirectDamage+=FMath::Min(BeforeHP,ShotDmg);}
 if(SightExperiment.Started && SightExperiment.FlankTrial)
 {
  auto& E=SightExperiment;++E.TeamShots[U->Team];
  const double Damage=Damaged?FMath::Min(BeforeHP,ShotDmg):0.f;
  const bool Kill=Damaged && BeforeHP>0 && !Store->Find(Victim)->IsAlive();
  E.TeamDamage[U->Team]+=Damage;E.TeamKills[U->Team]+=Kill;
  double Angle=-1;
  if(Damaged){const auto View=Sight(Victim);Angle=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(View.Forward.GetSafeNormal2D(),(Position(ID)-Position(Victim)).GetSafeNormal2D()),-1.,1.)));}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_SHOT t=%.3f squad=%d team=%d victim_squad=%d damage=%.1f kill=%d angle=%.1f"),GetWorld()->GetSubsystem<UBattleMatch>()->Elapsed,U->SquadID,U->Team,VictimSquad,Damage,Kill,Angle);
 }
 // Anti-tank round: the explosion at the impact hurts enemies around it (not the soldier already hit; never own soldiers).
 if(bAT)Explode(ID,Hit.bBlockingHit?FVector(Hit.ImpactPoint):End,Victim);
 if(Damaged)
  if(bPlayerShot) if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->HitMarkerUntil=Now+.18;
 if(U->Team==0 || (Actor(ID) && Actor(ID)->bRevealed))
 {
  const FVector From=TracerStart(ID,Start,Direction);
  DrawDebugLine(GetWorld(),From,Hit.bBlockingHit?Hit.ImpactPoint:End,U->Team?FColor::Orange:FColor::Cyan,false,.09f,0,2);
  // Not the player's own flash through a scope: at x4-x8 it would cover the whole view.
  if(!(bPlayerShot&&Actor(ID)&&Actor(ID)->ScopeView()))BattleMuzzleFlash::Show(GetWorld(),From,Direction,U->Weapon==EBattleWeapon::Carbine?1.f:1.4f);
 }
 return true;
}
FVector UBattleLeaderRuntime::TracerStart(int32 ID,const FVector& Start,const FVector& Direction) const
{
 // Display only (the shot itself is still traced from the eye): the tracer leaves the gun.
 // Tank: the end of the barrel (BattleVehicle.cpp: 340 cm ahead of the turret centre, in the turret's 1.7 scale).
 if(const auto* Turret=Turrets.Find(ID);Turret&&Turret->IsValid())return (*Turret)->GetComponentTransform().TransformPosition(FVector(200,0,0));
 const ABattleUnit* A=Actor(ID);const auto* B=Bodies.Find(ID);
 const UStaticMeshComponent* Model=A?A->Rifle.Get():B&&!(VAT&&VAT->IsFar(ID))?B->Weapon.Get():nullptr;
 return ABattleUnit::TracerStart(Model,Start,Direction);
}
void UBattleLeaderRuntime::RefreshDisplay()
{
 const auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 const bool DirectBodies=PC && PC->ShowsUnspottedBodies();
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>(); TSet<int32> Visible;
 for(const auto& B:Bodies) if(Store->Find(B.Key)->Team==0 && Store->Find(B.Key)->IsAlive())
  // A scout's long-range sightings are not shown until reported (the player's own camera still shows what it sees).
  if(const auto* M=GetWorld()->GetSubsystem<UBattleMatch>();M)
  if(const auto* Contacts=Sense->GetContacts(B.Key)) for(const auto& C:*Contacts) if(C.Value.bVisible) if(!M->bArea||!M->IsScoutLongContact(B.Key,C.Value)) Visible.Add(C.Key);
 for(auto& B:Bodies)
 {
  const bool Show=Store->Find(B.Key)->Team==0 || Visible.Contains(B.Key);
  const bool BodyShow=Show || DirectBodies;
  // bRevealed is sensor knowledge the AI and HUD read: keep updating it for a hidden corpse (SetRevealed keeps it hidden).
  if(B.Value.Actor.IsValid()) B.Value.Actor->SetRevealed(Show);
  else if(HiddenCorpses.Contains(B.Key)) continue; // removed follower corpse: its mesh stays hidden
  else { if(VAT)VAT->SetVisible(B.Key,BodyShow); if(!VAT || !VAT->IsFar(B.Key)){BattleFollowerBones::SetVisible(B.Value.Mesh.Get(),BodyShow,bReduceHiddenBones); if(bReduceAnimTasks) B.Value.AnimTasks.Apply(B.Value.Mesh.Get(),true); B.Value.Weapon->SetVisibility(BodyShow && Store->Find(B.Key)->IsAlive());} }
 }
}
void UBattleLeaderRuntime::Step(float Dt)
{
 if(!bEnabled) return;
 // Named CPU trace scopes (Unreal Insights, -trace=cpu): free when no trace is recording.
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleRuntimeStep);
 if(SightExperiment.Enabled) { TRACE_CPUPROFILER_EVENT_SCOPE(BattleSightExperiment); StepSightExperiment(Dt); }
 // After the previous frame's facing and poses; also on the Phase3 path (it returns before the body timers).
 if(BattleBodyAnim::Enabled())AimFollowerRifles(GetWorld()->GetTimeSeconds());
 if(Phase3 && Phase3->bEnabled) { Phase3->Step(Dt); return; }
 StepBodyTimers();
 StepCorpses();
 if(bMovementTrial && MoveTrialStage==1 && !Mode->bCombatPaused) MoveTrialFrameMS.Add(Dt*1000);
 const double Now=GetWorld()->GetTimeSeconds(); auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 // Build the cover slots while the match waits to start (they were built lazily by the first decision: ~100 ms in one frame).
 if(bWideField && (Mode->bCombatPaused || Now<Mode->CombatStartsAt)) GetWorld()->GetSubsystem<UBattleCover>()->PrebuildSlots();
 if(bWideField && bStableMovement && Mode->bCombatPaused)
  for(auto& Body:Bodies) {Body.Value.LastProgressTime=Now;Body.Value.StalledSince=0;}
 // P pauses AI decisions, not the directly controlled soldier's reload (same as the wide Actor mode).
 if(bWideField) Store->AdvanceReloads(Now);
 if(bWideField && !bMovementTrial && Now>=Mode->CombatStartsAt)
 {
  { TRACE_CPUPROFILER_EVENT_SCOPE(BattleMatchUpdate); GetWorld()->GetSubsystem<UBattleMatch>()->Update(Dt); }
  { TRACE_CPUPROFILER_EVENT_SCOPE(BattleSquadTacticsStep); GetWorld()->GetSubsystem<UBattleSquadTactics>()->Step(Dt); }
 }
 if(!Mode->bCombatPaused && !Mode->bBattleEnded && Now>=Mode->CombatStartsAt)
 {
  if(!bWideField) Store->AdvanceReloads(Now);
  Store->AdvanceLaunchers(Dt);
  Store->AdvanceShields(Dt);
  if(Store->HasExoskeletons()) StepExoLaunchers(Now);
  RefreshSightTeamMasks();
  if(!bMovementTrial) { TRACE_CPUPROFILER_EVENT_SCOPE(BattlePerceptionUpdate); GetWorld()->GetSubsystem<UBattlePerception>()->Update(Mode->Units,Now,-1,0); }
  {
   TRACE_CPUPROFILER_EVENT_SCOPE(BattleThink);
   // Trial -BattleThinkCap=N (0 = no cap, default): at most N squad decisions per step. Squads over the cap stay due and
   // think on the next steps in the same order. Normally 2-3 squads are due per step; the cap only binds when many fall due
   // together (match start: all 50 in one 139 ms frame, 2026-11-18 trace).
   static const int32 ThinkCap=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleThinkCap="),V);return FMath::Max(0,V);}();
   int32 Thought=0;
   for(auto& P:Plans) if(Now>=P.Value.NextThink || (Store->FindSquad(P.Key) && P.Value.OrderRevision!=Store->FindSquad(P.Key)->OrderRevision))
   {
    if(ThinkCap>0 && Thought>=ThinkCap) break;
    Think(P.Key,Now);++Thought;
   }
  }
  if(bWideField)GuardFalls(Now);
  // Drop old cell keys, but retain bucket storage and append in the original Bodies order.
  for(int32 UsedBucket:AvoidanceUsed)AvoidanceBuckets[UsedBucket].Reset();
  AvoidanceUsed.Reset();AvoidanceGrid.Reset();
  for(const auto& B:Bodies) if(Store->Find(B.Key)->IsAlive())
  {
   const FVector P=Position(B.Key);const FIntPoint BodyCell(FMath::FloorToInt(P.X/150.),FMath::FloorToInt(P.Y/150.));
   int32* FoundBucket=AvoidanceGrid.Find(BodyCell);
   if(!FoundBucket)
   {
    const int32 NewBucket=AvoidanceUsed.Num();
    if(NewBucket==AvoidanceBuckets.Num())AvoidanceBuckets.AddDefaulted();
    AvoidanceUsed.Add(NewBucket);FoundBucket=&AvoidanceGrid.Add(BodyCell,NewBucket);
   }
   AvoidanceBuckets[*FoundBucket].Add(B.Key);
  }
  // Death may invalidate a plan during firing; iterate stable UnitIDs, never a plan iterator.
  { TRACE_CPUPROFILER_EVENT_SCOPE(BattleFollow); for(const auto& B:Bodies) Follow(B.Key,FMath::Min(Dt,.05f),Now); }
  // Indirect-fire shells in flight land after their flight time (paused with the combat).
  if(bArea && ArtilleryShells.Num()) StepArtilleryShells(Dt);
 }
 int32 Living[2]={0,0}; for(const auto& U:Store->GetUnits()) if(U.Value.IsAlive()) ++Living[U.Value.Team];
 if(!bArea && !Mode->bBattleEnded && (!Living[0] || !Living[1]))
 {
  if(bWideField) GetWorld()->GetSubsystem<UBattleMatch>()->Finish(Living[0]?0:1,Living[0]?TEXT("VICTORY / ENEMY ELIMINATED"):TEXT("DEFEAT / FORCE ELIMINATED"));
  else { Mode->bBattleEnded=true; Mode->Result=Living[0]?TEXT("VICTORY"):TEXT("DEFEAT"); }
 }
 StepStateTrace(); // stage 2 diagnosis, -BattleStateTrace only: the state after this frame's logic, before the display
 { TRACE_CPUPROFILER_EVENT_SCOPE(BattleRefreshDisplay); RefreshDisplay(); }
}

float UBattleLeaderRuntime::TargetRange(int32 ID) const
{
 const float Base=GetDefault<UBattleTuning>()->EffectiveWeaponRange();
 if(!UnderFire(ID) && !(UnderFireCoverEnabled() && HitWithin(ID,GetWorld()->GetTimeSeconds(),15.)))return Base;
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 return U?FMath::Max(Base,BattleRanges::Reach(U->Weapon)):Base;
}

float UBattleLeaderRuntime::ScoutStillSight(int32 ID) const
{
 // Called for every sight check: cheap tests first (scout squad, at a lookout), the speed last.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive()) return 0.f;
 const auto* S=Store->FindSquad(U->SquadID);
 float Range=S?BattleSquadType::StillSightRange(S->SquadType):0.f;
 if(AtLookout(ID)){const auto* T=GetDefault<UBattleTuning>();Range=FMath::Max(Range,FMath::Clamp(BattleRanges::Get().bActive?T->LookoutSightRangeProfile:T->LookoutSightRange,0.f,60000.f));}
 if(Range<=0.f) return 0.f;
 const auto* B=Bodies.Find(ID); if(!B) return 0.f;
 const float Speed=B->Actor.IsValid()?B->Actor->GetVelocity().Size2D():B->Velocity.Size2D();
 return Speed<=GetDefault<UBattleTuning>()->ScoutStillSpeed?Range:0.f;
}
bool UBattleLeaderRuntime::AtLookout(int32 ID) const
{
 if(!bLookoutCache){LookoutCache=BattleFieldLayout::Lookouts(GetWorld());bLookoutCache=true;}
 if(LookoutCache.IsEmpty()||!Bodies.Contains(ID)) return false;
 const FVector P=Position(ID);const double R2=FMath::Square(FMath::Max(0.f,GetDefault<UBattleTuning>()->LookoutRadius));
 for(const FVector& L:LookoutCache) if(FVector::DistSquared2D(P,L)<=R2) return true;
 return false;
}
bool UBattleLeaderRuntime::HasLongSight(int32 ID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
 return (S&&S->SquadType==BattleSquadType::Scout)||AtLookout(ID);
}

bool UBattleLeaderRuntime::SectorContactEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorContact="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::SectorMoveEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorMove="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::ArrivalWalkEnabled()
{
 static const bool bOn=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleArrivalWalk="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::SectorMoveFireEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorMoveFire="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::SquadCoverCheckEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSquadCoverCheck="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::BoundingEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleBounding="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::NoCoverFallbackEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleNoCoverFallback="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::HitFaceEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleHitFace="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::HitRevealEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleHitReveal="),V);return V!=0;}();
 return bOn;
}
void UBattleLeaderRuntime::RevealShooter(int32 ID,int32 Shooter,const FVector& From)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* V=Store->Find(ID);const auto* S=Store->Find(Shooter);
 if(!V||!S||!V->IsAlive()||!S->IsAlive()||V->Team==S->Team)return;
 auto* Perc=GetWorld()->GetSubsystem<UBattlePerception>();const double Now=GetWorld()->GetTimeSeconds();
 if(const auto* C=Perc->FindContact(ID,Shooter);C && C->bVisible){SightExperiment.Cover(TEXT("hit_reveal_already_seen"));return;}
 if(CanSee(ID,Shooter)){Perc->RevealShooter(ID,Shooter,Now,V->Perception.DetectionSeconds);SightExperiment.Cover(TEXT("hit_reveal_los"));return;}
 // Deterministic error (no random stream consumed): direction from the pair and the frame, length 10% of the distance.
 const FVector Me=Position(ID);const double Dist=FVector::Dist2D(Me,From);
 const uint32 H=HashCombine(HashCombine(GetTypeHash(ID),GetTypeHash(Shooter)),GetTypeHash(uint32(GFrameCounter)));
 const double Angle=double(H%3600u)*(PI/1800.);
 const FVector Guess=From+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0.)*(.1*Dist);
 Perc->AddHint(ID,Shooter,Guess,Now,.4f);SightExperiment.Cover(TEXT("hit_reveal_hint"));
}
const TCHAR* UBattleLeaderRuntime::SightBlockReason(int32 Obs,int32 Tgt,double& EyeZ,double& EndZ) const
{
 const auto C=Sight(Obs);const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* T=Store->Find(Tgt);EyeZ=C.Eye.Z;EndZ=EyeZ;
 if(!C.bAlive||!T||!T->IsAlive()||!Bodies.Contains(Tgt))return TEXT("gone");
 const FVector P=Position(Tgt),D=(P-C.Position).GetSafeNormal2D();
 if(FVector::DistSquared(P,C.Position)>(IsVehicle(Tgt)?FMath::Max(C.RangeSquared,FMath::Square(VehicleSpotRange())):C.RangeSquared))return TEXT("range");
 if(!C.bOmnidirectional && !D.IsNearlyZero() && FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)return TEXT("fov");
 FVector End;if(!PrepareSightTrace(C,Tgt,End))return TEXT("other_check");EndZ=End.Z;
 FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,C.Eye,End,ECC_Visibility,C.Query);
 if(SightHitVisible(Hit,Tgt))return TEXT("clear");
 if(const int32* ID=HitIDs.Find(Hit.GetComponent()))
 {
  if(IsVehicle(*ID))return TEXT("vehicle");
  const auto* B=Store->Find(*ID);const auto* O=Store->Find(Obs);
  return B&&O&&B->Team==O->Team?TEXT("own_side_body"):TEXT("target_side_body");
 }
 const UPrimitiveComponent* HC=Hit.GetComponent();const AActor* A=Hit.GetActor();
 if(!HC)return TEXT("other");
 if(A&&A->ActorHasTag(TEXT("BattleConcealment")))return TEXT("concealment");
 if(HC->GetClass()->GetName().Contains(TEXT("Landscape"))||SightHitActorGroup(A).Contains(TEXT("Floor")))return TEXT("floor");
 if(A&&(Cast<ABattleUnit>(A)||A==DisplayHost.Get()))return TEXT("unit_part");
 if(HC->IsA<UInstancedStaticMeshComponent>())return TEXT("instanced_mesh");
 if(HC->IsA<UStaticMeshComponent>())return TEXT("static_mesh");
 return TEXT("other");
}
void UBattleLeaderRuntime::DiagnoseHitSight(int32 ID,int32 Shooter)
{
 auto& E=SightExperiment;const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* V=Store->Find(ID);const auto* S=Store->Find(Shooter);
 if(!V||!S||!V->IsAlive()||!S->IsAlive()||V->Team==S->Team)return;
 const auto* Perc=GetWorld()->GetSubsystem<UBattlePerception>();
 if(const auto* K=Perc->FindContact(ID,Shooter);K && K->bVisible){E.Cover(TEXT("hitlos_victim_sees"));return;}
 double VEye,VEnd,SEye,SEnd;
 const TCHAR* VS=SightBlockReason(ID,Shooter,VEye,VEnd);const TCHAR* SV=SightBlockReason(Shooter,ID,SEye,SEnd);
 E.Cover(*FString::Printf(TEXT("hitlos_v2s_%s"),VS));E.Cover(*FString::Printf(TEXT("hitlos_s2v_%s"),SV));
 const bool bOneWay=FCString::Strcmp(VS,TEXT("clear"))!=0 && FCString::Strcmp(SV,TEXT("clear"))==0;
 if(!bOneWay)return;
 E.Cover(*FString::Printf(TEXT("hitlos_oneway_%s"),VS));
 auto StName=[](uint8 St){return St==BattleStance::Prone?TEXT("prone"):St==BattleStance::Crouch?TEXT("crouch"):TEXT("stand");};
 E.Cover(*FString::Printf(TEXT("hitlos_oneway_victim_%s"),StName(StanceOf(ID))));E.Cover(*FString::Printf(TEXT("hitlos_oneway_shooter_%s"),StName(StanceOf(Shooter))));
 const double Now=GetWorld()->GetTimeSeconds();FVector Dummy;
 if(IsHeadExposed(Shooter,Now)||SideExposure(Shooter,Now,Dummy))E.Cover(TEXT("hitlos_oneway_shooter_exposed"));
 // The trace that failed (victim eye -> shooter aim point) against the clear one (shooter eye -> victim aim point), in metres.
 if(FCString::Strcmp(VS,TEXT("range"))!=0 && FCString::Strcmp(VS,TEXT("fov"))!=0)
 {
  const double Rise=(VEnd-VEye)/100.;
  E.Cover(Rise<-1.?TEXT("hitlos_oneway_blocked_look_down"):Rise>1.?TEXT("hitlos_oneway_blocked_look_up"):TEXT("hitlos_oneway_blocked_level"));
  E.Cover(VEye+50.<SEye?TEXT("hitlos_oneway_blocked_victim_eye_lower"):VEye>SEye+50.?TEXT("hitlos_oneway_blocked_victim_eye_higher"):TEXT("hitlos_oneway_blocked_eye_same"));
  E.Cover(VEnd+30.<SEye?TEXT("hitlos_oneway_blocked_aim_below_shooter_eye"):TEXT("hitlos_oneway_blocked_aim_at_eye"));
 }
}
bool UBattleLeaderRuntime::CoverPriorityEnabled()
{
 static const bool bOn=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverPriority="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::UnderFireCoverEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleUnderFireCover="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::SectorCoverWalkEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorCoverWalk="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::CloseInEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCloseIn="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::PinnedFlankEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattlePinnedFlank="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::CoverDenseEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverDense="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::CoverIndoorEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverIndoor="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::CoverOutwardEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverOutward="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::ArrivalThreatEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArrivalThreat="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::CoverStrictEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverStrict="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::ShotDirectionEnabled()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleShotDirection="),V);return V!=0;}();
 return bOn;
}
bool UBattleLeaderRuntime::SquadShotFrom(int32 SID,double Now,FVector& Out) const
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);if(!S||ShotFrom.IsEmpty())return false;
 double Newest=-1.;for(int32 ID:S->Members)if(const auto* F=ShotFrom.Find(ID);F && Now-F->Value<=5. && F->Value>Newest){Newest=F->Value;Out=F->Key;}
 return Newest>=0.;
}
bool UBattleLeaderRuntime::SquadShotAt(const FBattleSquadState& S,double Now) const
{
 for(int32 ID:S.Members)if(const double* T=SectorHitUntil.Find(ID);T&&*T>Now)return true;
 return false;
}
void UBattleLeaderRuntime::NoteUnderFire(int32 ID)
{
 // A direct hit adds one impulse even when a shield absorbs all damage (all weapon damage paths).
 if(ExoSuppressionApplies(ID))AddSuppression(ID,BattleSuppression::Tuning().PerShot,GetWorld()->GetTimeSeconds());
 if(UBattleMatch::DangerRouteOn())if(const auto* V=GetWorld()->GetSubsystem<UBattleState>()->Find(ID))GetWorld()->GetSubsystem<UBattleMatch>()->NoteSectorDanger(V->Team,Position(ID));
 if(ShotDirectionEnabled() || UnderFireCoverEnabled() || CoverPriorityEnabled() || HitFaceEnabled() || (BattleTactics::VehicleHitCover()&&IsVehicle(ID))){FVector From;if(GetWorld()->GetSubsystem<UBattleState>()->PendingHitSource(From))ShotFrom.Add(ID,TPair<FVector,double>(From,GetWorld()->GetTimeSeconds()));}
 if(SectorContactEnabled() || SectorMoveFireEnabled() || !SectorFireBack.IsEmpty())SectorHitUntil.Add(ID,GetWorld()->GetTimeSeconds()+5.0);
 if(SightExperiment.Enabled)if(const int32 Shooter=GetWorld()->GetSubsystem<UBattleState>()->PendingHitShooter();Shooter!=INDEX_NONE)DiagnoseHitSight(ID,Shooter);
 if(HitRevealEnabled()){const auto* Store=GetWorld()->GetSubsystem<UBattleState>();FVector From;const int32 Shooter=Store->PendingHitShooter();
  if(Shooter!=INDEX_NONE && Store->PendingHitSource(From))RevealShooter(ID,Shooter,From);}
 if(!BattleTactics::ReturnFireEnabled())return;
 const int32 Trial=BattleTactics::AreaStrategy().TrialTeam;
 if(Trial>=0){const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!U||U->Team!=Trial)return;}
 UnderFireUntil.Add(ID,GetWorld()->GetTimeSeconds()+5.0);
}
