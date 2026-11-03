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
	virtual void Tick(float DeltaTime) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	UPROPERTY() TArray<TObjectPtr<ABattleUnit>> Units;
	bool bCombatPaused = false;
	bool bBattleEnded = false;
	FString Result;
	double CombatStartsAt = 8.;
	void RefreshVisibility();
	void RunSmokeTest();
private:
	double NextVisibilityUpdate = 0.;
	bool bSmokeMode = false;
	int32 SmokeFailures = 0;
	FVector SmokeStart;
	void CheckSmoke(bool bPass, const TCHAR* Description);
	void FinishSmokeTest();
};
