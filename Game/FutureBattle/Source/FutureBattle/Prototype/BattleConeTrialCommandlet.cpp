#include "BattleConeTrialCommandlet.h"
#include "BattleState.h"
#include "BattleTactics.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

UBattleConeTrialCommandlet::UBattleConeTrialCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
namespace {
// Closest distance between a forward ray and the capsule's vertical centre segment.
bool CapsuleHit(FVector D, double Distance)
{
 const FVector O(0,0,60), A(Distance,0,-54), B(Distance,0,54), V=B-A, W=O-A;
 const double UV=FVector::DotProduct(D,V), UW=FVector::DotProduct(D,W), VW=FVector::DotProduct(V,W), VV=V.SizeSquared();
 double S=FMath::Clamp((VW-UV*UW)/(VV-UV*UV),0.,1.);
 double T=FMath::Max(0.,UV*S-UW);
 if(T==0) S=FMath::Clamp(VW/VV,0.,1.);
 return (O+D*T-(A+V*S)).SizeSquared()<=36.*36.;
}
}
int32 UBattleConeTrialCommandlet::Main(const FString& Params)
{
 FString Out; int32 N=1000000;
 if(!FParse::Value(*Params,TEXT("Output="),Out)) return 1;
 FParse::Value(*Params,TEXT("Samples="),N); if(N<1000000) return 1;
 IFileManager::Get().MakeDirectory(*Out,true);
 FString Hist=TEXT("degrees,bin_upper_degrees,count,cdf\n"), Hit=TEXT("degrees,distance_m,samples,hits,probability\n");
 for(float Degrees : {1.2f,4.f,.35f}) {
  FBattleUnitState S; S.UnitID=1; S.Weapon=EBattleWeapon::Carbine; S.AimStableSince=-1; S.bMovingShot=false; S.SpreadSkillScale=Degrees/1.2f;
  if(FMath::Abs(BattleTactics::SpreadDegrees(S,0)-Degrees)>1.e-5) return 2;
  TArray<float> Angles; Angles.Reserve(N); int64 Bins[200]={}, Hits[7]={};
  const double Distances[]={500,1000,1500,2000,3000,4000,5000};
  for(int32 I=1;I<=N;++I) {
   S.ShotSequence=I;
   const FVector D=BattleTactics::ShotDirection(S,FVector::ForwardVector,0);
   const double Angle=FMath::RadiansToDegrees(FMath::Atan2(FMath::Sqrt(D.Y*D.Y+D.Z*D.Z),D.X));
   Angles.Add(float(Angle)); ++Bins[FMath::Clamp(int32(Angle/Degrees*200),0,199)];
   for(int32 J=0;J<7;++J) Hits[J]+=CapsuleHit(BattleTactics::ShotDirection(S,FVector(Distances[J],0,-25),0),Distances[J]);
  }
  const FString File=Out/FString::Printf(TEXT("angles-%.2f.f32"),Degrees);
  TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*File)); if(!Ar) return 3;
  Ar->Serialize(Angles.GetData(),int64(N)*sizeof(float)); if(!Ar->Close()) return 3;
  int64 Sum=0;
  for(int32 B=0;B<200;++B) { Sum+=Bins[B]; Hist+=FString::Printf(TEXT("%.2f,%.9f,%lld,%.9f\n"),Degrees,Degrees*(B+1)/200.,Bins[B],double(Sum)/N); }
  for(int32 J=0;J<7;++J) Hit+=FString::Printf(TEXT("%.2f,%.1f,%d,%lld,%.9f\n"),Degrees,Distances[J]/100,N,Hits[J],double(Hits[J])/N);
 }
 const bool OK=FFileHelper::SaveStringToFile(Hist,*(Out/TEXT("histogram.csv"))) && FFileHelper::SaveStringToFile(Hit,*(Out/TEXT("capsule.csv")));
 UE_LOG(LogTemp,Display,TEXT("CONE_TRIAL samples_per_angle=%d success=%d"),N,OK); return OK?0:1;
}
