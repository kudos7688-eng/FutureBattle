#include "BattleSoldierBody.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "FutureBattle.h"

namespace
{
const TCHAR* QuinnPath=TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple");
const TCHAR* RB01Package=TEXT("/Game/Art/RB01/SK_RB01");
const TCHAR* RB01Path=TEXT("/Game/Art/RB01/SK_RB01.SK_RB01");
}

int32 BattleSoldierBody::Version()
{
 static const int32 V=[]
 {
  int32 Requested=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSoldierBody="),Requested);
  const bool bPresent=Requested==1&&FPackageName::DoesPackageExist(RB01Package);
  if(Requested!=0)UE_LOG(LogFutureBattle,Display,TEXT("SOLDIER_BODY requested=%d present=%d using=%s"),Requested,bPresent,bPresent?RB01Path:QuinnPath);
  return bPresent?1:0;
 }();
 return V;
}

USkeletalMesh* BattleSoldierBody::LoadMesh()
{
 if(Version()==1)if(auto* M=LoadObject<USkeletalMesh>(nullptr,RB01Path,nullptr,LOAD_NoWarn))return M;
 return LoadObject<USkeletalMesh>(nullptr,QuinnPath);
}

FString BattleSoldierBody::VATRoot()
{
 return Version()==1?TEXT("/Game/Prototype/DisplayLODTrial_RB01/"):TEXT("/Game/Prototype/DisplayLODTrial/");
}
