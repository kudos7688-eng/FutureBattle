#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleGameMode.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunMixedFastSmoke()
{
 auto Check=[&](bool OK,const TCHAR* Label){if(!OK)++SmokeFailures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_FAST_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(SmokeStage==0)
 {
  Check(bMixedFastMatch&&Bodies.Num()==20&&Mode->Units.Num()==2&&Match->Phase==EBattleMatchPhase::Deployment,TEXT("initial two mixed squads in deployment"));
  Check(Match->Credits[0]==3000-Match->CurrentSquadPrice(EBattleTraining::Regular),TEXT("initial ten soldiers charged once"));
  Check(!Match->HireSquad(0,EBattleTraining::Veteran)&&Bodies.Num()==20,TEXT("insufficient funds retain canonical roster"));
  Match->Credits[0]=10000;const int32 Before=Match->Credits[0];
  Check(Match->HireSquad(0,EBattleTraining::Veteran)&&Bodies.Num()==30&&Mode->Units.Num()==3,TEXT("purchase adds one Actor and nine Mass"));
  Check(Match->Credits[0]==Before-Match->CurrentSquadPrice(EBattleTraining::Veteran)&&Store->Find(30)->Training==EBattleTraining::Veteran,TEXT("training and exact price applied"));
  Check(!Match->HireSquad(2,EBattleTraining::Regular)&&Bodies.Num()==30,TEXT("invalid team cannot mutate roster"));
  Check(Match->StartCombat(),TEXT("deployment enters combat"));
  auto* U=const_cast<FBattleUnitState*>(Store->Find(22));U->Reserve=0;
  const int32 Cash=Match->Credits[0];Check(Match->Resupply(22,10)==10&&Store->Find(22)->Reserve==10&&Match->Credits[0]==Cash-10,TEXT("Mass follower receives paid home resupply"));
  Store->ApplyDamage(21,1,100000);
  Check(!Store->Find(22)->IsAlive()&&Store->FindSquad(3)->CommandStatus==EBattleCommandStatus::Disabled,TEXT("purchased leader loss disables its whole squad"));
  Check(Match->HireSquad(0,EBattleTraining::Recruit)&&Bodies.Contains(40),TEXT("subsequent purchase allocates fresh IDs"));
  FString PurchasePrefix;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),PurchasePrefix);
  if(!PurchasePrefix.IsEmpty())FScreenshotRequest::RequestScreenshot(PurchasePrefix+TEXT("-purchase.png"),true,false);
  // Use the new squad's Mass follower during the existing six-second live interval.
  auto* ReloadFollower=const_cast<FBattleUnitState*>(Store->Find(32));
  ReloadFollower->Ammo=5;ReloadFollower->Reserve=200; // above the 90-round cap, so the home resupply tick adds nothing during the check
  ReloadFollower->ReloadEndTime=GetWorld()->GetTimeSeconds()+1.;
  ReloadFollower->NextShotTime=GetWorld()->GetTimeSeconds()+10.;
  FTimerHandle ReloadCheckTimer;
  GetWorld()->GetTimerManager().SetTimer(ReloadCheckTimer,FTimerDelegate::CreateWeakLambda(this,[this]()
  {
   auto* ReloadState=const_cast<FBattleUnitState*>(GetWorld()->GetSubsystem<UBattleState>()->Find(32));
   const bool ReloadOK=ReloadState&&ReloadState->IsAlive()&&!ReloadState->bActorRepresentation&&ReloadState->ReloadEndTime==0&&ReloadState->Ammo>5&&ReloadState->Reserve==200-(ReloadState->Ammo-5);
   if(!ReloadOK)++SmokeFailures;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_FAST_CHECK %s follower reload completes in real time"),ReloadOK?TEXT("PASS"):TEXT("FAIL"));
   if(!ReloadState)return;
   const int32 ReloadReserve=ReloadState->Reserve;
   // Leave room in the magazine so a duplicate completion cannot hide behind capacity.
   ReloadState->Ammo=5;
   FTimerHandle ReloadOnceTimer;
   GetWorld()->GetTimerManager().SetTimer(ReloadOnceTimer,FTimerDelegate::CreateWeakLambda(this,[this,ReloadReserve]()
   {
    const auto* ReloadAfter=GetWorld()->GetSubsystem<UBattleState>()->Find(32);
    const bool ReloadOnceOK=ReloadAfter&&ReloadAfter->IsAlive()&&ReloadAfter->ReloadEndTime==0&&ReloadAfter->Ammo==5&&ReloadAfter->Reserve==ReloadReserve;
    if(!ReloadOnceOK)++SmokeFailures;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_FAST_CHECK %s follower reload does not replenish twice"),ReloadOnceOK?TEXT("PASS"):TEXT("FAIL"));
   }),2.f,false);
  }),2.f,false);
  Mode->bCombatPaused=false;SmokeStage=1;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunMixedFastSmoke,6.f,false);return;
 }
 Check(Store->GetUnits().Num()>=50,TEXT("enemy director purchases mixed reinforcements"));
 Match->Elapsed=600;Match->Score[0]=10;Match->Score[1]=5;Match->CheckTimeLimit();Check(Match->Winner==0&&Mode->bBattleEnded,TEXT("mixed economy reaches timed victory"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_FAST_RESULT failures=%d"),SmokeFailures);FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
