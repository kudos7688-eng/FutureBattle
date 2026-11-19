{
 UMaterial* Field=Material(TEXT("M_SectorField"),FLinearColor(.32f,.28f,.12f));
 UMaterial* Water=Material(TEXT("M_SectorWater"),FLinearColor(.04f,.23f,.34f));
 UMaterial* Gravel=Material(TEXT("M_FordGravel"),FLinearColor(.33f,.36f,.29f));
 UMaterial* Woods=Material(TEXT("M_SectorWood"),FLinearColor(.12f,.25f,.11f));
 UMaterial* Road=Material(TEXT("M_SectorRoad"),FLinearColor(.24f,.24f,.22f));
 // Adjacent bank strips share edges; only the decks connect the banks on the navigation mesh.
 for(int Y=-34500;Y<35000;Y+=1000){
  const double X=BattleSectors::RiverX(Y),West=X-500,East=X+500;
  Box(TEXT("RiverBank"),FVector((West-50000)*.5,Y,-60),FVector(West+50000,1000,120),FloorMat);
  Box(TEXT("RiverBank"),FVector((East+50000)*.5,Y,-60),FVector(50000-East,1000,120),FloorMat);
  // 2026-11-19 (Claude, map lane): the river had no bottom. The gap between the banks was open below the water surface (which
  // has no collision), so a leader pushed off a bank or a ford fell without end (hundreds of positions below -10m per match).
  // Now a bed with collision lies 40cm below the banks: low enough to stay off the navigation mesh (the ground volume starts
  // at -20cm), shallow enough to step out of (45cm). Kerbs (55cm, too high to step on) along both banks keep soldiers out
  // except at the decks. The water surface is drawn just above the bed.
  Box(TEXT("River"),FVector(X,Y,-34),FVector(1000,1000,8),Water,false);
  Box(TEXT("RiverBed"),FVector(X,Y,-100),FVector(1000,1000,120),FloorMat);
  {
   // The part of this 10m strip that a deck covers (with a 40cm margin) gets no kerb.
   double Open0=1.e9,Open1=-1.e9;
   for(int C:BattleSectors::Crossings)
   {
    const double W=BattleSectors::IsFord(C)?210:C==0?BattleSectors::MainWidth:BattleSectors::SideWidth;
    if(C+W*.5+40>Y-500&&C-W*.5-40<Y+500){Open0=FMath::Min(Open0,C-W*.5-40);Open1=FMath::Max(Open1,C+W*.5+40);}
   }
   auto Kerb=[&](double Y0,double Y1)
   {
    if(Y1-Y0<20)return;
    Box(TEXT("RiverKerb"),FVector(West-10,(Y0+Y1)*.5,27.5),FVector(20,Y1-Y0,55),WallMat);
    Box(TEXT("RiverKerb"),FVector(East+10,(Y0+Y1)*.5,27.5),FVector(20,Y1-Y0,55),WallMat);
   };
   if(Open0>Open1)Kerb(Y-500,Y+500);else{Kerb(Y-500,FMath::Min(Open0,double(Y+500)));Kerb(FMath::Max(Open1,double(Y-500)),Y+500);}
   // Where the next strip is offset, the step in the bank line is closed too (unless a deck is there).
   if(Y+1000<35000)
   {
    const double NX=BattleSectors::RiverX(Y+1000),Edge=Y+500;
    if(FMath::Abs(NX-X)>1&&!(Edge>Open0&&Edge<Open1))
     for(double Side:{-500.,500.})Box(TEXT("RiverKerb"),FVector((X+NX)*.5+Side,Edge,27.5),FVector(FMath::Abs(NX-X)+20,20,55),WallMat);
   }
  }
 }
 for(int Y:BattleSectors::Crossings)
 {
  const bool Ford=BattleSectors::IsFord(Y);
  const double X=BattleSectors::RiverX(Y);
  const double Width=Ford?210:Y==0?BattleSectors::MainWidth:BattleSectors::SideWidth;
  Box(Ford?TEXT("Ford"):TEXT("Bridge"),FVector(X,Y,-40),FVector(1600,Width,80),Ford?Gravel:Road);
  if(!Ford)for(int Side:{-1,1})
   Box(TEXT("BridgeParapet"),FVector(X,Y+Side*(Width*.5+25),55),FVector(1400,50,110),WallMat);

 }
 // The road surfaces follow the same graph as the minimap and the route orders.
 const auto& Map=BattleFieldLayout::ByName(WideName);
 for(const auto& Edge:Map.RouteEdges)
 {
  const FVector A=Map.RouteNodes[Edge.A],B=Map.RouteNodes[Edge.B];
  const bool AlongX=A.Y==B.Y;
  const int Width=Edge.Lane==BattleFieldLayout::ERouteLane::Centre?BattleSectors::MainWidth:
      Edge.Lane==BattleFieldLayout::ERouteLane::Connector?BattleSectors::StreetWidth:BattleSectors::SideWidth;
  Box(TEXT("Road"),(A+B)*.5+FVector(0,0,5),FVector(AlongX?FMath::Abs(A.X-B.X):Width,AlongX?Width:FMath::Abs(A.Y-B.Y),2),Road,false);
 }
 for(int Y:{-BattleSectors::FordOffset,BattleSectors::FordOffset})
  for(int Sign:{-1,1})
  {
   const double Bank=BattleSectors::RiverX(Y)+Sign*500, Junction=Sign*(Sign*Y<0?30000:10000);
   Box(TEXT("FarmTrack"),FVector((Bank+Junction)*.5,Y,4),FVector(FMath::Abs(Junction-Bank),BattleSectors::TrailWidth,2),Field,false);
  }
 for(int Y=0;Y<14;++Y)for(int X=10;X<20;++X)
 {
  const FVector P=BattleSectors::Center({X,Y});const char K=BattleSectors::Kind({X,Y});
  for(int Sign:{-1,1})
  {
   const FVector Origin=P*Sign;
   if(K=='F'||K=='T')Box(TEXT("LandCover"),Origin+FVector(0,0,1),FVector(5000,5000,2),K=='F'?Field:Woods,false);
  }
  if(K=='W')
  {
   Emit(TEXT("DepotWall"),P.X+850,P.Y+2050,2000,75,350);
   for(int Side:{-1,1})Emit(TEXT("DepotWall"),P.X+850+Side*960,P.Y+1300,75,1450,350);
   Emit(TEXT("DepotCrates"),P.X+1250,P.Y+1460,250,320,160);
  }
  if(K=='T')for(int I=0;I<5;++I)
  {
   const double TX=P.X-1500+I*640,TY=P.Y+(I%2?1050:-1000);
   Emit(TEXT("TreeTrunk"),TX,TY,90,90,420);
   Emit(TEXT("Hedge"),TX+280,TY,380,100,220,true);
  }
  if(K=='H'||K=='W'||K=='J')
  {
   Emit(TEXT("ParcelWall"),P.X-1250,P.Y-1100,840,60,130);
   Emit(TEXT("ParcelWall"),P.X+1350,P.Y-1250,60,700,130);
  }
 }
 // Reuse the same walkable geometry for houses and hills as the terrain sampler.
#include "BattleMapLayoutG.inl"
}
