// Individual AI stage B1 (2026-11-20, trial -BattleSoldierMove=1, default 0): inside the squad's sector an AI soldier in cover
// chooses for himself whether a better cover place is worth the walk. The squad still picks the sector, the route to it and
// the arrival places (ROADMAP "2026-11-20 個人AI"). Part of UBattleLeaderRuntime.
#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattleSectors.h"
#include "BattleSquadTactics.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::SoldierMoveEnabled(int32 Team)
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierMove="),V);return V!=0;}();
 static const int32 Only=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierMoveTeam="),V);return V==0||V==1?V:-1;}();
 return bOn && (Only<0 || Only==Team);
}

float UBattleLeaderRuntime::RateCoverPlace(int32 ID,FVector From,FVector Anchor,FVector Peek,bool bFire,const TArray<FVector,TInlineAllocator<3>>& Threats) const
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!U || Threats.IsEmpty())return -100.f;
 const FVector Eye=Threats[0]+FVector(0,0,60);
 // Hidden from the target (the caller has already thrown away places that are not), and from the other threats.
 float Score=1.f-BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction();
 for(int32 I=1;I<Threats.Num();++I)
  Score+=.5f*(BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Threats[I]+FVector(0,0,60)).Fraction()<=CoverExposureLimit()?1.f:-1.f);
 // A place he can fire from.
 if(bFire)Score+=1.f;
 // Within engagement range of the target, and not closer than 15 m.
 const double Range=FVector::Dist2D(Peek,Threats[0]);
 Score+=Range<=BattleRanges::Engage(U->Weapon) && Range>=1500.?.3f:-.3f;
 // Seen on the way: three points of the straight walk, against each threat.
 const double Walk=FVector::Dist2D(From,Anchor);
 if(Walk>150.)
  for(const float T:{.25f,.5f,.75f})
  {
   const FVector P=FMath::Lerp(From,Anchor,T)+FVector(0,0,60);
   for(const FVector& Th:Threats)if(BattleTactics::HasTerrainLine(GetWorld(),P,Th+FVector(0,0,60)))Score-=.1f;
  }
 // A short walk, and not on top of squadmates (3 m).
 Score-=.3f*float(Walk/2500.);
 if(const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(U->SquadID))
  for(int32 M:S->Members)if(M!=ID && FVector::DistSquared2D(Position(M),Anchor)<FMath::Square(300.))
   if(const auto* MU=GetWorld()->GetSubsystem<UBattleState>()->Find(M);MU && MU->IsAlive())Score-=.15f;
 return Score;
}

bool UBattleLeaderRuntime::StepSoldierMove(int32 ID,FVector Primary,double Now,float Dt)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U || !U->IsAlive() || U->CoverPhase!=2 || U->bSoldierOrder || !SoldierMoveEnabled(U->Team) || IsVehicle(ID) || U->Weapon==EBattleWeapon::Mortar)return false;
 const auto* Squad=Store->FindSquad(U->SquadID);const auto* Plan=Plans.Find(U->SquadID);
 if(!Squad || !Plan || !Squad->bSectorMission || Squad->TacticalPlan.Action!=EBattleSquadTactic::Mission || !SoldierAIFocus(ID))return false;
 // Once a second per soldier, half a second away from the stage A check (same stateless phase rule).
 const double Phase=double((ID+8)%16)/16.;
 if(FMath::FloorToDouble(Now+Phase)==FMath::FloorToDouble(Now-Dt+Phase))return false;
 if(Now<U->SoldierMoveAfter)return false;
 const FIntPoint Sector=BattleSectors::Cell(Squad->Destination);
 const FVector Pos=Position(ID);
 if(BattleSectors::Cell(Pos)!=Sector)return false; // the squad brings him into the sector first
 // Squad rules: at most half of the living members relocate at once, and only while a squadmate is in cover.
 int32 Living=0,Moving=0,Covering=0;
 for(int32 M:Squad->Members)if(const auto* MU=Store->Find(M);MU && MU->IsAlive())
 {
  ++Living;
  if(M==ID)continue;
  if(MU->CoverPhase==1 || CoverApproaches.Contains(M))++Moving;
  else if((MU->CoverPhase==2 || MU->CoverPhase==4) && FVector::DistSquared2D(Position(M),Pos)<FMath::Square(3000.))++Covering;
 }
 auto& Diag=SightExperiment;
 if((Moving+1)*2>Living || !Covering){Diag.Cover(TEXT("soldier_move_squad_busy"));return false;}
 if(SoldierMoveFrame!=GFrameCounter){SoldierMoveFrame=GFrameCounter;SoldierMovesThisFrame=0;}
 if(SoldierMovesThisFrame>=4){Diag.Cover(TEXT("soldier_move_frame_full"));return false;}
 ++SoldierMovesThisFrame;Diag.Cover(TEXT("soldier_move_think"));
 TArray<FVector,TInlineAllocator<3>> Threats;GatherThreats(ID,Primary,Now,Threats);
 const FVector Eye=Primary+FVector(0,0,60);
 const float Here=RateCoverPlace(ID,Pos,U->CoverAnchor,U->CoverPeek,BattleTactics::HasTerrainLine(GetWorld(),U->CoverPeek+FVector(0,0,60),Eye),Threats);
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 auto Clear=[&](FVector A,FVector B){return !GetWorld()->SweepTestByObjectType(A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88));};
 int32 Best=INDEX_NONE,Rated=0;float BestScore=Here+FMath::Max(.2f,.2f*FMath::Abs(Here));FVector BestAnchor=FVector::ZeroVector,BestPeek=FVector::ZeroVector;
 for(int32 Slot:Cover->NearbySlots(Pos,2500.f,24))
 {
  if(Slot==U->CoverSlot || !Cover->IsAvailable(Slot,ID,Now) || BrokenCoverSlot(ID,Slot,Now))continue;
  const FVector Anchor=Cover->GetSlots()[Slot]+FVector(0,0,90);
  if(FVector::Dist2D(Anchor,Pos)<200. || BattleSectors::Cell(Anchor)!=Sector || FVector::DistSquared2D(Anchor,Plan->Center)>FMath::Square(3000.))continue;
  bool Taken=false;for(const auto& A:CoverApproaches)if(A.Key!=ID && A.Value.Slot==Slot){Taken=true;break;}
  if(Taken)continue;
  if(BattleTactics::ExposureAt(GetWorld(),Anchor,Forward(ID),Eye).Fraction()>CoverExposureLimit())continue;
  if(++Rated>8)break;
  if(!Clear(Pos,Anchor))continue; // the cover walk goes straight
  FVector Peek;const bool bFire=ContactFirePoint(ID,Anchor,Primary,Peek);
  if(!bFire)continue; // a saved cover action needs a valid peek point; a place he cannot fire from is not worth the walk
  const float Score=RateCoverPlace(ID,Pos,Anchor,Peek,true,Threats);
  if(Score>BestScore){BestScore=Score;Best=Slot;BestAnchor=Anchor;BestPeek=Peek;}
 }
 if(Best==INDEX_NONE){Diag.Cover(TEXT("soldier_move_stay"));return false;}
 Cover->Release(ID);
 if(!Cover->Reserve(Best,ID,Now)){Cover->Reserve(U->CoverSlot,ID,Now);Diag.Cover(TEXT("soldier_move_reserve_failed"));return false;}
 const double Walk=FVector::Dist2D(Pos,BestAnchor);
 UE_LOG(LogFutureBattle,Verbose,TEXT("BATTLE_SOLDIER_MOVE unit=%d from=%d to=%d walk_cm=%.0f here=%.2f there=%.2f"),ID,U->CoverSlot,Best,Walk,Here,BestScore);
 U->CoverAnchor=BestAnchor;U->CoverPeek=BestPeek;U->CoverSlot=Best;U->CoverPhase=1;U->CoverSeconds=3.f;U->SoldierMoveUntil=Now+Walk/250.+1.5;U->CoverFirstBreak=0;
 U->DecisionReason=TEXT("COVER / MOVING TO A BETTER PLACE");
 U->SoldierMoveAfter=Now+8.;
 Diag.Cover(TEXT("soldier_move_started"));
 return true;
}

// -BattleSoldierAIFocus=<m> (2026-11-20, default 0 = every soldier): the individual AI (stage A, B1) works only within this
// distance of the player: the soldier he controls directly, otherwise the point the tactical camera looks at.
// -BattleSoldierAIFocusAt=X,Y (m) fixes the point instead (headless tests have no player). Both teams alike.
bool UBattleLeaderRuntime::SoldierAIFocus(int32 ID) const
{
 static const double Radius=[]{double V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierAIFocus="),V);return FMath::Max(0.,V)*100.;}();
 if(Radius<=0)return true;
 static const TOptional<FVector> Fixed=[]()->TOptional<FVector>{FString V;if(!FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierAIFocusAt="),V))return {};
  FString X,Y;if(!V.Split(TEXT(","),&X,&Y))return {};return FVector(FCString::Atod(*X)*100.,FCString::Atod(*Y)*100.,0);}();
 if(FocusFrame!=GFrameCounter)
 {
  FocusFrame=GFrameCounter;FocusValid=false;
  if(Fixed.IsSet()){FocusPoint=Fixed.GetValue();FocusValid=true;}
  else if(const APlayerController* PC=GetWorld()->GetFirstPlayerController();PC && PC->GetPawn()){FocusPoint=PC->GetPawn()->GetActorLocation();FocusValid=true;}
 }
 return FocusValid && FVector::DistSquared2D(Position(ID),FocusPoint)<=FMath::Square(Radius);
}
