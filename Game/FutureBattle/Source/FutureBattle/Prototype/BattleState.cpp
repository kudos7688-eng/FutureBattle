#include "BattleState.h"
#include "BattleMetrics.h"
#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "FutureBattle.h"

namespace
{
float TrialSightRange(float DefaultRange)
{
 static const float Override=[]()
 {
  FString Raw;
  if(!FParse::Value(FCommandLine::Get(),TEXT("BattleSightRange="),Raw)) return -1.f;
  float Value=0;
  if(!LexTryParseString(Value,*Raw) || !FMath::IsFinite(Value) || Value<100.f || Value>16000.f)
   UE_LOG(LogFutureBattle,Fatal,TEXT("BattleSightRange must be finite, 100..16000 cm (veteran multiplier preserved)"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SIGHT_OVERRIDE base_cm=%.1f recruit_cm=%.1f regular_cm=%.1f veteran_cm=%.1f scope=new_training_profiles saved_records=preserved"),Value,Value*.75f,Value,Value*1.25f);
  return Value;
 }();
 return Override<0 ? DefaultRange:Override;
}
}

float UBattleTuning::EffectiveSightRange() const
{
 // Range profile (map E): base sight of a regular soldier; see BattleRanges.h.
 if(const auto& Profile=BattleRanges::Get();Profile.bActive)return Profile.Sight;
 return TrialSightRange(SightRange);
}

float UBattleTuning::EffectiveWeaponRange() const
{
 // Range profile (map E): the shared "weapon range" becomes the carbine's AI engagement distance; bullets reach further.
 if(const auto& Profile=BattleRanges::Get();Profile.bActive)return Profile.EngageCarbine;
 static const float Override=[]()
 {
  FString Raw;
  if(!FParse::Value(FCommandLine::Get(),TEXT("BattleWeaponRange="),Raw)) return -1.f;
  float Value=0;
  if(!LexTryParseString(Value,*Raw) || !FMath::IsFinite(Value) || Value<100.f || Value>10000.f)
   UE_LOG(LogFutureBattle,Fatal,TEXT("BattleWeaponRange must be finite, 100..10000 cm"));
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_WEAPON_OVERRIDE cm=%.1f scope=trial_all_weapons"),Value);
  return Value;
 }();
 return Override<0 ? WeaponRange:Override;
}

void UBattleState::RestoreRecords(const TArray<FBattleUnitState>& SavedUnits, const TArray<FBattleSquadState>& SavedSquads, bool PreservePlans)
{
	Units.Reset(); Squads.Reset(); NextID = NextSquadID = 1;
	for (const auto& Unit : SavedUnits) { Units.Add(Unit.UnitID, Unit); NextID = FMath::Max(NextID, Unit.UnitID + 1); }
	for (auto Squad : SavedSquads)
 {
  if(!PreservePlans) Squad.TacticalPlan = FBattleSquadTacticalPlan();
  // Old saves have no mission fields; rebuild only from their own stored orders.
  if (!Squad.bHasMission)
  {
   Squad.bHasMission = true; Squad.MissionOrder = Squad.Order; Squad.MissionDestination = Squad.Destination;
   for (int32 ID : Squad.Members) if (const auto* U = Find(ID)) Squad.MissionGoals.Add(ID, U->Destination);
  }
  Squads.Add(Squad.SquadID, Squad); NextSquadID = FMath::Max(NextSquadID, Squad.SquadID + 1);
 }
 GetWorld()->GetSubsystem<UBattleSquadTactics>()->Reset();
}

int32 UBattleState::AddReserve(int32 ID, int32 Rounds)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !Unit->IsAlive() || Rounds <= 0) return 0;
	const int32 Added = FMath::Min(Rounds, FMath::Max(0, GetDefault<UBattleTuning>()->ReserveFor(Unit->Weapon) - Unit->Reserve));
	Unit->Reserve += Added; return Added;
}

float UBattleState::Heal(int32 ID, float Amount)
{
 auto* Unit = Units.Find(ID);
 if (!Unit || !Unit->IsAlive() || !FMath::IsFinite(Amount) || Amount <= 0.f) return 0.f;
 const float Before = Unit->HP; Unit->HP = FMath::Min(MaxHealthOf(ID), Unit->HP + Amount);
 return FMath::Max(0.f, Unit->HP - Before);
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
	State.Reserve = FMath::Max(0, Tuning->ReserveFor(Weapon));
	Units.Add(State.UnitID, State);
	SetTrainingPreset(State.UnitID, EBattleTraining::Regular);
	return State.UnitID;
}

bool UBattleState::ReplaceDeadFollower(int32 OldID,int32 NewID,FVector Position)
{
 auto* Old=Units.Find(OldID);auto* New=Units.Find(NewID);auto* S=Old?Squads.Find(Old->SquadID):nullptr;
 if(!Old||!New||!S||Old->IsAlive()||Old->bRetired||Old->bActorRepresentation||S->CommandStatus!=EBattleCommandStatus::Active||
    !New->IsAlive()||New->SquadID!=INDEX_NONE||New->Team!=Old->Team||!S->Members.Contains(OldID))return false;
 S->Members[S->Members.Find(OldID)]=NewID;Old->bRetired=true;
 New->SquadID=S->SquadID;New->bLeaderManaged=true;New->bActorRepresentation=false;New->bCanPlayerControl=false;
 New->Order=S->Order;New->Destination=Position;New->OrderRevision=S->OrderRevision;
 S->MissionGoals.Remove(OldID);if(S->bHasMission)S->MissionGoals.Add(NewID,S->MissionDestination);
 ++S->OrderRevision;return true;
}

bool UBattleState::SetTrainingPreset(int32 ID, EBattleTraining Training)
{
	auto* Unit = Units.Find(ID);
	if (!Unit || !Unit->IsAlive() || static_cast<uint8>(Training) > static_cast<uint8>(EBattleTraining::Veteran)) return false;
	const auto* T = GetDefault<UBattleTuning>();
	const bool bRecruit = Training == EBattleTraining::Recruit, bVeteran = Training == EBattleTraining::Veteran;
	FBattlePerceptionProfile P;
	P.SightRange = T->EffectiveSightRange() * (bRecruit ? .75f : bVeteran ? 1.25f : 1.f);
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

bool UBattleState::SetSquadDelegated(int32 ID, bool bDelegated)
{
	auto* Squad = Squads.Find(ID);
	if (!Squad || Squad->Team != 0 || (Squad->CommandStatus != EBattleCommandStatus::Legacy &&
  !(Squad->CommandStatus == EBattleCommandStatus::Active && GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsWideField()))) return false;
	bool bAlive = false;
	for (int32 Member : Squad->Members) if (const auto* U = Find(Member); U && U->IsAlive()) bAlive = true;
	if (!bAlive) return false;
	Squad->bPlayerDelegated = bDelegated;
 if (!bDelegated) CancelSquadTacticalPlan(ID, TEXT("MANUAL COMMAND / KEEP EXECUTION"));
	return true;
}

bool UBattleState::SetSquadOrder(int32 ID, EBattleOrder Order, FVector Destination, const TMap<int32, FVector>& Goals, bool bMissionUpdate)
{
	auto* Squad = Squads.Find(ID);
	if (!Squad || Destination.ContainsNaN() || static_cast<uint8>(Order) > static_cast<uint8>(EBattleOrder::Defend)) return false;
	int32 Living = 0;
	for (int32 Member : Squad->Members) if (Units.FindChecked(Member).IsAlive())
	{
		const FVector* Goal = Goals.Find(Member);
		if (!Goal || Goal->ContainsNaN()) return false;
		if (Order == EBattleOrder::Defend && FVector::DistSquared2D(*Goal, Destination) > FMath::Square(Squad->DefenseRadius - 60.f)) return false;
		++Living;
	}
	if (!Living) return false;
 if (!Squad->bHasMission || Squad->MissionOrder != Order || !Squad->MissionDestination.Equals(Destination, .01f) || !bMissionUpdate)
  ++Squad->MissionRevision;
 Squad->bHasMission = true; Squad->MissionOrder = Order; Squad->MissionDestination = Destination; Squad->MissionGoals = Goals;
 if (bMissionUpdate && Squad->TacticalPlan.Action != EBattleSquadTactic::Mission)
 {
  GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("MissionDeferredByPlan"));
  return true;
 }
 CancelSquadTacticalPlan(ID, bMissionUpdate ? TEXT("FOLLOW MISSION / ST1") : TEXT("EXPLICIT ORDER"));
	Squad->Order = Order; Squad->Destination = Destination;
	++Squad->OrderRevision;
	for (int32 Member : Squad->Members) if (Units.FindChecked(Member).IsAlive()) SetOrder(Member, Order, Goals.FindChecked(Member));
	return true;
}

void UBattleState::CancelSquadTacticalPlan(int32 ID, const FString& Reason)
{
 if (auto* S = Squads.Find(ID))
 {
  if (S->TacticalPlan.Action != EBattleSquadTactic::Mission)
   GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("PlanCancelled"));
  const int32 Revision = S->TacticalPlan.Revision + (S->TacticalPlan.Action != EBattleSquadTactic::Mission);
  S->TacticalPlan = FBattleSquadTacticalPlan(); S->TacticalPlan.Revision = Revision; S->TacticalPlan.Reason = Reason;
 }
}

bool UBattleState::SetSquadTacticalPlan(int32 ID, EBattleSquadTactic Action, EBattleOrder Order, FVector Destination,
 const TMap<int32, FVector>& Goals, float Lifetime, const FString& Reason)
{
 auto* S = Squads.Find(ID);
 if (!GetWorld()->GetSubsystem<UBattleSquadTactics>()->IsEnabled() || !S ||
     (S->CommandStatus != EBattleCommandStatus::Legacy && !(S->CommandStatus == EBattleCommandStatus::Active && GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsWideField())) ||
     (S->Team == 0 && !S->bPlayerDelegated) || Action == EBattleSquadTactic::Mission ||
     static_cast<uint8>(Action) > static_cast<uint8>(EBattleSquadTactic::Recover) ||
     !FMath::IsFinite(Lifetime) || Lifetime <= 0 || Lifetime > 60 || Reason.Len() > 160) return false;
 const FBattleSquadState Before = *S;
 // Validate every living member before making any change, using the normal order path.
 if (!SetSquadOrder(ID, Order, Destination, Goals)) return false;
 S->bHasMission = Before.bHasMission; S->MissionOrder = Before.MissionOrder;
 S->MissionDestination = Before.MissionDestination; S->MissionGoals = Before.MissionGoals; S->MissionRevision = Before.MissionRevision;
 S->TacticalPlan.Action = Action; S->TacticalPlan.Destination = Destination; S->TacticalPlan.Remaining = Lifetime;
 S->TacticalPlan.Revision = Before.TacticalPlan.Revision + 1; S->TacticalPlan.Reason = Reason;
 return true;
}

void UBattleState::AdvanceSquadTacticalPlan(int32 ID, float DeltaSeconds)
{
 auto* S = Squads.Find(ID);
 if (!S || S->TacticalPlan.Action == EBattleSquadTactic::Mission || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
 bool bAlive = false;
 for (int32 Member : S->Members) if (const auto* U = Find(Member); U && U->IsAlive()) bAlive = true;
 if (!bAlive) { CancelSquadTacticalPlan(ID, TEXT("NO LIVING MEMBERS")); return; }
 S->TacticalPlan.Remaining = FMath::Max(0.f, S->TacticalPlan.Remaining - DeltaSeconds);
 if (S->TacticalPlan.Remaining > 0) return;
 CancelSquadTacticalPlan(ID, TEXT("PLAN EXPIRED"));
 const auto Goals = S->MissionGoals;
 if (S->bHasMission && !SetSquadOrder(ID, S->MissionOrder, S->MissionDestination, Goals, true))
  S->TacticalPlan.Reason = TEXT("MISSION RESUME FAILED / KEEP EXECUTION");
}

void UBattleState::SetAction(int32 ID, EBattleAction Action)
{
	if (auto* Unit = Units.Find(ID))
 {
  if (Unit->IsAlive() && Unit->Action != Action && Action == EBattleAction::MoveFailed)
   GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("MoveExecutionFailed"));
  Unit->Action = Unit->IsAlive() ? Action : EBattleAction::Dead;
 }
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
 if(BattleTactics::CoverActionsEnabled() && !Direction.ContainsNaN() && !Direction.IsNearlyZero())
 {
  const FVector Aim=Direction.GetSafeNormal();
  if(Unit->bWeaponAimInitialized)
  {
   const float Angle=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Unit->WeaponAimDirection,Aim),-1.,1.)));
   const auto* AimTuning=GetDefault<UBattleTuning>();
   if(Angle>=AimTuning->AimSwitchMinDegrees){Unit->NextShotTime=FMath::Max(Unit->NextShotTime,Now+AimTuning->AimSwitchDelay(Angle));Unit->WeaponAimDirection=Aim;}
  }
  if(!Unit->bWeaponAimInitialized)Unit->WeaponAimDirection=Aim;Unit->bWeaponAimInitialized=true;
 }
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
  GetWorld()->GetSubsystem<UBattleSquadTactics>()->RecordDamage(State->SquadID, FMath::Min(State->HP, Damage));
 if(auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>())Runtime->NoteUnderFire(ID);
	State->HP = FMath::Max(0.f, State->HP - Damage);
	if (!State->IsAlive())
	{
		State->Action = EBattleAction::Dead;
  if (const auto* Squad = FindSquad(State->SquadID))
  {
   bool bSurvivor = false;
   for (int32 Member : Squad->Members) if (const auto* U = Find(Member); U && U->IsAlive()) bSurvivor = true;
   if (!bSurvivor) CancelSquadTacticalPlan(Squad->SquadID, TEXT("NO LIVING MEMBERS"));
  }
		if (State->bLeaderManaged) { State->DeathCause = TEXT("Combat"); OnUnitDied.Broadcast(ID); }
	}
	return true;
}

int32 UBattleState::CreateLeaderSquad(int32 Team, const TArray<int32>& Members, int32 LeaderID, bool bAllowControl)
{
 // Rifle squads have 10 members; heavy infantry has fewer (BattleSquadType::Members). The caller sets the type afterwards.
 if (Members.Num() < 2 || Members.Num() > 10 || !Members.Contains(LeaderID)) return INDEX_NONE;
 TSet<int32> Unique;
 for (int32 ID : Members) { const auto* U=Find(ID); if (!U || !U->IsAlive() || U->Team!=Team || U->SquadID!=INDEX_NONE || Unique.Contains(ID)) return INDEX_NONE; Unique.Add(ID); }
 FBattleSquadState S; S.SquadID=NextSquadID++; S.Team=Team; S.Members=Members; S.LeaderUnitID=LeaderID;
 S.CommandStatus=EBattleCommandStatus::Active; S.DisplayName=FString::Printf(TEXT("第%d小隊"),S.SquadID);
 for (int32 ID:Members) { auto& U=Units.FindChecked(ID); U.SquadID=S.SquadID; U.bLeaderManaged=true; U.bActorRepresentation=ID==LeaderID; U.bCanPlayerControl=ID==LeaderID && bAllowControl; }
 // Trial -BattleLeaderHealth: only a leader at the normal starting HP is scaled (fixtures with custom HP keep theirs).
 if (auto& Leader=Units.FindChecked(LeaderID); BattleTactics::LeaderHealthMultiplier()>1.f && Leader.HP==GetDefault<UBattleTuning>()->Health)
  Leader.HP=GetDefault<UBattleTuning>()->Health*BattleTactics::LeaderHealthMultiplier();
 Squads.Add(S.SquadID,S); return S.SquadID;
}
float UBattleState::MaxHealthOf(int32 UnitID) const
{
 const float Base=GetDefault<UBattleTuning>()->Health;
 const auto* U=Find(UnitID); const auto* S=U?FindSquad(U->SquadID):nullptr;
 // Squad type scale (heavy infantry); a leader keeps the larger of that and the leader multiplier.
 const float TypeScale=S?BattleSquadType::HealthScale(S->SquadType):1.f;
 return S && S->LeaderUnitID==UnitID ? Base*FMath::Max(TypeScale,BattleTactics::LeaderHealthMultiplier()) : Base*TypeScale;
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
 S->bPlayerDelegated=false; CancelSquadTacticalPlan(ID,TEXT("LEADER LOST"));
 TArray<int32> Dead;
 for(int32 Member:S->Members) { auto& U=Units.FindChecked(Member); if(U.IsAlive()) { U.HP=0; U.Action=EBattleAction::Dead; U.DeathCause=TEXT("LeaderLoss"); Dead.Add(Member); } U.ReloadEndTime=0; }
 for(int32 Member:Dead) OnUnitDied.Broadcast(Member);
 return true;
}
bool UBattleState::HandOverLeader(int32 ID,int32 NewLeaderID)
{
 auto* S=Squads.Find(ID); auto* U=Units.Find(NewLeaderID);
 if(!S||S->CommandStatus!=EBattleCommandStatus::Active||!U||!U->IsAlive()||!S->Members.Contains(NewLeaderID)||NewLeaderID==S->LeaderUnitID) return false;
 if(auto* Old=Units.Find(S->LeaderUnitID)) Old->bCanPlayerControl=false;
 S->LeaderUnitID=NewLeaderID; ++S->CommandRevision;
 U->bActorRepresentation=true; U->bCanPlayerControl=U->Team==0;
 return true;
}

// ---- Squad types.
// Heavy infantry values come from config and can be overridden per run by trial switches (-BattleHeavyMembers=, -BattleHeavyHealth=,
// -BattleHeavySpeed=, -BattleHeavyCost=, -BattleHeavyDamage=), so number screens never edit Config (the frozen snapshot hashes it).
namespace
{
 struct FHeavyValues { int32 Members; float Health,Speed,Damage; int32 Cost; };
 const FHeavyValues& HeavyValues()
 {
  static const FHeavyValues V=[]{
   const UBattleTuning* T=GetDefault<UBattleTuning>();
   FHeavyValues H{T->HeavyMembers,T->HeavyHealthScale,T->HeavySpeedScale,T->HeavyDamageScale,T->AreaHeavyCost};
   const TCHAR* C=FCommandLine::Get();
   FParse::Value(C,TEXT("BattleHeavyMembers="),H.Members);FParse::Value(C,TEXT("BattleHeavyHealth="),H.Health);
   FParse::Value(C,TEXT("BattleHeavySpeed="),H.Speed);FParse::Value(C,TEXT("BattleHeavyDamage="),H.Damage);FParse::Value(C,TEXT("BattleHeavyCost="),H.Cost);
   H.Members=FMath::Clamp(H.Members,2,10);H.Cost=FMath::Clamp(H.Cost,1,100);
   H.Health=FMath::IsFinite(H.Health)?FMath::Clamp(H.Health,1.f,10.f):3.f;
   H.Speed=FMath::IsFinite(H.Speed)?FMath::Clamp(H.Speed,.2f,1.f):.714f;
   H.Damage=FMath::IsFinite(H.Damage)?FMath::Clamp(H.Damage,.1f,10.f):1.f;
   return H;}();
  return V;
 }
}
int32 BattleSquadType::Members(uint8 Type){return Type==Heavy?HeavyValues().Members:Type==Scout?2:Type==AntiTank?4:10;}
bool BattleSquadType::Replenished(uint8 Type){return Type!=Scout;}
bool BattleSquadType::LeaderHandover(uint8 Type){return Type==Scout;}
float BattleSquadType::StillSightRange(uint8 Type)
{
 // Scouts see far only while still; the range profile (map E) uses the longer value. 0 = no still-sight bonus.
 if(Type!=Scout) return 0.f;
 const auto* T=GetDefault<UBattleTuning>();
 return FMath::Clamp(BattleRanges::Get().bActive?T->ScoutSightRangeProfile:T->ScoutSightRange,0.f,60000.f);
}
float BattleSquadType::HealthScale(uint8 Type){return Type==Heavy?HeavyValues().Health:1.f;}
float BattleSquadType::SpeedScale(uint8 Type){return Type==Heavy?HeavyValues().Speed:1.f;}
float BattleSquadType::DamageScale(uint8 Type){return Type==Heavy?HeavyValues().Damage:1.f;}
int32 BattleSquadType::AreaCost(uint8 Type){if(Type==AntiTank)return FMath::Clamp(GetDefault<UBattleTuning>()->AreaATCost,1,100);if(Type==Scout)return FMath::Clamp(GetDefault<UBattleTuning>()->AreaScoutCost,1,100);return Type==Heavy?HeavyValues().Cost:FMath::Clamp(GetDefault<UBattleTuning>()->AreaSquadCost,1,100);}
EBattleWeapon BattleSquadType::Weapon(uint8 Type,int32 Slot){if(Type==AntiTank)return Slot==1||Slot==2?EBattleWeapon::AntiTank:EBattleWeapon::Carbine;return Type==Heavy?EBattleWeapon::SupportGun:WeaponForSlot(Slot);}
void UBattleState::SetSquadType(int32 SquadID,uint8 Type)
{
 // Purchased squads only: name, and every member at the type's full health (the leader keeps the larger of the two scales).
 auto* S=Squads.Find(SquadID); if(!S||Type>=BattleSquadType::Count) return;
 S->SquadType=Type; if(Type==BattleSquadType::Heavy) S->DisplayName=FString::Printf(TEXT("第%d重装歩兵隊"),SquadID);
 if(Type==BattleSquadType::Scout) S->DisplayName=FString::Printf(TEXT("第%d偵察組"),SquadID);
 if(Type==BattleSquadType::AntiTank) S->DisplayName=FString::Printf(TEXT("第%d対戦車班"),SquadID);
 for(int32 ID:S->Members) if(auto* U=Units.Find(ID); U&&U->IsAlive()) U->HP=MaxHealthOf(ID);
}
