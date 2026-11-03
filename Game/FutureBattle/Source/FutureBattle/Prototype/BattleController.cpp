#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
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
	const float Height = FMath::Clamp(Current.Z + Delta, 1000.f, 4500.f);
	Camera->SetRelativeLocation(FVector(-Height * .65f, 0, Height));
}

void ABattleController::BeginPlay()
{
	Super::BeginPlay();
	TacticalPawn = GetWorld()->SpawnActor<ABattleCameraPawn>(FVector(-2100, 0, 0), FRotator::ZeroRotator);
	EnterTactical();
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
	Button(TEXT("Hold"), EKeys::H, &ABattleController::HoldSelected);
	Button(TEXT("Reload"), EKeys::R, &ABattleController::Reload);
	Button(TEXT("ToggleCombat"), EKeys::P, &ABattleController::ToggleCombat);
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
		P.X = FMath::Clamp(P.X, -3500.f, 3500.f); TacticalPawn->SetActorLocation(P);
	}
	else if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
		if (Unit->IsAlive()) Unit->AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Axis);
}

void ABattleController::Right(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (bTactical && TacticalPawn)
	{
		FVector P = TacticalPawn->GetActorLocation() + FVector(0, Axis * 1800 * GetWorld()->GetDeltaSeconds(), 0);
		P.Y = FMath::Clamp(P.Y, -2500.f, 2500.f); TacticalPawn->SetActorLocation(P);
	}
	else if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
		if (Unit->IsAlive()) Unit->AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Axis);
}

void ABattleController::LookYaw(const FInputActionValue& Value) { if (!bTactical) AddYawInput(Value.Get<float>() * .8f); }
void ABattleController::LookPitch(const FInputActionValue& Value) { if (!bTactical) AddPitchInput(-Value.Get<float>() * .8f); }

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
	if (!Unit || !Unit->IsAlive() || Unit->GetState()->Team != 0) return false;
	if (!bTactical) EnterTactical();
	const FBattleUnitState Before = *Unit->GetState();
	if (AAIController* AI = Cast<AAIController>(Unit->GetController())) { Unit->SavedAI = AI; AI->StopMovement(); AI->UnPossess(); }
	Possess(Unit);
	Unit->SetDirectControl(true);
	SetControlRotation(FRotator(0, Unit->GetActorRotation().Yaw, 0));
	LastControlled = Unit;
	SetSelection(Unit);
	bTactical = false;
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
	else if (LastControlled && LastControlled->IsAlive()) ControlUnit(LastControlled);
	else ShowNotice(TEXT("Select a friendly unit first"));
}
void ABattleController::ControlSelected() { if (bTactical && !Selected.IsEmpty()) ControlUnit(Selected[0]); }
void ABattleController::SetSelection(ABattleUnit* Unit) { Selected.Reset(); if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0) Selected.Add(Unit); }

void ABattleController::SelectAll()
{
	if (!bTactical) return;
	Selected.Reset();
	if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0) Selected.Add(Unit);
}

void ABattleController::PrimaryPressed()
{
	bTriggerHeld = true;
	GetPointerPosition(DragOrigin);
	if (!bTactical)
		if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
			if (Unit->IsAlive())
				if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
					if (!Mode->bBattleEnded) Unit->FireAt(Unit->FirstPersonCamera->GetComponentLocation(), GetControlRotation().Vector(), true);
}

bool ABattleController::InputKey(const FInputKeyEventArgs& Params)
{
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
				if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && ProjectWorldLocationToScreen(Unit->GetActorLocation(), Screen) && Rect.IsInside(Screen)) Selected.AddUnique(Unit);
			}
	}
	else
	{
		FHitResult Hit;
		GetHitResultAtScreenPosition(End, ECC_Visibility, false, Hit);
		if (ABattleUnit* Unit = Cast<ABattleUnit>(Hit.GetActor()))
			if (Unit->IsAlive() && Unit->GetState()->Team == 0) Selected.AddUnique(Unit);
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_INPUT Selection=%d"), Selected.Num());
}

void ABattleController::MoveSelected()
{
	if (!bTactical || Selected.IsEmpty()) return;
	FHitResult Hit;
	FVector2D Position;
	if (GetPointerPosition(Position) && GetHitResultAtScreenPosition(Position, ECC_Visibility, false, Hit)) IssueMoveAt(Hit.ImpactPoint);
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
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) { ShowNotice(TEXT("Navigation is not ready")); return; }
	int32 Issued = 0;
	const int32 Columns = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Selected.Num())));
	for (int32 Index = 0; Index < Selected.Num(); ++Index)
	{
		ABattleUnit* Unit = Selected[Index];
		if (!Unit || !Unit->IsAlive()) continue;
		const FVector Offset((Index / Columns) * 140.f, (Index % Columns - (Columns - 1) * .5f) * 140.f, 0);
		FNavLocation Location;
		if (!Nav->ProjectPointToNavigation(Target + Offset, Location, FVector(180, 180, 400))) continue;
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Unit->GetActorLocation(), Location.Location, Unit);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		GetWorld()->GetSubsystem<UBattleState>()->SetOrder(Unit->GetUnitID(), EBattleOrder::Move, Location.Location);
		if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->MoveToLocation(Location.Location, 55.f, true, true, false, true, nullptr, false);
		DrawDebugCircle(GetWorld(), Location.Location + FVector(0, 0, 8), 50, 24, FColor(70, 240, 210), false, 2.f, 0, 3.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		++Issued;
	}
	ShowNotice(Issued ? FString::Printf(TEXT("MOVE ORDER / %d UNITS"), Issued) : TEXT("No reachable ground at this location"));
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_INPUT MoveOrders=%d"), Issued);
}

void ABattleController::HoldSelected()
{
	if (!bTactical) return;
	for (ABattleUnit* Unit : Selected) if (Unit && Unit->IsAlive())
	{
		GetWorld()->GetSubsystem<UBattleState>()->SetOrder(Unit->GetUnitID(), EBattleOrder::Hold, Unit->GetActorLocation());
		if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->StopMovement();
	}
	ShowNotice(TEXT("HOLD POSITION"));
}

void ABattleController::Reload() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) GetWorld()->GetSubsystem<UBattleState>()->StartReload(U->GetUnitID(), GetWorld()->GetTimeSeconds()); }
void ABattleController::JumpPressed() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) if (U->IsAlive()) U->Jump(); }
void ABattleController::JumpReleased() { if (ABattleUnit* U = Cast<ABattleUnit>(GetPawn())) U->StopJumping(); }
void ABattleController::ZoomIn() { if (bTactical && TacticalPawn) TacticalPawn->Zoom(-220); }
void ABattleController::ZoomOut() { if (bTactical && TacticalPawn) TacticalPawn->Zoom(220); }
void ABattleController::RestartBattle() { UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), true); }
void ABattleController::ToggleCombat()
{
	if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>())
	{
		Mode->bCombatPaused = !Mode->bCombatPaused;
		for (ABattleUnit* Unit : Mode->Units) if (Unit) if (AAIController* AI = Cast<AAIController>(Unit->GetController())) AI->StopMovement();
		ShowNotice(Mode->bCombatPaused ? TEXT("AI PAUSED / practice controls freely") : TEXT("AI RESUMED"));
	}
}
void ABattleController::ShowNotice(const FString& Text) { Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds() + 3.; }
void ABattleController::BattleVerify() { if (ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>()) Mode->RunSmokeTest(); }

void ABattleController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	Selected.RemoveAll([](const TObjectPtr<ABattleUnit>& Unit) { return !Unit || !Unit->IsAlive(); });
	if (ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn()))
	{
		if (!Unit->IsAlive()) { EnterTactical(); ShowNotice(TEXT("UNIT DOWN / select another ally")); }
		else if (bTriggerHeld)
		{
			ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
			if (!Mode || !Mode->bBattleEnded) Unit->FireAt(Unit->FirstPersonCamera->GetComponentLocation(), GetControlRotation().Vector(), true);
		}
	}
}
