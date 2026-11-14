#include "BattleLeaderRuntime.h"
#include "BattleSave.h"
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleVATDisplay.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"

bool UBattleLeaderRuntime::ReplaceFollower(int32 DeadID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Old=Store->Find(DeadID);
 const auto* S=Old?Store->FindSquad(Old->SquadID):nullptr;
 if(!bArea||!Old||Old->IsAlive()||Old->bRetired||Old->bActorRepresentation||!S||S->CommandStatus!=EBattleCommandStatus::Active||Store->GetUnits().Num()>=1000)return false;
 const auto Weapon=Old->Weapon;const auto Training=Old->Training;const int32 Team=Old->Team,SID=S->SquadID;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
 FVector PositionToUse;bool Found=false;
 for(int32 I=0;I<32&&!Found;++I)
 {
  FNavLocation P;const FVector Candidate=Position(S->LeaderUnitID)+FVector(FMath::Cos(I*PI/4),FMath::Sin(I*PI/4),0)*(250+(I/8)*150);
  if(!Nav->ProjectPointToNavigation(Candidate,P,FVector(60,60,250)))continue;
  PositionToUse=P.Location+FVector(0,0,100);
  Found=!GetWorld()->OverlapBlockingTestByChannel(PositionToUse,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,90));
 }
 if(!Found)return false;
 FBattleSavedUnit E;E.Position=PositionToUse;E.Rotation=Forward(S->LeaderUnitID).Rotation();E.State.bActorRepresentation=false;E.State.Weapon=Weapon;
 FBattleLeaderBody B;if(!CreateBody(E,DisplayHost,B)){DestroyBody(B);return false;}
 const int32 ID=Store->RegisterUnit(Team,0,Weapon);Store->SetTrainingPreset(ID,Training);
 const bool Replaced=Store->ReplaceDeadFollower(DeadID,ID,PositionToUse);check(Replaced);
 if(VAT)VAT->RemoveUnit(DeadID);
 if(auto* Previous=Bodies.Find(DeadID)){HitIDs.Remove(Previous->Capsule.Get());DestroyBody(*Previous);Bodies.Remove(DeadID);}
 Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).UnitID=ID;
 Bodies.Add(ID,B);HitIDs.Add(B.Capsule.Get(),ID);Plans.FindOrAdd(SID).bReplan=true;bSummaryDirty=true;
 return true;
}

bool UBattleLeaderRuntime::SpawnPurchasedSquad(int32 Team,EBattleTraining Training,FString& Error,bool bSelect,int32* OutSquadID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Fail=[&](const TCHAR* Why){Error=Why;return false;};
 if((!bMixedFastMatch&&!bArea) || !Mode || Team<0||Team>1||static_cast<uint8>(Training)>2)return Fail(TEXT("Mixed recruitment unavailable"));
 // Bound retained corpses and persistence size as well as living population.
 // Area: 500 starting soldiers + replenishments + 10 x hires (resources incl. income; capped at 1000 records, the save limit).
 if(Store->GetUnits().Num()+10>(bArea?1000:500))return Fail(bArea?TEXT("Match roster capacity reached (1000 including losses)"):TEXT("Match roster capacity reached (500 including losses)"));
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return Fail(TEXT("Navigation unavailable"));
 TArray<FVector> Positions;
 // Area homes follow -BattleTrialSwap; MixedFast keeps its fixed sides. Side points from the centre to the home.
 const float HomeX=bArea?GetWorld()->GetSubsystem<UBattleMatch>()->AreaHome(Team).X:(Team?1:-1)*BattleFieldLayout::HomeDistance(GetWorld());
 const float Side=HomeX>=0?1.f:-1.f;
 for(int32 Column=0;Column<5 && Positions.Num()<10;++Column)for(int32 Row=0;Row<8 && Positions.Num()<10;++Row)
 {
  TArray<FVector> Candidate;
  for(int32 Slot=0;Slot<10;++Slot)
  {
   FNavLocation P;
   if(!Nav->ProjectPointToNavigation(FVector(HomeX-Side*(400+Column*350-(Slot/5)*120),(Row-3.5)*600+(Slot%5-2)*120,0),P,FVector(60,60,250)))break;
   const FVector Position=P.Location+FVector(0,0,100);
   if(GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,90)))break;
   bool Occupied=false;for(FVector Other:Candidate)Occupied|=FVector::Dist2D(Position,Other)<100;
   if(Occupied)break;Candidate.Add(Position);
  }
  if(Candidate.Num()==10)Positions=MoveTemp(Candidate);
 }
 if(Positions.Num()!=10)return Fail(TEXT("Home deployment area is blocked"));
 TArray<FBattleLeaderBody> Staged;
 for(int32 Slot=0;Slot<10;++Slot)
 {
  FBattleSavedUnit E;E.Position=Positions[Slot];E.Rotation=FRotator(0,Side>0?180:0,0);
  E.State.bActorRepresentation=Slot==0;E.State.Weapon=WeaponForSlot(Slot);
  FBattleLeaderBody B;
  if(!CreateBody(E,DisplayHost,B))
  {DestroyBody(B);for(auto& Old:Staged)DestroyBody(Old);return Fail(TEXT("Representation staging failed; purchase retained"));}
  Staged.Add(B);
 }
 // All fallible navigation/body work precedes allocation of canonical IDs.
 TArray<int32> Members;
 for(int32 Slot=0;Slot<10;++Slot)
 {
  const int32 ID=Store->RegisterUnit(Team,0,WeaponForSlot(Slot));Store->SetTrainingPreset(ID,Training);Members.Add(ID);
  auto& B=Staged[Slot];Bodies.Add(ID,B);
  if(B.Actor.IsValid()){B.Actor->InitializeUnit(ID);Mode->Units.Add(B.Actor.Get());HitIDs.Add(B.Actor->GetCapsuleComponent(),ID);}
  else {Manager->GetFragmentDataChecked<FBattleFollowerFragment>(B.Entity).UnitID=ID;HitIDs.Add(B.Capsule.Get(),ID);}
 }
 const int32 SID=Store->CreateLeaderSquad(Team,Members,Members[0],Team==0);
 check(SID!=INDEX_NONE);
 FBattleLeaderPlan Plan;Plan.Center=Positions[0];Plans.Add(SID,Plan);
 IssueOrder(SID,EBattleOrder::Hold,Positions[0]);bSummaryDirty=true;
 if(OutSquadID)*OutSquadID=SID;
 if(auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController())){PC->BindLeaderDeaths();if(Team==0&&bSelect)PC->SetSelection(Actor(Members[0]));}
 // Newly purchased followers use the normal skeletal fallback until next load.
 // Existing VAT entries and animation clocks remain untouched by recruitment.
 Mode->RefreshVisibility();return true;
}
