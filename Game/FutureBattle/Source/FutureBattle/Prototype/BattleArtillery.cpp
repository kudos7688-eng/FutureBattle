// Indirect fire, stage 1 (ROADMAP 2026-11-14 heavy equipment order: ... armoured car -> indirect fire; first values 2026-11-18, numbers
// delegated): the mortar team (squad type 6; slot 1 carries EBattleWeapon::Mortar) and the self-propelled gun (squad type 7, a vehicle).
// Both set up while standing still (moving undoes it), then lob shells at positions their team knows: what the unit sees itself plus its
// team's knowledge (soldiers' sightings, scout and lookout reports; UBattleMatch::AreaIntelView), like the tank main gun. True positions
// of unseen enemies are never used; own units are true positions (a team knows where its own soldiers are). Shells are scheduled and land
// after their flight time, so a target may have moved away. The old support strike (BattleSupport.cpp, the old all-Actor FastMatch) is
// not reused: it works on Actors and a team-wide credit budget; only its idea (a delayed impact at an observed position) is kept.
#include "BattleLeaderRuntime.h"
#include "BattleAudio.h"
#include "BattleEffects.h"
#include "BattleMuzzleFlash.h"
#include "BattleTactics.h"
#include "BattleState.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleSuppression.h"
#include "BattleUnit.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Math/RandomStream.h"
#include "BattleFieldLayout.h"
#include "BattleVehicleNavMesh.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"

bool BattleTactics::ArtilleryObservedEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryObserved="),V);return V!=0;}();
 return On;
}

bool BattleTactics::VehicleOrderHere()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleVehicleOrderHere="),V);return V!=0;}();
 return On;
}

bool BattleTactics::VehicleHitCover()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleVehicleHitCover="),V);return V==1;}();
 return On;
}

bool BattleTactics::TankGunFlash()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleTankGunFlash="),V);return V==1;}();
 return On;
}

bool BattleTactics::ArtilleryFlash()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryFlash="),V);return V==1;}();
 return On;
}

// Counter-battery fire (2026-11-23, user request after the subtraction showed the gun unanswered). -BattleCounterBattery=1 (default 0;
// the trial team only with -BattleAreaTrialTeam): a self-propelled gun shoots first at the newest fresh report of enemy artillery fire
// within its range (BATTLE_ARTILLERY_FLASH reports, so -BattleArtilleryFlash=1 is needed; observed reports only, never the true position).
bool BattleTactics::CounterBattery(int32 Team)
{
 static const int32 V=[]{int32 X=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCounterBattery="),X);return X;}();
 if(V==0||!ArtilleryFlash())return false;
 const int32 Trial=AreaStrategy().TrialTeam;
 return Trial<0||Trial==Team;
}

// "Sometimes" counter-battery (2026-11-23, user choice after the always-on version spent the own gun on the enemy gun):
// -BattleCounterBatteryShots=<n> (default 0 = every shell while a report is fresh): only a report of the last 15 s (the enemy gun is
// firing now) is answered, with n shells; then the gun rests -BattleCounterBatteryRest=<s> (default 60) shooting only at soldiers.
int32 BattleTactics::CounterBatteryShots()
{
 static const int32 V=[]{int32 X=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCounterBatteryShots="),X);return FMath::Max(0,X);}();
 return V;
}
float BattleTactics::CounterBatteryRest()
{
 static const float V=[]{float X=60.f;FParse::Value(FCommandLine::Get(),TEXT("BattleCounterBatteryRest="),X);return FMath::Max(0.f,X);}();
 return V;
}

// -BattleArtilleryVehicleSplash=<0..1> (default 0 = vehicles only by a direct hit, as before): a gun shell also hurts vehicles within its
// blast radius by this share of the soldiers' falloff (both teams; physics, not a trial-team choice).
float BattleTactics::ArtilleryVehicleSplash()
{
 static const float V=[]{float X=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryVehicleSplash="),X);return FMath::IsFinite(X)?FMath::Clamp(X,0.f,1.f):0.f;}();
 return V;
}

// 2026-11-23 (user decision "1と3" after counter-battery fire hit the home behind which the enemy gun stood): two trials, both default 0.
// -BattleRepairDepot=<m>: vehicles and exoskeleton soldiers are repaired at a depot <m> beside their home (UBattleMatch::RepairPoint)
// instead of at the home; ammunition and replacements stay at home. -BattleArtilleryHomeGap=<m>: an artillery post (AreaArtilleryPost)
// keeps at least <m> from the own home, on the side away from the depot.
float BattleTactics::RepairDepot()
{
 static const float V=[]{float X=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleRepairDepot="),X);return FMath::IsFinite(X)?FMath::Clamp(X,0.f,400.f)*100.f:0.f;}();
 return V;
}
float BattleTactics::ArtilleryHomeGap()
{
 static const float V=[]{float X=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryHomeGap="),X);return FMath::IsFinite(X)?FMath::Clamp(X,0.f,300.f)*100.f:0.f;}();
 return V;
}
// The depot beside the home (the point-symmetric side: +Y for the home on +X), projected on the navigation mesh once per team; the
// home itself without -BattleRepairDepot or when no navigable point is found.
FVector UBattleMatch::RepairPoint(int32 Team) const
{
 const FVector Home=AreaHome(Team);const float Offset=BattleTactics::RepairDepot();
 if(Offset<=0.f||Team<0||Team>1)return Home;
 if(!bDepotPointBuilt[Team])
 {
  bDepotPointBuilt[Team]=true;DepotPoint[Team]=Home;
  FVector Want=Home+FVector(0.,(Home.X>0?1.:-1.)*Offset,0.);
  if(const auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
  {FNavLocation L;if(Nav->ProjectPointToNavigation(Want,L,FVector(2000.,2000.,5000.)))DepotPoint[Team]=L.Location;}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_REPAIR_DEPOT team=%d home=%s depot=%s offset=%.0f"),Team,*Home.ToCompactString(),*DepotPoint[Team].ToCompactString(),Offset);
 }
 return DepotPoint[Team];
}

// The depot can be broken (2026-11-23, user proposal: return fire and raids that hit the depot make firing next to it a risk).
// -BattleDepotHealth=<hp> (default 0 = it never breaks): a gun shell within its blast radius plus 3 m does the soldiers' falloff damage,
// and every living enemy within 25 m wears it down by 4 hp/s. Broken, it repairs nothing for -BattleDepotRebuild=<s> (default 120);
// machines then go to the home to be repaired, as without the depot. Then it is whole again.
float BattleTactics::DepotHealth()
{
 static const float V=[]{float X=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleDepotHealth="),X);return FMath::IsFinite(X)?FMath::Max(0.f,X):0.f;}();
 return RepairDepot()>0.f?V:0.f;
}
float BattleTactics::DepotRebuild()
{
 static const float V=[]{float X=120.f;FParse::Value(FCommandLine::Get(),TEXT("BattleDepotRebuild="),X);return FMath::IsFinite(X)?FMath::Max(0.f,X):120.f;}();
 return V;
}
float BattleTactics::ArtilleryPostRisk()
{
 static const float V=[]{float X=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryPostRisk="),X);return FMath::IsFinite(X)?FMath::Max(0.f,X):0.f;}();
 return RepairDepot()>0.f?V:0.f;
}
bool UBattleMatch::DepotWorking(int32 Team) const
{
 return BattleTactics::RepairDepot()>0.f&&Team>=0&&Team<2&&!(DepotDownUntil[Team]>=0.&&Elapsed<DepotDownUntil[Team]);
}
void UBattleMatch::DamageDepot(int32 Team,float Damage,const TCHAR* Cause)
{
 const float Health=BattleTactics::DepotHealth();
 if(Health<=0.f||Damage<=0.f||!DepotWorking(Team))return;
 DepotDamage[Team]+=Damage;
 if(DepotDamage[Team]>=Health)
 {
  DepotDamage[Team]=0.f;DepotDownUntil[Team]=Elapsed+BattleTactics::DepotRebuild();++DepotBreaks[Team];
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DEPOT_BROKEN team=%d cause=%s at=%.1f until=%.1f breaks=%d"),Team,Cause,Elapsed,DepotDownUntil[Team],DepotBreaks[Team]);
 }
}
void UBattleMatch::UpdateDepots(float Dt)
{
 if(BattleTactics::DepotHealth()<=0.f||Dt<=0.f)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return;
 for(int32 Team=0;Team<2;++Team)
 {
  if(DepotDownUntil[Team]>=0.&&Elapsed>=DepotDownUntil[Team])
  {DepotDownUntil[Team]=-1.;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DEPOT_REBUILT team=%d at=%.1f"),Team,Elapsed);}
  if(!DepotWorking(Team))continue;
  const FVector Depot=RepairPoint(Team);int32 Near=0;
  for(const auto& S:Store->GetSquads())if(S.Value.Team==1-Team)
   for(const int32 ID:S.Value.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()&&FVector::DistSquared2D(R->Position(ID),Depot)<=FMath::Square(2500.))++Near;
  if(Near>0)DamageDepot(Team,4.f*Near*Dt,TEXT("raid"));
 }
}

bool BattleTactics::ArtilleryPosts()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryPosts="),V);return V==1;}();
 return On;
}

bool BattleTactics::ArtilleryRaidChase()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryRaidChase="),V);return V==1;}();
 return On&&ArtilleryPosts();
}

float BattleTactics::ArtilleryRaidSneak()
{
 static const float Metres=[]{float V=0.f;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryRaidSneak="),V);return FMath::IsFinite(V)?FMath::Clamp(V,0.f,500.f):0.f;}();
 return ArtilleryPosts()?Metres*100.f:0.f;
}

int32 BattleTactics::ArtilleryRaidGroup()
{
 static const int32 Size=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryRaidGroup="),V);return V>=2?FMath::Min(V,3):0;}();
 return ArtilleryRaidChase()?Size:0;
}

int32 BattleTactics::ArtilleryRaidScreen()
{
 static const int32 Count=[]{int32 V=2;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryRaidScreen="),V);return FMath::Clamp(V,0,4);}();
 return ArtilleryRaidGroup()>0?Count:0;
}

int32 BattleTactics::AreaSPGCost()
{
 static int32 OverrideCost=35;
 static const bool HasOverride=FParse::Value(FCommandLine::Get(),TEXT("BattleAreaSPGCost="),OverrideCost);
 return FMath::Clamp(HasOverride?OverrideCost:GetDefault<UBattleTuning>()->AreaSPGCost,1,200);
}

float BattleTactics::SPGRange()
{
 static float OverrideRange=GetDefault<UBattleTuning>()->SPGRange;
 static const bool HasOverride=FParse::Value(FCommandLine::Get(),TEXT("BattleSPGRange="),OverrideRange);
 const auto* RangeTuning=GetDefault<UBattleTuning>();
 // Preserve the raw config without an override, including the original spread reference.
 return HasOverride?FMath::Clamp(OverrideRange,FMath::Max(0.f,RangeTuning->SPGMinRange)+100.f,200000.f):RangeTuning->SPGRange;
}

float BattleTactics::ArtilleryStandoff()
{
 static float OverrideStandoff=GetDefault<UBattleTuning>()->ArtilleryStandoff;
 static const bool HasOverride=FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryStandoff="),OverrideStandoff)&&FMath::IsFinite(OverrideStandoff);
 return HasOverride?FMath::Clamp(OverrideStandoff,0.f,1.f):GetDefault<UBattleTuning>()->ArtilleryStandoff;
}

namespace { int32 SaveReloadBoundarySmokeOverride=-1; }
bool BattleTactics::SaveReloadBoundary()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSaveReloadBoundary="),V);return V==1;}();
 return SaveReloadBoundarySmokeOverride>=0?SaveReloadBoundarySmokeOverride==1:On;
}
int32 BattleTactics::OverrideSaveReloadBoundaryForSmoke(int32 Value)
{
 const int32 Previous=SaveReloadBoundarySmokeOverride;
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleMixedSaveSmoke")))SaveReloadBoundarySmokeOverride=FMath::Clamp(Value,-1,1);
 return Previous;
}

bool BattleTactics::SPGAmmoEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSPGAmmo="),V);return V!=0;}();
 return On;
}

bool BattleTactics::BlastOcclusionEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleBlastOcclusion="),V);return V!=0;}();
 return On;
}

bool BattleTactics::ArtilleryClustersEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryClusters="),V);return V!=0;}();
 return On;
}

bool UBattleLeaderRuntime::IsArtillery(int32 ID) const
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
 if(!U||!U->IsAlive())return false;
 return U->Weapon==EBattleWeapon::Mortar||IsSPG(ID);
}

float UBattleLeaderRuntime::ArtilleryMinRange(bool bSPG) const
{
 const auto* T=GetDefault<UBattleTuning>();
 return FMath::Max(0.f,bSPG?T->SPGMinRange:T->MortarMinRange);
}

float UBattleLeaderRuntime::ArtilleryMaxRange(bool bSPG) const
{
 // Mortar: 300 m, 400 m with the range profile (map E). Gun: configured 1000 m unless overridden for the trial.
 const auto* T=GetDefault<UBattleTuning>();
 const float Range=bSPG?BattleTactics::SPGRange():(BattleRanges::Get().bActive?T->MortarRangeProfile:T->MortarRange);
 return FMath::Max(ArtilleryMinRange(bSPG)+100.f,Range);
}

float UBattleLeaderRuntime::ArtilleryBlastRadius(bool bSPG) const
{
 const auto* T=GetDefault<UBattleTuning>();
 return FMath::Max(50.f,bSPG?T->SPGBlastRadius:T->MortarBlastRadius);
}

float UBattleLeaderRuntime::ArtillerySpread(bool bSPG,double Distance) const
{
 // The landing circle grows with distance: MortarSpread at MortarRange (8 m at 300 m), SPGSpread at SPGRange (15 m at 1000 m).
 const auto* T=GetDefault<UBattleTuning>();
 const float At=FMath::Max(0.f,bSPG?T->SPGSpread:T->MortarSpread),Reference=FMath::Max(100.f,bSPG?BattleTactics::SPGRange():T->MortarRange);
 return At*float(FMath::Max(0.0,Distance))/Reference;
}

FVector UBattleMatch::AreaArtilleryPost(int32 Team,int32 SquadID,const FVector& Goal) const
{
 // Minimal tactical role: keep the squad ArtilleryStandoff x its range away from every enemy its team knows of, walking back from the
 // assigned goal towards the own home in 10 m steps (the home when no step is clear). Known positions only (AreaIntelView).
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||!BattleSquadType::IsArtillery(S->SquadType))return Goal;
 const double Range=R->ArtilleryMaxRange(S->SquadType==BattleSquadType::SPGun);
 const double Keep=FMath::Clamp(BattleTactics::ArtilleryStandoff(),0.f,1.f)*Range;
 const auto& V=AreaIntelView(Team);
 if(Keep<=0.||V.Enemies.IsEmpty())return Goal;
 auto Clear=[&](const FVector& P){for(const FVector& E:V.Enemies)if(FVector::DistSquared2D(P,E)<Keep*Keep)return false;return true;};
 FVector Post=Goal;
 if(!Clear(Goal))
 {
  const FVector Home=AreaHome(Team);const double Length=FVector::Dist2D(Goal,Home);const FVector Back=(Home-Goal).GetSafeNormal2D();
  Post=Home;
  for(double Step=1000.;Step<Length;Step+=1000.){const FVector P=Goal+Back*Step;if(Clear(P)){Post=P;break;}}
 }
 // Stage 2 (2026-11-18), moving into range: when no known enemy is within ArtilleryAdvance x range of the post, the post moves towards the
 // nearest known enemy until that enemy is ArtilleryAdvance x range away, stepping back in 10 m steps while the standoff is not clear.
 const double Reach=FMath::Clamp(GetDefault<UBattleTuning>()->ArtilleryAdvance,0.f,1.f)*Range;
 if(Reach>Keep)
 {
  const FVector* Nearest=nullptr;double NearestD=DBL_MAX;
  for(const FVector& E:V.Enemies){const double D=FVector::Dist2D(Post,E);if(D<NearestD){NearestD=D;Nearest=&E;}}
  if(Nearest&&NearestD>Reach)
  {
   const FVector Dir=(Post-*Nearest).GetSafeNormal2D();
   for(double Step=0.;Step<=NearestD-Reach;Step+=1000.)
   {
    FVector P=*Nearest+Dir*(Reach+Step);P.Z=Post.Z;
    if(Clear(P)){Post=P;break;}
   }
  }
 }
 // Trial -BattleArtilleryHomeGap: not within the gap of the own home (counter-battery fire at the gun would hit the home); pushed out
 // from the home, straight sideways (away from the depot side) when the post is the home itself.
 // Trial -BattleArtilleryPostRisk (with the depot): instead of the fixed sideways push, the post is scored. Cost in metres = the distance
 // to the depot (a damaged gun drives there) + Risk x the chance that return fire at the gun lands on the depot: 1 within the blast
 // radius plus 3 m of it, falling to 0 at that plus 30 m (the error of the averaged fire reports) plus the gun's spread at 60% range.
 // Candidates: the post itself when outside the home gap, and points 0/20/40 m beyond the gap round the home (not behind it), on the
 // navigation mesh and clear of the standoff. None found keeps the fixed push below.
 const float PostRisk=BattleTactics::ArtilleryPostRisk();
 bool bScored=false;
 if(PostRisk>0.f&&S->SquadType==BattleSquadType::SPGun)
 {
  const FVector Home=AreaHome(Team),Depot=RepairPoint(Team);const double Gap=BattleTactics::ArtilleryHomeGap();
  const double Safe=double(R->ArtilleryBlastRadius(true))+300.,RiskReach=Safe+3000.+R->ArtillerySpread(true,Range*.6);
  auto Chance=[&](const FVector& P){return FMath::Clamp(1.-(FVector::Dist2D(P,Depot)-Safe)/FMath::Max(1.,RiskReach-Safe),0.,1.);};
  auto Cost=[&](const FVector& P){return FVector::Dist2D(P,Depot)/100.+double(PostRisk)*Chance(P);};
  const bool bPostOk=FVector::Dist2D(Post,Home)>=Gap;
  if(!bPostOk||Chance(Post)>0.)
  {
   // Near the home the standoff is often not clear anywhere (the post fell back to the home): then the candidates are scored without it.
   const bool bAnyClear=Clear(Post);
   FVector Best=Post;double BestCost=bPostOk?Cost(Post):DBL_MAX;
   const auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   const double HomeSign=Home.X>0?1.:-1.;
   for(int32 Ring=0;Ring<3;++Ring)for(int32 A=0;A<24;++A)
   {
    const double Angle=A*PI/12.;const FVector Dir(FMath::Cos(Angle),FMath::Sin(Angle),0.);
    if(Dir.X*HomeSign>.2)continue; // behind the home is the map edge
    FVector P=Home+Dir*(FMath::Max(Gap,1000.)+Ring*2000.);P.Z=Home.Z;
    if(Nav){FNavLocation L;if(!Nav->ProjectPointToNavigation(P,L,FVector(500.,500.,5000.)))continue;P=L.Location;}
    if(bAnyClear&&!Clear(P))continue;
    const double C=Cost(P);if(C<BestCost){BestCost=C;Best=P;}
   }
   if(BestCost<DBL_MAX)
   {
    Post=Best;bScored=true;
   }
  }
  else bScored=true;
 }
 if(const float Gap=BattleTactics::ArtilleryHomeGap();Gap>0.f&&!bScored)
 {
  const FVector Home=AreaHome(Team);
  if(FVector::Dist2D(Post,Home)<Gap)
  {
   FVector Out=(Post-Home).GetSafeNormal2D();
   if(Out.IsNearlyZero()||Out.X*Home.X>0.)Out=FVector(0.,Home.X>0?-1.:1.,0.); // behind the home is the map edge: go sideways
   Post=Home+Out*double(Gap);Post.Z=Home.Z;
  }
 }
 // A post near the current mission is kept: no new order for every small change in what the team knows.
 if(S->bHasMission&&FVector::Dist2D(S->MissionDestination,Post)<2500.)return S->MissionDestination;
 return Post;
}

bool UBattleLeaderRuntime::AllyNear(int32 Team,const FVector& At,double Radius) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const double R2=Radius*Radius;
 for(const auto& B:Bodies)
 {
  const auto* U=Store->Find(B.Key);
  if(U&&U->IsAlive()&&U->Team==Team&&FVector::DistSquared2D(Position(B.Key),At)<=R2)return true;
 }
 return false;
}

namespace BattleGroupFire
{
 // Group fire support (trial -BattleGroupFireSupport=1, user approval 2026-11-21; ROADMAP "群に入れた自走砲・迫撃砲の支援").
 // Default 1 by user decision 2026-11-21 (unmeasured: the standard matches had no artillery then). Only an explicit =1 is a trial that
 // -BattleAreaTrialTeam limits to one team (2026-11-22 fix: a one-sided trial of another switch turned it off for the other team).
 bool On(int32 Team){static const int32 V=[]{int32 X=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleGroupFireSupport="),X);return X;}();if(V<0)return true;const int32 Trial=BattleTactics::AreaStrategy().TrialTeam;return V!=0&&(Trial<0||Trial==Team);}
 constexpr double TroubleNear=4000., HelpRadius=6000., RallyRadius=8000.;
 constexpr double TroubleBonus=100., RallyBonus=50.;
}

namespace {
template<typename Candidate,typename Safety>
int32 FirstSafeArtillery(const TArray<Candidate>& Candidates,bool Unlimited,Safety IsSafe)
{
 for(int32 I=0;I<Candidates.Num()&&(Unlimited||I<8);++I)if(IsSafe(Candidates[I]))return I;
 return INDEX_NONE;
}

TArray<UBattleMatch::FArtilleryCluster> BuildArtilleryClusters(TArray<TPair<int32,FVector>> Soldiers,double Radius,int32 Minimum)
{
 // Stable accumulation order, independent of TMap insertion order and CPU timing.
 Soldiers.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
 TArray<UBattleMatch::FArtilleryCluster> Result;
 TMap<FString,int32> ByMembers;
 for(const auto& Seed:Soldiers)
 {
  FVector Sum=FVector::ZeroVector;int32 Count=0;FString Members;
  for(const auto& Other:Soldiers)if(FVector::DistSquared2D(Seed.Value,Other.Value)<=Radius*Radius)
  {Sum+=Other.Value;++Count;Members+=FString::FromInt(Other.Key)+TEXT(",");}
  if(Count<Minimum)continue;
  if(const int32* Existing=ByMembers.Find(Members)){Result[*Existing].Anchors.Add(Seed.Key);continue;}
  UBattleMatch::FArtilleryCluster C;C.At=Sum/double(Count);C.Count=Count;C.Anchors.Add(Seed.Key);
  ByMembers.Add(Members,Result.Num());Result.Add(MoveTemp(C));
 }
 return Result;
}
}

const TArray<UBattleMatch::FArtilleryCluster>& UBattleMatch::AreaArtilleryClusters(int32 Team,bool bSPG) const
{
 const auto& V=AreaIntelView(Team);const int32 Kind=bSPG?1:0;
 if(!V.bClustersBuilt[Kind])
 {
  const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
  TArray<TPair<int32,FVector>> Soldiers;
  for(int32 I=0;I<V.Enemies.Num();++I)if(!(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(V.ObservedType(V.EnemyKeys[I])):V.EnemyKeys[I]>0&&R->IsVehicle(V.EnemyKeys[I])))
   Soldiers.Emplace(V.EnemyKeys[I],V.Enemies[I]);
  V.Clusters[Kind]=BuildArtilleryClusters(MoveTemp(Soldiers),R->ArtilleryBlastRadius(bSPG),FMath::Max(1,GetDefault<UBattleTuning>()->ArtilleryMinCluster));
  V.bClustersBuilt[Kind]=true;
 }
 return V.Clusters[Kind];
}

bool UBattleLeaderRuntime::PickArtilleryAim(int32 ID,const TMap<int32,FVector>& Known,FVector& Aim,bool& bBase,bool& bFriendlyHeld,int32* TargetKey,bool bTeamClusters,double Now,const TMap<int32,uint8>* KnownTypes)
{
 bBase=false;bFriendlyHeld=false;if(TargetKey)*TargetKey=INDEX_NONE;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U||!U->IsAlive())return false;
 const auto* T=GetDefault<UBattleTuning>();const bool bSPG=IsSPG(ID);
 const FVector Pos=Position(ID);
 if(BattleTactics::ArtilleryPosts()&&!bSPG&&!GetWorld()->GetSubsystem<UBattleMatch>()->AtArtilleryPost(U->Team,Pos))return false;
 const double Min2=FMath::Square(double(ArtilleryMinRange(bSPG))),Max2=FMath::Square(double(ArtilleryMaxRange(bSPG)));
 const double Blast=ArtilleryBlastRadius(bSPG),Blast2=Blast*Blast;
 // Counter-battery (-BattleCounterBattery=1): the newest fresh artillery report in range comes first; ties by shooter key.
 const int32 CBShots=BattleTactics::CounterBatteryShots();
 const double* CBRest=CBShots>0?CounterBatteryRestUntil.Find(ID):nullptr;
 if(bSPG&&U->Team>=0&&U->Team<2&&BattleTactics::CounterBattery(U->Team)&&!(CBRest&&Now<*CBRest))
  if(const auto* CM=GetWorld()->GetSubsystem<UBattleMatch>())
  {
   int32 BestKey=INDEX_NONE;const UBattleMatch::FFireReveal* Best=nullptr;
   for(const auto& FR:CM->FireReveals[U->Team])
   {
    const auto& R=FR.Value;if(!R.bArtillery||CM->Elapsed-R.At>(CBShots>0?FMath::Min(15.,double(R.Memory)):R.Memory)||CM->Elapsed<R.At)continue;
    const double D2=FVector::DistSquared2D(Pos,R.Position);if(D2<Min2||D2>Max2)continue;
    if(!Best||R.At>Best->At||(R.At==Best->At&&FR.Key<BestKey)){Best=&R;BestKey=FR.Key;}
   }
   const FVector BestAt=Best&&Best->Count>0?Best->Sum/double(Best->Count):Best?Best->Position:FVector::ZeroVector; // the mean of the gun's repeated reports
   if(Best&&!AllyNear(U->Team,BestAt,Blast+ArtillerySpread(bSPG,FVector::Dist2D(Pos,BestAt))))
   {
    Aim=BestAt;
    if(const int32* Last=CounterBatteryTarget.Find(ID);!Last||*Last!=BestKey)
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COUNTER_BATTERY unit=%d team=%d target=%d aim=(%.0f,%.0f) age=%.1f range=%.0f reports=%d"),ID,U->Team,BestKey,Aim.X,Aim.Y,CM->Elapsed-Best->At,FVector::Dist2D(Pos,Aim),Best->Count);
    CounterBatteryTarget.Add(ID,BestKey);++CounterBatteryAims;
    return true;
   }
  }
 CounterBatteryTarget.Remove(ID);
 // Soldiers only: the explosion does not hurt vehicles (a gun's direct hit on one is luck, not a plan).
 TArray<TPair<int32,FVector>> Soldiers;
 if(!BattleTactics::ArtilleryClustersEnabled()||!bTeamClusters)
 for(const auto& K:Known)
 {
  const uint8* KnownType=BattleTactics::ObservedArmour()&&KnownTypes?KnownTypes->Find(K.Key):nullptr;
  if(!(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(KnownType?*KnownType:uint8(255)):K.Key>0&&IsVehicle(K.Key)))Soldiers.Add(TPair<int32,FVector>(K.Key,K.Value));
 }
 // A candidate per known soldier: the centre of the known soldiers within the blast radius of him, worth a shell when it holds
 // ArtilleryMinCluster or more and lies within range. More soldiers first, then nearer.
 struct FCandidate{int32 Key;FVector At;double Score;int32 Helped=INDEX_NONE;};
 TArray<FCandidate> Candidates;
 if(BattleTactics::ArtilleryClustersEnabled())
 {
  const auto* M=GetWorld()->GetSubsystem<UBattleMatch>();
  TArray<UBattleMatch::FArtilleryCluster> Local;
  if(!bTeamClusters)Local=BuildArtilleryClusters(Soldiers,Blast,FMath::Max(1,T->ArtilleryMinCluster));
  const auto& Shared=bTeamClusters?M->AreaArtilleryClusters(U->Team,bSPG):Local;
  const auto* Squad=Store->FindSquad(U->SquadID);
  const bool SupportAim=BattleTactics::WeaponOrderGateEnabled()&&Squad&&Squad->TacticalPlan.Action==EBattleSquadTactic::Support;
  for(const auto& C:Shared)
  {
   for(int32 Key:C.Anchors)
   {
    const FVector* Anchor=Known.Find(Key);if(!Anchor)continue;
    const FVector Center=SupportAim?*Anchor:C.At;const double D2=FVector::DistSquared2D(Pos,Center);
    if(D2<Min2||D2>Max2)continue;
    if(BattleTactics::WeaponOrderGateEnabled()&&(!WeaponOrderAllows(ID,Key,*Anchor)||!WeaponOrderAllows(ID,Key,Center)))continue;
    Candidates.Add({Key,Center,double(C.Count)-FMath::Sqrt(D2)/1.e6});
    // Identical member sets have one centroid; support must keep its specifically permitted anchor.
    break;
   }
  }
 }
 else
 {
 for(const auto& K:Soldiers)
 {
  if(BattleTactics::WeaponOrderGateEnabled()&&!WeaponOrderAllows(ID,K.Key,K.Value))continue;
  int32 Around=0;FVector Sum=FVector::ZeroVector;
  for(const auto& O:Soldiers)if(FVector::DistSquared2D(O.Value,K.Value)<=Blast2){++Around;Sum+=O.Value;}
  if(Around<FMath::Max(1,T->ArtilleryMinCluster))continue;
  const auto* Squad=Store->FindSquad(U->SquadID);
  const bool SupportAim=BattleTactics::WeaponOrderGateEnabled()&&Squad&&Squad->TacticalPlan.Action==EBattleSquadTactic::Support;
  const FVector Center=SupportAim?K.Value:Sum/double(Around);const double D2=FVector::DistSquared2D(Pos,Center);
  if(D2<Min2||D2>Max2)continue;
  if(BattleTactics::WeaponOrderGateEnabled()&&!WeaponOrderAllows(ID,K.Key,Center))continue;
  Candidates.Add({K.Key,Center,double(Around)-FMath::Sqrt(D2)/1.e6});
 }
 }
 // Group fire support: a mortar or gun in a group shoots first at known soldiers near the group's squads in trouble (damage in the last
 // 5 s, a suppressed leader, or a known soldier within TroubleNear of the leader); with none in trouble, near the rally point of an attack
 // or surprise group. Own squads' state and known enemies only.
 if(BattleGroupFire::On(U->Team))if(const auto* GM=GetWorld()->GetSubsystem<UBattleMatch>();GM&&GM->bArea)if(const auto* G=GM->Groups.Find(GM->GroupOf(U->SquadID)))
 {
  using namespace BattleGroupFire;
  TArray<TPair<int32,FVector>> Trouble;const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
  for(int32 SID:G->Squads)if(SID!=U->SquadID)if(const auto* S=Store->FindSquad(SID))if(const auto* Lead=Store->Find(S->LeaderUnitID);Lead&&Lead->IsAlive())
  {
   const FVector P=Position(S->LeaderUnitID);
   bool bTrouble=Suppression(S->LeaderUnitID,Now)>=.5f;
   if(const auto* O=Tactics?Tactics->Find(SID):nullptr)bTrouble|=O->RecentDamage>0.f;
   for(const auto& K:Known)if(!bTrouble)
   {
    const uint8* TroubleKnownType=BattleTactics::ObservedArmour()&&KnownTypes?KnownTypes->Find(K.Key):nullptr;
    const bool bKnownVehicle=BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(TroubleKnownType?*TroubleKnownType:uint8(255)):K.Key>0&&IsVehicle(K.Key);
    if(!bKnownVehicle)bTrouble=FVector::DistSquared2D(K.Value,P)<=FMath::Square(TroubleNear);
   }
   if(bTrouble)Trouble.Add(TPair<int32,FVector>(SID,P));
  }
  for(auto& C:Candidates)
  {
   if(Trouble.Num()){for(const auto& H:Trouble)if(FVector::DistSquared2D(C.At,H.Value)<=FMath::Square(HelpRadius)){C.Score+=TroubleBonus;C.Helped=H.Key;break;}}
   else if(G->Kind!=1&&FVector::DistSquared2D(C.At,G->Rally)<=FMath::Square(RallyRadius))C.Score+=RallyBonus;
  }
 }
 Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Score!=B.Score?A.Score>B.Score:A.Key<B.Key;});
 // Never where the blast plus the landing spread may reach an own unit.
 const int32 Selected=FirstSafeArtillery(Candidates,BattleTactics::ArtilleryClustersEnabled(),[&](const FCandidate& C)
 {
  if(AllyNear(U->Team,C.At,Blast+ArtillerySpread(bSPG,FVector::Dist2D(Pos,C.At)))){bFriendlyHeld=true;return false;}
  return true;
 });
 if(Selected!=INDEX_NONE)
 {
  const auto& C=Candidates[Selected];Aim=C.At;if(TargetKey)*TargetKey=C.Key;
  if(C.Helped!=INDEX_NONE)
  {
   ++ArtilleryGroupAims;
   if(const int32* Last=ArtillerySupportSquad.Find(ID);!Last||*Last!=C.Helped)
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_GROUP_FIRE unit=%d squad=%d helps=%d aim=(%.0f,%.0f) spg=%d"),ID,U->SquadID,C.Helped,Aim.X,Aim.Y,bSPG?1:0);
   ArtillerySupportSquad.Add(ID,C.Helped);
  }
  else ArtillerySupportSquad.Remove(ID);
  return true;
 }
 // The enemy home: in range and seen by the unit itself or by its team (bHomeCovered), like the tank gun.
 const auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const int32 Enemy=1-U->Team;
 if(M&&M->bArea&&M->BaseActors.IsValidIndex(Enemy)&&M->BaseActors[Enemy]&&M->Area.BaseHP.IsValidIndex(Enemy)&&M->Area.BaseHP[Enemy]>0)
 {
  const FVector Home=M->BaseActors[Enemy]->GetActorLocation();const double D2=FVector::DistSquared2D(Pos,Home);
  if((!BattleTactics::WeaponOrderGateEnabled()||WeaponOrderAllows(ID,INDEX_NONE,Home))&&D2>=Min2&&D2<=Max2&&(BattleTactics::ArtilleryObservedEnabled()?M->ArtilleryHomeObserved(U->Team):(M->AreaIntelView(U->Team).bHomeCovered||M->AreaVisible(ID,Home,M->BaseActors[Enemy]))))
  {
   if(AllyNear(U->Team,Home,Blast+ArtillerySpread(bSPG,FMath::Sqrt(D2))))bFriendlyHeld=true;
   else{Aim=Home;bBase=true;return true;}
  }
 }
 return false;
}

bool UBattleLeaderRuntime::StepArtillery(int32 ID,double Now,bool bMoving,FVector& Direction)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive()){Artillery.Remove(ID);return false;}
 const auto* T=GetDefault<UBattleTuning>();const bool bSPG=IsSPG(ID);
 auto& A=Artillery.FindOrAdd(ID);
 if(BattleTactics::ArtilleryPosts()&&!bSPG&&!GetWorld()->GetSubsystem<UBattleMatch>()->AtArtilleryPost(U->Team,Position(ID)))
 {A.StillSince=-1;A.SetupCredit=0;A.bHasTarget=false;return false;}
 // Guard stale entries; restore rebases the clocks and carries elapsed setup separately.
 if(A.ReadyAt>Now+FMath::Max(0.f,T->SPGReload)+1.)A.ReadyAt=Now;
 if(A.StillSince>Now)A.StillSince=-1;
 // Moving undoes the set-up (the target is kept until the next pick).
 if(bMoving){A.StillSince=-1;A.SetupCredit=0;return false;}
 if(A.StillSince<0)A.StillSince=Now;
 auto* M=GetWorld()->GetSubsystem<UBattleMatch>();
 if(Now>=A.NextPick&&M&&M->bArea&&M->Phase==EBattleMatchPhase::Combat)
 {
  const double PickStart=FPlatformTime::Seconds();
  A.NextPick=Now+FMath::Max(.1f,T->ArtilleryPickInterval);++ArtilleryPicks;
  // Known enemies: the unit's own visible contacts, then its team's knowledge (one entry per key).
  TMap<int32,FVector> Known;TMap<int32,uint8> KnownTypes;
  if(!BattleTactics::ArtilleryClustersEnabled())
  if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared)
   {Known.Add(C.Key,C.Value.LastKnownPosition);if(BattleTactics::ObservedArmour())KnownTypes.Add(C.Key,C.Value.ObservedSquadType);}
  const auto& V=M->AreaIntelView(U->Team);
  for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(!Known.Contains(V.EnemyKeys[I]))
  {Known.Add(V.EnemyKeys[I],V.Enemies[I]);if(BattleTactics::ObservedArmour())KnownTypes.Add(V.EnemyKeys[I],V.ObservedType(V.EnemyKeys[I]));}
  bool bHeld=false;
  A.bHasTarget=PickArtilleryAim(ID,Known,A.Aim,A.bBase,bHeld,&A.TargetKey,BattleTactics::ArtilleryClustersEnabled(),Now,BattleTactics::ObservedArmour()?&KnownTypes:nullptr);
  if(bHeld)++ArtilleryFriendlyHolds;
  ArtilleryMs+=(FPlatformTime::Seconds()-PickStart)*1000.;
 }
 if(!A.bHasTarget)return false;
 if(BattleTactics::WeaponOrderGateEnabled()&&!WeaponOrderAllows(ID,A.TargetKey,A.Aim)){A.bHasTarget=false;A.NextPick=Now;return false;}
 Direction=(A.Aim-Position(ID)).GetSafeNormal2D();
 if(Direction.IsNearlyZero())return false;
 const float Setup=FMath::Max(0.f,bSPG?T->SPGSetupSeconds:T->MortarSetupSeconds);
 if(Now-A.StillSince+A.SetupCredit>=Setup)
 {
  bool bReady=false;
  if(bSPG)
  {
   // The gun fires when loaded and turned to the target (the turret turns at 60 deg/s, see AimTurret).
   const auto* Turret=Turrets.Find(ID);
   bReady=Now>=A.ReadyAt&&Turret&&Turret->IsValid()&&FMath::Abs(FMath::FindDeltaAngleDegrees((*Turret)->GetComponentRotation().Yaw,Direction.Rotation().Yaw))<=3.f;
  }
  else bReady=U->Ammo>0&&U->ReloadEndTime<=0.&&Now>=U->NextShotTime;
  if(bReady)
  {
   FireArtillery(ID,A.Aim,Now);
   if(bSPG&&CounterBatteryTarget.Contains(ID))
   {
    ++CounterBatteryFired;
    if(const int32 CBShots=BattleTactics::CounterBatteryShots();CBShots>0&&++CounterBatteryShots.FindOrAdd(ID)>=CBShots)
    {
     // The burst is spent: rest, and pick a soldier target at once.
     CounterBatteryShots.Remove(ID);CounterBatteryRestUntil.Add(ID,Now+BattleTactics::CounterBatteryRest());++CounterBatteryRests;
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_COUNTER_BATTERY_REST unit=%d team=%d shots=%d until=%.1f"),ID,U->Team,CBShots,Now+BattleTactics::CounterBatteryRest());
     CounterBatteryTarget.Remove(ID);A.NextPick=Now;
    }
   }
  }
 }
 // The mortar loads its next round from the reserve.
 if(!bSPG){const auto* After=Store->Find(ID);if(After&&After->Ammo==0&&After->Reserve>0&&After->ReloadEndTime==0)Store->StartReload(ID,Now);}
 return true;
}

bool UBattleLeaderRuntime::FireArtillery(int32 ID,const FVector& Aim,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive())return false;
 const auto* Cached=Artillery.Find(ID);
 if(BattleTactics::ArtilleryObservedEnabled())
 {
  const auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const int32 Enemy=1-U->Team;
  const bool AtHome=M&&M->BaseActors.IsValidIndex(Enemy)&&M->BaseActors[Enemy]&&FVector::DistSquared(Aim,M->BaseActors[Enemy]->GetActorLocation())<1.;
  if(((Cached&&Cached->bBase)||AtHome)&&(!M||!M->ArtilleryHomeObserved(U->Team)))return false;
 }
 if(BattleTactics::WeaponOrderGateEnabled()&&!WeaponOrderAllows(ID,Cached?Cached->TargetKey:INDEX_NONE,Aim))return false;
 const auto* T=GetDefault<UBattleTuning>();const bool bSPG=IsSPG(ID);
 if(!bSPG&&U->Weapon!=EBattleWeapon::Mortar)return false;
 if(BattleTactics::ArtilleryPosts()&&!bSPG&&!GetWorld()->GetSubsystem<UBattleMatch>()->AtArtilleryPost(U->Team,Position(ID)))return false;
 if(BattleTactics::ArtilleryClustersEnabled()&&AllyNear(U->Team,Aim,ArtilleryBlastRadius(bSPG)+ArtillerySpread(bSPG,FVector::Dist2D(Position(ID),Aim))))return false;
 auto& A=Artillery.FindOrAdd(ID);
 if(bSPG){if(Now<A.ReadyAt||!Store->UseSPGAmmo(ID))return false;A.ReadyAt=Now+FMath::Max(.5f,T->SPGReload);}
 else if(!Store->ConsumeShot(ID,Now,T->ShotInterval(EBattleWeapon::Mortar,false)))return false;
 U=Store->Find(ID);
 const FVector Start=Position(ID);const double Distance=FVector::Dist2D(Start,Aim);
 // Deterministic landing spread per unit and round (uniform over the landing circle).
 FRandomStream Random(int32(uint32(ID)*2654435761u+uint32(A.Shots++)*40503u+1931u));
 const float Spread=ArtillerySpread(bSPG,Distance);const float Angle=Random.FRandRange(0.f,2.f*PI),Radius=Spread*FMath::Sqrt(Random.FRand());
 FArtilleryShell Shell;Shell.ShooterID=ID;Shell.Team=U->Team;Shell.bSPG=bSPG;
 Shell.At=Aim+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0.f);
 const auto* SS=Store->FindSquad(U->SquadID);Shell.ShooterType=SS?SS->SquadType:0;
 Shell.Remaining=float(Distance/FMath::Max(100.f,bSPG?T->SPGShellSpeed:T->MortarShellSpeed));
 ArtilleryShells.Add(Shell);
 if(BattleTactics::ArtilleryFlash()&&U->Team>=0&&U->Team<2)
 {
  if(auto* RevealMatch=GetWorld()->GetSubsystem<UBattleMatch>())
  {
   const int32 RevealTeam=1-U->Team;
   TArray<int32> RevealSquads;
   for(const auto& RevealSquad:Store->GetSquads())
    if(RevealSquad.Value.Team==RevealTeam&&RevealSquad.Value.CommandStatus==EBattleCommandStatus::Active)RevealSquads.Add(RevealSquad.Key);
   RevealSquads.Sort();
   for(int32 RevealSquadID:RevealSquads)
   {
    const auto* RevealSquadState=Store->FindSquad(RevealSquadID);
    const int32 RevealLeader=RevealSquadState->LeaderUnitID;
    const auto* RevealUnit=Store->Find(RevealLeader);
    if(!RevealUnit||!RevealUnit->IsAlive()||RevealUnit->Team!=RevealTeam)continue;
    const double RevealDistanceSquared=FVector::DistSquared(Position(RevealLeader),Start);
    if(RevealDistanceSquared>FMath::Square(150000.))continue;
    // Sound and smoke: no LOS trace. Independent stream, uniform disc area.
    FRandomStream RevealRandom(int32(uint32(ID)*2654435761u+uint32(A.Shots)*40503u+15401u));
    auto& RevealReport=RevealMatch->FireReveals[RevealTeam].FindOrAdd(ID);
    if(BattleTactics::ObservedArmour())RevealReport.ObservedSquadType=bSPG?BattleSquadType::SPGun:BattleSquadType::Mortar;
    const bool bRevealFresh=RevealReport.bArtillery&&RevealReport.Count>0&&RevealMatch->Elapsed-RevealReport.At<=30.;
    RevealReport.bArtillery=true;RevealReport.At=RevealMatch->Elapsed;RevealReport.Memory=30.;RevealReport.ErrorRadius=3000.f;
    const float RevealAngle=RevealRandom.FRand()*2.f*PI;
    const float RevealRadius=FMath::Sqrt(RevealRandom.FRand())*RevealReport.ErrorRadius;
    RevealReport.Position=Start+FVector(FMath::Cos(RevealAngle)*RevealRadius,FMath::Sin(RevealAngle)*RevealRadius,0);
    // Repeated reports of one gun (within 30 s and 60 m of their mean) are averaged for counter-battery fire; used by nothing else.
    if(bRevealFresh&&FVector::Dist2D(RevealReport.Sum/double(RevealReport.Count),RevealReport.Position)<=6000.){RevealReport.Sum+=RevealReport.Position;++RevealReport.Count;}
    else{RevealReport.Sum=RevealReport.Position;RevealReport.Count=1;}
    RevealMatch->IntelViews[RevealTeam].At=-1;
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_FLASH team=%d shooter=%d type=%s observer=%d range=%.0f at=%s"),RevealTeam,ID,bSPG?TEXT("spg"):TEXT("mortar"),RevealLeader,FMath::Sqrt(RevealDistanceSquared),*RevealReport.Position.ToCompactString());
    break;
   }
  }
 }
 if(bSPG)++ArtilleryShotsSPG;else ++ArtilleryShotsMortar;
 if(SightExperiment.Started){++SightExperiment.Shots;if(SS&&SS->SquadType<BattleSquadType::Count&&U->Team>=0&&U->Team<2)++SightExperiment.TypeShots[U->Team][SS->SquadType];}
 if(U->Team==0||(Actor(ID)&&Actor(ID)->bRevealed))
 {
  DrawDebugLine(GetWorld(),Start+FVector(0,0,60),Start+FVector(0,0,600),U->Team?FColor::Red:FColor::Yellow,false,.3f,0,4);
  // Display only (2026-11-22): the flash of the gun or the tube, shown like the tracer.
  BattleMuzzleFlash::Show(GetWorld(),Start+FVector(0,0,bSPG?220:90),FVector(0,0,1),bSPG?5.f:2.5f,.1f);
 }
 // Sound only (2026-11-22): the launch is heard whether or not the gun is seen; a shell with a short flight whistles at once.
 BattleAudio::Cue(GetWorld(),bSPG?BattleAudio::ECue::CannonFire:BattleAudio::ECue::MortarLaunch,&Start);
 if(Shell.Remaining<=BattleAudio::IncomingLead)BattleAudio::Incoming(GetWorld(),Shell.At,Shell.Remaining);
 return true;
}

void UBattleLeaderRuntime::StepArtilleryShells(float Dt)
{
 // Land in launch order; an explosion may kill units but never launches shells.
 TArray<FArtilleryShell> Landed;
 for(int32 I=0;I<ArtilleryShells.Num();)
 {
  const float Before=ArtilleryShells[I].Remaining;
  ArtilleryShells[I].Remaining-=Dt;
  // Sound only (2026-11-22): the incoming whistle starts BattleAudio::IncomingLead seconds before the shell lands.
  if(Before>BattleAudio::IncomingLead&&ArtilleryShells[I].Remaining<=BattleAudio::IncomingLead)BattleAudio::Incoming(GetWorld(),ArtilleryShells[I].At,FMath::Max(0.f,ArtilleryShells[I].Remaining));
  if(ArtilleryShells[I].Remaining<=0.f){Landed.Add(ArtilleryShells[I]);ArtilleryShells.RemoveAt(I);}
  else ++I;
 }
 for(const auto& Shell:Landed)ArtilleryImpact(Shell);
}

float UBattleLeaderRuntime::ArtilleryBlastTransmission(const FVector& From,const FVector& To) const
{
 if(!BattleTactics::BlastOcclusionEnabled())return 1.f;
 // Body positions are capsule centres. Do not lift the blast above walls or roofs.
 const FVector Offsets[]={FVector(0,0,-30),FVector::ZeroVector,FVector(0,0,30)};
 int32 Clear=0;
 for(const FVector& Offset:Offsets)Clear+=BattleTactics::HasTerrainLine(GetWorld(),From,To+Offset);
 return float(Clear)/3.f;
}

int32 UBattleLeaderRuntime::ArtilleryImpact(const FArtilleryShell& Shell)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const auto* T=GetDefault<UBattleTuning>();
 ++ArtilleryImpacts;
 const float Radius=ArtilleryBlastRadius(Shell.bSPG),MaxDamage=Shell.bSPG?T->SPGBlastDamage:T->MortarBlastDamage;const int32 Team=Shell.Team;
 // Collect first: damage broadcasts deaths. Enemies only; vehicles only by the gun's direct hit (the impact within the body).
 TArray<TPair<int32,float>> Hits;
 for(const auto& B:Bodies)
 {
  const auto* U=Store->Find(B.Key);if(!U||!U->IsAlive()||U->Team==Team)continue;
  const FVector P=Position(B.Key);
  if(IsVehicle(B.Key))
  {
   if(Shell.bSPG&&FVector::Dist2D(P,Shell.At)<=BattleSquadType::VehicleRadius(VehicleType(B.Key))&&FMath::Abs(P.Z-Shell.At.Z)<=400.)Hits.Add(TPair<int32,float>(B.Key,MaxDamage*ArtilleryBlastTransmission(Shell.At,P)));
   // Trial -BattleArtilleryVehicleSplash (default 0): a near miss by a gun shell hurts the vehicle by a share of the blast falloff.
   else if(const float Splash=Shell.bSPG?BattleTactics::ArtilleryVehicleSplash():0.f;Splash>0.f)
   {
    const double VD=FVector::Dist(P,Shell.At);
    if(VD<Radius){const float VDamage=float(MaxDamage*(1.0-VD/Radius))*Splash*ArtilleryBlastTransmission(Shell.At,P);if(VDamage>0.f)Hits.Add(TPair<int32,float>(B.Key,VDamage));}
   }
   continue;
  }
  const double D=FVector::Dist(P,Shell.At);if(D>Radius)continue;
  const float Damage=float(MaxDamage*(1.0-D/Radius))*ArtilleryBlastTransmission(Shell.At,P);if(Damage>0.f)Hits.Add(TPair<int32,float>(B.Key,Damage));
 }
 Hits.Sort([](const TPair<int32,float>& A,const TPair<int32,float>& B){return A.Key<B.Key;});
 int32 Damaged=0;
 for(const auto& H:Hits)
 {
  const auto* Before=Store->Find(H.Key);if(!Before||!Before->IsAlive()||H.Value<=0.f)continue;
  const float BeforeHP=Before->HP;const int32 VictimTeam=Before->Team;const auto* VictimSquad=Store->FindSquad(Before->SquadID);const uint8 VictimType=VictimSquad?VictimSquad->SquadType:0;
  if(!Store->ApplyDamageFrom(H.Key,Team,H.Value,Shell.At,Shell.ShooterID,EBattleKillKind::Artillery))continue;
  ++Damaged;++ArtilleryHits;const bool Killed=!Store->Find(H.Key)->IsAlive();ArtilleryKills+=Killed;
  if(SightExperiment.Started)
  {
   auto& E=SightExperiment;const float Dealt=FMath::Min(BeforeHP,H.Value);++E.Hits;
   if(Shell.ShooterType<BattleSquadType::Count&&Team>=0&&Team<2){E.TypeDamage[Team][Shell.ShooterType]+=Dealt;E.TypeKills[Team][Shell.ShooterType]+=Killed;}
   if(VictimType<BattleSquadType::Count&&VictimTeam>=0&&VictimTeam<2){E.TypeTaken[VictimTeam][VictimType]+=Dealt;E.TypeDeaths[VictimTeam][VictimType]+=Killed;}
   if(Team>=0&&Team<2){E.TeamDamage[Team]+=Dealt;E.TeamKills[Team]+=Killed;}
  }
 }
 // The enemy home within the blast radius (its bounding box).
 const int32 Enemy=1-Team;
 if(M&&M->BaseActors.IsValidIndex(Enemy)&&M->BaseActors[Enemy])
 {
  const FBox Box=M->BaseActors[Enemy]->GetComponentsBoundingBox(true);
  if(Box.IsValid&&Box.ComputeSquaredDistanceToPoint(Shell.At)<=FMath::Square(double(Radius)))
  {
   const FVector Surface=Box.GetClosestPointTo(Shell.At);
   const float Transmission=!BattleTactics::BlastOcclusionEnabled()?1.f:
    (BattleTactics::HasTerrainLine(GetWorld(),Shell.At,Surface+(Shell.At-Surface).GetSafeNormal()*2.)?1.f:0.f);
   if(Transmission>0.f&&M->DamageBase(M->BaseActors[Enemy],Team,(Shell.bSPG?T->SPGBaseDamage:T->MortarBaseDamage)*Transmission))++ArtilleryBaseHits;
  }
 }
 // Trial -BattleDepotHealth: the enemy depot within the blast radius plus 3 m takes the falloff damage.
 if(M&&Enemy>=0&&Enemy<2&&BattleTactics::DepotHealth()>0.f&&M->DepotWorking(Enemy))
 {
  const double DD=FVector::Dist2D(M->RepairPoint(Enemy),Shell.At),Reach=double(Radius)+300.;
  if(DD<Reach)
  {
   const float DepotHit=float(MaxDamage*(1.-DD/Reach));
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DEPOT_HIT team=%d shooter=%d damage=%.0f distance=%.0f"),Enemy,Shell.ShooterID,DepotHit,DD);
   M->DamageDepot(Enemy,DepotHit,Shell.bSPG?TEXT("gun"):TEXT("mortar"));
  }
 }
 const double Now=GetWorld()->GetTimeSeconds();
 SuppressNear(Team,Shell.At+FVector(0,0,300),Shell.At,Now,BattleSuppression::Tuning().SupportScale);
 // Sound and effect only (2026-11-22): every shell landing is heard and seen (the place of an impact gives the gun away
 // no more than before). Size by blast radius: mortar 2, self-propelled gun 2.5.
 {
  const float Size=FMath::Clamp(Radius/FMath::Max(1.f,T->ATSplashRadius),1.f,3.f);
  BattleAudio::Explosion(GetWorld(),Shell.At,Size);
  BattleEffects::Explosion(GetWorld(),Shell.At,Size);
 }
 if(!BattleEffects::Active(GetWorld())&&(Team==0||(Actor(Shell.ShooterID)&&Actor(Shell.ShooterID)->bRevealed)))DrawDebugSphere(GetWorld(),Shell.At,Radius,10,Team?FColor::Red:FColor::Yellow,false,.5f,0,3);
 return Damaged;
}

int32 BattleTactics::ArtilleryTrialFlags()
{
 return (ArtilleryObservedEnabled()?1:0)|(SPGAmmoEnabled()?2:0)|(BlastOcclusionEnabled()?4:0)|(ArtilleryClustersEnabled()?8:0);
}

bool UBattleLeaderRuntime::ArtilleryClusterSmoke() const
{
 TArray<TPair<int32,FVector>> Input;
 // Ten disjoint groups, deliberately more than the old eight-entry cutoff.
 for(int32 I=0;I<10;++I){Input.Emplace(-100-I*2,FVector(I*1000,0,0));Input.Emplace(-101-I*2,FVector(I*1000+10,0,0));}
 const auto A=BuildArtilleryClusters(Input,100.,2);
 Input.Sort([](const auto& L,const auto& R){return L.Key>R.Key;});
 const auto B=BuildArtilleryClusters(Input,100.,2);
 if(A.Num()!=10||B.Num()!=10)return false;
 // The first nine ranked candidates are unsafe; an unlimited search must still find the tenth.
 auto Safe=[&](const UBattleMatch::FArtilleryCluster& C){return C.At==A.Last().At;};
 if(FirstSafeArtillery(A,true,Safe)!=9||FirstSafeArtillery(A,false,Safe)!=INDEX_NONE)return false;
 for(int32 I=0;I<A.Num();++I)if(A[I].At!=B[I].At||A[I].Anchors!=B[I].Anchors||A[I].Count!=2||A[I].Anchors.Num()!=2)return false;
 return true;
}

// Public candidate posts: no random draws or navigation work outside initialization/restore.
void UBattleMatch::GenerateArtilleryPosts()
{
 if(!BattleTactics::ArtilleryPosts())return;
 ArtilleryRaids.Reset();MortarPostChoices.Reset();
 ArtilleryRaidChases.Reset();ArtilleryRaidFlashes[0]=ArtilleryRaidFlashes[1]=FArtilleryRaidFlash();ArtilleryRaidClearedAt[0]=ArtilleryRaidClearedAt[1]=-1.;
 ArtilleryRaidGroups[0]=ArtilleryRaidGroups[1]=FArtilleryRaidGroup();
 for(int32 PostTeam=0;PostTeam<2;++PostTeam){ArtilleryPostPositions[PostTeam].Reset();ArtilleryPostEvidence[PostTeam].Init(-1.,3);}
 auto* PostNav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 const ANavigationData* PostData=nullptr;
 for(TActorIterator<ABattleVehicleNavMesh> PostNavIt(GetWorld());PostNavIt;++PostNavIt){PostData=*PostNavIt;break;}
 const auto& PostHeights=BattleFieldLayout::Of(GetWorld()).Heights;
 const FVector FieldCentre=(AreaHome(0)+AreaHome(1))*.5;
 const bool MirrorPosts=BattleTactics::SymmetricStart(GetWorld());
 const double PostFractions[3]={.3,.4,.5};
 for(int32 PostTeam=0;PostTeam<(MirrorPosts?1:2);++PostTeam)
 {
  const FVector PostHome=AreaHome(PostTeam),PostAxis=FieldCentre-PostHome;
  const double PostLength=PostAxis.Size2D();const FVector PostSide(-PostAxis.Y,PostAxis.X,0);
  for(int32 PostIndex=0;PostIndex<3;++PostIndex)
  {
   const FVector RawPost=PostHome+PostAxis*PostFractions[PostIndex]+PostSide.GetSafeNormal2D()*((PostIndex-1)*.15*PostLength);
   FVector GroundPost=RawPost,MirrorPost=FVector::ZeroVector;bool ProjectedPost=false;
   for(int32 PostStep=0;PostStep<=10;++PostStep)
   {
    GroundPost=RawPost+(PostHome-RawPost).GetSafeNormal2D()*FMath::Min(double(PostStep)*1000.,FVector::Dist2D(RawPost,PostHome));
    PostHeights.Lift(GroundPost);FNavLocation NavPost;
    if(!PostNav||!PostData||!PostNav->ProjectPointToNavigation(GroundPost,NavPost,FVector(1500,1500,400),PostData))continue;
    if(FVector::DotProduct(NavPost.Location-FieldCentre,PostHome-FieldCentre)<=0)continue;
    if(MirrorPosts)
    {
     MirrorPost=FieldCentre*2.-NavPost.Location;MirrorPost.Z=0;PostHeights.Lift(MirrorPost);FNavLocation MirrorNav;
     if(!PostNav->ProjectPointToNavigation(MirrorPost,MirrorNav,FVector(1500,1500,400),PostData)||FVector::DistSquared2D(MirrorNav.Location,MirrorPost)>1.)continue;
     MirrorPost.Z=MirrorNav.Location.Z;
    }
    GroundPost=NavPost.Location;ProjectedPost=true;break;
   }
   // If no nav exists or all attempts fail, retain the last deterministic ground candidate and report it.
   ArtilleryPostPositions[PostTeam].Add(GroundPost);
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_POST team=%d index=%d at=%s projected=%d"),PostTeam,PostIndex,*GroundPost.ToCompactString(),ProjectedPost);
   if(MirrorPosts)
   {
    if(!ProjectedPost){MirrorPost=FieldCentre*2.-GroundPost;MirrorPost.Z=0;PostHeights.Lift(MirrorPost);}
    ArtilleryPostPositions[1].Add(MirrorPost);
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_POST team=1 index=%d at=%s projected=%d"),PostIndex,*MirrorPost.ToCompactString(),ProjectedPost);
   }
  }
 }
}

bool UBattleMatch::AtArtilleryPost(int32 Team,const FVector& Position) const
{
 if(Team<0||Team>1)return false;
 for(const FVector& GunPost:ArtilleryPostPositions[Team])if(FVector::DistSquared2D(Position,GunPost)<=FMath::Square(1500.))return true;
 return false;
}

FVector UBattleMatch::ChooseMortarPost(int32 Team,int32 SquadID) const
{
 const auto* PostRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* PostSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SquadID);
 const auto& OwnPosts=ArtilleryPostPositions[Team];if(!PostSquad||OwnPosts.IsEmpty())return AreaHome(Team);
 const FVector MortarPosition=PostRuntime->Position(PostSquad->LeaderUnitID);
 const double MortarMax=PostRuntime->ArtilleryMaxRange(false),MortarMin=PostRuntime->ArtilleryMinRange(false);
 const auto& PostIntel=AreaIntelView(Team);TArray<double> PostScores;
 int32 BestPost=0;
 for(int32 CandidatePost=0;CandidatePost<OwnPosts.Num();++CandidatePost)
 {
  double PostScore=-FVector::Dist2D(MortarPosition,OwnPosts[CandidatePost])/100.;
  for(const FVector& KnownEnemy:PostIntel.Enemies)
  {
   const double PostDistance=FVector::Dist2D(KnownEnemy,OwnPosts[CandidatePost]);
   if(PostDistance>MortarMin&&PostDistance<=MortarMax)PostScore+=1000.;
   if(PostDistance<=.6*MortarMax)PostScore-=1500.;
  }
  PostScores.Add(PostScore);if(PostScore>PostScores[BestPost])BestPost=CandidatePost;
 }
 if(const int32* PreviousPost=MortarPostChoices.Find(SquadID);PreviousPost&&PostScores.IsValidIndex(*PreviousPost)&&PostScores[*PreviousPost]>=PostScores[BestPost]-500.)BestPost=*PreviousPost;
 MortarPostChoices.Add(SquadID,BestPost);return OwnPosts[BestPost];
}

void UBattleMatch::UpdateArtilleryRaids(int32 Team)
{
 if(!bArea||!BattleTactics::ArtilleryPosts())return;
 const auto* RaidStore=GetWorld()->GetSubsystem<UBattleState>();const auto* RaidRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto& EnemyPosts=ArtilleryPostPositions[1-Team];auto& EvidenceTimes=ArtilleryPostEvidence[Team];
 const auto& RaidIntel=AreaIntelView(Team);
 const bool ChaseRaid=BattleTactics::ArtilleryRaidChase();
 if(ChaseRaid)
 {
  auto& LatestRaidFlash=ArtilleryRaidFlashes[Team];
  for(const auto& NoticedFlash:FireReveals[Team])
   if(NoticedFlash.Value.bArtillery&&NoticedFlash.Value.At<=Elapsed&&Elapsed-NoticedFlash.Value.At<NoticedFlash.Value.Memory&&
      (NoticedFlash.Value.At>LatestRaidFlash.At||(NoticedFlash.Value.At==LatestRaidFlash.At&&NoticedFlash.Key<LatestRaidFlash.Shooter)))
    {LatestRaidFlash.Target=NoticedFlash.Value.Position;LatestRaidFlash.At=NoticedFlash.Value.At;LatestRaidFlash.Shooter=NoticedFlash.Key;}
 }
 for(int32 EvidencePost=0;EvidencePost<EnemyPosts.Num();++EvidencePost)
 {
  double EvidenceAt=EvidenceTimes[EvidencePost];const TCHAR* EvidenceKind=TEXT("seen");
  if(BattleTactics::ArtilleryFlash())
  {
   for(const auto& RaidFlash:FireReveals[Team])
    if(RaidFlash.Value.bArtillery&&RaidFlash.Value.At<=Elapsed&&Elapsed-RaidFlash.Value.At<RaidFlash.Value.Memory&&
       RaidFlash.Value.At>EvidenceAt&&FVector::DistSquared2D(RaidFlash.Value.Position,EnemyPosts[EvidencePost])<=FMath::Square(3000.))
     {EvidenceAt=RaidFlash.Value.At;EvidenceKind=TEXT("flash");}
  }
  else for(const auto& DirectContact:RaidIntel.DirectEnemies)
  {
   const uint8* ObservedType=RaidIntel.EnemyTypes.Find(DirectContact.Key);
   const double EvidenceRadius=ObservedType&&(*ObservedType==BattleSquadType::Mortar||*ObservedType==BattleSquadType::SPGun)?3000.:2000.;
   if(FVector::DistSquared2D(DirectContact.Value,EnemyPosts[EvidencePost])<=FMath::Square(EvidenceRadius))EvidenceAt=FMath::Max(EvidenceAt,RaidIntel.At);
  }
  if(EvidenceAt>EvidenceTimes[EvidencePost])
  {
   EvidenceTimes[EvidencePost]=EvidenceAt;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_SUSPECT team=%d post=%d evidence=%s at=%.1f"),Team,EvidencePost,EvidenceKind,EvidenceAt);
  }
 }
 const double RaidLifetime=ChaseRaid?120.:60.;
 int32 ChasePost=INDEX_NONE;
 if(ChaseRaid)for(int32 ChasePostIndex=0;ChasePostIndex<EvidenceTimes.Num();++ChasePostIndex)
  if(EvidenceTimes[ChasePostIndex]>=0&&Elapsed-EvidenceTimes[ChasePostIndex]<RaidLifetime&&
     (ChasePost==INDEX_NONE||EvidenceTimes[ChasePostIndex]>EvidenceTimes[ChasePost]))ChasePost=ChasePostIndex;
 const auto& RememberedFlash=ArtilleryRaidFlashes[Team];
 const bool HasRaidFlash=ChaseRaid&&RememberedFlash.At>=0&&Elapsed-RememberedFlash.At<RaidLifetime;
 bool HasRaid=false;
 TArray<int32> RaidKeys;ArtilleryRaids.GetKeys(RaidKeys);RaidKeys.Sort();
 for(int32 RaidSquadID:RaidKeys)
 {
  const auto* RaidSquad=RaidStore->FindSquad(RaidSquadID);if(RaidSquad&&RaidSquad->Team!=Team)continue;
  const int32 RaidPostIndex=ArtilleryRaids[RaidSquadID];const TCHAR* RaidEnd=nullptr;
  FArtilleryRaidChase* RaidChase=nullptr;
  if(ChaseRaid)
  {
   RaidChase=&ArtilleryRaidChases.FindOrAdd(RaidSquadID);
   if(HasRaidFlash&&RememberedFlash.At>RaidChase->FlashAt)
   {
    // A flash report carries up to 30 m of error, so a gun firing from one place moved the target (and re-issued the order, which
    // rebuilds the route) every few seconds; keep the target unless the new flash is over 50 m away or the raider is already there.
    bool RaidAtTarget=false;
    if(RaidSquad)if(const auto* TargetLeader=RaidStore->Find(RaidSquad->LeaderUnitID);TargetLeader&&TargetLeader->IsAlive())
     RaidAtTarget=FVector::DistSquared2D(RaidRuntime->Position(RaidSquad->LeaderUnitID),RaidChase->Target)<=FMath::Square(2500.);
    const bool RaidMoveTarget=!RaidChase->bFlash||RaidAtTarget||FVector::DistSquared2D(RememberedFlash.Target,RaidChase->Target)>FMath::Square(5000.);
    RaidChase->FlashAt=RememberedFlash.At;RaidChase->ArrivedAt=-1.;
    if(RaidMoveTarget)
    {
     RaidChase->Target=RememberedFlash.Target;RaidChase->bFlash=true;
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID_RETARGET team=%d squad=%d target=%s"),Team,RaidSquadID,*RaidChase->Target.ToCompactString());
    }
   }
   else if(!HasRaidFlash&&ChasePost!=INDEX_NONE)
   {
    if(RaidChase->bFlash||RaidChase->Target!=EnemyPosts[ChasePost])RaidChase->ArrivedAt=-1.;
    RaidChase->Target=EnemyPosts[ChasePost];RaidChase->bFlash=false;ArtilleryRaids[RaidSquadID]=ChasePost;
   }
   if(HasRaidFlash)RaidChase->EvidenceAt=FMath::Max(RaidChase->EvidenceAt,RememberedFlash.At);
   const int32 ChaseEvidencePost=ArtilleryRaids[RaidSquadID];
   if(EvidenceTimes.IsValidIndex(ChaseEvidencePost))RaidChase->EvidenceAt=FMath::Max(RaidChase->EvidenceAt,EvidenceTimes[ChaseEvidencePost]);
  }
  int32 RaidLiving=0;if(RaidSquad)for(int32 RaidMember:RaidSquad->Members)if(const auto* RaidUnit=RaidStore->Find(RaidMember);RaidUnit&&RaidUnit->IsAlive())++RaidLiving;
  if(!RaidSquad||RaidSquad->CommandStatus!=EBattleCommandStatus::Active)RaidEnd=TEXT("inactive");
  else if(RaidLiving*2<RaidSquad->Members.Num())RaidEnd=TEXT("losses");
  else if(Team==0&&!RaidSquad->bPlayerDelegated)RaidEnd=TEXT("manual");
  else if(ChaseRaid?(RaidChase->EvidenceAt<0||Elapsed-RaidChase->EvidenceAt>=RaidLifetime):(!EvidenceTimes.IsValidIndex(RaidPostIndex)||EvidenceTimes[RaidPostIndex]<0||Elapsed-EvidenceTimes[RaidPostIndex]>=60.))RaidEnd=TEXT("expired");
  else if(Area.HirePurpose.Contains(RaidSquadID)||IntelPurpose.Contains(RaidSquadID))RaidEnd=TEXT("purpose");
  if(!RaidEnd&&ChaseRaid)
  {
   const auto* RaidLeader=RaidStore->Find(RaidSquad->LeaderUnitID);
   if(RaidLeader&&RaidLeader->IsAlive()&&FVector::DistSquared2D(RaidRuntime->Position(RaidSquad->LeaderUnitID),RaidChase->Target)<=FMath::Square(2500.))
   {
    if(RaidChase->ArrivedAt<0)RaidChase->ArrivedAt=Elapsed;
    if(Elapsed-RaidChase->ArrivedAt>=20.){RaidEnd=TEXT("cleared");ArtilleryRaidClearedAt[Team]=RaidChase->EvidenceAt;}
   }
   else RaidChase->ArrivedAt=-1.;
  }
  if(RaidEnd){UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID_END team=%d squad=%d reason=%s"),Team,RaidSquadID,RaidEnd);ArtilleryRaids.Remove(RaidSquadID);if(ChaseRaid)ArtilleryRaidChases.Remove(RaidSquadID);}
  else HasRaid=true;
 }
 const int32 RaidGroupSize=BattleTactics::ArtilleryRaidGroup();
 if(RaidGroupSize>0)UpdateArtilleryRaidGroup(Team);
 if(HasRaid)return;
 int32 SuspectedPost=INDEX_NONE;
 for(int32 SuspectIndex=0;SuspectIndex<EvidenceTimes.Num();++SuspectIndex)
  if(EvidenceTimes[SuspectIndex]>=0&&Elapsed-EvidenceTimes[SuspectIndex]<RaidLifetime&&(SuspectedPost==INDEX_NONE||EvidenceTimes[SuspectIndex]>EvidenceTimes[SuspectedPost]))SuspectedPost=SuspectIndex;
 if(SuspectedPost==INDEX_NONE&&!HasRaidFlash)return;
 const FVector RaidTarget=HasRaidFlash?RememberedFlash.Target:EnemyPosts[SuspectedPost];
 const double RaidTargetEvidence=FMath::Max(HasRaidFlash?RememberedFlash.At:-1.,SuspectedPost!=INDEX_NONE?EvidenceTimes[SuspectedPost]:-1.);
 if(ChaseRaid&&RaidTargetEvidence<=ArtilleryRaidClearedAt[Team])return;
 // User decision 2026-11-21: a raid on artillery is dangerous; send exoskeleton soldiers first (fast, shielded, launcher kills
 // guns), armoured cars only when no exo is free; never rifle squads or heavy infantry.
 int32 NearestRaider=INDEX_NONE,NearestRaidRank=MAX_int32;double NearestRaidDistance=DBL_MAX;int32 RaidSlot=0;
 struct FRaidPick { int32 Rank; double Distance; int32 SquadID; };TArray<FRaidPick> RaidPicks;
 for(const auto& RaidPair:RaidStore->GetSquads())if(RaidPair.Value.Team==Team)
 {
  ++RaidSlot;const auto& CandidateSquad=RaidPair.Value;const uint8 RaidType=CandidateSquad.SquadType;
  if(CandidateSquad.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!CandidateSquad.bPlayerDelegated)||
     (RaidType!=BattleSquadType::Exo&&RaidType!=BattleSquadType::ArmoredCar)||
     Area.HirePurpose.Contains(RaidPair.Key)||IntelPurpose.Contains(RaidPair.Key))continue;
  if(const FPlayerRoute* RaidRoute=PlayerRoutes.Find(RaidPair.Key);RaidRoute&&RaidRoute->bMission)continue;
  if(const auto* RaidGroup=Groups.Find(GroupOf(RaidPair.Key));RaidGroup&&GroupAIRun(*RaidGroup))continue;
  bool RaidBase=false;if(BattleTactics::AreaStrategy().bEnabled)AreaStrategyTarget(RaidPair.Key,Team,RaidBase);else RaidBase=RaidSlot%5==0;
  if(RaidBase)continue;
  int32 CandidateLiving=0;for(int32 CandidateMember:CandidateSquad.Members)if(const auto* CandidateUnit=RaidStore->Find(CandidateMember);CandidateUnit&&CandidateUnit->IsAlive())++CandidateLiving;
  if(CandidateLiving==0||CandidateLiving*2<CandidateSquad.Members.Num())continue;
  const double RaidDistance=FVector::DistSquared2D(RaidRuntime->Position(CandidateSquad.LeaderUnitID),RaidTarget);
  const int32 RaidRank=RaidType==BattleSquadType::Exo?0:1;
  if(RaidGroupSize>0)RaidPicks.Add({RaidRank,RaidDistance,RaidPair.Key});
  if(RaidRank<NearestRaidRank||(RaidRank==NearestRaidRank&&(RaidDistance<NearestRaidDistance||(RaidDistance==NearestRaidDistance&&(NearestRaider==INDEX_NONE||RaidPair.Key<NearestRaider)))))
   {NearestRaider=RaidPair.Key;NearestRaidRank=RaidRank;NearestRaidDistance=RaidDistance;}
 }
 // Measured: exos are bought one at a time and each went alone at once (all squads=1, all lost); the group waits for two candidates.
 if(RaidGroupSize>0&&RaidPicks.Num()<2)return;
 if(RaidGroupSize>0)
 {
  // Trial -BattleArtilleryRaidGroup: single raiders died on the way (map I); the raid takes the best few candidates in the same order
  // (exo first, nearest, lowest ID), gathers them at their centre, then goes round the flank together (UpdateArtilleryRaidGroup).
  RaidPicks.Sort([](const FRaidPick& A,const FRaidPick& B){return A.Rank!=B.Rank?A.Rank<B.Rank:A.Distance!=B.Distance?A.Distance<B.Distance:A.SquadID<B.SquadID;});
  auto& NewRaidGroup=ArtilleryRaidGroups[Team];NewRaidGroup=FArtilleryRaidGroup();
  const int32 PickCount=FMath::Min(RaidGroupSize,RaidPicks.Num());
  for(int32 PickIndex=0;PickIndex<PickCount;++PickIndex)
  {
   const int32 PickedRaider=RaidPicks[PickIndex].SquadID;
   ArtilleryRaids.Add(PickedRaider,SuspectedPost);
   auto& PickedChase=ArtilleryRaidChases.FindOrAdd(PickedRaider);PickedChase=FArtilleryRaidChase();
   PickedChase.Target=RaidTarget;PickedChase.EvidenceAt=RaidTargetEvidence;PickedChase.bFlash=HasRaidFlash;PickedChase.FlashAt=HasRaidFlash?RememberedFlash.At:-1.;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID team=%d squad=%d post=%d target=%s source=%s"),Team,PickedRaider,SuspectedPost,*RaidTarget.ToCompactString(),HasRaidFlash?TEXT("flash"):TEXT("post"));
   if(const auto* PickedSquad=RaidStore->FindSquad(PickedRaider))NewRaidGroup.Rally+=RaidRuntime->Position(PickedSquad->LeaderUnitID);
  }
  NewRaidGroup.Rally/=double(PickCount);NewRaidGroup.Rally.Z=0;NewRaidGroup.PhaseAt=Elapsed;NewRaidGroup.Phase=PickCount>=2?0:2;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID_GROUP team=%d phase=%s squads=%d rally=%s"),Team,PickCount>=2?TEXT("gather"):TEXT("strike"),PickCount,*NewRaidGroup.Rally.ToCompactString());
  return;
 }
 if(NearestRaider!=INDEX_NONE)
 {
  ArtilleryRaids.Add(NearestRaider,SuspectedPost);
  if(ChaseRaid)
  {
   auto& NewRaidChase=ArtilleryRaidChases.FindOrAdd(NearestRaider);NewRaidChase=FArtilleryRaidChase();
   NewRaidChase.Target=RaidTarget;NewRaidChase.EvidenceAt=RaidTargetEvidence;NewRaidChase.bFlash=HasRaidFlash;NewRaidChase.FlashAt=HasRaidFlash?RememberedFlash.At:-1.;
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID team=%d squad=%d post=%d target=%s source=%s"),Team,NearestRaider,SuspectedPost,*RaidTarget.ToCompactString(),HasRaidFlash?TEXT("flash"):TEXT("post"));
  }
  else UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID team=%d squad=%d post=%d"),Team,NearestRaider,SuspectedPost);
 }
}

void UBattleMatch::UpdateArtilleryRaidGroup(int32 Team)
{
 auto& RaidGroup=ArtilleryRaidGroups[Team];if(RaidGroup.Phase<0)return;
 const auto* GroupStore=GetWorld()->GetSubsystem<UBattleState>();const auto* GroupRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 TArray<FVector> GroupPositions;int32 LeadRaider=INDEX_NONE;
 TArray<int32> GroupKeys;ArtilleryRaids.GetKeys(GroupKeys);GroupKeys.Sort();
 for(int32 GroupSquadID:GroupKeys)if(const auto* GroupSquad=GroupStore->FindSquad(GroupSquadID);GroupSquad&&GroupSquad->Team==Team)
  if(const auto* GroupLeader=GroupStore->Find(GroupSquad->LeaderUnitID);GroupLeader&&GroupLeader->IsAlive())
  {GroupPositions.Add(GroupRuntime->Position(GroupSquad->LeaderUnitID));if(LeadRaider==INDEX_NONE)LeadRaider=GroupSquadID;}
 const FArtilleryRaidChase* LeadChase=LeadRaider!=INDEX_NONE?ArtilleryRaidChases.Find(LeadRaider):nullptr;
 if(!LeadChase)
 {
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID_GROUP team=%d phase=end"),Team);
  RaidGroup=FArtilleryRaidGroup();return;
 }
 FVector GroupCentre=FVector::ZeroVector;for(const FVector& GroupPosition:GroupPositions)GroupCentre+=GroupPosition;GroupCentre/=double(GroupPositions.Num());GroupCentre.Z=0;
 auto AllWithin=[&GroupPositions](const FVector& Point,double Radius){for(const FVector& Member:GroupPositions)if(FVector::DistSquared2D(Member,Point)>FMath::Square(Radius))return false;return true;};
 if(RaidGroup.Phase==0&&(AllWithin(RaidGroup.Rally,4000.)||Elapsed-RaidGroup.PhaseAt>=30.))
 {
  PlanArtilleryRaidFlank(Team,GroupCentre,LeadChase->Target);RaidGroup.Phase=1;RaidGroup.PhaseAt=Elapsed;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID_GROUP team=%d phase=flank squads=%d way=%s screen=%s screens=%d"),Team,GroupPositions.Num(),
   *RaidGroup.Way.ToCompactString(),*RaidGroup.Screen.ToCompactString(),RaidGroup.Screens.Num());
 }
 else if(RaidGroup.Phase==1&&(AllWithin(RaidGroup.Way,5000.)||Elapsed-RaidGroup.PhaseAt>=90.||
         FVector::DistSquared2D(GroupCentre,LeadChase->Target)<=FVector::DistSquared2D(RaidGroup.Way,LeadChase->Target)))
 {
  RaidGroup.Phase=2;RaidGroup.PhaseAt=Elapsed;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARTILLERY_RAID_GROUP team=%d phase=strike squads=%d"),Team,GroupPositions.Num());
 }
 // The screen holds the front out on the raid's side while the group goes round and for a minute of the strike, then rejoins the team.
 if(RaidGroup.Phase==2&&Elapsed-RaidGroup.PhaseAt>=60.)RaidGroup.Screens.Reset();
 RaidGroup.Screens.RemoveAll([this,GroupStore,Team](int32 ScreenID){const auto* ScreenSquad=GroupStore->FindSquad(ScreenID);
  return !ScreenSquad||ScreenSquad->CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!ScreenSquad->bPlayerDelegated)||Area.HirePurpose.Contains(ScreenID)||IntelPurpose.Contains(ScreenID);});
}

void UBattleMatch::PlanArtilleryRaidFlank(int32 Team,const FVector& From,const FVector& Target)
{
 auto& FlankGroup=ArtilleryRaidGroups[Team];FlankGroup.Screens.Reset();
 FVector FlankAxis=Target-From;FlankAxis.Z=0;const double FlankLength=FlankAxis.Size2D();
 if(FlankLength<1.){FlankGroup.Way=Target;FlankGroup.Screen=From;return;}
 const FVector FlankDir=FlankAxis/FlankLength,FlankSide(-FlankDir.Y,FlankDir.X,0.);
 // The side with fewer known enemies (own sight, scout reports) between the group and its target (observed information only; ties go left of the line).
 int32 SideCrowd[2]={0,0};
 for(const FVector& SeenEnemy:AreaIntelView(Team).Enemies)
 {
  const FVector SeenOffset=SeenEnemy-From;const double SeenAlong=FVector::DotProduct(SeenOffset,FlankDir);
  if(SeenAlong>=0.&&SeenAlong<=FlankLength)++SideCrowd[FVector::DotProduct(SeenOffset,FlankSide)>=0.?0:1];
 }
 const double FlankSign=SideCrowd[1]<SideCrowd[0]?-1.:1.,FlankOffset=FMath::Clamp(.4*FlankLength,8000.,20000.);
 const FVector2D FlankHalf=BattleFieldLayout::FieldHalfExtent(GetWorld());
 auto InsideField=[&FlankHalf](FVector Point){if(FlankHalf.X>6000.&&FlankHalf.Y>6000.){Point.X=FMath::Clamp(Point.X,-FlankHalf.X+3000.,FlankHalf.X-3000.);Point.Y=FMath::Clamp(Point.Y,-FlankHalf.Y+3000.,FlankHalf.Y-3000.);}return Point;};
 FlankGroup.Way=InsideField(From+FlankDir*(.55*FlankLength)+FlankSide*(FlankSign*FlankOffset));
 FlankGroup.Screen=InsideField(From+FlankDir*(.4*FlankLength)+FlankSide*(FlankSign*.6*FlankOffset));
 const int32 ScreenCount=BattleTactics::ArtilleryRaidScreen();if(ScreenCount<=0)return;
 // The infantry squads nearest the screen point push the front out there (rifle and heavy squads the team AI runs, no mission of their own).
 const auto* ScreenStore=GetWorld()->GetSubsystem<UBattleState>();const auto* ScreenRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 struct FScreenPick { double Distance; int32 SquadID; };TArray<FScreenPick> ScreenPicks;
 for(const auto& ScreenPair:ScreenStore->GetSquads())if(ScreenPair.Value.Team==Team)
 {
  const auto& ScreenSquad=ScreenPair.Value;
  if(ScreenSquad.CommandStatus!=EBattleCommandStatus::Active||(Team==0&&!ScreenSquad.bPlayerDelegated)||
     (ScreenSquad.SquadType!=BattleSquadType::Rifle&&ScreenSquad.SquadType!=BattleSquadType::Heavy)||
     ArtilleryRaids.Contains(ScreenPair.Key)||Area.HirePurpose.Contains(ScreenPair.Key)||IntelPurpose.Contains(ScreenPair.Key))continue;
  if(const FPlayerRoute* ScreenRoute=PlayerRoutes.Find(ScreenPair.Key);ScreenRoute&&ScreenRoute->bMission)continue;
  if(const auto* ScreenGroup=Groups.Find(GroupOf(ScreenPair.Key));ScreenGroup&&GroupAIRun(*ScreenGroup))continue;
  int32 ScreenLiving=0;for(int32 ScreenMember:ScreenSquad.Members)if(const auto* ScreenUnit=ScreenStore->Find(ScreenMember);ScreenUnit&&ScreenUnit->IsAlive())++ScreenLiving;
  if(ScreenLiving==0||ScreenLiving*2<ScreenSquad.Members.Num()||!ScreenStore->Find(ScreenSquad.LeaderUnitID))continue;
  ScreenPicks.Add({FVector::DistSquared2D(ScreenRuntime->Position(ScreenSquad.LeaderUnitID),FlankGroup.Screen),ScreenPair.Key});
 }
 ScreenPicks.Sort([](const FScreenPick& A,const FScreenPick& B){return A.Distance!=B.Distance?A.Distance<B.Distance:A.SquadID<B.SquadID;});
 for(int32 ScreenIndex=0;ScreenIndex<FMath::Min(ScreenCount,ScreenPicks.Num());++ScreenIndex)FlankGroup.Screens.Add(ScreenPicks[ScreenIndex].SquadID);
}

bool UBattleMatch::ArtilleryRaidGoal(int32 Team,int32 SquadID,FVector& Center,EBattleOrder& Order) const
{
 if(Team<0||Team>1)return false;
 const auto& GoalGroup=ArtilleryRaidGroups[Team];if(GoalGroup.Phase<0)return false;
 if(ArtilleryRaids.Contains(SquadID))
 {
  // Gathering and going round, the raid squads move (they do not stop at seen enemies); striking, the normal raid order applies.
  if(GoalGroup.Phase==0){Center=GoalGroup.Rally;Order=EBattleOrder::Move;return true;}
  if(GoalGroup.Phase==1){Center=GoalGroup.Way;Order=EBattleOrder::Move;return true;}
  return false;
 }
 if(GoalGroup.Phase>=1&&GoalGroup.Screens.Contains(SquadID)){Center=GoalGroup.Screen;Order=EBattleOrder::AttackMove;return true;}
 return false;
}

bool UBattleMatch::RaidSneaking(int32 SquadID) const
{
 const float SneakRadius=BattleTactics::ArtilleryRaidSneak();
 if(SneakRadius<=0.f||!bArea)return false;
 const int32* SneakPost=ArtilleryRaids.Find(SquadID);if(!SneakPost)return false;
 const auto* SneakStore=GetWorld()->GetSubsystem<UBattleState>();const auto* SneakSquad=SneakStore->FindSquad(SquadID);
 if(!SneakSquad||SneakSquad->Team<0||SneakSquad->Team>1||!SneakStore->Find(SneakSquad->LeaderUnitID))return false;
 FVector SneakTarget=FVector::ZeroVector;
 if(BattleTactics::ArtilleryRaidChase()){const FArtilleryRaidChase* SneakChase=ArtilleryRaidChases.Find(SquadID);if(!SneakChase)return false;SneakTarget=SneakChase->Target;}
 else{const auto& SneakPosts=ArtilleryPostPositions[1-SneakSquad->Team];if(!SneakPosts.IsValidIndex(*SneakPost))return false;SneakTarget=SneakPosts[*SneakPost];}
 const auto* SneakRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 return SneakRuntime&&FVector::Dist2D(SneakRuntime->Position(SneakSquad->LeaderUnitID),SneakTarget)>SneakRadius;
}
