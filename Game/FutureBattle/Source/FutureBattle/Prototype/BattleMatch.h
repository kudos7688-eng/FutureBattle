#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleState.h"
#include "BattleAreaState.h"
#include "BattleMatch.generated.h"
class UBattleSave;
// Area strategy layer (ROADMAP 2026-11-16). The team intent is all the lower layers read: point allocation (S1 with S4/S6) and
// AI purchasing. Stage 1: a fixed intent built from the tuning, so behaviour is unchanged.
struct FAreaIntent
{
 float AttackWeight=2.f,BaseShare=0.2f; bool bFrontBalance=false; float FrontGain=1.f,FocusGain=0.f;
 static constexpr int32 MaxObjectives=12; // three objectives on maps A-D, five on map E, nine on map F
 float PointWeight[MaxObjectives]={1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f}; // per objective index, multiplies the S1 weight
 int32 HireReserve=0;                 // resources kept back before an AI purchase (above pending replacements)
 float FlankOffset=1.3f; bool bFlankSameSide=true; // S7/S8 route of home-assault squads (0 = straight; FS13BR5 default since 2026-11-17)
 int32 BaseReturnLiving=5;            // S8: living count at which home-assault squads return for supply (0 = SupplyReturnLiving; 5 by default since 2026-11-17)
 int32 Phase=0;                       // index of the active phase of the team plan (0 when no plan)
 float HeavyShare=0.f;                // share of AI purchases that are heavy infantry (0 = rifle squads only)
 int32 TankKeep=0;                    // tanks the AI keeps active (same pacing as scouts; 0 = none)
 int32 ACKeep=0;                      // armoured cars the AI keeps active (same pacing as scouts; 0 = none)
 int32 MortarKeep=0;                  // mortar teams the AI keeps active (same pacing as scouts; 0 = none)
 int32 SPGKeep=0;                     // self-propelled guns the AI keeps active (same pacing as scouts; 0 = none)
 int32 ExoKeep=0;                     // armoured exoskeleton soldiers the AI keeps active (same pacing as scouts; 0 = none)
 int32 ATKeep=0;                      // anti-tank teams the AI keeps active (same pacing as scouts; 0 = none)
 int32 ScoutKeep=0;                   // scout pairs the AI keeps active (one more per ScoutAfterHires purchases, up to this; 0 = none)
};
// One sector (the part of the field nearest one objective; a lane on maps A-D) as seen by one team: own soldiers and own front (furthest progress
// from own home, 0..1 along the home-to-home line, 90th percentile), and enemies currently visible to the team (count and the
// progress of the most advanced one, -1 = none seen). Observed information only.
struct FAreaFront { float OwnFront=-1.f,EnemyFront=-1.f; int32 Own=0,EnemySeen=0; };

enum class EBattleMatchPhase : uint8 { Disabled, Deployment, Combat, Finished };

USTRUCT()
struct FBattleObjective
{
	GENERATED_BODY()
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() int32 Owner = INDEX_NONE;
	UPROPERTY() int32 CapturingTeam = INDEX_NONE;
	UPROPERTY() float CaptureProgress = 0.f;
	UPROPERTY() bool bContested = false;
	UPROPERTY() float IncomeClock = 0.f;
};

USTRUCT()
struct FBattleStrike
{
	GENERATED_BODY()
	UPROPERTY() int32 Team = 0;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() float Remaining = 3.f;
};

// Match economy is independent of actors and possession. All purchases pass through this store.
// Player route choice (BattlePlayerRoutes.cpp): the main route a player's squad takes on maps with a road network.
struct FPlayerRoute
{
	int32 Lane = 0;                          // 0 automatic, 1 centre, 2 north, 3 south
	int32 Phase = 0;                         // 0 to the route, 1 along it, 2 from it to the goal
	FVector PhaseGoal = FVector(FLT_MAX);    // the goal the phase belongs to
	bool bMission = false;                   // a routed manual order under way: Order and Goal are the player's, Step the junction last ordered
	EBattleOrder Order = EBattleOrder::Move;
	FVector Goal = FVector::ZeroVector, Step = FVector::ZeroVector;
	// A way drawn by the player (BattlePlayerPath.cpp): the points still ahead of Step, and when Step is given up as unreachable.
	bool bPath = false; TArray<FVector> Waypoints; float StepDeadline = 0.f;
};
inline const TCHAR* BattleRouteLaneName(int32 Lane)
{
	return Lane == 1 ? TEXT("\u4e2d\u592e") : Lane == 2 ? TEXT("\u5317") : Lane == 3 ? TEXT("\u5357") : TEXT("\u81ea\u52d5");
}

UCLASS()
class UBattleMatch : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	EBattleMatchPhase Phase = EBattleMatchPhase::Disabled;
	FGuid MatchID;
	int32 Credits[2] = { 0, 0 };
	int32 Winner = INDEX_NONE;
	UPROPERTY() TArray<FBattleObjective> Objectives;
	float Score[2] = { 0.f, 0.f };
	float Elapsed = 0.f;
	float DirectorClock = 0.f, SupplyClock = 0.f;
	UPROPERTY() TArray<FBattleStrike> Strikes;
	float SupportCooldown[2] = { 0, 0 };
	int32 SupportUsed[2] = { 0, 0 };
	FString LastMessage;
 bool bArea=false;
 UPROPERTY() FBattleAreaState Area;
 UPROPERTY() TArray<TObjectPtr<AActor>> BaseActors;
 void InitializeArea();
 void UpdateArea(float Dt);
 bool AreaVisible(int32 Observer,FVector Point,AActor* Target=nullptr) const;
 bool DamageBase(AActor* Target,int32 AttackerTeam,float Damage);
 bool CanFireAtBase(int32 UnitID,FVector& Target) const;
 FVector AreaHome(int32 Team) const;
 int32 ObservedOwner(int32 Team,int32 Point) const;
 int32 AreaScore(int32 Team) const;
 // War score trial (BattleWarScore.h). Earned = points + enemy losses; Score adds unspent resources; Estimate is what Observer
 // can know of the opponent's Earned (own losses exactly, points from observed ownership only).
 int32 WarEarned(int32 Team) const;
 int32 WarScore(int32 Team) const;
 int32 WarScoreEstimate(int32 Observer) const;
 void RecordWarLoss(int32 LostTeam);
 void UpdateWarScore(float Dt);
 FString WarScoreBreakdown(int32 Team) const;
 // Area new squad (regular only) into a free slot (<25 active squads) for BattleTactics::AreaHire().Cost resources.
 // bAuto: AI purchase (no selection change; a team-0 squad is delegated). Cancels pending replenishments the spend can no longer cover.
 bool HireAreaSquad(int32 Team,EBattleTraining Training,bool bAuto,uint8 SquadType=0);
 int32 LastHiredSquadID=INDEX_NONE;
 // P1 trial: the observed need a new AI squad would serve (see FAreaHireTuning), or -1 for none.
 int32 AreaHireNeed(int32 Team) const;
 // AI purchase rule, every director tick: own resources minus own pending replenishments >= cost + reserve, and a free slot.
 // Team 1 always; team 0 only in the all-AI sight experiment.
 void UpdateAreaHiring();
 int32 ActiveSquads(int32 Team) const;
 // Active scout pairs of a team (at most UBattleTuning::AreaScoutLimit).
 int32 ActiveScouts(int32 Team) const;
 int32 ActiveOfType(int32 Team,uint8 SquadType) const;
 float MatchTimeLimit() const;
 // Smoke fixtures only (not saved): suspend Area income so exact resource checks stay deterministic.
 bool bAreaIncomeHeld=false;
	void InitializeMatch();
	static int32 SquadPrice(EBattleTraining Training);
 int32 CurrentSquadPrice(EBattleTraining Training) const;
	bool HireSquad(int32 Team, EBattleTraining Training);
	bool StartCombat();
	void Finish(int32 WinningTeam, const FString& Reason);
	void Update(float DeltaSeconds);
	void UpdateObjectives(float DeltaSeconds);
	void AdvanceObjectiveControl(float DeltaSeconds, TFunctionRef<int32(const FBattleObjective&)> Presence);
	static bool InObjective(FVector Position, const FBattleObjective& Point);
	int32 ChooseObjective(int32 Group, int32 Team) const;
	// S1 allocation for one squad from observed point ownership only (Area). bBase = assault the enemy home instead.
	int32 AreaStrategyTarget(int32 SquadID, int32 Team, bool& bBase) const;
	FAreaIntent AreaIntent(int32 Team) const;
	// Heavy infantry objective: the observed own point with the fewest own soldiers within 100 m (hold the thin front), or,
	// with no own point, the point nearest the own home. Observed ownership, own positions and fixed positions only.
	int32 AreaHeavyTarget(int32 Team) const;
	// Observation post of a scout pair: ahead of an own front point (observed ownership and static positions only).
	FVector AreaScoutPost(int32 Team, int32 SquadID) const;
	// Indirect-fire squads (mortar team, self-propelled gun; BattleArtillery.cpp): their assigned goal, pulled back towards the own home
	// until it is ArtilleryStandoff x their range from every enemy the team knows of (known positions only).
	FVector AreaArtilleryPost(int32 Team, int32 SquadID, const FVector& Goal) const;
	// Armoured car objective (stage 2, BattleVehicleTactics.cpp): outside the city of map E, away from known enemy anti-tank soldiers and
	// tanks, preferring points the team does not hold, points with known enemies and near ones; kept ACChoiceHold s unless it turns unsafe.
	// INDEX_NONE when no objective qualifies (the caller keeps its default). Known enemies (AreaIntelView) and observed ownership only.
	int32 AreaCarTarget(int32 Team, int32 SquadID) const;
	// True when an enemy the team knows of and can tell apart as an anti-tank soldier or a tank threatens a vehicle at P.
	bool AreaArmourThreat(int32 Team, const FVector& P, bool bTankGuns = true) const; // bTankGuns false: anti-tank soldiers only
	// Armour AI, stage 3 (BattleArmourAI.cpp; -BattleArmourAI, default 1). Known enemies and own state only.
	// Enemy vehicles the team has known of within the last VehicleMemory seconds (seen units; transient, not saved).
	struct FKnownVehicle { float At = -1.f; FVector Position = FVector::ZeroVector; };
	mutable TMap<int32, FKnownVehicle> KnownVehicles[2];
	void NoteKnownVehicles(int32 Team) const;
	int32 KnownEnemyVehicles(int32 Team, TArray<FVector>* Positions = nullptr) const;
	// Squad reaction to armour: 1 = a vehicle backs away to Goal (a known anti-tank soldier within reach, or DamageRatio of its health
	// lost in 5 s), 2 = infantry with no anti-tank weapon falls back from a tank it sees, 0 = none.
	int32 AreaArmourReaction(int32 Team, int32 SquadID, float DamageRatio, FVector& Goal, FString& Why) const;
	// Tank objective (the point with the most known enemies, outside the city of map E) and its overwatch post short of that point.
	int32 AreaTankTarget(int32 Team, int32 SquadID) const;
	FVector AreaTankPost(int32 Team, int32 Point, float Overwatch = -1.f) const; // Overwatch below zero: UBattleTuning::TankOverwatch
	// Squad groups, stage 1 (BattleGroups.cpp): members, one objective, a phase (0 assemble at Rally, 1 advance, 2 hold). Not saved.
	// Stage 2: roles while advancing (0 assault: to the point; 1 support: fires from a post short of it; 2 armour: the tank's post behind
	// the infantry; 3 fire: mortar or gun at its firing post; 4 guard: the anti-tank team beside the support post; 255 none: the squad
	// keeps its own type's rules). A point the group could not take is avoided until AvoidUntil.
	struct FBattleGroup { int32 ID = 0, Team = 0; TArray<int32> Squads; int32 Objective = INDEX_NONE; uint8 Phase = 0; float PhaseAt = 0.f; FVector Rally = FVector::ZeroVector; bool bPlayer = false; TMap<int32, uint8> Roles; int32 Avoid = INDEX_NONE; float AvoidUntil = 0.f;
		// Stage 3b: the living soldiers when the advance began (losses end it early) and how often in a row the group failed at StallPoint.
		int32 StartLiving = 0, StallPoint = INDEX_NONE, Stalls = 0;
		bool bOpened = false; /* the opening rush was decided (once per group) */
		bool bArrived = false; /* this advance has reached its point: the stall clock (PhaseAt) runs from then */
		float ArrivedAt = 0.f; int32 Extensions = 0; /* stage 3d: when it arrived, and how often the stall clock was extended while pushing */ };
	uint8 GroupRole(const FBattleGroup& Group, int32 SquadID) const;
	void AssignGroupRoles(FBattleGroup& Group) const;
	FVector GroupSupportPost(int32 Team, int32 Point) const;
	TMap<int32, FBattleGroup> Groups;
	TMap<int32, int32> SquadGroup; // squad -> group
	int32 NextGroupID = 1;
	int32 GroupOf(int32 SquadID) const; // INDEX_NONE when the squad is in no group
	// A new group of active squads of one team (they leave their old groups). bPlayer: formed by the player (may hold one squad).
	int32 FormGroup(int32 Team, const TArray<int32>& Squads, bool bPlayer);
	void DissolveGroup(int32 GroupID, const TCHAR* Why);
	bool GroupAIRun(const FBattleGroup& Group) const;
	int32 GroupLiving(const FBattleGroup& Group, int32* Full = nullptr) const;
	int32 GroupFocus(int32 Team, const FVector& From, int32 Avoid = INDEX_NONE) const;
	FVector GroupRally(int32 Team, int32 Point) const;
	// Every director update, before the team's orders: members, AI formation (trial -BattleAreaGroups=1), phases.
	void UpdateGroups(int32 Team);
	// The order of a squad in a group the AI runs (false: the squad keeps the normal allocation).
	bool GroupOrder(int32 SquadID, int32 Team, FVector& Center, EBattleOrder& Order) const;
	// Anti-tank team objective: the point nearest a known enemy vehicle; INDEX_NONE while the team knows of none.
	int32 AreaATTarget(int32 Team, int32 SquadID) const;
	// Exoskeleton soldier objective (BattleExo.cpp): the team's fast reserve. An own point with more known enemies than own soldiers
	// first, then the point nearest a known enemy vehicle, then a point the team does not hold and few enemies are known at.
	int32 AreaExoTarget(int32 Team, int32 SquadID) const;
	int32 AreaVehicleChoice(int32 SquadID, TFunctionRef<double(int32)> ScoreOf) const;
	// Strategic layer trial (-BattleAreaComposition=1): the AI's own mix of special squad types, written into the intent's Keep numbers.
	void AreaComposition(int32 Team, FAreaIntent& Intent, int32 Bought) const;
	struct FCarChoice { int32 Target = INDEX_NONE; double At = -1; };
	mutable TMap<int32, FCarChoice> CarChoices; // transient, squad -> current objective
	// Scout reports (ROADMAP 2026-11-14, stage 3): what a scout pair sees beyond normal sight reaches its team only through a timed
	// two-step report (direction after ScoutAlertSeconds, positions and size after ScoutReportSeconds). Transient: a load restarts the timers.
	struct FScoutReport { double Started=-1, LastLong=-1, AlertAt=-1, ReportedAt=-1, NextHint=0, RetreatUntil=-1, PauseStart=-1, LastDanger=-1; FVector ReportFrom=FVector::ZeroVector; float LastHP=-1; int32 Stage=0, Living=0, Sequence=0, AlertSequence=0, Burns=0; FVector Direction=FVector::ZeroVector, Approx=FVector::ZeroVector, RetreatPoint=FVector::ZeroVector; TArray<FVector> Positions; };
	// Scout retreat (2026-11-17): an AI-led pair that is hit falls back ScoutRetreatDistance from the nearest seen enemy, watches from
	// there until ScoutHoldSeconds pass without a hit, then returns to its post shifted sideways (Burns). The report pauses while it moves.
	bool IsAIScoutLed(int32 SquadID) const;
	// Intel trial (-BattleAreaIntel=1, 2026-11-17): the AI acts on what its team knows of enemy positions (own sight now, scout reports).
	// (1) reinforce own points where more enemies are known than own soldiers, (2) attack the points where fewer enemies are known,
	// (3) raise the home-assault share when the enemy home was observed lightly held, (4) buy squads to defend a threatened point
	// (released once the threat is gone and the squad is there). Known positions only; never the hidden truth.
	struct FAreaIntelView { double At=-1; TArray<FVector> Enemies; TArray<int32> EnemyKeys /* unit IDs, below zero for scout report positions */; TArray<int32> OwnAt, EnemyAt; int32 HomeEnemies=0; bool bHomeCovered=false; };
	const FAreaIntelView& AreaIntelView(int32 Team) const;
	bool AreaIntelOn(int32 Team) const;
	int32 AreaIntelThreat(int32 Team) const;   // own point with the largest known enemy surplus (-1 = none)
	bool AreaIntelThreatAt(int32 Team, int32 Point) const;
	void UpdateIntelPurposes();
	TMap<int32, int32> IntelPurpose;           // squad -> point it was bought to defend (transient)
	mutable FAreaIntelView IntelViews[2];
	TMap<int32, FScoutReport> ScoutReports;
	void UpdateScoutReports();
	// True when a scout sees this contact only because of its long still-sight (team displays and plans use the reports instead).
	bool IsScoutLongContact(int32 ObserverID, const struct FBattleContact& Contact) const;
	// Positions from the team's scout reports of the last ScoutReportMemory seconds (keys below zero, never unit IDs).
	void AddScoutReportSightings(int32 Team, TMap<int32, FVector>& Seen) const;
	void AreaFronts(int32 Team, FAreaFront (&Out)[FAreaIntent::MaxObjectives]) const;
	// Objective worth (FMapDef::ObjectiveValues; 1 where the map sets none): multiplies its income and its end score.
	float ObjectiveValue(int32 Point) const;
	// Resources a team gains per income tick from the objectives it truly owns now (before the time ramp): base + per point x value x map scale.
	int32 AreaIncomeGain(int32 Team) const;
	// Depots in the point allocation (BattleAreaDepot.cpp): multiplies Intent.PointWeight of the objectives worth more than one.
	void ApplyDepotWeights(int32 Team, bool bTrialTeam, struct FAreaIntent& Intent) const;
	mutable TArray<float> DepotWeights[2];
	mutable float DepotWeightsAt[2]={-1.f,-1.f};
	// Routes (BattleRoutes.cpp): on maps with a road network, the next junction a squad at From is sent to on its way to Goal.
	bool RoutesOn() const;
	TArray<FVector> RouteKnownEnemies(int32 Team) const;
	int32 RouteFlankSide(const FVector& From, const FVector& OwnHome, const TArray<FVector>& KnownEnemies, int32 SquadID) const;
	FVector RouteStep(const FVector& From, const FVector& Goal, int32 FlankSide, const TArray<FVector>& KnownEnemies) const;
	// Player route choice (BattlePlayerRoutes.cpp), per squad of the player's team. Empty in matches without a player's choice.
	TMap<int32, FPlayerRoute> PlayerRoutes;
	float PlayerRouteClock = 0.f;
	int32 PlayerRouteLane(int32 SquadID) const;
	void SetPlayerRouteLane(int32 SquadID, int32 Lane);
	void ClearPlayerRouteMission(int32 SquadID);
	// A manual order over the squad's chosen route: -1 not routed (give the ordinary order), 0 refused, 1 accepted.
	int32 PlayerRouteOrder(int32 SquadID, EBattleOrder Order, const FVector& Goal);
	// The next junction for a squad with a chosen route (RouteStep without one).
	FVector PlayerRouteStep(int32 SquadID, const FVector& From, const FVector& Goal);
	void UpdatePlayerRoutes(float DeltaSeconds);
	// Drawn way (BattlePlayerPath.cpp): the squad walks Points in order and takes Order at the last. 1 accepted, 0 refused.
	int32 PlayerPathOrder(int32 SquadID, EBattleOrder Order, const TArray<FVector>& Points);
	bool PlayerPathNext(FPlayerRoute& Route, const FVector& Leader, FVector& Step) const;
	// The points a routed or drawn order still has ahead of the one being walked to (display).
	bool PlayerRoutePreview(int32 SquadID, TArray<FVector>& Out) const;
	// Self-test of the height differences (BattleTerrainTest.cpp, -BattleHeightTest).
	void UpdateTerrainTest(float DeltaSeconds);
	void CheckTimeLimit();
	void UpdateEnemyOrders();
	void UpdateTeamOrders(int32 Team, bool bDelegatedOnly = false);
	bool CanDelegateAllies() const;
	bool SetAllyDelegated(int32 SquadID, bool bDelegated);
	float DelegationClock = 5.f;
	int32 Resupply(int32 UnitID, int32 RequestedRounds = 10);
	UBattleSave* CaptureSnapshot() const;
	bool RestoreSnapshot(const UBattleSave* Snapshot);
	bool SaveMatch(const FString& Slot = TEXT("FutureBattleQuick01"));
	bool LoadMatch(const FString& Slot = TEXT("FutureBattleQuick01"));
	bool RequestSupport(int32 ObserverID);
	void UpdateSupport(float DeltaSeconds);
	bool IsEnabled() const { return Phase != EBattleMatchPhase::Disabled; }
};
