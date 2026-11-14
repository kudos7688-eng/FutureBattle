#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "BattleState.h"
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
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	void EnterTactical();
 void BindLeaderDeaths();
 void OnLeaderUnitDied(int32 ID);
 bool LeaderSelectable(ABattleUnit* Unit) const;
 int32 SquadListScroll=0;
 bool SquadPanelAtPointer() const;
 void ScrollSquadList(int32 Delta);
 void SelectSquadListRow(FVector2D Position);
 int32 SquadListIDAt(FVector2D Position) const;
	bool ControlUnit(ABattleUnit* Unit);
	void ToggleMode();
	void ControlSelected();
	void IssueMoveAt(FVector Target);
	void IssueOrderAt(FVector Target, EBattleOrder Order);
	void SelectAll();
	bool CanShowDelegation() const;
	void SetSelectedDelegated(bool bDelegated);
	void DelegateSelected() { SetSelectedDelegated(true); }
	void TakeCommandSelected() { SetSelectedDelegated(false); }
	void GetDelegationCounts(int32& Manual, int32& Automatic) const;
	int32 DelegationButtonAt(FVector2D Position) const;
	bool DelegationPanelAt(FVector2D Position) const;
	void HoldSelected();
	void ChooseAttackMove();
	void ChooseDefend();
	EBattleOrder PendingOrder = EBattleOrder::Move;
	bool bShowContactDebug = false;
	void ToggleContactDebug() { bShowContactDebug = !bShowContactDebug; }
	void CycleTrainingSelected();
	void HireRecruit();
	void HireRegular();
	void HireVeteran();
	void StartMatch();
	void SaveMatch();
	void LoadMatch();
 FString QuickSaveSlot=TEXT("FutureBattleQuick01"); // Smoke fixtures inject the whitelisted test slot.
	void ResetMatchControl();
	void RequestSupport();
	bool IsTactical() const { return bTactical; }
	// Play-speed toggle (tactical view, standalone only): [.] cycles x1/x2/x4 via global time dilation; direct control always runs at x1.
	void CycleGameSpeed();
	void ApplyGameSpeed();
	int32 GetGameSpeed() const { return GameSpeed; }
	bool ShowsUnspottedBodies() const;
	void RefreshBodyPresentation();
	const TArray<TObjectPtr<ABattleUnit>>& GetSelection() const { return Selected; }
	bool IsSelected(const ABattleUnit* Unit) const { return Selected.Contains(Unit); }
	void SetSelection(ABattleUnit* Unit);
	bool IsDragging() const { return bTactical && bTriggerHeld; }
	bool GetPointerPosition(FVector2D& Position) const;
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
	int32 GameSpeed = 1;
	bool bTriggerHeld = false;
	int32 DelegationPress = INDEX_NONE;
 bool bSquadListPress=false;
 int32 PressedSquadID=INDEX_NONE;
 FDelegateHandle LeaderDeathHandle;
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
	void ToggleCombat();
	void RestartBattle();
	void ZoomIn();
	void ZoomOut();
	void HireTraining(EBattleTraining Training);
};
