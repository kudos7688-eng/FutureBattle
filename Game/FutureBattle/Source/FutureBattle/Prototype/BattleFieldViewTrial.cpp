#include "BattleSquadTrial.h"
#include "BattleRenderDiagnostic.h"
#include "BattleViewVisibility.h"
#include "RenderingThread.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "StaticMeshResources.h"
#include "SkeletalRenderPublic.h"
#include "GameFramework/HUD.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UnrealClient.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Animation/AnimSingleNodeInstance.h"

namespace {
const uint8* At(const TArray<uint8>& B,int32 F) { return B.GetData()+32+int64(F)*(28+500*32); }
bool Save(const TSharedPtr<FJsonObject>& O,const FString& P) {
 FString S; FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); return FFileHelper::SaveStringToFile(S,*P);
}

}

void UBattleSquadTrial::SelectT5() {
 if(T5Diagnostic) {SelectT5Diagnostic();return;}
 // 15 matched conditions, two counterbalanced repetitions. No multiple views rendered at once.
 const int32 K=(T5Condition%15+Seed*7+(T5Condition/15)*8)%15;
 const TCHAR* Filters[]={TEXT("All"),TEXT("Actors"),TEXT("Mass"),TEXT("NoWeapons"),TEXT("None")};
 const TCHAR* Views[]={TEXT("RTS300"),TEXT("RTS150"),TEXT("RTS60"),TEXT("TPS"),TEXT("FPS")};
 T5View=K<5?TEXT("Recorded"):Views[(K-5)/2]; T5Filter=K<5?Filters[K]:(K%2?TEXT("All"):TEXT("None"));
 Capture=FPaths::GetPath(T5Base)/FString::Printf(TEXT("seed%d-%s-%s-r%d.png"),Seed,*T5View,*T5Filter,T5Condition/15+1);
}

bool UBattleSquadTrial::StartT5(ABattleGameMode& Mode) {
 UE_LOG(LogTemp,Display,TEXT("T5_START file=%s seed=%d"),*ReplayFile,Seed);
 if(!bT5) {
  bT5=true; T5Base=Capture;
  if(!FFileHelper::LoadFileToArray(ReplayBytes,*ReplayFile) || ReplayBytes.Num()<32) return false;
  FMemory::Memcpy(&ReplayHeader,ReplayBytes.GetData(),32);
  UE_LOG(LogTemp,Display,TEXT("T5_HEADER magic=%u version=%u count=%d hz=%d seed=%d mode=%d bytes=%d frames=%d"),ReplayHeader.Magic,ReplayHeader.Version,ReplayHeader.Count,ReplayHeader.Hz,ReplayHeader.Seed,ReplayHeader.Mode,ReplayBytes.Num(),ReplayHeader.Frames);
  if(ReplayHeader.Magic!=0x31524246 || ReplayHeader.Version!=1 || ReplayHeader.Count!=500 || ReplayHeader.Hz!=30 || ReplayHeader.Seed!=Seed || ReplayHeader.Mode!=0) return false;
  if(ReplayBytes.Num()!=32+int64(ReplayHeader.Frames)*(28+500*32) || ReplayHeader.FirstShot<0 || ReplayHeader.Frames<=ReplayHeader.FirstShot+900) return false;
  ReplayStart=ReplayHeader.FirstShot;
  // Team 0 Actor slot, independent of future survival. Follow last recorded transform after death.
  T5Anchor=(Seed%25)*10;
  FParse::Value(FCommandLine::Get(),TEXT("T5-Conditions="),T5Limit);
  if(T5Limit<1 || T5Limit>256) return false;
  if(!ConfigureT5Diagnostic()) return false;
  InspectT5Assets();
  T5Visibility=FSceneViewExtensions::NewExtension<FT5Visibility>();
  T5Visibility->InspectShadows=T5Diagnostic && T5Diagnostic->Stage==TEXT("Shadow");
  for(auto& M:Members) {auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display;T5Visibility->Groups.Add(Mesh->GetPrimitiveSceneId(),M.Actor?0:1);
   if(T5Diagnostic) {const int32 Slot=int32(&M-Members.GetData());T5Visibility->OriginalSlots.Add(Mesh->GetPrimitiveSceneId(),Slot);
    if((T5Diagnostic->Stage==TEXT("A1") || T5Diagnostic->Stage==TEXT("Shadow"))) {TArray<USceneComponent*> Children;Mesh->GetChildrenComponents(true,Children);for(auto* Child:Children) if(auto* S=Cast<UStaticMeshComponent>(Child)) T5Visibility->WeaponSlots.Add(S->GetPrimitiveSceneId(),Slot);}
   }
  }

 }
 SelectT5(); {FScopeLock Lock(&T5Visibility->Mutex);T5Visibility->Counts.Reset();} ReplayCursor=-120; Frames.Reset(); T5Samples.Reset(); ReplayCameraErrors=0;
 ReplayVisible.Init(true,500); T5LastRendered.Init(-1000,500);
 for(double& D:DiagCombat) D=0;
 auto* World=Mode.GetWorld(); auto* PC=World->GetFirstPlayerController();
 if(!ReplayCamera) ReplayCamera=World->SpawnActor<ACameraActor>();
 PC->bAutoManageActiveCameraTarget=false; PC->SetViewTarget(ReplayCamera); if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
 for(auto& M:Members) if(M.Actor) { M.Actor->SetActorTickEnabled(false); if(auto* C=M.Actor->GetController()) C->SetActorTickEnabled(false); }
 FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./30.);
 return true;
}

void UBattleSquadTrial::InspectT5Assets() {
 TSet<FString> Done;
 for(auto& M:Members) {
  auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display; auto* A=Mesh->GetSkeletalMeshAsset();
  const FString Group=M.Actor?TEXT("Actor"):TEXT("Mass"); const FString Key=Group+A->GetPathName();
  if(!Done.Contains(Key)) {
   Done.Add(Key); auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("group"),Group); O->SetStringField(TEXT("asset"),A->GetPathName()); O->SetStringField(TEXT("type"),TEXT("skeletal"));
   O->SetBoolField(TEXT("leader_pose"),Mesh->LeaderPoseComponent.IsValid()); TArray<TSharedPtr<FJsonValue>> Lods;
   auto* D=A->GetResourceForRendering();
   for(int32 I=0;I<D->LODRenderData.Num();++I) { const auto& L=D->LODRenderData[I]; auto R=MakeShared<FJsonObject>();
    int32 Tri=0; for(auto& S:L.RenderSections) Tri+=S.NumTriangles;
    R->SetNumberField(TEXT("lod"),I); R->SetNumberField(TEXT("vertices"),L.GetNumVertices()); R->SetNumberField(TEXT("triangles"),Tri); R->SetNumberField(TEXT("sections"),L.RenderSections.Num()); Lods.Add(MakeShared<FJsonValueObject>(R)); }
   O->SetArrayField(TEXT("lods"),Lods); T5Assets.Add(MakeShared<FJsonValueObject>(O));
  }
  TArray<USceneComponent*> Children; Mesh->GetChildrenComponents(true,Children);
  for(auto* C:Children) if(auto* S=Cast<UStaticMeshComponent>(C)) if(UStaticMesh* AStatic=S->GetStaticMesh()) {
   const FString K=Group+AStatic->GetPathName(); if(Done.Contains(K)) continue; Done.Add(K);
   auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("group"),Group); O->SetStringField(TEXT("asset"),AStatic->GetPathName()); O->SetStringField(TEXT("type"),TEXT("static"));
   TArray<TSharedPtr<FJsonValue>> Lods; const auto* D=AStatic->GetRenderData();
   for(int32 I=0;I<D->LODResources.Num();++I) {const auto& L=D->LODResources[I];auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("lod"),I);R->SetNumberField(TEXT("vertices"),L.GetNumVertices());R->SetNumberField(TEXT("triangles"),L.GetNumTriangles());R->SetNumberField(TEXT("sections"),L.Sections.Num());Lods.Add(MakeShared<FJsonValueObject>(R));}
   O->SetArrayField(TEXT("lods"),Lods);T5Assets.Add(MakeShared<FJsonValueObject>(O));
  }
 }
}

void UBattleSquadTrial::StepT5(ABattleGameMode& Mode) {
 const double Now=FPlatformTime::Seconds(); auto* PC=Mode.GetWorld()->GetFirstPlayerController();
 if(ReplayCursor>0 && ReplayCursor<=900) {
  auto O=MakeShared<FJsonObject>(); O->SetNumberField(TEXT("frame"),ReplayCursor-1);O->SetNumberField(TEXT("view_time_key"),T5PreviousTimeKey);
  if(T5Diagnostic) for(auto& KV:T5Diagnostic->Last->Values) O->SetField(KV.Key,KV.Value);
  const double Wall=(Now-LastFrame)*1000.; Frames.Add(Wall); O->SetNumberField(TEXT("frame_ms"),Wall);
  O->SetNumberField(TEXT("game_thread"),FPlatformTime::ToMilliseconds(GGameThreadTime));
  O->SetNumberField(TEXT("render_thread"),FPlatformTime::ToMilliseconds(GRenderThreadTime));
  O->SetNumberField(TEXT("rhi_thread"),FPlatformTime::ToMilliseconds(GRHIThreadTime));
  O->SetNumberField(TEXT("gpu"),FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
  O->SetNumberField(TEXT("draw_calls"),GNumDrawCallsRHI[0]); O->SetNumberField(TEXT("draw_primitives"),GNumPrimitivesDrawnRHI[0]);
  O->SetNumberField(TEXT("display_sync"),DiagCombat[SyncTime]);
  int32 PredictedLODMismatches=0;
  int32 Shown[2]={},Frustum[2]={},Drawn[2]={}; int32 Lods[2][8]={}, RenderLods[2][8]={};
  for(int32 I=0;I<500;++I) { auto& M=Members[I];auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display; const int32 G=M.Actor?0:1;
   const float T=Mesh->GetLastRenderTimeOnScreen(); const bool Fresh=T>T5LastRendered[I];T5LastRendered[I]=T;
   if(!ReplayVisible[I]) continue; ++Shown[G]; Drawn[G]+=Fresh;
   if(T5Diagnostic && (T5Diagnostic->Stage==TEXT("B") || T5Diagnostic->Stage==TEXT("C") || T5Diagnostic->Stage==TEXT("D"))) PredictedLODMismatches+=Mesh->GetPredictedLODLevel()!=T5Diagnostic->RequestedLOD[I];
   ++Lods[G][FMath::Clamp(Mesh->GetPredictedLODLevel(),0,7)];
   if(Mesh->GetMeshObject()) ++RenderLods[G][FMath::Clamp(Mesh->GetMeshObject()->GetLOD(),0,7)];
  }
  if(T5Diagnostic) for(int32 I=0;I<T5Diagnostic->Extra.Num();++I) {auto* M=T5Diagnostic->Extra[I];if(!M->IsRegistered() || !M->IsVisible()) continue;const int32 G=I%10==0?0:1;++Shown[G];++Lods[G][FMath::Clamp(M->GetPredictedLODLevel(),0,7)];if(M->GetMeshObject()) ++RenderLods[G][FMath::Clamp(M->GetMeshObject()->GetLOD(),0,7)];}
  for(int32 G=0;G<2;++G) {const FString P=G==0?TEXT("actor_"):TEXT("mass_"); O->SetNumberField(P+TEXT("shown"),Shown[G]);O->SetNumberField(P+TEXT("frustum"),Frustum[G]);O->SetNumberField(P+TEXT("drawn"),Drawn[G]); for(int32 L=0;L<8;++L) {O->SetNumberField(P+FString::Printf(TEXT("lod%d"),L),Lods[G][L]);O->SetNumberField(P+FString::Printf(TEXT("render_lod%d"),L),RenderLods[G][L]);}}
  if(T5Diagnostic && (T5Diagnostic->Stage==TEXT("B") || T5Diagnostic->Stage==TEXT("C") || T5Diagnostic->Stage==TEXT("D"))) O->SetNumberField(TEXT("predicted_lod_mismatches"),PredictedLODMismatches);
  const auto* Prev=reinterpret_cast<const FBattleReplayUnit*>(At(ReplayBytes,ReplayStart+ReplayCursor-1)+28);
  O->SetBoolField(TEXT("anchor_alive"),Prev[T5Anchor].Alive!=0);T5Samples.Add(MakeShared<FJsonValueObject>(O));
  auto* C=PC->PlayerCameraManager.Get();
  ReplayCameraErrors+=PC->GetViewTarget()!=ReplayCamera || !C || !C->GetCameraLocation().Equals(FVector(T5PreviousCamera.X,T5PreviousCamera.Y,T5PreviousCamera.Z),.1) || !C->GetCameraRotation().Equals(FRotator(T5PreviousCamera.Pitch,T5PreviousCamera.Yaw,T5PreviousCamera.Roll),.01) || FMath::Abs(C->GetFOVAngle()-T5PreviousCamera.FOV)>.01;
 }
 LastFrame=Now;
 if(ReplayCursor==900) {
  if(!FinishT5()) {++ReplayCursor;FPlatformMisc::RequestExitWithStatus(false,1);return;}
  if(++T5Condition<T5Limit) {if(!StartT5(Mode)) FPlatformMisc::RequestExitWithStatus(false,1);}
  else {++ReplayCursor;FPlatformMisc::RequestExitWithStatus(false,0);} return;
 }
 if(ReplayCursor>900) return;
 const uint8* Data=At(ReplayBytes,ReplayStart+FMath::Max(0,ReplayCursor)); FBattleReplayCamera C; FMemory::Memcpy(&C,Data,28);
 const auto* U=reinterpret_cast<const FBattleReplayUnit*>(Data+28);
 if(T5View!=TEXT("Recorded")) {
  const auto& A=U[T5Anchor]; const FVector P(A.X,A.Y,A.Z), F=FVector(A.FX,A.FY,0).GetSafeNormal(); FVector Eye;FRotator R;
  if(T5View.StartsWith(TEXT("RTS"))) {const double H=T5View==TEXT("RTS300")?30000.:T5View==TEXT("RTS150")?15000.:6000.; Eye=P+FVector(0,0,H);R=FRotator(-90,0,0);}
  else if(T5View==TEXT("TPS")) {Eye=P-F*350.+FVector(0,0,150);R=(P+F*100.+FVector(0,0,50)-Eye).Rotation();}
  else {Eye=P+F*15.+FVector(0,0,60);R=F.Rotation();}
  C={float(Eye.X),float(Eye.Y),float(Eye.Z),float(R.Pitch),float(R.Yaw),float(R.Roll),90.f};
 }
 if(T5Diagnostic && T5Diagnostic->Stage==TEXT("C") && T5Diagnostic->LODStress) ApplyT5LODCamera(U,C);
 // Stress-only visual QA: long shadows from behind the camera. Never used for performance data.
 if(T5Diagnostic && T5Diagnostic->Stage==TEXT("Shadow") && FParse::Param(FCommandLine::Get(),TEXT("T5-ShadowQA")) && FParse::Param(FCommandLine::Get(),TEXT("T5-VisualQA"))) {
  for(TActorIterator<ADirectionalLight> It(Mode.GetWorld());It;++It) It->SetActorRotation(FRotator(-8,C.Yaw,0));
 }
 ReplayCamera->SetActorLocationAndRotation(FVector(C.X,C.Y,C.Z),FRotator(C.Pitch,C.Yaw,C.Roll));ReplayCamera->GetCameraComponent()->SetFieldOfView(C.FOV);PC->SetViewTarget(ReplayCamera);T5PreviousCamera=C;
 const double Begin=FPlatformTime::Seconds();
 if(T5Diagnostic) BeginT5Diagnostic(Mode,U,C);
 for(int32 I=0;I<500;++I) {
  auto& M=Members[I]; auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display;
  bool Show=U[I].Alive && U[I].Visible && T5Filter!=TEXT("None") && !(T5Filter==TEXT("Actors")&&!M.Actor) && !(T5Filter==TEXT("Mass")&&M.Actor) && !(T5View==TEXT("FPS")&&I==T5Anchor);
  const bool Update=!T5Diagnostic || FilterT5Diagnostic(I,U,C,Show);
  if(T5Diagnostic && T5Diagnostic->Stage==TEXT("Shadow")) {
   // Hidden/dead/FPS-owner bodies must not reappear as shadows.
   const bool Eligible=U[I].Alive && U[I].Visible && !(T5View==TEXT("FPS") && I==T5Anchor);
   const bool KeepShadow=T5Filter==TEXT("FrustumShadow") && Eligible && !Show;
   if(M.Actor) M.Actor->SetActorHiddenInGame(false);
   // QA uses one fixed pose so image differences are not dominated by phase drift between replays.
   if(FParse::Param(FCommandLine::Get(),TEXT("T5-VisualQA"))) {
    auto* PoseMesh=Cast<USkeletalMeshComponent>(Mesh->LeaderPoseComponent.Get());if(!PoseMesh) PoseMesh=Mesh;
    if(auto* Anim=PoseMesh->GetSingleNodeInstance()) {Anim->SetPlayRate(0);Anim->SetPosition(.4f,false);}
   }
   Mesh->SetVisibility(true,true);
   Mesh->SetHiddenInGame(!Show,true);
   Mesh->SetCastHiddenShadow(KeepShadow);
   TArray<USceneComponent*> ShadowChildren;Mesh->GetChildrenComponents(true,ShadowChildren);
   for(auto* Child:ShadowChildren) if(auto* P=Cast<UPrimitiveComponent>(Child)) P->SetCastHiddenShadow(KeepShadow);
   ReplayVisible[I]=Show;
   if(KeepShadow && (!Mesh->bHiddenInGame || !Mesh->bCastHiddenShadow || !Mesh->IsComponentTickEnabled())) ++T5Diagnostic->Errors;
   const TCHAR* Key=TEXT("hidden_shadow_bodies");
   double Count=0;T5Diagnostic->Last->TryGetNumberField(Key,Count);T5Diagnostic->Last->SetNumberField(Key,Count+int32(KeepShadow));
  }
  else if(ReplayCursor==-120 || ReplayVisible[I]!=Show) {if(M.Actor) M.Actor->SetActorHiddenInGame(!Show);else Mesh->SetVisibility(Show,true);ReplayVisible[I]=Show;}
  if(ReplayCursor==-120 && !(T5Diagnostic && T5Diagnostic->Stage==TEXT("Shadow"))) {
   TArray<USceneComponent*> Children;Mesh->GetChildrenComponents(true,Children);for(auto* Child:Children) if(auto* S=Cast<UStaticMeshComponent>(Child)) S->SetVisibility(Show&&T5Filter!=TEXT("NoWeapons"));
  }
  if(!Update) continue;
  if(T5Diagnostic && T5Diagnostic->Stage==TEXT("D")) ApplyT5Material(I,U,C);
  const int32 DisplayLOD=T5Diagnostic && (T5Diagnostic->Stage==TEXT("B") || T5Diagnostic->Stage==TEXT("C") || T5Diagnostic->Stage==TEXT("D"))?ApplyT5LOD(I,U,C):int32(U[I].LOD);
  const bool MeasureLOD=T5Diagnostic && T5Diagnostic->Stage==TEXT("C");
  const bool LODChanged=MeasureLOD && Mesh->GetForcedLOD()!=DisplayLOD+1;
  const double LODSetStart=MeasureLOD?FPlatformTime::Seconds():0.;Mesh->SetForcedLOD(DisplayLOD+1);
  if(MeasureLOD) {
   const double Elapsed=(FPlatformTime::Seconds()-LODSetStart)*1000.;
   auto Add=[&](const TCHAR* Key,double Value){double Previous=0;T5Diagnostic->Last->TryGetNumberField(Key,Previous);T5Diagnostic->Last->SetNumberField(Key,Previous+Value);};
   Add(TEXT("lod_set_ms"),Elapsed);Add(TEXT("lod_set_changed_ms"),LODChanged?Elapsed:0.);Add(TEXT("lod_set_changed_calls"),LODChanged);
  }
  const FVector P(U[I].X,U[I].Y,U[I].Z);const FRotator R=FVector(U[I].FX,U[I].FY,U[I].FZ).Rotation();
  if(M.Actor) M.Actor->SetActorLocationAndRotation(P,R);else Mesh->SetWorldLocationAndRotation(P-FVector(0,0,90),R+FRotator(0,-90,0));
 }
 if(T5Diagnostic && (T5Diagnostic->Stage==TEXT("B") || T5Diagnostic->Stage==TEXT("C") || T5Diagnostic->Stage==TEXT("D"))) FinishT5LODFrame();
 if(T5Diagnostic) EndT5Diagnostic(Mode,U,C);
 DiagCombat[SyncTime]=(FPlatformTime::Seconds()-Begin)*1000.;
 T5PreviousTimeKey=FMath::RoundToInt64(Mode.GetWorld()->GetTimeSeconds()*30000.);
 if(ReplayCursor==-30) FScreenshotRequest::RequestScreenshot(Capture,false,false);
 int32 QAFrame=450;FParse::Value(FCommandLine::Get(),TEXT("T5-CaptureFrame="),QAFrame);
 if(FParse::Param(FCommandLine::Get(),TEXT("T5-VisualQA")) && (ReplayCursor>=QAFrame-1 && ReplayCursor<=QAFrame+1)) FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Capture)/FPaths::GetBaseFilename(Capture)+FString::Printf(TEXT("-frame%d.png"),ReplayCursor),false,false);
 ++ReplayCursor;
}

bool UBattleSquadTrial::FinishT5() {
 FlushRenderingCommands(); int32 VisibilityMissing=0,VisibilityErrors=0;
 {
  FScopeLock Lock(&T5Visibility->Mutex);
  for(auto& V:T5Samples) {auto R=V->AsObject(); const auto* C=T5Visibility->Counts.Find(int64(R->GetNumberField(TEXT("view_time_key"))));
   if(!C) {++VisibilityMissing;continue;}
   R->SetNumberField(TEXT("visibility_observer_ms"),C->ObserverMS);
   if(T5Diagnostic && T5Diagnostic->Stage==TEXT("A1") && T5Filter==TEXT("Total500")) {
    auto& Ref=T5Diagnostic->ReferenceScreen.FindOrAdd(T5View);if(Ref.Num()!=900) Ref.SetNum(900);
    auto& Set=Ref[int32(R->GetNumberField(TEXT("frame")))];for(int32 J=0;J<16;++J) Set.Bits[J]=C->OriginalFrustum[J]|C->WeaponFrustum[J];
   }
   if(T5Diagnostic) {R->SetStringField(TEXT("original_frustum_bits"),BytesToHex(reinterpret_cast<const uint8*>(C->OriginalFrustum),sizeof(C->OriginalFrustum)));R->SetStringField(TEXT("original_drawn_bits"),BytesToHex(reinterpret_cast<const uint8*>(C->OriginalDrawn),sizeof(C->OriginalDrawn)));}
   if(T5Diagnostic && T5Diagnostic->Stage==TEXT("Shadow")) {
    R->SetStringField(TEXT("shadow_body_bits"),BytesToHex(reinterpret_cast<const uint8*>(C->ShadowBodies),sizeof(C->ShadowBodies)));
    R->SetStringField(TEXT("shadow_weapon_bits"),BytesToHex(reinterpret_cast<const uint8*>(C->ShadowWeapons),sizeof(C->ShadowWeapons)));
   }
   for(int32 G=0;G<2;++G) {const FString P=G==0?TEXT("actor_"):TEXT("mass_");
    R->SetNumberField(P+TEXT("render_updates"),R->GetNumberField(P+TEXT("drawn")));
    R->SetNumberField(P+TEXT("frustum"),C->Frustum[G]);R->SetNumberField(P+TEXT("drawn"),C->Drawn[G]);R->SetNumberField(P+TEXT("body_triangles"),C->BodyTriangles[G]);
    VisibilityErrors+=C->Drawn[G]>C->Frustum[G];
   }
  }
 }

 auto O=MakeShared<FJsonObject>();
 if(T5Diagnostic && (T5Diagnostic->Stage==TEXT("C") || T5Diagnostic->Stage==TEXT("D"))) {O->SetStringField(TEXT("lod_policy"),T5Diagnostic->LODPolicy);O->SetBoolField(TEXT("lod_stress"),T5Diagnostic->LODStress);O->SetNumberField(TEXT("lod_probe_index"),T5Diagnostic->LODProbe);TArray<TSharedPtr<FJsonValue>> Actors;for(int32 I=0;I<500;++I) if(Members[I].Actor) Actors.Add(MakeShared<FJsonValueNumber>(I));O->SetArrayField(TEXT("actor_indices"),Actors);}
 if(T5Diagnostic && T5Diagnostic->Stage==TEXT("D")) {O->SetArrayField(TEXT("material_statistics"),T5Diagnostic->MaterialStatistics);O->SetObjectField(TEXT("condition_settings"),T5Diagnostic->ConditionSettings);}
 O->SetNumberField(TEXT("seed"),Seed);O->SetStringField(TEXT("view"),T5View);O->SetStringField(TEXT("filter"),T5Filter);O->SetNumberField(TEXT("run"),T5Diagnostic?T5Diagnostic->Run:T5Condition/15+1);O->SetNumberField(TEXT("anchor_index"),T5Anchor);O->SetNumberField(TEXT("anchor_unit_id"),Members[T5Anchor].ID);O->SetStringField(TEXT("record"),ReplayFile);O->SetNumberField(TEXT("start_frame"),ReplayStart);O->SetNumberField(TEXT("frames"),Frames.Num());
 O->SetBoolField(TEXT("visual_qa_only"),FParse::Param(FCommandLine::Get(),TEXT("T5-VisualQA")));O->SetNumberField(TEXT("camera_errors"),ReplayCameraErrors);O->SetNumberField(TEXT("shots"),Shots);O->SetNumberField(TEXT("rays"),Rays);O->SetNumberField(TEXT("paths"),Paths);O->SetArrayField(TEXT("samples"),T5Samples);O->SetArrayField(TEXT("assets"),T5Assets);
 O->SetNumberField(TEXT("visibility_missing"),VisibilityMissing);O->SetNumberField(TEXT("visibility_errors"),VisibilityErrors);
 if(T5Diagnostic) {O->SetStringField(TEXT("stage"),T5Diagnostic->Stage);O->SetStringField(TEXT("profile"),T5Diagnostic->Profile);O->SetObjectField(TEXT("settings"),T5Diagnostic->Settings);O->SetNumberField(TEXT("diagnostic_errors"),T5Diagnostic->Errors);}
 const bool OK=(!T5Diagnostic || T5Diagnostic->Errors==0) && Frames.Num()==900 && VisibilityMissing==0 && VisibilityErrors==0 && ReplayCameraErrors==0 && Shots==0 && Rays==0 && Paths==0 && Save(O,FPaths::ChangeExtension(Capture,TEXT("json")));
 UE_LOG(LogTemp,Display,TEXT("T5_REPLAY seed=%d view=%s filter=%s run=%d frames=%d cameraErrors=%d visibilityMissing=%d visibilityErrors=%d success=%d"),Seed,*T5View,*T5Filter,T5Diagnostic?T5Diagnostic->Run:T5Condition/15+1,Frames.Num(),ReplayCameraErrors,VisibilityMissing,VisibilityErrors,OK);return OK;
}
