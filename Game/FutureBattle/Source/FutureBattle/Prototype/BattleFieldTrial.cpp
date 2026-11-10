#include "BattleSquadTrial.h"
#include "BattleFieldLayout.h"
#include "BattleGameMode.h"
#include "BattleMatch.h"
#include "BattleTactics.h"
#include "BattleUnit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/HUD.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace
{
	struct FFieldTimer
 {
  double& Sum; bool Enabled; double Start;
  FFieldTimer(double& In, bool Active):Sum(In),Enabled(Active),Start(Active ? FPlatformTime::Seconds() : 0.) {}
  ~FFieldTimer() { if(Enabled) Sum += (FPlatformTime::Seconds()-Start)*1000.; }
 };
 constexpr int32 FieldHz = 30;
	constexpr int32 ScanFrames = 6;
}

bool UBattleSquadTrial::PlanFieldPath(UWorld* World, int32 Index, FVector Goal)
{
	FFieldTimer Measure(DiagFrame[NavTime],bFieldDiagnostics);
	auto& S = Soldiers[Index];
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	for (double Scale : {1., .5, 0.})
	{
		FNavLocation End;
		if (!Nav->ProjectPointToNavigation(Goal+S.Slot*Scale, End, FVector(100,100,400))) continue;
		++Paths;
		auto* Path = Nav->FindPathToLocationSynchronously(World, S.P-FVector(0,0,90), End.Location);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		S.Path = Path->PathPoints;
		S.Waypoint = 1;
		return true;
	}
	++PathFailures;
	S.Path.Reset();
	return false;
}

void UBattleSquadTrial::StepField(ABattleGameMode& Mode)
{
	auto* World = Mode.GetWorld();
	auto* Match = World->GetSubsystem<UBattleMatch>();
	auto* Store = World->GetSubsystem<UBattleState>();
	const auto* Rules = GetDefault<UBattleTuning>();
	if (Frame == -58)
	{
		Match->InitializeMatch();
		auto* PC = World->GetFirstPlayerController();
		auto* Cam = World->SpawnActor<ACameraActor>(BattleFieldLayout::IsLarge(World) ? FVector(0,0,30000) : FVector(-5400,0,7500), BattleFieldLayout::IsLarge(World) ? FRotator(-90,0,0) : FRotator(-54,0,0));
		Cam->GetCameraComponent()->SetFieldOfView(70);
		PC->SetViewTarget(Cam);
		if (PC->GetHUD()) PC->GetHUD()->bShowHUD = false;
	}
	if (Frame == -20 && !bT2aHeadless && !bT2bHeadless) FScreenshotRequest::RequestScreenshot(Capture,false,false);
	if (Frame < 0) { ++Frame; return; }
	const double Time = FPlatformTime::Seconds();
 const bool MeasureCombat = Shots>0 && CombatFrames.Num()<900;
	if (Frame) { Frames.Add((Time-LastFrame)*1000); if (Shots>0 && CombatFrames.Num()<900) CombatFrames.Add((Time-LastFrame)*1000); }
	if (Frame==1800 && !bT2aHeadless && !bT2bHeadless) FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Capture)/FPaths::GetBaseFilename(Capture)+TEXT("-combat.png"),false,false);
	LastFrame = Time;
	if (Frame == 900)
	{
		double Sum = 0;
		for (double D : Frames) Sum += D;
		FPS30 = 900000 / Sum;
		Loss30 = 0;
		for (const auto& S : Soldiers) Loss30 += !S.bAlive;
	}
	if (Match->Phase == EBattleMatchPhase::Finished)
	{
		FinishField(Mode);
		Frame = -100000;
		return;
	}
 double StepStart=0;

 if(bFieldDiagnostics) {
  for(double& D:DiagFrame) D=0;
  StepStart=FPlatformTime::Seconds();
  DiagFrame[GameTime]=FPlatformTime::ToMilliseconds(GGameThreadTime);
  DiagFrame[RenderTime]=FPlatformTime::ToMilliseconds(GRenderThreadTime);
  DiagFrame[RHITime]=FPlatformTime::ToMilliseconds(GRHIThreadTime);
  DiagFrame[GPUTime]=FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
 }
	if (!Frame)
	{
		auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FRandomStream Layout(Seed);
		FieldSquads.SetNum(Members.Num()/5);
  if(bT2aMetrics) T2aWaits.SetNum(Members.Num());
  if(bT2bMetrics) T2bUnits.SetNum(Members.Num());
		for (const auto& M : Members)
		{
			FNavLocation Start;
			if (!Nav || !Nav->ProjectPointToNavigation(M.Start, Start, FVector(350,350,400)))
			{
				UE_LOG(LogTemp, Error, TEXT("FIELD_INVALID_START id=%d"),M.ID);
				FPlatformMisc::RequestExitWithStatus(false,1);
				return;
			}
			auto& S = Soldiers.AddDefaulted_GetRef();
			S.P = Start.Location + FVector(0,0,90);
			S.Forward = FVector(M.ID<=Members.Num()/2 ? 1 : -1,0,0);
			const double A = Layout.FRand()*2*PI;
			const double Radius = FMath::Sqrt(Layout.FRand())*420;
			S.Slot = FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0);
		}
		Match->Phase = EBattleMatchPhase::Combat;
	}
	const float Dt = 1.f/FieldHz;
	Match->Elapsed = double(Frame+1)/FieldHz;
	Store->AdvanceReloads(Match->Elapsed);
	int32 Presence[3] = {};
	int32 Alive[2] = {};
	for (int32 I = 0; I < Soldiers.Num(); ++I)
	{
		auto& S = Soldiers[I];
		auto& M = Members[I];
		if (!S.bAlive) continue;
		const int32 Team = I/(Members.Num()/2);
		if (Frame % 150 == (I/5)%150 || !Frame)
		{
			const int32 Goal = Match->ChooseObjective((I%(Members.Num()/2))/5,Team);
			if (S.Goal != Goal || S.Path.IsEmpty())
				S.Goal = PlanFieldPath(World,I,Match->Objectives[Goal].Position) ? Goal : -1;
		}
		const bool Engaged = S.bEngaged && S.Candidate >= 0 && Soldiers[S.Candidate].bAlive;
		S.bMoving = !Engaged && S.Path.IsValidIndex(S.Waypoint);
		if (S.bMoving)
		{
			const FVector Delta = S.Path[S.Waypoint]+FVector(0,0,90)-S.P;
			const FVector Dir = Delta.GetSafeNormal();
			if (!Dir.IsNearlyZero() && !T2bActive(I)) S.Forward = Dir.GetSafeNormal2D();
			S.P += Dir*FMath::Min(Delta.Size(),double(Rules->MoveSpeed*Dt));
			if (Delta.Size() <= Rules->MoveSpeed*Dt) ++S.Waypoint;
		}
		if(bT2bOn) AdvanceT2b(I);
  if (S.bMoving || !Frame || T2bActive(I))
		{
			FFieldTimer Measure(DiagFrame[SyncTime],bFieldDiagnostics);
			if (M.Actor) M.Actor->SetActorLocationAndRotation(S.P,S.Forward.Rotation());
			else
			{
				Manager->GetFragmentDataChecked<FBattleTrialSoldier>(M.Entity).Position = S.P;
				M.Display->SetWorldLocationAndRotation(S.P-FVector(0,0,90), S.Forward.Rotation()+FRotator(0,-90,0));
			}
		}
		if (Frame == 3)
		{
			if (M.Actor) VisibleActors += M.Actor->GetMesh()->WasRecentlyRendered(.5f);
			else VisibleMass += M.Display->WasRecentlyRendered(.5f);
		}
	}
 if(bT2bMetrics) T2bBeforeScan();
	ScanField(Mode);
 if(bT2bMetrics) T2bAfterScan();
 if(bT2aMetrics) {
  if(MeasureCombat) { for(const auto& S:Soldiers) T2aAliveSum+=S.bAlive; ++T2aAliveFrames; }
  for(int32 I=0;I<Soldiers.Num();++I) if(!Soldiers[I].bAlive) T2aEndWait(I,TEXT("shooter_dead"));
 }
	for (int32 I = 0; I < Soldiers.Num(); ++I) if (Soldiers[I].bAlive)
	{
		const int32 Team = I/(Members.Num()/2);
		++Alive[Team];
		for (int32 J = 0; J < 3; ++J)
			if (UBattleMatch::InObjective(Soldiers[I].P, Match->Objectives[J])) Presence[J] |= 1<<Team;
	}
	int32 Owners[3];
	for (int32 J=0; J<3; ++J) Owners[J]=Match->Objectives[J].Owner;
	Match->AdvanceObjectiveControl(Dt,[&](const FBattleObjective& Point){ return Presence[&Point-Match->Objectives.GetData()]; });
	bool Contested = false;
	for (int32 J=0; J<3; ++J)
	{
		PointChanges += Owners[J]!=Match->Objectives[J].Owner;
		Contested |= Match->Objectives[J].bContested;
	}
	ContestedFrames += Contested;
	if (Match->Phase==EBattleMatchPhase::Combat && (!Alive[0] || !Alive[1]))
		Match->Finish(Alive[0]==Alive[1] ? -1 : Alive[0] ? 0 : 1,TEXT("ELIMINATION"));
	Match->CheckTimeLimit();
 if(bFieldDiagnostics) {
  DiagFrame[StepTime]=(FPlatformTime::Seconds()-StepStart)*1000.;
  ++DiagSamples; if(MeasureCombat) ++DiagCombatSamples;
  for(int32 K=0;K<TimingCount;++K) { DiagTotal[K]+=DiagFrame[K]; if(MeasureCombat) DiagCombat[K]+=DiagFrame[K]; }
 }
 if(!RecordFile.IsEmpty()) CaptureField(Mode);
 ++Frame;
}

void UBattleSquadTrial::ScanField(ABattleGameMode& Mode)
{
 FFieldTimer ScanMeasure(DiagFrame[ScanTime],bFieldDiagnostics);
 int32 Reject=0;
	auto* World = Mode.GetWorld();
	auto* Store = World->GetSubsystem<UBattleState>();
	const auto* Rules = GetDefault<UBattleTuning>();
	const double Now = double(Frame)/FieldHz;
	const auto& Profile = Store->Find(Members[0].ID)->Perception;
	const double Range2 = FMath::Square(Profile.SightRange);
	const double FovCos = FMath::Cos(FMath::DegreesToRadians(Profile.FieldOfView*.5));
	const int32 Delay = FMath::RoundToInt(Profile.DetectionSeconds*FieldHz);
	auto Visible = [&](int32 I,int32 J)
	{
		Reject=0;
		if (J < 0 || !Soldiers[J].bAlive || !Soldiers[I].bAlive) { Reject=1; return false; }
		const FVector Delta = Soldiers[J].P-Soldiers[I].P;
		if (Delta.SizeSquared()>Range2) { Reject=2; return false; }
		if (FVector::DotProduct(Delta.GetSafeNormal2D(),Soldiers[I].Forward)<FovCos) { Reject=3; return false; }
		++Rays;
		FFieldTimer RayMeasure(DiagFrame[RayTime],bFieldDiagnostics);
		const bool Clear = !World->LineTraceTestByObjectType(Soldiers[I].P+FVector(0,0,60),Soldiers[J].P+FVector(0,0,35),FCollisionObjectQueryParams(ECC_WorldStatic));
		if(!Clear) Reject=4; return Clear;
	};
	auto FindEnemy = [&](int32 I,int32 Previous)
	{
		FFieldTimer FindMeasure(DiagFrame[FindTime],bFieldDiagnostics);
		const double RayBefore=DiagFrame[RayTime];
		struct FSubtract { double& Sum; double& Rays; double Before; ~FSubtract(){Sum+=Rays-Before;} } Subtract{DiagFrame[FindRayTime],DiagFrame[RayTime],RayBefore};
		if (!Soldiers[I].bAlive) return -1;
		if (Visible(I,Previous)) return Previous;
		const int32 First = I<Members.Num()/2 ? Members.Num()/2 : 0;
		int32 Best = -1;
		double BestDist = Range2;
		for (int32 J=First; J<First+Members.Num()/2; ++J) if (Soldiers[J].bAlive)
		{
			++Candidates;
			const double D = FVector::DistSquared(Soldiers[I].P,Soldiers[J].P);
			if (D < BestDist && Visible(I,J)) { BestDist=D; Best=J; }
		}
		return Best;
	};
	TMap<int32,int32> T2aSlots;
 TMap<int32,double> T2aExpectedDamage;
 TArray<int32> PendingHits;
 TArray<int32> PendingBins; TMap<int32,int32> ShotTargets;
	for (int32 Base=0; Base<Soldiers.Num(); Base+=5)
	{
		if (Frame%ScanFrames!=(Base/5)%ScanFrames) continue;
		auto& Squad=FieldSquads[Base/5];
		const bool Shared = Name==TEXT("Squad");
		int32 Contact=-1, ContactObserver=-1;
		if (Shared)
		{
			for (int32 Slot : {2,0,4,1,3})
			{
				Contact=FindEnemy(Base+Slot,Squad.Candidate);
				if (Contact>=0) { ContactObserver=Base+Slot; break; }
			}
			if (Contact!=Squad.Candidate) { Squad.Candidate=Contact; Squad.Since=Frame; }
		}
		for (int32 I=Base; I<Base+5; ++I)
		{
			auto& S=Soldiers[I];
			if (!S.bAlive) continue;
			const bool GroupMember=Shared && !Members[I].Actor;
			const int32 Next=GroupMember ? Contact : FindEnemy(I,S.Candidate);
			if(bT2bMetrics) T2bCandidate(I,Next);
   if(bT2aMetrics && T2aWaits[I].Start>=0 && Next!=T2aWaits[I].Target) T2aEndWait(I,TEXT("target_changed"));
   if (Next!=S.Candidate) { if(bFieldDiagnostics) ++TargetChanges; S.Candidate=Next; S.Since=Frame; }
			S.bEngaged = false;
			if (Next<0) continue;
			if (Squad.Exposure<0) Squad.Exposure=Frame;
			if (Frame-(GroupMember ? Squad.Since : S.Since)<Delay) continue;
			if (Squad.Discovery<0) Squad.Discovery=Frame;
			if (GroupMember) {
    if(bFieldDiagnostics) ++SharedChecks;
    const bool Clear=Visible(I,Next);
    if(!Clear && bFieldDiagnostics) ++SharedDenied[Reject];
    const auto* ReadyState=Store->Find(Members[I].ID);
    const bool Ready=ReadyState->IsAlive() && ReadyState->Ammo>0 && ReadyState->ReloadEndTime<=0. && Now>=ReadyState->NextShotTime;
    if(bT2bMetrics && T2bCheck(I,Next,Clear,Reject,Ready)) continue;
    if(!Clear) continue;
   }
			S.bEngaged = true;
			const auto* State=Store->Find(Members[I].ID);
			if (State->Ammo==0) { Store->StartReload(State->UnitID,Now); continue; }
   // Only recipients of another soldier's shared contact are gated. Observer/Actor shots are untouched.
   const bool T2aShared=GroupMember && I!=ContactObserver;
   double Expected=0.;
   if(bT2aMetrics && T2aShared && State->IsAlive() && State->Ammo>0 && State->ReloadEndTime<=0. && Now>=State->NextShotTime) {
    ++T2aEligible;
    auto Estimate=*State; Estimate.bMovingShot=S.bMoving; Estimate.AimStableSince=-1.;
    const double Half=FMath::DegreesToRadians(double(BattleTactics::SpreadDegrees(Estimate,Now)));
    const double Angle=FMath::Asin(FMath::Clamp(30./FVector::Dist(Soldiers[Next].P,S.P),0.,1.));
    double Probability=1.;
    if(Angle<Half && Half>0.) { Probability=0.; for(double K=0.;K<UE_DOUBLE_PI;K+=Half) Probability+=(FMath::Cos(K)-FMath::Cos(FMath::Min(K+Angle,UE_DOUBLE_PI)))*.5; }
    Expected=Rules->ShotDamage*FMath::Clamp(Probability,0.,1.);
    const int32 Cap=T2aAllocation==TEXT("Cap1") ? 1 : 2;
    const bool Block=(T2aAllocation==TEXT("Cap1") || T2aAllocation==TEXT("Cap2")) ? T2aSlots.FindRef(Next)>=Cap : T2aAllocation==TEXT("Expected") && T2aExpectedDamage.FindRef(Next)>=100.;
    if(Block) {
     ++T2aDeferred;
     if(T2aWaits[I].Start<0) { T2aWaits[I].Start=Frame; T2aWaits[I].Target=Next; }
     continue;
    }
   }
			if (!Store->ConsumeShot(State->UnitID,Now,Rules->ShotInterval(State->Weapon, false))) continue;
   if(bT2aMetrics) {
    T2aEndWait(I,TEXT("fired"));
    if(T2aShared) { ++T2aAllocated; ++T2aSlots.FindOrAdd(Next); T2aExpectedDamage.FindOrAdd(Next)+=Expected; }
   }
   if(bT2bMetrics) T2bShot(I,Next);
			++Shots;
			auto Shooter=*State;
			Shooter.bMovingShot=S.bMoving;
			Shooter.AimStableSince=-1;
			const FVector Delta=Soldiers[Next].P-S.P;
			const FVector Dir=BattleTactics::ShotDirection(Shooter,Delta,Now);
			const bool Hit = FVector::CrossProduct(Delta,Dir).SizeSquared()<=900;
   if (Hit) PendingHits.Add(Next);
   if(bFieldDiagnostics) {
    const double Distance=Delta.Size();
    const int32 Bin=(S.bMoving ? 6 : 0)+(State->Weapon==EBattleWeapon::SupportGun ? 3 : 0)+(Distance<800 ? 0 : Distance<1600 ? 1 : 2);
    auto& B=ShotBins[Bin]; ++B.Shots; B.RawHits+=Hit; B.Distance+=Distance/100.; B.Spread+=BattleTactics::SpreadDegrees(Shooter,Now);
    RawHits+=Hit; if(Hit) PendingBins.Add(Bin); ++ShotTargets.FindOrAdd(Next);
   }
		}
	}
	if(bFieldDiagnostics) for(const auto& P:ShotTargets) { ++ShotTargetGroups; DuplicateTargetShots+=P.Value-1; }
	for(int32 H=0; H<PendingHits.Num(); ++H)
	{
		const int32 J=PendingHits[H]; if(!Soldiers[J].bAlive) continue;
		if (Store->ApplyDamage(Members[J].ID,1-J/(Members.Num()/2),Rules->ShotDamage)) { ++Hits; if(bFieldDiagnostics) ++ShotBins[PendingBins[H]].AppliedHits; }
		if (!Store->Find(Members[J].ID)->IsAlive())
		{
			Soldiers[J].bAlive=false;
			if (Members[J].Actor) Members[J].Actor->SetActorHiddenInGame(true);
			else Members[J].Display->SetVisibility(false,true);
		}
	}
}

void UBattleSquadTrial::FinishField(ABattleGameMode& Mode)
{
	auto* Store=Mode.GetWorld()->GetSubsystem<UBattleState>();
	auto* Match=Mode.GetWorld()->GetSubsystem<UBattleMatch>();
	int32 Alive[2]={}, Spent=0, Damage=0;
	for (const auto& M : Members)
	{
		const auto* S=Store->Find(M.ID);
		Alive[S->Team]+=S->IsAlive();
		Damage+=FMath::RoundToInt(100-S->HP);
		Spent+=S->ShotSequence;
	}
	double Sum=0, First=0, Delay=0, CombatSum=0;
	for(double D:CombatFrames) CombatSum+=D;
	for (double D : Frames) Sum+=D;
	Frames.Sort();
	for (const auto& Squad : FieldSquads) if (Squad.Discovery>=0)
	{
		First+=double(Squad.Discovery)/FieldHz;
		Delay+=double(Squad.Discovery-Squad.Exposure)/FieldHz;
		Latencies.Add(Squad.Discovery-Squad.Exposure);
	}
	Latencies.Sort();
	const int32 N=Latencies.Num();
	const bool Ok=N>0 && Paths>=Members.Num() && Candidates>0 && Match->Phase==EBattleMatchPhase::Finished &&
		ContestedFrames>0 && Shots==Spent && Damage==Hits*10 && Hits>0 && (bT2aHeadless || bT2bHeadless || VisibleActors+VisibleMass==Members.Num());
	UE_LOG(LogTemp,Display,TEXT("FIELD_TRIAL mode=%s seed=%d drawn=%d/%d frames=%d fps=%.2f fps30=%.2f combatFPS=%.2f combatFrames=%d p95ms=%.2f paths=%d failures=%d candidates=%lld rays=%d contacts=%d first=%.3f delay=%.3f delay95=%.3f losses30=%d shots=%d hits=%d alive=%d/%d score=%.1f/%.1f captures=%d contested=%.1f time=%.1f winner=%d success=%d"),
		*Name,Seed,VisibleActors,VisibleMass,Frames.Num(),Frames.Num()*1000/Sum,FPS30,CombatFrames.Num()*1000/FMath::Max(1.,CombatSum),CombatFrames.Num(),Frames[FMath::CeilToInt(.95*Frames.Num())-1],
		Paths,PathFailures,Candidates,Rays,N,First/FMath::Max(1,N),Delay/FMath::Max(1,N),N?double(Latencies[FMath::CeilToInt(.95*N)-1])/FieldHz:0.,
		Loss30,Shots,Hits,Alive[0],Alive[1],Match->Score[0],Match->Score[1],PointChanges,double(ContestedFrames)/FieldHz,Match->Elapsed,Match->Winner,Ok);
 if(!RecordFile.IsEmpty() && !SaveFieldRecording()) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
	if(bFieldDiagnostics) FinishFieldDiagnostics();
 if(bT2aMetrics) FinishT2a();
 if(bT2bMetrics) FinishT2b();
	FPlatformMisc::RequestExitWithStatus(false,Ok?0:1);
}

void UBattleSquadTrial::FinishFieldDiagnostics()
{
 auto Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("mode"),Name); Root->SetNumberField(TEXT("seed"),Seed);
 Root->SetNumberField(TEXT("frames"),DiagSamples); Root->SetNumberField(TEXT("combat_frames"),DiagCombatSamples);
 const TCHAR* Keys[]={TEXT("step"),TEXT("find_inclusive"),TEXT("find_ray"),TEXT("ray"),TEXT("navigation"),TEXT("display_sync"),TEXT("scan_inclusive"),TEXT("game_thread"),TEXT("render_thread"),TEXT("rhi_thread"),TEXT("gpu")};
 for(int32 Window=0;Window<2;++Window) {
  auto Times=MakeShared<FJsonObject>(); const double* Values=Window ? DiagCombat : DiagTotal; const int32 N=Window ? DiagCombatSamples : DiagSamples;
  for(int32 K=0;K<TimingCount;++K) Times->SetNumberField(Keys[K],Values[K]/FMath::Max(1,N));
  Times->SetNumberField(TEXT("find_excluding_rays"),(Values[FindTime]-Values[FindRayTime])/FMath::Max(1,N));
  Root->SetObjectField(Window ? TEXT("combat_ms_per_frame") : TEXT("match_ms_per_frame"),Times);
 }
 Root->SetNumberField(TEXT("shots"),Shots); Root->SetNumberField(TEXT("raw_hits"),RawHits); Root->SetNumberField(TEXT("applied_hits"),Hits);
 Root->SetNumberField(TEXT("overkill_hits"),RawHits-Hits); Root->SetNumberField(TEXT("spread_misses"),Shots-RawHits);
 Root->SetNumberField(TEXT("shared_checks"),SharedChecks); Root->SetNumberField(TEXT("shared_reject_dead"),SharedDenied[1]);
 Root->SetNumberField(TEXT("shared_reject_range"),SharedDenied[2]); Root->SetNumberField(TEXT("shared_reject_fov"),SharedDenied[3]); Root->SetNumberField(TEXT("shared_reject_terrain"),SharedDenied[4]);
 Root->SetNumberField(TEXT("target_changes"),TargetChanges); Root->SetNumberField(TEXT("duplicate_target_shots"),DuplicateTargetShots); Root->SetNumberField(TEXT("shot_target_groups"),ShotTargetGroups);
 TArray<TSharedPtr<FJsonValue>> Bins;
 for(int32 I=0;I<12;++I) { const auto& B=ShotBins[I]; auto J=MakeShared<FJsonObject>();
  J->SetBoolField(TEXT("moving"),I>=6); J->SetStringField(TEXT("weapon"),(I%6)>=3 ? TEXT("support") : TEXT("carbine")); J->SetNumberField(TEXT("distance_bin"),I%3);
  J->SetNumberField(TEXT("shots"),B.Shots); J->SetNumberField(TEXT("raw_hits"),B.RawHits); J->SetNumberField(TEXT("applied_hits"),B.AppliedHits);
  J->SetNumberField(TEXT("distance_sum_m"),B.Distance); J->SetNumberField(TEXT("spread_sum_deg"),B.Spread); Bins.Add(MakeShared<FJsonValueObject>(J)); }
 Root->SetArrayField(TEXT("shot_bins"),Bins);
 FString Json; auto Writer=TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Root,Writer);
 const FString File=FPaths::GetPath(Capture)/FPaths::GetBaseFilename(Capture)+TEXT("-diagnostics.json");
 if(!FFileHelper::SaveStringToFile(Json,*File)) { FPlatformMisc::RequestExitWithStatus(false,1); }
 UE_LOG(LogTemp,Display,TEXT("FIELD_DIAGNOSTICS file=%s raw=%lld overkill=%lld spreadMiss=%lld"),*File,RawHits,RawHits-Hits,Shots-RawHits);
}

void UBattleSquadTrial::T2aEndWait(int32 I, const TCHAR* Reason)
{
 auto& W=T2aWaits[I]; if(W.Start<0) return;
 auto E=MakeShared<FJsonObject>(); E->SetNumberField(TEXT("shooter"),I); E->SetNumberField(TEXT("target"),W.Target);
 E->SetNumberField(TEXT("start_frame"),W.Start); E->SetNumberField(TEXT("end_frame"),Frame);
 E->SetNumberField(TEXT("delay_seconds"),double(Frame-W.Start)/FieldHz); E->SetStringField(TEXT("outcome"),Reason);
 T2aEvents.Add(MakeShared<FJsonValueObject>(E)); W.Start=-1; W.Target=-1;
}
void UBattleSquadTrial::FinishT2a()
{
 for(int32 I=0;I<T2aWaits.Num();++I) T2aEndWait(I,TEXT("match_end"));
 auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("allocation"),T2aAllocation); R->SetStringField(TEXT("mode"),Name); R->SetNumberField(TEXT("seed"),Seed);
 R->SetBoolField(TEXT("headless"),bT2aHeadless); R->SetNumberField(TEXT("eligible_attempts"),T2aEligible);
 R->SetNumberField(TEXT("allocated_shared_shots"),T2aAllocated); R->SetNumberField(TEXT("deferred_attempts"),T2aDeferred);
 R->SetNumberField(TEXT("combat_alive_frames"),T2aAliveFrames); R->SetNumberField(TEXT("combat_mean_alive"),T2aAliveSum/FMath::Max(1,T2aAliveFrames));
 R->SetArrayField(TEXT("wait_episodes"),T2aEvents);
 FString Json; auto Writer=TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(R,Writer);
 const FString File=FPaths::GetPath(Capture)/FPaths::GetBaseFilename(Capture)+TEXT("-t2a.json");
 if(!FFileHelper::SaveStringToFile(Json,*File)) FPlatformMisc::RequestExitWithStatus(false,1);
}
