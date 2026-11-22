// Layout of map J ("terrain-j-v1", 2026-11-22, Docs/design-map-j-terrain.md): every sector of BattleMapJ.h is filled by its kind,
// with one of several layouts and one of eight orientations per sector (BattleMapJ::Variant / Mirror), so that the town does not
// repeat itself (user request 2026-11-22: "the maps are regular with little variety; diversify them, buildings included").
// Included in BattleMapCommandlet.cpp where Box, Emit, Piece, Ramp and Material exist; Emit and Piece place the 180-degree partner
// too, so only the east half is listed. The middle of every sector that cannot be entered stays free (a sector order goes to the
// middle), nothing stands within 4.5m of a border except mountains (the roads run there). Units: cm.
{
	UMaterial* const GreyJ=WallMat;
	UMaterial* HouseMatJ=Material(TEXT("M_MapHouse"),FLinearColor(.46f,.38f,.28f));
	UMaterial* BrickMatJ=Material(TEXT("M_MapBrick"),FLinearColor(.42f,.2f,.14f));
	UMaterial* TowerMatJ=Material(TEXT("M_MapTower"),FLinearColor(.5f,.55f,.6f));
	UMaterial* FlatMatJ=Material(TEXT("M_MapFlats"),FLinearColor(.62f,.58f,.5f));
	UMaterial* ChurchMatJ=Material(TEXT("M_MapChurch"),FLinearColor(.66f,.62f,.52f));
	UMaterial* RustMatJ=Material(TEXT("M_MapRust"),FLinearColor(.36f,.2f,.1f));
	UMaterial* RuinMatJ=Material(TEXT("M_MapRuin"),FLinearColor(.3f,.29f,.27f));
	UMaterial* HillMatJ=Material(TEXT("M_MapHill"),FLinearColor(.2f,.3f,.14f));
	UMaterial* RockMatJ=Material(TEXT("M_MapRock"),FLinearColor(.33f,.31f,.28f));
	UMaterial* SnowMatJ=Material(TEXT("M_MapRockTop"),FLinearColor(.5f,.48f,.45f));
	UMaterial* TreeMatJ=Material(TEXT("M_MapTree"),FLinearColor(.05f,.2f,.06f));
	UMaterial* TrunkMatJ=Material(TEXT("M_MapTrunk"),FLinearColor(.2f,.13f,.07f));
	UMaterial* WaterMatJ=Material(TEXT("M_MapWater"),FLinearColor(.05f,.2f,.45f));
	UMaterial* RoadMatJ=Material(TEXT("M_MapRoad"),FLinearColor(.07f,.08f,.09f));
	UMaterial* FieldMatJ=Material(TEXT("M_MapField"),FLinearColor(.3f,.27f,.12f));
	UMaterial* GrassMatJ=Material(TEXT("M_MapGrass"),FLinearColor(.12f,.24f,.08f));
	UMaterial* PaveMatJ=Material(TEXT("M_MapPave"),FLinearColor(.2f,.2f,.21f));
	// Emit and Piece take their material from WallMat (captured by reference): set it before a group of pieces.
	auto UseJ=[&](UMaterial* NewMat){WallMat=NewMat;};
	// Ground paint (no collision): roads, fields, parks, car parks, lakes.
	for(const BattleMapJ::FRoad& Rd:BattleMapJ::Roads())
		Box(TEXT("Road"),Rd.bAlongX?FVector((Rd.From+Rd.To)*.5,Rd.At,1.5):FVector(Rd.At,(Rd.From+Rd.To)*.5,1.5),Rd.bAlongX?FVector(Rd.To-Rd.From,800,3):FVector(800,Rd.To-Rd.From,3),RoadMatJ,false);
	for(int32 Cx=0;Cx<BattleMapJ::Columns;++Cx)for(int32 Rx=0;Rx<BattleMapJ::Rows;++Rx)
	{
		const TCHAR Kx=BattleMapJ::Kind(Cx,Rx);const FVector2D Mx=BattleMapJ::Centre(Cx,Rx);
		if(Kx==TEXT('F'))Box(TEXT("FieldGround"),FVector(Mx.X,Mx.Y,1),FVector(BattleMapJ::Cell-900,BattleMapJ::Cell-900,2),FieldMatJ,false);
		else if(Kx==TEXT('G'))Box(TEXT("ParkGround"),FVector(Mx.X,Mx.Y,1),FVector(BattleMapJ::Cell-900,BattleMapJ::Cell-900,2),GrassMatJ,false);
		else if(Kx==TEXT('Y')||Kx==TEXT('P'))Box(TEXT("PavedGround"),FVector(Mx.X,Mx.Y,1),FVector(BattleMapJ::Cell-900,BattleMapJ::Cell-900,2),PaveMatJ,false);
		else if(Kx==TEXT('L'))Box(TEXT("Lake"),FVector(Mx.X,Mx.Y,2),FVector(BattleMapJ::Cell,BattleMapJ::Cell,4),WaterMatJ,false);
	}
	// Two-storey houses (the shapes of BattleTerrain.h, as on maps G and I): walls in sections, doors on the south and east sides of
	// the ground floor, window sills in every second section, stairs along the north wall, an upper floor without a roof, crates.
	for(const BattleTerrain::FHouse& Hs:BattleMapJ::Houses())
	{
		UseJ((BattleMapJ::Hash(int32(Hs.X),int32(Hs.Y),7)%3)==0?BrickMatJ:HouseMatJ);
		const BattleTerrain::FHouseShape Sh(Hs);const double Th=BattleTerrain::WallThick,Fz=BattleTerrain::FloorZ;
		const double HX0=Hs.X-Hs.W*.5,HX1=Hs.X+Hs.W*.5,HY0=Hs.Y-Hs.H*.5,HY1=Hs.Y+Hs.H*.5;
		Piece(TEXT("UpperFloor"),(Sh.W0+Sh.E0)*.5,(Sh.S0y+Sh.SY)*.5,Fz-20,Sh.E0-Sh.W0,Sh.SY-Sh.S0y,20,true);
		Piece(TEXT("UpperFloor"),(Sh.S1+Sh.E0)*.5,(Sh.SY+Sh.N0y)*.5,Fz-20,Sh.E0-Sh.S1,Sh.N0y-Sh.SY,20,true);
		Ramp(TEXT("Stairs"),Sh.S0,Sh.S1,Sh.SY,Sh.N0y,true,true,Fz);
		const int32 NX=FMath::Max(3,FMath::RoundToInt(Hs.W/230.)),NY=FMath::Max(3,FMath::RoundToInt((Hs.H-2*Th)/230.));
		for(int32 Storey=0;Storey<2;++Storey)
		{
			const double Zs=Storey*Fz,Full=Storey?250:Fz;
			Piece(TEXT("HouseWall"),Hs.X,HY1-Th*.5,Zs,Hs.W,Th,Full);
			for(int32 Ix=0;Ix<NX;++Ix)
			{
				if(!Storey&&Ix==NX/2)continue; // door
				const double Ln=Hs.W/NX;const bool bWindow=Ix%2==1;
				Piece(bWindow?TEXT("HouseSill"):TEXT("HouseWall"),HX0+Ln*(Ix+.5),HY0+Th*.5,Zs,Ln,Th,bWindow?130:Full);
			}
			for(int32 Side:{-1,1}) for(int32 Ix=0;Ix<NY;++Ix)
			{
				if(!Storey&&Side>0&&Ix==NY/2)continue; // door
				const double Ln=(Hs.H-2*Th)/NY;const bool bWindow=Ix%2==1;
				Piece(bWindow?TEXT("HouseSill"):TEXT("HouseWall"),Side>0?HX1-Th*.5:HX0+Th*.5,HY0+Th+Ln*(Ix+.5),Zs,Th,Ln,bWindow?130:Full);
			}
		}
		Piece(TEXT("HouseCrate"),Hs.X+Hs.W*.25,Hs.Y-Hs.H*.2,0,130,130,130);
		Piece(TEXT("UpperCrate"),Hs.X-Hs.W*.25,Hs.Y-Hs.H*.15,Fz,130,130,130);
	}
	// Hills: a flat top with a ramp on each side, low walls on top along the two long edges.
	UseJ(HillMatJ);
	for(const BattleTerrain::FHill& Hl:BattleMapJ::Hills())
	{
		const double LX0=Hl.X-Hl.W*.5,LX1=Hl.X+Hl.W*.5,LY0=Hl.Y-Hl.H*.5,LY1=Hl.Y+Hl.H*.5;
		Piece(TEXT("HillTop"),Hl.X,Hl.Y,0,Hl.W,Hl.H,Hl.Top,true);
		Ramp(TEXT("HillSide"),LX0-Hl.Run,LX0,LY0,LY1,true,true,Hl.Top);Ramp(TEXT("HillSide"),LX1,LX1+Hl.Run,LY0,LY1,true,false,Hl.Top);
		Ramp(TEXT("HillSide"),LX0,LX1,LY0-Hl.Run,LY0,false,true,Hl.Top);Ramp(TEXT("HillSide"),LX0,LX1,LY1,LY1+Hl.Run,false,false,Hl.Top);
	}
	UseJ(GreyJ);
	for(const BattleTerrain::FHill& Hl:BattleMapJ::Hills())
	{
		const bool bLongX=Hl.W>=Hl.H;
		for(int32 Side:{-1,1}) for(int32 Ix=-1;Ix<=1;++Ix)
		{
			if(bLongX)Piece(TEXT("HillWall"),Hl.X+Ix*Hl.W*.3,Hl.Y+Side*(Hl.H*.5-150),Hl.Top,500,60,130);
			else Piece(TEXT("HillWall"),Hl.X+Side*(Hl.W*.5-150),Hl.Y+Ix*Hl.H*.3,Hl.Top,60,500,130);
		}
	}
	FRandomStream RandomJ(20261122);
	for(int32 Cx=BattleMapJ::Columns/2;Cx<BattleMapJ::Columns;++Cx)for(int32 Rx=0;Rx<BattleMapJ::Rows;++Rx)
	{
		const TCHAR Kx=BattleMapJ::Kind(Cx,Rx);const FVector2D Mx=BattleMapJ::Centre(Cx,Rx);const double SXc=Mx.X,SYc=Mx.Y;
		const BattleMapJ::FMirror Mr=BattleMapJ::Mirror(Cx,Rx);
		const int32 Vr=BattleMapJ::Variant(Cx,Rx,6);const uint32 Hh=BattleMapJ::Hash(Cx,Rx,3);
		// Put: an obstacle given in the sector's own frame (offset from the middle, size), turned by the sector's orientation.
		auto Put=[&](const TCHAR* Lb,double Ox,double Oy,double Sw,double Sd,double Hz,bool bCc=false)
		{const FVector2D Of=Mr.Offset(Ox,Oy),Sz=Mr.Size(Sw,Sd);Emit(Lb,SXc+Of.X,SYc+Of.Y,Sz.X,Sz.Y,Hz,bCc);};
		auto PutOn=[&](const TCHAR* Lb,double Ox,double Oy,double Z0,double Sw,double Sd,double Hz)
		{const FVector2D Of=Mr.Offset(Ox,Oy),Sz=Mr.Size(Sw,Sd);Piece(Lb,SXc+Of.X,SYc+Of.Y,Z0,Sz.X,Sz.Y,Hz);};
		// A tree: a trunk that stops bullets and a crown that is only drawn.
		auto Tree=[&](double Ox,double Oy)
		{
			const FVector2D Of=Mr.Offset(Ox,Oy);UseJ(TrunkMatJ);Emit(TEXT("Tree"),SXc+Of.X,SYc+Of.Y,70,70,700);
			for(int32 Sg:{-1,1})Box(TEXT("TreeCrown"),FVector((SXc+Of.X)*Sg,(SYc+Of.Y)*Sg,600),FVector(380,380,260),TreeMatJ,false);
		};
		// A hall without a roof: walls of Hz cm, doors in the front (south) wall and in each end wall, in the sector's frame.
		auto Hall=[&](double Ox,double Oy,double Sw,double Sd,double Hz,UMaterial* HallMat)
		{
			UseJ(HallMat);
			const double Tw=50,Door=400,Third=(Sw-2*Door)/3;
			Put(TEXT("HallWall"),Ox,Oy+Sd*.5-Tw*.5,Sw,Tw,Hz);
			for(int32 Ix=0;Ix<3;++Ix)Put(TEXT("HallWall"),Ox-Sw*.5+Third*.5+Ix*(Third+Door),Oy-Sd*.5+Tw*.5,Third,Tw,Hz);
			for(int32 Side:{-1,1})for(int32 Ix=0;Ix<2;++Ix)
			{const double Ln=(Sd-2*Tw-Door)*.5;Put(TEXT("HallWall"),Ox+Side*(Sw*.5-Tw*.5),Oy-Sd*.5+Tw+Ln*.5+Ix*(Ln+Door),Tw,Ln,Hz);}
			UseJ(GreyJ);
		};
		// Objective cover: low walls on four sides 9m from the middle, crates at the corners 12m out except the house's corner
		// (an O sector's house stands in the frame's north-east corner) or a built north side (D).
		auto ObjectiveCoverJ=[&](bool bHouseNE,bool bNorthBuilt)
		{
			for(int32 Side:{-1,1})
			{
				if(!(bNorthBuilt&&Side>0))Put(TEXT("PlazaWall"),0,Side*900,500,60,120);
				Put(TEXT("PlazaWall"),Side*900,0,60,500,120);
				for(int32 Other:{-1,1})
				{
					if((bNorthBuilt&&Other>0)||(bHouseNE&&Side>0&&Other>0))continue;
					Put(TEXT("PlazaCrate"),Side*1200,Other*1200,140,140,130);
				}
			}
		};
		auto Car=[&](double Ox,double Oy,bool bAlongX){Put(TEXT("Car"),Ox,Oy,bAlongX?450:200,bAlongX?200:450,150);};
		UseJ(GreyJ);
		switch(Kx)
		{
		case TEXT('H'):
			// Yards around the house (BattleMapJ::HouseOf): walls and a car, a garden with hedges, or a courtyard with a shed.
			if(Vr%3==0)
			{
				Put(TEXT("YardWall"),-300,-1450,800,60,130);Put(TEXT("YardWall"),1600,300,60,800,130);Put(TEXT("YardWall"),-1900,600,60,700,130);
				Car(1300,-1800,true);Put(TEXT("YardCrate"),-1500,-1700,140,140,130);
			}
			else if(Vr%3==1)
			{
				UseJ(TreeMatJ);Put(TEXT("Hedge"),0,-1650,1800,100,220,true);Put(TEXT("Hedge"),1750,0,100,1600,220,true);UseJ(GreyJ);
				Put(TEXT("GardenWall"),-1700,-1200,60,600,130);Tree(1500,-1500);Tree(-1600,1600);
			}
			else
			{
				Put(TEXT("CourtWall"),1300,-1500,1200,60,130);Put(TEXT("CourtWall"),1900,-900,60,1200,130);
				Put(TEXT("Shed"),-1500,-1500,500,400,260);Put(TEXT("Woodpile"),1500,1500,300,120,130);
			}
			break;
		case TEXT('B'):
			// Office towers: one tall tower in a corner, an L of two blocks, or a tower on a podium. Not enterable; they block sight.
			UseJ(TowerMatJ);
			if(Vr%3==0)
			{
				Put(TEXT("Tower"),1250,1200,1500,1300,2400+(Hh%4)*200);UseJ(GreyJ);
				Put(TEXT("Planter"),-1300,-1300,600,200,130);Put(TEXT("Planter"),-1500,800,200,600,130);Car(1300,-1500,true);
			}
			else if(Vr%3==1)
			{
				Put(TEXT("Tower"),0,1500,3600,900,1800);Put(TEXT("TowerWing"),1500,-150,900,2200,2400);UseJ(GreyJ);
				Put(TEXT("Planter"),-1400,-1300,600,200,130);Car(-1200,-1700,true);
			}
			else
			{
				Put(TEXT("Podium"),0,1450,3800,1100,600);PutOn(TEXT("Tower"),-800,1450,600,1400,1000,2200+(Hh%3)*300);UseJ(GreyJ);
				Put(TEXT("Bench"),-900,-1300,400,80,130);Put(TEXT("Kiosk"),1400,-1400,250,250,260);
			}
			break;
		case TEXT('A'):
			// Apartment blocks: a long slab along one side, two blocks in opposite corners, or an L. Not enterable.
			UseJ(FlatMatJ);
			if(Vr%3==0){Put(TEXT("Apartment"),0,1450,3800,1000,1200);UseJ(GreyJ);Car(-1300,-1550,true);Car(0,-1600,true);Put(TEXT("YardWall"),1500,-700,60,800,130);}
			else if(Vr%3==1){Put(TEXT("Apartment"),1200,1300,1400,1300,900);Put(TEXT("Apartment"),-1250,-1250,1300,1400,900);UseJ(GreyJ);Put(TEXT("Playground"),1300,-1300,600,60,130);Put(TEXT("Playground"),-1300,1300,60,600,130);}
			else{Put(TEXT("Apartment"),-1450,200,1000,3200,1000);Put(TEXT("Apartment"),400,1450,1700,1000,1000);UseJ(TreeMatJ);Put(TEXT("Hedge"),1300,-1300,1200,100,220,true);UseJ(GreyJ);Car(1500,-500,false);}
			break;
		case TEXT('C'):
			// Church: a nave and a bell tower (a landmark seen from afar), a churchyard with rows of gravestones and a wall.
			UseJ(ChurchMatJ);
			if(Vr%2==0){Put(TEXT("Nave"),-300,1400,2800,1100,800);Put(TEXT("BellTower"),1500,1400,600,600,2200);}
			else{Put(TEXT("Nave"),-300,1300,2400,1200,900);Put(TEXT("BellTower"),-1650,-1400,600,600,2400);}
			UseJ(GreyJ);
			for(int32 Ix=0;Ix<5;++Ix)for(int32 Row=0;Row<2;++Row)
				if((Hh>>(Ix+5*Row))&1)Put(TEXT("Gravestone"),-1000+Ix*450,-1000-Row*500,60,160,130);
			Put(TEXT("ChurchyardWall"),300,-1950,3200,60,130);
			break;
		case TEXT('K'):
			// Factory: a hall without a roof, a chimney, storage tanks, containers.
			if(Vr%2==0){Hall(0,1350,3600,1300,500,RustMatJ);Put(TEXT("Chimney"),-1600,-1500,350,350,2500);Put(TEXT("StorageTank"),1300,-1300,700,700,500);}
			else{Hall(-1350,0,1300,3400,500,RustMatJ);Put(TEXT("Chimney"),1600,1500,350,350,2800);Put(TEXT("Container"),1300,-1400,600,250,260);}
			if(Vr%2==0)Put(TEXT("Container"),-600,-1700,600,250,260);
			Put(TEXT("PipeRack"),700,-1850,800,80,130);
			break;
		case TEXT('U'):
			// Ruins: the shell of a house with broken walls of varied height, rubble, a burnt-out car.
			UseJ(RuinMatJ);
			{
				// Wall pieces of the shell (north side and both ends), each missing at random or broken to a random height.
				static const double Pieces[6][4]={{-1200,1850,700,50},{-400,1850,700,50},{400,1850,700,50},{-1600,1450,50,700},{1600,1450,50,700},{-1600,650,50,700}};
				for(int32 Ix=0;Ix<6;++Ix)
				{
					const uint32 Hr=BattleMapJ::Hash(Cx,Rx,10+Ix);if(Hr%5==0)continue; // a gap
					Put(TEXT("RuinWall"),Pieces[Ix][0],Pieces[Ix][1],Pieces[Ix][2],Pieces[Ix][3],60+double(Hr%5)*50);
				}
			}
			Put(TEXT("Rubble"),-900,-1300,350,250,110);Put(TEXT("Rubble"),1300,-900,250,350,110);Put(TEXT("Rubble"),-1600,-300,300,300,110);
			UseJ(GreyJ);Car(1200,-1700,true);
			break;
		case TEXT('Y'):
			// Car park: two rows of cars with gaps, lamp posts, a ticket booth.
			for(int32 Ix=0;Ix<6;++Ix)
			{
				if((Hh>>Ix)&1)Car(-1500+Ix*600,1300,false);
				if((Hh>>(Ix+6))&1)Car(-1500+Ix*600,-1300,false);
			}
			Put(TEXT("LampPost"),1850,0,30,30,600);Put(TEXT("LampPost"),-1850,0,30,30,600);Put(TEXT("Booth"),-1800,-1900,250,200,260);
			break;
		case TEXT('G'):
			// Park: trees in a loose ring, hedges, a statue and benches.
			for(int32 Ix=0;Ix<8;++Ix)
			{
				const double An=Ix*UE_PI/4+double(Hh%7)*.1,Rr=1550+double((Hh>>Ix)&3)*120;
				if(((Hh>>(Ix+8))&3)==0)continue;
				Tree(Rr*FMath::Cos(An),Rr*FMath::Sin(An));
			}
			UseJ(TreeMatJ);Put(TEXT("Hedge"),-900,1150,900,100,220,true);Put(TEXT("Hedge"),1100,-1000,100,900,220,true);UseJ(GreyJ);
			Put(TEXT("Statue"),900,900,250,250,300);Put(TEXT("Bench"),-1000,-1000,400,80,130);
			break;
		case TEXT('Q'):
			// A row of shops along one side (not enterable), with an alley in it; cars and bins in front.
			UseJ((Hh%2)?BrickMatJ:HouseMatJ);
			for(int32 Ix=0;Ix<4;++Ix)
			{
				if(Ix==int32(Hh%4))continue; // the alley
				Put(TEXT("Shop"),-1500+Ix*1000,1500,850,800,350+double((Hh>>(Ix*2))&3)*80);
			}
			UseJ(GreyJ);Car(-1000,-1500,true);Car(800,-1600,true);Put(TEXT("Bin"),1700,-900,150,150,130);
			break;
		case TEXT('P'):
			// Plaza: a fountain with walls and cars, a market of stalls, or a statue with trees. The middle stays free.
			if(Vr%3==0)
			{
				Put(TEXT("Fountain"),700,-700,300,300,150);
				for(int32 Side:{-1,1}){Put(TEXT("PlazaWall"),300,Side*1300,500,60,130);Put(TEXT("PlazaWall"),Side*1300,-300,60,500,130);}
				Car(1750,1850,true);Car(-1750,-1850,true);Put(TEXT("Kiosk"),800,800,250,250,260);
				for(const FVector2D Pl:{FVector2D(-1650,1650),FVector2D(1650,-1650),FVector2D(1100,1700),FVector2D(-1100,-1700)})Put(TEXT("Planter"),Pl.X,Pl.Y,200,200,110);
			}
			else if(Vr%3==1)
			{
				for(int32 Ix=0;Ix<6;++Ix){const double An=Ix*UE_PI/3+.3;Put(TEXT("Stall"),1450*FMath::Cos(An),1300*FMath::Sin(An),400,250,220);}
				Put(TEXT("PlazaCrate"),-1800,1700,140,140,130);
				for(const FVector2D Pl:{FVector2D(1800,1800),FVector2D(1800,-1800),FVector2D(-1800,-1800),FVector2D(-1200,1800)})Put(TEXT("Planter"),Pl.X,Pl.Y,200,200,110);
			}
			else
			{
				Put(TEXT("Statue"),-1000,1000,300,300,350);Tree(1300,1300);Tree(-1300,-1300);Tree(1400,-1200);
				Put(TEXT("Bench"),0,-1500,500,80,130);Put(TEXT("PlazaWall"),1600,200,60,700,130);
				for(const FVector2D Pl:{FVector2D(1000,1750),FVector2D(-1750,1750),FVector2D(-1700,-1000),FVector2D(1100,-1800)})Put(TEXT("Planter"),Pl.X,Pl.Y,200,200,110);
			}
			// 2026-11-22: four planters per plaza, off the middle cross (the pass tracks run through it): the plazas at the pass exits
			// had too little low cover (SECTOR_MAP_COVER_SHORT 4-5 per side).
			break;
		case TEXT('O'):
			ObjectiveCoverJ(true,false);
			if(Vr%2==0){Put(TEXT("YardWall"),-1600,-1500,700,60,130);Put(TEXT("YardWall"),-1700,1200,60,700,130);Car(1700,-300,false);Car(-300,-1900,true);}
			else{UseJ(TreeMatJ);Put(TEXT("Hedge"),-1650,-600,100,1400,220,true);UseJ(GreyJ);Put(TEXT("Kiosk"),-1500,1500,250,250,260);Car(400,-1800,true);}
			break;
		case TEXT('D'):
			ObjectiveCoverJ(false,true);
			Hall(0,1450,2600,1100,350,HouseMatJ);
			Put(TEXT("Container"),-1500,-1500,600,250,260);Put(TEXT("Container"),1600,-1300,250,600,260);
			Put(TEXT("DepotWall"),-1800,200,60,700,130);Put(TEXT("DepotWall"),1800,100,60,700,130);Put(TEXT("DepotCrate"),-600,-1200,160,160,130);
			break;
		case TEXT('W'):
			Hall(0,300,2800,1800,350,HouseMatJ);
			Put(TEXT("Container"),-1400,-1500,600,250,260);Put(TEXT("Container"),1300,-1600,600,250,260);
			Put(TEXT("DepotWall"),-1900,300,60,700,130);Put(TEXT("DepotWall"),1900,300,60,700,130);
			break;
		case TEXT('F'):
			// Fields: open on purpose - hay bales, rows of crops that only conceal, or a tractor and trailer.
			if(Vr%3==0){for(int32 Ix=0;Ix<int32(Hh%3);++Ix){const double BX=RandomJ.FRandRange(-1700.,1700.),BY=RandomJ.FRandRange(-1700.,1700.);if(FMath::Abs(BX)>600||FMath::Abs(BY)>600)Emit(TEXT("HayBale"),SXc+BX,SYc+BY,220,220,150);}}
			else if(Vr%3==1){UseJ(TreeMatJ);for(const double Oy:{-1600.,-800.,800.,1600.})Put(TEXT("Crops"),0,Oy,3000,80,110,true);UseJ(GreyJ);}
			else{Put(TEXT("Tractor"),-1300,900,350,200,250);Put(TEXT("Trailer"),-700,900,450,220,150);Put(TEXT("HayBale"),1200,-1200,220,220,150);}
			break;
		case TEXT('T'):
		{
			// A wood: trunks on a jittered 10m grid, bushes that only conceal, fallen trees as low cover. Density varies.
			const float Skip=.15f+.05f*float(Hh%4);
			for(int32 GX=0;GX<4;++GX)for(int32 GY=0;GY<4;++GY)
			{
				const double TX=SXc-1500+GX*1000+RandomJ.FRandRange(-300.,300.),TY=SYc-1500+GY*1000+RandomJ.FRandRange(-300.,300.);
				if(RandomJ.FRand()<Skip)continue;
				UseJ(TrunkMatJ);Emit(TEXT("Tree"),TX,TY,70,70,700);
				for(int32 Sg:{-1,1})Box(TEXT("TreeCrown"),FVector(TX*Sg,TY*Sg,600),FVector(380,380,260),TreeMatJ,false);
			}
			UseJ(TreeMatJ);
			for(int32 Ix=0;Ix<3;++Ix)Emit(TEXT("Hedge"),SXc+RandomJ.FRandRange(-1800.,1800.),SYc+RandomJ.FRandRange(-1800.,1800.),Ix%2?450:150,Ix%2?150:450,220,true);
			UseJ(TrunkMatJ);Put(TEXT("FallenTree"),900,-1900,500,80,130);Put(TEXT("FallenTree"),-1900,900,80,500,130);
			// 2026-11-22: two more fallen trees, one each way (sparse woods had 5 cover points to the north and south).
			Put(TEXT("FallenTree"),-1300,1700,500,80,130);Put(TEXT("FallenTree"),1700,-1300,80,500,130);
			break;
		}
		case TEXT('M'):
		{
			// Mountain: stacked rock with uneven steps, filling the whole sector so that neighbouring mountain sectors join without a
			// gap. Its foot gives cover; the steps are far higher than a soldier can climb (and the sector is cut out of the
			// navigation mesh by BattleMapCommandlet as well). Not marked as ground.
			UseJ(RockMatJ);
			Emit(TEXT("Mountain"),SXc,SYc,BattleMapJ::Cell,BattleMapJ::Cell,500+double(Hh%3)*100);
			const double J1=double(int32(Hh%5)-2)*150,J2=double(int32((Hh>>3)%5)-2)*150;
			Piece(TEXT("Mountain"),SXc+J1,SYc+J2,500,4000,4200,600+double((Hh>>6)%3)*150);
			Piece(TEXT("Mountain"),SXc-J2,SYc+J1,1000,2800+double((Hh>>8)%3)*300,2600,700+double((Hh>>10)%3)*200);
			UseJ(SnowMatJ);
			Piece(TEXT("MountainTop"),SXc+J2*.5,SYc-J1*.5,1600,1300+double((Hh>>12)%3)*200,1200,500+double((Hh>>14)%4)*250);
			break;
		}
		case TEXT('L'):
		{
			// Lake: water (the sector is cut out of the navigation mesh by BattleMapCommandlet), reeds along the shore that only conceal.
			UseJ(TreeMatJ);
			const TCHAR KN=BattleMapJ::Kind(Cx,Rx+1),KS=BattleMapJ::Kind(Cx,Rx-1),KE=BattleMapJ::Kind(Cx+1,Rx),KW=BattleMapJ::Kind(Cx-1,Rx);
			if(KN!=TEXT('L'))Emit(TEXT("Reeds"),SXc-500,SYc+2100,2000,150,200,true);
			if(KS!=TEXT('L'))Emit(TEXT("Reeds"),SXc+500,SYc-2100,2000,150,200,true);
			if(KE!=TEXT('L'))Emit(TEXT("Reeds"),SXc+2100,SYc+300,150,2000,200,true);
			if(KW!=TEXT('L')&&SXc-2100>1.)Emit(TEXT("Reeds"),SXc-2100,SYc-300,150,2000,200,true);
			UseJ(GreyJ);
			break;
		}
		case TEXT('S'):
			// Home base: sandbag walls clear of the starting columns (x = 390..408m).
			if(Cx==17){Emit(TEXT("HomeYardWall"),SXc-1300,SYc,100,2000,130);}
			else if(Cx==19){Emit(TEXT("HomeYardWall"),SXc,SYc+((Cx+Rx)%2?1000:-1000),100,1600,130);Emit(TEXT("HomeCrate"),SXc+800,SYc-((Cx+Rx)%2?1200:-1200),160,160,130);}
			else Emit(TEXT("HomeYardWall"),SXc+1800,SYc+((Cx+Rx)%2?1500:-1500),100,1200,130);
			break;
		default: break; // hills are built above
		}
		// Borders (north and east edge of every sector; the west edge on the x = 0 line): a hedge, a stone wall, a fence of low posts
		// or nothing, where no road runs, leaving the corners open. None beside a hill, a mountain, a lake or a home base.
		auto OpenJ=[](TCHAR A,TCHAR B){auto SkipK=[](TCHAR T){return T==TEXT('h')||T==TEXT('M')||T==TEXT('L')||T==TEXT('S')||T==TEXT(' ');};return SkipK(A)||SkipK(B);};
		auto BorderJ=[&](bool bAlongX,double At,double Mid,uint32 Pick)
		{
			if(BattleMapJ::OnRoad(bAlongX,At,Mid))return;
			const uint32 Kd=Pick%5;if(Kd==4)return; // open
			const bool bHedge=Kd<=1;const bool bPosts=Kd==3;UseJ(bHedge?TreeMatJ:GreyJ);
			const double Len=bPosts?1600:2000,Thick=bHedge?100:60,Hz=bHedge?220:bPosts?110:130;
			if(bAlongX)Emit(bHedge?TEXT("Hedge"):bPosts?TEXT("LowFence"):TEXT("StoneWall"),Mid,At,Len,Thick,Hz,bHedge);
			else Emit(bHedge?TEXT("Hedge"):bPosts?TEXT("LowFence"):TEXT("StoneWall"),At,Mid,Thick,Len,Hz,bHedge);
		};
		auto OpenAt=[&](int32 C2,int32 R2){return OpenJ(Kx,BattleMapJ::Kind(C2,R2))||BattleMapJ::IsPass(Cx,Rx)||BattleMapJ::IsPass(C2,R2);};
		if(Rx+1<BattleMapJ::Rows&&!OpenAt(Cx,Rx+1))BorderJ(true,SYc+BattleMapJ::Cell*.5,SXc,BattleMapJ::Hash(Cx,Rx,20));
		if(Cx+1<BattleMapJ::Columns&&!OpenAt(Cx+1,Rx))BorderJ(false,SXc+BattleMapJ::Cell*.5,SYc,BattleMapJ::Hash(Cx,Rx,21));
		if(Cx==BattleMapJ::Columns/2&&SYc>0&&!OpenAt(Cx-1,Rx))BorderJ(false,0,SYc,BattleMapJ::Hash(Cx,Rx,22));
	}
	UseJ(GreyJ);
	UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_LAYOUT objects=%d (x2 by rotation) layout=terrain-j-v1"),Objects);
}
