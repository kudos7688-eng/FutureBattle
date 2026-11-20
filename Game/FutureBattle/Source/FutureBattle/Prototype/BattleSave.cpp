#include "BattleSave.h"
#include "BattleComposition.h"
#include "BattleLeaderRuntime.h"
#include "BattleFieldLayout.h"
#include "BattleSquadTactics.h"
#include "BattleTactics.h"
#include "BattleTeamAI.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "FutureBattle.h"
#include "BattleController.h"
#include "BattleCover.h"
#include "BattleSquadAI.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	bool InRange(double Value, double Min, double Max) { return FMath::IsFinite(Value) && Value >= Min && Value <= Max; }
	// Limit: FMapDef::SaveLimit of the map for mixed saves, 100m for the old all-actor saves.
	bool ValidSnapshotPosition(const FVector& P, FVector2D Limit) { return !P.ContainsNaN() && FMath::Abs(P.X) <= Limit.X && FMath::Abs(P.Y) <= Limit.Y && InRange(P.Z, -1000, 10000); }
	bool ValidTeam(int32 Team, bool bNeutral = false) { return Team == 0 || Team == 1 || (bNeutral && Team == INDEX_NONE); }
}

bool UBattleSave::Validate(FString& Error) const
{
	const bool Mixed=Version==2||Version==3;
 if(CombatStateVersion<0||CombatStateVersion>4||(!Mixed&&CombatStateVersion!=0)||
    (CombatStateVersion==0&&(!SavedArtillery.IsEmpty()||!SavedArtilleryShells.IsEmpty())))
 {Error=TEXT("Unsupported combat state extension");return false;}
 if(ArtilleryTrialFlags<0||ArtilleryTrialFlags>15||(CombatStateVersion<3&&ArtilleryTrialFlags!=0))
 {Error=TEXT("Invalid artillery trial configuration");return false;}
 if(CombatStateVersion<2&&(!SavedGroups.IsEmpty()||!SavedSquadGroup.IsEmpty()||!SavedCarChoices.IsEmpty()||SavedNextGroupID!=1))
 {Error=TEXT("Strategic state requires combat extension 2");return false;}
 if(StrengthShieldTrialFlags < -1||StrengthShieldTrialFlags>3){Error=TEXT("Invalid strength/shield trial flags");return false;}
 if(!BattleComposition::ValidateSave(*this,Error))return false;
 auto ValidPosition=[&](const FVector& P){return ValidSnapshotPosition(P,Mixed?BattleFieldLayout::ByName(MapName).SaveLimit:FVector2D(10000,10000));};
	auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
	if ((Version != 1 && Version != 2 && Version!=3) || (Version==3)!=(Ruleset==TEXT("Area")) || !MatchID.IsValid() || Phase < 1 || Phase > 3 || !ValidTeam(Winner, true) || Result.Len() > 200 || ViewRotation.ContainsNaN()) return Fail(TEXT("Unsupported or invalid match header"));
	if (!InRange(AllyCredits, 0, 1000000) || !InRange(EnemyCredits, 0, 1000000) || !InRange(AllyScore, 0, 1000) || !InRange(EnemyScore, 0, 1000) || !InRange(Elapsed, 0, 1000) || !InRange(DirectorClock, 0, 5) || !InRange(SupplyClock, 0, Mixed?1000:1)) return Fail(TEXT("Invalid economy or match timers"));
	// Pending sighting reports: cap raised 100000 -> 1000000 on 2026-11-16 (a heavy Area fight reached 100822 at 80 s).
	// Area units 1000 -> 2000, squads 100 -> 400, mixed observers 500 -> 2000 on 2026-11-20: with -BattleAreaMaxSquads=40 map E had 516 soldiers
	// at the 80 s save, and records keep retired soldiers (start 400 + about 340 hired + 200 replaced is estimated at about 930 by the end).
	const int32 ExpectedObjectives=Mixed?BattleFieldLayout::ObjectiveCountForMap(MapName):3;
	if (Objectives.Num() != ExpectedObjectives || Units.Num() > (Version==3?2000:500) || Squads.Num() > (Version==3?400:100) || Observers.Num() > (Mixed?2000:100) || Reports.Num() > 1000000) UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_SAVE_SIZE objectives=%d units=%d squads=%d observers=%d reports=%d"),Objectives.Num(),Units.Num(),Squads.Num(),Observers.Num(),Reports.Num());
	if (Objectives.Num() != ExpectedObjectives || Units.Num() > (Version==3?2000:500) || Squads.Num() > (Version==3?400:100) || Observers.Num() > (Mixed?2000:100) || Reports.Num() > 1000000) return Fail(TEXT("Invalid snapshot size"));
	if (Strikes.Num() > 2 || !InRange(AllySupportCooldown, 0, 20) || !InRange(EnemySupportCooldown, 0, 20) || !InRange(AllySupportUsed, 0, 3) || !InRange(EnemySupportUsed, 0, 3)) return Fail(TEXT("Invalid support resources"));
	TSet<int32> StrikeTeams;
	for (const auto& Strike : Strikes)
	{
		if (!ValidTeam(Strike.Team) || StrikeTeams.Contains(Strike.Team) || !ValidPosition(Strike.Position) || !InRange(Strike.Remaining, 0, 3) || (Strike.Team ? EnemySupportUsed : AllySupportUsed) < 1 || (Strike.Team ? EnemySupportCooldown : AllySupportCooldown) < Strike.Remaining) return Fail(TEXT("Invalid pending strike"));
		StrikeTeams.Add(Strike.Team);
	}
	const FVector Expected[] = { FVector(-800, -1800, 0), FVector(0, 1800, 0), FVector(800, -1800, 0) };
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const auto& P = Objectives[Index];
		if (!P.Position.Equals(Mixed?(MapName==TEXT("Lvl_BattleField500")?FVector(0,(Index-1)*4500,0):BattleFieldLayout::ObjectiveForMap(MapName,Index)):Expected[Index], .01) || !ValidTeam(P.Owner, true) || !ValidTeam(P.CapturingTeam, true) || !InRange(P.CaptureProgress, 0, 8) || !InRange(P.IncomeClock, 0, 5)) return Fail(TEXT("Invalid objective state"));
	}
	TMap<int32, const FBattleUnitState*> ByID; TSet<int32> SquadIDs; int32 Alive = 0;
	for (const auto& Entry : Units)
	{
		const auto& S = Entry.State; const auto& P = S.Perception; const auto& D = S.Decision;
  if(S.WeaponAimDirection.ContainsNaN() || (S.bWeaponAimInitialized && !FMath::IsNearlyEqual(S.WeaponAimDirection.SizeSquared(),1.,.01)) || S.CoverPhase>4 || !InRange(S.CoverSeconds,0,3.01) || S.CoverOrderRevision<0 || !ValidPosition(S.CoverAnchor) || !ValidPosition(S.CoverPeek) || (S.CoverPhase && (!Mixed || !(TacticalFeatures&128) || !S.IsAlive())))return Fail(TEXT("Invalid cover or weapon aim state"));
  // A soldier behind low cover stands up to fire where he is: the peek point is the anchor itself (distance 0).
  if(S.CoverPhase && FVector::Dist2D(S.CoverAnchor,S.CoverPeek)>1. && !InRange(FVector::Dist2D(S.CoverAnchor,S.CoverPeek),100,450))return Fail(TEXT("Invalid cover peek distance"));
		if (S.UnitID <= 0 || S.UnitID > 1000000 || ByID.Contains(S.UnitID) || !ValidTeam(S.Team) || !ValidPosition(Entry.Position) || Entry.Rotation.ContainsNaN() || !ValidPosition(S.Destination)) return Fail(TEXT("Invalid or duplicate unit identity/transform"));
		if (!InRange(S.HP, 0, 10000) || !InRange(S.Ammo, 0, 10000) || !InRange(S.Reserve, 0, 100000) || !InRange(S.ReloadEndTime, 0, 120) || !InRange(S.NextShotTime, 0, 120) || S.ShotSequence < 0 || S.OrderRevision < 0 || S.PerceptionRevision < 0 || S.DecisionRevision < 0) return Fail(TEXT("Invalid unit resources or timers"));
		if (static_cast<uint8>(S.Weapon) > static_cast<uint8>(EBattleWeapon::Mortar) || static_cast<uint8>(S.Order) > static_cast<uint8>(EBattleOrder::Defend) || static_cast<uint8>(S.Training) > 2 || !InRange(S.SpreadSkillScale, .25, 3) || !InRange(S.HireCost, 1, 1000000)) return Fail(TEXT("Invalid unit order or training"));
		if (!InRange(P.SightRange, 100, 20000) || !InRange(P.FieldOfView, 1, 360) || !InRange(P.DetectionSeconds, 0, 30) || !InRange(P.MemorySeconds, 0, 120) || !InRange(P.ShareRange, 0, 20000) || !InRange(P.ShareDelay, 0, 30) || !InRange(D.ThinkSeconds, .01, 10) || !InRange(D.CoverThinkSeconds, .01, 30) || !InRange(D.CoverCandidates, 1, 100)) return Fail(TEXT("Invalid unit capabilities"));
  if(!InRange(S.Shield,0,1000)||!InRange(S.ShieldWait,0,BattleTactics::ExoShieldHitDelay())||
     !InRange(S.Launcher,0,20)||!InRange(S.LauncherWait,0,FMath::Max(.5f,GetDefault<UBattleTuning>()->ExoLauncherReload)))return Fail(TEXT("Invalid shield or launcher resources"));
  if(!Mixed&&(S.Shield!=0||S.ShieldWait!=0||S.Launcher!=0||S.LauncherWait!=0||S.bLauncherSelected))return Fail(TEXT("Exoskeleton resources outside mixed mode"));
		if (S.IsAlive()) ++Alive; ByID.Add(S.UnitID, &S);
	}
	if (Alive > (Mixed?(Version==3?1000:500):100)) return Fail(TEXT("Too many living units")); // area 500 -> 1000 on 2026-11-20 (40-squad limit)
	TSet<int32> Assigned;
	for (const auto& Squad : Squads)
	{
		if (Squad.SquadID <= 0 || Squad.SquadID > 1000000 || SquadIDs.Contains(Squad.SquadID) || !ValidTeam(Squad.Team) || Squad.Members.IsEmpty() || Squad.Members.Num() > (Mixed?10:5) || !ValidPosition(Squad.Destination) || !InRange(Squad.DefenseRadius, 250, 10000) || Squad.OrderRevision < 0 || static_cast<uint8>(Squad.Order) > static_cast<uint8>(EBattleOrder::Defend)) return Fail(TEXT("Invalid squad"));
		SquadIDs.Add(Squad.SquadID);
  if (Squad.bHasMission)
  {
   if (static_cast<uint8>(Squad.MissionOrder) > static_cast<uint8>(EBattleOrder::Defend) ||
       !ValidPosition(Squad.MissionDestination) || Squad.MissionRevision < 0 || Squad.MissionGoals.Num() > (Mixed?10:5))
    return Fail(TEXT("Invalid squad mission"));
   for (const auto& Goal : Squad.MissionGoals)
    if (!Squad.Members.Contains(Goal.Key) || !ValidPosition(Goal.Value)) return Fail(TEXT("Invalid mission member goal"));
   for (int32 ID : Squad.Members) if (const auto* const* U = ByID.Find(ID); U && (*U)->IsAlive())
    if (!Squad.MissionGoals.Contains(ID)) return Fail(TEXT("Missing living mission member goal"));
  }
		for (int32 ID : Squad.Members)
		{
			const auto* const* S = ByID.Find(ID);
			if (!S || Assigned.Contains(ID) || (*S)->SquadID != Squad.SquadID || (*S)->Team != Squad.Team) return Fail(TEXT("Broken squad membership"));
			Assigned.Add(ID);
		}
	}
 for(const auto& U:Units)if(U.State.bRetired)
 {
  if(Version!=3||U.State.IsAlive()||U.State.bActorRepresentation||U.State.bCanPlayerControl||!U.State.bLeaderManaged||Assigned.Contains(U.State.UnitID)||!SquadIDs.Contains(U.State.SquadID))return Fail(TEXT("Invalid retired soldier"));
  Assigned.Add(U.State.UnitID);
 }
	if (Assigned.Num() != Units.Num()) return Fail(TEXT("Unit without a squad"));
	for (int32 ID : { SelectedID, ControlledID }) if (ID != INDEX_NONE)
	{
		const auto* const* S = ByID.Find(ID); if (!S || !(*S)->IsAlive() || (*S)->Team != 0) return Fail(TEXT("Invalid player control target"));
	}
	if (ControlledID != INDEX_NONE && Phase != static_cast<uint8>(EBattleMatchPhase::Combat)) return Fail(TEXT("Direct control outside combat"));
	auto ValidContact = [&](int32 Receiver, const FBattleContact& C)
	{
		const auto* const* Source = ByID.Find(C.SourceUnitID); const auto* const* Target = ByID.Find(C.TargetID); const auto* const* Observer = ByID.Find(Receiver);
		return (C.ObservedSquadType==255||C.ObservedSquadType<BattleSquadType::Count) && Observer && Source && Target && (*Source)->Team == (*Observer)->Team && (*Target)->Team != (*Observer)->Team && ValidPosition(C.LastKnownPosition) && InRange(C.LastSeenTime, -1000000, 0.01) && InRange(C.Confidence, 0, 1);
	};
	for (const auto& Observer : Observers)
	{
		const auto* const* S = ByID.Find(Observer.Key);
		if (!S || !(*S)->IsAlive() || Observer.Value.ProfileRevision != (*S)->PerceptionRevision || Observer.Value.Contacts.Num() > (Version==3?1000:Mixed?500:100) || Observer.Value.DetectionStarted.Num() > (Version==3?1000:Mixed?500:100)) return Fail(TEXT("Invalid observer"));
		for (const auto& Contact : Observer.Value.Contacts) if (Contact.Key != Contact.Value.TargetID || !ValidContact(Observer.Key, Contact.Value)) return Fail(TEXT("Invalid known contact"));
		for (const auto& Detection : Observer.Value.DetectionStarted)
			if (!ByID.Contains(Detection.Key) || ByID[Detection.Key]->Team == (*S)->Team || !InRange(Detection.Value, -1000000, .01)) return Fail(TEXT("Invalid detection timer"));
	}
	for (const auto& Report : Reports)
		if (!ValidContact(Report.ReceiverID, Report.Contact) || !InRange(Report.DeliveryTime, -120, 30) || Report.SourceRevision < 0 || Report.ReceiverRevision < 0) return Fail(TEXT("Invalid pending report"));
	if(Mixed && !ValidateMixed(Error))return false;
	Error.Reset(); return true;
}

UBattleSave* UBattleMatch::CaptureSnapshot() const
{
	if (!IsEnabled()) return nullptr;
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>(); auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	if (!Mode || !PC) return nullptr;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const bool Mixed=Runtime->IsWideField();
 if(Runtime->IsEnabled()&&!Mixed)return nullptr;
 auto* Save = NewObject<UBattleSave>(); const double Now = GetWorld()->GetTimeSeconds();
 if(Mixed)
 {
  Save->CombatStateVersion=3;
  // OFF keeps extension 3 and does not evaluate planner definitions/rules or allocate planner state.
  if(bArea&&CompositionTeams!=0)
  {
   Save->CombatStateVersion=4;Save->CompositionTeams=CompositionTeams;Save->CompositionPolicy=BattleComposition::PolicyVersion;
   Save->CompositionRules=BattleComposition::RulesKey();Save->CompositionState=CompositionState;
  }
  Save->ArtilleryTrialFlags=BattleTactics::ArtilleryTrialFlags();
  Save->StrengthShieldTrialFlags=(BattleTactics::ExoShieldSuppressionEnabled()?1:0)|(BattleTactics::GroupStrengthEnabled()?2:0);
  Save->SavedNextGroupID=NextGroupID;Save->SavedSquadGroup=SquadGroup;
  for(const auto& Pair:Groups)
  {
   const auto& G=Pair.Value;FBattleSavedGroup E;
   E.ID=G.ID;E.Team=G.Team;E.Squads=G.Squads;E.Objective=G.Objective;E.Phase=G.Phase;
   E.PhaseAge=double(Elapsed)-double(G.PhaseAt);E.Rally=G.Rally;E.bPlayer=G.bPlayer;E.Roles=G.Roles;
   E.Avoid=G.Avoid;E.AvoidRemaining=double(G.AvoidUntil)-double(Elapsed);E.StartLiving=G.StartLiving;E.StartStrength=G.StartStrength;
   E.StallPoint=G.StallPoint;E.Stalls=G.Stalls;E.bOpened=G.bOpened;E.bArrived=G.bArrived;
   E.ArrivalAge=double(Elapsed)-double(G.ArrivedAt);E.Extensions=G.Extensions;
   Save->SavedGroups.Add(Pair.Key,E);
  }
  for(const auto& Pair:CarChoices)
  {
   FBattleSavedCarChoice E;E.Target=Pair.Value.Target;E.bHasTime=Pair.Value.At>=0;
   E.Age=E.bHasTime?double(Elapsed)-Pair.Value.At:0.;Save->SavedCarChoices.Add(Pair.Key,E);
  }
  for(const auto& Pair:Runtime->Artillery)
  {
   const auto& A=Pair.Value;FBattleSavedArtillery E;
   E.bSettingUp=A.StillSince>=0;E.SetupElapsed=E.bSettingUp?FMath::Max(0.,Now-A.StillSince)+A.SetupCredit:0.;
   E.ReloadRemaining=FMath::Max(0.,A.ReadyAt-Now);E.PickRemaining=FMath::Max(0.,A.NextPick-Now);
   E.Shots=A.Shots;E.TargetKey=A.TargetKey;E.Aim=A.Aim;E.bHasTarget=A.bHasTarget;E.bBase=A.bBase;
   Save->SavedArtillery.Add(Pair.Key,E);
  }
  for(const auto& Shell:Runtime->ArtilleryShells)
  {
   FBattleSavedArtilleryShell E;E.ShooterID=Shell.ShooterID;E.Team=Shell.Team;E.ShooterType=Shell.ShooterType;
   E.bSPG=Shell.bSPG;E.At=Shell.At;E.Remaining=Shell.Remaining;Save->SavedArtilleryShells.Add(E);
  }
  Save->Version=bArea?3:2;Save->Area=Area;Save->MapName=UGameplayStatics::GetCurrentLevelName(GetWorld(),true);Save->Ruleset=Runtime->SnapshotMode();
  Save->BaseSightRange=GetDefault<UBattleTuning>()->EffectiveSightRange();Save->WeaponRange=GetDefault<UBattleTuning>()->EffectiveWeaponRange();Save->bRangeAware=Runtime->RangeAwareEnabled();
  auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();auto* Team=GetWorld()->GetSubsystem<UBattleTeamAI>();
  Save->TacticalFeatures=T->FeatureMask()|(Team->IsEnabled()?8:0)|BattleTactics::LeaderHealthSaveBits();T->CaptureSave(*Save,Now);Team->CaptureSave(*Save);Save->DelegationClock=DelegationClock;
 }
	Save->MatchID = MatchID; Save->Phase = static_cast<uint8>(Phase); Save->AllyCredits = Credits[0]; Save->EnemyCredits = Credits[1];
	Save->AllyScore = Score[0]; Save->EnemyScore = Score[1]; Save->Elapsed = Elapsed; Save->DirectorClock = DirectorClock; Save->SupplyClock = SupplyClock;
	Save->Objectives = Objectives; Save->Winner = Winner; Save->Result = Mode->Result; Save->bPaused = Mode->bCombatPaused;
	Save->Strikes = Strikes; Save->AllySupportCooldown = SupportCooldown[0]; Save->EnemySupportCooldown = SupportCooldown[1]; Save->AllySupportUsed = SupportUsed[0]; Save->EnemySupportUsed = SupportUsed[1];
	if (const auto* Unit = Cast<ABattleUnit>(PC->GetPawn())) if (Unit->IsAlive() && Phase == EBattleMatchPhase::Combat) Save->ControlledID = Unit->GetUnitID();
	if (!PC->GetSelection().IsEmpty() && PC->GetSelection()[0]->IsAlive()) Save->SelectedID = PC->GetSelection()[0]->GetUnitID();
	Save->ViewRotation = PC->GetControlRotation();
	auto* Store = GetWorld()->GetSubsystem<UBattleState>(); TSet<int32> SquadIDs;
 // Active plan persistence is ST2+. Never silently save a partial tactical execution in ST1.
 if(!Mixed) for (const auto& Pair : Store->GetSquads()) if (Pair.Value.TacticalPlan.Action != EBattleSquadTactic::Mission) return nullptr;
 if(Mixed) for(const auto& Pair:Store->GetUnits())
 {
  FBattleSavedUnit E;E.State=Pair.Value;E.Position=Runtime->Position(Pair.Key);E.Rotation=Runtime->Forward(Pair.Key).Rotation();
  E.State.NextShotTime=FMath::Max(0.,E.State.NextShotTime-Now);E.State.ReloadEndTime=FMath::Max(0.,E.State.ReloadEndTime-Now);
  Save->Units.Add(E);SquadIDs.Add(E.State.SquadID);
 }
	if(!Mixed) for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->GetState())
	{
		FBattleSavedUnit Entry; Entry.State = *Unit->GetState(); Entry.Position = Unit->GetActorLocation(); Entry.Rotation = Unit->GetActorRotation();
		Entry.State.NextShotTime = FMath::Max(0., Entry.State.NextShotTime - Now);
		Entry.State.ReloadEndTime = FMath::Max(0., Entry.State.ReloadEndTime - Now);
		Save->Units.Add(Entry); SquadIDs.Add(Entry.State.SquadID);
	}
	for (int32 ID : SquadIDs) if (const auto* Squad = Store->FindSquad(ID)) {Save->Squads.Add(*Squad);if(Mixed)Save->SavedPlans.Add(ID,Squad->TacticalPlan);}
 Save->Units.Sort([](const FBattleSavedUnit& A,const FBattleSavedUnit& B){return A.State.UnitID<B.State.UnitID;});
 Save->Squads.Sort([](const FBattleSquadState& A,const FBattleSquadState& B){return A.SquadID<B.SquadID;});
	GetWorld()->GetSubsystem<UBattlePerception>()->Capture(Save->Observers, Save->Reports, Now);
	// Dead observers can remain until the next sliced perception update.
	for (auto It = Save->Observers.CreateIterator(); It; ++It) if (!Store->Find(It.Key()) || !Store->Find(It.Key())->IsAlive() || It.Value().ProfileRevision != Store->Find(It.Key())->PerceptionRevision) It.RemoveCurrent();
	return Save;
}

bool UBattleMatch::RestoreSnapshot(const UBattleSave* Save)
{
	if (!Save || !Save->Validate(LastMessage)) { if (!Save) LastMessage = TEXT("No compatible save found"); return false; }
	auto* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>(); auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	if (!Mode || !PC) return false;
 auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const bool Mixed=Runtime->IsWideField();
 auto* T=GetWorld()->GetSubsystem<UBattleSquadTactics>();auto* Team=GetWorld()->GetSubsystem<UBattleTeamAI>();
 if(Mixed!=(Save->Version>=2) || (Runtime->IsEnabled()&&!Mixed) ||
    (Mixed&&(Save->MapName!=UGameplayStatics::GetCurrentLevelName(GetWorld(),true)||Save->Ruleset!=Runtime->SnapshotMode()||
      Save->BaseSightRange!=GetDefault<UBattleTuning>()->EffectiveSightRange()||Save->WeaponRange!=GetDefault<UBattleTuning>()->EffectiveWeaponRange()||
      Save->bRangeAware!=Runtime->RangeAwareEnabled()||Save->TacticalFeatures!=(T->FeatureMask()|(Team->IsEnabled()?8:0)|BattleTactics::LeaderHealthSaveBits()))))
 {LastMessage=TEXT("Save mode, map or rule configuration does not match");return false;}
 if(Mixed&&Save->CombatStateVersion>=3&&Save->ArtilleryTrialFlags!=BattleTactics::ArtilleryTrialFlags())
 {LastMessage=TEXT("Indirect-fire trial configuration does not match");return false;}
 if(Mixed&&Save->StrengthShieldTrialFlags>=0&&Save->StrengthShieldTrialFlags!=((BattleTactics::ExoShieldSuppressionEnabled()?1:0)|(BattleTactics::GroupStrengthEnabled()?2:0)))
 {LastMessage=TEXT("Strength/shield trial configuration does not match");return false;}
 TArray<ABattleUnit*> Spawned;TArray<FBattleUnitState> Records;const double Now=GetWorld()->GetTimeSeconds();
 if(Save->CombatStateVersion>=4)
 {
  const auto CompositionOptions=BattleComposition::Settings();
  if(Save->CompositionTeams!=(CompositionOptions.bEnabled?CompositionOptions.Teams:0)||Save->CompositionRules!=BattleComposition::RulesKey())
  {LastMessage=TEXT("Composition planner configuration does not match");return false;}
  for(const auto& CompositionEntry:Save->CompositionState)if(CompositionEntry.SaveType>=0&&CompositionEntry.SaveCost!=BattleComposition::Definitions()[CompositionEntry.SaveType].Cost)
  {LastMessage=TEXT("Composition saving price does not match definitions");return false;}
 }
 if(Mixed)
 {
  if(!Runtime->RestoreMixedBodies(Save,LastMessage))return false;
  for(ABattleUnit* U:Mode->Units)if(U)Spawned.Add(U);
 }
 else
 {
 // Stage all actors before touching the active match. No tick or physics step runs in this transaction.
	for (const auto& Entry : Save->Units)
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Unit = GetWorld()->SpawnActor<ABattleUnit>(Entry.Position, Entry.Rotation, Params);
		if (!Unit) { for (auto* Rollback : Spawned) Rollback->Destroy(); LastMessage = TEXT("Restore failed while creating units"); return false; }
		Spawned.Add(Unit);
	}
	PC->ResetMatchControl();
	for (ABattleUnit* Old : Mode->Units) if (Old) { if (Old->SavedAI) Old->SavedAI->Destroy(); Old->Destroy(); }
	Mode->Units.Reset();
	GetWorld()->GetSubsystem<UBattleSquadAI>()->Reset(); GetWorld()->GetSubsystem<UBattleCover>()->ResetReservations();

	for (const auto& Entry : Save->Units)
	{
		auto S = Entry.State;
		S.ReloadEndTime = S.ReloadEndTime > 0 ? Now + S.ReloadEndTime : 0; S.NextShotTime += Now;
		S.Role = EBattleRole::Independent; S.CoordinatedTarget = INDEX_NONE; S.CoverSlot = INDEX_NONE; S.AimStableSince = -1.;
		S.MoveRetries = 0; S.Action = S.IsAlive() ? EBattleAction::Idle : EBattleAction::Dead; S.DecisionReason = TEXT("RESTORED / REEVALUATE ORDER");
		Records.Add(S);
	}
	GetWorld()->GetSubsystem<UBattleState>()->RestoreRecords(Records, Save->Squads);
 }
 // Route preferences remain deliberately unsaved, and are discarded only on a successful load.
 PlayerRoutes.Reset();
	MatchID = Save->MatchID; Phase = static_cast<EBattleMatchPhase>(Save->Phase); Credits[0] = Save->AllyCredits; Credits[1] = Save->EnemyCredits;
	Score[0] = Save->AllyScore; Score[1] = Save->EnemyScore; Elapsed = Save->Elapsed; DirectorClock = Save->DirectorClock; SupplyClock = Save->SupplyClock;
	Objectives = Save->Objectives; Winner = Save->Winner;
 if(bArea)Area=Save->Area;
 if(Save->CombatStateVersion>=4){CompositionTeams=Save->CompositionTeams;CompositionState=Save->CompositionState;}
 else if(CompositionTeams!=0||!CompositionState.IsEmpty())
 {
  CompositionTeams=0;CompositionState.Reset();
  UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_COMPOSITION_LOAD legacy save: planner disabled for this match (no implicit conversion)"));
 }
 // No fallible work remains. Replace, rather than merge, the complete strategic snapshot.
 // Groups and car choices use the restored match clock; artillery uses the new world clock.
 TMap<int32,FBattleGroup> RestoredGroups;TMap<int32,FCarChoice> RestoredChoices;
 if(Save->CombatStateVersion>=2)
 {
  for(const auto& Pair:Save->SavedGroups)
  {
   const auto& E=Pair.Value;FBattleGroup G;
   G.ID=E.ID;G.Team=E.Team;G.Squads=E.Squads;G.Objective=E.Objective;G.Phase=E.Phase;
   G.PhaseAt=float(double(Elapsed)-E.PhaseAge);G.Rally=E.Rally;G.bPlayer=E.bPlayer;G.Roles=E.Roles;
   G.Avoid=E.Avoid;G.AvoidUntil=float(double(Elapsed)+E.AvoidRemaining);G.StartLiving=E.StartLiving;G.StartStrength=E.StartStrength;
   G.StallPoint=E.StallPoint;G.Stalls=E.Stalls;G.bOpened=E.bOpened;G.bArrived=E.bArrived;
   G.ArrivedAt=float(double(Elapsed)-E.ArrivalAge);G.Extensions=E.Extensions;RestoredGroups.Add(Pair.Key,MoveTemp(G));
  }
  for(const auto& Pair:Save->SavedCarChoices)
  {
   FCarChoice C;C.Target=Pair.Value.Target;C.At=Pair.Value.bHasTime?double(Elapsed)-Pair.Value.Age:-1.;
   RestoredChoices.Add(Pair.Key,C);
  }
 }
 Groups=MoveTemp(RestoredGroups);CarChoices=MoveTemp(RestoredChoices);
 SquadGroup=Save->SavedSquadGroup;NextGroupID=Save->SavedNextGroupID;
 // Rebuilt on demand from restored observations; never persist a cached view of the future.
 IntelViews[0]=IntelViews[1]=FAreaIntelView();
	Strikes = Save->Strikes; SupportCooldown[0] = Save->AllySupportCooldown; SupportCooldown[1] = Save->EnemySupportCooldown; SupportUsed[0] = Save->AllySupportUsed; SupportUsed[1] = Save->EnemySupportUsed;
	Mode->bCombatPaused = Save->bPaused; Mode->bBattleEnded = Phase == EBattleMatchPhase::Finished; Mode->Result = Save->Result; Mode->CombatStartsAt = Now;
	for (int32 Index = 0; Index < Spawned.Num(); ++Index)
	{
		auto* Unit = Spawned[Index]; if(!Mixed){Unit->InitializeUnit(Records[Index].UnitID); Mode->Units.Add(Unit);}
		if (!Unit->IsAlive()) Unit->ApplyDeath();
		if (Unit->GetUnitID() == Save->SelectedID) PC->SetSelection(Unit);
	}
	GetWorld()->GetSubsystem<UBattlePerception>()->Restore(Save->Observers, Save->Reports, Now);
	if(Mixed){T->RestoreSave(*Save,Now);Team->RestoreSave(*Save);DelegationClock=Save->DelegationClock;GetWorld()->GetSubsystem<UBattlePerception>()->Update(Mode->Units,Now);Runtime->RefreshDisplay();}
	Mode->RefreshVisibility();
	for (auto* Unit : Spawned) if (Unit->GetUnitID() == Save->ControlledID) { PC->ControlUnit(Unit); PC->SetControlRotation(Save->ViewRotation); break; }
	LastMessage = TEXT("Match restored"); return true;
}

namespace {
// Experiment round trips (-BattleVerifyMixedSave) run several processes at once; a shared smoke slot file made them
// collide ("Could not write save file"), so each such process uses its own smoke slot and removes it after loading.
bool PerProcessSmokeSlot(const FString& Requested)
{
 static const bool Experiment=FParse::Param(FCommandLine::Get(),TEXT("BattleVerifyMixedSave"));
 return Experiment&&Requested==TEXT("FutureBattleSmokeOnly");
}
FString CompatibleSlot(const UBattleLeaderRuntime* R,const FString& Requested)
{
 if(Requested!=TEXT("FutureBattleQuick01")&&Requested!=TEXT("FutureBattleSmokeOnly"))return {};
 if(R->IsEnabled()&&!R->IsWideField())return {};
 FString Slot;
 if(R->bArea)Slot=Requested==TEXT("FutureBattleQuick01")?TEXT("FutureBattleAreaQuick03"):TEXT("FutureBattleAreaSmoke03");
 else if(R->bMixedFastMatch)Slot=Requested==TEXT("FutureBattleQuick01")?TEXT("FutureBattleMixedFastQuick02"):TEXT("FutureBattleMixedFastSmoke02");
 else Slot=R->IsWideField()?(Requested==TEXT("FutureBattleQuick01")?TEXT("FutureBattleMixedQuick02"):TEXT("FutureBattleMixedSmoke02")):Requested;
 if(PerProcessSmokeSlot(Requested))Slot+=FString::Printf(TEXT("_%u"),FPlatformProcess::GetCurrentProcessId());
 return Slot;
}
}
bool UBattleMatch::SaveMatch(const FString& Slot)
{
 const FString Actual=CompatibleSlot(GetWorld()->GetSubsystem<UBattleLeaderRuntime>(),Slot);
 if(Actual.IsEmpty()){LastMessage=TEXT("Unsupported save mode or slot");return false;}
 auto* Save=CaptureSnapshot();if(!Save||!Save->Validate(LastMessage))return false;
 const bool Saved=UGameplayStatics::SaveGameToSlot(Save,Actual,0);
 LastMessage=Saved?TEXT("Match saved [F9 to load]"):TEXT("Could not write save file");return Saved;
}
bool UBattleMatch::LoadMatch(const FString& Slot)
{
 const FString Actual=CompatibleSlot(GetWorld()->GetSubsystem<UBattleLeaderRuntime>(),Slot);
 if(!IsEnabled()||Actual.IsEmpty()){LastMessage=TEXT("Unsupported save mode or slot");return false;}
 auto* Loaded=Cast<UBattleSave>(UGameplayStatics::LoadGameFromSlot(Actual,0));
 if(PerProcessSmokeSlot(Slot))UGameplayStatics::DeleteGameInSlot(Actual,0);
 return RestoreSnapshot(Loaded);
}
