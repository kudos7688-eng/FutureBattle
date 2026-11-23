// In-sector and cross-sector fights (2026-11-23, trial -BattleSectorSight, default 0; Docs/design-sector-combat.md, user decision).
// Maps with sectors only. =1: in a rifle squad only the lookout (the leader; while the player controls him or he is down, the
// first other living member) scans for enemies in another sector. An enemy in the soldier's own sector, or within 20 m, is an
// in-sector enemy and every soldier scans for it as before. A member fires at a cross-sector enemy the lookout sees now when his
// own line of sight at the moment of firing is clear. Observed information only. -BattleSectorSightTeam=t limits it to one team.
// Part of UBattleLeaderRuntime.
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "BattleState.h"

int32 UBattleLeaderRuntime::SectorSightMode(int32 Team)
{
 static const int32 Value=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSight="),V);return FMath::Clamp(V,0,1);}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSightTeam="),V);return V==0||V==1?V:-1;}();
 return Only<0 || Only==Team ? Value : 0;
}

bool UBattleLeaderRuntime::SectorSightFar(const FVector& From,const FVector& To)
{
 return BattleSectors::Cell(From)!=BattleSectors::Cell(To) && FVector::DistSquared2D(From,To)>FMath::Square(2000.);
}

int32 UBattleLeaderRuntime::SectorLookout(const FBattleSquadState& Squad) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Fit=[&](int32 ID){const auto* U=Store->Find(ID);return U && U->IsAlive() && U->Action!=EBattleAction::DirectControl;};
 if(Fit(Squad.LeaderUnitID))return Squad.LeaderUnitID;
 for(int32 ID:Squad.Members)if(Fit(ID))return ID;
 return INDEX_NONE;
}

// True for a soldier who leaves the cross-sector scan to his squad's lookout.
bool UBattleLeaderRuntime::SectorSightMember(int32 ID) const
{
 if(!SectorSightMode(0) && !SectorSightMode(1))return false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U || !SectorSightMode(U->Team) || !BattleFieldLayout::Of(GetWorld()).bSectors)return false;
 const auto* S=Store->FindSquad(U->SquadID);
 return S && S->SquadType==BattleSquadType::Rifle && SectorLookout(*S)!=ID;
}
