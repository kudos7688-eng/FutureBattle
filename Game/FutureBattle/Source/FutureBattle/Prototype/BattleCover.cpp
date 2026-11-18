#include "BattleCover.h"
#include "BattleState.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "AI/NavDataGenerator.h"

void UBattleCover::BuildSlots(bool bForce)
{
	if (bBuilt && !bForce) return;
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return;
	Slots.Reset(); Reservations.Reset(); bBuilt = true;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleCoverSlots), false);
	if (const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit) Query.AddIgnoredActor(Unit);
 if(const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();Runtime->IsEnabled())
  for(const auto& Body:Runtime->GetBodies())if(Body.Value.Capsule.IsValid())Query.AddIgnoredComponent(Body.Value.Capsule.Get());
	// Duplicate check "any slot within 100 cm (2D)" through a 100 cm grid (3x3 cells): same result and order as scanning all slots.
	TMap<FIntPoint, TArray<int32>> SlotGrid;
	auto Cell = [](const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / 100.), FMath::FloorToInt(P.Y / 100.)); };
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		// Ground pieces of a map with heights (ramps, hilltops, upper floors) are no cover.
		if (BattleTerrain::IsGround(*It)) continue;
		FVector Center, Extent; It->GetActorBounds(true, Center, Extent);
		// Map E: city buildings are up to 52m x 30m, so they count as cover and get slots nearer their corners as well.
		const bool Urban=BattleFieldLayout::IsUrban(GetWorld());
		const FVector Limit=Urban?FVector(3200,3200,0):BattleFieldLayout::Expand(FVector(1000,1000,0),BattleFieldLayout::IsExpanded(GetWorld()));
		if (Extent.Z < 45.f || Extent.X > Limit.X || Extent.Y > Limit.Y || Extent.X < 10.f || Extent.Y < 10.f) continue;
		TArray<FVector> Candidates;
		static const TArray<float> WideSamples={-.75f,0.f,.75f},UrbanSamples={-.95f,-.5f,0.f,.5f,.95f};
		for (float Side : {-1.f, 1.f}) for (float Along : (Urban?UrbanSamples:WideSamples))
		{
			Candidates.Add(FVector(Center.X + Side * (Extent.X + 95.f), Center.Y + Along * Extent.Y, Center.Z - Extent.Z));
			Candidates.Add(FVector(Center.X + Along * Extent.X, Center.Y + Side * (Extent.Y + 95.f), Center.Z - Extent.Z));
		}
		for (const FVector& Candidate : Candidates)
		{
			FNavLocation NavPoint;
			if (!Nav->ProjectPointToNavigation(Candidate, NavPoint, FVector(70, 70, 300))) continue;
			if (GetWorld()->OverlapBlockingTestByChannel(NavPoint.Location + FVector(0, 0, 94), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(40.f, 90.f), Query)) continue;
			const FIntPoint C = Cell(NavPoint.Location); bool bNear = false;
			for (int32 DX = -1; DX <= 1 && !bNear; ++DX) for (int32 DY = -1; DY <= 1 && !bNear; ++DY)
				if (const auto* Near = SlotGrid.Find(C + FIntPoint(DX, DY)))
					for (int32 Index : *Near) if (FVector::DistSquared2D(Slots[Index], NavPoint.Location) < FMath::Square(100.f)) { bNear = true; break; }
			if (!bNear) SlotGrid.FindOrAdd(C).Add(Slots.Add(NavPoint.Location));
		}
	}
}

void UBattleCover::PrebuildSlots()
{
	if (bBuilt) return;
	// Only the default (infantry) navmesh decides the slots; the vehicle navmesh keeps building at runtime during the wait.
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* Data = Nav ? Nav->GetDefaultNavDataInstance() : nullptr;
	if (!Data || (Data->GetGenerator() && Data->GetGenerator()->IsBuildInProgressCheckDirty())) return;
	const double Start = FPlatformTime::Seconds();
	BuildSlots();
	UE_LOG(LogTemp, Display, TEXT("BATTLE_COVER_PREBUILT slots=%d ms=%.1f t=%.1f"), Slots.Num(), (FPlatformTime::Seconds() - Start) * 1000., GetWorld()->GetTimeSeconds());
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
