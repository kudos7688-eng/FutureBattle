#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "BattleVATDisplay.generated.h"
class UBattleLeaderRuntime;
class UBattleVATData;
class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
UCLASS()
class UBattleVATSkeletalMeshComponent : public USkeletalMeshComponent
{
 GENERATED_BODY()
public:
 TWeakObjectPtr<class ABattleVATDisplay> Presenter;
 int32 UnitID=0;
 bool VATFar=false;
 bool HasNativeTick=false;
 uint64 NativeTickFrame=0;
 float PendingDt=0;
 virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
 virtual bool ShouldCreateRenderState() const override;
 void EndAnimSkip();
 // A near follower's rifle pitch (UBattleLeaderRuntime::AimFollowerRifles, display only): after each evaluated pose the upper
 // body (spine_01 and every bone under it: chest, arms, head, the rifle on the hand) bends up or down by SpinePitch degrees
 // about spine_01. The legs and feet keep the clip's pose (the whole body tilted before, a toe or heel sank or lifted up to 8 cm).
 float SpinePitch=0.f;
 // The bend the newest evaluated pose was made with (SpinePitch then; 0 when it was not bent). The shown rifle is posed with
 // it, so the aim correction adds to this angle (a skipped evaluation does not show a newer SpinePitch yet).
 float AppliedSpinePitch=0.f;
 virtual void FinalizeBoneTransform() override;
private:
 TBitArray<> BendBones;
 const void* BendMesh=nullptr;
 int32 BendRoot=INDEX_NONE;
};
struct FBattleVATEntry
{
 TWeakObjectPtr<USkeletalMeshComponent> Body;
 TWeakObjectPtr<UStaticMeshComponent> Weapon;
 FTransform BodyRelative,WeaponRelative;
 FQuat NearRotation=FQuat::Identity; // the near body's own rotation (a far corpse is tipped over; a returning one is not)
 int32 ID=0,Pose=0,BodyGroup=0,Slot=0,WeaponGroup=1,WeaponSlot=0;
 float Time=0,Rate=1;
 bool Far=false,Visible=true,Dead=false,Initialized=false;
 FVector3f PreviousFrames=FVector3f::ZeroVector;
 uint64 AdvancedFrame=0;
};
// Mass follower presentation. No writes to simulation, capsules, perception or hit IDs.
UCLASS()
class ABattleVATDisplay : public AActor
{
 GENERATED_BODY()
public:
 ABattleVATDisplay();
 void PrepareRendering();
 bool Initialize(UBattleLeaderRuntime* Runtime);
 void Select(int32 ID,int32 LOD,double Distance,double Scale,const FString& View);
 bool IsFar(int32 ID) const;
 void SetVisible(int32 ID,bool Visible);
 void ChangePose(int32 ID,int32 Pose);
 void Freeze(int32 ID);
 // A dying follower leaves the far display for good: the far display has no death clips (call before Freeze).
 void ShowNear(int32 ID);
 void RemoveUnit(int32 ID);
 void ResetBodies();
 void Advance(int32 ID,float Dt);
 int64 StatAnimTicks=0,StatAnimEvals=0;
 float AnimDemand=0;
 uint64 AnimDemandFrame=0;
 int32 AnimStretch=1;
 int64 StatAnimStretch=0,StatAnimStretchFrames=0;
 int32 StatCorpseEnters=0,StatCorpseReturns=0; // corpses that went far, and came back lying on their death clip
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UBattleVATData> Data;
 UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
 UPROPERTY() TObjectPtr<UBattleLeaderRuntime> Runtime;
 UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> AuditBodies;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> AuditWeapons;
 TMap<int32,int32> AuditIndices;
 bool Audit=false;
 int32 AuditFrame=0,AuditErrors=0,AuditChecks=0,AuditReturns=0,AuditEnters=0;
 FString AuditCSV=TEXT("frame,id,far,dead,pose,time,reference_time,time_equal,bones_equal,weapon_equal\n");
 int64 StatFrames=0,StatFar=0,StatShown=0;
 double StatMS=0;
 int32 SavedRayWPO=0,SavedRayInstances=1;
 bool RaySettingsChanged=false;
 bool AssetsReady=false;
 uint64 AssetCheckFrame=MAX_uint64;
 TMap<int32,FBattleVATEntry> Entries;
 TArray<TArray<FTransform>> Transforms;
 void Enter(FBattleVATEntry& E);
 void Leave(FBattleVATEntry& E);
 void AdvanceFar(FBattleVATEntry& E,float Dt);
 FVector3f Frames(const FBattleVATEntry& E) const;
};
