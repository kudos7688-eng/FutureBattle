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
#include "BattleLeaderRuntime.generated.h"

class ABattleGameMode;
class UBattleSave;
struct FBattleSavedUnit;
class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

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

// Shared by Canvas and pointer input; no stale hit rectangles from the previous draw.
struct FBattleSquadListLayout
{
 FVector2D Origin;
 float Width=360, Height=0, RowHeight=56, Header=44;
 int32 VisibleRows=0;
 static FBattleSquadListLayout Make(FVector2D View);
 // Header controls (command UI 2026-11-16): five filter tabs and a sort button on the second header line.
 static constexpr int32 FilterCount=5; static constexpr float TabWidth=50, TabTop=23, TabHeight=18, SortWidth=86;
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
 double ExoNextPick=0;int64 ExoLauncherShots=0,ExoLauncherHits=0;
 // Look: armour parts on a rig that follows the chest and keeps the body's heading, the launcher tube, and a shield bubble shown while shield points are left.
 // bSuit: the AC01 suit (ApplyExoSuit: own body, clips and weapon models) instead of the placeholder plates.
 struct FExoLook{TWeakObjectPtr<USceneComponent> Rig;TWeakObjectPtr<class UStaticMeshComponent> Bubble;TWeakObjectPtr<class UMaterialInstanceDynamic> BubblePaint;TWeakObjectPtr<class UStaticMeshComponent> Stowed;FTransform StowUpright,StowProne;bool bSuit=false,bShowingLauncher=false,bStowProne=false;double AILauncherUntil=0.;};
 bool ApplyExoSuit(int32 ID);
 TMap<int32,FExoLook> ExoLooks;
 void UpdateExoLooks();
 void RefreshVehicles();
 float VehicleSpotRange() const;
 // Damage scale of a weapon on a unit: anti-tank hits fully; small arms by the vehicle type (tank none, armoured car ACArmorSmallArms).
 float ArmorScale(int32 VictimID,EBattleWeapon Weapon) const;
 // Turns a vehicle towards Direction at its type's turn rate; it drives only roughly forwards (Direction becomes its heading, Scale drops while turning).
 void SteerVehicle(int32 ID,FVector& Direction,float& Scale);
 void AimTurret(int32 ID,const FVector& Direction);
 // Armour AI, stage 3 (BattleArmourAI.cpp). PickArmourTargets: weapon-aware squad targets while the squad sees a vehicle.
 // FaceVehicle: a stopped vehicle turns its hull (its forward view) at its turn rate. VehicleWatchDirection: towards the nearest
 // enemy its team knows of, else the enemy home.
 void PickArmourTargets(int32 SquadID,struct FBattleLeaderPlan& Plan);
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
 // Runtime only, not saved: a restored unit is not set up and its gun is loaded; shells in flight are dropped on restore.
 struct FArtillery{double StillSince=-1,ReadyAt=0,NextPick=0;int32 Shots=0;FVector Aim=FVector::ZeroVector;bool bHasTarget=false,bBase=false;};
 struct FArtilleryShell{int32 ShooterID=INDEX_NONE,Team=0;uint8 ShooterType=0;bool bSPG=false;FVector At=FVector::ZeroVector;float Remaining=0;};
 TMap<int32,FArtillery> Artillery;
 TArray<FArtilleryShell> ArtilleryShells;
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
 bool PickArtilleryAim(int32 ID,const TMap<int32,FVector>& Known,FVector& Aim,bool& bBase,bool& bFriendlyHeld);
 bool AllyNear(int32 Team,const FVector& At,double Radius) const;
 // Launches one shell at Aim (spread applied, flight time Distance / shell speed). Consumes mortar ammunition or loads the gun.
 bool FireArtillery(int32 ID,const FVector& Aim,double Now);
 void StepArtilleryShells(float Dt);
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
 // Trial -BattleSightTeamMask=1 (rules, default OFF; user request 2026-11-17): soldiers of one side do not block their own side's line of
 // sight. Each soldier body carries the mask bit 1<<team (FBodyInstance mask filter) and sight queries ignore the observer's bit
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
 mutable int64 FollowSightTraced=0,FollowSightUsed=0; // evaluation counters (BATTLE_FOLLOW_SIGHT)
 bool Fire(int32 ID,FVector Start,FVector Direction,bool bPlayerShot);
 FVector TracerStart(int32 ID,const FVector& Start,const FVector& Direction) const;
 bool RequestCoverAction(int32 ID,FVector Threat,int32 Target=INDEX_NONE,float SearchRadius=-1.f);
 void CancelCoverAction(int32 ID);
 bool StepCoverAction(int32 ID,float Dt,double Now);
 void RunCoverActionSmoke();
 void RunSectorSmoke();
 double NextCoverProbe=0;
 uint64 CoverProbeFrame=0;int32 CoverProbesThisFrame=0;
 TMap<int32,double> CoverProbeAfter;
 // Trial -BattleCoverApproach: runtime-only walk toward a hidden cover slot (not saved; cleared on restore).
 struct FCoverApproach{FVector Goal=FVector::ZeroVector;int32 Slot=INDEX_NONE,Target=INDEX_NONE,Revision=0;double Until=0;bool bKeptTargetChange=false,bFree=false,bStandUp=false;};
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
 struct FBodyTimer{double At=0;bool bFreeze=false;};
 TMap<int32,FBodyTimer> BodyTimers;
 void StepBodyTimers();
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
 uint8 StanceOf(int32 ID) const {const uint8* Found=Stances.Find(ID);return Found?*Found:0;}
 bool SetStance(int32 ID,uint8 Stance,double Now,bool bForce=false);
 // Follower clip for pose 0 idle / 1 walk / 2 reload; with -BattleStanceAnim=1 a crouched follower gets the crouch clip.
 class UAnimSequence* FollowerClip(int32 ID,int32 Pose) const;
 void RefreshFollowerPose(int32 ID);
 void UpdateAIStance(int32 ID,double Now,bool bMoving,bool bCombat,bool bLostSight);
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
 int32 RestoreStageFailureAfter=INDEX_NONE; // Exercised only by BattleMixedSaveSmoke.
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
 FString QAPrefix;
 FVector SmokePosition=FVector::ZeroVector;
 TSharedPtr<FMassEntityManager> Manager;
 TMap<int32,FBattleLeaderBody> Bodies;
 TMap<int32,FBattleLeaderPlan> Plans;
 TMap<FIntPoint,TArray<int32>> AvoidanceGrid;
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
 // Trial -BattleFollowSightParallel: sight tests of the Follow loop traced in parallel before it (soldier -> target, visible).
 void PrefetchFollowSight();
 static bool FollowerOverlapsOff();
 bool FollowCanSee(int32 ID,int32 Target) const;
 TMap<int32,TPair<int32,bool>> FollowSight;

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
