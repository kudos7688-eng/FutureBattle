// Tank main gun, stage 2 (ROADMAP 2026-11-14 heavy equipment; user decisions 2026-11-17: map E uses the revised table, 600 m for the AI
// and 1000 m flight; maps A-D 300 m; an enemy home takes 30 per round). The machine gun keeps the shared infantry weapon path (Fire);
// the main gun is a separate, slow weapon on the turret.
// AI cognition: the gun aims at what the tank sees itself, or, beyond its own sight, at positions its team knows (soldiers' sightings and
// scout/lookout reports, UBattleMatch::AreaIntelView). True positions of unseen enemies are never used.
#include "BattleLeaderRuntime.h"
#include "BattleMuzzleFlash.h"
#include "BattleState.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "BattleController.h"
#include "BattleSuppression.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"

float UBattleLeaderRuntime::TankGunEngage() const
{
 const auto* T=GetDefault<UBattleTuning>();
 return FMath::Max(100.f,BattleRanges::Get().bActive?T->TankGunRangeProfile:T->TankGunRange);
}

float UBattleLeaderRuntime::TankGunReach() const
{
 const auto* T=GetDefault<UBattleTuning>();
 return FMath::Max(TankGunEngage(),BattleRanges::Get().bActive?T->TankGunReachProfile:T->TankGunRange);
}

bool UBattleLeaderRuntime::TankGunTarget(int32 ID,double Now,FVector& Direction)
{
 auto* M=GetWorld()->GetSubsystem<UBattleMatch>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!M||!M->bArea||M->Phase!=EBattleMatchPhase::Combat||!U||!U->IsAlive()||!HasMainGun(ID)){TankGuns.Remove(ID);return false;}
 const auto* T=GetDefault<UBattleTuning>();
 auto& G=TankGuns.FindOrAdd(ID);
 if(G.ReadyAt>Now+FMath::Max(0.f,T->TankGunReload)+1.)G.ReadyAt=Now; // a load to an earlier time
 if(Now>=G.NextPick)
 {
  const double PickStart=FPlatformTime::Seconds();
  G.NextPick=Now+.5;G.bHasTarget=false;G.bBase=false;G.TargetKey=INDEX_NONE;++TankGunPicks;
  const FVector Pos=Position(ID);const double Engage2=FMath::Square(TankGunEngage()),Min2=FMath::Square(FMath::Max(0.f,T->TankGunMinRange));
  const double Splash2=FMath::Square(FMath::Max(1.f,T->TankGunSplashRadius));
  // Known enemies: the tank's own visible contacts, then its team's knowledge (one entry per key).
  TMap<int32,FVector> Known;
  if(const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID))
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared)Known.Add(C.Key,C.Value.LastKnownPosition);
  const auto& V=M->AreaIntelView(U->Team);
  for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(!Known.Contains(V.EnemyKeys[I]))Known.Add(V.EnemyKeys[I],V.Enemies[I]);
  struct FCandidate{int32 Key;FVector At;double Score;};TArray<FCandidate> Candidates;
  for(const auto& K:Known)
  {
   const double D2=FVector::DistSquared2D(Pos,K.Value);if(D2>Engage2||D2<Min2)continue;
   const bool Vehicle=K.Key>0&&IsVehicle(K.Key);
   int32 Around=0;if(!Vehicle)for(const auto& O:Known)Around+=FVector::DistSquared2D(O.Value,K.Value)<=Splash2;
   if(!Vehicle&&Around<FMath::Max(1,T->TankGunMinCluster))continue;
   Candidates.Add({K.Key,K.Value,(Vehicle?1000.:double(Around))-FMath::Sqrt(D2)/1.e6});
  }
  Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Score!=B.Score?A.Score>B.Score:A.Key<B.Key;});
  // Line of fire from the turret: a clear line, or the round would burst close enough to the target.
  auto Q=Sight(ID).Query;const FVector Muzzle=Pos+FVector(0,0,60);
  for(int32 I=0;I<Candidates.Num()&&I<3&&!G.bHasTarget;++I)
  {
   const auto& C=Candidates[I];const FVector Aim=C.At+FVector(0,0,C.Key>0?BattleStance::AimOffset(StanceOf(C.Key)):35.f);
   FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,Aim,ECC_Visibility,Q);
   if(!Hit.bBlockingHit||FVector::DistSquared(Hit.ImpactPoint,Aim)<=Splash2||HitIDs.FindRef(Hit.GetComponent())>0){G.bHasTarget=true;G.TargetKey=C.Key;G.Aim=Aim;}
  }
  // The enemy home: when in range and seen by the tank itself or by its team (bHomeCovered).
  const int32 Enemy=1-U->Team;
  if(!G.bHasTarget&&M->BaseActors.IsValidIndex(Enemy)&&M->BaseActors[Enemy]&&M->Area.BaseHP.IsValidIndex(Enemy)&&M->Area.BaseHP[Enemy]>0)
  {
   const FVector Home=M->BaseActors[Enemy]->GetActorLocation();const double D2=FVector::DistSquared2D(Pos,Home);
   if(D2<=Engage2&&(V.bHomeCovered||M->AreaVisible(ID,Home,M->BaseActors[Enemy])))
   {
    FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,Home,ECC_Visibility,Q);
    if(!Hit.bBlockingHit||Hit.GetActor()==M->BaseActors[Enemy]){G.bHasTarget=true;G.bBase=true;G.Aim=Home;}
   }
  }
  TankGunMs+=(FPlatformTime::Seconds()-PickStart)*1000.;
 }
 if(!G.bHasTarget)return false;
 Direction=(G.Aim-Position(ID)).GetSafeNormal2D();
 return !Direction.IsNearlyZero();
}

bool UBattleLeaderRuntime::StepTankGun(int32 ID,double Now)
{
 auto* G=TankGuns.Find(ID);if(!G||!G->bHasTarget||Now<G->ReadyAt)return false;
 const auto* Turret=Turrets.Find(ID);if(!Turret||!Turret->IsValid())return false;
 const float Want=(G->Aim-Position(ID)).Rotation().Yaw;
 if(FMath::Abs(FMath::FindDeltaAngleDegrees((*Turret)->GetComponentRotation().Yaw,Want))>3.f)return false; // the turret is still turning
 return FireTankGun(ID,G->Aim,Now);
}

bool UBattleLeaderRuntime::TankGunHit(int32 ID,int32 Victim)
{
 // Direct hit: full damage, vehicles included (the gun pierces armour). Never own units.
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);const auto* Before=Store->Find(Victim);
 if(!U||!Before||!Before->IsAlive()||Before->Team==U->Team)return false;
 const auto* T=GetDefault<UBattleTuning>();const int32 Team=U->Team;
 const float BeforeHP=Before->HP;const auto* VS=Store->FindSquad(Before->SquadID);const uint8 VictimType=VS?VS->SquadType:0;const int32 VictimTeam=Before->Team;
 const bool Vehicle=IsVehicle(Victim);
 if(!Store->ApplyDamageFrom(Victim,Team,T->TankGunDamage,Position(ID),ID,EBattleKillKind::TankGun))return false;
 ++TankGunHits;TankGunVehicleHits+=Vehicle;const bool Killed=!Store->Find(Victim)->IsAlive();TankGunKills+=Killed;
 if(SightExperiment.Started)
 {
  auto& E=SightExperiment;const float Dealt=FMath::Min(BeforeHP,T->TankGunDamage);++E.Hits;
  if(const auto* SS=Store->FindSquad(U->SquadID);SS&&SS->SquadType<BattleSquadType::Count&&Team>=0&&Team<2){E.TypeDamage[Team][SS->SquadType]+=Dealt;E.TypeKills[Team][SS->SquadType]+=Killed;}
  if(VictimType<BattleSquadType::Count&&VictimTeam>=0&&VictimTeam<2){E.TypeTaken[VictimTeam][VictimType]+=Dealt;E.TypeDeaths[VictimTeam][VictimType]+=Killed;}
  E.TeamDamage[Team]+=Dealt;E.TeamKills[Team]+=Killed;
 }
 return true;
}

bool UBattleLeaderRuntime::FireTankGun(int32 ID,const FVector& Aim,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive()||!M)return false;
 const auto* T=GetDefault<UBattleTuning>();auto& G=TankGuns.FindOrAdd(ID);
 G.ReadyAt=Now+FMath::Max(.5f,T->TankGunReload);
 const int32 Team=U->Team;const FVector Start=Position(ID)+FVector(0,0,60);
 // Deterministic spread per tank and round (like the infantry shot sequence).
 FRandomStream Random(int32(uint32(ID)*2654435761u+uint32(G.Shots++)*40503u+977u));
 const FVector Direction=Random.VRandCone((Aim-Start).GetSafeNormal(),FMath::DegreesToRadians(FMath::Max(0.f,T->TankGunSpread)));
 auto Q=Sight(ID).Query;if(ConcealmentActors.Num())Q.AddIgnoredActors(ConcealmentActors);
 FHitResult Hit;const FVector End=Start+Direction*TankGunReach();
 GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q);
 const FVector Impact=Hit.bBlockingHit?FVector(Hit.ImpactPoint):End;
 ++TankGunShots;
 if(SightExperiment.Started){++SightExperiment.Shots;if(const auto* SS=Store->FindSquad(U->SquadID);SS&&SS->SquadType<BattleSquadType::Count&&Team>=0&&Team<2)++SightExperiment.TypeShots[Team][SS->SquadType];}
 SuppressNear(Team,Start,Impact,Now,BattleSuppression::Tuning().SupportScale);
 if(M->DamageBase(Hit.GetActor(),Team,T->TankGunBaseDamage))++TankGunBaseHits;
 const int32 Victim=HitIDs.FindRef(Hit.GetComponent());
 TankGunHit(ID,Victim);
 Explode(ID,Impact,Victim,T->TankGunSplashRadius,T->TankGunSplashDamage);
 if(Team==0||(Actor(ID)&&Actor(ID)->bRevealed))
 {
  const FVector From=TracerStart(ID,Start,Direction);
  BattleMuzzleFlash::Show(GetWorld(),From,Direction,6.f,.12f);
  DrawDebugLine(GetWorld(),From,Impact,Team?FColor::Red:FColor::Yellow,false,.25f,0,6);
  DrawDebugSphere(GetWorld(),Impact,FMath::Max(50.f,T->TankGunSplashRadius),10,Team?FColor::Red:FColor::Yellow,false,.4f,0,3);
 }
 return true;
}
