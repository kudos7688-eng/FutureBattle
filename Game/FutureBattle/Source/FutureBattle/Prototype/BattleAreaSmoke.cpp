#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "InputKeyEventArgs.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "BattleRanges.h"
#include "BattleWarScore.h"
#include "BattleSuppression.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "BattleUnit.h"
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
   // Heavy infantry (squad type 1): a freed slot (leader 41 lost), 15 resources, 6 soldiers with support guns at 3x health;
   // the type and health survive save and restore.
   {
    Store->ApplyDamage(41,1,10000);M->UpdateArea(1);
    if(M->Credits[0]<BattleSquadType::AreaCost(BattleSquadType::Heavy)){const int32 Add=BattleSquadType::AreaCost(BattleSquadType::Heavy)-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0],Spent0=M->Area.HireSpent[0],HeavyRecords=Store->GetUnits().Num();
    const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Heavy);
    int32 HSID=0;for(const auto& P:Store->GetSquads())HSID=FMath::Max(HSID,P.Key);const auto* HS=Store->FindSquad(HSID);
    bool Heavy=Hired&&HS&&HS->SquadType==BattleSquadType::Heavy&&HS->Members.Num()==BattleSquadType::Members(BattleSquadType::Heavy)&&M->Credits[0]==C0-BattleSquadType::AreaCost(BattleSquadType::Heavy)&&M->Area.HireSpent[0]==Spent0+BattleSquadType::AreaCost(BattleSquadType::Heavy)&&Store->GetUnits().Num()==HeavyRecords+HS->Members.Num();
    if(Heavy)for(int32 ID:HS->Members){const auto* U=Store->Find(ID);Heavy&=U&&U->Weapon==EBattleWeapon::SupportGun&&FMath::IsNearlyEqual(U->HP,GetDefault<UBattleTuning>()->Health*3.f);}
    Heavy&=FMath::IsNearlyEqual(BattleSquadType::DamageScale(BattleSquadType::Heavy),1.5f)&&FMath::IsNearlyEqual(BattleSquadType::DamageScale(BattleSquadType::Rifle),1.f);
    Check(Heavy,TEXT("heavy infantry: 6 soldiers, support guns, 3x health, 15 resources, 1.5x damage"));
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);const auto* After=Store->FindSquad(HSID);
    Check(Restored&&After&&After->SquadType==BattleSquadType::Heavy&&FMath::IsNearlyEqual(Store->Find(After->Members[1])->HP,GetDefault<UBattleTuning>()->Health*3.f),TEXT("heavy infantry survives save and restore"));
    Mode->bCombatPaused=false;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_HEAVY_SMOKE squad=%d members=%d validation=%s"),HSID,HS?HS->Members.Num():0,*Error);
   }
   // Scout pairs (squad type 2): 2 rifles for 8 resources, at most 3 per team, long sight while still, the survivor takes
   // command when the leader falls, never replenished, and the handed-over squad survives save and restore.
   {
    for(int32 Leader:{51,61,71,81})Store->ApplyDamage(Leader,1,10000);
    M->UpdateArea(1);M->Area.Replacements.Reset();
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::Scout);
    if(M->Credits[0]<Cost*4){const int32 Add=Cost*4-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    TArray<int32> Scouts;bool Bought=true;
    for(int32 I=0;I<3;++I)
    {
     const int32 C0=M->Credits[0];Bought&=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Scout)&&M->Credits[0]==C0-Cost;
     int32 SID=0;for(const auto& P:Store->GetSquads())SID=FMath::Max(SID,P.Key);Scouts.Add(SID);
     const auto* S=Store->FindSquad(SID);
     Bought&=S&&S->SquadType==BattleSquadType::Scout&&S->Members.Num()==2&&S->DisplayName.Contains(TEXT("偵察組"));
     if(S)for(int32 ID:S->Members)Bought&=Store->Find(ID)->Weapon==EBattleWeapon::Carbine;
    }
    const int32 C1=M->Credits[0];const bool Limited=!M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Scout)&&M->Credits[0]==C1&&M->ActiveScouts(0)==3;
    Check(Bought&&Cost==8&&Limited,TEXT("scout pairs: 2 rifles, 8 resources, at most 3 per team"));
    // AI use: a post 30 m behind an own front point (long still-sight), and the intent keeps two pairs by default.
    {
     const FVector Post=M->AreaScoutPost(0,Scouts[1]);double Near=DBL_MAX;FVector Point=FVector::ZeroVector;
     for(const auto& O:M->Objectives){const double D=FVector::Dist2D(Post,O.Position);if(D<Near){Near=D;Point=O.Position;}}
     bool AtLookout=false;for(const FVector& L:BattleFieldLayout::Lookouts(GetWorld()))AtLookout|=FVector::Dist2D(Post,L)<1500;
     Check(((Near>1000&&Near<9000&&FVector::Dist2D(Post,M->AreaHome(1))>FVector::Dist2D(Point,M->AreaHome(1)))||AtLookout)&&M->AreaIntent(0).ScoutKeep==0,TEXT("scout post at a lookout or behind an own front point; the AI keeps no pairs by default"));
    }
    // Reports (stage 3): a sighting beyond normal sight is a scout long contact (hidden until reported); a report's positions
    // reach the team's plans for ScoutReportMemory seconds.
    {
     const int32 Eye=Store->FindSquad(Scouts[1])->LeaderUnitID;const float Normal=Store->Find(Eye)->Perception.SightRange;
     FBattleContact Far;Far.LastKnownPosition=Position(Eye)+FVector(Normal+5000.f,0,0);FBattleContact Close;Close.LastKnownPosition=Position(Eye)+FVector(Normal*.5f,0,0);
     const bool Long=M->IsScoutLongContact(Eye,Far)&&!M->IsScoutLongContact(Eye,Close)&&!M->IsScoutLongContact(1,Far);
     auto& Rep=M->ScoutReports.FindOrAdd(Scouts[1]);Rep.ReportedAt=M->Elapsed;Rep.Positions={FVector(1000,0,0),FVector(2000,0,0)};
     TMap<int32,FVector> Seen;M->AddScoutReportSightings(0,Seen);const int32 Mine=Seen.Num();Seen.Reset();M->AddScoutReportSightings(1,Seen);
     M->ScoutReports.Remove(Scouts[1]);
     Check(Long&&Mine==2&&Seen.IsEmpty(),TEXT("scout long sightings wait for the report; reports reach only their own team"));
     // Intel trial: 40 reported enemies at an own point make it the team's threat (known positions only).
     int32 OwnPoint=-1;for(int32 I=0;I<M->Objectives.Num();++I)if(M->ObservedOwner(0,I)==0){OwnPoint=I;break;}
     if(OwnPoint>=0)
     {
      auto& Threat=M->ScoutReports.FindOrAdd(Scouts[1]);Threat.ReportedAt=M->Elapsed;Threat.Positions.Init(M->Objectives[OwnPoint].Position+FVector(3000,0,0),40);
      M->IntelViews[0].At=-1;const bool Seen40=M->AreaIntelView(0).EnemyAt[OwnPoint]>=40&&M->AreaIntelThreatAt(0,OwnPoint)&&M->AreaIntelThreat(0)==OwnPoint;
      M->ScoutReports.Remove(Scouts[1]);M->IntelViews[0].At=-1;
      Check(Seen40&&!M->AreaIntelThreatAt(0,OwnPoint),TEXT("intel: reported enemies at an own point make it the threat"));
     }
     else Check(false,TEXT("intel: fixture has an observed own point"));
    }
    // Retreat: an AI-led pair that is hit falls back up to 40 m, its report pauses (not restarts), and its next post is shifted sideways.
    {
     const int32 SID=Scouts[2];Store->SetSquadDelegated(SID,true);const FVector PostBefore=M->AreaScoutPost(0,SID);
     M->UpdateScoutReports();M->ScoutReports.FindOrAdd(SID).Started=M->Elapsed;M->ScoutReports.FindOrAdd(SID).LastLong=M->Elapsed;Store->ApplyDamage(Store->FindSquad(SID)->Members[1],1,10);M->UpdateScoutReports();
     const auto* Rep=M->ScoutReports.Find(SID);const FVector PostAfter=M->AreaScoutPost(0,SID);
     const bool Back=Rep&&Rep->RetreatUntil>M->Elapsed&&Rep->Burns==1&&Rep->PauseStart>=0&&Rep->Started>=0&&FVector::Dist2D(Rep->RetreatPoint,Position(Store->FindSquad(SID)->LeaderUnitID))<=4200.f;
     if(Rep)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_SCOUT_RETREAT_SMOKE until=%.1f now=%.1f burns=%d pause=%.1f started=%.1f dist=%.0f post=%.0f danger=%.1f"),Rep->RetreatUntil,M->Elapsed,Rep->Burns,Rep->PauseStart,Rep->Started,FVector::Dist2D(Rep->RetreatPoint,Position(Store->FindSquad(SID)->LeaderUnitID)),FVector::Dist2D(PostBefore,PostAfter),Rep->LastDanger);
     Check(Back&&FVector::Dist2D(PostBefore,PostAfter)>1000,TEXT("scout pair falls back at most 40 m when hit, pauses its report and moves its post"));
     Store->SetSquadDelegated(SID,false);M->ScoutReports.Remove(SID);
    }
    const auto* First=Store->FindSquad(Scouts[0]);const int32 OldLeader=First->LeaderUnitID,Heir=First->Members[1];
    const float Still=ScoutStillSight(OldLeader);
    Check(Still>=29999.f&&Still==ScoutStillSight(Heir)&&ScoutStillSight(1)==0.f,TEXT("still scouts see 300 m, other soldiers keep their range"));
    Store->ApplyDamage(OldLeader,1,10000);
    First=Store->FindSquad(Scouts[0]);
    const bool Handed=First->CommandStatus==EBattleCommandStatus::Active&&First->LeaderUnitID==Heir&&Actor(Heir)&&Store->CanSelectLeader(Heir,0);
    M->UpdateArea(1);M->UpdateArea(1);
    Check(Handed&&!M->Area.Replacements.Contains(OldLeader),TEXT("scout survivor takes command and the pair is not replenished"));
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);const auto* After=Store->FindSquad(Scouts[0]);
    Check(Restored&&After&&After->SquadType==BattleSquadType::Scout&&After->LeaderUnitID==Heir&&After->CommandStatus==EBattleCommandStatus::Active&&Actor(Heir),TEXT("handed-over scout pair survives save and restore"));
    Mode->bCombatPaused=false;
    Store->ApplyDamage(Heir,1,10000);
    Check(Store->FindSquad(Scouts[0])->CommandStatus==EBattleCommandStatus::Disabled,TEXT("scout pair stops when both are lost"));
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_SCOUT_SMOKE squads=%d,%d,%d heir=%d validation=%s"),Scouts[0],Scouts[1],Scouts[2],Heir,*Error);
   }
   // Anti-tank team (squad type 3): 4 soldiers, two anti-tank weapons (1 loaded + 3 reserve, never refilled beyond that), 12
   // resources; its explosion hurts an enemy at the impact by ATSplashDamage and never an ally. Lookouts exist on this map.
   {
    for(int32 Leader:{91,101})Store->ApplyDamage(Leader,1,10000);
    M->UpdateArea(1);M->Area.Replacements.Reset();
    {
     const int32 SCost=BattleSquadType::AreaCost(BattleSquadType::Supply);
     if(M->Credits[0]<SCost){const int32 Add=SCost-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
     const int32 C1=M->Credits[0];const bool SHired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Supply);
     int32 SupplyID=0;for(const auto& P:Store->GetSquads())SupplyID=FMath::Max(SupplyID,P.Key);const auto* SS=Store->FindSquad(SupplyID);
     bool SGood=SHired&&SS&&SCost==10&&M->Credits[0]==C1-SCost&&SS->SquadType==BattleSquadType::Supply&&SS->Members.Num()==3&&SS->DisplayName.Contains(TEXT("補給班"));
     if(SGood){const FVector At=Position(SS->LeaderUnitID);SGood=M->SupplyTeamNear(0,At+FVector(2000,0,0))&&!M->SupplyTeamNear(0,At+FVector(4000,0,0))&&!M->SupplyTeamNear(1,At);}
     Check(SGood,TEXT("supply team: 3 soldiers, 10 resources, own squads within 30 m of its leader are supplied"));
    }
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::AntiTank);
    if(M->Credits[0]<Cost){const int32 Add=Cost-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0];const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::AntiTank);
    int32 SID=0;for(const auto& P:Store->GetSquads())SID=FMath::Max(SID,P.Key);const auto* AS=Store->FindSquad(SID);
    bool ATGood=Hired&&Cost==12&&M->Credits[0]==C0-Cost&&AS&&AS->SquadType==BattleSquadType::AntiTank&&AS->Members.Num()==4&&AS->DisplayName.Contains(TEXT("対戦車班"));
    int32 Gunners=0;if(ATGood)for(int32 ID:AS->Members){const auto* U=Store->Find(ID);if(U->Weapon==EBattleWeapon::AntiTank){++Gunners;ATGood&=U->Ammo==1&&U->Reserve==3;}}
    if(ATGood){const int32 G=AS->Members[1];Store->AddReserve(G,90);ATGood&=Store->Find(G)->Reserve==3;}
    Check(ATGood&&Gunners==2,TEXT("anti-tank team: 4 soldiers, 2 anti-tank weapons with 4 rounds, 12 resources"));
    if(AS)
    {
     // Reserve rounds move between squad mates with the same kind of weapon, up to the receiver's limit; nothing is created.
     const int32 Rifle0=AS->Members[0],Rifle3=AS->Members[3],Gun=AS->Members[1];auto& Taker=Store->Units.FindChecked(Rifle0);
     const int32 Full=Taker.Reserve,Giver=Store->Find(Rifle3)->Reserve;Taker.Reserve=0;
     bool Share=Store->MoveReserve(Rifle3,Rifle0,30)==30&&Taker.Reserve==30&&Store->Find(Rifle3)->Reserve==Giver-30;
     Share&=Store->MoveReserve(Gun,Rifle0,30)==0&&Store->MoveReserve(Rifle0,Rifle0,30)==0&&Store->MoveReserve(Rifle3,Rifle0,1000)==Giver-30&&Taker.Reserve==Giver;
     Store->Units.FindChecked(Rifle3).Reserve=Giver;Taker.Reserve=Full;
     Check(Share&&Store->MoveReserve(Rifle3,Rifle0,30)==0,TEXT("reserve rounds move between squad mates of the same kind of weapon without creating any"));
    }
    int32 Enemy=INDEX_NONE;for(const auto& P:Store->GetUnits())if(P.Value.Team==1&&P.Value.IsAlive()&&Bodies.Contains(P.Key)){Enemy=P.Key;break;}
    const int32 Ally=AS?AS->Members[0]:INDEX_NONE;
    const float EnemyBefore=Enemy!=INDEX_NONE?Store->Find(Enemy)->HP:0.f,AllyBefore=Ally!=INDEX_NONE?Store->Find(Ally)->HP:0.f;
    const int32 ATHit=AS&&Enemy!=INDEX_NONE?Explode(AS->Members[1],Position(Enemy),INDEX_NONE):0;
    const float Lost=Enemy!=INDEX_NONE?EnemyBefore-Store->Find(Enemy)->HP:0.f;
    Check(ATHit>=1&&FMath::IsNearlyEqual(Lost,FMath::Min(EnemyBefore,GetDefault<UBattleTuning>()->ATSplashDamage),1.f)&&Store->Find(Ally)->HP==AllyBefore,TEXT("anti-tank explosion hurts enemies around the impact, never allies"));
    Check(BattleFieldLayout::Lookouts(GetWorld()).Num()==2&&!HasLongSight(1),TEXT("two lookouts; a soldier away from them has normal sight"));
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_AT_SMOKE squad=%d gunners=%d enemy=%d lost=%.1f hit=%d"),SID,Gunners,Enemy,Lost,ATHit);
   }
   // Tank (squad type 4, stage 1): one wide, armoured vehicle for 40 resources, at most 2 per team, its own navigation data,
   // seen from farther away, 8 m/s in the open; the vehicle survives save and restore.
   {
    for(int32 Leader:{111,121,131})Store->ApplyDamage(Leader,1,10000);
    M->UpdateArea(1);M->Area.Replacements.Reset();
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::Tank);
    if(M->Credits[0]<Cost*3){const int32 Add=Cost*3-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0];const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Tank);
    int32 TSID=0;for(const auto& P:Store->GetSquads())TSID=FMath::Max(TSID,P.Key);const auto* TS=Store->FindSquad(TSID);
    const int32 TankID=TS?TS->LeaderUnitID:INDEX_NONE;ABattleUnit* TankActor=Actor(TankID);
    const bool Body=Hired&&Cost==40&&M->Credits[0]==C0-Cost&&TS&&TS->SquadType==BattleSquadType::Tank&&TS->Members.Num()==1&&TankActor&&
     TankActor->GetCapsuleComponent()->GetScaledCapsuleRadius()>=150.f&&FMath::IsNearlyEqual(Store->Find(TankID)->HP,350.f)&&IsVehicle(TankID)&&TS->DisplayName.Contains(TEXT("戦車"));
    Check(Body,TEXT("tank: one wide vehicle, 350 health, 40 resources"));
    Check(ArmorScale(TankID,EBattleWeapon::Carbine)==0.f&&ArmorScale(TankID,EBattleWeapon::SupportGun)==0.f&&ArmorScale(TankID,EBattleWeapon::AntiTank)==1.f&&ArmorScale(1,EBattleWeapon::Carbine)==1.f,TEXT("tank armour: only anti-tank weapons hurt it"));
    auto* NavSys=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());const ANavigationData* VN=NavFor(TankID);
    Check(VN&&NavSys&&VN!=NavSys->GetDefaultNavDataInstance()&&NavFor(1)==nullptr,TEXT("tank uses the vehicle navigation data"));
    Check(VehicleSpotRange()>=Store->Find(1)->Perception.SightRange*1.9f&&FMath::IsNearlyEqual(BattleSquadType::SpeedAt(BattleSquadType::Tank,GetWorld(),FVector::ZeroVector),800.f/GetDefault<UBattleTuning>()->MoveSpeed,.01f),TEXT("tank: seen from twice the normal sight, 8 m/s in the open"));
    const bool Second=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Tank);const int32 C2=M->Credits[0];
    const bool Third=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Tank);
    Check(Second&&!Third&&M->Credits[0]==C2&&M->ActiveOfType(0,BattleSquadType::Tank)==2,TEXT("tanks: at most 2 per team"));
    // Performance: a far tank's line of sight is traced once per TankSightReuse seconds per observer (range and view each scan);
    // a tank's accepted (or refused) goal is not reissued by the director every update.
    {
     int32 Obs=INDEX_NONE;for(const auto& B:GetBodies())if(const auto* U=Store->Find(B.Key);U&&U->IsAlive()&&U->Team==1&&Actor(B.Key)){Obs=B.Key;break;}
     bool Reuse=false;
     if(Obs!=INDEX_NONE)
     {
      FBattleSightContext C=Sight(Obs);C.Position=Position(TankID)+FVector(3000,0,0);C.Eye=C.Position+FVector(0,0,60);C.bOmnidirectional=true;C.RangeSquared=FMath::Square(1000.f);
      const float Was=GetDefault<UBattleTuning>()->TankSightReuse;GetMutableDefault<UBattleTuning>()->TankSightReuse=1.f;VehicleSightCache.Reset();
      const int64 T0=TankSightTraces,R0=TankSightReused;
      const bool A=CanSeeVehicleFar(Obs,C,TankID,1000.),B=CanSeeVehicleFar(Obs,C,TankID,1000.5);const int64 T1=TankSightTraces;
      CanSeeVehicleFar(Obs,C,TankID,1001.1);
      Reuse=A==B&&T1==T0+1&&TankSightReused==R0+1&&TankSightTraces==T0+2;
      GetMutableDefault<UBattleTuning>()->TankSightReuse=Was;VehicleSightCache.Reset();
     }
     Check(Reuse,TEXT("far tank sight: one trace per second per observer, reused in between"));
     const FVector Goal=Position(TankID)+FVector(400,0,0);IssueOrder(TSID,EBattleOrder::Move,Goal,true);
     Check(SameVehicleMission(TSID,EBattleOrder::Move,Goal)&&!SameVehicleMission(TSID,EBattleOrder::Move,Goal+FVector(3000,0,0))&&!SameVehicleMission(1,EBattleOrder::Move,Goal),TEXT("tank order: the same goal is not reissued every update"));
    }
    // Main gun (stage 2): 300 m on maps A-D, one round per TankGunReload seconds, a direct hit pierces armour (three kill a tank),
    // never own units.
    {
     const auto* TT=GetDefault<UBattleTuning>();const double Now=GetWorld()->GetTimeSeconds();
     const auto SavedGuns=TankGuns;
     const int64 S0=TankGunShots;
     const bool Fired=FireTankGun(TankID,Position(TankID)+FVector(5000,0,60),Now);
     const bool Loaded=TankGuns.Contains(TankID)&&FMath::IsNearlyEqual(TankGuns[TankID].ReadyAt,Now+TT->TankGunReload,.01)&&!StepTankGun(TankID,Now+1.);
     Check(Fired&&TankGunShots==S0+1&&Loaded&&FMath::IsNearlyEqual(TankGunEngage(),TT->TankGunRange)&&TankGunEngage()>=29999.f,TEXT("tank gun: 300 m, one round per reload"));
     if(M->Credits[1]<Cost){const int32 Add=Cost-M->Credits[1];M->Area.Income[1]+=Add;M->Credits[1]+=Add;}
     // Free an enemy squad slot first (the fixture may fill all 25).
     for(const auto& P:Store->GetSquads())if(P.Value.Team==1&&P.Value.CommandStatus==EBattleCommandStatus::Active){Store->ApplyDamage(P.Value.LeaderUnitID,0,10000);break;}
     M->UpdateArea(1);M->Area.Replacements.Reset();
     const bool EnemyHired=M->HireAreaSquad(1,EBattleTraining::Regular,false,BattleSquadType::Tank);
     if(!EnemyHired)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_TANK_GUN_SMOKE enemy hire refused: %s"),*M->LastMessage);
     int32 ESID=0;for(const auto& P:Store->GetSquads())ESID=FMath::Max(ESID,P.Key);const auto* ES=Store->FindSquad(ESID);
     const int32 EnemyTank=EnemyHired&&ES&&ES->SquadType==BattleSquadType::Tank?ES->LeaderUnitID:INDEX_NONE;
     const int32 OwnTank=Store->FindSquad(TSID+1)?Store->FindSquad(TSID+1)->LeaderUnitID:INDEX_NONE;
     const bool NoFriendly=OwnTank!=INDEX_NONE&&!TankGunHit(TankID,OwnTank)&&FMath::IsNearlyEqual(Store->Find(OwnTank)->HP,350.f);
     const bool H1=TankGunHit(TankID,EnemyTank),H2=TankGunHit(TankID,EnemyTank);const float After2=EnemyTank!=INDEX_NONE?Store->Find(EnemyTank)->HP:-1.f;
     const bool H3=TankGunHit(TankID,EnemyTank);
     Check(NoFriendly&&H1&&H2&&H3&&FMath::IsNearlyEqual(After2,350.f-2*TT->TankGunDamage,1.f)&&!Store->Find(EnemyTank)->IsAlive(),TEXT("tank gun pierces armour: three hits destroy a tank, never own tanks"));
     TankGuns=SavedGuns;
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_TANK_GUN_SMOKE tank=%d enemy=%d own=%d after2=%.0f"),TankID,EnemyTank,OwnTank,After2);
    }
    // Armour AI, stage 3 (BattleArmourAI.cpp): the tank's objective is kept on the next pick and its overwatch post lies short of the
    // point on the own side; a stopped hull turns no faster than its turn rate; no reaction without a threat or a hit; an enemy vehicle
    // the team knows of sends the anti-tank teams to the nearest point and changes the AI's own mix (anti-tank team, second tank).
    {
     const auto* TT=GetDefault<UBattleTuning>();
     M->CarChoices.Remove(TSID);
     const int32 TankPick=M->AreaTankTarget(0,TSID),Again=M->AreaTankTarget(0,TSID);
     const FVector WatchPost=M->Objectives.IsValidIndex(TankPick)?M->AreaTankPost(0,TankPick):FVector::ZeroVector;
     const bool bPost=M->Objectives.IsValidIndex(TankPick)&&FVector::Dist2D(WatchPost,M->AreaHome(0))<FVector::Dist2D(M->Objectives[TankPick].Position,M->AreaHome(0))&&
      FVector::Dist2D(WatchPost,M->Objectives[TankPick].Position)>=FMath::Min(double(TT->TankOverwatch),FVector::Dist2D(M->Objectives[TankPick].Position,M->AreaHome(0))-3000.)-1.;
     Check(BattleTactics::ArmourAI(0)&&BattleTactics::ArmourAI(1)&&TankPick==Again&&bPost,TEXT("armour AI: tank objective kept, overwatch post short of the point on the own side"));
     ABattleUnit* Hull=Actor(TankID);const float Yaw0=Hull?Hull->GetActorRotation().Yaw:0.f;
     FaceVehicle(TankID,FRotator(0,Yaw0+90.f,0).Vector());
     const float Turned=Hull?FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw0,Hull->GetActorRotation().Yaw)):-1.f;
     Check(Hull&&Turned<=TT->TankTurnRate*.1f+.01f&&!VehicleWatchDirection(TankID).IsNearlyZero(),TEXT("armour AI: a stopped hull turns at its turn rate and has a watch direction"));
     if(Hull)Hull->SetActorRotation(FRotator(0,Yaw0,0));
     const auto SavedKnown=M->KnownVehicles[0];M->KnownVehicles[0].Reset();
     FVector BackGoal;FString Why;
     const int32 Calm=M->AreaArmourReaction(0,TSID,0.f,BackGoal,Why),Shock=M->AreaArmourReaction(0,TSID,.5f,BackGoal,Why);
     FAreaIntent Mix0;M->AreaComposition(0,Mix0,0);
     const int32 NoneKnown=M->KnownEnemyVehicles(0),NoATTarget=M->AreaATTarget(0,TSID);
     const int32 SeenPoint=M->Objectives.Num()-1;
     UBattleMatch::FKnownVehicle Seen;Seen.At=M->Elapsed;Seen.Position=M->Objectives[SeenPoint].Position;M->KnownVehicles[0].Add(900001,Seen);
     M->CarChoices.Remove(TSID);
     const int32 KnownNow=M->KnownEnemyVehicles(0),ATTarget=M->AreaATTarget(0,TSID);
     FAreaIntent Mix6;M->AreaComposition(0,Mix6,6);
     M->KnownVehicles[0]=SavedKnown;M->CarChoices.Remove(TSID);
     Check(Calm==0&&(Shock==0||(Shock==1&&!BackGoal.ContainsNaN()))&&NoneKnown==0&&NoATTarget==INDEX_NONE&&KnownNow==1&&ATTarget==SeenPoint,TEXT("armour AI: no reaction without a threat; a known enemy vehicle draws the anti-tank teams to the nearest point"));
     Check(Mix0.ATKeep==0&&Mix0.TankKeep==0&&Mix0.SPGKeep==0&&Mix0.ACKeep==1&&Mix0.MortarKeep==1&&Mix6.ATKeep==1&&Mix6.TankKeep==FMath::Min(2,TT->AreaTankLimit)&&Mix6.SPGKeep==0&&!BattleTactics::AreaComposition(1),
      TEXT("composition: car and mortar first, a tank after six purchases, anti-tank teams and a second tank against known armour; off by default"));
     // Squad groups, stage 1 (BattleGroups.cpp): a player group of two squads; not run by the AI until both are delegated; then it gathers
     // at a rally point short of its objective, advances after the assembly time, and a regrouped squad leaves its old group.
     {
      TArray<int32> Two;for(const auto& P:Store->GetSquads())if(P.Value.Team==0&&P.Value.SquadType==BattleSquadType::Rifle&&P.Value.CommandStatus==EBattleCommandStatus::Active&&Two.Num()<2)Two.Add(P.Key);
      const auto SavedGroups=M->Groups;const auto SavedSquadGroup=M->SquadGroup;const int32 SavedNext=M->NextGroupID;
      const int32 GID=Two.Num()==2?M->FormGroup(0,Two,true):INDEX_NONE;const auto* GG=M->Groups.Find(GID);
      bool bFormed=GG&&M->GroupOf(Two[0])==GID&&M->GroupOf(Two[1])==GID&&GG->Phase==0&&!BattleTactics::AreaGroups(0);
      bool bManual=false,bGather=false,bAdvance=false,bLeft=false,bRoles=true,bStall=true;
      if(bFormed)
      {
       const bool D0=Store->FindSquad(Two[0])->bPlayerDelegated,D1=Store->FindSquad(Two[1])->bPlayerDelegated;
       FVector GroupCenter=FVector::ZeroVector;EBattleOrder GroupOrderValue=EBattleOrder::Hold;
       Store->SetSquadDelegated(Two[0],false);Store->SetSquadDelegated(Two[1],false);
       bManual=!M->GroupAIRun(*GG)&&!M->GroupOrder(Two[0],0,GroupCenter,GroupOrderValue);
       Store->SetSquadDelegated(Two[0],true);Store->SetSquadDelegated(Two[1],true);
       const bool bValid=M->Objectives.IsValidIndex(GG->Objective);
       bGather=bValid&&M->GroupAIRun(*GG)&&M->GroupOrder(Two[0],0,GroupCenter,GroupOrderValue)&&GroupOrderValue==EBattleOrder::AttackMove&&GroupCenter.Equals(GG->Rally,1.)&&
        FVector::Dist2D(GG->Rally,M->AreaHome(0))<FVector::Dist2D(M->Objectives[GG->Objective].Position,M->AreaHome(0));
       if(bValid&&M->ObservedOwner(0,GG->Objective)!=0)
       {
        M->Groups[GID].PhaseAt=M->Elapsed-1000.f;M->UpdateGroups(0);GG=M->Groups.Find(GID);
        bAdvance=GG&&GG->Phase==1&&M->GroupOrder(Two[0],0,GroupCenter,GroupOrderValue)&&GroupCenter.Equals(M->Objectives[GG->Objective].Position,1.)&&GroupOrderValue==EBattleOrder::AttackMove;
       }
       else bAdvance=true; // every point already observed as own in this fixture: nothing to advance on
       // Stage 2: a group of three rifle squads has exactly one support squad, posted short of the point; a stalled advance ends.
       if(GG&&GG->Phase==1)
       {
        int32 ThirdSquad=INDEX_NONE;for(const auto& P:Store->GetSquads())if(P.Value.Team==0&&P.Value.SquadType==BattleSquadType::Rifle&&P.Value.CommandStatus==EBattleCommandStatus::Active&&!Two.Contains(P.Key)){ThirdSquad=P.Key;break;}
        if(ThirdSquad!=INDEX_NONE)
        {
         auto& Three=M->Groups[GID];Three.Squads.Add(ThirdSquad);M->SquadGroup.Add(ThirdSquad,GID);M->AssignGroupRoles(Three);
         int32 SupportCount=0,AssaultCount=0,SupportID=INDEX_NONE;for(int32 SID:Three.Squads){const uint8 RoleValue=M->GroupRole(Three,SID);AssaultCount+=RoleValue==0;if(RoleValue==1){++SupportCount;SupportID=SID;}}
         const bool D2=Store->FindSquad(ThirdSquad)->bPlayerDelegated;Store->SetSquadDelegated(ThirdSquad,true);
         const FVector TakePoint=M->Objectives[Three.Objective].Position;
         bRoles=SupportCount==1&&AssaultCount==2&&M->GroupOrder(SupportID,0,GroupCenter,GroupOrderValue)&&GroupOrderValue==EBattleOrder::AttackMove&&
          FVector::Dist2D(GroupCenter,TakePoint)>500.&&FVector::Dist2D(GroupCenter,M->AreaHome(0))<FVector::Dist2D(TakePoint,M->AreaHome(0));
         Store->SetSquadDelegated(ThirdSquad,D2);Three.Squads.Remove(ThirdSquad);M->SquadGroup.Remove(ThirdSquad);M->AssignGroupRoles(Three);
        }
        M->Groups[GID].PhaseAt=M->Elapsed-1000.f;M->UpdateGroups(0);GG=M->Groups.Find(GID);
        bStall=GG&&(GG->Phase==0||GG->Phase==3);
       }
       const int32 Solo=M->FormGroup(0,{Two[1]},true);
       bLeft=Solo!=INDEX_NONE&&M->GroupOf(Two[1])==Solo&&M->GroupOf(Two[0])==GID&&M->Groups.Contains(GID)&&M->Groups[GID].Squads.Num()==1;
       Store->SetSquadDelegated(Two[0],D0);Store->SetSquadDelegated(Two[1],D1);
      }
      M->Groups=SavedGroups;M->SquadGroup=SavedSquadGroup;M->NextGroupID=SavedNext;
      Check(bFormed&&bManual&&bGather&&bAdvance&&bLeft,TEXT("squad groups: a player group follows the AI only when delegated; gather short of the objective, then advance; one group per squad"));
      Check(bFormed&&bRoles&&bStall&&!BattleTactics::DefendEngage(0),TEXT("squad groups, stage 2: one support squad short of the point, a stalled advance gathers again; defend-engage trial off by default"));
      UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_GROUP_SMOKE formed=%d manual=%d gather=%d advance=%d left=%d"),bFormed?1:0,bManual?1:0,bGather?1:0,bAdvance?1:0,bLeft?1:0);
     }
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_ARMOUR_AI_SMOKE tank_target=%d post=%s turned=%.2f shock=%d why=%s at_target=%d mix6=at%d/tank%d/ac%d/mortar%d/spg%d"),TankPick,*WatchPost.ToCompactString(),Turned,Shock,*Why,ATTarget,Mix6.ATKeep,Mix6.TankKeep,Mix6.ACKeep,Mix6.MortarKeep,Mix6.SPGKeep);
    }
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);ABattleUnit* After=Actor(TankID);
    Check(Restored&&After&&IsVehicle(TankID)&&After->GetCapsuleComponent()->GetScaledCapsuleRadius()>=150.f&&Turrets.Contains(TankID),TEXT("tank survives save and restore"));
    Mode->bCombatPaused=false;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_TANK_SMOKE squad=%d tank=%d nav=%s validation=%s"),TSID,TankID,VN?*VN->GetName():TEXT("none"),*Error);
   }
   // Armoured car (squad type 5, stage 1 2026-11-18): one vehicle for 20 resources, at most 3 per team, health 150, small arms do a tenth,
   // two anti-tank hits destroy it, 10 m/s in the open, a 120 degree view, the vehicle navigation data, no main gun; survives save and restore.
   {
    // Free squad slots: stop team 0 rifle squads (leader killed) until three cars fit under the 25 active squads.
    for(int32 Pass=0;Pass<6&&M->ActiveSquads(0)>22;++Pass)
    {
     for(const auto& P:Store->GetSquads())if(P.Value.Team==0&&P.Value.SquadType==BattleSquadType::Rifle&&P.Value.CommandStatus==EBattleCommandStatus::Active)
      if(const auto* L=Store->Find(P.Value.LeaderUnitID);L&&L->IsAlive()){Store->ApplyDamage(L->UnitID,1,10000);break;}
     M->UpdateArea(1);M->Area.Replacements.Reset();
    }
    const auto* TT=GetDefault<UBattleTuning>();
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::ArmoredCar);
    if(M->Credits[0]<Cost*4){const int32 Add=Cost*4-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0];const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::ArmoredCar);
    int32 CSID=0;for(const auto& P:Store->GetSquads())CSID=FMath::Max(CSID,P.Key);const auto* CS=Store->FindSquad(CSID);
    const int32 CarID=CS?CS->LeaderUnitID:INDEX_NONE;ABattleUnit* CarActor=Actor(CarID);
    const bool Body=Hired&&Cost==20&&M->Credits[0]==C0-Cost&&CS&&CS->SquadType==BattleSquadType::ArmoredCar&&CS->Members.Num()==1&&CarActor&&
     FMath::IsNearlyEqual(CarActor->GetCapsuleComponent()->GetScaledCapsuleRadius(),TT->ACRadius,1.f)&&FMath::IsNearlyEqual(Store->Find(CarID)->HP,150.f)&&
     IsVehicle(CarID)&&!HasMainGun(CarID)&&CS->DisplayName.Contains(TEXT("装甲車"))&&FMath::IsNearlyEqual(Store->Find(CarID)->Perception.FieldOfView,120.f)&&
     Store->Find(CarID)->Weapon==EBattleWeapon::SupportGun;
    Check(Body,TEXT("armoured car: one vehicle, 150 health, 20 resources, machine gun, 120 degree view, no main gun"));
    Check(FMath::IsNearlyEqual(ArmorScale(CarID,EBattleWeapon::Carbine),.1f)&&FMath::IsNearlyEqual(ArmorScale(CarID,EBattleWeapon::SupportGun),.1f)&&ArmorScale(CarID,EBattleWeapon::AntiTank)==1.f,
     TEXT("armoured car armour: small arms do a tenth, anti-tank weapons full damage"));
    auto* NavSys=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());const ANavigationData* CN=NavFor(CarID);
    const float Open=BattleSquadType::SpeedAt(BattleSquadType::ArmoredCar,GetWorld(),FVector::ZeroVector);
    Check(CN&&NavSys&&CN!=NavSys->GetDefaultNavDataInstance()&&FMath::IsNearlyEqual(Open,1000.f/TT->MoveSpeed,.01f),TEXT("armoured car: vehicle navigation data, 10 m/s in the open"));
    FVector GunDirection;Check(!TankGunTarget(CarID,GetWorld()->GetTimeSeconds(),GunDirection)&&!TankGuns.Contains(CarID),TEXT("armoured car has no main gun"));
    const bool Second=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::ArmoredCar);
    int32 Third=INDEX_NONE;if(M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::ArmoredCar)){const auto* TS=Store->FindSquad(M->LastHiredSquadID);Third=TS?TS->LeaderUnitID:INDEX_NONE;}
    const int32 C3=M->Credits[0];const bool Fourth=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::ArmoredCar);
    Check(Second&&Third!=INDEX_NONE&&!Fourth&&M->Credits[0]==C3&&M->ActiveOfType(0,BattleSquadType::ArmoredCar)==3,TEXT("armoured cars: at most 3 per team"));
    // Two anti-tank hits (100 each) destroy a car; the first leaves it alive.
    bool TwoHits=false;
    if(Third!=INDEX_NONE)
    {
     Store->ApplyDamage(Third,1,TT->ATDamage);const bool AliveAfterOne=Store->Find(Third)->IsAlive();
     Store->ApplyDamage(Third,1,TT->ATDamage);TwoHits=AliveAfterOne&&!Store->Find(Third)->IsAlive();
    }
    Check(TwoHits,TEXT("armoured car: two anti-tank hits destroy it"));
    M->UpdateArea(1);M->Area.Replacements.Reset();
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);ABattleUnit* After=Actor(CarID);
    Check(Restored&&After&&IsVehicle(CarID)&&FMath::IsNearlyEqual(After->GetCapsuleComponent()->GetScaledCapsuleRadius(),TT->ACRadius,1.f)&&Turrets.Contains(CarID)&&
     FMath::IsNearlyEqual(Store->Find(CarID)->Perception.FieldOfView,120.f),TEXT("armoured car survives save and restore"));
    Mode->bCombatPaused=false;
    // Stage 2: the car's objective is a valid point outside the city of map E and is kept on the next pick (ACChoiceHold).
    {
     M->CarChoices.Remove(CSID);
     const int32 First=M->AreaCarTarget(0,CSID),Again=M->AreaCarTarget(0,CSID);
     const bool bCity=First!=INDEX_NONE&&BattleFieldLayout::IsUrban(GetWorld())&&FMath::Abs(M->Objectives[First].Position.X)<25000&&FMath::Abs(M->Objectives[First].Position.Y)<15000;
     Check(M->Objectives.IsValidIndex(First)&&First==Again&&!bCity,TEXT("armoured car objective: a point outside the city, kept on the next pick"));    }
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_AC_SMOKE squad=%d car=%d third=%d nav=%s validation=%s"),CSID,CarID,Third,CN?*CN->GetName():TEXT("none"),*Error);
   }
   // Indirect fire (stage 1 2026-11-18). Two known soldiers 2 m apart: a target worth a shell (ArtilleryMinCluster 2); keys below zero
   // stand for report positions. A direction from a unit whose blast-plus-spread circle at Distance holds no own unit (zero when none).
   auto KnownPair=[](const FVector& At){TMap<int32,FVector> K;K.Add(-901,At);K.Add(-902,At+FVector(200,0,0));return K;};
   auto ClearDirection=[&](int32 ID,bool bSPG,double Distance)
   {
    for(int32 A=0;A<16;++A){const FVector D(FMath::Cos(A*PI/8),FMath::Sin(A*PI/8),0);if(!AllyNear(0,Position(ID)+D*Distance,ArtilleryBlastRadius(bSPG)+ArtillerySpread(bSPG,Distance)+500.))return D;}
    return FVector(FVector::ZeroVector);
   };
   // Free team 0 squad slots: stop rifle squads (leader killed) until Keep slots are free under the 25 active squads.
   auto FreeSlots=[&](int32 Keep)
   {
    for(int32 Pass=0;Pass<10&&M->ActiveSquads(0)>25-Keep;++Pass)
    {
     for(const auto& P:Store->GetSquads())if(P.Value.Team==0&&P.Value.SquadType==BattleSquadType::Rifle&&P.Value.CommandStatus==EBattleCommandStatus::Active)
      if(const auto* L=Store->Find(P.Value.LeaderUnitID);L&&L->IsAlive()){Store->ApplyDamage(L->UnitID,1,10000);break;}
     M->UpdateArea(1);M->Area.Replacements.Reset();
    }
   };
   // Mortar team (squad type 6): 3 soldiers (slot 1 the mortar: 1 loaded, 11 in reserve), 14 resources, at most 3 per team; targets only
   // between 50 m and 300 m and never where the blast and spread may reach an own unit; no shot before 4 s of set-up or while moving;
   // the shell lands after its flight time; the explosion hurts enemy soldiers and never own units; survives save and restore.
   {
    FreeSlots(4);
    const auto* TT=GetDefault<UBattleTuning>();
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::Mortar);
    if(M->Credits[0]<Cost*5){const int32 Add=Cost*5-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0];const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Mortar);
    const auto* MS=Hired?Store->FindSquad(M->LastHiredSquadID):nullptr;const int32 MSID=MS?MS->SquadID:INDEX_NONE;
    const int32 Gunner=MS&&MS->Members.Num()==3?MS->Members[1]:INDEX_NONE;const int32 MLeader=MS?MS->LeaderUnitID:INDEX_NONE;
    bool Body=Hired&&Cost==14&&M->Credits[0]==C0-Cost&&MS&&MS->SquadType==BattleSquadType::Mortar&&MS->DisplayName.Contains(TEXT("迫撃砲班"))&&Gunner!=INDEX_NONE;
    if(Body)for(int32 I=0;I<3;++I){const auto* U=Store->Find(MS->Members[I]);Body&=U&&U->Weapon==(I==1?EBattleWeapon::Mortar:EBattleWeapon::Carbine);}
    if(Body){const auto* G=Store->Find(Gunner);Body&=G->Ammo==1&&G->Reserve==11&&IsArtillery(Gunner)&&!IsArtillery(MLeader)&&!IsVehicle(Gunner);}
    Check(Body,TEXT("mortar team: 3 soldiers, one mortar (1 loaded, 11 in reserve), 14 resources"));
    Check(Gunner!=INDEX_NONE&&!Fire(Gunner,Position(Gunner)+FVector(0,0,60),FVector::ForwardVector,false)&&Store->Find(Gunner)->Ammo==1,TEXT("a mortar never fires along the line of sight"));
    const bool Second=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Mortar),Third=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Mortar);
    const int32 C3=M->Credits[0];const bool Fourth=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Mortar);
    Check(Second&&Third&&!Fourth&&M->Credits[0]==C3&&M->ActiveOfType(0,BattleSquadType::Mortar)==3,TEXT("mortar teams: at most 3 per team"));
    // Range: 50 m to 300 m (400 m with the range profile); spread 8 m at 300 m.
    const float MinR=ArtilleryMinRange(false),MaxR=ArtilleryMaxRange(false);const double Mid=(MinR+MaxR)*.5;
    Check(FMath::IsNearlyEqual(MinR,TT->MortarMinRange)&&FMath::IsNearlyEqual(MaxR,BattleRanges::Get().bActive?TT->MortarRangeProfile:TT->MortarRange)&&FMath::IsNearlyEqual(ArtillerySpread(false,TT->MortarRange),TT->MortarSpread,1.f),
     TEXT("mortar: 50 m to 300 m (400 m with the range profile), 8 m spread at 300 m"));
    const FVector GPos=Gunner!=INDEX_NONE?Position(Gunner):FVector::ZeroVector;const FVector Dir=Gunner!=INDEX_NONE?ClearDirection(Gunner,false,Mid):FVector::ZeroVector;
    FVector Aim=FVector::ZeroVector;bool bBase=false,bHeld=false;
    const bool Inside=!Dir.IsZero()&&PickArtilleryAim(Gunner,KnownPair(GPos+Dir*Mid),Aim,bBase,bHeld)&&!bBase&&FVector::Dist2D(Aim,GPos+Dir*Mid+FVector(100,0,0))<10.;
    const bool Near=!Dir.IsZero()&&(!PickArtilleryAim(Gunner,KnownPair(GPos+Dir*(MinR*.8)),Aim,bBase,bHeld)||bBase);
    const bool Far=!Dir.IsZero()&&(!PickArtilleryAim(Gunner,KnownPair(GPos+Dir*(MaxR*1.1)),Aim,bBase,bHeld)||bBase);
    TMap<int32,FVector> One;One.Add(-903,GPos+Dir*Mid);
    const bool Single=!Dir.IsZero()&&(!PickArtilleryAim(Gunner,One,Aim,bBase,bHeld)||bBase);
    Check(Inside&&Near&&Far&&Single,TEXT("mortar target: two or more known soldiers, between the minimum and the maximum range only"));
    // An own soldier at the target: no shot there.
    bool Held=false;
    if(!Dir.IsZero()&&MLeader!=INDEX_NONE)
    {
     const FVector LeaderAt=Position(MLeader);Place(MLeader,GPos+Dir*Mid);
     Held=!PickArtilleryAim(Gunner,KnownPair(GPos+Dir*Mid),Aim,bBase,bHeld)&&bHeld;Place(MLeader,LeaderAt);
    }
    Check(Held,TEXT("mortar holds fire where the blast and spread may reach an own unit"));
    // Set-up: nothing while moving or during the first 4 s still; then one round, and moving undoes the set-up.
    const double Now=GetWorld()->GetTimeSeconds();bool SetUp=false;
    if(!Dir.IsZero())
    {
     auto& A=Artillery.FindOrAdd(Gunner);A=FArtillery();A.bHasTarget=true;A.Aim=GPos+Dir*Mid;A.NextPick=Now+1000.;
     const int64 S0=ArtilleryShotsMortar;const int32 F0=ArtilleryShells.Num();FVector ADir;
     const bool Moving=!StepArtillery(Gunner,Now,true,ADir)&&ArtilleryShotsMortar==S0;
     StepArtillery(Gunner,Now+1.,false,ADir);const bool Early=ArtilleryShotsMortar==S0;
     StepArtillery(Gunner,Now+1.+TT->MortarSetupSeconds-.1,false,ADir);const bool Late=ArtilleryShotsMortar==S0;
     StepArtillery(Gunner,Now+1.+TT->MortarSetupSeconds+.05,false,ADir);
     const bool Fired=ArtilleryShotsMortar==S0+1&&ArtilleryShells.Num()==F0+1&&Store->Find(Gunner)->Ammo==0;
     StepArtillery(Gunner,Now+2.+TT->MortarSetupSeconds,true,ADir);const bool Undone=Artillery.FindChecked(Gunner).StillSince<0;
     SetUp=Moving&&Early&&Late&&Fired&&Undone;
    }
    Check(SetUp,TEXT("mortar fires only after 4 s of set-up while still; moving undoes the set-up"));
    // Explosion at an enemy soldier with an own soldier 3 m away: the enemy loses MortarBlastDamage, the own soldier nothing.
    int32 Enemy=INDEX_NONE;for(const auto& B:GetBodies())if(const auto* U=Store->Find(B.Key);U&&U->IsAlive()&&U->Team==1&&!IsVehicle(B.Key)){Enemy=B.Key;break;}
    float Lost=-1.f;bool Blast=false;
    if(Enemy!=INDEX_NONE&&MLeader!=INDEX_NONE&&Gunner!=INDEX_NONE)
    {
     const FVector LeaderAt=Position(MLeader);Place(MLeader,Position(Enemy)+FVector(300,0,0));
     const float EnemyBefore=Store->Find(Enemy)->HP,AllyBefore=Store->Find(MLeader)->HP;
     FArtilleryShell Shell;Shell.ShooterID=Gunner;Shell.Team=0;Shell.ShooterType=BattleSquadType::Mortar;Shell.At=Position(Enemy);
     const int32 Damaged=ArtilleryImpact(Shell);Lost=EnemyBefore-Store->Find(Enemy)->HP;
     Blast=Damaged>=1&&FMath::IsNearlyEqual(Lost,FMath::Min(EnemyBefore,TT->MortarBlastDamage),1.f)&&Store->Find(MLeader)->HP==AllyBefore;
     Place(MLeader,LeaderAt);
    }
    Check(Blast,TEXT("mortar explosion hurts enemy soldiers around the impact, never own units"));
    // Flight: the shell lands after distance / MortarShellSpeed, not before.
    int32 FlightTarget=INDEX_NONE;for(const auto& B:GetBodies())if(const auto* U=Store->Find(B.Key);U&&U->IsAlive()&&U->Team==1&&!IsVehicle(B.Key)){FlightTarget=B.Key;break;}
    float Flight=-1.f;bool Landed=false;
    if(FlightTarget!=INDEX_NONE&&Gunner!=INDEX_NONE)
    {
     ArtilleryShells.Reset();
     {auto& GS=Store->Units.FindChecked(Gunner);GS.Ammo=1;GS.ReloadEndTime=0;GS.NextShotTime=0;}
     const float HP0=Store->Find(FlightTarget)->HP;const double Distance=FVector::Dist2D(Position(Gunner),Position(FlightTarget));
     const int64 I0=ArtilleryImpacts;
     const bool Launched=FireArtillery(Gunner,Position(FlightTarget),Now+20.);
     Flight=ArtilleryShells.Num()==1?ArtilleryShells[0].Remaining:-1.f;
     StepArtilleryShells(Flight*.5f);const bool Waiting=ArtilleryShells.Num()==1&&Store->Find(FlightTarget)->HP==HP0&&ArtilleryImpacts==I0;
     StepArtilleryShells(Flight*.6f);
     Landed=Launched&&FMath::IsNearlyEqual(Flight,float(Distance/TT->MortarShellSpeed),.01f)&&Waiting&&ArtilleryShells.IsEmpty()&&ArtilleryImpacts==I0+1;
    }
    Check(Landed,TEXT("mortar shell lands after its flight time (distance / 100 m/s)"));
    // Save and restore: the team and its mortar survive; set-up and shells in flight are not saved.
    ArtilleryShells.Add(FArtilleryShell());
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);const auto* GU=Gunner!=INDEX_NONE?Store->Find(Gunner):nullptr;const auto* RS=Store->FindSquad(MSID);
    Check(Restored&&GU&&GU->Weapon==EBattleWeapon::Mortar&&RS&&RS->SquadType==BattleSquadType::Mortar&&Artillery.IsEmpty()&&ArtilleryShells.IsEmpty(),TEXT("mortar team survives save and restore (set-up and shells in flight dropped)"));
    Mode->bCombatPaused=false;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_MORTAR_SMOKE squad=%d gunner=%d dir=%s lost=%.1f flight=%.2f validation=%s"),MSID,Gunner,*Dir.ToCompactString(),Lost,Flight,*Error);
   }
   // Self-propelled gun (squad type 7): one vehicle for 35 resources, at most 2 per team, health 200, small arms do a tenth, 7 m/s in the
   // open, a 120 degree view, a machine gun and no direct main gun; targets between 100 m and 1000 m; 8 s of set-up, one round per 10 s;
   // its shell hurts a vehicle only on a direct hit, a mortar shell never; survives save and restore.
   {
    FreeSlots(3);
    const auto* TT=GetDefault<UBattleTuning>();
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::SPGun);
    if(M->Credits[0]<Cost*3){const int32 Add=Cost*3-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0];const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::SPGun);
    const auto* GS=Hired?Store->FindSquad(M->LastHiredSquadID):nullptr;const int32 GSID=GS?GS->SquadID:INDEX_NONE;
    const int32 GunID=GS?GS->LeaderUnitID:INDEX_NONE;ABattleUnit* GunActor=Actor(GunID);
    const bool Body=Hired&&Cost==35&&M->Credits[0]==C0-Cost&&GS&&GS->SquadType==BattleSquadType::SPGun&&GS->Members.Num()==1&&GunActor&&
     FMath::IsNearlyEqual(GunActor->GetCapsuleComponent()->GetScaledCapsuleRadius(),TT->SPGRadius,1.f)&&FMath::IsNearlyEqual(Store->Find(GunID)->HP,200.f)&&
     IsVehicle(GunID)&&IsSPG(GunID)&&IsArtillery(GunID)&&!HasMainGun(GunID)&&GS->DisplayName.Contains(TEXT("自走砲"))&&
     FMath::IsNearlyEqual(Store->Find(GunID)->Perception.FieldOfView,120.f)&&Store->Find(GunID)->Weapon==EBattleWeapon::SupportGun&&Turrets.Contains(GunID);
    Check(Body,TEXT("self-propelled gun: one vehicle, 200 health, 35 resources, machine gun, 120 degree view, gun mount"));
    Check(FMath::IsNearlyEqual(ArmorScale(GunID,EBattleWeapon::Carbine),.1f)&&FMath::IsNearlyEqual(ArmorScale(GunID,EBattleWeapon::SupportGun),.1f)&&ArmorScale(GunID,EBattleWeapon::AntiTank)==1.f,
     TEXT("self-propelled gun armour: small arms do a tenth, anti-tank weapons full damage"));
    auto* NavSys=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());const ANavigationData* GN=NavFor(GunID);
    Check(GN&&NavSys&&GN!=NavSys->GetDefaultNavDataInstance()&&FMath::IsNearlyEqual(BattleSquadType::SpeedAt(BattleSquadType::SPGun,GetWorld(),FVector::ZeroVector),700.f/TT->MoveSpeed,.01f),
     TEXT("self-propelled gun: vehicle navigation data, 7 m/s in the open"));
    const bool Second=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::SPGun);const int32 C2=M->Credits[0];
    const bool Third=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::SPGun);
    Check(Second&&!Third&&M->Credits[0]==C2&&M->ActiveOfType(0,BattleSquadType::SPGun)==2,TEXT("self-propelled guns: at most 2 per team"));
    // Range 100 m to 1000 m on every map; spread 15 m at 1000 m.
    const float MinR=ArtilleryMinRange(true),MaxR=ArtilleryMaxRange(true);const double Mid=(MinR+MaxR)*.5;
    Check(FMath::IsNearlyEqual(MinR,10000.f)&&FMath::IsNearlyEqual(MaxR,100000.f)&&FMath::IsNearlyEqual(ArtillerySpread(true,100000.),1500.f,1.f),TEXT("self-propelled gun: 100 m to 1000 m, 15 m spread at 1000 m"));
    const FVector GPos=GunID!=INDEX_NONE?Position(GunID):FVector::ZeroVector;const FVector Dir=GunID!=INDEX_NONE?ClearDirection(GunID,true,Mid):FVector::ZeroVector;
    FVector Aim=FVector::ZeroVector;bool bBase=false,bHeld=false;
    const bool Inside=!Dir.IsZero()&&PickArtilleryAim(GunID,KnownPair(GPos+Dir*Mid),Aim,bBase,bHeld)&&!bBase;
    const bool Near=!Dir.IsZero()&&(!PickArtilleryAim(GunID,KnownPair(GPos+Dir*(MinR*.8)),Aim,bBase,bHeld)||bBase);
    const bool Far=!Dir.IsZero()&&(!PickArtilleryAim(GunID,KnownPair(GPos+Dir*(MaxR*1.1)),Aim,bBase,bHeld)||bBase);
    Check(Inside&&Near&&Far,TEXT("self-propelled gun target: between 100 m and 1000 m only"));
    // Set-up 8 s with the gun turned to the target, then one round per SPGReload seconds.
    const double Now=GetWorld()->GetTimeSeconds();bool SetUp=false;
    if(!Dir.IsZero())
    {
     if(const auto* Turret=Turrets.Find(GunID);Turret&&Turret->IsValid())(*Turret)->SetWorldRotation(FRotator(0,Dir.Rotation().Yaw,0));
     auto& A=Artillery.FindOrAdd(GunID);A=FArtillery();A.bHasTarget=true;A.Aim=GPos+Dir*Mid;A.NextPick=Now+1000.;
     const int64 S0=ArtilleryShotsSPG;FVector ADir;
     StepArtillery(GunID,Now,false,ADir);
     StepArtillery(GunID,Now+TT->SPGSetupSeconds-.1,false,ADir);const bool Early=ArtilleryShotsSPG==S0;
     StepArtillery(GunID,Now+TT->SPGSetupSeconds+.05,false,ADir);const bool Fired=ArtilleryShotsSPG==S0+1;
     StepArtillery(GunID,Now+TT->SPGSetupSeconds+.05+TT->SPGReload*.5,false,ADir);const bool Loading=ArtilleryShotsSPG==S0+1;
     StepArtillery(GunID,Now+TT->SPGSetupSeconds+.1+TT->SPGReload,false,ADir);const bool Again=ArtilleryShotsSPG==S0+2;
     SetUp=Early&&Fired&&Loading&&Again;
    }
    Check(SetUp,TEXT("self-propelled gun fires after 8 s of set-up, one round per 10 s"));
    ArtilleryShells.Reset();
    // Armour: a mortar shell does nothing to an enemy vehicle, the gun's direct hit SPGBlastDamage, a near miss nothing.
    if(M->Credits[1]<Cost){const int32 Add=Cost-M->Credits[1];M->Area.Income[1]+=Add;M->Credits[1]+=Add;}
    for(const auto& P:Store->GetSquads())if(P.Value.Team==1&&P.Value.CommandStatus==EBattleCommandStatus::Active&&P.Value.SquadType==BattleSquadType::Rifle){Store->ApplyDamage(P.Value.LeaderUnitID,0,10000);break;}
    M->UpdateArea(1);M->Area.Replacements.Reset();
    const bool EnemyHired=M->HireAreaSquad(1,EBattleTraining::Regular,false,BattleSquadType::SPGun);
    if(!EnemyHired)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_SPG_SMOKE enemy hire refused: %s"),*M->LastMessage);
    const auto* ES=EnemyHired?Store->FindSquad(M->LastHiredSquadID):nullptr;const int32 EnemyGun=ES?ES->LeaderUnitID:INDEX_NONE;
    bool Armour=false;float AfterHit=-1.f;
    if(EnemyGun!=INDEX_NONE&&GunID!=INDEX_NONE)
    {
     FArtilleryShell Shell;Shell.ShooterID=GunID;Shell.Team=0;Shell.At=Position(EnemyGun);
     Shell.bSPG=false;Shell.ShooterType=BattleSquadType::Mortar;ArtilleryImpact(Shell);const bool MortarNone=FMath::IsNearlyEqual(Store->Find(EnemyGun)->HP,200.f);
     Shell.bSPG=true;Shell.ShooterType=BattleSquadType::SPGun;Shell.At=Position(EnemyGun)+FVector(BattleSquadType::VehicleRadius(BattleSquadType::SPGun)+300.f,0,0);ArtilleryImpact(Shell);
     const bool MissNone=FMath::IsNearlyEqual(Store->Find(EnemyGun)->HP,200.f);
     Shell.At=Position(EnemyGun);ArtilleryImpact(Shell);AfterHit=Store->Find(EnemyGun)->HP;
     Armour=MortarNone&&MissNone&&FMath::IsNearlyEqual(AfterHit,200.f-TT->SPGBlastDamage,1.f);
    }
    Check(Armour,TEXT("shells and vehicles: mortar shells never hurt them, the gun only on a direct hit"));
    // Save and restore: the gun keeps its body and mount; set-up and load are not saved.
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);ABattleUnit* After=Actor(GunID);
    Check(Restored&&After&&IsSPG(GunID)&&IsArtillery(GunID)&&FMath::IsNearlyEqual(After->GetCapsuleComponent()->GetScaledCapsuleRadius(),TT->SPGRadius,1.f)&&Turrets.Contains(GunID)&&Artillery.IsEmpty(),
     TEXT("self-propelled gun survives save and restore"));
    Mode->bCombatPaused=false;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_SPG_SMOKE squad=%d gun=%d enemy=%d dir=%s after_hit=%.0f nav=%s validation=%s"),GSID,GunID,EnemyGun,*Dir.ToCompactString(),AfterHit,GN?*GN->GetName():TEXT("none"),*Error);
   }
  }
  // Armoured exoskeleton soldier (squad type 8, 2026-11-18): one soldier for 30 resources, a strong automatic rifle and a launcher,
  // armour against small arms, 10 m/s, a shield that takes damage first and refills after a pause, 5 soldiers in the score, save and restore.
  {
   const auto* TT=GetDefault<UBattleTuning>();const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::Exo);const double Now=GetWorld()->GetTimeSeconds();
   for(const auto& P:Store->GetSquads())if(P.Value.Team==0&&P.Value.CommandStatus==EBattleCommandStatus::Active&&P.Value.SquadType==BattleSquadType::Rifle){Store->ApplyDamage(P.Value.LeaderUnitID,1,10000);break;}
   M->UpdateArea(1);M->Area.Replacements.Reset();
   if(M->Credits[0]<Cost){const int32 Add=Cost-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
   const int32 C0=M->Credits[0],Score0=M->AreaScore(0);const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Exo);
   if(!Hired)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_EXO_SMOKE hire refused: %s"),*M->LastMessage);
   const int32 XSID=Hired?M->LastHiredSquadID:INDEX_NONE;const auto* XS=Store->FindSquad(XSID);
   bool Body=false,Armour=false,Shield=false,Regen=false,Calm=false,Launch=false,Scored=false,Kept=false,Reserve=false;float ShieldSaved=-1.f;
   if(XS&&XS->Members.Num()==1)
   {
    const int32 L=XS->LeaderUnitID;const auto* LU=Store->Find(L);const float HP0=LU->HP;
    Body=Cost==30&&M->Credits[0]==C0-Cost&&XS->SquadType==BattleSquadType::Exo&&XS->DisplayName.Contains(TEXT("外骨格"))&&!IsVehicle(L)&&Actor(L)&&ExoLooks.Contains(L)&&ExoLooks[L].Bubble.IsValid()&&
     LU->Weapon==EBattleWeapon::SupportGun&&FMath::IsNearlyEqual(HP0,TT->Health*2.f)&&FMath::IsNearlyEqual(LU->Shield,100.f)&&FMath::IsNearlyEqual(Store->MaxShieldOf(L),100.f)&&Store->MaxShieldOf(1)==0.f&&
     LU->Launcher==4&&LU->CarriesAntiTank()&&!Store->Find(1)->CarriesAntiTank()&&
     FMath::IsNearlyEqual(BattleSquadType::SpeedAt(BattleSquadType::Exo,GetWorld(),FVector::ZeroVector),1000.f/TT->MoveSpeed,.01f)&&FMath::IsNearlyEqual(BattleSquadType::DamageScale(BattleSquadType::Exo),2.f);
    Scored=BattleWarScore::Tuning().bEnabled||M->AreaScore(0)==Score0-Cost+5;
    Armour=FMath::IsNearlyEqual(ArmorScale(L,EBattleWeapon::Carbine),.3f)&&FMath::IsNearlyEqual(ArmorScale(L,EBattleWeapon::SupportGun),.3f)&&ArmorScale(L,EBattleWeapon::AntiTank)==1.f&&ArmorScale(1,EBattleWeapon::Carbine)==1.f;
    // No suppression while the shield holds.
    SuppressionZeroAt.Remove(L);AddSuppression(L,1.f,Now);const bool Unshaken=Suppression(L,Now)==0.f;
    // Shield: 70 damage leaves health untouched; the next 50 empties the shield and takes 20 health.
    Store->ApplyDamage(L,1,70);const bool First=FMath::IsNearlyEqual(LU->HP,HP0)&&FMath::IsNearlyEqual(LU->Shield,30.f);
    Store->ApplyDamage(L,1,50);Shield=First&&FMath::IsNearlyEqual(LU->HP,HP0-20.f)&&LU->Shield==0.f;
    AddSuppression(L,1.f,Now);Calm=Unshaken&&(!BattleSuppression::Tuning().bEnabled||Suppression(L,Now)>0.f);SuppressionZeroAt.Remove(L);
    // Refill: nothing for 5 s after the last hit, then 20 points/s up to 100; health does not refill.
    Store->AdvanceShields(2.5f);Store->AdvanceShields(2.5f);const bool Waited=LU->Shield==0.f;
    Store->AdvanceShields(2.f);const bool Partial=FMath::IsNearlyEqual(LU->Shield,40.f);
    Store->AdvanceShields(100.f);Regen=Waited&&Partial&&FMath::IsNearlyEqual(LU->Shield,100.f)&&FMath::IsNearlyEqual(LU->HP,HP0-20.f);
    // Launcher: one round out (straight up, hitting nothing), then 8 s to load the next; the main weapon's ammunition is untouched.
    const int32 Ammo0=LU->Ammo;const bool Fired=FireExoLauncher(L,Position(L)+FVector(0,0,5000),Now);const bool Loading=!FireExoLauncher(L,Position(L)+FVector(0,0,5000),Now);
    const bool One=LU->Launcher==3&&FMath::IsNearlyEqual(LU->LauncherWait,8.f)&&LU->Ammo==Ammo0;
    Store->AdvanceShields(8.f);Launch=Fired&&Loading&&One&&LU->LauncherWait==0.f;
    // Team layer: the fast reserve gets a valid objective from what its team knows.
    Reserve=M->Objectives.IsValidIndex(M->AreaExoTarget(0,XSID));
    // Save and restore: type, shield points, the pause before the refill and the launcher rounds survive.
    Store->ApplyDamage(L,1,25);ShieldSaved=LU->Shield;
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);const auto* After=Store->FindSquad(XSID);const auto* RU=Store->Find(L);
    Kept=Restored&&After&&After->SquadType==BattleSquadType::Exo&&After->CommandStatus==EBattleCommandStatus::Active&&RU&&Actor(L)&&ExoLooks.Contains(L)&&FMath::IsNearlyEqual(RU->Shield,75.f)&&FMath::IsNearlyEqual(RU->ShieldWait,TT->ExoShieldDelay)&&
     RU->Launcher==3&&FMath::IsNearlyEqual(RU->HP,HP0-20.f)&&Store->HasShields()&&FMath::IsNearlyEqual(ArmorScale(L,EBattleWeapon::Carbine),.3f);
    Mode->bCombatPaused=false;
   }
   Check(Body,TEXT("exoskeleton soldier: one soldier, 30 resources, automatic rifle at 2x damage, 4 launcher rounds, 200 health, 100 shield, 10 m/s"));
   Check(Scored,TEXT("exoskeleton soldier counts as 5 soldiers in the score"));
   Check(Armour,TEXT("exoskeleton armour: small arms do 0.3, anti-tank weapons full damage, other soldiers unchanged"));
   Check(Shield,TEXT("exoskeleton shield takes damage before health"));
   Check(Calm,TEXT("exoskeleton is not suppressed while its shield holds"));
   Check(Regen,TEXT("exoskeleton shield refills 5 s after the last hit at 20 points per second"));
   Check(Launch,TEXT("exoskeleton launcher: one round per 8 s, separate from the main weapon"));
   Check(Reserve,TEXT("exoskeleton soldier: the team layer gives it an objective as the fast reserve"));
   Check(Kept,TEXT("exoskeleton soldier survives save and restore with its shield, launcher rounds and look"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_EXO_SMOKE squad=%d shield_saved=%.0f validation=%s"),XSID,ShieldSaved,*Error);
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
  // War score trial (default OFF): points from true ownership every 5 s, losses score for the other team, the opponent estimate
  // uses observed ownership only, unspent resources are added at the end, and the record survives a snapshot.
  {
   auto& W=BattleWarScore::MutableTuning();const auto SavedW=W;const auto KnownBefore=M->Area.KnownOwners;
   Check(!SavedW.bEnabled,TEXT("war score trial is off by default"));
   M->Area.WarClock=0;for(int32 I=0;I<6;++I)M->UpdateArea(1);M->RecordWarLoss(0);
   Check(M->Area.WarPoint[0]==0&&M->Area.WarPoint[1]==0&&M->Area.WarLoss[1]==0&&M->Area.WarClock==0,TEXT("disabled war score records nothing"));
   W=BattleWarScore::FTuning();W.bEnabled=true;
   for(auto& P:M->Objectives)P.Owner=INDEX_NONE;M->Objectives[0].Owner=0;M->Objectives[1].Owner=0;M->Objectives[2].Owner=1;
   auto Seen=[&](){for(int32 I=0;I<6;++I)M->Area.KnownOwners[I]=-2;M->Area.KnownOwners[2]=1;}; // team 0 has seen only point 2, held by the enemy
   M->Area.WarPoint={0,0};M->Area.WarLoss={0,0};M->Area.WarPointEstimate={0,0};M->Area.WarClock=0;
   for(int32 I=0;I<4;++I){Seen();M->UpdateArea(1);}
   Check(M->Area.WarPoint[0]==0&&M->Area.WarPoint[1]==0,TEXT("no war points before the 5 second tick"));
   Seen();M->UpdateArea(1);
   Check(M->Area.WarPoint[0]==2&&M->Area.WarPoint[1]==1,TEXT("each owned point scores 1 per 5 seconds"));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_WAR estimate=%d/%d known=%d,%d,%d,%d,%d,%d"),M->Area.WarPointEstimate[0],M->Area.WarPointEstimate[1],M->Area.KnownOwners[0],M->Area.KnownOwners[1],M->Area.KnownOwners[2],M->Area.KnownOwners[3],M->Area.KnownOwners[4],M->Area.KnownOwners[5]);
   Check(M->Area.WarPointEstimate[0]==1&&M->Area.WarPointEstimate[1]==0,TEXT("opponent estimate counts only observed ownership"));
   M->RecordWarLoss(0);
   Check(M->Area.WarLoss[1]==1&&M->Area.WarLoss[0]==0&&M->WarScoreEstimate(0)==2,TEXT("a lost soldier scores for the other team and is known to the loser"));
   Check(M->WarScore(0)==2+M->Credits[0]&&M->WarScore(1)==2+M->Credits[1]&&M->AreaScore(0)==M->WarScore(0),TEXT("unspent resources are added to the war score"));
   Mode->bCombatPaused=true;Save=M->CaptureSnapshot();
   Check(Save&&Save->Validate(Error)&&Save->Area.WarPoint[0]==2&&Save->Area.WarLoss[1]==1,TEXT("war score joins the snapshot"));
   if(Save){Save->Area.WarPoint[0]=-1;Check(!Save->Validate(Error),TEXT("invalid war score rejected"));}
   Mode->bCombatPaused=false;
   M->Area.WarPoint={5,0};M->Area.WarLoss={0,0};M->Credits[0]=10;M->Credits[1]=16;M->Elapsed=600;M->CheckTimeLimit();
   Check(M->Winner==1&&M->Phase==EBattleMatchPhase::Finished,TEXT("resources kept decide the time limit under war score"));
   M->Phase=EBattleMatchPhase::Combat;Mode->bBattleEnded=false;M->Winner=INDEX_NONE;M->Elapsed=100;
   W.Target=5;M->UpdateArea(1);
   Check(M->Winner==0&&M->Phase==EBattleMatchPhase::Finished,TEXT("optional target ends the match on earned score"));
   M->Phase=EBattleMatchPhase::Combat;Mode->bBattleEnded=false;M->Winner=INDEX_NONE;
   M->Area.WarPoint={0,0};M->Area.WarLoss={0,0};M->Area.WarPointEstimate={0,0};M->Area.WarClock=0;M->Area.KnownOwners=KnownBefore;W=SavedW;
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
  // Notification log (command UI): the stopped squads and observed points of this fixture appear before the screenshot.
  PC->UpdateNoticeLog(true);Check(PC->VisibleNotices().Num()>0,TEXT("notification log records observed events"));
  // Squad list view: the stopped filter shows exactly the stopped squads; the loss sort keeps every row.
  {
   PC->SquadFilter=4;const auto Stopped=PC->SquadListRows();bool AllStopped=Stopped.Num()>0;for(const auto& R:Stopped)AllStopped&=!R.bActive;
   PC->SquadFilter=0;PC->SquadSort=1;const int32 Sorted=PC->SquadListRows().Num();PC->SquadSort=0;
   Check(AllStopped&&Sorted==PC->SquadListRows().Num(),TEXT("squad list filter and sort"));
  }
  // Command bar: the attack-move button arms the same pending order as the T key; the move button clears it.
  { PC->PressCommandButton(1);const bool Armed=PC->PendingOrder==EBattleOrder::AttackMove;PC->PressCommandButton(0);
    Check(Armed&&PC->PendingOrder==EBattleOrder::Move,TEXT("command bar buttons arm and clear orders")); }
  // Gamepad: tactical pad buttons read as the same keys; a simulated pad press marks the pad as the active device.
  { const bool Map=PC->GamepadToKey(EKeys::Gamepad_FaceButton_Top)==EKeys::T&&PC->GamepadToKey(EKeys::Gamepad_FaceButton_Bottom)==EKeys::LeftMouseButton&&PC->GamepadToKey(EKeys::Gamepad_FaceButton_Left)==EKeys::RightMouseButton;
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftThumbstick,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftThumbstick,IE_Released,0.f));
    Check(Map&&PC->bGamepadActive,TEXT("gamepad buttons map to the keyboard and mouse actions"));PC->bGamepadActive=false; }
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-restored.png"),true,false);
  SmokeStage=1;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunAreaSmoke,.5f,false);return;
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
