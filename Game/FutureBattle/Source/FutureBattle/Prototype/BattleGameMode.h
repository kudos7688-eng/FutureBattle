#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BattleGameMode.generated.h"
class ABattleUnit;

UCLASS()
class ABattleGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ABattleGameMode();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	UPROPERTY() TArray<TObjectPtr<ABattleUnit>> Units;
	bool bLeaderCommand = false;
	bool bCombatPaused = false;
	bool bBattleEnded = false;
	bool bScaleTest = false;
	bool bSplitDecisionLoad = false;
	bool IsVisibilityFrame(double Now) const { return LastVisibilityTime == Now || Now >= NextVisibilityUpdate; }
	int32 ScaleUnitCount = 20;
	FString Result;
	double CombatStartsAt = 8.;
	void RefreshVisibility(int32 Slice = 0, int32 SliceCount = 1);
	void RunSmokeTest();
	void RunThirdPersonSmokeTest();
	void RunDelegationSmokeTest();
 void RunSquadTacticsSmokeTest();
	void RunCornerTrial();
	void RunWeaponPreview();
	void RunLargeFieldTest();
	int32 WeaponPreviewStage = 0;
	ABattleUnit* FindUnitByID(int32 ID) const;
protected:
	virtual int32 InitialUnitCount() const { return bScaleTest ? ScaleUnitCount : 20; }
	virtual float InitialUnitHealth() const { return 0; }
private:
	double LastVisibilityTime = -1.;
	double NextVisibilityUpdate = 0.;
	int32 VisibilitySlice = 0;
	bool bBenchmarkMode = false, bBenchmarkStarted = false, bBenchmarkFinished = false, bBenchmarkStableLoad = false;
	int32 BenchmarkUnitCount = 20, BenchmarkSeed = 7, BenchmarkTraining = 1, BenchmarkOpponentTraining = 1;
	float BenchmarkSeconds = 20.f;
	FString BenchmarkReport;
	FString BenchmarkCourse;
	UPROPERTY() TObjectPtr<class UBattleSquadTrial> SquadTrial;
	bool bMassMovement = false;
	UPROPERTY() TObjectPtr<class UBattleMassTrial> MassTrial;
	UPROPERTY() TObjectPtr<class UBattleMoveCourse> MoveCourse;
	void ConfigureBenchmark();
	bool PrepareBenchmarkOrders();
	bool bBenchmarkOrdersReady = false;
	int32 BenchmarkAdjustedSquads = 0;
	void UpdateBenchmark();
	void UpdateMatchSoak();
	bool bMatchSoakSaved = false, bMatchSoakComplete = false;
	float MatchSoakNextOrder = 0;
	bool bSmokeMode = false;
	int32 SmokeFailures = 0;
	FVector SmokeStart;
	void CheckSmoke(bool bPass, const TCHAR* Description);
	void FinishSmokeTest();
	void RunOrderSmokeTest();
	void CompleteSmokeTest();
	void RunPerceptionSmokeTest();
	void RunPerceptionOptimizationSmokeTest();
	void RunTacticsSmokeTest();
	void RunTacticalSmokeTest();
	void RunSquadSmokeTest();
	void RunAbilitySmokeTest();
	void RunMatchSmokeTest();
	void RunObjectiveSmokeTest();
	void RunSaveSmokeTest();
	void RunSupportSmokeTest();
	void RunScaleSmokeTest();
	int32 ScaleSmokeStage = 0, ScaleSmokeAmmo = 0, ScaleSmokeReserve = 0;
	float ScaleSmokeHP = 0;
	TArray<FVector> ScaleSmokeStarts;
	UPROPERTY() TArray<TObjectPtr<ABattleUnit>> SquadSmokeUnits;
	int32 SquadSmokeStage = 0;
	int32 SquadSmokeMover = INDEX_NONE;
	int32 SquadSmokeSupport = INDEX_NONE;
	int32 SquadSmokeAmmo = 0;
	int32 SquadSmokeRevision = 0;
	FVector SquadSmokePosition;
	int32 TacticalSmokeStage = 0;
	int32 TacticalSmokeSlot = INDEX_NONE;
	int32 TacticalSmokeAmmo = 0;
	int32 TacticalSmokeReserve = 0;
	FVector TacticalSmokePosition;
	int32 OrderSmokeStage = 0;
	int32 OrderSmokeAmmo = 0;
	FVector OrderSmokePosition;
};
