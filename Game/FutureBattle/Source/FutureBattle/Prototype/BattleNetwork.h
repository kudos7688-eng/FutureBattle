#pragma once
#include "CoreMinimal.h"
#include "BattleGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "BattleState.h"
#include "BattleNetwork.generated.h"
class ABattleUnit;
class ABattleCameraPawn;

USTRUCT()
struct FBattleNetUnit
{
	GENERATED_BODY()
	UPROPERTY() int32 ID = INDEX_NONE;
	UPROPERTY() int32 Team = 0;
	UPROPERTY() int32 Squad = INDEX_NONE;
	UPROPERTY() float HP = 0;
	UPROPERTY() int32 Ammo = 0;
	UPROPERTY() int32 Reserve = 0;
	UPROPERTY() int32 ShotSequence = 0;
	UPROPERTY() float ReloadRemaining = 0;
	UPROPERTY() EBattleOrder Order = EBattleOrder::Hold;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FVector Destination = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FBattleNetUnit> : TStructOpsTypeTraitsBase2<FBattleNetUnit>
{
	enum { WithNetSerializer = true };
};

USTRUCT()
struct FBattleNetFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Team = INDEX_NONE;
	UPROPERTY() int32 Sequence = 0;
	UPROPERTY() bool bRunning = false;
	UPROPERTY() bool bEnded = false;
	UPROPERTY() int32 ControlledID = INDEX_NONE;
	UPROPERTY() int32 ControlEpoch = 0;
	UPROPERTY() TArray<FBattleNetUnit> Units;
};

UCLASS()
class ABattleNetController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	int32 ServerTeam = INDEX_NONE;
	FBattleNetFrame View;
	int32 SelectedSquad = INDEX_NONE;
	int32 SelectedUnitID = INDEX_NONE;
	FString Notice;
	UPROPERTY() TMap<int32, TObjectPtr<ABattleUnit>> Proxies;
	void SendView();
	void SendPilotView();
	void TickDirect(float DeltaTime);
	void ReleaseDirect();
	UFUNCTION(Client, Unreliable) void ClientView(const FBattleNetFrame& Frame);
	UFUNCTION(Client, Unreliable) void ClientPilotView(const FBattleNetUnit& Unit, int32 Epoch, int32 Sequence);
	UFUNCTION(Server, Reliable) void ServerOrder(int32 SquadID, EBattleOrder Order, FVector Target, int32 Sequence);
	UFUNCTION(Client, Reliable) void ClientOrderResult(int32 Sequence, bool bAccepted, const FString& Message);
	UFUNCTION(Server, Reliable) void ServerControl(int32 UnitID, int32 Sequence);
	UFUNCTION(Client, Reliable) void ClientControlResult(int32 Sequence, bool bAccepted, const FString& Message);
	UFUNCTION(Server, Unreliable) void ServerDirectInput(int32 Epoch, int32 Sequence, FVector2D Move, FRotator Aim, bool bFire, bool bJump, bool bSprint);
	UFUNCTION(Server, Reliable) void ServerReload(int32 Epoch);
	UFUNCTION(Server, Unreliable) void ServerEcho(double ClientStamp);
	UFUNCTION(Client, Unreliable) void ClientEcho(double ClientStamp);
private:
	UPROPERTY() TObjectPtr<ABattleCameraPawn> CameraPawn;
	EBattleOrder PendingOrder = EBattleOrder::Move;
	int32 NextRequest = 1, LastServerRequest = 0, NextFrame = 1;
	int32 LastPilotFrame = 0;
	int32 NetBenchPilotViews = 0;
	double LastServerOrderAt = -100;
	void SendSelectedOrder(FVector Target, EBattleOrder Order);
	void RunNetworkProbe();
	void RunAdmissionProbe();
	int32 AdmissionProbeState = -1;
	double AdmissionLeaveAt = 0;
	int32 ProbeStage = 0;
	double NextProbeAt = 0;
	bool bProbeOwnAccepted = false, bProbeForeignRejected = false, bProbeInvalidRejected = false;
	bool bProbeReplayRejected = false, bProbeRateRejected = false;
	FVector ProbeStart;
	UPROPERTY() TObjectPtr<ABattleUnit> ServerControlled;
	int32 ControlEpoch = 0, LastControlRequest = 0, LastInputSequence = 0;
	int32 NextControlRequest = 1, NextInputSequence = 1;
	double LastInputAt = -100, NextInputAt = 0;
	double LastControlAt = -100;
	FVector2D PilotMove = FVector2D::ZeroVector;
	FRotator PilotAim = FRotator::ZeroRotator;
	bool bPilotFire = false, bPilotJump = false, bPilotSprint = false, bLocalFire = false;
	void UpdateLocalPilot(float DeltaTime);
	void UpdatePilotCamera(int32 PreviousID, int32 PreviousEpoch);
	void RunPilotProbe();
	bool bPilotClaimed = false, bPilotForeignRejected = false, bPilotReleased = false;
	int32 PilotProbeStage = 0;
	double PilotProbeAt = 0;
	FVector PilotProbeStart;
	float PilotProbeHP = 0;
	int32 PilotProbeAmmo = 0, PilotProbeReserve = 0, PilotProbeUnitID = INDEX_NONE;
 	EBattleOrder PilotProbeOrder = EBattleOrder::Hold;
	void UpdateNetworkBenchmark();
	double NetBenchStart = 0, NetBenchLastFrame = 0, NetBenchNextOrder = 0, NetBenchNextPing = 0;
	int32 NetBenchStage = 0, NetBenchOrderIndex = 0, NetBenchFrames = 0, NetBenchViews = 0;
	int32 NetBenchInBytes = 0, NetBenchOutBytes = 0, NetBenchInPackets = 0, NetBenchOutPackets = 0;
	int32 NetBenchPeakVisible = 0;
	TArray<double> NetBenchFrameMS, NetBenchRTT;
};

UCLASS()
class ABattleNetGameMode : public ABattleGameMode
{
	GENERATED_BODY()
public:
	ABattleNetGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;
protected:
	virtual int32 InitialUnitCount() const override;
	virtual float InitialUnitHealth() const override;
private:
	double NextNetworkFrame = 0;
	double NextPilotFrame = 0;
	bool bStarted = false;
};

UCLASS()
class ABattleNetHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	double NextLabelCheck = 0;
	TSet<int32> ClearLabels;
};
