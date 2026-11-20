// Armour AI, stage 3 (2026-11-18, user request: improve the AI that handles tanks, armoured cars and the other new squad types at the
// individual, squad, tactical and strategic layers). One switch, -BattleArmourAI=0/1 (UBattleTuning::bArmourAI, default 1;
// -BattleArmourAITeam=0/1 limits it to one team for one-team trials), covers the first three layers; every rule works only while a
// vehicle exists on the field, so matches without vehicles (the default: the AI buys none) are unchanged.
//  Individual: weapon-aware targets (small arms do not pick a target they cannot hurt, anti-tank gunners pick vehicles first and keep
//   their last rounds for them), a stopped vehicle turns its hull (and so its forward view) to its target or to the nearest known
//   enemy. Vehicle bodies take no infantry cover actions or stances regardless of this AI switch.
//  Squad: a vehicle backs out of the reach of a known anti-tank soldier, or after a heavy hit from somewhere unknown; infantry with no
//   anti-tank weapon falls back from a visible tank it cannot hurt; a regrouped vehicle resumes above its repair threshold.
//  Tactical: tanks support the point with the most known enemies from an overwatch post short of it, outside known anti-tank reach;
//   anti-tank teams go to the point nearest the enemy vehicles their team knows of.
//  Strategic (-BattleAreaComposition=0/1, UBattleTuning::bAreaComposition, default 0; the trial team with -BattleAreaTrialTeam): the AI
//   chooses its own mix: an armoured car and a mortar team early, a tank and a self-propelled gun later, anti-tank teams as soon as it
//   knows of enemy vehicles, a second tank against known armour.
// AI cognition: only what the team knows (UBattleMatch::AreaIntelView: own soldiers' sightings and scout or lookout reports). A known
// enemy is told apart as a vehicle or an anti-tank soldier only when it is a seen unit (report positions carry no type).
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleFieldLayout.h"
#include "BattleTactics.h"
#include "NavigationSystem.h"
#include "FutureBattle.h"

bool BattleTactics::ArmourAI(int32 Team)
{
 struct FSwitch{bool bOn=true;int32 Team=-1;};
 static const FSwitch S=[]{FSwitch V;int32 On=GetDefault<UBattleTuning>()->bArmourAI?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleArmourAI="),On);V.bOn=On!=0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleArmourAITeam="),V.Team);if(V.Team!=0&&V.Team!=1)V.Team=-1;return V;}();
 return S.bOn&&(S.Team<0||S.Team==Team);
}

bool BattleTactics::AreaComposition(int32 Team)
{
 static const bool bOn=[]{int32 V=GetDefault<UBattleTuning>()->bAreaComposition?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaComposition="),V);return V!=0;}();
 const int32 Trial=AreaStrategy().TrialTeam;
 return bOn&&(Trial<0||Trial==Team);
}

// ---------------------------------------------------------------- individual

void UBattleLeaderRuntime::PickArmourTargets(int32 SID,FBattleLeaderPlan& P,const TSet<int32>* AllowedTargets)
{
 // Runs after Think's nearest-contact pick, with the same filters, and replaces it only while the squad sees a vehicle.
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);const auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 P.ArmourTargetID=INDEX_NONE;if(!S||!Sense)return;
 const double ATReach2=FMath::Square(double(BattleRanges::Engage(EBattleWeapon::AntiTank)));
 bool bSeesVehicle=false;int32 Target=INDEX_NONE,Armour=INDEX_NONE;double Best=DBL_MAX,BestArmour=DBL_MAX;
 for(int32 ID:S->Members)
 {
  const auto* U=Store->Find(ID);const auto* Contacts=U&&U->IsAlive()?Sense->GetContacts(ID):nullptr;if(!Contacts)continue;
  const bool bGunner=U->Weapon==EBattleWeapon::AntiTank&&U->Ammo+U->Reserve>0;const FVector Pos=Position(ID);
  for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared)
  {
   if(AllowedTargets)
   {
    if(!AllowedTargets->Contains(C.Key))continue;
   }
   else
   {
    if(S->Order==EBattleOrder::Defend&&!BattleTactics::DefendEngage(S->Team)&&FVector::Dist2D(C.Value.LastKnownPosition,S->Destination)>S->DefenseRadius)continue;
    if(S->TacticalPlan.Action==EBattleSquadTactic::Support&&C.Key!=S->TacticalPlan.SupportTarget)continue;
   }
   const bool bVehicle=IsVehicle(C.Key);bSeesVehicle|=bVehicle;
   const double Range2=FVector::DistSquared(Pos,C.Value.LastKnownPosition);double D=FVector::DistSquared(C.Value.LastKnownPosition,P.Center);
   // An anti-tank gunner's vehicle target: within the anti-tank engagement distance, the nearest to the squad.
   if(bVehicle&&bGunner&&Range2<=ATReach2&&(D<BestArmour||(D==BestArmour&&C.Key<Armour))){BestArmour=D;Armour=C.Key;}
   if(bRangeAwareCombat&&Range2>FMath::Square(TargetRange(ID)))continue;
   if(bVehicle)
   {
    // Small arms: never a vehicle they cannot hurt; a lightly armoured one only when it is much nearer than the soldiers around it.
    const float Scale=BattleSquadType::ArmorSmallArms(VehicleType(C.Key));
    if(Scale<=0.f)continue;
    if(Scale<1.f)D*=4.;
   }
   if(D<Best||(D==Best&&C.Key<Target)){Best=D;Target=C.Key;}
  }
 }
 if(!bSeesVehicle)return;
 P.TargetID=Target;P.ArmourTargetID=Armour;
}

void UBattleLeaderRuntime::FaceVehicle(int32 ID,const FVector& Direction)
{
 ABattleUnit* A=Actor(ID);if(!A||Direction.IsNearlyZero())return;
 const float Dt=FMath::Min(GetWorld()->GetDeltaSeconds(),.1f);const float Rate=BattleSquadType::VehicleTurnRate(VehicleType(ID));
 const float Yaw=A->GetActorRotation().Yaw,Delta=FMath::FindDeltaAngleDegrees(Yaw,Direction.Rotation().Yaw);
 if(FMath::Abs(Delta)>1.f)A->SetActorRotation(FRotator(0,Yaw+FMath::Clamp(Delta,-Rate*Dt,Rate*Dt),0));
}

FVector UBattleLeaderRuntime::VehicleWatchDirection(int32 ID) const
{
 // Where a stopped vehicle with no target looks: the nearest enemy its team knows of, else the enemy home (a public place).
 const auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!M||!M->bArea||!U)return FVector::ZeroVector;
 const FVector Pos=Position(ID);const auto& V=M->AreaIntelView(U->Team);
 const FVector* Nearest=nullptr;double Best=DBL_MAX;
 for(const FVector& E:V.Enemies){const double D=FVector::DistSquared2D(Pos,E);if(D<Best){Best=D;Nearest=&E;}}
 return ((Nearest?*Nearest:M->AreaHome(1-U->Team))-Pos).GetSafeNormal2D();
}

// ---------------------------------------------------------------- team knowledge of enemy vehicles

void UBattleMatch::NoteKnownVehicles(int32 Team) const
{
 // Called when the team's knowledge is refreshed (AreaIntelView): seen enemy units that are vehicles, with the time and the place.
 const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!R||R->VehicleIDs.IsEmpty()||Team<0||Team>1)return;
 const auto& V=IntelViews[Team];
 for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(V.EnemyKeys[I]>0&&R->IsVehicle(V.EnemyKeys[I]))
 {auto& K=KnownVehicles[Team].FindOrAdd(V.EnemyKeys[I]);K.At=Elapsed;K.Position=V.Enemies[I];}
}

int32 UBattleMatch::KnownEnemyVehicles(int32 Team,TArray<FVector>* Positions) const
{
 const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!bArea||!R||R->VehicleIDs.IsEmpty()||Team<0||Team>1)return 0;
 AreaIntelView(Team);
 const float Memory=FMath::Max(1.f,GetDefault<UBattleTuning>()->VehicleMemory);int32 Count=0;
 TArray<int32> Keys;KnownVehicles[Team].GetKeys(Keys);Keys.Sort();
 for(int32 Key:Keys){const auto& K=KnownVehicles[Team][Key];if(K.At<=Elapsed&&Elapsed-K.At<=Memory){++Count;if(Positions)Positions->Add(K.Position);}}
 return Count;
}

// ---------------------------------------------------------------- squad

int32 UBattleMatch::AreaArmourReaction(int32 Team,int32 SquadID,float DamageRatio,FVector& Goal,FString& Why) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!bArea||!S||!R||R->VehicleIDs.IsEmpty())return 0;
 const auto* T=GetDefault<UBattleTuning>();const FVector Here=R->Position(S->LeaderUnitID),Home=AreaHome(Team);
 if(BattleSquadType::IsVehicle(S->SquadType))
 {
  // A vehicle keeps out of the reach of anti-tank soldiers it knows of (it outranges and outruns them), and pulls back after a heavy hit
  // (an anti-tank round or a tank gun) even when it does not know where it came from. Other tanks are fought or avoided by the team
  // layer (AreaCarTarget), not by backing away: distance does not protect from a tank gun.
  const double Keep=BattleRanges::Reach(EBattleWeapon::AntiTank)+FMath::Max(0.f,T->VehicleStandOffMargin);
  const auto& V=AreaIntelView(Team);const FVector* Threat=nullptr;double Nearest=DBL_MAX;
  for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)
  {
   const int32 Key=V.EnemyKeys[I];const auto* U=Key>0?Store->Find(Key):nullptr;
   if(!U||!U->IsAlive()||!U->CarriesAntiTank())continue;
   const double D=FVector::Dist2D(Here,V.Enemies[I]);if(D<=Keep&&D<Nearest){Nearest=D;Threat=&V.Enemies[I];}
  }
  const bool bShock=DamageRatio>=FMath::Max(.01f,T->VehicleShockRatio);
  if(!Threat&&!bShock)return 0;
  const FVector ToHome=(Home-Here).GetSafeNormal2D();
  FVector Away=Threat?((Here-*Threat).GetSafeNormal2D()+ToHome*.5).GetSafeNormal2D():ToHome;if(Away.IsNearlyZero())Away=ToHome;
  const double Distance=Threat?FMath::Max(2000.,Keep+2000.-Nearest):10000.;
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());const ANavigationData* Data=R->NavFor(S->LeaderUnitID);
  if(!Nav||Away.IsNearlyZero())return 0;
  FNavLocation L;
  if(!Nav->ProjectPointToNavigation(Here+Away*Distance,L,FVector(1500,1500,400),Data)&&!Nav->ProjectPointToNavigation(Here+ToHome*Distance,L,FVector(1500,1500,400),Data))return 0;
  Goal=L.Location;Why=Threat?TEXT("ANTI-TANK THREAT / STAND OFF"):TEXT("HEAVY HIT / PULL BACK");
  return 1;
 }
 // Infantry: a squad with no loaded anti-tank weapon does not stand in front of a tank it sees (small arms do nothing to it).
 const auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();if(!Sense)return 0;
 for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()&&((U->Weapon==EBattleWeapon::AntiTank&&U->Ammo+U->Reserve>0)||U->Launcher>0))return 0;
 const double Fear=FMath::Max(0.f,BattleRanges::Get().bActive?T->ArmourFearRangeProfile:T->ArmourFearRange);
 for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())if(const auto* Contacts=Sense->GetContacts(ID))
  for(const auto& C:*Contacts)
   if(C.Value.bVisible&&!C.Value.bShared&&R->IsVehicle(C.Key)&&BattleSquadType::ArmorSmallArms(R->VehicleType(C.Key))<=0.f&&FVector::Dist2D(Here,C.Value.LastKnownPosition)<=Fear)
   {Why=TEXT("ENEMY ARMOUR / NO ANTI-TANK WEAPON");return 2;}
 return 0;
}

// ---------------------------------------------------------------- tactical

int32 UBattleMatch::AreaVehicleChoice(int32 SquadID,TFunctionRef<double(int32)> ScoreOf) const
{
 // The best objective by ScoreOf, kept ACChoiceHold seconds unless it turned unsafe (-50 or worse) or another is clearly better.
 int32 Best=INDEX_NONE;double BestScore=-DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I){const double Sc=ScoreOf(I);if(Sc>BestScore){BestScore=Sc;Best=I;}}
 auto& C=CarChoices.FindOrAdd(SquadID);
 if(C.Target!=INDEX_NONE&&Objectives.IsValidIndex(C.Target)&&C.Target!=Best)
 {
  const double Current=ScoreOf(C.Target);
  const bool bUnsafe=Current<=-50.;
  if(!bUnsafe&&(Elapsed-C.At<FMath::Max(0.f,GetDefault<UBattleTuning>()->ACChoiceHold)||BestScore<Current+1.))return C.Target;
 }
 if(C.Target!=Best){C.Target=Best;C.At=Elapsed;}
 return Best;
}

FVector UBattleMatch::AreaTankPost(int32 Team,int32 Point,float Overwatch) const
{
 // Overwatch: TankOverwatch short of the point on the own side, further back (20 m steps) while a known anti-tank soldier reaches it.
 if(!Objectives.IsValidIndex(Point))return AreaHome(Team);
 const FVector P=Objectives[Point].Position,Back=(AreaHome(Team)-P).GetSafeNormal2D();
 const double Length=FVector::Dist2D(AreaHome(Team),P);
 double Step=FMath::Clamp(double(Overwatch>=0.f?Overwatch:GetDefault<UBattleTuning>()->TankOverwatch),0.,FMath::Max(0.,Length-3000.));
 for(int32 I=0;I<6&&Step+2000.<Length&&AreaArmourThreat(Team,P+Back*Step,false);++I)Step+=2000.;
 return P+Back*Step;
}

int32 UBattleMatch::AreaTankTarget(int32 Team,int32 SquadID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||Objectives.IsEmpty())return INDEX_NONE;
 const FVector Here=R->Position(S->LeaderUnitID);
 const bool bUrbanMap=BattleFieldLayout::IsUrban(GetWorld());
 const auto& V=AreaIntelView(Team);
 // Fire support where the team knows of the most enemies (+2, up to +3 more with their number), a point the team does not hold
 // +1.5, -0.3 per 100 m; the city of map E (slow, close quarters) and posts under known anti-tank reach only when nothing else is left.
 return AreaVehicleChoice(SquadID,[&](int32 I)->double
 {
  const FVector P=Objectives[I].Position;const int32 Known=V.EnemyAt.IsValidIndex(I)?V.EnemyAt[I]:0;
  double Sc=(Known>0?2.+FMath::Min(3.,Known/5.):0.)+(ObservedOwner(Team,I)!=Team?1.5:0.)-FVector::Dist2D(Here,P)/10000.*.3;
  if(bUrbanMap&&FMath::Abs(P.X)<25000&&FMath::Abs(P.Y)<15000)Sc-=100.;
  if(AreaArmourThreat(Team,AreaTankPost(Team,I),false))Sc-=50.;
  return Sc;
 });
}

int32 UBattleMatch::AreaATTarget(int32 Team,int32 SquadID) const
{
 // Anti-tank teams: with enemy vehicles known to the team, the point nearest one of them (own points a little preferred: the team
 // waits for the vehicle where it has cover and friends). INDEX_NONE without known vehicles (the caller keeps the normal spread).
 TArray<FVector> Vehicles;
 if(Objectives.IsEmpty()||KnownEnemyVehicles(Team,&Vehicles)==0)return INDEX_NONE;
 return AreaVehicleChoice(SquadID,[&](int32 I)->double
 {
  double Nearest=DBL_MAX;for(const FVector& P:Vehicles)Nearest=FMath::Min(Nearest,FVector::Dist2D(P,Objectives[I].Position));
  return (ObservedOwner(Team,I)==Team?1.:0.)-Nearest/10000.;
 });
}

// ---------------------------------------------------------------- strategic

void UBattleMatch::AreaComposition(int32 Team,FAreaIntent& I,int32 Bought) const
{
 // The numbers the AI keeps of each special type; UpdateAreaHiring paces them (one more per AreaScoutAfterHires purchases). An explicit
 // -BattleArea*Keep value wins. Own purchase count and known enemy vehicles only. First values (numbers delegated).
 const auto* T=GetDefault<UBattleTuning>();const auto& S=BattleTactics::AreaStrategy();
 const int32 Armour=KnownEnemyVehicles(Team);
 auto Cap=[](int32 Value,int32 Limit){return FMath::Clamp(Value,0,FMath::Max(0,Limit));};
 if(S.ACKeep<0)I.ACKeep=Cap(Bought>=12?2:1,T->AreaACLimit);
 if(S.MortarKeep<0)I.MortarKeep=Cap(Bought>=12?2:1,T->AreaMortarLimit);
 if(S.TankKeep<0)I.TankKeep=Cap(Bought>=6?(Armour>0?2:1):0,T->AreaTankLimit);
 if(S.SPGKeep<0)I.SPGKeep=Cap(Bought>=9?1:0,T->AreaSPGLimit);
 if(S.ATKeep<0)I.ATKeep=Cap(Armour,T->AreaATLimit);
}
