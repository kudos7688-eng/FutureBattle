#include "BattleLeaderRuntime.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "BattleMetrics.h"
#include "BattlePerception.h"
#include "BattleSquadTactics.h"
#include "BattleSave.h"
#include "BattleFieldLayout.h"
#include "BattleTactics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::StepSightExperiment(float Dt)
{
 if(!SightExperiment.Enabled || SightExperiment.Finished) return;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* Metrics=GetWorld()->GetSubsystem<UBattleMetrics>();
 auto& E=SightExperiment;
 if(!E.Started)
 {
  if(GetWorld()->GetTimeSeconds()<10) return;
  auto* PC=Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
  PC->SelectAll();PC->DelegateSelected();PC->SetSelection(Actor(1));
  if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())) {Camera->SetActorLocation(FVector::ZeroVector);Camera->Zoom(BattleFieldLayout::IsExpanded(GetWorld())?130000:100000);}
  Mode->bCombatPaused=false;Mode->CombatStartsAt=0;
  E.Started=true;E.StartWall=FPlatformTime::Seconds();Metrics->Start();
  E.FlankTrial=FParse::Param(FCommandLine::Get(),TEXT("BattleFlankTrial"));
  UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_EXPERIMENT_START output=%s"),*E.Output);
 }
 if(!E.SaveCheckDone && Match->Elapsed>=80 && FParse::Param(FCommandLine::Get(),TEXT("BattleVerifyMixedSave")))
 {
  E.SaveCheckDone=true;auto* Before=Match->CaptureSnapshot();FString Error;
  bool OK=Before&&Before->Validate(Error)&&Match->SaveMatch(TEXT("FutureBattleSmokeOnly"))&&Match->LoadMatch(TEXT("FutureBattleSmokeOnly"));
  if(OK)
  {
   auto* After=Match->CaptureSnapshot();OK=After&&After->Validate(Error)&&After->MatchID==Before->MatchID&&After->Units.Num()==Before->Units.Num()&&After->SavedPlans.Num()==Before->SavedPlans.Num();
   if(OK&&Match->bArea)OK=After->AllyCredits==Before->AllyCredits&&After->EnemyCredits==Before->EnemyCredits&&After->Area.BaseHP==Before->Area.BaseHP&&After->Area.KnownOwners==Before->Area.KnownOwners&&After->Area.Replacements.OrderIndependentCompareEqual(Before->Area.Replacements)&&After->Area.HireSpent==Before->Area.HireSpent&&After->Area.Income==Before->Area.Income&&After->Area.IncomeClock==Before->Area.IncomeClock&&After->Area.LastAIHireAt==Before->Area.LastAIHireAt&&After->Squads.Num()==Before->Squads.Num();
   if(OK)for(int32 I=0;I<Before->Units.Num();++I)
   {
    const auto& A=Before->Units[I];const auto& B=After->Units[I];
    OK&=A.State.UnitID==B.State.UnitID&&A.State.HP==B.State.HP&&A.State.Ammo==B.State.Ammo&&A.State.Reserve==B.State.Reserve&&A.State.bActorRepresentation==B.State.bActorRepresentation&&A.Position.Equals(B.Position,.01);
   }
   if(OK)for(const auto& P:Before->SavedPlans)
   {
    const auto* Q=After->SavedPlans.Find(P.Key);
    OK&=Q&&Q->Action==P.Value.Action&&Q->Age==P.Value.Age&&Q->SupportSquad==P.Value.SupportSquad&&Q->FlankTarget==P.Value.FlankTarget&&Q->FlankSupport==P.Value.FlankSupport&&Q->FlankSupportChanges==P.Value.FlankSupportChanges;
   }
  }
  E.SaveCheckPassed=OK;
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_MIXED_SAVE_SOAK_RESULT failures=%d seconds=%.2f validation=%s message=%s"),OK?0:1,Match->Elapsed,*Error,*Match->LastMessage);
  if(!OK){E.Finished=true;FPlatformMisc::RequestExitWithStatus(false,1);return;}
 }
 Metrics->BeginFrame();
 if(Match->Elapsed>=E.NextProgress){UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_EXPERIMENT_PROGRESS seconds=%.1f shots=%d score=%.1f/%.1f"),Match->Elapsed,E.Shots,Match->Score[0],Match->Score[1]);E.NextProgress+=20;}
 // Observation aggregates are sampled at 5Hz and cost is included in recorded frames.
 if(Match->Elapsed>=E.NextSample)
 {
  const double SampleDt=E.NextSample==0?0:Match->Elapsed-E.LastSample;
  E.LastSample=Match->Elapsed;
  E.NextSample=Match->Elapsed+.2;
  if(E.FlankTrial)
  {
   for(const auto& Point:Match->Objectives)if(Point.Owner>=0 && Point.Owner<2)E.PointSeconds[Point.Owner]+=SampleDt;
   if(Match->Elapsed>=E.NextFlankSample || Match->Phase==EBattleMatchPhase::Finished)
   {
    E.NextFlankSample=Match->Elapsed+1;
    for(const auto& Pair:Store->GetSquads())
    {
     const auto& S=Pair.Value;int32 Living=0,OnPoint=0;double HP=0;
     for(int32 ID:S.Members)if(const auto* U=Store->Find(ID);U && U->IsAlive())
     {
      ++Living;HP+=U->HP;
      for(const auto& Point:Match->Objectives)if(FMath::Abs(Position(ID).Z-Point.Position.Z)<250 && FVector::Dist2D(Position(ID),Point.Position)<=450){++OnPoint;break;}
     }
     const auto Pos=Position(S.LeaderUnitID);
     UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_FLANK_STATE t=%.3f squad=%d team=%d action=%d living=%d hp=%.1f point=%d x=%.1f y=%.1f"),Match->Elapsed,Pair.Key,S.Team,int32(S.TacticalPlan.Action),Living,HP,OnPoint,Pos.X,Pos.Y);
    }
   }
  }
  while(Match->bArea&&E.NextFrontSample<=20&&Match->Elapsed>=E.NextFrontSample*30.0-0.05)
  {
   const int32 S=E.NextFrontSample-1;
   for(int32 T=0;T<2;++T){FAreaFront F[FAreaIntent::MaxObjectives];Match->AreaFronts(T,F);for(int32 L=0;L<Match->Objectives.Num()&&L<FBattleSightExperiment::MaxObjectives;++L){E.FrontTimeline[T][S][L][0]=F[L].OwnFront;E.FrontTimeline[T][S][L][1]=F[L].EnemyFront;E.FrontTimeline[T][S][L][2]=F[L].Own;E.FrontTimeline[T][S][L][3]=F[L].EnemySeen;}}
   ++E.NextFrontSample;
  }
  while(Match->bArea&&E.NextEconMinute<=10&&Match->Elapsed>=E.NextEconMinute*60.0-0.05)
  {
   const int32 M=E.NextEconMinute-1;int32 Living[2]={0,0};
   for(const auto& U:Store->GetUnits())if(U.Value.IsAlive()&&U.Value.Team>=0&&U.Value.Team<2)++Living[U.Value.Team];
   for(int32 T=0;T<2;++T){E.EconTimeline[T][M][0]=Match->Credits[T];E.EconTimeline[T][M][1]=Match->Area.Income.IsValidIndex(T)?Match->Area.Income[T]:0;E.EconTimeline[T][M][2]=Match->Area.HireSpent.IsValidIndex(T)?Match->Area.HireSpent[T]:0;E.EconTimeline[T][M][3]=Living[T];}
   ++E.NextEconMinute;
  }
  if(Match->bArea)for(int32 I=0;I<Match->Objectives.Num()&&I<FBattleSightExperiment::MaxObjectives;++I)if(const int32 Owner=Match->Objectives[I].Owner;Owner>=0&&Owner<2)
  {E.AreaPointSeconds[I][Owner]+=SampleDt;if(E.FirstCapture[Owner]<0)E.FirstCapture[Owner]=Match->Elapsed;}
  if(SampleDt>0)for(const auto& Pair:Store->GetUnits()) if(Pair.Value.IsAlive() && Pair.Value.Team>=0 && Pair.Value.Team<2)
  {
   const FVector P=Position(Pair.Key);
   for(const auto& Point:Match->Objectives) if(FVector::DistSquared2D(P,Point.Position)<=FMath::Square(10000.)){E.FrontPersonSeconds[Pair.Value.Team]+=SampleDt;break;}
   if(Match->bArea)
   {
    const double D2=FVector::DistSquared2D(P,Match->AreaHome(1-Pair.Value.Team));
    if(D2<=FMath::Square(double(GetDefault<UBattleTuning>()->EffectiveWeaponRange())))E.EnemyHomeRangeSeconds[Pair.Value.Team]+=SampleDt;
    if(D2<=FMath::Square(10000.))E.EnemyHomeNearSeconds[Pair.Value.Team]+=SampleDt;
   }
  }
  if(SampleDt>0&&Match->bArea&&BattleTactics::AreaStrategy().bEnabled)
  {
   const auto& Trips=GetWorld()->GetSubsystem<UBattleSquadTactics>()->OpenSupplyTrips();
   for(const auto& Pair:Store->GetSquads())
   {
    const auto& S=Pair.Value;if(S.Team<0||S.Team>1)continue;
    bool bBase=false;Match->AreaStrategyTarget(Pair.Key,S.Team,bBase);if(!bBase)continue;
    int32 Living=0;FVector Sum=FVector::ZeroVector;
    for(int32 ID:S.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()){++Living;Sum+=Position(ID);}
    if(!Living)continue;
    const FVector H0=Match->AreaHome(S.Team),Axis=Match->AreaHome(1-S.Team)-H0;
    const double Progress=FMath::Clamp(FVector::DotProduct(Sum/Living-H0,Axis)/FMath::Max(1.,Axis.SizeSquared()),0.,1.);
    const double W=SampleDt*Living;
    E.BaseAssaultProgress[S.Team][FMath::Min(9,int32(Progress*10))]+=W;
    E.BaseAssaultMaxProgress[S.Team]=FMath::Max(E.BaseAssaultMaxProgress[S.Team],Progress);
    E.BaseAssaultState[S.Team][Trips.Contains(Pair.Key)?7:!S.bHasMission?8:FMath::Min(6,int32(S.TacticalPlan.Action))]+=W;
   }
  }
  auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
  TSet<int32> Seeing;
  for(const auto& Pair:Store->GetUnits()) if(Pair.Value.IsAlive())
  {
   bool DirectOutside=false,SharedOutside=false,MemoryOutside=false;
   double Nearest=DBL_MAX;
   if(const auto* Contacts=Sense->GetContacts(Pair.Key)) for(const auto& C:*Contacts)
   {
    const double Distance=FVector::Dist(Position(Pair.Key),C.Value.LastKnownPosition);
    if(C.Value.bVisible && !C.Value.bShared)
    {
     Seeing.Add(Pair.Key);Nearest=FMath::Min(Nearest,Distance);
     if(E.FirstSight<0){E.FirstSight=Match->Elapsed;E.FirstSightDistance=Distance;}
     DirectOutside |= Distance>GetDefault<UBattleTuning>()->EffectiveWeaponRange() && Distance<=Pair.Value.Perception.SightRange;
    }
    else if(C.Value.bShared){++E.SharedSamples;SharedOutside |= Distance>GetDefault<UBattleTuning>()->EffectiveWeaponRange() && Distance<=Pair.Value.Perception.SightRange;}
    else MemoryOutside |= Distance>GetDefault<UBattleTuning>()->EffectiveWeaponRange() && Distance<=Pair.Value.Perception.SightRange;
   }
   if(Seeing.Contains(Pair.Key) && !E.PreviouslySeeing.Contains(Pair.Key)) E.EngagementDistances.Add(Nearest);
   E.VisibleOutsideSeconds+=DirectOutside?SampleDt:0;E.SharedOutsideSeconds+=SharedOutside?SampleDt:0;E.MemoryOutsideSeconds+=MemoryOutside?SampleDt:0;
  }
  E.PreviouslySeeing=MoveTemp(Seeing);
 }
 if(Match->Phase!=EBattleMatchPhase::Finished) return;
 E.Finished=true;Mode->bCombatPaused=true;
 auto Root=Metrics->Finish();
 Root->SetNumberField(TEXT("winner"),Match->Winner);Root->SetStringField(TEXT("reason"),Mode->Result);
 Root->SetNumberField(TEXT("match_seconds"),Match->Elapsed);
 Root->SetBoolField(TEXT("mixed_save_round_trip_verified"),E.SaveCheckPassed);
 Root->SetNumberField(TEXT("ally_score"),Match->Score[0]);Root->SetNumberField(TEXT("enemy_score"),Match->Score[1]);
 TArray<TSharedPtr<FJsonValue>> PointStates;
 for(const auto& Point:Match->Objectives)
 {
  auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("owner"),Point.Owner);
  State->SetNumberField(TEXT("capturing_team"),Point.CapturingTeam);State->SetNumberField(TEXT("capture_progress"),Point.CaptureProgress);
  const int32 I=PointStates.Num();State->SetNumberField(TEXT("shots_within_100m"),E.ObjectiveShots[I]);State->SetNumberField(TEXT("damage_from_shots_within_100m"),E.ObjectiveDamage[I]);State->SetNumberField(TEXT("first_shot_seconds"),E.ObjectiveFirstShot[I]);
  PointStates.Add(MakeShared<FJsonValueObject>(State));
 }
 Root->SetArrayField(TEXT("final_objectives"),PointStates);
 Root->SetStringField(TEXT("ruleset"),SnapshotMode().ToString());
 if(E.FlankTrial)for(int32 Team=0;Team<2;++Team)
 {
  auto Outcome=MakeShared<FJsonObject>();Outcome->SetNumberField(TEXT("shots"),E.TeamShots[Team]);Outcome->SetNumberField(TEXT("damage"),E.TeamDamage[Team]);
  Outcome->SetNumberField(TEXT("direct_kills"),E.TeamKills[Team]);Outcome->SetNumberField(TEXT("owned_point_seconds"),E.PointSeconds[Team]);
  Root->SetObjectField(Team?TEXT("flank_trial_team1"):TEXT("flank_trial_team0"),Outcome);
 }
 if(Match->bArea)
 {
  Root->SetNumberField(TEXT("ally_resources"),Match->Credits[0]);Root->SetNumberField(TEXT("enemy_resources"),Match->Credits[1]);
  Root->SetNumberField(TEXT("ally_base_hp"),Match->Area.BaseHP[0]);Root->SetNumberField(TEXT("enemy_base_hp"),Match->Area.BaseHP[1]);
  Root->SetNumberField(TEXT("pending_replacements"),Match->Area.Replacements.Num());
  int32 Retired=0;for(const auto& U:Store->GetUnits())Retired+=U.Value.bRetired;Root->SetNumberField(TEXT("completed_replacements"),Retired);
  for(int32 Team=0;Team<2;++Team)
  {
   TArray<TSharedPtr<FJsonValue>> Times;for(double T:E.HireTimes[Team])Times.Add(MakeShared<FJsonValueNumber>(T));
   Root->SetNumberField(Team?TEXT("enemy_hired_squads"):TEXT("ally_hired_squads"),E.HireTimes[Team].Num());
   Root->SetArrayField(Team?TEXT("enemy_hire_times"):TEXT("ally_hire_times"),Times);
   Root->SetNumberField(Team?TEXT("enemy_hire_spent"):TEXT("ally_hire_spent"),Match->Area.HireSpent.IsValidIndex(Team)?Match->Area.HireSpent[Team]:0);
   Root->SetNumberField(Team?TEXT("enemy_active_squads"):TEXT("ally_active_squads"),Match->ActiveSquads(Team));
   Root->SetNumberField(Team?TEXT("enemy_income"):TEXT("ally_income"),Match->Area.Income.IsValidIndex(Team)?Match->Area.Income[Team]:0);
   // War score trial (0 when the trial is off).
   Root->SetNumberField(Team?TEXT("enemy_war_point"):TEXT("ally_war_point"),Match->Area.WarPoint.IsValidIndex(Team)?Match->Area.WarPoint[Team]:0);
   Root->SetNumberField(Team?TEXT("enemy_war_loss"):TEXT("ally_war_loss"),Match->Area.WarLoss.IsValidIndex(Team)?Match->Area.WarLoss[Team]:0);
   Root->SetNumberField(Team?TEXT("enemy_war_estimate_of_opponent"):TEXT("ally_war_estimate_of_opponent"),Match->WarScoreEstimate(Team));
   TArray<TSharedPtr<FJsonValue>> PointSeconds;for(int32 I=0;I<Match->Objectives.Num()&&I<FBattleSightExperiment::MaxObjectives;++I)PointSeconds.Add(MakeShared<FJsonValueNumber>(E.AreaPointSeconds[I][Team]));
   Root->SetArrayField(Team?TEXT("enemy_point_owned_seconds"):TEXT("ally_point_owned_seconds"),PointSeconds);
   Root->SetNumberField(Team?TEXT("enemy_first_capture_seconds"):TEXT("ally_first_capture_seconds"),E.FirstCapture[Team]);
   Root->SetNumberField(Team?TEXT("enemy_at_opponent_home_range_seconds"):TEXT("ally_at_opponent_home_range_seconds"),E.EnemyHomeRangeSeconds[Team]);
   Root->SetNumberField(Team?TEXT("enemy_near_opponent_home_seconds"):TEXT("ally_near_opponent_home_seconds"),E.EnemyHomeNearSeconds[Team]);
   TArray<TSharedPtr<FJsonValue>> Progress,States;
   for(double V:E.BaseAssaultProgress[Team])Progress.Add(MakeShared<FJsonValueNumber>(V));
   for(double V:E.BaseAssaultState[Team])States.Add(MakeShared<FJsonValueNumber>(V));
   Root->SetArrayField(Team?TEXT("enemy_base_assault_progress_seconds"):TEXT("ally_base_assault_progress_seconds"),Progress);
   Root->SetArrayField(Team?TEXT("enemy_base_assault_state_seconds"):TEXT("ally_base_assault_state_seconds"),States);
   Root->SetNumberField(Team?TEXT("enemy_base_assault_max_progress"):TEXT("ally_base_assault_max_progress"),E.BaseAssaultMaxProgress[Team]);
   TArray<TSharedPtr<FJsonValue>> Econ;
   for(int32 M=0;M<10;++M){TArray<TSharedPtr<FJsonValue>> Row;for(double V:E.EconTimeline[Team][M])Row.Add(MakeShared<FJsonValueNumber>(V));Econ.Add(MakeShared<FJsonValueArray>(Row));}
   Root->SetArrayField(Team?TEXT("enemy_economy_per_minute"):TEXT("ally_economy_per_minute"),Econ);
   TArray<TSharedPtr<FJsonValue>> Front;
   for(int32 S=0;S<20;++S){TArray<TSharedPtr<FJsonValue>> Row;for(int32 L=0;L<Match->Objectives.Num()&&L<FBattleSightExperiment::MaxObjectives;++L)for(float V:E.FrontTimeline[Team][S][L])Row.Add(MakeShared<FJsonValueNumber>(V));Front.Add(MakeShared<FJsonValueArray>(Row));}
   Root->SetArrayField(Team?TEXT("enemy_front_per_30s"):TEXT("ally_front_per_30s"),Front);
  }
 }
 const auto* Tactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();
 auto Events=MakeShared<FJsonObject>();for(const auto& Pair:Tactics->EventCounts())Events->SetNumberField(Pair.Key.ToString(),Pair.Value);
 auto Durations=MakeShared<FJsonObject>();for(const auto& Pair:Tactics->ActionDurations())Durations->SetNumberField(Pair.Key.ToString(),Pair.Value);
 Root->SetObjectField(TEXT("tactical_events"),Events);Root->SetObjectField(TEXT("tactical_squad_seconds"),Durations);
 auto CoverDiag=MakeShared<FJsonObject>();for(const auto& Pair:E.CoverDiag)CoverDiag->SetNumberField(Pair.Key.ToString(),Pair.Value);
 Root->SetObjectField(TEXT("cover_diagnostics"),CoverDiag);
 Root->SetStringField(TEXT("cover_diagnostics_scope"),TEXT("Whole match including pre-save time; ai_* are AI unit-frames in Follow; request_* are per RequestCoverAction call by furthest stage; peek_* are per rejected peek offset; cancel_* are cover endings"));
 Root->SetStringField(TEXT("tactical_time_scope"),TEXT("1Hz squad samples; active delegated AI only; not per-soldier or exact transition timestamps"));
 Root->SetNumberField(TEXT("first_sight_seconds"),E.FirstSight);Root->SetNumberField(TEXT("first_sight_cm"),E.FirstSightDistance);
 Root->SetNumberField(TEXT("first_shot_seconds"),E.FirstShot);Root->SetNumberField(TEXT("first_shot_cm"),E.FirstShotDistance);
 Root->SetNumberField(TEXT("shots"),E.Shots);Root->SetNumberField(TEXT("physical_unit_hits"),E.Hits);
 Root->SetNumberField(TEXT("visible_outside_person_seconds"),E.VisibleOutsideSeconds);
 Root->SetNumberField(TEXT("shared_outside_person_seconds"),E.SharedOutsideSeconds);
 Root->SetNumberField(TEXT("memory_outside_person_seconds"),E.MemoryOutsideSeconds);
 Root->SetNumberField(TEXT("shared_contact_samples"),E.SharedSamples);
 Root->SetStringField(TEXT("observation_scope"),TEXT("5Hz sampling; person-seconds, memory/shared separated; headless frame timing is not rendered performance"));
 auto Array=[&](const TCHAR* Name,const TArray<double>& Values){TArray<TSharedPtr<FJsonValue>> A;for(double V:Values)A.Add(MakeShared<FJsonValueNumber>(V));Root->SetArrayField(Name,A);};
 Array(TEXT("shot_distances_cm"),E.ShotDistances);Array(TEXT("engagement_distances_cm"),E.EngagementDistances);Array(TEXT("path_query_ms"),E.PathMS);
 Root->SetNumberField(TEXT("max_path_cm"),E.MaxPathLength);
 {
  auto Survival=MakeShared<FJsonObject>();
  for(int32 Team=0;Team<2;++Team)
  {
   auto TeamObject=MakeShared<FJsonObject>();TeamObject->SetNumberField(TEXT("leader_hits"),E.LeaderHits[Team]);TeamObject->SetNumberField(TEXT("follower_hits"),E.FollowerHits[Team]);
   TeamObject->SetNumberField(TEXT("leader_combat_deaths"),E.LeaderCombatDeaths[Team]);TeamObject->SetNumberField(TEXT("follower_combat_deaths"),E.FollowerCombatDeaths[Team]);
   TeamObject->SetNumberField(TEXT("leader_loss_followers"),E.LeaderLossFollowers[Team]);
   Survival->SetObjectField(Team?TEXT("team1"):TEXT("team0"),TeamObject);
  }
  TArray<TSharedPtr<FJsonValue>> Ranks;for(int32 R:E.LeaderDeathRanks)Ranks.Add(MakeShared<FJsonValueNumber>(R));
  Survival->SetArrayField(TEXT("leader_death_front_ranks"),Ranks);Survival->SetNumberField(TEXT("leader_deaths_in_cover"),E.LeaderDeathsInCover);
  Survival->SetBoolField(TEXT("leader_guard"),BattleTactics::LeaderGuardEnabled());
  Survival->SetNumberField(TEXT("leader_health_multiplier"),BattleTactics::LeaderHealthMultiplier());
  Survival->SetStringField(TEXT("scope"),TEXT("Unit shots only, by victim team, from experiment start (save/load does not reset); rank 0 = living squad member nearest the shooter"));
  Root->SetObjectField(TEXT("leader_survival"),Survival);
 }
 {
  // Evaluation only: home supply trips (Area) and front-line presence.
  const auto* SupplyTactics=GetWorld()->GetSubsystem<UBattleSquadTactics>();const auto& Sup=BattleTactics::Supply();
  auto Supply=MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> TripSeconds;for(double D:SupplyTactics->CompletedSupplyTripSeconds())TripSeconds.Add(MakeShared<FJsonValueNumber>(D));
  Supply->SetArrayField(TEXT("resumed_trip_seconds"),TripSeconds);
  int32 LeaderLost=0,InProgress=0,Other=0;
  for(const auto& Trip:SupplyTactics->OpenSupplyTrips())
  {
   const auto* S=Store->FindSquad(Trip.Key);
   if(!S||S->CommandStatus!=EBattleCommandStatus::Active)++LeaderLost;
   else if(S->TacticalPlan.Action==EBattleSquadTactic::Recover&&S->TacticalPlan.Reason.StartsWith(TEXT("HOME SUPPLY")))++InProgress;
   else ++Other;
  }
  Supply->SetNumberField(TEXT("open_trips_leader_lost"),LeaderLost);Supply->SetNumberField(TEXT("open_trips_in_progress_at_end"),InProgress);
  Supply->SetNumberField(TEXT("open_trips_replaced_by_other_plan"),Other);
  Supply->SetNumberField(TEXT("front_person_seconds_team0"),E.FrontPersonSeconds[0]);Supply->SetNumberField(TEXT("front_person_seconds_team1"),E.FrontPersonSeconds[1]);
  Supply->SetNumberField(TEXT("return_living"),Sup.ReturnLiving);Supply->SetNumberField(TEXT("resume_living"),Sup.ResumeLiving);
  Supply->SetNumberField(TEXT("away_ratio"),Sup.AwayRatio);Supply->SetNumberField(TEXT("heal_per_second"),Sup.HealPerSecond);Supply->SetNumberField(TEXT("resume_health_ratio"),Sup.ResumeHealthRatio);
  Supply->SetStringField(TEXT("scope"),TEXT("Trips timed on the tactical clock from HomeSupplyReturn to HomeSupplyResume; emergency aborts are tactical_events.HomeSupplyAbortEmergency; trips begun before a save/load lose their start (HomeSupplyEndWithoutStart); front = living soldiers within 100m 2D of any objective, 5Hz"));
  Root->SetObjectField(TEXT("supply_cycle"),Supply);
 }
 for(int32 Team=0;Team<2;++Team){int32 Alive=0;for(const auto& U:Store->GetUnits()) if(U.Value.Team==Team && U.Value.IsAlive())++Alive;Root->SetNumberField(Team?TEXT("enemy_alive"):TEXT("ally_alive"),Alive);}
 int32 Requests=0,Arrived=0;for(const auto& P:Plans){Requests+=P.Value.PathRequests;for(const auto& G:P.Value.ArrivalGoals)if(Store->Find(G.Key)->IsAlive() && FVector::Dist2D(Position(G.Key),G.Value)<100)++Arrived;}
 Root->SetNumberField(TEXT("path_requests"),Requests);Root->SetNumberField(TEXT("alive_within_1m_current_reservation"),Arrived);
 Root->SetStringField(TEXT("arrival_scope"),TEXT("End snapshot only, not cumulative order success"));
 FString Json;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
 const bool Saved=FFileHelper::SaveStringToFile(Json,*E.Output);
 UE_LOG(LogFutureBattle,Display,TEXT("SIGHT_EXPERIMENT_RESULT failures=%d winner=%d seconds=%.2f shots=%d"),Saved?0:1,Match->Winner,Match->Elapsed,E.Shots);
 if(!IsRunningCommandlet()) FScreenshotRequest::RequestScreenshot(E.Output+TEXT(".png"),true,false);
 FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateWeakLambda(this,[Saved](){FPlatformMisc::RequestExitWithStatus(false,Saved?0:1);}),1.f,false);
}
