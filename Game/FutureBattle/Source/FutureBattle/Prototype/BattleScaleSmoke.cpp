#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "FutureBattle.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

void ABattleGameMode::RunScaleSmokeTest()
{
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	if (!PC || Units.Num() != ScaleUnitCount)
	{ CheckSmoke(false, TEXT("Scale fixture initialized")); UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SCALE_RESULT failures=%d"), SmokeFailures); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
	ABattleUnit* A = Units[0];
	FString Screenshot; FParse::Value(FCommandLine::Get(), TEXT("BattleScaleScreenshot="), Screenshot);
	float Delay = 3.f;
	switch (ScaleSmokeStage++)
	{
	case 0:
	{
		TSet<int32> IDs; for (ABattleUnit* Unit : Units) { IDs.Add(Unit->GetUnitID()); ScaleSmokeStarts.Add(Unit->GetActorLocation()); }
		CheckSmoke(IDs.Num() == ScaleUnitCount && Store->GetSquads().Num() == ScaleUnitCount / 5, TEXT("Large field has unique IDs and five-member squads"));
		const int32 FirstID = Units[0]->GetUnitID(), LastID = Units.Last()->GetUnitID();
		Units.Swap(0, Units.Num() - 1);
		CheckSmoke(FindUnitByID(FirstID) == Units.Last() && FindUnitByID(LastID) == Units[0] && !FindUnitByID(INDEX_NONE) && !FindUnitByID(MAX_int32), TEXT("Unit lookup survives reordered actors and rejects invalid IDs"));
		Units.Swap(0, Units.Num() - 1);
		CheckSmoke(bCombatPaused && bBenchmarkOrdersReady && !bBattleEnded, TEXT("Large field starts paused with reachable initial enemy orders"));
		PC->SelectAll(); CheckSmoke(PC->GetSelection().Num() == ScaleUnitCount / 2, TEXT("Select all includes every friendly and no enemy"));
		PC->IssueMoveAt(FVector::ZeroVector);
		int32 Moving = 0; bool bBounded = true, bEnemyUnchanged = true;
		for (ABattleUnit* Unit : Units)
		{
			const auto* S = Unit->GetState();
			if (S->Team) { bEnemyUnchanged &= S->Order == EBattleOrder::AttackMove; continue; }
			if (S->Order == EBattleOrder::Move) { ++Moving; bBounded &= FMath::Abs(S->Destination.X) < 3800 && FMath::Abs(S->Destination.Y) < 2800; }
		}
		CheckSmoke(Moving >= ScaleUnitCount * .4f && bBounded && bEnemyUnchanged, TEXT("Large selection distributes reachable squad goals inside the field without commanding enemies"));
		break;
	}
	case 1:
	{
		int32 Moved = 0; for (int32 I = 0; I < Units.Num(); ++I) if (!Units[I]->GetState()->Team && FVector::Dist2D(Units[I]->GetActorLocation(), ScaleSmokeStarts[I]) > 100) ++Moved;
		CheckSmoke(Moved >= ScaleUnitCount / 10, TEXT("Large selection executes real movement while combat is paused"));
		PC->HoldSelected();
		ScaleSmokeHP = A->GetState()->HP; ScaleSmokeAmmo = A->GetState()->Ammo; ScaleSmokeReserve = A->GetState()->Reserve;
		CheckSmoke(PC->ControlUnit(A), TEXT("Large field permits friendly FPS control"));
		CheckSmoke(A->FireAt(A->EyePosition(), FVector::UpVector, true) && A->GetState()->Ammo == ScaleSmokeAmmo - 1, TEXT("FPS shot consumes exactly one round in large field"));
		CheckSmoke(Store->StartReload(A->GetUnitID(), GetWorld()->GetTimeSeconds()), TEXT("Large field reload starts"));
		if (!Screenshot.IsEmpty())
		{
			FTimerHandle CaptureTimer;
			GetWorldTimerManager().SetTimer(CaptureTimer, FTimerDelegate::CreateLambda([Screenshot]() { FScreenshotRequest::RequestScreenshot(Screenshot + TEXT("-fps.png"), true, false); }), .25f, false);
		}
		Delay = .5f;
		break;
	}
	case 2:
		PC->EnterTactical(); Delay = 2.5f; break;
	case 3:
		CheckSmoke(A->GetState()->HP == ScaleSmokeHP && A->GetState()->Ammo == ScaleSmokeAmmo && A->GetState()->Reserve == ScaleSmokeReserve - 1 && A->GetState()->Order == EBattleOrder::Hold,
			TEXT("FPS to RTS preserves health and order and completes reload exactly once"));
		PC->ControlUnit(A); PC->EnterTactical();
		CheckSmoke(A->GetController() == A->SavedAI && A->GetState()->Reserve == ScaleSmokeReserve - 1, TEXT("Repeated large-field handoff restores AI without duplicating ammunition"));
		bCombatPaused = false; CombatStartsAt = 0; Delay = 6.f;
		break;
	default:
	{
		int32 Shots = 0; for (ABattleUnit* Unit : Units) Shots += Unit->GetState()->ShotSequence;
		CheckSmoke(Shots > 1, TEXT("Large field AI resumes and fires after unpausing"));
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SCALE_RESULT failures=%d units=%d shots=%d"), SmokeFailures, Units.Num(), Shots);
		if (!Screenshot.IsEmpty())
		{
			FScreenshotRequest::RequestScreenshot(Screenshot + TEXT("-rts.png"), true, false);
			FTimerHandle ExitTimer; const uint8 ExitCode = SmokeFailures ? 1 : 0;
			GetWorldTimerManager().SetTimer(ExitTimer, FTimerDelegate::CreateLambda([ExitCode]() { FPlatformMisc::RequestExitWithStatus(false, ExitCode); }), 3.f, false);
		}
		else FPlatformMisc::RequestExitWithStatus(false, SmokeFailures ? 1 : 0);
		return;
	}
	}
	FTimerHandle Timer; GetWorldTimerManager().SetTimer(Timer, this, &ABattleGameMode::RunScaleSmokeTest, Delay, false);
}
