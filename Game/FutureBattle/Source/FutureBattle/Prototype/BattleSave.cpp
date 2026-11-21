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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/ArchiveProxy.h"
#include "UObject/UnrealType.h"

namespace
{
 int32 SaveRoutesSmokeOverride=-1;
 void CompleteDueReloadOnSave(FBattleUnitState& Copy,double Now)
 {
  if(!BattleTactics::SaveReloadBoundary()||!(Copy.ReloadEndTime>0.&&Copy.ReloadEndTime<=Now))return;
  // Same completion as AdvanceReloads, on the copy only; no extra frame of delay.
  if(Copy.IsAlive())
  {
   const int32 Transfer=FMath::Min(Copy.Reserve,FMath::Max(0,GetDefault<UBattleTuning>()->Capacity(Copy.Weapon)-Copy.Ammo));
   Copy.Ammo+=Transfer;Copy.Reserve-=Transfer;
  }
  Copy.ReloadEndTime=0.;
 }
 // Default-valued UPROPERTYs still produce tags in SaveGameToMemory. Filter only the absent extension,
 // without changing global property flags, the existing tags, or object duplication/reference collection.
 class FSaveWithoutRoutesArchive : public FArchiveProxy
 {
 public:
  explicit FSaveWithoutRoutesArchive(FArchive& Inner):FArchiveProxy(Inner){}
  virtual bool ShouldSkipProperty(const FProperty* Property) const override
  {
   return (Property->GetOwnerStruct()==UBattleSave::StaticClass()&&
    (Property->GetFName()==GET_MEMBER_NAME_CHECKED(UBattleSave,PlayerRoutesVersion)||
     Property->GetFName()==GET_MEMBER_NAME_CHECKED(UBattleSave,SavedPlayerRoutes)||
     Property->GetFName()==GET_MEMBER_NAME_CHECKED(UBattleSave,SavedPlayerRouteClock)))||FArchiveProxy::ShouldSkipProperty(Property);
  }
 };
	bool InRange(double Value, double Min, double Max) { return FMath::IsFinite(Value) && Value >= Min && Value <= Max; }
	// Limit: FMapDef::SaveLimit of the map for mixed saves, 100m for the old all-actor saves.
	bool ValidSnapshotPosition(const FVector& P, FVector2D Limit) { return !P.ContainsNaN() && FMath::Abs(P.X) <= Limit.X && FMath::Abs(P.Y) <= Limit.Y && InRange(P.Z, -1000, 10000); }
	bool ValidTeam(int32 Team, bool bNeutral = false) { return Team == 0 || Team == 1 || (bNeutral && Team == INDEX_NONE); }
}

bool BattleSaveValidation::Contact(const FBattleContact& Value,int32 ObserverTeam,const FBattleUnitState* Source,
 const FBattleUnitState* Target,FVector2D Limit)
{
 // Observed type is historical knowledge, not necessarily the target's current type.
 return (Value.ObservedSquadType==255||Value.ObservedSquadType<BattleSquadType::Count)&&Source&&Target&&
  Source->Team==ObserverTeam&&Target->Team!=ObserverTeam&&ValidSnapshotPosition(Value.LastKnownPosition,Limit)&&
  InRange(Value.LastSeenTime,-1000000,.01)&&InRange(Value.Confidence,0,1);
}

USaveGame* BattleSaveValidation::Decode(const TArray<uint8>& Bytes,FString& Error)
{
 if(Bytes.Num()>MaxBytes){Error=TEXT("Save file exceeds size limit");return nullptr;}
 USaveGame* Decoded=Bytes.IsEmpty()?nullptr:UGameplayStatics::LoadGameFromMemory(Bytes);
 Error=Decoded?FString():TEXT("No compatible save found");
 return Decoded;
}

double BattleSaveValidation::ClockLimit(const FString& MapName)
{
 // Keep historical fixture headroom, allow a final frame past the map's match limit.
 return FMath::Clamp(double(BattleFieldLayout::ByName(MapName).MatchSeconds)+1.,1000.,86400.);
}

USaveGame* BattleSaveValidation::Decode(const TArray<uint8>& Bytes)
{
 FString DecodeError;
 return Decode(Bytes,DecodeError);
}

bool BattleTactics::SaveRoutes()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSaveRoutes="),V);return V==1;}();
 return SaveRoutesSmokeOverride>=0?SaveRoutesSmokeOverride==1:On;
}

int32 BattleTactics::OverrideSaveRoutesForSmoke(int32 Value)
{
 const int32 Previous=SaveRoutesSmokeOverride;
 if(FParse::Param(FCommandLine::Get(),TEXT("BattleMixedSaveSmoke")))SaveRoutesSmokeOverride=FMath::Clamp(Value,-1,1);
 return Previous;
}

void UBattleSave::Serialize(FArchive& Ar)
{
 if(Ar.IsSaving()&&Ar.IsPersistent()&&PlayerRoutesVersion==0&&SavedPlayerRoutes.IsEmpty()&&SavedPlayerRouteClock==0)
 {FSaveWithoutRoutesArchive WithoutRoutes(Ar);Super::Serialize(WithoutRoutes);}
 else Super::Serialize(Ar);
}

bool UBattleSave::Validate(FString& Error) const
{
	const bool Mixed=Version==2||Version==3;
 const auto* ResourceTuning=GetDefault<UBattleTuning>();
 const int32 UnitLimit=Version==3?BattleRosterLimits::Area:BattleRosterLimits::Mixed;
 const int32 LivingLimit=Mixed?UnitLimit:100;
 const int32 SquadLimit=Version==3?UnitLimit:100;
 const double MatchClockLimit=BattleSaveValidation::ClockLimit(MapName);
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
	if (!InRange(AllyCredits, 0, 1000000) || !InRange(EnemyCredits, 0, 1000000) || !InRange(AllyScore, 0, 1000) || !InRange(EnemyScore, 0, 1000) || !InRange(Elapsed, 0, MatchClockLimit) || !InRange(DirectorClock, 0, 5) || !InRange(SupplyClock, 0, Mixed?MatchClockLimit:1)) return Fail(TEXT("Invalid economy or match timers"));
	// Pending sighting reports: cap raised 100000 -> 1000000 on 2026-11-16 (a heavy Area fight reached 100822 at 80 s).
 // Records (including losses) share the actual recruitment/replenishment ceiling. Squads may have only one member.
	const int32 ExpectedObjectives=Mixed?BattleFieldLayout::ObjectiveCountForMap(MapName):3;
	if (Objectives.Num() != ExpectedObjectives || Units.Num() > UnitLimit || Squads.Num() > SquadLimit || Observers.Num() > LivingLimit || Reports.Num() > 1000000) UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_SAVE_SIZE objectives=%d units=%d squads=%d observers=%d reports=%d"),Objectives.Num(),Units.Num(),Squads.Num(),Observers.Num(),Reports.Num());
	if (Objectives.Num() != ExpectedObjectives || Units.Num() > UnitLimit || Squads.Num() > SquadLimit || Observers.Num() > LivingLimit || Reports.Num() > 1000000) return Fail(TEXT("Invalid snapshot size"));
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
  if(S.WeaponAimDirection.ContainsNaN() || (S.bWeaponAimInitialized && !FMath::IsNearlyEqual(S.WeaponAimDirection.SizeSquared(),1.,.01)) || S.CoverPhase>4 || !InRange(S.CoverSeconds,0,3.01) || !BattleSaveValidation::Counter(S.CoverOrderRevision) || !ValidPosition(S.CoverAnchor) || !ValidPosition(S.CoverPeek) || (S.CoverPhase && (!Mixed || !(TacticalFeatures&128) || !S.IsAlive())))return Fail(TEXT("Invalid cover or weapon aim state"));
  // A soldier behind low cover stands up to fire where he is: the peek point is the anchor itself (distance 0).
  if(S.CoverPhase && FVector::Dist2D(S.CoverAnchor,S.CoverPeek)>1. && !InRange(FVector::Dist2D(S.CoverAnchor,S.CoverPeek),100,450))return Fail(TEXT("Invalid cover peek distance"));
		if (S.UnitID <= 0 || S.UnitID > 1000000 || ByID.Contains(S.UnitID) || !ValidTeam(S.Team) || !ValidPosition(Entry.Position) || Entry.Rotation.ContainsNaN() || !ValidPosition(S.Destination)) return Fail(TEXT("Invalid or duplicate unit identity/transform"));
  // Absolute caps bound pathological configuration too; ordinary limits come from the producer.
  const double ReloadLimit=FMath::Clamp(double(ResourceTuning->ReloadTime(S.Weapon))+.01,.11,86400.);
  const double ShotIntervalLimit=FMath::Max(double(ResourceTuning->ShotInterval(S.Weapon,true)),double(ResourceTuning->ShotInterval(S.Weapon,false)));
  const double ShotLimit=FMath::Clamp(FMath::Max(ShotIntervalLimit,double(ResourceTuning->AimSwitchDelay(180.f)))+.01,120.,86400.);
  if (!InRange(S.HP, 0, Mixed?1.e9:FMath::Clamp(double(ResourceTuning->Health),10000.,1.e9)) || !InRange(S.Ammo, 0, FMath::Clamp(ResourceTuning->Capacity(S.Weapon),1,1000000)) || !InRange(S.Reserve, 0, FMath::Clamp(ResourceTuning->ReserveFor(S.Weapon),0,1000000)) || !InRange(S.ReloadEndTime, 0, ReloadLimit) || !InRange(S.NextShotTime, 0, ShotLimit) || !BattleSaveValidation::Counter(S.ShotSequence) || !BattleSaveValidation::Counter(S.OrderRevision) || !BattleSaveValidation::Counter(S.PerceptionRevision) || !BattleSaveValidation::Counter(S.DecisionRevision) || !BattleSaveValidation::Counter(S.CoordinationRevision) || !BattleSaveValidation::Counter(S.MoveRetries)) return Fail(TEXT("Invalid unit resources or timers"));
		if (static_cast<uint8>(S.Weapon) > static_cast<uint8>(EBattleWeapon::Mortar) || static_cast<uint8>(S.Order) > static_cast<uint8>(EBattleOrder::Defend) || static_cast<uint8>(S.Training) > 2 || !InRange(S.SpreadSkillScale, .25, 3) || !InRange(S.HireCost, 1, 1000000)) return Fail(TEXT("Invalid unit order or training"));
		if (!InRange(P.SightRange, 100, 20000) || !InRange(P.FieldOfView, 1, 360) || !InRange(P.DetectionSeconds, 0, 30) || !InRange(P.MemorySeconds, 0, 120) || !InRange(P.ShareRange, 0, 20000) || !InRange(P.ShareDelay, 0, 30) || !InRange(D.ThinkSeconds, .01, 10) || !InRange(D.CoverThinkSeconds, .01, 30) || !InRange(D.CoverCandidates, 1, 100)) return Fail(TEXT("Invalid unit capabilities"));
  if(!InRange(S.Shield,0,1000)||!InRange(S.ShieldWait,0,BattleTactics::ExoShieldHitDelay())||
     !InRange(S.Launcher,0,20)||!InRange(S.LauncherWait,0,FMath::Max(.5f,GetDefault<UBattleTuning>()->ExoLauncherReload)))return Fail(TEXT("Invalid shield or launcher resources"));
  if(!Mixed&&(S.Shield!=0||S.ShieldWait!=0||S.Launcher!=0||S.LauncherWait!=0||S.bLauncherSelected))return Fail(TEXT("Exoskeleton resources outside mixed mode"));
		if (S.IsAlive()) ++Alive; ByID.Add(S.UnitID, &S);
	}
	if (Alive > LivingLimit) return Fail(TEXT("Too many living units"));
	TSet<int32> Assigned;
	for (const auto& Squad : Squads)
	{
		if (Squad.SquadID <= 0 || Squad.SquadID > 1000000 || SquadIDs.Contains(Squad.SquadID) || !ValidTeam(Squad.Team) || Squad.Members.IsEmpty() || Squad.Members.Num() > (Mixed?10:5) || !ValidPosition(Squad.Destination) || !InRange(Squad.DefenseRadius, 250, 10000) || !BattleSaveValidation::Counter(Squad.OrderRevision) || !BattleSaveValidation::Counter(Squad.MissionRevision) || !BattleSaveValidation::Counter(Squad.CommandRevision) || !BattleSaveValidation::Counter(Squad.TacticalPlan.Revision) || static_cast<uint8>(Squad.Order) > static_cast<uint8>(EBattleOrder::Defend)) return Fail(TEXT("Invalid squad"));
		SquadIDs.Add(Squad.SquadID);
  if (Squad.bHasMission)
  {
   if (static_cast<uint8>(Squad.MissionOrder) > static_cast<uint8>(EBattleOrder::Defend) ||
       !ValidPosition(Squad.MissionDestination) || Squad.MissionGoals.Num() > (Mixed?10:5))
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
 if(PlayerRoutesVersion<0||PlayerRoutesVersion>1||
    (PlayerRoutesVersion==0&&(!SavedPlayerRoutes.IsEmpty()||SavedPlayerRouteClock!=0))||
    (PlayerRoutesVersion==1&&(!Mixed||SavedPlayerRoutes.IsEmpty())))return Fail(TEXT("Invalid player route extension"));
 if(PlayerRoutesVersion==1)
 {
  if(SavedPlayerRoutes.Num()>Squads.Num()||SavedPlayerRoutes.Num()>SquadLimit||!InRange(SavedPlayerRouteClock,0,1))return Fail(TEXT("Invalid player route extension"));
  TSet<int32> RouteSquads;
  const double MaxRouteWait=20.+2.*BattleFieldLayout::ByName(MapName).SaveLimit.Size()/250.+.1;
  for(const auto& Route:SavedPlayerRoutes)
  {
   const auto* RouteSquad=Squads.FindByPredicate([&](const FBattleSquadState& Candidate){return Candidate.SquadID==Route.SquadID;});
   if(!RouteSquad||RouteSquad->Team!=0||RouteSquad->CommandStatus!=EBattleCommandStatus::Active||RouteSquads.Contains(Route.SquadID))
    return Fail(TEXT("Invalid player route squad"));
   RouteSquads.Add(Route.SquadID);
   if(Route.Lane<0||Route.Lane>3||Route.Phase<0||Route.Phase>2||Route.Order>uint8(EBattleOrder::Defend)||
      (Route.bMission&&Route.Order==uint8(EBattleOrder::Hold))||Route.Waypoints.Num()>96||(!Route.bPath&&!Route.Waypoints.IsEmpty())||
      !ValidPosition(Route.Goal)||(!ValidPosition(Route.Step)&&Route.Step!=FVector(FLT_MAX))||
      (!ValidPosition(Route.PhaseGoal)&&Route.PhaseGoal!=FVector(FLT_MAX))||
      !InRange(Route.DeadlineRemaining,-double(Elapsed),MaxRouteWait))return Fail(TEXT("Invalid player route state"));
   for(const FVector& Waypoint:Route.Waypoints)if(!ValidPosition(Waypoint))return Fail(TEXT("Invalid player route waypoint"));
  }
 }
	for (int32 ID : { SelectedID, ControlledID }) if (ID != INDEX_NONE)
	{
		const auto* const* S = ByID.Find(ID); if (!S || !(*S)->IsAlive() || (*S)->Team != 0) return Fail(TEXT("Invalid player control target"));
	}
	if (ControlledID != INDEX_NONE && Phase != static_cast<uint8>(EBattleMatchPhase::Combat)) return Fail(TEXT("Direct control outside combat"));
	auto ValidContact = [&](int32 Receiver, const FBattleContact& C)
	{
		const auto* const* Source = ByID.Find(C.SourceUnitID); const auto* const* Target = ByID.Find(C.TargetID); const auto* const* Observer = ByID.Find(Receiver);
		return Observer && BattleSaveValidation::Contact(C,(*Observer)->Team,Source?*Source:nullptr,Target?*Target:nullptr,
   Mixed?BattleFieldLayout::ByName(MapName).SaveLimit:FVector2D(10000,10000));
	};
	for (const auto& Observer : Observers)
	{
		const auto* const* S = ByID.Find(Observer.Key);
		if (!S || !(*S)->IsAlive() || Observer.Value.ProfileRevision != (*S)->PerceptionRevision || Observer.Value.Contacts.Num() > LivingLimit || Observer.Value.DetectionStarted.Num() > LivingLimit) return Fail(TEXT("Invalid observer"));
		for (const auto& Contact : Observer.Value.Contacts) if (Contact.Key != Contact.Value.TargetID || !ValidContact(Observer.Key, Contact.Value)) return Fail(TEXT("Invalid known contact"));
		for (const auto& Detection : Observer.Value.DetectionStarted)
			if (!ByID.Contains(Detection.Key) || ByID[Detection.Key]->Team == (*S)->Team || !InRange(Detection.Value, -1000000, .01)) return Fail(TEXT("Invalid detection timer"));
	}
	for (const auto& Report : Reports)
		if (!ValidContact(Report.ReceiverID, Report.Contact) || !InRange(Report.DeliveryTime, -120, 30) || !BattleSaveValidation::Counter(Report.SourceRevision) || !BattleSaveValidation::Counter(Report.ReceiverRevision)) return Fail(TEXT("Invalid pending report"));
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
   E.Kind=G.Kind;E.SurpriseRow=G.SurpriseRow;E.SurpriseBridgeStage=G.SurpriseBridgeStage;E.SurpriseBridgeLeader=G.SurpriseBridgeLeader;
   E.SurpriseBridgeAge=double(Elapsed)-double(G.SurpriseBridgeAt);E.SurpriseNextCol=G.SurpriseNextCol;
   E.FrontHold=G.FrontHold;E.FrontGap=G.FrontGap;E.FrontNeed=G.FrontNeed;E.FrontReleaseAge=double(Elapsed)-double(G.FrontReleaseAt);E.FrontExcessAge=G.FrontExcessSince<0.f?-1.:double(Elapsed)-double(G.FrontExcessSince);E.bFrontDrawn=G.bFrontDrawn;E.FrontBackAge=double(Elapsed)-double(G.FrontBackAt);E.FrontStepAge=double(Elapsed)-double(G.FrontStepAt);
   E.SurpriseBridgeEntry=G.SurpriseBridgeEntry;E.SurpriseBridgeExit=G.SurpriseBridgeExit;E.SurpriseBridgeGoal=G.SurpriseBridgeGoal;
   E.bSurpriseWaiting=G.SurpriseWaitAt>=0.f;E.SurpriseWaitAge=E.bSurpriseWaiting?double(Elapsed)-double(G.SurpriseWaitAt):0.;
   E.SurpriseLogRemaining=double(G.SurpriseLogAt)-double(Elapsed);E.SurpriseEndRemaining=double(G.SurpriseEndAt)-double(Elapsed);
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
  CompleteDueReloadOnSave(E.State,Now);
  E.State.NextShotTime=FMath::Max(0.,E.State.NextShotTime-Now);E.State.ReloadEndTime=FMath::Max(0.,E.State.ReloadEndTime-Now);
  Save->Units.Add(E);SquadIDs.Add(E.State.SquadID);
 }
	if(!Mixed) for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->GetState())
	{
		FBattleSavedUnit Entry; Entry.State = *Unit->GetState(); Entry.Position = Unit->GetActorLocation(); Entry.Rotation = Unit->GetActorRotation();
		CompleteDueReloadOnSave(Entry.State,Now);
		Entry.State.NextShotTime = FMath::Max(0., Entry.State.NextShotTime - Now);
		Entry.State.ReloadEndTime = FMath::Max(0., Entry.State.ReloadEndTime - Now);
		Save->Units.Add(Entry); SquadIDs.Add(Entry.State.SquadID);
	}
	for (int32 ID : SquadIDs) if (const auto* Squad = Store->FindSquad(ID)) {Save->Squads.Add(*Squad);if(Mixed)Save->SavedPlans.Add(ID,Squad->TacticalPlan);}
 Save->Units.Sort([](const FBattleSavedUnit& A,const FBattleSavedUnit& B){return A.State.UnitID<B.State.UnitID;});
 Save->Squads.Sort([](const FBattleSquadState& A,const FBattleSquadState& B){return A.SquadID<B.SquadID;});
 if(Mixed&&BattleTactics::SaveRoutes())
 {
  // Dead/removed squads can leave lane preferences behind; they cannot resume a player order.
  for(const auto& RoutePair:PlayerRoutes)
  {
   const auto* RouteSquad=Store->FindSquad(RoutePair.Key);
   if(!RouteSquad||RouteSquad->Team!=0||RouteSquad->CommandStatus!=EBattleCommandStatus::Active)continue;
   const auto& Route=RoutePair.Value;FBattleSavedPlayerRoute Entry;
   Entry.SquadID=RoutePair.Key;Entry.Lane=Route.Lane;Entry.Phase=Route.Phase;Entry.PhaseGoal=Route.PhaseGoal;
   Entry.bMission=Route.bMission;Entry.Order=uint8(Route.Order);Entry.Goal=Route.Goal;Entry.Step=Route.Step;
   Entry.bPath=Route.bPath;Entry.Waypoints=Route.Waypoints;Entry.DeadlineRemaining=double(Route.StepDeadline)-double(Elapsed);
   Save->SavedPlayerRoutes.Add(MoveTemp(Entry));
  }
  if(!Save->SavedPlayerRoutes.IsEmpty())
  {
   Save->PlayerRoutesVersion=1;Save->SavedPlayerRouteClock=PlayerRouteClock;
   Save->SavedPlayerRoutes.Sort([](const FBattleSavedPlayerRoute& A,const FBattleSavedPlayerRoute& B){return A.SquadID<B.SquadID;});
  }
 }
	GetWorld()->GetSubsystem<UBattlePerception>()->Capture(Save->Observers, Save->Reports, Now);
	// Dead observers can remain until the next sliced perception update.
	for (auto It = Save->Observers.CreateIterator(); It; ++It) if (!Store->Find(It.Key()) || !Store->Find(It.Key())->IsAlive() || It.Value().ProfileRevision != Store->Find(It.Key())->PerceptionRevision) It.RemoveCurrent();
	return Save;
}

bool UBattleMatch::RestoreSnapshot(const UBattleSave* Save)
{
	if (!Save || !Save->Validate(LastMessage)) { if (!Save) LastMessage = TEXT("No compatible save found"); return false; }
 if(Save->PlayerRoutesVersion!=0&&!BattleTactics::SaveRoutes())
 {LastMessage=TEXT("Player route save requires BattleSaveRoutes=1");return false;}
 // Stage the complete extension before any actor or canonical state is replaced.
 TMap<int32,FPlayerRoute> StagedPlayerRoutes;
 for(const auto& RouteEntry:Save->SavedPlayerRoutes)
 {
  FPlayerRoute Route;
  Route.Lane=RouteEntry.Lane;Route.Phase=RouteEntry.Phase;Route.PhaseGoal=RouteEntry.PhaseGoal;
  Route.bMission=RouteEntry.bMission;Route.Order=EBattleOrder(RouteEntry.Order);Route.Goal=RouteEntry.Goal;Route.Step=RouteEntry.Step;
  Route.bPath=RouteEntry.bPath;Route.Waypoints=RouteEntry.Waypoints;Route.StepDeadline=float(double(Save->Elapsed)+RouteEntry.DeadlineRemaining);
  StagedPlayerRoutes.Add(RouteEntry.SquadID,MoveTemp(Route));
 }
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
 // Legacy saves still discard route choices and leave the old update clock unchanged.
 if(Save->PlayerRoutesVersion==1){PlayerRoutes=MoveTemp(StagedPlayerRoutes);PlayerRouteClock=Save->SavedPlayerRouteClock;}
 else PlayerRoutes.Reset();
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
   G.ArrivedAt=float(double(Elapsed)-E.ArrivalAge);G.Extensions=E.Extensions;
   G.Kind=E.Kind;G.SurpriseRow=E.SurpriseRow;G.SurpriseBridgeStage=E.SurpriseBridgeStage;G.SurpriseBridgeLeader=E.SurpriseBridgeLeader;
   G.SurpriseBridgeAt=float(double(Elapsed)-E.SurpriseBridgeAge);G.SurpriseNextCol=E.SurpriseNextCol;
   G.FrontHold=E.FrontHold;G.FrontGap=E.FrontGap;G.FrontNeed=E.FrontNeed;G.FrontReleaseAt=float(double(Elapsed)-E.FrontReleaseAge);G.FrontExcessSince=E.FrontExcessAge<0.?-1.f:float(double(Elapsed)-E.FrontExcessAge);G.bFrontDrawn=E.bFrontDrawn;G.FrontBackAt=float(double(Elapsed)-E.FrontBackAge);G.FrontStepAt=float(double(Elapsed)-E.FrontStepAge);
   G.SurpriseBridgeEntry=E.SurpriseBridgeEntry;G.SurpriseBridgeExit=E.SurpriseBridgeExit;G.SurpriseBridgeGoal=E.SurpriseBridgeGoal;
   G.SurpriseWaitAt=E.bSurpriseWaiting?float(double(Elapsed)-E.SurpriseWaitAge):-1.f;
   G.SurpriseLogAt=float(double(Elapsed)+E.SurpriseLogRemaining);G.SurpriseEndAt=float(double(Elapsed)+E.SurpriseEndRemaining);
   RestoredGroups.Add(Pair.Key,MoveTemp(G));
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
 FireReveals[0].Reset();FireReveals[1].Reset();
 if(BattleTactics::ArtilleryPosts())GenerateArtilleryPosts();
 ClampFrontTimers();
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
 ++RestoreGeneration;
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
 TArray<uint8> SaveBytes;
 if(!UGameplayStatics::SaveGameToMemory(Save,SaveBytes)){LastMessage=TEXT("Could not write save file");return false;}
 if(SaveBytes.Num()>BattleSaveValidation::MaxBytes){LastMessage=TEXT("Save file exceeds size limit");return false;}
 const bool Saved=UGameplayStatics::SaveDataToSlot(SaveBytes,Actual,0);
 LastMessage=Saved?TEXT("Match saved [F9 to load]"):TEXT("Could not write save file");return Saved;
}
bool UBattleMatch::LoadMatch(const FString& Slot)
{
 const FString Actual=CompatibleSlot(GetWorld()->GetSubsystem<UBattleLeaderRuntime>(),Slot);
 if(!IsEnabled()||Actual.IsEmpty()){LastMessage=TEXT("Unsupported save mode or slot");return false;}
 TArray<uint8> SaveBytes;
 if(!UGameplayStatics::LoadDataFromSlot(SaveBytes,Actual,0)){LastMessage=TEXT("No compatible save found");return false;}
 auto* Loaded=Cast<UBattleSave>(BattleSaveValidation::Decode(SaveBytes,LastMessage));
 if(!Loaded){if(LastMessage.IsEmpty())LastMessage=TEXT("No compatible save found");return false;}
 if(PerProcessSmokeSlot(Slot))UGameplayStatics::DeleteGameInSlot(Actual,0);
 return RestoreSnapshot(Loaded);
}
