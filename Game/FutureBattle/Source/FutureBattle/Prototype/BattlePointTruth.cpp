// User-authorized exception: only opposing infantry co-occupying an objective/home sector reveals truth.
#include "BattlePerception.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

int32 BattleTactics::PointTruth()
{
 static const int32 Mode=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattlePointTruth="),Value);return FMath::Clamp(Value,0,1);}();return Mode;
}
int32 BattleTactics::PointTruthTeam()
{
 static const int32 Team=[](){int32 Value=-1;FParse::Value(FCommandLine::Get(),TEXT("BattlePointTruthTeam="),Value);return FMath::Clamp(Value,-1,1);}();return Team;
}

void UBattlePerception::ResetPointTruth()
{
 PointTruthCells.Reset();PointTruthUnits.Reset();PointTruthWrites.Reset();PointTruthAt=-1.;
}

void UBattlePerception::ClearPointTruthVisibility()
{
 // Demote only our previous writes before ordinary sensing. A fresh real sighting can replace their provenance.
 for(const FIntPoint& WrittenPair:PointTruthWrites)
  if(auto* PointObserver=Observers.Find(WrittenPair.X))
   if(auto* PointContact=PointObserver->Contacts.Find(WrittenPair.Y);PointContact && PointContact->bPointTruth)
   {PointContact->bVisible=false;PointContact->bTracked=false;}
 PointTruthWrites.Reset();
}

void UBattlePerception::UpdatePointTruth(double Now)
{
 if(!BattleTactics::PointTruth())return;
 const auto* PointMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 const auto* PointRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* PointStore=GetWorld()->GetSubsystem<UBattleState>();
 auto* PointTactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 if(!PointMatch->bArea || !BattleFieldLayout::Of(GetWorld()).bSectors){ResetPointTruth();return;}
 if(PointTruthAt<0. || Now<PointTruthAt || Now-PointTruthAt>=.5)
 {
  TMap<FIntPoint,FPointTruthCell> PointNext;
  // Objectives have stable public positions. Duplicate objectives/homes in a cell share one proximity state.
  for(int32 PointIndex=0;PointIndex<PointMatch->Objectives.Num();++PointIndex)
  {
   const FIntPoint ObjectiveCell=BattleSectors::Cell(PointMatch->Objectives[PointIndex].Position);
   if(BattleSectors::Valid(ObjectiveCell) && !PointNext.Contains(ObjectiveCell))PointNext.FindOrAdd(ObjectiveCell).Point=PointIndex;
  }
  for(int32 HomeTeam=0;HomeTeam<2;++HomeTeam)
  {
   const FIntPoint HomeCell=BattleSectors::Cell(PointMatch->AreaHome(HomeTeam));
   if(BattleSectors::Valid(HomeCell) && !PointNext.Contains(HomeCell))PointNext.FindOrAdd(HomeCell).Point=-1-HomeTeam;
  }
  PointTruthUnits.Reset();
  // The ONLY additional world-position scan: once per .5 s, one pass over living bodies, no per-observer global search.
  for(const auto& UnitPair:PointStore->GetUnits())
  {
   const auto& PointUnit=UnitPair.Value;
   if(!PointUnit.IsAlive() || PointUnit.Team<0 || PointUnit.Team>1 || !PointRuntime->GetBodies().Contains(UnitPair.Key))continue;
   const FVector PointPosition=PointRuntime->Position(UnitPair.Key);
   auto* PointBucket=PointNext.Find(BattleSectors::Cell(PointPosition));if(!PointBucket)continue;
   const auto* PointSquad=PointStore->FindSquad(PointUnit.SquadID);
   FPointTruthUnit Sample;Sample.ID=UnitPair.Key;Sample.Squad=PointUnit.SquadID;Sample.Team=PointUnit.Team;
   Sample.At=PointPosition;Sample.Type=PointSquad?PointSquad->SquadType:255;Sample.Memory=PointUnit.Perception.MemorySeconds;
   PointTruthUnits.Add(Sample.ID,Sample);PointBucket->Units[Sample.Team].Add(Sample.ID);
   // Match the runtime's individual-body distinction: a dismounted crewman is infantry, the gun/vehicle is not.
   if(!PointRuntime->IsVehicle(Sample.ID) && !PointRuntime->IsArtillery(Sample.ID))PointBucket->Infantry[Sample.Team].Add(Sample.ID);
  }
  TArray<FVector> PointSpots[2];
  for(auto& CellPair:PointNext)
  {
   auto& NextCell=CellPair.Value;const auto* OldCell=PointTruthCells.Find(CellPair.Key);
   const bool WasClose=OldCell && OldCell->Since>=0.;
   const bool IsClose=!NextCell.Infantry[0].IsEmpty() && !NextCell.Infantry[1].IsEmpty();
   NextCell.Since=IsClose?(WasClose?OldCell->Since:Now):-1.;
   if(WasClose!=IsClose)
   {
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_POINT_TRUTH t=%.0f point=%d cell=%d,%d what=%s own0=%d own1=%d secs=%.0f"),
     PointMatch->Elapsed,NextCell.Point,CellPair.Key.X,CellPair.Key.Y,IsClose?TEXT("on"):TEXT("off"),
     NextCell.Infantry[0].Num(),NextCell.Infantry[1].Num(),IsClose?0.:Now-OldCell->Since);
    if(IsClose)PointTactics->Event(TEXT("PointTruthOn"));
   }
   if(!IsClose)continue;
   for(int32 ReceiverTeam=0;ReceiverTeam<2;++ReceiverTeam)
   {
    if(BattleTactics::PointTruthTeam()>=0 && BattleTactics::PointTruthTeam()!=ReceiverTeam)continue;
    for(int32 EnemyID:NextCell.Units[1-ReceiverTeam])PointSpots[ReceiverTeam].Add(PointTruthUnits.FindChecked(EnemyID).At);
   }
  }
  PointTruthCells=MoveTemp(PointNext);PointTruthAt=Now;
  for(int32 SpotTeam=0;SpotTeam<2;++SpotTeam)PointTactics->InjectPointTruthSpots(SpotTeam,PointSpots[SpotTeam]);
 }
 // Reapply the bounded snapshot after ordinary scans (which clear bVisible). Never refresh its acquisition timestamp here.
 // Work is limited to local observer/target output pairs; no world scan or position query occurs below.
 for(const auto& CellPair:PointTruthCells)
 {
  const auto& CloseCell=CellPair.Value;if(CloseCell.Since<0.)continue;
  for(int32 ReceiverTeam=0;ReceiverTeam<2;++ReceiverTeam)
  {
   if(BattleTactics::PointTruthTeam()>=0 && BattleTactics::PointTruthTeam()!=ReceiverTeam)continue;
   for(int32 ReceiverID:CloseCell.Units[ReceiverTeam])
   {
    const auto* ReceiverState=PointStore->Find(ReceiverID);if(!ReceiverState || !ReceiverState->IsAlive())continue;
    const auto& ReceiverSample=PointTruthUnits.FindChecked(ReceiverID);
    auto& ReceiverContacts=Observers.FindOrAdd(ReceiverID).Contacts;
    for(int32 EnemyID:CloseCell.Units[1-ReceiverTeam])
    {
     const auto* EnemyState=PointStore->Find(EnemyID);if(!EnemyState || !EnemyState->IsAlive())continue;
     const auto& EnemySample=PointTruthUnits.FindChecked(EnemyID);
     if(const auto* Existing=ReceiverContacts.Find(EnemyID);Existing && !Existing->bPointTruth && Existing->LastSeenTime>=PointTruthAt && Existing->bVisible)continue;
     FBattleContact PointContact;PointContact.TargetID=EnemyID;PointContact.ObservedSquadType=EnemySample.Type;
     PointContact.LastKnownPosition=EnemySample.At;PointContact.LastSeenTime=PointTruthAt;PointContact.Confidence=1.f;
     PointContact.bVisible=true;PointContact.SourceUnitID=ReceiverID;PointContact.bShared=false;
     PointContact.bLongRangeObservation=false;PointContact.bLocalShareEligible=true;PointContact.bPointTruth=true;
     PointContact.KnownExpiresAt=PointTruthAt+ReceiverSample.Memory;PointContact.KnownSourceSquad=ReceiverSample.Squad;
     ReceiverContacts.Add(EnemyID,PointContact);PointTruthWrites.Add(FIntPoint(ReceiverID,EnemyID));
    }
   }
  }
 }
}

void UBattleSquadTactics::InjectPointTruthSpots(int32 Team,const TArray<FVector>& Positions)
{
 if(!BattleTactics::PointTruth() || Team<0 || Team>1 || (BattleTactics::PointTruthTeam()>=0 && BattleTactics::PointTruthTeam()!=Team))return;
 // Same insertion and expiry contract as SpotTruth bit 1; do not touch its switch, scan or diagnostic totals.
 // Equal tactical timestamps may occur with squad tactics disabled; use the existing last-write-wins path.
 const double PointNow=Clock();int32 PointWrites=0;
 for(const FVector& PointAt:Positions)if(AddKnownSpot(Team,PointAt,PointNow,5,true))++PointWrites;
 if(PointWrites)Event(TEXT("PointTruthSpots"),PointWrites);
 for(auto PointIt=KnownSpots[Team].CreateIterator();PointIt;++PointIt)
  if(PointNow<PointIt->Value.Seen || PointNow-PointIt->Value.Seen>BattleTactics::SeekKnownSeconds())PointIt.RemoveCurrent();
}
