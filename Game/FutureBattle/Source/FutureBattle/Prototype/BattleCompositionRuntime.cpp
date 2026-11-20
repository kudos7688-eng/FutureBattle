#include "BattleComposition.h"
#include "BattleMatch.h"
#include "BattleTactics.h"
#include "BattlePerception.h"
#include "BattleLeaderRuntime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
#include "FutureBattle.h"

FString BattleComposition::RulesKey()
{
 // Conservative compatibility key: all config tuning plus purchase/supply/strategy overrides.
 // Sorting makes argument/property enumeration order irrelevant. Output paths and trial seeds are not rules.
 TArray<FString> Parts;const auto* Tuning=GetDefault<UBattleTuning>();
 Parts.Add(FString::Printf(TEXT("composition_policy=%d"),PolicyVersion));
 for(TFieldIterator<FProperty> Property(Tuning->GetClass());Property;++Property)if(Property->HasAnyPropertyFlags(CPF_Config))
 {
  FString Value;Property->ExportText_InContainer(0,Value,Tuning,nullptr,const_cast<UBattleTuning*>(Tuning),PPF_None);
  Parts.Add(Property->GetName()+TEXT("=")+Value);
 }
 const TCHAR* Cursor=FCommandLine::Get();FString Token;TSet<FString> OverrideNames;
 while(FParse::Token(Cursor,Token,false))
 {
  Token.RemoveFromStart(TEXT("-"));
  if(!Token.RemoveFromStart(TEXT("Battle")))continue;
  // Prefix categories, not additional switches (keep these out of Export-Switches' literal-name scan).
  if(Token.StartsWith(TEXT("AreaCompositionPlanner"))||Token==TEXT("AreaSmoke"))continue;
  if(Token.StartsWith(TEXT("Area"))||Token.StartsWith(TEXT("Heavy"))||Token.StartsWith(TEXT("Rifle"))||
     Token.StartsWith(TEXT("Supply"))||Token.StartsWith(TEXT("LeaderHealth"))||Token.StartsWith(TEXT("Armour"))||
     Token.StartsWith(TEXT("Group"))||Token.StartsWith(TEXT("SPGAmmo")))
  {
   int32 Equals=INDEX_NONE;Token.FindChar(TCHAR('='),Equals);const FString Name=(Equals>=0?Token.Left(Equals):Token).ToLower();
   // FParse resolves the first occurrence; duplicate override order must not collide in the key.
   if(!OverrideNames.Contains(Name)){OverrideNames.Add(Name);Parts.Add(Token);}
  }
 }
 for(const auto& Def:Definitions())Parts.Add(FString::Printf(TEXT("def=%d,%d,%d,%d,%d,%d"),Def.TypeID,Def.Cost,Def.HardActiveLimit,Def.Members,Def.DefaultGroupRole,Def.Capabilities));
 Parts.Sort();return FMD5::HashAnsiString(*FString::Join(Parts,TEXT("\n")));
}

void UBattleMatch::InitializeComposition()
{
 const auto Options=BattleComposition::Settings();if(!Options.bEnabled||Options.Teams==0)return;
 CompositionTeams=Options.Teams;CompositionState.SetNum(2);
 for(int32 Team=0;Team<2;++Team)if((CompositionTeams&(1<<Team))!=0)
 {
  auto& State=CompositionState[Team];State=FCompositionTeamState();State.WindowCredits=Credits[Team];
  State.WindowAtMS=FMath::FloorToInt(Elapsed*1000.f);State.WindowIncome=Area.Income[Team];State.WindowHireSpent=Area.HireSpent[Team];
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_RULES policy=%d key=%s teams=%d"),BattleComposition::PolicyVersion,*BattleComposition::RulesKey(),CompositionTeams);
}

void UBattleMatch::UpdateCompositionHiring(int32 Team)
{
 if(Team<0||Team>1||(CompositionTeams&(1<<Team))==0||!CompositionState.IsValidIndex(Team))return;
 const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(Team==0&&!Runtime->SightExperiment.Enabled)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 const int32 NowMS=FMath::FloorToInt(Elapsed*1000.f);const double WorldNow=GetWorld()->GetTimeSeconds();
 auto& State=CompositionState[Team];
 // Actual resource receipts, not objective truth. A 20s window spans the default income cycle; no speculative
 // saving before the first sample. Purchases (including manual ones) do not masquerade as replacement expense.
 const int32 WindowMS=NowMS-State.WindowAtMS;
 if(WindowMS>=20000)
 {
  const int32 IncomeDelta=Area.Income[Team]-State.WindowIncome;
  const int32 HireDelta=Area.HireSpent[Team]-State.WindowHireSpent;
  const int32 OtherSpent=FMath::Max(0,State.WindowCredits+IncomeDelta-HireDelta-Credits[Team]);
  State.NetPerMinute=static_cast<int32>(int64(IncomeDelta-OtherSpent)*60000/WindowMS);
  State.WindowAtMS=NowMS;State.WindowCredits=Credits[Team];State.WindowIncome=Area.Income[Team];State.WindowHireSpent=Area.HireSpent[Team];
 }
 // Only observed types and observation ages enter the planner's own persisted memory. No enemy state lookup.
 TArray<int32> OwnIDs;for(const auto& Pair:Store->GetUnits())if(Pair.Value.Team==Team&&Pair.Value.IsAlive())OwnIDs.Add(Pair.Key);OwnIDs.Sort();
 const int32 MemoryMS=FMath::FloorToInt(FMath::Max(1.f,GetDefault<UBattleTuning>()->VehicleMemory)*1000.f);
 for(int32 OwnID:OwnIDs)if(const auto* Contacts=Perception?Perception->GetContacts(OwnID):nullptr)
 {
  TArray<int32> ContactIDs;Contacts->GetKeys(ContactIDs);ContactIDs.Sort();
  for(int32 ContactID:ContactIDs)
  {
   const auto& Contact=Contacts->FindChecked(ContactID);
   if(Contact.Confidence<=0||!BattleSquadType::IsVehicle(Contact.ObservedSquadType)||IsScoutLongContact(OwnID,Contact))continue;
   const double Age=FMath::Max(0.,WorldNow-Contact.LastSeenTime);
   if(Age*1000.>MemoryMS)continue;
   const int32 SeenMS=FMath::Max(0,NowMS-FMath::FloorToInt(Age*1000.));
   int32& Remembered=State.VehicleSeenMS.FindOrAdd(ContactID);Remembered=FMath::Max(Remembered,SeenMS);
  }
 }
 for(auto It=State.VehicleSeenMS.CreateIterator();It;++It)if(NowMS-It.Value()>MemoryMS)It.RemoveCurrent();
 int32 Bought=0;for(int32 Count:State.Purchased)Bought+=Count;
 FAreaIntent Intent=AreaIntent(Team);const bool AutoTargets=BattleTactics::AreaComposition(Team);
 if(AutoTargets)
 {
  // Preserve AreaComposition's target formula/explicit override semantics, using observed memory instead of
  // its legacy KnownEnemyVehicles refresh (which consults the runtime vehicle registry).
  const auto* Tuning=GetDefault<UBattleTuning>();const auto& Strategy=BattleTactics::AreaStrategy();
  auto Cap=[](int32 Want,int32 Limit){return FMath::Clamp(Want,0,FMath::Max(0,Limit));};
  if(Strategy.ACKeep<0)Intent.ACKeep=Cap(Bought>=12?2:1,Tuning->AreaACLimit);
  if(Strategy.MortarKeep<0)Intent.MortarKeep=Cap(Bought>=12?2:1,Tuning->AreaMortarLimit);
  if(Strategy.TankKeep<0)Intent.TankKeep=Cap(Bought>=6?(!State.VehicleSeenMS.IsEmpty()?2:1):0,Tuning->AreaTankLimit);
  if(Strategy.SPGKeep<0)Intent.SPGKeep=Cap(Bought>=9?1:0,Tuning->AreaSPGLimit);
  if(Strategy.ATKeep<0)Intent.ATKeep=Cap(State.VehicleSeenMS.Num(),Tuning->AreaATLimit);
 }
 auto Input=BattleComposition::Capture(*this,*Store,Team,Intent,AutoTargets,true);
 Input.NowMS=NowMS;Input.ObservedVehicles=State.VehicleSeenMS.Num();
 const auto& Hire=BattleTactics::AreaHire();Input.MaxActive=Hire.MaxActive;Input.FrontMinimum=FMath::Min(3,Hire.MaxActive);
 Input.StopAtMS=FMath::FloorToInt((MatchTimeLimit()-Hire.Stop)*1000.f);
 Input.NetPerMinute=BattleTactics::AreaIncome().bEnabled&&!bAreaIncomeHeld?State.NetPerMinute:0;
 Input.bCanPurchase=Hire.Interval<=0||Elapsed-Area.LastAIHireAt[Team]>=Hire.Interval;
 Input.bMissionRequired=Hire.bPurpose&&(BattleTactics::AreaStrategy().TrialTeam<0||BattleTactics::AreaStrategy().TrialTeam==Team);
 Input.bHomeThreat=Area.HomeHitAt.IsValidIndex(Team)&&Elapsed-Area.HomeHitAt[Team]>=0&&Elapsed-Area.HomeHitAt[Team]<30.f;
 Input.Mission=Input.bMissionRequired?AreaHireNeed(Team):Input.bHomeThreat?101:-1;
 // Targets still unlock by successful purchase count. The live path no longer infers it from SquadID>50.
 const int32 Unlocked=Bought/FMath::Max(1,GetDefault<UBattleTuning>()->AreaScoutAfterHires);
 for(int32 Type=2;Type<BattleSquadType::Count;++Type)Input.Types[Type].PacedTarget=Type==BattleSquadType::AntiTank&&AutoTargets?Input.Types[Type].Keep:FMath::Min(Input.Types[Type].Keep,Unlocked);
 FVector Goal=FVector::ZeroVector;
 if(Objectives.IsValidIndex(Input.Mission))Goal=Objectives[Input.Mission].Position;
 else if(Input.Mission==100)Goal=AreaHome(1-Team);
 else if(Input.Mission==101)Goal=AreaHome(Team);
 // Provisional arrival estimate: straight-line public map distance, 25% detour + 5s deployment. No Nav queries.
 for(const auto& Def:BattleComposition::Definitions())
 {
  const double Speed=FMath::Max(100.,double(GetDefault<UBattleTuning>()->MoveSpeed)*BattleSquadType::SpeedScale(Def.TypeID));
  Input.ArrivalMS[Def.TypeID]=5000+FMath::CeilToInt(FVector::Dist2D(AreaHome(Team),Goal)*1250./Speed);
 }
 BattleComposition::LogInput(Input);
 auto Decision=BattleComposition::Decide(Input,BattleComposition::Definitions(),State);State=Decision.Next;
 for(const auto& Candidate:Decision.Candidates)
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_CANDIDATE team=%d serial=%d type=%d mission=%d target=%d deficit=%d role_deficit=%d urgency=%d score=%d cost=%d arrival_ms=%d excluded=%s"),Team,State.Serial,Candidate.Type,Candidate.Mission,Candidate.Target,Candidate.Deficit,Candidate.RoleDeficit,Candidate.Urgency,Candidate.Score,Candidate.Cost,Candidate.ArrivalMS,*Candidate.Excluded);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_DECISION team=%d serial=%d action=%d type=%d mission=%d free=%d net_per_minute=%d deadline_ms=%d reason=%s"),Team,State.Serial,int32(Decision.Action),Decision.Type,Decision.Mission,Input.Free,Input.NetPerMinute,State.SaveDeadlineMS,*Decision.Reason);
 if(Decision.Action!=BattleComposition::EAction::Buy)return;
 const FString PreviousMessage=LastMessage;LastHiredSquadID=INDEX_NONE;
 const bool Purchased=HireAreaSquad(Team,EBattleTraining::Regular,true,static_cast<uint8>(Decision.Type));
 if(Purchased&&LastHiredSquadID!=INDEX_NONE&&Decision.Mission>=0)Area.HirePurpose.Add(LastHiredSquadID,Decision.Mission);
 BattleComposition::CompletePurchase(State,Purchased,Decision.Type);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_EXECUTE team=%d serial=%d type=%d success=%d squad=%d reason=%s"),Team,State.Serial,Decision.Type,Purchased?1:0,LastHiredSquadID,*State.Reason);
 LastMessage=PreviousMessage;
}
