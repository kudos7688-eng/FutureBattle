// Map statistics (-BattleMapStats="<file>.json", Setup/Test-MapStats.ps1): numbers that describe a wide map as built, so layout
// decisions rest on measurements. Static geometry and navigation only; soldiers are ignored and no match is played.
//  - per zone (city / flank / approach / corner, from FMapDef::CityHalf): how far a standing soldier sees in 16 directions,
//    the free space around a point (can a tank stand there), cover slots and obstacles per hectare;
//  - per route (home to each objective and to the enemy home): length on foot, whether a vehicle arrives, and the share of the
//    route that the lookouts and the objectives overlook.
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleMatch.h"
#include "BattleCover.h"
#include "BattleVehicleNavMesh.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/NavDataGenerator.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "FutureBattle.h"

namespace
{
 struct FMapZone
 {
  FString Name;double AreaHa=0;int32 Points=0,Slots=0,Low=0,Mid=0,Tall=0,Conceal=0;
  TArray<double> Sight,Longest,Clear;
 };
 double Quantile(TArray<double>& V,double Q){if(V.IsEmpty())return -1;V.Sort();return V[FMath::Clamp(FMath::RoundToInt(Q*(V.Num()-1)),0,V.Num()-1)];}
 double ShareAtLeast(const TArray<double>& V,double Min){if(V.IsEmpty())return -1;int32 N=0;for(double D:V)N+=D>=Min;return double(N)/V.Num();}
 double Round1(double V){return FMath::RoundToDouble(V*10.)/10.;}
 double Round3(double V){return FMath::RoundToDouble(V*1000.)/1000.;}
}

void UBattleLeaderRuntime::RunMapStats()
{
 UWorld* W=GetWorld();FString File;FParse::Value(FCommandLine::Get(),TEXT("BattleMapStats="),File);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
 const ANavigationData* Foot=Nav?Nav->GetDefaultNavDataInstance():nullptr;
 ANavigationData* Vehicle=nullptr;for(TActorIterator<ABattleVehicleNavMesh> It(W);It;++It){Vehicle=*It;break;}
 auto Building=[](const ANavigationData* D){return D&&D->GetGenerator()&&D->GetGenerator()->IsBuildInProgressCheckDirty();};
 // Both navmeshes must be complete (the vehicle one is built at runtime); give up waiting after two minutes and say so.
 if((!Foot||Building(Foot)||Building(Vehicle))&&W->GetTimeSeconds()<120.f)
 {FTimerHandle H;W->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMapStats,1.f,false);return;}
 const auto& Def=BattleFieldLayout::Of(W);const FVector2D Half=Def.HalfExtent,City=Def.CityHalf;
 // The same vehicle path budget as a tank gets in a match on this map (UBattleLeaderRuntime::NavFor).
 if(auto* Recast=Cast<ARecastNavMesh>(Vehicle);Recast&&Def.VehicleSearchNodes>0)
 {Recast->DefaultMaxSearchNodes=float(Def.VehicleSearchNodes);const_cast<FNavigationQueryFilter&>(*Recast->GetDefaultQueryFilter()).SetMaxSearchNodes(Def.VehicleSearchNodes);}
 const auto* Tuning=GetDefault<UBattleTuning>();auto* Match=W->GetSubsystem<UBattleMatch>();

 TArray<FMapZone> Zones;
 if(City.X>0)
 {
  Zones.SetNum(4);
  Zones[0].Name=TEXT("city");Zones[0].AreaHa=4*City.X*City.Y/1.e8;
  Zones[1].Name=TEXT("flank");Zones[1].AreaHa=4*City.X*(Half.Y-City.Y)/1.e8;
  Zones[2].Name=TEXT("approach");Zones[2].AreaHa=4*(Half.X-City.X)*City.Y/1.e8;
  Zones[3].Name=TEXT("corner");Zones[3].AreaHa=4*(Half.X-City.X)*(Half.Y-City.Y)/1.e8;
 }
 else{Zones.SetNum(1);Zones[0].Name=TEXT("field");Zones[0].AreaHa=4*Half.X*Half.Y/1.e8;}
 auto ZoneOf=[&](const FVector& P){if(City.X<=0)return 0;const bool InX=FMath::Abs(P.X)<City.X,InY=FMath::Abs(P.Y)<City.Y;return InX&&InY?0:InX?1:InY?2:3;};

 // Static geometry only: soldiers and vehicles do not count.
 const FCollisionObjectQueryParams Static(ECC_WorldStatic);const FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleMapStats),false);
 auto Ray=[&](const FVector& From,const FVector& To){FHitResult Hit;return W->LineTraceSingleByObjectType(Hit,From,To,Static,Query)?double(Hit.Distance):FVector::Dist(From,To);};
 auto Sees=[&](const FVector& From,const FVector& To){FHitResult Hit;return !W->LineTraceSingleByObjectType(Hit,From+FVector(0,0,150),To+FVector(0,0,150),Static,Query);};
 const double SightReach=60000,ClearReach=5000,Cell=2500;
 for(double X=-Half.X+Cell*.5;X<Half.X;X+=Cell) for(double Y=-Half.Y+Cell*.5;Y<Half.Y;Y+=Cell)
 {
  // At the height of the base ground (the field of map L; 0 elsewhere).
  FNavLocation Ground;if(!Nav||!Nav->ProjectPointToNavigation(FVector(X,Y,BattleFieldLayout::Of(W).Heights.Base(X,Y)),Ground,FVector(60,60,200)))continue;
  FMapZone& Z=Zones[ZoneOf(Ground.Location)];++Z.Points;
  double Longest=0,Clear=ClearReach;
  for(int32 I=0;I<16;++I)
  {
   const double A=2*PI*I/16;const FVector Dir(FMath::Cos(A),FMath::Sin(A),0);
   const FVector Eye(Ground.Location.X,Ground.Location.Y,150),Low(Ground.Location.X,Ground.Location.Y,60);
   const double D=Ray(Eye,Eye+Dir*SightReach);Z.Sight.Add(D/100.);Longest=FMath::Max(Longest,D);
   Clear=FMath::Min(Clear,Ray(Low,Low+Dir*ClearReach));
  }
  Z.Longest.Add(Longest/100.);Z.Clear.Add(Clear/100.);
 }
 for(TActorIterator<AStaticMeshActor> It(W);It;++It)
 {
  FVector Center,Extent;It->GetActorBounds(true,Center,Extent);const double Top=Center.Z+Extent.Z;
  // Floors, ground markers and the boundary walls are not obstacles.
  if(Top<10||Extent.X>20000||Extent.Y>20000||BattleTerrain::IsGround(*It))continue;
  FMapZone& Z=Zones[ZoneOf(Center)];
  if(It->ActorHasTag(TEXT("BattleConcealment")))++Z.Conceal;else if(Top<150)++Z.Low;else if(Top<350)++Z.Mid;else ++Z.Tall;
 }
 auto* Cover=W->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 for(const FVector& S:Cover->GetSlots())++Zones[ZoneOf(S)].Slots;

 TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
 Root->SetStringField(TEXT("map"),W->GetMapName());
 Root->SetNumberField(TEXT("floor_x_m"),Half.X/50.);Root->SetNumberField(TEXT("floor_y_m"),Half.Y/50.);
 Root->SetNumberField(TEXT("city_x_m"),City.X/50.);Root->SetNumberField(TEXT("city_y_m"),City.Y/50.);
 Root->SetNumberField(TEXT("waited_seconds"),Round1(W->GetTimeSeconds()));
 Root->SetBoolField(TEXT("foot_nav_ready"),Foot&&!Building(Foot));Root->SetBoolField(TEXT("vehicle_nav_present"),Vehicle!=nullptr);
 Root->SetBoolField(TEXT("vehicle_nav_ready"),Vehicle&&!Building(Vehicle));
 Root->SetStringField(TEXT("scope"),TEXT("Static geometry only. sight_m: 16 rays per 25m grid point at 150cm, capped at 600m. clearance_m: nearest obstacle at 60cm, capped at 50m. Heights: low <150cm, mid <350cm, tall above."));
 TArray<TSharedPtr<FJsonValue>> ZoneRows;
 for(FMapZone& Z:Zones)
 {
  TSharedRef<FJsonObject> O=MakeShared<FJsonObject>();
  O->SetStringField(TEXT("zone"),Z.Name);O->SetNumberField(TEXT("area_ha"),Round1(Z.AreaHa));O->SetNumberField(TEXT("sample_points"),Z.Points);
  TSharedRef<FJsonObject> S=MakeShared<FJsonObject>();
  S->SetNumberField(TEXT("p25"),Round1(Quantile(Z.Sight,.25)));S->SetNumberField(TEXT("median"),Round1(Quantile(Z.Sight,.5)));
  S->SetNumberField(TEXT("p75"),Round1(Quantile(Z.Sight,.75)));S->SetNumberField(TEXT("p90"),Round1(Quantile(Z.Sight,.9)));
  S->SetNumberField(TEXT("share_100m"),Round3(ShareAtLeast(Z.Sight,100)));S->SetNumberField(TEXT("share_150m"),Round3(ShareAtLeast(Z.Sight,150)));
  S->SetNumberField(TEXT("share_200m"),Round3(ShareAtLeast(Z.Sight,200)));S->SetNumberField(TEXT("longest_ray_median"),Round1(Quantile(Z.Longest,.5)));
  O->SetObjectField(TEXT("sight_m"),S);
  TSharedRef<FJsonObject> C=MakeShared<FJsonObject>();
  C->SetNumberField(TEXT("p10"),Round1(Quantile(Z.Clear,.1)));C->SetNumberField(TEXT("median"),Round1(Quantile(Z.Clear,.5)));
  C->SetNumberField(TEXT("share_2_5m"),Round3(ShareAtLeast(Z.Clear,2.5)));C->SetNumberField(TEXT("share_10m"),Round3(ShareAtLeast(Z.Clear,10)));
  O->SetObjectField(TEXT("clearance_m"),C);
  const double Ha=FMath::Max(.01,Z.AreaHa);
  O->SetNumberField(TEXT("cover_slots"),Z.Slots);O->SetNumberField(TEXT("cover_slots_per_ha"),Round1(Z.Slots/Ha));
  O->SetNumberField(TEXT("obstacles_low"),Z.Low);O->SetNumberField(TEXT("obstacles_mid"),Z.Mid);O->SetNumberField(TEXT("obstacles_tall"),Z.Tall);
  O->SetNumberField(TEXT("concealment"),Z.Conceal);O->SetNumberField(TEXT("obstacles_per_ha"),Round1((Z.Low+Z.Mid+Z.Tall+Z.Conceal)/Ha));
  ZoneRows.Add(MakeShared<FJsonValueObject>(O));
 }
 Root->SetArrayField(TEXT("zones"),ZoneRows);

 // Routes from the west home (the map is point symmetric, so the east home mirrors them).
 const FVector Home(-Def.HomeDistance,0,0),EnemyHome(Def.HomeDistance,0,0);
 TArray<TPair<FString,FVector>> Goals;
 if(Match)for(int32 I=0;I<Match->Objectives.Num();++I)Goals.Emplace(FString::Printf(TEXT("objective_%d"),I),Match->Objectives[I].Position);
 Goals.Emplace(TEXT("enemy_home"),EnemyHome*(25./27.));
 const TArray<FVector> Lookouts=Def.Lookouts;const double LookoutRange=Def.bUrban?Tuning->LookoutSightRangeProfile:Tuning->LookoutSightRange;
 const double PointRange=Def.bUrban?20000.:10000.;
 TArray<TSharedPtr<FJsonValue>> RouteRows;
 for(const auto& Goal:Goals)
 {
  TSharedRef<FJsonObject> O=MakeShared<FJsonObject>();
  O->SetStringField(TEXT("to"),Goal.Key);O->SetNumberField(TEXT("x_m"),Goal.Value.X/100.);O->SetNumberField(TEXT("y_m"),Goal.Value.Y/100.);
  O->SetNumberField(TEXT("straight_m"),Round1(FVector::Dist2D(Home,Goal.Value)/100.));
  auto* Path=Nav?Nav->FindPathToLocationSynchronously(W,Home,Goal.Value):nullptr;
  const bool bPath=Path&&Path->IsValid()&&Path->PathPoints.Num()>1;
  O->SetBoolField(TEXT("foot_complete"),bPath&&!Path->IsPartial());
  if(bPath)
  {
   const double Length=Path->GetPathLength();
   O->SetNumberField(TEXT("foot_m"),Round1(Length/100.));O->SetNumberField(TEXT("foot_seconds"),Round1(Length/FMath::Max(1.f,Tuning->MoveSpeed)));
   // Every 10m of the route: overlooked from a lookout, or from an objective other than the goal.
   int32 Samples=0,FromLookout=0,FromPoint=0;
   for(int32 I=0;I+1<Path->PathPoints.Num();++I)
   {
    const FVector A=Path->PathPoints[I],B=Path->PathPoints[I+1];const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist(A,B)/1000.));
    for(int32 S=0;S<Steps;++S)
    {
     const FVector P=FMath::Lerp(A,B,double(S)/Steps);++Samples;
     bool L=false;for(const FVector& Look:Lookouts)L|=FVector::Dist2D(P,Look)<=LookoutRange&&Sees(P,Look);
     bool Pt=false;if(Match)for(const auto& Obj:Match->Objectives)Pt|=!Obj.Position.Equals(Goal.Value,1)&&FVector::Dist2D(P,Obj.Position)<=PointRange&&Sees(P,Obj.Position);
     FromLookout+=L;FromPoint+=Pt;
    }
   }
   O->SetNumberField(TEXT("seen_from_lookouts"),Round3(double(FromLookout)/FMath::Max(1,Samples)));
   O->SetNumberField(TEXT("seen_from_objectives"),Round3(double(FromPoint)/FMath::Max(1,Samples)));
  }
  // Vehicle: does the wide agent arrive, and how far short does it stop.
  FNavLocation From,To;
  if(Vehicle&&Nav->ProjectPointToNavigation(Home,From,FVector(600,600,400),Vehicle))
  {
   const bool bGoalOnNav=Nav->ProjectPointToNavigation(Goal.Value,To,FVector(600,600,400),Vehicle);
   FPathFindingQuery VehicleQuery(nullptr,*Vehicle,From.Location,bGoalOnNav?To.Location:Goal.Value);VehicleQuery.SetAllowPartialPaths(true);
   const FPathFindingResult Result=Nav->FindPathSync(VehicleQuery);
   const bool bFound=Result.IsSuccessful()&&Result.Path.IsValid()&&Result.Path->GetPathPoints().Num()>1;
   O->SetBoolField(TEXT("vehicle_goal_on_nav"),bGoalOnNav);O->SetBoolField(TEXT("vehicle_arrives"),bFound&&bGoalOnNav&&!Result.Path->IsPartial());
   if(bFound)
   {
    O->SetNumberField(TEXT("vehicle_m"),Round1(Result.Path->GetLength()/100.));
    O->SetNumberField(TEXT("vehicle_stops_short_m"),Round1(FVector::Dist2D(Result.Path->GetPathPoints().Last().Location,Goal.Value)/100.));
    // Where the goal was put on the vehicle mesh and where the route ends (a goal on a patch not joined to the roads gives a partial route).
    const FVector EndAt=Result.Path->GetPathPoints().Last().Location;
    O->SetNumberField(TEXT("vehicle_goal_x_m"),Round1((bGoalOnNav?To.Location.X:Goal.Value.X)/100.));O->SetNumberField(TEXT("vehicle_goal_y_m"),Round1((bGoalOnNav?To.Location.Y:Goal.Value.Y)/100.));
    O->SetNumberField(TEXT("vehicle_end_x_m"),Round1(EndAt.X/100.));O->SetNumberField(TEXT("vehicle_end_y_m"),Round1(EndAt.Y/100.));
   }
  }
  RouteRows.Add(MakeShared<FJsonValueObject>(O));
 }
 Root->SetArrayField(TEXT("routes_from_west_home"),RouteRows);

 FString Text;const TSharedRef<TJsonWriter<>> Writer=TJsonWriterFactory<>::Create(&Text);
 const bool bSaved=FJsonSerializer::Serialize(Root,Writer)&&!File.IsEmpty()&&FFileHelper::SaveStringToFile(Text,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MAP_STATS_RESULT failures=%d zones=%d routes=%d slots=%d file=%s"),bSaved?0:1,Zones.Num(),RouteRows.Num(),Cover->GetSlots().Num(),*File);
 FPlatformMisc::RequestExitWithStatus(false,bSaved?0:1);
}
