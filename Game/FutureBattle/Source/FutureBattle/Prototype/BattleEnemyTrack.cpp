#include "BattleEnemyTrack.h"
#include "BattleController.h"
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

float BattleEnemyTrack::TrackSeconds()
{
 static const float Seconds=[]{float Value=15.f;FParse::Value(FCommandLine::Get(),TEXT("BattleEnemyMapTrack="),Value);return FMath::Clamp(Value,0.f,60.f);}();
 return Seconds;
}
float BattleEnemyTrack::KeepSeconds()
{
 static const float Seconds=[]{float Value=60.f;FParse::Value(FCommandLine::Get(),TEXT("BattleEnemyMapKeep="),Value);Value=FMath::Clamp(Value,0.f,600.f);return Value==0.f?0.f:FMath::Max(Value,TrackSeconds());}();
 return Seconds;
}
int32 BattleEnemyTrack::StateAt(double Age,float Track,float Keep)
{
 if(Keep<=0.f)return -1;
 const double TrackLimit=FMath::Max(0.f,Track),KeepLimit=FMath::Max(double(Keep),TrackLimit);
 if(Age>KeepLimit)return -1;
 return TrackLimit>0. && Age<=TrackLimit?1:2;
}
 TMap<int32,FBattleSeenEnemySquad> SeenEnemySquads(const UWorld* World)
 {
  TMap<int32,FBattleSeenEnemySquad> Out; TSet<int32> Counted;
  const auto* Store=World->GetSubsystem<UBattleState>(); const auto* Sense=World->GetSubsystem<UBattlePerception>(); const auto* Match=World->GetSubsystem<UBattleMatch>();
  for(const auto& Pair:Store->GetUnits()) if(Pair.Value.Team==0&&Pair.Value.IsAlive())
   if(const auto* Contacts=Sense->GetContacts(Pair.Key)) for(const auto& C:*Contacts)
   {
    if(!C.Value.bVisible||(Match->bArea&&Match->IsScoutLongContact(Pair.Key,C.Value))) continue;
    const auto* Target=Store->Find(C.Key); if(!Target||!Target->IsAlive()||Target->Team==0) continue;
    auto& G=Out.FindOrAdd(Target->SquadID);
    if(C.Value.ObservedSquadType<BattleSquadType::Count) G.Type=C.Value.ObservedSquadType;
    if(Counted.Contains(C.Key)) continue; Counted.Add(C.Key);
    G.Sum+=C.Value.LastKnownPosition; ++G.Count; G.Soldiers.Add(C.Value.LastKnownPosition);
   }
  return Out;
 }

void ABattleController::UpdateEnemyTrack()
{
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const float Keep=BattleEnemyTrack::KeepSeconds();if(!Runtime->IsEnabled()||Keep<=0.f)return;
 const double Now=GetWorld()->GetTimeSeconds();const float Track=BattleEnemyTrack::TrackSeconds();
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(auto It=EnemyTrackDestroyed.CreateIterator();It;++It)if(Now-It.Value()>60.)It.RemoveCurrent();
 // All reads of true membership and position are confined to seen/tracked rows.
 auto HasLiving=[&](int32 EnemyID)
 {
  if(const auto* Squad=Store->FindSquad(EnemyID))for(int32 MemberID:Squad->Members)
   if(const auto* Member=Store->Find(MemberID);Member&&Member->IsAlive())return true;
  return false;
 };
 const auto Seen=SeenEnemySquads(GetWorld());
 for(const auto& Observed:Seen)
 {
  auto& Entry=EnemyTrack.FindOrAdd(Observed.Key);
  if(Entry.State==2)EnemyLongestRemembered=FMath::Max(EnemyLongestRemembered,FMath::Min(Now-Entry.LastSeenAt,double(Keep))-Track);
  Entry.SquadID=Observed.Key;Entry.State=0;Entry.Where=Observed.Value.Sum/Observed.Value.Count;
  Entry.Count=Observed.Value.Count;Entry.Soldiers=Observed.Value.Soldiers;Entry.LastSeenAt=Now;
  if(Observed.Value.Type<BattleSquadType::Count)Entry.Type=Observed.Value.Type;
 }
 for(auto It=EnemyTrack.CreateIterator();It;++It)
 {
  auto& Entry=It.Value();const double Age=Now-Entry.LastSeenAt;
  if(Seen.Contains(It.Key()))
  {
   if(!HasLiving(It.Key())){EnemyTrackDestroyed.Add(It.Key(),Now);It.RemoveCurrent();}
   continue;
  }
  // A previously visible row may disappear from Seen because its last soldier just died.
  // State 2 never enters this check, even if its actual squad has since been destroyed.
  if(Entry.State==0&&!HasLiving(It.Key())){EnemyTrackDestroyed.Add(It.Key(),Now);It.RemoveCurrent();continue;}
  const int32 NextState=BattleEnemyTrack::StateAt(Age,Track,Keep);
  // Only the approved tracking window reads current positions.
  if(NextState==1)
  {
   if(!HasLiving(It.Key())){EnemyTrackDestroyed.Add(It.Key(),Now);It.RemoveCurrent();continue;}
   FVector Sum=FVector::ZeroVector;Entry.Soldiers.Reset();
   if(const auto* Squad=Store->FindSquad(It.Key()))for(int32 MemberID:Squad->Members)
    if(const auto* Member=Store->Find(MemberID);Member&&Member->IsAlive())
    {const FVector At=Runtime->Position(MemberID);if(!At.IsNearlyZero()){Sum+=At;Entry.Soldiers.Add(At);}}
   if(!Entry.Soldiers.IsEmpty())Entry.Where=Sum/Entry.Soldiers.Num();
  }
  if(Entry.State==2)EnemyLongestRemembered=FMath::Max(EnemyLongestRemembered,FMath::Min(Age,double(Keep))-Track);
  if(NextState<0){It.RemoveCurrent();continue;}
  if(NextState!=Entry.State){if(NextState==1)++EnemyTrackedEntries;else ++EnemyRememberedEntries;}
  Entry.State=uint8(NextState);
  if(Entry.State==2)EnemyLongestRemembered=FMath::Max(EnemyLongestRemembered,Age-Track);
 }
}
