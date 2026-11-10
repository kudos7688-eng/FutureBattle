#include "BattleNetwork.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleNavigation.h"
#include "FutureBattle.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

ABattleNetGameMode::ABattleNetGameMode()
{
	PlayerControllerClass = ABattleNetController::StaticClass(); HUDClass = ABattleNetHUD::StaticClass();
}

int32 ABattleNetGameMode::InitialUnitCount() const
{
	int32 Count = 20; FParse::Value(FCommandLine::Get(), TEXT("BattleNetUnits="), Count);
	return Count == 100 ? 100 : 20;
}

float ABattleNetGameMode::InitialUnitHealth() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("BattleNetBenchmark")) ? 10000.f : 0.f;
}

void ABattleNetGameMode::BeginPlay()
{
	Super::BeginPlay(); bCombatPaused = true; CombatStartsAt = 0;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	for (ABattleUnit* Unit : Units) if (Unit) Unit->SetReplicates(false);
	for (const auto& Pair : Store->GetSquads())
	{
		TMap<int32, FVector> Goals;
		for (ABattleUnit* Unit : Units) if (Unit->GetState()->SquadID == Pair.Key) Goals.Add(Unit->GetUnitID(), Unit->GetActorLocation());
		Store->SetSquadOrder(Pair.Key, EBattleOrder::Hold, FVector::ZeroVector, Goals);
	}
}

void ABattleNetGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	int32 Count = 0; for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (Cast<ABattleNetController>(It->Get())) ++Count;
	if (Count >= 2) ErrorMessage = TEXT("This prototype has two command slots");
}

void ABattleNetGameMode::HandleStartingNewPlayer_Implementation(APlayerController* Player)
{
	auto* PC = Cast<ABattleNetController>(Player); if (!PC) return;
	bool Used[2] = {false, false};
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (const auto* Other = Cast<ABattleNetController>(It->Get()))
		if (Other != PC && Other->ServerTeam >= 0 && Other->ServerTeam < 2) Used[Other->ServerTeam] = true;
	// Recheck at assignment: multiple pending handshakes may all have passed PreLogin.
	if (Used[0] && Used[1])
	{
		const FText Reason = FText::FromString(TEXT("This prototype has two command slots"));
		if (!GameSession || !GameSession->KickPlayer(PC, Reason)) PC->Destroy();
		return;
	}
	PC->ServerTeam = Used[0] ? 1 : 0;
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_JOIN Team=%d"), PC->ServerTeam);
}

void ABattleNetGameMode::Logout(AController* Exiting)
{
	if (auto* PC = Cast<ABattleNetController>(Exiting); PC && PC->ServerTeam >= 0)
	{
		PC->ReleaseDirect(); bCombatPaused = true;
		for (ABattleUnit* Unit : Units) if (Unit) if (auto* AI = Cast<ABattleAIController>(Unit->SavedAI)) AI->SuspendMovement();
	}
	Super::Logout(Exiting);
}

void ABattleNetGameMode::Tick(float DeltaTime)
{
	int32 Connected = 0;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (const auto* PC = Cast<ABattleNetController>(It->Get()); PC && PC->ServerTeam >= 0) ++Connected;
	if (Connected == 2 && !bBattleEnded) { bCombatPaused = false; bStarted = true; }
	else bCombatPaused = true;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (auto* PC = Cast<ABattleNetController>(It->Get())) PC->TickDirect(DeltaTime);
	Super::Tick(DeltaTime);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextNetworkFrame)
	{
		NextNetworkFrame = Now + .2;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (auto* PC = Cast<ABattleNetController>(It->Get())) PC->SendView();
	}
	if (Now >= NextPilotFrame)
	{
		NextPilotFrame += .05; if (NextPilotFrame < Now) NextPilotFrame = Now + .05;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (auto* PC = Cast<ABattleNetController>(It->Get())) PC->SendPilotView();
	}
}

void ABattleNetController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		CameraPawn = GetWorld()->SpawnActor<ABattleCameraPawn>(FVector(-2100, 0, 0), FRotator::ZeroRotator);
		SetViewTarget(CameraPawn); bShowMouseCursor = true; FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false); SetInputMode(Input);
	}
}

void ABattleNetController::SendView()
{
	if (!HasAuthority() || ServerTeam < 0) return;
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleNetGameMode>(); if (!Mode) return;
	auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>();
	TSet<int32> VisibleTargets; Perception->CollectVisibleTargets(Mode->Units, ServerTeam, VisibleTargets);
	FBattleNetFrame Frame; Frame.Team = ServerTeam; Frame.Sequence = NextFrame++; Frame.bRunning = !Mode->bCombatPaused; Frame.bEnded = Mode->bBattleEnded;
	Frame.ControlledID = ServerControlled ? ServerControlled->GetUnitID() : INDEX_NONE; Frame.ControlEpoch = ControlEpoch;
	for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->GetState())
	{
		const auto* S = Unit->GetState(); const bool bOwn = S->Team == ServerTeam;
		const bool bVisible = bOwn || (Unit->IsAlive() && VisibleTargets.Contains(S->UnitID));
		if (!bVisible) continue;
		FBattleNetUnit V; V.ID = S->UnitID; V.Team = S->Team; V.HP = S->HP; V.Position = Unit->GetActorLocation(); V.Rotation = Unit->GetActorRotation();
		// Enemy inventory, orders and squad membership are not part of the observer's public view.
		if (bOwn) { V.Squad = S->SquadID; V.Ammo = S->Ammo; V.Reserve = S->Reserve; V.Order = S->Order; V.Destination = S->Destination; V.ShotSequence = S->ShotSequence; V.ReloadRemaining = FMath::Max(0., S->ReloadEndTime - GetWorld()->GetTimeSeconds()); }
		Frame.Units.Add(V);
	}
	ClientView(Frame);
}

void ABattleNetController::ClientView_Implementation(const FBattleNetFrame& Frame)
{
	if (Frame.Sequence <= View.Sequence) return;
	if (NetBenchStage == 1) { ++NetBenchViews; NetBenchPeakVisible = FMath::Max(NetBenchPeakVisible, Frame.Units.Num()); }
	const bool bFirst = View.Team == INDEX_NONE; const int32 PreviousID = View.ControlledID, PreviousEpoch = View.ControlEpoch;
	// A delayed tactical snapshot must not rewind a more recent direct-control pose.
	FBattleNetUnit LatestPilot; bool bKeepPilot = false;
	if (Frame.Sequence < LastPilotFrame && Frame.ControlledID == PreviousID && Frame.ControlEpoch == PreviousEpoch)
		for (const auto& U : View.Units) if (U.ID == PreviousID) { LatestPilot = U; bKeepPilot = true; break; }
	View = Frame;
	if (bKeepPilot) for (auto& U : View.Units) if (U.ID == PreviousID) { U = LatestPilot; break; }
	if (bFirst && CameraPawn)
	{
		CameraPawn->SetActorLocation(FVector(Frame.Team ? 2100 : -2100, 0, 0));
		CameraPawn->SetActorRotation(FRotator(0, Frame.Team ? 180 : 0, 0));
	}
	TArray<FBattleUnitState> Records; TSet<int32> Present;
	for (const auto& V : View.Units)
	{
		FBattleUnitState S; S.UnitID = V.ID; S.Team = V.Team; S.SquadID = V.Squad; S.HP = V.HP; S.Ammo = V.Ammo; S.Reserve = V.Reserve; S.Order = V.Order; S.Destination = V.Destination;
		Records.Add(S); Present.Add(V.ID);
		if (SelectedUnitID == INDEX_NONE && V.Team == Frame.Team && V.HP > 0) { SelectedSquad = V.Squad; SelectedUnitID = V.ID; }
	}
	GetWorld()->GetSubsystem<UBattleState>()->RestoreRecords(Records, {});
	for (auto It = Proxies.CreateIterator(); It; ++It) if (!Present.Contains(It.Key())) { if (It.Value()) It.Value()->Destroy(); It.RemoveCurrent(); }
	for (const auto& V : View.Units)
	{
		auto& Proxy = Proxies.FindOrAdd(V.ID);
		if (!Proxy)
		{
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Proxy = GetWorld()->SpawnActor<ABattleUnit>(V.Position, V.Rotation, Params);
			if (!Proxy) continue;
			Proxy->SetReplicates(false); Proxy->InitializeUnit(V.ID); Proxy->SetActorTickEnabled(false); Proxy->GetCharacterMovement()->DisableMovement();
			// Transport owns these poses. Keep animation velocity, without a second local physics simulation.
			Proxy->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
		Proxy->SetRevealed(true); if (V.HP <= 0) Proxy->ApplyDeath();
	}
	UpdatePilotCamera(PreviousID, PreviousEpoch);
}

void ABattleNetController::ServerOrder_Implementation(int32 SquadID, EBattleOrder Order, FVector Target, int32 Sequence)
{
	auto Reject = [&](const TCHAR* Why) { ClientOrderResult(Sequence, false, Why); UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_COMMAND Team=%d Seq=%d Accepted=0 Reason=%s"), ServerTeam, Sequence, Why); };
	if (Sequence <= LastServerRequest || Sequence > 100000000) { Reject(TEXT("Stale request")); return; }
	LastServerRequest = Sequence;
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleNetGameMode>();
	if (!Mode || ServerTeam < 0 || Mode->bCombatPaused || Mode->bBattleEnded) { Reject(TEXT("Battle is not active")); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerOrderAt < .1) { Reject(TEXT("Command rate exceeded")); return; }
	LastServerOrderAt = Now;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>(); const auto* Squad = Store->FindSquad(SquadID);
	if (!Squad || Squad->Team != ServerTeam) { Reject(TEXT("Squad is not owned by this connection")); return; }
	if (static_cast<uint8>(Order) > static_cast<uint8>(EBattleOrder::Defend) || Target.ContainsNaN() || Target.SizeSquared() > FMath::Square(10000.f)) { Reject(TEXT("Invalid order or destination")); return; }
	TMap<int32, FVector> Goals;
	for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->SquadID == SquadID)
	{
		FVector Goal = Unit->GetActorLocation(); const int32 Slot = Squad->Members.IndexOfByKey(Unit->GetUnitID());
		if (Order == EBattleOrder::Hold || BattleNavigation::FormationGoal(Unit, Target, FVector((Slot / 3) * 140, (Slot % 3 - 1) * 140, 0), Goal)) Goals.Add(Unit->GetUnitID(), Goal);
	}
	if (!Store->SetSquadOrder(SquadID, Order, Target, Goals)) { Reject(TEXT("Squad cannot reach that destination")); return; }
	for (ABattleUnit* Unit : Mode->Units) if (Unit && Goals.Contains(Unit->GetUnitID()) && Unit->GetState()->Action != EBattleAction::DirectControl) if (auto* AI = Cast<ABattleAIController>(Unit->SavedAI)) AI->BeginOrder();
	ClientOrderResult(Sequence, true, TEXT("Command accepted"));
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_COMMAND Team=%d Seq=%d Accepted=1 Squad=%d"), ServerTeam, Sequence, SquadID);
}

void ABattleNetController::ClientOrderResult_Implementation(int32 Sequence, bool bAccepted, const FString& Message)
{
	Notice = Message;
	if (Sequence == 1) bProbeOwnAccepted = bAccepted;
	if (Sequence == 2) bProbeForeignRejected = !bAccepted && Message.Contains(TEXT("not owned"));
	if (Sequence == 3) bProbeInvalidRejected = !bAccepted && Message.Contains(TEXT("Invalid order"));
	if (Sequence == 4 && !bAccepted && Message.Contains(TEXT("Stale request"))) bProbeReplayRejected = true;
	if (Sequence == 6) bProbeRateRejected = !bAccepted && Message.Contains(TEXT("rate exceeded"));
}

void ABattleNetController::SendSelectedOrder(FVector Target, EBattleOrder Order)
{
	if (SelectedSquad != INDEX_NONE) ServerOrder(SelectedSquad, Order, Target, NextRequest++);
}

bool ABattleNetController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bHandled = Super::InputKey(Params);
	if (IsLocalController() && View.ControlledID != INDEX_NONE && Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Pressed) bLocalFire = true; else if (Params.Event == IE_Released) bLocalFire = false;
		return true;
	}
	if (!IsLocalController() || Params.Event != IE_Pressed) return bHandled;
	if (Params.Key == EKeys::Tab || Params.Key == EKeys::E) { ServerControl(View.ControlledID == INDEX_NONE ? SelectedUnitID : INDEX_NONE, NextControlRequest++); return true; }
	if (View.ControlledID != INDEX_NONE)
	{
		if (Params.Key == EKeys::R) ServerReload(View.ControlEpoch);
		return bHandled;
	}
	if (Params.Key == EKeys::T) { PendingOrder = EBattleOrder::AttackMove; return true; }
	if (Params.Key == EKeys::G) { PendingOrder = EBattleOrder::Defend; return true; }
	if (Params.Key == EKeys::H) { SendSelectedOrder(FVector::ZeroVector, EBattleOrder::Hold); return true; }
	if (CameraPawn && (Params.Key == EKeys::MouseScrollDown || Params.Key == EKeys::MouseScrollUp)) { CameraPawn->Zoom(Params.Key == EKeys::MouseScrollDown ? 250 : -250); return true; }
	if (Params.Key == EKeys::LeftMouseButton || Params.Key == EKeys::RightMouseButton)
	{
		FHitResult Hit;
		FVector2D Pointer; bool bPointer = GetMousePosition(Pointer.X, Pointer.Y);
		if (!bPointer) if (auto* Client = GetWorld()->GetGameViewport()) if (Client->Viewport)
		{
			FIntPoint P; Client->Viewport->GetMousePos(P); const FIntPoint Size = Client->Viewport->GetSizeXY();
			bPointer = P.X >= 0 && P.Y >= 0 && P.X < Size.X && P.Y < Size.Y; Pointer = FVector2D(P);
		}
		if (bPointer && GetHitResultAtScreenPosition(Pointer, ECC_Visibility, false, Hit))
		{
			if (Params.Key == EKeys::LeftMouseButton)
			{
				if (auto* Unit = Cast<ABattleUnit>(Hit.GetActor())) if (Unit->GetState() && Unit->GetState()->Team == View.Team) { SelectedSquad = Unit->GetState()->SquadID; SelectedUnitID = Unit->GetUnitID(); }
			}
			else { SendSelectedOrder(Hit.ImpactPoint, PendingOrder); PendingOrder = EBattleOrder::Move; }
		}
		return true;
	}
	return bHandled;
}

void ABattleNetController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime); if (!IsLocalController()) return;
	if (CameraPawn && View.ControlledID == INDEX_NONE)
	{
		const float Forward = (IsInputKeyDown(EKeys::W) ? 1 : 0) - (IsInputKeyDown(EKeys::S) ? 1 : 0);
		const float Right = (IsInputKeyDown(EKeys::D) ? 1 : 0) - (IsInputKeyDown(EKeys::A) ? 1 : 0);
		FVector P = CameraPawn->GetActorLocation() + (CameraPawn->GetActorForwardVector() * Forward + CameraPawn->GetActorRightVector() * Right) * DeltaTime * 1800;
		P.X = FMath::Clamp(P.X, -3500.f, 3500.f); P.Y = FMath::Clamp(P.Y, -2500.f, 2500.f); CameraPawn->SetActorLocation(P);
	}
	for (const auto& V : View.Units) if (auto* Proxy = Proxies.Find(V.ID)) if (*Proxy)
	{
		const FVector OldPosition = (*Proxy)->GetActorLocation();
		const FVector NewPosition = FMath::VInterpTo(OldPosition, V.Position, DeltaTime, 12);
		(*Proxy)->SetActorLocation(NewPosition, false, nullptr, ETeleportType::TeleportPhysics);
		(*Proxy)->GetCharacterMovement()->Velocity = V.HP > 0 && DeltaTime > SMALL_NUMBER ? (NewPosition - OldPosition) / DeltaTime : FVector::ZeroVector;
		(*Proxy)->SetActorRotation(FMath::RInterpTo((*Proxy)->GetActorRotation(), V.Rotation, DeltaTime, 12));
	}
	UpdateLocalPilot(DeltaTime);
	RunNetworkProbe();
	RunAdmissionProbe();
	RunPilotProbe();
	UpdateNetworkBenchmark();
}

void ABattleNetHUD::DrawHUD()
{
	Super::DrawHUD(); auto* PC = Cast<ABattleNetController>(PlayerOwner); if (!Canvas || !PC) return;
	const float W = Canvas->SizeX, H = Canvas->SizeY; const FLinearColor Mint(.28f, .95f, .8f), Red(1, .35f, .25f), Ink(.025f, .04f, .065f, .92f);
	auto Label = [&](const FString& S, float X, float Y, FLinearColor C) { DrawText(S, C, X, Y, GEngine->GetSmallFont(), 1.1f); };
	DrawRect(Ink, 16, 16, W - 32, 72); Label(FString::Printf(TEXT("FUTURE BATTLE / TACTICAL LINK / TEAM %d"), PC->View.Team + 1), 30, 27, Mint);
	int32 Allies = 0, Visible = 0; for (const auto& U : PC->View.Units) if (U.HP > 0) { if (U.Team == PC->View.Team) ++Allies; else ++Visible; }
	Label(FString::Printf(TEXT("ALLY %d / SPOTTED %d / SQUAD %d / %s"), Allies, Visible, PC->SelectedSquad, PC->View.bEnded ? TEXT("BATTLE ENDED") : PC->View.bRunning ? TEXT("CONNECTED") : TEXT("WAITING FOR SECOND COMMANDER")), 30, 55, FLinearColor::White);
	const bool bFPS = PC->View.ControlledID != INDEX_NONE;
	if (bFPS && GetWorld()->GetTimeSeconds() >= NextLabelCheck)
	{
		NextLabelCheck = GetWorld()->GetTimeSeconds() + .1; ClearLabels.Reset();
		FVector Eye; FRotator Look; PC->GetPlayerViewPoint(Eye, Look);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleNetLabels), false);
		if (auto* Controlled = PC->Proxies.Find(PC->View.ControlledID)) Query.AddIgnoredActor(Controlled->Get());
		for (const auto& U : PC->View.Units) if (U.HP > 0 && U.ID != PC->View.ControlledID)
			if (auto* Proxy = PC->Proxies.Find(U.ID); Proxy && *Proxy)
			{
				const FVector Target = (*Proxy)->EyePosition();
				if (FVector::DotProduct((Target - Eye).GetSafeNormal(), Look.Vector()) < .5) continue;
				FHitResult Hit;
				if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Query) || Hit.GetActor() == *Proxy) ClearLabels.Add(U.ID);
			}
	}
	TArray<const FBattleNetUnit*> LabelUnits;
	for (const auto& U : PC->View.Units) if (U.HP > 0 && U.ID != PC->View.ControlledID && (!bFPS || ClearLabels.Contains(U.ID))) LabelUnits.Add(&U);
	LabelUnits.Sort([&](const FBattleNetUnit& A, const FBattleNetUnit& B)
	{
		const bool ASelected = A.Squad == PC->SelectedSquad, BSelected = B.Squad == PC->SelectedSquad;
		if (ASelected != BSelected) return ASelected;
		return FVector::DistSquared(A.Position, PC->PlayerCameraManager->GetCameraLocation()) < FVector::DistSquared(B.Position, PC->PlayerCameraManager->GetCameraLocation());
	});
	TArray<FVector2D> Occupied;
	for (const auto* LabelUnit : LabelUnits) if (const auto& U = *LabelUnit; U.HP > 0) if (auto* Proxy = PC->Proxies.Find(U.ID)) if (*Proxy)
	{
		FVector2D P; if (!PC->ProjectWorldLocationToScreen((*Proxy)->GetActorLocation() + FVector(0, 0, 120), P) || P.Y < 95 || P.Y > H - 100) continue;
		if (P.X < 30 || P.X > W - 60) continue;
		bool bOverlap = false; for (const FVector2D& Other : Occupied) if (FMath::Abs(P.X - Other.X) < 90 && FMath::Abs(P.Y - Other.Y) < 30) { bOverlap = true; break; }
		if (bOverlap || (bFPS && Occupied.Num() >= 12)) continue;
		Occupied.Add(P);
		const bool bOwn = U.Team == PC->View.Team; const auto Color = bOwn ? Mint : Red;
		DrawRect(Ink, P.X - 25, P.Y, 50, 5); DrawRect(Color, P.X - 25, P.Y, FMath::Clamp(U.HP, 0.f, 100.f) * .5, 5);
		Label(bOwn ? FString::Printf(TEXT("S%d / %d%s"), U.Squad, U.ID, U.Squad == PC->SelectedSquad ? TEXT(" *") : TEXT("")) : TEXT("CONTACT"), P.X - 25, P.Y - 20, Color);
	}
	DrawRect(Ink, 16, H - 90, W - 32, 74);
	if (PC->View.ControlledID != INDEX_NONE)
	{
		for (const auto& U : PC->View.Units) if (U.ID == PC->View.ControlledID)
			Label(FString::Printf(TEXT("UNIT %d / HP %.0f / AMMO %d + %d / %s%s"), U.ID, U.HP, U.Ammo, U.Reserve, BattleOrderName(U.Order), U.ReloadRemaining > 0 ? TEXT(" / RELOADING") : TEXT("")), 30, H - 77, Mint);
		Label(TEXT("WASD: move  Mouse: aim  LMB: fire  R: reload  Space: jump  Tab: tactical"), 30, H - 48, FLinearColor::White);
		DrawLine(W / 2 - 10, H / 2, W / 2 + 10, H / 2, FLinearColor::White); DrawLine(W / 2, H / 2 - 10, W / 2, H / 2 + 10, FLinearColor::White);
	}
	else Label(TEXT("LMB: squad  RMB: move  T: attack move  G: defend  H: hold  WASD: pan  Wheel: zoom  E/Tab: FPS"), 30, H - 77, Mint);
	if (PC->View.ControlledID == INDEX_NONE) Label(PC->Notice, 30, H - 48, FLinearColor::White);
}
