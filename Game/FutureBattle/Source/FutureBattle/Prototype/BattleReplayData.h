#pragma once
#include "CoreMinimal.h"
// Test-only Win64 little-endian format. No pointers or engine serialization versions.
struct FBattleReplayHeader
{
 uint32 Magic=0x31524246, Version=1;
 int32 Count=500, Frames=0, FirstShot=-1, Seed=0, Mode=0, Hz=30;
};
struct FBattleReplayCamera { float X,Y,Z,Pitch,Yaw,Roll,FOV; };
struct FBattleReplayUnit
{
 float X,Y,Z,FX,FY,FZ;
 uint8 Alive,Actor,Visible,LOD,Team,Reserved[3];
};
static_assert(sizeof(FBattleReplayHeader)==32 && sizeof(FBattleReplayCamera)==28 && sizeof(FBattleReplayUnit)==32);
