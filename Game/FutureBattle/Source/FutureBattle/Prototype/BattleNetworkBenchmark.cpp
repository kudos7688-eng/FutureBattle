#include "BattleNetwork.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "FutureBattle.h"
#include "Engine/NetConnection.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

void ABattleNetController::ServerEcho_Implementation(double ClientStamp)
{
	// Diagnostic echo only; client clocks never affect simulation or damage.
	if (FMath::IsFinite(ClientStamp)) ClientEcho(ClientStamp);
}

void ABattleNetController::ClientEcho_Implementation(double ClientStamp)
{
	const double RTT = (FPlatformTime::Seconds() - ClientStamp) * 1000.;
	if (NetBenchStage == 1 && RTT >= 0 && RTT < 10000) NetBenchRTT.Add(RTT);
}

void ABattleNetController::UpdateNetworkBenchmark()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("BattleNetBenchmark")) || View.Team < 0) return;
	const double Now = FPlatformTime::Seconds();
	const bool bPilotBenchmark = FParse::Param(FCommandLine::Get(), TEXT("BattleNetBenchmarkPilot"));
	if (NetBenchStage == 2) { if (Now > NetBenchStart + 3) FPlatformMisc::RequestExit(false); return; }
	if (!View.bRunning) return;
	UNetConnection* Connection = GetNetConnection(); if (!Connection) return;
	if (!NetBenchStart)
	{
		NetBenchStart = Now;
		if (bPilotBenchmark) ServerControl(SelectedUnitID, NextControlRequest++);
		else if (CameraPawn) { CameraPawn->SetActorLocation(FVector::ZeroVector); CameraPawn->Zoom(1900); }
		return;
	}
	if (NetBenchStage == 0)
	{
		if (Now - NetBenchStart < 3) return;
		if (bPilotBenchmark && View.ControlledID == INDEX_NONE)
		{
			UE_LOG(LogFutureBattle, Error, TEXT("BATTLE_NET_RESULT success=0 team=%d pilot benchmark claim failed"), View.Team);
			FPlatformMisc::RequestExit(false); return;
		}
		NetBenchStage = 1; NetBenchStart = Now; NetBenchLastFrame = Now;
		NetBenchInBytes = Connection->InTotalBytes; NetBenchOutBytes = Connection->OutTotalBytes;
		NetBenchInPackets = Connection->InTotalPackets; NetBenchOutPackets = Connection->OutTotalPackets;
	}
	else { NetBenchFrameMS.Add((Now - NetBenchLastFrame) * 1000); NetBenchLastFrame = Now; }
	++NetBenchFrames;
	if (bPilotBenchmark && Now >= NextInputAt)
	{
		NextInputAt = Now + .05;
		const float Direction = FMath::Fmod(Now - NetBenchStart, 3.) < 1.5 ? 1.f : -1.f;
		ServerDirectInput(View.ControlEpoch, NextInputSequence++, FVector2D(Direction, 0), FRotator(0, View.Team ? 180 : 0, 0), true, false, false);
		for (const auto& U : View.Units) if (U.ID == View.ControlledID && U.Ammo == 0 && U.ReloadRemaining <= 0) ServerReload(View.ControlEpoch);
	}
	if (Now >= NetBenchNextPing) { NetBenchNextPing = Now + .5; ServerEcho(Now); }
	// Move every owned squad into the public center lanes. No hidden enemy state is consulted.
	TArray<int32> Squads;
	for (const auto& U : View.Units) if (U.Team == View.Team && U.HP > 0) Squads.AddUnique(U.Squad);
	Squads.Sort();
	if (Now >= NetBenchNextOrder && NetBenchOrderIndex < Squads.Num())
	{
		const int32 Index = NetBenchOrderIndex++;
		ServerOrder(Squads[Index], EBattleOrder::AttackMove, FVector(View.Team ? 700 : -700, -1900 + (Index % 10) * 380, 0), NextRequest++);
		NetBenchNextOrder = Now + .2;
	}
	float Duration = 20; FParse::Value(FCommandLine::Get(), TEXT("BattleNetSeconds="), Duration); Duration = FMath::Clamp(Duration, 5.f, 120.f);
	const double Elapsed = Now - NetBenchStart; if (Elapsed < Duration) return;
	auto Report = MakeShared<FJsonObject>();
	Report->SetNumberField(TEXT("wire_format"), 2);
	Report->SetStringField(TEXT("camera"), bPilotBenchmark ? TEXT("first_person") : TEXT("overview_4500cm"));
	Report->SetBoolField(TEXT("animation_lod"), !FParse::Param(FCommandLine::Get(), TEXT("BattleNoAnimationLOD")));
	Report->SetBoolField(TEXT("pilot_mode"), bPilotBenchmark); Report->SetNumberField(TEXT("pilot_view_updates"), NetBenchPilotViews);
	int32 Lag = 0, Loss = 0; FParse::Value(FCommandLine::Get(), TEXT("PktLag="), Lag); FParse::Value(FCommandLine::Get(), TEXT("PktLoss="), Loss);
	Report->SetNumberField(TEXT("simulated_one_way_lag_ms"), Lag); Report->SetNumberField(TEXT("simulated_packet_loss_percent"), Loss);
	Report->SetNumberField(TEXT("tactical_update_hz"), 5); Report->SetNumberField(TEXT("pilot_update_hz"), 20);
	int32 UnitCount = 20; FParse::Value(FCommandLine::Get(), TEXT("BattleNetUnits="), UnitCount);
	Report->SetNumberField(TEXT("unit_count"), UnitCount); Report->SetNumberField(TEXT("team"), View.Team);
	Report->SetNumberField(TEXT("seconds"), Elapsed); Report->SetBoolField(TEXT("rendered"), !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")));
	Report->SetNumberField(TEXT("warmup_seconds"), 3); Report->SetNumberField(TEXT("initial_health"), 10000);
	Report->SetNumberField(TEXT("received_bytes"), Connection->InTotalBytes - NetBenchInBytes);
	Report->SetNumberField(TEXT("sent_bytes"), Connection->OutTotalBytes - NetBenchOutBytes);
	Report->SetNumberField(TEXT("received_bytes_per_second"), (Connection->InTotalBytes - NetBenchInBytes) / Elapsed);
	Report->SetNumberField(TEXT("sent_bytes_per_second"), (Connection->OutTotalBytes - NetBenchOutBytes) / Elapsed);
	Report->SetNumberField(TEXT("received_packets"), Connection->InTotalPackets - NetBenchInPackets);
	Report->SetNumberField(TEXT("sent_packets"), Connection->OutTotalPackets - NetBenchOutPackets);
	Report->SetNumberField(TEXT("view_updates"), NetBenchViews); Report->SetNumberField(TEXT("peak_visible_units"), NetBenchPeakVisible);
	auto Stats = [](TArray<double> Values)
	{
		auto Result = MakeShared<FJsonObject>(); double Sum = 0; for (double V : Values) Sum += V; Values.Sort();
		Result->SetNumberField(TEXT("samples"), Values.Num()); Result->SetNumberField(TEXT("mean"), Values.IsEmpty() ? 0 : Sum / Values.Num());
		Result->SetNumberField(TEXT("p95"), Values.IsEmpty() ? 0 : Values[FMath::Clamp(FMath::CeilToInt(Values.Num() * .95) - 1, 0, Values.Num() - 1)]);
		Result->SetNumberField(TEXT("max"), Values.IsEmpty() ? 0 : Values.Last()); return Result;
	};
	Report->SetObjectField(TEXT("client_frame_wall_ms"), Stats(NetBenchFrameMS)); Report->SetObjectField(TEXT("rpc_round_trip_ms"), Stats(NetBenchRTT));
	bool bPrivate = true; int32 Own = 0, Shots = 0;
	for (const auto& U : View.Units)
	{
		if (U.Team == View.Team) { ++Own; Shots += U.ShotSequence; }
		else if (U.Squad != INDEX_NONE || U.Ammo || U.Reserve || U.Destination != FVector::ZeroVector) bPrivate = false;
	}
	Report->SetNumberField(TEXT("own_units"), Own); Report->SetNumberField(TEXT("own_shots"), Shots);
	Report->SetBoolField(TEXT("enemy_inventory_private"), bPrivate);
	const bool bSuccess = bPrivate && Own == UnitCount / 2 && NetBenchViews > 10 && !NetBenchRTT.IsEmpty() && NetBenchPeakVisible > UnitCount / 2 && (!bPilotBenchmark || NetBenchPilotViews > Duration * 10);
	Report->SetBoolField(TEXT("success"), bSuccess);
	FString Path; FParse::Value(FCommandLine::Get(), TEXT("BattleNetReport="), Path);
	FString JSON; const auto Writer = TJsonWriterFactory<>::Create(&JSON); FJsonSerializer::Serialize(Report, Writer);
	const bool bWritten = !Path.IsEmpty() && FFileHelper::SaveStringToFile(JSON, *Path);
	FString Screenshot; FParse::Value(FCommandLine::Get(), TEXT("BattleNetScreenshot="), Screenshot);
	if (!Screenshot.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
	UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_NET_RESULT success=%d team=%d benchmark=1 units=%d visible=%d receive_Bps=%.1f send_Bps=%.1f"), bSuccess && bWritten, View.Team, UnitCount, NetBenchPeakVisible, (Connection->InTotalBytes - NetBenchInBytes) / Elapsed, (Connection->OutTotalBytes - NetBenchOutBytes) / Elapsed);
	NetBenchStage = 2; NetBenchStart = Now;
}
