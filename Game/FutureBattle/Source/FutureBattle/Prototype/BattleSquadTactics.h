#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleSquadTactics.generated.h"

struct FBattleSquadState;
class UBattleSave;
USTRUCT()
struct FBattleSquadObservation
{
 GENERATED_BODY()
 // Sum of living members' maximum HP (leader multiplier included). Not saved; 0 falls back to PerUnit*Living.
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
 void CaptureSave(UBattleSave& Save,double Now) const;
 void RestoreSave(const UBattleSave& Save,double Now);
 int32 FeatureMask() const {return (bEnabled?1:0)|(bST2?2:0)|(bFlank?4:0)|(bRelaxedFlank?16:0)|((FlankTeams^3)<<5)|(bCoverActions?128:0)|(bCoordination?256:0);}
 double Clock() const { return ActiveSeconds; }
 const TMap<FName,int64>& EventCounts() const { return Events; }
 const TMap<FName,double>& ActionDurations() const { return ActionSeconds; }
 // Evaluation only (never read by AI decisions, not saved): home supply trips by squad, timed on the tactical clock.
 const TMap<int32,double>& OpenSupplyTrips() const { return SupplyTripStart; }
 const TArray<double>& CompletedSupplyTripSeconds() const { return SupplyTripSeconds; }
private:
 friend class UBattleLeaderRuntime; // In-world integration fixtures exercise the same evaluator.
 friend class UBattleTeamAI; // Reassess a flank immediately if support acceptance fails.
 void Evaluate(int32 SquadID, const FBattleSquadObservation& Sample, float Dt);
 void EvaluateFlank(int32 SquadID, const FBattleSquadObservation& Sample);
 bool RetreatGoal(const FBattleSquadState& Squad, const FBattleSquadObservation& Sample, FVector& Goal);
 bool Apply(int32 SquadID, EBattleSquadTactic Action, EBattleOrder Order, FVector Goal, const FString& Reason);
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
 void SupplyTripEnd(int32 SquadID, bool bResumed);
 TMap<int32, TArray<TPair<double, float>>> DamageHistory;
 double TotalStepMs = 0, MaxStepMs = 0;
 int64 StepCount = 0;
};
