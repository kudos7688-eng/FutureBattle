#include "BattleComposition.h"

namespace
{
 void ClearCompositionGoal(FCompositionTeamState& State)
 {
  State.SaveType=-1;State.SaveMission=-1;State.SaveCost=0;State.SaveUrgency=0;State.SaveStartedMS=0;State.SaveDeadlineMS=0;
  State.SwitchType=-1;State.SwitchCount=0;
 }
 bool IsCompositionFrontType(int32 Type){return Type==BattleSquadType::Rifle||Type==BattleSquadType::Heavy||Type==BattleSquadType::Exo;}
}

BattleComposition::FDecision BattleComposition::Decide(const FInput& Input,const TArray<FTypeDef>& Defs,const FCompositionTeamState& Previous)
{
 FDecision Result;Result.Next=Previous;auto& Next=Result.Next;
 auto Finish=[&](EAction Action,const FCandidate* Candidate,const TCHAR* Reason)->FDecision
 {
  Result.Action=Action;Result.Reason+=Reason;Next.Reason=Result.Reason;
  if(Candidate){Result.Type=Candidate->Type;Result.Mission=Candidate->Mission;}
  return Result;
 };
 // Explicit bounds make malformed pure-function inputs fail closed; save validation is stricter.
 if(Defs.Num()!=BattleSquadType::Count||Next.Waiting.Num()!=BattleSquadType::Count||Next.SaveBlockedUntil.Num()!=BattleSquadType::Count||Next.Purchased.Num()!=BattleSquadType::Count||Next.SaveType< -1||Next.SaveType>=BattleSquadType::Supply||Next.Serial<0||Next.Serial>=100000)
  return Finish(EAction::Wait,nullptr,TEXT("invalid_input"));
 if(Next.bConfigurationFailed)return Finish(EAction::Wait,nullptr,TEXT("configuration_failed"));
 ++Next.Serial;
 bool Seen[BattleSquadType::Count]={};
 for(const auto& Def:Defs){if(Def.TypeID>=BattleSquadType::Count||Seen[Def.TypeID])return Finish(EAction::Wait,nullptr,TEXT("invalid_definitions"));Seen[Def.TypeID]=true;}
 if(Input.Mission==101&&(!Input.bHomeThreat||Input.HomeDefenders>=1))
 {ClearCompositionGoal(Next);return Finish(EAction::Wait,nullptr,TEXT("emergency_defence_not_needed"));}
 if(!Input.bAutoAllowed||!Input.bHireEnabled||Input.NowMS>=Input.StopAtMS||Input.Active>=Input.MaxActive||Input.Records>=100||(Input.bMissionRequired&&Input.Mission<0))
 {
  ClearCompositionGoal(Next);return Finish(EAction::Wait,nullptr,TEXT("permission_time_capacity_or_mission"));
 }
 if(!Input.bCanPurchase||Next.Serial<=Next.RetryUntil)return Finish(EAction::Wait,nullptr,TEXT("interval_or_spawn_backoff"));

 // Provisional policy 1: reserve three rifle/heavy frontline slots, then special goals, then the remaining
 // rifle/heavy mix. Explicit Keep=0 stays zero. At overflow, observed armour first, then stable TypeID.
 int32 Targets[BattleSquadType::Count]={};int32 Budget=FMath::Max(0,Input.MaxActive-FMath::Min(Input.FrontMinimum,Input.MaxActive));
 TArray<FTypeDef> Ordered=Defs;
 Ordered.Sort([&](const FTypeDef& A,const FTypeDef& B)
 {
  const bool UrgentA=Input.ObservedVehicles>0&&(A.Capabilities&AntiArmour)!=0;
  const bool UrgentB=Input.ObservedVehicles>0&&(B.Capabilities&AntiArmour)!=0;
  return UrgentA!=UrgentB?UrgentA:A.TypeID<B.TypeID;
 });
 for(const auto& Def:Ordered)if(Def.TypeID>=BattleSquadType::Scout&&Def.bAIEligible)
 {
  const int32 Want=FMath::Clamp(Input.Types[Def.TypeID].PacedTarget,0,FMath::Max(0,Def.HardActiveLimit));
  Targets[Def.TypeID]=FMath::Min(Budget,Want);Budget-=Targets[Def.TypeID];
 }
 int32 Specials=0;for(int32 Type=2;Type<BattleSquadType::Count;++Type)Specials+=Targets[Type];
 const int32 Infantry=FMath::Max(0,Input.MaxActive-Specials);
 Targets[BattleSquadType::Heavy]=FMath::Clamp(FMath::CeilToInt(FMath::Clamp(Input.HeavyShare,0.f,1.f)*Infantry),0,Infantry);
 Targets[BattleSquadType::Rifle]=Infantry-Targets[BattleSquadType::Heavy];
 // Role demand is aggregated once, not once per group. A squad contributes to exactly one actual group role.
 int32 RoleTargets[5]={};for(const auto& Def:Defs)if(Def.DefaultGroupRole<5)RoleTargets[Def.DefaultGroupRole]+=Targets[Def.TypeID];
 int32 ArmourEffective=0;for(const auto& Def:Defs)if((Def.Capabilities&AntiArmour)!=0)ArmourEffective+=Input.Types[Def.TypeID].AntiArmourEffective;
 const bool ArmourShort=Input.ObservedVehicles*1000>ArmourEffective;
 const bool FrontShort=Input.FrontEffective<Input.FrontMinimum*1000;
 for(const auto& Def:Defs)
 {
  FCandidate Candidate;Candidate.Type=Def.TypeID;Candidate.Mission=Input.Mission;Candidate.Cost=Def.Cost;
  Candidate.Target=Targets[Def.TypeID];Candidate.ArrivalMS=Input.ArrivalMS[Def.TypeID];
  const auto& TypeInput=Input.Types[Def.TypeID];
  Candidate.Deficit=FMath::Clamp((1000*Candidate.Target-TypeInput.Effective)/FMath::Max(1,Candidate.Target),0,1000);
  if(Def.DefaultGroupRole<5)
  {
   const int32 Role=Def.DefaultGroupRole;
   Candidate.RoleDeficit=FMath::Clamp((1000*RoleTargets[Role]-Input.RoleEffective[Role])/FMath::Max(1,RoleTargets[Role]),0,1000);
  }
  const bool AnswersArmour=(Def.Capabilities&AntiArmour)!=0;
  const bool AnswersHome=(Def.Capabilities&(AntiInfantry|AntiArmour))!=0&&(Def.Capabilities&Indirect)==0&&Def.TypeID!=BattleSquadType::Scout;
  Candidate.Urgency=(FrontShort&&IsCompositionFrontType(Def.TypeID))||(Input.bHomeThreat&&AnswersHome&&(!ArmourShort||AnswersArmour))?3:
   ArmourShort&&AnswersArmour?2:Candidate.RoleDeficit>0?1:0;
  // Same weights as the design. 60s travel = 1000 penalty; no price divisor or random tie breaking.
  Candidate.Score=4*Candidate.Deficit+3*Candidate.RoleDeficit-FMath::Clamp(Candidate.ArrivalMS/60,0,1000);
  if(!Def.bAIEligible)Candidate.Excluded=TEXT("not_ai_eligible");
  else if(Def.Members<=0||Def.Cost<=0)Candidate.Excluded=TEXT("invalid_configuration");
  else if(Def.Members>Input.UnitSlots)Candidate.Excluded=TEXT("unit_record_capacity");
  else if(Candidate.Target<=0)Candidate.Excluded=TEXT("zero_or_locked_target");
  else if(TypeInput.Active>=Def.HardActiveLimit)Candidate.Excluded=TEXT("type_limit");
  else if(Candidate.Deficit<=0)Candidate.Excluded=TEXT("satisfied");
  else if((Input.Mission==100||Input.Mission==101)&&(Def.TypeID==BattleSquadType::Scout||(Def.Capabilities&Indirect)!=0))Candidate.Excluded=TEXT("home_mission_incompatible");
  else if(FrontShort&&!IsCompositionFrontType(Def.TypeID))Candidate.Excluded=TEXT("frontline_emergency");
  else if(int64(Input.NowMS)+Candidate.ArrivalMS>=Input.ArrivalDeadlineMS)Candidate.Excluded=TEXT("too_late_to_arrive");
  Next.Waiting[Def.TypeID]=Candidate.Excluded.IsEmpty()?FMath::Min(100000,Next.Waiting[Def.TypeID]+1):0;
  Candidate.Waiting=Next.Waiting[Def.TypeID];Result.Candidates.Add(Candidate);
 }
 Result.Candidates.Sort([](const FCandidate& A,const FCandidate& B)
 {
  if(A.Urgency!=B.Urgency)return A.Urgency>B.Urgency;
  if(A.Score!=B.Score)return A.Score>B.Score;
  if(A.Waiting!=B.Waiting)return A.Waiting>B.Waiting;
  return A.Type!=B.Type?A.Type<B.Type:A.Mission<B.Mission;
 });
 auto SaveDelay=[&](const FCandidate& Candidate)->int32
 {
  if(Input.NetPerMinute<=0)return MAX_int32;
  return static_cast<int32>(FMath::Min<int64>(MAX_int32,(int64(FMath::Max(0,Candidate.Cost-Input.Free))*60000+Input.NetPerMinute-1)/Input.NetPerMinute));
 };
 auto CanSave=[&](const FCandidate& Candidate,int32 Deadline)->bool
 {
  const int32 Delay=SaveDelay(Candidate);
  return !FrontShort&&Input.NetPerMinute>0&&Next.Serial>Next.SaveBlockedUntil[Candidate.Type]&&Delay<=60000&&
   int64(Input.NowMS)+Delay<Deadline&&int64(Input.NowMS)+Delay<Input.StopAtMS&&int64(Input.NowMS)+Delay+Candidate.ArrivalMS<Input.ArrivalDeadlineMS;
 };
 auto EmergencyChoice=[&](int32 PreviousUrgency,int32 Deadline)->const FCandidate*
 {
  const FCandidate* Emergency=nullptr;
  for(const auto& Candidate:Result.Candidates)if(Candidate.Excluded.IsEmpty()&&Candidate.Urgency>PreviousUrgency&&Candidate.Cost<=Input.Free)return &Candidate;
  for(const auto& Candidate:Result.Candidates)if(Candidate.Excluded.IsEmpty()&&Candidate.Urgency>PreviousUrgency&&CanSave(Candidate,Deadline))
   if(!Emergency||SaveDelay(Candidate)<SaveDelay(*Emergency))Emergency=&Candidate;
  return Emergency;
 };
 const FCandidate* Best=nullptr;const FCandidate* Held=nullptr;
 for(const auto& Candidate:Result.Candidates)if(Candidate.Excluded.IsEmpty())
 {
  if(!Best)Best=&Candidate;
  if(Candidate.Type==Next.SaveType&&Candidate.Mission==Next.SaveMission&&Candidate.Cost==Next.SaveCost)Held=&Candidate;
 }
 int32 OriginalDeadline=Next.SaveDeadlineMS;
 if(Next.SaveType>=0)
 {
  if(Input.NowMS>=Next.SaveDeadlineMS)
  {
   Next.SaveBlockedUntil[Next.SaveType]=Next.Serial+3;ClearCompositionGoal(Next);Held=nullptr;OriginalDeadline=0;Result.Reason=TEXT("expired; ");
  }
  else if(!Held){ClearCompositionGoal(Next);OriginalDeadline=0;Result.Reason=TEXT("goal_invalid; ");}
  else if(Held->Cost>Input.Free&&!CanSave(*Held,Next.SaveDeadlineMS))
  {ClearCompositionGoal(Next);Held=nullptr;OriginalDeadline=0;Result.Reason=TEXT("unreachable_or_frontline; ");}
 }
 const FCandidate* Chosen=nullptr;
 const int32 SelectionDeadline=Previous.SaveType>=0&&Previous.SaveDeadlineMS>Input.NowMS?Previous.SaveDeadlineMS:Input.NowMS+60000;
 // A newly required home-defence mission can invalidate the old mission before the urgency comparison.
 // It is still an interruption: prefer an affordable response and do not grant a fresh 60s deadline.
 if(!Held&&Previous.SaveType>=0&&Previous.SaveDeadlineMS>Input.NowMS&&Best&&Best->Urgency>0)
 {
  Chosen=EmergencyChoice(0,Previous.SaveDeadlineMS);
  if(Chosen){OriginalDeadline=Previous.SaveDeadlineMS;Result.Reason+=TEXT("urgent_new_mission; ");}
 }
 if(Held)
 {
  // Interrupt baseline: the lower of the urgency the goal started with and its current urgency (review C06 + armour that the held type also answers).
  const int32 HeldUrgency=FMath::Min<int32>(Held->Urgency,Next.SaveUrgency);
  if(Best&&Best->Urgency>HeldUrgency)
  {
   // Higher urgency: affordable response first, otherwise shortest reachable saving delay (stable sorted ties).
   Chosen=EmergencyChoice(HeldUrgency,OriginalDeadline);
   if(Chosen)Result.Reason+=TEXT("urgent_interrupt; ");
  }
  else if(Best&&Best->Type!=Held->Type&&Best->Urgency==Held->Urgency&&Best->Score>=Held->Score+500)
  {
   Next.SwitchCount=Next.SwitchType==Best->Type?Next.SwitchCount+1:1;Next.SwitchType=Best->Type;
   if(Next.SwitchCount>=2&&(Best->Cost<=Input.Free||CanSave(*Best,OriginalDeadline))){Chosen=Best;Result.Reason+=TEXT("stable_better_goal; ");}
  }
  else{Next.SwitchType=-1;Next.SwitchCount=0;}
  if(!Chosen)Chosen=Held;
 }
 if(!Chosen)for(const auto& Candidate:Result.Candidates)if(Candidate.Excluded.IsEmpty())
 {
  if(Candidate.Cost<=Input.Free||CanSave(Candidate,SelectionDeadline)){Chosen=&Candidate;break;}
 }
 if(!Chosen){ClearCompositionGoal(Next);return Finish(EAction::Wait,nullptr,TEXT("no_affordable_or_reachable_candidate"));}
 if(Chosen->Cost<=Input.Free)return Finish(EAction::Buy,Chosen,TEXT("buy"));
 if(Next.SaveType!=Chosen->Type||Next.SaveMission!=Chosen->Mission)
 {
  const int32 Started=Next.SaveType>=0?Next.SaveStartedMS:Input.NowMS;
  ClearCompositionGoal(Next);Next.SaveType=Chosen->Type;Next.SaveMission=Chosen->Mission;Next.SaveCost=Chosen->Cost;Next.SaveUrgency=Chosen->Urgency;
  Next.SaveStartedMS=Started;Next.SaveDeadlineMS=OriginalDeadline>0?OriginalDeadline:SelectionDeadline;
 }
 return Finish(EAction::Save,Chosen,TEXT("save"));
}

BattleComposition::EFailure BattleComposition::ClassifyFailure(const FString& Message)
{
 if(Message.Contains(TEXT("capacity"))||Message.Contains(TEXT("limit")))return EFailure::Capacity;
 if(Message==TEXT("Home deployment area is blocked")||Message==TEXT("Representation staging failed; purchase retained"))return EFailure::Placement;
 return EFailure::Configuration; // unavailable navigation/mode or unexpected executor contract failure
}

void BattleComposition::CompletePurchase(FCompositionTeamState& State,bool bSuccess,int32 Type,EFailure Failure)
{
 ClearCompositionGoal(State);
 if(bSuccess){State.LastSuccess=State.Serial;if(State.Waiting.IsValidIndex(Type))State.Waiting[Type]=0;State.Reason=TEXT("purchased");}
 else if(Failure==EFailure::Placement){State.RetryUntil=State.Serial+2;State.Reason=TEXT("spawn_failed_backoff");}
 else if(Failure==EFailure::Capacity){State.RetryUntil=0;State.Reason=TEXT("capacity_recheck");}
 else{State.bConfigurationFailed=true;State.Reason=TEXT("configuration_failed");}
 // Purchased[] is advanced only by HireAreaSquad AFTER actual successful spawning, including manual purchases.
}

bool BattleComposition::ValidateState(const FCompositionTeamState& State,int32 NowMS,int32 Objectives,FString& Error)
{
 auto Fail=[&](){Error=TEXT("Invalid composition planner state");return false;};
 auto MissionOK=[&](int32 Mission){return Mission==-1||(Mission>=0&&Mission<Objectives)||Mission==100||Mission==101;};
 if(State.EmergencyUntilMS.Num()>1)return Fail();
 for(const auto& Emergency:State.EmergencyUntilMS)if(Emergency.Key<=0||Emergency.Value<=0||Emergency.Value>NowMS+30000)return Fail();
 if(State.Serial<0||State.Serial>100000||State.Purchased.Num()!=10||State.Waiting.Num()!=10||State.SaveBlockedUntil.Num()!=10||State.Reason.Len()>200)return Fail();
 for(int32 Type=0;Type<10;++Type)if(State.Purchased[Type]<0||State.Purchased[Type]>100||State.Waiting[Type]<0||State.Waiting[Type]>100000||State.SaveBlockedUntil[Type]<0||State.SaveBlockedUntil[Type]>State.Serial+3)return Fail();
 if(State.RetryUntil<0||State.RetryUntil>State.Serial+2||State.LastSuccess<0||State.LastSuccess>State.Serial||State.SwitchType< -1||State.SwitchType>=9||State.SwitchCount<0||State.SwitchCount>100000||(State.SwitchType<0&&State.SwitchCount!=0))return Fail();
 if(State.SaveType< -1||State.SaveType>=9||State.SaveUrgency<0||State.SaveUrgency>3||!MissionOK(State.SaveMission))return Fail();
 if(State.SaveType<0)
 {if(State.SaveMission!=-1||State.SaveCost!=0||State.SaveUrgency!=0||State.SaveStartedMS!=0||State.SaveDeadlineMS!=0||State.SwitchType!=-1||State.SwitchCount!=0)return Fail();}
 else if(State.SaveCost<1||State.SaveCost>200||State.SaveStartedMS<0||State.SaveStartedMS>NowMS||State.SaveDeadlineMS<=State.SaveStartedMS||State.SaveDeadlineMS-State.SaveStartedMS>60000||State.SaveDeadlineMS>NowMS+60000)return Fail();
 if(State.WindowAtMS<0||State.WindowAtMS>NowMS||State.WindowCredits<0||State.WindowCredits>1000000||State.WindowIncome<0||State.WindowIncome>1000000||State.WindowHireSpent<0||State.WindowHireSpent>1000000||State.NetPerMinute< -3000000||State.NetPerMinute>3000000||State.VehicleSeenMS.Num()>2000)return Fail();
 for(const auto& Observation:State.VehicleSeenMS)if(Observation.Key<=0||Observation.Value<0||Observation.Value>NowMS)return Fail();
 return true;
}
