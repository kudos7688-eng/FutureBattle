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
	double NextLabelCheck = 0;
	TSet<int32> ClearLabels;
	TWeakObjectPtr<APawn> LabelViewPawn;
};
