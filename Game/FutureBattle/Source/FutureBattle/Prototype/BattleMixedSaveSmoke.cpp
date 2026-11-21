#include "BattleLeaderRuntime.h"
#include "BattleSave.h"
#include "BattleMatch.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleSquadTactics.h"
#include "BattleTeamAI.h"
#include "BattleTactics.h"
#include <limits>
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunMixedSaveSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK)++SmokeFailures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_SAVE_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMixedSaveSmoke,Delay,false);};
 auto Resource=[&](int32 ID)->FBattleUnitState& {return *const_cast<FBattleUnitState*>(Store->Find(ID));};
 auto Plan=[&](int32 SID)->FBattleSquadTacticalPlan& {return const_cast<FBattleSquadState*>(Store->FindSquad(SID))->TacticalPlan;};
 const double Now=GetWorld()->GetTimeSeconds();
 // Rifle squad size N (8 since 2026-11-19): squad k has unit IDs (k-1)*N+1..k*N, leader first; team 1 starts at squad 26.
 const int32 N=BattleSquadType::RifleMembers(),Total=50*N,Enemy=25*N+1,LastLeader=49*N+1,Follower=N;
 if(SmokeStage==0)
 {
  Mode->bCombatPaused=true;Mode->CombatStartsAt=0;PC->EnterTactical();T->Reset();T->ActiveSeconds=10;
  auto Place=[&](int32 ID,FVector P)
  {
   auto& B=Bodies.FindChecked(ID);
   if(B.Actor.IsValid()){B.Actor->SetActorLocation(P);B.Actor->SetActorRotation(FRotator::ZeroRotator);B.Actor->GetCharacterMovement()->StopMovementImmediately();}
   else{B.Capsule->SetWorldLocation(P);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=P;F.Forward=FVector::ForwardVector;}
  };
  for(const auto& Pair:Store->GetUnits())
  {
   Place(Pair.Key,FVector(8000+(Pair.Key%20)*120,-7000+(Pair.Key/20)*120,100));
   if(Pair.Value.Team==1){Resource(Pair.Key).Ammo=0;Resource(Pair.Key).Reserve=0;Resource(Pair.Key).Perception.SightRange=100;}
  }
  for(int32 SID=1;SID<=3;++SID)
  {
   for(int32 I=0;I<N;++I)Place((SID-1)*N+I+1,FVector(-9000-(I/5)*120,-7000-(SID-1)*650-(I%5)*100,100));
   IssueOrder(SID,EBattleOrder::AttackMove,FVector(-6000,-7000-(SID-1)*650,0),true);Store->SetSquadDelegated(SID,true);
  }
  Place(Enemy,FVector(-6000,-7000,100));Resource(Enemy).HP=10000;
  Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  for(int32 SID=1;SID<=3;++SID)T->Samples.Add(SID,T->Observe(*Store->FindSquad(SID)));
  Check(Sense->FindContact(1,Enemy)&&Sense->FindContact(1,Enemy)->bVisible,TEXT("fixture has real acquired contact"));
  Check(T->Apply(1,EBattleSquadTactic::Retreat,EBattleOrder::Move,FVector(-10000,-7000,0),TEXT("SAVE RETREAT")),TEXT("active retreat fixture"));
  Check(T->Apply(2,EBattleSquadTactic::Support,EBattleOrder::Hold,Position(N+1),TEXT("SAVE SUPPORT")),TEXT("active support fixture"));
  Plan(2).SupportSquad=1;Plan(2).SupportTarget=Enemy;Plan(2).SupportUntil=18;Plan(2).LastSupportShot=10;Plan(2).bSupportConfirmed=true;
  Check(T->Apply(3,EBattleSquadTactic::Flank,EBattleOrder::Move,FVector(-8500,-8300,0),TEXT("SAVE SOLO FLANK")),TEXT("active flank fixture"));
  Plan(3).FlankTarget=Enemy;Plan(3).FlankContact=Position(Enemy);Plan(3).FlankExposure=.6f;
  Plan(1).Age=3.25;Plan(1).RetrySeconds=1.5;Plan(3).Age=2.5;
  T->RecordDamage(1,25);
  Resource(Follower).HP=55;Resource(Follower).Ammo=5;Resource(Follower).Reserve=17;Resource(Follower).ReloadEndTime=Now+1.5;Resource(Follower).NextShotTime=Now+.5;
  Store->ApplyDamage(LastLeader,0,100000);PC->SetSelection(Actor(1));Match->DelegationClock=2.5;
  auto* Saved=Match->CaptureSnapshot();FString Error;
  const bool Valid=Saved&&Saved->Validate(Error);Check(Valid,TEXT("mixed snapshot validates"));
  if(!Valid){UE_LOG(LogFutureBattle,Error,TEXT("MIXED_SAVE_VALIDATION %s"),*Error);SmokeStage=3;Later(.1f);return;}
  Check(Saved->Version==2&&Saved->Units.Num()==Total&&Saved->Squads.Num()==50,TEXT("canonical 50 squads x rifle squad size records"));
  int32 Actors=0;for(const auto& E:Saved->Units)Actors+=E.State.bActorRepresentation;
  Check(Actors==50,TEXT("50 Actor leaders and the rest Mass followers encoded"));
  Check(Match->SaveMatch(TEXT("FutureBattleSmokeOnly")),TEXT("separate mixed smoke slot written"));
  Resource(Follower).HP=1;Resource(Follower).Ammo=0;Match->Credits[0]=7;
  Check(Match->LoadMatch(TEXT("FutureBattleSmokeOnly")),TEXT("serialized mixed snapshot restored"));
  UE_LOG(LogFutureBattle,Display,TEXT("MIXED_SAVE_LOAD_MESSAGE %s"),*Match->LastMessage);
  Check(Store->Find(Follower)->HP==55&&Store->Find(Follower)->Ammo==5&&Store->Find(Follower)->Reserve==17,TEXT("follower resources survive body replacement"));
  Check(FMath::IsNearlyEqual(Store->Find(Follower)->ReloadEndTime-Now,1.5,.01)&&FMath::IsNearlyEqual(Store->Find(Follower)->NextShotTime-Now,.5,.01),TEXT("relative reload and shot timers survive"));
  Check(Bodies.Num()==Total&&Mode->Units.Num()==50&&Store->GetUnits().Num()==Total,TEXT("representation counts preserved"));
  Check(Plan(1).Action==EBattleSquadTactic::Retreat&&Plan(2).Action==EBattleSquadTactic::Support&&Plan(3).Action==EBattleSquadTactic::Flank,TEXT("three active tactical actions restored"));
  Check(Plan(2).SupportSquad==1&&Plan(2).SupportTarget==Enemy&&Plan(3).FlankTarget==Enemy&&Plan(1).Age==3.25f&&Plan(1).RetrySeconds==1.5f,TEXT("coordination references and elapsed timers restored"));
  Check(T->Clock()==10&&T->Observe(*Store->FindSquad(1)).RecentDamage==25&&Match->DelegationClock==2.5f,TEXT("decision clocks and recent damage preserved"));
  Check(Store->FindSquad(50)->CommandStatus==EBattleCommandStatus::Disabled&&!Store->Find(LastLeader)->IsAlive()&&!CanControlLeader(LastLeader),TEXT("dead leader stays disabled"));
  bool Transforms=true;for(const auto& E:Saved->Units)if(!Position(E.State.UnitID).Equals(E.Position,.01))
  {Transforms=false;UE_LOG(LogFutureBattle,Display,TEXT("MIXED_SAVE_POSITION id=%d saved=%s restored=%s"),E.State.UnitID,*E.Position.ToCompactString(),*Position(E.State.UnitID).ToCompactString());}
  Check(Transforms,TEXT("all canonical IDs retain their transforms"));
  Check(Plans.FindChecked(1).ArrivalGoals.Num()==N&&Plans.FindChecked(3).ArrivalGoals.Num()==N,TEXT("active routes and arrival reservations rebuilt"));
  auto Reject=[&](TFunctionRef<void(UBattleSave&)> Corrupt,const TCHAR* Label)
  {
   auto* Bad=DuplicateObject<UBattleSave>(Saved,this);Corrupt(*Bad);
   const FGuid Before=Match->MatchID;const float HP=Store->Find(Follower)->HP;auto* Body=Actor(1);const int32 Credit=Match->Credits[0];
   // Non-empty transient state is essential: comparing empty maps missed resets before failed staging.
   const auto OriginalApproaches=CoverApproaches;const auto OriginalFire=FirePositions;
   const auto OriginalSuppression=SuppressionZeroAt;const auto OriginalStances=Stances;const auto OriginalLocks=StanceLockedUntil;
   FCoverApproach Approach;Approach.Goal=Position(Follower)+FVector(120,80,0);Approach.Target=Enemy;
   Approach.Revision=Store->FindSquad(1)->OrderRevision;Approach.Until=Now+4.;Approach.bFree=true;Approach.bStandUp=true;
   CoverApproaches.Add(Follower,Approach);FirePositions.Add(1,Position(1)+FVector(80,120,0));
   SuppressionZeroAt.Add(Follower,Now+3.);Stances.Add(Follower,uint8(1));StanceLockedUntil.Add(Follower,Now+2.);
   const auto ExpectedApproaches=CoverApproaches;const auto ExpectedFire=FirePositions;
   const auto ExpectedSuppression=SuppressionZeroAt;const auto ExpectedStances=Stances;const auto ExpectedLocks=StanceLockedUntil;
   Check(!Match->RestoreSnapshot(Bad)&&Match->MatchID==Before&&Store->Find(Follower)->HP==HP&&Actor(1)==Body&&Match->Credits[0]==Credit&&Bodies.Num()==Total,Label);
   bool ApproachesKept=CoverApproaches.Num()==ExpectedApproaches.Num();
   for(const auto& Entry:ExpectedApproaches)
   {
    const auto* Actual=CoverApproaches.Find(Entry.Key);const auto& Expected=Entry.Value;
    ApproachesKept&=Actual && Actual->Goal==Expected.Goal && Actual->Slot==Expected.Slot && Actual->Target==Expected.Target &&
     Actual->Revision==Expected.Revision && Actual->Until==Expected.Until && Actual->bKeptTargetChange==Expected.bKeptTargetChange &&
     Actual->bFree==Expected.bFree && Actual->bStandUp==Expected.bStandUp && Actual->bFallback==Expected.bFallback;
   }
   const bool TransientsKept=ApproachesKept && FirePositions.OrderIndependentCompareEqual(ExpectedFire) &&
    SuppressionZeroAt.OrderIndependentCompareEqual(ExpectedSuppression) && Stances.OrderIndependentCompareEqual(ExpectedStances) &&
    StanceLockedUntil.OrderIndependentCompareEqual(ExpectedLocks);
   Check(TransientsKept,TEXT("failed restore preserves nonempty cover/fire/suppression/stance/lock maps"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_SAVE_TRANSIENTS %s case=%s"),TransientsKept?TEXT("PASS"):TEXT("FAIL"),Label);
   CoverApproaches=OriginalApproaches;FirePositions=OriginalFire;SuppressionZeroAt=OriginalSuppression;
   Stances=OriginalStances;StanceLockedUntil=OriginalLocks;
  };
  Reject([](UBattleSave& S){S.Units[1].State.UnitID=S.Units[0].State.UnitID;},TEXT("duplicate identity rejected atomically"));
  Reject([](UBattleSave& S){S.SavedPlans[2].SupportSquad=26;},TEXT("enemy support reference rejected atomically"));
  Reject([](UBattleSave& S){S.Units[1].State.bActorRepresentation=true;},TEXT("follower Actor type corruption rejected atomically"));
  Reject([](UBattleSave& S){S.MapName=TEXT("OtherMap");},TEXT("foreign map rejected atomically"));
  Reject([](UBattleSave& S){S.WeaponRange+=100;},TEXT("changed range rules rejected atomically"));
  Reject([](UBattleSave& S){S.CombatStateVersion=4;},TEXT("unknown combat extension rejected"));
  Reject([](UBattleSave& S){S.Units[0].State.Shield=1;},TEXT("shield on rifle soldier rejected"));
  Reject([](UBattleSave& S){S.Units[0].State.LauncherWait=-1;},TEXT("negative launcher timer rejected"));
  Reject([](UBattleSave& S){S.SavedArtilleryShells.Add(FBattleSavedArtilleryShell());},TEXT("invalid shell shooter rejected"));
  Reject([](UBattleSave& S){S.Version=1;},TEXT("legacy schema cannot replace mixed match"));
  Reject([](UBattleSave& S){S.Squads[0].Destination=FVector(31000,21000,0);S.SavedPlans[1].Destination=S.Squads[0].Destination;},TEXT("unreachable saved route rejected before body replacement"));
  Check(Match->LastMessage==TEXT("Saved route unreachable; restore aborted"),TEXT("restore failure reaches route validation"));
  RestoreStageFailureAfter=2;
  Reject([](UBattleSave&){},TEXT("failure after staging Actor and Mass bodies rolls back atomically"));
  Check(Match->LastMessage==TEXT("Could not stage representations; restore aborted"),TEXT("restore failure reaches Actor/Mass staging"));
  RestoreStageFailureAfter=INDEX_NONE;
  Check(!Match->SaveMatch(TEXT("UnexpectedSlot")),TEXT("unapproved slot does not write"));
  // Capture and restore actual direct control through the same public APIs used by F8/F9.
  PC->ControlUnit(Actor(1));auto* Direct=Match->CaptureSnapshot();
  Check(Direct&&Direct->ControlledID==1&&Match->RestoreSnapshot(Direct)&&PC->GetPawn()==Actor(1),TEXT("TPS control restored to recreated leader"));
  PC->EnterTactical();Check(PC->GetPawn()!=Actor(1),TEXT("restored leader returns to RTS"));
  {
   // The override is accepted only by this smoke command and always restored on scope exit.
   struct FRouteSmokeScope
   {
    int32 Previous=BattleTactics::OverrideSaveRoutesForSmoke(0);
    ~FRouteSmokeScope(){BattleTactics::OverrideSaveRoutesForSmoke(Previous);}
   } RouteSmokeScope;
   const int32 RouteSID=4;
   // The same reachable mission point the setup gives this squad (line 46); a goal at the leader itself can be refused.
   const FVector RouteStep=FVector(-6000,-7000-(RouteSID-1)*650,0);
   Check(IssueOrder(RouteSID,EBattleOrder::AttackMove,RouteStep,true),TEXT("player route current step fixture"));
   Store->SetSquadDelegated(RouteSID,false);
   auto* RouteBaseline=Match->CaptureSnapshot();TArray<uint8> RouteBaselineBytes;
   Check(RouteBaseline&&UGameplayStatics::SaveGameToMemory(RouteBaseline,RouteBaselineBytes),TEXT("route-free OFF baseline encoded"));
   BattleTactics::OverrideSaveRoutesForSmoke(1);
   auto* EmptyRoutesOn=Match->CaptureSnapshot();TArray<uint8> EmptyRoutesOnBytes;
   Check(EmptyRoutesOn&&EmptyRoutesOn->PlayerRoutesVersion==0&&UGameplayStatics::SaveGameToMemory(EmptyRoutesOn,EmptyRoutesOnBytes)&&
    EmptyRoutesOnBytes==RouteBaselineBytes,TEXT("route-free ON save bytes identical to OFF (AI round trip)"));
   FPlayerRoute RouteFixture;
   RouteFixture.Lane=2;RouteFixture.Phase=1;RouteFixture.bMission=true;RouteFixture.bPath=true;RouteFixture.Order=EBattleOrder::Defend;
   RouteFixture.Step=RouteStep;RouteFixture.Goal=RouteStep+FVector(1800,0,0);RouteFixture.PhaseGoal=RouteFixture.Goal;
   RouteFixture.Waypoints={RouteStep+FVector(600,200,0),RouteStep+FVector(1200,-200,0)};
   RouteFixture.StepDeadline=Match->Elapsed+30.f;
   Match->PlayerRoutes.Add(RouteSID,RouteFixture);Match->PlayerRouteClock=.375f;
   BattleTactics::OverrideSaveRoutesForSmoke(0);
   auto* RoutesOff=Match->CaptureSnapshot();TArray<uint8> RoutesOffBytes;
   Check(RoutesOff&&RoutesOff->PlayerRoutesVersion==0&&RoutesOff->SavedPlayerRoutes.IsEmpty()&&RoutesOff->SavedPlayerRouteClock==0&&
    UGameplayStatics::SaveGameToMemory(RoutesOff,RoutesOffBytes)&&RoutesOffBytes==RouteBaselineBytes,
    TEXT("OFF omits route section and keeps route-free save bytes"));
   // Look for the actual ANSI property names emitted by the tagged SaveGame archive, not just default field values.
   auto HasRouteTag=[&](const ANSICHAR* Tag)
   {
    const int32 TagLength=FCStringAnsi::Strlen(Tag);
    for(int32 ByteIndex=0;ByteIndex+TagLength<=RoutesOffBytes.Num();++ByteIndex)
     if(FMemory::Memcmp(RoutesOffBytes.GetData()+ByteIndex,Tag,TagLength)==0)return true;
    return false;
   };
   Check(!HasRouteTag("PlayerRoutesVersion")&&!HasRouteTag("SavedPlayerRoutes")&&!HasRouteTag("SavedPlayerRouteClock"),TEXT("OFF archive contains no route property tags"));
   BattleTactics::OverrideSaveRoutesForSmoke(1);
   auto* RoutesOn=Match->CaptureSnapshot();TArray<uint8> RouteBytes;FString RouteError;
   const bool RouteEncoded=RoutesOn&&RoutesOn->Validate(RouteError)&&UGameplayStatics::SaveGameToMemory(RoutesOn,RouteBytes);
   auto* RouteDecoded=RouteEncoded?Cast<UBattleSave>(UGameplayStatics::LoadGameFromMemory(RouteBytes)):nullptr;
   Check(RouteDecoded&&RouteDecoded->PlayerRoutesVersion==1&&RouteDecoded->SavedPlayerRoutes.Num()==1&&
    Match->RestoreSnapshot(RouteDecoded),TEXT("player route extension codec and restore"));
   auto RouteKept=[&]()
   {
    const FPlayerRoute* ActualRoute=Match->PlayerRoutes.Find(RouteSID);
    return ActualRoute&&Match->PlayerRoutes.Num()==1&&ActualRoute->Lane==RouteFixture.Lane&&ActualRoute->Phase==RouteFixture.Phase&&
     ActualRoute->PhaseGoal==RouteFixture.PhaseGoal&&ActualRoute->bMission==RouteFixture.bMission&&ActualRoute->bPath==RouteFixture.bPath&&
     ActualRoute->Order==RouteFixture.Order&&ActualRoute->Goal==RouteFixture.Goal&&ActualRoute->Step==RouteFixture.Step&&
     ActualRoute->Waypoints==RouteFixture.Waypoints&&ActualRoute->StepDeadline==RouteFixture.StepDeadline&&Match->PlayerRouteClock==.375f;
   };
   Check(RouteKept(),TEXT("final Defend, goal, two waypoints, step, lane, flags and clocks survive"));
   if(RouteDecoded&&RouteDecoded->SavedPlayerRoutes.Num()==1&&RouteDecoded->SavedPlayerRoutes[0].Waypoints.Num()==2)
   {
    // Reuse the existing failed-restore checks, including nonempty runtime transient maps.
    auto* PreviousSaved=Saved;Saved=RouteDecoded;
    auto RejectRoute=[&](TFunctionRef<void(UBattleSave&)> CorruptRoute,const TCHAR* RouteLabel)
    {Reject(CorruptRoute,RouteLabel);Check(RouteKept(),TEXT("failed restore retains complete player route and clock"));};
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].SquadID=999999;},TEXT("bad player route squad rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].SquadID=26;},TEXT("enemy player route rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].SquadID=50;},TEXT("disabled player route rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].Waypoints[0].X=std::numeric_limits<double>::quiet_NaN();},TEXT("NaN player waypoint rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].Waypoints[0].X=1.e9;},TEXT("out-of-map player waypoint rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].Waypoints.Init(FVector::ZeroVector,97);},TEXT("excessive player waypoint count rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){const auto DuplicateRoute=RouteSave.SavedPlayerRoutes[0];RouteSave.SavedPlayerRoutes.Add(DuplicateRoute);},TEXT("duplicate player route rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].Order=255;},TEXT("invalid final order rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.SavedPlayerRoutes[0].DeadlineRemaining=std::numeric_limits<double>::infinity();},TEXT("nonfinite route deadline rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.PlayerRoutesVersion=2;},TEXT("unknown player route version rejected atomically"));
    RejectRoute([](UBattleSave& RouteSave){RouteSave.PlayerRoutesVersion=0;},TEXT("orphan player route section rejected atomically"));
    auto* ExpiredRoute=DuplicateObject<UBattleSave>(RouteDecoded,this);
    ExpiredRoute->Elapsed=10;ExpiredRoute->SavedPlayerRoutes[0].DeadlineRemaining=-2.;
    ExpiredRoute->SavedPlayerRoutes[0].Step=FVector(FLT_MAX);ExpiredRoute->SavedPlayerRoutes[0].PhaseGoal=FVector(FLT_MAX);
    Check(ExpiredRoute->Validate(RouteError),TEXT("expired route deadline and lane-reset sentinels remain valid"));
    BattleTactics::OverrideSaveRoutesForSmoke(0);
    RejectRoute([](UBattleSave&){},TEXT("route extension rejected atomically when switch OFF"));
    Check(Match->LastMessage==TEXT("Player route save requires BattleSaveRoutes=1"),TEXT("route policy mismatch is explicit"));
    BattleTactics::OverrideSaveRoutesForSmoke(1);RestoreStageFailureAfter=2;
    RejectRoute([](UBattleSave&){},TEXT("route retained after body staging failure"));RestoreStageFailureAfter=INDEX_NONE;
    Saved=PreviousSaved;
   }
   Check(Match->RestoreSnapshot(RouteBaseline)&&Match->PlayerRoutes.IsEmpty(),TEXT("legacy route-free save still clears routes with switch ON"));
   BattleTactics::OverrideSaveRoutesForSmoke(0);Match->PlayerRoutes.Add(RouteSID,RouteFixture);
   Check(Match->RestoreSnapshot(RouteBaseline)&&Match->PlayerRoutes.IsEmpty(),TEXT("legacy route-free save still clears routes with switch OFF"));
  }
  auto* Legacy=DuplicateObject<UBattleSave>(Saved,this);Legacy->CombatStateVersion=0;Legacy->ArtilleryTrialFlags=0;for(auto& E:Legacy->Units)E.State.SPGAmmo=0;Legacy->SavedArtillery.Reset();Legacy->SavedArtilleryShells.Reset();
  Legacy->SavedGroups.Reset();Legacy->SavedSquadGroup.Reset();Legacy->SavedNextGroupID=1;Legacy->SavedCarChoices.Reset();
  Check(Match->RestoreSnapshot(Legacy)&&Artillery.IsEmpty()&&ArtilleryShells.IsEmpty(),TEXT("pre-extension mixed snapshot remains loadable"));
  Check(Match->RestoreSnapshot(Saved),TEXT("restore fixture for live continuation"));
  PC->QuickSaveSlot=TEXT("FutureBattleSmokeOnly");PC->SaveMatch();Resource(Follower).HP=1;PC->LoadMatch();
  Check(Store->Find(Follower)->HP==55&&Match->LastMessage==TEXT("Match restored"),TEXT("F8/F9 controller callbacks reach mixed save/load using isolated test slot"));
  PC->QuickSaveSlot=TEXT("FutureBattleQuick01");
  Mode->bCombatPaused=false;Match->DirectorClock=0;Match->DelegationClock=0;
  SmokePosition=Position(1);SmokeStage=1;Later(2);return;
 }
 if(SmokeStage==1)
 {
  Check(FVector::Dist2D(Position(1),SmokePosition)>50,TEXT("restored retreat continues through real Actor movement"));
  Check(Store->Find(Follower)->ReloadEndTime==0&&Store->Find(Follower)->Ammo>5,TEXT("restored follower reload completes"));
  Check(Bodies.Num()==Total&&Mode->Units.Num()==50,TEXT("live continuation preserves mixed representation"));
  FString LiveError;auto* Live=Match->CaptureSnapshot();
  Check(Live&&Live->Validate(LiveError)&&Match->SaveMatch(TEXT("FutureBattleSmokeOnly")),TEXT("live mixed combat remains serializable after clocks advance"));
  if(!LiveError.IsEmpty())UE_LOG(LogFutureBattle,Error,TEXT("MIXED_SAVE_LIVE_VALIDATION %s"),*LiveError);
  Mode->bCombatPaused=true;SmokeStage=3;
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
  if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-restored.png"),true,false);
  Later(1);return;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_SAVE_RESULT failures=%d"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(true,SmokeFailures?1:0);
}
