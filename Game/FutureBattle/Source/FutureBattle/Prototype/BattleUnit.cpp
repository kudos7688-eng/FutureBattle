#include "BattleUnit.h"
#include "UObject/StrongObjectPtr.h"
#include "BattleLeaderRuntime.h"
#include "BattleMuzzleFlash.h"
#include "BattleAudio.h"
#include "Animation/AnimSequence.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "BattleTactics.h"
#include "BattleRanges.h"
#include "BattleCover.h"
#include "BattleStance.h"
#include "BattleBlindFire.h"
#include "BattleBodyAnim.h"
#include "BattleSoldierBody.h"
#include "BattleSquadAI.h"
#include "BattleMetrics.h"
#include "BattleMatch.h"
#include "NavMesh/RecastNavMesh.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "BattleRagdoll.h"
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
	// Trial -BattleSoldierBody=1 (BattleSoldierBody.h): the RB01 body on the same skeleton. The exo look replaces it afterwards.
	if (BattleSoldierBody::Version() == 1) GetMesh()->SetSkeletalMesh(BattleSoldierBody::LoadMesh());
	FString TrialVariant;
	bWeaponVisuals = !FParse::Param(FCommandLine::Get(), TEXT("BattleLegacyWeaponVisuals"));
	bTrialPose = FParse::Value(FCommandLine::Get(), TEXT("BattleSquadTrial="), TrialVariant);
	if (bWeaponVisuals && GetState()) MountWeapon(Rifle, GetMesh(), GetState()->ActiveWeapon(), false);
	// Body animation (BattleBodyAnim.h; -BattleBodyAnim=0 keeps the single whole-body clip).
	if (bWeaponVisuals && !bTrialPose && BattleBodyAnim::Enabled() && BattleBodyAnim::Get(BattleBodyAnim::EClip::StandIdle))
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(UBattleBodyAnimInstance::StaticClass());
	}
	SpawnDefaultController();
	SavedAI = Cast<AAIController>(GetController());
}

const FBattleUnitState* ABattleUnit::GetState() const
{
	return GetWorld()->GetSubsystem<UBattleState>()->Find(UnitID);
}

bool ABattleUnit::IsAlive() const { const auto* S = GetState(); return S && S->IsAlive(); }
// The eye sinks with the stance (BattleStance::EyeOffset, as the runtime's sight and shots); the capsule keeps its height.
FVector ABattleUnit::EyePosition() const { return GetActorLocation() + FVector(0, 0, BattleStance::EyeOffset(VisualStance)); }

// Closer than this to the eye the rear camera is inside the head or the chest.
static constexpr float CameraInBodyDistance = 70.f;

void ABattleUnit::CalcCamera(float DeltaTime, FMinimalViewInfo& View)
{
	Super::CalcCamera(DeltaTime, View);
	bCameraInBody = false;
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
	bCameraInBody = Hit.bBlockingHit && FVector::DistSquared(Hit.Location, Pivot) < FMath::Square(CameraInBodyDistance);
}

static const float Magnification[] = { 1.f, 1.6f, 4.f, 8.f };

float ABattleUnit::AimSpreadScale() const
{
	// Only once the view has (nearly) reached the level: a tap of the button gives nothing.
	if (!AimLevel || !IsPlayerControlled() || AimZoom < Magnification[AimLevel] * .9f) return 1.f;
	const UBattleTuning* Tuning = GetDefault<UBattleTuning>();
	return FMath::Clamp(AimLevel == 1 ? Tuning->AimSpreadScale : AimLevel == 2 ? Tuning->Scope4SpreadScale : Tuning->Scope8SpreadScale, .05f, 1.f);
}

float ABattleUnit::PlayerSpreadScale() const
{
	// Only a soldier a human really controls: test shots flagged as player shots on AI soldiers keep the common cone.
	return IsPlayerControlled() ? FMath::Clamp(GetDefault<UBattleTuning>()->PlayerSpreadScale, .05f, 1.f) * AimSpreadScale() : 1.f;
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
	const bool Hide = ScopeView() || (bThirdPersonView && bCameraInBody);
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
	// While the camera is still sinking after a stance change, the shot already leaves no higher than the new eye (the
	// enemy's sight of him has changed at once), so getting down never fires over a wall that now hides him.
	Start.Z = FMath::Min(Start.Z, GetActorLocation().Z + 65.f + BattleStance::EyeOffset(VisualStance) - BattleStance::EyeOffset(BattleStance::Stand));
	Direction = GetControlRotation().Vector();
	if (!bThirdPersonView) return;
	FMinimalViewInfo View; CalcCamera(0.f, View);
	// Aim to wherever the bullet can reach (range profile: further than the AI engagement distance).
	const FVector End = View.Location + View.Rotation.Vector() * BattleRanges::Reach(GetState()->bLauncherSelected ? EBattleWeapon::AntiTank : GetState()->Weapon);
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
	// Stances: the camera (view, held rifle and the player's shot start) sinks with the body over about the stance change.
	{
		const float Target = BattleStance::EyeOffset(VisualStance) - BattleStance::EyeOffset(BattleStance::Stand);
		const float Drop = FMath::FInterpConstantTo(CameraStanceDrop, Target, DeltaTime, 120.f);
		if (Drop != CameraStanceDrop) { FirstPersonCamera->AddRelativeLocation(FVector(0, 0, Drop - CameraStanceDrop)); CameraStanceDrop = Drop; }
	}
	if (const auto* State = GetState())
	{
		if (State->IsAlive()) GetWorld()->GetSubsystem<UBattleState>()->UpdateAim(UnitID, GetWorld()->GetTimeSeconds(), GetVelocity().Size2D(), GetBaseAimRotation().Vector(), GetCharacterMovement()->IsMovingOnGround());
		else ApplyDeath();
		if (State->IsAlive() && bWeaponVisuals && !bTrialPose) UpdateWeaponPose(*State);
	}
	if (RagdollStage < 2 && GetWorld()->GetTimeSeconds() >= RagdollAt)
	{
		// The death clip ends bent over, still standing: the body falls the rest of the way under physics, then holds.
		if (RagdollStage == 0 && !bCorpseHidden && BattleRagdoll::Start(GetMesh())) { RagdollStage = 1; RagdollAt = GetWorld()->GetTimeSeconds() + BattleRagdoll::SettleSeconds; }
		else { BattleRagdoll::Settle(GetMesh()); RagdollStage = 2; }
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
	Direction = BattleTactics::ShotDirection(*GetState(), Direction, GetWorld()->GetTimeSeconds(), bPlayerShot ? PlayerSpreadScale() : 1.f);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleShot), false, this);
	const FVector End = Start + Direction.GetSafeNormal() * BattleRanges::Reach(GetState()->Weapon);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
	if (ABattleUnit* Victim = Cast<ABattleUnit>(Hit.GetActor()))
	{
		if (Store->ApplyDamageFrom(Victim->GetUnitID(), GetState()->Team, Tuning->ShotDamage, Start, UnitID, EBattleKillKind::Shot))
		{
			if (Metrics->bRecording) ++Metrics->Hits;
			if (bPlayerShot) if (ABattleController* PC = Cast<ABattleController>(GetController())) PC->HitMarkerUntil = GetWorld()->GetTimeSeconds() + .18;
			if (!Victim->IsAlive()) Victim->ApplyDeath();
		}
	}
	// Sound only (2026-11-22), as in UBattleLeaderRuntime::Fire.
	{
		const FVector HitAt = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : End;
		BattleAudio::Shot(GetWorld(), Start, GetState()->Weapon == EBattleWeapon::SupportGun ? BattleAudio::EShot::Heavy : BattleAudio::EShot::Rifle, bPlayerShot);
		if (Hit.bBlockingHit) BattleAudio::Impact(GetWorld(), HitAt, Cast<ABattleUnit>(Hit.GetActor()) != nullptr);
		BattleAudio::ShotPassing(GetWorld(), Start, HitAt, GetState()->Team, Cast<ABattleUnit>(Hit.GetActor()) ? Cast<ABattleUnit>(Hit.GetActor())->GetUnitID() : INDEX_NONE);
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

void ABattleUnit::SetLook(UAnimSequence* Idle, UAnimSequence* Move, UAnimSequence* CrouchIdle, UAnimSequence* CrouchMove, UStaticMesh* Weapon, FName Bone, const FTransform& Grip)
{
	LookIdle = Idle; LookMove = Move; LookCrouchIdle = CrouchIdle; LookCrouchMove = CrouchMove;
	LookWeapon = Weapon; LookBone = Bone; LookGrip = Grip;
	CurrentPose = nullptr;
	if (!Idle)
	{
		LookFire = LookReload = LookSequenceFirst = LookSequenceThen = nullptr;
		// The look is gone and UpdateLookTilt no longer runs: drop its lean so the plain body stands upright.
		LookVelShown = LookShotVelocity = FVector::ZeroVector;
		if (!MeshTiltShown.Equals(FQuat::Identity, 1.e-4f))
		{
			MeshTiltShown = FQuat::Identity;
			GetMesh()->SetRelativeRotation(FRotator(0, -90 + MeshYawShown, 0));
		}
	}
	const bool bFirstPerson = IsPlayerControlled() && !bThirdPersonView;
	if (bWeaponVisuals && GetState()) MountWeapon(Rifle, bFirstPerson ? static_cast<USceneComponent*>(FirstPersonCamera) : GetMesh(), GetState()->ActiveWeapon(), bFirstPerson);
}

void ABattleUnit::RemountWeapon()
{
	const bool bFirstPerson = IsPlayerControlled() && !bThirdPersonView;
	if (bWeaponVisuals && GetState()) MountWeapon(Rifle, bFirstPerson ? static_cast<USceneComponent*>(FirstPersonCamera) : GetMesh(), GetState()->ActiveWeapon(), bFirstPerson);
}

void ABattleUnit::PlayArmsClip(UAnimSequence* Clip)
{
	if (auto* Body = Cast<UBattleBodyAnimInstance>(GetMesh()->GetAnimInstance()); Body && Clip) Body->PlayOverlay(Clip, 1.f);
}

float ABattleUnit::PlayLookSequence(UAnimSequence* First, UAnimSequence* Then, double Time)
{
	LookSequenceFirst = First; LookSequenceThen = First ? Then : nullptr; LookSequenceStart = Time;
	CurrentPose = nullptr;  // starts over even when the same clip is showing
	return (First ? First->GetPlayLength() : 0.f) + (LookSequenceThen ? LookSequenceThen->GetPlayLength() : 0.f);
}

bool ABattleUnit::IsPlayingLookSequence(double Time) const
{
	if (!LookSequenceFirst) return false;
	const double Length = LookSequenceFirst->GetPlayLength() + (LookSequenceThen ? LookSequenceThen->GetPlayLength() : 0.f);
	return Time >= LookSequenceStart && Time < LookSequenceStart + Length;
}

bool ABattleUnit::LookHasProneClip() const
{
	if (!LookIdle || !BattleStance::AnimEnabled()) return false;
	const UAnimSequence* Prone = BattleBodyAnim::Get(BattleBodyAnim::EClip::ProneIdle);
	return Prone && Prone->GetSkeleton() == LookIdle->GetSkeleton();
}

void ABattleUnit::MountWeapon(UStaticMeshComponent* Model, USceneComponent* Parent, EBattleWeapon Weapon, bool bFirstPerson) const
{
	if (LookWeapon)
	{
		// The look's own model: origin at its grip, barrel along local -Y like the other weapon models.
		Model->SetStaticMesh(LookWeapon);
		Model->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bFirstPerson ? NAME_None : LookBone);
		Model->SetRelativeScale3D(FVector(1.));
		if (bFirstPerson) Model->SetRelativeLocationAndRotation(FVector(47,15,-16), FRotator(0,90,0));
		else Model->SetRelativeLocationAndRotation(LookGrip.GetLocation(), LookGrip.GetRotation());
		return;
	}
	// No anti-tank or mortar model yet: the support gun model, larger.
	const bool Support = Weapon == EBattleWeapon::SupportGun || Weapon == EBattleWeapon::AntiTank || Weapon == EBattleWeapon::Mortar || Weapon == EBattleWeapon::Shotgun;
	// Weapons added 2026-11-22 (BattleArms.cpp) use existing models: the exoskeleton's rifle for the marksman rifle and its launcher,
	// smaller, for the grenade launcher; the carbine model shortened for the pistol and SMG, the support model shortened for the shotgun.
	static TStrongObjectPtr<UStaticMesh> AC01Rifle, AC01Launcher; static bool bAC01Loaded = false;
	if (!bAC01Loaded && (Weapon == EBattleWeapon::Marksman || Weapon == EBattleWeapon::Sniper || Weapon == EBattleWeapon::GrenadeLauncher))
	{
		bAC01Loaded = true;
		AC01Rifle.Reset(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/AC01T2/Weapons/SM_AC01_Rifle.SM_AC01_Rifle"), nullptr, LOAD_NoWarn));
		AC01Launcher.Reset(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/AC01T2/Weapons/SM_AC01_Launcher.SM_AC01_Launcher"), nullptr, LOAD_NoWarn));
	}
	UStaticMesh* Own = Weapon == EBattleWeapon::Marksman || Weapon == EBattleWeapon::Sniper ? AC01Rifle.Get() : Weapon == EBattleWeapon::GrenadeLauncher ? AC01Launcher.Get() : nullptr;
	Model->SetStaticMesh(Own ? Own : Support ? SupportModel.Get() : CarbineModel.Get());
	Model->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bFirstPerson ? NAME_None : FName(TEXT("hand_r")));
	const double Scale = Weapon == EBattleWeapon::AntiTank || Weapon == EBattleWeapon::Mortar ? 1.35 : Weapon == EBattleWeapon::Shotgun ? .7 :
		Weapon == EBattleWeapon::Pistol ? .3 : Weapon == EBattleWeapon::SMG ? .45 : Weapon == EBattleWeapon::Marksman ? (Own ? .85 : .8) :
		Weapon == EBattleWeapon::GrenadeLauncher ? (Own ? .55 : .8) : Weapon == EBattleWeapon::Sniper ? (Own ? 1.0 : .7) : Support ? 1.0 : .62;
	// The sniper rifle: the same rifle model with a longer barrel (the models point along their local -Y).
	Model->SetRelativeScale3D(Weapon == EBattleWeapon::Sniper ? FVector(Scale, Scale * 1.3, Scale) : FVector(Scale));
	Model->SetRelativeRotation(FRotator(0,90,0));
	Model->SetRelativeLocation(FVector(47,15,-16));
	if (!bFirstPerson)
	{
		// Grip calibration against the bundled rifle idle pose. The simplified
		// Quinn mesh has hand_r but omits the animation skeleton weapon_r bone.
		const FTransform Grip = BattleBodyAnim::RifleGrip();
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

UBattleBodyAnimInstance* ABattleUnit::BodyAnim() const
{
	return LookIdle ? nullptr : Cast<UBattleBodyAnimInstance>(GetMesh()->GetAnimInstance());
}

void ABattleUnit::SetMeshLean(const FVector& WorldOffset, float DeltaSeconds)
{
	const FVector Target = GetActorTransform().InverseTransformVectorNoScale(WorldOffset);
	if (Target.Equals(MeshLeanShown, .1)) return;
	const FVector Base = GetMesh()->GetRelativeLocation() - MeshLeanShown;
	// About a quarter of a second to the edge of a 2 m lean and back.
	MeshLeanShown = FMath::VInterpConstantTo(MeshLeanShown, Target, DeltaSeconds, 900.f);
	GetMesh()->SetRelativeLocation(Base + MeshLeanShown);
}

void ABattleUnit::SetMeshYaw(float Yaw)
{
	if (FMath::IsNearlyEqual(Yaw, MeshYawShown)) return;
	MeshYawShown = Yaw;
	GetMesh()->SetRelativeRotation(MeshTiltShown * FRotator(0, -90 + Yaw, 0).Quaternion());
}

void ABattleUnit::UpdateLookTilt(bool bUpright)
{
	const float Dt = GetWorld()->GetDeltaSeconds();
	FVector Velocity = LookShotVelocity.IsNearlyZero() ? GetActorRotation().UnrotateVector(GetVelocity()) : LookShotVelocity;
	Velocity.Z = 0.;
	if (LookVelShown.ContainsNaN() || Velocity.ContainsNaN()) LookVelShown = Velocity = FVector::ZeroVector;
	FQuat Tilt = FQuat::Identity;
	if (!bUpright)
	{
		// Crouching, prone or in a stance change: no braking lean (that tipped the body backwards); ease straight back upright.
		LookVelShown = FVector::ZeroVector;
		Tilt = MeshTiltShown.Equals(FQuat::Identity, 1.e-3f) ? FQuat::Identity : FQuat::Slerp(MeshTiltShown, FQuat::Identity, FMath::Clamp(Dt * 8.f, 0.f, 1.f));
	}
	else
	{
		LookVelShown = FMath::VInterpTo(LookVelShown, Velocity, Dt, 4.f);
		// Degrees per full walking speed: 5 while cruising; starting adds up to 7 more, braking takes 7 back (about 2 back at a stop).
		const float Speed = FMath::Max(GetCharacterMovement()->MaxWalkSpeed, 100.f);
		const FVector Lean = ((LookVelShown * 5.f + (Velocity - LookVelShown) * 7.f) / Speed).GetClampedToMaxSize(10.f);
		const FVector Up(0, 0, 1);
		if (!Lean.IsNearlyZero(.05))
			Tilt = FQuat::FindBetweenNormals(Up, (Up + Lean.GetSafeNormal() * FMath::Tan(FMath::DegreesToRadians(Lean.Size()))).GetSafeNormal());
	}
	if (Tilt.Equals(MeshTiltShown, 1.e-4f)) return;
	MeshTiltShown = Tilt;
	GetMesh()->SetRelativeRotation(MeshTiltShown * FRotator(0, -90 + MeshYawShown, 0).Quaternion());
}

void ABattleUnit::UpdateBodyAnim(UBattleBodyAnimInstance& Body, const FBattleUnitState& S)
{
	using namespace BattleBodyAnim;
	const double Now = GetWorld()->GetTimeSeconds();
	const auto* Leaders = GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
	Body.SetHandRaise(FVector::ZeroVector, 0.f);
	{
		// Leaning out past the edge for a side blind shot: the body centre goes to SideLeanBack behind the shown part.
		FVector Shown, Lean = FVector::ZeroVector;
		if (Leaders && Leaders->SideExposure(UnitID, Now, Shown))
		{
			const FVector Pos = GetActorLocation(), Out = (Shown - Pos).GetSafeNormal2D();
			Lean = Shown - Out * BattleBlindFire::SideLeanBack - Pos; Lean.Z = 0.;
		}
		SetMeshLean(Lean, GetWorld()->GetDeltaSeconds());
	}
	// Lower body: a stance change plays its transition once, then the idle or the walk in the direction of travel.
	// Cross-fade of the walks of a crouch or crawl, and of a stance change on the move (the ordinary 0.2 s cut them short).
	constexpr float StanceBlendSeconds = .35f;
	if (VisualStance != BodyStance)
	{
		// User 11-24: changing stance on the move looked unnatural (the clip played in place while the body slid on).
		// A crouch or stand-up on the move cross-fades straight into the walk of the new stance (StanceBlendSeconds); lying down
		// and getting up from prone play their clip, and the player does not walk meanwhile (SetSprinting). The player's own
		// change ends within the rule's stance lock (0.5 s, 1.2 s to or from prone; UBattleLeaderRuntime::SetStance), so the
		// body is ready when the next change is allowed. Display only.
		const bool bProne = BodyStance == BattleStance::Prone || VisualStance == BattleStance::Prone;
		const bool bMoving = GetVelocity().SizeSquared2D() > 400.;
		TransitionClip = bMoving && !bProne ? nullptr : Transition(BodyStance, VisualStance);
		// On the move a prone change keeps the hurried clip below (MovingTransitionSeconds); from standing still the player waits.
		TransitionRate = TransitionClip && IsPlayerControlled() && !bMoving ? FMath::Max(1.f, float(TransitionClip->GetPlayLength() / (bProne ? 1.2 : .5))) : 1.f;
		TransitionUntil = TransitionClip ? Now + TransitionClip->GetPlayLength() / TransitionRate : 0.;
		bTransitionProne = TransitionClip && bProne && !bMoving;
		StanceBlendUntil = TransitionClip ? 0. : Now + StanceBlendSeconds;
		BodyStance = VisualStance;
	}
	// Cover clips (BattleCoverClips.h): hidden and firing poses of the cover action, with a one-shot between phases. The table
	// is picked once per cover action. While a cover clip shows, it is the whole body (reload still layers on the upper body).
	if (S.CoverPhase != ShownCoverPhase)
	{
		using namespace BattleCoverClips;
		if (!ShownCoverPhase && S.CoverPhase) CoverClipSet = CoverSet(GetWorld(), S, Leaders ? Leaders->CoverFacing(UnitID, GetActorForwardVector()) : GetActorForwardVector());
		const TCHAR* Shot = nullptr; float ShotYaw = 0.f;
		if (S.CoverPhase == 2 && ShownCoverPhase == 1) { Shot = CoverClipSet.Enter; ShotYaw = YawOffset(2); }
		else if (S.CoverPhase == 4) { Shot = CoverClipSet.ToAim; ShotYaw = YawOffset(4); }
		else if (ShownCoverPhase == 4 && S.CoverPhase && BattleBlindFire::IsOverTop(S)) { Shot = CoverClipSet.FromAim; ShotYaw = YawOffset(2); }
		CoverOneShot = S.CoverPhase ? CoverClip(Shot) : nullptr;
		CoverOneShotUntil = CoverOneShot ? Now + CoverOneShot->GetPlayLength() : 0.;
		CoverOneShotYaw = ShotYaw;
		ShownCoverPhase = S.CoverPhase;
	}
	{
		UAnimSequence* CoverBase = nullptr; bool bCoverLoop = true; float Yaw = 0.f;
		// Blind fire: around the edge the clip is a crouched aim of the whole body. Over the top the clip stands with the rifle
		// above the head: the whole body shows it while the head is exposed (the rules aim at and hit the head then,
		// BattleBlindFire::ExposeEnabled); with -BattleBlindFireExpose=0 only its upper body shows over the hidden pose and the
		// hands lift the rifle over the cover, the head staying below it.
		UAnimSequence* Blind = S.CoverPhase && Leaders && Leaders->IsBlindFiring(UnitID, Now) ? BattleBlindFire::Clip(BattleBlindFire::IsOverTop(S)) : nullptr;
		UAnimSequence* BlindUpper = nullptr;
		if (Blind && BattleBlindFire::IsOverTop(S) && BattleBlindFire::ExposeEnabled()) CoverBase = Blind;
		else if (Blind && BattleBlindFire::IsOverTop(S) && CoverClip(CoverClipSet.Hidden)) { CoverBase = CoverClip(CoverClipSet.Hidden); BlindUpper = Blind; Yaw = BattleBlindFire::OverTopYaw(CoverClipSet.Hidden); }
		// Around the edge: the side's own crouched aim (AimL or AimR of the cover set), else the default side clip.
		// The body turns so the rifle points around the corner (BlindFireLook), not into the cover.
		else if (Blind) { CoverBase = CoverClip(CoverClipSet.Aim) ? CoverClip(CoverClipSet.Aim) : Blind; Yaw = Leaders->BlindFireYaw(UnitID, GetActorForwardVector()); }
		else if (CoverOneShot && Now < CoverOneShotUntil) { CoverBase = CoverOneShot; bCoverLoop = false; Yaw = CoverOneShotYaw; }
		else if (S.CoverPhase == 2) { CoverBase = CoverClip(CoverClipSet.Hidden); Yaw = BattleCoverClips::YawOffset(2); }
		else if (S.CoverPhase == 4) { CoverBase = CoverClip(CoverClipSet.Aim); Yaw = BattleCoverClips::YawOffset(4); }
		if (CoverBase)
		{
			// The player's wall cover (BattleWallCover.cpp): the hidden pose stays against the wall while the camera turns the body.
			if (S.CoverPhase != 4 && !Blind && Leaders) Yaw += Leaders->WallCoverYaw(UnitID, GetActorForwardVector());
			SetMeshYaw(Yaw);
			// Over the top: the hands lift the rifle above the cover (two-bone IK) while the body stays crouched behind it.
			// Target: 12 cm over the measured cover top, 30 cm ahead of the body toward the shot (display only).
			if (BlindUpper)
			{
				const FVector Pos = GetActorLocation();
				const FVector* Look = Leaders->BlindFireLook.Find(UnitID);
				const FVector Ahead = Look ? (*Look - Pos).GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
				const float* Top = Leaders->BlindFireTop.Find(UnitID);
				const FVector Target = Pos + Ahead * 30.f + FVector(0, 0, (Top ? *Top - Pos.Z : 45.f) + 12.f);
				Body.SetHandRaise(GetMesh()->GetComponentTransform().InverseTransformPosition(Target), 1.f);
			}
			Body.SetBase(CoverBase, bCoverLoop);
			Body.SetAim(0.f, 0.f);
			const float ReloadSeconds = GetDefault<UBattleTuning>()->ReloadTime(S.Weapon);
			UAnimSequence* CoverReload = Get(EClip::Reload);
			if (S.ReloadEndTime > Now && CoverReload) Body.SetUpper(CoverReload, 1.f, CoverReload->GetPlayLength() * FMath::Clamp(1. - (S.ReloadEndTime - Now) / ReloadSeconds, 0., 1.));
			else Body.SetUpper(BlindUpper, BlindUpper ? 1.f : 0.f);
			if (LastVisualShot > ShownShot) ShownShot = LastVisualShot;
			ShownHP = S.HP;
			return;
		}
	}
	SetMeshYaw(0.f);
	const FVector Local = GetActorRotation().UnrotateVector(GetVelocity());
	// Getting up on the move (moving stands the soldier up): the rest of the transition plays fast enough to end within
	// MovingTransitionSeconds instead of sliding along the ground at its own speed.
	const double Hurry = MovingTransitionSeconds(TransitionClip.Get());
	// Walking off in the middle of a crouch or stand-up: it blends into the walk at once instead of sliding through the clip.
	// (MovingTransitionSeconds is 0.4 s for those, 0.6 s for the prone ones.)
	if (TransitionClip && Now < TransitionUntil && Hurry < .5 && Local.SizeSquared2D() > 400.)
	{ TransitionUntil = Now; StanceBlendUntil = Now + StanceBlendSeconds; }
	if (TransitionClip && Now < TransitionUntil && TransitionRate == 1.f && Local.SizeSquared2D() > 400. && TransitionUntil - Now > Hurry)
	{
		TransitionRate = float((TransitionUntil - Now) / Hurry);
		TransitionUntil = Now + Hurry;
	}
	const bool bTransition = TransitionClip && Now < TransitionUntil;
	// Moving without firing (no shot for ReadySeconds, the player not aiming): a jog with the rifle held low, no aim layer.
	// Firing on the move, or aiming, brings the rifle up: the aimed walk and the aim pose.
	const bool bReady = Now - LastVisualShot < ReadySeconds || (IsPlayerControlled() && AimLevel > 0);
	const bool bRun = !bReady && VisualStance == BattleStance::Stand && Local.SizeSquared2D() > 400.;
	UAnimSequence* Walk = Locomotion(VisualStance, Local, bRun);
	// Walking crouched or prone (-BattleStanceMove): the steps keep pace with the body (rate from the clip's own ground speed).
	const float WalkRate = VisualStance != BattleStance::Stand && Local.SizeSquared2D() > 400. ? MatchRate(Walk, float(Local.Size2D())) : 1.f;
	const float Fade = !bTransition && (VisualStance != BattleStance::Stand || Now < StanceBlendUntil) ? StanceBlendSeconds : -1.f;
	Body.SetBase(bTransition ? TransitionClip.Get() : Walk, !bTransition, bTransition ? TransitionRate : WalkRate, Fade);
	// Upper body: reload (synced to the reload timer), blind fire, otherwise the aim pose at the weapon's pitch.
	const float Duration = GetDefault<UBattleTuning>()->ReloadTime(S.Weapon);
	const bool bProne = VisualStance == BattleStance::Prone;
	UAnimSequence* Reload = Get(bProne ? EClip::ProneReload : EClip::Reload);
	if (S.ReloadEndTime > Now && Reload)
		Body.SetUpper(Reload, 1.f, Reload->GetPlayLength() * FMath::Clamp(1. - (S.ReloadEndTime - Now) / Duration, 0., 1.));
	else if (UAnimSequence* Blind = Leaders && Leaders->IsBlindFiring(UnitID, Now) ? BattleBlindFire::Clip(BattleBlindFire::IsOverTop(S)) : nullptr)
		Body.SetUpper(Blind, 1.f);
	else Body.SetUpper(nullptr, 0.f);
	const FVector AimDir = IsPlayerControlled() ? GetControlRotation().Vector() : Now - DisplayAimTime < .5 && !DisplayAim.IsNearlyZero() ? DisplayAim : S.WeaponAimDirection;
	// An AI soldier's weapon direction keeps its last pitch until the aim turns far (UBattleState::UpdateAim): shown only while
	// he fires (BattleBodyAnim::ReadySeconds after his last shot), else the body eases back level instead of staying bent over a stale aim.
	const bool bShowPitch = IsPlayerControlled() || Now - LastVisualShot < ReadySeconds;
	const float WantPitch = bShowPitch ? FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(AimDir.GetSafeNormal().Z, -1., 1.))) : 0.f;
	ShownAimPitch = IsPlayerControlled() ? WantPitch : FMath::FInterpConstantTo(ShownAimPitch, WantPitch, GetWorld()->GetDeltaSeconds(), bShowPitch ? 240.f : 60.f);
	Body.SetAim(ShownAimPitch, bProne || bTransition || bRun ? 0.f : 1.f);
	// One-shot overlays: recoil for each shot, a flinch when hit (from the side the shot came from).
	if (LastVisualShot > ShownShot) { ShownShot = LastVisualShot; if (!bProne) Body.PlayOverlay(Get(EClip::Fire), .55f); }
	// A hit reaction at most every HitReactSeconds: under steady fire each hit restarted the flinch and the body stayed bent over.
	if (ShownHP >= 0.f && S.HP < ShownHP - .01f && !bProne && Now - LastHitReact >= HitReactSeconds)
	{
		LastHitReact = Now;
		Body.PlayOverlay(Hit(GetActorForwardVector(), Leaders ? Leaders->LastShotDirection(UnitID) : FVector::ZeroVector, UnitID + int32(Now * 10.)), 1.f);
	}
	ShownHP = S.HP;
}

void ABattleUnit::UpdateWeaponPose(const FBattleUnitState& S)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Duration = GetDefault<UBattleTuning>()->ReloadTime(S.Weapon);
	const bool Reloading = S.ReloadEndTime > Now;
	const bool Moving = GetVelocity().SizeSquared2D() > 400.;
	if (auto* Body = BodyAnim()) UpdateBodyAnim(*Body, S);
	else {
	auto* Pose = Reloading ? RifleReload.Get() : Moving ? RifleWalk.Get() : RifleIdle.Get();
	// Stance visuals trial: a crouched soldier plays the crouch clip (reloading included: there is no crouched reload clip).
	if (auto* Crouch = VisualStance == BattleStance::Crouch ? BattleStance::CrouchClip(Moving) : nullptr) Pose = Crouch;
	bool bLookTransition = false;
	bool bAction = false, bRestart = false;
	float ActionRate = 1.f; double ActionAt = 0.;
	if (LookIdle)
	{
		const bool Crouched = VisualStance == BattleStance::Crouch && LookCrouchIdle && BattleStance::AnimEnabled();
		Pose = Crouched ? (Moving && LookCrouchMove ? LookCrouchMove.Get() : LookCrouchIdle.Get()) : Moving && LookMove ? LookMove.Get() : LookIdle.Get();
		// Prone with the shared clips: getting down or up plays the transition once, then the prone idle or the crawl in the
		// direction of travel. Stand <-> crouch keeps the look's own clips (no transition).
		if (LookHasProneClip())
		{
			const double ClipNow = GetWorld()->GetTimeSeconds();
			if (VisualStance != BodyStance)
			{
				const bool bProneChange = VisualStance == BattleStance::Prone || BodyStance == BattleStance::Prone;
				TransitionClip = bProneChange ? BattleBodyAnim::Transition(BodyStance, VisualStance) : nullptr;
				TransitionUntil = TransitionClip ? ClipNow + TransitionClip->GetPlayLength() : 0.;
				BodyStance = VisualStance;
			}
			if (TransitionClip && ClipNow < TransitionUntil) { Pose = TransitionClip; bLookTransition = true; }
			else if (VisualStance == BattleStance::Prone)
				if (auto* Prone = BattleBodyAnim::Locomotion(BattleStance::Prone, GetActorRotation().UnrotateVector(GetVelocity()), false)) Pose = Prone;
		}
		// Action clips (SetLookActions, PlayLookSequence), standing only, moving or not (the glide keeps the feet still too):
		// a running sequence (the launcher's recoil, then its reload), else the reload paced by the reload timer, else the recoil
		// of the last shot, from its start at every shot. Each starts and ends on the idle's first frame.
		if (VisualStance == BattleStance::Stand && !bLookTransition)
		{
			const double Since = Now - LookSequenceStart;
			const float First = LookSequenceFirst ? LookSequenceFirst->GetPlayLength() : 0.f;
			const float Then = LookSequenceThen ? LookSequenceThen->GetPlayLength() : 0.f;
			if (LookSequenceFirst && Since >= 0. && Since < First) { Pose = LookSequenceFirst; ActionAt = Since; bAction = true; }
			else if (LookSequenceThen && Since >= First && Since < First + Then) { Pose = LookSequenceThen; ActionAt = Since - First; bAction = true; }
			else if (Reloading && LookReload)
			{
				Pose = LookReload; bAction = true; ActionRate = LookReload->GetPlayLength() / FMath::Max(.1f, Duration);
				ActionAt = LookReload->GetPlayLength() * FMath::Clamp(1. - (S.ReloadEndTime - Now) / FMath::Max(.1f, Duration), 0., 1.);
			}
			else if (LookFire && Now - LastVisualShot < LookFire->GetPlayLength())
			{
				Pose = LookFire; bAction = true; ActionAt = Now - LastVisualShot;
				bRestart = LastVisualShot > LookShotShown; LookShotShown = LastVisualShot;
			}
		}
		UpdateLookTilt(VisualStance == BattleStance::Stand && !bLookTransition);
	}
	// Blind fire (BattleBlindFire.cpp): the clip that raises the rifle over or around the cover while it lasts.
	// Not for a soldier with its own look (other skeleton). Over the top it is a standing clip (the head shows): only while the
	// rules expose the head (BattleBlindFire::ExposeEnabled).
	const bool bOverTop = BattleBlindFire::IsOverTop(S);
	if (!Reloading && !LookIdle && (!bOverTop || BattleBlindFire::ExposeEnabled())) if (const auto* Leaders = GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); Leaders && Leaders->IsBlindFiring(UnitID, Now))
		if (auto* Blind = BattleBlindFire::Clip(bOverTop)) Pose = Blind;
	const bool ReloadPose = Reloading && Pose == RifleReload.Get();
	const bool Looping = !ReloadPose && !bLookTransition && !bAction;
	if (Pose && (CurrentPose != Pose || bRestart))
	{
		CurrentPose = Pose;
		GetMesh()->PlayAnimation(Pose,Looping);
		GetMesh()->SetPlayRate(ReloadPose ? Pose->GetPlayLength()/Duration : bAction ? ActionRate : 1.f);
		if (ReloadPose) GetMesh()->SetPosition(Pose->GetPlayLength()*FMath::Clamp(1.-(S.ReloadEndTime-Now)/Duration,0.,1.), false);
		else if (bAction) GetMesh()->SetPosition(ActionAt, false);
	}
	}
	if (IsPlayerControlled() && Rifle->GetAttachParent()==FirstPersonCamera)
	{
		const double Kick = FMath::Clamp(1.-(Now-LastVisualShot)/.12,0.,1.);
		const double Dip = Reloading ? FMath::Sin(PI*FMath::Clamp((S.ReloadEndTime-Now)/Duration,0.,1.)) : 0.;
		Rifle->SetRelativeLocation(FVector(47-4*Kick,15,-16-6*Dip));
		Rifle->SetRelativeRotation(FRotator(0,90,4*Kick-25*Dip));
	}
}

void ABattleUnit::SetSprinting(bool bRequested)
{
 auto* Movement = GetCharacterMovement();
 const bool bSprint = bRequested && bDirectAvoidanceOverride && IsAlive() && VisualStance == BattleStance::Stand
  && !GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsVehicle(UnitID);
 // -BattleStanceMove (default 1): crouched or prone, the directly controlled soldier walks at that stance's share of his speed
 // (called every frame of direct control, so a stance change takes effect at once).
 const float Low = bDirectAvoidanceOverride && IsAlive() && BattleStance::MoveMode() >= 1 ? BattleStance::MoveScale(VisualStance) : 1.f;
 // Lying down or getting up from prone (user 11-24: walking during it slid the body): no walking until the clip has played
 // (UpdateBodyAnim; at most the 1.2 s stance lock).
 const bool bHold = bDirectAvoidanceOverride && IsAlive() && bTransitionProne && TransitionClip && GetWorld()->GetTimeSeconds() < TransitionUntil;
 const float Scale = bHold ? 0.f : bSprint ? 1.6f : Low;
 if (Scale != 1.f && (WalkSpeedBeforeSprint > 0.f || Movement->MaxWalkSpeed > 0.f))
 {
  if (WalkSpeedBeforeSprint == 0.f) WalkSpeedBeforeSprint = Movement->MaxWalkSpeed;
  if (Movement->MaxWalkSpeed != WalkSpeedBeforeSprint * Scale) Movement->MaxWalkSpeed = WalkSpeedBeforeSprint * Scale;
 }
 else if (Scale == 1.f && WalkSpeedBeforeSprint > 0.f)
 {
  Movement->MaxWalkSpeed = WalkSpeedBeforeSprint;
  WalkSpeedBeforeSprint = 0.f;
 }
}

void ABattleUnit::ReloadIfEmpty()
{
 const auto* State = GetState();
 if (State && State->Ammo == 0 && State->Reserve > 0 && State->ReloadEndTime == 0.)
  GetWorld()->GetSubsystem<UBattleState>()->StartReload(UnitID, GetWorld()->GetTimeSeconds());
}

void ABattleUnit::SetDirectControl(bool bDirect)
{
 if (!bDirect)
 {
  SetSprinting(false);
  // Back to the AI: the full walking speed again (a crouched or prone walk keeps it lowered above).
  if (WalkSpeedBeforeSprint > 0.f) { GetCharacterMovement()->MaxWalkSpeed = WalkSpeedBeforeSprint; WalkSpeedBeforeSprint = 0.f; }
 }
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
	if (bWeaponVisuals && GetState()) MountWeapon(Rifle, bFirstPerson ? static_cast<USceneComponent*>(FirstPersonCamera) : GetMesh(), GetState()->ActiveWeapon(), bFirstPerson);
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
	SetActorHiddenInGame(bCorpseHidden || !(bVisible || (PC && PC->ShowsUnspottedBodies())));
}

void ABattleUnit::HideCorpse(bool bHideActor,bool bStopMeshTick)
{
	if (!bDeathApplied) return;
	bCorpseHidden = true;
	// Both parts default on; -BattleCorpseSteps (BattleCorpses.cpp) leaves one out to find the display->match coupling.
	if (bHideActor) SetActorHiddenInGame(true);
	if (bStopMeshTick) GetMesh()->SetComponentTickEnabled(false);
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
	// Body animation: a death clip chosen by where the killing shot came from, holding its last frame; otherwise the body tips over.
	UBattleBodyAnimInstance* Body = BodyAnim();
	const auto* Leaders = GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
	if (UAnimSequence* Death = Body ? BattleBodyAnim::Death(GetActorForwardVector(), Leaders ? Leaders->LastShotDirection(UnitID) : FVector::ZeroVector, UnitID) : nullptr)
	{
		GetMesh()->SetRelativeScale3D(FVector(1.)); SetMeshYaw(0.f);
		Body->SetUpper(nullptr, 0.f); Body->SetAim(0.f, 0.f); Body->SetBase(Death, false);
		RagdollStage = 0; RagdollAt = GetWorld()->GetTimeSeconds() + Death->GetPlayLength();
	}
	else
	{
		GetMesh()->SetRelativeRotation(FRotator(0, -90, 85));
		GetMesh()->SetRelativeLocation(FVector(0, 0, -65));
	}
	TeamLabel->SetText(FText::FromString(TEXT("DOWN")));
	TeamLabel->SetTextRenderColor(FColor(140, 140, 140));
	Rifle->SetVisibility(false);
}
