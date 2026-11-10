#include "BattleGameMode.h"
#include "FutureBattle.h"
#include "BattleUnit.h"
#include "BattlePerception.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

namespace
{
bool SameContact(const FBattleContact& A, const FBattleContact& B)
{
	return A.TargetID == B.TargetID && A.LastKnownPosition == B.LastKnownPosition &&
		A.LastSeenTime == B.LastSeenTime && A.Confidence == B.Confidence && A.bVisible == B.bVisible &&
		A.SourceUnitID == B.SourceUnitID && A.bShared == B.bShared;
}
bool SamePerception(UBattlePerception* A, UBattlePerception* B, double Now)
{
	TMap<int32, FBattleObserver> OA, OB; TArray<FBattleReport> RA, RB;
	A->Capture(OA, RA, Now); B->Capture(OB, RB, Now);
	if (OA.Num() != OB.Num() || RA.Num() != RB.Num()) return false;
	for (const auto& Pair : OA)
	{
		const auto* Other = OB.Find(Pair.Key); const auto& This = Pair.Value;
		if (!Other || This.ProfileRevision != Other->ProfileRevision || This.Contacts.Num() != Other->Contacts.Num() || This.DetectionStarted.Num() != Other->DetectionStarted.Num()) return false;
		for (const auto& Contact : This.Contacts)
		{
			const auto* Found = Other->Contacts.Find(Contact.Key);
			if (!Found || !SameContact(Contact.Value, *Found)) return false;
		}
		for (const auto& Started : This.DetectionStarted)
		{
			const auto* Found = Other->DetectionStarted.Find(Started.Key);
			if (!Found || *Found != Started.Value) return false;
		}
	}
	for (int32 I = 0; I < RA.Num(); ++I)
		if (RA[I].ReceiverID != RB[I].ReceiverID || RA[I].SourceRevision != RB[I].SourceRevision || RA[I].ReceiverRevision != RB[I].ReceiverRevision ||
			RA[I].DeliveryTime != RB[I].DeliveryTime || !SameContact(RA[I].Contact, RB[I].Contact)) return false;
	return true;
}
}

void ABattleGameMode::RunPerceptionOptimizationSmokeTest()
{
	auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	TArray<TObjectPtr<ABattleUnit>> Fixture = {Units[5], Units[6], Units[15], Units[16]};
	TArray<FTransform> Transforms; TArray<FBattlePerceptionProfile> Profiles;
	for (ABattleUnit* Unit : Fixture) { Transforms.Add(Unit->GetActorTransform()); Profiles.Add(Unit->GetState()->Perception); }
	auto* Reference = NewObject<UBattlePerception>(GetWorld());
	auto* Grid = NewObject<UBattlePerception>(GetWorld()); Grid->bUseSpatialGrid = true;
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		Reference->Reset(); Grid->Reset();
		AStaticMeshActor* Cover = nullptr;
		if (Scenario == 2)
		{
			Cover = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-10800, -10800, 150), FRotator::ZeroRotator);
			Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Cover->SetActorScale3D(FVector(1, 10, 4)); Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		}
		bool bEqual = true;
		for (int32 Step = 0; Step < 160; ++Step)
		{
			if (Step == 0 || Step == 40 || Step == 80 || Step == 120)
				for (int32 I = 0; I < Fixture.Num(); ++I)
				{
					FBattlePerceptionProfile P;
					P.SightRange = Step == 40 ? 0.f : Step == 80 ? 20000.f : 2400.f;
					P.FieldOfView = I == 1 ? 360.f : 120.f; P.DetectionSeconds = .3f;
					P.ShareRange = 1600.f; P.ShareDelay = .4f; P.MemorySeconds = 2.f;
					Store->SetPerceptionProfile(Fixture[I]->GetUnitID(), P);
				}
			const float Spread = Scenario == 0 ? 1800.f : 200.f;
			for (int32 I = 0; I < Fixture.Num(); ++I)
			{
				// Cross negative cell edges, exact range edges and both view directions.
				const float X = -12000.f + (I >= 2 ? 2399.f + (Step % 5) : 0.f) + (Step % 20) * 61.f;
				const float Y = -10800.f + (I % 2) * Spread;
				Fixture[I]->SetActorLocation(FVector(X, Y, 95 + (Step % 3)), false, nullptr, ETeleportType::TeleportPhysics);
				Fixture[I]->SetActorRotation(FRotator(0, I >= 2 ? 180.f : Step % 30 < 20 ? 0.f : 180.f, 0));
			}
			if (Cover && Step == 60) Cover->SetActorLocation(FVector(-10800, -8000, 150));
			TArray<TObjectPtr<ABattleUnit>> Active = Fixture;
			if (Step >= 100 && Step < 110) Active.RemoveAt(3); // removal/reappearance
			const double Now = 1000. + Step * .1;
			Reference->Update(Active, Now); Grid->Update(Active, Now);
			bEqual &= SamePerception(Reference, Grid, Now);
		}
		CheckSmoke(bEqual, Scenario == 0 ? TEXT("Grid equals exhaustive sensing across open-space range/cell boundaries, profiles, removal and sharing") :
			Scenario == 1 ? TEXT("Grid equals exhaustive sensing in dense positions with rotating fields of view") :
			TEXT("Grid equals exhaustive sensing with moving physical cover, memory and delayed reports"));
		if (Cover) Cover->Destroy();
	}
	// Frame-by-frame scheduling tests at multiple start phases, using real traces.
	for (int32 DistanceCase = 0; DistanceCase < 2; ++DistanceCase)
	{
		bool bTiming = true, bLost = true, bSharing = true;
		double LatestFirstShared = 0.;
		for (int32 Trial = 0; Trial < 32; ++Trial)
		{
			auto* Even = NewObject<UBattlePerception>(GetWorld()); Even->bUseSpatialGrid = true;
			auto* Far = NewObject<UBattlePerception>(GetWorld()); Far->bUseSpatialGrid = true; Far->bSlowUnknownFar = true;
			FBattlePerceptionProfile P; P.SightRange = 2400.f; P.FieldOfView = 120.f; P.DetectionSeconds = .6f;
			P.MemorySeconds = 2.f; P.ShareRange = 1600.f; P.ShareDelay = .8f;
			Store->SetPerceptionProfile(Fixture[0]->GetUnitID(), P);
			P.SightRange = 0.f;
			Store->SetPerceptionProfile(Fixture[1]->GetUnitID(), P);
			Store->SetPerceptionProfile(Fixture[2]->GetUnitID(), P);
			Fixture[0]->SetActorLocation(FVector(-12000, -10800, 95), false, nullptr, ETeleportType::TeleportPhysics);
			Fixture[1]->SetActorLocation(FVector(-12000, -11200, 95), false, nullptr, ETeleportType::TeleportPhysics);
			Fixture[2]->SetActorLocation(FVector(-12000 + (DistanceCase ? 1800 : 800), -10800, 95), false, nullptr, ETeleportType::TeleportPhysics);
			Fixture[0]->SetActorRotation(FRotator::ZeroRotator);
			TArray<TObjectPtr<ABattleUnit>> Active = {Fixture[0], Fixture[1], Fixture[2]};
			const double Start = 2000. + Trial * .007;
			double FirstEven = -1., FirstFar = -1.;
			bool bReceivedEven = false, bReceivedFar = false;
			for (int32 Step = 0; Step <= 200; ++Step)
			{
				const double Now = Start + Step * .01;
				if (Step == 120) Fixture[0]->SetActorRotation(FRotator(0, 180, 0));
				Even->Update(Active, Now, -1, 0); Far->Update(Active, Now, -1, 0);
				const auto* CE = Even->FindContact(Fixture[0]->GetUnitID(), Fixture[2]->GetUnitID());
				const auto* CF = Far->FindContact(Fixture[0]->GetUnitID(), Fixture[2]->GetUnitID());
				if (CE && CE->bVisible && FirstEven < 0.) FirstEven = Now;
				if (CF && CF->bVisible && FirstFar < 0.) FirstFar = Now;
				if (Step >= 142) bLost &= CE && CF && !CE->bVisible && !CF->bVisible;
				for (auto* S : {Even, Far}) if (const auto* Shared = S->FindContact(Fixture[1]->GetUnitID(), Fixture[2]->GetUnitID()))
					{
						bSharing &= Shared->bShared && !Shared->bVisible && Now - Shared->LastSeenTime + 1.e-6 >= .8;
						if ((S == Even && !bReceivedEven) || (S == Far && !bReceivedFar)) LatestFirstShared = FMath::Max(LatestFirstShared, Now - Start);
						if (S == Even) bReceivedEven = true; else bReceivedFar = true;
					}
			}
			bSharing &= bReceivedEven && bReceivedFar;
			bTiming &= FirstEven >= Start + .6 && FirstEven <= Start + .81 && FirstFar >= FirstEven - 1.e-6 && FirstFar <= FirstEven + .21;
			if (!DistanceCase) bTiming &= FMath::IsNearlyEqual(FirstEven, FirstFar, 1.e-6);
		}
		CheckSmoke(bTiming, DistanceCase ? TEXT("Far diagnostic adds at most one 0.2s visit to stationary far acquisition across 32 phases") :
			TEXT("Even schedule preserves acquisition delay and Far diagnostic preserves near acquisition across 32 phases"));
		UE_LOG(LogFutureBattle, Display, TEXT("BATTLE_SENSE_TIMING distance_case=%d latest_first_report=%.3f lost_ok=%d sharing_ok=%d"), DistanceCase, LatestFirstShared, bLost, bSharing);
		CheckSmoke(bLost && bSharing, DistanceCase ? TEXT("Far diagnostic still clears lost known targets within one visit and respects report delay") :
			TEXT("Even schedule clears lost near targets within one visit and never delivers shared reports early"));
	}
	for (int32 I = 0; I < Fixture.Num(); ++I)
	{
		Fixture[I]->SetActorTransform(Transforms[I], false, nullptr, ETeleportType::TeleportPhysics);
		Store->SetPerceptionProfile(Fixture[I]->GetUnitID(), Profiles[I]);
	}
}
