#include "BattleLeaderRuntime.h"
#include "BattleComposition.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleGameMode.h"
#include "Engine/World.h"
#include "UObject/Class.h"
#include "Kismet/GameplayStatics.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunCompositionSmoke()
{
 auto CompositionCheck=[&](bool Passed,const TCHAR* Label){if(!Passed)++SmokeFailures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_CHECK %s %s"),Passed?TEXT("PASS"):TEXT("FAIL"),Label);};
 // Fixed inputs deliberately do not depend on ini prices or command-line Keep overrides.
 TArray<BattleComposition::FTypeDef> CompositionDefs=BattleComposition::Definitions();
 for(auto& CompositionDef:CompositionDefs){CompositionDef.HardActiveLimit=10;CompositionDef.Cost=12;}
 CompositionDefs[BattleSquadType::Tank].Cost=40;CompositionDefs[BattleSquadType::Heavy].Cost=15;CompositionDefs[BattleSquadType::Scout].Cost=8;
 BattleComposition::FInput CompositionInput;
 CompositionInput.Team=1;CompositionInput.bAutoAllowed=true;CompositionInput.bHireEnabled=true;CompositionInput.MaxActive=10;
 CompositionInput.Active=5;CompositionInput.Records=5;CompositionInput.FrontMinimum=3;CompositionInput.FrontEffective=5000;
 CompositionInput.StopAtMS=600000;CompositionInput.ArrivalDeadlineMS=600000;CompositionInput.NetPerMinute=60;CompositionInput.HeavyShare=.33f;
 CompositionInput.Types[0].Effective=6000;CompositionInput.Types[1].Effective=3000;
 CompositionInput.RoleEffective[0]=6000;CompositionInput.RoleEffective[1]=3000;
 CompositionInput.Types[BattleSquadType::Tank].PacedTarget=1;
 for(int32& CompositionArrival:CompositionInput.ArrivalMS)CompositionArrival=5000;
 FCompositionTeamState CompositionInitial;
 auto CompositionSaved=BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionInitial);
 CompositionCheck(CompositionSaved.Action==BattleComposition::EAction::Save&&CompositionSaved.Type==BattleSquadType::Tank&&CompositionSaved.Next.SaveDeadlineMS==60000,TEXT("planner saves for 40-cost tank from zero free resources"));
 FCompositionTeamState CompositionProgress=CompositionSaved.Next;
 for(int32 CompositionFree:{23,24,39})
 {
  CompositionInput.NowMS+=5000;CompositionInput.Free=CompositionFree;
  const auto CompositionStep=BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionProgress);
  CompositionCheck(CompositionStep.Action==BattleComposition::EAction::Save&&CompositionStep.Type==BattleSquadType::Tank&&CompositionStep.Next.SaveDeadlineMS==60000,TEXT("planner holds saving goal across 60 percent boundary without extending deadline"));
  CompositionProgress=CompositionStep.Next;
 }
 CompositionInput.Free=40;CompositionInput.NowMS+=5000;
 const auto CompositionBuy=BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionProgress);
 CompositionCheck(CompositionBuy.Action==BattleComposition::EAction::Buy&&CompositionBuy.Type==BattleSquadType::Tank,TEXT("planner buys once budget reaches cost"));
 auto CompositionCompleted=CompositionBuy.Next;BattleComposition::CompletePurchase(CompositionCompleted,true,CompositionBuy.Type);
 CompositionCheck(CompositionCompleted.SaveType==-1&&CompositionCompleted.LastSuccess==CompositionCompleted.Serial&&CompositionCompleted.Purchased[4]==0,TEXT("planner completion clears goal; executor alone owns purchase accounting"));
 auto CompositionFailed=CompositionBuy.Next;BattleComposition::CompletePurchase(CompositionFailed,false,CompositionBuy.Type);
 const auto CompositionRetry1=BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionFailed);
 const auto CompositionRetry2=BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionRetry1.Next);
 const auto CompositionRetry3=BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionRetry2.Next);
 CompositionCheck(CompositionRetry1.Action==BattleComposition::EAction::Wait&&CompositionRetry2.Action==BattleComposition::EAction::Wait&&CompositionRetry3.Action==BattleComposition::EAction::Buy,TEXT("planner spawn failure waits two decisions without charging history"));

 auto CompositionNoIncome=CompositionInput;CompositionNoIncome.Free=23;CompositionNoIncome.NetPerMinute=0;
 const auto CompositionCancelled=BattleComposition::Decide(CompositionNoIncome,CompositionDefs,CompositionProgress);
 CompositionCheck(CompositionCancelled.Action==BattleComposition::EAction::Wait&&CompositionCancelled.Next.SaveType==-1,TEXT("planner cancels unreachable saving when income stops"));
 auto CompositionExpired=CompositionNoIncome;CompositionExpired.NetPerMinute=60;CompositionExpired.NowMS=60000;
 const auto CompositionExpiry=BattleComposition::Decide(CompositionExpired,CompositionDefs,CompositionProgress);
 CompositionCheck(CompositionExpiry.Action==BattleComposition::EAction::Wait&&CompositionExpiry.Next.SaveBlockedUntil[4]==CompositionExpiry.Next.Serial+3,TEXT("planner expires goal and blocks its saving restart for three decisions"));
 CompositionExpired.Free=40;
 CompositionCheck(BattleComposition::Decide(CompositionExpired,CompositionDefs,CompositionExpiry.Next).Action==BattleComposition::EAction::Buy,TEXT("planner expiry cooldown does not block an affordable purchase"));
 auto CompositionMissionLost=CompositionInput;CompositionMissionLost.bMissionRequired=true;CompositionMissionLost.Mission=-1;
 CompositionCheck(BattleComposition::Decide(CompositionMissionLost,CompositionDefs,CompositionProgress).Next.SaveType==-1,TEXT("planner cancels saving when required mission disappears"));

 auto CompositionEmergency=CompositionInput;CompositionEmergency.Free=12;CompositionEmergency.ObservedVehicles=1;
 CompositionEmergency.Types[BattleSquadType::Scout].PacedTarget=1;CompositionEmergency.Types[BattleSquadType::AntiTank].PacedTarget=1;
 const auto CompositionResponse=BattleComposition::Decide(CompositionEmergency,CompositionDefs,CompositionProgress);
 CompositionCheck(CompositionResponse.Action==BattleComposition::EAction::Buy&&CompositionResponse.Type==BattleSquadType::AntiTank,TEXT("planner observed armour interrupts saving and outranks scout deficit"));
 auto CompositionHomeEmergency=CompositionEmergency;CompositionHomeEmergency.Mission=101;CompositionHomeEmergency.bHomeThreat=true;
 const auto CompositionHomeResponse=BattleComposition::Decide(CompositionHomeEmergency,CompositionDefs,CompositionProgress);
 CompositionCheck(CompositionHomeResponse.Action==BattleComposition::EAction::Buy&&CompositionHomeResponse.Type==BattleSquadType::AntiTank&&CompositionHomeResponse.Mission==101,
  TEXT("planner mission change still prefers affordable emergency response"));
 CompositionEmergency.Types[BattleSquadType::AntiTank].PacedTarget=0;
 CompositionCheck(BattleComposition::Decide(CompositionEmergency,CompositionDefs,CompositionInitial).Type!=BattleSquadType::AntiTank,TEXT("planner urgency never overrides explicit zero target"));
 auto CompositionFront=CompositionInput;CompositionFront.Free=12;CompositionFront.FrontEffective=0;CompositionFront.Types[0].Effective=0;CompositionFront.RoleEffective[0]=0;
 CompositionCheck(BattleComposition::Decide(CompositionFront,CompositionDefs,CompositionProgress).Type==BattleSquadType::Rifle,TEXT("planner frontline collapse interrupts tank saving for affordable infantry"));
 auto CompositionHeavy=CompositionInput;CompositionHeavy.Free=100;CompositionHeavy.Types[1].Effective=0;CompositionHeavy.RoleEffective[1]=0;
 const auto CompositionBoth=BattleComposition::Decide(CompositionHeavy,CompositionDefs,CompositionInitial);
 bool CompositionHeavyCandidate=false,CompositionTankCandidate=false;
 for(const auto& CompositionCandidate:CompositionBoth.Candidates)
 {
  CompositionHeavyCandidate|=CompositionCandidate.Type==1&&CompositionCandidate.Excluded.IsEmpty();
  CompositionTankCandidate|=CompositionCandidate.Type==4&&CompositionCandidate.Excluded.IsEmpty();
 }
 CompositionCheck(CompositionHeavyCandidate&&CompositionTankCandidate,TEXT("planner considers heavy and tank deficits simultaneously"));
 auto CompositionSwitchInput=CompositionHeavy;CompositionSwitchInput.Free=10;CompositionSwitchInput.NowMS=20000;CompositionSwitchInput.Types[4].Effective=750;
 const auto CompositionSwitch1=BattleComposition::Decide(CompositionSwitchInput,CompositionDefs,CompositionSaved.Next);
 CompositionSwitchInput.NowMS=25000;
 const auto CompositionSwitch2=BattleComposition::Decide(CompositionSwitchInput,CompositionDefs,CompositionSwitch1.Next);
 CompositionCheck(CompositionSwitch1.Type==4&&CompositionSwitch1.Next.SwitchCount==1&&CompositionSwitch2.Type==1&&CompositionSwitch2.Action==BattleComposition::EAction::Save&&CompositionSwitch2.Next.SaveDeadlineMS==60000,
  TEXT("planner requires two better scores to switch and preserves original saving deadline"));
 TArray<BattleComposition::FTypeDef> CompositionReversed;
 for(int32 CompositionIndex=CompositionDefs.Num()-1;CompositionIndex>=0;--CompositionIndex)CompositionReversed.Add(CompositionDefs[CompositionIndex]);
 const auto CompositionReverseResult=BattleComposition::Decide(CompositionHeavy,CompositionReversed,CompositionInitial);
 CompositionCheck(CompositionBoth.Type==CompositionReverseResult.Type&&CompositionBoth.Reason==CompositionReverseResult.Reason,TEXT("planner candidate definition order cannot change deterministic choice"));
 for(int32 CompositionGate=0;CompositionGate<4;++CompositionGate)
 {
  auto CompositionBlocked=CompositionInput;
  if(CompositionGate==0)CompositionBlocked.Active=CompositionBlocked.MaxActive;
  if(CompositionGate==1)CompositionBlocked.Records=100;
  if(CompositionGate==2)CompositionBlocked.bAutoAllowed=false;
  if(CompositionGate==3)CompositionBlocked.NowMS=CompositionBlocked.StopAtMS;
  CompositionCheck(BattleComposition::Decide(CompositionBlocked,CompositionDefs,CompositionProgress).Action==BattleComposition::EAction::Wait,TEXT("planner respects total slots record capacity player ownership and stop time"));
 }
 auto CompositionTypeFull=CompositionInput;CompositionTypeFull.Types[4].Active=10;
 CompositionCheck(BattleComposition::Decide(CompositionTypeFull,CompositionDefs,CompositionProgress).Type!=4,TEXT("planner respects hard type capacity"));

 // Batch B boundaries: shared retained-unit capacity, distinct deadlines, and current urgency.
 auto CompositionCapacity=CompositionHeavy;CompositionCapacity.UnitSlots=1;
 CompositionCheck(BattleComposition::Decide(CompositionCapacity,CompositionDefs,CompositionInitial).Type==BattleSquadType::Tank,TEXT("planner remaining one unit slot selects tank over heavy"));
 CompositionCapacity.UnitSlots=0;
 CompositionCheck(BattleComposition::Decide(CompositionCapacity,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Wait,TEXT("planner zero unit slots excludes all candidates"));
 CompositionCapacity.UnitSlots=CompositionDefs[1].Members;
 const auto CompositionExactCapacity=BattleComposition::Decide(CompositionCapacity,CompositionDefs,CompositionInitial);
 bool CompositionHeavyFits=false;
 for(const auto& CapacityCandidate:CompositionExactCapacity.Candidates)if(CapacityCandidate.Type==1)CompositionHeavyFits=CapacityCandidate.Excluded.IsEmpty();
 CompositionCheck(CompositionHeavyFits,TEXT("planner exact member capacity is allowed"));
 auto CompositionLate=CompositionInput;CompositionLate.NowMS=530000;CompositionLate.StopAtMS=540000;CompositionLate.ArrivalDeadlineMS=600000;CompositionLate.ArrivalMS[4]=20000;
 CompositionCheck(BattleComposition::Decide(CompositionLate,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Buy,TEXT("planner may arrive after hire stop but before match end"));
 CompositionLate.NowMS=540000;
 CompositionCheck(BattleComposition::Decide(CompositionLate,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Wait,TEXT("planner cannot buy at hire stop"));
 CompositionLate.NowMS=530000;CompositionLate.ArrivalDeadlineMS=550000;
 CompositionCheck(BattleComposition::Decide(CompositionLate,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Wait,TEXT("planner arrival exactly at mission deadline is too late"));
 CompositionLate.ArrivalDeadlineMS=600000;CompositionLate.Free=31;
 CompositionCheck(BattleComposition::Decide(CompositionLate,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Save,TEXT("planner saves when purchase precedes stop and arrival follows stop"));
 CompositionLate.Free=30;
 CompositionCheck(BattleComposition::Decide(CompositionLate,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Wait,TEXT("planner saving cannot purchase exactly at stop"));
 auto CompositionUrgency=CompositionInput;CompositionUrgency.Free=0;CompositionUrgency.bHomeThreat=true;CompositionUrgency.Mission=0;
 CompositionUrgency.Types[4].PacedTarget=0;CompositionUrgency.Types[5].PacedTarget=1;CompositionDefs[5].Cost=40;
 const auto CompositionUrgentSave=BattleComposition::Decide(CompositionUrgency,CompositionDefs,CompositionInitial);
 CompositionCheck(CompositionUrgentSave.Type==5&&CompositionUrgentSave.Next.SaveUrgency==3,TEXT("planner starts home-threat car saving at urgency three"));
 CompositionUrgency.bHomeThreat=false;CompositionUrgency.ObservedVehicles=1;CompositionUrgency.Types[3].PacedTarget=1;CompositionUrgency.Free=12;CompositionUrgency.NowMS+=5000;
 const auto CompositionCurrentInterrupt=BattleComposition::Decide(CompositionUrgency,CompositionDefs,CompositionUrgentSave.Next);
 CompositionCheck(CompositionCurrentInterrupt.Type==3&&CompositionCurrentInterrupt.Action==BattleComposition::EAction::Buy&&CompositionCurrentInterrupt.Reason.Contains(TEXT("urgent_interrupt")),TEXT("planner current urgency zero allows urgency two interruption despite saved urgency three"));
 auto CompositionDefended=CompositionHomeEmergency;CompositionDefended.HomeDefenders=1;
 CompositionCheck(BattleComposition::Decide(CompositionDefended,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Wait,TEXT("planner emergency defence requires fewer than one existing defender"));
 CompositionDefended.HomeDefenders=0;CompositionDefended.bHomeThreat=false;
 CompositionCheck(BattleComposition::Decide(CompositionDefended,CompositionDefs,CompositionInitial).Action==BattleComposition::EAction::Wait,TEXT("planner emergency mission ends when threat clears"));
 auto CompositionConfigFailure=CompositionInitial;
 BattleComposition::CompletePurchase(CompositionConfigFailure,false,4,BattleComposition::EFailure::Configuration);
 CompositionCheck(BattleComposition::Decide(CompositionInput,CompositionDefs,CompositionConfigFailure).Reason==TEXT("configuration_failed"),TEXT("planner configuration failure prevents repeated execution"));
 auto CompositionCapacityFailure=CompositionInitial;
 BattleComposition::CompletePurchase(CompositionCapacityFailure,false,4,BattleComposition::EFailure::Capacity);
 CompositionCheck(CompositionCapacityFailure.RetryUntil==0&&!CompositionCapacityFailure.bConfigurationFailed,TEXT("planner capacity failure rechecks candidates without global placement backoff"));
 CompositionCheck(BattleComposition::ClassifyFailure(TEXT("Match roster capacity reached (1000 including losses)"))==BattleComposition::EFailure::Capacity&&
  BattleComposition::ClassifyFailure(TEXT("Home deployment area is blocked"))==BattleComposition::EFailure::Placement&&
  BattleComposition::ClassifyFailure(TEXT("Navigation unavailable"))==BattleComposition::EFailure::Configuration,TEXT("planner classifies executor capacity placement and configuration messages"));

 // Serialize nonempty goal and window state through the real SaveGame codec, then continue the pure decision.
 auto* CompositionMatch=GetWorld()->GetSubsystem<UBattleMatch>();const FString CompositionOriginalMessage=CompositionMatch->LastMessage;
 auto* CompositionSnapshot=CompositionMatch->CaptureSnapshot();
 CompositionCheck(CompositionSnapshot!=nullptr,TEXT("planner can capture area fixture"));
 if(CompositionSnapshot)
 {
  CompositionSnapshot->CombatStateVersion=4;CompositionSnapshot->CompositionTeams=2;
  CompositionSnapshot->CompositionPolicy=BattleComposition::PolicyVersion;CompositionSnapshot->CompositionRules=BattleComposition::RulesKey();
  CompositionSnapshot->CompositionState.SetNum(2);CompositionSnapshot->CompositionState[0]=FCompositionTeamState();
  auto CompositionPersisted=CompositionSaved.Next;CompositionPersisted.SaveCost=BattleComposition::Definitions()[4].Cost;
  CompositionPersisted.WindowCredits=CompositionSnapshot->EnemyCredits;CompositionPersisted.NetPerMinute=60;
  for(const auto& PersistSquad:CompositionSnapshot->Squads)if(PersistSquad.Team==1)
  {CompositionPersisted.EmergencyUntilMS.Add(PersistSquad.SquadID,30000);CompositionSnapshot->Area.HirePurpose.Add(PersistSquad.SquadID,101);break;}
  CompositionPersisted.bConfigurationFailed=true;
  CompositionSnapshot->CompositionState[1]=CompositionPersisted;
  FString CompositionSaveError;TArray<uint8> CompositionBytes;
  const bool CompositionEncoded=CompositionSnapshot->Validate(CompositionSaveError)&&UGameplayStatics::SaveGameToMemory(CompositionSnapshot,CompositionBytes);
  auto* CompositionDecoded=CompositionEncoded?Cast<UBattleSave>(UGameplayStatics::LoadGameFromMemory(CompositionBytes)):nullptr;
  CompositionCheck(CompositionDecoded&&CompositionDecoded->Validate(CompositionSaveError)&&FCompositionTeamState::StaticStruct()->CompareScriptStruct(&CompositionDecoded->CompositionState[1],&CompositionPersisted,0),TEXT("planner revision four serializes full saving and income state"));
  if(CompositionDecoded)
  {
   CompositionPersisted.bConfigurationFailed=false;CompositionDecoded->CompositionState[1].bConfigurationFailed=false;
   auto CompositionContinueInput=CompositionInput;CompositionContinueInput.Free=23;
   const auto CompositionContinuous=BattleComposition::Decide(CompositionContinueInput,BattleComposition::Definitions(),CompositionPersisted);
   const auto CompositionResumed=BattleComposition::Decide(CompositionContinueInput,BattleComposition::Definitions(),CompositionDecoded->CompositionState[1]);
   CompositionCheck(CompositionContinuous.Type==CompositionResumed.Type&&CompositionContinuous.Action==CompositionResumed.Action&&FCompositionTeamState::StaticStruct()->CompareScriptStruct(&CompositionContinuous.Next,&CompositionResumed.Next,0),TEXT("planner post-save decision and state match uninterrupted calculation"));
   auto* CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CompositionPolicy=1;
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner rejects old policy one v4 instead of guessing emergency provenance"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CompositionState[1].EmergencyUntilMS.Reset();CompositionBad->CompositionState[1].EmergencyUntilMS.Add(MAX_int32,30000);
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner rejects unknown emergency squad reference"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->Area.HirePurpose.Reset();
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner rejects emergency provenance without matching hire purpose"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);
   for(auto& EmergencyValue:CompositionBad->CompositionState[1].EmergencyUntilMS)EmergencyValue.Value=FMath::FloorToInt(CompositionBad->Elapsed*1000.f)+30001;
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner rejects emergency deadline beyond thirty seconds"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CompositionState[1].SaveType=99;
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner save rejects invalid type"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CompositionState[1].SaveDeadlineMS=CompositionBad->CompositionState[1].SaveStartedMS-1;
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner save rejects inverted deadline"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CompositionState[1].VehicleSeenMS.Add(MAX_int32,0);
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner save rejects unknown observation ID"));
   CompositionBad->CompositionState[1].VehicleSeenMS.Reset();
   for(const auto& CompositionUnit:CompositionBad->Units)if(CompositionUnit.State.Team==1){CompositionBad->CompositionState[1].VehicleSeenMS.Add(CompositionUnit.State.UnitID,0);break;}
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner save rejects own-team vehicle memory"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CompositionRules=FString::ChrN(32,TCHAR('0'));
   const int32 CompositionCreditsBefore=CompositionMatch->Credits[1];const int32 CompositionMaskBefore=CompositionMatch->CompositionTeams;
   CompositionCheck(!CompositionMatch->RestoreSnapshot(CompositionBad)&&CompositionMatch->Credits[1]==CompositionCreditsBefore&&CompositionMatch->CompositionTeams==CompositionMaskBefore,TEXT("planner rule mismatch rejected before mutating live match"));
   CompositionBad=DuplicateObject<UBattleSave>(CompositionDecoded,this);CompositionBad->CombatStateVersion=3;
   CompositionCheck(!CompositionBad->Validate(CompositionSaveError),TEXT("planner state cannot be smuggled into older extension"));
   CompositionBad->CompositionTeams=0;CompositionBad->CompositionPolicy=0;CompositionBad->CompositionRules.Reset();CompositionBad->CompositionState.Reset();
   CompositionCheck(CompositionBad->Validate(CompositionSaveError),TEXT("legacy revision three remains readable without planner data"));
  }
 }
 CompositionMatch->LastMessage=CompositionOriginalMessage;
}
