#include "BattleHUD.h"
#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "BattleController.h"
#include "BattleUnit.h"
#include "BattleGameMode.h"
#include "BattlePerception.h"
#include "BattleTactics.h"
#include "BattleWarScore.h"
#include "BattleUIText.h"
#include "BattleStance.h"
#include "BattleCover.h"
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ABattleHUD::DrawHUD()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawHUD);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleHUD);
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
 Label(GetWorld()->GetSubsystem<UBattleMatch>()->bArea?FString(TEXT("エリア戦 ／ 観測情報のみ表示")):FString::Printf(TEXT("試作01 ／ %d体の野戦試験"), (Mode->bLeaderCommand?GetWorld()->GetSubsystem<UBattleState>()->GetUnits().Num():Mode->Units.Num())), 35, 53, Muted, .9f);
	int32 Allies = 0, Enemies = 0, Spotted = 0;
	for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive())
	{
		if (Unit->GetState()->Team == 0) ++Allies;
		else { ++Enemies; if (Unit->bRevealed) ++Spotted; }
	}
	if(Mode->bLeaderCommand)
 {
  Allies=Enemies=Spotted=0; TSet<int32> Known;
  for(const auto& P:GetWorld()->GetSubsystem<UBattleState>()->GetUnits()) if(P.Value.Team==0 && P.Value.IsAlive())
  { ++Allies; if(const auto* C=GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(P.Key)) for(const auto& Contact:*C) if(Contact.Value.bVisible) Known.Add(Contact.Key); }
  Enemies=Spotted=Known.Num();
 }
	const auto* Controlled=Cast<ABattleUnit>(PC->GetPawn());
	Label(PC->IsTactical() ? TEXT("作戦指揮") : Controlled && !Controlled->UsesThirdPersonView() ? TEXT("直接操作（一人称）") : TEXT("直接操作（三人称）"), W * .4f, 30, Mint, 1.2f);
	Label(FString::Printf(TEXT("味方 %02d ／ 敵 %02d ／ 発見中 %02d"), Allies, Enemies, Spotted), W * .4f, 54, White, .9f);
	Label(Mode->bCombatPaused ? TEXT("AI停止中 [P]") : TEXT("AI稼働中 [P]"), W - 165, 35, Mode->bCombatPaused ? FLinearColor::Yellow : Muted);
	if (const auto* SpeedPC = Cast<ABattleController>(GetOwningPlayerController()); SpeedPC && SpeedPC->IsTactical())
		Label(FString::Printf(TEXT("速度 x%d [.]"), SpeedPC->GetGameSpeed()), W - 165, 55, SpeedPC->GetGameSpeed() > 1 ? FLinearColor::Yellow : Muted);
	const auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled())
	{
		const TCHAR* Phase = Match->Phase == EBattleMatchPhase::Deployment ? TEXT("展開中 ／ Enterで開始") : Match->Phase == EBattleMatchPhase::Combat ? TEXT("戦闘中 ／ 増援あり") : TEXT("終了 ／ F5で再開");
		DrawRect(Ink, 16, 84, W - 32, 26);
  // Own income rate (user decision 2026-11-16): the next payment from the points the own team actually holds. The opponent's
  // rate and resource total are never shown.
  FString IncomeText;
  if(Match->bArea&&BattleTactics::AreaIncome().bEnabled)
  {
   const auto& I=BattleTactics::AreaIncome();const bool bUrban=BattleFieldLayout::IsUrban(GetWorld());
   int32 Gain=I.BaseFor(bUrban);for(const auto& P:Match->Objectives)Gain+=P.Owner==0?I.PerPointFor(bUrban):0;
   if(I.Ramp>0)Gain=FMath::RoundToInt(Gain*(1.f+I.Ramp*Match->Elapsed/60.f));
   IncomeText=FString::Printf(TEXT(" (+%d / %.0fs)"),Gain,I.Seconds);
  }
  if(Match->bArea)Label(FString::Printf(TEXT("資源 %d%s ／ 本拠地の耐久 %.0f ／ 敵本拠地の耐久（最後に見た値） %s%s"),Match->Credits[0],*IncomeText,Match->Area.BaseHP[0],Match->Area.KnownEnemyBaseHP[0]<0?TEXT("不明"):*FString::Printf(TEXT("%.0f"),Match->Area.KnownEnemyBaseHP[0]),
   BattleTactics::AreaHire().bEnabled?*FString::Printf(TEXT(" ／ [2] 小隊を購入 %d（得点 %+d、%d/25）"),BattleTactics::AreaHire().Cost,10-BattleTactics::AreaHire().Cost,Match->ActiveSquads(0)):TEXT("")),28,90,Mint,.9f);
  else if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch) Label(FString::Printf(TEXT("%s ／ 資源 %d ／ [1] %d [2] %d [3] %d ／ 10人"),Phase,Match->Credits[0],Match->CurrentSquadPrice(EBattleTraining::Recruit),Match->CurrentSquadPrice(EBattleTraining::Regular),Match->CurrentSquadPrice(EBattleTraining::Veteran)),28,90,Mint,.9f);
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(TEXT("500人 ／ 拠点3か所 ／ P：一時停止・再開"),28,90,Mint,.9f);
		else Label(FString::Printf(TEXT("%s   資源 %d   [1] 新兵 %d   [2] 正規兵 %d   [3] 熟練兵 %d ／ 5体"), Phase, Match->Credits[0], UBattleMatch::SquadPrice(EBattleTraining::Recruit), UBattleMatch::SquadPrice(EBattleTraining::Regular), UBattleMatch::SquadPrice(EBattleTraining::Veteran)), 28, 90, Mint, .9f);
		DrawRect(Ink, 16, 112, 560, 51);
		const float Limit=BattleFieldLayout::IsSightField(GetWorld())?600.f:180.f;
  if(Match->bArea)Label(FString::Printf(TEXT("残り %.0f秒 ／ 本拠地の破壊、または終了時の得点で勝敗"),FMath::Max(0.f,Match->MatchTimeLimit()-Match->Elapsed)),28,119,White);
  else Label(FString::Printf(TEXT("支配 %.2f : %.2f / 100    残り %.0f秒"), Match->Score[0], Match->Score[1], FMath::Max(0.f, Limit - Match->Elapsed)), 28, 119, White);
  // War score trial: own score exactly; the opponent only as an estimate from own losses and observed ownership (its resources unknown).
  if(Match->bArea&&BattleWarScore::Tuning().bEnabled)Label(FString::Printf(TEXT("戦果点 %s ／ 敵の推定 %d ＋ 資源は不明"),*Match->WarScoreBreakdown(0),Match->WarScoreEstimate(0)),28,141,Muted,.85f);
  else if(Match->bArea)
  {
   // Command UI top bar (2026-11-16): own final score as it stands, from own state and the points the own side observes as its own;
   // the opponent only from observed points (its living soldiers and resources are not shown).
   int32 Living=0;for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())Living+=U.Value.Team==0&&U.Value.IsAlive();
   int32 OwnPoints=0,EnemyPoints=0;for(int32 I=0;I<Match->Objectives.Num();++I){const int32 O=Match->ObservedOwner(0,I);OwnPoints+=O==0;EnemyPoints+=O==1;}
   DrawRect(Ink,576,112,260,51);
   Label(FString::Printf(TEXT("得点 %d ＝ 生存 %d ＋ 資源 %d ＋ 拠点 %d×20"),Living+Match->Credits[0]+OwnPoints*20,Living,Match->Credits[0],OwnPoints),28,141,Mint,.85f);
   Label(FString::Printf(TEXT("敵：拠点 %d×20（観測）"),EnemyPoints),588,119,Muted,.85f);
   Label(TEXT("＋ 生存・資源は不明"),588,141,Muted,.85f);
  }
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(FString::Printf(TEXT("占領8秒 ／ 毎秒1点 ／ 先に100点 ／ 制限 %.0f秒"),Limit),28,141,Muted,.85f);
		else Label(TEXT("占領8秒 ／ 5秒ごとに収入+20 ／ 補給1発につき1 ／ F8 保存 ／ F9 読込"), 28, 141, Muted, .85f);
		DrawRect(Ink, 16, 165, 560, 26);
  if(Match->bArea)Label(TEXT("本拠地：10秒で補充 ／ 兵1人につき資源1 ／ F8 保存 ／ F9 読込"),28,172,Mint,.85f);
  else if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch)Label(TEXT("本拠地・保有拠点で補給：1発につき資源1 ／ F8 保存 ／ F9 読込 ／ 支援攻撃なし"),28,172,Mint,.85f);
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(TEXT("固定戦力 ／ 増援・補給・支援攻撃なし"),28,172,Mint,.85f);
		else Label(FString::Printf(TEXT("[B] 支援攻撃 400 ／ 残り%d ／ 再装填 %.0f秒 ／ 選択部隊が知る位置へ"), 3 - Match->SupportUsed[0], Match->SupportCooldown[0]), 28, 172, Mint, .85f);
		for (const auto& Strike : Match->Strikes)
		{
			FVector2D Point;
			if (PC->ProjectWorldLocationToScreen(Strike.Position + FVector(0, 0, 100), Point) && Point.Y > 200 && Point.Y < H - 135)
			{
				DrawRect(Ink, Point.X - 75, Point.Y - 18, 150, 26);
				Label(FString::Printf(TEXT("着弾まで %.1f秒"), Strike.Remaining), Point.X - 65, Point.Y - 12, FLinearColor::Yellow);
			}
		}
		for (int32 Index = 0; Index < Match->Objectives.Num(); ++Index)
		{
   auto Point = Match->Objectives[Index];
   if(Match->bArea){Point.Owner=Match->ObservedOwner(0,Index);Point.bContested=false;Point.CaptureProgress=0;}
			const FLinearColor Color = Point.bContested ? FLinearColor::Yellow : Point.Owner == 0 ? Mint : Point.Owner == 1 ? FLinearColor(1, .35f, .25f) : White;
			FVector2D Center;
			if (PC->ProjectWorldLocationToScreen(Point.Position + FVector(0, 0, 45), Center) && Center.Y > 160 && Center.Y < H - 135)
			{
				DrawRect(Ink, Center.X - 48, Center.Y - 18, 170, 24);
    Label(Match->bArea?(Point.Owner==-2?FString::Printf(TEXT("%c ／ 不明"),'A'+Index):FString::Printf(TEXT("%c ／ %s ／ %.0f秒前に確認"),'A'+Index,Point.Owner==0?TEXT("味方"):Point.Owner==1?TEXT("敵"):TEXT("中立"),Match->Area.SeenAt[Index])):FString::Printf(TEXT("%c / %s %.0f%%"), 'A' + Index, Point.bContested ? TEXT("争奪中") : Point.Owner == 0 ? TEXT("味方") : Point.Owner == 1 ? TEXT("敵") : TEXT("中立"), Point.CaptureProgress / 8.f * 100.f), Center.X - 42, Center.Y - 12, Color, .85f);
			}
			for (int32 Segment = 0; Segment < 32; ++Segment)
			{
				const float A = Segment * 2.f * PI / 32, B = (Segment + 1) * 2.f * PI / 32;
				FVector2D P1, P2;
				if (PC->ProjectWorldLocationToScreen(Point.Position + FVector(FMath::Cos(A) * 450, FMath::Sin(A) * 450, 12), P1) && PC->ProjectWorldLocationToScreen(Point.Position + FVector(FMath::Cos(B) * 450, FMath::Sin(B) * 450, 12), P2) && P1.Y > 170 && P2.Y > 170 && P1.Y < H - 135 && P2.Y < H - 135) DrawLine(P1.X, P1.Y, P2.X, P2.Y, Color, 2);
			}
		}
	}

 if(Match->bArea)for(int32 Team=0;Team<2;++Team)
 {
  FVector2D P;if(PC->ProjectWorldLocationToScreen(Match->AreaHome(Team),P)&&P.Y>200&&P.Y<H-135)
  {DrawRect(Ink,P.X-65,P.Y-16,190,26);Label(Team?TEXT("敵本拠地（位置は公開）"):TEXT("自軍本拠地（補充区域）"),P.X-60,P.Y-10,Team?FLinearColor(1,.35f,.25f):Mint,.85f);}
 }
 TSet<int32> DrawnDefenseSquads;
	TArray<FVector2D> LabelPositions;
	if (!PC->IsTactical() && (LabelViewPawn.Get() != PC->GetPawn() || GetWorld()->GetTimeSeconds() >= NextLabelCheck))
	{
		LabelViewPawn = PC->GetPawn(); NextLabelCheck = GetWorld()->GetTimeSeconds() + .1; ClearLabels.Reset();
		FVector Eye; FRotator Look; PC->GetPlayerViewPoint(Eye, Look);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleLabels), false, PC->GetPawn());
		for (ABattleUnit* Unit : Mode->Units) if (Unit && Unit->IsAlive() && Unit->bRevealed && Unit != PC->GetPawn())
		{
			const FVector Target = Unit->EyePosition();
			if (FVector::DotProduct((Target - Eye).GetSafeNormal(), Look.Vector()) < .5) continue;
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Query) || Hit.GetActor() == Unit) ClearLabels.Add(Unit->GetUnitID());
		}
	}
	for (ABattleUnit* Unit : Mode->Units)
	{
		if (!Unit || !Unit->IsAlive() || !Unit->bRevealed || Unit == PC->GetPawn()) continue;
		if (!PC->IsTactical() && !ClearLabels.Contains(Unit->GetUnitID())) continue;
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Unit->GetActorLocation() + FVector(0, 0, 120), Screen)) continue;
		if (Screen.X < 20 || Screen.X > W - 20 || Screen.Y < 90 || Screen.Y > H - 130) continue;
		const FBattleUnitState* State = Unit->GetState();
		const FLinearColor Color = State->Team == 0 ? Mint : FLinearColor(1, .35f, .25f);
		const bool bDense = Mode->Units.Num() > 100 || !PC->IsTactical();
		bool bDrawLabel = true;
		if (bDense)
		{
			bDrawLabel = LabelPositions.Num() < (PC->IsTactical() ? 40 : 12);
			for (const FVector2D& Existing : LabelPositions)
				if (FMath::Abs(Existing.X - Screen.X) < 70 && FMath::Abs(Existing.Y - Screen.Y) < 32) { bDrawLabel = false; break; }
			DrawRect(Color, Screen.X - 2, Screen.Y, 4, 4);
		}
		if (bDrawLabel)
		{
			LabelPositions.Add(Screen);
			DrawRect(Ink, Screen.X - 25, Screen.Y, 50, 5);
			DrawRect(Color, Screen.X - 25, Screen.Y, 50 * FMath::Clamp(State->HP / FMath::Max(1.f, GetWorld()->GetSubsystem<UBattleState>()->MaxHealthOf(State->UnitID)), 0.f, 1.f), 5);
			Label(FString::Printf(TEXT("S%d / %02d"), State->SquadID, State->UnitID), Screen.X - 22, Screen.Y - 19, Color, .85f);
		}
		if (PC->IsSelected(Unit))
		{
			if (PC->IsTactical() && State->Order == EBattleOrder::Defend && !DrawnDefenseSquads.Contains(State->SquadID))
			{
				DrawnDefenseSquads.Add(State->SquadID);
				if (const auto* Squad = GetWorld()->GetSubsystem<UBattleState>()->FindSquad(State->SquadID))
					for (int32 Segment = 0; Segment < 48; ++Segment)
					{
						const float A = Segment * 2.f * PI / 48.f, B = (Segment + 1) * 2.f * PI / 48.f;
						FVector2D P1, P2;
						const FVector Center = Squad->Destination + FVector(0, 0, 12);
						if (PC->ProjectWorldLocationToScreen(Center + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Squad->DefenseRadius, P1) &&
							PC->ProjectWorldLocationToScreen(Center + FVector(FMath::Cos(B), FMath::Sin(B), 0) * Squad->DefenseRadius, P2))
							DrawLine(P1.X, P1.Y, P2.X, P2.Y, FLinearColor(.95f, .8f, .25f), 2.f);
					}
			}
			DrawLine(Screen.X - 33, Screen.Y - 22, Screen.X - 33, Screen.Y + 12, Mint, 2);
			DrawLine(Screen.X + 33, Screen.Y - 22, Screen.X + 33, Screen.Y + 12, Mint, 2);
			if (State->Order != EBattleOrder::Hold && PC->IsTactical())
			{
				FVector2D Goal;
				if (PC->ProjectWorldLocationToScreen(State->Destination, Goal)) DrawLine(Screen.X, Screen.Y + 10, Goal.X, Goal.Y, FLinearColor(.25f, .7f, .6f, .6f), 1);
			}
		}
	}
	if (PC->IsDragging())
	{
		FVector2D End = PC->DragOrigin; PC->GetPointerPosition(End);
		FVector2D Start = PC->DragOrigin;
		DrawRect(FLinearColor(.1f, .8f, .65f, .12f), FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y), FMath::Abs(Start.X - End.X), FMath::Abs(Start.Y - End.Y));
		DrawLine(Start.X, Start.Y, End.X, Start.Y, Mint); DrawLine(End.X, Start.Y, End.X, End.Y, Mint);
		DrawLine(End.X, End.Y, Start.X, End.Y, Mint); DrawLine(Start.X, End.Y, Start.X, Start.Y, Mint);
	}
	if (PC->CanShowDelegation())
	{
		int32 Manual = 0, Automatic = 0; PC->GetDelegationCounts(Manual, Automatic);
		DrawRect(Ink, 16, 238, 450, 94);
		Label(FString::Printf(TEXT("選択中の部隊 ／ 手動 %d ／ AI委任 %d"), Manual, Automatic), 28, 247, White, .95f);
		const bool bSelected = Manual + Automatic > 0;
		DrawRect(bSelected && Automatic == 0 ? FLinearColor(.12f,.38f,.32f,.98f) : FLinearColor(.10f,.15f,.20f,.98f), 28, 270, 200, 30);
		DrawRect(bSelected && Manual == 0 ? FLinearColor(.12f,.38f,.32f,.98f) : FLinearColor(.10f,.15f,.20f,.98f), 240, 270, 214, 30);
		Label(TEXT("[K] 手動指揮"), 40, 278, bSelected ? Mint : Muted, .9f);
		Label(TEXT("[J] AIに委任"), 252, 278, bSelected ? Mint : Muted, .9f);
		Label(TEXT("Q：全選択 ／ 移動・停止の命令で手動指揮に戻る"), 28, 310, Muted, .85f);
  if (GetWorld()->GetSubsystem<UBattleSquadTactics>()->IsEnabled() && !PC->GetSelection().IsEmpty())
   if (const auto* Selected = PC->GetSelection()[0]->GetState())
    if (const auto* Squad = GetWorld()->GetSubsystem<UBattleState>()->FindSquad(Selected->SquadID))
    {
     const FString Movement=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->MovementLabel(Squad->SquadID);
     DrawRect(Ink, 16, 336, 450, (Movement.IsEmpty()?79:98)+40);
     Label(FString::Printf(TEXT("部隊%d　任務：%s"), Squad->SquadID, *BattleUIText::Ja(BattleOrderName(Squad->bHasMission ? Squad->MissionOrder : Squad->Order))), 28, 343, White, .85f);
     Label(FString::Printf(TEXT("戦術：%s"), *BattleUIText::Ja(BattleSquadTacticName(Squad->TacticalPlan.Action))), 28, 362, Mint, .85f);
     const auto* Sample = GetWorld()->GetSubsystem<UBattleSquadTactics>()->Find(Squad->SquadID);
     const FString Reason = !Squad->bPlayerDelegated ? TEXT("手動 ／ 部隊AI") : Squad->TacticalPlan.Action != EBattleSquadTactic::Mission ? Squad->TacticalPlan.Reason : Sample ? Sample->Reason : TEXT("状態の取得待ち");
     Label(BattleUIText::Ja(Reason), 28, 382, Muted, .8f);
     if(!Movement.IsEmpty()) Label(Movement,28,402,Muted,.8f);
     // Squad state (own soldiers) and the last decision reasons, newest first.
     {
      auto* LR=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const double Now=GetWorld()->GetTimeSeconds();
      int32 Living=0,Ammo=0,Crouch=0,Prone=0,Pinned=0,InCover=0; float HP=0;
      for(int32 ID:Squad->Members) if(const auto* U=Store->Find(ID); U && U->IsAlive())
      {++Living;HP+=U->HP;Ammo+=U->Ammo+U->Reserve;const uint8 St=LR->StanceOf(ID);Crouch+=St==BattleStance::Crouch;Prone+=St==BattleStance::Prone;Pinned+=LR->Suppression(ID,Now)>=.5f;InCover+=U->CoverPhase!=0;}
      const float Y0=Movement.IsEmpty()?402.f:421.f;
      Label(FString::Printf(TEXT("生存 %d/%d   HP %.0f   弾 %d   しゃがみ %d・伏せ %d   制圧 %d   遮蔽 %d"),Living,Squad->Members.Num(),HP,Ammo,Crouch,Prone,Pinned,InCover),28,Y0,White,.8f);
      FString History; if(const auto* Past=PC->ReasonHistory.Find(Squad->SquadID)) for(int32 I=0;I<Past->Num();++I) History+=(I?TEXT(" ← "):TEXT(""))+BattleUIText::Ja((*Past)[I]);
      Label(TEXT("履歴：")+(History.IsEmpty()?FString(TEXT("なし")):History.Left(60)),28,Y0+19,Muted,.75f);
     }
    }
	}
	DrawRect(Ink, 16, H - 128, W - 32, 112);
	const ABattleUnit* Focus = PC->IsTactical() ? (PC->GetSelection().IsEmpty() ? nullptr : PC->GetSelection()[0].Get()) : Cast<ABattleUnit>(PC->GetPawn());
	if (Focus && Focus->GetState())
	{
		const FBattleUnitState* S = Focus->GetState();
		Label(FString::Printf(TEXT("%s %02d ／ 部隊%02d   HP %.0f   弾 %02d / %02d   %s ／ %s   再試行 %d%s"), S->Weapon == EBattleWeapon::AntiTank ? TEXT("対戦車火器") : S->Weapon == EBattleWeapon::SupportGun ? TEXT("支援火器") : TEXT("小銃"), S->UnitID, S->SquadID, S->HP, S->Ammo, S->Reserve,
			*BattleUIText::Ja(BattleOrderName(S->Order)), *BattleUIText::Ja(BattleActionName(S->Action)), S->MoveRetries, S->ReloadEndTime > 0 ? TEXT("   装填中…") : TEXT("")), 34, H - 112, S->Action == EBattleAction::MoveFailed ? FLinearColor::Yellow : White, 1.05f);
		if(!Mode->bLeaderCommand)
        {
		int32 Seen = 0, Remembered = 0, Shared = 0;
		if (const auto* Contacts = GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(S->UnitID))
			for (const auto& Pair : *Contacts)
			{
				const auto& Contact = Pair.Value;
				if (Contact.bVisible) ++Seen; else if (Contact.bShared) ++Shared; else ++Remembered;
				if (PC->bShowContactDebug && !Contact.bVisible)
				{
					FVector2D Point;
					if (PC->ProjectWorldLocationToScreen(Contact.LastKnownPosition + FVector(0, 0, 120), Point))
					{
						DrawLine(Point.X - 6, Point.Y - 6, Point.X + 6, Point.Y + 6, FLinearColor::Yellow, 2.f);
						DrawLine(Point.X - 6, Point.Y + 6, Point.X + 6, Point.Y - 6, FLinearColor::Yellow, 2.f);
						Label(FString::Printf(TEXT("最後 %02d ／ %.1f秒 ／ %.0f%% ／ %s %02d"), Contact.TargetID, GetWorld()->GetTimeSeconds() - Contact.LastSeenTime, Contact.Confidence * 100.f,
							Contact.bShared ? TEXT("共有元") : TEXT("発見者"), Contact.SourceUnitID), Point.X + 10, Point.Y, FLinearColor::Yellow, .8f);
					}
				}
			}
		DrawRect(Ink, W - 576, 112, 560, 27);
		Label(FString::Printf(TEXT("視界 %.0fm ／ %.0f度 ／ %.1f秒   視認 %d  記憶 %d  共有 %d ／ F6 %s"), S->Perception.SightRange / 100.f, S->Perception.FieldOfView, S->Perception.DetectionSeconds, Seen, Remembered, Shared, PC->bShowContactDebug ? TEXT("入") : TEXT("切")), W - 566, 119, White, .85f);
		DrawRect(Ink, W - 576, 141, 560, 27);
		FString CoverText = TEXT("遮蔽：観測した脅威なし");
		if (const auto* Contacts = GetWorld()->GetSubsystem<UBattlePerception>()->GetContacts(S->UnitID))
		{
			const FBattleContact* Nearest = nullptr; float Best = TNumericLimits<float>::Max();
			for (const auto& Pair : *Contacts) if (Pair.Value.bVisible)
			{
				const float Distance = FVector::DistSquared(Focus->GetActorLocation(), Pair.Value.LastKnownPosition);
				if (Distance < Best) { Best = Distance; Nearest = &Pair.Value; }
			}
			if (Nearest)
			{
				const auto Exposure = BattleTactics::ExposureFrom(Focus, Nearest->LastKnownPosition + FVector(0, 0, 60));
				CoverText = FString::Printf(TEXT("露出 %d/5（対 %02d）%s"), Exposure.VisibleSamples, Nearest->TargetID, Exposure.bFlanked ? TEXT(" ／ 側面を取られた") : TEXT(""));
			}
		}
		/* Suppression trial: the shown spread includes the same multiplier the shot uses. */ const auto* Leaders = GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); const float SuppressedScale = Leaders->SuppressionSpreadScale(S->UnitID, GetWorld()->GetTimeSeconds());
		if (SuppressedScale > 1.f) CoverText = FString::Printf(TEXT("制圧 %d%%  %s"), FMath::RoundToInt(Leaders->Suppression(S->UnitID, GetWorld()->GetTimeSeconds()) * 100.f), *CoverText);
		const float Steady = S->AimStableSince < 0. ? 0.f : static_cast<float>(GetWorld()->GetTimeSeconds() - S->AimStableSince);
		Label(FString::Printf(TEXT("照準 %.1f秒   拡散 %.2f度   %s"), FMath::Min(Steady, GetDefault<UBattleTuning>()->AimSettleSeconds), BattleTactics::SpreadDegrees(*S, GetWorld()->GetTimeSeconds(), SuppressedScale), *CoverText), W - 566, 148, White, .85f);
		DrawRect(Ink, W - 576, 170, 560, 27);
		Label(S->DecisionReason.IsEmpty() ? TEXT("方針：命令に従う") : FString::Printf(TEXT("方針：%s ／ 位置 %d"), *BattleUIText::Ja(S->DecisionReason), S->CoverSlot), W - 566, 177, Mint, .85f);
		DrawRect(Ink, W - 576, 199, 560, 27);
		const FString RoleText = S->Role == EBattleRole::Independent ? TEXT("単独") : FString::Printf(TEXT("%s ／ 集中 %02d"), BattleRoleName(S->Role), S->CoordinatedTarget);
		Label(FString::Printf(TEXT("%s ／ 試験費用 %d ／ 役割：%s"), BattleTrainingName(S->Training), S->HireCost, *RoleText), W - 566, 206, White, .85f);
		if (PC->IsTactical() && S->Role == EBattleRole::Flank)
		{
			FVector2D Goal, Origin;
			if (PC->ProjectWorldLocationToScreen(S->TacticalDestination, Goal) && PC->ProjectWorldLocationToScreen(Focus->GetActorLocation(), Origin))
			{ DrawLine(Origin.X, Origin.Y, Goal.X, Goal.Y, FLinearColor(.4f, .7f, 1.f), 2.f); DrawRect(FLinearColor(.4f, .7f, 1.f), Goal.X - 5, Goal.Y - 5, 10, 10); }
		}
		if (PC->IsTactical() && S->CoverSlot != INDEX_NONE)
		{
			const auto& Slots = GetWorld()->GetSubsystem<UBattleCover>()->GetSlots();
			FVector2D Point, Origin;
			if (Slots.IsValidIndex(S->CoverSlot) && PC->ProjectWorldLocationToScreen(Slots[S->CoverSlot] + FVector(0, 0, 15), Point))
			{
				DrawRect(FLinearColor::Yellow, Point.X - 5, Point.Y - 5, 10, 10);
				if (PC->ProjectWorldLocationToScreen(Focus->GetActorLocation(), Origin)) DrawLine(Origin.X, Origin.Y, Point.X, Point.Y, FLinearColor::Yellow, 2.f);
			}
		}
        }
	}
	else Label(TEXT("味方の部隊を選んで開始"), 34, H - 112, White, 1.2f);
	if (PC->IsTactical())
	{
		// Command bar (command UI 2026-11-16): buttons call the same actions as the keys shown in brackets.
		{
			const auto Bar = FBattleCommandBarLayout::Make(FVector2D(W, H));
			static const TCHAR* const KeyNames[] = { TEXT("移動 [右クリック]"), TEXT("攻撃移動 [T]"), TEXT("防衛 [G]"), TEXT("停止 [H]"), TEXT("手動指揮 [K]"), TEXT("AIに委任 [J]"), TEXT("小隊を購入 [2]"), TEXT("重装歩兵 [3]"), TEXT("偵察組 [4]"), TEXT("対戦車班 [5]"), TEXT("戦車 [6]") };
			static const TCHAR* const PadNames[] = { TEXT("移動 [X]"), TEXT("攻撃移動 [Y]"), TEXT("防衛 [RB]"), TEXT("停止 [B]"), TEXT("手動指揮 [十字↑]"), TEXT("AIに委任 [十字↓]"), TEXT("小隊を購入 [十字←]"), TEXT("重装歩兵 [Aでクリック]"), TEXT("偵察組 [Aでクリック]"), TEXT("対戦車班 [Aでクリック]"), TEXT("戦車 [Aでクリック]") };
			const TCHAR* const* Names = PC->bGamepadActive ? PadNames : KeyNames;
			const bool bAny = !PC->GetSelection().IsEmpty();
			for (int32 I = 0; I < FBattleCommandBarLayout::Count; ++I)
			{
				const float X = Bar.Origin.X + I * (Bar.ButtonWidth + Bar.Gap);
				const bool bUsable = I >= 6 ? (Match->bArea && BattleTactics::AreaHire().bEnabled) : bAny;
				const bool bPending = (I == 1 && PC->PendingOrder == EBattleOrder::AttackMove) || (I == 2 && PC->PendingOrder == EBattleOrder::Defend);
				DrawRect(bPending ? FLinearColor(.45f, .4f, .1f, .98f) : bUsable ? FLinearColor(.10f, .24f, .24f, .98f) : FLinearColor(.08f, .11f, .15f, .98f), X, Bar.Origin.Y, Bar.ButtonWidth, Bar.ButtonHeight);
				Label(Names[I], X + 8, Bar.Origin.Y + 5, bPending ? FLinearColor::Yellow : bUsable ? Mint : Muted, .85f);
			}
		}
		if (PC->bGamepadActive) Label(TEXT("右スティック：カーソル   A：選択・決定   左スティック：視点移動   LT/RT：拡大縮小   LB：全選択   ビュー：直接操作   メニュー：AI一時停止   十字→：速度"), 34, H - 53, Muted, .9f);
		else
		Label(FString::Printf(TEXT("左ドラッグ：選択   Q：全選択   E/Tab：直接操作   WASD：視点移動   ホイール：拡大縮小   P：AI一時停止   F5：やり直し   F6：記憶表示   %s      選択中 %02d"), Match->IsEnabled() ? TEXT("1〜6：購入") : TEXT("F7：練度の切替"), PC->GetSelection().Num()), 34, H - 53, Muted, .9f);
	}
	else
	{
		if (PC->bGamepadActive) Label(TEXT("左スティック：移動   右スティック：照準（補助あり）   RT：射撃   X：装填   A：ジャンプ   B：姿勢   Y：遮蔽   ビュー：作戦画面"), 34, H - 80, Mint);
		else
		Label(TEXT("WASD：移動   マウス：照準   左クリック：射撃   R：装填   Space：ジャンプ   Tab：作戦画面"), 34, H - 80, Mint);
		Label(TEXT("P：AI一時停止   F5：戦闘をやり直す   ｜   操作を切り替えても、兵・HP・弾薬・命令は引き継がれる"), 34, H - 53, Muted, .9f);
		const FLinearColor CrossColor = PC->HitMarkerUntil > GetWorld()->GetTimeSeconds() ? FLinearColor::Yellow : White;
		DrawLine(W / 2 - 13, H / 2, W / 2 - 4, H / 2, CrossColor, 1.5f);
		DrawLine(W / 2 + 4, H / 2, W / 2 + 13, H / 2, CrossColor, 1.5f);
		DrawLine(W / 2, H / 2 - 13, W / 2, H / 2 - 4, CrossColor, 1.5f);
		DrawLine(W / 2, H / 2 + 4, W / 2, H / 2 + 13, CrossColor, 1.5f);
	}
	if (PC->NoticeUntil > GetWorld()->GetTimeSeconds()) Label(BattleUIText::Ja(PC->Notice), 35, PC->CanShowDelegation() ? H - 156 : Match->IsEnabled() ? 235 : 96, Mint, 1.1f);
	if (PC->IsTactical() && PC->PendingOrder != EBattleOrder::Move)
		Label(FString::Printf(TEXT("%s：右クリックで目的地 ／ Hで取消"), *BattleUIText::Ja(BattleOrderName(PC->PendingOrder))), 35, 148, FLinearColor::Yellow);
	if (!Match->IsEnabled() && GetWorld()->GetTimeSeconds() < Mode->CombatStartsAt)
		Label(FString::Printf(TEXT("展開中 ／ AIの前進まで %.0f秒"), FMath::CeilToFloat(Mode->CombatStartsAt - GetWorld()->GetTimeSeconds())), 35, 123, White);
	if (Mode->bBattleEnded)
	{
		DrawRect(Ink, W * .25f, H * .35f, W * .5f, 100);
		Label(BattleUIText::Ja(Mode->Result), W * .25f + 25, H * .35f + 16, Mint, 1.8f);
		Label(TEXT("F5で再展開"), W * .25f + 25, H * .35f + 60, White);
		// War score trial: both teams' breakdown is disclosed once the battle has ended.
		if (Match->bArea && BattleWarScore::Tuning().bEnabled)
		{
			DrawRect(Ink, W * .25f, H * .35f + 104, W * .5f, 58);
			Label(FString::Printf(TEXT("自軍  %s"), *Match->WarScoreBreakdown(0)), W * .25f + 25, H * .35f + 112, Mint);
			Label(FString::Printf(TEXT("敵軍  %s"), *Match->WarScoreBreakdown(1)), W * .25f + 25, H * .35f + 136, White);
		}
	}
 DrawOrderMarks();
 DrawSquadList();
 DrawMinimap();
 DrawNoticeLog();
}

void ABattleHUD::DrawMinimap()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawMinimap);
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 const auto Map=FBattleMinimapLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld()); if(Map.Size.X<=0) return;
 const FLinearColor Ink(.025f,.04f,.065f,.94f), Ground(.07f,.1f,.13f,.96f), Mint(.28f,.95f,.8f), Enemy(1,.35f,.25f), Stale(.95f,.65f,.3f,.75f), White(.94f,.97f,1.f), Muted(.45f,.52f,.58f);
 auto Dot=[&](FVector World,float Size,FLinearColor Color){const FVector2D P=Map.ToScreen(World);if(Map.Contains(P))DrawRect(Color,P.X-Size*.5f,P.Y-Size*.5f,Size,Size);};
 auto Frame=[&](FVector2D A,FVector2D B,FLinearColor Color)
 {DrawLine(A.X,A.Y,B.X,A.Y,Color,1);DrawLine(B.X,A.Y,B.X,B.Y,Color,1);DrawLine(B.X,B.Y,A.X,B.Y,Color,1);DrawLine(A.X,B.Y,A.X,A.Y,Color,1);};
 DrawRect(Ink,Map.Origin.X-5,Map.Origin.Y-24,Map.Size.X+10,Map.Size.Y+29);
 DrawText(TEXT("地図（観測情報のみ）"),Muted,Map.Origin.X,Map.Origin.Y-20,GEngine->GetSmallFont(),.8f);
 DrawRect(Ground,Map.Origin.X,Map.Origin.Y,Map.Size.X,Map.Size.Y);
 // Map E: the city block outline (public map knowledge).
 if(const FVector2D City=BattleFieldLayout::Of(GetWorld()).CityHalf;City.X>0) Frame(Map.ToScreen(FVector(City.X,-City.Y,0)),Map.ToScreen(FVector(-City.X,City.Y,0)),Muted);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 // Homes are public; objective ownership is what the player's side last observed.
 if(Match->bArea) for(int32 Team=0;Team<2;++Team)
 {const FVector2D P=Map.ToScreen(Match->AreaHome(Team));Frame(P-FVector2D(7,7),P+FVector2D(7,7),Team?Enemy:Mint);}
 for(int32 Index=0;Index<Match->Objectives.Num();++Index)
 {
  const int32 Holder=Match->bArea?Match->ObservedOwner(0,Index):Match->Objectives[Index].Owner;
  const FLinearColor Color=Holder==0?Mint:Holder==1?Enemy:Holder==-2?Muted:White;
  const FVector2D P=Map.ToScreen(Match->Objectives[Index].Position);
  DrawRect(Color,P.X-5,P.Y-5,10,10);
  DrawText(FString::Printf(TEXT("%c"),'A'+Index),Color,P.X+7,P.Y-7,GEngine->GetSmallFont(),.8f);
 }
 // Lookouts (public map places): a small diamond.
 for(const FVector& Look:BattleFieldLayout::Lookouts(GetWorld()))
 {const FVector2D P=Map.ToScreen(Look);DrawLine(P.X,P.Y-6,P.X+6,P.Y,White,1);DrawLine(P.X+6,P.Y,P.X,P.Y+6,White,1);DrawLine(P.X,P.Y+6,P.X-6,P.Y,White,1);DrawLine(P.X-6,P.Y,P.X,P.Y-6,White,1);}
 // Enemies: only contacts held by the player's side. Seen now = solid; remembered = dim, at the last known position.
 TMap<int32,const FBattleContact*> Known;
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 for(const auto& Pair:Store->GetUnits()) if(Pair.Value.Team==0 && Pair.Value.IsAlive())
  if(const auto* Contacts=Sense->GetContacts(Pair.Key)) for(const auto& C:*Contacts)
  {
   // A scout's long-range sightings appear only through its reports (drawn below).
   if(Match->bArea&&Match->IsScoutLongContact(Pair.Key,C.Value)) continue;
   const FBattleContact*& Best=Known.FindOrAdd(C.Key);
   if(!Best || (C.Value.bVisible && !Best->bVisible) || (C.Value.bVisible==Best->bVisible && C.Value.LastSeenTime>Best->LastSeenTime)) Best=&C.Value;
  }
 for(const auto& Pair:Known) if(!Pair.Value->bVisible && Pair.Value->Confidence>.35f) Dot(Pair.Value->LastKnownPosition,3,Stale);
 for(const auto& Pair:Known) if(Pair.Value->bVisible) Dot(Pair.Value->LastKnownPosition,4,Enemy);
 // Scout reports: own pairs' direction alerts (a line from the pair) and reported positions; enemy observers reporting near own
 // soldiers as a circle at their approximate position.
 if(Match->bArea)
 {
  const float Memory=GetDefault<UBattleTuning>()->ScoutReportMemory;
  for(const auto& P:Match->ScoutReports)
  {
   const auto* S=Store->FindSquad(P.Key); if(!S) continue; const auto& R=P.Value;
   if(S->Team==0)
   {
    if(R.AlertAt>=0&&Match->Elapsed-R.AlertAt<=Memory)
    {const FVector From=L->Position(S->LeaderUnitID);const FVector2D A=Map.ToScreen(From),B=Map.ToScreen(From+R.Direction*30000.f);DrawLine(A.X,A.Y,B.X,B.Y,Stale,2);}
    if(R.ReportedAt>=0&&Match->Elapsed-R.ReportedAt<=Memory) for(const FVector& Where:R.Positions) Dot(Where,4,Stale);
   }
   else if(R.Started>=0)
   {
    bool Near=false;for(const auto& U:Store->GetUnits()) if(U.Value.Team==0&&U.Value.IsAlive()&&FVector::DistSquared2D(L->Position(U.Key),R.Approx)<=FMath::Square(GetDefault<UBattleTuning>()->ScoutHintRadius)){Near=true;break;}
    if(Near){const FVector2D C=Map.ToScreen(R.Approx);const float Rad=FMath::Max(4.f,FMath::Abs(float(Map.ToScreen(R.Approx+FVector(GetDefault<UBattleTuning>()->ScoutHintGrid,0,0)).X-C.X)));
     for(int32 I=0;I<16;++I){const float A0=I*PI/8,A1=(I+1)*PI/8;DrawLine(C.X+Rad*FMath::Cos(A0),C.Y+Rad*FMath::Sin(A0),C.X+Rad*FMath::Cos(A1),C.Y+Rad*FMath::Sin(A1),Enemy,1);}}
   }
  }
 }
 // Own squads: one mark per leader; selected squads stand out, stopped squads are not drawn.
 for(const auto& S:L->Summaries()) if(S.bActive)
 {
  auto* Leader=L->Actor(S.LeaderUnitID); const bool Selected=Leader && PC->IsSelected(Leader);
  Dot(L->Position(S.LeaderUnitID),Selected?7:5,Selected?White:Mint);
 }
 // Where the tactical camera is looking.
 if(const APawn* Camera=PC->GetPawn()){const FVector2D P=Map.ToScreen(Camera->GetActorLocation());if(Map.Contains(P))Frame(P-FVector2D(9,6),P+FVector2D(9,6),White);}
}

void ABattleHUD::DrawSquadList()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawSquadList);
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 PC->ScrollSquadList(0); const auto Layout=FBattleSquadListLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY));
 if(Layout.VisibleRows<=0) return;
 const auto& All=L->Summaries(); int32 Active=0; for(const auto& S:All) if(S.bActive) ++Active;
 // Rows after the list filter and sort (command UI 2026-11-16); counts per filter for the tabs.
 const auto Rows=PC->SquadListRows(); int32 Counts[FBattleSquadListLayout::FilterCount]={All.Num(),0,0,0,0}; for(const auto& S:All) ++Counts[PC->SquadCategory(S)];
 DrawRect(FLinearColor(.025f,.04f,.065f,.96f),Layout.Origin.X,Layout.Origin.Y,Layout.Width,Layout.Height);
 DrawText(FString::Printf(TEXT("自軍部隊　稼働 %d / 停止 %d"),Active,All.Num()-Active),FLinearColor::White,Layout.Origin.X+8,Layout.Origin.Y+5,GEngine->GetSmallFont(),1);
 DrawText(TEXT("クリックで選択・ダブルクリックで移動・Eで操作"),FLinearColor(.6f,.65f,.7f),Layout.Origin.X+8,Layout.Origin.Y+Layout.Height-18,GEngine->GetSmallFont(),.7f);
 static const TCHAR* const FilterNames[]={TEXT("全部"),TEXT("交戦"),TEXT("補給"),TEXT("待機"),TEXT("停止")};
 for(int32 T=0;T<FBattleSquadListLayout::FilterCount;++T)
 {
  const float X=Layout.Origin.X+6+T*FBattleSquadListLayout::TabWidth,Y=Layout.Origin.Y+FBattleSquadListLayout::TabTop; const bool On=PC->SquadFilter==T;
  DrawRect(On?FLinearColor(.12f,.38f,.36f):FLinearColor(.08f,.11f,.15f),X,Y,FBattleSquadListLayout::TabWidth-4,FBattleSquadListLayout::TabHeight);
  DrawText(FString::Printf(TEXT("%s %d"),FilterNames[T],Counts[T]),On?FLinearColor::White:FLinearColor(.7f,.75f,.8f),X+4,Y+2,GEngine->GetSmallFont(),.75f);
 }
 static const TCHAR* const SortNames[]={TEXT("並び：番号順"),TEXT("並び：損耗順"),TEXT("並び：拠点別")};
 {
  const float X=Layout.Origin.X+Layout.Width-FBattleSquadListLayout::SortWidth-8,Y=Layout.Origin.Y+FBattleSquadListLayout::TabTop;
  DrawRect(FLinearColor(.08f,.11f,.15f),X,Y,FBattleSquadListLayout::SortWidth,FBattleSquadListLayout::TabHeight);
  DrawText(SortNames[FMath::Min<int32>(PC->SquadSort,2)],FLinearColor(.7f,.75f,.8f),X+4,Y+2,GEngine->GetSmallFont(),.75f);
 }
 for(int32 I=0;I<Layout.VisibleRows && I+PC->SquadListScroll<Rows.Num();++I)
 {
  const auto& S=Rows[I+PC->SquadListScroll]; const float Y=Layout.Origin.Y+Layout.Header+I*Layout.RowHeight;
  const FLinearColor Color=!S.bActive?FLinearColor(.55f,.55f,.55f):!S.bSelectable?FLinearColor(.95f,.7f,.3f):FLinearColor(.28f,.95f,.8f);
  if(auto* A=L->Actor(S.LeaderUnitID); A && PC->IsSelected(A)) DrawRect(FLinearColor(.08f,.22f,.22f),Layout.Origin.X+3,Y,Layout.Width-12,Layout.RowHeight-2);
  const TCHAR* State=!S.bActive?TEXT("× 停止"):!S.bSelectable?TEXT("鍵 操作不可"):TEXT("○ 稼働");
  FString Name=S.Name.Len()>12?S.Name.Left(11)+TEXT("…"):S.Name;
  DrawText(Name,Color,Layout.Origin.X+9,Y+4,GEngine->GetSmallFont(),1);
  DrawText(State,Color,Layout.Origin.X+Layout.Width-110,Y+4,GEngine->GetSmallFont(),1);
  if(const auto* Squad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(S.SquadID))
  {
   const auto& Plan=Squad->TacticalPlan;
   const bool Mission=Plan.Action==EBattleSquadTactic::Mission;
   const FVector Goal=Mission?Squad->Destination:Plan.Destination;
   const int32 Partner=Plan.Action==EBattleSquadTactic::Support?Plan.SupportSquad:Plan.Action==EBattleSquadTactic::Flank?Plan.FlankSupport:INDEX_NONE;
   const FString Link=Partner==INDEX_NONE?TEXT(""):FString::Printf(TEXT(" / S%d"),Partner);
   const FString Action=Mission?BattleUIText::Ja(BattleOrderName(Squad->Order)):FString::Printf(TEXT("%s %.0f秒"),*BattleUIText::Ja(BattleSquadTacticName(Plan.Action)),Plan.Age);
   DrawText(FString::Printf(TEXT("%s ／ %.0fm%s"),*Action,FVector::Dist2D(L->Position(S.LeaderUnitID),Goal)/100.,*Link),Color,Layout.Origin.X+9,Y+23,GEngine->GetSmallFont(),.8f);
   const auto* Sample=GetWorld()->GetSubsystem<UBattleSquadTactics>()->Find(S.SquadID);
   int32 Waiting=0;for(int32 ID:Squad->Members)if(const auto* Body=L->GetBodies().Find(ID))Waiting+=Body->bTrafficWait;
   FString Reason=!S.bActive?Squad->StopReason.ToString():!Squad->bPlayerDelegated?TEXT("手動指揮"):
    Waiting>0?FString::Printf(TEXT("通行待ち %d"),Waiting):!Mission?Plan.Reason:Sample?Sample->Reason:TEXT("報告待ち");
   Reason=BattleUIText::Ja(Reason);
   if(Reason.Len()>44)Reason=Reason.Left(43)+TEXT("...");
   DrawText(Reason,FLinearColor(.72f,.78f,.8f),Layout.Origin.X+9,Y+40,GEngine->GetSmallFont(),.7f);
  }
 }
 if(Rows.Num()>Layout.VisibleRows)
 {
  const float Track=Layout.VisibleRows*Layout.RowHeight, Thumb=Track*Layout.VisibleRows/Rows.Num();
  const float Top=(Track-Thumb)*PC->SquadListScroll/FMath::Max(1,Rows.Num()-Layout.VisibleRows);
  DrawRect(FLinearColor(.65f,.65f,.65f),Layout.Origin.X+Layout.Width-5,Layout.Origin.Y+Layout.Header+Top,3,Thumb);
 }
}

void ABattleHUD::DrawNoticeLog()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawNoticeLog);
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 const auto Notices=PC->VisibleNotices(); if(Notices.IsEmpty()) return;
 const auto Layout=FBattleNoticeLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld(),Notices.Num(),PC->NoticeLogTop()); if(Layout.Rows<=0) return;
 static const FLinearColor Colors[]={FLinearColor(1,.35f,.25f),FLinearColor(.28f,.95f,.8f),FLinearColor(.95f,.65f,.3f),FLinearColor(.95f,.65f,.3f),FLinearColor(1,.9f,.35f),FLinearColor(.94f,.97f,1.f)};
 DrawRect(FLinearColor(.025f,.04f,.065f,.9f),Layout.Origin.X-5,Layout.Origin.Y,Layout.Width+10,Layout.Header+Layout.Rows*Layout.RowHeight+4);
 DrawText(TEXT("通知（クリックで現地へ）"),FLinearColor(.45f,.52f,.58f),Layout.Origin.X,Layout.Origin.Y+3,GEngine->GetSmallFont(),.8f);
 const double Now=GetWorld()->GetTimeSeconds();
 for(int32 I=0;I<Layout.Rows;++I)
 {
  const auto& E=*Notices[I]; const float Y=Layout.Origin.Y+Layout.Header+I*Layout.RowHeight;
  FLinearColor Color=Colors[FMath::Min<int32>(E.Kind,5)]; if(Now-E.Time>20.) Color.A=.6f; // older entries fade
  const FString Text=E.Count>1?FString::Printf(TEXT("%s ×%d"),*E.Text,E.Count):E.Text;
  DrawText(FString::Printf(TEXT("%3.0f秒前  %s"),Now-E.Time,*Text),Color,Layout.Origin.X,Y+3,GEngine->GetSmallFont(),.9f);
 }
}

void ABattleHUD::DrawOrderMarks()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawOrderMarks);
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical() || PC->GetSelection().IsEmpty()) return;
 const float H=Canvas->SizeY; const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Screen=[&](FVector World,FVector2D& Out){return PC->ProjectWorldLocationToScreen(World,Out)&&Out.Y>110&&Out.Y<H-130;};
 auto Segment=[&](FVector A,FVector B,FLinearColor Color,float Thickness)
 {
  // Projected in short pieces so a line that leaves the screen is still drawn where it is visible.
  FVector2D P0; bool bPrev=Screen(A,P0);
  for(int32 I=1;I<=12;++I){FVector2D P1;const bool bNow=Screen(FMath::Lerp(A,B,I/12.f),P1);if(bPrev&&bNow)DrawLine(P0.X,P0.Y,P1.X,P1.Y,Color,Thickness);P0=P1;bPrev=bNow;}
 };
 TSet<int32> Done;
 for(const auto& Unit:PC->GetSelection())
 {
  if(!Unit||!Unit->GetState()) continue; const int32 SID=Unit->GetState()->SquadID; if(Done.Contains(SID)) continue; Done.Add(SID);
  const auto* S=Store->FindSquad(SID); if(!S||S->CommandStatus!=EBattleCommandStatus::Active) continue;
  const EBattleOrder Order=S->bHasMission?S->MissionOrder:S->Order; const FVector Goal=(S->bHasMission?S->MissionDestination:S->Destination)+FVector(0,0,25);
  if(Order==EBattleOrder::Hold) continue;
  const FLinearColor Color=Order==EBattleOrder::AttackMove?FLinearColor(1,.6f,.2f,.9f):Order==EBattleOrder::Defend?FLinearColor(.45f,.7f,1,.9f):FLinearColor(.28f,.95f,.8f,.9f);
  // Line from the leader to the destination, and a diamond at the destination.
  Segment(L->Position(S->LeaderUnitID)+FVector(0,0,40),Goal,Color,1.5f);
  FVector2D G; if(Screen(Goal,G)){DrawLine(G.X,G.Y-9,G.X+9,G.Y,Color,2);DrawLine(G.X+9,G.Y,G.X,G.Y+9,Color,2);DrawLine(G.X,G.Y+9,G.X-9,G.Y,Color,2);DrawLine(G.X-9,G.Y,G.X,G.Y-9,Color,2);}
  // Defence radius.
  if(Order==EBattleOrder::Defend)
   for(int32 I=0;I<32;++I){const float A=I*2*PI/32,B=(I+1)*2*PI/32;Segment(Goal+FVector(FMath::Cos(A),FMath::Sin(A),0)*S->DefenseRadius,Goal+FVector(FMath::Cos(B),FMath::Sin(B),0)*S->DefenseRadius,Color*FLinearColor(1,1,1,.6f),1);}
  // Arrival slots of living members.
  if(const auto* Plan=L->PlanOf(SID)) for(const auto& Slot:Plan->ArrivalGoals) if(const auto* M=Store->Find(Slot.Key); M&&M->IsAlive())
  {FVector2D P; if(Screen(Slot.Value+FVector(0,0,20),P)) DrawRect(Color*FLinearColor(1,1,1,.7f),P.X-2,P.Y-2,4,4);}
 }
}
