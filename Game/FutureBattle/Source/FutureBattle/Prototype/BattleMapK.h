#pragma once
#include "BattleMapJ.h"

// Map K ("town-k-v1", 2026-11-22, Docs/design-map-k.md, Docs/design-map-policy.md; user decision "inside the 1000m x 700m frame,
// ringed by mountains"): a dense small town for close fights (30-50m) that come from the ground and not from a sight switch.
// The grid of maps I and J (20 x 14 sectors of 50m, 180-degree rotation). The town fills columns 4..15 and rows 3..10
// (600m x 400m, 96 sectors, no field, no wood, no hill, no water); everything else is mountain (M, impassable, blocks sight) but
// the two home bases (S, where map J has them) and one column of open ground between each home and the town.
// Six objectives: north, middle and south on each half, none in the very middle. The avenue between the homes turns twice and
// the streets are short pieces, so no street is seen along for more than 250m.
// The sector kinds, their layouts and orientations are those of map J: BattleMapCommandlet.cpp includes BattleMapLayoutJ.inl
// with this namespace in place of BattleMapJ, so this header offers the same names. The east half is listed. Units: cm.
namespace BattleMapK
{
	using BattleMapJ::Columns;using BattleMapJ::Rows;using BattleMapJ::Cell;using BattleMapJ::HalfX;using BattleMapJ::HalfY;
	using BattleMapJ::Centre;using BattleMapJ::Impassable;using BattleMapJ::Hash;using BattleMapJ::Variant;
	using BattleMapJ::FMirror;using BattleMapJ::Mirror;using BattleMapJ::FRoad;
	// The letters of BattleMapJ.h: H house (enterable), B office towers, A apartment blocks, C church, K factory, U ruins, Y car park,
	// G park, Q shops, W warehouse, P plaza, O objective plaza with a house, S home base, M mountain.
	inline TCHAR Kind(int32 C,int32 R)
	{
		// Columns 10..19 (x = 0..500m), rows from the north (13) to the south (0).
		static const TCHAR* East[Rows]=
		{
			TEXT("MMMMMMMMMM"), // 13
			TEXT("MMMMMMMMMM"), // 12
			TEXT("MMMMMMMMMM"), // 11
			TEXT("AHQBHGMMMM"), // 10   north edge of the town
			TEXT("QBOHAUMMMM"), //  9   north objective O (col 12)
			TEXT("HPAYBHGSSS"), //  8   home base S (cols 17-19, rows 5-8), open ground in col 16
			TEXT("BQHCAYPSSS"), //  7   avenue on its south edge east of x = 100m (y = +50m)
			TEXT("POAHGBYSSS"), //  6   middle objective O (col 11); avenue on its north edge west of x = 100m (y = 0)
			TEXT("AHYBQHGSSS"), //  5
			TEXT("HKUOHAMMMM"), //  4   south objective O (col 13)
			TEXT("GAWHBGMMMM"), //  3   south edge of the town
			TEXT("MMMMMMMMMM"), //  2
			TEXT("MMMMMMMMMM"), //  1
			TEXT("MMMMMMMMMM"), //  0
		};
		if(C<0||R<0||C>=Columns||R>=Rows)return TEXT(' ');
		if(C<Columns/2){C=Columns-1-C;R=Rows-1-R;}
		return East[Rows-1-R][C-Columns/2];
	}
	// No pass on this map (BattleMapLayoutJ.inl leaves the borders of a pass open).
	inline bool IsPass(int32,int32) {return false;}
	// Roads: straight lines on sector borders, both halves listed (each line below has its 180-degree partner in the list).
	// bAlongX: y = At for x in From..To; otherwise x = At for y in From..To. No border wall or hedge is built on a road.
	inline const TArray<FRoad>& Roads()
	{
		static const TArray<FRoad> List=[]
		{
			TArray<FRoad> L;
			// Avenue (the tanks' way between the homes): home - x = 350m - y = +50m - x = 100m - y = 0 - x = -100m - y = -50m - home.
			L.Add({true,0.,35000.,42000.});L.Add({false,35000.,0.,5000.});L.Add({true,5000.,10000.,35000.});L.Add({false,10000.,0.,5000.});
			L.Add({true,0.,-10000.,10000.});
			L.Add({false,-10000.,-5000.,0.});L.Add({true,-5000.,-35000.,-10000.});L.Add({false,-35000.,-5000.,0.});L.Add({true,0.,-42000.,-35000.});
			// North lane: y = +150m at both ends, y = +100m in the middle. South lane: its rotation.
			L.Add({true,15000.,-30000.,-10000.});L.Add({false,-10000.,10000.,15000.});L.Add({true,10000.,-10000.,10000.});L.Add({false,10000.,10000.,15000.});L.Add({true,15000.,10000.,30000.});
			L.Add({true,-15000.,10000.,30000.});L.Add({false,10000.,-15000.,-10000.});L.Add({true,-10000.,-10000.,10000.});L.Add({false,-10000.,-15000.,-10000.});L.Add({true,-15000.,-30000.,-10000.});
			// Connectors: beside the town's ends (x = +-300m) and through its middle (x = +-100m).
			L.Add({false,30000.,5000.,15000.});L.Add({false,30000.,-15000.,5000.});L.Add({false,-30000.,-15000.,-5000.});L.Add({false,-30000.,-5000.,15000.});
			L.Add({false,10000.,5000.,10000.});L.Add({false,10000.,-10000.,0.});L.Add({false,-10000.,-10000.,-5000.});L.Add({false,-10000.,0.,10000.});
			return L;
		}();
		return List;
	}
	inline bool OnRoad(bool bAlongX,double At,double Mid)
	{
		for(const FRoad& R:Roads())if(R.bAlongX==bAlongX&&FMath::Abs(R.At-At)<1.&&Mid>R.From&&Mid<R.To)return true;
		return false;
	}
	// The enterable house of an H or O sector (east half), placed as on map J (BattleMapJ::HouseOf) from this map's kinds.
	inline bool HouseOf(int32 C,int32 R,BattleTerrain::FHouse& Out)
	{
		const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);const FMirror F=Mirror(C,R);
		if(K==TEXT('H'))
		{
			static const double Places[4][2]={{-300,400},{400,-300},{-500,-450},{350,500}};
			static const double Sizes[3][2]={{1600,1100},{1400,1200},{1800,1000}};
			const int32 V=Variant(C,R,12);const FVector2D O=F.Offset(Places[V%4][0],Places[V%4][1]);const FVector2D S=F.Size(Sizes[V/4][0],Sizes[V/4][1]);
			Out.X=M.X+O.X;Out.Y=M.Y+O.Y;Out.W=S.X;Out.H=S.Y;return true;
		}
		if(K==TEXT('O'))
		{
			const FVector2D O=F.Offset(1350,1500);const FVector2D S=F.Size(1400,1000);Out.X=M.X+O.X;Out.Y=M.Y+O.Y;Out.W=S.X;Out.H=S.Y;return true;
		}
		return false;
	}
	inline const TArray<BattleTerrain::FHouse>& Houses()
	{
		static const TArray<BattleTerrain::FHouse> List=[]
		{
			TArray<BattleTerrain::FHouse> L;
			for(int32 C=Columns/2;C<Columns;++C)for(int32 R=0;R<Rows;++R){BattleTerrain::FHouse B;if(HouseOf(C,R,B))L.Add(B);}
			return L;
		}();
		return List;
	}
	// No hills.
	inline const TArray<BattleTerrain::FHill>& Hills() {static const TArray<BattleTerrain::FHill> List;return List;}
	inline BattleTerrain::FHeights MakeHeights(FVector2D Half)
	{
		BattleTerrain::FHeights T;
		for(const BattleTerrain::FHouse& B:Houses())BattleTerrain::AddHouse(T,B);
		const int32 East=T.Patches.Num();
		for(int32 I=0;I<East;++I)T.Patches.Add(T.Patches[I].Rotated());
		T.Build(Half);return T;
	}
	// Objectives: the middle of an objective sector (O). Index I and 5-I are 180-degree counterparts.
	// 0 south-west, 1 north-west, 2 west middle, 3 east middle, 4 south-east, 5 north-east.
	inline TArray<FVector> Objectives()
	{
		return {FVector(-12500,-12500,0),FVector(-17500,12500,0),FVector(-7500,2500,0),FVector(7500,-2500,0),FVector(17500,-12500,0),FVector(12500,12500,0)};
	}
}
