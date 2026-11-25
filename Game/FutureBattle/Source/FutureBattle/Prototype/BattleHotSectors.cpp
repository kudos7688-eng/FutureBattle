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

bool UBattleLeaderRuntime::IsHotSector(int32 Team,const FVector& At) const
{
 if(Team<0||Team>1)return false;
 const FHotSector* S=HotSectors.Find(Key(BattleSectors::Cell(At)));return S&&S->bTeam[Team];
}

void UBattleLeaderRuntime::StepHotSectors(double Now)
{
 if(!HotSectorsEnabled()||!BattleFieldLayout::Of(GetWorld()).bSectors||Now<HotNext)return;
 HotNext=Now+1.;
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
  PlayerTeam=U->Team;const FVector P=Pawn->GetActorLocation();const FIntPoint C0=BattleSectors::Cell(P);
  for(int32 DX=-1;DX<=1;++DX)for(int32 DY=-1;DY<=1;++DY)
  {
   const FIntPoint C(C0.X+DX,C0.Y+DY);if(!BattleSectors::Valid(C))continue;
   const FVector Mid=BattleSectors::Center(C);const double Half=BattleSectors::Size*.5;
   const double Dx=FMath::Max(0.,FMath::Abs(P.X-Mid.X)-Half),Dy=FMath::Max(0.,FMath::Abs(P.Y-Mid.Y)-Half);
   if(Dx*Dx+Dy*Dy<=PlayerRadius*PlayerRadius)Player.Add(Key(C));
  }
 }
 // 2 Attack orders (earliest group first).
 TMap<int32,uint8> Attack; // sector -> attacking teams (a bit per team)
 if(Match)
 {
  // The teams take turns (each team's groups in ID order), so the shared limit does not favour the team whose groups were made first.
  TArray<int32> ByTeam[2];
  {TArray<int32> GIDs;Match->Groups.GetKeys(GIDs);GIDs.Sort();
   for(int32 GID:GIDs){const auto& G=Match->Groups[GID];if((G.Kind==0||G.Kind==2)&&G.Phase==1&&Match->Objectives.IsValidIndex(G.Objective)&&G.Team>=0&&G.Team<=1)ByTeam[G.Team].Add(GID);}}
  TArray<int32> Order;for(int32 I=0;I<FMath::Max(ByTeam[0].Num(),ByTeam[1].Num());++I)for(int32 T=0;T<2;++T)if(I<ByTeam[T].Num())Order.Add(ByTeam[T][I]);
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
    if(!Attack.Contains(K)&&Attack.Num()>=AttackMax)continue;
    Attack.FindOrAdd(K)|=uint8(1<<G.Team);
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
   S.bTeam[Team]=(S.Source==1&&Team==S.PlayerTeam)||((Attack.FindRef(P.Key)&(1<<Team))!=0)||Observed(Team,P.Key); // any source: the attackers of an order on it
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
