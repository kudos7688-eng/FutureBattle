#include "BattleKitProps.h"
#include "BattleKitLook.h"
#include "FutureBattle.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"

namespace BattleKitLook
{
namespace
{
 enum class EProp : uint8 {JerseyBarrier,SandbagWall,StoneWall,Hedge,WoodCrate,Hesco,OilBarrel,Container,Boulder,CarWreck,Truck,TreeBroadleaf,TreeConifer,HedgeTall,RuinWall,Count};
 const TCHAR* const PropNames[]={TEXT("JerseyBarrier"),TEXT("SandbagWall"),TEXT("StoneWall"),TEXT("Hedge"),TEXT("WoodCrate"),TEXT("Hesco"),TEXT("OilBarrel"),TEXT("Container"),TEXT("Boulder"),TEXT("CarWreck"),TEXT("Truck"),TEXT("TreeBroadleaf"),TEXT("TreeConifer"),TEXT("HedgeTall"),TEXT("RuinWall")};
 constexpr int32 PropCount=int32(EProp::Count);
 static_assert(UE_ARRAY_COUNT(PropNames)==PropCount,"Update prop names");
 enum class EArrangement : uint8 {Single,Wall,Barrels,Tree};
 struct FDressRule
 {
  const TCHAR* Materials;
  const TCHAR* Labels; // Empty = material is unambiguous. Labels are NOT used for randomness.
  FVector Minimum,Maximum; // long horizontal edge, short edge, height in cm
  EArrangement Arrangement;
  EProp Choices[3];int32 ChoiceCount;
 };
 // The only classification table. Shared grey material also paints houses/bridges: use an allow-list of
 // saved layout labels, never broad matches such as "Wall". With stripped labels, ambiguous grey boxes stay.
 // Only named broken RuinWall pieces may be dressed; other architectural shells stay. No tank traps in these layouts.
 const FDressRule DressRules[]={
  {TEXT("M_MapTrunk"),TEXT(""),FVector(40,40,350),FVector(180,180,1800),EArrangement::Tree,{EProp::TreeBroadleaf,EProp::TreeConifer},2},
  {TEXT("M_MapTree|M_ArenaCover"),TEXT("Hedge"),FVector(200,40,160),FVector(30000,160,300),EArrangement::Wall,{EProp::HedgeTall},1},
  {TEXT("M_MapTree|M_ArenaCover"),TEXT("Hedge"),FVector(200,40,60),FVector(30000,160,159.99),EArrangement::Wall,{EProp::Hedge},1},
  {TEXT("M_MapRuin"),TEXT("RuinWall"),FVector(200,20,60),FVector(30000,300,300),EArrangement::Wall,{EProp::RuinWall},1},
  {TEXT("M_MapRust"),TEXT("RoadLorry"),FVector(400,180,220),FVector(650,350,320),EArrangement::Single,{EProp::Truck},1},
  {TEXT("M_ArenaCover|M_MapRust"),TEXT("Car|YardCar|PlazaCar|Wreck"),FVector(300,150,120),FVector(500,300,160),EArrangement::Single,{EProp::CarWreck},1},
  {TEXT("M_ArenaCover|M_MapRust"),TEXT("Container|DepotCrates|DepotCrate"),FVector(300,150,161),FVector(650,300,300),EArrangement::Single,{EProp::Container},1},
  {TEXT("M_ArenaCover|M_MapHill"),TEXT("Barrier|Sandbags|LookoutSandbags|GardenWall|PlanterWall|YardWall|PlazaWall|DepotWall|HomeYardWall|StoneWall|LaneWall|ParcelWall|HillWall|CentralCover|WestLowCover"),FVector(200,20,70),FVector(30000,130,160),EArrangement::Wall,{EProp::JerseyBarrier,EProp::SandbagWall,EProp::StoneWall},3},
  {TEXT("M_ArenaCover|M_MapHouse|M_MapRust"),TEXT("Crate|HouseCrate|UpperCrate|WarehouseCrate|PlazaCrate|YardCrate|DepotCrate|HomeCrate|Bin"),FVector(120,60,100),FVector(160,100,160),EArrangement::Barrels,{EProp::OilBarrel},1},
  {TEXT("M_ArenaCover|M_MapHouse|M_MapRust"),TEXT("Crate|HouseCrate|UpperCrate|WarehouseCrate|PlazaCrate|YardCrate|DepotCrate|HomeCrate|Bin"),FVector(60,60,60),FVector(160,160,160),EArrangement::Single,{EProp::WoodCrate,EProp::Hesco},2},
  {TEXT("M_MapRock"),TEXT(""),FVector(100,100,100),FVector(400,400,400),EArrangement::Single,{EProp::Boulder},1},
  {TEXT("M_ArenaCover"),TEXT("Boulder|RiverRock"),FVector(100,100,100),FVector(400,400,160),EArrangement::Single,{EProp::Boulder},1}
 };
 bool Listed(const FString& Value,const TCHAR* List,bool bLabel=false)
 {
  TArray<FString> Entries;FString(List).ParseIntoArray(Entries,TEXT("|"));
  for(const FString& Entry:Entries)
  {
   if(Value==Entry)return true;
   if(bLabel&&Value.StartsWith(Entry))
   {
    FString Suffix=Value.Mid(Entry.Len());Suffix.RemoveFromStart(TEXT("_"));
    if(!Suffix.IsEmpty()&&Suffix.IsNumeric())return true;
   }
  }
  return false;
 }
 bool InRange(const FVector& Value,const FVector& Low,const FVector& High)
 {return Value.X>=Low.X&&Value.Y>=Low.Y&&Value.Z>=Low.Z&&Value.X<=High.X&&Value.Y<=High.Y&&Value.Z<=High.Z;}
 struct FDressBox
 {
  UStaticMeshComponent* Source=nullptr;
  FString Material,Label;
  FVector Center,Size;
  FQuat Rotation;
  bool bUsed=false;
 };
 uint32 PositionHash(const FVector& Position)
 {
  // FNV-1a over explicit cm-rounded integers; no engine RNG, FName IDs or object addresses.
  uint32 Hash=2166136261u;
  for(double Coordinate:{Position.X,Position.Y,Position.Z})
  {
   const uint64 Bits=uint64(FMath::RoundToInt64(Coordinate));
   for(int32 Byte=0;Byte<8;++Byte){Hash^=uint32((Bits>>(Byte*8))&255u);Hash*=16777619u;}
  }
  return Hash;
 }
 FIntPoint TreeKey(const FVector& Position){return FIntPoint(FMath::RoundToInt(Position.X),FMath::RoundToInt(Position.Y));}
 struct FPropBatch
 {
  bool bTried=false;
  TStrongObjectPtr<UStaticMesh> Mesh;
  TArray<FTransform> Transforms;
  TArray<UStaticMeshComponent*> Hide;
 };
 UStaticMesh* PropMesh(FPropBatch& Batch,EProp Kind,int32& Missing)
 {
  if(!Batch.bTried)
  {
   Batch.bTried=true;const TCHAR* Name=PropNames[int32(Kind)];
   const FString Path=FString::Printf(TEXT("/Game/Kit/Props/SM_Kit_%s.SM_Kit_%s"),Name,Name);
   Batch.Mesh.Reset(LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn));
   if(!Batch.Mesh.IsValid())++Missing;
  }
  return Batch.Mesh.Get();
 }
 bool Fit(const FDressBox& Box,UStaticMesh* Mesh,EArrangement Arrangement,uint32 Hash,TArray<FTransform>& Out)
 {
  const FBox Bounds=Mesh->GetBoundingBox();const FVector Native=Bounds.GetSize();
  if(!Bounds.IsValid||Native.ContainsNaN()||Native.GetMin()<=KINDA_SMALL_NUMBER)return false;
  const bool bTree=Arrangement==EArrangement::Tree;
  const bool bAlongY=Box.Size.Y>Box.Size.X;
  const int32 Quarter=bTree?int32((Hash>>8)%4):(bAlongY?1:0)+int32((Hash>>8)%2)*2;
  const FVector Target=(Quarter%2)?FVector(Box.Size.Y,Box.Size.X,Box.Size.Z):Box.Size;
  const int32 Segments=Arrangement==EArrangement::Wall?FMath::Max(1,FMath::RoundToInt(Target.X/Native.X)):
   Arrangement==EArrangement::Barrels?2+int32((Hash>>10)%2):1;
  if(Segments>256)return false;
  const FVector Part(Target.X/Segments,Target.Y,Target.Z),Scale=Part/Native;
  if(!InRange(Scale,FVector(bTree?.4:.6),FVector(bTree?2.5:1.6)))return false;
  const FQuat Turn=Box.Rotation*FRotator(0,Quarter*90.,0).Quaternion();
  for(int32 I=0;I<Segments;++I)
  {
   // Align the mesh's actual bounding-box TOP, not its assumed pivot, to the cube's top. This also
   // preserves map L's cut boxes: their extra foundation remains below the same visible top, never an added sink.
   const FVector Top=Box.Center+Turn.RotateVector(FVector(-Target.X*.5+Part.X*(I+.5),0,Target.Z*.5));
   const FVector LocalTop(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Max.Z);
   Out.Emplace(Turn,Top-Turn.RotateVector(LocalTop*Scale),Scale);
  }
  return true;
 }
}
}

BattleKitLook::FPropsResult BattleKitLook::Dress(UWorld* World)
{
 FPropsResult Result;if(!Active(World))return Result;
 TArray<FDressBox> Boxes;TMap<FIntPoint,TArray<int32>> Crowns;TMap<FIntPoint,int32> Trunks;
 for(TActorIterator<AStaticMeshActor> It(World);It;++It)
 {
  if(!It->HasAnyFlags(RF_WasLoaded)||It->HasAnyFlags(RF_Transient)||It->ActorHasTag(TEXT("BattleTerrain")))continue;
  auto* Source=It->GetStaticMeshComponent();const auto* Mesh=Source?Source->GetStaticMesh():nullptr;
  if(!Mesh||Mesh->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")||!Source->IsVisible()||
   !Source->HasAnyFlags(RF_WasLoaded)||Source->HasAnyFlags(RF_Transient))continue;
  const auto* Material=Source->GetMaterial(0);if(!Material)continue;
  const FTransform Transform=Source->GetComponentTransform();
  // Keep mirrored/tilted or malformed authored geometry intact. Yaw rotation is fully preserved.
  if(Transform.ContainsNaN()||Transform.GetScale3D().GetMin()<=0.||Transform.GetUnitAxis(EAxis::Z).Z<.9999)continue;
  FDressBox Box;Box.Source=Source;Box.Material=Material->GetName();Box.Label=It->GetActorNameOrLabel();
  Box.Center=Transform.TransformPosition(Mesh->GetBoundingBox().GetCenter());
  Box.Size=Mesh->GetBoundingBox().GetSize()*Transform.GetScale3D();Box.Rotation=Transform.GetRotation();
  const int32 Index=Boxes.Add(Box);if(Box.Material==TEXT("M_MapTree"))Crowns.FindOrAdd(TreeKey(Box.Center)).Add(Index);
  if(Box.Material==TEXT("M_MapTrunk"))++Trunks.FindOrAdd(TreeKey(Box.Center));
 }
 FPropBatch Batches[PropCount];
 for(int32 Index=0;Index<Boxes.Num();++Index)
 {
  FDressBox& Box=Boxes[Index];if(Box.bUsed)continue;
  const FVector Dimensions(FMath::Max(Box.Size.X,Box.Size.Y),FMath::Min(Box.Size.X,Box.Size.Y),Box.Size.Z);
  const uint32 Hash=PositionHash(Box.Center);
  for(const FDressRule& Rule:DressRules)
  {
   if(!Listed(Box.Material,Rule.Materials)||!InRange(Dimensions,Rule.Minimum,Rule.Maximum)||
    (Rule.Labels[0]&&!Listed(Box.Label,Rule.Labels,true)))continue;
   FDressBox Target=Box;int32 CrownIndex=INDEX_NONE;
   if(Rule.Arrangement==EArrangement::Tree)
   {
    if(Trunks.FindRef(TreeKey(Box.Center))!=1)continue;
    const auto* Matches=Crowns.Find(TreeKey(Box.Center));if(!Matches)continue;
    int32 Candidates=0;
    for(int32 Candidate:*Matches)
    {
     const FDressBox& Crown=Boxes[Candidate];
     const double CrownBottom=Crown.Center.Z-Crown.Size.Z*.5,TrunkTop=Box.Center.Z+Box.Size.Z*.5;
     if(!Crown.bUsed&&FVector::DistSquared2D(Crown.Center,Box.Center)<1.&&
      CrownBottom>Box.Center.Z&&CrownBottom<TrunkTop&&Crown.Center.Z+Crown.Size.Z*.5>=TrunkTop&&
      Crown.Size.X>=Box.Size.X&&Crown.Size.Y>=Box.Size.Y){CrownIndex=Candidate;++Candidates;}
    }
    if(Candidates!=1)continue; // ambiguous pairs are left untouched, regardless of actor iteration order
    const FDressBox& Crown=Boxes[CrownIndex];
    const double Bottom=Box.Center.Z-Box.Size.Z*.5,Top=Crown.Center.Z+Crown.Size.Z*.5;
    Target.Center=FVector(Crown.Center.X,Crown.Center.Y,(Bottom+Top)*.5);
    Target.Size=FVector(Crown.Size.X,Crown.Size.Y,Top-Bottom);Target.Rotation=Crown.Rotation;
   }
   if(Rule.Choices[0]==EProp::Boulder&&Dimensions.GetMax()>Dimensions.GetMin()*1.6)continue;
   // Starting choice and facing are hash-derived. Try only geometrically compatible alternatives.
   for(int32 Choice=0;Choice<Rule.ChoiceCount;++Choice)
   {
    const EProp Kind=Rule.Choices[(Hash+uint32(Choice))%uint32(Rule.ChoiceCount)];FPropBatch& Batch=Batches[int32(Kind)];
    UStaticMesh* Mesh=PropMesh(Batch,Kind,Result.Missing);if(!Mesh)continue;
    TArray<FTransform> Placements;if(!Fit(Target,Mesh,Rule.Arrangement,Hash,Placements))continue;
    Batch.Transforms.Append(Placements);Batch.Hide.Add(Box.Source);Box.bUsed=true;
    if(CrownIndex!=INDEX_NONE){Batch.Hide.Add(Boxes[CrownIndex].Source);Boxes[CrownIndex].bUsed=true;}
    break;
   }
   if(Box.bUsed)break;
  }
 }
 int32 Counts[PropCount]={};
 AActor* Host=nullptr;
 for(int32 KindIndex=0;KindIndex<PropCount;++KindIndex)
 {
  FPropBatch& Batch=Batches[KindIndex];if(Batch.Transforms.IsEmpty())continue;
  if(!Host)
  {
   FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
   Host=World->SpawnActor<AActor>(Spawn);if(!Host)break;
  }
  auto* Instances=NewObject<UInstancedStaticMeshComponent>(Host,NAME_None,RF_Transient);
  // Configure while unregistered, with no mesh and no instances yet. Only these NEW visual components
  // receive collision/navigation settings; none of the original actors/components is reconfigured.
  Instances->SetCanEverAffectNavigation(false);
  Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);Instances->SetGenerateOverlapEvents(false);
  Instances->SetMobility(EComponentMobility::Static);Instances->SetStaticMesh(Batch.Mesh.Get());
  Host->AddInstanceComponent(Instances);
  // Identity component transform; placements are world coordinates. Explicitly disable navigation updates.
  Instances->AddInstances(Batch.Transforms,false,false,false);
  Instances->RegisterComponent();
  if(!Instances->IsRegistered()||Instances->GetInstanceCount()!=Batch.Transforms.Num()){Instances->DestroyComponent();continue;}
  // Hide only after the entire batch exists. Visibility never changes the source's collision or navigation.
  for(UStaticMeshComponent* Source:Batch.Hide)
  {Source->SetVisibility(false);Source->SetCastShadow(false);++Result.Cubes;}
  Counts[KindIndex]=Batch.Transforms.Num();Result.Instances+=Counts[KindIndex];
 }
 FString Detail=FString::Printf(TEXT("BATTLE_KITLOOK_PROPS Tree=%d"),Counts[int32(EProp::TreeBroadleaf)]+Counts[int32(EProp::TreeConifer)]);
 for(int32 KindIndex=0;KindIndex<PropCount;++KindIndex)Detail+=FString::Printf(TEXT(" %s=%d"),PropNames[KindIndex],Counts[KindIndex]);
 UE_LOG(LogFutureBattle,Display,TEXT("%s"),*Detail);
 return Result;
}
