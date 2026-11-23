#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "BattleState.h"
#include "BattleAnimTasks.h"
#include "BattleUnit.h"
#include "BattleSightExperiment.h"
#include "BattleTactics.h"
#include "BattleCoverClips.h"
#include "BattleBodyAnim.h"
#include "BattleLeaderRuntime.generated.h"

class ABattleGameMode;
class UBattleSave;
struct FBattleSavedUnit;
class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// Stage A: a cover place a soldier left because it was seen into. The place is kept by position (slot numbers follow the
// order the places were built in), so it means the same place after a load, also before the places are built again.
struct FBattleBrokenCover
{
 int32 Slot=INDEX_NONE; // as numbered when he left it (for saves and logs)
 FVector At=FVector::ZeroVector;
 double Until=0;
};

USTRUCT()
struct FBattleFollowerFragment : public FMassFragment
{
 GENERATED_BODY()
 int32 UnitID=INDEX_NONE;
 FVector Position=FVector::ZeroVector;
 FVector Forward=FVector::ForwardVector;
};

enum class EBattleSquadIntent : uint8 { Unspecified };
struct FBattleSquadSummary
{
 int32 SquadID=INDEX_NONE, LeaderUnitID=INDEX_NONE, Revision=0;
 FString Name;
 bool bActive=false, bSelectable=false;
 EBattleSquadIntent Intent=EBattleSquadIntent::Unspecified;
 uint32 ProblemFlags=0;
};
// One row of the command UI squad list (2026-11-20): a group header (GroupID set, bGroupHeader) or a squad (GroupID = its group in the group view).
struct FBattleSquadListRow
{
 FBattleSquadSummary Squad;
 int32 GroupID=INDEX_NONE;
 bool bGroupHeader=false;
 bool bNewGroup=false; // the "+ new attack group" drop row at the end of the attack army tab (2026-11-21)
 // Click identity: the squad ID, -(GroupID+1) for a group header, NewGroupKey for the new group row (never INDEX_NONE).
 static constexpr int32 NewGroupKey=-1000000;
 int32 Key() const { return bNewGroup?NewGroupKey:bGroupHeader?-(GroupID+1):Squad.SquadID; }
};

// Shared by Canvas and pointer input; no stale hit rectangles from the previous draw.
struct FBattleSquadListLayout
{
 FVector2D Origin;
 float Width=360, Height=0, RowHeight=56, Header=44;
 int32 VisibleRows=0;
 static FBattleSquadListLayout Make(FVector2D View,const UWorld* World);
 // Header controls (command UI 2026-11-16; tabs 2026-11-20): four tabs (front army, attack army, unassigned, all) and a sort button on the second header line.
 static constexpr int32 FilterCount=4; static constexpr float TabWidth=64, TabTop=23, TabHeight=18, SortWidth=86;
 int32 FilterTabAt(FVector2D P) const {if(P.Y<Origin.Y+TabTop||P.Y>=Origin.Y+TabTop+TabHeight||P.X<Origin.X+6)return INDEX_NONE;const int32 I=FMath::FloorToInt((P.X-Origin.X-6)/TabWidth);return I<FilterCount?I:INDEX_NONE;}
 bool SortButtonAt(FVector2D P) const {return P.Y>=Origin.Y+TabTop&&P.Y<Origin.Y+TabTop+TabHeight&&P.X>=Origin.X+Width-SortWidth-8&&P.X<Origin.X+Width-8;}
 bool Contains(FVector2D P) const;
 int32 RowAt(FVector2D P) const;
};

// Tactical minimap, shared by Canvas drawing and pointer input. World +X is up and +Y is right, like the tactical camera.
struct FBattleMinimapLayout
{
 FVector2D Origin=FVector2D::ZeroVector, Size=FVector2D::ZeroVector, HalfExtent=FVector2D(30000,20000);
 static FBattleMinimapLayout Make(FVector2D View,const UWorld* World);
 bool Contains(FVector2D P) const { return Size.X>0 && P.X>=Origin.X && P.X<Origin.X+Size.X && P.Y>=Origin.Y && P.Y<Origin.Y+Size.Y; }
 FVector2D ToScreen(FVector World) const { return FVector2D(Origin.X+(World.Y+HalfExtent.Y)/(2*HalfExtent.Y)*Size.X,Origin.Y+(HalfExtent.X-World.X)/(2*HalfExtent.X)*Size.Y); }
 FVector ToWorld(FVector2D P) const { return FVector(HalfExtent.X-(P.Y-Origin.Y)/Size.Y*2*HalfExtent.X,(P.X-Origin.X)/Size.X*2*HalfExtent.Y-HalfExtent.Y,0); }
};

struct FBattleLeaderBody
{
 TWeakObjectPtr<ABattleUnit> Actor;
 FMassEntityHandle Entity;
 TWeakObjectPtr<UCapsuleComponent> Capsule;
 TWeakObjectPtr<USkeletalMeshComponent> Mesh;
 TWeakObjectPtr<UStaticMeshComponent> Weapon;
 FVector Velocity=FVector::ZeroVector;
 double StalledSince=0;
 // Position and continuous movement ticks for the headless experiment only.
 FVector MoveSampleOrigin=FVector::ZeroVector;
 uint64 LastMoveSampleFrame=0;
 float StationaryMoveSeconds=0;
 FVector LastProgressPosition=FVector::ZeroVector;
 double LastProgressTime=0;
 bool bTrafficWait=false, bArrived=false;
 // Display: the near follower mesh shows the blind fire clip (BattleBlindFire.cpp).
 bool bBlindPose=false;
 bool bRunPose=false;
 float MuzzleYaw=0.f,MuzzlePitch=0.f; // AimFollowerRifles: the mesh turn and tilt that point the rifle along the shot
 // Display: cover phase whose clip the near follower mesh shows (2 hidden, 4 firing; 0 none) and the table picked for it.
 uint8 CoverClipPhase=0;
 BattleCoverClips::FSet CoverClipSet{};
 int32 PassingID=INDEX_NONE;

 FVector PassingWaypoint=FVector::ZeroVector, PassingForward=FVector::ForwardVector, PassingOffset=FVector::ZeroVector;
 bool bPassingSecond=false;
 double EscapeUntil=0, NextEscapeProbe=0;
 // Map E: earliest time this member may ask for a route of his own again after being cut off from the shared corridor.
 double NextOwnPath=0;
 FVector EscapeWaypoint=FVector::ZeroVector;
 TArray<FVector> ContactRoute;
 double NextContactProbe=0, ContactRouteUntil=0, NextArrivalProbe=0;
 bool bArrivalOccupied=false, bContactWaiting=false;
 // Trial -BattleAllyPass=1 (2026-11-19, user request): after waiting 3 s in front of a stopped ally cluster that no detour gets
 // around, the soldier may walk through stopped allies (only them) until AllyPassUntil. The components it ignores meanwhile.
 double AllyPassUntil=0;
 TArray<TWeakObjectPtr<UPrimitiveComponent>> AllyPassIgnored;
 int32 Waypoint=1, Pose=0;
 FBattleAnimTasks AnimTasks;
};
struct FBattleLeaderPlan
{
 int32 OrderRevision=INDEX_NONE, TargetID=INDEX_NONE;
 // Armour AI: the visible enemy vehicle the squad's anti-tank gunners fire at (INDEX_NONE = none; they share TargetID then).
 int32 ArmourTargetID=INDEX_NONE;
 double NextThink=0, NextPath=0;
 bool bReplan=true;
 // Trial -BattleSectorContact=1: the target is an enemy outside the sector the squad was sent to, picked because it shot at them.
 bool bSectorContact=false;
 // Trial -BattleSquadCoverCheck=1: the squad, shot at on its way to the sector, found cover for at least 80% of its living
 // members near it, so it stops and fights from there instead of firing on the move (re-checked every 2 s).
 bool bCoverStop=false;double CoverCheckAt=0;
 // Trial -BattleBounding=1: while stopping in cover on the way, half the squad (BoundGroup = member index % 2) moves on firing
 // while the other half fights from cover; they swap every 7 s (none moves for the first 4 s). -1 = nobody moves.
 int32 BoundGroup=-1;double BoundSwitchAt=0;
 // Trial -BattlePinnedFlank=1: shot at for 20 s on the way without getting 10 m closer to the sector = pinned (the AI order
 // loop then sends the squad to a sector beside the threat first, UBattleMatch::PinnedFlankStage).
 double PinnedSince=0,PinnedBestDist=0;bool bPinned=false;FVector PinnedAt=FVector::ZeroVector;
 // Trial -BattleArrivalThreat=1: the direction the arrival places were chosen against (zero = the direction of advance).
 FVector ArrivalFacing=FVector::ZeroVector;double FacingCheckAt=0;
 // Trial -BattleAttackFocus=1: the point this sector squad defends toward (from its team's sightings, re-picked every 3 s) and
 // the direction its current arrival places were chosen against (from the destination; zero = not chosen against a focus).
 FVector Focus=FVector::ZeroVector;bool bFocus=false;FVector FocusPlanDir=FVector::ZeroVector;double FocusCheckAt=0,FocusReplanAt=0;
 // Trial -BattleCloseIn=1: in its sector, fighting a target over 90 m away for 8 s while at least even (the -BattleSquadCoverCheck
 // count) = too far (the AI order loop then moves the squad one sector closer, UBattleMatch::CloseInStage).
 double FarSince=0,FarCheckAt=0;bool bCloseIn=false;FVector CloseInAt=FVector::ZeroVector;
 TArray<FVector> Path;
 FVector Center=FVector::ZeroVector;
 TMap<int32,FVector> ArrivalGoals;
 // Map E cover arrival: a member's own short route from the end of the shared path to a cover slot that the
 // destination cannot reach in a straight line. Rebuilt on demand (not saved).
 TMap<int32,TArray<FVector>> ArrivalTails;
 TMap<int32,TArray<FVector>> Corridors;
 int32 PathRequests=0, ArrivalReassignments=0, ContactProbes=0;
 int32 ArrivalRevision=INDEX_NONE;
 bool bNarrowCorridor=false;
};

UCLASS()
class UBattleLeaderRuntime : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool IsEnabled() const { return bEnabled; }
 bool IsWideField() const { return bWideField; }
 bool RangeAwareEnabled() const {return bRangeAwareCombat;}
 // Return fire (trial -BattleReturnFire=1, ROADMAP range table): a soldier hit within the last ReturnFireSeconds may engage a
 // contact it can currently see up to its weapon reach instead of the shorter AI engagement distance; its squad may pick such a target.
 // Only differs from the old rule where the range profile separates reach from engagement (map E).
 // One-team trials (-BattleAreaTrialTeam=t, 2026-11-17): only that team's soldiers return fire.
 bool ExoSuppressionApplies(int32 ID) const;
 void NoteUnderFire(int32 ID);
 bool UnderFire(int32 ID) const {const double* T=UnderFireUntil.Find(ID);return T&&*T>GetWorld()->GetTimeSeconds();}
 TMap<int32,double> UnderFireUntil;
 // Trial -BattleSectorContact=1 (2026-11-19, default 0): a squad on a sector mission that is shot at (hit or near miss within
 // the last 5 s) fights back against enemies outside the sector: members take cover or a safer firing place nearby (also
 // outside the sector) and fight from it; members that find none keep going to the sector and fire on the way.
 static bool SectorContactEnabled();
 TMap<int32,double> SectorHitUntil;
 // Fall guard (2026-11-19): a soldier body (an Actor) that drops 5 m below the ground (the open river channel of map H has no
 // floor) is put back where it last stood on the ground and stops; without such a place it dies. Never happens on solid ground.
 void GuardFalls(double Now);
 TMap<int32,FVector> LastGround;
 double NextFallCheck=0;
 bool SquadShotAt(const struct FBattleSquadState& S,double Now) const;
 // Squad target search range for one member: the AI engagement distance, or its weapon reach while under fire (return fire).
 float TargetRange(int32 ID) const;
 bool bMixedFastMatch=false;
 bool bArea=false;
 FName SnapshotMode() const {return bArea?TEXT("Area"):bMixedFastMatch?TEXT("MixedFast"):TEXT("Wide");}
 bool ReplaceFollower(int32 DeadID);
 // Scout pairs: the surviving follower becomes the squad leader (its Mass body is replaced by an Actor in place).
 bool PromoteScout(int32 SquadID,int32 NewLeaderID);
 // -BattleHireSightRefresh=1 restores the full sight update after a hire or scout handover (default 0 = skipped, user decision 2026-11-18).
 static bool HireSightRefresh();
 // Long sight of a still observer: a scout pair member, or any soldier at a lookout (0 when moving or neither).
 // AtLookout: within LookoutRadius of a lookout (public map places).
 bool AtLookout(int32 ID) const;
 bool HasLongSight(int32 ID) const;
 mutable TArray<FVector> LookoutCache; mutable bool bLookoutCache=false; // BattleFieldLayout::Lookouts of this world
 float ScoutStillSight(int32 ID) const;
 // Anti-tank explosion at a point: enemies of the shooter within ATSplashRadius lose up to ATSplashDamage (falling off to zero at the edge).
 // Radius/Damage below zero: the anti-tank values (the tank gun passes its own).
 int32 Explode(int32 ShooterID,FVector At,int32 ExcludeID,float Radius=-1.f,float Damage=-1.f);
 // Vehicles (tanks, squad type 4; armoured cars, type 5): living vehicle IDs (refreshed on purchase, restore and death), their bodies, armour and steering.
 TSet<int32> VehicleIDs;
 TMap<int32,TWeakObjectPtr<USceneComponent>> Turrets;
 bool IsVehicle(int32 ID) const {return VehicleIDs.Num()>0&&VehicleIDs.Contains(ID);}
 bool HasMainGun(int32 ID) const {return IsVehicle(ID)&&BattleSquadType::HasMainGun(VehicleType(ID));}
 uint8 VehicleType(int32 ID) const; // the unit's squad type (tank, armoured car; Rifle when unknown)
 void ConfigureVehicle(int32 ID);
 // The armoured exoskeleton soldier is drawn larger (the near skeletal body only; the far VAT display keeps the normal size).
 void ApplySquadLook(int32 SquadID);
 // Exoskeleton second weapon (BattleExo.cpp): picks a target among what the soldier sees itself and fires an anti-tank round.
 void StepExoLaunchers(double Now);
 bool FireExoLauncher(int32 ID,const FVector& Aim,double Now);
 // Display only: the launcher in hand through its recoil and reload clips (BattleExo.cpp); returns their length (0 without them:
 // the launcher shows half a second as before).
 float PlayExoLauncherAction(int32 ID,double Now);
 double ExoNextPick=0;int64 ExoLauncherShots=0,ExoLauncherHits=0;
 // Look: armour parts on a rig that follows the chest and keeps the body's heading, the launcher tube, and a shield bubble shown while shield points are left.
 // bSuit: the AC01 suit (ApplyExoSuit: own body, clips and weapon models) instead of the placeholder plates.
 struct FExoLook{TWeakObjectPtr<USceneComponent> Rig;TWeakObjectPtr<class UStaticMeshComponent> Bubble;TWeakObjectPtr<class UMaterialInstanceDynamic> BubblePaint;TWeakObjectPtr<class UStaticMeshComponent> Stowed;FTransform StowUpright,StowProne;bool bSuit=false,bShowingLauncher=false,bStowProne=false;double AILauncherUntil=0.;};
 bool ApplyExoSuit(int32 ID);
 TMap<int32,FExoLook> ExoLooks;
 void UpdateExoLooks();
 float VehicleSpotRange() const;
 // Damage scale of a weapon on a unit: anti-tank hits fully; small arms by the vehicle type (tank none, armoured car ACArmorSmallArms).
 float ArmorScale(int32 VictimID,EBattleWeapon Weapon) const;
 // Turns a vehicle towards Direction at its type's turn rate; it drives only roughly forwards (Direction becomes its heading, Scale drops while turning).
 void SteerVehicle(int32 ID,FVector& Direction,float& Scale);
 void AimTurret(int32 ID,const FVector& Direction);
 // Armour AI, stage 3 (BattleArmourAI.cpp). PickArmourTargets: weapon-aware squad targets while the squad sees a vehicle.
 // FaceVehicle: a stopped vehicle turns its hull (its forward view) at its turn rate. VehicleWatchDirection: towards the nearest
 // enemy its team knows of, else the enemy home.
 bool HasArmourKnowledge(int32 Team) const;
 void PickArmourTargets(int32 SquadID,struct FBattleLeaderPlan& Plan,const TSet<int32>* AllowedTargets=nullptr,FVector* SelectedPosition=nullptr);
 void FaceVehicle(int32 ID,const FVector& Direction);
 FVector VehicleWatchDirection(int32 ID) const;
 // Navigation data for a unit's paths: the vehicle agent for tanks, nullptr (the default infantry data) otherwise.
 class ANavigationData* NavFor(int32 ID) const;
 // Performance (2026-11-17): a far enemy vehicle (beyond the observer's own sight, within VehicleSpotRange) has its line of sight
 // traced at most every TankSightReuse seconds per observer; range and field of view are still checked every scan.
 bool CanSeeVehicleFar(int32 ObserverID,const FBattleSightContext& Context,int32 TargetID,double Now) const;
 mutable TMap<FIntPoint,TPair<double,bool>> VehicleSightCache; // (observer, vehicle) -> (trace due time, last result)
 // A vehicle's accepted order: the goal the caller asked for (the squad's own goal may be the end of a partial route) and the
 // mission revision it produced, so the director does not reissue the same order every update. A refused goal waits 5 s.
 struct FVehicleOrder{FVector Requested=FVector::ZeroVector;int32 MissionRevision=-1;FVector Refused=FVector(1.e9);double RetryAt=0;};
 TMap<int32,FVehicleOrder> VehicleOrders;
 bool SameVehicleMission(int32 SquadID,EBattleOrder Order,const FVector& Goal) const;
 // Tank main gun (stage 2): picked target and reload per tank (not saved: a restored tank's gun is loaded).
 struct FTankGun{double ReadyAt=0,NextPick=0;int32 TargetKey=INDEX_NONE,Shots=0;FVector Aim=FVector::ZeroVector;bool bHasTarget=false,bBase=false;};
 TMap<int32,FTankGun> TankGuns;
 float TankGunEngage() const;
 float TankGunReach() const;
 // Picks the main gun target (every 0.5 s) and points Direction at it (the turret follows). False: no main gun target.
 bool TankGunTarget(int32 ID,double Now,FVector& Direction);
 // Fires when loaded and the turret is on the target. True when a round was fired.
 bool StepTankGun(int32 ID,double Now);
 bool FireTankGun(int32 ID,const FVector& Aim,double Now);
 bool TankGunHit(int32 ID,int32 VictimID); // a direct hit's damage (TankGunDamage, armour pierced)
 int64 TankGunShots=0,TankGunHits=0,TankGunKills=0,TankGunVehicleHits=0,TankGunBaseHits=0,TankGunPicks=0;double TankGunMs=0;
 // Indirect fire (BattleArtillery.cpp, stage 1 2026-11-18): the mortar gunner (weapon Mortar) and the self-propelled gun (squad type 7).
 // Saved by combat extension 1. SetupCredit carries elapsed setup across a new world clock without a negative StillSince sentinel.
 struct FArtillery{double StillSince=-1,ReadyAt=0,NextPick=0,SetupCredit=0;int32 Shots=0,TargetKey=INDEX_NONE;FVector Aim=FVector::ZeroVector;bool bHasTarget=false,bBase=false;};
 struct FArtilleryShell{int32 ShooterID=INDEX_NONE,Team=0;uint8 ShooterType=0;bool bSPG=false;FVector At=FVector::ZeroVector;float Remaining=0;};
 TMap<int32,FArtillery> Artillery;
 TArray<FArtilleryShell> ArtilleryShells;
 bool WeaponOrderAllows(int32 ID,int32 TargetID,const FVector& KnownPosition) const;
 bool IsArtillery(int32 ID) const; // a living mortar gunner or self-propelled gun
 bool IsSPG(int32 ID) const {return IsVehicle(ID)&&VehicleType(ID)==BattleSquadType::SPGun;}
 float ArtilleryMinRange(bool bSPG) const;
 float ArtilleryMaxRange(bool bSPG) const;
 float ArtilleryBlastRadius(bool bSPG) const;
 float ArtillerySpread(bool bSPG,double Distance) const; // landing circle radius at a distance
 // Per frame from Follow: set-up (bMoving undoes it), a target pick every ArtilleryPickInterval s while still, firing when set up and
 // loaded. True with Direction (2D) towards the current target.
 bool StepArtillery(int32 ID,double Now,bool bMoving,FVector& Direction);
 // Picks an aim point from Known (unit key -> known position): a cluster of at least ArtilleryMinCluster soldiers within range and no
 // own unit within the blast radius plus spread (bFriendlyHeld: a candidate was refused for that). Then the enemy home if seen.
 bool PickArtilleryAim(int32 ID,const TMap<int32,FVector>& Known,FVector& Aim,bool& bBase,bool& bFriendlyHeld,int32* TargetKey=nullptr,bool bTeamClusters=false,double Now=0.,const TMap<int32,uint8>* KnownTypes=nullptr);
 TMap<int32,int32> ArtillerySupportSquad; // group fire support: the own squad each gun last fired for (log on change; not saved)
 int64 ArtilleryGroupAims=0;
 TMap<int32,int32> CounterBatteryTarget; // -BattleCounterBattery: the enemy shooter each gun last aimed at (log on change; not saved)
 int64 CounterBatteryAims=0;
 TMap<int32,int32> CounterBatteryShots; // -BattleCounterBatteryShots: counter-battery shells fired in the current burst (not saved)
 TMap<int32,double> CounterBatteryRestUntil; // ... and the time the gun shoots only at soldiers until (not saved)
 int64 CounterBatteryFired=0,CounterBatteryRests=0;
 bool AllyNear(int32 Team,const FVector& At,double Radius) const;
 // Launches one shell at Aim (spread applied, flight time Distance / shell speed). Consumes mortar ammunition or loads the gun.
 bool FireArtillery(int32 ID,const FVector& Aim,double Now);
 void StepArtilleryShells(float Dt);
 bool ArtilleryClusterSmoke() const;
 float ArtilleryBlastTransmission(const FVector& From,const FVector& To) const;
 int32 ArtilleryImpact(const FArtilleryShell& Shell); // the explosion; returns the units damaged
 int64 ArtilleryShotsMortar=0,ArtilleryShotsSPG=0,ArtilleryImpacts=0,ArtilleryHits=0,ArtilleryKills=0,ArtilleryBaseHits=0,ArtilleryFriendlyHolds=0,ArtilleryPicks=0;double ArtilleryMs=0;
 // Evaluation counters for BATTLE_TANK_COST (not AI input).
 mutable int64 TankSightTraces=0,TankSightReused=0,TankOrderSkips=0;mutable double TankSightMs=0;int32 TankOrders=0;double TankOrderMs=0;
 // MixedFast or Area. bSelect selects a new team-0 leader (player purchase); OutSquadID receives the new squad.
 bool SpawnPurchasedSquad(int32 Team,EBattleTraining Training,FString& Error,bool bSelect=true,int32* OutSquadID=nullptr,uint8 SquadType=0);
 bool CreateBody(const FBattleSavedUnit& Entry,AActor* Host,FBattleLeaderBody& Body);
 void DestroyBody(FBattleLeaderBody& Body);
 bool RestoreMixedBodies(const UBattleSave* Save,FString& Error);
 void RunMixedSaveSmoke();
 void RunMixedFastSmoke();
 void RunAreaSmoke();
 void RunCompositionSmoke();
 static FVector LimitedSteering(FVector Desired, FVector Separation, float MaxSide);
 bool Start(ABattleGameMode& Mode);
 void Step(float DeltaTime);
 void RefreshDisplay();
 virtual void Deinitialize() override;
 FVector Position(int32 ID) const;
 FVector Forward(int32 ID) const;
 ABattleUnit* Actor(int32 ID) const;
 const TMap<int32,FBattleLeaderBody>& GetBodies() const { return Bodies; }
 FBattleSightContext Sight(int32 ID) const;
 bool CanSee(int32 ObserverID,int32 TargetID) const;
 // Context must belong to the current observer/update; never retained across updates.
 // ObserverID (optional) enables the line-of-sight pair reuse trial (-BattleSightPairReuse=1).
 bool CanSeeWithContext(const FBattleSightContext& Context,int32 TargetID,int32 ObserverID=-1) const;
 struct FSightPair{double At=0;FVector Low=FVector::ZeroVector,High=FVector::ZeroVector;bool bLowObserver=false,bVisible=false;};
 mutable TMap<uint64,FSightPair> SightPairs; mutable double SightPairsPruneAt=0;
 mutable int64 SightPairTraced=0,SightPairReused=0; // evaluation counters (BATTLE_SIGHT_PAIR)
 // The parts of every unit line-of-sight test (CanSee, CanSeeWithContext, CanSeeVehicleFar and the parallel perception scan): the
 // checks before the trace (false = not visible without tracing; End receives the trace end), the trace and the hit policy.
 bool PrepareSightTrace(const FBattleSightContext& Context,int32 TargetID,FVector& End) const;
 bool SightHitVisible(const FHitResult& Hit,int32 TargetID) const { return !Hit.bBlockingHit || HitIDs.FindRef(Hit.GetComponent())==TargetID; }
 // Evaluation (-BattleSightHitStats=1, default OFF; behaviour unchanged): the first object each line-of-sight trace hits, by kind,
 // with the trace time; class names, actor tags and actor names of the hits. Logged as BATTLE_SIGHT_HITS* at match end.
 enum ESightHitSource : uint8 {SightFromPerception,SightFromCanSee,SightFromVehicleFar,SightFromArea,SightSourceCount};
 // One sight trace from the context's eye to End on ECC_Visibility, recorded for the hit statistics when they are on (TargetID -1: a point).
 FHitResult TraceSight(const FBattleSightContext& Context,const FVector& End,int32 TargetID,ESightHitSource Source) const;
 static bool SightHitStatsEnabled();
 // -BattleSightTeamMask (rules, default ON since 2026-11-22 by user decision; =0 restores the old rule; user request 2026-11-17): soldiers
 // of one side do not block their own side's line of sight, nor its bullets (Fire traces with Sight(ID).Query, which carries the mask).
 // Each soldier body carries the mask bit 1<<team (FBodyInstance mask filter) and sight queries ignore the observer's bit
 // (FCollisionQueryParams::IgnoreMask; Chaos skips a shape when the two masks share a bit). The trace channel stays ECC_Visibility.
 static bool SightTeamMaskEnabled();
 FMaskFilter SightTeamMaskFor(int32 ObserverID) const;
 void RefreshSightTeamMasks();
 void RecordSightHit(const FHitResult& Hit,int32 TargetID,uint64 Cycles,ESightHitSource Source) const;
 void LogSightHitStats(const TCHAR* Why) const;
 struct FSightHitCount{int64 Count=0;uint64 Cycles=0;};
 static constexpr int32 SightHitKindCount=12;
 mutable FSightHitCount SightHitKinds[SightHitKindCount],SightHitSources[SightSourceCount];
 mutable TMap<FString,int64> SightHitClasses,SightHitTags,SightHitActors;
 mutable bool bSightHitsLogged=false;
 int64 MoveSweeps=0,MoveHitSoldier=0,MoveHitWorld=0,MoveShortened=0; // Mass follower moves (BATTLE_MOVE_SWEEP)
 bool Fire(int32 ID,FVector Start,FVector Direction,bool bPlayerShot);
 FVector TracerStart(int32 ID,const FVector& Start,const FVector& Direction) const;
 bool RequestCoverAction(int32 ID,FVector Threat,int32 Target=INDEX_NONE,float SearchRadius=-1.f,bool bAllHidden=false,const TFunction<bool(int32,const FVector&)>& Accept=nullptr);
 void CancelCoverAction(int32 ID);
 bool StepCoverAction(int32 ID,float Dt,double Now);
 void RunCoverActionSmoke();
 void RunSectorSmoke();
 double NextCoverProbe=0;
 uint64 CoverProbeFrame=0;int32 CoverProbesThisFrame=0;
 TMap<int32,double> CoverProbeAfter;
 // Evaluation only: per soldier, the cover-search gate of his last Follow frame and how his last request ended (-1 = found).
 TMap<int32,uint8> CoverGate;TMap<int32,int8> LastCoverStage;
 TMap<int32,double> NoCoverProneUntil; // -BattleNoCoverFallback (runtime only)
 // -BattleCoverPriority (runtime only): own per-frame cap for hit soldiers, and when each soldier's last search found nothing.
 uint64 PriorityProbeFrame=0;int32 PriorityProbesThisFrame=0;TMap<int32,double> CoverProbeFailedAt;
 // Set by every StartFirePosition call: false only because the soldier is already out of sight crouched where he is.
 bool bFirePositionHereGood=false;
 // Trial -BattleCoverApproach: runtime-only walk toward a hidden cover slot (not saved; cleared on restore).
 struct FCoverApproach{FVector Goal=FVector::ZeroVector;int32 Slot=INDEX_NONE,Target=INDEX_NONE,Revision=0;double Until=0;bool bKeptTargetChange=false,bFree=false,bStandUp=false,bFallback=false;};
 // -BattleContactCover: where a soldier settled after walking to a firing position that is not a cover slot (runtime only, evaluation).
 TMap<int32,FVector> FirePositions;
 TMap<int32,FCoverApproach> CoverApproaches;
 // -BattleCoverFire: shots fired during the current AI peek (runtime only; a restored peek starts from zero).
 TMap<int32,int32> CoverPeekShots;
 // Blind fire (2026-11-19, user request; BattleBlindFire.cpp): a soldier hidden behind cover (CoverPhase 2) fires over a low
 // cover or around the edge of a wide one without leaving it. The player does it by firing without aiming (aiming and firing
 // still peeks); AI soldiers only with the trial -BattleAIBlindFire=1, while pinned. -BattleBlindFire=0 turns it off.
 // Runtime only (display): until when the blind fire clip shows.
 TMap<int32,double> BlindFireUntil;
 // Runtime only (display): the point the body turns its rifle toward during a blind fire around the edge, 2 m past the
 // origin toward the aim, so the clip points around the corner rather than into the cover.
 TMap<int32,FVector> BlindFireLook;
 // Runtime only (display): the top of the cover under an over-the-top blind fire origin (world Z), where the body's hands
 // raise the rifle to (ABattleUnit::UpdateBodyAnim). Measured once per blind shot by one line down from the origin.
 TMap<int32,float> BlindFireTop;
 // Over-the-top blind fire shows the head (BattleBlindFire::ExposeEnabled): while it does, the heights others aim at and
 // hit are the head's. Every rule that reads a target's stance height goes through these two.
 bool IsHeadExposed(int32 ID,double Now) const;
 // Around the edge: the origin of the last side blind shot, and whether that part of the body shows now (Point = its centre).
 TMap<int32,FVector> BlindFireSide;
 bool SideExposure(int32 ID,double Now,FVector& Point) const;
 float TargetAimOffset(int32 ID) const;
 float TargetTopOffset(int32 ID) const;
 // Mesh yaw (degrees, right positive) that turns a body facing Forward toward BlindFireLook; 0 over the top or when unknown.
 float BlindFireYaw(int32 ID,const FVector& Forward) const;
 // Body animation (BattleBodyAnim.h; display only, runtime only): travel direction of the last shot that hit each soldier,
 // and near followers whose one-shot clip ends at a time (a death clip then freezes; a stance transition returns to the loop).
 TMap<int32,FVector> LastShotDirs;
 FVector LastShotDirection(int32 ID) const {return LastShotDirs.FindRef(ID);}
 // The death clip a follower body shows (the same choice as OnDeath): the far display hands it back to a returning corpse.
 class UAnimSequence* DeathClip(int32 ID) const;
 // Display: when each soldier last fired. A near follower moving without firing for BattleBodyAnim::ReadySeconds runs with
 // the rifle low (FollowerClip); B.bRunPose is the run shown, refreshed by the pose updates.
 TMap<int32,double> LastFireTimes;
 bool ShowsRun(int32 ID,int32 Pose,double Now) const {return Pose==1 && Now-LastFireTimes.FindRef(ID,-100.)>=BattleBodyAnim::ReadySeconds && StanceOf(ID)==0;}
 struct FBodyTimer{double At=0;bool bFreeze=false;bool bSettle=false;}; // bSettle: a fallen corpse stops its ragdoll (BattleRagdoll)
 TMap<int32,FBodyTimer> BodyTimers;
 void StepBodyTimers();
 // Once a second: a near living follower whose mesh shows another clip than FollowerClip (a one-shot left on its last
 // frame) is put back on his loop and logged (BATTLE_BODY_POSE_RESET).
 double NextPoseAudit=0;
 void AuditFollowerPoses(double Now);
 // Every frame: a near follower that fired within BattleBodyAnim::ReadySeconds turns his mesh (whole-body clip, no upper layer)
 // so the rifle in his hand points along his last shot: across (B.MuzzleYaw, at most MaxFollowerMuzzleYaw) to his facing, and
 // up or down (B.MuzzlePitch, at most MaxFollowerMuzzlePitch) to the shot's pitch, the upper body bending about spine_01
 // (UBattleVATSkeletalMeshComponent::SpinePitch; the legs and feet keep the clip's pose). Both ease back to 0 when he stops
 // firing. Not in cover (the cover clips turn the mesh themselves), not prone,
 // not reloading or running. Followers keep their single-node clip (the far display and bone reductions rely on it).
 void AimFollowerRifles(double Now);
 static constexpr float MaxFollowerMuzzleYaw=45.f,MaxFollowerMuzzlePitch=20.f;
 TMap<int32,FVector> FollowerAims; // a follower's last shot direction (Fire), for AimFollowerRifles
 // Corpse removal (BattleCorpses.cpp; display only, runtime only). -BattleCorpseLifetime=S (default 20; 0 = bodies stay):
 // S seconds after death a body sinks into the ground for 1.5 s and is then hidden. The unit record, its position and
 // its collision (already off) are unchanged; the match still differs slightly (see BattleCorpses.cpp).
 struct FCorpse{double SinkAt=0;FVector BaseOffset=FVector::ZeroVector;bool bSinking=false;};
 TMap<int32,FCorpse> Corpses;
 TSet<int32> HiddenCorpses;
 static float CorpseLifetime();
 void AddCorpse(int32 ID);
 void StepCorpses();
 void HideCorpse(int32 ID);
 bool IsCorpseHidden(int32 ID) const {return HiddenCorpses.Contains(ID);}
 // Stage 2 diagnosis (BattleStateTrace.cpp, Docs/design-divergence-trace.md): -BattleStateTrace=<N> writes BATTLE_STATE_TRACE every N
 // frames with one hash per block of the match state (positions, health, cover, perception, plans, match, shots); reads only.
 void StepStateTrace();
 int32 StateTraceFrame=0;
 bool IsBlindFiring(int32 ID,double Now) const {const double* T=BlindFireUntil.Find(ID);return T&&*T>Now;}
 // Where a blind shot leaves: above the top of a low cover, or just past the edge of a wide one, with a clear first stretch
 // toward AimPoint. False when neither works (the caller falls back to the old peek).
 bool BlindFireOrigin(int32 ID,FVector AimPoint,FVector& Origin) const;
 bool bAIBlindShot=false;
 bool TryAIBlindFire(int32 ID,FVector Threat,double Now);
 // Evaluation only: records how an AI peek ended (peek_end_*) and its shot count; removes the CoverPeekShots entry.
 void EndPeekDiag(int32 ID,const TCHAR* Cause);
 // Cover validation: a visibility line from a standing position at From to the observed threat position
 // (own capsule ignored; soldiers' bodies are transient and do not count as blocking).
 bool CoverLineClear(int32 ID,FVector From,FVector Threat) const;
 // Re-picks a peek point beside the cover anchor with a clear line to Threat; false keeps the soldier hidden.
 bool RevalidatePeek(int32 ID,FVector Threat);
 // Suppression trial (-BattleSuppression=1, see BattleSuppression.h). Runtime only: time at which each unit's level decays to zero.
 TMap<int32,double> SuppressionZeroAt;
 float Suppression(int32 ID,double Now) const;
 bool IsPinned(int32 ID,double Now) const;
 float SuppressionSpreadScale(int32 ID,double Now) const;
 void AddSuppression(int32 ID,float Amount,double Now);
 // Suppresses living enemies of ShooterTeam within the tuning radius of the travelled bullet path Start..End.
 void SuppressNear(int32 ShooterTeam,FVector Start,FVector End,double Now,float Scale=1.f);
 // Stances (BattleStance.h; map E default). Runtime only: 0 stand, 1 crouch, 2 prone, and the earliest time each may change again.
 TMap<int32,uint8> Stances;
 TMap<int32,double> StanceLockedUntil;
 // Trial -BattleStanceMove=2: until when a soldier on the move stays crouched after his last threat (LowMoveHoldSeconds).
 // Crouching lowers his eye: behind a low wall he lost the enemy, stood up, saw him again and crouched again (in a map E match
 // about 25,000 changes each way); the hold keeps him low through that.
 TMap<int32,double> LowMoveUntil;
 static constexpr double LowMoveHoldSeconds=3.;
 uint8 StanceOf(int32 ID) const {const uint8* Found=Stances.Find(ID);return Found?*Found:0;}
 // bOnTheMove: he walks off as the stance changes; a near follower's transition clip is then played within
 // BattleBodyAnim::MovingTransitionSeconds (a leader's body hurries its own from its velocity, ABattleUnit::UpdateBodyAnim).
 bool SetStance(int32 ID,uint8 Stance,double Now,bool bForce=false,bool bOnTheMove=false);
 // Follower clip for pose 0 idle / 1 walk / 2 reload; with -BattleStanceAnim=1 a crouched follower gets the crouch clip.
 class UAnimSequence* FollowerClip(int32 ID,int32 Pose) const;
 // Drops a follower's rifle turn and tilt (AimFollowerRifles) and puts his mesh back in its plain place: at death and when
 // the far display takes the body (it keeps the mesh's transform for the far body and a returning corpse).
 void ClearFollowerRifleTurn(int32 ID);
 void RefreshFollowerPose(int32 ID);
 // Plays FollowerClip on a near follower's mesh (a crouched or prone walk at the rate its steps match his walking speed).
 void PlayFollowerClip(int32 ID,int32 Pose);
 // A stance transition still playing on a near follower ends now (his pose's loop at its own rate): the far display keeps the
 // clip's rate and time when it takes the body, and the transition's hurried rate carried into the far walk and back.
 void EndFollowerTransition(int32 ID);
 // He sees an enemy himself now (his own visible contacts, not ones shared by others).
 bool SeesEnemy(int32 ID) const;
 // bThreat: he is under fire or sees an enemy now (his own state and sight), for -BattleStanceMove=2.
 void UpdateAIStance(int32 ID,double Now,bool bMoving,bool bCombat,bool bLostSight,bool bThreat=false);
 // Concealment (hedges tagged BattleConcealment in the map): blocks sight but shots ignore it. Collected once per world.
 TArray<AActor*> ConcealmentActors;
 bool bConcealmentBuilt=false;
 bool StartCoverApproach(int32 ID,FVector Threat,int32 Target,double Now,float LocalRadius);
 // -BattleContactCover: the firing point of a cover slot against Threat (observed position). The slot itself when a soldier can crouch
 // out of sight and stand up to fire, or when he can fire from it with most of his body covered; otherwise the nearest step sideways
 // (1 to 4 m) with a line to the threat. False when the slot is too exposed or offers no such point.
 bool ContactFirePoint(int32 ID,FVector Anchor,FVector Threat,FVector& Peek);
 // -BattleContactCover: no cover slot serves, so look at the ground around the soldier (rings out to 12 m) for a place with a line
 // of fire to the observed threat position where less of his body is in view than here - best of all one where he is out of
 // sight crouched - and start walking to it. Uses static geometry and the observed threat position only.
 bool StartFirePosition(int32 ID,FVector Threat,int32 Target,double Now);
 // CoverSlot value of a cover action at such a free place (no slot reservation).
 static constexpr int32 FreeCoverSlot=-2;
 bool StepCoverApproach(int32 ID,float Dt,double Now,int32 Target);
 bool IssueOrder(int32 SquadID,EBattleOrder Order,FVector Goal,bool bMissionUpdate=false,bool bSector=false);
 // Player's order to one soldier (2026-11-20, BattleSoldierOrder.cpp): Ctrl+click picks a soldier of a player squad in the
 // tactical view, a right click sends him alone to a place within SoldierOrderLeash of his squad's centre. He walks his own
 // nav path (firing on the move), holds there (taking cover within 4 m when he sees his squad's target) and ignores the
 // individual AI stage B1 until the squad gets a new order, the squad moves away, or he is sent back (H).
 static constexpr float SoldierOrderLeash=4000.f;
 bool IssueSoldierOrder(int32 ID,FVector Goal,FString& Why);
 bool ReturnSoldier(int32 ID);
 void ClearSoldierOrder(int32 ID,const TCHAR* Why);
 bool SoldierOrderActive(int32 ID);
 void StepSoldierOrder(int32 ID,float Dt,double Now,FVector& Direction);
 struct FSoldierPath{TArray<FVector> Points;int32 Next=1;};
 TMap<int32,FSoldierPath> SoldierPaths; // cache only; rebuilt from the saved goal (stall time and repaths are unit state)
 void StepSoldierOrderTest();
 struct FSoldierOrderTest{int32 Stage=0,Unit=INDEX_NONE,Squad=INDEX_NONE,Failures=0;FVector Goal=FVector::ZeroVector;double Since=0,Arrived=0,ShotAt=0;int32 Shot=0;FString EndWhy;} SoldierTest;
 // Trial -BattleSectorMove=1 (2026-11-20, default 0; Play-Area turns it on for maps with sectors): the player's plain move click
 // is a sector mission (an attack move into the sector) whose squad, until a member is inside the sector, never stops to fight:
 // it keeps walking and fires on the move, also back at enemies outside the sector that shoot at it (as -BattleSectorContact,
 // for this squad only, without taking cover on the way). Set after IssueOrder by the player controller; any later explicit
 // order clears it. AI squads never have it, so matches between AIs are unchanged.
 static bool SectorMoveEnabled();
 // Trial -BattleSectorMoveFire=1 (2026-11-20, default 0): every squad on a sector mission (AI squads and the player's sector
 // attack/hold orders) fights as the player's sector move does - until a member is inside the sector it fires on the move and
 // fights back at enemies outside the sector that shoot at it, without stopping or walking to cover on the way.
 static bool SectorMoveFireEnabled();
 // -BattleArrivalWalk (2026-11-21, default 1 by user decision after s23: open hits -23%, cover shots +18%; =0 for the old rule; user 11-21: soldiers stopped short of their planned cover and fought in the
 // open): in his own sector, a soldier whose arrival place (with its own tail route) is more than 1.5 m away keeps walking to it
 // and fires on the move, instead of stopping for combat where the squad first sees an enemy. He also walks the tail of his
 // route past the end of the shared path (otherwise the route gate stops him at the shared end point). Counts arrival_walk_*.
 static bool ArrivalWalkEnabled();
 // Trial -BattleSquadCoverCheck=1 (2026-11-20, default 0; with fire on the move): the squad leader counts how many of his
 // living members could hide from the threat near the squad - free cover slots within 20 m that show at most 2 of 5 body
 // samples to the threat, and up to two places right behind each such slot that hide as well. 80% of the squad or more (user, 11-20; half before):
 // the whole squad stops and fights from cover (the -BattleSectorContact behaviour); fewer: the whole squad goes on firing.
 static bool SquadCoverCheckEnabled();
 // The squad stops only when it can also fight at least evenly: its living members and the team's other soldiers within
 // 30 m against the enemies its members see themselves (within reach). Otherwise it keeps firing on the move (user, 11-20).
 static bool BoundingEnabled();
 static bool PinnedFlankEnabled();
 bool PinnedThreat(int32 SquadID,FVector& Threat) const {const auto* P=Plans.Find(SquadID);if(!P||!P->bPinned)return false;Threat=P->PinnedAt;return true;}
 void ClearPinned(int32 SquadID){if(auto* P=Plans.Find(SquadID)){P->bPinned=false;P->PinnedSince=0;}}
 // Trial -BattleCloseIn=1 (2026-11-20, default 0; user: "too many long-range firefights - close in first"): see FBattleSquadPlan.
 static bool CloseInEnabled();
 // Trial -BattleSectorCoverWalk=1 (2026-11-20, default 0): a soldier of a sector squad who fights from inside his sector and has
 // no cover within the local search walks to a cover slot elsewhere in the sector (up to 20 m, one he can fire from - as
 // -BattleContactCover searches), else lines up beside a taken slot (-BattleCoverHuddle places), else to a nearby place where
 // less of him is in view (the fire position). Before, only the local search ran there and he stood where he was.
 static bool SectorCoverWalkEnabled();
 // Trial -BattleUnderFireCover=1 (2026-11-20, default 0): a soldier hit in the last 5 s who is not walking his path looks for
 // cover even when he cannot see his target himself (a contact reported by his squad within the last 5 s is enough) or the
 // target is beyond the 150 m engagement distance (up to his weapon's reach for 15 s after the hit). He hides from where the
 // shot came from when that is not his target. No cover here: walks to one, else beside a taken one, else a less exposed place.
 static bool UnderFireCoverEnabled();
 // Trial -BattleNoCoverFallback=1 (2026-11-20, default 0): a soldier fighting a visible target whose cover search found nothing
 // (no slot, too exposed, no peek) does not stand in the open until the next search: he moves to a nearby place where less of
 // him is in view (the fire position), else goes prone there for 5 s while he still sees his target.
 static bool NoCoverFallbackEnabled();
 // -BattleCoverPriority (2026-11-20 trial, default ON since 2026-11-21 by user decision; =0 turns it off): a soldier hit in the
 // last 5 s does not wait for the shared 2-per-frame cover-search slot (own cap of 4 per frame), and searches again 2 s (not 5 s)
 // after a failed search - also when he was already waiting after one.
 static bool CoverPriorityEnabled();
 // Trial -BattleHitReveal=1 (default 0, 2026-11-21): a hit soldier who does not see the shooter checks the line toward him at once.
 // Clear line: detection completes on the next scan. Blocked or out of sight: a remembered hint near the shot origin, its error
 // 10% of the distance (never the exact position). Counts hit_reveal_* in the sight diagnostics.
 static bool HitRevealEnabled();
 // Trial -BattleHitFace=1 (default 0, 2026-11-21): for 2 s after a hit, a soldier with no target to fire at (and not firing at a base,
 // flanking or aiming indirect fire) faces where the shot came from, so his forward field of view can find the shooter. Movement keeps
 // its own steering (only the facing changes). Counts hit_face_turned.
 static bool HitFaceEnabled();
 // -BattleSoldierWatch (default 1, user decision 2026-11-22; =0 turns it off; -BattleSoldierWatchTeam=0/1 for one team): a soldier standing still with
 // nothing to fire at (not in a cover action, not a vehicle, not under direct control) faces the squad's target when he knows
 // where it is, else the nearest enemy he sees, else the nearest he knows of (memory and reports still held; both within his
 // weapon's reach), else his share of the ground ahead (toward the enemy's home, or 70 degrees to either side by his place in the
 // squad). Not before his first perception scan (match start or a load). Sight has a 120-degree forward field of view.
 // BattleSoldierWatch.cpp; counts watch_{target,seen,contact,sector}_frames.
 static bool SoldierWatchEnabled(int32 Team);
 bool SoldierWatchDirection(int32 ID,int32 TargetID,const FVector& Pos,double Now,FVector& Out);
 int32 KnownEnemyDirection(int32 ID,int32 TargetID,const FVector& Pos,double Now,FVector& Out,double Within) const;
 // Trial -BattleWalkLook=1/2 (default 0, 2026-11-22; -BattleWalkLookTeam=0/1 for one team): a soldier on the move with nothing to
 // fire at keeps his body along his way and turns his head - only his view (Sight) changes. He looks at the enemy he knows of
 // within his sight (KnownEnemyDirection), else to his share of the ground around the squad's way by his place in the squad
 // (ahead, 50 and 100 degrees to either side). =2: the members who look ahead keep looking ahead. WalkLooks holds his view
 // from one Follow to the end of the next (so his own sight checks there use it; dropped there unless he takes the look again,
 // and on a load; not saved: after a load it comes back with his next Follow after his first scan). BattleSoldierWatch.cpp; counts walk_look_{ahead,side,target,seen,contact}_frames.
 static int32 WalkLookMode(int32 Team);
 bool WalkLookDirection(int32 ID,int32 TargetID,const FVector& Pos,const FVector& Way,double Now,FVector& Out);
 TMap<int32,FVector> WalkLooks;
 // Trial -BattleOwnTarget=1/2 (default 0, 2026-11-22; -BattleOwnTargetTeam=0/1 for one team): a soldier who cannot fire at the
 // squad's target (none, unknown to him, out of his sight or of the distance) fires at an enemy soldier he sees himself within
 // the engagement distance - the one in the direction he was just hit from first, else the nearest. =2: hit with nothing to
 // fire at, he also turns toward where the shot came from for 2 s (as -BattleHitFace). Only his aim, fire and that turn
 // change (the squad's target, movement and cover do not); nothing is saved. BattleSoldierFire.cpp; counts
 // own_target_{frames,shots,at_shooter_frames,no_squad_target_frames,squad_target_unseen_frames,hit_turn_frames}.
 static int32 OwnTargetMode(int32 Team);
 int32 OwnTarget(int32 ID,const FVector& Pos,double Now,bool& bAtShooter) const;
 void RevealShooter(int32 VictimID,int32 ShooterID,const FVector& From);
 // Evaluation only (sight diagnostics on, behaviour unchanged, 2026-11-21): a hit soldier who does not see his shooter. Why each of the
 // two lines (victim->shooter, shooter->victim) fails, using the same checks as CanSee (range, field of view, first blocking hit).
 // Counts hitlos_* (direct trace, not added to the -BattleSightHitStats counters).
 void DiagnoseHitSight(int32 VictimID,int32 ShooterID);
 const TCHAR* SightBlockReason(int32 ObserverID,int32 TargetID,double& EyeZ,double& EndZ) const;
 bool CloseInThreat(int32 SquadID,FVector& Threat) const {const auto* P=Plans.Find(SquadID);if(!P||!P->bCloseIn)return false;Threat=P->CloseInAt;return true;}
 void ClearCloseIn(int32 SquadID){if(auto* P=Plans.Find(SquadID)){P->bCloseIn=false;P->FarSince=0;}}
 // Trial -BattleArrivalThreat=1 (default 0): a sector squad picks its arrival cover places against the known enemy nearest
 // to the sector (the team's current sightings) instead of the direction it came from, and picks again when its target lies
 // more than 45 degrees away from that direction.
 static bool ArrivalThreatEnabled();
 // Trial -BattleCoverOutward=1 (2026-11-21, default 0; user idea 11-21, steps 1+2): a sector squad picks arrival cover slots with
 // an obstacle within 2.5 m toward any of the neighbouring sectors (8 directions from the sector centre) that can hold enemies,
 // instead of only straight ahead along the way in - so soldiers stand on the centre side of walls facing outward. Ignored:
 // off-map neighbours, neighbours where only the team's own soldiers are (-BattleSectorFront state 1), and neighbours toward
 // the team's home (rear, more than 45 degrees toward it) unless enemies are known there. With no direction left, the old rule.
 // Counts arrival_outward_*.
 static bool CoverOutwardEnabled();
 // Trial -BattleCoverDense=1 (2026-11-21, default 0; user idea 11-21, step 3): on maps with city cover (IsUrban), every side of an
 // obstacle longer than 3 m also gets cover slots every 1.4 m (BattleCover.cpp BuildSlots; the usual 5 samples per side stay),
 // so several soldiers can use one long wall. Sector arrival then treats a slot as taken only within 1.3 m of another
 // reservation (190 cm before) and looks at up to 512 nearby slots (256 before). The slots are shared with the cover actions.
 static bool CoverDenseEnabled();
 // Trial -BattleCoverIndoor=1 (2026-11-21, default 0; user idea 11-21, step 4): a sector squad tries its arrival cover slots under a
 // roof or upper floor (a static obstacle 1.2-8 m above the slot: inside houses and sheds, by windows) before the ones outside.
 // Counts arrival_indoor_*.
 static bool CoverIndoorEnabled();
 // -BattleAttackFocus (2026-11-22, default 1 by user decision after s25 on map I: hits in the open -16..-20%, shooter outside the
 // view -16..-40%; =0 for the old rules; user idea 11-22: an attack order toward an enemy squad or sector, and cover
 // against that direction - stage 1+2, the sector AI picks the focus itself): a sector squad keeps a focus point, the enemies its
 // team sees now (RouteKnownEnemies: own sightings and scout reports, no hidden state) in the destination sector and its 8
 // neighbours - the sector with the most of them (ties: nearer to the destination), their mean position; else the known enemy
 // nearest to the destination within 200 m; else none (the old rules). Re-picked every 3 s. Its arrival cover slots need an
 // obstacle within 2.5 m toward the focus or 30 degrees either side (instead of straight ahead along the way in, or the
 // -BattleArrivalThreat direction, whose re-facing it replaces), a still soldier with nothing to fire at watches the focus
 // (members fanned 0/-20/+20/-40/+40 degrees), and in its sector the squad re-plans its arrival places when the focus turns more
 // than 45 degrees from the direction they were chosen against (or appears after they were chosen without one; at most every
 // 10 s). Counts attack_focus_*, arrival_focus_facing, focus_facing_frames.
 static bool AttackFocusEnabled();
 // Trial -BattleFocusTarget=1 (2026-11-22, default 0; with -BattleAttackFocus): the squad's target choice counts a visible enemy
 // more than 45 degrees off the focus (seen from the squad centre) as 1.5 times as far, so the squad fires toward its focus first
 // and still answers a much nearer enemy elsewhere. Counts focus_target_in_cone / focus_target_off_cone.
 static bool FocusTargetEnabled();
 // -BattleSectorSymmetric (2026-11-22, default 1 since 11-22 12:00 by user decision, =0 the old way; west/east bias of the sector AI on map I, STATUS 11-22 5:00): the sector
 // AI's choices that scan the 3x3 sectors around a squad in a fixed compass order and keep the first of equal scores (the stop
 // beside the next junction, the pinned squad's flank stop, the close-in stop - BattleSectorRoute.cpp), and the arrival places
 // sorted with ties by world X then Y (BuildMovementPlan, ReassignOccupiedArrival), read them in the team's own frame: the
 // east team (AreaHome.X > 0) turned 180 degrees, so on the point-symmetric maps both teams break ties alike.
 static bool SectorSymmetricEnabled();
 // -BattleHireMirror (2026-11-22; default 1 since the user decision that day, =0 the earlier rows): in an area battle purchased squads appear at home rows point-mirrored for the
 // east team (rows mirrored in Y as well as X), so both teams' new squads start at the same place in their own frames.
 static bool HireMirrorEnabled();
 // -BattleSectorSymmetric: -1 for the east team (its frame turned 180 degrees), else 1.
 int32 SectorTurn(int32 Team) const;
 // -BattleSectorSymmetric: the sector of a point as the team reads it - for the east team the sector of the point turned 180
 // degrees, turned back (a point on a sector border then falls on the same side for both teams). Else BattleSectors::Cell.
 FIntPoint SectorCellFor(int32 Team,const FVector& At) const;
 // -BattleAttackFocus: each team's current sightings (RouteKnownEnemies), refreshed at most once a second.
 TArray<FVector> FocusEnemies[2];double FocusEnemiesAt[2]={-1.,-1.};
 bool PickAttackFocus(const struct FBattleSquadState& S,double Now,FVector& Out,const TCHAR*& Source);
 // Trial -BattleCoverStrict=1 (2026-11-20, default 0; user: "cover that does not hide from the enemy, standing in the open"):
 // a cover place counts only when the threat sees at most 2 of the 5 body samples (was 3) - for the cover actions, for the
 // arrival places of a sector squad and its places along the wall (checked against the known enemy nearest to the sector), and
 // a soldier of a sector squad that finds no cover does not stand and fight in the open: he walks to a nearby place where
 // less of him is in view (the -BattleContactCover fire position).
 static bool CoverStrictEnabled();
 // Trial -BattleShotDirection=1 (2026-11-20, default 0): where a soldier was last hit from (the shooter's position, kept 5 s).
 // Narrowed after the first comparison (more hits in the open, fewer shots from cover): it is used only when the shooter is
 // someone other than the soldier's target (more than 10 m from it and more than 30 degrees off its direction) - then his
 // cover search faces the shooter, and the squad's cover count faces the newest such shooter of its members. A soldier is
 // never moved by a squad-mate's hit, and the arrival places of a sector squad are not re-planned on a hit (a soldier already
 // in cover keeps it; a soldier in cover never searches again anyway). Otherwise: the enemy in view, as before.
 static bool ShotDirectionEnabled();
 TMap<int32,TPair<FVector,double>> ShotFrom;
 bool HitWithin(int32 ID,double Now,double Seconds) const {const auto* F=ShotFrom.Find(ID);return F&&Now-F->Value<=Seconds;}
 bool UnitShotFrom(int32 ID,double Now,FVector& Out,double Seconds=5.) const {const auto* F=ShotFrom.Find(ID);if(!F||Now-F->Value>Seconds)return false;Out=F->Key;return true;}
 bool SquadShotFrom(int32 SquadID,double Now,FVector& Out) const;
 static bool ShotFromOther(const FVector& Me,const FVector& Target,const FVector& From)
 {return FVector::DistSquared2D(From,Target)>FMath::Square(1000.) && FVector::DotProduct((From-Me).GetSafeNormal2D(),(Target-Me).GetSafeNormal2D())<0.866f;}
 static float CoverExposureLimit(){return CoverStrictEnabled()?.41f:.6f;}
 // Trial -BattleThreatCover=1 (2026-11-20, default 0; individual AI stage A, BattleThreatCover.cpp; -BattleThreatCoverTeam=0/1
 // limits it to one team): a soldier keeps the enemies he knows himself (his own sightings of the last 4 s within reach, and
 // where he was last hit from) as his threats. A cover place must hide him from all of them (the target first, up to 2 more),
 // and once a second (deterministic, staggered by unit) a soldier hidden in cover checks that it still does; when a threat
 // sees too much of him (or the place is flanked) he leaves it and searches again at once, never taking the same place back
 // within 10 s. When no place hides him from everyone he takes the best one against his target, as before.
 static bool ThreatCoverEnabled(int32 Team);
 static int32 ThreatCoverMode(int32 Team); // -BattleThreatCover=2: narrowed threats, leave after two breaks in a row
 int32 GatherThreats(int32 ID,FVector Primary,double Now,TArray<FVector,TInlineAllocator<3>>& Out) const;
 bool ExposedToThreats(int32 ID,FVector Anchor,const TArray<FVector,TInlineAllocator<3>>& Threats) const;
 // Trial -BattleThreatCoverCrouch=1 (2026-11-21, default 0; with stances and -BattleThreatCover): a stage A soldier's cover
 // places are judged with the crouched body's points (breaks, threats that see in, choosing places). Map I runs: 33-52% of
 // confirmed breaks were hidden when crouched.
 static bool ThreatCoverCrouch(int32 Team); // -BattleThreatCoverCrouchTeam=0/1: one team only
 float ThreatSeen(int32 ID,FVector Anchor,FVector Threat) const; // fraction of the body a threat sees at the place
 bool CheckCoverThreats(int32 ID,FVector Primary,double Now,float Dt);
 // Trial -BattleThreatCoverRelocate (2026-11-21, default 0; with -BattleThreatCover): before leaving a broken place he looks for
 // the next one from where he is (12 m, inside the squad's sector and 30 m of its centre, not a place someone walks to;
 // hidden from every threat when another one sees in) and walks there; the walk is not ended halfway by a stale contact.
 // 1 = none found: leave as before. 2 = none found: stay (keeping down while another threat sees in) and search again at
 // each confirmed break. Searches: 2 soldiers a frame (B1 has its own 4). 3 (2026-11-21) = none found: stay, but instead of
 // keeping down he fights from where he is: the nearest enemy he sees that sees into the place and that he can fire at from it
 // becomes his target (or stays it), and he peeks from the place itself (it is seen into anyway); with none such he keeps his
 // usual peek. He searches again at each confirmed break, as with 2.
 static int32 ThreatCoverRelocate();
 int32 ExposingEnemy(int32 ID,FVector Anchor,double Now) const; // BattleThreatCover.cpp
 uint64 RelocateFrame=0;int32 RelocatesThisFrame=0;
 // Soldiers who found the search budget full, with whether other threats saw in: on the next frame they search again with
 // the break already confirmed (no new check). Not saved: after a load they wait for their own phase, as before.
 TMap<int32,bool> RelocateWaiting;
 bool BrokenCoverSlot(int32 ID,int32 Slot,double Now) const; // BattleThreatCover.cpp
 void MarkBrokenCover(int32 ID,int32 Slot,double Until);
 TMap<int32,FBattleBrokenCover> BrokenCover;
 // Trial -BattleSoldierMove=1 (2026-11-20, default 0; individual AI stage B1, BattleSoldierMove.cpp; -BattleSoldierMoveTeam=0/1
 // limits it to one team): an AI infantryman hidden in cover during a sector mission rates his place once a second (staggered
 // by unit, at most 4 soldiers a frame) against up to 8 free cover places within 25 m in the squad's sector and 30 m of its
 // centre, and walks to the best one when it is clearly better (hidden from the target and his other threats, a firing point,
 // within engagement range, little exposure on the way, short walk, not crowded). At most half of a squad relocates at once,
 // only while a squadmate is in cover, and not again within 8 s. The squad still chooses sectors and arrival places.
 static bool SoldierMoveEnabled(int32 Team);
 float RateCoverPlace(int32 ID,FVector From,FVector Anchor,FVector Peek,bool bFire,const TArray<FVector,TInlineAllocator<3>>& Threats) const;
 bool StepSoldierMove(int32 ID,FVector Primary,double Now,float Dt);
 uint64 SoldierMoveFrame=0;int32 SoldierMovesThisFrame=0;
 // -BattleSoldierAIFocus=<m>: stage A and B1 only near the player (or -BattleSoldierAIFocusAt=X,Y in metres), both teams.
 bool SoldierAIFocus(int32 ID) const;
 mutable uint64 FocusFrame=0;mutable FVector FocusPoint=FVector::ZeroVector;mutable bool FocusValid=false;
 // Several squads of a team in one place share the places: slots reserved by anyone and places that soldiers are already
 // walking to do not count, and the soldiers of other stopping squads near by who still look for cover are counted against it.
 bool SquadCanHide(int32 SquadID,FVector Threat,double Now) const;
 // A member of a squad stopping in cover (bCoverStop) that found no slot of its own walks to a place right behind a cover slot
 // that hides him from the threat (1.3 m back, or 1.1 m to a side of that), not taken by another soldier of any squad.
 bool StartHuddle(int32 ID,FVector Threat,int32 Target,double Now);
 void SetSectorFireBack(int32 SquadID){SectorFireBack.Add(SquadID);}
 bool HasSectorFireBack(int32 SquadID) const {return SectorFireBack.Contains(SquadID);}
 TSet<int32> SectorFireBack;
 bool CanSelectLeader(int32 ID) const;
 bool CanControlLeader(int32 ID) const;
 const TArray<FBattleSquadSummary>& Summaries();
 // Current squad target (a contact the squad engages), or INDEX_NONE. For the command UI squad list.
 int32 SquadTargetID(int32 SquadID) const {const auto* P=Plans.Find(SquadID);return P?P->TargetID:INDEX_NONE;}
 // Squad plan (arrival goals per member) for the command UI ground marks. Read only.
 const FBattleLeaderPlan* PlanOf(int32 SquadID) const {return Plans.Find(SquadID);}
 void OnDeath(int32 ID);
 void RunSmoke();
 void RunUIQA();
 void RunWideSmoke();
 void RunWideSoak();
 void RunST2Smoke();
 void RunSupportSmoke();
 void RunFlankSmoke();
 void RunFlankBayTrial();
 double BayStart=0, BayArrival=-1, BayMovingDamage=0, BayLastHP=0;
 int32 BayStage=0;
 double BayDamageGiven[2]={0,0}, BayMoveDirectDamage=0;
 FVector FlankSmokeGoal=FVector::ZeroVector;
 void RunMovementTrial();
 void RunContactTrial();
 void RunSightTrial();
 void RunWeaponRangeTrial();
 void RunWideMapAudit();
 // Map statistics (-BattleMapStats=<json>, BattleMapStats.cpp): sight lengths, clearance, cover and routes of the map as built.
 void RunMapStats();
 void RunSightVisual();
 int32 SightVisualStage=0;
 // Stance visuals check (-BattleStanceShot, BattleStanceShot.cpp): close-up screenshots of a leader and a follower.
 void RunStanceShot();
 void RunExoShot();
 int32 ExoShotStage=0,ExoShotIDs[2]={0,0};
 void RunDeathShot(); // BattleDeathShot.cpp
 int32 DeathShotStage=0,DeathShotIDs[2]={0,0};
 // Aim / scope view check (-BattleScopeShot, BattleScopeShot.cpp): leader 1 under direct control in each view.
 void RunScopeShot();
 int32 StanceShotStage=0,StanceShotFollower=0;
 void StepSightExperiment(float DeltaSeconds);
 FBattleSightExperiment SightExperiment;
 int32 MapAuditStage=0;
 double MapAuditStart=0;
 int32 SightTrialCase=0, SightTrialTick=0;
 int32 SightTrialAmmo=0;
 FVector SightTrialStart=FVector::ZeroVector;
 FString MovementLabel(int32 SquadID) const;
 class ABattleVATDisplay* VATDisplay() const {return VAT;}
private:
 friend class UBattlePhase3;
 friend struct FBattleMixedUnit;
 bool bEnabled=false, bSummaryDirty=true, bReduceHiddenBones=false, bReduceAnimTasks=false;
 bool bWideField=false;
 bool bDisplayLODEnabled=true;
 int32 RestoreStageFailureAfter=INDEX_NONE; // Exercised only by BattleMixedSaveSmoke / BattleAreaSmoke.
 bool bRangeAwareCombat=true; // BattleRangeAware=0 retains the legacy trial baseline.
 bool bStableMovement=true, bMovementTrial=false, bContactTrial=false, bContactAvoidance=true;
 bool ContactBlockersHeld=true, ContactWaitObserved=false, ContactReleased=false;
 TMap<int32,FVector> ContactBlockerStarts;
 int32 MoveTrialScene=0, MoveTrialStage=0;
 int32 MoveTrialOnly=-1;
 double MoveTrialStart=0, MoveTrialStall=0;
 double MoveTrialMaxDeviation=0;
 double MoveTrialMinSpacing=DBL_MAX;
 TArray<float> MoveTrialFrameMS;
 TArray<int32> MoveTrialIDs;
 TMap<int32,FVector> MoveTrialStarts, MoveTrialGoals;
 TMap<int32,double> MoveTrialArrivals;
 TArray<TWeakObjectPtr<AActor>> MoveTrialWalls;
 int32 WideStage=0;
 FVector WideStart=FVector::ZeroVector;
 UPROPERTY() TObjectPtr<class UBattlePhase3> Phase3;
 UPROPERTY() TObjectPtr<class ABattleVATDisplay> VAT;
 int32 SmokeFailures=0, SmokeStage=0, QAStage=0; FVector SmokeSavedPeek=FVector::ZeroVector;
 // Body display checks of the cover smoke (muzzle along the aim while strafing; hit reaction recovery).
 float SmokeMuzzleMax=0.f, SmokeHead=0.f; FVector SmokeDir=FVector::ZeroVector; int32 SmokeMuzzleFrames=0, SmokeShots=0; FQuat SmokeChest=FQuat::Identity; double SmokeHit=0.; const class UAnimSequence* SmokeHitClip=nullptr;
 FString QAPrefix;
 FVector SmokePosition=FVector::ZeroVector;
 TSharedPtr<FMassEntityManager> Manager;
 TMap<int32,FBattleLeaderBody> Bodies;
 TMap<int32,FBattleLeaderPlan> Plans;
 bool ObservedAvoidancePosition(int32 ObserverID,int32 OtherID,FVector& OutPosition) const;
 void BuildObservedAvoidance(int32 ObserverID,TMap<FIntPoint,int32>& OutGrid,TArray<TArray<int32>>& OutBuckets,TMap<int32,FVector>& OutPositions) const;
 TMap<FIntPoint,int32> AvoidanceGrid;
 TArray<TArray<int32>> AvoidanceBuckets;
 TArray<int32> AvoidanceUsed;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,int32> HitIDs;
 TArray<FBattleSquadSummary> CachedSummaries;
 UPROPERTY() TObjectPtr<AActor> DisplayHost;
 UPROPERTY() TObjectPtr<class UAnimSequence> IdlePose;
 UPROPERTY() TObjectPtr<class UAnimSequence> WalkPose;
 UPROPERTY() TObjectPtr<class UAnimSequence> ReloadPose;
 UPROPERTY() TObjectPtr<ABattleGameMode> Mode;
 FDelegateHandle DeathHandle;
 void Think(int32 SquadID,double Now);
 void Follow(int32 ID,float Dt,double Now);
 static bool FollowerOverlapsOff();

 void BuildMovementPlan(int32 SquadID);
 bool StableMove(int32 ID,float Dt,double Now,FVector& Direction);
 bool IsStationaryAlly(int32 Observer,int32 Other) const;
 void BeginAllyPass(int32 ID,double Now);
 void EndAllyPass(int32 ID);
 void ReassignOccupiedArrival(int32 ID,double Now);
 bool ResolveStoppedContact(int32 ID,FVector Way,double Now,FVector& Direction,float& Scale);
 bool ClearMovementSegment(FVector A,FVector B) const;
 // Map with heights (BattleTerrain.h): gives the step of a hand-moved soldier the height change of the ground; false = the
 // ground there cannot be stepped onto from here. Flat maps: true, the step is untouched.
 bool TerrainStep(const FVector& From,FVector& Step) const;
};
