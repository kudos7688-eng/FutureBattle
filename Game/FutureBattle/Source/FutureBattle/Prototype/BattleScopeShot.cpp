// Aim / scope view check (2026-11-18): -BattleScopeShot -BattleWideScreens=<prefix> on a wide mixed map.
// Combat is paused; leader 1 is taken under direct control and photographed (HUD included) in each view:
// rear camera, aim (x1.6), scope x4, scope x8, and after release. Logs the magnification, field of view and camera offset.
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunScopeShot()
{
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());ABattleUnit* A=Actor(1);
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunScopeShot,Delay,false);};
 Mode->bCombatPaused=true;
 if(!PC||!A){UE_LOG(LogFutureBattle,Error,TEXT("SCOPE_SHOT no controller or leader"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
 static const TCHAR* const Names[]={TEXT("tps"),TEXT("aim"),TEXT("scope4"),TEXT("scope8"),TEXT("released")};
 static const float Want[]={1.f,1.6f,4.f,8.f,1.f};
 const int32 Step=StanceShotStage/2,Phase=StanceShotStage%2; // per view: set, then check and photograph
 if(Step>=5)
 {
  UE_LOG(LogFutureBattle,Display,TEXT("SCOPE_SHOT_RESULT failures=%d"),SmokeFailures);
  FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);return;
 }
 if(Phase==0)
 {
  if(Step==0&&!PC->ControlUnit(A)){UE_LOG(LogFutureBattle,Error,TEXT("SCOPE_SHOT cannot control leader 1"));++SmokeFailures;}
  PC->bAimHeld=Step>=1&&Step<=3;if(PC->bAimHeld)PC->AimLevel=Step;
  ++StanceShotStage;Later(1.f);return;
 }
 FMinimalViewInfo View;A->CalcCamera(0,View);
 const FVector Offset=View.Rotation.UnrotateVector(View.Location-A->FirstPersonCamera->GetComponentLocation());
 const bool Scope=Step==2||Step==3;
 const auto* T=GetDefault<UBattleTuning>();
 const float WantSpread[]={1.f,T->AimSpreadScale,T->Scope4SpreadScale,T->Scope8SpreadScale,1.f};
 const float Spread=BattleTactics::SpreadDegrees(*A->GetState(),GetWorld()->GetTimeSeconds(),A->PlayerSpreadScale());
 UE_LOG(LogFutureBattle,Display,TEXT("SCOPE_SHOT view=%s spread_scale=%.2f spread_degrees=%.3f"),Names[Step],A->AimSpreadScale(),Spread);
 const bool Ok=FMath::IsNearlyEqual(A->AimSpreadScale(),WantSpread[Step],.001f)&&FMath::IsNearlyEqual(A->PlayerSpreadScale(),FMath::Clamp(T->PlayerSpreadScale,.05f,1.f)*WantSpread[Step],.001f)&&FMath::IsNearlyEqual(A->AimMagnification(),Want[Step],.01f)&&A->ScopeView()==Scope&&bool(A->GetMesh()->bOwnerNoSee)==(Scope&&A->UsesThirdPersonView());
 if(!Ok)++SmokeFailures;
 UE_LOG(LogFutureBattle,Display,TEXT("SCOPE_SHOT view=%s magnification=%.2f fov=%.1f camera=%s scope=%d body_hidden=%d ok=%d"),Names[Step],A->AimMagnification(),View.FOV,*Offset.ToCompactString(),A->ScopeView(),bool(A->GetMesh()->bOwnerNoSee),Ok);
 FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),Prefix);
 if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-")+Names[Step]+TEXT(".png"),true,false);
 ++StanceShotStage;Later(.5f);
}
