#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"
#include "Kismet/GameplayStatics.h"

void UBattleLeaderRuntime::RunAreaSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK)++SmokeFailures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* M=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Place=[&](int32 ID,FVector P){auto& B=Bodies.FindChecked(ID);if(B.Actor.IsValid()){B.Actor->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->SetActorRotation(FRotator::ZeroRotator);}else{B.Capsule->SetWorldLocation(P);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=P;F.Forward=FVector::ForwardVector;}};
 if(SmokeStage==0)
 {
  Mode->CombatStartsAt=1.e6;Mode->bCombatPaused=false;M->bAreaIncomeHeld=true; // exact resource fixtures; income is checked separately below
  Check(bArea&&M->bArea&&M->BaseActors.Num()==2&&M->Credits[0]==100&&M->Credits[1]==100,TEXT("area starts with symmetric bases and resources"));
  for(int32 ID=1;ID<=10;++ID)Place(ID,FVector(-26500+(ID/5)*130,(ID%5-2)*130,100));
  Store->ApplyDamage(2,1,10000);Store->ApplyDamage(3,1,10000);
  M->UpdateArea(1);Check(M->Area.Replacements.Contains(2)&&M->Area.Replacements.Contains(3)&&M->Credits[0]==100,TEXT("home queues missing soldiers without spending resources"));
  const int32 BeforeScore=M->AreaScore(0);
  for(int32 I=0;I<5;++I)M->UpdateArea(1);
  Check(!Store->Find(2)->IsAlive()&&M->AreaScore(0)==BeforeScore,TEXT("incomplete soldiers do not increase score"));
  Mode->bCombatPaused=true;FString Error;auto* Save=M->CaptureSnapshot();
  Check(Save&&Save->Version==3&&Save->Validate(Error),TEXT("version three validates pending replenishment"));
  if(Save){Save->AllyCredits=1;Check(!M->RestoreSnapshot(Save)&&M->Credits[0]==100,TEXT("oversubscribed replacement resources rejected"));Save->AllyCredits=100;Save->Area.Replacements.Add(1,5);Check(!M->RestoreSnapshot(Save)&&M->Credits[0]==100,TEXT("invalid leader replacement rejected without mutation"));}
  PC->QuickSaveSlot=TEXT("FutureBattleSmokeOnly");PC->SaveMatch();M->Area.Replacements.Reset();PC->LoadMatch();
  Check(M->Area.Replacements.FindRef(2)==5&&M->Credits[0]==100,TEXT("F8 F9 preserve pending clocks and unspent resources"));
  auto* Blocker=GetWorld()->SpawnActor<AStaticMeshActor>(Position(1)+FVector(0,0,200),FRotator::ZeroRotator);
  Blocker->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Blocker->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Blocker->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);Blocker->SetActorScale3D(FVector(20,20,10));Blocker->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
  auto* Solid=NewObject<UBoxComponent>(Blocker);Blocker->AddInstanceComponent(Solid);Solid->SetBoxExtent(FVector(1000,1000,500));Solid->SetCollisionProfileName(TEXT("BlockAll"));Solid->SetCanEverAffectNavigation(false);Solid->RegisterComponent();Solid->SetWorldLocation(Position(1)+FVector(0,0,200));
  Check(GetWorld()->OverlapBlockingTestByChannel(Position(1)+FVector(250,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,90)),TEXT("blocked spawn fixture has a real blocking volume"));
  Mode->bCombatPaused=false;for(int32 I=0;I<5;++I)M->UpdateArea(1);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_BLOCKED credits=%d records=%d pending=%.2f bodies=%d home=%s blocker=%s"),M->Credits[0],Store->GetUnits().Num(),M->Area.Replacements.FindRef(2),Bodies.Num(),*Position(1).ToCompactString(),*Blocker->GetActorLocation().ToCompactString());
  Check(M->Credits[0]==100&&Store->GetUnits().Num()==500&&M->Area.Replacements.FindRef(2)==10,TEXT("blocked completion retains resources IDs and pending work"));
  Blocker->Destroy();M->UpdateArea(1);
  Check(M->Credits[0]==98&&Store->Find(2)->bRetired&&Store->Find(3)->bRetired&&Store->Find(501)&&Store->Find(502),TEXT("completion spends one resource per fresh soldier identity"));
  Check(Bodies.Num()==500&&Store->GetUnits().Num()==502&&M->AreaScore(0)==BeforeScore,TEXT("replenishment preserves body capacity and resource score"));
  Save=M->CaptureSnapshot();Check(Save&&Save->Validate(Error)&&M->RestoreSnapshot(Save),TEXT("retired contacts and new roster restore atomically"));
  M->Objectives[0].Owner=1;M->Area.KnownOwners[0]=-2;M->UpdateArea(1);
  Check(M->ObservedOwner(0,0)==-2,TEXT("unobserved objective ownership remains unknown"));
  const int32 Assignment=M->ChooseObjective(0,0);M->Objectives[0].Owner=0;
  Check(M->ChooseObjective(0,0)==Assignment,TEXT("unseen true ownership cannot alter AI assignment"));
  {
   // S1 point allocation (default ON): spread by fixed per-squad slot, own observed points weigh less, ~20% home assault.
   const TArray<int32> SavedKnown=M->Area.KnownOwners;const int32 SavedOwner1=M->Objectives[1].Owner;
   Check(BattleTactics::AreaStrategy().bEnabled,TEXT("S1 point allocation is ON by default"));
   auto Spread=[&](int32 (&Count)[3])->int32{int32 Base=0;Count[0]=Count[1]=Count[2]=0;for(int32 ID=1;ID<=25;++ID){bool bBase=false;const int32 T=M->AreaStrategyTarget(ID,0,bBase);if(bBase)++Base;else if(T>=0&&T<3)++Count[T];}return Base;};
   for(int32 P=0;P<3;++P)M->Area.KnownOwners[P]=-2;
   int32 Unknown[3];const int32 Base=Spread(Unknown);
   Check(Base>=3&&Base<=7&&Unknown[0]>0&&Unknown[1]>0&&Unknown[2]>0,TEXT("S1 spreads 25 squads over all three points with about 20 percent home assault"));
   M->Area.KnownOwners[0]=0;int32 OwnFirst[3];Spread(OwnFirst);
   Check(OwnFirst[0]<Unknown[0]&&OwnFirst[1]+OwnFirst[2]>Unknown[1]+Unknown[2],TEXT("S1 sends fewer squads to a point observed as own"));
   bool bBase=false;const int32 Before=M->AreaStrategyTarget(7,0,bBase);M->Objectives[1].Owner=0;
   Check(M->AreaStrategyTarget(7,0,bBase)==Before,TEXT("unseen true ownership cannot alter S1 allocation"));
   M->Objectives[1].Owner=SavedOwner1;M->Area.KnownOwners=SavedKnown;
  }
  const FVector ObserverStart=Position(11);Place(11,M->Objectives[0].Position+FVector(-200,0,100));M->UpdateArea(1);
  Check(M->ObservedOwner(0,0)==0,TEXT("actual own line of sight updates point ownership"));Place(11,ObserverStart);
  M->Objectives[0].Owner=1;M->UpdateArea(1);Check(M->ObservedOwner(0,0)==0,TEXT("lost sight retains old ownership instead of reading truth"));
  M->Objectives[0].Owner=INDEX_NONE;
  Store->ApplyDamage(4,1,10000);M->UpdateArea(1);const int32 Credits=M->Credits[0];
  Store->ApplyDamage(1,1,10000);M->UpdateArea(1);
  Check(!M->Area.Replacements.Contains(4)&&M->Credits[0]==Credits,TEXT("leader death cancels replacement without spending"));
  Store->ApplyDamage(12,1,10000);Store->ApplyDamage(13,1,10000);Store->SetSquadDelegated(2,true);
  auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();auto Observation=T->Observe(*Store->FindSquad(2));Observation.Contacts.Reset();Observation.RecentDamage=0;
  const FVector Mission=Store->FindSquad(2)->MissionDestination;T->Evaluate(2,Observation,1);
  Check(Store->FindSquad(2)->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY"))&&Store->FindSquad(2)->MissionDestination.Equals(Mission),TEXT("depleted delegated squad returns home while preserving mission"));
  Store->CancelSquadTacticalPlan(2,TEXT("RESOURCE EXHAUSTION FIXTURE"));M->Credits[0]=0;T->Evaluate(2,Observation,1);
  Check(!Store->FindSquad(2)->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")),TEXT("armed squad does not repeatedly return when replacement resources are exhausted"));M->Credits[0]=Credits;
  Mode->bBattleEnded=false;M->Phase=EBattleMatchPhase::Combat;
  Place(11,FVector(27400,0,100));FVector Target;
  Check(M->CanFireAtBase(11,Target),TEXT("own line of sight and weapon range permit base engagement"));
  auto* Shooter=const_cast<FBattleUnitState*>(Store->Find(11));Shooter->NextShotTime=0;
  Check(Fire(11,Position(11)+FVector(0,0,60),(M->BaseActors[1]->GetActorLocation()-(Position(11)+FVector(0,0,60))).GetSafeNormal(),true)&&M->Area.BaseHP[1]<1000,TEXT("shared player and AI shot path damages physical enemy base"));
  Check(!M->DamageBase(M->BaseActors[0],0,100),TEXT("friendly base cannot be damaged"));
  // New-squad hiring (squad 1 lost its leader above, so team 0 has one free slot).
  {
   const int32 Before=M->Credits[0],Records=Store->GetUnits().Num(),SquadCount=Store->GetSquads().Num(),Active=M->ActiveSquads(0);
   Check(Active==24&&!M->HireSquad(0,EBattleTraining::Veteran)&&!M->HireSquad(0,EBattleTraining::Recruit)&&M->Credits[0]==Before&&Store->GetUnits().Num()==Records,TEXT("area hiring accepts regulars only without mutation"));
   M->Credits[0]=14;Check(!M->HireSquad(0,EBattleTraining::Regular)&&M->Credits[0]==14&&Store->GetSquads().Num()==SquadCount,TEXT("area hiring needs 15 resources"));
   M->Credits[0]=Before;int32 MaxID=0;for(const auto& U:Store->GetUnits())MaxID=FMath::Max(MaxID,U.Key);
   Check(M->HireSquad(0,EBattleTraining::Regular)&&M->Credits[0]==Before-15&&M->Area.HireSpent[0]==15&&Store->GetUnits().Num()==Records+10&&Store->GetSquads().Num()==SquadCount+1&&M->ActiveSquads(0)==25,TEXT("area hire charges 15 for ten fresh soldiers"));
   int32 NewSID=0;for(const auto& P:Store->GetSquads())NewSID=FMath::Max(NewSID,P.Key);const auto* NewSquad=Store->FindSquad(NewSID);
   bool Fresh=NewSquad&&NewSquad->Team==0&&NewSquad->Members.Num()==10&&NewSquad->CommandStatus==EBattleCommandStatus::Active&&!NewSquad->bPlayerDelegated;
   if(Fresh)for(int32 ID:NewSquad->Members)Fresh&=ID>MaxID&&Store->Find(ID)->IsAlive()&&Store->Find(ID)->bActorRepresentation==(ID==NewSquad->LeaderUnitID);
   const int32 NewLeader=NewSquad?NewSquad->LeaderUnitID:0;
   Check(Fresh&&NewSID==51&&FVector::Dist2D(Position(NewSquad->LeaderUnitID),M->AreaHome(0))<=2500,TEXT("hired squad gets a new number, new IDs, an Actor leader and home placement"));
   Check(!M->HireSquad(0,EBattleTraining::Regular)&&M->Credits[0]==Before-15,TEXT("no free slot rejects hiring at 25 active squads"));
   Mode->bCombatPaused=true;Save=M->CaptureSnapshot();
   Check(Save&&Save->Validate(Error)&&Save->Squads.Num()==51,TEXT("save with a hired squad validates"));
   if(Save)
   {
    Save->Area.HireSpent[0]=0;Check(!M->RestoreSnapshot(Save)&&M->Credits[0]==Before-15,TEXT("hire spending outside conservation rejected"));Save->Area.HireSpent[0]=15;
    Check(M->RestoreSnapshot(Save)&&Store->GetSquads().Num()==SquadCount+1&&M->Area.HireSpent[0]==15&&M->Credits[0]==Before-15&&Bodies.Contains(NewLeader)&&Store->Find(NewLeader)->IsAlive(),TEXT("hired squad survives restore"));
   }
   Mode->bCombatPaused=false;
   // A purchase that leaves fewer resources than queued replenishments cancels the least advanced ones.
   Store->ApplyDamage(21,1,10000);M->UpdateArea(1);
   M->Credits[0]=16;M->Area.Replacements.Add(12,5);M->Area.Replacements.Add(13,1);
   Check(M->ActiveSquads(0)==24&&M->HireSquad(0,EBattleTraining::Regular)&&M->Credits[0]==1&&M->Area.Replacements.Contains(12)&&!M->Area.Replacements.Contains(13)&&M->Area.HireSpent[0]==30,TEXT("purchase cancels uncovered replenishment reservations"));
   // The fixture forced 16 resources; restore the conserving amount (100 - replenished - hire spending) and re-validate.
   int32 Retired0=0;for(const auto& U:Store->GetUnits())Retired0+=U.Value.bRetired&&U.Value.Team==0;
   M->Credits[0]=100-Retired0-M->Area.HireSpent[0];
   Mode->bCombatPaused=true;Save=M->CaptureSnapshot();Check(Save&&Save->Validate(Error)&&Save->Squads.Num()==52,TEXT("two hires and a reservation validate"));Mode->bCombatPaused=false;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_HIRE_SMOKE squads=%d records=%d credits=%d validation=%s"),Store->GetSquads().Num(),Store->GetUnits().Num(),M->Credits[0],*Error);
   M->Area.Replacements.Reset();
  }
  // Resource income: base 1 plus 1 per actually owned point every 20 s, for both teams at once.
  {
   M->bAreaIncomeHeld=false;for(auto& P:M->Objectives)P.Owner=INDEX_NONE;M->Objectives[0].Owner=0;M->Objectives[2].Owner=0;
   const int32 C0=M->Credits[0],C1=M->Credits[1],I0=M->Area.Income[0],I1=M->Area.Income[1];M->Area.IncomeClock=0;
   for(int32 I=0;I<19;++I)M->UpdateArea(1);
   Check(M->Credits[0]==C0&&M->Credits[1]==C1,TEXT("no income before the 20 second payment"));
   M->UpdateArea(1);
   // Fixture: team 0 owns two points, team 1 none. Amounts follow the configured income (INC4 = 4 + 4 per point since 2026-11-16).
   const int32 G0=BattleTactics::AreaIncome().Base+2*BattleTactics::AreaIncome().PerPoint,G1=BattleTactics::AreaIncome().Base;
   Check(M->Credits[0]==C0+G0&&M->Credits[1]==C1+G1&&M->Area.Income[0]==I0+G0&&M->Area.Income[1]==I1+G1,TEXT("payment is base plus per owned point (configured amounts)"));
   Mode->bCombatPaused=true;Save=M->CaptureSnapshot();
   Check(Save&&Save->Validate(Error),TEXT("income joins resource conservation"));
   if(Save){Save->Area.Income[0]-=1;Check(!M->RestoreSnapshot(Save)&&M->Credits[0]==C0+G0,TEXT("income outside conservation rejected"));}
   Mode->bCombatPaused=false;M->bAreaIncomeHeld=true;
   for(auto& P:M->Objectives)P.Owner=INDEX_NONE;
  }
  for(auto& P:M->Objectives)P.Owner=INDEX_NONE;
  M->Credits[0]=100;M->Credits[1]=100;M->Credits[1]=100-(M->AreaScore(1)-M->AreaScore(0)); // formula assumes Credits[1]==100 (income may have changed it)
  M->Elapsed=600;M->CheckTimeLimit();Check(M->Winner==INDEX_NONE&&M->Phase==EBattleMatchPhase::Finished,TEXT("equal final resource scores produce a draw"));
  M->Phase=EBattleMatchPhase::Combat;Mode->bBattleEnded=false;M->Credits[0]=0;M->Elapsed=100;
  Check(M->DamageBase(M->BaseActors[1],0,10000)&&M->Winner==0&&Mode->bBattleEnded,TEXT("base destruction wins regardless of zero resources"));
  Mode->bCombatPaused=true;PC->EnterTactical();
  {
   PC->CycleGameSpeed();const float X2=UGameplayStatics::GetGlobalTimeDilation(GetWorld());PC->CycleGameSpeed();const float X4=UGameplayStatics::GetGlobalTimeDilation(GetWorld());
   PC->CycleGameSpeed();Check(X2==2.f&&X4==4.f&&UGameplayStatics::GetGlobalTimeDilation(GetWorld())==1.f&&PC->GetGameSpeed()==1,TEXT("tactical speed toggle cycles x2 x4 x1"));
  }
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-restored.png"),true,false);
  SmokeStage=1;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunAreaSmoke,.5f,false);return;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
