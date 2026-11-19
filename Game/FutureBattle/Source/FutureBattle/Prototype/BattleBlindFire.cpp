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

bool UBattleLeaderRuntime::TryAIBlindFire(int32 ID,FVector Threat,double Now)
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!U || U->CoverPhase!=2 || U->Ammo<=0 || U->ReloadEndTime>Now || U->NextShotTime>Now)return false;
 if(FVector::Dist2D(Position(ID),Threat)>BattleRanges::Engage(U->Weapon))return false;
 const FVector Eye=Position(ID)+FVector(0,0,60);
 TGuardValue<bool> Guard(bAIBlindShot,true);
 return Fire(ID,Eye,(Threat+FVector(0,0,35)-Eye).GetSafeNormal(),false);
}
