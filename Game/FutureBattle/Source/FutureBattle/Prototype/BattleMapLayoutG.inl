// What map G ("heights-v1", 2026-11-18) adds to the layout of map F. Included at the end of BattleMapLayoutF.inl, where
// Piece(Label,X,Y,Z0,SizeX,SizeY,Height,bTerrain) places a box that stands on Z0 and Ramp(Label,X0,X1,Y0,Y1,bAlongX,
// bRisesPositive,Top) a ramp, each with its 180-degree partner. The shapes come from BattleTerrain.h (HousesG, HillsG), which
// also gives the ground heights the soldiers follow, so the geometry and the heights cannot drift apart. Units: cm.
{
	// Two-storey houses: outer walls in sections (a door on the south and east sides of the ground floor, window sills in every
	// second section on both floors, the north wall closed), stairs along the north wall, an upper floor without a roof (the
	// tactical camera looks in from above), a crate on each floor.
	for(const BattleTerrain::FHouse& B:BattleTerrain::HousesG())
	{
		const BattleTerrain::FHouseShape S(B);const double T=BattleTerrain::WallThick,F=BattleTerrain::FloorZ;
		const double X0=B.X-B.W*.5,X1=B.X+B.W*.5,Y0=B.Y-B.H*.5,Y1=B.Y+B.H*.5;
		Piece(TEXT("UpperFloor"),(S.W0+S.E0)*.5,(S.S0y+S.SY)*.5,F-20,S.E0-S.W0,S.SY-S.S0y,20,true);
		Piece(TEXT("UpperFloor"),(S.S1+S.E0)*.5,(S.SY+S.N0y)*.5,F-20,S.E0-S.S1,S.N0y-S.SY,20,true);
		Ramp(TEXT("Stairs"),S.S0,S.S1,S.SY,S.N0y,true,true,F);
		const int32 NX=FMath::Max(3,FMath::RoundToInt(B.W/230.)),NY=FMath::Max(3,FMath::RoundToInt((B.H-2*T)/230.));
		for(int32 Storey=0;Storey<2;++Storey)
		{
			const double Z=Storey*F,Full=Storey?250:F;
			Piece(TEXT("HouseWall"),B.X,Y1-T*.5,Z,B.W,T,Full);
			for(int32 I=0;I<NX;++I)
			{
				if(!Storey&&I==NX/2)continue; // door
				const double L=B.W/NX;const bool bWindow=I%2==1;
				Piece(bWindow?TEXT("HouseSill"):TEXT("HouseWall"),X0+L*(I+.5),Y0+T*.5,Z,L,T,bWindow?130:Full);
			}
			for(int32 Side:{-1,1}) for(int32 I=0;I<NY;++I)
			{
				if(!Storey&&Side>0&&I==NY/2)continue; // door
				const double L=(B.H-2*T)/NY;const bool bWindow=I%2==1;
				Piece(bWindow?TEXT("HouseSill"):TEXT("HouseWall"),Side>0?X1-T*.5:X0+T*.5,Y0+T+L*(I+.5),Z,T,L,bWindow?130:Full);
			}
		}
		Piece(TEXT("HouseCrate"),B.X+B.W*.25,B.Y-B.H*.2,0,130,130,130);
		Piece(TEXT("UpperCrate"),B.X-B.W*.25,B.Y-B.H*.15,F,130,130,130);
	}
	// Hills: a flat top with a ramp on each side; on top low walls along the two long edges (60cm thick, so the navigation mesh
	// gets no island on them).
	for(const BattleTerrain::FHill& H:BattleTerrain::HillsG())
	{
		const double X0=H.X-H.W*.5,X1=H.X+H.W*.5,Y0=H.Y-H.H*.5,Y1=H.Y+H.H*.5;
		Piece(TEXT("HillTop"),H.X,H.Y,0,H.W,H.H,H.Top,true);
		Ramp(TEXT("HillSide"),X0-H.Run,X0,Y0,Y1,true,true,H.Top);Ramp(TEXT("HillSide"),X1,X1+H.Run,Y0,Y1,true,false,H.Top);
		Ramp(TEXT("HillSide"),X0,X1,Y0-H.Run,Y0,false,true,H.Top);Ramp(TEXT("HillSide"),X0,X1,Y1,Y1+H.Run,false,false,H.Top);
		const bool bLongX=H.W>=H.H;
		for(int32 Side:{-1,1}) for(int32 I=-1;I<=1;++I)
		{
			if(bLongX)Piece(TEXT("HillWall"),H.X+I*H.W*.3,H.Y+Side*(H.H*.5-150),H.Top,500,60,130);
			else Piece(TEXT("HillWall"),H.X+Side*(H.W*.5-150),H.Y+I*H.H*.3,H.Top,60,500,130);
		}
	}
	// More cover than on map F along the roads: low walls beside the avenue outside the city and beside the flank roads, on
	// alternating sides every 50m (never on a junction).
	for(int32 I=0;I<4;++I){const double X=27000+I*2500;Piece(TEXT("RoadsideWall"),X,(I%2?1:-1)*1500,0,500,60,130);}
	for(int32 SideY:{-1,1}) for(int32 I=0;I<9;++I)
	{
		const double X=2500+I*5000;
		if(FMath::Abs(X-13000)<1500||FMath::Abs(X-25000)<1500||FMath::Abs(X-38000)<1500)continue;
		Piece(TEXT("RoadsideWall"),X,SideY*25000+(I%2?-900:900),0,500,60,130);
	}
}
