#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattleController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ABattleUnit::ABattleUnit()
{
	PrimaryActorTick.bCanEverTick = true;
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
	GetCharacterMovement()->AvoidanceConsiderationRadius = 350.f;
	bUseControllerRotationYaw = false;
	AIControllerClass = ABattleAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled;
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetRootComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0, 0, 65));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = 90.f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
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
	if (const FBattleUnitState* S = GetState())
	{
		TeamLabel->SetText(FText::FromString(FString::Printf(TEXT("%s %02d"), S->Team == 0 ? TEXT("ALLY") : TEXT("HOSTILE"), ID)));
		TeamLabel->SetTextRenderColor(S->Team == 0 ? FColor(75, 230, 195) : FColor(255, 90, 70));
	}
	SpawnDefaultController();
	SavedAI = Cast<AAIController>(GetController());
}

const FBattleUnitState* ABattleUnit::GetState() const
{
	return GetWorld()->GetSubsystem<UBattleState>()->Find(UnitID);
}

bool ABattleUnit::IsAlive() const { const auto* S = GetState(); return S && S->IsAlive(); }
FVector ABattleUnit::EyePosition() const { return GetActorLocation() + FVector(0, 0, 60); }

void ABattleUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GetState() && !IsAlive()) ApplyDeath();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager) TeamLabel->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation() - TeamLabel->GetComponentLocation()).Rotation());
	}
}

bool ABattleUnit::CanSee(const ABattleUnit* Other) const
{
	if (!Other || !Other->IsAlive() || !IsAlive()) return false;
	if (FVector::DistSquared(GetActorLocation(), Other->GetActorLocation()) > FMath::Square(GetDefault<UBattleTuning>()->SightRange)) return false;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleSight), false, this);
	GetWorld()->LineTraceSingleByChannel(Hit, EyePosition(), Other->GetActorLocation() + FVector(0, 0, 35), ECC_Visibility, Query);
	return !Hit.bBlockingHit || Hit.GetActor() == Other;
}

bool ABattleUnit::FireAt(FVector Start, FVector Direction, bool bPlayerShot)
{
	const UBattleTuning* Tuning = GetDefault<UBattleTuning>();
	UBattleState* Store = GetWorld()->GetSubsystem<UBattleState>();
	if (Direction.IsNearlyZero() || !Store->ConsumeShot(UnitID, GetWorld()->GetTimeSeconds(), bPlayerShot ? Tuning->PlayerShotInterval : Tuning->AIShotInterval)) return false;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleShot), false, this);
	const FVector End = Start + Direction.GetSafeNormal() * Tuning->WeaponRange;
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
	if (ABattleUnit* Victim = Cast<ABattleUnit>(Hit.GetActor()))
	{
		if (Store->ApplyDamage(Victim->GetUnitID(), GetState()->Team, Tuning->ShotDamage))
		{
			if (bPlayerShot) if (ABattleController* PC = Cast<ABattleController>(GetController())) PC->HitMarkerUntil = GetWorld()->GetTimeSeconds() + .18;
			if (!Victim->IsAlive()) Victim->ApplyDeath();
		}
	}
	if (bRevealed)
	{
		const FVector TracerStart = bPlayerShot ? Start + Direction.GetSafeNormal() * 70.f + FirstPersonCamera->GetRightVector() * 12.f - FVector(0, 0, 10) : Start;
		DrawDebugLine(GetWorld(), TracerStart, Hit.bBlockingHit ? Hit.ImpactPoint : End,
			GetState()->Team == 0 ? FColor(100, 245, 230) : FColor(255, 120, 65), false, .09f, 0, 2.f);
		if (Hit.bBlockingHit) DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10, FColor::Yellow, false, .12f);
	}
	return true;
}

void ABattleUnit::SetDirectControl(bool bDirect)
{
	bUseControllerRotationYaw = bDirect;
	GetCharacterMovement()->bOrientRotationToMovement = !bDirect;
	GetMesh()->SetOwnerNoSee(bDirect);
	TeamLabel->SetVisibility(false);
}

void ABattleUnit::SetRevealed(bool bVisible)
{
	bRevealed = bVisible;
	SetActorHiddenInGame(!bVisible);
}

void ABattleUnit::ApplyDeath()
{
	if (bDeathApplied) return;
	bDeathApplied = true;
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

ABattleAIController::ABattleAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .1f;
	bAttachToPawn = true;
}

void ABattleAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ABattleUnit* Unit = Cast<ABattleUnit>(GetPawn());
	ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Unit || !Unit->IsAlive() || !Mode || Mode->bCombatPaused || Mode->bBattleEnded) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < Mode->CombatStartsAt) return;
	if (Now < NextDecision) return;
	NextDecision = Now + .2;
	const FBattleUnitState* S = Unit->GetState();
	if (S->Order == EBattleOrder::Move)
	{
		if (FVector::DistSquared2D(Unit->GetActorLocation(), S->Destination) < FMath::Square(90.f))
		{
			GetWorld()->GetSubsystem<UBattleState>()->SetOrder(Unit->GetUnitID(), EBattleOrder::Hold, S->Destination);
			StopMovement();
		}
		else if (Now >= NextMoveRequest)
		{
			MoveToLocation(S->Destination, 55.f, true, true, false, true, nullptr, false);
			NextMoveRequest = Now + 1.;
		}
	}
	ABattleUnit* Target = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (ABattleUnit* Candidate : Mode->Units)
	{
		if (!Candidate || !Candidate->IsAlive() || Candidate->GetState()->Team == S->Team) continue;
		const float Distance = FVector::DistSquared(Unit->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < Best && Unit->CanSee(Candidate)) { Best = Distance; Target = Candidate; }
	}
	if (Target)
	{
		SetFocus(Target);
		if (S->Ammo == 0) GetWorld()->GetSubsystem<UBattleState>()->StartReload(Unit->GetUnitID(), Now);
		else Unit->FireAt(Unit->EyePosition(), Target->GetActorLocation() + FVector(0, 0, 35) - Unit->EyePosition(), false);
	}
	else ClearFocus(EAIFocusPriority::Gameplay);
}
