#include "BattleSuppression.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"

BattleSuppression::FTuning& BattleSuppression::MutableTuning()
{
 static FTuning Value=[]()
 {
  FTuning T;int32 Enabled=0;const TCHAR* Line=FCommandLine::Get();
  FParse::Value(Line,TEXT("BattleSuppression="),Enabled);T.bEnabled=Enabled!=0;
  FParse::Value(Line,TEXT("BattleSuppressionRadius="),T.Radius);FParse::Value(Line,TEXT("BattleSuppressionPerShot="),T.PerShot);
  FParse::Value(Line,TEXT("BattleSuppressionDecay="),T.DecayPerSecond);FParse::Value(Line,TEXT("BattleSuppressionSpread="),T.SpreadScale);
  FParse::Value(Line,TEXT("BattleSuppressionPin="),T.PinLevel);
  FParse::Value(Line,TEXT("BattleSuppressionSupport="),T.SupportScale);
  T.Radius=FMath::IsFinite(T.Radius)?FMath::Clamp(T.Radius,10.f,1000.f):200.f;
  T.PerShot=FMath::IsFinite(T.PerShot)?FMath::Clamp(T.PerShot,0.f,1.f):.12f;
  T.DecayPerSecond=FMath::IsFinite(T.DecayPerSecond)?FMath::Clamp(T.DecayPerSecond,.01f,10.f):.25f;
  T.SpreadScale=FMath::IsFinite(T.SpreadScale)?FMath::Clamp(T.SpreadScale,0.f,10.f):1.5f;
  T.PinLevel=FMath::IsFinite(T.PinLevel)?FMath::Clamp(T.PinLevel,.05f,2.f):.5f;
  T.SupportScale=FMath::IsFinite(T.SupportScale)?FMath::Clamp(T.SupportScale,.1f,10.f):1.f;
  return T;
 }();
 return Value;
}
const BattleSuppression::FTuning& BattleSuppression::Tuning() { return MutableTuning(); }

float UBattleLeaderRuntime::Suppression(int32 ID,double Now) const
{
 const double* ZeroAt=SuppressionZeroAt.Find(ID);
 return ZeroAt?float(FMath::Clamp((*ZeroAt-Now)*BattleSuppression::Tuning().DecayPerSecond,0.,1.)):0.f;
}

bool UBattleLeaderRuntime::IsPinned(int32 ID,double Now) const
{
 const auto& T=BattleSuppression::Tuning();
 return T.bEnabled && Suppression(ID,Now)>=T.PinLevel;
}

float UBattleLeaderRuntime::SuppressionSpreadScale(int32 ID,double Now) const
{
 const auto& T=BattleSuppression::Tuning();
 return T.bEnabled?1.f+T.SpreadScale*Suppression(ID,Now):1.f;
}

void UBattleLeaderRuntime::AddSuppression(int32 ID,float Amount,double Now)
{
 const auto& T=BattleSuppression::Tuning();
 if(!T.bEnabled || Amount<=0)return;
 double& ZeroAt=SuppressionZeroAt.FindOrAdd(ID);
 // Linear decay is stored as the time the level reaches zero; the level is capped at 1.
 ZeroAt=FMath::Min(FMath::Max(ZeroAt,Now)+Amount/T.DecayPerSecond,Now+1./T.DecayPerSecond);
 SightExperiment.Cover(TEXT("suppression_events"));
}

void UBattleLeaderRuntime::SuppressNear(int32 ShooterTeam,FVector Start,FVector End,double Now,float Scale)
{
 const auto& T=BattleSuppression::Tuning();
 if(!T.bEnabled)return;
 // Physical effect of the bullet path, like damage: it uses true positions and is not an AI decision input.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();FBox Bounds(ForceInit);Bounds+=Start;Bounds+=End;
 const FBox Reach=Bounds.ExpandBy(T.Radius+40.f);
 for(const auto& Body:Bodies)
 {
  const FVector Chest=Position(Body.Key)+FVector(0,0,35);
  if(!Reach.IsInsideOrOn(Chest))continue;
  const auto* U=Store->Find(Body.Key);
  if(!U || !U->IsAlive() || U->Team==ShooterTeam)continue;
  const float Distance=FMath::PointDistToSegment(Chest,Start,End);
  if(Distance<T.Radius)AddSuppression(Body.Key,T.PerShot*Scale*(1.f-Distance/T.Radius),Now);
 }
}
