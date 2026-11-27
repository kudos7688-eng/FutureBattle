// Anti-tank range, anti-tank hunts and the armoured car shield (trials, 2026-11-23, user proposal: "tune the anti-tank soldiers; an
// armoured unit that finds an anti-tank soldier asks the infantry around it to remove it; the armoured car moves forward as a shield for
// the infantry"). Measured before (maps I/E/H): armoured cars stayed 110-400 m from enemies, out of their machine gun's 250 m, because they
// backed off 330 m from every known anti-tank soldier (launcher reach 300 m); moving their goal or post did not raise their kills.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "NavigationSystem.h"

int32 BattleTactics::ATRange()
{
 // -BattleATRange=<cm> (default 0 = the range profile's 300 m reach / 200 m engage): the launcher's reach; the engage distance is 3/4 of it.
 static const int32 Reach=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleATRange="),V);return V>0?FMath::Clamp(V,2000,30000):0;}();
 return Reach;
}

int32 BattleTactics::ATHunt()
{
 // 1: as measured 2026-11-23 (a hunt ends after 20 s unseen). 2: an unseen hunt goes on to the last seen position and ends there.
 static const int32 On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleATHunt="),V);return V==1||V==2?V:0;}();
 return On;
}

bool BattleTactics::ATFire()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleATFire="),V);return V==1;}();
 return On;
}

int32 BattleTactics::ACStandOff()
{
 static const int32 Keep=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleACStandOff="),V);return V>0?FMath::Clamp(V,2000,60000):0;}();
 return Keep;
}

int32 BattleTactics::ACTankStandOff()
{
 static const int32 Keep=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleACTankStandOff="),V);return V>0?FMath::Clamp(V,2000,100000):0;}();
 return Keep;
}

bool BattleTactics::ACShield()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleACShield="),V);return V==1;}();
 return On;
}

void UBattleMatch::UpdateATHunts(int32 Team)
{
 // Trial -BattleATHunt: an armoured unit (armoured car or tank) that sees an enemy anti-tank soldier itself (its own visible contact,
 // not a shared report) within 400 m asks the nearest one or two free rifle / heavy squads within 500 m of that soldier to attack it.
 // A hunt ends when the soldier is dead, unseen by the team for 20 s, after 60 s, or when the squad is no longer active.
 if(!bArea||(!BattleTactics::ATHunt()&&!BattleTactics::ATFire())||Team<0||Team>1)return;
 if(Elapsed<ATHuntClock[Team]){ATHunts.Reset();ATFireCalls[0].Reset();ATFireCalls[1].Reset();}
 // -BattleATFire: a call lapses 10 s after the last own armoured sighting.
 for(auto It=ATFireCalls[Team].CreateIterator();It;++It)if(Elapsed-It.Value()>10.)It.RemoveCurrent();
 ATHuntClock[Team]=Elapsed;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 if(!Store||!R||!Perception)return;
 const auto& V=AreaIntelView(Team);
 TArray<int32> HuntKeys;ATHunts.GetKeys(HuntKeys);HuntKeys.Sort();
 for(int32 HunterID:HuntKeys)
 {
  FATHunt& Hunt=ATHunts[HunterID];if(Hunt.Team!=Team)continue;
  const auto* Hunter=Store->FindSquad(HunterID);const auto* Target=Store->Find(Hunt.Target);
  for(int32 I=0;I<V.EnemyKeys.Num()&&I<V.Enemies.Num();++I)if(V.EnemyKeys[I]==Hunt.Target){Hunt.Position=V.Enemies[I];Hunt.SeenAt=Elapsed;break;}
  // Keep the real sighting clock separate: tracking cannot renew its own permission.
  const float TrackSeconds=BattleTactics::ATHunt()?BattleTactics::HuntTrack():0.f;
  const bool bTrackWindow=TrackSeconds>0.f&&Hunt.SeenAt>=0.&&Elapsed>=Hunt.SeenAt&&Elapsed-Hunt.SeenAt<=TrackSeconds;
  if(bTrackWindow&&Elapsed>Hunt.SeenAt&&Target&&Target->IsAlive())
  {
   Hunt.Position=R->Position(Hunt.Target);Hunt.TrackedAt=Elapsed;
   if(++Hunt.Tracked==1)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HUNT_TRACK kind=at team=%d squad=%d target=%d"),Team,HunterID,Hunt.Target);
  }
  const double LastLocatedAt=TrackSeconds>0.f?FMath::Max(Hunt.SeenAt,Hunt.TrackedAt):Hunt.SeenAt;
  // -BattleATHunt=2: an unseen soldier is searched for at his last seen position (ends when the squad leader is within 15 m of it).
  const bool bSearch=BattleTactics::ATHunt()==2;
  const bool bArrived=bSearch&&Hunter&&!bTrackWindow&&Elapsed-LastLocatedAt>2.&&FVector::DistSquared2D(R->Position(Hunter->LeaderUnitID),Hunt.Position)<FMath::Square(1500.);
  const TCHAR* End=!Target||!Target->IsAlive()?TEXT("killed"):!Hunter||Hunter->CommandStatus!=EBattleCommandStatus::Active?TEXT("inactive"):
   bArrived?TEXT("searched"):!bSearch&&!bTrackWindow&&Elapsed-LastLocatedAt>20.?TEXT("unseen"):Elapsed-Hunt.StartAt>60.?TEXT("timeout"):nullptr;
  if(End)
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AT_HUNT_END team=%d squad=%d target=%d reason=%s after=%.0f tracked=%d"),Team,HunterID,Hunt.Target,End,Elapsed-Hunt.StartAt,Hunt.Tracked);
   ATHunts.Remove(HunterID);
  }
 }
 TSet<int32> Hunted;for(const auto& Pair:ATHunts)Hunted.Add(Pair.Value.Target);
 TArray<int32> SquadKeys;Store->GetSquads().GetKeys(SquadKeys);SquadKeys.Sort();
 for(int32 ArmourID:SquadKeys)
 {
  const auto* Armour=Store->FindSquad(ArmourID);
  if(!Armour||Armour->Team!=Team||(Armour->SquadType!=BattleSquadType::ArmoredCar&&Armour->SquadType!=BattleSquadType::Tank)||Armour->CommandStatus!=EBattleCommandStatus::Active)continue;
  const auto* Leader=Store->Find(Armour->LeaderUnitID);if(!Leader||!Leader->IsAlive())continue;
  const auto* Contacts=Perception->GetContacts(Armour->LeaderUnitID);if(!Contacts)continue;
  const FVector ArmourAt=R->Position(Armour->LeaderUnitID);
  TArray<int32> ContactKeys;Contacts->GetKeys(ContactKeys);ContactKeys.Sort();
  for(int32 EnemyID:ContactKeys)
  {
   const auto& Contact=(*Contacts)[EnemyID];
   if(!Contact.bVisible||Contact.bShared)continue;
   const auto* Enemy=Store->Find(EnemyID);if(!Enemy||!Enemy->IsAlive()||Enemy->Team==Team||!Enemy->CarriesAntiTank())continue;
   const FVector EnemyAt=Contact.LastKnownPosition;if(FVector::DistSquared2D(ArmourAt,EnemyAt)>FMath::Square(40000.))continue;
   if(BattleTactics::ATFire())
   {
    if(!ATFireCalls[Team].Contains(EnemyID))UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AT_FIRE_CALL team=%d target=%d requester=%d"),Team,EnemyID,ArmourID);
    ATFireCalls[Team].Add(EnemyID,Elapsed);
   }
   if(!BattleTactics::ATHunt()||Hunted.Contains(EnemyID))continue;
   struct FPick{double Distance;int32 SquadID;};TArray<FPick> Picks;
   for(int32 CandidateID:SquadKeys)
   {
    const auto* C=Store->FindSquad(CandidateID);
    if(!C||C->Team!=Team||(C->SquadType!=BattleSquadType::Rifle&&C->SquadType!=BattleSquadType::Heavy)||C->CommandStatus!=EBattleCommandStatus::Active||
       (Team==0&&!C->bPlayerDelegated)||ATHunts.Contains(CandidateID)||ArtilleryRaids.Contains(CandidateID)||
       Area.HirePurpose.Contains(CandidateID)||IntelPurpose.Contains(CandidateID))continue;
    if(const FPlayerRoute* Route=PlayerRoutes.Find(CandidateID);Route&&Route->bMission)continue;
    if(const auto* Group=Groups.Find(GroupOf(CandidateID));Group&&Group->Kind==1)continue;
    const auto* CL=Store->Find(C->LeaderUnitID);if(!CL||!CL->IsAlive())continue;
    int32 Living=0;for(int32 M:C->Members)if(const auto* MU=Store->Find(M);MU&&MU->IsAlive())++Living;
    if(Living*2<C->Members.Num())continue;
    const double D=FVector::DistSquared2D(R->Position(C->LeaderUnitID),EnemyAt);if(D>FMath::Square(50000.))continue;
    Picks.Add({D,CandidateID});
   }
   Picks.Sort([](const FPick& A,const FPick& B){return A.Distance!=B.Distance?A.Distance<B.Distance:A.SquadID<B.SquadID;});
   for(int32 I=0;I<Picks.Num()&&I<2;++I)
   {
    FATHunt& Hunt=ATHunts.Add(Picks[I].SquadID);Hunt.Team=Team;Hunt.Target=EnemyID;Hunt.Position=EnemyAt;Hunt.StartAt=Hunt.SeenAt=Elapsed;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AT_HUNT team=%d squad=%d target=%d requester=%d range=%.0f"),Team,Picks[I].SquadID,EnemyID,ArmourID,FMath::Sqrt(Picks[I].Distance));
   }
   if(!Picks.IsEmpty())Hunted.Add(EnemyID);
  }
 }
}

void UBattleMatch::UpdateSupplyHunts(int32 Team)
{
 // Trial -BattleSupplyCarHunt (BattleTactics.h): observations only, except the opt-in HuntTrack position update below.
 if(!bArea||!BattleTactics::SupplyCarHunt()||Team<0||Team>1)return;
 if(Elapsed<SupplyHuntClock[Team]){SupplyHunts.Reset();SupplyHuntRest[0].Reset();SupplyHuntRest[1].Reset();SupplySeenLogged[0].Reset();SupplySeenLogged[1].Reset();}
 SupplyHuntClock[Team]=Elapsed;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return;
 const auto& V=AreaIntelView(Team);
 // The enemy supply vehicles the team sees now (unit keys only: a scout report dot has no identity or type). The type: observed
 // with -BattleObservedArmour, else that of the unit seen, as the vehicle checks of the default game.
 TMap<int32,FVector> Seen;
 for(int32 I=0;I<V.EnemyKeys.Num()&&I<V.Enemies.Num();++I)
 {
  const int32 Key=V.EnemyKeys[I];if(Key<=0)continue;
  const uint8 Type=BattleTactics::ObservedArmour()?V.ObservedType(Key):(R->IsVehicle(Key)?R->VehicleType(Key):uint8(255));
  if(Type!=BattleSquadType::Supply)continue;
  Seen.Add(Key,V.Enemies[I]);
  if(!SupplySeenLogged[Team].Contains(Key)){SupplySeenLogged[Team].Add(Key);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPLY_SEEN team=%d time=%.0f target=%d"),Team,Elapsed,Key);}
 }
 TArray<int32> HuntKeys;SupplyHunts.GetKeys(HuntKeys);HuntKeys.Sort();
 for(int32 HunterID:HuntKeys)
 {
  FSupplyHunt& Hunt=SupplyHunts[HunterID];if(Hunt.Team!=Team)continue;
  const auto* Hunter=Store->FindSquad(HunterID);
  if(const FVector* At=Seen.Find(Hunt.Target)){Hunt.Position=*At;Hunt.SeenAt=Elapsed;}
  // Read truth only for this hunt's one target and only inside the sighting window.
  const float TrackSeconds=BattleTactics::HuntTrack();
  const bool bTrackWindow=TrackSeconds>0.f&&Hunt.SeenAt>=0.&&Elapsed>=Hunt.SeenAt&&Elapsed-Hunt.SeenAt<=TrackSeconds;
  if(bTrackWindow&&Elapsed>Hunt.SeenAt)
  {
   const auto* TrackedTarget=Store->Find(Hunt.Target);
   if(TrackedTarget&&TrackedTarget->IsAlive())
   {
    Hunt.Position=R->Position(Hunt.Target);Hunt.TrackedAt=Elapsed;
    if(++Hunt.Tracked==1)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HUNT_TRACK kind=supply team=%d squad=%d target=%d"),Team,HunterID,Hunt.Target);
   }
  }
  // Do not infer death from missing samples; the search waits for the tracking window to end.
  const double LastLocatedAt=TrackSeconds>0.f?FMath::Max(Hunt.SeenAt,Hunt.TrackedAt):Hunt.SeenAt;
  const double Distance=Hunter?FVector::Dist2D(R->Position(Hunter->LeaderUnitID),Hunt.Position):0.;
  const bool bArrived=Hunter&&!bTrackWindow&&Elapsed-LastLocatedAt>2.&&Distance<1500.;
  // Log only (-BattleSupplyHuntLog, default off; STATUS 2026-11-26 21:10: the hunts do not get there): every 5 s where the squad
  // is, the order and mission it holds, its plan and what its leader does. Changes nothing in the match.
  static const bool bHuntLog=FParse::Param(FCommandLine::Get(),TEXT("BattleSupplyHuntLog"));
  if(bHuntLog&&Hunter&&(Elapsed-Hunt.TraceAt>=5.||Elapsed<Hunt.TraceAt))
  {
   Hunt.TraceAt=Elapsed;const auto* LU=Store->Find(Hunter->LeaderUnitID);const FVector HunterAt=R->Position(Hunter->LeaderUnitID);
   int32 Living=0;for(int32 M:Hunter->Members)if(const auto* MU=Store->Find(M);MU&&MU->IsAlive())++Living;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPLY_HUNT_TRACE t=%.0f squad=%d type=%d target=%d dist=%.0f unseen=%.0f here=(%.0f,%.0f) goal=(%.0f,%.0f) status=%d order=%d dest=(%.0f,%.0f) dest_to_goal=%.0f has_mission=%d mission_order=%d mission_dest=(%.0f,%.0f) mission_to_goal=%.0f plan=%d plan_dest_to_goal=%.0f leader_alive=%d leader_order=%d leader_action=%d leader_dest_to_goal=%.0f retries=%d living=%d plan_reason=%s"),
    Elapsed,HunterID,int32(Hunter->SquadType),Hunt.Target,Distance,Elapsed-Hunt.SeenAt,HunterAt.X,HunterAt.Y,Hunt.Position.X,Hunt.Position.Y,int32(Hunter->CommandStatus),int32(Hunter->Order),Hunter->Destination.X,Hunter->Destination.Y,
    FVector::Dist2D(Hunter->Destination,Hunt.Position),Hunter->bHasMission?1:0,int32(Hunter->MissionOrder),Hunter->MissionDestination.X,Hunter->MissionDestination.Y,FVector::Dist2D(Hunter->MissionDestination,Hunt.Position),
    int32(Hunter->TacticalPlan.Action),FVector::Dist2D(Hunter->TacticalPlan.Destination,Hunt.Position),LU&&LU->IsAlive()?1:0,LU?int32(LU->Order):-1,LU?int32(LU->Action):-1,LU?FVector::Dist2D(LU->Destination,Hunt.Position):-1.,LU?LU->MoveRetries:-1,Living,*Hunter->TacticalPlan.Reason);
  }
  // Every 30 s: 10 m nearer than at the last check, or the hunt is not getting there.
  bool bStalled=false;
  if(Elapsed-Hunt.CheckAt>=30.){bStalled=Distance>Hunt.CheckDistance-1000.&&Distance>=3000.;Hunt.CheckAt=Elapsed;Hunt.CheckDistance=Distance;}
  const bool bSupplyTrip=Hunter&&Hunter->TacticalPlan.Action==EBattleSquadTactic::Recover;
  const TCHAR* End=!Hunter||Hunter->CommandStatus!=EBattleCommandStatus::Active?TEXT("inactive"):bArrived?TEXT("searched"):bSupplyTrip?TEXT("supply"):
   bStalled?TEXT("stalled"):Elapsed-Hunt.StartAt>120.?TEXT("timeout"):nullptr;
  if(End)
  {
   if(!bArrived)SupplyHuntRest[Team].Add(Hunt.Target,Elapsed+60.);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPLY_HUNT_END team=%d squad=%d target=%d reason=%s after=%.0f unseen=%.0f tracked=%d"),Team,HunterID,Hunt.Target,End,Elapsed-Hunt.StartAt,Elapsed-Hunt.SeenAt,Hunt.Tracked);
   SupplyHunts.Remove(HunterID);
  }
 }
 TSet<int32> Hunted;for(const auto& Pair:SupplyHunts)if(Pair.Value.Team==Team)Hunted.Add(Pair.Value.Target);
 TArray<int32> SquadKeys;Store->GetSquads().GetKeys(SquadKeys);SquadKeys.Sort();
 TArray<int32> SeenKeys;Seen.GetKeys(SeenKeys);SeenKeys.Sort();
 for(int32 CarID:SeenKeys)
 {
  if(Hunted.Contains(CarID))continue;
  if(const double* Rest=SupplyHuntRest[Team].Find(CarID);Rest&&Elapsed<*Rest)continue;
  const FVector CarAt=Seen[CarID];
  struct FPick{int32 Kind;double Distance;int32 SquadID;};TArray<FPick> Picks;
  for(int32 CandidateID:SquadKeys)
  {
   const auto* C=Store->FindSquad(CandidateID);
   // =2: tanks and armoured cars too.
   const bool bHuntVehicle=BattleTactics::SupplyCarHunt()>=2&&(C&&(C->SquadType==BattleSquadType::Tank||C->SquadType==BattleSquadType::ArmoredCar));
   if(!C||C->Team!=Team||(!bHuntVehicle&&C->SquadType!=BattleSquadType::Rifle&&C->SquadType!=BattleSquadType::Heavy&&C->SquadType!=BattleSquadType::AntiTank&&C->SquadType!=BattleSquadType::Exo)||C->CommandStatus!=EBattleCommandStatus::Active||
      C->TacticalPlan.Action==EBattleSquadTactic::Recover||C->TacticalPlan.Action==EBattleSquadTactic::Retreat||C->TacticalPlan.Action==EBattleSquadTactic::Regroup||
      (Team==0&&!C->bPlayerDelegated)||SupplyHunts.Contains(CandidateID)||ATHunts.Contains(CandidateID)||ArtilleryRaids.Contains(CandidateID)||
      Area.HirePurpose.Contains(CandidateID)||IntelPurpose.Contains(CandidateID))continue;
   if(const FPlayerRoute* Route=PlayerRoutes.Find(CandidateID);Route&&Route->bMission)continue;
   if(const auto* Group=Groups.Find(GroupOf(CandidateID));Group&&Group->Kind==1)continue;
   const auto* CL=Store->Find(C->LeaderUnitID);if(!CL||!CL->IsAlive())continue;
   // At least half its members alive. Squads with an anti-tank weapon and a round for it come first.
   int32 Living=0;bool bAntiTank=false;
   for(int32 M:C->Members)if(const auto* MU=Store->Find(M);MU&&MU->IsAlive()){++Living;bAntiTank|=MU->Launcher>0||(MU->Weapon==EBattleWeapon::AntiTank&&MU->Ammo+MU->Reserve>0);}
   if(Living*2<C->Members.Num())continue;
   const double D=FVector::DistSquared2D(R->Position(C->LeaderUnitID),CarAt);if(D>FMath::Square(bHuntVehicle?80000.:50000.))continue;
   // The order of choice: a tank, an anti-tank weapon, an armoured car, small arms.
   Picks.Add({C->SquadType==BattleSquadType::Tank?0:bAntiTank?1:C->SquadType==BattleSquadType::ArmoredCar?2:3,D,CandidateID});
  }
  Picks.Sort([](const FPick& A,const FPick& B){return A.Kind!=B.Kind?A.Kind<B.Kind:A.Distance!=B.Distance?A.Distance<B.Distance:A.SquadID<B.SquadID;});
  for(int32 I=0;I<Picks.Num()&&I<2;++I)
  {
   FSupplyHunt& Hunt=SupplyHunts.Add(Picks[I].SquadID);Hunt.Team=Team;Hunt.Target=CarID;Hunt.Position=CarAt;Hunt.StartAt=Hunt.SeenAt=Hunt.CheckAt=Elapsed;Hunt.CheckDistance=FMath::Sqrt(Picks[I].Distance);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SUPPLY_HUNT team=%d time=%.0f squad=%d type=%d antitank=%d target=%d range=%.0f"),Team,Elapsed,Picks[I].SquadID,int32(Store->FindSquad(Picks[I].SquadID)->SquadType),Picks[I].Kind<=1?1:0,CarID,FMath::Sqrt(Picks[I].Distance));
  }
  if(!Picks.IsEmpty())Hunted.Add(CarID);
 }
}

bool UBattleMatch::SupplyHuntGoal(int32 Team,int32 SquadID,FVector& Center) const
{
 const FSupplyHunt* Hunt=SupplyHunts.Find(SquadID);if(!Hunt||Hunt->Team!=Team)return false;
 Center=Hunt->Position;return true;
}

bool UBattleMatch::ATHuntGoal(int32 Team,int32 SquadID,FVector& Center) const
{
 const FATHunt* Hunt=ATHunts.Find(SquadID);if(!Hunt||Hunt->Team!=Team)return false;
 Center=Hunt->Position;return true;
}

bool UBattleMatch::ACShieldPost(int32 Team,int32 SquadID,FVector& Post) const
{
 // Trial -BattleACShield: the car goes 40 m ahead of the nearest own rifle / heavy squad within 600 m that is attacking (its mission is
 // an attack move), toward the nearest enemy the team knows within 300 m of that squad, else toward the squad's destination.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Car=Store?Store->FindSquad(SquadID):nullptr;if(!Car||!R)return false;
 const FVector CarAt=R->Position(Car->LeaderUnitID);
 int32 Best=INDEX_NONE;double BestD=FMath::Square(60000.);
 TArray<int32> Keys;Store->GetSquads().GetKeys(Keys);Keys.Sort();
 for(int32 Key:Keys)
 {
  const auto* S=Store->FindSquad(Key);
  if(!S||S->Team!=Team||(S->SquadType!=BattleSquadType::Rifle&&S->SquadType!=BattleSquadType::Heavy)||S->CommandStatus!=EBattleCommandStatus::Active||
     !S->bHasMission||S->MissionOrder!=EBattleOrder::AttackMove)continue;
  const auto* L=Store->Find(S->LeaderUnitID);if(!L||!L->IsAlive())continue;
  const double D=FVector::DistSquared2D(CarAt,R->Position(S->LeaderUnitID));if(D<BestD){BestD=D;Best=Key;}
 }
 if(Best==INDEX_NONE)return false;
 const auto* Lead=Store->FindSquad(Best);const FVector LeadAt=R->Position(Lead->LeaderUnitID);
 const auto& V=AreaIntelView(Team);FVector Toward=FVector::ZeroVector;double Nearest=FMath::Square(30000.);
 for(const FVector& E:V.Enemies){const double D=FVector::DistSquared2D(LeadAt,E);if(D<Nearest){Nearest=D;Toward=E;}}
 if(Toward.IsZero())Toward=Lead->MissionDestination;
 const FVector Dir=(Toward-LeadAt).GetSafeNormal2D();if(Dir.IsNearlyZero())return false;
 Post=LeadAt+Dir*FMath::Min(4000.,FVector::Dist2D(LeadAt,Toward));
 BattleFieldLayout::Of(GetWorld()).Heights.OnField(Post);
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){FNavLocation L;if(Nav->ProjectPointToNavigation(Post,L,FVector(1500,1500,400),R->NavFor(Car->LeaderUnitID)))Post=L.Location;}
 return true;
}
