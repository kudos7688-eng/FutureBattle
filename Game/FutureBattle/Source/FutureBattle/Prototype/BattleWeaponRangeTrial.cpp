#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunWeaponRangeTrial()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 Mode->bCombatPaused=true;Mode->bBattleEnded=false;
 const float Range=GetDefault<UBattleTuning>()->EffectiveWeaponRange();
 auto Place=[&](int32 ID,FVector P)
 {
  auto& B=Bodies.FindChecked(ID);
  if(B.Actor.IsValid()){B.Actor->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else {B.Capsule->SetWorldLocation(P);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=P;}
  B.Velocity=FVector::ZeroVector;
 };
 for(const auto& Pair:Store->GetUnits()) Place(Pair.Key,FVector(7000+(Pair.Key%20)*110,2000+(Pair.Key/20)*110,100));
 int32 Cases=0;
 for(int32 Observer:{1,252}) for(auto Weapon:{EBattleWeapon::Carbine,EBattleWeapon::SupportGun}) for(bool Player:{false,true})
 {
  if(Player && !Actor(Observer)) continue; // Mass followers cannot be possessed.
  const int32 Target=Observer==1?252:1;
  for(float Offset:{-100.f,100.f})
  {
   const FVector Origin(-9500,-8000,100);
   Place(Observer,Origin);Place(Target,Origin+FVector(Range+Offset,0,0));
   auto& U=*const_cast<FBattleUnitState*>(Store->Find(Observer));
   auto& V=*const_cast<FBattleUnitState*>(Store->Find(Target));
   U.Weapon=Weapon;U.Ammo=30;U.Reserve=0;U.ReloadEndTime=0;U.NextShotTime=0;
   // Isolate the range boundary from aim randomness; the shared weapon code still runs.
   U.SpreadSkillScale=.001f;U.AimStableSince=GetWorld()->GetTimeSeconds()-10;V.HP=100000;
   const FVector Start=Origin+FVector(0,0,60);
   const FVector Direction=(Position(Target)-Start).GetSafeNormal();
   const bool Fired=Player?Actor(Observer)->FireAt(Start,Direction,true):Fire(Observer,Start,Direction,false);
   const bool Hit=V.HP<100000;
   const bool Passed=Fired && U.Ammo==29 && Hit==(Offset<0);
   if(!Passed) ++SmokeFailures;
   ++Cases;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WEAPON_CASE %s observer=%d weapon=%d player=%d range_cm=%.1f target_cm=%.1f hit=%d"),Passed?TEXT("PASS"):TEXT("FAIL"),Observer,int32(Weapon),Player,Range,Range+Offset,Hit);
  }
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WEAPON_RESULT failures=%d cases=%d"),SmokeFailures,Cases);
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}

