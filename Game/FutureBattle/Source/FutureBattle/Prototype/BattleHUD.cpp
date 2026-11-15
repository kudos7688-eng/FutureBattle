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
#include "BattleCover.h"
#include "BattleMatch.h"
#include "BattleFieldLayout.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ABattleHUD::DrawHUD()
{
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
 Label(GetWorld()->GetSubsystem<UBattleMatch>()->bArea?FString(TEXT("AREA BATTLE / OBSERVED INFORMATION")):FString::Printf(TEXT("PROTOTYPE 01  /  %d-UNIT FIELD TEST"), (Mode->bLeaderCommand?GetWorld()->GetSubsystem<UBattleState>()->GetUnits().Num():Mode->Units.Num())), 35, 53, Muted, .9f);
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
	Label(PC->IsTactical() ? TEXT("TACTICAL COMMAND") : Controlled && !Controlled->UsesThirdPersonView() ? TEXT("DIRECT CONTROL / FPS") : TEXT("DIRECT CONTROL / TPS"), W * .4f, 30, Mint, 1.2f);
	Label(FString::Printf(TEXT("ALLY %02d   /   ENEMY %02d   /   SPOTTED %02d"), Allies, Enemies, Spotted), W * .4f, 54, White, .9f);
	Label(Mode->bCombatPaused ? TEXT("AI PAUSED [P]") : TEXT("AI ACTIVE [P]"), W - 165, 35, Mode->bCombatPaused ? FLinearColor::Yellow : Muted);
	if (const auto* SpeedPC = Cast<ABattleController>(GetOwningPlayerController()); SpeedPC && SpeedPC->IsTactical())
		Label(FString::Printf(TEXT("SPEED x%d [.]"), SpeedPC->GetGameSpeed()), W - 165, 55, SpeedPC->GetGameSpeed() > 1 ? FLinearColor::Yellow : Muted);
	const auto* Match = GetWorld()->GetSubsystem<UBattleMatch>();
	if (Match->IsEnabled())
	{
		const TCHAR* Phase = Match->Phase == EBattleMatchPhase::Deployment ? TEXT("DEPLOYMENT / ENTER: START") : Match->Phase == EBattleMatchPhase::Combat ? TEXT("BATTLE / REINFORCEMENTS") : TEXT("FINISHED / F5: RESTART");
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
  if(Match->bArea)Label(FString::Printf(TEXT("AREA / RESOURCES %d%s / HOME HP %.0f / ENEMY BASE LAST SEEN %s%s"),Match->Credits[0],*IncomeText,Match->Area.BaseHP[0],Match->Area.KnownEnemyBaseHP[0]<0?TEXT("UNKNOWN"):*FString::Printf(TEXT("%.0f"),Match->Area.KnownEnemyBaseHP[0]),
   BattleTactics::AreaHire().bEnabled?*FString::Printf(TEXT(" / [2] NEW SQUAD %d = SCORE %+d (%d/25)"),BattleTactics::AreaHire().Cost,10-BattleTactics::AreaHire().Cost,Match->ActiveSquads(0)):TEXT("")),28,90,Mint,.9f);
  else if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch) Label(FString::Printf(TEXT("%s / CREDITS %d / [1] %d [2] %d [3] %d / 10 soldiers"),Phase,Match->Credits[0],Match->CurrentSquadPrice(EBattleTraining::Recruit),Match->CurrentSquadPrice(EBattleTraining::Regular),Match->CurrentSquadPrice(EBattleTraining::Veteran)),28,90,Mint,.9f);
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(TEXT("500 SOLDIERS / THREE CONTROL POINTS / P: PAUSE OR RESUME"),28,90,Mint,.9f);
		else Label(FString::Printf(TEXT("%s   CREDITS %d   [1] RECRUIT %d   [2] REGULAR %d   [3] VETERAN %d / 5 units"), Phase, Match->Credits[0], UBattleMatch::SquadPrice(EBattleTraining::Recruit), UBattleMatch::SquadPrice(EBattleTraining::Regular), UBattleMatch::SquadPrice(EBattleTraining::Veteran)), 28, 90, Mint, .9f);
		DrawRect(Ink, 16, 112, 560, 51);
		const float Limit=BattleFieldLayout::IsSightField(GetWorld())?600.f:180.f;
  if(Match->bArea)Label(FString::Printf(TEXT("TIME %.0fs / BASE DESTRUCTION OR FINAL RESOURCE SCORE"),FMath::Max(0.f,Match->MatchTimeLimit()-Match->Elapsed)),28,119,White);
  else Label(FString::Printf(TEXT("CONTROL %.2f : %.2f / 100    TIME %.0fs"), Match->Score[0], Match->Score[1], FMath::Max(0.f, Limit - Match->Elapsed)), 28, 119, White);
  // War score trial: own score exactly; the opponent only as an estimate from own losses and observed ownership (its resources unknown).
  if(Match->bArea&&BattleWarScore::Tuning().bEnabled)Label(FString::Printf(TEXT("WAR SCORE %s / ENEMY EST %d + RESOURCES ?"),*Match->WarScoreBreakdown(0),Match->WarScoreEstimate(0)),28,141,Muted,.85f);
  else if(Match->bArea)
  {
   // Command UI top bar (2026-11-16): own final score as it stands, from own state and the points the own side observes as its own;
   // the opponent only from observed points (its living soldiers and resources are not shown).
   int32 Living=0;for(const auto& U:GetWorld()->GetSubsystem<UBattleState>()->GetUnits())Living+=U.Value.Team==0&&U.Value.IsAlive();
   int32 OwnPoints=0,EnemyPoints=0;for(int32 I=0;I<Match->Objectives.Num();++I){const int32 O=Match->ObservedOwner(0,I);OwnPoints+=O==0;EnemyPoints+=O==1;}
   DrawRect(Ink,576,112,260,51);
   Label(FString::Printf(TEXT("SCORE %d = LIVING %d + RESOURCES %d + POINTS %dx20"),Living+Match->Credits[0]+OwnPoints*20,Living,Match->Credits[0],OwnPoints),28,141,Mint,.85f);
   Label(FString::Printf(TEXT("ENEMY: POINTS %dx20 (observed)"),EnemyPoints),588,119,Muted,.85f);
   Label(TEXT("+ living / resources unknown"),588,141,Muted,.85f);
  }
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(FString::Printf(TEXT("Capture 8s / 1 point per second / First to 100 / Limit %.0fs"),Limit),28,141,Muted,.85f);
		else Label(TEXT("Capture 8s / Income +20 per 5s / Supply 1 per round / F8 save / F9 load"), 28, 141, Muted, .85f);
		DrawRect(Ink, 16, 165, 560, 26);
  if(Match->bArea)Label(TEXT("Home: replenish in 10s / 1 resource per completed soldier / F8 save / F9 load"),28,172,Mint,.85f);
  else if(GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->bMixedFastMatch)Label(TEXT("Home / owned point supply: 1 credit per round / F8 save / F9 load / strikes unavailable"),28,172,Mint,.85f);
  else if (BattleFieldLayout::IsLarge(GetWorld())) Label(TEXT("Fixed forces / no reinforcements, resupply or support strikes"),28,172,Mint,.85f);
		else Label(FString::Printf(TEXT("[B] SUPPORT 400 / %d left / rearm %.0fs / selected unit's known position"), 3 - Match->SupportUsed[0], Match->SupportCooldown[0]), 28, 172, Mint, .85f);
		for (const auto& Strike : Match->Strikes)
		{
			FVector2D Point;
			if (PC->ProjectWorldLocationToScreen(Strike.Position + FVector(0, 0, 100), Point) && Point.Y > 200 && Point.Y < H - 135)
			{
				DrawRect(Ink, Point.X - 75, Point.Y - 18, 150, 26);
				Label(FString::Printf(TEXT("INCOMING / %.1fs"), Strike.Remaining), Point.X - 65, Point.Y - 12, FLinearColor::Yellow);
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
    Label(Match->bArea?(Point.Owner==-2?FString::Printf(TEXT("%c / UNKNOWN"),'A'+Index):FString::Printf(TEXT("%c / %s / seen %.0fs"),'A'+Index,Point.Owner==0?TEXT("ALLY"):Point.Owner==1?TEXT("ENEMY"):TEXT("NEUTRAL"),Match->Area.SeenAt[Index])):FString::Printf(TEXT("%c / %s %.0f%%"), 'A' + Index, Point.bContested ? TEXT("CONTESTED") : Point.Owner == 0 ? TEXT("ALLY") : Point.Owner == 1 ? TEXT("ENEMY") : TEXT("NEUTRAL"), Point.CaptureProgress / 8.f * 100.f), Center.X - 42, Center.Y - 12, Color, .85f);
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
  {DrawRect(Ink,P.X-65,P.Y-16,190,26);Label(Team?TEXT("ENEMY BASE / PUBLIC LOCATION"):TEXT("HOME / REPLENISHMENT AREA"),P.X-60,P.Y-10,Team?FLinearColor(1,.35f,.25f):Mint,.85f);}
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
		Label(FString::Printf(TEXT("SELECTED SQUADS  /  MANUAL %d  /  AI %d"), Manual, Automatic), 28, 247, White, .95f);
		const bool bSelected = Manual + Automatic > 0;
		DrawRect(bSelected && Automatic == 0 ? FLinearColor(.12f,.38f,.32f,.98f) : FLinearColor(.10f,.15f,.20f,.98f), 28, 270, 200, 30);
		DrawRect(bSelected && Manual == 0 ? FLinearColor(.12f,.38f,.32f,.98f) : FLinearColor(.10f,.15f,.20f,.98f), 240, 270, 214, 30);
		Label(TEXT("[K] MANUAL COMMAND"), 40, 278, bSelected ? Mint : Muted, .9f);
		Label(TEXT("[J] DELEGATE TO AI"), 252, 278, bSelected ? Mint : Muted, .9f);
		Label(TEXT("Q: select all / Move or Hold takes command"), 28, 310, Muted, .85f);
  if (GetWorld()->GetSubsystem<UBattleSquadTactics>()->IsEnabled() && !PC->GetSelection().IsEmpty())
   if (const auto* Selected = PC->GetSelection()[0]->GetState())
    if (const auto* Squad = GetWorld()->GetSubsystem<UBattleState>()->FindSquad(Selected->SquadID))
    {
     const FString Movement=GetWorld()->GetSubsystem<UBattleLeaderRuntime>()->MovementLabel(Squad->SquadID);
     DrawRect(Ink, 16, 336, 450, Movement.IsEmpty()?79:98);
     Label(FString::Printf(TEXT("SQ %d  MISSION: %s"), Squad->SquadID, BattleOrderName(Squad->bHasMission ? Squad->MissionOrder : Squad->Order)), 28, 343, White, .85f);
     Label(FString::Printf(TEXT("TACTIC: %s"), BattleSquadTacticName(Squad->TacticalPlan.Action)), 28, 362, Mint, .85f);
     const auto* Sample = GetWorld()->GetSubsystem<UBattleSquadTactics>()->Find(Squad->SquadID);
     const FString Reason = !Squad->bPlayerDelegated ? TEXT("MANUAL / LOCAL AI") : Squad->TacticalPlan.Action != EBattleSquadTactic::Mission ? Squad->TacticalPlan.Reason : Sample ? Sample->Reason : TEXT("WAITING FOR ACTIVE SAMPLE");
     Label(Reason, 28, 382, Muted, .8f);
     if(!Movement.IsEmpty()) Label(Movement,28,402,Muted,.8f);
    }
	}
	DrawRect(Ink, 16, H - 128, W - 32, 112);
	const ABattleUnit* Focus = PC->IsTactical() ? (PC->GetSelection().IsEmpty() ? nullptr : PC->GetSelection()[0].Get()) : Cast<ABattleUnit>(PC->GetPawn());
	if (Focus && Focus->GetState())
	{
		const FBattleUnitState* S = Focus->GetState();
		Label(FString::Printf(TEXT("%s %02d / SQ %02d   HP %.0f   AMMO %02d / %02d   %s / %s   RETRY %d%s"), S->Weapon == EBattleWeapon::SupportGun ? TEXT("SUPPORT") : TEXT("RIFLE"), S->UnitID, S->SquadID, S->HP, S->Ammo, S->Reserve,
			BattleOrderName(S->Order), BattleActionName(S->Action), S->MoveRetries, S->ReloadEndTime > 0 ? TEXT("   RELOADING...") : TEXT("")), 34, H - 112, S->Action == EBattleAction::MoveFailed ? FLinearColor::Yellow : White, 1.05f);
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
						Label(FString::Printf(TEXT("LAST %02d / %.1fs / %.0f%% / %s %02d"), Contact.TargetID, GetWorld()->GetTimeSeconds() - Contact.LastSeenTime, Contact.Confidence * 100.f,
							Contact.bShared ? TEXT("SHARED BY") : TEXT("SEEN BY"), Contact.SourceUnitID), Point.X + 10, Point.Y, FLinearColor::Yellow, .8f);
					}
				}
			}
		DrawRect(Ink, W - 576, 112, 560, 27);
		Label(FString::Printf(TEXT("SIGHT %.0fm / %.0fdeg / %.1fs   SEEN %d  MEMORY %d  SHARED %d / F6 %s"), S->Perception.SightRange / 100.f, S->Perception.FieldOfView, S->Perception.DetectionSeconds, Seen, Remembered, Shared, PC->bShowContactDebug ? TEXT("ON") : TEXT("OFF")), W - 566, 119, White, .85f);
		DrawRect(Ink, W - 576, 141, 560, 27);
		FString CoverText = TEXT("COVER: no observed threat");
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
				CoverText = FString::Printf(TEXT("EXPOSED %d/5 vs %02d%s"), Exposure.VisibleSamples, Nearest->TargetID, Exposure.bFlanked ? TEXT(" / FLANKED") : TEXT(""));
			}
		}
		/* Suppression trial: the shown spread includes the same multiplier the shot uses. */ const auto* Leaders = GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); const float SuppressedScale = Leaders->SuppressionSpreadScale(S->UnitID, GetWorld()->GetTimeSeconds());
		if (SuppressedScale > 1.f) CoverText = FString::Printf(TEXT("SUPPRESSED %d%%  %s"), FMath::RoundToInt(Leaders->Suppression(S->UnitID, GetWorld()->GetTimeSeconds()) * 100.f), *CoverText);
		const float Steady = S->AimStableSince < 0. ? 0.f : static_cast<float>(GetWorld()->GetTimeSeconds() - S->AimStableSince);
		Label(FString::Printf(TEXT("AIM %.1fs   SPREAD %.2fdeg   %s"), FMath::Min(Steady, GetDefault<UBattleTuning>()->AimSettleSeconds), BattleTactics::SpreadDegrees(*S, GetWorld()->GetTimeSeconds(), SuppressedScale), *CoverText), W - 566, 148, White, .85f);
		DrawRect(Ink, W - 576, 170, 560, 27);
		Label(S->DecisionReason.IsEmpty() ? TEXT("PLAN: FOLLOW ORDER") : FString::Printf(TEXT("PLAN: %s / SLOT %d"), *S->DecisionReason, S->CoverSlot), W - 566, 177, Mint, .85f);
		DrawRect(Ink, W - 576, 199, 560, 27);
		const FString RoleText = S->Role == EBattleRole::Independent ? TEXT("INDEPENDENT") : FString::Printf(TEXT("%s / FOCUS %02d"), BattleRoleName(S->Role), S->CoordinatedTarget);
		Label(FString::Printf(TEXT("%s / TEST COST %d / ROLE: %s"), BattleTrainingName(S->Training), S->HireCost, *RoleText), W - 566, 206, White, .85f);
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
	else Label(TEXT("SELECT A FRIENDLY UNIT TO BEGIN"), 34, H - 112, White, 1.2f);
	if (PC->IsTactical())
	{
		Label(TEXT("LMB/drag: select   RMB: move   T then RMB: attack move   G then RMB: defend   H: hold   Q: all   E/Tab: TPS"), 34, H - 80, Mint);
		Label(FString::Printf(TEXT("WASD: pan   Wheel: zoom   P: pause AI   F5: reset   F6: memory   %s      SELECTED %02d"), Match->IsEnabled() ? TEXT("1/2/3: recruit") : TEXT("F7: training preset"), PC->GetSelection().Num()), 34, H - 53, Muted, .9f);
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
	if (PC->NoticeUntil > GetWorld()->GetTimeSeconds()) Label(PC->Notice, 35, PC->CanShowDelegation() ? H - 156 : Match->IsEnabled() ? 235 : 96, Mint, 1.1f);
	if (PC->IsTactical() && PC->PendingOrder != EBattleOrder::Move)
		Label(FString::Printf(TEXT("%s: RIGHT-CLICK DESTINATION / H TO CANCEL"), BattleOrderName(PC->PendingOrder)), 35, 148, FLinearColor::Yellow);
	if (!Match->IsEnabled() && GetWorld()->GetTimeSeconds() < Mode->CombatStartsAt)
		Label(FString::Printf(TEXT("DEPLOYMENT / AI ADVANCE IN %.0f"), FMath::CeilToFloat(Mode->CombatStartsAt - GetWorld()->GetTimeSeconds())), 35, 123, White);
	if (Mode->bBattleEnded)
	{
		DrawRect(Ink, W * .25f, H * .35f, W * .5f, 100);
		Label(Mode->Result, W * .25f + 25, H * .35f + 16, Mint, 1.8f);
		Label(TEXT("F5 TO REDEPLOY"), W * .25f + 25, H * .35f + 60, White);
		// War score trial: both teams' breakdown is disclosed once the battle has ended.
		if (Match->bArea && BattleWarScore::Tuning().bEnabled)
		{
			DrawRect(Ink, W * .25f, H * .35f + 104, W * .5f, 58);
			Label(FString::Printf(TEXT("YOU    %s"), *Match->WarScoreBreakdown(0)), W * .25f + 25, H * .35f + 112, Mint);
			Label(FString::Printf(TEXT("ENEMY  %s"), *Match->WarScoreBreakdown(1)), W * .25f + 25, H * .35f + 136, White);
		}
	}
 DrawSquadList();
 DrawMinimap();
 DrawNoticeLog();
}

void ABattleHUD::DrawMinimap()
{
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 const auto Map=FBattleMinimapLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld()); if(Map.Size.X<=0) return;
 const FLinearColor Ink(.025f,.04f,.065f,.94f), Ground(.07f,.1f,.13f,.96f), Mint(.28f,.95f,.8f), Enemy(1,.35f,.25f), Stale(.95f,.65f,.3f,.75f), White(.94f,.97f,1.f), Muted(.45f,.52f,.58f);
 auto Dot=[&](FVector World,float Size,FLinearColor Color){const FVector2D P=Map.ToScreen(World);if(Map.Contains(P))DrawRect(Color,P.X-Size*.5f,P.Y-Size*.5f,Size,Size);};
 auto Frame=[&](FVector2D A,FVector2D B,FLinearColor Color)
 {DrawLine(A.X,A.Y,B.X,A.Y,Color,1);DrawLine(B.X,A.Y,B.X,B.Y,Color,1);DrawLine(B.X,B.Y,A.X,B.Y,Color,1);DrawLine(A.X,B.Y,A.X,A.Y,Color,1);};
 DrawRect(Ink,Map.Origin.X-5,Map.Origin.Y-24,Map.Size.X+10,Map.Size.Y+29);
 DrawText(TEXT("MAP / observed only"),Muted,Map.Origin.X,Map.Origin.Y-20,GEngine->GetSmallFont(),.8f);
 DrawRect(Ground,Map.Origin.X,Map.Origin.Y,Map.Size.X,Map.Size.Y);
 // Map E: the city block outline (public map knowledge).
 if(BattleFieldLayout::IsUrban(GetWorld())) Frame(Map.ToScreen(FVector(25000,-15000,0)),Map.ToScreen(FVector(-25000,15000,0)),Muted);
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
 // Enemies: only contacts held by the player's side. Seen now = solid; remembered = dim, at the last known position.
 TMap<int32,const FBattleContact*> Known;
 auto* Sense=GetWorld()->GetSubsystem<UBattlePerception>();
 for(const auto& Pair:Store->GetUnits()) if(Pair.Value.Team==0 && Pair.Value.IsAlive())
  if(const auto* Contacts=Sense->GetContacts(Pair.Key)) for(const auto& C:*Contacts)
  {
   const FBattleContact*& Best=Known.FindOrAdd(C.Key);
   if(!Best || (C.Value.bVisible && !Best->bVisible) || (C.Value.bVisible==Best->bVisible && C.Value.LastSeenTime>Best->LastSeenTime)) Best=&C.Value;
  }
 for(const auto& Pair:Known) if(!Pair.Value->bVisible && Pair.Value->Confidence>.35f) Dot(Pair.Value->LastKnownPosition,3,Stale);
 for(const auto& Pair:Known) if(Pair.Value->bVisible) Dot(Pair.Value->LastKnownPosition,4,Enemy);
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
 auto* L=GetWorld()->GetSubsystem<UBattleLeaderRuntime>(); if(!L->IsEnabled()) return;
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 PC->ScrollSquadList(0); const auto Layout=FBattleSquadListLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY));
 if(Layout.VisibleRows<=0) return;
 const auto& Rows=L->Summaries(); int32 Active=0; for(const auto& S:Rows) if(S.bActive) ++Active;
 DrawRect(FLinearColor(.025f,.04f,.065f,.96f),Layout.Origin.X,Layout.Origin.Y,Layout.Width,Layout.Height);
 DrawText(FString::Printf(TEXT("自軍部隊　稼働 %d / 停止 %d"),Active,Rows.Num()-Active),FLinearColor::White,Layout.Origin.X+8,Layout.Origin.Y+5,GEngine->GetSmallFont(),1);
 DrawText(TEXT("クリックで選択 → Eで操作"),FLinearColor(.7f,.7f,.7f),Layout.Origin.X+8,Layout.Origin.Y+24,GEngine->GetSmallFont(),.85);
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
   const FString Action=Mission?FString(BattleOrderName(Squad->Order)):FString::Printf(TEXT("%s %.0fs"),BattleSquadTacticName(Plan.Action),Plan.Age);
   DrawText(FString::Printf(TEXT("%s / %.0fm%s"),*Action,FVector::Dist2D(L->Position(S.LeaderUnitID),Goal)/100.,*Link),Color,Layout.Origin.X+9,Y+23,GEngine->GetSmallFont(),.8f);
   const auto* Sample=GetWorld()->GetSubsystem<UBattleSquadTactics>()->Find(S.SquadID);
   int32 Waiting=0;for(int32 ID:Squad->Members)if(const auto* Body=L->GetBodies().Find(ID))Waiting+=Body->bTrafficWait;
   FString Reason=!S.bActive?Squad->StopReason.ToString():!Squad->bPlayerDelegated?TEXT("MANUAL COMMAND"):
    Waiting>0?FString::Printf(TEXT("TRAFFIC WAIT %d"),Waiting):!Mission?Plan.Reason:Sample?Sample->Reason:TEXT("WAITING FOR REPORT");
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
 auto* PC=Cast<ABattleController>(PlayerOwner); if(!PC || !Canvas || !PC->IsTactical()) return;
 const auto Notices=PC->VisibleNotices(); if(Notices.IsEmpty()) return;
 const auto Layout=FBattleNoticeLayout::Make(FVector2D(Canvas->SizeX,Canvas->SizeY),GetWorld(),Notices.Num()); if(Layout.Rows<=0) return;
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
