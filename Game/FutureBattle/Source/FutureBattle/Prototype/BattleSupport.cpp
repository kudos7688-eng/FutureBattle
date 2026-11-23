#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattlePerception.h"
#include "DrawDebugHelpers.h"

bool UBattleMatch::RequestSupport(int32 ObserverID)
{
	if (BattleFieldLayout::IsLarge(GetWorld())) return false;
	auto Fail = [this](const TCHAR* Message) { LastMessage = Message; return false; };
	auto* Store = GetWorld()->GetSubsystem<UBattleState>(); const auto* Observer = Store->Find(ObserverID);
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (Phase != EBattleMatchPhase::Combat || !Mode || Mode->bCombatPaused || !Observer || !Observer->IsAlive()) return Fail(TEXT("Support unavailable"));
	const int32 Team = Observer->Team;
	if (SupportUsed[Team] >= 3) return Fail(TEXT("All three support missions used"));
	if (SupportCooldown[Team] > 0) return Fail(TEXT("Support rearming (20s)"));
	if (Credits[Team] < 400) return Fail(TEXT("Support requires 400 credits"));
	const auto* Contacts = GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ObserverID);
	const FBattleContact* Chosen = nullptr; double Best = -1.;
	if (Contacts) for (const auto& Pair : *Contacts)
	{
		const auto& Contact = Pair.Value;
		const double Age = GetWorld()->GetTimeSeconds() - Contact.LastSeenTime;
		if (Contact.Confidence < .35f || Age < 0 || Age > Observer->Perception.MemorySeconds || Contact.LastKnownPosition.ContainsNaN()) continue;
		const double Rank = (Contact.bVisible ? 1000. : 0.) + Contact.Confidence * 100. - Age;
		if (!Chosen || Rank > Best || (Rank == Best && Contact.TargetID < Chosen->TargetID)) { Chosen = &Contact; Best = Rank; }
	}
	// -BattleSquadKnowledge=1: what his squad saw and heard is in the squad's table.
	if (const auto* SquadKnown = GetWorld()->GetSubsystem<UBattlePerception>()->GetSquadContacts(Observer->SquadID)) for (const auto& Pair : *SquadKnown)
	{
		const auto& Contact = Pair.Value.Contact;
		const double Age = GetWorld()->GetTimeSeconds() - Contact.LastSeenTime;
		if (Contact.Confidence < .35f || Age < 0 || Age > Observer->Perception.MemorySeconds || Contact.LastKnownPosition.ContainsNaN()) continue;
		const double Rank = (Contact.bVisible ? 1000. : 0.) + Contact.Confidence * 100. - Age;
		if (!Chosen || Rank > Best || (Rank == Best && Contact.TargetID < Chosen->TargetID)) { Chosen = &Contact; Best = Rank; }
	}
	if (!Chosen) return Fail(TEXT("No reliable known enemy position / scout or receive a report"));
	// Snapshot the observed coordinate. Target actors and their current positions are never consulted here.
	FBattleStrike Strike; Strike.Team = Team; Strike.Position = Chosen->LastKnownPosition;
	Strikes.Add(Strike); Credits[Team] -= 400; ++SupportUsed[Team]; SupportCooldown[Team] = 20.f;
	LastMessage = TEXT("Support requested / impact in 3s / -400 credits"); return true;
}

void UBattleMatch::UpdateSupport(float DeltaSeconds)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (Phase != EBattleMatchPhase::Combat || !Mode || Mode->bCombatPaused || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	for (float& Cooldown : SupportCooldown) Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	for (int32 Index = Strikes.Num() - 1; Index >= 0; --Index)
	{
		auto& Strike = Strikes[Index]; Strike.Remaining -= DeltaSeconds;
		if (Strike.Remaining > 0) continue;
		const FVector Origin = Strike.Position + FVector(0, 0, 100);
		DrawDebugSphere(GetWorld(), Origin, 350, 16, FColor::Orange, false, .7f, 0, 3.f);
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team != Strike.Team)
		{
			const float Distance = FVector::Dist(Unit->GetActorLocation(), Strike.Position);
			if (Distance >= 350) continue;
			FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleSupport), false);
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Unit->EyePosition(), ECC_Visibility, Query);
			if (bBlocked && Hit.GetActor() != Unit) continue;
			const float Damage = 60.f * (1.f - Distance / 350.f);
			if (Store->ApplyDamage(Unit->GetUnitID(), Strike.Team, Damage) && !Unit->IsAlive()) Unit->ApplyDeath();
		}
		Strikes.RemoveAtSwap(Index);
	}
}
