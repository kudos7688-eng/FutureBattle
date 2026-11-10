#include "BattleCover.h"
#include "BattleState.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

void UBattleCover::BuildSlots(bool bForce)
{
	if (bBuilt && !bForce) return;
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return;
	Slots.Reset(); Reservations.Reset(); bBuilt = true;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleCoverSlots), false);
	if (const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit) Query.AddIgnoredActor(Unit);
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		FVector Center, Extent; It->GetActorBounds(true, Center, Extent);
		if (Extent.Z < 45.f || Extent.X > 1000.f || Extent.Y > 1000.f || Extent.X < 10.f || Extent.Y < 10.f) continue;
		TArray<FVector> Candidates;
		for (float Side : {-1.f, 1.f}) for (float Along : {-.75f, 0.f, .75f})
		{
			Candidates.Add(FVector(Center.X + Side * (Extent.X + 95.f), Center.Y + Along * Extent.Y, Center.Z - Extent.Z));
			Candidates.Add(FVector(Center.X + Along * Extent.X, Center.Y + Side * (Extent.Y + 95.f), Center.Z - Extent.Z));
		}
		for (const FVector& Candidate : Candidates)
		{
			FNavLocation NavPoint;
			if (!Nav->ProjectPointToNavigation(Candidate, NavPoint, FVector(70, 70, 300))) continue;
			if (GetWorld()->OverlapBlockingTestByChannel(NavPoint.Location + FVector(0, 0, 94), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(40.f, 90.f), Query)) continue;
			if (!Slots.ContainsByPredicate([&](const FVector& Point) { return FVector::DistSquared2D(Point, NavPoint.Location) < FMath::Square(100.f); })) Slots.Add(NavPoint.Location);
		}
	}
}

TArray<int32> UBattleCover::NearbySlots(FVector Origin, float Radius, int32 Limit) const
{
	TArray<int32> Result;
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot) if (FVector::DistSquared2D(Origin, Slots[Slot]) <= FMath::Square(Radius)) Result.Add(Slot);
	Result.Sort([&](int32 A, int32 B) { return FVector::DistSquared2D(Origin, Slots[A]) < FVector::DistSquared2D(Origin, Slots[B]); });
	if (Result.Num() > Limit) Result.SetNum(FMath::Max(0, Limit));
	return Result;
}

int32 UBattleCover::Owner(int32 Slot, double Now) const
{
	const auto* Reservation = Reservations.Find(Slot);
	if (!Reservation || Reservation->ExpiresAt <= Now) return INDEX_NONE;
	const auto* State = GetWorld()->GetSubsystem<UBattleState>()->Find(Reservation->UnitID);
	return State && State->IsAlive() ? Reservation->UnitID : INDEX_NONE;
}

bool UBattleCover::IsAvailable(int32 Slot, int32 UnitID, double Now) const
{
	if (!Slots.IsValidIndex(Slot)) return false;
	for (const auto& Pair : Reservations)
	{
		const int32 Other = Owner(Pair.Key, Now);
		if (Other != INDEX_NONE && Other != UnitID && FVector::DistSquared2D(Slots[Slot], Slots[Pair.Key]) < FMath::Square(140.f)) return false;
	}
	return true;
}

bool UBattleCover::Reserve(int32 Slot, int32 UnitID, double Now)
{
	const auto* State = GetWorld()->GetSubsystem<UBattleState>()->Find(UnitID);
	if (!State || !State->IsAlive() || !IsAvailable(Slot, UnitID, Now)) return false;
	Release(UnitID);
	FBattleCoverReservation Reservation; Reservation.UnitID = UnitID; Reservation.ExpiresAt = Now + 2.5;
	Reservations.Add(Slot, Reservation);
	return true;
}

void UBattleCover::Release(int32 UnitID)
{
	for (auto It = Reservations.CreateIterator(); It; ++It) if (It.Value().UnitID == UnitID) It.RemoveCurrent();
}
