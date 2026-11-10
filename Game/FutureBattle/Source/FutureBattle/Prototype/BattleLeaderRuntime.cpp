#include "BattleLeaderRuntime.h"
#include "BattleFollowerBones.h"
#include "BattleVATDisplay.h"
#include "BattlePhase3.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "BattleMetrics.h"
#include "MassEntitySubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "FutureBattle.h"

FBattleSquadListLayout FBattleSquadListLayout::Make(FVector2D View)
{
 FBattleSquadListLayout L; L.Width=FMath::Min(300.f,float(View.X)-32.f);
 L.Origin=FVector2D(View.X-L.Width-16,104); L.Height=FMath::Max(0.f,FMath::Min(720.f,float(View.Y)-234.f));
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
 int32 Count=40; FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderUnits="),Count);
 if(Count<20 || Count>1000 || Count%20) return false;
 Mode=&InMode; bEnabled=true;
 int32 AnimTasks=0; FParse::Value(FCommandLine::Get(),TEXT("BattleReduceAnimTasks="),AnimTasks);
 if(AnimTasks!=0 && AnimTasks!=1) return false;
 bReduceAnimTasks=AnimTasks==1;
 UE_LOG(LogFutureBattle,Display,TEXT("ANIM_TASKS enabled=%d scope=hidden_update_only"),AnimTasks);
 int32 HiddenBones=0; FParse::Value(FCommandLine::Get(),TEXT("BattleReduceHiddenBones="),HiddenBones);
 if(HiddenBones!=0 && HiddenBones!=1) return false;
 bReduceHiddenBones=HiddenBones==1;
 UE_LOG(LogFutureBattle,Display,TEXT("HIDDEN_BONES enabled=%d scope=fog_hidden_followers"),HiddenBones);
 int32 ReuseSight=0; FParse::Value(FCommandLine::Get(),TEXT("BattleReuseSightContext="),ReuseSight);
 if(ReuseSight!=0 && ReuseSight!=1) return false;
 GetWorld()->GetSubsystem<UBattlePerception>()->bReuseSightContext=ReuseSight==1;
 int32 Grid=0; FParse::Value(FCommandLine::Get(),TEXT("BattlePerceptionGrid="),Grid);
 if(Grid!=0 && Grid!=1) return false;
 GetWorld()->GetSubsystem<UBattlePerception>()->bUseSpatialGrid=Grid==1;
 UE_LOG(LogFutureBattle,Display,TEXT("PERCEPTION_GRID enabled=%d far=0 scope=existing_candidate_grid"),Grid);
 UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_CONTEXT_REUSE enabled=%d scope=observer_scan"),ReuseSight);
 Phase3=GetWorld()->GetSubsystem<UBattlePhase3>();
 if(!Phase3->Configure(*this)) return false;
 int32 DisplayLOD=0; FParse::Value(FCommandLine::Get(),TEXT("BattleDisplayLOD="),DisplayLOD);
 if((DisplayLOD!=0 && DisplayLOD!=1) || (DisplayLOD==1 && !Phase3->bEnabled))return false;
 UE_LOG(LogFutureBattle,Display,TEXT("VAT_DISPLAY requested=%d default=0 trial_only=1"),DisplayLOD);
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
  const FVector Center=Phase3->bEnabled?Phase3->SpawnCenter(Team,Group):FVector(Team ? 1800:-1800, (Group-(Count/20-1)*.5)*800, 95);
  ABattleUnit* Leader=nullptr;
  for(int32 Slot=0;Slot<10;++Slot)
  {
   const int32 ID=Store->RegisterUnit(Team,Phase3->bStable?10000.f:0.f,WeaponForSlot(Slot)); IDs.Add(ID);
   const FVector P=Center+FVector((Slot/5)*-130*(Team?-1:1),(Slot%5-2)*110,0);
   FBattleLeaderBody B;
   if(Slot==0)
   {
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Leader=GetWorld()->SpawnActor<ABattleUnit>(P,FRotator(0,Team?180:0,0),Params);
    if(!Leader) return false;
    Leader->InitializeUnit(ID); Mode->Units.Add(Leader); B.Actor=Leader;
    Leader->GetCharacterMovement()->bOrientRotationToMovement=false;
    HitIDs.Add(Leader->GetCapsuleComponent(),ID);
   }
   else
   {
    B.Entity=Manager->CreateEntity(Archetype); auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);
    F.UnitID=ID; F.Position=P; F.Forward=FVector(Team?-1:1,0,0);
    auto* Capsule=NewObject<UCapsuleComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Capsule);
    Capsule->InitCapsuleSize(36,90); Capsule->SetCollisionObjectType(ECC_Pawn);
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Capsule->SetCollisionResponseToAllChannels(ECR_Block);
    Capsule->SetCanEverAffectNavigation(false); Capsule->SetupAttachment(Root); Capsule->RegisterComponent(); Capsule->SetWorldLocation(P);
    B.Capsule=Capsule; HitIDs.Add(Capsule,ID);
    auto* Mesh=DisplayLOD==1?NewObject<USkeletalMeshComponent>(DisplayHost,UBattleVATSkeletalMeshComponent::StaticClass()):NewObject<USkeletalMeshComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Mesh);
    Mesh->SetSkeletalMesh(MeshAsset); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetupAttachment(Capsule);
    Mesh->SetRelativeLocation(FVector(0,0,-90)); Mesh->SetRelativeRotation(FRotator(0,-90,0)); Mesh->RegisterComponent();
    Mesh->PlayAnimation(Idle,true); B.Mesh=Mesh;
    auto* Weapon=NewObject<UStaticMeshComponent>(DisplayHost); DisplayHost->AddInstanceComponent(Weapon);
    Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision); Weapon->RegisterComponent(); Leader->MountWeapon(Weapon,Mesh,Store->Find(ID)->Weapon,false); B.Weapon=Weapon;
   }
   Bodies.Add(ID,B);
  }
  const int32 SID=Store->CreateLeaderSquad(Team,IDs,IDs[0],Team==0 && Group!=NoControl);
  if(SID==INDEX_NONE) return false;
  FBattleLeaderPlan Plan; Plan.Center=Center; Plan.NextThink=GetWorld()->GetTimeSeconds()+.05*(SID%10); Plans.Add(SID,Plan);
  IssueOrder(SID,Phase3->bEnabled?EBattleOrder::AttackMove:Team?EBattleOrder::AttackMove:EBattleOrder::Hold,Phase3->bEnabled?Phase3->Goal(Team,Group):FVector(Team?600:Center.X,Center.Y,0));
 }
 if(DisplayLOD==1 && !VAT->Initialize(this))return false;
 Mode->CombatStartsAt=GetWorld()->GetTimeSeconds()+3.;
 if(Phase3->bEnabled) Phase3->Begin();
 if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->BindLeaderDeaths();
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_LEADER_READY total=%d actors=%d mass=%d"),Count,Mode->Units.Num(),Count-Mode->Units.Num());
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleLeaderSmoke")))
 { Mode->bCombatPaused=true; FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSmoke,3.f,false); }
 if(FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderQA="),QAPrefix))
 { Mode->bCombatPaused=true; FTimerHandle H; GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunUIQA,5.f,false); }
 return true;
}
void UBattleLeaderRuntime::Deinitialize()
{
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
 return B->Actor.IsValid()? B->Actor->GetActorLocation():Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B->Entity).Position;
}
FVector UBattleLeaderRuntime::Forward(int32 ID) const
{
 const auto* B=Bodies.Find(ID); if(!B) return FVector::ForwardVector;
 return B->Actor.IsValid()? B->Actor->CaptureSightContext().Forward:Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B->Entity).Forward;
}
FBattleSightContext UBattleLeaderRuntime::Sight(int32 ID) const
{
 if(auto* A=Actor(ID)) return A->CaptureSightContext();
 FBattleSightContext C; const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 C.Position=Position(ID); C.Eye=C.Position+FVector(0,0,60); C.Forward=Forward(ID); C.bAlive=U && U->IsAlive();
 if(U) { C.RangeSquared=FMath::Square(U->Perception.SightRange); C.HalfAngleCos=FMath::Cos(FMath::DegreesToRadians(U->Perception.FieldOfView*.5)); C.bOmnidirectional=U->Perception.FieldOfView>=360; }
 C.Metrics=GetWorld()->GetSubsystem<UBattleMetrics>(); C.Query=FCollisionQueryParams(SCENE_QUERY_STAT(LeaderSight),false);
 if(const auto* B=Bodies.Find(ID)) C.Query.AddIgnoredComponent(B->Capsule.Get()); return C;
}
bool UBattleLeaderRuntime::CanSee(int32 ID,int32 Target) const
{
 const auto* T=GetWorld()->GetSubsystem<UBattleState>()->Find(Target); const auto C=Sight(ID);
 if(!C.bAlive || !T || !T->IsAlive() || !Bodies.Contains(Target)) return false;
 const FVector P=Position(Target), D=(P-C.Position).GetSafeNormal2D();
 if(FVector::DistSquared(P,C.Position)>C.RangeSquared || (!C.bOmnidirectional && !D.IsNearlyZero() && FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)) return false;
 FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,C.Eye,P+FVector(0,0,35),ECC_Visibility,C.Query);
 return !Hit.bBlockingHit || HitIDs.FindRef(Hit.GetComponent())==Target;
}
bool UBattleLeaderRuntime::CanSeeWithContext(const FBattleSightContext& C,int32 Target) const
{
 // Keep reference arithmetic, target reads, query and hit policy unchanged.
 // Only the observer Context construction is hoisted by the perception caller.
 const auto* T=GetWorld()->GetSubsystem<UBattleState>()->Find(Target);
 if(!C.bAlive || !T || !T->IsAlive() || !Bodies.Contains(Target)) return false;
 const FVector P=Position(Target), D=(P-C.Position).GetSafeNormal2D();
 if(FVector::DistSquared(P,C.Position)>C.RangeSquared || (!C.bOmnidirectional && !D.IsNearlyZero() && FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)) return false;
 FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,C.Eye,P+FVector(0,0,35),ECC_Visibility,C.Query);
 return !Hit.bBlockingHit || HitIDs.FindRef(Hit.GetComponent())==Target;
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
 if(Phase3 && Phase3->bEnabled) Phase3->OnDeath(ID);
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); if(!U) return;
 const auto* S=Store->FindSquad(U->SquadID);
 if(S && S->LeaderUnitID==ID) { Store->DisableLeaderSquad(S->SquadID); Plans.Remove(S->SquadID); }
 if(auto* B=Bodies.Find(ID))
 {
  B->Velocity=FVector::ZeroVector;
  if(B->Actor.IsValid()) B->Actor->ApplyDeath();
  else { B->Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); if(VAT)VAT->Freeze(ID); if(!VAT || !VAT->IsFar(ID)){BattleFollowerBones::PrepareFreeze(B->Mesh.Get(),bReduceHiddenBones); B->Mesh->SetComponentTickEnabled(false); B->Mesh->SetRelativeRotation(FRotator(0,-90,85)); B->Weapon->SetVisibility(false);} }
 }
 bSummaryDirty=true;
}
bool UBattleLeaderRuntime::IssueOrder(int32 SID,EBattleOrder Order,FVector Goal)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID);
 if(!Mode || Mode->bBattleEnded || !S || S->CommandStatus!=EBattleCommandStatus::Active || Goal.ContainsNaN()) return false;
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
 const bool OK=Store->SetSquadOrder(SID,Order,Goal,Goals); if(OK) { auto& P=Plans.FindOrAdd(SID); P.bReplan=true; P.NextThink=0; } return OK;
}
void UBattleLeaderRuntime::Think(int32 SID,double Now)
{
 FPhase3Scope Timing(Phase3,EPhase3Cost::Decision);
 if(Phase3 && Phase3->bEnabled) ++Phase3->Decisions;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* S=Store->FindSquad(SID); auto& P=Plans.FindChecked(SID);
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 if(Phase3 && Phase3->bEnabled && P.NextThink>0) Phase3->DecisionLateMS=FMath::Max(Phase3->DecisionLateMS,(Now-P.NextThink)*1000);
 P.NextThink=Now+FMath::Max(.2f,GetDefault<UBattleTuning>()->SquadDecisionSeconds);
 P.TargetID=INDEX_NONE; double Best=DBL_MAX;
 P.Center=FVector::ZeroVector; int32 Count=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) { P.Center+=Position(ID); ++Count; } if(Count) P.Center/=Count;
 if(S->Order!=EBattleOrder::Move) for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) if(const auto* Contacts=Sense->GetContacts(ID))
  for(const auto& C:*Contacts) if(C.Value.bVisible && !C.Value.bShared)
  {
   if(S->Order==EBattleOrder::Defend && FVector::Dist2D(C.Value.LastKnownPosition,S->Destination)>S->DefenseRadius) continue;
   const double D=FVector::DistSquared(C.Value.LastKnownPosition,P.Center);
   if(D<Best || (D==Best && C.Key<P.TargetID)) { Best=D; P.TargetID=C.Key; }
  }
 if(P.OrderRevision!=S->OrderRevision) { P.OrderRevision=S->OrderRevision; P.bReplan=true; }
 if(P.bReplan && Now>=P.NextPath && S->Order!=EBattleOrder::Hold)
 {
  P.NextPath=Now+1.; P.bReplan=false; P.Path.Reset();
  P.Center=FVector::ZeroVector; int32 N=0; for(int32 ID:S->Members) if(Store->Find(ID)->IsAlive()) { P.Center+=Position(ID); ++N; } if(N) P.Center/=N;
  FPhase3Scope PathTiming(Phase3,EPhase3Cost::Path);
  if(Phase3 && Phase3->bEnabled) ++Phase3->PathRequests;
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
  FNavLocation From,To;
  if(Nav && Nav->ProjectPointToNavigation(P.Center,From) && Nav->ProjectPointToNavigation(S->Destination,To))
  {
   auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),From.Location,To.Location);
   if(Path && Path->IsValid() && !Path->IsPartial()) P.Path=Path->PathPoints;
  }
  for(int32 ID:S->Members) if(auto* B=Bodies.Find(ID)) B->Waypoint=1;
 }
}
void UBattleLeaderRuntime::Follow(int32 ID,float Dt,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); if(!U || !U->IsAlive()) return;
 auto& B=Bodies.FindChecked(ID); if(B.Actor.IsValid() && B.Actor->IsPlayerControlled()) return;
 const auto* S=Store->FindSquad(U->SquadID); auto* P=Plans.Find(U->SquadID); if(!P || S->CommandStatus!=EBattleCommandStatus::Active) return;
 FVector Pos=Position(ID), Direction=Forward(ID); B.Velocity=FVector::ZeroVector;
 const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,P->TargetID);
 const bool CanEngage=Contact && Contact->bVisible && !Contact->bShared && CanSee(ID,P->TargetID);
 const bool StopForCombat=P->TargetID!=INDEX_NONE && S->Order!=EBattleOrder::Move;
 if(S->Order!=EBattleOrder::Hold && !StopForCombat && P->Path.IsValidIndex(B.Waypoint))
 {
  const FVector Segment=(P->Path[B.Waypoint]-P->Path[B.Waypoint-1]).GetSafeNormal2D();
  const int32 Slot=S->Members.IndexOfByKey(ID);
  // Shrink to the corridor center after a collision, then ask the commander to replan.
  const float Width=B.StalledSince>0?0.f:float(Slot%5-2)*80.f;
  const FVector Way=P->Path[B.Waypoint]+FVector(-Segment.Y,Segment.X,0)*Width;
  FVector D=(Way-Pos).GetSafeNormal2D();
  // A shared corridor, with local spacing; no per-follower NavMesh query.
  FVector Separation=FVector::ZeroVector;
  const FIntPoint Cell(FMath::FloorToInt(Pos.X/150.),FMath::FloorToInt(Pos.Y/150.));
  int32 Checked=0;
  for(int32 X=-1;X<=1 && Checked<32;++X) for(int32 Y=-1;Y<=1 && Checked<32;++Y)
   if(const auto* Nearby=AvoidanceGrid.Find(Cell+FIntPoint(X,Y))) for(int32 Other:*Nearby)
   {
    if(Other==ID || !Store->Find(Other)->IsAlive()) continue;
    if(++Checked>32) break;
    const FVector Away=Pos-Position(Other); const double Dist=Away.Size2D();
    if(Dist>1 && Dist<95) Separation+=Away.GetSafeNormal2D()*(95-Dist)/95;
   }
  Direction=(D+Separation*.65).GetSafeNormal2D();
  const float Speed=GetDefault<UBattleTuning>()->MoveSpeed;
  if(B.Actor.IsValid())
  {
   B.Actor->AddMovementInput(Direction); B.Velocity=B.Actor->GetVelocity();
   if(B.Velocity.Size2D()<10) { if(B.StalledSince==0) B.StalledSince=Now; }
   else B.StalledSince=0;
   if(B.StalledSince>0 && Now-B.StalledSince>2) { P->bReplan=true; B.StalledSince=Now; }
  }
  else
  {
   FHitResult Hit; const FVector Before=B.Capsule->GetComponentLocation();
   B.Capsule->MoveComponent(Direction*FMath::Min(Speed*Dt,float(FVector::Dist2D(Way,Pos))),Direction.Rotation().Quaternion(),true,&Hit);
   auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity); F.Position=B.Capsule->GetComponentLocation(); B.Velocity=(F.Position-Before)/FMath::Max(Dt,.001f);
   if(Hit.bBlockingHit && B.StalledSince==0) B.StalledSince=Now;
   else if(!Hit.bBlockingHit) B.StalledSince=0;
   if(B.StalledSince>0 && Now-B.StalledSince>2) { P->bReplan=true; B.StalledSince=Now; }
  }
  if(FVector::Dist2D(Position(ID),Way)<90) ++B.Waypoint;
 }
 else if(B.Actor.IsValid()) B.Actor->GetCharacterMovement()->StopMovementImmediately();
 if(CanEngage) Direction=(Contact->LastKnownPosition+FVector(0,0,35)-(Pos+FVector(0,0,60))).GetSafeNormal();
 if(B.Actor.IsValid()) B.Actor->SetActorRotation(FRotator(0,Direction.Rotation().Yaw,0));
 else
 {
  Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=Direction.GetSafeNormal2D();
  B.Capsule->SetWorldRotation(FRotator(0,Direction.Rotation().Yaw,0));
  const int32 Pose=U->ReloadEndTime>Now?2:B.Velocity.SizeSquared2D()>400?1:0;
  if(Pose!=B.Pose) { B.Pose=Pose; if(VAT)VAT->ChangePose(ID,Pose); if(!VAT || !VAT->IsFar(ID))B.Mesh->PlayAnimation(Pose==2?ReloadPose:Pose==1?WalkPose:IdlePose,true); }
 }
 Store->UpdateAim(ID,Now,B.Velocity.Size2D(),Direction,true);
 if(CanEngage) { if(U->Ammo==0) Store->StartReload(ID,Now); else Fire(ID,Pos+FVector(0,0,60),Direction,false); }
 Store->SetAction(ID,U->ReloadEndTime>0?EBattleAction::Reloading:CanEngage?EBattleAction::Engaging:B.Velocity.SizeSquared2D()>1?EBattleAction::Moving:EBattleAction::Idle);
}
bool UBattleLeaderRuntime::Fire(int32 ID,FVector Start,FVector Direction,bool bPlayerShot)
{
 FPhase3Scope Timing(Phase3,EPhase3Cost::Shot);
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* U=Store->Find(ID); const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
 if(!S || S->CommandStatus!=EBattleCommandStatus::Active || !U->IsAlive() || Mode->bBattleEnded || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
 const double Now=GetWorld()->GetTimeSeconds(); const auto* T=GetDefault<UBattleTuning>();
 Store->UpdateAim(ID,Now,Actor(ID)?Actor(ID)->GetVelocity().Size2D():Bodies.FindChecked(ID).Velocity.Size2D(),Direction,Actor(ID)?Actor(ID)->GetCharacterMovement()->IsMovingOnGround():true);
 if(!Store->ConsumeShot(ID,Now,T->ShotInterval(U->Weapon,bPlayerShot))) return false;
 if(Phase3 && Phase3->bEnabled) ++Phase3->Shots;
 Direction=BattleTactics::ShotDirection(*U,Direction,Now);
 auto Q=Sight(ID).Query; FHitResult Hit; const FVector End=Start+Direction.GetSafeNormal()*T->WeaponRange;
 GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q);
 const int32 Victim=HitIDs.FindRef(Hit.GetComponent());
 if(Victim>0 && Store->ApplyDamage(Victim,U->Team,T->ShotDamage))
  if(bPlayerShot) if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->HitMarkerUntil=Now+.18;
 if(U->Team==0 || (Actor(ID) && Actor(ID)->bRevealed)) DrawDebugLine(GetWorld(),Start,Hit.bBlockingHit?Hit.ImpactPoint:End,U->Team?FColor::Orange:FColor::Cyan,false,.09f,0,2);
 return true;
}
void UBattleLeaderRuntime::RefreshDisplay()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>(); TSet<int32> Visible;
 for(const auto& B:Bodies) if(Store->Find(B.Key)->Team==0 && Store->Find(B.Key)->IsAlive())
  if(const auto* Contacts=Sense->GetContacts(B.Key)) for(const auto& C:*Contacts) if(C.Value.bVisible) Visible.Add(C.Key);
 for(auto& B:Bodies)
 {
  const bool Show=Store->Find(B.Key)->Team==0 || Visible.Contains(B.Key);
  if(B.Value.Actor.IsValid()) B.Value.Actor->SetRevealed(Show);
  else { if(VAT)VAT->SetVisible(B.Key,Show); if(!VAT || !VAT->IsFar(B.Key)){BattleFollowerBones::SetVisible(B.Value.Mesh.Get(),Show,bReduceHiddenBones); if(bReduceAnimTasks) B.Value.AnimTasks.Apply(B.Value.Mesh.Get(),true); B.Value.Weapon->SetVisibility(Show && Store->Find(B.Key)->IsAlive());} }
 }
}
void UBattleLeaderRuntime::Step(float Dt)
{
 if(!bEnabled) return;
 if(Phase3 && Phase3->bEnabled) { Phase3->Step(Dt); return; }
 const double Now=GetWorld()->GetTimeSeconds(); auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(!Mode->bCombatPaused && !Mode->bBattleEnded && Now>=Mode->CombatStartsAt)
 {
  Store->AdvanceReloads(Now);
  GetWorld()->GetSubsystem<UBattlePerception>()->Update(Mode->Units,Now,-1,0);
  for(auto& P:Plans) if(Now>=P.Value.NextThink) Think(P.Key,Now);
  AvoidanceGrid.Reset();
  for(const auto& B:Bodies) if(Store->Find(B.Key)->IsAlive()) { const FVector P=Position(B.Key); AvoidanceGrid.FindOrAdd(FIntPoint(FMath::FloorToInt(P.X/150.),FMath::FloorToInt(P.Y/150.))).Add(B.Key); }
  // Death may invalidate a plan during firing; iterate stable UnitIDs, never a plan iterator.
  for(const auto& B:Bodies) Follow(B.Key,FMath::Min(Dt,.05f),Now);
 }
 int32 Living[2]={0,0}; for(const auto& U:Store->GetUnits()) if(U.Value.IsAlive()) ++Living[U.Value.Team];
 if(!Living[0] || !Living[1]) { Mode->bBattleEnded=true; Mode->Result=Living[0]?TEXT("VICTORY"):TEXT("DEFEAT"); }
 RefreshDisplay();
}
