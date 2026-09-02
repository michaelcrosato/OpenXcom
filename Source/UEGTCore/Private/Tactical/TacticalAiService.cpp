#include "Tactical/TacticalAiService.h"

#include "Tactical/TacticalCombatService.h"
#include "Tactical/TacticalNavigationService.h"

namespace TacticalAiPrivate
{
	struct FPolicy
	{
		int32 VisionRangeModifier = 0;
		int32 RetreatHealthPercent = 30;
		int32 MinimumHitChance = 25;
		int32 MovementBudgetPercent = 75;
		int32 WoundedTargetWeight = 4;
	};

	void AddDiagnostic(FTacticalAiDecision& Decision, const FName Code, FString Message)
	{
		FTacticalAiDiagnostic& Diagnostic = Decision.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool GetPolicy(const ECampaignDifficulty Difficulty, FPolicy& OutPolicy)
	{
		switch (Difficulty)
		{
		case ECampaignDifficulty::Cadet:
			OutPolicy = { -2, 45, 40, 60, 0 };
			return true;
		case ECampaignDifficulty::Standard:
			OutPolicy = { 0, 30, 25, 75, 4 };
			return true;
		case ECampaignDifficulty::Veteran:
			OutPolicy = { 2, 20, 10, 90, 8 };
			return true;
		case ECampaignDifficulty::Apex:
			OutPolicy = { 4, 10, 5, 100, 12 };
			return true;
		default:
			return false;
		}
	}

	FName PosturePolicyId(const ETacticalAiPosture Posture)
	{
		switch (Posture)
		{
		case ETacticalAiPosture::Assault:
			return FName(TEXT("ai.posture.assault"));
		case ETacticalAiPosture::SignalPressure:
			return FName(TEXT("ai.posture.signal-pressure"));
		case ETacticalAiPosture::ObjectivePush:
			return FName(TEXT("ai.posture.objective-push"));
		case ETacticalAiPosture::Sentinel:
			return FName(TEXT("ai.posture.sentinel"));
		default:
			return FName(TEXT("ai.posture.assault"));
		}
	}

	bool IsKnownPosture(const ETacticalAiPosture Posture)
	{
		return Posture == ETacticalAiPosture::Assault
			|| Posture == ETacticalAiPosture::SignalPressure
			|| Posture == ETacticalAiPosture::ObjectivePush
			|| Posture == ETacticalAiPosture::Sentinel;
	}

	const FTacticalUnitState* FindUnit(const FTacticalBattleState& Battle, const FGuid UnitId)
	{
		return Battle.Units.FindByPredicate(
			[&UnitId](const FTacticalUnitState& Unit) { return Unit.UnitId == UnitId; });
	}

	bool GuidComesFirst(const FGuid& Left, const FGuid& Right)
	{
		return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
	}

	int32 CeilDistance(
		const int32 OriginX,
		const int32 OriginY,
		const int32 OriginZ,
		const int32 TargetX,
		const int32 TargetY,
		const int32 TargetZ)
	{
		const int32 DeltaX = TargetX - OriginX;
		const int32 DeltaY = TargetY - OriginY;
		const int32 DeltaZ = (TargetZ - OriginZ) * 2;
		const int32 DistanceSquared = DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ;
		for (int32 Distance = 0; Distance <= 128; ++Distance)
		{
			if (Distance * Distance >= DistanceSquared)
			{
				return Distance;
			}
		}
		return MAX_int32;
	}

	int32 CoverAt(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const int32 X,
		const int32 Y,
		const int32 Z)
	{
		int32 Cover = 0;
		static constexpr int32 OffsetX[] = { -1, 1, 0, 0 };
		static constexpr int32 OffsetY[] = { 0, 0, -1, 1 };
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			const int32 NeighborX = X + OffsetX[Direction];
			const int32 NeighborY = Y + OffsetY[Direction];
			if (!Battle.IsWithinGrid(NeighborX, NeighborY, Z))
			{
				continue;
			}
			const FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(NeighborX, NeighborY, Z)];
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			if (Terrain != nullptr && Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen)
			{
				Cover = FMath::Max(Cover, Terrain->CoverPercent);
			}
		}
		return Cover;
	}

	bool CanSeeFrom(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const int32 X,
		const int32 Y,
		const int32 Z,
		const FTacticalUnitState& Target)
	{
		return FTacticalNavigationService::HasLineOfSight(
			Battle, Rules, X, Y, Target.X, Target.Y, Z, Target.Z)
			&& FTacticalNavigationService::ComputeSmokeObscuration(
				Battle, X, Y, Target.X, Target.Y, Z, Target.Z) < 60;
	}

	int32 MinimumHostileDistance(
		const int32 X,
		const int32 Y,
		const int32 Z,
		const TArray<const FTacticalUnitState*>& Hostiles)
	{
		int32 Distance = MAX_int32;
		for (const FTacticalUnitState* Hostile : Hostiles)
		{
			Distance = FMath::Min(Distance, CeilDistance(X, Y, Z, Hostile->X, Hostile->Y, Hostile->Z));
		}
		return Distance;
	}

	bool AnyHostileVisibleFrom(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const int32 X,
		const int32 Y,
		const int32 Z,
		const TArray<const FTacticalUnitState*>& Hostiles)
	{
		return Hostiles.ContainsByPredicate(
			[&Battle, &Rules, X, Y, Z](const FTacticalUnitState* Hostile)
			{
				return CanSeeFrom(Battle, Rules, X, Y, Z, *Hostile);
			});
	}

	int64 ScorePosition(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const FTacticalUnitRule& UnitRule,
		const FTacticalReachableCell& Cell,
		const FTacticalUnitState& PrimaryTarget,
		const TArray<const FTacticalUnitState*>& Hostiles,
		const int32 RemainingActionPoints,
		const bool bWithdraw)
	{
		const FTacticalCellState& TerrainCell = Battle.Cells[Cell.CellIndex];
		const int32 Cover = CoverAt(Battle, Rules, Cell.X, Cell.Y, Cell.Z);
		const int64 HazardPenalty = static_cast<int64>(TerrainCell.Fire) * 500
			+ static_cast<int64>(TerrainCell.Smoke) * (bWithdraw ? 1 : 10);
		if (bWithdraw)
		{
			const int32 Distance = MinimumHostileDistance(Cell.X, Cell.Y, Cell.Z, Hostiles);
			const bool bExposed = AnyHostileVisibleFrom(Battle, Rules, Cell.X, Cell.Y, Cell.Z, Hostiles);
			return static_cast<int64>(Distance) * 10000
				+ static_cast<int64>(Cover) * 30
				+ (bExposed ? 0 : 5000)
				- HazardPenalty
				- Cell.TotalCost;
		}

		const int32 Distance = CeilDistance(
			Cell.X, Cell.Y, Cell.Z, PrimaryTarget.X, PrimaryTarget.Y, PrimaryTarget.Z);
		const bool bHasLineOfSight = CanSeeFrom(Battle, Rules, Cell.X, Cell.Y, Cell.Z, PrimaryTarget);
		const bool bCanAttackAfterMoving = bHasLineOfSight
			&& Distance > 0 && Distance <= UnitRule.AttackRange
			&& RemainingActionPoints - Cell.TotalCost >= UnitRule.AttackActionPointCost;
		return -static_cast<int64>(Distance) * 10000
			+ (bCanAttackAfterMoving ? 200000 : (bHasLineOfSight ? 5000 : 0))
			+ static_cast<int64>(Cell.Z - PrimaryTarget.Z) * 250
			+ static_cast<int64>(Cover) * 10
			- HazardPenalty
			- Cell.TotalCost;
	}

	int32 ClampUtility(const int64 Utility)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Utility, MIN_int32, MAX_int32));
	}
}

FName FTacticalAiService::GetPosturePolicyId(const ETacticalAiPosture Posture)
{
	return TacticalAiPrivate::PosturePolicyId(Posture);
}

bool FTacticalAiDecision::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalAiDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FTacticalAiDecision FTacticalAiService::ChooseAction(
	const FTacticalBattleState& Battle,
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FGuid UnitId)
{
	using namespace TacticalAiPrivate;

	FTacticalAiDecision Decision;
	Decision.UnitId = UnitId;
	if (Battle.Phase != ETacticalBattlePhase::AdversaryTurn || Battle.ActiveTeam != ETacticalTeam::Adversary)
	{
		AddDiagnostic(Decision, TEXT("inactive_tactical_ai"), TEXT("Tactical AI can choose actions only during an active adversary turn."));
		return Decision;
	}
	const FTacticalUnitState* Unit = FindUnit(Battle, UnitId);
	if (Unit == nullptr)
	{
		AddDiagnostic(Decision, TEXT("unknown_tactical_unit"), TEXT("Tactical AI references an unknown unit."));
		return Decision;
	}
	Decision.DestinationX = Unit->X;
	Decision.DestinationY = Unit->Y;
	Decision.DestinationZ = Unit->Z;
	Decision.DesiredStance = Unit->Stance;
	if (Unit->Team != ETacticalTeam::Adversary || Unit->CurrentHealth <= 0 || Unit->bExtracted)
	{
		AddDiagnostic(Decision, TEXT("invalid_tactical_ai_unit"), TEXT("Tactical AI requires a living, deployed adversary unit."));
		return Decision;
	}
	const FTacticalUnitRule* UnitRule = Rules.TacticalUnits.Find(Unit->SourceRuleId);
	if (UnitRule == nullptr)
	{
		AddDiagnostic(Decision, TEXT("unknown_tactical_ai_rule"), TEXT("Tactical AI unit has no resolved adversary rule."));
		return Decision;
	}
	FPolicy Policy;
	if (!GetPolicy(Campaign.Difficulty, Policy))
	{
		AddDiagnostic(Decision, TEXT("invalid_campaign_difficulty"), TEXT("Tactical AI cannot resolve an unsupported campaign difficulty."));
		return Decision;
	}

	const FTacticalVisibilityResult Visibility = FTacticalNavigationService::ComputeTeamVisibility(
		Battle, Rules, ETacticalTeam::Adversary, Policy.VisionRangeModifier);
	if (!Visibility.bSucceeded)
	{
		for (const FTacticalNavigationDiagnostic& Diagnostic : Visibility.Diagnostics)
		{
			AddDiagnostic(Decision, Diagnostic.Code, Diagnostic.Message);
		}
		return Decision;
	}
	TArray<const FTacticalUnitState*> Hostiles;
	for (const FTacticalUnitState& Candidate : Battle.Units)
	{
		if (Candidate.Team == ETacticalTeam::Player && Candidate.CurrentHealth > 0 && !Candidate.bExtracted
			&& Visibility.IsUnitVisible(Candidate.UnitId))
		{
			Hostiles.Add(&Candidate);
		}
	}
	Hostiles.Sort(
		[](const FTacticalUnitState& Left, const FTacticalUnitState& Right)
		{
			return GuidComesFirst(Left.UnitId, Right.UnitId);
		});
	Decision.PerceivedHostileCount = Hostiles.Num();
	Decision.bSucceeded = true;
	const FTacticalMissionRule* Mission = Rules.TacticalMissions.Find(Battle.MissionRuleId);
	Decision.Posture = Mission != nullptr && IsKnownPosture(Mission->AiPosture)
		? Mission->AiPosture
		: ETacticalAiPosture::Assault;
	const FTacticalObjectiveState* ControlObjective = Battle.Objectives.FindByPredicate(
		[](const FTacticalObjectiveState& Objective)
		{
			return Objective.Type == ETacticalObjectiveType::Control
				&& Objective.Status == ETacticalObjectiveStatus::Active;
		});
	if (Mission != nullptr && ControlObjective != nullptr && Unit->RemainingActionPoints > 0)
	{
		Decision.ObjectiveId = ControlObjective->ObjectiveId;
		const int32 ObjectiveDistance = FMath::Abs(Unit->X - ControlObjective->X)
			+ FMath::Abs(Unit->Y - ControlObjective->Y)
			+ FMath::Abs(Unit->Z - ControlObjective->Z);
		if (ObjectiveDistance <= 1 && Unit->RemainingActionPoints >= Mission->ObjectiveActionPointCost)
		{
			Decision.Goal = ETacticalAiGoal::ControlObjective;
			Decision.ActionType = ETacticalAiActionType::InteractObjective;
			Decision.DestinationX = ControlObjective->X;
			Decision.DestinationY = ControlObjective->Y;
			Decision.DestinationZ = ControlObjective->Z;
			Decision.UtilityScore = 100000
				+ (ControlObjective->AdversaryInteractions - ControlObjective->CompletedInteractions) * 1000;
			return Decision;
		}
		if (Hostiles.IsEmpty() || Decision.Posture == ETacticalAiPosture::ObjectivePush)
		{
			const int32 MovementBudget = FMath::Clamp(
				Unit->RemainingActionPoints * Policy.MovementBudgetPercent / 100,
				1,
				Unit->RemainingActionPoints);
			const FTacticalReachabilityResult Reachability = FTacticalNavigationService::ComputeReachableCells(
				Battle, Rules, UnitId, MovementBudget);
			if (!Reachability.bSucceeded)
			{
				Decision.bSucceeded = false;
				for (const FTacticalNavigationDiagnostic& Diagnostic : Reachability.Diagnostics)
				{
					AddDiagnostic(Decision, Diagnostic.Code, Diagnostic.Message);
				}
				return Decision;
			}
			const FTacticalReachableCell* BestCell = nullptr;
			int64 BestScore = -static_cast<int64>(ObjectiveDistance) * 10000;
			for (const FTacticalReachableCell& Candidate : Reachability.Cells)
			{
				if (Candidate.TotalCost <= 0)
				{
					continue;
				}
				const FTacticalCellState& CandidateCell = Battle.Cells[Candidate.CellIndex];
				const int32 Distance = FMath::Abs(Candidate.X - ControlObjective->X)
					+ FMath::Abs(Candidate.Y - ControlObjective->Y)
					+ FMath::Abs(Candidate.Z - ControlObjective->Z);
				const int64 Score = -static_cast<int64>(Distance) * 10000
					- static_cast<int64>(CandidateCell.Fire) * 500
					- static_cast<int64>(CandidateCell.Smoke) * 10
					- Candidate.TotalCost;
				if (Score > BestScore
					|| (Score == BestScore && BestCell != nullptr
						&& (Candidate.TotalCost < BestCell->TotalCost
							|| (Candidate.TotalCost == BestCell->TotalCost && Candidate.CellIndex < BestCell->CellIndex))))
				{
					BestCell = &Candidate;
					BestScore = Score;
				}
			}
			if (BestCell != nullptr)
			{
				Decision.Goal = ETacticalAiGoal::ControlObjective;
				Decision.ActionType = ETacticalAiActionType::Move;
				Decision.DestinationX = BestCell->X;
				Decision.DestinationY = BestCell->Y;
				Decision.DestinationZ = BestCell->Z;
				Decision.MovementCost = BestCell->TotalCost;
				Decision.UtilityScore = ClampUtility(BestScore + static_cast<int64>(ObjectiveDistance) * 10000);
				return Decision;
			}
		}
	}
	if (Hostiles.IsEmpty() || Unit->RemainingActionPoints <= 0)
	{
		return Decision;
	}

	const bool bWithdraw = Unit->CurrentHealth * 100 <= Unit->MaxHealth * Policy.RetreatHealthPercent
		|| Unit->CurrentMorale <= FMath::Max(10, Unit->MaxMorale / 5)
		|| Unit->Suppression >= 80;
	const FTacticalUnitState* PrimaryTarget = nullptr;
	int64 BestPursuitScore = MIN_int64;
	for (const FTacticalUnitState* Hostile : Hostiles)
	{
		const int32 Distance = CeilDistance(Unit->X, Unit->Y, Unit->Z, Hostile->X, Hostile->Y, Hostile->Z);
		const int64 PursuitScore = -static_cast<int64>(Distance) * 1000
			+ static_cast<int64>(Hostile->MaxHealth - Hostile->CurrentHealth) * Policy.WoundedTargetWeight;
		if (PrimaryTarget == nullptr || PursuitScore > BestPursuitScore
			|| (PursuitScore == BestPursuitScore && GuidComesFirst(Hostile->UnitId, PrimaryTarget->UnitId)))
		{
			PrimaryTarget = Hostile;
			BestPursuitScore = PursuitScore;
		}
	}
	check(PrimaryTarget != nullptr);
	Decision.TargetUnitId = PrimaryTarget->UnitId;

	if (!bWithdraw)
	{
		const FTacticalUnitState* AttackTarget = nullptr;
		FTacticalAttackPreview BestAttack;
		int64 BestAttackScore = MIN_int64;
		const FTacticalUnitState* SignalTarget = nullptr;
		FTacticalSignalPreview BestSignal;
		int64 BestSignalScore = MIN_int64;
		for (const FTacticalUnitState* Hostile : Hostiles)
		{
			const FTacticalAttackPreview Preview = FTacticalCombatService::PreviewUnitAttack(
				Battle, Campaign, Rules, UnitId, Hostile->UnitId, NAME_None);
			if (Preview.bSucceeded && Preview.HitChance >= Policy.MinimumHitChance)
			{
				const int64 AttackScore = static_cast<int64>(Preview.HitChance) * 1000
					+ static_cast<int64>(FMath::Min(Preview.AttackPower, Hostile->CurrentHealth)) * 100
					+ static_cast<int64>(Hostile->MaxHealth - Hostile->CurrentHealth) * Policy.WoundedTargetWeight;
				if (AttackTarget == nullptr || AttackScore > BestAttackScore
					|| (AttackScore == BestAttackScore && GuidComesFirst(Hostile->UnitId, AttackTarget->UnitId)))
				{
					AttackTarget = Hostile;
					BestAttack = Preview;
					BestAttackScore = AttackScore;
				}
			}
			const FTacticalSignalPreview Signal = FTacticalCombatService::PreviewSignalProjection(
				Battle, Campaign, Rules, UnitId, Hostile->UnitId);
			if (Signal.bSucceeded && Signal.HitChance >= Policy.MinimumHitChance)
			{
				const int64 SignalScore = static_cast<int64>(Signal.HitChance) * 1000
					+ static_cast<int64>(Signal.MoraleDamage) * 180
					+ static_cast<int64>(Signal.SuppressionGain) * 120
					+ static_cast<int64>(Hostile->MaxMorale - Hostile->CurrentMorale) * 30;
				if (SignalTarget == nullptr || SignalScore > BestSignalScore
					|| (SignalScore == BestSignalScore && GuidComesFirst(Hostile->UnitId, SignalTarget->UnitId)))
				{
					SignalTarget = Hostile;
					BestSignal = Signal;
					BestSignalScore = SignalScore;
				}
			}
		}
		if (SignalTarget != nullptr && Decision.Posture != ETacticalAiPosture::Sentinel
			&& (Decision.Posture == ETacticalAiPosture::SignalPressure
				|| AttackTarget == nullptr || BestSignalScore > BestAttackScore))
		{
			Decision.Goal = ETacticalAiGoal::Engage;
			Decision.ActionType = ETacticalAiActionType::ProjectSignal;
			Decision.TargetUnitId = SignalTarget->UnitId;
			Decision.DestinationX = SignalTarget->X;
			Decision.DestinationY = SignalTarget->Y;
			Decision.DestinationZ = SignalTarget->Z;
			Decision.HitChance = BestSignal.HitChance;
			Decision.UtilityScore = ClampUtility(BestSignalScore);
			return Decision;
		}
		if (AttackTarget != nullptr)
		{
			Decision.Goal = ETacticalAiGoal::Engage;
			Decision.ActionType = ETacticalAiActionType::AttackUnit;
			Decision.TargetUnitId = AttackTarget->UnitId;
			Decision.DestinationX = AttackTarget->X;
			Decision.DestinationY = AttackTarget->Y;
			Decision.DestinationZ = AttackTarget->Z;
			Decision.HitChance = BestAttack.HitChance;
			Decision.UtilityScore = ClampUtility(BestAttackScore);
			return Decision;
		}
	}
	if (!bWithdraw && Decision.Posture == ETacticalAiPosture::Sentinel)
	{
		if (Unit->Stance == ETacticalStance::Standing && Unit->RemainingActionPoints >= 1)
		{
			Decision.Goal = ETacticalAiGoal::Guard;
			Decision.ActionType = ETacticalAiActionType::ChangeStance;
			Decision.DesiredStance = ETacticalStance::Crouched;
			Decision.UtilityScore = 100;
		}
		return Decision;
	}

	const int32 CurrentDistance = CeilDistance(
		Unit->X, Unit->Y, Unit->Z, PrimaryTarget->X, PrimaryTarget->Y, PrimaryTarget->Z);
	if (!bWithdraw)
	{
		int32 BestDoorDistance = MAX_int32;
		int32 BestDoorIndex = INDEX_NONE;
		static constexpr int32 OffsetX[] = { -1, 1, 0, 0, 0, 0 };
		static constexpr int32 OffsetY[] = { 0, 0, -1, 1, 0, 0 };
		static constexpr int32 OffsetZ[] = { 0, 0, 0, 0, -1, 1 };
		for (int32 Direction = 0; Direction < 6; ++Direction)
		{
			const int32 X = Unit->X + OffsetX[Direction];
			const int32 Y = Unit->Y + OffsetY[Direction];
			const int32 Z = Unit->Z + OffsetZ[Direction];
			if (!Battle.IsWithinGrid(X, Y, Z))
			{
				continue;
			}
			const int32 CellIndex = Battle.GetCellIndex(X, Y, Z);
			const FTacticalCellState& Cell = Battle.Cells[CellIndex];
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			if (Terrain == nullptr || !Terrain->IsDoor() || Cell.CurrentIntegrity <= 0 || Cell.bDoorOpen
				|| Terrain->DoorActionPointCost > Unit->RemainingActionPoints)
			{
				continue;
			}
			const int32 DoorDistance = CeilDistance(X, Y, Z, PrimaryTarget->X, PrimaryTarget->Y, PrimaryTarget->Z);
			if (DoorDistance < CurrentDistance
				&& (DoorDistance < BestDoorDistance
					|| (DoorDistance == BestDoorDistance && CellIndex < BestDoorIndex)))
			{
				BestDoorDistance = DoorDistance;
				BestDoorIndex = CellIndex;
			}
		}
		if (BestDoorIndex != INDEX_NONE)
		{
			const FTacticalCellState& Door = Battle.Cells[BestDoorIndex];
			Decision.Goal = ETacticalAiGoal::Advance;
			Decision.ActionType = ETacticalAiActionType::OpenDoor;
			Decision.DestinationX = Door.X;
			Decision.DestinationY = Door.Y;
			Decision.DestinationZ = Door.Z;
			Decision.UtilityScore = (CurrentDistance - BestDoorDistance) * 1000;
			return Decision;
		}
	}

	const int32 MovementBudget = FMath::Clamp(
		Unit->RemainingActionPoints * Policy.MovementBudgetPercent / 100,
		1,
		Unit->RemainingActionPoints);
	const FTacticalReachabilityResult Reachability = FTacticalNavigationService::ComputeReachableCells(
		Battle, Rules, UnitId, MovementBudget);
	if (!Reachability.bSucceeded)
	{
		Decision.bSucceeded = false;
		for (const FTacticalNavigationDiagnostic& Diagnostic : Reachability.Diagnostics)
		{
			AddDiagnostic(Decision, Diagnostic.Code, Diagnostic.Message);
		}
		return Decision;
	}
	const int32 CurrentIndex = Battle.GetCellIndex(Unit->X, Unit->Y, Unit->Z);
	FTacticalReachableCell CurrentCell;
	CurrentCell.CellIndex = CurrentIndex;
	CurrentCell.X = Unit->X;
	CurrentCell.Y = Unit->Y;
	CurrentCell.Z = Unit->Z;
	const int64 CurrentScore = ScorePosition(
		Battle, Rules, *UnitRule, CurrentCell, *PrimaryTarget, Hostiles, Unit->RemainingActionPoints, bWithdraw);
	const FTacticalReachableCell* BestCell = nullptr;
	int64 BestScore = CurrentScore;
	for (const FTacticalReachableCell& Candidate : Reachability.Cells)
	{
		if (Candidate.TotalCost <= 0)
		{
			continue;
		}
		const int64 CandidateScore = ScorePosition(
			Battle, Rules, *UnitRule, Candidate, *PrimaryTarget, Hostiles, Unit->RemainingActionPoints, bWithdraw);
		if (CandidateScore > BestScore
			|| (CandidateScore == BestScore && BestCell != nullptr
				&& (Candidate.TotalCost < BestCell->TotalCost
					|| (Candidate.TotalCost == BestCell->TotalCost && Candidate.CellIndex < BestCell->CellIndex))))
		{
			BestCell = &Candidate;
			BestScore = CandidateScore;
		}
	}
	if (BestCell != nullptr)
	{
		Decision.Goal = bWithdraw ? ETacticalAiGoal::Withdraw : ETacticalAiGoal::Advance;
		Decision.ActionType = ETacticalAiActionType::Move;
		Decision.DestinationX = BestCell->X;
		Decision.DestinationY = BestCell->Y;
		Decision.DestinationZ = BestCell->Z;
		Decision.MovementCost = BestCell->TotalCost;
		Decision.UtilityScore = ClampUtility(BestScore - CurrentScore);
		return Decision;
	}

	if (Unit->Stance == ETacticalStance::Standing && Unit->RemainingActionPoints >= 1)
	{
		Decision.Goal = bWithdraw ? ETacticalAiGoal::Withdraw : ETacticalAiGoal::Guard;
		Decision.ActionType = ETacticalAiActionType::ChangeStance;
		Decision.DesiredStance = ETacticalStance::Crouched;
	}
	return Decision;
}
