#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleTactics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

void ABattleGameMode::RunTacticsSmokeTest()
{
	bCombatPaused = true;
	ABattleUnit* A = Units[8]; ABattleUnit* B = Units[9]; ABattleUnit* Victim = Units[17];
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto* Tuning = GetDefault<UBattleTuning>();
	const double T = GetWorld()->GetTimeSeconds();
	for (ABattleUnit* Unit : Units)
	{
		if (Unit->SavedAI) { Unit->SavedAI->StopMovement(); Unit->SavedAI->SetActorTickEnabled(false); }
		Unit->GetCharacterMovement()->StopMovementImmediately();
		if (Unit != A && Unit != B && Unit != Victim) Unit->SetActorEnableCollision(false);
	}
	for (ABattleUnit* Unit : {A, B, Victim})
	{
		Unit->SetActorEnableCollision(true); Unit->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	Store->UpdateAim(A->GetUnitID(), T, 300.f, FVector::ForwardVector, true);
	const float Moving = BattleTactics::SpreadDegrees(*A->GetState(), T);
	Store->UpdateAim(A->GetUnitID(), T + .1, 0.f, FVector::ForwardVector, true);
	const float Standing = BattleTactics::SpreadDegrees(*A->GetState(), T + .1);
	Store->UpdateAim(A->GetUnitID(), T + 1.1, 0.f, FVector::ForwardVector, true);
	const float Settled = BattleTactics::SpreadDegrees(*A->GetState(), T + 1.1);
	CheckSmoke(Settled < Standing && Standing < Moving, TEXT("Stationary aim reduces spread only after settle time"));
	Store->UpdateAim(A->GetUnitID(), T + 1.2, 0.f, FRotator(0, 5, 0).Vector(), true);
	CheckSmoke(BattleTactics::SpreadDegrees(*A->GetState(), T + 1.2) == Standing, TEXT("Turning aim beyond threshold cancels settled bonus"));
	Store->UpdateAim(A->GetUnitID(), T + 2.3, 0.f, FRotator(0, 5, 0).Vector(), true);
	Store->UpdateAim(A->GetUnitID(), T + 2.4, 0.f, FRotator(0, 5, 0).Vector(), false);
	CheckSmoke(A->GetState()->AimStableSince < 0. && BattleTactics::SpreadDegrees(*A->GetState(), T + 2.4) == Moving, TEXT("Airborne state cancels steady aim and applies movement spread"));
	Store->UpdateAim(A->GetUnitID(), T - 2., 0.f, FVector::ForwardVector, true);
	ABattleController* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	const double StableBefore = A->GetState()->AimStableSince;
	PC->ControlUnit(A); PC->EnterTactical();
	CheckSmoke(A->GetState()->AimStableSince == StableBefore && FMath::IsNearlyEqual(BattleTactics::SpreadDegrees(*A->GetState(), T), Settled),
		TEXT("AI/FPS ownership switch preserves aim effect under identical conditions"));
	const float SightBefore = A->GetState()->Perception.SightRange;
	Store->SetSpreadSkill(A->GetUnitID(), .5f);
	CheckSmoke(BattleTactics::SpreadDegrees(*A->GetState(), T) < Settled && A->GetState()->Perception.SightRange == SightBefore,
		TEXT("Shooting skill scales spread independently of perception ability"));
	Store->SetSpreadSkill(A->GetUnitID(), 1.f);
	Store->ConsumeShot(A->GetUnitID(), T - 4., .1);
	Store->StartReload(A->GetUnitID(), T);
	Store->UpdateAim(A->GetUnitID(), T + .1, 0.f, FVector::ForwardVector, true);
	CheckSmoke(A->GetState()->AimStableSince < 0., TEXT("Reload interrupts aim accumulation"));
	Store->AdvanceReloads(T + 10.);

	FBattleUnitState Sample = *A->GetState();
	Sample.AimStableSince = T - 2.; Sample.bMovingShot = false;
	int32 SettledHits = 0, MovingHits = 0; bool bConeBounded = true;
	FVector Average = FVector::ZeroVector;
	for (int32 Shot = 1; Shot <= 1024; ++Shot)
	{
		Sample.ShotSequence = Shot; Sample.bMovingShot = false;
		const FVector StableRay = BattleTactics::ShotDirection(Sample, FVector::ForwardVector, T);
		SettledHits += StableRay.X > FMath::Cos(FMath::DegreesToRadians(.5f));
		Sample.bMovingShot = true;
		const FVector MovingRay = BattleTactics::ShotDirection(Sample, FVector::ForwardVector, T);
		MovingHits += MovingRay.X > FMath::Cos(FMath::DegreesToRadians(.5f));
		bConeBounded &= MovingRay.X >= FMath::Cos(FMath::DegreesToRadians(Moving)) - 1.e-5f;
		Average += MovingRay;
	}
	CheckSmoke(SettledHits > MovingHits * 5 && SettledHits > 900, TEXT("Fixed-seed 1024-shot comparison improves angular target hits with steady aim"));
	CheckSmoke(bConeBounded && FMath::Abs(Average.Y / 1024.f) < .01f && FMath::Abs(Average.Z / 1024.f) < .01f,
		TEXT("Spread stays inside configured cone without consistent lateral or vertical bias"));
	CheckSmoke(BattleTactics::ShotDirection(Sample, FVector::ForwardVector, T).Equals(BattleTactics::ShotDirection(Sample, FVector::ForwardVector, T)), TEXT("Identical unit and shot sequence reproduce deterministic spread"));

	A->SetActorLocation(FVector(-2200, -2100, 95), false, nullptr, ETeleportType::TeleportPhysics);
	B->SetActorLocation(FVector(-1200, -1300, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Victim->SetActorLocation(FVector(-1200, -2100, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Victim->SetActorRotation(FRotator(0, 180, 0));
	AStaticMeshActor* Cover = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-1700, -2100, 150), FRotator::ZeroRotator);
	Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Cover->SetActorScale3D(FVector(1, 4, 3)); Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	const auto Front = BattleTactics::ExposureFrom(Victim, A->EyePosition());
	const auto Side = BattleTactics::ExposureFrom(Victim, B->EyePosition());
	CheckSmoke(Front.VisibleSamples == 0 && Front.HasCover() && !Front.bFlanked, TEXT("Front cover blocks all five exposure samples"));
	CheckSmoke(Side.VisibleSamples == 5 && !Side.HasCover() && Side.bFlanked, TEXT("Side approach bypasses directional cover and satisfies flank condition"));
	const float HPBefore = Victim->GetState()->HP;
	Store->UpdateAim(A->GetUnitID(), T - 2., 0.f, FVector::ForwardVector, true);
	CheckSmoke(A->FireAt(A->EyePosition(), Victim->EyePosition() - A->EyePosition(), false) && Victim->GetState()->HP == HPBefore,
		TEXT("AI shot cannot penetrate real cover despite aim bonus"));
	const FVector SideAim = (Victim->EyePosition() - B->EyePosition()).GetSafeNormal();
	Store->UpdateAim(B->GetUnitID(), T - 2., 0.f, SideAim, true);
	CheckSmoke(B->FireAt(B->EyePosition(), SideAim, true) && FMath::IsNearlyEqual(Victim->GetState()->HP, HPBefore - Tuning->ShotDamage),
		TEXT("Direct-control side shot bypasses cover with base damage, without double-counting flank bonuses"));
	Cover->SetActorLocation(FVector(-1700, -2100, 60)); Cover->SetActorScale3D(FVector(1, 4, 1.2));
	const auto Low = BattleTactics::ExposureFrom(Victim, A->EyePosition());
	CheckSmoke(Low.VisibleSamples > 0 && Low.VisibleSamples < 5, TEXT("Low cover reduces exposed samples without acting like a full-height wall"));
	Cover->Destroy();
	A->SetActorLocation(FVector(-1200, -1300, 95), false, nullptr, ETeleportType::TeleportPhysics);
	B->SetActorEnableCollision(false);
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, A, Victim, HPBefore]()
	{
		const FVector Aim = (Victim->EyePosition() - A->EyePosition()).GetSafeNormal();
		GetWorld()->GetSubsystem<UBattleState>()->UpdateAim(A->GetUnitID(), GetWorld()->GetTimeSeconds() - 2., 0.f, Aim, true);
		CheckSmoke(A->FireAt(A->EyePosition(), Aim, false) && FMath::IsNearlyEqual(Victim->GetState()->HP, HPBefore - 2.f * GetDefault<UBattleTuning>()->ShotDamage),
			TEXT("AI side shot uses the same physical exposure and base damage as direct control"));
		RunTacticalSmokeTest();
	}), 1.6f, false);
}
