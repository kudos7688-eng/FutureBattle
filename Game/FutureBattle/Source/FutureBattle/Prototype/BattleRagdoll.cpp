#include "BattleRagdoll.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool BattleRagdoll::Enabled()
{
 static const bool bOn=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleRagdoll="),V);return V!=0;}();
 return bOn;
}

bool BattleRagdoll::Start(USkeletalMeshComponent* M)
{
 if(!Enabled()||!M)return false;
 if(!M->GetPhysicsAsset())
 {
  static TWeakObjectPtr<UPhysicsAsset> Asset;
  if(!Asset.IsValid())Asset=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
  if(!Asset.IsValid())return false;
  M->SetPhysicsAsset(Asset.Get(),false);
 }
 M->HandleExistingParallelEvaluationTask(true,true);
 M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 M->SetCollisionObjectType(ECC_PhysicsBody);
 M->SetCollisionResponseToAllChannels(ECR_Ignore);
 M->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
 M->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
 if(M->Bodies.IsEmpty())M->RecreatePhysicsState();
 if(M->Bodies.IsEmpty())return false;
 M->SetAllBodiesSimulatePhysics(true);M->bBlendPhysics=true;M->WakeAllRigidBodies();
 if(!M->IsComponentTickEnabled())M->SetComponentTickEnabled(true);
 return true;
}

void BattleRagdoll::Settle(USkeletalMeshComponent* M)
{
 if(!M||!M->IsAnySimulatingPhysics())return;
 M->HandleExistingParallelEvaluationTask(true,true);
 M->SetComponentTickEnabled(false);
 M->SetAllBodiesSimulatePhysics(false);
 M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
