#include "BattleNetwork.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattleSquadAI.h"
#include "BattleCover.h"
#include "FutureBattle.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"

void ABattleNetController::SendPilotView()
{
	if (!HasAuthority() || !ServerControlled || !ServerControlled->GetState()) return;
	const auto* S = ServerControlled->GetState();
	FBattleNetUnit V; V.ID = S->UnitID; V.Team = S->Team; V.Squad = S->SquadID; V.HP = S->HP;
	V.Ammo = S->Ammo; V.Reserve = S->Reserve; V.Order = S->Order; V.Destination = S->Destination;
	V.ShotSequence = S->ShotSequence; V.ReloadRemaining = FMath::Max(0., S->ReloadEndTime - GetWorld()->GetTimeSeconds());
	V.Position = ServerControlled->GetActorLocation(); V.Rotation = ServerControlled->GetActorRotation();
	ClientPilotView(V, ControlEpoch, NextFrame++);
}

void ABattleNetController::ClientPilotView_Implementation(const FBattleNetUnit& Unit, int32 Epoch, int32 Sequence)
{
	if (Epoch != View.ControlEpoch || Unit.ID != View.ControlledID || Sequence <= FMath::Max(View.Sequence, LastPilotFrame)) return;
	for (auto& V : View.Units) if (V.ID == Unit.ID) { V = Unit; LastPilotFrame = Sequence; if (NetBenchStage == 1) ++NetBenchPilotViews; break; }
}

void ABattleNetController::ServerControl_Implementation(int32 UnitID, int32 Sequence)
{
	auto Reject = [&](const TCHAR* Reason) { ClientControlResult(Sequence, false, Reason); };
	if (Sequence <= LastControlRequest || Sequence > 100000000) { Reject(TEXT("Stale control request")); return; }
	LastControlRequest = Sequence;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastControlAt < .1) { Reject(TEXT("Control request rate exceeded")); return; }
	LastControlAt = Now;
	if (UnitID == INDEX_NONE) { ReleaseDirect(); ClientControlResult(Sequence, true, TEXT("Tactical command restored")); return; }
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleNetGameMode>();
	if (!Mode || Mode->bCombatPaused || Mode->bBattleEnded) { Reject(TEXT("Battle is not active")); return; }
	ABattleUnit* Unit = nullptr; for (ABattleUnit* Candidate : Mode->Units) if (Candidate && Candidate->GetUnitID() == UnitID) { Unit = Candidate; break; }
	if (!Unit || !Unit->IsAlive() || Unit->GetState()->Team != ServerTeam) { Reject(TEXT("Unit is not owned or alive")); return; }
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (auto* Other = Cast<ABattleNetController>(It->Get()))
		if (Other != this && Other->ServerControlled == Unit) { Reject(TEXT("Unit is already controlled")); return; }
	if (!Unit->SavedAI) { Reject(TEXT("Unit controller unavailable")); return; }
	ReleaseDirect(); ServerControlled = Unit; ++ControlEpoch; LastInputSequence = 0; LastInputAt = -100;
	if (auto* AI = Cast<ABattleAIController>(Unit->SavedAI)) AI->PrepareControlHandoff();
	Unit->SavedAI->SetActorTickEnabled(false); Unit->GetCharacterMovement()->StopMovementImmediately();
	GetWorld()->GetSubsystem<UBattleSquadAI>()->ReleaseUnit(UnitID); GetWorld()->GetSubsystem<UBattleCover>()->Release(UnitID);
	Unit->SetDirectControl(true); PilotAim = Unit->GetActorRotation();
	ClientControlResult(Sequence, true, TEXT("Direct control granted")); SendView();
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_CONTROL Team=%d Unit=%d Epoch=%d Granted=1"), ServerTeam, UnitID, ControlEpoch);
}

void ABattleNetController::ReleaseDirect()
{
	if (!ServerControlled) return;
	auto* Unit = ServerControlled.Get(); ServerControlled = nullptr; ++ControlEpoch;
	PilotMove = FVector2D::ZeroVector; bPilotFire = bPilotJump = bPilotSprint = false; LastInputAt = -100;
	Unit->SetSprinting(false);
	if (Unit->IsAlive()) Unit->SetDirectControl(false);
	if (Unit->SavedAI)
	{
		Unit->SavedAI->SetActorTickEnabled(true);
		if (auto* AI = Cast<ABattleAIController>(Unit->SavedAI))
		{
			AI->PrepareControlHandoff();
			if (auto* Mode = GetWorld()->GetAuthGameMode<ABattleNetGameMode>(); Mode && !Mode->bCombatPaused && !Mode->bBattleEnded && Unit->IsAlive()) AI->BeginOrder();
		}
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_CONTROL Team=%d Unit=%d Epoch=%d Released=1 AI=%d"), ServerTeam, Unit->GetUnitID(), ControlEpoch, Unit->SavedAI && Unit->SavedAI->IsActorTickEnabled());
}

void ABattleNetController::ClientControlResult_Implementation(int32 Sequence, bool bAccepted, const FString& Message)
{
	Notice = Message;
	if (Sequence == 1) bPilotForeignRejected = !bAccepted && Message.Contains(TEXT("not owned"));
	if (Sequence == 2) bPilotClaimed = bAccepted;
	if (Sequence == 3) bPilotReleased = bAccepted;
}

void ABattleNetController::ServerDirectInput_Implementation(int32 Epoch, int32 Sequence, FVector2D Move, FRotator Aim, bool bFire, bool bJump, bool bSprint)
{
	if (!ServerControlled || Epoch != ControlEpoch || Sequence <= LastInputSequence || Sequence > 100000000 || Move.ContainsNaN() || Aim.ContainsNaN() || FMath::Abs(Move.X) > 1 || FMath::Abs(Move.Y) > 1) return;
	const double Now = GetWorld()->GetTimeSeconds(); if (Now - LastInputAt < .01) return;
	LastInputSequence = Sequence; LastInputAt = Now; PilotMove = Move.GetClampedToMaxSize(1);
	PilotAim = FRotator(FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch), -85.f, 85.f), FRotator::NormalizeAxis(Aim.Yaw), 0);
	bPilotFire = bFire; bPilotJump = bJump; bPilotSprint = bSprint;
}

void ABattleNetController::ServerReload_Implementation(int32 Epoch)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleNetGameMode>();
	if (ServerControlled && Epoch == ControlEpoch && Mode && !Mode->bCombatPaused && !Mode->bBattleEnded)
		GetWorld()->GetSubsystem<UBattleState>()->StartReload(ServerControlled->GetUnitID(), GetWorld()->GetTimeSeconds());
}

void ABattleNetController::TickDirect(float DeltaTime)
{
	if (!ServerControlled) return;
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleNetGameMode>();
	if (!ServerControlled->IsAlive() || !Mode || Mode->bBattleEnded) { ReleaseDirect(); return; }
	if (!Mode->bCombatPaused) ServerControlled->ReloadIfEmpty();
	if (Mode->bCombatPaused || GetWorld()->GetTimeSeconds() - LastInputAt > .25) { ServerControlled->SetSprinting(false); ServerControlled->StopJumping(); return; }
	auto* Unit = ServerControlled.Get(); Unit->SavedAI->SetControlRotation(PilotAim); Unit->SetActorRotation(FRotator(0, PilotAim.Yaw, 0));
	const FRotationMatrix Facing(FRotator(0, PilotAim.Yaw, 0));
	Unit->SetSprinting(bPilotSprint);
	Unit->AddMovementInput(Facing.GetUnitAxis(EAxis::X), PilotMove.X, true); Unit->AddMovementInput(Facing.GetUnitAxis(EAxis::Y), PilotMove.Y, true);
	if (bPilotJump) Unit->Jump(); else Unit->StopJumping();
	if (bPilotFire) Unit->FireAt(Unit->EyePosition(), PilotAim.Vector(), true);
}

void ABattleNetController::UpdatePilotCamera(int32 PreviousID, int32 PreviousEpoch)
{
	if (!IsLocalController() || (PreviousID == View.ControlledID && PreviousEpoch == View.ControlEpoch)) return;
	bLocalFire = false;
	if (auto* Previous = Proxies.Find(PreviousID)) if (*Previous) { (*Previous)->GetMesh()->SetOwnerNoSee(false); (*Previous)->SetOwner(nullptr); (*Previous)->SetAnimationDetail(false); }
	if (auto* Current = Proxies.Find(View.ControlledID); Current && *Current)
	{
		auto* Unit = Current->Get(); Unit->SetOwner(this); Unit->GetMesh()->SetOwnerNoSee(true); Unit->FirstPersonCamera->bUsePawnControlRotation = false;
		Unit->SetAnimationDetail(true);
		Unit->FirstPersonCamera->PostProcessSettings.bOverride_MotionBlurAmount = true; Unit->FirstPersonCamera->PostProcessSettings.MotionBlurAmount = 0;
		SetControlRotation(Unit->GetActorRotation()); Unit->FirstPersonCamera->SetWorldRotation(GetControlRotation()); SetViewTarget(Unit);
		bShowMouseCursor = false; FInputModeGameOnly Input; Input.SetConsumeCaptureMouseDown(false); SetInputMode(Input);
	}
	else
	{
		if (CameraPawn) { if (auto* Previous = Proxies.Find(PreviousID)) if (*Previous) CameraPawn->SetActorLocation(FVector((*Previous)->GetActorLocation().X, (*Previous)->GetActorLocation().Y, 0)); SetViewTarget(CameraPawn); }
		bShowMouseCursor = true; FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false); SetInputMode(Input);
	}
}

void ABattleNetController::UpdateLocalPilot(float DeltaTime)
{
	if (View.ControlledID == INDEX_NONE) return;
	auto* Proxy = Proxies.Find(View.ControlledID); if (!Proxy || !*Proxy) return;
	float X = 0, Y = 0; GetInputMouseDelta(X, Y);
	FRotator Aim = GetControlRotation(); Aim.Yaw += X * .18f; Aim.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch - Y * .18f), -85.f, 85.f); SetControlRotation(Aim);
	(*Proxy)->FirstPersonCamera->SetWorldRotation(Aim);
	if (FParse::Param(FCommandLine::Get(), TEXT("BattleNetPilotProbe")) || FParse::Param(FCommandLine::Get(), TEXT("BattleNetBenchmark"))) return;
	const double Now = GetWorld()->GetTimeSeconds(); if (Now < NextInputAt) return; NextInputAt = Now + .05;
	const FVector2D Move((IsInputKeyDown(EKeys::W) ? 1 : 0) - (IsInputKeyDown(EKeys::S) ? 1 : 0), (IsInputKeyDown(EKeys::D) ? 1 : 0) - (IsInputKeyDown(EKeys::A) ? 1 : 0));
	ServerDirectInput(View.ControlEpoch, NextInputSequence++, Move.GetClampedToMaxSize(1), Aim, bLocalFire, IsInputKeyDown(EKeys::SpaceBar), IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift));
}
