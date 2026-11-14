#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleFieldLayout.h"
#include "BattleTactics.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunCoverActionSmoke()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
 auto Check=[&](bool OK,const TCHAR* Label){SmokeFailures+=!OK;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunCoverActionSmoke,Delay,false);};
 auto Place=[&](int32 ID,FVector P){auto& B=Bodies.FindChecked(ID);if(B.Actor.IsValid())B.Actor->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);else{B.Capsule->SetWorldLocation(P);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=P;}};
 const double Now=GetWorld()->GetTimeSeconds();
 auto Fixture=[&](FVector P){return BattleFieldLayout::IsExpanded(GetWorld())?BattleFieldLayout::Expand(P-FVector(-200,16000,0),true)+FVector(-1733.333333,-4500,0):P;};
 const FVector Threat=Fixture(FVector(-3000,16000,100));
 if(SmokeStage==0)
 {
  Mode->CombatStartsAt=0;Mode->bCombatPaused=false;Match->DirectorClock=-1000;Match->DelegationClock=-1000;
  for(const auto& P:Store->GetSquads())IssueOrder(P.Key,EBattleOrder::Hold,Position(P.Value.LeaderUnitID));
  for(const auto& P:Store->GetUnits())if(P.Key!=1&&P.Key!=2){Store->Units.FindChecked(P.Key).Ammo=0;Store->Units.FindChecked(P.Key).Reserve=0;}
  Place(1,Fixture(FVector(-55,16525,100)));Place(2,Fixture(FVector(-55,15475,100)));Place(251,Threat);
  for(int32 ID:{1,2,251}){auto& B=Bodies.FindChecked(ID);if(B.Actor.IsValid())B.Actor->SetActorRotation(FRotator(0,ID==251?0:180,0));else Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=FVector(-1,0,0);}
  auto& U=Store->Units.FindChecked(1);U.NextShotTime=0;U.bWeaponAimInitialized=false;
  Store->UpdateAim(1,Now,0,FVector(1,0,0),true);Store->UpdateAim(1,Now,0,FVector(-1,0,0),true);
  const int32 Ammo=U.Ammo;Check(!Store->ConsumeShot(1,Now,.2)&&U.Ammo==Ammo,TEXT("large aim change delays actual shot without consuming ammunition"));
  Check(Store->ConsumeShot(1,Now+.6,.2),TEXT("shot becomes available after shared aim delay"));U.NextShotTime=0;
  {
   auto* Tuning=GetMutableDefault<UBattleTuning>();
   Check(Tuning->AimSwitchDelay(44)==0 && FMath::IsNearlyEqual(Tuning->AimSwitchDelay(45),.2375f,.001f) && FMath::IsNearlyEqual(Tuning->AimSwitchDelay(180),.5f,.001f),TEXT("default aim switch tuning keeps previous 0.24-0.5s delay"));
   const float OldBase=Tuning->AimSwitchBaseSeconds;Tuning->AimSwitchBaseSeconds=1.f;
   U.bWeaponAimInitialized=false;Store->UpdateAim(1,Now,0,FVector(1,0,0),true);Store->UpdateAim(1,Now,0,FVector(-1,0,0),true);
   const bool Early=Store->ConsumeShot(1,Now+.6,.2);const bool Late=Store->ConsumeShot(1,Now+1.36,.2);
   Tuning->AimSwitchBaseSeconds=OldBase;U.NextShotTime=0;U.Ammo=FMath::Max(U.Ammo,Ammo);
   Check(!Early && Late,TEXT("aim switch delay follows shared config"));
  }
  Check(!RequestCoverAction(21,FVector::ZeroVector),TEXT("open field cannot manufacture cover"));
  PC->ControlUnit(Actor(1));
  CoverProbeAfter.Remove(1);Follow(1,.016f,Now);Check(!CoverProbeAfter.Contains(1),TEXT("direct-controlled unit is outside AI cover probe cognition"));
  auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();Sense->Reset();Sense->Update(Mode->Units,Now-1);Sense->Update(Mode->Units,Now);
  Check(RequestCoverAction(1,Threat),TEXT("direct controlled Actor enters physical cover"));
  Check(RequestCoverAction(2,Threat,251),TEXT("Mass follower uses same cover action"));
  SmokePosition=Position(1);BayStart=Now;SmokeStage=1;Later(.1);return;
 }
 if(SmokeStage==1)
 {
  if(Store->Find(1)->CoverPhase!=2 && Now-BayStart<5){Later(.1);return;}
  Check(Store->Find(1)->CoverPhase==2,TEXT("Actor reaches protected anchor"));
  Match->DirectorClock=0;Match->DelegationClock=0;
  FString Error;auto* Snapshot=Match->CaptureSnapshot();Check(Snapshot&&Snapshot->Validate(Error),TEXT("active cover snapshot validates"));
  Check(Match->SaveMatch(TEXT("FutureBattleSmokeOnly"))&&Match->LoadMatch(TEXT("FutureBattleSmokeOnly")),TEXT("active cover serializes and restores"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_SAVE message=%s validation=%s"),*Match->LastMessage,*Error);
  Match->DirectorClock=-1000;Match->DelegationClock=-1000;
  Check(Store->Find(1)->CoverPhase==2 && FVector::Dist2D(Position(1),Store->Find(1)->CoverAnchor)<40,TEXT("cover phase and anchor survive loading"));
  Check(FVector::Dist2D(Store->Find(1)->CoverAnchor,Store->Find(1)->CoverPeek)>=100 && Store->Find(2)->CoverTargetID==251,TEXT("peek destination and Mass target survive serialization"));
  {
   const bool AnchorLine=CoverLineClear(1,Store->Find(1)->CoverAnchor,Threat),PeekLine=CoverLineClear(1,Store->Find(1)->CoverPeek,Threat);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_LINE anchor=%d peek=%d"),AnchorLine,PeekLine);
   Check(GetDefault<UBattleTuning>()->bAICoverValidation && BattleTactics::CoverValidationEnabled(),TEXT("AI cover validation is on by default"));
   Check(GetDefault<UBattleTuning>()->bAICoverFire && BattleTactics::CoverFireEnabled(),TEXT("AI cover firing window is on by default"));
   Check(GetDefault<UBattleTuning>()->bAICoverPeekSight && BattleTactics::CoverPeekSightEnabled(),TEXT("AI blind peek exit is on by default"));
   Check(PeekLine,TEXT("peek destination has a clear line to the threat"));
   const FVector PeekBefore=Store->Find(2)->CoverPeek;
   Check(RevalidatePeek(2,Threat) && CoverLineClear(2,Store->Find(2)->CoverPeek,Threat),TEXT("Mass follower peek revalidates against the observed threat"));
   Store->Units.FindChecked(2).CoverPeek=PeekBefore;
  }
  const int32 Ammo=Store->Find(1)->Ammo;
  Check(!Fire(1,Position(1)+FVector(0,0,60),FVector(-1,0,0),true)&&Store->Find(1)->Ammo==Ammo,TEXT("hidden trigger requests emergence without firing through cover"));
  Check(Store->Find(1)->CoverPhase==3,TEXT("trigger starts physical peek movement"));
  BayStart=Now;SmokeStage=2;Later(.1);return;
 }
 if(SmokeStage==2)
 {
  if(Store->Find(1)->CoverPhase!=4 && Now-BayStart<5){Later(.1);return;}
  Check(Store->Find(1)->CoverPhase==4 && FVector::Dist2D(Position(1),SmokePosition)>100,TEXT("peek moves actual body before enabling fire"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_PEEK phase=%d position=%s goal=%s player=%d paused=%d"),Store->Find(1)->CoverPhase,*Position(1).ToCompactString(),*Store->Find(1)->CoverPeek.ToCompactString(),Actor(1)->IsPlayerControlled(),Mode->bCombatPaused);
  auto& U=Store->Units.FindChecked(1);U.NextShotTime=0;
  const FVector Aim=(Position(251)-(Position(1)+FVector(0,0,60))).GetSafeNormal();U.WeaponAimDirection=Aim;
  Check(Fire(1,Position(1)+FVector(0,0,60),Aim,true),TEXT("exposed Actor fires through shared weapon path"));
  PC->EnterTactical();if(auto* C=Cast<ABattleCameraPawn>(PC->GetPawn())){C->SetActorLocation(Fixture(FVector(-400,16000,0)));C->Zoom(3500-C->Camera->GetRelativeLocation().Z);}
  FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("CoverScreenshot="),Prefix);if(!Prefix.IsEmpty())FScreenshotRequest::RequestScreenshot(Prefix,true,false);
  SmokeStage=3;Later(.5);return;
 }
 CancelCoverAction(1);Check(Store->Find(1)->CoverPhase==0,TEXT("explicit release clears cover action"));
 IssueOrder(1,EBattleOrder::Move,Fixture(FVector(-1500,18000,0)));StepCoverAction(1,.1f,Now);
 Check(Store->Find(1)->CoverPhase==0,TEXT("new movement order retains movement authority"));
 CancelCoverAction(2);Check(Store->Find(2)->CoverPhase==0,TEXT("Mass release clears shared action"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
