// Defensive deployment, stage 1a (2026-11-25, user design; Docs/design-defense-deployment.md). Trial -BattleDefendSpread=1 (default 0):
// a squad sent to hold a sector (order Defend or Hold, sector mission, cover arrival) splits into fire teams instead of each soldier
// taking the nearest free cover slot. BuildMovementPlan (BattleStableMovement.cpp) then walks each member's own candidate order.
//  Teams: 2 (fewer than 6 living) or 3, members by their place across the threat direction (left to right, so nobody crosses);
//   the anchors stand on the line across the threat direction through the destination, 12 m apart.
//  Approach slots: cover slots 15-60 m ahead of the destination toward the threat and within 40 m to either side whose obstacle
//   is on our side of them (an enemy bounding there is hidden from straight ahead) - where a bounding enemy is likely to stop.
//  View: how many approach slots a candidate sees (a clear line from its eye to the slot), i.e. can fire into from the side.
//  Each member's order: distance to his team's anchor less 3 m for every approach slot in view (the first 40 candidates are rated).
//  No fallback where cover is scarce: weak ground stays weak (user, 11-25). Threat direction: the arrival facing of the plan
//  (-BattleArrivalThreat: the known enemy nearest to the sector), else the way in. Inputs: own positions, known enemies, map cover.
// Records: defend_spread_plans, defend_spread_teams_2 / _3, defend_spread_approach_slots, defend_spread_rated, defend_spread_view_sum.
#include "BattleLeaderRuntime.h"
#include "BattleCover.h"
#include "BattlePerception.h"
#include "BattleSectors.h"
#include "BattleState.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
 constexpr double TeamGap=1200., AheadMin=1500., AheadMax=6000., SideMax=4000., ViewWeight=300.;
 constexpr int32 ApproachMax=24, RateMax=40;
}

bool UBattleLeaderRuntime::DefendSpreadEnabled()
{
 static const bool b=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleDefendSpread="),V);return V!=0;}();
 return b;
}

bool UBattleLeaderRuntime::DefendSpreadFor(int32 Team)
{
 // -BattleDefendSpreadTeam=0/1 (one-sided trial, default -1 both): only that team's squads place fire teams.
 static const int32 T=[]{int32 V=-1;FParse::Value(FCommandLine::Get(),TEXT("BattleDefendSpreadTeam="),V);return V;}();
 return DefendSpreadEnabled() && (T<0 || T==Team);
}

void UBattleLeaderRuntime::ReleaseSpreadCover(const FBattleSquadState& S)
{
 // Codex 11-25: a soldier in a cover action is stepped by StepCoverAction before his move and could stay in his old cover while
 // the plan already faces the new threat. Stage 1b and the first placement after taking the sector release them.
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(int32 ID:S.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()&&U->CoverPhase&&ID!=INDEX_NONE)
 {CancelCoverAction(ID);SightExperiment.Cover(TEXT("defend_spread_cover_released"));}
}

void UBattleLeaderRuntime::PlanDefendSpread(const FBattleSquadState& S,const FVector& Forward,const FVector& Right,const TArray<FVector>& Candidates,TMap<int32,TArray<FVector>>& Order)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();
 TArray<int32> Living;for(int32 ID:S.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())Living.Add(ID);
 if(Living.Num()<2||Candidates.Num()<2||Forward.IsNearlyZero())return;
 const FVector Dest=S.Destination;
 Living.Sort([&](int32 A,int32 B){const double LA=FVector::DotProduct(Position(A)-Dest,Right),LB=FVector::DotProduct(Position(B)-Dest,Right);return LA!=LB?LA<LB:A<B;});
 const int32 Teams=Living.Num()>=6?3:2;
 TArray<FVector> Anchors;
 for(int32 T=0;T<Teams;++T)Anchors.Add(Dest+Right*((T-(Teams-1)*.5)*TeamGap));
 // Approach slots, nearest to the destination first.
 const FCollisionObjectQueryParams Static(ECC_WorldStatic);
 TArray<FVector> Approach;
 {
  TArray<TPair<double,FVector>> Found;
  const float Reach=float(FMath::Sqrt(FMath::Square((AheadMax-AheadMin)*.5)+FMath::Square(SideMax)))+100.f; // the rectangle's half diagonal
  for(int32 Slot:Cover->NearbySlots(Dest+Forward*((AheadMin+AheadMax)*.5),Reach,256))
  {
   const FVector A=Cover->GetSlots()[Slot];const FVector D=A-Dest;
   const double Ahead=FVector::DotProduct(D,Forward),Side=FMath::Abs(FVector::DotProduct(D,Right));
   if(Ahead<AheadMin||Ahead>AheadMax||Side>SideMax)continue;
   if(!GetWorld()->LineTraceTestByObjectType(A+FVector(0,0,100),A-Forward*250+FVector(0,0,100),Static))continue; // no obstacle on our side
   Found.Add(TPair<double,FVector>(FVector::DistSquared2D(A,Dest),A));
  }
  Found.Sort([](const TPair<double,FVector>& X,const TPair<double,FVector>& Y){return X.Key!=Y.Key?X.Key<Y.Key:X.Value.X!=Y.Value.X?X.Value.X<Y.Value.X:X.Value.Y<Y.Value.Y;});
  for(int32 I=0;I<Found.Num()&&I<ApproachMax;++I)Approach.Add(Found[I].Value);
 }
 // How many approach slots each rated candidate sees.
 TArray<int32> View;View.Init(0,Candidates.Num());int32 ViewSum=0;
 for(int32 C=0;C<Candidates.Num()&&C<RateMax;++C)
  for(const FVector& A:Approach)
   if(!GetWorld()->LineTraceTestByObjectType(Candidates[C]+FVector(0,0,120),A+FVector(0,0,90),Static)){++View[C];++ViewSum;}
 // Each member's candidate order for his team.
 for(int32 I=0;I<Living.Num();++I)
 {
  const int32 Team=FMath::Min(Teams-1,I*Teams/Living.Num());const FVector Anchor=Anchors[Team];
  TArray<int32> Index;for(int32 C=0;C<Candidates.Num();++C)Index.Add(C);
  Index.Sort([&](int32 A,int32 B){const double SA=FVector::Dist2D(Candidates[A],Anchor)-ViewWeight*View[A],SB=FVector::Dist2D(Candidates[B],Anchor)-ViewWeight*View[B];return SA!=SB?SA<SB:A<B;});
  auto& Out=Order.Add(Living[I]);for(int32 C:Index)Out.Add(Candidates[C]);
 }
 SightExperiment.Cover(TEXT("defend_spread_plans"));SightExperiment.Cover(Teams==3?TEXT("defend_spread_teams_3"):TEXT("defend_spread_teams_2"));
 if(SightExperiment.Enabled)
 {
  SightExperiment.CoverDiag.FindOrAdd(FName(TEXT("defend_spread_approach_slots")))+=Approach.Num();
  SightExperiment.CoverDiag.FindOrAdd(FName(TEXT("defend_spread_rated")))+=FMath::Min(RateMax,Candidates.Num());
  SightExperiment.CoverDiag.FindOrAdd(FName(TEXT("defend_spread_view_sum")))+=ViewSum;
 }
}

// Evaluation only (2026-11-26, why the side using -BattleDefendSpread loses in the one-sided trial): hits and deaths of soldiers
// holding a sector (squad on a sector mission with order Defend or Hold, the soldier inside its sector), per team, with what was
// around them at death; and the hits and kills they scored. Recorded with the switch on or off (compare against switch off).
// Keys defend_<name>_t<team>. Never AI input.
bool UBattleLeaderRuntime::IsSectorDefender(int32 ID) const
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);if(!U)return false;
 const auto* S=Store->FindSquad(U->SquadID);
 return S && S->bSectorMission && (S->Order==EBattleOrder::Defend||S->Order==EBattleOrder::Hold) && !BattleSquadType::IsVehicle(S->SquadType)
  && BattleSectors::Cell(Position(ID))==BattleSectors::Cell(S->Destination);
}

void UBattleLeaderRuntime::NoteDefendHit(int32 Victim,int32 Shooter,const FVector& From,bool bKilled,int32 CoverState,bool bWalking)
{
 if(!SightExperiment.Started)return;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Perc=GetWorld()->GetSubsystem<UBattlePerception>();
 auto Note=[&](const TCHAR* Name,int32 Team){SightExperiment.Cover(*FString::Printf(TEXT("defend_%s_t%d"),Name,Team));};
 if(const auto* SU=Store->Find(Shooter);SU&&SU->Team>=0&&SU->Team<2&&IsSectorDefender(Shooter)){Note(TEXT("shooter_hits"),SU->Team);if(bKilled)Note(TEXT("shooter_kills"),SU->Team);}
 const auto* V=Store->Find(Victim);if(!V||V->Team<0||V->Team>1||!IsSectorDefender(Victim))return;
 const int32 T=V->Team;const FVector At=Position(Victim);
 Note(TEXT("victim_hits"),T);if(bWalking)Note(TEXT("victim_hit_walking"),T);
 if(!bKilled)return;
 Note(TEXT("victim_deaths"),T);
 Note(CoverState==0?TEXT("victim_death_no_cover"):TEXT("victim_death_cover"),T);
 if(bWalking)Note(TEXT("victim_death_walking"),T);
 // The nearest living squad mate, and the squad mates within 20 m that see the shooter now (could answer him).
 double Near=DBL_MAX;int32 Seeing=0;
 if(const auto* S=Store->FindSquad(V->SquadID))for(int32 M:S->Members)if(M!=Victim)if(const auto* MU=Store->Find(M);MU&&MU->IsAlive())
 {
  const double D=FVector::Dist2D(Position(M),At);Near=FMath::Min(Near,D);
  if(D<2000.)if(const auto* C=Perc->FindContact(M,Shooter);C&&C->bVisible)++Seeing;
 }
 Note(Near<500.?TEXT("victim_death_mate_lt5m"):Near<1500.?TEXT("victim_death_mate_lt15m"):Near<DBL_MAX?TEXT("victim_death_mate_ge15m"):TEXT("victim_death_mate_none"),T);
 Note(Seeing==0?TEXT("victim_death_support_0"):Seeing==1?TEXT("victim_death_support_1"):TEXT("victim_death_support_2plus"),T);
 const double R=FVector::Dist2D(From,At);
 Note(R<1500.?TEXT("victim_death_range_lt15m"):R<4000.?TEXT("victim_death_range_lt40m"):TEXT("victim_death_range_ge40m"),T);
 const auto* VC=Perc->FindContact(Victim,Shooter);Note(VC&&VC->bVisible?TEXT("victim_death_shooter_seen"):TEXT("victim_death_shooter_unseen"),T);
}

void UBattleLeaderRuntime::NoteDefendSeconds()
{
 // 1 Hz (StepHotSectors): soldier-seconds holding a sector per team, and those in a cover action.
 if(!SightExperiment.Started)return;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(const auto& P:Store->GetUnits())if(P.Value.IsAlive()&&P.Value.Team>=0&&P.Value.Team<2&&Bodies.Contains(P.Key)&&IsSectorDefender(P.Key))
 {
  SightExperiment.Cover(*FString::Printf(TEXT("defend_member_seconds_t%d"),P.Value.Team));
  if(P.Value.CoverPhase!=0)SightExperiment.Cover(*FString::Printf(TEXT("defend_member_cover_seconds_t%d"),P.Value.Team));
 }
}
