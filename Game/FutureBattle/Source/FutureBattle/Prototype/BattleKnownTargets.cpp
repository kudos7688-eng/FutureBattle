// Team observation index, built once after perception applies its results. No enemy truth reads.
#include "BattleKnownTargets.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool BattleKnownFire::Enabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleKnownTargetFire="),V);return V==1 || V==2;}();
 return On;
}

void UBattlePerception::BuildTeamKnownTargets(double Now)
{
 if(!BattleKnownFire::Enabled())return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 BattleKnownFire::FTeam Next[2];
 auto Add=[&](int32 Team,const FBattleContact& Contact,double MemoryUntil)
 {
  // Anonymous hints and unreported long-range scout sightings cannot acquire an identity here.
  if(Team<0||Team>1||Contact.TargetID==INDEX_NONE||Contact.bLongRangeObservation||Contact.KnownExpiresAt<=0)return;
  BattleKnownFire::FTarget Entry;
  Entry.TargetID=Contact.TargetID;Entry.ObservedSquadType=Contact.ObservedSquadType;
  Entry.LastKnownPosition=Contact.LastKnownPosition;Entry.ObservedAt=Contact.LastSeenTime;
  Entry.SourceUnitID=Contact.SourceUnitID;Entry.SourceSquadID=Contact.KnownSourceSquad;Entry.SourceKind=Contact.bShared?1:0;
  Entry.ExpiresAt=FMath::Min(Contact.KnownExpiresAt,MemoryUntil);Entry.ReceivedAt=Now;
  if(Contact.bTracked && !Contact.bShared && BattleTactics::SpotTrack()>0)
  {Entry.TrackedPosition=Contact.TrackedPosition;Entry.TrackSampleAt=Contact.KnownTrackSampleAt;Entry.TrackUntil=FMath::Min(Entry.ExpiresAt,Contact.LastSeenTime+BattleTactics::SpotTrack());}
  if(!Entry.Fresh(Now)){GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->SightExperiment.Cover(TEXT("known_fire_reject_stale"));return;}
  const auto* Previous=Next[Team].Targets.Find(Entry.TargetID);
  if(Previous && (Previous->ObservedAt>Entry.ObservedAt || (Previous->ObservedAt==Entry.ObservedAt &&
   (Previous->SourceKind<Entry.SourceKind || (Previous->SourceKind==Entry.SourceKind && Previous->SourceUnitID<Entry.SourceUnitID)))))return;
  if(Previous && Previous->ObservedAt==Entry.ObservedAt && Previous->SourceKind==Entry.SourceKind && Previous->SourceUnitID==Entry.SourceUnitID && Previous->ExpiresAt>=Entry.ExpiresAt)return;
  Next[Team].Targets.Add(Entry.TargetID,Entry);
 };
 TArray<int32> ObserverIDs;Observers.GetKeys(ObserverIDs);ObserverIDs.Sort();
 for(int32 ObserverID:ObserverIDs)if(const auto* Unit=Store->Find(ObserverID);Unit && Unit->IsAlive())
  for(const auto& Pair:Observers.FindChecked(ObserverID).Contacts)Add(Unit->Team,Pair.Value,Pair.Value.LastSeenTime+Unit->Perception.MemorySeconds);
 // Delivered squad contacts have IDs too. Pending report queues are intentionally not visited.
 TArray<int32> SquadIDs;SquadIntel.GetKeys(SquadIDs);SquadIDs.Sort();
 for(int32 SquadID:SquadIDs)if(const auto* Squad=Store->FindSquad(SquadID))
  for(const auto& Pair:SquadIntel.FindChecked(SquadID).Known)Add(Squad->Team,Pair.Value.Contact,Pair.Value.Contact.KnownExpiresAt);
 for(int32 Team=0;Team<2;++Team)
 {
  auto& Old=TeamKnownTargets[Team];auto& Fresh=Next[Team];Fresh.Revision=Old.Revision;
  TArray<int32> TargetIDs;Fresh.Targets.GetKeys(TargetIDs);TargetIDs.Sort();
  for(int32 TargetID:TargetIDs)
  {
   auto& Entry=Fresh.Targets.FindChecked(TargetID);const auto* Previous=Old.Targets.Find(TargetID);
   if(Previous && Previous->ObservedAt==Entry.ObservedAt && Previous->SourceUnitID==Entry.SourceUnitID && Previous->SourceKind==Entry.SourceKind)
    Entry.ReceivedAt=Previous->ReceivedAt;
   const bool Same=Previous && Previous->ObservedAt==Entry.ObservedAt && Previous->SourceUnitID==Entry.SourceUnitID && Previous->SourceKind==Entry.SourceKind &&
    Previous->ObservedSquadType==Entry.ObservedSquadType && Previous->SourceSquadID==Entry.SourceSquadID && Previous->ExpiresAt==Entry.ExpiresAt && Previous->TrackUntil==Entry.TrackUntil && Previous->TrackSampleAt==Entry.TrackSampleAt &&
    Previous->LastKnownPosition==Entry.LastKnownPosition && Previous->TrackedPosition==Entry.TrackedPosition;
   Entry.Revision=Same?Previous->Revision:++Fresh.Revision;
   const FVector Point=Entry.At(Now);Fresh.Grid.FindOrAdd(FIntPoint(FMath::FloorToInt(Point.X/5000.),FMath::FloorToInt(Point.Y/5000.))).Add(TargetID);
  }
  Old=MoveTemp(Fresh);
 }
}
