#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleState.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "AI/NavDataGenerator.h"

namespace
{
	constexpr double CoverCellSize = 140.;
	bool CoverCell(const FVector& Position, FIntPoint& OutCell)
	{
		// Bound conversions and floating-point error; unusual coordinates use the original scan.
		if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) ||
			FMath::Abs(Position.X) > 1.e7 || FMath::Abs(Position.Y) > 1.e7) return false;
		OutCell = FIntPoint(FMath::FloorToInt(Position.X / CoverCellSize), FMath::FloorToInt(Position.Y / CoverCellSize));
		return true;
	}
}

void UBattleCover::BuildSlots(bool bForce)
{
	if (bBuilt && !bForce) return;
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return;
	Slots.Reset(); ResetReservations(); SlotCells.Reset(); ConflictSlots.Reset(); bSlotIndexValid = false; bBuilt = true;
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
		// Trial -BattleCoverDense=1: a slot every 1.4 m along each side longer than 3 m (duplicates within 1 m are dropped below).
		if (Urban && UBattleLeaderRuntime::CoverDenseEnabled())
			for (float Side : {-1.f, 1.f})
			{
				if (Extent.Y > 150.f) for (double A = -Extent.Y + 70.; A <= Extent.Y - 69.; A += 140.) Candidates.Add(FVector(Center.X + Side * (Extent.X + 95.f), Center.Y + A, Center.Z - Extent.Z));
				if (Extent.X > 150.f) for (double A = -Extent.X + 70.; A <= Extent.X - 69.; A += 140.) Candidates.Add(FVector(Center.X + A, Center.Y + Side * (Extent.Y + 95.f), Center.Z - Extent.Z));
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
	// Trial -BattleCoverSlotsMirror=1 (2026-11-25, default 0; map K west/east divergence, STATUS 11-25): a place is dropped when
	// another lies within 1 m, so which of two near places stays depends on the order the pieces are walked, and on map K one
	// place in five has no image within 30 cm on the other half (BATTLE_SYMMETRY no_partner_30cm 2101 / 2057 of 10300): mirror
	// squads then take other arrival places from the first order on. With the switch, on a symmetric-start map, the places of
	// the east half (x > 0) are replaced by the images (-x, -y) of the places of the west half.
	static const bool bSlotsMirror = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("BattleCoverSlotsMirror="), V); return V != 0; }();
	if (bSlotsMirror && BattleTactics::SymmetricStart(GetWorld()))
	{
		TArray<FVector> WestSlots;
		for (const FVector& WestSlot : Slots) if (WestSlot.X < 0.) WestSlots.Add(WestSlot);
		const int32 Before = Slots.Num();
		Slots = WestSlots;
		for (const FVector& WestSlot : WestSlots) Slots.Add(FVector(-WestSlot.X, -WestSlot.Y, WestSlot.Z));
		UE_LOG(LogTemp, Display, TEXT("BATTLE_COVER_MIRROR slots=%d before=%d west=%d"), Slots.Num(), Before, WestSlots.Num());
	}
	BuildSlotIndex();
	if (FParse::Param(FCommandLine::Get(), TEXT("BattleSymmetryAudit"))) AuditSymmetry();
}

void UBattleCover::BuildSlotIndex()
{
	SlotCells.Reset(); ConflictSlots.Reset(); bSlotIndexValid = false;
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FIntPoint SlotCell;
		if (!CoverCell(Slots[SlotIndex], SlotCell)) { SlotCells.Reset(); return; }
		SlotCells.FindOrAdd(SlotCell).Add(SlotIndex);
	}
	ConflictSlots.SetNum(Slots.Num());
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FIntPoint SlotCell; CoverCell(Slots[SlotIndex], SlotCell);
		// The usual 3x3 neighbourhood plus a guard ring for rounding at cell boundaries; filter exactly below.
		for (int32 OffsetX = -2; OffsetX <= 2; ++OffsetX) for (int32 OffsetY = -2; OffsetY <= 2; ++OffsetY)
			if (const auto* CellSlots = SlotCells.Find(SlotCell + FIntPoint(OffsetX, OffsetY)))
				for (int32 NearSlot : *CellSlots)
					if (FVector::DistSquared2D(Slots[SlotIndex], Slots[NearSlot]) < FMath::Square(140.f)) ConflictSlots[SlotIndex].Add(NearSlot);
	}
	bSlotIndexValid = true;
}

void UBattleCover::AuditSymmetry() const
{
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return;
	// 1. Obstacles: every static mesh actor and its partner at (-x, -y).
	TArray<FVector> Boxes; int32 LonelyBoxes = 0;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It) { FVector C, E; It->GetActorBounds(true, C, E); Boxes.Add(C); }
	for (const FVector& B : Boxes) LonelyBoxes += !Boxes.ContainsByPredicate([&](const FVector& O) { return FMath::Abs(O.X + B.X) < 2. && FMath::Abs(O.Y + B.Y) < 2. && FMath::Abs(O.Z - B.Z) < 2.; });
	// 2. Cover slots: by half, and those with no partner within 30 cm / 100 cm of the mirrored place.
	int32 West = 0, East = 0, No30[2] = {0, 0}, No100[2] = {0, 0};
	for (const FVector& S : Slots)
	{
		const int32 Half = S.X > 0; (Half ? East : West)++;
		double Best = DBL_MAX; for (const FVector& O : Slots) Best = FMath::Min(Best, FVector::DistSquared2D(O, FVector(-S.X, -S.Y, 0)));
		No30[Half] += Best > FMath::Square(30.); No100[Half] += Best > FMath::Square(100.);
	}
	UE_LOG(LogTemp, Display, TEXT("BATTLE_SYMMETRY boxes=%d boxes_without_partner=%d slots=%d west=%d east=%d no_partner_30cm west=%d east=%d no_partner_100cm west=%d east=%d"),
		Boxes.Num(), LonelyBoxes, Slots.Num(), West, East, No30[0], No30[1], No100[0], No100[1]);
	// 3. Navigation mesh: a 5 m grid over the city and its approaches; points where only one of the pair projects (within 50 cm).
	int32 NavBoth = 0, NavOnly[2] = {0, 0};
	for (double X = 250.; X <= 30000.; X += 500.) for (double Y = -19750.; Y <= 19750.; Y += 500.)
	{
		FNavLocation A, B;
		const bool bEast = Nav->ProjectPointToNavigation(FVector(X, Y, 100), A, FVector(50, 50, 300)), bWest = Nav->ProjectPointToNavigation(FVector(-X, -Y, 100), B, FVector(50, 50, 300));
		NavBoth += bEast && bWest; NavOnly[0] += bWest && !bEast; NavOnly[1] += bEast && !bWest;
	}
	UE_LOG(LogTemp, Display, TEXT("BATTLE_SYMMETRY nav_grid both=%d west_only=%d east_only=%d"), NavBoth, NavOnly[0], NavOnly[1]);
	// 4. Lines of sight at eye height (static geometry only): pairs of grid points up to 150 m apart and their mirrored pairs.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleSymmetryAudit), false);
	if (const auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>()) for (ABattleUnit* Unit : Mode->Units) if (Unit) Query.AddIgnoredActor(Unit);
	if (const auto* Runtime = GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); Runtime->IsEnabled()) for (const auto& Body : Runtime->GetBodies()) if (Body.Value.Capsule.IsValid()) Query.AddIgnoredComponent(Body.Value.Capsule.Get());
	auto Sees = [&](const FVector& A, const FVector& B) { FHitResult Hit; return !GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Query); };
	int32 Lines = 0, Differ = 0, OpenWestOnly = 0, OpenEastOnly = 0;
	for (double X = -12000.; X <= 12000.; X += 1500.) for (double Y = -9000.; Y <= 9000.; Y += 1500.)
		for (double DX = -15000.; DX <= 15000.; DX += 2500.) for (double DY = -15000.; DY <= 15000.; DY += 2500.)
		{
			if ((DX == 0. && DY == 0.) || DX * DX + DY * DY > FMath::Square(15000.)) continue;
			const FVector A(X, Y, 160), B(X + DX, Y + DY, 160);
			const bool bHere = Sees(A, B), bMirror = Sees(FVector(-A.X, -A.Y, 160), FVector(-B.X, -B.Y, 160));
			++Lines; Differ += bHere != bMirror;
		}
	UE_LOG(LogTemp, Display, TEXT("BATTLE_SYMMETRY sight_lines=%d differ_from_mirror=%d"), Lines, Differ);
	// 6. Routes: from the west side's starting rows to every objective, and the mirrored route from the east side. Length, and how far
	//    the east route (rotated 180 degrees) runs from the west route at its worst point.
	{
		const int32 N = BattleFieldLayout::ObjectiveCountForMap(GetWorld()->GetMapName());
		int32 Routes = 0, Apart10 = 0, Apart30 = 0; double WorstApart = 0., LengthDiff = 0.;
		auto Sample = [](const TArray<FVector>& Path, TArray<FVector>& Out) { Out.Reset(); for (int32 I = 1; I < Path.Num(); ++I) { const double L = FVector::Dist2D(Path[I - 1], Path[I]); const int32 Steps = FMath::Max(1, FMath::CeilToInt(L / 200.)); for (int32 S = 0; S <= Steps; ++S) Out.Add(FMath::Lerp(Path[I - 1], Path[I], double(S) / Steps)); } };
		auto Length = [](const TArray<FVector>& Path) { double L = 0; for (int32 I = 1; I < Path.Num(); ++I) L += FVector::Dist2D(Path[I - 1], Path[I]); return L; };
		for (int32 Group = 0; Group < 25; Group += 3) for (int32 Point = 0; Point < N; ++Point)
		{
			const FVector Start = BattleFieldLayout::WideSpawnFor(GetWorld(), 0, Group), Goal = BattleFieldLayout::ObjectiveForMap(GetWorld()->GetMapName(), Point);
			FNavLocation S0, G0, S1, G1;
			if (!Nav->ProjectPointToNavigation(Start, S0, FVector(300, 300, 400)) || !Nav->ProjectPointToNavigation(Goal, G0, FVector(300, 300, 400)) ||
				!Nav->ProjectPointToNavigation(FVector(-Start.X, -Start.Y, Start.Z), S1, FVector(300, 300, 400)) || !Nav->ProjectPointToNavigation(FVector(-Goal.X, -Goal.Y, Goal.Z), G1, FVector(300, 300, 400))) continue;
			auto* W = Nav->FindPathToLocationSynchronously(GetWorld(), S0.Location, G0.Location); auto* E = Nav->FindPathToLocationSynchronously(GetWorld(), S1.Location, G1.Location);
			if (!W || !E || !W->IsValid() || !E->IsValid()) continue;
			TArray<FVector> A, B; Sample(W->PathPoints, A); Sample(E->PathPoints, B);
			double Apart = 0.; FVector Where = FVector::ZeroVector;
			for (const FVector& P : A) { double Best = DBL_MAX; for (const FVector& Q : B) Best = FMath::Min(Best, FVector::DistSquared2D(P, FVector(-Q.X, -Q.Y, 0))); if (Best > Apart) { Apart = Best; Where = P; } }
			Apart = FMath::Sqrt(Apart); const double LW = Length(W->PathPoints), LE = Length(E->PathPoints);
			++Routes; Apart10 += Apart > 1000.; Apart30 += Apart > 3000.; WorstApart = FMath::Max(WorstApart, Apart); LengthDiff += FMath::Abs(LW - LE);
			if (Apart > 1000.) UE_LOG(LogTemp, Display, TEXT("BATTLE_SYMMETRY route group=%d point=%d west_len=%.0f east_len=%.0f apart=%.0f at_west=(%.0f,%.0f)"), Group, Point, LW / 100, LE / 100, Apart / 100, Where.X / 100, Where.Y / 100);
		}
		UE_LOG(LogTemp, Display, TEXT("BATTLE_SYMMETRY routes=%d apart_over_10m=%d apart_over_30m=%d worst_apart_m=%.0f mean_length_diff_m=%.1f"), Routes, Apart10, Apart30, WorstApart / 100, Routes ? LengthDiff / Routes / 100 : 0.);
	}
	// 5. The places where west leaders fall early (world metres) and their mirrored places: cover slots within 30 m, and how many of
	//    the points 60-150 m away on the enemy side see the place.
	const FVector Places[] = {FVector(-3000, -4000, 0), FVector(-6000, -2000, 0), FVector(-3000, 6000, 0), FVector(-9000, -1000, 0)};
	for (const FVector& W : Places)
	{
		const FVector Pair[2] = {W, FVector(-W.X, -W.Y, 0)}; int32 Near[2] = {0, 0}, Seen[2] = {0, 0}, Tried[2] = {0, 0};
		for (int32 I = 0; I < 2; ++I)
		{
			for (const FVector& S : Slots) Near[I] += FVector::DistSquared2D(S, Pair[I]) <= FMath::Square(3000.);
			const double Toward = I == 0 ? 1. : -1.; // the enemy is to the east of a west place and to the west of an east place
			for (double DX = 6000.; DX <= 15000.; DX += 1000.) for (double DY = -9000.; DY <= 9000.; DY += 1000.)
			{
				if (DX * DX + DY * DY > FMath::Square(15000.)) continue;
				const FVector From(Pair[I].X + Toward * DX, Pair[I].Y + Toward * DY, 160);
				FNavLocation On; if (!Nav->ProjectPointToNavigation(From, On, FVector(50, 50, 300))) continue;
				++Tried[I]; Seen[I] += Sees(From, FVector(Pair[I].X, Pair[I].Y, 160));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("BATTLE_SYMMETRY place west=(%.0f,%.0f) slots_30m west=%d east=%d seen_from_enemy_side west=%d/%d east=%d/%d"), W.X / 100, W.Y / 100, Near[0], Near[1], Seen[0], Tried[0], Seen[1], Tried[1]);
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
	// Use the original float square (also for negative radii). One cm pads rounding at the rectangle boundary.
	const double RadiusSquared = FMath::Square(Radius), Extent = FMath::Sqrt(RadiusSquared) + 1.;
	FIntPoint MinCell, MaxCell;
	const bool bUseGrid = bSlotIndexValid && FMath::IsFinite(RadiusSquared) &&
		CoverCell(Origin - FVector(Extent, Extent, 0), MinCell) && CoverCell(Origin + FVector(Extent, Extent, 0), MaxCell) &&
		(int64(MaxCell.X) - MinCell.X + 1) * (int64(MaxCell.Y) - MinCell.Y + 1) <= Slots.Num();
	if (bUseGrid)
	{
		for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX) for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			if (const auto* CellSlots = SlotCells.Find(FIntPoint(CellX, CellY)))
				for (int32 Slot : *CellSlots) if (FVector::DistSquared2D(Origin, Slots[Slot]) <= FMath::Square(Radius)) Result.Add(Slot);
		// Feed the same complete sequence to the same unstable distance sort, including all ties.
		Result.Sort();
	}
	else for (int32 Slot = 0; Slot < Slots.Num(); ++Slot) if (FVector::DistSquared2D(Origin, Slots[Slot]) <= FMath::Square(Radius)) Result.Add(Slot);
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
	if (bSlotIndexValid)
	{
		// Owner reads expiry and life now, not at index-build time. No traces or state changes occur here.
		for (int32 ConflictSlot : ConflictSlots[Slot])
		{
			const int32 ConflictOwner = Owner(ConflictSlot, Now);
			if (ConflictOwner != INDEX_NONE && ConflictOwner != UnitID) return false;
		}
	}
	else for (const auto& Pair : Reservations)
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
	// This is the only insertion path: release first keeps one stored slot per owner, even after expiry/death.
	Release(UnitID);
	// An expired/dead owner's record can be overwritten before that owner releases it.
	if (const auto* Previous = Reservations.Find(Slot))
		if (const int32* PreviousSlot = UnitToSlot.Find(Previous->UnitID); PreviousSlot && *PreviousSlot == Slot) UnitToSlot.Remove(Previous->UnitID);
	FBattleCoverReservation Reservation; Reservation.UnitID = UnitID; Reservation.ExpiresAt = Now + 2.5;
	Reservations.Add(Slot, Reservation);
	UnitToSlot.Add(UnitID, Slot);
	return true;
}

void UBattleCover::Release(int32 UnitID)
{
	if (const int32* ReservedSlot = UnitToSlot.Find(UnitID))
	{
		const auto* OwnedReservation = Reservations.Find(*ReservedSlot);
		if (OwnedReservation && OwnedReservation->UnitID == UnitID) Reservations.Remove(*ReservedSlot);
		UnitToSlot.Remove(UnitID);
	}
}
