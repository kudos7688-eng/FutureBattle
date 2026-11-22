#include "BattleMatch.h"
#include "BattleComposition.h"
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
#include "BattlePerception.h"
#include "NavigationSystem.h"

FVector UBattleMatch::AreaHome(int32 Team)const{return FVector((Team!=int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"))))?BattleFieldLayout::HomeDistance(GetWorld()):-BattleFieldLayout::HomeDistance(GetWorld()),0,0);}
int32 UBattleMatch::ObservedOwner(int32 Team,int32 Point)const{return bArea?Area.KnownOwners[Team*Objectives.Num()+Point]:Objectives[Point].Owner;}
int32 UBattleMatch::AreaScore(int32 Team)const
{
 // War score trial: points held over time + enemy losses + unspent resources replace the living/resource/point formula.
 if(BattleWarScore::Tuning().bEnabled)return WarScore(Team);
 int32 Total=Credits[Team];for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())++Total;
 // An exoskeleton soldier counts as ScoreCount soldiers (user decision 2026-11-18: one-soldier types are not penalised by the head count).
 if(const auto* Store=GetWorld()->GetSubsystem<UBattleState>();Store->HasExoskeletons())for(const auto& S:Store->GetSquads())
  if(S.Value.Team==Team&&BattleSquadType::ScoreCount(S.Value.SquadType)>1)for(int32 ID:S.Value.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())Total+=BattleSquadType::ScoreCount(S.Value.SquadType)-1;
 for(int32 I=0;I<Objectives.Num();++I)if(Objectives[I].Owner==Team)Total+=FMath::RoundToInt(20*ObjectiveValue(I));return Total;
}
void UBattleMatch::InitializeArea()
{
 BattleComposition::LogStart(); // stage 1: returns before constructing definitions when OFF
 Credits[0]=Credits[1]=BattleTactics::AreaIncome().StartCredits;Area=FBattleAreaState();ScoutReports.Reset();IntelPurpose.Reset();PlayerRoutes.Reset();IntelViews[0]=IntelViews[1]=FAreaIntelView();KnownVehicles[0].Reset();KnownVehicles[1].Reset();FireReveals[0].Reset();FireReveals[1].Reset();CarChoices.Reset();Groups.Reset();SquadGroup.Reset();NextGroupID=1;FrontLine[0].Reset();FrontLine[1].Reset();FrontPosts.Reset();FrontReservePosts.Reset();FrontReserveRow.Reset();FreeLastPos.Reset();FreeLastStep.Reset();FreeReversals[0]=FreeReversals[1]=0;FreeMoved[0]=FreeMoved[1]=0.;bSetupDone=bSetupBurst=false;FrontLineAt[0]=FrontLineAt[1]=FrontLineLogAt[0]=FrontLineLogAt[1]=-1.f;SurpriseNextAt[0]=SurpriseNextAt[1]=0.f;Score[0]=Score[1]=0;
 // Observation records are per observing team and objective (2 x 3 on maps A-D, 2 x 5 on map E).
 Area.KnownOwners.Init(-2,2*Objectives.Num());Area.SeenAt.Init(-1.f,2*Objectives.Num());
 // A team knows the objectives it holds from the start (the depots of map F).
 for(int32 I=0;I<Objectives.Num();++I)if(const int32 Owner=Objectives[I].Owner;Owner==0||Owner==1){Area.KnownOwners[Owner*Objectives.Num()+I]=Owner;Area.SeenAt[Owner*Objectives.Num()+I]=0.f;}
 for(int32 Team=0;Team<2;++Team)
 {
  auto* Base=GetWorld()->SpawnActor<AStaticMeshActor>(AreaHome(Team)*(28./27.)+FVector(0,0,200),FRotator::ZeroRotator);
  Base->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
  Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Base->SetActorScale3D(FVector(4,4,4));Base->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
  Base->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));BaseActors.Add(Base);
 }
 if(BattleTactics::ArtilleryPosts())GenerateArtilleryPosts();
 InitializeComposition();
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleAreaSmoke")))
 {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,GetWorld()->GetSubsystem<UBattleLeaderRuntime>(),&UBattleLeaderRuntime::RunAreaSmoke,5.f,false);}
}
bool UBattleMatch::AreaVisible(int32 ID,FVector Point,AActor* Target)const
{
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto C=R->Sight(ID);
 if(!C.bAlive||FVector::DistSquared(C.Position,Point)>C.RangeSquared)return false;
 const FVector D=(Point-C.Position).GetSafeNormal2D();
 if(!C.bOmnidirectional&&FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)return false;
 const FHitResult Hit=R->TraceSight(C,Point,-1,UBattleLeaderRuntime::SightFromArea);
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
int32 UBattleMatch::ActiveScouts(int32 Team)const{return ActiveOfType(Team,BattleSquadType::Scout);}
int32 UBattleMatch::ActiveOfType(int32 Team,uint8 SquadType)const
{
 int32 Count=0;for(const auto& P:GetWorld()->GetSubsystem<UBattleState>()->GetSquads())Count+=P.Value.Team==Team&&P.Value.CommandStatus==EBattleCommandStatus::Active&&P.Value.SquadType==SquadType;
 return Count;
}
bool UBattleMatch::HireAreaSquad(int32 Team,EBattleTraining Training,bool bAuto,uint8 SquadType)
{
 LastMessage.Reset();
 auto Fail=[this](const TCHAR* Why){LastMessage=Why;return false;};
 const auto& H=BattleTactics::AreaHire();
 if(!bArea||!H.bEnabled)return Fail(TEXT("Area battle: new squads disabled"));
 if(Team<0||Team>1||Phase!=EBattleMatchPhase::Combat)return Fail(TEXT("Recruitment unavailable"));
 if(Training!=EBattleTraining::Regular)return Fail(TEXT("Area battle: regular squads only [2]"));
 if(ActiveSquads(Team)>=BattleTactics::AreaHire().MaxActive)return Fail(TEXT("No free squad slot (active squad limit)"));
 // Squad type: rifle squad (AreaSquadCost) or heavy infantry (AreaHeavyCost).
 if(SquadType>=BattleSquadType::Count)return Fail(TEXT("Recruitment unavailable"));
 const int32 Cost=SquadType==BattleSquadType::Rifle?H.Cost:BattleSquadType::AreaCost(SquadType);
 if(SquadType==BattleSquadType::Scout&&ActiveScouts(Team)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaScoutLimit))return Fail(TEXT("Scout pair limit reached"));
 if(SquadType==BattleSquadType::AntiTank&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaATLimit))return Fail(TEXT("Anti-tank team limit reached"));
 if(SquadType==BattleSquadType::Tank&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaTankLimit))return Fail(TEXT("Tank limit reached"));
 if(SquadType==BattleSquadType::ArmoredCar&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaACLimit))return Fail(TEXT("Armoured car limit reached"));
 if(SquadType==BattleSquadType::Mortar&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaMortarLimit))return Fail(TEXT("Mortar team limit reached"));
 if(SquadType==BattleSquadType::SPGun&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaSPGLimit))return Fail(TEXT("Self-propelled gun limit reached"));
 if(SquadType==BattleSquadType::Exo&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaExoLimit))return Fail(TEXT("Exoskeleton soldier limit reached"));
 if(SquadType==BattleSquadType::Supply&&ActiveOfType(Team,SquadType)>=FMath::Max(0,GetDefault<UBattleTuning>()->AreaSupplyLimit))return Fail(TEXT("Supply vehicle limit reached"));
 if(Credits[Team]<Cost)return Fail(TEXT("Insufficient resources"));
 if(GetWorld()->GetSubsystem<UBattleState>()->GetSquads().Num()>=100)return Fail(TEXT("Squad record capacity reached (100 including lost squads)"));
 auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 int32 SID=INDEX_NONE;
 if(!R->SpawnPurchasedSquad(Team,EBattleTraining::Regular,LastMessage,!bAuto,&SID,SquadType))return false;
 Credits[Team]-=Cost;LastHiredSquadID=SID;
 if((CompositionTeams&(1<<Team))!=0&&CompositionState.IsValidIndex(Team))++CompositionState[Team].Purchased[SquadType];
 if(Area.HireSpent.Num()!=2)Area.HireSpent={0,0};
 Area.HireSpent[Team]+=Cost;
 // Queued replenishments reserve one resource each; drop the least advanced ones the remaining resources cannot cover.
 TArray<int32> Queued;for(const auto& Q:Area.Replacements)if(Store->Find(Q.Key)->Team==Team)Queued.Add(Q.Key);
 Queued.Sort([this](int32 A,int32 B){const float PA=Area.Replacements.FindChecked(A),PB=Area.Replacements.FindChecked(B);return PA!=PB?PA<PB:A>B;});
 int32 Cancelled=0;for(int32 I=0;I<Queued.Num()-Credits[Team];++I){Area.Replacements.Remove(Queued[I]);++Cancelled;}
 if(bAuto){if(Area.LastAIHireAt.Num()!=2)Area.LastAIHireAt={-1000,-1000};Area.LastAIHireAt[Team]=Elapsed;}
 if(R->SightExperiment.Enabled)R->SightExperiment.HireTimes[Team].Add(Elapsed);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_HIRE team=%d squad=%d auto=%d cost=%d credits=%d cancelled_replacements=%d active=%d time=%.1f leader=%s home=%s type=%d"),
  Team,SID,bAuto?1:0,Cost,Credits[Team],Cancelled,ActiveSquads(Team),Elapsed,*R->Position(Store->FindSquad(SID)->LeaderUnitID).ToCompactString(),*AreaHome(Team).ToCompactString(),int32(SquadType));
 LastMessage=SquadType==BattleSquadType::Supply?FString::Printf(TEXT("Supply vehicle deployed / -%d resources"),Cost):SquadType==BattleSquadType::Exo?FString::Printf(TEXT("Exoskeleton soldier deployed / -%d resources"),Cost):SquadType==BattleSquadType::SPGun?FString::Printf(TEXT("Self-propelled gun deployed / -%d resources"),Cost):SquadType==BattleSquadType::Mortar?FString::Printf(TEXT("Mortar team deployed / -%d resources"),Cost):SquadType==BattleSquadType::ArmoredCar?FString::Printf(TEXT("Armoured car deployed / -%d resources"),Cost):SquadType==BattleSquadType::Tank?FString::Printf(TEXT("Tank deployed / -%d resources"),Cost):SquadType==BattleSquadType::AntiTank?FString::Printf(TEXT("Anti-tank team deployed / -%d resources"),Cost):SquadType==BattleSquadType::Scout?FString::Printf(TEXT("Scout pair deployed / -%d resources"),Cost):SquadType==BattleSquadType::Heavy?FString::Printf(TEXT("Heavy infantry deployed / -%d resources"),Cost):FString::Printf(TEXT("New squad deployed / -%d resources"),Cost);
 return true;
}
bool UBattleMatch::SetupPhase(){static const int32 V=[]{int32 X=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSetupPhase="),X);return X;}();return V!=0;}
bool UBattleMatch::OrderSync(){static const int32 V=[]{int32 X=1;FParse::Value(FCommandLine::Get(),TEXT("BattleOrderSync="),X);return X;}();return V!=0;} // default on (user decision 2026-11-22)
int32 UBattleMatch::StartSquads(){static const int32 V=[]{int32 X=25;FParse::Value(FCommandLine::Get(),TEXT("BattleStartSquads="),X);return FMath::Clamp(X,1,25);}();return V;}
void UBattleMatch::UpdateAreaHiring()
{
 const auto& H=BattleTactics::AreaHire();
 if(!bArea||!H.bEnabled||Phase!=EBattleMatchPhase::Combat)return;
 const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 UpdateIntelPurposes();
 for(int32 Team=0;Team<2;++Team)
 {
  if(Team==0&&!R->SightExperiment.Enabled)continue; // The player's resources are spent only by the player.
  if((CompositionTeams&(1<<Team))!=0){UpdateCompositionHiring(Team);continue;}
  int32 Pending=0;for(const auto& Q:Area.Replacements)Pending+=Store->Find(Q.Key)->Team==Team;
  // Squad type by the intent's heavy share: heavy infantry while the team's bought heavy squads are below that share of its purchases.
  FAreaIntent HireIntent=AreaIntent(Team); uint8 Type=BattleSquadType::Rifle;
  int32 Bought=0,Heavy=0;for(const auto& P:Store->GetSquads())if(P.Value.Team==Team){Bought+=P.Key>StartSquadCount();Heavy+=P.Value.SquadType==BattleSquadType::Heavy;}
  // Subtraction evaluation (Docs/design-subtraction-eval.md, trials default 0): -BattleAreaAllTypes=1 gives BOTH teams the all-types mix
  // (the composition schedule below plus heavy share 0.2, two scout pairs, one exoskeleton soldier, one supply vehicle; command-line
  // keep values are not mixed in); -BattleAreaWithout=<type> then removes one type (the trial team only with -BattleAreaTrialTeam).
  const bool bAllTypes=BattleTactics::AreaAllTypes();const int32 Without=BattleTactics::AreaWithout(Team);
  if(bAllTypes)
  {
   const auto* TT=GetDefault<UBattleTuning>();auto Cap=[](int32 Value,int32 Limit){return FMath::Clamp(Value,0,FMath::Max(0,Limit));};
   HireIntent.HeavyShare=FMath::Max(HireIntent.HeavyShare,.2f);HireIntent.ScoutKeep=Cap(2,TT->AreaScoutLimit);HireIntent.ExoKeep=Cap(1,TT->AreaExoLimit);HireIntent.SupplyKeep=Cap(1,TT->AreaSupplyLimit);
  }
  if(Without==BattleSquadType::Heavy)HireIntent.HeavyShare=0.f;
  if(HireIntent.HeavyShare>0&&Heavy<HireIntent.HeavyShare*(Bought+1))Type=BattleSquadType::Heavy;
  // Strategic layer -BattleAreaComposition (default on since 2026-11-22): the AI sets the numbers of special squad types it keeps (BattleArmourAI.cpp).
  const bool bComposition=BattleTactics::AreaComposition(Team)||bAllTypes;const uint8 PlainType=Type;
  if(bComposition)AreaComposition(Team,HireIntent,Bought);
  switch(Without)
  {
   case BattleSquadType::Scout:HireIntent.ScoutKeep=0;break;case BattleSquadType::AntiTank:HireIntent.ATKeep=0;break;case BattleSquadType::Tank:HireIntent.TankKeep=0;break;
   case BattleSquadType::ArmoredCar:HireIntent.ACKeep=0;break;case BattleSquadType::Mortar:HireIntent.MortarKeep=0;break;case BattleSquadType::SPGun:HireIntent.SPGKeep=0;break;
   case BattleSquadType::Exo:HireIntent.ExoKeep=0;break;case BattleSquadType::Supply:HireIntent.SupplyKeep=0;break;default:break;
  }
  // Scout pairs first when the team has fewer active pairs than it wants: none during the first purchases (taking points),
  // then one more per AreaScoutAfterHires purchases up to the intent's ScoutKeep; a lost pair is bought again.
  {
   const int32 After=FMath::Max(1,GetDefault<UBattleTuning>()->AreaScoutAfterHires);
   const int32 Want=FMath::Min(HireIntent.ScoutKeep,Bought/After);
   if(Want>0&&ActiveScouts(Team)<Want)Type=BattleSquadType::Scout;
   // Anti-tank teams (trial -BattleAreaATKeep, default AreaATKeep 0): the same pacing as scout pairs, scouts first.
   // With the composition trial, anti-tank teams answer known enemy vehicles at once (no pacing by purchases).
   const int32 ATWant=bComposition?HireIntent.ATKeep:FMath::Min(HireIntent.ATKeep,Bought/After);
   if(Type!=BattleSquadType::Scout&&ATWant>0&&ActiveOfType(Team,BattleSquadType::AntiTank)<ATWant)Type=BattleSquadType::AntiTank;
   // Tanks (trial -BattleAreaTankKeep, default AreaTankKeep 0): the same pacing.
   const int32 TankWant=FMath::Min(HireIntent.TankKeep,Bought/After);
   if(Type==BattleSquadType::Rifle&&TankWant>0&&ActiveOfType(Team,BattleSquadType::Tank)<TankWant)Type=BattleSquadType::Tank;
   // Armoured cars (trial -BattleAreaACKeep, default AreaACKeep 0): the same pacing, after tanks.
   const int32 ACWant=FMath::Min(HireIntent.ACKeep,Bought/After);
   if(Type==BattleSquadType::Rifle&&ACWant>0&&ActiveOfType(Team,BattleSquadType::ArmoredCar)<ACWant)Type=BattleSquadType::ArmoredCar;
   // Indirect fire (trials -BattleAreaMortarKeep / -BattleAreaSPGKeep, defaults 0): the same pacing, after armoured cars.
   const int32 MortarWant=FMath::Min(HireIntent.MortarKeep,Bought/After);
   if(Type==BattleSquadType::Rifle&&MortarWant>0&&ActiveOfType(Team,BattleSquadType::Mortar)<MortarWant)Type=BattleSquadType::Mortar;
   const int32 SPGWant=FMath::Min(HireIntent.SPGKeep,Bought/After);
   if(Type==BattleSquadType::Rifle&&SPGWant>0&&ActiveOfType(Team,BattleSquadType::SPGun)<SPGWant)Type=BattleSquadType::SPGun;
   // Armoured exoskeleton soldiers (trial -BattleAreaExoKeep, default AreaExoKeep 0): the same pacing, last.
   // Trial -BattleAreaExoEarly=1 (default 0): after the first After purchases (points first), all ExoKeep at once and before the other special types (after scouts), so that a raid
   // group (-BattleArtilleryRaidGroup) can form; measured without it the exos came one at a time after 300 s and the group formed late.
   static const bool bExoEarly=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaExoEarly="),V);return V==1;}();
   const int32 ExoWant=bExoEarly?(Bought>=After?HireIntent.ExoKeep:0):FMath::Min(HireIntent.ExoKeep,Bought/After);
   if((Type==BattleSquadType::Rifle||(bExoEarly&&Type!=BattleSquadType::Scout))&&ExoWant>0&&ActiveOfType(Team,BattleSquadType::Exo)<ExoWant)Type=BattleSquadType::Exo;
   // Supply vehicles (trial -BattleAreaSupplyKeep, default AreaSupplyKeep 0; BattleSupplyCar.cpp): the same pacing, after everything else.
   const int32 SupplyWant=FMath::Min(HireIntent.SupplyKeep,Bought/After);
   if(Type==BattleSquadType::Rifle&&SupplyWant>0&&ActiveOfType(Team,BattleSquadType::Supply)<SupplyWant)Type=BattleSquadType::Supply;
  }
  // Subtraction evaluation -BattleAreaEnsureTypes=1 (default 0): one of each rarely reached type is bought once (ever, lost ones are
  // not replaced) after the first After purchases, ahead of everything but scouts, and the team saves for it instead of falling back.
  bool bEnsured=false;
  if(bAllTypes&&BattleTactics::AreaEnsureTypes()&&Type!=BattleSquadType::Scout&&Bought>=FMath::Max(1,GetDefault<UBattleTuning>()->AreaScoutAfterHires))
   for(const uint8 Ensure:{BattleSquadType::SPGun,BattleSquadType::Exo,BattleSquadType::Supply})
   {
    if(Ensure==Without)continue;
    bool bEver=false;for(const auto& P:Store->GetSquads())if(P.Value.Team==Team&&P.Key>StartSquadCount()&&P.Value.SquadType==Ensure){bEver=true;break;}
    if(!bEver){Type=Ensure;bEnsured=true;break;}
   }
  // Composition trial: an expensive type the team is far from affording (below 60% of its cost) does not hold up the plain purchases.
  if(!bEnsured&&bComposition&&Type!=PlainType&&BattleSquadType::AreaCost(Type)>=20&&Credits[Team]-Pending<FMath::CeilToInt(BattleSquadType::AreaCost(Type)*.6f)+HireIntent.HireReserve)Type=PlainType;
  const int32 TypeCost=Type==BattleSquadType::Rifle?H.Cost:BattleSquadType::AreaCost(Type);
  if(Credits[Team]-Pending<TypeCost+HireIntent.HireReserve||ActiveSquads(Team)>=BattleTactics::AreaHire().MaxActive)continue;
  // Pacing (own clock and own purchase history only): spacing between purchases and no purchases near the time limit,
  // where a squad costs score (15 resources for 10 living) and has little time to take a point.
  if(H.Stop>0&&Elapsed>=MatchTimeLimit()-H.Stop)continue;
  if(!bSetupBurst&&H.Interval>0&&Area.LastAIHireAt.IsValidIndex(Team)&&Elapsed-Area.LastAIHireAt[Team]<H.Interval)continue; // the setup buys at once
  // P1 trial: buy only for an observed need and keep that mission for the new squad.
  int32 Need=-1;
  const bool bPurpose=H.bPurpose&&(BattleTactics::AreaStrategy().TrialTeam<0||BattleTactics::AreaStrategy().TrialTeam==Team);
  // Intel trial (4): with a known threat at an own point, the purchase (not a scout pair) is bought to defend it.
  const int32 IntelNeed=!bPurpose&&Type!=BattleSquadType::Scout&&AreaIntelOn(Team)?AreaIntelThreat(Team):-1;
  if(bPurpose&&(Need=AreaHireNeed(Team))<0)continue;
  const FString Message=LastMessage;LastHiredSquadID=INDEX_NONE;
  if(HireAreaSquad(Team,EBattleTraining::Regular,true,Type)&&bPurpose&&LastHiredSquadID!=INDEX_NONE){Area.HirePurpose.Add(LastHiredSquadID,Need);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_HIRE_PURPOSE team=%d squad=%d purpose=%d time=%.1f"),Team,LastHiredSquadID,Need,Elapsed);}
  else if(IntelNeed>=0&&LastHiredSquadID!=INDEX_NONE&&Store->FindSquad(LastHiredSquadID)){IntelPurpose.Add(LastHiredSquadID,IntelNeed);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_INTEL_HIRE team=%d squad=%d point=%d time=%.1f"),Team,LastHiredSquadID,IntelNeed,Elapsed);}
  // Strategy layer S4 (BattleStrategy.cpp, -BattleStrategy=4): a rifle squad bought while the team has a focus reinforces it; its mission
  // follows the focus when the plan changes (UpdateStrategy).
  else if(LastHiredSquadID!=INDEX_NONE&&Type==BattleSquadType::Rifle&&StrategyOn(Team,4)&&Store->FindSquad(LastHiredSquadID))
   if(const int32 Focus=StrategyFocusPoint(Team);Focus!=INDEX_NONE&&ObservedOwner(Team,Focus)!=Team)
   {Area.HirePurpose.Add(LastHiredSquadID,Focus);Area.StrategyHired.Add(LastHiredSquadID);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STRATEGY_HIRE team=%d squad=%d focus=%d time=%.1f"),Team,LastHiredSquadID,Focus,Elapsed);}
  LastMessage=Message;
 }
}
int32 UBattleMatch::AreaIncomeGain(int32 Team) const
{
 // Whole numbers on maps without objective values or an income scale (A-E): the sum below is then exact.
 const auto& I=BattleTactics::AreaIncome();const auto& Def=BattleFieldLayout::Of(GetWorld());
 double Points=0;for(int32 Point=0;Point<Objectives.Num();++Point)if(Objectives[Point].Owner==Team)Points+=BattleFieldLayout::ObjectiveValue(Def,Point);
 return I.BaseFor(Def.bUrban)+FMath::RoundToInt(I.PerPointFor(Def.bUrban)*Points*Def.PointIncomeScale);
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
    int32 Gain=AreaIncomeGain(Team);
    if(I.Ramp>0)Gain=FMath::RoundToInt(Gain*(1.f+I.Ramp*Elapsed/60.f));
    Credits[Team]+=Gain;Area.Income[Team]+=Gain;
   }
  }
 }
 UpdateWarScore(Dt);if(Phase!=EBattleMatchPhase::Combat)return;
 UpdateScoutReports();
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
  const auto* S=Store->FindSquad(SID);const bool bHome=AtHome.Contains(SID);
  // A supply vehicle at home loads its stock again (BattleSupplyCar.cpp).
  if(bHome&&S->SquadType==BattleSquadType::Supply)RestockSupplyCar(SID);
  // -BattleAreaPointSupply (default ON since 2026-11-22): the same ammunition refill at a point the squad's team owns (calm: not contested, not being captured).
  bool bPoint=false;
  if(!bHome&&BattleTactics::Supply().bPointAmmo&&S->CommandStatus==EBattleCommandStatus::Active)
  {
   const FVector Leader=R->Position(S->LeaderUnitID);const float Radius=BattleTactics::Supply().PointRadius;
   for(const auto& Point:Objectives)if(Point.Owner==S->Team&&!Point.bContested&&Point.CapturingTeam==INDEX_NONE&&FVector::Dist2D(Leader,Point.Position)<=Radius){bPoint=true;break;}
  }
  // A supply vehicle of the squad's side nearby with stock left gives the same refill anywhere and loses the rounds it gave.
  const int32 Car=!bHome&&!bPoint&&S->CommandStatus==EBattleCommandStatus::Active&&S->SquadType!=BattleSquadType::Supply?SupplyCarNear(S->Team,R->Position(S->LeaderUnitID)):INDEX_NONE;
  const bool bTeamSupply=Car!=INDEX_NONE;
  if(!bHome&&!bPoint&&!bTeamSupply){Area.AmmoTimers.Remove(SID);continue;}
  float& AmmoTime=Area.AmmoTimers.FindOrAdd(SID);AmmoTime+=Step;
  if(AmmoTime>=10)
  {
   AmmoTime=0;int32 Rounds=0;for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive()){Rounds+=Store->AddReserve(ID,GetDefault<UBattleTuning>()->ReserveAmmo);Store->RefillLauncher(ID);Rounds+=Store->RefillSPGAmmo(ID);}
   if(bPoint&&Rounds>0)UE_LOG(LogFutureBattle,Display,TEXT("AREA_POINT_SUPPLY time=%.0f squad=%d team=%d rounds=%d"),Elapsed,SID,S->Team,Rounds);
   if(bTeamSupply&&Rounds>0){SpendSupply(Car,Rounds);UE_LOG(LogFutureBattle,Display,TEXT("AREA_TEAM_SUPPLY time=%.0f squad=%d team=%d rounds=%d car=%d stock=%d"),Elapsed,SID,S->Team,Rounds,Car,SupplyStockOf(Car));}
  }
  if(!bHome)continue; // a point gives ammunition only: no healing, no replacements
  // Trial -BattleSupplyHeal>0: living members (leader included) recover HP at home, up to their own maximum.
  if(const float Heal=BattleTactics::Supply().HealPerSecond;Heal>0.f)
   for(int32 ID:S->Members)Store->Heal(ID,Heal*Step);
  // Scout pairs are never replenished.
  if(BattleSquadType::Replenished(S->SquadType))
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
 {bDefender|=P.Value==101;bAssault|=P.Value==100;}
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
 if(bThreat&&!bDefender)return 101;
 int32 Best=-1;
 for(int32 I=0;I<Own.Num();++I)if(Own[I]<BattleTactics::AreaHire().PurposeFront)
  if(Best<0||Own[I]<Own[Best]||(Own[I]==Own[Best]&&FVector::DistSquared2D(Objectives[I].Position,Home)<FVector::DistSquared2D(Objectives[Best].Position,Home)))Best=I;
 if(Best>=0)return Best;
 if(!bAssault&&Area.KnownEnemyBaseHP.IsValidIndex(Team)&&Area.KnownEnemyBaseHP[Team]>=0&&Area.KnownEnemyBaseHP[Team]<=500)return 100;
 return -1;
}
FAreaIntent UBattleMatch::AreaIntent(int32 Team) const
{
 // Stage 1 (ROADMAP 2026-11-16): the fixed intent equals the configured S1/S4/S6 tuning and AI hire reserve.
 // -BattleAreaTrialTeam=0/1 limits the S4/S6 command-line trials to one team; the other team keeps the configured S4/S6 (2026-11-18 fix:
 // before, it lost S4/S6, which biased every one-team screen since S4/S6 became the default).
 const auto* T=GetDefault<UBattleTuning>();
 const auto& S=BattleTactics::AreaStrategy();const bool bTrial=S.TrialTeam<0||S.TrialTeam==Team;
 FAreaIntent I;I.AttackWeight=S.AttackWeight;I.BaseShare=S.BaseShare;I.bFrontBalance=bTrial?S.bFrontBalance:T->bAreaFrontBalance;I.FrontGain=bTrial?S.FrontGain:FMath::Clamp(T->AreaFrontGain,0.f,5.f);
 I.FocusGain=bTrial?S.FocusGain:FMath::Clamp(T->AreaFocusGain,0.f,10.f);I.HireReserve=BattleTactics::AreaHire().Reserve;
 // Heavy infantry share of AI purchases: the configured default for both teams; the -BattleAreaHeavyShare trial replaces it for the trial team(s).
 I.HeavyShare=bTrial&&S.HeavyShare>=0?S.HeavyShare:FMath::Clamp(GetDefault<UBattleTuning>()->AreaHeavyShare,0.f,1.f);
 // Scout pairs the AI keeps: configured default; -BattleAreaScoutKeep replaces it for the trial team(s).
 I.ScoutKeep=FMath::Clamp(bTrial&&S.ScoutKeep>=0?S.ScoutKeep:GetDefault<UBattleTuning>()->AreaScoutKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaScoutLimit));
 I.ATKeep=FMath::Clamp(bTrial&&S.ATKeep>=0?S.ATKeep:GetDefault<UBattleTuning>()->AreaATKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaATLimit));
 I.TankKeep=FMath::Clamp(bTrial&&S.TankKeep>=0?S.TankKeep:GetDefault<UBattleTuning>()->AreaTankKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaTankLimit));
 I.ACKeep=FMath::Clamp(bTrial&&S.ACKeep>=0?S.ACKeep:GetDefault<UBattleTuning>()->AreaACKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaACLimit));
 I.MortarKeep=FMath::Clamp(bTrial&&S.MortarKeep>=0?S.MortarKeep:GetDefault<UBattleTuning>()->AreaMortarKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaMortarLimit));
 I.SPGKeep=FMath::Clamp(bTrial&&S.SPGKeep>=0?S.SPGKeep:GetDefault<UBattleTuning>()->AreaSPGKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaSPGLimit));
 I.ExoKeep=FMath::Clamp(bTrial&&S.ExoKeep>=0?S.ExoKeep:GetDefault<UBattleTuning>()->AreaExoKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaExoLimit));
 I.SupplyKeep=FMath::Clamp(bTrial&&S.SupplyKeep>=0?S.SupplyKeep:GetDefault<UBattleTuning>()->AreaSupplyKeep,0,FMath::Max(0,GetDefault<UBattleTuning>()->AreaSupplyLimit));
 // S7/S8 trials (one team only with -BattleAreaTrialTeam).
 if(bTrial){I.FlankOffset=S.FlankOffset;I.bFlankSameSide=S.bFlankSameSide;I.BaseReturnLiving=S.BaseReturnLiving;}
 // Stage 2: time-phased team plans (match time only; no hidden information). Values are first trial values (numbers delegated).
 // Phase times are written for the 600 s maps A-D and scale with the match length (map E: 900 s, so x1.5).
 const float K=MatchTimeLimit()/600.f;
 if(bTrial&&S.Plan==1)
 {
  // EH: expand (take points, spend freely) -> hold (owned points weigh more, keep resources: they are score and pay
  // replacements) -> late hold (no new squads: a squad costs 5 score).
  if(Elapsed<240.f*K){I.Phase=1;I.AttackWeight=3.f;I.HireReserve=0;}
  else if(Elapsed<480.f*K){I.Phase=2;I.AttackWeight=.5f;I.BaseShare=.1f;I.HireReserve=45;}
  else{I.Phase=3;I.AttackWeight=.5f;I.BaseShare=.1f;I.HireReserve=1000;}
 }
 else if(bTrial&&S.Plan==2)
 {
  // ESA: expand -> save (balanced hold, few home-assault squads, bank resources) -> mass on the enemy home (half the slots
  // assault on one flank route, assault squads return for supply only at 5 living, spend the bank).
  if(Elapsed<180.f*K){I.Phase=1;I.AttackWeight=3.f;I.HireReserve=0;}
  else if(Elapsed<420.f*K){I.Phase=2;I.AttackWeight=1.f;I.BaseShare=.1f;I.HireReserve=75;}
  else{I.Phase=3;I.BaseShare=.5f;I.FlankOffset=1.3f;I.bFlankSameSide=true;I.BaseReturnLiving=5;I.HireReserve=0;}
 }
 // Plans use heavy infantry where they hold ground: more in hold/save phases, fewer while expanding or massing.
 if(I.HeavyShare>0&&I.Phase>0)
 {
  const bool bHoldPhase=(S.Plan==1&&I.Phase>=2)||(S.Plan==2&&I.Phase==2);
  I.HeavyShare=FMath::Clamp(I.HeavyShare*(bHoldPhase?1.5f:.5f),0.f,.8f);
 }
 // Strategy layer S2 (BattleStrategy.cpp): the team's focus objective, chosen once per order update (-1 without -BattleStrategy>=2).
 I.FocusPoint=StrategyFocusPoint(Team);
 // Stage S3: a held point weighs twice in the S1 spread (BattleStrategyValues::HoldWeight).
 for(int32 Pt=0;Pt<Objectives.Num()&&Pt<FAreaIntent::MaxObjectives;++Pt)if(StrategyHolds(Team,Pt))I.PointWeight[Pt]*=2.f;
 ApplyDepotWeights(Team,bTrial,I);
 return I;
}
void UBattleMatch::AreaFronts(int32 Team, FAreaFront (&Out)[FAreaIntent::MaxObjectives]) const
{
 for(auto& F:Out)F=FAreaFront();
 const int32 N=Objectives.Num();if(N<1||N>FAreaIntent::MaxObjectives||Team<0||Team>1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 const FVector Own=AreaHome(Team),Axis=AreaHome(1-Team)-Own;const double Len2=FMath::Max(1.0,Axis.SizeSquared2D());
 // Sectors: on three-objective maps (A-D) the lane by nearest objective Y (north, centre, south); otherwise the nearest objective in 2D.
 auto Lane=[this,N](const FVector& P){int32 Best=0;for(int32 I=1;I<N;++I){const double D=N==3?FMath::Abs(P.Y-Objectives[I].Position.Y):FVector::DistSquared2D(P,Objectives[I].Position);const double B=N==3?FMath::Abs(P.Y-Objectives[Best].Position.Y):FVector::DistSquared2D(P,Objectives[Best].Position);if(D<B)Best=I;}return Best;};
 auto Progress=[&](const FVector& P){return float(FVector::DotProduct(P-Own,Axis)/Len2);};
 TArray<float> OwnT[FAreaIntent::MaxObjectives];TMap<int32,FVector> Seen;
 for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())
 {
  const FVector P=R->Position(U.Key);OwnT[Lane(P)].Add(Progress(P));
  if(const auto* Contacts=Perception?Perception->GetContacts(U.Key):nullptr)
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!IsScoutLongContact(U.Key,C.Value))Seen.Add(C.Key,C.Value.LastKnownPosition);
 }
 // Scout reports: what scouts saw beyond normal sight enters team plans only through their reports.
 AddScoutReportSightings(Team,Seen);
 for(int32 L=0;L<N;++L)if(OwnT[L].Num())
 {OwnT[L].Sort();Out[L].Own=OwnT[L].Num();Out[L].OwnFront=OwnT[L][FMath::Min(OwnT[L].Num()-1,FMath::FloorToInt(OwnT[L].Num()*.9f))];}
 for(const auto& E:Seen)
 {const int32 L=Lane(E.Value);const float T=Progress(E.Value);++Out[L].EnemySeen;Out[L].EnemyFront=Out[L].EnemyFront<0?T:FMath::Min(Out[L].EnemyFront,T);}
}
int32 UBattleMatch::AreaHeavyTarget(int32 Team) const
{
 if(Objectives.IsEmpty()) return 0;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 int32 Best=INDEX_NONE,BestOwn=MAX_int32;
 for(int32 I=0;I<Objectives.Num();++I) if(ObservedOwner(Team,I)==Team)
 {
  int32 Own=0;for(const auto& U:Store->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive()&&FVector::DistSquared2D(R->Position(U.Key),Objectives[I].Position)<=FMath::Square(10000.))++Own;
  if(Own<BestOwn){BestOwn=Own;Best=I;}
 }
 if(Best!=INDEX_NONE) return Best;
 const FVector Home=AreaHome(Team);double BestD=DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I){const double D=FVector::DistSquared2D(Objectives[I].Position,Home);if(D<BestD){BestD=D;Best=I;}}
 return Best;
}

FVector UBattleMatch::AreaScoutPost(int32 Team,int32 SquadID) const
{
 // Fronts: observed own points, most forward first (nearest the enemy home); with none, the points nearest own home.
 // Each active scout pair of the team (in squad ID order) takes the next front, ScoutPostAdvance towards the enemy home.
 if(Objectives.IsEmpty()) return AreaHome(Team);
 const FVector Own=AreaHome(Team),Enemy=AreaHome(1-Team);
 TArray<int32> Fronts;for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)==Team)Fronts.Add(I);
 const bool bOwned=!Fronts.IsEmpty();if(!bOwned)for(int32 I=0;I<Objectives.Num();++I)Fronts.Add(I);
 Fronts.Sort([&](int32 A,int32 B){const FVector& RefPos=bOwned?Enemy:Own;const double DA=FVector::DistSquared2D(Objectives[A].Position,RefPos),DB=FVector::DistSquared2D(Objectives[B].Position,RefPos);return DA!=DB?DA<DB:A<B;});
 int32 Rank=0;for(const auto& P:GetWorld()->GetSubsystem<UBattleState>()->GetSquads())
  Rank+=P.Key<SquadID&&P.Value.Team==Team&&P.Value.SquadType==BattleSquadType::Scout&&P.Value.CommandStatus==EBattleCommandStatus::Active;
 // Lookouts first (public places with long sight), when not ahead of the team's own front; then the front points.
 TArray<FVector> Lookouts;
 if(bOwned){const double FrontD=FVector::Dist2D(Objectives[Fronts[0]].Position,Enemy);for(const FVector& L:BattleFieldLayout::Lookouts(GetWorld()))if(FVector::Dist2D(L,Enemy)>=FrontD-2000.)Lookouts.Add(L);}
 Lookouts.Sort([&](const FVector& A,const FVector& B){return FVector::DistSquared2D(A,Enemy)<FVector::DistSquared2D(B,Enemy);});
 const int32 Slot=Rank%(Lookouts.Num()+Fronts.Num());
 if(Slot<Lookouts.Num())
 {
  FVector Post=Lookouts[Slot];
  if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){FNavLocation L;if(Nav->ProjectPointToNavigation(Post,L,FVector(500,500,500)))Post=L.Location;}
  return Post;
 }
 const FVector Point=Objectives[Fronts[Slot-Lookouts.Num()]].Position;
 const FVector Ahead=(Enemy-Point).GetSafeNormal2D();
 FVector Post=Point+Ahead*FMath::Clamp(GetDefault<UBattleTuning>()->ScoutPostAdvance,-30000.f,30000.f);
 // After each retreat the pair watches from a post shifted sideways (right, left, back to centre), not the spot it was found at.
 if(const auto* Rep=ScoutReports.Find(SquadID);Rep&&Rep->Burns>0){const int32 Step=Rep->Burns%3;Post+=FVector(-Ahead.Y,Ahead.X,0)*(Step==1?1.f:Step==2?-1.f:0.f)*FMath::Clamp(GetDefault<UBattleTuning>()->ScoutPostShift,0.f,20000.f);}
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
 {FNavLocation L;if(Nav->ProjectPointToNavigation(Post,L,FVector(1500,1500,500)))Post=L.Location;else Post=Point;}
 return Post;
}
