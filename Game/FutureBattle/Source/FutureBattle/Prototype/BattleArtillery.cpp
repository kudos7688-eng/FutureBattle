// Indirect fire, stage 1 (ROADMAP 2026-11-14 heavy equipment order: ... armoured car -> indirect fire; first values 2026-11-18, numbers
// delegated): the mortar team (squad type 6; slot 1 carries EBattleWeapon::Mortar) and the self-propelled gun (squad type 7, a vehicle).
// Both set up while standing still (moving undoes it), then lob shells at positions their team knows: what the unit sees itself plus its
// team's knowledge (soldiers' sightings, scout and lookout reports; UBattleMatch::AreaIntelView), like the tank main gun. True positions
// of unseen enemies are never used; own units are true positions (a team knows where its own soldiers are). Shells are scheduled and land
// after their flight time, so a target may have moved away. The old support strike (BattleSupport.cpp, the old all-Actor FastMatch) is
// not reused: it works on Actors and a team-wide credit budget; only its idea (a delayed impact at an observed position) is kept.
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleSuppression.h"
#include "BattleUnit.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"

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
 const double Keep=FMath::Clamp(GetDefault<UBattleTuning>()->ArtilleryStandoff,0.f,1.f)*R->ArtilleryMaxRange(S->SquadType==BattleSquadType::SPGun);
 const auto& V=AreaIntelView(Team);
 if(Keep<=0.||V.Enemies.IsEmpty())return Goal;
 auto Clear=[&](const FVector& P){for(const FVector& E:V.Enemies)if(FVector::DistSquared2D(P,E)<Keep*Keep)return false;return true;};
 if(Clear(Goal))return Goal;
 const FVector Home=AreaHome(Team);const double Length=FVector::Dist2D(Goal,Home);const FVector Back=(Home-Goal).GetSafeNormal2D();
 FVector Post=Home;
 for(double Step=1000.;Step<Length;Step+=1000.){const FVector P=Goal+Back*Step;if(Clear(P)){Post=P;break;}}
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

bool UBattleLeaderRuntime::PickArtilleryAim(int32 ID,const TMap<int32,FVector>& Known,FVector& Aim,bool& bBase,bool& bFriendlyHeld)
{
 bBase=false;bFriendlyHeld=false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U||!U->IsAlive())return false;
 const auto* T=GetDefault<UBattleTuning>();const bool bSPG=IsSPG(ID);
 const FVector Pos=Position(ID);
 const double Min2=FMath::Square(double(ArtilleryMinRange(bSPG))),Max2=FMath::Square(double(ArtilleryMaxRange(bSPG)));
 const double Blast=ArtilleryBlastRadius(bSPG),Blast2=Blast*Blast;
 // Soldiers only: the explosion does not hurt vehicles (a gun's direct hit on one is luck, not a plan).
 TArray<TPair<int32,FVector>> Soldiers;
 for(const auto& K:Known)if(!(K.Key>0&&IsVehicle(K.Key)))Soldiers.Add(TPair<int32,FVector>(K.Key,K.Value));
 // A candidate per known soldier: the centre of the known soldiers within the blast radius of him, worth a shell when it holds
 // ArtilleryMinCluster or more and lies within range. More soldiers first, then nearer.
 struct FCandidate{int32 Key;FVector At;double Score;};
 TArray<FCandidate> Candidates;
 for(const auto& K:Soldiers)
 {
  int32 Around=0;FVector Sum=FVector::ZeroVector;
  for(const auto& O:Soldiers)if(FVector::DistSquared2D(O.Value,K.Value)<=Blast2){++Around;Sum+=O.Value;}
  if(Around<FMath::Max(1,T->ArtilleryMinCluster))continue;
  const FVector Center=Sum/double(Around);const double D2=FVector::DistSquared2D(Pos,Center);
  if(D2<Min2||D2>Max2)continue;
  Candidates.Add({K.Key,Center,double(Around)-FMath::Sqrt(D2)/1.e6});
 }
 Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Score!=B.Score?A.Score>B.Score:A.Key<B.Key;});
 // Never where the blast plus the landing spread may reach an own unit.
 for(int32 I=0;I<Candidates.Num()&&I<8;++I)
 {
  const auto& C=Candidates[I];
  if(AllyNear(U->Team,C.At,Blast+ArtillerySpread(bSPG,FVector::Dist2D(Pos,C.At)))){bFriendlyHeld=true;continue;}
  Aim=C.At;return true;
 }
 // The enemy home: in range and seen by the unit itself or by its team (bHomeCovered), like the tank gun.
 const auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const int32 Enemy=1-U->Team;
 if(M&&M->bArea&&M->BaseActors.IsValidIndex(Enemy)&&M->BaseActors[Enemy]&&M->Area.BaseHP.IsValidIndex(Enemy)&&M->Area.BaseHP[Enemy]>0)
 {
  const FVector Home=M->BaseActors[Enemy]->GetActorLocation();const double D2=FVector::DistSquared2D(Pos,Home);
  if(D2>=Min2&&D2<=Max2&&(M->AreaIntelView(U->Team).bHomeCovered||M->AreaVisible(ID,Home,M->BaseActors[Enemy])))
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
 // Times from a later clock (a restore clears this state, so these only guard against stale entries).
 if(A.ReadyAt>Now+FMath::Max(0.f,T->SPGReload)+1.)A.ReadyAt=Now;
 if(A.StillSince>Now)A.StillSince=-1;
 // Moving undoes the set-up (the target is kept until the next pick).
 if(bMoving){A.StillSince=-1;return false;}
 if(A.StillSince<0)A.StillSince=Now;
 auto* M=GetWorld()->GetSubsystem<UBattleMatch>();
 if(Now>=A.NextPick&&M&&M->bArea&&M->Phase==EBattleMatchPhase::Combat)
 {
  const double PickStart=FPlatformTime::Seconds();
  A.NextPick=Now+FMath::Max(.1f,T->ArtilleryPickInterval);++ArtilleryPicks;
  // Known enemies: the unit's own visible contacts, then its team's knowledge (one entry per key).
  TMap<int32,FVector> Known;
  if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared)Known.Add(C.Key,C.Value.LastKnownPosition);
  const auto& V=M->AreaIntelView(U->Team);
  for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(!Known.Contains(V.EnemyKeys[I]))Known.Add(V.EnemyKeys[I],V.Enemies[I]);
  bool bHeld=false;
  A.bHasTarget=PickArtilleryAim(ID,Known,A.Aim,A.bBase,bHeld);
  if(bHeld)++ArtilleryFriendlyHolds;
  ArtilleryMs+=(FPlatformTime::Seconds()-PickStart)*1000.;
 }
 if(!A.bHasTarget)return false;
 Direction=(A.Aim-Position(ID)).GetSafeNormal2D();
 if(Direction.IsNearlyZero())return false;
 const float Setup=FMath::Max(0.f,bSPG?T->SPGSetupSeconds:T->MortarSetupSeconds);
 if(Now-A.StillSince>=Setup)
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
 const auto* T=GetDefault<UBattleTuning>();const bool bSPG=IsSPG(ID);
 if(!bSPG&&U->Weapon!=EBattleWeapon::Mortar)return false;
 auto& A=Artillery.FindOrAdd(ID);
 if(bSPG){if(Now<A.ReadyAt)return false;A.ReadyAt=Now+FMath::Max(.5f,T->SPGReload);}
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
   if(Shell.bSPG&&FVector::Dist2D(P,Shell.At)<=BattleSquadType::VehicleRadius(VehicleType(B.Key))&&FMath::Abs(P.Z-Shell.At.Z)<=400.)Hits.Add(TPair<int32,float>(B.Key,MaxDamage));
   continue;
  }
  const double D=FVector::Dist(P,Shell.At);if(D>Radius)continue;
  const float Damage=float(MaxDamage*(1.0-D/Radius));if(Damage>0.f)Hits.Add(TPair<int32,float>(B.Key,Damage));
 }
 Hits.Sort([](const TPair<int32,float>& A,const TPair<int32,float>& B){return A.Key<B.Key;});
 int32 Damaged=0;
 for(const auto& H:Hits)
 {
  const auto* Before=Store->Find(H.Key);if(!Before||!Before->IsAlive())continue;
  const float BeforeHP=Before->HP;const int32 VictimTeam=Before->Team;const auto* VictimSquad=Store->FindSquad(Before->SquadID);const uint8 VictimType=VictimSquad?VictimSquad->SquadType:0;
  if(!Store->ApplyDamage(H.Key,Team,H.Value))continue;
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
  if(Box.IsValid&&Box.ComputeSquaredDistanceToPoint(Shell.At)<=FMath::Square(double(Radius))&&M->DamageBase(M->BaseActors[Enemy],Team,Shell.bSPG?T->SPGBaseDamage:T->MortarBaseDamage))++ArtilleryBaseHits;
 }
 const double Now=GetWorld()->GetTimeSeconds();
 SuppressNear(Team,Shell.At+FVector(0,0,300),Shell.At,Now,BattleSuppression::Tuning().SupportScale);
 if(Team==0||(Actor(Shell.ShooterID)&&Actor(Shell.ShooterID)->bRevealed))DrawDebugSphere(GetWorld(),Shell.At,Radius,10,Team?FColor::Red:FColor::Yellow,false,.5f,0,3);
 return Damaged;
}
