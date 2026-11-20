#pragma once
#include "CoreMinimal.h"
#include "BattleState.h"

class UBattleMatch;
struct FAreaIntent;

// Stage 1 only: read-only diagnostics. No selection, purchase, wallet, or persistent planner state.
namespace BattleComposition
{
 enum ECapability : uint16 { Front=1, AntiInfantry=2, Observe=4, AntiArmour=8, Indirect=16, Mobile=32, Shield=64, SupplyAmmo=128 };
 struct FSettings
 {
  bool bEnabled=false;
  int32 Teams=0;
  bool Applies(int32 Team) const { return bEnabled&&Team>=0&&Team<2&&(Teams&(1<<Team))!=0; }
 };
 struct FTypeDef
 {
  uint8 TypeID=0,DefaultGroupRole=255;
  uint16 Capabilities=0,AllowedGroupRoles=0;
  int32 Cost=0,HardActiveLimit=0,Members=0;
  bool bAIEligible=false;
 };
 struct FTypeInput
 {
  int32 Active=0,Living=0,Armed=0,Returning=0;
  int32 HealthPermilleSum=0; // living members' HP fractions; not yet a capability/availability score
  int32 Keep=0,PacedTarget=0;
 };
 struct FInput
 {
  int32 Team=INDEX_NONE,Credits=0,Pending=0,Reserve=0,Free=0;
  int32 Active=0,Records=0,LegacyBought=0,LegacyHeavy=0,ObservedVehicles=0;
  float Elapsed=0,Remaining=0,HeavyShare=0,Interval=0,Stop=0,LastHireAt=-1000.f;
  bool bAutoAllowed=false,bAutoTargets=false,bHireEnabled=false;
  FTypeInput Types[BattleSquadType::Count];
 };
 FSettings Settings();
 const TArray<FTypeDef>& Definitions();
 FInput Capture(const UBattleMatch& Match,const UBattleState& Store,int32 Team,const FAreaIntent& ResolvedIntent,bool bAutoTargets,bool bAutoAllowed);
 FString Describe(const FInput& Input);
 void LogStart();
 void LogInput(const FInput& Input);
}
