#include "BattlePhase3.h"
#include "BattleLeaderRuntime.h"
#include "BattleMixedDriver.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FutureBattle.h"

void UBattlePhase3::Verify()
{
 auto Check=[&](bool OK,const TCHAR* Name){ if(!OK) ++Failures; UE_LOG(LogFutureBattle,Display,TEXT("P3_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Name); };
 const int32 InitialFailures=Failures;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 TSet<int32> Unique; for(const auto& P:AvoidanceIDs) Unique.Add(P.Value);
 Check(Unique.Num()==Runtime->Bodies.Num(),TEXT("unique RVO IDs for every Actor and Mass entity"));
 bool ActorsOK=true; for(const auto& P:Runtime->Bodies) if(P.Value.Actor.IsValid()) ActorsOK &= !P.Value.Actor->GetCharacterMovement()->bUseRVOAvoidance;
 Check(ActorsOK,TEXT("no second CharacterMovement RVO solver"));
 auto* Test=NewObject<UBattleMixedAvoidance>(GetWorld());
 auto Data=[&](FVector Pos,FVector V,float Weight=.5f){FNavAvoidanceData D; D.Center=Pos;D.Velocity=V;D.Radius=36*Test->ArtificialRadiusExpansion;D.HalfHeight=90;D.Weight=Weight;D.OverrideWeightTime=0;D.RemainingTimeToLive=Test->DefaultTimeToLive;D.GroupMask=1;D.GroupsToAvoid=-1;D.GroupsToIgnore=0;D.TestRadius2D=350;return D;};
 auto A=Data(FVector(-120,0,90),FVector(420,0,0)),B=Data(FVector(120,0,90),FVector(-420,0,0));
 Test->Publish(1,A); Test->Publish(2,B);
 const FVector VA=Test->GetAvoidanceVelocityIgnoringUID(A,Test->DeltaTimeToPredict,1),VB=Test->GetAvoidanceVelocityIgnoringUID(B,Test->DeltaTimeToPredict,2);
 Check(!VA.ContainsNaN() && !VB.ContainsNaN() && !VA.Equals(A.Velocity,1) && !VB.Equals(B.Velocity,1),TEXT("head-on mutual RVO changes both velocities"));
 B=Data(FVector(120,0,90),FVector::ZeroVector,1); Test->Publish(2,B);
 Check(!Test->GetAvoidanceVelocityIgnoringUID(A,Test->DeltaTimeToPredict,1).Equals(A.Velocity,1),TEXT("moving agent avoids stationary living agent"));
 B=Data(FVector(0,-120,90),FVector(0,420,0)); Test->Publish(2,B);
 Check(!Test->GetAvoidanceVelocityIgnoringUID(A,Test->DeltaTimeToPredict,1).Equals(A.Velocity,1),TEXT("crossing trajectories detected"));
 Test->RemoveAvoidanceObject(2);
 Check(Test->GetAvoidanceObjectForUID(2)->ShouldBeIgnored() && Test->GetAvoidanceVelocityIgnoringUID(A,Test->DeltaTimeToPredict,1).Equals(A.Velocity,1),TEXT("removed dead agent no longer influences avoidance"));
 // Directly exercise the ported legacy decision engine, without perception fabrication.
 const int32 SID=Store->Find(2)->SquadID; const FVector OriginalGoal=Store->FindSquad(SID)->Destination;
 const double OriginalStart=Mode->CombatStartsAt; Mode->CombatStartsAt=-1;
 Runtime->IssueOrder(SID,EBattleOrder::Hold,OriginalGoal);
 Drivers[2]->Tick(.016f);
 Check(!Paths.Contains(2) && Store->Find(2)->Action==EBattleAction::Idle,TEXT("individual Hold produces no path and no unseen target"));
 const int32 Ammo=Store->Find(2)->Ammo; Drivers[2]->NextDecision=0; Drivers[2]->NextControllerTick=0; Drivers[2]->Tick(.016f);
 Check(Store->Find(2)->Ammo==Ammo,TEXT("individual unknown targets never consume ammunition"));
 Runtime->IssueOrder(SID,EBattleOrder::Move,OriginalGoal); Drivers[2]->NextControllerTick=0; Drivers[2]->Tick(.016f);
 Check(Paths.Contains(2) && Paths[2].Num()>1 && Store->Find(2)->Action==EBattleAction::Moving,TEXT("individual Move requests full NavMesh route"));
 TArray<FVector> Invalid;
 Check(!Path(2,FVector(1000000,1000000,0),Invalid),TEXT("unreachable goal rejected without partial-path success"));
 Check(Drivers[2]->NextDecision>GetWorld()->GetTimeSeconds(),TEXT("individual original decision cadence retained"));
 Runtime->IssueOrder(SID,EBattleOrder::AttackMove,OriginalGoal); Drivers[2]->ResetExecution(); Paths.Reset();
 Mode->CombatStartsAt=OriginalStart;
 ApplyLOD(); bool LODOK=true;
 for(const auto& Pair:Runtime->Bodies) { auto* Mesh=Pair.Value.Actor.IsValid()?Pair.Value.Actor->GetMesh():Pair.Value.Mesh.Get(); LODOK &= Mesh->GetForcedLOD()==LODs[Pair.Key]+1; if(Store->Find(Pair.Key)->bCanPlayerControl) LODOK &= LODs[Pair.Key]==0; }
 Check(LODOK,TEXT("applied mesh LOD agrees with policy and controllable Actor LOD0"));
 Check(InitialFailures==0,TEXT("all initial spawn and destination NavMesh routes valid"));
 // Setup checks have no contribution to the timed counters.
 PathRequests=PathFailures=Decisions=Shots=Deaths=RVOChanges=0;
}

void UBattlePhase3::VerifyCombat()
{
 auto Check=[&](bool OK,const TCHAR* Name){if(!OK) ++Failures;UE_LOG(LogFutureBattle,Display,TEXT("P3_CHECK %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Name);};
 auto* Store=GetWorld()->GetSubsystem<UBattleState>(); auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 auto Place=[&](int32 ID,FVector P,FVector D){auto& B=Runtime->Bodies.FindChecked(ID); if(B.Actor.IsValid()){B.Actor->SetActorLocation(P);B.Actor->GetCharacterMovement()->StopMovementImmediately();}else{B.Capsule->SetWorldLocation(P);Runtime->Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Position=P;B.Velocity=FVector::ZeroVector;} Rotate(ID,D.Rotation());};
 Check(Runtime->Bodies.Num()==40,TEXT("functional fixture has 40 units")); if(Runtime->Bodies.Num()!=40) return;
 for(int32 ID:{1,2,21,22}) { const bool Ally=ID<20; Place(ID,FVector(Ally?-9000:-8600,(ID%10)==1?-7000:-7800,95),FVector(Ally?1:-1,0,0)); }
 const double Now=GetWorld()->GetTimeSeconds(); Mode->bBattleEnded=false; Mode->CombatStartsAt=-1;
 Runtime->IssueOrder(1,EBattleOrder::AttackMove,FVector(-8000,-7400,0));
 Sense->Reset(); Sense->Update(Mode->Units,Now); Sense->Update(Mode->Units,Now+.7);
 Check(Runtime->CanSee(1,21) && Runtime->CanSee(2,22),TEXT("Actor and Mass physical targets visible in paired fixture"));
 const float HPA=Store->Find(21)->HP,HPM=Store->Find(22)->HP; const int32 AA=Store->Find(1)->Ammo,AM=Store->Find(2)->Ammo;
 // Run the unmodified legacy Actor controller on 1, and the mixed value adapter on 2.
 // Only this terminating functional fixture temporarily disables the opt-in delegation.
 Runtime->bEnabled=false;
 auto* Legacy=Cast<ABattleAIController>(Runtime->Actor(1)->SavedAI);
 if(Legacy) Legacy->Tick(.05f);
 Runtime->bEnabled=true;
 Check(Legacy!=nullptr,TEXT("real legacy Actor controller available for parity check"));
 Drivers[2]->ResetExecution(); Drivers[2]->NextControllerTick=0; Drivers[2]->Tick(.016f);
 Check(Store->Find(1)->Ammo==AA-1 && Store->Find(2)->Ammo==AM-1,TEXT("legacy Actor and mixed Mass decision both consume one shot"));
 Check(Store->Find(21)->HP<HPA && Store->Find(22)->HP<HPM,TEXT("Actor and Mass use actual common damage path"));
 Check(Store->Find(1)->Action==EBattleAction::Engaging && Store->Find(2)->Action==EBattleAction::Engaging,TEXT("legacy Actor and mixed Mass engagement action agrees"));
 auto* Wall=NewObject<UBoxComponent>(Runtime->DisplayHost);Runtime->DisplayHost->AddInstanceComponent(Wall);Wall->SetBoxExtent(FVector(25,100,200));Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Wall->SetCollisionResponseToAllChannels(ECR_Block);Wall->RegisterComponent();Wall->SetWorldLocation(FVector(-8800,-7800,100));
 Check(!Runtime->CanSee(2,22),TEXT("individual mixed execution cannot see through geometry"));
 Sense->Update(Mode->Units,Now+.9); const int32 Before=Store->Find(2)->Ammo; Drivers[2]->NextControllerTick=Drivers[2]->NextDecision=0;Drivers[2]->Tick(.016f);
 Check(Store->Find(2)->Ammo==Before,TEXT("remembered blocked contact cannot fire"));Wall->DestroyComponent();
 auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());Check(PC->ControlUnit(Runtime->Actor(1)),TEXT("functional commander possession"));
 const int32 BeforeRVO=AvoidanceIDs.Num();Store->ApplyDamage(1,1,100000);
 Check(AvoidanceIDs.Num()==BeforeRVO-10 && !AvoidanceIDs.Contains(2),TEXT("commander loss removes all ten RVO participants"));
 Check(PC->IsTactical() && PC->GetSelection().IsEmpty(),TEXT("mixed runtime death returns RTS without transfer"));
 Check(!Runtime->Fire(2,Runtime->Position(2),FVector::ForwardVector,false),TEXT("disabled follower cannot shoot"));
}
