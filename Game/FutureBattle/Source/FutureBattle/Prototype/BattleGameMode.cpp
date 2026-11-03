#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleHUD.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	for (int32 Team = 0; Team < 2; ++Team)
		for (int32 Index = 0; Index < 10; ++Index)
		{
			const float X = Team == 0 ? -2000.f - (Index / 5) * 260.f : 2000.f + (Index / 5) * 260.f;
			const float Y = (Index % 5 - 2) * 300.f;
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			ABattleUnit* Unit = GetWorld()->SpawnActor<ABattleUnit>(FVector(X, Y, 100), FRotator(0, Team ? 180 : 0, 0), Spawn);
			if (!Unit) continue;
			Unit->InitializeUnit(Store->RegisterUnit(Team));
			Units.Add(Unit);
			if (Team == 1) Store->SetOrder(Unit->GetUnitID(), EBattleOrder::Move, FVector(350 + (Index / 5) * 250, Y, 0));
		}
	CombatStartsAt = GetWorld()->GetTimeSeconds() + 8.;
	RefreshVisibility();
	if (ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController()))
		if (!Units.IsEmpty()) PC->SetSelection(Units[0]);
	bSmokeMode = FParse::Param(FCommandLine::Get(), TEXT("BattleSmokeTest"));
	if (bSmokeMode)
	{
		bCombatPaused = true;
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, this, &ABattleGameMode::RunSmokeTest, 3.f, false);
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_READY Units=%d"), Units.Num());
}

void ABattleGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const double Now = GetWorld()->GetTimeSeconds();
	GetWorld()->GetSubsystem<UBattleState>()->AdvanceReloads(Now);
	if (Now >= NextVisibilityUpdate) { RefreshVisibility(); NextVisibilityUpdate = Now + .2; }
	int32 Allies = 0, Enemies = 0;
	for (ABattleUnit* Unit : Units) if (Unit && Unit->IsAlive()) { if (Unit->GetState()->Team == 0) ++Allies; else ++Enemies; }
	if (!bBattleEnded && !bSmokeMode && Units.Num() > 0 && (Allies == 0 || Enemies == 0))
	{
		bBattleEnded = true;
		Result = Allies == 0 ? TEXT("SQUAD ELIMINATED") : TEXT("SECTOR SECURED");
		for (ABattleUnit* Unit : Units) if (Unit) if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->StopMovement();
	}
}

void ABattleGameMode::RefreshVisibility()
{
	for (ABattleUnit* Enemy : Units)
	{
		if (!Enemy || Enemy->GetState()->Team == 0) continue;
		bool bVisible = false;
		for (ABattleUnit* Ally : Units)
			if (Ally && Ally->GetState()->Team == 0 && Ally->CanSee(Enemy)) { bVisible = true; break; }
		Enemy->SetRevealed(bVisible);
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
	const double Now = GetWorld()->GetTimeSeconds();
	Store->ApplyDamage(A->GetUnitID(), 1, 17);
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
	CheckSmoke(A->GetState()->Order == EBattleOrder::Move && A->GetState()->Destination.X > -1200, TEXT("RTS issues reachable navigation order"));
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), SmokeStart, A->GetState()->Destination, A);
	CheckSmoke(Path && Path->IsValid() && !Path->IsPartial(), TEXT("NavMesh provides a complete path"));
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, this, &ABattleGameMode::FinishSmokeTest, 3.f, false);
}

void ABattleGameMode::FinishSmokeTest()
{
	if (Units.Num() == 20)
	{
		ABattleUnit* A = Units[0]; ABattleUnit* Enemy = Units[10];
		CheckSmoke(FVector::Dist2D(A->GetActorLocation(), SmokeStart) > 200, TEXT("AI physically follows RTS move order"));
		UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
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
			UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SMOKE_RESULT failures=%d"), SmokeFailures);
			if (bSmokeMode) FPlatformMisc::RequestExitWithStatus(false, SmokeFailures ? 1 : 0);
			else if (ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController())) PC->ShowNotice(FString::Printf(TEXT("VERIFY: %d FAILURES / F5 TO RESET"), SmokeFailures));
		}), 2.f, false);
	}
	else if (bSmokeMode) FPlatformMisc::RequestExitWithStatus(false, 1);
}
