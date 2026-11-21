#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "BattleState.h"
#include "InputKeyEventArgs.h"
#include "BattleController.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ABattleUnit;
struct FBattleSquadSummary;
struct FBattleSquadListRow;
struct FInputActionValue;

UCLASS()
class ABattleCameraPawn : public APawn
{
	GENERATED_BODY()
public:
	ABattleCameraPawn();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	void Zoom(float Delta);
	// Wheel zoom: scales the height by a factor (>1 out, <1 in), so the step grows with height and
	// the whole range (1000 .. CameraMaxHeight, up to 160000) takes about 20 ticks instead of ~260.
	void ZoomScale(float Factor);
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
 static constexpr int32 Count=16;
 struct FButton { FVector2D Origin=FVector2D::ZeroVector; float Width=0, Scale=.85f; FString Caption; };
 FButton Buttons[Count];
 float ButtonHeight=26, PanelTop=0, SummaryY=0;
 FVector2D RouteOrigin=FVector2D::ZeroVector; float RouteWidth=176;
 // Reserve the larger keyboard/pad label so changing input devices never moves a button.
 static FBattleCommandBarLayout Make(FVector2D View,const UWorld* World,bool bPad=false);
 bool RouteAt(FVector2D P) const {return P.X>=RouteOrigin.X&&P.X<RouteOrigin.X+RouteWidth&&P.Y>=RouteOrigin.Y&&P.Y<RouteOrigin.Y+ButtonHeight;}
 int32 ButtonAt(FVector2D P) const
 {
  for(int32 I=0;I<Count;++I){const auto& B=Buttons[I];if(P.X>=B.Origin.X&&P.X<B.Origin.X+B.Width&&P.Y>=B.Origin.Y&&P.Y<B.Origin.Y+ButtonHeight)return I;}
  return INDEX_NONE;
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
 // Squad list view (command UI 2026-11-16; tabs by group 2026-11-20, user proposal): filter 0 groups (each own group as a header row
 // followed by its active squads), 1 ungrouped active squads, 2 stopped squads, 3 all squads. Sort (inside each group) 0 squad number,
 // 1 heaviest losses first, 2 by nearest objective. The HUD and clicks both use SquadListRows(). Clicking a group header selects the group.
 uint8 SquadFilter=0, SquadSort=0;
 uint8 SquadCategory(const FBattleSquadSummary& Row) const; // 0 front army, 1 attack army (other own groups), 2 unassigned, 3 stopped (never listed)
 // The squad being dragged from the list toward an army tab (2026-11-21), or INDEX_NONE.
 int32 DraggedSquadID() const { return bSquadListPress&&PressedSquadID>=0?PressedSquadID:INDEX_NONE; }
 // The squads a drag carries: every selected own squad when the dragged one is selected, else that squad alone.
 TArray<int32> DraggedSquads() const;
 TArray<FBattleSquadListRow> SquadListRows() const;
 void SortSquadRows(TArray<FBattleSquadSummary>& Rows) const;
 // Own groups shown in the group tab, by group number.
 TArray<int32> OwnGroupIDs() const;
 void SelectGroup(int32 GroupID);
 double LastRowClickTime=-10; int32 LastRowClickID=INDEX_NONE;
 void SelectSquadListRow(FVector2D Position);
 int32 SquadListIDAt(FVector2D Position) const;
	bool ControlUnit(ABattleUnit* Unit);
	void ToggleMode();
	void ControlSelected();
	void IssueMoveAt(FVector Target);
	void IssueOrderAt(FVector Target, EBattleOrder Order);
	void SelectAll();
	// Squad groups (BattleGroups.cpp): F forms a group from the selected squads (or disbands it when the selection is exactly one group); V selects the whole groups of the selected squads.
	void FormGroupSelected();
	void SelectGroupSelected();
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
	// Tactical view overlays (display only, ABattleHUD::DrawFieldOverlay): M = sector grid, N = fog of war.
	bool bShowSectorGrid = true, bShowFog = true;
	void ToggleSectorGrid();
	void ToggleFogView();
	// Range rings of seen enemy squads (L, display only, ABattleHUD::DrawRangeRings): off = only the enemy under the pointer.
	bool bShowEnemyRanges = false;
	void ToggleEnemyRanges();
	void CycleTrainingSelected();
	void HireRecruit();
	void HireRegular();
	void HireVeteran();
	void HireScout();
	void HireAntiTank();
	void HireTank();
	void HireArmoredCar();
	void HireMortar();
	void HireSPGun();
	void HireExo();
	void HireSupply();
	void StartMatch();
	void SaveMatch();
	void LoadMatch();
 FString QuickSaveSlot=TEXT("FutureBattleQuick01"); // Smoke fixtures inject the whitelisted test slot.
	void ResetMatchControl();
	void RequestSupport();
	bool IsTactical() const { return bTactical; }
	// Play-speed toggle (tactical view, standalone only): [.] cycles x1/x2/x4 via global time dilation; direct control always runs at x1.
	void CycleGameSpeed();
	// Route choice (BattlePlayerRoutes.cpp): the next of automatic, centre, north, south for the selected squads.
	void CycleRouteSelected();
	// The route the selected squads share: 0-3, or -1 when nothing is selected or the choices differ.
	int32 SelectedRouteLane() const;
	// Player input helpers (BattlePlayerPath.cpp): the point an order is given at (upper floors behind their walls, Alt for the
	// ground floor), a way drawn by dragging the right button, the camera speed, and vaulting over a low obstacle.
	bool PickOrderPoint(FVector2D Screen, FVector& Out, int32* Level = nullptr);
	void BeginPathDraw();
	void UpdatePathDraw();
	void EndPathDraw();
	bool IsDrawingPath() const { return bPathDraw; }
	bool IsDrawingFrontLine() const { return bPathDraw && bFrontDraw; } // Ctrl + right drag: the front line (UBattleMatch::SetDrawnFrontLine)
	const TArray<FVector>& GetDrawnPath() const { return DrawnPath; }
	float PanSpeed() const;
	bool TryVault(ABattleUnit* Unit);
	void UpdateVault(float DeltaTime);
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
 bool IsGamepadMenuOpen() const {return bTactical&&bGamepadMenuOpen;}
 int32 GetGamepadMenuRow() const {return GamepadMenuRow;}
 bool IsGroundFloorPending() const {return bGroundFloorPending;}
	FKey GamepadToKey(const FKey& Key) const;
	void UpdateGamepad(float DeltaTime);
	float AimAssistScale() const;
	float ViewMagnification() const;
	// Direct control: the right button is held (aim), and the view level it gives (1 aim, 2 scope x4, 3 scope x8).
	bool bAimHeld = false;
	int32 AimLevel = 1;
	UFUNCTION(Exec) void BattleVerify();
private:
 bool bGamepadMenuOpen=false, bGroundFloorPending=false;
 int32 GamepadMenuRow=0;
 TSet<FKey> GamepadMenuKeys;
 bool HandleGamepadMenu(const FInputKeyEventArgs& Params);
 void ConfirmGamepadMenu();
 // Preserve subpixel motion; a different mouse position resynchronizes the virtual cursor.
 FVector2f VirtualCursor=FVector2f::ZeroVector, LastCursorPosition=FVector2f::ZeroVector;
 bool bVirtualCursorValid=false;
 // Physical key -> logical key chosen on press. Invalid values wait for physical release after cancellation.
 TMap<FKey,FInputKeyEventArgs> HeldPlayerKeys;
 bool HandlePlayerInput(const FInputKeyEventArgs& Params);
 void CancelPlayerInput(bool bGamepadOnly=false);
	UPROPERTY() TObjectPtr<ABattleCameraPawn> TacticalPawn;
	UPROPERTY() TObjectPtr<ABattleUnit> LastControlled;
	UPROPERTY() TArray<TObjectPtr<ABattleUnit>> Selected;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
	bool bTactical = true;
	int32 GameSpeed = 1;
	bool bTriggerHeld = false;
	bool bPathDraw = false, bFrontDraw = false; TArray<FVector> DrawnPath; double DrawnLength = 0; FVector2D PathScreenStart = FVector2D::ZeroVector;
	UPROPERTY() TObjectPtr<ABattleUnit> VaultUnit;
	FVector VaultFrom = FVector::ZeroVector, VaultTo = FVector::ZeroVector; double VaultPeak = 0; float VaultTime = 0, VaultDuration = .4f;
	int32 DelegationPress = INDEX_NONE;
 bool bSquadListPress=false;
 // A left press that started on the minimap (moves the tactical camera); its release is swallowed too.
 bool bMinimapPress=false;
 int32 PressedSquadID=INDEX_NONE;
 FDelegateHandle LeaderDeathHandle;
 // Notification polling: what the player's side last knew (own home HP, observed point owners, own squads' state).
 double NextNoticePoll=0; float NoticeElapsed=-1; float LastHomeHP=-1; TArray<int32> LastObservedOwner; TMap<int32,bool> LastSquadActive; TSet<int32> AmmoWarned; bool bEnemyHomeWeakNoted=false;
 // Scout reports already announced (alert and report sequence per own pair; enemy observers by pair).
 TMap<int32,int32> ScoutAlertsSeen, ScoutReportsSeen; TSet<int32> EnemyObserversNoted;
	void Forward(const FInputActionValue& Value);
	void Right(const FInputActionValue& Value);
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void PrimaryPressed();
	void PrimaryReleased();
	void Reload();
	// Direct control: empty reserve -> one magazine from the squad mate who carries the most (UBattleTuning::PlayerAmmoShareRange).
	void UpdatePlayerAmmo(class ABattleUnit* Unit);
	int32 AmmoUnit = 0, AmmoReserveSeen = 0; double NextAmmoShare = 0.;
	void JumpPressed();
	void JumpReleased();
	void ToggleCombat();
	void RestartBattle();
	void ZoomIn();
	void ZoomOut();
	void HireTraining(EBattleTraining Training);
};
