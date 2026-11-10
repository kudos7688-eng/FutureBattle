// Test-only full-state fingerprints. Never enabled during performance runs.
#include "BattlePhase3.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleGameMode.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Misc/SecureHash.h"

void UBattlePhase3::RecordContextReplay()
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 const double Now=GetWorld()->GetTimeSeconds();
 TArray<uint8> StateBytes,SenseBytes;
 FMemoryWriter StateWriter(StateBytes),SenseWriter(SenseBytes);
 FObjectAndNameAsStringProxyArchive StateAr(StateWriter,false),SenseAr(SenseWriter,false);
 TArray<int32> IDs; Store->GetUnits().GetKeys(IDs); IDs.Sort();
 double HP=0; int32 Ammo=0,Alive=0;
 for(int32 ID:IDs)
 {
  FBattleUnitState Unit=*Store->Find(ID);
  FBattleUnitState::StaticStruct()->SerializeItem(StateAr,&Unit,nullptr);
  FVector P=Runtime->Position(ID),F=Runtime->Forward(ID),V=Velocity(ID);
  StateAr << P << F << V;
  HP+=Unit.HP; Ammo+=Unit.Ammo; Alive+=Unit.IsAlive();
 }
 Store->GetSquads().GetKeys(IDs); IDs.Sort();
 for(int32 ID:IDs)
 {
  FBattleSquadState Squad=*Store->FindSquad(ID);
  FBattleSquadState::StaticStruct()->SerializeItem(StateAr,&Squad,nullptr);
 }
 Runtime->Plans.GetKeys(IDs); IDs.Sort();
 for(int32 ID:IDs)
 {
  auto Plan=Runtime->Plans.FindChecked(ID);
  StateAr << ID << Plan.OrderRevision << Plan.TargetID << Plan.NextThink << Plan.NextPath << Plan.bReplan << Plan.Path << Plan.Center;
 }
 StateAr << Paths << Waypoints << Desired << Solved;
 StateAr << Shots << Deaths << Decisions << PathRequests << Mode->bBattleEnded;
 Sense->Observers.GetKeys(IDs); IDs.Sort();
 for(int32 ID:IDs)
 {
  FBattleObserver Observer=Sense->Observers.FindChecked(ID);
  SenseAr << ID; FBattleObserver::StaticStruct()->SerializeItem(SenseAr,&Observer,nullptr);
 }
 for(auto Report:Sense->PendingReports) FBattleReport::StaticStruct()->SerializeItem(SenseAr,&Report,nullptr);
 SenseAr << Sense->PendingCounts << Sense->NextScanTimes << Sense->LastScanTimes;
 auto Digest=[](const TArray<uint8>& Bytes) { uint8 Hash[20]; FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash); return BytesToHex(Hash,20); };
 ContextReplayRows.Add(FString::Printf(TEXT("%d,%.9f,%s,%s,%lld,%lld,%.9f,%d,%d"),ContextReplayRows.Num()-1,Now-Started,*Digest(StateBytes),*Digest(SenseBytes),Shots,Deaths,HP,Ammo,Alive));
}
