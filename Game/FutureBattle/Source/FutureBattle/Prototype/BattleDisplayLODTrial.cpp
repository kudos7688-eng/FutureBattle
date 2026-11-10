#include "BattleSquadTrial.h"
#include "BattleRenderDiagnostic.h"
#include "BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"

namespace {
void LODDistances(const FString& Policy,double FOV,double& Near,double& Far) {
 Near=2000.;Far=5000.;
 if(Policy==TEXT("Dist10_30")) {Near=1000.;Far=3000.;}
 if(Policy==TEXT("Screen120_48")) {const double Scale=180.*960./FMath::Tan(FMath::DegreesToRadians(FOV*.5));Near=Scale/120.;Far=Scale/48.;}
}
}

// Opt-in replay-only LOD experiment. Canonical unit state and gameplay never use these choices.
void UBattleSquadTrial::SelectT5LOD() {
 auto& D=*T5Diagnostic;
 for(int32 I=0;I<500;++I) {
  D.PreviousLOD[I]=-1;D.RequestedLOD[I]=-1;
  auto* Mesh=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;
  const bool Original=D.Stage==TEXT("B") && T5Filter==TEXT("All");
  Mesh->bIgnoreLeaderPoseComponentLOD=!Original;
  if(auto* Leader=Mesh->LeaderPoseComponent.Get()) Leader->SetForcedLOD(Original?0:1);
 }
}

int32 UBattleSquadTrial::ApplyT5LOD(int32 I,const FBattleReplayUnit* U,const FBattleReplayCamera& C) {
 auto& D=*T5Diagnostic;
 if(FParse::Param(FCommandLine::Get(),TEXT("T5-VisualQA"))) {
  auto* Mesh=Members[I].Actor?Members[I].Actor->GetMesh():Members[I].Display;
  auto* PoseMesh=Cast<USkeletalMeshComponent>(Mesh->LeaderPoseComponent.Get());if(!PoseMesh) PoseMesh=Mesh;
  if(auto* Anim=PoseMesh->GetSingleNodeInstance()) if(Anim->GetLength()>0) {Anim->SetPlayRate(0);Anim->SetPosition(FMath::Fmod(.4f+FMath::Max(0,ReplayCursor)/30.f,Anim->GetLength()),false);}
 }
 const FVector Eye(C.X,C.Y,C.Z),P(U[I].X,U[I].Y,U[I].Z);
 const double Distance=FMath::Max(1.,FVector::Distance(Eye,P));
 // Projected height estimate for a 180 cm soldier, horizontal FOV, native 1920 px width.
 const double HeightPixels=180.*960./(Distance*FMath::Tan(FMath::DegreesToRadians(C.FOV*.5)));
 int32 LOD=FMath::Clamp(int32(U[I].LOD),0,2);
 if(T5Filter==TEXT("LOD0")) LOD=0;
 else if(T5Filter==TEXT("LOD1")) LOD=1;
 else if(T5Filter==TEXT("LOD2")) LOD=2;
 else if(T5Filter==TEXT("Dist10_30")) LOD=Distance<1000.?0:Distance<3000.?1:2;
 else if(T5Filter==TEXT("Dist20_50")) LOD=Distance<2000.?0:Distance<5000.?1:2;
 else if(T5Filter==TEXT("Screen120_48")) LOD=HeightPixels>=120.?0:HeightPixels>=48.?1:2;
 if(D.Stage==TEXT("C") || D.Stage==TEXT("D")) {
  double Near,Far;LODDistances(D.LODPolicy,C.FOV,Near,Far);
  LOD=Distance<Near?0:Distance<Far?1:2;
  const double H=D.Stage==TEXT("D")?D.SelectedHysteresis:T5Filter.StartsWith(TEXT("H"))?FCString::Atof(*T5Filter.Mid(1))/100.:0.;
  if(D.PreviousLOD[I]>=0 && (D.Stage==TEXT("D") || T5Filter!=TEXT("Reference"))) {
   LOD=D.PreviousLOD[I];
   while(LOD<2 && Distance>=(LOD==0?Near:Far)*(1.+H)) ++LOD;
   while(LOD>0 && Distance<(LOD==1?Near:Far)*(1.-H)) --LOD;
  }
  if(Members[I].Actor && (D.Stage==TEXT("D") || T5Filter!=TEXT("Reference"))) LOD=0;
  if(I==D.LODProbe) {D.Last->SetNumberField(TEXT("probe_lod"),LOD);D.Last->SetNumberField(TEXT("probe_distance_cm"),Distance);}
 }
 const bool Changed=D.PreviousLOD[I]>=0 && D.PreviousLOD[I]!=LOD;
 auto Add=[&](const TCHAR* Key,double N){double V=0;D.Last->TryGetNumberField(Key,V);D.Last->SetNumberField(Key,V+N);};
 Add(TEXT("lod_changes"),Changed);Add(TEXT("lod_changes_shown"),Changed && ReplayVisible[I]);
 Add(*FString::Printf(TEXT("requested_lod%d"),LOD),ReplayVisible[I]);
 D.PreviousLOD[I]=LOD;D.RequestedLOD[I]=LOD;
 return LOD;
}

void UBattleSquadTrial::ApplyT5LODCamera(const FBattleReplayUnit* U,FBattleReplayCamera& C) {
 auto& D=*T5Diagnostic;check(D.LODProbe>=0);
 double Near,Far;LODDistances(D.LODPolicy,C.FOV,Near,Far);
 // Separate synthetic stress case: 1 Hz, +/-5/15/25%, five seconds each at both boundaries.
 const int32 Segment=FMath::Clamp(ReplayCursor,0,899)/150;
 const double Amplitudes[]={.05,.15,.25};const double Amplitude=Amplitudes[Segment%3];
 const double Boundary=Segment<3?Near:Far;
 const double Distance=Boundary*(1.+Amplitude*FMath::Sin(2.*PI*ReplayCursor/30.));
 const auto& Probe=U[D.LODProbe];
 C.X=Probe.X-float(Distance);C.Y=Probe.Y;C.Z=Probe.Z;
 C.Pitch=0;C.Yaw=0;C.Roll=0;
}

void UBattleSquadTrial::FinishT5LODFrame() {
 auto& D=*T5Diagnostic;FString Codes;Codes.Reserve(500);
 for(int32 I=0;I<500;++I) Codes.AppendChar(TCHAR('0'+D.RequestedLOD[I]));
 D.Last->SetStringField(TEXT("lod_codes"),Codes);
 for(const TCHAR* K:{TEXT("requested_lod0"),TEXT("requested_lod1"),TEXT("requested_lod2")}) if(!D.Last->HasField(K)) D.Last->SetNumberField(K,0);
}
