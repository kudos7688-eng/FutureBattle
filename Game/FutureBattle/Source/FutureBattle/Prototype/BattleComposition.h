#pragma once
#include "CoreMinimal.h"
#include "BattleState.h"
#include "BattleCompositionState.h"

class UBattleMatch;
struct FAreaIntent;

// Selection is pure with respect to the world: explicit input and prior state -> decision and next state.
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
  int32 Keep=0,PacedTarget=0,Effective=0,AntiArmourEffective=0;
 };
 struct FInput
 {
  int32 Team=INDEX_NONE,Credits=0,Pending=0,Reserve=0,Free=0;
  int32 HomeDefenders=0;
  int32 UnitSlots=1000; // shared retained-unit capacity, including losses
  int32 WindowAtMS=0,WindowCredits=0,WindowIncome=0,WindowHireSpent=0;
  int32 Active=0,Records=0,LegacyBought=0,LegacyHeavy=0,ObservedVehicles=0;
  float Elapsed=0,Remaining=0,HeavyShare=0,Interval=0,Stop=0,LastHireAt=-1000.f;
  bool bAutoAllowed=false,bAutoTargets=false,bHireEnabled=false;
  FTypeInput Types[BattleSquadType::Count];
  int32 NowMS=0,StopAtMS=0,ArrivalDeadlineMS=0,MaxActive=0,Mission=-1,FrontMinimum=0,FrontEffective=0;
  int32 NetPerMinute=0,RoleEffective[5]={0,0,0,0,0};
  int32 ArrivalMS[BattleSquadType::Count]={};
  bool bMissionRequired=false,bHomeThreat=false,bCanPurchase=true;
 };
 enum class EAction : uint8 { Wait, Save, Buy };
 struct FCandidate
 {
  int32 Type=-1,Mission=-1,Target=0,Deficit=0,RoleDeficit=0,Urgency=0,Score=0,Cost=0,ArrivalMS=0,Waiting=0;
  FString Excluded;
 };
 struct FDecision
 {
  EAction Action=EAction::Wait;
  int32 Type=-1,Mission=-1;
  FString Reason;
  FCompositionTeamState Next;
  TArray<FCandidate> Candidates;
 };
 enum class EFailure : uint8 { Placement, Capacity, Configuration };
 EFailure ClassifyFailure(const FString& Message);
 constexpr int32 PolicyVersion=2;
 FDecision Decide(const FInput& Input,const TArray<FTypeDef>& Defs,const FCompositionTeamState& Previous);
 void CompletePurchase(FCompositionTeamState& State,bool bSuccess,int32 Type,EFailure Failure=EFailure::Placement);
 FString RulesKey();
 bool ValidateState(const FCompositionTeamState& State,int32 NowMS,int32 Objectives,FString& Error);
 FSettings Settings();
 const TArray<FTypeDef>& Definitions();
 FInput Capture(const UBattleMatch& Match,const UBattleState& Store,int32 Team,const FAreaIntent& ResolvedIntent,bool bAutoTargets,bool bAutoAllowed);
 FString Describe(const FInput& Input);
 void LogStart();
 void LogInput(const FInput& Input);
}
