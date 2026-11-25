#pragma once
#include "BattleMapI.h"

// Map L (2026-11-25, Docs/design-terrain-field.md): the sectors, roads, river, houses and objectives of map I on rolling ground.
// The ground is a height field (BattleTerrain::FHeightField, 2.5m grid) instead of the flat floor: gentle rolling everywhere
// (slopes of a few per cent), the knoll and the ridge of map I as smooth hills (6m and 10m instead of boxes with ramps), two broad
// rises, a shallow valley along each river and a low rise in each half of the town. The home bases stay flat at height 0 (the
// starting columns, hiring places and supply checks assume it), and every house and warehouse stands on a flat pad. The heights
// are point symmetric (every feature is listed with its 180-degree partner). One table for the map definition (BattleFieldLayout.h)
// and the generator (BattleMapCommandlet.cpp, the ground mesh), so the two cannot drift apart. Units: cm.
namespace BattleMapL
{
	using namespace BattleMapI;
	constexpr double GridStep=250.;
	inline double Smooth(double T) {T=FMath::Clamp(T,0.,1.);return T*T*(3.-2.*T);}
	// A smooth hill: Height at distance 0, falling to 0 at Radius (cos^2 profile).
	inline double Bump(double D,double Height,double Radius) {return D>=Radius?0.:Height*FMath::Square(FMath::Cos(UE_DOUBLE_HALF_PI*D/Radius));}
	inline double SegmentDistance(double X,double Y,double X0,double Y0,double X1,double Y1)
	{
		const FVector2D A(X0,Y0),B(X1,Y1),P(X,Y);const FVector2D AB=B-A;
		const double T=FMath::Clamp(FVector2D::DotProduct(P-A,AB)/FMath::Max(1.,AB.SizeSquared()),0.,1.);
		return FVector2D::Distance(P,A+AB*T);
	}
	// Flat footprints: every house (as on map I) and warehouse (BattleMapLayoutI.inl), east half. Centre and size.
	struct FPad { double X=0,Y=0,W=0,H=0; };
	inline const TArray<FPad>& Pads()
	{
		static const TArray<FPad> List=[]
		{
			TArray<FPad> L;
			for(const BattleTerrain::FHouse& B:Houses())L.Add({B.X,B.Y,B.W,B.H});
			for(int32 C=Columns/2;C<Columns;++C)for(int32 R=0;R<Rows;++R)
			{
				const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);
				if(K==TEXT('W'))L.Add({M.X,M.Y+300,2800,1800});
				else if(K==TEXT('D'))L.Add({M.X,M.Y+1450,2600,1100});
			}
			return L;
		}();
		return List;
	}
	// Margin of flat ground around a pad, and the steepest the ground may rise or fall away from the flat ground of a pad (25%,
	// 14 degrees): around a pad the ground is the rolling ground held within that slope of the pad's height, so a pad cuts a
	// terrace into a hillside or stands on a bank of that slope, and nothing around it is steeper than the ground already was or 25%.
	// A blend (earlier version) made the hill flanks next to pads up to 51% steep.
	constexpr double PadMargin=300.,PadSlope=.25,PadReach=6000.;
	// Shape of the ground before the pads (point symmetric).
	inline double Raw(double X,double Y)
	{
		const double Tau=2.*UE_DOUBLE_PI;
		// Rolling: even functions of (X,Y) only, so h(-X,-Y) = h(X,Y). Slopes of each term below 5%.
		double H=260.
			+110.*FMath::Cos(Tau*(X/26000.+Y/41000.))
			+80.*FMath::Sin(Tau*X/19000.)*FMath::Sin(Tau*Y/23000.)
			+55.*FMath::Cos(Tau*(Y/9500.-X/13000.))
			+35.*FMath::Cos(Tau*(X/7300.+Y/11800.));
		for(const double S:{1.,-1.})
		{
			const double PX=S*X,PY=S*Y; // the feature's own (east-half) frame
			// The knoll of map I inside the town's east edge, overlooking the avenue (lookout). Its foot (40m) stays clear of the pads
			// of the three houses around it (a 50m foot put them into its flanks: 43% at the cuts).
			H+=Bump(FVector2D::Distance(FVector2D(PX,PY),FVector2D(22500,7500)),600.,4000.);
			// The ridge of map I south of the south road: a crest from x = 185m to 215m, its foot inside the ridge's sectors and clear of
			// the house west of them (a longer crest put that house's pad into the flank: 51% at the cut).
			H+=Bump(SegmentDistance(PX,PY,18500,-29500,21500,-29500),1000.,6000.);
			// Broad rises: the north fields beyond the north road, and the town between the avenue and the south road.
			H+=Bump(FVector2D::Distance(FVector2D(PX,PY),FVector2D(36000,27000)),450.,9000.);
			H+=Bump(FVector2D::Distance(FVector2D(PX,PY),FVector2D(12500,-6500)),300.,8000.);
			// A shallow valley along the river (down to the pond at its south end).
			const double RiverMid=(RiverX0+RiverX1)*.5;
			const double Along=PY<RiverY0?FMath::Max(0.,1.-(RiverY0-PY)/4000.):1.;
			H-=Along*Bump(FMath::Abs(PX-RiverMid),300.,3200.);
		}
		H=FMath::Max(H,0.);
		// Home bases: flat at 0 within 12m of the starting area (x beyond 340m, |y| up to 120m), rising over the next 90m.
		for(const double S:{1.,-1.})
		{
			const double PX=S*X,PY=S*Y;
			const double DX=FMath::Max(0.,34000.-PX),DY=FMath::Max(0.,FMath::Abs(PY)-12000.);
			H*=Smooth(FMath::Sqrt(DX*DX+DY*DY)/9000.);
		}
		return H;
	}
	inline BattleTerrain::FHeightField MakeField(FVector2D Half)
	{
		BattleTerrain::FHeightField F;F.Step=GridStep;F.Origin=-Half;
		F.NX=FMath::RoundToInt(2*Half.X/GridStep)+1;F.NY=FMath::RoundToInt(2*Half.Y/GridStep)+1;F.Z.SetNumUninitialized(F.NX*F.NY);
		for(int32 J=0;J<F.NY;++J)for(int32 I=0;I<F.NX;++I)F.Z[J*F.NX+I]=float(Raw(F.Origin.X+I*GridStep,F.Origin.Y+J*GridStep));
		// Pads: the ground at the middle of the footprint, flat over the footprint and its margin, held within PadSlope of that height
		// outside (the slope of the result is at most the larger of the ground's and PadSlope). Both halves, one after the other.
		for(const FPad& P:Pads())for(const double S:{1.,-1.})
		{
			const double CX=S*P.X,CY=S*P.Y,Level=Raw(CX,CY),HX=P.W*.5+PadMargin,HY=P.H*.5+PadMargin;
			const int32 I0=FMath::Max(0,FMath::FloorToInt((CX-HX-PadReach-F.Origin.X)/GridStep)),I1=FMath::Min(F.NX-1,FMath::CeilToInt((CX+HX+PadReach-F.Origin.X)/GridStep));
			const int32 J0=FMath::Max(0,FMath::FloorToInt((CY-HY-PadReach-F.Origin.Y)/GridStep)),J1=FMath::Min(F.NY-1,FMath::CeilToInt((CY+HY+PadReach-F.Origin.Y)/GridStep));
			for(int32 J=J0;J<=J1;++J)for(int32 I=I0;I<=I1;++I)
			{
				const double DX=FMath::Max(0.,FMath::Abs(F.Origin.X+I*GridStep-CX)-HX),DY=FMath::Max(0.,FMath::Abs(F.Origin.Y+J*GridStep-CY)-HY);
				const double Allowed=PadSlope*FMath::Sqrt(DX*DX+DY*DY);
				float& Z=F.Z[J*F.NX+I];Z=float(Level+FMath::Clamp(double(Z)-Level,-Allowed,Allowed));
			}
		}
		// Exactly point symmetric: the order of the pads above can differ by a little where the slope limits of two pads meet, so every
		// point of the west half takes the height of its 180-degree partner.
		for(int32 Index=0;Index<F.Z.Num();++Index)if(const int32 Partner=F.Z.Num()-1-Index;Partner<Index)F.Z[Index]=F.Z[Partner];
		return F;
	}
	inline BattleTerrain::FHeights MakeHeights(FVector2D Half)
	{
		BattleTerrain::FHeights T;T.Field=MakeField(Half);
		// Houses on their pads; no hills as patches (the hills are part of the field).
		for(const BattleTerrain::FHouse& B:Houses())BattleTerrain::AddHouse(T,B,T.Field.Height(B.X,B.Y));
		const int32 East=T.Patches.Num();
		for(int32 I=0;I<East;++I)T.Patches.Add(T.Patches[I].Rotated());
		T.Build(Half);return T;
	}
	// Objectives of map I at the height of the ground there.
	inline TArray<FVector> Objectives(const BattleTerrain::FHeightField& Field)
	{
		TArray<FVector> L=BattleMapI::Objectives();
		for(FVector& P:L)P.Z=Field.Height(P.X,P.Y);
		return L;
	}
}
