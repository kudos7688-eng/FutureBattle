#include "BattleHUD.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ABattleHUD::DrawHUD()
{
	Super::DrawHUD();
	ABattleController* PC = Cast<ABattleController>(PlayerOwner);
	ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Canvas || !PC || !Mode) return;
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const FLinearColor Ink(.025f, .04f, .065f, .92f), Mint(.28f, .95f, .8f), Muted(.62f, .7f, .77f), White(.94f, .97f, 1.f);
	auto Label = [&](const FString& Text, float X, float Y, FLinearColor Color, float Scale = 1.f)
	{ DrawText(Text, Color, X, Y, GEngine->GetSmallFont(), Scale); };
	DrawRect(Ink, 16, 16, W - 32, 66);
	DrawRect(Mint, 16, 16, 4, 66);
	Label(TEXT("FUTURE BATTLE"), 34, 25, White, 1.55f);
	Label(TEXT("PROTOTYPE 01  /  20-UNIT FIELD TEST"), 35, 53, Muted, .9f);
	int32 Allies = 0, Enemies = 0, Spotted = 0;
	for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive())
	{
		if (Unit->GetState()->Team == 0) ++Allies;
		else { ++Enemies; if (Unit->bRevealed) ++Spotted; }
	}
	Label(PC->IsTactical() ? TEXT("TACTICAL COMMAND") : TEXT("DIRECT CONTROL / FPS"), W * .4f, 30, Mint, 1.2f);
	Label(FString::Printf(TEXT("ALLY %02d   /   ENEMY %02d   /   SPOTTED %02d"), Allies, Enemies, Spotted), W * .4f, 54, White, .9f);
	Label(Mode->bCombatPaused ? TEXT("AI PAUSED [P]") : TEXT("AI ACTIVE [P]"), W - 165, 35, Mode->bCombatPaused ? FLinearColor::Yellow : Muted);

	for (ABattleUnit* Unit : Mode->Units)
	{
		if (!Unit || !Unit->IsAlive() || !Unit->bRevealed || Unit == PC->GetPawn()) continue;
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Unit->GetActorLocation() + FVector(0, 0, 120), Screen)) continue;
		if (Screen.X < 20 || Screen.X > W - 20 || Screen.Y < 90 || Screen.Y > H - 130) continue;
		const FBattleUnitState* State = Unit->GetState();
		const FLinearColor Color = State->Team == 0 ? Mint : FLinearColor(1, .35f, .25f);
		DrawRect(Ink, Screen.X - 25, Screen.Y, 50, 5);
		DrawRect(Color, Screen.X - 25, Screen.Y, 50 * State->HP / FMath::Max(1.f, GetDefault<UBattleTuning>()->Health), 5);
		Label(FString::Printf(TEXT("%02d"), State->UnitID), Screen.X - 8, Screen.Y - 19, Color, .85f);
		if (PC->IsSelected(Unit))
		{
			DrawLine(Screen.X - 33, Screen.Y - 22, Screen.X - 33, Screen.Y + 12, Mint, 2);
			DrawLine(Screen.X + 33, Screen.Y - 22, Screen.X + 33, Screen.Y + 12, Mint, 2);
			if (State->Order == EBattleOrder::Move && PC->IsTactical())
			{
				FVector2D Goal;
				if (PC->ProjectWorldLocationToScreen(State->Destination, Goal)) DrawLine(Screen.X, Screen.Y + 10, Goal.X, Goal.Y, FLinearColor(.25f, .7f, .6f, .6f), 1);
			}
		}
	}
	if (PC->IsDragging())
	{
		FVector2D End; PC->GetMousePosition(End.X, End.Y);
		FVector2D Start = PC->DragOrigin;
		DrawRect(FLinearColor(.1f, .8f, .65f, .12f), FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y), FMath::Abs(Start.X - End.X), FMath::Abs(Start.Y - End.Y));
		DrawLine(Start.X, Start.Y, End.X, Start.Y, Mint); DrawLine(End.X, Start.Y, End.X, End.Y, Mint);
		DrawLine(End.X, End.Y, Start.X, End.Y, Mint); DrawLine(Start.X, End.Y, Start.X, Start.Y, Mint);
	}
	DrawRect(Ink, 16, H - 128, W - 32, 112);
	const ABattleUnit* Focus = PC->IsTactical() ? (PC->GetSelection().IsEmpty() ? nullptr : PC->GetSelection()[0].Get()) : Cast<ABattleUnit>(PC->GetPawn());
	if (Focus && Focus->GetState())
	{
		const FBattleUnitState* S = Focus->GetState();
		Label(FString::Printf(TEXT("UNIT %02d   HP %.0f   AMMO %02d / %02d   ORDER %s%s"), S->UnitID, S->HP, S->Ammo, S->Reserve,
			S->Order == EBattleOrder::Move ? TEXT("MOVE") : TEXT("HOLD"), S->ReloadEndTime > 0 ? TEXT("   RELOADING...") : TEXT("")), 34, H - 112, White, 1.2f);
	}
	else Label(TEXT("SELECT A FRIENDLY UNIT TO BEGIN"), 34, H - 112, White, 1.2f);
	if (PC->IsTactical())
	{
		Label(TEXT("LMB / drag: select   Shift: add   RMB: move   Q: select all   H: hold   E / Tab: control unit"), 34, H - 80, Mint);
		Label(FString::Printf(TEXT("WASD: pan   Wheel: zoom   P: pause AI   F5: reset battle      SELECTED %02d   |   Enemies require allied line of sight"), PC->GetSelection().Num()), 34, H - 53, Muted, .9f);
	}
	else
	{
		Label(TEXT("WASD: move   Mouse: aim   LMB: fire   R: reload   Space: jump   Tab: tactical view"), 34, H - 80, Mint);
		Label(TEXT("P: pause AI   F5: reset battle   |   Unit ID, health, ammo and orders survive every control switch"), 34, H - 53, Muted, .9f);
		const FLinearColor CrossColor = PC->HitMarkerUntil > GetWorld()->GetTimeSeconds() ? FLinearColor::Yellow : White;
		DrawLine(W / 2 - 13, H / 2, W / 2 - 4, H / 2, CrossColor, 1.5f);
		DrawLine(W / 2 + 4, H / 2, W / 2 + 13, H / 2, CrossColor, 1.5f);
		DrawLine(W / 2, H / 2 - 13, W / 2, H / 2 - 4, CrossColor, 1.5f);
		DrawLine(W / 2, H / 2 + 4, W / 2, H / 2 + 13, CrossColor, 1.5f);
	}
	if (PC->NoticeUntil > GetWorld()->GetTimeSeconds()) Label(PC->Notice, 35, 96, Mint, 1.1f);
	if (GetWorld()->GetTimeSeconds() < Mode->CombatStartsAt)
		Label(FString::Printf(TEXT("DEPLOYMENT / AI ADVANCE IN %.0f"), FMath::CeilToFloat(Mode->CombatStartsAt - GetWorld()->GetTimeSeconds())), 35, 123, White);
	if (Mode->bBattleEnded)
	{
		DrawRect(Ink, W * .25f, H * .35f, W * .5f, 100);
		Label(Mode->Result, W * .25f + 25, H * .35f + 16, Mint, 1.8f);
		Label(TEXT("F5 TO REDEPLOY"), W * .25f + 25, H * .35f + 60, White);
	}
}
