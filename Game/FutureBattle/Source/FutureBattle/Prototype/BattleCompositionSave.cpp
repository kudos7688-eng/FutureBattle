#include "BattleComposition.h"
#include "BattleSave.h"
#include "UObject/Class.h"

bool BattleComposition::ValidateSave(const UBattleSave& Save,FString& Error)
{
 auto Fail=[&](const TCHAR* Message){Error=Message;return false;};
 if(Save.CombatStateVersion<4)
 {
  if(Save.CompositionTeams!=0||Save.CompositionPolicy!=0||!Save.CompositionRules.IsEmpty()||!Save.CompositionState.IsEmpty())
   return Fail(TEXT("Composition state requires combat extension 4"));
  return true;
 }
 if(Save.Version!=3||Save.CompositionTeams<1||Save.CompositionTeams>3||Save.CompositionPolicy!=PolicyVersion||Save.CompositionState.Num()!=2||Save.CompositionRules.Len()!=32)
  return Fail(TEXT("Invalid composition header"));
 for(TCHAR Character:Save.CompositionRules)if(!FChar::IsHexDigit(Character))return Fail(TEXT("Invalid composition rules key"));
 if(!FMath::IsFinite(Save.Elapsed)||Save.Elapsed<0||Save.Elapsed>BattleSaveValidation::ClockLimit(Save.MapName))return Fail(TEXT("Invalid composition clock"));
 const int32 NowMS=FMath::FloorToInt(Save.Elapsed*1000.f);
 TMap<int32,int32> UnitTeams;for(const auto& Unit:Save.Units)UnitTeams.Add(Unit.State.UnitID,Unit.State.Team);
 for(int32 Team=0;Team<2;++Team)
 {
  const auto& State=Save.CompositionState[Team];
  if(!ValidateState(State,NowMS,Save.Objectives.Num(),Error))return false;
  if((Save.CompositionTeams&(1<<Team))==0)
  {
   const FCompositionTeamState Empty;
   if(!FCompositionTeamState::StaticStruct()->CompareScriptStruct(&State,&Empty,0))return Fail(TEXT("Inactive team has composition history"));
  }
  for(const auto& Emergency:State.EmergencyUntilMS)
  {
   const auto* Defender=Save.Squads.FindByPredicate([&](const FBattleSquadState& Squad){return Squad.SquadID==Emergency.Key;});
   const int32* Purpose=Save.Area.HirePurpose.Find(Emergency.Key);
   if(!Defender||Defender->Team!=Team||!Purpose||*Purpose!=101)return Fail(TEXT("Invalid planner emergency purpose reference"));
  }
  int32 Purchases=0;for(int32 Count:State.Purchased)Purchases+=Count;
  int32 OwnRecords=0;for(const auto& Squad:Save.Squads)OwnRecords+=Squad.Team==Team;
  if(Purchases>OwnRecords)return Fail(TEXT("Composition purchase history exceeds own records"));
  for(const auto& Observation:State.VehicleSeenMS)
  {
   const int32* ObservedTeam=UnitTeams.Find(Observation.Key);
   if(!ObservedTeam||*ObservedTeam==Team)return Fail(TEXT("Invalid composition observation reference"));
  }
 }
 return true;
}
