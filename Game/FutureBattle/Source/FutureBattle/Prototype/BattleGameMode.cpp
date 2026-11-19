#include "BattleGameMode.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleMassTrial.h"
#include "BattleSquadTrial.h"
#include "BattleMoveCourse.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleHUD.h"
#include "BattleState.h"
#include "BattlePerception.h"
#include "BattleSquadAI.h"
#include "BattleSquadTactics.h"
#include "BattleMetrics.h"
#include "BattleMatch.h"
#include "FutureBattle.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ABattleUnit* ABattleGameMode::FindUnitByID(int32 ID) const
{
	if (ID <= 0) return nullptr;
	// Normal spawning is contiguous. Verify identity before using the direct slot;
	// saves and future removals may leave gaps or reorder the actor array.
	if (Units.IsValidIndex(ID - 1))
		if (ABattleUnit* Unit = Units[ID - 1]; IsValid(Unit) && Unit->GetUnitID() == ID) return Unit;
	for (ABattleUnit* Unit : Units) if (IsValid(Unit) && Unit->GetUnitID() == ID) return Unit;
	return nullptr;
}

ABattleGameMode::ABattleGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ABattleController::StaticClass();
	HUDClass = ABattleHUD::StaticClass();
}

void ABattleGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// The controller owns a tactical camera; no template pawn is spawned.
}

void ABattleGameMode::BeginPlay()
{
	Super::BeginPlay();
 int32 LeaderOption=0; FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderCommand="),LeaderOption);
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleWideMixed")) || BattleFieldLayout::IsSightField(GetWorld())) LeaderOption=1;
 if(LeaderOption!=0)
 {
  bLeaderCommand=true;
  if(LeaderOption!=1 || !GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Start(*this))
  { UE_LOG(LogFutureBattle,Error,TEXT("BATTLE_LEADER_REJECTED incompatible mode or formation")); FPlatformMisc::RequestExitWithStatus(false,1); }
  return;
 }
	FString TrialMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("BattleSquadTrial="), TrialMode))
	{
		bCombatPaused = true;
		SquadTrial = NewObject<UBattleSquadTrial>(this);
		SquadTrial->Start(*this, TrialMode);
		return;
	}
	ConfigureBenchmark();
	const bool bFastMatch = !bBenchmarkMode && FParse::Param(FCommandLine::Get(), TEXT("BattleFastMatch"));
	if (!BenchmarkCourse.IsEmpty())
	{
		bCombatPaused = true;
		MoveCourse = NewObject<UBattleMoveCourse>(this);
		if (BenchmarkCourse == TEXT("Gate")) UBattleMoveCourse::BuildGates(*GetWorld());
	}
	bScaleTest = !bBenchmarkMode && !bFastMatch && FParse::Value(FCommandLine::Get(), TEXT("BattleScaleUnits="), ScaleUnitCount) && (ScaleUnitCount == 100 || ScaleUnitCount == 500);
	if (BattleFieldLayout::IsLarge(GetWorld())) { bScaleTest=true; ScaleUnitCount=500; }
	if (bScaleTest) bCombatPaused = true;
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	const int32 PerTeam = bFastMatch ? 0 : bBenchmarkMode ? BenchmarkUnitCount / 2 : InitialUnitCount() / 2;
	const int32 Rows = PerTeam == 500 ? 40 : PerTeam > 50 ? 25 : PerTeam > 10 ? 10 : 5;
	const float RowSpacing = PerTeam > 50 ? 140.f : 300.f, ColumnSpacing = PerTeam > 50 ? 140.f : 260.f;
	FRandomStream Layout(BenchmarkSeed);
	for (int32 Team = 0; Team < 2; ++Team)
		for (int32 Index = 0; Index < PerTeam; ++Index)
		{
			float X = Team == 0 ? -2000.f - (Index / Rows) * ColumnSpacing : 2000.f + (Index / Rows) * ColumnSpacing;
			float Y = (Index % Rows - (Rows - 1) * .5f) * RowSpacing;
			if (bBenchmarkMode) { X += Layout.FRandRange(-25, 25); Y += Layout.FRandRange(-25, 25); }
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FVector Start = BattleFieldLayout::IsLarge(GetWorld()) ? BattleFieldLayout::Spawn(Team,Index) : MoveCourse ? UBattleMoveCourse::SpawnPosition(Team, Index, PerTeam) : FVector(X, Y, 100);
			UClass* UnitClass = bMassMovement ? ABattleMassTrialUnit::StaticClass() : ABattleUnit::StaticClass();
			ABattleUnit* Unit = GetWorld()->SpawnActor<ABattleUnit>(UnitClass, Start, FRotator(0, Team ? 180 : 0, 0), Spawn);
			if (!Unit) continue;
			Unit->InitializeUnit(Store->RegisterUnit(Team, bBenchmarkStableLoad ? 10000.f : InitialUnitHealth(), !bBenchmarkMode && GetNetMode()==NM_Standalone ? WeaponForSlot(Index) : EBattleWeapon::Carbine));
			Units.Add(Unit);
			if (bBenchmarkMode)
			{
				Store->SetTrainingPreset(Unit->GetUnitID(), static_cast<EBattleTraining>(Team ? BenchmarkOpponentTraining : BenchmarkTraining));
				if (MoveCourse) Store->SetOrder(Unit->GetUnitID(), EBattleOrder::Move, FVector(Start.X, Start.Y + 3000., 0));
				else Store->SetOrder(Unit->GetUnitID(), EBattleOrder::AttackMove, FVector((Team ? 1.f : -1.f) * (350 + (Index / Rows) * 100), Y * .75f, 0));
			}
			else if (Team == 1 && BattleFieldLayout::IsLarge(GetWorld())) Store->SetOrder(Unit->GetUnitID(),EBattleOrder::AttackMove,BattleFieldLayout::Goal(Index));
			else if (Team == 1) Store->SetOrder(Unit->GetUnitID(), bScaleTest ? EBattleOrder::AttackMove : EBattleOrder::Move,
				FVector(bScaleTest ? 350 + (Index / Rows) * 100 : 350 + (Index / 5) * 250, bScaleTest ? Y * .75f : Y, 0));
		}
	if (Units.Num() == PerTeam * 2) for (int32 Team = 0; Team < 2; ++Team)
		for (int32 Group = 0; Group < PerTeam / 5; ++Group)
		{
			TArray<int32> Members;
			TMap<int32, FVector> Goals;
			for (int32 Index = 0; Index < 5; ++Index)
			{
				ABattleUnit* Unit = Units[Team * PerTeam + Group * 5 + Index];
				Members.Add(Unit->GetUnitID());
				Goals.Add(Unit->GetUnitID(), Team || bBenchmarkMode ? Unit->GetState()->Destination : Unit->GetActorLocation());
			}
			const int32 SquadID = Store->CreateSquad(Team, Members);
			const auto Order = MoveCourse ? EBattleOrder::Move : bBenchmarkMode || (bScaleTest && Team) ? EBattleOrder::AttackMove : Team ? EBattleOrder::Move : EBattleOrder::Hold;
			Store->SetSquadOrder(SquadID, Order, Goals.FindChecked(Members[2]), Goals);
		}
	CombatStartsAt = GetWorld()->GetTimeSeconds() + (bBenchmarkMode ? 3. : 8.);
	if (bFastMatch)
	{
		auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
		Match->InitializeMatch(); bCombatPaused = true;
		// Navigation is ready after the map has completed its first frames.
		FTimerHandle DeploymentTimer;
		GetWorldTimerManager().SetTimer(DeploymentTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
			if (Match->Phase == EBattleMatchPhase::Deployment && Units.IsEmpty()) { Match->HireSquad(1, EBattleTraining::Regular); Match->HireSquad(1, EBattleTraining::Regular); }
		}), 1.f, false);
	}
	if (BattleFieldLayout::IsLarge(GetWorld())) { auto* M=GetWorld()->GetSubsystem<UBattleMatch>(); M->InitializeMatch(); M->Phase=EBattleMatchPhase::Combat; }
	RefreshVisibility();
	if (ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController()))
		if (!Units.IsEmpty()) PC->SetSelection(Units[0]);
	bSmokeMode = FParse::Param(FCommandLine::Get(), TEXT("BattleSmokeTest"));
	if (BattleFieldLayout::IsLarge(GetWorld()) && FParse::Param(FCommandLine::Get(),TEXT("BattleLargeFieldTest")))
	{
		FTimerHandle Test; GetWorldTimerManager().SetTimer(Test,this,&ABattleGameMode::RunLargeFieldTest,4.f,false);
	}
	if (bScaleTest && FParse::Param(FCommandLine::Get(), TEXT("BattleScaleSmoke")))
	{
		FTimerHandle ScaleTimer; GetWorldTimerManager().SetTimer(ScaleTimer, this, &ABattleGameMode::RunScaleSmokeTest, 3.f, false);
	}
	if (bSmokeMode)
	{
		bCombatPaused = true;
		FTimerHandle Timer;
		FString Suite;
		FParse::Value(FCommandLine::Get(), TEXT("BattleSmokeSuite="), Suite);
		GetWorldTimerManager().SetTimer(Timer, this, Suite == TEXT("SquadTactics") ? &ABattleGameMode::RunSquadTacticsSmokeTest : Suite == TEXT("Delegation") ? &ABattleGameMode::RunDelegationSmokeTest : Suite == TEXT("TPS") ? &ABattleGameMode::RunThirdPersonSmokeTest : Suite == TEXT("Weapons") ? &ABattleGameMode::RunWeaponPreview : Suite == TEXT("Corner") ? &ABattleGameMode::RunCornerTrial : Suite == TEXT("Match") ? &ABattleGameMode::RunMatchSmokeTest : Suite == TEXT("Ability") ? &ABattleGameMode::RunAbilitySmokeTest : Suite == TEXT("Squad") ? &ABattleGameMode::RunSquadSmokeTest : Suite == TEXT("Tactical") ? &ABattleGameMode::RunTacticalSmokeTest : &ABattleGameMode::RunSmokeTest, 3.f, false);
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_READY Units=%d"), Units.Num());
}

void ABattleGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (SquadTrial) SquadTrial->Stop();
	if (MassTrial) MassTrial->Stop();
	Super::EndPlay(Reason);
}

void ABattleGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(bLeaderCommand) { GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->Step(DeltaTime); return; }
	if (SquadTrial) { SquadTrial->Step(*this); return; }
	GetWorld()->GetSubsystem<UBattleMetrics>()->BeginFrame();
	if (MassTrial) MassTrial->Update(DeltaTime);
	if (MoveCourse && bBenchmarkStarted && !bBenchmarkFinished) MoveCourse->Update(*this, DeltaTime);
	UpdateBenchmark();
	GetWorld()->GetSubsystem<UBattleMatch>()->Update(DeltaTime);
 GetWorld()->GetSubsystem<UBattleSquadTactics>()->Step(DeltaTime);
	UpdateMatchSoak();
	const double Now = GetWorld()->GetTimeSeconds();
	GetWorld()->GetSubsystem<UBattleState>()->AdvanceReloads(Now);
	if (GetWorld()->GetSubsystem<UBattlePerception>()->bEvenSchedule) RefreshVisibility(-1, 0);
	else
	{
		const int32 Slices = FMath::Clamp(GetDefault<UBattleTuning>()->PerceptionSlices, 1, 16);
		if (NextVisibilityUpdate <= 0.) NextVisibilityUpdate = Now;
		int32 UpdatedSlices = 0;
		while (Now >= NextVisibilityUpdate && UpdatedSlices++ < Slices)
		{
			RefreshVisibility(VisibilitySlice % Slices, Slices); ++VisibilitySlice;
			NextVisibilityUpdate += .2 / Slices;
		}
		if (Now - NextVisibilityUpdate > .2) NextVisibilityUpdate = Now + .2 / Slices;
	}
	if (!bCombatPaused && !bBattleEnded && Now >= CombatStartsAt) GetWorld()->GetSubsystem<UBattleSquadAI>()->Update(Units, Now);
	int32 Allies = 0, Enemies = 0;
	for (ABattleUnit* Unit : Units) if (Unit && Unit->IsAlive()) { if (Unit->GetState()->Team == 0) ++Allies; else ++Enemies; }
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->Phase == EBattleMatchPhase::Combat && (Allies == 0 || Enemies == 0)) Match->Finish(Allies == 0 ? 1 : 0, Allies == 0 ? TEXT("DEFEAT / FORCE ELIMINATED") : TEXT("VICTORY / ENEMY ELIMINATED"));
	if (!Match->IsEnabled() && !bBattleEnded && !bSmokeMode && !bBenchmarkMode && Units.Num() > 0 && (Allies == 0 || Enemies == 0))
	{
		bBattleEnded = true;
		Result = Allies == 0 ? TEXT("SQUAD ELIMINATED") : TEXT("SECTOR SECURED");
		for (ABattleUnit* Unit : Units) if (Unit) if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->StopMovement();
	}
}

void ABattleGameMode::RefreshVisibility(int32 Slice, int32 SliceCount)
{
	LastVisibilityTime = GetWorld()->GetTimeSeconds();
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleVisibility);
	UBattlePerception* Perception = GetWorld()->GetSubsystem<UBattlePerception>();
	Perception->Update(Units, GetWorld()->GetTimeSeconds(), Slice, SliceCount);
	TSet<int32> VisibleTargets; Perception->CollectVisibleTargets(Units, 0, VisibleTargets);
	for (ABattleUnit* Enemy : Units)
	{
		if (!Enemy || Enemy->GetState()->Team == 0) continue;
		Enemy->SetRevealed(VisibleTargets.Contains(Enemy->GetUnitID()));
	}
}

void ABattleGameMode::CheckSmoke(bool bPass, const TCHAR* Description)
{
	if (!bPass) ++SmokeFailures;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_TEST %s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), Description);
}

void ABattleGameMode::RunSmokeTest()
{
	bCombatPaused = true;
	SmokeFailures = 0;
	ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	CheckSmoke(Units.Num() == 20, TEXT("20 units spawned"));
	if (Units.Num() != 20 || !PC) { CheckSmoke(false, TEXT("Controller and units available")); FinishSmokeTest(); return; }
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	ABattleUnit* A = Units[0]; ABattleUnit* B = Units[1]; ABattleUnit* Enemy = Units[10];
	TSet<int32> IDs; for (ABattleUnit* U : Units) IDs.Add(U->GetUnitID());
	CheckSmoke(IDs.Num() == 20 && Store->GetUnits().Num() == 20, TEXT("Unique authoritative UnitIDs"));
	bool bSquadsValid = Store->GetSquads().Num() == 4;
	for (const auto& Pair : Store->GetSquads())
	{
		bSquadsValid &= Pair.Value.Members.Num() == 5;
		for (int32 ID : Pair.Value.Members) bSquadsValid &= Store->Find(ID) && Store->Find(ID)->SquadID == Pair.Key && Store->Find(ID)->Team == Pair.Value.Team;
	}
	CheckSmoke(bSquadsValid, TEXT("Four persistent five-member squads with matching team membership"));
	const double Now = GetWorld()->GetTimeSeconds();
	// Hit direction (display only): hits on the watched unit are kept, with the source when the caller gives one.
	// The same 17 damage as before, in two hits (10 with a source, 7 without), so the rest of the test is unchanged.
	Store->HitWatchID = A->GetUnitID(); Store->HitNotices.Reset();
	const FVector HitFrom = A->GetActorLocation() + FVector(0, 1000, 0);
	Store->ApplyDamageFrom(A->GetUnitID(), 1, 10, HitFrom);
	Store->ApplyDamage(A->GetUnitID(), 1, 7);
	CheckSmoke(Store->HitNotices.Num() == 2 && Store->HitNotices[0].bKnown && Store->HitNotices[0].From.Equals(HitFrom) && !Store->HitNotices[1].bKnown, TEXT("Hits on the controlled unit keep their source"));
	Store->HitWatchID = INDEX_NONE; Store->HitNotices.Reset();
	Store->ConsumeShot(A->GetUnitID(), Now - 1., .1);
	Store->SetOrder(A->GetUnitID(), EBattleOrder::Move, FVector(-1400, -600, 0));
	const FBattleUnitState Before = *A->GetState();
	CheckSmoke(PC->ControlUnit(A), TEXT("Possess friendly A"));
	PC->EnterTactical(); PC->ControlUnit(B); PC->EnterTactical(); PC->ControlUnit(A);
	const FBattleUnitState After = *A->GetState();
	CheckSmoke(Before.UnitID == After.UnitID && Before.HP == After.HP && Before.Ammo == After.Ammo && Before.Reserve == After.Reserve && Before.Order == After.Order && Before.Destination == After.Destination, TEXT("A-RTS-B-RTS-A preserves ID HP ammo reserve and destination"));
	CheckSmoke(!PC->ControlUnit(Enemy), TEXT("Cannot possess enemy"));
	PC->EnterTactical();
	CheckSmoke(A->GetController() == A->SavedAI, TEXT("AI resumes ownership"));
	CheckSmoke(!Store->ApplyDamage(A->GetUnitID(), 0, 20) && A->GetState()->HP == Before.HP, TEXT("Friendly fire rejected"));
	CheckSmoke(Store->StartReload(A->GetUnitID(), Now), TEXT("Reload starts"));
	CheckSmoke(!Store->ConsumeShot(A->GetUnitID(), Now + .2, .1), TEXT("Cannot fire while reloading"));
	PC->ControlUnit(A); PC->EnterTactical();
	CheckSmoke(A->GetState()->ReloadEndTime > 0, TEXT("Reload survives control switch"));
	Store->AdvanceReloads(Now + 10);
	CheckSmoke(A->GetState()->Ammo == GetDefault<UBattleTuning>()->Magazine && A->GetState()->Reserve == Before.Reserve - 1, TEXT("Reload conserves ammunition"));

	// Isolated firing lane, away from the rest of the squad.
	A->SetActorLocation(FVector(-1800, -2100, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->SetActorLocation(FVector(-800, -2100, 95), false, nullptr, ETeleportType::TeleportPhysics);
	CheckSmoke(A->CanSee(Enemy), TEXT("Unobstructed enemy detected"));
	AStaticMeshActor* Cover = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-1300, -2100, 160), FRotator::ZeroRotator);
	Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Cover->SetActorScale3D(FVector(1, 4, 3.2));
	Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	CheckSmoke(!A->CanSee(Enemy), TEXT("Cover blocks AI sight"));
	const float EnemyHP = Enemy->GetState()->HP;
	CheckSmoke(A->FireAt(A->EyePosition(), Enemy->EyePosition() - A->EyePosition(), true), TEXT("Cover test actually fires a round"));
	CheckSmoke(Enemy->GetState()->HP == EnemyHP, TEXT("Cover blocks hitscan damage"));
	Cover->Destroy();
	// Different shooter avoids the previous shot cooldown.
	B->SetActorLocation(FVector(-1800, -1900, 95), false, nullptr, ETeleportType::TeleportPhysics);
	B->FireAt(B->EyePosition(), Enemy->GetActorLocation() + FVector(0, 0, 35) - B->EyePosition(), true);
	CheckSmoke(Enemy->GetState()->HP < EnemyHP, TEXT("Unobstructed hitscan damages enemy"));
	CheckSmoke(!B->FireAt(B->EyePosition(), Enemy->EyePosition() - B->EyePosition(), true), TEXT("Fire rate enforced"));
	Enemy->SetActorLocation(FVector(3000, 2100, 95), false, nullptr, ETeleportType::TeleportPhysics);
	RefreshVisibility(); CheckSmoke(!Enemy->bRevealed, TEXT("Unseen enemy hidden from tactical view"));
	// Navigation uses the actual command path, including projection and path validity.
	PC->SetSelection(A); SmokeStart = A->GetActorLocation();
	PC->IssueMoveAt(FVector(-1000, -2100, 0));
	bool bSquadMove = true;
	TMap<int32, FVector> MemberStarts;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		bSquadMove &= Units[Index]->GetState()->Order == EBattleOrder::Move;
		MemberStarts.Add(Index, Units[Index]->GetActorLocation());
	}
	CheckSmoke(bSquadMove && Units[5]->GetState()->Order == EBattleOrder::Hold, TEXT("One selected member commands only its five-member squad"));
	const FVector ValidGoal = A->GetState()->Destination;
	PC->IssueMoveAt(FVector(1000000, 1000000, 0));
	CheckSmoke(A->GetState()->Destination == ValidGoal, TEXT("Unreachable squad command preserves previous order"));
	PC->HoldSelected();
	bool bSquadHeld = true;
	for (int32 Index = 0; Index < 5; ++Index) bSquadHeld &= Units[Index]->GetState()->Order == EBattleOrder::Hold && Units[Index]->SavedAI->GetMoveStatus() == EPathFollowingStatus::Idle;
	CheckSmoke(bSquadHeld, TEXT("Hold stops every living member of selected squad"));
	PC->IssueMoveAt(FVector(-1000, -2100, 0));
	PC->ControlUnit(A);
	CheckSmoke(A->GetState()->Action == EBattleAction::DirectControl && Store->FindSquad(A->GetState()->SquadID)->Order == EBattleOrder::Move, TEXT("Direct control preserves squad move intent"));
	// Replace the order through another member while A is directly controlled.
	PC->SetSelection(B);
	PC->IssueMoveAt(FVector(-900, -2100, 0));
	CheckSmoke(A->GetState()->Destination.X > ValidGoal.X && A->GetState()->Action == EBattleAction::DirectControl, TEXT("Squad order updates directly controlled member without stealing control"));
	PC->EnterTactical();
	CombatStartsAt = 0; bCombatPaused = false;
	A->SavedAI->Tick(.3f);
	bCombatPaused = true;
	CheckSmoke(A->GetState()->Action == EBattleAction::Moving && A->SavedAI->GetMoveStatus() == EPathFollowingStatus::Moving, TEXT("AI reevaluates latest squad goal after FPS release"));
	CheckSmoke(A->GetState()->Order == EBattleOrder::Move && A->GetState()->Destination.X > -1200, TEXT("RTS issues reachable navigation order"));
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), SmokeStart, A->GetState()->Destination, A);
	CheckSmoke(Path && Path->IsValid() && !Path->IsPartial(), TEXT("NavMesh provides a complete path"));
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, MemberStarts]()
	{
		bool bAllMoved = true;
		for (const auto& Pair : MemberStarts) bAllMoved &= FVector::Dist2D(Units[Pair.Key]->GetActorLocation(), Pair.Value) > 100.f;
		CheckSmoke(bAllMoved, TEXT("All five squad members physically move including released FPS unit"));
		FinishSmokeTest();
	}), 3.f, false);
}

void ABattleGameMode::FinishSmokeTest()
{
	if (Units.Num() == 20)
	{
		ABattleUnit* A = Units[0]; ABattleUnit* Enemy = Units[10];
		CheckSmoke(FVector::Dist2D(A->GetActorLocation(), SmokeStart) > 200, TEXT("AI physically follows RTS move order"));
		UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
		// Arrival is an execution state, not cancellation of the persistent command.
		A->SetActorLocation(FVector(A->GetState()->Destination.X, A->GetState()->Destination.Y, A->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		bCombatPaused = false;
		A->SavedAI->Tick(.3f);
		bCombatPaused = true;
		CheckSmoke(A->GetState()->Action == EBattleAction::Idle && A->GetState()->Order == EBattleOrder::Move && Store->FindSquad(A->GetState()->SquadID)->Order == EBattleOrder::Move, TEXT("Arrival idles member without cancelling squad intent"));
		Store->ApplyDamage(A->GetUnitID(), 1, 10000); A->ApplyDeath();
		CheckSmoke(!A->IsAlive() && !Store->ConsumeShot(A->GetUnitID(), GetWorld()->GetTimeSeconds(), .1), TEXT("Dead units cannot fire"));
		if (ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) CheckSmoke(!PC->ControlUnit(A), TEXT("Dead units cannot be possessed"));
		CheckSmoke(Store->Find(A->GetUnitID()) != nullptr, TEXT("Dead unit keeps authoritative record"));
		// Observe AI target acquisition and firing with a real tick, not a direct FireAt call.
		Enemy->SetActorLocation(Units[1]->GetActorLocation() + FVector(900, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Store->SetOrder(Enemy->GetUnitID(), EBattleOrder::Hold, Enemy->GetActorLocation());
		CombatStartsAt = 0; bCombatPaused = false;
		const int32 AmmoBefore = Enemy->GetState()->Ammo;
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Enemy, AmmoBefore]()
		{
			CheckSmoke(Enemy->GetState()->Ammo < AmmoBefore, TEXT("AI acquires target and fires during world tick"));
			bCombatPaused = true;
			OrderSmokeStage = 0;
			RunOrderSmokeTest();
		}), 2.f, false);
	}
	else if (bSmokeMode) FPlatformMisc::RequestExitWithStatus(false, 1);
}
