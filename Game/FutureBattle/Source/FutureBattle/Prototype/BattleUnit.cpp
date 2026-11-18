#include "BattleUnit.h"
#include "BattleLeaderRuntime.h"
#include "BattleMuzzleFlash.h"
#include "Animation/AnimSequence.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattleCover.h"
#include "BattleStance.h"
#include "BattleSquadAI.h"
#include "BattleMetrics.h"
#include "BattleMatch.h"
#include "NavMesh/RecastNavMesh.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"

ABattleUnit::ABattleUnit(const FObjectInitializer& Init) : Super(Init)
{
	PrimaryActorTick.bCanEverTick = true;
	FString Phase3Output;
	bThirdPersonView = !FParse::Param(FCommandLine::Get(), TEXT("BattleFirstPerson")) &&
		!FParse::Value(FCommandLine::Get(), TEXT("BattlePhase3="), Phase3Output);
	GetCapsuleComponent()->InitCapsuleSize(36.f, 90.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetRelativeLocation(FVector(0, 0, -90));
	GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> UnitMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (UnitMesh.Succeeded()) GetMesh()->SetSkeletalMesh(UnitMesh.Object);
	if (Anim.Succeeded()) GetMesh()->SetAnimInstanceClass(Anim.Class);
	GetCharacterMovement()->MaxWalkSpeed = GetDefault<UBattleTuning>()->MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);
	GetCharacterMovement()->JumpZVelocity = 480;
	GetCharacterMovement()->AirControl = 0.3f;
	GetCharacterMovement()->bUseRVOAvoidance = true;
	// Soldiers path on the Default (infantry) navigation data; tanks switch to ABattleVehicleNavMesh (BattleVehicle.cpp).
	GetCharacterMovement()->NavAgentProps.SetPreferredNavData(ARecastNavMesh::StaticClass());
	GetCharacterMovement()->AvoidanceConsiderationRadius = 350.f;
	bUseControllerRotationYaw = false;
	AIControllerClass = ABattleAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled;
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetRootComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0, 0, 65));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = 90.f;
	FirstPersonCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	FirstPersonCamera->PostProcessSettings.MotionBlurAmount = 0;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Carbine(TEXT("/Game/Prototype/Weapons/SM_Carbine"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Support(TEXT("/Game/Prototype/Weapons/SM_Support"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Reload(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"));
	CarbineModel = Carbine.Object; SupportModel = Support.Object;
	RifleIdle = Idle.Object; RifleWalk = Walk.Object; RifleReload = Reload.Object;
	Rifle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeRifle"));
	Rifle->SetupAttachment(FirstPersonCamera);
	Rifle->SetStaticMesh(Cube.Object);
	Rifle->SetRelativeLocation(FVector(42, 18, -18));
	Rifle->SetRelativeScale3D(FVector(.65, .07, .09));
	Rifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rifle->SetCastShadow(false);
	TeamLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("UnitLabel"));
	TeamLabel->SetupAttachment(GetRootComponent());
	TeamLabel->SetRelativeLocation(FVector(0, 0, 115));
	TeamLabel->SetHorizontalAlignment(EHTA_Center);
	TeamLabel->SetWorldSize(28.f);
	TeamLabel->SetVisibility(false); // HUD already draws compact labels at a fixed screen size.
}

void ABattleUnit::InitializeUnit(int32 ID)
{
	UnitID = ID;
	SetAnimationDetail(false);
	if (const FBattleUnitState* S = GetState())
	{
		TeamLabel->SetText(FText::FromString(FString::Printf(TEXT("%s %02d"), S->Team == 0 ? TEXT("ALLY") : TEXT("HOSTILE"), ID)));
		TeamLabel->SetTextRenderColor(S->Team == 0 ? FColor(75, 230, 195) : FColor(255, 90, 70));
	}
	FString TrialVariant;
	bWeaponVisuals = !FParse::Param(FCommandLine::Get(), TEXT("BattleLegacyWeaponVisuals"));
	bTrialPose = FParse::Value(FCommandLine::Get(), TEXT("BattleSquadTrial="), TrialVariant);
	if (bWeaponVisuals && GetState()) MountWeapon(Rifle, GetMesh(), GetState()->Weapon, false);
	SpawnDefaultController();
	SavedAI = Cast<AAIController>(GetController());
}

const FBattleUnitState* ABattleUnit::GetState() const
{
	return GetWorld()->GetSubsystem<UBattleState>()->Find(UnitID);
}

bool ABattleUnit::IsAlive() const { const auto* S = GetState(); return S && S->IsAlive(); }
FVector ABattleUnit::EyePosition() const { return GetActorLocation() + FVector(0, 0, 60); }

void ABattleUnit::CalcCamera(float DeltaTime, FMinimalViewInfo& View)
{
	Super::CalcCamera(DeltaTime, View);
	if (!IsPlayerControlled()) return;
	// Aim / scope: the magnification narrows the field of view.
	if (AimZoom > 1.001f) View.FOV = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(View.FOV * .5f)) / AimZoom));
	if (!bThirdPersonView) return;
	View.Rotation = GetControlRotation();
	const FVector Pivot = FirstPersonCamera->GetComponentLocation();
	// Aiming pulls the rear camera in over the shoulder; a scope looks from the eye itself.
	const float Aim = FMath::Clamp((AimZoom - 1.f) / .6f, 0.f, 1.f);
	const FVector Offset = FMath::Lerp(FVector(-350, 65, 45), FVector(-150, 55, 28), Aim) * (1.f - ScopeAlpha);
	View.Location = Pivot;
	if (Offset.IsNearlyZero()) return;
	const FVector Desired = Pivot + View.Rotation.RotateVector(Offset);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleThirdPersonCamera), false, this);
	GetWorld()->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Query);
	View.Location = Hit.bBlockingHit ? Hit.Location : Desired;
}

static const float Magnification[] = { 1.f, 1.6f, 4.f, 8.f };

float ABattleUnit::AimSpreadScale() const
{
	// Only once the view has (nearly) reached the level: a tap of the button gives nothing.
	if (!AimLevel || !IsPlayerControlled() || AimZoom < Magnification[AimLevel] * .9f) return 1.f;
	const UBattleTuning* Tuning = GetDefault<UBattleTuning>();
	return FMath::Clamp(AimLevel == 1 ? Tuning->AimSpreadScale : AimLevel == 2 ? Tuning->Scope4SpreadScale : Tuning->Scope8SpreadScale, .05f, 1.f);
}

void ABattleUnit::UpdateAimView()
{
	if (!IsPlayerControlled()) AimLevel = 0;
	// The view follows the real clock, not the game speed.
	const float Dt = FMath::Min(float(FApp::GetDeltaTime()), .1f);
	AimZoom = FMath::FInterpTo(AimZoom, Magnification[AimLevel], Dt, 14.f);
	if (FMath::IsNearlyEqual(AimZoom, Magnification[AimLevel], .005f)) AimZoom = Magnification[AimLevel];
	ScopeAlpha = FMath::FInterpConstantTo(ScopeAlpha, AimLevel >= 2 ? 1.f : 0.f, Dt, 8.f);
	// Through the scope the soldier's own body and rifle would fill the view.
	const bool Hide = ScopeView();
	if (Hide != bScopeHidden)
	{
		bScopeHidden = Hide;
		Rifle->SetOwnerNoSee(Hide);
		if (bThirdPersonView) GetMesh()->SetOwnerNoSee(Hide);
	}
}

void ABattleUnit::GetPlayerShot(FVector& Start, FVector& Direction)
{
	// Keep the shot at the soldier's eye: the rear camera must never fire around cover.
	Start = FirstPersonCamera->GetComponentLocation();
	Direction = GetControlRotation().Vector();
	if (!bThirdPersonView) return;
	FMinimalViewInfo View; CalcCamera(0.f, View);
	// Aim to wherever the bullet can reach (range profile: further than the AI engagement distance).
	const FVector End = View.Location + View.Rotation.Vector() * BattleRanges::Reach(GetState()->Weapon);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleThirdPersonAim), false, this);
	GetWorld()->LineTraceSingleByChannel(Hit, View.Location, End, ECC_Visibility, Query);
	Direction = ((Hit.bBlockingHit ? Hit.ImpactPoint : End) - Start).GetSafeNormal();
}

bool ABattleUnit::FireFromPlayerView()
{
	FVector Start, Direction; GetPlayerShot(Start, Direction);
	return FireAt(Start, Direction, true);
}

void ABattleUnit::Tick(float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleUnitTick);
	Super::Tick(DeltaTime);
	if (AimLevel || AimZoom > 1.f || ScopeAlpha > 0.f) UpdateAimView();
	if (const auto* State = GetState())
	{
		if (State->IsAlive()) GetWorld()->GetSubsystem<UBattleState>()->UpdateAim(UnitID, GetWorld()->GetTimeSeconds(), GetVelocity().Size2D(), GetBaseAimRotation().Vector(), GetCharacterMovement()->IsMovingOnGround());
		else ApplyDeath();
		if (State->IsAlive() && bWeaponVisuals && !bTrialPose) UpdateWeaponPose(*State);
	}
	if (TeamLabel->IsVisible()) if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager) TeamLabel->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation() - TeamLabel->GetComponentLocation()).Rotation());
	}
}

bool ABattleUnit::CanSee(const ABattleUnit* Other) const
{
	return CanSeeSnapshot(CaptureSightContext(), Other, Other ? Other->GetActorLocation() : FVector::ZeroVector, Other && Other->IsAlive());
}

FBattleSightContext ABattleUnit::CaptureSightContext() const
{
	FBattleSightContext Context;
	const auto* State = GetState(); Context.bAlive = State && State->IsAlive();
	Context.Position = GetActorLocation(); Context.Eye = EyePosition();
	const float Yaw = IsPlayerControlled() ? GetControlRotation().Yaw : GetActorRotation().Yaw;
	Context.Forward = FRotator(0, Yaw, 0).Vector();
	if (State)
	{
		Context.RangeSquared = FMath::Square(State->Perception.SightRange);
		Context.HalfAngleCos = FMath::Cos(FMath::DegreesToRadians(State->Perception.FieldOfView * .5f));
		Context.bOmnidirectional = State->Perception.FieldOfView >= 360.f;
	}
	Context.Metrics = GetWorld()->GetSubsystem<UBattleMetrics>();
	Context.Query = FCollisionQueryParams(SCENE_QUERY_STAT(BattleSight), false, this);
	return Context;
}

bool ABattleUnit::CanSeeSnapshot(const FBattleSightContext& Context, const ABattleUnit* Other, const FVector& Position, bool bAlive) const
{
	if (Context.Metrics->bRecording) ++Context.Metrics->SightChecks;
	if (!Other || !bAlive || !Context.bAlive) return false;
	if (FVector::DistSquared(Context.Position, Position) > Context.RangeSquared) return false;
	const FVector Direction = (Position - Context.Position).GetSafeNormal2D();
	if (!Context.bOmnidirectional && !Direction.IsNearlyZero() && FVector::DotProduct(Context.Forward, Direction) + 1.e-6f < Context.HalfAngleCos) return false;
	FHitResult Hit;
	if (Context.Metrics->bRecording) ++Context.Metrics->SightRays;
	GetWorld()->LineTraceSingleByChannel(Hit, Context.Eye, Position + FVector(0, 0, 35), ECC_Visibility, Context.Query);
	return !Hit.bBlockingHit || Hit.GetActor() == Other;
}

bool ABattleUnit::FireAt(FVector Start, FVector Direction, bool bPlayerShot)
{
 if(auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); L->IsEnabled()) return L->Fire(UnitID,Start,Direction,bPlayerShot);
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled() && Match->Phase != EBattleMatchPhase::Combat) return false;
	const UBattleTuning* Tuning = GetDefault<UBattleTuning>();
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	if (Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
	Store->UpdateAim(UnitID, GetWorld()->GetTimeSeconds(), GetVelocity().Size2D(), Direction, GetCharacterMovement()->IsMovingOnGround());
	if (!Store->ConsumeShot(UnitID, GetWorld()->GetTimeSeconds(), Tuning->ShotInterval(GetState()->Weapon, bPlayerShot))) return false;
	auto* Metrics = GetWorld()->GetSubsystem<UBattleMetrics>();
	if (Metrics->bRecording) ++Metrics->Shots;
	LastVisualShot = GetWorld()->GetTimeSeconds();
	// Apply exactly the same bounded cone to AI and directly controlled shots.
	Direction = BattleTactics::ShotDirection(*GetState(), Direction, GetWorld()->GetTimeSeconds(), bPlayerShot ? AimSpreadScale() : 1.f);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleShot), false, this);
	const FVector End = Start + Direction.GetSafeNormal() * BattleRanges::Reach(GetState()->Weapon);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
	if (ABattleUnit* Victim = Cast<ABattleUnit>(Hit.GetActor()))
	{
		if (Store->ApplyDamage(Victim->GetUnitID(), GetState()->Team, Tuning->ShotDamage))
		{
			if (Metrics->bRecording) ++Metrics->Hits;
			if (bPlayerShot) if (ABattleController* PC = Cast<ABattleController>(GetController())) PC->HitMarkerUntil = GetWorld()->GetTimeSeconds() + .18;
			if (!Victim->IsAlive()) Victim->ApplyDeath();
		}
	}
	if (bRevealed)
	{
		const FVector From = TracerStart(Rifle, Start, Direction);
		if (!(bPlayerShot && ScopeView())) BattleMuzzleFlash::Show(GetWorld(), From, Direction);
		DrawDebugLine(GetWorld(), From, Hit.bBlockingHit ? Hit.ImpactPoint : End,
			GetState()->Team == 0 ? FColor(100, 245, 230) : FColor(255, 120, 65), false, .09f, 0, 2.f);
		if (Hit.bBlockingHit) DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10, FColor::Yellow, false, .12f);
	}
	return true;
}

void ABattleUnit::MountWeapon(UStaticMeshComponent* Model, USceneComponent* Parent, EBattleWeapon Weapon, bool bFirstPerson) const
{
	// No anti-tank or mortar model yet: the support gun model, larger.
	const bool Support = Weapon == EBattleWeapon::SupportGun || Weapon == EBattleWeapon::AntiTank || Weapon == EBattleWeapon::Mortar;
	Model->SetStaticMesh(Support ? SupportModel : CarbineModel);
	Model->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bFirstPerson ? NAME_None : FName(TEXT("hand_r")));
	Model->SetRelativeScale3D(FVector(Weapon == EBattleWeapon::AntiTank || Weapon == EBattleWeapon::Mortar ? 1.35 : Support ? 1.0 : .62));
	Model->SetRelativeRotation(FRotator(0,90,0));
	Model->SetRelativeLocation(FVector(47,15,-16));
	if (!bFirstPerson)
	{
		// Grip calibration against the bundled rifle idle pose. The simplified
		// Quinn mesh has hand_r but omits the animation skeleton weapon_r bone.
		const FTransform Hand(FRotator(-11.885756,-81.776576,16.174298),FVector(-16.823813,7.097377,139.904362));
		const FTransform Gun(FRotator(0,180,0),FVector(-14.9,29.,143.5));
		const FTransform Grip = Gun.GetRelativeTransform(Hand);
		Model->SetRelativeLocationAndRotation(Grip.GetLocation(),Grip.GetRotation());
	}
}

FVector ABattleUnit::TracerStart(const UStaticMeshComponent* Model, const FVector& Eye, const FVector& Direction)
{
	// No visible model (far soldiers, hidden bodies, legacy visuals): about where a shouldered rifle ends.
	const FVector Forward = Direction.GetSafeNormal();
	const FVector Guess = Eye + Forward * 70.f + FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal() * 12.f - FVector(0, 0, 10);
	const UStaticMesh* Mesh = Model && Model->IsVisible() ? Model->GetStaticMesh() : nullptr;
	if (!Mesh) return Guess;
	// The weapon models point along their local -Y (see MountWeapon). The muzzle is the middle of the
	// vertices at that end; without vertex data on the CPU, the middle of that face of the bounding box.
	static TMap<TWeakObjectPtr<const UStaticMesh>, FVector> Muzzles;
	const FVector* Local = Muzzles.Find(Mesh);
	if (!Local)
	{
		const FBox Box = Mesh->GetBoundingBox();
		FVector Tip(Box.GetCenter().X, Box.Min.Y, Box.GetCenter().Z), Sum = FVector::ZeroVector; int32 Count = 0;
		if (const FStaticMeshRenderData* Data = Mesh->GetRenderData(); Data && Data->LODResources.Num())
		{
			const FPositionVertexBuffer& Positions = Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
			if (Positions.GetAllowCPUAccess() && Positions.GetVertexData())
				for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
					if (const FVector P(Positions.VertexPosition(I)); P.Y <= Box.Min.Y + 1.) { Sum += P; ++Count; }
		}
		if (Count) Tip = FVector(Sum.X / Count, Box.Min.Y, Sum.Z / Count);
		Local = &Muzzles.Add(Mesh, Tip);
	}
	const FVector Muzzle = Model->GetComponentTransform().TransformPosition(*Local);
	// Only a muzzle ahead of the eye and near it: a body whose pose has not been evaluated (never rendered yet)
	// or is not a firing pose holds the model elsewhere, and a tracer must not start at the feet.
	const FVector Off = Muzzle - Eye;
	return FVector::DotProduct(Off, Forward.GetSafeNormal2D()) >= 30. && Off.SizeSquared() <= FMath::Square(140.) ? Muzzle : Guess;
}

void ABattleUnit::UpdateWeaponPose(const FBattleUnitState& S)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Duration = GetDefault<UBattleTuning>()->ReloadTime(S.Weapon);
	const bool Reloading = S.ReloadEndTime > Now;
	const bool Moving = GetVelocity().SizeSquared2D() > 400.;
	auto* Pose = Reloading ? RifleReload.Get() : Moving ? RifleWalk.Get() : RifleIdle.Get();
	// Stance visuals trial: a crouched soldier plays the crouch clip (reloading included: there is no crouched reload clip).
	if (auto* Crouch = VisualStance == BattleStance::Crouch ? BattleStance::CrouchClip(Moving) : nullptr) Pose = Crouch;
	const bool Looping = !Reloading || Pose != RifleReload.Get();
	if (Pose && CurrentPose != Pose)
	{
		CurrentPose = Pose;
		GetMesh()->PlayAnimation(Pose,Looping);
		GetMesh()->SetPlayRate(!Looping ? Pose->GetPlayLength()/Duration : 1.f);
		if (!Looping) GetMesh()->SetPosition(Pose->GetPlayLength()*FMath::Clamp(1.-(S.ReloadEndTime-Now)/Duration,0.,1.), false);
	}
	if (IsPlayerControlled() && Rifle->GetAttachParent()==FirstPersonCamera)
	{
		const double Kick = FMath::Clamp(1.-(Now-LastVisualShot)/.12,0.,1.);
		const double Dip = Reloading ? FMath::Sin(PI*FMath::Clamp((S.ReloadEndTime-Now)/Duration,0.,1.)) : 0.;
		Rifle->SetRelativeLocation(FVector(47-4*Kick,15,-16-6*Dip));
		Rifle->SetRelativeRotation(FRotator(0,90,4*Kick-25*Dip));
	}
}

void ABattleUnit::SetDirectControl(bool bDirect)
{
	auto* Movement = GetCharacterMovement();
	if (bDirect && !bDirectAvoidanceOverride)
	{
		SavedAvoidanceWeight = Movement->GetRVOAvoidanceWeight();
		// UE skips CalcAvoidanceVelocity at weight 1, but still publishes this
		// participant so AI soldiers can avoid the player. Physical collision remains.
		Movement->SetRVOAvoidanceWeight(1.f);
		bDirectAvoidanceOverride = true;
	}
	else if (!bDirect && bDirectAvoidanceOverride)
	{
		Movement->SetRVOAvoidanceWeight(SavedAvoidanceWeight);
		bDirectAvoidanceOverride = false;
	}
	SetAnimationDetail(bDirect);
	GetWorld()->GetSubsystem<UBattleState>()->SetAction(UnitID, bDirect ? EBattleAction::DirectControl : EBattleAction::Idle);
	bUseControllerRotationYaw = bDirect;
	GetCharacterMovement()->bOrientRotationToMovement = !bDirect;
	const bool bFirstPerson = bDirect && !bThirdPersonView;
	AimLevel = 0; AimZoom = 1.f; ScopeAlpha = 0.f; bScopeHidden = false; Rifle->SetOwnerNoSee(false);
	GetMesh()->SetOwnerNoSee(bFirstPerson);
	if (bWeaponVisuals && GetState()) MountWeapon(Rifle, bFirstPerson ? static_cast<USceneComponent*>(FirstPersonCamera) : GetMesh(), GetState()->Weapon, bFirstPerson);
	TeamLabel->SetVisibility(false);
}

void ABattleUnit::SetAnimationDetail(bool bDirect)
{
	// UE uses projected size and interpolation for remote bodies. Physics, senses and weapons keep their own clocks.
	const bool bOptimize = !bDirect && !FParse::Param(FCommandLine::Get(), TEXT("BattleNoAnimationLOD"));
	GetMesh()->bEnableUpdateRateOptimizations = bOptimize;
	if (GetMesh()->AnimUpdateRateParams)
	{
		// Keep full detail for large on-screen bodies; small RTS silhouettes can
		// evaluate less often. UE staggers updates and interpolates the nearer tiers.
		// Keep the previous policy available for same-binary performance comparisons.
		const bool bLegacy = FParse::Param(FCommandLine::Get(), TEXT("BattleLegacyAnimationLOD"));
		GetMesh()->AnimUpdateRateParams->BaseVisibleDistanceFactorThesholds = bLegacy
			? TArray<float>{ .24f, .12f } : TArray<float>{ .24f, .12f, .06f, .03f };
	}
	// Gameplay uses capsule/actor transforms, not animation bones or root motion.
	// Hidden bodies can stop evaluating the animation graph without stopping combat.
	GetMesh()->VisibilityBasedAnimTickOption = bOptimize ? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered : EVisibilityBasedAnimTickOption::AlwaysTickPose;
}

void ABattleUnit::SetRevealed(bool bVisible)
{
	bRevealed = bVisible;
	// bRevealed remains sensor knowledge for HUD/AI. TPS body rendering is independent.
	const auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	SetActorHiddenInGame(!(bVisible || (PC && PC->ShowsUnspottedBodies())));
}

void ABattleUnit::ApplyDeath()
{
	if (bDeathApplied) return;
	bDeathApplied = true;
	GetWorld()->GetSubsystem<UBattleSquadAI>()->ReleaseUnit(UnitID);
	GetWorld()->GetSubsystem<UBattleCover>()->Release(UnitID);
	GetWorld()->GetSubsystem<UBattleState>()->SetTacticalState(UnitID, INDEX_NONE, TEXT("UNIT DOWN"));
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetMesh()->SetRelativeRotation(FRotator(0, -90, 85));
	GetMesh()->SetRelativeLocation(FVector(0, 0, -65));
	TeamLabel->SetText(FText::FromString(TEXT("DOWN")));
	TeamLabel->SetTextRenderColor(FColor(140, 140, 140));
	Rifle->SetVisibility(false);
}
