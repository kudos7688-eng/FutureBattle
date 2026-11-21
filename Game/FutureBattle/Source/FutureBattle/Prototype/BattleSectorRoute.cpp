// Sector routes (trial -BattleSectorRoute=1, 2026-11-19, default 0): on a map with sectors (map H), an AI infantry squad that the
// road network would send to the next junction is sent to a sector instead - the junction's sector or one of its eight neighbours,
// whichever has the most cover positions while still bringing the squad closer to its goal. The squad then takes and holds that
// sector as a sector mission (cover positions inside it), and the next AI update sends it on to the next sector. The result is a
// march from sector to sector, each stop in the cover the parcel offers, instead of a walk along the road from junction to junction.
#include "BattleMatch.h"
#include "BattleCover.h"
#include "BattleSectors.h"
#include "BattleFieldLayout.h"
#include "BattleState.h"
#include "BattleSquadTactics.h"
#include "BattleLeaderRuntime.h"

bool UBattleMatch::SectorRouteOn()
{
	static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorRoute="),V);return V!=0;}();
	return bOn;
}

bool UBattleMatch::SectorCoverRouteOn()
{
	static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorCoverRoute="),V);return V!=0;}();
	return bOn;
}

bool UBattleMatch::DangerRouteOn()
{
	static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleDangerRoute="),V);return V!=0;}();
	return bOn;
}
void UBattleMatch::NoteSectorDanger(int32 Team,const FVector& At)
{
	const FIntPoint C=BattleSectors::Cell(At);if((Team!=0&&Team!=1)||!BattleSectors::Valid(C))return;
	const double Now=GetWorld()->GetTimeSeconds();auto& D=SectorDangerMap[Team].FindOrAdd(C,TPair<double,double>(0.,Now));
	D.Key=D.Key*FMath::Pow(.5,(Now-D.Value)/60.)+1.;D.Value=Now;
}
double UBattleMatch::SectorDanger(int32 Team,FIntPoint C) const
{
	if(!DangerRouteOn()||(Team!=0&&Team!=1))return 0.;const auto* D=SectorDangerMap[Team].Find(C);if(!D)return 0.;
	return D->Key*FMath::Pow(.5,(GetWorld()->GetTimeSeconds()-D->Value)/60.);
}
bool UBattleMatch::CloseInStage(int32 SID,const FVector& Leader,FVector& Stage)
{
	const double Now=GetWorld()->GetTimeSeconds();
	if(const auto* Kept=CloseIn.Find(SID))
	{
		if(Now<Kept->Value){Stage=BattleSectors::Center(Kept->Key);return true;}
		CloseIn.Remove(SID);
	}
	auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();FVector Threat;
	if(!Runtime || !Runtime->CloseInThreat(SID,Threat))return false;
	Runtime->ClearCloseIn(SID);
	const auto* S=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);if(!S)return false;
	const FIntPoint Here=BattleSectors::Cell(Leader);const double Base=FVector::Dist2D(Leader,Threat);
	double Best=-DBL_MAX;FIntPoint Pick(-1,-1);
	const int32 Turn=Runtime->SectorTurn(S->Team); // -BattleSectorSymmetric: the east team scans in its own frame
	for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
	{
		const FIntPoint C(Here.X+Turn*DX,Here.Y+Turn*DY);
		if((!DX&&!DY) || !BattleSectors::Valid(C))continue;
		const char K=BattleFieldLayout::SectorKind(GetWorld(),C);if(K=='R'||K=='M'||K=='L'||K=='F')continue;
		if(SectorFrontOn() && (S->Team==0||S->Team==1) && SectorState[S->Team].FindRef(C)==2)continue;
		const double D=FVector::Dist2D(BattleSectors::Center(C),Threat);
		if(D>Base-BattleSectors::Size*.5 || D<3000.)continue; // closer by half a sector; not onto the enemy
		const double CloseScore=SectorCoverCount(C)*.25-3.*FMath::Min(SectorDanger(S->Team,C),8.)-(K=='P'?6.:0.);
		if(CloseScore>Best){Best=CloseScore;Pick=C;}
	}
	if(Best==-DBL_MAX){GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("SectorCloseInNone"));return false;}
	CloseIn.Add(SID,TPair<FIntPoint,double>(Pick,Now+60.));
	GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("SectorCloseIn"));
	Stage=BattleSectors::Center(Pick);return true;
}

bool UBattleMatch::PinnedFlankStage(int32 SID,const FVector& Leader,FVector& Stage)
{
	const double Now=GetWorld()->GetTimeSeconds();
	if(const auto* Kept=PinnedFlank.Find(SID))
	{
		if(Now<Kept->Value && BattleSectors::Cell(Leader)!=Kept->Key){Stage=BattleSectors::Center(Kept->Key);return true;}
		PinnedFlank.Remove(SID);
	}
	auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();FVector Threat;
	if(!Runtime || !Runtime->PinnedThreat(SID,Threat))return false;
	Runtime->ClearPinned(SID);
	const FIntPoint Here=BattleSectors::Cell(Leader);
	const FVector Front=(Leader-Threat).GetSafeNormal2D();const double Base=FVector::Dist2D(Leader,Threat);
	double Best=-DBL_MAX;FIntPoint Pick(-1,-1);
	const auto* PinnedSquad=GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID);
	const int32 Turn=Runtime->SectorTurn(PinnedSquad?PinnedSquad->Team:-1); // -BattleSectorSymmetric
	for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
	{
		const FIntPoint C(Here.X+Turn*DX,Here.Y+Turn*DY);
		if((!DX&&!DY) || !BattleSectors::Valid(C) || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),C)))continue;
		const FVector P=BattleSectors::Center(C),V=P-Threat;const double D=FVector::Dist2D(P,Threat);
		if(D<Base-BattleSectors::Size || D>Base+BattleSectors::Size*.5)continue; // not much closer, not falling back
		const double Side=FMath::Abs(Front.X*V.Y-Front.Y*V.X)/BattleSectors::Size;
		if(Side<.8)continue;
		const char K=BattleFieldLayout::SectorKind(GetWorld(),C);
		const double FlankScore=12.*FMath::Min(Side,3.)+SectorCoverCount(C)*.25-(K=='F'?10.:0.)-3.*FMath::Min(SectorDanger(GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID)?GetWorld()->GetSubsystem<UBattleState>()->FindSquad(SID)->Team:-1,C),8.);
		if(FlankScore>Best){Best=FlankScore;Pick=C;}
	}
	if(Best==-DBL_MAX)return false;
	PinnedFlank.Add(SID,TPair<FIntPoint,double>(Pick,Now+40.));
	GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("SectorPinnedFlank"));
	Stage=BattleSectors::Center(Pick);return true;
}

int32 UBattleMatch::SectorCoverCount(FIntPoint Cell) const
{
	if(SectorCover.IsEmpty())
	{
		auto* Cover=GetWorld()->GetSubsystem<UBattleCover>();
		Cover->BuildSlots();
		for(const FVector& Slot:Cover->GetSlots())
			if(const FIntPoint C=BattleSectors::Cell(Slot);BattleSectors::Valid(C))++SectorCover.FindOrAdd(C);
		SectorCover.FindOrAdd(FIntPoint(-1,-1)); // built (a map without slots does not rebuild every call)
	}
	return SectorCover.FindRef(Cell);
}

bool UBattleMatch::SectorFlankOn()
{
	static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorFlank="),V);return V!=0;}();
	return bOn;
}

bool UBattleMatch::SectorFrontOn()
{
	static const bool bOn=[]{int32 V=0;FParse::Value(FCommandLine::Get(),TEXT("BattleSectorFront="),V);return V!=0;}();
	return bOn;
}

// Trial -BattleSectorFront=1: the team's picture of the sectors, every 2 s. A sector holds the team's own soldiers (own
// positions) and/or enemies the team knows of (the same observed positions the routes use, never the true ones). Enemy sectors
// hold known enemies and none of the team's soldiers; the front is the team's own sectors next to (8 neighbours) a sector with
// known enemies.
void UBattleMatch::UpdateSectorFront(int32 Team,const TArray<FVector>& KnownEnemies) const
{
	const double Now=GetWorld()->GetTimeSeconds();
	if(Team<0||Team>1||Now<SectorFrontAt[Team])return;
	SectorFrontAt[Team]=Now+2.;
	auto& State=SectorState[Team];State.Reset();SectorFrontCells[Team].Reset();
	const auto* Store=GetWorld()->GetSubsystem<UBattleState>();const auto* Runtime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
	for(const auto& Pair:Store->GetSquads())if(Pair.Value.Team==Team)
		for(int32 ID:Pair.Value.Members)if(const auto* U=Store->Find(ID);U&&U->IsAlive())
			if(const FIntPoint C=BattleSectors::Cell(Runtime->Position(ID));BattleSectors::Valid(C))State.FindOrAdd(C)|=1;
	for(const FVector& E:KnownEnemies)if(const FIntPoint C=BattleSectors::Cell(E);BattleSectors::Valid(C))State.FindOrAdd(C)|=2;
	int32 Own=0,Enemy=0,Contested=0;
	for(const auto& Pair:State)
	{
		Own+=Pair.Value==1;Enemy+=Pair.Value==2;Contested+=Pair.Value==3;
		if(!(Pair.Value&1))continue;
		bool bFront=false;
		for(int32 DY=-1;DY<=1&&!bFront;++DY)for(int32 DX=-1;DX<=1;++DX)
			if((DX||DY)&&(SectorState[Team].FindRef(Pair.Key+FIntPoint(DX,DY))&2)){bFront=true;break;}
		if(bFront||Pair.Value==3)SectorFrontCells[Team].Add(Pair.Key);
	}
	if(Now>=SectorFrontLogAt[Team])
	{
		SectorFrontLogAt[Team]=Now+30.;
		UE_LOG(LogTemp,Display,TEXT("BATTLE_SECTOR_FRONT t=%.0f team=%d own=%d enemy=%d contested=%d front=%d"),Now,Team,Own,Enemy,Contested,SectorFrontCells[Team].Num());
	}
}

bool UBattleMatch::SectorStage(const FBattleSquadState& S,const FVector& Leader,const FVector& Step,const FVector& Goal,const TArray<FVector>& KnownEnemies,FVector& Stage) const
{
	const FIntPoint Here=BattleSectors::Cell(Leader),StepCell=BattleSectors::Cell(Step),GoalCell=BattleSectors::Cell(Goal);
	if(!BattleSectors::Valid(StepCell))return false;
	const double Remaining=FVector::Dist2D(Leader,Goal);
	// Trial -BattleSectorFlank=1: enemies this team knows of within 200 m of the goal. Their front is taken to face the squad
	// (observed positions only; nothing is known of where they look), and stops beside that line are preferred.
	const bool bFinal=StepCell==GoalCell;
	const bool bFrontOn=SectorFrontOn()&&(S.Team==0||S.Team==1);
	if(bFrontOn)UpdateSectorFront(S.Team,KnownEnemies);
	FVector EnemyAt=FVector::ZeroVector;int32 Near=0;
	if(SectorFlankOn())for(const FVector& E:KnownEnemies)if(FVector::Dist2D(E,Goal)<=20000.){EnemyAt+=E;++Near;}
	if(Near)EnemyAt/=Near;
	const FVector Front=Near?(Leader-EnemyAt).GetSafeNormal2D():FVector::ZeroVector;
	// The last leg: without a flank to take (or already beside the goal) the squad goes straight into the goal sector.
	if(bFinal && (!Near || (FMath::Abs(Here.X-GoalCell.X)<=1 && FMath::Abs(Here.Y-GoalCell.Y)<=1)))return false;
	// Keep a stage the squad is still marching to while it lies beside the junction ahead (no new order on every update).
	if(S.bHasMission && S.bSectorMission && S.Order==EBattleOrder::AttackMove)
	{
		const FIntPoint Current=BattleSectors::Cell(S.MissionDestination);
		if(Current!=Here && FMath::Abs(Current.X-StepCell.X)<=1 && FMath::Abs(Current.Y-StepCell.Y)<=1)
		{Stage=BattleSectors::Center(Current);return true;}
	}
	double Best=-DBL_MAX;bool bFound=false;
	// -BattleSectorSymmetric: the east team scans in its own frame (turned 180 degrees), so ties between the two sectors beside
	// the line to a goal at a sector centre fall the same way for both teams.
	const auto* SymRuntime=GetWorld()->GetSubsystem<UBattleLeaderRuntime>();
	const int32 Turn=SymRuntime?SymRuntime->SectorTurn(S.Team):1;
	for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
	{
		const FIntPoint C(StepCell.X+Turn*DX,StepCell.Y+Turn*DY);
		if(!BattleSectors::Valid(C) || C==Here || BattleSectors::Impassable(BattleFieldLayout::SectorKind(GetWorld(),C)))continue; // no stop in the river (the kind of the map's own sector table)
		if(bFinal && C==GoalCell)continue; // the flank stop comes first
		// -BattleSectorFront: no stop inside an enemy sector short of the goal; the last stop before one is a front sector.
		const uint8 Held=bFrontOn?SectorState[S.Team].FindRef(C):0;
		if(Held==2 && C!=GoalCell)continue;
		const FVector P=BattleSectors::Center(C);
		// Progress: at least half a sector closer to the goal than the squad is now.
		const double Closer=Remaining-FVector::Dist2D(P,Goal);
		if(Closer<BattleSectors::Size*.5)continue;
		// Most cover first; a little for the junction's own sector (on the road) and for progress.
		double StageScore=SectorCoverCount(C)+(C==StepCell?4.:0.)+Closer/BattleSectors::Size;
		if(bFrontOn && SectorFrontCells[S.Team].Contains(C))StageScore+=8.; // gather at the front, in the most cover
		// -BattleSectorCoverRoute: an open sector within sight of a known enemy (150 m) is a poor stop on the way.
		if(SectorCoverRouteOn())
			if(const char K=BattleFieldLayout::SectorKind(GetWorld(),C);K=='F'||K=='P')
				for(const FVector& E:KnownEnemies)if(FVector::DistSquared2D(E,P)<FMath::Square(15000.)){StageScore-=K=='F'?15.:6.;break;}
		StageScore-=3.*FMath::Min(SectorDanger(S.Team,C),8.); // -BattleDangerRoute: sectors where the team was just hit
		if(Near)
		{
			// Sideways from the enemy's front counts up to three sectors; a stop straight in front of it is avoided.
			const FVector V=P-EnemyAt;
			const double Side=FMath::Abs(Front.X*V.Y-Front.Y*V.X)/BattleSectors::Size,Ahead=FVector::DotProduct(Front,V)/BattleSectors::Size;
			StageScore+=12.*FMath::Min(Side,3.)-(Ahead>0 && Side<1.?30.:0.);
		}
		if(StageScore>Best){Best=StageScore;Stage=P;bFound=true;}
	}
	if(bFound)GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(Near?TEXT("SectorFlankStage"):TEXT("SectorRouteStage"));
	if(bFound && bFrontOn && SectorFrontCells[S.Team].Contains(BattleSectors::Cell(Stage)))GetWorld()->GetSubsystem<UBattleSquadTactics>()->Event(TEXT("SectorFrontStage"));
	return bFound;
}
