#pragma once
#include "CoreMinimal.h"

namespace BattleKnownFire
{
 bool Enabled();
 bool MoveEnabled();
 double MoveRange();
 double MoveSquadRadius();
 int32 MoveMax();
 enum class EReject : uint8 { None, Stale, Order, Range, FOV, Invalid, SensorWall, AimWall, Deferred, Waiting };
 struct FTarget
 {
  int32 TargetID=INDEX_NONE,SourceUnitID=INDEX_NONE,SourceSquadID=INDEX_NONE;
  uint8 ObservedSquadType=255,SourceKind=0; // 0 direct, 1 delivered contact
  FVector LastKnownPosition=FVector::ZeroVector,TrackedPosition=FVector::ZeroVector;
  double ObservedAt=0,ReceivedAt=0,ExpiresAt=0,TrackSampleAt=0,TrackUntil=0;
  uint64 Revision=0;
  bool Tracked(double Now) const {return TrackUntil>Now && ExpiresAt>Now;}
  bool Fresh(double Now) const {return Now>=ObservedAt && Now<ExpiresAt && (Tracked(Now)||Now<ObservedAt+5.);}
  FVector At(double Now) const {return Tracked(Now)?TrackedPosition:LastKnownPosition;}
 };
 struct FTeam
 {
  TMap<int32,FTarget> Targets;
  TMap<FIntPoint,TArray<int32>> Grid;
  uint64 Revision=0;
 };
 struct FResult
 {
  int32 TargetID=INDEX_NONE,SourceUnitID=INDEX_NONE;
  FVector AimPosition=FVector::ZeroVector;
  double CheckedAt=0;
  uint64 Revision=0;
  bool bTracked=false;
  EReject Reject=EReject::Waiting;
  bool Ready() const {return TargetID!=INDEX_NONE && Reject==EReject::None;}
 };
 struct FSearch
 {
  double NextAt=0;TArray<int32> Candidates;int32 Cursor=0,Selected=INDEX_NONE;bool bPending=true;
  int32 ConfirmedTarget=INDEX_NONE;FVector ConfirmedAim=FVector::ZeroVector;
  bool bCoverFacing=false;FVector CoverAnchor=FVector::ZeroVector,CoverPeek=FVector::ZeroVector,CoverFacing=FVector::ZeroVector;
  int32 CoverTarget=INDEX_NONE,CoverRevision=INDEX_NONE;
 };
 struct FBudget
 {
  int64 Window=-1;
  int32 Used=0;
  TArray<int32> Waiting;
  TMap<int32,double> RequestedAt;
 };
}
