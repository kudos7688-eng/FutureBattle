#include "BattleController.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
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
	const float Height = FMath::Clamp(Current.Z + Delta, 1000.f, BattleFieldLayout::IsExpanded(GetWorld()) ? 130000.f : BattleFieldLayout::IsSightField(GetWorld()) ? 100000.f : BattleFieldLayout::IsLarge(GetWorld()) ? 30000.f : 4500.f);
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
	Button(TEXT("DelegateSelected"), EKeys::J, &ABattleController::DelegateSelected);
	Button(TEXT("TakeCommandSelected"), EKeys::K, &ABattleController::TakeCommandSelected);
	Button(TEXT("Hold"), EKeys::H, &ABattleController::HoldSelected);
	Button(TEXT("AttackMove"), EKeys::T, &ABattleController::ChooseAttackMove);
	Button(TEXT("Defend"), EKeys::G, &ABattleController::ChooseDefend);
	Button(TEXT("ContactDebug"), EKeys::F6, &ABattleController::ToggleContactDebug);
	Button(TEXT("TrainingPreset"), EKeys::F7, &ABattleController::CycleTrainingSelected);
	Button(TEXT("HireRecruit"), EKeys::One, &ABattleController::HireRecruit);
	Button(TEXT("HireRegular"), EKeys::Two, &ABattleController::HireRegular);
	Button(TEXT("HireVeteran"), EKeys::Three, &ABattleController::HireVeteran);
	Button(TEXT("StartMatch"), EKeys::Enter, &ABattleController::StartMatch);
	Button(TEXT("SaveMatch"), EKeys::F8, &ABattleController::SaveMatch);
	Button(TEXT("LoadMatch"), EKeys::F9, &ABattleController::LoadMatch);
	Button(TEXT("RequestSupport"), EKeys::B, &ABattleController::RequestSupport);
	Button(TEXT("Reload"), EKeys::R, &ABattleController::Reload);
	Button(TEXT("ToggleCombat"), EKeys::P, &ABattleController::ToggleCombat);
	Button(TEXT("GameSpeed"), EKeys::Period, &ABattleController::CycleGameSpeed);
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
		FVector P = TacticalPawn->GetActorLocation() + FVector(Axis * 1800 * GetWorld()->GetDeltaSeconds(), 0, 0);
		const float Limit=BattleFieldLayout::IsExpanded(GetWorld())?39500.f:BattleFieldLayout::IsSightField(GetWorld())?29500.f:BattleFieldLayout::IsLarge(GetWorld())?11500.f:3500.f;
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
		FVector P = TacticalPawn->GetActorLocation() + FVector(0, Axis * 1800 * GetWorld()->GetDeltaSeconds(), 0);
		const float Limit=BattleFieldLayout::IsExpanded(GetWorld())?29500.f:BattleFieldLayout::IsSightField(GetWorld())?19500.f:BattleFieldLayout::IsLarge(GetWorld())?8500.f:2500.f;
		P.Y = FMath::Clamp(P.Y,-Limit,Limit); TacticalPawn->SetActorLocation(P);
	}
	else if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
		if (Unit->IsAlive()){if(FMath::Abs(Axis)>.01f)GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->CancelCoverAction(Unit->GetUnitID());Unit->AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Axis);}
}

void ABattleController::LookYaw(const FInputActionValue& Value) { if (!bTactical) AddYawInput(Value.Get<float>() * .8f); }
void ABattleController::LookPitch(const FInputActionValue& Value) { if (!bTactical) AddPitchInput(-Value.Get<float>() * .8f); }

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
	Match->HireSquad(0, Training); ShowNotice(Match->LastMessage);
}
void ABattleController::HireRecruit() { HireTraining(EBattleTraining::Recruit); }
void ABattleController::HireRegular() { HireTraining(EBattleTraining::Regular); }
void ABattleController::HireVeteran() { HireTraining(EBattleTraining::Veteran); }
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
 if(!bTactical && Params.Key==EKeys::C && Params.Event==IE_Pressed)
 {
  if(auto* U=Cast<ABattleUnit>(GetPawn()))
  {
   auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
   if(U->GetState()->CoverPhase){R->CancelCoverAction(U->GetUnitID());ShowNotice(TEXT("COVER RELEASED"));}
   else ShowNotice(R->RequestCoverAction(U->GetUnitID(),U->GetActorLocation()+GetControlRotation().Vector()*5000)?TEXT("COVER / FIRE TO PEEK / C OR MOVE TO RELEASE"):TEXT("NO NEARBY COVER WITH SAFE PEEK"));
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
  const bool Over=SquadPanelAtPointer();
  if(Params.Key==EKeys::MouseScrollUp || Params.Key==EKeys::MouseScrollDown)
  { if(Over) { if(Params.Event==IE_Pressed) ScrollSquadList(Params.Key==EKeys::MouseScrollUp?-3:3); return true; } }
  if(Params.Key==EKeys::LeftMouseButton)
  {
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
	if (Params.Key == EKeys::RightMouseButton && Params.Event == IE_Pressed) { MoveSelected(); return true; }
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
	FHitResult Hit;
	FVector2D Position;
	if (GetPointerPosition(Position) && GetHitResultAtScreenPosition(Position, ECC_Visibility, false, Hit))
	{
		IssueOrderAt(Hit.ImpactPoint, PendingOrder);
		PendingOrder = EBattleOrder::Move;
	}
}

void ABattleController::ChooseAttackMove()
{
	if (bTactical) { PendingOrder = EBattleOrder::AttackMove; ShowNotice(TEXT("ATTACK MOVE: right-click destination")); }
}

void ABattleController::ChooseDefend()
{
	if (bTactical) { PendingOrder = EBattleOrder::Defend; ShowNotice(TEXT("DEFEND: right-click center")); }
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
  int32 Accepted=0,Rejected=0;
  for(ABattleUnit* U:Selected) if(LeaderSelectable(U)) { if(L->IssueOrder(U->GetState()->SquadID,Order,Target)) ++Accepted; else ++Rejected; }
  if(L->IsWideField()) ShowNotice(FString::Printf(TEXT("ORDER: %d SQUADS / %d REJECTED"),Accepted,Rejected));
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
 { for(ABattleUnit* U:Selected) if(LeaderSelectable(U)) L->IssueOrder(U->GetState()->SquadID,EBattleOrder::Hold,U->GetActorLocation()); return; }
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
void ABattleController::JumpPressed() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) if (U->IsAlive()) U->Jump(); }
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
	Selected.RemoveAll([](const TObjectPtr<ABattleUnit>& Unit) { return !Unit || !Unit->IsAlive(); });
	if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
	{
		if (!Unit->IsAlive()) { if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled()) OnLeaderUnitDied(Unit->GetUnitID()); else { EnterTactical(); ShowNotice(TEXT("UNIT DOWN / select another ally")); } }
		else if (bTriggerHeld)
		{
			ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
			if (!Mode || !Mode->bBattleEnded) Unit->FireFromPlayerView();
		}
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
 SquadListScroll=FMath::Clamp(SquadListScroll+Delta,0,FMath::Max(0,L->Summaries().Num()-Layout.VisibleRows));
}
void ABattleController::SelectSquadListRow(FVector2D P)
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); int32 W,H; GetViewportSize(W,H);
 const int32 R=FBattleSquadListLayout::Make(FVector2D(W,H)).RowAt(P); ScrollSquadList(0);
 const auto& Rows=L->Summaries(); if(R==INDEX_NONE || !Rows.IsValidIndex(R+SquadListScroll)) return;
 const auto& Row=Rows[R+SquadListScroll];
 if(L->CanSelectLeader(Row.LeaderUnitID)) SetSelection(L->Actor(Row.LeaderUnitID));
 else ShowNotice(Row.bActive?TEXT("この隊長は操作不可です"):TEXT("この部隊は機能停止しています"));
}

int32 ABattleController::SquadListIDAt(FVector2D P) const
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); int32 W,H; GetViewportSize(W,H);
 const int32 R=FBattleSquadListLayout::Make(FVector2D(W,H)).RowAt(P); const auto& Rows=L->Summaries();
 return R!=INDEX_NONE && Rows.IsValidIndex(R+SquadListScroll)?Rows[R+SquadListScroll].SquadID:INDEX_NONE;
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
