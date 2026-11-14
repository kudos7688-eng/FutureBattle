#include "BattleSave.h"
#include "BattleSquadTactics.h"
#include "BattleTeamAI.h"
void UBattleSquadTactics::CaptureSave(UBattleSave& Save,double Now) const
{
 Save.TacticalClock=ActiveSeconds;Save.TacticalSamples=Samples;Save.NextSamples=NextSample;
 for(auto& O:Save.TacticalSamples)for(auto& C:O.Value.Contacts)C.Value.LastSeenTime-=Now;
 Save.DamageBuckets.Reset();
 for(const auto& H:DamageHistory)for(const auto& D:H.Value)if(ActiveSeconds-D.Key<=6)
 {FBattleSavedDamage Entry;Entry.SquadID=H.Key;Entry.Bucket=D.Key;Entry.Damage=D.Value;Save.DamageBuckets.Add(Entry);}
}
void UBattleSquadTactics::RestoreSave(const UBattleSave& Save,double Now)
{
 Reset();ActiveSeconds=Save.TacticalClock;NextDump=ActiveSeconds+5;Samples=Save.TacticalSamples;NextSample=Save.NextSamples;
 for(auto& O:Samples)for(auto& C:O.Value.Contacts){C.Value.LastSeenTime+=Now;C.Value.bVisible=false;}
 for(const auto& D:Save.DamageBuckets)DamageHistory.FindOrAdd(D.SquadID).Emplace(D.Bucket,D.Damage);
 // Measurement counters start a new segment; gameplay timers/knowledge are restored.
}
void UBattleTeamAI::CaptureSave(UBattleSave& Save) const
{Save.TeamNextAssignment=NextAssignment;Save.TeamRetryAfter=RetryAfter;}
void UBattleTeamAI::RestoreSave(const UBattleSave& Save)
{NextAssignment=Save.TeamNextAssignment;RetryAfter=Save.TeamRetryAfter;}

