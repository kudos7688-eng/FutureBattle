#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

void ABattleGameMode::RunObjectiveSmokeTest()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>(); auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	Match->Phase = EBattleMatchPhase::Combat; bBattleEnded = false; bCombatPaused = false;
	Match->Credits[0] = Match->Credits[1] = 2000;
	Match->HireSquad(1, EBattleTraining::Regular);
	ABattleUnit* Ally = nullptr; ABattleUnit* Enemy = nullptr;
	for (ABattleUnit* Unit : Units) if (Unit && Unit->IsAlive())
	{
		Unit->SavedAI->SetActorTickEnabled(false); Unit->SavedAI->StopMovement(); Unit->GetCharacterMovement()->StopMovementImmediately();
		Unit->SetActorLocation(FVector(Unit->GetState()->Team ? 2800 : -2800, 2400, 95), false, nullptr, ETeleportType::TeleportPhysics);
		if (Unit->GetState()->Team == 0) Ally = Unit; else Enemy = Unit;
	}
	if (!Ally || !Enemy) { CheckSmoke(false, TEXT("Objective fixtures available")); CompleteSmokeTest(); return; }
	const FVector Point = Match->Objectives[0].Position;
	Ally->SetActorLocation(Point + FVector(0, 0, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Match->UpdateObjectives(4.f);
	CheckSmoke(Match->Objectives[0].Owner == INDEX_NONE && Match->Objectives[0].CaptureProgress == 4.f, TEXT("Public objective requires continuous occupation before ownership"));
	Enemy->SetActorLocation(Point + FVector(180, 0, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Match->UpdateObjectives(5.f);
	CheckSmoke(Match->Objectives[0].bContested && Match->Objectives[0].CaptureProgress == 4.f && Match->Score[0] == 0.f, TEXT("Opposing occupants freeze capture and scoring"));
	Enemy->SetActorLocation(FVector(2800, 2400, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Match->UpdateObjectives(4.f);
	CheckSmoke(Match->Objectives[0].Owner == 0 && Match->Score[0] == 0.f, TEXT("Eight uncontested seconds capture without a retroactive payout"));
	const int32 BeforeIncome = Match->Credits[0]; Match->UpdateObjectives(5.f);
	CheckSmoke(Match->Score[0] == 5.f && Match->Credits[0] == BeforeIncome + 20, TEXT("Owned point yields one score per second and 20 credits per five seconds"));
	bCombatPaused = true; const float BeforeElapsed = Match->Elapsed; Match->Update(10.f);
	CheckSmoke(Match->Elapsed == BeforeElapsed && Match->Score[0] == 5.f && Match->Credits[0] == BeforeIncome + 20, TEXT("Paused match does not advance time income or objectives")); bCombatPaused = false;
	// Spend a magazine through the canonical fire/reload rules, then purchase only the missing reserve.
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Shot = 0; Shot < 30; ++Shot) Store->ConsumeShot(Ally->GetUnitID(), Now + Shot, .1);
	Store->StartReload(Ally->GetUnitID(), Now + 31); Store->AdvanceReloads(Now + 40);
	const int32 BeforeSupply = Match->Credits[0];
	CheckSmoke(Ally->GetState()->Reserve == 60 && Match->Resupply(Ally->GetUnitID(), 50) == 30 && Ally->GetState()->Reserve == 90 && Match->Credits[0] == BeforeSupply - 30, TEXT("Owned supply point restores reserve only and charges actual rounds up to capacity"));
	CheckSmoke(Match->Resupply(Ally->GetUnitID(), 10) == 0 && Match->Credits[0] == BeforeSupply - 30 && Match->Resupply(Ally->GetUnitID(), -1) == 0, TEXT("Full reserve and invalid supply requests cannot charge or mint credits"));
	for (int32 Shot = 0; Shot < 30; ++Shot) Store->ConsumeShot(Ally->GetUnitID(), Now + 50 + Shot, .1);
	Store->StartReload(Ally->GetUnitID(), Now + 81); Store->AdvanceReloads(Now + 90);
	Enemy->SetActorLocation(Point + FVector(180, 0, 95), false, nullptr, ETeleportType::TeleportPhysics); Match->UpdateObjectives(1.f);
	CheckSmoke(Match->Resupply(Ally->GetUnitID()) == 0 && Ally->GetState()->Reserve == 60, TEXT("Contested point cannot supply ammunition"));
	Ally->SetActorLocation(FVector(-2600, 0, 95), false, nullptr, ETeleportType::TeleportPhysics); Match->Credits[0] = 3;
	CheckSmoke(Match->Resupply(Ally->GetUnitID(), 10) == 3 && Match->Credits[0] == 0 && Ally->GetState()->Reserve == 63, TEXT("Home base supply is limited by available credits without overdraft"));
	Match->UpdateObjectives(8.f);
	CheckSmoke(Match->Objectives[0].Owner == 1, TEXT("Enemy occupation transfers ownership after eight seconds"));
	Match->UpdateEnemyOrders();
	const auto* EnemySquad = Store->FindSquad(Enemy->GetState()->SquadID);
	bool bPublicGoal = false; for (const auto& Objective : Match->Objectives) bPublicGoal |= EnemySquad->Destination.Equals(Objective.Position, 20);
	CheckSmoke(bPublicGoal && EnemySquad->Order == EBattleOrder::AttackMove, TEXT("Enemy director assigns a public unowned objective instead of unseen enemy positions"));
	// Victory conditions use separate fixture snapshots, preserving the tested economic outcome above.
	for (auto& Objective : Match->Objectives) { Objective.Owner = INDEX_NONE; Objective.CapturingTeam = INDEX_NONE; Objective.CaptureProgress = 0; }
	Match->Objectives[0].Owner = 1; Match->Score[0] = 5; Match->Score[1] = 99;
	Match->UpdateObjectives(1.f);
	CheckSmoke(Match->Phase == EBattleMatchPhase::Finished && Match->Winner == 1, TEXT("Control score target ends the match with the scoring team as winner"));
	Match->Phase = EBattleMatchPhase::Combat; bBattleEnded = false; Match->Score[0] = Match->Score[1] = 0;
	Match->Objectives[0].Owner = INDEX_NONE; Enemy->SetActorLocation(FVector(2800, 2400, 95), false, nullptr, ETeleportType::TeleportPhysics);
	Match->Elapsed = 179.9f; Match->DirectorClock = 0; Match->SupplyClock = 0;
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Match]()
	{
		CheckSmoke(Match->Phase == EBattleMatchPhase::Finished && Match->Winner == INDEX_NONE && bBattleEnded, TEXT("Real world tick ends an equal-score match as a draw at the time limit"));
		// The save smoke used to run here and start the support smoke; it was removed with save/load on 2026-11-25.
		RunSupportSmokeTest();
	}), .3f, false);
}
