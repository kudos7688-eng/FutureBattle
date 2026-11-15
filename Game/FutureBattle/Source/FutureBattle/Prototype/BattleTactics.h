#pragma once
#include "CoreMinimal.h"
struct FBattleUnitState;
class ABattleUnit;

struct FBattleExposure
{
	int32 VisibleSamples = 0;
	int32 TotalSamples = 5;
	bool bFlanked = false;
	float Fraction() const { return static_cast<float>(VisibleSamples) / TotalSamples; }
	bool HasCover() const { return VisibleSamples < TotalSamples; }
};

namespace BattleTactics
{
 bool CoverActionsEnabled();
 // Trial switch -BattleAICoverProbe=1: per-frame AI cover probe budget and wider AI search instead of one shared probe per interval.
 bool AICoverProbeBudgetEnabled();
 // Trial switch -BattleCoverApproach=1: an engaged AI soldier with no cover nearby walks to a hidden cover slot within a wider radius.
 bool CoverApproachEnabled();
 // AI cover firing window (UBattleTuning::bAICoverFire, default ON; -BattleCoverFire=0/1 overrides).
 bool CoverFireEnabled();
 // AI cover range check and peek line revalidation (UBattleTuning::bAICoverValidation, default ON; -BattleCoverValidation=0/1 overrides).
 bool CoverValidationEnabled();
 // Blind peek exit (UBattleTuning::bAICoverPeekSight, default ON since 2026-11-14; -BattleCoverPeekSight=0/1): end an AI peek early without an own sighting.
 bool CoverPeekSightEnabled();
 // Trial switch -BattleReacquireGrace=1 (condition B): re-seeing an own contact within ReacquireGraceSeconds skips the
 // detection delay, and a squad keeps its previous target for TargetHoldSeconds after a member's own sighting.
 bool ReacquireGraceEnabled();
 // Trial switch -BattleLeaderGuard=1: AI leaders take the rearmost arrival slot and look for cover first.
 // Leader death still disables the whole squad; this only changes AI positioning/cover cognition.
 bool LeaderGuardEnabled();
 // Squad leaders start with N times Health: UBattleTuning::LeaderHealthMultiplier (default 3), overridden by -BattleLeaderHealth=N (1..10).
 float LeaderHealthMultiplier();
 // Bits added to the mixed-save rule check so saves only load under the same leader multiplier.
 int32 LeaderHealthSaveBits();
 // Area home supply cycle: UBattleTuning::Supply* overridden by -BattleSupplyReturnLiving= / -BattleSupplyResumeLiving= /
 // -BattleSupplyAwayRatio= / -BattleSupplyHeal= / -BattleSupplyResumeHealth= (trials only; not part of the save check).
 struct FSupplyTuning { int32 ReturnLiving=8, ResumeLiving=10; float AwayRatio=1.f, HealPerSecond=0.f, ResumeHealthRatio=0.f; };
 const FSupplyTuning& Supply();
 // Area new-squad hiring: UBattleTuning::bAreaHire/AreaSquadCost/AreaHireReserve; -BattleAreaHire=0/1 overrides.
 // AI pacing: Interval between a team's AI purchases and Stop = no AI purchase in the last N seconds
 // (-BattleAreaHireReserve= / -BattleAreaHireInterval= / -BattleAreaHireStop= override; not part of the save check).
 // P1 trial -BattleAreaHirePurpose=1 (default OFF; -BattleAreaHirePurposeFront=N, default 10): the AI buys only for an observed need
 // (own home hit in the last 30 s or enemies seen within 150 m of it, a point with fewer than N own soldiers within 100 m, or the
 // enemy home seen at or below 500 HP) and gives the new squad that mission; otherwise it keeps its resources.
 struct FAreaHireTuning { bool bEnabled=true; int32 Cost=15, Reserve=10; float Interval=0.f, Stop=0.f; bool bPurpose=false; int32 PurposeFront=10; };
 const FAreaHireTuning& AreaHire();
 // Area income: UBattleTuning::bAreaIncome/AreaIncomeSeconds/AreaBaseIncome/AreaPointIncome; -BattleAreaIncome=0/1 overrides.
 // Trials (not part of the save check): -BattleAreaIncomeBase= / -BattleAreaIncomePerPoint= override the amounts; -BattleAreaIncomeRamp=r
 // multiplies each payment by 1+r*elapsed minutes (rounded); -BattleAreaStartCredits= replaces the 100 starting resources.
 struct FAreaIncomeTuning { bool bEnabled=true; float Seconds=20.f; int32 Base=1, PerPoint=1; float Ramp=0.f; int32 StartCredits=100; };
 const FAreaIncomeTuning& AreaIncome();
 // Area AI point allocation S1 (UBattleTuning::bAreaStrategy/AreaAttackWeight/AreaBaseShare, adopted ON 2026-11-14; -BattleAreaStrategy=0 restores the old round-robin): fixed per-squad
 // golden-ratio slot; points the team does not observe as its own weigh AttackWeight, owned ones 1; slots above 1-BaseShare assault the enemy home.
 // S3 trial (-BattleAreaBaseRule=1, default OFF): home-assault share is BaseShareHigh when the team observes owning 2+ points or last saw the
 // enemy home at or below BaseLowHP, otherwise BaseShareLow (-BattleAreaBaseShareHigh= / -BattleAreaBaseShareLow= / -BattleAreaBaseLowHP=).
 // S4 (UBattleTuning::bAreaFrontBalance/AreaFrontGain, adopted ON 2026-11-15; -BattleAreaFrontBalance=0/1, -BattleAreaFrontGain=): each point weight is multiplied by 1+FrontGain*deficit, where deficit is
 // how far the team's OWN living soldiers within 100m of the point fall below the per-point average (own positions only).
 // S5 trial (-BattleAreaBaseInfiltrate=<m>, default 0 = OFF): a home-assault squad moves with a Move order (no stopping to fight)
 // until its leader is within that many metres of the enemy home, then switches to AttackMove.
 // S6 (UBattleTuning::AreaFocusGain, adopted 1 on 2026-11-15; -BattleAreaFocus=<gain>, 0 = OFF): the point not observed as own that is nearest the team's own home gets its
 // weight multiplied by 1+gain (concentration of force; static positions and observed ownership only).
 // S7 trial (-BattleAreaBaseFlank=<f>, default 0 = OFF): a home-assault squad heads first for a flank waypoint at x=0, y=+/-f times the
 // side points' |y| (side alternates by squad), then for the enemy home once its leader is past 45% of the home-to-home line.
 // S8 trials: -BattleAreaBaseFlankSide=1 puts every S7 waypoint on the +y side (concentrate the assault on one route);
 // -BattleAreaBaseReturnLiving=N (1..9, default 0 = use SupplyReturnLiving) is the living count at which an S1 home-assault squad returns for supply.
 // Strategy layer stage 2 (ROADMAP 2026-11-16) -BattleAreaPlan=N (default 0 = no plan, the fixed intent): 1 = EH expand then hold,
 // 2 = ESA expand, save, then mass on the enemy home. Phases switch by match time only; see UBattleMatch::AreaIntent.
 struct FAreaStrategyTuning { bool bEnabled=true; float AttackWeight=2.f, BaseShare=0.2f; bool bBaseRule=false; float BaseShareLow=0.f, BaseShareHigh=0.3f, BaseLowHP=500.f; bool bFrontBalance=false; float FrontGain=1.f; float InfiltrateRadius=0.f, FocusGain=0.f, FlankOffset=0.f; bool bFlankSameSide=false; int32 BaseReturnLiving=0; int32 TrialTeam=-1; int32 Plan=0; };
 const FAreaStrategyTuning& AreaStrategy();
	float SpreadDegrees(const FBattleUnitState& State, double Now);
	FVector ShotDirection(const FBattleUnitState& State, FVector Aim, double Now);
	FBattleExposure ExposureFrom(const ABattleUnit* Target, FVector AttackOrigin);
	FBattleExposure ExposureAt(UWorld* World, FVector Position, FVector Forward, FVector AttackOrigin);
	bool HasTerrainLine(UWorld* World, FVector From, FVector To);
}
