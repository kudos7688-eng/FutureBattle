#pragma once
#include "CoreMinimal.h"

class UWorld;
struct FBattleContact;

// Tactical-view display of the player's side (team 0), 2026-11-20. Display only: nothing here is read by rules or AI.
//  - Fog grid: 12.5m cells over the field; a cell is "in sight" while a living own soldier within his sight range has a clear
//    line to it (rays against static geometry, full circle, at eye height). Cells seen before keep "explored".
//  - Sector info (maps with 50m sectors): own soldiers, enemies the side knows (seen now / remembered), the share of the
//    sector in sight, the objective, own squads ordered there and cover positions (public map knowledge).
struct FBattleFogGrid
{
	static constexpr double Cell = 1250.;
	FVector2D Half = FVector2D::ZeroVector;
	int32 Columns = 0, Rows = 0;
	// Real time of the last ray that reached the cell; -1 = never seen.
	TArray<double> LastSeen;
	// Ground height at each cell corner ((Columns+1) x (Rows+1)), for drawing on the field.
	TArray<float> CornerZ;
	TWeakObjectPtr<const UWorld> World;
	double CycleStart = 0, CycleSeconds = .3;

	void Reset(const UWorld* InWorld);
	// Casts up to TraceBudget rays this frame, continuing the round of casters; a new round starts when the last one ends.
	void Update(const UWorld* InWorld, double Now, int32 TraceBudget);
	bool IsValid() const { return Columns > 0; }
	int32 IndexOf(double X, double Y) const;
	FVector2D CellMin(int32 X, int32 Y) const { return FVector2D(-Half.X + X * Cell, -Half.Y + Y * Cell); }
	// A cell counts as in sight until a whole round of casters has passed without reaching it.
	double Window() const { return FMath::Max(.75, CycleSeconds * 1.6 + .1); }
	bool InSight(int32 Index, double Now) const { return LastSeen.IsValidIndex(Index) && LastSeen[Index] >= 0 && Now - LastSeen[Index] <= Window(); }
	bool Explored(int32 Index) const { return LastSeen.IsValidIndex(Index) && LastSeen[Index] >= 0; }
	// 0 = in sight, 1 = explored but not in sight, 2 = never seen.
	uint8 State(int32 Index, double Now) const { return InSight(Index, Now) ? 0 : Explored(Index) ? 1 : 2; }
	float Corner(int32 X, int32 Y) const { return CornerZ.IsValidIndex(Y * (Columns + 1) + X) ? CornerZ[Y * (Columns + 1) + X] : 0.f; }

private:
	struct FCaster { FVector Eye; float Range = 0; };
	TArray<FCaster> Casters;
	TMap<int32, TArray<FVector>> RayDirections;
	int32 NextCaster = 0, NextRay = 0;
	void StartRound(const UWorld* InWorld, double Now);
	void Mark(double X, double Y, double Now);
};

struct FBattleSectorInfo
{
	int32 Own = 0, Seen = 0, Remembered = 0, InSightCells = 0, Cells = 0, Objective = INDEX_NONE, Cover = 0;
	TArray<int32> Squads;
	float SightShare() const { return Cells > 0 ? float(InSightCells) / Cells : 0.f; }
};

namespace BattleSectorView
{
	// Enemy contacts the player's side holds, best per enemy (seen now over remembered, then newest). Scouts' long-range sightings
	// are left out (they reach the side through reports), as on the minimap.
	void KnownEnemies(const UWorld* World, TMap<int32, const FBattleContact*>& Out, TSet<int32>& Visible);
	// One entry per sector (BattleSectors::Columns x Rows, index Y*Columns+X). Empty when the map has no sectors.
	void Gather(const UWorld* World, const FBattleFogGrid& Fog, double Now, const TMap<int32, const FBattleContact*>& Known, TArray<FBattleSectorInfo>& Out);
	// Cover positions per sector, counted once per world.
	const TArray<int32>& CoverCounts(const UWorld* World);
	// Ground height used for drawing (the terrain table; 0 on flat maps).
	float GroundZ(const UWorld* World, double X, double Y);
	// Japanese names for display.
	const TCHAR* KindName(char Kind);
	// Short status word and its colour index (0 none / fog, 1 own, 2 contested, 3 enemy seen, 4 enemy remembered, 5 clear in sight).
	int32 StatusOf(const FBattleSectorInfo& Info);
	const TCHAR* StatusName(int32 Status);
	FString SectorName(FIntPoint Cell);
}
