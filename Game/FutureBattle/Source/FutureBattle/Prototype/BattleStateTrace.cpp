// Stage 2 diagnosis (ROADMAP 2026-11-20 stage 2, Docs/design-divergence-trace.md): find the first frame where two matches differ.
// -BattleStateTrace=<N> (default 0 = off) writes, every N frames of combat, one line
//   BATTLE_STATE_TRACE f=<frame> t=<match s> pos=<crc> hp=<crc> cover=<crc> sense=<crc> plan=<crc> match=<crc> shot=<crc>
// with one hash per block of the match state, each built in unit/squad ID order from rounded values. Setup/Find-Divergence.py
// compares two run.logs and prints the first differing frame and block. -BattleStateTraceDetail=<from>-<to> (match seconds) adds
// BATTLE_STATE_UNIT lines with the raw values of every living soldier inside that window. Reads only: no cache is refreshed and no
// state is written, so a match with the trace equals one without.
#include "BattleLeaderRuntime.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "Misc/Crc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
 struct FStateTraceSettings { int32 Every=0; float DetailFrom=-1.f,DetailTo=-1.f; };
 const FStateTraceSettings& StateTraceSettings()
 {
  static const FStateTraceSettings S=[]
  {
   FStateTraceSettings V;FParse::Value(FCommandLine::Get(),TEXT("BattleStateTrace="),V.Every);V.Every=FMath::Max(0,V.Every);
   FString Window;
   if(FParse::Value(FCommandLine::Get(),TEXT("BattleStateTraceDetail="),Window))
   {
    FString From,To;
    if(Window.Split(TEXT("-"),&From,&To)){V.DetailFrom=FCString::Atof(*From);V.DetailTo=FCString::Atof(*To);}
   }
   return V;
  }();
  return S;
 }
 struct FHash
 {
  uint32 Value=0;
  void Int(int64 V){Value=FCrc::MemCrc32(&V,sizeof(V),Value);}
  void Round(double V,double Unit){Int(FMath::IsFinite(V)?int64(FMath::RoundToDouble(V/Unit)):int64(MIN_int64));}
  void Vec(const FVector& V,double Unit){Round(V.X,Unit);Round(V.Y,Unit);Round(V.Z,Unit);}
 };
}

void UBattleLeaderRuntime::StepStateTrace()
{
 const auto& S=StateTraceSettings();
 if(S.Every<=0||!bWideField)return;
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 if(!Match||Match->Phase!=EBattleMatchPhase::Combat)return;
 const int32 Frame=StateTraceFrame++;
 if(Frame%S.Every)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();
 if(!Store)return;
 const float T=Match->Elapsed;
 const bool bDetail=S.DetailFrom>=0&&T>=S.DetailFrom&&T<=S.DetailTo;
 FHash Pos,HP,Cover,Sense,Plan,MatchHash,Shot;
 TArray<int32> IDs;Store->GetUnits().GetKeys(IDs);IDs.Sort();
 for(int32 ID:IDs)
 {
  const FBattleUnitState* U=Store->Find(ID);if(!U)continue;
  HP.Int(ID);HP.Int(U->IsAlive());HP.Round(U->HP,.01);HP.Int(U->Ammo);HP.Int(U->Reserve);HP.Round(U->Shield,.01);HP.Int(U->bRetired);
  Shot.Int(ID);Shot.Int(U->ShotSequence);
  if(!U->IsAlive())continue;
  const FVector P=Position(ID),F=Forward(ID);
  Pos.Int(ID);Pos.Vec(P,1.);Pos.Round(FMath::RadiansToDegrees(FMath::Atan2(F.Y,F.X)),1.);
  Cover.Int(ID);Cover.Int(U->CoverPhase);Cover.Vec(U->CoverAnchor,1.);Cover.Int(U->CoverSlot);Cover.Int(U->CoverTargetID);Cover.Int(StanceOf(ID));
  if(const auto* Contacts=Perception?Perception->GetContacts(ID):nullptr)
  {
   TArray<int32> Keys;Contacts->GetKeys(Keys);Keys.Sort();
   Sense.Int(ID);Sense.Int(Keys.Num());
   for(int32 K:Keys){const FBattleContact& C=(*Contacts)[K];Sense.Int(K);Sense.Int(C.bVisible);Sense.Vec(C.LastKnownPosition,10.);}
  }
  if(bDetail)
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STATE_UNIT f=%d t=%.3f id=%d x=%.1f y=%.1f z=%.1f yaw=%.1f hp=%.2f ammo=%d reserve=%d cover=%d anchor=%s slot=%d target=%d stance=%d shots=%d contacts=%d"),
    Frame,T,ID,P.X,P.Y,P.Z,FMath::RadiansToDegrees(FMath::Atan2(F.Y,F.X)),U->HP,U->Ammo,U->Reserve,int32(U->CoverPhase),*U->CoverAnchor.ToCompactString(),U->CoverSlot,U->CoverTargetID,int32(StanceOf(ID)),U->ShotSequence,
    Perception&&Perception->GetContacts(ID)?Perception->GetContacts(ID)->Num():0);
 }
 TArray<int32> SquadIDs;Store->GetSquads().GetKeys(SquadIDs);SquadIDs.Sort();
 for(int32 SID:SquadIDs)
 {
  const FBattleSquadState* Q=Store->FindSquad(SID);if(!Q)continue;
  Plan.Int(SID);Plan.Int(int32(Q->CommandStatus));Plan.Int(int32(Q->Order));Plan.Vec(Q->Destination,10.);Plan.Int(Q->bHasMission);Plan.Int(int32(Q->MissionOrder));
  Plan.Vec(Q->MissionDestination,10.);Plan.Int(int32(Q->TacticalPlan.Action));Plan.Vec(Q->TacticalPlan.Destination,10.);
  if(const FBattleLeaderPlan* LP=Plans.Find(SID)){Plan.Int(LP->TargetID);Plan.Int(LP->ArmourTargetID);}
 }
 MatchHash.Int(Match->Credits[0]);MatchHash.Int(Match->Credits[1]);
 for(const auto& O:Match->Objectives){MatchHash.Int(O.Owner);MatchHash.Int(O.CapturingTeam);MatchHash.Round(O.CaptureProgress,.001);}
 TArray<int32> GroupIDs;Match->Groups.GetKeys(GroupIDs);GroupIDs.Sort();
 for(int32 GID:GroupIDs){const auto& G=Match->Groups[GID];MatchHash.Int(GID);MatchHash.Int(G.Squads.Num());MatchHash.Int(G.Objective);MatchHash.Int(G.Phase);}
 for(int32 Team=0;Team<2;++Team)
  if(Match->Area.StrategyFocus.IsValidIndex(Team)){MatchHash.Int(Match->Area.StrategyFocus[Team]);MatchHash.Int(Match->Area.StrategyPhase.IsValidIndex(Team)?Match->Area.StrategyPhase[Team]:0);}
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STATE_TRACE f=%d t=%.3f pos=%08x hp=%08x cover=%08x sense=%08x plan=%08x match=%08x shot=%08x"),
  Frame,T,Pos.Value,HP.Value,Cover.Value,Sense.Value,Plan.Value,MatchHash.Value,Shot.Value);
}
