#pragma once
#include "CoreMinimal.h"
#include "BattleState.h"

// Infantry arms (2026-11-22, user request; Docs/design-infantry-arms.md): sidearms, hand grenades, melee strikes, the elite platoon
// (a rifle squad with Grade 1 and a role per slot) and the weapons added to EBattleWeapon. Values and switches live here; the
// runtime part (throws, launcher rounds, strikes, healing, sidearm use, role targets) is UBattleLeaderRuntime in BattleArms.cpp.
// Every rule change is a trial switch, default 0: with all of them off and no elite platoon bought, nothing here runs.
namespace BattleArms
{
 // Roles in an elite platoon (FBattleUnitState::Kit). 0 = an ordinary soldier.
 // Kits 1-8: elite platoon (Grade 1). Kits 9-10: sniper team (Grade 2: the sniper leads, the spotter follows).
 enum EKit : uint8 { KitNone=0, KitLeader, KitGunner, KitGrenadier, KitMarksman, KitBreacher, KitAssault, KitMedic, KitRifleman, KitSniper, KitSpotter, KitCount };
 bool SidearmsEnabled();  // -BattleSidearms=1: every foot soldier carries a pistol
 int32 GrenadeCount();    // -BattleGrenades=n: hand grenades per foot soldier (0 = none)
 bool MeleeEnabled();     // -BattleMelee=1: every foot soldier strikes an enemy within reach
 bool EliteEnabled();     // -BattleElite=1: the player may buy elite platoons (= key)
 int32 AreaEliteKeep();   // -BattleAreaEliteKeep=n: elite platoons the area AI keeps (0 = none)
 int32 EliteCost();       // -BattleEliteCost=n: area purchase cost (default 26; a rifle squad costs AreaSquadCost)
 bool SniperEnabled();    // -BattleSniperTeam=1: the player may buy sniper teams (] key)
 int32 AreaSniperKeep();  // -BattleAreaSniperKeep=n: sniper teams the area AI keeps (0 = none)
 int32 SniperCost();      // -BattleSniperCost=n: area purchase cost (default 14; a scout pair costs AreaScoutCost)
 constexpr int32 SniperLimit=3; // sniper teams per team (they do not count against the scout pair limit)
 int32 AssaultMode();     // -BattleArmsAssault: 0 off, 1 elite assault roles dash close (UBattleLeaderRuntime::StartAssault), 2 also one
                          // grenade carrier per ordinary rifle squad
 bool ChargeEnabled();    // -BattleArmsCharge=1: the assault roles (with -BattleArmsAssault=2 also ordinary riflemen) charge an enemy
                          // they see within 12 m, or after an assault bound one known within 30 m, and strike him (StartCharge)
 bool SmokeEnabled();     // -BattleArmsSmoke: the self-test (UBattleLeaderRuntime::RunArmsSmoke)
 bool AnyTrial();         // any of the switches above
 uint8 EliteKit(int32 Slot);            // role of a slot (slots past the table are riflemen)
 uint8 GradeKit(uint8 Grade,int32 Slot); // the role of a slot in a squad of that grade (1 elite platoon, 2 sniper team)
 bool ValidGrade(uint8 SquadType,uint8 Grade); // 0 any; 1 rifle squads; 2 scout pairs
 bool GradeWeapon(uint8 Grade,EBattleWeapon W); // a weapon some role of that grade carries (save validation)
 EBattleWeapon KitWeapon(uint8 Kit);    // primary weapon of a role
 int32 KitGrenades(uint8 Kit);
 bool ValidKit(uint8 Kit);
 bool EliteWeapon(EBattleWeapon W);     // a weapon some elite role carries (save validation)
 const TCHAR* KitName(uint8 Kit);       // Japanese role name for the HUD ("" for none)
 const TCHAR* WeaponName(EBattleWeapon W);
 // Damage scale of a shot of W that travelled Distance cm (the shotgun's falls with distance; others are constant).
 float DamageAt(EBattleWeapon W,double Distance);
 float SpreadScale(EBattleWeapon W);
 // Hand grenade and launcher round: centre damage falling to zero at Radius, throw/fire distance limits, flight speed (cm/s),
 // fuse after landing, landing error (cm) = ErrorBase + ErrorScale x distance, and the thrower's own wait.
 struct FBlast { float Damage=90.f,Radius=500.f,MinRange=600.f,MaxRange=3000.f,Speed=1500.f,Fuse=.4f,ErrorBase=50.f,ErrorScale=.06f,Cooldown=8.f,Arc=.25f; };
 const FBlast& Grenade();
 const FBlast& LauncherRound();
 float GrenadeSquadGap(); // one hand grenade per squad per this many seconds
 struct FMelee { float Damage=60.f,Reach=200.f,Cooldown=1.2f,ConeCos=.5f; };
 const FMelee& Melee();
 // The charge: at an enemy he sees within Range cm, or - within BoundWindow s after an assault bound brought him to cover - at one
 // seen or reported within Known s and BoundRange cm (also hidden behind his cover: he runs to where the enemy was last seen). Not
 // more than CrowdMax known enemies within CrowdRadius of the target (him included) and a clear straight way - from where he is, or
 // from a step aside (his cover's firing point, or SideStep1 / SideStep2 cm to either side), which he walks to first (until within
 // ViaReach cm). The soldier runs at SpeedScale x the walking speed to Stop cm from him for at most Limit s; the next charge waits
 // Cooldown s from this start; at most SquadMax of a squad at once.
 struct FCharge { float Range=1200.f,BoundRange=3000.f,BoundWindow=15.f,Known=3.f,Stop=120.f,Limit=8.f,SpeedScale=1.3f,CrowdRadius=600.f,Cooldown=8.f,SideStep1=220.f,SideStep2=400.f,ViaReach=40.f; int32 CrowdMax=2,SquadMax=2; };
 const FCharge& Charge();
 struct FMedic { float Rate=4.f,Range=1000.f; };
 const FMedic& Medic();
 // The pistol: one shot while drawn (rounds loaded, shot timer shared with the primary). False when it cannot fire.
 bool ConsumeSidearmShot(FBattleUnitState& U,double Now,double Interval);
}
