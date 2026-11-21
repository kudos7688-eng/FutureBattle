// Strategy layer (ROADMAP stage 4, Docs/design-strategy-layer.md). Stage S1: the team's situation map (theatres = the objectives'
// Voronoi cells plus the two home zones) built from what the team knows - own soldiers, enemies seen now, scout reports, and
// remembered contacts weighted by age - and the evaluation log BATTLE_SITUATION (-BattleSituationLog=1), which adds the true enemy
// count of each theatre for evaluation only. Nothing here changes the match: no cache is refreshed (AreaIntelView is not called)
// and no decision reads the situation yet.
#include "BattleMatch.h"
#include "BattleLeaderRuntime.h"
#include "BattlePerception.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "FutureBattle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace BattleSituationValues
{
 constexpr double HomeZone=15000.;       // cm: within 150 m of a home is that home's zone
 constexpr double FreshAge=2.;           // s: a contact seen this recently counts fully (bVisible is false right after a restore)
 constexpr double MemoryAge=90.;         // s: remembered contacts and scout reports weigh 1 - age / MemoryAge (report delay + walk)
 constexpr float MemoryConfidence=.35f;  // remembered contacts above this confidence count (the minimap's remembered dots)
 constexpr double UnknownAge=60.;        // s: neither the point nor an enemy in the theatre seen for this long = unknown
 constexpr double EnemyAhead=1.2,OwnAhead=1.5;
 constexpr double EnemyAheadMin=3.;      // known enemies (fresh + weighted memory) needed for "enemy ahead"
 constexpr double LogEvery=30.;          // s of match time between BATTLE_SITUATION blocks
}

bool UBattleMatch::SituationLogOn()
{
 static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSituationLog="),V);return V!=0;}();
 return bOn;
}

int32 UBattleMatch::TheatreOf(int32 Team,const FVector& P) const
{
 const int32 N=Objectives.Num();if(N<1||Team<0||Team>1)return INDEX_NONE;
 if(FVector::DistSquared2D(P,AreaHome(1-Team))<=FMath::Square(BattleSituationValues::HomeZone))return N;
 if(FVector::DistSquared2D(P,AreaHome(Team))<=FMath::Square(BattleSituationValues::HomeZone))return N+1;
 // As AreaFronts: lanes by Y on three-objective maps (A-D), else the nearest objective.
 auto Dist=[&](int32 I){return N==3?FMath::Abs(P.Y-Objectives[I].Position.Y):FVector::DistSquared2D(P,Objectives[I].Position);};
 int32 Best=0;double BestD=Dist(0);
 for(int32 I=1;I<N;++I){const double D=Dist(I);if(D<BestD){BestD=D;Best=I;}}
 return Best;
}

void UBattleMatch::BuildSituation(int32 Team,FBattleSituation& Out) const
{
 Out=FBattleSituation();
 const int32 N=Objectives.Num();if(N<1||Team<0||Team>1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* Perception=GetWorld()->GetSubsystem<UBattlePerception>();const auto* T=GetDefault<UBattleTuning>();
 if(!Store||!R)return;
 using namespace BattleSituationValues;
 const double Now=GetWorld()->GetTimeSeconds();
 Out.At=Elapsed;Out.Theatres.SetNum(N+2);
 TArray<FGroupStrength> OwnPower;OwnPower.SetNum(N+2);
 // Known enemies by unit ID: the freshest sighting among the team's soldiers (soldiers in ID order, so ties keep the same one).
 struct FKnown{FVector At=FVector::ZeroVector;double Age=0;uint8 Type=255;};
 TMap<int32,FKnown> Known;
 TArray<int32> IDs;Store->GetUnits().GetKeys(IDs);IDs.Sort();
 for(int32 ID:IDs)
 {
  const auto* U=Store->Find(ID);if(!U||U->Team!=Team||!U->IsAlive())continue;
  const FVector P=R->Position(ID);
  if(const auto* S=Store->FindSquad(U->SquadID);S&&GroupSquadPresent(*S))
   if(const int32 Th=TheatreOf(Team,P);Th!=INDEX_NONE){++Out.Theatres[Th].Own;OwnPower[Th].Add(S->SquadType);}
  const auto* Contacts=Perception?Perception->GetContacts(ID):nullptr;if(!Contacts)continue;
  for(const auto& C:*Contacts)
  {
   const FBattleContact& K=C.Value;
   // Long sight enters the team's knowledge only through the scouts' reports (as AreaIntelView).
   if(IsScoutLongContact(ID,K))continue;
   const double Age=K.bVisible?0.:FMath::Max(0.,Now-K.LastSeenTime);
   if(!K.bVisible&&(K.Confidence<=MemoryConfidence||Age>=MemoryAge))continue;
   const FKnown* Old=Known.Find(C.Key);
   if(!Old||Age<Old->Age){FKnown New;New.At=K.LastKnownPosition;New.Age=Age;New.Type=K.ObservedSquadType;Known.Add(C.Key,New);}
  }
 }
 // An enemy of unknown type counts as one rifleman (one seen soldier is one soldier; no guess at what else is there).
 const double RifleWeight=T->GroupAntiInfantry.IsValidIndex(BattleSquadType::Rifle)&&FMath::IsFinite(T->GroupAntiInfantry[BattleSquadType::Rifle])?
  double(FMath::Clamp(T->GroupAntiInfantry[BattleSquadType::Rifle],0.f,100.f)):1.;
 auto AddEnemy=[&](const FVector& At,double Age,uint8 Type)
 {
  const int32 Th=TheatreOf(Team,At);if(Th==INDEX_NONE)return;
  auto& X=Out.Theatres[Th];
  const double W=Age<=FreshAge?1.:FMath::Max(0.,1.-Age/MemoryAge);if(W<=0)return;
  if(Age<=FreshAge)++X.Fresh;else X.Memory+=W;
  FGroupStrength One;One.Add(Type);X.KnownPower+=W*(One.Total()+One.Unknown*RifleWeight);
  if(X.EnemySeenAge<0||Age<X.EnemySeenAge)X.EnemySeenAge=Age;
 };
 TArray<int32> Keys;Known.GetKeys(Keys);Keys.Sort();
 for(int32 Key:Keys){const FKnown& K=Known[Key];AddEnemy(K.At,K.Age,K.Type);}
 // Scout reports of the last ScoutReportMemory seconds: positions only (no identity), aged from the report.
 TArray<int32> Scouts;ScoutReports.GetKeys(Scouts);Scouts.Sort();
 for(int32 SID:Scouts)
 {
  const FScoutReport& Rep=ScoutReports[SID];const auto* S=Store->FindSquad(SID);
  if(!S||S->Team!=Team||Rep.ReportedAt<0||Elapsed-Rep.ReportedAt>T->ScoutReportMemory)continue;
  for(const FVector& At:Rep.Positions)AddEnemy(At,FMath::Max(0.,Elapsed-Rep.ReportedAt),255);
 }
 // Points: when the team last observed each point (Area.SeenAt, from the existing 0.2 s point checks) and its observed owner.
 for(int32 I=0;I<N+2;++I)
 {
  auto& X=Out.Theatres[I];X.OwnPower=OwnPower[I].Total();
  if(I<N)
  {
   const int32 Slot=Team*N+I;
   if(Area.SeenAt.IsValidIndex(Slot)&&Area.SeenAt[Slot]>=0)X.PointSeenAge=FMath::Max(0.,Elapsed-Area.SeenAt[Slot]);
   X.Owner=ObservedOwner(Team,I);
  }
  const double KnownCount=X.Fresh+X.Memory;
  const bool bPointStale=I>=N||X.PointSeenAge<0||X.PointSeenAge>UnknownAge,bEnemyStale=X.EnemySeenAge<0||X.EnemySeenAge>UnknownAge;
  if(bPointStale&&bEnemyStale)X.State=0;
  else if(KnownCount>=EnemyAheadMin&&X.KnownPower>=X.OwnPower*EnemyAhead)X.State=1;
  else if(X.OwnPower>0&&X.OwnPower>=X.KnownPower*OwnAhead)X.State=3;
  else X.State=2;
 }
}

void UBattleMatch::UpdateSituationLog()
{
 if((!SituationLogOn()&&StrategyLevel()<1)||!bArea||Phase!=EBattleMatchPhase::Combat||Objectives.IsEmpty())return;
 const int32 Bucket=FMath::FloorToInt(Elapsed/BattleSituationValues::LogEvery);
 if(Bucket==SituationLogBucket)return;
 SituationLogBucket=Bucket;if(Bucket<1)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!Store||!R)return;
 const int32 N=Objectives.Num();
 static const TCHAR* States[]={TEXT("unknown"),TEXT("enemy"),TEXT("even"),TEXT("own")};
 TArray<int32> IDs;Store->GetUnits().GetKeys(IDs);IDs.Sort();
 for(int32 Team=0;Team<2;++Team)
 {
  FBattleSituation S;BuildSituation(Team,S);if(S.Theatres.Num()!=N+2)continue;
  // Evaluation only: the true enemy soldiers in each theatre, counted here and written to the log; no function returns them.
  TArray<int32> TrueCount;TrueCount.Init(0,N+2);TArray<FGroupStrength> TruePower;TruePower.SetNum(N+2);
  for(int32 ID:IDs)
  {
   const auto* U=Store->Find(ID);if(!U||U->Team==Team||!U->IsAlive())continue;
   const int32 Th=TheatreOf(Team,R->Position(ID));if(Th==INDEX_NONE)continue;
   ++TrueCount[Th];if(const auto* Sq=Store->FindSquad(U->SquadID))TruePower[Th].Add(Sq->SquadType);
  }
  for(int32 I=0;I<N+2;++I)
  {
   const auto& X=S.Theatres[I];
   UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_SITUATION team=%d t=%.0f theatre=%d kind=%s state=%s own=%d ownP=%.1f fresh=%d mem=%.1f knownP=%.1f seenAge=%.0f pointAge=%.0f owner=%d | true=%d trueP=%.1f"),
    Team,Elapsed,I,I<N?TEXT("point"):I==N?TEXT("enemyhome"):TEXT("ownhome"),States[FMath::Min<int32>(X.State,3)],X.Own,X.OwnPower,X.Fresh,X.Memory,X.KnownPower,
    X.EnemySeenAge,X.PointSeenAge,X.Owner,TrueCount[I],TruePower[I].Total());
  }
 }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Stage S2 (-BattleStrategy=2): recon -> focus attack. The team deploys as before until the first review (StrategyOpen x match
// length / 600 s), then concentrates on the objective with the best score from its situation map: few known enemies, own strength
// near it, close to its own front. The focus is kept at least StrategyFocusHold seconds (unless it is taken), reviewed every
// StrategyReview seconds, and handed down through FAreaIntent::FocusPoint (S6) and the AI attack group's next point.
namespace BattleStrategyValues
{
 float Seconds(const TCHAR* Name,float Default,float Min)
 {
  float V=Default;FParse::Value(FCommandLine::Get(),Name,V);
  return FMath::IsFinite(V)?FMath::Max(Min,V):Default;
 }
 constexpr double AvoidSeconds=120.;  // a focus the AI attack group gave up on is avoided this long
 constexpr double KnownWeight=1.;     // per unit of known enemy power in the theatre (memory-weighted)
 constexpr double OwnWeight=.5;       // per unit of own power in the theatre and its two nearest neighbours (ease of gathering)
 constexpr double DistanceWeight=1.;  // per 100 m from the nearest point the team observes as its own (its home when none)
 constexpr double AvoidPenalty=20.;
 constexpr double KeepBonus=3.;       // the current focus: no switch for a small difference
 // Stage S3 (-BattleStrategy=3): hold and defend -> counter.
 constexpr double HoldAfter=20.;      // s an own point's theatre stays "enemy ahead" before it is held
 constexpr double QuietAfter=30.;     // s without seeing an enemy in a held theatre before the hold ends
 constexpr double CounterSeconds=90.; // the counter candidate (where the attackers came from) keeps its bonus this long
 constexpr double CounterBonus=5.;
 constexpr double HoldWeight=2.;      // S1 weight multiplier of a held point
}

int32 UBattleMatch::StrategyLevel()
{
 static const int32 Level=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleStrategy="),V);return FMath::Clamp(V,0,4);}();
 return Level;
}

bool UBattleMatch::StrategyOn(int32 Team,int32 Level)
{
 // One team only: -BattleStrategyTeam=0/1, else the one-team trial flag of the strategy screens (-BattleAreaTrialTeam).
 static const int32 Only=[]{int32 V=-1;if(!FParse::Value(FCommandLine::Get(),TEXT("BattleStrategyTeam="),V))V=BattleTactics::AreaStrategy().TrialTeam;return V;}();
 return StrategyLevel()>=Level&&(Only<0||Only==Team);
}

int32 UBattleMatch::StrategyFocusPoint(int32 Team) const
{
 if(!bArea||Team<0||Team>1||!StrategyOn(Team,2)||!Area.StrategyFocus.IsValidIndex(Team))return INDEX_NONE;
 const int32 F=Area.StrategyFocus[Team];
 return Objectives.IsValidIndex(F)?F:INDEX_NONE;
}

int32 UBattleMatch::StrategyHoldPoint(int32 Team) const
{
 // Stage S3: the held point threatened longest (its theatre turned "enemy ahead" first); INDEX_NONE without holds.
 if(!bArea||Team<0||Team>1||!StrategyOn(Team,3)||!Area.StrategyHoldMask.IsValidIndex(Team)||Area.StrategyHoldMask[Team]==0)return INDEX_NONE;
 const int32 N=Objectives.Num();int32 Best=INDEX_NONE;float BestSince=MAX_flt;
 for(int32 I=0;I<N&&I<31;++I)if((Area.StrategyHoldMask[Team]>>I)&1)
 {
  const float Since=Area.StrategyThreatSince.IsValidIndex(Team*N+I)?Area.StrategyThreatSince[Team*N+I]:-1.f;
  const float Key=Since>=0?Since:MAX_flt/2;
  if(Best==INDEX_NONE||Key<BestSince){Best=I;BestSince=Key;}
 }
 return Best;
}

bool UBattleMatch::StrategyHolds(int32 Team,int32 Point) const
{
 return bArea&&Team>=0&&Team<2&&Point>=0&&Point<31&&StrategyOn(Team,3)&&Area.StrategyHoldMask.IsValidIndex(Team)&&((Area.StrategyHoldMask[Team]>>Point)&1)!=0;
}

void UBattleMatch::UpdateStrategy(int32 Team)
{
 if(!bArea||Team<0||Team>1||!StrategyOn(Team,2)||Objectives.IsEmpty()||Phase!=EBattleMatchPhase::Combat)return;
 using namespace BattleStrategyValues;
 static const float OpenBase=Seconds(TEXT("BattleStrategyOpen="),150.f,0.f);
 static const float Hold=Seconds(TEXT("BattleStrategyFocusHold="),90.f,0.f);
 static const float Review=Seconds(TEXT("BattleStrategyReview="),60.f,10.f);
 auto Fix=[](auto& A,auto V){if(A.Num()!=2)A.Init(V,2);};
 Fix(Area.StrategyPhase,0);Fix(Area.StrategyFocus,-1);Fix(Area.StrategyFocusAt,-1.f);Fix(Area.StrategyReviewAt,-1.f);Fix(Area.StrategyAvoid,-1);Fix(Area.StrategyAvoidUntil,-1.f);
 int32& SPhase=Area.StrategyPhase[Team];int32& Focus=Area.StrategyFocus[Team];int32& Avoid=Area.StrategyAvoid[Team];
 float& FocusAt=Area.StrategyFocusAt[Team];float& ReviewAt=Area.StrategyReviewAt[Team];float& AvoidUntil=Area.StrategyAvoidUntil[Team];
 const int32 N=Objectives.Num();
 if(Elapsed<OpenBase*MatchTimeLimit()/600.f)return;
 // The situation map is built at most once per call: for the holds (S3, every order update) and for a review.
 FBattleSituation S;bool bBuilt=false;
 auto Situation=[&]()->const FBattleSituation&{if(!bBuilt){BuildSituation(Team,S);bBuilt=true;}return S;};
 // Stage S3: an own point whose theatre stays "enemy ahead" for HoldAfter s is held (S1 weight, the attack group's reserve and
 // defence go there); the hold ends when the point is lost, or after QuietAfter s without a sighting there - then the neighbouring
 // theatre where an enemy was seen most recently (where the attackers came from, by own observations) becomes a counter candidate.
 bool bCounter=false;
 if(StrategyOn(Team,3)&&N<=31&&Situation().Theatres.Num()==N+2)
 {
  Fix(Area.StrategyHoldMask,0);Fix(Area.StrategyCounter,-1);Fix(Area.StrategyCounterUntil,-1.f);
  if(Area.StrategyThreatSince.Num()!=2*N)Area.StrategyThreatSince.Init(-1.f,2*N);
  int32& Mask=Area.StrategyHoldMask[Team];int32& Counter=Area.StrategyCounter[Team];float& CounterUntil=Area.StrategyCounterUntil[Team];
  if(Counter>=0&&Elapsed>=CounterUntil){Counter=-1;CounterUntil=-1.f;}
  for(int32 I=0;I<N;++I)
  {
   float& Since=Area.StrategyThreatSince[Team*N+I];const auto& X=S.Theatres[I];const bool bHeld=((Mask>>I)&1)!=0;
   if(ObservedOwner(Team,I)!=Team)
   {
    Since=-1.f;
    if(bHeld){Mask&=~(1<<I);UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STRATEGY_HOLD team=%d t=%.1f point=%d hold=0 reason=lost"),Team,Elapsed,I);}
    continue;
   }
   if(X.State==1){if(Since<0)Since=Elapsed;}else Since=-1.f;
   if(!bHeld&&Since>=0&&Elapsed-Since>=HoldAfter)
   {Mask|=1<<I;UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STRATEGY_HOLD team=%d t=%.1f point=%d hold=1 known=%.1f own=%.1f"),Team,Elapsed,I,X.KnownPower,X.OwnPower);}
   else if(bHeld&&X.State!=1&&(X.EnemySeenAge<0||X.EnemySeenAge>=QuietAfter))
   {
    Mask&=~(1<<I);
    // The two nearest points not own: the one where an enemy was seen most recently.
    int32 Best=INDEX_NONE;double BestAge=DBL_MAX;
    TArray<int32> Near;for(int32 J=0;J<N;++J)if(J!=I&&ObservedOwner(Team,J)!=Team)Near.Add(J);
    Near.Sort([&](int32 A,int32 B){const double DA=FVector::DistSquared2D(Objectives[A].Position,Objectives[I].Position),DB=FVector::DistSquared2D(Objectives[B].Position,Objectives[I].Position);return DA!=DB?DA<DB:A<B;});
    for(int32 K=0;K<FMath::Min(2,Near.Num());++K){const double Age=S.Theatres[Near[K]].EnemySeenAge;if(Age>=0&&Age<BestAge){BestAge=Age;Best=Near[K];}}
    if(Best!=INDEX_NONE){Counter=Best;CounterUntil=Elapsed+CounterSeconds;bCounter=true;}
    UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STRATEGY_HOLD team=%d t=%.1f point=%d hold=0 reason=quiet counter=%d"),Team,Elapsed,I,Best);
   }
  }
 }
 if(Avoid>=0&&Elapsed>=AvoidUntil){Avoid=-1;AvoidUntil=-1.f;}
 // The AI attack group gave up on the focus (its own avoid clock runs): the team avoids it too; the switch still waits for the hold.
 if(Objectives.IsValidIndex(Focus)&&Avoid!=Focus)
  for(const auto& P:Groups)
   if(P.Value.Team==Team&&!P.Value.bPlayer&&P.Value.Kind==0&&P.Value.Avoid==Focus&&Elapsed<P.Value.AvoidUntil){Avoid=Focus;AvoidUntil=Elapsed+AvoidSeconds;break;}
 const TCHAR* Why=nullptr;
 if(SPhase==0&&ReviewAt<0)Why=TEXT("open");
 else if(Objectives.IsValidIndex(Focus)&&ObservedOwner(Team,Focus)==Team)Why=TEXT("taken");
 else if(Elapsed>=ReviewAt)Why=TEXT("review");
 else if(bCounter)Why=TEXT("counter");
 if(!Why)return;
 // A focus is kept at least Hold seconds; only taking it ends it sooner.
 const bool bTaken=FCString::Strcmp(Why,TEXT("taken"))==0;
 if(SPhase==1&&Objectives.IsValidIndex(Focus)&&!bTaken&&Elapsed-FocusAt<Hold){ReviewAt=FocusAt+Hold;return;}
 if(Situation().Theatres.Num()!=N+2)return;
 const int32 Counter=StrategyOn(Team,3)&&Area.StrategyCounter.IsValidIndex(Team)?Area.StrategyCounter[Team]:INDEX_NONE;
 TArray<int32> OwnPoints;for(int32 I=0;I<N;++I)if(ObservedOwner(Team,I)==Team)OwnPoints.Add(I);
 const FVector Home=AreaHome(Team);
 auto FrontDistance=[&](int32 I)
 {
  double D=FVector::Dist2D(Objectives[I].Position,Home);
  for(int32 J:OwnPoints)D=FMath::Min(D,FVector::Dist2D(Objectives[I].Position,Objectives[J].Position));
  return D;
 };
 // Own power in the theatre and in the two theatres whose points are nearest its point.
 auto OwnNear=[&](int32 I)
 {
  int32 A=INDEX_NONE,B=INDEX_NONE;double DA=DBL_MAX,DB=DBL_MAX;
  for(int32 J=0;J<N;++J)if(J!=I)
  {
   const double D=FVector::DistSquared2D(Objectives[I].Position,Objectives[J].Position);
   if(D<DA){B=A;DB=DA;A=J;DA=D;}else if(D<DB){B=J;DB=D;}
  }
  return S.Theatres[I].OwnPower+(A!=INDEX_NONE?S.Theatres[A].OwnPower:0.)+(B!=INDEX_NONE?S.Theatres[B].OwnPower:0.);
 };
 int32 Best=INDEX_NONE,Next=INDEX_NONE,Unknown=0;double BestScore=0,NextScore=0;
 for(int32 I=0;I<N;++I)
 {
  if(OwnPoints.Contains(I))continue;
  const auto& X=S.Theatres[I];
  // An unknown theatre is not guessed at: it is not a focus candidate (user decision 2026-11-17).
  if(X.State==0){++Unknown;continue;}
  const double Score=-KnownWeight*X.KnownPower+OwnWeight*OwnNear(I)-DistanceWeight*FrontDistance(I)/10000.-(I==Avoid?AvoidPenalty:0.)+(I==Focus?KeepBonus:0.)+(I==Counter?CounterBonus:0.);
  if(Best==INDEX_NONE||Score>BestScore){Next=Best;NextScore=BestScore;Best=I;BestScore=Score;}
  else if(Next==INDEX_NONE||Score>NextScore){Next=I;NextScore=Score;}
 }
 const int32 Old=Focus;
 if(Best!=Focus){Focus=Best;FocusAt=Elapsed;}
 SPhase=Best!=INDEX_NONE?1:0;ReviewAt=Elapsed+Review;
 const auto* F=Objectives.IsValidIndex(Best)?&S.Theatres[Best]:nullptr;
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_STRATEGY team=%d t=%.1f phase=%d focus=%d old=%d reason=%s score=%.1f known=%.1f own=%.1f next=%d nextScore=%.1f unknown=%d avoid=%d"),
  Team,Elapsed,SPhase,Best,Old,Why,Best!=INDEX_NONE?BestScore:0.,F?F->KnownPower:0.,F?OwnNear(Best):0.,Next,Next!=INDEX_NONE?NextScore:0.,Unknown,Avoid);
}
