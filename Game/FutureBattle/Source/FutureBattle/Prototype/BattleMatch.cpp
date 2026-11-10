#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "NavigationSystem.h"
#include "BattleNavigation.h"

void UBattleMatch::InitializeMatch()
{
	if (IsEnabled()) return;
	MatchID = FGuid::NewGuid(); Phase = EBattleMatchPhase::Deployment;
	Credits[0] = Credits[1] = 3000; Winner = INDEX_NONE;
	for (const FVector Position : { FVector(-800, -1800, 0), FVector(0, 1800, 0), FVector(800, -1800, 0) })
	{
		FBattleObjective Point; Point.Position = BattleFieldLayout::IsLarge(GetWorld()) ? FVector(0,(Objectives.Num()-1)*4500,0) : Position; Objectives.Add(Point);
	}
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

bool UBattleMatch::HireSquad(int32 Team, EBattleTraining Training)
{
	LastMessage.Reset();
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

void UBattleMatch::Update(float DeltaSeconds)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (Phase != EBattleMatchPhase::Combat || !Mode || Mode->bCombatPaused || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	Elapsed += DeltaSeconds;
	if (!BattleFieldLayout::IsLarge(GetWorld())) UpdateSupport(DeltaSeconds);
	UpdateObjectives(DeltaSeconds);
	if (Phase != EBattleMatchPhase::Combat) return;
	DirectorClock += DeltaSeconds; SupplyClock += DeltaSeconds;
	if (DirectorClock >= 5.f)
	{
		DirectorClock = FMath::Fmod(DirectorClock, 5.f);
		int32 EnemyAlive = 0; for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 1) ++EnemyAlive;
		if (!BattleFieldLayout::IsLarge(GetWorld()) && EnemyAlive < 10) HireSquad(1, Credits[1] >= SquadPrice(EBattleTraining::Regular) ? EBattleTraining::Regular : EBattleTraining::Recruit);
		UpdateEnemyOrders();
		if (!BattleFieldLayout::IsLarge(GetWorld()) && Credits[1] >= 1300 && SupportCooldown[1] <= 0 && SupportUsed[1] < 3)
			for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 1 && RequestSupport(Unit->GetUnitID())) break;
	}
	if (!BattleFieldLayout::IsLarge(GetWorld()) && SupplyClock >= 1.f)
	{
		SupplyClock = FMath::Fmod(SupplyClock, 1.f);
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive()) Resupply(Unit->GetUnitID());
	}
	CheckTimeLimit();
}

void UBattleMatch::CheckTimeLimit()
{
	if (Elapsed >= 180.f)
	{
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
		if (Point.Owner != INDEX_NONE && Point.Owner == OwnerBefore && (Team == INDEX_NONE || Team == Point.Owner))
		{
			Score[Point.Owner] += DeltaSeconds;
			Point.IncomeClock += DeltaSeconds;
			const int32 Payments = FMath::FloorToInt(Point.IncomeClock / 5.f);
			if (Payments > 0) { Credits[Point.Owner] = FMath::Min(1000000, Credits[Point.Owner] + Payments * 20); Point.IncomeClock = FMath::Fmod(Point.IncomeClock, 5.f); }
		}
	}
	if (Score[0] >= 100 || Score[1] >= 100)
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
	for (int32 Offset = 0; Offset < Objectives.Num(); ++Offset)
	{
		const int32 Target = (Start + Offset) % Objectives.Num();
		if (Objectives[Target].Owner != Team) return Target;
	}
	return Start;
}

void UBattleMatch::UpdateTeamOrders(int32 Team)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Mode || (Team != 0 && Team != 1) || Phase != EBattleMatchPhase::Combat || Objectives.IsEmpty()) return;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	int32 Group = 0;
	for (const auto& Pair : Store->GetSquads()) if (Pair.Value.Team == Team)
	{
		// Public objective status only; never use an unseen enemy's location.
		const int32 Target = ChooseObjective(Group++, Team);
		const FVector Center = Objectives[Target].Position;
		const EBattleOrder Order = Objectives[Target].Owner == Team ? EBattleOrder::Defend : EBattleOrder::AttackMove;
		TMap<int32, FVector> Goals; bool bNeedsOrder = Pair.Value.Order != Order || !Pair.Value.Destination.Equals(Center, 20); bool bReachable = true;
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->SquadID == Pair.Key)
		{
			bNeedsOrder |= Unit->GetState()->Action == EBattleAction::MoveFailed;
			FVector Goal; const FVector Offset(0, (Goals.Num() - 2) * 100, 0);
			if (BattleNavigation::FormationGoal(Unit, Center, Offset, Goal)) Goals.Add(Unit->GetUnitID(), Goal); else bReachable = false;
		}
		if (bNeedsOrder && bReachable && !Goals.IsEmpty()) Store->SetSquadOrder(Pair.Key, Order, Center, Goals);
	}
}

int32 UBattleMatch::Resupply(int32 UnitID, int32 RequestedRounds)
{
	if (Phase != EBattleMatchPhase::Combat || RequestedRounds <= 0) return 0;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>(); const auto* S = Store->Find(UnitID);
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!S || !S->IsAlive() || !Mode || Mode->bCombatPaused) return 0;
	ABattleUnit* Unit = nullptr; for (ABattleUnit* Candidate : Mode->Units) if (Candidate && Candidate->GetUnitID() == UnitID) { Unit = Candidate; break; }
	if (!Unit) return 0;
	const FVector Position = Unit->GetActorLocation();
	bool bInSupply = FMath::Abs(Position.X - (S->Team ? 2600.f : -2600.f)) <= 800 && FMath::Abs(Position.Y) <= 2100 && FMath::Abs(Position.Z) < 250;
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
