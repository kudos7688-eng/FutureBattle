#pragma once
#include "CoreMinimal.h"

class UWorld;
class UStaticMesh;
class UStaticMeshComponent;
enum class EBattleWeapon : uint8;

// Display only. The world argument also works for follower components mounted through the unit CDO.
namespace BattleKitLook
{
 bool Active(UWorld* World);
 void Apply(UWorld* World);
 UStaticMesh* Weapon(UWorld* World, EBattleWeapon Kind);
 bool IsWeapon(UWorld* World, const UStaticMesh* Mesh);
 FTransform Grip(EBattleWeapon Kind, bool bFirstPerson);
 bool AnimateFirstPerson(UStaticMeshComponent* Model, double Kick, double Dip);
}
