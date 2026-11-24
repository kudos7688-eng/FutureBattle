#include "BattleSuppression.h"
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
 return (T.bEnabled||ExoSuppressionApplies(ID)||ArmsCoverSuppressed(ID)) && Suppression(ID,Now)>=T.PinLevel;
}

float UBattleLeaderRuntime::SuppressionSpreadScale(int32 ID,double Now) const
{
 const auto& T=BattleSuppression::Tuning();
 return (T.bEnabled||ExoSuppressionApplies(ID)||ArmsCoverSuppressed(ID))?1.f+T.SpreadScale*Suppression(ID,Now):1.f;
}

void UBattleLeaderRuntime::AddSuppression(int32 ID,float Amount,double Now)
{
 const auto& T=BattleSuppression::Tuning();
 if((!T.bEnabled&&!ExoSuppressionApplies(ID)&&!bArmsCoverShot) || Amount<=0)return;
 if(bArmsCoverShot)SightExperiment.Cover(TEXT("arms_cover_fire_suppress_events"));
 // Legacy immunity is preserved unless the shield-suppression trial is explicitly enabled.
 if(!BattleTactics::ExoShieldSuppressionEnabled())
 if(const auto* Store=GetWorld()->GetSubsystem<UBattleState>();Store->HasShields())if(const auto* U=Store->Find(ID);U&&U->Shield>0.f)return;
 double& ZeroAt=SuppressionZeroAt.FindOrAdd(ID);
 // Linear decay is stored as the time the level reaches zero; the level is capped at 1.
 ZeroAt=FMath::Min(FMath::Max(ZeroAt,Now)+Amount/T.DecayPerSecond,Now+1./T.DecayPerSecond);
 SightExperiment.Cover(TEXT("suppression_events"));
}

void UBattleLeaderRuntime::SuppressNear(int32 ShooterTeam,FVector Start,FVector End,double Now,float Scale)
{
 const auto& T=BattleSuppression::Tuning();
 // Covering fire (-BattleArmsCoverFire, BattleArmsSector.cpp) suppresses even with this trial off.
 if(!T.bEnabled&&!BattleTactics::ExoShieldSuppressionEnabled()&&!bArmsCoverShot)return;
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

bool BattleTactics::ExoShieldSuppressionEnabled()
{
 static const bool On=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleExoShieldSuppression="),V);return V!=0;}();
 return On;
}
float BattleTactics::ExoShieldHitDelay()
{
 const auto* T=GetDefault<UBattleTuning>();
 if(!ExoShieldSuppressionEnabled())return FMath::Max(0.f,T->ExoShieldDelay);
 const float V=T->ExoShieldSuppressionDelay;
 return FMath::IsFinite(V)?FMath::Max(0.f,V):5.f;
}
bool UBattleLeaderRuntime::ExoSuppressionApplies(int32 ID) const
{
 if(!BattleTactics::ExoShieldSuppressionEnabled())return false;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
 return U&&U->IsAlive()&&S&&S->SquadType==BattleSquadType::Exo;
}
