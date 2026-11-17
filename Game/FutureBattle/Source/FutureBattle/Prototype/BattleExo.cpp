// Armoured exoskeleton soldier (squad type 8, 2026-11-18): one armoured, shielded, fast soldier with two weapons.
// The rules shared with other squad types live with them: values in BattleSquadType (BattleState.cpp), the shield in
// UBattleState::ApplyDamage / AdvanceShields, the armour in UBattleLeaderRuntime::ArmorScale, no suppression while shielded in
// AddSuppression, the score count in UBattleMatch::AreaScore, going home for repair in UBattleSquadTactics::Evaluate.
// This file holds the second weapon (the shoulder anti-tank launcher) and the look.
// AI cognition: the launcher fires only at what the soldier sees itself (its own visible contacts).
#include "BattleLeaderRuntime.h"
#include "BattleState.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "BattleSuppression.h"
#include "BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "BattleFieldLayout.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"

void UBattleLeaderRuntime::ApplySquadLook(int32 SquadID)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SquadID);
 if(!S||S->SquadType!=BattleSquadType::Exo)return;
 // Placeholder look from engine basic shapes (a Fab model can replace it later): the soldier body drawn 1.2x, armour plates, a
 // backpack and the launcher tube on a rig at the chest, round shoulder, head and knee pieces on the bones, and a shield bubble.
 ABattleUnit* A=Actor(S->LeaderUnitID);const auto* U=Store->Find(S->LeaderUnitID);if(!A||!U||!A->GetMesh())return;
 if(const auto* Old=ExoLooks.Find(S->LeaderUnitID);Old&&Old->Rig.IsValid())return;
 auto* Body=A->GetMesh();Body->SetRelativeScale3D(FVector(1.2f));
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 auto* Clear=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
 UMaterialInstanceDynamic* Plate=Base?UMaterialInstanceDynamic::Create(Base,A):nullptr;UMaterialInstanceDynamic* Trim=Base?UMaterialInstanceDynamic::Create(Base,A):nullptr;
 if(Plate)Plate->SetVectorParameterValue(TEXT("Color"),U->Team==0?FLinearColor(.10f,.24f,.34f):FLinearColor(.36f,.13f,.10f));
 if(Trim)Trim->SetVectorParameterValue(TEXT("Color"),FLinearColor(.06f,.06f,.07f));
 auto Part=[&](UStaticMesh* Mesh,USceneComponent* Parent,FName Socket,FVector Location,FRotator Rotation,FVector Scale,UMaterialInterface* Material)->UStaticMeshComponent*
 {
  auto* C=NewObject<UStaticMeshComponent>(A);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
  C->SetupAttachment(Parent,Socket);C->SetRelativeLocation(Location);C->SetRelativeRotation(Rotation);C->SetRelativeScale3D(Scale);
  if(Material)C->SetMaterial(0,Material);C->RegisterComponent();A->AddInstanceComponent(C);return C;
 };
 // The rig sits on the upper spine and keeps the actor's heading (its rotation is absolute and set every frame), so its parts are
 // laid out in plain body axes: X forward, Y right, Z up.
 auto* Rig=NewObject<USceneComponent>(A);Rig->SetupAttachment(Body,TEXT("spine_05"));Rig->SetUsingAbsoluteRotation(true);Rig->SetUsingAbsoluteScale(true);
 Rig->RegisterComponent();A->AddInstanceComponent(Rig);
 Part(Cube,Rig,NAME_None,FVector(15,0,-6),FRotator::ZeroRotator,FVector(.14f,.46f,.40f),Plate);            // chest plate
 Part(Cube,Rig,NAME_None,FVector(13,0,-34),FRotator::ZeroRotator,FVector(.12f,.38f,.22f),Trim);            // belly plate
 Part(Cube,Rig,NAME_None,FVector(-24,0,-4),FRotator::ZeroRotator,FVector(.26f,.44f,.50f),Trim);            // power pack
 Part(Cylinder,Rig,NAME_None,FVector(-30,-14,26),FRotator::ZeroRotator,FVector(.06f,.06f,.30f),Plate);     // exhaust
 Part(Cylinder,Rig,NAME_None,FVector(2,30,30),FRotator(90,0,0),FVector(.17f,.17f,.95f),Trim);              // launcher tube over the right shoulder
 Part(Cube,Rig,NAME_None,FVector(-8,30,30),FRotator::ZeroRotator,FVector(.34f,.24f,.24f),Plate);           // launcher housing
 // Round pieces on the bones (their look does not depend on the bone axes).
 Part(Sphere,Body,TEXT("upperarm_l"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(.30f),Plate);
 Part(Sphere,Body,TEXT("upperarm_r"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(.30f),Plate);
 Part(Sphere,Body,TEXT("head"),FVector(8,0,0),FRotator::ZeroRotator,FVector(.30f),Plate);
 Part(Sphere,Body,TEXT("calf_l"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(.15f),Trim);
 Part(Sphere,Body,TEXT("calf_r"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(.15f),Trim);
 Part(Sphere,Body,TEXT("lowerarm_l"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(.17f),Trim);
 Part(Sphere,Body,TEXT("lowerarm_r"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(.17f),Trim);
 // Shield bubble around the soldier: translucent, no shadow, hidden when the shield is empty (UpdateExoLooks).
 FExoLook Look;Look.Rig=Rig;
 Look.BubblePaint=Clear?UMaterialInstanceDynamic::Create(Clear,A):nullptr;
 auto* Bubble=Part(Sphere,A->GetCapsuleComponent(),NAME_None,FVector(0,0,18),FRotator::ZeroRotator,FVector(1.7f,1.7f,2.5f),Look.BubblePaint.Get());
 Bubble->SetCastShadow(false);Look.Bubble=Bubble;
 ExoLooks.Add(S->LeaderUnitID,Look);UpdateExoLooks();
}

void UBattleLeaderRuntime::UpdateExoLooks()
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 for(auto It=ExoLooks.CreateIterator();It;++It)
 {
  const auto* U=Store->Find(It.Key());ABattleUnit* A=Actor(It.Key());auto& L=It.Value();
  if(!U||!A||!L.Rig.IsValid()){It.RemoveCurrent();continue;}
  L.Rig->SetWorldRotation(FRotator(0,A->GetActorRotation().Yaw,0));
  // The placeholder stances squash the body; the rig (absolute scale) follows its height so the plates do not tower over a prone soldier.
  if(A->GetMesh())L.Rig->SetWorldScale3D(FVector(1.f,1.f,FMath::Clamp(float(A->GetMesh()->GetRelativeScale3D().Z)/1.2f,.35f,1.f)));
  const float Max=Store->MaxShieldOf(It.Key());const float Ratio=Max>0.f&&U->IsAlive()?FMath::Clamp(U->Shield/Max,0.f,1.f):0.f;
  if(L.Bubble.IsValid())
  {
   // Follows what the body shows (an enemy hidden by the fog of war shows no bubble either).
   const bool bShow=Ratio>0.f&&A->GetMesh()&&A->GetMesh()->IsVisible();
   if(L.Bubble->IsVisible()!=bShow)L.Bubble->SetVisibility(bShow);
   if(bShow&&L.BubblePaint.IsValid())L.BubblePaint->SetVectorParameterValue(TEXT("Color"),FLinearColor(.25f,.7f,1.f,.02f+.07f*Ratio));
  }
 }
}

void UBattleLeaderRuntime::StepExoLaunchers(double Now)
{
 UpdateExoLooks();
 // Twice a second: every exoskeleton soldier with a loaded launcher looks for a target (not the one the player controls: F fires his).
 if(Now<ExoNextPick)return;
 ExoNextPick=Now+.5;
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();if(!Sense)return;
 const auto* T=GetDefault<UBattleTuning>();
 const double Engage2=FMath::Square(double(BattleRanges::Engage(EBattleWeapon::AntiTank))),Splash2=FMath::Square(FMath::Max(1.f,T->ATSplashRadius));
 TArray<int32> Shooters;
 for(const auto& P:Store->GetSquads())if(P.Value.SquadType==BattleSquadType::Exo&&P.Value.CommandStatus==EBattleCommandStatus::Active)
  for(int32 ID:P.Value.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive()&&U->Action!=EBattleAction::DirectControl&&U->Launcher>0&&U->LauncherWait<=0.f&&Bodies.Contains(ID))Shooters.Add(ID);
 Shooters.Sort();
 for(int32 ID:Shooters)
 {
  const auto* U=Store->Find(ID);const auto* Contacts=Sense->GetContacts(ID);if(!U||!Contacts)continue;
  const FVector Pos=Position(ID);
  TArray<TPair<int32,FVector>> Seen;
  for(const auto& C:*Contacts)if(C.Value.bVisible&&!C.Value.bShared&&FVector::DistSquared2D(Pos,C.Value.LastKnownPosition)<=Engage2)Seen.Add(TPair<int32,FVector>(C.Key,C.Value.LastKnownPosition));
  // A vehicle first (the nearest); soldiers only when ExoLauncherCluster or more stand within one explosion and rounds beyond the kept ones are left.
  int32 Best=INDEX_NONE;double BestScore=-1.;FVector BestAt=FVector::ZeroVector;
  for(const auto& K:Seen)
  {
   const bool Vehicle=IsVehicle(K.Key);int32 Around=0;
   if(!Vehicle){if(U->Launcher<=FMath::Max(0,T->ExoLauncherKeep))continue;for(const auto& O:Seen)Around+=!IsVehicle(O.Key)&&FVector::DistSquared2D(O.Value,K.Value)<=Splash2;if(Around<FMath::Max(1,T->ExoLauncherCluster))continue;}
   const double Score=(Vehicle?1000.:double(Around))-FVector::Dist2D(Pos,K.Value)/1.e6;
   if(Score>BestScore||(Score==BestScore&&K.Key<Best)){BestScore=Score;Best=K.Key;BestAt=K.Value;}
  }
  if(Best==INDEX_NONE)continue;
  // Line of fire from the shoulder: clear, or ending on the target or close enough for the explosion.
  const FVector Muzzle=Pos+FVector(0,0,BattleStance::EyeOffset(StanceOf(ID))),Aim=BestAt+FVector(0,0,BattleStance::AimOffset(StanceOf(Best)));
  auto Q=Sight(ID).Query;FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,Aim,ECC_Visibility,Q);
  if(Hit.bBlockingHit&&HitIDs.FindRef(Hit.GetComponent())!=Best&&FVector::DistSquared(Hit.ImpactPoint,Aim)>Splash2)continue;
  FireExoLauncher(ID,Aim,Now);
 }
}

bool UBattleLeaderRuntime::FireExoLauncher(int32 ID,const FVector& Aim,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive()||!Bodies.Contains(ID))return false;
 const int32 Sequence=U->Launcher;
 if(!Store->UseLauncher(ID))return false;
 const auto* T=GetDefault<UBattleTuning>();const int32 Team=U->Team;const auto* SS=Store->FindSquad(U->SquadID);
 const FVector Start=Position(ID)+FVector(0,0,BattleStance::EyeOffset(StanceOf(ID)));
 // Deterministic spread per soldier and round (half a degree).
 FRandomStream Random(int32(uint32(ID)*2654435761u+uint32(Sequence)*40503u+3571u));
 const FVector Direction=Random.VRandCone((Aim-Start).GetSafeNormal(),FMath::DegreesToRadians(.5f));
 auto Q=Sight(ID).Query;if(ConcealmentActors.Num())Q.AddIgnoredActors(ConcealmentActors);
 FHitResult Hit;const FVector End=Start+Direction*BattleRanges::Reach(EBattleWeapon::AntiTank);
 GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q);
 const FVector Impact=Hit.bBlockingHit?FVector(Hit.ImpactPoint):End;
 ++ExoLauncherShots;
 const bool bTyped=SS&&SS->SquadType<BattleSquadType::Count&&Team>=0&&Team<2;
 if(SightExperiment.Started){++SightExperiment.Shots;if(bTyped)++SightExperiment.TypeShots[Team][SS->SquadType];}
 SuppressNear(Team,Start,Impact,Now,BattleSuppression::Tuning().SupportScale);
 if(M&&bArea)M->DamageBase(Hit.GetActor(),Team,T->ATBaseDamage);
 // Direct hit: the full anti-tank damage, armour or not. Never own units.
 const int32 Victim=HitIDs.FindRef(Hit.GetComponent());
 if(const auto* Before=Store->Find(Victim);Before&&Before->IsAlive()&&Before->Team!=Team)
 {
  const float BeforeHP=Before->HP;const int32 VictimTeam=Before->Team;const auto* VS=Store->FindSquad(Before->SquadID);const uint8 VictimType=VS?VS->SquadType:0;
  if(Store->ApplyDamage(Victim,Team,T->ATDamage))
  {
   ++ExoLauncherHits;
   if(SightExperiment.Started)
   {
    auto& E=SightExperiment;const float Dealt=FMath::Min(BeforeHP,T->ATDamage);const bool Killed=!Store->Find(Victim)->IsAlive();++E.Hits;
    if(bTyped){E.TypeDamage[Team][SS->SquadType]+=Dealt;E.TypeKills[Team][SS->SquadType]+=Killed;}
    if(VictimType<BattleSquadType::Count&&VictimTeam>=0&&VictimTeam<2){E.TypeTaken[VictimTeam][VictimType]+=Dealt;E.TypeDeaths[VictimTeam][VictimType]+=Killed;}
    E.TeamDamage[Team]+=Dealt;E.TeamKills[Team]+=Killed;
   }
  }
 }
 Explode(ID,Impact,Victim);
 UE_LOG(LogFutureBattle,Display,TEXT("BATTLE_EXO_LAUNCH unit=%d team=%d victim=%d left=%d impact=%s"),ID,Team,Victim,Sequence-1,*Impact.ToCompactString());
 if(Team==0||(Actor(ID)&&Actor(ID)->bRevealed))
 {
  DrawDebugLine(GetWorld(),Start,Impact,Team?FColor::Red:FColor::Yellow,false,.25f,0,4);
  DrawDebugSphere(GetWorld(),Impact,FMath::Max(50.f,T->ATSplashRadius),10,Team?FColor::Red:FColor::Yellow,false,.4f,0,2);
 }
 return true;
}

int32 UBattleMatch::AreaExoTarget(int32 Team,int32 SquadID) const
{
 // The fast reserve: it reaches any point of the field in well under a minute, so it goes where the team's own knowledge says it is
 // needed now. Observed owners and known enemies only (AreaIntelView); kept ACChoiceHold seconds like the vehicles' choices.
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* R=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 const auto* S=Store?Store->FindSquad(SquadID):nullptr;
 if(!S||!R||Objectives.IsEmpty())return INDEX_NONE;
 const FVector Here=R->Position(S->LeaderUnitID);const auto& V=AreaIntelView(Team);
 // The point nearest a known enemy vehicle (the soldier carries the launcher).
 int32 VehiclePoint=INDEX_NONE;double VehicleBest=DBL_MAX;
 for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(V.EnemyKeys[I]>0&&R->IsVehicle(V.EnemyKeys[I]))
  for(int32 P=0;P<Objectives.Num();++P){const double D=FVector::DistSquared2D(Objectives[P].Position,V.Enemies[I]);if(D<VehicleBest){VehicleBest=D;VehiclePoint=P;}}
 return AreaVehicleChoice(SquadID,[&](int32 I)->double
 {
  const int32 Known=V.EnemyAt.IsValidIndex(I)?V.EnemyAt[I]:0,Own=V.OwnAt.IsValidIndex(I)?V.OwnAt[I]:0;const bool bOurs=ObservedOwner(Team,I)==Team;
  double Sc=-FVector::Dist2D(Here,Objectives[I].Position)/10000.*.15;
  if(bOurs&&Known>Own)Sc+=5.;                       // an own point about to be lost
  if(I==VehiclePoint)Sc+=4.;                        // enemy armour
  if(!bOurs)Sc+=3.+(Known<=4?1.5:Known>=10?-2.:0.); // a point to take, better a thinly held one; not alone into a crowd
  return Sc;
 });
}
