#include "BattlePhase3.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Serialization/MemoryWriter.h"
#include "FutureBattle.h"

// Test-only synchronous mixed Actor/Mass fixture. Never used for performance.
void UBattlePhase3::VerifyGrid()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 TMap<int32,FBattlePerceptionProfile> SavedProfiles;
 TMap<int32,FTransform> SavedTransforms;
 TMap<int32,FVector> SavedForward;
 for(const auto& P:Runtime->Bodies)
 {
  SavedProfiles.Add(P.Key,Store->Find(P.Key)->Perception);
  FBattlePerceptionProfile Zero; Zero.SightRange=0; Zero.ShareRange=0;
  Store->SetPerceptionProfile(P.Key,Zero);
 }
 const TArray<int32> IDs={1,2,Runtime->Bodies.Num()/2+1,Runtime->Bodies.Num()/2+2};
 for(int32 ID:IDs)
 {
  auto& B=Runtime->Bodies.FindChecked(ID);
  SavedTransforms.Add(ID,B.Actor.IsValid()?B.Actor->GetActorTransform():B.Capsule->GetComponentTransform());
  SavedForward.Add(ID,Runtime->Forward(ID));
 }
 auto Move=[&](int32 ID,FVector P,FRotator R)
 {
  auto& B=Runtime->Bodies.FindChecked(ID);
  if(B.Actor.IsValid()) { B.Actor->SetActorLocationAndRotation(P,R,false,nullptr,ETeleportType::TeleportPhysics); }
  else
  {
   B.Capsule->SetWorldLocationAndRotation(P,R,false,nullptr,ETeleportType::TeleportPhysics);
   auto& F=Runtime->Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity); F.Position=P; F.Forward=R.Vector();
  }
 };
 auto Bytes=[&](UBattlePerception* S,double Now)
 {
  TMap<int32,FBattleObserver> Observers; TArray<FBattleReport> Reports; S->Capture(Observers,Reports,Now);
  TArray<uint8> Out; FMemoryWriter Ar(Out); TArray<int32> Keys; Observers.GetKeys(Keys); Ar<<Keys;
  for(int32 ID:Keys) FBattleObserver::StaticStruct()->SerializeItem(Ar,&Observers.FindChecked(ID),nullptr);
  int32 N=Reports.Num(); Ar<<N; for(auto& Report:Reports) FBattleReport::StaticStruct()->SerializeItem(Ar,&Report,nullptr);
  Ar<<S->NextScanTimes<<S->LastScanTimes<<S->PendingCounts;
  return Out;
 };
 int32 Checks=0,Mismatches=0,Direct=0,Shared=0;
 for(int32 Scenario=0;Scenario<3;++Scenario)
 {
  auto* Reference=NewObject<UBattlePerception>(GetWorld()); Reference->bReuseSightContext=true;
  auto* Grid=NewObject<UBattlePerception>(GetWorld()); Grid->bReuseSightContext=true; Grid->bUseSpatialGrid=true;
  AActor* Cover=nullptr;
  if(Scenario==2)
  {
   Cover=GetWorld()->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Cover); Cover->SetRootComponent(Box); Cover->AddInstanceComponent(Box);
   Box->SetBoxExtent(FVector(50,500,200)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Cover->SetActorLocation(FVector(-10800,-10800,150));
  }
  for(int32 Step=0;Step<160;++Step)
  {
   if(Step%40==0) for(int32 I=0;I<4;++I)
   {
    FBattlePerceptionProfile P; P.SightRange=Step==40?0.f:Step==80?20000.f:2400.f;
    P.FieldOfView=I==1?360.f:120.f; P.DetectionSeconds=.3f; P.MemorySeconds=2.f; P.ShareRange=1600.f; P.ShareDelay=.4f;
    Store->SetPerceptionProfile(IDs[I],P);
   }
   for(int32 I=0;I<4;++I)
   {
    const float Spread=Scenario==0?1800.f:200.f;
    const float X=-12000.f+(I>=2?2399.f+Step%5:0.f)+(Step%20)*61.f;
    Move(IDs[I],FVector(X,-10800.f+(I%2)*Spread,95+Step%3),FRotator(0,I>=2?180.f:Step%30<20?0.f:180.f,0));
   }
   if(Cover && Step==60) Cover->SetActorLocation(FVector(-10800,-8000,150));
   const double Now=1000.+Scenario*20.+Step*.05;
   Reference->Update(Mode->Units,Now,Step%4,4); Grid->Update(Mode->Units,Now,Step%4,4);
   ++Checks; if(Bytes(Reference,Now)!=Bytes(Grid,Now)) ++Mismatches;
   for(const auto& O:Reference->Observers) for(const auto& C:O.Value.Contacts) {Direct+=C.Value.bVisible; Shared+=C.Value.bShared;}
  }
  if(Cover) Cover->Destroy();
 }
 for(int32 ID:IDs) { const auto& T=SavedTransforms.FindChecked(ID); Move(ID,T.GetLocation(),T.Rotator()); auto& B=Runtime->Bodies.FindChecked(ID); if(!B.Actor.IsValid()) Runtime->Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=SavedForward.FindChecked(ID); }
 for(const auto& P:SavedProfiles) Store->SetPerceptionProfile(P.Key,P.Value);
 if(Mismatches || !Direct || !Shared) ++Failures;
 UE_LOG(LogFutureBattle,Display,TEXT("P3_GRID_FIXTURE checks=%d mismatches=%d direct_samples=%d shared_samples=%d"),Checks,Mismatches,Direct,Shared);
}
