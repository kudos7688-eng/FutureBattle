#include "BattleState.h"
#include "BattleMetrics.h"

void UBattleState::RestoreRecords(const TArray<FBattleUnitState>& SavedUnits, const TArray<FBattleSquadState>& SavedSquads)
{
	Units.Reset(); Squads.Reset(); NextID = NextSquadID = 1;
	for (const auto& Unit : SavedUnits) { Units.Add(Unit.UnitID, Unit); NextID = FMath::Max(NextID, Unit.UnitID + 1); }
	for (const auto& Squad : SavedSquads) { Squads.Add(Squad.SquadID, Squad); NextSquadID = FMath::Max(NextSquadID, Squad.SquadID + 1); }
}

int32 UBattleState::AddReserve(int32 ID, int32 Rounds)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !Unit->IsAlive() || Rounds <= 0) return 0;
	const int32 Added = FMath::Min(Rounds, FMath::Max(0, GetDefault<UBattleTuning>()->ReserveAmmo - Unit->Reserve));
	Unit->Reserve += Added; return Added;
}

int32 UBattleState::RegisterUnit(int32 Team, float InitialHealth, EBattleWeapon Weapon)
{
	const UBattleTuning* Tuning = GetDefault<UBattleTuning>();
	FBattleUnitState State;
	State.UnitID = NextID++;
	State.Team = Team;
	State.HP = FMath::Max(1.f, InitialHealth > 0.f && FMath::IsFinite(InitialHealth) ? InitialHealth : Tuning->Health);
	State.Weapon = Weapon;
	State.Ammo = FMath::Max(1, Tuning->Capacity(Weapon));
	State.Reserve = FMath::Max(0, Tuning->ReserveAmmo);
	Units.Add(State.UnitID, State);
	SetTrainingPreset(State.UnitID, EBattleTraining::Regular);
	return State.UnitID;
}

bool UBattleState::SetTrainingPreset(int32 ID, EBattleTraining Training)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !Unit->IsAlive() || static_cast<uint8>(Training) > static_cast<uint8>(EBattleTraining::Veteran)) return false;
	const auto* T = GetDefault<UBattleTuning>();
	const bool bRecruit = Training == EBattleTraining::Recruit, bVeteran = Training == EBattleTraining::Veteran;
	FBattlePerceptionProfile P;
	P.SightRange = T->SightRange * (bRecruit ? .75f : bVeteran ? 1.25f : 1.f);
	P.FieldOfView = T->FieldOfView * (bRecruit ? .75f : bVeteran ? 1.25f : 1.f);
	P.DetectionSeconds = T->DetectionSeconds * (bRecruit ? 2.f : bVeteran ? .5f : 1.f);
	P.MemorySeconds = T->MemorySeconds * (bRecruit ? .6f : bVeteran ? 1.6f : 1.f);
	P.ShareRange = T->ShareRange * (bRecruit ? .625f : bVeteran ? 1.375f : 1.f);
	P.ShareDelay = T->ShareDelay * (bRecruit ? 1.5f : bVeteran ? .5f : 1.f);
	if (!SetPerceptionProfile(ID, P)) return false;
	Unit->Training = Training;
	Unit->Decision.ThinkSeconds = FMath::Clamp(T->AIDecisionSeconds * (bRecruit ? 2.f : bVeteran ? .5f : 1.f), .1f, 2.f);
	Unit->Decision.CoverThinkSeconds = FMath::Clamp(T->TacticalDecisionSeconds * (bRecruit ? 2.f : bVeteran ? .5f : 1.f), .2f, 5.f);
	Unit->Decision.CoverCandidates = FMath::Clamp(bRecruit ? T->CoverCandidateLimit / 3 : bVeteran ? T->CoverCandidateLimit + 4 : T->CoverCandidateLimit, 1, 32);
	Unit->Decision.bCoordinate = !bRecruit;
	Unit->HireCost = FMath::Clamp(bRecruit ? T->RecruitCost : bVeteran ? T->VeteranCost : T->RegularCost, 0, 100000);
	++Unit->DecisionRevision;
	SetCoordination(ID, EBattleRole::Independent);
	return true;
}

bool UBattleState::SetPerceptionProfile(int32 ID, const FBattlePerceptionProfile& Profile)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !FMath::IsFinite(Profile.SightRange) || !FMath::IsFinite(Profile.FieldOfView) ||
		!FMath::IsFinite(Profile.DetectionSeconds) || !FMath::IsFinite(Profile.MemorySeconds) ||
		!FMath::IsFinite(Profile.ShareRange) || !FMath::IsFinite(Profile.ShareDelay)) return false;
	Unit->Perception = Profile;
	Unit->Perception.SightRange = FMath::Clamp(Profile.SightRange, 0.f, 20000.f);
	Unit->Perception.FieldOfView = FMath::Clamp(Profile.FieldOfView, 1.f, 360.f);
	Unit->Perception.DetectionSeconds = FMath::Clamp(Profile.DetectionSeconds, 0.f, 30.f);
	Unit->Perception.MemorySeconds = FMath::Clamp(Profile.MemorySeconds, 0.f, 120.f);
	Unit->Perception.ShareRange = FMath::Clamp(Profile.ShareRange, 0.f, 20000.f);
	Unit->Perception.ShareDelay = FMath::Clamp(Profile.ShareDelay, 0.f, 30.f);
	++Unit->PerceptionRevision;
	return true;
}

int32 UBattleState::CreateSquad(int32 Team, const TArray<int32>& Members)
{
	if (Members.IsEmpty() || Members.Num() > 5) return INDEX_NONE;
	TSet<int32> Unique;
	for (int32 ID : Members)
	{
		const auto* Unit = Find(ID);
		if (!Unit || Unit->Team != Team || Unit->SquadID != INDEX_NONE || Unique.Contains(ID)) return INDEX_NONE;
		Unique.Add(ID);
	}
	FBattleSquadState Squad;
	Squad.SquadID = NextSquadID++; Squad.Team = Team; Squad.Members = Members;
	Squad.DefenseRadius = FMath::Max(250.f, GetDefault<UBattleTuning>()->DefenseRadius);
	for (int32 ID : Members) Units.FindChecked(ID).SquadID = Squad.SquadID;
	Squads.Add(Squad.SquadID, Squad);
	return Squad.SquadID;
}

bool UBattleState::SetSquadOrder(int32 ID, EBattleOrder Order, FVector Destination, const TMap<int32, FVector>& Goals)
{
	auto* Squad = Squads.Find(ID);
	if (!Squad || Destination.ContainsNaN()) return false;
	int32 Living = 0;
	for (int32 Member : Squad->Members) if (Units.FindChecked(Member).IsAlive())
	{
		const FVector* Goal = Goals.Find(Member);
		if (!Goal || Goal->ContainsNaN()) return false;
		if (Order == EBattleOrder::Defend && FVector::DistSquared2D(*Goal, Destination) > FMath::Square(Squad->DefenseRadius - 60.f)) return false;
		++Living;
	}
	if (!Living) return false;
	Squad->Order = Order; Squad->Destination = Destination;
	++Squad->OrderRevision;
	for (int32 Member : Squad->Members) if (Units.FindChecked(Member).IsAlive()) SetOrder(Member, Order, Goals.FindChecked(Member));
	return true;
}

void UBattleState::SetAction(int32 ID, EBattleAction Action)
{
	if (auto* Unit = Units.Find(ID)) Unit->Action = Unit->IsAlive() ? Action : EBattleAction::Dead;
}

void UBattleState::SetMoveRetries(int32 ID, int32 Retries)
{
	if (auto* Unit = Units.Find(ID)) Unit->MoveRetries = FMath::Max(0, Retries);
}

bool UBattleState::SetOrder(int32 ID, EBattleOrder Order, FVector Destination)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || Destination.ContainsNaN()) return false;
	State->Order = Order;
	State->Destination = Destination;
	++State->OrderRevision;
	State->MoveRetries = 0;
	SetCoordination(ID, EBattleRole::Independent);
	if (State->Action != EBattleAction::DirectControl) State->Action = EBattleAction::Idle;
	return true;
}

bool UBattleState::ConsumeShot(int32 ID, double Now, double Interval)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || State->Ammo <= 0 || State->ReloadEndTime > 0. || Now < State->NextShotTime) return false;
	--State->Ammo;
	++State->ShotSequence;
	State->NextShotTime = Now + FMath::Max(0.05, Interval);
	return true;
}

bool UBattleState::StartReload(int32 ID, double Now)
{
	FBattleUnitState* State = Units.Find(ID);
	if (!State || !State->IsAlive() || State->ReloadEndTime > 0. || State->Reserve <= 0 || State->Ammo >= GetDefault<UBattleTuning>()->Capacity(State->Weapon)) return false;
	State->ReloadEndTime = Now + FMath::Max(0.1f, GetDefault<UBattleTuning>()->ReloadTime(State->Weapon));
	State->AimStableSince = -1.;
	return true;
}

void UBattleState::UpdateAim(int32 ID, double Now, float Speed, FVector Direction, bool bGrounded)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !Unit->IsAlive()) return;
	const auto* Tuning = GetDefault<UBattleTuning>();
	Unit->bMovingShot = !bGrounded || Speed > FMath::Max(0.f, Tuning->AimStationarySpeed);
	if (Unit->bMovingShot || Unit->ReloadEndTime > 0. || Direction.ContainsNaN() || Direction.IsNearlyZero())
	{ Unit->AimStableSince = -1.; return; }
	Direction.Normalize();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Tuning->AimResetDegrees, .1f, 90.f)));
	if (Unit->AimStableSince < 0. || FVector::DotProduct(Unit->AimAnchor, Direction) < MinDot)
	{ Unit->AimStableSince = Now; Unit->AimAnchor = Direction; }
}

bool UBattleState::SetSpreadSkill(int32 ID, float Scale)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !FMath::IsFinite(Scale)) return false;
	Unit->SpreadSkillScale = FMath::Clamp(Scale, .25f, 3.f);
	return true;
}

void UBattleState::SetTacticalState(int32 ID, int32 CoverSlot, const FString& Reason)
{
	if (auto* Unit = Units.Find(ID)) { Unit->CoverSlot = CoverSlot; Unit->DecisionReason = Reason; }
}

void UBattleState::SetCoordination(int32 ID, EBattleRole Role, int32 TargetID, FVector Goal)
{
	auto* Unit = Units.Find(ID);
	if (!Unit) return;
	if (!Unit->IsAlive() || Unit->Action == EBattleAction::DirectControl) { Role = EBattleRole::Independent; TargetID = INDEX_NONE; }
	if (Unit->Role == Role && Unit->CoordinatedTarget == TargetID && Unit->TacticalDestination.Equals(Goal, 1.f)) return;
	if (auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->CoordinationChanges;
	Unit->Role = Role; Unit->CoordinatedTarget = TargetID; Unit->TacticalDestination = Goal; ++Unit->CoordinationRevision;
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
				const int32 Count = FMath::Min(State.Reserve, FMath::Max(0, GetDefault<UBattleTuning>()->Capacity(State.Weapon) - State.Ammo));
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
	if (!State->IsAlive())
	{
		State->Action = EBattleAction::Dead;
		if (State->bLeaderManaged) { State->DeathCause = TEXT("Combat"); OnUnitDied.Broadcast(ID); }
	}
	return true;
}

int32 UBattleState::CreateLeaderSquad(int32 Team, const TArray<int32>& Members, int32 LeaderID, bool bAllowControl)
{
 if (Members.Num() != 10 || !Members.Contains(LeaderID)) return INDEX_NONE;
 TSet<int32> Unique;
 for (int32 ID : Members) { const auto* U=Find(ID); if (!U || !U->IsAlive() || U->Team!=Team || U->SquadID!=INDEX_NONE || Unique.Contains(ID)) return INDEX_NONE; Unique.Add(ID); }
 FBattleSquadState S; S.SquadID=NextSquadID++; S.Team=Team; S.Members=Members; S.LeaderUnitID=LeaderID;
 S.CommandStatus=EBattleCommandStatus::Active; S.DisplayName=FString::Printf(TEXT("第%d小隊"),S.SquadID);
 for (int32 ID:Members) { auto& U=Units.FindChecked(ID); U.SquadID=S.SquadID; U.bLeaderManaged=true; U.bActorRepresentation=ID==LeaderID; U.bCanPlayerControl=ID==LeaderID && bAllowControl; }
 Squads.Add(S.SquadID,S); return S.SquadID;
}
bool UBattleState::CanSelectLeader(int32 ID,int32 PlayerTeam) const
{
 const auto* U=Find(ID); if (!U || !U->IsAlive() || !U->bCanPlayerControl || !U->bActorRepresentation || U->Team!=PlayerTeam) return false;
 const auto* S=FindSquad(U->SquadID); return S && S->LeaderUnitID==ID && S->CommandStatus==EBattleCommandStatus::Active;
}
bool UBattleState::DisableLeaderSquad(int32 ID)
{
 auto* S=Squads.Find(ID); if (!S || S->CommandStatus!=EBattleCommandStatus::Active) return false;
 S->CommandStatus=EBattleCommandStatus::Disabled; S->StopReason=TEXT("LeaderDeath"); ++S->CommandRevision;
 TArray<int32> Dead;
 for(int32 Member:S->Members) { auto& U=Units.FindChecked(Member); if(U.IsAlive()) { U.HP=0; U.Action=EBattleAction::Dead; U.DeathCause=TEXT("LeaderLoss"); Dead.Add(Member); } U.ReloadEndTime=0; }
 for(int32 Member:Dead) OnUnitDied.Broadcast(Member);
 return true;
}
