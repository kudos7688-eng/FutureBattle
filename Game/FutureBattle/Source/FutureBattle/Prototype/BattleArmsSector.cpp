// Infantry arms and the sector tasks (2026-11-24, user requests; Docs/design-infantry-arms.md). Three trial switches, default 0:
//  -BattleArmsSectorTask=1  assault bounds only for squads on an attack task (AttackMove); defending squads charge only enemies inside
//                           the place they defend (BattleArms.cpp StepArms).
//  -BattleArmsRushWatch=1   defending squads (Defend / Hold) notice an enemy they see closing fast on the place they defend and answer:
//                           fire at him first (ArmsRushTarget), a grenade where he runs to (ArmsRushGrenade), fall back to cover inside
//                           their place (StartArmsRetreat) or meet a lone one with a charge (StepArmsRush).
//  -BattleArmsCoverFire=1   a soldier who starts an assault bound calls for covering fire: his squad and the squads within 60 m fire
//                           at the enemy position he knows (StepArmsCoverCalls, the fire in the soldier step); those shots suppress
//                           like -BattleSuppression even when that trial is off.
// All decisions use what the soldiers see (their contacts), never the state of the enemy. Runtime only (cleared on restore).
#include "BattleLeaderRuntime.h"
#include "BattleArms.h"
#include "BattleCover.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleSectors.h"
#include "BattleStance.h"
#include "BattleTactics.h"
#include "BattleState.h"
#include "FutureBattle.h"

bool UBattleLeaderRuntime::ArmsDefending(const FBattleSquadState& S) const
{
	return S.Order == EBattleOrder::Defend || S.Order == EBattleOrder::Hold;
}

bool UBattleLeaderRuntime::ArmsInDefendedArea(const FBattleSquadState& S, const FVector& At) const
{
	if (FVector::Dist2D(At, S.Destination) <= FMath::Max(double(S.DefenseRadius), 2500.)) return true;
	return S.bSectorMission && BattleSectors::Cell(At) == BattleSectors::Cell(S.Destination);
}

int32 UBattleLeaderRuntime::ArmsRushTarget(int32 ID, int32 Current, double Now)
{
	const auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto* U = Store->Find(ID);
	if (!U) return Current;
	const auto* Rushers = ArmsRushers.Find(U->SquadID);
	if (!Rushers || IsPinned(ID, Now)) return Current;
	const EBattleWeapon W = U->ActiveWeapon();
	if (W == EBattleWeapon::Mortar || W == EBattleWeapon::AntiTank || W == EBattleWeapon::GrenadeLauncher) return Current;
	const auto* Contacts = GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(ID);
	if (!Contacts) return Current;
	const FVector Pos = Position(ID);
	int32 Best = INDEX_NONE;
	double BestD = BattleRanges::Engage(W);
	for (const auto& R : *Rushers)
	{
		if (Now - R.Value.Seen > 1.) continue;
		const auto* C = Contacts->Find(R.Key);
		if (!C || !C->bVisible || C->bShared) continue;
		const double D = FVector::Dist2D(Pos, C->LastKnownPosition);
		if (D < BestD || (D == BestD && R.Key < Best)) { Best = R.Key; BestD = D; }
	}
	if (Best == INDEX_NONE) return Current;
	SightExperiment.Cover(Best == Current ? TEXT("arms_rush_target_kept_frames") : TEXT("arms_rush_target_frames"));
	return Best;
}

bool UBattleLeaderRuntime::StartArmsRetreat(int32 ID, int32 Threat, const FVector& ThreatAt, double Now)
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto* U = Store->Find(ID);
	if (!U || !U->IsAlive() || !Bodies.Contains(ID) || IsVehicle(ID)) return false;
	const auto* S = Store->FindSquad(U->SquadID);
	if (!S) return false;
	auto* Cover = GetWorld()->GetSubsystem<UBattleCover>();
	Cover->BuildSlots();
	const FVector Pos = Position(ID), Eye = ThreatAt + FVector(0, 0, 60);
	const double Here = FVector::Dist2D(Pos, ThreatAt);
	// A cover slot inside the defended place, at most 20 m away, at least 8 m farther from the threat than he is now, hidden from
	// the threat, with a straight walkable line; the shortest walk.
	int32 BestSlot = INDEX_NONE;
	double BestWalk = DBL_MAX;
	FVector BestAnchor = FVector::ZeroVector;
	for (int32 Slot : Cover->NearbySlots(Pos, 2000.f, 64))
	{
		const FVector Anchor = Cover->GetSlots()[Slot] + FVector(0, 0, 90);
		const double Walk = FVector::Dist2D(Pos, Anchor);
		if (Walk > 2000. || Walk >= BestWalk || FVector::Dist2D(Anchor, ThreatAt) < Here + 800. || !ArmsInDefendedArea(*S, Anchor)) continue;
		if (!Cover->IsAvailable(Slot, ID, Now)) continue;
		bool bTaken = false;
		for (const auto& Other : CoverApproaches) if (Other.Key != ID && Other.Value.Slot == Slot) { bTaken = true; break; }
		if (bTaken) continue;
		if (BattleTactics::ExposureAt(GetWorld(), Anchor, (ThreatAt - Anchor).GetSafeNormal2D(), Eye).Fraction() > CoverExposureLimit()) continue;
		if (GetWorld()->SweepTestByObjectType(Pos, Anchor, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeCapsule(36, 88))) continue;
		BestSlot = Slot; BestWalk = Walk; BestAnchor = Anchor;
	}
	if (BestSlot == INDEX_NONE) { SightExperiment.Cover(TEXT("arms_rush_retreat_no_cover")); return false; }
	if (U->CoverPhase) CancelCoverAction(ID);
	U = Store->Find(ID);
	FCoverApproach A;
	A.Goal = BestAnchor; A.Slot = BestSlot; A.Target = Threat; A.Revision = U->OrderRevision; A.Until = Now + 8.; A.bAssault = true; A.bRetreat = true;
	CoverApproaches.Add(ID, A);
	FirePositions.Remove(ID);
	SightExperiment.Cover(TEXT("arms_rush_retreats"));
	return true;
}

void UBattleLeaderRuntime::StepArmsRush(double Now)
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Sense = GetWorld()->GetSubsystem<UBattlePerception>();
	if (!Sense) return;
	for (auto It = ArmsRushers.CreateIterator(); It; ++It)
	{
		for (auto R = It.Value().CreateIterator(); R; ++R) if (Now - R.Value().Seen > 3.) R.RemoveCurrent();
		if (It.Value().IsEmpty()) It.RemoveCurrent();
	}
	for (auto It = ArmsRushSamples.CreateIterator(); It; ++It) if (Now - It.Value().Time > 3.) It.RemoveCurrent();
	TArray<int32> SIDs;
	for (const auto& P : Store->GetSquads())
		if (P.Value.CommandStatus == EBattleCommandStatus::Active && ArmsDefending(P.Value) && !BattleSquadType::IsVehicle(P.Value.SquadType) && BattleArms::ForTeam(P.Value.Team)) SIDs.Add(P.Key);
	SIDs.Sort();
	const auto& Ch = BattleArms::Charge();
	for (int32 SID : SIDs)
	{
		const auto* S = Store->FindSquad(SID);
		// What the members see themselves within 50 m (not vehicles): position and how many of them see each enemy.
		TMap<int32, FVector> Seen;
		TMap<int32, int32> Watchers;
		for (int32 Member : S->Members)
		{
			const auto* M = Store->Find(Member);
			if (!M || !M->IsAlive() || !Bodies.Contains(Member) || IsVehicle(Member)) continue;
			const auto* Contacts = Sense->GetContacts(Member);
			if (!Contacts) continue;
			const FVector MP = Position(Member);
			for (const auto& C : *Contacts)
			{
				if (!C.Value.bVisible || C.Value.bShared || FVector::Dist2D(MP, C.Value.LastKnownPosition) > 5000.) continue;
				if (BattleTactics::ObservedArmour() ? BattleSquadType::IsVehicle(C.Value.ObservedSquadType) : IsVehicle(C.Key)) continue;
				if (!Seen.Contains(C.Key)) Seen.Add(C.Key, C.Value.LastKnownPosition);
				++Watchers.FindOrAdd(C.Key);
			}
		}
		TArray<int32> Enemies;
		Seen.GetKeys(Enemies);
		Enemies.Sort();
		for (int32 E : Enemies)
		{
			const FVector At = Seen[E];
			const uint64 Key = (uint64(uint32(SID)) << 32) | uint32(E);
			FArmsRushSample* Old = ArmsRushSamples.Find(Key);
			if (!Old) { ArmsRushSamples.Add(Key, { At, Now }); continue; }
			const double Dt = Now - Old->Time;
			if (Dt < .5) continue;
			if (Dt <= 1.5)
			{
				const double Closing = (FVector::Dist2D(Old->At, S->Destination) - FVector::Dist2D(At, S->Destination)) / Dt;
				if (Closing >= 300.)
				{
					auto& Rushers = ArmsRushers.FindOrAdd(SID);
					const bool bNew = !Rushers.Contains(E);
					auto& R = Rushers.FindOrAdd(E);
					R.At = At; R.Vel = (At - Old->At) / Dt; R.Vel.Z = 0; R.Seen = Now; R.Watchers = Watchers.FindRef(E);
					if (bNew) SightExperiment.Cover(TEXT("arms_rush_seen"));
				}
				else if (auto* Known = ArmsRushers.Find(SID)) if (auto* R = Known->Find(E)) { R->At = At; R->Watchers = Watchers.FindRef(E); }
			}
			*Old = { At, Now };
		}
		auto* Rushers = ArmsRushers.Find(SID);
		if (!Rushers) continue;
		TArray<int32> Keys;
		Rushers->GetKeys(Keys);
		Keys.Sort();
		// A lone rusher close by whom two or more of the squad see: the nearest member who sees him meets him with a charge.
		if (BattleArms::ChargeEnabled())
		{
			int32 Charging = 0;
			for (int32 Member : S->Members) if (const auto* M = Store->Find(Member); M && M->IsAlive() && ArmsCharging(Member)) ++Charging;
			for (int32 E : Keys)
			{
				if (Charging >= Ch.SquadMax) break;
				const auto& R = (*Rushers)[E];
				if (Now - R.Seen > .5 || R.Watchers < 2 || !ArmsInDefendedArea(*S, R.At)) continue;
				bool bLone = true;
				for (const auto& O : Seen) if (O.Key != E && FVector::DistSquared2D(O.Value, R.At) <= FMath::Square(double(Ch.CrowdRadius))) { bLone = false; break; }
				if (!bLone) continue;
				int32 Best = INDEX_NONE;
				double BestD = Ch.Range;
				for (int32 Member : S->Members)
				{
					const auto* M = Store->Find(Member);
					if (!M || !M->IsAlive() || Member == S->LeaderUnitID || !Bodies.Contains(Member) || IsVehicle(Member) || ArmsDashing(Member) ||
						M->Action == EBattleAction::DirectControl || Now < ArmsChargeNext.FindRef(Member)) continue;
					if (M->Weapon == EBattleWeapon::Mortar || M->Weapon == EBattleWeapon::AntiTank || IsArtillery(Member)) continue;
					const auto* C = Sense->FindContact(Member, E);
					if (!C || !C->bVisible || C->bShared) continue;
					const double D = FVector::Dist2D(Position(Member), C->LastKnownPosition);
					if (D > Ch.Stop && (D < BestD || (D == BestD && Member < Best))) { Best = Member; BestD = D; }
				}
				if (Best != INDEX_NONE && StartCharge(Best, E, R.At, Now))
				{
					++Charging;
					ArmsChargeNext.Add(Best, Now + Ch.Cooldown);
					SightExperiment.Cover(TEXT("arms_rush_counter_charges"));
				}
			}
		}
		// Fall back: two rushers within 15 m of him (one when he cannot fire: no rounds or reloading).
		for (int32 Member : S->Members)
		{
			const auto* M = Store->Find(Member);
			if (!M || !M->IsAlive() || !Bodies.Contains(Member) || IsVehicle(Member) || CoverApproaches.Contains(Member) ||
				M->Action == EBattleAction::DirectControl || Now < ArmsRetreatNext.FindRef(Member)) continue;
			const FVector MP = Position(Member);
			int32 Near = 0, Nearest = INDEX_NONE;
			double NearestD = 1500.;
			FVector NearestAt = FVector::ZeroVector;
			for (int32 E : Keys)
			{
				const auto& R = (*Rushers)[E];
				if (Now - R.Seen > 1.) continue;
				const double D = FVector::Dist2D(MP, R.At);
				if (D > 1500.) continue;
				++Near;
				if (D <= NearestD) { Nearest = E; NearestD = D; NearestAt = R.At; }
			}
			const bool bCannotFire = M->Ammo == 0 || M->ReloadEndTime > 0;
			if (Nearest == INDEX_NONE || Near < (bCannotFire ? 1 : 2)) continue;
			ArmsRetreatNext.Add(Member, Now + 6.);
			StartArmsRetreat(Member, Nearest, NearestAt, Now);
		}
	}
}

bool UBattleLeaderRuntime::ArmsRushGrenade(int32 ID, double Now)
{
	const auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	const auto* U = Store->Find(ID);
	if (!U) return false;
	const auto* Rushers = ArmsRushers.Find(U->SquadID);
	if (!Rushers || IsPinned(ID, Now)) return false;
	const auto* Sense = GetWorld()->GetSubsystem<UBattlePerception>();
	const auto& G = BattleArms::Grenade();
	const FVector Pos = Position(ID);
	int32 Best = INDEX_NONE;
	double BestD = G.MaxRange;
	FVector BestAt = FVector::ZeroVector;
	TArray<int32> Keys;
	Rushers->GetKeys(Keys);
	Keys.Sort();
	for (int32 E : Keys)
	{
		const auto& R = (*Rushers)[E];
		if (Now - R.Seen > 1.) continue;
		const auto* C = Sense->FindContact(ID, E);
		if (!C || Now - C->LastSeenTime > 1.) continue;
		// Where he runs to in a second, kept within the throw.
		FVector At = R.At + R.Vel * 1.;
		double D = FVector::Dist2D(Pos, At);
		if (D < G.MinRange || D > G.MaxRange) { At = R.At; D = FVector::Dist2D(Pos, At); }
		if (D < G.MinRange || D > G.MaxRange || D >= BestD) continue;
		if (BattleTactics::WeaponOrderGateEnabled() && !WeaponOrderAllows(ID, E, At)) continue;
		Best = E; BestD = D; BestAt = At;
	}
	if (Best == INDEX_NONE || !ThrowGrenade(ID, BestAt, Now, false)) return false;
	SightExperiment.Cover(TEXT("arms_rush_grenades"));
	return true;
}

void UBattleLeaderRuntime::AddArmsCoverCall(int32 Bounder, int32 Target, const FVector& TargetAt, const FVector& From, double Seconds, double Now)
{
	const auto* U = GetWorld()->GetSubsystem<UBattleState>()->Find(Bounder);
	if (!U) return;
	FArmsCoverCall C;
	C.Team = U->Team; C.Target = Target; C.At = TargetAt; C.From = From; C.Until = Now + FMath::Clamp(Seconds + 1.5, 2., 10.);
	ArmsCoverCalls.Add(Bounder, C);
	SightExperiment.Cover(TEXT("arms_cover_fire_calls"));
}

void UBattleLeaderRuntime::StepArmsCoverCalls(double Now)
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* Sense = GetWorld()->GetSubsystem<UBattlePerception>();
	for (auto It = ArmsCoverCalls.CreateIterator(); It; ++It)
	{
		const auto* B = Store->Find(It.Key());
		if (!B || !B->IsAlive() || Now > It.Value().Until) { It.RemoveCurrent(); continue; }
		// The bounder's own sighting moves the point.
		if (Sense) if (const auto* C = Sense->FindContact(It.Key(), It.Value().Target); C && C->bVisible && !C->bShared) It.Value().At = C->LastKnownPosition;
	}
	// A supporter drops out when the call ends, or he is down, his squad is ordered to move (or out of command), or the point is out of
	// his weapon's reach after he moved. The line to the point is checked only when he is picked.
	for (auto It = ArmsCoverFire.CreateIterator(); It; ++It)
	{
		const FArmsCoverCall* Call = ArmsCoverCalls.Find(It.Value().Bounder);
		const auto* U = Store->Find(It.Key());
		const auto* S = U ? Store->FindSquad(U->SquadID) : nullptr;
		if (Now > It.Value().Until || !Call || !U || !U->IsAlive() || !S || S->CommandStatus != EBattleCommandStatus::Active || S->Order == EBattleOrder::Move
			|| !Bodies.Contains(It.Key()) || FVector::Dist2D(Position(It.Key()), Call->At) > BattleRanges::Engage(U->ActiveWeapon()))
			It.RemoveCurrent();
	}
	if (ArmsCoverCalls.IsEmpty()) return;
	TArray<int32> Bounders;
	ArmsCoverCalls.GetKeys(Bounders);
	Bounders.Sort();
	for (int32 Bounder : Bounders)
	{
		const FArmsCoverCall Call = ArmsCoverCalls[Bounder];
		int32 Have = 0;
		for (const auto& F : ArmsCoverFire) Have += F.Value.Bounder == Bounder;
		if (Have >= 6) continue;
		// Candidates within 60 m of where the bound started, nearest first.
		TArray<TPair<double, int32>> Cands;
		for (const auto& Body : Bodies)
		{
			const int32 ID = Body.Key;
			if (ID == Bounder || ArmsCoverFire.Contains(ID)) continue;
			const auto* U = Store->Find(ID);
			if (!U || !U->IsAlive() || U->Team != Call.Team || U->Action == EBattleAction::DirectControl || IsVehicle(ID) || IsArtillery(ID) || ArmsDashing(ID)) continue;
			const EBattleWeapon W = U->ActiveWeapon();
			if (W == EBattleWeapon::Mortar || W == EBattleWeapon::AntiTank || W == EBattleWeapon::GrenadeLauncher || U->Ammo + U->Reserve <= 0) continue;
			const auto* S = Store->FindSquad(U->SquadID);
			if (!S || S->CommandStatus != EBattleCommandStatus::Active || S->Order == EBattleOrder::Move) continue;
			const FVector Pos = Position(ID);
			const double D = FVector::Dist2D(Pos, Call.From);
			if (D > 6000. || FVector::Dist2D(Pos, Call.At) > BattleRanges::Engage(W)) continue;
			Cands.Add(TPair<double, int32>(D, ID));
		}
		Cands.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key || (A.Key == B.Key && A.Value < B.Value); });
		int32 Traces = 0;
		for (const auto& Cand : Cands)
		{
			if (Have >= 6 || Traces >= 12) break;
			const int32 ID = Cand.Value;
			const FVector Eye = Position(ID) + FVector(0, 0, BattleStance::EyeOffset(StanceOf(ID))), Aim = Call.At + FVector(0, 0, 60);
			++Traces;
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Eye, Aim, FCollisionObjectQueryParams(ECC_WorldStatic)) && FVector::Dist(Hit.ImpactPoint, Aim) > 300.) continue;
			ArmsCoverFire.Add(ID, { Bounder, Call.Until });
			++Have;
			SightExperiment.Cover(TEXT("arms_cover_fire_supporters"));
		}
	}
}

bool UBattleLeaderRuntime::ArmsCoverFireAt(int32 ID, double Now, FVector& At) const
{
	const auto* F = ArmsCoverFire.Find(ID);
	if (!F || Now > F->Until) return false;
	const auto* Call = ArmsCoverCalls.Find(F->Bounder);
	if (!Call) return false;
	// Around the point (1.5 m), a new spot four times a second.
	const uint32 H = HashCombine(GetTypeHash(ID), GetTypeHash(int32(Now * 4.)));
	const double Angle = FMath::DegreesToRadians(double(H % 360u)), R = double((H >> 9) % 150u);
	At = Call->At + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 60.);
	return true;
}

bool UBattleLeaderRuntime::ArmsCoverSuppressed(int32 ID) const
{
	return BattleArms::CoverFireEnabled() && SuppressionZeroAt.Contains(ID);
}
