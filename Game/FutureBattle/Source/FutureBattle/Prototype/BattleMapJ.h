#pragma once
#include "BattleTerrain.h"

// Map J ("terrain-j-v1", 2026-11-22, Docs/design-map-j-terrain.md; user request: impassable mountains and terrain, varied maps with
// buildings). The grid of map I (1000m x 700m, 20 x 14 sectors of 50m, 180-degree rotation, at most one enterable building per
// sector) with two new kinds - M mountain (no passage, blocks sight and shots) and L lake (no passage, sight crosses) - and more
// kinds of buildings. Two mountain ranges (rows 3 and 10) split the field into a north lane, a central valley with the town and a
// south lane, joined by passes 50m wide (columns 6 and 13) and by the open ground near both homes. Every sector picks one of
// several layouts and one of eight orientations from a hash of its cell (Variant / Mirror), so equal kinds do not look alike.
// The east half is listed; the west half is its 180-degree rotation. Units: cm.
namespace BattleMapJ
{
	constexpr int32 Columns=20,Rows=14;
	constexpr double Cell=5000.,HalfX=50000.,HalfY=35000.;
	// H two-storey house (enterable)     B office tower block (not enterable, 12-30m)   A apartment block (not enterable, 9-12m)
	// C church with a bell tower         K factory (hall without roof, chimney)          U ruins (broken walls, rubble)
	// Y car park                         G park                                          Q row of shops (not enterable)
	// W warehouse   P plaza   O objective plaza with a house   D objective depot   F field   T wood   h hill   S home base
	// M mountain (impassable, blocks sight)   L lake (impassable, sight crosses)
	inline TCHAR Kind(int32 C,int32 R)
	{
		// Columns 10..19 (x = 0..500m), rows from the north (13) to the south (0).
		static const TCHAR* East[Rows]=
		{
			TEXT("TFGTHFTFFT"), // 13   north lane (outer edge)
			TEXT("FHUFOTHFTF"), // 12   north lane objective O (col 14); north road on its south edge (y = +250m)
			TEXT("THFFTFhTFF"), // 11   knoll h (col 16)
			TEXT("MMMFMMMTDF"), // 10   north range, pass at col 13; depot D (col 18)
			TEXT("BLLPQHGTHF"), //  9   lake L (cols 11-12)
			TEXT("ALLPBCHSSS"), //  8
			TEXT("OQHYBAUSSS"), //  7   town objective O (col 10); avenue on its south edge (y = 0)
			TEXT("BPAHQGHSSS"), //  6
			TEXT("HUWKHYHSSS"), //  5
			TEXT("QHAPHTFTFT"), //  4
			TEXT("MMMFMMMMTF"), //  3   south range, pass at col 13
			TEXT("FKWTOFTHFT"), //  2   south lane objective O (col 14); south road on its north edge (y = -250m)
			TEXT("TFhhFWTFFT"), //  1   ridge h (cols 12-13, rows 0-1)
			TEXT("TFhhTFTFTF"), //  0
		};
		if(C<0||R<0||C>=Columns||R>=Rows)return TEXT(' ');
		if(C<Columns/2){C=Columns-1-C;R=Rows-1-R;}
		return East[Rows-1-R][C-Columns/2];
	}
	inline FVector2D Centre(int32 C,int32 R) {return FVector2D(-HalfX+Cell*(C+.5),-HalfY+Cell*(R+.5));}
	inline bool Impassable(TCHAR K) {return K==TEXT('M')||K==TEXT('L');}
	// A pass: an open sector with mountains on both sides in its row. No wall or hedge is built on its borders.
	inline bool IsPass(int32 C,int32 R) {return !Impassable(Kind(C,R))&&Kind(C-1,R)==TEXT('M')&&Kind(C+1,R)==TEXT('M');}
	// A stable hash of a cell (the same on both halves for 180-degree partners, since only the east half is laid out).
	inline uint32 Hash(int32 C,int32 R,uint32 Salt=0)
	{
		uint32 H=uint32(C)*73856093u^uint32(R)*19349663u^(Salt+0x9e3779b9u);
		H^=H>>13;H*=0x5bd1e995u;H^=H>>15;return H;
	}
	inline int32 Variant(int32 C,int32 R,int32 Count) {return int32(Hash(C,R,1)%uint32(FMath::Max(1,Count)));}
	// One of eight orientations of a sector layout: swap x and y, then mirror x and/or y. Offsets and sizes go through it.
	struct FMirror
	{
		bool bSwap=false;double SX=1,SY=1;
		FVector2D Offset(double DX,double DY) const {const double X=bSwap?DY:DX,Y=bSwap?DX:DY;return FVector2D(X*SX,Y*SY);}
		FVector2D Size(double W,double H) const {return bSwap?FVector2D(H,W):FVector2D(W,H);}
	};
	inline FMirror Mirror(int32 C,int32 R) {const uint32 H=Hash(C,R,2);FMirror M;M.bSwap=(H&1)!=0;M.SX=(H&2)?-1.:1.;M.SY=(H&4)?-1.:1.;return M;}
	// Roads: straight lines on sector borders. bAlongX: y = At for x in From..To; otherwise x = At for y in From..To.
	struct FRoad { bool bAlongX=true; double At=0,From=0,To=0; };
	inline const TArray<FRoad>& Roads()
	{
		static const TArray<FRoad> List=[]
		{
			TArray<FRoad> L;
			L.Add({true,0.,-42000.,42000.});                                  // avenue through the valley
			L.Add({true,25000.,-45000.,45000.});L.Add({true,-25000.,-45000.,45000.}); // north and south lane roads
			// Tracks over the four passes (the middle of the pass column, x = +-175m; not on a sector border).
			L.Add({false,17500.,12500.,27500.});L.Add({false,17500.,-27500.,-12500.});L.Add({false,-17500.,12500.,27500.});L.Add({false,-17500.,-27500.,-12500.});
			L.Add({false,0.,-15000.,15000.});L.Add({false,25000.,-15000.,15000.});L.Add({false,-25000.,-15000.,15000.}); // town streets
			L.Add({false,40000.,-25000.,25000.});L.Add({false,-40000.,-25000.,25000.}); // beside the homes, from lane to lane
			return L;
		}();
		return List;
	}
	inline bool OnRoad(bool bAlongX,double At,double Mid)
	{
		for(const FRoad& R:Roads())if(R.bAlongX==bAlongX&&FMath::Abs(R.At-At)<1.&&Mid>R.From&&Mid<R.To)return true;
		return false;
	}
	// The enterable house of an H or O sector (east half), as laid out by BattleMapLayoutJ.inl. H: one of four places and three
	// sizes, turned by the sector's orientation; O: a smaller house in one corner (the middle stays free for the capture area).
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
	// Hills of the east half: a knoll (5m) in the north lane and a ridge (7m) in the south lane, each filling its own sectors (h).
	inline const TArray<BattleTerrain::FHill>& Hills()
	{
		static const TArray<BattleTerrain::FHill> List=[]
		{
			TArray<BattleTerrain::FHill> L;
			{BattleTerrain::FHill H;H.X=32500;H.Y=22500;H.W=2000;H.H=2000;H.Top=500;H.Run=1300;L.Add(H);}
			{BattleTerrain::FHill H;H.X=15000;H.Y=-30000;H.W=6000;H.H=2400;H.Top=700;H.Run=2000;L.Add(H);}
			return L;
		}();
		return List;
	}
	inline BattleTerrain::FHeights MakeHeights(FVector2D Half)
	{
		BattleTerrain::FHeights T;
		for(const BattleTerrain::FHouse& B:Houses())BattleTerrain::AddHouse(T,B);
		for(const BattleTerrain::FHill& H:Hills())BattleTerrain::AddHill(T,H);
		const int32 East=T.Patches.Num();
		for(int32 I=0;I<East;++I)T.Patches.Add(T.Patches[I].Rotated());
		T.Build(Half);return T;
	}
	// Objectives: the middle of an objective sector (O, D). Index I and 7-I are 180-degree counterparts.
	// 0 west depot, 1 north-west lane, 2 south-west lane, 3 west town, 4 east town, 5 north-east lane, 6 south-east lane, 7 east depot.
	inline TArray<FVector> Objectives()
	{
		return {FVector(-42500,-17500,0),FVector(-22500,22500,0),FVector(-22500,-27500,0),FVector(-2500,-2500,0),
			FVector(2500,2500,0),FVector(22500,27500,0),FVector(22500,-22500,0),FVector(42500,17500,0)};
	}
}
