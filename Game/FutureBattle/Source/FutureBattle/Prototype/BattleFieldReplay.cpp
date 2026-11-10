#include "BattleSquadTrial.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/HUD.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UnrealClient.h"

namespace {
bool LoadReplay(const FString& Path,TArray<uint8>& Bytes,FBattleReplayHeader& H)
{
 if(!FFileHelper::LoadFileToArray(Bytes,*Path) || Bytes.Num()<sizeof(H)) return false;
 FMemory::Memcpy(&H,Bytes.GetData(),sizeof(H));
 return H.Magic==0x31524246 && H.Version==1 && H.Count==500 && H.Hz==30 && H.FirstShot>=0 && H.Frames>H.FirstShot+901 &&
  int64(Bytes.Num())==sizeof(H)+int64(H.Frames)*(sizeof(FBattleReplayCamera)+H.Count*sizeof(FBattleReplayUnit));
}
const uint8* ReplayFrame(const TArray<uint8>& Bytes,int32 Frame)
{ return Bytes.GetData()+sizeof(FBattleReplayHeader)+int64(Frame)*(sizeof(FBattleReplayCamera)+500*sizeof(FBattleReplayUnit)); }
uint32 Priority(int32 ID,int32 Seed)
{ uint32 X=uint32(ID)^uint32(Seed*104729); X^=X>>16; X*=0x7feb352d; X^=X>>15; X*=0x846ca68b; return X^(X>>16); }
}
void UBattleSquadTrial::CaptureField(ABattleGameMode& Mode)
{
 if(ReplayBytes.IsEmpty()) { ReplayHeader.Seed=Seed; ReplayHeader.Mode=Name==TEXT("Squad"); ReplayBytes.AddZeroed(sizeof(ReplayHeader)); }
 if(Shots>0 && ReplayHeader.FirstShot<0) ReplayHeader.FirstShot=Frame;
 auto* PC=Mode.GetWorld()->GetFirstPlayerController(); auto* Cam=Cast<ACameraActor>(PC->GetViewTarget());
 const FVector P=Cam->GetActorLocation(); const FRotator R=Cam->GetActorRotation();
 FBattleReplayCamera C{float(P.X),float(P.Y),float(P.Z),float(R.Pitch),float(R.Yaw),float(R.Roll),Cam->GetCameraComponent()->FieldOfView};
 ReplayBytes.Append(reinterpret_cast<uint8*>(&C),sizeof(C));
 for(int32 I=0;I<Members.Num();++I) {
  const auto& M=Members[I]; const auto& S=Soldiers[I]; auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display;
  FBattleReplayUnit U{}; U.X=S.P.X;U.Y=S.P.Y;U.Z=S.P.Z;U.FX=S.Forward.X;U.FY=S.Forward.Y;U.FZ=S.Forward.Z;
  U.Alive=S.bAlive; U.Actor=M.Actor!=nullptr; U.Team=I/(Members.Num()/2);
  U.Visible=M.Actor ? !M.Actor->IsHidden() && Mesh->IsVisible() : Mesh->IsVisible(); U.LOD=FMath::Clamp(Mesh->GetPredictedLODLevel(),0,255);
  ReplayBytes.Append(reinterpret_cast<uint8*>(&U),sizeof(U));
 }
 ++ReplayHeader.Frames;
}
bool UBattleSquadTrial::SaveFieldRecording()
{
 FMemory::Memcpy(ReplayBytes.GetData(),&ReplayHeader,sizeof(ReplayHeader));
 const bool OK=FFileHelper::SaveArrayToFile(ReplayBytes,*RecordFile);
 UE_LOG(LogTemp,Display,TEXT("FIELD_RECORD frames=%d firstShot=%d bytes=%d success=%d"),ReplayHeader.Frames,ReplayHeader.FirstShot,ReplayBytes.Num(),OK);
 return OK;
}
bool UBattleSquadTrial::StartReplay(ABattleGameMode& Mode)
{
 if(FParse::Param(FCommandLine::Get(),TEXT("T5-Replay"))) return StartT5(Mode);
 if(ReplayArm.IsEmpty()) {
 FParse::Value(FCommandLine::Get(),TEXT("BattleReplayReference="),ReplayReference);
 FParse::Value(FCommandLine::Get(),TEXT("BattleReplayArm="),ReplayArm);
 FParse::Value(FCommandLine::Get(),TEXT("BattleReplayFeature="),ReplayFeature);
 bReplayBatch=FParse::Param(FCommandLine::Get(),TEXT("BattleReplayBatch"));
 ReplaySourceB=ReplayFile; ReplayCaptureBase=Capture;
 if(bReplayBatch) SelectReplayBatch();
 }
 if(!LoadReplay(ReplayFile,ReplayBytes,ReplayHeader) || ReplayHeader.Seed!=Seed) return false;
 if(ReplayArm!=TEXT("A") && ReplayArm!=TEXT("B") && ReplayArm!=TEXT("C") && ReplayArm!=TEXT("D")) return false;
 if(ReplayFeature!=TEXT("AllOn") && ReplayFeature!=TEXT("NoFar") && ReplayFeature!=TEXT("NoShadow") && ReplayFeature!=TEXT("SlowSync") && ReplayFeature!=TEXT("AllOff")) return false;
 FBattleReplayHeader RefHeader;
 if(!LoadReplay(ReplayReference,ReferenceBytes,RefHeader) || RefHeader.Seed!=Seed) return false;
 ReplayStart=RefHeader.FirstShot; // Identical absolute simulation times, not separate first-shot offsets.
 if(ReplayHeader.Frames<=ReplayStart+901) return false;
 ReplayVisible.Init(true,Members.Num()); ReplayPriority.Reset(); ReplayCameraErrors=0;
 for(int32 I=0;I<Members.Num();++I) ReplayPriority.Add(I);
 ReplayPriority.Sort([&](int32 A,int32 B){return Priority(A,Seed)<Priority(B,Seed);});
 ReplayMasks.SetNum(900*Members.Num()); ReplayAdded=ReplayRemoved=0;
 for(int32 Sample=0;Sample<900;++Sample) {
  const int32 Index=ReplayStart+Sample;
  const auto* Units=reinterpret_cast<const FBattleReplayUnit*>(ReplayFrame(ReplayBytes,Index)+sizeof(FBattleReplayCamera));
  const auto* Ref=reinterpret_cast<const FBattleReplayUnit*>(ReplayFrame(ReferenceBytes,Index)+sizeof(FBattleReplayCamera));
 TArray<bool> Show; Show.SetNum(Members.Num());
 for(int32 I=0;I<Members.Num();++I) Show[I]=Units[I].Alive && Units[I].Visible;
 if(ReplayArm==TEXT("C")) {
  // Match A's counts separately by team and representation. Random fixed rank, never future survival.
  for(int32 Group=0;Group<4;++Group) {
   int32 Want=0,Have=0;
   for(int32 I=0;I<Members.Num();++I) if(Units[I].Team*2+Units[I].Actor==Group) { Want+=Ref[I].Alive && Ref[I].Visible;Have+=Show[I]; }
   for(int32 I:ReplayPriority) if(Units[I].Team*2+Units[I].Actor==Group && Have>Want && Show[I]) {Show[I]=false;--Have;++ReplayRemoved;}
   // If B has fewer alive, restore randomly ranked B placeholders at their last recorded B location.
   for(int32 I:ReplayPriority) if(Units[I].Team*2+Units[I].Actor==Group && Have<Want && !Show[I]) {Show[I]=true;++Have;++ReplayAdded;}
   check(Have==Want);
  }
 }

  for(int32 I=0;I<Members.Num();++I) ReplayMasks[Sample*Members.Num()+I]=Show[I];
 }
 if(ReplayOriginalShadows.IsEmpty()) for(auto& M:Members) {
  auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display;
  TArray<USceneComponent*> Children;Mesh->GetChildrenComponents(true,Children);Children.Add(Mesh);
  for(auto* Child:Children) if(auto* P=Cast<UPrimitiveComponent>(Child)) ReplayOriginalShadows.Add(P,P->CastShadow);
 }
 auto* World=Mode.GetWorld(); auto* PC=World->GetFirstPlayerController();
 if(ReplayCamera) ReplayCamera->Destroy();
 ReplayCamera=World->SpawnActor<ACameraActor>(); PC->bAutoManageActiveCameraTarget=false; PC->SetViewTarget(ReplayCamera); if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
 for(auto& M:Members) if(M.Actor) { M.Actor->SetActorTickEnabled(false); if(auto* Controller=M.Actor->GetController()) Controller->SetActorTickEnabled(false); }
 // Frame-based animation, shared by every replay arm, independent of achieved FPS.
 FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./30.);
 return true;
}
void UBattleSquadTrial::StepReplay(ABattleGameMode& Mode)
{
 if(bT5) { StepT5(Mode); return; }
 const double Now=FPlatformTime::Seconds();
 if(ReplayCursor>0 && ReplayCursor<=900) {
  Frames.Add((Now-LastFrame)*1000.);
  DiagCombat[GameTime]+=FPlatformTime::ToMilliseconds(GGameThreadTime);
  DiagCombat[RenderTime]+=FPlatformTime::ToMilliseconds(GRenderThreadTime);
  DiagCombat[RHITime]+=FPlatformTime::ToMilliseconds(GRHIThreadTime);
  DiagCombat[GPUTime]+=FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
 }
 LastFrame=Now;
 if(ReplayCursor==900) {
  if(!FinishReplay()) { ++ReplayCursor; FPlatformMisc::RequestExitWithStatus(false,1); return; }
  if(bReplayBatch && ++ReplayBatchIndex<32) {
   SelectReplayBatch(); ReplayCursor=-120; Frames.Reset(); ReplayAdded=ReplayRemoved=0; ReplayAliveSum=0;
   for(double& D:DiagCombat) D=0;
   if(!StartReplay(Mode)) FPlatformMisc::RequestExitWithStatus(false,1);
  } else { ++ReplayCursor; FPlatformMisc::RequestExitWithStatus(false,0); }
  return;
 }
 if(ReplayCursor>900) return;
 const int32 Index=ReplayStart+FMath::Max(0,ReplayCursor);
 const uint8* Data=ReplayFrame(ReplayBytes,Index);
 FBattleReplayCamera Cam; FMemory::Memcpy(&Cam,Data,sizeof(Cam));
 auto* PC=Mode.GetWorld()->GetFirstPlayerController();
 // BeginPlay possession can replace a view target set by GameMode::BeginPlay.
 // Keep the recorded view authoritative, and check the camera actually used on the preceding frame.
 if(ReplayCursor>=0) {
  auto* ManagerCamera=PC->PlayerCameraManager.Get();
  ReplayCameraErrors+=PC->GetViewTarget()!=ReplayCamera || !ManagerCamera ||
   !ManagerCamera->GetCameraLocation().Equals(FVector(Cam.X,Cam.Y,Cam.Z),.1) ||
   !ManagerCamera->GetCameraRotation().Equals(FRotator(Cam.Pitch,Cam.Yaw,Cam.Roll),.01) ||
   FMath::Abs(ManagerCamera->GetFOVAngle()-Cam.FOV)>.01;
 }
 ReplayCamera->SetActorLocationAndRotation(FVector(Cam.X,Cam.Y,Cam.Z),FRotator(Cam.Pitch,Cam.Yaw,Cam.Roll)); ReplayCamera->GetCameraComponent()->SetFieldOfView(Cam.FOV);
 PC->SetViewTarget(ReplayCamera);
 const auto* Units=reinterpret_cast<const FBattleReplayUnit*>(Data+sizeof(Cam));
 const uint8* Show=ReplayMasks.GetData()+FMath::Max(0,ReplayCursor)*Members.Num();
 const bool FarOff=ReplayFeature==TEXT("NoFar") || ReplayFeature==TEXT("AllOff");
 const bool ShadowOff=ReplayFeature==TEXT("NoShadow") || ReplayFeature==TEXT("AllOff");
 const bool Slow=ReplayFeature==TEXT("SlowSync") || ReplayFeature==TEXT("AllOff");
 const double SyncStart=FPlatformTime::Seconds();
 int32 Alive=0;
 for(int32 I=0;I<Members.Num();++I) {
  auto& M=Members[I]; const auto& U=Units[I]; Alive+=Show[I];
  check(bool(M.Actor)==bool(U.Actor));
  const FVector P(U.X,U.Y,U.Z); const FRotator R=FVector(U.FX,U.FY,U.FZ).Rotation();
  auto* Mesh=M.Actor?M.Actor->GetMesh():M.Display;
  const bool Visible=Show[I] && !(FarOff && FVector::DistSquared(P,FVector(Cam.X,Cam.Y,Cam.Z))>FMath::Square(15000.));
  if(ReplayCursor==-120 || ReplayVisible[I]!=Visible) {
   if(M.Actor) M.Actor->SetActorHiddenInGame(!Visible); else Mesh->SetVisibility(Visible,true);
   ReplayVisible[I]=Visible;
  }
  if(ReplayCursor==-120) {
   TArray<USceneComponent*> Children; Mesh->GetChildrenComponents(true,Children); Children.Add(Mesh);
   for(auto* Child:Children) if(auto* Primitive=Cast<UPrimitiveComponent>(Child)) Primitive->SetCastShadow(!ShadowOff && ReplayOriginalShadows.FindChecked(Primitive));
  }
  Mesh->SetForcedLOD(int32(U.LOD)+1);
  if(M.Actor || !Slow || ReplayCursor<0 || ReplayCursor%4==0) {
   if(M.Actor) M.Actor->SetActorLocationAndRotation(P,R);
   else Mesh->SetWorldLocationAndRotation(P-FVector(0,0,90),R+FRotator(0,-90,0));
  }
 }
 if(ReplayCursor>=0) {ReplayAliveSum+=Alive;DiagCombat[SyncTime]+=(FPlatformTime::Seconds()-SyncStart)*1000.;}
 if(ReplayCursor==-30) FScreenshotRequest::RequestScreenshot(Capture,false,false);
 ++ReplayCursor;
}
bool UBattleSquadTrial::FinishReplay()
{
 double Sum=0;for(double T:Frames)Sum+=T;Frames.Sort();
 auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("seed"),Seed);Root->SetStringField(TEXT("arm"),ReplayArm);Root->SetStringField(TEXT("feature"),ReplayFeature);
 Root->SetStringField(TEXT("record"),ReplayFile);Root->SetNumberField(TEXT("start_frame"),ReplayStart);Root->SetNumberField(TEXT("frames"),Frames.Num());
 Root->SetNumberField(TEXT("fps"),900000./Sum);Root->SetNumberField(TEXT("frame_ms"),Sum/900);Root->SetNumberField(TEXT("p95_ms"),Frames[854]);
 const TCHAR* Keys[]={TEXT("game_thread"),TEXT("render_thread"),TEXT("rhi_thread"),TEXT("gpu")};
 for(int32 K=0;K<4;++K) Root->SetNumberField(Keys[K],DiagCombat[GameTime+K]/900.);
 Root->SetNumberField(TEXT("display_sync"),DiagCombat[SyncTime]/900.);Root->SetNumberField(TEXT("mean_alive"),ReplayAliveSum/900.);
 Root->SetNumberField(TEXT("added_unit_frames"),ReplayAdded);Root->SetNumberField(TEXT("removed_unit_frames"),ReplayRemoved);
 Root->SetNumberField(TEXT("shots"),Shots);Root->SetNumberField(TEXT("rays"),Rays);Root->SetNumberField(TEXT("paths"),Paths);
 Root->SetNumberField(TEXT("camera_errors"),ReplayCameraErrors);
 FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
 const bool Saved=FFileHelper::SaveStringToFile(Json,*(FPaths::ChangeExtension(Capture,TEXT("json"))));
 const bool OK=Frames.Num()==900 && Shots==0 && Rays==0 && Paths==0 && ReplayCameraErrors==0 && Saved;
 UE_LOG(LogTemp,Display,TEXT("FIELD_REPLAY arm=%s feature=%s seed=%d fps=%.3f meanAlive=%.3f added=%d removed=%d success=%d"),*ReplayArm,*ReplayFeature,Seed,900000./Sum,ReplayAliveSum/900,ReplayAdded,ReplayRemoved,OK);
 return OK;
}

void UBattleSquadTrial::SelectReplayBatch()
{
 const int32 Run=ReplayBatchIndex/16;
 const int32 K=(ReplayBatchIndex%16+Seed*7+Run*8)%16;
 const TCHAR* Features[]={TEXT("AllOn"),TEXT("NoFar"),TEXT("NoShadow"),TEXT("SlowSync"),TEXT("AllOff")};
 ReplayArm=K==15?TEXT("D"):K/5==0?TEXT("A"):K/5==1?TEXT("B"):TEXT("C");
 ReplayFeature=K==15?TEXT("AllOn"):Features[K%5];
 ReplayFile=(ReplayArm==TEXT("A") || ReplayArm==TEXT("D"))?ReplayReference:ReplaySourceB;
 Capture=FPaths::GetPath(ReplayCaptureBase)/FString::Printf(TEXT("seed%d-%s-%s-r%d.png"),Seed,*ReplayArm,*ReplayFeature,Run+1);
}
