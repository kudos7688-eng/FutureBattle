#include "BattleNavigation.h"
#include "BattleUnit.h"
#include "BattleMetrics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

bool BattleNavigation::ResolveGoal(ABattleUnit* Unit, const FVector& Requested, FVector& Resolved)
{
	if (!Unit || Requested.ContainsNaN()) return false;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Unit->GetWorld());
	FNavLocation Projected;
	if (!Nav || !Nav->ProjectPointToNavigation(Requested, Projected, FVector(100, 100, 400))) return false;
	if (auto* Metrics = Unit->GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->Paths;
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(Unit->GetWorld(), Unit->GetActorLocation(), Projected.Location, Unit);
	if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
	Resolved = Projected.Location;
	return true;
}

bool BattleNavigation::FormationGoal(ABattleUnit* Unit, const FVector& Center, const FVector& Offset, FVector& Resolved)
{
	// Relax destination spacing when a wall or narrow entrance blocks the nominal slot.
	for (float Scale : {1.f, .5f, 0.f})
		if (ResolveGoal(Unit, Center + Offset * Scale, Resolved)) return true;
	return false;
}

bool BattleNavigation::IsPathWithinDefense(ABattleUnit* Unit, const FVector& Goal)
{
	const auto* State = Unit->GetState();
	if (State->Order != EBattleOrder::Defend) return true;
	const auto* Squad = Unit->GetWorld()->GetSubsystem<UBattleState>()->FindSquad(State->SquadID);
	if (!Squad) return false;
	const float LimitSquared = FMath::Square(Squad->DefenseRadius);
	if (FVector::DistSquared2D(Goal, Squad->Destination) > LimitSquared) return false;
	// A displaced/directly controlled member may need a detour to return from outside.
	if (FVector::DistSquared2D(Unit->GetActorLocation(), Squad->Destination) > LimitSquared) return true;
	if (auto* Metrics = Unit->GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->Paths;
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(Unit->GetWorld(), Unit->GetActorLocation(), Goal, Unit);
	if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
	for (const FVector& Point : Path->PathPoints)
		if (FVector::DistSquared2D(Point, Squad->Destination) > LimitSquared) return false;
	return true;
}
