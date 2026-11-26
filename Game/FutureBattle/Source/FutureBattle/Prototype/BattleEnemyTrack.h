#pragma once
#include "CoreMinimal.h"

class UWorld;

// Player map information only; never an AI perception source.
struct FBattleSeenEnemySquad
{
 FVector Sum=FVector::ZeroVector; int32 Count=0; uint8 Type=255;
 TArray<FVector,TInlineAllocator<8>> Soldiers;
};
TMap<int32,FBattleSeenEnemySquad> SeenEnemySquads(const UWorld* World);

struct FBattleEnemyTrackEntry
{
 int32 SquadID=INDEX_NONE; uint8 Type=255;
 int32 Count=0; uint8 State=0; // 0 seen, 1 tracked, 2 remembered
 FVector Where=FVector::ZeroVector;
 double LastSeenAt=0.;
 TArray<FVector,TInlineAllocator<8>> Soldiers;
};
namespace BattleEnemyTrack
{
 float TrackSeconds();
 float KeepSeconds(); // Explicit zero disables the table, even if Track is positive.
 int32 StateAt(double Age,float Track,float Keep);
}
