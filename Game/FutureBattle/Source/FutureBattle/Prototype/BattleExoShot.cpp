// Exoskeleton look check (2026-11-18): -BattleExoShot -BattleArea -BattleWideScreens=<prefix> on a wide mixed map.
// Combat is paused; one exoskeleton soldier per team is deployed at its home and photographed from 3.2 m: front, side and back with the
// shield up, then with the shield gone, crouched and prone, and the other team's soldier. Setup/Test-ExoShot.ps1 runs it.
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleStance.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/HUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunExoShot()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunExoShot,Delay,false);};
 auto Finish=[&](){UE_LOG(LogFutureBattle,Display,TEXT("EXO_SHOT_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);};
 Mode->bCombatPaused=true;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const double Now=GetWorld()->GetTimeSeconds();
 if(ExoShotStage==0)
 {
  for(int32 Team=0;Team<2;++Team)
  {
   FString Error;int32 SID=INDEX_NONE;
   if(!SpawnPurchasedSquad(Team,EBattleTraining::Regular,Error,false,&SID,BattleSquadType::Exo)||!Store->FindSquad(SID))
   {UE_LOG(LogFutureBattle,Error,TEXT("EXO_SHOT deploy failed team=%d: %s"),Team,*Error);++SmokeFailures;Finish();return;}
   ExoShotIDs[Team]=Store->FindSquad(SID)->LeaderUnitID;
   if(auto* A=Actor(ExoShotIDs[Team])){A->SetActorHiddenInGame(false);A->bRevealed=true;}
   UE_LOG(LogFutureBattle,Display,TEXT("EXO_SHOT deployed team=%d unit=%d look=%d at=%s"),Team,ExoShotIDs[Team],ExoLooks.Contains(ExoShotIDs[Team]),*Position(ExoShotIDs[Team]).ToCompactString());
   if(!ExoLooks.Contains(ExoShotIDs[Team]))++SmokeFailures;
  }
  ++ExoShotStage;Later(1.5f);return;
 }
 struct FShot{int32 Team;float Yaw;uint8 Stance;bool bShieldDown;const TCHAR* Name;bool bLauncher=false;bool bMove=false;};
 static const FShot Shots[]={
  {0,25.f,BattleStance::Stand,false,TEXT("front")},{0,100.f,BattleStance::Stand,false,TEXT("side")},{0,180.f,BattleStance::Stand,false,TEXT("back")},
  {0,25.f,BattleStance::Stand,false,TEXT("launcher_front"),true},{0,150.f,BattleStance::Stand,false,TEXT("launcher_back"),true},
  {0,25.f,BattleStance::Stand,true,TEXT("noshield")},{0,100.f,BattleStance::Stand,true,TEXT("move_side"),false,true},{0,25.f,BattleStance::Stand,true,TEXT("move_strafe"),false,true},{0,60.f,BattleStance::Crouch,true,TEXT("crouch")},{0,60.f,BattleStance::Prone,true,TEXT("prone")},
  {0,120.f,BattleStance::Prone,true,TEXT("prone_side")},{0,200.f,BattleStance::Prone,true,TEXT("prone_back")},  // after the get-down clip
  {1,25.f,BattleStance::Stand,false,TEXT("enemy")}};
 const int32 Index=ExoShotStage-1;
 if(Index>=UE_ARRAY_COUNT(Shots)){Finish();return;}
 const FShot& S=Shots[Index];const int32 ID=ExoShotIDs[S.Team];ABattleUnit* A=Actor(ID);
 if(!A){++SmokeFailures;Finish();return;}
 if(S.bShieldDown&&Store->Find(ID)->Shield>0.f)Store->ApplyDamage(ID,1-S.Team,Store->Find(ID)->Shield);
 if(BattleStance::Enabled(GetWorld()))SetStance(ID,S.Stance,Now,true);
 // Launcher shots: the bazooka in the right hand and the rifle on the back (the AI firing pose, held).
 if(auto* L=ExoLooks.Find(ID))L->AILauncherUntil=S.bLauncher?Now+1.e6:0.;
 UpdateExoLooks();
 // Move shot: the look's moving clip (the glide) played in place; the next shot changes the stance and replays its own pose.
 // The body tilt is drawn for a full-speed move: forward on the side shot, to the soldier's right on the strafe shot.
 const float Walk=A->GetCharacterMovement()->MaxWalkSpeed;
 A->SetLookShotVelocity(!S.bMove?FVector::ZeroVector:FCString::Strcmp(S.Name,TEXT("move_strafe"))==0?FVector(0,Walk,0):FVector(Walk,0,0));
 if(S.bMove)if(auto* Move=A->GetLookMove())A->GetMesh()->PlayAnimation(Move,true);
 const FVector P=Position(ID);const FVector Direction=FRotator(0,A->GetActorRotation().Yaw+S.Yaw,0).Vector();
 const FVector Eye=P+Direction*430+FVector(0,0,70);
 static TWeakObjectPtr<ACameraActor> Camera;
 if(!Camera.IsValid())
 {
  Camera=GetWorld()->SpawnActor<ACameraActor>();
  auto& PP=Camera->GetCameraComponent()->PostProcessSettings;PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=0;
  Camera->GetCameraComponent()->SetFieldOfView(60);
 }
 Camera->SetActorLocationAndRotation(Eye,(P+FVector(0,0,30)-Eye).Rotation());
 PC->SetViewTarget(Camera.Get());
 PC->bAutoManageActiveCameraTarget=false;if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
 const auto* Look=ExoLooks.Find(ID);
 UE_LOG(LogFutureBattle,Display,TEXT("EXO_SHOT shot=%s team=%d unit=%d shield=%.0f bubble=%d stance=%d scale=%.2f"),S.Name,S.Team,ID,Store->Find(ID)->Shield,
  Look&&Look->Bubble.IsValid()&&Look->Bubble->IsVisible()?1:0,StanceOf(ID),A->GetMesh()->GetRelativeScale3D().Z);
 // The bubble shows exactly while shield points are left.
 if(!Look||!Look->Bubble.IsValid()||Look->Bubble->IsVisible()!=(Store->Find(ID)->Shield>0.f))++SmokeFailures;
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 const FString File=Prefix+FString::Printf(TEXT("-%d-%s.png"),Index+1,S.Name);
 if(!Prefix.IsEmpty()){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[File](){FScreenshotRequest::RequestScreenshot(File,false,false);}),.6f,false);}
 ++ExoShotStage;Later(1.2f);
}
