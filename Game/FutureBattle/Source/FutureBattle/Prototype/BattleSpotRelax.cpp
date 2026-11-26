#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleSectors.h"

namespace
{
 FIntPoint SpotCell(const FVector& At){return FIntPoint(FMath::FloorToInt32(At.X/5000.),FMath::FloorToInt32(At.Y/5000.));}
}

bool UBattleSquadTactics::AddKnownSpot(int32 Team,const FVector& At,double Seen,uint8 Kind,bool bLegacy)
{
 // Legacy writes intentionally replace equal timestamps: the last contact in the original iteration wins.
 // Do not even read/write trial counters in this path (all switches off).
 if(Team<0 || Team>1)return false;
 if(!bLegacy && (At.ContainsNaN() || !FMath::IsFinite(Seen) || Kind>5))return false;
 const FIntPoint Cell=SpotCell(At);auto& Spots=KnownSpots[Team];
 if(!bLegacy)
 {
  if(const auto* Old=Spots.Find(Cell))
  {
   if(Seen<=Old->Seen)return false;
   if((Kind==3 || Kind==4) && Old->Kind!=3 && Old->Kind!=4 && Seen-Old->Seen<=10.)return false;
  }
 }
 FKnownSpot Spot;Spot.At=At;Spot.Seen=Seen;Spot.Kind=Kind;Spots.Add(Cell,Spot);
 if(!bLegacy)++SpotWrites[Team][Kind];
 return true;
}

void UBattleSquadTactics::ResetSpotRelax()
{
 ShotSpotAt.Reset();LostSpots.Reset();
 for(int32 Team=0;Team<2;++Team)
 {
  HQSpots[Team].Reset();HQTakenAt[Team]=0.;SpotRelaxLogAt[Team]=-1.;SpotTruthSpots[Team]=0;
  for(int32 K=0;K<6;++K)SpotWrites[Team][K]=0;
 }
}

FVector UBattleSquadTactics::ShotSpotError(int32 ShooterID,const FVector& At,double Now,float Error,float Every)
{
 const uint64 Bucket=uint64(FMath::FloorToDouble(FMath::Max(0.,Now)/Every));
 const uint32 Seed=uint32(ShooterID)*0x9e3779b9u ^ uint32(Bucket)*0x85ebca6bu ^ uint32(Bucket>>32);
 FRandomStream Random{int32(Seed)};
 const double Angle=Random.FRand()*2.*PI,Radius=FMath::Sqrt(Random.FRand())*Error;
 return At+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0.);
}

bool UBattleSquadTactics::NoteHeardShotSpot(int32 ShooterID,int32 Team,const FVector& At,double Now,float Error,float Every,TFunctionRef<bool()> CanHear)
{
 if(Team<0 || Team>1 || Every<=0.f)return false;
 if(const double* Last=ShotSpotAt.Find(ShooterID);Last && Now>=*Last && Now-*Last<Every)return false;
 // Throttle attempts as well as delivered reports: an unheard automatic burst must not scan the army every shot.
 ShotSpotAt.Add(ShooterID,Now);
 if(!CanHear())return false;
 return AddKnownSpot(1-Team,ShotSpotError(ShooterID,At,Now,Error,Every),Now,2);
}

bool UBattleSquadTactics::ShotSpotListener(int32 Team,int32 ListenerTeam,bool bAlive,bool bVehicle,const FVector& At,const FVector& Listener,float Hear)
{
 return ListenerTeam==1-Team && bAlive && !bVehicle && FVector::DistSquared2D(At,Listener)<=FMath::Square(double(Hear));
}

bool UBattleSquadTactics::NoteShotSpot(int32 ShooterID,int32 Team,const FVector& At,double Now,float Hear,float Error,float Every)
{
 return NoteHeardShotSpot(ShooterID,Team,At,Now,Error,Every,[&]()
 {
  const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  for(const auto& U:Store->GetUnits())if(U.Value.Team==1-Team && U.Value.IsAlive() && !Runtime->IsVehicle(U.Key) &&
   ShotSpotListener(Team,U.Value.Team,true,false,At,Runtime->Position(U.Key),Hear))return true;
  return false;
 });
}

void UBattleSquadTactics::NotePointLost(int32 PointID,int32 NewOwner,const FVector& At,double Now,float Seconds)
{
 // Cancel this point's old report on recapture, without erasing a different source that replaced its square.
 if(const auto* Old=LostSpots.Find(PointID))
 {
  auto& Spots=KnownSpots[Old->Team];const auto* Spot=Spots.Find(SpotCell(Old->At));
  if(Spot && Spot->Kind==3 && Spot->Seen==Old->Written && Spot->At==Old->At)Spots.Remove(SpotCell(Old->At));
  LostSpots.Remove(PointID);
 }
 if(NewOwner<0 || NewOwner>1)return;
 FLostSpot Lost;Lost.Team=1-NewOwner;Lost.At=At;Lost.Until=Now+Seconds;
 if(AddKnownSpot(Lost.Team,At,Now,3))Lost.Written=Now;
 LostSpots.Add(PointID,Lost);
}

void UBattleSquadTactics::AdvanceLostSpots(int32 Team,double Now)
{
 for(auto It=LostSpots.CreateIterator();It;++It)
 {
  auto& Lost=It->Value;if(Lost.Team!=Team)continue;
  if(Now>=Lost.Until)
  {
   auto& Spots=KnownSpots[Team];const auto* Spot=Spots.Find(SpotCell(Lost.At));
   if(Spot && Spot->Kind==3 && Spot->Seen==Lost.Written && Spot->At==Lost.At)Spots.Remove(SpotCell(Lost.At));
   It.RemoveCurrent();continue;
  }
  if(AddKnownSpot(Team,Lost.At,Now,3))Lost.Written=Now;
 }
}

bool UBattleSquadTactics::SpotDepthAllows(const FVector& At,const FVector& Home,const FVector& EnemyHome,int32 Depth)
{
 if(Depth<=0)return true;
 const FIntPoint Cell=BattleSectors::Cell(At),Enemy=BattleSectors::Cell(EnemyHome);
 if(!BattleSectors::Valid(Cell) || !BattleSectors::Valid(Enemy))return false;
 // Same column orientation as BattleFrontLine.cpp::ColOf / FrontCell.
 const bool bLow=Home.X<0;const int32 Col=bLow?Cell.X:BattleSectors::Columns-1-Cell.X;
 const int32 EnemyCol=bLow?Enemy.X:BattleSectors::Columns-1-Enemy.X;
 return Col<=EnemyCol-Depth;
}

void UBattleSquadTactics::AdvanceHQReports(int32 Team,double Now,float Interval,float Delay,int32 Min,int32 Depth,const FVector& Home,const FVector& EnemyHome,const TMap<FIntPoint,int32>& Counts)
{
 if(Team<0 || Team>1 || Interval<=0.f)return;
 if(Now-HQTakenAt[Team]>=Interval)
 {
  HQTakenAt[Team]=Now;
  // Sort cells so insertion order does not depend on the population map's bucket layout.
  TArray<FIntPoint> Cells;Counts.GetKeys(Cells);Cells.Sort([](const FIntPoint& A,const FIntPoint& B){return A.X!=B.X?A.X<B.X:A.Y<B.Y;});
  for(const auto& Cell:Cells)if(BattleSectors::Valid(Cell) && Counts.FindChecked(Cell)>=Min)
  {
   const FVector At=BattleSectors::Center(Cell);if(!SpotDepthAllows(At,Home,EnemyHome,Depth))continue;
   FHQSpot Spot;Spot.At=At;Spot.Taken=Now;HQSpots[Team].Add(Spot);
  }
 }
 for(auto It=HQSpots[Team].CreateIterator();It;++It)if(Now-It->Taken>=Delay)
 {
  AddKnownSpot(Team,It->At,Now,4);It.RemoveCurrent();
 }
}

void UBattleSquadTactics::AdvanceHQSpots(int32 Team,double Now,float Interval,float Delay,int32 Min,int32 Depth)
{
 if(Team<0 || Team>1 || !BattleFieldLayout::Of(GetWorld()).bSectors)return;
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 const FVector Home=Match->AreaHome(Team),EnemyHome=Match->AreaHome(1-Team);
 if(!BattleSectors::Valid(BattleSectors::Cell(Home)) || !BattleSectors::Valid(BattleSectors::Cell(EnemyHome)))return;
 TMap<FIntPoint,int32> Counts;
 if(Now-HQTakenAt[Team]>=Interval)
 {
  const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  for(const auto& U:Store->GetUnits())if(U.Value.Team==1-Team && U.Value.IsAlive() && !Runtime->IsVehicle(U.Key))
  {
   const auto Cell=BattleSectors::Cell(Runtime->Position(U.Key));if(BattleSectors::Valid(Cell))++Counts.FindOrAdd(Cell);
  }
 }
 AdvanceHQReports(Team,Now,Interval,Delay,Min,Depth,Home,EnemyHome,Counts);
}

void UBattleSquadTactics::TestSpotRelax(UWorld* World,TFunctionRef<void(bool,const TCHAR*)> Check)
{
 // Never mutate the world's subsystem, actors, contacts, units, clock or RNG. This uninitialized instance owns only fixture tables.
 auto* T=NewObject<UBattleSquadTactics>(World);
 const FVector A(100,100,0),B(200,200,0);const FIntPoint Cell=SpotCell(A);
 Check(T->AddKnownSpot(0,A,20.,0) && !T->AddKnownSpot(0,B,19.,1) && !T->AddKnownSpot(0,B,20.,1) &&
  T->AddKnownSpot(0,B,21.,1) && T->KnownSpots[0].FindChecked(Cell).At==B,TEXT("spot newer timestamp wins; equal and older do not replace"));
 bool Protected=true;
 for(uint8 Fine:{uint8(0),uint8(1),uint8(2),uint8(5)})for(uint8 Coarse:{uint8(3),uint8(4)})
 {
  T->KnownSpots[0].Reset();T->AddKnownSpot(0,A,20.,Fine);
  Protected &= !T->AddKnownSpot(0,B,30.,Coarse) && T->KnownSpots[0].FindChecked(Cell).Kind==Fine && T->AddKnownSpot(0,B,30.01,Coarse);
 }
 Check(Protected,TEXT("spot coarse reports cannot replace any precise kind within ten seconds"));
 T->KnownSpots[0].Reset();T->ResetSpotRelax();TMap<FIntPoint,FKnownSpot> Legacy;
 for(const FVector& At:{A,FVector(6000,100,0),B,FVector(-100,-100,0),A})
 {
  FKnownSpot Spot;Spot.At=At;Spot.Seen=20.;Legacy.Add(SpotCell(At),Spot);T->AddKnownSpot(0,At,20.,0,true);
 }
 TArray<FIntPoint> Before,After;Legacy.GetKeys(Before);T->KnownSpots[0].GetKeys(After);bool Same=Before==After;
 for(const auto& P:Legacy){const auto* Got=T->KnownSpots[0].Find(P.Key);Same &= Got && Got->At==P.Value.At && Got->Seen==P.Value.Seen && Got->Kind==0;}
 Check(Same && T->SpotWrites[0][0]==0,TEXT("spot legacy path matches unconditional Add contents and insertion order without counters"));
 T->KnownSpots[0].Reset();T->KnownSpots[1].Reset();T->ResetSpotRelax();
 int32 Scans=0;FVector Listener=A+FVector(1001,0,0);
 auto Hear=[&](){++Scans;return ShotSpotListener(0,1,true,false,A,Listener,1000.f);};
 Check(!T->NoteHeardShotSpot(7,0,A,10.,0.f,2.f,Hear) && T->KnownSpots[1].IsEmpty(),TEXT("spot unheard shot writes no square"));
 Listener=A+FVector(1000,0,0);
 Check(!T->NoteHeardShotSpot(7,0,A,11.,0.f,2.f,Hear) && Scans==1,TEXT("spot shot interval runs before hearing scan, including unheard shots"));
 Check(T->NoteHeardShotSpot(7,0,A,12.,0.f,2.f,Hear) && T->KnownSpots[0].IsEmpty() &&
  T->KnownSpots[1].FindChecked(Cell).Kind==2,TEXT("spot heard shot goes only to enemy army at inclusive hearing boundary"));
 Check(!T->NoteHeardShotSpot(7,0,A,13.,0.f,2.f,Hear) && Scans==2,TEXT("spot second heard shot inside interval does not scan or write"));
 Check(!ShotSpotListener(0,0,true,false,A,A,1000.f) && !ShotSpotListener(0,1,false,false,A,A,1000.f) &&
  !ShotSpotListener(0,1,true,true,A,A,1000.f),TEXT("spot hearing excludes allies, dead soldiers and vehicles"));
 bool ErrorOK=true;
 for(int32 ID=1;ID<=64;++ID)
 {
  const FVector P=ShotSpotError(ID,A,123.5,2500.f,2.f);
  ErrorOK &= P==ShotSpotError(ID,A,123.5,2500.f,2.f) && FVector::DistSquared2D(P,A)<=FMath::Square(2500.) && P.Z==A.Z;
 }
 Check(ErrorOK && ShotSpotError(7,A,10.,0.f,2.f)==A,TEXT("spot shot error is deterministic and within disk, including zero error"));
 T->KnownSpots[0].Reset();T->KnownSpots[1].Reset();T->ResetSpotRelax();
 T->NotePointLost(1,1,A,20.,5.f);T->AdvanceLostSpots(0,24.);
 Check(T->KnownSpots[1].IsEmpty() && T->KnownSpots[0].FindChecked(Cell).Seen==24.,TEXT("spot lost point refreshes only the new owner's enemy army"));
 T->AdvanceLostSpots(0,25.);
 Check(T->KnownSpots[0].IsEmpty() && T->LostSpots.IsEmpty(),TEXT("spot lost point expires at its deadline"));
 T->NotePointLost(1,1,A,30.,60.f);T->NotePointLost(1,0,A,31.,60.f);
 Check(T->KnownSpots[0].IsEmpty() && T->KnownSpots[1].FindChecked(Cell).Kind==3,TEXT("spot recapture cancels former report and informs the other army"));
 T->AddKnownSpot(1,B,32.,0);T->AdvanceLostSpots(1,91.);
 Check(T->KnownSpots[1].FindChecked(Cell).Kind==0,TEXT("spot lost expiry preserves a newer precise report in the same square"));
 T->KnownSpots[0].Reset();T->KnownSpots[1].Reset();T->ResetSpotRelax();
 const FVector Home=BattleSectors::Center({1,7}),Enemy=BattleSectors::Center({18,7});
 TMap<FIntPoint,int32> Counts;Counts.Add({10,7},2);Counts.Add({11,7},1);Counts.Add({17,7},3);Counts.Add({16,7},2);Counts.Add({-1,7},5);
 T->AdvanceHQReports(0,9.,10.f,30.f,2,2,Home,Enemy,Counts);
 Check(T->HQSpots[0].IsEmpty(),TEXT("spot HQ waits for first snapshot interval"));
 T->AdvanceHQReports(0,10.,10.f,30.f,2,2,Home,Enemy,Counts);
 Counts.Reset();T->AdvanceHQReports(0,39.,10.f,30.f,2,2,Home,Enemy,Counts);
 Check(T->KnownSpots[0].IsEmpty() && T->HQSpots[0].Num()==2,TEXT("spot HQ snapshot waits for delay and excludes sparse, deep and invalid cells"));
 T->AdvanceHQReports(0,40.,10.f,30.f,2,2,Home,Enemy,Counts);
 const FVector Center=BattleSectors::Center({10,7});const auto* Delivered=T->KnownSpots[0].Find(SpotCell(Center));
 Check(Delivered && Delivered->At==Center && Delivered->Seen==40. && Delivered->Kind==4 && T->KnownSpots[0].Num()==2 &&
  T->HQSpots[0].IsEmpty() && T->KnownSpots[1].IsEmpty(),TEXT("spot HQ delivers captured sector centers without resampling, stamped at delivery"));
 Check(SpotDepthAllows(BattleSectors::Center({16,7}),Home,Enemy,2) && !SpotDepthAllows(BattleSectors::Center({17,7}),Home,Enemy,2) &&
  SpotDepthAllows(BattleSectors::Center({3,7}),Enemy,Home,2) && !SpotDepthAllows(BattleSectors::Center({2,7}),Enemy,Home,2) &&
  SpotDepthAllows(Enemy,Home,Enemy,0),TEXT("spot HQ and truth share mirrored depth boundary and unlimited zero"));
 T->KnownSpots[0].Reset();T->KnownSpots[1].Reset();T->ResetSpotRelax();
 Counts.Add({10,7},2);T->AdvanceHQReports(1,10.,10.f,0.f,2,0,Enemy,Home,Counts);
 Check(T->KnownSpots[1].Num()==1 && T->HQSpots[1].IsEmpty(),TEXT("spot HQ zero delay delivers in snapshot tick"));
 // All fixture state belongs to T and is discarded; no Reset() on the live subsystem (which would reset team AI).
}
