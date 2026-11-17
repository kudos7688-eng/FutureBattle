// Tanks, stage 1 (ROADMAP 2026-11-14 heavy equipment; user decisions 2026-11-17): a one-vehicle squad led by an Actor with a wide
// body, a separate navigation agent ("Vehicle" in DefaultEngine.ini), armour, terrain speed, a limited turn rate, a narrow forward
// view and a long distance at which enemies see it. Placeholder body: engine basic shapes (a Fab model can replace them later).
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleUnit.h"
#include "BattleVehicleNavMesh.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "FutureBattle.h"

float UBattleLeaderRuntime::VehicleSpotRange() const
{
 const auto* T=GetDefault<UBattleTuning>();
 return FMath::Max(0.f,BattleRanges::Get().bActive?T->TankSpotRangeProfile:T->TankSpotRange);
}

float UBattleLeaderRuntime::ArmorScale(int32 VictimID,EBattleWeapon Weapon) const
{
 return IsVehicle(VictimID)&&Weapon!=EBattleWeapon::AntiTank?0.f:1.f;
}

void UBattleLeaderRuntime::RefreshVehicles()
{
 VehicleIDs.Reset();
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(const auto& P:Store->GetSquads())
  if(BattleSquadType::IsVehicle(P.Value.SquadType))if(const auto* U=Store->Find(P.Value.LeaderUnitID);U&&U->IsAlive())VehicleIDs.Add(P.Value.LeaderUnitID);
}

void UBattleLeaderRuntime::ConfigureVehicle(int32 ID)
{
 ABattleUnit* A=Actor(ID);auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* U=Store->Find(ID);
 if(!A||!U)return;
 const auto* T=GetDefault<UBattleTuning>();
 // Body: a wide capsule (collision and the navigation agent), the soldier mesh and rifle hidden.
 A->GetCapsuleComponent()->SetCapsuleSize(FMath::Max(50.f,T->TankRadius),130.f);
 A->GetMesh()->SetVisibility(false,true);if(A->Rifle)A->Rifle->SetVisibility(false);
 // Tanks path on the vehicle navigation data (the wide agent), told apart from infantry by PreferredNavData.
 auto* Move=A->GetCharacterMovement();Move->bOrientRotationToMovement=false;Move->NavAgentProps.SetPreferredNavData(ABattleVehicleNavMesh::StaticClass());Move->UpdateNavAgent(*A);
 // Placeholder shapes: hull 4.2 x 2.5 x 1.1 m, turret, barrel; team colour like the soldiers' labels.
 if(!Turrets.Contains(ID)||!Turrets[ID].IsValid())
 {
  auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  UMaterialInstanceDynamic* Paint=Base?UMaterialInstanceDynamic::Create(Base,A):nullptr;
  if(Paint)Paint->SetVectorParameterValue(TEXT("Color"),U->Team==0?FLinearColor(.18f,.42f,.38f):FLinearColor(.48f,.2f,.16f));
  auto Part=[&](UStaticMesh* Mesh,USceneComponent* Parent,FVector Location,FRotator Rotation,FVector Scale)->UStaticMeshComponent*
  {
   auto* C=NewObject<UStaticMeshComponent>(A);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
   C->SetupAttachment(Parent);C->SetRelativeLocation(Location);C->SetRelativeRotation(Rotation);C->SetRelativeScale3D(Scale);
   if(Paint)C->SetMaterial(0,Paint);C->RegisterComponent();A->AddInstanceComponent(C);return C;
  };
  USceneComponent* Root=A->GetCapsuleComponent();
  Part(Cube,Root,FVector(0,0,-75),FRotator::ZeroRotator,FVector(4.2,2.5,1.1));
  auto* Turret=Part(Cylinder,Root,FVector(-20,0,15),FRotator::ZeroRotator,FVector(1.7,1.7,.6));
  Part(Cylinder,Turret,FVector(180/1.7,0,0),FRotator(90,0,0),FVector(.12/1.7,.12/1.7,3.2/.6));
  Turrets.Add(ID,Turret);
 }
 if(A->TeamLabel)A->TeamLabel->SetRelativeLocation(FVector(0,0,260));
 // Narrow forward view (the turret turns; the hull's view does not).
 // Only when it differs (a restored tank already has it; changing the profile would reset its contacts).
 const float Fov=FMath::Clamp(T->TankFieldOfView,1.f,360.f);
 if(!FMath::IsNearlyEqual(U->Perception.FieldOfView,Fov)){FBattlePerceptionProfile P=U->Perception;P.FieldOfView=Fov;Store->SetPerceptionProfile(ID,P);}
 VehicleIDs.Add(ID);
}

void UBattleLeaderRuntime::SteerVehicle(int32 ID,FVector& Direction,float& Scale)
{
 ABattleUnit* A=Actor(ID);if(!A||Direction.IsNearlyZero())return;
 const float Dt=FMath::Min(GetWorld()->GetDeltaSeconds(),.1f);const float Rate=FMath::Max(1.f,GetDefault<UBattleTuning>()->TankTurnRate);
 const float Yaw=A->GetActorRotation().Yaw,Delta=FMath::FindDeltaAngleDegrees(Yaw,Direction.Rotation().Yaw);
 A->SetActorRotation(FRotator(0,Yaw+FMath::Clamp(Delta,-Rate*Dt,Rate*Dt),0));
 // Drive along the hull: full speed when lined up, slower while turning, turning in place beyond 60 degrees.
 Direction=A->GetActorForwardVector().GetSafeNormal2D();
 Scale*=FMath::Abs(Delta)>=60.f?0.f:FMath::Cos(FMath::DegreesToRadians(Delta));
}

void UBattleLeaderRuntime::AimTurret(int32 ID,const FVector& Direction)
{
 const auto* Turret=Turrets.Find(ID);ABattleUnit* A=Actor(ID);if(!Turret||!Turret->IsValid()||!A||Direction.IsNearlyZero())return;
 const float Dt=FMath::Min(GetWorld()->GetDeltaSeconds(),.1f);
 const float Current=(*Turret)->GetComponentRotation().Yaw,Delta=FMath::FindDeltaAngleDegrees(Current,Direction.Rotation().Yaw);
 (*Turret)->SetWorldRotation(FRotator(0,Current+FMath::Clamp(Delta,-60.f*Dt,60.f*Dt),0)); // turret: 60 deg/s
}

ANavigationData* UBattleLeaderRuntime::NavFor(int32 ID) const
{
 if(!IsVehicle(ID))return nullptr;
 ABattleUnit* A=Actor(ID);auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 return A&&Nav?Nav->GetNavDataForProps(A->GetNavAgentPropertiesRef()):nullptr;
}

bool UBattleLeaderRuntime::CanSeeVehicleFar(int32 ObserverID,const FBattleSightContext& C,int32 Target,double Now) const
{
 // Same checks as CanSeeWithContext; only the line trace result is reused for TankSightReuse seconds (performance: every soldier
 // within the long vehicle spotting range otherwise traces up to 200-400 m to each enemy tank five times a second).
 const auto* T=GetWorld()->GetSubsystem<UBattleState>()->Find(Target);
 if(!C.bAlive || !T || !T->IsAlive() || !Bodies.Contains(Target)) return false;
 const FVector P=Position(Target), D=(P-C.Position).GetSafeNormal2D();
 if(FVector::DistSquared(P,C.Position)>FMath::Max(C.RangeSquared,FMath::Square(VehicleSpotRange())) || (!C.bOmnidirectional && !D.IsNearlyZero() && FVector::DotProduct(C.Forward,D)+1.e-6<C.HalfAngleCos)) return false;
 auto& Entry=VehicleSightCache.FindOrAdd(FIntPoint(ObserverID,Target),TPair<double,bool>(-1.,false));
 if(Now+1.e-6<Entry.Key){++TankSightReused;return Entry.Value;}
 const double Start=FPlatformTime::Seconds();
 FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,C.Eye,P+FVector(0,0,BattleStance::AimOffset(StanceOf(Target))),ECC_Visibility,C.Query);
 TankSightMs+=(FPlatformTime::Seconds()-Start)*1000.;++TankSightTraces;
 if(SightHitStatsEnabled()) RecordSightHit(Hit,Target,uint64((FPlatformTime::Seconds()-Start)/FPlatformTime::GetSecondsPerCycle64()),SightFromVehicleFar);
 Entry.Key=Now+FMath::Max(0.f,GetDefault<UBattleTuning>()->TankSightReuse);Entry.Value=!Hit.bBlockingHit || HitIDs.FindRef(Hit.GetComponent())==Target;
 return Entry.Value;
}

bool UBattleLeaderRuntime::SameVehicleMission(int32 SID,EBattleOrder Order,const FVector& Goal) const
{
 const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);const auto* V=VehicleOrders.Find(SID);
 if(!S||!V||!IsVehicle(S->LeaderUnitID))return false;
 // A refused goal (no vehicle route) is retried after 5 s, not every director update.
 if(V->Refused.Equals(Goal,20)&&GetWorld()->GetTimeSeconds()<V->RetryAt){++TankOrderSkips;return true;}
 const bool Same=S->bHasMission&&S->MissionOrder==Order&&S->MissionRevision==V->MissionRevision&&V->Requested.Equals(Goal,20);
 if(Same)++TankOrderSkips;
 return Same;
}
