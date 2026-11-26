// Supply vehicle (squad type 9; 2026-11-20, user decision: the 3-soldier supply team is replaced by one vehicle, so it is limited to
// where vehicles go and becomes a target). It carries UBattleTuning::SupplyCarStock rounds. Own squads whose leader stays within
// SupplyTeamRadius of it get the home ammunition refill (UBattleMatch::UpdateArea) and the rounds given leave its stock; at home it
// loads again. An AI-run vehicle drives home while low, keeps behind its group's post (group role 5) or behind the own point nearest
// the enemy home. A squad short of ammunition drives to a vehicle with stock within FieldSupplyRange instead of going home
// (BattleSquadRetreat.cpp, "FIELD SUPPLY"). Inputs: own squads' state and positions, observed ownership, known enemies (AreaArmourThreat).
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
 // P moved Back toward the own home, then further back in 20 m steps (up to 6) while a known anti-tank soldier reaches it.
 FVector SupplyBack(const UBattleMatch& M,int32 Team,const FVector& P,double Back)
 {
  const FVector Home=M.AreaHome(Team);const FVector Dir=(Home-P).GetSafeNormal2D();const double Length=FVector::Dist2D(Home,P);
  double Step=FMath::Clamp(Back,0.,FMath::Max(0.,Length-3000.));
  for(int32 I=0;I<6&&Step+2000.<Length&&M.AreaArmourThreat(Team,P+Dir*Step,false);++I)Step+=2000.;
  return P+Dir*Step;
 }
}

int32 UBattleMatch::SupplyStockMax() const { return FMath::Clamp(GetDefault<UBattleTuning>()->SupplyCarStock,0,100000); }
int32 UBattleMatch::SupplyStockOf(int32 SquadID) const { const int32* S=Area.SupplyStock.Find(SquadID); return S?*S:SupplyStockMax(); }
void UBattleMatch::SpendSupply(int32 SquadID,int32 Rounds) { Area.SupplyStock.Add(SquadID,FMath::Max(0,SupplyStockOf(SquadID)-FMath::Max(0,Rounds))); }
void UBattleMatch::RestockSupplyCar(int32 SquadID)
{
 if(!Area.SupplyStock.Contains(SquadID))return;
 UE_LOG(LogFutureBattle,Display,TEXT("AREA_SUPPLY_RESTOCK time=%.0f squad=%d stock=%d"),Elapsed,SquadID,Area.SupplyStock[SquadID]);
 Area.SupplyStock.Remove(SquadID); // no entry = full
}

void UBattleMatch::CarRecoverShot(int32 SquadID,int32 CarID)
{
 static const bool bOn=FParse::Param(FCommandLine::Get(),TEXT("BattleCarRecoverShots"));
 static const FString Prefix=[]{FString P;FParse::Value(FCommandLine::Get(),TEXT("BattleWideScreens="),P);return P;}();
 if(!bOn||Prefix.IsEmpty())return;
 static int32 Shots=0;static float LastAt=-100.f;
 if(Shots>=4||(Elapsed>=LastAt&&Elapsed-LastAt<20.f))return;
 auto* PC=GetWorld()->GetFirstPlayerController();
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store->FindSquad(SquadID);const auto* C=Store->FindSquad(CarID);if(!PC||!Runtime||!S||!C)return;
 const FVector CarAt=Runtime->Position(C->LeaderUnitID),SquadAt=Runtime->Position(S->LeaderUnitID);
 const FVector Mid=(CarAt+SquadAt)*.5;FVector Along=(SquadAt-CarAt).GetSafeNormal2D();if(Along.IsNearlyZero())Along=FVector(1,0,0);
 static TWeakObjectPtr<ACameraActor> Camera;
 if(!Camera.IsValid())
 {
  Camera=GetWorld()->SpawnActor<ACameraActor>();if(!Camera.IsValid())return;
  auto& PP=Camera->GetCameraComponent()->PostProcessSettings;PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=0;
  Camera->GetCameraComponent()->SetFieldOfView(60);Camera->GetCameraComponent()->bConstrainAspectRatio=false;
 }
 const FVector Eye=Mid+FVector(-Along.Y,Along.X,0)*1500.+FVector(0,0,1300);
 Camera->SetActorLocationAndRotation(Eye,(Mid-Eye).Rotation());
 PC->SetViewTarget(Camera.Get());PC->bAutoManageActiveCameraTarget=false;
 const int32 Number=++Shots;LastAt=Elapsed;
 int32 Living=0;for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())++Living;
 UE_LOG(LogFutureBattle,Display,TEXT("CAR_RECOVER_SHOT n=%d time=%.0f squad=%d team=%d car=%d living=%d of %d squad_at=%s car_at=%s"),Number,Elapsed,SquadID,S->Team,CarID,Living,S->Members.Num(),*SquadAt.ToCompactString(),*CarAt.ToCompactString());
 const FString Name=Prefix+FString::Printf(TEXT("-carrecover-%d-squad%d"),Number,SquadID);
 FTimerHandle First,Second,End;auto& Timers=GetWorld()->GetTimerManager();
 Timers.SetTimer(First,FTimerDelegate::CreateWeakLambda(this,[Name]{FScreenshotRequest::RequestScreenshot(Name+TEXT("-a.png"),true,false);}),1.f,false);
 Timers.SetTimer(Second,FTimerDelegate::CreateWeakLambda(this,[Name]{FScreenshotRequest::RequestScreenshot(Name+TEXT("-b.png"),true,false);}),4.f,false);
 if(Number>=4)Timers.SetTimer(End,FTimerDelegate::CreateWeakLambda(this,[]{UE_LOG(LogFutureBattle,Display,TEXT("CAR_RECOVER_SHOT done"));FPlatformMisc::RequestExitWithStatus(false,0);}),6.f,false);
}

int32 UBattleMatch::SupplyCarNear(int32 Team,const FVector& Position,float Radius) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return INDEX_NONE;
 const double Reach=Radius>0.f?Radius:FMath::Clamp(GetDefault<UBattleTuning>()->SupplyTeamRadius,100.f,20000.f);
 int32 Best=INDEX_NONE;double BestD=Reach;
 for(const auto& Pair:Store->GetSquads())
 {
  const auto& S=Pair.Value;if(S.Team!=Team||S.SquadType!=BattleSquadType::Supply||S.CommandStatus!=EBattleCommandStatus::Active||SupplyStockOf(Pair.Key)<=0)continue;
  const auto* Leader=Store->Find(S.LeaderUnitID);if(!Leader||!Leader->IsAlive())continue;
  const double D=FVector::Dist2D(R->Position(S.LeaderUnitID),Position);
  if(D<=BestD){BestD=D;Best=Pair.Key;}
 }
 return Best;
}

bool UBattleMatch::SupplyCarOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||S->SquadType!=BattleSquadType::Supply||S->Team!=Team)return false;
 const auto* T=GetDefault<UBattleTuning>();
 const FVector Home=AreaHome(Team),Here=R->Position(S->LeaderUnitID);
 const int32 Stock=SupplyStockOf(SquadID),Max=SupplyStockMax();
 // Loading: low on stock, or at home and not yet full. The waiting place is beside the home, toward the field.
 if(Max>0&&(Stock<FMath::CeilToInt(Max*FMath::Clamp(T->SupplyCarRestockRatio,0.f,1.f))||(Stock<Max&&FVector::Dist2D(Here,Home)<=2500.)))
 {Center=Home+FVector((Home.X>0?-1:1)*700,(SquadID%5-2)*600,0);Order=EBattleOrder::Move;return true;}
 const double Back=FMath::Max(0.f,T->SupplyCarBack);
 // With its group (when the AI runs it): the rally point while gathering, behind the support post while advancing, behind the point while holding.
 if(const int32 GID=GroupOf(SquadID);GID!=INDEX_NONE)
  if(const auto* G=Groups.Find(GID);G&&GroupAIRun(*G)&&Objectives.IsValidIndex(G->Objective))
  {
   const FVector Post=G->Phase==0?G->Rally:G->Phase==1?GroupSupportPost(Team,G->Objective):Objectives[G->Objective].Position;
   Center=G->Phase==0?Post:SupplyBack(*this,Team,Post,Back);Order=EBattleOrder::Defend;return true;
  }
 // Otherwise behind the own point (observed) nearest the enemy home; none: at home.
 int32 Best=INDEX_NONE;double BestD=DBL_MAX;
 for(int32 I=0;I<Objectives.Num();++I)if(ObservedOwner(Team,I)==Team)
 {const double D=FVector::Dist2D(Objectives[I].Position,AreaHome(1-Team));if(D<BestD){BestD=D;Best=I;}}
 Center=Best!=INDEX_NONE?SupplyBack(*this,Team,Objectives[Best].Position,Back):Home+FVector((Home.X>0?-1:1)*700,(SquadID%5-2)*600,0);
 Order=EBattleOrder::Defend;return true;
}

bool UBattleMatch::FieldSupplyPoint(int32 SquadID,FVector& Where,int32* CarID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||!bArea||S->SquadType==BattleSquadType::Supply||BattleSquadType::IsVehicle(S->SquadType))return false;
 const FVector Here=R->Position(S->LeaderUnitID);const float Range=FMath::Max(0.f,GetDefault<UBattleTuning>()->FieldSupplyRange);
 if(Range<=0.f)return false;
 // The squad's own group's vehicle first, then the nearest one in range.
 int32 Car=INDEX_NONE;
 if(const int32 GID=GroupOf(SquadID);GID!=INDEX_NONE)if(const auto* G=Groups.Find(GID))
  for(int32 Other:G->Squads)if(const auto* O=Store->FindSquad(Other);O&&O->SquadType==BattleSquadType::Supply&&O->CommandStatus==EBattleCommandStatus::Active&&SupplyStockOf(Other)>0)
   if(FVector::Dist2D(R->Position(O->LeaderUnitID),Here)<=Range){Car=Other;break;}
 if(Car==INDEX_NONE)Car=SupplyCarNear(S->Team,Here,Range);
 // A vehicle low enough to be going home to load is not sent for (it still supplies squads beside it).
 if(Car==INDEX_NONE||SupplyStockOf(Car)<FMath::CeilToInt(SupplyStockMax()*FMath::Clamp(GetDefault<UBattleTuning>()->SupplyCarRestockRatio,0.f,1.f)))return false;
 Where=R->Position(Store->FindSquad(Car)->LeaderUnitID);if(CarID)*CarID=Car;
 return true;
}
