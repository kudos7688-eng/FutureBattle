#include "BattleBlindFire.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleStance.h"
#include "BattleRanges.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
 int32 Switch(const TCHAR* Name,int32 Default){int32 Value=Default;FParse::Value(FCommandLine::Get(),Name,Value);return Value;}
 // Height of the blind shot above the capsule centre (about 90cm above the ground): 175cm and 200cm above the ground, over
 // the 130cm low walls of the urban maps and taller sandbags; the first that has a clear first stretch is used.
 const float OverTopHeights[]={85.f,110.f};
 // Around the edge: the shot leaves at chest height of a crouched soldier, first point past the edge toward the peek point.
 // The search reaches the peek point (cover action anchors sit up to about 220cm from it; at 150cm the cover test's anchor
 // found no point past the edge), capped at 250cm. The body stays at the anchor whatever the distance.
 constexpr float SideHeight=30.f,SideFirst=45.f,SideStep=35.f,SideMax=250.f;
 // Length of the first stretch toward the aim point that must be free of walls.
 constexpr float ProbeCm=150.f;
}

bool BattleBlindFire::PlayerEnabled(){static const bool bOn=Switch(TEXT("BattleBlindFire="),1)!=0;return bOn;}
bool BattleBlindFire::AIEnabled(){static const bool bOn=Switch(TEXT("BattleAIBlindFire="),0)!=0;return bOn;}
float BattleBlindFire::SpreadScale()
{
 static const float Scale=[]{float Value=4.f;FParse::Value(FCommandLine::Get(),TEXT("BattleBlindFireSpread="),Value);return FMath::Clamp(Value,.5f,20.f);}();
 return Scale;
}
bool BattleBlindFire::IsOverTop(const FBattleUnitState& U){return FVector::Dist2D(U.CoverAnchor,U.CoverPeek)<1.;}

UAnimSequence* BattleBlindFire::Clip(bool bOverTop)
{
 if(!BattleStance::AnimEnabled())return nullptr;
 static TStrongObjectPtr<UAnimSequence> Top,Side;static bool bLoaded=false;
 if(!bLoaded)
 {
  bLoaded=true;
  // Clip names can be swapped without a build while the choice is reviewed (-BattleBlindFireClipTop=, -BattleBlindFireClipSide=).
  FString TopName=TEXT("AS_CoverLo_AimU_down"),SideName=TEXT("AS_CoverLo_AimR");
  FParse::Value(FCommandLine::Get(),TEXT("BattleBlindFireClipTop="),TopName);FParse::Value(FCommandLine::Get(),TEXT("BattleBlindFireClipSide="),SideName);
  const FString Folder=TEXT("/Game/Characters/Mannequins/Anims/Rifle/Cover/");
  Top.Reset(LoadObject<UAnimSequence>(nullptr,*(Folder+TopName)));
  Side.Reset(LoadObject<UAnimSequence>(nullptr,*(Folder+SideName)));
  UE_LOG(LogTemp,Display,TEXT("BATTLE_BLINDFIRE_ANIM top=%s:%d side=%s:%d"),*TopName,Top.IsValid(),*SideName,Side.IsValid());
 }
 return bOverTop?Top.Get():Side.Get();
}

bool UBattleLeaderRuntime::BlindFireOrigin(int32 ID,FVector AimPoint,FVector& Origin) const
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!U || U->CoverPhase!=2)return false;
 const FVector Pos=Position(ID);
 const FVector Flat=(AimPoint-Pos).GetSafeNormal2D();
 if(Flat.IsNearlyZero())return false;
 const FCollisionObjectQueryParams Static(ECC_WorldStatic);
 auto Clear=[&](const FVector& A,const FVector& B){return !GetWorld()->LineTraceTestByObjectType(A,B,Static);};
 // The rifle is raised from the crouched head and must reach the origin without passing through the cover, and the bullet
 // must clear the cover on its first stretch.
 const FVector Head=Pos+FVector(0,0,BattleStance::TopOffset(BattleStance::Crouch));
 auto Try=[&](const FVector& P){return Clear(Head,P) && Clear(P,P+(AimPoint-P).GetSafeNormal()*ProbeCm);};
 if(BattleBlindFire::IsOverTop(*U))
 {
  for(float Up:OverTopHeights){const FVector P=Pos+Flat*35.f+FVector(0,0,Up);if(Try(P)){Origin=P;return true;}}
  return false;
 }
 const FVector Side=(U->CoverPeek-U->CoverAnchor).GetSafeNormal2D();
 const float Limit=FMath::Min(SideMax,float(FVector::Dist2D(U->CoverAnchor,U->CoverPeek)));
 for(float Out=SideFirst;Out<=Limit;Out+=SideStep){const FVector P=Pos+Side*Out+FVector(0,0,SideHeight);if(Try(P)){Origin=P;return true;}}
 return false;
}

bool BattleBlindFire::ExposeEnabled(){static const bool bOn=Switch(TEXT("BattleBlindFireExpose="),1)!=0;return bOn;}

bool UBattleLeaderRuntime::IsHeadExposed(int32 ID,double Now) const
{
 if(!BattleBlindFire::ExposeEnabled() || !IsBlindFiring(ID,Now))return false;
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 return U && U->CoverPhase==2 && BattleBlindFire::IsOverTop(*U);
}

float UBattleLeaderRuntime::TargetAimOffset(int32 ID) const
{
 return IsHeadExposed(ID,GetWorld()->GetTimeSeconds())?BattleBlindFire::ExposedAimOffset:BattleStance::AimOffset(StanceOf(ID));
}

float UBattleLeaderRuntime::TargetTopOffset(int32 ID) const
{
 return IsHeadExposed(ID,GetWorld()->GetTimeSeconds())?BattleBlindFire::ExposedTopOffset:BattleStance::TopOffset(StanceOf(ID));
}

float BattleBlindFire::CoverTop(UWorld* World,const FVector& Origin,const FVector& Body)
{
 // Three lines down, at the origin and 25 / 55 cm further toward the threat (the cover may stand a little ahead). The
 // highest hit more than 30 cm above the capsule's lower half is the cover; lower hits are the ground.
 float Top=Body.Z+45.f;bool bFound=false;
 const FVector Ahead=FVector(Origin.X-Body.X,Origin.Y-Body.Y,0.f).GetSafeNormal();
 for(const float Extra:{0.f,25.f,55.f})
 {
  const FVector From=Origin+Ahead*Extra;FHitResult Hit;
  if(World && World->LineTraceSingleByObjectType(Hit,From,FVector(From.X,From.Y,Body.Z-80.f),FCollisionObjectQueryParams(ECC_WorldStatic))
   && Hit.ImpactPoint.Z>Body.Z-30.f && (!bFound || Hit.ImpactPoint.Z>Top)){Top=Hit.ImpactPoint.Z;bFound=true;}
 }
 return Top;
}

float BattleBlindFire::OverTopYaw(const TCHAR* HiddenClip)
{
 if(!HiddenClip)return 0.f;
 struct FHip{const TCHAR* Name;float Yaw;};
 static const FHip Hips[]={{TEXT("AS_coverlo_R_idle"),98.f},{TEXT("AS_CoverLo_CoverL"),-109.f},{TEXT("AS_cover_hi_r_idle"),104.f},{TEXT("AS_CoverHi_CoverL"),-163.f}};
 for(const FHip& H:Hips)if(FCString::Stricmp(H.Name,HiddenClip)==0)return FRotator::NormalizeAxis(64.f-H.Yaw);
 return 0.f;
}

float UBattleLeaderRuntime::BlindFireYaw(int32 ID,const FVector& Forward) const
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);const FVector* Look=BlindFireLook.Find(ID);
 if(!U || !Look || BattleBlindFire::IsOverTop(*U))return 0.f;
 const FVector F=Forward.GetSafeNormal2D(),To=(*Look-Position(ID)).GetSafeNormal2D();
 if(F.IsNearlyZero() || To.IsNearlyZero())return 0.f;
 return FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F,To).Z,FVector::DotProduct(F,To)));
}

bool UBattleLeaderRuntime::TryAIBlindFire(int32 ID,FVector Threat,double Now)
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!U || U->CoverPhase!=2 || U->Ammo<=0 || U->ReloadEndTime>Now || U->NextShotTime>Now)return false;
 if(FVector::Dist2D(Position(ID),Threat)>BattleRanges::Engage(U->Weapon))return false;
 const FVector Eye=Position(ID)+FVector(0,0,60);
 TGuardValue<bool> Guard(bAIBlindShot,true);
 return Fire(ID,Eye,(Threat+FVector(0,0,35)-Eye).GetSafeNormal(),false);
}
