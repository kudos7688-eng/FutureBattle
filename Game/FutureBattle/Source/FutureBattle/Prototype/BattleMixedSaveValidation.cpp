#include "BattleSave.h"
#include "BattleFieldLayout.h"
#include "BattleTactics.h"
namespace {
bool Finite(double V,double Low,double High){return FMath::IsFinite(V)&&V>=Low&&V<=High;}
bool ValidPoint(FVector P,FVector2D Limit){return !P.ContainsNaN()&&FMath::Abs(P.X)<=Limit.X&&FMath::Abs(P.Y)<=Limit.Y&&Finite(P.Z,-1000,10000);}
}
bool UBattleSave::ValidateMixed(FString& Error) const
{
 auto Point=[&](FVector P){return ValidPoint(P,BattleFieldLayout::ByName(MapName).SaveLimit);};
 auto Fail=[&](const TCHAR* Why){Error=Why;return false;};
 if(!BattleFieldLayout::IsMixedSaveMap(MapName)) return Fail(TEXT("Unknown mixed map"));
 if((Ruleset!=TEXT("Wide")&&Ruleset!=TEXT("MixedFast")&&Ruleset!=TEXT("Area")) || !Finite(BaseSightRange,100,50000)||!Finite(WeaponRange,100,100000)||TacticalFeatures<0||TacticalFeatures>((90<<9)|511) ||
    !Finite(TacticalClock,0,1000)||!Finite(TeamNextAssignment,0,TacticalClock+1.1)||!Finite(DelegationClock,0,5.1))return Fail(TEXT("Invalid mixed rules/clocks"));
 if(Ruleset==TEXT("Wide")&&(Units.Num()!=50*BattleSquadType::RifleMembers() || Squads.Num()!=50))return Fail(TEXT("Fixed wide roster must contain 500 soldiers and 50 squads"));
 if(Ruleset==TEXT("MixedFast")&&(Units.Num()<20||Units.Num()>500||Units.Num()!=Squads.Num()*BattleSquadType::RifleMembers()))return Fail(TEXT("Invalid purchased roster"));
 TMap<int32,const FBattleSavedUnit*> ByID;
 TMap<int32,const FBattleSquadState*> BySquad;
 for(const auto& U:Units)ByID.Add(U.State.UnitID,&U);
 for(const auto& S:Squads)BySquad.Add(S.SquadID,&S);
 // Groups may still contain disabled squads until the next director update. Historical Roles may
 // also name squads that left the group. Preserve both; only current membership must be reciprocal.
 if(SavedGroups.Num()>Squads.Num()||SavedSquadGroup.Num()>Squads.Num()||SavedCarChoices.Num()>Squads.Num()||
    SavedNextGroupID<1||SavedNextGroupID>1000001)return Fail(TEXT("Invalid strategic collection sizes or next group ID"));
 auto Objective=[&](int32 ID){return ID==INDEX_NONE||Objectives.IsValidIndex(ID);};
 TSet<int32> GroupMembers;
 for(const auto& Pair:SavedGroups)
 {
  const auto& G=Pair.Value;
  if(Pair.Key!=G.ID||G.ID<1||G.ID>=SavedNextGroupID||G.Team<0||G.Team>1||G.Phase>3||
     G.Squads.Num()<(G.bPlayer?1:2)||G.Squads.Num()>Squads.Num()||G.Roles.Num()>Squads.Num()||
     !Objective(G.Objective)||!Objective(G.Avoid)||!Objective(G.StallPoint)||!Point(G.Rally)||
     !(G.StartStrength==-1||Finite(G.StartStrength,0,1.e9))||G.StartLiving<0||G.StartLiving>Units.Num()||G.Stalls<0||G.Stalls>1000000||G.Extensions<0||G.Extensions>1000000)
   return Fail(TEXT("Invalid saved group identity, phase or history"));
  // PushExtend can put PhaseAt up to 30 seconds ahead. Expired AvoidUntil must retain its signed offset.
  if(!Finite(G.PhaseAge,-30.01,double(Elapsed))||!Finite(G.ArrivalAge,0,double(Elapsed))||
     !Finite(G.AvoidRemaining,-double(Elapsed),3600.01))return Fail(TEXT("Invalid saved group relative clocks"));
  for(int32 SID:G.Squads)
  {
   const auto* S=BySquad.FindRef(SID);const int32* Of=SavedSquadGroup.Find(SID);
   if(!S||S->Team!=G.Team||GroupMembers.Contains(SID)||!Of||*Of!=G.ID)return Fail(TEXT("Broken saved group membership"));
   GroupMembers.Add(SID);
  }
  for(const auto& Role:G.Roles)
  {
   const auto* S=BySquad.FindRef(Role.Key);
   if(!S||S->Team!=G.Team||(Role.Value>4&&Role.Value!=255))return Fail(TEXT("Invalid saved group role"));
  }
 }
 if(GroupMembers.Num()!=SavedSquadGroup.Num())return Fail(TEXT("Extra saved squad-to-group reference"));
 for(const auto& Pair:SavedCarChoices)
 {
  if(!BySquad.Contains(Pair.Key)||!Objective(Pair.Value.Target)||
     !Finite(Pair.Value.Age,0,double(Elapsed))||(!Pair.Value.bHasTime&&(Pair.Value.Age!=0||Pair.Value.Target!=INDEX_NONE)))
   return Fail(TEXT("Invalid saved armour choice or relative clock"));
 }
 // Check every record, including retired soldiers which are no longer in Members.
 for(const auto& E:Units)
 {
  const auto* S=BySquad.FindRef(E.State.SquadID);const auto& U=E.State;
  if(!S||S->SquadType>=BattleSquadType::Count)return Fail(TEXT("Invalid equipment squad"));
  const bool Exo=S->SquadType==BattleSquadType::Exo;
  const bool SPG=S->SquadType==BattleSquadType::SPGun;
  if(U.SPGAmmo<0||U.SPGAmmo>FMath::Max(0,GetDefault<UBattleTuning>()->SPGAmmoCapacity)||
     ((!SPG||CombatStateVersion<3||(ArtilleryTrialFlags&2)==0)&&U.SPGAmmo!=0))
   return Fail(TEXT("Invalid SPG ammunition or extension version"));
  if(!Finite(U.Shield,0,BattleSquadType::ShieldMax(S->SquadType))||!Finite(U.Launcher,0,BattleSquadType::LauncherRounds(S->SquadType))||
     !Finite(U.ShieldWait,0,Exo?BattleTactics::ExoShieldHitDelay():0.f)||
     !Finite(U.LauncherWait,0,Exo?FMath::Max(.5f,GetDefault<UBattleTuning>()->ExoLauncherReload):0.f)||(!Exo&&U.bLauncherSelected))
   return Fail(TEXT("Shield or launcher inconsistent with squad type"));
 }
 if(SavedArtillery.Num()>Units.Num()||SavedArtilleryShells.Num()>100000)return Fail(TEXT("Invalid artillery collection size"));
 auto ArtilleryType=[&](int32 ID,bool bSPG)
 {
  const auto* U=ByID.FindRef(ID);const auto* S=U?BySquad.FindRef(U->State.SquadID):nullptr;
  return S&&(bSPG?(S->SquadType==BattleSquadType::SPGun&&S->LeaderUnitID==ID):
   (S->SquadType==BattleSquadType::Mortar&&U->State.Weapon==EBattleWeapon::Mortar));
 };
 // Landing spread and observed targets may lie beyond the playable map's save bounds.
 auto ArtilleryPoint=[&](const FVector& P){return Finite(P.X,-1.e9,1.e9)&&Finite(P.Y,-1.e9,1.e9)&&Finite(P.Z,-1.e9,1.e9);};
 for(const auto& Pair:SavedArtillery)
 {
  const auto& A=Pair.Value;
  if((!ArtilleryType(Pair.Key,false)&&!ArtilleryType(Pair.Key,true))||!Finite(A.SetupElapsed,0,1000000)||
     (!A.bSettingUp&&A.SetupElapsed!=0)||!Finite(A.ReloadRemaining,0,1000000)||!Finite(A.PickRemaining,0,1000000)||
     !BattleSaveValidation::Counter(A.Shots)||!ArtilleryPoint(A.Aim))return Fail(TEXT("Invalid saved artillery"));
  if(A.TargetKey>0)
  {
   const auto* Target=ByID.FindRef(A.TargetKey);const auto* Shooter=ByID.FindRef(Pair.Key);
   if(!Target||!Shooter||Target->State.Team==Shooter->State.Team)return Fail(TEXT("Invalid artillery observed target"));
  }
 }
 for(const auto& Shell:SavedArtilleryShells)
 {
  const auto* U=ByID.FindRef(Shell.ShooterID);
  if(!U||!ArtilleryType(Shell.ShooterID,Shell.bSPG)||Shell.Team!=U->State.Team||
     Shell.ShooterType!=(Shell.bSPG?BattleSquadType::SPGun:BattleSquadType::Mortar)||
     !ArtilleryPoint(Shell.At)||!Finite(Shell.Remaining,0,1000000))return Fail(TEXT("Invalid saved artillery shell"));
 }
 if(Ruleset==TEXT("Area"))
 {
  int32 Current=0;for(const auto& U:Units)Current+=!U.State.bRetired;
  // Every squad keeps its type's size (replacements retire the dead soldier): 50 starting rifle squads plus purchases.
  int32 ExpectedRoster=0;for(const auto& S:Squads)ExpectedRoster+=S.SquadType<BattleSquadType::Count?BattleSquadType::Members(S.SquadType):0;
  int32 Retired[2]={0,0};for(const auto& U:Units)if(U.State.bRetired)
  {const auto* S=BySquad.FindRef(U.State.SquadID);if(!S||S->Team!=U.State.Team)return Fail(TEXT("Invalid retired squad reference"));++Retired[U.State.Team];}
  // New-squad hiring (2026-11-14): each team starts with 25 squads; every extra squad is a purchase of 10 fresh soldiers,
  // at most BattleTactics::AreaHire().MaxActive (default 25) active per team, and resources spent on hires join the conservation law. Pre-hiring saves have none.
  int32 TeamSquads[2]={0,0},TeamActive[2]={0,0};
  for(const auto& S:Squads){if(S.Team<0||S.Team>1)return Fail(TEXT("Invalid squad team"));++TeamSquads[S.Team];TeamActive[S.Team]+=S.CommandStatus==EBattleCommandStatus::Active;}
  if(Area.HireSpent.Num()!=2)return Fail(TEXT("Invalid area hire record"));
  for(int32 Team=0;Team<2;++Team)
  {
   const int32 Hires=TeamSquads[Team]-25;
   if(Hires<0||TeamActive[Team]>BattleTactics::AreaHire().MaxActive||Area.HireSpent[Team]<Hires||Area.HireSpent[Team]>100100||(Hires==0)!=(Area.HireSpent[Team]==0))return Fail(TEXT("Invalid area squad hiring"));
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
  for(const auto& Q:Area.SupplyStock){const auto* S=BySquad.FindRef(Q.Key);if(!S||S->SquadType!=BattleSquadType::Supply||Q.Value<0||Q.Value>100000)return Fail(TEXT("Invalid supply vehicle stock"));}
 }
 if(Squads.Num()>(Ruleset==TEXT("Area")?400:50)||SavedPlans.Num()!=Squads.Num()||TacticalSamples.Num()>Squads.Num()||NextSamples.Num()>Squads.Num()||TeamRetryAfter.Num()>Squads.Num()||DamageBuckets.Num()>Squads.Num()*7)return Fail(TEXT("Invalid mixed collection sizes"));
 for(const auto& S:Squads)
 {
  const auto* Leader=ByID.FindRef(S.LeaderUnitID);
  if(!Leader||!S.Members.Contains(S.LeaderUnitID)||S.SquadType>=BattleSquadType::Count||S.Members.Num()!=BattleSquadType::Members(S.SquadType)||
    (S.CommandStatus!=EBattleCommandStatus::Active&&S.CommandStatus!=EBattleCommandStatus::Disabled)||S.DisplayName.Len()>80||!BattleSaveValidation::Counter(S.CommandRevision)||
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
    !Finite(P->Remaining,0,60)||!BattleSaveValidation::Counter(P->Revision)||P->Reason.Len()>160||!Finite(P->Age,0,1000)||!Finite(P->SafeSeconds,0,1000)||
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
   if(C.TargetID!=Contact.Key||!BattleSaveValidation::Contact(C,S->Team,Source?&Source->State:nullptr,Target?&Target->State:nullptr,
     BattleFieldLayout::ByName(MapName).SaveLimit))return Fail(TEXT("Invalid observed contact"));
  }
 }
 for(const auto& P:NextSamples)if(!BySquad.Contains(P.Key)||!Finite(P.Value,0,TacticalClock+1.1))return Fail(TEXT("Invalid tactical scheduling"));
 for(const auto& P:TeamRetryAfter)if(!BySquad.Contains(P.Key)||!Finite(P.Value,0,TacticalClock+60))return Fail(TEXT("Invalid team scheduling"));
 // RecordDamage prunes ages >5 before appending: at most six distinct integer seconds.
 // Capture preserves each squad's array order (the order between squads is immaterial).
 TMap<int32,double> LastDamageBucket;
 TMap<int32,int32> DamageBucketCounts;
 for(const auto& D:DamageBuckets)
 {
  if(!BySquad.Contains(D.SquadID)||!Finite(D.Bucket,0,TacticalClock)||
     D.Bucket!=FMath::FloorToDouble(D.Bucket)||!Finite(D.Damage,0,100000))return Fail(TEXT("Invalid damage history"));
  const double* PreviousBucket=LastDamageBucket.Find(D.SquadID);
  if((PreviousBucket&&D.Bucket<=*PreviousBucket)||++DamageBucketCounts.FindOrAdd(D.SquadID)>6)
   return Fail(TEXT("Invalid damage history"));
  LastDamageBucket.Add(D.SquadID,D.Bucket);
 }
 return true;
}

