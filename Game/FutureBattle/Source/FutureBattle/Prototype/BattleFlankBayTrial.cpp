#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleController.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::RunFlankBayTrial()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 int32 Flank=0,Sign=1;FParse::Value(FCommandLine::Get(),TEXT("BayFlank="),Flank);FParse::Value(FCommandLine::Get(),TEXT("BaySign="),Sign);Sign=Sign<0?-1:1;
 const FVector Origin(0,Sign*16000,0),Goal=Origin+FVector(0,2000*Sign,0);
 auto Later=[&](float Delay){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunFlankBayTrial,Delay,false);};
 auto HP=[&](int32 SID){double Sum=0;for(int32 ID:Store->FindSquad(SID)->Members)Sum+=Store->Find(ID)->HP;return Sum;};
 if(BayStage==0)
 {
  if(!GetWorld()->GetMapName().EndsWith(TEXT("Lvl_BattleWide_C"))){FPlatformMisc::RequestExitWithStatus(false,1);return;}
  Match->DirectorClock=-1000;Match->DelegationClock=-1000;Mode->CombatStartsAt=0;
  for(const auto& Pair:Store->GetSquads())IssueOrder(Pair.Key,EBattleOrder::Hold,Position(Pair.Value.LeaderUnitID));
  for(const auto& Pair:Store->GetUnits())
  {
   auto& U=*const_cast<FBattleUnitState*>(Store->Find(Pair.Key));
   if(U.SquadID!=1 && U.SquadID!=2 && U.SquadID!=26){U.Ammo=U.Reserve=0;auto P=U.Perception;P.SightRange=0;P.ShareRange=0;Store->SetPerceptionProfile(U.UnitID,P);continue;}
   const int32 I=Store->FindSquad(U.SquadID)->Members.IndexOfByKey(U.UnitID);
   const FVector P=Origin+FVector((U.Team?100+(I/5)*120:(U.SquadID==1?-3000:-3400)-(I/5)*120)*Sign,(I%5-2)*100,100);
   auto& B=Bodies.FindChecked(U.UnitID);
   const FVector Facing(U.Team?-Sign:Sign,0,0);
   if(B.Actor.IsValid()){B.Actor->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->SetActorRotation(Facing.Rotation());}
   else {B.Capsule->SetWorldLocationAndRotation(P,Facing.Rotation());auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=P;F.Forward=Facing;}
   Store->SetTrainingPreset(U.UnitID,EBattleTraining::Regular);
  }
  for(int32 SID:{1,2,26})IssueOrder(SID,EBattleOrder::Hold,Position(Store->FindSquad(SID)->LeaderUnitID));
  if(Flank && !IssueOrder(2,EBattleOrder::Move,Goal)){FPlatformMisc::RequestExitWithStatus(false,2);return;}
  GetWorld()->GetSubsystem<UBattlePerception>()->Reset();
  if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())){PC->EnterTactical();if(auto* C=Cast<ABattleCameraPawn>(PC->GetPawn())){C->SetActorLocation(Origin+FVector(-1200*Sign,800*Sign,0));C->Zoom(7000-C->Camera->GetRelativeLocation().Z);}}
  BayStart=GetWorld()->GetTimeSeconds();BayLastHP=HP(2);BayStage=1;Mode->bCombatPaused=false;Later(.1f);return;
 }
 const double Elapsed=GetWorld()->GetTimeSeconds()-BayStart;
 if(BayStage==2){FPlatformMisc::RequestExitWithStatus(false,0);return;}
 if(Flank && BayArrival<0)
 {
  BayMovingDamage+=FMath::Max(0.,BayLastHP-HP(2));BayLastHP=HP(2);
  const auto* S=Store->FindSquad(2);int32 Living=0,Arrived=0;
  for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive()){++Living;Arrived+=FVector::Dist2D(Position(ID),Goal)<500;}
  if(S->CommandStatus==EBattleCommandStatus::Active && FVector::Dist2D(Position(S->LeaderUnitID),Goal)<300 && Arrived>=FMath::CeilToInt(Living*.7f))
  {BayArrival=Elapsed;IssueOrder(2,EBattleOrder::Hold,Position(S->LeaderUnitID));}
 }
 if(Elapsed<45){Later(.1f);return;}
 Mode->bCombatPaused=true;
 auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("flank"),Flank);Root->SetNumberField(TEXT("sign"),Sign);Root->SetNumberField(TEXT("seconds"),Elapsed);
 Root->SetNumberField(TEXT("arrival_seconds"),BayArrival);Root->SetNumberField(TEXT("moving_squad_hp_loss"),BayMovingDamage);
 Root->SetNumberField(TEXT("attackers_hp_loss"),2000-HP(1)-HP(2));Root->SetNumberField(TEXT("defenders_hp_loss"),1000-HP(26));
 Root->SetNumberField(TEXT("attacker_direct_damage"),BayDamageGiven[0]);Root->SetNumberField(TEXT("defender_direct_damage"),BayDamageGiven[1]);Root->SetNumberField(TEXT("moving_direct_damage"),Flank?BayMoveDirectDamage:0);
 // HP loss includes the existing leader-death squad shutdown. Shots are recorded separately.
 for(int32 SID:{1,2,26}){int32 Shots=0;for(int32 ID:Store->FindSquad(SID)->Members)Shots+=Store->Find(ID)->ShotSequence;Root->SetNumberField(FString::Printf(TEXT("squad_%d_shots"),SID),Shots);}
 FString Out,JSON;FParse::Value(FCommandLine::Get(),TEXT("BayOutput="),Out);FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&JSON));
 const bool Saved=!Out.IsEmpty()&&FFileHelper::SaveStringToFile(JSON,*Out);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BAY_RESULT failures=%d flank=%d arrived=%.2f attack_loss=%.0f defense_loss=%.0f"),Saved?0:1,Flank,BayArrival,2000-HP(1)-HP(2),1000-HP(26));
 if(!Saved){FPlatformMisc::RequestExitWithStatus(false,3);return;}
 if(FParse::Param(FCommandLine::Get(),TEXT("BayRendered")))FScreenshotRequest::RequestScreenshot(Out+TEXT(".png"),true,false);
 BayStage=2;Later(1.f);
}
