// Layout of map I ("sectors-v1", 2026-11-19): every sector of BattleMapI.h is filled by its kind. Included in
// BattleMapCommandlet.cpp where Box, Emit(Label,X,Y,SizeX,SizeY,Height,bConcealment), Piece(Label,X,Y,Z0,SizeX,SizeY,Height,
// bTerrain) and Ramp(...) exist; Emit, Piece and Ramp place the 180-degree partner too, so only the east half is listed.
// Nothing stands within 4m of a sector border (the roads run there); borders that are no road get a wall or a hedge with 15m
// gaps at the corners. Units: cm.
{
	UMaterial* const Grey=WallMat;
	UMaterial* HouseMat=Material(TEXT("M_MapHouse"),FLinearColor(.46f,.38f,.28f));
	UMaterial* HillMat=Material(TEXT("M_MapHill"),FLinearColor(.2f,.3f,.14f));
	UMaterial* TreeMat=Material(TEXT("M_MapTree"),FLinearColor(.05f,.2f,.06f));
	UMaterial* TrunkMat=Material(TEXT("M_MapTrunk"),FLinearColor(.2f,.13f,.07f));
	UMaterial* WaterMat=Material(TEXT("M_MapWater"),FLinearColor(.05f,.2f,.45f));
	UMaterial* RoadMat=Material(TEXT("M_MapRoad"),FLinearColor(.07f,.08f,.09f));
	UMaterial* FieldMat=Material(TEXT("M_MapField"),FLinearColor(.3f,.27f,.12f));
	UMaterial* BridgeMat=Material(TEXT("M_MapBridge"),FLinearColor(.3f,.22f,.14f));
	// Emit, Piece and Ramp take their material from WallMat (captured by reference): set it before a group of pieces.
	auto Use=[&](UMaterial* Mat){WallMat=Mat;};
	using namespace BattleMapI;
	// Ground paint (no collision): roads, fields, water.
	for(const FRoad& R:Roads())
		Box(TEXT("Road"),R.bAlongX?FVector((R.From+R.To)*.5,R.At,1.5):FVector(R.At,(R.From+R.To)*.5,1.5),R.bAlongX?FVector(R.To-R.From,800,3):FVector(800,R.To-R.From,3),RoadMat,false);
	for(int32 C=0;C<Columns;++C)for(int32 R=0;R<Rows;++R)if(Kind(C,R)==TEXT('F'))
	{const FVector2D M=Centre(C,R);Box(TEXT("FieldGround"),FVector(M.X,M.Y,1),FVector(Cell-900,Cell-900,2),FieldMat,false);}
	for(int32 Sign:{-1,1})
	{
		Box(TEXT("River"),FVector(Sign*(RiverX0+RiverX1)*.5,Sign*(RiverY0+RiverY1)*.5,2),FVector(RiverX1-RiverX0,RiverY1-RiverY0,4),WaterMat,false);
		Box(TEXT("BridgeDeck"),FVector(Sign*(RiverX0+RiverX1)*.5,Sign*BridgeY,4),FVector(RiverX1-RiverX0+400,BridgeHalf*2,6),BridgeMat,false);
		Box(TEXT("Ford"),FVector(Sign*(RiverX0+RiverX1)*.5,Sign*FordY,4),FVector(RiverX1-RiverX0+200,FordHalf*2,6),FieldMat,false);
	}
	// Two-storey houses (the shapes of BattleTerrain.h, as on map G): outer walls in sections with a door on the south and east
	// sides of the ground floor and window sills in every second section, stairs along the north wall, an upper floor without a
	// roof, a crate on each floor.
	Use(HouseMat);
	for(const BattleTerrain::FHouse& B:Houses())
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
	// Hills: a flat top with a ramp on each side, low walls on top along the two long edges (60cm thick: no navigation island).
	Use(HillMat);
	for(const BattleTerrain::FHill& H:Hills())
	{
		const double X0=H.X-H.W*.5,X1=H.X+H.W*.5,Y0=H.Y-H.H*.5,Y1=H.Y+H.H*.5;
		Piece(TEXT("HillTop"),H.X,H.Y,0,H.W,H.H,H.Top,true);
		Ramp(TEXT("HillSide"),X0-H.Run,X0,Y0,Y1,true,true,H.Top);Ramp(TEXT("HillSide"),X1,X1+H.Run,Y0,Y1,true,false,H.Top);
		Ramp(TEXT("HillSide"),X0,X1,Y0-H.Run,Y0,false,true,H.Top);Ramp(TEXT("HillSide"),X0,X1,Y1,Y1+H.Run,false,false,H.Top);
	}
	Use(Grey);
	for(const BattleTerrain::FHill& H:Hills())
	{
		const bool bLongX=H.W>=H.H;
		for(int32 Side:{-1,1}) for(int32 I=-1;I<=1;++I)
		{
			if(bLongX)Piece(TEXT("HillWall"),H.X+I*H.W*.3,H.Y+Side*(H.H*.5-150),H.Top,500,60,130);
			else Piece(TEXT("HillWall"),H.X+Side*(H.W*.5-150),H.Y+I*H.H*.3,H.Top,60,500,130);
		}
	}
	// A warehouse: one storey, 3.5m walls without a roof, two doors in the south wall and one in each end wall, crates inside.
	auto Warehouse=[&](double X,double Y,double W,double H)
	{
		Use(HouseMat);
		const double T=50,X0=X-W*.5,X1=X+W*.5,Y0=Y-H*.5,Y1=Y+H*.5,Door=400,Third=(W-2*Door)/3;
		Piece(TEXT("WarehouseWall"),X,Y1-T*.5,0,W,T,350);
		for(int32 I=0;I<3;++I)Piece(TEXT("WarehouseWall"),X0+Third*.5+I*(Third+Door),Y0+T*.5,0,Third,T,350);
		for(int32 Side:{-1,1})for(int32 I=0;I<2;++I)
		{const double L=(H-2*T-Door)*.5;Piece(TEXT("WarehouseWall"),Side>0?X1-T*.5:X0+T*.5,Y0+T+L*.5+I*(L+Door),0,T,L,350);}
		Use(Grey);
		Piece(TEXT("WarehouseCrate"),X-W*.25,Y+H*.1,0,160,160,130);Piece(TEXT("WarehouseCrate"),X+W*.25,Y-H*.1,0,160,160,130);
	};
	// The objective cover of map E: low walls on four sides inside the 4.5m capture radius and crates at the corners.
	auto ObjectiveCover=[&](double X,double Y)
	{
		for(int32 Side:{-1,1})
		{
			Emit(TEXT("PlazaWall"),X,Y+Side*380,300,60,120);Emit(TEXT("PlazaWall"),X+Side*380,Y,60,300,120);
			for(int32 Other:{-1,1})Emit(TEXT("PlazaCrate"),X+Side*650,Y+Other*650,140,140,130);
		}
	};
	FRandomStream Random(20261119);
	for(int32 C=Columns/2;C<Columns;++C)for(int32 R=0;R<Rows;++R)
	{
		const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);const double X=M.X,Y=M.Y;
		const int32 Mix=(C*7+R*13)%4;const double Flip=(C+R)%2?1.:-1.;
		Use(Grey);
		switch(K)
		{
		case TEXT('H'):
			// The house stands a little north-west of the middle; yard walls on all four sides, a car, a shed (not enterable), crates.
			Emit(TEXT("YardWall"),X-300+Flip*500,Y-1350,700,60,130);Emit(TEXT("YardWall"),X+1500,Y+300+Flip*400,60,700,130);
			Emit(TEXT("YardWall"),X-2000,Y+500-Flip*400,60,700,130);Emit(TEXT("YardWall"),X+300-Flip*500,Y+1950,700,60,130);
			Emit(TEXT("YardCar"),X+Flip*1300,Y-1800,450,200,150);
			Emit(TEXT("YardCrate"),X-Flip*1500,Y-1700,140,140,130);Emit(TEXT("YardCrate"),X+1900,Y-700,140,140,130);
			if(Mix<2)Emit(TEXT("Shed"),X+1500,Y+1500,450,350,260);else Emit(TEXT("YardWall"),X+1300,Y+1500,60,600,130);
			break;
		case TEXT('P'):
			Emit(TEXT("Fountain"),X,Y,300,300,150);
			for(int32 Side:{-1,1})
			{
				Emit(TEXT("PlazaWall"),X+Flip*300,Y+Side*1300,500,60,130);Emit(TEXT("PlazaWall"),X+Side*1300,Y-Flip*300,60,500,130);
				for(int32 Other:{-1,1})Emit(TEXT("PlazaCar"),X+Side*1750,Y+Other*1850,(Side*Other>0)?450:200,(Side*Other>0)?200:450,150);
			}
			Emit(TEXT("Kiosk"),X+Flip*800,Y+800,250,250,260);Emit(TEXT("PlazaCrate"),X-Flip*800,Y-800,140,140,130);
			break;
		case TEXT('O'):
			ObjectiveCover(X,Y);
			Emit(TEXT("YardWall"),X-1600,Y-1500,700,60,130);Emit(TEXT("YardWall"),X-1700,Y+1200,60,700,130);
			Emit(TEXT("YardWall"),X+1500,Y-1600,60,700,130);Emit(TEXT("YardWall"),X-600,Y+1900,700,60,130);
			Emit(TEXT("YardCar"),X+1700,Y-300,200,450,150);Emit(TEXT("YardCar"),X-300,Y-1900,450,200,150);
			break;
		case TEXT('D'):
			ObjectiveCover(X,Y);
			Warehouse(X,Y+1450,2600,1100);Use(Grey);
			Emit(TEXT("Container"),X-1500,Y-1500,600,250,260);Emit(TEXT("Container"),X+1600,Y-1300,250,600,260);
			Emit(TEXT("DepotWall"),X-1800,Y+200,60,700,130);Emit(TEXT("DepotWall"),X+1800,Y+100,60,700,130);
			Emit(TEXT("DepotWall"),X+200,Y-1900,700,60,130);Emit(TEXT("DepotCrate"),X-600,Y-1200,160,160,130);
			break;
		case TEXT('W'):
			Warehouse(X,Y+300,2800,1800);Use(Grey);
			Emit(TEXT("Container"),X-1400,Y-1500,600,250,260);Emit(TEXT("Container"),X+1300,Y-1600,600,250,260);
			Emit(TEXT("DepotWall"),X-1900,Y+300,60,700,130);Emit(TEXT("DepotWall"),X+1900,Y+300,60,700,130);
			Emit(TEXT("DepotWall"),X,Y-1950,700,60,130);Emit(TEXT("DepotWall"),X,Y+1800,700,60,130);
			break;
		case TEXT('F'):
			// Open on purpose: up to two hay bales.
			for(int32 I=0;I<Mix%3;++I)Emit(TEXT("HayBale"),X+Random.FRandRange(-1700.,1700.),Y+Random.FRandRange(-1700.,1700.),220,220,150);
			break;
		case TEXT('T'):
			// A wood: trunks on a jittered 10m grid (they stop bullets), bushes that only conceal, two fallen trees as low cover.
			for(int32 GX=0;GX<4;++GX)for(int32 GY=0;GY<4;++GY)
			{
				const double TX=X-1500+GX*1000+Random.FRandRange(-300.,300.),TY=Y-1500+GY*1000+Random.FRandRange(-300.,300.);
				if(Random.FRand()<.2)continue;
				Use(TrunkMat);Emit(TEXT("Tree"),TX,TY,70,70,700);
				for(int32 Sign:{-1,1})Box(TEXT("TreeCrown"),FVector(TX*Sign,TY*Sign,600),FVector(380,380,260),TreeMat,false);
			}
			Use(TreeMat);
			for(int32 I=0;I<3;++I)Emit(TEXT("Hedge"),X+Random.FRandRange(-1800.,1800.),Y+Random.FRandRange(-1800.,1800.),I%2?450:150,I%2?150:450,220,true);
			Use(TrunkMat);
			Emit(TEXT("FallenTree"),X+Flip*900,Y-1900,500,80,130);Emit(TEXT("FallenTree"),X-1900,Y+Flip*900,80,500,130);
			Emit(TEXT("FallenTree"),X-Flip*900,Y+1900,500,80,130);Emit(TEXT("FallenTree"),X+1900,Y-Flip*900,80,500,130);
			break;
		case TEXT('R'):
			// River banks: rocks and reeds beside the water, away from the bridge and the ford.
			for(double Off:{-1300.,1300.})
			{
				const double BY=Y+Off;if(FMath::Abs(BY-BridgeY)<1800||FMath::Abs(BY-FordY)<900)continue;
				Emit(TEXT("RiverRock"),RiverX0-450,BY,220,220,150);Emit(TEXT("RiverRock"),RiverX1+450,BY+Flip*500,220,220,150);
				Use(TreeMat);Emit(TEXT("Hedge"),RiverX0-250,BY+900*Flip,100,700,220,true);Use(Grey);
			}
			break;
		case TEXT('S'):
			// Home base: sandbag walls clear of the starting columns (x = 390..408m).
			if(C==17){Emit(TEXT("HomeYardWall"),X-1300,Y,100,2000,130);}
			else if(C==19){Emit(TEXT("HomeYardWall"),X,Y+Flip*1000,100,1600,130);Emit(TEXT("HomeCrate"),X+800,Y-Flip*1200,160,160,130);}
			else Emit(TEXT("HomeYardWall"),X+1800,Y+Flip*1500,100,1200,130);
			break;
		default: break; // hills are built above
		}
		// Borders (north and east edge of every sector; the west edge on the x = 0 line): a wall or a hedge where no road runs,
		// leaving the corners open. None beside a hill, a river sector or a home base.
		auto Open=[](TCHAR A,TCHAR B){auto Skip=[](TCHAR T){return T==TEXT('h')||T==TEXT('R')||T==TEXT('S')||T==TEXT(' ');};return Skip(A)||Skip(B);};
		auto Border=[&](bool bAlongX,double At,double Mid,int32 Variant)
		{
			if(OnRoad(bAlongX,At,Mid))return;
			const bool bHedge=Variant%3==0;Use(bHedge?TreeMat:Grey);
			if(bAlongX)Emit(bHedge?TEXT("Hedge"):TEXT("StoneWall"),Mid,At,2000,bHedge?100:60,bHedge?220:130,bHedge);
			else Emit(bHedge?TEXT("Hedge"):TEXT("StoneWall"),At,Mid,bHedge?100:60,2000,bHedge?220:130,bHedge);
		};
		if(R+1<Rows&&!Open(K,Kind(C,R+1)))Border(true,Y+Cell*.5,X,C+2*R);
		if(C+1<Columns&&!Open(K,Kind(C+1,R)))Border(false,X+Cell*.5,Y,2*C+R+1);
		if(C==Columns/2&&Y>0&&!Open(K,Kind(C-1,R)))Border(false,0,Y,R);
	}
	Use(Grey);
	// Bridge railings (cover on the bridge) on both sides of the deck.
	for(int32 Side:{-1,1})Emit(TEXT("BridgeRail"),(RiverX0+RiverX1)*.5,BridgeY+Side*(BridgeHalf+30),RiverX1-RiverX0,40,110);
	UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_LAYOUT objects=%d (x2 by rotation) layout=sectors-v1"),Objects);
}
