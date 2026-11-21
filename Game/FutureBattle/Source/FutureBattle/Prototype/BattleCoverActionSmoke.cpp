#include "BattleLeaderRuntime.h"
#include "BattleBlindFire.h"
#include "BattleBodyAnim.h"
#include "BattleStance.h"
#include "BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleFieldLayout.h"
#include "BattleTactics.h"
#include "BattleSuppression.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "BattleVATDisplay.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
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
 // Rendered run (-CoverScreenshot=): a close view of the player's soldier from beside the cover line, for the body clips.
 auto BodyShotPrefix=[]{FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("CoverScreenshot="),Prefix);return Prefix;};
 auto BodyShot=[&](const TCHAR* Name)
 {
  const FVector Body=Position(1),ToThreat=(Threat-Body).GetSafeNormal2D(),Side=FVector::CrossProduct(FVector::UpVector,ToThreat);
  // The camera is placed one stage before the picture (stage 5): a picture on the frame of a camera cut is smeared by motion blur.
  if(!PC || Cast<ACameraActor>(PC->GetViewTarget())==nullptr)
  {
   const FVector Eye=Body+Side*260.f-ToThreat*100.f+FVector(0,0,90);
   if(auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Body-Eye).Rotation());Camera && PC){PC->bAutoManageActiveCameraTarget=false;PC->SetViewTarget(Camera);}
   return;
  }
  const FString Path=FPaths::ChangeExtension(BodyShotPrefix(),TEXT(""))+TEXT("-")+Name+TEXT(".png");
  FScreenshotRequest::RequestScreenshot(Path,true,false);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_BODY_SHOT %s phase=%d blind=%d base=%s"),*Path,Store->Find(1)->CoverPhase,IsBlindFiring(1,GetWorld()->GetTimeSeconds()),
   *GetNameSafe(Cast<UBattleBodyAnimInstance>(Actor(1)->GetMesh()->GetAnimInstance())?Cast<UBattleBodyAnimInstance>(Actor(1)->GetMesh()->GetAnimInstance())->GetBase():nullptr));
 };
 auto BlindFire=[&]
 {
   // Blind fire: firing without aiming shoots from past the cover edge while the body stays at the anchor.
   auto& P=Store->Units.FindChecked(1);P.NextShotTime=0;P.ReloadEndTime=0;
   const int32 Before=P.Ammo;const FVector Eye=Position(1)+FVector(0,0,60);
   // Already facing the threat: the aim switch delay is a separate check (a rendered run waits here, and the AI may turn the rifle).
   P.WeaponAimDirection=(Threat-Eye).GetSafeNormal();
   FVector Origin;const bool HasOrigin=BlindFireOrigin(1,Threat,Origin);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BLINDFIRE has_origin=%d over_top=%d origin=%s anchor=%s"),HasOrigin,BattleBlindFire::IsOverTop(P),*Origin.ToCompactString(),*P.CoverAnchor.ToCompactString());
   Check(HasOrigin,TEXT("hidden player has a blind fire origin past the cover"));
   Actor(1)->SetAimLevel(0);
   Check(Fire(1,Eye,(Threat-Eye).GetSafeNormal(),true)&&Store->Find(1)->Ammo<Before,TEXT("unaimed trigger behind cover blind-fires"));
   Check(Store->Find(1)->CoverPhase==2&&IsBlindFiring(1,Now)&&FVector::Dist2D(Position(1),Store->Find(1)->CoverAnchor)<40,TEXT("blind fire keeps the body hidden at the anchor"));
   Check(!IsHeadExposed(1,Now)&&TargetAimOffset(1)==BattleStance::AimOffset(StanceOf(1)),TEXT("blind fire around the edge does not expose the head"));
   if(BattleBlindFire::ExposeEnabled())
   {
    // A little of the body shows past the edge: it is seen and looks from there, and a bullet through it hits.
    FVector Shown;const bool bShown=SideExposure(1,Now,Shown);
    Check(bShown&&Shown.Equals(Origin,1.)&&Sight(1).Eye.Equals(Shown,1.),TEXT("blind fire around the edge shows a little of the body past the edge"));
    if(bShown)
    {
     auto& E=Store->Units.FindChecked(251);E.NextShotTime=0;E.ReloadEndTime=0;E.Ammo=FMath::Max(E.Ammo,1);
     const FVector Dir=(Threat-Shown).GetSafeNormal(),From=Shown+Dir*120.;E.WeaponAimDirection=-Dir;
     const float HP=Store->Find(1)->HP;Fire(251,From,-Dir,false);
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BLINDFIRE_SIDE shown=%s hp_before=%.0f hp_after=%.0f"),*Shown.ToCompactString(),HP,Store->Find(1)->HP);
     Check(Store->Find(1)->HP<HP,TEXT("a bullet through the part past the edge hits the blind-firing soldier"));
     Store->Units.FindChecked(1).HP=HP;
    }
   }
 };
 // Over the top (the stand-up kind: peek point = anchor): the head comes up while the clip shows, so others aim at and hit it.
 auto FireOverTop=[&]
 {
  auto& P=Store->Units.FindChecked(1);SmokeSavedPeek=P.CoverPeek;P.CoverPeek=P.CoverAnchor;
  P.NextShotTime=0;const FVector Eye=Position(1)+FVector(0,0,60);P.WeaponAimDirection=(Threat-Eye).GetSafeNormal();
  FVector Origin;const bool HasOrigin=BlindFireOrigin(1,Threat,Origin);const bool Fired=Fire(1,Eye,(Threat-Eye).GetSafeNormal(),true);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BLINDFIRE_TOP has_origin=%d fired=%d exposed=%d aim=%.0f top=%.0f"),HasOrigin,Fired,IsHeadExposed(1,Now),TargetAimOffset(1),TargetTopOffset(1));
  Check(HasOrigin&&Fired,TEXT("over-the-top blind fire fires from above the cover"));
  if(BattleBlindFire::ExposeEnabled())
  {
   Check(IsHeadExposed(1,Now)&&TargetAimOffset(1)==BattleBlindFire::ExposedAimOffset&&TargetTopOffset(1)==BattleBlindFire::ExposedTopOffset,TEXT("over-the-top blind fire exposes the head to sight and hits"));
   Check(FMath::IsNearlyEqual(Sight(1).Eye.Z-Position(1).Z,BattleBlindFire::ExposedEyeOffset,1.f),TEXT("the exposed head looks over the cover from its own height"));
  }
 };
 auto EndOverTop=[&]
 {
  Store->Units.FindChecked(1).CoverPeek=SmokeSavedPeek;BlindFireUntil.Remove(1);
  FVector Shown;
  Check(!IsHeadExposed(1,GetWorld()->GetTimeSeconds())&&!SideExposure(1,GetWorld()->GetTimeSeconds(),Shown)&&TargetTopOffset(1)==BattleStance::TopOffset(StanceOf(1))
   &&FMath::IsNearlyEqual(Sight(1).Eye.Z-Position(1).Z,BattleStance::EyeOffset(StanceOf(1)),1.f),TEXT("head is hidden again once the blind fire ends (sight and eye)"));
 };
 auto AfterBlindFire=[&]
 {
  if(PC && PC->GetPawn() && Cast<ACameraActor>(PC->GetViewTarget())){PC->bAutoManageActiveCameraTarget=true;PC->SetViewTarget(PC->GetPawn());}
  Actor(1)->SetAimLevel(1);
  const int32 Ammo=Store->Find(1)->Ammo;
  Check(!Fire(1,Position(1)+FVector(0,0,60),FVector(-1,0,0),true)&&Store->Find(1)->Ammo==Ammo,TEXT("aimed trigger requests emergence without firing through cover"));
  Actor(1)->SetAimLevel(0);
  Check(Store->Find(1)->CoverPhase==3,TEXT("trigger starts physical peek movement"));
  BayStart=Now;SmokeStage=2;Later(.1);
 };
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
  {
   // Suppression trial (default OFF): accrual from the bullet path, decay, cap, spread multiplier.
   auto& Tune=BattleSuppression::MutableTuning();const auto Saved=Tune;
   const FVector Near=Position(251)+FVector(0,50,35),A=Near-FVector(1000,0,0),B=Near+FVector(1000,0,0);
   Check(!Saved.bEnabled,TEXT("suppression trial is off by default"));
   SuppressNear(0,A,B,Now);
   Check(Suppression(251,Now)==0 && SuppressionSpreadScale(251,Now)==1.f && !IsPinned(251,Now),TEXT("disabled suppression changes nothing"));
   Tune=BattleSuppression::FTuning();Tune.bEnabled=true;
   SuppressNear(0,A,B,Now);const float One=Suppression(251,Now);
   Check(FMath::IsNearlyEqual(One,.09f,.005f),TEXT("near miss at 50cm adds distance-scaled suppression"));
   SuppressNear(1,A,B,Now);SuppressNear(0,A+FVector(0,300,0),B+FVector(0,300,0),Now);
   Check(FMath::IsNearlyEqual(Suppression(251,Now),One,.0001f),TEXT("own-team fire and distant paths do not suppress"));
   Check(FMath::IsNearlyEqual(Suppression(251,Now+.2),One-.05f,.005f) && Suppression(251,Now+1)==0,TEXT("suppression decays linearly to zero"));
   for(int32 I=0;I<20;++I)AddSuppression(251,.12f,Now);
   const auto& Victim=*Store->Find(251);
   Check(Suppression(251,Now)==1.f && IsPinned(251,Now) && FMath::IsNearlyEqual(SuppressionSpreadScale(251,Now),2.5f,.001f),TEXT("suppression caps at 1 and pins"));
   Check(FMath::IsNearlyEqual(BattleTactics::SpreadDegrees(Victim,Now,2.5f),2.5f*BattleTactics::SpreadDegrees(Victim,Now),.001f),TEXT("suppression multiplies the shared weapon spread"));
   Check(!IsPinned(251,Now+2.1),TEXT("pin ends as suppression decays"));
   SuppressionZeroAt.Reset();Tune=Saved;
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
  // Body animation (BattleBodyAnim.h): the leader body uses the layered instance once the clips are imported, and every clip loads.
  if(BattleBodyAnim::Enabled())
  {
   int32 Missing=0;for(int32 C=0;C<int32(BattleBodyAnim::EClip::Count);++C)Missing+=!BattleBodyAnim::Get(BattleBodyAnim::EClip(C));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BODY_ANIM_SMOKE missing=%d instance=%s"),Missing,*GetNameSafe(Actor(1)->GetMesh()->GetAnimInstance()));
   Check(Missing==0,TEXT("all body animation clips load"));
   Check(Cast<UBattleBodyAnimInstance>(Actor(1)->GetMesh()->GetAnimInstance())!=nullptr,TEXT("leader body uses the layered body animation"));
   Check(BattleStance::BodyScale(BattleStance::Prone)==1.f,TEXT("prone body keeps its size with the prone clip"));
   Check(BattleBodyAnim::Locomotion(0,FVector(-200,0,0),false)==BattleBodyAnim::Get(BattleBodyAnim::EClip::StandBwd)&&BattleBodyAnim::Locomotion(1,FVector(0,200,0),false)==BattleBodyAnim::Get(BattleBodyAnim::EClip::CrouchRight),TEXT("walk clip follows the direction of travel"));
   Check(BattleBodyAnim::Locomotion(0,FVector(200,0,0),true)==BattleBodyAnim::Get(BattleBodyAnim::EClip::StandJog)&&BattleBodyAnim::Locomotion(0,FVector(-200,0,0),true)==BattleBodyAnim::Get(BattleBodyAnim::EClip::JogBwd)&&BattleBodyAnim::Locomotion(1,FVector(200,0,0),true)==BattleBodyAnim::Get(BattleBodyAnim::EClip::CrouchFwd),TEXT("moving without firing runs with the rifle low (standing only)"));
   Check(BattleBodyAnim::Death(FVector(1,0,0),FVector(1,0,0),0)==BattleBodyAnim::Get(BattleBodyAnim::EClip::DeathBack),TEXT("a shot from behind picks the back death clip"));
  }
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
  {
   // Suppression trial: a pinned AI soldier behind cover does not step out; once it wears off he does.
   auto& Tune=BattleSuppression::MutableTuning();const auto Saved=Tune;Tune=BattleSuppression::FTuning();Tune.bEnabled=true;
   auto& F=Store->Units.FindChecked(2);
   Check(F.CoverPhase!=0,TEXT("Mass follower is still in its cover action"));
   F.CoverPhase=2;F.CoverSeconds=0;F.ReloadEndTime=0;AddSuppression(2,1.f,Now);StepCoverAction(2,.016f,Now);
   Check(F.CoverPhase==2,TEXT("pinned AI soldier stays hidden"));
   SuppressionZeroAt.Reset();F.CoverSeconds=0;StepCoverAction(2,.016f,Now);
   Check(F.CoverPhase==3,TEXT("AI soldier leaves cover to peek once suppression is gone"));
   F.CoverPhase=4;F.CoverSeconds=2;AddSuppression(2,1.f,Now);StepCoverAction(2,.016f,Now);
   // The body is still at the anchor in this fixture, so the return move (1) may complete to hidden (2) in the same step.
   Check(F.CoverPhase==1 || F.CoverPhase==2,TEXT("pinned AI soldier ducks back from an exposed peek"));
   SuppressionZeroAt.Reset();Tune=Saved;
  }
  // Rendered run: close views of the hidden body and of the blind fire before going on (stages 5 to 7).
  if(!BodyShotPrefix().IsEmpty()){SmokeStage=5;Later(.6);return;}
  BlindFire();
  FireOverTop();EndOverTop();
  AfterBlindFire();
  return;
 }
 if(SmokeStage==5){BodyShot(nullptr);SmokeStage=8;Later(.5);return;}
 if(SmokeStage==8){BodyShot(TEXT("hidden"));SmokeStage=6;Later(.2);return;}
 if(SmokeStage==6){BlindFire();SmokeStage=7;Later(.3);return;}
 if(SmokeStage==7){BodyShot(TEXT("blind"));SmokeStage=9;Later(.7);return;}
 if(SmokeStage==9)
 {
  FireOverTop();
  SmokeStage=10;Later(.3);return;
 }
 if(SmokeStage==10)
 {
  BodyShot(TEXT("blind-top"));
  {
   // The rifle is above the cover; the head shows over it while exposed (rules), else stays below (display; posed by the
   // anim tick between the stages).
   auto* Mesh=Actor(1)->GetMesh();const float* Top=BlindFireTop.Find(1);
   const float Hand=Mesh->GetSocketLocation(TEXT("hand_r")).Z,Head=Mesh->GetSocketLocation(TEXT("head")).Z;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BLINDFIRE_TOP_POSE cover_top=%.0f hand_r=%.0f head=%.0f ground=%.0f exposed_top=%.0f"),Top?*Top:-1.f,Hand,Head,Mesh->GetComponentLocation().Z,Position(1).Z+TargetTopOffset(1));
   Check(Top && Hand>*Top && (BattleBlindFire::ExposeEnabled()?Head>*Top:Head<*Top),TEXT("over-the-top blind fire shows the rifle (and the exposed head) above the cover"));
  }
  EndOverTop();
  // The picture is taken at the end of this frame: the view goes back to the soldier on the next stage.
  SmokeStage=11;Later(.1);return;
 }
 if(SmokeStage==11){AfterBlindFire();return;}
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
 // Rendered run: out of cover, a hit reaction and then a shot (recoil) over the standing aimed body. Lyra's hit, recoil and
 // aim clips are additive: the upper body must keep its height and size (it vanished when they were blended as poses).
 if(SmokeStage==3 && !BodyShotPrefix().IsEmpty())
 {
  CancelCoverAction(1);
  if(auto* Body=Cast<UBattleBodyAnimInstance>(Actor(1)->GetMesh()->GetAnimInstance()))Body->PlayOverlay(BattleBodyAnim::Get(BattleBodyAnim::EClip::HitFront1),1.f);
  SmokeStage=12;Later(.15);return;
 }
 if(SmokeStage==12||SmokeStage==13)
 {
  auto* Mesh=Actor(1)->GetMesh();
  const float Head=Mesh->GetSocketLocation(TEXT("head")).Z-Mesh->GetComponentLocation().Z;
  const float Size=Mesh->GetSocketTransform(TEXT("spine_03"),RTS_Component).GetScale3D().GetMin();
  const bool bHit=SmokeStage==12;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BODY_OVERLAY_POSE overlay=%s head=%.0f spine_scale=%.2f"),bHit?TEXT("hit"):TEXT("recoil"),Head,Size);
  Check(Head>120.f&&Size>.9f,bHit?TEXT("a hit reaction keeps the upper body whole (additive clip)"):TEXT("a shot's recoil keeps the upper body whole (additive clip)"));
  if(bHit){Actor(1)->NoteVisualShot(Now);SmokeStage=13;Later(.15);return;}
  BayStart=Now;SmokeStage=14;
 }
 // Rendered run: the player strafes right and fires, aiming 8 degrees down. The rifle sits in hand_r, so the strafe clip's hand
 // turned it away from the shot; the chest turns it back along the aim (FBattleBodyAnimProxy::AlignMuzzle). Input every frame;
 // the barrel's 3D angle to the aim is measured every frame from 0.4 s (after the 0.2 s cross-fade) to 1.2 s.
 if(SmokeStage==14)
 {
  ABattleUnit* A=Actor(1);
  if(Now-BayStart<.01){PC->ControlUnit(A);SmokeMuzzleMax=0.f;SmokeMuzzleFrames=0;SmokeShots=0;}
  PC->SetControlRotation(FRotator(-8.f,A->GetActorRotation().Yaw,0.f));
  const FVector Aim=PC->GetControlRotation().Vector();
  if(Now-BayStart<1.2)
  {
   A->AddMovementInput(A->GetActorRightVector(),1.f);
   Store->Units.FindChecked(1).Ammo=FMath::Max(Store->Find(1)->Ammo,5);
   SmokeShots+=Fire(1,A->EyePosition(),Aim,true)?1:0;
   if(Now-BayStart>.4)
   {
    const FVector Barrel=-A->Rifle->GetComponentTransform().GetUnitAxis(EAxis::Y);
    SmokeMuzzleMax=FMath::Max(SmokeMuzzleMax,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Barrel.GetSafeNormal(),Aim),-1.,1.))));
    ++SmokeMuzzleFrames;
   }
   Later(.001f);return;
  }
  const auto* Body=Cast<UBattleBodyAnimInstance>(A->GetMesh()->GetAnimInstance());
  const FVector Local=A->GetActorRotation().UnrotateVector(A->GetVelocity());
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BODY_MUZZLE_ALIGN base=%s local_velocity=%s frames=%d shots=%d barrel_to_aim_max=%.1f"),Body&&Body->GetBase()?*Body->GetBase()->GetName():TEXT("none"),*Local.ToCompactString(),SmokeMuzzleFrames,SmokeShots,SmokeMuzzleMax);
  Check(Local.Y>100.f && SmokeMuzzleFrames>5 && SmokeShots>=2 && SmokeMuzzleMax<8.f,TEXT("firing while strafing points the barrel along the aim (chest turns the rifle)"));
  SmokeStage=15;BayStart=Now;Later(.8);return;
 }
 // Rendered run: a hit through the normal path (HP drops, the body's tick plays the reaction); a second hit 0.3 s later does
 // not restart it; the reaction ends and the body stands back up (head height and chest angle as before); a hit after
 // HitReactSeconds plays again.
 if(SmokeStage>=15 && SmokeStage<=19)
 {
  ABattleUnit* A=Actor(1);auto* Body=Cast<UBattleBodyAnimInstance>(A->GetMesh()->GetAnimInstance());auto* Mesh=A->GetMesh();
  auto& U=Store->Units.FindChecked(1);
  const float Head=Mesh->GetSocketLocation(TEXT("head")).Z-Mesh->GetComponentLocation().Z;
  const FQuat Chest=Mesh->GetSocketTransform(TEXT("spine_03"),RTS_Component).GetRotation();
  const UAnimSequence* HitClip=Body?Body->GetOverlay():nullptr;
  if(SmokeStage==15){SmokeHead=Head;SmokeChest=Chest;U.HP-=1.f;SmokeStage=16;Later(.3f);return;}
  if(SmokeStage==16)
  {
   SmokeHit=A->LastHitReact;SmokeHitClip=HitClip;
   Check(Now-SmokeHit<.5 && HitClip!=nullptr,TEXT("a hit plays a hit reaction"));
   U.HP-=1.f;SmokeStage=17;Later(.1f);return;
  }
  if(SmokeStage==17)
  {
   Check(A->LastHitReact==SmokeHit,TEXT("a second hit 0.3 s later does not restart the reaction"));
   const float Length=SmokeHitClip?SmokeHitClip->GetPlayLength():1.f;
   SmokeStage=18;Later(FMath::Max(.1f,float(SmokeHit+Length+.3-Now)));return;
  }
  if(SmokeStage==18)
  {
   const float ChestTurn=FMath::RadiansToDegrees(Chest.AngularDistance(SmokeChest));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BODY_HIT_RECOVER clip=%s length=%.2f head_before=%.0f head_after=%.0f chest_turn=%.1f overlay_left=%d"),SmokeHitClip?*SmokeHitClip->GetName():TEXT("none"),SmokeHitClip?SmokeHitClip->GetPlayLength():0.f,SmokeHead,Head,ChestTurn,HitClip?1:0);
   Check(!HitClip && FMath::Abs(Head-SmokeHead)<4.f && ChestTurn<8.f,TEXT("a hit reaction ends and the body stands back up"));
   SmokeStage=19;Later(FMath::Max(.1f,float(SmokeHit+BattleBodyAnim::HitReactSeconds+.1-Now)));return;
  }
  if(SmokeStage==19 && SmokeHit>0.)
  {
   U.HP-=1.f;SmokeHit=-SmokeHit;Later(.2f);return;
  }
  Check(A->LastHitReact>-SmokeHit,TEXT("a hit after HitReactSeconds plays a new reaction"));
  U.HP+=3.f;
  CancelCoverAction(2);LastFireTimes.Add(2,Now);SmokeStage=20;Later(.6f);return;
 }
 // Rendered run: a near follower (one whole-body clip, no upper layer) that has just fired turns his mesh so the rifle points
 // along his facing, the shot's direction (UBattleLeaderRuntime::AimFollowerRifles).
 if(SmokeStage==20)
 {
  const auto& B=Bodies.FindChecked(2);
  const bool bNear=B.Mesh.IsValid() && B.Weapon.IsValid() && B.Mesh->IsComponentTickEnabled() && !(VAT&&VAT->IsFar(2));
  float Off=180.f;
  if(bNear)
  {
   const FVector Barrel=-B.Weapon->GetComponentTransform().GetUnitAxis(EAxis::Y);
   Off=FMath::Abs(FMath::FindDeltaAngleDegrees(Barrel.Rotation().Yaw,B.Capsule->GetForwardVector().Rotation().Yaw));
  }
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BODY_FOLLOWER_MUZZLE near=%d clip=%s mesh_turn=%.1f barrel_yaw_off=%.1f"),bNear?1:0,bNear&&B.Mesh->GetSingleNodeInstance()&&B.Mesh->GetSingleNodeInstance()->GetAnimationAsset()?*B.Mesh->GetSingleNodeInstance()->GetAnimationAsset()->GetName():TEXT("none"),B.MuzzleYaw,Off);
  Check(bNear && Off<6.f,TEXT("a near follower that fires points his rifle along his facing (mesh turned)"));
 }
 if(BodyShotPrefix().IsEmpty())CancelCoverAction(1);
 Check(Store->Find(1)->CoverPhase==0,TEXT("explicit release clears cover action"));
 IssueOrder(1,EBattleOrder::Move,Fixture(FVector(-1500,18000,0)));StepCoverAction(1,.1f,Now);
 Check(Store->Find(1)->CoverPhase==0,TEXT("new movement order retains movement authority"));
 CancelCoverAction(2);Check(Store->Find(2)->CoverPhase==0,TEXT("Mass release clears shared action"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
