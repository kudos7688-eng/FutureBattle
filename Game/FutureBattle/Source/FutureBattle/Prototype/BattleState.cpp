#include "BattleState.h"

int32 UBattleState::RegisterUnit(int32 Team)
{
	const UBattleTuning* Tuning = GetDefault<UBattleTuning>();
	FBattleUnitState State;
	State.UnitID = NextID++;
	State.Team = Team;
	State.HP = FMath::Max(1.f, Tuning->Health);
	State.Ammo = FMath::Max(1, Tuning->Magazine);
	State.Reserve = FMath::Max(0, Tuning->ReserveAmmo);
	Units.Add(State.UnitID, State);
	return State.UnitID;
}

bool UBattleState::SetOrder(int32 ID, EBattleOrder Order, FVector Destination)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || Destination.ContainsNaN()) return false;
	State->Order = Order;
	State->Destination = Destination;
	return true;
}

bool UBattleState::ConsumeShot(int32 ID, double Now, double Interval)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || State->Ammo <= 0 || State->ReloadEndTime > 0. || Now < State->NextShotTime) return false;
	--State->Ammo;
	State->NextShotTime = Now + FMath::Max(0.05, Interval);
	return true;
}

bool UBattleState::StartReload(int32 ID, double Now)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || State->ReloadEndTime > 0. || State->Reserve <= 0 || State->Ammo >= GetDefault<UBattleTuning>()->Magazine) return false;
	State->ReloadEndTime = Now + FMath::Max(0.1f, GetDefault<UBattleTuning>()->ReloadSeconds);
	return true;
}

void UBattleState::AdvanceReloads(double Now)
{
	for (auto& Pair : Units)
	{
		FBattleUnitState& State = Pair.Value;
		if (State.ReloadEndTime > 0. && Now >= State.ReloadEndTime)
		{
			if (State.IsAlive())
			{
				const int32 Count = FMath::Min(State.Reserve, FMath::Max(0, GetDefault<UBattleTuning>()->Magazine - State.Ammo));
				State.Ammo += Count;
				State.Reserve -= Count;
			}
			State.ReloadEndTime = 0.;
		}
	}
}

bool UBattleState::ApplyDamage(int32 ID, int32 AttackingTeam, float Damage)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || State->Team == AttackingTeam || !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	State->HP = FMath::Max(0.f, State->HP - Damage);
	return true;
}
