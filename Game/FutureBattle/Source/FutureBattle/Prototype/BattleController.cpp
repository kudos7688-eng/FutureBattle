#include "BattleController.h"
#include "BattleLeaderRuntime.h"
#include "BattleStance.h"
#include "BattleBlindFire.h"
#include "BattleFieldLayout.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleNavigation.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"

ABattleCameraPawn::ABattleCameraPawn()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("TacticalCamera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeLocation(FVector(-1700, 0, 2600));
	Camera->SetRelativeRotation(FRotator(-57, 0, 0));
	Camera->FieldOfView = 65;
}

void ABattleCameraPawn::Zoom(float Delta)
{
	const FVector Current = Camera->GetRelativeLocation();
	const float Height = FMath::Clamp(Current.Z + Delta, 1000.f, BattleFieldLayout::Of(GetWorld()).CameraMaxHeight);
	Camera->SetRelativeLocation(FVector(-Height * .65f, 0, Height));
}

void ABattleController::BeginPlay()
{
	Super::BeginPlay();
	TacticalPawn = GetWorld()->SpawnActor<ABattleCameraPawn>(FVector(-2100, 0, 0), FRotator::ZeroRotator);
	int32 ScaleUnits = 0; FParse::Value(FCommandLine::Get(), TEXT("BattleScaleUnits="), ScaleUnits);
	if (FParse::Param(FCommandLine::Get(), TEXT("BattleBenchmark")) || ScaleUnits > 20)
	{ TacticalPawn->SetActorLocation(FVector::ZeroVector); TacticalPawn->Zoom(1900); }
	EnterTactical();
	if (BattleFieldLayout::IsLarge(GetWorld())) { TacticalPawn->SetActorLocation(FVector(BattleFieldLayout::IsSightField(GetWorld())?-BattleFieldLayout::HomeDistance(GetWorld())+2000:-8500,0,0)); TacticalPawn->Zoom(3500); }
}

void ABattleController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(InputComponent);
	Mapping = NewObject<UInputMappingContext>(this);
	auto NewAction = [this](const TCHAR* Name, EInputActionValueType Type, FKey Key)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		Actions.Add(Action);
		Mapping->MapKey(Action, Key);
		return Action;
	};
	auto Axis = [&](const TCHAR* Name, FKey Positive, FKey Negative, auto Handler)
	{
		UInputAction* Action = NewAction(Name, EInputActionValueType::Axis1D, Positive);
		Mapping->MapKey(Action, Negative).Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
		Input->BindAction(Action, ETriggerEvent::Triggered, this, Handler);
	};
	Axis(TEXT("Forward"), EKeys::W, EKeys::S, &ABattleController::Forward);
	Axis(TEXT("Right"), EKeys::D, EKeys::A, &ABattleController::Right);
	Input->BindAction(NewAction(TEXT("LookYaw"), EInputActionValueType::Axis1D, EKeys::MouseX), ETriggerEvent::Triggered, this, &ABattleController::LookYaw);
	Input->BindAction(NewAction(TEXT("LookPitch"), EInputActionValueType::Axis1D, EKeys::MouseY), ETriggerEvent::Triggered, this, &ABattleController::LookPitch);
	auto Button = [&](const TCHAR* Name, FKey Key, auto Handler)
	{
		UInputAction* Action = NewAction(Name, EInputActionValueType::Boolean, Key);
		Input->BindAction(Action, ETriggerEvent::Started, this, Handler);
		return Action;
	};
	// Mouse button edges are handled by InputKey, so a press and release within one
	// frame cannot be lost by the sampled Enhanced Input action value.
	Button(TEXT("ToggleMode"), EKeys::Tab, &ABattleController::ToggleMode);
	Button(TEXT("ControlSelected"), EKeys::E, &ABattleController::ControlSelected);
	Button(TEXT("SelectAll"), EKeys::Q, &ABattleController::SelectAll);
	Button(TEXT("FormGroup"), EKeys::F, &ABattleController::FormGroupSelected);
	Button(TEXT("SelectGroup"), EKeys::V, &ABattleController::SelectGroupSelected);
	Button(TEXT("DelegateSelected"), EKeys::J, &ABattleController::DelegateSelected);
	Button(TEXT("TakeCommandSelected"), EKeys::K, &ABattleController::TakeCommandSelected);
	Button(TEXT("Hold"), EKeys::H, &ABattleController::HoldSelected);
	Button(TEXT("AttackMove"), EKeys::T, &ABattleController::ChooseAttackMove);
	Button(TEXT("Defend"), EKeys::G, &ABattleController::ChooseDefend);
	Button(TEXT("ContactDebug"), EKeys::F6, &ABattleController::ToggleContactDebug);
	Button(TEXT("SectorGrid"), EKeys::M, &ABattleController::ToggleSectorGrid);
	Button(TEXT("FogView"), EKeys::N, &ABattleController::ToggleFogView);
	Button(TEXT("TrainingPreset"), EKeys::F7, &ABattleController::CycleTrainingSelected);
	Button(TEXT("HireRecruit"), EKeys::One, &ABattleController::HireRecruit);
	Button(TEXT("HireRegular"), EKeys::Two, &ABattleController::HireRegular);
	Button(TEXT("HireVeteran"), EKeys::Three, &ABattleController::HireVeteran);
	Button(TEXT("HireScout"), EKeys::Four, &ABattleController::HireScout);
	Button(TEXT("HireAntiTank"), EKeys::Five, &ABattleController::HireAntiTank);
	Button(TEXT("HireTank"), EKeys::Six, &ABattleController::HireTank);
	Button(TEXT("HireArmoredCar"), EKeys::Seven, &ABattleController::HireArmoredCar);
	Button(TEXT("HireMortar"), EKeys::Eight, &ABattleController::HireMortar);
	Button(TEXT("HireSPGun"), EKeys::Nine, &ABattleController::HireSPGun);
	Button(TEXT("HireExo"), EKeys::Zero, &ABattleController::HireExo);
	Button(TEXT("HireSupply"), EKeys::Hyphen, &ABattleController::HireSupply);
	Button(TEXT("StartMatch"), EKeys::Enter, &ABattleController::StartMatch);
	Button(TEXT("SaveMatch"), EKeys::F8, &ABattleController::SaveMatch);
	Button(TEXT("LoadMatch"), EKeys::F9, &ABattleController::LoadMatch);
	Button(TEXT("RequestSupport"), EKeys::B, &ABattleController::RequestSupport);
	Button(TEXT("Reload"), EKeys::R, &ABattleController::Reload);
	Button(TEXT("ToggleCombat"), EKeys::P, &ABattleController::ToggleCombat);
	Button(TEXT("GameSpeed"), EKeys::Period, &ABattleController::CycleGameSpeed);
	// Z: V and F belong to the squad groups (select, form) on the suppression branch.
	Button(TEXT("Route"), EKeys::Z, &ABattleController::CycleRouteSelected);
	Button(TEXT("Restart"), EKeys::F5, &ABattleController::RestartBattle);
	Button(TEXT("ZoomIn"), EKeys::MouseScrollUp, &ABattleController::ZoomIn);
	Button(TEXT("ZoomOut"), EKeys::MouseScrollDown, &ABattleController::ZoomOut);
	UInputAction* Jump = Button(TEXT("Jump"), EKeys::SpaceBar, &ABattleController::JumpPressed);
	Input->BindAction(Jump, ETriggerEvent::Completed, this, &ABattleController::JumpReleased);
	if (ULocalPlayer* Local = GetLocalPlayer())
		if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Local)) Subsystem->AddMappingContext(Mapping, 10);
}

void ABattleController::Forward(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (bTactical && TacticalPawn)
	{
		FVector P = TacticalPawn->GetActorLocation() + FVector(Axis * PanSpeed() * GetWorld()->DeltaRealTimeSeconds, 0, 0);
		const float Limit=float(BattleFieldLayout::Of(GetWorld()).HalfExtent.X)-500.f;
		P.X = FMath::Clamp(P.X,-Limit,Limit); TacticalPawn->SetActorLocation(P);
	}
	else if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
		if (Unit->IsAlive()){if(FMath::Abs(Axis)>.01f)GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->CancelCoverAction(Unit->GetUnitID());Unit->AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Axis);}
}

void ABattleController::Right(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (bTactical && TacticalPawn)
	{
		FVector P = TacticalPawn->GetActorLocation() + FVector(0, Axis * PanSpeed() * GetWorld()->DeltaRealTimeSeconds, 0);
		const float Limit=float(BattleFieldLayout::Of(GetWorld()).HalfExtent.Y)-500.f;
		P.Y = FMath::Clamp(P.Y,-Limit,Limit); TacticalPawn->SetActorLocation(P);
	}
	else if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
		if (Unit->IsAlive()){if(FMath::Abs(Axis)>.01f)GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->CancelCoverAction(Unit->GetUnitID());Unit->AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Axis);}
}

// Aim / scope: the look speed drops with the magnification, so the picture moves at the same speed on screen.
float ABattleController::ViewMagnification() const { const auto* Unit = Cast<ABattleUnit>(GetPawn()); return Unit ? FMath::Max(1.f, Unit->AimMagnification()) : 1.f; }
void ABattleController::LookYaw(const FInputActionValue& Value) { if (!bTactical) AddYawInput(Value.Get<float>() * .8f / ViewMagnification()); }
void ABattleController::LookPitch(const FInputActionValue& Value) { if (!bTactical) AddPitchInput(-Value.Get<float>() * .8f / ViewMagnification()); }

void ABattleController::CycleTrainingSelected()
{
	if (GetWorld()->GetSubsystem<UBattleMatch>()->IsEnabled()) { ShowNotice(TEXT("Training is fixed at recruitment")); return; }
	if (!bTactical || Selected.IsEmpty()) return;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto Next = static_cast<EBattleTraining>((static_cast<uint8>(Selected[0]->GetState()->Training) + 1) % 3);
	TSet<int32> Squads;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) Squads.Add(Unit->GetState()->SquadID);
	int32 Count = 0;
	for (int32 ID : Squads) if (const auto* Squad = Store->FindSquad(ID))
		for (int32 Member : Squad->Members) if (Store->SetTrainingPreset(Member, Next)) ++Count;
	ShowNotice(FString::Printf(TEXT("TEST TRAINING: %s / %d UNITS / equipment unchanged"), BattleTrainingName(Next), Count));
}

void ABattleController::HireTraining(EBattleTraining Training)
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled()) return;
	// Area battle: key 3 buys heavy infantry (squad type 1); key 2 the rifle squad.
	if (Match->bArea && Training == EBattleTraining::Veteran) { Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::Heavy); ShowNotice(Match->LastMessage); return; }
	Match->HireSquad(0, Training); ShowNotice(Match->LastMessage);
}
void ABattleController::HireRecruit() { HireTraining(EBattleTraining::Recruit); }
void ABattleController::HireRegular() { HireTraining(EBattleTraining::Regular); }
void ABattleController::HireVeteran() { HireTraining(EBattleTraining::Veteran); }
// Area battle: key 4 buys a scout pair (squad type 2, at most AreaScoutLimit per team).
void ABattleController::HireScout()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::Scout); ShowNotice(Match->LastMessage);
}
void ABattleController::StartMatch()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled()) { Match->StartCombat(); ShowNotice(Match->LastMessage); }
}

void ABattleController::SaveMatch()
{
 if(const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); R->IsEnabled()&&!R->IsWideField()) { ShowNotice(TEXT("この部隊構成の保存は未対応です")); return; }
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled()) { Match->SaveMatch(QuickSaveSlot); ShowNotice(Match->LastMessage); }
}
void ABattleController::LoadMatch()
{
 if(const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); R->IsEnabled()&&!R->IsWideField()) { ShowNotice(TEXT("この部隊構成の読込は未対応です")); return; }
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled()) { Match->LoadMatch(QuickSaveSlot); ShowNotice(Match->LastMessage); }
}
void ABattleController::ResetMatchControl()
{
	EnterTactical(); Selected.Reset(); LastControlled = nullptr; bTriggerHeld = false; PendingOrder = EBattleOrder::Move;
}

void ABattleController::RequestSupport()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!Match->IsEnabled()) return;
	ABattleUnit* Observer = bTactical ? (Selected.IsEmpty() ? nullptr : Selected[0].Get()) : Cast<ABattleUnit>(GetPawn());
	if (!Observer) { ShowNotice(TEXT("Select a friendly observer first")); return; }
	Match->RequestSupport(Observer->GetUnitID()); ShowNotice(Match->LastMessage);
}

bool ABattleController::ShowsUnspottedBodies() const
{
	const auto* Unit = Cast<ABattleUnit>(GetPawn());
	return !bTactical && Unit && Unit->UsesThirdPersonView();
}

void ABattleController::RefreshBodyPresentation()
{
	auto* Leader = GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
	if (Leader->IsEnabled()) { Leader->RefreshDisplay(); return; }
	if (auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit) Unit->SetRevealed(Unit->bRevealed);
}

void ABattleController::EnterTactical()
{
	if (!TacticalPawn) return;
	if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
	{
		LastControlled = Unit;
		TacticalPawn->SetActorLocation(FVector(Unit->GetActorLocation().X, Unit->GetActorLocation().Y, 0));
		const FBattleUnitState Before = *Unit->GetState();
		Unit->SetDirectControl(false);
		UnPossess();
		if (Unit->IsAlive())
		{
			if (Unit->SavedAI) Unit->SavedAI->Possess(Unit);
			else { Unit->SpawnDefaultController(); Unit->SavedAI = Cast<AAIController>(Unit->GetController()); }
		}
		const FBattleUnitState& After = *Unit->GetState();
		ensureAlwaysMsgf(Before.UnitID == After.UnitID && Before.HP == After.HP && Before.Ammo == After.Ammo && Before.Order == After.Order && Before.Destination == After.Destination, TEXT("Unit state changed during tactical switch"));
	}
	Possess(TacticalPawn);
	bTactical = true;
	ApplyGameSpeed();
	RefreshBodyPresentation();
	bTriggerHeld = false;
	bShowMouseCursor = true;
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(false);
	SetInputMode(Mode);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
		Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
	}
}

bool ABattleController::ControlUnit(ABattleUnit* Unit)
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled() && Match->Phase != EBattleMatchPhase::Combat) return false;
	if (!Unit || !Unit->IsAlive() || Unit->GetState()->Team != 0) return false;
 if(auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); L->IsEnabled() && !L->CanControlLeader(Unit->GetUnitID())) return false;
	PendingOrder = EBattleOrder::Move;
	if (!bTactical) EnterTactical();
	const FBattleUnitState Before = *Unit->GetState();
	if (AAIController* AI = Cast<AAIController>(Unit->GetController())) { Unit->SavedAI = AI; AI->StopMovement(); AI->UnPossess(); }
	Possess(Unit);
	if (const auto* Squad = GetWorld()->GetSubsystem<UBattleState>()->FindSquad(Before.SquadID); Squad && Squad->bPlayerDelegated)
		Match->SetAllyDelegated(Before.SquadID, false);
	Unit->SetDirectControl(true);
	SetControlRotation(FRotator(0, Unit->GetActorRotation().Yaw, 0));
	LastControlled = Unit;
	SetSelection(Unit);
	bTactical = false;
	ApplyGameSpeed();
	RefreshBodyPresentation();
	bTriggerHeld = false;
	bShowMouseCursor = false;
	FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); SetInputMode(Mode);
	const FBattleUnitState& After = *Unit->GetState();
	ensureAlwaysMsgf(Before.UnitID == After.UnitID && Before.HP == After.HP && Before.Ammo == After.Ammo && Before.Order == After.Order && Before.Destination == After.Destination, TEXT("Unit state changed during possession"));
	ShowNotice(FString::Printf(TEXT("DIRECT CONTROL / UNIT %02d"), Unit->GetUnitID()));
	return true;
}

void ABattleController::ToggleMode()
{
	if (!bTactical) EnterTactical();
	else if (!Selected.IsEmpty()) ControlUnit(Selected[0]);
	else if (!GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled() && LastControlled && LastControlled->IsAlive()) ControlUnit(LastControlled);
	else ShowNotice(TEXT("Select a friendly unit first"));
}
void ABattleController::ControlSelected() { if (bTactical && !Selected.IsEmpty()) ControlUnit(Selected[0]); }
void ABattleController::SetSelection(ABattleUnit* Unit) { Selected.Reset(); if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) Selected.Add(Unit); }

void ABattleController::SelectAll()
{
	if (!bTactical) return;
	Selected.Reset();
	if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) Selected.Add(Unit);
}

void ABattleController::FormGroupSelected()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match || !Match->bArea) return;
	TArray<int32> Squads;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) Squads.AddUnique(Unit->GetState()->SquadID);
	if (Squads.IsEmpty()) { ShowNotice(TEXT("群：部隊を選択してから F")); return; }
	// The selection is exactly one whole group: disband it.
	const int32 Current = Match->GroupOf(Squads[0]);
	if (Current != INDEX_NONE && Match->Groups[Current].Squads.Num() == Squads.Num())
	{
		bool bSame = true; for (int32 ID : Squads) bSame &= Match->GroupOf(ID) == Current;
		if (bSame) { Match->DissolveGroup(Current, TEXT("player")); ShowNotice(FString::Printf(TEXT("群%d を解散"), Current)); return; }
	}
	const int32 Group = Match->FormGroup(0, Squads, true);
	ShowNotice(Group != INDEX_NONE ? FString::Printf(TEXT("群%d を編成：%d 部隊（V で群を選択）"), Group, Squads.Num()) : FString(TEXT("群を編成できません")));
}

void ABattleController::SelectGroupSelected()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>(); auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!bTactical || !Match || !Mode) return;
	TSet<int32> Wanted;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0)
		if (const int32 Group = Match->GroupOf(Unit->GetState()->SquadID); Group != INDEX_NONE) Wanted.Add(Group);
	if (Wanted.IsEmpty()) { ShowNotice(TEXT("群に入っている部隊を選択してから V")); return; }
	for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit) && Wanted.Contains(Match->GroupOf(Unit->GetState()->SquadID))) Selected.AddUnique(Unit);
	ShowNotice(FString::Printf(TEXT("群を選択：%d 部隊"), Selected.Num()));
}

bool ABattleController::CanShowDelegation() const
{
	return bTactical && GetWorld()->GetSubsystem<UBattleMatch>()->CanDelegateAllies();
}

void ABattleController::GetDelegationCounts(int32& Manual, int32& Automatic) const
{
	Manual = Automatic = 0;
	TSet<int32> IDs;
	const auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	for (const ABattleUnit* U : Selected) if (U && U->IsAlive() && U->GetState()->Team == 0) IDs.Add(U->GetState()->SquadID);
	for (int32 ID : IDs) if (const auto* S = Store->FindSquad(ID))
		{ if (S->bPlayerDelegated) ++Automatic; else ++Manual; }
}

bool ABattleController::DelegationPanelAt(FVector2D P) const
{
	return CanShowDelegation() && P.X >= 16 && P.X < 466 && P.Y >= 238 && P.Y < 332;
}

int32 ABattleController::DelegationButtonAt(FVector2D P) const
{
	if (!DelegationPanelAt(P) || P.Y < 270 || P.Y >= 300) return INDEX_NONE;
	if (P.X >= 28 && P.X < 228) return 0;
	if (P.X >= 240 && P.X < 454) return 1;
	return INDEX_NONE;
}

void ABattleController::SetSelectedDelegated(bool bDelegated)
{
	if (!CanShowDelegation()) return;
	TSet<int32> IDs;
	for (const ABattleUnit* U : Selected) if (U && U->IsAlive() && U->GetState()->Team == 0) IDs.Add(U->GetState()->SquadID);
	int32 Count = 0;
	for (int32 ID : IDs) if (GetWorld()->GetSubsystem<UBattleMatch>()->SetAllyDelegated(ID, bDelegated)) ++Count;
	PendingOrder = EBattleOrder::Move;
	ShowNotice(Count ? FString::Printf(TEXT("%d SQUADS / %s"), Count, bDelegated ? TEXT("AI COMMAND / P TO RESUME IF PAUSED") : TEXT("MANUAL COMMAND / CURRENT ORDERS RETAINED")) : TEXT("SELECT FRIENDLY SQUADS FIRST"));
}

void ABattleController::PrimaryPressed()
{
	bTriggerHeld = true;
	GetPointerPosition(DragOrigin);
	if (!bTactical)
		if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
			if (Unit->IsAlive())
				if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
					if (!Mode->bBattleEnded) Unit->FireFromPlayerView();
}

bool ABattleController::InputKey(const FInputKeyEventArgs& Params)
{
 // Gamepad: read the button as the keyboard/mouse key with the same action (see GamepadToKey).
 if(Params.Key.IsGamepadKey())
 {
  bGamepadActive=true;
  const FKey Mapped=GamepadToKey(Params.Key); if(!Mapped.IsValid()) return Super::InputKey(Params);
  FInputKeyEventArgs Translated=Params; Translated.Key=Mapped; Translated.AmountDepressed=Params.Event==IE_Released?0.f:1.f;
  const bool bResult=InputKey(Translated); bGamepadActive=true; return bResult;
 }
 // Any keyboard key or mouse button switches the HUD hints back to keyboard and mouse.
 if(Params.Key.IsMouseButton()||!Params.Key.IsAnalog()) bGamepadActive=false;
 // Direct control: hold the right button to aim; while aiming, the wheel steps the view through
 // aim (x1.6) -> scope x4 -> scope x8. Display only; the level is kept for the next time.
 if(!bTactical && Params.Key==EKeys::RightMouseButton && (Params.Event==IE_Pressed||Params.Event==IE_Released))
 { bAimHeld=Params.Event==IE_Pressed; return true; }
 if(!bTactical && bAimHeld && (Params.Key==EKeys::MouseScrollUp||Params.Key==EKeys::MouseScrollDown))
 { if(Params.Event==IE_Pressed) AimLevel=FMath::Clamp(AimLevel+(Params.Key==EKeys::MouseScrollUp?1:-1),1,3); return true; }
 // Stances (map E): X cycles stand -> crouch -> prone for the directly controlled soldier.
 if(!bTactical && Params.Key==EKeys::X && Params.Event==IE_Pressed)
  if(auto* U=Cast<ABattleUnit>(GetPawn()))
   if(auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();R->IsEnabled()&&BattleStance::Enabled(GetWorld()))
   {
    const uint8 Next=(R->StanceOf(U->GetUnitID())+1)%3;
    if(R->SetStance(U->GetUnitID(),Next,GetWorld()->GetTimeSeconds()))ShowNotice(Next==BattleStance::Prone?TEXT("PRONE / X TO STAND"):Next==BattleStance::Crouch?TEXT("CROUCHED / X FOR PRONE"):TEXT("STANDING / X TO CROUCH"));
    return true;
   }
 // F / LB swaps the hand and back mounts. Primary fire uses the equipped weapon.
 if(!bTactical && Params.Key==EKeys::F && Params.Event==IE_Pressed)
  if(auto* U=Cast<ABattleUnit>(GetPawn());U && GetWorld()->GetSubsystem<UBattleState>()->ToggleExoWeapon(U->GetUnitID()))
  {
   GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->UpdateExoLooks();
   ShowNotice(U->GetState()->bLauncherSelected ? TEXT("LAUNCHER EQUIPPED") : TEXT("RIFLE EQUIPPED"));
   return true;
  }
 if(!bTactical && Params.Key==EKeys::C && Params.Event==IE_Pressed)
 {
  if(auto* U=Cast<ABattleUnit>(GetPawn()))
  {
   auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
   if(U->GetState()->CoverPhase){R->CancelCoverAction(U->GetUnitID());ShowNotice(TEXT("COVER RELEASED"));}
   else ShowNotice(R->RequestCoverAction(U->GetUnitID(),U->GetActorLocation()+GetControlRotation().Vector()*5000)?(BattleBlindFire::PlayerEnabled()?TEXT("COVER / FIRE = BLIND FIRE / AIM + FIRE = PEEK / C OR MOVE TO RELEASE"):TEXT("COVER / FIRE TO PEEK / C OR MOVE TO RELEASE")):TEXT("NO NEARBY COVER WITH SAFE PEEK"));
   return true;
  }
 }
	FVector2D Pointer;
	const bool OverDelegation = GetPointerPosition(Pointer) && DelegationPanelAt(Pointer);
	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Pressed && OverDelegation)
		{
			DelegationPress = DelegationButtonAt(Pointer); bTriggerHeld = false; return true;
		}
		if (Params.Event == IE_Released && (DelegationPress != INDEX_NONE || OverDelegation))
		{
			if (DelegationPress != INDEX_NONE && OverDelegation && DelegationButtonAt(Pointer) == DelegationPress)
				SetSelectedDelegated(DelegationPress == 1);
			DelegationPress = INDEX_NONE; bTriggerHeld = false; return true;
		}
	}
	if (OverDelegation && Params.Key == EKeys::RightMouseButton) return true;
	if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled() && bTactical)
 {
  // Command bar: a left click on a button runs that action (its release is swallowed like the minimap's).
  if(Params.Key==EKeys::LeftMouseButton && Params.Event==IE_Pressed)
  {
   FVector2D BarPointer; int32 ViewW,ViewH; GetViewportSize(ViewW,ViewH);
   if(GetPointerPosition(BarPointer))
   {
    const auto Bar=FBattleCommandBarLayout::Make(FVector2D(ViewW,ViewH));
    if(const int32 Button=Bar.ButtonAt(BarPointer);Button!=INDEX_NONE)
    {bMinimapPress=true;bTriggerHeld=false;PressCommandButton(Button);return true;}
    // The route button stands apart from the bar and exists only on maps with a road network.
    if(Bar.RouteAt(BarPointer)&&GetWorld()->GetSubsystem<UBattleMatch>()->RoutesOn())
    {bMinimapPress=true;bTriggerHeld=false;CycleRouteSelected();return true;}
   }
  }
  // Notification log: a left click on a row moves the tactical camera to that event (its release is swallowed like the minimap's).
  if(Params.Key==EKeys::LeftMouseButton && Params.Event==IE_Pressed)
  {
   FVector2D NoticePointer; int32 ViewW,ViewH; GetViewportSize(ViewW,ViewH);
   const auto Notices=VisibleNotices(); const auto Layout=FBattleNoticeLayout::Make(FVector2D(ViewW,ViewH),GetWorld(),Notices.Num(),NoticeLogTop());
   if(GetPointerPosition(NoticePointer))
    if(const int32 Row=Layout.RowAt(NoticePointer);Row!=INDEX_NONE && Notices.IsValidIndex(Row))
    {
     bMinimapPress=true; bTriggerHeld=false;
     if(TacticalPawn) TacticalPawn->SetActorLocation(FVector(Notices[Row]->Where.X,Notices[Row]->Where.Y,TacticalPawn->GetActorLocation().Z));
     return true;
    }
  }
  // Minimap: a left click moves the tactical camera there.
  if(Params.Key==EKeys::LeftMouseButton)
  {
   FVector2D MapPointer; int32 ViewW,ViewH; GetViewportSize(ViewW,ViewH);
   const auto Map=FBattleMinimapLayout::Make(FVector2D(ViewW,ViewH),GetWorld());
   if(Params.Event==IE_Pressed && GetPointerPosition(MapPointer) && Map.Contains(MapPointer))
   {
    bMinimapPress=true; bTriggerHeld=false;
    if(TacticalPawn){const FVector Target=Map.ToWorld(MapPointer);TacticalPawn->SetActorLocation(FVector(Target.X,Target.Y,TacticalPawn->GetActorLocation().Z));}
    return true;
   }
   if(Params.Event==IE_Released && bMinimapPress){bMinimapPress=false;bTriggerHeld=false;return true;}
  }
  const bool Over=SquadPanelAtPointer();
  if(Params.Key==EKeys::MouseScrollUp || Params.Key==EKeys::MouseScrollDown)
  { if(Over) { if(Params.Event==IE_Pressed) ScrollSquadList(Params.Key==EKeys::MouseScrollUp?-3:3); return true; } }
  if(Params.Key==EKeys::LeftMouseButton)
  {
   // Header controls: a filter tab or the sort button.
   if(Params.Event==IE_Pressed && Over)
   {
    FVector2D P; int32 ViewW,ViewH; GetViewportSize(ViewW,ViewH); const auto Layout=FBattleSquadListLayout::Make(FVector2D(ViewW,ViewH));
    if(GetPointerPosition(P))
    {
     if(const int32 Tab=Layout.FilterTabAt(P);Tab!=INDEX_NONE){SquadFilter=uint8(Tab);SquadListScroll=0;bSquadListPress=false;bTriggerHeld=false;return true;}
     if(Layout.SortButtonAt(P)){SquadSort=uint8((SquadSort+1)%3);SquadListScroll=0;bSquadListPress=false;bTriggerHeld=false;return true;}
    }
   }
   if(Params.Event==IE_Pressed && Over) { bSquadListPress=true; bTriggerHeld=false; FVector2D P; PressedSquadID=GetPointerPosition(P)?SquadListIDAt(P):INDEX_NONE; return true; }
   if(Params.Event==IE_Released && (bSquadListPress || Over))
   { if(bSquadListPress && Over) { FVector2D P; if(GetPointerPosition(P) && PressedSquadID!=INDEX_NONE && SquadListIDAt(P)==PressedSquadID) SelectSquadListRow(P); } bSquadListPress=false; bTriggerHeld=false; return true; }
  }
  if(Over && Params.Key==EKeys::RightMouseButton) return true;
 }
 const bool bHandled = Super::InputKey(Params);
	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Pressed) PrimaryPressed();
		else if (Params.Event == IE_Released) PrimaryReleased();
		return true;
	}
	// The order is given on release: a click orders the point, a drag draws a way (BattlePlayerPath.cpp).
	if (Params.Key == EKeys::RightMouseButton && Params.Event == IE_Pressed) { BeginPathDraw(); return true; }
	if (Params.Key == EKeys::RightMouseButton && Params.Event == IE_Released) { EndPathDraw(); return true; }
	return bHandled;
}

void ABattleController::PrimaryReleased()
{
	if (!bTriggerHeld) return;
	bTriggerHeld = false;
	if (!bTactical) return;
	FVector2D End;
	if (!GetPointerPosition(End)) return;
	const bool bAdd = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if (!bAdd) Selected.Reset();
	if (FVector2D::Distance(End, DragOrigin) > 10)
	{
		FBox2D Rect(ForceInit); Rect += End; Rect += DragOrigin;
		if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
			for (ABattleUnit* Unit : Mode->Units)
			{
				FVector2D Screen;
				if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit) && ProjectWorldLocationToScreen(Unit->GetActorLocation(), Screen) && Rect.IsInside(Screen)) Selected.AddUnique(Unit);
			}
	}
	else
	{
		FHitResult Hit;
		GetHitResultAtScreenPosition(End, ECC_Visibility, false, Hit);
		if (ABattleUnit* Unit = Cast<ABattleUnit>(Hit.GetActor()))
			if (Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) Selected.AddUnique(Unit);
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_INPUT Selection=%d"), Selected.Num());
}

void ABattleController::MoveSelected()
{
	if (!bTactical || Selected.IsEmpty()) return;
	FVector Target;
	FVector2D Position;
	if (GetPointerPosition(Position) && PickOrderPoint(Position, Target))
	{
		IssueOrderAt(Target, PendingOrder);
		PendingOrder = EBattleOrder::Move;
	}
}

void ABattleController::ChooseAttackMove()
{
	if (bTactical) { PendingOrder = EBattleOrder::AttackMove; ShowNotice(BattleFieldLayout::Of(GetWorld()).bSectors?TEXT("区画制圧：対象区画を右クリック"):TEXT("ATTACK MOVE: right-click destination")); }
}

void ABattleController::ChooseDefend()
{
	if (bTactical) { PendingOrder = EBattleOrder::Defend; ShowNotice(BattleFieldLayout::Of(GetWorld()).bSectors?TEXT("区画維持：対象区画を右クリック"):TEXT("DEFEND: right-click center")); }
}

void ABattleController::ToggleSectorGrid()
{
	if (!bTactical) return;
	bShowSectorGrid = !bShowSectorGrid;
	ShowNotice(bShowSectorGrid ? TEXT("区画の格子：表示") : TEXT("区画の格子：非表示"));
}

void ABattleController::ToggleFogView()
{
	if (!bTactical) return;
	bShowFog = !bShowFog;
	ShowNotice(bShowFog ? TEXT("戦場の霧：表示") : TEXT("戦場の霧：非表示"));
}

bool ABattleController::GetPointerPosition(FVector2D& Position) const
{
	if (GetMousePosition(Position.X, Position.Y)) return true;
	// Slate's IsMouseAttached can be false for a virtual/remote mouse despite valid
	// viewport pointer events. Use the same cached viewport coordinates in that case.
	if (UGameViewportClient* Client = GetWorld()->GetGameViewport())
		if (FViewport* Viewport = Client->Viewport)
		{
			FIntPoint Point; Viewport->GetMousePos(Point);
			const FIntPoint Size = Viewport->GetSizeXY();
			if (Point.X >= 0 && Point.Y >= 0 && Point.X < Size.X && Point.Y < Size.Y)
			{
				Position = FVector2D(Point); return true;
			}
		}
	return false;
}

void ABattleController::IssueMoveAt(FVector Target)
{
	IssueOrderAt(Target, EBattleOrder::Move);
}

void ABattleController::IssueOrderAt(FVector Target, EBattleOrder Order)
{
 if(auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); L->IsEnabled())
 {
  int32 Accepted=0,Rejected=0,SectorMoves=0;
  auto* Routes=GetWorld()->GetSubsystem<UBattleMatch>();
  for(ABattleUnit* U:Selected) if(LeaderSelectable(U))
  {
   // A squad with a chosen route walks a distant order over it (BattlePlayerRoutes.cpp); -1 = not routed, the ordinary order.
   const int32 SID=U->GetState()->SquadID,Routed=Routes->PlayerRouteOrder(SID,Order,Target);
   const bool Sector=BattleFieldLayout::Of(GetWorld()).bSectors && !L->IsVehicle(U->GetUnitID()) && (Order==EBattleOrder::Defend || Order==EBattleOrder::AttackMove);
   // Trial -BattleSectorMove=1: a plain move click sends the squad into the sector as an attack move that fires on the move
   // (UBattleLeaderRuntime::SetSectorFireBack); a drawn line stays a point move (BattlePlayerPath.cpp).
   const bool SectorMove=Order==EBattleOrder::Move && Routed<0 && UBattleLeaderRuntime::SectorMoveEnabled() && BattleFieldLayout::Of(GetWorld()).bSectors && !L->IsVehicle(U->GetUnitID());
   if(Routed<0?L->IssueOrder(SID,SectorMove?EBattleOrder::AttackMove:Order,Target,false,Sector||SectorMove):Routed>0)
   {
    ++Accepted;
    if(SectorMove){L->SetSectorFireBack(SID);++SectorMoves;}
   }
   else ++Rejected;
  }
  const FIntPoint Cell=BattleSectors::Cell(Target);
  if(SectorMoves)UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_INPUT SectorMove cell=%c%d squads=%d rejected=%d"),TCHAR('A'+Cell.X),1+Cell.Y,SectorMoves,Rejected);
  if(SectorMoves) ShowNotice(FString::Printf(TEXT("区画移動 %c%d：%d部隊（撃たれたら移動しながら撃ち返す／不可 %d）"),TCHAR('A'+Cell.X),1+Cell.Y,Accepted,Rejected));
  else if(L->IsWideField()) ShowNotice(FString::Printf(TEXT("ORDER: %d SQUADS / %d REJECTED"),Accepted,Rejected));
  return;
 }
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled() && Match->Phase != EBattleMatchPhase::Combat) { ShowNotice(TEXT("Start the battle with ENTER")); return; }
	if (Target.ContainsNaN() || Order == EBattleOrder::Hold) return;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) { ShowNotice(TEXT("Navigation is not ready")); return; }
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Mode) return;
	TArray<int32> SquadIDs;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) SquadIDs.AddUnique(Unit->GetState()->SquadID);
	FVector Origin = FVector::ZeroVector; int32 Count = 0;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) { Origin += Unit->GetActorLocation(); ++Count; }
	if (Count) Origin /= Count;
	FVector Forward = (Target - Origin).GetSafeNormal2D(); if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
	const FVector Right(-Forward.Y, Forward.X, 0);
	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(SquadIDs.Num()))));
	if (SquadIDs.Num() > 2) SquadIDs.Sort();
	int32 Issued = 0;
	int32 Rejected = 0;
	for (int32 Group = 0; Group < SquadIDs.Num(); ++Group)
	{
		const auto* Squad = Store->FindSquad(SquadIDs[Group]);
		if (!Squad) continue;
		// Preserve the small prototype layout; large selections form a bounded grid behind the clicked front.
		const FVector Center = SquadIDs.Num() <= 2 ? Target + FVector(0, Group * 650.f, 0) :
			Target + Right * ((Group % Columns - (Columns - 1) * .5f) * 500.f) - Forward * ((Group / Columns) * 400.f);
		TMap<int32, FVector> Goals;
		for (ABattleUnit* Unit : Mode->Units)
		{
			if (!Unit || !Unit->IsAlive() || Unit->GetState()->SquadID != Squad->SquadID) continue;
			const int32 Slot = Squad->Members.IndexOfByKey(Unit->GetUnitID());
			const FVector Offset((Slot / 3) * 140.f, (Slot % 3 - 1) * 140.f, 0);
			FVector Location;
			if (BattleNavigation::FormationGoal(Unit, Center, Offset, Location)) Goals.Add(Unit->GetUnitID(), Location);
		}
		// Atomic per squad: retain the previous order if any living member has no path.
		if (!Store->SetSquadOrder(Squad->SquadID, Order, Center, Goals)) { ++Rejected; continue; }
		Store->SetSquadDelegated(Squad->SquadID, false);
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Goals.Contains(Unit->GetUnitID()))
		{
			if (ABattleAIController* AI = Cast<ABattleAIController>(Unit->GetController()))
			{
				AI->BeginOrder();
			}
			++Issued;
		}
	}
	ShowNotice(FString::Printf(TEXT("%s / %d UNITS / %d SQUADS UNREACHABLE"), BattleOrderName(Order), Issued, Rejected));
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_INPUT MoveOrders=%d"), Issued);
}

void ABattleController::HoldSelected()
{
 if(auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); L->IsEnabled())
 { for(ABattleUnit* U:Selected) if(LeaderSelectable(U)) { GetWorld()->GetSubsystem<UBattleMatch>()->ClearPlayerRouteMission(U->GetState()->SquadID); L->IssueOrder(U->GetState()->SquadID,EBattleOrder::Hold,U->GetActorLocation()); } return; }
	if (!bTactical) return;
	PendingOrder = EBattleOrder::Move;
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Mode) return;
	TSet<int32> SquadIDs;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && LeaderSelectable(Unit)) SquadIDs.Add(Unit->GetState()->SquadID);
	for (int32 ID : SquadIDs)
	{
		TMap<int32, FVector> Goals;
		FVector Center = FVector::ZeroVector;
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->SquadID == ID)
		{ Goals.Add(Unit->GetUnitID(), Unit->GetActorLocation()); Center += Unit->GetActorLocation(); }
		if (Goals.IsEmpty() || !Store->SetSquadOrder(ID, EBattleOrder::Hold, Center / Goals.Num(), Goals)) continue;
		Store->SetSquadDelegated(ID, false);
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Goals.Contains(Unit->GetUnitID()))
			if (ABattleAIController* AI = Cast<ABattleAIController>(Unit->GetController())) AI->BeginOrder();
	}
	ShowNotice(TEXT("SQUAD HOLD POSITION"));
}

void ABattleController::Reload() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) GetWorld()->GetSubsystem<UBattleState>()->StartReload(U->GetUnitID(), GetWorld()->GetTimeSeconds()); }
void ABattleController::UpdatePlayerAmmo(ABattleUnit* Unit)
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>(); auto* L = GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
	const auto* S = Unit->GetState(); if (!S) return;
	const int32 ID = S->UnitID; const double Now = GetWorld()->GetTimeSeconds();
	if (AmmoUnit != ID) { AmmoUnit = ID; AmmoReserveSeen = S->Reserve; }
	const float Range = GetDefault<UBattleTuning>()->PlayerAmmoShareRange;
	if (S->Reserve == 0 && Range > 0.f && L->IsEnabled() && Now >= NextAmmoShare)
	{
		NextAmmoShare = Now + 1.;
		if (const auto* Squad = Store->FindSquad(S->SquadID))
		{
			int32 Donor = 0, Most = 0; const FVector Here = L->Position(ID);
			for (int32 Mate : Squad->Members) if (Mate != ID)
				if (const auto* M = Store->Find(Mate); M && M->IsAlive() && M->Reserve > Most && UBattleTuning::AmmoKind(M->Weapon) == UBattleTuning::AmmoKind(S->Weapon) && FVector::DistSquared(L->Position(Mate), Here) <= FMath::Square(Range))
				{ Donor = Mate; Most = M->Reserve; }
			if (const int32 Moved = Donor ? Store->MoveReserve(Donor, ID, GetDefault<UBattleTuning>()->Capacity(S->Weapon)) : 0; Moved > 0)
			{
				UE_LOG(LogFutureBattle, Display, TEXT("PLAYER_AMMO_SHARE unit=%d from=%d rounds=%d"), ID, Donor, Moved);
				ShowNotice(FString::Printf(TEXT("部下から弾を受け取った（+%d）"), Moved)); AmmoReserveSeen = S->Reserve;
			}
		}
	}
	// Home or point refill of the soldier the player controls.
	if (S->Reserve > AmmoReserveSeen + 1) ShowNotice(FString::Printf(TEXT("弾を補充した（予備 %d）"), S->Reserve));
	AmmoReserveSeen = S->Reserve;
}
void ABattleController::JumpPressed() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) if (U->IsAlive() && !TryVault(U)) U->Jump(); }
void ABattleController::JumpReleased() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) U->StopJumping(); }
void ABattleController::ZoomIn() { if (SquadPanelAtPointer()) return; if (bTactical && TacticalPawn) TacticalPawn->Zoom(BattleFieldLayout::IsLarge(GetWorld()) ? -600 : -220); }
void ABattleController::ZoomOut() { if (SquadPanelAtPointer()) return; if (bTactical && TacticalPawn) TacticalPawn->Zoom(BattleFieldLayout::IsLarge(GetWorld()) ? 600 : 220); }
void ABattleController::RestartBattle() { UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), true); }
void ABattleController::ToggleCombat()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled() && Match->Phase != EBattleMatchPhase::Combat) return;
	if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
	{
		Mode->bCombatPaused = !Mode->bCombatPaused;
		for (ABattleUnit* Unit : Mode->Units) if (Unit) if (ABattleAIController* AI = Cast<ABattleAIController>(Unit->GetController())) AI->SuspendMovement();
		ShowNotice(Mode->bCombatPaused ? TEXT("AI PAUSED / practice controls freely") : TEXT("AI RESUMED"));
	}
}
void ABattleController::ShowNotice(const FString& Text) { Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds() + 3.; }
void ABattleController::BattleVerify() { if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>()) Mode->RunSmokeTest(); }

void ABattleController::PlayerTick(float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattlePlayerTick);
	Super::PlayerTick(DeltaTime);
	UpdateNoticeLog();
	UpdateGamepad(DeltaTime);
	UpdatePathDraw();
	UpdateVault(DeltaTime);
	Selected.RemoveAll([](const TObjectPtr<ABattleUnit>& Unit) { return !Unit || !Unit->IsAlive(); });
	if (bTactical) bAimHeld = false;
	if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
	{
		Unit->SetAimLevel(bAimHeld && Unit->IsAlive() ? AimLevel : 0);
		Unit->SetSprinting(!bAimHeld && (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift) || IsInputKeyDown(EKeys::Gamepad_LeftThumbstick)));
		if (Unit->IsAlive()) UpdatePlayerAmmo(Unit);
		if (!Unit->IsAlive()) { if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled()) OnLeaderUnitDied(Unit->GetUnitID()); else { EnterTactical(); ShowNotice(TEXT("UNIT DOWN / select another ally")); } }
		else if (bTriggerHeld)
		{
			ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
			if (!Mode || !Mode->bBattleEnded) Unit->FireFromPlayerView();
		}
		if (Unit->IsAlive()) Unit->ReloadIfEmpty();
	}
}

void ABattleController::BindLeaderDeaths()
{
 if(!LeaderDeathHandle.IsValid()) LeaderDeathHandle=GetWorld()->GetSubsystem<UBattleState>()->OnUnitDied.AddUObject(this,&ABattleController::OnLeaderUnitDied);
}
void ABattleController::OnLeaderUnitDied(int32 ID)
{
 if(!GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled()) return;
 if(auto* U=Cast<ABattleUnit>(GetPawn()); U && U->GetUnitID()==ID)
 { EnterTactical(); Selected.Reset(); LastControlled=nullptr; bTriggerHeld=false; bSquadListPress=false; ShowNotice(TEXT("隊長死亡 / 部隊停止。稼働中の隊長を選択してください")); }
 Selected.RemoveAll([this](const TObjectPtr<ABattleUnit>& U){return !LeaderSelectable(U);});
}
bool ABattleController::LeaderSelectable(ABattleUnit* U) const
{
 if(!U) return false;
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); return !L->IsEnabled() || L->CanSelectLeader(U->GetUnitID());
}
bool ABattleController::SquadPanelAtPointer() const
{
 if(!bTactical || !GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled()) return false;
 FVector2D P; int32 W,H; GetViewportSize(W,H);
 return GetPointerPosition(P) && FBattleSquadListLayout::Make(FVector2D(W,H)).Contains(P);
}
void ABattleController::ScrollSquadList(int32 Delta)
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); int32 W,H; GetViewportSize(W,H);
 const auto Layout=FBattleSquadListLayout::Make(FVector2D(W,H));
 SquadListScroll=FMath::Clamp(SquadListScroll+Delta,0,FMath::Max(0,SquadListRows().Num()-Layout.VisibleRows));
}
void ABattleController::SelectSquadListRow(FVector2D P)
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); int32 W,H; GetViewportSize(W,H);
 const int32 R=FBattleSquadListLayout::Make(FVector2D(W,H)).RowAt(P); ScrollSquadList(0);
 const auto Rows=SquadListRows(); if(R==INDEX_NONE || !Rows.IsValidIndex(R+SquadListScroll)) return;
 const auto& Entry=Rows[R+SquadListScroll]; const auto& Row=Entry.Squad;
 // Double-click (two clicks on the same row within 0.4 s): move the tactical camera to the squad leader (a group: its leaders' centre).
 { const double Now=GetWorld()->GetRealTimeSeconds();
   if(LastRowClickID==Entry.Key() && Now-LastRowClickTime<.4 && TacticalPawn)
   {
    FVector Where=FVector::ZeroVector; int32 N=0;
    if(Entry.bGroupHeader){ for(const auto& S:L->Summaries()) if(S.bActive && GetWorld()->GetSubsystem<UBattleMatch>()->GroupOf(S.SquadID)==Entry.GroupID){Where+=L->Position(S.LeaderUnitID);++N;} if(N) Where/=N; }
    else { Where=L->Position(Row.LeaderUnitID); N=1; }
    if(N) TacticalPawn->SetActorLocation(FVector(Where.X,Where.Y,TacticalPawn->GetActorLocation().Z));
   }
   LastRowClickID=Entry.Key(); LastRowClickTime=Now; }
 if(Entry.bGroupHeader) { SelectGroup(Entry.GroupID); return; }
 if(L->CanSelectLeader(Row.LeaderUnitID)) SetSelection(L->Actor(Row.LeaderUnitID));
 else ShowNotice(Row.bActive?TEXT("この隊長は操作不可です"):TEXT("この部隊は機能停止しています"));
}

int32 ABattleController::SquadListIDAt(FVector2D P) const
{
 int32 W,H; GetViewportSize(W,H);
 const int32 R=FBattleSquadListLayout::Make(FVector2D(W,H)).RowAt(P); const auto Rows=SquadListRows();
 return R!=INDEX_NONE && Rows.IsValidIndex(R+SquadListScroll)?Rows[R+SquadListScroll].Key():INDEX_NONE;
}

int32 ABattleController::SelectedRouteLane() const
{
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();int32 Lane=-1;
 for(const ABattleUnit* U:Selected) if(U && U->IsAlive() && U->GetState()->Team==0)
 {
  const int32 Own=Match->PlayerRouteLane(U->GetState()->SquadID);
  if(Lane>=0 && Lane!=Own) return -1;
  Lane=Own;
 }
 return Lane;
}

void ABattleController::CycleRouteSelected()
{
 if(!bTactical) return;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(!Match->RoutesOn()) { ShowNotice(TEXT("このマップには通り道がありません")); return; }
 TSet<int32> IDs;
 for(const ABattleUnit* U:Selected) if(U && U->IsAlive() && U->GetState()->Team==0) IDs.Add(U->GetState()->SquadID);
 if(IDs.IsEmpty()) { ShowNotice(TEXT("通り道：先に部隊を選択")); return; }
 // Differing choices start again from automatic.
 const int32 Next=(SelectedRouteLane()+1)%4;
 for(int32 ID:IDs) Match->SetPlayerRouteLane(ID,Next);
 ShowNotice(FString::Printf(TEXT("通り道：%s（%d部隊）"),BattleRouteLaneName(Next),IDs.Num()));
 UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_INPUT Route=%d Squads=%d"), Next, IDs.Num());
}

void ABattleController::CycleGameSpeed()
{
	if (GetNetMode() != NM_Standalone) { ShowNotice(TEXT("Game speed: standalone only")); return; }
	if (!bTactical) { ShowNotice(TEXT("Game speed: tactical view only (direct control runs at x1)")); return; }
	GameSpeed = GameSpeed == 1 ? 2 : GameSpeed == 2 ? 4 : 1;
	ApplyGameSpeed();
	ShowNotice(FString::Printf(TEXT("GAME SPEED x%d [.]"), GameSpeed));
}

void ABattleController::ApplyGameSpeed()
{
	// Simulation steps scale with the world delta, so AI, movement and timers all run faster together.
	if (GetNetMode() == NM_Standalone) UGameplayStatics::SetGlobalTimeDilation(GetWorld(), bTactical ? float(GameSpeed) : 1.f);
}

// ---- Notification log (command UI). Only what the player's side observes: own home HP, observed point owners, own squads,
// and the enemy home HP last seen by own soldiers. Area battles on the wide maps only.
FBattleNoticeLayout FBattleNoticeLayout::Make(FVector2D View,const UWorld* World,int32 Entries,float Top)
{
 FBattleNoticeLayout L; L.Origin.Y=Top; if(Entries<=0) return L;
 const auto Map=FBattleMinimapLayout::Make(View,World);
 const float Bottom=Map.Size.X>0?Map.Origin.Y-32.f:float(View.Y)-170.f;
 L.Rows=FMath::Clamp(FMath::FloorToInt((Bottom-L.Origin.Y-L.Header)/L.RowHeight),0,FMath::Min(6,Entries));
 return L;
}
int32 FBattleNoticeLayout::RowAt(FVector2D P) const
{
 if(Rows<=0||P.X<Origin.X||P.X>=Origin.X+Width) return INDEX_NONE;
 const float Y=P.Y-Origin.Y-Header; if(Y<0) return INDEX_NONE;
 const int32 Row=FMath::FloorToInt(Y/RowHeight); return Row<Rows?Row:INDEX_NONE;
}
void ABattleController::PushNotice(uint8 Kind,const FString& Text,FVector Where)
{
 const double Now=GetWorld()->GetTimeSeconds();
 // The same notice within 15 s is merged into one row (count and time refreshed, moved to the top).
 for(int32 I=0;I<NoticeLog.Num();++I) if(NoticeLog[I].Kind==Kind && NoticeLog[I].Text==Text && Now-NoticeLog[I].Time<15.)
 { FBattleNoticeEntry E=NoticeLog[I]; ++E.Count; E.Time=Now; E.Where=Where; NoticeLog.RemoveAt(I); NoticeLog.Insert(E,0); return; }
 FBattleNoticeEntry E; E.Text=Text; E.Where=Where; E.Time=Now; E.Kind=Kind; NoticeLog.Insert(E,0);
 if(NoticeLog.Num()>32) NoticeLog.SetNum(32);
 UE_LOG(LogTemp,Display,TEXT("BATTLE_NOTICE kind=%d text=%s"),Kind,*Text);
}
TArray<const FBattleNoticeEntry*> ABattleController::VisibleNotices() const
{
 TArray<const FBattleNoticeEntry*> Out; const double Now=GetWorld()->GetTimeSeconds();
 for(const auto& E:NoticeLog) if(Now-E.Time<=60.) Out.Add(&E);
 return Out;
}
void ABattleController::UpdateNoticeLog(bool bForce)
{
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Match||!Match->bArea||!L||!L->IsEnabled()||!BattleFieldLayout::IsSightField(GetWorld())) return;
 const double Now=GetWorld()->GetTimeSeconds(); if(!bForce&&Now<NextNoticePoll) return; NextNoticePoll=Now+.25;
 // A new match (F5 or a load to an earlier time) starts a fresh log.
 if(Match->Elapsed+1.f<NoticeElapsed){NoticeLog.Reset();LastHomeHP=-1;LastObservedOwner.Reset();LastSquadActive.Reset();AmmoWarned.Reset();ReasonHistory.Reset();bEnemyHomeWeakNoted=false;ScoutAlertsSeen.Reset();ScoutReportsSeen.Reset();EnemyObserversNoted.Reset();}
 NoticeElapsed=Match->Elapsed;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 // Own home.
 if(Match->Area.BaseHP.IsValidIndex(0))
 {
  const float HP=Match->Area.BaseHP[0];
  if(LastHomeHP>=0 && HP<LastHomeHP) PushNotice(0,FString::Printf(TEXT("本拠地が攻撃を受けている（耐久 %.0f）"),HP),Match->AreaHome(0));
  LastHomeHP=HP;
 }
 // Points as the player's side last observed them.
 const int32 N=Match->Objectives.Num();
 if(LastObservedOwner.Num()!=N){LastObservedOwner.Init(-3,N);}
 for(int32 I=0;I<N;++I)
 {
  const int32 Holder=Match->ObservedOwner(0,I),Before=LastObservedOwner[I];LastObservedOwner[I]=Holder;
  if(Before==-3||Before==Holder) continue;
  const TCHAR Name=TCHAR('A'+I); const FVector Where=Match->Objectives[I].Position;
  if(Holder==0) PushNotice(1,FString::Printf(TEXT("拠点%c を確保"),Name),Where);
  else if(Holder==1) PushNotice(2,Before==0?FString::Printf(TEXT("拠点%c を失った"),Name):FString::Printf(TEXT("拠点%c は敵が保有"),Name),Where);
  else if(Before==0) PushNotice(2,FString::Printf(TEXT("拠点%c が中立になった"),Name),Where);
 }
 // Own squads: stopped (leader lost) and out of ammunition.
 for(const auto& S:L->Summaries())
 {
  const bool* Was=LastSquadActive.Find(S.SquadID);
  if(Was && *Was && !S.bActive) PushNotice(3,FString::Printf(TEXT("%s 停止（隊長戦死）"),*S.Name),L->Position(S.LeaderUnitID));
  LastSquadActive.Add(S.SquadID,S.bActive);
  // Decision reason history for the selected squad detail (own squad state only).
  if(const auto* Sq=Store->FindSquad(S.SquadID); Sq && !Sq->TacticalPlan.Reason.IsEmpty())
  {
   auto& History=ReasonHistory.FindOrAdd(S.SquadID);
   if(History.IsEmpty()||History[0]!=Sq->TacticalPlan.Reason){History.Insert(Sq->TacticalPlan.Reason,0);if(History.Num()>3)History.SetNum(3);}
  }
  if(!S.bActive||AmmoWarned.Contains(S.SquadID)) continue;
  if(const auto* Squad=Store->FindSquad(S.SquadID))
  {
   int32 Living=0,Armed=0;for(int32 ID:Squad->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()){++Living;Armed+=U->Ammo>0||U->Reserve>0;}
   if(Living>0&&Armed==0){AmmoWarned.Add(S.SquadID);PushNotice(4,FString::Printf(TEXT("%s 弾切れ"),*S.Name),L->Position(S.LeaderUnitID));}
  }
 }
 // Scout reports: own pairs' direction alerts and reports; an enemy observer reporting near own soldiers (approximate position).
 {
  const FVector Own=Match->AreaHome(0),Axis=(Match->AreaHome(1)-Own).GetSafeNormal2D(),Side(-Axis.Y,Axis.X,0);
  auto Bearing=[&](const FVector& D)->const TCHAR*
  {
   const float F=FVector::DotProduct(D,Axis),S=FVector::DotProduct(D,Side);const float A=FMath::RadiansToDegrees(FMath::Atan2(S,F));
   static const TCHAR* const Names[]={TEXT("前方"),TEXT("右前方"),TEXT("右"),TEXT("右後方"),TEXT("後方"),TEXT("左後方"),TEXT("左"),TEXT("左前方")};
   return Names[(FMath::RoundToInt((A<0?A+360.f:A)/45.f))%8];
  };
  for(const auto& P:Match->ScoutReports)
  {
   const auto* Sq=Store->FindSquad(P.Key); if(!Sq) continue; const auto& R=P.Value;
   if(Sq->Team==0)
   {
    int32& Alert=ScoutAlertsSeen.FindOrAdd(P.Key); int32& Report=ScoutReportsSeen.FindOrAdd(P.Key);
    if(R.AlertSequence>Alert){Alert=R.AlertSequence;PushNotice(5,FString::Printf(TEXT("%s：%sに敵（規模不明）"),*Sq->DisplayName,Bearing(R.Direction)),L->Position(Sq->LeaderUnitID)+R.Direction*15000.f);}
    if(R.Sequence>Report&&!R.Positions.IsEmpty()){Report=R.Sequence;FVector C=FVector::ZeroVector;for(const FVector& W:R.Positions)C+=W;C/=R.Positions.Num();
     PushNotice(5,FString::Printf(TEXT("%s：敵 約%d人を報告"),*Sq->DisplayName,R.Positions.Num()),C);}
   }
   else if(R.Started>=0&&!EnemyObserversNoted.Contains(P.Key))
   {
    bool Near=false;for(const auto& U:Store->GetUnits()) if(U.Value.Team==0&&U.Value.IsAlive()&&FVector::DistSquared2D(L->Position(U.Key),R.Approx)<=FMath::Square(GetDefault<UBattleTuning>()->ScoutHintRadius)){Near=true;break;}
    if(Near){EnemyObserversNoted.Add(P.Key);PushNotice(2,TEXT("近くに敵の観測者がいる（おおよその位置を地図に表示）"),R.Approx);}
   }
  }
 }
 // Enemy home seen weak by own soldiers.
 if(!bEnemyHomeWeakNoted&&Match->Area.KnownEnemyBaseHP.IsValidIndex(0)&&Match->Area.KnownEnemyBaseHP[0]>=0&&Match->Area.KnownEnemyBaseHP[0]<=500)
 {bEnemyHomeWeakNoted=true;PushNotice(5,FString::Printf(TEXT("敵本拠地の耐久が低い（%.0f）"),Match->Area.KnownEnemyBaseHP[0]),Match->AreaHome(1));}
}

// ---- Squad list view (command UI): tabs by group, sort, double-click. Own squads' state and own groups only.
uint8 ABattleController::SquadCategory(const FBattleSquadSummary& Row) const
{
 if(!Row.bActive) return 2;
 return GetWorld()->GetSubsystem<UBattleMatch>()->GroupOf(Row.SquadID)!=INDEX_NONE?0:1;
}
TArray<int32> ABattleController::OwnGroupIDs() const
{
 TArray<int32> IDs; for(const auto& G:GetWorld()->GetSubsystem<UBattleMatch>()->Groups) if(G.Value.Team==0) IDs.Add(G.Key);
 IDs.Sort(); return IDs;
}
void ABattleController::SortSquadRows(TArray<FBattleSquadSummary>& Rows) const
{
 if(SquadSort==0) return;
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 TMap<int32,float> Key;
 for(const auto& Row:Rows)
 {
  float K=0;
  if(SquadSort==1)
  {
   // Heaviest losses first: share of members still alive (stopped squads last).
   int32 Living=0,Members=0; if(const auto* S=Store->FindSquad(Row.SquadID)) for(int32 ID:S->Members){++Members;if(const auto* U=Store->Find(ID);U&&U->IsAlive())++Living;}
   K=Row.bActive?float(Living)/FMath::Max(1,Members):2.f;
  }
  else
  {
   // Grouped by the objective nearest the leader (public positions), then by distance to it.
   const FVector P=L->Position(Row.LeaderUnitID); int32 Best=0; double BestD=DBL_MAX;
   for(int32 I=0;I<Match->Objectives.Num();++I){const double D=FVector::Dist2D(P,Match->Objectives[I].Position);if(D<BestD){BestD=D;Best=I;}}
   K=Best*1000000.f+float(FMath::Min(BestD,999999.0));
  }
  Key.Add(Row.SquadID,K);
 }
 Rows.StableSort([&Key](const FBattleSquadSummary& A,const FBattleSquadSummary& B){return Key[A.SquadID]<Key[B.SquadID];});
}
TArray<FBattleSquadListRow> ABattleController::SquadListRows() const
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); TArray<FBattleSquadListRow> Out;
 auto Append=[&Out](const TArray<FBattleSquadSummary>& Rows,int32 Group){for(const auto& R:Rows){FBattleSquadListRow E;E.Squad=R;E.GroupID=Group;Out.Add(E);}};
 if(SquadFilter==0)
 {
  // Each own group: a header row, then its active squads.
  for(int32 GID:OwnGroupIDs())
  {
   FBattleSquadListRow H; H.GroupID=GID; H.bGroupHeader=true; Out.Add(H);
   TArray<FBattleSquadSummary> Rows; for(const auto& Row:L->Summaries()) if(Row.bActive && Match->GroupOf(Row.SquadID)==GID) Rows.Add(Row);
   SortSquadRows(Rows); Append(Rows,GID);
  }
  return Out;
 }
 TArray<FBattleSquadSummary> Rows;
 for(const auto& Row:L->Summaries()) if(SquadFilter==3 || SquadCategory(Row)==SquadFilter) Rows.Add(Row);
 SortSquadRows(Rows); Append(Rows,INDEX_NONE);
 return Out;
}
void ABattleController::SelectGroup(int32 GroupID)
{
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); auto* Mode=GetWorld()->GetAuthGameMode<ABattleGameMode>();
 if(!Match || !Mode || !Match->Groups.Contains(GroupID)) return;
 Selected.Reset();
 for(ABattleUnit* Unit:Mode->Units) if(Unit && Unit->IsAlive() && Unit->GetState()->Team==0 && LeaderSelectable(Unit) && Match->GroupOf(Unit->GetState()->SquadID)==GroupID) Selected.AddUnique(Unit);
 ShowNotice(Selected.Num()>0?FString::Printf(TEXT("群%d を選択：%d 部隊"),GroupID,Selected.Num()):FString::Printf(TEXT("群%d に操作できる隊長がいません"),GroupID));
}

// ---- Command bar (command UI): each button calls the same action as its key.
void ABattleController::PressCommandButton(int32 Index)
{
 switch(Index)
 {
 case 0: PendingOrder=EBattleOrder::Move; ShowNotice(TEXT("移動：右クリックで目的地")); break;
 case 1: ChooseAttackMove(); break;
 case 2: ChooseDefend(); break;
 case 3: HoldSelected(); break;
 case 4: TakeCommandSelected(); break;
 case 5: DelegateSelected(); break;
 case 6: HireRegular(); break;
 case 7: HireVeteran(); break; // area battle: heavy infantry
 case 8: HireScout(); break; // area battle: scout pair
 case 9: HireAntiTank(); break; // area battle: anti-tank team
 case 10: HireTank(); break; // area battle: tank
 case 11: HireArmoredCar(); break; // area battle: armoured car
 case 12: HireMortar(); break; // area battle: mortar team
 case 13: HireSPGun(); break; // area battle: self-propelled gun
 case 14: HireExo(); break; // area battle: armoured exoskeleton soldier
 case 15: HireSupply(); break; // area battle: supply team
 default: break;
 }
}

// ---- Gamepad (Xbox controller). Buttons become the keyboard/mouse key with the same action; sticks are read every frame.
FKey ABattleController::GamepadToKey(const FKey& Key) const
{
 if(bTactical)
 {
  if(Key==EKeys::Gamepad_FaceButton_Bottom) return EKeys::LeftMouseButton;   // A: select / press a panel button
  if(Key==EKeys::Gamepad_FaceButton_Left) return EKeys::RightMouseButton;    // X: order destination
  if(Key==EKeys::Gamepad_FaceButton_Top) return EKeys::T;                    // Y: attack move
  if(Key==EKeys::Gamepad_FaceButton_Right) return EKeys::H;                  // B: hold
  if(Key==EKeys::Gamepad_RightShoulder) return EKeys::G;                     // RB: defend
  if(Key==EKeys::Gamepad_LeftShoulder) return EKeys::Q;                      // LB: select all
  if(Key==EKeys::Gamepad_LeftTrigger) return EKeys::MouseScrollDown;         // LT: zoom out
  if(Key==EKeys::Gamepad_RightTrigger) return EKeys::MouseScrollUp;          // RT: zoom in
  if(Key==EKeys::Gamepad_Special_Left) return EKeys::E;                      // View: control the selected leader
  if(Key==EKeys::Gamepad_DPad_Up) return EKeys::K;                           // manual command
  if(Key==EKeys::Gamepad_DPad_Down) return EKeys::J;                         // delegate to AI
  if(Key==EKeys::Gamepad_DPad_Left) return EKeys::Two;                       // buy a squad
  if(Key==EKeys::Gamepad_DPad_Right) return EKeys::Period;                   // game speed
  if(Key==EKeys::Gamepad_RightThumbstick) return EKeys::Enter;               // start the match
 }
 else
 {
  if(Key==EKeys::Gamepad_RightTrigger) return EKeys::LeftMouseButton;        // RT: fire
  if(Key==EKeys::Gamepad_LeftShoulder) return EKeys::F;                      // LB: exoskeleton weapon swap
  if(Key==EKeys::Gamepad_LeftTrigger) return EKeys::RightMouseButton;        // LT: aim (held)
  if(Key==EKeys::Gamepad_DPad_Up) return EKeys::MouseScrollUp;               // while aiming: more magnification
  if(Key==EKeys::Gamepad_DPad_Down) return EKeys::MouseScrollDown;           // while aiming: less
  if(Key==EKeys::Gamepad_FaceButton_Bottom) return EKeys::SpaceBar;          // A: jump
  if(Key==EKeys::Gamepad_FaceButton_Left) return EKeys::R;                   // X: reload (held: future interact)
  if(Key==EKeys::Gamepad_FaceButton_Right) return EKeys::X;                  // B: stance
  if(Key==EKeys::Gamepad_FaceButton_Top) return EKeys::C;                    // Y: cover
  if(Key==EKeys::Gamepad_Special_Left) return EKeys::Tab;                    // View: back to the tactical view
 }
 if(Key==EKeys::Gamepad_Special_Right) return EKeys::P;                      // Menu: pause AI
 return EKeys::Invalid;
}
float ABattleController::AimAssistScale() const
{
 // Light aim assist (user decision): the look speed drops near a contact the controlled soldier currently sees. No pull
 // toward the target and no unseen enemies. -BattleAimAssist=0 turns it off.
 static const bool bEnabled=[](){int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleAimAssist="),V);return V!=0;}();
 const auto* Unit=Cast<ABattleUnit>(GetPawn()); if(!bEnabled||!Unit||!PlayerCameraManager) return 1.f;
 const auto* Contacts=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(Unit->GetUnitID()); if(!Contacts) return 1.f;
 const FVector Eye=PlayerCameraManager->GetCameraLocation(),Aim=GetControlRotation().Vector();
 float Best=180.f;
 for(const auto& C:*Contacts) if(C.Value.bVisible)
 {
  const FVector To=(C.Value.LastKnownPosition+FVector(0,0,100)-Eye).GetSafeNormal();
  Best=FMath::Min(Best,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Aim,To),-1.f,1.f))));
 }
 return Best>=4.f?1.f:FMath::Lerp(.5f,1.f,Best/4.f);
}
void ABattleController::UpdateGamepad(float DeltaTime)
{
 auto Stick=[this](const FKey& Key){const float V=GetInputAnalogKeyState(Key);return FMath::Abs(V)<.2f?0.f:FMath::Sign(V)*(FMath::Abs(V)-.2f)/.8f;};
 const float LX=Stick(EKeys::Gamepad_LeftX),LY=Stick(EKeys::Gamepad_LeftY),RX=Stick(EKeys::Gamepad_RightX),RY=Stick(EKeys::Gamepad_RightY);
 if(LX==0&&LY==0&&RX==0&&RY==0) return;
 bGamepadActive=true;
 if(LY!=0) Forward(FInputActionValue(LY));
 if(LX!=0) Right(FInputActionValue(LX));
 if(bTactical)
 {
  // Virtual cursor: the right stick moves the mouse cursor, so every mouse-driven panel and order works as with a mouse.
  float X=0,Y=0; int32 W=0,H=0; GetViewportSize(W,H);
  if((RX!=0||RY!=0)&&GetMousePosition(X,Y)&&W>0)
  {
   const float Speed=1200.f*DeltaTime;
   SetMouseLocation(FMath::Clamp(FMath::RoundToInt(X+RX*FMath::Abs(RX)*Speed),0,W-1),FMath::Clamp(FMath::RoundToInt(Y-RY*FMath::Abs(RY)*Speed),0,H-1));
  }
 }
 else if(RX!=0||RY!=0)
 {
  const float Assist=AimAssistScale()/ViewMagnification();
  FRotator R=GetControlRotation();
  R.Yaw+=RX*150.f*Assist*DeltaTime;
  R.Pitch=FMath::Clamp(FRotator::NormalizeAxis(R.Pitch+RY*110.f*Assist*DeltaTime),-80.f,80.f);
  SetControlRotation(R);
 }
}
// Area battle: key 5 buys an anti-tank team (squad type 3, at most AreaATLimit per team).
void ABattleController::HireAntiTank()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::AntiTank); ShowNotice(Match->LastMessage);
}
// Area battle: key 6 buys a tank (squad type 4, at most AreaTankLimit per team).
void ABattleController::HireTank()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::Tank); ShowNotice(Match->LastMessage);
}
// Area battle: key 7 buys an armoured car (squad type 5, at most AreaACLimit per team).
void ABattleController::HireArmoredCar()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::ArmoredCar); ShowNotice(Match->LastMessage);
}
// Area battle: key 8 buys a mortar team (squad type 6, at most AreaMortarLimit per team).
void ABattleController::HireMortar()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::Mortar); ShowNotice(Match->LastMessage);
}
// Area battle: the - key buys a supply team (squad type 9, at most AreaSupplyLimit per team).
void ABattleController::HireSupply()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::Supply); ShowNotice(Match->LastMessage);
}
// Area battle: key 0 buys an armoured exoskeleton soldier (squad type 8, at most AreaExoLimit per team).
void ABattleController::HireExo()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::Exo); ShowNotice(Match->LastMessage);
}
// Area battle: key 9 buys a self-propelled gun (squad type 7, at most AreaSPGLimit per team).
void ABattleController::HireSPGun()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (!bTactical || !Match->IsEnabled() || !Match->bArea) return;
	Match->HireAreaSquad(0, EBattleTraining::Regular, false, BattleSquadType::SPGun); ShowNotice(Match->LastMessage);
}
