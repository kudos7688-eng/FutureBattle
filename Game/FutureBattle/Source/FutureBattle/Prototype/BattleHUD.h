#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BattleSectorView.h"
#include "CanvasItem.h"
#include "BattleHUD.generated.h"

UCLASS()
class ABattleHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
 void DrawGamepadMenu(class ABattleController* PC);
 void DrawSquadList();
 // Tactical minimap: own squads and only what the player's side has observed (never unseen enemies).
 void DrawMinimap();
 // Notification log (newest first; click a row to move the camera there). Observed events only.
 void DrawNoticeLog();
 // Ground marks for the selected squads: line to the order destination, destination mark, defence radius, arrival slots.
 void DrawOrderMarks();
 // Range rings (2026-11-21, display only, tactical view): around the leader of each selected squad, a ring at the distance its
 // weapons open fire (BattleRanges::Engage; anti-tank, tank gun and artillery min/max get their own ring), labelled in metres.
 // Seen enemy squads get rings for their identified type (the one under the pointer, or all with L), observed information only.
 // -BattleRangeRings=0 hides them (also on the minimap).
 void DrawRangeRings();
 // The same rings on the tactical minimap (no labels; enemies only with L).
 void DrawMinimapRangeRings(class ABattleController* PC, const struct FBattleMinimapLayout& Map);
 // The predicted way of the selected squads and the way being drawn (BattlePlayerPath.cpp).
 void DrawOrderWays();
 // The player's orders to single soldiers and the soldier picked with Ctrl+click (BattleSoldierOrder.cpp).
 void DrawSoldierOrders();
 // Hit direction (2026-11-19, display only): while the player controls a soldier, a red arc around the crosshair points to
 // where each recent hit came from (1.5 s fade; a faint ring when the source is unknown). -BattleHitDirection=0 hides it.
 void DrawHitDirection(class ABattleController* PC);
 // Off-screen markers (2026-11-20, display only): in direct control the objectives and the soldier's own
 // order goal, in the tactical view the selected squads, that are off the screen get an arrow on the screen edge. -BattleOffscreenMarkers=0 hides them.
 void DrawOffscreenMarkers(class ABattleController* PC);
 // Kill log (2026-11-20, display only): the last deaths involving the player's team with a known killer, top right
 // (left of the squad list in the tactical view), 8 s each. Enemies are named only as "敵". -BattleKillLog=0 hides it.
 void DrawKillLog(class ABattleController* PC);
 // Sector grid and fog of war (2026-11-20, display only, tactical view; BattleSectorView.h). On the field: the fog of the
 // player's side (dark = not in sight, darker = never seen), the 50m sector grid with names and a tint by status (maps with
 // sectors). M toggles the grid, N the fog. DrawSectorPanel: the status of the sector under the pointer (field or minimap).
 void DrawFieldOverlay(class ABattleController* PC);
 void DrawSectorPanel(class ABattleController* PC);
 // Sector under the pointer (field or minimap); false when the map has no sectors or the pointer is over a panel.
 bool HoveredSector(class ABattleController* PC, FIntPoint& Out) const;
 void DrawMinimapOverlay(class ABattleController* PC, const struct FBattleMinimapLayout& Map);
 FBattleFogGrid Fog;
 uint64 FogRestoreGeneration=0;
 TArray<FBattleSectorInfo> Sectors;
 // Contact pointers exist only during DrawHUD; every frame rebuilds the snapshot in source order.
 TMap<int32,const FBattleContact*> FrameContacts;
 TSet<int32> FrameVisibleContacts;
 TArray<FVector2D> OverlayScreen;
 TBitArray<> OverlayOn;
 TArray<float> OverlayDark;
 TArray<uint8> OverlayCount;
 TArray<FCanvasUVTri> OverlayTris;
	double NextLabelCheck = 0;
	TSet<int32> ClearLabels;
	TWeakObjectPtr<APawn> LabelViewPawn;
};
