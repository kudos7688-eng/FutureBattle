#include "BattlePerception.h"
#include "BattleMassTrial.h"
#include "BattleMoveCourse.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleMetrics.h"
#include "BattleState.h"
#include "BattleNavigation.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ABattleGameMode::ConfigureBenchmark()
{
	bBenchmarkMode = FParse::Param(FCommandLine::Get(), TEXT("BattleBenchmark"));
	FString SenseMode; FParse::Value(FCommandLine::Get(), TEXT("BattleSenseMode="), SenseMode);
	auto* Sense = GetWorld()->GetSubsystem<UBattlePerception>();
	Sense->bUseSpatialGrid = SenseMode == TEXT("Grid") || SenseMode == TEXT("Even") || SenseMode == TEXT("Far");
	Sense->bEvenSchedule = SenseMode == TEXT("Even") || SenseMode == TEXT("Far");
	Sense->bSlowUnknownFar = SenseMode == TEXT("Far");
	if (!bBenchmarkMode) return;
	bSplitDecisionLoad = FParse::Param(FCommandLine::Get(), TEXT("BattleSplitDecisions"));
	FParse::Value(FCommandLine::Get(), TEXT("BattleUnits="), BenchmarkUnitCount);
	FParse::Value(FCommandLine::Get(), TEXT("BattleMoveCourse="), BenchmarkCourse);
	if ((BenchmarkCourse != TEXT("Gate") && BenchmarkCourse != TEXT("Open")) || (BenchmarkUnitCount != 500 && BenchmarkUnitCount != 1000)) BenchmarkCourse.Reset();
	bMassMovement = FParse::Param(FCommandLine::Get(), TEXT("BattleMassMovement"));
	if (BenchmarkUnitCount != 500 && BenchmarkUnitCount != 1000) BenchmarkUnitCount = FMath::Clamp(BenchmarkUnitCount / 10 * 10, 10, 100);
	FParse::Value(FCommandLine::Get(), TEXT("BattleSeconds="), BenchmarkSeconds); BenchmarkSeconds = FMath::Clamp(BenchmarkSeconds, 5.f, 120.f);
	FParse::Value(FCommandLine::Get(), TEXT("BattleSeed="), BenchmarkSeed);
	auto ReadTier = [](const TCHAR* Key)
	{
		FString Tier; FParse::Value(FCommandLine::Get(), Key, Tier);
		return Tier.Equals(TEXT("Recruit"), ESearchCase::IgnoreCase) ? 0 : Tier.Equals(TEXT("Veteran"), ESearchCase::IgnoreCase) ? 2 : 1;
	};
	BenchmarkTraining = ReadTier(TEXT("BattlePreset=")); BenchmarkOpponentTraining = ReadTier(TEXT("BattleOpponentPreset="));
	bBenchmarkStableLoad = FParse::Param(FCommandLine::Get(), TEXT("BattleStableLoad"));
	FParse::Value(FCommandLine::Get(), TEXT("BattleReport="), BenchmarkReport);
	if (BenchmarkReport.IsEmpty()) BenchmarkReport = FPaths::ProjectSavedDir() / TEXT("Benchmarks") / TEXT("Latest.json");

	auto* Tuning = GetMutableDefault<UBattleTuning>();
	Tuning->bStaggerDecisions = !FParse::Param(FCommandLine::Get(), TEXT("BattleNoStagger"));
	int32 Slices = 4;
	FParse::Value(FCommandLine::Get(), TEXT("BattlePerceptionSlices="), Slices);
	Tuning->PerceptionSlices = Tuning->bStaggerDecisions ? FMath::Clamp(Slices, 1, 16) : 1;
}

bool ABattleGameMode::PrepareBenchmarkOrders()
{
	// Dense layouts introduce goals near cover. Validate before combat, keeping the original 20/100 scenarios intact.
	if (Units.Num() <= 100) return true;
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	const TArray<FVector> Offsets = {FVector::ZeroVector, FVector(200,0,0), FVector(-200,0,0), FVector(0,200,0), FVector(0,-200,0), FVector(400,0,0), FVector(-400,0,0), FVector(0,400,0), FVector(0,-400,0)};
	for (const auto& Pair : Store->GetSquads())
	{
		if (Pair.Value.Order == EBattleOrder::Hold) continue;
		TArray<ABattleUnit*> Members; for (ABattleUnit* Unit : Units) if (Unit && Unit->GetState()->SquadID == Pair.Key) Members.Add(Unit);
		if (Members.Num() != 5) return false;
		const FVector OriginalCenter = Pair.Value.Destination; bool bValid = false;
		for (const FVector& Offset : Offsets)
		{
			FVector Center; if (!BattleNavigation::ResolveGoal(Members[2], OriginalCenter + Offset, Center)) continue;
			TMap<int32, FVector> Goals;
			for (ABattleUnit* Unit : Members)
			{
				FVector Goal;
				if (BattleNavigation::FormationGoal(Unit, Center, Unit->GetState()->Destination - OriginalCenter, Goal)) Goals.Add(Unit->GetUnitID(), Goal);
			}
			if (Goals.Num() == Members.Num() && Store->SetSquadOrder(Pair.Key, Pair.Value.Order, Center, Goals))
			{ bValid = true; BenchmarkAdjustedSquads += !Offset.IsNearlyZero(); break; }
		}
		if (!bValid) return false;
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_BENCH_ORDERS validated=%d adjusted_squads=%d"), Units.Num(), BenchmarkAdjustedSquads);
	return true;
}

void ABattleGameMode::UpdateBenchmark()
{
	if ((bBenchmarkMode || bScaleTest) && !bBenchmarkFinished && !bBenchmarkOrdersReady && GetWorld()->GetTimeSeconds() >= 1)
	{
		bBenchmarkOrdersReady = PrepareBenchmarkOrders();
		if (bBenchmarkOrdersReady && bMassMovement)
		{
			MassTrial = NewObject<UBattleMassTrial>(this);
			MassTrial->Start(*GetWorld(), Units);
		}
		if (!bBenchmarkOrdersReady)
		{
			bBenchmarkFinished = true; UE_LOG(LogFutureBattle, Error, TEXT("BATTLE_BENCH_RESULT success=0 reason=InvalidInitialOrders"));
			if (bBenchmarkMode) FPlatformMisc::RequestExitWithStatus(false, 1);
			else { bBattleEnded = true; bCombatPaused = true; Result = TEXT("INITIAL ORDERS UNREACHABLE / F5 TO RESTART"); }
			return;
		}
	}
	// Let initial possession/order bursts settle before comparing steady scheduling.
	if (!bBenchmarkMode || bBenchmarkFinished || GetWorld()->GetTimeSeconds() < CombatStartsAt + 1.) return;
	auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>();
	if (!bBenchmarkStarted)
	{
		// Diagnostic ablations apply only in benchmark mode, after warmup.
		FString Omit; FParse::Value(FCommandLine::Get(), TEXT("BattleDiagnosticOmit="), Omit);
		for (ABattleUnit* Unit : Units) if (Unit)
		{
			if (Omit == TEXT("RVO")) Unit->GetCharacterMovement()->SetAvoidanceEnabled(false);
			if (Omit == TEXT("Animation")) Unit->GetMesh()->SetComponentTickEnabled(false);
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("BattleBenchmarkFPS")))
		{
			auto* Controller = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
			if (!Controller || Units.IsEmpty() || !Controller->ControlUnit(Units[0]))
			{
				bBenchmarkFinished = true;
				UE_LOG(LogFutureBattle, Error, TEXT("BATTLE_BENCH_RESULT success=0 reason=PossessionFailed"));
				FPlatformMisc::RequestExitWithStatus(false, 1);
				return;
			}
		}
		if (MoveCourse) MoveCourse->Start(*this, BenchmarkCourse == TEXT("Gate"));
		Metrics->Start(); bBenchmarkStarted = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("BattleProfileCSV")))
			FCsvProfiler::Get()->BeginCapture(-1, FPaths::GetPath(BenchmarkReport), FPaths::GetBaseFilename(BenchmarkReport) + TEXT("-profile.csv"));
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_BENCH_START units=%d seconds=%.1f stable_load=%d staggered=%d"), Units.Num(), BenchmarkSeconds, bBenchmarkStableLoad, GetDefault<UBattleTuning>()->bStaggerDecisions);
		return;
	}
	if (MoveCourse ? MoveCourse->Seconds() < BenchmarkSeconds : FPlatformTime::Seconds() - Metrics->StartedAt < BenchmarkSeconds) return;
	bBenchmarkFinished = true;
	auto Report = Metrics->Finish();
	Report->SetStringField(TEXT("movement_backend"), MassTrial ? TEXT("MassFlat") : TEXT("CharacterMovement"));
	if (MassTrial) Report->SetNumberField(TEXT("mass_active_peak"), MassTrial->ActivePeak);
	if (MoveCourse) MoveCourse->Report(*Report);
	if (FParse::Param(FCommandLine::Get(), TEXT("BattleProfileCSV"))) FCsvProfiler::Get()->EndCapture();
	Report->SetBoolField(TEXT("csv_profile"), FParse::Param(FCommandLine::Get(), TEXT("BattleProfileCSV")));
	FString Omitted; FParse::Value(FCommandLine::Get(), TEXT("BattleDiagnosticOmit="), Omitted);
	Report->SetStringField(TEXT("diagnostic_omission"), Omitted.IsEmpty() ? TEXT("None") : Omitted);
	const auto* Sense = GetWorld()->GetSubsystem<UBattlePerception>();
	Report->SetStringField(TEXT("sense_mode"), Sense->bSlowUnknownFar ? TEXT("Far") : Sense->bEvenSchedule ? TEXT("Even") : Sense->bUseSpatialGrid ? TEXT("Grid") : TEXT("Legacy"));
	Report->SetBoolField(TEXT("split_decisions"), bSplitDecisionLoad);
	Report->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
	Report->SetNumberField(TEXT("unit_count"), Units.Num()); Report->SetNumberField(TEXT("seed"), BenchmarkSeed);
	Report->SetStringField(TEXT("layout"), MoveCourse ? TEXT("course_25_rows_88cm") : BenchmarkUnitCount == 1000 ? TEXT("dense_40_rows_140cm") : BenchmarkUnitCount > 100 ? TEXT("dense_25_rows_140cm") : TEXT("original_300cm_rows"));
	Report->SetStringField(TEXT("camera"), FParse::Param(FCommandLine::Get(), TEXT("BattleBenchmarkFPS")) ? TEXT("first_person_stationary") : TEXT("overview_4500cm"));
	Report->SetBoolField(TEXT("validated_dense_goals"), BenchmarkUnitCount > 100 && bBenchmarkOrdersReady);
	Report->SetNumberField(TEXT("adjusted_squads"), BenchmarkAdjustedSquads);
	Report->SetBoolField(TEXT("animation_lod"), !FParse::Param(FCommandLine::Get(), TEXT("BattleNoAnimationLOD")));
	Report->SetBoolField(TEXT("hidden_animation_culling"), !FParse::Param(FCommandLine::Get(), TEXT("BattleNoAnimationLOD")));
	Report->SetNumberField(TEXT("combat_warmup_seconds"), 1.);
	Report->SetBoolField(TEXT("stable_load"), bBenchmarkStableLoad);
	Report->SetBoolField(TEXT("staggered"), GetDefault<UBattleTuning>()->bStaggerDecisions);
	Report->SetBoolField(TEXT("rendered"), !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")));
	Report->SetNumberField(TEXT("perception_slices"), GetDefault<UBattleTuning>()->PerceptionSlices);
	Report->SetNumberField(TEXT("initial_health"), bBenchmarkStableLoad ? 10000. : GetDefault<UBattleTuning>()->Health);
	Report->SetStringField(TEXT("ally_training"), BattleTrainingName(static_cast<EBattleTraining>(BenchmarkTraining)));
	Report->SetStringField(TEXT("enemy_training"), BattleTrainingName(static_cast<EBattleTraining>(BenchmarkOpponentTraining)));
	TArray<TSharedPtr<FJsonValue>> Teams;
	for (int32 Team = 0; Team < 2; ++Team)
	{
		int32 Alive = 0, Cost = 0, PendingMoves = 0; double HP = 0.; int64 Shots = 0;
		for (ABattleUnit* Unit : Units) if (Unit && Unit->GetState()->Team == Team)
		{
			const auto* S = Unit->GetState(); Alive += S->IsAlive(); HP += S->HP; Cost += S->HireCost; Shots += S->ShotSequence;
			PendingMoves += S->Action == EBattleAction::Moving || S->Action == EBattleAction::Flanking || S->Action == EBattleAction::TakingCover;
		}
		auto TeamResult = MakeShared<FJsonObject>(); TeamResult->SetNumberField(TEXT("team"), Team); TeamResult->SetNumberField(TEXT("alive"), Alive);
		TeamResult->SetNumberField(TEXT("remaining_hp"), HP); TeamResult->SetNumberField(TEXT("test_cost"), Cost); TeamResult->SetNumberField(TEXT("shots"), Shots);
		TeamResult->SetNumberField(TEXT("pending_moves"), PendingMoves); Teams.Add(MakeShared<FJsonValueObject>(TeamResult));
	}
	Report->SetArrayField(TEXT("teams"), Teams);
	FString JSON; auto Writer = TJsonWriterFactory<>::Create(&JSON); FJsonSerializer::Serialize(Report, Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(BenchmarkReport), true);
	const bool bSaved = FFileHelper::SaveStringToFile(JSON, *BenchmarkReport);
	const bool bValid = bSaved && Units.Num() == BenchmarkUnitCount && (!MassTrial || MassTrial->ActivePeak > 0)
		&& (MoveCourse ? MoveCourse->HasMoved() : Metrics->Decisions > 0 && Metrics->Shots > 0);
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_BENCH_RESULT success=%d report=%s decisions=%lld shots=%lld"), bValid, *BenchmarkReport, Metrics->Decisions, Metrics->Shots);
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(BenchmarkReport, TEXT("png")), true, false);
		FTimerHandle ExitTimer;
		GetWorldTimerManager().SetTimer(ExitTimer, FTimerDelegate::CreateLambda([bValid]() { FPlatformMisc::RequestExitWithStatus(false, bValid ? 0 : 1); }), 3.f, false);
	}
	else FPlatformMisc::RequestExitWithStatus(false, bValid ? 0 : 1);
}
