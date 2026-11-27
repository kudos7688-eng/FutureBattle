#include "BattleKitHouses.h"
#include "BattleKitLook.h"
#include "FutureBattle.h"
#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace BattleKitLook
{
namespace
{
 enum class EPart : uint8 {WindowA,WindowB,WindowTall,DoorA,DoorB,Awning,Balcony,WindowOpen,DoorOpen,RoofGable,RoofHip,RoofRafters,Trim,RoofVent,Chimney,Count};
 const TCHAR* const Names[]={TEXT("WindowA"),TEXT("WindowB"),TEXT("WindowTall"),TEXT("DoorA"),TEXT("DoorB"),TEXT("Awning"),TEXT("Balcony"),TEXT("WindowOpen"),TEXT("DoorOpen"),TEXT("RoofGable"),TEXT("RoofHip"),TEXT("RoofRafters"),TEXT("Trim"),TEXT("RoofVent"),TEXT("Chimney")};
 constexpr int32 PartCount=int32(EPart::Count);
 static_assert(UE_ARRAY_COUNT(Names)==PartCount,"Update house part names");
 bool LabelIs(const FString& Label,const TCHAR* Name)
 {
  const FString Prefix(Name);if(Label==Prefix)return true;
  if(!Label.StartsWith(Prefix))return false;
  FString Tail=Label.Mid(Prefix.Len());Tail.RemoveFromStart(TEXT("_"));
  return !Tail.IsEmpty()&&Tail.IsNumeric();
 }
 FString BuildingKind(const FString& Label)
 {
  // Audited against BattleMapCommandlet and layouts F/G/I/J. Thin J/K shells fail the size test.
  // This list deliberately has no prop/cover labels (Container, Crate, HallWall, etc.).
  for(const TCHAR* Kind:{TEXT("House"),TEXT("FarmHouse"),TEXT("Apartment"),TEXT("Tower"),TEXT("TowerWing"),TEXT("Podium"),TEXT("Shop"),TEXT("Kiosk"),TEXT("Shed"),TEXT("Warehouse"),TEXT("WideWarehouse"),TEXT("Nave"),TEXT("BellTower")})
   if(LabelIs(Label,Kind))return Kind;
  return FString();
 }
 uint32 PositionHash(const FVector& P)
 {
  uint32 H=2166136261u;
  for(double V:{P.X,P.Y,P.Z})
  {const uint64 Bits=uint64(FMath::RoundToInt64(V));for(int32 B=0;B<8;++B){H^=uint32((Bits>>(B*8))&255u);H*=16777619u;}}
  return H;
 }
 struct FBoxPart
 {
  UStaticMeshComponent* Source=nullptr;
  FBox Bounds;
  FString Kind;
  bool bHouse=false,bSill=false,bNoRoof=false;
 };
 struct FGroup {TArray<int32> Members;FBox Bounds=FBox(ForceInit);};
 TArray<FGroup> GroupBoxes(const TArray<FBoxPart>& Boxes,bool bHouse)
 {
  TArray<FGroup> Groups;TSet<int32> Used;
  for(int32 I=0;I<Boxes.Num();++I)
  {
   if(Boxes[I].bHouse!=bHouse||Used.Contains(I))continue;
   FGroup G;G.Members.Add(I);Used.Add(I);
   for(int32 Next=0;Next<G.Members.Num();++Next)
   {
    const FBox B=Boxes[G.Members[Next]].Bounds;G.Bounds+=B;
    for(int32 J=0;J<Boxes.Num();++J)
     if(Boxes[J].bHouse==bHouse&&!Used.Contains(J)&&B.ExpandBy(5.).Intersect(Boxes[J].Bounds.ExpandBy(5.)))
     {G.Members.Add(J);Used.Add(J);}
   }
   Groups.Add(MoveTemp(G));
  }
  return Groups;
 }
 // Side 0/1 = west/east, 2/3 = south/north. Local +X points out, local Y spans the facade.
 FVector Normal(int32 Side){return Side<2?FVector(Side?1.:-1.,0,0):FVector(0,Side==3?1.:-1.,0);}
 int32 Along(int32 Side){return Side<2?1:0;}
 double Face(const FBox& B,int32 Side){return Side%2?B.Max[Side/2]:B.Min[Side/2];}
 FVector OnFace(const FBox& B,int32 Side,double Across,double Z)
 {FVector P=B.GetCenter();P[Side/2]=Face(B,Side);P[Along(Side)]=Across;P.Z=Z;return P;}
 int32 WallSide(const FBox& Wall,const FBox& House)
 {
  int32 Best=0;double Distance=MAX_dbl;
  const bool bThinX=Wall.GetSize().X<Wall.GetSize().Y;
  for(int32 S=bThinX?0:2;S<(bThinX?2:4);++S)
  {const double D=FMath::Abs(Face(Wall,S)-Face(House,S));if(D<Distance){Distance=D;Best=S;}}
  return Best;
 }
 struct FBatch {TArray<FTransform> Poses;};
 struct FDresser
 {
  UWorld* World;
  TStrongObjectPtr<UStaticMesh> Meshes[PartCount];bool Tried[PartCount]={};
  TStrongObjectPtr<UMaterialInterface> RoofMaterials[2];bool RoofTried[2]={};
  FBatch Batches[PartCount][3]; // 0 = native material, 1/2 = clay/slate, roof slots only
  TMap<UMaterialInterface*,TArray<UMaterialInstanceDynamic*>> Tints;
  TArray<UStaticMeshComponent*> Ground;
  int32 Missing=0,Tinted=0;
  explicit FDresser(UWorld* W):World(W){}
  bool Add(EPart Part,const FVector& P,const FVector& Scale=FVector(1.),double Yaw=0.,int32 Roof=-1)
  {
   const int32 K=int32(Part);
   if(!Tried[K])
   {
    Tried[K]=true;const FString Path=FString::Printf(TEXT("/Game/Kit/Parts/SM_Kit_%s.SM_Kit_%s"),Names[K],Names[K]);
    Meshes[K].Reset(LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn));if(!Meshes[K].IsValid())++Missing;
   }
   if(!Meshes[K].IsValid())return false;
   Batches[K][Roof+1].Poses.Emplace(FRotator(0,Yaw,0),P,Scale);return true;
  }
  bool Wall(EPart Part,const FVector& P,int32 Side,const FVector& Scale=FVector(1.))
  {return Add(Part,P,Scale,Normal(Side).Rotation().Yaw);}
  double GroundZ(const FVector& P) const
  {
   double Z=-MAX_dbl;
   // Read-only traces against saved ground components, never against units or decorative parts.
   for(UStaticMeshComponent* C:Ground)
   {
    const FBox B=C->Bounds.GetBox();if(P.X<B.Min.X||P.X>B.Max.X||P.Y<B.Min.Y||P.Y>B.Max.Y)continue;
    FHitResult Hit;FCollisionQueryParams Query;Query.bTraceComplex=true;
    if(C->LineTraceComponent(Hit,FVector(P.X,P.Y,B.Max.Z+100.),FVector(P.X,P.Y,B.Min.Z-100.),Query))Z=FMath::Max(Z,Hit.ImpactPoint.Z);
   }
   return Z;
  }
  void Tint(UStaticMeshComponent* C,uint32 H)
  {
   static const FLinearColor Palette[]={FLinearColor(.94f,.93f,.90f),FLinearColor(.90f,.85f,.73f),FLinearColor(.91f,.86f,.64f),FLinearColor(.87f,.73f,.70f),FLinearColor(.65f,.74f,.79f),FLinearColor(.73f,.74f,.73f)};
   for(int32 Slot=0;Slot<C->GetNumMaterials();++Slot)
   {
    UMaterialInterface* Base=C->GetMaterial(Slot);
    if(!Base||!Base->GetMaterial()||Base->GetMaterial()->GetFName()!=TEXT("M_Kit_Surface"))continue;
    FLinearColor Old;if(!Base->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")),Old))continue;
    const int32 Color=H%UE_ARRAY_COUNT(Palette);
    TArray<UMaterialInstanceDynamic*>& Cache=Tints.FindOrAdd(Base);if(Cache.IsEmpty())Cache.SetNumZeroed(UE_ARRAY_COUNT(Palette));
    if(!Cache[Color])
    {
     const FString Name=Base->GetName();const bool bMasonry=Name.Contains(TEXT("Brick"))||Name.Contains(TEXT("Stone"));
     Cache[Color]=UMaterialInstanceDynamic::Create(Base,World);
     if(!Cache[Color])continue;
     FLinearColor Value=bMasonry?Old*(.9f+.03f*Color):Old*Palette[Color];Value.A=Old.A;
     Cache[Color]->SetVectorParameterValue(TEXT("Tint"),Value);
    }
    C->SetMaterial(Slot,Cache[Color]);++Tinted;
   }
  }
  void Flush(int32& Windows,int32& Doors,int32& Roofs,int32& Instances)
  {
   AActor* Host=nullptr;
   for(int32 K=0;K<PartCount;++K)for(int32 R=0;R<3;++R)
   {
    const TArray<FTransform>& Poses=Batches[K][R].Poses;if(Poses.IsEmpty())continue;
    if(!Host){FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;Host=World->SpawnActor<AActor>(Spawn);if(!Host)return;}
    auto* C=NewObject<UInstancedStaticMeshComponent>(Host,NAME_None,RF_Transient);
    C->SetCanEverAffectNavigation(false);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);
    C->SetMobility(EComponentMobility::Movable);C->SetCastShadow(true);C->SetStaticMesh(Meshes[K].Get());
    if(R>0)
    {
     if(!RoofTried[R-1])
     {
      RoofTried[R-1]=true;const TCHAR* Name=R==1?TEXT("RoofClay"):TEXT("RoofSlate");
      const FString Path=FString::Printf(TEXT("/Game/Kit/Surfaces/MI_Kit_%s.MI_Kit_%s"),Name,Name);
      RoofMaterials[R-1].Reset(LoadObject<UMaterialInterface>(nullptr,*Path,nullptr,LOAD_NoWarn));
     }
     if(RoofMaterials[R-1].IsValid())
      for(int32 Slot=0;Slot<Meshes[K]->GetStaticMaterials().Num();++Slot)
       if(Meshes[K]->GetStaticMaterials()[Slot].MaterialSlotName==TEXT("Roof"))C->SetMaterial(Slot,RoofMaterials[R-1].Get());
    }
    Host->AddInstanceComponent(C);C->AddInstances(Poses,false,false,false);C->RegisterComponent();
    if(!C->IsRegistered()||C->GetInstanceCount()!=Poses.Num()){C->DestroyComponent();continue;}
    Instances+=Poses.Num();
    if(K<=int32(EPart::WindowTall)||K==int32(EPart::WindowOpen))Windows+=Poses.Num();
    if(K==int32(EPart::DoorA)||K==int32(EPart::DoorB)||K==int32(EPart::DoorOpen))Doors+=Poses.Num();
    if(K>=int32(EPart::RoofGable))Roofs+=Poses.Num(); // counts roof pieces, including trim/vents/chimneys
   }
  }
 };
 void Pitched(FDresser& D,const FBox& B,EPart Part,double Eave,double Ratio,uint32 Hash)
 {
  const FVector Size=B.GetSize();const bool bLongX=Size.X>=Size.Y;
  D.Add(Part,FVector(B.GetCenter().X,B.GetCenter().Y,B.Max.Z),
   FVector((FMath::Max(Size.X,Size.Y)+2*Eave)/100.,(FMath::Min(Size.X,Size.Y)+2*Eave)/100.,FMath::Min(Size.X,Size.Y)*Ratio/100.),bLongX?0.:90.,int32((Hash>>4)%2));
 }
 void OpenHouse(FDresser& D,const FGroup& G,const TArray<FBoxPart>& Boxes)
 {
  const uint32 Hash=PositionHash(G.Bounds.GetCenter());
  for(int32 I:G.Members)
  {
   const FBoxPart& P=Boxes[I];D.Tint(P.Source,Hash);if(!P.bSill)continue;
   double Top=P.Bounds.Max.Z;
   for(int32 J:G.Members)if(!Boxes[J].bSill&&FMath::Abs(Boxes[J].Bounds.Min.Z-P.Bounds.Min.Z)<5.)Top=FMath::Max(Top,Boxes[J].Bounds.Max.Z);
   const double Height=Top-P.Bounds.Max.Z;if(Height<=0.)continue;
   const int32 Side=WallSide(P.Bounds,G.Bounds);
   FVector At=P.Bounds.GetCenter();At.Z=P.Bounds.Max.Z;
   D.Wall(EPart::WindowOpen,At,Side,FVector(1.,P.Bounds.GetSize()[Along(Side)]/200.,Height/180.));
  }
  // Merge the occupied intervals of the first storey on each facade; only interior gaps are doors.
  for(int32 Side=0;Side<4;++Side)
  {
   TArray<FVector2D> Spans;double Top=MAX_dbl,Thickness=0.;
   for(int32 I:G.Members)
   {
    const FBoxPart& P=Boxes[I];if(FMath::Abs(P.Bounds.Min.Z-G.Bounds.Min.Z)>5.||WallSide(P.Bounds,G.Bounds)!=Side)continue;
    Spans.Emplace(P.Bounds.Min[Along(Side)],P.Bounds.Max[Along(Side)]);
    Thickness=FMath::Max(Thickness,P.Bounds.GetSize()[Side/2]);if(!P.bSill)Top=FMath::Min(Top,P.Bounds.Max.Z);
   }
   if(Spans.Num()<2||Top==MAX_dbl)continue;
   Spans.Sort([](const FVector2D& A,const FVector2D& B){return A.X<B.X;});double End=Spans[0].Y;
   for(int32 I=1;I<Spans.Num();++I)
   {
    if(Spans[I].X-End>60.)
    {
     FVector At=OnFace(G.Bounds,Side,(End+Spans[I].X)*.5,G.Bounds.Min.Z)-Normal(Side)*Thickness*.5;
     D.Wall(EPart::DoorOpen,At,Side,FVector(1.,(Spans[I].X-End)/200.,(Top-G.Bounds.Min.Z)/180.));
    }
    End=FMath::Max(End,Spans[I].Y);
   }
  }
  Pitched(D,G.Bounds,EPart::RoofRafters,30.,.35,Hash);
 }
 bool Hidden(const FVector& P,int32 Self,const TArray<FBoxPart>& Boxes)
 {for(int32 I=0;I<Boxes.Num();++I)if(I!=Self&&Boxes[I].Bounds.IsInsideOrOn(P))return true;return false;}
 void SolidBuilding(FDresser& D,int32 Index,uint32 Hash,const TArray<FBoxPart>& Boxes,const TArray<FBox>& NavBounds)
 {
  const FBoxPart& P=Boxes[Index];const FBox& B=P.Bounds;const FVector Size=B.GetSize();D.Tint(P.Source,Hash);
  const bool bWarehouse=P.Kind.Contains(TEXT("Warehouse")),bShop=P.Kind==TEXT("Shop")||P.Kind==TEXT("Kiosk");
  const bool bChurch=P.Kind==TEXT("Nave")||P.Kind==TEXT("BellTower");
  const int32 DoorSide=(Size.X>=Size.Y?2:0)+int32((Hash>>3)%2);
  for(int32 Side=0;Side<4;++Side)
  {
   const double Width=Size[Along(Side)];if(Width<250.)continue;
   const int32 Columns=FMath::FloorToInt((Width-100.)/280.),Floors=FMath::FloorToInt(Size.Z/300.);
   const int32 DoorColumn=Columns/2;
   const double DoorAcross=Columns>0?B.Min[Along(Side)]+Width*(DoorColumn+.5)/Columns:B.GetCenter()[Along(Side)];
   FVector Door=OnFace(B,Side,DoorAcross,B.Min.Z);Door.Z=FMath::Max(Door.Z,D.GroundZ(Door));
   const bool bDoor=Side==DoorSide&&!Hidden(Door+Normal(Side)*30.+FVector(0,0,110),Index,Boxes)&&Door.Z+(bShop||bWarehouse?220.:210.)<=B.Max.Z;
   if(bDoor)
   {
    D.Wall(bShop||bWarehouse?EPart::DoorB:EPart::DoorA,Door,Side);
    if(bShop&&Door.Z+240.<B.Max.Z)D.Wall(EPart::Awning,Door+FVector(0,0,230),Side);
   }
   for(int32 Floor=0;Floor<Floors;++Floor)for(int32 Col=0;Col<Columns;++Col)
   {
    if(bWarehouse&&(Floor!=Floors-1||Col%2!=0))continue;
    EPart Kind=bChurch?EPart::WindowTall:static_cast<EPart>(Hash%3);
    const double Height=Kind==EPart::WindowTall?200.:140.,HalfWidth=Kind==EPart::WindowB?110.:Kind==EPart::WindowTall?50.:55.;
    const double Across=B.Min[Along(Side)]+Width*(Col+.5)/Columns;
    FVector At=OnFace(B,Side,Across,B.Min.Z+Floor*300.+100.);
    if(bWarehouse)At.Z=B.Max.Z-Height-40.;
    if(At.Z+Height>B.Max.Z||At.Z<D.GroundZ(At)||Across-HalfWidth<B.Min[Along(Side)]+10.||Across+HalfWidth>B.Max[Along(Side)]-10.)continue;
    if(bDoor&&At.Z<Door.Z+240.&&FMath::Abs(Across-DoorAcross)<HalfWidth+(bShop||bWarehouse?125.:50.))continue;
    if(Hidden(At+FVector(0,0,Height*.5)+Normal(Side)*30.,Index,Boxes))continue;
    if(D.Wall(Kind,At,Side)&&P.Kind==TEXT("Apartment")&&Floor>=1&&PositionHash(At)%10<3&&Width/Columns>=240.)D.Wall(EPart::Balcony,At-FVector(0,0,100),Side);
   }
  }
  // NoRoof is authored on L's non-walkable obstacles. Else conservatively omit roofs wherever a nav volume reaches the top.
  if(!P.bNoRoof)
   for(const FBox& Nav:NavBounds)
   {FBox Top=B;Top.Min.Z=B.Max.Z-2.;Top.Max.Z=B.Max.Z+2.;if(Nav.Intersect(Top))return;}
  // Stacked/touching boxes may share colour but must not get a roof inside a taller box.
  for(int32 J=0;J<Boxes.Num();++J)if(J!=Index)
  {
   const FBox& Other=Boxes[J].Bounds;
   if(Other.Min.X<B.Max.X&&Other.Max.X>B.Min.X&&Other.Min.Y<B.Max.Y&&Other.Max.Y>B.Min.Y&&Other.Min.Z<=B.Max.Z+5.&&Other.Max.Z>B.Max.Z)return;
  }
  if(Size.Z<=900.&&FMath::Min(Size.X,Size.Y)<=1600.)
  {
   Pitched(D,B,(Hash&1)?EPart::RoofGable:EPart::RoofHip,40.,.4,Hash);
   // Along the ridge, start chimney at ridge height (native origin is its bottom).
   FVector At=B.GetCenter();At.Z=B.Max.Z+FMath::Min(Size.X,Size.Y)*.4;
   D.Add(EPart::Chimney,At);
  }
  else
  {
   for(int32 Side=0;Side<4;++Side)
   {
    FVector At=OnFace(B,Side,B.GetCenter()[Along(Side)],B.Max.Z)-Normal(Side)*15.;
    D.Add(EPart::Trim,At,Side<2?FVector(.3,Size.Y/100.,.6):FVector((Size.X-60.)/100.,.3,.6));
   }
   const int32 Count=FMath::Min(1+int32((Hash>>7)%3),FMath::Max(1,FMath::FloorToInt((FMath::Max(Size.X,Size.Y)-60.)/160.)));
   for(int32 V=0;V<Count;++V)
   {FVector At=B.GetCenter();At.Z=B.Max.Z;At[Size.X>=Size.Y?0:1]+=(V-(Count-1)*.5)*160.;D.Add(EPart::RoofVent,At,FVector(1.),Size.X>=Size.Y?0.:90.);}
  }
 }
}
}

void BattleKitLook::DressHouses(UWorld* World)
{
 if(!Active(World))return;
 FDresser D(World);TArray<FBoxPart> Boxes;TArray<FBox> NavBounds;
 for(TActorIterator<ANavMeshBoundsVolume> It(World);It;++It)NavBounds.Add(It->GetComponentsBoundingBox(true));
 for(TActorIterator<AStaticMeshActor> It(World);It;++It)
 {
  if(!It->HasAnyFlags(RF_WasLoaded)||It->HasAnyFlags(RF_Transient))continue;
  UStaticMeshComponent* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C?C->GetStaticMesh().Get():nullptr;
  if(!Mesh||!C->HasAnyFlags(RF_WasLoaded)||C->HasAnyFlags(RF_Transient))continue;
  const FString Label=It->GetActorNameOrLabel();if(LabelIs(Label,TEXT("Ground")))D.Ground.Add(C);
  if(Mesh->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")||!C->IsVisible()||It->ActorHasTag(TEXT("BattleTerrain")))continue;
  const FTransform T=C->GetComponentTransform();if(T.ContainsNaN()||T.GetScale3D().GetMin()<=0.||T.GetUnitAxis(EAxis::Z).Z<.9999)continue;
  // Layouts use axis-aligned boxes. Do not approximate a hand-rotated building with an enlarged AABB.
  const FVector X=T.GetUnitAxis(EAxis::X);if(FMath::Max(FMath::Abs(X.X),FMath::Abs(X.Y))<.9999)continue;
  FBoxPart P;P.Source=C;P.Bounds=Mesh->GetBoundingBox().TransformBy(T);P.bSill=LabelIs(Label,TEXT("HouseSill"));P.bHouse=P.bSill||LabelIs(Label,TEXT("HouseWall"));
  P.Kind=BuildingKind(Label);const FVector Size=P.Bounds.GetSize();
  if(!P.bHouse&&(P.Kind.IsEmpty()||Size.Z<250.||FMath::Min(Size.X,Size.Y)<300.))continue;
  TInlineComponentArray<UNavModifierComponent*> Modifiers;It->GetComponents(Modifiers);
  for(const auto* Modifier:Modifiers)if(Modifier->GetFName()==TEXT("NoRoof"))P.bNoRoof=true;
  Boxes.Add(MoveTemp(P));
 }
 const TArray<FGroup> Houses=GroupBoxes(Boxes,true),Buildings=GroupBoxes(Boxes,false);
 for(const FGroup& G:Houses)OpenHouse(D,G,Boxes);
 for(const FGroup& G:Buildings)
 {const uint32 Hash=PositionHash(G.Bounds.GetCenter());for(int32 I:G.Members)SolidBuilding(D,I,Hash,Boxes,NavBounds);}
 // Where to point a camera for a look check (Setup/Shot-KitLook.ps1 -Cams): the first few of each kind.
 for(int32 I=0;I<FMath::Min(4,Houses.Num());++I){const FVector C=Houses[I].Bounds.GetCenter(),S=Houses[I].Bounds.GetSize();UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KITLOOK_HOUSE_AT kind=house x=%.0f y=%.0f z=%.0f w=%.0f h=%.0f"),C.X,C.Y,Houses[I].Bounds.Min.Z,S.X,S.Y);}
 for(int32 I=0;I<FMath::Min(6,Buildings.Num());++I){const FVector C=Buildings[I].Bounds.GetCenter(),S=Buildings[I].Bounds.GetSize();UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KITLOOK_HOUSE_AT kind=%s x=%.0f y=%.0f z=%.0f w=%.0f h=%.0f top=%.0f"),*Boxes[Buildings[I].Members[0]].Kind,C.X,C.Y,Buildings[I].Bounds.Min.Z,S.X,S.Y,S.Z);}
 int32 Windows=0,Doors=0,Roofs=0,Instances=0;D.Flush(Windows,Doors,Roofs,Instances);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_KITLOOK_HOUSES houses=%d buildings=%d windows=%d doors=%d roofs=%d tinted=%d instances=%d missing_meshes=%d"),Houses.Num(),Buildings.Num(),Windows,Doors,Roofs,D.Tinted,Instances,D.Missing);
}
