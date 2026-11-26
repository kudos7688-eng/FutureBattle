#include "BattleController.h"
#include "BattleEnemyTrack.h"
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleUnit.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"
#include "UnrealClient.h"

namespace
{
 // Resolve through IssueOrder, with no extra navigation queries or random draws.
 bool TargetDestination(UBattleLeaderRuntime* Runtime,int32 OwnID,const FBattleEnemyTrackEntry& Entry,bool bUpdate,FVector& AcceptedAt,int32* RefusedCalls=nullptr)
 {
  TArray<FVector,TInlineAllocator<8>> Candidates=Entry.Soldiers;
  Candidates.StableSort([&Entry](const FVector& Left,const FVector& Right){return FVector::DistSquared2D(Left,Entry.Where)<FVector::DistSquared2D(Right,Entry.Where);});
  Candidates.Insert(Entry.Where,0);
  for(int32 Attempt=0;Attempt<FMath::Min(5,Candidates.Num());++Attempt)
  {
   if(Runtime->IssueOrder(OwnID,EBattleOrder::AttackMove,Candidates[Attempt],bUpdate,false)){AcceptedAt=Candidates[Attempt];return true;}
   if(RefusedCalls)++(*RefusedCalls);
  }
  return false;
 }
 bool TargetSquadAlive(const UBattleState* Store,const FBattleSquadState& Squad)
 {
  for(int32 MemberID:Squad.Members)if(const auto* Member=Store->Find(MemberID);Member&&Member->IsAlive())return true;
  return false;
 }
}
void UBattleLeaderRuntime::SetOrderTarget(int32 SquadID,int32 EnemySquadID)
{
 if(EnemySquadID==INDEX_NONE){OrderTargets.Remove(SquadID);return;}
 OrderTargets.Add(SquadID,EnemySquadID);
}
int32 UBattleLeaderRuntime::OrderTarget(int32 SquadID) const
{
 const int32* EnemyID=OrderTargets.IsEmpty()?nullptr:OrderTargets.Find(SquadID);
 return EnemyID?*EnemyID:INDEX_NONE;
}
bool ABattleController::EnemyEntry(int32 EnemySquadID,FBattleEnemyTrackEntry& Out) const
{
 if(EnemySquadID==INDEX_NONE)return false;
 if(BattleEnemyTrack::KeepSeconds()>0.f)
 {if(const auto* Entry=EnemyTrack.Find(EnemySquadID)){Out=*Entry;return true;}return false;}
 const auto Seen=SeenEnemySquads(GetWorld());
 if(const auto* Entry=Seen.Find(EnemySquadID))
 {
  Out=FBattleEnemyTrackEntry{};Out.SquadID=EnemySquadID;Out.Type=Entry->Type;Out.Count=Entry->Count;
  Out.Where=Entry->Sum/Entry->Count;Out.Soldiers=Entry->Soldiers;Out.LastSeenAt=GetWorld()->GetTimeSeconds();return true;
 }
 return false;
}
int32 ABattleController::EnemySquadAt(FVector2D Screen) const
{
 int32 Closest=INDEX_NONE;double BestDistance=TNumericLimits<double>::Max();
 auto Consider=[&](int32 EnemyID,const FVector& Center,const TArray<FVector,TInlineAllocator<8>>& Soldiers)
 {
  auto Hit=[&](const FVector2D& Point)
  {const double Distance=FVector2D::DistSquared(Point,Screen);if(Distance<BestDistance||(Distance==BestDistance&&EnemyID<Closest)){BestDistance=Distance;Closest=EnemyID;}};
  FVector2D IconAt;
  if(ProjectWorldLocationToScreen(Center+FVector(0,0,250),IconAt))
  {IconAt.Y-=14.;if(FMath::Abs(IconAt.X-Screen.X)<=20.&&FMath::Abs(IconAt.Y-Screen.Y)<=16.)Hit(IconAt);}
  for(const FVector& SoldierAt:Soldiers)
  {FVector2D SoldierScreen;if(ProjectWorldLocationToScreen(SoldierAt+FVector(0,0,90),SoldierScreen)&&FVector2D::DistSquared(SoldierScreen,Screen)<=28.*28.)Hit(SoldierScreen);}
 };
 if(BattleEnemyTrack::KeepSeconds()>0.f)
 {for(const auto& Track:EnemyTrack)Consider(Track.Key,Track.Value.Where,Track.Value.Soldiers);}
 else
 {for(const auto& Seen:SeenEnemySquads(GetWorld()))Consider(Seen.Key,Seen.Value.Sum/Seen.Value.Count,Seen.Value.Soldiers);}
 return Closest;
}
bool ABattleController::FireRevealAt(FVector2D Screen,int32& OutKey,FVector& OutWhere) const
{
 OutKey=INDEX_NONE;OutWhere=FVector::ZeroVector;bool bFound=false;
 double ClosestDistance=TNumericLimits<double>::Max();
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 for(const auto& ReportPair:Match->FireReveals[0])
 {
  const auto& Report=ReportPair.Value;
  if(Report.At>Match->Elapsed||Match->Elapsed-Report.At>=Report.Memory)continue;
  FVector2D ReportScreen;if(!ProjectWorldLocationToScreen(Report.Position+FVector(0,0,250),ReportScreen))continue;
  ReportScreen.Y-=14.;
  if(FMath::Abs(ReportScreen.X-Screen.X)>20.||FMath::Abs(ReportScreen.Y-Screen.Y)>16.)continue;
  const double ReportDistance=FVector2D::DistSquared(ReportScreen,Screen);
  if(!bFound||ReportDistance<ClosestDistance||(ReportDistance==ClosestDistance&&ReportPair.Key<OutKey))
  {bFound=true;ClosestDistance=ReportDistance;OutKey=ReportPair.Key;OutWhere=Report.Position;}
 }
 return bFound;
}
int32 ABattleController::IssueFlashOrder(int32 RevealKey)
{
 return IssueFlashOrderForSquads(RevealKey,SelectedTargetSquads());
}
int32 ABattleController::IssueFlashOrderForSquads(int32 RevealKey,const TArray<int32>& SquadIDs)
{
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!Runtime->IsEnabled())return 0;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();const auto* Report=Match->FireReveals[0].Find(RevealKey);
 if(!Report||Report->At>Match->Elapsed||Match->Elapsed-Report->At>=Report->Memory)return 0;
 const FVector EstimatedWhere=Report->Position;const float EstimatedRadius=FMath::Max(0.f,Report->ErrorRadius);
 const double Now=GetWorld()->GetTimeSeconds();const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 int32 Accepted=0,Rejected=0;TSet<int32> Done;
 for(int32 OwnID:SquadIDs)
 {
  if(Done.Contains(OwnID))continue;Done.Add(OwnID);
  const auto* OwnSquad=Store->FindSquad(OwnID);
  if(!OwnSquad||OwnSquad->Team!=0||OwnSquad->CommandStatus!=EBattleCommandStatus::Active||!TargetSquadAlive(Store,*OwnSquad))continue;
  bool bAccepted=false;
  // Centre first, then eight fixed directions, starting at +X. No random sampling.
  for(int32 Attempt=0;Attempt<9;++Attempt)
  {
   const double Angle=(Attempt-1)*PI/4.;
   const FVector CandidateWhere=Attempt==0?EstimatedWhere:EstimatedWhere+FVector(FMath::Cos(Angle)*600.,FMath::Sin(Angle)*600.,0.);
   if(Runtime->IssueOrder(OwnID,EBattleOrder::AttackMove,CandidateWhere,false,false)){bAccepted=true;break;}
  }
  if(bAccepted)
  {
   Match->ClearPlayerRouteMission(OwnID);
   TargetOrders.Remove(OwnID);Runtime->SetOrderTarget(OwnID,INDEX_NONE);
   FlashOrders.Add(OwnID,{EstimatedWhere,EstimatedRadius,Store->FindSquad(OwnID)->MissionRevision,Now});++Accepted;
  }
  else ++Rejected;
 }
 ShowNotice(FString::Printf(TEXT("発砲位置へ攻撃：%d部隊（不可 %d）。位置は推定です"),Accepted,Rejected));
 UE_LOG(LogTemp,Display,TEXT("BATTLE_INPUT FlashOrder key=%d accepted=%d rejected=%d"),RevealKey,Accepted,Rejected);
 return Accepted;
}

bool ABattleController::TryTargetOrderClick()
{
 const bool bPressed=bTargetPress;bTargetPress=false;
 if(!bPressed||!bTactical||Selected.IsEmpty()||SoldierSelection!=INDEX_NONE||bFrontDraw||PendingOrder==EBattleOrder::Defend)return false;
 FVector2D PointerAt;
 if(!GetPointerPosition(PointerAt)||FVector2D::Distance(PointerAt,bPathDraw?PathScreenStart:TargetPressScreen)>12.)return false;
 const int32 EnemyID=EnemySquadAt(PointerAt);
 if(EnemyID!=INDEX_NONE)IssueTargetOrder(EnemyID);
 else
 {
  int32 FlashKey=INDEX_NONE;FVector FlashWhere;
  if(!FireRevealAt(PointerAt,FlashKey,FlashWhere))return false;
  IssueFlashOrder(FlashKey);
 }
 bPathDraw=false;DrawnPath.Reset();DrawnLength=0.;PendingOrder=EBattleOrder::Move;return true;
}
TArray<int32> ABattleController::SelectedTargetSquads() const
{
 TArray<int32> SquadIDs;
 for(ABattleUnit* Unit:Selected)
  if(Unit&&Unit->IsAlive()&&Unit->GetState()&&Unit->GetState()->Team==0&&LeaderSelectable(Unit))SquadIDs.AddUnique(Unit->GetState()->SquadID);
 return SquadIDs;
}
int32 ABattleController::IssueTargetOrder(int32 EnemySquadID)
{
 return IssueTargetOrderForSquads(EnemySquadID,SelectedTargetSquads(),true);
}
int32 ABattleController::IssueTargetOrderForSquads(int32 EnemySquadID,const TArray<int32>& SquadIDs,bool bNotify)
{
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!Runtime->IsEnabled())return 0;
 FBattleEnemyTrackEntry Entry;if(!EnemyEntry(EnemySquadID,Entry))return 0;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const double Now=GetWorld()->GetTimeSeconds();int32 Accepted=0,Rejected=0;TSet<int32> Done;
 for(int32 OwnID:SquadIDs)
 {
  if(Done.Contains(OwnID))continue;Done.Add(OwnID);
  const auto* OwnSquad=Store->FindSquad(OwnID);
  if(!OwnSquad||OwnSquad->Team!=0||OwnSquad->CommandStatus!=EBattleCommandStatus::Active||!TargetSquadAlive(Store,*OwnSquad))continue;
  FVector AcceptedAt;
  if(TargetDestination(Runtime,OwnID,Entry,false,AcceptedAt))
  {
   Match->ClearPlayerRouteMission(OwnID);Runtime->SetOrderTarget(OwnID,EnemySquadID);
   const auto* Squad=Store->FindSquad(OwnID);
   TargetOrders.Add(OwnID,{EnemySquadID,AcceptedAt,Squad->MissionRevision,Now,Now});FlashOrders.Remove(OwnID);++Accepted;
  }
  else ++Rejected;
 }
 if(bNotify)ShowNotice(FString::Printf(TEXT("攻撃目標：敵の%s部隊（%d部隊が向かう／不可 %d）"),Entry.Type<BattleSquadType::Count?FBattleBuyPanelLayout::TypeName(Entry.Type):TEXT("不明の"),Accepted,Rejected));
 UE_LOG(LogTemp,Display,TEXT("BATTLE_INPUT TargetOrder enemy=%d state=%d accepted=%d rejected=%d"),EnemySquadID,Entry.State,Accepted,Rejected);
 return Accepted;
}
int32 ABattleController::TargetOrderOf(int32 SquadID) const
{
 const auto* Order=TargetOrders.Find(SquadID);return Order?Order->EnemySquad:INDEX_NONE;
}
void ABattleController::UpdateTargetOrders()
{
 // This gate also avoids touching any runtime table in AI-only matches.
 if(TargetOrders.IsEmpty()&&FlashOrders.IsEmpty())return;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!Runtime->IsEnabled())return;
 const double Now=GetWorld()->GetTimeSeconds();if(Now<NextTargetOrderUpdate)return;NextTargetOrderUpdate=Now+.5;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();bool bDestroyedNotice=false,bLostNotice=false,bArrivedNotice=false;
 for(auto It=TargetOrders.CreateIterator();It;++It)
 {
  auto& Order=It.Value();const int32 OwnID=It.Key();const auto* Squad=Store->FindSquad(OwnID);
  auto Remove=[&]{Runtime->SetOrderTarget(OwnID,INDEX_NONE);It.RemoveCurrent();};
  if(!Squad||Squad->CommandStatus!=EBattleCommandStatus::Active||!TargetSquadAlive(Store,*Squad)){Remove();continue;}
  if(Squad->MissionRevision!=Order.Revision){Remove();continue;}
  if(EnemyTrackDestroyed.Contains(Order.EnemySquad)){bDestroyedNotice=true;Remove();continue;}
  FBattleEnemyTrackEntry Entry;
  if(!EnemyEntry(Order.EnemySquad,Entry)){bLostNotice=true;Remove();continue;}
  if(Entry.State<=1&&FVector::Dist2D(Entry.Where,Order.Goal)>1500.&&Now-Order.LastReissueAt>=2.)
  {
   FVector AcceptedAt;
   if(TargetDestination(Runtime,OwnID,Entry,true,AcceptedAt,&TargetReissueRefused))
   {
    Order.Goal=AcceptedAt;Order.Revision=Store->FindSquad(OwnID)->MissionRevision;Order.LastReissueAt=Now;
    UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_REISSUE squad=%d enemy=%d accepted=1"),OwnID,Order.EnemySquad);
   }
   else
   {
    ++TargetRefusedBySquad.FindOrAdd(OwnID);
    UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_REISSUE squad=%d enemy=%d accepted=0"),OwnID,Order.EnemySquad);
   }
  }
  if(Entry.State==2&&FVector::Dist2D(Runtime->Position(Squad->LeaderUnitID),Order.Goal)<=2000.&&Now-Entry.LastSeenAt>BattleEnemyTrack::TrackSeconds()+5.)
  {bArrivedNotice=true;Remove();}
 }
 // Snapshot own IDs because a successful conversion removes its FlashOrders row.
 TArray<int32> FlashSquads;FlashOrders.GetKeys(FlashSquads);bool bConvertedFlash=false;
 for(int32 FlashSquadID:FlashSquads)
 {
  const auto* FlashMission=FlashOrders.Find(FlashSquadID);if(!FlashMission)continue;
  const auto* FlashSquad=Store->FindSquad(FlashSquadID);
  if(!FlashSquad||FlashSquad->CommandStatus!=EBattleCommandStatus::Active||!TargetSquadAlive(Store,*FlashSquad)||
     FlashSquad->MissionRevision!=FlashMission->Revision||Now-FlashMission->IssuedAt>=120.)
  {FlashOrders.Remove(FlashSquadID);continue;}
  // Match only observed/tracked map positions, never the shooter ID or hidden true state.
  int32 DiscoveredID=INDEX_NONE;double NearestDistance=FMath::Square(double(FlashMission->ErrorRadius));
  for(const auto& KnownEnemy:EnemyTrack)if(KnownEnemy.Value.State<=1)
  {
   const double EnemyDistance=FVector::DistSquared2D(KnownEnemy.Value.Where,FlashMission->Where);
   if(EnemyDistance<NearestDistance||(EnemyDistance==NearestDistance&&(DiscoveredID==INDEX_NONE||KnownEnemy.Key<DiscoveredID)))
   {DiscoveredID=KnownEnemy.Key;NearestDistance=EnemyDistance;}
  }
  if(DiscoveredID!=INDEX_NONE&&IssueTargetOrderForSquads(DiscoveredID,TArray<int32>{FlashSquadID},false)>0)bConvertedFlash=true;
 }
 if(bDestroyedNotice)ShowNotice(TEXT("攻撃目標の敵部隊を撃破しました"));
 if(bLostNotice)ShowNotice(TEXT("攻撃目標を見失いました"));
 if(bArrivedNotice)ShowNotice(TEXT("攻撃目標を見失いました（最後の位置に着きました）"));
 if(bConvertedFlash)ShowNotice(TEXT("発砲した敵部隊を発見、攻撃目標にしました"));
}

// Controller-driven self-test: requires a local player controller, including in -nullrhi games.
void ABattleController::StepTargetOrderTest()
{
 static const bool bTest=FParse::Param(FCommandLine::Get(),TEXT("BattleTargetOrderTest"));
 if(!bTest||TargetTest.Stage>=5)return;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!Runtime->IsEnabled())return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const double Now=GetWorld()->GetTimeSeconds();auto& Test=TargetTest;
 auto Fail=[&](const TCHAR* Why){++Test.Failures;UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_TEST fail stage=%d why=%s"),Test.Stage,Why);};
 static const FString Shots=[]{FString Prefix;FParse::Value(FCommandLine::Get(),TEXT("BattleTargetOrderShots="),Prefix);return Prefix;}();
 auto Aim=[&](const FVector& Where)
 {
  if(!FApp::CanEverRender())return false;
  if(!IsTactical())EnterTactical();
  APawn* CameraPawn=GetPawn();if(!CameraPawn||!PlayerCameraManager)return false;
  const FVector Eye=PlayerCameraManager->GetCameraLocation(),ForwardDirection=PlayerCameraManager->GetCameraRotation().Vector();
  const double Down=FMath::Max(.2,-ForwardDirection.Z);
  const FVector LookAt=Eye+ForwardDirection*((Eye.Z-Where.Z)/Down);
  CameraPawn->SetActorLocation(CameraPawn->GetActorLocation()+FVector(Where.X-LookAt.X,Where.Y-LookAt.Y,0.));
  return true;
 };
 if(!Shots.IsEmpty())
 {
  if(Test.OrderShotAt>=0.&&!Test.bOrderShot&&Now>=Test.OrderShotAt)
  {FScreenshotRequest::RequestScreenshot(Shots+TEXT("-order.png"),true,false);Test.bOrderShot=true;}
  if(Test.Enemy!=INDEX_NONE&&!Test.bRememberedShot&&Test.RememberedShotAt<0.)
   if(const auto* Entry=EnemyTrack.Find(Test.Enemy);Entry&&Entry->State==2)
   {
    if(Aim(Entry->Where))Test.RememberedShotAt=Now+1.;
    else {UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_SHOT skipped=remembered why=no_render_or_camera"));Test.bRememberedShot=true;}
   }
  if(Test.RememberedShotAt>=0.&&!Test.bRememberedShot&&Now>=Test.RememberedShotAt)
  {FScreenshotRequest::RequestScreenshot(Shots+TEXT("-remembered.png"),true,false);Test.bRememberedShot=true;}
 }
 if(Test.Stage==0)
 {
  if(Now>=300.){Fail(TEXT("no_enemy_seen"));Test.Stage=3;}
  else if(Match->Phase==EBattleMatchPhase::Combat)
  {
   ABattleUnit* Pick=nullptr;int32 EnemyID=INDEX_NONE;double Closest=TNumericLimits<double>::Max();
   for(const auto& Entry:EnemyTrack)if(Entry.Value.State==0)
    for(const auto& Own:Store->GetSquads())
    {
     const auto& Squad=Own.Value;
     if(Squad.Team!=0||Squad.CommandStatus!=EBattleCommandStatus::Active||Runtime->IsVehicle(Squad.LeaderUnitID))continue;
     ABattleUnit* Candidate=Runtime->Actor(Squad.LeaderUnitID);
     if(!Candidate||!Candidate->IsAlive()||!LeaderSelectable(Candidate))continue;
     const double Distance=FVector::DistSquared2D(Candidate->GetActorLocation(),Entry.Value.Where);
     if(Distance<Closest){Closest=Distance;Pick=Candidate;EnemyID=Entry.Key;}
    }
   if(Pick)
   {
    SetSelection(Pick);Test.Squad=Pick->GetState()->SquadID;Test.Enemy=EnemyID;
    Test.Started=Test.Since=Now;
    const int32 Accepted=IssueTargetOrder(EnemyID);const auto* Squad=Store->FindSquad(Test.Squad);
    const auto* Order=TargetOrders.Find(Test.Squad);
    if(Accepted<1)Fail(TEXT("order_refused"));
    if(TargetOrderOf(Test.Squad)!=EnemyID||Runtime->OrderTarget(Test.Squad)!=EnemyID)Fail(TEXT("target_not_set"));
    if(!Squad||Squad->MissionOrder!=EBattleOrder::AttackMove||Squad->bSectorMission)Fail(TEXT("not_point_attack_move"));
    if(!Squad||!Order||FVector::Dist2D(Squad->MissionDestination,Order->Goal)>300.)Fail(TEXT("wrong_destination"));
    Test.MoveGoal=Order?Order->Goal:FVector::ZeroVector;Test.Stage=1;
    UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_TEST start squad=%d enemy=%d"),Test.Squad,EnemyID);
    if(!Shots.IsEmpty())
    {
     const auto* Entry=EnemyTrack.Find(EnemyID);
     if(Entry&&Aim((Pick->GetActorLocation()+Entry->Where)*.5))Test.OrderShotAt=Now+1.;
     else {UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_SHOT skipped=order why=no_render_or_camera"));Test.bOrderShot=true;}
    }
   }
  }
 }
 else if(Test.Stage==1&&Now>=Test.NextPoll)
 {
  Test.NextPoll=FMath::Max(Now+.5,NextTargetOrderUpdate);
  const auto* Entry=EnemyTrack.Find(Test.Enemy);const auto* Squad=Store->FindSquad(Test.Squad);
  const auto* Order=TargetOrders.Find(Test.Squad);
  if(!Entry||!Order)
  {
   UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_TEST ended reason=%s"),EnemyTrackDestroyed.Contains(Test.Enemy)?TEXT("destroyed"):!Entry?TEXT("track_expired"):TEXT("order_ended"));
   if(TargetOrderOf(Test.Squad)!=INDEX_NONE||Runtime->OrderTarget(Test.Squad)!=INDEX_NONE)Fail(TEXT("order_not_cleared"));
   Test.Stage=2;Test.Since=Now;
  }
  else
  {
   UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_TEST sample state=%d age=%.1f"),Entry->State,Now-Entry->LastSeenAt);
   // Compare with the previous accepted goal, since UpdateTargetOrders may already have reissued this tick.
   if(Entry->State<=1&&Test.MoveAt<0.&&FVector::Dist2D(Entry->Where,Test.MoveGoal)>1500.)
   {++Test.Moved;Test.MoveAt=Now;Test.RefusedAtMove=Test.LastRefusedSeen;}
   if(Test.MoveAt>=0.)
   {
    if(Squad&&FVector::Dist2D(Order->Goal,Test.MoveGoal)>1.&&FVector::Dist2D(Squad->MissionDestination,Order->Goal)<=300.)
    {++Test.Followed;Test.MoveAt=-1.;Test.MoveGoal=Order->Goal;}
    else if(Now-Test.MoveAt>=3.)
    {
     if(TargetRefusedBySquad.FindRef(Test.Squad)==Test.RefusedAtMove&&Entry->State<=1)Fail(TEXT("not_followed"));
     Test.MoveAt=-1.;Test.MoveGoal=Order->Goal;
    }
   }
   Test.LastRefusedSeen=TargetRefusedBySquad.FindRef(Test.Squad);
   if(Now-Test.Since>=30.&&Test.MoveAt<0.){Test.Stage=2;Test.Since=Now;}
   else if(Now-Test.Since>=90.){Fail(TEXT("follow_timeout"));Test.Stage=2;Test.Since=Now;}
  }
 }
 else if(Test.Stage==2)
 {
  if(!Test.bHoldIssued)
  {
   Test.bHoldIssued=true;Test.Since=Now;
   if(TargetOrderOf(Test.Squad)==INDEX_NONE)
   {if(Runtime->OrderTarget(Test.Squad)!=INDEX_NONE)Fail(TEXT("order_not_cleared"));Test.Stage=3;}
   else if(const auto* Squad=Store->FindSquad(Test.Squad))
   {
    if(!Runtime->IssueOrder(Test.Squad,EBattleOrder::Hold,Runtime->Position(Squad->LeaderUnitID)))
    {Fail(TEXT("hold_refused"));Test.Stage=3;}
   }
   else {Fail(TEXT("own_squad_missing"));Test.Stage=3;}
  }
  else if(TargetOrderOf(Test.Squad)==INDEX_NONE&&Runtime->OrderTarget(Test.Squad)==INDEX_NONE)Test.Stage=3;
  else if(Now-Test.Since>=1.5){Fail(TEXT("order_not_cleared"));Test.Stage=3;}
 }
 if(Test.Stage==3)
 {
  // Synthetic report exists only during this synchronous fixture and is removed on every exit.
  if(const auto* FlashTestSquad=Store->FindSquad(Test.Squad);FlashTestSquad&&TargetSquadAlive(Store,*FlashTestSquad))
  {
   constexpr int32 FlashTestKey=-9001;
   UBattleMatch::FFireReveal FlashReport;
   FlashReport.Position=Runtime->Position(FlashTestSquad->LeaderUnitID)+FVector(3000,0,0);
   FlashReport.At=Match->Elapsed;FlashReport.Memory=30.;FlashReport.bArtillery=true;
   Match->FireReveals[0].Add(FlashTestKey,FlashReport);
   ON_SCOPE_EXIT{Match->FireReveals[0].Remove(FlashTestKey);FlashOrders.Remove(Test.Squad);};
   const int32 FlashAccepted=IssueFlashOrderForSquads(FlashTestKey,TArray<int32>{Test.Squad});
   const auto* FlashResultSquad=Store->FindSquad(Test.Squad);
   if(FlashAccepted==1&&FlashResultSquad&&FlashResultSquad->MissionOrder==EBattleOrder::AttackMove&&!FlashResultSquad->bSectorMission&&
      FVector::Dist2D(FlashResultSquad->MissionDestination,FlashReport.Position)<=700.&&
      TargetOrderOf(Test.Squad)==INDEX_NONE&&Runtime->OrderTarget(Test.Squad)==INDEX_NONE)Test.FlashOrder=1;
   else Fail(TEXT("flash_order"));
  }
  else Fail(TEXT("flash_order_no_squad"));
  struct FStateCase { double Age; float Track,Keep; int32 Expected; };
  const FStateCase Cases[]={{.1,10,60,1},{10,10,60,1},{10.1,10,60,2},{60,10,60,2},{60.1,10,60,-1},
   {.1,0,60,2},{0,0,60,2},{10,10,5,1},{10.1,10,5,-1},{.1,10,0,-1}};
  for(const auto& StateCase:Cases)
   if(BattleEnemyTrack::StateAt(StateCase.Age,StateCase.Track,StateCase.Keep)!=StateCase.Expected)Fail(TEXT("state_lifetime"));
  Test.Stage=4;
 }
 if(Match->Phase==EBattleMatchPhase::Finished&&Test.Stage<4)
 {Fail(Test.Stage==0?TEXT("no_enemy_seen"):TEXT("match_ended_before_checks"));Test.Stage=3;return;}
 if(Test.Stage==4&&(Test.Started<0.||Now-Test.Started>=240.||Match->Phase==EBattleMatchPhase::Finished))
 {
  if(!Shots.IsEmpty())
  {
   if(!Test.bOrderShot)UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_SHOT skipped=order why=not_reached"));
   if(!Test.bRememberedShot)UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_SHOT skipped=remembered why=no_remembered_target"));
  }
  UE_LOG(LogTemp,Display,TEXT("BATTLE_TARGET_ORDER_RESULT failures=%d tracked_entries=%d remembered_entries=%d longest_remembered_s=%.1f followed=%d moved=%d reissue_refused=%d order_target_pick=%d flash_order=%d"),
   Test.Failures,EnemyTrackedEntries,EnemyRememberedEntries,EnemyLongestRemembered,Test.Followed,Test.Moved,TargetReissueRefused,Runtime->OrderTargetPicks,Test.FlashOrder);
  Test.Stage=5;
 }
}
