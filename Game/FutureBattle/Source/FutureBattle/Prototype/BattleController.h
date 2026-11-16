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
struct FBattleSquadSummary;
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

// Command UI notification log (ROADMAP 2026-11-15 command UI design): events the player's side has observed, newest first,
// at the left of the tactical screen above the minimap. A click on a row moves the tactical camera to the event.
struct FBattleNoticeEntry
{
 FString Text; FVector Where=FVector::ZeroVector; double Time=0; int32 Count=1;
 uint8 Kind=0; // 0 home hit, 1 point taken, 2 point lost, 3 squad stopped, 4 out of ammunition, 5 enemy home weak
};
struct FBattleNoticeLayout
{
 FVector2D Origin=FVector2D(20,262); float Width=400, RowHeight=22, Header=22; int32 Rows=0;
 // Fits between the single-line notice area and the minimap; too small a window shows fewer rows.
 static FBattleNoticeLayout Make(FVector2D View,const UWorld* World,int32 Entries,float Top=262.f);
 int32 RowAt(FVector2D P) const;
};
// Command bar (command UI 2026-11-16): buttons on the bottom panel that call the same actions as the keys.
struct FBattleCommandBarLayout
{
 FVector2D Origin=FVector2D::ZeroVector; float ButtonWidth=124, ButtonHeight=26, Gap=6;
 static constexpr int32 Count=9; // move, attack move, defend, hold, manual command, delegate to AI, buy a rifle squad, heavy infantry, a scout pair
 static FBattleCommandBarLayout Make(FVector2D View){FBattleCommandBarLayout L;L.Origin=FVector2D(34,View.Y-90);return L;}
 int32 ButtonAt(FVector2D P) const
 {
  if(P.Y<Origin.Y||P.Y>=Origin.Y+ButtonHeight||P.X<Origin.X) return INDEX_NONE;
  const int32 I=FMath::FloorToInt((P.X-Origin.X)/(ButtonWidth+Gap));
  return I<Count&&P.X<Origin.X+I*(ButtonWidth+Gap)+ButtonWidth?I:INDEX_NONE;
 }
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
 // Squad list view (command UI 2026-11-16): filter 0 all, 1 engaged, 2 home supply, 3 other active, 4 stopped;
 // sort 0 squad number, 1 heaviest losses first, 2 by nearest objective. The HUD and clicks both use SquadListRows().
 uint8 SquadFilter=0, SquadSort=0;
 uint8 SquadCategory(const FBattleSquadSummary& Row) const;
 TArray<FBattleSquadSummary> SquadListRows() const;
 double LastRowClickTime=-10; int32 LastRowClickID=INDEX_NONE;
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
	void HireScout();
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
	// Notification log (newest first, at most 32 kept; the HUD draws those of the last 60 s).
	TArray<FBattleNoticeEntry> NoticeLog;
	void PushNotice(uint8 Kind,const FString& Text,FVector Where);
	TArray<const FBattleNoticeEntry*> VisibleNotices() const;
	void UpdateNoticeLog(bool bForce=false);
	// Command bar button (index as in FBattleCommandBarLayout).
	void PressCommandButton(int32 Index);
	// Last distinct decision reasons per own squad, newest first (at most 3), for the selected squad detail.
	TMap<int32,TArray<FString>> ReasonHistory;
	// Top of the notification log: below the delegation and squad detail panels while they are shown.
	float NoticeLogTop() const {return CanShowDelegation()?440.f:262.f;}
	// Gamepad (Xbox controller, user decision 2026-11-16). Buttons are read as the keyboard/mouse key with the same action
	// (tactical: A=left click, X=right click, Y=T, B=H, RB=G, LB=Q ...; direct: RT=fire, A=jump, X=reload, B=stance, Y=cover),
	// so every existing action and panel works unchanged. Tactical: the right stick moves the mouse cursor (virtual cursor) and
	// the left stick pans; direct: left stick moves, right stick looks with a light aim assist. X held in direct control is
	// reserved for a future interact action. bGamepadActive switches the HUD hints to pad buttons.
	bool bGamepadActive=false;
	FKey GamepadToKey(const FKey& Key) const;
	void UpdateGamepad(float DeltaTime);
	float AimAssistScale() const;
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
 // A left press that started on the minimap (moves the tactical camera); its release is swallowed too.
 bool bMinimapPress=false;
 int32 PressedSquadID=INDEX_NONE;
 FDelegateHandle LeaderDeathHandle;
 // Notification polling: what the player's side last knew (own home HP, observed point owners, own squads' state).
 double NextNoticePoll=0; float NoticeElapsed=-1; float LastHomeHP=-1; TArray<int32> LastObservedOwner; TMap<int32,bool> LastSquadActive; TSet<int32> AmmoWarned; bool bEnemyHomeWeakNoted=false;
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
