#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleMetrics.h"
#include "BattlePerception.h"
#include "Components/BoxComponent.h"
#include "FutureBattle.h"

namespace
{
struct FCornerResult
{
	int32 FirstStep = INDEX_NONE;
	int32 Shooters = 0;
	int64 SenseRays = 0, GateRays = 0;
};

// Matched one-target kernels. Only the observer set and the ownership of
// the acquisition timer differ. Both use the shipping collision channel.
FCornerResult Sample(const TArray<TObjectPtr<ABattleUnit>>& Team, ABattleUnit* Target, UBattleMetrics& Metrics, int32 Mode)
{
	// Modes 2/3 retain the original continuous squad timer; 4/5 keep a
	// separate timer per alternate, invalidated on that observer's own miss.
	const bool bSquad = Mode != 0;
	int32 Alternate = Mode == 3 || Mode == 5 ? 3 : 1;
	FCornerResult Result;
	int32 FirstVisible[5] = {-1,-1,-1,-1,-1};
	const double Delay = Team[0]->GetState()->Perception.DetectionSeconds;
	for (int32 Step = 0; Step <= 5; ++Step)
	{
		const int64 BeforeScan = Metrics.SightRays;
		int32 Eligible = 0;
		bool SeenBySquad = false;
		for (int32 I = 0; I < 5; ++I)
		{
			if (bSquad && I % 2 != 0) continue; // A, C and E are representatives.
			const bool Seen = Team[I]->CanSee(Target);
			if (bSquad)
			{
				if (Seen) { SeenBySquad = true; break; }
				continue;
			}
			if (!Seen) FirstVisible[I] = -1;
			else if (FirstVisible[I] < 0) FirstVisible[I] = Step;
			if (Seen && (Step - FirstVisible[I]) * .2 + 1.e-6 >= Delay) Eligible |= 1 << I;
		}
		if (bSquad)
		{
			if (!SeenBySquad && (Mode == 1 || Mode >= 4)) FirstVisible[0] = -1;
			else if (SeenBySquad && FirstVisible[0] < 0) FirstVisible[0] = Step;
			if (SeenBySquad && (Step - FirstVisible[0]) * .2 + 1.e-6 >= Delay) Eligible = 31;
		}
		if (Mode >= 2 && !SeenBySquad)
		{
			const int32 I = Alternate;
			Alternate = I == 1 ? 3 : 1;
			const bool Seen = Team[I]->CanSee(Target);
			const int32 Timer = Mode >= 4 ? I : 0;
			if (!Seen) FirstVisible[Timer] = -1;
			else if (FirstVisible[Timer] < 0) FirstVisible[Timer] = Step;
			if (Seen && (Step - FirstVisible[Timer]) * .2 + 1.e-6 >= Delay) Eligible = 31;
		}
		Result.SenseRays += Metrics.SightRays - BeforeScan;
		if (!Eligible || Result.FirstStep != INDEX_NONE) continue;
		Result.FirstStep = Step;
		const int64 BeforeGate = Metrics.SightRays;
		for (int32 I = 0; I < 5; ++I)
			if ((Eligible & (1 << I)) && Team[I]->CanSee(Target)) Result.Shooters |= 1 << I;
		Result.GateRays += Metrics.SightRays - BeforeGate;
	}
	return Result;
}
}

void ABattleGameMode::RunCornerTrial()
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>();
	const FVector Origin(-12000, -12000, 100);
	TArray<TObjectPtr<ABattleUnit>> Team = {Units[0], Units[1], Units[2], Units[3], Units[4]};
	ABattleUnit* Enemy = Units[10];
	Enemy->SetActorLocation(Origin + FVector(700, 220, 0));
	for (ABattleUnit* Unit : Team) Store->SetTrainingPreset(Unit->GetUnitID(), EBattleTraining::Regular);
	auto* Wall = GetWorld()->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(50, 550, 200));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation(Origin + FVector(0, -450, 0));
	Box->RegisterComponent();

	TArray<FBattleUnitState> SavedUnits;
	TArray<FBattleSquadState> SavedSquads;
	Store->GetUnits().GenerateValueArray(SavedUnits);
	Store->GetSquads().GenerateValueArray(SavedSquads);
	TArray<TObjectPtr<ABattleUnit>> Active = Team;
	Active.Add(Enemy);
	// Separate process for this suite; world time does not advance inside a trial.
	for (int32 Exposed : {0, 4, 1, 3})
	{
		Store->RestoreRecords(SavedUnits, SavedSquads);
		for (int32 I = 0; I < 5; ++I)
		{
			const int32 Slot = I == Exposed ? 0 : I == 0 ? Exposed : I;
			Team[I]->SetActorLocation(Origin + FVector(-350, Slot == 0 ? 240 : -25 - Slot * 150, 0));
			Team[I]->SetActorRotation(FRotator::ZeroRotator);
		}
		// Validate the individual kernel against actual live perception contacts.
		auto* Perception = NewObject<UBattlePerception>(GetWorld());
		int32 OracleFirst = -1, OracleMask = 0;
		for (int32 Step = 0; Step <= 5; ++Step)
		{
			Perception->Update(Active, 100. + Step * .2);
			OracleMask = 0;
			for (int32 I = 0; I < 5; ++I)
				if (const auto* Contact = Perception->FindContact(Team[I]->GetUnitID(), Enemy->GetUnitID()); Contact && Contact->bVisible)
					OracleMask |= 1 << I;
			if (OracleMask && OracleFirst < 0) OracleFirst = Step;
		}
		CheckSmoke(OracleMask == (1 << Exposed) && OracleFirst == 3, TEXT("Production perception sees only the exposed soldier's target after 0.6 seconds"));
		const TCHAR* Names[] = {TEXT("individual"), TEXT("fixed"), TEXT("alternateB"), TEXT("alternateD"), TEXT("retainedB"), TEXT("retainedD")};
		constexpr int32 Modes = UE_ARRAY_COUNT(Names);
		FCornerResult Results[Modes];
		TArray<double> Times[Modes];
		Metrics->bRecording = true;
		for (int32 Batch = -1; Batch < 12; ++Batch)
			for (int32 Order = 0; Order < Modes; ++Order)
			{
				const int32 Mode = (Order + Batch + 1) % Modes;
				const double Start = FPlatformTime::Seconds();
				for (int32 Repeat = 0; Repeat < 256; ++Repeat)
					Results[Mode] = Sample(Team, Enemy, *Metrics, Mode);
				const double Us = (FPlatformTime::Seconds() - Start) * 1.e6 / 256.;
				if (Batch >= 0) Times[Mode].Add(Us);
			}
		for (int32 Mode = 0; Mode < Modes; ++Mode)
		{
			Store->RestoreRecords(SavedUnits, SavedSquads);
			const auto& R = Results[Mode];
			const bool bNonRepresentative = Exposed % 2 == 1;
			const int32 Expected = bNonRepresentative && Mode >= 1 && Mode <= 3 ? 0 : OracleMask;
			const int32 ExpectedStep = !Expected ? -1 : bNonRepresentative && Mode >= 4 ? (Exposed == (Mode == 4 ? 1 : 3) ? 4 : 5) : OracleFirst;
			CheckSmoke(R.Shooters == Expected && R.FirstStep == ExpectedStep, TEXT("Acquisition and firing eligibility match corner geometry and representative coverage"));
			const float HP = Enemy->GetState()->HP;
			const int64 BeforeShots = Metrics->Shots;
			int32 Fired = 0;
			for (int32 I = 0; I < 5; ++I)
				if ((R.Shooters & (1 << I)) && Team[I]->FireAt(Team[I]->EyePosition(), Enemy->GetActorLocation() + FVector(0,0,35) - Team[I]->EyePosition(), false))
					Fired |= 1 << I;
			const int64 BulletRays = Metrics->Shots - BeforeShots;
			CheckSmoke(Fired == Expected && BulletRays == (Expected ? 1 : 0) && HP - Enemy->GetState()->HP == (Expected ? GetDefault<UBattleTuning>()->ShotDamage : 0.f), TEXT("Actual shot, bullet trace and damage agree with permitted shooter"));
			Times[Mode].Sort();
			UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_CORNER exposed=%c mode=%s first_s=%.1f fired_mask=%d damage=%.0f sense_rays=%lld gate_rays=%lld bullet_rays=%lld median_us=%.3f min_us=%.3f max_us=%.3f"),
				'A'+Exposed, Names[Mode], R.FirstStep < 0 ? -1. : R.FirstStep * .2, Fired, HP-Enemy->GetState()->HP, R.SenseRays, R.GateRays, BulletRays, (Times[Mode][5]+Times[Mode][6])*.5, Times[Mode][0], Times[Mode].Last());
		}
		Metrics->bRecording = false;
	}
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SMOKE_RESULT failures=%d"), SmokeFailures);
	FPlatformMisc::RequestExitWithStatus(false, SmokeFailures ? 1 : 0);
}
