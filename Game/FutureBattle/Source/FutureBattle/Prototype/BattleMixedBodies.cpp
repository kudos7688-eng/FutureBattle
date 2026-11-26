#include "BattleTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleVATDisplay.h"
#include "BattleSoldierBody.h"
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

bool UBattleLeaderRuntime::CreateBody(const FBattleBodyInit& Entry,AActor* Host,FBattleLeaderBody& B)
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
 auto* MeshAsset=BattleSoldierBody::LoadMesh(); // Quinn, or RB01 with -BattleSoldierBody=1
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
 GetDefault<ABattleUnit>()->MountWeapon(Weapon,Mesh,Entry.State.ActiveWeapon(),false);B.Weapon=Weapon;
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
