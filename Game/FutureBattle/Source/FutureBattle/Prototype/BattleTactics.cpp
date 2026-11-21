#include "BattleTactics.h"
#include "BattleFieldLayout.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattleMetrics.h"

bool BattleTactics::AICoverProbeBudgetEnabled()
{
 static const bool Enabled=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAICoverProbe="),Value);return Value!=0;}();
 return Enabled;
}

bool BattleTactics::LeaderGuardEnabled()
{
 static const bool Enabled=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderGuard="),Value);return Value!=0;}();
 return Enabled;
}

float BattleTactics::LeaderHealthMultiplier()
{
 // Config default (UBattleTuning::LeaderHealthMultiplier); the command line overrides it for trials.
 static const float Multiplier=[](){float Value=GetDefault<UBattleTuning>()->LeaderHealthMultiplier;FParse::Value(FCommandLine::Get(),TEXT("BattleLeaderHealth="),Value);return FMath::IsFinite(Value)?FMath::Clamp(Value,1.f,10.f):1.f;}();
 return Multiplier;
}

const BattleTactics::FSupplyTuning& BattleTactics::Supply()
{
 static const FSupplyTuning Value=[]()
 {
  const auto* T=GetDefault<UBattleTuning>(); FSupplyTuning S;
  S.ReturnLiving=T->SupplyReturnLiving;S.ResumeLiving=T->SupplyResumeLiving;S.AwayRatio=T->SupplyAwayRatio;
  S.HealPerSecond=T->SupplyHealPerSecond;S.ResumeHealthRatio=T->SupplyResumeHealthRatio;
  const TCHAR* Line=FCommandLine::Get();
  FParse::Value(Line,TEXT("BattleSupplyReturnLiving="),S.ReturnLiving);FParse::Value(Line,TEXT("BattleSupplyResumeLiving="),S.ResumeLiving);
  FParse::Value(Line,TEXT("BattleSupplyAwayRatio="),S.AwayRatio);FParse::Value(Line,TEXT("BattleSupplyHeal="),S.HealPerSecond);
  FParse::Value(Line,TEXT("BattleSupplyResumeHealth="),S.ResumeHealthRatio);
  // Ammunition at owned points: UBattleTuning::bAreaPointSupply, -BattleAreaPointSupply=0/1 overrides (default OFF).
  int32 PointAmmo=T->bAreaPointSupply?1:0;FParse::Value(Line,TEXT("BattleAreaPointSupply="),PointAmmo);S.bPointAmmo=PointAmmo!=0;
  S.PointRadius=FMath::IsFinite(T->AreaPointSupplyRadius)?FMath::Clamp(T->AreaPointSupplyRadius,100.f,10000.f):2500.f;
  S.ReturnLiving=FMath::Clamp(S.ReturnLiving,1,9);S.ResumeLiving=FMath::Clamp(S.ResumeLiving,1,10);
  S.AwayRatio=FMath::IsFinite(S.AwayRatio)?FMath::Clamp(S.AwayRatio,.05f,1.f):1.f;
  S.HealPerSecond=FMath::IsFinite(S.HealPerSecond)?FMath::Clamp(S.HealPerSecond,0.f,1000.f):0.f;
  S.ResumeHealthRatio=FMath::IsFinite(S.ResumeHealthRatio)?FMath::Clamp(S.ResumeHealthRatio,0.f,1.f):0.f;
  return S;
 }();
 return Value;
}

const BattleTactics::FAreaHireTuning& BattleTactics::AreaHire()
{
 static const FAreaHireTuning Value=[]()
 {
  const auto* T=GetDefault<UBattleTuning>();FAreaHireTuning H;int32 Enabled=T->bAreaHire?1:0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHire="),Enabled);
  H.bEnabled=Enabled!=0;H.Cost=FMath::Clamp(T->AreaSquadCost,1,100);H.Reserve=T->AreaHireReserve;H.Interval=T->AreaHireIntervalSeconds;H.Stop=T->AreaHireStopSeconds;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHireReserve="),H.Reserve);FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHireInterval="),H.Interval);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHireStop="),H.Stop);
  H.MaxActive=T->AreaMaxActiveSquads;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaMaxSquads="),H.MaxActive);H.MaxActive=FMath::Clamp(H.MaxActive,1,100);
  int32 Purpose=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHirePurpose="),Purpose);H.bPurpose=Purpose!=0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHirePurposeFront="),H.PurposeFront);H.PurposeFront=FMath::Clamp(H.PurposeFront,1,100);
  H.Reserve=FMath::Clamp(H.Reserve,0,1000);H.Interval=FMath::IsFinite(H.Interval)?FMath::Clamp(H.Interval,0.f,600.f):0.f;H.Stop=FMath::IsFinite(H.Stop)?FMath::Clamp(H.Stop,0.f,600.f):0.f;
  return H;
 }();
 return Value;
}

const BattleTactics::FAreaStrategyTuning& BattleTactics::AreaStrategy()
{
 static const FAreaStrategyTuning Value=[]()
 {
  const auto* T=GetDefault<UBattleTuning>();FAreaStrategyTuning S;int32 Enabled=T->bAreaStrategy?1:0;
  S.AttackWeight=T->AreaAttackWeight;S.BaseShare=T->AreaBaseShare;S.bFrontBalance=T->bAreaFrontBalance;S.FrontGain=T->AreaFrontGain;S.FocusGain=T->AreaFocusGain;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaStrategy="),Enabled);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaAttackWeight="),S.AttackWeight);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseShare="),S.BaseShare);
  int32 BaseRule=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseRule="),BaseRule);S.bBaseRule=BaseRule!=0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseShareLow="),S.BaseShareLow);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseShareHigh="),S.BaseShareHigh);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseLowHP="),S.BaseLowHP);
  int32 Front=S.bFrontBalance?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaFrontBalance="),Front);S.bFrontBalance=Front!=0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaFrontGain="),S.FrontGain);
  S.FrontGain=FMath::IsFinite(S.FrontGain)?FMath::Clamp(S.FrontGain,0.f,5.f):1.f;
  float Infiltrate=0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseInfiltrate="),Infiltrate);
  S.InfiltrateRadius=FMath::IsFinite(Infiltrate)?FMath::Clamp(Infiltrate,0.f,500.f)*100.f:0.f;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaFocus="),S.FocusGain);
  S.FocusGain=FMath::IsFinite(S.FocusGain)?FMath::Clamp(S.FocusGain,0.f,10.f):0.f;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseFlank="),S.FlankOffset);
  S.FlankOffset=FMath::IsFinite(S.FlankOffset)?FMath::Clamp(S.FlankOffset,0.f,1.6f):0.f;
  int32 SameSide=S.bFlankSameSide?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseFlankSide="),SameSide);S.bFlankSameSide=SameSide!=0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaBaseReturnLiving="),S.BaseReturnLiving);S.BaseReturnLiving=FMath::Clamp(S.BaseReturnLiving,0,9);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaPlan="),S.Plan);S.Plan=FMath::Clamp(S.Plan,0,2);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaHeavyShare="),S.HeavyShare);S.HeavyShare=!FMath::IsFinite(S.HeavyShare)||S.HeavyShare<0?-1.f:FMath::Min(S.HeavyShare,1.f);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaScoutKeep="),S.ScoutKeep);S.ScoutKeep=FMath::Clamp(S.ScoutKeep,-1,10);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaATKeep="),S.ATKeep);S.ATKeep=FMath::Clamp(S.ATKeep,-1,10);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaTankKeep="),S.TankKeep);S.TankKeep=FMath::Clamp(S.TankKeep,-1,10);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaACKeep="),S.ACKeep);S.ACKeep=FMath::Clamp(S.ACKeep,-1,10);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaMortarKeep="),S.MortarKeep);S.MortarKeep=FMath::Clamp(S.MortarKeep,-1,10);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaSPGKeep="),S.SPGKeep);S.SPGKeep=FMath::Clamp(S.SPGKeep,-1,10);
  // Trial -BattleAreaExoKeep=n (default -1 = UBattleTuning::AreaExoKeep, 0): armoured exoskeleton soldiers the AI keeps (the trial team with -BattleAreaTrialTeam).
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaExoKeep="),S.ExoKeep);S.ExoKeep=FMath::Clamp(S.ExoKeep,-1,10);
  // Trial -BattleAreaSupplyKeep=n (default -1 = UBattleTuning::AreaSupplyKeep, 0): supply vehicles the AI keeps (the trial team with -BattleAreaTrialTeam).
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaSupplyKeep="),S.SupplyKeep);S.SupplyKeep=FMath::Clamp(S.SupplyKeep,-1,10);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaTrialTeam="),S.TrialTeam);S.TrialTeam=S.TrialTeam==0||S.TrialTeam==1?S.TrialTeam:-1;
  S.BaseShareLow=FMath::IsFinite(S.BaseShareLow)?FMath::Clamp(S.BaseShareLow,0.f,0.5f):0.f;
  S.BaseShareHigh=FMath::IsFinite(S.BaseShareHigh)?FMath::Clamp(S.BaseShareHigh,0.f,0.5f):0.3f;
  S.BaseLowHP=FMath::IsFinite(S.BaseLowHP)?FMath::Clamp(S.BaseLowHP,0.f,1000.f):500.f;
  S.bEnabled=Enabled!=0;
  S.AttackWeight=FMath::IsFinite(S.AttackWeight)?FMath::Clamp(S.AttackWeight,0.1f,10.f):2.f;
  S.BaseShare=FMath::IsFinite(S.BaseShare)?FMath::Clamp(S.BaseShare,0.f,0.5f):0.2f;
  return S;
 }();
 return Value;
}

const BattleTactics::FAreaIncomeTuning& BattleTactics::AreaIncome()
{
 static const FAreaIncomeTuning Value=[]()
 {
  const auto* T=GetDefault<UBattleTuning>();FAreaIncomeTuning I;int32 Enabled=T->bAreaIncome?1:0;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIncome="),Enabled);
  I.bEnabled=Enabled!=0;I.Seconds=FMath::IsFinite(T->AreaIncomeSeconds)?FMath::Clamp(T->AreaIncomeSeconds,1.f,600.f):20.f;
  I.Base=FMath::Clamp(T->AreaBaseIncome,0,100);I.PerPoint=FMath::Clamp(T->AreaPointIncome,0,100);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIncomeBase="),I.Base);FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIncomePerPoint="),I.PerPoint);
  I.Base=FMath::Clamp(I.Base,0,100);I.PerPoint=FMath::Clamp(I.PerPoint,0,100);
  I.UrbanBase=T->AreaBaseIncomeUrban;I.UrbanPerPoint=T->AreaPointIncomeUrban;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIncomeUrbanBase="),I.UrbanBase);FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIncomeUrbanPerPoint="),I.UrbanPerPoint);
  I.UrbanBase=I.UrbanBase<0?-1:FMath::Min(I.UrbanBase,100);I.UrbanPerPoint=I.UrbanPerPoint<0?-1:FMath::Min(I.UrbanPerPoint,100);
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaIncomeRamp="),I.Ramp);I.Ramp=FMath::IsFinite(I.Ramp)?FMath::Clamp(I.Ramp,0.f,2.f):0.f;
  FParse::Value(FCommandLine::Get(),TEXT("BattleAreaStartCredits="),I.StartCredits);I.StartCredits=FMath::Clamp(I.StartCredits,0,1000);
  return I;
 }();
 return Value;
}

int32 BattleTactics::LeaderHealthSaveBits()
{
 // 0 for the former x1 rule, so saves made before the multiplier existed load only under x1.
 return FMath::Clamp(FMath::RoundToInt(LeaderHealthMultiplier()*10.f)-10,0,90)<<9;
}

bool BattleTactics::ReacquireGraceEnabled()
{
 // Config default (UBattleTuning::bReacquireGrace, adopted ON 2026-11-13); -BattleReacquireGrace=0/1 overrides for trials.
 static const bool Enabled=[](){int32 Value=GetDefault<UBattleTuning>()->bReacquireGrace?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleReacquireGrace="),Value);return Value!=0;}();
 return Enabled;
}

bool BattleTactics::CoverFireEnabled()
{
 // Config default (UBattleTuning::bAICoverFire, ON since 2026-11-14); -BattleCoverFire=0/1 overrides for trials.
 static const bool Enabled=[](){int32 Value=GetDefault<UBattleTuning>()->bAICoverFire?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverFire="),Value);return Value!=0;}();
 return Enabled;
}
bool BattleTactics::CoverValidationEnabled()
{
 // Config default (UBattleTuning::bAICoverValidation, ON); -BattleCoverValidation=0/1 overrides for trials.
 static const bool Enabled=[](){int32 Value=GetDefault<UBattleTuning>()->bAICoverValidation?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverValidation="),Value);return Value!=0;}();
 return Enabled;
}
bool BattleTactics::CoverPeekSightEnabled()
{
 // Config default (UBattleTuning::bAICoverPeekSight, ON since 2026-11-14); -BattleCoverPeekSight=0/1 overrides.
 static const bool Enabled=[](){int32 Value=GetDefault<UBattleTuning>()->bAICoverPeekSight?1:0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverPeekSight="),Value);return Value!=0;}();
 return Enabled;
}

bool BattleTactics::ReturnFireEnabled()
{
 static const bool Enabled=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleReturnFire="),Value);return Value!=0;}();
 return Enabled;
}

bool BattleTactics::CoverApproachEnabled(const UWorld* World)
{
 static const int32 Mode=[](){int32 Value=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverApproach="),Value);return Value;}();
 return Mode<0 ? BattleFieldLayout::Of(World).bSectors : Mode!=0;
}

bool BattleTactics::ContactCover(int32 Team)
{
 static const int32 Only=[](){int32 Value=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleContactCoverTeam="),Value);return Value==0||Value==1?Value:-1;}();
 return ContactCoverEnabled() && (Only<0 || Only==Team);
}

bool BattleTactics::ContactCoverEnabled()
{
 static const bool Enabled=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleContactCover="),Value);return Value!=0;}();
 return Enabled;
}

bool BattleTactics::CoverActionsEnabled()
{
 static const bool Enabled=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleCoverActions="),Value);return Value!=0;}();
 return Enabled;
}

float BattleTactics::SpreadDegrees(const FBattleUnitState& State, double Now, float SpreadScale)
{
	const auto* Tuning = GetDefault<UBattleTuning>();
	const bool bSettled = State.AimStableSince >= 0. && Now - State.AimStableSince >= FMath::Max(0.f, Tuning->AimSettleSeconds);
	const float Base = State.bMovingShot ? Tuning->MovingSpreadDegrees : bSettled ? Tuning->SettledSpreadDegrees : Tuning->StandingSpreadDegrees;
	return FMath::Clamp(Base * SpreadScale * State.SpreadSkillScale * (State.Weapon == EBattleWeapon::SupportGun && State.bMovingShot ? Tuning->SupportMovingSpreadScale : 1.f), .05f, 15.f);
}

FVector BattleTactics::ShotDirection(const FBattleUnitState& State, FVector Aim, double Now, float SpreadScale)
{
	// Match-local sequence remains in canonical state across possession changes.
	static const uint32 TrialSalt=[](){int32 Value=0;FParse::Value(FCommandLine::Get(),TEXT("BattleTrialSeed="),Value);return uint32(Value)*2654435761u;}();
	const uint32 Seed = static_cast<uint32>(State.UnitID) * 196613u + static_cast<uint32>(State.ShotSequence) * 314159u + 271828u + TrialSalt;
	FRandomStream Random(static_cast<int32>(Seed));
	return Random.VRandCone(Aim.GetSafeNormal(), FMath::DegreesToRadians(SpreadDegrees(State, Now, SpreadScale)));
}

FBattleExposure BattleTactics::ExposureFrom(const ABattleUnit* Target, FVector AttackOrigin)
{
	if (!Target || !Target->IsAlive()) return FBattleExposure();
	return ExposureAt(Target->GetWorld(), Target->GetActorLocation(), Target->GetActorForwardVector(), AttackOrigin);
}

bool BattleTactics::HasTerrainLine(UWorld* World, FVector From, FVector To)
{
	if (auto* Metrics = World->GetSubsystem<UBattleMetrics>(); Metrics->bRecording) ++Metrics->TerrainRays;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleTacticalLine), false);
	if (const auto* Mode = World->GetAuthGameMode<ABattleGameMode>())
		for (ABattleUnit* Unit : Mode->Units) if (Unit) Query.AddIgnoredActor(Unit);
	World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
	// Soldiers' bodies do not block a terrain line (default since 2026-11-19, user decision; -BattleTerrainLineBodies=0 restores the
	// old behaviour). The ignore list above holds only the leaders' actors; a follower is a bare capsule that blocks every channel,
	// so a line to the place where a follower stands always ended on his own capsule and every cover test against him read
	// "hidden, no line of fire".
	static const bool bSkipBodies = []() { int32 Value = 1; FParse::Value(FCommandLine::Get(), TEXT("BattleTerrainLineBodies="), Value); return Value != 0; }();
	if (bSkipBodies)
		for (int32 Pass = 0; Pass < 8 && Hit.bBlockingHit && Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_Pawn; ++Pass)
		{
			Query.AddIgnoredComponent(Hit.GetComponent()); Hit = FHitResult();
			World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
		}
	return !Hit.bBlockingHit;
}

FBattleExposure BattleTactics::ExposureAt(UWorld* World, FVector Position, FVector Forward, FVector AttackOrigin)
{
	FBattleExposure Result;
	const FVector TowardAttacker = (AttackOrigin - Position).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, TowardAttacker);
	const FVector Samples[] = { FVector(0, 0, 60), FVector(0, 0, 30), FVector(0, 0, -35), Right * 25.f + FVector(0, 0, 25), -Right * 25.f + FVector(0, 0, 25) };
	for (const FVector& Offset : Samples)
	{
		if (HasTerrainLine(World, Position + Offset, AttackOrigin)) ++Result.VisibleSamples;
	}
	Result.bFlanked = FVector::DotProduct(Forward.GetSafeNormal2D(), TowardAttacker) <
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(GetDefault<UBattleTuning>()->FlankAngleDegrees, 1.f, 179.f)));
	return Result;
}
