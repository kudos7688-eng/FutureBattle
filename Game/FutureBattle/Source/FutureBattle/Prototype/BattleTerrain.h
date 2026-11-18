#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"

// Ground heights of a map (2026-11-18, map G). Maps A-F are one flat floor and carry no patches; every user of this file does
// nothing on them. A map with height differences lists rectangular patches: a flat one (a hilltop, an upper floor) or a ramp that
// rises along X or Y (a hillside, stairs). The map generator builds the same shapes from the same list (BattleMapCommandlet), so
// the soldiers that are moved by hand (the many followers; leaders walk with the engine's character movement) can be kept on the
// ground without a trace per step. Two levels may lie above each other (ground floor and upper floor): the level nearest to the
// soldier's feet is his.
namespace BattleTerrain
{
	// Largest height a soldier steps up or down at once (the navigation agent's step is 35cm).
	constexpr double StepMax=45.;
	// Tag of the actors that are ground to walk on (ramps, hilltops, upper floors). Hand-moved soldiers ignore them in their sweeps
	// and follow FHeightPatch instead; cover slots and the obstacle count skip them.
	inline const FName& Tag(){static const FName Name(TEXT("BattleTerrain"));return Name;}
	inline bool IsGround(const AActor* Actor) {return Actor&&Actor->Tags.Num()&&Actor->ActorHasTag(Tag());}
	inline bool IsGround(const FHitResult& Hit) {return IsGround(Hit.GetActor());}
	struct FHeightPatch
	{
		FVector2D Min=FVector2D::ZeroVector,Max=FVector2D::ZeroVector;
		// Axis 0: flat at Z0. Axis 1: Z0 at Min.X rising (or falling) to Z1 at Max.X. Axis 2: the same along Y.
		float Z0=0,Z1=0;uint8 Axis=0;
		// Solid = nothing to walk on beneath it (a hill, stairs); otherwise the ground below stays a level (an upper floor).
		bool bSolid=true;
		bool Contains(double X,double Y) const {return X>=Min.X&&X<=Max.X&&Y>=Min.Y&&Y<=Max.Y;}
		double HeightAt(double X,double Y) const
		{
			if(Axis==0)return Z0;
			const double T=Axis==1?(X-Min.X)/FMath::Max(1.,Max.X-Min.X):(Y-Min.Y)/FMath::Max(1.,Max.Y-Min.Y);
			return Z0+(Z1-Z0)*FMath::Clamp(T,0.,1.);
		}
		// Rise per run of a ramp (0 on a flat patch).
		double Slope() const {return Axis==0?0.:FMath::Abs(Z1-Z0)/FMath::Max(1.,Axis==1?Max.X-Min.X:Max.Y-Min.Y);}
		// The 180-degree counterpart (the maps are point symmetric).
		FHeightPatch Rotated() const {FHeightPatch P=*this;P.Min=-Max;P.Max=-Min;P.Z0=Z1;P.Z1=Z0;if(Axis==0)P.Z1=P.Z0=Z0;return P;}
	};
	struct FHeights
	{
		TArray<FHeightPatch> Patches;
		// 25m cells over the floor, each with the patches that touch it.
		TArray<TArray<int32>> Cells;FVector2D Half=FVector2D::ZeroVector;int32 Columns=0,Rows=0;
		static constexpr double Cell=2500.;
		bool IsEmpty() const {return Patches.IsEmpty();}
		void Build(FVector2D InHalf)
		{
			Half=InHalf;Columns=FMath::CeilToInt(2*Half.X/Cell);Rows=FMath::CeilToInt(2*Half.Y/Cell);Cells.Reset();Cells.SetNum(Columns*Rows);
			for(int32 I=0;I<Patches.Num();++I)
			{
				const FHeightPatch& P=Patches[I];
				const int32 X0=FMath::Clamp(FMath::FloorToInt((P.Min.X+Half.X)/Cell),0,Columns-1),X1=FMath::Clamp(FMath::FloorToInt((P.Max.X+Half.X)/Cell),0,Columns-1);
				const int32 Y0=FMath::Clamp(FMath::FloorToInt((P.Min.Y+Half.Y)/Cell),0,Rows-1),Y1=FMath::Clamp(FMath::FloorToInt((P.Max.Y+Half.Y)/Cell),0,Rows-1);
				for(int32 X=X0;X<=X1;++X)for(int32 Y=Y0;Y<=Y1;++Y)Cells[Y*Columns+X].Add(I);
			}
		}
		// The level at (X,Y) nearest to Feet. False when no level is within Step of the feet (a wall of earth, a drop): the place
		// cannot be stepped onto from where the soldier stands. Slope: rise per run of the ground there.
		bool Ground(double X,double Y,double Feet,double& Out,double* Slope=nullptr,double Step=StepMax) const
		{
			Out=0;if(Slope)*Slope=0;
			if(Patches.IsEmpty())return true;
			const int32 CX=FMath::FloorToInt((X+Half.X)/Cell),CY=FMath::FloorToInt((Y+Half.Y)/Cell);
			bool bBase=true;double Best=0,BestGap=FMath::Abs(Feet),BestSlope=0;bool bAny=false;
			if(CX>=0&&CY>=0&&CX<Columns&&CY<Rows)for(int32 I:Cells[CY*Columns+CX])
			{
				const FHeightPatch& P=Patches[I];if(!P.Contains(X,Y))continue;
				if(P.bSolid)bBase=false;
				const double Z=P.HeightAt(X,Y),Gap=FMath::Abs(Z-Feet);
				if(!bAny||Gap<BestGap){bAny=true;Best=Z;BestGap=Gap;BestSlope=P.Slope();}
			}
			if(bBase&&(!bAny||FMath::Abs(Feet)<BestGap)){Best=0;BestGap=FMath::Abs(Feet);BestSlope=0;}
			Out=Best;if(Slope)*Slope=BestSlope;
			return BestGap<=Step;
		}
	};
	// How far above the ground the centre of a hand-moved soldier's capsule (radius 36) is kept: 100cm on the flat (as on maps
	// A-F), a little more on a slope so the round bottom clears it.
	inline double CentreAbove(double Slope) {return 100.+(Slope>0?36.*(FMath::Sqrt(1.+Slope*Slope)-1.)+2.:0.);}

	// Map G, east half (the west half is the rotation). One list for the map definition (heights) and the generator (geometry).
	// House: two storeys, enterable. Centre, size along X (at least 14m) and Y (at least 9m). The stairs run along the north wall
	// and rise towards +X; the upper floor is at 320cm.
	struct FHouse { double X=0,Y=0,W=1400,H=1000; };
	// Hill: a flat top (centre, size) at Top cm with a ramp of Run cm on each of the four sides.
	struct FHill { double X=0,Y=0,W=0,H=0,Top=180,Run=600; };
	constexpr double FloorZ=320.,WallThick=40.,StairWidth=180.,StairRun=640.;
	struct FHouseShape
	{
		// Inner faces of the outer walls, the stairs (x from S0 to S1, y from SY to N) and the two parts of the upper floor.
		double W0,E0,S0y,N0y,S0,S1,SY;
		explicit FHouseShape(const FHouse& B)
		{
			W0=B.X-B.W*.5+WallThick;E0=B.X+B.W*.5-WallThick;S0y=B.Y-B.H*.5+WallThick;N0y=B.Y+B.H*.5-WallThick;
			S0=B.X-StairRun*.5;S1=B.X+StairRun*.5;SY=N0y-StairWidth;
		}
	};
	inline void AddHouse(FHeights& T,const FHouse& B)
	{
		const FHouseShape S(B);FHeightPatch P;
		// Stairs.
		P.Min=FVector2D(S.S0,S.SY);P.Max=FVector2D(S.S1,S.N0y);P.Z0=0;P.Z1=FloorZ;P.Axis=1;P.bSolid=true;T.Patches.Add(P);
		// Upper floor: everything south of the stairs, and the landing east of them.
		P=FHeightPatch();P.Z0=P.Z1=FloorZ;P.Axis=0;P.bSolid=false;
		P.Min=FVector2D(S.W0,S.S0y);P.Max=FVector2D(S.E0,S.SY);T.Patches.Add(P);
		P.Min=FVector2D(S.S1,S.SY);P.Max=FVector2D(S.E0,S.N0y);T.Patches.Add(P);
	}
	inline void AddHill(FHeights& T,const FHill& H)
	{
		const double X0=H.X-H.W*.5,X1=H.X+H.W*.5,Y0=H.Y-H.H*.5,Y1=H.Y+H.H*.5;FHeightPatch P;P.bSolid=true;
		P.Min=FVector2D(X0,Y0);P.Max=FVector2D(X1,Y1);P.Z0=P.Z1=H.Top;P.Axis=0;T.Patches.Add(P);
		P.Axis=1;P.Min=FVector2D(X0-H.Run,Y0);P.Max=FVector2D(X0,Y1);P.Z0=0;P.Z1=H.Top;T.Patches.Add(P);
		P.Min=FVector2D(X1,Y0);P.Max=FVector2D(X1+H.Run,Y1);P.Z0=H.Top;P.Z1=0;T.Patches.Add(P);
		P.Axis=2;P.Min=FVector2D(X0,Y0-H.Run);P.Max=FVector2D(X1,Y0);P.Z0=0;P.Z1=H.Top;T.Patches.Add(P);
		P.Min=FVector2D(X0,Y1);P.Max=FVector2D(X1,Y1+H.Run);P.Z0=H.Top;P.Z1=0;T.Patches.Add(P);
	}
	// East-half houses and hills of map G (see BattleMapLayoutF.inl for where they stand in the layout).
	// Houses: every "house with a yard" lot of the city (the same blocks and lots as BattleMapLayoutF.inl: the old town beside
	// the avenue and the houses district), and the farm of the south hamlet.
	inline const TArray<FHouse>& HousesG()
	{
		static const TArray<FHouse> List=[]
		{
			TArray<FHouse> L;
			const double BlockX[2][2]={{800,12200},{13800,25000}};
			const double BlockY[4][2]={{-15000,-8300},{-6700,-1200},{1200,6700},{8300,15000}};
			for(int32 BX=0;BX<2;++BX) for(int32 BY=0;BY<4;++BY)
			{
				if((BX==0&&BY==0)||(BX==1&&BY<=1))continue; // park, warehouses
				const double X0=BlockX[BX][0],X1=BlockX[BX][1],Y0=BlockY[BY][0],Y1=BlockY[BY][1],CX=(X0+X1)*.5,CY=(Y0+Y1)*.5;
				for(int32 LX=0;LX<2;++LX) for(int32 LY=0;LY<2;++LY)
				{
					const double LX0=LX?CX+400:X0,LX1=LX?X1:CX-400,LY0=LY?CY+400:Y0,LY1=LY?Y1:CY-400;
					const int32 Kind=BX==1?((LX+LY+BY)%2?2:0):(BX+2*BY+LX+3*LY);
					if(Kind%4)continue;
					const double W=LX1-LX0,H=LY1-LY0;
					FHouse B;B.X=(LX0+LX1)*.5-W*.2;B.Y=(LY0+LY1)*.5;B.W=W*.42;B.H=H-600;L.Add(B);
				}
			}
			{FHouse B;B.X=17100;B.Y=-26900;B.W=1500;B.H=1000;L.Add(B);}
			return L;
		}();
		return List;
	}
	// Hills: a ridge on the south flank between the city edge and the flank road, a knoll north of the north flank road, and a
	// rise between the city and the east home that overlooks the avenue. All clear of the roads.
	inline const TArray<FHill>& HillsG()
	{
		static const TArray<FHill> List=[]
		{
			TArray<FHill> L;
			{FHill H;H.X=19000;H.Y=-20000;H.W=5000;H.H=2500;H.Top=180;L.Add(H);}
			{FHill H;H.X=7000;H.Y=29000;H.W=4000;H.H=2000;H.Top=180;L.Add(H);}
			{FHill H;H.X=31000;H.Y=8000;H.W=3000;H.H=3000;H.Top=150;L.Add(H);}
			return L;
		}();
		return List;
	}
	// True within Margin of a hill (its top and ramps): scattered ground cover stays out.
	inline bool NearHillG(double X,double Y,double Margin)
	{
		for(int32 Side:{1,-1})for(const FHill& H:HillsG())
			if(FMath::Abs(X*Side-H.X)<H.W*.5+H.Run+Margin&&FMath::Abs(Y*Side-H.Y)<H.H*.5+H.Run+Margin)return true;
		return false;
	}
	inline FHeights MakeG(FVector2D Half)
	{
		FHeights T;
		for(const FHouse& B:HousesG())AddHouse(T,B);
		for(const FHill& H:HillsG())AddHill(T,H);
		const int32 East=T.Patches.Num();
		for(int32 I=0;I<East;++I)T.Patches.Add(T.Patches[I].Rotated());
		T.Build(Half);return T;
	}
}
