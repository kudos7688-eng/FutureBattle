#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleState.generated.h"

UENUM(BlueprintType)
enum class EBattleOrder : uint8 { Hold, Move };

// Authoritative match data. Possession and Actor lifetime never create/reset this record.
USTRUCT(BlueprintType)
struct FBattleUnitState
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 UnitID = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Team = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HP = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Ammo = 30;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Reserve = 90;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EBattleOrder Order = EBattleOrder::Hold;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Destination = FVector::ZeroVector;
	UPROPERTY() double NextShotTime = 0.;
	UPROPERTY() double ReloadEndTime = 0.;
	bool IsAlive() const { return HP > 0.f; }
};

// Tune the prototype without recompiling via DefaultGame.ini.
UCLASS(Config=Game, DefaultConfig)
class UBattleTuning : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere) float Health = 100.f;
	UPROPERTY(Config, EditAnywhere) int32 Magazine = 30;
	UPROPERTY(Config, EditAnywhere) int32 ReserveAmmo = 90;
	UPROPERTY(Config, EditAnywhere) float ShotDamage = 10.f;
	UPROPERTY(Config, EditAnywhere) float PlayerShotInterval = 0.16f;
	UPROPERTY(Config, EditAnywhere) float AIShotInterval = 1.4f;
	UPROPERTY(Config, EditAnywhere) float ReloadSeconds = 1.8f;
	UPROPERTY(Config, EditAnywhere) float SightRange = 2400.f;
	UPROPERTY(Config, EditAnywhere) float WeaponRange = 5000.f;
	UPROPERTY(Config, EditAnywhere) float MoveSpeed = 420.f;
};

UCLASS()
class UBattleState : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	int32 RegisterUnit(int32 Team);
	const FBattleUnitState* Find(int32 ID) const { return Units.Find(ID); }
	bool SetOrder(int32 ID, EBattleOrder Order, FVector Destination);
	bool ConsumeShot(int32 ID, double Now, double Interval);
	bool StartReload(int32 ID, double Now);
	void AdvanceReloads(double Now);
	bool ApplyDamage(int32 ID, int32 AttackingTeam, float Damage);
	const TMap<int32, FBattleUnitState>& GetUnits() const { return Units; }
private:
	UPROPERTY() TMap<int32, FBattleUnitState> Units;
	int32 NextID = 1;
};
