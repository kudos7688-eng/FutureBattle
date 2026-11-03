#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "BattleController.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ABattleUnit;
struct FInputActionValue;

UCLASS()
class ABattleCameraPawn : public APawn
{
	GENERATED_BODY()
public:
	ABattleCameraPawn();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	void Zoom(float Delta);
};

UCLASS()
class ABattleController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	void EnterTactical();
	bool ControlUnit(ABattleUnit* Unit);
	void ToggleMode();
	void ControlSelected();
	void IssueMoveAt(FVector Target);
	void SelectAll();
	bool IsTactical() const { return bTactical; }
	const TArray<TObjectPtr<ABattleUnit>>& GetSelection() const { return Selected; }
	bool IsSelected(const ABattleUnit* Unit) const { return Selected.Contains(Unit); }
	void SetSelection(ABattleUnit* Unit);
	bool IsDragging() const { return bTactical && bTriggerHeld; }
	FVector2D DragOrigin;
	double HitMarkerUntil = 0.;
	FString Notice;
	double NoticeUntil = 0.;
	void ShowNotice(const FString& Text);
	UFUNCTION(Exec) void BattleVerify();
private:
	UPROPERTY() TObjectPtr<ABattleCameraPawn> TacticalPawn;
	UPROPERTY() TObjectPtr<ABattleUnit> LastControlled;
	UPROPERTY() TArray<TObjectPtr<ABattleUnit>> Selected;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
	bool bTactical = true;
	bool bTriggerHeld = false;
	void Forward(const FInputActionValue& Value);
	void Right(const FInputActionValue& Value);
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void PrimaryPressed();
	void PrimaryReleased();
	void MoveSelected();
	void Reload();
	void JumpPressed();
	void JumpReleased();
	void HoldSelected();
	void ToggleCombat();
	void RestartBattle();
	void ZoomIn();
	void ZoomOut();
};
