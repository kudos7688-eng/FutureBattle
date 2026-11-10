#include "BattleNetwork.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"

void ABattleNetController::RunPilotProbe()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("BattleNetPilotProbe")) || View.Team < 0) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (PilotProbeStage == 10) { if (Now >= PilotProbeAt) FPlatformMisc::RequestExit(false); return; }
	if (!View.bRunning) return;
	auto Finish = [&](bool bSuccess, const TCHAR* Why)
	{
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_RESULT success=%d team=%d pilot=1 %s"), bSuccess, View.Team, Why);
		PilotProbeStage = 10; PilotProbeAt = Now + 3;
	};
	if (PilotProbeStage == 0)
	{
		for (const auto& U : View.Units) if (U.ID == SelectedUnitID)
		{
			PilotProbeUnitID = U.ID; PilotProbeStart = U.Position; PilotProbeHP = U.HP; PilotProbeAmmo = U.Ammo; PilotProbeReserve = U.Reserve; PilotProbeOrder = U.Order;
		}
		int32 UnitCount = 20; FParse::Value(FCommandLine::Get(), TEXT("BattleNetUnits="), UnitCount);
		ServerControl(View.Team ? 1 : UnitCount / 2 + 1, 1); PilotProbeStage = 1; PilotProbeAt = Now + .4; return;
	}
	if (PilotProbeStage == 3)
	{
		if (Now < PilotProbeAt)
		{
			if (Now >= NextInputAt) { NextInputAt = Now + .05; ServerDirectInput(View.ControlEpoch, NextInputSequence++, FVector2D(1, 0), FRotator(60, View.Team ? 180 : 0, 0), true, false); }
			return;
		}
		PilotProbeStage = 4; PilotProbeAt = Now + .8; return; // Stop sending, exercising the stale-input timeout.
	}
	if (Now < PilotProbeAt) return;
	const FBattleNetUnit* Current = nullptr; for (const auto& U : View.Units) if (U.ID == PilotProbeUnitID) { Current = &U; break; }
	if (!Current) { Finish(false, TEXT("Controlled unit missing from private view")); return; }
	if (PilotProbeStage == 1)
	{
		if (!bPilotForeignRejected || View.ControlledID != INDEX_NONE) { Finish(false, TEXT("Foreign control was not rejected")); return; }
		ServerControl(PilotProbeUnitID, 2); PilotProbeStage = 2; PilotProbeAt = Now + .5; return;
	}
	if (PilotProbeStage == 2)
	{
		if (!bPilotClaimed || View.ControlledID != PilotProbeUnitID || Current->HP != PilotProbeHP || Current->Ammo != PilotProbeAmmo || Current->Order != PilotProbeOrder) { Finish(false, TEXT("Control claim lost canonical state")); return; }
		PilotProbeStage = 3; PilotProbeAt = Now + .8; NextInputAt = 0; return;
	}
	if (PilotProbeStage == 4)
	{
		const float Distance = FVector::Dist2D(PilotProbeStart, Current->Position); const int32 Spent = PilotProbeAmmo - Current->Ammo;
		if (Distance < 100 || Distance > 500 || Spent < 1 || Spent > 8 || Current->HP != PilotProbeHP) { Finish(false, *FString::Printf(TEXT("Invalid server movement or fire rate: distance=%.1f spent=%d"), Distance, Spent)); return; }
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_PILOT team=%d moved=%.1f spent=%d timeout=1"), View.Team, Distance, Spent);
		PilotProbeStart = Current->Position; PilotProbeAmmo = Current->Ammo;
		ServerDirectInput(View.ControlEpoch - 1, NextInputSequence++, FVector2D(1, 0), FRotator::ZeroRotator, true, false);
		ServerDirectInput(View.ControlEpoch, NextInputSequence++, FVector2D(100, 0), FRotator::ZeroRotator, true, false);
		PilotProbeStage = 5; PilotProbeAt = Now + .5; return;
	}
	if (PilotProbeStage == 5)
	{
		if (Current->Ammo != PilotProbeAmmo || FVector::Dist2D(Current->Position, PilotProbeStart) > 8) { Finish(false, TEXT("Stale epoch or oversized input changed the server unit")); return; }
		ServerReload(View.ControlEpoch); ServerControl(INDEX_NONE, 3);
		PilotProbeStage = 6; PilotProbeAt = Now + .4; return;
	}
	if (PilotProbeStage == 6)
	{
		if (!bPilotReleased || View.ControlledID != INDEX_NONE || Current->ReloadRemaining <= 0 || Current->Ammo != PilotProbeAmmo || Current->Order != PilotProbeOrder) { Finish(false, TEXT("Release interrupted reload or persistent order")); return; }
		ServerControl(PilotProbeUnitID, 4); PilotProbeStage = 7; PilotProbeAt = Now + .4; return;
	}
	if (PilotProbeStage == 7)
	{
		if (View.ControlledID != PilotProbeUnitID || Current->ReloadRemaining <= 0 || Current->Ammo != PilotProbeAmmo) { Finish(false, TEXT("Reclaim interrupted reload")); return; }
		PilotProbeStage = 8; PilotProbeAt = Now + 1.5; return;
	}
	if (PilotProbeStage == 8)
	{
		// Completion is authoritative; a 5 Hz view can legitimately still contain the final reload frame.
		if (Current->ReloadRemaining > 0 && Now < PilotProbeAt + 2) return;
		if (Current->Ammo != 30 || Current->Reserve != PilotProbeReserve - (30 - PilotProbeAmmo) || Current->ReloadRemaining > 0 || Current->HP != PilotProbeHP || Current->Order != PilotProbeOrder)
		{
			Finish(false, *FString::Printf(TEXT("Reload mismatch ammo=%d reserve=%d expected=%d remaining=%.3f HP=%.1f expectedHP=%.1f order=%d expectedOrder=%d"), Current->Ammo, Current->Reserve, PilotProbeReserve - (30 - PilotProbeAmmo), Current->ReloadRemaining, Current->HP, PilotProbeHP, static_cast<int32>(Current->Order), static_cast<int32>(PilotProbeOrder))); return;
		}
		// A tactical order may change while this member is controlled, without AI stealing its movement.
		PilotProbeStart = Current->Position; PilotProbeAmmo = Current->Ammo; PilotProbeReserve = Current->Reserve;
		ServerOrder(Current->Squad, EBattleOrder::Move, FVector(View.Team ? 2500 : -2500, Current->Position.Y, 0), 1);
		PilotProbeStage = 9; PilotProbeAt = Now + .6; return;
	}
	if (PilotProbeStage == 9)
	{
		if (!bProbeOwnAccepted || View.ControlledID != PilotProbeUnitID || Current->Order != EBattleOrder::Move || FVector::Dist2D(Current->Position, PilotProbeStart) > 8)
		{ Finish(false, TEXT("Tactical order stole direct control or was not retained")); return; }
		ServerControl(INDEX_NONE, 5); PilotProbeStage = 11; PilotProbeAt = Now + 2; return;
	}
	if (PilotProbeStage == 11)
	{
		if (View.ControlledID != INDEX_NONE || Current->Order != EBattleOrder::Move || FVector::Dist2D(Current->Position, PilotProbeStart) < 100 || Current->Ammo != PilotProbeAmmo || Current->Reserve != PilotProbeReserve)
		{ Finish(false, TEXT("AI did not execute the retained order after release")); return; }
		ServerControl(PilotProbeUnitID, 6); PilotProbeStage = 12; PilotProbeAt = Now + .5; return;
	}
	if (PilotProbeStage == 12)
	{
		if (View.ControlledID != PilotProbeUnitID || Current->Order != EBattleOrder::Move) { Finish(false, TEXT("Reclaim lost the updated tactical order")); return; }
		// Intentionally exit while holding the lease. The server must release it on disconnect.
		Finish(true, TEXT("foreign=1 state=1 movement=1 rate=1 stale=1 reload=1 order_resume=1 disconnect_lease=1")); return;
	}
}
