#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "BattleSave.h"
#include "BattleUnit.h"
#include "BattleFieldLayout.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FutureBattle.h"

void ABattleGameMode::RunDelegationSmokeTest()
{
	bCombatPaused = true; bBattleEnded = false;
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	PC->EnterTactical();
	Match->InitializeMatch(); Match->Phase = EBattleMatchPhase::Combat;
	ABattleUnit* A = Units[0]; ABattleUnit* B = Units[5];
	const int32 AID = A->GetUnitID(), SA = A->GetState()->SquadID, SB = B->GetState()->SquadID;
	int32 EnemySquad = INDEX_NONE;
	for (const auto& Pair : Store->GetSquads()) if (Pair.Value.Team == 1) { EnemySquad = Pair.Key; break; }
	CheckSmoke(PC->CanShowDelegation() && !Store->FindSquad(SA)->bPlayerDelegated, TEXT("RTS starts in manual command with delegation controls available"));
	CheckSmoke(!Match->SetAllyDelegated(EnemySquad, true) && !Match->SetAllyDelegated(-1, true), TEXT("Enemy and invalid squads cannot be delegated"));
	PC->SetSelection(A); PC->DelegateSelected();
	CheckSmoke(Store->FindSquad(SA)->bPlayerDelegated && !Store->FindSquad(SB)->bPlayerDelegated, TEXT("Only the selected squad is delegated"));
	const int32 RevA = Store->FindSquad(SA)->OrderRevision, RevB = Store->FindSquad(SB)->OrderRevision;
	Match->Update(.1f);
	CheckSmoke(Store->FindSquad(SA)->OrderRevision == RevA, TEXT("Delegation does not issue orders while AI is paused"));
	bCombatPaused = false; CombatStartsAt = 0; Match->Update(.1f); bCombatPaused = true;
	CheckSmoke(Store->FindSquad(SA)->OrderRevision > RevA && Store->FindSquad(SA)->Order == EBattleOrder::AttackMove, TEXT("Delegated squad receives objective attack order on resume"));
	CheckSmoke(Store->FindSquad(SB)->OrderRevision == RevB, TEXT("Autonomous commander leaves manual squad untouched"));
	const FVector Goal = Store->FindSquad(SA)->Destination;
	bool bPublicGoal = false;
	for (const auto& Point : Match->Objectives) bPublicGoal |= Goal.Equals(Point.Position, 20);
	CheckSmoke(bPublicGoal, TEXT("Autonomous destination is a public objective"));
	for (auto& Point : Match->Objectives) Point.Owner = 0;
	bCombatPaused = false; Match->DelegationClock = 5; Match->Update(.1f); bCombatPaused = true;
	CheckSmoke(Store->FindSquad(SA)->Order == EBattleOrder::Defend, TEXT("Delegated squad defends when all objectives belong to allies"));
	PC->TakeCommandSelected();
	const int32 ManualRevision = Store->FindSquad(SA)->OrderRevision;
	for (auto& Point : Match->Objectives) Point.Owner = INDEX_NONE;
	bCombatPaused = false; Match->DelegationClock = 5; Match->Update(.1f); bCombatPaused = true;
	CheckSmoke(!Store->FindSquad(SA)->bPlayerDelegated && Store->FindSquad(SA)->OrderRevision == ManualRevision, TEXT("Manual takeover keeps current order and prevents AI reassignment"));
	PC->DelegateSelected(); PC->HoldSelected();
	CheckSmoke(!Store->FindSquad(SA)->bPlayerDelegated && Store->FindSquad(SA)->Order == EBattleOrder::Hold, TEXT("Hold command cancels delegation"));
	PC->DelegateSelected(); PC->IssueOrderAt(A->GetActorLocation(), EBattleOrder::Move);
	CheckSmoke(!Store->FindSquad(SA)->bPlayerDelegated && Store->FindSquad(SA)->Order == EBattleOrder::Move, TEXT("Valid manual movement takes command"));
	PC->DelegateSelected();
	const int32 BeforeInvalid = Store->FindSquad(SA)->OrderRevision;
	PC->IssueOrderAt(FVector(1000000,1000000,0), EBattleOrder::Move);
	CheckSmoke(Store->FindSquad(SA)->bPlayerDelegated && Store->FindSquad(SA)->OrderRevision == BeforeInvalid, TEXT("Unreachable manual order preserves delegation and previous order"));
	const auto Before = *A->GetState();
	CheckSmoke(PC->ControlUnit(A) && !PC->CanShowDelegation(), TEXT("TPS control remains available while squad is delegated"));
	bCombatPaused = false; Match->DelegationClock = 5; Match->Update(.1f); bCombatPaused = true;
	CheckSmoke(A->GetState()->Action == EBattleAction::DirectControl && PC->GetPawn() == A && A->GetState()->HP == Before.HP && A->GetState()->Ammo == Before.Ammo, TEXT("AI command updates do not steal TPS possession or change resources"));
	PC->EnterTactical();
	CheckSmoke(!Store->FindSquad(SA)->bPlayerDelegated, TEXT("Direct takeover retains manual authority on return to RTS"));
	PC->DelegateSelected();
	PC->SelectAll();
	int32 Manual = 0, Automatic = 0; PC->GetDelegationCounts(Manual, Automatic);
	CheckSmoke(Manual > 0 && Automatic == 1, TEXT("Mixed selection reports manual and AI squad counts"));
	PC->DelegateSelected(); PC->GetDelegationCounts(Manual, Automatic);
	CheckSmoke(Manual == 0 && Automatic == Units.Num()/10, TEXT("Select all delegates every allied squad exactly once"));
	PC->TakeCommandSelected(); PC->GetDelegationCounts(Manual, Automatic);
	CheckSmoke(Automatic == 0 && Manual == Units.Num()/10, TEXT("Select all can take command of all allied squads"));
	PC->SetSelection(nullptr); PC->DelegateSelected(); PC->GetDelegationCounts(Manual, Automatic);
	CheckSmoke(Manual == 0 && Automatic == 0, TEXT("Empty selection does not delegate any squads"));
	PC->SetSelection(A); PC->DelegateSelected();
	if (!BattleFieldLayout::IsLarge(GetWorld()))
	{
		auto* Save = Match->CaptureSnapshot(); TArray<uint8> Bytes;
		const bool bSaved = Save && UGameplayStatics::SaveGameToMemory(Save, Bytes);
		auto* Decoded = bSaved ? Cast<UBattleSave>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		FString Error;
		CheckSmoke(Decoded && Decoded->Validate(Error), *FString::Printf(TEXT("Delegation snapshot serializes and validates (%s)"), *Error));
		const bool bRestored = Decoded && Match->RestoreSnapshot(Decoded);
		CheckSmoke(bRestored && Store->FindSquad(SA)->bPlayerDelegated && !Store->FindSquad(SB)->bPlayerDelegated, TEXT("Snapshot restore preserves per-squad command ownership"));
		A = FindUnitByID(AID); PC->SetSelection(A);
	}
	Match->Phase = EBattleMatchPhase::Finished;
	CheckSmoke(!PC->CanShowDelegation() && !Match->SetAllyDelegated(SA, false), TEXT("Finished battle rejects command-mode changes"));
	Match->Phase = EBattleMatchPhase::Combat;
	CheckSmoke(PC->DelegationButtonAt(FVector2D(40,280)) == 0 && PC->DelegationButtonAt(FVector2D(260,280)) == 1 && PC->DelegationButtonAt(FVector2D(234,280)) == INDEX_NONE, TEXT("RTS button hit regions match display and exclude gap"));
	FString Prefix; FParse::Value(FCommandLine::Get(), TEXT("BattleDelegationScreens="), Prefix);
	if (!Prefix.IsEmpty())
	{
		// Exercise the same viewport input path used by real clicks, then capture the HUD.
		auto* View = GetWorld()->GetGameViewport()->Viewport;
		auto Click = [PC, View](int32 X, int32 Y)
		{
			PC->SetMouseLocation(X,Y);
			PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftMouseButton,IE_Pressed,FPlatformTime::Cycles64()));
			PC->InputKey(FInputKeyEventArgs(View,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftMouseButton,IE_Released,FPlatformTime::Cycles64()));
		};
		Click(40,280);
		CheckSmoke(!Store->FindSquad(SA)->bPlayerDelegated && PC->IsSelected(A) && !PC->IsDragging(), TEXT("Manual button click keeps selection and consumes world input"));
		Click(260,280);
		CheckSmoke(Store->FindSquad(SA)->bPlayerDelegated && PC->IsSelected(A) && !PC->IsDragging(), TEXT("AI button click delegates without deselecting or dragging"));

	}
	const FVector MotionStart = A->GetActorLocation();
	const int32 UntouchedRevision = Store->FindSquad(SB)->OrderRevision;
	bCombatPaused = false; CombatStartsAt = 0; Match->DelegationClock = 5;
	FTimerHandle Motion;
	GetWorldTimerManager().SetTimer(Motion, FTimerDelegate::CreateWeakLambda(this,[this,Prefix,AID,SB,MotionStart,UntouchedRevision]()
	{
		bCombatPaused = true;
		const auto* Moved = FindUnitByID(AID);
		CheckSmoke(Moved && FVector::Dist2D(Moved->GetActorLocation(),MotionStart) > 30, TEXT("Delegated soldier advances during live world ticks"));
		CheckSmoke(GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SB)->OrderRevision == UntouchedRevision, TEXT("Live autonomous updates preserve manual squad orders"));
		if (!Prefix.IsEmpty()) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-rts.png"),true,false);
		UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DELEGATION_RESULT failures=%d"),SmokeFailures);
		FTimerHandle Exit;
		GetWorldTimerManager().SetTimer(Exit, FTimerDelegate::CreateWeakLambda(this,[this](){FPlatformMisc::RequestExitWithStatus(false,SmokeFailures?1:0);}),1.f,false);
	}),3.f,false);
}
