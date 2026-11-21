// Sound and effects check (2026-11-22). Test only: does nothing without -BattleFXCheck. See BattleFXCheck.h.
#include "BattleFXCheck.h"
#include "BattleAudio.h"
#include "BattleController.h"
#include "BattleEffects.h"
#include "BattleGameMode.h"
#include "FutureBattle.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundSubmix.h"
#include "UnrealClient.h"

namespace BattleFXCheck
{
namespace
{
 enum class EEvent : uint8 { Record, Rifle, Heavy, Impacts, ShotImpacts, Blast1, ShotBlast1, Blast25, ShotBlast25, ShotBlast25Late,
  Rocket, Mortar, Cannon, Incoming, BlastMortar, ShotBlastMortar, CaptureUs, CaptureThem, Win, Lose, StopRecording, Finish };
 struct FEvent { float T; EEvent E; };
 // Seconds after the check starts. Setup/Analyze-SoundFX.py reads the same times (minus the 0.5 s before recording starts).
 const FEvent Events[]={
  {.5f,EEvent::Record},
  {1.f,EEvent::Rifle},{1.1f,EEvent::Rifle},{1.2f,EEvent::Rifle},{1.3f,EEvent::Rifle},{1.4f,EEvent::Rifle},{1.5f,EEvent::Rifle},
  {2.2f,EEvent::Heavy},{2.3f,EEvent::Heavy},{2.4f,EEvent::Heavy},{2.5f,EEvent::Heavy},
  {3.2f,EEvent::Impacts},{3.32f,EEvent::ShotImpacts},
  {4.f,EEvent::Blast1},{4.12f,EEvent::ShotBlast1},
  {5.5f,EEvent::Blast25},{5.62f,EEvent::ShotBlast25},{6.4f,EEvent::ShotBlast25Late},
  {7.5f,EEvent::Rocket},{8.5f,EEvent::Mortar},{9.5f,EEvent::Cannon},
  {10.5f,EEvent::Incoming},{12.3f,EEvent::BlastMortar},{12.42f,EEvent::ShotBlastMortar},
  {13.5f,EEvent::CaptureUs},{15.f,EEvent::CaptureThem},{16.5f,EEvent::Win},{18.5f,EEvent::Lose},
  {21.f,EEvent::StopRecording},{21.5f,EEvent::Finish},
 };

 struct FCheck
 {
  bool bStarted=false, bDone=false;
  double Start=0.;
  int32 Step=0, Failures=0, StartedBefore=0;
  FVector Focus=FVector::ZeroVector, Ahead=FVector::ForwardVector, Right=FVector::RightVector;
  FString Prefix;
  TWeakObjectPtr<ACameraActor> Camera;
 };
 FCheck& Get(){static FCheck C;return C;}

 void Check(FCheck& C,bool bOk,const FString& What)
 {
  UE_LOG(LogFutureBattle,Display,TEXT("FX_CHECK %s: %s"),bOk?TEXT("PASS"):TEXT("FAIL"),*What);
  if(!bOk)++C.Failures;
 }

 FVector Ground(UWorld* World,const FVector& P)
 {
  FHitResult Hit;
  if(World->LineTraceSingleByObjectType(Hit,P+FVector(0,0,3000),P-FVector(0,0,3000),FCollisionObjectQueryParams(ECC_WorldStatic)))return Hit.ImpactPoint;
  return P;
 }

 // A point Forward metres ahead of the focus and Side metres to its right, Up metres above the ground.
 FVector Place(UWorld* World,const FCheck& C,float Forward,float Side,float Up=0.f)
 {
  return Ground(World,C.Focus+C.Ahead*(Forward*100.f)+C.Right*(Side*100.f))+FVector(0,0,Up*100.f);
 }

 void Screenshot(const FCheck& C,const TCHAR* Name)
 {
  if(!C.Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(C.Prefix+TEXT("-")+Name+TEXT(".png"),false,false);
 }

 FString RecordingFolder(const FCheck& C){return FPaths::GetPath(C.Prefix);}
 FString RecordingName(const FCheck& C){return FPaths::GetCleanFilename(C.Prefix)+TEXT("-audio");}

 // Returns false while the step must wait (the recording file is written in the background).
 bool Run(UWorld* World,FCheck& C,EEvent E,double Elapsed)
 {
  switch(E)
  {
  case EEvent::Record:
  {
   Check(C,BattleAudio::Enabled(World),TEXT("audio device present (run without -nosound)"));
   Check(C,BattleEffects::Active(World),TEXT("effects active (rendering, explosion asset loaded)"));
   // The game window is not focused, which mutes the game (UnfocusedVolumeMultiplier 0): ignore that, and instead turn
   // the main output down after the point where the mix is recorded, so the speakers stay silent.
   if(IConsoleVariable* AppVolume=IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume")))AppVolume->Set(1,ECVF_SetByCode);
   if(FAudioDevice* Device=World->GetAudioDeviceRaw())Device->GetMainSubmixObject().SetSubmixOutputVolume(World,0.f);
   UAudioMixerBlueprintLibrary::StartRecordingOutput(World,30.f);
   C.StartedBefore=BattleAudio::Started();
   break;
  }
  case EEvent::Rifle:BattleAudio::Shot(World,Place(World,C,12.f,-6.f,1.5f),BattleAudio::EShot::Rifle,false);break;
  case EEvent::Heavy:BattleAudio::Shot(World,Place(World,C,12.f,6.f,1.5f),BattleAudio::EShot::Heavy,false);break;
  case EEvent::Impacts:
  {
   int32 Shown=0;
   for(int32 I=0;I<4;++I)
   {
    const FVector P=Place(World,C,9.f,-2.4f+1.6f*I);
    Shown+=BattleEffects::BulletImpact(World,P,FVector::UpVector)?1:0;
    BattleAudio::Impact(World,P,false);
   }
   Check(C,Shown==4,FString::Printf(TEXT("bullet impact effects started: %d of 4"),Shown));
   break;
  }
  case EEvent::ShotImpacts:Screenshot(C,TEXT("impacts"));break;
  case EEvent::Blast1:
  {
   const FVector P=Place(World,C,15.f,-5.f);
   Check(C,BattleEffects::Explosion(World,P,1.f),TEXT("explosion size 1 (anti-tank round) started"));
   BattleAudio::Explosion(World,P,1.f);
   break;
  }
  case EEvent::ShotBlast1:Screenshot(C,TEXT("blast-at"));break;
  case EEvent::Blast25:
  {
   const FVector P=Place(World,C,22.f,5.f);
   Check(C,BattleEffects::Explosion(World,P,2.5f),TEXT("explosion size 2.5 (self-propelled gun shell) started"));
   BattleAudio::Explosion(World,P,2.5f);
   break;
  }
  case EEvent::ShotBlast25:Screenshot(C,TEXT("blast-spg"));break;
  case EEvent::ShotBlast25Late:Screenshot(C,TEXT("blast-spg-late"));break;
  case EEvent::Rocket:{const FVector P=Place(World,C,8.f,-8.f,1.5f);BattleAudio::Cue(World,BattleAudio::ECue::RocketLaunch,&P);break;}
  case EEvent::Mortar:{const FVector P=Place(World,C,8.f,8.f,1.f);BattleAudio::Cue(World,BattleAudio::ECue::MortarLaunch,&P);break;}
  case EEvent::Cannon:{const FVector P=Place(World,C,18.f,0.f,2.f);BattleAudio::Cue(World,BattleAudio::ECue::CannonFire,&P);break;}
  case EEvent::Incoming:BattleAudio::Incoming(World,Place(World,C,14.f,0.f),BattleAudio::IncomingLead);break;
  case EEvent::BlastMortar:
  {
   const FVector P=Place(World,C,14.f,0.f);
   Check(C,BattleEffects::Explosion(World,P,2.f),TEXT("explosion size 2 (mortar shell) started"));
   BattleAudio::Explosion(World,P,2.f);
   break;
  }
  case EEvent::ShotBlastMortar:Screenshot(C,TEXT("blast-mortar"));break;
  case EEvent::CaptureUs:BattleAudio::Cue(World,BattleAudio::ECue::CaptureFriendly);break;
  case EEvent::CaptureThem:BattleAudio::Cue(World,BattleAudio::ECue::CaptureEnemy);break;
  case EEvent::Win:BattleAudio::Cue(World,BattleAudio::ECue::Victory);break;
  case EEvent::Lose:BattleAudio::Cue(World,BattleAudio::ECue::Defeat);break;
  case EEvent::StopRecording:
  {
   const int32 Started=BattleAudio::Started()-C.StartedBefore;
   // 6 rifle + 4 machine-gun shots, 3 impact sounds (per-frame budget), 3 explosions of 2-3 layers, 4 launch layers,
   // the whistle and 4 cues: at least 25 sounds started.
   Check(C,Started>=25,FString::Printf(TEXT("sounds started during the check: %d (25 or more expected)"),Started));
   UAudioMixerBlueprintLibrary::StopRecordingOutput(World,EAudioRecordingExportType::WavFile,RecordingName(C),RecordingFolder(C));
   break;
  }
  case EEvent::Finish:
  {
   const FString File=FPaths::Combine(RecordingFolder(C),RecordingName(C)+TEXT(".wav"));
   if(!FPaths::FileExists(File)&&Elapsed<30.)return false;
   Check(C,FPaths::FileExists(File),TEXT("recording written: ")+File);
   UE_LOG(LogFutureBattle,Display,TEXT("FX_CHECK_RESULT failures=%d"),C.Failures);
   C.bDone=true;
   FPlatformMisc::RequestExitWithStatus(false,C.Failures?1:0);
   break;
  }
  }
  return true;
 }
}
}

void BattleFXCheck::Tick(ABattleController* PC)
{
 static const bool bOn=FParse::Param(FCommandLine::Get(),TEXT("BattleFXCheck"));
 if(!bOn||!PC||!PC->GetWorld())return;
 UWorld* World=PC->GetWorld();
 FCheck& C=Get();
 if(C.bDone)return;
 const double Now=World->GetTimeSeconds();
 if(!C.bStarted)
 {
  // Start once the battle is set up and the command view looks over our own side.
  if(Now<3.||!PC->IsTactical()||!PC->GetPawn()||!PC->PlayerCameraManager)return;
  C.bStarted=true;C.Start=Now;
  FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),C.Prefix);
  if(ABattleGameMode* Mode=World->GetAuthGameMode<ABattleGameMode>())Mode->bCombatPaused=true;
  C.Focus=Ground(World,PC->GetPawn()->GetActorLocation());
  const float Yaw=PC->PlayerCameraManager->GetCameraRotation().Yaw;
  C.Ahead=FRotator(0.f,Yaw,0.f).Vector();C.Right=FRotator(0.f,Yaw+90.f,0.f).Vector();
  ACameraActor* Camera=World->SpawnActor<ACameraActor>();
  if(Camera)
  {
   auto& Post=Camera->GetCameraComponent()->PostProcessSettings;Post.bOverride_MotionBlurAmount=true;Post.MotionBlurAmount=0.f;
   Camera->GetCameraComponent()->SetFieldOfView(70.f);
   const FVector Eye=C.Focus-C.Ahead*1500.+FVector(0,0,900);
   Camera->SetActorLocationAndRotation(Eye,(C.Focus+C.Ahead*1500.-Eye).Rotation());
   PC->SetViewTarget(Camera);PC->bAutoManageActiveCameraTarget=false;
   C.Camera=Camera;
  }
  if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
  UE_LOG(LogFutureBattle,Display,TEXT("FX_CHECK start focus=%s"),*C.Focus.ToCompactString());
 }
 const double Elapsed=Now-C.Start;
 while(!C.bDone&&C.Step<int32(UE_ARRAY_COUNT(Events))&&Elapsed>=Events[C.Step].T)
 {
  if(!Run(World,C,Events[C.Step].E,Elapsed))break;
  ++C.Step;
 }
}
