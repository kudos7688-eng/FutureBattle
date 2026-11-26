// Static cube geometry only; never called by modes 0/1 or by slot construction.
#include "BattleCover.h"
#include "BattleTerrain.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

TArray<FBox> UBattleCover::KnownMoveGeometry(FVector Origin,double Radius,bool Rebuild)
{
 if(Rebuild){bKnownGeometryBuilt=false;KnownGeometryBoxes.Reset();KnownGeometryCells.Reset();}
 if(!bKnownGeometryBuilt)
 {
  bKnownGeometryBuilt=true;
  for(TActorIterator<AActor> GeometryActor(GetWorld());GeometryActor;++GeometryActor)
  {
   if(BattleTerrain::IsGround(*GeometryActor) || !GeometryActor->GetActorEnableCollision())continue;
   TInlineComponentArray<UPrimitiveComponent*> Pieces(*GeometryActor);
   for(const UPrimitiveComponent* Piece:Pieces)
   {
    if(Piece->Mobility!=EComponentMobility::Static || !Piece->IsQueryCollisionEnabled() || Piece->GetCollisionObjectType()!=ECC_WorldStatic || Piece->GetCollisionResponseToChannel(ECC_Visibility)!=ECR_Block)continue;
    const auto* Mesh=Cast<UStaticMeshComponent>(Piece);
    if(!Cast<UBoxComponent>(Piece) && (!Mesh || !Mesh->GetStaticMesh() || Mesh->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")))continue;
    // A rotated box's bounds are not solid. Keep only world-axis-aligned pieces.
    const FRotator Rotation=Piece->GetComponentRotation();
    if(!FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Rotation.Pitch),90.),.01) || !FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Rotation.Yaw),90.),.01) || !FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Rotation.Roll),90.),.01))continue;
    const FBox Bounds=Piece->Bounds.GetBox();if(Bounds.GetExtent().Z<45.)continue;
    const int32 BoxIndex=KnownGeometryBoxes.Add(Bounds);
    const int32 MinX=FMath::FloorToInt(Bounds.Min.X/2500.),MaxX=FMath::FloorToInt(Bounds.Max.X/2500.);
    const int32 MinY=FMath::FloorToInt(Bounds.Min.Y/2500.),MaxY=FMath::FloorToInt(Bounds.Max.Y/2500.);
    for(int32 CellX=MinX;CellX<=MaxX;++CellX)for(int32 CellY=MinY;CellY<=MaxY;++CellY)KnownGeometryCells.FindOrAdd(FIntPoint(CellX,CellY)).Add(BoxIndex);
   }
  }
 }
 TSet<int32> Found;
 const int32 QueryMinX=FMath::FloorToInt((Origin.X-Radius)/2500.),QueryMaxX=FMath::FloorToInt((Origin.X+Radius)/2500.);
 const int32 QueryMinY=FMath::FloorToInt((Origin.Y-Radius)/2500.),QueryMaxY=FMath::FloorToInt((Origin.Y+Radius)/2500.);
 for(int32 QueryX=QueryMinX;QueryX<=QueryMaxX;++QueryX)for(int32 QueryY=QueryMinY;QueryY<=QueryMaxY;++QueryY)
  if(const auto* Bucket=KnownGeometryCells.Find(FIntPoint(QueryX,QueryY)))for(int32 FoundIndex:*Bucket)Found.Add(FoundIndex);
 TArray<int32> Ordered=Found.Array();
 Ordered.Sort([&](int32 LeftIndex,int32 RightIndex){const double LeftGap=KnownGeometryBoxes[LeftIndex].ComputeSquaredDistanceToPoint(Origin),RightGap=KnownGeometryBoxes[RightIndex].ComputeSquaredDistanceToPoint(Origin);return LeftGap==RightGap?LeftIndex<RightIndex:LeftGap<RightGap;});
 TArray<FBox> Result;
 for(int32 OrderedIndex:Ordered)
 {
  const FBox& NearBox=KnownGeometryBoxes[OrderedIndex];
  if(NearBox.ComputeSquaredDistanceToPoint(Origin)>FMath::Square(Radius))continue;
  Result.Add(NearBox);if(Result.Num()==96)break;
 }
 return Result;
}
