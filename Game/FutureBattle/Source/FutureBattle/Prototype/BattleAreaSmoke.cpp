#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "InputKeyEventArgs.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "BattleWarScore.h"
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
    const int32 Cost=BattleSquadType::AreaCost(BattleSquadType::AntiTank);
    if(M->Credits[0]<Cost){const int32 Add=Cost-M->Credits[0];M->Area.Income[0]+=Add;M->Credits[0]+=Add;}
    const int32 C0=M->Credits[0];const bool Hired=M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::AntiTank);
    int32 SID=0;for(const auto& P:Store->GetSquads())SID=FMath::Max(SID,P.Key);const auto* AS=Store->FindSquad(SID);
    bool ATGood=Hired&&Cost==12&&M->Credits[0]==C0-Cost&&AS&&AS->SquadType==BattleSquadType::AntiTank&&AS->Members.Num()==4&&AS->DisplayName.Contains(TEXT("対戦車班"));
    int32 Gunners=0;if(ATGood)for(int32 ID:AS->Members){const auto* U=Store->Find(ID);if(U->Weapon==EBattleWeapon::AntiTank){++Gunners;ATGood&=U->Ammo==1&&U->Reserve==3;}}
    if(ATGood){const int32 G=AS->Members[1];Store->AddReserve(G,90);ATGood&=Store->Find(G)->Reserve==3;}
    Check(ATGood&&Gunners==2,TEXT("anti-tank team: 4 soldiers, 2 anti-tank weapons with 4 rounds, 12 resources"));
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
    Mode->bCombatPaused=true;Save=M->CaptureSnapshot();const bool Valid=Save&&Save->Validate(Error);
    const bool Restored=Valid&&M->RestoreSnapshot(Save);ABattleUnit* After=Actor(TankID);
    Check(Restored&&After&&IsVehicle(TankID)&&After->GetCapsuleComponent()->GetScaledCapsuleRadius()>=150.f&&Turrets.Contains(TankID),TEXT("tank survives save and restore"));
    Mode->bCombatPaused=false;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AREA_TANK_SMOKE squad=%d tank=%d nav=%s validation=%s"),TSID,TankID,VN?*VN->GetName():TEXT("none"),*Error);
   }
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
