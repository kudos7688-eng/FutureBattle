#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleWarScore.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "FutureBattle.h"

FVector UBattleMatch::AreaHome(int32 Team)const{return FVector((Team!=int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"))))?BattleFieldLayout::HomeDistance(GetWorld()):-BattleFieldLayout::HomeDistance(GetWorld()),0,0);}
int32 UBattleMatch::ObservedOwner(int32 Team,int32 Point)const{return bArea?Area.KnownOwners[Team*Objectives.Num()+Point]:Objectives[Point].Owner;}
int32 UBattleMatch::AreaScore(int32 Team)const
{
 // War score trial: points held over time + enemy losses + unspent resources replace the living/resource/point formula.
 if(BattleWarScore::Tuning().bEnabled)return WarScore(Team);
 int32 Total=Credits[Team];for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())++Total;
 for(const auto& P:Objectives)if(P.Owner==Team)Total+=20;return Total;
}
void UBattleMatch::InitializeArea()
{
 Credits[0]=Credits[1]=100;Area=FBattleAreaState();Score[0]=Score[1]=0;
 // Observation records are per observing team and objective (2 x 3 on maps A-D, 2 x 5 on map E).
 Area.KnownOwners.Init(-2,2*Objectives.Num());Area.SeenAt.Init(-1.f,2*Objectives.Num());
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
 Credits[Team]-=H.Cost;
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
  if(Credits[Team]-Pending<H.Cost+H.Reserve||ActiveSquads(Team)>=25)continue;
  // Pacing (own clock and own purchase history only): spacing between purchases and no purchases near the time limit,
  // where a squad costs score (15 resources for 10 living) and has little time to take a point.
  if(H.Stop>0&&Elapsed>=MatchTimeLimit()-H.Stop)continue;
  if(H.Interval>0&&Area.LastAIHireAt.IsValidIndex(Team)&&Elapsed-Area.LastAIHireAt[Team]<H.Interval)continue;
  const FString Message=LastMessage;HireAreaSquad(Team,EBattleTraining::Regular,true);LastMessage=Message;
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
   for(int32 Point=0;Point<Objectives.Num();++Point)for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive()&&AreaVisible(U.Key,Objectives[Point].Position+FVector(0,0,80)))
   {Area.KnownOwners[Team*Objectives.Num()+Point]=Objectives[Point].Owner;Area.SeenAt[Team*Objectives.Num()+Point]=Elapsed;break;}
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
    Credits[Team]+=Gain;Area.Income[Team]+=Gain;
   }
  }
 }
 UpdateWarScore(Dt);if(Phase!=EBattleMatchPhase::Combat)return;
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
