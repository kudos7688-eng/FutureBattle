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

void UBattleMatch::InitializeMatch()
{
	if (IsEnabled()) return;
	MatchID = FGuid::NewGuid(); Phase = EBattleMatchPhase::Deployment;
	Credits[0] = Credits[1] = 3000; Winner = INDEX_NONE;
	for (const FVector Position : { FVector(-800, -1800, 0), FVector(0, 1800, 0), FVector(800, -1800, 0) })
	{
		FBattleObjective Point; Point.Position = BattleFieldLayout::IsSightField(GetWorld()) ? BattleFieldLayout::Objective(Objectives.Num(),BattleFieldLayout::IsExpanded(GetWorld())) : BattleFieldLayout::IsLarge(GetWorld()) ? FVector(0,(Objectives.Num()-1)*4500,0) : Position; Objectives.Add(Point);
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
	if (bDelegated) DelegationClock = 5.f;
	return true;
}

void UBattleMatch::Update(float DeltaSeconds)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (Phase != EBattleMatchPhase::Combat || !Mode || Mode->bCombatPaused || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	Elapsed += DeltaSeconds;
	if (CanDelegateAllies())
	{
		DelegationClock += DeltaSeconds;
		if (DelegationClock >= 5.f)
		{
			DelegationClock = 0.f;
			UpdateTeamOrders(0, true);
		}
	}
	if (!BattleFieldLayout::IsLarge(GetWorld())) UpdateSupport(DeltaSeconds);
	UpdateObjectives(DeltaSeconds);
 if(bArea){UpdateArea(DeltaSeconds);DirectorClock+=DeltaSeconds;if(DirectorClock>=5){DirectorClock=0;UpdateAreaHiring();UpdateEnemyOrders();}CheckTimeLimit();return;}
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

float UBattleMatch::MatchTimeLimit() const { return BattleFieldLayout::IsSightField(GetWorld()) ? 600.f : 180.f; }

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
   for(const auto& B:L->GetBodies()) if(const auto* U=Store->Find(B.Key); U && U->IsAlive() && InObjective(L->Position(B.Key),Point)) Teams|=1<<U->Team;
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
 bBase=false;
 if(Objectives.IsEmpty())return 0;
 const bool Rotate=BattleFieldLayout::IsSightField(GetWorld())&&(Team!=int32(FParse::Param(FCommandLine::Get(),TEXT("BattleTrialSwap"))));
 TArray<double> Weight;double Sum=0;int32 Owned=0;
 for(int32 I=0;I<Objectives.Num();++I){const int32 P=Rotate?Objectives.Num()-1-I:I;const bool bOwn=ObservedOwner(Team,P)==Team;Owned+=bOwn;const double W=bOwn?1.0:Strategy.AttackWeight;Weight.Add(W);Sum+=W;}
 // S3 trial: home-assault share from observed state only (own observed points, last seen enemy home HP).
 double Share=Strategy.BaseShare;
 if(Strategy.bBaseRule)
 {
  const float SeenHP=bArea&&Area.KnownEnemyBaseHP.IsValidIndex(Team)?Area.KnownEnemyBaseHP[Team]:-1.f;
  Share=(Owned>=2||(SeenHP>=0&&SeenHP<=Strategy.BaseLowHP))?Strategy.BaseShareHigh:Strategy.BaseShareLow;
 }
 const double Slot=FMath::Frac(double(SquadID)*0.6180339887498949);
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
	for (const auto& Pair : Store->GetSquads()) if (Pair.Value.Team == Team)
	{
		// Public objective status only; never use an unseen enemy's location.
  bool bStrategyBase=false;
		const int32 Target = bStrategy ? AreaStrategyTarget(Pair.Key,Team,bStrategyBase) : ChooseObjective(Group, Team);
  ++Group;
		// Keep assignment stable when only some squads are delegated.
		if (bDelegatedOnly && !Pair.Value.bPlayerDelegated) continue;
  const bool BaseAssault=bStrategy?bStrategyBase:(bArea&&Group%5==0);
  const FVector Center=BaseAssault?AreaHome(1-Team)*(25./27.):Objectives[Target].Position;
  const EBattleOrder Order=!BaseAssault&&ObservedOwner(Team,Target)==Team?EBattleOrder::Defend:EBattleOrder::AttackMove;
  if(auto* Leader=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); Leader->IsWideField())
  {
   const auto& S=Pair.Value;
   if(S.CommandStatus!=EBattleCommandStatus::Active) continue;
   if(!S.bHasMission || S.MissionOrder!=Order || !S.MissionDestination.Equals(Center,20)) Leader->IssueOrder(Pair.Key,Order,Center,true);
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

void UBattleMatch::Finish(int32 WinningTeam, const FString& Reason)
{
	if (Phase != EBattleMatchPhase::Combat) return;
	Phase = EBattleMatchPhase::Finished; Winner = WinningTeam;
	if (auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
	{
		Mode->bBattleEnded = true; Mode->Result = Reason;
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->SavedAI) Unit->SavedAI->StopMovement();
	}
}
