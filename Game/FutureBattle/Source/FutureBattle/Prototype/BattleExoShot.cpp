// Exoskeleton look check (2026-11-18): -BattleExoShot -BattleArea -BattleWideScreens=<prefix> on a wide mixed map.
// Combat is paused; one exoskeleton soldier per team is deployed at its home and photographed from 3.2 m: front, side and back with the
// shield up, then with the shield gone, close-ups from 1.9 m, the action clips (the launcher's recoil and reload, the rifle's recoil
// and reload), crouched, prone and prone close-ups from 1.8 m (and from above), and the other team's soldier. Setup/Test-ExoShot.ps1 runs it.
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
 struct FShot{int32 Team;float Yaw;uint8 Stance;bool bShieldDown;const TCHAR* Name;bool bLauncher=false;bool bMove=false;float Distance=430.f;float EyeZ=70.f;float AimZ=30.f;
  uint8 Action=0;float Capture=.6f;const TCHAR* Expect=nullptr;};
 static const FShot Shots[]={
  {0,25.f,BattleStance::Stand,false,TEXT("front")},{0,100.f,BattleStance::Stand,false,TEXT("side")},{0,180.f,BattleStance::Stand,false,TEXT("back")},
  {0,25.f,BattleStance::Stand,false,TEXT("launcher_front"),true},{0,150.f,BattleStance::Stand,false,TEXT("launcher_back"),true},
  {0,25.f,BattleStance::Stand,true,TEXT("noshield")},{0,100.f,BattleStance::Stand,true,TEXT("move_side"),false,true},{0,25.f,BattleStance::Stand,true,TEXT("move_strafe"),false,true},
  // The bazooka in hand while moving (its glide clip) and close up (2026-11-22: the head turned backwards on the move, the shoulder
  // plates stood off the body).
  {0,100.f,BattleStance::Stand,true,TEXT("launcher_move_side"),true,true},{0,25.f,BattleStance::Stand,true,TEXT("launcher_move_front"),true,true},
  {0,200.f,BattleStance::Stand,true,TEXT("launcher_move_back"),true,true},
  {0,25.f,BattleStance::Stand,true,TEXT("launcher_close_front"),true,false,190.f,80.f,60.f},{0,200.f,BattleStance::Stand,true,TEXT("launcher_close_back"),true,false,190.f,90.f,60.f},
  {0,25.f,BattleStance::Stand,true,TEXT("close_front"),false,false,190.f,80.f,60.f},{0,100.f,BattleStance::Stand,true,TEXT("close_side"),false,false,190.f,80.f,60.f},{0,200.f,BattleStance::Stand,true,TEXT("close_back"),false,false,190.f,90.f,60.f},
  // Action shots, from 3.2 m (Action 1: a launcher shot's clips, 2: a rifle shot, 3: a rifle reload), taken Capture seconds after
  // it starts: the launcher's kick, its reload seen from the front (the round going into the muzzle) and from the left (the hand
  // behind the hip), the rifle's kick, the rifle reload half way. The clip showing then must be Expect.
  {0,25.f,BattleStance::Stand,true,TEXT("launcher_fire"),true,false,320.f,80.f,45.f,1,.1f,TEXT("Dual_Fire")},
  {0,25.f,BattleStance::Stand,true,TEXT("launcher_reload_front"),true,false,320.f,80.f,45.f,1,2.3f,TEXT("Dual_Reload")},
  {0,260.f,BattleStance::Stand,true,TEXT("launcher_reload_left"),true,false,320.f,80.f,45.f,1,1.75f,TEXT("Dual_Reload")},
  {0,25.f,BattleStance::Stand,true,TEXT("rifle_fire"),false,false,320.f,80.f,45.f,2,.12f,TEXT("OneHand_Fire")},
  {0,60.f,BattleStance::Stand,true,TEXT("rifle_reload"),false,false,320.f,80.f,45.f,3,1.f,TEXT("OneHand_ReloadStill")},
  // Jump shots (Action 4: a jump of the game's height, 480 cm/s up; 2026-11-24): high in the air with the rifle and with the
  // launcher (the jump clip), and just after touching down (the landing clip).
  {0,90.f,BattleStance::Stand,true,TEXT("jump_air"),false,false,520.f,150.f,110.f,4,.4f,TEXT("OneHand_Jump")},
  {0,30.f,BattleStance::Stand,true,TEXT("launcher_jump_air"),true,false,520.f,150.f,110.f,4,.4f,TEXT("Dual_Jump")},
  {0,90.f,BattleStance::Stand,true,TEXT("jump_land"),false,false,380.f,80.f,30.f,4,1.12f,TEXT("OneHand_Land")},
  {0,60.f,BattleStance::Crouch,true,TEXT("crouch")},{0,60.f,BattleStance::Prone,true,TEXT("prone")},
  {0,120.f,BattleStance::Prone,true,TEXT("prone_side")},{0,200.f,BattleStance::Prone,true,TEXT("prone_back")},  // after the get-down clip
  {0,30.f,BattleStance::Prone,true,TEXT("prone_close_front"),false,false,180.f,25.f,-50.f},{0,95.f,BattleStance::Prone,true,TEXT("prone_close_side"),false,false,180.f,25.f,-50.f},
  {0,160.f,BattleStance::Prone,true,TEXT("prone_close_back"),false,false,180.f,25.f,-50.f},{0,60.f,BattleStance::Prone,true,TEXT("prone_close_top"),false,false,120.f,170.f,-60.f},
  {1,25.f,BattleStance::Stand,false,TEXT("enemy")}};
 const int32 Index=ExoShotStage-1;
 if(Index>=UE_ARRAY_COUNT(Shots)){Finish();return;}
 const FShot& S=Shots[Index];const int32 ID=ExoShotIDs[S.Team];ABattleUnit* A=Actor(ID);
 if(!A){++SmokeFailures;Finish();return;}
 if(S.bShieldDown&&Store->Find(ID)->Shield>0.f)Store->ApplyDamage(ID,1-S.Team,Store->Find(ID)->Shield);
 if(BattleStance::Enabled(GetWorld()))SetStance(ID,S.Stance,Now,true);
 A->PlayLookSequence(nullptr,nullptr,Now);  // the last shot's launcher clips stop
 // Launcher shots: the bazooka in the right hand and the rifle on the back (the AI firing pose, held).
 if(auto* L=ExoLooks.Find(ID))L->AILauncherUntil=S.bLauncher?Now+1.e6:0.;
 UpdateExoLooks();
 // Move shot: the look's moving clip (the glide) played in place; the next shot changes the stance and replays its own pose.
 // The body tilt is drawn for a full-speed move: forward on the side shot, to the soldier's right on the strafe shot.
 const float Walk=A->GetCharacterMovement()->MaxWalkSpeed;
 A->SetLookShotVelocity(!S.bMove?FVector::ZeroVector:FCString::Strcmp(S.Name,TEXT("move_strafe"))==0?FVector(0,Walk,0):FVector(Walk,0,0));
 if(S.bMove)if(auto* Move=A->GetLookMove())A->GetMesh()->PlayAnimation(Move,true);
 if(S.Action==1)PlayExoLauncherAction(ID,Now);
 if(S.Action==2)A->NoteVisualShot(Now);
 if(S.Action==4)A->LaunchCharacter(FVector(0,0,A->GetCharacterMovement()->JumpZVelocity),false,true);
 if(S.Action==3){auto& Edit=Store->Units.FindChecked(ID);Edit.Ammo=FMath::Max(0,Edit.Ammo-1);if(!Store->StartReload(ID,Now))++SmokeFailures;}
 const FVector P=Position(ID);const FVector Direction=FRotator(0,A->GetActorRotation().Yaw+S.Yaw,0).Vector();
 const FVector Eye=P+Direction*S.Distance+FVector(0,0,S.EyeZ);
 static TWeakObjectPtr<ACameraActor> Camera;
 if(!Camera.IsValid())
 {
  Camera=GetWorld()->SpawnActor<ACameraActor>();
  auto& PP=Camera->GetCameraComponent()->PostProcessSettings;PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=0;
  Camera->GetCameraComponent()->SetFieldOfView(60);
 }
 Camera->SetActorLocationAndRotation(Eye,(P+FVector(0,0,S.AimZ)-Eye).Rotation());
 PC->SetViewTarget(Camera.Get());
 PC->bAutoManageActiveCameraTarget=false;if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
 const auto* Look=ExoLooks.Find(ID);
 UE_LOG(LogFutureBattle,Display,TEXT("EXO_SHOT shot=%s team=%d unit=%d shield=%.0f bubble=%d stance=%d scale=%.2f"),S.Name,S.Team,ID,Store->Find(ID)->Shield,
  Look&&Look->Bubble.IsValid()&&Look->Bubble->IsVisible()?1:0,StanceOf(ID),A->GetMesh()->GetRelativeScale3D().Z);
 // The bubble shows exactly while shield points are left.
 if(!Look||!Look->Bubble.IsValid()||Look->Bubble->IsVisible()!=(Store->Find(ID)->Shield>0.f))++SmokeFailures;
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 const FString File=Prefix+FString::Printf(TEXT("-%d-%s.png"),Index+1,S.Name);
 const TWeakObjectPtr<ABattleUnit> Body(A);
 FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[this,File,Body,Name=S.Name,Expect=S.Expect,bShoot=!Prefix.IsEmpty()]()
 {
  const UAnimSequence* Shown=Body.IsValid()?Body->GetShownPose():nullptr;
  const bool bOk=!Expect||(Shown&&Shown->GetName().Contains(Expect));
  UE_LOG(LogFutureBattle,Display,TEXT("EXO_SHOT_CAPTURE shot=%s pose=%s%s"),Name,Shown?*Shown->GetName():TEXT("none"),bOk?TEXT(""):TEXT(" EXPECTED_OTHER"));
  if(!bOk)++SmokeFailures;
  // The other team's body is hidden while no one of team 0 spots it (RefreshVisibility); shown again for the picture.
  if(Body.IsValid())Body->SetActorHiddenInGame(false);
  if(bShoot)FScreenshotRequest::RequestScreenshot(File,false,false);
 }),S.Capture,false);
 ++ExoShotStage;Later(FMath::Max(1.2f,S.Capture+.6f));
}
