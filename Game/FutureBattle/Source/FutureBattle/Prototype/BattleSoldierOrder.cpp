// The player's order to one soldier (2026-11-20, ROADMAP "個人への移動指定" plan 1). Ctrl+click picks a soldier of a player
// squad in the tactical view; a right click sends him alone to a place near his squad; H sends him back. Parts of
// UBattleLeaderRuntime (the order and the walk), ABattleController (input) and ABattleHUD (marks).
#include "BattleLeaderRuntime.h"
#include "BattleController.h"
#include "BattleHUD.h"
#include "BattleState.h"
#include "BattleFieldLayout.h"
#include "BattleMatch.h"
#include "BattleUnit.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/Canvas.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::IssueSoldierOrder(int32 ID,FVector Goal,FString& Why)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!bEnabled || !U || !U->IsAlive() || !Bodies.Contains(ID) || Goal.ContainsNaN()){Why=TEXT("この兵には命令できません");return false;}
 if(IsVehicle(ID) || U->Weapon==EBattleWeapon::Mortar){Why=TEXT("車両と迫撃砲班には個別の命令を出せません");return false;}
 if(const auto& B=Bodies.FindChecked(ID);B.Actor.IsValid() && B.Actor->IsPlayerControlled()){Why=TEXT("直接操作中の兵です");return false;}
 const auto* S=Store->FindSquad(U->SquadID);const auto* P=Plans.Find(U->SquadID);
 if(!S || !P || S->CommandStatus!=EBattleCommandStatus::Active){Why=TEXT("部隊が命令を受けられません");return false;}
 // The squad's path and centre start from its leader; he moves with the squad's orders.
 if(ID==S->LeaderUnitID){Why=TEXT("隊長には部隊の命令を使ってください");return false;}
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Projected;
 if(bWideField)BattleFieldLayout::Of(GetWorld()).Heights.Lift(Goal);
 if(!Nav || !Nav->ProjectPointToNavigation(Goal,Projected,FVector(100,100,400))){Why=TEXT("そこへは行けません");return false;}
 if(FVector::Dist2D(Projected.Location,P->Center)>SoldierOrderLeash){Why=FString::Printf(TEXT("部隊から離れすぎです（%.0fm以内）"),SoldierOrderLeash/100.f);return false;}
 const FVector Pos=Position(ID);
 UNavigationPath* Route=Nav->FindPathToLocationSynchronously(GetWorld(),Pos,Projected.Location);
 if(!Route || !Route->IsValid() || Route->IsPartial() || Route->PathPoints.Num()<2){Why=TEXT("そこまでの道がありません");return false;}
 CancelCoverAction(ID,TEXT("move"));CoverApproaches.Remove(ID);FirePositions.Remove(ID);CoverProbeAfter.Remove(ID);
 U->bSoldierOrder=true;U->bSoldierReturn=false;U->SoldierGoal=Projected.Location;U->SoldierOrderMission=S->MissionRevision;U->SoldierRepaths=0;U->SoldierStalledSince=0;
 auto& Path=SoldierPaths.FindOrAdd(ID);Path=FSoldierPath();for(const FVector& Point:Route->PathPoints)Path.Points.Add(Point);
 U->DecisionReason=TEXT("PLAYER ORDER / OWN PLACE");
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER unit=%d squad=%d goal=%s walk_cm=%.0f"),ID,U->SquadID,*U->SoldierGoal.ToCompactString(),FVector::Dist2D(Pos,U->SoldierGoal));
 return true;
}

bool UBattleLeaderRuntime::ReturnSoldier(int32 ID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U || !U->bSoldierOrder)return false;
 const auto* S=Store->FindSquad(U->SquadID);const auto* P=Plans.Find(U->SquadID);
 // On the march he simply follows the squad's path again; a stopped squad gets him back to his own place in it.
 if(!S || !P || (S->Order!=EBattleOrder::Hold && P->Path.Num()>1)){ClearSoldierOrder(ID,TEXT("returned"));return true;}
 FString Why;const int32 Mission=U->SoldierOrderMission;
 if(!IssueSoldierOrder(ID,U->Destination,Why))
 {
  // No full path back (or his place is out of reach): he still walks back; the stuck rule ends the order if he cannot.
  CancelCoverAction(ID,TEXT("move"));SoldierPaths.Remove(ID);U->SoldierGoal=U->Destination;U->SoldierRepaths=0;U->SoldierStalledSince=0;
 }
 U->bSoldierReturn=true;U->SoldierOrderMission=Mission;return true;
}

void UBattleLeaderRuntime::ClearSoldierOrder(int32 ID,const TCHAR* Why)
{
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);SoldierPaths.Remove(ID);
 if(!U || !U->bSoldierOrder)return;
 U->bSoldierOrder=false;U->bSoldierReturn=false;U->SoldierRepaths=0;U->SoldierStalledSince=0;
 if(ID==SoldierTest.Unit)SoldierTest.EndWhy=Why;
 // A cover place he took under his own order is not the squad's: he rejoins the squad's movement.
 CancelCoverAction(ID);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_END unit=%d why=%s"),ID,Why);
}

bool UBattleLeaderRuntime::SoldierOrderActive(int32 ID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U || !U->bSoldierOrder)return false;
 const auto* S=Store->FindSquad(U->SquadID);const auto* P=Plans.Find(U->SquadID);
 const auto* B=Bodies.Find(ID);
 const TCHAR* End=!U->IsAlive()?TEXT("died"):(!S || !P || S->CommandStatus!=EBattleCommandStatus::Active)?TEXT("squad_inactive"):
  (B && B->Actor.IsValid() && B->Actor->IsPlayerControlled())?TEXT("player_control"):
  S->MissionRevision!=U->SoldierOrderMission?TEXT("new_squad_order"):
  // The squad walked off (retreat, resupply): he goes with it.
  FVector::Dist2D(P->Center,U->SoldierGoal)>SoldierOrderLeash+1500.f?TEXT("squad_left"):
  (U->bSoldierReturn && FVector::DistSquared2D(Position(ID),U->SoldierGoal)<FMath::Square(250.))?TEXT("returned"):nullptr;
 if(End){ClearSoldierOrder(ID,End);return false;}
 return true;
}

void UBattleLeaderRuntime::StepSoldierOrder(int32 ID,float Dt,double Now,FVector& Direction)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);const auto* S=Store->FindSquad(U->SquadID);
 auto& B=Bodies.FindChecked(ID);const FVector Pos=Position(ID);
 auto& Path=SoldierPaths.FindOrAdd(ID);
 // The path is not saved: after loading (or after being stuck) it is searched again from where he stands.
 if(Path.Points.Num()<2)
 {
  Path.Points.Reset();Path.Next=1;
  auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
  UNavigationPath* Route=Nav?Nav->FindPathToLocationSynchronously(GetWorld(),Pos,U->SoldierGoal):nullptr;
  if(Route && Route->IsValid() && Route->PathPoints.Num()>1)for(const FVector& Point:Route->PathPoints)Path.Points.Add(Point);
  else{Path.Points.Add(Pos);Path.Points.Add(U->SoldierGoal);}
 }
 while(Path.Next<Path.Points.Num()-1 && FVector::DistSquared2D(Pos,Path.Points[Path.Next])<FMath::Square(90.))++Path.Next;
 const FVector Way=Path.Points[FMath::Min(Path.Next,Path.Points.Num()-1)];
 const FVector D=(Way-Pos).GetSafeNormal2D();
 const float Speed=GetDefault<UBattleTuning>()->MoveSpeed*BattleSquadType::SpeedAt(S->SquadType,GetWorld(),Pos);
 bool bStuck=false;
 // At the end of the path: done near the goal; the end of a partial path short of it counts as stuck.
 if(D.IsNearlyZero()){if(FVector::DistSquared2D(Pos,U->SoldierGoal)<=FMath::Square(100.))return;bStuck=true;} // 1 m: arrived, as in Follow
 else if(Direction=D;B.Actor.IsValid()){B.Actor->GetCharacterMovement()->MaxWalkSpeed=Speed;B.Actor->AddMovementInput(D);B.Velocity=B.Actor->GetVelocity();bStuck=B.Velocity.Size2D()<10;}
 else
 {
  // Map with heights: the step follows the ground (TerrainStep, as in the squad's movement); a place that cannot be stepped onto stops it.
  FVector Step=D*FMath::Min(Speed*Dt,float(FVector::Dist2D(Way,Pos)));if(!TerrainStep(B.Capsule->GetComponentLocation(),Step))Step=FVector::ZeroVector;
  FHitResult Hit;B.Capsule->MoveComponent(Step,D.Rotation().Quaternion(),true,&Hit);
  auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=B.Capsule->GetComponentLocation();
  B.Velocity=(F.Position-Pos)/FMath::Max(Dt,.001f);bStuck=(Hit.bBlockingHit || Step.IsNearlyZero()) && B.Velocity.Size2D()<Speed*.3f;
 }
 if(!bStuck){U->SoldierStalledSince=0;return;}
 if(U->SoldierStalledSince==0){U->SoldierStalledSince=Now;return;}
 if(Now-U->SoldierStalledSince<2.)return;
 // Stuck for 2 s: search the path again from here, twice at most, then give up and rejoin the squad.
 U->SoldierStalledSince=0;
 if(++U->SoldierRepaths>2){ClearSoldierOrder(ID,TEXT("stuck"));return;}
 Path=FSoldierPath();
}

// ---- Input (ABattleController) ----

bool ABattleController::PickSoldier(FVector2D Screen,int32& Out) const
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!L->IsEnabled())return false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 double Best=FMath::Square(28.);Out=INDEX_NONE;
 for(const auto& Pair:Store->GetUnits())
 {
  const auto& U=Pair.Value;if(!U.IsAlive() || U.Team!=0 || L->IsVehicle(U.UnitID))continue;
  const auto* S=Store->FindSquad(U.SquadID);if(!S || S->CommandStatus!=EBattleCommandStatus::Active)continue;
  FVector2D At;if(!ProjectWorldLocationToScreen(L->Position(U.UnitID),At))continue;
  if(const double D=FVector2D::DistSquared(At,Screen);D<Best){Best=D;Out=U.UnitID;}
 }
 return Out!=INDEX_NONE;
}

void ABattleController::SelectSoldierAt(FVector2D Screen)
{
 int32 ID;
 if(!PickSoldier(Screen,ID)){SoldierSelection=INDEX_NONE;ShowNotice(TEXT("兵がいません（Ctrl＋クリックで兵を1人選ぶ）"));return;}
 SoldierSelection=ID;Selected.Reset();
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 ShowNotice(FString::Printf(TEXT("兵を選択（第%d小隊）：右クリックで移動先、Hで部隊へ戻す"),U?U->SquadID:0));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT SoldierSelection=%d"),ID);
}

void ABattleController::OrderSoldierAt(FVector2D Screen)
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();FVector Target;
 if(SoldierSelection==INDEX_NONE || !PickOrderPoint(Screen,Target))return;
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(SoldierSelection);
 if(!U || !U->IsAlive()){SoldierSelection=INDEX_NONE;ShowNotice(TEXT("選んだ兵はもういません"));return;}
 FString Why;
 if(L->IssueSoldierOrder(SoldierSelection,Target,Why))ShowNotice(TEXT("兵へ命令：その場所へ移動して守る"));
 else ShowNotice(FString::Printf(TEXT("命令できません：%s"),*Why));
}

bool ABattleController::ReturnSelectedSoldier()
{
 if(SoldierSelection==INDEX_NONE)return false;
 // A soldier who died since he was picked: H is the squad's hold again.
 if(const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(SoldierSelection);!U || !U->IsAlive()){SoldierSelection=INDEX_NONE;return false;}
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 ShowNotice(L->ReturnSoldier(SoldierSelection)?TEXT("兵を部隊へ戻す"):TEXT("この兵には個別の命令がありません"));
 return true;
}

void ABattleController::CommandSoldierFromView()
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 auto* Own=Cast<ABattleUnit>(GetPawn());if(!L->IsEnabled() || !Own || !PlayerCameraManager)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const FVector Eye=PlayerCameraManager->GetCameraLocation(),Aim=GetControlRotation().Vector();
 // The friendly soldier nearest the crosshair (within 4 degrees and 60 m), not the one the player is.
 int32 Best=INDEX_NONE;double BestDot=FMath::Cos(FMath::DegreesToRadians(4.));
 for(const auto& Pair:Store->GetUnits())
 {
  const auto& U=Pair.Value;if(!U.IsAlive() || U.Team!=0 || U.UnitID==Own->GetUnitID() || L->IsVehicle(U.UnitID))continue;
  const FVector To=L->Position(U.UnitID)-Eye;if(To.SizeSquared()>FMath::Square(6000.))continue;
  if(const double Dot=FVector::DotProduct(To.GetSafeNormal(),Aim);Dot>BestDot){BestDot=Dot;Best=U.UnitID;}
 }
 const auto* Picked=Store->Find(SoldierSelection);
 if(Best!=INDEX_NONE && Best!=SoldierSelection)
 {
  SoldierSelection=Best;ShowNotice(TEXT("兵を選択：狙った場所でGを押すと移動、Hで部隊へ戻す"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT SoldierSelection=%d view=direct"),Best);return;
 }
 if(!Picked || !Picked->IsAlive()){SoldierSelection=INDEX_NONE;ShowNotice(TEXT("照準を味方の兵に合わせてGで選ぶ"));return;}
 // The place under the crosshair (100 m at most).
 FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(BattleSoldierOrderAim),false,Own);
 if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Aim*10000.,ECC_Visibility,Params)){ShowNotice(TEXT("命令できません：狙った場所が遠すぎます"));return;}
 FString Why;
 if(L->IssueSoldierOrder(SoldierSelection,Hit.ImpactPoint,Why))ShowNotice(TEXT("兵へ命令：その場所へ移動して守る"));
 else ShowNotice(FString::Printf(TEXT("命令できません：%s"),*Why));
}

// ---- Marks (ABattleHUD) ----

void ABattleHUD::DrawSoldierOrders()
{
 // Both views: the tactical map and direct control (the marks stand in the 3D view there).
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!L->IsEnabled())return;
 auto* PC=Cast<ABattleController>(PlayerOwner);if(!PC || !Canvas)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const FLinearColor Own(1.f,.82f,.25f,.95f),Back(.55f,.82f,1.f,.9f),Pick(.3f,1.f,.6f,.95f);
 UFont* Font=GEngine->GetSmallFont();
 // Text on a dark plate, so it reads over squad labels and bright ground.
 auto Tag=[&](const FString& Text,float X,float Y,FLinearColor Color)
 {float TW=0,TH=0;GetTextSize(Text,TW,TH,Font,1.f);DrawRect(FLinearColor(0,0,0,.6f),X-4,Y-2,TW+8,TH+4);DrawText(Text,Color,X,Y,Font,1.f);};
 auto Ring=[&](FVector2D C,float R,FLinearColor Color){for(int32 I=0;I<16;++I){const float A=I*PI/8,B=(I+1)*PI/8;DrawLine(C.X+R*FMath::Cos(A),C.Y+R*FMath::Sin(A),C.X+R*FMath::Cos(B),C.Y+R*FMath::Sin(B),Color,3);}};
 auto Diamond=[&](FVector2D C,float R,FLinearColor Color){DrawLine(C.X-R,C.Y,C.X,C.Y-R,Color,3);DrawLine(C.X,C.Y-R,C.X+R,C.Y,Color,3);DrawLine(C.X+R,C.Y,C.X,C.Y+R,Color,3);DrawLine(C.X,C.Y+R,C.X-R,C.Y,Color,3);};
 const int32 Picked=PC->GetSoldierSelection();
 int32 Drawn=0;
 for(const auto& Pair:Store->GetUnits())
 {
  const auto& U=Pair.Value;if(!U.bSoldierOrder || !U.IsAlive() || U.Team!=0 || ++Drawn>40)continue;
  const FLinearColor Color=U.bSoldierReturn?Back:Own;
  FVector2D From,To;
  const bool bFrom=PC->ProjectWorldLocationToScreen(L->Position(U.UnitID),From),bTo=PC->ProjectWorldLocationToScreen(U.SoldierGoal+FVector(0,0,40),To);
  if(bFrom && bTo)DrawLine(From.X,From.Y,To.X,To.Y,Color,2.f);
  if(bTo){Diamond(To,11,Color);if(U.UnitID==Picked)Tag(U.bSoldierReturn?TEXT("戻り先"):TEXT("移動先"),To.X+15,To.Y-8,Color);}
 }
 if(const auto* U=Store->Find(Picked);U && U->IsAlive())
 {
  FVector2D At;
  if(PC->ProjectWorldLocationToScreen(L->Position(Picked),At))
  {
   Ring(At,20,Pick);
   Tag(U->bSoldierOrder?(U->bSoldierReturn?TEXT("戻る途中"):TEXT("個別の命令")):TEXT("選択中の兵"),At.X+26,At.Y-24,Pick);
  }
  // How to use it, at the bottom of the direct-control view.
  if(!PC->IsTactical())Tag(TEXT("選択中の兵：狙った場所でGを押すと移動、Hで部隊へ戻す"),Canvas->SizeX*.5f-200,Canvas->SizeY-150,Pick);
 }
}

// ---- Self-test -BattleSoldierOrderTest (headless, any mixed map) ----
// A rifle squad of team 0 is put on hold (a player order, so the AI no longer commands it). One of its men is sent 12 m
// away, must arrive and hold; H sends him back and the order must end there; a second order must end with the squad's next
// order. BATTLE_SOLDIER_ORDER_TEST lines and BATTLE_SOLDIER_ORDER_RESULT failures=N.
void UBattleLeaderRuntime::StepSoldierOrderTest()
{
 static const bool bTest=FParse::Param(FCommandLine::Get(),TEXT("BattleSoldierOrderTest"));
 if(!bTest || SoldierTest.Stage>=7)return;
 // A failure that returned early last time: write the result line now.
 if(SoldierTest.Stage==6){UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_RESULT failures=%d"),SoldierTest.Failures);SoldierTest.Stage=7;return;}
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const double Now=GetWorld()->GetTimeSeconds();
 // After the opening orders of the match (the hold below takes the squad from the AI: a player order).
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto& T=SoldierTest;auto Finish=[&](const TCHAR* Why){++T.Failures;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_TEST fail stage=%d why=%s"),T.Stage,Why);T.Stage=6;};
 const auto* U=Store->Find(T.Unit);
 if(T.Stage>0 && T.Stage<5 && (!U || !U->IsAlive())){Finish(TEXT("soldier_died"));}
 else if(T.Stage==0 && Match && Match->Phase==EBattleMatchPhase::Combat && Match->Elapsed>=20.f)
 {
  TArray<int32> IDs;for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==0 && Pair.Value.SquadType==0 && Pair.Value.CommandStatus==EBattleCommandStatus::Active && !IsVehicle(Pair.Value.LeaderUnitID))IDs.Add(Pair.Key);
  IDs.Sort();
  for(int32 SID:IDs)
  {
   const auto* S=Store->FindSquad(SID);int32 Member=INDEX_NONE;
   for(int32 M:S->Members)if(M!=S->LeaderUnitID && Store->Find(M)->IsAlive()){Member=M;break;}
   if(Member==INDEX_NONE || !IssueOrder(SID,EBattleOrder::Hold,Position(S->LeaderUnitID)))continue;
   for(const FVector Off:{FVector(1200,0,0),FVector(0,1200,0),FVector(-1200,0,0),FVector(0,-1200,0)})
   {
    FString Why;
    if(IssueSoldierOrder(Member,Position(Member)+Off,Why)){T.Unit=Member;T.Squad=SID;T.Goal=Store->Find(Member)->SoldierGoal;T.Since=Now;T.Stage=1;break;}
   }
   if(T.Stage==1)break;
  }
  if(T.Stage!=1){Finish(TEXT("no_order_accepted"));}
  else UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_TEST start unit=%d squad=%d goal=%s"),T.Unit,T.Squad,*T.Goal.ToCompactString());
 }
 else if(T.Stage==1)
 {
  // -BattleSoldierOrderShots=<path prefix> (rendered runs): the tactical camera over him, picked, on the way and after arriving.
  static const FString Shots=[]{FString V;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierOrderShots="),V);return V;}();
  // The tactical camera comes down over him (lowest zoom); after he arrives the player takes the squad leader (direct
  // control) and looks at him.
  if(!Shots.IsEmpty())
   if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());PC && PC->GetPawn())
   {
    PC->SetSoldierSelection(T.Unit);
    if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn()))
    {
     const FVector At=Position(T.Unit);Camera->SetActorLocation(FVector(At.X,At.Y,Camera->GetActorLocation().Z));Camera->Zoom(-1.e6f);
     if(T.Shot==0 && Now-T.Since>=1.){FScreenshotRequest::RequestScreenshot(Shots+TEXT("-walking.png"),true,false);T.Shot=1;}
    }
    if(T.Shot==1 && T.Arrived>0 && Now-T.Arrived>=1.)
     if(const auto* S=Store->FindSquad(T.Squad);S && PC->ControlUnit(Actor(S->LeaderUnitID))){T.Shot=2;T.ShotAt=Now;}
    if(T.Shot==2)
    {
     const FVector Eye=Position(Store->FindSquad(T.Squad)->LeaderUnitID)+FVector(0,0,60);
     PC->SetControlRotation(FRotator(-12.f,(T.Goal-Eye).Rotation().Yaw,0));
     if(Now-T.ShotAt>=1.5){FScreenshotRequest::RequestScreenshot(Shots+TEXT("-direct.png"),true,false);T.Shot=3;}
    }
   }
  // Walk and hold: within 1.5 m of his own place, and still under his own order 5 s later.
  const double D=FVector::Dist2D(Position(T.Unit),T.Goal);
  if(!U->bSoldierOrder)Finish(TEXT("order_lost_on_the_way"));
  else if(D<150. && T.Arrived==0)T.Arrived=Now;
  else if(T.Arrived>0 && Now-T.Arrived>=5.)
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_TEST arrived unit=%d seconds=%.1f distance_cm=%.0f"),T.Unit,T.Arrived-T.Since,D);
   ReturnSoldier(T.Unit);T.Stage=2;T.Since=Now;
  }
  else if(Now-T.Since>30.)Finish(TEXT("not_arrived"));
 }
 else if(T.Stage==2)
 {
  // Sent back: the order ends when he reaches his place in the squad.
  // Back means the order ended as "returned", within the 2.5 m of the return rule (not stuck and given up on the way).
  if(!U->bSoldierOrder && (T.EndWhy!=TEXT("returned") || FVector::Dist2D(Position(T.Unit),U->SoldierGoal)>260.)){Finish(*(TEXT("order_ended_")+T.EndWhy));return;}
  if(!U->bSoldierOrder)
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_TEST returned unit=%d seconds=%.1f from_goal_cm=%.0f"),T.Unit,Now-T.Since,FVector::Dist2D(Position(T.Unit),T.Goal));
   FString Why;
   if(!IssueSoldierOrder(T.Unit,T.Goal,Why)){Finish(TEXT("second_order_refused"));return;}
   T.Stage=3;T.Since=Now;
  }
  else if(Now-T.Since>30.)Finish(TEXT("not_returned"));
 }
 else if(T.Stage==3 && Now-T.Since>=3.)
 {
  // The squad's next order ends his own.
  const auto* S=Store->FindSquad(T.Squad);
  if(!S || !IssueOrder(T.Squad,EBattleOrder::Hold,Position(S->LeaderUnitID))){Finish(TEXT("squad_order_refused"));return;}
  T.Stage=4;T.Since=Now;
 }
 else if(T.Stage==4)
 {
  if(!U->bSoldierOrder || !SoldierOrderActive(T.Unit))
  {
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_TEST cleared_by_squad_order unit=%d"),T.Unit);
   // Last: ordered again, then killed; his order must end with him (a dead soldier with an order fails the save check).
   FString Why;
   if(!IssueSoldierOrder(T.Unit,T.Goal,Why)){Finish(TEXT("third_order_refused"));return;}
   Store->ApplyDamage(T.Unit,1,100000);T.Stage=5;
  }
  else if(Now-T.Since>2.)Finish(TEXT("not_cleared_by_squad_order"));
 }
 else if(T.Stage==5)
 {
  if(U && U->IsAlive())Finish(TEXT("not_killed"));
  else if(U && U->bSoldierOrder)Finish(TEXT("order_kept_after_death"));
  else{UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_TEST cleared_by_death unit=%d"),T.Unit);T.Stage=6;}
 }
 if(T.Stage==6){UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SOLDIER_ORDER_RESULT failures=%d"),T.Failures);T.Stage=7;}
}
