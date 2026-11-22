// Infantry arms (2026-11-22, user request: sidearms for ordinary soldiers, hand grenades, melee strikes, an AI that uses them, an
// elite platoon with roles and more weapon types; "use existing assets where they fit"). Docs/design-infantry-arms.md.
// Values and switches (namespace BattleArms) and the runtime part of UBattleLeaderRuntime: trial equipment, timers, hand grenades and
// launcher rounds in flight, strikes, the medic, sidearm use and the elite roles' own targets.
// AI cognition: throws, launcher rounds and strikes choose from the soldier's own contacts (seen now, or seen or reported within the
// last 3 s); the marksman reads the weapon of an enemy he sees right now. Only the physical results (blast, strike) read true positions.
#include "BattleArms.h"
#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "BattleSuppression.h"
#include "BattleTactics.h"
#include "BattleUnit.h"
#include "BattleMuzzleFlash.h"
#include "BattleCover.h"
#include "BattleVATDisplay.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "UObject/StrongObjectPtr.h"
#include "FutureBattle.h"

namespace
{
 int32 IntSwitch(const TCHAR* Key,int32 Default=0){int32 V=Default;FParse::Value(FCommandLine::Get(),Key,V);return V;}
 float FloatSwitch(const TCHAR* Key,float Default){float V=Default;FParse::Value(FCommandLine::Get(),Key,V);return FMath::IsFinite(V)?V:Default;}
}

bool BattleArms::SidearmsEnabled(){static const bool b=IntSwitch(TEXT("BattleSidearms="))!=0;return b;}
int32 BattleArms::GrenadeCount(){static const int32 N=FMath::Clamp(IntSwitch(TEXT("BattleGrenades=")),0,10);return N;}
bool BattleArms::MeleeEnabled(){static const bool b=IntSwitch(TEXT("BattleMelee="))!=0;return b;}
bool BattleArms::EliteEnabled(){static const bool b=IntSwitch(TEXT("BattleElite="))!=0;return b;}
int32 BattleArms::AreaEliteKeep(){static const int32 N=FMath::Clamp(IntSwitch(TEXT("BattleAreaEliteKeep=")),0,20);return N;}
int32 BattleArms::EliteCost(){static const int32 N=FMath::Clamp(IntSwitch(TEXT("BattleEliteCost="),26),1,200);return N;}
bool BattleArms::SniperEnabled(){static const bool b=IntSwitch(TEXT("BattleSniperTeam="))!=0;return b;}
int32 BattleArms::AreaSniperKeep(){static const int32 N=FMath::Clamp(IntSwitch(TEXT("BattleAreaSniperKeep=")),0,SniperLimit);return N;}
int32 BattleArms::SniperCost(){static const int32 N=FMath::Clamp(IntSwitch(TEXT("BattleSniperCost="),14),1,200);return N;}
int32 BattleArms::AssaultMode(){static const int32 N=FMath::Clamp(IntSwitch(TEXT("BattleArmsAssault=")),0,2);return N;}
bool BattleArms::SmokeEnabled(){static const bool b=FParse::Param(FCommandLine::Get(),TEXT("BattleArmsSmoke"));return b;}
bool BattleArms::AnyTrial(){return SidearmsEnabled()||GrenadeCount()>0||MeleeEnabled()||EliteEnabled()||AreaEliteKeep()>0||SniperEnabled()||AreaSniperKeep()>0||AssaultMode()>0||SmokeEnabled();}

uint8 BattleArms::EliteKit(int32 Slot)
{
 static const uint8 Table[]={KitLeader,KitGunner,KitGrenadier,KitMarksman,KitBreacher,KitAssault,KitMedic,KitRifleman};
 return Slot>=0&&Slot<int32(UE_ARRAY_COUNT(Table))?Table[Slot]:uint8(KitRifleman);
}
uint8 BattleArms::GradeKit(uint8 Grade,int32 Slot){return Grade==2?uint8(Slot==0?KitSniper:KitSpotter):Grade==1?EliteKit(Slot):uint8(KitNone);}
bool BattleArms::ValidGrade(uint8 SquadType,uint8 Grade){return Grade==0||(Grade==1&&SquadType==BattleSquadType::Rifle)||(Grade==2&&SquadType==BattleSquadType::Scout);}
bool BattleArms::GradeWeapon(uint8 Grade,EBattleWeapon W){for(int32 Slot=0;Slot<10;++Slot)if(GradeKit(Grade,Slot)!=KitNone&&KitWeapon(GradeKit(Grade,Slot))==W)return true;return false;}
EBattleWeapon BattleArms::KitWeapon(uint8 Kit)
{
 switch(Kit)
 {
  case KitGunner:return EBattleWeapon::SupportGun;
  case KitGrenadier:return EBattleWeapon::GrenadeLauncher;
  case KitMarksman:return EBattleWeapon::Marksman;
  case KitBreacher:return EBattleWeapon::Shotgun;
  case KitAssault:return EBattleWeapon::SMG;
  case KitSniper:return EBattleWeapon::Sniper;
  default:return EBattleWeapon::Carbine;
 }
}
int32 BattleArms::KitGrenades(uint8 Kit)
{
 switch(Kit)
 {
  case KitNone:case KitGrenadier:case KitSniper:return 0;
  case KitLeader:return 2;
  case KitBreacher:case KitAssault:case KitRifleman:return 3;
  default:return 1;
 }
}
bool BattleArms::ValidKit(uint8 Kit){return Kit<KitCount;}
bool BattleArms::EliteWeapon(EBattleWeapon W){for(uint8 K=1;K<KitCount;++K)if(KitWeapon(K)==W)return true;return false;}
const TCHAR* BattleArms::KitName(uint8 Kit)
{
 switch(Kit)
 {
  case KitLeader:return TEXT("隊長");case KitGunner:return TEXT("機関銃手");case KitGrenadier:return TEXT("擲弾手");
  case KitMarksman:return TEXT("選抜射手");case KitBreacher:return TEXT("突撃手（散弾）");case KitAssault:return TEXT("突撃手（短機関銃）");
  case KitMedic:return TEXT("衛生兵");case KitRifleman:return TEXT("小銃手");case KitSniper:return TEXT("狙撃手");case KitSpotter:return TEXT("観測手");default:return TEXT("");
 }
}
const TCHAR* BattleArms::WeaponName(EBattleWeapon W)
{
 switch(W)
 {
  case EBattleWeapon::SupportGun:return TEXT("支援火器");case EBattleWeapon::AntiTank:return TEXT("対戦車火器");case EBattleWeapon::Mortar:return TEXT("迫撃砲");
  case EBattleWeapon::Pistol:return TEXT("拳銃");case EBattleWeapon::SMG:return TEXT("短機関銃");case EBattleWeapon::Shotgun:return TEXT("散弾銃");
  case EBattleWeapon::Marksman:return TEXT("選抜射手銃");case EBattleWeapon::GrenadeLauncher:return TEXT("擲弾銃");case EBattleWeapon::Sniper:return TEXT("狙撃銃");default:return TEXT("小銃");
 }
}

// First values (Docs/design-infantry-arms.md). Ranges are scales of the carbine's engage/reach distance (BattleRanges) with caps.
const FBattleWeaponDef* BattleWeaponDef(EBattleWeapon W)
{
 struct FTable{FBattleWeaponDef Pistol,SMG,Shotgun,Marksman,Launcher,Sniper;};
 static const FTable T=[]{
  FTable X;
  auto& P=X.Pistol;P.Magazine=12;P.Reserve=24;P.AmmoKind=3;P.Reload=1.5f;P.AIInterval=.8f;P.PlayerInterval=.22f;P.DamageScale=P.DamageNear=.8f;
  P.SpreadScale=1.4f;P.EngageCap=2500.f;P.ReachCap=5000.f;P.SuppressionScale=.8f;
  auto& S=X.SMG;S.Magazine=30;S.Reserve=90;S.AmmoKind=3;S.Reload=2.f;S.AIInterval=.55f;S.PlayerInterval=.075f;S.DamageScale=S.DamageNear=.65f;
  S.SpreadScale=1.6f;S.EngageCap=5000.f;S.ReachCap=10000.f;
  auto& G=X.Shotgun;G.Magazine=8;G.Reserve=24;G.AmmoKind=4;G.Reload=3.f;G.AIInterval=1.1f;G.PlayerInterval=.85f;G.DamageScale=1.f;G.DamageNear=4.f;
  G.NearRange=500.f;G.FarRange=2000.f;G.SpreadScale=.8f;G.EngageCap=2500.f;G.ReachCap=5000.f;G.SuppressionScale=1.2f;
  auto& M=X.Marksman;M.Magazine=10;M.Reserve=40;M.AmmoKind=5;M.Reload=2.5f;M.AIInterval=2.4f;M.PlayerInterval=.45f;M.DamageScale=M.DamageNear=3.f;
  M.SpreadScale=.35f;M.EngageScale=1.6f;M.ReachScale=1.3f;M.SuppressionScale=.8f;
  auto& L=X.Launcher;L.Magazine=1;L.Reserve=10;L.AmmoKind=6;L.Reload=2.6f;L.AIInterval=3.f;L.PlayerInterval=.8f;L.EngageCap=15000.f;L.ReachCap=15000.f;
  // Sniper rifle: one aimed shot kills an ordinary soldier (10 x ShotDamage = 100); its distance is limited by what the shooter sees.
  auto& N=X.Sniper;N.Magazine=5;N.Reserve=25;N.AmmoKind=7;N.Reload=3.5f;N.AIInterval=4.f;N.PlayerInterval=1.4f;N.DamageScale=N.DamageNear=10.f;
  N.SpreadScale=.15f;N.EngageScale=2.5f;N.ReachScale=2.f;N.SuppressionScale=1.5f;
  return X;}();
 switch(W)
 {
  case EBattleWeapon::Pistol:return &T.Pistol;case EBattleWeapon::SMG:return &T.SMG;case EBattleWeapon::Shotgun:return &T.Shotgun;
  case EBattleWeapon::Marksman:return &T.Marksman;case EBattleWeapon::GrenadeLauncher:return &T.Launcher;case EBattleWeapon::Sniper:return &T.Sniper;default:return nullptr;
 }
}

float BattleArms::DamageAt(EBattleWeapon W,double Distance)
{
 const auto* D=BattleWeaponDef(W);if(!D)return 1.f;
 if(D->FarRange<=D->NearRange)return D->DamageScale;
 const float A=FMath::Clamp(float((Distance-D->NearRange)/(D->FarRange-D->NearRange)),0.f,1.f);
 return FMath::Lerp(D->DamageNear,D->DamageScale,A);
}
float BattleArms::SpreadScale(EBattleWeapon W){const auto* D=BattleWeaponDef(W);return D?D->SpreadScale:1.f;}

const BattleArms::FBlast& BattleArms::Grenade()
{
 static const FBlast B=[]{FBlast X;
  X.Damage=FMath::Clamp(FloatSwitch(TEXT("BattleGrenadeDamage="),90.f),0.f,1000.f);X.Radius=FMath::Clamp(FloatSwitch(TEXT("BattleGrenadeRadius="),500.f),50.f,3000.f);
  X.MaxRange=FMath::Clamp(FloatSwitch(TEXT("BattleGrenadeRange="),3000.f),700.f,6000.f);return X;}();
 return B;
}
const BattleArms::FBlast& BattleArms::LauncherRound()
{
 static const FBlast B=[]{FBlast X;X.Damage=60.f;X.Radius=400.f;X.MinRange=1500.f;X.MaxRange=15000.f;X.Speed=4000.f;X.Fuse=0.f;X.ErrorBase=80.f;X.ErrorScale=.03f;X.Cooldown=0.f;X.Arc=.12f;
  X.Damage=FMath::Clamp(FloatSwitch(TEXT("BattleLauncherDamage="),X.Damage),0.f,1000.f);return X;}();
 return B;
}
float BattleArms::GrenadeSquadGap(){static const float V=FMath::Clamp(FloatSwitch(TEXT("BattleGrenadeSquadGap="),3.f),0.f,60.f);return V;}
const BattleArms::FMelee& BattleArms::Melee()
{
 static const FMelee M=[]{FMelee X;X.Damage=FMath::Clamp(FloatSwitch(TEXT("BattleMeleeDamage="),60.f),0.f,1000.f);return X;}();
 return M;
}
const BattleArms::FMedic& BattleArms::Medic()
{
 static const FMedic M=[]{FMedic X;X.Rate=FMath::Clamp(FloatSwitch(TEXT("BattleMedicRate="),4.f),0.f,100.f);return X;}();
 return M;
}

bool BattleArms::ConsumeSidearmShot(FBattleUnitState& U,double Now,double Interval)
{
 if(!U.IsAlive()||U.SideAmmo<=0||U.SideWait>0.f||Now<U.NextShotTime)return false;
 --U.SideAmmo;++U.ShotSequence;U.NextShotTime=Now+FMath::Max(.05,Interval);
 return true;
}

// ---- Runtime (UBattleLeaderRuntime) ----

bool UBattleLeaderRuntime::ArmsActive() const{return bArmsElite||ArmsShells.Num()>0||BattleArms::AnyTrial();}

void UBattleLeaderRuntime::IssueArms(int32 ID)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U||U->bArmsIssued)return;
 U->bArmsIssued=true;
 const auto* S=Store->FindSquad(U->SquadID);
 // Foot soldiers only: vehicles and the exoskeleton soldier carry their own weapons.
 if(!S||BattleSquadType::IsVehicle(S->SquadType)||S->SquadType==BattleSquadType::Exo)return;
 if(BattleArms::SidearmsEnabled()&&U->Weapon!=EBattleWeapon::Pistol){const auto* P=BattleWeaponDef(EBattleWeapon::Pistol);U->SideAmmo=P->Magazine;U->SideReserve=P->Reserve;}
 U->Grenades=FMath::Max(U->Grenades,BattleArms::GrenadeCount());
}

void UBattleLeaderRuntime::IssueEliteKit(int32 ID,int32 Slot,uint8 Grade)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);if(!U)return;
 const uint8 Kit=BattleArms::GradeKit(Grade,Slot);if(Kit==BattleArms::KitNone)return;const auto* T=GetDefault<UBattleTuning>();const auto* P=BattleWeaponDef(EBattleWeapon::Pistol);
 U->Kit=Kit;U->bArmsIssued=true;U->Weapon=BattleArms::KitWeapon(Kit);U->Ammo=FMath::Max(1,T->Capacity(U->Weapon));U->Reserve=FMath::Max(0,T->ReserveFor(U->Weapon));
 U->Grenades=BattleArms::KitGrenades(Kit);U->SideAmmo=P->Magazine;U->SideReserve=P->Reserve;U->bSidearmSelected=false;
 // Veteran perception and decisions; the sight stays within the saved range (the range profile's 200 m x 1.25 would exceed it), and
 // a steadier aim than the rifle squads (SpreadSkillScale).
 Store->SetTrainingPreset(ID,EBattleTraining::Veteran);
 if(auto* V=Store->Units.Find(ID))
 {
  FBattlePerceptionProfile EliteSight=V->Perception;EliteSight.SightRange=FMath::Min(EliteSight.SightRange,20000.f);EliteSight.ShareRange=FMath::Min(EliteSight.ShareRange,20000.f);
  Store->SetPerceptionProfile(ID,EliteSight);V->SpreadSkillScale=.75f;
 }
 bArmsElite=true;
}

int32 UBattleState::RefillArms(int32 ID)
{
 auto* U=Units.Find(ID);if(!U||!U->IsAlive()||!U->bArmsIssued)return 0;
 const auto* S=Squads.Find(U->SquadID);if(!S||BattleSquadType::IsVehicle(S->SquadType)||S->SquadType==BattleSquadType::Exo)return 0;
 const int32 Want=U->Kit?BattleArms::KitGrenades(U->Kit):BattleArms::GrenadeCount();
 int32 Added=FMath::Max(0,Want-U->Grenades);U->Grenades+=Added;
 if(U->Kit||BattleArms::SidearmsEnabled())
 {const auto* P=BattleWeaponDef(EBattleWeapon::Pistol);const int32 Rounds=FMath::Max(0,P->Reserve-U->SideReserve);U->SideReserve+=Rounds;Added+=Rounds;}
 return Added;
}

void UBattleLeaderRuntime::StepArms(float Dt,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 // Trial equipment for soldiers without it yet (starting squads, purchases, replacements); bArmsIssued keeps it to once.
 if(Store->GetUnits().Num()!=ArmsIssuedUnits)
 {
  ArmsIssuedUnits=Store->GetUnits().Num();
  for(const auto& P:Store->GetSquads())bArmsElite|=P.Value.Grade>0&&P.Value.CommandStatus==EBattleCommandStatus::Active;
  if(BattleArms::SidearmsEnabled()||BattleArms::GrenadeCount()>0)
  {TArray<int32> IDs;for(const auto& P:Store->GetUnits())if(!P.Value.bArmsIssued&&P.Value.IsAlive())IDs.Add(P.Key);IDs.Sort();for(int32 ID:IDs)IssueArms(ID);}
 }
 // Relative timers: thrower and striker waits, the pistol's reload, the squad's grenade gap.
 const auto* Pistol=BattleWeaponDef(EBattleWeapon::Pistol);
 for(auto& P:Store->Units)
 {
  auto& U=P.Value;if(!U.IsAlive())continue;
  if(U.GrenadeWait>0.f)U.GrenadeWait=FMath::Max(0.f,U.GrenadeWait-Dt);
  if(U.MeleeWait>0.f)U.MeleeWait=FMath::Max(0.f,U.MeleeWait-Dt);
  if(U.SideWait>0.f)
  {
   U.SideWait=FMath::Max(0.f,U.SideWait-Dt);
   if(U.SideWait==0.f){const int32 N=FMath::Min(U.SideReserve,FMath::Max(0,Pistol->Magazine-U.SideAmmo));U.SideAmmo+=N;U.SideReserve-=N;}
  }
  else if(U.SideAmmo==0&&U.SideReserve>0)U.SideWait=Pistol->Reload;
 }
 for(auto& P:Store->Squads)if(P.Value.GrenadeGap>0.f)P.Value.GrenadeGap=FMath::Max(0.f,P.Value.GrenadeGap-Dt);
 // Rounds in flight land in throw order; an explosion never throws.
 if(ArmsShells.Num())
 {
  TArray<FArmsShell> Landed;
  for(int32 I=0;I<ArmsShells.Num();){ArmsShells[I].Remaining-=Dt;if(ArmsShells[I].Remaining<=0.f){Landed.Add(ArmsShells[I]);ArmsShells.RemoveAt(I);}else ++I;}
  for(const auto& Shell:Landed)ArmsImpact(Shell);
 }
 DrawArmsShells(Now);
 if(BattleArms::SmokeEnabled()&&!bArmsSmokeDone&&Mode&&Now>=Mode->CombatStartsAt+3.){bArmsSmokeDone=true;RunArmsSmoke(Now);}
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();if(!Sense)return;
 // Strikes, four times a second: a soldier with an enemy he sees within reach strikes it instead of shooting.
 if(Now>=ArmsNextMelee)
 {
  ArmsNextMelee=Now+.25;
  const bool bAll=BattleArms::MeleeEnabled();const float Reach=BattleArms::Melee().Reach+60.f;
  TArray<int32> IDs;
  for(const auto& B:Bodies)if(const auto* U=Store->Find(B.Key);U&&U->IsAlive()&&U->MeleeWait<=0.f&&U->Action!=EBattleAction::DirectControl&&(bAll||U->Kit!=0)&&!IsVehicle(B.Key))IDs.Add(B.Key);
  IDs.Sort();
  for(int32 ID:IDs)
  {
   const auto* U=Store->Find(ID);if(!U||!U->IsAlive())continue;
   const auto* S=Store->FindSquad(U->SquadID);if(!S||S->CommandStatus!=EBattleCommandStatus::Active||S->SquadType==BattleSquadType::Exo)continue;
   const auto* Contacts=Sense->GetContacts(ID);if(!Contacts)continue;
   const FVector Pos=Position(ID);int32 Hint=INDEX_NONE;double Best=FMath::Square(double(Reach));
   for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared)
   {const double D=FVector::DistSquared2D(Pos,C.Value.LastKnownPosition);if(D<Best||(D==Best&&C.Key<Hint)){Best=D;Hint=C.Key;}}
   if(Hint!=INDEX_NONE)MeleeStrike(ID,Now,false,Hint);
  }
 }
 // Twice a second: medics heal, grenadiers fire launcher rounds, soldiers with grenades throw them.
 if(Now<ArmsNextPick)return;
 ArmsNextPick=Now+.5;
 const auto& G=BattleArms::Grenade();const auto& L=BattleArms::LauncherRound();const auto& Med=BattleArms::Medic();
 TArray<int32> IDs;
 for(const auto& B:Bodies)if(const auto* U=Store->Find(B.Key);U&&U->IsAlive()&&U->Action!=EBattleAction::DirectControl&&!IsVehicle(B.Key)&&
  (U->Grenades>0||U->Weapon==EBattleWeapon::GrenadeLauncher||U->Kit==BattleArms::KitMedic||
   (BattleArms::AssaultMode()>0&&(U->Kit==BattleArms::KitBreacher||U->Kit==BattleArms::KitAssault||U->Kit==BattleArms::KitRifleman))))IDs.Add(B.Key);
 IDs.Sort();
 for(int32 ID:IDs)
 {
  const auto* U=Store->Find(ID);if(!U||!U->IsAlive())continue;
  const auto* S=Store->FindSquad(U->SquadID);if(!S||S->CommandStatus!=EBattleCommandStatus::Active)continue;
  // Medic: the most hurt member of his own squad (himself included) within reach, while he is not under fire himself.
  if(U->Kit==BattleArms::KitMedic&&Med.Rate>0.f&&!UnderFire(ID))
  {
   const FVector MP=Position(ID);int32 Patient=INDEX_NONE;float Worst=.999f;
   for(int32 Member:S->Members)if(const auto* M=Store->Find(Member);M&&M->IsAlive())
   {
    const float Max=Store->MaxHealthOf(Member);const float Ratio=Max>0.f?M->HP/Max:1.f;
    if(Ratio<Worst&&FVector::DistSquared(MP,Position(Member))<=FMath::Square(double(Med.Range))){Worst=Ratio;Patient=Member;}
   }
   if(Patient!=INDEX_NONE&&Store->Heal(Patient,Med.Rate*.5f)>0.f)SightExperiment.Cover(TEXT("arms_heal_ticks"));
   U=Store->Find(ID);
  }
  // Close-quarters assault (-BattleArmsAssault): the elite platoon's breacher, assault man and rifleman (mode 2: also one grenade
  // carrier of an ordinary rifle squad) close on a known enemy 30-150 m away in bounds from cover to cover, while no more than two of
  // the squad are dashing and the enemies around the target are not more than half the squad. Known: seen by him, or seen / reported
  // within 3 s.
  if(const int32 DashMode=BattleArms::AssaultMode();DashMode>0&&S->Order!=EBattleOrder::Move&&!CoverApproaches.Contains(ID)&&Now>=ArmsAssaultNext.FindRef(ID))
  {
   const bool bRole=U->Kit==BattleArms::KitBreacher||U->Kit==BattleArms::KitAssault||U->Kit==BattleArms::KitRifleman;
   const bool bPlain=DashMode>=2&&S->Grade==0&&S->SquadType==BattleSquadType::Rifle&&U->Grenades>0&&S->LeaderUnitID!=ID;
   if(bRole||bPlain)
   {
    int32 Living=0,Dashing=0;
    for(int32 Member:S->Members)if(const auto* M=Store->Find(Member);M&&M->IsAlive()){++Living;if(const auto* A=CoverApproaches.Find(Member);A&&A->bAssault)++Dashing;}
    const auto* Contacts=Sense->GetContacts(ID);const FVector Pos=Position(ID);
    int32 Best=INDEX_NONE;double BestD=15000.;FVector BestAt=FVector::ZeroVector;
    if(Contacts&&Dashing<(bRole?2:1))for(const auto& C:*Contacts)
    {
     if(Now-C.Value.LastSeenTime>3.||(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(C.Value.ObservedSquadType):IsVehicle(C.Key)))continue;
     const double D=FVector::Dist2D(Pos,C.Value.LastKnownPosition);if(D<=BattleArms::Grenade().MaxRange||D>=BestD)continue;
     int32 Around=0;for(const auto& O:*Contacts)Around+=Now-O.Value.LastSeenTime<=3.&&FVector::DistSquared2D(O.Value.LastKnownPosition,C.Value.LastKnownPosition)<=FMath::Square(1500.);
     if(Around*2>Living)continue;
     Best=C.Key;BestD=D;BestAt=C.Value.LastKnownPosition;
    }
    if(Best!=INDEX_NONE)ArmsAssaultNext.Add(ID,Now+(StartAssault(ID,Best,BestAt,Now)?12.:4.));
    U=Store->Find(ID);
   }
  }
  const bool bLauncher=U->Weapon==EBattleWeapon::GrenadeLauncher&&!U->bSidearmSelected&&U->Ammo>0&&U->ReloadEndTime<=0.&&Now>=U->NextShotTime;
  const bool bGrenade=U->Grenades>0&&U->GrenadeWait<=0.f&&S->GrenadeGap<=0.f;
  if((!bLauncher&&!bGrenade)||S->Order==EBattleOrder::Move)continue;
  const auto* Contacts=Sense->GetContacts(ID);if(!Contacts)continue;
  const FVector Pos=Position(ID);
  // Known enemies: seen by this soldier now, or seen / reported within the last 3 s. Not vehicles (the blast does not hurt them).
  TArray<TPair<int32,FVector>,TInlineAllocator<32>> Known;TArray<bool,TInlineAllocator<32>> Hidden;
  for(const auto& C:*Contacts)
  {
   if(Now-C.Value.LastSeenTime>3.)continue;
   if(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(C.Value.ObservedSquadType):IsVehicle(C.Key))continue;
   Known.Add(TPair<int32,FVector>(C.Key,C.Value.LastKnownPosition));Hidden.Add(!(C.Value.bVisible&&!C.Value.bShared));
  }
  if(Known.IsEmpty())continue;
  // The best landing point: most known enemies within one blast, then a hidden one (behind cover, out of his sight), then near.
  // A hand grenade needs two or more together or a hidden one; the launcher (the grenadier's main weapon) takes any target.
  auto Pick=[&](const BattleArms::FBlast& B,float MaxRange,bool bAny,FVector& At)->bool
  {
   double BestScore=-1.e18;int32 BestKey=INDEX_NONE;const double R2=FMath::Square(double(B.Radius));
   for(int32 I=0;I<Known.Num();++I)
   {
    const double D=FVector::Dist2D(Pos,Known[I].Value);if(D<B.MinRange||D>MaxRange)continue;
    if(BattleTactics::WeaponOrderGateEnabled()&&!WeaponOrderAllows(ID,Known[I].Key,Known[I].Value))continue;
    int32 Around=0;for(const auto& O:Known)Around+=FVector::DistSquared2D(O.Value,Known[I].Value)<=R2;
    if(!bAny&&Around<2&&!Hidden[I])continue;
    const double Score=Around*2.+(Hidden[I]?1.5:0.)-D/1.e6;
    if(Score>BestScore||(Score==BestScore&&Known[I].Key<BestKey)){BestScore=Score;BestKey=Known[I].Key;At=Known[I].Value;}
   }
   return BestKey!=INDEX_NONE;
  };
  FVector At=FVector::ZeroVector;
  if(bLauncher&&Pick(L,FMath::Min(L.MaxRange,BattleRanges::Engage(EBattleWeapon::GrenadeLauncher)),true,At)&&FireLauncherRound(ID,At,Now,false))continue;
  if(bGrenade&&Pick(G,G.MaxRange,false,At))ThrowGrenade(ID,At,Now,false);
 }
}

bool UBattleLeaderRuntime::StartAssault(int32 ID,int32 Target,const FVector& TargetAt,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive()||!Bodies.Contains(ID)||IsVehicle(ID)||TargetAt.ContainsNaN())return false;
 auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();Cover->BuildSlots();
 const FVector Pos=Position(ID),Eye=TargetAt+FVector(0,0,60);const double Base=FVector::Dist2D(Pos,TargetAt);
 // One bound: a cover slot at most 40 m away that brings him at least 10 m closer to the enemy but not within 8 m of him, hides
 // from him, and has a straight walkable line to it (the same test as the cover approach). The most progress, then the shorter
 // walk. No dash across the open without cover at the end; the next bound follows from there (StepArms, every 12 s at most).
 int32 BestSlot=INDEX_NONE;double BestProgress=-1.,BestWalk=DBL_MAX;FVector BestAnchor=FVector::ZeroVector;
 const FVector Probe=Pos+(TargetAt-Pos).GetSafeNormal2D()*FMath::Max(0.,FMath::Min(3000.,Base-1500.));
 for(int32 Slot:Cover->NearbySlots(Probe,2000.f,96))
 {
  const FVector Anchor=Cover->GetSlots()[Slot]+FVector(0,0,90);
  const double ToEnemy=FVector::Dist2D(Anchor,TargetAt),Walk=FVector::Dist2D(Pos,Anchor),Progress=Base-ToEnemy;
  if(ToEnemy<800.||Progress<1000.||Walk>4000.||Progress<BestProgress-1.||(FMath::Abs(Progress-BestProgress)<=1.&&Walk>=BestWalk))continue;
  if(!Cover->IsAvailable(Slot,ID,Now))continue;
  bool bTaken=false;for(const auto& Other:CoverApproaches)if(Other.Key!=ID&&Other.Value.Slot==Slot){bTaken=true;break;}
  if(bTaken)continue;
  if(BattleTactics::ExposureAt(GetWorld(),Anchor,(TargetAt-Anchor).GetSafeNormal2D(),Eye).Fraction()>CoverExposureLimit())continue;
  if(GetWorld()->SweepTestByObjectType(Pos,Anchor,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(36,88)))continue;
  BestSlot=Slot;BestProgress=Progress;BestWalk=Walk;BestAnchor=Anchor;
 }
 if(BestSlot==INDEX_NONE){SightExperiment.Cover(TEXT("arms_assault_no_cover"));return false;}
 // The dash starts from where he is: out of his cover first.
 if(U->CoverPhase)CancelCoverAction(ID);
 U=Store->Find(ID);
 FCoverApproach A;A.Goal=BestAnchor;A.Slot=BestSlot;A.Target=Target;A.Revision=U->OrderRevision;A.Until=Now+20.;A.bAssault=true;
 CoverApproaches.Add(ID,A);FirePositions.Remove(ID);
 SightExperiment.Cover(TEXT("arms_assault_started"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_ASSAULT unit=%d team=%d target=%d from=%.0f to=%.0f"),ID,U->Team,Target,Base,FVector::Dist2D(BestAnchor,TargetAt));
 return true;
}

bool UBattleLeaderRuntime::LobClear(const FVector& From,FVector& To,float Arc) const
{
 // A lobbed round flies a parabola (8 straight pieces) over low walls but not through buildings or roofs. Something in the last fifth
 // of the way (the cover in front of the target, the ground at the end) is where the round lands instead; earlier, it cannot be fired.
 const double H=LobHeight(FVector::Dist2D(From,To),Arc);const FCollisionObjectQueryParams Static(ECC_WorldStatic);
 auto At=[&](double T){return FMath::Lerp(From,To,T)+FVector(0,0,4.*H*T*(1.-T));};
 constexpr int32 Pieces=8;
 for(int32 I=0;I<Pieces;++I)
 {
  const double T0=double(I)/Pieces,T1=double(I+1)/Pieces;FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByObjectType(Hit,At(T0),At(T1),Static))continue;
  if(T0+(T1-T0)*Hit.Time<.8)return false;
  To=Hit.ImpactPoint+Hit.ImpactNormal*20.;
  return true;
 }
 return true;
}

bool UBattleLeaderRuntime::ThrowGrenade(int32 ID,const FVector& TargetIn,double Now,bool bPlayer)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U||!U->IsAlive()||U->Grenades<=0||U->GrenadeWait>0.f||!Bodies.Contains(ID)||IsVehicle(ID)||!Mode||Mode->bBattleEnded)return false;
 const auto* S=Store->FindSquad(U->SquadID);if(!S||S->CommandStatus!=EBattleCommandStatus::Active||(!bPlayer&&S->GrenadeGap>0.f))return false;
 // The throw rises to standing height whatever the stance (a soldier behind cover throws over it).
 const auto& G=BattleArms::Grenade();const FVector Pos=Position(ID);const FVector Hand=Pos+FVector(0,0,BattleStance::EyeOffset(BattleStance::Stand));
 FVector Target=TargetIn;FVector Flat=Target-Pos;Flat.Z=0;double Dist=Flat.Size();
 if(Dist>G.MaxRange){Target=Pos+Flat/Dist*G.MaxRange;Target.Z=TargetIn.Z;Dist=G.MaxRange;}
 if(!bPlayer&&Dist<G.MinRange)return false;
 // Deterministic landing error per soldier and grenade, then down onto the ground there.
 FRandomStream Random(int32(uint32(ID)*2654435761u+uint32(U->Grenades)*40503u+7919u));
 const float Error=G.ErrorBase+G.ErrorScale*float(Dist);const float Angle=Random.FRand()*2.f*PI,R=FMath::Sqrt(Random.FRand())*Error;
 FVector At=Target+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,0);
 {FHitResult Hit;if(GetWorld()->LineTraceSingleByObjectType(Hit,At+FVector(0,0,300),At-FVector(0,0,600),FCollisionObjectQueryParams(ECC_WorldStatic)))At=Hit.ImpactPoint+FVector(0,0,20);}
 if(!LobClear(Hand,At,G.Arc)){if(!bPlayer)SightExperiment.Cover(TEXT("arms_grenade_blocked"));return false;}
 --U->Grenades;U->GrenadeWait=bPlayer?1.f:G.Cooldown;const int32 Left=U->Grenades,Team=U->Team;
 if(auto* SM=Store->Squads.Find(U->SquadID))SM->GrenadeGap=BattleArms::GrenadeSquadGap();
 FArmsShell Shell;Shell.ShooterID=ID;Shell.Team=Team;Shell.Kind=0;Shell.From=Hand;Shell.At=At;Shell.Flight=FMath::Max(.6f,float(Dist/FMath::Max(100.f,G.Speed)));Shell.Remaining=Shell.Flight+G.Fuse;
 ArmsShells.Add(Shell);
 ShowArmsAction(ID,2,Now);
 SightExperiment.Cover(bPlayer?TEXT("arms_grenade_thrown_player"):TEXT("arms_grenade_thrown"));
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_GRENADE unit=%d team=%d player=%d left=%d dist=%.0f at=%s"),ID,Team,bPlayer?1:0,Left,Dist,*At.ToCompactString());
 return true;
}

bool UBattleLeaderRuntime::FireLauncherRound(int32 ID,const FVector& TargetIn,double Now,bool bPlayer)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U||!U->IsAlive()||U->Weapon!=EBattleWeapon::GrenadeLauncher||U->bSidearmSelected||!Bodies.Contains(ID)||!Mode||Mode->bBattleEnded)return false;
 const auto* S=Store->FindSquad(U->SquadID);if(!S||S->CommandStatus!=EBattleCommandStatus::Active)return false;
 const auto& L=BattleArms::LauncherRound();const FVector Pos=Position(ID);const FVector Muzzle=Pos+FVector(0,0,BattleStance::EyeOffset(StanceOf(ID)));
 const float MaxRange=FMath::Min(L.MaxRange,BattleRanges::Reach(EBattleWeapon::GrenadeLauncher));
 FVector Target=TargetIn;FVector Flat=Target-Pos;Flat.Z=0;double Dist=Flat.Size();
 if(Dist>MaxRange){Target=Pos+Flat/Dist*MaxRange;Target.Z=TargetIn.Z;Dist=MaxRange;}
 if(!bPlayer&&Dist<L.MinRange)return false;
 FRandomStream Random(int32(uint32(ID)*2654435761u+uint32(U->ShotSequence)*40503u+104729u));
 const float Error=L.ErrorBase+L.ErrorScale*float(Dist);const float Angle=Random.FRand()*2.f*PI,R=FMath::Sqrt(Random.FRand())*Error;
 FVector At=Target+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,0);
 {FHitResult Hit;if(GetWorld()->LineTraceSingleByObjectType(Hit,At+FVector(0,0,300),At-FVector(0,0,600),FCollisionObjectQueryParams(ECC_WorldStatic)))At=Hit.ImpactPoint+FVector(0,0,20);}
 if(!LobClear(Muzzle,At,L.Arc)){if(!bPlayer)SightExperiment.Cover(TEXT("arms_gl_blocked"));return false;}
 if(!Store->ConsumeShot(ID,Now,GetDefault<UBattleTuning>()->ShotInterval(EBattleWeapon::GrenadeLauncher,bPlayer)))return false;
 const int32 Team=U->Team;
 FArmsShell Shell;Shell.ShooterID=ID;Shell.Team=Team;Shell.Kind=1;Shell.From=Muzzle;Shell.At=At;Shell.Flight=FMath::Max(.3f,float(Dist/FMath::Max(100.f,L.Speed)));Shell.Remaining=Shell.Flight+L.Fuse;
 ArmsShells.Add(Shell);
 LastFireTimes.Add(ID,Now);
 if(ABattleUnit* A=Actor(ID))A->NoteVisualShot(Now);
 const auto* B=Bodies.Find(ID);
 if(Team==0||(Actor(ID)&&Actor(ID)->bRevealed)||(B&&B->Mesh.IsValid()&&B->Mesh->IsVisible()))BattleMuzzleFlash::Show(GetWorld(),Muzzle,(At-Muzzle).GetSafeNormal(),1.6f);
 SuppressNear(Team,Muzzle,At,Now,.5f);
 SightExperiment.Cover(bPlayer?TEXT("arms_gl_shots_player"):TEXT("arms_gl_shots"));
 if(SightExperiment.Started){++SightExperiment.Shots;if(S->SquadType<BattleSquadType::Count&&Team>=0&&Team<2)++SightExperiment.TypeShots[Team][S->SquadType];}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_LAUNCH unit=%d team=%d player=%d dist=%.0f at=%s"),ID,Team,bPlayer?1:0,Dist,*At.ToCompactString());
 return true;
}

int32 UBattleLeaderRuntime::ArmsBlast(int32 ShooterID,int32 Team,const FVector& At,float Radius,float Damage,bool& bKilled)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();bKilled=false;
 const auto* Shooter=Store->Find(ShooterID);const auto* ShooterSquad=Shooter?Store->FindSquad(Shooter->SquadID):nullptr;
 const uint8 ShooterType=ShooterSquad?ShooterSquad->SquadType:uint8(BattleSquadType::Count);
 // Collect first: damage broadcasts deaths. Enemies only, no vehicles; walls stop the blast (feet, centre and head must see it);
 // lying flat takes less; the exoskeleton's armour scales it like small arms.
 TArray<TPair<int32,float>> Hits;const FCollisionObjectQueryParams Static(ECC_WorldStatic);
 for(const auto& B:Bodies)
 {
  if(IsVehicle(B.Key))continue;const auto* U=Store->Find(B.Key);if(!U||!U->IsAlive()||U->Team==Team)continue;
  const FVector P=Position(B.Key);const double D=FVector::Dist(P,At);if(D>Radius)continue;
  int32 Clear=0;for(const float Z:{-40.f,0.f,40.f})Clear+=!GetWorld()->LineTraceTestByObjectType(At+FVector(0,0,30),P+FVector(0,0,Z),Static);
  if(Clear==0)continue;
  const float Low=StanceOf(B.Key)==BattleStance::Prone?.6f:1.f;
  const float Value=float(Damage*(1.0-D/Radius))*Clear/3.f*Low*ArmorScale(B.Key,EBattleWeapon::Carbine);
  if(Value>0.f)Hits.Add(TPair<int32,float>(B.Key,Value));
 }
 Hits.Sort([](const TPair<int32,float>& A,const TPair<int32,float>& B){return A.Key<B.Key;});
 int32 Damaged=0;
 for(const auto& H:Hits)
 {
  const auto* Before=Store->Find(H.Key);if(!Before||!Before->IsAlive())continue;
  const float BeforeHP=Before->HP;const int32 VictimTeam=Before->Team;const auto* VS=Store->FindSquad(Before->SquadID);const uint8 VictimType=VS?VS->SquadType:0;
  LastShotDirs.Add(H.Key,(Position(H.Key)-At).GetSafeNormal2D());
  if(!Store->ApplyDamageFrom(H.Key,Team,H.Value,At,ShooterID,EBattleKillKind::Grenade))continue;
  ++Damaged;const bool Killed=!Store->Find(H.Key)->IsAlive();bKilled|=Killed;
  if(SightExperiment.Started)
  {
   auto& E=SightExperiment;const float Dealt=FMath::Min(BeforeHP,H.Value);++E.Hits;
   if(ShooterType<BattleSquadType::Count&&Team>=0&&Team<2){E.TypeDamage[Team][ShooterType]+=Dealt;E.TypeKills[Team][ShooterType]+=Killed;}
   if(VictimType<BattleSquadType::Count&&VictimTeam>=0&&VictimTeam<2){E.TypeTaken[VictimTeam][VictimType]+=Dealt;E.TypeDeaths[VictimTeam][VictimType]+=Killed;}
   if(Team>=0&&Team<2){E.TeamDamage[Team]+=Dealt;E.TeamKills[Team]+=Killed;}
   E.Cover(TEXT("arms_blast_damage_hits"));if(Killed)E.Cover(TEXT("arms_blast_kills"));
  }
 }
 return Damaged;
}

void UBattleLeaderRuntime::ArmsImpact(const FArmsShell& Shell)
{
 const auto& B=Shell.Kind==1?BattleArms::LauncherRound():BattleArms::Grenade();
 bool bKilled=false;const int32 Damaged=ArmsBlast(Shell.ShooterID,Shell.Team,Shell.At,B.Radius,B.Damage,bKilled);
 const double Now=GetWorld()->GetTimeSeconds();
 SuppressNear(Shell.Team,Shell.At+FVector(0,0,300),Shell.At,Now,BattleSuppression::Tuning().SupportScale);
 SightExperiment.Cover(Shell.Kind==1?TEXT("arms_gl_impacts"):TEXT("arms_grenade_impacts"));
 // An explosion is seen from anywhere: the flash for all, the radius only for the player's side or a shooter he sees.
 BattleMuzzleFlash::Show(GetWorld(),Shell.At+FVector(0,0,40),FVector::UpVector,Shell.Kind==1?5.f:6.f,.25f);
 if(Shell.Team==0||(Actor(Shell.ShooterID)&&Actor(Shell.ShooterID)->bRevealed))DrawDebugSphere(GetWorld(),Shell.At,B.Radius,10,Shell.Team?FColor::Red:FColor::Yellow,false,.4f,0,2);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_BLAST kind=%s shooter=%d team=%d damaged=%d killed=%d at=%s"),Shell.Kind==1?TEXT("launcher"):TEXT("grenade"),Shell.ShooterID,Shell.Team,Damaged,bKilled?1:0,*Shell.At.ToCompactString());
}

bool UBattleLeaderRuntime::MeleeStrike(int32 ID,double Now,bool bPlayer,int32 Hint)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U||!U->IsAlive()||U->MeleeWait>0.f||!Bodies.Contains(ID)||IsVehicle(ID)||!Mode||Mode->bBattleEnded)return false;
 const auto& M=BattleArms::Melee();const FVector Pos=Position(ID);const int32 Team=U->Team;
 FVector Facing=Forward(ID).GetSafeNormal2D();
 if(bPlayer)if(ABattleUnit* A=Actor(ID))Facing=A->GetControlRotation().Vector().GetSafeNormal2D();
 // The victim: the enemy the AI saw within reach, or the nearest enemy body in front of the player. The reach and the wall
 // between them are physical (true positions).
 int32 Victim=INDEX_NONE;double Best=FMath::Square(double(M.Reach));
 auto Consider=[&](int32 Other)
 {
  const auto* O=Store->Find(Other);if(!O||!O->IsAlive()||O->Team==Team||IsVehicle(Other))return;
  const FVector To=Position(Other)-Pos;const double D2=To.SizeSquared2D();if(D2>Best||FMath::Abs(To.Z)>150.)return;
  if(bPlayer&&D2>1.&&FVector::DotProduct(To.GetSafeNormal2D(),Facing)<M.ConeCos)return;
  Best=D2;Victim=Other;
 };
 if(Hint!=INDEX_NONE)Consider(Hint);else for(const auto& B:Bodies)Consider(B.Key);
 if(Victim==INDEX_NONE)return false;
 if(GetWorld()->LineTraceTestByObjectType(Pos+FVector(0,0,40),Position(Victim)+FVector(0,0,40),FCollisionObjectQueryParams(ECC_WorldStatic)))return false;
 U->MeleeWait=M.Cooldown;
 const auto* ShooterSquad=Store->FindSquad(U->SquadID);const uint8 ShooterType=ShooterSquad?ShooterSquad->SquadType:uint8(BattleSquadType::Count);
 const auto* Before=Store->Find(Victim);const float BeforeHP=Before->HP;const int32 VictimTeam=Before->Team;
 const auto* VS=Store->FindSquad(Before->SquadID);const uint8 VictimType=VS?VS->SquadType:0;
 const float Damage=M.Damage*ArmorScale(Victim,EBattleWeapon::Carbine);
 LastShotDirs.Add(Victim,(Position(Victim)-Pos).GetSafeNormal2D());
 ShowArmsAction(ID,1,Now);
 const bool bHit=Damage>0.f&&Store->ApplyDamageFrom(Victim,Team,Damage,Pos,ID,EBattleKillKind::Melee);
 const bool bKilled=bHit&&!Store->Find(Victim)->IsAlive();
 SightExperiment.Cover(bPlayer?TEXT("arms_melee_strikes_player"):TEXT("arms_melee_strikes"));
 if(bKilled)SightExperiment.Cover(TEXT("arms_melee_kills"));
 if(bHit&&SightExperiment.Started)
 {
  auto& E=SightExperiment;const float Dealt=FMath::Min(BeforeHP,Damage);++E.Hits;
  if(ShooterType<BattleSquadType::Count&&Team>=0&&Team<2){E.TypeDamage[Team][ShooterType]+=Dealt;E.TypeKills[Team][ShooterType]+=bKilled;}
  if(VictimType<BattleSquadType::Count&&VictimTeam>=0&&VictimTeam<2){E.TypeTaken[VictimTeam][VictimType]+=Dealt;E.TypeDeaths[VictimTeam][VictimType]+=bKilled;}
  if(Team>=0&&Team<2){E.TeamDamage[Team]+=Dealt;E.TeamKills[Team]+=bKilled;}
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_MELEE unit=%d team=%d victim=%d player=%d damage=%.0f killed=%d"),ID,Team,Victim,bPlayer?1:0,Damage,bKilled?1:0);
 return true;
}

bool UBattleLeaderRuntime::ToggleSidearm(int32 ID)
{
 auto* U=GetWorld()->GetSubsystem<UBattleState>()->Units.Find(ID);
 if(!U||!U->IsAlive()||!U->HasSidearm())return false;
 U->bSidearmSelected=!U->bSidearmSelected;RefreshArmsLook(ID);
 return true;
}

bool UBattleLeaderRuntime::UseSidearm(int32 ID,int32 TargetID,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* U=Store->Units.Find(ID);
 if(!U||U->Action==EBattleAction::DirectControl||(U->SideAmmo+U->SideReserve<=0&&!U->bSidearmSelected))return false;
 // Draw while the primary cannot fire (empty, reloading, or a weapon without direct fire) and the target he sees is within the
 // pistol's distance; put it away once the primary is ready again.
 bool bWant=false;
 if(U->SideAmmo>0||U->SideWait>0.f)
 {
  const bool bPrimaryOut=U->Ammo==0||U->ReloadEndTime>0.||U->Weapon==EBattleWeapon::GrenadeLauncher||U->Weapon==EBattleWeapon::Mortar;
  if(bPrimaryOut&&TargetID!=INDEX_NONE)
   if(const auto* C=GetWorld()->GetSubsystem<UBattlePerception>()->FindContact(ID,TargetID);C&&C->bVisible)
    bWant=FVector::DistSquared(Position(ID),C->LastKnownPosition)<=FMath::Square(double(BattleRanges::Engage(EBattleWeapon::Pistol)));
 }
 if(bWant&&U->Ammo==0&&U->Reserve>0&&U->ReloadEndTime<=0.)Store->StartReload(ID,Now);
 if(bWant!=U->bSidearmSelected){U->bSidearmSelected=bWant;RefreshArmsLook(ID);if(bWant)SightExperiment.Cover(TEXT("arms_sidearm_draws"));}
 return bWant&&U->SideAmmo>0;
}

int32 UBattleLeaderRuntime::ArmsRoleTarget(int32 ID,int32 SquadTarget) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!U||(U->Kit!=BattleArms::KitMarksman&&U->Kit!=BattleArms::KitSniper&&U->Kit!=BattleArms::KitBreacher&&U->Kit!=BattleArms::KitAssault))return SquadTarget;
 const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID);if(!Contacts)return SquadTarget;
 const FVector Pos=Position(ID);const double Engage2=FMath::Square(double(BattleRanges::Engage(U->Weapon)));
 auto Usable=[&](int32 Key,const FBattleContact& C)
 {return C.bVisible&&!C.bShared&&FVector::DistSquared(Pos,C.LastKnownPosition)<=Engage2&&!(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(C.ObservedSquadType):IsVehicle(Key));};
 const auto* Squad=SquadTarget!=INDEX_NONE?Contacts->Find(SquadTarget):nullptr;const bool bSquadUsable=Squad&&Usable(SquadTarget,*Squad);
 // Close weapons (shotgun, SMG): the squad's target while it is within their distance, else the nearest enemy they see within it.
 // Marksman: an enemy he sees carrying a machine gun, anti-tank weapon, marksman rifle, launcher or sniper rifle first (nearest), else
 // the squad's. Sniper: the same, and before them a squad leader he sees (told apart by look: radio, hand signals).
 int32 Best=INDEX_NONE;double BestScore=-1.e18;
 for(const auto& C:*Contacts)if(Usable(C.Key,C.Value))
 {
  double Score=-FVector::Dist(Pos,C.Value.LastKnownPosition);
  if(U->Kit==BattleArms::KitMarksman||U->Kit==BattleArms::KitSniper)
  {
   const auto* O=Store->Find(C.Key);const EBattleWeapon W=O?O->ActiveWeapon():EBattleWeapon::Carbine;
   if(W==EBattleWeapon::SupportGun||W==EBattleWeapon::AntiTank||W==EBattleWeapon::Marksman||W==EBattleWeapon::GrenadeLauncher||W==EBattleWeapon::Sniper)Score+=1.e7;
   if(U->Kit==BattleArms::KitSniper&&O)if(const auto* OS=Store->FindSquad(O->SquadID);OS&&OS->LeaderUnitID==C.Key)Score+=2.e7;
  }
  if(Score>BestScore||(Score==BestScore&&C.Key<Best)){BestScore=Score;Best=C.Key;}
 }
 if(Best==INDEX_NONE)return SquadTarget;
 if(U->Kit==BattleArms::KitMarksman||U->Kit==BattleArms::KitSniper)return BestScore>0.||!bSquadUsable?Best:SquadTarget;
 return bSquadUsable?SquadTarget:Best;
}

void UBattleLeaderRuntime::RefreshArmsLook(int32 ID)
{
 const auto* U=GetWorld()->GetSubsystem<UBattleState>()->Find(ID);const auto* B=Bodies.Find(ID);if(!U||!B)return;
 if(ABattleUnit* A=B->Actor.Get())A->RemountWeapon();
 else if(B->Weapon.IsValid()&&B->Mesh.IsValid())GetDefault<ABattleUnit>()->MountWeapon(B->Weapon.Get(),B->Mesh.Get(),U->ActiveWeapon(),false);
}

namespace
{
 // Display only: existing mannequin clips. No throw clip exists, so a second strike clip stands in for the throw.
 UAnimSequence* ArmsActionClip(uint8 Kind)
 {
  static TStrongObjectPtr<UAnimSequence> Strike,Throw;static bool bLoaded=false;
  if(!bLoaded)
  {
   bLoaded=true;
   Strike.Reset(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"),nullptr,LOAD_NoWarn));
   Throw.Reset(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"),nullptr,LOAD_NoWarn));
  }
  return Kind==1?Strike.Get():Throw.Get();
 }
}

UAnimSequence* UBattleLeaderRuntime::ArmsClip(int32 ID,double Now) const
{
 const double* Until=ArmsActionUntil.Find(ID);if(!Until||*Until<=Now)return nullptr;
 return ArmsActionClip(ArmsActionKind.FindRef(ID));
}

void UBattleLeaderRuntime::ShowArmsAction(int32 ID,uint8 Kind,double Now)
{
 UAnimSequence* Clip=ArmsActionClip(Kind);if(!Clip)return;
 const auto* B=Bodies.Find(ID);if(!B)return;
 if(ABattleUnit* A=B->Actor.Get()){A->PlayArmsClip(Clip);return;}
 // A follower plays one whole-body clip: the strike or throw for a moment, then its normal pose again (a body timer).
 ArmsActionUntil.Add(ID,Now+.8);ArmsActionKind.Add(ID,Kind);
 RefreshFollowerPose(ID);
 if(!BodyTimers.Contains(ID)){FBodyTimer Timer;Timer.At=Now+.81;BodyTimers.Add(ID,Timer);}
}

void UBattleLeaderRuntime::DrawArmsShells(double Now)
{
 // Display only: a small ball along the arc while the round flies (the player's side, or a shooter he sees).
 if(!DisplayHost)return;
 static TStrongObjectPtr<UStaticMesh> Ball;static TStrongObjectPtr<UMaterialInterface> Base;
 if(!Ball.IsValid()){Ball.Reset(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Base.Reset(LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));}
 if(!Ball.IsValid())return;
 int32 Used=0;
 for(const auto& Shell:ArmsShells)
 {
  const auto* SB=Bodies.Find(Shell.ShooterID);
  const bool bShow=Shell.Team==0||(Actor(Shell.ShooterID)&&Actor(Shell.ShooterID)->bRevealed)||(SB&&SB->Mesh.IsValid()&&SB->Mesh->IsVisible());
  if(!bShow)continue;
  const float Fuse=(Shell.Kind==1?BattleArms::LauncherRound():BattleArms::Grenade()).Fuse;const float Arc=(Shell.Kind==1?BattleArms::LauncherRound():BattleArms::Grenade()).Arc;
  const float T=Shell.Flight>0.f?FMath::Clamp((Shell.Flight+Fuse-Shell.Remaining)/Shell.Flight,0.f,1.f):1.f;
  const double H=LobHeight(FVector::Dist2D(Shell.From,Shell.At),Arc);
  const FVector At=FMath::Lerp(Shell.From,Shell.At,double(T))+FVector(0,0,4.*H*T*(1.-T));
  if(!ArmsShellMeshes.IsValidIndex(Used)||!ArmsShellMeshes[Used].IsValid())
  {
   auto* C=NewObject<UStaticMeshComponent>(DisplayHost);C->SetStaticMesh(Ball.Get());C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   C->SetCanEverAffectNavigation(false);C->SetCastShadow(false);C->SetRelativeScale3D(FVector(.12f));
   if(Base.IsValid())if(auto* Paint=UMaterialInstanceDynamic::Create(Base.Get(),C)){Paint->SetVectorParameterValue(TEXT("Color"),FLinearColor(.12f,.16f,.08f));C->SetMaterial(0,Paint);}
   C->SetupAttachment(DisplayHost->GetRootComponent());C->RegisterComponent();DisplayHost->AddInstanceComponent(C);
   if(ArmsShellMeshes.IsValidIndex(Used))ArmsShellMeshes[Used]=C;else ArmsShellMeshes.Add(C);
  }
  auto* C=ArmsShellMeshes[Used].Get();C->SetWorldLocation(At);if(!C->IsVisible())C->SetVisibility(true);++Used;
 }
 for(int32 I=Used;I<ArmsShellMeshes.Num();++I)if(auto* C=ArmsShellMeshes[I].Get();C&&C->IsVisible())C->SetVisibility(false);
}

// Self-test (-BattleArmsSmoke -BattleElite=1 on an area map): the weapon table, an elite platoon's purchase and kit, a blast on an
// enemy, a pistol shot and a hand grenade in flight. Changes the match (a test run only).
void UBattleLeaderRuntime::RunArmsSmoke(double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const auto* T=GetDefault<UBattleTuning>();
 int32 Failures=0;
 auto Check=[&](bool bOk,const TCHAR* What){if(!bOk){++Failures;UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_ARMS_SMOKE_FAIL %s"),What);}};
 for(const EBattleWeapon W:{EBattleWeapon::Pistol,EBattleWeapon::SMG,EBattleWeapon::Shotgun,EBattleWeapon::Marksman,EBattleWeapon::GrenadeLauncher})
  Check(BattleWeaponDef(W)&&T->Capacity(W)>0&&T->ReserveFor(W)>0&&T->ReloadTime(W)>0.f&&BattleRanges::Engage(W)>0.f&&BattleRanges::Reach(W)>=BattleRanges::Engage(W),TEXT("weapon table"));
 Check(BattleWeaponDef(EBattleWeapon::Carbine)==nullptr&&BattleArms::DamageAt(EBattleWeapon::Carbine,1000.)==1.f&&BattleArms::SpreadScale(EBattleWeapon::SupportGun)==1.f,TEXT("old weapons unchanged"));
 Check(FMath::IsNearlyEqual(BattleArms::DamageAt(EBattleWeapon::Shotgun,100.),4.f)&&FMath::IsNearlyEqual(BattleArms::DamageAt(EBattleWeapon::Shotgun,3000.),1.f),TEXT("shotgun falloff"));
 // Purchase from the starting resources (adding resources here would break the resource conservation the save check verifies).
 int32 SID=INDEX_NONE;
 if(M&&M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Rifle,1))SID=M->LastHiredSquadID;
 const auto* S=SID!=INDEX_NONE?Store->FindSquad(SID):nullptr;
 Check(S&&S->Grade==1&&S->SquadType==BattleSquadType::Rifle&&S->Members.Num()==BattleSquadType::RifleMembers(),TEXT("elite purchase"));
 if(!S){UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_SMOKE_RESULT failures=%d (no squad: %s)"),Failures,M?*M->LastMessage:TEXT("no match"));return;}
 for(int32 Slot=0;Slot<S->Members.Num();++Slot)
 {
  const auto* U=Store->Find(S->Members[Slot]);const uint8 Kit=BattleArms::EliteKit(Slot);
  Check(U&&U->Kit==Kit&&U->Weapon==BattleArms::KitWeapon(Kit)&&U->Grenades==BattleArms::KitGrenades(Kit)&&U->SideAmmo>0&&U->Training==EBattleTraining::Veteran&&
   U->Perception.SightRange<=20000.f&&U->Ammo==T->Capacity(U->Weapon),TEXT("elite kit"));
 }
 const int32 Leader=S->LeaderUnitID;
 // A blast on an enemy foot soldier.
 int32 Enemy=INDEX_NONE;for(const auto& B:Bodies)if(const auto* U=Store->Find(B.Key);U&&U->IsAlive()&&U->Team==1&&!IsVehicle(B.Key)){Enemy=B.Key;break;}
 if(Enemy!=INDEX_NONE){const float HP0=Store->Find(Enemy)->HP;bool bKilled=false;ArmsBlast(Leader,0,Position(Enemy),500.f,30.f,bKilled);Check(Store->Find(Enemy)->HP<HP0,TEXT("blast damage"));}
 else Check(false,TEXT("no enemy for the blast"));
 // Pistol: drawn, one shot from the pistol's magazine, put away.
 const int32 Side0=Store->Find(Leader)->SideAmmo;
 Check(ToggleSidearm(Leader)&&Store->Find(Leader)->ActiveWeapon()==EBattleWeapon::Pistol,TEXT("draw pistol"));
 const FVector Eye=Position(Leader)+FVector(0,0,BattleStance::EyeOffset(StanceOf(Leader)));
 Check(Fire(Leader,Eye,Forward(Leader),true)&&Store->Find(Leader)->SideAmmo==Side0-1&&Store->Find(Leader)->Ammo==T->Capacity(EBattleWeapon::Carbine),TEXT("pistol shot"));
 Check(ToggleSidearm(Leader)&&!Store->Find(Leader)->bSidearmSelected,TEXT("put pistol away"));
 // A hand grenade 10 m away in some open direction, in flight afterwards.
 const int32 G0=Store->Find(Leader)->Grenades;bool bThrown=false;
 for(int32 Dir=0;Dir<8&&!bThrown;++Dir)bThrown=ThrowGrenade(Leader,Position(Leader)+FVector(FMath::Cos(Dir*PI/4),FMath::Sin(Dir*PI/4),0)*1000.,Now,true);
 Check(bThrown&&Store->Find(Leader)->Grenades==G0-1&&ArmsShells.Num()>=1&&ArmsShells.Last().ShooterID==Leader,TEXT("grenade thrown"));
 // Sniper team: a scout pair of grade 2, the sniper leads (sniper rifle), the spotter follows (rifle).
 int32 Sniper=INDEX_NONE;
 if(M&&M->HireAreaSquad(0,EBattleTraining::Regular,false,BattleSquadType::Scout,2))Sniper=M->LastHiredSquadID;
 const auto* SS=Sniper!=INDEX_NONE?Store->FindSquad(Sniper):nullptr;
 Check(SS&&SS->Grade==2&&SS->SquadType==BattleSquadType::Scout&&SS->Members.Num()==2&&M->ActiveScouts(0)==M->ActiveOfType(0,BattleSquadType::Scout)-M->ActiveSnipers(0),TEXT("sniper team purchase"));
 if(SS)
 {
  const auto* A=Store->Find(SS->Members[0]);const auto* B=Store->Find(SS->Members[1]);
  Check(A&&B&&SS->LeaderUnitID==SS->Members[0]&&A->Kit==BattleArms::KitSniper&&A->Weapon==EBattleWeapon::Sniper&&B->Kit==BattleArms::KitSpotter&&B->Weapon==EBattleWeapon::Carbine&&
   BattleRanges::Engage(EBattleWeapon::Sniper)>BattleRanges::Engage(EBattleWeapon::Marksman),TEXT("sniper team kit"));
 }
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_ARMS_SMOKE_RESULT failures=%d squad=%d leader=%d sniper_team=%d"),Failures,SID,Leader,Sniper);
}
