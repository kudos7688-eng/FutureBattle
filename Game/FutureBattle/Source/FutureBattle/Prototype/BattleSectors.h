#pragma once
#include "BattleTerrain.h"

// East bank, south to north. West bank is its half-turn. One character per 50 m parcel.
// R bank, B bridge approach, S ford, J junction, H house, W warehouse, F field, T wood, ^ hill.
namespace BattleSectors
{
 constexpr int32 Columns=20, Rows=14, Size=5000;
 inline constexpr char East[Rows][11]={
  "RTT^^TTFFF",
  "RTTTTTFFFF",
  "BJJFFFWWFF",
  "RFFFFFWWFF",
  "SFFFFFWWFF",
  "RHHHHHJJFF",
  "BJHHHHJJJJ",
  "BJHHHHJJJJ",
  "RHHHHHJJFF",
  "SFFFTTTTFF",
  "RFFTT^^TTF",
  "BJJFFFFFFF",
  "RFFFFFFFFF",
  "RFFFFFFFFF"};
 // Each road is a continuous centreline in centimetres. Shared intersections are also AI junctions.
 constexpr int BridgeOffset=22500, FordOffset=12500, MainWidth=1400, SideWidth=850, StreetWidth=550, TrailWidth=300;
 inline constexpr int Crossings[]={-BridgeOffset,-FordOffset,0,FordOffset,BridgeOffset};
 // Survey points of the river on the north half (metres). Rotate them about the centre for the south half.
 inline const FIntPoint RiverNorth[]={{0,0},{8,50},{12,100},{10,125},{-5,175},{-12,225},{-8,275},{7,350}};
 inline double RiverX(double Y)
 {
  const double North=FMath::Abs(Y)/100.;
  for(int I=1;I<UE_ARRAY_COUNT(RiverNorth);++I){
   const auto A=RiverNorth[I-1],B=RiverNorth[I];
   if(North<=B.Y)return FMath::Sign(Y)*100.*FMath::Lerp(double(A.X),double(B.X),(North-A.Y)/(B.Y-A.Y));
  }
  return FMath::Sign(Y)*100.*RiverNorth[UE_ARRAY_COUNT(RiverNorth)-1].X;
 }
 inline bool IsFord(int Y){return FMath::Abs(Y)==FordOffset;}
 inline FIntPoint Cell(FVector P)
 {
  return {static_cast<int32>(FMath::FloorToInt(P.X/Size))+Columns/2,static_cast<int32>(FMath::FloorToInt(P.Y/Size))+Rows/2};
 }
 inline bool Valid(FIntPoint C){return C.X>=0 && C.X<Columns && C.Y>=0 && C.Y<Rows;}
 // Sector kinds no squad stops in or crosses: river R (maps H, I), mountain M and lake L (map J, 2026-11-22).
 inline bool Impassable(char Kind){return Kind=='R'||Kind=='M'||Kind=='L';}
 inline FVector Center(FIntPoint C){return FVector((C.X-Columns/2+.5)*Size,(C.Y-Rows/2+.5)*Size,0);}
 inline char Kind(FIntPoint C){return C.X>=10?East[C.Y][C.X-10]:East[Rows-1-C.Y][9-C.X];}
 inline const TArray<BattleTerrain::FHouse>& Houses()
 {
  static const auto List=[]{
   TArray<BattleTerrain::FHouse> Result;
   for(int Y=0;Y<Rows;++Y)for(int X=10;X<Columns;++X)if(Kind({X,Y})=='H')
   {
    const FVector P=Center({X,Y});Result.Add({P.X+800,P.Y+700,1500,1100});
   }
   return Result;
  }();return List;
 }
 inline const TArray<BattleTerrain::FHill>& Hills()
 {
  static const auto List=[]{
   TArray<BattleTerrain::FHill> Result;
   for(int Y=0;Y<Rows;++Y)for(int X=10;X<Columns;++X)if(Kind({X,Y})=='^')
   {
    const FVector P=Center({X,Y});Result.Add({P.X,P.Y,1600,1200,600,1250});
   }
   return Result;
  }();return List;
 }
}
