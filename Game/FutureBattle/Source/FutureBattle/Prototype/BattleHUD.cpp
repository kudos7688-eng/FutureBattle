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
#include "BattleRanges.h"
#include "BattleFieldLayout.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "UnrealClient.h"
#include "GlobalRenderResources.h"
#include "BattleSectors.h"
#include "BattleSectorView.h"
#include "Engine/Engine.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"

void ABattleHUD::DrawHUD()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawHUD);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(BattleHUD);
	Super::DrawHUD();
	ABattleController* PC = Cast<ABattleController>(PlayerOwner);
	ABattleGameMode* Mode = GetWorld()->GetAuthGameMode<ABattleGameMode>();
	if (!Canvas || !PC || !Mode) return;
 const auto* HUDMatch=GetWorld()->GetSubsystem<UBattleMatch>();
 if(FogRestoreGeneration!=HUDMatch->RestoreGeneration)
 {Fog.Reset(GetWorld());Sectors.Reset();FogRestoreGeneration=HUDMatch->RestoreGeneration;}
	const float W = Canvas->SizeX, H = Canvas->SizeY;
 const auto BottomBar=FBattleCommandBarLayout::Make(FVector2D(W,H),GetWorld(),PC->bGamepadActive);
 const float PanelTop=PC->IsTactical()?BottomBar.PanelTop:H-128.f;
	const FLinearColor Ink(.025f, .04f, .065f, .92f), Mint(.28f, .95f, .8f), Muted(.62f, .7f, .77f), White(.94f, .97f, 1.f);
	auto Label = [&](const FString& Text, float X, float Y, FLinearColor Color, float Scale = 1.f)
	{ if(PC->IsTactical()&&Y>=PanelTop){float LabelWidth=0,LabelHeight=0;Canvas->StrLen(GEngine->GetSmallFont(),Text,LabelWidth,LabelHeight);Scale=FMath::Min(Scale,FMath::Max(1.f,W-X-34.f)/FMath::Max(1.f,LabelWidth));}
   DrawText(Text, Color, X, Y, GEngine->GetSmallFont(), Scale); };
	// Scope (direct control, right button + wheel): black outside the lens and a reticle, under the rest of the HUD.
	const ABattleUnit* ScopeUnit = PC->IsTactical() ? nullptr : Cast<ABattleUnit>(PC->GetPawn());
	const bool bScope = ScopeUnit && ScopeUnit->ScopeView();
	if (bScope)
	{
		const float R = H * .46f, Cx = W * .5f, Cy = H * .5f;
		const FLinearColor Black(0, 0, 0, 1), Line(.95f, .16f, .1f, 1); // red: readable on a dark and on a bright scene
		DrawRect(Black, 0, 0, Cx - R, H); DrawRect(Black, Cx + R, 0, W - Cx - R, H);
		DrawRect(Black, Cx - R, 0, 2 * R, Cy - R); DrawRect(Black, Cx - R, Cy + R, 2 * R, H - Cy - R);
		for (float Y = -R; Y < R; Y += 3.f)
		{
			const float Mid = FMath::Min(FMath::Abs(Y), FMath::Abs(Y + 3.f)), Half = FMath::Sqrt(FMath::Max(0.f, R * R - Mid * Mid));
			DrawRect(Black, Cx - R, Cy + Y, R - Half + 1, 3); DrawRect(Black, Cx + Half - 1, Cy + Y, R - Half + 1, 3);
		}
		const FLinearColor Hair = PC->HitMarkerUntil > GetWorld()->GetTimeSeconds() ? FLinearColor::Yellow : Line;
		DrawLine(Cx - R, Cy, Cx - 10, Cy, Hair, 1.f); DrawLine(Cx + 10, Cy, Cx + R, Cy, Hair, 1.f);
		DrawLine(Cx, Cy - R, Cx, Cy - 10, Hair, 1.f); DrawLine(Cx, Cy + 10, Cx, Cy + R, Hair, 1.f);
		DrawLine(Cx - R, Cy, Cx - R * .35f, Cy, Hair, 4.f); DrawLine(Cx + R * .35f, Cy, Cx + R, Cy, Hair, 4.f);
		DrawLine(Cx, Cy + R * .35f, Cx, Cy + R, Hair, 4.f);
		DrawRect(Hair, Cx - 1, Cy - 1, 2, 2);
		DrawText(FString::Printf(TEXT("×%.0f"), ScopeUnit->AimMagnification()), FLinearColor(.94f, .97f, 1.f), Cx + R * .62f, Cy + R * .62f, GEngine->GetSmallFont(), 1.2f);
	}
	// Fog of war and sector grid on the field (tactical view), under every panel.
 // Build both HUD projections in one enumeration; retain no contact pointers after this draw.
 BattleSectorView::KnownEnemies(GetWorld(),FrameContacts,FrameVisibleContacts);
 ON_SCOPE_EXIT { FrameContacts.Reset(); FrameVisibleContacts.Reset(); };
	DrawFieldOverlay(PC);
	// Range rings of the selected (and seen enemy) squads, also under the panels.
	DrawRangeRings();
	// Squad type icons (zoomed out) and the last known place of enemy squads, also under the panels.
	DrawSquadIcons();
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
  Allies=Enemies=Spotted=0;
  for(const auto& P:GetWorld()->GetSubsystem<UBattleState>()->GetUnits()) if(P.Value.Team==0 && P.Value.IsAlive()) ++Allies;
  Enemies=Spotted=FrameVisibleContacts.Num();
 }
	const auto* Controlled=Cast<ABattleUnit>(PC->GetPawn());
	Label(PC->IsTactical() ? TEXT("作戦指揮") : Controlled && !Controlled->UsesThirdPersonView() ? TEXT("直接操作（一人称）") : TEXT("直接操作（三人称）"), W * .4f, 30, Mint, 1.2f);
	Label(FString::Printf(TEXT("味方 %02d ／ 敵 %02d ／ 発見中 %02d"), Allies, Enemies, Spotted), W * .4f, 54, White, .9f);
	Label(Mode->bCombatPaused ? TEXT("AI停止中 [P]") : TEXT("AI稼働中 [P]"), W - 165, 35, Mode->bCombatPaused ? FLinearColor::Yellow : Muted);
	// Setup before the start (-BattleSetupPhase=1, user decision 2026-11-22): the match waits paused at 0 s for the player's purchases and orders.
	if (const auto* SetupMatch = GetWorld()->GetSubsystem<UBattleMatch>(); SetupMatch && SetupMatch->bArea && UBattleMatch::SetupPhase() && Mode->bCombatPaused && SetupMatch->Elapsed <= 0.f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, .55f), W * .5f - 330, 120, 660, 54);
		Label(FString::Printf(TEXT("開始前の編成 ／ 資源 %d ／ 部隊 %d/%d"), SetupMatch->Credits[0], SetupMatch->ActiveSquads(0), BattleTactics::AreaHire().MaxActive), W * .5f - 318, 126, FLinearColor::Yellow, 1.f);
		Label(TEXT("B の購入画面で部隊を買い、前線軍・攻撃軍と命令を整えたら P で開始"), W * .5f - 318, 150, White, .85f);
	}
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
   const auto& I=BattleTactics::AreaIncome();
   int32 Gain=Match->AreaIncomeGain(0);
   if(I.Ramp>0)Gain=FMath::RoundToInt(Gain*(1.f+I.Ramp*Match->Elapsed/60.f));
   IncomeText=FString::Printf(TEXT(" (+%d / %.0fs)"),Gain,I.Seconds);
  }
  if(Match->bArea)Label(FString::Printf(TEXT("資源 %d%s ／ 本拠地の耐久 %.0f ／ 敵本拠地の耐久（最後に見た値） %s%s"),Match->Credits[0],*IncomeText,Match->Area.BaseHP[0],Match->Area.KnownEnemyBaseHP[0]<0?TEXT("不明"):*FString::Printf(TEXT("%.0f"),Match->Area.KnownEnemyBaseHP[0]),
   BattleTactics::AreaHire().bEnabled?*FString::Printf(TEXT(" ／ [2] 小隊を購入 %d（得点 %+d、%d/%d）"),BattleTactics::AreaHire().Cost,BattleSquadType::RifleMembers()-BattleTactics::AreaHire().Cost,Match->ActiveSquads(0),BattleTactics::AreaHire().MaxActive):TEXT("")),28,90,Mint,.9f);
  else if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch) Label(FString::Printf(TEXT("%s ／ 資源 %d ／ [1] %d [2] %d [3] %d ／ %d人"),Phase,Match->Credits[0],Match->CurrentSquadPrice(EBattleTraining::Recruit),Match->CurrentSquadPrice(EBattleTraining::Regular),Match->CurrentSquadPrice(EBattleTraining::Veteran),BattleSquadType::RifleMembers()),28,90,Mint,.9f);
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(*FString::Printf(TEXT("%d人 ／ 拠点3か所 ／ P：一時停止・再開"),50*BattleSquadType::RifleMembers()),28,90,Mint,.9f);
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
   // An objective counts by its worth (a depot of map F counts as two), like the end score.
   int32 OwnPoints=0,EnemyPoints=0;for(int32 I=0;I<Match->Objectives.Num();++I){const int32 O=Match->ObservedOwner(0,I);const int32 Worth=FMath::RoundToInt(Match->ObjectiveValue(I));OwnPoints+=O==0?Worth:0;EnemyPoints+=O==1?Worth:0;}
   DrawRect(Ink,576,112,260,51);
   Label(FString::Printf(TEXT("得点 %d ＝ 生存 %d ＋ 資源 %d ＋ 拠点 %d×20"),Living+Match->Credits[0]+OwnPoints*20,Living,Match->Credits[0],OwnPoints),28,141,Mint,.85f);
   Label(FString::Printf(TEXT("敵：拠点 %d×20（観測）"),EnemyPoints),588,119,Muted,.85f);
   Label(TEXT("＋ 生存・資源は不明"),588,141,Muted,.85f);
  }
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(FString::Printf(TEXT("占領8秒 ／ 毎秒1点 ／ 先に100点 ／ 制限 %.0f秒"),Limit),28,141,Muted,.85f);
		else Label(TEXT("占領8秒 ／ 5秒ごとに収入+20 ／ 補給1発につき1 ／ F8 保存 ／ F9 読込"), 28, 141, Muted, .85f);
		DrawRect(Ink, 16, 165, 560, 26);
  if(Match->bArea)Label(TEXT("本拠地：10秒で補充 ／ 兵1人につき資源1 ／ F8 保存 ／ F9 読込"),28,172,Mint,.85f);
  // Purchase reservations (-BattleHireReserve=1): the own list in order with each one's wait (Docs/design-front-strategy.md 6).
  if(Match->bArea&&UBattleMatch::HireReserveOn())
  {
   static const TCHAR* Short[]={TEXT("小銃"),TEXT("重装"),TEXT("斥候"),TEXT("対戦車"),TEXT("戦車"),TEXT("装甲車"),TEXT("迫撃砲"),TEXT("自走砲"),TEXT("外骨格"),TEXT("補給車")};
   const TArray<int32> Mine=Match->HireQueueOf(0);
   FString Text=FString::Printf(TEXT("予約 %d/%d"),Mine.Num(),UBattleMatch::HireQueueMax);
   for(int32 K=0;K<Mine.Num();++K)
   {
    const auto& Q=Match->Area.HireQueue[Mine[K]];FString Why;const int32 B=Match->HireBlock(0,uint8(Q.Type),Why);
    const TCHAR* Wait=B==0?TEXT("順番待ち"):Why.Contains(TEXT("resources"))?TEXT("資源待ち"):Why.Contains(TEXT("slot"))?TEXT("小隊の上限待ち"):TEXT("上限待ち");
    Text+=FString::Printf(TEXT(" ／ %d.%s（%s %.0f秒）"),K+1,Q.Type>=0&&Q.Type<int32(UE_ARRAY_COUNT(Short))?Short[Q.Type]:TEXT("?"),Wait,Match->Elapsed-Q.At);
   }
   if(Mine.Num())Text+=TEXT(" ／ Backspace 取消 ／ = 先頭へ");
   DrawRect(Ink,576,165,720,26);Label(Text,588,172,Mint,.8f);
  }
  else if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch)Label(TEXT("本拠地・保有拠点で補給：1発につき資源1 ／ F8 保存 ／ F9 読込 ／ 支援攻撃なし"),28,172,Mint,.85f);
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(TEXT("固定戦力 ／ 増援・補給・支援攻撃なし"),28,172,Mint,.85f);
		else Label(FString::Printf(TEXT("[B] 支援攻撃 400 ／ 残り%d ／ 再装填 %.0f秒 ／ 選択部隊が知る位置へ"), 3 - Match->SupportUsed[0], Match->SupportCooldown[0]), 28, 172, Mint, .85f);
  // Strategy band (Docs/design-strategy-ui.md 2.1, display only): the own AI's plan, for the squads the player delegated. Shown only
  // while the strategy layer runs for team 0 (-BattleStrategy>=2); the opponent's plan is never shown.
  if(Match->bArea&&UBattleMatch::StrategyOn(0,2))
  {
   const int32 Focus=Match->StrategyFocusPoint(0);
   const float Open=UBattleMatch::StrategyOpenSeconds()*Match->MatchTimeLimit()/600.f;
   FString Plan;
   if(Match->Elapsed<Open)Plan=FString::Printf(TEXT("AIの方針：展開中（集中先は %.0f秒後に決める）"),Open-Match->Elapsed);
   else if(Focus==INDEX_NONE)Plan=TEXT("AIの方針：展開（集中先の候補なし。見えている戦線が無い）");
   else
   {
    const float Left=Match->Area.StrategyFocusAt.IsValidIndex(0)?Match->Area.StrategyFocusAt[0]+UBattleMatch::StrategyFocusHoldSeconds()-Match->Elapsed:0.f;
    Plan=Left>0?FString::Printf(TEXT("AIの方針：集中 → 拠点%c（あと%.0f秒は変えない）"),TCHAR('A'+Focus),Left):FString::Printf(TEXT("AIの方針：集中 → 拠点%c（見直し待ち）"),TCHAR('A'+Focus));
   }
   FString Holds;int32 HoldBits=0;
   for(int32 I=0;I<Match->Objectives.Num()&&I<31;++I)if(Match->StrategyHolds(0,I)){HoldBits|=1<<I;Holds+=FString::Printf(TEXT("%s拠点%c"),Holds.IsEmpty()?TEXT(""):TEXT("・"),TCHAR('A'+I));}
   if(!Holds.IsEmpty())Plan+=FString::Printf(TEXT(" ／ 保持：%s"),*Holds);
   DrawRect(Ink,16,193,560,22);Label(Plan+TEXT("（委任した部隊）"),28,196,FLinearColor(1.f,.85f,.35f),.8f);
   // Notices when the plan changes (2.3): the reason is not stored, so the text says what changed.
   if(auto* NoticePC=Cast<ABattleController>(PlayerOwner))
   {
    if(ShownStrategyFocus!=-2&&Focus!=ShownStrategyFocus)
     NoticePC->PushNotice(6,Focus==INDEX_NONE?FString(TEXT("方針の変更：集中をやめた")):ShownStrategyFocus>=0?FString::Printf(TEXT("方針の変更：集中 拠点%c → 拠点%c"),TCHAR('A'+ShownStrategyFocus),TCHAR('A'+Focus)):FString::Printf(TEXT("方針：拠点%c に集中"),TCHAR('A'+Focus)),
      Focus!=INDEX_NONE?Match->Objectives[Focus].Position:Match->AreaHome(0));
    if(ShownStrategyHolds>=0)for(int32 I=0;I<Match->Objectives.Num()&&I<31;++I)
    {
     const bool bNow=(HoldBits>>I)&1,bBefore=(ShownStrategyHolds>>I)&1;
     if(bNow!=bBefore)NoticePC->PushNotice(6,bNow?FString::Printf(TEXT("拠点%c を保持（敵が優勢）"),TCHAR('A'+I)):FString::Printf(TEXT("拠点%c の保持を解除"),TCHAR('A'+I)),Match->Objectives[I].Position);
    }
   }
   ShownStrategyFocus=Focus;ShownStrategyHolds=HoldBits;
  }
  // Front plan band (stage 7, -BattleFrontPlanPlayer=1): the own plan by section, the player's choices included. The enemy's is never shown.
  if(Match->bArea&&Match->PlayerRolesOn(0))
  {
   const auto& BandFP=Match->FrontPlanState[0];
   const int32 BandFocus=Match->StrategyFocusPoint(0);
   FString BandPlan=BandFocus!=INDEX_NONE?FString::Printf(TEXT("自軍の方針：重点 → 拠点%c"),TCHAR('A'+BandFocus)):FString(TEXT("自軍の方針：重点なし"));
   if(Match->Objectives.IsValidIndex(BandFP.Focus2))BandPlan+=FString::Printf(TEXT("・拠点%c"),TCHAR('A'+BandFP.Focus2));
   FString BandHolds,BandBacks;
   for(int32 BandK=0;BandK<BandFP.Role.Num();++BandK)
   {
    if(BandFP.Role[BandK]==TEXT('H'))BandHolds+=FString::Printf(TEXT("%s%d"),BandHolds.IsEmpty()?TEXT(""):TEXT("・"),BandK+1);
    if(BandFP.Role[BandK]==TEXT('R'))BandBacks+=FString::Printf(TEXT("%s%d"),BandBacks.IsEmpty()?TEXT(""):TEXT("・"),BandK+1);
   }
   if(!BandHolds.IsEmpty())BandPlan+=FString::Printf(TEXT(" ／ 保持：区間%s"),*BandHolds);
   if(!BandBacks.IsEmpty())BandPlan+=FString::Printf(TEXT(" ／ 下がってよい：区間%s"),*BandBacks);
   int32 Chosen=0;for(const uint8 PickRole:Match->Area.PlayerSectionRole)Chosen+=PickRole!=0;
   BandPlan+=Chosen?FString::Printf(TEXT(" ／ 指定 %d区間 ／ [U] 作戦方針"),Chosen):FString(TEXT(" ／ [U] 作戦方針"));
   DrawRect(Ink,16,193,560,22);Label(BandPlan,28,196,FLinearColor(1.f,.85f,.35f),.8f);
  }
		for (const auto& Strike : Match->Strikes)
		{
			FVector2D Point;
			if (PC->ProjectWorldLocationToScreen(Strike.Position + FVector(0, 0, 100), Point) && Point.Y > 200 && Point.Y < (PC->IsTactical()?PanelTop-7.f:H-135.f))
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
			if (PC->ProjectWorldLocationToScreen(Point.Position + FVector(0, 0, 45), Center) && Center.Y > 160 && Center.Y < (PC->IsTactical()?PanelTop-7.f:H-135.f))
			{
				DrawRect(Ink, Center.X - 48, Center.Y - 18, 170, 24);
    const TCHAR* Kind=Match->ObjectiveValue(Index)>1.f?TEXT("（集積所）"):TEXT("");
    Label(Match->bArea?(Point.Owner==-2?FString::Printf(TEXT("%c%s ／ 不明"),'A'+Index,Kind):FString::Printf(TEXT("%c%s ／ %s ／ %.0f秒前に確認"),'A'+Index,Kind,Point.Owner==0?TEXT("味方"):Point.Owner==1?TEXT("敵"):TEXT("中立"),Match->Area.SeenAt[Index])):FString::Printf(TEXT("%c / %s %.0f%%"), 'A' + Index, Point.bContested ? TEXT("争奪中") : Point.Owner == 0 ? TEXT("味方") : Point.Owner == 1 ? TEXT("敵") : TEXT("中立"), Point.CaptureProgress / 8.f * 100.f), Center.X - 42, Center.Y - 12, Color, .85f);
			}
			for (int32 Segment = 0; Segment < 32; ++Segment)
			{
				const float A = Segment * 2.f * PI / 32, B = (Segment + 1) * 2.f * PI / 32;
				FVector2D P1, P2;
				if (PC->ProjectWorldLocationToScreen(Point.Position + FVector(FMath::Cos(A) * 450, FMath::Sin(A) * 450, 12), P1) && PC->ProjectWorldLocationToScreen(Point.Position + FVector(FMath::Cos(B) * 450, FMath::Sin(B) * 450, 12), P2) && P1.Y > 170 && P2.Y > 170 && P1.Y < (PC->IsTactical()?PanelTop-7.f:H-135.f) && P2.Y < (PC->IsTactical()?PanelTop-7.f:H-135.f)) DrawLine(P1.X, P1.Y, P2.X, P2.Y, Color, 2);
			}
		}
	}

 if(Match->bArea)for(int32 Team=0;Team<2;++Team)
 {
  FVector2D P;if(PC->ProjectWorldLocationToScreen(Match->AreaHome(Team),P)&&P.Y>200&&P.Y<PanelTop-7.f)
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
		if (Screen.X < 20 || Screen.X > W - 20 || Screen.Y < 90 || Screen.Y > (PC->IsTactical()?PanelTop-2.f:H-130.f)) continue;
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
		// Squad icons (DrawSquadIcons) take over when the camera is far: a small dot per soldier instead of the name and bars.
		static const bool bSquadIcons = [] { int32 On = 1; FParse::Value(FCommandLine::Get(), TEXT("BattleSquadIcons="), On); return On != 0; }();
		if (bDrawLabel && PC->IsTactical() && bSquadIcons && PC->PlayerCameraManager && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Unit->GetActorLocation()) > 8000.f)
		{ bDrawLabel = false; DrawRect(Color, Screen.X - 2, Screen.Y, 4, 4); }
		if (bDrawLabel)
		{
			LabelPositions.Add(Screen);
			DrawRect(Ink, Screen.X - 25, Screen.Y, 50, 5);
			DrawRect(Color, Screen.X - 25, Screen.Y, 50 * FMath::Clamp(State->HP / FMath::Max(1.f, GetWorld()->GetSubsystem<UBattleState>()->MaxHealthOf(State->UnitID)), 0.f, 1.f), 5);
			// Exoskeleton shield: a thin light-blue bar above the health bar.
			if (const float MaxShield = GetWorld()->GetSubsystem<UBattleState>()->MaxShieldOf(State->UnitID); MaxShield > 0.f)
			{ DrawRect(Ink, Screen.X - 25, Screen.Y - 4, 50, 3); DrawRect(FLinearColor(.35f, .8f, 1.f), Screen.X - 25, Screen.Y - 4, 50 * FMath::Clamp(State->Shield / MaxShield, 0.f, 1.f), 3); }
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
	DrawRect(Ink, 16, PanelTop, W - 32, H-16-PanelTop);
	const ABattleUnit* Focus = PC->IsTactical() ? (PC->GetSelection().IsEmpty() ? nullptr : PC->GetSelection()[0].Get()) : Cast<ABattleUnit>(PC->GetPawn());
	if (Focus && Focus->GetState())
	{
		const FBattleUnitState* S = Focus->GetState();
		Label(FString::Printf(TEXT("%s %02d ／ 部隊%02d   HP %.0f%s   弾 %02d / %02d   %s ／ %s   再試行 %d%s"), S->bLauncherSelected && !PC->IsTactical() ? TEXT("バズーカ") : S->Weapon == EBattleWeapon::Mortar ? TEXT("迫撃砲") : S->Weapon == EBattleWeapon::AntiTank ? TEXT("対戦車火器") : S->Weapon == EBattleWeapon::SupportGun ? TEXT("支援火器") : TEXT("小銃"), S->UnitID, S->SquadID, S->HP, GetWorld()->GetSubsystem<UBattleState>()->MaxShieldOf(S->UnitID) > 0.f ? *FString::Printf(TEXT(" ＋盾 %.0f   対戦車弾 %d%s%s"), S->Shield, S->Launcher, S->LauncherWait > 0.f ? TEXT("（装填中）") : TEXT(""), PC->IsTactical() ? TEXT("") : PC->bGamepadActive ? TEXT(" [LBで切替]") : TEXT(" [Fで切替]")) : TEXT(""), S->Ammo, S->Reserve,
			*BattleUIText::Ja(BattleOrderName(S->Order)), *BattleUIText::Ja(BattleActionName(S->Action)), S->MoveRetries, S->ReloadEndTime > 0 ? TEXT("   装填中…") : TEXT("")), 34, PC->IsTactical()?BottomBar.SummaryY:H-112, S->Action == EBattleAction::MoveFailed ? FLinearColor::Yellow : White, 1.05f);
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
		Label(FString::Printf(TEXT("照準 %.1f秒   拡散 %.2f度   %s"), FMath::Min(Steady, GetDefault<UBattleTuning>()->AimSettleSeconds), BattleTactics::SpreadDegrees(*S, GetWorld()->GetTimeSeconds(), SuppressedScale * Focus->PlayerSpreadScale()), *CoverText), W - 566, 148, White, .85f);
		DrawRect(Ink, W - 576, 170, 560, 27);
		Label(S->DecisionReason.IsEmpty() ? TEXT("方針：命令に従う") : FString::Printf(TEXT("方針：%s ／ 位置 %d"), *BattleUIText::Ja(S->DecisionReason), S->CoverSlot), W - 566, 177, Mint, .85f);
		DrawRect(Ink, W - 576, 199, 560, 27);
		const FString RoleText = S->Role == EBattleRole::Independent ? TEXT("単独") : FString::Printf(TEXT("%s ／ 集中 %02d"), BattleRoleName(S->Role), S->CoordinatedTarget);
		Label(FString::Printf(TEXT("%s ／ 試験費用 %d ／ 役割：%s"), BattleTrainingName(S->Training), S->HireCost, *RoleText), W - 566, 206, White, .85f);
		if(const auto* Squad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(S->SquadID);Squad && Squad->bSectorMission && Squad->TacticalPlan.Action==EBattleSquadTactic::Mission)
        {
            const auto Cell=BattleSectors::Cell(Squad->Destination);
            int Living=0,Present=0;
            for(int ID:Squad->Members)if(GetWorld()->GetSubsystem<UBattleState>()->Find(ID)->IsAlive())
            {++Living;if(BattleSectors::Cell(Leaders->Position(ID))==Cell)++Present;}
            const TCHAR* State=Leaders->SquadTargetID(S->SquadID)!=INDEX_NONE?TEXT("交戦中"):Present*2>Living?TEXT("維持中"):TEXT("展開中");
            Label(FString::Printf(TEXT("%s %c%d　%s　%d/%d"),Leaders->HasSectorFireBack(S->SquadID)?TEXT("区画移動"):TEXT("区画"),'A'+Cell.X,1+Cell.Y,State,Present,Living),W-566,234,Mint,.85f);
            if(PC->IsTactical()){
                const FVector C=BattleSectors::Center(Cell);
                const FVector Corners[]={C+FVector(-2500,-2500,10),C+FVector(2500,-2500,10),C+FVector(2500,2500,10),C+FVector(-2500,2500,10)};
                for(int I=0;I<4;++I){FVector2D A,B;if(PC->ProjectWorldLocationToScreen(Corners[I],A)&&PC->ProjectWorldLocationToScreen(Corners[(I+1)%4],B))DrawLine(A.X,A.Y,B.X,B.Y,Mint,2);}
            }
        }
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
	else Label(TEXT("味方の部隊を選んで開始"), 34, PC->IsTactical()?BottomBar.SummaryY:H-112, White, 1.2f);
	if (PC->IsTactical())
	{
		// Command bar (command UI 2026-11-16): buttons call the same actions as the keys shown in brackets.
		{
			const auto& Bar = BottomBar;
			const bool bAny = !PC->GetSelection().IsEmpty();
			for (int32 I = 0; I < FBattleCommandBarLayout::Count; ++I)
			{
				const auto& Button = Bar.Buttons[I];
				const bool bUsable = I >= 6 ? (Match->bArea && BattleTactics::AreaHire().bEnabled) : bAny;
				const bool bPending = (I == 1 && PC->PendingOrder == EBattleOrder::AttackMove) || (I == 2 && PC->PendingOrder == EBattleOrder::Defend) || (I == 6 && PC->IsBuyPanelOpen());
				DrawRect(bPending ? FLinearColor(.45f, .4f, .1f, .98f) : bUsable ? FLinearColor(.10f, .24f, .24f, .98f) : FLinearColor(.08f, .11f, .15f, .98f), Button.Origin.X, Button.Origin.Y, Button.Width, Bar.ButtonHeight);
                Label(Button.Caption, Button.Origin.X+8, Button.Origin.Y+5, bPending ? FLinearColor::Yellow : bUsable ? Mint : Muted, Button.Scale);
			}
			// Route of the selected squads (maps with a road network only): its own button at the right end of the panel's top row,
			// apart from the bar. Shows the current choice, lit when one is set.
			if (Match->RoutesOn())
			{
				const int32 RouteLane = PC->SelectedRouteLane();
				const bool bLit = bAny && RouteLane != 0;
				DrawRect(bLit ? FLinearColor(.45f, .4f, .1f, .98f) : bAny ? FLinearColor(.10f, .24f, .24f, .98f) : FLinearColor(.08f, .11f, .15f, .98f), Bar.RouteOrigin.X, Bar.RouteOrigin.Y, Bar.RouteWidth, Bar.ButtonHeight);
				Label(FString::Printf(TEXT("通り道：%s%s"), !bAny ? TEXT("―") : RouteLane < 0 ? TEXT("混在") : BattleRouteLaneName(RouteLane), PC->bGamepadActive ? TEXT("") : TEXT(" [Z]")), Bar.RouteOrigin.X + 8, Bar.RouteOrigin.Y + 5, bLit ? FLinearColor::Yellow : bAny ? Mint : Muted, .85f);
			}
		}
		if (PC->bGamepadActive) Label(TEXT("右スティック：カーソル   A：選択・決定   左スティック：視点移動   LT/RT：拡大縮小   LB：全選択   ビュー：直接操作   メニュー：AI一時停止   十字←：購入   十字→：速度"), 34, H - 53, Muted, .9f);
		else
		Label(FString::Printf(TEXT("左ドラッグ：選択   Q：全選択   E/Tab：直接操作   WASD：視点移動   ホイール：拡大縮小   P：AI一時停止   F5：やり直し   F6：記憶表示   %s   %s   L：射程の円（%s）      選択中 %02d"), Match->IsEnabled() ? (Match->bArea ? TEXT("B：部隊の購入") : TEXT("1〜3：購入")) : TEXT("F7：練度の切替"), BattleFieldLayout::Of(GetWorld()).bSectors ? TEXT("M：区画の格子   N：霧") : TEXT("N：霧"), ABattleController::RangeRingModeName(PC->RangeRingMode), PC->GetSelection().Num()), 34, H - 53, Muted, .9f);
	}
	else
	{
		if (PC->bGamepadActive) Label(TEXT("左スティック：移動   右スティック：照準（補助あり）   RT：射撃   LT：構える（十字↑↓：倍率）   X：装填   A：ジャンプ   B：姿勢   Y：遮蔽   ビュー：作戦画面"), 34, H - 80, Mint);
		else
		Label(TEXT("WASD：移動   マウス：照準   左クリック：射撃   右クリック：構える（＋ホイール：スコープ）   R：装填   Space：ジャンプ   Tab：作戦画面"), 34, H - 80, Mint);
		Label(TEXT("P：AI一時停止   F5：戦闘をやり直す   ｜   操作を切り替えても、兵・HP・弾薬・命令は引き継がれる"), 34, H - 53, Muted, .9f);
		const FLinearColor CrossColor = PC->HitMarkerUntil > GetWorld()->GetTimeSeconds() ? FLinearColor::Yellow : White;
		// Aiming tightens the player's shots: say so next to the crosshair while it applies.
		if (ScopeUnit && ScopeUnit->AimSpreadScale() < 1.f) Label(FString::Printf(TEXT("構え：散らばり ×%.1f"), ScopeUnit->AimSpreadScale()), W / 2 + 26, H / 2 + 18, Mint, .9f);
		if (!bScope) {
		DrawLine(W / 2 - 13, H / 2, W / 2 - 4, H / 2, CrossColor, 1.5f);
		DrawLine(W / 2 + 4, H / 2, W / 2 + 13, H / 2, CrossColor, 1.5f);
		DrawLine(W / 2, H / 2 - 13, W / 2, H / 2 - 4, CrossColor, 1.5f);
		DrawLine(W / 2, H / 2 + 4, W / 2, H / 2 + 13, CrossColor, 1.5f);
		}
	}
	DrawHitDirection(PC);
	DrawOffscreenMarkers(PC);
	DrawKillLog(PC);
	DrawSoldierOrders();
	if (PC->NoticeUntil > GetWorld()->GetTimeSeconds())
 {
  const FString NoticeText=BattleUIText::Ja(PC->Notice);float NoticeWidth=0,NoticeHeight=0;
  Canvas->StrLen(GEngine->GetSmallFont(),NoticeText,NoticeWidth,NoticeHeight);
  const float NoticeScale=PC->CanShowDelegation()?FMath::Min(1.1f,float(BottomBar.RouteOrigin.X-47.f)/FMath::Max(1.f,NoticeWidth)):1.1f;
  Label(NoticeText,35,PC->CanShowDelegation()?BottomBar.RouteOrigin.Y+5.f:Match->IsEnabled()?235:96,Mint,NoticeScale);
 }
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
 DrawOrderWays();
 DrawNoticeLog();
 DrawSectorPanel(PC);
 DrawPlanPanel(PC);
 DrawBuyPanel(PC);
 DrawGamepadMenu(PC);
}

void ABattleHUD::DrawPlanPanel(ABattleController* PC)
{
 if(!PC||!Canvas||!PC->IsTactical()||!PC->IsPlanPanelOpen())return;
 const auto L=FBattlePlanPanelLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld());
 if(L.Items.IsEmpty())return;
 const FLinearColor Ink(.025f,.04f,.065f,.95f),Mint(.28f,.95f,.8f),Muted(.62f,.7f,.77f),White(.94f,.97f,1.f),Gold(1.f,.85f,.35f);
 DrawRect(Ink,L.Origin.X,L.Origin.Y,L.Size.X,L.Size.Y);DrawRect(Gold,L.Origin.X,L.Origin.Y,L.Size.X,2);
 for(const auto& I:L.Items)
 {
  if(I.Kind==0){DrawText(I.Text,I.Tone==4?Gold:I.Tone==5?Muted:White,I.Origin.X,I.Origin.Y,GEngine->GetSmallFont(),I.Tone==4?1.f:.8f);continue;}
  const FLinearColor Fill=I.Tone==1?FLinearColor(.10f,.24f,.24f,.98f):I.Tone==2?FLinearColor(.25f,.08f,.08f,.98f):I.Tone==3?FLinearColor(.45f,.4f,.1f,.98f):FLinearColor(.08f,.11f,.15f,.98f);
  DrawRect(Fill,I.Origin.X,I.Origin.Y,I.Size.X,I.Size.Y);
  DrawText(I.Text,I.Tone==2?Muted:I.Tone==3?FLinearColor::Yellow:Mint,I.Origin.X+6,I.Origin.Y+3,GEngine->GetSmallFont(),.8f);
 }
}

void ABattleHUD::DrawBuyPanel(ABattleController* PC)
{
 if(!PC||!Canvas||!PC->IsBuyPanelOpen())return;
 const auto L=FBattleBuyPanelLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld(),PC->bGamepadActive);
 if(L.Rows.IsEmpty())return;
 const FLinearColor Ink(.025f,.04f,.065f,.96f),Mint(.28f,.95f,.8f),Muted(.62f,.7f,.77f),White(.94f,.97f,1.f),Gold(1.f,.85f,.35f),Wait(1.f,.66f,.32f);
 // Text shrunk to its column when it would run into the next one.
 auto Cell=[&](const FString& Text,FLinearColor Color,float X,float Y,float MaxWidth,float Scale)
 {float TextWidth=0,TextHeight=0;Canvas->StrLen(GEngine->GetSmallFont(),Text,TextWidth,TextHeight);DrawText(Text,Color,X,Y,GEngine->GetSmallFont(),FMath::Min(Scale,FMath::Max(1.f,MaxWidth)/FMath::Max(1.f,TextWidth)));};
 DrawRect(Ink,L.Origin.X,L.Origin.Y,L.Size.X,L.Size.Y);DrawRect(Gold,L.Origin.X,L.Origin.Y,L.Size.X,2);
 Cell(L.Title,Gold,L.Origin.X+12,L.Origin.Y+9,L.Size.X-36-L.CloseSize,1.f);
 DrawRect(FLinearColor(.08f,.11f,.15f,.98f),L.CloseOrigin.X,L.CloseOrigin.Y,L.CloseSize,L.CloseSize);
 DrawText(TEXT("×"),Mint,L.CloseOrigin.X+7,L.CloseOrigin.Y+4,GEngine->GetSmallFont(),.9f);
 const float X0=L.Origin.X,StateX=L.Origin.X+L.Size.X-12-L.StateWidth;
 const float Cols[]={X0+L.ColKey,X0+L.ColName,X0+L.ColMembers,X0+L.ColCost,X0+L.ColOwned,X0+L.ColRole,StateX};
 const float Ends[]={X0+L.ColName,X0+L.ColMembers,X0+L.ColCost,X0+L.ColOwned,X0+L.ColRole,StateX,L.Origin.X+L.Size.X-12};
 const TCHAR* Names[]={TEXT("キー"),TEXT("兵種"),TEXT("人数"),TEXT("費用"),TEXT("保有"),TEXT("役割"),TEXT("状態")};
 for(int32 C=0;C<7;++C)Cell(Names[C],Muted,Cols[C],L.HeaderY,Ends[C]-Cols[C]-6,.75f);
 // Highlight: the pad's chosen row, or the row under the mouse pointer.
 int32 Lit=INDEX_NONE;
 if(PC->bGamepadActive)Lit=PC->GetBuyPanelRow();
 else if(FVector2D P;PC->GetPointerPosition(P))Lit=L.RowAt(P);
 for(int32 I=0;I<L.Rows.Num();++I)
 {
  const auto& R=L.Rows[I];const bool bLit=I==Lit,bOk=R.Block==0;
  const FLinearColor Fill=bLit?(bOk?FLinearColor(.16f,.38f,.36f,.98f):FLinearColor(.20f,.20f,.24f,.98f)):bOk?FLinearColor(.10f,.24f,.24f,.98f):FLinearColor(.07f,.09f,.12f,.98f);
  DrawRect(Fill,R.Origin.X,R.Origin.Y,R.Size.X,R.Size.Y);
  if(bLit)DrawRect(Gold,R.Origin.X,R.Origin.Y,3,R.Size.Y);
  const float TY=R.Origin.Y+5;const FLinearColor Main=bOk?White:Muted;
  const FString Key=FString::Printf(TEXT("[%s]"),*R.Key);
  const FString* Texts[]={&Key,&R.Name,&R.Members,&R.Cost,&R.Owned,&R.Role,&R.State};
  for(int32 C=0;C<7;++C)
   Cell(*Texts[C],C==6?(bOk?Mint:R.Block==1?Wait:Muted):C==5?Muted:C==0?(bOk?Mint:Muted):Main,Cols[C],TY,Ends[C]-Cols[C]-6,C==1?.85f:.8f);
 }
 Cell(L.Footer,Muted,L.Origin.X+12,L.FooterY,L.Size.X-24,.75f);
}

void ABattleHUD::DrawGamepadMenu(ABattleController* PC)
{
 if(!PC->IsTactical()||(!PC->bGamepadActive&&!PC->IsGamepadMenuOpen()&&!PC->IsGroundFloorPending()))return;
 const auto MenuBar=FBattleCommandBarLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld(),true);
 const float MenuWidth=FMath::Min(360.f,FMath::Max(1.f,float(Canvas->SizeX)-32.f));
 const float MenuRowHeight=FMath::Max(26.f,MenuBar.ButtonHeight);
 const float MenuHeight=PC->IsGamepadMenuOpen()?MenuRowHeight*8.f+16.f:MenuRowHeight+12.f;
 const float MenuX=(float(Canvas->SizeX)-MenuWidth)*.5f, MenuY=FMath::Max(16.f,MenuBar.PanelTop-MenuHeight-8.f);
 const FLinearColor MenuInk(.025f,.04f,.065f,.98f),MenuMint(.28f,.95f,.8f),MenuWhite(.94f,.97f,1.f);
 DrawRect(MenuInk,MenuX,MenuY,MenuWidth,MenuHeight);
 auto MenuLabel=[&](const FString& MenuText,float MenuTextY,FLinearColor MenuColor)
 {
  float MenuTextWidth=0,MenuTextHeight=0;
  Canvas->StrLen(GEngine->GetSmallFont(),MenuText,MenuTextWidth,MenuTextHeight);
  const float MenuScale=FMath::Min(.85f,(MenuWidth-24.f)/FMath::Max(1.f,MenuTextWidth));
  DrawText(MenuText,MenuColor,MenuX+12.f,MenuTextY,GEngine->GetSmallFont(),MenuScale);
 };
 if(!PC->IsGamepadMenuOpen())
 {
  MenuLabel(PC->IsGroundFloorPending()?TEXT("次の命令：1階 ／ 左スティック押込：メニュー"):TEXT("左スティック押込：群・保存メニュー"),MenuY+6.f,MenuMint);return;
 }
 MenuLabel(TEXT("作戦メニュー ／ 左スティック押込で閉じる"),MenuY+6.f,MenuMint);
 const FString MenuEntries[]={TEXT("群を編成／解散 (F)"),TEXT("群を選択 (V)"),TEXT("クイック保存 (F8)"),TEXT("クイック読込 (F9)"),
  PC->IsGroundFloorPending()?TEXT("1階を指定：入（次の命令／Alt）"):TEXT("1階を指定：切（次の命令／Alt）"),TEXT("閉じる")};
 for(int32 MenuIndex=0;MenuIndex<UE_ARRAY_COUNT(MenuEntries);++MenuIndex)
 {
  const float MenuEntryY=MenuY+8.f+MenuRowHeight*float(MenuIndex+1);
  const bool bMenuSelected=MenuIndex==PC->GetGamepadMenuRow();
  if(bMenuSelected)DrawRect(FLinearColor(.10f,.24f,.24f,.98f),MenuX+6.f,MenuEntryY,MenuWidth-12.f,MenuRowHeight-2.f);
  MenuLabel(MenuEntries[MenuIndex],MenuEntryY+4.f,bMenuSelected?MenuMint:MenuWhite);
 }
 MenuLabel(TEXT("十字↑↓：選択  A：決定  B：閉じる"),MenuY+8.f+MenuRowHeight*7.f,MenuWhite);
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
 // Sector maps: impassable mountains (brown) and lakes (blue) - public map knowledge (map J, 2026-11-22).
 if(BattleFieldLayout::Of(GetWorld()).bSectors)
  for(int32 SX=0;SX<BattleSectors::Columns;++SX)for(int32 SY=0;SY<BattleSectors::Rows;++SY)
  {
   const char Terrain=BattleFieldLayout::SectorKind(GetWorld(),FIntPoint(SX,SY));if(Terrain!='M'&&Terrain!='L')continue;
   const FVector Mid=BattleSectors::Center(FIntPoint(SX,SY));const double HalfCell=BattleSectors::Size*.5;
   const FVector2D A=Map.ToScreen(Mid+FVector(HalfCell,-HalfCell,0)),B=Map.ToScreen(Mid+FVector(-HalfCell,HalfCell,0));
   DrawRect(Terrain=='M'?FLinearColor(.24f,.2f,.15f,.95f):FLinearColor(.08f,.2f,.38f,.95f),FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y),FMath::Abs(B.X-A.X),FMath::Abs(B.Y-A.Y));
  }
 // Map E: the city block outline (public map knowledge).
 if(const FVector2D City=BattleFieldLayout::Of(GetWorld()).CityHalf;City.X>0) Frame(Map.ToScreen(FVector(City.X,-City.Y,0)),Map.ToScreen(FVector(-City.X,City.Y,0)),Muted);
 // Roads of the map (public map knowledge): the routes squads follow. The three main routes are brighter than the connectors.
 {
  const auto& Def=BattleFieldLayout::Of(GetWorld());const FLinearColor Road(.3f,.36f,.42f,.9f),Side(.2f,.25f,.3f,.8f);
  for(const auto& E:Def.RouteEdges)
  {const FVector2D A=Map.ToScreen(Def.RouteNodes[E.A]),B=Map.ToScreen(Def.RouteNodes[E.B]);DrawLine(A.X,A.Y,B.X,B.Y,E.Lane==BattleFieldLayout::ERouteLane::Connector?Side:Road,1);}
  // The routes chosen for the selected squads (BattlePlayerRoutes.cpp) are drawn over the roads.
  if(!Def.RouteEdges.IsEmpty())
  {
   bool bChosen[4]={false,false,false,false};const auto* Routes=GetWorld()->GetSubsystem<UBattleMatch>();
   for(const ABattleUnit* U:PC->GetSelection())if(U&&U->IsAlive()&&U->GetState()->Team==0)bChosen[FMath::Clamp(Routes->PlayerRouteLane(U->GetState()->SquadID),0,3)]=true;
   for(const auto& E:Def.RouteEdges)if(E.Lane!=BattleFieldLayout::ERouteLane::Connector&&bChosen[int32(E.Lane)+1])
   {const FVector2D A=Map.ToScreen(Def.RouteNodes[E.A]),B=Map.ToScreen(Def.RouteNodes[E.B]);DrawLine(A.X,A.Y,B.X,B.Y,FLinearColor(1.f,.85f,.2f),2);}
  }
 }
 DrawMinimapOverlay(PC,Map);
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 if(Match->bArea&&BattleTactics::ArtilleryPosts())
 {
  for(int32 MarkerTeam=0;MarkerTeam<2;++MarkerTeam)for(const FVector& MarkerPost:Match->ArtilleryPostPositions[MarkerTeam])Dot(MarkerPost,4,Muted);
  DrawText(TEXT("砲撃陣地：灰点"),Muted,Map.Origin.X+150,Map.Origin.Y-20,GEngine->GetSmallFont(),.7f);
  // Public posts and fresh team reports; never look up the shooter's current state.
  for(const auto& MarkerFlash:Match->FireReveals[0])if(MarkerFlash.Value.At<=Match->Elapsed&&Match->Elapsed-MarkerFlash.Value.At<MarkerFlash.Value.Memory)Dot(MarkerFlash.Value.Position,4,Stale);
 }
 // Homes are public; objective ownership is what the player's side last observed.
 if(Match->bArea) for(int32 Team=0;Team<2;++Team)
 {const FVector2D P=Map.ToScreen(Match->AreaHome(Team));Frame(P-FVector2D(7,7),P+FVector2D(7,7),Team?Enemy:Mint);}
 for(int32 Index=0;Index<Match->Objectives.Num();++Index)
 {
  const int32 Holder=Match->bArea?Match->ObservedOwner(0,Index):Match->Objectives[Index].Owner;
  const FLinearColor Color=Holder==0?Mint:Holder==1?Enemy:Holder==-2?Muted:White;
  const FVector2D P=Map.ToScreen(Match->Objectives[Index].Position);
  DrawRect(Color,P.X-5,P.Y-5,10,10);
  // An objective worth more than the others (a depot) gets a frame around its square.
  if(Match->ObjectiveValue(Index)>1.f)Frame(P-FVector2D(8,8),P+FVector2D(8,8),Color);
  DrawText(FString::Printf(TEXT("%c"),'A'+Index),Color,P.X+7,P.Y-7,GEngine->GetSmallFont(),.8f);
 }
 // Lookouts (public map places): a small diamond.
 for(const FVector& Look:BattleFieldLayout::Lookouts(GetWorld()))
 {const FVector2D P=Map.ToScreen(Look);DrawLine(P.X,P.Y-6,P.X+6,P.Y,White,1);DrawLine(P.X+6,P.Y,P.X,P.Y+6,White,1);DrawLine(P.X,P.Y+6,P.X-6,P.Y,White,1);DrawLine(P.X-6,P.Y,P.X,P.Y-6,White,1);}
 // Enemies: only contacts held by the player's side. Seen now = solid; remembered = dim, at the last known position.
 const auto& Known=FrameContacts;
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
 // Range rings of the selected squads, under the squad marks.
 DrawMinimapRangeRings(PC,Map);
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
 PC->ScrollSquadList(0); const auto Layout=FBattleSquadListLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld());
 if(Layout.VisibleRows<=0) return;
 const auto& All=L->Summaries(); int32 Active=0; for(const auto& S:All) if(S.bActive) ++Active;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();
 // Rows of the chosen tab (groups / ungrouped / stopped / all; command UI 2026-11-20) and the counts for the tab labels.
 // Army tabs (2026-11-21): squads in the front army, the attack army, unassigned, and every active squad. Stopped squads are not listed.
 const auto Rows=PC->SquadListRows(); int32 Counts[FBattleSquadListLayout::FilterCount]={0,0,0,Active};
 for(const auto& S:All) if(const uint8 C=PC->SquadCategory(S);C<3) ++Counts[C];
 const int32 Dragged=PC->DraggedSquadID();
 DrawRect(FLinearColor(.025f,.04f,.065f,.96f),Layout.Origin.X,Layout.Origin.Y,Layout.Width,Layout.Height);
 DrawText(FString::Printf(TEXT("自軍部隊　稼働 %d"),Active),FLinearColor::White,Layout.Origin.X+8,Layout.Origin.Y+5,GEngine->GetSmallFont(),1);
 DrawText(TEXT("タブ・群へドラッグで編入・Shift+なぞりで複数選択・Ctrl+右ドラッグで前線を引く"),FLinearColor(.6f,.65f,.7f),Layout.Origin.X+8,Layout.Origin.Y+Layout.Height-18,GEngine->GetSmallFont(),.7f);
 static const TCHAR* const FilterNames[]={TEXT("前線軍"),TEXT("攻撃軍"),TEXT("未所属"),TEXT("全部")};
 for(int32 T=0;T<FBattleSquadListLayout::FilterCount;++T)
 {
  const float X=Layout.Origin.X+6+T*FBattleSquadListLayout::TabWidth,Y=Layout.Origin.Y+FBattleSquadListLayout::TabTop; const bool On=PC->SquadFilter==T;
  DrawRect(On?FLinearColor(.12f,.38f,.36f):FLinearColor(.08f,.11f,.15f),X,Y,FBattleSquadListLayout::TabWidth-4,FBattleSquadListLayout::TabHeight);
  // While a squad is dragged, the three army tabs are drop targets.
  if(Dragged!=INDEX_NONE && T<3) DrawRect(FLinearColor(1.f,.85f,.35f,.35f),X,Y+FBattleSquadListLayout::TabHeight-3,FBattleSquadListLayout::TabWidth-4,3);
  DrawText(FString::Printf(TEXT("%s %d"),FilterNames[T],Counts[T]),On?FLinearColor::White:FLinearColor(.7f,.75f,.8f),X+4,Y+2,GEngine->GetSmallFont(),.75f);
 }
 static const TCHAR* const SortNames[]={TEXT("並び：番号順"),TEXT("並び：損耗順"),TEXT("並び：拠点別")};
 {
  const float X=Layout.Origin.X+Layout.Width-FBattleSquadListLayout::SortWidth-8,Y=Layout.Origin.Y+FBattleSquadListLayout::TabTop;
  DrawRect(FLinearColor(.08f,.11f,.15f),X,Y,FBattleSquadListLayout::SortWidth,FBattleSquadListLayout::TabHeight);
  DrawText(SortNames[FMath::Min<int32>(PC->SquadSort,2)],FLinearColor(.7f,.75f,.8f),X+4,Y+2,GEngine->GetSmallFont(),.75f);
 }
 static const TCHAR* const EmptyText[]={TEXT("前線軍はありません（部隊をこのタブへドラッグ）"),TEXT("攻撃軍はありません（部隊をこのタブへドラッグ）"),TEXT("未所属の部隊はありません"),TEXT("稼働中の部隊はありません")};
 if(Rows.IsEmpty())
  DrawText(EmptyText[FMath::Min<int32>(PC->SquadFilter,3)],FLinearColor(.6f,.65f,.7f),Layout.Origin.X+12,Layout.Origin.Y+Layout.Header+8,GEngine->GetSmallFont(),.85f);
 for(int32 I=0;I<Layout.VisibleRows && I+PC->SquadListScroll<Rows.Num();++I)
 {
  const auto& Entry=Rows[I+PC->SquadListScroll]; const float Y=Layout.Origin.Y+Layout.Header+I*Layout.RowHeight;
  if(Entry.bNewGroup)
  {
   // The drop row for a new attack group (2026-11-21: where to make a group was unclear in test play).
   DrawRect(Dragged!=INDEX_NONE?FLinearColor(.2f,.17f,.05f):FLinearColor(.08f,.08f,.05f),Layout.Origin.X+3,Y,Layout.Width-12,Layout.RowHeight-2);
   DrawText(TEXT("＋ 新しい攻撃群"),FLinearColor(1.f,.85f,.35f),Layout.Origin.X+9,Y+4,GEngine->GetSmallFont(),1);
   DrawText(TEXT("部隊をここへドラッグすると群を作ります（群の見出しへドラッグでその群へ）"),FLinearColor(.8f,.75f,.55f),Layout.Origin.X+9,Y+23,GEngine->GetSmallFont(),.7f);
   continue;
  }
  if(Entry.bGroupHeader)
  {
   // Group header: number, who runs it, strength; phase and objective; roles of its squads.
   const auto* G=Match->Groups.Find(Entry.GroupID); if(!G) continue;
   const FLinearColor Gold(1.f,.85f,.35f);
   int32 Leaders=0,Picked=0; FVector Center=FVector::ZeroVector; int32 Placed=0;
   for(int32 SID:G->Squads) for(const auto& S:All) if(S.SquadID==SID && S.bActive)
   { Center+=L->Position(S.LeaderUnitID); ++Placed; if(auto* A=L->Actor(S.LeaderUnitID)){++Leaders; Picked+=PC->IsSelected(A);} }
   if(Placed) Center/=Placed;
   DrawRect(Leaders>0&&Picked==Leaders?FLinearColor(.24f,.2f,.06f):FLinearColor(.13f,.11f,.05f),Layout.Origin.X+3,Y,Layout.Width-12,Layout.RowHeight-2);
   int32 Full=0; const int32 Living=Match->GroupLiving(*G,&Full);
   const TCHAR* Who=!G->bPlayer?TEXT("AI編成"):Match->GroupAIRun(*G)?TEXT("委任"):TEXT("手動指揮");
   DrawText(FString::Printf(TEXT("群%d　%s"),G->ID,Who),Gold,Layout.Origin.X+9,Y+4,GEngine->GetSmallFont(),1);
   DrawText(FString::Printf(TEXT("%d部隊 ／ 兵 %d/%d"),G->Squads.Num(),Living,Full),Gold,Layout.Origin.X+Layout.Width-150,Y+4,GEngine->GetSmallFont(),.85f);
   static const TCHAR* const PhaseNames[]={TEXT("集結"),TEXT("前進"),TEXT("保持")};
   FString Goal=TEXT("目標なし");
   if(Match->Objectives.IsValidIndex(G->Objective)) Goal=FString::Printf(TEXT("拠点%c ／ %.0fm"),TCHAR('A'+G->Objective),Placed?FVector::Dist2D(Center,Match->Objectives[G->Objective].Position)/100.:0.);
   static const TCHAR* const SurpriseNames[]={TEXT("集結"),TEXT("潜行"),TEXT("突入")};
   if(G->Kind==1) Goal=TEXT("前線維持");
   if(G->Kind==2) Goal=FString::Printf(TEXT("奇襲 ／ %s ／ 敵本拠地"),G->Phase<3?SurpriseNames[G->Phase]:TEXT("？"));
   DrawText(G->Kind!=0?Goal:FString::Printf(TEXT("%s ／ %s"),G->Phase<3?PhaseNames[G->Phase]:TEXT("？"),*Goal),Gold,Layout.Origin.X+9,Y+23,GEngine->GetSmallFont(),.8f);
   static const TCHAR* const RoleNames[]={TEXT("突撃"),TEXT("支援"),TEXT("装甲"),TEXT("火力"),TEXT("護衛"),TEXT("補給")};
   int32 Roles[7]={0,0,0,0,0,0,0}; for(int32 SID:G->Squads){const uint8 R=Match->GroupRole(*G,SID);++Roles[R<6?R:6];}
   FString RoleText; for(int32 R=0;R<7;++R) if(Roles[R]) RoleText+=FString::Printf(TEXT("%s%s %d"),RoleText.IsEmpty()?TEXT(""):TEXT("　"),R<6?RoleNames[R]:TEXT("役割なし"),Roles[R]);
   DrawText(RoleText,FLinearColor(.8f,.75f,.55f),Layout.Origin.X+9,Y+40,GEngine->GetSmallFont(),.7f);
   continue;
  }
  const auto& S=Entry.Squad; const float Indent=Entry.GroupID!=INDEX_NONE?14.f:0.f;
  const FLinearColor Color=!S.bActive?FLinearColor(.55f,.55f,.55f):!S.bSelectable?FLinearColor(.95f,.7f,.3f):FLinearColor(.28f,.95f,.8f);
  if(auto* A=L->Actor(S.LeaderUnitID); A && PC->IsSelected(A)) DrawRect(FLinearColor(.08f,.22f,.22f),Layout.Origin.X+3+Indent,Y,Layout.Width-12-Indent,Layout.RowHeight-2);
  if(Indent>0) DrawRect(FLinearColor(1.f,.85f,.35f,.5f),Layout.Origin.X+8,Y,2,Layout.RowHeight-2);
  const TCHAR* State=!S.bSelectable?TEXT("鍵 操作不可"):TEXT("○ 稼働");
  FString Name=S.Name.Len()>12?S.Name.Left(11)+TEXT("…"):S.Name;
  DrawText(Name,Color,Layout.Origin.X+9+Indent,Y+4,GEngine->GetSmallFont(),1);
  DrawText(State,Color,Layout.Origin.X+Layout.Width-110,Y+4,GEngine->GetSmallFont(),1);
  // Squad group number (BattleGroups.cpp), left of the state; not repeated inside the group tab.
  if(const int32 Army=Match->ArmyOf(S.SquadID);Army!=0 && Indent==0)DrawText(Army==1?TEXT("前線"):TEXT("攻撃"),FLinearColor(1.f,.85f,.35f),Layout.Origin.X+Layout.Width-152,Y+4,GEngine->GetSmallFont(),1);
  if(S.SquadID==Dragged) DrawRect(FLinearColor(1.f,.85f,.35f,.18f),Layout.Origin.X+3,Y,Layout.Width-12,Layout.RowHeight-2);
  if(const auto* Squad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(S.SquadID))
  {
   const auto& Plan=Squad->TacticalPlan;
   const bool Mission=Plan.Action==EBattleSquadTactic::Mission;
   const FVector Goal=Mission?Squad->Destination:Plan.Destination;
   const int32 Partner=Plan.Action==EBattleSquadTactic::Support?Plan.SupportSquad:Plan.Action==EBattleSquadTactic::Flank?Plan.FlankSupport:INDEX_NONE;
   const FString Link=Partner==INDEX_NONE?TEXT(""):FString::Printf(TEXT(" / S%d"),Partner);
   const FString Action=Mission?BattleUIText::Ja(BattleOrderName(Squad->Order)):FString::Printf(TEXT("%s %.0f秒"),*BattleUIText::Ja(BattleSquadTacticName(Plan.Action)),Plan.Age);
   DrawText(FString::Printf(TEXT("%s ／ %.0fm%s"),*Action,FVector::Dist2D(L->Position(S.LeaderUnitID),Goal)/100.,*Link),Color,Layout.Origin.X+9+Indent,Y+23,GEngine->GetSmallFont(),.8f);
   const auto* Sample=GetWorld()->GetSubsystem<UBattleSquadTactics>()->Find(S.SquadID);
   int32 Waiting=0;for(int32 ID:Squad->Members)if(const auto* Body=L->GetBodies().Find(ID))Waiting+=Body->bTrafficWait;
   FString Reason=!S.bActive?Squad->StopReason.ToString():!Squad->bPlayerDelegated?TEXT("手動指揮"):
    Waiting>0?FString::Printf(TEXT("通行待ち %d"),Waiting):!Mission?Plan.Reason:Sample?Sample->Reason:TEXT("報告待ち");
   Reason=BattleUIText::Ja(Reason);
   // The route chosen for the squad (BattlePlayerRoutes.cpp).
   if(const int32 RouteLane=Match->PlayerRouteLane(S.SquadID))Reason=FString::Printf(TEXT("道：%s ／ %s"),BattleRouteLaneName(RouteLane),*Reason);
   if(Reason.Len()>44)Reason=Reason.Left(43)+TEXT("...");
   DrawText(Reason,FLinearColor(.72f,.78f,.8f),Layout.Origin.X+9+Indent,Y+40,GEngine->GetSmallFont(),.7f);
  }
 }
 // The dragged squad's label follows the pointer.
 if(FVector2D P;Dragged!=INDEX_NONE && PC->GetPointerPosition(P))
 {
  DrawRect(FLinearColor(.1f,.09f,.03f,.9f),P.X+12,P.Y+6,120,20);
  const int32 Count=PC->DraggedSquads().Num();
  DrawText(Count>1?FString::Printf(TEXT("%d部隊 → 軍へ"),Count):FString::Printf(TEXT("部隊%d → 軍へ"),Dragged),FLinearColor(1.f,.85f,.35f),P.X+16,P.Y+8,GEngine->GetSmallFont(),.8f);
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

namespace
{
 struct FBattleRangeRing{float Radius;FLinearColor Color;FString Name;bool bDashed;};
 using FBattleRangeRings=TArray<FBattleRangeRing,TInlineAllocator<4>>;
 bool RangeRingsOn(){static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleRangeRings="),Mode);return Mode!=0;}();return bOn;}
 // Rings from a set of weapons. Own squads in mint/pink/yellow/orange, enemies in reds with "敵" in front of the name.
 FBattleRangeRings RangeRingsFor(const UBattleLeaderRuntime* L,float Direct,bool bAT,bool bMortar,bool bSPG,bool bTank,bool bEnemy,bool bGuess)
 {
  // Faint (alpha .4, user 11-21 "a little in the way"): the rings sit under the units and panels without hiding them.
  const FLinearColor Rifle=bEnemy?FLinearColor(1.f,.32f,.25f,.4f):FLinearColor(.28f,.95f,.8f,.4f), AT=bEnemy?FLinearColor(1.f,.35f,.6f,.4f):FLinearColor(1.f,.55f,.85f,.4f),
   Gun=bEnemy?FLinearColor(1.f,.6f,.15f,.45f):FLinearColor(1.f,.85f,.3f,.4f), Indirect=bEnemy?FLinearColor(.95f,.45f,.1f,.4f):FLinearColor(1.f,.55f,.2f,.4f);
  const TCHAR* Who=bEnemy?TEXT("敵 "):TEXT("");
  FBattleRangeRings Rings;
  if(Direct>0.f) Rings.Add({Direct,Rifle,FString::Printf(TEXT("%s射程%s"),Who,bGuess?TEXT("?"):TEXT("")),false});
  if(bAT) Rings.Add({BattleRanges::Engage(EBattleWeapon::AntiTank),AT,FString::Printf(TEXT("%s対戦車"),Who),false});
  if(bTank) Rings.Add({L->TankGunEngage(),Gun,FString::Printf(TEXT("%s主砲"),Who),false});
  if(bMortar||bSPG)
  {
   Rings.Add({L->ArtilleryMaxRange(bSPG),Indirect,FString::Printf(TEXT("%s砲撃"),Who),false});
   if(const float Min=L->ArtilleryMinRange(bSPG); Min>0.f) Rings.Add({Min,Indirect*FLinearColor(1,1,1,.7f),FString::Printf(TEXT("%s最短"),Who),true});
  }
  return Rings;
 }
 // Own squad: from the weapons of its living members (a squad that lost its anti-tank gunners loses that ring).
 FBattleRangeRings RangeRingsOf(const UBattleLeaderRuntime* L,const UBattleState* Store,const FBattleSquadState& S)
 {
  float Direct=0.f; bool bAT=false, bMortar=false;
  for(int32 ID:S.Members) if(const auto* M=Store->Find(ID); M&&M->IsAlive())
  {
   if(M->Weapon==EBattleWeapon::AntiTank) bAT=true; else if(M->Weapon==EBattleWeapon::Mortar) bMortar=true; else Direct=FMath::Max(Direct,BattleRanges::Engage(M->Weapon));
  }
  const bool bSPG=L->IsSPG(S.LeaderUnitID), bTank=L->IsVehicle(S.LeaderUnitID)&&L->VehicleType(S.LeaderUnitID)==BattleSquadType::Tank;
  return RangeRingsFor(L,Direct,bAT,bMortar,bSPG,bTank,false,false);
 }
 // Enemy squad: only what the player's side has identified (FBattleContact::ObservedSquadType), as a full squad of that type;
 // an unidentified squad is shown as a rifle squad with "?".
 FBattleRangeRings EnemyRangeRings(const UBattleLeaderRuntime* L,uint8 ObservedType)
 {
  const bool bGuess=ObservedType>=BattleSquadType::Count; const uint8 Type=bGuess?uint8(BattleSquadType::Rifle):ObservedType;
  float Direct=0.f; bool bAT=false, bMortar=false;
  const int32 Count=Type==BattleSquadType::Rifle?BattleSquadType::RifleMembers():BattleSquadType::Members(Type);
  for(int32 Slot=0;Slot<FMath::Max(1,Count);++Slot)
  {
   const EBattleWeapon W=BattleSquadType::Weapon(Type,Slot);
   if(W==EBattleWeapon::AntiTank) bAT=true; else if(W==EBattleWeapon::Mortar) bMortar=true; else Direct=FMath::Max(Direct,BattleRanges::Engage(W));
  }
  return RangeRingsFor(L,Direct,bAT,bMortar,Type==BattleSquadType::SPGun,Type==BattleSquadType::Tank,true,bGuess);
 }
 // Enemy squads the player's side sees now: centre of their seen soldiers and the identified type. Seen contacts only
 // (remembered ones and a scout's long-range sightings are left out, as on the minimap).
 struct FBattleSeenEnemySquad{FVector Sum=FVector::ZeroVector;int32 Count=0;uint8 Type=255;TArray<FVector,TInlineAllocator<8>> Soldiers;};
 TMap<int32,FBattleSeenEnemySquad> SeenEnemySquads(const UWorld* World)
 {
  TMap<int32,FBattleSeenEnemySquad> Out; TSet<int32> Counted;
  const auto* Store=World->GetSubsystem<UBattleState>(); const auto* Sense=World->GetSubsystem<UBattlePerception>(); const auto* Match=World->GetSubsystem<UBattleMatch>();
  for(const auto& Pair:Store->GetUnits()) if(Pair.Value.Team==0&&Pair.Value.IsAlive())
   if(const auto* Contacts=Sense->GetContacts(Pair.Key)) for(const auto& C:*Contacts)
   {
    if(!C.Value.bVisible||(Match->bArea&&Match->IsScoutLongContact(Pair.Key,C.Value))) continue;
    const auto* Target=Store->Find(C.Key); if(!Target||!Target->IsAlive()||Target->Team==0) continue;
    auto& G=Out.FindOrAdd(Target->SquadID);
    if(C.Value.ObservedSquadType<BattleSquadType::Count) G.Type=C.Value.ObservedSquadType;
    if(Counted.Contains(C.Key)) continue; Counted.Add(C.Key);
    G.Sum+=C.Value.LastKnownPosition; ++G.Count; G.Soldiers.Add(C.Value.LastKnownPosition);
   }
  return Out;
 }
}

void ABattleHUD::DrawRangeRings()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawRangeRings);
 if(!RangeRingsOn()) return;
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 const float H=Canvas->SizeY; const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 // Display check -BattleRangeRingShot=<prefix> (rendered game only): at 40 s the first own squad is selected and the camera moved
 // over it; from 90 s, once an enemy is seen (240 s at the latest), the own squad nearest a seen enemy is selected, every seen enemy's rings are shown (as with L) and the camera
 // is moved between them. A screenshot each time (the frame after the move), a BATTLE_RANGE_RING_SHOT line, then quit.
 static const FString ShotPrefix=[]{FString P;FParse::Value(FCommandLine::Get(),TEXT("BattleRangeRingShot="),P);return P;}();
 if(!ShotPrefix.IsEmpty())
 {
  static int32 Stage=0; static double ShootAt=-1., AimAt=-1.; static FVector AimTarget=FVector::ZeroVector; static bool bResumed=false; static double QuitAt=0.;
  const double T=GetWorld()->GetTimeSeconds();
  // The area match starts with the AI paused (practice); resume it once so squads move and meet the enemy.
  if(auto* Mode=GetWorld()->GetAuthGameMode<ABattleGameMode>(); !bResumed&&T>3.&&Mode&&GetWorld()->GetSubsystem<UBattleMatch>()->Phase==EBattleMatchPhase::Combat)
  {Mode->bCombatPaused=false;bResumed=true;}
  // Moves the camera pawn (height kept) so that the view centre lands on Where.
  auto Aim=[&](const FVector& Where)
  {
   APawn* Camera=PC->GetPawn(); if(!Camera||!PC->PlayerCameraManager) return;
   const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(), Forward=PC->PlayerCameraManager->GetCameraRotation().Vector();
   const double Down=FMath::Max(.2,-Forward.Z); const FVector Look=Eye+Forward*((Eye.Z-Where.Z)/Down);
   Camera->SetActorLocation(Camera->GetActorLocation()+FVector(Where.X-Look.X,Where.Y-Look.Y,0.));
  };
  if(ShootAt<0.&&Stage<2&&T>(Stage==0?40.:90.)&&(Stage==0||T>240.||!SeenEnemySquads(GetWorld()).IsEmpty()))
  {
   ABattleUnit* Pick=nullptr; double BestD=TNumericLimits<double>::Max(); FVector EnemyAt=FVector::ZeroVector;
   const auto Seen=SeenEnemySquads(GetWorld());
   for(const auto& S:L->Summaries()) if(S.bActive) if(ABattleUnit* A=L->Actor(S.LeaderUnitID); A&&A->IsAlive()&&A->GetState()->Team==0)
   {
    if(Stage==0){Pick=A;break;}
    for(const auto& E:Seen){const FVector P=E.Value.Sum/E.Value.Count;if(const double D=FVector::DistSquared2D(P,A->GetActorLocation());D<BestD){BestD=D;Pick=A;EnemyAt=P;}}
   }
   if(Stage==1&&!Pick) for(const auto& S:L->Summaries()) if(S.bActive) if(ABattleUnit* A=L->Actor(S.LeaderUnitID); A&&A->IsAlive()&&A->GetState()->Team==0){Pick=A;break;}
   PC->SetSelection(Pick); PC->RangeRingMode=Stage==1?3:1;
   // Zoom out (about x9 height, clamped by the camera) so a 250 m ring fits; aim on a later frame, once the new height is in the camera.
   if(Stage==0) if(auto* Camera=Cast<ABattleCameraPawn>(PC->GetPawn())) for(int32 I=0;I<10;++I) Camera->ZoomScale(1.25f);
   AimTarget=Pick?(Stage==1&&BestD<TNumericLimits<double>::Max()?(Pick->GetActorLocation()+EnemyAt)*.5:Pick->GetActorLocation()):FVector::ZeroVector;
   UE_LOG(LogTemp,Display,TEXT("BATTLE_RANGE_RING_SHOT t=%.0f stage=%d selected=%d seen_enemy_squads=%d enemy_dist_m=%.0f"),T,Stage,Pick?Pick->GetState()->SquadID:-1,Seen.Num(),BestD<TNumericLimits<double>::Max()?FMath::Sqrt(BestD)/100.:-1.);
   AimAt=Pick?T+.3:-1.; ShootAt=T+.8;
  }
  else if(AimAt>=0.&&T>=AimAt){Aim(AimTarget);AimAt=-1.;}
  else if(ShootAt>=0.&&T>=ShootAt)
  {FScreenshotRequest::RequestScreenshot(ShotPrefix+(Stage?TEXT("-enemy.png"):TEXT("-40s.png")),true,false);++Stage;ShootAt=-1.;QuitAt=T+3.;}
  if(Stage==2&&T>QuitAt)FPlatformMisc::RequestExit(false,TEXT("BattleRangeRingShot"));
 }
 auto Screen=[&](FVector World,FVector2D& Out){return PC->ProjectWorldLocationToScreen(World,Out)&&Out.Y>110&&Out.Y<H-130;};
 TArray<FVector2D> LabelSpots;
 auto DrawRings=[&](const FVector& Center,const FBattleRangeRings& Rings)
 {
  for(const FBattleRangeRing& R:Rings)
  {
   // Enough pieces that a 1 km ring stays round; each piece is projected so the visible part is drawn when the ring leaves the screen.
   const int32 N=FMath::Clamp(FMath::RoundToInt(R.Radius/400.f),48,160);
   FVector2D Prev; bool bPrev=Screen(Center+FVector(R.Radius,0,0),Prev); FVector2D Best(0,-1);
   for(int32 I=1;I<=N;++I)
   {
    const float A=I*2.f*PI/N; FVector2D P; const bool bNow=Screen(Center+FVector(FMath::Cos(A),FMath::Sin(A),0)*R.Radius,P);
    if(bPrev&&bNow&&(!R.bDashed||(I&1))) DrawLine(Prev.X,Prev.Y,P.X,P.Y,R.Color,1.f);
    if(bNow&&P.Y>Best.Y&&P.Y<H-215) Best=P; Prev=P; bPrev=bNow;
   }
   // Label at the lowest visible point of the ring above the bottom panel (nearest the camera), unless another label is already there.
   if(Best.Y<0) continue;
   bool bFree=true; for(const FVector2D& Spot:LabelSpots) if(FMath::Abs(Spot.X-Best.X)<90&&FMath::Abs(Spot.Y-Best.Y)<16){bFree=false;break;}
   if(!bFree) continue; LabelSpots.Add(Best);
   const FString Text=FString::Printf(TEXT("%s %.0fm"),*R.Name,R.Radius/100.f);
   DrawRect(FLinearColor(.025f,.04f,.065f,.4f),Best.X-3,Best.Y+3,Text.Len()*7.5f+6,13);
   DrawText(Text,FLinearColor(R.Color.R,R.Color.G,R.Color.B,.75f),Best.X,Best.Y+3,GEngine->GetSmallFont(),.7f);
  }
 };
 if(!PC->ShowsOwnRanges()) return;
 // Own selected squads, around the leader.
 TSet<int32> Done;
 for(const auto& Unit:PC->GetSelection())
 {
  if(!Unit||!Unit->GetState()||!Unit->IsAlive()) continue; const int32 SID=Unit->GetState()->SquadID; if(Done.Contains(SID)) continue; Done.Add(SID);
  if(const auto* S=Store->FindSquad(SID)) DrawRings(L->Position(S->LeaderUnitID)+FVector(0,0,30),RangeRingsOf(L,Store,*S));
 }
 // Seen enemy squads, around the centre of their seen soldiers: the one under the pointer (mode 2) or all of them (mode 3).
 if(!PC->ShowsHoveredEnemyRanges()&&!PC->ShowsAllEnemyRanges()) return;
 const auto Enemies=SeenEnemySquads(GetWorld()); if(Enemies.IsEmpty()) return;
 int32 Hovered=INDEX_NONE;
 FVector2D Pointer;
 if(PC->ShowsHoveredEnemyRanges()&&PC->GetPointerPosition(Pointer))
 {
  float BestD=28.f*28.f;
  for(const auto& E:Enemies) for(const FVector& Where:E.Value.Soldiers)
  {FVector2D P; if(PC->ProjectWorldLocationToScreen(Where+FVector(0,0,90),P)) if(const float D=FVector2D::DistSquared(P,Pointer); D<BestD){BestD=D;Hovered=E.Key;}}
 }
 for(const auto& E:Enemies) if(PC->ShowsAllEnemyRanges()||E.Key==Hovered)
  DrawRings(E.Value.Sum/E.Value.Count+FVector(0,0,30),EnemyRangeRings(L,E.Value.Type));
}

void ABattleHUD::DrawMinimapRangeRings(ABattleController* PC,const FBattleMinimapLayout& Map)
{
 if(!RangeRingsOn()||!PC->ShowsOwnRanges()) return;
 const auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto DrawRings=[&](const FVector& Center,const FBattleRangeRings& Rings)
 {
  for(const FBattleRangeRing& R:Rings)
  {
   // Through ToScreen point by point (the map may scale X and Y differently); pieces outside the map are left out.
   constexpr int32 N=48; FVector2D Prev=Map.ToScreen(Center+FVector(R.Radius,0,0));
   for(int32 I=1;I<=N;++I)
   {
    const float A=I*2.f*PI/N; const FVector2D P=Map.ToScreen(Center+FVector(FMath::Cos(A),FMath::Sin(A),0)*R.Radius);
    if(Map.Contains(Prev)&&Map.Contains(P)&&(!R.bDashed||(I&1))) DrawLine(Prev.X,Prev.Y,P.X,P.Y,R.Color,1.f);
    Prev=P;
   }
  }
 };
 TSet<int32> Done;
 for(const auto& Unit:PC->GetSelection())
 {
  if(!Unit||!Unit->GetState()||!Unit->IsAlive()) continue; const int32 SID=Unit->GetState()->SquadID; if(Done.Contains(SID)) continue; Done.Add(SID);
  if(const auto* S=Store->FindSquad(SID)) DrawRings(L->Position(S->LeaderUnitID),RangeRingsOf(L,Store,*S));
 }
 // Enemies on the minimap only in mode 3 (the pointer is usually on the field, not the map).
 if(PC->ShowsAllEnemyRanges()) for(const auto& E:SeenEnemySquads(GetWorld())) DrawRings(E.Value.Sum/E.Value.Count,EnemyRangeRings(L,E.Value.Type));
}

namespace
{
 bool SquadIconsOn(){static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleSquadIcons="),Mode);return Mode!=0;}();return bOn;}
 // Remembered enemy squads (none of their soldiers seen now): the newest last-known position among their remembered soldiers,
 // when they were last seen and the identified type. Same contacts as the minimap's remembered dots (confidence above .35).
 struct FBattleRememberedEnemySquad{FVector Where=FVector::ZeroVector;double SeenAt=-1.;uint8 Type=255;int32 Count=0;};
 TMap<int32,FBattleRememberedEnemySquad> RememberedEnemySquads(const UWorld* World,const TMap<int32,FBattleSeenEnemySquad>& Seen)
 {
  TMap<int32,FBattleRememberedEnemySquad> Out; TSet<int32> Counted;
  const auto* Store=World->GetSubsystem<UBattleState>(); const auto* Sense=World->GetSubsystem<UBattlePerception>(); const auto* Match=World->GetSubsystem<UBattleMatch>();
  for(const auto& Pair:Store->GetUnits()) if(Pair.Value.Team==0&&Pair.Value.IsAlive())
   if(const auto* Contacts=Sense->GetContacts(Pair.Key)) for(const auto& C:*Contacts)
   {
    if(C.Value.bVisible||C.Value.Confidence<=.35f||(Match->bArea&&Match->IsScoutLongContact(Pair.Key,C.Value))) continue;
    const auto* Target=Store->Find(C.Key); if(!Target||Target->Team==0||Seen.Contains(Target->SquadID)) continue;
    auto& G=Out.FindOrAdd(Target->SquadID);
    if(C.Value.ObservedSquadType<BattleSquadType::Count) G.Type=C.Value.ObservedSquadType;
    if(C.Value.LastSeenTime>G.SeenAt){G.SeenAt=C.Value.LastSeenTime;G.Where=C.Value.LastKnownPosition;}
    if(!Counted.Contains(C.Key)){Counted.Add(C.Key);++G.Count;}
   }
  return Out;
 }
 const TCHAR* SquadTypeLetter(uint8 Type)
 {
  static const TCHAR* const Letters[BattleSquadType::Count]={TEXT("歩"),TEXT("重"),TEXT("偵"),TEXT("対"),TEXT("戦"),TEXT("装"),TEXT("迫"),TEXT("砲"),TEXT("外"),TEXT("補")};
  return Type<BattleSquadType::Count?Letters[Type]:TEXT("？");
 }
}

void ABattleHUD::DrawSquadIcons()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawSquadIcons);
 if(!SquadIconsOn()) return;
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical() || !PC->PlayerCameraManager) return;
 const float H=Canvas->SizeY; const auto* Store=GetWorld()->GetSubsystem<UBattleState>(); const double Now=GetWorld()->GetTimeSeconds();
 const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
 // Icons fade in as the camera pulls away (50 m -> 100 m from the squad): close up the soldiers themselves are readable.
 auto Fade=[&](const FVector& Where){return FMath::Clamp(float((FVector::Dist(Eye,Where)-5000.)/5000.),0.f,1.f);};
 // One map-symbol style icon at a screen point: a frame (dashed for a remembered position), the type symbol inside, the type letter
 // under it and an optional note at the right (soldier count, or how long ago it was seen).
 auto Icon=[&](FVector2D C,uint8 Type,FLinearColor Color,bool bDashed,bool bSelected,const FString& Note)
 {
  const float HW=14.f,HH=9.f,T=bSelected?2.f:1.2f;
  const FLinearColor Ink(.025f,.04f,.065f,.55f*Color.A);
  DrawRect(Ink,C.X-HW,C.Y-HH,2*HW,2*HH);
  auto Line=[&](float X0,float Y0,float X1,float Y1,float Th){DrawLine(C.X+X0,C.Y+Y0,C.X+X1,C.Y+Y1,Color,Th);};
  const FVector2D Corners[4]={FVector2D(-HW,-HH),FVector2D(HW,-HH),FVector2D(HW,HH),FVector2D(-HW,HH)};
  for(int32 I=0;I<4;++I)
  {
   const FVector2D A=Corners[I],B=Corners[(I+1)%4];
   if(!bDashed){Line(A.X,A.Y,B.X,B.Y,T);continue;}
   for(int32 S=0;S<4;S+=2){const FVector2D P0=FMath::Lerp(A,B,S/4.f),P1=FMath::Lerp(A,B,(S+1)/4.f);Line(P0.X,P0.Y,P1.X,P1.Y,T);}
  }
  auto Oval=[&](float RX,float RY,float OX=0.f,float OY=0.f){for(int32 I=0;I<16;++I){const float A0=I*PI/8,A1=(I+1)*PI/8;Line(OX+RX*FMath::Cos(A0),OY+RY*FMath::Sin(A0),OX+RX*FMath::Cos(A1),OY+RY*FMath::Sin(A1),1.f);}};
  using namespace BattleSquadType;
  switch(Type)
  {
  case Rifle: Line(-HW,-HH,HW,HH,1.f);Line(-HW,HH,HW,-HH,1.f);break;                                   // infantry: X
  case Heavy: Line(-HW,-HH,HW,HH,1.f);Line(-HW,HH,HW,-HH,1.f);Line(-HW,0,HW,0,1.f);break;              // X + bar
  case Scout: Line(-HW,HH,HW,-HH,1.f);break;                                                         // reconnaissance: one diagonal
  case AntiTank: Line(-HW,HH,0,-HH,1.f);Line(0,-HH,HW,HH,1.f);break;                                  // anti-tank: inverted V
  case Tank: Oval(HW*.65f,HH*.55f);break;                                                            // armour: track oval
  case ArmoredCar: Oval(HW*.65f,HH*.55f);Line(-HW,HH,HW,-HH,1.f);break;                               // oval + diagonal
  case Mortar: Oval(2.5f,2.5f,0.f,HH*.45f);Line(0,HH*.45f-2.5f,0,-HH*.7f,1.f);Line(-3,-HH*.35f,0,-HH*.7f,1.f);Line(3,-HH*.35f,0,-HH*.7f,1.f);break; // tube with arrow
  case SPGun: Oval(HW*.65f,HH*.55f);DrawRect(Color,C.X-1.5f,C.Y-1.5f,3,3);break;                       // oval + dot
  case Exo: Line(-HW,-HH,HW,HH,1.f);Line(-HW,HH,HW,-HH,1.f);Oval(3.f,3.f);break;                       // X + ring
  case Supply: Line(-HW,HH*.35f,HW,HH*.35f,1.f);break;                                                 // supply: lower bar
  default: DrawText(TEXT("?"),Color,C.X-3,C.Y-8,GEngine->GetSmallFont(),.9f);break;                    // unidentified
  }
  if(Type<BattleSquadType::Count) DrawText(SquadTypeLetter(Type),Color,C.X-6,C.Y+HH+1,GEngine->GetSmallFont(),.75f);
  if(!Note.IsEmpty()) DrawText(Note,Color,C.X+HW+3,C.Y-7,GEngine->GetSmallFont(),.72f);
 };
 struct FItem{FVector2D C;uint8 Type;FLinearColor Color;bool bDashed,bSelected;FString Note;int32 Kind;int32 Merged=0;};
 TArray<FItem> Items;
 auto Add=[&](const FVector& World,uint8 Type,FLinearColor Color,bool bDashed,bool bSelected,FString Note,int32 Kind)
 {
  FVector2D C; if(!PC->ProjectWorldLocationToScreen(World+FVector(0,0,250),C)) return;
  C.Y-=14.f; if(C.X<20||C.X>Canvas->SizeX-20||C.Y<120||C.Y>H-215) return;
  Items.Add({C,Type,Color,bDashed,bSelected,MoveTemp(Note),Kind});
 };
 // Own squads (every active one), at the leader. Selected ones get a heavier frame.
 for(const auto& S:L->Summaries()) if(S.bActive)
 {
  const auto* Squad=Store->FindSquad(S.SquadID); if(!Squad||Squad->Team!=0) continue;
  const FVector Where=L->Position(S.LeaderUnitID); const float A=Fade(Where); if(A<=0.f) continue;
  int32 Alive=0; for(int32 ID:Squad->Members) if(const auto* M=Store->Find(ID); M&&M->IsAlive()) ++Alive;
  ABattleUnit* Leader=L->Actor(S.LeaderUnitID);
  Add(Where,Squad->SquadType,FLinearColor(.45f,.85f,1.f,.9f*A),false,Leader&&PC->IsSelected(Leader),FString::FromInt(Alive),0);
 }
 // Seen enemy squads: identified type only, at the centre of the seen soldiers.
 const auto Seen=SeenEnemySquads(GetWorld());
 for(const auto& E:Seen)
 {
  const FVector Where=E.Value.Sum/E.Value.Count; const float A=Fade(Where); if(A<=0.f) continue;
  Add(Where,E.Value.Type,FLinearColor(1.f,.35f,.28f,.95f*A),false,false,FString::FromInt(E.Value.Count),1);
 }
 // Last known place of enemy squads no longer seen: always shown (also close up), dashed and dimmer, with the time since.
 for(const auto& R:RememberedEnemySquads(GetWorld(),Seen))
  Add(R.Value.Where,R.Value.Type,FLinearColor(1.f,.55f,.35f,.7f),true,false,FString::Printf(TEXT("%.0f秒前"),FMath::Max(0.,Now-R.Value.SeenAt)),2);
 // Icons of the same kind that would overlap are merged into the first (selected first): it shows "+N" for the others.
 Items.StableSort([](const FItem& X,const FItem& Y){return X.bSelected&&!Y.bSelected;});
 TArray<int32> Shown;
 for(int32 I=0;I<Items.Num();++I)
 {
  bool bMerged=false;
  for(int32 J:Shown) if(Items[J].Kind==Items[I].Kind&&FMath::Abs(Items[J].C.X-Items[I].C.X)<34.f&&FMath::Abs(Items[J].C.Y-Items[I].C.Y)<26.f){++Items[J].Merged;bMerged=true;break;}
  if(!bMerged) Shown.Add(I);
 }
 for(int32 I:Shown)
 {
  const FItem& It=Items[I];
  Icon(It.C,It.Type,It.Color,It.bDashed,It.bSelected,It.Merged>0?FString::Printf(TEXT("%s +%d"),*It.Note,It.Merged):It.Note);
 }
}

void ABattleHUD::DrawOrderMarks()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawOrderMarks);
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical() || PC->GetSelection().IsEmpty()) return;
 const float MarksBottom=FBattleCommandBarLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld()).PanelTop-2.f; const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 auto Screen=[&](FVector World,FVector2D& Out){return PC->ProjectWorldLocationToScreen(World,Out)&&Out.Y>110&&Out.Y<MarksBottom;};
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
  // (The way itself is drawn by DrawOrderWays; the straight line stays only as a faint hint of the direction.)
  Segment(L->Position(S->LeaderUnitID)+FVector(0,0,40),Goal,Color*FLinearColor(1,1,1,.25f),1.f);
  FVector2D G; if(Screen(Goal,G)){DrawLine(G.X,G.Y-9,G.X+9,G.Y,Color,2);DrawLine(G.X+9,G.Y,G.X,G.Y+9,Color,2);DrawLine(G.X,G.Y+9,G.X-9,G.Y,Color,2);DrawLine(G.X-9,G.Y,G.X,G.Y-9,Color,2);}
  // Defence radius.
  if(Order==EBattleOrder::Defend)
   for(int32 I=0;I<32;++I){const float A=I*2*PI/32,B=(I+1)*2*PI/32;Segment(Goal+FVector(FMath::Cos(A),FMath::Sin(A),0)*S->DefenseRadius,Goal+FVector(FMath::Cos(B),FMath::Sin(B),0)*S->DefenseRadius,Color*FLinearColor(1,1,1,.6f),1);}
  // Arrival slots of living members.
  if(const auto* Plan=L->PlanOf(SID)) for(const auto& Slot:Plan->ArrivalGoals) if(const auto* M=Store->Find(Slot.Key); M&&M->IsAlive())
  {FVector2D P; if(Screen(Slot.Value+FVector(0,0,20),P)) DrawRect(Color*FLinearColor(1,1,1,.7f),P.X-2,P.Y-2,4,4);}
 }
}

void ABattleHUD::DrawHitDirection(ABattleController* PC)
{
 static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleHitDirection="),Mode);return Mode!=0;}();
 auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const ABattleUnit* Unit=PC->IsTactical()?nullptr:Cast<ABattleUnit>(PC->GetPawn());
 const int32 Watch=Unit?Unit->GetUnitID():INDEX_NONE;
 if(Store->HitWatchID!=Watch){Store->HitWatchID=Watch;Store->HitNotices.Reset();}
 if(!bOn||!Unit||!Canvas)return;
 const double Now=GetWorld()->GetTimeSeconds();const float Fade=1.5f;
 const float W=Canvas->SizeX,H=Canvas->SizeY,Cx=W*.5f,Cy=H*.5f,R=FMath::Min(W,H)*.14f;
 const float CameraYaw=PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraRotation().Yaw:PC->GetControlRotation().Yaw;
 const FVector Here=Unit->GetActorLocation();
 for(const FBattleHitNotice& N:Store->HitNotices)
 {
  const float Age=float(Now-N.Time);if(Age<0.f||Age>Fade)continue;
  const float Alpha=1.f-Age/Fade;const FLinearColor Red(1.f,.12f,.08f,.9f*Alpha);
  const FVector To=N.From-Here;
  if(!N.bKnown||To.SizeSquared2D()<100.f*100.f)
  {
   // Source unknown or right on top of the soldier: a thin ring.
   for(int32 I=0;I<32;++I){const float A0=I*2*PI/32,A1=(I+1)*2*PI/32;DrawLine(Cx+FMath::Sin(A0)*R,Cy-FMath::Cos(A0)*R,Cx+FMath::Sin(A1)*R,Cy-FMath::Cos(A1)*R,Red*FLinearColor(1,1,1,.6f),2.f);}
   continue;
  }
  // Angle on screen: 0 = straight ahead (top), clockwise to the right.
  const float Angle=FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(CameraYaw,FMath::RadiansToDegrees(FMath::Atan2(To.Y,To.X))));
  const float Half=FMath::DegreesToRadians(20.f);
  for(int32 I=0;I<8;++I)
  {
   const float A0=Angle-Half+2*Half*I/8,A1=Angle-Half+2*Half*(I+1)/8;
   DrawLine(Cx+FMath::Sin(A0)*R,Cy-FMath::Cos(A0)*R,Cx+FMath::Sin(A1)*R,Cy-FMath::Cos(A1)*R,Red,6.f);
  }
  // A short point outward so the arc reads as an arrow.
  const FVector2D Tip(Cx+FMath::Sin(Angle)*(R+18.f),Cy-FMath::Cos(Angle)*(R+18.f));
  for(const float Side:{-1.f,1.f})
  {const float A=Angle+Side*FMath::DegreesToRadians(7.f);DrawLine(Cx+FMath::Sin(A)*(R+3.f),Cy-FMath::Cos(A)*(R+3.f),Tip.X,Tip.Y,Red,3.f);}
 }
}

void ABattleHUD::DrawOffscreenMarkers(ABattleController* PC)
{
 static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleOffscreenMarkers="),Mode);return Mode!=0;}();
 if(!bOn||!Canvas||!PC->PlayerCameraManager)return;
 auto* Match=GetWorld()->GetSubsystem<UBattleMatch>();auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 const float W=Canvas->SizeX,H=Canvas->SizeY;
 // The free part of the screen: the top bar and the bottom help lines are kept clear (same bounds as the on-screen labels).
 const float Left=40.f,Right=W-40.f,Top=110.f,Bottom=PC->IsTactical()?FBattleCommandBarLayout::Make(FVector2D(W,H),GetWorld()).PanelTop-12.f:H-140.f;const FVector2D Center(W*.5f,(Top+Bottom)*.5f);
 const FVector CamLoc=PC->PlayerCameraManager->GetCameraLocation();const FRotator CamRot=PC->PlayerCameraManager->GetCameraRotation();
 const ABattleUnit* Unit=PC->IsTactical()?nullptr:Cast<ABattleUnit>(PC->GetPawn());
 const FLinearColor Mint(.28f,.95f,.8f),Enemy(1,.35f,.25f),Muted(.62f,.7f,.77f),White(.94f,.97f,1.f),Ink(.025f,.04f,.065f,.8f);
 auto Marker=[&](const FVector& World,const FString& Text,FLinearColor Color)
 {
  const FVector Local=CamRot.UnrotateVector(World-CamLoc); // X forward, Y right, Z up
  FVector2D Dir;FVector2D P;
  if(Local.X>1.f&&PC->ProjectWorldLocationToScreen(World,P))
  {
   if(P.X>=Left&&P.X<=Right&&P.Y>=Top&&P.Y<=Bottom)return; // on the screen: the normal labels show it
   Dir=P-Center;
  }
  else Dir=FVector2D(Local.Y,-Local.Z); // behind the camera: never use the mirrored projection
  if(Dir.IsNearlyZero())Dir=FVector2D(0,1);
  Dir.Normalize();
  // Where the ray from the centre leaves the free rectangle.
  const float Tx=Dir.X>0?(Right-Center.X)/Dir.X:Dir.X<0?(Left-Center.X)/Dir.X:BIG_NUMBER;
  const float Ty=Dir.Y>0?(Bottom-Center.Y)/Dir.Y:Dir.Y<0?(Top-Center.Y)/Dir.Y:BIG_NUMBER;
  const FVector2D Edge=Center+Dir*FMath::Min(Tx,Ty);
  const FVector2D Side(-Dir.Y,Dir.X),Tip=Edge+Dir*10.f,Back=Edge-Dir*8.f;
  DrawLine(Tip.X,Tip.Y,Back.X+Side.X*9.f,Back.Y+Side.Y*9.f,Color,3.f);
  DrawLine(Tip.X,Tip.Y,Back.X-Side.X*9.f,Back.Y-Side.Y*9.f,Color,3.f);
  DrawLine(Back.X+Side.X*9.f,Back.Y+Side.Y*9.f,Back.X-Side.X*9.f,Back.Y-Side.Y*9.f,Color,2.f);
  // Label inside the arrow, pulled toward the centre so it stays on the screen.
  const FString Full=Unit?FString::Printf(TEXT("%s %.0fm"),*Text,FVector::Dist2D(World,Unit->GetActorLocation())/100.f):Text;
  float TW=0,TH=0;GetTextSize(Full,TW,TH,GEngine->GetSmallFont(),.85f);
  const FVector2D L=Back-Dir*(16.f+FMath::Abs(Dir.X)*TW*.5f+FMath::Abs(Dir.Y)*TH*.5f)-FVector2D(TW*.5f,TH*.5f);
  DrawRect(Ink,L.X-3,L.Y-1,TW+6,TH+2);DrawText(Full,Color,L.X,L.Y,GEngine->GetSmallFont(),.85f);
 };
 // Objectives only in direct control: the tactical view has the minimap and a free camera.
 if(Match&&Unit)for(int32 Index=0;Index<Match->Objectives.Num();++Index)
 {
  const int32 Holder=Match->bArea?Match->ObservedOwner(0,Index):Match->Objectives[Index].Owner;
  const FLinearColor Color=Holder==0?Mint:Holder==1?Enemy:Holder==-2?Muted:White;
  Marker(Match->Objectives[Index].Position+FVector(0,0,45),FString::Printf(TEXT("%c"),'A'+Index),Color);
 }
 if(Unit)
 {
  // Direct control: where the soldier's own squad has been ordered to go.
  const auto* U=Unit->GetState();const auto* S=U?Store->FindSquad(U->SquadID):nullptr;
  if(S&&S->CommandStatus==EBattleCommandStatus::Active)
  {
   const EBattleOrder Order=S->bHasMission?S->MissionOrder:S->Order;
   if(Order!=EBattleOrder::Hold)Marker((S->bHasMission?S->MissionDestination:S->Destination)+FVector(0,0,25),TEXT("目標"),FLinearColor::Yellow);
  }
 }
 else
 {
  // Tactical view: the selected squads (one marker per squad, at its leader).
  TSet<int32> Done;
  for(const auto& Selected:PC->GetSelection())
  {
   if(!Selected||!Selected->GetState()||!Selected->IsAlive())continue;const int32 SID=Selected->GetState()->SquadID;
   if(SID==INDEX_NONE||Done.Contains(SID))continue;Done.Add(SID);
   const auto* S=Store->FindSquad(SID);Marker(Selected->GetActorLocation()+FVector(0,0,120),S?S->DisplayName:FString(TEXT("選択中")),Mint);
  }
 }
}

void ABattleHUD::DrawKillLog(ABattleController* PC)
{
 static const bool bOn=[]{int32 Mode=1;FParse::Value(FCommandLine::Get(),TEXT("BattleKillLog="),Mode);return Mode!=0;}();
 if(!bOn||!Canvas)return;
 const auto* Store=GetWorld()->GetSubsystem<UBattleState>();if(Store->KillNotices.IsEmpty())return;
 const double Now=GetWorld()->GetTimeSeconds();const float Life=8.f,RowH=22.f,Width=340.f;
 const ABattleUnit* Unit=PC->IsTactical()?nullptr:Cast<ABattleUnit>(PC->GetPawn());const int32 Me=Unit?Unit->GetUnitID():INDEX_NONE;
 const float W=Canvas->SizeX;
 float X=W-16.f-Width,Y=96.f;
 if(PC->IsTactical()&&GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->IsEnabled())
 {const auto List=FBattleSquadListLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld());if(List.Height>0){X=List.Origin.X-10.f-Width;Y=List.Origin.Y;}}
 auto Weapon=[](const FBattleKillNotice& N)->const TCHAR*
 {
  switch(N.Kind)
  {
   case EBattleKillKind::Blast:return TEXT("対戦車弾");
   case EBattleKillKind::Artillery:return TEXT("砲撃");
   case EBattleKillKind::TankGun:return TEXT("戦車砲");
   case EBattleKillKind::Launcher:return TEXT("バズーカ");
   case EBattleKillKind::Shot:
    if(N.bKillerVehicle)return TEXT("車載機関銃");
    return N.KillerWeapon==EBattleWeapon::SupportGun?TEXT("機関銃"):N.KillerWeapon==EBattleWeapon::AntiTank?TEXT("対戦車兵"):N.KillerWeapon==EBattleWeapon::Mortar?TEXT("砲"):TEXT("小銃");
   default:return TEXT("");
  }
 };
 int32 Row=0;
 for(int32 I=Store->KillNotices.Num()-1;I>=0&&Row<5;--I)
 {
  const FBattleKillNotice& N=Store->KillNotices[I];const float Age=float(Now-N.Time);if(Age<0.f||Age>Life)continue;
  const float Alpha=FMath::Clamp(Life-Age,0.f,1.f);
  const FString Killer=N.KillerID==Me?FString(TEXT("あなた")):N.KillerTeam==0?(N.KillerSquad.IsEmpty()?FString(TEXT("味方")):N.KillerSquad):FString(TEXT("敵"));
  const FString Victim=N.VictimID==Me?FString(TEXT("あなた"))
   :N.VictimTeam==0?(N.VictimSquad.IsEmpty()?FString(TEXT("味方の兵")):N.VictimSquad+(N.bVictimVehicle?TEXT(""):N.bVictimLeader?TEXT("の隊長"):TEXT("の兵")))
   :FString(N.bVictimVehicle?TEXT("敵の車両"):N.bVictimLeader?TEXT("敵の隊長"):TEXT("敵兵"));
  const bool bMine=N.KillerID==Me||N.VictimID==Me;
  const FLinearColor Color=bMine?FLinearColor(1,.9f,.35f,Alpha):N.KillerTeam==0?FLinearColor(.28f,.95f,.8f,Alpha):FLinearColor(1,.45f,.35f,Alpha);
  const float RowY=Y+Row*RowH;
  DrawRect(FLinearColor(.025f,.04f,.065f,.8f*Alpha),X,RowY,Width,RowH-2);
  DrawRect(Color,X,RowY,3,RowH-2);
  DrawText(FString::Printf(TEXT("%s → %s"),*Killer,*Victim),Color,X+9,RowY+3,GEngine->GetSmallFont(),.85f);
  const FString Tag=Weapon(N);
  if(!Tag.IsEmpty()){float TW=0,TH=0;GetTextSize(Tag,TW,TH,GEngine->GetSmallFont(),.8f);DrawText(Tag,FLinearColor(.62f,.7f,.77f,Alpha),X+Width-TW-8,RowY+4,GEngine->GetSmallFont(),.8f);}
  ++Row;
 }
}

namespace
{
 const FString& SectorViewShotPrefix(){static const FString Prefix=[]{FString P;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorViewShot="),P);return P;}();return Prefix;}
 // Status colours shared by the field and the minimap (index = BattleSectorView::StatusOf).
 FLinearColor SectorTint(int32 Status)
 {
  switch(Status)
  {
  case 1:return FLinearColor(.28f,.95f,.8f);case 2:return FLinearColor(1.f,.85f,.2f);case 3:return FLinearColor(1.f,.35f,.25f);
  case 4:return FLinearColor(.95f,.6f,.3f);case 5:return FLinearColor(.6f,.75f,.9f);default:return FLinearColor(.5f,.55f,.6f);
  }
 }
 void AddQuad(TArray<FCanvasUVTri>& Tris,const FVector2D P[4],const FLinearColor C[4])
 {
  FCanvasUVTri T;T.V0_Pos=P[0];T.V1_Pos=P[1];T.V2_Pos=P[2];T.V0_Color=C[0];T.V1_Color=C[1];T.V2_Color=C[2];Tris.Add(T);
  T.V0_Pos=P[0];T.V1_Pos=P[2];T.V2_Pos=P[3];T.V0_Color=C[0];T.V1_Color=C[2];T.V2_Color=C[3];Tris.Add(T);
 }
 void DrawTris(UCanvas* Canvas,const TArray<FCanvasUVTri>& Tris)
 {
  if(Tris.IsEmpty()||!Canvas)return;
  FCanvasTriangleItem Item(Tris,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
 }
 // Fog darkness by state (0 in sight, 1 explored, 2 never seen).
 constexpr float FogAlpha[3]={0.f,.52f,.74f};
 const FLinearColor FogInk(.015f,.025f,.05f);
}

void ABattleHUD::DrawFieldOverlay(ABattleController* PC)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(BattleHUD_DrawFieldOverlay);
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
 if(!PC||!Canvas||!PC->IsTactical()||!L->IsEnabled()||!BattleFieldLayout::IsSightField(GetWorld())) return;
 const double Now=GetWorld()->GetRealTimeSeconds();
 // The fog is kept up to date whenever the tactical view is open, so the minimap shows it even with the field overlay hidden.
 Fog.Update(GetWorld(),Now,400);
 const bool bSectors=BattleFieldLayout::Of(GetWorld()).bSectors;
 BattleSectorView::Gather(GetWorld(),Fog,Now,FrameContacts,Sectors);
 // Display check -BattleSectorViewShot=<prefix> (rendered game only): screenshots of the tactical view at 40 s and 90 s of game time
 // with the pointer held at the screen centre, a BATTLE_SECTOR_VIEW line with the fog and sector counts, then quit.
 if(const FString& Shot=SectorViewShotPrefix();!Shot.IsEmpty())
 {
  static int32 Stage=0;const double T=GetWorld()->GetTimeSeconds();
  if(Stage<2&&T>(Stage==0?40.:90.))
  {
   int32 InSight=0,Explored=0;for(int32 I=0;I<Fog.LastSeen.Num();++I){if(Fog.InSight(I,Now))++InSight;if(Fog.Explored(I))++Explored;}
   int32 ByStatus[6]={0,0,0,0,0,0};for(const auto& S:Sectors)++ByStatus[BattleSectorView::StatusOf(S)];
   UE_LOG(LogTemp,Display,TEXT("BATTLE_SECTOR_VIEW t=%.0f cells=%d in_sight=%d explored=%d cycle=%.2f sectors=%d fog=%d own=%d contested=%d enemy=%d trace=%d clear=%d"),
    T,Fog.LastSeen.Num(),InSight,Explored,Fog.CycleSeconds,Sectors.Num(),ByStatus[0],ByStatus[1],ByStatus[2],ByStatus[3],ByStatus[4],ByStatus[5]);
   FScreenshotRequest::RequestScreenshot(Shot+(Stage?TEXT("-90s.png"):TEXT("-40s.png")),true,false);++Stage;
  }
  if(Stage==2&&T>93.)FPlatformMisc::RequestExit(false,TEXT("BattleSectorViewShot"));
 }
 if(!Fog.IsValid()||(!PC->bShowFog&&(!bSectors||!PC->bShowSectorGrid))) return;
 const float W=Canvas->SizeX,H=Canvas->SizeY;
 // Corners of the fog cells on the screen (false where the corner is behind the camera).
 const int32 VC=Fog.Columns+1,VR=Fog.Rows+1;
 auto& Screen=OverlayScreen;Screen.SetNumUninitialized(VC*VR,EAllowShrinking::No);auto& On=OverlayOn;On.Init(false,VC*VR);
 for(int32 Y=0;Y<VR;++Y)for(int32 X=0;X<VC;++X)
 {
  const FVector2D M=Fog.CellMin(X,Y);FVector2D S;
  if(PC->ProjectWorldLocationToScreen(FVector(M.X,M.Y,Fog.Corner(X,Y)+20),S)){Screen[Y*VC+X]=S;On[Y*VC+X]=true;}
 }
 auto Visible=[&](int32 X,int32 Y)
 {
  const int32 I[4]={Y*VC+X,Y*VC+X+1,(Y+1)*VC+X+1,(Y+1)*VC+X};
  bool bInside=false;
  for(int32 K:I){if(!On[K])return false;const FVector2D& S=Screen[K];if(S.X>-50&&S.X<W+50&&S.Y>-50&&S.Y<H+50)bInside=true;}
  return bInside;
 };
 // Sector status tint (under the fog): seen now in a stronger tint, only remembered in a fainter one.
 if(bSectors&&PC->bShowSectorGrid&&!Sectors.IsEmpty())
 {
  auto& Tris=OverlayTris;Tris.Reset();
  for(int32 Y=0;Y<Fog.Rows;++Y)for(int32 X=0;X<Fog.Columns;++X)
  {
   const FVector2D Mid=Fog.CellMin(X,Y)+FVector2D(FBattleFogGrid::Cell*.5);const FIntPoint C=BattleSectors::Cell(FVector(Mid.X,Mid.Y,0));
   if(!BattleSectors::Valid(C))continue;const int32 Status=BattleSectorView::StatusOf(Sectors[C.Y*BattleSectors::Columns+C.X]);
   if(Status<1||Status>4||!Visible(X,Y))continue;
   FLinearColor Tint=SectorTint(Status);Tint.A=Status==4?.10f:.16f;
   const FVector2D P[4]={Screen[Y*VC+X],Screen[Y*VC+X+1],Screen[(Y+1)*VC+X+1],Screen[(Y+1)*VC+X]};const FLinearColor Cs[4]={Tint,Tint,Tint,Tint};
   AddQuad(Tris,P,Cs);
  }
  DrawTris(Canvas,Tris);
 }
 // Fog: each corner takes the mean darkness of the cells around it, so the edge of sight is soft.
 if(PC->bShowFog)
 {
  auto& Dark=OverlayDark;Dark.Reset(VC*VR);Dark.AddZeroed(VC*VR);auto& Count=OverlayCount;Count.Reset(VC*VR);Count.AddZeroed(VC*VR);
  for(int32 Y=0;Y<Fog.Rows;++Y)for(int32 X=0;X<Fog.Columns;++X)
  {
   const float A=FogAlpha[Fog.State(Y*Fog.Columns+X,Now)];
   for(const int32 K:{Y*VC+X,Y*VC+X+1,(Y+1)*VC+X+1,(Y+1)*VC+X}){Dark[K]+=A;++Count[K];}
  }
  for(int32 K=0;K<Dark.Num();++K)if(Count[K])Dark[K]/=Count[K];
  auto& Tris=OverlayTris;Tris.Reset();
  for(int32 Y=0;Y<Fog.Rows;++Y)for(int32 X=0;X<Fog.Columns;++X)
  {
   const int32 I[4]={Y*VC+X,Y*VC+X+1,(Y+1)*VC+X+1,(Y+1)*VC+X};
   if(Dark[I[0]]<=0&&Dark[I[1]]<=0&&Dark[I[2]]<=0&&Dark[I[3]]<=0)continue;
   if(!Visible(X,Y))continue;
   FVector2D P[4];FLinearColor Cs[4];for(int32 K=0;K<4;++K){P[K]=Screen[I[K]];Cs[K]=FogInk;Cs[K].A=Dark[I[K]];}
   AddQuad(Tris,P,Cs);
  }
  DrawTris(Canvas,Tris);
 }
 if(!bSectors||!PC->bShowSectorGrid) return;
 // Grid lines on the sector borders, following the ground in pieces of one fog cell.
 const FLinearColor Line(.85f,.9f,1.f,.32f);
 const double Size=BattleSectors::Size,HX=BattleSectors::Columns*Size*.5,HY=BattleSectors::Rows*Size*.5;
 auto OnScreen=[&](const FVector2D& S){return S.X>=0&&S.X<=W&&S.Y>=0&&S.Y<=H;};
 auto Piece=[&](FVector A,FVector B){FVector2D SA,SB;if(PC->ProjectWorldLocationToScreen(A,SA)&&PC->ProjectWorldLocationToScreen(B,SB)&&(OnScreen(SA)||OnScreen(SB)))DrawLine(SA.X,SA.Y,SB.X,SB.Y,Line,1.f);};
 auto Ground=[&](double X,double Y){return FVector(X,Y,BattleSectorView::GroundZ(GetWorld(),X,Y)+25);};
 const double StepLen=FBattleFogGrid::Cell;
 for(int32 C=0;C<=BattleSectors::Columns;++C){const double X=-HX+C*Size;for(double Y=-HY;Y<HY-1;Y+=StepLen)Piece(Ground(X,Y),Ground(X,FMath::Min(Y+StepLen,HY)));}
 for(int32 R=0;R<=BattleSectors::Rows;++R){const double Y=-HY+R*Size;for(double X=-HX;X<HX-1;X+=StepLen)Piece(Ground(X,Y),Ground(FMath::Min(X+StepLen,HX),Y));}
 // Sector names where a sector is large enough on the screen; the objective's letter beside it.
 for(int32 R=0;R<BattleSectors::Rows;++R)for(int32 C=0;C<BattleSectors::Columns;++C)
 {
  const FVector Mid=BattleSectors::Center({C,R});FVector2D A,B,S;
  if(!PC->ProjectWorldLocationToScreen(Ground(Mid.X-Size*.5,Mid.Y-Size*.5),A)||!PC->ProjectWorldLocationToScreen(Ground(Mid.X+Size*.5,Mid.Y+Size*.5),B)||
     !PC->ProjectWorldLocationToScreen(Ground(Mid.X,Mid.Y),S))continue;
  if(FVector2D::Distance(A,B)<70||!OnScreen(S))continue;
  const auto& Info=Sectors[R*BattleSectors::Columns+C];const int32 Status=BattleSectorView::StatusOf(Info);
  FString Text=BattleSectorView::SectorName({C,R});
  if(Info.Objective!=INDEX_NONE)Text+=FString::Printf(TEXT(" 拠点%c"),TCHAR('A'+Info.Objective));
  FLinearColor Color=Status>=1&&Status<=4?SectorTint(Status):FLinearColor(.85f,.9f,1.f);Color.A=.8f;
  DrawText(Text,Color,S.X-14,S.Y-8,GEngine->GetSmallFont(),.8f);
 }
 // The own team's front line (trial -BattleFrontLine=1, BattleFrontLine.cpp): the forward edge of each row's line sector, joined
 // across the rows. It is the team's own picture (own positions and known enemies), so it may be shown.
 if(const auto* FM=GetWorld()->GetSubsystem<UBattleMatch>();FM&&FM->FrontLine[0].Num()==BattleSectors::Rows)
 {
  const FLinearColor FrontInk(1.f,.55f,.15f,.9f);const double Sign=FM->AreaHome(0).X<0?1.:-1.;
  // By grid row (top to bottom); FrontLine is in the team's own rows, which run the other way for the team on the high X side.
  auto EdgeX=[&](int32 GridRow){const int32 Row=Sign>0?GridRow:BattleSectors::Rows-1-GridRow;return BattleSectors::Center(FM->FrontCell(0,FM->FrontLine[0][Row],Row)).X+Sign*Size*.5;};
  auto Thick=[&](FVector A,FVector B){FVector2D SA,SB;if(PC->ProjectWorldLocationToScreen(A,SA)&&PC->ProjectWorldLocationToScreen(B,SB)&&(OnScreen(SA)||OnScreen(SB)))DrawLine(SA.X,SA.Y,SB.X,SB.Y,FrontInk,3.f);};
  for(int32 R=0;R<BattleSectors::Rows;++R)
  {
   const double X=EdgeX(R),Y0=-HY+R*Size;
   for(double Y=Y0;Y<Y0+Size-1;Y+=StepLen)Thick(Ground(X,Y),Ground(X,FMath::Min(Y+StepLen,Y0+Size)));
   if(R+1<BattleSectors::Rows&&EdgeX(R+1)!=X){const double X1=EdgeX(R+1),Y=Y0+Size;for(double T=0;T<1;T+=.25)Thick(Ground(FMath::Lerp(X,X1,T),Y),Ground(FMath::Lerp(X,X1,T+.25),Y));}
  }
  // Front plan stage 7: each section's number and role at its middle row's line (the player's choice marked).
  if(FM->PlayerRolesOn(0))
  {
   const auto& LineFP=FM->FrontPlanState[0];
   for(int32 SecK=0;SecK<LineFP.Sections;++SecK)
   {
    int32 SecFirst=-1,SecLast=-1;for(int32 SR=0;SR<LineFP.SectionOfRow.Num();++SR)if(LineFP.SectionOfRow[SR]==SecK){if(SecFirst<0)SecFirst=SR;SecLast=SR;}
    if(SecFirst<0)continue;
    const int32 MidRow=(SecFirst+SecLast)/2;const FVector LineMid=BattleSectors::Center(FM->FrontCell(0,FM->FrontLine[0][MidRow],MidRow));
    FVector2D LabelAt;if(!PC->ProjectWorldLocationToScreen(Ground(LineMid.X+Sign*Size*.5,LineMid.Y),LabelAt)||!OnScreen(LabelAt))continue;
    const TCHAR SecRole=LineFP.Role.IsValidIndex(SecK)?LineFP.Role[SecK]:TEXT('K');
    const TCHAR* RoleJa=SecRole==TEXT('F')?TEXT("重点"):SecRole==TEXT('H')?TEXT("保持"):SecRole==TEXT('R')?TEXT("下がってよい"):TEXT("保つ");
    DrawText(FString::Printf(TEXT("区間%d %s%s"),SecK+1,RoleJa,FM->PlayerSectionRole(SecK)?TEXT("（指定）"):TEXT("")),FrontInk,LabelAt.X+6,LabelAt.Y-18,GEngine->GetSmallFont(),1.f);
   }
  }
 }
 // The sector under the pointer: a bright outline.
 FIntPoint Hover;
 if(HoveredSector(PC,Hover))
 {
  const FVector Mid=BattleSectors::Center(Hover);const FLinearColor Bright(1.f,.9f,.3f,.95f);
  const double S2=Size*.5;const FVector2D Q[4]={FVector2D(Mid.X-S2,Mid.Y-S2),FVector2D(Mid.X+S2,Mid.Y-S2),FVector2D(Mid.X+S2,Mid.Y+S2),FVector2D(Mid.X-S2,Mid.Y+S2)};
  for(int32 E=0;E<4;++E)for(int32 K=0;K<4;++K)
  {
   const FVector2D P0=FMath::Lerp(Q[E],Q[(E+1)%4],K/4.f),P1=FMath::Lerp(Q[E],Q[(E+1)%4],(K+1)/4.f);FVector2D SA,SB;
   if(PC->ProjectWorldLocationToScreen(Ground(P0.X,P0.Y),SA)&&PC->ProjectWorldLocationToScreen(Ground(P1.X,P1.Y),SB))DrawLine(SA.X,SA.Y,SB.X,SB.Y,Bright,2.f);
  }
 }
}

bool ABattleHUD::HoveredSector(ABattleController* PC,FIntPoint& Out) const
{
 if(!PC||!Canvas||!PC->IsTactical()||!BattleFieldLayout::Of(GetWorld()).bSectors) return false;
 FVector2D P; if(!SectorViewShotPrefix().IsEmpty()) P=FVector2D(Canvas->SizeX*.5f,Canvas->SizeY*.5f); else if(!PC->GetPointerPosition(P)) return false;
 const FVector2D View(Canvas->SizeX,Canvas->SizeY);
 const auto Map=FBattleMinimapLayout::Make(View,GetWorld());
 FVector World=FVector::ZeroVector;
 if(Map.Contains(P)) World=Map.ToWorld(P);
 else
 {
  // Not over the panels: the top bar, the squad list, the bottom panel, the minimap's frame.
  const auto List=FBattleSquadListLayout::Make(View,GetWorld());
  if(P.Y<100||P.Y>=FBattleCommandBarLayout::Make(View,GetWorld()).PanelTop||(P.X>=List.Origin.X&&P.Y<List.Origin.Y+List.Height)) return false;
  if(Map.Size.X>0&&P.X<Map.Origin.X+Map.Size.X+10&&P.Y>Map.Origin.Y-30) return false;
  FVector Origin,Dir; if(!PC->DeprojectScreenPositionToWorld(P.X,P.Y,Origin,Dir)||Dir.Z>-.01) return false;
  // The ray meets the ground: once at height 0, then again at the ground height found there.
  double Z=0; for(int32 K=0;K<2;++K){const double T=(Z-Origin.Z)/Dir.Z;World=Origin+Dir*T;Z=BattleSectorView::GroundZ(GetWorld(),World.X,World.Y);}
 }
 Out=BattleSectors::Cell(World); return BattleSectors::Valid(Out);
}

void ABattleHUD::DrawSectorPanel(ABattleController* PC)
{
 FIntPoint Cell; if(Sectors.IsEmpty()||!HoveredSector(PC,Cell)) return;
 const auto& Info=Sectors[Cell.Y*BattleSectors::Columns+Cell.X];
 const auto* Match=GetWorld()->GetSubsystem<UBattleMatch>(); const auto* Store=GetWorld()->GetSubsystem<UBattleState>();
 TArray<TPair<FString,FLinearColor>> Lines;
 const FLinearColor White(.94f,.97f,1.f),Muted(.62f,.7f,.77f);const int32 Status=BattleSectorView::StatusOf(Info);
 Lines.Emplace(FString::Printf(TEXT("区画 %s（%s）"),*BattleSectorView::SectorName(Cell),BattleSectorView::KindName(BattleFieldLayout::SectorKind(GetWorld(),Cell))),White);
 Lines.Emplace(FString::Printf(TEXT("状態：%s"),BattleSectorView::StatusName(Status)),SectorTint(Status));
 Lines.Emplace(FString::Printf(TEXT("自軍 %d人 ／ 敵 確認 %d・痕跡 %d"),Info.Own,Info.Seen,Info.Remembered),White);
 const bool bFoggy=Info.SightShare()<.5f;
 Lines.Emplace(FString::Printf(TEXT("視界 %d%%%s"),FMath::RoundToInt(Info.SightShare()*100),bFoggy?TEXT("（霧が多い：敵がいても見えない）"):TEXT("")),bFoggy?Muted:White);
 if(Info.Objective!=INDEX_NONE)
 {
  const int32 Holder=Match->bArea?Match->ObservedOwner(0,Info.Objective):Match->Objectives[Info.Objective].Owner;
  const TCHAR* Who=Holder==0?TEXT("自軍"):Holder==1?TEXT("敵軍"):Holder==-2?TEXT("不明"):TEXT("中立");
  Lines.Emplace(FString::Printf(TEXT("拠点 %c（%s）%s"),TCHAR('A'+Info.Objective),Who,Match->ObjectiveValue(Info.Objective)>1.f?TEXT(" 集積所"):TEXT("")),White);
 }
 Lines.Emplace(FString::Printf(TEXT("遮蔽位置 %d か所"),Info.Cover),Muted);
 if(!Info.Squads.IsEmpty())
 {
  FString Names;
  for(int32 I=0;I<Info.Squads.Num();++I)
  {
   if(I==4){Names+=FString::Printf(TEXT(" ほか%d"),Info.Squads.Num()-4);break;}
   if(const auto* S=Store->FindSquad(Info.Squads[I])){if(!Names.IsEmpty())Names+=TEXT("、");Names+=S->DisplayName;}
  }
  Lines.Emplace(FString::Printf(TEXT("向かう・維持：%s"),*Names),FLinearColor(.28f,.95f,.8f));
 }
 else Lines.Emplace(TEXT("命令中の部隊なし"),Muted);
 FVector2D P(Canvas->SizeX*.5f,Canvas->SizeY*.5f); if(SectorViewShotPrefix().IsEmpty()) PC->GetPointerPosition(P);
 const float Width=330,Row=18,Height=10+Row*Lines.Num();
 float X=P.X+22,Y=P.Y+18; if(X+Width>Canvas->SizeX-8)X=P.X-Width-14; if(Y+Height>Canvas->SizeY-8)Y=Canvas->SizeY-8-Height;
 DrawRect(FLinearColor(.025f,.04f,.065f,.9f),X,Y,Width,Height);DrawRect(SectorTint(Status),X,Y,3,Height);
 for(int32 I=0;I<Lines.Num();++I)DrawText(Lines[I].Key,Lines[I].Value,X+10,Y+5+I*Row,GEngine->GetSmallFont(),.85f);
}

// Minimap part of the overlay: sector tint, fog and grid, drawn over the ground and roads and under the marks.
void ABattleHUD::DrawMinimapOverlay(ABattleController* PC,const FBattleMinimapLayout& Map)
{
 const double Now=GetWorld()->GetRealTimeSeconds();
 auto Rect=[&](double X0,double Y0,double X1,double Y1,FLinearColor C,TArray<FCanvasUVTri>& Tris)
 {
  const FVector2D A=Map.ToScreen(FVector(X0,Y0,0)),B=Map.ToScreen(FVector(X1,Y1,0));
  const FVector2D Lo(FMath::Max(FMath::Min(A.X,B.X),Map.Origin.X),FMath::Max(FMath::Min(A.Y,B.Y),Map.Origin.Y));
  const FVector2D Hi(FMath::Min(FMath::Max(A.X,B.X),Map.Origin.X+Map.Size.X),FMath::Min(FMath::Max(A.Y,B.Y),Map.Origin.Y+Map.Size.Y));
  if(Hi.X<=Lo.X||Hi.Y<=Lo.Y)return;
  const FVector2D P[4]={Lo,FVector2D(Hi.X,Lo.Y),Hi,FVector2D(Lo.X,Hi.Y)};const FLinearColor Cs[4]={C,C,C,C};AddQuad(Tris,P,Cs);
 };
 const bool bSectors=BattleFieldLayout::Of(GetWorld()).bSectors&&!Sectors.IsEmpty();
 const double Size=BattleSectors::Size,HX=BattleSectors::Columns*Size*.5,HY=BattleSectors::Rows*Size*.5;
 if(bSectors)
 {
  auto& Tris=OverlayTris;Tris.Reset();
  for(int32 R=0;R<BattleSectors::Rows;++R)for(int32 C=0;C<BattleSectors::Columns;++C)
  {
   const int32 Status=BattleSectorView::StatusOf(Sectors[R*BattleSectors::Columns+C]);if(Status<1||Status>4)continue;
   FLinearColor Tint=SectorTint(Status);Tint.A=Status==4?.18f:.30f;
   Rect(-HX+C*Size,-HY+R*Size,-HX+(C+1)*Size,-HY+(R+1)*Size,Tint,Tris);
  }
  DrawTris(Canvas,Tris);
 }
 if(PC->bShowFog&&Fog.IsValid())
 {
  auto& Tris=OverlayTris;Tris.Reset();
  for(int32 Y=0;Y<Fog.Rows;++Y)for(int32 X=0;X<Fog.Columns;++X)
  {
   const uint8 State=Fog.State(Y*Fog.Columns+X,Now);if(!State)continue;
   FLinearColor C=FogInk;C.A=FogAlpha[State]+.1f;const FVector2D M=Fog.CellMin(X,Y);
   Rect(M.X,M.Y,M.X+FBattleFogGrid::Cell,M.Y+FBattleFogGrid::Cell,C,Tris);
  }
  DrawTris(Canvas,Tris);
 }
 if(!bSectors)return;
 const FLinearColor Line(.8f,.86f,.95f,.22f),Label(.7f,.76f,.84f,.9f);
 for(int32 C=0;C<=BattleSectors::Columns;++C){const FVector2D A=Map.ToScreen(FVector(-HX+C*Size,-HY,0)),B=Map.ToScreen(FVector(-HX+C*Size,HY,0));DrawLine(A.X,A.Y,B.X,B.Y,Line,1);}
 for(int32 R=0;R<=BattleSectors::Rows;++R){const FVector2D A=Map.ToScreen(FVector(-HX,-HY+R*Size,0)),B=Map.ToScreen(FVector(HX,-HY+R*Size,0));DrawLine(A.X,A.Y,B.X,B.Y,Line,1);}
 // Column letters down the left edge and row numbers along the bottom (every other one where they would crowd).
 const float CellPx=Map.Size.Y/BattleSectors::Columns;const int32 Every=CellPx<12?2:1;
 for(int32 C=0;C<BattleSectors::Columns;C+=Every){const FVector2D P=Map.ToScreen(BattleSectors::Center({C,0}));DrawText(FString::Printf(TEXT("%c"),TCHAR('A'+C)),Label,Map.Origin.X-12,P.Y-6,GEngine->GetSmallFont(),.6f);}
 for(int32 R=0;R<BattleSectors::Rows;R+=Every){const FVector2D P=Map.ToScreen(BattleSectors::Center({0,R}));DrawText(FString::Printf(TEXT("%d"),R+1),Label,P.X-3,Map.Origin.Y+Map.Size.Y+1,GEngine->GetSmallFont(),.6f);}
 FIntPoint Hover;
 if(HoveredSector(PC,Hover))
 {
  const FVector M=BattleSectors::Center(Hover);const FVector2D A=Map.ToScreen(M+FVector(-Size*.5,-Size*.5,0)),B=Map.ToScreen(M+FVector(Size*.5,Size*.5,0));
  const FLinearColor Bright(1.f,.9f,.3f);
  DrawLine(A.X,A.Y,B.X,A.Y,Bright,1.5f);DrawLine(B.X,A.Y,B.X,B.Y,Bright,1.5f);DrawLine(B.X,B.Y,A.X,B.Y,Bright,1.5f);DrawLine(A.X,B.Y,A.X,A.Y,Bright,1.5f);
 }
}
