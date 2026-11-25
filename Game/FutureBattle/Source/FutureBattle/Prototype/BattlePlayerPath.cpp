// Player input helpers of 2026-11-19 (user requests after the first look at map G). All of it is the player's own input and
// display; a match between AIs never runs this file.
//  - PickOrderPoint: where a right click points. On a map with height levels the upper floor (or hilltop) the cursor's ray
//    crosses is taken even when a wall of that storey hides the floor; Alt takes the ground floor beneath an upper floor.
//  - Drawn path: holding the right button and dragging draws a line; on release the selected squads walk it point by point
//    (UBattleMatch::PlayerPathOrder, handed on by UpdatePlayerRoutes) and take the pending order at its end. A short drag is a click.
//    Trial -BattleSectorMove=1 on a map with sectors: a move click is a sector move (IssueOrderAt); any drag of the pointer by more
//    than 12 pixels is a point move, along the line when it is long enough for way points, else straight to its end.
//  - PlayerRoutePreview: the points a routed or drawn order still has ahead (HUD: the predicted way of a selected squad).
//  - Vault: the jump key in front of a low obstacle (up to 1.6m, free ground behind) carries the directly controlled leader over.
//  - PanSpeed: the tactical camera moves faster the higher it is, and faster again with Shift.
#include "BattleController.h"
#include "Framework/Application/SlateApplication.h"
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleTerrain.h"
#include "BattleUnit.h"
#include "FutureBattle.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

bool ABattleController::PickOrderPoint(FVector2D Screen,FVector& Out,int32* Level)
{
 if(Level)*Level=0;
 FHitResult Hit;const bool bHit=GetHitResultAtScreenPosition(Screen,ECC_Visibility,false,Hit);if(bHit)Out=Hit.ImpactPoint;
 const auto& Heights=BattleFieldLayout::Of(GetWorld()).Heights;
 FVector Origin,Dir;
 if(Heights.IsEmpty()||!UGameplayStatics::DeprojectScreenToWorld(this,Screen,Origin,Dir)||Dir.Z>-.05)return bHit;
 const bool bLower=bGroundFloorPending||IsInputKeyDown(EKeys::LeftAlt)||IsInputKeyDown(EKeys::RightAlt);
 double BestT=DBL_MAX;
 for(const auto& P:Heights.Patches)
 {
  // Flat raised levels only; beneath a solid one (a hill) there is nothing to stand on.
  if(P.Axis!=0||P.Z0<BattleTerrain::StepMax||(bLower&&P.bSolid))continue;
  // The ground floor under an upper floor is FloorZ below it (height 0 on the flat maps; the house's pad on map L).
  const double T=((bLower?double(P.Z0)-BattleTerrain::FloorZ:double(P.Z0))-Origin.Z)/Dir.Z;if(T<=0||T>=BestT)continue;
  const FVector At=Origin+Dir*T;if(!P.Contains(At.X,At.Y))continue;
  BestT=T;Out=At;if(Level)*Level=P.bSolid?0:bLower?1:2;
 }
 return bHit||BestT<DBL_MAX;
}

void ABattleController::BeginPathDraw()
{
 bPathDraw=false;bFrontDraw=false;DrawnPath.Reset();DrawnLength=0;
 // Ctrl + right drag draws the own front line (user request 2026-11-21 after test play); no selection needed.
 // Slate's modifier state as well: it holds even when the key's press went to another handler first.
 const bool bCtrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl)||(FSlateApplication::IsInitialized()&&FSlateApplication::Get().GetModifierKeys().IsControlDown());
 // The command bar's "前線を引く" button arms the same draw for the next right drag (user 2026-11-23: the Ctrl draw was never logged in test play).
 if(bTactical&&(bCtrl||bFrontDrawArmed))
 {
  bFrontDrawArmed=false;
  FVector2D Pointer;FVector At;
  if(GetPointerPosition(Pointer)&&PickOrderPoint(Pointer,At)){bPathDraw=bFrontDraw=true;DrawnPath.Add(At);PathScreenStart=Pointer;}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT FrontDrawBegin ctrl=%d started=%d"),bCtrl?1:0,bFrontDraw?1:0);
  return;
 }
 if(!bTactical||Selected.IsEmpty())return;
 FVector2D Pointer;FVector At;
 if(GetPointerPosition(Pointer)&&PickOrderPoint(Pointer,At)){bPathDraw=true;DrawnPath.Add(At);PathScreenStart=Pointer;}
}

void ABattleController::UpdatePathDraw()
{
 if(!bPathDraw)return;
 if(!bTactical||(!bFrontDraw&&Selected.IsEmpty())){bPathDraw=false;DrawnPath.Reset();return;}
 FVector2D Pointer;FVector At;
 if(!GetPointerPosition(Pointer)||!PickOrderPoint(Pointer,At)||DrawnPath.Num()>=96)return;
 const double Step=FVector::Dist2D(At,DrawnPath.Last());
 if(Step>=1000){DrawnLength+=Step;DrawnPath.Add(At);}
}

void ABattleController::EndPathDraw()
{
 if(!bPathDraw)return;
 bPathDraw=false;
 TArray<FVector> Drawn=MoveTemp(DrawnPath);DrawnPath.Reset();
 if(bFrontDraw)
 {
  bFrontDraw=false;if(!bTactical)return;
  FVector2D Pointer=PathScreenStart;FVector End;
  if(GetPointerPosition(Pointer)&&PickOrderPoint(Pointer,End))Drawn.Add(End);
  // A Ctrl + right click (no drag) hands the line back to the front army.
  if(FVector2D::Distance(Pointer,PathScreenStart)<=12.)Drawn.Reset();
  const int32 Rows=GetWorld()->GetSubsystem<UBattleMatch>()->SetDrawnFrontLine(0,Drawn);
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT FrontDraw points=%d result=%d"),Drawn.Num(),Rows);
  // The tactical camera looks toward +X, so rows run up and down the screen and a front line is drawn left to right
  // (test play 2026-11-22: the old hint said "vertically", which crosses no row).
  ShowNotice(Rows>0&&GetWorld()->GetSubsystem<UBattleMatch>()->FrontSectionOrdersOn(0)?FString::Printf(TEXT("目標の線を引きました（%d行）。作戦方針（U）で「前進」「後退」にした区間がこの線へ動きます"),Rows)
   :Rows>0?FString::Printf(TEXT("前線を引きました（%d行）。Ctrl+右クリックで前線軍に任せます"),Rows)
   :Rows==0?FString(TEXT("前線を前線軍に任せました")):Rows==-2?FString(TEXT("前線は敵陣に向かって横切るように（左右に）引いてください"))
   :FString(TEXT("前線軍がありません（部隊を前線軍タブへ入れてください）")));
  return;
 }
 if(!bTactical||Selected.IsEmpty())return;
 FVector2D Pointer=PathScreenStart;FVector End;int32 Level=0;
 if(!GetPointerPosition(Pointer)||!PickOrderPoint(Pointer,End,&Level)){if(Drawn.IsEmpty())return;End=Drawn.Last();}
 bGroundFloorPending=false; // Consume once, after all points of this order have used the same floor selection.
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const double Length=DrawnLength+(Drawn.Num()?FVector::Dist2D(End,Drawn.Last()):0.);
 const bool bSectorMove=UBattleLeaderRuntime::SectorMoveEnabled() && BattleFieldLayout::Of(GetWorld()).bSectors && PendingOrder==EBattleOrder::Move;
 const bool bScreenDrag=FVector2D::Distance(Pointer,PathScreenStart)>12.;
 if((bSectorMove?!bScreenDrag:((BattleFieldLayout::Of(GetWorld()).bSectors && PendingOrder!=EBattleOrder::Move)||Length<2500))||!L->IsEnabled()||!L->IsWideField())
 {
  // A click (or a drag too short to be a way): the ordinary order at the point.
  IssueOrderAt(End,PendingOrder);
  if(Level)ShowNotice(Level==2?TEXT("命令先：2階"):TEXT("命令先：1階"));
  PendingOrder=EBattleOrder::Move;return;
 }
 // Way points about 30m apart; the end is always the last one.
 TArray<FVector> Points;
 for(const FVector& P:Drawn)if(Points.IsEmpty()||FVector::Dist2D(P,Points.Last())>=3000)Points.Add(P);
 while(Points.Num()&&FVector::Dist2D(Points.Last(),End)<1500)Points.Pop();
 Points.Add(End);
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 int32 Accepted=0,Rejected=0;TSet<int32> Done;
 for(ABattleUnit* U:Selected)if(LeaderSelectable(U))
 {
  const int32 SID=U->GetState()->SquadID;if(Done.Contains(SID))continue;Done.Add(SID);
  if(Points.Num()>1?Match->PlayerPathOrder(SID,PendingOrder,Points)>0:L->IssueOrder(SID,PendingOrder,End))
  {if(Points.Num()==1)Match->ClearPlayerRouteMission(SID);++Accepted;}else ++Rejected;
 }
 ShowNotice(Points.Num()>1||!bSectorMove?FString::Printf(TEXT("道筋の命令：%d部隊（中継点 %d／不可 %d）"),Accepted,FMath::Max(0,Points.Num()-1),Rejected)
  :FString::Printf(TEXT("地点へ移動：%d部隊（不可 %d）"),Accepted,Rejected));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT DrawnPath points=%d length_m=%.0f accepted=%d rejected=%d"),Points.Num(),Length/100.,Accepted,Rejected);
 PendingOrder=EBattleOrder::Move;
}

float ABattleController::PanSpeed() const
{
 const float Height=TacticalPawn&&TacticalPawn->Camera?TacticalPawn->Camera->GetRelativeLocation().Z:1000.f;
 const bool bFast=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
 return FMath::Max(1800.f,Height*1.5f)*(bFast?2.5f:1.f);
}

bool ABattleController::TryVault(ABattleUnit* Unit)
{
 auto* Move=Unit?Unit->GetCharacterMovement():nullptr;
 if(!Move||!Move->IsMovingOnGround()||VaultUnit)return false;
 const float Half=Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Unit->GetCapsuleComponent()->GetScaledCapsuleRadius();
 const FVector Centre=Unit->GetActorLocation(),Feet=Centre-FVector(0,0,Half),Dir=FRotator(0,GetControlRotation().Yaw,0).Vector();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleVault),false,Unit);
 UWorld* World=GetWorld();
 // An obstacle ahead at knee height, and its top just behind the face.
 FHitResult Wall;
 if(!World->LineTraceSingleByChannel(Wall,Feet+FVector(0,0,50),Feet+FVector(0,0,50)+Dir*(Radius+90),ECC_Visibility,Query)||Cast<APawn>(Wall.GetActor()))return false;
 const FVector Over=Wall.ImpactPoint+Dir*12;FHitResult Top;
 if(!World->LineTraceSingleByChannel(Top,FVector(Over.X,Over.Y,Feet.Z+175),FVector(Over.X,Over.Y,Feet.Z+40),ECC_Visibility,Query)||Top.bStartPenetrating)return false;
 const double Height=Top.ImpactPoint.Z-Feet.Z;
 if(Height<50||Height>160)return false;
 // Room to pass over the top (the body goes over low), and the nearest free place to stand behind it.
 const FCollisionShape Body=FCollisionShape::MakeCapsule(Radius,Half),Low=FCollisionShape::MakeSphere(Radius*.8f);
 for(const double Beyond:{70.,100.,130.,170.,210.})
 {
  const FVector Past=Wall.ImpactPoint+Dir*Beyond;
  const FVector PassA(Centre.X,Centre.Y,Feet.Z+Height+Radius+6),PassB(Past.X,Past.Y,Feet.Z+Height+Radius+6);
  if(World->SweepTestByChannel(PassA,PassB,FQuat::Identity,ECC_Visibility,Low,Query))return false;
  // The ground behind: at most 4m down (out of an upper window the leader drops to the ground).
  FHitResult Ground;double LandZ=Feet.Z-400;
  if(World->LineTraceSingleByChannel(Ground,FVector(Past.X,Past.Y,Feet.Z+Height+4),FVector(Past.X,Past.Y,Feet.Z-400),ECC_Visibility,Query))
  {if(Ground.bStartPenetrating)continue;LandZ=Ground.ImpactPoint.Z;}
  const FVector Land(Past.X,Past.Y,LandZ+Half+3);
  if(World->OverlapBlockingTestByChannel(Land,FQuat::Identity,ECC_Pawn,Body,Query))continue;
  VaultUnit=Unit;VaultFrom=Centre;VaultTo=Land;VaultPeak=Feet.Z+Height+Half+8;VaultTime=0;
  VaultDuration=FMath::Clamp(float(.3+Height/400.+FMath::Max(0.,Feet.Z-LandZ)/900.),.35f,.9f);
  Move->StopMovementImmediately();Move->SetMovementMode(MOVE_None);
  return true;
 }
 return false;
}

void ABattleController::UpdateVault(float DeltaTime)
{
 if(!VaultUnit)return;
 ABattleUnit* Unit=VaultUnit;
 auto* Move=Unit->GetCharacterMovement();
 if(!Unit->IsAlive()||GetPawn()!=Unit){if(Move&&Move->MovementMode==MOVE_None)Move->SetMovementMode(MOVE_Falling);VaultUnit=nullptr;return;}
 VaultTime+=DeltaTime;
 const float T=FMath::Clamp(VaultTime/FMath::Max(.05f,VaultDuration),0.f,1.f);
 // Up to the top in the first 45%, across and down in the rest.
 const float Rise=FMath::Clamp(T/.45f,0.f,1.f),Fall=FMath::Clamp((T-.45f)/.55f,0.f,1.f);
 FVector At=FMath::Lerp(VaultFrom,VaultTo,FMath::SmoothStep(.15f,1.f,T));
 At.Z=T<.45f?FMath::Lerp(VaultFrom.Z,FMath::Max(VaultPeak,VaultFrom.Z),FMath::Sin(Rise*HALF_PI)):FMath::Lerp(FMath::Max(VaultPeak,VaultFrom.Z),VaultTo.Z,Fall*Fall);
 Unit->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);
 if(T>=1){if(Move)Move->SetMovementMode(MOVE_Falling);VaultUnit=nullptr;}
}

namespace
{
 // Junctions of a main route, and the way along it from one junction to another (breadth-first over its sections).
 void LaneNodes(const BattleFieldLayout::FMapDef& Def,BattleFieldLayout::ERouteLane Lane,TArray<int32>& On)
 {
  for(const auto& E:Def.RouteEdges)if(E.Lane==Lane){On.AddUnique(E.A);On.AddUnique(E.B);}
 }
 void LanePath(const BattleFieldLayout::FMapDef& Def,BattleFieldLayout::ERouteLane Lane,int32 Start,int32 Exit,TArray<int32>& Path)
 {
  TMap<int32,int32> Prev;TArray<int32> Open;Open.Add(Start);Prev.Add(Start,INDEX_NONE);
  for(int32 Head=0;Head<Open.Num()&&!Prev.Contains(Exit);++Head)
   for(const auto& E:Def.RouteEdges)if(E.Lane==Lane)
   {
    const int32 U=Open[Head],V=E.A==U?E.B:E.B==U?E.A:INDEX_NONE;
    if(V!=INDEX_NONE&&!Prev.Contains(V)){Prev.Add(V,U);Open.Add(V);}
   }
  if(Prev.Contains(Exit))for(int32 I=Exit;I!=INDEX_NONE;I=Prev[I])Path.Insert(I,0);
 }
}

int32 UBattleMatch::PlayerPathOrder(int32 SquadID,EBattleOrder Order,const TArray<FVector>& Points)
{
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const auto* S=Store->FindSquad(SquadID);
 if(Points.Num()<2||Order==EBattleOrder::Hold||!Runtime||!Runtime->IsWideField()||!S||!Store->Find(S->LeaderUnitID))return 0;
 const EBattleOrder OnWay=Order==EBattleOrder::Move?EBattleOrder::Move:EBattleOrder::AttackMove;
 // The first point that takes an order (a point drawn on a roof or in a wall is skipped).
 for(int32 I=0;I<Points.Num();++I)
 {
  const bool bLast=I+1==Points.Num();
  if(!Runtime->IssueOrder(SquadID,bLast?Order:OnWay,Points[I]))continue;
  FPlayerRoute& R=PlayerRoutes.FindOrAdd(SquadID);
  R.bMission=!bLast;R.bPath=!bLast;R.Order=Order;R.Goal=Points.Last();R.Step=Points[I];R.Waypoints.Reset();
  for(int32 J=I+1;J+1<Points.Num();++J)R.Waypoints.Add(Points[J]);
  R.StepDeadline=Elapsed+20.f+float(FVector::Dist2D(Runtime->Position(S->LeaderUnitID),Points[I])/250.);
  PlayerRouteClock=0;
  return 1;
 }
 return 0;
}

bool UBattleMatch::PlayerPathNext(FPlayerRoute& R,const FVector& Leader,FVector& Step) const
{
 // The next point once the leader is at the ordered one or has taken far too long to reach it.
 const FVector& Next=R.Waypoints.Num()?R.Waypoints[0]:R.Goal;
 // (Within 25m: the leader's own arrival place may be a cover slot beside the point. No "already past it" rule: a drawn way
 // may turn back sharply, and such a rule skipped the first point of a zigzag.)
 const bool bThere=FVector::Dist2D(Leader,R.Step)<2500||Elapsed>R.StepDeadline;
 if(!bThere)return false;
 Step=Next;return true;
}

bool UBattleMatch::PlayerRoutePreview(int32 SquadID,TArray<FVector>& Out) const
{
 Out.Reset();
 const FPlayerRoute* R=PlayerRoutes.Find(SquadID);
 if(!R||!R->bMission)return false;
 if(R->bPath){Out=R->Waypoints;Out.Add(R->Goal);return true;}
 const auto& Def=BattleFieldLayout::Of(GetWorld());
 if(R->Lane>=1&&R->Lane<=3&&R->Phase<2)
 {
  const auto Lane=BattleFieldLayout::ERouteLane(R->Lane-1);
  TArray<int32> On;LaneNodes(Def,Lane,On);
  if(On.Num()>=2)
  {
   // From the junction the squad is walking to, along the route to its junction nearest the goal.
   int32 Exit=On[0],Start=On[0];double Best=DBL_MAX;
   for(int32 I:On){const double D=FVector::DistSquared2D(R->Goal,Def.RouteNodes[I]);if(D<Best){Best=D;Exit=I;}}
   Best=DBL_MAX;
   for(int32 I:On){const double D=FVector::DistSquared2D(R->Step,Def.RouteNodes[I]);if(D<Best){Best=D;Start=I;}}
   TArray<int32> Path;LanePath(Def,Lane,Start,Exit,Path);
   for(int32 I:Path)if(FVector::Dist2D(Def.RouteNodes[I],R->Step)>1000)Out.Add(FVector(Def.RouteNodes[I].X,Def.RouteNodes[I].Y,0));
  }
 }
 if(!R->Step.Equals(R->Goal,1))Out.Add(R->Goal);
 return Out.Num()>0;
}

// The predicted way of the selected squads (tactical view): the path the leader is walking now, then the points a routed or
// drawn order still has ahead, on the ground and on the minimap. Also the line being drawn with the right button.
#include "BattleHUD.h"
#include "Engine/Canvas.h"
void ABattleHUD::DrawOrderWays()
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();if(!L->IsEnabled())return;
 auto* PC=Cast<ABattleController>(PlayerOwner);if(!PC||!Canvas||!PC->IsTactical())return;
 const float WaysBottom=FBattleCommandBarLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld()).PanelTop-2.f;const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const auto Map=FBattleMinimapLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld());
 auto Screen=[&](FVector World,FVector2D& Out){return PC->ProjectWorldLocationToScreen(World,Out)&&Out.Y>110&&Out.Y<WaysBottom&&!Map.Contains(Out);};
 auto Line=[&](const TArray<FVector>& Points,FLinearColor Color,float Thickness)
 {
  for(int32 I=0;I+1<Points.Num();++I)
  {
   const FVector A=Points[I]+FVector(0,0,40),B=Points[I+1]+FVector(0,0,40);
   // Projected in pieces of about 15m so a line that leaves the screen is still drawn where it is visible.
   const int32 Pieces=FMath::Clamp(FMath::CeilToInt(FVector::Dist(A,B)/1500.),1,12);
   FVector2D P0;bool bPrev=Screen(A,P0);
   for(int32 J=1;J<=Pieces;++J){FVector2D P1;const bool bNow=Screen(FMath::Lerp(A,B,J/float(Pieces)),P1);if(bPrev&&bNow)DrawLine(P0.X,P0.Y,P1.X,P1.Y,Color,Thickness);P0=P1;bPrev=bNow;}
  }
  if(Map.Size.X>0)for(int32 I=0;I+1<Points.Num();++I)
  {const FVector2D A=Map.ToScreen(Points[I]),B=Map.ToScreen(Points[I+1]);if(Map.Contains(A)&&Map.Contains(B))DrawLine(A.X,A.Y,B.X,B.Y,Color,1);}
 };
 TSet<int32> Done;int32 Drawn=0;
 for(const auto& Unit:PC->GetSelection())
 {
  if(!Unit||!Unit->GetState()||Drawn>=24)continue;const int32 SID=Unit->GetState()->SquadID;if(Done.Contains(SID))continue;Done.Add(SID);
  const auto* S=Store->FindSquad(SID);if(!S||S->CommandStatus!=EBattleCommandStatus::Active)continue;
  const EBattleOrder Order=S->bHasMission?S->MissionOrder:S->Order;if(Order==EBattleOrder::Hold)continue;
  const FLinearColor Color=Order==EBattleOrder::AttackMove?FLinearColor(1,.6f,.2f,.9f):Order==EBattleOrder::Defend?FLinearColor(.45f,.7f,1,.9f):FLinearColor(.28f,.95f,.8f,.9f);
  const FVector Leader=L->Position(S->LeaderUnitID);
  // Now: from the leader along what is left of his path.
  TArray<FVector> Now;Now.Add(FVector(Leader.X,Leader.Y,Leader.Z-90));
  if(const auto* Plan=L->PlanOf(SID);Plan&&Plan->Path.Num()>1)
  {
   int32 Nearest=0;double Best=DBL_MAX;
   for(int32 I=0;I+1<Plan->Path.Num();++I){const double D=FMath::PointDistToSegmentSquared(Leader,Plan->Path[I],Plan->Path[I+1]);if(D<Best){Best=D;Nearest=I;}}
   for(int32 I=Nearest+1;I<Plan->Path.Num();++I)Now.Add(Plan->Path[I]);
  }
  else Now.Add(S->Destination);
  if(Now.Num()<2||FVector::Dist2D(Now[0],Now.Last())<300)continue;
  Line(Now,Color,2.f);++Drawn;
  // Later: the junctions or drawn points still ahead, and the goal with its mark.
  TArray<FVector> Later;
  if(Match->PlayerRoutePreview(SID,Later))
  {
   Later.Insert(Now.Last(),0);Line(Later,Color*FLinearColor(1,1,1,.55f),1.5f);
   FVector2D G;if(Screen(Later.Last()+FVector(0,0,25),G)){DrawLine(G.X,G.Y-9,G.X+9,G.Y,Color,2);DrawLine(G.X+9,G.Y,G.X,G.Y+9,Color,2);DrawLine(G.X,G.Y+9,G.X-9,G.Y,Color,2);DrawLine(G.X-9,G.Y,G.X,G.Y-9,Color,2);}
   for(int32 I=1;I+1<Later.Num();++I){FVector2D P;if(Screen(Later[I]+FVector(0,0,25),P))DrawRect(Color*FLinearColor(1,1,1,.8f),P.X-3,P.Y-3,6,6);}
  }
 }
 // The line being drawn.
 if(PC->IsDrawingPath()&&PC->GetDrawnPath().Num())
 {
  TArray<FVector> Draft=PC->GetDrawnPath();FVector2D Pointer;FVector At;
  if(PC->GetPointerPosition(Pointer)&&PC->PickOrderPoint(Pointer,At))Draft.Add(At);
  Line(Draft,PC->IsDrawingFrontLine()?FLinearColor(1.f,.55f,.15f,.95f):FLinearColor(1.f,.85f,.2f,.95f),PC->IsDrawingFrontLine()?4.f:2.5f);
 }
}
