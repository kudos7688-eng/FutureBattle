#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleUnit.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

void ABattleGameMode::UpdateMatchSoak()
{
	if (bMatchSoakComplete || !FParse::Param(FCommandLine::Get(), TEXT("BattleMatchSoak"))) return;
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->Phase == EBattleMatchPhase::Deployment && GetWorld()->GetTimeSeconds() > 3)
	{
		int32 Scenario = 0; FParse::Value(FCommandLine::Get(), TEXT("BattleMatchScenario="), Scenario);
		Match->HireSquad(0, Scenario == 1 ? EBattleTraining::Recruit : EBattleTraining::Regular);
		Match->HireSquad(0, Scenario == 2 ? EBattleTraining::Veteran : EBattleTraining::Regular);
		Match->StartCombat(); Match->UpdateTeamOrders(0);
	}
	if (Match->Phase == EBattleMatchPhase::Combat)
	{
		if (Match->Elapsed >= MatchSoakNextOrder)
		{
			MatchSoakNextOrder = Match->Elapsed + 5;
			int32 Alive = 0; for (ABattleUnit* Unit : Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0) ++Alive;
			if (Alive < 10) Match->HireSquad(0, Match->Credits[0] >= 900 ? EBattleTraining::Regular : EBattleTraining::Recruit);
			Match->UpdateTeamOrders(0);
			if (Match->Credits[0] >= 1300) for (ABattleUnit* Unit : Units) if (Unit && Unit->IsAlive() && Unit->GetState()->Team == 0 && Match->RequestSupport(Unit->GetUnitID())) break;
		}
	}
	if (Match->Phase != EBattleMatchPhase::Finished && GetWorld()->GetTimeSeconds() < 200) return;
	bMatchSoakComplete = true;
	TSet<int32> IDs; int32 Alive[2] = {0, 0}; int32 Shots = 0;
	bool bValid = Match->Phase == EBattleMatchPhase::Finished && Match->Credits[0] >= 0 && Match->Credits[1] >= 0;
	for (ABattleUnit* Unit : Units) if (Unit && Unit->GetState())
	{
		const auto* S = Unit->GetState(); IDs.Add(S->UnitID); if (S->IsAlive()) ++Alive[S->Team]; Shots += S->ShotSequence;
		bValid &= S->Ammo >= 0 && S->Reserve >= 0 && FMath::IsFinite(S->HP);
	}
	bValid &= IDs.Num() == Units.Num() && Shots > 0;
	auto Json = MakeShared<FJsonObject>(); Json->SetBoolField(TEXT("success"), bValid); Json->SetNumberField(TEXT("seconds"), Match->Elapsed);
	Json->SetNumberField(TEXT("winner"), Match->Winner); Json->SetStringField(TEXT("result"), Result); Json->SetBoolField(TEXT("save_reload"), false);
	Json->SetNumberField(TEXT("allies_alive"), Alive[0]); Json->SetNumberField(TEXT("enemies_alive"), Alive[1]); Json->SetNumberField(TEXT("units_deployed"), Units.Num()); Json->SetNumberField(TEXT("shots"), Shots);
	Json->SetNumberField(TEXT("ally_score"), Match->Score[0]); Json->SetNumberField(TEXT("enemy_score"), Match->Score[1]); Json->SetNumberField(TEXT("ally_credits"), Match->Credits[0]); Json->SetNumberField(TEXT("enemy_credits"), Match->Credits[1]);
	Json->SetNumberField(TEXT("ally_support_used"), Match->SupportUsed[0]); Json->SetNumberField(TEXT("enemy_support_used"), Match->SupportUsed[1]);
	FString Text, Path; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)); FParse::Value(FCommandLine::Get(), TEXT("BattleMatchReport="), Path);
	if (!Path.IsEmpty()) bValid &= FFileHelper::SaveStringToFile(Text, *Path);
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_MATCH_RUN success=%d winner=%d time=%.1f save=%d shots=%d"), bValid, Match->Winner, Match->Elapsed, false, Shots);
	FPlatformMisc::RequestExitWithStatus(false, bValid ? 0 : 1);
}
