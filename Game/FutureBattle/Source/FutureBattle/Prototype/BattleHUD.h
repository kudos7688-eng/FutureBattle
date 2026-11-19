#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BattleHUD.generated.h"

UCLASS()
class ABattleHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
 void DrawSquadList();
 // Tactical minimap: own squads and only what the player's side has observed (never unseen enemies).
 void DrawMinimap();
 // Notification log (newest first; click a row to move the camera there). Observed events only.
 void DrawNoticeLog();
 // Ground marks for the selected squads: line to the order destination, destination mark, defence radius, arrival slots.
 void DrawOrderMarks();
 // The predicted way of the selected squads and the way being drawn (BattlePlayerPath.cpp).
 void DrawOrderWays();
 // Hit direction (2026-11-19, display only): while the player controls a soldier, a red arc around the crosshair points to
 // where each recent hit came from (1.5 s fade; a faint ring when the source is unknown). -BattleHitDirection=0 hides it.
 void DrawHitDirection(class ABattleController* PC);
 // Off-screen markers (2026-11-20, display only): in direct control the objectives and the soldier's own
 // order goal, in the tactical view the selected squads, that are off the screen get an arrow on the screen edge. -BattleOffscreenMarkers=0 hides them.
 void DrawOffscreenMarkers(class ABattleController* PC);
 // Kill log (2026-11-20, display only): the last deaths involving the player's team with a known killer, top right
 // (left of the squad list in the tactical view), 8 s each. Enemies are named only as "敵". -BattleKillLog=0 hides it.
 void DrawKillLog(class ABattleController* PC);
	double NextLabelCheck = 0;
	TSet<int32> ClearLabels;
	TWeakObjectPtr<APawn> LabelViewPawn;
};
