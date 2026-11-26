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
#include "BattlePerception.h"

int32 UBattleLeaderRuntime::SectorSightMode(int32 Team)
{
 static const int32 Value=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSight="),V);return FMath::Clamp(V,0,2);}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSightTeam="),V);return V==0||V==1?V:-1;}();
 return Only<0 || Only==Team ? Value : 0;
}

int32 UBattleLeaderRuntime::SectorSightHitMode()
{
 static const int32 Value=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSightHit="),V);return FMath::Clamp(V,0,2);}();
 return Value;
}

bool UBattleLeaderRuntime::SectorSightFar(const FVector& From,const FVector& To)
{
 // -BattleSectorSightNear=<cm> (default 2000, 500..20000): an enemy across a sector edge but this close is still an in-sector enemy.
 static const double Near=[]{double V=2000.;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorSightNear="),V);return FMath::Clamp(V,500.,20000.);}();
 return BattleSectors::Cell(From)!=BattleSectors::Cell(To) && FVector::DistSquared2D(From,To)>FMath::Square(Near);
}

int32 UBattleLeaderRuntime::SectorLookout(const FBattleSquadState& Squad) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Fit=[&](int32 ID){const auto* U=Store->Find(ID);return U && U->IsAlive() && U->Action!=EBattleAction::DirectControl;};
 if(Fit(Squad.LeaderUnitID))return Squad.LeaderUnitID;
 for(int32 ID:Squad.Members)if(Fit(ID))return ID;
 return INDEX_NONE;
}

// Who scans for cross-sector enemies, by squad type (user decision 2026-11-23, Docs/design-sector-combat.md 4.2).
// =1: rifle squads only (lookout). =2 adds: heavy and supply squads - lookout only; anti-tank squads - the lookout, and the
// anti-tank soldiers for vehicles only; mortar teams - nobody (they fire at reported positions); scouts, exo soldiers and
// vehicles scan as before. Returns 0 scans as before, 1 no cross-sector scan, 2 cross-sector vehicles only.
int32 UBattleLeaderRuntime::SectorSightRole(int32 ID) const
{
 if(!SectorSightMode(0) && !SectorSightMode(1))return 0;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 const int32 Sight=U?SectorSightMode(U->Team):0;
 if(!Sight || !BattleFieldLayout::Of(GetWorld()).bSectors)return 0;
 const auto* S=Store->FindSquad(U->SquadID);
 if(!S)return 0;
 const uint8 Type=S->SquadType;
 if(Sight>=2 && Type==BattleSquadType::Mortar)return 1;
 if(SectorLookout(*S)==ID)return 0;
 if(Type==BattleSquadType::Rifle)return 1;
 if(Sight<2)return 0;
 if(Type==BattleSquadType::Heavy || Type==BattleSquadType::Supply)return 1;
 if(Type==BattleSquadType::AntiTank)return U->Weapon==EBattleWeapon::AntiTank?2:1;
 return 0;
}

// The same observed-only fallback for ordinary fire and the optional cover peek. No traces or contact writes.
const FBattleContact* UBattleLeaderRuntime::SectorLookoutSighting(int32 ID,int32 TargetID,const FVector& Pos,const FBattleSquadState& Squad,const FBattleContact* Contact) const
{
 if(TargetID!=INDEX_NONE && !(Contact && Contact->bVisible && !Contact->bShared) && SectorSightMember(ID))
  if(const auto* Seen=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(SectorLookout(Squad),TargetID);Seen && Seen->bVisible && !Seen->bShared && SectorSightFar(Pos,Seen->LastKnownPosition))return Seen;
 return nullptr;
}
