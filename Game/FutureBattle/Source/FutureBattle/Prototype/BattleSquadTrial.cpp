#include "BattleSquadTrial.h"
#include "BattleFieldLayout.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleTactics.h"
#include "MassEntitySubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

void UBattleSquadTrial::Start(ABattleGameMode& Mode, const FString& Variant)
{
	Name = Variant;
 FParse::Value(FCommandLine::Get(),TEXT("BattleFieldRecord="),RecordFile);
 FParse::Value(FCommandLine::Get(),TEXT("BattleFieldReplay="),ReplayFile);
	bFieldDiagnostics = FParse::Param(FCommandLine::Get(),TEXT("BattleFieldDiagnostics"));
	bMoving = FParse::Value(FCommandLine::Get(), TEXT("BattleMovingCapture="), Capture);
	bField = FParse::Value(FCommandLine::Get(), TEXT("BattleFieldSeed="), Seed);
	FParse::Value(FCommandLine::Get(),TEXT("T2a-Allocation="),T2aAllocation);
 bT2aMetrics=FParse::Param(FCommandLine::Get(),TEXT("T2a-Metrics"));
 bT2aHeadless=FParse::Param(FCommandLine::Get(),TEXT("T2a-Headless"));
 if(T2aAllocation!=TEXT("Off") && T2aAllocation!=TEXT("Cap1") && T2aAllocation!=TEXT("Cap2") && T2aAllocation!=TEXT("Expected")) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
 bT2aMetrics |= T2aAllocation!=TEXT("Off") || bT2aHeadless;
 if(bT2aMetrics) bFieldDiagnostics=true;
 FRandomStream Layout(Seed);
	int32 Count = 1000;
	FParse::Value(FCommandLine::Get(), TEXT("BattleUnits="), Count);
	if ((bField && !bMoving) || (bMoving && !bField && Count != 1000) || (Count != 500 && Count != 1000) || (Name != TEXT("Actors") && Name != TEXT("Individual") && Name != TEXT("Squad")))
	{
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
 if((!RecordFile.IsEmpty() || !ReplayFile.IsEmpty()) && (!bField || Count!=500 || Name==TEXT("Actors"))) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
	if(bT2aMetrics && (!bField || Count!=500 || !ReplayFile.IsEmpty())) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
 if(!ConfigureT2b(Count)) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
 UWorld* World = Mode.GetWorld();
	auto* State = World->GetSubsystem<UBattleState>();
	Manager = World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager().AsShared();
	auto Archetype = Manager->CreateArchetype({ FBattleTrialSoldier::StaticStruct() });
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Team = 0; Team < 2; ++Team)
	{
		TArray<int32> Squad;
		for (int32 I = 0; I < Count / 2; ++I)
		{
			const FVector P = bField && BattleFieldLayout::IsLarge(Mode.GetWorld()) ? BattleFieldLayout::Spawn(Team,I)+FVector(Layout.FRandRange(-20,20),Layout.FRandRange(-35,35),0) : bField ? FVector((Team ? 1 : -1) * (2000 + I/40 * 100 + Layout.FRandRange(-20,20)), (I%40-19.5)*125 + Layout.FRandRange(-35,35), 95) : bMoving ? MovingStart(Team, I) : FVector(Team ? 900 : -900, I * 120., 1500);
			auto& Member = Members.AddDefaulted_GetRef();
			Member.Start = P;
			Member.ID = State->RegisterUnit(Team, bMoving ? 100 : 10000, bField && FParse::Param(FCommandLine::Get(), TEXT("BattleWeaponLoadouts")) ? WeaponForSlot(I) : EBattleWeapon::Carbine);
			if (Name == TEXT("Actors") || I % 10 == 0)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Member.Actor = World->SpawnActor<ABattleUnit>(P, FRotator(0, Team ? 180 : 0, 0), Params);
				Member.Actor->InitializeUnit(Member.ID);
				Member.Actor->GetCharacterMovement()->DisableMovement();
				Mode.Units.Add(Member.Actor);
			}
			else
			{
				Member.Entity = Manager->CreateEntity(Archetype);
				auto& Row = Manager->GetFragmentDataChecked<FBattleTrialSoldier>(Member.Entity);
				Row.UnitID = Member.ID;
				Row.Position = P;
			}
			Squad.Add(Member.ID);
			if (Squad.Num() == 5) { State->CreateSquad(Team, Squad); Squad.Reset(); }
			if (!bField && Team == 0 && I % (bMoving ? 5 : 20) == 0)
			{
				// Hide the first two lanes of every fourth squad.
				auto* Wall = World->SpawnActor<AStaticMeshActor>(bMoving ? P + FVector(350, 156, 0) : FVector(0, P.Y + 60, P.Z), FRotator::ZeroRotator);
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Wall->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
				Wall->SetActorScale3D(FVector(1, bMoving ? 4.4 : 2., 4));
			}
		}
	}
	if (bMoving) SetupMovingView(Mode);
 if(!ReplayFile.IsEmpty() && !StartReplay(Mode)) { Members.Reset(); FPlatformMisc::RequestExitWithStatus(false,1); }
}

FVector UBattleSquadTrial::Position(const FMember& Member) const
{
	return Member.Actor ? Member.Actor->GetActorLocation() : Manager->GetFragmentDataChecked<FBattleTrialSoldier>(Member.Entity).Position;
}

void UBattleSquadTrial::Step(ABattleGameMode& Mode)
{
	if (Members.IsEmpty()) return;
 if(!ReplayFile.IsEmpty()) { StepReplay(Mode); return; }
	if (bField) { StepField(Mode); return; }
	if (bMoving) { StepMoving(Mode); return; }
	if (Frame > 600) return;
	const double WallTime = FPlatformTime::Seconds();
	if (Frame > 0) Frames.Add((WallTime - LastFrame) * 1000);
	LastFrame = WallTime;
	if (Frame == -30)
	{
		if (auto* PC = Cast<ABattleController>(Mode.GetWorld()->GetFirstPlayerController()))
		{
			bPilot = PC->ControlUnit(Members[0].Actor) && PC->GetPawn() == Members[0].Actor;
			PC->EnterTactical();
			bPilot &= Members[0].Actor->SavedAI && Members[0].Actor->GetController() == Members[0].Actor->SavedAI;
			// Possession restarts CharacterMovement. Restore the stationary fixture.
			Members[0].Actor->GetCharacterMovement()->DisableMovement();
			Members[0].Actor->SetActorLocation(FVector(-900, 0, 1500));
		}
	}
	if (Frame < 0) { ++Frame; return; }
	auto* State = Mode.GetWorld()->GetSubsystem<UBattleState>();
	const int32 Half = Members.Num() / 2;
	if (Frame == 600)
	{
		double Sum = 0;
		for (double Sample : Frames) Sum += Sample;
		Frames.Sort();
		int32 Debits = 0, Damage = 0, Entities = 0, BlockedShots = 0;
		for (int32 I = 0; I < Members.Num(); ++I)
		{
			const auto& M = Members[I];
			const auto* S = State->Find(M.ID);
			Debits += GetDefault<UBattleTuning>()->Magazine - S->Ammo;
			Damage += FMath::RoundToInt(10000 - S->HP);
			if (I % Half % 20 < 2) BlockedShots += S->ShotSequence;
			if (!M.Actor && Manager->GetFragmentDataChecked<FBattleTrialSoldier>(M.Entity).UnitID == M.ID) ++Entities;
		}
		const int32 ExpectedActors = Name == TEXT("Actors") ? Members.Num() : Members.Num()/10;
		const bool Ok = bPilot && Shots > 0 && Hits > 0 && Debits == Shots && Damage == Hits * 10 && !BlockedShots
			&& Mode.Units.Num() == ExpectedActors && Entities == Members.Num() - ExpectedActors && State->GetSquads().Num() == Members.Num()/5;
		UE_LOG(LogTemp, Display, TEXT("SQUAD_TRIAL mode=%s actors=%d mass=%d squads=%d rays=%d shots=%d hits=%d blockedShots=%d pilot=%d meanMS=%.4f p95MS=%.4f success=%d"),
			*Name, Mode.Units.Num(), Entities, State->GetSquads().Num(), Rays, Shots, Hits, BlockedShots, bPilot, Sum/Frames.Num(), Frames[569], Ok);
		++Frame;
		FPlatformMisc::RequestExitWithStatus(false, Ok ? 0 : 1);
		return;
	}
	// One fixed opposing candidate per lane. Only the sensor may consult its location.
	const double Now = Frame / 60.;
	auto Visible = [&](int32 Index)
	{
		++Rays;
		const FVector A = Position(Members[Index]), B = Position(Members[(Index + Half) % Members.Num()]);
		return !Mode.GetWorld()->LineTraceTestByObjectType(A, B, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams::DefaultQueryParam);
	};
	for (int32 Base = 0; Base < Members.Num(); Base += 5)
	{
		if (Frame % 12 != (Base / 5) % 12) continue;
		// A clear representative opens the squad contact. Each actual shot still checks its own lane.
		const bool Shared = Name == TEXT("Squad");
		const bool Contact = Shared && (Visible(Base + 2) || Visible(Base) || Visible(Base + 4) || Visible(Base + 1) || Visible(Base + 3));
		for (int32 Slot = 0; Slot < 5; ++Slot)
		{
			const int32 I = Base + Slot;
			auto& M = Members[I];
			const bool Seen = Shared && !M.Actor ? Contact : Visible(I);
			M.Seen = Seen ? M.Seen + 1 : 0;
			if (M.Seen < 4 || Now < State->Find(M.ID)->NextShotTime) continue;
			if (Shared && !M.Actor && !Visible(I)) continue;
			if (!State->ConsumeShot(M.ID, Now, GetDefault<UBattleTuning>()->AIShotInterval)) continue;
			++Shots;
			const auto& Target = Members[(I + Half) % Members.Num()];
			const FVector Delta = Position(Target) - Position(M);
			// Identical standing spread and analytic 30 cm target in all three arms.
			auto Shooter = *State->Find(M.ID);
			Shooter.bMovingShot = false;
			Shooter.AimStableSince = -1;
			const FVector Direction = BattleTactics::ShotDirection(Shooter, Delta, Now);
			if (FVector::CrossProduct(Delta, Direction).SizeSquared() <= 900 &&
				State->ApplyDamage(Target.ID, Shooter.Team, 10)) ++Hits;
		}
	}
	++Frame;
}

void UBattleSquadTrial::Stop()
{
	for (const auto& M : Members) if (M.Entity.IsSet()) Manager->DestroyEntity(M.Entity);
	Members.Reset();
	Manager.Reset();
}
