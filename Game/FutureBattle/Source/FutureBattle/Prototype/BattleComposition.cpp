#include "BattleComposition.h"
#include "BattleMatch.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

BattleComposition::FSettings BattleComposition::Settings()
{
 // No static cache: OFF only reads the command line, without initializing any planner state.
 int32 Planner=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaCompositionPlanner="),Planner);
 int32 Teams=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaCompositionPlannerTeams="),Teams);
 FSettings Result;Result.bEnabled=Planner==1;Result.Teams=FMath::Clamp(Teams,0,3);return Result;
}

const TArray<BattleComposition::FTypeDef>& BattleComposition::Definitions()
{
 // Built once, lazily, from the same resolved sources used by the executor. Never called by the OFF hook.
 static const TArray<FTypeDef> Table=[]()
 {
  const auto* T=GetDefault<UBattleTuning>();const auto& H=BattleTactics::AreaHire();
  const int32 Limits[]={H.MaxActive,H.MaxActive,T->AreaScoutLimit,T->AreaATLimit,T->AreaTankLimit,T->AreaACLimit,T->AreaMortarLimit,T->AreaSPGLimit,T->AreaExoLimit,T->AreaSupplyLimit};
  const uint8 Roles[]={0,1,255,4,2,255,3,3,255,255};
  const uint16 Tags[]={Front|AntiInfantry,AntiInfantry,Observe,AntiArmour,AntiArmour|AntiInfantry,Mobile|AntiInfantry,Indirect,Indirect|Mobile,Front|Shield|AntiArmour,SupplyAmmo};
  TArray<FTypeDef> Result;
  for(uint8 Type=0;Type<BattleSquadType::Count;++Type)
  {
   FTypeDef D;D.TypeID=Type;D.Cost=Type==BattleSquadType::Rifle?H.Cost:BattleSquadType::AreaCost(Type);
   D.HardActiveLimit=FMath::Max(0,Limits[Type]);D.Members=BattleSquadType::Members(Type);
   D.DefaultGroupRole=Roles[Type];D.AllowedGroupRoles=Roles[Type]==255?0:static_cast<uint16>(1<<Roles[Type]);
   if(Type==BattleSquadType::Rifle)D.AllowedGroupRoles|=1<<1; // weakest rifle may support
   D.Capabilities=Tags[Type];D.bAIEligible=Type!=BattleSquadType::Supply;Result.Add(D);
  }
  return Result;
 }();
 return Table;
}

BattleComposition::FInput BattleComposition::Capture(const UBattleMatch& Match,const UBattleState& Store,int32 Team,const FAreaIntent& ResolvedIntent,bool bAutoTargets,bool bAutoAllowed)
{
 FInput Result;if(Team<0||Team>1)return Result;
 Result.Team=Team;Result.Credits=Match.Credits[Team];Result.Reserve=ResolvedIntent.HireReserve;
 Result.Elapsed=Match.Elapsed;Result.Remaining=FMath::Max(0.f,Match.MatchTimeLimit()-Match.Elapsed);
 Result.HeavyShare=ResolvedIntent.HeavyShare;Result.bAutoTargets=bAutoTargets;Result.bAutoAllowed=bAutoAllowed;
 const auto& H=BattleTactics::AreaHire();Result.bHireEnabled=H.bEnabled;Result.Interval=H.Interval;Result.Stop=H.Stop;
 if(Match.Area.LastAIHireAt.IsValidIndex(Team))Result.LastHireAt=Match.Area.LastAIHireAt[Team];
 Result.Records=Store.GetSquads().Num(); // shared record capacity, not an enemy combat input
 for(const auto& Replacement:Match.Area.Replacements)if(const auto* Unit=Store.Find(Replacement.Key);Unit&&Unit->Team==Team)++Result.Pending;
 Result.Free=FMath::Max(0,Result.Credits-Result.Pending-Result.Reserve);
 TArray<int32> SquadIDs;Store.GetSquads().GetKeys(SquadIDs);SquadIDs.Sort();
 for(int32 SquadID:SquadIDs)
 {
  const auto& Squad=Store.GetSquads().FindChecked(SquadID);if(Squad.Team!=Team)continue;
  Result.LegacyBought+=SquadID>50;Result.LegacyHeavy+=Squad.SquadType==BattleSquadType::Heavy;
  if(Squad.CommandStatus!=EBattleCommandStatus::Active)continue;
  ++Result.Active;if(Squad.SquadType>=BattleSquadType::Count)continue;
  auto& Type=Result.Types[Squad.SquadType];++Type.Active;
  int32 Ready=0;bool ArmourArmed=Squad.SquadType==BattleSquadType::Tank;
  TArray<int32> Members=Squad.Members;Members.Sort();
  for(int32 MemberID:Members)if(const auto* Unit=Store.Find(MemberID);Unit&&Unit->Team==Team&&Unit->IsAlive())
  {
   ++Type.Living;Type.Armed+=Unit->Ammo>0||Unit->Reserve>0;
   Type.HealthPermilleSum+=FMath::Clamp(FMath::FloorToInt(1000.f*Unit->HP/FMath::Max(1.f,Store.MaxHealthOf(MemberID))),0,1000);
   Type.Returning+=Unit->Action==EBattleAction::Returning;
   const int32 Health=FMath::Clamp(FMath::FloorToInt(1000.f*Unit->HP/FMath::Max(1.f,Store.MaxHealthOf(MemberID))),0,1000);
   bool Armed=Unit->Ammo>0||Unit->Reserve>0;
   if(Squad.SquadType==BattleSquadType::SPGun&&BattleTactics::SPGAmmoEnabled())Armed=Unit->SPGAmmo>0;
   Ready+=Health*(Armed?1000:250)/1000;
   ArmourArmed|=(Unit->Weapon==EBattleWeapon::AntiTank&&Armed)||Unit->Launcher>0;
  }
  Ready=FMath::Clamp(Ready/FMath::Max(1,BattleSquadType::Members(Squad.SquadType)),0,1000);
  // Retreat plans survive saves; do not use transient display/action state to make purchase decisions.
  if(Squad.TacticalPlan.Action==EBattleSquadTactic::Retreat)Ready=Ready*3/4;
  Type.Effective+=Ready;if(ArmourArmed)Type.AntiArmourEffective+=Ready;
  if(Squad.SquadType==BattleSquadType::Rifle||Squad.SquadType==BattleSquadType::Heavy||Squad.SquadType==BattleSquadType::Exo)Result.FrontEffective+=Ready;
  FBattleGroup Empty;const FBattleGroup* Group=&Empty;
  if(const int32* GroupID=Match.SquadGroup.Find(SquadID))if(const auto* Found=Match.Groups.Find(*GroupID))Group=Found;
  const uint8 Role=Match.GroupRole(*Group,SquadID);if(Role<5)Result.RoleEffective[Role]+=Ready;
 }
 // Read the already-recorded observation memory. Do not call KnownEnemyVehicles/AreaIntelView:
 // those const methods refresh mutable caches. Diagnostics must not alter the legacy path even when ON.
 const float Memory=FMath::Max(1.f,GetDefault<UBattleTuning>()->VehicleMemory);
 TArray<int32> ObservationIDs;Match.KnownVehicles[Team].GetKeys(ObservationIDs);ObservationIDs.Sort();
 for(int32 ObservationID:ObservationIDs)
 {
  const auto& Observation=Match.KnownVehicles[Team].FindChecked(ObservationID);
  Result.ObservedVehicles+=Observation.At<=Match.Elapsed&&Match.Elapsed-Observation.At<=Memory;
 }
 const int32 Keeps[]={0,0,ResolvedIntent.ScoutKeep,ResolvedIntent.ATKeep,ResolvedIntent.TankKeep,ResolvedIntent.ACKeep,ResolvedIntent.MortarKeep,ResolvedIntent.SPGKeep,ResolvedIntent.ExoKeep,0};
 const int32 Unlocked=Result.LegacyBought/FMath::Max(1,GetDefault<UBattleTuning>()->AreaScoutAfterHires);
 for(uint8 Type=BattleSquadType::Scout;Type<BattleSquadType::Count;++Type)
 {
  Result.Types[Type].Keep=Keeps[Type];
  Result.Types[Type].PacedTarget=Type==BattleSquadType::AntiTank&&bAutoTargets?Keeps[Type]:FMath::Min(Keeps[Type],Unlocked);
 }
 return Result;
}

FString BattleComposition::Describe(const FInput& Input)
{
 FString Result=FString::Printf(TEXT("stage=2 mode=planner team=%d time=%.3f remaining=%.3f auto_allowed=%d hire=%d composition=%d credits=%d pending=%d reserve=%d free=%d active=%d records=%d legacy_bought=%d legacy_heavy=%d heavy_share=%.4f observed_vehicles=%d interval=%.3f stop=%.3f last_hire=%.3f"),
  Input.Team,Input.Elapsed,Input.Remaining,Input.bAutoAllowed?1:0,Input.bHireEnabled?1:0,Input.bAutoTargets?1:0,Input.Credits,Input.Pending,Input.Reserve,Input.Free,Input.Active,Input.Records,Input.LegacyBought,Input.LegacyHeavy,Input.HeavyShare,Input.ObservedVehicles,Input.Interval,Input.Stop,Input.LastHireAt);
 for(uint8 Type=0;Type<BattleSquadType::Count;++Type)
 {
  const auto& Value=Input.Types[Type];
  Result+=FString::Printf(TEXT(" type%d=[active:%d,living:%d,armed:%d,returning:%d,hp_permille_sum:%d,keep:%d,paced:%d]"),Type,Value.Active,Value.Living,Value.Armed,Value.Returning,Value.HealthPermilleSum,Value.Keep,Value.PacedTarget);
 }
 return Result;
}

void BattleComposition::LogStart()
{
 const auto Options=Settings();if(!Options.bEnabled)return;
 if(Options.Teams==0){UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_COMPOSITION_CONFIG stage=2 planner=1 teams=0: no team selected; legacy purchasing unchanged"));return;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_CONFIG stage=2 planner=1 teams=%d team0=%d team1=%d purchase=planner"),Options.Teams,Options.Applies(0)?1:0,Options.Applies(1)?1:0);
 for(const auto& D:Definitions())UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_DEF type=%d cost=%d limit=%d members=%d role=%d allowed_roles=%d capabilities=%d ai_eligible=%d"),D.TypeID,D.Cost,D.HardActiveLimit,D.Members,D.DefaultGroupRole,D.AllowedGroupRoles,D.Capabilities,D.bAIEligible?1:0);
}

void BattleComposition::LogInput(const FInput& Input)
{
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COMPOSITION_INPUT %s"),*Describe(Input));
}
