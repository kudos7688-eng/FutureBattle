#pragma once
#include "CoreMinimal.h"
class USkeletalMesh;
// Trial -BattleSoldierBody=1 (2026-11-23, default 0, display only): infantry bodies - squad leaders (ABattleUnit), Mass followers
// and their far VAT copies - use RB01 (Art/RB01_Tripo in the main checkout, imported by Setup/Import-RB01.py to /Game/Art/RB01
// on the shared SK_Mannequin skeleton) instead of the bundled Quinn. Capsules, sockets, clips and the match itself are unchanged.
// The exo squad still replaces its leader's body with AC01 (ApplySquadLook). When the RB01 asset is missing the bodies stay Quinn.
namespace BattleSoldierBody
{
 int32 Version();           // 1 = RB01 is in use (requested and present), else 0
 USkeletalMesh* LoadMesh(); // body for the infantry: RB01 or Quinn
 FString VATRoot();         // folder of the far-display VAT bake for that body, with the trailing slash
}
