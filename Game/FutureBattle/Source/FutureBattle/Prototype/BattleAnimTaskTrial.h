#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleAnimTasks.h"
#include "BattleAnimTaskTrial.generated.h"
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAnimSequence;
class UBattlePhase3;
UCLASS()
class ABattleAnimTaskTrial : public AActor
{
 GENERATED_BODY()
public:
 ABattleAnimTaskTrial();
 void Start(UBattlePhase3* InPhase);
 virtual void Tick(float Dt) override;
 bool Done=false;
private:
 UPROPERTY() TObjectPtr<UBattlePhase3> Phase;
 UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Meshes;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Weapons;
 UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Clips;
 TArray<int32> BoneUpdates;
 TArray<FBattleAnimTasks> Policies;
 int32 Frame=0,Errors=0,Checks=0,ReducedTicks=0,Reveals=0;
 bool Shown=true;
 void Compare(int32 Pair);
};
