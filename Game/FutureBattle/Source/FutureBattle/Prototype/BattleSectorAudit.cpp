// Sector orders (map H, BattleSectors.h): verification -BattleSectorAudit (headless, a map with sectors). Written to check
// Codex's sector capture/hold orders (2026-11-19); it only issues orders and reads state, it changes no rule.
//  1. At 2s one rifle squad of team 0 is given a capture order for every sector in turn: BATTLE_SECTOR_AUDIT counts the orders
//     refused, and for each refused sector whether its middle is off the navigation mesh although the sector has ground to stand
//     on (the order snaps to the middle, so such a sector cannot be ordered at all).
//  2. Then up to eight other rifle squads get a capture order each for a different sector on their own side (houses, fields, a
//     wood, a junction). At 15s BATTLE_SECTOR_PLAN reports per squad how many arrival places lie inside the ordered sector.
//  3. At 240s BATTLE_SECTOR_STATE reports per squad how many living members stand in the sector and whether the order has
//     become hold. BATTLE_SECTOR_AUDIT_RESULT counts the failures.
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "NavigationSystem.h"
#include "Misc/CommandLine.h"

namespace
{
 struct FSectorAuditRow { int32 Squad=INDEX_NONE; FIntPoint Cell=FIntPoint::ZeroValue; bool bAccepted=false; int32 Inside=0,Outside=0,Members=0; };
 TArray<FSectorAuditRow> SectorAuditRows;
 int32 SectorAuditState=0,SectorAuditFailures=0; // 0 not started, 1 orders given, 2 plans read, 3 reported
 float SectorAuditClock=0;
}

void BattleSectorAudit(UBattleMatch& Match,float DeltaSeconds)
{
 static const bool bTest=FParse::Param(FCommandLine::Get(),TEXT("BattleSectorAudit"));
 if(!bTest||SectorAuditState==3)return;
 SectorAuditClock+=DeltaSeconds;if(SectorAuditClock<1.f)return;
 SectorAuditClock=0;
 UWorld* World=Match.GetWorld();
 auto* Runtime=World->GetSubsystem<UBattleLeaderRuntime>();auto* Store=World->GetSubsystem<UBattleState>();
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
 if(!Runtime||!Runtime->IsWideField()||!Nav||!BattleFieldLayout::Of(World).bSectors)return;
 if(SectorAuditState==0&&Match.Elapsed>=2)
 {
  SectorAuditState=1;
  TArray<int32> IDs;for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==0&&Pair.Value.SquadType==0&&Pair.Value.CommandStatus==EBattleCommandStatus::Active&&!Runtime->IsVehicle(Pair.Value.LeaderUnitID))IDs.Add(Pair.Key);
  IDs.Sort();
  if(IDs.IsEmpty()){++SectorAuditFailures;return;}
  // 1. Every sector, one squad.
  int32 Accepted=0,Refused=0,MiddleBlocked=0,NoGround=0;TMap<TCHAR,int32> RefusedByKind;
  for(int32 X=0;X<BattleSectors::Columns;++X)for(int32 Y=0;Y<BattleSectors::Rows;++Y)
  {
   const FIntPoint Cell(X,Y);const FVector Middle=BattleSectors::Center(Cell);const TCHAR Kind=TCHAR(BattleFieldLayout::SectorKind(World,Cell));
   if(Runtime->IssueOrder(IDs[0],EBattleOrder::AttackMove,Middle,false,true)){++Accepted;continue;}
   ++Refused;++RefusedByKind.FindOrAdd(Kind);
   FVector Lifted=Middle;BattleFieldLayout::Of(World).Heights.Ground(Lifted.X,Lifted.Y,0,Lifted.Z,nullptr,1.e9);
   FNavLocation At;const bool bMiddle=Nav->ProjectPointToNavigation(Lifted,At,FVector(100,100,400));
   int32 Ground=0;
   for(double DX=-2000;DX<=2000;DX+=1000)for(double DY=-2000;DY<=2000;DY+=1000)Ground+=Nav->ProjectPointToNavigation(Middle+FVector(DX,DY,0),At,FVector(60,60,200));
   if(!bMiddle&&Ground>0)++MiddleBlocked;else if(!Ground)++NoGround;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_AUDIT_REFUSED cell=%c%d kind=%c x_m=%.0f y_m=%.0f middle_on_nav=%d ground_samples=%d/25"),TCHAR('A'+X),1+Y,Kind,Middle.X/100,Middle.Y/100,bMiddle?1:0,Ground);
  }
  FString Kinds;for(const auto& K:RefusedByKind)Kinds+=FString::Printf(TEXT("%c:%d "),K.Key,K.Value);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_AUDIT cells=%d accepted=%d refused=%d middle_blocked=%d no_ground=%d refused_by_kind=%s"),BattleSectors::Columns*BattleSectors::Rows,Accepted,Refused,MiddleBlocked,NoGround,*Kinds);
  // 2. One sector each for up to eight squads, on the west side (team 0's own): houses, a junction, fields, woods.
  // Map J (2026-11-22): (7,3) is a mountain there; the pass (6,3) instead.
  const bool bMapJ=FCString::Strcmp(BattleFieldLayout::Of(World).Name,TEXT("Lvl_BattleWide_J"))==0;
  // Map K (2026-11-22): the town is columns 4..15, rows 3..10; the rest is mountain.
  const bool bMapK=FCString::Strcmp(BattleFieldLayout::Of(World).Name,TEXT("Lvl_BattleWide_K"))==0;
  const FIntPoint CellsK[8]={FIntPoint(5,6),FIntPoint(6,7),FIntPoint(4,5),FIntPoint(8,7),FIntPoint(5,9),FIntPoint(7,3),FIntPoint(6,4),FIntPoint(4,8)};
  const FIntPoint CellsIJ[8]={FIntPoint(5,6),FIntPoint(6,7),FIntPoint(4,5),FIntPoint(8,7),FIntPoint(3,9),bMapJ?FIntPoint(6,3):FIntPoint(7,3),FIntPoint(5,1),FIntPoint(2,6)};
  const FIntPoint* Cells=bMapK?CellsK:CellsIJ;
  SectorAuditRows.Reset();
  for(int32 I=0;I<8&&I+1<IDs.Num();++I)
  {
   FSectorAuditRow Row;Row.Squad=IDs[I+1];Row.Cell=Cells[I];
   Row.bAccepted=Runtime->IssueOrder(Row.Squad,EBattleOrder::AttackMove,BattleSectors::Center(Row.Cell),false,true);
   SectorAuditRows.Add(Row);
  }
  // The first squad gets a point order (not a sector) so that it does not wander with the last audited sector: it walks
  // 60m behind its starting place, out of the way of the others. (Left standing at the start it blocked the squad sent to F2:
  // with squads of eight that squad did not move for 240s, "rifle8" session 2026-11-19.) Hold where it stands if that is refused.
  {
   const FVector At=Runtime->Position(Store->FindSquad(IDs[0])->LeaderUnitID);
   const FVector Behind(FMath::Max(At.X-6000.,-BattleFieldLayout::Of(World).HalfExtent.X+1500.),At.Y,0);
   if(!Runtime->IssueOrder(IDs[0],EBattleOrder::Move,Behind))Runtime->IssueOrder(IDs[0],EBattleOrder::Hold,At);
  }
  return;
 }
 if(SectorAuditState==1&&Match.Elapsed>=15)
 {
  SectorAuditState=2;
  for(auto& Row:SectorAuditRows)
  {
   const auto* S=Store->FindSquad(Row.Squad);const auto* Plan=Runtime->PlanOf(Row.Squad);
   if(Plan)for(const auto& Place:Plan->ArrivalGoals){if(BattleSectors::Cell(Place.Value)==Row.Cell)++Row.Inside;else ++Row.Outside;}
   const bool bOK=Row.bAccepted&&S&&S->bSectorMission&&Row.Inside>0&&Row.Outside==0;SectorAuditFailures+=!bOK;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_PLAN squad=%d cell=%c%d kind=%c accepted=%d sector_mission=%d places_inside=%d places_outside=%d ok=%d"),Row.Squad,TCHAR('A'+Row.Cell.X),1+Row.Cell.Y,TCHAR(BattleFieldLayout::SectorKind(World,Row.Cell)),Row.bAccepted?1:0,S&&S->bSectorMission?1:0,Row.Inside,Row.Outside,bOK?1:0);
  }
  return;
 }
 if(SectorAuditState==2&&Match.Elapsed>=240)
 {
  SectorAuditState=3;
  for(auto& Row:SectorAuditRows)
  {
   const auto* S=Store->FindSquad(Row.Squad);if(!S)continue;
   int32 Living=0,Present=0;
   for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()){++Living;Present+=BattleSectors::Cell(Runtime->Position(ID))==Row.Cell;}
   const bool bHold=S->Order==EBattleOrder::Defend;
   // Reached and holding: most of the living members in the sector and the order turned to hold. A squad shot to pieces on
   // the way is reported, not counted as a failure of the order.
   const bool bOK=!Living||(Present*2>Living&&bHold&&S->bSectorMission);SectorAuditFailures+=!bOK;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_STATE squad=%d cell=%c%d kind=%c living=%d present=%d order=%d hold=%d sector_mission=%d status=%d ok=%d"),Row.Squad,TCHAR('A'+Row.Cell.X),1+Row.Cell.Y,TCHAR(BattleFieldLayout::SectorKind(World,Row.Cell)),Living,Present,int32(S->Order),bHold?1:0,S->bSectorMission?1:0,int32(S->CommandStatus),bOK?1:0);
  }
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_AUDIT_RESULT failures=%d"),SectorAuditFailures);
 }
}
