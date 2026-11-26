#include "BattleMatch.h"
#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "NavigationSystem.h"
#include "BattleNavigation.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "BattleTactics.h"
#include "FutureBattle.h"

void UBattleMatch::InitializeMatch()
{
	if (IsEnabled()) return;
	MatchID = FGuid::NewGuid(); Phase = EBattleMatchPhase::Deployment;
	Credits[0] = Credits[1] = 3000; Winner = INDEX_NONE;
	if (BattleFieldLayout::IsSightField(GetWorld()))
	{
		// Wide maps take their objective count and positions from the layout (three on A-D, five on E).
		const FString Map = GetWorld()->GetMapName();
		for (int32 Index = 0; Index < BattleFieldLayout::ObjectiveCountForMap(Map); ++Index)
		{ FBattleObjective Point; Point.Position = BattleFieldLayout::ObjectiveForMap(Map, Index); Objectives.Add(Point); }
		// Objectives a side holds from the start (the depots of map F). Side 0 is the team of the west home, which is team 1 in a swapped trial.
		const auto& Def = BattleFieldLayout::ByName(Map); const int32 Swap = int32(FParse::Param(FCommandLine::Get(), TEXT("BattleTrialSwap")));
		for (int32 Index = 0; Index < Objectives.Num(); ++Index)
			if (const int32 Side = BattleFieldLayout::ObjectiveOwnerSide(Def, Index); Side == 0 || Side == 1) Objectives[Index].Owner = Side ^ Swap;
	}
	else for (const FVector Position : { FVector(-800, -1800, 0), FVector(0, 1800, 0), FVector(800, -1800, 0) })
	{
		FBattleObjective Point; Point.Position = BattleFieldLayout::IsLarge(GetWorld()) ? FVector(0,(Objectives.Num()-1)*4500,0) : Position; Objectives.Add(Point);
	}
 bArea=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bArea;if(bArea)InitializeArea();
}

int32 UBattleMatch::SquadPrice(EBattleTraining Training)
{
	const auto* T = GetDefault<UBattleTuning>();
	int32 Price = 0;
	switch (Training) {
	case EBattleTraining::Recruit: Price = T->RecruitCost; break;
	case EBattleTraining::Regular: Price = T->RegularCost; break;
	case EBattleTraining::Veteran: Price = T->VeteranCost; break;
	default: return INDEX_NONE;
	}
	return Price > 0 && Price <= MAX_int32 / 5 ? Price * 5 : INDEX_NONE;
}

int32 UBattleMatch::CurrentSquadPrice(EBattleTraining Training) const
{
 const int32 Price=SquadPrice(Training);return Price>0 && GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch?Price*2:Price;
}

bool UBattleMatch::HireSquad(int32 Team, EBattleTraining Training)
{
	LastMessage.Reset();
 if(bArea)return HireAreaSquad(Team,Training,false);
 auto* Mixed=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(Mixed->bMixedFastMatch)
 {
  const int32 Price=CurrentSquadPrice(Training);
  if(Team<0||Team>1||Price<=0||(Phase!=EBattleMatchPhase::Deployment&&Phase!=EBattleMatchPhase::Combat)){LastMessage=TEXT("Recruitment unavailable");return false;}
  if(Credits[Team]<Price){LastMessage=TEXT("Insufficient credits");return false;}
  if(!Mixed->SpawnPurchasedSquad(Team,Training,LastMessage))return false;
  Credits[Team]-=Price;LastMessage=FString::Printf(TEXT("10 soldiers deployed / -%d credits"),Price);return true;
 }
	if (BattleFieldLayout::IsLarge(GetWorld())) { LastMessage=TEXT("Fixed 500-soldier field / no reinforcements"); return false; }
	auto Fail = [this](const TCHAR* Message) { LastMessage = Message; return false; };
	if ((Team != 0 && Team != 1) || (Phase != EBattleMatchPhase::Deployment && Phase != EBattleMatchPhase::Combat)) return Fail(TEXT("Recruitment unavailable"));
	const int32 Price = SquadPrice(Training);
	if (Price <= 0) return Fail(TEXT("Invalid training price"));
	if (Credits[Team] < Price) return Fail(TEXT("Insufficient credits"));
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Mode || !Nav) return Fail(TEXT("Deployment navigation unavailable"));
	int32 Alive = 0; for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive()) ++Alive;
	if (Alive + 5 > 100) return Fail(TEXT("Field capacity reached (100)"));
	TArray<FVector> Positions;
	// Search the home area, validating every capsule before changing money or canonical state.
	for (int32 Column = 0; Column < 5 && Positions.Num() < 5; ++Column)
		for (int32 Row = 0; Row < 14 && Positions.Num() < 5; ++Row)
		{
			const FVector Candidate((Team ? 1.f : -1.f) * (2200 + Column * 220), (Row - 6.5f) * 260, 0);
			FNavLocation Projected;
			if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(75, 75, 200))) continue;
			const FVector Position = Projected.Location + FVector(0, 0, 95);
			bool bOccupied = GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(40, 90));
			for (const FVector& Other : Positions) bOccupied |= FVector::Dist2D(Position, Other) < 100;
			if (!bOccupied) Positions.Add(Position);
		}
	if (Positions.Num() != 5) return Fail(TEXT("Home deployment area is blocked"));
	TArray<ABattleUnit*> Spawned;
	for (const FVector& Position : Positions)
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		auto* Unit = GetWorld()->SpawnActor<ABattleUnit>(Position, FRotator(0, Team ? 180 : 0, 0), Params);
		if (!Unit)
		{
			for (auto* Rollback : Spawned) Rollback->Destroy();
			return Fail(TEXT("Deployment failed; credits retained"));
		}
		Spawned.Add(Unit);
	}
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	TArray<int32> Members; TMap<int32, FVector> Goals;
	for (auto* Unit : Spawned)
	{
		const int32 ID = Store->RegisterUnit(Team, 0, WeaponForSlot(Members.Num()));
		Store->SetTrainingPreset(ID, Training); Unit->InitializeUnit(ID); Mode->Units.Add(Unit);
		Members.Add(ID); Goals.Add(ID, Unit->GetActorLocation());
	}
	const int32 Squad = Store->CreateSquad(Team, Members);
	Store->SetSquadOrder(Squad, EBattleOrder::Hold, Positions[2], Goals);
	if (Team == 0) Store->SetSquadDelegated(Squad, true);
	Credits[Team] -= Price;
	LastMessage = FString::Printf(TEXT("%s squad deployed / -%d credits"), BattleTrainingName(Training), Price);
	if (Team == 0) if (auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->SetSelection(Spawned[0]);
	Mode->RefreshVisibility();
	return true;
}

bool UBattleMatch::StartCombat()
{
	if (Phase != EBattleMatchPhase::Deployment) return false;
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	int32 Alive[2] = {0, 0};
	if (Mode) for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive()) ++Alive[Unit->GetState()->Team];
	if (!Mode || !Alive[0] || !Alive[1]) { LastMessage = TEXT("Deploy at least one squad before starting"); return false; }
	Phase = EBattleMatchPhase::Combat; Mode->bCombatPaused = false; Mode->CombatStartsAt = GetWorld()->GetTimeSeconds();
	UpdateEnemyOrders();
	LastMessage = TEXT("Battle started");
	return true;
}

bool UBattleMatch::CanDelegateAllies() const
{
	const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	return Mode && (!Mode->bLeaderCommand || GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsWideField()) &&
		(Phase == EBattleMatchPhase::Deployment || Phase == EBattleMatchPhase::Combat);
}

bool UBattleMatch::SetAllyDelegated(int32 SquadID, bool bDelegated)
{
	if (!CanDelegateAllies() || !GetWorld()->GetSubsystem<UBattleState>()->SetSquadDelegated(SquadID, bDelegated)) return false;
	ClearPlayerRouteMission(SquadID);
	if (bDelegated) DelegationClock = 5.f;
	return true;
}

void UBattleMatch::Update(float DeltaSeconds)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (Phase != EBattleMatchPhase::Combat || !Mode || Mode->bCombatPaused || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	Elapsed += DeltaSeconds;
	UpdatePlayerRoutes(DeltaSeconds);
	UpdateTerrainTest(DeltaSeconds);
	// OrderSync (area matches): team 0's orders wait for this step's purchases, below (BattleMatch.h).
	const bool bSync=bArea&&OrderSync();bool bDelegateNow=false;
	if (CanDelegateAllies())
	{
		DelegationClock += DeltaSeconds;
		if (DelegationClock >= 5.f)
		{
			DelegationClock = 0.f;
			if(bSync)bDelegateNow=true;else UpdateTeamOrders(0, true);
		}
	}
	if (!BattleFieldLayout::IsLarge(GetWorld())) UpdateSupport(DeltaSeconds);
	UpdateObjectives(DeltaSeconds);
 if(bArea)
 {
  UpdateArea(DeltaSeconds);if(BattleTactics::BaseGarrison()&&Phase!=EBattleMatchPhase::Combat)return;UpdateSituationLog();UpdateHireQueue();DirectorClock+=DeltaSeconds;
  if(DirectorClock>=5)
  {
   DirectorClock=0;
   // SetupPhase: on the first step the AI teams spend their starting resources before any orders (Elapsed<1: not after a load).
   if(SetupPhase()&&!bSetupDone&&Elapsed<1.f)
   {
    bSetupDone=bSetupBurst=true;
    for(int32 Pass=0;Pass<60;++Pass){const int32 Before=Credits[0]+Credits[1];UpdateAreaHiring();if(Credits[0]+Credits[1]==Before)break;}
    bSetupBurst=false;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SETUP credits=%d/%d active=%d/%d time=%.1f"),Credits[0],Credits[1],ActiveSquads(0),ActiveSquads(1),Elapsed);
   }
   else UpdateAreaHiring();
   if(bDelegateNow)UpdateTeamOrders(0,true);
   UpdateEnemyOrders();
  }
  else if(bDelegateNow)UpdateTeamOrders(0,true);
  CheckTimeLimit();return;
 }
	if (Phase != EBattleMatchPhase::Combat) return;
	DirectorClock += DeltaSeconds; SupplyClock += DeltaSeconds;
	if (DirectorClock >= 5.f)
	{
		DirectorClock = FMath::Fmod(DirectorClock, 5.f);
  const bool MixedFast=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch;
  int32 EnemyAlive=0;for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())if(U.Value.IsAlive()&&U.Value.Team==1)++EnemyAlive;
  if((!BattleFieldLayout::IsLarge(GetWorld())||MixedFast) && EnemyAlive<(MixedFast?20:10)) HireSquad(1,Credits[1]>=CurrentSquadPrice(EBattleTraining::Regular)?EBattleTraining::Regular:EBattleTraining::Recruit);
		UpdateEnemyOrders();
		if (!BattleFieldLayout::IsLarge(GetWorld()) && Credits[1] >= 1300 && SupportCooldown[1] <= 0 && SupportUsed[1] < 3)
			for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 1 && RequestSupport(Unit->GetUnitID())) break;
	}
	if ((!BattleFieldLayout::IsLarge(GetWorld()) || GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch) && SupplyClock >= 1.f)
	{
		SupplyClock = FMath::Fmod(SupplyClock, 1.f);
  for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())if(U.Value.IsAlive())Resupply(U.Key);
	}
	CheckTimeLimit();
}

float UBattleMatch::ObjectiveValue(int32 Point) const { return BattleFieldLayout::ObjectiveValue(BattleFieldLayout::Of(GetWorld()), Point); }
float UBattleMatch::MatchTimeLimit() const { return BattleFieldLayout::Of(GetWorld()).MatchSeconds; }

void UBattleMatch::CheckTimeLimit()
{
	if (Elapsed >= MatchTimeLimit())
	{
  if(bArea){Score[0]=AreaScore(0);Score[1]=AreaScore(1);}
		const int32 WinningTeam = FMath::IsNearlyEqual(Score[0], Score[1], .01f) ? INDEX_NONE : Score[0] > Score[1] ? 0 : 1;
		Finish(WinningTeam, WinningTeam == INDEX_NONE ? TEXT("DRAW / TIME LIMIT") : WinningTeam == 0 ? TEXT("VICTORY / TIME LIMIT") : TEXT("DEFEAT / TIME LIMIT"));
	}
}

void UBattleMatch::UpdateObjectives(float DeltaSeconds)
{
	if (Phase != EBattleMatchPhase::Combat || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>(); if (!Mode || Mode->bCombatPaused) return;
	AdvanceObjectiveControl(DeltaSeconds, [Mode](const FBattleObjective& Point)
	{
		int32 Teams = 0;
  if(auto* L=Mode->GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); L->IsEnabled())
  {
   const auto* Store=Mode->GetWorld()->GetSubsystem<UBattleState>();
   for(const auto& B:L->GetBodies()) if(const auto* U=Store->Find(B.Key); U && U->IsAlive() && (BattleFieldLayout::Of(Mode->GetWorld()).bSectors ? BattleSectors::Cell(L->Position(B.Key))==BattleSectors::Cell(Point.Position) : InObjective(L->Position(B.Key),Point))) Teams|=1<<U->Team;
   return Teams;
  }
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && InObjective(Unit->GetActorLocation(), Point))
			Teams |= 1 << Unit->GetState()->Team;
		return Teams;
	});
}

bool UBattleMatch::InObjective(FVector Position, const FBattleObjective& Point)
{
	return FMath::Abs(Position.Z - Point.Position.Z) < 250 && FVector::DistSquared2D(Position, Point.Position) <= 450.*450.;
}

void UBattleMatch::AdvanceObjectiveControl(float DeltaSeconds, TFunctionRef<int32(const FBattleObjective&)> Presence)
{
	for (auto& Point : Objectives)
	{
		const int32 Teams = Presence(Point);
		Point.bContested = Teams == 3;
		if (Point.bContested) continue;
		const int32 Team = Teams == 1 ? 0 : Teams == 2 ? 1 : INDEX_NONE;
		const int32 OwnerBefore = Point.Owner;
		if (Team != INDEX_NONE && Team != Point.Owner)
		{
			if (Point.CapturingTeam != Team) { Point.CaptureProgress = 0; Point.CapturingTeam = Team; }
			Point.CaptureProgress += DeltaSeconds;
			if (Point.CaptureProgress >= 8.f)
			{
				Point.Owner = Team; Point.CapturingTeam = INDEX_NONE; Point.CaptureProgress = 0; Point.IncomeClock = 0;
			}
		}
		else { Point.CaptureProgress = 0; Point.CapturingTeam = INDEX_NONE; }
		// No payout on the capture frame or while an opponent is capturing the point.
		if (!bArea && Point.Owner != INDEX_NONE && Point.Owner == OwnerBefore && (Team == INDEX_NONE || Team == Point.Owner))
		{
			Score[Point.Owner] += DeltaSeconds;
			Point.IncomeClock += DeltaSeconds;
			const int32 Payments = FMath::FloorToInt(Point.IncomeClock / 5.f);
			if (Payments > 0) { Credits[Point.Owner] = FMath::Min(1000000, Credits[Point.Owner] + Payments * 20); Point.IncomeClock = FMath::Fmod(Point.IncomeClock, 5.f); }
		}
	}
	if (!bArea && (Score[0] >= 100 || Score[1] >= 100))
	{
		const int32 WinningTeam = FMath::IsNearlyEqual(Score[0], Score[1], .01f) ? INDEX_NONE : Score[0] > Score[1] ? 0 : 1;
		Finish(WinningTeam, WinningTeam == INDEX_NONE ? TEXT("DRAW / CONTROL SCORE") : WinningTeam == 0 ? TEXT("VICTORY / CONTROL SCORE") : TEXT("DEFEAT / CONTROL SCORE"));
	}
}

void UBattleMatch::UpdateEnemyOrders()
{
	UpdateTeamOrders(1);
}

int32 UBattleMatch::ChooseObjective(int32 Group, int32 Team) const
{
	const int32 Start = Group % Objectives.Num();
	const bool Rotate = BattleFieldLayout::IsSightField(GetWorld()) && (Team != int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"))));
	auto Index = [&](int32 I){return Rotate ? Objectives.Num()-1-I : I;};
	for (int32 Offset = 0; Offset < Objectives.Num(); ++Offset)
	{
		const int32 Target = Index((Start + Offset) % Objectives.Num());
		if (ObservedOwner(Team,Target) != Team) return Target;
	}
	return Index(Start);
}

int32 UBattleMatch::AreaStrategyTarget(int32 SquadID, int32 Team, bool& bBase) const
{
 // Each squad keeps a fixed slot frac(ID*golden ratio) so deaths and hires do not reshuffle other squads.
 // Points the team observes as its own weigh 1, others AttackWeight; the top BaseShare of slots assault the enemy home.
 const auto& Strategy=BattleTactics::AreaStrategy();
 // Stage 1 of the strategy layer (ROADMAP 2026-11-16): allocation numbers come from the team intent.
 const FAreaIntent Intent=AreaIntent(Team);
 bBase=false;
 if(Objectives.IsEmpty())return 0;
 const bool Rotate=BattleFieldLayout::IsSightField(GetWorld())&&(Team!=int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"))));
 TArray<double> Weight;double Sum=0;int32 Owned=0;
 for(int32 I=0;I<Objectives.Num();++I){const int32 P=Rotate?Objectives.Num()-1-I:I;const bool bOwn=ObservedOwner(Team,P)==Team;Owned+=bOwn;const double W=(bOwn?1.0:Intent.AttackWeight)*(P<FAreaIntent::MaxObjectives?Intent.PointWeight[P]:1.f);Weight.Add(W);Sum+=W;}
 // S4 trial: boost points where the team's own living soldiers (own positions only) are below the per-point average.
 // -BattleAreaTrialTeam=0/1 limits the S3-S6 trials to one team (the other plays the default) to measure who wins.
 const bool bTrial=Strategy.TrialTeam<0||Strategy.TrialTeam==Team;
 if(Intent.bFrontBalance&&Sum>0)
 {
  TArray<double> Own;Own.Init(0,Objectives.Num());
  const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())if(U.Value.Team==Team&&U.Value.IsAlive())
  {
   const FVector P=Runtime->Position(U.Key);
   for(int32 I=0;I<Objectives.Num();++I){const int32 Pt=Rotate?Objectives.Num()-1-I:I;if(FVector::DistSquared2D(P,Objectives[Pt].Position)<=FMath::Square(10000.)){Own[I]+=1;break;}}
  }
  double Avg=0;for(double O:Own)Avg+=O;Avg/=FMath::Max(1,Own.Num());
  Sum=0;
  for(int32 I=0;I<Weight.Num();++I){const double Deficit=Avg>0?FMath::Max(0.0,Avg-Own[I])/Avg:0.0;Weight[I]*=1.0+Intent.FrontGain*Deficit;Sum+=Weight[I];}
 }
 // Intel trial (1)(2): own points with more known enemies than own soldiers weigh more; other points weigh less the more enemies
 // are known around them (BattleAreaIntel.cpp; known positions only).
 if(AreaIntelOn(Team)&&Sum>0)
 {
  const auto& V=AreaIntelView(Team);const auto* T=GetDefault<UBattleTuning>();
  double Avg=0;for(int32 O:V.OwnAt)Avg+=O;Avg=FMath::Max(1.0,Avg/FMath::Max(1,V.OwnAt.Num()));
  Sum=0;
  for(int32 I=0;I<Weight.Num();++I)
  {
   const int32 P=Rotate?Objectives.Num()-1-I:I;if(!V.EnemyAt.IsValidIndex(P)){Sum+=Weight[I];continue;}
   if(ObservedOwner(Team,P)==Team)Weight[I]*=1.0+T->AreaIntelDefendGain*FMath::Min(2.0,FMath::Max(0,V.EnemyAt[P]-V.OwnAt[P])/Avg);
   else Weight[I]/=1.0+T->AreaIntelAttackGain*V.EnemyAt[P]/Avg;
   Sum+=Weight[I];
  }
 }
 // S6 trial: concentrate on the one point not observed as own that is nearest the team's own home (static positions only).
 if(Intent.FocusGain>0&&Sum>0)
 {
  int32 Best=INDEX_NONE;double BestD=DBL_MAX;const FVector Home=AreaHome(Team);
  // Strategy layer S2: the focus the team chose from its situation map comes first (while it is not observed as own).
  if(Objectives.IsValidIndex(Intent.FocusPoint)&&ObservedOwner(Team,Intent.FocusPoint)!=Team)Best=Rotate?Objectives.Num()-1-Intent.FocusPoint:Intent.FocusPoint;
  else
  for(int32 I=0;I<Objectives.Num();++I){const int32 P=Rotate?Objectives.Num()-1-I:I;if(ObservedOwner(Team,P)==Team)continue;const double D=FVector::DistSquared2D(Objectives[P].Position,Home);if(D<BestD){BestD=D;Best=I;}}
  if(Best!=INDEX_NONE){Sum+=Weight[Best]*Intent.FocusGain;Weight[Best]*=1.0+Intent.FocusGain;}
 }
 // S3 trial: home-assault share from observed state only (own observed points, last seen enemy home HP).
 double Share=Intent.BaseShare;
 if(bTrial&&Strategy.bBaseRule)
 {
  const float SeenHP=bArea&&Area.KnownEnemyBaseHP.IsValidIndex(Team)?Area.KnownEnemyBaseHP[Team]:-1.f;
  Share=(Owned>=2||(SeenHP>=0&&SeenHP<=Strategy.BaseLowHP))?Strategy.BaseShareHigh:Strategy.BaseShareLow;
 }
 // Intel trial (3): the enemy home was observed (own sight or a scout report) with few enemies around it: more squads assault it.
 if(AreaIntelOn(Team)){const auto& V=AreaIntelView(Team);if(V.bHomeCovered&&V.HomeEnemies<=GetDefault<UBattleTuning>()->AreaIntelHomeWeak)Share=FMath::Max(Share,double(GetDefault<UBattleTuning>()->AreaIntelHomeShare));}
 const double Slot=FMath::Frac(double(BattleTactics::AllocationIndex(GetWorld(),SquadID,Team))*0.6180339887498949);
 bBase=Slot>=1.0-Share;
 const double Pos=Slot/FMath::Max(1e-6,1.0-Share)*Sum;
 double Acc=0;
 for(int32 I=0;I<Weight.Num();++I){Acc+=Weight[I];if(Pos<Acc||I==Weight.Num()-1)return Rotate?Objectives.Num()-1-I:I;}
 return 0;
}

void UBattleMatch::UpdateTeamOrders(int32 Team, bool bDelegatedOnly)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Mode || (Team != 0 && Team != 1) || Phase != EBattleMatchPhase::Combat || Objectives.IsEmpty()) return;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	int32 Group = 0;
 // S1 (default ON, -BattleAreaStrategy=0 = old round-robin): weighted point spread from observed ownership only.
 const bool bStrategy=bArea&&BattleTactics::AreaStrategy().bEnabled;
 // Strategy layer S2 (BattleStrategy.cpp, -BattleStrategy>=2): the team's plan is reviewed once here, before any intent is read.
 // Front plan stage 2 (BattleFrontLine.cpp, -BattleFrontPlan=1; off while -BattleStrategy>=2 runs for the team).
 if(bStrategy)UpdateFrontPlan(Team);
 const FAreaIntent TeamIntent=AreaIntent(Team);
 // Squad groups (BattleGroups.cpp): members, AI formation (trial) and phases, before the orders that read them.
 if((CompositionTeams&(1<<Team))!=0)UpdateCompositionPurposes(Team);
 UpdateGroups(Team);
 if(bStrategy)UpdateBaseGroup(Team); // trial -BattleBaseGroup (BattleBaseGroup.cpp)
 if(bArea&&BattleTactics::ArtilleryPosts())UpdateArtilleryRaids(Team);
 if(bArea&&(BattleTactics::ATHunt()||BattleTactics::ATFire()))UpdateATHunts(Team);
 // Routes (map F): what the team sees now, gathered once for all its squads.
 const bool bRoutes=bStrategy&&RoutesOn();TArray<FVector> RouteEnemies;if(bRoutes)RouteEnemies=RouteKnownEnemies(Team);
 TArray<int32> RouteLoad; // -BattleRouteLoad: squads of this team already sent over each road section in this round
	for (const auto& Pair : Store->GetSquads()) if (Pair.Value.Team == Team)
	{
  if(BattleTactics::BaseGarrison()&&IsBaseGarrison(Pair.Key))
  {
   const auto& Guard=Pair.Value;
   if(Guard.CommandStatus==EBattleCommandStatus::Active&&(Team!=0||Guard.bPlayerDelegated)&&
      (!Guard.bHasMission||Guard.MissionOrder!=EBattleOrder::Defend||FVector::Dist2D(Guard.MissionDestination,AreaHome(Team))>BattleTactics::BaseGarrisonRadius()))
    GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IssueOrder(Pair.Key,EBattleOrder::Defend,AreaHome(Team),true);
   continue;
  }
		// Public objective status only; never use an unseen enemy's location.
  bool bStrategyBase=false;
		int32 Target = bStrategy ? AreaStrategyTarget(Pair.Key,Team,bStrategyBase) : ChooseObjective(Group, Team);
  // P1 trial: a squad bought for a need keeps that mission (point, enemy home, or own home defence).
  const int32* Purpose=bStrategy?Area.HirePurpose.Find(Pair.Key):nullptr;
  // Intel trial (4): a squad bought to defend a threatened point keeps that mission until released (BattleAreaIntel.cpp).
  if(!Purpose&&bStrategy)Purpose=IntelPurpose.Find(Pair.Key);
  if(Purpose&&*Purpose>=0&&*Purpose<Objectives.Num()){Target=*Purpose;bStrategyBase=false;}else if(Purpose&&*Purpose==100)bStrategyBase=true;
  const bool bHomeDefence=Purpose&&*Purpose==101;if(bHomeDefence)bStrategyBase=false;
  // Heavy infantry (tactical role): hold the thinnest observed own point, or take the point nearest home; never the home assault.
  if(bStrategy&&!Purpose&&Pair.Value.SquadType==BattleSquadType::Heavy&&!Objectives.IsEmpty()){bStrategyBase=false;Target=AreaHeavyTarget(Team);}
  // Scout pairs (tactical role): watch from a post ahead of an own front point (AreaScoutPost); never the home assault.
  const bool bScout=bStrategy&&!Purpose&&Pair.Value.SquadType==BattleSquadType::Scout;if(bScout)bStrategyBase=false;
  // Indirect fire (mortar teams, self-propelled guns; tactical role): never the home assault; held back from known enemies below.
  const bool bArtillery=bStrategy&&!Purpose&&BattleSquadType::IsArtillery(Pair.Value.SquadType);if(bArtillery)bStrategyBase=false;
  // Armoured cars (tactical role, stage 2): an open point away from known anti-tank threats (AreaCarTarget); never the home assault.
  if(bStrategy&&!Purpose&&Pair.Value.SquadType==BattleSquadType::ArmoredCar)
  {bStrategyBase=false;if(const int32 CarTarget=AreaCarTarget(Team,Pair.Key);CarTarget!=INDEX_NONE)Target=CarTarget;}
  // Trial -BattleACShield: ahead of the nearest attacking own infantry squad (none: as above).
  bool bCarShield=false;FVector CarShield=FVector::ZeroVector;
  if(bStrategy&&!Purpose&&BattleTactics::ACShield()&&Pair.Value.SquadType==BattleSquadType::ArmoredCar&&Pair.Value.CommandStatus==EBattleCommandStatus::Active)
   bCarShield=ACShieldPost(Team,Pair.Key,CarShield);
  // Exoskeleton soldiers (tactical role): the fast reserve (AreaExoTarget); never the home assault.
  if(bStrategy&&!Purpose&&Pair.Value.SquadType==BattleSquadType::Exo&&Pair.Value.CommandStatus==EBattleCommandStatus::Active)
  {bStrategyBase=false;if(const int32 ExoTarget=AreaExoTarget(Team,Pair.Key);ExoTarget!=INDEX_NONE)Target=ExoTarget;}
  // Armour AI, stage 3 (BattleArmourAI.cpp): a tank outside the home-assault slots supports the point with the most known enemies from
  // an overwatch post; an anti-tank team goes to the point nearest a known enemy vehicle (the normal spread while none is known).
  bool bTankPost=false;
  if(bStrategy&&!Purpose&&BattleTactics::ArmourAI(Team)&&Pair.Value.CommandStatus==EBattleCommandStatus::Active)
  {
   if(Pair.Value.SquadType==BattleSquadType::Tank&&!bStrategyBase){if(const int32 TankTarget=AreaTankTarget(Team,Pair.Key);TankTarget!=INDEX_NONE){Target=TankTarget;bTankPost=true;}}
   else if(Pair.Value.SquadType==BattleSquadType::AntiTank){if(const int32 ATTarget=AreaATTarget(Team,Pair.Key);ATTarget!=INDEX_NONE){Target=ATTarget;bStrategyBase=false;}}
  }
  ++Group;
		// Keep assignment stable when only some squads are delegated.
		if (bDelegatedOnly && !Pair.Value.bPlayerDelegated)
		{
   // Log only: the player's own squads the team AI does not move (BATTLE_FREE_SQUAD role=player, see below).
   if(bArea&&GroupOf(Pair.Key)==INDEX_NONE&&Pair.Value.CommandStatus==EBattleCommandStatus::Active)
    if(auto* FreeRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();FreeRuntime&&FreeRuntime->IsWideField()&&Store->Find(Pair.Value.LeaderUnitID))
     if(int32& Last=FreeLogSlot.FindOrAdd(Pair.Key,-1);Last!=FMath::FloorToInt(Elapsed/30.f))
     {
      Last=FMath::FloorToInt(Elapsed/30.f);const FVector LP=FreeRuntime->Position(Pair.Value.LeaderUnitID);const FIntPoint Here=BattleSectors::Cell(LP),To=BattleSectors::Cell(Pair.Value.MissionDestination);
      UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FREE_SQUAD t=%.0f team=%d squad=%d type=%d role=player here=%d,%d home_m=%.0f to=%d,%d order=%d target=-1 delegated=0 action=%d reason=%s"),
       Elapsed,Team,Pair.Key,int32(Pair.Value.SquadType),Here.X,Here.Y,FVector::Dist2D(LP,AreaHome(Team))/100.,To.X,To.Y,Pair.Value.bHasMission?int32(Pair.Value.MissionOrder):-1,
       int32(Pair.Value.TacticalPlan.Action),*Pair.Value.TacticalPlan.Reason);
     }
   continue;
		}
  // A routed manual order of the player is under way (BattlePlayerRoutes.cpp): the team AI leaves the squad alone.
  if(!PlayerRoutes.IsEmpty())if(const FPlayerRoute* Routed=PlayerRoutes.Find(Pair.Key);Routed&&Routed->bMission)continue;
  const bool BaseAssault=bStrategy?bStrategyBase:(bArea&&Group%5==0);
  FVector Center=BaseAssault?AreaHome(1-Team)*(25./27.):bHomeDefence?AreaHome(Team)*(25./27.):Objectives[Target].Position;
  EBattleOrder Order=bHomeDefence||(!BaseAssault&&ObservedOwner(Team,Target)==Team)?EBattleOrder::Defend:EBattleOrder::AttackMove;
  // After a warning (an enemy near or a hit) the pair moves to its fall-back point, then watches from there until ScoutHoldSeconds pass without one (BattleScoutReport.cpp).
  if(bScout)
  {
   const auto* Rep=ScoutReports.Find(Pair.Key);
   if(Rep&&Elapsed<Rep->RetreatUntil){Center=Rep->RetreatPoint;Order=EBattleOrder::Move;}
   else if(Rep&&Rep->LastDanger>=0&&Elapsed-Rep->LastDanger<GetDefault<UBattleTuning>()->ScoutHoldSeconds&&Rep->Burns>0){Center=Rep->RetreatPoint;Order=EBattleOrder::Defend;}
   else{Center=AreaScoutPost(Team,Pair.Key);Order=EBattleOrder::Defend;}
  }
  // S5 trial: a home-assault squad does not stop to fight until its leader is within InfiltrateRadius of the enemy home.
  if(BaseAssault&&bStrategy&&BattleTactics::AreaStrategy().InfiltrateRadius>0&&(BattleTactics::AreaStrategy().TrialTeam<0||BattleTactics::AreaStrategy().TrialTeam==Team))
   if(auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();Runtime&&Runtime->IsWideField()&&Store->Find(Pair.Value.LeaderUnitID))
    if(FVector::Dist2D(Runtime->Position(Pair.Value.LeaderUnitID),AreaHome(1-Team))>BattleTactics::AreaStrategy().InfiltrateRadius)Order=EBattleOrder::Move;
  // S7 trial: a home-assault squad first heads for a flank waypoint beside the centre line (away from the central point),
  // then for the enemy home once its leader is past the middle of the field. Static map positions and own leader only.
  if(BaseAssault&&bStrategy&&TeamIntent.FlankOffset>0&&!bRoutes)
   if(auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();Runtime&&Runtime->IsWideField()&&Store->Find(Pair.Value.LeaderUnitID))
   {
    const FVector Own=AreaHome(Team),Axis=AreaHome(1-Team)-Own,P=Runtime->Position(Pair.Value.LeaderUnitID);
    // Symmetric start trial: the east side's route is the west side's rotated 180 degrees (south instead of north).
    const float SideSign=BattleTactics::SymmetricStart(GetWorld())&&Own.X>0?-1.f:1.f;
    const FVector Way=BattleFieldLayout::Expand(FVector(0,SideSign*((TeamIntent.bFlankSameSide||(Pair.Key/10)%2)?1:-1)*TeamIntent.FlankOffset*12000.f,0),BattleFieldLayout::IsExpanded(GetWorld()));
    const double T=FVector::DotProduct(P-Own,Axis)/FMath::Max(1.0,Axis.SizeSquared2D());
    if(T<0.45&&FVector::Dist2D(P,Way)>2500)Center=Way;
   }
  const bool bMortarPost=bArea&&BattleTactics::ArtilleryPosts()&&Pair.Value.SquadType==BattleSquadType::Mortar;
  if(bMortarPost){Center=ChooseMortarPost(Team,Pair.Key);Order=EBattleOrder::Defend;}
  else if(bArtillery&&bArea)Center=AreaArtilleryPost(Team,Pair.Key,Center);
  const int32* RaidPost=bArea&&BattleTactics::ArtilleryPosts()?ArtilleryRaids.Find(Pair.Key):nullptr;
  if(RaidPost){Center=BattleTactics::ArtilleryRaidChase()?ArtilleryRaidChases.FindChecked(Pair.Key).Target:ArtilleryPostPositions[1-Team][*RaidPost];Order=EBattleOrder::AttackMove;
   // Trial -BattleArtilleryRaidSneak: measured raids stalled in the mid-field fights; until the leader is near the target the raid
   // moves, the squad tactics do not hold at every seen enemy (BattleSquadRetreat.cpp) and the team AI does not use it for flank/support.
   if(RaidSneaking(Pair.Key))Order=EBattleOrder::Move;}
  // Trial -BattleArtilleryRaidGroup: the raid group's rally point / flank waypoint, and the infantry screen on the same side.
  bool bRaidScreen=false;
  if(bArea&&BattleTactics::ArtilleryRaidGroup()>0){FVector RaidGoalCenter=Center;EBattleOrder RaidGoalOrder=Order;
   if(ArtilleryRaidGoal(Team,Pair.Key,RaidGoalCenter,RaidGoalOrder)){Center=RaidGoalCenter;Order=RaidGoalOrder;bRaidScreen=RaidPost==nullptr;}}
  // Trial -BattleATHunt: a squad asked by an armoured unit attacks the anti-tank soldier it saw (BattleATHunt.cpp).
  bool bATHunt=false;
  if(bArea&&BattleTactics::ATHunt()){FVector HuntCenter;if(ATHuntGoal(Team,Pair.Key,HuntCenter)){Center=HuntCenter;Order=EBattleOrder::AttackMove;bATHunt=true;}}
  const bool bRaidDuty=RaidPost!=nullptr||bRaidScreen||bATHunt;
  // The tank's overwatch post; a post near its current mission is kept (no new order for every small change in what the team knows).
  if(bCarShield){Center=CarShield;Order=EBattleOrder::AttackMove;if(Pair.Value.bHasMission&&FVector::Dist2D(Pair.Value.MissionDestination,Center)<2000.)Center=Pair.Value.MissionDestination;
   else UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AC_SHIELD team=%d squad=%d post=%s"),Team,Pair.Key,*Center.ToCompactString());}
  if(bTankPost){Center=AreaTankPost(Team,Target);if(Pair.Value.bHasMission&&FVector::Dist2D(Pair.Value.MissionDestination,Center)<2500.)Center=Pair.Value.MissionDestination;}
  // A squad in a group the AI runs follows its group: the rally point while gathering, then the group's objective together.
  if(!bRaidDuty&&!bMortarPost&&bStrategy&&!Purpose&&!Groups.IsEmpty())GroupOrder(Pair.Key,Team,Center,Order);
  // Trial -BattleFrontReserve: a free squad of the front army's reserve waits behind the line (BattleFrontLine.cpp, 6c).
  if(!bRaidDuty&&!bMortarPost&&bStrategy&&!Purpose&&!FrontReservePosts.IsEmpty()&&GroupOf(Pair.Key)==INDEX_NONE)FrontReserveOrder(Pair.Key,Team,Center,Order);
  // A supply vehicle (BattleSupplyCar.cpp): home to load while low, else behind its group's post or the front own point; never the home assault.
  if(bStrategy&&Pair.Value.SquadType==BattleSquadType::Supply)SupplyCarOrder(Pair.Key,Team,Center,Order);
  // Routes: a distant goal is reached junction by junction along the roads of the map (BattleRoutes.cpp). Not for scout pairs
  // (their posts and fall-backs are their own) or the home defence.
  // Diagnosis -BattleOrderTrace=1 (2026-11-25, log only): the goal before the road routing, for BATTLE_ORDER_TRACE below.
  const FVector TraceGoal=Center;
  bool bRoadWaypoint=false,bSectorStage=false;
  // A surprise bridge leg is already a precise waypoint; road/sector routing must not replace it with another crossing.
  bool bSurpriseBridge=false;
  if(bStrategy&&!Purpose&&SurpriseOn(Team))if(const auto* G=Groups.Find(GroupOf(Pair.Key));G&&G->Kind==2&&G->Phase<2&&G->SurpriseBridgeStage!=0&&GroupAIRun(*G))bSurpriseBridge=true;
  if(!bRaidDuty&&!bMortarPost&&bRoutes&&!bScout&&!bHomeDefence&&!bSurpriseBridge)
   if(auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();Runtime&&Runtime->IsWideField()&&Store->Find(Pair.Value.LeaderUnitID))
   {
    const FVector P=Runtime->Position(Pair.Value.LeaderUnitID);
    // The player's route choice for the squad comes before the AI's own (empty map in matches without a player's choice).
    const bool bChosen=!PlayerRoutes.IsEmpty()&&PlayerRouteLane(Pair.Key)!=0;
    FVector Step=bChosen?PlayerRouteStep(Pair.Key,P,Center):RouteStep(P,Center,BaseAssault&&!(BattleTactics::GroupSupplySlot()&&Pair.Value.SquadType==BattleSquadType::Supply)?RouteFlankSide(P,AreaHome(Team),RouteEnemies,Pair.Key):0,RouteEnemies,&RouteLoad,Pair.Key);
    // -BattlePinnedFlank: a pinned AI sector squad first takes a sector beside the threat.
    static const bool bSectorAIOn=[](){int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorAI="),V);return V!=0;}();
    FVector PinStage;
    if(UBattleLeaderRuntime::PinnedFlankEnabled()&&SectorRouteOn()&&bSectorAIOn&&!bChosen&&!Purpose&&BattleFieldLayout::Of(GetWorld()).bSectors&&
       !Runtime->IsVehicle(Pair.Value.LeaderUnitID)&&PinnedFlankStage(Pair.Key,P,PinStage))
    {Center=PinStage;Order=EBattleOrder::AttackMove;bSectorStage=true;}
    // -BattleCloseIn: a sector squad trading fire at long range moves one sector closer first.
    else if(UBattleLeaderRuntime::CloseInEnabled()&&SectorRouteOn()&&bSectorAIOn&&!bChosen&&!Purpose&&BattleFieldLayout::Of(GetWorld()).bSectors&&
       !Runtime->IsVehicle(Pair.Value.LeaderUnitID)&&CloseInStage(Pair.Key,P,PinStage))
    {Center=PinStage;Order=EBattleOrder::AttackMove;bSectorStage=true;}
    // -BattleSectorFlank: the last leg (no junction left) may also stop in a sector beside the enemy's front first.
    else if(!Step.Equals(Center,1) || (SectorFlankOn()&&SectorRouteOn()))
    {
     // Trial -BattleSectorRoute=1 (BattleSectorRoute.cpp): infantry on a map with sectors stops in a sector beside the junction
     // (a sector mission) instead of walking to the junction itself.
     FVector Stage;
     static const bool bSectorAI=[](){int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorAI="),V);return V!=0;}();
     if(SectorRouteOn()&&bSectorAI&&!bChosen&&!Purpose&&BattleFieldLayout::Of(GetWorld()).bSectors&&!Runtime->IsVehicle(Pair.Value.LeaderUnitID)&&SectorStage(Pair.Value,P,Step,Center,RouteEnemies,Stage))
     {Center=Stage;Order=EBattleOrder::AttackMove;bSectorStage=true;}
     else if(!Step.Equals(Center,1)){bRoadWaypoint=true;Center=Step;if(!Runtime->IsVehicle(Pair.Value.LeaderUnitID))++RoadSteps[Team]; /* log only (road=) */if(Order==EBattleOrder::Defend)Order=EBattleOrder::AttackMove;}
    }
   }
  // Log only (user question 2026-11-26: what do the unassigned squads near the home do?): every 30 s, each active squad in no group
  // with its role in this update, where its leader is, where it is sent and what its tactics do.
  if(bArea&&GroupOf(Pair.Key)==INDEX_NONE&&Pair.Value.CommandStatus==EBattleCommandStatus::Active)
   if(auto* FreeRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();FreeRuntime&&FreeRuntime->IsWideField()&&Store->Find(Pair.Value.LeaderUnitID))
   {
    const int32 Slot=FMath::FloorToInt(Elapsed/30.f);
    if(int32& Last=FreeLogSlot.FindOrAdd(Pair.Key,-1);Last!=Slot)
    {
     Last=Slot;
     const TCHAR* Role=bRaidDuty?TEXT("raid_or_hunt"):bMortarPost||bArtillery?TEXT("artillery"):bScout?TEXT("scout"):bHomeDefence?TEXT("home_defence"):BaseAssault?TEXT("enemy_home")
      :FrontReservePosts.Contains(Pair.Key)?TEXT("front_reserve"):bTankPost||bCarShield?TEXT("armour_post"):Purpose?TEXT("bought_for_point"):TEXT("point");
     const FVector LP=FreeRuntime->Position(Pair.Value.LeaderUnitID);const FIntPoint Here=BattleSectors::Cell(LP),To=BattleSectors::Cell(Center);
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FREE_SQUAD t=%.0f team=%d squad=%d type=%d role=%s here=%d,%d home_m=%.0f to=%d,%d order=%d target=%d delegated=%d action=%d reason=%s"),
      Elapsed,Team,Pair.Key,int32(Pair.Value.SquadType),Role,Here.X,Here.Y,FVector::Dist2D(LP,AreaHome(Team))/100.,To.X,To.Y,int32(Order),Target,Pair.Value.bPlayerDelegated?1:0,
      int32(Pair.Value.TacticalPlan.Action),*Pair.Value.TacticalPlan.Reason);
    }
   }
  if(auto* Leader=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); Leader->IsWideField())
  {
   const auto& S=Pair.Value;
   if(S.CommandStatus!=EBattleCommandStatus::Active) continue;
   // A tank keeps the goal it was given even when it drives only to the end of a partial route (SameVehicleMission).
   static const bool EnableSectorAI=[](){int32 V=0; FParse::Value(FCommandLine::Get(),TEXT("BattleSectorAI="),V);return V!=0;}();
   // Trial -BattleFrontPostSector=1 (default 0; user test play 2026-11-24, "the front gathers in the centre, nobody on the flanks"): the
   // posts were spread across the map, but only a quarter of the front army's squads stood on theirs and six in ten were in the four
   // middle rows - walking out from the home in the middle, each stopped to fight what it met on the way (an AI order without sectors
   // halts at contact). A front army squad's post becomes a sector mission, so it fires on the move until it is inside its post's sector
   // (-BattleSectorMoveFire, on for map I), as the whole AI does with -BattleSectorAI=1.
   static const bool bFrontPostSector=[](){int32 V=0; FParse::Value(FCommandLine::Get(),TEXT("BattleFrontPostSector="),V);return V!=0;}();
   const bool bSectorAIHere=EnableSectorAI || (bFrontPostSector && FrontPosts.Contains(Pair.Key) && [&]{const auto* FG=Groups.Find(GroupOf(Pair.Key));return FG&&FG->Kind==1;}());
   const bool Sector=!bRaidDuty && !bMortarPost && bSectorAIHere && !bSurpriseBridge && !bRoadWaypoint && BattleFieldLayout::Of(GetWorld()).bSectors && (bSectorStage || !BaseAssault) && !Purpose && !Leader->IsVehicle(S.LeaderUnitID)
      && (Order==EBattleOrder::AttackMove || Order==EBattleOrder::Defend) && BattleSectors::Valid(BattleSectors::Cell(Center));
   if(Sector)Center=BattleSectors::Center(BattleSectors::Cell(Center));
   // Trial -BattleArriveWatch (BattleTactics.h): arrived at its attack-move destination, it keeps the mission for a while.
   if(BattleTactics::ArriveWatch()>0.f)
   {
    const auto* WatchGroup=Groups.Find(GroupOf(Pair.Key));
    if(bRaidDuty || bMortarPost || bSurpriseBridge || bHomeDefence || Order!=EBattleOrder::AttackMove || (WatchGroup && WatchGroup->Phase==0) || Leader->IsVehicle(S.LeaderUnitID))ArriveWatches.Remove(Pair.Key);
    else if(ArriveWatchHolds(S,Leader->Position(S.LeaderUnitID)))continue;
   }
   // Diagnosis -BattleOrderTrace=1 (2026-11-25, default 0, log only): every new mission of an infantry squad - where the leader stands, the
   // goal the layers above gave (group, front, flank), and what the road routing made of it. Map K with lanes: the destination sector
   // of a squad alternated every order round (1221 of 1513 lane choices in a match).
   static const bool bOrderTrace=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleOrderTrace="),V);return V!=0;}();
   if(bOrderTrace && !Leader->IsVehicle(S.LeaderUnitID) && (!S.bHasMission || S.bSectorMission!=Sector || S.MissionOrder!=Order || !(Sector?BattleSectors::Cell(S.MissionDestination)==BattleSectors::Cell(Center):S.MissionDestination.Equals(Center,20))))
   {const FVector TraceAt=Leader->Position(S.LeaderUnitID);
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ORDER_TRACE squad=%d team=%d at=(%.0f,%.0f) goal=(%.0f,%.0f) now=(%.0f,%.0f) was=(%.0f,%.0f) order=%d road=%d stage=%d sector=%d group=%d"),
     Pair.Key,Team,TraceAt.X,TraceAt.Y,TraceGoal.X,TraceGoal.Y,Center.X,Center.Y,S.MissionDestination.X,S.MissionDestination.Y,int32(Order),bRoadWaypoint?1:0,bSectorStage?1:0,Sector?1:0,GroupOf(Pair.Key));}
   if(!S.bHasMission || S.bSectorMission!=Sector || S.MissionOrder!=Order || !(Sector?BattleSectors::Cell(S.MissionDestination)==BattleSectors::Cell(Center):S.MissionDestination.Equals(Center,20))) if(!Leader->SameVehicleMission(Pair.Key,Order,Center)) Leader->IssueOrder(Pair.Key,Order,Center,true,Sector);
   continue;
  }
		TMap<int32, FVector> Goals; bool bNeedsOrder = Pair.Value.Order != Order || !Pair.Value.Destination.Equals(Center, 20); bool bReachable = true;
  if (GetWorld()->GetSubsystem<UBattleSquadTactics>()->IsEnabled())
   bNeedsOrder |= !Pair.Value.bHasMission || Pair.Value.MissionOrder != Order || !Pair.Value.MissionDestination.Equals(Center, 20);
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->SquadID == Pair.Key)
		{
			bNeedsOrder |= Unit->GetState()->Action == EBattleAction::MoveFailed;
			FVector Goal; const FVector Offset(0, (Goals.Num() - 2) * 100, 0);
			if (BattleNavigation::FormationGoal(Unit, Center, Offset, Goal)) Goals.Add(Unit->GetUnitID(), Goal); else bReachable = false;
		}
		if (bNeedsOrder && bReachable && !Goals.IsEmpty()) Store->SetSquadOrder(Pair.Key, Order, Center, Goals,
   GetWorld()->GetSubsystem<UBattleSquadTactics>()->IsEnabled());
	}
}

int32 UBattleMatch::Resupply(int32 UnitID, int32 RequestedRounds)
{
	if(bArea)return 0; // Area resources buy replacement soldiers only.
	if (Phase != EBattleMatchPhase::Combat || RequestedRounds <= 0) return 0;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>(); const auto* S = Store->Find(UnitID);
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!S || !S->IsAlive() || !Mode || Mode->bCombatPaused) return 0;
	ABattleUnit* Unit = nullptr; for (ABattleUnit* Candidate : Mode->Units) if (Candidate && Candidate->GetUnitID() == UnitID) { Unit = Candidate; break; }
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Unit && !Runtime->bMixedFastMatch)return 0;
 const FVector Position=Runtime->bMixedFastMatch?Runtime->Position(UnitID):Unit->GetActorLocation();
	bool bInSupply = FMath::Abs(Position.X - (S->Team ? 2600.f : -2600.f)) <= 800 && FMath::Abs(Position.Y) <= 2100 && FMath::Abs(Position.Z) < 250;
 if(Runtime->bMixedFastMatch)bInSupply=FVector::Dist2D(Position,FVector((S->Team?1:-1)*BattleFieldLayout::HomeDistance(GetWorld()),0,0))<=2500 && FMath::Abs(Position.Z)<250;
	for (const auto& Point : Objectives) bInSupply |= Point.Owner == S->Team && !Point.bContested && Point.CapturingTeam == INDEX_NONE && FVector::Dist2D(Position, Point.Position) <= 450 && FMath::Abs(Position.Z - Point.Position.Z) < 250;
	if (!bInSupply) return 0;
	const int32 Added = Store->AddReserve(UnitID, FMath::Min(RequestedRounds, Credits[S->Team]));
	Credits[S->Team] -= Added; return Added;
}

bool UBattleMatch::ArriveWatchHolds(const FBattleSquadState& Squad,const FVector& LeaderAt)
{
 // Only while the squad follows its attack-move mission (fighting, retreating or resupplying it is elsewhere; back on the mission
 // it watches again).
 if(Squad.SquadType!=BattleSquadType::Rifle || !Squad.bHasMission || Squad.MissionOrder!=EBattleOrder::AttackMove || Squad.TacticalPlan.Action!=EBattleSquadTactic::Mission)
 {ArriveWatches.Remove(Squad.SquadID);return false;}
 // Arrived: the leader within 15 m of the destination; in a sector mission, inside the sector. Near: within 65 m (a road waypoint
 // is replaced by the next one at 60 m).
 const double D2=FVector::DistSquared2D(LeaderAt,Squad.MissionDestination);
 const bool bThere=Squad.bSectorMission?BattleSectors::Cell(LeaderAt)==BattleSectors::Cell(Squad.MissionDestination):D2<=FMath::Square(1500.);
 auto* W=ArriveWatches.Find(Squad.SquadID);
 // Another mission (the next one, or a player's order in between): its own watch. A clock that ran back: a new start.
 if(W && (W->Revision!=Squad.MissionRevision || Elapsed<W->Start)){ArriveWatches.Remove(Squad.SquadID);W=nullptr;}
 if(!W)
 {
  if(!bThere && (Squad.bSectorMission || D2>FMath::Square(6500.)))return false;
  FArriveWatch Watch;Watch.Revision=Squad.MissionRevision;Watch.Start=Elapsed;W=&ArriveWatches.Add(Squad.SquadID,Watch);
 }
 if(W->Until<0.f)
 {
  // Walking in: at most 30 s (a destination its leader cannot reach), then the next mission.
  if(!bThere && Elapsed-W->Start<30.f)return true;
  W->Until=bThere?Elapsed+BattleTactics::ArriveWatch():Elapsed;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARRIVE_WATCH t=%.0f squad=%d team=%d sector=%d arrived=%d walk_in=%.0f until=%.0f"),Elapsed,Squad.SquadID,Squad.Team,Squad.bSectorMission?1:0,bThere?1:0,Elapsed-W->Start,W->Until);
 }
 return Elapsed<W->Until;
}
void UBattleMatch::Finish(int32 WinningTeam, const FString& Reason)
{
	if (Phase != EBattleMatchPhase::Combat) return;
	Phase = EBattleMatchPhase::Finished; Winner = WinningTeam;
	if (auto* Runtime = GetWorld()->GetSubsystem<UBattleLeaderRuntime>()) Runtime->LogSightHitStats(TEXT("finish"));
	if (auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
	{
		Mode->bBattleEnded = true; Mode->Result = Reason;
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->SavedAI) Unit->SavedAI->StopMovement();
	}
}
