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
 int32 ObjectiveShots[3]={0,0,0};
 double ObjectiveDamage[3]={0,0,0},ObjectiveFirstShot[3]={-1,-1,-1};
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
 double AreaPointSeconds[3][2]={{0,0},{0,0},{0,0}};
 double FirstCapture[2]={-1,-1};
 void Cover(const TCHAR* Key){if(Enabled)++CoverDiag.FindOrAdd(FName(Key));}
};
