#pragma once
#include "CoreMinimal.h"

// War score trial for the Area battle (-BattleAreaWarScore=1, default OFF). Each team earns PerPoint for every central point it
// owns each PointSeconds, and PerLoss for every enemy soldier lost (including followers stopped by their leader's death).
// At the time limit the unspent resources are added (x ResourceValue) and the higher total wins; living soldiers no longer count,
// so resources kept as resources are worth more than resources spent. Base destruction stays an immediate win.
// A team sees its own score exactly. The opponent's score is shown only as an estimate: the observer's own losses (exact) plus
// points credited from the last OBSERVED ownership; the opponent's resources stay unknown.
// Target > 0 also ends the match when a team's points+losses reach it (default 0 = time limit and base destruction only).
// Overrides: -BattleAreaWarPointSeconds= / -BattleAreaWarResourceValue= / -BattleAreaWarTarget=.
namespace BattleWarScore
{
 struct FTuning { bool bEnabled=false; float PointSeconds=5.f; int32 PerPoint=1, PerLoss=1, ResourceValue=1, Target=0; };
 const FTuning& Tuning();
 // Smoke tests only: toggles the trial inside one process.
 FTuning& MutableTuning();
}
