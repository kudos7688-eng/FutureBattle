#pragma once
#include "CoreMinimal.h"
#include "NavMesh/RecastNavMesh.h"
#include "BattleVehicleNavMesh.generated.h"

// Navigation data of the "Vehicle" agent (DefaultEngine.ini SupportedAgents, 2026-11-17). A class of its own so the agents are told
// apart by PreferredNavData: infantry prefers ARecastNavMesh (the Default agent), tanks prefer this class. Without it the engine's
// best-fit rule sent infantry (capsule 36 cm, wider than the Default agent's 34 cm) to the vehicle navmesh.
UCLASS()
class ABattleVehicleNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()
};
