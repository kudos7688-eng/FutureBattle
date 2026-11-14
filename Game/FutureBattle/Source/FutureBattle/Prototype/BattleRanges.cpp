#include "BattleRanges.h"
#include "BattleFieldLayout.h"
#include "Engine/World.h"
#include "FutureBattle.h"

const BattleRanges::FProfile& BattleRanges::Get()
{
 // Resolved once per world: the profile depends on the map, and UBattleTuning has no world of its own.
 static FProfile Profile;static TWeakObjectPtr<const UWorld> ResolvedFor;static bool bResolved=false;
 const UWorld* World=GWorld;
 if(!bResolved||ResolvedFor.Get()!=World)
 {
  bResolved=true;ResolvedFor=World;Profile=FProfile();
  const TCHAR* Line=FCommandLine::Get();int32 Mode=-1;FParse::Value(Line,TEXT("BattleRangeProfile="),Mode);
  Profile.bActive=Mode<0?BattleFieldLayout::IsUrban(World):Mode!=0;
  if(Profile.bActive)
  {
   FParse::Value(Line,TEXT("BattleRangeSight="),Profile.Sight);
   // Longer names first is not needed: FParse matches the whole "Key=" token.
   FParse::Value(Line,TEXT("BattleRangeEngage="),Profile.EngageCarbine);FParse::Value(Line,TEXT("BattleRangeEngageSupport="),Profile.EngageSupport);
   FParse::Value(Line,TEXT("BattleRangeReach="),Profile.ReachCarbine);FParse::Value(Line,TEXT("BattleRangeReachSupport="),Profile.ReachSupport);
   auto Fix=[](float& Value,float Low,float High,float Default){Value=FMath::IsFinite(Value)?FMath::Clamp(Value,Low,High):Default;};
   Fix(Profile.Sight,1000.f,50000.f,20000.f);Fix(Profile.EngageCarbine,500.f,100000.f,15000.f);Fix(Profile.EngageSupport,500.f,100000.f,25000.f);
   Fix(Profile.ReachCarbine,500.f,200000.f,50000.f);Fix(Profile.ReachSupport,500.f,200000.f,60000.f);
   // A bullet always reaches at least as far as the AI is willing to fire.
   Profile.ReachCarbine=FMath::Max(Profile.ReachCarbine,Profile.EngageCarbine);Profile.ReachSupport=FMath::Max(Profile.ReachSupport,Profile.EngageSupport);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_RANGE_PROFILE active=1 sight_cm=%.0f engage_cm=%.0f/%.0f reach_cm=%.0f/%.0f (carbine/support)"),
    Profile.Sight,Profile.EngageCarbine,Profile.EngageSupport,Profile.ReachCarbine,Profile.ReachSupport);
  }
 }
 return Profile;
}

float BattleRanges::Engage(EBattleWeapon Weapon)
{
 const auto& P=Get();
 return P.bActive?(Weapon==EBattleWeapon::SupportGun?P.EngageSupport:P.EngageCarbine):GetDefault<UBattleTuning>()->EffectiveWeaponRange();
}

float BattleRanges::Reach(EBattleWeapon Weapon)
{
 const auto& P=Get();
 return P.bActive?(Weapon==EBattleWeapon::SupportGun?P.ReachSupport:P.ReachCarbine):GetDefault<UBattleTuning>()->EffectiveWeaponRange();
}
