// Shared mission permission: observed target positions only, never enemy true positions.
#include "BattleLeaderRuntime.h"
#include "BattleTactics.h"
#include "BattleSectors.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool BattleTactics::WeaponOrderGateEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleWeaponOrderGate="),V);return V!=0;}();
 return On;
}

bool BattleTactics::ArmourSectorGateEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArmourSectorGate="),V);return V!=0;}();
 return On;
}

bool BattleTactics::OrderAllowsTarget(const FBattleSquadState& S,int32 TargetID,const FVector& KnownPosition)
{
 if(S.Order==EBattleOrder::Move)return false;
 if(S.TacticalPlan.Action==EBattleSquadTactic::Support&&(TargetID<=0||TargetID!=S.TacticalPlan.SupportTarget))return false;
 const bool Sector=S.bSectorMission&&S.TacticalPlan.Action==EBattleSquadTactic::Mission;
 if(!Sector&&S.Order==EBattleOrder::Defend&&!DefendEngage(S.Team)&&FVector::Dist2D(KnownPosition,S.Destination)>S.DefenseRadius)return false;
 return true;
}

bool UBattleLeaderRuntime::WeaponOrderAllows(int32 ID,int32 TargetID,const FVector& KnownPosition) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
 if(!U||!U->IsAlive()||!S||S->CommandStatus!=EBattleCommandStatus::Active||!BattleTactics::OrderAllowsTarget(*S,TargetID,KnownPosition))return false;
 if(!S->bSectorMission||S->TacticalPlan.Action!=EBattleSquadTactic::Mission||BattleSectors::Cell(KnownPosition)==BattleSectors::Cell(S->Destination))return true;
 // Same outside-sector return-fire exception as the normal squad pick.
 return (SectorContactEnabled()||SectorMoveFireEnabled()||SectorFireBack.Contains(S->SquadID))&&
  SquadShotAt(*S,GetWorld()->GetTimeSeconds())&&FVector::DistSquared(Position(ID),KnownPosition)<=FMath::Square(TargetRange(ID));
}
