#include "BattleGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "BattleSave.h"
#include "BattleMatch.h"
#include "BattleUnit.h"
#include "BattleController.h"
#include "BattlePerception.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include <limits>

void ABattleGameMode::RunSaveSmokeTest()
{
	auto* Match = GetWorld()->GetSubsystem<UBattleMatch>(); auto* Store = GetWorld()->GetSubsystem<UBattleState>();
	auto* PC = Cast<ABattleController>(GetWorld()->GetFirstPlayerController());
	Match->Phase = EBattleMatchPhase::Combat; Match->Elapsed = 12.f; Match->Credits[0] = 1234; Match->Credits[1] = 987; Match->Winner = INDEX_NONE;
	Match->Score[0] = 23; Match->Score[1] = 17; Match->DirectorClock = .2f; Match->SupplyClock = .4f;
	bBattleEnded = false; bCombatPaused = true; Result.Reset();
	ABattleUnit* A = Units[0]; const int32 ID = A->GetUnitID(); const double Now = GetWorld()->GetTimeSeconds();
	Store->SetOrder(ID, EBattleOrder::AttackMove, FVector(-1200, 0, 0)); Store->ApplyDamage(ID, 1, 17);
	Store->ConsumeShot(ID, Now, .1); Store->StartReload(ID, Now);
	PC->ControlUnit(A); PC->SetControlRotation(FRotator(-12, 70, 0));
	// Include a non-visible old contact and an in-flight report; neither may become omniscient on restore.
	auto* Perception = GetWorld()->GetSubsystem<UBattlePerception>(); Perception->Reset();
	int32 EnemyID = INDEX_NONE, FriendID = Units[1]->GetUnitID();
	for (ABattleUnit* Unit : Units) if (Unit->IsAlive() && Unit->GetState()->Team == 1) { EnemyID = Unit->GetUnitID(); break; }
	TMap<int32, FBattleObserver> Known; FBattleObserver Observer; Observer.ProfileRevision = A->GetState()->PerceptionRevision;
	FBattleContact Contact; Contact.TargetID = EnemyID; Contact.SourceUnitID = ID; Contact.LastKnownPosition = FVector(900, 1000, 95); Contact.LastSeenTime = -2.; Contact.Confidence = .6f;
	Observer.Contacts.Add(EnemyID, Contact); Known.Add(ID, Observer);
	FBattleReport Report; Report.ReceiverID = FriendID; Report.SourceRevision = A->GetState()->PerceptionRevision; Report.ReceiverRevision = Units[1]->GetState()->PerceptionRevision; Report.DeliveryTime = .5; Report.Contact = Contact;
	Perception->Restore(Known, { Report }, Now);
	auto* Save = Match->CaptureSnapshot(); FString Error;
	CheckSmoke(Save && Save->Validate(Error), *FString::Printf(TEXT("Live match snapshot validates before serialization (%s)"), *Error));
	if (!Save || !Save->Validate(Error)) { CompleteSmokeTest(); return; }
	const FGuid SavedID = Save->MatchID; const FBattleUnitState Before = *A->GetState();
	CheckSmoke(Save->ControlledID == ID && FMath::IsNearlyEqual(Save->Units[0].State.ReloadEndTime, 1.8, .01) && Save->Units[0].State.NextShotTime <= .11, TEXT("Snapshot stores FPS target and remaining cooldowns rather than absolute world time"));
	TArray<uint8> Bytes; const bool bSerialized = UGameplayStatics::SaveGameToMemory(Save, Bytes);
	auto* Decoded = bSerialized ? Cast<UBattleSave>(BattleSaveValidation::Decode(Bytes)) : nullptr;
	CheckSmoke(Decoded && Decoded->Validate(Error) && Decoded->Units.Num() == Units.Num() && Decoded->Reports.Num() == 1 && Decoded->AllyCredits == 1234, TEXT("Binary round trip preserves units economy and pending shared information"));
	if (!Decoded) { CompleteSmokeTest(); return; }
	int32 Rejected = 0;
	for (int32 Case = 0; Case < 8; ++Case)
	{
		auto* Bad = DuplicateObject<UBattleSave>(Decoded, GetTransientPackage());
		switch (Case) {
		case 0: Bad->Version = 999; break;
		case 1: Bad->Units[1].State.UnitID = Bad->Units[0].State.UnitID; break;
		case 2: Bad->Squads[0].Members.Add(999999); break;
		case 3: Bad->AllyCredits = -100; break;
		case 4: Bad->Units[0].State.HP = std::numeric_limits<float>::quiet_NaN(); break;
		case 5: Bad->Units[0].State.ReloadEndTime = -1; break;
		case 6: Bad->ControlledID = EnemyID; break;
		case 7: Bad->Objectives[0].Owner = 7; break;
		}
		if (!Match->RestoreSnapshot(Bad) && Units[0] == A && Match->MatchID == SavedID && Match->Credits[0] == 1234 && A->GetState()->HP == Before.HP && PC->GetPawn() == A) ++Rejected;
	}
	CheckSmoke(Rejected == 8, TEXT("Eight corrupt snapshots reject atomically without changing actors control resources or match ID"));
	CheckSmoke(Match->SaveMatch(TEXT("FutureBattleSmokeOnly")), TEXT("Dedicated smoke save slot writes successfully without touching the player quicksave"));
	Match->Credits[0] = 1; Match->Score[0] = 0; Store->ApplyDamage(ID, 1, 10); PC->EnterTactical();
	CheckSmoke(Match->LoadMatch(TEXT("FutureBattleSmokeOnly")), TEXT("Disk save reload restores a validated match"));
	CheckSmoke(Units[4]->GetState()->Weapon == EBattleWeapon::SupportGun && Units[4]->Rifle->GetStaticMesh()->GetName() == TEXT("SM_Support"), TEXT("Disk load re-equips saved support weapon on replacement actor"));
	auto* Restored = Cast<ABattleUnit>(PC->GetPawn());
	CheckSmoke(Restored && Restored != A && Restored->GetUnitID() == ID && Restored->GetState()->HP == Before.HP && Restored->GetState()->Ammo == Before.Ammo && Restored->GetState()->Reserve == Before.Reserve && Restored->GetState()->Training == Before.Training,
		TEXT("New actors recover the same authoritative ID health ammunition and purchased capability"));
	CheckSmoke(Match->MatchID == SavedID && Match->Credits[0] == 1234 && Match->Credits[1] == 987 && Match->Score[0] == 23 && Match->Score[1] == 17 && Match->Elapsed == 12 && bCombatPaused,
		TEXT("Match identity economy score elapsed time and pause survive disk restoration"));
	CheckSmoke(Restored && Restored->GetState()->Order == EBattleOrder::AttackMove && Restored->GetState()->Destination == FVector(-1200, 0, 0) && PC->GetControlRotation().Equals(FRotator(-12, 70, 0), .1), TEXT("Persistent orders and FPS view survive while transient navigation is rebuilt"));
	const auto* RestoredContact = Perception->FindContact(ID, EnemyID);
	CheckSmoke(RestoredContact && !RestoredContact->bVisible && RestoredContact->LastKnownPosition == Contact.LastKnownPosition && FMath::IsNearlyEqual(GetWorld()->GetTimeSeconds() - RestoredContact->LastSeenTime, 2., .05), TEXT("Remembered enemy stays hidden at its saved known location with the same age"));
	int32 MaxID = 0; for (ABattleUnit* Unit : Units) MaxID = FMath::Max(MaxID, Unit->GetUnitID());
	PC->EnterTactical(); const int32 Count = Units.Num();
	CheckSmoke(Match->HireSquad(0, EBattleTraining::Recruit) && Units.Num() == Count + 5 && Units.Last()->GetUnitID() > MaxID, TEXT("Post-load recruitment allocates new IDs without colliding with restored members"));
	CheckSmoke(!Match->LoadMatch(TEXT("NotAnAllowedSlot")) && Match->MatchID == SavedID, TEXT("Unsupported save slot leaves active match intact"));
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, ID, Before]()
	{
		const auto* S = GetWorld()->GetSubsystem<UBattleState>()->Find(ID);
		CheckSmoke(S && S->ReloadEndTime == 0 && S->Ammo == 30 && S->Reserve == Before.Reserve - 1, TEXT("Restored reload finishes once after its remaining duration and conserves ammunition"));
		RunSupportSmokeTest();
	}), 2.f, false);
}
