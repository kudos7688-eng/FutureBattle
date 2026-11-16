#include "BattleSave.h"
#include "BattleFieldLayout.h"
namespace {
bool Finite(double V,double Low,double High){return FMath::IsFinite(V)&&V>=Low&&V<=High;}
bool ValidPoint(FVector P,bool Expanded,bool Urban){return !P.ContainsNaN()&&FMath::Abs(P.X)<=(Urban?52000:Expanded?42000:32000)&&FMath::Abs(P.Y)<=(Urban?37000:Expanded?32000:22000)&&Finite(P.Z,-1000,10000);}
}
bool UBattleSave::ValidateMixed(FString& Error) const
{
 auto Point=[&](FVector P){return ValidPoint(P,MapName==TEXT("Lvl_BattleWide_D"),BattleFieldLayout::IsUrbanMap(MapName));};
 auto Fail=[&](const TCHAR* Why){Error=Why;return false;};
 if(MapName!=TEXT("Lvl_BattleField500")&&MapName!=TEXT("Lvl_BattleWide_A")&&MapName!=TEXT("Lvl_BattleWide_B")&&MapName!=TEXT("Lvl_BattleWide_C")&&MapName!=TEXT("Lvl_BattleWide_D")&&MapName!=TEXT("Lvl_BattleWide_E")) return Fail(TEXT("Unknown mixed map"));
 if((Ruleset!=TEXT("Wide")&&Ruleset!=TEXT("MixedFast")&&Ruleset!=TEXT("Area")) || !Finite(BaseSightRange,100,50000)||!Finite(WeaponRange,100,100000)||TacticalFeatures<0||TacticalFeatures>((90<<9)|511) ||
    !Finite(TacticalClock,0,1000)||!Finite(TeamNextAssignment,0,TacticalClock+1.1)||!Finite(DelegationClock,0,5.1))return Fail(TEXT("Invalid mixed rules/clocks"));
 if(Ruleset==TEXT("Wide")&&(Units.Num()!=500 || Squads.Num()!=50))return Fail(TEXT("Fixed wide roster must contain 500 soldiers and 50 squads"));
 if(Ruleset==TEXT("MixedFast")&&(Units.Num()<20||Units.Num()>500||Units.Num()!=Squads.Num()*10))return Fail(TEXT("Invalid purchased roster"));
 TMap<int32,const FBattleSavedUnit*> ByID;
 TMap<int32,const FBattleSquadState*> BySquad;
 for(const auto& U:Units)ByID.Add(U.State.UnitID,&U);
 for(const auto& S:Squads)BySquad.Add(S.SquadID,&S);
 if(Ruleset==TEXT("Area"))
 {
  int32 Current=0;for(const auto& U:Units)Current+=!U.State.bRetired;
  // Every squad keeps its type's size (replacements retire the dead soldier): 50 starting rifle squads plus purchases.
  int32 ExpectedRoster=0;for(const auto& S:Squads)ExpectedRoster+=S.SquadType<BattleSquadType::Count?BattleSquadType::Members(S.SquadType):0;
  int32 Retired[2]={0,0};for(const auto& U:Units)if(U.State.bRetired)
  {const auto* S=BySquad.FindRef(U.State.SquadID);if(!S||S->Team!=U.State.Team)return Fail(TEXT("Invalid retired squad reference"));++Retired[U.State.Team];}
  // New-squad hiring (2026-11-14): each team starts with 25 squads; every extra squad is a purchase of 10 fresh soldiers,
  // at most 25 active per team, and resources spent on hires join the conservation law. Pre-hiring saves have none.
  int32 TeamSquads[2]={0,0},TeamActive[2]={0,0};
  for(const auto& S:Squads){if(S.Team<0||S.Team>1)return Fail(TEXT("Invalid squad team"));++TeamSquads[S.Team];TeamActive[S.Team]+=S.CommandStatus==EBattleCommandStatus::Active;}
  if(Area.HireSpent.Num()!=2)return Fail(TEXT("Invalid area hire record"));
  for(int32 Team=0;Team<2;++Team)
  {
   const int32 Hires=TeamSquads[Team]-25;
   if(Hires<0||TeamActive[Team]>25||Area.HireSpent[Team]<Hires||Area.HireSpent[Team]>100100||(Hires==0)!=(Area.HireSpent[Team]==0))return Fail(TEXT("Invalid area squad hiring"));
  }
  // Income (2026-11-14) adds to the starting 100. Pre-income saves have Income {0,0} and IncomeClock 0.
  if(Area.Income.Num()!=2||Area.Income[0]<0||Area.Income[1]<0||Area.Income[0]>100000||Area.Income[1]>100000||!Finite(Area.IncomeClock,0,600))return Fail(TEXT("Invalid area income record"));
  // War score trial: older saves have {0,0} and clock 0. Range checks only (losses are not reconstructed from the records).
  if(Area.WarPoint.Num()!=2||Area.WarLoss.Num()!=2||Area.WarPointEstimate.Num()!=2||!Finite(Area.WarClock,0,600))return Fail(TEXT("Invalid area war score record"));
  for(int32 Team=0;Team<2;++Team)if(Area.WarPoint[Team]<0||Area.WarPoint[Team]>1000000||Area.WarLoss[Team]<0||Area.WarLoss[Team]>1000000||Area.WarPointEstimate[Team]<0||Area.WarPointEstimate[Team]>1000000)return Fail(TEXT("Invalid area war score"));
  if(Area.LastAIHireAt.Num()!=2||!Finite(Area.LastAIHireAt[0],-1000,Elapsed+.01)||!Finite(Area.LastAIHireAt[1],-1000,Elapsed+.01))return Fail(TEXT("Invalid area AI hire clock"));
  // P1 trial (2026-11-16): bought-squad missions point at existing squads with a known mission; home hit times are past clocks.
  for(const auto& P:Area.HirePurpose)if(!((P.Value>=0&&P.Value<Objectives.Num())||P.Value==100||P.Value==101)||!BySquad.FindRef(P.Key))return Fail(TEXT("Invalid area hire purpose"));
  if(Area.HomeHitAt.Num()!=2||!Finite(Area.HomeHitAt[0],-1000,Elapsed+.01)||!Finite(Area.HomeHitAt[1],-1000,Elapsed+.01))return Fail(TEXT("Invalid area home hit clock"));
  if(Retired[0]+Area.HireSpent[0]+AllyCredits!=100+Area.Income[0]||Retired[1]+Area.HireSpent[1]+EnemyCredits!=100+Area.Income[1])return Fail(TEXT("Area resource conservation mismatch"));
  if(Version!=3||MapName==TEXT("Lvl_BattleField500")||Current!=ExpectedRoster||Area.BaseHP.Num()!=2||Area.KnownEnemyBaseHP.Num()!=2||Area.KnownOwners.Num()!=2*Objectives.Num()||Area.SeenAt.Num()!=2*Objectives.Num()||!Finite(AllyCredits,0,100+Area.Income[0])||!Finite(EnemyCredits,0,100+Area.Income[1])||!Finite(Area.ObserveClock,0,.21)||!Finite(Area.SupplyClock,0,1.1)||Area.Replacements.Num()>200||Area.AmmoTimers.Num()>Squads.Num())return Fail(TEXT("Invalid area state"));
  for(int32 Team=0;Team<2;++Team)if(!Finite(Area.BaseHP[Team],0,1000)||!Finite(Area.KnownEnemyBaseHP[Team],-1,1000))return Fail(TEXT("Invalid base state"));
  for(int32 I=0;I<Area.KnownOwners.Num();++I)if(Area.KnownOwners[I]<-2||Area.KnownOwners[I]>1||!Finite(Area.SeenAt[I],-1,Elapsed+.01))return Fail(TEXT("Invalid observed objective"));
  int32 Reserved[2]={0,0};
  for(const auto& Q:Area.Replacements)
  {
   const auto* U=ByID.FindRef(Q.Key);const auto* S=U?BySquad.FindRef(U->State.SquadID):nullptr;
   if(!U||U->State.IsAlive()||U->State.bRetired||U->State.bActorRepresentation||!S||S->CommandStatus!=EBattleCommandStatus::Active||!Finite(Q.Value,0,10))return Fail(TEXT("Invalid replenishment queue"));
   ++Reserved[U->State.Team];
  }
  if(Reserved[0]>AllyCredits||Reserved[1]>EnemyCredits)return Fail(TEXT("Overbooked replenishment resources"));
  for(const auto& Q:Area.AmmoTimers)if(!BySquad.Contains(Q.Key)||!Finite(Q.Value,0,10))return Fail(TEXT("Invalid home ammunition timer"));
 }
 if(Squads.Num()>(Ruleset==TEXT("Area")?100:50)||SavedPlans.Num()!=Squads.Num()||TacticalSamples.Num()>Squads.Num()||NextSamples.Num()>Squads.Num()||TeamRetryAfter.Num()>Squads.Num()||DamageBuckets.Num()>Squads.Num()*7)return Fail(TEXT("Invalid mixed collection sizes"));
 for(const auto& S:Squads)
 {
  const auto* Leader=ByID.FindRef(S.LeaderUnitID);
  if(!Leader||!S.Members.Contains(S.LeaderUnitID)||S.SquadType>=BattleSquadType::Count||S.Members.Num()!=BattleSquadType::Members(S.SquadType)||
    (S.CommandStatus!=EBattleCommandStatus::Active&&S.CommandStatus!=EBattleCommandStatus::Disabled)||S.DisplayName.Len()>80||S.CommandRevision<0||
    !Finite(S.FlankCooldown,0,15.1))return Fail(TEXT("Invalid leader squad"));
  for(int32 ID:S.Members)
  {
   const auto& U=ByID.FindChecked(ID)->State;
   // A scout pair that handed over command keeps its dead former leader as an Actor body (no other squad has two Actors).
   const bool HandedOver=BattleSquadType::LeaderHandover(S.SquadType)&&ID!=S.LeaderUnitID&&!U.IsAlive()&&U.bActorRepresentation;
   if(!U.bLeaderManaged || (U.bActorRepresentation!=(ID==S.LeaderUnitID)&&!HandedOver) ||
      (U.bCanPlayerControl && (ID!=S.LeaderUnitID||U.Team!=0))||
      (S.CommandStatus==EBattleCommandStatus::Disabled && U.IsAlive()))return Fail(TEXT("Invalid Actor/Mass representation"));
  }
  if((S.CommandStatus==EBattleCommandStatus::Active)!=Leader->State.IsAlive() ||
     (S.CommandStatus==EBattleCommandStatus::Disabled&&S.bPlayerDelegated))return Fail(TEXT("Invalid dead leader state"));
  const auto* P=SavedPlans.Find(S.SquadID);
  if(!P||static_cast<uint8>(P->Action)>static_cast<uint8>(EBattleSquadTactic::Recover)||!Point(P->Destination)||!Point(P->FlankContact)||
    !Finite(P->Remaining,0,60)||P->Revision<0||P->Reason.Len()>160||!Finite(P->Age,0,1000)||!Finite(P->SafeSeconds,0,1000)||
    !Finite(P->FlankSupportChanges,0,1)||!Finite(P->RetrySeconds,0,600)||!Finite(P->FlankExposure,0,1)||!Finite(P->SupportUntil,0,TacticalClock+20.1)||!Finite(P->LastSupportShot,-1,TacticalClock+.01))return Fail(TEXT("Invalid saved tactical plan"));
  auto Ally=[&](int32 ID){const auto* Other=BySquad.FindRef(ID);return Other&&ID!=S.SquadID&&Other->Team==S.Team;};
  auto Enemy=[&](int32 ID){const auto* Other=ByID.FindRef(ID);return Other&&Other->State.Team!=S.Team;};
  if(P->Action==EBattleSquadTactic::Support && (!Ally(P->SupportSquad)||!Enemy(P->SupportTarget)))return Fail(TEXT("Invalid support references"));
  if(P->Action==EBattleSquadTactic::Flank && (!Enemy(P->FlankTarget)||(P->FlankSupport!=INDEX_NONE&&!Ally(P->FlankSupport))))return Fail(TEXT("Invalid flank references"));
  if(S.CommandStatus==EBattleCommandStatus::Disabled&&P->Action!=EBattleSquadTactic::Mission)return Fail(TEXT("Dead squad has active plan"));
 }
 for(int32 ID:{SelectedID,ControlledID})if(ID!=INDEX_NONE)
 {
  const auto* E=ByID.FindRef(ID);
  if(!E||!E->State.bActorRepresentation || !E->State.bCanPlayerControl)return Fail(TEXT("Player target is not a controllable leader"));
 }
 for(const auto& Pair:TacticalSamples)
 {
  const auto* S=BySquad.FindRef(Pair.Key);const auto& O=Pair.Value;
  if(!S||!Finite(O.SampleTime,0,TacticalClock+.01)||!Finite(O.Health,0,100000)||!Finite(O.RecentDamage,0,100000)||
     O.Reason.Len()>160||O.Contacts.Num()>(Version==3?1000:500))return Fail(TEXT("Invalid tactical observation"));
  for(int32 N:{O.Living,O.Ready,O.Armed,O.Direct,O.Cover,O.Support,O.Flank,O.Failed})if(N<0||N>10)return Fail(TEXT("Invalid observation count"));
  if(O.Ready>O.Living||O.Armed>O.Living||O.Ammo<0||O.Ammo>100000||O.Reserve<0||O.Reserve>1000000)return Fail(TEXT("Invalid observation resources"));
  for(const auto& Contact:O.Contacts)
  {
   const auto& C=Contact.Value;const auto* Source=ByID.FindRef(C.SourceUnitID);const auto* Target=ByID.FindRef(C.TargetID);
   if(C.TargetID!=Contact.Key||!Source||!Target||Source->State.Team!=S->Team||Target->State.Team==S->Team||!Point(C.LastKnownPosition)||
     !Finite(C.LastSeenTime,-1000000,.01)||!Finite(C.Confidence,0,1))return Fail(TEXT("Invalid observed contact"));
  }
 }
 for(const auto& P:NextSamples)if(!BySquad.Contains(P.Key)||!Finite(P.Value,0,TacticalClock+1.1))return Fail(TEXT("Invalid tactical scheduling"));
 for(const auto& P:TeamRetryAfter)if(!BySquad.Contains(P.Key)||!Finite(P.Value,0,TacticalClock+60))return Fail(TEXT("Invalid team scheduling"));
 for(const auto& D:DamageBuckets)if(!BySquad.Contains(D.SquadID)||!Finite(D.Bucket,0,TacticalClock)||!Finite(D.Damage,0,100000))return Fail(TEXT("Invalid damage history"));
 return true;
}

