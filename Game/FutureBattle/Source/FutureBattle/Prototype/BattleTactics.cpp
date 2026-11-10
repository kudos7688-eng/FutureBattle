#include "BattleTactics.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattleMetrics.h"

float BattleTactics::SpreadDegrees(const FBattleUnitState& State, double Now)
{
	const auto* Tuning = GetDefault<UBattleTuning>();
	const bool bSettled = State.AimStableSince >= 0. && Now - State.AimStableSince >= FMath::Max(0.f, Tuning->AimSettleSeconds);
	const float Base = State.bMovingShot ? Tuning->MovingSpreadDegrees : bSettled ? Tuning->SettledSpreadDegrees : Tuning->StandingSpreadDegrees;
	return FMath::Clamp(Base * State.SpreadSkillScale * (State.Weapon == EBattleWeapon::SupportGun && State.bMovingShot ? Tuning->SupportMovingSpreadScale : 1.f), .05f, 15.f);
}

FVector BattleTactics::ShotDirection(const FBattleUnitState& State, FVector Aim, double Now)
{
	// Match-local sequence remains in canonical state across possession changes.
	const uint32 Seed = static_cast<uint32>(State.UnitID) * 196613u + static_cast<uint32>(State.ShotSequence) * 314159u + 271828u;
	FRandomStream Random(static_cast<int32>(Seed));
	return Random.VRandCone(Aim.GetSafeNormal(), FMath::DegreesToRadians(SpreadDegrees(State, Now)));
}

FBattleExposure BattleTactics::ExposureFrom(const ABattleUnit* Target, FVector AttackOrigin)
{
	if (!Target || !Target->IsAlive()) return FBattleExposure();
	return ExposureAt(Target->GetWorld(), Target->GetActorLocation(), Target->GetActorForwardVector(), AttackOrigin);
}

bool BattleTactics::HasTerrainLine(UWorld* World, FVector From, FVector To)
{
	if (auto* Metrics = World->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->TerrainRays;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleTacticalLine), false);
	if (const auto* Mode = World->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit) Query.AddIgnoredActor(Unit);
	World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
	return !Hit.bBlockingHit;
}

FBattleExposure BattleTactics::ExposureAt(UWorld* World, FVector Position, FVector Forward, FVector AttackOrigin)
{
	FBattleExposure Result;
	const FVector TowardAttacker = (AttackOrigin - Position).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, TowardAttacker);
	const FVector Samples[] = { FVector(0, 0, 60), FVector(0, 0, 30), FVector(0, 0, -35), Right * 25.f + FVector(0, 0, 25), -Right * 25.f + FVector(0, 0, 25) };
	for (const FVector& Offset : Samples)
	{
		if (HasTerrainLine(World, Position + Offset, AttackOrigin)) ++Result.VisibleSamples;
	}
	Result.bFlanked = FVector::DotProduct(Forward.GetSafeNormal2D(), TowardAttacker) <
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(GetDefault<UBattleTuning>()->FlankAngleDegrees, 1.f, 179.f)));
	return Result;
}
