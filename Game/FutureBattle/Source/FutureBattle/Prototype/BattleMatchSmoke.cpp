#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "BattleSquadAI.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

void ABattleGameMode::RunMatchSmokeTest()
{
	bCombatPaused = true; bBattleEnded = false;
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController()); PC->EnterTactical();
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	for (ABattleUnit* Unit : Units) if (Unit)
	{
		Store->ApplyDamage(Unit->GetUnitID(), 1 - Unit->GetState()->Team, 100000); Unit->ApplyDeath(); Unit->Destroy();
	}
	Units.Reset(); GetWorld()->GetSubsystem<UBattlePerception>()->Reset(); GetWorld()->GetSubsystem<UBattleSquadAI>()->Reset();
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>(); Match->InitializeMatch();
	CheckSmoke(Match->MatchID.IsValid() && Match->Credits[0] == 3000 && Match->Credits[1] == 3000 && Match->Phase == EBattleMatchPhase::Deployment, TEXT("Fast match begins with independent team budgets and a match ID"));
	CheckSmoke(!Match->StartCombat() && Match->Phase == EBattleMatchPhase::Deployment, TEXT("Empty deployment cannot start combat"));
	const int32 Records = Store->GetUnits().Num(), Squads = Store->GetSquads().Num();
	CheckSmoke(!Match->HireSquad(-1, EBattleTraining::Regular) && !Match->HireSquad(0, static_cast<EBattleTraining>(99)) && Match->Credits[0] == 3000 && Store->GetUnits().Num() == Records, TEXT("Invalid purchase leaves credits and unit records unchanged"));
	CheckSmoke(Match->HireSquad(0, EBattleTraining::Veteran) && Match->Credits[0] == 1500 && Units.Num() == 5 && Store->GetSquads().Num() == Squads + 1, TEXT("Veteran purchase deploys exactly five members and charges 1500 once"));
	if (Units.Num() != 5) { CompleteSmokeTest(); return; }
	ABattleUnit* A = Units[0];
	bool bTraining = true; for (ABattleUnit* Unit : Units) bTraining &= Unit->GetState()->Training == EBattleTraining::Veteran && Unit->GetState()->Ammo == GetDefault<UBattleTuning>()->Capacity(Unit->GetState()->Weapon) && Unit->GetState()->HP == 100;
	CheckSmoke(bTraining, TEXT("Purchased capability tier keeps standard weapons health and ammunition"));
	CheckSmoke(Units[4]->GetState()->Weapon == EBattleWeapon::SupportGun && Units[4]->GetState()->Ammo == 60 && Units[0]->GetState()->Weapon == EBattleWeapon::Carbine, TEXT("Fifth squad member gets support gun independently of training"));
	PC->CycleTrainingSelected();
	CheckSmoke(A->GetState()->Training == EBattleTraining::Veteran && Match->Credits[0] == 1500, TEXT("Free training debug switch cannot bypass match prices"));
	PC->IssueMoveAt(FVector(0, 0, 0));
	CheckSmoke(A->GetState()->Order == EBattleOrder::Hold && !PC->ControlUnit(A) && !A->FireAt(A->EyePosition(), FVector::ForwardVector, true), TEXT("Deployment blocks movement possession and shooting before start"));
	CheckSmoke(Match->HireSquad(0, EBattleTraining::Regular) && Match->Credits[0] == 600 && Units.Num() == 10, TEXT("Second purchase finds unoccupied home positions"));
	const int32 BeforeRejected = Store->GetUnits().Num();
	CheckSmoke(!Match->HireSquad(0, EBattleTraining::Regular) && Match->Credits[0] == 600 && Store->GetUnits().Num() == BeforeRejected, TEXT("Insufficient credits cannot create a partial squad or debit money"));
	auto* Block = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-2700, 0, 150), FRotator::ZeroRotator);
	Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Block->SetActorScale3D(FVector(16, 42, 3)); Block->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	CheckSmoke(!Match->HireSquad(0, EBattleTraining::Recruit) && Match->Credits[0] == 600 && Store->GetUnits().Num() == BeforeRejected, TEXT("Blocked deployment preserves both credits and canonical records")); Block->Destroy();
	CheckSmoke(Match->HireSquad(1, EBattleTraining::Regular) && Match->Credits[1] == 2100 && Match->Credits[0] == 600, TEXT("Opponent pays from its own budget"));
	CheckSmoke(Match->StartCombat() && !Match->StartCombat() && Match->Phase == EBattleMatchPhase::Combat && !bCombatPaused, TEXT("Start transitions once from deployment into active combat"));
	CheckSmoke(PC->ControlUnit(A), TEXT("Purchased unit can enter FPS after combat starts")); PC->EnterTactical();
	CheckSmoke(A->GetState()->Training == EBattleTraining::Veteran && A->GetState()->HP == 100 && A->GetState()->Ammo == 30 && Match->Credits[0] == 600, TEXT("FPS return preserves purchased tier and match budget"));
	CheckSmoke(Match->HireSquad(0, EBattleTraining::Recruit) && Match->Credits[0] == 100 && Units.Num() == 20, TEXT("Combat reinforcement uses the same five-member purchase transaction"));
	for (ABattleUnit* Unit : Units) if (Unit->GetState()->Team == 1) { Store->ApplyDamage(Unit->GetUnitID(), 0, 100000); Unit->ApplyDeath(); }
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Match, A]()
	{
		CheckSmoke(Match->Phase == EBattleMatchPhase::Finished && Match->Winner == 0 && bBattleEnded, TEXT("Eliminating the opponent ends the active match on world tick"));
		const int32 Count = Units.Num();
		CheckSmoke(!Match->HireSquad(1, EBattleTraining::Recruit) && Units.Num() == Count && !Match->StartCombat() && !A->FireAt(A->EyePosition(), FVector::ForwardVector, true), TEXT("Finished match rejects reinforcement restart and shooting"));
		RunObjectiveSmokeTest();
	}), .3f, false);
}
