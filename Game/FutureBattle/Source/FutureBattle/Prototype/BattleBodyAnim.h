#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BattleCoverClips.h"
#include "BattleBodyAnim.generated.h"

class UAnimSequence;
struct FBattleUnitState;

// Body animation (2026-11-19, user request; display only: sight, hits, heights and every rule are unchanged).
// Clips come from the asset samples (BattleBodyAnimImport.cpp): Lyra (rifle locomotion, crouch, death, hit, fire, aim poses)
// and the cover shooter sample (prone, stance transitions, cover). -BattleBodyAnim=0 restores the previous visuals
// (one whole-body clip per leader, a body tipped over at death, a squashed body when prone).
// - Leaders (soldier Actors, the player included) use UBattleBodyAnimInstance: a lower-body clip chosen from stance, movement
//   direction, stance transition or death, and an upper-body layer above spine_01 (aim pose blended by aim pitch, reload,
//   blind fire), plus one-shot overlays (rifle recoil, hit reaction). Soldiers with their own look (SetLook) keep the old path.
// - Near followers (Mass bodies with a mesh) keep one whole-body clip: prone clips and a death clip are added.
// - Far followers (VAT) are unchanged.
namespace BattleBodyAnim
{
 bool Enabled();
 enum class EClip : uint8
 {
  StandIdle,StandFwd,StandBwd,StandLeft,StandRight,StandJog,
  CrouchIdle,CrouchFwd,CrouchBwd,CrouchLeft,CrouchRight,
  ProneIdle,ProneFwd,ProneBwd,ProneLeft,ProneRight,
  StandToCrouch,CrouchToStand,StandToProne,ProneToStand,CrouchToProne,ProneToCrouch,
  Reload,ProneReload,Fire,AimUp,AimCenter,AimDown,
  HitFront1,HitFront2,HitFront3,HitFront4,HitBack,
  DeathFront1,DeathFront2,DeathFront3,DeathBack,DeathLeft,DeathRight,
  Count
 };
 UAnimSequence* Get(EClip Clip);
 // Idle or moving clip for a stance (0 stand, 1 crouch, 2 prone); LocalVelocity is in the body's frame (X forward, Y right).
 UAnimSequence* Locomotion(uint8 Stance,const FVector& LocalVelocity,bool bJog);
 UAnimSequence* Transition(uint8 From,uint8 To);
 // Death and hit clips by where the shot came from (ShotDirection = travel direction of the bullet; zero when unknown).
 UAnimSequence* Death(const FVector& BodyForward,const FVector& ShotDirection,int32 Seed);
 UAnimSequence* Hit(const FVector& BodyForward,const FVector& ShotDirection,int32 Seed);
 // Cover clips (BattleCoverClips.h, the table by the asset review session): a clip of /Game/.../Rifle/Cover/ by name (cached),
 // and the set for a cover action from its anchor, peek point and the body's facing toward the threat. Height costs one
 // terrain line, so callers pick the set once per cover action.
 UAnimSequence* CoverClip(const TCHAR* Name);
 BattleCoverClips::FSet CoverSet(UWorld* World,const FBattleUnitState& U,const FVector& Forward);
}

struct FBattleBodyLayer
{
 UAnimSequence* Clip=nullptr;
 float Time=0.f;
 bool bLoop=true;
};

struct FBattleBodyAnimProxy : public FAnimInstanceProxy
{
 FBattleBodyAnimProxy()=default;
 explicit FBattleBodyAnimProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance){}
 FBattleBodyLayer Base,Previous,Upper,Overlay;
 float PreviousWeight=0.f,UpperWeight=0.f,OverlayWeight=0.f,AimWeight=0.f,AimPitch=0.f;
 UAnimSequence* Aim[3]={nullptr,nullptr,nullptr};
 virtual void PreUpdate(UAnimInstance* InAnimInstance,float DeltaSeconds) override;
 virtual bool Evaluate(FPoseContext& Output) override;
};

UCLASS(Transient)
class UBattleBodyAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 // Inputs, set every frame on the game thread (ABattleUnit::UpdateBodyAnim).
 // The lower-body clip; a change cross-fades over 0.2 s. A clip that does not loop holds its last frame.
 void SetBase(UAnimSequence* Clip,bool bLoop=true);
 // The upper-body layer (above spine_01). Time<0 plays it on its own clock; Time>=0 sets the position (reload synced to the timer).
 void SetUpper(UAnimSequence* Clip,float Weight,float Time=-1.f);
 // Aim pose blended by pitch in degrees (up positive) under the upper layer; Weight 0 shows the base clip's own upper body.
 void SetAim(float PitchDegrees,float Weight);
 // One-shot upper-body overlay (recoil, hit reaction): plays once at Weight, then fades.
 void PlayOverlay(UAnimSequence* Clip,float Weight);
 UAnimSequence* GetBase() const {return BaseClip;}
protected:
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override {return new FBattleBodyAnimProxy(this);}
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override {delete InProxy;}
private:
 friend struct FBattleBodyAnimProxy;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> BaseClip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> PreviousClip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> UpperClip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> OverlayClip;
 float BaseTime=0.f,PreviousTime=0.f,PreviousWeight=0.f,UpperTime=0.f,UpperWeight=0.f,OverlayTime=0.f,OverlayWeight=0.f,AimPitch=0.f,AimWeight=0.f;
 bool bBaseLoop=true,bPreviousLoop=true,bUpperDriven=false;
};
