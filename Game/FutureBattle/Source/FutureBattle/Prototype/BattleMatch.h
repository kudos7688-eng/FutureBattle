#pragma once
#include "CoreMinimal.h"
#include <cfloat>
#include "Subsystems/WorldSubsystem.h"
#include "BattleState.h"
#include "BattleAreaState.h"
#include "BattleCompositionState.h"
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
 int32 SupplyKeep=0;                  // supply vehicles the AI keeps active (same pacing as scouts; 0 = none; BattleSupplyCar.cpp)
 int32 ATKeep=0;                      // anti-tank teams the AI keeps active (same pacing as scouts; 0 = none)
 int32 ScoutKeep=0;                   // scout pairs the AI keeps active (one more per ScoutAfterHires purchases, up to this; 0 = none)
 int32 FocusPoint=-1;                 // strategy layer S2 (-BattleStrategy>=2): the objective the team concentrates on (replaces S6's static pick; -1 = none)
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
 // HUD invalidation only; not part of the saved match state.
 uint64 RestoreGeneration=0;
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
 // Area new squad (regular only) into a free slot (< BattleTactics::AreaHire().MaxActive active squads, default 25) for BattleTactics::AreaHire().Cost resources.
 // bAuto: AI purchase (no selection change; a team-0 squad is delegated). Cancels pending replenishments the spend can no longer cover.
 bool HireAreaSquad(int32 Team,EBattleTraining Training,bool bAuto,uint8 SquadType=0);
 int32 LastHiredSquadID=INDEX_NONE;
 // P1 trial: the observed need a new AI squad would serve (see FAreaHireTuning), or -1 for none.
 int32 AreaHireNeed(int32 Team) const;
 // AI purchase rule, every director tick: own resources minus own pending replenishments >= cost + reserve, and a free slot.
 // Team 1 always; team 0 only in the all-AI sight experiment.
 void UpdateAreaHiring();
 // Initialized only for new ON matches. Loading pre-extension-4 saves explicitly disables the planner.
 int32 CompositionTeams=0;
 UPROPERTY() TArray<FCompositionTeamState> CompositionState;
 void InitializeComposition();
 void UpdateCompositionHiring(int32 Team);
 void UpdateCompositionPurposes(int32 Team);
 bool CompositionHomeThreat(int32 Team) const;
 int32 ActiveSquads(int32 Team) const;
 // Active scout pairs of a team (at most UBattleTuning::AreaScoutLimit).
 int32 ActiveScouts(int32 Team) const;
 int32 ActiveOfType(int32 Team,uint8 SquadType) const;
 // Supply vehicles (squad type 9, BattleSupplyCar.cpp). SupplyCarNear: the nearest commanded vehicle of the team with stock left within
 // Radius (default UBattleTuning::SupplyTeamRadius) of the position (INDEX_NONE: none); SupplyTeamNear says whether there is one.
 int32 SupplyCarNear(int32 Team,const FVector& Position,float Radius=-1.f) const;
 bool SupplyTeamNear(int32 Team,const FVector& Position) const { return SupplyCarNear(Team,Position)!=INDEX_NONE; }
 int32 SupplyStockMax() const;
 int32 SupplyStockOf(int32 SquadID) const;
 void SpendSupply(int32 SquadID,int32 Rounds);
 void RestockSupplyCar(int32 SquadID);
 // The order of an AI-run supply vehicle: home to load while low; with its group (role 5) behind the group's post; otherwise behind the
 // own point nearest the enemy home. False: not a supply vehicle.
 bool SupplyCarOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const;
 // Where a squad short of ammunition gets it in the field: the position of a supply vehicle of its team with stock within
 // UBattleTuning::FieldSupplyRange (its own group's first). False: none (the squad goes home as before).
 bool FieldSupplyPoint(int32 SquadID,FVector& Where,int32* CarID=nullptr) const;
 // Front line and front-holding group (2026-11-20, user proposal; group stage 4, BattleFrontLine.cpp). Trial -BattleFrontLine=1 (default
 // 0; with -BattleAreaTrialTeam for that team only), maps with sectors only. FrontLine[Team][Row]: the line's sector column in that
 // sector row, counted from the team's own home edge (0); empty while off. Built from own positions and known enemies only.
 bool FrontLineOn(int32 Team) const;
 void UpdateFrontLine(int32 Team);
 bool FrontGroupOrder(int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const;
 FIntPoint FrontCell(int32 Team,int32 Column,int32 Row) const; // a line column/row of the team -> grid cell
 TArray<int32> FrontLine[2];
 // Surprise group (trial -BattleSurpriseGroup=1, needs the front line): along an edge row to the enemy home, then strikes it.
 bool SurpriseOn(int32 Team) const;
 void UpdateSurpriseGroup(int32 Team,int32 FrontID);
 bool SurpriseGroupOrder(int32 GroupID,int32 SquadID,int32 Team,FVector& Center,EBattleOrder& Order) const;
 // True while the squad's surprise group is moving along its row (phase 1): its tactics do not stop to hold at a seen enemy.
 bool SurpriseMoving(int32 SquadID) const;
 // After a save restore: no front/surprise timer further ahead than its own interval (an earlier save would stall them).
 void ClampFrontTimers();
 float SurpriseNextAt[2]={0.f,0.f},SurpriseSkipLogAt[2]={0.f,0.f};
 TMap<int32,FVector> FrontPosts; // squad of a front-holding group -> the sector centre it holds (transient)
 float FrontLineAt[2]={-1.f,-1.f},FrontLineLogAt[2]={-1.f,-1.f};
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
 // Public map geometry and AI purposes: transient, rebuilt/reset after a successful restore.
 TArray<FVector> ArtilleryPostPositions[2];
 TArray<double> ArtilleryPostEvidence[2]; // observer team -> enemy post's last evidence timestamp
 TMap<int32,int32> ArtilleryRaids;
 mutable TMap<int32,int32> MortarPostChoices;
 void GenerateArtilleryPosts();
 void UpdateArtilleryRaids(int32 Team);
 bool AtArtilleryPost(int32 Team,const FVector& Position) const;
 FVector ChooseMortarPost(int32 Team,int32 SquadID) const;
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
	// Squad groups, stage 1 (BattleGroups.cpp): members, one objective, a phase (0 assemble at Rally, 1 advance, 2 hold). Saved with all decision history by combat extension 2 (older saves start empty).
	// Stage 2: roles while advancing (0 assault: to the point; 1 support: fires from a post short of it; 2 armour: the tank's post behind
	// the infantry; 3 fire: mortar or gun at its firing post; 4 guard: the anti-tank team beside the support post; 5 supply: the supply vehicle behind
	// the group's post (BattleSupplyCar.cpp); 255 none: the squad
	// keeps its own type's rules). A point the group could not take is avoided until AvoidUntil.
	struct FBattleGroup { int32 ID = 0, Team = 0; TArray<int32> Squads; int32 Objective = INDEX_NONE; uint8 Phase = 0; float PhaseAt = 0.f; FVector Rally = FVector::ZeroVector; bool bPlayer = false; TMap<int32, uint8> Roles; int32 Avoid = INDEX_NONE; float AvoidUntil = 0.f;
		// Stage 3b: the living soldiers when the advance began (losses end it early) and how often in a row the group failed at StallPoint.
		double StartStrength = -1;
  int32 StartLiving = 0, StallPoint = INDEX_NONE, Stalls = 0;
		bool bOpened = false; /* the opening rush was decided (once per group) */
		bool bArrived = false; /* this advance has reached its point: the stall clock (PhaseAt) runs from then */
		float ArrivedAt = 0.f; int32 Extensions = 0; /* stage 3d: when it arrived, and how often the stall clock was extended while pushing */
		uint8 Kind = 0; /* 0 attack (raid) group, 1 front-holding group, 2 surprise group (BattleFrontLine.cpp) */
		int32 SurpriseRow = 0; /* surprise group: the edge sector row it moves along */
		uint8 SurpriseBridgeStage = 0; /* 0 none, 1 entrance, 2 exit, 3 original edge goal; retained during same-time restore */
		int32 SurpriseBridgeLeader = INDEX_NONE;
		float SurpriseBridgeAt = 0.f; /* when the current bridge leg began */
		int32 SurpriseNextCol = -1; /* surprise group: the furthest stop column given in phase 1; a temporary pull-back does not move it back */
		TArray<int32> FrontHold; /* front-holding group: the line the group holds, one column per row (own-home based); moved by UpdateFrontLine only */
		float FrontStepAt = 0.f; /* front-holding group: when its line last stepped forward */
		float FrontBackAt = -100.f; /* front-holding group: when rows last fell back for damage */
		bool bFrontDrawn = false; /* front-holding group: FrontHold is a line the player drew (SetDrawnFrontLine); it does not step or fall back */
		FVector SurpriseBridgeEntry = FVector::ZeroVector, SurpriseBridgeExit = FVector::ZeroVector, SurpriseBridgeGoal = FVector::ZeroVector;
		float SurpriseWaitAt = -1.f, SurpriseLogAt = 0.f, SurpriseEndAt = 0.f; /* surprise group: since when its lead waits for the others; next progress log; when it ends whatever its phase */ };
	bool UpdateSurpriseBridge(FBattleGroup& Group,const FVector& Lead);
	// The player draws the team's front line (Ctrl + right drag, BattleController): each sector row the drawn line crosses takes that
	// column; rows it does not cross keep theirs. Fewer than two points hand the line back to the group. Returns the rows set, 0 when
	// handed back, -1 without a front-holding group (or -BattleFrontHold=0), -2 when the line crossed no row.
	int32 SetDrawnFrontLine(int32 Team,const TArray<FVector>& Points);
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
	// Command UI armies (2026-11-21): 0 unassigned, 1 front army (the team's front-holding group), 2 attack army (its other groups).
	// AssignArmy moves an own squad there and pins it, so automatic filling (front army, attack and surprise groups) leaves it. Not saved.
	// TargetGroup: an own attack group to join, NewAttackGroup for a new one (the "+ new group" row), INDEX_NONE for the army's default.
	static constexpr int32 NewAttackGroup = -2;
	bool AssignArmy(int32 SquadID, int32 Army, int32 TargetGroup = INDEX_NONE);
	int32 ArmyOf(int32 SquadID) const;
	TSet<int32> ArmyPinned;
	bool GroupAIRun(const FBattleGroup& Group) const;
	int32 GroupLiving(const FBattleGroup& Group, int32* Full = nullptr) const;
 struct FGroupStrength {
  double Infantry=0,Armour=0,Soft=0,Hard=0,Unknown=0;
  void Add(uint8 Type);
  double Total() const { return Infantry+Armour; }
  double Against(const FGroupStrength& Other) const;
  bool SuperiorTo(const FGroupStrength& Other,double Ratio) const;
 };
 static bool GroupSquadPresent(const FBattleSquadState& Squad);
 FGroupStrength GroupPower(const FBattleGroup& Group,bool bFull=false) const;
 FGroupStrength GroupPointPower(int32 Team,int32 Point,bool bEnemy) const;
 bool GroupPointThreat(int32 Team,int32 Point) const;
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
	mutable TMap<int32, FCarChoice> CarChoices; // saved, squad -> current objective (At uses match Elapsed)
	// Scout reports (ROADMAP 2026-11-14, stage 3): what a scout pair sees beyond normal sight reaches its team only through a timed
	// two-step report (direction after ScoutAlertSeconds, positions and size after ScoutReportSeconds). Transient: a load restarts the timers.
	struct FScoutReport { bool bHomeObserved=false; double Started=-1, LastLong=-1, AlertAt=-1, ReportedAt=-1, NextHint=0, RetreatUntil=-1, PauseStart=-1, LastDanger=-1; FVector ReportFrom=FVector::ZeroVector; float LastHP=-1; int32 Stage=0, Living=0, Sequence=0, AlertSequence=0, Burns=0; FVector Direction=FVector::ZeroVector, Approx=FVector::ZeroVector, RetreatPoint=FVector::ZeroVector; TArray<FVector> Positions; };
	// Scout retreat (2026-11-17): an AI-led pair that is hit falls back ScoutRetreatDistance from the nearest seen enemy, watches from
	// there until ScoutHoldSeconds pass without a hit, then returns to its post shifted sideways (Burns). The report pauses while it moves.
	bool IsAIScoutLed(int32 SquadID) const;
	// Intel trial (-BattleAreaIntel=1, 2026-11-17): the AI acts on what its team knows of enemy positions (own sight now, scout reports).
	// (1) reinforce own points where more enemies are known than own soldiers, (2) attack the points where fewer enemies are known,
	// (3) raise the home-assault share when the enemy home was observed lightly held, (4) buy squads to defend a threatened point
	// (released once the threat is gone and the squad is there). Known positions only; never the hidden truth.
	struct FArtilleryCluster { TArray<int32> Anchors; FVector At=FVector::ZeroVector; int32 Count=0; };
 struct FAreaIntelView { double At=-1; TArray<FVector> Enemies; TArray<int32> EnemyKeys /* unit IDs, below zero for scout report positions */; TArray<int32> OwnAt, EnemyAt; int32 HomeEnemies=0; bool bHomeCovered=false;
  // Fresh direct LOS report, distinct from knowing the base position. Match-clock timestamp.
  int32 HomeObserver=INDEX_NONE; double HomeObservedAt=-1; FVector HomeReportFrom=FVector::ZeroVector;
  TMap<int32,FVector> DirectEnemies; // direct sightings only, for the post trial
  TMap<int32,uint8> EnemyTypes; // observed contact metadata only; report dots without identity remain unknown
  TMap<int32,double> EnemyObservedAt; // match-clock time of the same observation as position and type; transient
  uint8 ObservedType(int32 Key) const { const uint8* Type=EnemyTypes.Find(Key);return Type?*Type:uint8(255); }
  bool RecentArmourObservation(int32 Key,double Now,double Memory) const { const double* SeenAt=EnemyObservedAt.Find(Key);return SeenAt&&*SeenAt<=Now&&Now-*SeenAt<=Memory; }
  double FlashExpiresAt=DBL_MAX;
  mutable bool bClustersBuilt[2]={false,false};
  mutable TArray<FArtilleryCluster> Clusters[2];
 };
	const FAreaIntelView& AreaIntelView(int32 Team) const;
 bool ArtilleryHomeObserved(int32 Team) const;
 const TArray<FArtilleryCluster>& AreaArtilleryClusters(int32 Team,bool bSPG) const;
	bool AreaIntelOn(int32 Team) const;
	int32 AreaIntelThreat(int32 Team) const;   // own point with the largest known enemy surplus (-1 = none)
	bool AreaIntelThreatAt(int32 Team, int32 Point) const;
	void UpdateIntelPurposes();
	TMap<int32, int32> IntelPurpose;           // squad -> point it was bought to defend (transient)
	mutable FAreaIntelView IntelViews[2];
 struct FFireReveal { uint8 ObservedSquadType=255; bool bArtillery=false; double At=0,Memory=10.; float ErrorRadius=500.f; FVector Position=FVector::ZeroVector; };
 // Shared gun reports, keyed by shooter unit ID; match seconds, transient, never saved.
 mutable TMap<int32,FFireReveal> FireReveals[2];
	TMap<int32, FScoutReport> ScoutReports;
	void UpdateScoutReports();
	// True when a scout sees this contact only because of its long still-sight (team displays and plans use the reports instead).
	bool IsScoutLongContact(int32 ObserverID, const struct FBattleContact& Contact) const;
	// Positions from the team's scout reports of the last ScoutReportMemory seconds (keys below zero, never unit IDs).
	void AddScoutReportSightings(int32 Team, TMap<int32, FVector>& Seen) const;
	void AreaFronts(int32 Team, FAreaFront (&Out)[FAreaIntent::MaxObjectives]) const;
 // Strategy layer, stage S1 (BattleStrategy.cpp, Docs/design-strategy-layer.md): the team's situation map. Theatres are the objectives'
 // Voronoi cells (by Y on three-objective maps, as AreaFronts), plus the enemy home zone (index N) and the own home zone (N+1), each
 // within 150 m of that home. Own soldiers and known enemies (sightings now, scout reports, and remembered contacts weighted by
 // age) only; never the enemy's true positions. Built on demand without touching any cache (AreaIntelView is not called).
 struct FSituationTheatre { int32 Own=0; double OwnPower=0; int32 Fresh=0; double Memory=0; double KnownPower=0; double EnemySeenAge=-1, PointSeenAge=-1; int32 Owner=-2; uint8 State=0; /* 0 unknown, 1 enemy ahead, 2 even, 3 own ahead */ };
 struct FBattleSituation { double At=-1; TArray<FSituationTheatre> Theatres; };
 int32 TheatreOf(int32 Team, const FVector& P) const;
 void BuildSituation(int32 Team, FBattleSituation& Out) const;
 // -BattleSituationLog=1 (default 0): BATTLE_SITUATION every 30 match seconds per team and theatre, with the true enemy count of the
 // theatre after '|' for evaluation only (counted inside the log function; no decision reads it). Does not change the match.
 static bool SituationLogOn();
 void UpdateSituationLog();
 int32 SituationLogBucket = -1;
 // Stage S2 (-BattleStrategy=2, -BattleStrategyTeam=0/1 for one team): recon -> focus attack. UpdateStrategy runs once per team order
 // update (before AreaIntent is read): after the opening it picks the focus objective from the situation map, keeps it at least
 // StrategyFocusHold seconds, reviews every StrategyReview seconds and at once when the focus is taken. State in Area.Strategy*.
 static int32 StrategyLevel();
 static bool StrategyOn(int32 Team, int32 Level);
 int32 StrategyFocusPoint(int32 Team) const; // INDEX_NONE without a valid focus
 void UpdateStrategy(int32 Team);
 // Stage S3 (-BattleStrategy=3): own points held against a known superior enemy (Area.StrategyHoldMask).
 int32 StrategyHoldPoint(int32 Team) const; // the one threatened longest; INDEX_NONE without holds
 bool StrategyHolds(int32 Team, int32 Point) const;
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
	// Sector routes (BattleSectorRoute.cpp, trial -BattleSectorRoute=1): the sector beside the next junction an AI squad stops in.
	static bool SectorRouteOn();
	static bool SectorFlankOn(); // -BattleSectorFlank=1: stops beside the known enemy's front near the goal
	// -BattleSectorFront=1: each team's sectors (1 own soldiers, 2 known enemies, 3 both) and its front (own sectors next to enemies).
	static bool SectorFrontOn();
	void UpdateSectorFront(int32 Team, const TArray<FVector>& KnownEnemies) const;
	mutable TMap<FIntPoint, uint8> SectorState[2];
	mutable TSet<FIntPoint> SectorFrontCells[2];
	mutable double SectorFrontAt[2] = {0, 0}, SectorFrontLogAt[2] = {0, 0};
	int32 SectorCoverCount(FIntPoint Cell) const;
	bool SectorStage(const struct FBattleSquadState& S, const FVector& Leader, const FVector& Step, const FVector& Goal, const TArray<FVector>& KnownEnemies, FVector& Stage) const;
	mutable TMap<FIntPoint, int32> SectorCover;
	// Trial -BattleSectorCoverRoute=1: with a known enemy within 150 m of a candidate stop, open sectors (fields, plazas) score
	// lower, so the march goes through houses and woods.
	static bool SectorCoverRouteOn();
	// Trial -BattlePinnedFlank=1: a pinned squad (UBattleLeaderRuntime::PinnedThreat) is sent to the neighbouring sector most to
	// the side of the threat (not much closer to it, not falling back) and keeps that stop for 40 s or until it is there.
	bool PinnedFlankStage(int32 SquadID, const FVector& Leader, FVector& Stage);
	TMap<int32, TPair<FIntPoint, double>> PinnedFlank;
	// Trial -BattleCloseIn=1: a squad fighting from its sector at long range (UBattleLeaderRuntime::CloseInThreat) is sent to the
	// neighbouring sector that is at least half a sector closer to the threat - the most cover, the least danger, not open
	// fields, not an enemy sector - and keeps that stop for 60 s (it then fights from there; the order loop takes over again).
	bool CloseInStage(int32 SquadID, const FVector& Leader, FVector& Stage);
	TMap<int32, TPair<FIntPoint, double>> CloseIn;
	// Trial -BattleDangerRoute=1 (2026-11-20, default 0; user: "tell whether the area ahead is dangerous and do not go through"):
	// every hit on a soldier marks his sector for his team (1 per hit, halved every 60 s; own losses only, never the enemy's
	// true positions). Route stops, pinned-flank stops and close-in stops score 3 lower per point (up to 8 points).
	static bool DangerRouteOn();
	void NoteSectorDanger(int32 Team, const FVector& At);
	double SectorDanger(int32 Team, FIntPoint Cell) const;
	TMap<FIntPoint, TPair<double, double>> SectorDangerMap[2];
	// Player route choice (BattlePlayerRoutes.cpp), per squad of the player's team. Empty in matches without a player's choice.
	TMap<int32, FPlayerRoute> PlayerRoutes;
	float PlayerRouteClock = 0.f;
	int32 PlayerRouteLane(int32 SquadID) const;
	void SetPlayerRouteLane(int32 SquadID, int32 Lane);
	void ClearPlayerRouteMission(int32 SquadID);
	// A manual order over the squad's chosen route: -1 not routed (give the ordinary order), 0 refused, 1 accepted.
	int32 PlayerRouteOrder(int32 SquadID, EBattleOrder Order, const FVector& Goal);
	// The next junction for a squad with a chosen route (RouteStep without one).
	FVector PlayerRouteStep(int32 SquadID, const FVector& From, const FVector& Goal, FPlayerRoute* Candidate = nullptr);
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
