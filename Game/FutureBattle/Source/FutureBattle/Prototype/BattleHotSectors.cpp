// Hot sectors (2026-11-25, user design: simplify the front as a whole and spend computation where bounds and close fights happen;
// Docs/design-defense-deployment.md). Trial -BattleHotSectors=1 (default 0), maps with sectors only. Once a second it picks the sectors
// whose squads get the detailed defence computation (stage 1 of the defensive deployment, not built yet). So far it only records:
// nothing reads the choice, so a match plays the same with the switch on.
//  1 Player: every sector a 40 m circle around the soldier the player controls touches (also across a sector border), outside the
//    limit, kept 10 s after he leaves. The player's team is detailed there; the other team once it observed (below).
//  2 Attack orders: for every attack or surprise group (FBattleGroup Kind 0 / 2) advancing (Phase 1), the sector of its objective
//    and the last sector short of it on the path of the group's first squad (none without a path); at most AttackMax sectors for both teams, the teams taking turns. The attacking team is detailed at once; the defending team only once it observed there (user decision 11-25: an
//    attack order must not make the defence of exactly that sector smarter before it has seen anything).
//  3 Contact: the other sectors by heat over the last 10 s (a rush seen 3, a hit 1, a shot over at most 50 m 0.2, a soldier
//    killed 2), at most HeatMax: in at 5 points, out after 10 s below 2, a full list takes a sector with 2 times the lowest
//    member's heat when that member has been in for 20 s. Each team is detailed once it observed there.
//  Observed: a soldier of the team in that sector sees an enemy himself (a visible contact, not a shared report) or was hit there,
//  within the last 10 s. The heat only chooses where to compute; it never goes into an AI decision.
// Records (metrics cover_diagnostics): hot_enter_* / hot_exit_* / hot_replace, hot_seconds_* (sector-seconds by source),
// hot_team_seconds (team-sector seconds detailed), hot_defender_observed and hot_defender_delay_ms (from an attack order on a
// sector to the defender's first observation there, once per order; hot_defender_unobserved: orders that ended first), hot_select_us (time spent here). BATTLE_HOT_SECTORS every 10 s.
#include "BattleLeaderRuntime.h"
#include "BattleController.h"
#include "BattleCover.h"
#include "BattleGameMode.h"
#include "BattleFieldLayout.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleSectors.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
 constexpr double PlayerRadius=4000., Linger=10., Window=10., HeatIn=5., HeatOut=2., Replace=2., MinStay=20.; // 11-25: 1.5 / 10 s replaced a heat slot every ~7 s (121-131 per match), 2 / 20 s
 constexpr int32 AttackMax=6, HeatMax=2;
 int32 Key(FIntPoint C){return C.X*100+C.Y;}
 FIntPoint CellOf(int32 K){return FIntPoint(K/100,K%100);}
 void Add(FBattleSightExperiment& E,const TCHAR* K,int32 N){if(E.Enabled&&N)E.CoverDiag.FindOrAdd(FName(K))+=N;}
}

bool UBattleLeaderRuntime::HotSectorsEnabled()
{
 static const bool b=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleHotSectors="),V);return V!=0;}();
 return b;
}

void UBattleLeaderRuntime::NoteHotEvent(const FVector& At,float Points)
{
 if(!HotSectorsEnabled()||!BattleFieldLayout::Of(GetWorld()).bSectors)return;
 const FIntPoint C=BattleSectors::Cell(At);if(!BattleSectors::Valid(C))return;
 const int32 Sec=FMath::FloorToInt(GetWorld()->GetTimeSeconds());auto& H=HotHeat.FindOrAdd(Key(C));const int32 I=((Sec%10)+10)%10;
 if(H.Sec[I]!=Sec){H.Sec[I]=Sec;H.Points[I]=0.f;}
 H.Points[I]+=Points;
}

void UBattleLeaderRuntime::NoteHotObserved(int32 Team,const FVector& At,double Now)
{
 if(!HotSectorsEnabled()||Team<0||Team>1)return;
 const FIntPoint C=BattleSectors::Cell(At);if(BattleSectors::Valid(C))HotObserved[Team].Add(Key(C),Now);
}

void UBattleLeaderRuntime::HotPlayerCells(const FVector& At,TSet<int32>& Out)
{
 const FIntPoint C0=BattleSectors::Cell(At);
 for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
 {
  const FIntPoint C(C0.X+DX,C0.Y+DY);if(!BattleSectors::Valid(C))continue;
  const FVector Mid=BattleSectors::Center(C);const double Half=BattleSectors::Size*.5;
  const double Dx=FMath::Max(0.,FMath::Abs(At.X-Mid.X)-Half),Dy=FMath::Max(0.,FMath::Abs(At.Y-Mid.Y)-Half);
  if(Dx*Dx+Dy*Dy<=PlayerRadius*PlayerRadius)Out.Add(Key(C));
 }
}

TArray<int32> UBattleLeaderRuntime::HotAttackOrder(const TArray<int32>& Team0,const TArray<int32>& Team1)
{
 TArray<int32> Order;const TArray<int32>* By[2]={&Team0,&Team1};
 for(int32 I=0;I<FMath::Max(Team0.Num(),Team1.Num());++I)for(int32 T=0;T<2;++T)if(I<By[T]->Num())Order.Add((*By[T])[I]);
 return Order;
}

bool UBattleLeaderRuntime::HotTeamDetailed(const FHotSector& S,int32 Team,uint8 AttackBits,bool bObserved)
{
 // The player's team in his sectors; the attackers of an order on the sector; everyone else (a defender) only once observed.
 return (S.Source==1&&Team==S.PlayerTeam)||(AttackBits&(1<<Team))!=0||bObserved;
}

bool UBattleLeaderRuntime::HotAttackAdd(TMap<int32,uint8>& Attack,int32 K,int32 Team)
{
 // A new sector only while fewer than AttackMax are listed; a listed one takes another attacking team at any time.
 if(!Attack.Contains(K)&&Attack.Num()>=AttackMax)return false;
 Attack.FindOrAdd(K)|=uint8(1<<Team);return true;
}

bool UBattleLeaderRuntime::IsHotSector(int32 Team,const FVector& At) const
{
 if(Team<0||Team>1)return false;
 const FHotSector* S=HotSectors.Find(Key(BattleSectors::Cell(At)));return S&&S->bTeam[Team];
}

void UBattleLeaderRuntime::StepHotSectors(double Now)
{
 if(DefendSmokeEnabled()&&!bDefendSmokeDone&&Mode&&Now>=Mode->CombatStartsAt+3.){bDefendSmokeDone=true;RunDefendSmoke(Now);}
 if(!HotSectorsEnabled()||!BattleFieldLayout::Of(GetWorld()).bSectors||Now<HotNext)return;
 HotNext=Now+1.;
 NoteDefendSeconds(); // evaluation only (BattleDefendSpread.cpp)
 const double T0=FPlatformTime::Seconds();
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 // Observations and deaths.
 TSet<int32> Alive;
 for(const auto& P:Store->GetUnits())
 {
  if(!P.Value.IsAlive()){if(HotAlive.Contains(P.Key))NoteHotEvent(Position(P.Key),2.f);continue;}
  Alive.Add(P.Key);
  if(!Bodies.Contains(P.Key)||!Sense)continue;
  if(const auto* Contacts=Sense->GetContacts(P.Key))for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared){NoteHotObserved(P.Value.Team,Position(P.Key),Now);break;}
 }
 HotAlive=MoveTemp(Alive);
 auto Observed=[&](int32 Team,int32 K){const double* T=HotObserved[Team].Find(K);return T&&Now-*T<=Window;};
 auto Heat=[&](int32 K){const auto* H=HotHeat.Find(K);if(!H)return 0.;double Sum=0;const int32 Sec=FMath::FloorToInt(Now);for(int32 I=0;I<10;++I)if(Sec>=H->Sec[I]&&Sec-H->Sec[I]<10)Sum+=H->Points[I];return Sum;};
 // 1 Player.
 TSet<int32> Player;int32 PlayerTeam=INDEX_NONE;
 if(auto* PC=GetWorld()->GetFirstPlayerController())if(const auto* Pawn=Cast<ABattleUnit>(PC->GetPawn()))if(const auto* U=Store->Find(Pawn->GetUnitID());U&&U->IsAlive())
 {
  PlayerTeam=U->Team;HotPlayerCells(Pawn->GetActorLocation(),Player);
 }
 // 2 Attack orders (earliest group first).
 TMap<int32,uint8> Attack; // sector -> attacking teams (a bit per team)
 if(Match)
 {
  // The teams take turns (each team's groups in ID order), so the shared limit does not favour the team whose groups were made first.
  TArray<int32> ByTeam[2];
  {TArray<int32> GIDs;Match->Groups.GetKeys(GIDs);GIDs.Sort();
   for(int32 GID:GIDs){const auto& G=Match->Groups[GID];if((G.Kind==0||G.Kind==2)&&G.Phase==1&&Match->Objectives.IsValidIndex(G.Objective)&&G.Team>=0&&G.Team<=1)ByTeam[G.Team].Add(GID);}}
  const TArray<int32> Order=HotAttackOrder(ByTeam[0],ByTeam[1]);
  for(int32 GID:Order)
  {
   const auto& G=Match->Groups[GID];
   const FVector Obj=Match->Objectives[G.Objective].Position;const FIntPoint OC=BattleSectors::Cell(Obj);
   // The sector short of the objective on the way in (user decision 11-25, Codex proposal): walking the path of the group's first
   // squad that has one back from its end, the first sector that is not the objective's. Without such a path: the objective only.
   FIntPoint BC(-1,-1);
   {
    TArray<int32> SIDs=G.Squads;SIDs.Sort();
    for(int32 SID:SIDs)if(const auto* SP=Plans.Find(SID);SP&&SP->Path.Num()>1)
    {
     for(int32 I=SP->Path.Num()-1;I>0&&!BattleSectors::Valid(BC);--I)
     {
      const FVector A=SP->Path[I],B=SP->Path[I-1];const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/1000.));
      for(int32 K=1;K<=Steps;++K){const FIntPoint C=BattleSectors::Cell(FMath::Lerp(A,B,double(K)/Steps));if(C!=OC&&BattleSectors::Valid(C)){BC=C;break;}}
     }
     break;
    }
    if(!BattleSectors::Valid(BC))Add(SightExperiment,TEXT("hot_attack_no_path"),1);
   }
   const FIntPoint Cells[2]={OC,BC};
   for(const FIntPoint& C:Cells)
   {
    if(!BattleSectors::Valid(C))continue;const int32 K=Key(C);
    if(!HotAttackAdd(Attack,K,G.Team))continue;
    const int32 DefKey=K*2+(1-G.Team);if(!HotAttackSince.Contains(DefKey))HotAttackSince.Add(DefKey,Now);
   }
  }
 }
 // The defender's delay: from the attack order on a sector to the defending team's first observation there.
 for(auto It=HotAttackSince.CreateIterator();It;++It)
 {
  const int32 K=It.Key()/2,Def=It.Key()%2;
  // Measured once per order on the sector: the entry stays (time -1) until the attack on the sector ends.
  const uint8* A=Attack.Find(K);
  if(!A||!(*A&(1<<(1-Def)))){if(It.Value()>=0)Add(SightExperiment,TEXT("hot_defender_unobserved"),1);It.RemoveCurrent();continue;}
  if(It.Value()>=0&&Observed(Def,K)){Add(SightExperiment,TEXT("hot_defender_observed"),1);Add(SightExperiment,TEXT("hot_defender_delay_ms"),int32((Now-It.Value())*1000.));It.Value()=-1;}
 }
 // Sources 1 and 2 replace their old members.
 for(auto It=HotSectors.CreateIterator();It;++It)
 {
  auto& S=It.Value();const int32 K=It.Key();
  if(S.Source==1){if(Player.Contains(K)){S.Until=Now+Linger;S.PlayerTeam=PlayerTeam;}else if(Now>S.Until){Add(SightExperiment,TEXT("hot_exit_player"),1);It.RemoveCurrent();}continue;}
  if(S.Source==2&&!Attack.Contains(K)){Add(SightExperiment,TEXT("hot_exit_attack"),1);It.RemoveCurrent();}
 }
 for(int32 K:Player)
 {
  FHotSector* S=HotSectors.Find(K);
  if(S&&S->Source==1)continue;
  if(S)Add(SightExperiment,S->Source==2?TEXT("hot_exit_attack"):TEXT("hot_exit_heat"),1);
  FHotSector N;N.Source=1;N.Since=Now;N.Until=Now+Linger;N.PlayerTeam=PlayerTeam;HotSectors.Add(K,N);Add(SightExperiment,TEXT("hot_enter_player"),1);
 }
 for(const auto& A:Attack)
 {
  FHotSector* S=HotSectors.Find(A.Key);
  if(S&&S->Source<=2){S->Attackers=A.Value;continue;}
  if(S)Add(SightExperiment,TEXT("hot_exit_heat"),1);
  FHotSector N;N.Source=2;N.Since=Now;N.Attackers=A.Value;HotSectors.Add(A.Key,N);Add(SightExperiment,TEXT("hot_enter_attack"),1);
 }
 // 3 Contact by heat.
 TArray<int32> Members;
 for(auto It=HotSectors.CreateIterator();It;++It)if(It.Value().Source==3)
 {
  auto& S=It.Value();const double H=Heat(It.Key());
  if(H<HeatOut){if(S.LowSince<0)S.LowSince=Now;else if(Now-S.LowSince>=Linger){Add(SightExperiment,TEXT("hot_exit_heat"),1);It.RemoveCurrent();continue;}}
  else S.LowSince=-1;
  Members.Add(It.Key());
 }
 TArray<TPair<double,int32>> Cand;
 for(const auto& H:HotHeat)if(!HotSectors.Contains(H.Key)){const double V=Heat(H.Key);if(V>=HeatIn)Cand.Add(TPair<double,int32>(V,H.Key));}
 Cand.Sort([](const TPair<double,int32>& A,const TPair<double,int32>& B){return A.Key!=B.Key?A.Key>B.Key:A.Value<B.Value;});
 for(const auto& C:Cand)
 {
  if(Members.Num()<HeatMax){FHotSector N;N.Source=3;N.Since=Now;HotSectors.Add(C.Value,N);Members.Add(C.Value);Add(SightExperiment,TEXT("hot_enter_heat"),1);continue;}
  int32 Low=INDEX_NONE;double LowHeat=DBL_MAX;
  for(int32 M:Members){const double V=Heat(M);if(V<LowHeat||(V==LowHeat&&M>Low)){LowHeat=V;Low=M;}}
  if(Low==INDEX_NONE||C.Key<Replace*FMath::Max(LowHeat,1e-3)||Now-HotSectors[Low].Since<MinStay)continue;
  HotSectors.Remove(Low);Members.Remove(Low);
  FHotSector N;N.Source=3;N.Since=Now;HotSectors.Add(C.Value,N);Members.Add(C.Value);Add(SightExperiment,TEXT("hot_replace"),1);
 }
 // Which teams are detailed in each hot sector.
 int32 BySource[4]={0,0,0,0},TeamSectors=0;
 for(auto& P:HotSectors)
 {
  auto& S=P.Value;
  for(int32 Team=0;Team<2;++Team)
   S.bTeam[Team]=HotTeamDetailed(S,Team,Attack.FindRef(P.Key),Observed(Team,P.Key));
  ++BySource[S.Source];TeamSectors+=int32(S.bTeam[0])+int32(S.bTeam[1]);
 }
 Add(SightExperiment,TEXT("hot_seconds_player"),BySource[1]);Add(SightExperiment,TEXT("hot_seconds_attack"),BySource[2]);
 Add(SightExperiment,TEXT("hot_seconds_heat"),BySource[3]);Add(SightExperiment,TEXT("hot_team_seconds"),TeamSectors);
 Add(SightExperiment,TEXT("hot_select_us"),int32((FPlatformTime::Seconds()-T0)*1.e6));
 if(Now>=HotNextLog)
 {
  HotNextLog=Now+10.;FString List;
  TArray<int32> Keys;HotSectors.GetKeys(Keys);Keys.Sort();
  for(int32 K:Keys){const auto& S=HotSectors[K];const FIntPoint C=CellOf(K);List+=FString::Printf(TEXT(" %d,%d:%d%s%s"),C.X,C.Y,S.Source,S.bTeam[0]?TEXT("a"):TEXT(""),S.bTeam[1]?TEXT("b"):TEXT(""));}
  UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_HOT_SECTORS t=%.0f player=%d attack=%d heat=%d team_sectors=%d list=%s"),Now,BySource[1],BySource[2],BySource[3],TeamSectors,*List);
 }
}

bool UBattleLeaderRuntime::DefendSmokeEnabled()
{
 static const bool b=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleDefendSmoke="),V);return V!=0;}();
 return b;
}

// Fixed-scene self-test (2026-11-25, the allocation note: test the boundaries on a small fixed scene before any seed comparison;
// Codex review of the first version applied). It checks the pieces of the hot-sector choice and one defensive placement on the
// real map cover; it does not judge strength. All hot-sector state and the diagnostic counters are saved first and restored at the
// end, so the match goes on as without the test (log lines stay).
//  H1 the player's circle: from a sector centre it touches all 9 sectors around, from a sector corner the 4 that meet there.
//  H2 the defender is not detailed before it observed: the attackers of an order are, the other team only once observed; in the
//     player's sectors the player's team is, the other team only once observed; in a contact sector only an observing team.
//  H3 the shared attack limit: the teams take turns; at most 6 sectors; a listed sector takes the other attacking team.
//  H4 the linger is 10 s: a player sector (no pawn refreshes it) is kept with the player's team detailed just before it runs out
//     and leaves just after (other sources cleared, so nothing else can list it again).
//  D1 placement: a team-0 rifle squad (6 or more living) holding the sector with the most cover slots, facing the enemy home.
//     Every living member gets the candidates in some order, each once; 3 teams; the same input gives the same orders. Both his
//     order and the plain nearest-first order go through the same greedy allocation (a taken slot within 1.9 m is skipped):
//     the spread allocation stands wider across than the plain one, and at least 6 m from the leftmost to the rightmost soldier.
void UBattleLeaderRuntime::RunDefendSmoke(double Now)
{
 int32 Failures=0;
 auto Check=[&](bool bOk,const TCHAR* What){UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DEFEND_SMOKE %s %s"),bOk?TEXT("PASS"):TEXT("FAIL"),What);if(!bOk)++Failures;};
 // Saved state (restored at the end).
 const auto SavedSectors=HotSectors;const auto SavedHeat=HotHeat;const auto SavedObs0=HotObserved[0];const auto SavedObs1=HotObserved[1];
 const auto SavedSince=HotAttackSince;const auto SavedAlive=HotAlive;const double SavedNext=HotNext,SavedNextLog=HotNextLog;
 const auto SavedDiag=SightExperiment.CoverDiag;
 // H1
 {
  TSet<int32> Mid,Corner;const FIntPoint C(10,7);
  HotPlayerCells(BattleSectors::Center(C),Mid);
  HotPlayerCells(BattleSectors::Center(C)-FVector(BattleSectors::Size*.5,BattleSectors::Size*.5,0),Corner);
  Check(Mid.Num()==9,TEXT("H1 player circle from a sector centre touches 9 sectors"));
  Check(Corner.Num()==4,TEXT("H1 player circle from a sector corner touches 4 sectors"));
 }
 // H2
 {
  FHotSector A;A.Source=2;A.Attackers=1; // team 0 attacks the sector
  Check(HotTeamDetailed(A,0,1,false),TEXT("H2 attackers detailed at once"));
  Check(!HotTeamDetailed(A,1,1,false),TEXT("H2 defender not detailed before it observed"));
  Check(HotTeamDetailed(A,1,1,true),TEXT("H2 defender detailed once it observed"));
  FHotSector P;P.Source=1;P.PlayerTeam=0;
  Check(HotTeamDetailed(P,0,0,false)&&!HotTeamDetailed(P,1,0,false)&&HotTeamDetailed(P,1,0,true),TEXT("H2 player sector: player's team, the other team once observed"));
  FHotSector H;H.Source=3;
  Check(!HotTeamDetailed(H,0,0,false)&&HotTeamDetailed(H,0,0,true),TEXT("H2 contact sector: only once observed"));
 }
 // H3
 {
  Check(HotAttackOrder({1,2,3},{10})==TArray<int32>({1,10,2,3}),TEXT("H3 the teams take turns for the shared limit"));
  TMap<int32,uint8> Attack;int32 Added=0;
  for(int32 K=0;K<8;++K)Added+=HotAttackAdd(Attack,K,K%2)?1:0;
  Check(Added==AttackMax&&Attack.Num()==AttackMax,TEXT("H3 at most 6 attack-order sectors"));
  Check(HotAttackAdd(Attack,0,1)&&Attack[0]==3,TEXT("H3 a listed sector takes the other attacking team when the list is full"));
 }
 // H4
 {
  HotSectors.Reset();HotHeat.Reset();HotObserved[0].Reset();HotObserved[1].Reset();HotAttackSince.Reset();
  Check(FMath::IsNearlyEqual(Linger,10.),TEXT("H4 the linger is 10 s"));
  const int32 K=Key(FIntPoint(1,1));
  FHotSector P;P.Source=1;P.PlayerTeam=0;P.Since=Now;P.Until=Now+Linger;HotSectors.Add(K,P); // as registered at Now
  HotNext=0;StepHotSectors(Now+Linger-.5);
  const FHotSector* In=HotSectors.Find(K);
  Check(In&&In->Source==1&&In->bTeam[0],TEXT("H4 player sector kept with the player's team just before the linger runs out"));
  HotNext=0;StepHotSectors(Now+Linger+.5);
  const FHotSector* Out=HotSectors.Find(K);
  Check(!Out||Out->Source!=1,TEXT("H4 player sector leaves just after the linger"));
 }
 // D1
 {
  auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
  Cover->BuildSlots();
  TMap<int32,int32> PerCell;for(const FVector& At:Cover->GetSlots()){const FIntPoint C=BattleSectors::Cell(At);if(BattleSectors::Valid(C))++PerCell.FindOrAdd(Key(C));}
  int32 BestCell=INDEX_NONE,BestN=0;for(const auto& P:PerCell)if(P.Value>BestN||(P.Value==BestN&&P.Key<BestCell)){BestN=P.Value;BestCell=P.Key;}
  const FBattleSquadState* Squad=nullptr;
  {
   TArray<int32> SIDs;Store->GetSquads().GetKeys(SIDs);SIDs.Sort();
   for(int32 SID:SIDs)
   {
    const auto* S=Store->FindSquad(SID);if(!S||S->Team!=0||S->SquadType!=BattleSquadType::Rifle||S->CommandStatus!=EBattleCommandStatus::Active)continue;
    int32 Living=0;for(int32 ID:S->Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()&&Bodies.Contains(ID))++Living;
    if(Living>=6){Squad=S;break;}
   }
  }
  Check(BestCell!=INDEX_NONE&&BestN>=12,TEXT("D1 a sector with at least 12 cover slots"));
  Check(Squad!=nullptr,TEXT("D1 a team-0 rifle squad with 6 living"));
  if(BestCell!=INDEX_NONE&&Squad&&Match)
  {
   const FIntPoint Cell=CellOf(BestCell);const FVector Mid=BattleSectors::Center(Cell);
   FBattleSquadState S=*Squad;S.Destination=Mid;S.Order=EBattleOrder::Defend;S.bSectorMission=true;
   const FVector Forward=(Match->AreaHome(1)-Mid).GetSafeNormal2D();const FVector Right(-Forward.Y,Forward.X,0);
   TArray<FVector> Candidates;
   for(int32 Slot:Cover->NearbySlots(Mid,3600.f,256)){const FVector At=Cover->GetSlots()[Slot];if(BattleSectors::Cell(At)==Cell)Candidates.Add(At);}
   TMap<int32,TArray<FVector>> Order,Again;
   const int32 Teams3=SightExperiment.CoverDiag.FindRef(FName(TEXT("defend_spread_teams_3")));
   PlanDefendSpread(S,Forward,Right,Candidates,Order);
   const bool bThree=!SightExperiment.Enabled||SightExperiment.CoverDiag.FindRef(FName(TEXT("defend_spread_teams_3")))==Teams3+1;
   PlanDefendSpread(S,Forward,Right,Candidates,Again);
   TArray<int32> Living;for(int32 ID:S.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())Living.Add(ID);
   auto Less=[](const FVector& A,const FVector& B){return A.X!=B.X?A.X<B.X:A.Y!=B.Y?A.Y<B.Y:A.Z<B.Z;};
   TArray<FVector> Sorted=Candidates;Sorted.Sort(Less);
   bool bPerm=Order.Num()==Living.Num();
   for(int32 ID:Living){const auto* O=Order.Find(ID);if(!O){bPerm=false;continue;}TArray<FVector> X=*O;X.Sort(Less);bPerm&=X==Sorted;}
   Check(bPerm,TEXT("D1 every living member gets every candidate exactly once"));
   Check(Living.Num()>=6&&bThree,TEXT("D1 three teams for 6 or more living"));
   Check(Order.OrderIndependentCompareEqual(Again),TEXT("D1 the same input gives the same orders"));
   if(bPerm&&Living.Num()>=2)
   {
    // The same greedy allocation for both orders.
    auto Allocate=[&](TFunctionRef<const TArray<FVector>&(int32)> OrderOf){TArray<FVector> Taken;for(int32 ID:Living)for(const FVector& At:OrderOf(ID)){bool bTaken=false;for(const FVector& T:Taken)if(FVector::DistSquared2D(T,At)<FMath::Square(190.)){bTaken=true;break;}if(!bTaken){Taken.Add(At);break;}}return Taken;};
    TArray<FVector> Plain=Candidates;Plain.Sort([&](const FVector& A,const FVector& B){const double DA=FVector::DistSquared2D(A,Mid),DB=FVector::DistSquared2D(B,Mid);return DA!=DB?DA<DB:A.X!=B.X?A.X<B.X:A.Y<B.Y;});
    const TArray<FVector> Spread=Allocate([&](int32 ID)->const TArray<FVector>&{return Order[ID];});
    const TArray<FVector> Near=Allocate([&](int32)->const TArray<FVector>&{return Plain;});
    auto Width=[&](const TArray<FVector>& G){double Lo=DBL_MAX,Hi=-DBL_MAX;for(const FVector& V:G){const double L=FVector::DotProduct(V-Mid,Right);Lo=FMath::Min(Lo,L);Hi=FMath::Max(Hi,L);}return G.Num()?Hi-Lo:0.;};
    const double WS=Width(Spread),WP=Width(Near);
    Check(Spread.Num()==Living.Num(),TEXT("D1 the spread allocation places every member"));
    Check(WS>=600.,TEXT("D1 the spread allocation is at least 6 m across"));
    Check(WS>WP,TEXT("D1 the spread allocation stands wider across than the nearest-first one"));
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DEFEND_SMOKE scene sector=%d,%d slots=%d candidates=%d members=%d width_spread_cm=%.0f width_plain_cm=%.0f"),Cell.X,Cell.Y,BestN,Candidates.Num(),Living.Num(),WS,WP);
   }
  }
 }
 HotSectors=SavedSectors;HotHeat=SavedHeat;HotObserved[0]=SavedObs0;HotObserved[1]=SavedObs1;HotAttackSince=SavedSince;HotAlive=SavedAlive;
 HotNext=SavedNext;HotNextLog=SavedNextLog;SightExperiment.CoverDiag=SavedDiag;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_DEFEND_SMOKE_RESULT failures=%d"),Failures);
}
