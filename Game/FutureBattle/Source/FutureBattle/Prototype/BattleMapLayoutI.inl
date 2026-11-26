// Layout of map I ("sectors-v3", 2026-11-26): every sector of BattleMapI.h is filled by its kind. Included in
// BattleMapCommandlet.cpp where Box, Emit(Label,X,Y,SizeX,SizeY,Height,bConcealment), Piece(Label,X,Y,Z0,SizeX,SizeY,Height,
// bTerrain) and Ramp(...) exist; Emit, Piece and Ramp place the 180-degree partner too, so only the east half is listed.
// 2026-11-26: I has tall town borders, protected doorways and authored lanes; L retains sectors-v2. Units: cm.
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
	// Ground paint (no collision): roads, fields, water. Map L paints them into its ground mesh instead (BattleMapCommandlet.cpp).
	if(!UrbanL)
	{
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
	// Map L: the hills are part of the rolling ground (BattleMapL.h); only the low walls are built, standing on that ground.
	Use(HillMat);
	if(!UrbanL)for(const BattleTerrain::FHill& H:Hills())
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
			const double Z0=UrbanL?0.:H.Top;
			if(bLongX)Piece(TEXT("HillWall"),H.X+I*H.W*.3,H.Y+Side*(H.H*.5-150),Z0,500,60,130);
			else Piece(TEXT("HillWall"),H.X+Side*(H.W*.5-150),H.Y+I*H.H*.3,Z0,60,500,130);
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

	// 2026-11-26: plan I's shared walls before its contents, as on K. L keeps the original emission path.
	struct FSideI3
	{
		bool Along=false,Road=false,Both=false;double Line=0.,Mid=0.,In=0.,Lo=0.,Hi=0.;
		TArray<double> Doors;TArray<FBox2D> Blinds;
	};
	struct FSmallI3 {const TCHAR* Label;double X,Y,W,H,Z;bool Conceal,Keep;UMaterial* Mat;};
	struct FSectorI3
	{
		TArray<FSmallI3> Contents;TArray<FSideI3> Sides;TArray<FBox2D> Lanes;TArray<TArray<FVector2D>> Routes;FVector2D Hub;
	};
	TArray<FSectorI3> PlansI3;PlansI3.SetNum(Columns*Rows);
	TArray<FBox2D> BuildingsI3,FixedI3,WallsI3,BlindsI3,DoorsI3,RoadSpaceI3,PlacedI3,HutOpeningsI3,PlacedLowI3,AllLanesI3;
	TArray<FSmallI3> SmallI3;
	bool CollectI3=false,KeepI3=false;
	int32 LaneSectorsI3=0,LanesI3=0,DroppedI3=0,ClearedI3=0,AdjustedI3=0,SingleI3=0,HutsI3=0,SolidI3=0,MovedI3=0,LowAddedI3=0,LowMinI3=MAX_int32;
	auto TownI3=[](TCHAR KindI3){return KindI3==TEXT('H')||KindI3==TEXT('P')||KindI3==TEXT('O')||KindI3==TEXT('W')||KindI3==TEXT('D');};
	auto RectI3=[](double PxI3,double PyI3,double WxI3,double HyI3)
	{return FBox2D(FVector2D(PxI3-WxI3*.5,PyI3-HyI3*.5),FVector2D(PxI3+WxI3*.5,PyI3+HyI3*.5));};
	auto TurnI3=[](const FBox2D& RectTurnI3){return FBox2D(-RectTurnI3.Max,-RectTurnI3.Min);};
	auto TouchI3=[](const FBox2D& TestI3,const TArray<FBox2D>& ListI3,double MarginI3)
	{for(const FBox2D& OtherI3:ListI3)if(TestI3.Min.X<OtherI3.Max.X+MarginI3&&TestI3.Max.X>OtherI3.Min.X-MarginI3&&TestI3.Min.Y<OtherI3.Max.Y+MarginI3&&TestI3.Max.Y>OtherI3.Min.Y-MarginI3)return true;return false;};
	auto PairRectI3=[&](TArray<FBox2D>& ListPairI3,const FBox2D& RectPairI3)
	{ListPairI3.Add(RectPairI3);ListPairI3.Add(TurnI3(RectPairI3));};
	auto AlongRectI3=[&](const FSideI3& SideRectI3,double AlongAtI3,double AcrossAtI3,double LengthI3,double DepthI3)
	{return SideRectI3.Along?RectI3(AlongAtI3,AcrossAtI3,LengthI3,DepthI3):RectI3(AcrossAtI3,AlongAtI3,DepthI3,LengthI3);};
	auto EmitRectI3=[&](const TCHAR* LabelRectI3,const FBox2D& ShapeI3,double HeightI3)
	{const FVector2D PosRectI3=ShapeI3.GetCenter(),SizeRectI3=ShapeI3.GetSize();Emit(LabelRectI3,PosRectI3.X,PosRectI3.Y,SizeRectI3.X,SizeRectI3.Y,HeightI3);};
	auto PutI3=[&](const TCHAR* LabelPutI3,double XPutI3,double YPutI3,double WPutI3,double HPutI3,double ZPutI3,bool ConcealPutI3=false)
	{
		if(!UrbanL&&CollectI3){SmallI3.Add({LabelPutI3,XPutI3,YPutI3,WPutI3,HPutI3,ZPutI3,ConcealPutI3,KeepI3,WallMat});return;}
		// 2026-11-26: non-town content stays immediate, with the original single sequence of random draws.
		if(!UrbanL)PairRectI3(PlacedI3,RectI3(XPutI3,YPutI3,WPutI3,HPutI3));
		Emit(LabelPutI3,XPutI3,YPutI3,WPutI3,HPutI3,ZPutI3,ConcealPutI3);
	};
	if(!UrbanL)
	{
		for(const FRoad& RoadI3:Roads())RoadSpaceI3.Add(RoadI3.bAlongX?RectI3((RoadI3.From+RoadI3.To)*.5,RoadI3.At,RoadI3.To-RoadI3.From,800.):RectI3(RoadI3.At,(RoadI3.From+RoadI3.To)*.5,800.,RoadI3.To-RoadI3.From));
		for(const BattleTerrain::FHouse& HouseI3:Houses())PairRectI3(BuildingsI3,RectI3(HouseI3.X,HouseI3.Y,HouseI3.W,HouseI3.H));
		// 2026-11-26: reserve terrain footprints and the later bridge rails without changing their emission order.
		for(const BattleTerrain::FHill& TerrainHillI3:Hills())
		{
			PairRectI3(PlacedI3,RectI3(TerrainHillI3.X,TerrainHillI3.Y,TerrainHillI3.W,TerrainHillI3.H));
			for(int32 TerrainSideI3:{-1,1})
			{
				PairRectI3(PlacedI3,RectI3(TerrainHillI3.X+TerrainSideI3*(TerrainHillI3.W+TerrainHillI3.Run)*.5,TerrainHillI3.Y,TerrainHillI3.Run,TerrainHillI3.H));
				PairRectI3(PlacedI3,RectI3(TerrainHillI3.X,TerrainHillI3.Y+TerrainSideI3*(TerrainHillI3.H+TerrainHillI3.Run)*.5,TerrainHillI3.W,TerrainHillI3.Run));
			}
		}
		for(int32 RailSideI3:{-1,1})PairRectI3(PlacedI3,RectI3((RiverX0+RiverX1)*.5,BridgeY+RailSideI3*(BridgeHalf+30),RiverX1-RiverX0,40.));
		for(int32 ColI3=Columns/2;ColI3<Columns;++ColI3)for(int32 RowI3=0;RowI3<Rows;++RowI3)
		{
			const TCHAR KindI3=Kind(ColI3,RowI3);const FVector2D MidI3=Centre(ColI3,RowI3);
			if(KindI3==TEXT('W'))PairRectI3(BuildingsI3,RectI3(MidI3.X,MidI3.Y+300.,2800.,1800.));
			if(KindI3==TEXT('D'))PairRectI3(BuildingsI3,RectI3(MidI3.X,MidI3.Y+1450.,2600.,1100.));
			if(KindI3==TEXT('O')||KindI3==TEXT('D'))for(int32 CoverSideI3:{-1,1})
			{
				if(!(KindI3==TEXT('D')&&CoverSideI3>0))PairRectI3(FixedI3,RectI3(MidI3.X,MidI3.Y+CoverSideI3*900.,500.,60.));
				PairRectI3(FixedI3,RectI3(MidI3.X+CoverSideI3*900.,MidI3.Y,60.,500.));
				for(int32 CoverOtherI3:{-1,1})if(!(KindI3==TEXT('D')&&CoverOtherI3>0)&&!(KindI3==TEXT('O')&&CoverSideI3>0&&CoverOtherI3>0))
					PairRectI3(FixedI3,RectI3(MidI3.X+CoverSideI3*1200.,MidI3.Y+CoverOtherI3*1200.,140.,140.));
			}
		}
		// Shared borders use a canonical (east, then north) key, including the rotated x=0 border.
		// Door search is exhaustive in 2m steps: minimise total displacement, then keep the original first doorway.
		for(int32 ColI3=Columns/2;ColI3<Columns;++ColI3)for(int32 RowI3=0;RowI3<Rows;++RowI3)
		{
			if(!TownI3(Kind(ColI3,RowI3)))continue;
			FSectorI3& PlanI3=PlansI3[ColI3*Rows+RowI3];const FVector2D MidI3=Centre(ColI3,RowI3);
			for(int32 EdgeI3=0;EdgeI3<4;++EdgeI3)
			{
				const int32 DcI3=EdgeI3==2?1:EdgeI3==3?-1:0,DrI3=EdgeI3==0?1:EdgeI3==1?-1:0;
				const TCHAR NextI3=Kind(ColI3+DcI3,RowI3+DrI3);
				if(NextI3==TEXT('h')||NextI3==TEXT('R')||NextI3==TEXT('S')||NextI3==TEXT(' '))continue;
				FSideI3 SideI3;SideI3.Along=EdgeI3<2;SideI3.In=-(DcI3+DrI3);
				const double BorderAtI3=(SideI3.Along?MidI3.Y:MidI3.X)-SideI3.In*2500.;
				SideI3.Mid=SideI3.Along?MidI3.X:MidI3.Y;SideI3.Road=OnRoad(SideI3.Along,BorderAtI3,SideI3.Mid);
				SideI3.Both=SideI3.Road||!TownI3(NextI3);
				SideI3.Line=BorderAtI3+(SideI3.Road?SideI3.In*500.:0.);
				auto CrossI3=[&](double EndI3){return OnRoad(!SideI3.Along,EndI3,BorderAtI3-100.)||OnRoad(!SideI3.Along,EndI3,BorderAtI3+100.);};
				SideI3.Lo=SideI3.Mid-2500.+(CrossI3(SideI3.Mid-2500.)?500.:0.);
				SideI3.Hi=SideI3.Mid+2500.-(CrossI3(SideI3.Mid+2500.)?500.:0.);
				const bool RotateKeyI3=!SideI3.Along&&BorderAtI3==0.&&SideI3.Mid<0.;
				const double KeyMidI3=RotateKeyI3?-SideI3.Mid:SideI3.Mid;
				uint32 BitsI3=uint32(FMath::RoundToInt(BorderAtI3/2500.))*73856093u^uint32(FMath::RoundToInt(KeyMidI3/2500.))*19349663u^(SideI3.Along?20u:21u)^0x9e3779b9u;
				BitsI3^=BitsI3>>13;BitsI3*=0x5bd1e995u;BitsI3^=BitsI3>>15;
				if(SideI3.Road&&(RotateKeyI3?-SideI3.In:SideI3.In)<0.)BitsI3>>=7;
				double OriginalI3[2]={KeyMidI3-1700.+400.*((BitsI3>>1)%3),KeyMidI3+700.+400.*((BitsI3>>3)%3)};
				if(RotateKeyI3){const double OldDoorI3=OriginalI3[0];OriginalI3[0]=-OriginalI3[1];OriginalI3[1]=-OldDoorI3;}
				TArray<double> ChoicesI3[2];
				for(int32 DoorIndexI3=0;DoorIndexI3<2;++DoorIndexI3)for(int32 StepI3=0;StepI3<=40;++StepI3)
				{
					const int32 OffsetI3=StepI3==0?0:((StepI3+1)/2)*(StepI3%2?1:-1);
					const double CandidateI3=OriginalI3[DoorIndexI3]+OffsetI3*200.;
					// 30cm accounts for the perpendicular wall at a road corner; 6m depth starts at the wall face.
					if(CandidateI3<SideI3.Lo+330.||CandidateI3>SideI3.Hi-330.)continue;
					const FBox2D PassageI3=AlongRectI3(SideI3,CandidateI3,SideI3.Line,600.,1260.);
					if(TouchI3(PassageI3,BuildingsI3,0.)||TouchI3(PassageI3,FixedI3,0.))continue;
					ChoicesI3[DoorIndexI3].Add(CandidateI3);
				}
				double CostI3=1.e30;double BestAI3=0.,BestBI3=0.;
				for(double AI3:ChoicesI3[0])for(double BI3:ChoicesI3[1])
				{
					if(BI3-AI3<900.)continue;
					const double TryCostI3=FMath::Abs(AI3-OriginalI3[0])+FMath::Abs(BI3-OriginalI3[1]);
					if(TryCostI3<CostI3){CostI3=TryCostI3;BestAI3=AI3;BestBI3=BI3;}
				}
				if(CostI3<1.e30){SideI3.Doors.Add(BestAI3);SideI3.Doors.Add(BestBI3);}
				else
				{
					for(int32 ChoiceIndexI3=0;ChoiceIndexI3<2;++ChoiceIndexI3)for(double ChoiceI3:ChoicesI3[ChoiceIndexI3])
						if(FMath::Abs(ChoiceI3-OriginalI3[ChoiceIndexI3])<CostI3){CostI3=FMath::Abs(ChoiceI3-OriginalI3[ChoiceIndexI3]);BestAI3=ChoiceI3;}
					if(CostI3<1.e30)SideI3.Doors.Add(BestAI3);
				}
				// Do not silently seal a sector if a future footprint makes even one doorway impossible.
				if(SideI3.Doors.IsEmpty()){UE_LOG(LogTemp,Error,TEXT("TOWN_I_NO_DOOR C=%d R=%d edge=%d"),ColI3,RowI3,EdgeI3);continue;}
				bool ExistingI3=false;
				for(const FBox2D& WallSeenI3:WallsI3)
				{
					const FVector2D SeenMidI3=WallSeenI3.GetCenter();
					if(SideI3.Along?FMath::Abs(SeenMidI3.Y-SideI3.Line)<1.&&SeenMidI3.X>SideI3.Lo&&SeenMidI3.X<SideI3.Hi:FMath::Abs(SeenMidI3.X-SideI3.Line)<1.&&SeenMidI3.Y>SideI3.Lo&&SeenMidI3.Y<SideI3.Hi){ExistingI3=true;break;}
				}
				if(!ExistingI3)
				{
					if(SideI3.Doors.Num()==1)++SingleI3;
					if(SideI3.Doors.Num()!=2||SideI3.Doors[0]!=OriginalI3[0]||SideI3.Doors.Last()!=OriginalI3[1])++AdjustedI3;
					double StartI3=SideI3.Lo;
					for(int32 PartI3=0;PartI3<=SideI3.Doors.Num();++PartI3)
					{
						const double EndI3=PartI3<SideI3.Doors.Num()?SideI3.Doors[PartI3]-300.:SideI3.Hi;
						if(EndI3>StartI3)PairRectI3(WallsI3,AlongRectI3(SideI3,(StartI3+EndI3)*.5,SideI3.Line,EndI3-StartI3,60.));
						if(PartI3<SideI3.Doors.Num()){PairRectI3(DoorsI3,AlongRectI3(SideI3,SideI3.Doors[PartI3],SideI3.Line,600.,1260.));StartI3=SideI3.Doors[PartI3]+300.;}
					}
				}
				PlanI3.Sides.Add(SideI3);
			}
		}
		// 9c blinds: 6.6m behind the wall (6m face-to-face), 18m long, or 14m for two on one side.
		// The 4.6m fallback would violate the requested 6m doorway depth, so shorten in 2m steps or omit instead.
		// Check every side's passage before accepting a blind; order cannot close an already planned doorway.
		for(int32 ColI3=Columns/2;ColI3<Columns;++ColI3)for(int32 RowI3=0;RowI3<Rows;++RowI3)
		{
			FSectorI3& PlanI3=PlansI3[ColI3*Rows+RowI3];const FVector2D MidI3=Centre(ColI3,RowI3);
			const TCHAR KindI3=Kind(ColI3,RowI3);const double MiddleI3=KindI3==TEXT('O')||KindI3==TEXT('D')?1200.:450.;
			for(FSideI3& SideI3:PlanI3.Sides)for(int32 DoorIndexI3=0;DoorIndexI3<SideI3.Doors.Num();++DoorIndexI3)
			{
				// A shared town wall has one blind per doorway, on alternate sides. A road has its own yard on each side.
				if(!SideI3.Both&&SideI3.In!=(DoorIndexI3%2?1.:-1.))continue;
				const double BlindAtI3=SideI3.Line+SideI3.In*660.;
				for(double HalfBlindI3=SideI3.Both&&SideI3.Doors.Num()==2?700.:900.;HalfBlindI3>=300.;HalfBlindI3-=100.)
				{
					const FBox2D BlindI3=AlongRectI3(SideI3,SideI3.Doors[DoorIndexI3],BlindAtI3,HalfBlindI3*2.,60.);
					if(BlindI3.Min.X<MidI3.X-2500.||BlindI3.Max.X>MidI3.X+2500.||BlindI3.Min.Y<MidI3.Y-2500.||BlindI3.Max.Y>MidI3.Y+2500.)continue;
					if(BlindI3.Intersect(RectI3(MidI3.X,MidI3.Y,MiddleI3*2.,MiddleI3*2.))||TouchI3(BlindI3,BuildingsI3,300.)||TouchI3(BlindI3,FixedI3,300.)||TouchI3(BlindI3,WallsI3,300.)||TouchI3(BlindI3,BlindsI3,300.)||TouchI3(BlindI3,DoorsI3,0.)||TouchI3(BlindI3,RoadSpaceI3,0.))continue;
					SideI3.Blinds.Add(BlindI3);PairRectI3(BlindsI3,BlindI3);break;
				}
			}
		}
		Use(Grey);
		for(int32 WallIndexI3=0;WallIndexI3<WallsI3.Num();WallIndexI3+=2)EmitRectI3(TEXT("YardHighWall"),WallsI3[WallIndexI3],250.);
		for(int32 BlindIndexI3=0;BlindIndexI3<BlindsI3.Num();BlindIndexI3+=2)EmitRectI3(TEXT("YardBlind"),BlindsI3[BlindIndexI3],250.);
	}
	// Objective cover: low walls on four sides 9m from the middle and crates at the corners 12m out. The middle stays open.
	// (v1 used the pen of map E, walls 3.8m from the middle inside the 4.5m capture radius. With sector orders an objective is held
	// by the whole sector and every soldier is sent towards the middle: they jammed in the pen, 93 stalls within 5m of one
	// objective's middle in a match, mi2-I-sector1-s7.)
	// bHouseNE: a house stands in the north-east corner (no crate there). bNorthBuilt: a warehouse fills the north side (no wall
	// and no crates there).
	auto ObjectiveCover=[&](double X,double Y,bool bHouseNE,bool bNorthBuilt)
	{
		KeepI3=true;
		for(int32 Side:{-1,1})
		{
			if(!(bNorthBuilt&&Side>0))PutI3(TEXT("PlazaWall"),X,Y+Side*900,500,60,120);
			PutI3(TEXT("PlazaWall"),X+Side*900,Y,60,500,120);
			for(int32 Other:{-1,1})
			{
				if((bNorthBuilt&&Other>0)||(bHouseNE&&Side>0&&Other>0))continue;
				PutI3(TEXT("PlazaCrate"),X+Side*1200,Y+Other*1200,140,140,130);
			}
		}
		KeepI3=false;
	};
	FRandomStream Random(20261119);
	for(int32 C=Columns/2;C<Columns;++C)for(int32 R=0;R<Rows;++R)
	{
		const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);const double X=M.X,Y=M.Y;
		const int32 Mix=(C*7+R*13)%4;const double Flip=(C+R)%2?1.:-1.;
		Use(Grey);
		CollectI3=!UrbanL&&TownI3(K);SmallI3.Reset();
		switch(K)
		{
		case TEXT('H'):
			// The house stands a little north-west of the middle; yard walls on all four sides, a car, a shed (not enterable), crates.
			PutI3(TEXT("YardWall"),X-300+Flip*500,Y-1350,700,60,130);PutI3(TEXT("YardWall"),X+1500,Y+300+Flip*400,60,700,130);
			PutI3(TEXT("YardWall"),X-2000,Y+500-Flip*400,60,700,130);PutI3(TEXT("YardWall"),X+300-Flip*500,Y+1950,700,60,130);
			PutI3(TEXT("YardCar"),X+Flip*1300,Y-1800,450,200,150);
			PutI3(TEXT("YardCrate"),X-Flip*1500,Y-1700,140,140,130);PutI3(TEXT("YardCrate"),X+1900,Y-700,140,140,130);
			if(Mix<2)PutI3(TEXT("Shed"),X+1500,Y+1500,450,350,260);else PutI3(TEXT("YardWall"),X+1300,Y+1500,60,600,130);
			break;
		case TEXT('P'):
			// The middle of the sector stays free: a sector order goes to the middle, and an order into an obstacle is refused.
			PutI3(TEXT("Fountain"),X+Flip*700,Y-700,300,300,150);
			for(int32 Side:{-1,1})
			{
				PutI3(TEXT("PlazaWall"),X+Flip*300,Y+Side*1300,500,60,130);PutI3(TEXT("PlazaWall"),X+Side*1300,Y-Flip*300,60,500,130);
				for(int32 Other:{-1,1})PutI3(TEXT("PlazaCar"),X+Side*1750,Y+Other*1850,(Side*Other>0)?450:200,(Side*Other>0)?200:450,150);
			}
			PutI3(TEXT("Kiosk"),X+Flip*800,Y+800,250,250,260);PutI3(TEXT("PlazaCrate"),X-Flip*800,Y-800,140,140,130);
			break;
		case TEXT('O'):
			ObjectiveCover(X,Y,true,false);
			PutI3(TEXT("YardWall"),X-1600,Y-1500,700,60,130);PutI3(TEXT("YardWall"),X-1700,Y+1200,60,700,130);
			PutI3(TEXT("YardWall"),X+1500,Y-1600,60,700,130);PutI3(TEXT("YardWall"),X-600,Y+1900,700,60,130);
			PutI3(TEXT("YardCar"),X+1700,Y-300,200,450,150);PutI3(TEXT("YardCar"),X-300,Y-1900,450,200,150);
			break;
		case TEXT('D'):
			ObjectiveCover(X,Y,false,true);
			Warehouse(X,Y+1450,2600,1100);Use(Grey);
			PutI3(TEXT("Container"),X-1500,Y-1500,600,250,260);PutI3(TEXT("Container"),X+1600,Y-1300,250,600,260);
			PutI3(TEXT("DepotWall"),X-1800,Y+200,60,700,130);PutI3(TEXT("DepotWall"),X+1800,Y+100,60,700,130);
			PutI3(TEXT("DepotWall"),X+200,Y-1900,700,60,130);PutI3(TEXT("DepotCrate"),X-600,Y-1200,160,160,130);
			break;
		case TEXT('W'):
			Warehouse(X,Y+300,2800,1800);Use(Grey);
			PutI3(TEXT("Container"),X-1400,Y-1500,600,250,260);PutI3(TEXT("Container"),X+1300,Y-1600,600,250,260);
			PutI3(TEXT("DepotWall"),X-1900,Y+300,60,700,130);PutI3(TEXT("DepotWall"),X+1900,Y+300,60,700,130);
			PutI3(TEXT("DepotWall"),X,Y-1950,700,60,130);PutI3(TEXT("DepotWall"),X,Y+1800,700,60,130);
			break;
		case TEXT('F'):
			// Open on purpose: up to two hay bales.
			for(int32 I=0;I<Mix%3;++I)PutI3(TEXT("HayBale"),X+Random.FRandRange(-1700.,1700.),Y+Random.FRandRange(-1700.,1700.),220,220,150);
			break;
		case TEXT('T'):
			// A wood: trunks on a jittered 10m grid (they stop bullets), bushes that only conceal, two fallen trees as low cover.
			for(int32 GX=0;GX<4;++GX)for(int32 GY=0;GY<4;++GY)
			{
				const double TX=X-1500+GX*1000+Random.FRandRange(-300.,300.),TY=Y-1500+GY*1000+Random.FRandRange(-300.,300.);
				if(Random.FRand()<.2)continue;
				Use(TrunkMat);PutI3(TEXT("Tree"),TX,TY,70,70,700);
				for(int32 Sign:{-1,1})Box(TEXT("TreeCrown"),FVector(TX*Sign,TY*Sign,600),FVector(380,380,260),TreeMat,false);
			}
			Use(TreeMat);
			for(int32 I=0;I<3;++I)PutI3(TEXT("Hedge"),X+Random.FRandRange(-1800.,1800.),Y+Random.FRandRange(-1800.,1800.),I%2?450:150,I%2?150:450,220,true);
			Use(TrunkMat);
			PutI3(TEXT("FallenTree"),X+Flip*900,Y-1900,500,80,130);PutI3(TEXT("FallenTree"),X-1900,Y+Flip*900,80,500,130);
			PutI3(TEXT("FallenTree"),X-Flip*900,Y+1900,500,80,130);PutI3(TEXT("FallenTree"),X+1900,Y-Flip*900,80,500,130);
			break;
		case TEXT('R'):
			// River banks: rocks and reeds beside the water, away from the bridge and the ford.
			for(double Off:{-1300.,1300.})
			{
				const double BY=Y+Off;if(FMath::Abs(BY-BridgeY)<1800||FMath::Abs(BY-FordY)<900)continue;
				PutI3(TEXT("RiverRock"),RiverX0-450,BY,220,220,150);PutI3(TEXT("RiverRock"),RiverX1+450,BY+Flip*500,220,220,150);
				Use(TreeMat);PutI3(TEXT("Hedge"),RiverX0-250,BY+900*Flip,100,700,220,true);Use(Grey);
			}
			break;
		case TEXT('S'):
			// Home base: sandbag walls clear of the starting columns (x = 390..408m).
			if(C==17){PutI3(TEXT("HomeYardWall"),X-1300,Y,100,2000,130);}
			else if(C==19){PutI3(TEXT("HomeYardWall"),X,Y+Flip*1000,100,1600,130);PutI3(TEXT("HomeCrate"),X+800,Y-Flip*1200,160,160,130);}
			else PutI3(TEXT("HomeYardWall"),X+1800,Y+Flip*1500,100,1200,130);
			break;
		default: break; // hills are built above
		}
		CollectI3=false;

		if(!UrbanL&&TownI3(K))PlansI3[C*Rows+R].Contents=MoveTemp(SmallI3);

		// Borders (north and east edge of every sector; the west edge on the x = 0 line): a wall or a hedge where no road runs,
		// leaving the corners open. None beside a hill, a river sector or a home base.
		auto Open=[](TCHAR A,TCHAR B){auto Skip=[](TCHAR T){return T==TEXT('h')||T==TEXT('R')||T==TEXT('S')||T==TEXT(' ');};return Skip(A)||Skip(B);};
		auto Border=[&](bool bAlongX,double At,double Mid,int32 Variant)
		{
			if(OnRoad(bAlongX,At,Mid))return;
			const bool bHedge=Variant%3==0;Use(bHedge?TreeMat:Grey);
			// 2026-11-26: reserve non-town borders before town low-cover placement.
			if(!UrbanL)PairRectI3(PlacedI3,bAlongX?RectI3(Mid,At,2000.,bHedge?100.:60.):RectI3(At,Mid,bHedge?100.:60.,2000.));
			if(bAlongX)Emit(bHedge?TEXT("Hedge"):TEXT("StoneWall"),Mid,At,2000,bHedge?100:60,bHedge?220:130,bHedge);
			else Emit(bHedge?TEXT("Hedge"):TEXT("StoneWall"),At,Mid,bHedge?100:60,2000,bHedge?220:130,bHedge);
		};
		if(R+1<Rows&&!Open(K,Kind(C,R+1))&&(UrbanL||(!TownI3(K)&&!TownI3(Kind(C,R+1)))))Border(true,Y+Cell*.5,X,C+2*R);
		if(C+1<Columns&&!Open(K,Kind(C+1,R))&&(UrbanL||(!TownI3(K)&&!TownI3(Kind(C+1,R)))))Border(false,X+Cell*.5,Y,2*C+R+1);
		if(C==Columns/2&&Y>0&&!Open(K,Kind(C-1,R))&&(UrbanL||(!TownI3(K)&&!TownI3(Kind(C-1,R)))))Border(false,0,Y,R);
	}
	// 2026-11-26: only I defers town emission. The switch above consumes Random once for both I and L.
	// 2026-11-26: finish all huts first, then place low cover against the complete neighboring obstacle inventory.
	if(!UrbanL)for(int32 ContentPassI3=0;ContentPassI3<2;++ContentPassI3)
	for(int32 C=Columns/2;C<Columns;++C)for(int32 R=0;R<Rows;++R)
	{
		const TCHAR K=Kind(C,R);const FVector2D M=Centre(C,R);const double X=M.X,Y=M.Y;
		if(!UrbanL&&TownI3(K))
		{
			FSectorI3& SectorPlanI3=PlansI3[C*Rows+R];
			const TArray<FSmallI3>& ContentI3=SectorPlanI3.Contents;
			const FBox2D SectorBoundsI3=RectI3(X,Y,5000.,5000.);
			TArray<FBox2D> BlockersI3;
			for(const TArray<FBox2D>* SourceI3:{&BuildingsI3,&FixedI3,&WallsI3,&BlindsI3,&PlacedI3})
				for(const FBox2D& ObstacleI3:*SourceI3)if(ObstacleI3.Intersect(SectorBoundsI3.ExpandBy(175.)))BlockersI3.Add(ObstacleI3);
			if(ContentPassI3==0)
			{
				++LaneSectorsI3;
				// 2026-11-26: K's doorway-to-hub lanes, with an orthogonal visibility grid when one turn cannot pass I's buildings.
				// Nodes are only doorway/hub coordinates and obstacle edges plus the 1.75m half-width (1cm numerical clearance).
				// This is authoring only; the runtime receives the same ordered LanePts markers as K.
				TArray<double> GridXI3,GridYI3;TArray<FVector2D> HubsI3;
				const bool ObjectiveI3=K==TEXT('O')||K==TEXT('D');
				for(int32 HubTryI3=0;HubTryI3<(ObjectiveI3?1:25);++HubTryI3)
				{
					const int32 HubCellI3=(12+HubTryI3*7)%25;
					const FVector2D HubTryAtI3(X+(HubCellI3%5-2)*600.,Y+(HubCellI3/5-2)*600.);
					if(TouchI3(RectI3(HubTryAtI3.X,HubTryAtI3.Y,350.,350.),BlockersI3,0.))continue;
					HubsI3.Add(HubTryAtI3);GridXI3.AddUnique(HubTryAtI3.X);GridYI3.AddUnique(HubTryAtI3.Y);
				}
				for(const FSideI3& LaneSideI3:SectorPlanI3.Sides)for(double DoorAtI3:LaneSideI3.Doors)
				{
					GridXI3.AddUnique(LaneSideI3.Along?DoorAtI3:LaneSideI3.Line);
					GridYI3.AddUnique(LaneSideI3.Along?LaneSideI3.Line:DoorAtI3);
				}
				for(const FBox2D& GridObstacleI3:BlockersI3)
				{
					for(double GridAtI3:{GridObstacleI3.Min.X-176.,GridObstacleI3.Max.X+176.})if(GridAtI3>X-2500.&&GridAtI3<X+2500.)GridXI3.AddUnique(GridAtI3);
					for(double GridAtI3:{GridObstacleI3.Min.Y-176.,GridObstacleI3.Max.Y+176.})if(GridAtI3>Y-2500.&&GridAtI3<Y+2500.)GridYI3.AddUnique(GridAtI3);
				}
				GridXI3.Sort();GridYI3.Sort();const int32 NXI3=GridXI3.Num(),NYI3=GridYI3.Num(),NodesI3=NXI3*NYI3;
				TArray<uint8> LinksI3;LinksI3.Init(0,NodesI3);
				auto NodePointI3=[&](int32 NodeI3){return FVector2D(GridXI3[NodeI3%NXI3],GridYI3[NodeI3/NXI3]);};
				auto SegmentI3=[&](const FVector2D& FromI3,const FVector2D& ToI3)
				{return FBox2D(FVector2D(FMath::Min(FromI3.X,ToI3.X)-175.,FMath::Min(FromI3.Y,ToI3.Y)-175.),FVector2D(FMath::Max(FromI3.X,ToI3.X)+175.,FMath::Max(FromI3.Y,ToI3.Y)+175.));};
				for(int32 NodeI3=0;NodeI3<NodesI3;++NodeI3)
				{
					const FVector2D NodeAtI3=NodePointI3(NodeI3);
					if(NodeI3%NXI3+1<NXI3&&!TouchI3(SegmentI3(NodeAtI3,NodePointI3(NodeI3+1)),BlockersI3,0.)){LinksI3[NodeI3]|=1;LinksI3[NodeI3+1]|=2;}
					if(NodeI3/NXI3+1<NYI3&&!TouchI3(SegmentI3(NodeAtI3,NodePointI3(NodeI3+NXI3)),BlockersI3,0.)){LinksI3[NodeI3]|=4;LinksI3[NodeI3+NXI3]|=8;}
				}
				int32 BestCountI3=-1;TArray<int32> BestParentsI3,BestEntriesI3;
				for(const FVector2D& HubAtI3:HubsI3)
				{
					const int32 RootI3=GridYI3.Find(HubAtI3.Y)*NXI3+GridXI3.Find(HubAtI3.X);
					TArray<int32> ParentsI3,QueueI3;ParentsI3.Init(-1,NodesI3);ParentsI3[RootI3]=RootI3;QueueI3.Add(RootI3);
					for(int32 HeadI3=0;HeadI3<QueueI3.Num();++HeadI3)
					{
						const int32 VisitI3=QueueI3[HeadI3];const int32 StepsI3[4]={1,-1,NXI3,-NXI3};
						for(int32 LinkI3=0;LinkI3<4;++LinkI3)if(LinksI3[VisitI3]&(1<<LinkI3))
						{
							const int32 NextNodeI3=VisitI3+StepsI3[LinkI3];if(ParentsI3[NextNodeI3]>=0)continue;
							ParentsI3[NextNodeI3]=VisitI3;QueueI3.Add(NextNodeI3);
						}
					}
					int32 CountI3=0;TArray<int32> EntriesI3;
					for(const FSideI3& EntrySideI3:SectorPlanI3.Sides)
					{
						int32 EntryI3=-1;double EntryLengthI3=1.e30;
						for(double EntryDoorI3:EntrySideI3.Doors)
						{
							const int32 DoorNodeI3=GridYI3.Find(EntrySideI3.Along?EntrySideI3.Line:EntryDoorI3)*NXI3+GridXI3.Find(EntrySideI3.Along?EntryDoorI3:EntrySideI3.Line);
							if(ParentsI3[DoorNodeI3]<0)continue;
							double LengthI3=0.;for(int32 WalkI3=DoorNodeI3;WalkI3!=RootI3;WalkI3=ParentsI3[WalkI3])LengthI3+=(NodePointI3(WalkI3)-NodePointI3(ParentsI3[WalkI3])).Size();
							if(LengthI3<EntryLengthI3){EntryLengthI3=LengthI3;EntryI3=DoorNodeI3;}
						}
						EntriesI3.Add(EntryI3);if(EntryI3>=0)++CountI3;
					}
					if(CountI3>BestCountI3){BestCountI3=CountI3;SectorPlanI3.Hub=HubAtI3;BestParentsI3=MoveTemp(ParentsI3);BestEntriesI3=MoveTemp(EntriesI3);}
					if(CountI3==SectorPlanI3.Sides.Num())break;
				}
				for(int32 SideIndexI3=0;SideIndexI3<SectorPlanI3.Sides.Num();++SideIndexI3)
				{
					if(!BestEntriesI3.IsValidIndex(SideIndexI3)||BestEntriesI3[SideIndexI3]<0){++DroppedI3;continue;}
					TArray<FVector2D> RouteI3;
					for(int32 WalkI3=BestEntriesI3[SideIndexI3];;WalkI3=BestParentsI3[WalkI3])
					{
						const FVector2D PointI3=NodePointI3(WalkI3);
						if(RouteI3.Num()>=2)
						{
							const FVector2D BeforeI3=RouteI3[RouteI3.Num()-2],LastI3=RouteI3.Last();
							if((BeforeI3.X==LastI3.X&&LastI3.X==PointI3.X)||(BeforeI3.Y==LastI3.Y&&LastI3.Y==PointI3.Y))RouteI3.Pop();
						}
						RouteI3.Add(PointI3);if(BestParentsI3[WalkI3]==WalkI3)break;
					}
					++LanesI3;SectorPlanI3.Routes.Add(RouteI3);
					for(int32 LegI3=1;LegI3<RouteI3.Num();++LegI3)
					{
						const FBox2D LaneRectI3=SegmentI3(RouteI3[LegI3-1],RouteI3[LegI3]);SectorPlanI3.Lanes.Add(LaneRectI3);PairRectI3(AllLanesI3,LaneRectI3);
						const FVector2D LaneAtI3=LaneRectI3.GetCenter(),LaneSizeI3=LaneRectI3.GetSize();
						for(int32 LaneSignI3:{-1,1})Box(TEXT("Lane"),FVector(LaneAtI3.X*LaneSignI3,LaneAtI3.Y*LaneSignI3,2.25),FVector(LaneSizeI3.X,LaneSizeI3.Y,4.5),FieldMat,false);
					}
					for(int32 RouteSignI3:{-1,1})
					{
						FString RouteTagI3=TEXT("LanePts=");
						for(const FVector2D& RouteAtI3:RouteI3)
						{
							if(RouteTagI3.Len()>8)RouteTagI3+=TEXT(";");
							RouteTagI3+=FString::Printf(TEXT("%d,%d"),FMath::RoundToInt(RouteAtI3.X*RouteSignI3),FMath::RoundToInt(RouteAtI3.Y*RouteSignI3));
						}
						AStaticMeshActor* RouteActorI3=Box(TEXT("LaneRoute"),FVector(SectorPlanI3.Hub.X*RouteSignI3,SectorPlanI3.Hub.Y*RouteSignI3,2.),FVector(20,20,4),FieldMat,false);
						RouteActorI3->Tags.Add(TEXT("BattleLaneRoute"));RouteActorI3->Tags.Add(FName(*RouteTagI3));
					}
				}
				// 2026-11-26: fix huts and their opening segments before relocating any low cover.
				TArray<FBox2D> OccupiedI3=BlockersI3,HutSpaceI3;TArray<int32> HutSmallI3;
				for(int32 SmallIndexI3=0;SmallIndexI3<ContentI3.Num();++SmallIndexI3)
				{
					const FSmallI3& ItemI3=ContentI3[SmallIndexI3];
					if(ItemI3.Keep){Use(ItemI3.Mat);Emit(ItemI3.Label,ItemI3.X,ItemI3.Y,ItemI3.W,ItemI3.H,ItemI3.Z,ItemI3.Conceal);}
					else if(ItemI3.Z>150.)HutSmallI3.Add(SmallIndexI3);
				}
				for(int32 HutIndexI3:HutSmallI3)
				{
					const FSmallI3& HutItemI3=ContentI3[HutIndexI3];
					const bool BayI3=FCString::Strcmp(HutItemI3.Label,TEXT("Container"))==0,StallI3=FCString::Strcmp(HutItemI3.Label,TEXT("Kiosk"))==0;
					// Hut local X is its long front; local +Y faces the middle. Bays retain their original long axis.
					bool FoundHutI3=false;FVector2D HutAtI3;bool AlongHutI3=false;double FaceI3=1.;double BestMoveI3=1.e30;
					for(int32 MoveXI3=-40;MoveXI3<=40;++MoveXI3)for(int32 MoveYI3=-40;MoveYI3<=40;++MoveYI3)
					{
						const double MoveCostI3=double(MoveXI3*MoveXI3+MoveYI3*MoveYI3);if(MoveCostI3>=BestMoveI3)continue;
						const FVector2D TryHutI3(HutItemI3.X+MoveXI3*100.,HutItemI3.Y+MoveYI3*100.);
						const bool LongXI3=BayI3?HutItemI3.W>HutItemI3.H:FMath::Abs(Y-TryHutI3.Y)>=FMath::Abs(X-TryHutI3.X);
						const double FrontI3=BayI3?600.:StallI3?400.:500.,DeepI3=BayI3?360.:StallI3?350.:400.;
						const double DirectionI3=(LongXI3?Y-TryHutI3.Y:X-TryHutI3.X)>=0.?1.:-1.;
						const FBox2D FootI3=RectI3(TryHutI3.X,TryHutI3.Y,LongXI3?FrontI3:DeepI3,LongXI3?DeepI3:FrontI3);
						if(FootI3.Min.X<X-2100.||FootI3.Max.X>X+2100.||FootI3.Min.Y<Y-2100.||FootI3.Max.Y>Y+2100.)continue;
						if(FootI3.Intersect(RectI3(X,Y,900.,900.))||TouchI3(FootI3,OccupiedI3,300.)||TouchI3(FootI3,SectorPlanI3.Lanes,0.)||TouchI3(FootI3,DoorsI3,0.))continue;
						FBox2D AccessI3=FootI3;
						if(BayI3){if(LongXI3){AccessI3.Min.X-=300.;AccessI3.Max.X+=300.;}else{AccessI3.Min.Y-=300.;AccessI3.Max.Y+=300.;}}
						else if(LongXI3){if(DirectionI3>0.)AccessI3.Max.Y+=300.;else AccessI3.Min.Y-=300.;}
						else{if(DirectionI3>0.)AccessI3.Max.X+=300.;else AccessI3.Min.X-=300.;}
						if(AccessI3.Min.X<X-2100.||AccessI3.Max.X>X+2100.||AccessI3.Min.Y<Y-2100.||AccessI3.Max.Y>Y+2100.||TouchI3(AccessI3,OccupiedI3,0.))continue;
						FoundHutI3=true;BestMoveI3=MoveCostI3;HutAtI3=TryHutI3;AlongHutI3=LongXI3;FaceI3=DirectionI3;
					}
					Use(HutItemI3.Mat);
					if(!FoundHutI3)
					{
						// Lane clearance wins over retaining a solid fallback; count omitted originals as cleared, not kept solid.
						const FBox2D OldHutI3=RectI3(HutItemI3.X,HutItemI3.Y,HutItemI3.W,HutItemI3.H);
						if(TouchI3(OldHutI3,SectorPlanI3.Lanes,0.)||TouchI3(OldHutI3,DoorsI3,0.)||TouchI3(OldHutI3,WallsI3,300.)||TouchI3(OldHutI3,BlindsI3,300.)||TouchI3(OldHutI3,HutSpaceI3,300.)){++ClearedI3;continue;}
						Emit(HutItemI3.Label,HutItemI3.X,HutItemI3.Y,HutItemI3.W,HutItemI3.H,HutItemI3.Z);OccupiedI3.Add(OldHutI3);PairRectI3(PlacedI3,OldHutI3);++SolidI3;continue;
					}
					const double FrontI3=BayI3?600.:StallI3?400.:500.,DeepI3=BayI3?360.:StallI3?350.:400.;
					auto HutWallI3=[&](double LocalXI3,double LocalYI3,double WideI3,double DepthI3)
					{
						const double WallXI3=HutAtI3.X+(AlongHutI3?LocalXI3:LocalYI3*FaceI3),WallYI3=HutAtI3.Y+(AlongHutI3?LocalYI3*FaceI3:LocalXI3);
						Emit(HutItemI3.Label,WallXI3,WallYI3,AlongHutI3?WideI3:DepthI3,AlongHutI3?DepthI3:WideI3,260.);
					};
					if(BayI3)for(int32 HutSideI3:{-1,1})HutWallI3(0.,HutSideI3*(DeepI3*.5-15.),FrontI3,30.);
					else
					{
						HutWallI3(0.,-DeepI3*.5+15.,FrontI3,30.);
						for(int32 HutSideI3:{-1,1})HutWallI3(HutSideI3*(FrontI3*.5-15.),15.,30.,DeepI3-30.);
					}
					const FBox2D BuiltHutI3=RectI3(HutAtI3.X,HutAtI3.Y,AlongHutI3?FrontI3:DeepI3,AlongHutI3?DeepI3:FrontI3);
					OccupiedI3.Add(BuiltHutI3);HutSpaceI3.Add(BuiltHutI3);PairRectI3(PlacedI3,BuiltHutI3);++HutsI3;
					// 2026-11-26: reserve 3m from the entire open edge, at both ends of a bay.
					if(BayI3)for(int32 OpeningEndI3:{-1,1})
						PairRectI3(HutOpeningsI3,RectI3(HutAtI3.X+(AlongHutI3?OpeningEndI3*FrontI3*.5:0.),HutAtI3.Y+(AlongHutI3?0.:OpeningEndI3*FrontI3*.5),AlongHutI3?0.:DeepI3,AlongHutI3?DeepI3:0.));
					else PairRectI3(HutOpeningsI3,RectI3(HutAtI3.X+(AlongHutI3?0.:FaceI3*DeepI3*.5),HutAtI3.Y+(AlongHutI3?FaceI3*DeepI3*.5:0.),AlongHutI3?FrontI3:0.,AlongHutI3?0.:FrontI3));
				}
			}
			else
			{
				// 2026-11-26: distances are shortest footprint-to-footprint distances, not center distances.
				// Touching is allowed, overlap is not; [0.9m,3m) gaps and two close neighbors are forbidden.
				TArray<FBox2D> LowObstaclesI3;
				for(const TArray<FBox2D>* LowSourceI3:{&BuildingsI3,&FixedI3,&WallsI3,&BlindsI3,&PlacedI3})
					for(const FBox2D& LowObstacleI3:*LowSourceI3)
						if(LowObstacleI3.Intersect(SectorBoundsI3.ExpandBy(300.)))LowObstaclesI3.Add(LowObstacleI3);
				auto GapSqI3=[](const FBox2D& GapAI3,const FBox2D& GapBI3)
				{
					const double GapXI3=FMath::Max(0.,FMath::Max(GapAI3.Min.X-GapBI3.Max.X,GapBI3.Min.X-GapAI3.Max.X));
					const double GapYI3=FMath::Max(0.,FMath::Max(GapAI3.Min.Y-GapBI3.Max.Y,GapBI3.Min.Y-GapAI3.Max.Y));
					return GapXI3*GapXI3+GapYI3*GapYI3;
				};
				auto FreeLowI3=[&](const FBox2D& LowTestI3)
				{
					if(LowTestI3.Min.X<SectorBoundsI3.Min.X||LowTestI3.Max.X>SectorBoundsI3.Max.X||LowTestI3.Min.Y<SectorBoundsI3.Min.Y||LowTestI3.Max.Y>SectorBoundsI3.Max.Y)return false;
					if(GapSqI3(LowTestI3,RectI3(X,Y,0.,0.))<450.*450.||TouchI3(LowTestI3,AllLanesI3,0.)||TouchI3(LowTestI3,DoorsI3,0.))return false;
					for(const FBox2D& OpeningTestI3:HutOpeningsI3)if(GapSqI3(LowTestI3,OpeningTestI3)<300.*300.)return false;
					int32 CloseLowI3=0;
					for(const FBox2D& AgainstLowI3:LowObstaclesI3)
					{
						if(LowTestI3.Min.X<AgainstLowI3.Max.X&&LowTestI3.Max.X>AgainstLowI3.Min.X&&LowTestI3.Min.Y<AgainstLowI3.Max.Y&&LowTestI3.Max.Y>AgainstLowI3.Min.Y)return false;
						const double LowGapSqI3=GapSqI3(LowTestI3,AgainstLowI3);
						if(LowGapSqI3>=300.*300.)continue;
						if(LowGapSqI3>=90.*90.||++CloseLowI3>1)return false;
					}
					// 2026-11-26: a new neighbor must not give an earlier low piece a second close neighbor.
					for(const FBox2D& PriorLowI3:PlacedLowI3)if(GapSqI3(LowTestI3,PriorLowI3)<90.*90.)
						for(const FBox2D& PriorOtherI3:PlacedI3)
							if((PriorOtherI3.Min!=PriorLowI3.Min||PriorOtherI3.Max!=PriorLowI3.Max)&&GapSqI3(PriorLowI3,PriorOtherI3)<90.*90.)return false;
					for(const FBox2D& PriorLowI3:PlacedLowI3)if(GapSqI3(LowTestI3,PriorLowI3)<90.*90.)
						for(const TArray<FBox2D>* PriorFixedI3:{&BuildingsI3,&FixedI3,&WallsI3,&BlindsI3})
							for(const FBox2D& PriorShapeI3:*PriorFixedI3)if(GapSqI3(PriorLowI3,PriorShapeI3)<90.*90.)return false;
					return true;
				};
				int32 SectorLowI3=0;
				auto AcceptLowI3=[&](const FSmallI3& AcceptedI3)
				{
					Use(AcceptedI3.Mat);Emit(AcceptedI3.Label,AcceptedI3.X,AcceptedI3.Y,AcceptedI3.W,AcceptedI3.H,AcceptedI3.Z,AcceptedI3.Conceal);
					const FBox2D AcceptedRectI3=RectI3(AcceptedI3.X,AcceptedI3.Y,AcceptedI3.W,AcceptedI3.H);
					PairRectI3(PlacedI3,AcceptedRectI3);PairRectI3(PlacedLowI3,AcceptedRectI3);PairRectI3(LowObstaclesI3,AcceptedRectI3);
					if(AcceptedI3.Z>=110.&&AcceptedI3.Z<=150.)++SectorLowI3;
				};
				for(const FSmallI3& OriginalLowI3:ContentI3)
				{
					if(OriginalLowI3.Keep||OriginalLowI3.Z>150.)continue;
					if(FreeLowI3(RectI3(OriginalLowI3.X,OriginalLowI3.Y,OriginalLowI3.W,OriginalLowI3.H))){AcceptLowI3(OriginalLowI3);continue;}
					// 2026-11-26: exhaustive 1m offsets cover the entire 50m sector, in both orientations.
					// Minimize squared travel; ties prefer the original orientation, then increasing X and Y offsets.
					FSmallI3 BestLowI3=OriginalLowI3;double LowCostI3=1.e30;
					for(int32 LowTurnI3=0;LowTurnI3<2;++LowTurnI3)
					for(int32 LowStepXI3=-50;LowStepXI3<=50;++LowStepXI3)for(int32 LowStepYI3=-50;LowStepYI3<=50;++LowStepYI3)
					{
						const double LowTryCostI3=double(LowStepXI3*LowStepXI3+LowStepYI3*LowStepYI3);if(LowTryCostI3>=LowCostI3)continue;
						FSmallI3 LowTryI3=OriginalLowI3;LowTryI3.X+=LowStepXI3*100.;LowTryI3.Y+=LowStepYI3*100.;
						if(LowTurnI3){LowTryI3.W=OriginalLowI3.H;LowTryI3.H=OriginalLowI3.W;}
						if(!FreeLowI3(RectI3(LowTryI3.X,LowTryI3.Y,LowTryI3.W,LowTryI3.H)))continue;
						BestLowI3=LowTryI3;LowCostI3=LowTryCostI3;
					}
					if(LowCostI3==1.e30){++ClearedI3;continue;}
					AcceptLowI3(BestLowI3);++MovedI3;
				}
				// 2026-11-26: near face at 2.5m means a 2.8m center offset for a 60cm-thick LaneWall.
				// 2026-11-26: try both sides in 1m steps; allow ends to extend beyond a leg if all clearances hold.
				for(const TArray<FVector2D>& LowRouteI3:SectorPlanI3.Routes)
				for(int32 LowLegI3=1;LowLegI3<LowRouteI3.Num()&&SectorLowI3<6;++LowLegI3)
				{
					const FVector2D LowFromI3=LowRouteI3[LowLegI3-1],LowToI3=LowRouteI3[LowLegI3];
					const bool LowAlongXI3=LowFromI3.Y==LowToI3.Y;
					const double LowStartI3=LowAlongXI3?FMath::Min(LowFromI3.X,LowToI3.X):FMath::Min(LowFromI3.Y,LowToI3.Y);
					const double LowEndI3=LowAlongXI3?FMath::Max(LowFromI3.X,LowToI3.X):FMath::Max(LowFromI3.Y,LowToI3.Y);
					for(double LowAlongI3=LowStartI3;LowAlongI3<=LowEndI3&&SectorLowI3<6;LowAlongI3+=100.)
					for(int32 LowSideI3:{-1,1})
					{
						if(SectorLowI3>=6)break;
						FSmallI3 LaneLowI3={TEXT("LaneWall"),LowAlongXI3?LowAlongI3:LowFromI3.X+LowSideI3*280.,LowAlongXI3?LowFromI3.Y+LowSideI3*280.:LowAlongI3,LowAlongXI3?500.:60.,LowAlongXI3?60.:500.,130.,false,false,Grey};
						if(!FreeLowI3(RectI3(LaneLowI3.X,LaneLowI3.Y,LaneLowI3.W,LaneLowI3.H)))continue;
						AcceptLowI3(LaneLowI3);++LowAddedI3;
					}
				}
				LowMinI3=FMath::Min(LowMinI3,SectorLowI3);
			}
		}
	}

	Use(Grey);
	// Bridge railings (cover on the bridge) on both sides of the deck.
	for(int32 Side:{-1,1})Emit(TEXT("BridgeRail"),(RiverX0+RiverX1)*.5,BridgeY+Side*(BridgeHalf+30),RiverX1-RiverX0,40,110);
	if(!UrbanL)
	{
		UE_LOG(LogTemp,Display,TEXT("TOWN_I_LANES sectors=%d lanes=%d dropped=%d cleared=%d moved=%d (x2 by rotation)"),LaneSectorsI3,LanesI3,DroppedI3,ClearedI3,MovedI3);
		UE_LOG(LogTemp,Display,TEXT("TOWN_I_LOWCOVER sectors=%d min=%d added=%d"),LaneSectorsI3*2,LaneSectorsI3?LowMinI3:0,LowAddedI3*2);
		UE_LOG(LogTemp,Display,TEXT("TOWN_I_LANE_ROUTES routes=%d"),LanesI3*2);
		UE_LOG(LogTemp,Display,TEXT("TOWN_I_DOORS adjusted=%d single=%d"),AdjustedI3,SingleI3);
		UE_LOG(LogTemp,Display,TEXT("TOWN_I_HUTS built=%d kept_solid=%d"),HutsI3,SolidI3);
	}
	UE_LOG(LogTemp,Display,TEXT("URBAN_MAP_LAYOUT objects=%d (x2 by rotation) layout=%s"),Objects,UrbanL?TEXT("terrain-l-v1"):TEXT("sectors-v3"));
}
