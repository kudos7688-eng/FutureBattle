#include "BattleSquadTrial.h"
#include "BattleUnit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

bool UBattleSquadTrial::ConfigureT2b(int32 Count)
{
 FString Switch=TEXT("Off");
 FParse::Value(FCommandLine::Get(),TEXT("T2b-TurnToFace="),Switch);
 if(Switch!=TEXT("Off") && Switch!=TEXT("On")) return false;
 bT2bOn=Switch==TEXT("On");
 bT2bMetrics=bT2bOn || FParse::Param(FCommandLine::Get(),TEXT("T2b-Metrics"));
 bT2bHeadless=FParse::Param(FCommandLine::Get(),TEXT("T2b-Headless"));
 bT2bMetrics |= bT2bHeadless;
 if(!bT2bMetrics) return true;
 T2bRate=GetDefault<ABattleUnit>()->GetCharacterMovement()->RotationRate.Yaw;
 FParse::Value(FCommandLine::Get(),TEXT("T2b-TurnRate="),T2bRate);
 bFieldDiagnostics=true;
 return bField && bMoving && Count==500 && ReplayFile.IsEmpty() && T2aAllocation==TEXT("Off") &&
  T2bRate>0. && FMath::IsFinite(T2bRate);
}

bool UBattleSquadTrial::T2bActive(int32 I) const
{
 return bT2bOn && T2bUnits.IsValidIndex(I) && T2bUnits[I].Start>=0;
}

void UBattleSquadTrial::AdvanceT2b(int32 I)
{
 if(!T2bActive(I)) return;
 auto& T=T2bUnits[I]; auto& S=Soldiers[I];
 if(T.Aligned>=0) return; // The requested turn has finished; do not add ongoing target tracking.
 // Only the last position supplied by a currently observing squad member is used.
 const double Desired=(T.Observed-S.P).Rotation().Yaw;
 const double Yaw=S.Forward.Rotation().Yaw;
 const double Delta=FMath::FindDeltaAngleDegrees(Yaw,Desired);
 const double Step=FMath::Clamp(Delta,-T2bRate/30.,T2bRate/30.);
 S.Forward=FRotator(0,Yaw+Step,0).Vector(); T.Turned+=FMath::Abs(Step);
 const double Actual=FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,S.Forward.Rotation().Yaw));
 T2bMaxStepAngle=FMath::Max(T2bMaxStepAngle,Actual);
 if(Actual>T2bRate/30.+1.e-5) ++T2bAngularErrors;
 if(T.Aligned<0 && FMath::Abs(Delta-Step)<=1.) T.Aligned=Frame;
}

void UBattleSquadTrial::BeginT2b(int32 I,int32 Target,FVector Observed)
{
 auto& T=T2bUnits[I];
 T.Target=Target; T.Start=Frame; T.Aligned=-1; T.Confirmed=-1; T.Observed=Observed; T.Turned=0.;
 T.InitialAngle=FMath::Abs(FMath::FindDeltaAngleDegrees(Soldiers[I].Forward.Rotation().Yaw,(Observed-Soldiers[I].P).Rotation().Yaw));
 ++T2bStarted; ++T2bStartThisFrame;
}

void UBattleSquadTrial::EndT2b(int32 I,const TCHAR* Reason)
{
 auto& T=T2bUnits[I]; if(T.Start<0) return;
 auto E=MakeShared<FJsonObject>(); E->SetNumberField(TEXT("shooter"),I); E->SetNumberField(TEXT("target"),T.Target);
 E->SetNumberField(TEXT("start_frame"),T.Start); E->SetNumberField(TEXT("aligned_frame"),T.Aligned);
 E->SetNumberField(TEXT("confirmed_frame"),T.Confirmed); E->SetNumberField(TEXT("end_frame"),Frame);
 E->SetNumberField(TEXT("elapsed_s"),double(Frame-T.Start)/30.);
 E->SetNumberField(TEXT("rotation_s"),T.Aligned<0 ? -1. : double(T.Aligned-T.Start)/30.);
 E->SetNumberField(TEXT("initial_angle_deg"),T.InitialAngle); E->SetNumberField(TEXT("rotated_deg"),T.Turned);
 E->SetStringField(TEXT("outcome"),Reason); T2bTurns.Add(MakeShared<FJsonValueObject>(E));
 T.Target=T.Start=T.Aligned=T.Confirmed=-1;
}

void UBattleSquadTrial::T2bCandidate(int32 I,int32 Next)
{
 if(Next!=Soldiers[I].Candidate) {
  if(T2bActive(I)) EndT2b(I,Next<0 ? TEXT("candidate_lost") : TEXT("candidate_changed"));
  T2bUnits[I].CandidateFired=false; T2bUnits[I].ValidatedTurn=false;
 }
}

bool UBattleSquadTrial::T2bCheck(int32 I,int32 Next,bool Visible,int32 Reject,bool Ready)
{
 if(!Visible && Reject==3 && Ready) ++T2bReadyFov;
 if(!bT2bOn || Name!=TEXT("Squad")) return false;
 auto& T=T2bUnits[I];
 if(T2bActive(I)) {
  // Next was just confirmed by FindEnemy in this squad's existing scan; no extra acquisition.
  T.Observed=Soldiers[Next].P;
  if(T.Aligned<0) {
   if(Ready) { ++T2bReadyWaiting; if(Visible) ++T2bLostClear; }
   if(!Visible && (Reject==2 || Reject==4)) EndT2b(I,Reject==2 ? TEXT("range_rejected") : TEXT("terrain_rejected"));
   return true;
  }
  if(Visible) {
   if(T.Confirmed<0) T.Confirmed=Frame;
   T.ValidatedTurn=true;
   return false;
  }
  EndT2b(I,Reject==2 ? TEXT("range_rejected") : Reject==4 ? TEXT("terrain_rejected") : TEXT("fov_rejected"));
 }
 if(!Visible && Reject==3) {
  BeginT2b(I,Next,Soldiers[Next].P);
  if(Ready) ++T2bReadyWaiting;
  return true;
 }
 return false;
}

void UBattleSquadTrial::T2bShot(int32 I,int32 Next)
{
 auto& T=T2bUnits[I];
 if(T2bActive(I) && T.Aligned<0) ++T2bFireErrors;
 if(T.ValidatedTurn) ++T2bShotsAfterTurn;
 if(T2bActive(I)) EndT2b(I,TEXT("fired"));
 if(!T.CandidateFired) {
  auto E=MakeShared<FJsonObject>(); E->SetNumberField(TEXT("shooter"),I); E->SetNumberField(TEXT("target"),Next);
  E->SetNumberField(TEXT("acquired_frame"),Soldiers[I].Since); E->SetNumberField(TEXT("fired_frame"),Frame);
  E->SetNumberField(TEXT("delay_s"),double(Frame-Soldiers[I].Since)/30.);
  E->SetBoolField(TEXT("shared"),Name==TEXT("Squad") && !Members[I].Actor);
  T2bFirstShots.Add(MakeShared<FJsonValueObject>(E)); T.CandidateFired=true;
 }
}

void UBattleSquadTrial::T2bBeforeScan()
{
 T2bShotsBefore=Shots; T2bAliveBefore=0; T2bStartThisFrame=0;
 for(const auto& S:Soldiers) T2bAliveBefore+=S.bAlive;
}

void UBattleSquadTrial::T2bAfterScan()
{
 int32 Alive=0, Active=0;
 for(int32 I=0;I<Soldiers.Num();++I) {
  Alive+=Soldiers[I].bAlive;
  if(!Soldiers[I].bAlive && T2bActive(I)) EndT2b(I,TEXT("shooter_dead"));
  if(T2bActive(I) && T2bUnits[I].Aligned<0) ++Active;
 }
 T2bMaxStarts=FMath::Max(T2bMaxStarts,T2bStartThisFrame); T2bMaxActive=FMath::Max(T2bMaxActive,Active);
 if(T2bFirstFrame<0 && Shots>0) T2bFirstFrame=Frame;
 if(T2bFirstFrame<0) return;
 const int32 Offset=Frame-T2bFirstFrame;
 if(Offset>0 && Offset<=900) {
  ++T2bWindowFrames; T2bAliveSum+=Alive; T2bWindowShots+=Shots-T2bShotsBefore;
  T2bShooterSeconds+=double(T2bAliveBefore)/30.;
 }
 if(Offset%150==0) {
  auto E=MakeShared<FJsonObject>(); E->SetNumberField(TEXT("seconds"),double(Offset)/30.);
  E->SetNumberField(TEXT("alive"),Alive); E->SetNumberField(TEXT("shots_cumulative"),Shots);
  T2bCurve.Add(MakeShared<FJsonValueObject>(E));
 }
}

void UBattleSquadTrial::FinishT2b()
{
 for(int32 I=0;I<T2bUnits.Num();++I) if(T2bActive(I)) EndT2b(I,TEXT("match_end"));
 auto R=MakeShared<FJsonObject>();
 R->SetStringField(TEXT("mode"),Name); R->SetBoolField(TEXT("turn_on"),bT2bOn); R->SetNumberField(TEXT("turn_rate_deg_s"),T2bRate);
 R->SetNumberField(TEXT("seed"),Seed); R->SetBoolField(TEXT("headless"),bT2bHeadless);
 R->SetNumberField(TEXT("first_shot_frame"),T2bFirstFrame); R->SetNumberField(TEXT("combat_window_frames"),T2bWindowFrames);
 R->SetNumberField(TEXT("combat_window_shots"),T2bWindowShots); R->SetNumberField(TEXT("alive_shooter_seconds"),T2bShooterSeconds);
 R->SetNumberField(TEXT("shots_per_alive_shooter_second"),T2bWindowShots/FMath::Max(1.,T2bShooterSeconds));
 R->SetNumberField(TEXT("combat_mean_alive"),T2bAliveSum/FMath::Max(1,T2bWindowFrames));
 R->SetNumberField(TEXT("turn_started"),T2bStarted); R->SetNumberField(TEXT("shots_after_turn"),T2bShotsAfterTurn);
 R->SetNumberField(TEXT("ready_fov_rejections"),T2bReadyFov); R->SetNumberField(TEXT("ready_scans_waiting_for_turn"),T2bReadyWaiting);
 R->SetNumberField(TEXT("lost_clear_ready_opportunities"),T2bLostClear);
 R->SetNumberField(TEXT("max_turn_starts_per_frame"),T2bMaxStarts); R->SetNumberField(TEXT("max_simultaneously_turning"),T2bMaxActive);
 R->SetNumberField(TEXT("max_step_angle_deg"),T2bMaxStepAngle); R->SetNumberField(TEXT("angular_speed_violations"),T2bAngularErrors);
 R->SetNumberField(TEXT("shots_during_turn"),T2bFireErrors);
 R->SetArrayField(TEXT("turn_events"),T2bTurns); R->SetArrayField(TEXT("survival_curve"),T2bCurve); R->SetArrayField(TEXT("candidate_first_shots"),T2bFirstShots);
 FString Json; auto Writer=TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(R,Writer);
 const FString File=FPaths::GetPath(Capture)/FPaths::GetBaseFilename(Capture)+TEXT("-t2b.json");
 if(!FFileHelper::SaveStringToFile(Json,*File)) FPlatformMisc::RequestExitWithStatus(false,1);
}
