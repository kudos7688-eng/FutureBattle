// Indirect fire, stage 1 (ROADMAP 2026-11-14 heavy equipment order: ... armoured car -> indirect fire; first values 2026-11-18, numbers
// delegated): the mortar team (squad type 6; slot 1 carries EBattleWeapon::Mortar) and the self-propelled gun (squad type 7, a vehicle).
// Both set up while standing still (moving undoes it), then lob shells at positions their team knows: what the unit sees itself plus its
// team's knowledge (soldiers' sightings, scout and lookout reports; UBattleMatch::AreaIntelView), like the tank main gun. True positions
// of unseen enemies are never used; own units are true positions (a team knows where its own soldiers are). Shells are scheduled and land
// after their flight time, so a target may have moved away. The old support strike (BattleSupport.cpp, the old all-Actor FastMatch) is
// not reused: it works on Actors and a team-wide credit budget; only its idea (a delayed impact at an observed position) is kept.
#include "BattleLeaderRuntime.h"
#include "BattleTactics.h"
#include "BattleState.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleSuppression.h"
#include "BattleUnit.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool BattleTactics::ArtilleryObservedEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryObserved="),V);return V!=0;}();
 return On;
}

bool BattleTactics::ArtilleryNavFallback()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleArtilleryNavFallback="),V);return V!=0;}();
 return On;
}

bool BattleTactics::VehicleHitCover()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleVehicleHitCover="),V);return V==1;}();
 return On;
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
 // Mortar: 300 m, 400 m with the range profile (map E). Gun: 1000 m on every map.
 const auto* T=GetDefault<UBattleTuning>();
 const float Range=bSPG?T->SPGRange:(BattleRanges::Get().bActive?T->MortarRangeProfile:T->MortarRange);
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
 const float At=FMath::Max(0.f,bSPG?T->SPGSpread:T->MortarSpread),Reference=FMath::Max(100.f,bSPG?T->SPGRange:T->MortarRange);
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
 const double Keep=FMath::Clamp(GetDefault<UBattleTuning>()->ArtilleryStandoff,0.f,1.f)*Range;
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
  for(int32 I=0;I<V.Enemies.Num();++I)if(!(V.EnemyKeys[I]>0&&R->IsVehicle(V.EnemyKeys[I])))
   Soldiers.Emplace(V.EnemyKeys[I],V.Enemies[I]);
  V.Clusters[Kind]=BuildArtilleryClusters(MoveTemp(Soldiers),R->ArtilleryBlastRadius(bSPG),FMath::Max(1,GetDefault<UBattleTuning>()->ArtilleryMinCluster));
  V.bClustersBuilt[Kind]=true;
 }
 return V.Clusters[Kind];
}

bool UBattleLeaderRuntime::PickArtilleryAim(int32 ID,const TMap<int32,FVector>& Known,FVector& Aim,bool& bBase,bool& bFriendlyHeld,int32* TargetKey,bool bTeamClusters)
{
 bBase=false;bFriendlyHeld=false;if(TargetKey)*TargetKey=INDEX_NONE;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U||!U->IsAlive())return false;
 const auto* T=GetDefault<UBattleTuning>();const bool bSPG=IsSPG(ID);
 const FVector Pos=Position(ID);
 const double Min2=FMath::Square(double(ArtilleryMinRange(bSPG))),Max2=FMath::Square(double(ArtilleryMaxRange(bSPG)));
 const double Blast=ArtilleryBlastRadius(bSPG),Blast2=Blast*Blast;
 // Soldiers only: the explosion does not hurt vehicles (a gun's direct hit on one is luck, not a plan).
 TArray<TPair<int32,FVector>> Soldiers;
 if(!BattleTactics::ArtilleryClustersEnabled()||!bTeamClusters)
 for(const auto& K:Known)if(!(K.Key>0&&IsVehicle(K.Key)))Soldiers.Add(TPair<int32,FVector>(K.Key,K.Value));
 // A candidate per known soldier: the centre of the known soldiers within the blast radius of him, worth a shell when it holds
 // ArtilleryMinCluster or more and lies within range. More soldiers first, then nearer.
 struct FCandidate{int32 Key;FVector At;double Score;};
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
 Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Score!=B.Score?A.Score>B.Score:A.Key<B.Key;});
 // Never where the blast plus the landing spread may reach an own unit.
 const int32 Selected=FirstSafeArtillery(Candidates,BattleTactics::ArtilleryClustersEnabled(),[&](const FCandidate& C)
 {
  if(AllyNear(U->Team,C.At,Blast+ArtillerySpread(bSPG,FVector::Dist2D(Pos,C.At)))){bFriendlyHeld=true;return false;}
  return true;
 });
 if(Selected!=INDEX_NONE){Aim=Candidates[Selected].At;if(TargetKey)*TargetKey=Candidates[Selected].Key;return true;}
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
  TMap<int32,FVector> Known;
  if(!BattleTactics::ArtilleryClustersEnabled())
  if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared)Known.Add(C.Key,C.Value.LastKnownPosition);
  const auto& V=M->AreaIntelView(U->Team);
  for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(!Known.Contains(V.EnemyKeys[I]))Known.Add(V.EnemyKeys[I],V.Enemies[I]);
  bool bHeld=false;
  A.bHasTarget=PickArtilleryAim(ID,Known,A.Aim,A.bBase,bHeld,&A.TargetKey,BattleTactics::ArtilleryClustersEnabled());
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
  if(bReady)FireArtillery(ID,A.Aim,Now);
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
 if(bSPG)++ArtilleryShotsSPG;else ++ArtilleryShotsMortar;
 if(SightExperiment.Started){++SightExperiment.Shots;if(SS&&SS->SquadType<BattleSquadType::Count&&U->Team>=0&&U->Team<2)++SightExperiment.TypeShots[U->Team][SS->SquadType];}
 if(U->Team==0||(Actor(ID)&&Actor(ID)->bRevealed))DrawDebugLine(GetWorld(),Start+FVector(0,0,60),Start+FVector(0,0,600),U->Team?FColor::Red:FColor::Yellow,false,.3f,0,4);
 return true;
}

void UBattleLeaderRuntime::StepArtilleryShells(float Dt)
{
 // Land in launch order; an explosion may kill units but never launches shells.
 TArray<FArtilleryShell> Landed;
 for(int32 I=0;I<ArtilleryShells.Num();)
 {
  ArtilleryShells[I].Remaining-=Dt;
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
 const double Now=GetWorld()->GetTimeSeconds();
 SuppressNear(Team,Shell.At+FVector(0,0,300),Shell.At,Now,BattleSuppression::Tuning().SupportScale);
 if(Team==0||(Actor(Shell.ShooterID)&&Actor(Shell.ShooterID)->bRevealed))DrawDebugSphere(GetWorld(),Shell.At,Radius,10,Team?FColor::Red:FColor::Yellow,false,.5f,0,3);
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
