#include "BattleSectorView.h"
#include "BattleSectors.h"
#include "BattleFieldLayout.h"
#include "BattleState.h"
#include "BattlePerception.h"
#include "BattleMatch.h"
#include "BattleCover.h"
#include "BattleLeaderRuntime.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

float BattleSectorView::GroundZ(const UWorld* World, double X, double Y)
{
	const auto& Heights = BattleFieldLayout::Of(World).Heights;
	if (Heights.IsEmpty()) return 0.f;
	double Z = 0; Heights.Ground(X, Y, 0., Z, nullptr, 1e7);
	return float(Z);
}

void FBattleFogGrid::Reset(const UWorld* InWorld)
{
	World = InWorld;
	Half = BattleFieldLayout::FieldHalfExtent(InWorld);
	Columns = FMath::Max(1, FMath::CeilToInt(2 * Half.X / Cell)); Rows = FMath::Max(1, FMath::CeilToInt(2 * Half.Y / Cell));
	LastSeen.Init(-1., Columns * Rows);
	CornerZ.SetNumUninitialized((Columns + 1) * (Rows + 1));
	for (int32 Y = 0; Y <= Rows; ++Y) for (int32 X = 0; X <= Columns; ++X)
	{
		const FVector2D P = CellMin(X, Y);
		CornerZ[Y * (Columns + 1) + X] = BattleSectorView::GroundZ(InWorld, P.X, P.Y);
	}
	Casters.Reset(); NextCaster = NextRay = 0; CycleStart = 0; CycleSeconds = .3;
}

int32 FBattleFogGrid::IndexOf(double X, double Y) const
{
	const int32 CX = FMath::FloorToInt((X + Half.X) / Cell), CY = FMath::FloorToInt((Y + Half.Y) / Cell);
	return CX >= 0 && CY >= 0 && CX < Columns && CY < Rows ? CY * Columns + CX : INDEX_NONE;
}

void FBattleFogGrid::Mark(double X, double Y, double Now)
{
	const int32 I = IndexOf(X, Y); if (I != INDEX_NONE) LastSeen[I] = Now;
}

void FBattleFogGrid::StartRound(const UWorld* InWorld, double Now)
{
	if (CycleStart > 0) CycleSeconds = FMath::Clamp(Now - CycleStart, .02, 5.);
	CycleStart = Now; Casters.Reset(); NextCaster = NextRay = 0;
	const auto* Store = InWorld->GetSubsystem<UBattleState>(); const auto* Leaders = InWorld->GetSubsystem<UBattleLeaderRuntime>();
	if (!Store || !Leaders) return;
	// One caster per occupied cell (soldiers of a squad stand close together); the longest sight range of the cell wins.
	TMap<int32, int32> ByCell;
	for (const auto& Pair : Store->GetUnits())
	{
		if (Pair.Value.Team != 0 || !Pair.Value.IsAlive()) continue;
		const FVector P = Leaders->Position(Pair.Key); if (P.IsZero()) continue;
		const int32 I = IndexOf(P.X, P.Y); if (I == INDEX_NONE) continue;
		const float Range = Pair.Value.Perception.SightRange;
		if (const int32* Known = ByCell.Find(I)) { if (Casters[*Known].Range < Range) Casters[*Known].Range = Range; continue; }
		ByCell.Add(I, Casters.Add({ P + FVector(0, 0, 100), Range }));
	}
}

void FBattleFogGrid::Update(const UWorld* InWorld, double Now, int32 TraceBudget)
{
	if (!IsValid() || World.Get() != InWorld) Reset(InWorld);
	if (NextCaster >= Casters.Num()) StartRound(InWorld, Now);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleFogView), false);
	const FCollisionObjectQueryParams Static(ECC_WorldStatic);
	const double Step = Cell * .5;
	while (TraceBudget > 0 && NextCaster < Casters.Num())
	{
		const FCaster& C = Casters[NextCaster];
		// Enough rays that neighbouring rays are less than a cell apart at the end of the range.
		const int32 Rays = FMath::Clamp(FMath::CeilToInt(2 * PI * C.Range / (Cell * .8)), 24, 160);
		TArray<FVector>& Directions = RayDirections.FindOrAdd(Rays);
		if (Directions.IsEmpty())
		{
			Directions.Reserve(Rays);
			for (int32 RayIndex = 0; RayIndex < Rays; ++RayIndex)
			{
				// Keep the original double expression and separate sin/cos calls.
				const double Angle = 2 * PI * RayIndex / Rays;
				Directions.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0));
			}
		}
		if (NextRay == 0) Mark(C.Eye.X, C.Eye.Y, Now);
		for (; NextRay < Rays && TraceBudget > 0; ++NextRay, --TraceBudget)
		{
			const FVector& Dir = Directions[NextRay];
			// Each caster/ray runs once per round, so there is no repeated endpoint to cache here.
			FVector End = C.Eye + Dir * C.Range; End.Z = BattleSectorView::GroundZ(InWorld, End.X, End.Y) + 100.;
			FHitResult Hit;
			const double Reach = InWorld->LineTraceSingleByObjectType(Hit, C.Eye, End, Static, Query) ? Hit.Distance + Cell * .6 : C.Range;
			for (double D = Step; D <= FMath::Min<double>(Reach, C.Range); D += Step) Mark(C.Eye.X + Dir.X * D, C.Eye.Y + Dir.Y * D, Now);
		}
		if (NextRay >= Rays) { ++NextCaster; NextRay = 0; }
	}
	if (NextCaster >= Casters.Num() && Casters.IsEmpty()) CycleSeconds = .3;
}

void BattleSectorView::KnownEnemies(const UWorld* World, TMap<int32, const FBattleContact*>& Out, TSet<int32>& Visible)
{
	Out.Reset(); Visible.Reset();
	const auto* Store = World->GetSubsystem<UBattleState>(); auto* Sense = World->GetSubsystem<UBattlePerception>(); const auto* Match = World->GetSubsystem<UBattleMatch>();
	if (!Store || !Sense || !Match) return;
	for (const auto& Pair : Store->GetUnits()) if (Pair.Value.Team == 0 && Pair.Value.IsAlive())
		if (const auto* Contacts = Sense->GetContacts(Pair.Key)) for (const auto& C : *Contacts)
		{
			if (Match->bArea && Match->IsScoutLongContact(Pair.Key, C.Value)) continue;
			if (C.Value.bVisible) Visible.Add(C.Key);
			const FBattleContact*& Best = Out.FindOrAdd(C.Key);
			if (!Best || (C.Value.bVisible && !Best->bVisible) || (C.Value.bVisible == Best->bVisible && C.Value.LastSeenTime > Best->LastSeenTime)) Best = &C.Value;
		}
}

const TArray<int32>& BattleSectorView::CoverCounts(const UWorld* World)
{
	static TWeakObjectPtr<const UWorld> For; static int32 SlotCount = -1; static TArray<int32> Counts;
	const auto* Cover = World->GetSubsystem<UBattleCover>();
	const int32 Now = Cover ? Cover->GetSlots().Num() : 0;
	if (For.Get() != World || SlotCount != Now)
	{
		For = World; SlotCount = Now; Counts.Init(0, BattleSectors::Columns * BattleSectors::Rows);
		if (Cover) for (const FVector& S : Cover->GetSlots()) { const FIntPoint C = BattleSectors::Cell(S); if (BattleSectors::Valid(C)) ++Counts[C.Y * BattleSectors::Columns + C.X]; }
	}
	return Counts;
}

void BattleSectorView::Gather(const UWorld* World, const FBattleFogGrid& Fog, double Now, const TMap<int32, const FBattleContact*>& Known, TArray<FBattleSectorInfo>& Out)
{
	if (!BattleFieldLayout::Of(World).bSectors) { Out.Reset(); return; }
	constexpr int32 N = BattleSectors::Columns * BattleSectors::Rows;
	Out.SetNum(N);
	for (auto& Sector : Out)
	{
		Sector.Own = Sector.Seen = Sector.Remembered = Sector.InSightCells = Sector.Cells = Sector.Cover = 0;
		Sector.Objective = INDEX_NONE; Sector.Squads.Reset();
	}
	auto Slot = [&](FVector P)->FBattleSectorInfo* { const FIntPoint C = BattleSectors::Cell(P); return BattleSectors::Valid(C) ? &Out[C.Y * BattleSectors::Columns + C.X] : nullptr; };
	const auto* Store = World->GetSubsystem<UBattleState>(); const auto* Leaders = World->GetSubsystem<UBattleLeaderRuntime>(); const auto* Match = World->GetSubsystem<UBattleMatch>();
	if (!Store || !Leaders || !Match) return;
	for (const auto& Pair : Store->GetUnits()) if (Pair.Value.Team == 0 && Pair.Value.IsAlive())
	{ const FVector P = Leaders->Position(Pair.Key); if (!P.IsZero()) if (auto* S = Slot(P)) ++S->Own; }
	for (const auto& Pair : Known) if (auto* S = Slot(Pair.Value->LastKnownPosition))
	{ if (Pair.Value->bVisible) ++S->Seen; else if (Pair.Value->Confidence > .35f) ++S->Remembered; }
	for (int32 I = 0; I < Match->Objectives.Num(); ++I) if (auto* S = Slot(Match->Objectives[I].Position)) S->Objective = I;
	for (const auto& Pair : Store->GetSquads())
	{
		const auto& Q = Pair.Value;
		if (Q.Team != 0 || !Q.bSectorMission || Q.CommandStatus == EBattleCommandStatus::Disabled) continue;
		bool bAlive = false; for (int32 ID : Q.Members) if (const auto* U = Store->Find(ID); U && U->IsAlive()) { bAlive = true; break; }
		if (bAlive) if (auto* S = Slot(Q.Destination)) S->Squads.Add(Pair.Key);
	}
	const auto& Cover = CoverCounts(World);
	for (int32 I = 0; I < N; ++I) Out[I].Cover = Cover.IsValidIndex(I) ? Cover[I] : 0;
	// Share of the sector in sight: the fog cells whose centre lies inside it.
	if (Fog.IsValid()) for (int32 Y = 0; Y < Fog.Rows; ++Y) for (int32 X = 0; X < Fog.Columns; ++X)
	{
		const FVector2D M = Fog.CellMin(X, Y) + FVector2D(FBattleFogGrid::Cell * .5);
		if (auto* S = Slot(FVector(M.X, M.Y, 0))) { ++S->Cells; if (Fog.InSight(Y * Fog.Columns + X, Now)) ++S->InSightCells; }
	}
}

const TCHAR* BattleSectorView::KindName(char Kind)
{
	switch (Kind)
	{
	case 'H': return TEXT("住宅"); case 'J': return TEXT("交差点・広場"); case 'W': return TEXT("倉庫・集積所"); case 'F': return TEXT("畑");
	case 'T': return TEXT("林"); case '^': return TEXT("丘"); case 'R': return TEXT("川"); case 'M': return TEXT("山"); case 'L': return TEXT("湖"); case 'B': return TEXT("橋"); case 'S': return TEXT("浅瀬");
	default: return TEXT("―");
	}
}

int32 BattleSectorView::StatusOf(const FBattleSectorInfo& Info)
{
	if (Info.Own > 0 && Info.Seen > 0) return 2;
	if (Info.Seen > 0) return 3;
	if (Info.Own > 0) return 1;
	if (Info.Remembered > 0) return 4;
	return Info.SightShare() >= .5f ? 5 : 0;
}

const TCHAR* BattleSectorView::StatusName(int32 Status)
{
	static const TCHAR* const Names[] = { TEXT("霧（不明）"), TEXT("自軍が確保"), TEXT("争奪中"), TEXT("敵を確認"), TEXT("敵の痕跡"), TEXT("視界内・無人") };
	return Names[FMath::Clamp(Status, 0, 5)];
}

FString BattleSectorView::SectorName(FIntPoint Cell) { return FString::Printf(TEXT("%c%d"), TCHAR('A' + Cell.X), 1 + Cell.Y); }
