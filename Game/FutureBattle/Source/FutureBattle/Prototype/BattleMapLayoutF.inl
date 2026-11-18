// Layout of map F ("districts-v1", 2026-11-18). Included inside UBattleMapCommandlet::Main, where Emit(Label,X,Y,SizeX,SizeY,Height,
// bConcealment) places an obstacle together with its 180-degree partner and accepts only positions in the +X half (or on the +Y
// axis), so everything below describes the east half and the west half follows by rotation. Units: cm.
//
// Roads (kept free of scattered cover; BattleFieldLayout's route nodes sit on them):
//   centre  the avenue y = 0, 24m wide with a clear 14m lane, from home to home through objective C
//   north / south  the flank roads y = +-250m
//   connectors  x = 0, +-130m, +-250m between the city edge and the flank roads; x = +-380m between the avenue and both flank roads
// City districts (east half; the west half is the rotation): old town beside the avenue and in the north inner block, a park in the
// south inner block, warehouses in the south outer blocks, houses in the north outer blocks.
{
	// Share of 25m cells that get a piece of scattered cover (map E: 0.6 on the flanks, 0.45 on the approaches).
	const double FlankChance=UrbanG?.45:.35,ApproachChance=UrbanG?.45:.35;
	auto OnRoad=[](double X,double Y,double Margin)
	{
		const double AX=FMath::Abs(X),AY=FMath::Abs(Y);
		if(AY<1200+Margin&&AX>25000)return true;
		if(FMath::Abs(AY-25000)<600+Margin)return true;
		if(AY>15000&&AY<25000&&(AX<600+Margin||FMath::Abs(AX-13000)<600+Margin||FMath::Abs(AX-25000)<600+Margin))return true;
		if(FMath::Abs(AX-38000)<600+Margin&&AY<25000)return true;
		return false;
	};
	// The four lots of map E's city (house with a yard, ruin, U-shaped building, car park).
	auto Lot=[&](int32 Kind,double CX,double CY,double W,double H)
	{
		switch(((Kind%4)+4)%4)
		{
		case 0:
			// Map G: a two-storey house that can be entered stands here instead (BattleMapLayoutG.inl).
			if(!UrbanG)Emit(TEXT("House"),CX-W*.2,CY,W*.42,H-600,600);
			for(int32 I=0;I<3;++I)
			{
				Emit(TEXT("YardWall"),CX+W*.12+I*550,CY-H*.5+250,300,60,120);
				Emit(TEXT("YardWall"),CX+W*.12+I*550,CY+H*.5-250,300,60,120);
			}
			Emit(TEXT("YardWall"),CX+W*.5-250,CY,60,500,120);
			Emit(TEXT("Crate"),CX+W*.25,CY+250,130,130,130);Emit(TEXT("Crate"),CX+W*.3,CY-300,130,130,130);
			break;
		case 1:
			{
				const double RW=W-900,RH=H-700,Thick=60;
				for(int32 Side:{-1,1}) for(int32 I=0;I<5;++I)
				{
					if(I==2)continue;
					Emit(I%2?TEXT("RuinSill"):TEXT("RuinWall"),CX-RW*.5+RW*(I+.5)/5.,CY+Side*RH*.5,RW/5.,Thick,I%2?110:320);
				}
				for(int32 Side:{-1,1}) for(int32 I=0;I<3;++I)
				{
					if(I==1)continue;
					Emit(TEXT("RuinWall"),CX+Side*RW*.5,CY-RH*.5+RH*(I+.5)/3.,Thick,RH/3.,320);
				}
				Emit(TEXT("RuinPartition"),CX-RW*.17,CY+RH*.12,Thick,RH*.4,320);Emit(TEXT("RuinPartition"),CX+RW*.2,CY-RH*.12,Thick,RH*.4,320);
				Emit(TEXT("Crate"),CX,CY,130,130,130);
			}
			break;
		case 2:
			Emit(TEXT("Wing"),CX,CY+H*.5-650,W-800,700,700);
			Emit(TEXT("Wing"),CX-W*.5+750,CY-150,700,H-1400,700);Emit(TEXT("Wing"),CX+W*.5-750,CY-150,700,H-1400,700);
			Emit(TEXT("CourtWall"),CX-500,CY-H*.5+500,400,60,120);Emit(TEXT("CourtWall"),CX+500,CY-H*.5+500,400,60,120);
			Emit(TEXT("Car"),CX,CY-200,450,200,160);
			break;
		default:
			for(int32 I=0;I<5;++I)Emit(TEXT("Car"),CX-W*.5+700+I*((W-1400)/4.),CY+(I%2?450:-450),200,450,160);
			Emit(TEXT("Barrier"),CX-900,CY,300,60,120);Emit(TEXT("Barrier"),CX+900,CY,300,60,120);
			Emit(TEXT("Kiosk"),CX,CY+H*.5-450,250,250,260);Emit(TEXT("Kiosk"),CX,CY-H*.5+450,250,250,260);
			break;
		}
	};
	// Blocks of the east half. The avenue takes y = -12m..12m, so the two rows beside it are 55m deep, the outer rows 67m.
	const double BlockX[2][2]={{800,12200},{13800,25000}};
	const double BlockY[4][2]={{-15000,-8300},{-6700,-1200},{1200,6700},{8300,15000}};
	for(int32 BX=0;BX<2;++BX) for(int32 BY=0;BY<4;++BY)
	{
		const double X0=BlockX[BX][0],X1=BlockX[BX][1],Y0=BlockY[BY][0],Y1=BlockY[BY][1];
		const double CX=(X0+X1)*.5,CY=(Y0+Y1)*.5,W=X1-X0,H=Y1-Y0;
		if(BX==0&&BY==0)
		{
			// Park: open ground with trees and hedges (concealment), a monument and a few low walls. Long views across it.
			Emit(TEXT("Monument"),CX,CY,300,300,400);
			for(int32 GX=-2;GX<=2;++GX) for(int32 GY=-1;GY<=1;++GY)
			{
				if(!GX&&!GY)continue;
				if((GX+GY)%2)Emit(TEXT("Hedge"),CX+GX*2200,CY+GY*2200,GX%2?900:100,GX%2?100:900,220,true);
				else{Emit(TEXT("Tree"),CX+GX*2200,CY+GY*2200,70,70,700);Emit(TEXT("Tree"),CX+GX*2200+450,CY+GY*2200+300,70,70,700);}
			}
			for(int32 Side:{-1,1}){Emit(TEXT("ParkWall"),CX+Side*3300,CY+600,60,500,120);Emit(TEXT("ParkWall"),CX+Side*1100,CY-1100,500,60,120);}
			Emit(TEXT("Kiosk"),X1-500,Y1-500,250,250,260);
		}
		else if(BX==1&&BY<=1)
		{
			// Warehouses: two large halls behind a loading yard with containers. Few, large pieces: long views along the yard.
			const double Depth=H-3000;
			for(int32 Side:{-1,1})Emit(TEXT("Warehouse"),CX+Side*2900,Y1-400-Depth*.5,4400,Depth,700);
			for(int32 I=0;I<4;++I)Emit(TEXT("Container"),X0+1400+I*2800,Y0+900+(I%2?500:0),I%2?250:600,I%2?600:250,260);
			for(int32 I=0;I<3;++I)Emit(TEXT("DockWall"),X0+2800+I*2800,Y0+1900,400,60,120);
			Emit(TEXT("Truck"),CX+(BY?1500:-1500),Y0+2300,700,250,300);
		}
		else
		{
			// Old town (beside the avenue and the north inner block) and houses (north outer blocks): 2 x 2 lots split by 8m alleys.
			const bool bHouses=BX==1;
			for(int32 LX=0;LX<2;++LX) for(int32 LY=0;LY<2;++LY)
			{
				const double LX0=LX?CX+400:X0,LX1=LX?X1:CX-400,LY0=LY?CY+400:Y0,LY1=LY?Y1:CY-400;
				const int32 Kind=bHouses?((LX+LY+BY)%2?2:0):(BX+2*BY+LX+3*LY);
				Lot(Kind,(LX0+LX1)*.5,(LY0+LY1)*.5,LX1-LX0,LY1-LY0);
			}
		}
	}
	// Street clutter as on map E for the 16m streets (a wreck in the middle of every segment, on alternating sides).
	auto Clutter=[&](double X,double Y,bool bAlongX,int32 Kind)
	{
		switch(((Kind%4)+4)%4)
		{
		case 0: Emit(TEXT("Car"),X,Y,bAlongX?450:200,bAlongX?200:450,160);break;
		case 1: Emit(TEXT("Barrier"),X,Y,bAlongX?60:300,bAlongX?300:60,120);break;
		case 2: Emit(TEXT("Sandbags"),X,Y,bAlongX?90:400,bAlongX?400:90,120);break;
		default: Emit(TEXT("Kiosk"),X,Y,250,250,260);break;
		}
	};
	const double StreetX[5]={-25000,-13000,0,13000,25000},StreetY[5]={-15000,-7500,0,7500,15000};
	const double Along[4]={.18,.31,.69,.82};
	for(int32 Row:{1,3}) for(int32 Seg=0;Seg<4;++Seg)
	{
		const double Start=StreetX[Seg],Length=StreetX[Seg+1]-StreetX[Seg],Y=StreetY[Row];
		Emit(TEXT("StreetWreck"),Start+Length*.5,Y+((Seg+Row)%2?300:-300),300,1000,300);
		for(int32 I=0;I<4;++I)Clutter(Start+Length*Along[I],Y+((I+Seg+Row)%2?500:-500),true,I+Seg+Row);
	}
	for(int32 Column=1;Column<4;++Column) for(int32 Seg=0;Seg<4;++Seg)
	{
		const double Start=StreetY[Seg],Length=StreetY[Seg+1]-StreetY[Seg],X=StreetX[Column];
		Emit(TEXT("StreetWreck"),X+((Seg+Column)%2?300:-300),Start+Length*.5,1000,300,300);
		for(int32 I=0;I<4;++I)Clutter(X+((I+Seg+Column)%2?500:-500),Start+Length*Along[I],false,I+Seg+Column+1);
	}
	// The avenue: cover only along its edges (y = +-10m), so the middle 14m stays clear for vehicles.
	for(int32 Seg=0;Seg<4;++Seg) for(int32 I=0;I<4;++I)
	{
		const double Start=StreetX[Seg],Length=StreetX[Seg+1]-StreetX[Seg];
		Clutter(Start+Length*Along[I],(I+Seg)%2?1000:-1000,true,I+Seg);
	}
	// City objectives: low walls on four sides, crates at the corners. Objective C stands on the avenue and keeps only the walls,
	// which leaves an 8m lane on each side for vehicles.
	const int32 PointCount=BattleFieldLayout::ObjectiveCountForMap(WideName);
	for(int32 I=0;I<PointCount;++I)
	{
		const FVector O=BattleFieldLayout::ObjectiveForMap(WideName,I);
		if(FMath::Abs(O.X)>=25000||FMath::Abs(O.Y)>=15000)continue;
		const bool bAvenue=FMath::Abs(O.Y)<1200;
		for(int32 Side:{-1,1})
		{
			Emit(TEXT("PlazaWall"),O.X,O.Y+Side*380,300,60,120);Emit(TEXT("PlazaWall"),O.X+Side*380,O.Y,60,300,120);
			if(!bAvenue)for(int32 Other:{-1,1})Emit(TEXT("PlazaCrate"),O.X+Side*650,O.Y+Other*650,140,140,130);
		}
	}
	// South flank objective (the north one is its rotation): a hamlet on the flank road beside the x = 130m connector.
	{
		const double X=15000,Y=-25000;
		for(int32 Side:{-1,1}){Emit(TEXT("HamletWall"),X,Y+Side*500,400,60,120);Emit(TEXT("HamletCrate"),X+Side*700,Y+Side*700,140,140,130);Emit(TEXT("HamletCrate"),X+Side*700,Y-Side*700,140,140,130);}
		if(!UrbanG)Emit(TEXT("FarmHouse"),X+1900,Y-1700,1100,800,450);
		Emit(TEXT("FarmWall"),X+2800,Y-900,70,700,130);
		Emit(TEXT("Shed"),X+1600,Y+1500,500,400,280);Emit(TEXT("StoneWall"),X-600,Y+1500,600,70,130);
	}
	// East depot (the west one is its rotation): stacked crates and a container around the objective, between the city and the home.
	{
		const double X=33000,Y=20000;
		Emit(TEXT("DepotCrate"),X+700,Y+700,600,400,200);Emit(TEXT("DepotCrate"),X-800,Y+600,400,600,200);
		Emit(TEXT("Container"),X,Y-1000,600,250,260);Emit(TEXT("Shed"),X+1500,Y-1300,500,400,280);
		Emit(TEXT("DepotWall"),X+1100,Y-200,60,500,120);Emit(TEXT("DepotWall"),X-1100,Y-200,60,500,120);
	}
	// Flanks and approaches: scattered cover on a jittered grid (fixed seed), never on a road, and thinner than on map E. Measured
	// median sight on the flanks: 125m on E, 160m here (Setup/Benchmarks/MapStats). The outermost 30m stays nearly bare.
	FRandomStream Random(20261118);
	auto Scatter=[&](double MinX,double MaxX,double MinY,double MaxY,double Cell,double Chance)
	{
		for(double GX=MinX;GX<MaxX;GX+=Cell) for(double GY=MinY;GY<MaxY;GY+=Cell)
		{
			const double X=GX+Cell*(.25+.5*Random.FRand()),Y=GY+Cell*(.25+.5*Random.FRand());
			const double Roll=Random.FRand(),Kind=Random.FRand();const bool bAlongX=Random.FRand()<.5;
			if(Roll>Chance||OnRoad(X,Y,700)||(UrbanG&&BattleTerrain::NearHillG(X,Y,1300)))continue;
			if(Kind<.25)Emit(TEXT("StoneWall"),X,Y,bAlongX?600:70,bAlongX?70:600,130);
			else if(Kind<.45)Emit(TEXT("Hedge"),X,Y,bAlongX?900:100,bAlongX?100:900,220,true);
			else if(Kind<.60){Emit(TEXT("Tree"),X,Y,70,70,700);Emit(TEXT("Tree"),X+(bAlongX?450:0),Y+(bAlongX?0:450),70,70,700);}
			else if(Kind<.75)Emit(TEXT("Boulder"),X,Y,220,220,160);
			else if(Kind<.84)Emit(TEXT("Shed"),X,Y,500,400,280);
			else if(Kind<.93)Emit(TEXT("Wreck"),X,Y,bAlongX?450:200,bAlongX?200:450,160);
			else{Emit(TEXT("FarmHouse"),X,Y,1100,800,450);Emit(TEXT("FarmWall"),X+900,Y-300,70,700,130);Emit(TEXT("FarmWall"),X+300,Y+700,700,70,130);}
		}
	};
	for(int32 Side:{-1,1})
	{
		const double Near=Side>0?16500:-31500,Far=Side>0?31500:-16500;
		Scatter(-46000,46000,Near,Far,2500,FlankChance);
		Scatter(-46000,46000,Side>0?31500:-34000,Side>0?34000:-31500,2500,.1);
	}
	Scatter(26000,37500,-16000,16000,2500,ApproachChance);
	// Hedgerows beside the flank roads every 100m, on alternating sides: a column on the road is hidden in places, seen in others.
	for(int32 Side:{-1,1}) for(int32 I=0;I<19;++I)
	{
		const double X=-45000+I*5000;
		if(FMath::Abs(FMath::Abs(X)-13000)<1500||FMath::Abs(FMath::Abs(X)-25000)<1500||FMath::Abs(FMath::Abs(X)-38000)<1500||FMath::Abs(X)<1500)continue;
		Emit(TEXT("Hedge"),X,Side*25000+(I%2?1000:-1000),900,100,220,true);
	}
	// Low walls at the junctions of the flank roads: a place to hold where routes split and join.
	for(int32 SideY:{-1,1}) for(double X:{0.,13000.,25000.,38000.})
	{
		Emit(TEXT("JunctionWall"),X+900,SideY*25000+900,400,70,130);Emit(TEXT("JunctionWall"),X-900,SideY*25000-900,70,400,130);
	}
	// Home yards and lookouts as on map E.
	Emit(TEXT("HomeYardWall"),36000,3000,100,2400,130);Emit(TEXT("HomeYardWall"),36000,-3000,100,2400,130);
	Emit(TEXT("LookoutSandbags"),300,16900,400,90,120);Emit(TEXT("LookoutSandbags"),-300,-16900,400,90,120);
	if(UrbanG)
#include "BattleMapLayoutG.inl"
	UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_LAYOUT objects=%d (x2 by rotation) layout=%s"),Objects,UrbanG?TEXT("heights-v1"):TEXT("districts-v1"));
}
