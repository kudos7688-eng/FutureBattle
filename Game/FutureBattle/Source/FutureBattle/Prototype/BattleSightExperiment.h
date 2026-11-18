#pragma once
#include "CoreMinimal.h"

struct FBattleSightExperiment
{
 bool Enabled=false, Started=false, Finished=false;
 bool SaveCheckDone=false, SaveCheckPassed=false;
 FString Output;
 double StartWall=0, NextSample=0, FirstSight=-1, FirstSightDistance=-1, FirstShot=-1, FirstShotDistance=-1;
 double NextProgress=20, LastSample=0;
 double VisibleOutsideSeconds=0, SharedOutsideSeconds=0, MemoryOutsideSeconds=0;
 int32 Shots=0, Hits=0, SharedSamples=0;
 bool FlankTrial=false;
 double NextFlankSample=0;
 int32 TeamShots[2]={0,0},TeamKills[2]={0,0};
 double TeamDamage[2]={0,0},PointSeconds[2]={0,0};
 // Per objective (three on maps A-D, five on map E; sized for up to MaxObjectives).
 static constexpr int32 MaxObjectives=12;
 int32 ObjectiveShots[MaxObjectives]={};
 double ObjectiveDamage[MaxObjectives]={},ObjectiveFirstShot[MaxObjectives]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
 TArray<double> ShotDistances, PathMS;
 double MaxPathLength=0;
 TSet<int32> PreviouslySeeing;
 TArray<double> EngagementDistances; // Evaluation-only cover action funnel (unit-frames, requests, outcomes). Never read by AI decisions.
 TMap<FName,int32> CoverDiag;
 // Evaluation only: when each squad last lost its visible target (squad_target_* keys in CoverDiag).
 TMap<int32,double> SquadTargetLostAt;
 // Evaluation only: living soldiers within 100m (2D) of any objective, person-seconds by team (5Hz samples).
 double FrontPersonSeconds[2]={0,0};
 // Evaluation only: shot hits/deaths split by leader and follower, indexed by victim team.
 int32 LeaderHits[2]={0,0},FollowerHits[2]={0,0},LeaderCombatDeaths[2]={0,0},FollowerCombatDeaths[2]={0,0},LeaderLossFollowers[2]={0,0};
 int32 LeaderDeathsInCover=0;
 TArray<int32> LeaderDeathRanks;
 // Evaluation only: match time of each Area new-squad purchase per team.
 TArray<double> HireTimes[2];
 // Evaluation only (Area): true-owner seconds per central point and team, and the first capture time per team (-1 = never).
 double AreaPointSeconds[MaxObjectives][2]={};
 double FirstCapture[2]={-1,-1};
 // Evaluation only (Area): living person-seconds within weapon range / 100m (2D) of the ENEMY home, by attacking team (5Hz samples).
 double EnemyHomeRangeSeconds[2]={0,0},EnemyHomeNearSeconds[2]={0,0};
 // Evaluation only (Area, S1): living person-seconds of squads whose S1 slot is the home assault, by team and by
 // progress from own home (0) to enemy home (1) in tenths (squad mean position), and by squad state:
 // [0..6] = EBattleSquadTactic, 7 = home supply trip, 8 = no mission. MaxProgress = furthest squad mean reached.
 double BaseAssaultProgress[2][10]={},BaseAssaultState[2][9]={},BaseAssaultMaxProgress[2]={0,0};
 // Evaluation only (Area): economy at each full minute 1..10 per team: [0] resources held, [1] cumulative income,
 // [2] cumulative new-squad spend, [3] living soldiers. Replacement spend = 100 + income - held - squad spend.
 double EconTimeline[2][10][4]={};int32 NextEconMinute=1;
 // Evaluation only (Area, strategy layer stage 1): UBattleMatch::AreaFronts every 30 s (samples 1..20) per team and lane (objective
 // index): [0] own front, [1] most advanced visible enemy, [2] own soldiers, [3] visible enemies.
 float FrontTimeline[2][20][MaxObjectives][4]={};int32 NextFrontSample=1;
 // Evaluation only (squad types, 2026-11-16): per team and squad type (0 rifle, 1 heavy): living person-seconds, person-seconds
 // within 100 m of an objective, shots, damage dealt, kills, damage taken, deaths. Squad type is that of the soldier's squad.
 static constexpr int32 MaxSquadTypes=9; // BattleSquadType::Count (checked in BattleSightExperiment.cpp)
 double TypePersonSec[2][MaxSquadTypes]={},TypePointSec[2][MaxSquadTypes]={},TypeDamage[2][MaxSquadTypes]={},TypeTaken[2][MaxSquadTypes]={}; int32 TypeShots[2][MaxSquadTypes]={},TypeKills[2][MaxSquadTypes]={},TypeDeaths[2][MaxSquadTypes]={};
 void Cover(const TCHAR* Key){if(Enabled)++CoverDiag.FindOrAdd(FName(Key));}
};
