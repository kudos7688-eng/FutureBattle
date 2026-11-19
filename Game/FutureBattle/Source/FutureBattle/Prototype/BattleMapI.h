#pragma once
#include "BattleTerrain.h"

// Map I ("sectors-v1", 2026-11-19): a map designed from sectors. 1000m x 700m in 20 x 14 sectors of 50m; every sector has a kind
// that decides what stands in it, at most one building that can be entered, and cover towards all four sides (fields excepted,
// they are open on purpose). Roads run on sector borders, so a sector is what lies between the roads, walls and hedges around it.
// The east half is listed; the west half is its 180-degree rotation. One table for the map definition (objectives, heights,
// routes) and the generator (BattleMapLayoutI.inl), so the two cannot drift apart. Units: cm.
// (Codex builds map H from the same decisions of ROADMAP 2026-11-19; map I is the second design for comparison.)
namespace BattleMapI
{
	constexpr int32 Columns=20,Rows=14;
	constexpr double Cell=5000.,HalfX=50000.,HalfY=35000.;
	// H house (two storeys, enterable), W warehouse, P plaza, O objective plaza with a house, D objective depot, F field (open),
	// T wood, R river, h hill, S home base.
	inline TCHAR Kind(int32 C,int32 R)
	{
		// Columns 10..19 (x = 0..500m), rows from the north (13) to the south (0).
		static const TCHAR* East[Rows]=
		{
			TEXT("FRFTTFFFFF"), // 13
			TEXT("TRHFTTFHFF"), // 12
			TEXT("HRHHFTHFFF"), // 11
			TEXT("HROHFHDFFF"), // 10   ---- north road on its north edge (y = +200m)
			TEXT("HRHPHFTFFF"), //  9
			TEXT("HHPHhTFSSS"), //  8
			TEXT("OHHPHFFSSS"), //  7   ---- avenue on its south edge (y = 0)
			TEXT("HPHHFTFSSS"), //  6
			TEXT("HHWHFFHSSS"), //  5
			TEXT("PHHFTFFFFF"), //  4   ---- south road on its south edge (y = -200m)
			TEXT("HHFOHFTHFF"), //  3
			TEXT("FHTFFFTFFF"), //  2
			TEXT("FFFhhFFFFF"), //  1
			TEXT("TFFhhFTFFF"), //  0
		};
		if(C<0||R<0||C>=Columns||R>=Rows)return TEXT(' ');
		if(C<Columns/2){C=Columns-1-C;R=Rows-1-R;}
		return East[Rows-1-R][C-Columns/2];
	}
	inline FVector2D Centre(int32 C,int32 R) {return FVector2D(-HalfX+Cell*(C+.5),-HalfY+Cell*(R+.5));}
	inline void SectorAt(double X,double Y,int32& C,int32& R) {C=FMath::Clamp(FMath::FloorToInt((X+HalfX)/Cell),0,Columns-1);R=FMath::Clamp(FMath::FloorToInt((Y+HalfY)/Cell),0,Rows-1);}
	// Roads: straight lines on sector borders. bAlongX: y = At for x in From..To; otherwise x = At for y in From..To.
	struct FRoad { bool bAlongX=true; double At=0,From=0,To=0; };
	inline const TArray<FRoad>& Roads()
	{
		static const TArray<FRoad> List=[]
		{
			TArray<FRoad> L;
			L.Add({true,0.,-42000.,42000.});L.Add({true,20000.,-35000.,35000.});L.Add({true,-20000.,-35000.,35000.});
			for(double X:{-35000.,-25000.,-10000.,0.,10000.,25000.,35000.})L.Add({false,X,-20000.,20000.});
			return L;
		}();
		return List;
	}
	// True when the border piece along X (bAlongX) or Y at At, with its middle at Mid, lies on a road.
	inline bool OnRoad(bool bAlongX,double At,double Mid)
	{
		for(const FRoad& R:Roads())if(R.bAlongX==bAlongX&&FMath::Abs(R.At-At)<1.&&Mid>R.From&&Mid<R.To)return true;
		return false;
	}
	// The river of the east half: a strip beside the x = +100m road from the pond at y = +100m to the north edge. The north road
	// crosses it on a bridge (vehicles too); a ford further north is for infantry only (3m wide). The west river is the rotation.
	constexpr double RiverX0=6500.,RiverX1=8500.,RiverY0=10000.,RiverY1=35000.,BridgeY=20000.,BridgeHalf=500.,FordY=28000.,FordHalf=150.;
	// Houses of the east half: one in every H sector, and a smaller one in the north-east corner of an objective plaza (O), whose
	// middle stays free for the capture area.
	inline const TArray<BattleTerrain::FHouse>& Houses()
	{
		static const TArray<BattleTerrain::FHouse> List=[]
		{
			TArray<BattleTerrain::FHouse> L;
			for(int32 C=Columns/2;C<Columns;++C)for(int32 R=0;R<Rows;++R)
			{
				const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);BattleTerrain::FHouse B;
				if(K==TEXT('H')){B.X=M.X-300;B.Y=M.Y+400;B.W=1600;B.H=1100;L.Add(B);}
				else if(K==TEXT('O')){B.X=M.X+1350;B.Y=M.Y+1500;B.W=1400;B.H=1000;L.Add(B);}
			}
			return L;
		}();
		return List;
	}
	// Hills of the east half: a knoll (5m) inside the town's east edge that overlooks the avenue, and a ridge (7m) south of the
	// south road. Each fills its own sectors (h), which hold nothing else.
	inline const TArray<BattleTerrain::FHill>& Hills()
	{
		static const TArray<BattleTerrain::FHill> List=[]
		{
			TArray<BattleTerrain::FHill> L;
			{BattleTerrain::FHill H;H.X=22500;H.Y=7500;H.W=2000;H.H=2000;H.Top=500;H.Run=1300;L.Add(H);}
			{BattleTerrain::FHill H;H.X=20000;H.Y=-29500;H.W=6000;H.H=2400;H.Top=700;H.Run=2000;L.Add(H);}
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
	// 0 west depot, 1 north-west hamlet, 2 south-west bridge, 3 west town plaza, 4 east town plaza, 5 north-east bridge,
	// 6 south-east hamlet, 7 east depot.
	inline TArray<FVector> Objectives()
	{
		return {FVector(-32500,-17500,0),FVector(-17500,17500,0),FVector(-12500,-17500,0),FVector(-2500,-2500,0),
			FVector(2500,2500,0),FVector(12500,17500,0),FVector(17500,-17500,0),FVector(32500,17500,0)};
	}
}
