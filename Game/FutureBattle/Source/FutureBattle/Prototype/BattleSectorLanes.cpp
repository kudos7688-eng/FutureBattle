// Trial -BattleSectorLanes=1 (2026-11-25, default 0): spread entering squads over authored lanes and keep their choice.
#include "BattleLeaderRuntime.h"
#include "BattleSectors.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "BattleState.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::SectorLanesEnabled()
{
 // Default 1 since 2026-11-26 (user decision "make it the default"; map K, four seeds on the new default: stalls 399->329,
 // 398->281, 427->398, 807->260, town entrance stalls 53-87 -> 4-6). =0 is the old behaviour. Only maps with lane markers (K).
 static const bool bOn=[]{int32 LaneArg=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorLanes="),LaneArg);return LaneArg!=0;}();
 return bOn;
}

void UBattleLeaderRuntime::ReadSectorLanes()
{
 if(bSectorLanesRead)return;
 bSectorLanesRead=true;
 for(TActorIterator<AStaticMeshActor> LaneActor(GetWorld());LaneActor;++LaneActor)
 {
  if(!LaneActor->ActorHasTag(TEXT("BattleLaneRoute")))continue;
  for(const FName& LaneTag:LaneActor->Tags)
  {
   const FString LaneText=LaneTag.ToString();if(!LaneText.StartsWith(TEXT("LanePts=")))continue;
   FBattleSectorLane ReadLane;TArray<FString> LanePairs;LaneText.Mid(8).ParseIntoArray(LanePairs,TEXT(";"),false);
   bool bLaneParsed=true;
   for(const FString& LanePair:LanePairs)
   {
    FString LaneXText,LaneYText;double LaneX=0.,LaneY=0.;
    if(!LanePair.Split(TEXT(","),&LaneXText,&LaneYText)||!LaneXText.IsNumeric()||!LaneYText.IsNumeric()||((LaneX=FCString::Atod(*LaneXText)),(LaneY=FCString::Atod(*LaneYText)),false)||!FMath::IsFinite(LaneX)||!FMath::IsFinite(LaneY))
    {bLaneParsed=false;break;}
    ReadLane.Points.Add(FVector(LaneX,LaneY,0));
   }
   if(bLaneParsed && ReadLane.Points.Num()>1)
   {
    ReadLane.Cell=BattleSectors::Cell(ReadLane.Points.Last());
    for(int32 SegmentIndex=1;SegmentIndex<ReadLane.Points.Num();++SegmentIndex)ReadLane.Length+=FVector::Dist2D(ReadLane.Points[SegmentIndex-1],ReadLane.Points[SegmentIndex]);
    SectorLanes.Add(MoveTemp(ReadLane));
   }
   break;
  }
 }
 // 2026-11-25: actor iteration order must not decide lane indices or assignment ties.
 SectorLanes.Sort([](const FBattleSectorLane& LaneA,const FBattleSectorLane& LaneB)
 {
  if(LaneA.Cell.X!=LaneB.Cell.X)return LaneA.Cell.X<LaneB.Cell.X;
  if(LaneA.Cell.Y!=LaneB.Cell.Y)return LaneA.Cell.Y<LaneB.Cell.Y;
  if(LaneA.Points[0].X!=LaneB.Points[0].X)return LaneA.Points[0].X<LaneB.Points[0].X;
  return LaneA.Points[0].Y<LaneB.Points[0].Y;
 });
 for(int32 ReadLaneIndex=0;ReadLaneIndex<SectorLanes.Num();++ReadLaneIndex)SectorLaneIndex.FindOrAdd(SectorLanes[ReadLaneIndex].Cell).Add(ReadLaneIndex);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_LANES lanes=%d sectors=%d"),SectorLanes.Num(),SectorLaneIndex.Num());
}

void UBattleLeaderRuntime::ApplySectorLane(int32 SquadID,const FVector& Origin,const FVector& From,const FVector& To,UNavigationSystemV1& Navigation)
{
 const auto* LaneStore=GetWorld()->GetSubsystem<UBattleState>();const auto* LaneSquad=LaneStore->FindSquad(SquadID);
 if(!LaneSquad)return;
 const FIntPoint LaneCell=BattleSectors::Cell(LaneSquad->Destination);
 if(BattleSectors::Cell(Origin)==LaneCell)return;
 ReadSectorLanes();
 const TArray<int32>* EntryLanes=SectorLaneIndex.Find(LaneCell);if(!EntryLanes)return;
 auto& LanePlan=Plans.FindChecked(SquadID);
 const auto ClearLaneChoice=[&](){LanePlan.LaneIndex=INDEX_NONE;LanePlan.LaneRevision=INDEX_NONE;LanePlan.LaneGoal=FVector::ZeroVector;};
 // Kept while the squad is bound for the same sector (first version: same order revision and goal within 1m - the orders come
 // anew every 5s with a new revision, so 2458 choices were made in a match and 3 kept, ln-K-lanes-s7, and the load never counted).
 const bool bKeptLane=SectorLanes.IsValidIndex(LanePlan.LaneIndex) && SectorLanes[LanePlan.LaneIndex].Cell==LaneCell;
 if(!bKeptLane)
 {
  static const double LaneLoad=[]{double LoadArg=1500.;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorLaneLoad="),LoadArg);return LoadArg;}();
  const int32 LaneTurn=SectorTurn(LaneSquad->Team);
  double BestLaneCost=DBL_MAX;int32 BestLaneIndex=INDEX_NONE,BestLaneLoad=0;
  for(int32 CandidateLaneIndex:*EntryLanes)
  {
   const FBattleSectorLane& CandidateLane=SectorLanes[CandidateLaneIndex];int32 CandidateLoad=0;
   for(const auto& OtherLanePlan:Plans)
   {
    if(OtherLanePlan.Key==SquadID || OtherLanePlan.Value.LaneIndex!=CandidateLaneIndex)continue;
    const auto* OtherLaneSquad=LaneStore->FindSquad(OtherLanePlan.Key);
    if(OtherLaneSquad && OtherLaneSquad->Team==LaneSquad->Team && BattleSectors::Cell(OtherLaneSquad->Destination)==LaneCell)++CandidateLoad;
   }
   const double CandidateCost=FVector::Dist2D(Origin,CandidateLane.Points[0])+CandidateLane.Length+FVector::Dist2D(CandidateLane.Points.Last(),LaneSquad->Destination)+LaneLoad*CandidateLoad;
   bool bLaneTieFirst=false;
   if(BestLaneIndex!=INDEX_NONE && CandidateCost==BestLaneCost)
   {
    const FVector& CandidateDoor=CandidateLane.Points[0];const FVector& BestDoorPoint=SectorLanes[BestLaneIndex].Points[0];
    bLaneTieFirst=CandidateDoor.X!=BestDoorPoint.X?LaneTurn*CandidateDoor.X<LaneTurn*BestDoorPoint.X:LaneTurn*CandidateDoor.Y<LaneTurn*BestDoorPoint.Y;
   }
   if(CandidateCost<BestLaneCost || bLaneTieFirst){BestLaneCost=CandidateCost;BestLaneIndex=CandidateLaneIndex;BestLaneLoad=CandidateLoad;}
  }
  ClearLaneChoice();
  if(BestLaneIndex==INDEX_NONE){SightExperiment.Cover(TEXT("sector_lane_none"));return;}
  LanePlan.LaneIndex=BestLaneIndex;LanePlan.LaneRevision=LaneSquad->OrderRevision;LanePlan.LaneGoal=LaneSquad->Destination;
  const FVector& ChosenDoor=SectorLanes[BestLaneIndex].Points[0];
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_LANE squad=%d team=%d cell=(%d,%d) lane=%d door=(%.0f,%.0f) cost=%.0f load=%d of=%d"),SquadID,LaneSquad->Team,LaneCell.X,LaneCell.Y,BestLaneIndex,ChosenDoor.X,ChosenDoor.Y,BestLaneCost,BestLaneLoad,EntryLanes->Num());
 }
 // 2026-11-25: stage every projection and both complete nav tails before replacing the normal path on success.
 const FBattleSectorLane& ChosenLane=SectorLanes[LanePlan.LaneIndex];TArray<FVector> ProjectedLane;
 for(const FVector& AuthoredPoint:ChosenLane.Points)
 {
  FNavLocation LaneOnMesh;
  if(!Navigation.ProjectPointToNavigation(AuthoredPoint,LaneOnMesh,FVector(100,100,400)))
  {
   // 2026-11-26: which lane fails and at which step (map I: 18% of the choices fall back to the normal path).
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_LANE_FAIL squad=%d lane=%d cell=(%d,%d) why=project point=%d of=%d at=(%.0f,%.0f)"),SquadID,LanePlan.LaneIndex,LaneCell.X,LaneCell.Y,ProjectedLane.Num(),ChosenLane.Points.Num(),AuthoredPoint.X,AuthoredPoint.Y);
   ClearLaneChoice();SightExperiment.Cover(TEXT("sector_lane_no_nav"));return;
  }
  ProjectedLane.Add(LaneOnMesh.Location);
 }
 auto* EntryPath=Navigation.FindPathToLocationSynchronously(GetWorld(),From,ProjectedLane[0]);
 // 2026-11-26 trial -BattleSectorLaneHubGoal=1 (default 0): a lane that ends on the goal itself needs no tail. In objective sectors the
 // hub is the sector's middle, which is also the sector order's goal: the tail query from a point to itself returns one point and
 // the lane was refused (map I: 187 of the 198 refusals of lf-I-lanefail-s7, all in the O and D sectors).
 static const bool bLaneHubGoal=[]{int32 HubGoalArg=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorLaneHubGoal="),HubGoalArg);return HubGoalArg!=0;}();
 const bool bLaneEndsAtGoal=bLaneHubGoal && FVector::Dist2D(ProjectedLane.Last(),To)<100.;
 auto* ExitPath=bLaneEndsAtGoal?nullptr:Navigation.FindPathToLocationSynchronously(GetWorld(),ProjectedLane.Last(),To);
 if(!EntryPath || !EntryPath->IsValid() || EntryPath->IsPartial() || EntryPath->PathPoints.Num()<2 ||
    (!bLaneEndsAtGoal && (!ExitPath || !ExitPath->IsValid() || ExitPath->IsPartial() || ExitPath->PathPoints.Num()<2)))
 {
  const bool bEntryBad=!EntryPath || !EntryPath->IsValid() || EntryPath->IsPartial() || EntryPath->PathPoints.Num()<2;
  const UNavigationPath* FailedLanePath=bEntryBad?EntryPath:ExitPath;
  const FVector FailFrom=bEntryBad?From:ProjectedLane.Last(),FailTo=bEntryBad?ProjectedLane[0]:To;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SECTOR_LANE_FAIL squad=%d lane=%d cell=(%d,%d) why=%s valid=%d partial=%d points=%d from=(%.0f,%.0f) to=(%.0f,%.0f) dist=%.0f"),SquadID,LanePlan.LaneIndex,LaneCell.X,LaneCell.Y,bEntryBad?TEXT("entry"):TEXT("exit"),FailedLanePath&&FailedLanePath->IsValid()?1:0,FailedLanePath&&FailedLanePath->IsPartial()?1:0,FailedLanePath?FailedLanePath->PathPoints.Num():-1,FailFrom.X,FailFrom.Y,FailTo.X,FailTo.Y,FVector::Dist2D(FailFrom,FailTo));
  ClearLaneChoice();SightExperiment.Cover(TEXT("sector_lane_no_nav"));return;
 }
 TArray<FVector> JoinedLanePath=EntryPath->PathPoints;
 for(int32 LanePointIndex=1;LanePointIndex<ProjectedLane.Num();++LanePointIndex)JoinedLanePath.Add(ProjectedLane[LanePointIndex]);
 if(bLaneEndsAtGoal)SightExperiment.Cover(TEXT("sector_lane_hub_goal"));
 for(int32 ExitPointIndex=1;ExitPath && ExitPointIndex<ExitPath->PathPoints.Num();++ExitPointIndex)JoinedLanePath.Add(ExitPath->PathPoints[ExitPointIndex]);
 LanePlan.Path=MoveTemp(JoinedLanePath);
 SightExperiment.Cover(TEXT("sector_lane_used"));
 if(bKeptLane)SightExperiment.Cover(TEXT("sector_lane_kept"));
}
