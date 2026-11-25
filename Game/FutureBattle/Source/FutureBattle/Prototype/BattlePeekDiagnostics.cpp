#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"

const TCHAR* FBattleSightExperiment::PeekClass(int32 Index)
{
 static const TCHAR* Names[]={TEXT("unsearched"),TEXT("detecting"),TEXT("scanwait"),TEXT("nolos"),TEXT("nosight_other"),TEXT("aimturn"),TEXT("interval"),TEXT("reload"),TEXT("noammo"),TEXT("cansee_fail"),TEXT("fired"),TEXT("other")};
 return Names[Index];
}

void UBattleLeaderRuntime::RecordPeekFrame(int32 ID,double Now,const FBattleContact* Contact,int32 Ray,bool bFired)
{
 auto& WhyDiag=SightExperiment;if(!WhyDiag.Enabled)return;
 auto* WhyEpisode=WhyDiag.PeekWhy.Find(ID);if(!WhyEpisode || !WhyEpisode->bArrived)return;
 if(WhyEpisode->LastFrameAt==Now)return;WhyEpisode->LastFrameAt=Now;
 const auto* WhyUnit=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);if(!WhyUnit)return;
 const bool bWhyOwn=Contact && Contact->bVisible && !Contact->bShared;
 WhyEpisode->bSaw|=bWhyOwn;
 int32 WhyClass=11;
 if(!bWhyOwn)
 {
  const auto* WhyScan=WhyDiag.PeekScans.Find(ID);
  if(WhyScan && WhyScan->Target!=WhyUnit->CoverTargetID)WhyScan=nullptr;
  const auto* WhySense=GetWorld()->GetSubsystem<UBattlePerception>();
  const double* WhyDue=WhySense->NextScanTimes.Find(ID);
  // No scan since arriving at the peek point = waiting for the scan. Otherwise the last scan's outcome: 0 not searched,
  // 1 searched but no line, 2 detection running. Unknown scans stay unknown (the cover CanSee, when it ran, decides nolos).
  const bool bWhyNoScanYet=WhyScan?WhyScan->At<WhyEpisode->ArrivedAt-1.e-6:(WhyDue && *WhyDue>Now+1.e-6);
  WhyClass=bWhyNoScanYet?2:!WhyScan?(Ray==0?3:4):WhyScan->Stage==0?0:WhyScan->Stage==1?3:WhyScan->Stage==2?1:Ray==0?3:4;
 }
 else if(bFired)WhyClass=10; // Fire has already moved its interval deadline; this frame fired, it did not wait.
 else if(Now<WhyUnit->NextShotTime && WhyDiag.PeekAimUntil.FindRef(ID)>Now && WhyDiag.PeekAimUntil.FindRef(ID)==WhyUnit->NextShotTime)WhyClass=5;
 else if(Now<WhyUnit->NextShotTime && WhyDiag.PeekIntervalUntil.FindRef(ID)==WhyUnit->NextShotTime)WhyClass=6;
 else if(WhyUnit->ReloadEndTime>0)WhyClass=7;
 else if(WhyUnit->Ammo<=0 && WhyUnit->Reserve<=0)WhyClass=8;
 else if(Ray==0)WhyClass=9;
 ++WhyEpisode->Frames[WhyClass];
}
