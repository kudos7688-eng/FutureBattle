// Armoured exoskeleton soldier (squad type 8, 2026-11-18): one armoured, shielded, fast soldier with two weapons.
// The rules shared with other squad types live with them: values in BattleSquadType (BattleState.cpp), the shield in
// UBattleState::ApplyDamage / AdvanceShields, the armour in UBattleLeaderRuntime::ArmorScale, no suppression while shielded in
// AddSuppression, the score count in UBattleMatch::AreaScore, going home for repair in UBattleSquadTactics::Evaluate.
// This file holds the second weapon (the shoulder anti-tank launcher) and the look.
// AI cognition: the launcher fires only at what the soldier sees itself (its own visible contacts).
#include "BattleLeaderRuntime.h"
#include "BattleTactics.h"
#include "BattleState.h"
#include "BattleMatch.h"
#include "BattlePerception.h"
#include "BattleRanges.h"
#include "BattleStance.h"
#include "BattleSuppression.h"
#include "BattleUnit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Misc/CommandLine.h"
#include "UObject/StrongObjectPtr.h"
#include "BattleFieldLayout.h"
#include "DrawDebugHelpers.h"
#include "FutureBattle.h"
#include "BattleBodyAnim.h"

// A bone of a clip's first frame in component space (the clips are authored on the suit's own proportions).
static FTransform ExoClipBone(const UAnimSequence* Clip,FName Bone)
{
 const FReferenceSkeleton& Ref=Clip->GetSkeleton()->GetReferenceSkeleton();FTransform T=FTransform::Identity;
 const FAnimExtractContext At(0.);
 for(int32 I=Ref.FindBoneIndex(Bone);I!=INDEX_NONE;I=Ref.GetParentIndex(I))
 {FTransform Local;Clip->GetBoneTransform(Local,FSkeletonPoseBoneIndex(I),At,WITH_EDITOR!=0);T=T*Local;}
 return T;
}

// Older installed suit packages contain only the armour shells. Cover the open joints
// with one instanced inner frame, updated when (and only when) the suit evaluates its bones.
static void FitExoInnerFrame(ABattleUnit* Unit)
{
 auto* Body=Unit->GetMesh();
 if(Body->GetMaterialIndex(TEXT("AC01_InnerFrame"))!=INDEX_NONE)return;
 struct FLink { FName Start,End; float Radius; };
 static const FLink Links[]={
  {TEXT("pelvis"),TEXT("spine_03"),15.f},{TEXT("spine_03"),TEXT("spine_05"),16.f},{TEXT("spine_05"),TEXT("neck_01"),10.f},{TEXT("neck_01"),TEXT("head"),7.f},
  {TEXT("clavicle_l"),TEXT("upperarm_l"),9.f},{TEXT("clavicle_r"),TEXT("upperarm_r"),9.f},
  {TEXT("upperarm_l"),TEXT("lowerarm_l"),8.f},{TEXT("upperarm_r"),TEXT("lowerarm_r"),8.f},
  {TEXT("lowerarm_l"),TEXT("hand_l"),6.f},{TEXT("lowerarm_r"),TEXT("hand_r"),6.f},
  {TEXT("pelvis"),TEXT("thigh_l"),9.f},{TEXT("pelvis"),TEXT("thigh_r"),9.f},
  {TEXT("thigh_l"),TEXT("calf_l"),10.f},{TEXT("thigh_r"),TEXT("calf_r"),10.f},
  {TEXT("calf_l"),TEXT("foot_l"),7.5f},{TEXT("calf_r"),TEXT("foot_r"),7.5f}
 };
 auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 if(!Sphere||!Base)return;
 auto* Pieces=NewObject<UInstancedStaticMeshComponent>(Unit);
 Pieces->SetupAttachment(Body);Pieces->SetStaticMesh(Sphere);
 Pieces->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pieces->SetCanEverAffectNavigation(false);
 auto* Graphite=UMaterialInstanceDynamic::Create(Base,Pieces);
 Graphite->SetVectorParameterValue(TEXT("Color"),FLinearColor(.075f,.08f,.09f));
 Pieces->SetMaterial(0,Graphite);Pieces->RegisterComponent();Unit->AddInstanceComponent(Pieces);
 auto Refresh=[Body,Pieces]()
 {
  Pieces->SetOwnerNoSee(Body->bOwnerNoSee);
  Pieces->SetVisibility(Body->IsVisible());
  if(!Body->IsVisible())return;
  TArray<FTransform,TInlineAllocator<UE_ARRAY_COUNT(Links)>> Shapes;
  for(const FLink& Link:Links)
  {
   const FVector A=Body->GetSocketTransform(Link.Start,RTS_Component).GetLocation();
   const FVector B=Body->GetSocketTransform(Link.End,RTS_Component).GetLocation();
   const FVector Axis=B-A;
   const FVector Scale(2*Link.Radius/100.,1.8*Link.Radius/100.,(Axis.Size()+1.65*Link.Radius)/100.);
   Shapes.Emplace(FRotationMatrix::MakeFromZ(Axis.GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector)).ToQuat(),(A+B)*.5,Scale);
  }
  if(Pieces->GetInstanceCount()==0)
   for(const auto& Shape:Shapes)Pieces->AddInstance(Shape);
  else Pieces->BatchUpdateInstancesTransforms(0,MakeArrayView(static_cast<const FTransform*>(Shapes.GetData()),Shapes.Num()),false,true,true);
 };
 Refresh();
 Body->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateWeakLambda(Pieces,Refresh));
}

// Load each carry pose with its matching right-hand mount; the other prop follows the upper spine.
struct FExoCarryPose
{
 TStrongObjectPtr<UAnimSequence> Idle,Move,CrouchIdle,CrouchMove,Fire,Reload;
 // Fire/Reload: the recoil of each shot and the reload of the weapon in the right hand (ABattleUnit::SetLookActions), if any.
 void Load(const TCHAR* Family,const TCHAR* FireName=nullptr,const TCHAR* ReloadName=nullptr)
 {
  auto Clip=[Family](const TCHAR* Name){
   const FString Asset=FString::Printf(TEXT("A_AC01T2_%s_%s"),Family,Name);
   return LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Art/AC01T2/Anims/%s.%s"),*Asset,*Asset),nullptr,LOAD_NoWarn);
  };
  // Moving on foot is a glide (2026-11-20, user request): the stance with the chest tipped forward, feet still
  // (Setup/Import-AC01Glide.ps1). Without the glide clip the jog is shown.
  Idle.Reset(Clip(TEXT("Idle")));Move.Reset(Clip(TEXT("Glide")));if(!Move.IsValid())Move.Reset(Clip(TEXT("Jog_Fwd")));
  CrouchIdle.Reset(Clip(TEXT("Crouch_Idle")));CrouchMove.Reset(Clip(TEXT("Crouch_Walk_Fwd")));
  if(FireName)Fire.Reset(Clip(FireName));
  if(ReloadName)Reload.Reset(Clip(ReloadName));
 }
};

namespace
{
struct FExoAssets{TStrongObjectPtr<USkeletalMesh> Suit;FExoCarryPose Primary,Secondary;TStrongObjectPtr<UAnimSequence> LauncherFire,LauncherReload;TStrongObjectPtr<UStaticMesh> Rifle,Launcher;bool bLoaded=false,bOk=false;};
FExoAssets& ExoAssets()
{
 static FExoAssets X;
 if(!X.bLoaded)
 {
  X.bLoaded=true;
  // Body 3 (default): seamless under-suit with the armour plates over it (Setup/Import-AC01T3.ps1); 2: the v2 shell with joint sleeves.
  int32 BodyVersion=3;FParse::Value(FCommandLine::Get(),TEXT("BattleExoBody="),BodyVersion);
  if(BodyVersion==3)X.Suit.Reset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Art/AC01T2/SK_AC01T3.SK_AC01T3"),nullptr,LOAD_NoWarn));
  if(!X.Suit.IsValid())X.Suit.Reset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Art/AC01T2/SK_AC01T2.SK_AC01T2"),nullptr,LOAD_NoWarn));
  // Action clips (Setup/Import-AC01Actions.ps1; Art/AC01_Tripo_v2/Scripts/17_exo_action_clips.py): the rifle's recoil and its reload
  // with the legs held still; the launcher's recoil and reload, played after a launcher shot (PlayExoLauncherAction). The launcher
  // in hand has no clip for the rifle's shots. Missing clips just leave the pose as it is.
  X.Primary.Load(TEXT("OneHand"),TEXT("Fire"),TEXT("ReloadStill"));X.Secondary.Load(TEXT("Dual"));
  auto Clip=[](const TCHAR* Asset){return LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Art/AC01T2/Anims/%s.%s"),Asset,Asset),nullptr,LOAD_NoWarn);};
  X.LauncherFire.Reset(Clip(TEXT("A_AC01T2_Dual_Fire")));X.LauncherReload.Reset(Clip(TEXT("A_AC01T2_Dual_Reload")));
  X.Rifle.Reset(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/AC01T2/Weapons/SM_AC01_Rifle.SM_AC01_Rifle"),nullptr,LOAD_NoWarn));
  X.Launcher.Reset(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/AC01T2/Weapons/SM_AC01_Launcher.SM_AC01_Launcher"),nullptr,LOAD_NoWarn));
  X.bOk=X.Suit.IsValid()&&X.Primary.Idle.IsValid()&&X.Primary.Move.IsValid()&&X.Secondary.Idle.IsValid()&&X.Secondary.Move.IsValid()&&X.Rifle.IsValid()&&X.Launcher.IsValid();
  UE_LOG(LogFutureBattle,Display,TEXT("EXO_SUIT assets=%d body=%s move=%s,%s actions=rifle%d%d,launcher%d%d"),X.bOk,X.Suit.IsValid()?*X.Suit->GetName():TEXT("none"),X.Primary.Move.IsValid()?*X.Primary.Move->GetName():TEXT("none"),X.Secondary.Move.IsValid()?*X.Secondary.Move->GetName():TEXT("none"),
   X.Primary.Fire.IsValid(),X.Primary.Reload.IsValid(),X.LauncherFire.IsValid(),X.LauncherReload.IsValid());
 }
 return X;
}
}

bool UBattleLeaderRuntime::ApplyExoSuit(int32 ID)
{
 static const bool bOff=FParse::Param(FCommandLine::Get(),TEXT("BattleExoPlaceholder"));
 if(bOff)return false;
 const auto& X=ExoAssets();
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();ABattleUnit* A=Actor(ID);const auto* U=Store->Find(ID);
 if(!X.bOk||!A||!U)return false;
 auto* Body=A->GetMesh();
 auto* Existing=ExoLooks.Find(ID);
 const bool Launcher=U->Action==EBattleAction::DirectControl?U->bLauncherSelected:(Existing && GetWorld()->GetTimeSeconds()<Existing->AILauncherUntil);
 auto* Back=Existing?Existing->Stowed.Get():nullptr;
 if(!Back)
 {
  Body->SetSkeletalMesh(X.Suit.Get());
  FitExoInnerFrame(A);
  if(auto* Paint=Body->CreateDynamicMaterialInstance(0))Paint->SetVectorParameterValue(TEXT("VisorColor"),U->Team==0?FLinearColor(.25f,2.2f,4.f):FLinearColor(4.f,.35f,.25f));
  Back=NewObject<UStaticMeshComponent>(A);Back->SetupAttachment(Body,TEXT("spine_05"));
  Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);Back->SetCanEverAffectNavigation(false);
  Back->RegisterComponent();A->AddInstanceComponent(Back);
 }
 const auto& Pose=Launcher?X.Secondary:X.Primary;
 const FTransform Hand=ExoClipBone(Pose.Idle.Get(),TEXT("hand_r"));
 const FVector Palm=FMath::Lerp(Hand.GetLocation(),ExoClipBone(Pose.Idle.Get(),TEXT("middle_01_r")).GetLocation(),.86);
 const FVector Barrel=FRotator(Launcher?-6.f:0.f,90.f,0.f).Vector();
 const FTransform Grip(FRotationMatrix::MakeFromYZ(-Barrel,FVector::UpVector).ToQuat(),Palm);
 A->SetLook(Pose.Idle.Get(),Pose.Move.Get(),Pose.CrouchIdle.Get(),Pose.CrouchMove.Get(),Launcher?X.Launcher.Get():X.Rifle.Get(),TEXT("hand_r"),Grip.GetRelativeTransform(Hand));
 A->SetLookActions(Pose.Fire.Get(),Pose.Reload.Get());
 const FTransform Spine=ExoClipBone(Pose.Idle.Get(),TEXT("spine_05"));
 // The carried weapon lies against the back plate (component space: +X is the suit's left, -Y its back), not floating behind it.
 // Launcher: nearly upright along the back, muzzle up past the left shoulder, 15 degrees from upright and 10 cm to the left, leaning
 // forward with the back, so its rear end stays behind the right hip seen from the front. Rifle: stock up at the left shoulder, muzzle
 // down to the right hip. Prone with the shared prone clips: against the back plate of the lying body (its spine taken from the prone
 // idle clip), along the body, shifted toward the feet so the launcher does not reach far past the head. Prone without them (the
 // squashed placeholder): flat across the back.
 // The models' origin is the grip; their middle is 10 cm behind it (launcher) or 19 cm ahead of it (rifle).
 const UAnimSequence* ProneClip=A->LookHasProneClip()?BattleBodyAnim::Get(BattleBodyAnim::EClip::ProneIdle):nullptr;
 auto Stow=[&](bool bProne)
 {
  const float Middle=Launcher?19.f:-10.f;
  if(bProne&&ProneClip)
  {
   const FTransform SpineP=ExoClipBone(ProneClip,TEXT("spine_05"));
   const FVector Head=(SpineP.GetLocation()-ExoClipBone(ProneClip,TEXT("pelvis")).GetLocation()).GetSafeNormal2D();
   const FVector Up(0,0,1),Left=FVector::CrossProduct(Head,Up);
   const FVector Axis=Launcher?-Head:Head;
   const FVector At=SpineP.GetLocation()+Left*(Launcher?0.f:8.f)+Up*12.f-Head*22.f-Axis*Middle;
   FTransform Sling=FTransform(FRotationMatrix::MakeFromYZ(-Axis,Up).ToQuat(),At).GetRelativeTransform(SpineP);
   Sling.SetScale3D(FVector(1.2f));
   return Sling;
  }
  const FVector Axis=(bProne?FVector(1,0,0):Launcher?FVector(-.55,0,-.83):FVector(.26,.10,.96)).GetSafeNormal();
  const FVector At=Spine.GetLocation()+FVector(bProne||Launcher?0.f:10.f,Launcher?-24.f:-22.f,bProne?0.f:Launcher?-15.f:4.f)-Axis*Middle;
  FTransform Sling=FTransform(FRotationMatrix::MakeFromYZ(-Axis,FVector(0,-1,0)).ToQuat(),At).GetRelativeTransform(Spine);
  Sling.SetScale3D(FVector(1.2f));  // absolute scale: the prone squash of the body does not stretch the weapon
  return Sling;
 };
 Back->SetUsingAbsoluteScale(true);Back->SetStaticMesh(Launcher?X.Rifle.Get():X.Launcher.Get());
 const FTransform Upright=Stow(false),Flat=Stow(true);
 if(Existing)
 {
  Existing->bShowingLauncher=Launcher;Existing->StowUpright=Upright;Existing->StowProne=Flat;
  Back->SetRelativeTransform(Existing->bStowProne?Flat:Upright);return true;
 }
 Back->SetRelativeTransform(Upright);
 // The rig only marks the look as applied here (no plates hang on it); the shield bubble is the same as the placeholder's.
 auto* Rig=NewObject<USceneComponent>(A);Rig->SetupAttachment(Body);Rig->RegisterComponent();A->AddInstanceComponent(Rig);
 FExoLook Look;Look.Rig=Rig;Look.bSuit=true;Look.Stowed=Back;Look.bShowingLauncher=Launcher;Look.StowUpright=Upright;Look.StowProne=Flat;
 auto* Clear=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
 Look.BubblePaint=Clear?UMaterialInstanceDynamic::Create(Clear,A):nullptr;
 auto* Bubble=NewObject<UStaticMeshComponent>(A);Bubble->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
 Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bubble->SetCanEverAffectNavigation(false);Bubble->SetupAttachment(A->GetCapsuleComponent());
 Bubble->SetRelativeLocation(FVector(0,0,18));Bubble->SetRelativeScale3D(FVector(1.7f,1.7f,2.5f));if(Look.BubblePaint.IsValid())Bubble->SetMaterial(0,Look.BubblePaint.Get());
 Bubble->SetCastShadow(false);Bubble->RegisterComponent();A->AddInstanceComponent(Bubble);Look.Bubble=Bubble;
 ExoLooks.Add(ID,Look);UpdateExoLooks();
 return true;
}

void UBattleLeaderRuntime::ApplySquadLook(int32 SquadID)
{
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* S=Store->FindSquad(SquadID);
 if(!S||S->SquadType!=BattleSquadType::Exo)return;
 // Placeholder look from engine basic shapes (a Fab model can replace it later): the soldier body drawn 1.2x, armour plates, a
 // backpack and the launcher tube on a rig at the chest, round shoulder, head and knee pieces on the bones, and a shield bubble.
 ABattleUnit* A=Actor(S->LeaderUnitID);const auto* U=Store->Find(S->LeaderUnitID);if(!A||!U||!A->GetMesh())return;
 if(const auto* Old=ExoLooks.Find(S->LeaderUnitID);Old&&Old->Rig.IsValid())return;
 auto* Body=A->GetMesh();Body->SetRelativeScale3D(FVector(1.2f));
 if(ApplyExoSuit(S->LeaderUnitID))return;
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
  if(L.bSuit)
  {
   const bool ShowLauncher=U->Action==EBattleAction::DirectControl?U->bLauncherSelected:GetWorld()->GetTimeSeconds()<L.AILauncherUntil;
   if(ShowLauncher!=L.bShowingLauncher)ApplyExoSuit(It.Key());
   if(auto* Back=L.Stowed.Get()){Back->SetOwnerNoSee(A->GetMesh()->bOwnerNoSee);Back->SetVisibility(A->GetMesh()->IsVisible());}
   // A stance change resets the body's scale (SetStance); the suit stays 1.2x, crouches by its own clip and lies down by the shared prone
   // clips (squashed only when those are missing). With the clips the carried weapon stays on the back plate and follows the spine.
   const float Squash=(A->VisualStance==BattleStance::Crouch&&BattleStance::AnimEnabled())||(A->VisualStance==BattleStance::Prone&&A->LookHasProneClip())?1.f:BattleStance::VisualScale(A->VisualStance);
   const FVector Want(1.2f,1.2f,1.2f*Squash);
   if(A->GetMesh()&&!A->GetMesh()->GetRelativeScale3D().Equals(Want,.001f))A->GetMesh()->SetRelativeScale3D(Want);
   // Prone: the carried weapon lies flat instead of standing up from the squashed body.
   const bool bLying=Squash<.99f||(A->VisualStance==BattleStance::Prone&&A->LookHasProneClip());
   if(auto* Back=L.Stowed.Get();Back&&bLying!=L.bStowProne){L.bStowProne=bLying;Back->SetRelativeTransform(L.bStowProne?L.StowProne:L.StowUpright);}
  }
  else L.Rig->SetWorldRotation(FRotator(0,A->GetActorRotation().Yaw,0));
  // The placeholder stances squash the body; the rig (absolute scale) follows its height so the plates do not tower over a prone soldier.
  if(!L.bSuit&&A->GetMesh())L.Rig->SetWorldScale3D(FVector(1.f,1.f,FMath::Clamp(float(A->GetMesh()->GetRelativeScale3D().Z)/1.2f,.35f,1.f)));
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
   // Permission restricts the aim target, not the observed neighbours used to value its splash.
   if(BattleTactics::WeaponOrderGateEnabled()&&!WeaponOrderAllows(ID,K.Key,K.Value))continue;
   const bool Vehicle=BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(Contacts->FindChecked(K.Key).ObservedSquadType):IsVehicle(K.Key);int32 Around=0;
   if(!Vehicle){if(U->Launcher<=FMath::Max(0,T->ExoLauncherKeep))continue;for(const auto& O:Seen)Around+=!(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(Contacts->FindChecked(O.Key).ObservedSquadType):IsVehicle(O.Key))&&FVector::DistSquared2D(O.Value,K.Value)<=Splash2;if(Around<FMath::Max(1,T->ExoLauncherCluster))continue;}
   const double Score=(Vehicle?1000.:double(Around))-FVector::Dist2D(Pos,K.Value)/1.e6;
   if(Score>BestScore||(Score==BestScore&&K.Key<Best)){BestScore=Score;Best=K.Key;BestAt=K.Value;}
  }
  if(Best==INDEX_NONE)continue;
  // Line of fire from the shoulder: clear, or ending on the target or close enough for the explosion.
  const FVector Muzzle=Pos+FVector(0,0,BattleStance::EyeOffset(StanceOf(ID))),Aim=BestAt+FVector(0,0,TargetAimOffset(Best));
  auto Q=Sight(ID).Query;FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,Aim,ECC_Visibility,Q);
  if(Hit.bBlockingHit&&HitIDs.FindRef(Hit.GetComponent())!=Best&&FVector::DistSquared(Hit.ImpactPoint,Aim)>Splash2)continue;
  FireExoLauncher(ID,Aim,Now);
 }
}

float UBattleLeaderRuntime::PlayExoLauncherAction(int32 ID,double Now)
{
 // The launcher shows in hand (AILauncherUntil; the player's selection keeps it anyway) through its recoil and reload clips, then
 // goes back on the back. Display only: the reload of the rules (LauncherWait) is separate and longer.
 auto* Look=ExoLooks.Find(ID);
 if(!Look)return 0.f;
 Look->AILauncherUntil=FMath::Max(Look->AILauncherUntil,Now+.5);
 if(!Look->bSuit)return 0.f;  // the placeholder plates have no clips
 ApplyExoSuit(ID);  // the launcher in hand now, so its clips play on the launcher's arrangement
 ABattleUnit* A=Actor(ID);const auto& X=ExoAssets();
 const float Seconds=A&&A->HasLook()&&X.LauncherFire.IsValid()?A->PlayLookSequence(X.LauncherFire.Get(),X.LauncherReload.Get(),Now):0.f;
 if(auto* After=ExoLooks.Find(ID))After->AILauncherUntil=FMath::Max(After->AILauncherUntil,Now+Seconds);
 return Seconds;
}

bool UBattleLeaderRuntime::FireExoLauncher(int32 ID,const FVector& Aim,double Now)
{
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();auto* M=GetWorld()->GetSubsystem<UBattleMatch>();const auto* U=Store->Find(ID);
 if(!U||!U->IsAlive()||!Bodies.Contains(ID))return false;
 const int32 Sequence=U->Launcher;
 if(!Store->UseLauncher(ID))return false;
 PlayExoLauncherAction(ID,Now);
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
  if(Store->ApplyDamageFrom(Victim,Team,T->ATDamage,Start,ID,EBattleKillKind::Launcher))
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
 for(int32 I=0;I<V.Enemies.Num()&&I<V.EnemyKeys.Num();++I)if(BattleTactics::ObservedArmour()?BattleSquadType::IsVehicle(V.ObservedType(V.EnemyKeys[I])):V.EnemyKeys[I]>0&&R->IsVehicle(V.EnemyKeys[I]))
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
