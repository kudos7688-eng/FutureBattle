#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "FutureBattle.h"
#include "BattlePerception.h"

FVector UBattleMatch::AreaHome(int32 Team)const{return FVector((Team!=int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"))))?BattleFieldLayout::HomeDistance(GetWorld()):-BattleFieldLayout::HomeDistance(GetWorld()),0,0);}
int32 UBattleMatch::ObservedOwner(int32 Team,int32 Point)const{return bArea?Area.KnownOwners[Team*3+Point]:Objectives[Point].Owner;}
int32 UBattleMatch::AreaScore(int32 Team)const
{
 int32 Total=Credits[Team];for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())++Total;
 for(const auto& P:Objectives)if(P.Owner==Team)Total+=20;return Total;
}
void UBattleMatch::InitializeArea()
{
 Credits[0]=Credits[1]=BattleTactics::AreaIncome().StartCredits;Area=FBattleAreaState();Score[0]=Score[1]=0;
 for(int32 Team=0;Team<2;++Team)
 {
  auto* Base=GetWorld()->SpawnActor<AStaticMeshActor>(AreaHome(Team)*(28./27.)+FVector(0,0,200),FRotator::ZeroRotator);
  Base->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
  Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Base->SetActorScale3D(FVector(4,4,4));Base->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
  Base->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));BaseActors.Add(Base);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleAreaSmoke")))
 {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,GetWorld()->GetSubsystem<UBattleLeaderRuntime>(),&UBattleLeaderRuntime::RunAreaSmoke,5.f,false);}
}
bool UBattleMatch::AreaVisible(int32 ID,FVector Point,AActor* Target)const
{
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto C=R->Sight(ID);
 if(!C.bAlive||FVector::DistSquared(C.Position,Point)>C.RangeSquared)return false;
 const FVector D=(Point-C.Position).GetSafeNormal2D();
 if(!C.bOmnidirectional&&FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)return false;
 FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,C.Eye,Point,ECC_Visibility,C.Query);
 return !Hit.bBlockingHit || (Target&&Hit.GetActor()==Target);
}
bool UBattleMatch::CanFireAtBase(int32 ID,FVector& Target)const
{
 if(!bArea||Phase!=EBattleMatchPhase::Combat)return false;
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!U||!U->IsAlive())return false;
 const int32 Enemy=1-U->Team;if(Area.BaseHP[Enemy]<=0||!BaseActors.IsValidIndex(Enemy))return false;
 Target=BaseActors[Enemy]->GetActorLocation();
 return FVector::DistSquared(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Position(ID),Target)<=FMath::Square(GetDefault<UBattleTuning>()->EffectiveWeaponRange())&&AreaVisible(ID,Target,BaseActors[Enemy]);
}
bool UBattleMatch::DamageBase(AActor* Target,int32 Team,float Damage)
{
 if(!bArea||Phase!=EBattleMatchPhase::Combat||Team<0||Team>1||!FMath::IsFinite(Damage)||Damage<=0||!BaseActors.IsValidIndex(1-Team)||Target!=BaseActors[1-Team])return false;
  Area.BaseHP[1-Team]=FMath::Max(0.f,Area.BaseHP[1-Team]-Damage);
 if(Area.HomeHitAt.Num()!=2)Area.HomeHitAt={-1000,-1000};Area.HomeHitAt[1-Team]=Elapsed;
 if(Area.BaseHP[1-Team]==0)Finish(Team,Team==0?TEXT("VICTORY / ENEMY BASE DESTROYED"):TEXT("DEFEAT / HOME BASE DESTROYED"));
 return true;
}
int32 UBattleMatch::ActiveSquads(int32 Team)const
{
 int32 Count=0;for(const auto& P:GetWorld()->GetSubsystem<UBattleState>()->GetSquads())Count+=P.Value.Team==Team&&P.Value.CommandStatus==EBattleCommandStatus::Active;
 return Count;
}
bool UBattleMatch::HireAreaSquad(int32 Team,EBattleTraining Training,bool bAuto)
{
 LastMessage.Reset();
 auto Fail=[this](const TCHAR* Why){LastMessage=Why;return false;};
 const auto& H=BattleTactics::AreaHire();
 if(!bArea||!H.bEnabled)return Fail(TEXT("Area battle: new squads disabled"));
 if(Team<0||Team>1||Phase!=EBattleMatchPhase::Combat)return Fail(TEXT("Recruitment unavailable"));
 if(Training!=EBattleTraining::Regular)return Fail(TEXT("Area battle: regular squads only [2]"));
 if(ActiveSquads(Team)>=25)return Fail(TEXT("No free squad slot (25 active squads)"));
 if(Credits[Team]<H.Cost)return Fail(TEXT("Insufficient resources"));
 if(GetWorld()->GetSubsystem<UBattleState>()->GetSquads().Num()>=100)return Fail(TEXT("Squad record capacity reached (100 including lost squads)"));
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 int32 SID=INDEX_NONE;
 if(!R->SpawnPurchasedSquad(Team,EBattleTraining::Regular,LastMessage,!bAuto,&SID))return false;
 Credits[Team]-=H.Cost;LastHiredSquadID=SID;
 if(Area.HireSpent.Num()!=2)Area.HireSpent={0,0};
 Area.HireSpent[Team]+=H.Cost;
 // Queued replenishments reserve one resource each; drop the least advanced ones the remaining resources cannot cover.
 TArray<int32> Queued;for(const auto& Q:Area.Replacements)if(Store->Find(Q.Key)->Team==Team)Queued.Add(Q.Key);
 Queued.Sort([this](int32 A,int32 B){const float PA=Area.Replacements.FindChecked(A),PB=Area.Replacements.FindChecked(B);return PA!=PB?PA<PB:A>B;});
 int32 Cancelled=0;for(int32 I=0;I<Queued.Num()-Credits[Team];++I){Area.Replacements.Remove(Queued[I]);++Cancelled;}
 if(bAuto&&Team==0)Store->SetSquadDelegated(SID,true);
 if(bAuto){if(Area.LastAIHireAt.Num()!=2)Area.LastAIHireAt={-1000,-1000};Area.LastAIHireAt[Team]=Elapsed;}
 if(R->SightExperiment.Enabled)R->SightExperiment.HireTimes[Team].Add(Elapsed);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_HIRE team=%d squad=%d auto=%d cost=%d credits=%d cancelled_replacements=%d active=%d time=%.1f leader=%s home=%s"),
  Team,SID,bAuto?1:0,H.Cost,Credits[Team],Cancelled,ActiveSquads(Team),Elapsed,*R->Position(Store->FindSquad(SID)->LeaderUnitID).ToCompactString(),*AreaHome(Team).ToCompactString());
 LastMessage=FString::Printf(TEXT("New squad deployed / -%d resources"),H.Cost);
 return true;
}
void UBattleMatch::UpdateAreaHiring()
{
 const auto& H=BattleTactics::AreaHire();
 if(!bArea||!H.bEnabled||Phase!=EBattleMatchPhase::Combat)return;
 const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(int32 Team=0;Team<2;++Team)
 {
  if(Team==0&&!R->SightExperiment.Enabled)continue; // The player's resources are spent only by the player.
  int32 Pending=0;for(const auto& Q:Area.Replacements)Pending+=Store->Find(Q.Key)->Team==Team;
  if(Credits[Team]-Pending<H.Cost+AreaIntent(Team).HireReserve||ActiveSquads(Team)>=25)continue;
  // Pacing (own clock and own purchase history only): spacing between purchases and no purchases near the time limit,
  // where a squad costs score (15 resources for 10 living) and has little time to take a point.
  if(H.Stop>0&&Elapsed>=MatchTimeLimit()-H.Stop)continue;
  if(H.Interval>0&&Area.LastAIHireAt.IsValidIndex(Team)&&Elapsed-Area.LastAIHireAt[Team]<H.Interval)continue;
  // P1 trial: buy only for an observed need and keep that mission for the new squad.
  int32 Need=-1;
  const bool bPurpose=H.bPurpose&&(BattleTactics::AreaStrategy().TrialTeam<0||BattleTactics::AreaStrategy().TrialTeam==Team);
  if(bPurpose&&(Need=AreaHireNeed(Team))<0)continue;
  const FString Message=LastMessage;LastHiredSquadID=INDEX_NONE;
  if(HireAreaSquad(Team,EBattleTraining::Regular,true)&&bPurpose&&LastHiredSquadID!=INDEX_NONE){Area.HirePurpose.Add(LastHiredSquadID,Need);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_HIRE_PURPOSE team=%d squad=%d purpose=%d time=%.1f"),Team,LastHiredSquadID,Need,Elapsed);}
  LastMessage=Message;
 }
}
void UBattleMatch::UpdateArea(float Dt)
{
 const auto* Mode=GetWorld()->GetAuthGameMode<ABattleGameMode>();
 if(!bArea||Phase!=EBattleMatchPhase::Combat||!Mode||Mode->bCombatPaused||!FMath::IsFinite(Dt)||Dt<=0)return;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 Area.ObserveClock+=Dt;
 if(Area.ObserveClock>=.2f)
 {
  Area.ObserveClock=FMath::Fmod(Area.ObserveClock,.2f);
  for(int32 Team=0;Team<2;++Team)
  {
   for(int32 Point=0;Point<3;++Point)for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive()&&AreaVisible(U.Key,Objectives[Point].Position+FVector(0,0,80)))
   {Area.KnownOwners[Team*3+Point]=Objectives[Point].Owner;Area.SeenAt[Team*3+Point]=Elapsed;break;}
   for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive()&&AreaVisible(U.Key,BaseActors[1-Team]->GetActorLocation(),BaseActors[1-Team]))
   {Area.KnownEnemyBaseHP[Team]=Area.BaseHP[1-Team];break;}
  }
 }
 // Income: a shared clock pays both teams at once. Uses true ownership (a game rule, like capture);
 // the HUD shows only the resulting total, never the rate, so unobserved ownership is not revealed directly.
 if(const auto& I=BattleTactics::AreaIncome();I.bEnabled&&!bAreaIncomeHeld)
 {
  if(Area.Income.Num()!=2)Area.Income={0,0};
  Area.IncomeClock+=Dt;
  while(Area.IncomeClock>=I.Seconds)
  {
   Area.IncomeClock-=I.Seconds;
   for(int32 Team=0;Team<2;++Team)
   {
    int32 Gain=I.Base;for(const auto& P:Objectives)Gain+=P.Owner==Team?I.PerPoint:0;
    if(I.Ramp>0)Gain=FMath::RoundToInt(Gain*(1.f+I.Ramp*Elapsed/60.f));
    Credits[Team]+=Gain;Area.Income[Team]+=Gain;
   }
  }
 }
 Area.SupplyClock+=Dt;if(Area.SupplyClock<1)return;
 const float Step=Area.SupplyClock;Area.SupplyClock=0;
 TSet<int32> AtHome;
 for(const auto& Pair:Store->GetSquads())
 {
  const auto& S=Pair.Value;if(S.CommandStatus!=EBattleCommandStatus::Active)continue;
  int32 Living=0,Near=0;for(int32 ID:S.Members)if(Store->Find(ID)->IsAlive()){++Living;Near+=FVector::Dist2D(R->Position(ID),AreaHome(S.Team))<=2500;}
  if(FVector::Dist2D(R->Position(S.LeaderUnitID),AreaHome(S.Team))<=2500&&Near>=FMath::CeilToInt(Living*.7))AtHome.Add(S.SquadID);
 }
 for(auto It=Area.Replacements.CreateIterator();It;++It)
 {
  const auto* U=Store->Find(It.Key());
  if(!U||U->IsAlive()||U->bRetired||!AtHome.Contains(U->SquadID)){It.RemoveCurrent();continue;}
  It.Value()=FMath::Min(10.f,It.Value()+Step);
 }
 // Stable IDs make simultaneous completion/resource accounting deterministic.
 TArray<int32> Pending;Area.Replacements.GetKeys(Pending);Pending.Sort();
 for(int32 ID:Pending)if(Area.Replacements.FindChecked(ID)>=10)
 {
  const int32 Team=Store->Find(ID)->Team;
  if(Credits[Team]>0&&R->ReplaceFollower(ID)){--Credits[Team];Area.Replacements.Remove(ID);}
 }
 int32 Reserved[2]={0,0};for(const auto& Q:Area.Replacements)++Reserved[Store->Find(Q.Key)->Team];
 TArray<int32> Squads;Store->GetSquads().GetKeys(Squads);Squads.Sort();
 for(int32 SID:Squads)
 {
  const auto* S=Store->FindSquad(SID);if(!AtHome.Contains(SID)){Area.AmmoTimers.Remove(SID);continue;}
  float& AmmoTime=Area.AmmoTimers.FindOrAdd(SID);AmmoTime+=Step;
  if(AmmoTime>=10)
  {
   AmmoTime=0;for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive())Store->AddReserve(ID,GetDefault<UBattleTuning>()->ReserveAmmo);
  }
  // Trial -BattleSupplyHeal>0: living members (leader included) recover HP at home, up to their own maximum.
  if(const float Heal=BattleTactics::Supply().HealPerSecond;Heal>0.f)
   for(int32 ID:S->Members)Store->Heal(ID,Heal*Step);
  for(int32 ID:S->Members)if(!Store->Find(ID)->IsAlive()&&ID!=S->LeaderUnitID&&!Area.Replacements.Contains(ID)&&Reserved[S->Team]<Credits[S->Team])
  {Area.Replacements.Add(ID,0);++Reserved[S->Team];}
 }
}
int32 UBattleMatch::AreaHireNeed(int32 Team) const
{
 // P1 trial. Observed information only: own home HP and own soldiers' current sightings, own soldier positions,
 // the last seen enemy home HP; plus the missions of the team's own bought squads.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 bool bDefender=false,bAssault=false;
 for(const auto& P:Area.HirePurpose)if(const auto* S=Store->FindSquad(P.Key);S&&S->Team==Team&&S->CommandStatus==EBattleCommandStatus::Active)
 {bDefender|=P.Value==4;bAssault|=P.Value==3;}
 const FVector Home=AreaHome(Team);
 bool bThreat=Area.HomeHitAt.IsValidIndex(Team)&&Elapsed-Area.HomeHitAt[Team]<30.f;
 TArray<int32> Own;Own.Init(0,Objectives.Num());
 for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())
 {
  const FVector P=R->Position(U.Key);
  for(int32 I=0;I<Objectives.Num();++I)if(FVector::DistSquared2D(P,Objectives[I].Position)<=FMath::Square(10000.)){++Own[I];break;}
  if(!bThreat&&Perception&&FVector::DistSquared2D(P,Home)<=FMath::Square(15000.))
   if(const auto* Contacts=Perception->GetContacts(U.Key))
    for(const auto& C:*Contacts)if(C.Value.bVisible&&FVector::DistSquared2D(C.Value.LastKnownPosition,Home)<=FMath::Square(15000.)){bThreat=true;break;}
 }
 if(bThreat&&!bDefender)return 4;
 int32 Best=-1;
 for(int32 I=0;I<Own.Num();++I)if(Own[I]<BattleTactics::AreaHire().PurposeFront)
  if(Best<0||Own[I]<Own[Best]||(Own[I]==Own[Best]&&FVector::DistSquared2D(Objectives[I].Position,Home)<FVector::DistSquared2D(Objectives[Best].Position,Home)))Best=I;
 if(Best>=0)return Best;
 if(!bAssault&&Area.KnownEnemyBaseHP.IsValidIndex(Team)&&Area.KnownEnemyBaseHP[Team]>=0&&Area.KnownEnemyBaseHP[Team]<=500)return 3;
 return -1;
}
FAreaIntent UBattleMatch::AreaIntent(int32 Team) const
{
 // Stage 1 (ROADMAP 2026-11-16): the fixed intent equals the configured S1/S4/S6 tuning and AI hire reserve.
 // -BattleAreaTrialTeam=0/1 still limits the S4/S6 trials to one team.
 const auto& S=BattleTactics::AreaStrategy();const bool bTrial=S.TrialTeam<0||S.TrialTeam==Team;
 FAreaIntent I;I.AttackWeight=S.AttackWeight;I.BaseShare=S.BaseShare;I.bFrontBalance=bTrial&&S.bFrontBalance;I.FrontGain=S.FrontGain;
 I.FocusGain=bTrial?S.FocusGain:0.f;I.HireReserve=BattleTactics::AreaHire().Reserve;
 return I;
}
void UBattleMatch::AreaFronts(int32 Team, FAreaFront (&Out)[3]) const
{
 for(auto& F:Out)F=FAreaFront();
 if(Objectives.Num()!=3||Team<0||Team>1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 const FVector Own=AreaHome(Team),Axis=AreaHome(1-Team)-Own;const double Len2=FMath::Max(1.0,Axis.SizeSquared2D());
 auto Lane=[this](const FVector& P){int32 Best=0;for(int32 I=1;I<3;++I)if(FMath::Abs(P.Y-Objectives[I].Position.Y)<FMath::Abs(P.Y-Objectives[Best].Position.Y))Best=I;return Best;};
 auto Progress=[&](const FVector& P){return float(FVector::DotProduct(P-Own,Axis)/Len2);};
 TArray<float> OwnT[3];TMap<int32,FVector> Seen;
 for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())
 {
  const FVector P=R->Position(U.Key);OwnT[Lane(P)].Add(Progress(P));
  if(const auto* Contacts=Perception?Perception->GetContacts(U.Key):nullptr)
   for(const auto& C:*Contacts)if(C.Value.bVisible)Seen.Add(C.Key,C.Value.LastKnownPosition);
 }
 for(int32 L=0;L<3;++L)if(OwnT[L].Num())
 {OwnT[L].Sort();Out[L].Own=OwnT[L].Num();Out[L].OwnFront=OwnT[L][FMath::Min(OwnT[L].Num()-1,FMath::FloorToInt(OwnT[L].Num()*.9f))];}
 for(const auto& E:Seen)
 {const int32 L=Lane(E.Value);const float T=Progress(E.Value);++Out[L].EnemySeen;Out[L].EnemyFront=Out[L].EnemyFront<0?T:FMath::Min(Out[L].EnemyFront,T);}
}
