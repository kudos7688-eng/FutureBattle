#include "BattleMassTrial.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "ProfilingDebugging/CsvProfiler.h"

ABattleMassTrialUnit::ABattleMassTrialUnit(const FObjectInitializer& Init)
	: Super(Init.SetDefaultSubobjectClass<UBattleTrialMovement>(ACharacter::CharacterMovementComponentName))
{
}

FVector UBattleTrialMovement::PlanStep(float Dt)
{
	AvoidanceLockTimer -= Dt;
	Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(ConsumeInputVector()));
	AnalogInputModifier = ComputeAnalogInputModifier();
	CalcVelocity(Dt, GroundFriction, false, GetMaxBrakingDeceleration());
	UpdateDefaultAvoidance();
	return FVector(Velocity.X, Velocity.Y, 0.) * Dt;
}

void UBattleTrialMovement::ApplyStep(const FVector& Delta, float Dt)
{
	if (!Delta.IsNearlyZero())
	{
		const FVector Start = UpdatedComponent->GetComponentLocation();
		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit()) SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		Velocity = (UpdatedComponent->GetComponentLocation() - Start) / Dt;
		PhysicsRotation(Dt);
	}
	// PhysicsRotation needs the pending path direction when navigation uses direct velocity.
	LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
	bHasRequestedVelocity = false;
	UpdateComponentVelocity();
}

void UBattleMassTrial::Start(UWorld& World, const TArray<TObjectPtr<ABattleUnit>>& Units)
{
	Manager = World.GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager().AsShared();
	Query = MakeUnique<FMassEntityQuery>(Manager);
	Query->AddRequirement<FBattleTrialMotion>(EMassFragmentAccess::ReadWrite);
	const auto Archetype = Manager->CreateArchetype({ FBattleTrialMotion::StaticStruct() });
	for (ABattleUnit* Unit : Units)
	{
		auto Handle = Manager->CreateEntity(Archetype);
		Manager->GetFragmentDataChecked<FBattleTrialMotion>(Handle).Movement = CastChecked<UBattleTrialMovement>(Unit->GetCharacterMovement());
		Entities.Add(Handle);
	}
}

void UBattleMassTrial::Update(float Dt)
{
	if (Dt <= SMALL_NUMBER) return;
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleMassMove);
	auto Context = Manager->CreateExecutionContext(Dt);
	int32 Active = 0;
	Query->ForEachEntityChunk(Context, [Dt, &Active](FMassExecutionContext& Chunk)
	{
		auto Rows = Chunk.GetMutableFragmentView<FBattleTrialMotion>();
		for (auto& Row : Rows)
		{
			auto* Move = Row.Movement.Get();
			if (!Move) continue;
			auto* Unit = CastChecked<ABattleUnit>(Move->GetOwner());
			const bool bManage = Unit->IsAlive() && !Unit->IsPlayerControlled() && Move->IsMovingOnGround();
			if (Row.bManaged != bManage)
			{
				Move->SetComponentTickEnabled(!bManage);
				Row.bManaged = bManage;
			}
			if (bManage) { Row.Delta = Move->PlanStep(Dt); ++Active; }
		}
		for (auto& Row : Rows)
			if (Row.bManaged && Row.Movement.IsValid()) Row.Movement->ApplyStep(Row.Delta, Dt);
	});
	ActivePeak = FMath::Max(ActivePeak, Active);
}

void UBattleMassTrial::Stop()
{
	for (auto Handle : Entities) Manager->DestroyEntity(Handle);
	Entities.Reset();
	Query.Reset();
	Manager.Reset();
}
