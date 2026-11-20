#include "BattleTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleSave.h"
#include "BattleVATDisplay.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleCover.h"
#include "AIController.h"
#include "BattleSquadAI.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::CreateBody(const FBattleSavedUnit& Entry,AActor* Host,FBattleLeaderBody& B)
{
 if(!Manager || !Host || !Host->GetRootComponent()) return false;
 if(Entry.State.bActorRepresentation)
 {
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Unit=GetWorld()->SpawnActor<ABattleUnit>(Entry.Position,Entry.Rotation,Params);
  if(!Unit) return false;
  Unit->GetCharacterMovement()->bOrientRotationToMovement=false;
  Unit->GetCharacterMovement()->SetRVOAvoidanceWeight(1.f);
  Unit->GetCharacterMovement()->AddTickPrerequisiteActor(Mode);
  B.Actor=Unit;return true;
 }
 auto* MeshAsset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
 if(!MeshAsset || !IdlePose) return false;
 const auto Archetype=Manager->CreateArchetype({FBattleFollowerFragment::StaticStruct()});
 B.Entity=Manager->CreateEntity(Archetype);
 if(!B.Entity.IsSet() || !Manager->IsEntityValid(B.Entity))return false;
 auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);
 F.UnitID=Entry.State.UnitID;F.Position=Entry.Position;F.Forward=Entry.Rotation.Vector().GetSafeNormal2D();
 auto* Capsule=NewObject<UCapsuleComponent>(Host);Host->AddInstanceComponent(Capsule);
 Capsule->InitCapsuleSize(36,90);Capsule->SetCollisionObjectType(ECC_Pawn);
 Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Capsule->SetCollisionResponseToAllChannels(ECR_Block);
 Capsule->SetCanEverAffectNavigation(false);Capsule->SetupAttachment(Host->GetRootComponent());
 if(FollowerOverlapsOff())Capsule->SetGenerateOverlapEvents(false);
 Capsule->RegisterComponent();Capsule->SetWorldLocation(Entry.Position);B.Capsule=Capsule;
 auto* Mesh=bDisplayLODEnabled?NewObject<USkeletalMeshComponent>(Host,UBattleVATSkeletalMeshComponent::StaticClass()):NewObject<USkeletalMeshComponent>(Host);
 Host->AddInstanceComponent(Mesh);Mesh->SetSkeletalMesh(MeshAsset);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Mesh->SetupAttachment(Capsule);Mesh->SetRelativeLocation(FVector(0,0,-90));
 Mesh->SetRelativeRotation(FRotator(0,-90,0));if(FollowerOverlapsOff())Mesh->SetGenerateOverlapEvents(false);Mesh->RegisterComponent();Mesh->PlayAnimation(IdlePose,true);B.Mesh=Mesh;
 auto* Weapon=NewObject<UStaticMeshComponent>(Host);Host->AddInstanceComponent(Weapon);
 Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);if(FollowerOverlapsOff())Weapon->SetGenerateOverlapEvents(false);Weapon->RegisterComponent();
 GetDefault<ABattleUnit>()->MountWeapon(Weapon,Mesh,Entry.State.Weapon,false);B.Weapon=Weapon;
 return true;
}
void UBattleLeaderRuntime::DestroyBody(FBattleLeaderBody& B)
{
 if(B.Actor.IsValid())
 {
  if(B.Actor->SavedAI) B.Actor->SavedAI->Destroy();
  B.Actor->Destroy();
 }
 else
 {
  if(B.Weapon.IsValid()) B.Weapon->DestroyComponent();
  if(B.Mesh.IsValid()) B.Mesh->DestroyComponent();
  if(B.Capsule.IsValid()) B.Capsule->DestroyComponent();
  if(Manager && B.Entity.IsSet() && Manager->IsEntityValid(B.Entity)) Manager->DestroyEntity(B.Entity);
 }
}
bool UBattleLeaderRuntime::RestoreMixedBodies(const UBattleSave* Save,FString& Error)
{
 if(!IsWideField() || !Save || Save->Version<2) {Error=TEXT("Mixed snapshot required");return false;}
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 if(!PC || !Mode) {Error=TEXT("Player or game mode unavailable; battle retained");return false;}
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 if(!Nav) {Error=TEXT("Navigation unavailable; battle retained");return false;}
 CoverApproaches.Reset();FirePositions.Reset();
 SuppressionZeroAt.Reset();
 Stances.Reset();StanceLockedUntil.Reset();
 TMap<int32,FVector> Positions;
 TMap<int32,TArray<FVector>> ValidatedPaths;
 for(const auto& E:Save->Units) Positions.Add(E.State.UnitID,E.Position);
 for(const auto& S:Save->Squads) if(S.CommandStatus==EBattleCommandStatus::Active && S.Order!=EBattleOrder::Hold)
 {
  const FVector LeaderAt=Positions.FindChecked(S.LeaderUnitID);
  auto* Path=Nav->FindPathToLocationSynchronously(GetWorld(),LeaderAt,S.Destination);
  if(!Path || !Path->IsValid() || Path->IsPartial())
  {
   // Only when the raw position fails: a living leader may stand just off the NavMesh (e.g. at a cover edge).
   // Retry from projected points, so loads that already succeeded keep their exact route.
   FNavLocation From,To;
   if(Nav->ProjectPointToNavigation(LeaderAt,From,FVector(100,100,400)) && Nav->ProjectPointToNavigation(S.Destination,To,FVector(100,100,400)))
    Path=Nav->FindPathToLocationSynchronously(GetWorld(),From.Location,To.Location);
   if(!Path || !Path->IsValid() || Path->IsPartial())
   {
    // A leader already standing on its own destination needs no route (same convention as the regroup path).
    if(FVector::Dist2D(LeaderAt,S.Destination)<50)
    {
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_RESTORE_ROUTE_IN_PLACE squad=%d leader=%d order=%d at=%s"),S.SquadID,S.LeaderUnitID,int32(S.Order),*LeaderAt.ToCompactString());
     ValidatedPaths.Add(S.SquadID,{LeaderAt,S.Destination});continue;
    }
    UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_MIXED_RESTORE_ROUTE_FAIL squad=%d leader=%d order=%d at=%s destination=%s"),S.SquadID,S.LeaderUnitID,int32(S.Order),*LeaderAt.ToCompactString(),*S.Destination.ToCompactString());
    Error=TEXT("Saved route unreachable; battle retained");return false;
   }
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_RESTORE_ROUTE_PROJECTED squad=%d leader=%d at=%s projected=%s"),S.SquadID,S.LeaderUnitID,*LeaderAt.ToCompactString(),*From.Location.ToCompactString());
  }
  ValidatedPaths.Add(S.SquadID,Path->PathPoints);
 }
 auto* NewHost=GetWorld()->SpawnActor<AActor>();
 if(!NewHost) {Error=TEXT("Could not stage body host; battle retained");return false;}
 auto* Root=NewObject<USceneComponent>(NewHost);NewHost->SetRootComponent(Root);
 NewHost->AddInstanceComponent(Root);Root->RegisterComponent();
 TMap<int32,FBattleLeaderBody> Staged;
 for(const auto& E:Save->Units)
 {
  if(E.State.bRetired)continue;
  FBattleLeaderBody B;
  const bool InjectedFailure=Staged.Num()==RestoreStageFailureAfter && FParse::Param(FCommandLine::Get(),TEXT("BattleMixedSaveSmoke"));
  if(InjectedFailure || !CreateBody(E,NewHost,B))
  {
   DestroyBody(B);for(auto& Pair:Staged) DestroyBody(Pair.Value);NewHost->Destroy();
   Error=TEXT("Could not stage representations; battle retained");return false;
  }
  Staged.Add(E.State.UnitID,B);
 }
 // No further fallible validation after this transaction boundary.
 PC->ResetMatchControl();
 const bool UseVAT=bDisplayLODEnabled;
 if(VAT)VAT->ResetBodies();
 for(auto& Pair:Bodies) DestroyBody(Pair.Value);
 if(DisplayHost) DisplayHost->Destroy();
 DisplayHost=NewHost;Bodies=MoveTemp(Staged);HitIDs.Reset();Plans.Reset();AvoidanceGrid.Reset();Mode->Units.Reset();Corpses.Reset();HiddenCorpses.Reset();
 GetWorld()->GetSubsystem<UBattleSquadAI>()->Reset();GetWorld()->GetSubsystem<UBattleCover>()->ResetReservations();
 const double Now=GetWorld()->GetTimeSeconds();TArray<FBattleUnitState> Records;
 for(const auto& E:Save->Units)
 {
  auto U=E.State;U.ReloadEndTime=U.ReloadEndTime>0?Now+U.ReloadEndTime:0;U.NextShotTime+=Now;
  U.CoverSlot=INDEX_NONE;U.AimStableSince=-1;U.MoveRetries=0;U.Role=EBattleRole::Independent;U.CoordinatedTarget=INDEX_NONE;
  U.Action=U.IsAlive()?EBattleAction::Idle:EBattleAction::Dead;U.DecisionReason=TEXT("RESTORED / REVALIDATE PATH");
  if(Save->CombatStateVersion<3&&BattleTactics::SPGAmmoEnabled())
   for(const auto& Squad:Save->Squads)if(Squad.SquadID==U.SquadID&&Squad.SquadType==BattleSquadType::SPGun)
    U.SPGAmmo=FMath::Clamp(GetDefault<UBattleTuning>()->SPGInitialAmmo,0,FMath::Max(0,GetDefault<UBattleTuning>()->SPGAmmoCapacity));
  Records.Add(U);
 }
 TArray<FBattleSquadState> Squads=Save->Squads;
 for(auto& S:Squads) S.TacticalPlan=Save->SavedPlans.FindChecked(S.SquadID);
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();Store->RestoreRecords(Records,Squads,true);
 for(auto& Pair:Bodies)
 {
  auto& B=Pair.Value;
  if(B.Actor.IsValid()){B.Actor->InitializeUnit(Pair.Key);Mode->Units.Add(B.Actor.Get());HitIDs.Add(B.Actor->GetCapsuleComponent(),Pair.Key);}
  else HitIDs.Add(B.Capsule.Get(),Pair.Key);
 }
 // Spawning/possessing a Character performs floor adjustment. Reapply the
 // snapshot after initialization so paused restores retain the saved transform.
 for(const auto& E:Save->Units)if(auto* Unit=Actor(E.State.UnitID))
  Unit->SetActorLocationAndRotation(E.Position,E.Rotation,false,nullptr,ETeleportType::TeleportPhysics);
 if(UseVAT && VAT)
 {
  if(!VAT->Initialize(this)){VAT->Destroy();VAT=nullptr;} // Safe skeletal fallback; canonical state is intact.
 }
 for(const auto& S:Squads) if(S.CommandStatus==EBattleCommandStatus::Active)
 {
  FBattleLeaderPlan P;P.Center=Position(S.LeaderUnitID);P.OrderRevision=S.OrderRevision;
  if(const auto* Path=ValidatedPaths.Find(S.SquadID)){P.Path=*Path;P.bReplan=false;}
  Plans.Add(S.SquadID,P);
 }
 // Vehicles: rebuild their bodies (wide capsule, placeholder shapes) before any path is planned.
 VehicleIDs.Reset();Turrets.Reset();ExoLooks.Reset();VehicleSightCache.Reset();VehicleOrders.Reset();
 // Revision 0 saves have no artillery state. Revision 1 restores relative clocks and launch order.
 Artillery.Reset();ArtilleryShells.Reset();
 for(const auto& Pair:Save->SavedArtillery)
 {
  const auto& E=Pair.Value;FArtillery A;
  A.StillSince=E.bSettingUp?Now:-1.;A.SetupCredit=E.SetupElapsed;
  A.ReadyAt=Now+E.ReloadRemaining;A.NextPick=Now+E.PickRemaining;
  A.Shots=E.Shots;A.TargetKey=E.TargetKey;A.Aim=E.Aim;A.bHasTarget=E.bHasTarget;A.bBase=E.bBase;Artillery.Add(Pair.Key,A);
 }
 for(const auto& E:Save->SavedArtilleryShells)
 {
  FArtilleryShell Shell;Shell.ShooterID=E.ShooterID;Shell.Team=E.Team;Shell.ShooterType=E.ShooterType;
  Shell.bSPG=E.bSPG;Shell.At=E.At;Shell.Remaining=E.Remaining;ArtilleryShells.Add(Shell);
 }
 for(const auto& S:Squads)if(S.CommandStatus==EBattleCommandStatus::Active&&BattleSquadType::IsVehicle(S.SquadType))ConfigureVehicle(S.LeaderUnitID);
 for(const auto& S:Squads)ApplySquadLook(S.SquadID);
 for(const auto& U:Records) if(!U.IsAlive()) OnDeath(U.UnitID);
 for(const auto& S:Squads)if(S.CommandStatus==EBattleCommandStatus::Active && S.Order!=EBattleOrder::Hold)BuildMovementPlan(S.SquadID);
 bSummaryDirty=true;PC->BindLeaderDeaths();return true;
}

