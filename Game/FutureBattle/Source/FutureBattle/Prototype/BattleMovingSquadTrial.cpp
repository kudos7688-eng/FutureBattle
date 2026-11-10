#include "BattleSquadTrial.h"
#include "BattleGameMode.h"
#include "BattleUnit.h"
#include "BattleTactics.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/HUD.h"
#include "UnrealClient.h"

namespace
{
	constexpr int32 MovingFrames = 900;
	constexpr int32 StepsPerSecond = 30;
	constexpr int32 ScanPeriod = 6;
}

FVector UBattleSquadTrial::MovingStart(int32 Team, int32 Lane)
{
	const int32 Group = Lane / 5;
	return FVector((Group % 5 - 2) * 1550 + (Team ? 1 : -1) * (350 + (Lane % 2) * 60),
		(Group / 5 - 9.5) * 1240 + (Lane % 5 - 2) * 78, 2200);
}

void UBattleSquadTrial::SetupMovingView(ABattleGameMode& Mode)
{
	auto* World = Mode.GetWorld();
	auto* Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Right.MF_Unarmed_Walk_Right"));
	const bool Armed = Members[0].Actor->HasWeaponVisuals();
	if (Armed) Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"));
	auto* Model = Members[0].Actor->GetMesh()->GetSkeletalMeshAsset();
	// One animation pose for the crowd, 900 separately rendered skinned mesh components.
	AActor* RenderHost = World->SpawnActor<AActor>();
	USkeletalMeshComponent* Leader = nullptr;
	for (auto& M : Members)
	{
		if (M.Actor)
		{
			M.Actor->GetMesh()->PlayAnimation(Walk, true);
			continue;
		}
		if (!Leader)
		{
			Leader = NewObject<USkeletalMeshComponent>(RenderHost);
			Leader->SetSkeletalMesh(Model);
			Leader->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Leader->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Leader->SetVisibility(false);
			Leader->RegisterComponent();
			Leader->PlayAnimation(Walk, true);
		}
		M.Display = NewObject<USkeletalMeshComponent>(RenderHost);
		M.Display->SetSkeletalMesh(Model);
		M.Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bool Enemy = M.ID > Members.Num()/2;
		M.Display->SetWorldLocationAndRotation(M.Start - FVector(0,0,90), FRotator(0, Enemy ? 90 : -90, 0));
		M.Display->RegisterComponent();
		M.Display->SetLeaderPoseComponent(Leader);
		M.Display->bUseBoundsFromLeaderPoseComponent = false;
		auto* Rifle = NewObject<UStaticMeshComponent>(RenderHost);
		Rifle->SetStaticMesh(Members[0].Actor->Rifle->GetStaticMesh());
		Rifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rifle->SetupAttachment(M.Display);
		Rifle->SetRelativeLocationAndRotation(FVector(0,42,137), FRotator(0,90,0));
		Rifle->SetRelativeScale3D(Members[0].Actor->Rifle->GetRelativeScale3D());
		Rifle->RegisterComponent();
		if (Armed) Members[0].Actor->MountWeapon(Rifle, M.Display, World->GetSubsystem<UBattleState>()->Find(M.ID)->Weapon, false);
	}
	if (bField) return;
	auto* Ground = World->SpawnActor<AStaticMeshActor>(FVector(0,0,2060), FRotator::ZeroRotator);
	Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ground->SetActorScale3D(FVector(86,250,1));
	Exposures.SetNum(Members.Num()/5);
	check(Walk && Model);
}

void UBattleSquadTrial::StepMoving(ABattleGameMode& Mode)
{
	if (Frame > MovingFrames) return;
	UWorld* World = Mode.GetWorld();
	if (Frame == -58)
	{
		auto* PC = World->GetFirstPlayerController();
		auto* Camera = World->SpawnActor<ACameraActor>(FVector(-11500,0,18000), FRotator(-54,0,0));
		Camera->GetCameraComponent()->SetFieldOfView(76);
		PC->SetViewTarget(Camera);
		if (PC->GetHUD()) PC->GetHUD()->bShowHUD = false;
	}
	if (Frame == -20) FScreenshotRequest::RequestScreenshot(Capture, false, false);
	const double WallTime = FPlatformTime::Seconds();
	if (Frame > 0) Frames.Add((WallTime - LastFrame)*1000);
	LastFrame = WallTime;
	if (Frame < -40) { ++Frame; return; }
	if (Frame == MovingFrames) { FinishMoving(Mode); ++Frame; return; }

	auto* Store = World->GetSubsystem<UBattleState>();
	const auto* Rules = GetDefault<UBattleTuning>();
	const double Now = double(Frame)/StepsPerSecond;
	const int32 Half = Members.Num()/2;
	TArray<int32> DamageQueue;
	for (int32 Base = 0; Base < Members.Num(); Base += 5)
	{
		const int32 Lane = Base % Half;
		const double Offset = 370 * FMath::Sin(Now * 1.35 + (Lane/5 % 13) * .48);
		
		for (int32 Slot = 0; Slot < 5; ++Slot)
		{
			auto& M = Members[Base + Slot];
			if (!Store->Find(M.ID)->IsAlive()) continue;
			const FVector P = M.Start + FVector(0,Offset,0);
			if (M.Actor)
			{
				M.Actor->SetActorLocation(P);
				if (Frame == 6 && M.Actor->GetMesh()->WasRecentlyRendered(.5f)) ++VisibleActors;
			}
			else
			{
				Manager->GetFragmentDataChecked<FBattleTrialSoldier>(M.Entity).Position = P;
				M.Display->SetWorldLocation(P - FVector(0,0,90));
				if (Frame == 6 && M.Display->WasRecentlyRendered(.5f)) ++VisibleMass;
			}
		}
	}
	if (Frame < 0) { ++Frame; return; }
	// Both teams have moved before any sensor runs. The analytic visibility oracle
	// measures exposures independently; no decision uses it.
	for (int32 Base = 0; Base < Members.Num(); Base += 5)
	{
		const int32 Opposite = (Base + Half) % Members.Num();
		const double WallY = MovingStart(0,Base % Half).Y + 156;
		auto PairAlive = [&](int32 S) { return Store->Find(Members[Base+S].ID)->IsAlive() && Store->Find(Members[Opposite+S].ID)->IsAlive(); };
		auto Open = [&](int32 S) { return FMath::Abs(Position(Members[Base+S]).Y - WallY) > 220; };
		auto Visible = [&](int32 S)
		{
			if (!PairAlive(S)) return false;
			++Rays;
			const bool Clear = !World->LineTraceTestByObjectType(Position(Members[Base+S]), Position(Members[Opposite+S]),
				FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams::DefaultQueryParam);
			if (Clear != Open(S)) ++GeometryErrors;
			return Clear;
		};
		auto& E = Exposures[Base/5];
		bool Any = false;
		for (int32 S = 0; S < 5; ++S) Any |= PairAlive(S) && Open(S);
		if (Any && !E.bWasOpen) { E.StartFrame = Frame; E.bDetected = false; }
		if (!Any && E.StartFrame >= 0)
		{
			if (!E.bDetected) ++Missed;
			E.StartFrame = -1;
		}
		E.bWasOpen = Any;
		if (Frame % ScanPeriod != (Base/5) % ScanPeriod) continue;
		const bool Shared = Name == TEXT("Squad");
		const bool Contact = Shared && (Visible(2) || Visible(0) || Visible(4) || Visible(1) || Visible(3));
		for (int32 S = 0; S < 5; ++S)
		{
			auto& M = Members[Base+S];
			auto& Target = Members[Opposite+S];
			const bool GroupMember = Shared && !M.Actor;
			const bool Seen = GroupMember ? Contact : Visible(S);
			M.Seen = Seen ? M.Seen+1 : 0;
			if (M.Seen < 4 || !PairAlive(S)) continue;
			if (E.StartFrame >= 0 && !E.bDetected)
			{
				Latencies.Add(Frame-E.StartFrame);
				E.bDetected = true;
			}
			if (Now < Store->Find(M.ID)->NextShotTime || (GroupMember && !Visible(S))) continue;
			if (!Store->ConsumeShot(M.ID, Now, Rules->AIShotInterval)) continue;
			++Shots;
			auto Shooter = *Store->Find(M.ID);
			Shooter.bMovingShot = true;
			const FVector Delta = Position(Target)-Position(M);
			const FVector Dir = BattleTactics::ShotDirection(Shooter, Delta, Now);
			if (FVector::CrossProduct(Delta,Dir).SizeSquared() <= 900) DamageQueue.Add(Target.ID);
		}
	}
	for (int32 ID : DamageQueue)
	{
		const auto* S = Store->Find(ID);
		if (Store->ApplyDamage(ID, 1-S->Team, Rules->ShotDamage)) ++Hits;
		if (!S->IsAlive())
		{
			auto& M = Members[ID-1];
			if (M.Actor) M.Actor->SetActorHiddenInGame(true);
			else M.Display->SetVisibility(false,true);
		}
	}
	++Frame;
}

void UBattleSquadTrial::FinishMoving(ABattleGameMode& Mode)
{
	auto* Store = Mode.GetWorld()->GetSubsystem<UBattleState>();
	int32 Alive[2] = {}, AmmoSpent = 0, Damage = 0, Mass = 0;
	for (const auto& M : Members)
	{
		const auto* S = Store->Find(M.ID);
		Alive[S->Team] += S->IsAlive();
		AmmoSpent += GetDefault<UBattleTuning>()->Magazine - S->Ammo;
		Damage += FMath::RoundToInt(100-S->HP);
		Mass += M.Display != nullptr;
	}
	for (const auto& E : Exposures) if (E.StartFrame >= 0 && !E.bDetected) ++Censored;
	Frames.Sort();
	Latencies.Sort();
	double Sum = 0, Delay = 0;
	for (double D : Frames) Sum += D;
	for (int32 D : Latencies) Delay += double(D)/StepsPerSecond;
	const int32 Confirmed = Latencies.Num();
	const bool Ok = Frames.Num()==MovingFrames && Confirmed>0 && GeometryErrors==0 && AmmoSpent==Shots &&
		Hits>0 && Damage==Hits*10 && VisibleActors==Mode.Units.Num() && VisibleMass==Mass && Mass==(Name==TEXT("Actors") ? 0 : 900);
	UE_LOG(LogTemp,Display,TEXT("MOVING_TRIAL mode=%s actors=%d drawnActors=%d mass=%d drawnMass=%d frames=%d fps=%.2f meanMS=%.3f p95MS=%.3f detected=%d missed=%d censored=%d delayMean=%.3f delayP95=%.3f rays=%d shots=%d hits=%d allies=%d enemies=%d geometryErrors=%d success=%d"),
		*Name, Mode.Units.Num(), VisibleActors, Mass, VisibleMass, Frames.Num(), Frames.Num()*1000/Sum, Sum/Frames.Num(), Frames[854],
		Confirmed,Missed,Censored,Delay/FMath::Max(1,Confirmed),Confirmed ? double(Latencies[FMath::CeilToInt(.95*Confirmed)-1])/StepsPerSecond : -1,
		Rays,Shots,Hits,Alive[0],Alive[1],GeometryErrors,Ok);
	FPlatformMisc::RequestExitWithStatus(false, Ok ? 0 : 1);
}
