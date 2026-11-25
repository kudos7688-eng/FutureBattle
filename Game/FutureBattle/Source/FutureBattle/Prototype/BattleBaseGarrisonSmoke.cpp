#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleTactics.h"
#include "BattleGameMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FutureBattle.h"

// Fixed scenes only: positions and casualties are controlled without running ordinary combat between checks.
void UBattleLeaderRuntime::StepBaseGarrisonSmoke(double Now)
{
 if(bBaseGarrisonSmokeDone||Now<Mode->CombatStartsAt+3.)return;
 bBaseGarrisonSmokeDone=true;Mode->bCombatPaused=false;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 int32 Failures=0;
 auto Check=[&](bool OK,const TCHAR* Label){Failures+=!OK;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON_SMOKE %s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto End=[&](){SmokeFailures+=Failures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON_SMOKE_RESULT failures=%d"),Failures);FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);};
 auto Place=[&](int32 ID,const FVector& At)
 {
  auto* Body=Bodies.Find(ID);if(!Body)return;
  if(Body->Actor.IsValid()){Body->Actor->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);Body->Actor->GetCharacterMovement()->StopMovementImmediately();}
  else{Body->Capsule->SetWorldLocation(At);Manager->GetFragmentDataChecked<FBattleFollowerFragment>(Body->Entity).Position=At;}
 };
 TArray<int32> Guards[2];int32 Ordinary[2]={0,0};int32 EnemyFoot=INDEX_NONE;
 for(const auto& Entry:Store->GetSquads())
 {
  const auto& Squad=Entry.Value;
  if(Match->IsBaseGarrison(Entry.Key))Guards[Squad.Team].Add(Entry.Key);
  else
  {
   Ordinary[Squad.Team]+=Squad.CommandStatus==EBattleCommandStatus::Active;
   if(Squad.Team==1&&!BattleSquadType::IsVehicle(Squad.SquadType)&&Store->Find(Squad.LeaderUnitID)->IsAlive())EnemyFoot=Squad.LeaderUnitID;
  }
 }
 Check(Match->Area.bGarrisonReady&&Guards[0].Num()==2&&Guards[1].Num()==2,TEXT("two squads per team"));
 if(Guards[0].Num()!=2||Guards[1].Num()!=2||EnemyFoot==INDEX_NONE){Check(false,TEXT("scene prerequisites"));End();return;}
 for(int32 Team=0;Team<2;++Team)
 {
  Check(Ordinary[Team]==Match->StartSquads()&&Match->ActiveSquads(Team)==Ordinary[Team],TEXT("active squads equal switch-off opening roster"));
  Check(BattleTactics::AreaHire().MaxActive-Match->ActiveSquads(Team)==BattleTactics::AreaHire().MaxActive-Match->StartSquads(),TEXT("free active slots equal switch-off opening"));
  Check(Match->Area.HireSpent[Team]==0&&Match->Area.LastAIHireAt[Team]==-1000,TEXT("deployment does not purchase or advance hire clock"));
 }
 const FVector Home=Match->AreaHome(0),Outside(0,0,100);const float Radius=BattleTactics::BaseGarrisonRadius();
 const int32 GuardSquad=Guards[0][0];bool Clamped=false;
 for(int32 Direction=0;Direction<16&&!Clamped;++Direction)
 {
  const FVector Asked=Home+FVector(FMath::Cos(Direction*PI/8),FMath::Sin(Direction*PI/8),0)*(Radius*2);
  if(IssueOrder(GuardSquad,EBattleOrder::Defend,Asked))Clamped=FVector::Dist2D(Store->FindSquad(GuardSquad)->Destination,Home)<=Radius;
 }
 Check(Clamped,TEXT("outside order clamps to navigable point inside base"));
 const auto* GuardState=Store->FindSquad(GuardSquad);const FVector Manual=GuardState->MissionDestination;
 Match->UpdateTeamOrders(0);Check(Store->FindSquad(GuardSquad)->MissionDestination.Equals(Manual),TEXT("player order survives allocation"));
 Check(!Match->AssignArmy(GuardSquad,1)&&Match->FormGroup(0,{GuardSquad},true)==INDEX_NONE,TEXT("manual groups reject garrison"));
 // Keep one living guard out of the circle until the pause test, and kill the others through normal death handling.
 // The leader: a squad whose leader dies is lost with all its members (UBattleState::DisableLeaderSquad).
 const int32 LastGuard=Store->FindSquad(GuardSquad)->LeaderUnitID;
 for(const auto& Entry:Store->GetUnits())if(Entry.Value.IsAlive())Place(Entry.Key,Outside);
 TArray<int32> GuardMembers;for(int32 GuardID:Guards[0])GuardMembers.Append(Store->FindSquad(GuardID)->Members);
 for(int32 Member:GuardMembers)if(Member!=LastGuard)Store->ApplyDamage(Member,1,100000.f);
 Match->Area.GarrisonHeld={0,0};Match->UpdateArea(.25f);
 Check(Match->Phase==EBattleMatchPhase::Combat&&Match->Area.GarrisonHeld[0]==0,TEXT("empty base without enemy does not end match"));
 Place(EnemyFoot,Home+FVector(0,0,100));Match->UpdateArea(.25f);
 Check(Match->Area.GarrisonHeld[0]>0,TEXT("enemy infantry advances occupation clock"));
 const float Held=Match->Area.GarrisonHeld[0];Place(LastGuard,Home+FVector(0,0,100));Match->UpdateArea(.25f);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_BASE_GARRISON_SMOKE_INFO guard=%d alive=%d dist=%.0f inside=%d all=%d held=%.2f->%.2f status=%d"),LastGuard,Store->Find(LastGuard)&&Store->Find(LastGuard)->IsAlive()?1:0,FVector::Dist2D(Position(LastGuard),Home),Match->GarrisonLiving(0,true),Match->GarrisonLiving(0,false),Held,Match->Area.GarrisonHeld[0],int32(Store->FindSquad(GuardSquad)->CommandStatus));
 Check(Match->Area.GarrisonHeld[0]==Held,TEXT("one living guard pauses and retains clock"));
 Place(EnemyFoot,Outside);Match->UpdateArea(.25f);
 Check(Match->Area.GarrisonHeld[0]==0,TEXT("enemy departure resets clock even with guard present"));
 Store->ApplyDamage(LastGuard,1,100000.f);Match->UpdateArea(.25f);
 Check(Match->GarrisonLiving(0)==0&&Match->Phase==EBattleMatchPhase::Combat,TEXT("garrison elimination alone does not end match"));
 FString Error;int32 VehicleSquad=INDEX_NONE;
 const bool VehicleOK=SpawnPurchasedSquad(1,EBattleTraining::Regular,Error,false,&VehicleSquad,BattleSquadType::Tank);
 Check(VehicleOK,TEXT("vehicle scene spawned"));
 if(VehicleOK)
 {
  const int32 ExpectedSlot=BattleTactics::SymmetricStart(GetWorld())?Match->StartSquads()+1:Match->StartSquadCount()+1;
  Check(BattleTactics::AllocationIndex(GetWorld(),VehicleSquad,1)==ExpectedSlot,TEXT("later squad allocation ignores four garrisons"));
  const TArray<int32> Crew=Store->FindSquad(VehicleSquad)->Members;
  for(int32 Member:Crew)Place(Member,Home+FVector(0,0,100));
  Match->UpdateArea(.25f);Check(Match->Area.GarrisonHeld[0]==0,TEXT("vehicle alone cannot occupy"));
  for(int32 Member:Crew)Place(Member,Outside);
 }
 Place(EnemyFoot,Home+FVector(0,0,100));
 Match->Area.GarrisonHeld[0]=FMath::Max(0.f,BattleTactics::BaseGarrisonSeconds()-.125f);
 const int32 CreditsBefore=Match->Credits[0];const float IncomeBefore=Match->Area.IncomeClock;
 Match->Update(.25f);
 Check(Match->Phase==EBattleMatchPhase::Finished&&Match->Winner==1&&Mode->bBattleEnded,TEXT("full clock ends match with occupying team winning"));
 Check(Match->Credits[0]==CreditsBefore&&Match->Area.IncomeClock==IncomeBefore,TEXT("occupation finish skips remaining area update"));
 End();
}
