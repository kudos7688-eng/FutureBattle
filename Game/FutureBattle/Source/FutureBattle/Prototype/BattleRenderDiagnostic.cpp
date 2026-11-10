#include "BattleRenderDiagnostic.h"
#include "BattleSquadTrial.h"
#include "BattleViewVisibility.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"
#include "RenderUtils.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace {
bool InView(const FBattleReplayCamera& C,FVector P,double Radius=250.) {
 const FVector V=FRotator(C.Pitch,C.Yaw,C.Roll).UnrotateVector(P-FVector(C.X,C.Y,C.Z));
 const double H=FMath::Tan(FMath::DegreesToRadians(C.FOV*.5)),W=H*1080./1920.;
 return V.X>=-Radius && FMath::Abs(V.Y)<=V.X*H+Radius*FMath::Sqrt(1+H*H) && FMath::Abs(V.Z)<=V.X*W+Radius*FMath::Sqrt(1+W*W);
}
void SetVar(const TCHAR* N,int32 V) {auto* C=IConsoleManager::Get().FindConsoleVariable(N);checkf(C,TEXT("Missing T5 CVar %s"),N);C->Set(V,ECVF_SetByCode);}
void RegisterTree(USkeletalMeshComponent* M,bool Keep,int32& Changes) {
 if(M->IsRegistered()==Keep) return;
 TArray<USceneComponent*> Children;M->GetChildrenComponents(true,Children);
 if(Keep) {if(!M->IsRegistered()){M->RegisterComponent();++Changes;} for(auto* C:Children) if(!C->IsRegistered()) C->RegisterComponent();}
 else {for(auto* C:Children) if(C->IsRegistered()) C->UnregisterComponent();if(M->IsRegistered()){M->UnregisterComponent();++Changes;}}
}
}

bool UBattleSquadTrial::ConfigureT5Diagnostic() {
 FString Stage;if(!FParse::Value(FCommandLine::Get(),TEXT("T5-Stage="),Stage)) return true;
 T5Diagnostic=MakeShared<FT5RenderDiagnostic>();auto& D=*T5Diagnostic;D.Stage=Stage;D.Profile=TEXT("Current");
 FParse::Value(FCommandLine::Get(),TEXT("T5-Profile="),D.Profile);
 if(Stage!=TEXT("A1") && Stage!=TEXT("A2") && Stage!=TEXT("A3") && Stage!=TEXT("Shadow") && Stage!=TEXT("B") && Stage!=TEXT("C") && Stage!=TEXT("D")) return false;
 const TArray<FString> Profiles={TEXT("Current"),TEXT("Software"),TEXT("Light"),TEXT("ClassicShadow"),TEXT("NoShadow"),TEXT("Medium"),TEXT("Low")};
 if(!Profiles.Contains(D.Profile)) return false;
 if(D.Profile!=TEXT("Current") && !FParse::Param(FCommandLine::Get(),TEXT("noraytracing"))) return false;
 // Reset to the original Epic baseline even if a prior process saved Low/Medium.
 {Scalability::FQualityLevels Q;Q.SetFromSingleQualityLevel(3);Q.ResolutionQuality=100.;Scalability::SetQualityLevels(Q,true);}
 if(D.Profile==TEXT("Medium") || D.Profile==TEXT("Low")) {Scalability::FQualityLevels Q;Q.SetFromSingleQualityLevel(D.Profile==TEXT("Medium")?1:0);Q.ResolutionQuality=100.;Scalability::SetQualityLevels(Q,true);}
 if(D.Profile!=TEXT("Current")) SetVar(TEXT("r.Lumen.HardwareRayTracing"),0);
 if(D.Profile==TEXT("Light") || D.Profile==TEXT("ClassicShadow") || D.Profile==TEXT("NoShadow")) {
  SetVar(TEXT("r.DynamicGlobalIlluminationMethod"),0);SetVar(TEXT("r.ReflectionMethod"),2);
  SetVar(TEXT("r.Lumen.DiffuseIndirect.Allow"),0);SetVar(TEXT("r.Lumen.Reflections.Allow"),0);
 }
 if(D.Profile==TEXT("ClassicShadow")) SetVar(TEXT("r.Shadow.Virtual.Enable"),0);
 if(D.Profile==TEXT("NoShadow")) SetVar(TEXT("r.ShadowQuality"),0);
 // Resolution remains native, including the reference scalability presets.
 SetVar(TEXT("r.ScreenPercentage"),100);
 D.Settings->SetBoolField(TEXT("rhi_supports_ray_tracing"),GRHISupportsRayTracing);
 D.Settings->SetBoolField(TEXT("ray_tracing_enabled"),IsRayTracingEnabled());
 if(D.Profile!=TEXT("Current") && (GRHISupportsRayTracing || IsRayTracingEnabled())) return false;
 for(const TCHAR* N:{TEXT("r.RayTracing"),TEXT("r.RayTracing.Enable"),TEXT("r.RayTracing.Scene.BuildMode"),TEXT("r.Lumen.HardwareRayTracing"),TEXT("r.DynamicGlobalIlluminationMethod"),TEXT("r.ReflectionMethod"),TEXT("r.Lumen.DiffuseIndirect.Allow"),TEXT("r.Lumen.Reflections.Allow"),TEXT("r.Lumen.FinalGatherMethod"),TEXT("r.Lumen.TraceMeshSDFs"),TEXT("r.Shadow.Virtual.Enable"),TEXT("r.ShadowQuality"),TEXT("r.Shadow.CSM.MaxCascades"),TEXT("r.Shadow.MaxResolution"),TEXT("r.SSR.Quality"),TEXT("r.DistanceFieldAO"),TEXT("r.ScreenPercentage"),TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality")}) {
  auto* V=IConsoleManager::Get().FindConsoleVariable(N);if(V) D.Settings->SetNumberField(N,V->GetFloat());
 }
 if(Stage==TEXT("C") || Stage==TEXT("D")) {
  FParse::Value(FCommandLine::Get(),TEXT("T5-LODPolicy="),D.LODPolicy);
  if(D.LODPolicy!=TEXT("Dist10_30") && D.LODPolicy!=TEXT("Dist20_50") && D.LODPolicy!=TEXT("Screen120_48")) return false;
  D.LODStress=Stage==TEXT("C") && FParse::Param(FCommandLine::Get(),TEXT("T5-LODStress"));
  double Percent=10.;FParse::Value(FCommandLine::Get(),TEXT("T5-Hysteresis="),Percent);if(Percent<0 || Percent>50) return false;D.SelectedHysteresis=Percent/100.;
  // Select a Mass soldier alive throughout the measured interval for the boundary stress camera.
  for(int32 I=0;I<500 && D.LODProbe<0;++I) if(!Members[I].Actor) {
   bool Alive=true;for(int32 F=0;F<900 && Alive;++F) {
    const auto* Units=reinterpret_cast<const FBattleReplayUnit*>(ReplayBytes.GetData()+32+int64(ReplayStart+F)*(28+500*32)+28);
    Alive=Units[I].Alive && Units[I].Visible;
   }
   if(Alive) D.LODProbe=I;
  }
  if(D.LODStress && D.LODProbe<0) return false;
 }
 if(Stage==TEXT("D") && !ConfigureT5Material()) return false;
 TArray<FString> Views,Filters;
 if(Stage==TEXT("A2")){Views={TEXT("Recorded"),TEXT("RTS60"),TEXT("TPS"),TEXT("FPS")};Filters={TEXT("All"),TEXT("None")};}
 if(Stage==TEXT("A1")){Views={TEXT("Recorded"),TEXT("TPS"),TEXT("FPS")};Filters={TEXT("Total0"),TEXT("Total250"),TEXT("Total500"),TEXT("Total1000")};}
 if(Stage==TEXT("A3")){Views={TEXT("TPS"),TEXT("FPS")};Filters={TEXT("All"),TEXT("None"),TEXT("FrustumHide"),TEXT("FrustumSleep"),TEXT("DistanceHide"),TEXT("DistanceSleep"),TEXT("KnowledgeProxy")};}
 if(Stage==TEXT("Shadow")){Views={TEXT("TPS"),TEXT("FPS")};Filters={TEXT("All"),TEXT("FrustumHide"),TEXT("FrustumShadow")};}
 if(Stage==TEXT("B")){Views={TEXT("Recorded"),TEXT("RTS60"),TEXT("TPS"),TEXT("FPS")};Filters={TEXT("All"),TEXT("None"),TEXT("LOD0"),TEXT("LOD1"),TEXT("LOD2"),TEXT("Dist10_30"),TEXT("Dist20_50"),TEXT("Screen120_48")};}
 if(Stage==TEXT("C")){Views={TEXT("TPS"),TEXT("FPS")};Filters={TEXT("Reference"),TEXT("H0"),TEXT("H10"),TEXT("H20"),TEXT("H30")};}
 if(Stage==TEXT("D")){Views={TEXT("Recorded"),TEXT("RTS60"),TEXT("TPS"),TEXT("FPS")};Filters={TEXT("All"),TEXT("None"),TEXT("Shadow30"),TEXT("Shadow60"),TEXT("Material40"),TEXT("Combined"),TEXT("Bias1"),TEXT("Bias1None")};}
 FString FilterOverride;if(FParse::Value(FCommandLine::Get(),TEXT("T5-Filters="),FilterOverride)) {
  TArray<FString> Selected;FilterOverride.ParseIntoArray(Selected,TEXT(","),true);TSet<FString> Unique;
  for(const auto& Filter:Selected) {if(!Filters.Contains(Filter) || Unique.Contains(Filter)) return false;Unique.Add(Filter);}
  if(Selected.IsEmpty()) return false;Filters=Selected;
 }
 for(auto& V:Views) for(auto& F:Filters) D.Cases.Add({V,F});
 T5Limit=D.Cases.Num()*2;int32 Limit=T5Limit;FParse::Value(FCommandLine::Get(),TEXT("T5-Conditions="),Limit);T5Limit=FMath::Clamp(Limit,1,T5Limit);
 UE_LOG(LogTemp,Display,TEXT("T5_DIAGNOSTIC stage=%s profile=%s conditions=%d hwrt=%d"),*Stage,*D.Profile,T5Limit,IsRayTracingEnabled());return true;
}

void UBattleSquadTrial::SelectT5Diagnostic() {
 auto& D=*T5Diagnostic;const int32 N=D.Cases.Num();int32 K=(T5Condition%N+Seed*7+(T5Condition/N)*(N/2+1))%N;
 // Acquire an actual main-view reference mask for all 900 frames before removing slots.
 if(D.Stage==TEXT("A1") && T5Condition<N) {const int32 Others[]={0,1,3};const int32 Order=T5Condition%4;K=((T5Condition/4+Seed)%3)*4+(Order==0?2:Others[(Order-1+Seed)%3]);}
 D.Run=T5Condition/N+1;T5View=D.Cases[K].Key;T5Filter=D.Cases[K].Value;
 Capture=FPaths::GetPath(T5Base)/FString::Printf(TEXT("seed%d-%s-%s-%s-%s-r%d.png"),Seed,*D.Stage,*D.Profile,*T5View,*T5Filter,D.Run);
 FlushRenderingCommands();D.Started=false;D.Errors=0;D.Last=MakeShared<FJsonObject>();
 for(int32 I=0;I<Members.Num();++I) {auto* M=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;int32 Ignored=0;RegisterTree(M,true,Ignored);M->SetComponentTickEnabled(true);D.WasSleeping[I]=false;D.PreviousInside[I]=false;D.LastSeen[I]=-100000;
  if(D.Stage==TEXT("A1")) {M->bComponentUseFixedSkelBounds=true;M->UpdateBounds();M->MarkRenderTransformDirty();}
 }
 if(D.Stage==TEXT("B") || D.Stage==TEXT("C") || D.Stage==TEXT("D")) SelectT5LOD();
 if(D.Stage==TEXT("D")) SelectT5Material();
 if(D.Markers) D.Markers->ClearInstances();D.MarkerPositions.Reset();
 for(auto* M:D.Extra) {int32 Ignored=0;RegisterTree(M,false,Ignored);}
}

void UBattleSquadTrial::BeginT5Diagnostic(ABattleGameMode& Mode,const FBattleReplayUnit* U,const FBattleReplayCamera& C) {
 auto& D=*T5Diagnostic;D.Last=MakeShared<FJsonObject>();D.Registrations=0;D.Wakes=0;D.Requested=500;
 if(T5Filter.StartsWith(TEXT("Total"))) D.Requested=FCString::Atoi(*T5Filter.Mid(5));
 const auto* Ref=D.ReferenceScreen.Find(T5View);int32 RequiredCount=0;uint32 RequiredHash=2166136261u;
 if(D.Stage==TEXT("A1") && D.Requested==250 && T5View!=TEXT("Recorded") && (!Ref || Ref->Num()!=900)) ++D.Errors;
 int32 Potential=0,Changes=0;uint32 Hash=2166136261u;
 for(int32 I=0;I<500;++I) {
  const bool Alive=U[I].Alive && U[I].Visible && !(T5View==TEXT("FPS") && I==T5Anchor);
  D.Inside[I]=Alive && InView(C,FVector(U[I].X,U[I].Y,U[I].Z));
  Potential+=D.Inside[I];Changes+=D.Started && D.Inside[I]!=D.PreviousInside[I];D.PreviousInside[I]=D.Inside[I];
  if(D.Inside[I]) {Hash=(Hash^uint32(I))*16777619u;Hash=FCrc::MemCrc32(&U[I],24,Hash);}
  D.Required[I]=Ref && Ref->Num()==900 && (((*Ref)[FMath::Clamp(ReplayCursor,0,899)].Bits[I/32]>>(I%32))&1u);
  if(D.Required[I]) {++RequiredCount;RequiredHash=(RequiredHash^uint32(I))*16777619u;RequiredHash=FCrc::MemCrc32(&U[I],24,RequiredHash);}
  D.Keep[I]=D.Stage==TEXT("A1")?D.Requested>=500:true;
 }
 // TPS/FPS: retain every conservative-frustum soldier; fill remaining slots by stable seed permutation.
 // Recorded: deliberate total-population comparison, so the screen population also changes.
 if(D.Stage==TEXT("A1") && D.Requested==250) {
  int32 Count=0;for(int32 I=0;I<500;++I) {D.Keep[I]=T5View!=TEXT("Recorded") && D.Required[I];Count+=D.Keep[I];}
  for(int32 J=0;J<500 && Count<250;++J) {const int32 I=(J*137+Seed*31)%500;if(!D.Keep[I]) {D.Keep[I]=true;++Count;}}
 }
 D.Last->SetNumberField(TEXT("potential_frustum"),Potential);D.Last->SetNumberField(TEXT("potential_hash"),Hash);
 D.Last->SetNumberField(TEXT("frustum_boundary_changes"),Changes);D.Last->SetNumberField(TEXT("requested_slots"),D.Requested);
 D.Last->SetNumberField(TEXT("required_original_count"),RequiredCount);D.Last->SetNumberField(TEXT("required_original_hash"),RequiredHash);
 D.Last->SetNumberField(TEXT("population_floor_excess"),D.Stage==TEXT("A1") && D.Requested==250 && T5View!=TEXT("Recorded")?FMath::Max(0,RequiredCount-250):0);
 D.Last->SetNumberField(TEXT("updated_bodies"),0);D.Last->SetNumberField(TEXT("missing_potential"),0);D.Last->SetNumberField(TEXT("visibility_changes"),0);D.Started=true;
}

bool UBattleSquadTrial::FilterT5Diagnostic(int32 I,const FBattleReplayUnit* U,const FBattleReplayCamera& C,bool& Show) {
 auto& D=*T5Diagnostic;auto* M=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;
 if(D.Stage==TEXT("A1")) {RegisterTree(M,D.Keep[I],D.Registrations);Show&=D.Keep[I];}
 if(D.Stage==TEXT("D") && T5Filter.EndsWith(TEXT("None"))) Show=false;
 if(T5Filter.StartsWith(TEXT("Frustum"))) Show&=D.Inside[I];
 if(T5Filter.StartsWith(TEXT("Distance"))) Show&=D.Inside[I] && FVector::DistSquared(FVector(U[I].X,U[I].Y,U[I].Z),FVector(C.X,C.Y,C.Z))<=10000.*10000.;
 if(Show!=ReplayVisible[I]) D.Last->SetNumberField(TEXT("visibility_changes"),D.Last->GetNumberField(TEXT("visibility_changes"))+1);
 if(T5Filter==TEXT("KnowledgeProxy") && U[I].Team!=U[T5Anchor].Team) {
  Show&=D.Inside[I] && FVector::DistSquared(FVector(U[I].X,U[I].Y,U[I].Z),FVector(C.X,C.Y,C.Z))<=10000.*10000.;
  if(Show) {D.LastSeen[I]=ReplayCursor;D.LastKnown[I]=FVector(U[I].X,U[I].Y,U[I].Z);}
 }
 const bool Sleep=((T5Filter.EndsWith(TEXT("Sleep")) || T5Filter==TEXT("KnowledgeProxy")) && !Show) || (D.Stage==TEXT("A1") && !D.Keep[I]);
 if(Sleep && !D.WasSleeping[I]) {D.SleepFrame[I]=ReplayCursor;if(auto* A=M->GetSingleNodeInstance()) D.SleepPoseTime[I]=A->GetCurrentTime();}
 if(D.WasSleeping[I] && !Sleep) {
  ++D.Wakes;
  // Resume at the recorded clock, without gameplay notifies; do not expose a stale frozen pose.
  if(auto* A=M->GetSingleNodeInstance()) if(A->GetLength()>0) A->SetPosition(FMath::Fmod(D.SleepPoseTime[I]+(ReplayCursor-D.SleepFrame[I])/30.f,A->GetLength()),false);
 }
 D.WasSleeping[I]=Sleep;M->SetComponentTickEnabled(!Sleep);
 if(!Sleep) D.Last->SetNumberField(TEXT("updated_bodies"),D.Last->GetNumberField(TEXT("updated_bodies"))+1);
 if(D.Inside[I] && !Show) D.Last->SetNumberField(TEXT("missing_potential"),D.Last->GetNumberField(TEXT("missing_potential"))+1);
 return !Sleep;
}

void UBattleSquadTrial::EndT5Diagnostic(ABattleGameMode& Mode,const FBattleReplayUnit* U,const FBattleReplayCamera& C) {
 auto& D=*T5Diagnostic;
 if(D.Stage==TEXT("A1") && D.Requested==1000 && D.Extra.IsEmpty()) {
  FlushRenderingCommands();auto* Host=Mode.GetWorld()->SpawnActor<AActor>();
  for(int32 I=0;I<500;++I) {
   auto* Source=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;
   auto* M=NewObject<USkeletalMeshComponent>(Host);Host->AddInstanceComponent(M);M->SetSkeletalMesh(Source->GetSkeletalMeshAsset());M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   M->SetLeaderPoseComponent(Source);M->bUseBoundsFromLeaderPoseComponent=false;M->bComponentUseFixedSkelBounds=true;M->SetCastShadow(Source->CastShadow);
   for(int32 J=0;J<Source->GetNumMaterials();++J) M->SetMaterial(J,Source->GetMaterial(J));D.Extra.Add(M);
   TArray<USceneComponent*> Children;Source->GetChildrenComponents(true,Children);UStaticMeshComponent* Weapon=nullptr;
   for(auto* Child:Children) if(auto* S=Cast<UStaticMeshComponent>(Child)) {Weapon=S;break;}
   UStaticMeshComponent* W=nullptr;
   if(Weapon) {W=NewObject<UStaticMeshComponent>(Host);Host->AddInstanceComponent(W);W->SetStaticMesh(Weapon->GetStaticMesh());W->SetCollisionEnabled(ECollisionEnabled::NoCollision);W->SetupAttachment(M,Weapon->GetAttachSocketName());W->SetRelativeTransform(Weapon->GetRelativeTransform());for(int32 J=0;J<Weapon->GetNumMaterials();++J) W->SetMaterial(J,Weapon->GetMaterial(J));}
   D.ExtraWeapons.Add(W);T5Visibility->Groups.Add(M->GetPrimitiveSceneId(),I%10==0?0:1);
  }
 }
 int32 ExtraVisible=0,AddedInside=0;
 if(D.Stage==TEXT("A1") && D.Requested==1000) for(int32 I=0;I<500;++I) {
  auto* M=D.Extra[I];RegisterTree(M,true,D.Registrations);const FRotator R=FVector(U[I].FX,U[I].FY,U[I].FZ).Rotation();
  FVector P(U[I].X,U[I].Y,U[I].Z);
  if(T5View==TEXT("Recorded")) P+=FVector(75,75,0);
  else P=FVector(C.X,C.Y,C.Z)+FRotator(C.Pitch,C.Yaw,C.Roll).RotateVector(FVector(-20000.,(I%25-12)*200.,(I/25-10)*200.));
  M->SetWorldLocationAndRotation(P-FVector(0,0,90),R+FRotator(0,-90,0));M->SetForcedLOD(int32(U[I].LOD)+1);M->SetVisibility(U[I].Alive && U[I].Visible,true);
  ExtraVisible+=U[I].Alive && U[I].Visible;AddedInside+=U[I].Alive && U[I].Visible && InView(C,P);
 }
 int32 Registered=0,Shown=0,TransformErrors=0,Ticking=0,MissingRequired=0;uint32 KeptHash=2166136261u,KeptRequiredHash=2166136261u;
 for(int32 I=0;I<500;++I) {auto* M=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;Registered+=M->IsRegistered();Shown+=ReplayVisible[I];Ticking+=M->IsComponentTickEnabled();
  if((D.Inside[I] || D.Required[I]) && ReplayVisible[I]) {
   const FVector P=Members[I].Actor?Members[I].Actor->GetActorLocation():M->GetComponentLocation()+FVector(0,0,90);
   TransformErrors+=!P.Equals(FVector(U[I].X,U[I].Y,U[I].Z),.1);
   if(D.Inside[I]) {KeptHash=(KeptHash^uint32(I))*16777619u;KeptHash=FCrc::MemCrc32(&U[I],24,KeptHash);}
   if(D.Required[I]) {KeptRequiredHash=(KeptRequiredHash^uint32(I))*16777619u;KeptRequiredHash=FCrc::MemCrc32(&U[I],24,KeptRequiredHash);}
  }
  MissingRequired+=D.Required[I] && !ReplayVisible[I];
 }
 for(auto* M:D.Extra) Registered+=M->IsRegistered();
 int32 SeenEnemies=0,AgeMax=0;TArray<FVector> Points;
 if(T5Filter==TEXT("KnowledgeProxy")) {
  if(!D.Markers) {
   auto* Host=Mode.GetWorld()->SpawnActor<AActor>();D.Markers=NewObject<UInstancedStaticMeshComponent>(Host);Host->AddInstanceComponent(D.Markers);
   D.Markers->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));D.Markers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   D.Markers->SetCastShadow(false);D.Markers->SetVisibleInRayTracing(false);D.Markers->RegisterComponent();
   if(auto* Mat=D.Markers->CreateDynamicMaterialInstance(0)) Mat->SetVectorParameterValue(TEXT("Color"),FLinearColor(1,.25f,.02f));
  }
  for(int32 I=0;I<500;++I) if(U[I].Team!=U[T5Anchor].Team) {
   const int32 Age=ReplayCursor-D.LastSeen[I];if(Age==0) ++SeenEnemies;
   else if(Age>0 && Age<=300) {Points.Add(D.LastKnown[I]);AgeMax=FMath::Max(AgeMax,Age);}
  }
  if(Points!=D.MarkerPositions) {D.Markers->ClearInstances();TArray<FTransform> X;for(FVector P:Points) X.Emplace(FRotator::ZeroRotator,P+FVector(0,0,35),FVector(.35,.35,.12));if(X.Num()) D.Markers->AddInstances(X,false,true,false);D.MarkerPositions=Points;}
 }
 D.Last->SetNumberField(TEXT("proxy_seen_enemies"),SeenEnemies);D.Last->SetNumberField(TEXT("marker_count"),Points.Num());D.Last->SetNumberField(TEXT("marker_max_age_frames"),AgeMax);
 D.Last->SetNumberField(TEXT("registered_bodies"),Registered);D.Last->SetNumberField(TEXT("extra_visible"),ExtraVisible);D.Last->SetNumberField(TEXT("added_inside"),AddedInside);
 D.Last->SetNumberField(TEXT("kept_hash"),KeptHash);D.Last->SetNumberField(TEXT("kept_required_hash"),KeptRequiredHash);D.Last->SetNumberField(TEXT("missing_required"),MissingRequired);D.Last->SetNumberField(TEXT("transform_errors"),TransformErrors);D.Last->SetNumberField(TEXT("tick_enabled_bodies"),Ticking);D.Errors+=TransformErrors;
 D.Last->SetNumberField(TEXT("registrations"),D.Registrations);D.Last->SetNumberField(TEXT("wakeups"),D.Wakes);D.PreviousShown=Shown;
 if(D.Stage==TEXT("A1") && T5View!=TEXT("Recorded") && D.Requested>0) D.Errors+=MissingRequired!=0 || AddedInside!=0;
}
