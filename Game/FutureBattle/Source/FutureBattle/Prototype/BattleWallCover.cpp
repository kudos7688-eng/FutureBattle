#include "BattleLeaderRuntime.h"
#include "BattleUnit.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "BattleStance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Wall cover of the directly controlled soldier (user 11-24: "the player should use cover naturally and automatically when he
// comes up to a wall"; before, C only took the pre-made cover places with a safe peek and on map I mostly said there was none).
// It is the ordinary cover action (BattleCoverAction.cpp) at a free place right where he stands (FreeCoverSlot), so hiding,
// peeking, blind fire, the cover clips and the rules' heights work as they do there. AI soldiers never use it.
namespace
{
 constexpr float Radius=36.f;          // the soldiers' capsule radius (the cover sweeps use 36 x 88 too)
 constexpr float PushReach=Radius+25.f; // a wall this close in front while he walks into it
 constexpr float EnterReach=Radius+60.f; // C may take a wall up to 60 cm away
 constexpr float PushSeconds=.3f;       // pressing that long against the wall takes cover
 // Heights above the capsule centre (90 cm above the ground; BattleStance): the top of a crouched soldier (110 cm above the
 // ground) must be behind the wall, and a wall that also blocks the standing eye (150 cm) is a high wall.
 const float CrouchTop=BattleStance::TopOffset(BattleStance::Crouch),StandEye=BattleStance::EyeOffset(BattleStance::Stand);
 // A high wall's edge within this distance to the side lets him lean out past it (aim). The peek point is EdgeStep past it.
 constexpr float EdgeSteps[]={30.f,60.f,90.f,120.f};
 constexpr float EdgeStep=50.f;

 bool WallHit(const UWorld* World,const AActor* Self,const FVector& From,const FVector& Dir,float Reach,FHitResult& Hit)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleWallCover),false,Self);
  return World->LineTraceSingleByObjectType(Hit,From,From+Dir*Reach,FCollisionObjectQueryParams(ECC_WorldStatic),Query);
 }
}

bool UBattleLeaderRuntime::PlayerWallCoverEnabled()
{
 static const bool bOn=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattlePlayerWallCover="),V);return V!=0;}();
 return bOn;
}

bool UBattleLeaderRuntime::TryWallCover(int32 ID,FVector Toward,double Now)
{
 if(!PlayerWallCoverEnabled() || !bEnabled || !BattleTactics::CoverActionsEnabled() || IsVehicle(ID))return false;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase)return false;
 const auto* Squad=Store->FindSquad(U->SquadID);if(!Squad || Squad->CommandStatus!=EBattleCommandStatus::Active)return false;
 ABattleUnit* A=Actor(ID);if(!A)return false;
 const FVector Dir=FVector(Toward.X,Toward.Y,0).GetSafeNormal();if(Dir.IsNearlyZero())return false;
 const FVector Pos=A->GetActorLocation();
 // The wall: it hides the top of a crouched soldier and faces him (not a slope or a post brushed in passing).
 FHitResult Hit;
 if(!WallHit(GetWorld(),A,Pos+FVector(0,0,CrouchTop),Dir,EnterReach,Hit) || FMath::Abs(Hit.ImpactNormal.Z)>.4f)return false;
 const FVector Normal=FVector(Hit.ImpactNormal.X,Hit.ImpactNormal.Y,0).GetSafeNormal();
 if(Normal.IsNearlyZero() || FVector::DotProduct(Normal,Dir)>-.5f)return false;
 const float Depth=Hit.Distance+40.f; // how far behind his place the wall face is checked at other heights and places
 FHitResult Other;
 const bool bHigh=WallHit(GetWorld(),A,Pos+FVector(0,0,StandEye),-Normal,Depth,Other);
 const FVector Lateral=FVector::CrossProduct(Normal,FVector::UpVector);
 FVector Peek=Pos;bool bPeek=!bHigh; // low wall: he stands up where he is and fires over it
 if(bHigh)
 {
  // The nearest edge to either side: no wall in front of the standing eye there. The peek point lies EdgeStep past it, with
  // room to step there and ground under it.
  Peek=Pos+Lateral*220.f; // no edge: a nominal side for the cover clips; aimed fire leaves the cover instead (Fire)
  for(const float Step:EdgeSteps)
  {
   for(const float Side:{1.f,-1.f})
   {
    const FVector At=Pos+Lateral*(Side*Step);
    if(WallHit(GetWorld(),A,At+FVector(0,0,StandEye),-Normal,Depth,Other))continue;
    const FVector Out=Pos+Lateral*(Side*(Step+EdgeStep));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleWallCoverPeek),false,A);
    if(GetWorld()->SweepTestByObjectType(Pos,Out,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Radius,88.f),Query))continue;
    FHitResult Ground;
    if(!GetWorld()->LineTraceSingleByObjectType(Ground,Out,Out-FVector(0,0,200),FCollisionObjectQueryParams(ECC_WorldStatic),Query))continue;
    Peek=Out;bPeek=true;break;
   }
   if(bPeek)break;
  }
 }
 // Registered before the action starts: the stance changes of this cover action apply to the player off the stance maps too.
 WallCovers.Add(ID,{Normal,StanceOf(ID),bHigh,bPeek});WallPushSeconds.Remove(ID);
 U->CoverAnchor=Pos;U->CoverPeek=Peek;U->CoverSlot=FreeCoverSlot;U->CoverPhase=1;U->CoverSeconds=3;
 U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=INDEX_NONE;U->DecisionReason=TEXT("COVER / AT THE WALL");
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WALL_COVER unit=%d high=%d peek=%d peek_cm=%.0f wall_cm=%.0f"),ID,bHigh,bPeek,FVector::Dist2D(Pos,Peek),Hit.Distance);
 return true;
}

uint8 UBattleLeaderRuntime::StepPlayerWallCover(int32 ID,FVector Input,bool bAiming,bool bSprinting,float Dt,double Now)
{
 if(!PlayerWallCoverEnabled() || !bEnabled)return 0;
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);if(!U || !U->IsAlive())return 0;
 const FVector Flat(Input.X,Input.Y,0);const bool bInput=Flat.SizeSquared()>.09f;
 if(const FWallCover* W=WallCovers.Find(ID))
 {
  // Leaving: moving anywhere but into the wall (pressing on into it keeps the cover).
  if(bInput && FVector::DotProduct(Flat.GetSafeNormal(),-W->Normal)<.5f){CancelCoverAction(ID);return 2;}
  // Aiming stands him up over a low wall, or leans him out past the edge of a high one; he stays out while he aims.
  if(U->CoverPhase==2 && bAiming && W->bPeek && U->ReloadEndTime==0){U->CoverPhase=3;U->CoverSeconds=3;}
  if(U->CoverPhase==4 && bAiming)U->CoverSeconds=FMath::Max(U->CoverSeconds,.3f);
  return 0;
 }
 ABattleUnit* A=Actor(ID);
 if(U->CoverPhase || !bInput || bSprinting || !A || StanceOf(ID)==BattleStance::Prone || !A->GetCharacterMovement()->IsMovingOnGround()){WallPushSeconds.Remove(ID);return 0;}
 FHitResult Hit;const FVector Dir=Flat.GetSafeNormal();
 if(!WallHit(GetWorld(),A,A->GetActorLocation()+FVector(0,0,CrouchTop),Dir,PushReach,Hit) || FMath::Abs(Hit.ImpactNormal.Z)>.4f ||
    FVector::DotProduct(FVector(Hit.ImpactNormal.X,Hit.ImpactNormal.Y,0).GetSafeNormal(),Dir)>-.6f){WallPushSeconds.Remove(ID);return 0;}
 float& Pushed=WallPushSeconds.FindOrAdd(ID);Pushed+=Dt;
 return Pushed>=PushSeconds && TryWallCover(ID,Dir,Now)?1:0;
}

float UBattleLeaderRuntime::WallCoverYaw(int32 ID,const FVector& BodyForward) const
{
 const FWallCover* W=WallCovers.Find(ID);if(!W)return 0.f;
 return FMath::FindDeltaAngleDegrees(BodyForward.Rotation().Yaw,(-W->Normal).Rotation().Yaw);
}

FVector UBattleLeaderRuntime::CoverFacing(int32 ID,const FVector& Fallback) const
{
 const FWallCover* W=WallCovers.Find(ID);return W?-W->Normal:Fallback;
}
