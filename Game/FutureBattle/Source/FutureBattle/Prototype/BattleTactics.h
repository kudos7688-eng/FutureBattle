#pragma once
#include "CoreMinimal.h"
struct FBattleUnitState;
struct FBattleSquadState;
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
 bool ExoShieldSuppressionEnabled();
 float ExoShieldHitDelay();
 bool GroupStrengthEnabled();
 int32 ArtilleryTrialFlags();
 bool ArtilleryObservedEnabled();
 bool SPGAmmoEnabled();
 // Trial -BattleVehicleOrderHere=1 (default 0): a vehicle order to where the vehicle already stands is accepted as arrived (no route needed).
 bool VehicleOrderHere();
 bool ArtilleryPosts(); // Trial -BattleArtilleryPosts=1 (default 0): public mortar posts and raids.
 bool ArtilleryRaidChase(); // Trial -BattleArtilleryRaidChase=1 (default 0), requires ArtilleryPosts.
 int32 AreaSPGCost(); // Optional -BattleAreaSPGCost override.
 bool ArtilleryFlash(); // Trial -BattleArtilleryFlash=1 (default 0): heard indirect fire reveals a rough position.
 float SPGRange(); // Optional -BattleSPGRange override; absent keeps the configured range.
 bool TankGunFlash(); // Trial -BattleTankGunFlash=1 (default 0): observed main-gun shots reveal a rough position.
 bool ObservedTactics(); // Trial -BattleObservedTactics=1 (default 0): tactical decisions use observations.
 bool ObservedArmour(); // Trial -BattleObservedArmour=1 (default 0): classify enemies from observations.
 bool VehicleHitCover(); // Trial -BattleVehicleHitCover=1 (default 0): hide from own hits before starting home repair.
 bool SaveReloadBoundary(); // Trial -BattleSaveReloadBoundary=1 (default 0): finish due reloads on the save copy.
 int32 OverrideSaveReloadBoundaryForSmoke(int32 Value); // Smoke only; -1 restores command-line policy.
 bool SaveRoutes(); // Trial -BattleSaveRoutes=1 (default 0): persist player route orders and lane choices.
 int32 OverrideSaveRoutesForSmoke(int32 Value); // -1 restores command-line policy; returns previous override. Smoke only.
 bool BlastOcclusionEnabled();
 bool ArtilleryClustersEnabled();
 bool WeaponOrderGateEnabled(); // trial, default 0: apply mission permissions to launchers and artillery
 bool ArmourSectorGateEnabled(); // trial, default 0: armour priority within Think's accepted candidate set
 bool OrderAllowsTarget(const FBattleSquadState& Squad,int32 TargetID,const FVector& KnownPosition);
 bool CoverActionsEnabled();
 // Trial switch -BattleAICoverProbe=1: per-frame AI cover probe budget and wider AI search instead of one shared probe per interval.
 bool AICoverProbeBudgetEnabled();
 // Trial switch -BattleCoverApproach=1: an engaged AI soldier with no cover nearby walks to a hidden cover slot within a wider radius.
 bool CoverApproachEnabled(const UWorld* World);
 // Trial switch -BattleContactCover=1 (needs -BattleCoverActions=1): on contact an AI soldier looks for a firing position behind cover
 // more often (per-frame probe budget, 2 s retry) and further away (20 m), walks only to a slot he could fire from, and keeps walking
 // while his own sighting of the threat is fresh even if the squad's target flickers. Implies the probe budget and the approach.
 bool ContactCoverEnabled();
 // The same switch for one soldier's team (-BattleContactCoverTeam=0/1 limits the trial to that team; default both).
 // With it, a soldier who finds no cover slot also looks at the ground around him (12 m) for a place he can fire from with less of
 // his body in the threat's view (UBattleLeaderRuntime::StartFirePosition), and moves there.
 bool ContactCover(int32 Team);
 // AI cover firing window (UBattleTuning::bAICoverFire, default ON; -BattleCoverFire=0/1 overrides).
 bool CoverFireEnabled();
 // AI cover range check and peek line revalidation (UBattleTuning::bAICoverValidation, default ON; -BattleCoverValidation=0/1 overrides).
 bool CoverValidationEnabled();
 // Blind peek exit (UBattleTuning::bAICoverPeekSight, default ON since 2026-11-14; -BattleCoverPeekSight=0/1): end an AI peek early without an own sighting.
 bool CoverPeekSightEnabled();
 // Trial switch -BattleReturnFire=1 (default OFF): AI soldiers hit in the last 5 s fire back at visible contacts up to bullet reach.
 bool ReturnFireEnabled();
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
 struct FSupplyTuning { int32 ReturnLiving=8, ResumeLiving=10; float AwayRatio=1.f, HealPerSecond=0.f, ResumeHealthRatio=0.f; bool bPointAmmo=false; float PointRadius=2500.f; };
 const FSupplyTuning& Supply();
 // Area new-squad hiring: UBattleTuning::bAreaHire/AreaSquadCost/AreaHireReserve; -BattleAreaHire=0/1 overrides.
 // MaxActive = UBattleTuning::AreaMaxActiveSquads (default 30 since 2026-11-20, was 25), -BattleAreaMaxSquads=N (1..100) overrides; the save check uses it.
 // AI pacing: Interval between a team's AI purchases and Stop = no AI purchase in the last N seconds
 // (-BattleAreaHireReserve= / -BattleAreaHireInterval= / -BattleAreaHireStop= override; not part of the save check).
 // P1 trial -BattleAreaHirePurpose=1 (default OFF; -BattleAreaHirePurposeFront=N, default 10): the AI buys only for an observed need
 // (own home hit in the last 30 s or enemies seen within 150 m of it, a point with fewer than N own soldiers within 100 m, or the
 // enemy home seen at or below 500 HP) and gives the new squad that mission; otherwise it keeps its resources.
 struct FAreaHireTuning { bool bEnabled=true; int32 Cost=15, Reserve=10, MaxActive=25; float Interval=0.f, Stop=0.f; bool bPurpose=false; int32 PurposeFront=10; };
 const FAreaHireTuning& AreaHire();
 // Area income: UBattleTuning::bAreaIncome/AreaIncomeSeconds/AreaBaseIncome/AreaPointIncome; -BattleAreaIncome=0/1 overrides.
 // Trials (not part of the save check): -BattleAreaIncomeBase= / -BattleAreaIncomePerPoint= override the amounts; -BattleAreaIncomeRamp=r
  // multiplies each payment by 1+r*elapsed minutes (rounded); -BattleAreaStartCredits= replaces the 100 starting resources.
 // Map E: UBattleTuning::AreaBaseIncomeUrban / AreaPointIncomeUrban (-1 = same as A-D), -BattleAreaIncomeUrbanBase= / -BattleAreaIncomeUrbanPerPoint=.
 struct FAreaIncomeTuning { bool bEnabled=true; float Seconds=20.f; int32 Base=1, PerPoint=1; int32 UrbanBase=-1, UrbanPerPoint=-1; float Ramp=0.f; int32 StartCredits=100;
  // Amounts for the current map: map E uses UrbanBase/UrbanPerPoint when set (>= 0).
  int32 BaseFor(bool bUrban) const {return bUrban&&UrbanBase>=0?UrbanBase:Base;}
  int32 PerPointFor(bool bUrban) const {return bUrban&&UrbanPerPoint>=0?UrbanPerPoint:PerPoint;} };
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
 // S7 (-BattleAreaBaseFlank=<f>; FS13BR5 = flank 1.3, one side, return at 5 living adopted as the default 2026-11-17, -BattleAreaBaseFlank=0 -BattleAreaBaseFlankSide=0 -BattleAreaBaseReturnLiving=0 restore the old straight assault): a home-assault squad heads first for a flank waypoint at x=0, y=+/-f times the
 // side points' |y| (side alternates by squad), then for the enemy home once its leader is past 45% of the home-to-home line.
 // S8 trials: -BattleAreaBaseFlankSide=1 puts every S7 waypoint on the +y side (concentrate the assault on one route);
 // -BattleAreaBaseReturnLiving=N (1..9, default 5; 0 = use SupplyReturnLiving) is the living count at which an S1 home-assault squad returns for supply.
 // Strategy layer stage 2 (ROADMAP 2026-11-16) -BattleAreaPlan=N (default 0 = no plan, the fixed intent): 1 = EH expand then hold,
 // 2 = ESA expand, save, then mass on the enemy home. Phases switch by match time only; see UBattleMatch::AreaIntent.
 // Heavy infantry share of AI purchases: -BattleAreaHeavyShare=f overrides UBattleTuning::AreaHeavyShare (default 0.33 since 2026-11-16) for the trial team(s); -1 = not passed.
 struct FAreaStrategyTuning { bool bEnabled=true; float AttackWeight=2.f, BaseShare=0.2f; bool bBaseRule=false; float BaseShareLow=0.f, BaseShareHigh=0.3f, BaseLowHP=500.f; bool bFrontBalance=false; float FrontGain=1.f; float InfiltrateRadius=0.f, FocusGain=0.f, FlankOffset=1.3f; bool bFlankSameSide=true; int32 BaseReturnLiving=5; int32 TrialTeam=-1; int32 Plan=0; float HeavyShare=-1.f; int32 ScoutKeep=-1; int32 ATKeep=-1; int32 TankKeep=-1; int32 ACKeep=-1; int32 MortarKeep=-1; int32 SPGKeep=-1; int32 ExoKeep=-1; int32 SupplyKeep=-1; };
 const FAreaStrategyTuning& AreaStrategy();
 // Armour AI, stage 3 (BattleArmourAI.cpp): -BattleArmourAI=0/1 (UBattleTuning::bArmourAI, default 1), -BattleArmourAITeam=0/1 for one team only.
 bool ArmourAI(int32 Team);
 // Strategic layer trial -BattleAreaComposition=0/1 (UBattleTuning::bAreaComposition, default 0; the trial team with -BattleAreaTrialTeam).
 bool AreaComposition(int32 Team);
 // Squad groups -BattleAreaGroups=0/1 (UBattleTuning::bAreaGroups, default 1 since 2026-11-20 by user decision; the trial team with -BattleAreaTrialTeam): the AI forms one attack group per team (BattleGroups.cpp).
 bool AreaGroups(int32 Team);
 // -BattleSymmetricStart=0/1 (default on for the maps with the urban rule set, E to I, since 2026-11-20 by user decision; off on
 // A/D unless UBattleTuning::bSymmetricStart): a point-symmetric start for both teams. The maps and
 // objectives are 180-degree symmetric, but (1) the east side's squads started in the same north-south rows as the west side's (now
 // rotated), (2) the allocation slot came from the squad ID, so team 1 (IDs 26-50) got another pattern than team 0 (now the squad's
 // number within its own team), (3) both teams' home-assault flank route went north (now the east side goes south).
 bool SymmetricStart(const class UWorld* World);
 // The number that picks a squad's allocation slot: the squad ID, or with the symmetric start its number within its own team.
 int32 AllocationIndex(const class UWorld* World,int32 SquadID,int32 Team);
 // Trial -BattleDefendEngage=0/1 (UBattleTuning::bDefendEngage, default 0; the trial team with -BattleAreaTrialTeam): defending squads target enemies within weapon range, not only within DefenseRadius of the defended place.
 bool DefendEngage(int32 Team);
	// SpreadScale: extra multiplier from runtime effects (suppression trial); 1 = none.
	float SpreadDegrees(const FBattleUnitState& State, double Now, float SpreadScale = 1.f);
	FVector ShotDirection(const FBattleUnitState& State, FVector Aim, double Now, float SpreadScale = 1.f);
	FBattleExposure ExposureFrom(const ABattleUnit* Target, FVector AttackOrigin);
	FBattleExposure ExposureAt(UWorld* World, FVector Position, FVector Forward, FVector AttackOrigin);
	bool HasTerrainLine(UWorld* World, FVector From, FVector To);
}
