// Battle sounds (2026-11-22). Display only: nothing here writes to the battle state, and the variations (which take, pitch)
// come from this file's own random stream, never from the battle's streams.
// Sound waves: TPS Multiplayer Shooter (gunfire, weapon handling), Lyra Starter Game (explosions, impacts, near misses) and
// Setup/Make-SynthSounds.py (launches, incoming shell, match cues). Details in Docs/ASSET_SOURCES_AUDIO_FX.md.
#include "BattleAudio.h"
#include "FutureBattle.h"
#include "BattleController.h"
#include "BattleMatch.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/Package.h"

namespace
{
 enum EBank : uint8 { RifleNear, RifleFar, HeavyNear, HeavyFar, BlastNear, BlastTailNear, BlastFar, BlastTailFar, Thump,
  ImpactSurface, ImpactBody, WhizNear, WhizMid, BankCount };
 enum ESingle : uint8 { SRocket, SMortar, SIncoming, SCaptureFriendly, SCaptureEnemy, SVictory, SDefeat,
  SReloadBegin, SReloadEnd, SDryFire, SAim, SScopeIn, SScopeOut, SingleCount };
 enum EAtt : uint8 { AttGun, AttBlast, AttImpact, AttWhiz, AttLaunch, AttIncoming, AttCount };
 enum EConc : uint8 { ConcPlayer, ConcGun, ConcBlast, ConcImpact, ConcWhiz, ConcLaunch, ConcUI, ConcCount };

 struct FBankDef { const TCHAR* Folder; const TCHAR* Prefix; int32 First, Last; bool bTwoDigits; };
 const TCHAR* const TPSWav=TEXT("/Game/Multiplayercovershooter/sounds/wav");
 const FBankDef BankDefs[BankCount]={
  {TPSWav,TEXT("M4_Designed_Oneshot_"),1,4,false},
  {TPSWav,TEXT("M4_Designed_Dist_Oneshot_"),1,4,false},
  {TPSWav,TEXT("AX50_Designed_Oneshot_"),1,4,false},
  {TPSWav,TEXT("AX50_Designed_Dist_Oneshot_"),1,4,false},
  {TEXT("/Game/Audio/Sounds/Explosions"),TEXT("Explosions_Grenade_Punch-Close_"),1,4,true},
  {TEXT("/Game/Audio/Sounds/Explosions"),TEXT("Explosions_Grenade_Noise-Exterior_"),1,4,true},
  {TEXT("/Game/Audio/Sounds/Explosions"),TEXT("Explosions_Grenade_Punch-Distant_"),1,4,true},
  {TEXT("/Game/Audio/Sounds/Explosions"),TEXT("Explosions_Grenade_Noise-Exterior-Distant_"),1,4,true},
  {TEXT("/Game/Audio/Sounds/Grenade"),TEXT("grenade_explode_thump_"),1,3,true},
  {TEXT("/Game/Audio/SoundWaves/Weapons"),TEXT("SFX_BulletImpact_"),1,8,true},
  {TEXT("/Game/Audio/Sounds/Impacts"),TEXT("Lyra_Plyr_BulletImpact_"),1,6,true},
  {TEXT("/Game/Audio/Sounds/WhizBys"),TEXT("sfx_AR_Whizbys_Oncoming_Close_"),1,4,true},
  {TEXT("/Game/Audio/Sounds/WhizBys"),TEXT("sfx_AR_Whizbys_Oncoming_Mid_"),1,2,true},
 };
 const TCHAR* const SinglePaths[SingleCount]={
  TEXT("/Game/Audio/Battle/SW_Synth_RocketLaunch"),TEXT("/Game/Audio/Battle/SW_Synth_MortarLaunch"),TEXT("/Game/Audio/Battle/SW_Synth_ShellIncoming"),
  TEXT("/Game/Audio/Battle/SW_Synth_CaptureFriendly"),TEXT("/Game/Audio/Battle/SW_Synth_CaptureEnemy"),
  TEXT("/Game/Audio/Battle/SW_Synth_Victory"),TEXT("/Game/Audio/Battle/SW_Synth_Defeat"),
  TEXT("/Game/Multiplayercovershooter/sounds/wav/SW_reload_begin"),TEXT("/Game/Multiplayercovershooter/sounds/wav/SW_reload_end"),
  TEXT("/Game/Multiplayercovershooter/sounds/wav/SW_trigger_empty"),TEXT("/Game/Multiplayercovershooter/sounds/wav/SW_aiming_1"),
  TEXT("/Game/Multiplayercovershooter/sounds/wav/SW_sniper_zoom_in"),TEXT("/Game/Multiplayercovershooter/sounds/wav/SW_sniper_zoom_out"),
 };
 // Distances in cm: full volume within Inner, fading over Falloff; far sounds lose their highs (air absorption).
 struct FAttDef { float Inner, Falloff, FarLowPass; };
 const FAttDef AttDefs[AttCount]={{400,30000,3000},{1500,90000,1200},{150,3000,6000},{100,1200,12000},{500,40000,2500},{600,9000,6000}};
 struct FConcDef { int32 Max; float Retrigger; };
 const FConcDef ConcDefs[ConcCount]={{4,0},{14,0},{12,0},{6,0},{2,.06f},{6,0},{3,0}};

 struct FState
 {
  // Loaded once per process and kept in the root set (raw pointers: nothing to release at exit).
  bool bLoaded=false;
  TArray<USoundBase*> Banks[BankCount];
  int32 LastPick[BankCount];
  USoundBase* Singles[SingleCount]={};
  USoundAttenuation* Att[AttCount]={};
  USoundConcurrency* Conc[ConcCount]={};
  FRandomStream Random{20261122};
  // Per world and per frame.
  TWeakObjectPtr<UWorld> World;
  uint64 Frame=MAX_uint64;
  bool bListener=false;
  FVector Listener=FVector::ZeroVector;
  int32 FarShots=0, Impacts=0, BodyImpacts=0, Blasts=0;
  double LastWhiz=-1.;
  bool bOverride=false;
  // The controlled soldier's weapon handling, and the match as last seen.
  int32 PlayerUnit=INDEX_NONE, LastAim=0;
  bool bLastScope=false, bReloadPending=false, bLastTrigger=false;
  double ReloadEnd=0.;
  TArray<int32> Owners;
  EBattleMatchPhase LastPhase=EBattleMatchPhase::Disabled;
 };
 FState& Get(){static FState S;return S;}

 USoundBase* LoadSound(const FString& Package,int32& Found,int32& Missing)
 {
  const FString Path=Package+TEXT(".")+FPackageName::GetShortName(Package);
  USoundBase* Sound=LoadObject<USoundBase>(nullptr,*Path,{},LOAD_NoWarn|LOAD_Quiet);
  if(Sound){Sound->AddToRoot();++Found;}
  else{++Missing;UE_LOG(LogFutureBattle,Warning,TEXT("BATTLE_AUDIO missing %s"),*Path);}
  return Sound;
 }

 void Load(FState& S)
 {
  if(S.bLoaded)return;
  S.bLoaded=true;
  int32 Found=0,Missing=0;
  for(int32 B=0;B<BankCount;++B)
  {
   const FBankDef& D=BankDefs[B];S.LastPick[B]=INDEX_NONE;
   for(int32 I=D.First;I<=D.Last;++I)
   {
    const FString Name=D.bTwoDigits?FString::Printf(TEXT("%s%02d"),D.Prefix,I):FString::Printf(TEXT("%s%d"),D.Prefix,I);
    if(USoundBase* Sound=LoadSound(FString(D.Folder)+TEXT("/")+Name,Found,Missing))S.Banks[B].Add(Sound);
   }
  }
  for(int32 I=0;I<SingleCount;++I)S.Singles[I]=LoadSound(SinglePaths[I],Found,Missing);
  for(int32 I=0;I<AttCount;++I)
  {
   USoundAttenuation* A=NewObject<USoundAttenuation>(GetTransientPackage());A->AddToRoot();
   FSoundAttenuationSettings& T=A->Attenuation;
   T.bAttenuate=true;T.bSpatialize=true;
   T.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;T.dBAttenuationAtMax=-50.f;
   T.AttenuationShape=EAttenuationShape::Sphere;T.AttenuationShapeExtents=FVector(AttDefs[I].Inner,0.,0.);T.FalloffDistance=AttDefs[I].Falloff;
   T.bAttenuateWithLPF=true;T.LPFRadiusMin=AttDefs[I].Inner;T.LPFRadiusMax=AttDefs[I].Inner+AttDefs[I].Falloff;
   T.LPFFrequencyAtMin=20000.f;T.LPFFrequencyAtMax=AttDefs[I].FarLowPass;
   S.Att[I]=A;
  }
  for(int32 I=0;I<ConcCount;++I)
  {
   USoundConcurrency* C=NewObject<USoundConcurrency>(GetTransientPackage());C->AddToRoot();
   C->Concurrency.MaxCount=ConcDefs[I].Max;C->Concurrency.bLimitToOwner=false;
   C->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopFarthestThenOldest;C->Concurrency.RetriggerTime=ConcDefs[I].Retrigger;
   S.Conc[I]=C;
  }
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_AUDIO loaded=%d missing=%d"),Found,Missing);
 }

 FState* Ready(UWorld* World)
 {
  if(!BattleAudio::Enabled(World))return nullptr;
  FState& S=Get();
  if(S.World.Get()!=World)
  {
   S.World=World;S.Frame=MAX_uint64;S.LastWhiz=-1.;S.bOverride=false;
   S.PlayerUnit=INDEX_NONE;S.Owners.Reset();S.LastPhase=EBattleMatchPhase::Disabled;
  }
  Load(S);
  if(S.Frame!=GFrameCounter)
  {
   S.Frame=GFrameCounter;S.FarShots=S.Impacts=S.BodyImpacts=S.Blasts=0;S.bListener=false;
   if(const APlayerController* PC=World->GetFirstPlayerController()){FVector Front,Right;PC->GetAudioListenerPosition(S.Listener,Front,Right);S.bListener=true;}
  }
  return &S;
 }

 USoundBase* Pick(FState& S,EBank B)
 {
  const TArray<USoundBase*>& List=S.Banks[B];
  if(List.IsEmpty())return nullptr;
  int32 I=S.Random.RandRange(0,List.Num()-1);
  if(List.Num()>1&&I==S.LastPick[B])I=(I+1)%List.Num(); // never the same take twice in a row
  S.LastPick[B]=I;
  return List[I];
 }

 void PlayAt(UWorld* World,const FState& S,USoundBase* Sound,const FVector& At,float Volume,float Pitch,EAtt A,EConc C,float StartTime=0.f)
 {
  if(Sound)UGameplayStatics::PlaySoundAtLocation(World,Sound,At,FRotator::ZeroRotator,Volume,Pitch,StartTime,S.Att[A],S.Conc[C]);
 }

 void Play2D(UWorld* World,const FState& S,USoundBase* Sound,float Volume,EConc C,bool bUI)
 {
  if(Sound)UGameplayStatics::PlaySound2D(World,Sound,Volume,1.f,0.f,S.Conc[C],nullptr,bUI);
 }
}

bool BattleAudio::Enabled(UWorld* World)
{
 static const bool bOff=[]{int32 V=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSFX="),V);return V==0;}();
 return !bOff&&World&&World->IsGameWorld()&&World->bAllowAudioPlayback&&World->GetAudioDeviceRaw()!=nullptr;
}

void BattleAudio::Preload(UWorld* World)
{
 Ready(World);
}

void BattleAudio::Shot(UWorld* World,const FVector& At,EShot Kind,bool bPlayer)
{
 FState* S=Ready(World);
 if(!S||!S->bListener)return;
 const double D=FVector::Dist(At,S->Listener);
 if(D>40000.)return;
 // Hundreds of soldiers may fire in the same frame: beyond 40 m only a few new shots per frame start (and the group keeps
 // the nearest voices). Shots nearby and the player's own are never thinned out.
 if(!bPlayer&&D>4000.&&S->FarShots++>=4)return;
 const bool bHeavy=Kind==EShot::Heavy,bNear=D<6000.;
 const EBank Bank=bHeavy?(bNear?HeavyNear:HeavyFar):(bNear?RifleNear:RifleFar);
 const float Volume=bPlayer?.9f:bHeavy?.75f:.6f;
 const float Pitch=S->Random.FRandRange(.95f,1.05f)*(bHeavy?1.06f:1.f);
 PlayAt(World,*S,Pick(*S,Bank),At,Volume,Pitch,AttGun,bPlayer?ConcPlayer:ConcGun);
}

void BattleAudio::Impact(UWorld* World,const FVector& At,bool bBody)
{
 FState* S=Ready(World);
 if(!S||!S->bListener)return;
 if(FVector::Dist(At,S->Listener)>(bBody?2500.:3500.))return;
 int32& Used=bBody?S->BodyImpacts:S->Impacts;
 if(Used++>=(bBody?2:3))return;
 PlayAt(World,*S,Pick(*S,bBody?ImpactBody:ImpactSurface),At,bBody?.7f:.5f,S->Random.FRandRange(.92f,1.08f),AttImpact,ConcImpact);
}

void BattleAudio::ShotPassing(UWorld* World,const FVector& Start,const FVector& End,int32 ShooterTeam,int32 VictimID)
{
 FState* S=Ready(World);
 if(!S)return;
 const auto* PC=Cast<ABattleController>(World->GetFirstPlayerController());
 const ABattleUnit* Me=PC&&!PC->IsTactical()?Cast<ABattleUnit>(PC->GetPawn()):nullptr;
 if(!Me||!Me->IsAlive())return;
 const FBattleUnitState* State=Me->GetState();
 if(!State||State->Team==ShooterTeam||State->UnitID==VictimID)return;
 const FVector Head=Me->EyePosition();
 const FVector Closest=FMath::ClosestPointOnSegment(Head,Start,End);
 const double Miss=FVector::Dist(Closest,Head);
 // Only a bullet that flies on past the soldier cracks; one that stops short is heard as its impact instead.
 if(Miss>300.||FVector::DistSquared(Closest,End)<FMath::Square(100.))return;
 const double Now=World->GetTimeSeconds();
 if(Now-S->LastWhiz<.07)return;
 S->LastWhiz=Now;
 PlayAt(World,*S,Pick(*S,Miss<120.?WhizNear:WhizMid),Closest,.85f,S->Random.FRandRange(.94f,1.06f),AttWhiz,ConcWhiz);
}

void BattleAudio::Explosion(UWorld* World,const FVector& At,float Size)
{
 FState* S=Ready(World);
 if(!S||!S->bListener)return;
 const double D=FVector::Dist(At,S->Listener);
 if(D>120000.||S->Blasts++>=4)return;
 const bool bNear=D<10000.;
 // Bigger blasts sound deeper and louder.
 const float Pitch=FMath::Clamp(1.12f-.16f*Size,.62f,1.1f)*S->Random.FRandRange(.95f,1.05f);
 const float Volume=FMath::Clamp(.75f+.15f*Size,.8f,1.25f);
 PlayAt(World,*S,Pick(*S,bNear?BlastNear:BlastFar),At,Volume,Pitch,AttBlast,ConcBlast);
 PlayAt(World,*S,Pick(*S,bNear?BlastTailNear:BlastTailFar),At,Volume*.8f,Pitch,AttBlast,ConcBlast);
 if(bNear&&Size>1.5f)PlayAt(World,*S,Pick(*S,Thump),At,Volume*.7f,Pitch*.9f,AttBlast,ConcBlast);
}

void BattleAudio::Incoming(UWorld* World,const FVector& At,float SecondsToImpact)
{
 FState* S=Ready(World);
 if(!S||!S->bListener||FVector::Dist(At,S->Listener)>9000.)return;
 // A shell with less flight time left starts its whistle part-way through, so it still ends at the impact.
 const float Skip=FMath::Clamp(IncomingLead-SecondsToImpact,0.f,IncomingLead-.2f);
 PlayAt(World,*S,S->Singles[SIncoming],At+FVector(0,0,800),.7f,1.f,AttIncoming,ConcLaunch,Skip);
}

void BattleAudio::Cue(UWorld* World,ECue Which,const FVector* At)
{
 FState* S=Ready(World);
 if(!S)return;
 if(At)
 {
  if(!S->bListener)return;
  const double D=FVector::Dist(*At,S->Listener);
  if(D>60000.)return;
  switch(Which)
  {
  case ECue::RocketLaunch:PlayAt(World,*S,S->Singles[SRocket],*At,.9f,S->Random.FRandRange(.95f,1.05f),AttLaunch,ConcLaunch);break;
  case ECue::MortarLaunch:PlayAt(World,*S,S->Singles[SMortar],*At,.9f,S->Random.FRandRange(.95f,1.05f),AttLaunch,ConcLaunch);break;
  case ECue::CannonFire:
   // A gun's report: the explosion punch slowed down, with the thump underneath when near.
   PlayAt(World,*S,Pick(*S,D<10000.?BlastNear:BlastFar),*At,1.1f,S->Random.FRandRange(.6f,.66f),AttBlast,ConcBlast);
   if(D<10000.)PlayAt(World,*S,Pick(*S,Thump),*At,.8f,.8f,AttBlast,ConcBlast);
   break;
  default:break;
  }
  return;
 }
 switch(Which)
 {
 case ECue::CaptureFriendly:Play2D(World,*S,S->Singles[SCaptureFriendly],.55f,ConcUI,true);break;
 case ECue::CaptureEnemy:Play2D(World,*S,S->Singles[SCaptureEnemy],.55f,ConcUI,true);break;
 case ECue::Victory:Play2D(World,*S,S->Singles[SVictory],.6f,ConcUI,true);break;
 case ECue::Defeat:Play2D(World,*S,S->Singles[SDefeat],.6f,ConcUI,true);break;
 default:break;
 }
}

void BattleAudio::TickPlayer(ABattleController* PC,bool bTriggerHeld)
{
 UWorld* World=PC?PC->GetWorld():nullptr;
 FState* S=Ready(World);
 if(!S)return;
 // Command view: listen from above the point the camera looks at (the camera itself may be a few hundred metres up),
 // so the battle in the middle of the screen is what is heard; left and right follow the screen.
 if(PC->IsTactical()&&PC->GetPawn()&&PC->PlayerCameraManager)
 {
  const FVector Focus=PC->GetPawn()->GetActorLocation();
  const float Height=FMath::Clamp(float(PC->PlayerCameraManager->GetCameraLocation().Z-Focus.Z)*.4f,800.f,3000.f);
  PC->SetAudioListenerOverride(nullptr,Focus+FVector(0,0,Height),FRotator(0.f,PC->PlayerCameraManager->GetCameraRotation().Yaw,0.f));
  S->bOverride=true;
 }
 else if(S->bOverride)
 {
  PC->ClearAudioListenerOverride();
  S->bOverride=false;
 }
 // The controlled soldier's weapon handling.
 const ABattleUnit* Unit=PC->IsTactical()?nullptr:Cast<ABattleUnit>(PC->GetPawn());
 const FBattleUnitState* U=Unit&&Unit->IsAlive()?Unit->GetState():nullptr;
 const double Now=World->GetTimeSeconds();
 if(!U)S->PlayerUnit=INDEX_NONE;
 else if(S->PlayerUnit!=U->UnitID)
 {
  // Taking over a soldier: start from what he is doing now, without sounds.
  S->PlayerUnit=U->UnitID;S->LastAim=Unit->GetAimLevel();S->bLastScope=Unit->ScopeView();
  S->ReloadEnd=U->ReloadEndTime;S->bReloadPending=U->ReloadEndTime>Now;
 }
 else
 {
  const int32 Aim=Unit->GetAimLevel();const bool bScope=Unit->ScopeView();
  if(Aim>0&&S->LastAim==0)Play2D(World,*S,S->Singles[SAim],.45f,ConcPlayer,false);
  if(bScope!=S->bLastScope)Play2D(World,*S,S->Singles[bScope?SScopeIn:SScopeOut],.5f,ConcPlayer,false);
  S->LastAim=Aim;S->bLastScope=bScope;
  if(U->ReloadEndTime>Now&&U->ReloadEndTime!=S->ReloadEnd)
  {
   S->ReloadEnd=U->ReloadEndTime;S->bReloadPending=true;
   Play2D(World,*S,S->Singles[SReloadBegin],.6f,ConcPlayer,false);
  }
  if(S->bReloadPending&&Now>=S->ReloadEnd){S->bReloadPending=false;Play2D(World,*S,S->Singles[SReloadEnd],.6f,ConcPlayer,false);}
  if(bTriggerHeld&&!S->bLastTrigger&&U->Ammo<=0)Play2D(World,*S,S->Singles[SDryFire],.6f,ConcPlayer,false);
 }
 S->bLastTrigger=bTriggerHeld;
 // Match cues: an objective taken by us or by the enemy (one cue per frame), then victory or defeat. The player is team 0.
 if(const UBattleMatch* M=World->GetSubsystem<UBattleMatch>())
 {
  constexpr int32 Mine=0;
  if(M->Phase==EBattleMatchPhase::Combat&&S->LastPhase==EBattleMatchPhase::Combat&&S->Owners.Num()==M->Objectives.Num())
   for(int32 I=0;I<M->Objectives.Num();++I)
   {
    const int32 Was=S->Owners[I],Owner=M->Objectives[I].Owner;
    if(Was==Owner)continue;
    if(Owner==Mine){Cue(World,ECue::CaptureFriendly);break;}
    if(Was==Mine||Owner!=INDEX_NONE){Cue(World,ECue::CaptureEnemy);break;}
   }
  S->Owners.SetNum(M->Objectives.Num());
  for(int32 I=0;I<M->Objectives.Num();++I)S->Owners[I]=M->Objectives[I].Owner;
  if(M->Phase==EBattleMatchPhase::Finished&&S->LastPhase==EBattleMatchPhase::Combat&&M->Winner!=INDEX_NONE)
   Cue(World,M->Winner==Mine?ECue::Victory:ECue::Defeat);
  S->LastPhase=M->Phase;
 }
}
