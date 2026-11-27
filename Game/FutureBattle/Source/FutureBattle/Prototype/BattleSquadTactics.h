#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleSquadTactics.generated.h"

struct FBattleSquadState;
USTRUCT()
struct FBattleSquadObservation
{
 GENERATED_BODY()
 // Sum of living members' maximum HP (leader multiplier included). 0 falls back to PerUnit*Living.
 float MaxHealth=0;
 float FullHealth(float PerUnit) const { return MaxHealth>0.f?MaxHealth:PerUnit*Living; }
 UPROPERTY() int32 Living=0;
 UPROPERTY() int32 Ready=0;
 UPROPERTY() int32 Armed=0;
 UPROPERTY() int32 Direct=0;
 UPROPERTY() int32 Ammo=0;
 UPROPERTY() int32 Reserve=0;
 UPROPERTY() int32 Cover=0;
 UPROPERTY() int32 Support=0;
 UPROPERTY() int32 Flank=0;
 UPROPERTY() int32 Failed=0;
 UPROPERTY() float Health=0;
 UPROPERTY() float RecentDamage=0;
 UPROPERTY() double SampleTime=0;
 UPROPERTY() TMap<int32,FBattleContact> Contacts;
 UPROPERTY() FString Reason;
};

// Shared observation; ST2 autonomous execution is restricted to the mixed wide field.
UCLASS()
class UBattleSquadTactics : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 bool IsEnabled() const { return bEnabled; }
 bool IsObserving() const { return bObserve; }
 bool IsST2Enabled() const { return bEnabled && bST2; }
 bool IsFlankEnabled() const { return IsST2Enabled() && bFlank; }
 bool IsFlankEnabledFor(int32 Team) const { return IsFlankEnabled() && Team>=0 && Team<2 && (FlankTeams & (1<<Team)); }
 bool IsRelaxedFlank() const { return bRelaxedFlank; }
 bool TryFlank(int32 SquadID, int32 Supporter = INDEX_NONE);
 void Step(float DeltaSeconds);
 bool AcceptSupport(int32 SquadID, int32 RetreatSquadID, int32 TargetID);
 void ReleaseSupport(int32 SquadID, const FString& Reason);
 void RecordSupportShot(int32 SquadID, int32 TargetID);
 bool CanSupport(int32 SquadID, int32 TargetID) const;
 bool CanMaintainSupport(int32 SquadID, int32 TargetID) const;
 bool TryReplaceFlankSupport(int32 SquadID);
 bool RefreshFlankTarget(int32 SquadID);
 FBattleSquadObservation Observe(const FBattleSquadState& Squad) const;
 const FBattleSquadObservation* Find(int32 SquadID) const { return Samples.Find(SquadID); }
 void Event(FName Reason, int32 Count = 1);
 void RecordDamage(int32 SquadID, float Damage);
 void Dump() const;
 void Reset();
 bool SeizingSector(int32 SID) const;
 bool SeizeClearing(int32 SID) const;
 bool SeizePointMember(int32 SID) const; // in a point concentration (assemble, assault, clear, support): not lent to SUPPORT/FLANK
 bool SeizeSupportFocus(int32 SID,FVector& Out) const;
 bool SeizeEscortVehicleTarget(int32 SID,int32 TargetID,uint8 SeenType) const;
 bool SeizeEscortPlace(int32 SID,const FVector& At) const;
 bool SeizeEscortVehicle(int32 SID) const;
 bool SeizeSupportTarget(int32 SID,const FVector& At) const;
 bool MarchingSector(int32 SID) const;
 bool DefendingSector(int32 SID) const;
 bool SectorDefendPlace(int32 SID,const FVector& Place) const;
 bool SectorDefendMoving(int32 SID) const;
 void SectorDefendReplan(int32 SID);

 double Clock() const { return ActiveSeconds; }
 void InjectPointTruthSpots(int32 Team,const TArray<FVector>& Positions);
 // Spot relaxation entry points. Callers gate trials; helpers take explicit values for isolated smoke tests.
 bool AddKnownSpot(int32 Team,const FVector& At,double Seen,uint8 Kind,bool bLegacy=false);
 bool NoteShotSpot(int32 ShooterID,int32 Team,const FVector& At,double Now,float Hear,float Error,float Every);
 void NotePointLost(int32 PointID,int32 NewOwner,const FVector& At,double Now,float Seconds);
 void AdvanceHQSpots(int32 Team,double Now,float Interval,float Delay,int32 Min,int32 Depth);
 static void TestSpotRelax(UWorld* World,TFunctionRef<void(bool,const TCHAR*)> Check);
 const TMap<FName,int64>& EventCounts() const { return Events; }
 const TMap<FName,double>& ActionDurations() const { return ActionSeconds; }
 // Evaluation only (never read by AI decisions): home supply trips by squad, timed on the tactical clock.
 const TMap<int32,double>& OpenSupplyTrips() const { return SupplyTripStart; }
 const TArray<double>& CompletedSupplyTripSeconds() const { return SupplyTripSeconds; }
private:
 friend class UBattleLeaderRuntime; // In-world integration fixtures exercise the same evaluator.
 friend class UBattleTeamAI; // Reassess a flank immediately if support acceptance fails.
 void Evaluate(int32 SquadID, const FBattleSquadObservation& Sample, float Dt);
 void EvaluateFlank(int32 SquadID, const FBattleSquadObservation& Sample);
 bool RetreatGoal(const FBattleSquadState& Squad, const FBattleSquadObservation& Sample, FVector& Goal);
 bool Apply(int32 SquadID, EBattleSquadTactic Action, EBattleOrder Order, FVector Goal, const FString& Reason);
 // B06: end a temporary plan and go back to the squad's current mission (order, goal and sector flag). BattleSquadRetreat.cpp.
 bool ResumeMission(int32 SquadID, const TCHAR* Reason, const TCHAR* EventName);
 bool bEnabled = true, bObserve = false;
 bool bST2 = true;
 bool bFlank = true;
 bool bRelaxedFlank = false;
 bool bCoverActions=false;
 bool bCoordination=false;
 int32 FlankTeams = 3;
 bool SoloFlankRisk(const FBattleSquadObservation& O,FVector Start,double Length,double Exposure) const;
 bool ReassessSoloFlank(int32 SquadID,const FBattleSquadObservation& O);
 double ActiveSeconds = 0, NextDump = 5;
 TMap<int32, double> NextSample;
 TMap<int32, FBattleSquadObservation> Samples;
 TMap<FName, int64> Events;
 TMap<FName, double> ActionSeconds;
 TMap<int32, double> SupplyTripStart;
 TArray<double> SupplyTripSeconds;
 void SupplyTripBegin(int32 SquadID){ SupplyTripStart.Add(SquadID, ActiveSeconds); }
 // Trial -BattleJoinFight (BattleTactics.h): the squad each joining squad walks to, when it set out, since when that squad has not
 // been fighting (-1: it fights) and when the join ended (-1: under way).
 struct FJoinFight{int32 Host=INDEX_NONE;double Start=0.,QuietSince=-1.,EndAt=-1.;};
 TMap<int32,FJoinFight> JoinFights;
 // Trial -BattleSupplyCarRecover (BattleTactics.h): when a squad gave up waiting for replacements at a supply vehicle (it does not
 // try a vehicle for its losses again for 180 s). Not saved.
 TMap<int32,double> FieldRecoverGaveUp;
 // ... and since when a squad is on a trip to a vehicle for its losses (a trip for ammunition only is not in here).
 TMap<int32,double> FieldRecoverSince;
 // -BattleSupplyCarFirst: the squads of FieldRecoverSince that have reached their vehicle (their 120 s count from then).
 TSet<int32> FieldRecoverArrived;
 // Trial -BattleSeekKnown (BattleTactics.h): per team, the 50 m squares in which one of its soldiers saw enemy infantry (the place
 // and the time of the last sighting; refreshed once a second), and per seeking squad the square it walks to, the place, when it
 // set out, when it arrived (-1: on its way) and when the seek ended (-1: under way). Not saved.
 struct FKnownSpot{FVector At=FVector::ZeroVector;double Seen=0.;uint8 Kind=0;};
 TMap<FIntPoint,FKnownSpot> KnownSpots[2];
 double KnownSpotsAt[2]={-100.,-100.};
 int64 SpotTruthSpots[2]={0,0}; // diagnostic -BattleSpotTruth bit 1: places written from the truth
 struct FSeekEnemy{FIntPoint Cell=FIntPoint::ZeroValue;FVector At=FVector::ZeroVector;double Start=0.,ArrivedAt=-1.,EndAt=-1.;};
 TMap<int32,FSeekEnemy> SeekEnemies;
 struct FSeizeSector
 {
  FIntPoint Cell=FIntPoint::ZeroValue;
  double Start=0.,EnterAt=-1.,QuietSince=-1.,TakenAt=-1.,EndAt=-1.,LastKnown=0.,LastSeen=-1.;
 };
 TMap<int32,FSeizeSector> SeizeSectors;
 // Written only inside the Point/Counter trials; no extra per-seize state on the legacy path.
 struct FSeizePointPlan
 {
  int32 Point=INDEX_NONE;
  FIntPoint Cell=FIntPoint::ZeroValue,Assembly=FIntPoint(-1,-1);
  double Start=0.,FirstAt=-1.,GoAt=-1.,ClearAt=-1.;
  bool Taken=false,Committed=false,Broken=false;
  int32 AssaultJoined=0,SupportJoined=0;
  FIntPoint SupportCell=FIntPoint(-1,-1);
 };
 struct FSeizeExtra{int32 Point=INDEX_NONE;bool Counter=false;double CapturedAt=-1.;
  bool EscortAtPost=false;bool Escort=false;FVector EscortGoal=FVector::ZeroVector;FIntPoint EscortAssembly=FIntPoint(-1,-1);
  FBattleMission EscortMission;
  bool Support=false;double FocusStart=-1.,ReleasedAt=-1.;FIntPoint PointCell=FIntPoint::ZeroValue;
  bool FreeCounter=false,GuardCounter=false,ClearHasSpot=false,Closing=false,FireWasActive=false;
  int32 AssaultOrdinal=0,SweepIndex=0;
  double ClearAt=-1.,ClearPickAt=-100.,SweepAt=-1.,FireSeconds=-1.,FireCheckedAt=-1.;
  FIntPoint ClearSpot=FIntPoint::ZeroValue,FireSpot=FIntPoint::ZeroValue;
  FVector ClearGoal=FVector::ZeroVector;};
 FSeizePointPlan SeizePoints[2];
 TMap<int32,double> SeizePointNoAt;
 double SeizePointNoneAt[2]={-100.,-100.};
 void SeizePointNo(int32 SID,int32 Team,const TCHAR* Why);
 void AdoptSeizePoint(int32 Team);
 bool SeizeEscort(int32 SID) const;
 int32 SeizeEscortCount(int32 Team,uint8 Type) const;
 bool SeizeEscortGoal(int32 SID,FIntPoint Cell,FIntPoint Assembly,FVector& Goal) const;
 void UpdateSeizeTanks(int32 Team);
 bool StepSeizeEscort(int32 SID);
 bool SeizeArmourCover(int32 SID) const;
 bool SeizeArmourStay(int32 SID);
 bool SeizeKnownVehicle(int32 Team,int32 Key,const FVector& At,bool Spot) const;
 TMap<int32,double> SeizeArmourStayAt;
 TMap<int32,FSeizeExtra> SeizeExtras;
 TMap<int32,TSet<FIntPoint>> SeizeGuards[2];
 TSet<FIntPoint> SeizeTrialOwn[2],SeizeTrialKnown[2];
 TMap<FIntPoint,int32> SeizeTrialSlots;
 double SeizeTrialAt[2]={-100.,-100.},SeizeTrialSlotsAt=-100.;
 void ResetSeizeExtensions();
 void UpdateSeizeExtensions(int32 Team);
 bool SeizeFreeSquad(int32 SID) const;
 bool BeginFreeCounter(int32 SID,const FBattleSquadObservation& O,bool Visible);
 bool StepSeizeClear(int32 SID,const FBattleSquadObservation& O,bool Visible);
 bool SeizeCounterCell(int32 Team,FIntPoint Cell,int32* Count=nullptr) const;
 bool SeizeGuardAllows(int32 SID,int32 Team,FIntPoint Cell) const;
 bool PickSeizeExtended(int32 SID,const FBattleSquadState& S,const FVector& Here,const FIntPoint* Around,FIntPoint& Out,bool CounterOnly=false);
 bool BeginSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible,int32 Threats);
 bool StartSeizeExtended(int32 SID,FIntPoint Cell,const TCHAR* SightReason,bool Adopt=false);
 bool StepSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible);
 bool ResumeSeizeExtended(int32 SID,const FBattleSquadObservation& O,bool Visible,bool Healthy);
 const TCHAR* SeizeExtendedEnd(int32 SID,double Now) const;
 FVector SeizeExtendedGoal(int32 SID,const TCHAR*& Reason) const;
 bool SeizePointWaiting(int32 SID) const;
 void LogSeizePoint(int32 Team,const TCHAR* What,const FIntPoint* LogCell=nullptr) const;
 bool PickSeizeSupport(int32 Team,FIntPoint& Out) const;
 TSet<int32> SeizeSuspended;
 bool ResumeSeizeSector(int32 SID,const FBattleSquadObservation& O,bool Visible);
 void SeizePlanChanged(int32 SID);
 struct FSectorDefend
 {
  FIntPoint Cell=FIntPoint::ZeroValue,From=FIntPoint::ZeroValue;
  double Start=0.,LastSeen=0.,LastReplan=-100.,BackAt=-1.;
  double LastDamage=0.,ArrivedAt=-1.;
  int32 OrderRevision=0;
 };
 struct FSectorMarch
 {
  FIntPoint Cell=FIntPoint::ZeroValue;int32 Revision=0;
  double Since=0.,Distance=0.,BlockedUntil=-1.;
  bool Kept=false,EnemyNoted=false;
 };
 TMap<int32,FSectorMarch> SectorMarches;
 TMap<int32,double> SectorMarchKeepAt;
 TMap<int32,bool> SectorMarchBaseSlots;
 TSet<FIntPoint> SectorMarchKnown[2];
 double SectorMarchKnownAt[2]={-100.,-100.};
 bool SectorMarchCandidate(int32 SID,FIntPoint& Cell,bool& Arrived) const;
 bool SectorMarchEnemy(int32 Team,FIntPoint Cell) const;
 bool StepSectorMarch(int32 SID,const FBattleSquadObservation& O,bool Visible);
 void LogSectorMarch(int32 SID,FIntPoint Cell,const TCHAR* What,double Distance) const;
 TMap<int32,FSectorDefend> SectorDefends;
 TMap<FIntPoint,int32> DefendSlotCounts;
 double DefendSlotsAt=-100.;
 bool SectorDefendEnabled(int32 Team) const;
 bool StepSectorDefend(int32 SID,const FBattleSquadObservation& O,bool Visible);
 bool FallBackSector(int32 SID,const FBattleSquadObservation& O);
 void EndSectorDefend(int32 SID);
 void LogSectorDefend(int32 SID,const FSectorDefend& Defence,const TCHAR* What) const;
 void CheckSectorTrials();

 TMap<int32,TMap<FIntPoint,double>> SeizeEnded;
 TSet<FIntPoint> SeizeOwnCells[2],SeizeKnownCells[2];
 double SeizeCellsAt[2]={-100.,-100.};
 bool SectorSeizeEnabled(int32 Team) const;
 bool SeizeSectorKnown(int32 Team,FIntPoint Cell,double Now,int32* Count=nullptr) const;
 int32 SeizeSectorAllies(int32 SID,int32 Team,FIntPoint Cell) const;
 bool PickSeizeSector(int32 SID,const FBattleSquadState& S,const FVector& Here,double Now,const FIntPoint* Around,FIntPoint& Out);
 bool StepSeizeSector(int32 SID,const FBattleSquadObservation& O,bool Visible);
 bool BeginSeizeSector(int32 SID,const FBattleSquadObservation& O,bool Visible,int32 Threats);
 void EndSeize(int32 SID,const TCHAR* Why=TEXT("SeizeEndOther"));
 void LogSeizeSector(int32 SID,const FSeizeSector& Seize,const TCHAR* Why) const;
 // Per squad, the squares its seeks ended in and when (30 s before it goes to one again, and its place there counts that long).
 TMap<int32,TMap<FIntPoint,double>> SeekEnded;
 // All auxiliary writes are inside enabled trial paths. Kind: own, shared, shot, lost, HQ, truth.
 int64 SpotWrites[2][6]={};
 double SpotRelaxLogAt[2]={-1.,-1.};
 TMap<int32,double> ShotSpotAt;
 struct FLostSpot{int32 Team=0;FVector At=FVector::ZeroVector;double Until=0.,Written=-1.;};
 TMap<int32,FLostSpot> LostSpots;
 struct FHQSpot{FVector At=FVector::ZeroVector;double Taken=0.;};
 TArray<FHQSpot> HQSpots[2];
 double HQTakenAt[2]={0.,0.};
 void ResetSpotRelax();
 void AdvanceLostSpots(int32 Team,double Now);
 bool NoteHeardShotSpot(int32 ShooterID,int32 Team,const FVector& At,double Now,float Error,float Every,TFunctionRef<bool()> CanHear);
 static bool ShotSpotListener(int32 Team,int32 ListenerTeam,bool bAlive,bool bVehicle,const FVector& At,const FVector& Listener,float Hear);
 static FVector ShotSpotError(int32 ShooterID,const FVector& At,double Now,float Error,float Every);
 static bool SpotDepthAllows(const FVector& At,const FVector& Home,const FVector& EnemyHome,int32 Depth);
 void AdvanceHQReports(int32 Team,double Now,float Interval,float Delay,int32 Min,int32 Depth,const FVector& Home,const FVector& EnemyHome,const TMap<FIntPoint,int32>& Counts);
 void RefreshKnownSpots(int32 Team);
 void EndSeek(int32 SquadID);
 void SupplyTripEnd(int32 SquadID, bool bResumed);
 TMap<int32, TArray<TPair<double, float>>> DamageHistory;
 double TotalStepMs = 0, MaxStepMs = 0;
 int64 StepCount = 0;
};
