#include "BattleNetwork.h"
#include "BattleUnit.h"
#include "FutureBattle.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

void ABattleNetController::RunAdmissionProbe()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("BattleNetHoldProbe")) || View.Team < 0) return;
	float LeaveAfter = 0; FParse::Value(FCommandLine::Get(), TEXT("BattleNetLeaveAfter="), LeaveAfter);
	if (LeaveAfter > 0 && View.bRunning && !AdmissionLeaveAt) AdmissionLeaveAt = GetWorld()->GetTimeSeconds() + LeaveAfter;
	if (AdmissionLeaveAt > 0 && GetWorld()->GetTimeSeconds() >= AdmissionLeaveAt) { FPlatformMisc::RequestExit(false); return; }
	const int32 State = View.bRunning ? 1 : 0;
	if (State == AdmissionProbeState) return; AdmissionProbeState = State;
	int32 Own = 0, Enemy = 0; for (const auto& U : View.Units) { if (U.Team == View.Team) ++Own; else ++Enemy; }
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_SLOT team=%d running=%d own=%d enemy=%d"), View.Team, State, Own, Enemy);
}

void ABattleNetController::RunNetworkProbe()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("BattleNetProbe")) || View.Team < 0) return;
	const double Now = GetWorld()->GetTimeSeconds();
	int32 UnitCount = 20; FParse::Value(FCommandLine::Get(), TEXT("BattleNetUnits="), UnitCount);
	if (ProbeStage == 7) { if (Now >= NextProbeAt) FPlatformMisc::RequestExit(false); return; }
	if (!View.bRunning || Now < NextProbeAt) return;
	auto Finish = [&](bool bSuccess, const TCHAR* Reason)
	{
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_RESULT success=%d team=%d own=%d foreign=%d invalid=%d %s"), bSuccess, View.Team, bProbeOwnAccepted, bProbeForeignRejected, bProbeInvalidRejected, Reason);
		ProbeStage = 7; NextProbeAt = Now + 3;
	};
	if (ProbeStage == 0)
	{
		int32 Own = 0, Enemy = 0; for (const auto& U : View.Units) { if (U.Team == View.Team) ++Own; else ++Enemy; }
		if (Own != UnitCount / 2 || Enemy != 0) { Finish(false, TEXT("Initial private view leaked or omitted units")); return; }
		for (const auto& U : View.Units) if (U.Squad == SelectedSquad) { ProbeStart = U.Position; break; }
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_PRIVATE team=%d friendly=%d enemies=%d"), View.Team, Own, Enemy);
		ServerOrder(SelectedSquad, EBattleOrder::Move, FVector(View.Team ? 900 : -900, -2100, 0), 1);
		ProbeStage = 1; NextProbeAt = Now + .6; return;
	}
	if (ProbeStage == 1)
	{
		ServerOrder(View.Team ? 1 : UnitCount / 10 + 1, EBattleOrder::Hold, FVector::ZeroVector, 2);
		ProbeStage = 2; NextProbeAt = Now + .6; return;
	}
	if (ProbeStage == 2)
	{
		ServerOrder(SelectedSquad, EBattleOrder::Move, FVector(100000, 100000, 0), 3);
		ProbeStage = 3; NextProbeAt = Now + 5; return;
	}
	if (ProbeStage == 4)
	{
		int32 Visible = 0; for (const auto& U : View.Units) if (U.Team != View.Team) ++Visible;
		if (Visible > 0 && Now < 40) { NextProbeAt = Now + 1; return; }
		int32 Actors = 0; for (TActorIterator<ABattleUnit> It(GetWorld()); It; ++It) ++Actors;
		if (Visible != 0 || Actors != Proxies.Num() || Proxies.Num() != UnitCount / 2) { Finish(false, TEXT("Lost contact remained on client")); return; }
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_CULL team=%d visible=0 proxies=%d"), View.Team, Proxies.Num());
		ServerOrder(SelectedSquad, EBattleOrder::Hold, FVector::ZeroVector, 4); // Deliberate replay.
		ProbeStage = 5; NextProbeAt = Now + .5; return;
	}
	if (ProbeStage == 5)
	{
		ServerOrder(SelectedSquad, EBattleOrder::Hold, FVector::ZeroVector, 5);
		ServerOrder(SelectedSquad, EBattleOrder::Hold, FVector::ZeroVector, 6);
		ProbeStage = 6; NextProbeAt = Now + .5; return;
	}
	if (ProbeStage == 6)
	{
		Finish(bProbeReplayRejected && bProbeRateRejected, *FString::Printf(TEXT("cull=1 replay=%d rate=%d"), bProbeReplayRejected, bProbeRateRejected)); return;
	}
	bool bMoved = false; int32 Visible = 0;
	for (const auto& U : View.Units)
	{
		if (U.Team == View.Team && U.Squad == SelectedSquad && FVector::Dist2D(ProbeStart, U.Position) > 200) bMoved = true;
		if (U.Team != View.Team) ++Visible;
	}
	if (Visible == 0 && Now < 30) { NextProbeAt = Now + 1; return; }
	int32 Actors = 0; for (TActorIterator<ABattleUnit> It(GetWorld()); It; ++It) ++Actors;
	const bool bSuccess = bProbeOwnAccepted && bProbeForeignRejected && bProbeInvalidRejected && bMoved && Visible > 0 && Actors == Proxies.Num();
	if (!bSuccess) { Finish(false, *FString::Printf(TEXT("moved=%d visible=%d proxies=%d actors=%d"), bMoved, Visible, Proxies.Num(), Actors)); return; }
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_CONTACT team=%d moved=1 visible=%d proxies=%d"), View.Team, Visible, Proxies.Num());
	ServerOrder(SelectedSquad, EBattleOrder::Move, FVector(View.Team ? 2900 : -2900, -2100, 0), 4);
	ProbeStage = 4; NextProbeAt = Now + 7;
}
