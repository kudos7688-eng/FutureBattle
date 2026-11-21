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
  JogBwd,JogLeft,JogRight,
  Count
 };
 UAnimSequence* Get(EClip Clip);
 // Idle or moving clip for a stance (0 stand, 1 crouch, 2 prone); LocalVelocity is in the body's frame (X forward, Y right).
 // bRun: standing, the rifle held low in a jog in the direction of travel (moving without firing); else the aimed walk.
 UAnimSequence* Locomotion(uint8 Stance,const FVector& LocalVelocity,bool bRun);
 // The ground speed (cm/s) a locomotion clip's steps were made for: its root track's travel over the clip divided by its length
 // (the clips carry root motion and play in place). 0 when the root does not travel. Cached per clip; logged once.
 float GroundSpeed(const UAnimSequence* Clip);
 // The play rate at which Clip's steps keep pace with walking at Speed (cm/s): Speed / GroundSpeed within 0.5 to 2; 1 when unknown.
 float MatchRate(const UAnimSequence* Clip,float Speed);
 // How long a soldier keeps his rifle up after his last shot before he runs with it lowered again (display only).
 constexpr double ReadySeconds=3.;
 UAnimSequence* Transition(uint8 From,uint8 To);
 // Death and hit clips by where the shot came from (ShotDirection = travel direction of the bullet; zero when unknown).
 UAnimSequence* Death(const FVector& BodyForward,const FVector& ShotDirection,int32 Seed);
 UAnimSequence* Hit(const FVector& BodyForward,const FVector& ShotDirection,int32 Seed);
 // The shortest time between two hit reactions on one body: under steady fire a new flinch every hit kept the body bent over
 // (it never stood back up); a hit inside this time shows no new reaction (display only, the damage is unchanged).
 constexpr double HitReactSeconds=1.2;
 // Moving stands an AI soldier up: the stand-up clip at its own speed played in place while the body already moved at full
 // speed (it slid along the ground while getting up). On the move the rest of a stance transition plays fast enough to end
 // within this time: 0.4 s, 0.6 s for one to or from prone (the prone clips are long: 2.7 s from prone to standing).
 double MovingTransitionSeconds(const UAnimSequence* Transition);
 // The rifle model's grip relative to hand_r (calibrated against the rifle idle pose; the barrel along the model's local -Y).
 FTransform RifleGrip();
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
 float PreviousWeight=0.f,UpperWeight=0.f,OverlayWeight=0.f,AimWeight=0.f,AimPitch=0.f,RaiseWeight=0.f;
 FVector RaiseTarget=FVector::ZeroVector;
 UAnimSequence* Aim[3]={nullptr,nullptr,nullptr};
 virtual void PreUpdate(UAnimInstance* InAnimInstance,float DeltaSeconds) override;
 virtual bool Evaluate(FPoseContext& Output) override;
 void EvaluateLayers(FPoseContext& Output);
 void RaiseHands(FCompactPose& Pose) const;
 // While aiming: the rifle sits in hand_r, so each locomotion clip's hand points it its own way (a strafe or a jog-to-walk
 // blend turned it away from the shot). spine_03 turns the whole upper body so the barrel points along the aim (the body's
 // forward at AimPitch), at most MaxMuzzleCorrection degrees, scaled by AimWeight. Applied after the aim pose and before the
 // upper clip and overlays (a reload clip replaces it; recoil and hit motion stay on top of it).
 void AlignMuzzle(FCompactPose& Pose) const;
 static constexpr float MaxMuzzleCorrection=50.f;
};

UCLASS(Transient)
class UBattleBodyAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 // Inputs, set every frame on the game thread (ABattleUnit::UpdateBodyAnim).
 // The lower-body clip; a change cross-fades over 0.2 s. A clip that does not loop holds its last frame. Rate scales its clock.
 void SetBase(UAnimSequence* Clip,bool bLoop=true,float Rate=1.f);
 // The upper-body layer (above spine_01). Time<0 plays it on its own clock; Time>=0 sets the position (reload synced to the timer).
 void SetUpper(UAnimSequence* Clip,float Weight,float Time=-1.f);
 // Aim pose blended by pitch in degrees (up positive) under the upper layer; Weight 0 shows the base clip's own upper body.
 void SetAim(float PitchDegrees,float Weight);
 // One-shot upper-body overlay (recoil, hit reaction): plays once at Weight, then fades.
 void PlayOverlay(UAnimSequence* Clip,float Weight);
 // Both hands moved together (the rifle keeps its grip and direction) so the right hand reaches Target, in the mesh
 // component's space, by two-bone IK on each arm after every layer; Weight 0 turns it off. The arms only reach as far as
 // they are long: the body itself does not rise. Used by the over-the-top blind fire (the rifle held above the cover).
 void SetHandRaise(const FVector& Target,float Weight){RaiseTarget=Target;RaiseWeight=FMath::Clamp(Weight,0.f,1.f);}
 UAnimSequence* GetBase() const {return BaseClip;}
 float GetBaseRate() const {return BaseRate;}
 UAnimSequence* GetOverlay() const {return OverlayClip;}
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
 float BaseRate=1.f,BaseTime=0.f,PreviousTime=0.f,PreviousWeight=0.f,UpperTime=0.f,UpperWeight=0.f,OverlayTime=0.f,OverlayWeight=0.f,AimPitch=0.f,AimWeight=0.f,RaiseWeight=0.f;
 FVector RaiseTarget=FVector::ZeroVector;
 bool bBaseLoop=true,bPreviousLoop=true,bUpperDriven=false;
};
