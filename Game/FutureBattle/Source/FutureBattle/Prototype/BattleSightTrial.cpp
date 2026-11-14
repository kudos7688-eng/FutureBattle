#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattlePerception.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "FutureBattle.h"

// Diagnostic fixture: retain 500 bodies, isolate one observer and one target.
// Use real perception, planning and execution; do not inject a known contact.
void UBattleLeaderRuntime::RunSightTrial()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 auto Later=[&](){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,this,&UBattleLeaderRuntime::RunSightTrial,.1f,false);};
 const float Distances[]={4000,7500,10000,15000};
 const int32 Observer=SightTrialCase<4?1:252, Target=SightTrialCase<4?252:1;
 const int32 SID=Store->Find(Observer)->SquadID;
 auto Place=[&](int32 ID,FVector P,FVector Facing)
 {
  auto& B=Bodies.FindChecked(ID);
  if(B.Actor.IsValid()){B.Actor->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);B.Actor->SetActorRotation(Facing.Rotation());B.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else {B.Capsule->SetWorldLocation(P);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=P;F.Forward=Facing;}
  B.Velocity=FVector::ZeroVector;
 };
 if(SightTrialTick==0)
 {
  Mode->bCombatPaused=true;Mode->CombatStartsAt=0;
  for(const auto& Pair:Store->GetUnits())
  {
   Place(Pair.Key,FVector(7000+(Pair.Key%20)*110,2000+(Pair.Key/20)*110,100),FVector(1,0,0));
   auto P=Pair.Value.Perception;P.SightRange=0;P.ShareRange=0;Store->SetPerceptionProfile(Pair.Key,P);
  }
  for(const auto& Pair:Store->GetSquads()) IssueOrder(Pair.Key,EBattleOrder::Hold,Position(Pair.Value.LeaderUnitID));
  Store->SetTrainingPreset(Observer,EBattleTraining::Regular);
  // Check training scaling independently for both representations/teams.
  const float Base=Store->Find(Observer)->Perception.SightRange;
  for(auto Training:{EBattleTraining::Recruit,EBattleTraining::Veteran,EBattleTraining::Regular})
  {
   Store->SetTrainingPreset(Observer,Training);
   const float Expected=Base*(Training==EBattleTraining::Recruit?.75f:Training==EBattleTraining::Veteran?1.25f:1.f);
   if(!FMath::IsNearlyEqual(Store->Find(Observer)->Perception.SightRange,Expected)) ++SmokeFailures;
  }
  auto P=Store->Find(Observer)->Perception;P.ShareRange=0;Store->SetPerceptionProfile(Observer,P);
  SightTrialStart=FVector(-9500,-8000,100);
  Place(Observer,SightTrialStart,FVector(1,0,0));
  Place(Target,SightTrialStart+FVector(Distances[SightTrialCase%4],0,0),FVector(-1,0,0));
  // Only this squad's executor is stepped; peers retain collision but cannot interfere.
  IssueOrder(SID,EBattleOrder::AttackMove,FVector(10000,-8000,0));
  Plans.FindChecked(SID).TargetID=INDEX_NONE;
  Sense->Reset();
  auto& U=const_cast<FBattleUnitState&>(*Store->Find(Observer));U.Ammo=100;U.Reserve=0;U.ReloadEndTime=0;
  U.NextShotTime=0;
  auto& T=const_cast<FBattleUnitState&>(*Store->Find(Target));T.HP=100000;
  SightTrialAmmo=U.Ammo;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_BEGIN case=%d observer=%d target=%d sight_cm=%.1f distance_cm=%.1f"),SightTrialCase,Observer,Target,Base,Distances[SightTrialCase%4]);
 }
 const double Now=GetWorld()->GetTimeSeconds();
 Sense->Update(Mode->Units,Now);
 Think(SID,Now);
 // A fixed execution step isolates decisions from wall-clock frame variation.
 Follow(Observer,.1f,Now);
 ++SightTrialTick;
 if(SightTrialTick<40){Later();return;}
 const auto* Contact=Sense->FindContact(Observer,Target);
 const bool Seen=Contact && Contact->bVisible && !Contact->bShared;
 const int32 Spent=SightTrialAmmo-Store->Find(Observer)->Ammo;
 const double Travel=FVector::Dist2D(Position(Observer),SightTrialStart);
 const double Distance=FVector::Dist(Position(Observer),Position(Target));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_CASE case=%d observer=%d representation=%s team=%d initial_cm=%.1f current_cm=%.1f sight_cm=%.1f visible=%d rounds=%d travel_cm=%.2f target_hp=%.1f"),SightTrialCase,Observer,Actor(Observer)?TEXT("Actor"):TEXT("Mass"),Store->Find(Observer)->Team,Distances[SightTrialCase%4],Distance,Store->Find(Observer)->Perception.SightRange,Seen,Spent,Travel,Store->Find(Target)->HP);
 // The fixture reports legacy out-of-range fire, rather than declaring it correct.
 if(Distances[SightTrialCase%4]==4000 && (!Seen || Spent==0)) ++SmokeFailures;
 if(bRangeAwareCombat && Distance>GetDefault<UBattleTuning>()->EffectiveWeaponRange() && Spent>0) ++SmokeFailures;
 if(++SightTrialCase<8){SightTrialTick=0;Later();return;}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_RESULT failures=%d cases=8"),SmokeFailures);
 FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);
}
