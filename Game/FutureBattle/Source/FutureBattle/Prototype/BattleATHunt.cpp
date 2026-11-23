// Anti-tank range, anti-tank hunts and the armoured car shield (trials, 2026-11-23, user proposal: "tune the anti-tank soldiers; an
// armoured unit that finds an anti-tank soldier asks the infantry around it to remove it; the armoured car moves forward as a shield for
// the infantry"). Measured before (maps I/E/H): armoured cars stayed 110-400 m from enemies, out of their machine gun's 250 m, because they
// backed off 330 m from every known anti-tank soldier (launcher reach 300 m); moving their goal or post did not raise their kills.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
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
 if(!bArea||!BattleTactics::ATHunt()||Team<0||Team>1)return;
 if(Elapsed<ATHuntClock[Team])ATHunts.Reset();
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
  // -BattleATHunt=2: an unseen soldier is searched for at his last seen position (ends when the squad leader is within 15 m of it).
  const bool bSearch=BattleTactics::ATHunt()==2;
  const bool bArrived=bSearch&&Hunter&&Elapsed-Hunt.SeenAt>2.&&FVector::DistSquared2D(R->Position(Hunter->LeaderUnitID),Hunt.Position)<FMath::Square(1500.);
  const TCHAR* End=!Target||!Target->IsAlive()?TEXT("killed"):!Hunter||Hunter->CommandStatus!=EBattleCommandStatus::Active?TEXT("inactive"):
   bArrived?TEXT("searched"):!bSearch&&Elapsed-Hunt.SeenAt>20.?TEXT("unseen"):Elapsed-Hunt.StartAt>60.?TEXT("timeout"):nullptr;
  if(End)
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AT_HUNT_END team=%d squad=%d target=%d reason=%s after=%.0f"),Team,HunterID,Hunt.Target,End,Elapsed-Hunt.StartAt);
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
   if(!Contact.bVisible||Contact.bShared||Hunted.Contains(EnemyID))continue;
   const auto* Enemy=Store->Find(EnemyID);if(!Enemy||!Enemy->IsAlive()||Enemy->Team==Team||!Enemy->CarriesAntiTank())continue;
   const FVector EnemyAt=Contact.LastKnownPosition;if(FVector::DistSquared2D(ArmourAt,EnemyAt)>FMath::Square(40000.))continue;
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
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){FNavLocation L;if(Nav->ProjectPointToNavigation(Post,L,FVector(1500,1500,400),R->NavFor(Car->LeaderUnitID)))Post=L.Location;}
 return true;
}
