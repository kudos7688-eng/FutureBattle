#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "BattleBlindFire.h"
#include "BattleBodyAnim.h"
#include "BattleFieldLayout.h"
#include "BattlePerception.h"
#include "BattleGameMode.h"
#include "BattleVATDisplay.h"
#include "BattleSquadTactics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace {
bool PeekRangeCheck()
{
 static const bool Enabled=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattlePeekRangeCheck="),V);return V!=0;}(); // trial: projected peeks must be 100..450 cm from their anchor
 return Enabled;
}
}

void UBattleLeaderRuntime::CancelCoverAction(int32 ID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U)return;
 if(U->CoverPhase==4)EndPeekDiag(ID,U->IsAlive()?TEXT("cancel"):TEXT("died"));
 if(U->CoverPhase){GetWorld()->GetSubsystem<UBattleCover>()->Release(ID);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_RELEASE unit=%d phase=%d position=%s remaining=%.2f revision=%d/%d"),ID,U->CoverPhase,*Position(ID).ToCompactString(),U->CoverSeconds,U->OrderRevision,U->CoverOrderRevision);}
 U->CoverPhase=0;U->CoverSeconds=0;U->CoverTargetID=INDEX_NONE;U->CoverSlot=INDEX_NONE;
 // A near follower showing a cover clip returns to his ordinary clip and facing.
 if(auto* B=Bodies.Find(ID);B && B->CoverClipPhase && !B->Actor.IsValid() && B->Mesh.IsValid())
 {
  B->CoverClipPhase=0;B->bBlindPose=false;
  // Back into the capsule from a lean past the edge (blind fire around it), alive or dead.
  B->Mesh->SetRelativeLocation(FVector(0,0,-90));
  if(U->IsAlive()){B->Mesh->SetRelativeRotation(FRotator(0,-90,0));RefreshFollowerPose(ID);}
 }
}

void UBattleLeaderRuntime::EndPeekDiag(int32 ID,const TCHAR* Cause)
{
 int32 Shots=0;if(!CoverPeekShots.RemoveAndCopyValue(ID,Shots))return;
 SightExperiment.Cover(*FString::Printf(TEXT("peek_end_%s"),Cause));
 SightExperiment.Cover(Shots<=0?TEXT("peek_end_shots_0"):Shots==1?TEXT("peek_end_shots_1"):TEXT("peek_end_shots_2plus"));
}

bool UBattleLeaderRuntime::CoverLineClear(int32 ID,FVector From,FVector Threat) const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CoverPeekLine),false);
 if(const auto* B=Bodies.Find(ID))Q.AddIgnoredComponent(B->Capsule.Get());
 FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,From+FVector(0,0,60),Threat+FVector(0,0,35),ECC_Visibility,Q);
 return !Hit.bBlockingHit || HitIDs.Contains(Hit.GetComponent());
}

bool UBattleLeaderRuntime::RevalidatePeek(int32 ID,FVector Threat)
{
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);if(!U)return false;
 const double ExistingDistance=PeekRangeCheck()?FVector::Dist2D(U->CoverAnchor,U->CoverPeek):0.;
 if((!PeekRangeCheck()||ExistingDistance==0.||(ExistingDistance>=100.&&ExistingDistance<=450.))&&CoverLineClear(ID,U->CoverPeek,Threat))return true;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 const FVector Anchor=U->CoverAnchor,Lateral=FVector::CrossProduct((Threat-Anchor).GetSafeNormal2D(),FVector::UpVector);
 for(float Offset:{-220.f,220.f,-400.f,400.f})
 {
  FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor+Lateral*Offset-FVector(0,0,90),Projected,FVector(50,50,150)))continue;
  const FVector Peek=Projected.Location+FVector(0,0,90);
  if(PeekRangeCheck()){const double D=FVector::Dist2D(Anchor,Peek);if(D<100.||D>450.)continue;}
  if(GetWorld()->SweepTestByObjectType(Anchor,Peek,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88)))continue;
  if(!CoverLineClear(ID,Peek,Threat))continue;
  U->CoverPeek=Peek;SightExperiment.Cover(TEXT("peek_repicked"));return true;
 }
 return false;
}

bool UBattleLeaderRuntime::RequestCoverAction(int32 ID,FVector Threat,int32 Target,float SearchRadius)
{
 if(!bEnabled || !BattleTactics::CoverActionsEnabled() || !Bodies.Contains(ID) || Threat.ContainsNaN())return false;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase)return false;
 const auto* Squad=Store->FindSquad(U->SquadID);if(!Squad || Squad->CommandStatus!=EBattleCommandStatus::Active)return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 const FVector Pos=Position(ID),Eye=Threat+FVector(0,0,60);const double Now=GetWorld()->GetTimeSeconds();
 const FVector Lateral=FVector::CrossProduct((Threat-Pos).GetSafeNormal2D(),FVector::UpVector);
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 auto Clear=[&](FVector A,FVector B){return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88));};
 const auto* Tuning=GetDefault<UBattleTuning>();
 // Evaluation-only funnel: furthest stage reached by this request (does not alter the search order or result).
 auto& Diag=SightExperiment;int32 Stage=0;
 static const TCHAR* StageKeys[]={TEXT("request_no_slot_in_radius"),TEXT("request_slot_reserved"),TEXT("request_path_to_anchor_blocked"),TEXT("request_anchor_too_exposed"),TEXT("request_no_valid_peek"),TEXT("request_reserve_failed")};
 Diag.Cover(TEXT("request_total"));if(Diag.Enabled)LastCoverStage.Add(ID,-1);
 for(int32 Slot:Cover->NearbySlots(Pos,FMath::Max(0.f,SearchRadius>=0?SearchRadius:Tuning->CoverActionSearchRadius),BattleFieldLayout::IsUrban(GetWorld())?12:FMath::Max(1,Tuning->CoverActionCandidateLimit)))
 {
  const FVector Anchor=Cover->GetSlots()[Slot]+FVector(0,0,90);
  Stage=FMath::Max(Stage,1);if(!Cover->IsAvailable(Slot,ID,Now))continue;
  Stage=FMath::Max(Stage,2);if(!Clear(Pos,Anchor))continue;
  Stage=FMath::Max(Stage,3);
  const float Hidden=BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction();
  if(Hidden>CoverExposureLimit())continue;
  Stage=FMath::Max(Stage,4);
  // -BattleContactCover: wider rules for the firing point (ContactFirePoint).
  if(BattleTactics::ContactCover(U->Team) && !(Bodies.FindChecked(ID).Actor.IsValid() && Bodies.FindChecked(ID).Actor->IsPlayerControlled()))
  {
   FVector Point;
   if(!ContactFirePoint(ID,Anchor,Threat,Point))continue;
   Stage=FMath::Max(Stage,5);
   if(!Cover->Reserve(Slot,ID,Now))continue;
   Diag.Cover(TEXT("request_success"));
   U->CoverAnchor=Anchor;U->CoverPeek=Point;U->CoverSlot=Slot;U->CoverPhase=1;U->CoverSeconds=3;
   U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=Target;U->DecisionReason=TEXT("COVER / FIRING POSITION");
   return true;
  }
  // Stances: low cover (a wall, a car, sandbags) is used by crouching behind it and standing up to fire, with no step
  // sideways. It qualifies when the threat cannot see the top of a crouched soldier here but a standing one can see the threat.
  if(BattleStance::Enabled(GetWorld()) && !BattleTactics::HasTerrainLine(GetWorld(),Anchor+FVector(0,0,BattleStance::TopOffset(BattleStance::Crouch)),Eye)
     && BattleTactics::HasTerrainLine(GetWorld(),Anchor+FVector(0,0,60),Eye))
  {
   Stage=FMath::Max(Stage,5);
   if(!Cover->Reserve(Slot,ID,Now))continue;
   Diag.Cover(TEXT("request_success"));Diag.Cover(TEXT("request_success_stand_up"));
   U->CoverAnchor=Anchor;U->CoverPeek=Anchor;U->CoverSlot=Slot;U->CoverPhase=1;U->CoverSeconds=3;
   U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=Target;U->DecisionReason=TEXT("COVER / CROUCH BEHIND LOW COVER");
   return true;
  }
  for(float Offset:{-220.f,220.f,-400.f,400.f})
  {
   FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor+Lateral*Offset-FVector(0,0,90),Projected,FVector(50,50,150))){Diag.Cover(TEXT("peek_off_navmesh"));continue;}
   const FVector Peek=Projected.Location+FVector(0,0,90);
   if(PeekRangeCheck()){const double D=FVector::Dist2D(Anchor,Peek);if(D<100.||D>450.)continue;}
   if(!Clear(Anchor,Peek)){Diag.Cover(TEXT("peek_path_blocked"));continue;}
   if(BattleTactics::ExposureAt(GetWorld(),Peek,Forward(ID),Eye).Fraction()<Hidden+.39f){Diag.Cover(TEXT("peek_not_exposed_enough"));continue;}
   if(!BattleTactics::HasTerrainLine(GetWorld(),Peek+FVector(0,0,60),Eye)){Diag.Cover(TEXT("peek_no_line_to_threat"));continue;}
   Stage=FMath::Max(Stage,5);
   if(!Cover->Reserve(Slot,ID,Now))continue;
   Diag.Cover(TEXT("request_success"));
   U->CoverAnchor=Anchor;U->CoverPeek=Peek;U->CoverSlot=Slot;U->CoverPhase=1;U->CoverSeconds=3;
   U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=Target;U->DecisionReason=TEXT("COVER / ENTER PHYSICAL SHELTER");
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COVER_ACTION unit=%d slot=%d hidden=%.2f peek_cm=%.1f"),ID,Slot,Hidden,FVector::Dist2D(Anchor,Peek));return true;
  }
 }
 Diag.Cover(StageKeys[Stage]);if(Diag.Enabled)LastCoverStage.Add(ID,(int8)Stage);
 return false;
}

bool UBattleLeaderRuntime::ContactFirePoint(int32 ID,FVector Anchor,FVector Threat,FVector& Peek)
{
 const FVector Eye=Threat+FVector(0,0,60);
 const float Hidden=BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction();
 if(Hidden>CoverExposureLimit())return false;
 // Fire from the slot itself: standing up from behind low cover, or from a place that covers most of the body.
 if(BattleTactics::HasTerrainLine(GetWorld(),Anchor+FVector(0,0,60),Eye))
 {
  if(!BattleStance::Enabled(GetWorld()))return false; // without stances he could never get out of sight here
  Peek=Anchor;SightExperiment.Cover(BattleTactics::HasTerrainLine(GetWorld(),Anchor+FVector(0,0,BattleStance::TopOffset(BattleStance::Crouch)),Eye)?TEXT("contact_point_partial"):TEXT("contact_point_stand_up"));return true;
 }
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 const FVector Lateral=FVector::CrossProduct((Threat-Anchor).GetSafeNormal2D(),FVector::UpVector);
 for(float Offset:{-100.f,100.f,-160.f,160.f,-220.f,220.f,-300.f,300.f,-400.f,400.f})
 {
  FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Anchor+Lateral*Offset-FVector(0,0,90),Projected,FVector(50,50,150)))continue;
  const FVector Point=Projected.Location+FVector(0,0,90);
  if(GetWorld()->SweepTestByObjectType(Anchor,Point,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88)))continue;
  if(!BattleTactics::HasTerrainLine(GetWorld(),Point+FVector(0,0,60),Eye))continue;
  if(const double Step=FVector::Dist2D(Anchor,Point);Step<100.||Step>450.)continue; // the range a saved cover action may hold
  Peek=Point;SightExperiment.Cover(TEXT("contact_point_step"));return true;
 }
 return false;
}

bool UBattleLeaderRuntime::SquadCanHide(int32 SID,FVector Threat,double Now) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SID);const auto* P=Plans.Find(SID);
 if(!S||!P)return false;
 int32 Living=0;for(int32 ID:S->Members)if(Store->Find(ID)->IsAlive())++Living;
 if(!Living)return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 // Soldiers of the team's other stopping squads near by who are not in cover yet want the same places.
 int32 Waiting=0;
 for(const auto& Other:Plans)if(Other.Key!=SID && Other.Value.bCoverStop && FVector::DistSquared2D(Other.Value.Center,P->Center)<FMath::Square(3000.))
  if(const auto* OS=Store->FindSquad(Other.Key);OS && OS->Team==S->Team)
   for(int32 ID:OS->Members)if(const auto* M=Store->Find(ID);M && M->IsAlive() && !M->CoverPhase && !CoverApproaches.Contains(ID))++Waiting;
 auto Claimed=[&](const FVector& At){for(const auto& A:CoverApproaches)if(FVector::DistSquared2D(A.Value.Goal,At)<FMath::Square(100.))return true;
  for(const auto& F:FirePositions)if(FVector::DistSquared2D(F.Value,At)<FMath::Square(100.))return true;return false;};
 const FVector Eye=Threat+FVector(0,0,60);int32 Hide=-Waiting;
 for(int32 Slot:Cover->NearbySlots(P->Center,2000.f,32))
 {
  const FVector At=Cover->GetSlots()[Slot];
  if(BattleTactics::ExposureAt(GetWorld(),At+FVector(0,0,90),FVector::ForwardVector,Eye).VisibleSamples>2)continue;
  if(Cover->IsAvailable(Slot,S->LeaderUnitID,Now) && !Claimed(At))++Hide;
  // Two places right behind the slot (seen from the threat), as members without a slot of their own would use them.
  const FVector Back=(At-Threat).GetSafeNormal2D(),Side(-Back.Y,Back.X,0);
  if(BattleTactics::ExposureAt(GetWorld(),At+Back*130+FVector(0,0,90),FVector::ForwardVector,Eye).VisibleSamples<=2)
   for(const float Off:{-110.f,110.f})if(!Claimed(At+Back*130+Side*Off))++Hide;
  if(Hide*5>=Living*4)return true;
 }
 return Hide*5>=Living*4;
}

bool UBattleLeaderRuntime::StartHuddle(int32 ID,FVector Threat,int32 Target,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase || Threat.ContainsNaN() || !Bodies.Contains(ID) || CoverApproaches.Contains(ID))return false;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 const FVector Pos=Position(ID),Eye=Threat+FVector(0,0,60);
 int32 Tried=0;
 for(int32 Slot:Cover->NearbySlots(Pos,2000.f,24))
 {
  const FVector At=Cover->GetSlots()[Slot];
  const FVector Back=(At-Threat).GetSafeNormal2D(),Side(-Back.Y,Back.X,0);
  for(const float Off:{0.f,-110.f,110.f})
  {
   const FVector Want=At+Back*130+Side*Off;
   bool Taken=false;
   for(const auto& A:CoverApproaches)if(FVector::DistSquared2D(A.Value.Goal,Want)<FMath::Square(100.)){Taken=true;break;}
   if(!Taken)for(const auto& F:FirePositions)if(F.Key!=ID && FVector::DistSquared2D(F.Value,Want)<FMath::Square(100.)){Taken=true;break;}
   if(Taken)continue;
   FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Want-FVector(0,0,90),Projected,FVector(40,40,150)) || FVector::Dist2D(Projected.Location,Want)>40)continue;
   const FVector Point=Projected.Location+FVector(0,0,90);
   if(BattleTactics::ExposureAt(GetWorld(),Point,FVector::ForwardVector,Eye).VisibleSamples>2)continue;
   if(++Tried>8){SightExperiment.Cover(TEXT("huddle_none"));return false;}
   if(GetWorld()->SweepTestByObjectType(Pos,Point,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88)))continue;
   FCoverApproach A;A.Goal=Point;A.Target=Target;A.Revision=U->OrderRevision;A.Until=Now+8.;A.bFree=true;A.bStandUp=false;
   CoverApproaches.Add(ID,A);FirePositions.Remove(ID);SightExperiment.Cover(TEXT("huddle_started"));return true;
  }
 }
 SightExperiment.Cover(TEXT("huddle_none"));return false;
}

bool UBattleLeaderRuntime::StartCoverApproach(int32 ID,FVector Threat,int32 Target,double Now,float LocalRadius)
{
 auto& Diag=SightExperiment;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase || Threat.ContainsNaN() || !Bodies.Contains(ID))return false;
 const auto* Squad=Store->FindSquad(U->SquadID);if(!Squad || Squad->Order==EBattleOrder::Hold || Squad->Order==EBattleOrder::Move)return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 const auto* T=GetDefault<UBattleTuning>();
 const FVector Pos=Position(ID),Eye=Threat+FVector(0,0,60);
 const double Near=FMath::Max(0.f,LocalRadius),Base=FVector::Dist2D(Pos,Threat);
 auto Clear=[&](FVector A,FVector B){return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88));};
 // -BattleContactCover: look further, and only for a slot the soldier could fire from once he is there.
 const bool bContactCover=BattleTactics::ContactCover(U->Team);
 // Trial -BattleSectorContact=1: shot at from outside the sector, cover anywhere nearby will do (searched as -BattleContactCover does).
 const auto* OwnPlan=Plans.Find(U->SquadID);const bool OutsideContact=OwnPlan && OwnPlan->bSectorContact;
 // -BattleSectorCoverWalk: fighting from inside his own sector, he searches the sector as widely.
 const bool InSector=SectorCoverWalkEnabled() && Squad->bSectorMission && BattleSectors::Cell(Pos)==BattleSectors::Cell(Squad->Destination);
 const bool bWide=bContactCover||OutsideContact||InSector;
 const int32 Limit=bWide?16:FMath::Max(1,T->AICoverApproachCandidates);int32 Evaluated=0;
 for(int32 Slot:Cover->NearbySlots(Pos,bWide?2000.f:FMath::Max(0.f,T->AICoverApproachRadius),Limit+32))
 {
  const FVector Anchor=Cover->GetSlots()[Slot]+FVector(0,0,90);
  if(FVector::Dist2D(Pos,Anchor)<=Near)continue; // Already covered by the local cover search.
  if(++Evaluated>Limit)break;
  if(!Cover->IsAvailable(Slot,ID,Now))continue;
  bool Taken=false;for(const auto& Other:CoverApproaches)if(Other.Key!=ID && Other.Value.Slot==Slot){Taken=true;break;}
  if(Taken)continue;
  if(FVector::Dist2D(Anchor,Threat)<Base-T->AICoverApproachMaxAdvance)continue;
  if(Squad->bSectorMission && Squad->TacticalPlan.Action==EBattleSquadTactic::Mission ? (!OutsideContact && BattleSectors::Cell(Anchor)!=BattleSectors::Cell(Squad->Destination)) : Squad->Order==EBattleOrder::Defend && FVector::Dist2D(Anchor,Squad->Destination)>Squad->DefenseRadius)continue;
  if(!Clear(Pos,Anchor))continue;
  FVector FirePoint;
  if(bWide?!ContactFirePoint(ID,Anchor,Threat,FirePoint):BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction()>CoverExposureLimit())continue;
  FCoverApproach A;A.Goal=Anchor;A.Slot=Slot;A.Target=Target;A.Revision=U->OrderRevision;A.Until=Now+(bWide?8.f:FMath::Max(0.f,T->AICoverApproachSeconds));
  CoverApproaches.Add(ID,A);Diag.Cover(TEXT("approach_started"));return true;
 }
 Diag.Cover(TEXT("approach_none_found"));return false;
}

bool UBattleLeaderRuntime::StartFirePosition(int32 ID,FVector Threat,int32 Target,double Now)
{
 bFirePositionHereGood=false;
 auto& Diag=SightExperiment;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase || Threat.ContainsNaN() || !Bodies.Contains(ID) || CoverApproaches.Contains(ID))return false;
 const auto* Squad=Store->FindSquad(U->SquadID);if(!Squad || Squad->Order==EBattleOrder::Hold || Squad->Order==EBattleOrder::Move)return false;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 const FVector Pos=Position(ID),Eye=Threat+FVector(0,0,60);
 const double Base=FVector::Dist2D(Pos,Threat);
 const float Here=BattleTactics::ExposureAt(GetWorld(),Pos,Forward(ID),Eye).Fraction();
 const bool bStances=BattleStance::Enabled(GetWorld());
 const auto* OwnPlan=Plans.Find(U->SquadID);const bool OutsideContact=OwnPlan && OwnPlan->bSectorContact;
 // Already out of sight when crouched here: nothing better to look for.
 if(bStances && !BattleTactics::HasTerrainLine(GetWorld(),Pos+FVector(0,0,BattleStance::TopOffset(BattleStance::Crouch)),Eye)){bFirePositionHereGood=true;Diag.Cover(TEXT("fire_position_here_is_good"));return false;}
 struct FPlace{FVector Point;float Score;bool bStandUp;};
 TArray<FPlace> Places;
 int32 Ring=0;
 for(float Radius:{150.f,300.f,500.f,800.f,1200.f})
 {
  for(int32 Step=0;Step<10;++Step)
  {
   const float Angle=FMath::DegreesToRadians(Step*36.f+Ring*18.f);
   const FVector Try=Pos+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius;
   if(FVector::Dist2D(Try,Threat)<Base-200.)continue; // not much closer to the threat
   if(Squad->bSectorMission && Squad->TacticalPlan.Action==EBattleSquadTactic::Mission ? (!OutsideContact && BattleSectors::Cell(Try)!=BattleSectors::Cell(Squad->Destination)) : Squad->Order==EBattleOrder::Defend && FVector::Dist2D(Try,Squad->Destination)>Squad->DefenseRadius)continue;
   FNavLocation Projected;if(!Nav->ProjectPointToNavigation(Try-FVector(0,0,90),Projected,FVector(50,50,150)))continue;
   const FVector Point=Projected.Location+FVector(0,0,90);
   if(!BattleTactics::HasTerrainLine(GetWorld(),Point+FVector(0,0,60),Eye))continue; // he must be able to fire from it
   const bool bStandUp=bStances && !BattleTactics::HasTerrainLine(GetWorld(),Point+FVector(0,0,BattleStance::TopOffset(BattleStance::Crouch)),Eye);
   const float Seen=BattleTactics::ExposureAt(GetWorld(),Point,Forward(ID),Eye).Fraction();
   if(!bStandUp && Seen>Here-.39f)continue; // clearly less of the body in view than here
   bool Taken=false;for(const auto& Other:CoverApproaches)if(FVector::DistSquared2D(Other.Value.Goal,Point)<FMath::Square(120.)){Taken=true;break;}
   if(!Taken)for(const auto& Other:FirePositions)if(Other.Key!=ID && FVector::DistSquared2D(Other.Value,Point)<FMath::Square(120.)){Taken=true;break;}
   if(Taken)continue;
   Places.Add({Point,Seen-(bStandUp?.5f:0.f)+Radius/6000.f,bStandUp});
  }
  ++Ring;
 }
 Places.Sort([](const FPlace& A,const FPlace& B){return A.Score<B.Score;});
 for(int32 I=0;I<Places.Num() && I<4;++I)
 {
  if(GetWorld()->SweepTestByObjectType(Pos,Places[I].Point,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88)))continue;
  FCoverApproach A;A.Goal=Places[I].Point;A.Target=Target;A.Revision=U->OrderRevision;A.Until=Now+8.;A.bFree=true;A.bStandUp=Places[I].bStandUp;
  CoverApproaches.Add(ID,A);FirePositions.Remove(ID);Diag.Cover(Places[I].bStandUp?TEXT("fire_position_started_stand_up"):TEXT("fire_position_started_partial"));return true;
 }
 Diag.Cover(Places.Num()?TEXT("fire_position_blocked"):TEXT("fire_position_none"));return false;
}

bool UBattleLeaderRuntime::StepCoverApproach(int32 ID,float Dt,double Now,int32 Target)
{
 auto* A=CoverApproaches.Find(ID);if(!A)return false;
 if(A->bCharge)return StepCharge(ID,Dt,Now); // a charge at a seen enemy (BattleArms.cpp, -BattleArmsCharge)
 auto& Diag=SightExperiment;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 const bool bContactCover=U && BattleTactics::ContactCover(U->Team);
 const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,A->Target);
 const FVector Pos=Position(ID);const TCHAR* End=nullptr;
 if(!U || !U->IsAlive())End=TEXT("approach_cancel_died");
 // An assault dash (BattleArms.cpp) survives a re-issued order while the squad still fights where it is (not a move order).
 else if(U->OrderRevision!=A->Revision && !(A->bAssault && Store->FindSquad(U->SquadID) && Store->FindSquad(U->SquadID)->Order!=EBattleOrder::Move))End=TEXT("approach_cancel_new_order");
 // -BattleContactCover: the squad's target flickers; the walk goes on while this soldier's own sighting is fresh (the stale check below).
 // An assault dash (BattleArms.cpp) goes on while the soldier's own sighting is fresh even when the squad stops trading fire.
 else if(Target==INDEX_NONE && !bContactCover && !A->bAssault)End=TEXT("approach_cancel_disengaged");
 // The squad re-picks its target every think; keep walking while this soldier still sees its own threat.
 // An assault dash (BattleArms.cpp) goes on toward an enemy hidden behind his cover while the sighting is fresh (the stale check below).
 else if(Target!=A->Target && !(Contact && Contact->bVisible) && !bContactCover && !A->bAssault)End=TEXT("approach_cancel_target_changed");
 // An assault dash keeps going toward an enemy seen or reported within 10 s (he is often behind his cover meanwhile).
 else if(!Contact || Now-Contact->LastSeenTime>FMath::Min(A->bAssault?10.f:5.f,U->Perception.MemorySeconds))End=TEXT("approach_cancel_contact_stale");
 else if(!A->bFree && !GetWorld()->GetSubsystem<UBattleCover>()->IsAvailable(A->Slot,ID,Now))End=TEXT("approach_cancel_slot_taken");
 else if(FVector::Dist2D(Pos,A->Goal)<120)
 {
  End=TEXT("approach_arrived");CoverProbeAfter.Remove(ID);
  // -BattleContactCover: take the cover at once (the slot was checked before the walk), without waiting for a probe slot.
  if((bContactCover || A->bFallback) && A->bFree)
  {
   // A free firing position: behind low cover the soldier starts the crouch-and-stand cycle there; otherwise he fights from the
   // place as usual and does not look for another for a while.
   const FCoverApproach Done=*A;CoverApproaches.Remove(ID);
   if(Done.bStandUp && !U->CoverPhase)
   {
    U->CoverAnchor=Done.Goal;U->CoverPeek=Done.Goal;U->CoverSlot=FreeCoverSlot;U->CoverPhase=1;U->CoverSeconds=3;
    U->CoverOrderRevision=U->OrderRevision;U->CoverTargetID=Done.Target;U->DecisionReason=TEXT("COVER / FIRING POSITION");
    Diag.Cover(TEXT("fire_position_cover"));
   }
   else{FirePositions.Add(ID,Done.Goal);CoverProbeAfter.Add(ID,Now+8.);Diag.Cover(TEXT("fire_position_settled"));}
   return false;
  }
  if((bContactCover || A->bAssault) && Contact)
  {
   const int32 Goal=A->Target;const FVector Threat=Contact->LastKnownPosition;
   if(A->bAssault){Diag.Cover(TEXT("arms_assault_arrived"));ArmsBoundArrived.Add(ID,Now);} // the charge may follow (BattleArms.cpp)
   Diag.Cover(End);CoverApproaches.Remove(ID);
   if(RequestCoverAction(ID,Threat,Goal,250.f))Diag.Cover(TEXT("approach_arrived_took_cover"));
   return false;
  }
 }
 else if(Now>A->Until)End=TEXT("approach_cancel_timeout");
 if(End){Diag.Cover(A->bFree?TEXT("fire_position_cancelled"):End);CoverApproaches.Remove(ID);return false;}
 if(Target!=A->Target && !A->bKeptTargetChange){A->bKeptTargetChange=true;Diag.Cover(TEXT("approach_kept_after_target_change"));}
 auto& B=Bodies.FindChecked(ID);const FVector D=(A->Goal-Pos).GetSafeNormal2D();
 if(B.Actor.IsValid()){B.Actor->AddMovementInput(D);B.Velocity=B.Actor->GetVelocity();}
 else
 {
  FHitResult Hit;B.Capsule->MoveComponent(D*FMath::Min(GetDefault<UBattleTuning>()->MoveSpeed*Dt,float(FVector::Dist2D(Pos,A->Goal))),B.Capsule->GetComponentQuat(),true,&Hit);
  auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=B.Capsule->GetComponentLocation();B.Velocity=(F.Position-Pos)/FMath::Max(.001f,Dt);
 }
 return true;
}

bool UBattleLeaderRuntime::StepCoverAction(int32 ID,float Dt,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U || !U->CoverPhase)return false;
 auto* Squad=Store->FindSquad(U->SquadID);auto& B=Bodies.FindChecked(ID);
 const bool Player=B.Actor.IsValid()&&B.Actor->IsPlayerControlled();
 auto& Diag=SightExperiment;
 if(!BattleTactics::CoverActionsEnabled() || !U->IsAlive() || !Squad || Squad->CommandStatus!=EBattleCommandStatus::Active || U->OrderRevision!=U->CoverOrderRevision || (!Player && U->Order==EBattleOrder::Move))
 {Diag.Cover(!U->IsAlive()?TEXT("cancel_died"):(!Squad || Squad->CommandStatus!=EBattleCommandStatus::Active)?TEXT("cancel_squad_inactive"):TEXT("cancel_new_order"));CancelCoverAction(ID);return false;}
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 // Reservations are rebuilt after loading, against saved positions in this same map.
 if(U->CoverSlot==FreeCoverSlot){}
 else if(U->CoverSlot==INDEX_NONE)for(int32 Slot:Cover->NearbySlots(U->CoverAnchor,120,4))if(FVector::Dist2D(Cover->GetSlots()[Slot],U->CoverAnchor)<100 && Cover->Reserve(Slot,ID,Now)){U->CoverSlot=Slot;break;}
 if(U->CoverSlot!=FreeCoverSlot && !Cover->Reserve(U->CoverSlot,ID,Now)){Diag.Cover(TEXT("cancel_slot_lost"));CancelCoverAction(ID);return false;}
 U->CoverSeconds=FMath::Max(0.f,U->CoverSeconds-Dt);
 const auto* Contact=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,U->CoverTargetID);
 if(!Player && (!Contact || Now-Contact->LastSeenTime>FMath::Min(5.f,U->Perception.MemorySeconds))){Diag.Cover(U->CoverPhase==1?TEXT("cancel_contact_stale_entering"):TEXT("cancel_contact_stale_in_cover"));CancelCoverAction(ID);return false;}
 // Cover validation (default ON): leave cover once the observed threat is beyond weapon range, and only step out to a
 // peek point with a clear line to the threat's last known position; otherwise stay hidden and check again shortly.
 const bool Validate=!Player && BattleTactics::CoverValidationEnabled();
 if(Validate && FVector::Dist2D(Position(ID),Contact->LastKnownPosition)>TargetRange(ID))
 {Diag.Cover(TEXT("cancel_target_out_of_range"));CancelCoverAction(ID);return false;}
 if(U->CoverPhase==2 && !Player && U->CoverSeconds<=0 && U->ReloadEndTime==0)
 {
  // Suppression trial: a pinned AI soldier stays behind cover and checks again shortly.
  // Trial -BattleAIBlindFire=1: a pinned AI soldier fires blind at the threat's last known position instead of waiting idle.
  if(IsPinned(ID,Now)){U->CoverSeconds=.5f;Diag.Cover(TEXT("peek_hold_suppressed"));if(BattleBlindFire::AIEnabled())TryAIBlindFire(ID,Contact->LastKnownPosition,Now);}
  else if(Validate && !RevalidatePeek(ID,Contact->LastKnownPosition)){U->CoverSeconds=.5f;Diag.Cover(TEXT("peek_hold_no_line"));}
  else{U->CoverPhase=3;U->CoverSeconds=3;}
 }
 // -BattleCoverFire=1: an AI peek ends after AICoverShotsPerPeek shots instead of a fixed 1.2s that is shorter than the AI shot interval.
 const bool AIPeekFire=!Player && BattleTactics::CoverFireEnabled();
 if(U->CoverPhase==4)
 {
  const TCHAR* PeekEnd=U->ReloadEndTime>0?TEXT("reload"):(!Player && IsPinned(ID,Now))?TEXT("suppressed"):(AIPeekFire && CoverPeekShots.FindRef(ID)>=FMath::Max(1,GetDefault<UBattleTuning>()->AICoverShotsPerPeek))?TEXT("shots_done"):U->CoverSeconds<=0?TEXT("timer"):
   (Validate && !CoverLineClear(ID,Position(ID),Contact->LastKnownPosition))?TEXT("no_line"):
   // -BattleCoverPeekSight=1: stop exposing once the blind time has passed at the peek point without an own sighting.
   (AIPeekFire && BattleTactics::CoverPeekSightEnabled() && !(Contact->bVisible && !Contact->bShared) &&
    FMath::Clamp(GetDefault<UBattleTuning>()->AICoverPeekSeconds,.1f,3.f)-U->CoverSeconds>=GetDefault<UBattleTuning>()->AICoverPeekBlindSeconds)?TEXT("blind"):nullptr;
  if(PeekEnd){EndPeekDiag(ID,PeekEnd);U->CoverPhase=1;U->CoverSeconds=3;}
 }
 B.Velocity=FVector::ZeroVector;
 if(U->CoverPhase==1 || U->CoverPhase==3)
 {
  const FVector Goal=U->CoverPhase==1?U->CoverAnchor:U->CoverPeek;
  const FVector Before=Position(ID),D=(Goal-Before).GetSafeNormal2D();
  if(FVector::Dist2D(Before,Goal)<40)
  {U->CoverPhase=U->CoverPhase==1?2:4;U->CoverSeconds=U->CoverPhase==2?.8f:AIPeekFire?FMath::Clamp(GetDefault<UBattleTuning>()->AICoverPeekSeconds,.1f,3.f):1.2f;if(U->CoverPhase==4 && !Player)
   {
    CoverPeekShots.Add(ID,0);
    // Evaluation only: what the AI knew and whether its weapon was ready when it reached the peek point.
    Diag.Cover(!Contact?TEXT("peek_start_no_contact"):Contact->bVisible&&!Contact->bShared?TEXT("peek_start_visible"):Now-Contact->LastSeenTime<=GetDefault<UBattleTuning>()->ReacquireGraceSeconds?TEXT("peek_start_seen_within_grace"):TEXT("peek_start_seen_before_grace"));
    if(U->NextShotTime>Now)Diag.Cover(TEXT("peek_start_cooldown_pending"));
   }
   if(!Player)Diag.Cover(U->CoverPhase==2?TEXT("reached_hidden"):TEXT("reached_peek"));if(B.Actor.IsValid())B.Actor->GetCharacterMovement()->StopMovementImmediately();}
  else if(U->CoverSeconds<=0){Diag.Cover(U->CoverPhase==1?TEXT("cancel_move_timeout_to_hidden"):TEXT("cancel_move_timeout_to_peek"));CancelCoverAction(ID);return false;}
  else
  {
   if(B.Actor.IsValid()){B.Actor->AddMovementInput(D);B.Velocity=B.Actor->GetVelocity();}
   else {FHitResult Hit;B.Capsule->MoveComponent(D*FMath::Min(250.f*Dt,float(FVector::Dist2D(Before,Goal))),B.Capsule->GetComponentQuat(),true,&Hit);auto& F=Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity);F.Position=B.Capsule->GetComponentLocation();B.Velocity=(F.Position-Before)/FMath::Max(.001f,Dt);}
  }
 }
 else if(B.Actor.IsValid())B.Actor->GetCharacterMovement()->StopMovementImmediately();
 if(!Player && Contact)
 {
  const FVector Aim=(Contact->LastKnownPosition+FVector(0,0,35)-(Position(ID)+FVector(0,0,60))).GetSafeNormal();
  if(B.Actor.IsValid())B.Actor->SetActorRotation(FRotator(0,Aim.Rotation().Yaw,0));
  else {Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).Forward=Aim.GetSafeNormal2D();B.Capsule->SetWorldRotation(FRotator(0,Aim.Rotation().Yaw,0));}
  Store->UpdateAim(ID,Now,B.Velocity.Size2D(),Aim,true);
  const bool InRange=FVector::Dist2D(Position(ID),Contact->LastKnownPosition)<=(UnderFire(ID)?BattleRanges::Reach(U->Weapon):BattleRanges::Engage(U->Weapon));
  if(U->CoverPhase==4 && Contact->bVisible && !Contact->bShared && InRange && CanSee(ID,U->CoverTargetID))
  {
   if(U->Ammo==0){Store->StartReload(ID,Now);Diag.Cover(TEXT("peekwait_reloading"));}
   else
   {
    const bool Shot=Fire(ID,Position(ID)+FVector(0,0,60),Aim,false);
    if(Shot){Diag.Cover(TEXT("cover_shots"));if(++CoverPeekShots.FindOrAdd(ID)==1)Diag.Cover(TEXT("peek_first_shot"));}
    else Diag.Cover(U->ReloadEndTime>0?TEXT("peekwait_reloading"):Now<U->NextShotTime?TEXT("peekwait_cooldown"):TEXT("peekwait_fire_other"));
    if(Shot && Squad->TacticalPlan.Action==EBattleSquadTactic::Support)GetWorld()->GetSubsystem<UBattleSquadTactics>()->RecordSupportShot(Squad->SquadID,U->CoverTargetID);
   }
  }
  // Evaluation only (never AI input): why this AI peek frame could not fire. The extra line-of-sight check runs only in experiments.
  else if(U->CoverPhase==4 && Diag.Enabled)
   Diag.Cover(!Contact->bVisible?(CanSee(ID,U->CoverTargetID)?TEXT("peekwait_detecting"):TEXT("peekwait_no_line_of_sight")):Contact->bShared?TEXT("peekwait_shared_only"):!InRange?TEXT("peekwait_out_of_range"):TEXT("peekwait_ray_blocked"));
 }
 else if(!Player && U->CoverPhase==4)Diag.Cover(TEXT("peekwait_no_contact"));
 if(U->CoverPhase==2 && U->Ammo==0)Store->StartReload(ID,Now);
 if(!B.Actor.IsValid())
 {
  const int32 Pose=U->ReloadEndTime>Now?2:B.Velocity.SizeSquared2D()>400?1:0;
  const bool Blind=IsBlindFiring(ID,Now);
  // Cover clips (BattleCoverClips.h): one whole-body loop per phase for a near follower (no one-shots), the mesh turned by the table.
  const uint8 ClipPhase=(U->CoverPhase==2||U->CoverPhase==4)&&BattleBodyAnim::Enabled()?U->CoverPhase:0;
  if(ClipPhase && !B.CoverClipPhase)B.CoverClipSet=BattleBodyAnim::CoverSet(GetWorld(),*U,Forward(ID));
  const bool Run=ShowsRun(ID,Pose,Now);
  if(Pose!=B.Pose || Blind!=B.bBlindPose || ClipPhase!=B.CoverClipPhase || Run!=B.bRunPose)
  {
   B.Pose=Pose;B.bBlindPose=Blind;B.bRunPose=Run;B.CoverClipPhase=ClipPhase;if(VAT)VAT->ChangePose(ID,Pose);
   if(!VAT || !VAT->IsFar(ID))
   {
    // Blind fire around the edge turns the body so the rifle points around the corner (BlindFireYaw, 0 over the top).
    B.Mesh->SetRelativeRotation(FRotator(0,-90+(Pose==2?0.f:Blind?BlindFireYaw(ID,Forward(ID)):ClipPhase?BattleCoverClips::YawOffset(ClipPhase):0.f),0));
    // Leaning out past the edge while the side blind fire shows part of the body (BattleBlindFire::SideLeanBack).
    FVector Lean=FVector::ZeroVector,Shown;
    if(Blind && SideExposure(ID,Now,Shown)){const FVector Pos=Position(ID),Out=(Shown-Pos).GetSafeNormal2D();Lean=Shown-Out*BattleBlindFire::SideLeanBack-Pos;Lean.Z=0.;}
    const USceneComponent* Parent=B.Mesh->GetAttachParent();
    B.Mesh->SetRelativeLocation(FVector(0,0,-90)+(Parent?Parent->GetComponentTransform().InverseTransformVectorNoScale(Lean):Lean));
    B.Mesh->PlayAnimation(FollowerClip(ID,Pose),true);
   }
  }
 }
 U->DecisionReason=U->CoverPhase==2?(IsBlindFiring(ID,Now)?TEXT("COVER / BLIND FIRE"):TEXT("COVER / HIDDEN - FIRE TO PEEK")):U->CoverPhase==4?TEXT("COVER / EXPOSED FIRING WINDOW"):TEXT("COVER / MOVING - FIRE BLOCKED");
 if(!Player)U->Action=U->CoverPhase==2?EBattleAction::TakingCover:U->CoverPhase==4?EBattleAction::Engaging:EBattleAction::Moving;
 // Stances: crouch while hidden behind cover, stand to move and to fire from the peek point.
 // (Direct control too: behind low cover the player is only hidden while crouched.)
 SetStance(ID,U->CoverPhase==2?BattleStance::Crouch:BattleStance::Stand,Now,true);
 return true;
}
