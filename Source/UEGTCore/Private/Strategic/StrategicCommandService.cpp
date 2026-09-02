// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/StrategicCommandService.h"

#include "Algo/Count.h"
#include "Content/ContentPackageResolver.h"
#include "Misc/Crc.h"
#include "Strategic/CraftServiceQueue.h"
#include "Strategic/MutualAidRelayQueue.h"
#include "Strategic/PersonnelLegacyRelay.h"
#include "Strategic/PersonnelMentorship.h"
#include "Strategic/PersonnelRecoveryPlan.h"
#include "Strategic/PersonnelSquadBond.h"
#include "Strategic/PersonnelStewardship.h"
#include "Tactical/TacticalAiService.h"
#include "Tactical/TacticalCombatService.h"
#include "Tactical/TacticalMissionGenerator.h"
#include "Tactical/TacticalNavigationService.h"

namespace StrategicCommandServicePrivate
{
	constexpr int32 WorksCadreMaximumEngineerCount = 3;
	constexpr int32 WorksCadreCommonPercentEach = 10;
	constexpr int32 WorksCadreSpecializedPrimaryPercentEach = 15;
	constexpr int32 WorksCadreSpecializedSecondaryPercentEach = 5;
	constexpr int32 WorksCadreMaximumFrontloadPercent =
		WorksCadreMaximumEngineerCount * WorksCadreSpecializedPrimaryPercentEach;

	bool IsKnownWorksCadreCharter(const EWorksCadreCharter Charter)
	{
		return Charter == EWorksCadreCharter::CommonCadence
			|| Charter == EWorksCadreCharter::AssemblyCadence
			|| Charter == EWorksCadreCharter::RestorationCadence;
	}

	void AddError(FStrategicCommandResult& Result, const FName Code, FString Message)
	{
		FStrategicCommandDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	struct FBaseSpecializationCandidate
	{
		FName SpecializationId;
		FName BenefitMetricId;
		int32 Score = 0;
		int64 BenefitValue = 0;
		FName OperationalBenefitMetricId;
		int64 OperationalBenefitValue = 0;
	};

	int32 NormalizeBaseSpecializationCapacity(const int32 Value, const int32 PointsPerUnit)
	{
		const int64 NonNegativeValue = FMath::Max<int64>(0, Value);
		return static_cast<int32>(FMath::Clamp<int64>(
			NonNegativeValue * PointsPerUnit, 0, 100));
	}

	FStrategicBaseSpecializationView BuildBaseSpecialization(
		const int32 DetectionStrength,
		const int32 FacilityScientistCapacity,
		const int32 FacilityEngineerCapacity,
		const int32 CraftCapacity,
		const int64 StorageCapacity)
	{
		const int64 NonNegativeStorageCapacity = FMath::Max<int64>(0, StorageCapacity);
		TArray<FBaseSpecializationCandidate> Candidates = {
			{
				TEXT("base.specialization.signal-relay"),
				TEXT("base.specialization.detection-strength"),
				FMath::Clamp(DetectionStrength, 0, 100),
				FMath::Max<int64>(0, DetectionStrength),
				TEXT("base.specialization.relay-channels"),
				1
			},
			{
				TEXT("base.specialization.research-enclave"),
				TEXT("base.specialization.scientist-capacity"),
				NormalizeBaseSpecializationCapacity(FacilityScientistCapacity, 10),
				FMath::Max<int64>(0, FacilityScientistCapacity),
				TEXT("base.specialization.research-rate"),
				20
			},
			{
				TEXT("base.specialization.fabrication-works"),
				TEXT("base.specialization.engineer-capacity"),
				NormalizeBaseSpecializationCapacity(FacilityEngineerCapacity, 10),
				FMath::Max<int64>(0, FacilityEngineerCapacity),
				TEXT("base.specialization.manufacturing-rate"),
				20
			},
			{
				TEXT("base.specialization.flight-operations"),
				TEXT("base.specialization.craft-berths"),
				NormalizeBaseSpecializationCapacity(CraftCapacity, 50),
				FMath::Max<int64>(0, CraftCapacity),
				TEXT("base.specialization.service-lanes"),
				1
			},
			{
				TEXT("base.specialization.logistics-depot"),
				TEXT("base.specialization.storage-capacity"),
				static_cast<int32>(FMath::Clamp<int64>(
					NonNegativeStorageCapacity / 12, 0, 100)),
				NonNegativeStorageCapacity,
				TEXT("base.specialization.storage-efficiency"),
				20
			}
		};
		Candidates.Sort([](
			const FBaseSpecializationCandidate& Left,
			const FBaseSpecializationCandidate& Right)
		{
			return Left.Score != Right.Score
				? Left.Score > Right.Score
				: Left.SpecializationId.LexicalLess(Right.SpecializationId);
		});

		FStrategicBaseSpecializationView Result;
		const FBaseSpecializationCandidate& Primary = Candidates[0];
		Result.Score = Primary.Score;
		Result.SecondaryScore = Candidates[1].Score;
		Result.bSpecialized = Result.Score >= 50
			&& Result.Score >= Result.SecondaryScore + 10;
		if (Result.bSpecialized)
		{
			Result.SpecializationId = Primary.SpecializationId;
			Result.BenefitMetricId = Primary.BenefitMetricId;
			Result.BenefitValue = Primary.BenefitValue;
			Result.OperationalBenefitMetricId = Primary.OperationalBenefitMetricId;
			Result.OperationalBenefitValue = Primary.OperationalBenefitValue;
		}
		return Result;
	}

	bool ValidateSequence(const FCampaignState& State, const int64 ExpectedSequence, FStrategicCommandResult& Result)
	{
		if (State.CommandSequence < 0 || State.CommandSequence == MAX_int64)
		{
			AddError(Result, TEXT("invalid_campaign_sequence"), TEXT("Campaign command sequence cannot accept another command."));
			return false;
		}
		if (ExpectedSequence != State.CommandSequence)
		{
			AddError(Result, TEXT("stale_command"), FString::Printf(TEXT("Command expected sequence %lld, but campaign is at %lld."), ExpectedSequence, State.CommandSequence));
			return false;
		}
		return true;
	}

	FStrategicBaseState* FindBase(FCampaignState& State, const FGuid& BaseId)
	{
		return State.Bases.FindByPredicate([&BaseId](const FStrategicBaseState& Base) { return Base.BaseId == BaseId; });
	}

	const FStrategicBaseState* FindBase(const FCampaignState& State, const FGuid& BaseId)
	{
		return State.Bases.FindByPredicate([&BaseId](const FStrategicBaseState& Base) { return Base.BaseId == BaseId; });
	}

	FBaseFacilityState* FindFacility(FStrategicBaseState& Base, const FGuid& FacilityInstanceId)
	{
		return Base.Facilities.FindByPredicate(
			[&FacilityInstanceId](const FBaseFacilityState& Facility)
			{
				return Facility.InstanceId == FacilityInstanceId;
			});
	}

	const FBaseFacilityState* FindFacility(const FStrategicBaseState& Base, const FGuid& FacilityInstanceId)
	{
		return Base.Facilities.FindByPredicate(
			[&FacilityInstanceId](const FBaseFacilityState& Facility)
			{
				return Facility.InstanceId == FacilityInstanceId;
			});
	}

	FResearchProjectState* FindResearchProject(FCampaignState& State, const FName ResearchId)
	{
		return State.ResearchProjects.FindByPredicate([ResearchId](const FResearchProjectState& Project) { return Project.ResearchId == ResearchId; });
	}

	FManufacturingProjectState* FindManufacturingProject(FCampaignState& State, const FGuid& ProjectId)
	{
		return State.ManufacturingProjects.FindByPredicate([&ProjectId](const FManufacturingProjectState& Project) { return Project.ProjectId == ProjectId; });
	}

	FPersonnelState* FindPersonnel(FCampaignState& State, const FGuid& PersonnelId)
	{
		return State.Personnel.FindByPredicate([&PersonnelId](const FPersonnelState& Person) { return Person.PersonnelId == PersonnelId; });
	}

	const FPersonnelState* FindPersonnel(const FCampaignState& State, const FGuid& PersonnelId)
	{
		return State.Personnel.FindByPredicate([&PersonnelId](const FPersonnelState& Person) { return Person.PersonnelId == PersonnelId; });
	}

	FCraftState* FindCraft(FCampaignState& State, const FGuid& CraftId)
	{
		return State.Craft.FindByPredicate([&CraftId](const FCraftState& Craft) { return Craft.CraftId == CraftId; });
	}

	const FCraftState* FindCraft(const FCampaignState& State, const FGuid& CraftId)
	{
		return State.Craft.FindByPredicate([&CraftId](const FCraftState& Craft) { return Craft.CraftId == CraftId; });
	}

	FStrategicContactState* FindContact(FCampaignState& State, const FGuid& ContactId)
	{
		return State.StrategicContacts.FindByPredicate(
			[&ContactId](const FStrategicContactState& Contact) { return Contact.ContactId == ContactId; });
	}

	const FStrategicContactState* FindContact(const FCampaignState& State, const FGuid& ContactId)
	{
		return State.StrategicContacts.FindByPredicate(
			[&ContactId](const FStrategicContactState& Contact) { return Contact.ContactId == ContactId; });
	}

	FBaseAssaultState* FindBaseAssault(FCampaignState& State, const FGuid& AssaultId)
	{
		return State.BaseAssaults.FindByPredicate(
			[&AssaultId](const FBaseAssaultState& Assault) { return Assault.AssaultId == AssaultId; });
	}

	const FBaseAssaultState* FindBaseAssault(const FCampaignState& State, const FGuid& AssaultId)
	{
		return State.BaseAssaults.FindByPredicate(
			[&AssaultId](const FBaseAssaultState& Assault) { return Assault.AssaultId == AssaultId; });
	}

	FStrategicSiteState* FindSite(FCampaignState& State, const FGuid& SiteId)
	{
		return State.StrategicSites.FindByPredicate(
			[&SiteId](const FStrategicSiteState& Site) { return Site.SiteId == SiteId; });
	}

	const FStrategicSiteState* FindSite(const FCampaignState& State, const FGuid& SiteId)
	{
		return State.StrategicSites.FindByPredicate(
			[&SiteId](const FStrategicSiteState& Site) { return Site.SiteId == SiteId; });
	}

	FTacticalOperationState* FindTacticalOperation(FCampaignState& State, const FGuid& OperationId)
	{
		return State.TacticalOperations.FindByPredicate(
			[&OperationId](const FTacticalOperationState& Operation) { return Operation.OperationId == OperationId; });
	}

	const FTacticalOperationState* FindTacticalOperation(const FCampaignState& State, const FGuid& OperationId)
	{
		return State.TacticalOperations.FindByPredicate(
			[&OperationId](const FTacticalOperationState& Operation) { return Operation.OperationId == OperationId; });
	}

	FTacticalBattleState* FindTacticalBattle(FCampaignState& State, const FGuid& BattleId)
	{
		return State.TacticalBattles.FindByPredicate(
			[&BattleId](const FTacticalBattleState& Battle) { return Battle.BattleId == BattleId; });
	}

	const FTacticalBattleState* FindTacticalBattle(const FCampaignState& State, const FGuid& BattleId)
	{
		return State.TacticalBattles.FindByPredicate(
			[&BattleId](const FTacticalBattleState& Battle) { return Battle.BattleId == BattleId; });
	}

	struct FTacticalResolutionEvaluation
	{
		bool bResolved = false;
		bool bObjectiveCompleted = false;
	};

	FTacticalResolutionEvaluation EvaluateTacticalBattleResolution(FTacticalBattleState& Battle)
	{
		FTacticalResolutionEvaluation Evaluation;
		const bool bAnyLivingPlayer = Battle.Units.ContainsByPredicate(
			[](const FTacticalUnitState& Unit)
			{
				return Unit.Team == ETacticalTeam::Player && Unit.CurrentHealth > 0;
			});
		const bool bAnyDeployedLivingPlayer = Battle.Units.ContainsByPredicate(
			[](const FTacticalUnitState& Unit)
			{
				return Unit.Team == ETacticalTeam::Player && Unit.CurrentHealth > 0 && !Unit.bExtracted;
			});
		if (!bAnyLivingPlayer)
		{
			for (FTacticalObjectiveState& Objective : Battle.Objectives)
			{
				Objective.Status = ETacticalObjectiveStatus::Failed;
			}
			Battle.Phase = ETacticalBattlePhase::Resolved;
			Evaluation.bResolved = true;
			return Evaluation;
		}
		if (!Battle.bRequiresExtraction)
		{
			Evaluation.bObjectiveCompleted = !Battle.Objectives.IsEmpty()
				&& !Battle.Objectives.ContainsByPredicate(
					[](const FTacticalObjectiveState& Objective)
					{
						return Objective.Status != ETacticalObjectiveStatus::Completed;
					});
			const bool bObjectiveFailed = Battle.Objectives.ContainsByPredicate(
				[](const FTacticalObjectiveState& Objective)
				{
					return Objective.Status == ETacticalObjectiveStatus::Failed;
				});
			if (!Evaluation.bObjectiveCompleted && !bObjectiveFailed)
			{
				return Evaluation;
			}
			for (FTacticalObjectiveState& Objective : Battle.Objectives)
			{
				if (Objective.Status == ETacticalObjectiveStatus::Active)
				{
					Objective.Status = ETacticalObjectiveStatus::Failed;
				}
			}
			Battle.Phase = ETacticalBattlePhase::Resolved;
			Evaluation.bResolved = true;
			return Evaluation;
		}
		if (bAnyDeployedLivingPlayer)
		{
			return Evaluation;
		}

		Evaluation.bObjectiveCompleted = !Battle.Objectives.IsEmpty()
			&& !Battle.Objectives.ContainsByPredicate(
				[](const FTacticalObjectiveState& Objective)
				{
					return Objective.Status != ETacticalObjectiveStatus::Completed;
				});
		if (!Evaluation.bObjectiveCompleted)
		{
			for (FTacticalObjectiveState& Objective : Battle.Objectives)
			{
				if (Objective.Status == ETacticalObjectiveStatus::Active)
				{
					Objective.Status = ETacticalObjectiveStatus::Failed;
				}
			}
		}
		Battle.Phase = ETacticalBattlePhase::Resolved;
		Evaluation.bResolved = true;
		return Evaluation;
	}

	struct FTacticalPressureChange
	{
		int32 SuppressionDelta = 0;
		int32 MoraleDelta = 0;
	};

	int32 GetTacticalArmor(const FTacticalUnitState& Unit, const ETacticalDamageType DamageType)
	{
		if (DamageType == ETacticalDamageType::Thermal)
		{
			return Unit.ThermalArmor;
		}
		if (DamageType == ETacticalDamageType::Arc)
		{
			return Unit.ArcArmor;
		}
		return Unit.KineticArmor;
	}

	int32 CeilTacticalDistance(const int32 DeltaX, const int32 DeltaY, const int32 DeltaZ = 0)
	{
		const int64 DistanceSquared = static_cast<int64>(DeltaX) * DeltaX
			+ static_cast<int64>(DeltaY) * DeltaY
			+ static_cast<int64>(DeltaZ * 2) * (DeltaZ * 2);
		for (int32 Distance = 0; Distance <= 128; ++Distance)
		{
			if (static_cast<int64>(Distance) * Distance >= DistanceSquared)
			{
				return Distance;
			}
		}
		return MAX_int32;
	}

	int32 ScaleTacticalValue(const int32 Value, const int32 Percent)
	{
		return static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(FMath::Max(0, Value)) * FMath::Clamp(Percent, 0, 300) / 100,
			0,
			MAX_int32));
	}

	FTacticalPressureChange ApplyAttackPressure(
		FTacticalUnitState& Target,
		const int32 AttackPower,
		const bool bHit,
		const bool bIncapacitated)
	{
		const int32 PreviousSuppression = Target.Suppression;
		const int32 PreviousMorale = Target.CurrentMorale;
		if (bIncapacitated)
		{
			Target.Suppression = 100;
			Target.CurrentMorale = 0;
		}
		else
		{
			const int32 Pressure = FMath::Clamp(5 + FMath::Max(0, AttackPower) / 8 + (bHit ? 5 : 0), 5, 30);
			Target.Suppression = FMath::Min(100, Target.Suppression + Pressure);
			const int32 MoraleLoss = FMath::Max(1, Pressure / 2 + (bHit ? 4 : 0) - Target.Resolve / 20);
			Target.CurrentMorale = FMath::Max(0, Target.CurrentMorale - MoraleLoss);
		}
		FTacticalPressureChange Change;
		Change.SuppressionDelta = Target.Suppression - PreviousSuppression;
		Change.MoraleDelta = Target.CurrentMorale - PreviousMorale;
		return Change;
	}

	struct FTacticalEnvironmentUnitOutcome
	{
		FGuid UnitId;
		FName UnitRuleId;
		int32 Damage = 0;
		int32 RemainingHealth = 0;
		int32 SuppressionDelta = 0;
		int32 Suppression = 0;
		int32 MoraleDelta = 0;
		int32 Morale = 0;
		bool bIncapacitated = false;
		bool bPanicked = false;
	};

	struct FTacticalEnvironmentOutcome
	{
		int32 SmokeCellCount = 0;
		int32 FireCellCount = 0;
		int32 DiffusedSmokeCellCount = 0;
		int32 SpreadFireCellCount = 0;
		int32 WindTransportedSmokeAmount = 0;
		int32 WindAssistedFireCellCount = 0;
		int32 VentilatedSmokeAmount = 0;
		int32 VentilatedSmokeCellCount = 0;
		int32 VerticalPropagatedSmokeAmount = 0;
		int32 VerticalSpreadFireCellCount = 0;
		TArray<FTacticalEnvironmentUnitOutcome> Units;
	};

	bool IsTacticalAirflowBlocked(const FTacticalCellState& Cell, const FTacticalTerrainRule& Terrain)
	{
		return Cell.CurrentIntegrity > 0
			&& Terrain.bBlocksMovement
			&& !(Terrain.IsDoor() && Cell.bDoorOpen);
	}

	int32 GetEffectiveVentilation(const FTacticalCellState& Cell, const FTacticalTerrainRule& Terrain)
	{
		return IsTacticalAirflowBlocked(Cell, Terrain) ? 0 : Terrain.VentilationPercent;
	}

	void GetTacticalWindOffset(const ETacticalWindDirection Direction, int32& OutX, int32& OutY)
	{
		OutX = 0;
		OutY = 0;
		switch (Direction)
		{
		case ETacticalWindDirection::North:
			OutY = -1;
			break;
		case ETacticalWindDirection::East:
			OutX = 1;
			break;
		case ETacticalWindDirection::South:
			OutY = 1;
			break;
		case ETacticalWindDirection::West:
			OutX = -1;
			break;
		default:
			break;
		}
	}

	FTacticalEnvironmentOutcome AdvanceTacticalEnvironment(
		FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules)
	{
		FTacticalEnvironmentOutcome Outcome;
		TArray<int32> PreviousSmoke;
		TArray<int32> PreviousFire;
		TArray<int32> EffectiveVentilation;
		TArray<int32> BaseSmokeOutgoing;
		TArray<int32> BaseSmokeIncoming;
		TArray<int32> WindSmokeOutgoing;
		TArray<int32> WindSmokeIncoming;
		PreviousSmoke.Reserve(Battle.Cells.Num());
		PreviousFire.Reserve(Battle.Cells.Num());
		EffectiveVentilation.Reserve(Battle.Cells.Num());
		BaseSmokeOutgoing.SetNumZeroed(Battle.Cells.Num());
		BaseSmokeIncoming.SetNumZeroed(Battle.Cells.Num());
		WindSmokeOutgoing.SetNumZeroed(Battle.Cells.Num());
		WindSmokeIncoming.SetNumZeroed(Battle.Cells.Num());
		for (const FTacticalCellState& Cell : Battle.Cells)
		{
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			check(Terrain != nullptr);
			PreviousSmoke.Add(Cell.Smoke);
			PreviousFire.Add(Cell.Fire);
			EffectiveVentilation.Add(GetEffectiveVentilation(Cell, *Terrain));
		}
		for (FTacticalUnitState& Unit : Battle.Units)
		{
			if (Unit.Team != Battle.ActiveTeam || Unit.CurrentHealth <= 0 || Unit.bExtracted)
			{
				continue;
			}
			const int32 PreviousHealth = Unit.CurrentHealth;
			const int32 PreviousSuppression = Unit.Suppression;
			const int32 PreviousMorale = Unit.CurrentMorale;
			const FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(Unit.X, Unit.Y, Unit.Z)];
			const int32 FireExposure = Cell.Fire;
			Unit.Suppression = FMath::Max(0, Unit.Suppression - (5 + Unit.Resolve / 10));
			if (FireExposure > 0)
			{
				const int32 FireDamage = FMath::Max(1, FireExposure / 10 - Unit.ThermalArmor / 10);
				Unit.CurrentHealth = FMath::Max(0, Unit.CurrentHealth - FireDamage);
				Unit.Suppression = FMath::Min(100, Unit.Suppression + FMath::Max(1, FireExposure / 8));
				Unit.CurrentMorale = FMath::Max(0, Unit.CurrentMorale - FMath::Max(1, FireExposure / 10 - Unit.Resolve / 20));
			}
			else if (Unit.Suppression <= 20)
			{
				Unit.CurrentMorale = FMath::Min(Unit.MaxMorale, Unit.CurrentMorale + FMath::Max(1, Unit.Resolve / 12));
			}

			const bool bIncapacitated = PreviousHealth > 0 && Unit.CurrentHealth == 0;
			if (bIncapacitated)
			{
				Unit.Suppression = 100;
				Unit.CurrentMorale = 0;
				Unit.RemainingActionPoints = 0;
			}
			else
			{
				const int32 PanicThreshold = FMath::Max(5, Unit.MaxMorale / 5);
				if (Unit.CurrentMorale <= PanicThreshold)
				{
					Unit.RemainingActionPoints = 0;
				}
				else
				{
					const int32 MoralePenalty = Unit.CurrentMorale * 3 < Unit.MaxMorale
						? 2
						: (Unit.CurrentMorale * 3 < Unit.MaxMorale * 2 ? 1 : 0);
					const int32 ActionPointPenalty = Unit.Suppression / 20 + MoralePenalty;
					Unit.RemainingActionPoints = FMath::Max(1, Unit.MaxActionPoints - ActionPointPenalty);
				}
			}

			FTacticalEnvironmentUnitOutcome& UnitOutcome = Outcome.Units.AddDefaulted_GetRef();
			UnitOutcome.UnitId = Unit.UnitId;
			UnitOutcome.UnitRuleId = Unit.SourceRuleId;
			UnitOutcome.Damage = PreviousHealth - Unit.CurrentHealth;
			UnitOutcome.RemainingHealth = Unit.CurrentHealth;
			UnitOutcome.SuppressionDelta = Unit.Suppression - PreviousSuppression;
			UnitOutcome.Suppression = Unit.Suppression;
			UnitOutcome.MoraleDelta = Unit.CurrentMorale - PreviousMorale;
			UnitOutcome.Morale = Unit.CurrentMorale;
			UnitOutcome.bIncapacitated = bIncapacitated;
			UnitOutcome.bPanicked = Unit.CurrentHealth > 0 && Unit.RemainingActionPoints == 0;
		}

		static constexpr int32 OffsetX[] = { -1, 1, 0, 0 };
		static constexpr int32 OffsetY[] = { 0, 0, -1, 1 };
		for (int32 SourceIndex = 0; SourceIndex < Battle.Cells.Num(); ++SourceIndex)
		{
			const FTacticalCellState& SourceCell = Battle.Cells[SourceIndex];
			const FTacticalTerrainRule* SourceTerrain = Rules.TacticalTerrains.Find(SourceCell.TerrainRuleId);
			check(SourceTerrain != nullptr);
			if (IsTacticalAirflowBlocked(SourceCell, *SourceTerrain))
			{
				continue;
			}
			for (int32 Direction = 0; Direction < 4; ++Direction)
			{
				const int32 NeighborX = SourceCell.X + OffsetX[Direction];
				const int32 NeighborY = SourceCell.Y + OffsetY[Direction];
				if (!Battle.IsWithinGrid(NeighborX, NeighborY, SourceCell.Z))
				{
					continue;
				}
				const int32 NeighborIndex = Battle.GetCellIndex(NeighborX, NeighborY, SourceCell.Z);
				const FTacticalCellState& NeighborCell = Battle.Cells[NeighborIndex];
				const FTacticalTerrainRule* NeighborTerrain = Rules.TacticalTerrains.Find(NeighborCell.TerrainRuleId);
				check(NeighborTerrain != nullptr);
				if (IsTacticalAirflowBlocked(NeighborCell, *NeighborTerrain))
				{
					continue;
				}
				const int32 Transfer = PreviousSmoke[SourceIndex] * 15 / 100;
				BaseSmokeOutgoing[SourceIndex] += Transfer;
				BaseSmokeIncoming[NeighborIndex] += Transfer;
			}
			if (SourceTerrain->IsVerticalConnector())
			{
				for (const int32 DeltaZ : { 1, -1 })
				{
					const int32 NeighborZ = SourceCell.Z + DeltaZ;
					if (!Battle.IsWithinGrid(SourceCell.X, SourceCell.Y, NeighborZ))
					{
						continue;
					}
					const int32 NeighborIndex = Battle.GetCellIndex(SourceCell.X, SourceCell.Y, NeighborZ);
					const FTacticalCellState& NeighborCell = Battle.Cells[NeighborIndex];
					const FTacticalTerrainRule* NeighborTerrain = Rules.TacticalTerrains.Find(NeighborCell.TerrainRuleId);
					check(NeighborTerrain != nullptr);
					if (!NeighborTerrain->IsVerticalConnector() || IsTacticalAirflowBlocked(NeighborCell, *NeighborTerrain))
					{
						continue;
					}
					const int32 TransferPercent = DeltaZ > 0 ? 25 : 5;
					const int32 Transfer = PreviousSmoke[SourceIndex] * TransferPercent / 100;
					BaseSmokeOutgoing[SourceIndex] += Transfer;
					BaseSmokeIncoming[NeighborIndex] += Transfer;
					Outcome.VerticalPropagatedSmokeAmount += Transfer;
				}
			}
		}

		int32 WindX = 0;
		int32 WindY = 0;
		GetTacticalWindOffset(Battle.WindDirection, WindX, WindY);
		if (Battle.WindStrength > 0 && (WindX != 0 || WindY != 0))
		{
			for (int32 SourceIndex = 0; SourceIndex < Battle.Cells.Num(); ++SourceIndex)
			{
				const FTacticalCellState& SourceCell = Battle.Cells[SourceIndex];
				const int32 SourceVentilation = EffectiveVentilation[SourceIndex];
				const int32 AvailableSmoke = FMath::Max(0, PreviousSmoke[SourceIndex] - BaseSmokeOutgoing[SourceIndex]);
				if (AvailableSmoke <= 0 || SourceVentilation <= 0)
				{
					continue;
				}

				const int32 TargetX = SourceCell.X + WindX;
				const int32 TargetY = SourceCell.Y + WindY;
				int32 FlowVentilation = SourceVentilation;
				int32 TargetIndex = INDEX_NONE;
				if (Battle.IsWithinGrid(TargetX, TargetY, SourceCell.Z))
				{
					TargetIndex = Battle.GetCellIndex(TargetX, TargetY, SourceCell.Z);
					const FTacticalCellState& TargetCell = Battle.Cells[TargetIndex];
					const FTacticalTerrainRule* TargetTerrain = Rules.TacticalTerrains.Find(TargetCell.TerrainRuleId);
					check(TargetTerrain != nullptr);
					if (IsTacticalAirflowBlocked(TargetCell, *TargetTerrain) || EffectiveVentilation[TargetIndex] <= 0)
					{
						continue;
					}
					FlowVentilation = FMath::Min(SourceVentilation, EffectiveVentilation[TargetIndex]);
				}

				const int32 PotentialTransfer = static_cast<int32>(
					static_cast<int64>(PreviousSmoke[SourceIndex]) * Battle.WindStrength * FlowVentilation / 500);
				const int32 Transfer = FMath::Min(AvailableSmoke, PotentialTransfer);
				if (Transfer <= 0)
				{
					continue;
				}
				WindSmokeOutgoing[SourceIndex] = Transfer;
				if (TargetIndex != INDEX_NONE)
				{
					WindSmokeIncoming[TargetIndex] += Transfer;
				}
				Outcome.WindTransportedSmokeAmount += Transfer;
			}
		}

		for (int32 CellIndex = 0; CellIndex < Battle.Cells.Num(); ++CellIndex)
		{
			FTacticalCellState& Cell = Battle.Cells[CellIndex];
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			check(Terrain != nullptr);
			int32 MaximumNeighborFire = 0;
			if (!IsTacticalAirflowBlocked(Cell, *Terrain))
			{
				for (int32 Direction = 0; Direction < 4; ++Direction)
				{
					const int32 NeighborX = Cell.X + OffsetX[Direction];
					const int32 NeighborY = Cell.Y + OffsetY[Direction];
					if (!Battle.IsWithinGrid(NeighborX, NeighborY, Cell.Z))
					{
						continue;
					}
					const int32 NeighborIndex = Battle.GetCellIndex(NeighborX, NeighborY, Cell.Z);
					const FTacticalCellState& NeighborCell = Battle.Cells[NeighborIndex];
					const FTacticalTerrainRule* NeighborTerrain = Rules.TacticalTerrains.Find(NeighborCell.TerrainRuleId);
					check(NeighborTerrain != nullptr);
					if (!IsTacticalAirflowBlocked(NeighborCell, *NeighborTerrain))
					{
						MaximumNeighborFire = FMath::Max(MaximumNeighborFire, PreviousFire[NeighborIndex]);
					}
				}
			}
			int32 VerticalSpreadFire = 0;
			if (!IsTacticalAirflowBlocked(Cell, *Terrain) && Terrain->IsVerticalConnector())
			{
				for (const int32 SourceDeltaZ : { -1, 1 })
				{
					const int32 SourceZ = Cell.Z + SourceDeltaZ;
					if (!Battle.IsWithinGrid(Cell.X, Cell.Y, SourceZ))
					{
						continue;
					}
					const int32 SourceIndex = Battle.GetCellIndex(Cell.X, Cell.Y, SourceZ);
					const FTacticalCellState& VerticalSource = Battle.Cells[SourceIndex];
					const FTacticalTerrainRule* VerticalTerrain = Rules.TacticalTerrains.Find(VerticalSource.TerrainRuleId);
					check(VerticalTerrain != nullptr);
					if (!VerticalTerrain->IsVerticalConnector() || IsTacticalAirflowBlocked(VerticalSource, *VerticalTerrain))
					{
						continue;
					}
					const int32 Divisor = SourceDeltaZ < 0 ? 100 : 300;
					VerticalSpreadFire = FMath::Max(
						VerticalSpreadFire,
						PreviousFire[SourceIndex] * Terrain->Flammability / Divisor);
				}
			}
			const int32 SmokeAfterTransport = FMath::Max(
				0,
				PreviousSmoke[CellIndex] - BaseSmokeOutgoing[CellIndex] - WindSmokeOutgoing[CellIndex]);
			const int32 VentilatedSmoke = SmokeAfterTransport * EffectiveVentilation[CellIndex] / 100;
			const int32 RetainedSmoke = SmokeAfterTransport - VentilatedSmoke;
			Outcome.VentilatedSmokeAmount += VentilatedSmoke;
			Outcome.VentilatedSmokeCellCount += VentilatedSmoke > 0 ? 1 : 0;
			Cell.Smoke = FMath::Clamp(
				RetainedSmoke + BaseSmokeIncoming[CellIndex] + WindSmokeIncoming[CellIndex]
					+ PreviousFire[CellIndex] / 5 - 10,
				0,
				100);
			const int32 FireDecay = FMath::Max(5, 15 - Terrain->Flammability / 10);
			const int32 RetainedFire = FMath::Max(0, PreviousFire[CellIndex] - FireDecay);
			const int32 BaseSpreadFire = MaximumNeighborFire * Terrain->Flammability / 200;
			int32 WindBiasedSpreadFire = 0;
			if (Battle.WindStrength > 0 && (WindX != 0 || WindY != 0) && EffectiveVentilation[CellIndex] > 0)
			{
				const int32 UpwindX = Cell.X - WindX;
				const int32 UpwindY = Cell.Y - WindY;
				if (Battle.IsWithinGrid(UpwindX, UpwindY, Cell.Z))
				{
					const int32 UpwindIndex = Battle.GetCellIndex(UpwindX, UpwindY, Cell.Z);
					const FTacticalCellState& UpwindCell = Battle.Cells[UpwindIndex];
					const FTacticalTerrainRule* UpwindTerrain = Rules.TacticalTerrains.Find(UpwindCell.TerrainRuleId);
					check(UpwindTerrain != nullptr);
					if (!IsTacticalAirflowBlocked(UpwindCell, *UpwindTerrain) && EffectiveVentilation[UpwindIndex] > 0)
					{
						const int32 LinkVentilation = FMath::Min(
							EffectiveVentilation[CellIndex],
							EffectiveVentilation[UpwindIndex]);
						const int32 UpwindBaseSpread = PreviousFire[UpwindIndex] * Terrain->Flammability / 200;
						const int32 WindBonus = static_cast<int32>(
							static_cast<int64>(PreviousFire[UpwindIndex]) * Terrain->Flammability
								* Battle.WindStrength * LinkVentilation / 50000);
						WindBiasedSpreadFire = UpwindBaseSpread + WindBonus;
					}
				}
			}
			const int32 NonVerticalFire = FMath::Max3(RetainedFire, BaseSpreadFire, WindBiasedSpreadFire);
			const int32 SpreadFire = FMath::Max(FMath::Max(BaseSpreadFire, WindBiasedSpreadFire), VerticalSpreadFire);
			Cell.Fire = FMath::Clamp(FMath::Max(RetainedFire, SpreadFire), 0, 100);
			Outcome.WindAssistedFireCellCount += WindBiasedSpreadFire > FMath::Max(RetainedFire, BaseSpreadFire) ? 1 : 0;
			Outcome.VerticalSpreadFireCellCount += VerticalSpreadFire > NonVerticalFire ? 1 : 0;
			Outcome.DiffusedSmokeCellCount += PreviousSmoke[CellIndex] == 0 && Cell.Smoke > 0 ? 1 : 0;
			Outcome.SpreadFireCellCount += PreviousFire[CellIndex] == 0 && Cell.Fire > 0 ? 1 : 0;
			Outcome.SmokeCellCount += Cell.Smoke > 0 ? 1 : 0;
			Outcome.FireCellCount += Cell.Fire > 0 ? 1 : 0;
		}
		return Outcome;
	}

	void AddCombatDiagnostics(FStrategicCommandResult& Result, const FTacticalAttackPreview& Preview)
	{
		for (const FTacticalCombatDiagnostic& Diagnostic : Preview.Diagnostics)
		{
			AddError(Result, Diagnostic.Code, Diagnostic.Message);
		}
	}

	void AddCombatDiagnostics(FStrategicCommandResult& Result, const FTacticalSignalPreview& Preview)
	{
		for (const FTacticalCombatDiagnostic& Diagnostic : Preview.Diagnostics)
		{
			AddError(Result, Diagnostic.Code, Diagnostic.Message);
		}
	}

	int32 FindBestEjectedMagazineIndex(
		const FTacticalUnitState& Unit,
		const FName WeaponItemId,
		const FName AmmunitionItemId)
	{
		int32 BestIndex = INDEX_NONE;
		int32 BestAmmunition = 0;
		for (int32 Index = 0; Index < Unit.EjectedMagazines.Num(); ++Index)
		{
			const FTacticalMagazineState& Magazine = Unit.EjectedMagazines[Index];
			if (Magazine.WeaponItemId == WeaponItemId
				&& Magazine.AmmunitionItemId == AmmunitionItemId
				&& Magazine.LoadedAmmunition > BestAmmunition)
			{
				BestIndex = Index;
				BestAmmunition = Magazine.LoadedAmmunition;
			}
		}
		return BestIndex;
	}

	void SyncTacticalConsumablesToPersonnel(
		FPersonnelState& Person,
		const FTacticalUnitState& Unit,
		const FResolvedRuleSet& Rules)
	{
		if (!Unit.WeaponStates.IsEmpty())
		{
			TSet<FName> AmmunitionItemIds;
			for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
			{
				const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
				if (Weapon != nullptr && !Weapon->TacticalAmmunitionItemId.IsNone())
				{
					AmmunitionItemIds.Add(Weapon->TacticalAmmunitionItemId);
				}
			}
			for (const FName AmmunitionItemId : AmmunitionItemIds)
			{
				int32 RemainingMagazines = 0;
				if (const FInventoryStack* Reserve = Unit.CarriedItems.FindByPredicate(
					[AmmunitionItemId](const FInventoryStack& Stack) { return Stack.ItemId == AmmunitionItemId; }))
				{
					RemainingMagazines += Reserve->Quantity;
				}
				for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
				{
					const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
					if (Weapon != nullptr && Weapon->TacticalAmmunitionItemId == AmmunitionItemId
						&& WeaponState.LoadedAmmunition > 0)
					{
						++RemainingMagazines;
					}
				}
				for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
				{
					if (Magazine.AmmunitionItemId == AmmunitionItemId
						&& Magazine.LoadedAmmunition > 0)
					{
						++RemainingMagazines;
					}
				}
				Person.EquippedItems.Remove(AmmunitionItemId);
				for (int32 Index = 0; Index < RemainingMagazines; ++Index)
				{
					Person.EquippedItems.Add(AmmunitionItemId);
				}
			}
		}

		TSet<FName> DeviceItemIds;
		for (const FName ItemId : Person.EquippedItems)
		{
			const FItemRule* Item = Rules.Items.Find(ItemId);
			if (Item != nullptr && Item->IsTacticalDevice())
			{
				DeviceItemIds.Add(ItemId);
			}
		}
		for (const FName DeviceItemId : DeviceItemIds)
		{
			const FInventoryStack* Remaining = Unit.CarriedItems.FindByPredicate(
				[DeviceItemId](const FInventoryStack& Stack) { return Stack.ItemId == DeviceItemId; });
			const int32 RemainingDevices = Remaining != nullptr ? Remaining->Quantity : 0;
			Person.EquippedItems.Remove(DeviceItemId);
			for (int32 Index = 0; Index < RemainingDevices; ++Index)
			{
				Person.EquippedItems.Add(DeviceItemId);
			}
		}
	}

	FAdversaryMissionState* FindAdversaryMission(FCampaignState& State, const FGuid& ContactId)
	{
		return State.AdversaryMissions.FindByPredicate(
			[&ContactId](const FAdversaryMissionState& Mission) { return Mission.ContactId == ContactId; });
	}

	const FAdversaryMissionState* FindAdversaryMission(const FCampaignState& State, const FGuid& ContactId)
	{
		return State.AdversaryMissions.FindByPredicate(
			[&ContactId](const FAdversaryMissionState& Mission) { return Mission.ContactId == ContactId; });
	}

	const FAdversaryMissionState* FindAdversaryMissionById(const FCampaignState& State, const FGuid& MissionId)
	{
		return State.AdversaryMissions.FindByPredicate(
			[&MissionId](const FAdversaryMissionState& Mission) { return Mission.MissionId == MissionId; });
	}

	FName ResolveMissionRegionId(
		const FCampaignState& State,
		const FAdversaryMissionState& Mission,
		const FAdversaryMissionRule& Rule)
	{
		const FStrategicBaseState* TargetBase = Mission.TargetBaseId.IsValid()
			? FindBase(State, Mission.TargetBaseId)
			: nullptr;
		return TargetBase != nullptr ? TargetBase->RegionId : Rule.TargetRegionId;
	}

	FRegionalPressureState* FindRegionalPressure(FCampaignState& State, const FName RegionId)
	{
		return State.RegionalPressure.FindByPredicate(
			[RegionId](const FRegionalPressureState& Pressure) { return Pressure.RegionId == RegionId; });
	}

	const FRegionalPressureState* FindRegionalPressure(const FCampaignState& State, const FName RegionId)
	{
		return State.RegionalPressure.FindByPredicate(
			[RegionId](const FRegionalPressureState& Pressure) { return Pressure.RegionId == RegionId; });
	}

	bool IsValidMutualAidRoutePolicy(const EMutualAidRoutePolicy Policy)
	{
		return Policy == EMutualAidRoutePolicy::OpenRelay
			|| Policy == EMutualAidRoutePolicy::RapidThread
			|| Policy == EMutualAidRoutePolicy::VeiledChain;
	}

	FName MutualAidRoutePolicyId(const EMutualAidRoutePolicy Policy)
	{
		switch (Policy)
		{
		case EMutualAidRoutePolicy::OpenRelay:
			return TEXT("logistics.mutual-aid-open-relay");
		case EMutualAidRoutePolicy::RapidThread:
			return TEXT("logistics.mutual-aid-rapid-thread");
		case EMutualAidRoutePolicy::VeiledChain:
			return TEXT("logistics.mutual-aid-veiled-chain");
		default:
			return NAME_None;
		}
	}

	FGuid MutualAidCurrentLegOriginBaseId(const FMutualAidConvoyState& Convoy)
	{
		return Convoy.CurrentLegOriginBaseId.IsValid()
			? Convoy.CurrentLegOriginBaseId
			: Convoy.SourceBaseId;
	}

	bool IsValidMutualAidWaypointState(
		const FCampaignState& State,
		const FMutualAidConvoyState& Convoy)
	{
		const FGuid CurrentLegOriginBaseId = MutualAidCurrentLegOriginBaseId(Convoy);
		if (FindBase(State, CurrentLegOriginBaseId) == nullptr
			|| CurrentLegOriginBaseId == Convoy.DestinationBaseId)
		{
			return false;
		}

		if (!Convoy.RelayWaypointBaseId.IsValid())
		{
			return (!Convoy.CurrentLegOriginBaseId.IsValid()
				|| Convoy.CurrentLegOriginBaseId != Convoy.SourceBaseId)
				&& Convoy.OnwardRoutePolicy == EMutualAidRoutePolicy::OpenRelay
				&& Convoy.OnwardTotalTransitSeconds == 0
				&& Convoy.OnwardRoutePressure == 0
				&& Convoy.bOnwardInterdictionResolved
				&& Convoy.OnwardForecastInterdictionDelaySeconds == 0
				&& Convoy.BalancedHandoffQuantity == 0;
		}

		return CurrentLegOriginBaseId == Convoy.SourceBaseId
			&& FindBase(State, Convoy.RelayWaypointBaseId) != nullptr
			&& Convoy.RelayWaypointBaseId != Convoy.SourceBaseId
			&& Convoy.RelayWaypointBaseId != Convoy.DestinationBaseId
			&& IsValidMutualAidRoutePolicy(Convoy.OnwardRoutePolicy)
			&& Convoy.OnwardTotalTransitSeconds > 0
			&& Convoy.OnwardRoutePressure >= 0
			&& Convoy.OnwardRoutePressure <= 100
			&& Convoy.OnwardForecastInterdictionDelaySeconds > 0
			&& Convoy.OnwardForecastInterdictionDelaySeconds
				<= Convoy.OnwardTotalTransitSeconds / 2
			&& (Convoy.BalancedHandoffQuantity == 0
				|| (Convoy.Quantity >= 2
					&& Convoy.BalancedHandoffQuantity == Convoy.Quantity / 2));
	}

	void ClearMutualAidOnwardRoute(FMutualAidConvoyState& Convoy)
	{
		Convoy.RelayWaypointBaseId.Invalidate();
		Convoy.OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;
		Convoy.OnwardTotalTransitSeconds = 0;
		Convoy.OnwardRoutePressure = 0;
		Convoy.bOnwardInterdictionResolved = true;
		Convoy.OnwardForecastInterdictionDelaySeconds = 0;
		Convoy.BalancedHandoffQuantity = 0;
	}

	bool IsMutualAidRoutingConfigValid(const FStrategicSimulationConfig& Config)
	{
		return Config.MutualAidRapidThreadTransitHours > 0
			&& Config.MutualAidRapidThreadTransitHours < Config.MutualAidConvoyTransitHours
			&& Config.MutualAidConvoyTransitHours < Config.MutualAidVeiledChainTransitHours
			&& Config.MutualAidVeiledChainTransitHours <= 8760
			&& Config.MutualAidRapidThreadExposure >= 0
			&& Config.MutualAidRapidThreadExposure <= 100
			&& Config.MutualAidVeiledChainExposureReduction >= 0
			&& Config.MutualAidVeiledChainExposureReduction <= 100
			&& Config.MutualAidInterdictionThreshold > 0
			&& Config.MutualAidInterdictionThreshold <= 100
			&& Config.MutualAidInterdictionDelayHours > 0
			&& Config.MutualAidInterdictionDelayHours <= 8760
			&& static_cast<int64>(Config.MutualAidInterdictionDelayHours) * 2
				<= Config.MutualAidRapidThreadTransitHours
			&& Config.MutualAidSignalEscortCost >= 0;
	}

	FRegionalMandateState* FindRegionalMandate(FCampaignState& State, const FName RegionId)
	{
		return State.RegionalMandates.FindByPredicate(
			[RegionId](const FRegionalMandateState& Mandate) { return Mandate.RegionId == RegionId; });
	}

	const FRegionalMandateState* FindRegionalMandate(const FCampaignState& State, const FName RegionId)
	{
		return State.RegionalMandates.FindByPredicate(
			[RegionId](const FRegionalMandateState& Mandate) { return Mandate.RegionId == RegionId; });
	}

	bool IsActiveHorizonCompactMember(
		const FCampaignState& State,
		const FRegionalMandateState& Mandate)
	{
		return State.bHorizonCompactRatified
			&& Mandate.bResilienceCharterSigned
			&& !Mandate.bHorizonCompactMemberWithdrawn;
	}

	bool WithdrawHorizonCompactMemberIfRequired(
		const FCampaignState& State,
		FRegionalMandateState& Mandate,
		const FStrategicSimulationConfig& Config)
	{
		if (!IsActiveHorizonCompactMember(State, Mandate)
			|| Mandate.Support >= Config.HorizonCompactWithdrawalSupportThreshold)
		{
			return false;
		}
		Mandate.bHorizonCompactMemberWithdrawn = true;
		return true;
	}

	const FRegionalPressureState* FindHorizonCompactPressureRecipient(
		const FCampaignState& State,
		const FName TargetRegionId)
	{
		const FRegionalPressureState* Best = nullptr;
		for (const FRegionalMandateState& Mandate : State.RegionalMandates)
		{
			if (!IsActiveHorizonCompactMember(State, Mandate)
				|| Mandate.RegionId == TargetRegionId)
			{
				continue;
			}
			const FRegionalPressureState* Candidate = FindRegionalPressure(State, Mandate.RegionId);
			if (Candidate == nullptr)
			{
				return nullptr;
			}
			if (Best == nullptr || Candidate->Pressure < Best->Pressure
				|| (Candidate->Pressure == Best->Pressure
					&& Candidate->RegionId.LexicalLess(Best->RegionId)))
			{
				Best = Candidate;
			}
		}
		return Best;
	}

	int32 GetDiplomaticMonthSerial(const FDateTime& Utc)
	{
		return Utc.GetYear() * 12 + Utc.GetMonth() - 1;
	}

	bool AreValidCoordinates(const int32 LongitudeMilliDegrees, const int32 LatitudeMilliDegrees)
	{
		return LongitudeMilliDegrees >= -180000 && LongitudeMilliDegrees <= 180000
			&& LatitudeMilliDegrees >= -90000 && LatitudeMilliDegrees <= 90000;
	}

	int64 SignedWrappedLongitudeDelta(const int32 FromLongitude, const int32 ToLongitude)
	{
		int64 Delta = static_cast<int64>(ToLongitude) - FromLongitude;
		if (Delta > 180000)
		{
			Delta -= 360000;
		}
		else if (Delta < -180000)
		{
			Delta += 360000;
		}
		return Delta;
	}

	int32 WrapLongitude(const int64 Longitude)
	{
		int64 Wrapped = Longitude;
		while (Wrapped > 180000)
		{
			Wrapped -= 360000;
		}
		while (Wrapped < -180000)
		{
			Wrapped += 360000;
		}
		return static_cast<int32>(Wrapped);
	}

	uint64 IntegerSquareRoot(const uint64 Value)
	{
		uint64 Result = 0;
		uint64 Bit = 1ULL << 62;
		while (Bit > Value)
		{
			Bit >>= 2;
		}
		uint64 Remainder = Value;
		while (Bit != 0)
		{
			if (Remainder >= Result + Bit)
			{
				Remainder -= Result + Bit;
				Result = (Result >> 1) + Bit;
			}
			else
			{
				Result >>= 1;
			}
			Bit >>= 2;
		}
		return Result;
	}

	int64 ApproximateSurfaceDistanceKilometers(
		const int32 FromLongitude,
		const int32 FromLatitude,
		const int32 ToLongitude,
		const int32 ToLatitude)
	{
		const int64 LongitudeDelta = SignedWrappedLongitudeDelta(FromLongitude, ToLongitude);
		const int64 LatitudeDelta = static_cast<int64>(ToLatitude) - FromLatitude;
		const uint64 SquaredDistance = static_cast<uint64>(LongitudeDelta * LongitudeDelta + LatitudeDelta * LatitudeDelta);
		const uint64 MilliDegrees = IntegerSquareRoot(SquaredDistance);
		return static_cast<int64>((MilliDegrees * 111ULL + 999ULL) / 1000ULL);
	}

	bool ComputeTravelSeconds(const int64 DistanceKilometers, const int32 SpeedKilometersPerHour, int64& OutSeconds)
	{
		if (DistanceKilometers < 0 || SpeedKilometersPerHour <= 0 || DistanceKilometers > MAX_int64 / 3600LL)
		{
			return false;
		}
		const int64 Numerator = DistanceKilometers * 3600LL;
		if (Numerator > MAX_int64 - (SpeedKilometersPerHour - 1LL))
		{
			return false;
		}
		OutSeconds = FMath::Max<int64>(5, (Numerator + SpeedKilometersPerHour - 1) / SpeedKilometersPerHour);
		return true;
	}

	void ComputeContactPosition(
		const FStrategicContactState& Contact,
		const int64 ElapsedRouteSeconds,
		int32& OutLongitudeMilliDegrees,
		int32& OutLatitudeMilliDegrees)
	{
		check(Contact.TotalRouteSeconds > 0);
		const int64 ClampedElapsed = FMath::Clamp<int64>(ElapsedRouteSeconds, 0, Contact.TotalRouteSeconds);
		const int64 LongitudeDelta = SignedWrappedLongitudeDelta(
			Contact.OriginLongitudeMilliDegrees,
			Contact.DestinationLongitudeMilliDegrees);
		const int64 LatitudeDelta = static_cast<int64>(Contact.DestinationLatitudeMilliDegrees)
			- Contact.OriginLatitudeMilliDegrees;
		OutLongitudeMilliDegrees = WrapLongitude(
			static_cast<int64>(Contact.OriginLongitudeMilliDegrees)
			+ LongitudeDelta * ClampedElapsed / Contact.TotalRouteSeconds);
		OutLatitudeMilliDegrees = static_cast<int32>(
			static_cast<int64>(Contact.OriginLatitudeMilliDegrees)
			+ LatitudeDelta * ClampedElapsed / Contact.TotalRouteSeconds);
	}

	bool IsValidPersonnelStatus(const EPersonnelStatus Status)
	{
		return Status == EPersonnelStatus::Available
			|| Status == EPersonnelStatus::Recovering
			|| Status == EPersonnelStatus::Training
			|| Status == EPersonnelStatus::Deployed
			|| Status == EPersonnelStatus::Stewarding;
	}

	bool IsValidTrainingFocus(const EPersonnelTrainingFocus Focus)
	{
		return Focus == EPersonnelTrainingFocus::Accuracy
			|| Focus == EPersonnelTrainingFocus::Resolve
			|| Focus == EPersonnelTrainingFocus::Mobility
			|| Focus == EPersonnelTrainingFocus::Strength;
	}

	bool HasPendingRecoveryPlan(const FCampaignState& State)
	{
		return State.Personnel.ContainsByPredicate(
			[](const FPersonnelState& Person)
			{
				return Person.Status == EPersonnelStatus::Recovering
					&& Person.RecoveryPlan == EPersonnelRecoveryPlan::DecisionRequired;
			});
	}

	bool IsUsablePersonnelName(const FString& Name)
	{
		return !Name.TrimStartAndEnd().IsEmpty() && Name.Len() <= 64;
	}

	bool PersonnelGuidLess(const FGuid& Left, const FGuid& Right)
	{
		return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
	}

	struct FPersonnelSquadBondAdvance
	{
		FGuid FirstPersonnelId;
		FGuid SecondPersonnelId;
		int32 PreviousSharedVictories = 0;
		int32 SharedVictories = 0;
		EPersonnelSquadBondTier PreviousTier = EPersonnelSquadBondTier::None;
		EPersonnelSquadBondTier Tier = EPersonnelSquadBondTier::None;
	};

	bool AdvancePersonnelSquadBonds(
		FCampaignState& State,
		TArray<FGuid> SurvivorIds,
		TArray<FPersonnelSquadBondAdvance>& OutAdvances,
		FStrategicCommandResult& Result)
	{
		OutAdvances.Reset();
		SurvivorIds.RemoveAll([](const FGuid& PersonnelId) { return !PersonnelId.IsValid(); });
		SurvivorIds.Sort(PersonnelGuidLess);
		for (int32 Index = SurvivorIds.Num() - 1; Index > 0; --Index)
		{
			if (SurvivorIds[Index] == SurvivorIds[Index - 1])
			{
				SurvivorIds.RemoveAt(Index, EAllowShrinking::No);
			}
		}

		int32 NewRecordCount = 0;
		for (int32 FirstIndex = 0; FirstIndex < SurvivorIds.Num(); ++FirstIndex)
		{
			for (int32 SecondIndex = FirstIndex + 1; SecondIndex < SurvivorIds.Num(); ++SecondIndex)
			{
				const FGuid FirstId = SurvivorIds[FirstIndex];
				const FGuid SecondId = SurvivorIds[SecondIndex];
				const FPersonnelSquadBondState* Existing = State.PersonnelSquadBonds.FindByPredicate(
					[&FirstId, &SecondId](const FPersonnelSquadBondState& Bond)
					{
						return Bond.FirstPersonnelId == FirstId && Bond.SecondPersonnelId == SecondId;
					});
				if (Existing != nullptr && Existing->SharedVictories == MAX_int32)
				{
					AddError(Result, TEXT("personnel_squad_bond_overflow"),
						TEXT("A surviving personnel pair cannot accept another shared victory."));
					return false;
				}
				NewRecordCount += Existing == nullptr ? 1 : 0;
			}
		}
		if (State.PersonnelSquadBonds.Num() > 10000 - NewRecordCount)
		{
			AddError(Result, TEXT("personnel_squad_bond_capacity"),
				TEXT("The campaign cannot retain additional personnel squad-bond records."));
			return false;
		}

		for (int32 FirstIndex = 0; FirstIndex < SurvivorIds.Num(); ++FirstIndex)
		{
			for (int32 SecondIndex = FirstIndex + 1; SecondIndex < SurvivorIds.Num(); ++SecondIndex)
			{
				const FGuid FirstId = SurvivorIds[FirstIndex];
				const FGuid SecondId = SurvivorIds[SecondIndex];
				FPersonnelSquadBondState* Bond = State.PersonnelSquadBonds.FindByPredicate(
					[&FirstId, &SecondId](const FPersonnelSquadBondState& Entry)
					{
						return Entry.FirstPersonnelId == FirstId && Entry.SecondPersonnelId == SecondId;
					});
				if (Bond == nullptr)
				{
					Bond = &State.PersonnelSquadBonds.AddDefaulted_GetRef();
					Bond->FirstPersonnelId = FirstId;
					Bond->SecondPersonnelId = SecondId;
				}
				FPersonnelSquadBondAdvance& Advance = OutAdvances.AddDefaulted_GetRef();
				Advance.FirstPersonnelId = FirstId;
				Advance.SecondPersonnelId = SecondId;
				Advance.PreviousSharedVictories = Bond->SharedVictories;
				Advance.PreviousTier = FPersonnelSquadBond::GetTier(Bond->SharedVictories);
				++Bond->SharedVictories;
				Advance.SharedVictories = Bond->SharedVictories;
				Advance.Tier = FPersonnelSquadBond::GetTier(Bond->SharedVictories);
			}
		}
		return true;
	}

	void AwardEligiblePersonnelCommendations(
		FPersonnelState& Person,
		const bool bMissionSucceeded,
		const FResolvedRuleSet& Rules,
		TArray<FName>& OutAwardedIds)
	{
		TArray<FName> CommendationIds;
		Rules.PersonnelCommendations.GenerateKeyArray(CommendationIds);
		CommendationIds.Sort(FNameLexicalLess());
		for (const FName CommendationId : CommendationIds)
		{
			const FPersonnelCommendationRule& Rule = Rules.PersonnelCommendations.FindChecked(CommendationId);
			if (Person.Commendations.Contains(CommendationId)
				|| (Rule.bRequiresSuccessfulMission && !bMissionSucceeded)
				|| Person.Missions < Rule.RequiredMissions
				|| Person.Kills < Rule.RequiredKills
				|| Person.Rank < Rule.RequiredRank)
			{
				continue;
			}
			Person.Commendations.Add(CommendationId);
			OutAwardedIds.Add(CommendationId);
		}
		Person.Commendations.Sort(FNameLexicalLess());
	}

	bool IsValidCraftStatus(const ECraftStatus Status)
	{
		return Status == ECraftStatus::Grounded
			|| Status == ECraftStatus::Servicing
			|| Status == ECraftStatus::Airborne
			|| Status == ECraftStatus::Intercepting
			|| Status == ECraftStatus::Returning
			|| Status == ECraftStatus::Deploying
			|| Status == ECraftStatus::OnSite;
	}

	bool IsFlightStatus(const ECraftStatus Status)
	{
		return Status == ECraftStatus::Airborne
			|| Status == ECraftStatus::Intercepting
			|| Status == ECraftStatus::Returning
			|| Status == ECraftStatus::Deploying
			|| Status == ECraftStatus::OnSite;
	}

	bool IsValidContactStatus(const EStrategicContactStatus Status)
	{
		return Status == EStrategicContactStatus::Hidden
			|| Status == EStrategicContactStatus::Detected
			|| Status == EStrategicContactStatus::Engaged;
	}

	bool IsValidCampaignOutcome(const ECampaignOutcome Outcome)
	{
		return Outcome == ECampaignOutcome::Ongoing
			|| Outcome == ECampaignOutcome::Victory
			|| Outcome == ECampaignOutcome::Failure;
	}

	bool IsUsableCraftName(const FString& Name)
	{
		return !Name.TrimStartAndEnd().IsEmpty() && Name.Len() <= 64;
	}

	bool IsEquippableCraftItem(const FItemRule& Item)
	{
		return Item.Category.ToString().StartsWith(TEXT("craft-"))
			&& Item.Category != FName(TEXT("craft-ammunition"));
	}

	bool IsEquippablePersonnelItem(const FItemRule& Item)
	{
		return Item.Category == FName(TEXT("sensor"))
			|| Item.Category == FName(TEXT("armor"))
			|| Item.Category == FName(TEXT("weapon"))
			|| Item.Category == FName(TEXT("ammunition"))
			|| Item.Category == FName(TEXT("medical"))
			|| Item.Category == FName(TEXT("device"));
	}

	bool IsValidCraftWeaponRule(const FItemRule& Weapon, const FResolvedRuleSet& Rules)
	{
		const FItemRule* Ammunition = Rules.Items.Find(Weapon.AmmunitionItemId);
		return Weapon.IsCraftWeapon()
			&& Ammunition != nullptr && Ammunition->Category == FName(TEXT("craft-ammunition"))
			&& Weapon.MagazineCapacity > 0
			&& Weapon.SalvoSize > 0 && Weapon.SalvoSize <= 16
			&& Weapon.InterceptionAccuracy > 0 && Weapon.InterceptionAccuracy <= 100
			&& Weapon.InterceptionDamage > 0
			&& Weapon.FireIntervalSeconds > 0;
	}

	int32 CountEquippedItem(const TArray<FName>& EquipmentItems, const FName ItemId)
	{
		int32 Count = 0;
		for (const FName EquippedItemId : EquipmentItems)
		{
			Count += EquippedItemId == ItemId ? 1 : 0;
		}
		return Count;
	}

	bool TryAdd(int64 Left, int64 Right, int64& OutValue);
	bool TryMultiplyNonNegative(int64 Left, int64 Right, int64& OutValue);
	bool TryScaleNonNegativeByPercent(int64 BaseValue, int32 Percent, int64& OutValue);
	bool WouldExhaustDeterministicRandomStream(
		const FDeterministicRandomStream& Stream,
		int64 RequiredDraws);

	bool TryComputeCargoMass(
		const TArray<FInventoryStack>& Cargo,
		const FResolvedRuleSet& Rules,
		int64& OutMass)
	{
		OutMass = 0;
		TSet<FName> SeenItems;
		for (const FInventoryStack& Stack : Cargo)
		{
			const FItemRule* Item = Rules.Items.Find(Stack.ItemId);
			int64 StackMass = 0;
			if (Item == nullptr || Stack.Quantity <= 0 || SeenItems.Contains(Stack.ItemId)
				|| !TryMultiplyNonNegative(Item->Mass, Stack.Quantity, StackMass)
				|| !TryAdd(OutMass, StackMass, OutMass))
			{
				return false;
			}
			SeenItems.Add(Stack.ItemId);
		}
		return true;
	}

	bool IsAgentAssignedToCraft(const FCampaignState& State, const FGuid& PersonnelId)
	{
		return State.Craft.ContainsByPredicate(
			[&PersonnelId](const FCraftState& Craft)
			{
				return Craft.AssignedPilotId == PersonnelId || Craft.AssignedAgentIds.Contains(PersonnelId);
			});
	}

	bool IsPersonnelIdentityInUse(const FCampaignState& State, const FGuid& PersonnelId)
	{
		return State.Personnel.ContainsByPredicate(
			[&PersonnelId](const FPersonnelState& Person) { return Person.PersonnelId == PersonnelId; })
			|| State.RecruitmentOrders.ContainsByPredicate(
				[&PersonnelId](const FRecruitmentOrderState& Order) { return Order.PersonnelId == PersonnelId; })
			|| State.Memorial.ContainsByPredicate(
				[&PersonnelId](const FMemorialRecord& Record) { return Record.PersonnelId == PersonnelId; });
	}

	bool IsCraftIdentityInUse(const FCampaignState& State, const FGuid& CraftId)
	{
		return State.Craft.ContainsByPredicate(
			[&CraftId](const FCraftState& Craft) { return Craft.CraftId == CraftId; })
			|| State.CraftAcquisitionOrders.ContainsByPredicate(
				[&CraftId](const FCraftAcquisitionOrderState& Order) { return Order.CraftId == CraftId; });
	}

	bool TryAdjustInventory(FStrategicBaseState& Base, const FName ItemId, const int32 Delta)
	{
		FInventoryStack* Stack = Base.Inventory.FindByPredicate(
			[ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		if (Stack == nullptr)
		{
			if (Delta <= 0)
			{
				return false;
			}
			Stack = &Base.Inventory.AddDefaulted_GetRef();
			Stack->ItemId = ItemId;
		}

		const int64 NewQuantity = static_cast<int64>(Stack->Quantity) + Delta;
		if (NewQuantity < 0 || NewQuantity > MAX_int32)
		{
			return false;
		}
		Stack->Quantity = static_cast<int32>(NewQuantity);
		if (Stack->Quantity == 0)
		{
			Base.Inventory.RemoveAll(
				[ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		}
		return true;
	}

	bool TryAdjustInventoryStacks(TArray<FInventoryStack>& Stacks, const FName ItemId, const int32 Delta)
	{
		FInventoryStack* Stack = Stacks.FindByPredicate(
			[ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		if (Stack == nullptr)
		{
			if (Delta <= 0 || Stacks.Num() >= 64)
			{
				return false;
			}
			Stack = &Stacks.AddDefaulted_GetRef();
			Stack->ItemId = ItemId;
		}
		const int64 NewQuantity = static_cast<int64>(Stack->Quantity) + Delta;
		if (NewQuantity < 0 || NewQuantity > MAX_int32)
		{
			return false;
		}
		Stack->Quantity = static_cast<int32>(NewQuantity);
		if (Stack->Quantity == 0)
		{
			Stacks.RemoveAll([ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		}
		return true;
	}

	bool IsFacilityOperational(const FBaseFacilityState& Facility, const FResolvedRuleSet& Rules)
	{
		const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
		return Rule != nullptr && Rule->MaxIntegrity > 0
			&& Facility.Damage >= 0 && Facility.Damage < Rule->MaxIntegrity;
	}

	struct FBaseDefenseShotPlan
	{
		FGuid FacilityInstanceId;
		FName FacilityId;
		FName SupplyItemId;
		int32 SupplyQuantity = 0;
		int32 Accuracy = 0;
		int32 Damage = 0;
	};

	struct FBaseDefenseVolleyPlan
	{
		int32 OperationalBatteryCount = 0;
		int32 MaximumDamage = 0;
		int32 ExpectedDamage = 0;
		TArray<FBaseDefenseShotPlan> ReadyShots;
		TArray<FBaseDefenseSupplyEvaluation> Supplies;
	};

	bool BuildBaseDefenseVolleyPlan(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const EBaseDefenseFireDoctrine Doctrine,
		FBaseDefenseVolleyPlan& OutPlan,
		FStrategicCommandResult& Result)
	{
		OutPlan = FBaseDefenseVolleyPlan();
		const bool bGridOvercharge = Doctrine == EBaseDefenseFireDoctrine::GridOvercharge;
		auto AdjustAccuracy = [bGridOvercharge, &Config](const int32 Accuracy)
		{
			return bGridOvercharge
				? FMath::Min(100, Accuracy + Config.BaseDefenseGridOverchargeAccuracyBonus)
				: Accuracy;
		};
		auto AdjustDamage = [bGridOvercharge, &Config](const int32 Damage)
		{
			if (!bGridOvercharge)
			{
				return Damage;
			}
			const int64 Numerator = static_cast<int64>(Damage)
				* Config.BaseDefenseGridOverchargeDamagePercent;
			return static_cast<int32>((Numerator + 99) / 100);
		};
		TArray<const FBaseFacilityState*> Batteries;
		for (const FBaseFacilityState& Facility : Base.Facilities)
		{
			const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
			if (Rule == nullptr || Rule->MaxIntegrity <= 0
				|| Facility.Damage < 0 || Facility.Damage > Rule->MaxIntegrity
				|| Rule->BaseDefenseAccuracy < 0 || Rule->BaseDefenseAccuracy > 100
				|| Rule->BaseDefenseDamage < 0 || Rule->BaseDefenseDamage > 100000
				|| ((Rule->BaseDefenseAccuracy == 0) != (Rule->BaseDefenseDamage == 0))
				|| (Rule->BaseDefenseSupplyItemId.IsNone()
					? Rule->BaseDefenseSupplyPerShot != 0
					: Rule->BaseDefenseAccuracy == 0 || Rule->BaseDefenseSupplyPerShot <= 0
						|| Rule->BaseDefenseSupplyPerShot > 100000))
			{
				AddError(Result, TEXT("invalid_base_defense_rule"), FString::Printf(
					TEXT("Base '%s' has an invalid base-defense facility or supply profile."), *Base.Name));
				return false;
			}
			const int32 Accuracy = Rule->ScaleEffectByIntegrity(Rule->BaseDefenseAccuracy, Facility.Damage);
			const int32 Damage = Rule->ScaleEffectByIntegrity(Rule->BaseDefenseDamage, Facility.Damage);
			if (Accuracy > 0 && Damage > 0)
			{
				Batteries.Add(&Facility);
			}
		}
		Batteries.Sort([Doctrine, &Rules, &AdjustAccuracy, &AdjustDamage](
			const FBaseFacilityState& Left,
			const FBaseFacilityState& Right)
		{
			const FFacilityRule& LeftRule = Rules.Facilities.FindChecked(Left.FacilityId);
			const FFacilityRule& RightRule = Rules.Facilities.FindChecked(Right.FacilityId);
			const int32 LeftAccuracy = AdjustAccuracy(LeftRule.ScaleEffectByIntegrity(
				LeftRule.BaseDefenseAccuracy, Left.Damage));
			const int32 RightAccuracy = AdjustAccuracy(RightRule.ScaleEffectByIntegrity(
				RightRule.BaseDefenseAccuracy, Right.Damage));
			const int32 LeftDamage = AdjustDamage(LeftRule.ScaleEffectByIntegrity(
				LeftRule.BaseDefenseDamage, Left.Damage));
			const int32 RightDamage = AdjustDamage(RightRule.ScaleEffectByIntegrity(
				RightRule.BaseDefenseDamage, Right.Damage));
			if (Doctrine == EBaseDefenseFireDoctrine::PrecisionScreen)
			{
				if (LeftAccuracy != RightAccuracy)
				{
					return LeftAccuracy > RightAccuracy;
				}
				if (LeftDamage != RightDamage)
				{
					return LeftDamage > RightDamage;
				}
			}
			else if (Doctrine == EBaseDefenseFireDoctrine::BreachBreaker
				|| Doctrine == EBaseDefenseFireDoctrine::GridOvercharge)
			{
				if (LeftDamage != RightDamage)
				{
					return LeftDamage > RightDamage;
				}
				if (LeftAccuracy != RightAccuracy)
				{
					return LeftAccuracy > RightAccuracy;
				}
			}
			return Left.InstanceId.ToString(EGuidFormats::Digits)
				< Right.InstanceId.ToString(EGuidFormats::Digits);
		});

		TMap<FName, int64> RequiredByItem;
		TMap<FName, int32> AvailableByItem;
		TMap<FName, int64> RemainingByItem;
		TMap<FName, int64> AllocatedByItem;
		int64 MaximumDamage = 0;
		int64 ExpectedDamageHundredths = 0;
		for (const FBaseFacilityState* Facility : Batteries)
		{
			check(Facility != nullptr);
			const FFacilityRule& Rule = Rules.Facilities.FindChecked(Facility->FacilityId);
			++OutPlan.OperationalBatteryCount;
			bool bReady = Rule.BaseDefenseSupplyItemId.IsNone();
			if (!Rule.BaseDefenseSupplyItemId.IsNone())
			{
				const FItemRule* SupplyRule = Rules.Items.Find(Rule.BaseDefenseSupplyItemId);
				if (SupplyRule == nullptr || SupplyRule->Category != FName(TEXT("base-defense-supply")))
				{
					AddError(Result, TEXT("missing_base_defense_supply_reference"), FString::Printf(
						TEXT("Base-defense facility '%s' requires unavailable supply item '%s'."),
						*Facility->FacilityId.ToString(), *Rule.BaseDefenseSupplyItemId.ToString()));
					return false;
				}
				int64& Required = RequiredByItem.FindOrAdd(Rule.BaseDefenseSupplyItemId);
				if (!TryAdd(Required, Rule.BaseDefenseSupplyPerShot, Required) || Required > MAX_int32)
				{
					AddError(Result, TEXT("base_defense_supply_overflow"),
						TEXT("A base-defense supply requirement exceeds the supported inventory range."));
					return false;
				}
				if (!RemainingByItem.Contains(Rule.BaseDefenseSupplyItemId))
				{
					int32 Available = 0;
					bool bFound = false;
					for (const FInventoryStack& Stack : Base.Inventory)
					{
						if (Stack.ItemId != Rule.BaseDefenseSupplyItemId)
						{
							continue;
						}
						if (bFound || Stack.Quantity <= 0)
						{
							AddError(Result, TEXT("invalid_base_defense_supply_inventory"), FString::Printf(
								TEXT("Base '%s' has invalid inventory for defense supply '%s'."),
								*Base.Name, *Rule.BaseDefenseSupplyItemId.ToString()));
							return false;
						}
						bFound = true;
						Available = Stack.Quantity;
					}
					AvailableByItem.Add(Rule.BaseDefenseSupplyItemId, Available);
					RemainingByItem.Add(Rule.BaseDefenseSupplyItemId, Available);
				}
				int64& Remaining = RemainingByItem.FindChecked(Rule.BaseDefenseSupplyItemId);
				if (Remaining >= Rule.BaseDefenseSupplyPerShot)
				{
					Remaining -= Rule.BaseDefenseSupplyPerShot;
					int64& Allocated = AllocatedByItem.FindOrAdd(Rule.BaseDefenseSupplyItemId);
					if (!TryAdd(Allocated, Rule.BaseDefenseSupplyPerShot, Allocated)
						|| Allocated > MAX_int32)
					{
						AddError(Result, TEXT("base_defense_supply_overflow"),
							TEXT("Allocated base-defense supply exceeds the supported inventory range."));
						return false;
					}
					bReady = true;
				}
			}
			if (!bReady)
			{
				continue;
			}
			FBaseDefenseShotPlan& Shot = OutPlan.ReadyShots.AddDefaulted_GetRef();
			Shot.FacilityInstanceId = Facility->InstanceId;
			Shot.FacilityId = Facility->FacilityId;
			Shot.SupplyItemId = Rule.BaseDefenseSupplyItemId;
			Shot.SupplyQuantity = Rule.BaseDefenseSupplyPerShot;
			Shot.Accuracy = AdjustAccuracy(
				Rule.ScaleEffectByIntegrity(Rule.BaseDefenseAccuracy, Facility->Damage));
			Shot.Damage = AdjustDamage(
				Rule.ScaleEffectByIntegrity(Rule.BaseDefenseDamage, Facility->Damage));
			if (!TryAdd(MaximumDamage, Shot.Damage, MaximumDamage)
				|| !TryAdd(ExpectedDamageHundredths,
					static_cast<int64>(Shot.Accuracy) * Shot.Damage, ExpectedDamageHundredths)
				|| MaximumDamage > MAX_int32 || ExpectedDamageHundredths > MAX_int32 * 100LL)
			{
				AddError(Result, TEXT("base_defense_overflow"), FString::Printf(
					TEXT("Base '%s' ready defense damage exceeds the supported range."), *Base.Name));
				return false;
			}
		}
		OutPlan.MaximumDamage = static_cast<int32>(MaximumDamage);
		OutPlan.ExpectedDamage = static_cast<int32>((ExpectedDamageHundredths + 50) / 100);

		TArray<FName> SupplyItemIds;
		RequiredByItem.GetKeys(SupplyItemIds);
		SupplyItemIds.Sort(FNameLexicalLess());
		for (const FName ItemId : SupplyItemIds)
		{
			FBaseDefenseSupplyEvaluation& Supply = OutPlan.Supplies.AddDefaulted_GetRef();
			Supply.ItemId = ItemId;
			Supply.RequiredQuantity = static_cast<int32>(RequiredByItem.FindChecked(ItemId));
			Supply.AvailableQuantity = AvailableByItem.FindRef(ItemId);
			Supply.AllocatedQuantity = static_cast<int32>(AllocatedByItem.FindRef(ItemId));
		}
		return true;
	}

	bool HasOperationalFacility(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		const FName FacilityId)
	{
		return Base.Facilities.ContainsByPredicate(
			[FacilityId, &Rules](const FBaseFacilityState& Facility)
			{
				return Facility.FacilityId == FacilityId && IsFacilityOperational(Facility, Rules);
			})
			|| Base.BuiltFacilities.Contains(FacilityId);
	}

	bool HasOperationalResearchFacilities(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		const FResearchRule& Research,
		TArray<FName>* OutMissingFacilityIds = nullptr)
	{
		if (OutMissingFacilityIds != nullptr)
		{
			OutMissingFacilityIds->Reset();
		}
		bool bAllOperational = true;
		for (const FName FacilityId : Research.RequiredFacilityIds)
		{
			if (HasOperationalFacility(Base, Rules, FacilityId))
			{
				continue;
			}
			bAllOperational = false;
			if (OutMissingFacilityIds != nullptr)
			{
				OutMissingFacilityIds->Add(FacilityId);
			}
		}
		return bAllOperational;
	}

	FGuid MakeDeterministicFacilityId(const FGuid& BaseId, const FName FacilityId, const int32 Index)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%s|%d"),
			*BaseId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*FacilityId.ToString(),
			Index);
		FGuid Result(
			FCrc::StrCrc32(*(Key + TEXT("|a"))) ^ BaseId.A,
			FCrc::StrCrc32(*(Key + TEXT("|b"))) ^ BaseId.B,
			FCrc::StrCrc32(*(Key + TEXT("|c"))) ^ BaseId.C,
			FCrc::StrCrc32(*(Key + TEXT("|d"))) ^ BaseId.D);
		if (!Result.IsValid())
		{
			Result.D = 1;
		}
		return Result;
	}

	FGuid MakeDeterministicMutualAidConvoyId(
		const FGuid& SourceBaseId,
		const FGuid& DestinationBaseId,
		const FName ItemId,
		const int32 Quantity,
		const EMutualAidRoutePolicy RoutePolicy,
		const bool bSignalEscort,
		const int64 CommandSequence)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%s|%s|%d|%d|%d|%lld|mutual-aid-convoy"),
			*SourceBaseId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*DestinationBaseId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*ItemId.ToString(), Quantity, static_cast<int32>(RoutePolicy),
			bSignalEscort ? 1 : 0, CommandSequence);
		FGuid Result(
			FCrc::StrCrc32(*(Key + TEXT("|a"))),
			FCrc::StrCrc32(*(Key + TEXT("|b"))),
			FCrc::StrCrc32(*(Key + TEXT("|c"))),
			FCrc::StrCrc32(*(Key + TEXT("|d"))));
		if (!Result.IsValid())
		{
			Result.D = 1;
		}
		return Result;
	}

	FGuid MakeDeterministicAdversaryId(const int64 InitialSeed, const int64 Serial, const TCHAR* Kind)
	{
		const FString Key = FString::Printf(TEXT("%lld|%lld|%s"), InitialSeed, Serial, Kind);
		FGuid Result(
			FCrc::StrCrc32(*(Key + TEXT("|a"))),
			FCrc::StrCrc32(*(Key + TEXT("|b"))),
			FCrc::StrCrc32(*(Key + TEXT("|c"))),
			FCrc::StrCrc32(*(Key + TEXT("|d"))));
		if (!Result.IsValid())
		{
			Result.D = 1;
		}
		return Result;
	}

	FGuid MakeDeterministicTacticalOperationId(
		const FGuid& CraftId,
		const FGuid& SiteId,
		const int64 CommandSequence)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%s|%lld|tactical-operation"),
			*CraftId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*SiteId.ToString(EGuidFormats::DigitsWithHyphensLower),
			CommandSequence);
		FGuid Result(
			FCrc::StrCrc32(*(Key + TEXT("|a"))),
			FCrc::StrCrc32(*(Key + TEXT("|b"))),
			FCrc::StrCrc32(*(Key + TEXT("|c"))),
			FCrc::StrCrc32(*(Key + TEXT("|d"))));
		if (!Result.IsValid())
		{
			Result.D = 1;
		}
		return Result;
	}

	FGuid MakeDeterministicBaseAssaultId(const FGuid& MissionId, const FGuid& BaseId)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%s|base-assault"),
			*MissionId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
		FGuid Result(
			FCrc::StrCrc32(*(Key + TEXT("|a"))),
			FCrc::StrCrc32(*(Key + TEXT("|b"))),
			FCrc::StrCrc32(*(Key + TEXT("|c"))),
			FCrc::StrCrc32(*(Key + TEXT("|d"))));
		if (!Result.IsValid())
		{
			Result.D = 1;
		}
		return Result;
	}

	bool RectanglesOverlap(
		const int32 AX,
		const int32 AY,
		const int32 AWidth,
		const int32 AHeight,
		const int32 BX,
		const int32 BY,
		const int32 BWidth,
		const int32 BHeight)
	{
		const int64 ARight = static_cast<int64>(AX) + AWidth;
		const int64 ABottom = static_cast<int64>(AY) + AHeight;
		const int64 BRight = static_cast<int64>(BX) + BWidth;
		const int64 BBottom = static_cast<int64>(BY) + BHeight;
		return static_cast<int64>(AX) < BRight && ARight > BX
			&& static_cast<int64>(AY) < BBottom && ABottom > BY;
	}

	bool RectanglesAreAdjacent(
		const int32 AX,
		const int32 AY,
		const int32 AWidth,
		const int32 AHeight,
		const int32 BX,
		const int32 BY,
		const int32 BWidth,
		const int32 BHeight)
	{
		const int64 ARight = static_cast<int64>(AX) + AWidth;
		const int64 ABottom = static_cast<int64>(AY) + AHeight;
		const int64 BRight = static_cast<int64>(BX) + BWidth;
		const int64 BBottom = static_cast<int64>(BY) + BHeight;
		const bool bVerticalEdge = (ARight == BX || BRight == AX)
			&& static_cast<int64>(AY) < BBottom && ABottom > BY;
		const bool bHorizontalEdge = (ABottom == BY || BBottom == AY)
			&& static_cast<int64>(AX) < BRight && ARight > BX;
		return bVerticalEdge || bHorizontalEdge;
	}

	struct FFacilityFootprint
	{
		FName FacilityId;
		int32 GridX = 0;
		int32 GridY = 0;
		int32 GridWidth = 1;
		int32 GridHeight = 1;
	};

	bool ValidateFacilityConnectivityAfterRemoval(
		const FCampaignState& State,
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FGuid RemovedFacilityInstanceId,
		FStrategicCommandResult& Result)
	{
		TArray<FFacilityFootprint> Footprints;
		Footprints.Reserve(Base.Facilities.Num() + State.FacilityConstructionProjects.Num());
		int32 OperationsRoot = INDEX_NONE;
		for (const FBaseFacilityState& Facility : Base.Facilities)
		{
			if (Facility.InstanceId == RemovedFacilityInstanceId)
			{
				continue;
			}
			const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
			if (Rule == nullptr || Rule->GridWidth <= 0 || Rule->GridHeight <= 0)
			{
				AddError(Result, TEXT("unknown_facility"), FString::Printf(
					TEXT("Base '%s' references unavailable facility '%s'."),
					*Base.Name, *Facility.FacilityId.ToString()));
				return false;
			}
			FFacilityFootprint& Footprint = Footprints.AddDefaulted_GetRef();
			Footprint.FacilityId = Facility.FacilityId;
			Footprint.GridX = Facility.GridX;
			Footprint.GridY = Facility.GridY;
			Footprint.GridWidth = Rule->GridWidth;
			Footprint.GridHeight = Rule->GridHeight;
			if (Facility.FacilityId == Config.OperationsFacilityId && OperationsRoot == INDEX_NONE)
			{
				OperationsRoot = Footprints.Num() - 1;
			}
		}
		for (const FFacilityConstructionProjectState& Project : State.FacilityConstructionProjects)
		{
			if (Project.BaseId != Base.BaseId)
			{
				continue;
			}
			const FFacilityRule* Rule = Rules.Facilities.Find(Project.FacilityId);
			if (Rule == nullptr || Rule->GridWidth <= 0 || Rule->GridHeight <= 0)
			{
				AddError(Result, TEXT("unknown_facility"), FString::Printf(
					TEXT("Base '%s' has construction for unavailable facility '%s'."),
					*Base.Name, *Project.FacilityId.ToString()));
				return false;
			}
			FFacilityFootprint& Footprint = Footprints.AddDefaulted_GetRef();
			Footprint.FacilityId = Project.FacilityId;
			Footprint.GridX = Project.GridX;
			Footprint.GridY = Project.GridY;
			Footprint.GridWidth = Rule->GridWidth;
			Footprint.GridHeight = Rule->GridHeight;
		}

		if (OperationsRoot == INDEX_NONE)
		{
			AddError(Result, TEXT("operations_facility_required"), FString::Printf(
				TEXT("Base '%s' must retain an operational '%s'."),
				*Base.Name, *Config.OperationsFacilityId.ToString()));
			return false;
		}

		TArray<int32> Pending;
		TSet<int32> Connected;
		Pending.Add(OperationsRoot);
		Connected.Add(OperationsRoot);
		while (!Pending.IsEmpty())
		{
			const int32 CurrentIndex = Pending.Pop(EAllowShrinking::No);
			const FFacilityFootprint& Current = Footprints[CurrentIndex];
			for (int32 OtherIndex = 0; OtherIndex < Footprints.Num(); ++OtherIndex)
			{
				if (Connected.Contains(OtherIndex))
				{
					continue;
				}
				const FFacilityFootprint& Other = Footprints[OtherIndex];
				if (RectanglesAreAdjacent(
					Current.GridX, Current.GridY, Current.GridWidth, Current.GridHeight,
					Other.GridX, Other.GridY, Other.GridWidth, Other.GridHeight))
				{
					Connected.Add(OtherIndex);
					Pending.Add(OtherIndex);
				}
			}
		}
		if (Connected.Num() != Footprints.Num())
		{
			AddError(Result, TEXT("facility_disconnects_base"), FString::Printf(
				TEXT("Dismantling this facility would disconnect part of base '%s' from its operations network."),
				*Base.Name));
			return false;
		}
		return true;
	}

	bool CanPlaceFacility(
		const FStrategicBaseState& Base,
		const TArray<FFacilityConstructionProjectState>& ConstructionProjects,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FFacilityRule& Candidate,
		const int32 GridX,
		const int32 GridY,
		const bool bRequireAdjacency,
		FName& OutFailureCode)
	{
		if (GridX < 0 || GridY < 0 || Candidate.GridWidth <= 0 || Candidate.GridHeight <= 0
			|| static_cast<int64>(GridX) + Candidate.GridWidth > Config.BaseGridWidth
			|| static_cast<int64>(GridY) + Candidate.GridHeight > Config.BaseGridHeight)
		{
			OutFailureCode = TEXT("facility_out_of_bounds");
			return false;
		}

		bool bAdjacent = !bRequireAdjacency;
		for (const FBaseFacilityState& Existing : Base.Facilities)
		{
			const FFacilityRule* ExistingRule = Rules.Facilities.Find(Existing.FacilityId);
			if (ExistingRule == nullptr)
			{
				OutFailureCode = TEXT("unknown_facility");
				return false;
			}
			if (RectanglesOverlap(GridX, GridY, Candidate.GridWidth, Candidate.GridHeight, Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight))
			{
				OutFailureCode = TEXT("facility_overlap");
				return false;
			}
			bAdjacent |= RectanglesAreAdjacent(GridX, GridY, Candidate.GridWidth, Candidate.GridHeight, Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight);
		}
		for (const FFacilityConstructionProjectState& Existing : ConstructionProjects)
		{
			if (Existing.BaseId != Base.BaseId)
			{
				continue;
			}
			const FFacilityRule* ExistingRule = Rules.Facilities.Find(Existing.FacilityId);
			if (ExistingRule == nullptr)
			{
				OutFailureCode = TEXT("unknown_facility");
				return false;
			}
			if (RectanglesOverlap(GridX, GridY, Candidate.GridWidth, Candidate.GridHeight, Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight))
			{
				OutFailureCode = TEXT("facility_overlap");
				return false;
			}
			bAdjacent |= RectanglesAreAdjacent(GridX, GridY, Candidate.GridWidth, Candidate.GridHeight, Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight);
		}

		if (!bAdjacent)
		{
			OutFailureCode = TEXT("facility_not_adjacent");
			return false;
		}
		OutFailureCode = NAME_None;
		return true;
	}

	bool TryPlaceFacilityFirstFit(
		FStrategicBaseState& Base,
		const FName FacilityId,
		const FGuid InstanceId,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config)
	{
		const FFacilityRule* Rule = Rules.Facilities.Find(FacilityId);
		if (Rule == nullptr)
		{
			return false;
		}
		for (int32 GridY = 0; GridY <= Config.BaseGridHeight - Rule->GridHeight; ++GridY)
		{
			for (int32 GridX = 0; GridX <= Config.BaseGridWidth - Rule->GridWidth; ++GridX)
			{
				FName FailureCode;
				if (CanPlaceFacility(Base, {}, Rules, Config, *Rule, GridX, GridY, !Base.Facilities.IsEmpty(), FailureCode))
				{
					FBaseFacilityState& Facility = Base.Facilities.AddDefaulted_GetRef();
					Facility.InstanceId = InstanceId;
					Facility.FacilityId = FacilityId;
					Facility.GridX = GridX;
					Facility.GridY = GridY;
					return true;
				}
			}
		}
		return false;
	}

	bool TryAdd(const int64 Left, const int64 Right, int64& OutValue)
	{
		if ((Right > 0 && Left > MAX_int64 - Right) || (Right < 0 && Left < MIN_int64 - Right))
		{
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

	bool TryMultiplyNonNegative(const int64 Left, const int64 Right, int64& OutValue)
	{
		if (Left < 0 || Right < 0 || (Right != 0 && Left > MAX_int64 / Right))
		{
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool WouldExhaustDeterministicRandomStream(
		const FDeterministicRandomStream& Stream,
		const int64 RequiredDraws)
	{
		if (!Stream.IsValid() || RequiredDraws < 0)
		{
			return true;
		}
		return RequiredDraws > (MAX_int64 - 1) - Stream.DrawCount;
	}

	bool ComputeBaseStorageCapacity(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		int64& OutCapacity,
		FStrategicCommandResult& Result)
	{
		OutCapacity = 0;
		if (!Base.Facilities.IsEmpty())
		{
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
				if (Rule == nullptr || Rule->StorageCapacity < 0 || Rule->StorageCapacity > 1000000
					|| Rule->MaxIntegrity <= 0 || Facility.Damage < 0 || Facility.Damage > Rule->MaxIntegrity)
				{
					AddError(Result, TEXT("invalid_storage_facility"), FString::Printf(
						TEXT("Base '%s' has an invalid storage-capacity facility '%s'."),
						*Base.Name, *Facility.FacilityId.ToString()));
					return false;
				}
				const int32 Contribution = Rule->ScaleEffectByIntegrity(Rule->StorageCapacity, Facility.Damage);
				if (!TryAdd(OutCapacity, Contribution, OutCapacity))
				{
					AddError(Result, TEXT("invalid_storage_capacity"), FString::Printf(
						TEXT("Base '%s' has overflowing integrity-scaled storage capacity."), *Base.Name));
					return false;
				}
			}
		}
		else
		{
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
				if (Facility == nullptr || Facility->StorageCapacity < 0 || Facility->StorageCapacity > 1000000
					|| !TryAdd(OutCapacity, Facility != nullptr ? Facility->StorageCapacity : 0,
						OutCapacity))
				{
					AddError(Result, TEXT("invalid_storage_capacity"), FString::Printf(
						TEXT("Base '%s' has invalid or overflowing storage capacity."), *Base.Name));
					return false;
				}
			}
		}
		return true;
	}

	bool ComputeBaseStorage(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicBaseState& Base,
		FBaseStorageEvaluation& OutEvaluation,
		FStrategicCommandResult& Result)
	{
		OutEvaluation = FBaseStorageEvaluation();
		OutEvaluation.BaseId = Base.BaseId;
		for (const TPair<FName, FFacilityRule>& Pair : Rules.Facilities)
		{
			OutEvaluation.bEnforced |= Pair.Value.StorageCapacity > 0;
		}

		if (!ComputeBaseStorageCapacity(Base, Rules, OutEvaluation.Capacity, Result))
		{
			return false;
		}
		const int32 StorageCapacityPercent =
			FStrategicCommandService::EvaluateBaseStorageCapacityPercent(Base, Rules);
		if (!TryScaleNonNegativeByPercent(
			OutEvaluation.Capacity, StorageCapacityPercent, OutEvaluation.Capacity))
		{
			AddError(Result, TEXT("storage_capacity_overflow"), FString::Printf(
				TEXT("Base '%s' storage capacity exceeds the supported numeric range after specialization scaling."),
				*Base.Name));
			return false;
		}

		TSet<FName> SeenItems;
		for (const FInventoryStack& Stack : Base.Inventory)
		{
			const FItemRule* Item = Rules.Items.Find(Stack.ItemId);
			int64 StackStorage = 0;
			if (Item == nullptr || Item->Mass < 0 || Stack.ItemId.IsNone() || Stack.Quantity <= 0
				|| SeenItems.Contains(Stack.ItemId)
				|| !TryMultiplyNonNegative(Item->Mass, Stack.Quantity, StackStorage)
				|| !TryAdd(OutEvaluation.Used, StackStorage, OutEvaluation.Used))
			{
				AddError(Result, TEXT("invalid_storage_inventory"), FString::Printf(
					TEXT("Base '%s' contains invalid or overflowing inventory storage data."), *Base.Name));
				return false;
			}
			SeenItems.Add(Stack.ItemId);
		}

		for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
		{
			if (Project.BaseId != Base.BaseId)
			{
				continue;
			}
			const FItemRule* Item = Rules.Items.Find(Project.ItemId);
			int64 ProjectStorage = 0;
			if (Item == nullptr || Item->Mass < 0 || Project.UnitsRemaining <= 0
				|| !TryMultiplyNonNegative(Item->Mass, Project.UnitsRemaining, ProjectStorage)
				|| !TryAdd(OutEvaluation.ManufacturingReserved, ProjectStorage,
					OutEvaluation.ManufacturingReserved))
			{
				AddError(Result, TEXT("invalid_storage_reservation"), FString::Printf(
					TEXT("Base '%s' has an invalid or overflowing production storage reservation."), *Base.Name));
				return false;
			}
		}
		for (const FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
		{
			const bool bDestinationReservation =
				Convoy.DestinationBaseId == Base.BaseId;
			const bool bWaypointReservation =
				Convoy.BalancedHandoffQuantity > 0
				&& Convoy.RelayWaypointBaseId == Base.BaseId;
			if (!bDestinationReservation && !bWaypointReservation)
			{
				continue;
			}
			const int32 ReservedQuantity = bWaypointReservation
				? Convoy.BalancedHandoffQuantity
				: Convoy.Quantity - Convoy.BalancedHandoffQuantity;
			const FItemRule* Item = Rules.Items.Find(Convoy.ItemId);
			int64 ConvoyStorage = 0;
			if (!Convoy.ConvoyId.IsValid() || Convoy.SourceBaseId == Convoy.DestinationBaseId
				|| FindBase(State, Convoy.SourceBaseId) == nullptr
				|| Item == nullptr || Item->Mass < 0 || Convoy.Quantity <= 0
				|| ReservedQuantity <= 0
				|| Convoy.DispatchSequence <= 0
				|| Convoy.DispatchSequence > State.CommandSequence
				|| !IsValidMutualAidRoutePolicy(Convoy.RoutePolicy)
				|| !IsValidMutualAidWaypointState(State, Convoy)
				|| Convoy.TotalTransitSeconds <= 0 || Convoy.RemainingTransitSeconds <= 0
				|| Convoy.RemainingTransitSeconds > Convoy.TotalTransitSeconds
				|| Convoy.RoutePressure < 0 || Convoy.RoutePressure > 100
				|| Convoy.SignalEscortCost < 0
				|| (!Convoy.bSignalEscort && Convoy.SignalEscortCost != 0)
				|| Convoy.ForecastInterdictionDelaySeconds <= 0
				|| Convoy.ForecastInterdictionDelaySeconds > Convoy.TotalTransitSeconds / 2
				|| Convoy.InterdictionDelaySeconds < 0
				|| Convoy.InterdictionDelaySeconds > Convoy.TotalTransitSeconds
				|| (Convoy.InterdictionDelaySeconds != 0
					&& Convoy.InterdictionDelaySeconds != Convoy.ForecastInterdictionDelaySeconds)
				|| (Convoy.InterdictionDelaySeconds > 0
					&& (!Convoy.bInterdictionResolved || Convoy.bSignalEscort))
				|| (!Convoy.bInterdictionResolved && Convoy.InterdictionDelaySeconds != 0)
				|| (!Convoy.bInterdictionResolved
					&& Convoy.RemainingTransitSeconds <= Convoy.TotalTransitSeconds / 2)
				|| !TryMultiplyNonNegative(Item->Mass, ReservedQuantity, ConvoyStorage)
				|| !TryAdd(OutEvaluation.MutualAidReserved, ConvoyStorage,
					OutEvaluation.MutualAidReserved))
			{
				AddError(Result, TEXT("invalid_mutual_aid_convoy"), FString::Printf(
					TEXT("Base '%s' has an invalid or overflowing inbound Mutual Aid Convoy reservation."),
					*Base.Name));
				return false;
			}
		}
		if (!TryAdd(OutEvaluation.ManufacturingReserved, OutEvaluation.MutualAidReserved,
			OutEvaluation.Reserved))
		{
			AddError(Result, TEXT("storage_usage_overflow"), FString::Printf(
				TEXT("Base '%s' storage reservations exceed the supported numeric range."), *Base.Name));
			return false;
		}
		if (!TryAdd(OutEvaluation.Used, OutEvaluation.Reserved, OutEvaluation.Committed))
		{
			AddError(Result, TEXT("storage_usage_overflow"), FString::Printf(
				TEXT("Base '%s' storage commitment exceeds the supported numeric range."), *Base.Name));
			return false;
		}
		if (OutEvaluation.bEnforced)
		{
			OutEvaluation.Available = FMath::Max<int64>(0, OutEvaluation.Capacity - OutEvaluation.Committed);
			OutEvaluation.Overflow = FMath::Max<int64>(0, OutEvaluation.Committed - OutEvaluation.Capacity);
		}
		OutEvaluation.bValid = true;
		return true;
	}

	bool ValidateMutualAidConvoyState(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		if (State.MutualAidConvoys.Num() > 10000)
		{
			AddError(Result, TEXT("invalid_mutual_aid_convoy"),
				TEXT("The Mutual Aid Convoy ledger exceeds its supported record count."));
			return false;
		}
		TSet<FGuid> SeenConvoyIds;
		TSet<int64> SeenDispatchSequences;
		TMap<FString, int64> InboundItemTotals;
		const auto ValidateInboundInventory =
			[&InboundItemTotals, &Result](
				const FStrategicBaseState& ReceivingBase,
				const FName ItemId,
				const int32 Quantity)
		{
			const FString ItemKey = FString::Printf(
				TEXT("%s|%s"),
				*ReceivingBase.BaseId.ToString(EGuidFormats::Digits), *ItemId.ToString());
			int64& InboundQuantity = InboundItemTotals.FindOrAdd(ItemKey);
			if (Quantity <= 0 || !TryAdd(InboundQuantity, Quantity, InboundQuantity))
			{
				AddError(Result, TEXT("mutual_aid_inventory_overflow"),
					TEXT("Inbound Mutual Aid Convoy cargo exceeds the supported inventory range."));
				return false;
			}
			const FInventoryStack* Existing = ReceivingBase.Inventory.FindByPredicate(
				[ItemId](const FInventoryStack& Stack) { return Stack.ItemId == ItemId; });
			if (InboundQuantity > MAX_int32
				|| (Existing != nullptr && InboundQuantity > MAX_int32 - Existing->Quantity))
			{
				AddError(Result, TEXT("mutual_aid_inventory_overflow"),
					TEXT("A Mutual Aid Convoy delivery would exceed the receiving base inventory range."));
				return false;
			}
			return true;
		};
		for (const FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
		{
			const FStrategicBaseState* Source = FindBase(State, Convoy.SourceBaseId);
			const FStrategicBaseState* Destination = FindBase(State, Convoy.DestinationBaseId);
			const FItemRule* Item = Rules.Items.Find(Convoy.ItemId);
			if (!Convoy.ConvoyId.IsValid() || SeenConvoyIds.Contains(Convoy.ConvoyId)
				|| Source == nullptr || Destination == nullptr || Source == Destination
				|| Item == nullptr || Item->Mass < 0 || Convoy.ItemId.IsNone()
				|| Convoy.Quantity <= 0 || Convoy.DispatchSequence <= 0
				|| Convoy.DispatchSequence > State.CommandSequence
				|| SeenDispatchSequences.Contains(Convoy.DispatchSequence)
				|| !IsValidMutualAidRoutePolicy(Convoy.RoutePolicy)
				|| !IsValidMutualAidWaypointState(State, Convoy)
				|| Convoy.TotalTransitSeconds <= 0 || Convoy.RemainingTransitSeconds <= 0
				|| Convoy.RemainingTransitSeconds > Convoy.TotalTransitSeconds
				|| Convoy.RoutePressure < 0 || Convoy.RoutePressure > 100
				|| Convoy.SignalEscortCost < 0
				|| (!Convoy.bSignalEscort && Convoy.SignalEscortCost != 0)
				|| Convoy.ForecastInterdictionDelaySeconds <= 0
				|| Convoy.ForecastInterdictionDelaySeconds > Convoy.TotalTransitSeconds / 2
				|| Convoy.InterdictionDelaySeconds < 0
				|| Convoy.InterdictionDelaySeconds > Convoy.TotalTransitSeconds
				|| (Convoy.InterdictionDelaySeconds != 0
					&& Convoy.InterdictionDelaySeconds != Convoy.ForecastInterdictionDelaySeconds)
				|| (Convoy.InterdictionDelaySeconds > 0
					&& (!Convoy.bInterdictionResolved || Convoy.bSignalEscort))
				|| (!Convoy.bInterdictionResolved && Convoy.InterdictionDelaySeconds != 0)
				|| (!Convoy.bInterdictionResolved
					&& Convoy.RemainingTransitSeconds <= Convoy.TotalTransitSeconds / 2))
			{
				AddError(Result, TEXT("invalid_mutual_aid_convoy"),
					TEXT("A Mutual Aid Convoy has invalid identity, bases, cargo, or transit time."));
				return false;
			}
			SeenConvoyIds.Add(Convoy.ConvoyId);
			SeenDispatchSequences.Add(Convoy.DispatchSequence);
			const int32 FinalQuantity = Convoy.Quantity - Convoy.BalancedHandoffQuantity;
			if (!ValidateInboundInventory(*Destination, Convoy.ItemId, FinalQuantity))
			{
				return false;
			}
			if (Convoy.BalancedHandoffQuantity > 0)
			{
				const FStrategicBaseState* Waypoint =
					FindBase(State, Convoy.RelayWaypointBaseId);
				if (Waypoint == nullptr
					|| !ValidateInboundInventory(
						*Waypoint, Convoy.ItemId, Convoy.BalancedHandoffQuantity))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool ApplyMutualAidReliefPriority(
		FCampaignState& State,
		const FGuid& ConvoyId,
		const TArray<FGuid>& BypassedConvoyIds)
	{
		FMutualAidConvoyState* Target = State.MutualAidConvoys.FindByPredicate(
			[&ConvoyId](const FMutualAidConvoyState& Convoy)
			{
				return Convoy.ConvoyId == ConvoyId;
			});
		if (Target == nullptr || BypassedConvoyIds.IsEmpty())
		{
			return false;
		}

		TArray<FMutualAidConvoyState*> Bypassed;
		TArray<int64> OriginalSequences;
		Bypassed.Reserve(BypassedConvoyIds.Num());
		OriginalSequences.Reserve(BypassedConvoyIds.Num());
		int64 PreviousSequence = 0;
		for (const FGuid& BypassedId : BypassedConvoyIds)
		{
			FMutualAidConvoyState* Entry = State.MutualAidConvoys.FindByPredicate(
				[&BypassedId](const FMutualAidConvoyState& Convoy)
				{
					return Convoy.ConvoyId == BypassedId;
				});
			if (Entry == nullptr || Entry == Target
				|| Entry->SourceBaseId != Target->SourceBaseId
				|| Entry->DispatchSequence <= PreviousSequence
				|| Entry->DispatchSequence >= Target->DispatchSequence)
			{
				return false;
			}
			Bypassed.Add(Entry);
			OriginalSequences.Add(Entry->DispatchSequence);
			PreviousSequence = Entry->DispatchSequence;
		}

		const int64 TargetSequence = Target->DispatchSequence;
		Target->DispatchSequence = OriginalSequences[0];
		for (int32 Index = 0; Index < Bypassed.Num(); ++Index)
		{
			Bypassed[Index]->DispatchSequence = Index + 1 < OriginalSequences.Num()
				? OriginalSequences[Index + 1]
				: TargetSequence;
		}
		return true;
	}

	bool ValidatePlayerStorageTransition(
		const FCampaignState& Before,
		const FCampaignState& After,
		const FResolvedRuleSet& Rules,
		const FGuid BaseId,
		const TCHAR* Action,
		FStrategicCommandResult& Result)
	{
		const FStrategicBaseState* BeforeBase = FindBase(Before, BaseId);
		const FStrategicBaseState* AfterBase = FindBase(After, BaseId);
		if (BeforeBase == nullptr || AfterBase == nullptr)
		{
			AddError(Result, TEXT("unknown_base"), TEXT("Storage transition references a missing base."));
			return false;
		}
		FBaseStorageEvaluation BeforeStorage;
		FBaseStorageEvaluation AfterStorage;
		if (!ComputeBaseStorage(Before, Rules, *BeforeBase, BeforeStorage, Result)
			|| !ComputeBaseStorage(After, Rules, *AfterBase, AfterStorage, Result))
		{
			return false;
		}
		if (AfterStorage.bEnforced && AfterStorage.Overflow > BeforeStorage.Overflow)
		{
			AddError(Result, TEXT("storage_capacity_exceeded"), FString::Printf(
				TEXT("%s would commit %lld storage units at a base with %lld capacity; free %lld storage units first."),
				Action, AfterStorage.Committed, AfterStorage.Capacity,
				AfterStorage.Overflow - BeforeStorage.Overflow));
			return false;
		}
		return true;
	}

	bool AdjustManufacturingInputs(
		FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		const FItemRule& Product,
		const int64 Units,
		const bool bReserve,
		TArray<FInventoryStack>& OutChanges,
		FStrategicCommandResult& Result)
	{
		OutChanges.Reset();
		if (Units < 0 || Product.ManufactureInputs.Num() > 16)
		{
			AddError(Result, TEXT("invalid_manufacturing_recipe"), TEXT("Manufacturing recipe inputs are invalid."));
			return false;
		}

		TSet<FName> SeenInputs;
		for (const FManufacturingInputRule& Input : Product.ManufactureInputs)
		{
			if (!FContentPackageResolver::IsValidPackageId(Input.ItemId)
				|| Input.ItemId == Product.Identity.RuleId
				|| Input.Quantity <= 0
				|| SeenInputs.Contains(Input.ItemId)
				|| !Rules.Items.Contains(Input.ItemId))
			{
				AddError(Result, TEXT("invalid_manufacturing_recipe"), FString::Printf(
					TEXT("Manufacturing recipe for '%s' has an invalid input definition."),
					*Product.Identity.RuleId.ToString()));
				return false;
			}
			SeenInputs.Add(Input.ItemId);

			int64 TotalQuantity = 0;
			if (!TryMultiplyNonNegative(Input.Quantity, Units, TotalQuantity) || TotalQuantity > MAX_int32)
			{
				AddError(Result, TEXT("manufacturing_materials_overflow"), FString::Printf(
					TEXT("Manufacturing input '%s' exceeds the supported inventory quantity."),
					*Input.ItemId.ToString()));
				return false;
			}
			if (TotalQuantity == 0)
			{
				continue;
			}

			const FInventoryStack* Existing = Base.Inventory.FindByPredicate(
				[&Input](const FInventoryStack& Stack) { return Stack.ItemId == Input.ItemId; });
			const int64 Available = Existing != nullptr ? Existing->Quantity : 0;
			if (bReserve && Available < TotalQuantity)
			{
				AddError(Result, TEXT("manufacturing_materials_missing"), FString::Printf(
					TEXT("Manufacturing requires %lld of '%s', but only %lld are available at this base."),
					TotalQuantity, *Input.ItemId.ToString(), Available));
				return false;
			}
			if (!bReserve && Available > MAX_int32 - TotalQuantity)
			{
				AddError(Result, TEXT("inventory_capacity_exceeded"), FString::Printf(
					TEXT("Returning manufacturing input '%s' would exceed the supported inventory quantity."),
					*Input.ItemId.ToString()));
				return false;
			}

			FInventoryStack& Change = OutChanges.AddDefaulted_GetRef();
			Change.ItemId = Input.ItemId;
			Change.Quantity = static_cast<int32>(TotalQuantity);
		}

		OutChanges.Sort([](const FInventoryStack& Left, const FInventoryStack& Right)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		});
		for (const FInventoryStack& Change : OutChanges)
		{
			const int32 Delta = bReserve ? -Change.Quantity : Change.Quantity;
			if (!TryAdjustInventory(Base, Change.ItemId, Delta))
			{
				AddError(Result, TEXT("inventory_transaction_failed"),
					TEXT("Manufacturing materials could not be adjusted atomically."));
				return false;
			}
		}
		return true;
	}

	void SortStateCollections(FCampaignState& State)
	{
		State.Bases.Sort(
			[](const FStrategicBaseState& Left, const FStrategicBaseState& Right)
			{
				return Left.BaseId.ToString(EGuidFormats::Digits) < Right.BaseId.ToString(EGuidFormats::Digits);
			});
		for (FStrategicBaseState& Base : State.Bases)
		{
			Base.BuiltFacilities.Sort(FNameLexicalLess());
			Base.Facilities.Sort(
				[](const FBaseFacilityState& Left, const FBaseFacilityState& Right)
				{
					return Left.InstanceId.ToString(EGuidFormats::Digits) < Right.InstanceId.ToString(EGuidFormats::Digits);
				});
			Base.Inventory.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
		}
		State.ResearchProjects.Sort(
			[](const FResearchProjectState& Left, const FResearchProjectState& Right)
			{
				return Left.ResearchId.LexicalLess(Right.ResearchId);
			});
		State.CompletedResearch.Sort(FNameLexicalLess());
		State.ManufacturingProjects.Sort(
			[](const FManufacturingProjectState& Left, const FManufacturingProjectState& Right)
			{
				return Left.ProjectId.ToString(EGuidFormats::Digits) < Right.ProjectId.ToString(EGuidFormats::Digits);
			});
		State.MutualAidConvoys.Sort(
			[](const FMutualAidConvoyState& Left, const FMutualAidConvoyState& Right)
			{
				return Left.ConvoyId.ToString(EGuidFormats::Digits)
					< Right.ConvoyId.ToString(EGuidFormats::Digits);
			});
		State.FacilityConstructionProjects.Sort(
			[](const FFacilityConstructionProjectState& Left, const FFacilityConstructionProjectState& Right)
			{
				return Left.ProjectId.ToString(EGuidFormats::Digits) < Right.ProjectId.ToString(EGuidFormats::Digits);
			});
		State.Personnel.Sort(
			[](const FPersonnelState& Left, const FPersonnelState& Right)
			{
				return Left.PersonnelId.ToString(EGuidFormats::Digits) < Right.PersonnelId.ToString(EGuidFormats::Digits);
			});
		for (FPersonnelState& Person : State.Personnel)
		{
			Person.EquippedItems.Sort(FNameLexicalLess());
			Person.DoctrineSelections.Sort(FNameLexicalLess());
			Person.Commendations.Sort(FNameLexicalLess());
		}
		State.PersonnelSquadBonds.Sort(
			[](const FPersonnelSquadBondState& Left, const FPersonnelSquadBondState& Right)
			{
				if (Left.FirstPersonnelId != Right.FirstPersonnelId)
				{
					return PersonnelGuidLess(Left.FirstPersonnelId, Right.FirstPersonnelId);
				}
				return PersonnelGuidLess(Left.SecondPersonnelId, Right.SecondPersonnelId);
			});
		State.RecruitmentOrders.Sort(
			[](const FRecruitmentOrderState& Left, const FRecruitmentOrderState& Right)
			{
				return Left.OrderId.ToString(EGuidFormats::Digits) < Right.OrderId.ToString(EGuidFormats::Digits);
			});
		State.Memorial.Sort(
			[](const FMemorialRecord& Left, const FMemorialRecord& Right)
			{
				if (Left.DeathUtc != Right.DeathUtc)
				{
					return Left.DeathUtc < Right.DeathUtc;
				}
				return Left.PersonnelId.ToString(EGuidFormats::Digits) < Right.PersonnelId.ToString(EGuidFormats::Digits);
			});
		for (FMemorialRecord& Record : State.Memorial)
		{
			Record.DoctrineSelections.Sort(FNameLexicalLess());
			Record.Commendations.Sort(FNameLexicalLess());
		}
		State.Craft.Sort(
			[](const FCraftState& Left, const FCraftState& Right)
			{
				return Left.CraftId.ToString(EGuidFormats::Digits) < Right.CraftId.ToString(EGuidFormats::Digits);
			});
		for (FCraftState& Craft : State.Craft)
		{
			Craft.EquipmentItems.Sort(FNameLexicalLess());
			Craft.AssignedAgentIds.Sort(
				[](const FGuid& Left, const FGuid& Right)
				{
					return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
				});
			Craft.Cargo.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
			Craft.PendingSalvage.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
			Craft.WeaponStates.Sort(
				[](const FCraftWeaponState& Left, const FCraftWeaponState& Right)
				{
					return Left.WeaponItemId.LexicalLess(Right.WeaponItemId);
				});
		}
		State.CraftAcquisitionOrders.Sort(
			[](const FCraftAcquisitionOrderState& Left, const FCraftAcquisitionOrderState& Right)
			{
				return Left.OrderId.ToString(EGuidFormats::Digits) < Right.OrderId.ToString(EGuidFormats::Digits);
			});
		State.StrategicContacts.Sort(
			[](const FStrategicContactState& Left, const FStrategicContactState& Right)
			{
				return Left.ContactId.ToString(EGuidFormats::Digits) < Right.ContactId.ToString(EGuidFormats::Digits);
			});
		State.StrategicSites.Sort(
			[](const FStrategicSiteState& Left, const FStrategicSiteState& Right)
			{
				return Left.SiteId.ToString(EGuidFormats::Digits) < Right.SiteId.ToString(EGuidFormats::Digits);
			});
		State.TacticalOperations.Sort(
			[](const FTacticalOperationState& Left, const FTacticalOperationState& Right)
			{
				return Left.OperationId.ToString(EGuidFormats::Digits) < Right.OperationId.ToString(EGuidFormats::Digits);
			});
		for (FTacticalOperationState& Operation : State.TacticalOperations)
		{
			Operation.AgentIds.Sort(
				[](const FGuid& Left, const FGuid& Right)
				{
					return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
				});
			Operation.Cargo.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
		}
		State.TacticalBattles.Sort(
			[](const FTacticalBattleState& Left, const FTacticalBattleState& Right)
			{
				return Left.BattleId.ToString(EGuidFormats::Digits) < Right.BattleId.ToString(EGuidFormats::Digits);
			});
		for (FTacticalBattleState& Battle : State.TacticalBattles)
		{
			Battle.Cells.Sort(
				[](const FTacticalCellState& Left, const FTacticalCellState& Right)
				{
					if (Left.Z != Right.Z)
					{
						return Left.Z < Right.Z;
					}
					return Left.Y != Right.Y ? Left.Y < Right.Y : Left.X < Right.X;
				});
			Battle.PlayerDiscoveredCellIndices.Sort();
			Battle.PlayerLastKnownAdversaries.Sort(
				[](const FTacticalUnitMemoryState& Left, const FTacticalUnitMemoryState& Right)
				{
					return Left.UnitId.ToString(EGuidFormats::Digits) < Right.UnitId.ToString(EGuidFormats::Digits);
				});
			Battle.Units.Sort(
				[](const FTacticalUnitState& Left, const FTacticalUnitState& Right)
				{
					return Left.UnitId.ToString(EGuidFormats::Digits) < Right.UnitId.ToString(EGuidFormats::Digits);
				});
			for (FTacticalUnitState& Unit : Battle.Units)
			{
				Unit.WeaponStates.Sort(
					[](const FTacticalWeaponState& Left, const FTacticalWeaponState& Right)
					{
						return Left.WeaponItemId.LexicalLess(Right.WeaponItemId);
					});
				Unit.CarriedItems.Sort(
					[](const FInventoryStack& Left, const FInventoryStack& Right)
					{
						return Left.ItemId.LexicalLess(Right.ItemId);
					});
				Unit.EjectedMagazines.Sort(
					[](const FTacticalMagazineState& Left, const FTacticalMagazineState& Right)
					{
						if (Left.WeaponItemId != Right.WeaponItemId)
						{
							return Left.WeaponItemId.LexicalLess(Right.WeaponItemId);
						}
						if (Left.AmmunitionItemId != Right.AmmunitionItemId)
						{
							return Left.AmmunitionItemId.LexicalLess(Right.AmmunitionItemId);
						}
						return Left.LoadedAmmunition > Right.LoadedAmmunition;
					});
			}
			Battle.Objectives.Sort(
				[](const FTacticalObjectiveState& Left, const FTacticalObjectiveState& Right)
				{
					return Left.ObjectiveId.LexicalLess(Right.ObjectiveId);
				});
			Battle.Cargo.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
		}
		State.RegionalPressure.Sort(
			[](const FRegionalPressureState& Left, const FRegionalPressureState& Right)
			{
				return Left.RegionId.LexicalLess(Right.RegionId);
			});
		State.RegionalMandates.Sort(
			[](const FRegionalMandateState& Left, const FRegionalMandateState& Right)
			{
				return Left.RegionId.LexicalLess(Right.RegionId);
			});
		State.AdversaryMissions.Sort(
			[](const FAdversaryMissionState& Left, const FAdversaryMissionState& Right)
			{
				return Left.MissionId.ToString(EGuidFormats::Digits) < Right.MissionId.ToString(EGuidFormats::Digits);
			});
		State.BaseAssaults.Sort(
			[](const FBaseAssaultState& Left, const FBaseAssaultState& Right)
			{
				return Left.AssaultId.ToString(EGuidFormats::Digits) < Right.AssaultId.ToString(EGuidFormats::Digits);
			});
	}

	FStrategicEvent& AddEvent(
		FStrategicCommandResult& Result,
		const EStrategicEventType Type,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		FStrategicEvent& Event = Result.Events.AddDefaulted_GetRef();
		Event.Type = Type;
		Event.CommandSequence = CommandSequence;
		Event.TimestampUtc = TimestampUtc;
		return Event;
	}

	FStrategicEvent& AddCraftServiceRotationEvent(
		FStrategicCommandResult& Result,
		const FCraftServiceQueueView& Queue,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		FStrategicEvent& Event = AddEvent(
			Result, EStrategicEventType::CraftServiceRotationScheduled,
			CommandSequence, TimestampUtc);
		Event.BaseId = Queue.BaseId;
		Event.CraftId = Queue.CraftId;
		Event.PolicyId = Queue.PolicyId;
		Event.ServiceLaneCount = Queue.ServiceLaneCount;
		Event.ServiceQueuePosition = Queue.QueuePosition;
		Event.ServiceLaneNumber = Queue.ServiceLaneNumber;
		Event.bServiceLaneActive = Queue.bInServiceLane;
		Event.ServiceQueueWaitSeconds = Queue.EstimatedWaitSeconds;
		Event.ServiceReadySeconds = Queue.EstimatedReadySeconds;
		return Event;
	}

	FStrategicEvent& AddMutualAidRelayScheduledEvent(
		FStrategicCommandResult& Result,
		const FMutualAidRelayQueueView& Queue,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		FStrategicEvent& Event = AddEvent(
			Result, EStrategicEventType::MutualAidConvoyRelayScheduled,
			CommandSequence, TimestampUtc);
		Event.BaseId = Queue.SourceBaseId;
		Event.ConvoyId = Queue.ConvoyId;
		Event.PolicyId = Queue.PolicyId;
		Event.ConvoyRelayChannelCount = Queue.RelayChannelCount;
		Event.SignalWatchFacilityChannelCount = Queue.FacilityRelayChannelCount;
		Event.SignalWatchAssignedScientists = Queue.SignalWatchScientistCount;
		Event.SignalWatchBonusChannelCount = Queue.SignalWatchBonusChannelCount;
		Event.SignalWatchTotalChannelCount = Queue.RelayChannelCount;
		Event.ConvoyRelayQueuePosition = Queue.QueuePosition;
		Event.ConvoyRelayChannelNumber = Queue.RelayChannelNumber;
		Event.bConvoyRelayActive = Queue.bInTransit;
		Event.ConvoyRelayWaitSeconds = Queue.EstimatedWaitSeconds;
		Event.ConvoyEstimatedArrivalSeconds = Queue.EstimatedArrivalSeconds;
		return Event;
	}

	FStrategicEvent& AddHorizonCompactWithdrawalEvent(
		FStrategicCommandResult& Result,
		const FRegionalMandateState& Mandate,
		const FStrategicSimulationConfig& Config,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		FStrategicEvent& Event = AddEvent(
			Result, EStrategicEventType::HorizonCompactMemberWithdrawn,
			CommandSequence, TimestampUtc);
		Event.RuleId = TEXT("coalition.horizon-compact");
		Event.RegionId = Mandate.RegionId;
		Event.Amount = Mandate.Support;
		Event.Quantity = Config.HorizonCompactWithdrawalSupportThreshold;
		Event.bSuccessful = true;
		return Event;
	}

	bool IsSupportedDifficultyPercent(const int32 Percent)
	{
		return Percent >= 25 && Percent <= 400;
	}

	bool HasValidAdversaryDifficultyTuning(const FStrategicSimulationConfig& Config)
	{
		return IsSupportedDifficultyPercent(Config.CadetAdversaryIntervalPercent)
			&& IsSupportedDifficultyPercent(Config.StandardAdversaryIntervalPercent)
			&& IsSupportedDifficultyPercent(Config.VeteranAdversaryIntervalPercent)
			&& IsSupportedDifficultyPercent(Config.ApexAdversaryIntervalPercent)
			&& IsSupportedDifficultyPercent(Config.CadetAdversaryConsequencePercent)
			&& IsSupportedDifficultyPercent(Config.StandardAdversaryConsequencePercent)
			&& IsSupportedDifficultyPercent(Config.VeteranAdversaryConsequencePercent)
			&& IsSupportedDifficultyPercent(Config.ApexAdversaryConsequencePercent);
	}

	bool HasValidBaseDefenseGridOverchargeConfig(const FStrategicSimulationConfig& Config)
	{
		return Config.BaseDefenseGridOverchargeCostPerThreat > 0
			&& Config.BaseDefenseGridOverchargeCostPerThreat <= MAX_int64 / 10
			&& Config.BaseDefenseGridOverchargeAccuracyBonus >= 0
			&& Config.BaseDefenseGridOverchargeAccuracyBonus <= 100
			&& Config.BaseDefenseGridOverchargeDamagePercent >= 100
			&& Config.BaseDefenseGridOverchargeDamagePercent <= 400;
	}

	bool HasValidResilienceCharterConfig(const FStrategicSimulationConfig& Config)
	{
		return Config.ResilienceCharterMinimumSupport > 0
			&& Config.ResilienceCharterMinimumSupport <= 100
			&& Config.ResilienceCharterCost >= 0
			&& Config.ResilienceCharterSupportCost > 0
			&& Config.ResilienceCharterSupportCost <= Config.ResilienceCharterMinimumSupport
			&& Config.ResilienceCharterFundingPercent >= 25
			&& Config.ResilienceCharterFundingPercent <= 100
			&& Config.ResilienceCharterMissionWeightPercent >= 25
			&& Config.ResilienceCharterMissionWeightPercent <= 100
			&& Config.ResilienceCharterEscapePressurePercent >= 25
			&& Config.ResilienceCharterEscapePressurePercent <= 100;
	}

	bool HasValidHorizonCompactConfig(const FStrategicSimulationConfig& Config)
	{
		return Config.HorizonCompactRequiredCharters >= 2
			&& Config.HorizonCompactRequiredCharters <= 100
			&& Config.HorizonCompactMinimumMemberSupport > 0
			&& Config.HorizonCompactMinimumMemberSupport <= 100
			&& Config.HorizonCompactCost >= 0
			&& Config.HorizonCompactMemberSupportCost > 0
			&& Config.HorizonCompactMemberSupportCost <= Config.HorizonCompactMinimumMemberSupport
			&& Config.HorizonCompactFundingPercent >= Config.ResilienceCharterFundingPercent
			&& Config.HorizonCompactFundingPercent <= 100
			&& Config.HorizonCompactSharedEscapePressurePercent > 0
			&& Config.HorizonCompactSharedEscapePressurePercent <= 50
			&& Config.HorizonCompactWithdrawalSupportThreshold > 0
			&& Config.HorizonCompactWithdrawalSupportThreshold
				< Config.HorizonCompactRestorationMinimumSupport
			&& Config.HorizonCompactRestorationMinimumSupport
				<= Config.HorizonCompactMinimumMemberSupport
			&& Config.HorizonCompactRestorationCost >= 0;
	}

	bool HasValidReciprocalAidConfig(const FStrategicSimulationConfig& Config)
	{
		return Config.ReciprocalAidCost >= 0
			&& Config.ReciprocalAidMinimumTargetPressure > 0
			&& Config.ReciprocalAidMinimumTargetPressure < 100
			&& Config.ReciprocalAidPressureTransfer > 0
			&& Config.ReciprocalAidPressureTransfer <= Config.ReciprocalAidMinimumTargetPressure
			&& Config.ReciprocalAidSupportTransfer > 0
			&& Config.ReciprocalAidSupportTransfer <= 100;
	}

	bool HasValidHorizonCompactEmergencyVoteConfig(const FStrategicSimulationConfig& Config)
	{
		return Config.HorizonCompactEmergencyVoteCost >= 0
			&& Config.HorizonCompactEmergencyTargetSupportGain > 0
			&& Config.HorizonCompactEmergencyTargetSupportGain <= 100
			&& Config.HorizonCompactEmergencyTargetPressureReduction > 0
			&& Config.HorizonCompactEmergencyTargetPressureReduction <= 100
			&& Config.HorizonCompactEmergencyVoterSupportCost > 0
			&& Config.HorizonCompactEmergencyVoterSupportCost
				<= 100 - Config.HorizonCompactWithdrawalSupportThreshold
			&& Config.HorizonCompactEmergencyMaximumVoterPressure >= 0
			&& Config.HorizonCompactEmergencyMaximumVoterPressure < 100;
	}

	bool TryScaleNonNegativeByPercent(
		const int64 BaseValue,
		const int32 Percent,
		int64& OutValue)
	{
		OutValue = 0;
		if (BaseValue < 0 || !IsSupportedDifficultyPercent(Percent))
		{
			return false;
		}
		const int64 Whole = BaseValue / 100;
		const int64 Remainder = BaseValue % 100;
		if (Whole > MAX_int64 / Percent)
		{
			return false;
		}
		const int64 ScaledWhole = Whole * Percent;
		const int64 ScaledRemainder = (Remainder * Percent + 99) / 100;
		if (ScaledWhole > MAX_int64 - ScaledRemainder)
		{
			return false;
		}
		OutValue = ScaledWhole + ScaledRemainder;
		return true;
	}

	bool TryApplyWorksCadreFrontload(
		const int64 BaselineSeconds,
		const int32 AssignedEngineers,
		const int32 FrontloadPercentPerEngineer,
		int64& OutCommittedSeconds,
		int32& OutFrontloadPercent)
	{
		OutCommittedSeconds = 0;
		OutFrontloadPercent = 0;
		if (BaselineSeconds <= 0 || AssignedEngineers < 0
			|| AssignedEngineers > WorksCadreMaximumEngineerCount)
		{
			return false;
		}
		if (FrontloadPercentPerEngineer <= 0
			|| FrontloadPercentPerEngineer > WorksCadreSpecializedPrimaryPercentEach)
		{
			return false;
		}
		OutFrontloadPercent = AssignedEngineers * FrontloadPercentPerEngineer;
		return OutFrontloadPercent <= WorksCadreMaximumFrontloadPercent
			&& TryScaleNonNegativeByPercent(
				BaselineSeconds, 100 - OutFrontloadPercent, OutCommittedSeconds)
			&& OutCommittedSeconds > 0;
	}

	bool ValidateAdversaryConfig(const FStrategicSimulationConfig& Config, FStrategicCommandResult& Result)
	{
		if (Config.StartingAdversaryDelayHours <= 0
			|| Config.InterceptionAftershockMinutesPerThreat < 0
			|| Config.InterceptionAftershockMinutesPerThreat > 360
			|| Config.MaxActiveAdversaryMissions <= 0
			|| Config.FailurePressureThreshold <= 0 || Config.FailurePressureThreshold > 100
			|| Config.VictoryThwartedMissions <= 0
			|| Config.ResolvedMissionsPerEscalationLevel <= 0
			|| Config.ResolvedMissionsPerEscalationLevel > 1000
			|| Config.MaxAdversaryEscalation <= 0 || Config.MaxAdversaryEscalation > 10
			|| Config.VictoryMinimumEscalationLevel <= 0
			|| Config.VictoryMinimumEscalationLevel > Config.MaxAdversaryEscalation
			|| Config.CivicReliefCost < 0
			|| Config.CivicReliefSupportGain <= 0 || Config.CivicReliefSupportGain > 100
			|| Config.CivicReliefPressureReduction <= 0 || Config.CivicReliefPressureReduction > 100
			|| Config.SecurityAccordCost < 0
			|| Config.SecurityAccordSupportGain <= 0 || Config.SecurityAccordSupportGain > 100
			|| Config.SecurityAccordPressureReduction <= 0 || Config.SecurityAccordPressureReduction > 100
			|| Config.CrisisMobilizationMinimumPressure <= 0
			|| Config.CrisisMobilizationMinimumPressure > 100
			|| Config.CrisisMobilizationSupportCost <= 0 || Config.CrisisMobilizationSupportCost > 100
			|| Config.CrisisMobilizationPressureReduction <= 0
			|| Config.CrisisMobilizationPressureReduction > Config.CrisisMobilizationMinimumPressure
			|| !HasValidAdversaryDifficultyTuning(Config))
		{
			AddError(Result, TEXT("invalid_adversary_config"), TEXT("Adversary, difficulty, regional diplomacy, adaptation, and campaign-outcome configuration must remain within supported limits."));
			return false;
		}
		if (!HasValidResilienceCharterConfig(Config))
		{
			AddError(Result, TEXT("invalid_regional_charter_config"),
				TEXT("Regional charter costs, support gate, funding, mission weight, and escape-pressure percentages must remain within supported limits."));
			return false;
		}
		if (!HasValidHorizonCompactConfig(Config))
		{
			AddError(Result, TEXT("invalid_coalition_compact_config"),
				TEXT("Horizon Compact charter count, support, cost, funding, shared pressure, withdrawal, and restoration settings must remain within supported limits."));
			return false;
		}
		if (!HasValidReciprocalAidConfig(Config))
		{
			AddError(Result, TEXT("invalid_coalition_aid_config"),
				TEXT("Reciprocal Aid cost, crisis threshold, pressure transfer, and support transfer must remain within supported limits."));
			return false;
		}
		if (!HasValidHorizonCompactEmergencyVoteConfig(Config))
		{
			AddError(Result, TEXT("invalid_coalition_emergency_vote_config"),
				TEXT("Emergency solidarity vote cost, recovery, voter support, and voter-pressure settings must remain within supported limits."));
			return false;
		}
		return true;
	}

	bool ApplyAdversaryAdaptation(
		FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const bool bEscaped,
		int32& OutPreviousEscalation)
	{
		OutPreviousEscalation = State.AdversaryEscalationLevel;
		FAdversaryAdaptationProgress Progress;
		if (!FStrategicCommandService::GetAdversaryAdaptationProgress(State, Config, Progress))
		{
			return false;
		}
		const int32 EscapeEscalation = bEscaped
			? FMath::Min(OutPreviousEscalation + 1, Config.MaxAdversaryEscalation)
			: OutPreviousEscalation;
		State.AdversaryEscalationLevel = FMath::Max(EscapeEscalation, Progress.EscalationFloor);
		return true;
	}

	void AddAdversaryEscalationEvent(
		FStrategicCommandResult& Result,
		const FAdversaryMissionState& Mission,
		const int32 PreviousEscalation,
		const int32 CurrentEscalation,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		if (CurrentEscalation == PreviousEscalation)
		{
			return;
		}
		FStrategicEvent& Adapted = AddEvent(
			Result, EStrategicEventType::AdversaryEscalationChanged, CommandSequence, TimestampUtc);
		Adapted.MissionId = Mission.MissionId;
		Adapted.ContactId = Mission.ContactId;
		Adapted.RuleId = Mission.MissionRuleId;
		Adapted.Amount = CurrentEscalation - PreviousEscalation;
		Adapted.Quantity = CurrentEscalation;
	}

	void EvaluateCampaignOutcome(
		FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		FStrategicCommandResult& Result,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		if (State.Outcome != ECampaignOutcome::Ongoing)
		{
			return;
		}
		const FRegionalPressureState* FailedRegion = State.RegionalPressure.FindByPredicate(
			[&Config](const FRegionalPressureState& Pressure)
			{
				return Pressure.Pressure >= Config.FailurePressureThreshold;
			});
		if (FailedRegion != nullptr)
		{
			State.Outcome = ECampaignOutcome::Failure;
			State.OutcomeReasonId = TEXT("outcome.regional-collapse");
			State.NextAdversaryMissionSeconds = 0;
			FStrategicEvent& Lost = AddEvent(Result, EStrategicEventType::CampaignLost, CommandSequence, TimestampUtc);
			Lost.RuleId = State.OutcomeReasonId;
			Lost.RegionId = FailedRegion->RegionId;
			Lost.Quantity = FailedRegion->Pressure;
			return;
		}
		if (State.AdversaryMissionsThwarted >= Config.VictoryThwartedMissions
			&& State.AdversaryEscalationLevel >= Config.VictoryMinimumEscalationLevel)
		{
			State.Outcome = ECampaignOutcome::Victory;
			State.OutcomeReasonId = TEXT("outcome.adversary-contained");
			State.NextAdversaryMissionSeconds = 0;
			FStrategicEvent& Won = AddEvent(Result, EStrategicEventType::CampaignWon, CommandSequence, TimestampUtc);
			Won.RuleId = State.OutcomeReasonId;
			Won.Quantity = State.AdversaryMissionsThwarted;
		}
	}

	struct FCoalitionCounterplaySupportChange
	{
		FName RegionId;
		int32 PreviousSupport = 0;
		int32 CurrentSupport = 0;
		bool bWithdrew = false;
	};

	bool ApplyCompactPeerStrain(
		FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FName TargetRegionId,
		const int32 AuthoredSupportLoss,
		TArray<FCoalitionCounterplaySupportChange>& OutChanges)
	{
		OutChanges.Reset();
		if (!State.bHorizonCompactRatified || AuthoredSupportLoss == 0)
		{
			return true;
		}

		int64 ScaledSupportLoss = 0;
		if (!FStrategicCommandService::ScaleAdversaryEscapeConsequence(
				AuthoredSupportLoss, State.Difficulty, Config, ScaledSupportLoss)
			|| ScaledSupportLoss <= 0 || ScaledSupportLoss > MAX_int32)
		{
			return false;
		}

		TArray<FName> AffectedRegionIds;
		for (const FRegionalMandateState& Mandate : State.RegionalMandates)
		{
			if (Mandate.RegionId != TargetRegionId
				&& IsActiveHorizonCompactMember(State, Mandate))
			{
				AffectedRegionIds.Add(Mandate.RegionId);
			}
		}
		AffectedRegionIds.Sort(
			[](const FName& Left, const FName& Right)
			{
				return Left.LexicalLess(Right);
			});

		for (const FName RegionId : AffectedRegionIds)
		{
			FRegionalMandateState* Mandate = FindRegionalMandate(State, RegionId);
			if (Mandate == nullptr || !IsActiveHorizonCompactMember(State, *Mandate))
			{
				return false;
			}
			FCoalitionCounterplaySupportChange& Change = OutChanges.AddDefaulted_GetRef();
			Change.RegionId = RegionId;
			Change.PreviousSupport = Mandate->Support;
			const int64 ReducedSupport = FMath::Max<int64>(
				0, static_cast<int64>(Mandate->Support) - ScaledSupportLoss);
			Mandate->Support = static_cast<int32>(ReducedSupport);
			Change.CurrentSupport = Mandate->Support;
			Change.bWithdrew = WithdrawHorizonCompactMemberIfRequired(
				State, *Mandate, Config);

			int64 UpdatedContribution = 0;
			int64 UpdatedMonthlyFunding = 0;
			if (!FStrategicCommandService::CalculateRegionalFundingContribution(
					*Mandate, Config, true, UpdatedContribution)
				|| !TryAdd(
					State.MonthlyFunding,
					UpdatedContribution - Mandate->CurrentMonthlyFunding,
					UpdatedMonthlyFunding))
			{
				return false;
			}
			Mandate->CurrentMonthlyFunding = UpdatedContribution;
			State.MonthlyFunding = UpdatedMonthlyFunding;
		}
		return true;
	}

	bool ApplyWithdrawnCompactRecovery(
		FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const int32 AuthoredSupportGain,
		TArray<FCoalitionCounterplaySupportChange>& OutChanges)
	{
		OutChanges.Reset();
		if (!State.bHorizonCompactRatified || AuthoredSupportGain == 0)
		{
			return true;
		}

		TArray<FName> AffectedRegionIds;
		for (const FRegionalMandateState& Mandate : State.RegionalMandates)
		{
			if (Mandate.bResilienceCharterSigned
				&& Mandate.bHorizonCompactMemberWithdrawn)
			{
				AffectedRegionIds.Add(Mandate.RegionId);
			}
		}
		AffectedRegionIds.Sort(
			[](const FName& Left, const FName& Right)
			{
				return Left.LexicalLess(Right);
			});

		for (const FName RegionId : AffectedRegionIds)
		{
			FRegionalMandateState* Mandate = FindRegionalMandate(State, RegionId);
			if (Mandate == nullptr || !Mandate->bResilienceCharterSigned
				|| !Mandate->bHorizonCompactMemberWithdrawn)
			{
				return false;
			}
			FCoalitionCounterplaySupportChange& Change = OutChanges.AddDefaulted_GetRef();
			Change.RegionId = RegionId;
			Change.PreviousSupport = Mandate->Support;
			Mandate->Support = FMath::Clamp(
				Mandate->Support + AuthoredSupportGain, 0, 100);
			Change.CurrentSupport = Mandate->Support;
			if (Change.CurrentSupport == Change.PreviousSupport)
			{
				OutChanges.Pop();
				continue;
			}

			int64 UpdatedContribution = 0;
			int64 UpdatedMonthlyFunding = 0;
			if (!FStrategicCommandService::CalculateRegionalFundingContribution(
					*Mandate, Config, true, UpdatedContribution)
				|| !TryAdd(
					State.MonthlyFunding,
					UpdatedContribution - Mandate->CurrentMonthlyFunding,
					UpdatedMonthlyFunding))
			{
				return false;
			}
			Mandate->CurrentMonthlyFunding = UpdatedContribution;
			State.MonthlyFunding = UpdatedMonthlyFunding;
		}
		return true;
	}

	bool LaunchAdversaryMission(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		FStrategicCommandResult& Result,
		int64 CommandSequence,
		const FDateTime& TimestampUtc,
		const FAdversaryMissionRule* ForcedRule = nullptr);

	bool AdvanceAdversaryPlan(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FAdversaryMissionState& ResolvedMission,
		const FAdversaryMissionRule& ResolvedRule,
		const bool bEscaped,
		FStrategicCommandResult& Result,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		if (ResolvedRule.PlanId.IsNone())
		{
			return true;
		}
		const FAdversaryPlanRule* Plan = Rules.AdversaryPlans.Find(ResolvedRule.PlanId);
		if (Plan == nullptr)
		{
			return false;
		}

		const FName BranchRuleId = bEscaped
			? ResolvedRule.EscapeBranchMissionRuleId
			: ResolvedRule.ThwartBranchMissionRuleId;
		if (State.Outcome != ECampaignOutcome::Ongoing || BranchRuleId.IsNone())
		{
			FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::AdversaryPlanCompleted, CommandSequence, TimestampUtc);
			Completed.MissionId = ResolvedMission.MissionId;
			Completed.ContactId = ResolvedMission.ContactId;
			Completed.RuleId = Plan->Identity.RuleId;
			Completed.Quantity = ResolvedRule.PlanStage;
			Completed.bSuccessful = !bEscaped;
			return true;
		}

		const FAdversaryMissionRule* BranchRule = Rules.AdversaryMissions.Find(BranchRuleId);
		if (BranchRule == nullptr
			|| BranchRule->PlanId != ResolvedRule.PlanId
			|| BranchRule->PlanStage != ResolvedRule.PlanStage + 1)
		{
			return false;
		}
		return LaunchAdversaryMission(
			State, Rules, Config, Result, CommandSequence, TimestampUtc, BranchRule);
	}

	bool ApplyAdversaryMissionEscape(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FGuid& ContactId,
		FStrategicCommandResult& Result,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		const FAdversaryMissionState* ExistingMission = FindAdversaryMission(State, ContactId);
		if (ExistingMission == nullptr)
		{
			return true;
		}
		const FAdversaryMissionState Mission = *ExistingMission;
		const FAdversaryMissionRule* MissionRule = Rules.AdversaryMissions.Find(Mission.MissionRuleId);
		if (MissionRule == nullptr || State.AdversaryMissionsEscaped == MAX_int32)
		{
			return false;
		}
		int64 ScorePenalty = 0;
		int64 FundingPenalty = 0;
		int64 PressureIncrease = 0;
		int64 SupportLoss = 0;
		if (!FStrategicCommandService::ScaleAdversaryEscapeConsequence(
				MissionRule->ScorePenaltyOnEscape, State.Difficulty, Config, ScorePenalty)
			|| !FStrategicCommandService::ScaleAdversaryEscapeConsequence(
				MissionRule->FundingPenaltyOnEscape, State.Difficulty, Config, FundingPenalty)
			|| !FStrategicCommandService::ScaleAdversaryEscapeConsequence(
				MissionRule->PressureOnEscape, State.Difficulty, Config, PressureIncrease)
			|| !FStrategicCommandService::ScaleAdversaryEscapeConsequence(
				MissionRule->SupportLossOnEscape, State.Difficulty, Config, SupportLoss))
		{
			return false;
		}
		const FName TargetRegionId = ResolveMissionRegionId(State, Mission, *MissionRule);
		const FRegionalMandateState* ExistingMandate = FindRegionalMandate(State, TargetRegionId);
		FName SharedPressureRegionId;
		int64 SharedPressureIncrease = 0;
		if (ExistingMandate != nullptr && ExistingMandate->bResilienceCharterSigned)
		{
			int64 ReducedPressure = 0;
			if (!TryScaleNonNegativeByPercent(
					PressureIncrease, Config.ResilienceCharterEscapePressurePercent, ReducedPressure))
			{
				return false;
			}
			PressureIncrease = ReducedPressure;
			if (IsActiveHorizonCompactMember(State, *ExistingMandate)
				&& PressureIncrease > 0)
			{
				const FRegionalPressureState* Recipient =
					FindHorizonCompactPressureRecipient(State, TargetRegionId);
				if (Recipient != nullptr)
				{
					if (!TryScaleNonNegativeByPercent(
							PressureIncrease, Config.HorizonCompactSharedEscapePressurePercent,
							SharedPressureIncrease)
						|| SharedPressureIncrease > PressureIncrease)
					{
						return false;
					}
					SharedPressureRegionId = Recipient->RegionId;
					PressureIncrease -= SharedPressureIncrease;
				}
			}
		}
		int64 NewScore = 0;
		int64 PenalizedFunding = 0;
		if (!TryAdd(State.CampaignScore, -ScorePenalty, NewScore))
		{
			return false;
		}
		if (!TryAdd(State.MonthlyFunding, -FundingPenalty, PenalizedFunding))
		{
			return false;
		}
		FRegionalPressureState* Pressure = FindRegionalPressure(State, TargetRegionId);
		FRegionalPressureState* SharedPressure = SharedPressureRegionId.IsNone()
			? nullptr
			: FindRegionalPressure(State, SharedPressureRegionId);
		FRegionalMandateState* Mandate = FindRegionalMandate(State, TargetRegionId);
		if (Pressure == nullptr)
		{
			Pressure = &State.RegionalPressure.AddDefaulted_GetRef();
			Pressure->RegionId = TargetRegionId;
		}
		const int32 OldPressure = Pressure->Pressure;
		const int32 OldSharedPressure = SharedPressure != nullptr ? SharedPressure->Pressure : 0;
		Pressure->Pressure = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(Pressure->Pressure) + PressureIncrease, 0, 100));
		if (!SharedPressureRegionId.IsNone() && SharedPressure == nullptr)
		{
			return false;
		}
		if (SharedPressure != nullptr)
		{
			SharedPressure->Pressure = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(SharedPressure->Pressure) + SharedPressureIncrease, 0, 100));
		}
		const int64 OldFunding = State.MonthlyFunding;
		int32 OldSupport = 0;
		int64 OldContribution = 0;
		bool bWithdrewFromCompact = false;
		if (Mandate != nullptr)
		{
			OldSupport = Mandate->Support;
			OldContribution = Mandate->CurrentMonthlyFunding;
			Mandate->Support = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(Mandate->Support) - SupportLoss, 0, 100));
			Mandate->BaselineMonthlyFunding = FMath::Max<int64>(
				0, Mandate->BaselineMonthlyFunding - FMath::Min(Mandate->BaselineMonthlyFunding, FundingPenalty));
			bWithdrewFromCompact = Mandate->Support < OldSupport
				&& WithdrawHorizonCompactMemberIfRequired(State, *Mandate, Config);
			if (bWithdrewFromCompact)
			{
				if (!FStrategicCommandService::CalculateRegionalFundingContribution(
						*Mandate, Config, true, Mandate->CurrentMonthlyFunding)
					|| !TryAdd(
						OldFunding,
						Mandate->CurrentMonthlyFunding - OldContribution,
						State.MonthlyFunding))
				{
					return false;
				}
			}
			else
			{
				const int64 CurrentFundingReduction =
					FMath::Min(Mandate->CurrentMonthlyFunding, FundingPenalty);
				Mandate->CurrentMonthlyFunding -= CurrentFundingReduction;
				State.MonthlyFunding -= CurrentFundingReduction;
			}
		}
		else
		{
			State.MonthlyFunding = FMath::Max<int64>(0, PenalizedFunding);
		}
		TArray<FCoalitionCounterplaySupportChange> CoalitionStrainChanges;
		if (!ApplyCompactPeerStrain(
				State, Config, TargetRegionId,
				MissionRule->CompactPeerSupportLossOnEscape,
				CoalitionStrainChanges))
		{
			return false;
		}
		State.CampaignScore = NewScore;
		++State.AdversaryMissionsEscaped;
		int32 PreviousEscalation = 0;
		if (!ApplyAdversaryAdaptation(State, Config, true, PreviousEscalation))
		{
			return false;
		}
		State.AdversaryMissions.RemoveAll(
			[&Mission](const FAdversaryMissionState& Entry) { return Entry.MissionId == Mission.MissionId; });

		FStrategicEvent& Escaped = AddEvent(Result, EStrategicEventType::AdversaryMissionEscaped, CommandSequence, TimestampUtc);
		Escaped.MissionId = Mission.MissionId;
		Escaped.ContactId = ContactId;
		Escaped.RuleId = Mission.MissionRuleId;
		Escaped.RegionId = TargetRegionId;
		Escaped.Amount = -ScorePenalty;
		Escaped.Quantity = State.AdversaryEscalationLevel;
		AddAdversaryEscalationEvent(
			Result, Mission, PreviousEscalation, State.AdversaryEscalationLevel,
			CommandSequence, TimestampUtc);
		if (SharedPressure != nullptr && SharedPressure->Pressure != OldSharedPressure)
		{
			FStrategicEvent& Shared = AddEvent(Result, EStrategicEventType::CoalitionPressureShared,
				CommandSequence, TimestampUtc);
			Shared.MissionId = Mission.MissionId;
			Shared.ContactId = ContactId;
			Shared.RuleId = TEXT("coalition.horizon-compact");
			Shared.RegionId = SharedPressureRegionId;
			Shared.Amount = SharedPressure->Pressure - OldSharedPressure;
			Shared.Quantity = Pressure->Pressure - OldPressure;
			Shared.bSuccessful = true;
		}
		FStrategicEvent& PressureChanged = AddEvent(Result, EStrategicEventType::RegionalPressureChanged, CommandSequence, TimestampUtc);
		PressureChanged.MissionId = Mission.MissionId;
		PressureChanged.ContactId = ContactId;
		PressureChanged.RuleId = Mission.MissionRuleId;
		PressureChanged.RegionId = TargetRegionId;
		PressureChanged.Amount = Pressure->Pressure - OldPressure;
		PressureChanged.Quantity = Pressure->Pressure;
		if (SharedPressure != nullptr && SharedPressure->Pressure != OldSharedPressure)
		{
			FStrategicEvent& RecipientPressureChanged = AddEvent(
				Result, EStrategicEventType::RegionalPressureChanged, CommandSequence, TimestampUtc);
			RecipientPressureChanged.MissionId = Mission.MissionId;
			RecipientPressureChanged.ContactId = ContactId;
			RecipientPressureChanged.RuleId = Mission.MissionRuleId;
			RecipientPressureChanged.RegionId = SharedPressureRegionId;
			RecipientPressureChanged.Amount = SharedPressure->Pressure - OldSharedPressure;
			RecipientPressureChanged.Quantity = SharedPressure->Pressure;
		}
		if (Mandate != nullptr && Mandate->Support != OldSupport)
		{
			FStrategicEvent& SupportChanged = AddEvent(Result, EStrategicEventType::RegionalSupportChanged, CommandSequence, TimestampUtc);
			SupportChanged.MissionId = Mission.MissionId;
			SupportChanged.ContactId = ContactId;
			SupportChanged.RuleId = Mission.MissionRuleId;
			SupportChanged.RegionId = TargetRegionId;
			SupportChanged.Amount = Mandate->Support - OldSupport;
			SupportChanged.Quantity = Mandate->Support;
		}
		if (Mandate != nullptr && bWithdrewFromCompact)
		{
			FStrategicEvent& Withdrew = AddHorizonCompactWithdrawalEvent(
				Result, *Mandate, Config, CommandSequence, TimestampUtc);
			Withdrew.MissionId = Mission.MissionId;
			Withdrew.ContactId = ContactId;
		}
		for (const FCoalitionCounterplaySupportChange& Change : CoalitionStrainChanges)
		{
			FStrategicEvent& Strained = AddEvent(
				Result, EStrategicEventType::CoalitionCohesionStrained,
				CommandSequence, TimestampUtc);
			Strained.MissionId = Mission.MissionId;
			Strained.ContactId = ContactId;
			Strained.RuleId = Mission.MissionRuleId;
			Strained.RegionId = Change.RegionId;
			Strained.Amount = Change.CurrentSupport - Change.PreviousSupport;
			Strained.Quantity = Change.CurrentSupport;

			FStrategicEvent& SupportChanged = AddEvent(
				Result, EStrategicEventType::RegionalSupportChanged,
				CommandSequence, TimestampUtc);
			SupportChanged.MissionId = Mission.MissionId;
			SupportChanged.ContactId = ContactId;
			SupportChanged.RuleId = Mission.MissionRuleId;
			SupportChanged.RegionId = Change.RegionId;
			SupportChanged.Amount = Change.CurrentSupport - Change.PreviousSupport;
			SupportChanged.Quantity = Change.CurrentSupport;

			if (Change.bWithdrew)
			{
				const FRegionalMandateState* WithdrawnMandate =
					FindRegionalMandate(State, Change.RegionId);
				if (WithdrawnMandate == nullptr)
				{
					return false;
				}
				FStrategicEvent& Withdrew = AddHorizonCompactWithdrawalEvent(
					Result, *WithdrawnMandate, Config,
					CommandSequence, TimestampUtc);
				Withdrew.MissionId = Mission.MissionId;
				Withdrew.ContactId = ContactId;
			}
		}
		if (State.MonthlyFunding != OldFunding)
		{
			FStrategicEvent& FundingChanged = AddEvent(Result, EStrategicEventType::MonthlyFundingChanged, CommandSequence, TimestampUtc);
			FundingChanged.MissionId = Mission.MissionId;
			FundingChanged.ContactId = ContactId;
			FundingChanged.RuleId = Mission.MissionRuleId;
			FundingChanged.RegionId = TargetRegionId;
			FundingChanged.Amount = State.MonthlyFunding - OldFunding;
		}
		EvaluateCampaignOutcome(State, Config, Result, CommandSequence, TimestampUtc);
		return AdvanceAdversaryPlan(
			State, Rules, Config, Mission, *MissionRule, true,
			Result, CommandSequence, TimestampUtc);
	}

	bool ApplyAdversaryMissionThwarted(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FGuid& ContactId,
		FStrategicCommandResult& Result,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		const FAdversaryMissionState* ExistingMission = FindAdversaryMission(State, ContactId);
		if (ExistingMission == nullptr)
		{
			return true;
		}
		const FAdversaryMissionState Mission = *ExistingMission;
		const FAdversaryMissionRule* MissionRule = Rules.AdversaryMissions.Find(Mission.MissionRuleId);
		if (MissionRule == nullptr || State.AdversaryMissionsThwarted == MAX_int32)
		{
			return false;
		}
		const FName TargetRegionId = ResolveMissionRegionId(State, Mission, *MissionRule);
		FRegionalPressureState* Pressure = FindRegionalPressure(State, TargetRegionId);
		if (Pressure == nullptr)
		{
			Pressure = &State.RegionalPressure.AddDefaulted_GetRef();
			Pressure->RegionId = TargetRegionId;
		}
		const int32 OldPressure = Pressure->Pressure;
		Pressure->Pressure = FMath::Max(0, Pressure->Pressure - MissionRule->PressureReductionOnDestroyed);
		FRegionalMandateState* Mandate = FindRegionalMandate(State, TargetRegionId);
		const int32 OldSupport = Mandate != nullptr ? Mandate->Support : 0;
		const int64 OldFunding = State.MonthlyFunding;
		if (Mandate != nullptr)
		{
			Mandate->Support = FMath::Clamp(Mandate->Support + MissionRule->SupportGainOnThwarted, 0, 100);
		}
		const int32 DirectTargetSupport = Mandate != nullptr ? Mandate->Support : 0;
		TArray<FCoalitionCounterplaySupportChange> CoalitionRecoveryChanges;
		if (!ApplyWithdrawnCompactRecovery(
				State, Config,
				MissionRule->WithdrawnCompactSupportGainOnThwarted,
				CoalitionRecoveryChanges))
		{
			return false;
		}
		++State.AdversaryMissionsThwarted;
		int32 PreviousEscalation = 0;
		if (!ApplyAdversaryAdaptation(State, Config, false, PreviousEscalation))
		{
			return false;
		}
		State.AdversaryMissions.RemoveAll(
			[&Mission](const FAdversaryMissionState& Entry) { return Entry.MissionId == Mission.MissionId; });

		FStrategicEvent& Thwarted = AddEvent(Result, EStrategicEventType::AdversaryMissionThwarted, CommandSequence, TimestampUtc);
		Thwarted.MissionId = Mission.MissionId;
		Thwarted.ContactId = ContactId;
		Thwarted.RuleId = Mission.MissionRuleId;
		Thwarted.RegionId = TargetRegionId;
		Thwarted.Quantity = State.AdversaryMissionsThwarted;
		AddAdversaryEscalationEvent(
			Result, Mission, PreviousEscalation, State.AdversaryEscalationLevel,
			CommandSequence, TimestampUtc);
		FStrategicEvent& PressureChanged = AddEvent(Result, EStrategicEventType::RegionalPressureChanged, CommandSequence, TimestampUtc);
		PressureChanged.MissionId = Mission.MissionId;
		PressureChanged.ContactId = ContactId;
		PressureChanged.RuleId = Mission.MissionRuleId;
		PressureChanged.RegionId = TargetRegionId;
		PressureChanged.Amount = Pressure->Pressure - OldPressure;
		PressureChanged.Quantity = Pressure->Pressure;
		if (Mandate != nullptr && DirectTargetSupport != OldSupport)
		{
			FStrategicEvent& SupportChanged = AddEvent(Result, EStrategicEventType::RegionalSupportChanged, CommandSequence, TimestampUtc);
			SupportChanged.MissionId = Mission.MissionId;
			SupportChanged.ContactId = ContactId;
			SupportChanged.RuleId = Mission.MissionRuleId;
			SupportChanged.RegionId = TargetRegionId;
			SupportChanged.Amount = DirectTargetSupport - OldSupport;
			SupportChanged.Quantity = DirectTargetSupport;
		}
		for (const FCoalitionCounterplaySupportChange& Change : CoalitionRecoveryChanges)
		{
			FStrategicEvent& Inspired = AddEvent(
				Result, EStrategicEventType::CoalitionRecoveryInspired,
				CommandSequence, TimestampUtc);
			Inspired.MissionId = Mission.MissionId;
			Inspired.ContactId = ContactId;
			Inspired.RuleId = Mission.MissionRuleId;
			Inspired.RegionId = Change.RegionId;
			Inspired.Amount = Change.CurrentSupport - Change.PreviousSupport;
			Inspired.Quantity = Change.CurrentSupport;
			Inspired.bSuccessful = true;

			FStrategicEvent& SupportChanged = AddEvent(
				Result, EStrategicEventType::RegionalSupportChanged,
				CommandSequence, TimestampUtc);
			SupportChanged.MissionId = Mission.MissionId;
			SupportChanged.ContactId = ContactId;
			SupportChanged.RuleId = Mission.MissionRuleId;
			SupportChanged.RegionId = Change.RegionId;
			SupportChanged.Amount = Change.CurrentSupport - Change.PreviousSupport;
			SupportChanged.Quantity = Change.CurrentSupport;
		}
		if (State.MonthlyFunding != OldFunding)
		{
			FStrategicEvent& FundingChanged = AddEvent(
				Result, EStrategicEventType::MonthlyFundingChanged,
				CommandSequence, TimestampUtc);
			FundingChanged.MissionId = Mission.MissionId;
			FundingChanged.ContactId = ContactId;
			FundingChanged.RuleId = Mission.MissionRuleId;
			FundingChanged.RegionId = TargetRegionId;
			FundingChanged.Amount = State.MonthlyFunding - OldFunding;
		}
		EvaluateCampaignOutcome(State, Config, Result, CommandSequence, TimestampUtc);
		return AdvanceAdversaryPlan(
			State, Rules, Config, Mission, *MissionRule, false,
			Result, CommandSequence, TimestampUtc);
	}

	bool LaunchAdversaryMission(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		FStrategicCommandResult& Result,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc,
		const FAdversaryMissionRule* ForcedRule)
	{
		if (State.AdversaryMissions.Num() >= Config.MaxActiveAdversaryMissions)
		{
			if (ForcedRule != nullptr)
			{
				return false;
			}
			State.NextAdversaryMissionSeconds = 3600;
			return true;
		}

		const FAdversaryMissionRule* SelectedRule = ForcedRule;
		if (SelectedRule == nullptr)
		{
			struct FWeightedMissionCandidate
			{
				const FAdversaryMissionRule* Rule = nullptr;
				int32 EffectiveWeight = 0;
			};
			TArray<FWeightedMissionCandidate> Eligible;
			for (const TPair<FName, FAdversaryMissionRule>& Pair : Rules.AdversaryMissions)
			{
				const FAdversaryPlanRule* Plan = Pair.Value.PlanId.IsNone()
					? nullptr
					: Rules.AdversaryPlans.Find(Pair.Value.PlanId);
				const bool bWeightedMission = Pair.Value.PlanId.IsNone()
					|| (Plan != nullptr && Plan->OpeningMissionRuleId == Pair.Key);
				if (bWeightedMission
					&& Pair.Value.MinimumEscalation <= State.AdversaryEscalationLevel
					&& (!Pair.Value.bTargetsPlayerBase || !State.Bases.IsEmpty()))
				{
					int64 EffectiveWeight = Pair.Value.SelectionWeight;
					const FRegionalMandateState* Mandate = Pair.Value.bTargetsPlayerBase
						? nullptr
						: FindRegionalMandate(State, Pair.Value.TargetRegionId);
					if (Mandate != nullptr && Mandate->bResilienceCharterSigned
						&& !TryScaleNonNegativeByPercent(
							Pair.Value.SelectionWeight,
							Config.ResilienceCharterMissionWeightPercent,
							EffectiveWeight))
					{
						return false;
					}
					if (EffectiveWeight <= 0 || EffectiveWeight > MAX_int32)
					{
						return false;
					}
					Eligible.Add({ &Pair.Value, static_cast<int32>(EffectiveWeight) });
				}
			}
			Eligible.Sort(
				[](const FWeightedMissionCandidate& Left, const FWeightedMissionCandidate& Right)
				{
					return Left.Rule->Identity.RuleId.LexicalLess(Right.Rule->Identity.RuleId);
				});
			if (Eligible.IsEmpty())
			{
				State.NextAdversaryMissionSeconds = 3600;
				return true;
			}
			int64 TotalWeight = 0;
			for (const FWeightedMissionCandidate& Candidate : Eligible)
			{
				TotalWeight += Candidate.EffectiveWeight;
			}
			if (TotalWeight <= 0 || TotalWeight > MAX_int32)
			{
				return false;
			}
			const int32 Selection = State.SimulationRandom.NextIntInclusive(1, static_cast<int32>(TotalWeight));
			int32 AccumulatedWeight = 0;
			SelectedRule = Eligible.Last().Rule;
			for (const FWeightedMissionCandidate& Candidate : Eligible)
			{
				AccumulatedWeight += Candidate.EffectiveWeight;
				if (Selection <= AccumulatedWeight)
				{
					SelectedRule = Candidate.Rule;
					break;
				}
			}
		}
		if (SelectedRule == nullptr
			|| State.NextAdversaryMissionSerial <= 0 || State.NextAdversaryMissionSerial == MAX_int64
			|| State.AdversaryMissionsLaunched == MAX_int32)
		{
			return false;
		}
		const FContactRule* ContactRule = Rules.Contacts.Find(SelectedRule->ContactRuleId);
		const FStrategicBaseState* TargetBase = nullptr;
		if (SelectedRule->bTargetsPlayerBase)
		{
			TArray<const FStrategicBaseState*> CandidateBases;
			CandidateBases.Reserve(State.Bases.Num());
			for (const FStrategicBaseState& Base : State.Bases)
			{
				CandidateBases.Add(&Base);
			}
			CandidateBases.Sort(
				[](const FStrategicBaseState& Left, const FStrategicBaseState& Right)
				{
					return Left.BaseId.ToString(EGuidFormats::Digits) < Right.BaseId.ToString(EGuidFormats::Digits);
				});
			if (CandidateBases.IsEmpty())
			{
				return false;
			}
			const int32 TargetIndex = CandidateBases.Num() == 1
				? 0
				: State.SimulationRandom.NextIntInclusive(0, CandidateBases.Num() - 1);
			TargetBase = CandidateBases[TargetIndex];
		}
		const int32 DestinationLongitude = TargetBase != nullptr
			? TargetBase->LongitudeMilliDegrees
			: SelectedRule->DestinationLongitudeMilliDegrees;
		const int32 DestinationLatitude = TargetBase != nullptr
			? TargetBase->LatitudeMilliDegrees
			: SelectedRule->DestinationLatitudeMilliDegrees;
		const int32 OriginLongitude = SelectedRule->OriginLongitudeMilliDegrees;
		int32 OriginLatitude = SelectedRule->OriginLatitudeMilliDegrees;
		if (OriginLongitude == DestinationLongitude && OriginLatitude == DestinationLatitude)
		{
			OriginLatitude += OriginLatitude >= 0 ? -1000 : 1000;
		}
		int64 RouteSeconds = 0;
		const int64 RouteDistance = ApproximateSurfaceDistanceKilometers(
			OriginLongitude,
			OriginLatitude,
			DestinationLongitude,
			DestinationLatitude);
		int64 NextMissionIntervalSeconds = 0;
		if (ContactRule == nullptr || RouteDistance <= 0
			|| !ComputeTravelSeconds(RouteDistance, ContactRule->CruiseSpeedKilometersPerHour, RouteSeconds)
			|| SelectedRule->IntervalHours <= 0
			|| SelectedRule->IntervalHours > MAX_int64 / 3600LL
			|| !FStrategicCommandService::ScaleAdversaryIntervalSeconds(
				static_cast<int64>(SelectedRule->IntervalHours) * 3600LL,
				State.Difficulty,
				Config,
				NextMissionIntervalSeconds))
		{
			return false;
		}
		const int64 Serial = State.NextAdversaryMissionSerial;
		const FGuid MissionId = MakeDeterministicAdversaryId(State.SimulationRandom.InitialSeed, Serial, TEXT("mission"));
		const FGuid ContactId = MakeDeterministicAdversaryId(State.SimulationRandom.InitialSeed, Serial, TEXT("contact"));
		const bool bIdentityConflict = State.AdversaryMissions.ContainsByPredicate(
			[&MissionId](const FAdversaryMissionState& Mission) { return Mission.MissionId == MissionId; })
			|| FindContact(State, ContactId) != nullptr || FindSite(State, ContactId) != nullptr;
		if (!MissionId.IsValid() || !ContactId.IsValid() || MissionId == ContactId || bIdentityConflict)
		{
			return false;
		}

		FStrategicContactState& Contact = State.StrategicContacts.AddDefaulted_GetRef();
		Contact.ContactId = ContactId;
		Contact.ContactRuleId = SelectedRule->ContactRuleId;
		Contact.Status = EStrategicContactStatus::Hidden;
		Contact.OriginLongitudeMilliDegrees = OriginLongitude;
		Contact.OriginLatitudeMilliDegrees = OriginLatitude;
		Contact.LongitudeMilliDegrees = OriginLongitude;
		Contact.LatitudeMilliDegrees = OriginLatitude;
		Contact.DestinationLongitudeMilliDegrees = DestinationLongitude;
		Contact.DestinationLatitudeMilliDegrees = DestinationLatitude;
		Contact.TotalRouteSeconds = RouteSeconds;
		Contact.CurrentHull = ContactRule->MaxHull;
		FAdversaryMissionState& Mission = State.AdversaryMissions.AddDefaulted_GetRef();
		Mission.MissionId = MissionId;
		Mission.ContactId = ContactId;
		Mission.MissionRuleId = SelectedRule->Identity.RuleId;
		Mission.TargetBaseId = TargetBase != nullptr ? TargetBase->BaseId : FGuid();
		Mission.StartedUtc = TimestampUtc;
		const FName TargetRegionId = TargetBase != nullptr ? TargetBase->RegionId : SelectedRule->TargetRegionId;
		if (FindRegionalPressure(State, TargetRegionId) == nullptr)
		{
			FRegionalPressureState& Pressure = State.RegionalPressure.AddDefaulted_GetRef();
			Pressure.RegionId = TargetRegionId;
		}
		++State.NextAdversaryMissionSerial;
		++State.AdversaryMissionsLaunched;
		State.NextAdversaryMissionSeconds = NextMissionIntervalSeconds;

		FStrategicEvent& Launched = AddEvent(Result, EStrategicEventType::AdversaryMissionLaunched, CommandSequence, TimestampUtc);
		Launched.MissionId = MissionId;
		Launched.ContactId = ContactId;
		Launched.BaseId = Mission.TargetBaseId;
		Launched.RuleId = SelectedRule->Identity.RuleId;
		Launched.RegionId = TargetRegionId;
		Launched.Quantity = State.AdversaryEscalationLevel;
		FStrategicEvent& Created = AddEvent(Result, EStrategicEventType::StrategicContactCreated, CommandSequence, TimestampUtc);
		Created.MissionId = MissionId;
		Created.ContactId = ContactId;
		Created.BaseId = Mission.TargetBaseId;
		Created.RuleId = Contact.ContactRuleId;
		Created.RegionId = TargetRegionId;
		Created.Quantity = ContactRule->ThreatRating;
		if (!SelectedRule->PlanId.IsNone())
		{
			FStrategicEvent& PlanEvent = AddEvent(
				Result,
				SelectedRule->PlanStage == 1
					? EStrategicEventType::AdversaryPlanStarted
					: EStrategicEventType::AdversaryPlanAdvanced,
				CommandSequence,
				TimestampUtc);
			PlanEvent.MissionId = MissionId;
			PlanEvent.ContactId = ContactId;
			PlanEvent.BaseId = Mission.TargetBaseId;
			PlanEvent.RuleId = SelectedRule->PlanId;
			PlanEvent.RegionId = TargetRegionId;
			PlanEvent.Quantity = SelectedRule->PlanStage;
			PlanEvent.bSuccessful = true;
		}
		return true;
	}

	bool ValidateFacilities(
		const TArray<FName>& FacilityIds,
		const FResolvedRuleSet& Rules,
		const FName DuplicateDiagnostic,
		FStrategicCommandResult& Result)
	{
		TSet<FName> Seen;
		for (const FName FacilityId : FacilityIds)
		{
			if (!Rules.Facilities.Contains(FacilityId))
			{
				AddError(Result, TEXT("unknown_facility"), FString::Printf(TEXT("Facility rule '%s' is not loaded."), *FacilityId.ToString()));
				return false;
			}
			if (Seen.Contains(FacilityId))
			{
				AddError(Result, DuplicateDiagnostic, FString::Printf(TEXT("Facility '%s' appears more than once."), *FacilityId.ToString()));
				return false;
			}
			Seen.Add(FacilityId);
		}
		return true;
	}

	bool ComputeMonthlyMaintenance(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		int64& OutMaintenance,
		FStrategicCommandResult& Result)
	{
		OutMaintenance = 0;
		for (const FStrategicBaseState& Base : State.Bases)
		{
			TArray<FName> OperationalFacilityIds;
			if (!Base.Facilities.IsEmpty())
			{
				for (const FBaseFacilityState& Facility : Base.Facilities)
				{
					OperationalFacilityIds.Add(Facility.FacilityId);
				}
			}
			else
			{
				OperationalFacilityIds = Base.BuiltFacilities;
			}
			for (const FName FacilityId : OperationalFacilityIds)
			{
				const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
				if (Facility == nullptr)
				{
					AddError(Result, TEXT("unknown_facility"), FString::Printf(TEXT("Base '%s' references unloaded facility '%s'."), *Base.Name, *FacilityId.ToString()));
					return false;
				}
				if (!TryAdd(OutMaintenance, Facility->MonthlyMaintenance, OutMaintenance))
				{
					AddError(Result, TEXT("financial_overflow"), TEXT("Monthly facility maintenance exceeds the campaign numeric range."));
					return false;
				}
			}
		}
		return true;
	}

	bool ValidateFacilityState(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		for (const FStrategicBaseState& Base : State.Bases)
		{
			if (!IsKnownWorksCadreCharter(Base.WorksCadreCharter))
			{
				AddError(Result, TEXT("invalid_works_cadre_charter"), FString::Printf(
					TEXT("Base '%s' has an unknown Works Charter."), *Base.Name));
				return false;
			}
			if (Base.SignalWatchScientists < 0 || Base.WorksCadreEngineers < 0
				|| Base.WorksCadreEngineers > WorksCadreMaximumEngineerCount)
			{
				AddError(Result, TEXT("invalid_staff_assignment"), FString::Printf(
					TEXT("Base '%s' has an invalid Signal Watch or Works Cadre assignment."), *Base.Name));
				return false;
			}
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
				if (Rule == nullptr)
				{
					AddError(Result, TEXT("unknown_facility"), FString::Printf(
						TEXT("Base '%s' references unloaded facility '%s'."),
						*Base.Name, *Facility.FacilityId.ToString()));
					return false;
				}
				if (Rule->MaxIntegrity <= 0 || Rule->RepairCostPerIntegrity < 0
					|| Rule->RepairHoursPerIntegrity <= 0
					|| Facility.Damage < 0 || Facility.Damage > Rule->MaxIntegrity
					|| Facility.ReservedRepairDamage < 0 || Facility.ReservedRepairDamage > Facility.Damage
					|| Facility.RemainingRepairSeconds < 0
					|| ((Facility.ReservedRepairDamage == 0) != (Facility.RemainingRepairSeconds == 0)))
				{
					AddError(Result, TEXT("invalid_facility_state"), FString::Printf(
						TEXT("Facility '%s' at base '%s' has invalid durability or repair state."),
						*Facility.FacilityId.ToString(), *Base.Name));
					return false;
				}
			}
		}
		return true;
	}

	bool ComputeMonthlyPersonnelSalaries(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		int64& OutSalaries,
		FStrategicCommandResult& Result)
	{
		OutSalaries = 0;
		for (const FPersonnelState& Person : State.Personnel)
		{
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId);
			if (Role == nullptr)
			{
				AddError(Result, TEXT("unknown_personnel_role"), FString::Printf(TEXT("Personnel '%s' references unloaded role '%s'."), *Person.DisplayName, *Person.RoleId.ToString()));
				return false;
			}
			if (!TryAdd(OutSalaries, Role->MonthlySalary, OutSalaries))
			{
				AddError(Result, TEXT("financial_overflow"), TEXT("Monthly personnel salaries exceed the campaign numeric range."));
				return false;
			}
		}
		return true;
	}

	bool ComputeBaseCraftCapacity(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		int32& OutCapacity,
		FStrategicCommandResult& Result)
	{
		int64 Capacity = 0;
		if (!Base.Facilities.IsEmpty())
		{
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
				if (Rule == nullptr || Rule->MaxIntegrity <= 0 || Facility.Damage < 0 || Facility.Damage > Rule->MaxIntegrity)
				{
					AddError(Result, TEXT("invalid_craft_capacity"), FString::Printf(
						TEXT("Base '%s' has an invalid facility durability state for '%s'."),
						*Base.Name, *Facility.FacilityId.ToString()));
					return false;
				}
				const int32 Contribution = Rule->ScaleEffectByIntegrity(Rule->CraftCapacity, Facility.Damage);
				if (!TryAdd(Capacity, Contribution, Capacity) || Capacity > MAX_int32)
				{
					AddError(Result, TEXT("craft_capacity_overflow"), FString::Printf(
						TEXT("Base '%s' craft capacity exceeds the supported range."), *Base.Name));
					return false;
				}
			}
		}
		else
		{
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
				if (Facility == nullptr || Facility->CraftCapacity < 0)
				{
					AddError(Result, TEXT("invalid_craft_capacity"), FString::Printf(TEXT("Base '%s' has an invalid craft-capacity facility '%s'."), *Base.Name, *FacilityId.ToString()));
					return false;
				}
				if (!TryAdd(Capacity, Facility->CraftCapacity, Capacity) || Capacity > MAX_int32)
				{
					AddError(Result, TEXT("craft_capacity_overflow"), FString::Printf(TEXT("Base '%s' craft capacity exceeds the supported range."), *Base.Name));
					return false;
				}
			}
		}
		OutCapacity = static_cast<int32>(Capacity);
		return true;
	}

	struct FBasePersonnelCapacityProfile
	{
		int32 BaseScientistCapacity = 0;
		int32 FacilityScientistCapacity = 0;
		int32 ScientistCapacity = 0;
		int32 BaseEngineerCapacity = 0;
		int32 FacilityEngineerCapacity = 0;
		int32 EngineerCapacity = 0;
	};

	bool ComputeBasePersonnelCapacities(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		FBasePersonnelCapacityProfile& OutProfile,
		FStrategicCommandResult& Result)
	{
		OutProfile = FBasePersonnelCapacityProfile();
		if (!IsKnownWorksCadreCharter(Base.WorksCadreCharter))
		{
			AddError(Result, TEXT("invalid_works_cadre_charter"), FString::Printf(
				TEXT("Base '%s' has an unknown Works Charter."), *Base.Name));
			return false;
		}
		if (Base.ScientistCapacity < 0 || Base.EngineerCapacity < 0
			|| Base.SignalWatchScientists < 0 || Base.WorksCadreEngineers < 0
			|| Base.WorksCadreEngineers > WorksCadreMaximumEngineerCount)
		{
			AddError(Result, TEXT("invalid_personnel_capacity"), FString::Printf(
				TEXT("Base '%s' has an invalid base-local personnel allowance or reserved-duty assignment."),
				*Base.Name));
			return false;
		}

		int64 FacilityScientists = 0;
		int64 FacilityEngineers = 0;
		const auto AddFacilityContribution = [&Base, &FacilityScientists, &FacilityEngineers, &Result](
			const FFacilityRule* Rule,
			const int32 Damage,
			const bool bValidateIntegrity)
		{
			if (Rule == nullptr || Rule->ScientistCapacity < 0 || Rule->ScientistCapacity > 1000000
				|| Rule->EngineerCapacity < 0 || Rule->EngineerCapacity > 1000000
				|| (bValidateIntegrity && (Rule->MaxIntegrity <= 0 || Damage < 0 || Damage > Rule->MaxIntegrity)))
			{
				AddError(Result, TEXT("invalid_personnel_capacity"), FString::Printf(
					TEXT("Base '%s' has an invalid personnel-capacity facility."), *Base.Name));
				return false;
			}
			const int32 Scientists = bValidateIntegrity
				? Rule->ScaleEffectByIntegrity(Rule->ScientistCapacity, Damage)
				: Rule->ScientistCapacity;
			const int32 Engineers = bValidateIntegrity
				? Rule->ScaleEffectByIntegrity(Rule->EngineerCapacity, Damage)
				: Rule->EngineerCapacity;
			if (!TryAdd(FacilityScientists, Scientists, FacilityScientists)
				|| !TryAdd(FacilityEngineers, Engineers, FacilityEngineers)
				|| FacilityScientists > MAX_int32 || FacilityEngineers > MAX_int32)
			{
				AddError(Result, TEXT("personnel_capacity_overflow"), FString::Printf(
					TEXT("Base '%s' facility personnel capacity exceeds the supported range."), *Base.Name));
				return false;
			}
			return true;
		};

		if (!Base.Facilities.IsEmpty())
		{
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				if (!AddFacilityContribution(Rules.Facilities.Find(Facility.FacilityId), Facility.Damage, true))
				{
					return false;
				}
			}
		}
		else
		{
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				if (!AddFacilityContribution(Rules.Facilities.Find(FacilityId), 0, false))
				{
					return false;
				}
			}
		}

		int64 TotalScientists = 0;
		int64 TotalEngineers = 0;
		if (!TryAdd(Base.ScientistCapacity, FacilityScientists, TotalScientists)
			|| !TryAdd(Base.EngineerCapacity, FacilityEngineers, TotalEngineers)
			|| TotalScientists > MAX_int32 || TotalEngineers > MAX_int32)
		{
			AddError(Result, TEXT("personnel_capacity_overflow"), FString::Printf(
				TEXT("Base '%s' total personnel capacity exceeds the supported range."), *Base.Name));
			return false;
		}

		OutProfile.BaseScientistCapacity = Base.ScientistCapacity;
		OutProfile.FacilityScientistCapacity = static_cast<int32>(FacilityScientists);
		OutProfile.ScientistCapacity = static_cast<int32>(TotalScientists);
		OutProfile.BaseEngineerCapacity = Base.EngineerCapacity;
		OutProfile.FacilityEngineerCapacity = static_cast<int32>(FacilityEngineers);
		OutProfile.EngineerCapacity = static_cast<int32>(TotalEngineers);
		return true;
	}

	bool ComputeBaseSensorProfile(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules,
		int32& OutRangeKilometers,
		int32& OutDetectionStrength,
		FStrategicCommandResult& Result)
	{
		OutRangeKilometers = 0;
		int64 DetectionStrength = 0;
		if (!Base.Facilities.IsEmpty())
		{
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
				if (Rule == nullptr || Rule->MaxIntegrity <= 0 || Facility.Damage < 0 || Facility.Damage > Rule->MaxIntegrity)
				{
					AddError(Result, TEXT("invalid_sensor_facility"), FString::Printf(
						TEXT("Base '%s' has an invalid facility durability state for '%s'."),
						*Base.Name, *Facility.FacilityId.ToString()));
					return false;
				}
				OutRangeKilometers = FMath::Max(OutRangeKilometers,
					Rule->ScaleEffectByIntegrity(Rule->SensorRangeKilometers, Facility.Damage));
				const int32 StrengthContribution =
					Rule->ScaleEffectByIntegrity(Rule->DetectionStrength, Facility.Damage);
				if (!TryAdd(DetectionStrength, StrengthContribution, DetectionStrength))
				{
					AddError(Result, TEXT("sensor_strength_overflow"), FString::Printf(
						TEXT("Base '%s' sensor strength exceeds the supported range."), *Base.Name));
					return false;
				}
			}
		}
		else
		{
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
				if (Facility == nullptr || Facility->SensorRangeKilometers < 0
					|| Facility->DetectionStrength < 0 || Facility->DetectionStrength > 100)
				{
					AddError(Result, TEXT("invalid_sensor_facility"), FString::Printf(TEXT("Base '%s' has invalid sensor facility '%s'."), *Base.Name, *FacilityId.ToString()));
					return false;
				}
				OutRangeKilometers = FMath::Max(OutRangeKilometers, Facility->SensorRangeKilometers);
				if (!TryAdd(DetectionStrength, Facility->DetectionStrength, DetectionStrength))
				{
					AddError(Result, TEXT("sensor_strength_overflow"), FString::Printf(TEXT("Base '%s' sensor strength exceeds the supported range."), *Base.Name));
					return false;
				}
			}
		}
		OutDetectionStrength = static_cast<int32>(FMath::Min<int64>(DetectionStrength, 100));
		return true;
	}

	bool ComputeMonthlyCraftMaintenance(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		int64& OutMaintenance,
		FStrategicCommandResult& Result)
	{
		OutMaintenance = 0;
		for (const FCraftState& Craft : State.Craft)
		{
			const FCraftRule* Rule = Rules.Craft.Find(Craft.CraftRuleId);
			if (Rule == nullptr)
			{
				AddError(Result, TEXT("unknown_craft_rule"), FString::Printf(TEXT("Craft '%s' references unloaded rule '%s'."), *Craft.DisplayName, *Craft.CraftRuleId.ToString()));
				return false;
			}
			if (!TryAdd(OutMaintenance, Rule->MonthlyMaintenance, OutMaintenance))
			{
				AddError(Result, TEXT("financial_overflow"), TEXT("Monthly craft maintenance exceeds the campaign numeric range."));
				return false;
			}
		}
		return true;
	}

	bool ValidatePersonnelState(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		TSet<FGuid> SeenPersonnelIds;
		TSet<FGuid> SeenOrderIds;
		TSet<FGuid> ActiveStewardBaseIds;
		TMap<FGuid, int32> CareerMissionCounts;
		for (const FPersonnelState& Person : State.Personnel)
		{
			TMap<FName, int32> DoctrineCounts;
			TSet<FName> SeenCommendations;
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId);
			const bool bStatusTimersValid =
				(Person.Status == EPersonnelStatus::Available && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds == 0 && Person.CurrentHealth == Person.MaxHealth)
				|| (Person.Status == EPersonnelStatus::Recovering && Person.RemainingRecoverySeconds > 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds == 0 && Person.CurrentHealth < Person.MaxHealth)
				|| (Person.Status == EPersonnelStatus::Training && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds > 0 && Person.RemainingStewardshipSeconds == 0 && Person.CurrentHealth == Person.MaxHealth)
				|| (Person.Status == EPersonnelStatus::Deployed && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds == 0)
				|| (Person.Status == EPersonnelStatus::Stewarding && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds > 0 && Person.CurrentHealth == Person.MaxHealth);
			const bool bRecoveryPlanValid = FPersonnelRecoveryPlan::IsKnown(Person.RecoveryPlan)
				&& (Person.Status == EPersonnelStatus::Recovering
					? Person.RecoveryPlan != EPersonnelRecoveryPlan::None
						|| Person.RemainingRecoverySeconds > 0
					: Person.RecoveryPlan == EPersonnelRecoveryPlan::None);
			const bool bStewardshipValid = FPersonnelStewardship::IsKnown(Person.StewardshipFocus)
				&& Person.StewardshipToursCompleted >= 0
				&& (Person.Status == EPersonnelStatus::Stewarding
					? FPersonnelStewardship::IsSelected(Person.StewardshipFocus)
						&& Role != nullptr && Role->Category == EPersonnelRoleCategory::FieldAgent
						&& !ActiveStewardBaseIds.Contains(Person.BaseId)
					: Person.StewardshipFocus == EPersonnelStewardshipFocus::None);
			if (!Person.PersonnelId.IsValid() || SeenPersonnelIds.Contains(Person.PersonnelId)
				|| !IsUsablePersonnelName(Person.DisplayName)
				|| Role == nullptr
				|| FindBase(State, Person.BaseId) == nullptr
				|| !IsValidPersonnelStatus(Person.Status)
				|| !IsValidTrainingFocus(Person.TrainingFocus)
				|| Person.Rank <= 0 || Person.Rank > 100 || Person.Missions < 0 || Person.Kills < 0 || Person.Experience < 0
				|| Person.PendingDoctrineChoices < 0
				|| static_cast<int64>(Person.PendingDoctrineChoices) + Person.DoctrineSelections.Num() > Person.Rank - 1
				|| Person.MaxHealth <= 0 || Person.MaxHealth > 200
				|| Person.CurrentHealth <= 0 || Person.CurrentHealth > Person.MaxHealth
				|| Person.Accuracy <= 0 || Person.Accuracy > 100
				|| Person.Resolve <= 0 || Person.Resolve > 100
				|| Person.Mobility <= 0 || Person.Mobility > 100
				|| Person.Strength <= 0 || Person.Strength > 100
				|| !bStatusTimersValid || !bRecoveryPlanValid || !bStewardshipValid)
			{
				AddError(Result, TEXT("invalid_personnel_state"), FString::Printf(TEXT("Personnel '%s' has invalid persisted identity, role, base, attributes, status, or timers."), *Person.DisplayName));
				return false;
			}
			for (const FName DoctrineId : Person.DoctrineSelections)
			{
				const FPersonnelDoctrineRule* Doctrine = Rules.PersonnelDoctrines.Find(DoctrineId);
				int32& SelectionCount = DoctrineCounts.FindOrAdd(DoctrineId);
				++SelectionCount;
				if (DoctrineId.IsNone() || Doctrine == nullptr || SelectionCount > Doctrine->MaxSelections)
				{
					AddError(Result, TEXT("invalid_personnel_doctrine"),
						FString::Printf(TEXT("Personnel '%s' has an invalid or over-selected field doctrine '%s'."),
							*Person.DisplayName, *DoctrineId.ToString()));
					return false;
				}
			}
			for (const FName CommendationId : Person.Commendations)
			{
				if (CommendationId.IsNone() || SeenCommendations.Contains(CommendationId)
					|| Rules.PersonnelCommendations.Find(CommendationId) == nullptr)
				{
					AddError(Result, TEXT("invalid_personnel_commendation"),
						FString::Printf(TEXT("Personnel '%s' has an invalid or duplicate commendation '%s'."),
							*Person.DisplayName, *CommendationId.ToString()));
					return false;
				}
				SeenCommendations.Add(CommendationId);
			}
			for (const FName ItemId : Person.EquippedItems)
			{
				const FItemRule* Item = Rules.Items.Find(ItemId);
				if (Item == nullptr || !IsEquippablePersonnelItem(*Item))
				{
					AddError(Result, TEXT("invalid_personnel_equipment"), FString::Printf(TEXT("Personnel '%s' references invalid equipped item '%s'."), *Person.DisplayName, *ItemId.ToString()));
					return false;
				}
			}
			SeenPersonnelIds.Add(Person.PersonnelId);
			if (Person.Status == EPersonnelStatus::Stewarding)
			{
				ActiveStewardBaseIds.Add(Person.BaseId);
			}
			CareerMissionCounts.Add(Person.PersonnelId, Person.Missions);
		}

		for (const FRecruitmentOrderState& Order : State.RecruitmentOrders)
		{
			if (!Order.OrderId.IsValid() || SeenOrderIds.Contains(Order.OrderId)
				|| !Order.PersonnelId.IsValid() || SeenPersonnelIds.Contains(Order.PersonnelId)
				|| !IsUsablePersonnelName(Order.DisplayName)
				|| Rules.PersonnelRoles.Find(Order.RoleId) == nullptr
				|| FindBase(State, Order.BaseId) == nullptr
				|| Order.RemainingTransitSeconds <= 0)
			{
				AddError(Result, TEXT("invalid_recruitment_order"), FString::Printf(TEXT("Recruitment order '%s' has invalid persisted identity, role, base, or transit time."), *Order.OrderId.ToString()));
				return false;
			}
			SeenOrderIds.Add(Order.OrderId);
			SeenPersonnelIds.Add(Order.PersonnelId);
		}

		for (const FMemorialRecord& Record : State.Memorial)
		{
			TMap<FName, int32> DoctrineCounts;
			TSet<FName> SeenCommendations;
			if (!Record.PersonnelId.IsValid() || SeenPersonnelIds.Contains(Record.PersonnelId)
				|| !IsUsablePersonnelName(Record.DisplayName)
				|| !FContentPackageResolver::IsValidPackageId(Record.RoleId)
				|| !FContentPackageResolver::IsValidPackageId(Record.CauseId)
				|| Record.Rank <= 0 || Record.Rank > 100 || Record.Missions < 0 || Record.Kills < 0
				|| Record.StewardshipToursCompleted < 0
				|| Record.DoctrineSelections.Num() > Record.Rank - 1
				|| Record.DeathUtc <= FDateTime::MinValue() || Record.DeathUtc > State.StrategicTime.Utc)
			{
				AddError(Result, TEXT("invalid_memorial_record"), FString::Printf(TEXT("Memorial record for '%s' has invalid persisted data."), *Record.DisplayName));
				return false;
			}
			for (const FName DoctrineId : Record.DoctrineSelections)
			{
				const FPersonnelDoctrineRule* Doctrine = Rules.PersonnelDoctrines.Find(DoctrineId);
				int32& SelectionCount = DoctrineCounts.FindOrAdd(DoctrineId);
				++SelectionCount;
				if (DoctrineId.IsNone() || Doctrine == nullptr || SelectionCount > Doctrine->MaxSelections)
				{
					AddError(Result, TEXT("invalid_memorial_doctrine"),
						FString::Printf(TEXT("Memorial record for '%s' has an invalid doctrine '%s'."),
							*Record.DisplayName, *DoctrineId.ToString()));
					return false;
				}
			}
			for (const FName CommendationId : Record.Commendations)
			{
				if (CommendationId.IsNone() || SeenCommendations.Contains(CommendationId)
					|| Rules.PersonnelCommendations.Find(CommendationId) == nullptr)
				{
					AddError(Result, TEXT("invalid_memorial_commendation"),
						FString::Printf(TEXT("Memorial record for '%s' has an invalid commendation '%s'."),
							*Record.DisplayName, *CommendationId.ToString()));
					return false;
				}
				SeenCommendations.Add(CommendationId);
			}
			SeenPersonnelIds.Add(Record.PersonnelId);
			CareerMissionCounts.Add(Record.PersonnelId, Record.Missions);
		}
		if (State.PersonnelSquadBonds.Num() > 10000)
		{
			AddError(Result, TEXT("invalid_personnel_squad_bond"),
				TEXT("The personnel squad-bond ledger exceeds its supported record count."));
			return false;
		}
		TSet<FString> SeenSquadBondPairs;
		for (const FPersonnelSquadBondState& Bond : State.PersonnelSquadBonds)
		{
			const FString FirstId = Bond.FirstPersonnelId.ToString(EGuidFormats::Digits);
			const FString SecondId = Bond.SecondPersonnelId.ToString(EGuidFormats::Digits);
			const FString PairKey = FirstId + TEXT(":") + SecondId;
			const int32* FirstMissions = CareerMissionCounts.Find(Bond.FirstPersonnelId);
			const int32* SecondMissions = CareerMissionCounts.Find(Bond.SecondPersonnelId);
			if (!Bond.FirstPersonnelId.IsValid() || !Bond.SecondPersonnelId.IsValid()
				|| Bond.FirstPersonnelId == Bond.SecondPersonnelId || !(FirstId < SecondId)
				|| FirstMissions == nullptr || SecondMissions == nullptr
				|| Bond.SharedVictories <= 0
				|| Bond.SharedVictories > FMath::Min(*FirstMissions, *SecondMissions)
				|| SeenSquadBondPairs.Contains(PairKey))
			{
				AddError(Result, TEXT("invalid_personnel_squad_bond"),
					TEXT("A personnel squad-bond record has invalid canonical identities, history, or shared victories."));
				return false;
			}
			SeenSquadBondPairs.Add(PairKey);
		}
		return true;
	}

	bool ValidateCraftState(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		TSet<FGuid> SeenCraftIds;
		TSet<FGuid> SeenOrderIds;
		TSet<FGuid> AssignedPilotIds;
		TSet<FGuid> AssignedAgentIds;
		TSet<FGuid> TargetedSiteIds;
		TMap<FGuid, int64> OccupiedBerths;
		for (const FCraftState& Craft : State.Craft)
		{
			const FCraftRule* Rule = Rules.Craft.Find(Craft.CraftRuleId);
			const FStrategicContactState* TargetContact = Craft.TargetContactId.IsValid()
				? FindContact(State, Craft.TargetContactId)
				: nullptr;
			const FStrategicSiteState* TargetSite = Craft.TargetSiteId.IsValid()
				? FindSite(State, Craft.TargetSiteId)
				: nullptr;
			const FTacticalOperationState* SiteOperation = State.TacticalOperations.FindByPredicate(
				[&Craft](const FTacticalOperationState& Operation)
				{
					return Operation.CraftId == Craft.CraftId && Operation.SiteId == Craft.TargetSiteId;
				});
			const bool bTimersValid = Rule != nullptr && (
				(Craft.Status == ECraftStatus::Grounded && Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
					|| (Craft.Status == ECraftStatus::Airborne && Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
					|| (Craft.Status == ECraftStatus::Intercepting && Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
					|| (Craft.Status == ECraftStatus::Returning && Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
					|| (Craft.Status == ECraftStatus::Deploying && Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
					|| (Craft.Status == ECraftStatus::OnSite && Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
					|| (Craft.Status == ECraftStatus::Servicing
					&& Craft.RemainingRepairSeconds >= 0 && Craft.RemainingRefuelSeconds >= 0
					&& (Craft.RemainingRepairSeconds > 0 || Craft.RemainingRefuelSeconds > 0)
					&& (Craft.RemainingRepairSeconds > 0 ? Craft.CurrentHull < Rule->MaxHull : Craft.CurrentHull == Rule->MaxHull)
					&& (Craft.RemainingRefuelSeconds > 0 ? Craft.CurrentFuel < Rule->FuelCapacity : Craft.CurrentFuel == Rule->FuelCapacity)));
			const bool bRouteValid =
				((Craft.Status == ECraftStatus::Grounded || Craft.Status == ECraftStatus::Servicing)
					&& !Craft.TargetContactId.IsValid() && !Craft.TargetSiteId.IsValid()
					&& Craft.RemainingRouteSeconds == 0 && Craft.ReservedReturnSeconds == 0)
				|| (Craft.Status == ECraftStatus::Airborne
					&& !Craft.TargetSiteId.IsValid()
					&& Craft.RemainingRouteSeconds == 0
					&& ((!Craft.TargetContactId.IsValid() && Craft.ReservedReturnSeconds == 0)
						|| (Craft.TargetContactId.IsValid() && Craft.ReservedReturnSeconds > 0
							&& TargetContact != nullptr && TargetContact->Status == EStrategicContactStatus::Engaged)))
				|| (Craft.Status == ECraftStatus::Intercepting
					&& Craft.TargetContactId.IsValid() && !Craft.TargetSiteId.IsValid()
					&& Craft.RemainingRouteSeconds > 0 && Craft.ReservedReturnSeconds > 0
					&& TargetContact != nullptr && TargetContact->Status != EStrategicContactStatus::Hidden)
				|| (Craft.Status == ECraftStatus::Returning
					&& !Craft.TargetContactId.IsValid() && !Craft.TargetSiteId.IsValid()
					&& Craft.RemainingRouteSeconds > 0 && Craft.ReservedReturnSeconds > 0)
				|| (Craft.Status == ECraftStatus::Deploying
					&& !Craft.TargetContactId.IsValid() && Craft.TargetSiteId.IsValid()
					&& Craft.RemainingRouteSeconds > 0 && Craft.ReservedReturnSeconds > 0
					&& TargetSite != nullptr && SiteOperation == nullptr)
				|| (Craft.Status == ECraftStatus::OnSite
					&& !Craft.TargetContactId.IsValid() && Craft.TargetSiteId.IsValid()
					&& Craft.RemainingRouteSeconds == 0 && Craft.ReservedReturnSeconds > 0
					&& TargetSite != nullptr && SiteOperation != nullptr);
			int64 CargoMass = 0;
			int64 PendingSalvageMass = 0;
			bool bPendingSalvageSubset = Craft.PendingSalvage.Num() <= 64;
			for (const FInventoryStack& Pending : Craft.PendingSalvage)
			{
				const FInventoryStack* CargoStack = Craft.Cargo.FindByPredicate(
					[&Pending](const FInventoryStack& Stack) { return Stack.ItemId == Pending.ItemId; });
				bPendingSalvageSubset &= CargoStack != nullptr && CargoStack->Quantity >= Pending.Quantity;
			}
			const bool bPendingSalvageStatus = Craft.PendingSalvage.IsEmpty()
				|| Craft.Status == ECraftStatus::OnSite
				|| Craft.Status == ECraftStatus::Returning
				|| Craft.Status == ECraftStatus::Grounded;
			const bool bCargoValid = Craft.Cargo.Num() <= 64
				&& TryComputeCargoMass(Craft.Cargo, Rules, CargoMass)
				&& TryComputeCargoMass(Craft.PendingSalvage, Rules, PendingSalvageMass)
				&& bPendingSalvageSubset && bPendingSalvageStatus
				&& Rule != nullptr && CargoMass <= Rule->CargoCapacity;
			if (!Craft.CraftId.IsValid() || SeenCraftIds.Contains(Craft.CraftId)
				|| !IsUsableCraftName(Craft.DisplayName)
				|| Rule == nullptr || FindBase(State, Craft.BaseId) == nullptr
				|| !IsValidCraftStatus(Craft.Status) || !bTimersValid || !bRouteValid
				|| Craft.CurrentHull <= 0 || Craft.CurrentHull > (Rule != nullptr ? Rule->MaxHull : 0)
				|| Craft.CurrentFuel < 0 || Craft.CurrentFuel > (Rule != nullptr ? Rule->FuelCapacity : -1)
				|| Craft.CompletedSorties < 0
				|| Craft.EquipmentItems.Num() > (Rule != nullptr ? Rule->EquipmentSlots : -1)
				|| Craft.AssignedAgentIds.Num() > (Rule != nullptr ? Rule->AgentCapacity : -1)
				|| !bCargoValid
				|| (Craft.TargetSiteId.IsValid() && TargetedSiteIds.Contains(Craft.TargetSiteId)))
			{
				AddError(Result, TEXT("invalid_craft_state"), FString::Printf(TEXT("Craft '%s' has invalid persisted identity, rule, base, status, condition, service timers, or equipment capacity."), *Craft.DisplayName));
				return false;
			}
			for (const FName ItemId : Craft.EquipmentItems)
			{
				const FItemRule* Item = Rules.Items.Find(ItemId);
				if (Item == nullptr || !IsEquippableCraftItem(*Item))
				{
					AddError(Result, TEXT("invalid_craft_equipment"), FString::Printf(TEXT("Craft '%s' references invalid equipment item '%s'."), *Craft.DisplayName, *ItemId.ToString()));
					return false;
				}
			}
			TSet<FName> SeenWeaponStates;
			for (const FCraftWeaponState& WeaponState : Craft.WeaponStates)
			{
				const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
				const int32 MountCount = CountEquippedItem(Craft.EquipmentItems, WeaponState.WeaponItemId);
				const int64 MaximumAmmunition = Weapon != nullptr
					? static_cast<int64>(Weapon->MagazineCapacity) * MountCount
					: -1;
				if (Weapon == nullptr || !IsValidCraftWeaponRule(*Weapon, Rules) || MountCount <= 0
					|| SeenWeaponStates.Contains(WeaponState.WeaponItemId)
					|| WeaponState.Ammunition < 0 || WeaponState.Ammunition > MaximumAmmunition
					|| WeaponState.RemainingCooldownSeconds < 0
					|| WeaponState.RemainingCooldownSeconds > Weapon->FireIntervalSeconds)
				{
					AddError(Result, TEXT("invalid_craft_weapon_state"), FString::Printf(TEXT("Craft '%s' has invalid persisted weapon state for '%s'."), *Craft.DisplayName, *WeaponState.WeaponItemId.ToString()));
					return false;
				}
				SeenWeaponStates.Add(WeaponState.WeaponItemId);
			}
			for (const FGuid& AgentId : Craft.AssignedAgentIds)
			{
				const FPersonnelState* Agent = FindPersonnel(State, AgentId);
				const FPersonnelRoleRule* AgentRole = Agent != nullptr ? Rules.PersonnelRoles.Find(Agent->RoleId) : nullptr;
				if (!AgentId.IsValid() || AssignedAgentIds.Contains(AgentId)
					|| Agent == nullptr || AgentRole == nullptr || AgentRole->Category != EPersonnelRoleCategory::FieldAgent
					|| Agent->BaseId != Craft.BaseId
					|| (IsFlightStatus(Craft.Status) && Agent->Status != EPersonnelStatus::Deployed)
					|| (!IsFlightStatus(Craft.Status) && Agent->Status != EPersonnelStatus::Available))
				{
					AddError(Result, TEXT("invalid_craft_agent"), FString::Printf(TEXT("Craft '%s' has an invalid or conflicting field-agent assignment."), *Craft.DisplayName));
					return false;
				}
				AssignedAgentIds.Add(AgentId);
			}
			if (Craft.AssignedPilotId.IsValid())
			{
				const FPersonnelState* Pilot = FindPersonnel(State, Craft.AssignedPilotId);
				const FPersonnelRoleRule* PilotRole = Pilot != nullptr ? Rules.PersonnelRoles.Find(Pilot->RoleId) : nullptr;
				if (Pilot == nullptr || PilotRole == nullptr || PilotRole->Category != EPersonnelRoleCategory::Pilot
					|| Pilot->BaseId != Craft.BaseId || AssignedPilotIds.Contains(Craft.AssignedPilotId)
					|| (IsFlightStatus(Craft.Status) && Pilot->Status != EPersonnelStatus::Deployed)
					|| (!IsFlightStatus(Craft.Status) && Pilot->Status == EPersonnelStatus::Deployed))
				{
					AddError(Result, TEXT("invalid_craft_pilot"), FString::Printf(TEXT("Craft '%s' has an invalid or conflicting pilot assignment."), *Craft.DisplayName));
					return false;
				}
				AssignedPilotIds.Add(Craft.AssignedPilotId);
			}
			else if (IsFlightStatus(Craft.Status))
			{
				AddError(Result, TEXT("invalid_craft_pilot"), FString::Printf(TEXT("Flying craft '%s' has no assigned pilot."), *Craft.DisplayName));
				return false;
			}
			SeenCraftIds.Add(Craft.CraftId);
			if (Craft.TargetSiteId.IsValid())
			{
				TargetedSiteIds.Add(Craft.TargetSiteId);
			}
			++OccupiedBerths.FindOrAdd(Craft.BaseId);
		}

		for (const FCraftAcquisitionOrderState& Order : State.CraftAcquisitionOrders)
		{
			if (!Order.OrderId.IsValid() || SeenOrderIds.Contains(Order.OrderId)
				|| !Order.CraftId.IsValid() || SeenCraftIds.Contains(Order.CraftId)
				|| !IsUsableCraftName(Order.DisplayName)
				|| Rules.Craft.Find(Order.CraftRuleId) == nullptr
				|| FindBase(State, Order.BaseId) == nullptr
				|| Order.RemainingTransitSeconds <= 0)
			{
				AddError(Result, TEXT("invalid_craft_acquisition"), FString::Printf(TEXT("Craft acquisition order '%s' has invalid persisted identity, rule, base, or transit time."), *Order.OrderId.ToString()));
				return false;
			}
			SeenOrderIds.Add(Order.OrderId);
			SeenCraftIds.Add(Order.CraftId);
			++OccupiedBerths.FindOrAdd(Order.BaseId);
		}

		for (const FStrategicBaseState& Base : State.Bases)
		{
			int32 Capacity = 0;
			if (!ComputeBaseCraftCapacity(Base, Rules, Capacity, Result))
			{
				return false;
			}
			if (OccupiedBerths.FindRef(Base.BaseId) > Capacity)
			{
				AddError(Result, TEXT("craft_capacity_exceeded"), FString::Printf(TEXT("Base '%s' has more active or incoming craft than operational berths."), *Base.Name));
				return false;
			}
		}
		return true;
	}

	bool ValidateStrategicContacts(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		TSet<FGuid> SeenContactIds;
		for (const FStrategicContactState& Contact : State.StrategicContacts)
		{
			const FContactRule* Rule = Rules.Contacts.Find(Contact.ContactRuleId);
			const FBaseAssaultState* PendingAssault = State.BaseAssaults.FindByPredicate(
				[&Contact](const FBaseAssaultState& Assault) { return Assault.ContactId == Contact.ContactId; });
			int64 ExpectedRouteSeconds = 0;
			const int64 RouteDistance = AreValidCoordinates(Contact.OriginLongitudeMilliDegrees, Contact.OriginLatitudeMilliDegrees)
				&& AreValidCoordinates(Contact.DestinationLongitudeMilliDegrees, Contact.DestinationLatitudeMilliDegrees)
				? ApproximateSurfaceDistanceKilometers(
					Contact.OriginLongitudeMilliDegrees,
					Contact.OriginLatitudeMilliDegrees,
					Contact.DestinationLongitudeMilliDegrees,
					Contact.DestinationLatitudeMilliDegrees)
				: -1;
			const bool bRouteTimeValid = Rule != nullptr
				&& RouteDistance > 0
				&& ComputeTravelSeconds(RouteDistance, Rule->CruiseSpeedKilometersPerHour, ExpectedRouteSeconds)
				&& Contact.TotalRouteSeconds == ExpectedRouteSeconds;
			int32 ExpectedLongitude = 0;
			int32 ExpectedLatitude = 0;
			if (bRouteTimeValid && Contact.ElapsedRouteSeconds >= 0 && Contact.ElapsedRouteSeconds <= Contact.TotalRouteSeconds)
			{
				ComputeContactPosition(Contact, Contact.ElapsedRouteSeconds, ExpectedLongitude, ExpectedLatitude);
			}
			const bool bHasOnStationCraft = State.Craft.ContainsByPredicate(
				[&Contact](const FCraftState& Craft)
				{
					return Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Contact.ContactId;
				});
			if (!Contact.ContactId.IsValid() || SeenContactIds.Contains(Contact.ContactId)
				|| Rule == nullptr || !IsValidContactStatus(Contact.Status)
				|| !AreValidCoordinates(Contact.OriginLongitudeMilliDegrees, Contact.OriginLatitudeMilliDegrees)
				|| !AreValidCoordinates(Contact.LongitudeMilliDegrees, Contact.LatitudeMilliDegrees)
				|| !AreValidCoordinates(Contact.DestinationLongitudeMilliDegrees, Contact.DestinationLatitudeMilliDegrees)
				|| !bRouteTimeValid
				|| Contact.ElapsedRouteSeconds < 0 || Contact.ElapsedRouteSeconds > Contact.TotalRouteSeconds
				|| ((Contact.ElapsedRouteSeconds == Contact.TotalRouteSeconds) != (PendingAssault != nullptr))
				|| Contact.LongitudeMilliDegrees != ExpectedLongitude || Contact.LatitudeMilliDegrees != ExpectedLatitude
				|| Contact.CurrentHull <= 0 || Contact.CurrentHull > (Rule != nullptr ? Rule->MaxHull : 0)
				|| Contact.CompletedCombatRounds < 0
				|| Contact.RemainingAttackCooldownSeconds < 0
				|| Contact.RemainingAttackCooldownSeconds > (Rule != nullptr ? Rule->AttackIntervalSeconds : -1)
				|| (PendingAssault != nullptr && Contact.Status != EStrategicContactStatus::Detected)
				|| (Contact.Status == EStrategicContactStatus::Engaged) != bHasOnStationCraft)
			{
				AddError(Result, TEXT("invalid_strategic_contact"), FString::Printf(TEXT("Strategic contact '%s' has invalid persisted identity, rule, route, condition, or engagement state."), *Contact.ContactId.ToString()));
				return false;
			}
			SeenContactIds.Add(Contact.ContactId);
		}
		return true;
	}

	bool ValidateStrategicSites(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		TSet<FGuid> SeenSiteIds;
		for (const FStrategicSiteState& Site : State.StrategicSites)
		{
			const FContactRule* SourceRule = Rules.Contacts.Find(Site.SourceContactRuleId);
			const bool bKnownType = Site.Type == EStrategicSiteType::Wreckage
				|| Site.Type == EStrategicSiteType::Landing;
			const bool bThreatMatchesType = SourceRule != nullptr
				&& (Site.Type == EStrategicSiteType::Wreckage
					? Site.ThreatRating == SourceRule->ThreatRating
					: Site.Type == EStrategicSiteType::Landing
						&& Site.ThreatRating > SourceRule->ThreatRating);
			if (!Site.SiteId.IsValid() || SeenSiteIds.Contains(Site.SiteId)
				|| FindContact(State, Site.SiteId) != nullptr
				|| !bKnownType
				|| SourceRule == nullptr
				|| !AreValidCoordinates(Site.LongitudeMilliDegrees, Site.LatitudeMilliDegrees)
				|| Site.ThreatRating <= 0 || Site.ThreatRating > 10
				|| !bThreatMatchesType
				|| Site.RemainingLifetimeSeconds <= 0)
			{
				AddError(Result, TEXT("invalid_strategic_site"), FString::Printf(TEXT("Strategic site '%s' has invalid persisted identity, source, type, coordinates, threat, or lifetime."), *Site.SiteId.ToString()));
				return false;
			}
			SeenSiteIds.Add(Site.SiteId);
		}
		return true;
	}

	bool ValidateTacticalOperations(
		const FCampaignState& State,
		FStrategicCommandResult& Result)
	{
		TSet<FGuid> SeenOperationIds;
		TSet<FGuid> SeenSiteIds;
		TSet<FGuid> SeenCraftIds;
		TSet<FGuid> SeenAssaultIds;
		TSet<FGuid> SeenAgentIds;
		for (const FTacticalOperationState& Operation : State.TacticalOperations)
		{
			const bool bSiteRecovery = Operation.Type == ETacticalOperationType::SiteRecovery;
			const bool bBaseDefense = Operation.Type == ETacticalOperationType::BaseDefense;
			const FCraftState* Craft = bSiteRecovery ? FindCraft(State, Operation.CraftId) : nullptr;
			const FStrategicSiteState* Site = bSiteRecovery ? FindSite(State, Operation.SiteId) : nullptr;
			const FStrategicBaseState* Base = bBaseDefense ? FindBase(State, Operation.BaseId) : nullptr;
			const FBaseAssaultState* Assault = bBaseDefense ? FindBaseAssault(State, Operation.AssaultId) : nullptr;
			bool bCargoMatches = Craft != nullptr && Operation.Cargo.Num() == Craft->Cargo.Num();
			if (bCargoMatches)
			{
				TSet<FName> OperationCargoItemIds;
				for (const FInventoryStack& Stack : Operation.Cargo)
				{
					if (OperationCargoItemIds.Contains(Stack.ItemId))
					{
						bCargoMatches = false;
						break;
					}
					const FInventoryStack* CraftStack = Craft->Cargo.FindByPredicate(
						[&Stack](const FInventoryStack& Entry) { return Entry.ItemId == Stack.ItemId; });
					if (CraftStack == nullptr || CraftStack->Quantity != Stack.Quantity)
					{
						bCargoMatches = false;
						break;
					}
					OperationCargoItemIds.Add(Stack.ItemId);
				}
			}
			bool bAgentsMatch = Craft != nullptr
				&& !Operation.AgentIds.IsEmpty()
				&& Operation.AgentIds.Num() == Craft->AssignedAgentIds.Num()
				&& Operation.AgentIds.ContainsByPredicate(
					[&Craft](const FGuid& AgentId) { return !Craft->AssignedAgentIds.Contains(AgentId); }) == false;
			bool bBaseAgentsValid = bBaseDefense && !Operation.AgentIds.IsEmpty();
			TSet<FGuid> OperationAgentIds;
			for (const FGuid& AgentId : Operation.AgentIds)
			{
				const FPersonnelState* Agent = FindPersonnel(State, AgentId);
				if (!AgentId.IsValid() || OperationAgentIds.Contains(AgentId) || SeenAgentIds.Contains(AgentId) || Agent == nullptr
					|| Agent->Status != EPersonnelStatus::Deployed
					|| (bBaseDefense && (Agent->BaseId != Operation.BaseId
						|| State.Craft.ContainsByPredicate([&AgentId](const FCraftState& Entry) { return Entry.AssignedAgentIds.Contains(AgentId); }))))
				{
					bBaseAgentsValid = false;
					bAgentsMatch = false;
					break;
				}
				OperationAgentIds.Add(AgentId);
			}
			const bool bContextValid =
				(bSiteRecovery
					&& Operation.SiteId.IsValid() && !SeenSiteIds.Contains(Operation.SiteId)
					&& Operation.CraftId.IsValid() && !SeenCraftIds.Contains(Operation.CraftId)
					&& !Operation.BaseId.IsValid() && !Operation.AssaultId.IsValid()
					&& Craft != nullptr && Site != nullptr
					&& Craft->Status == ECraftStatus::OnSite && Craft->TargetSiteId == Operation.SiteId
					&& bAgentsMatch && bCargoMatches)
				|| (bBaseDefense
					&& !Operation.SiteId.IsValid() && !Operation.CraftId.IsValid()
					&& Operation.BaseId.IsValid() && Operation.AssaultId.IsValid() && !SeenAssaultIds.Contains(Operation.AssaultId)
					&& Base != nullptr && Assault != nullptr && Assault->BaseId == Operation.BaseId
					&& Operation.Cargo.IsEmpty() && bBaseAgentsValid);
			if (!Operation.OperationId.IsValid() || SeenOperationIds.Contains(Operation.OperationId)
				|| Operation.CreatedUtc <= FDateTime::MinValue() || Operation.CreatedUtc > State.StrategicTime.Utc
				|| !bContextValid)
			{
				AddError(Result, TEXT("invalid_tactical_operation"), FString::Printf(TEXT("Tactical operation '%s' has invalid identity, context, strategic links, roster, cargo, or creation time."), *Operation.OperationId.ToString()));
				return false;
			}
			SeenOperationIds.Add(Operation.OperationId);
			for (const FGuid& AgentId : Operation.AgentIds)
			{
				SeenAgentIds.Add(AgentId);
			}
			if (bSiteRecovery)
			{
				SeenSiteIds.Add(Operation.SiteId);
				SeenCraftIds.Add(Operation.CraftId);
			}
			else
			{
				SeenAssaultIds.Add(Operation.AssaultId);
			}
		}
		return true;
	}

	bool ValidateTacticalBattles(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		TSet<FGuid> SeenBattleIds;
		TSet<FGuid> SeenOperationIds;
		for (const FTacticalBattleState& Battle : State.TacticalBattles)
		{
			if (!Battle.BattleId.IsValid() || SeenBattleIds.Contains(Battle.BattleId)
				|| !Battle.OperationId.IsValid() || SeenOperationIds.Contains(Battle.OperationId))
			{
				AddError(Result, TEXT("invalid_tactical_battle"), TEXT("Tactical battle identities and operation links must be valid and unique."));
				return false;
			}
			TArray<FTacticalGenerationDiagnostic> Diagnostics;
			if (!FTacticalMissionGenerator::ValidateBattle(Battle, State, Rules, Diagnostics))
			{
				for (const FTacticalGenerationDiagnostic& Diagnostic : Diagnostics)
				{
					AddError(Result, Diagnostic.Code, Diagnostic.Message);
				}
				return false;
			}
			SeenBattleIds.Add(Battle.BattleId);
			SeenOperationIds.Add(Battle.OperationId);
		}
		return true;
	}

	bool RefreshAndValidateTacticalBattles(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FStrategicCommandResult& Result)
	{
		for (FTacticalBattleState& Battle : State.TacticalBattles)
		{
			const FTacticalVisibilityResult Discovery = FTacticalNavigationService::RefreshPlayerDiscovery(Battle, Rules);
			if (!Discovery.bSucceeded)
			{
				for (const FTacticalNavigationDiagnostic& Diagnostic : Discovery.Diagnostics)
				{
					AddError(Result, Diagnostic.Code, Diagnostic.Message);
				}
				return false;
			}
		}
		return ValidateTacticalBattles(State, Rules, Result);
	}

	bool ValidateAdversaryState(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		FStrategicCommandResult& Result)
	{
		if (!IsValidCampaignOutcome(State.Outcome)
			|| State.AdversaryEscalationLevel <= 0 || State.AdversaryEscalationLevel > Config.MaxAdversaryEscalation
			|| State.NextAdversaryMissionSeconds < 0
			|| (State.Outcome == ECampaignOutcome::Ongoing && !Rules.AdversaryMissions.IsEmpty() && State.NextAdversaryMissionSeconds <= 0)
			|| State.NextAdversaryMissionSerial <= 0 || State.NextAdversaryMissionSerial == MAX_int64
			|| State.AdversaryMissionsLaunched < 0
			|| State.AdversaryMissionsEscaped < 0
			|| State.AdversaryMissionsThwarted < 0
			|| (State.Outcome == ECampaignOutcome::Ongoing && !State.OutcomeReasonId.IsNone())
			|| (State.Outcome != ECampaignOutcome::Ongoing && !FContentPackageResolver::IsValidPackageId(State.OutcomeReasonId)))
		{
			AddError(Result, TEXT("invalid_adversary_state"), TEXT("Campaign adversary cadence, escalation, counters, or outcome metadata is invalid."));
			return false;
		}
		const int64 ResolvedMissionCount = static_cast<int64>(State.AdversaryMissions.Num())
			+ State.AdversaryMissionsEscaped + State.AdversaryMissionsThwarted;
		if (ResolvedMissionCount != State.AdversaryMissionsLaunched
			|| State.AdversaryMissions.Num() > Config.MaxActiveAdversaryMissions)
		{
			AddError(Result, TEXT("invalid_adversary_mission_count"), TEXT("Launched, active, escaped, and thwarted adversary mission counters are inconsistent."));
			return false;
		}

		TSet<FName> SeenRegions;
		for (const FRegionalPressureState& Pressure : State.RegionalPressure)
		{
			if (!FContentPackageResolver::IsValidPackageId(Pressure.RegionId)
				|| SeenRegions.Contains(Pressure.RegionId)
				|| Pressure.Pressure < 0 || Pressure.Pressure > 100)
			{
				AddError(Result, TEXT("invalid_regional_pressure"), FString::Printf(TEXT("Region '%s' has invalid or duplicate pressure state."), *Pressure.RegionId.ToString()));
				return false;
			}
			SeenRegions.Add(Pressure.RegionId);
		}
		int32 SignedCharterCount = 0;
		if (!State.RegionalMandates.IsEmpty())
		{
			TSet<FName> SeenMandateRegions;
			int64 RegionalFunding = 0;
			const int32 CurrentMonth = GetDiplomaticMonthSerial(State.StrategicTime.Utc);
			for (const FRegionalMandateState& Mandate : State.RegionalMandates)
			{
				SignedCharterCount += Mandate.bResilienceCharterSigned ? 1 : 0;
				int64 UpdatedFunding = 0;
				if (!FContentPackageResolver::IsValidPackageId(Mandate.RegionId)
					|| SeenMandateRegions.Contains(Mandate.RegionId)
					|| !SeenRegions.Contains(Mandate.RegionId)
					|| (!Rules.Regions.IsEmpty() && !Rules.Regions.Contains(Mandate.RegionId))
					|| Mandate.Support < 0 || Mandate.Support > 100
					|| Mandate.BaselineMonthlyFunding < 0 || Mandate.CurrentMonthlyFunding < 0
					|| Mandate.LastDiplomaticActionMonth < 0
					|| Mandate.LastDiplomaticActionMonth > CurrentMonth
					|| (Mandate.bHorizonCompactMemberWithdrawn
						&& (!State.bHorizonCompactRatified || !Mandate.bResilienceCharterSigned))
					|| !TryAdd(RegionalFunding, Mandate.CurrentMonthlyFunding, UpdatedFunding))
				{
					AddError(Result, TEXT("invalid_regional_mandate"), FString::Printf(
						TEXT("Region '%s' has invalid or duplicate mandate support, funding, outreach, or coalition state."),
						*Mandate.RegionId.ToString()));
					return false;
				}
				RegionalFunding = UpdatedFunding;
				SeenMandateRegions.Add(Mandate.RegionId);
			}
			if (SeenMandateRegions.Num() != SeenRegions.Num() || RegionalFunding != State.MonthlyFunding)
			{
				AddError(Result, TEXT("invalid_regional_mandate"),
					TEXT("Regional mandates must cover every pressure region and sum to recurring campaign funding."));
				return false;
			}
		}
		if (State.bHorizonCompactRatified
			&& SignedCharterCount < Config.HorizonCompactRequiredCharters)
		{
			AddError(Result, TEXT("invalid_coalition_compact_state"),
				TEXT("A ratified Horizon Compact requires its configured number of signed regional charters."));
			return false;
		}
		const int32 CurrentCoalitionMonth = GetDiplomaticMonthSerial(State.StrategicTime.Utc);
		if (State.LastCoalitionAidMonth < 0
			|| State.LastCoalitionAidMonth > CurrentCoalitionMonth
			|| (!State.bHorizonCompactRatified && State.LastCoalitionAidMonth != 0))
		{
			AddError(Result, TEXT("invalid_coalition_aid_state"),
				TEXT("Reciprocal Aid history must belong to a ratified compact and cannot be dated after the campaign month."));
			return false;
		}
		if (State.LastCoalitionEmergencyVoteMonth < 0
			|| State.LastCoalitionEmergencyVoteMonth > CurrentCoalitionMonth
			|| (!State.bHorizonCompactRatified && State.LastCoalitionEmergencyVoteMonth != 0))
		{
			AddError(Result, TEXT("invalid_coalition_emergency_vote_state"),
				TEXT("Emergency solidarity vote history must belong to a ratified compact and cannot be dated after the campaign month."));
			return false;
		}

		TSet<FGuid> SeenMissionIds;
		TSet<FGuid> SeenMissionContactIds;
		for (const FAdversaryMissionState& Mission : State.AdversaryMissions)
		{
			const FAdversaryMissionRule* MissionRule = Rules.AdversaryMissions.Find(Mission.MissionRuleId);
			const FStrategicContactState* Contact = FindContact(State, Mission.ContactId);
			const FStrategicBaseState* TargetBase = Mission.TargetBaseId.IsValid()
				? FindBase(State, Mission.TargetBaseId)
				: nullptr;
			if (!Mission.MissionId.IsValid() || SeenMissionIds.Contains(Mission.MissionId)
				|| !Mission.ContactId.IsValid() || SeenMissionContactIds.Contains(Mission.ContactId)
				|| Mission.MissionId == Mission.ContactId
				|| MissionRule == nullptr || Contact == nullptr
				|| (MissionRule != nullptr && Contact != nullptr && MissionRule->ContactRuleId != Contact->ContactRuleId)
				|| (MissionRule != nullptr && MissionRule->bTargetsPlayerBase != Mission.TargetBaseId.IsValid())
				|| (Mission.TargetBaseId.IsValid() && TargetBase == nullptr)
				|| (TargetBase != nullptr && Contact != nullptr
					&& (Contact->DestinationLongitudeMilliDegrees != TargetBase->LongitudeMilliDegrees
						|| Contact->DestinationLatitudeMilliDegrees != TargetBase->LatitudeMilliDegrees))
				|| Mission.StartedUtc <= FDateTime::MinValue() || Mission.StartedUtc > State.StrategicTime.Utc)
			{
				AddError(Result, TEXT("invalid_adversary_mission"), FString::Printf(TEXT("Adversary mission '%s' has invalid identity, rule, contact, or start time."), *Mission.MissionId.ToString()));
				return false;
			}
			SeenMissionIds.Add(Mission.MissionId);
			SeenMissionContactIds.Add(Mission.ContactId);
		}

		TSet<FGuid> SeenAssaultIds;
		TSet<FGuid> AssaultMissionIds;
		TSet<FGuid> AssaultContactIds;
		for (const FBaseAssaultState& Assault : State.BaseAssaults)
		{
			const FAdversaryMissionState* Mission = FindAdversaryMissionById(State, Assault.MissionId);
			const FStrategicContactState* Contact = FindContact(State, Assault.ContactId);
			const FStrategicBaseState* Base = FindBase(State, Assault.BaseId);
			const FAdversaryMissionRule* MissionRule = Mission != nullptr
				? Rules.AdversaryMissions.Find(Mission->MissionRuleId)
				: nullptr;
			if (!Assault.AssaultId.IsValid() || SeenAssaultIds.Contains(Assault.AssaultId)
				|| !Assault.MissionId.IsValid() || AssaultMissionIds.Contains(Assault.MissionId)
				|| !Assault.ContactId.IsValid() || AssaultContactIds.Contains(Assault.ContactId)
				|| !Assault.BaseId.IsValid() || Mission == nullptr || Contact == nullptr || Base == nullptr
				|| MissionRule == nullptr || (MissionRule != nullptr && !MissionRule->bTargetsPlayerBase)
				|| (Mission != nullptr && (Mission->ContactId != Assault.ContactId || Mission->TargetBaseId != Assault.BaseId))
				|| (Contact != nullptr && (Contact->Status != EStrategicContactStatus::Detected
					|| Contact->ElapsedRouteSeconds != Contact->TotalRouteSeconds
					|| Contact->LongitudeMilliDegrees != Contact->DestinationLongitudeMilliDegrees
					|| Contact->LatitudeMilliDegrees != Contact->DestinationLatitudeMilliDegrees))
				|| Assault.ArrivedUtc <= FDateTime::MinValue() || Assault.ArrivedUtc > State.StrategicTime.Utc
				|| (Mission != nullptr && Assault.ArrivedUtc < Mission->StartedUtc))
			{
				AddError(Result, TEXT("invalid_base_assault"), FString::Printf(
					TEXT("Base assault '%s' has invalid mission, contact, target, or arrival state."), *Assault.AssaultId.ToString()));
				return false;
			}
			SeenAssaultIds.Add(Assault.AssaultId);
			AssaultMissionIds.Add(Assault.MissionId);
			AssaultContactIds.Add(Assault.ContactId);
		}

		if (State.Outcome == ECampaignOutcome::Failure)
		{
			const bool bFailureThresholdReached = State.RegionalPressure.ContainsByPredicate(
				[&Config](const FRegionalPressureState& Pressure)
				{
					return Pressure.Pressure >= Config.FailurePressureThreshold;
				});
			if (!bFailureThresholdReached)
			{
				AddError(Result, TEXT("invalid_campaign_outcome"), TEXT("Failed campaign has no region at the configured pressure threshold."));
				return false;
			}
		}
		else if (State.Outcome == ECampaignOutcome::Victory
			&& State.AdversaryMissionsThwarted < Config.VictoryThwartedMissions)
		{
			AddError(Result, TEXT("invalid_campaign_outcome"), TEXT("Victorious campaign has not reached the configured thwarted-mission target."));
			return false;
		}
		return true;
	}

	bool ReviewRegionalMandates(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		FStrategicCommandResult& Result,
		const int64 CommandSequence,
		const FDateTime& TimestampUtc)
	{
		if (State.RegionalMandates.IsEmpty())
		{
			return true;
		}

		int64 ReviewedFunding = 0;
		for (FRegionalMandateState& Mandate : State.RegionalMandates)
		{
			const FRegionalPressureState* Pressure = FindRegionalPressure(State, Mandate.RegionId);
			if (Pressure == nullptr)
			{
				return false;
			}
			const FStrategicRegionRule* RegionRule = Rules.Regions.Find(Mandate.RegionId);
			const int32 PressureTolerance = RegionRule != nullptr ? RegionRule->PressureTolerance : 50;
			const int32 LowPressureRecovery = RegionRule != nullptr ? RegionRule->LowPressureSupportRecovery : 2;
			const int32 HighPressureLossPerTen = RegionRule != nullptr ? RegionRule->HighPressureSupportLossPerTen : 1;

			const int32 OldSupport = Mandate.Support;
			const int64 OldContribution = Mandate.CurrentMonthlyFunding;
			if (Pressure->Pressure > PressureTolerance)
			{
				const int32 PressureBands = (Pressure->Pressure - PressureTolerance + 9) / 10;
				Mandate.Support = FMath::Max(0, Mandate.Support - PressureBands * HighPressureLossPerTen);
			}
			else if (Pressure->Pressure <= PressureTolerance / 2)
			{
				Mandate.Support = FMath::Min(100, Mandate.Support + LowPressureRecovery);
			}
			const bool bWithdrewFromCompact = Mandate.Support < OldSupport
				&& WithdrawHorizonCompactMemberIfRequired(State, Mandate, Config);

			if (!FStrategicCommandService::CalculateRegionalFundingContribution(
					Mandate, Config, State.bHorizonCompactRatified, Mandate.CurrentMonthlyFunding))
			{
				return false;
			}
			int64 UpdatedFunding = 0;
			if (!TryAdd(ReviewedFunding, Mandate.CurrentMonthlyFunding, UpdatedFunding))
			{
				return false;
			}
			ReviewedFunding = UpdatedFunding;

			if (Mandate.Support != OldSupport)
			{
				FStrategicEvent& SupportChanged = AddEvent(Result, EStrategicEventType::RegionalSupportChanged,
					CommandSequence, TimestampUtc);
				SupportChanged.RuleId = Mandate.RegionId;
				SupportChanged.RegionId = Mandate.RegionId;
				SupportChanged.Amount = Mandate.Support - OldSupport;
				SupportChanged.Quantity = Mandate.Support;
			}
			if (bWithdrewFromCompact)
			{
				AddHorizonCompactWithdrawalEvent(
					Result, Mandate, Config, CommandSequence, TimestampUtc);
			}
			if (Mandate.CurrentMonthlyFunding != OldContribution)
			{
				FStrategicEvent& FundingChanged = AddEvent(Result, EStrategicEventType::RegionalFundingChanged,
					CommandSequence, TimestampUtc);
				FundingChanged.RuleId = Mandate.RegionId;
				FundingChanged.RegionId = Mandate.RegionId;
				FundingChanged.Amount = Mandate.CurrentMonthlyFunding - OldContribution;
				FundingChanged.Quantity = FStrategicCommandService::GetRegionalFundingPercent(Mandate.Support);
			}
			FStrategicEvent& Reviewed = AddEvent(Result, EStrategicEventType::RegionalMandateReviewed,
				CommandSequence, TimestampUtc);
			Reviewed.RuleId = Mandate.RegionId;
			Reviewed.RegionId = Mandate.RegionId;
			Reviewed.Amount = Mandate.CurrentMonthlyFunding;
			Reviewed.Quantity = Mandate.Support;
		}
		State.MonthlyFunding = ReviewedFunding;
		return true;
	}

	bool CountPersonnelForCategory(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FGuid& BaseId,
		const EPersonnelRoleCategory Category,
		int32& OutCount)
	{
		OutCount = 0;
		for (const FPersonnelState& Person : State.Personnel)
		{
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId);
			if (Role == nullptr)
			{
				return false;
			}
			if (Person.BaseId == BaseId && Role->Category == Category)
			{
				if (OutCount == MAX_int32)
				{
					return false;
				}
				++OutCount;
			}
		}
		for (const FRecruitmentOrderState& Order : State.RecruitmentOrders)
		{
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Order.RoleId);
			if (Role == nullptr)
			{
				return false;
			}
			if (Order.BaseId == BaseId && Role->Category == Category)
			{
				if (OutCount == MAX_int32)
				{
					return false;
				}
				++OutCount;
			}
		}
		return true;
	}

	bool WouldViolateStaffingCommitmentAfterRelease(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FGuid& BaseId,
		const EPersonnelRoleCategory Category)
	{
		if (Category != EPersonnelRoleCategory::Scientist && Category != EPersonnelRoleCategory::Engineer)
		{
			return false;
		}
		int64 PersonnelAtBase = 0;
		for (const FPersonnelState& Person : State.Personnel)
		{
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId);
			if (Person.BaseId == BaseId && Role != nullptr && Role->Category == Category)
			{
				++PersonnelAtBase;
			}
		}
		int64 AssignedAtBase = 0;
		if (Category == EPersonnelRoleCategory::Scientist)
		{
			if (const FStrategicBaseState* Base = FindBase(State, BaseId))
			{
				AssignedAtBase += FMath::Max(0, Base->SignalWatchScientists);
			}
			for (const FResearchProjectState& Project : State.ResearchProjects)
			{
				if (Project.BaseId == BaseId)
				{
					AssignedAtBase += Project.AssignedScientists;
				}
			}
		}
		else
		{
			if (const FStrategicBaseState* Base = FindBase(State, BaseId))
			{
				AssignedAtBase += FMath::Max(0, Base->WorksCadreEngineers);
			}
			for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
			{
				if (Project.BaseId == BaseId)
				{
					AssignedAtBase += Project.AssignedEngineers;
				}
			}
		}
		return PersonnelAtBase - 1 < AssignedAtBase;
	}
}

bool FStrategicCommandResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FStrategicCommandDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FStrategicCommandResult::HasEvent(const EStrategicEventType Type) const
{
	return Events.ContainsByPredicate([Type](const FStrategicEvent& Event) { return Event.Type == Type; });
}

FName FStrategicCommandService::WorksCadrePolicyId()
{
	return TEXT("facilities.works-cadre");
}

int32 FStrategicCommandService::WorksCadreMaximumEngineers()
{
	return StrategicCommandServicePrivate::WorksCadreMaximumEngineerCount;
}

TArray<FWorksCadreCharterPolicy> FStrategicCommandService::GetWorksCadreCharterPolicies()
{
	return {
		GetWorksCadreCharterPolicy(EWorksCadreCharter::CommonCadence),
		GetWorksCadreCharterPolicy(EWorksCadreCharter::AssemblyCadence),
		GetWorksCadreCharterPolicy(EWorksCadreCharter::RestorationCadence)
	};
}

FWorksCadreCharterPolicy FStrategicCommandService::GetWorksCadreCharterPolicy(
	const EWorksCadreCharter Charter)
{
	using namespace StrategicCommandServicePrivate;

	FWorksCadreCharterPolicy Policy;
	Policy.Charter = Charter;
	switch (Charter)
	{
	case EWorksCadreCharter::CommonCadence:
		Policy.PolicyId = TEXT("facilities.works-charter-common-cadence");
		Policy.ConstructionFrontloadPercentPerEngineer = WorksCadreCommonPercentEach;
		Policy.RepairFrontloadPercentPerEngineer = WorksCadreCommonPercentEach;
		break;
	case EWorksCadreCharter::AssemblyCadence:
		Policy.PolicyId = TEXT("facilities.works-charter-assembly-cadence");
		Policy.ConstructionFrontloadPercentPerEngineer =
			WorksCadreSpecializedPrimaryPercentEach;
		Policy.RepairFrontloadPercentPerEngineer =
			WorksCadreSpecializedSecondaryPercentEach;
		break;
	case EWorksCadreCharter::RestorationCadence:
		Policy.PolicyId = TEXT("facilities.works-charter-restoration-cadence");
		Policy.ConstructionFrontloadPercentPerEngineer =
			WorksCadreSpecializedSecondaryPercentEach;
		Policy.RepairFrontloadPercentPerEngineer =
			WorksCadreSpecializedPrimaryPercentEach;
		break;
	default:
		break;
	}
	return Policy;
}

FInterceptionPosturePolicy FStrategicCommandService::GetInterceptionPosturePolicy(
	const EInterceptionPosture Posture)
{
	FInterceptionPosturePolicy Policy;
	Policy.Posture = Posture;
	switch (Posture)
	{
	case EInterceptionPosture::StandOffScreen:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("interception.stand-off-screen");
		Policy.OutgoingAccuracyModifier = -20;
		Policy.IncomingAccuracyModifier = -25;
		break;
	case EInterceptionPosture::BalancedVector:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("interception.balanced-vector");
		break;
	case EInterceptionPosture::CloseAssault:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("interception.close-assault");
		Policy.OutgoingAccuracyModifier = 20;
		Policy.IncomingAccuracyModifier = 25;
		break;
	default:
		break;
	}
	return Policy;
}

FInterceptionCoordinationPolicy FStrategicCommandService::EvaluateInterceptionCoordination(
	const FCampaignState& State,
	const FGuid ContactId)
{
	FInterceptionCoordinationPolicy Policy;
	for (const FCraftState& Craft : State.Craft)
	{
		if (Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == ContactId)
		{
			++Policy.OnStationCraftCount;
		}
	}
	if (Policy.OnStationCraftCount <= 0)
	{
		return Policy;
	}

	Policy.bValid = true;
	Policy.SupportingCraftCount = Policy.OnStationCraftCount - 1;
	if (Policy.SupportingCraftCount == 0)
	{
		Policy.PolicyId = TEXT("interception.coordination-solo-vector");
		return Policy;
	}

	Policy.bActive = true;
	Policy.PolicyId = TEXT("interception.coordination-linked-wing");
	const int32 CoordinationSteps = FMath::Min(Policy.SupportingCraftCount, 3);
	Policy.OutgoingAccuracyModifier = CoordinationSteps * 5;
	Policy.IncomingAccuracyModifier = CoordinationSteps * -5;
	return Policy;
}

FInterceptionContactManeuverPolicy FStrategicCommandService::EvaluateInterceptionContactManeuver(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FGuid ContactId)
{
	using namespace StrategicCommandServicePrivate;

	FInterceptionContactManeuverPolicy Policy;
	const FStrategicContactState* Contact = FindContact(State, ContactId);
	if (Contact == nullptr || Contact->CurrentHull <= 0 || Contact->CompletedCombatRounds < 0)
	{
		return Policy;
	}
	const FContactRule* ContactRule = Rules.Contacts.Find(Contact->ContactRuleId);
	if (ContactRule == nullptr || ContactRule->MaxHull <= 0 || Contact->CurrentHull > ContactRule->MaxHull)
	{
		return Policy;
	}

	Policy.bValid = true;
	Policy.CompletedCombatRounds = Contact->CompletedCombatRounds;
	Policy.CurrentHull = Contact->CurrentHull;
	Policy.MaximumHull = ContactRule->MaxHull;
	const bool bAtBreakline = static_cast<int64>(Contact->CurrentHull) * 100LL
		<= static_cast<int64>(ContactRule->MaxHull) * 35LL;
	if (bAtBreakline)
	{
		Policy.Maneuver = EInterceptionContactManeuver::BreaklineCounter;
		Policy.PolicyId = TEXT("interception.contact-breakline-counter");
		Policy.OutgoingAccuracyModifier = 10;
		Policy.IncomingAccuracyModifier = 20;
	}
	else if (Contact->CompletedCombatRounds >= 2)
	{
		Policy.Maneuver = EInterceptionContactManeuver::SignalShear;
		Policy.PolicyId = TEXT("interception.contact-signal-shear");
		Policy.OutgoingAccuracyModifier = -10;
		Policy.IncomingAccuracyModifier = -15;
	}
	else
	{
		Policy.Maneuver = EInterceptionContactManeuver::VectorSurvey;
		Policy.PolicyId = TEXT("interception.contact-vector-survey");
	}
	return Policy;
}

FInterceptionWithdrawalPolicy FStrategicCommandService::GetInterceptionWithdrawalPolicy(
	const EInterceptionWithdrawalDoctrine Doctrine)
{
	FInterceptionWithdrawalPolicy Policy;
	Policy.Doctrine = Doctrine;
	switch (Doctrine)
	{
	case EInterceptionWithdrawalDoctrine::FormationBreak:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("interception.withdrawal-formation-break");
		Policy.bWithdrawEntireFormation = true;
		break;
	case EInterceptionWithdrawalDoctrine::EvasiveRelay:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("interception.withdrawal-evasive-relay");
		break;
	case EInterceptionWithdrawalDoctrine::WakeSnare:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("interception.withdrawal-wake-snare");
		Policy.bWithdrawEntireFormation = true;
		Policy.bDelaysContactRoute = true;
		Policy.RequiredCombatRounds = 2;
		Policy.MaximumContactRouteDelaySeconds = 30 * 60;
		break;
	default:
		break;
	}
	return Policy;
}

FInterceptionWithdrawalEvaluation FStrategicCommandService::EvaluateInterceptionWithdrawal(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FWithdrawInterceptionCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FInterceptionWithdrawalEvaluation Evaluation;
	Evaluation.Doctrine = Command.Doctrine;
	const FInterceptionWithdrawalPolicy Policy = GetInterceptionWithdrawalPolicy(Command.Doctrine);
	Evaluation.PolicyId = Policy.PolicyId;
	auto Reject = [&Evaluation](const FName Code, const FString& Message)
	{
		Evaluation.UnavailableReasonCode = Code;
		Evaluation.UnavailableReason = Message;
	};
	if (!Policy.bValid)
	{
		Reject(TEXT("invalid_interception_withdrawal_doctrine"),
			TEXT("The selected interception withdrawal doctrine is not supported."));
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		Reject(TEXT("campaign_concluded"),
			TEXT("An interception formation cannot withdraw after the campaign has concluded."));
		return Evaluation;
	}
	const FStrategicContactState* Contact = FindContact(State, Command.ContactId);
	if (Contact == nullptr)
	{
		Reject(TEXT("unknown_contact"), TEXT("Strategic contact does not exist."));
		return Evaluation;
	}
	if (Contact->Status != EStrategicContactStatus::Engaged)
	{
		Reject(TEXT("contact_not_engaged"),
			TEXT("Interception withdrawal requires an engaged contact with craft on station."));
		return Evaluation;
	}
	Evaluation.CompletedCombatRounds = Contact->CompletedCombatRounds;
	Evaluation.RequiredCombatRounds = Policy.RequiredCombatRounds;
	if (Policy.bDelaysContactRoute)
	{
		Evaluation.ContactRouteDelaySeconds = FMath::Min(
			Contact->ElapsedRouteSeconds,
			Policy.MaximumContactRouteDelaySeconds);
	}

	TArray<const FCraftState*> Participants;
	for (const FCraftState& Craft : State.Craft)
	{
		if (Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Command.ContactId)
		{
			Participants.Add(&Craft);
		}
	}
	Participants.Sort(
		[](const FCraftState& Left, const FCraftState& Right)
		{
			return Left.CraftId.ToString(EGuidFormats::Digits)
				< Right.CraftId.ToString(EGuidFormats::Digits);
		});
	Evaluation.OnStationCraftCount = Participants.Num();
	if (Participants.IsEmpty())
	{
		Reject(TEXT("contact_not_engaged"), TEXT("Engaged contact has no on-station craft."));
		return Evaluation;
	}
	if (Policy.bDelaysContactRoute && Policy.bWithdrawEntireFormation)
	{
		Evaluation.WithdrawingCraftCount = Evaluation.OnStationCraftCount;
		Evaluation.RemainingCraftCount = 0;
	}
	if (Policy.bDelaysContactRoute
		&& Evaluation.CompletedCombatRounds < Evaluation.RequiredCombatRounds)
	{
		Reject(TEXT("interception_wake_snare_rounds_required"),
			TEXT("Wake Snare requires two completed combat rounds."));
		return Evaluation;
	}
	if (Policy.bDelaysContactRoute && Evaluation.ContactRouteDelaySeconds <= 0)
	{
		Reject(TEXT("interception_wake_snare_no_route_progress"),
			TEXT("Wake Snare requires recorded contact route progress to rewind."));
		return Evaluation;
	}

	if (Policy.bWithdrawEntireFormation)
	{
		for (const FCraftState* Craft : Participants)
		{
			check(Craft != nullptr);
			if (Craft->ReservedReturnSeconds <= 0)
			{
				Reject(TEXT("invalid_craft_route"),
					TEXT("An on-station craft has no valid return route."));
				return Evaluation;
			}
			Evaluation.WithdrawingCraftIds.Add(Craft->CraftId);
		}
	}
	else
	{
		const FCraftState* PriorityCraft = nullptr;
		int32 PriorityMaximumHull = 0;
		for (const FCraftState* Craft : Participants)
		{
			check(Craft != nullptr);
			const FCraftRule* CraftRule = Rules.Craft.Find(Craft->CraftRuleId);
			if (CraftRule == nullptr || CraftRule->MaxHull <= 0)
			{
				Reject(TEXT("invalid_craft_rule"),
					TEXT("An on-station craft has no valid authored hull profile."));
				return Evaluation;
			}
			if (Craft->CurrentHull <= 0 || Craft->CurrentHull > CraftRule->MaxHull)
			{
				Reject(TEXT("invalid_craft_state"),
					TEXT("An on-station craft has invalid hull integrity."));
				return Evaluation;
			}
			if (PriorityCraft == nullptr
				|| static_cast<int64>(Craft->CurrentHull) * PriorityMaximumHull
					< static_cast<int64>(PriorityCraft->CurrentHull) * CraftRule->MaxHull)
			{
				PriorityCraft = Craft;
				PriorityMaximumHull = CraftRule->MaxHull;
			}
		}
		check(PriorityCraft != nullptr);
		if (PriorityCraft->ReservedReturnSeconds <= 0)
		{
			Reject(TEXT("invalid_craft_route"),
				TEXT("The priority craft has no valid return route."));
			return Evaluation;
		}
		Evaluation.PriorityCraftId = PriorityCraft->CraftId;
		Evaluation.PriorityCraftCurrentHull = PriorityCraft->CurrentHull;
		Evaluation.PriorityCraftMaximumHull = PriorityMaximumHull;
		Evaluation.WithdrawingCraftIds.Add(PriorityCraft->CraftId);
	}

	Evaluation.WithdrawingCraftCount = Evaluation.WithdrawingCraftIds.Num();
	Evaluation.RemainingCraftCount =
		Evaluation.OnStationCraftCount - Evaluation.WithdrawingCraftCount;
	Evaluation.bCanExecute = Evaluation.WithdrawingCraftCount > 0;
	return Evaluation;
}

FBaseDefenseFireDoctrinePolicy FStrategicCommandService::GetBaseDefenseFireDoctrinePolicy(
	const EBaseDefenseFireDoctrine Doctrine)
{
	FBaseDefenseFireDoctrinePolicy Policy;
	Policy.Doctrine = Doctrine;
	switch (Doctrine)
	{
	case EBaseDefenseFireDoctrine::CoordinatedLine:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("base-defense.coordinated-line");
		break;
	case EBaseDefenseFireDoctrine::PrecisionScreen:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("base-defense.precision-screen");
		break;
	case EBaseDefenseFireDoctrine::BreachBreaker:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("base-defense.breach-breaker");
		break;
	case EBaseDefenseFireDoctrine::GridOvercharge:
		Policy.bValid = true;
		Policy.PolicyId = TEXT("base-defense.grid-overcharge");
		break;
	default:
		break;
	}
	return Policy;
}

bool FStrategicCommandService::GetAdversaryDifficultyTuning(
	const ECampaignDifficulty Difficulty,
	const FStrategicSimulationConfig& Config,
	FAdversaryDifficultyTuning& OutTuning)
{
	using namespace StrategicCommandServicePrivate;

	OutTuning = FAdversaryDifficultyTuning();
	if (!HasValidAdversaryDifficultyTuning(Config))
	{
		return false;
	}
	switch (Difficulty)
	{
	case ECampaignDifficulty::Cadet:
		OutTuning.MissionIntervalPercent = Config.CadetAdversaryIntervalPercent;
		OutTuning.EscapeConsequencePercent = Config.CadetAdversaryConsequencePercent;
		break;
	case ECampaignDifficulty::Standard:
		OutTuning.MissionIntervalPercent = Config.StandardAdversaryIntervalPercent;
		OutTuning.EscapeConsequencePercent = Config.StandardAdversaryConsequencePercent;
		break;
	case ECampaignDifficulty::Veteran:
		OutTuning.MissionIntervalPercent = Config.VeteranAdversaryIntervalPercent;
		OutTuning.EscapeConsequencePercent = Config.VeteranAdversaryConsequencePercent;
		break;
	case ECampaignDifficulty::Apex:
		OutTuning.MissionIntervalPercent = Config.ApexAdversaryIntervalPercent;
		OutTuning.EscapeConsequencePercent = Config.ApexAdversaryConsequencePercent;
		break;
	default:
		return false;
	}
	return true;
}

bool FStrategicCommandService::GetAdversaryAdaptationProgress(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	FAdversaryAdaptationProgress& OutProgress)
{
	OutProgress = FAdversaryAdaptationProgress();
	if (State.AdversaryMissionsEscaped < 0
		|| State.AdversaryMissionsThwarted < 0
		|| State.AdversaryEscalationLevel <= 0
		|| Config.ResolvedMissionsPerEscalationLevel <= 0
		|| Config.ResolvedMissionsPerEscalationLevel > 1000
		|| Config.MaxAdversaryEscalation <= 0
		|| Config.MaxAdversaryEscalation > 10
		|| State.AdversaryEscalationLevel > Config.MaxAdversaryEscalation)
	{
		return false;
	}

	OutProgress.ResolvedMissions = static_cast<int64>(State.AdversaryMissionsEscaped)
		+ State.AdversaryMissionsThwarted;
	const int64 AdaptationFloor = 1
		+ OutProgress.ResolvedMissions / Config.ResolvedMissionsPerEscalationLevel;
	OutProgress.EscalationFloor = static_cast<int32>(FMath::Min<int64>(
		AdaptationFloor, Config.MaxAdversaryEscalation));
	const int32 EffectiveEscalation = FMath::Max(
		State.AdversaryEscalationLevel, OutProgress.EscalationFloor);
	OutProgress.bAtMaximumEscalation = EffectiveEscalation >= Config.MaxAdversaryEscalation;
	if (!OutProgress.bAtMaximumEscalation)
	{
		const int64 NextThreshold = static_cast<int64>(EffectiveEscalation)
			* Config.ResolvedMissionsPerEscalationLevel;
		OutProgress.ResolvedMissionsUntilNextEscalation = FMath::Max<int64>(
			0, NextThreshold - OutProgress.ResolvedMissions);
	}
	return true;
}

bool FStrategicCommandService::ScaleAdversaryIntervalSeconds(
	const int64 BaseSeconds,
	const ECampaignDifficulty Difficulty,
	const FStrategicSimulationConfig& Config,
	int64& OutSeconds)
{
	OutSeconds = 0;
	FAdversaryDifficultyTuning Tuning;
	return BaseSeconds > 0
		&& GetAdversaryDifficultyTuning(Difficulty, Config, Tuning)
		&& StrategicCommandServicePrivate::TryScaleNonNegativeByPercent(
			BaseSeconds, Tuning.MissionIntervalPercent, OutSeconds)
		&& OutSeconds > 0;
}

bool FStrategicCommandService::ScaleAdversaryEscapeConsequence(
	const int64 BaseValue,
	const ECampaignDifficulty Difficulty,
	const FStrategicSimulationConfig& Config,
	int64& OutValue)
{
	OutValue = 0;
	FAdversaryDifficultyTuning Tuning;
	return GetAdversaryDifficultyTuning(Difficulty, Config, Tuning)
		&& StrategicCommandServicePrivate::TryScaleNonNegativeByPercent(
			BaseValue, Tuning.EscapeConsequencePercent, OutValue);
}

bool FStrategicCommandService::CalculateInterceptionAftershockSeconds(
	const int32 ContactThreatRating,
	const FStrategicSimulationConfig& Config,
	int64& OutSeconds)
{
	OutSeconds = 0;
	if (ContactThreatRating <= 0 || ContactThreatRating > 10
		|| Config.InterceptionAftershockMinutesPerThreat < 0
		|| Config.InterceptionAftershockMinutesPerThreat > 360)
	{
		return false;
	}
	int64 DelayMinutes = 0;
	return StrategicCommandServicePrivate::TryMultiplyNonNegative(
		ContactThreatRating,
		Config.InterceptionAftershockMinutesPerThreat,
		DelayMinutes)
		&& StrategicCommandServicePrivate::TryMultiplyNonNegative(
			DelayMinutes, 60, OutSeconds);
}

int32 FStrategicCommandService::GetRegionalFundingPercent(const int32 Support)
{
	if (Support < 0 || Support > 100 || Support < 15)
	{
		return 0;
	}
	if (Support < 40)
	{
		return 75;
	}
	if (Support < 75)
	{
		return 100;
	}
	return 110;
}

bool FStrategicCommandService::CalculateRegionalFundingContribution(
	const int64 BaselineMonthlyFunding,
	const int32 Support,
	int64& OutContribution)
{
	using namespace StrategicCommandServicePrivate;

	OutContribution = 0;
	if (BaselineMonthlyFunding < 0 || Support < 0 || Support > 100)
	{
		return false;
	}
	const int32 FundingPercent = GetRegionalFundingPercent(Support);
	return FundingPercent == 0
		|| TryScaleNonNegativeByPercent(BaselineMonthlyFunding, FundingPercent, OutContribution);
}

bool FStrategicCommandService::CalculateRegionalFundingContribution(
	const FRegionalMandateState& Mandate,
	const FStrategicSimulationConfig& Config,
	int64& OutContribution)
{
	return CalculateRegionalFundingContribution(Mandate, Config, false, OutContribution);
}

bool FStrategicCommandService::CalculateRegionalFundingContribution(
	const FRegionalMandateState& Mandate,
	const FStrategicSimulationConfig& Config,
	const bool bHorizonCompactRatified,
	int64& OutContribution)
{
	using namespace StrategicCommandServicePrivate;

	int64 TierContribution = 0;
	if (!CalculateRegionalFundingContribution(
			Mandate.BaselineMonthlyFunding, Mandate.Support, TierContribution))
	{
		OutContribution = 0;
		return false;
	}
	if (!Mandate.bResilienceCharterSigned)
	{
		OutContribution = TierContribution;
		return true;
	}
	const bool bReceivesCompactFunding = bHorizonCompactRatified
		&& !Mandate.bHorizonCompactMemberWithdrawn;
	const int32 FundingPercent = bReceivesCompactFunding
		? Config.HorizonCompactFundingPercent
		: Config.ResilienceCharterFundingPercent;
	return HasValidResilienceCharterConfig(Config)
		&& (!bReceivesCompactFunding || HasValidHorizonCompactConfig(Config))
		&& TryScaleNonNegativeByPercent(
			TierContribution, FundingPercent, OutContribution);
}

FRegionalDiplomacyEvaluation FStrategicCommandService::EvaluateRegionalDiplomacy(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FRegionalDiplomacyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FRegionalDiplomacyEvaluation Evaluation;
	Evaluation.RegionId = Command.RegionId;
	Evaluation.ActionType = Command.ActionType;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidateAdversaryConfig(Config, Validation))
	{
		return Reject();
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_finished"), TEXT("Regional outreach is unavailable after the campaign has ended."));
		return Reject();
	}
	switch (Command.ActionType)
	{
	case ERegionalDiplomacyActionType::CivicRelief:
		Evaluation.Cost = Config.CivicReliefCost;
		Evaluation.SupportDelta = Config.CivicReliefSupportGain;
		Evaluation.PressureReduction = Config.CivicReliefPressureReduction;
		break;
	case ERegionalDiplomacyActionType::SecurityAccord:
		Evaluation.Cost = Config.SecurityAccordCost;
		Evaluation.SupportDelta = Config.SecurityAccordSupportGain;
		Evaluation.PressureReduction = Config.SecurityAccordPressureReduction;
		break;
	case ERegionalDiplomacyActionType::CrisisMobilization:
		Evaluation.SupportDelta = -Config.CrisisMobilizationSupportCost;
		Evaluation.PressureReduction = Config.CrisisMobilizationPressureReduction;
		Evaluation.MinimumPressure = Config.CrisisMobilizationMinimumPressure;
		break;
	default:
		AddError(Validation, TEXT("invalid_regional_action"), TEXT("Regional outreach action is unsupported."));
		return Reject();
	}
	const FRegionalMandateState* Mandate = FindRegionalMandate(State, Command.RegionId);
	const FRegionalPressureState* Pressure = FindRegionalPressure(State, Command.RegionId);
	if (Mandate == nullptr || Pressure == nullptr)
	{
		AddError(Validation, TEXT("unknown_regional_mandate"), TEXT("Regional outreach requires an active mandate partner."));
		return Reject();
	}
	if (Mandate->LastDiplomaticActionMonth == GetDiplomaticMonthSerial(State.StrategicTime.Utc))
	{
		AddError(Validation, TEXT("regional_action_already_used"), TEXT("This regional partner has already received outreach this month."));
		return Reject();
	}
	if (Command.ActionType == ERegionalDiplomacyActionType::CrisisMobilization
		&& Pressure->Pressure < Evaluation.MinimumPressure)
	{
		AddError(Validation, TEXT("regional_crisis_not_severe"),
			TEXT("Regional pressure has not reached the configured crisis threshold."));
		return Reject();
	}
	if (Command.ActionType == ERegionalDiplomacyActionType::CrisisMobilization
		&& Mandate->Support < -Evaluation.SupportDelta)
	{
		AddError(Validation, TEXT("insufficient_crisis_mobilization_support"),
			TEXT("This partner lacks the support required for crisis mobilization."));
		return Reject();
	}
	if (State.Funds < Evaluation.Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("Regional outreach requires %lld funds, but only %lld are available."), Evaluation.Cost, State.Funds));
		return Reject();
	}
	if (Mandate->Support >= 100 && Pressure->Pressure <= 0)
	{
		AddError(Validation, TEXT("regional_action_no_effect"), TEXT("Regional support and pressure are already at their best supported values."));
		return Reject();
	}
	Evaluation.bWouldWithdrawCompactMember = Evaluation.SupportDelta < 0
		&& IsActiveHorizonCompactMember(State, *Mandate)
		&& Mandate->Support + Evaluation.SupportDelta
			< Config.HorizonCompactWithdrawalSupportThreshold;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FRegionalCharterEvaluation FStrategicCommandService::EvaluateRegionalCharter(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FSignRegionalCharterCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FRegionalCharterEvaluation Evaluation;
	Evaluation.RegionId = Command.RegionId;
	Evaluation.Cost = Config.ResilienceCharterCost;
	Evaluation.SupportCost = Config.ResilienceCharterSupportCost;
	Evaluation.MinimumSupport = Config.ResilienceCharterMinimumSupport;
	Evaluation.FundingPercent = Config.ResilienceCharterFundingPercent;
	Evaluation.MissionWeightPercent = Config.ResilienceCharterMissionWeightPercent;
	Evaluation.EscapePressurePercent = Config.ResilienceCharterEscapePressurePercent;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidateAdversaryConfig(Config, Validation))
	{
		return Reject();
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_finished"),
			TEXT("Regional charters are unavailable after the campaign has ended."));
		return Reject();
	}
	const FRegionalMandateState* Mandate = FindRegionalMandate(State, Command.RegionId);
	if (Mandate == nullptr || FindRegionalPressure(State, Command.RegionId) == nullptr)
	{
		AddError(Validation, TEXT("unknown_regional_mandate"),
			TEXT("A Resilience Charter requires an active regional mandate partner."));
		return Reject();
	}
	Evaluation.bSigned = Mandate->bResilienceCharterSigned;
	Evaluation.ProjectedMonthlyFunding = Mandate->CurrentMonthlyFunding;
	if (Evaluation.bSigned)
	{
		AddError(Validation, TEXT("regional_charter_already_signed"),
			TEXT("This regional partner has already signed the Resilience Charter."));
		return Reject();
	}
	if (Mandate->Support < Evaluation.MinimumSupport)
	{
		AddError(Validation, TEXT("regional_charter_support_required"),
			TEXT("This regional partner has not reached the support required for a Resilience Charter."));
		return Reject();
	}
	if (State.Funds < Evaluation.Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("The Resilience Charter requires %lld funds, but only %lld are available."),
			Evaluation.Cost, State.Funds));
		return Reject();
	}
	FRegionalMandateState CharteredMandate = *Mandate;
	CharteredMandate.Support -= Evaluation.SupportCost;
	CharteredMandate.bResilienceCharterSigned = true;
	if (!CalculateRegionalFundingContribution(
			CharteredMandate, Config, State.bHorizonCompactRatified,
			Evaluation.ProjectedMonthlyFunding))
	{
		AddError(Validation, TEXT("invalid_regional_charter_config"),
			TEXT("The Resilience Charter funding projection could not be represented safely."));
		return Reject();
	}
	Evaluation.MonthlyFundingDelta =
		Evaluation.ProjectedMonthlyFunding - Mandate->CurrentMonthlyFunding;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FHorizonCompactEvaluation FStrategicCommandService::EvaluateHorizonCompact(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FRatifyHorizonCompactCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FHorizonCompactEvaluation Evaluation;
	Evaluation.bRatified = State.bHorizonCompactRatified;
	Evaluation.Cost = Config.HorizonCompactCost;
	Evaluation.RequiredCharters = Config.HorizonCompactRequiredCharters;
	Evaluation.MinimumMemberSupport = Config.HorizonCompactMinimumMemberSupport;
	Evaluation.MemberSupportCost = Config.HorizonCompactMemberSupportCost;
	Evaluation.FundingPercent = Config.HorizonCompactFundingPercent;
	Evaluation.SharedEscapePressurePercent = Config.HorizonCompactSharedEscapePressurePercent;
	Evaluation.WithdrawalSupportThreshold = Config.HorizonCompactWithdrawalSupportThreshold;
	Evaluation.RestorationMinimumSupport = Config.HorizonCompactRestorationMinimumSupport;
	Evaluation.CurrentMonthlyFunding = State.MonthlyFunding;
	Evaluation.ProjectedMonthlyFunding = State.MonthlyFunding;
	for (const FRegionalMandateState& Mandate : State.RegionalMandates)
	{
		if (Mandate.bResilienceCharterSigned)
		{
			Evaluation.MemberRegionIds.Add(Mandate.RegionId);
			if (State.bHorizonCompactRatified && Mandate.bHorizonCompactMemberWithdrawn)
			{
				Evaluation.WithdrawnMemberRegionIds.Add(Mandate.RegionId);
			}
			else
			{
				Evaluation.ActiveMemberRegionIds.Add(Mandate.RegionId);
			}
		}
	}
	Evaluation.MemberRegionIds.Sort(FNameLexicalLess());
	Evaluation.ActiveMemberRegionIds.Sort(FNameLexicalLess());
	Evaluation.WithdrawnMemberRegionIds.Sort(FNameLexicalLess());
	Evaluation.SignedCharters = Evaluation.MemberRegionIds.Num();

	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidateAdversaryConfig(Config, Validation))
	{
		return Reject();
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_finished"),
			TEXT("The Horizon Compact is unavailable after the campaign has ended."));
		return Reject();
	}
	if (Evaluation.bRatified)
	{
		AddError(Validation, TEXT("coalition_compact_already_ratified"),
			TEXT("The Horizon Compact has already been ratified."));
		return Reject();
	}
	if (Evaluation.SignedCharters < Evaluation.RequiredCharters)
	{
		AddError(Validation, TEXT("coalition_compact_charters_required"),
			TEXT("The Horizon Compact requires more signed Resilience Charters."));
		return Reject();
	}
	for (const FName RegionId : Evaluation.MemberRegionIds)
	{
		const FRegionalMandateState* Mandate = FindRegionalMandate(State, RegionId);
		if (Mandate == nullptr || Mandate->Support < Evaluation.MinimumMemberSupport)
		{
			AddError(Validation, TEXT("coalition_compact_support_required"),
				TEXT("Every current charter partner must meet the support required for the Horizon Compact."));
			return Reject();
		}
	}
	if (State.Funds < Evaluation.Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("The Horizon Compact requires %lld funds, but only %lld are available."),
			Evaluation.Cost, State.Funds));
		return Reject();
	}

	Evaluation.ProjectedMonthlyFunding = 0;
	for (const FRegionalMandateState& ExistingMandate : State.RegionalMandates)
	{
		FRegionalMandateState ProjectedMandate = ExistingMandate;
		if (ProjectedMandate.bResilienceCharterSigned)
		{
			ProjectedMandate.Support -= Evaluation.MemberSupportCost;
		}
		int64 ProjectedContribution = 0;
		int64 UpdatedProjection = 0;
		if (!CalculateRegionalFundingContribution(
				ProjectedMandate, Config, true, ProjectedContribution)
			|| !TryAdd(
				Evaluation.ProjectedMonthlyFunding, ProjectedContribution,
				UpdatedProjection))
		{
			AddError(Validation, TEXT("financial_overflow"),
				TEXT("The Horizon Compact funding projection exceeds the campaign numeric range."));
			return Reject();
		}
		Evaluation.ProjectedMonthlyFunding = UpdatedProjection;
	}
	Evaluation.MonthlyFundingDelta =
		Evaluation.ProjectedMonthlyFunding - Evaluation.CurrentMonthlyFunding;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FReciprocalAidEvaluation FStrategicCommandService::EvaluateReciprocalAid(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FDeployReciprocalAidCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FReciprocalAidEvaluation Evaluation;
	Evaluation.TargetRegionId = Command.TargetRegionId;
	Evaluation.Cost = Config.ReciprocalAidCost;
	Evaluation.MinimumTargetPressure = Config.ReciprocalAidMinimumTargetPressure;
	Evaluation.MaximumPressureTransfer = Config.ReciprocalAidPressureTransfer;
	Evaluation.DonorSupportCost = Config.ReciprocalAidSupportTransfer;
	Evaluation.CurrentMonthlyFunding = State.MonthlyFunding;
	Evaluation.ProjectedMonthlyFunding = State.MonthlyFunding;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidateAdversaryConfig(Config, Validation))
	{
		return Reject();
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_finished"),
			TEXT("Reciprocal Aid is unavailable after the campaign has ended."));
		return Reject();
	}
	if (!State.bHorizonCompactRatified)
	{
		AddError(Validation, TEXT("coalition_aid_compact_required"),
			TEXT("Reciprocal Aid requires a ratified Horizon Compact."));
		return Reject();
	}
	const int32 CurrentMonth = GetDiplomaticMonthSerial(State.StrategicTime.Utc);
	if (State.LastCoalitionAidMonth < 0 || State.LastCoalitionAidMonth > CurrentMonth)
	{
		AddError(Validation, TEXT("invalid_coalition_aid_state"),
			TEXT("Reciprocal Aid history cannot be dated after the campaign month."));
		return Reject();
	}
	if (State.LastCoalitionAidMonth == CurrentMonth)
	{
		AddError(Validation, TEXT("coalition_aid_already_used"),
			TEXT("The Horizon Compact has already deployed Reciprocal Aid this month."));
		return Reject();
	}

	const FRegionalMandateState* TargetMandate = FindRegionalMandate(State, Command.TargetRegionId);
	const FRegionalPressureState* TargetPressure = FindRegionalPressure(State, Command.TargetRegionId);
	if (TargetMandate == nullptr || TargetPressure == nullptr
		|| !TargetMandate->bResilienceCharterSigned)
	{
		AddError(Validation, TEXT("coalition_aid_target_not_member"),
			TEXT("Reciprocal Aid must target a signed Horizon Compact member."));
		return Reject();
	}
	if (TargetMandate->bHorizonCompactMemberWithdrawn)
	{
		AddError(Validation, TEXT("coalition_aid_target_withdrawn"),
			TEXT("Reciprocal Aid cannot target a member that has withdrawn from the Horizon Compact."));
		return Reject();
	}
	Evaluation.TargetCurrentPressure = TargetPressure->Pressure;
	Evaluation.TargetProjectedPressure = TargetPressure->Pressure;
	Evaluation.TargetProjectedSupport = TargetMandate->Support;
	if (TargetPressure->Pressure < Evaluation.MinimumTargetPressure)
	{
		AddError(Validation, TEXT("coalition_aid_crisis_required"),
			TEXT("The requested compact member has not reached the Reciprocal Aid crisis threshold."));
		return Reject();
	}

	const FRegionalPressureState* DonorPressure =
		FindHorizonCompactPressureRecipient(State, Command.TargetRegionId);
	const FRegionalMandateState* DonorMandate = DonorPressure != nullptr
		? FindRegionalMandate(State, DonorPressure->RegionId)
		: nullptr;
	if (DonorPressure == nullptr || DonorMandate == nullptr
		|| !DonorMandate->bResilienceCharterSigned)
	{
		AddError(Validation, TEXT("coalition_aid_partner_required"),
			TEXT("Reciprocal Aid requires another signed compact member to accept the transferred pressure."));
		return Reject();
	}
	Evaluation.DonorRegionId = DonorPressure->RegionId;
	Evaluation.DonorCurrentPressure = DonorPressure->Pressure;
	Evaluation.DonorProjectedPressure = DonorPressure->Pressure;
	Evaluation.DonorProjectedSupport = DonorMandate->Support;
	if (DonorMandate->Support < Evaluation.DonorSupportCost)
	{
		AddError(Validation, TEXT("coalition_aid_partner_support_required"),
			TEXT("The least-strained compact member lacks the support required to accept Reciprocal Aid pressure."));
		return Reject();
	}

	Evaluation.PressureTransfer = FMath::Min3(
		Config.ReciprocalAidPressureTransfer,
		TargetPressure->Pressure,
		100 - DonorPressure->Pressure);
	if (Evaluation.PressureTransfer <= 0)
	{
		AddError(Validation, TEXT("coalition_aid_no_capacity"),
			TEXT("No other compact member has capacity to accept additional pressure."));
		return Reject();
	}
	if (State.Funds < Evaluation.Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("Reciprocal Aid requires %lld funds, but only %lld are available."),
			Evaluation.Cost, State.Funds));
		return Reject();
	}

	FRegionalMandateState ProjectedTarget = *TargetMandate;
	FRegionalMandateState ProjectedDonor = *DonorMandate;
	ProjectedTarget.Support = FMath::Min(100,
		ProjectedTarget.Support + Config.ReciprocalAidSupportTransfer);
	ProjectedDonor.Support -= Evaluation.DonorSupportCost;
	Evaluation.bDonorWouldWithdraw =
		ProjectedDonor.Support < Config.HorizonCompactWithdrawalSupportThreshold;
	if (Evaluation.bDonorWouldWithdraw)
	{
		ProjectedDonor.bHorizonCompactMemberWithdrawn = true;
	}
	Evaluation.TargetSupportGain = ProjectedTarget.Support - TargetMandate->Support;
	Evaluation.TargetProjectedSupport = ProjectedTarget.Support;
	Evaluation.DonorProjectedSupport = ProjectedDonor.Support;
	Evaluation.TargetProjectedPressure -= Evaluation.PressureTransfer;
	Evaluation.DonorProjectedPressure += Evaluation.PressureTransfer;
	int64 ProjectedTargetFunding = 0;
	int64 ProjectedDonorFunding = 0;
	int64 FundingAfterTarget = 0;
	if (!CalculateRegionalFundingContribution(ProjectedTarget, Config, true, ProjectedTargetFunding)
		|| !CalculateRegionalFundingContribution(ProjectedDonor, Config, true, ProjectedDonorFunding)
		|| !TryAdd(
			State.MonthlyFunding,
			ProjectedTargetFunding - TargetMandate->CurrentMonthlyFunding,
			FundingAfterTarget)
		|| !TryAdd(
			FundingAfterTarget,
			ProjectedDonorFunding - DonorMandate->CurrentMonthlyFunding,
			Evaluation.ProjectedMonthlyFunding))
	{
		AddError(Validation, TEXT("financial_overflow"),
			TEXT("The Reciprocal Aid funding projection exceeds the campaign numeric range."));
		return Reject();
	}
	Evaluation.MonthlyFundingDelta =
		Evaluation.ProjectedMonthlyFunding - Evaluation.CurrentMonthlyFunding;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FHorizonCompactRestorationEvaluation FStrategicCommandService::EvaluateHorizonCompactRestoration(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FRestoreHorizonCompactMemberCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FHorizonCompactRestorationEvaluation Evaluation;
	Evaluation.RegionId = Command.RegionId;
	Evaluation.Cost = Config.HorizonCompactRestorationCost;
	Evaluation.MinimumSupport = Config.HorizonCompactRestorationMinimumSupport;
	Evaluation.CurrentMonthlyFunding = State.MonthlyFunding;
	Evaluation.ProjectedMonthlyFunding = State.MonthlyFunding;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidateAdversaryConfig(Config, Validation))
	{
		return Reject();
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_finished"),
			TEXT("Compact restoration is unavailable after the campaign has ended."));
		return Reject();
	}
	if (!State.bHorizonCompactRatified)
	{
		AddError(Validation, TEXT("coalition_restoration_compact_required"),
			TEXT("Member restoration requires a ratified Horizon Compact."));
		return Reject();
	}

	const FRegionalMandateState* Mandate = FindRegionalMandate(State, Command.RegionId);
	if (Mandate == nullptr || !Mandate->bResilienceCharterSigned
		|| !Mandate->bHorizonCompactMemberWithdrawn)
	{
		AddError(Validation, TEXT("coalition_restoration_target_not_withdrawn"),
			TEXT("Compact restoration must target a signed member that has withdrawn."));
		return Reject();
	}
	Evaluation.bWithdrawn = true;
	Evaluation.CurrentSupport = Mandate->Support;
	if (Mandate->Support < Evaluation.MinimumSupport)
	{
		AddError(Validation, TEXT("coalition_restoration_support_required"),
			TEXT("This withdrawn member has not rebuilt enough support to rejoin the Horizon Compact."));
		return Reject();
	}
	if (State.Funds < Evaluation.Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("Compact restoration requires %lld funds, but only %lld are available."),
			Evaluation.Cost, State.Funds));
		return Reject();
	}

	FRegionalMandateState ProjectedMandate = *Mandate;
	ProjectedMandate.bHorizonCompactMemberWithdrawn = false;
	int64 ProjectedContribution = 0;
	if (!CalculateRegionalFundingContribution(
			ProjectedMandate, Config, true, ProjectedContribution)
		|| !TryAdd(
			State.MonthlyFunding,
			ProjectedContribution - Mandate->CurrentMonthlyFunding,
			Evaluation.ProjectedMonthlyFunding))
	{
		AddError(Validation, TEXT("financial_overflow"),
			TEXT("Compact restoration funding exceeds the campaign numeric range."));
		return Reject();
	}
	Evaluation.MonthlyFundingDelta =
		Evaluation.ProjectedMonthlyFunding - Evaluation.CurrentMonthlyFunding;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FHorizonCompactEmergencyVoteEvaluation FStrategicCommandService::EvaluateHorizonCompactEmergencyVote(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FCallHorizonCompactEmergencyVoteCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FHorizonCompactEmergencyVoteEvaluation Evaluation;
	Evaluation.TargetRegionId = Command.TargetRegionId;
	Evaluation.Cost = Config.HorizonCompactEmergencyVoteCost;
	Evaluation.VoterSupportCost = Config.HorizonCompactEmergencyVoterSupportCost;
	Evaluation.MaximumVoterPressure = Config.HorizonCompactEmergencyMaximumVoterPressure;
	Evaluation.CurrentMonthlyFunding = State.MonthlyFunding;
	Evaluation.ProjectedMonthlyFunding = State.MonthlyFunding;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidateAdversaryConfig(Config, Validation))
	{
		return Reject();
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_finished"),
			TEXT("Emergency solidarity votes are unavailable after the campaign has ended."));
		return Reject();
	}
	if (!State.bHorizonCompactRatified)
	{
		AddError(Validation, TEXT("coalition_emergency_vote_compact_required"),
			TEXT("An emergency solidarity vote requires a ratified Horizon Compact."));
		return Reject();
	}

	const int32 CurrentMonth = GetDiplomaticMonthSerial(State.StrategicTime.Utc);
	if (State.LastCoalitionEmergencyVoteMonth < 0
		|| State.LastCoalitionEmergencyVoteMonth > CurrentMonth)
	{
		AddError(Validation, TEXT("invalid_coalition_emergency_vote_state"),
			TEXT("Emergency solidarity vote history cannot be dated after the campaign month."));
		return Reject();
	}
	if (State.LastCoalitionEmergencyVoteMonth == CurrentMonth)
	{
		AddError(Validation, TEXT("coalition_emergency_vote_already_used"),
			TEXT("The Horizon Compact has already passed an emergency solidarity motion this month."));
		return Reject();
	}

	const FRegionalMandateState* TargetMandate =
		FindRegionalMandate(State, Command.TargetRegionId);
	const FRegionalPressureState* TargetPressure =
		FindRegionalPressure(State, Command.TargetRegionId);
	if (TargetMandate == nullptr || TargetPressure == nullptr
		|| !TargetMandate->bResilienceCharterSigned
		|| !TargetMandate->bHorizonCompactMemberWithdrawn)
	{
		AddError(Validation, TEXT("coalition_emergency_vote_target_not_withdrawn"),
			TEXT("An emergency solidarity vote must target a signed member that has withdrawn from the Horizon Compact."));
		return Reject();
	}
	Evaluation.bTargetWithdrawn = true;
	Evaluation.TargetCurrentSupport = TargetMandate->Support;
	Evaluation.TargetProjectedSupport = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(TargetMandate->Support)
			+ Config.HorizonCompactEmergencyTargetSupportGain,
		0, 100));
	Evaluation.TargetSupportGain =
		Evaluation.TargetProjectedSupport - Evaluation.TargetCurrentSupport;
	Evaluation.TargetCurrentPressure = TargetPressure->Pressure;
	Evaluation.TargetProjectedPressure = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(TargetPressure->Pressure)
			- Config.HorizonCompactEmergencyTargetPressureReduction,
		0, 100));
	Evaluation.TargetPressureReduction =
		Evaluation.TargetCurrentPressure - Evaluation.TargetProjectedPressure;

	for (const FRegionalMandateState& Mandate : State.RegionalMandates)
	{
		if (!IsActiveHorizonCompactMember(State, Mandate))
		{
			continue;
		}
		const FRegionalPressureState* Pressure = FindRegionalPressure(State, Mandate.RegionId);
		if (Pressure == nullptr)
		{
			AddError(Validation, TEXT("invalid_coalition_emergency_vote_state"),
				TEXT("Every emergency-vote member must have regional pressure state."));
			return Reject();
		}
		const bool bCanCommitSupport =
			Mandate.Support >= Evaluation.VoterSupportCost
			&& Mandate.Support - Evaluation.VoterSupportCost
				>= Config.HorizonCompactWithdrawalSupportThreshold;
		if (bCanCommitSupport && Pressure->Pressure <= Evaluation.MaximumVoterPressure)
		{
			Evaluation.SupportingMemberRegionIds.Add(Mandate.RegionId);
		}
		else
		{
			Evaluation.OpposingMemberRegionIds.Add(Mandate.RegionId);
		}
	}
	Evaluation.SupportingMemberRegionIds.Sort(FNameLexicalLess());
	Evaluation.OpposingMemberRegionIds.Sort(FNameLexicalLess());
	const int32 VoterCount = Evaluation.SupportingMemberRegionIds.Num()
		+ Evaluation.OpposingMemberRegionIds.Num();
	if (VoterCount == 0)
	{
		AddError(Validation, TEXT("coalition_emergency_vote_member_required"),
			TEXT("An emergency solidarity vote requires at least one active compact member."));
		return Reject();
	}
	Evaluation.RequiredVotes = VoterCount / 2 + 1;
	if (Evaluation.SupportingMemberRegionIds.Num() < Evaluation.RequiredVotes)
	{
		AddError(Validation, TEXT("coalition_emergency_vote_rejected"),
			TEXT("A strict majority of active compact members will not support this emergency motion."));
		return Reject();
	}
	if (Evaluation.TargetSupportGain == 0 && Evaluation.TargetPressureReduction == 0)
	{
		AddError(Validation, TEXT("coalition_emergency_vote_no_effect"),
			TEXT("The withdrawn target has no remaining support or pressure recovery for an emergency motion."));
		return Reject();
	}
	if (State.Funds < Evaluation.Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("An emergency solidarity vote requires %lld funds, but only %lld are available."),
			Evaluation.Cost, State.Funds));
		return Reject();
	}

	FRegionalMandateState ProjectedTarget = *TargetMandate;
	ProjectedTarget.Support = Evaluation.TargetProjectedSupport;
	int64 ProjectedTargetFunding = 0;
	if (!CalculateRegionalFundingContribution(
			ProjectedTarget, Config, true, ProjectedTargetFunding)
		|| !TryAdd(
			State.MonthlyFunding,
			ProjectedTargetFunding - TargetMandate->CurrentMonthlyFunding,
			Evaluation.ProjectedMonthlyFunding))
	{
		AddError(Validation, TEXT("financial_overflow"),
			TEXT("Emergency solidarity recovery funding exceeds the campaign numeric range."));
		return Reject();
	}
	for (const FName RegionId : Evaluation.SupportingMemberRegionIds)
	{
		const FRegionalMandateState* Mandate = FindRegionalMandate(State, RegionId);
		if (Mandate == nullptr)
		{
			AddError(Validation, TEXT("invalid_coalition_emergency_vote_state"),
				TEXT("Emergency-vote membership changed during funding projection."));
			return Reject();
		}
		FRegionalMandateState ProjectedVoter = *Mandate;
		ProjectedVoter.Support -= Evaluation.VoterSupportCost;
		int64 ProjectedVoterFunding = 0;
		if (!CalculateRegionalFundingContribution(
				ProjectedVoter, Config, true, ProjectedVoterFunding)
			|| !TryAdd(
				Evaluation.ProjectedMonthlyFunding,
				ProjectedVoterFunding - Mandate->CurrentMonthlyFunding,
				Evaluation.ProjectedMonthlyFunding))
		{
			AddError(Validation, TEXT("financial_overflow"),
				TEXT("Emergency-vote member funding exceeds the campaign numeric range."));
			return Reject();
		}
	}
	Evaluation.MonthlyFundingDelta =
		Evaluation.ProjectedMonthlyFunding - Evaluation.CurrentMonthlyFunding;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FPersonnelDoctrineEvaluation FStrategicCommandService::EvaluatePersonnelDoctrine(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSelectPersonnelDoctrineCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FPersonnelDoctrineEvaluation Evaluation;
	Evaluation.PersonnelId = Command.PersonnelId;
	Evaluation.DoctrineId = Command.DoctrineId;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation)
		|| !ValidatePersonnelState(State, Rules, Validation))
	{
		return Reject();
	}
	const FPersonnelState* Person = FindPersonnel(State, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Validation, TEXT("unknown_personnel"), TEXT("Personnel member does not exist in the active roster."));
		return Reject();
	}
	if (Person->PendingDoctrineChoices <= 0)
	{
		AddError(Validation, TEXT("personnel_doctrine_choice_unavailable"),
			TEXT("Personnel member has no pending promotion doctrine choice."));
		return Reject();
	}
	if (Person->Status != EPersonnelStatus::Available)
	{
		AddError(Validation, TEXT("personnel_unavailable"),
			TEXT("Only available personnel can select a field doctrine."));
		return Reject();
	}
	const FPersonnelDoctrineRule* Doctrine = Rules.PersonnelDoctrines.Find(Command.DoctrineId);
	if (Doctrine == nullptr)
	{
		AddError(Validation, TEXT("unknown_personnel_doctrine"),
			FString::Printf(TEXT("Personnel doctrine '%s' is not loaded."), *Command.DoctrineId.ToString()));
		return Reject();
	}
	const int64 TotalBonus = static_cast<int64>(Doctrine->MaxHealthBonus) + Doctrine->AccuracyBonus
		+ Doctrine->ResolveBonus + Doctrine->MobilityBonus + Doctrine->StrengthBonus;
	if (Doctrine->MaxSelections <= 0 || Doctrine->MaxSelections > 10
		|| Doctrine->MaxHealthBonus < 0 || Doctrine->MaxHealthBonus > 50
		|| Doctrine->AccuracyBonus < 0 || Doctrine->AccuracyBonus > 25
		|| Doctrine->ResolveBonus < 0 || Doctrine->ResolveBonus > 25
		|| Doctrine->MobilityBonus < 0 || Doctrine->MobilityBonus > 25
		|| Doctrine->StrengthBonus < 0 || Doctrine->StrengthBonus > 25
		|| TotalBonus <= 0)
	{
		AddError(Validation, TEXT("invalid_personnel_doctrine"),
			TEXT("Personnel doctrine bonuses or selection limits are invalid."));
		return Reject();
	}
	Evaluation.CurrentSelections = static_cast<int32>(Algo::CountIf(Person->DoctrineSelections,
		[&Command](const FName DoctrineId) { return DoctrineId == Command.DoctrineId; }));
	Evaluation.MaximumSelections = Doctrine->MaxSelections;
	if (Evaluation.CurrentSelections >= Evaluation.MaximumSelections)
	{
		AddError(Validation, TEXT("personnel_doctrine_maximum"),
			TEXT("Personnel member has already reached this doctrine's maximum level."));
		return Reject();
	}
	const bool bWouldChangeAttribute =
		(Doctrine->MaxHealthBonus > 0 && Person->MaxHealth < 200)
		|| (Doctrine->AccuracyBonus > 0 && Person->Accuracy < 100)
		|| (Doctrine->ResolveBonus > 0 && Person->Resolve < 100)
		|| (Doctrine->MobilityBonus > 0 && Person->Mobility < 100)
		|| (Doctrine->StrengthBonus > 0 && Person->Strength < 100);
	if (!bWouldChangeAttribute)
	{
		AddError(Validation, TEXT("personnel_doctrine_no_effect"),
			TEXT("Personnel attributes are already at this doctrine's supported limits."));
		return Reject();
	}
	Evaluation.bAllowed = true;
	return Evaluation;
}

FBaseInfrastructureEvaluation FStrategicCommandService::EvaluateBaseInfrastructure(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FGuid BaseId)
{
	using namespace StrategicCommandServicePrivate;

	FBaseInfrastructureEvaluation Evaluation;
	Evaluation.BaseId = BaseId;
	FStrategicCommandResult Validation;
	const FStrategicBaseState* Base = FindBase(State, BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"), TEXT("Infrastructure evaluation base does not exist."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	FBasePersonnelCapacityProfile PersonnelCapacity;
	if (!ComputeBasePersonnelCapacities(*Base, Rules, PersonnelCapacity, Validation)
		|| !ComputeBaseCraftCapacity(*Base, Rules, Evaluation.CraftCapacity, Validation)
		|| !ComputeBaseSensorProfile(*Base, Rules, Evaluation.SensorRangeKilometers,
			Evaluation.DetectionStrength, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.BaseScientistCapacity = PersonnelCapacity.BaseScientistCapacity;
	Evaluation.FacilityScientistCapacity = PersonnelCapacity.FacilityScientistCapacity;
	Evaluation.ScientistCapacity = PersonnelCapacity.ScientistCapacity;
	Evaluation.BaseEngineerCapacity = PersonnelCapacity.BaseEngineerCapacity;
	Evaluation.FacilityEngineerCapacity = PersonnelCapacity.FacilityEngineerCapacity;
	Evaluation.EngineerCapacity = PersonnelCapacity.EngineerCapacity;

	int64 MaximumDefenseDamage = 0;
	int64 ExpectedDefenseDamageHundredths = 0;
	const auto AddDefenseContribution = [
		&Evaluation,
		&MaximumDefenseDamage,
		&ExpectedDefenseDamageHundredths,
		&Validation,
		Base](
		const FFacilityRule* Rule,
		const int32 Damage)
	{
		if (Rule == nullptr || Rule->BaseDefenseAccuracy < 0 || Rule->BaseDefenseAccuracy > 100
			|| Rule->BaseDefenseDamage < 0
			|| ((Rule->BaseDefenseAccuracy == 0) != (Rule->BaseDefenseDamage == 0))
			|| (Rule->BaseDefenseSupplyItemId.IsNone()
				? Rule->BaseDefenseSupplyPerShot != 0
				: Rule->BaseDefenseAccuracy == 0 || Rule->BaseDefenseSupplyPerShot <= 0
					|| Rule->BaseDefenseSupplyPerShot > 100000))
		{
			AddError(Validation, TEXT("invalid_base_defense_rule"), FString::Printf(
				TEXT("Base '%s' has an invalid base-defense facility."), *Base->Name));
			return false;
		}
		const int32 Accuracy = Rule->ScaleEffectByIntegrity(Rule->BaseDefenseAccuracy, Damage);
		const int32 DefenseDamage = Rule->ScaleEffectByIntegrity(Rule->BaseDefenseDamage, Damage);
		if (Accuracy > 0 && DefenseDamage > 0)
		{
			++Evaluation.DefenseBatteryCount;
			MaximumDefenseDamage += DefenseDamage;
			ExpectedDefenseDamageHundredths += static_cast<int64>(Accuracy) * DefenseDamage;
			if (MaximumDefenseDamage > MAX_int32)
			{
				AddError(Validation, TEXT("base_defense_overflow"), FString::Printf(
					TEXT("Base '%s' defense damage exceeds the supported range."), *Base->Name));
				return false;
			}
		}
		return true;
	};
	if (!Base->Facilities.IsEmpty())
	{
		for (const FBaseFacilityState& Installed : Base->Facilities)
		{
			const FFacilityRule* Rule = Rules.Facilities.Find(Installed.FacilityId);
			if (Rule == nullptr || Rule->MaxIntegrity <= 0
				|| Installed.Damage < 0 || Installed.Damage > Rule->MaxIntegrity
				|| !AddDefenseContribution(Rule, Installed.Damage))
			{
				if (Validation.Diagnostics.IsEmpty())
				{
					AddError(Validation, TEXT("invalid_facility_state"), FString::Printf(
						TEXT("Base '%s' has invalid facility integrity data."), *Base->Name));
				}
				Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
				return Evaluation;
			}
		}
	}
	else
	{
		for (const FName FacilityId : Base->BuiltFacilities)
		{
			if (!AddDefenseContribution(Rules.Facilities.Find(FacilityId), 0))
			{
				Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
				return Evaluation;
			}
		}
	}
	Evaluation.MaximumDefenseDamage = static_cast<int32>(MaximumDefenseDamage);
	Evaluation.ExpectedDefenseDamage = static_cast<int32>((ExpectedDefenseDamageHundredths + 50) / 100);
	int64 StorageCapacity = 0;
	FStrategicCommandResult SpecializationValidation;
	if (ComputeBaseStorageCapacity(*Base, Rules, StorageCapacity, SpecializationValidation))
	{
		Evaluation.Specialization = BuildBaseSpecialization(
			Evaluation.DetectionStrength,
			Evaluation.FacilityScientistCapacity,
			Evaluation.FacilityEngineerCapacity,
			Evaluation.CraftCapacity,
			StorageCapacity);
	}
	Evaluation.bValid = true;
	return Evaluation;
}

FStrategicBaseSpecializationView FStrategicCommandService::EvaluateBaseSpecialization(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	FCampaignState State;
	State.Bases.Add(Base);
	return EvaluateBaseInfrastructure(State, Rules, Base.BaseId).Specialization;
}

int32 FStrategicCommandService::EvaluateBaseResearchRatePercent(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	const FStrategicBaseSpecializationView Specialization =
		EvaluateBaseSpecialization(Base, Rules);
	if (!Specialization.bSpecialized
		|| Specialization.OperationalBenefitMetricId
			!= FName(TEXT("base.specialization.research-rate"))
		|| Specialization.OperationalBenefitValue <= 0)
	{
		return 100;
	}
	return static_cast<int32>(FMath::Clamp<int64>(
		100 + Specialization.OperationalBenefitValue, 100, 300));
}

int32 FStrategicCommandService::EvaluateBaseManufacturingRatePercent(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	const FStrategicBaseSpecializationView Specialization =
		EvaluateBaseSpecialization(Base, Rules);
	if (!Specialization.bSpecialized
		|| Specialization.OperationalBenefitMetricId
			!= FName(TEXT("base.specialization.manufacturing-rate"))
		|| Specialization.OperationalBenefitValue <= 0)
	{
		return 100;
	}
	return static_cast<int32>(FMath::Clamp<int64>(
		100 + Specialization.OperationalBenefitValue, 100, 300));
}

int32 FStrategicCommandService::EvaluateBaseServiceLaneBonus(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	const FStrategicBaseSpecializationView Specialization =
		EvaluateBaseSpecialization(Base, Rules);
	if (!Specialization.bSpecialized
		|| Specialization.OperationalBenefitMetricId
			!= FName(TEXT("base.specialization.service-lanes"))
		|| Specialization.OperationalBenefitValue <= 0)
	{
		return 0;
	}
	return static_cast<int32>(FMath::Clamp<int64>(
		Specialization.OperationalBenefitValue, 0, MAX_int32));
}

int32 FStrategicCommandService::EvaluateBaseStorageCapacityPercent(
	const FStrategicBaseState& Base,
	const FResolvedRuleSet& Rules)
{
	const FStrategicBaseSpecializationView Specialization =
		EvaluateBaseSpecialization(Base, Rules);
	if (!Specialization.bSpecialized
		|| Specialization.OperationalBenefitMetricId
			!= FName(TEXT("base.specialization.storage-efficiency"))
		|| Specialization.OperationalBenefitValue <= 0)
	{
		return 100;
	}
	return static_cast<int32>(FMath::Clamp<int64>(
		100 + Specialization.OperationalBenefitValue, 100, 300));
}

FBaseStorageEvaluation FStrategicCommandService::EvaluateBaseStorage(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FGuid BaseId)
{
	using namespace StrategicCommandServicePrivate;

	FBaseStorageEvaluation Evaluation;
	Evaluation.BaseId = BaseId;
	FStrategicCommandResult Validation;
	const FStrategicBaseState* Base = FindBase(State, BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"), TEXT("Storage evaluation base does not exist."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ComputeBaseStorage(State, Rules, *Base, Evaluation, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
	}
	return Evaluation;
}

TArray<EMutualAidRoutePolicy> FStrategicCommandService::GetMutualAidRoutePolicies()
{
	return {
		EMutualAidRoutePolicy::OpenRelay,
		EMutualAidRoutePolicy::RapidThread,
		EMutualAidRoutePolicy::VeiledChain
	};
}

FName FStrategicCommandService::GetMutualAidRoutePolicyId(
	const EMutualAidRoutePolicy Policy)
{
	return StrategicCommandServicePrivate::MutualAidRoutePolicyId(Policy);
}

FMutualAidRouteEvaluation FStrategicCommandService::EvaluateMutualAidRoute(
	const FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FGuid SourceBaseId,
	const FGuid DestinationBaseId,
	const EMutualAidRoutePolicy Policy,
	const bool bSignalEscort)
{
	using namespace StrategicCommandServicePrivate;

	FMutualAidRouteEvaluation Evaluation;
	Evaluation.Policy = Policy;
	Evaluation.PolicyId = MutualAidRoutePolicyId(Policy);
	Evaluation.bSignalEscort = bSignalEscort;
	FStrategicCommandResult Validation;
	if (!IsMutualAidRoutingConfigValid(Config))
	{
		AddError(Validation, TEXT("invalid_simulation_config"),
			TEXT("Mutual Aid route durations, exposure, threshold, delay, and escort cost are invalid."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!IsValidMutualAidRoutePolicy(Policy))
	{
		AddError(Validation, TEXT("invalid_mutual_aid_route_policy"),
			TEXT("The selected Mutual Aid route policy is unsupported."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FStrategicBaseState* Source = FindBase(State, SourceBaseId);
	const FStrategicBaseState* Destination = FindBase(State, DestinationBaseId);
	if (Source == nullptr || Destination == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"),
			TEXT("Mutual Aid route evaluation requires two established bases."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Source == Destination)
	{
		AddError(Validation, TEXT("mutual_aid_same_base"),
			TEXT("A Mutual Aid route must connect two different established bases."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FRegionalPressureState* SourcePressure = FindRegionalPressure(State, Source->RegionId);
	const FRegionalPressureState* DestinationPressure = FindRegionalPressure(State, Destination->RegionId);
	const int32 SourceValue = FMath::Clamp(SourcePressure != nullptr ? SourcePressure->Pressure : 0, 0, 100);
	const int32 DestinationValue = FMath::Clamp(
		DestinationPressure != nullptr ? DestinationPressure->Pressure : 0, 0, 100);
	Evaluation.BaselinePressure = (SourceValue + DestinationValue + 1) / 2;
	int32 TransitHours = Config.MutualAidConvoyTransitHours;
	switch (Policy)
	{
	case EMutualAidRoutePolicy::OpenRelay:
		break;
	case EMutualAidRoutePolicy::RapidThread:
		TransitHours = Config.MutualAidRapidThreadTransitHours;
		Evaluation.ExposureModifier = Config.MutualAidRapidThreadExposure;
		break;
	case EMutualAidRoutePolicy::VeiledChain:
		TransitHours = Config.MutualAidVeiledChainTransitHours;
		Evaluation.ExposureModifier = -Config.MutualAidVeiledChainExposureReduction;
		break;
	default:
		checkNoEntry();
		break;
	}
	Evaluation.TransitSeconds = static_cast<int64>(TransitHours) * 3600LL;
	Evaluation.InterdictionDelaySeconds =
		static_cast<int64>(Config.MutualAidInterdictionDelayHours) * 3600LL;
	Evaluation.RoutePressure = FMath::Clamp(
		Evaluation.BaselinePressure + Evaluation.ExposureModifier, 0, 100);
	Evaluation.bInterdictionExpected =
		Evaluation.RoutePressure >= Config.MutualAidInterdictionThreshold;
	Evaluation.SignalEscortCost = Config.MutualAidSignalEscortCost;
	Evaluation.bSignalEscortAffordable = State.Funds >= Evaluation.SignalEscortCost;
	Evaluation.bValid = true;
	return Evaluation;
}

FThreadlineRetuneEvaluation FStrategicCommandService::EvaluateThreadlineRetune(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FRetuneMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FThreadlineRetuneEvaluation Evaluation;
	Evaluation.ConvoyId = Command.ConvoyId;
	Evaluation.RequestedPolicy = Command.RoutePolicy;
	Evaluation.RequestedPolicyId = MutualAidRoutePolicyId(Command.RoutePolicy);
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Mutual Aid Convoys cannot be retuned after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.CurrentPolicy = Convoy->RoutePolicy;
	Evaluation.CurrentPolicyId = MutualAidRoutePolicyId(Convoy->RoutePolicy);
	Evaluation.CurrentTransitSeconds = Convoy->TotalTransitSeconds;
	Evaluation.CurrentRoutePressure = Convoy->RoutePressure;
	Evaluation.bSignalEscort = Convoy->bSignalEscort;
	const FGuid CurrentLegDestinationBaseId = Convoy->RelayWaypointBaseId.IsValid()
		? Convoy->RelayWaypointBaseId
		: Convoy->DestinationBaseId;
	const FMutualAidRouteEvaluation Route = EvaluateMutualAidRoute(
		State, Config, MutualAidCurrentLegOriginBaseId(*Convoy),
		CurrentLegDestinationBaseId,
		Command.RoutePolicy, Convoy->bSignalEscort);
	if (!Route.bValid)
	{
		Evaluation.Diagnostics = Route.Diagnostics;
		return Evaluation;
	}
	Evaluation.RequestedPolicyId = Route.PolicyId;
	Evaluation.RequestedTransitSeconds = Route.TransitSeconds;
	Evaluation.RequestedRoutePressure = Route.RoutePressure;
	Evaluation.bInterdictionExpected = Route.bInterdictionExpected;
	Evaluation.ForecastInterdictionDelaySeconds = Route.InterdictionDelaySeconds;

	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue = CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentRelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (Command.RoutePolicy == Convoy->RoutePolicy)
	{
		AddError(Validation, TEXT("mutual_aid_retune_same_policy"),
			TEXT("Select a different Threadline route before retuning this convoy."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0)
	{
		AddError(Validation, TEXT("mutual_aid_retune_departed"),
			TEXT("Only a held convoy that has never progressed can change its Threadline route."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	FCampaignState Projection = State;
	FMutualAidConvoyState* ProjectedConvoy = Projection.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(ProjectedConvoy != nullptr);
	ProjectedConvoy->RoutePolicy = Route.Policy;
	ProjectedConvoy->TotalTransitSeconds = Route.TransitSeconds;
	ProjectedConvoy->RemainingTransitSeconds = Route.TransitSeconds;
	ProjectedConvoy->RoutePressure = Route.RoutePressure;
	ProjectedConvoy->bInterdictionResolved = !Route.bInterdictionExpected;
	ProjectedConvoy->ForecastInterdictionDelaySeconds = Route.InterdictionDelaySeconds;
	ProjectedConvoy->InterdictionDelaySeconds = 0;
	FStrategicCommandResult ProjectedValidation;
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	const FMutualAidRelayQueueView* ProjectedQueue =
		ProjectedRelay.FindConvoy(Command.ConvoyId);
	if (ProjectedQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The retuned Mutual Aid Convoy could not be projected in its Relay Weave."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.ProjectedRelayQueue = *ProjectedQueue;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FSignalEscortCommissionEvaluation FStrategicCommandService::EvaluateSignalEscortCommission(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FCommissionMutualAidSignalEscortCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FSignalEscortCommissionEvaluation Evaluation;
	Evaluation.ConvoyId = Command.ConvoyId;
	Evaluation.CurrentFunds = State.Funds;
	Evaluation.ProjectedFunds = State.Funds;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Mutual Aid Signal Escorts cannot be commissioned after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!IsMutualAidRoutingConfigValid(Config))
	{
		AddError(Validation, TEXT("invalid_simulation_config"),
			TEXT("Mutual Aid route durations, exposure, threshold, delay, and escort cost are invalid."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.RoutePolicy = Convoy->RoutePolicy;
	Evaluation.RoutePolicyId = MutualAidRoutePolicyId(Convoy->RoutePolicy);
	Evaluation.RoutePressure = Convoy->RoutePressure;
	Evaluation.FundingCost = Config.MutualAidSignalEscortCost;
	const int64 CurrentPreventedDelay = !Convoy->bInterdictionResolved
		? Convoy->ForecastInterdictionDelaySeconds
		: 0;
	const int64 OnwardPreventedDelay = Convoy->RelayWaypointBaseId.IsValid()
		&& !Convoy->bOnwardInterdictionResolved
			? Convoy->OnwardForecastInterdictionDelaySeconds
			: 0;
	if (!TryAdd(CurrentPreventedDelay, OnwardPreventedDelay,
		Evaluation.PreventedDelaySeconds))
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_projection_overflow"),
			TEXT("Relay Waypoint forecast delays exceed the supported route clock."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue = CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentRelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (Convoy->bSignalEscort)
	{
		AddError(Validation, TEXT("mutual_aid_signal_surety_already_committed"),
			TEXT("This convoy already has a committed Signal Escort."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0)
	{
		AddError(Validation, TEXT("mutual_aid_signal_surety_departed"),
			TEXT("Only a held convoy that has never progressed can commission Signal Surety."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentPreventedDelay == 0 && OnwardPreventedDelay == 0)
	{
		AddError(Validation, TEXT("mutual_aid_signal_surety_unneeded"),
			TEXT("This convoy has no unresolved forecast delay for a Signal Escort to prevent."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Funds < Evaluation.FundingCost)
	{
		AddError(Validation, TEXT("mutual_aid_signal_escort_funds"),
			TEXT("Campaign funds cannot cover this convoy's Signal Escort."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	FCampaignState Projection = State;
	FMutualAidConvoyState* ProjectedConvoy = Projection.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(ProjectedConvoy != nullptr);
	ProjectedConvoy->bSignalEscort = true;
	ProjectedConvoy->SignalEscortCost = Evaluation.FundingCost;
	FStrategicCommandResult ProjectedValidation;
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	const FMutualAidRelayQueueView* ProjectedQueue =
		ProjectedRelay.FindConvoy(Command.ConvoyId);
	if (ProjectedQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The escorted Mutual Aid Convoy could not be projected in its Relay Weave."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.ProjectedFunds = State.Funds - Evaluation.FundingCost;
	Evaluation.ProjectedRelayQueue = *ProjectedQueue;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FMutualAidReliefPriorityEvaluation FStrategicCommandService::EvaluateMutualAidReliefPriority(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FPrioritizeMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FMutualAidReliefPriorityEvaluation Evaluation;
	Evaluation.PolicyId = TEXT("logistics.relief-priority");
	Evaluation.ConvoyId = Command.ConvoyId;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Mutual Aid convoy priorities cannot change after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.CurrentDispatchSequence = Convoy->DispatchSequence;
	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue = CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentRelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0)
	{
		AddError(Validation, TEXT("mutual_aid_relief_priority_departed"),
			TEXT("Only a held convoy that has never progressed can receive Relief Priority."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	for (const FMutualAidRelayQueueView& QueueView : CurrentRelay.Convoys)
	{
		if (QueueView.SourceBaseId != Convoy->SourceBaseId || QueueView.bInTransit
			|| QueueView.QueuePosition >= CurrentQueue->QueuePosition)
		{
			continue;
		}
		const FMutualAidConvoyState* Bypassed = State.MutualAidConvoys.FindByPredicate(
			[&QueueView](const FMutualAidConvoyState& Entry)
			{
				return Entry.ConvoyId == QueueView.ConvoyId;
			});
		if (Bypassed == nullptr)
		{
			AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
				TEXT("A held Relay Weave assignment no longer has a convoy record."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (Bypassed->RemainingTransitSeconds != Bypassed->TotalTransitSeconds
			|| Bypassed->InterdictionDelaySeconds != 0)
		{
			AddError(Validation, TEXT("mutual_aid_relief_priority_departed_ahead"),
				TEXT("Relief Priority cannot bypass a held convoy that has already progressed."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		Evaluation.BypassedConvoyIds.Add(Bypassed->ConvoyId);
		Evaluation.CurrentBypassedRelayQueues.Add(QueueView);
	}
	Evaluation.BypassedConvoyCount = Evaluation.BypassedConvoyIds.Num();
	if (Evaluation.BypassedConvoyIds.IsEmpty())
	{
		AddError(Validation, TEXT("mutual_aid_relief_priority_already_front"),
			TEXT("This convoy already leads the held Relay Weave line."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	FCampaignState Projection = State;
	if (!ApplyMutualAidReliefPriority(
		Projection, Command.ConvoyId, Evaluation.BypassedConvoyIds))
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The held Relay Weave order could not be rotated safely."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	FStrategicCommandResult ProjectedValidation;
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* ProjectedConvoy =
		Projection.MutualAidConvoys.FindByPredicate(
			[&Command](const FMutualAidConvoyState& Entry)
			{
				return Entry.ConvoyId == Command.ConvoyId;
			});
	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	const FMutualAidRelayQueueView* ProjectedQueue =
		ProjectedRelay.FindConvoy(Command.ConvoyId);
	if (ProjectedConvoy == nullptr || ProjectedQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The prioritized Mutual Aid Convoy could not be projected in its Relay Weave."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.ProjectedDispatchSequence = ProjectedConvoy->DispatchSequence;
	Evaluation.ProjectedRelayQueue = *ProjectedQueue;
	Evaluation.RecoveredWaitSeconds = FMath::Max<int64>(
		0, Evaluation.CurrentRelayQueue.EstimatedWaitSeconds
			- Evaluation.ProjectedRelayQueue.EstimatedWaitSeconds);
	for (const FGuid& BypassedId : Evaluation.BypassedConvoyIds)
	{
		const FMutualAidRelayQueueView* ProjectedBypassed =
			ProjectedRelay.FindConvoy(BypassedId);
		if (ProjectedBypassed == nullptr)
		{
			AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
				TEXT("A displaced Mutual Aid Convoy could not be projected in its Relay Weave."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		Evaluation.ProjectedBypassedRelayQueues.Add(*ProjectedBypassed);
	}
	Evaluation.bAllowed = true;
	return Evaluation;
}

FMutualAidReliefStandDownEvaluation FStrategicCommandService::EvaluateMutualAidReliefStandDown(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStandDownMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FMutualAidReliefStandDownEvaluation Evaluation;
	Evaluation.PolicyId = TEXT("logistics.relief-stand-down");
	Evaluation.ConvoyId = Command.ConvoyId;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Mutual Aid convoys cannot stand down after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FStrategicBaseState* Source = FindBase(State, Convoy->SourceBaseId);
	const FItemRule* Item = Rules.Items.Find(Convoy->ItemId);
	int64 ReleasedStorage = 0;
	if (Source == nullptr || Item == nullptr || Item->Mass < 0
		|| !TryMultiplyNonNegative(Item->Mass, Convoy->Quantity, ReleasedStorage))
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has invalid source or cargo data."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.SourceBaseId = Convoy->SourceBaseId;
	Evaluation.DestinationBaseId = Convoy->DestinationBaseId;
	Evaluation.ItemId = Convoy->ItemId;
	Evaluation.Quantity = Convoy->Quantity;
	Evaluation.ReleasedStorage = ReleasedStorage;
	Evaluation.SunkSignalEscortCost = Convoy->SignalEscortCost;

	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue = CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentRelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0)
	{
		AddError(Validation, TEXT("mutual_aid_relief_stand_down_departed"),
			TEXT("Only a held convoy that has never progressed can stand down."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FInventoryStack* ExistingSourceStock = Source->Inventory.FindByPredicate(
		[Convoy](const FInventoryStack& Stack) { return Stack.ItemId == Convoy->ItemId; });
	if (ExistingSourceStock != nullptr
		&& Convoy->Quantity > MAX_int32 - ExistingSourceStock->Quantity)
	{
		AddError(Validation, TEXT("mutual_aid_relief_stand_down_inventory_overflow"),
			TEXT("Returning this convoy would exceed the source inventory quantity range."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	for (const FMutualAidRelayQueueView& QueueView : CurrentRelay.Convoys)
	{
		if (QueueView.SourceBaseId != Convoy->SourceBaseId || QueueView.bInTransit
			|| QueueView.QueuePosition <= CurrentQueue->QueuePosition)
		{
			continue;
		}
		Evaluation.AdvancedConvoyIds.Add(QueueView.ConvoyId);
		Evaluation.CurrentAdvancedRelayQueues.Add(QueueView);
	}
	Evaluation.AdvancedConvoyCount = Evaluation.AdvancedConvoyIds.Num();

	FCampaignState Projection = State;
	FStrategicBaseState* ProjectedSource = FindBase(Projection, Convoy->SourceBaseId);
	check(ProjectedSource != nullptr);
	if (!TryAdjustInventory(*ProjectedSource, Convoy->ItemId, Convoy->Quantity)
		|| Projection.MutualAidConvoys.RemoveAll(
			[&Command](const FMutualAidConvoyState& Entry)
			{
				return Entry.ConvoyId == Command.ConvoyId;
			}) != 1)
	{
		AddError(Validation, TEXT("mutual_aid_relief_stand_down_inventory_overflow"),
			TEXT("The convoy cargo could not be returned to its source inventory atomically."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	FBaseStorageEvaluation CurrentSourceStorage;
	FBaseStorageEvaluation ProjectedSourceStorage;
	FStrategicCommandResult ProjectedValidation;
	if (!ComputeBaseStorage(State, Rules, *Source, CurrentSourceStorage, ProjectedValidation)
		|| !ComputeBaseStorage(Projection, Rules, *ProjectedSource,
			ProjectedSourceStorage, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	if (ProjectedSourceStorage.bEnforced
		&& ProjectedSourceStorage.Overflow > CurrentSourceStorage.Overflow)
	{
		AddError(Validation, TEXT("mutual_aid_relief_stand_down_source_storage"),
			TEXT("The source base has no free storage for this convoy's returned cargo."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	for (const FMutualAidRelayQueueView& CurrentActive : CurrentRelay.Convoys)
	{
		if (CurrentActive.SourceBaseId != Convoy->SourceBaseId || !CurrentActive.bInTransit)
		{
			continue;
		}
		const FMutualAidRelayQueueView* ProjectedActive =
			ProjectedRelay.FindConvoy(CurrentActive.ConvoyId);
		if (ProjectedActive == nullptr || !ProjectedActive->bInTransit
			|| ProjectedActive->DispatchSequence != CurrentActive.DispatchSequence
			|| ProjectedActive->RelayChannelNumber != CurrentActive.RelayChannelNumber
			|| ProjectedActive->EstimatedWaitSeconds != CurrentActive.EstimatedWaitSeconds
			|| ProjectedActive->EstimatedArrivalSeconds != CurrentActive.EstimatedArrivalSeconds)
		{
			AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
				TEXT("Relief Stand-Down would alter active Relay Weave work."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}
	for (int32 Index = 0; Index < Evaluation.AdvancedConvoyIds.Num(); ++Index)
	{
		const FMutualAidRelayQueueView* ProjectedQueue =
			ProjectedRelay.FindConvoy(Evaluation.AdvancedConvoyIds[Index]);
		const FMutualAidRelayQueueView& CurrentAdvanced =
			Evaluation.CurrentAdvancedRelayQueues[Index];
		if (ProjectedQueue == nullptr || ProjectedQueue->bInTransit
			|| ProjectedQueue->DispatchSequence != CurrentAdvanced.DispatchSequence
			|| ProjectedQueue->QueuePosition != CurrentAdvanced.QueuePosition - 1
			|| ProjectedQueue->EstimatedWaitSeconds > CurrentAdvanced.EstimatedWaitSeconds
			|| ProjectedQueue->EstimatedArrivalSeconds > CurrentAdvanced.EstimatedArrivalSeconds)
		{
			AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
				TEXT("A later Mutual Aid Convoy could not advance safely after stand-down."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		const int64 RecoveredWait = CurrentAdvanced.EstimatedWaitSeconds
			- ProjectedQueue->EstimatedWaitSeconds;
		if (!TryAdd(Evaluation.TotalRecoveredWaitSeconds, RecoveredWait,
			Evaluation.TotalRecoveredWaitSeconds))
		{
			AddError(Validation, TEXT("mutual_aid_relief_stand_down_projection_overflow"),
				TEXT("Relief Stand-Down queue recovery exceeds the supported time range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		Evaluation.ProjectedAdvancedRelayQueues.Add(*ProjectedQueue);
	}
	Evaluation.bAllowed = true;
	return Evaluation;
}

FMutualAidReliefDiversionEvaluation FStrategicCommandService::EvaluateMutualAidReliefDiversion(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FDivertMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FMutualAidReliefDiversionEvaluation Evaluation;
	Evaluation.PolicyId = TEXT("logistics.relief-diversion");
	Evaluation.ConvoyId = Command.ConvoyId;
	Evaluation.RequestedDestinationBaseId = Command.DestinationBaseId;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Mutual Aid convoys cannot be diverted after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FStrategicBaseState* Source = FindBase(State, Convoy->SourceBaseId);
	const FStrategicBaseState* CurrentDestination =
		FindBase(State, Convoy->DestinationBaseId);
	const FStrategicBaseState* RequestedDestination =
		FindBase(State, Command.DestinationBaseId);
	const FItemRule* Item = Rules.Items.Find(Convoy->ItemId);
	int64 DivertedStorage = 0;
	if (Source == nullptr || CurrentDestination == nullptr || Item == nullptr
		|| Item->Mass < 0
		|| !TryMultiplyNonNegative(Item->Mass, Convoy->Quantity, DivertedStorage))
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has invalid bases or cargo data."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (RequestedDestination == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"),
			TEXT("Relief Diversion requires another established destination base."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (RequestedDestination == Source)
	{
		AddError(Validation, TEXT("mutual_aid_same_base"),
			TEXT("A Mutual Aid route must connect two different established bases."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.SourceBaseId = Convoy->SourceBaseId;
	Evaluation.CurrentDestinationBaseId = Convoy->DestinationBaseId;
	Evaluation.ItemId = Convoy->ItemId;
	Evaluation.Quantity = Convoy->Quantity;
	Evaluation.DivertedStorage = DivertedStorage;
	Evaluation.RoutePolicy = Convoy->RoutePolicy;
	Evaluation.RoutePolicyId = MutualAidRoutePolicyId(Convoy->RoutePolicy);
	Evaluation.CurrentTransitSeconds = Convoy->TotalTransitSeconds;
	Evaluation.CurrentRoutePressure = Convoy->RoutePressure;
	Evaluation.bSignalEscort = Convoy->bSignalEscort;
	Evaluation.RetainedSignalEscortCost = Convoy->SignalEscortCost;

	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue =
		CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentRelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (Command.DestinationBaseId == Convoy->DestinationBaseId)
	{
		AddError(Validation, TEXT("mutual_aid_relief_diversion_same_destination"),
			TEXT("Select a different destination before diverting this relief convoy."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0)
	{
		AddError(Validation, TEXT("mutual_aid_relief_diversion_departed"),
			TEXT("Only a held convoy that has never progressed can be diverted."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Convoy->RelayWaypointBaseId.IsValid())
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_diversion_pending"),
			TEXT("Restore the direct route before changing this convoy's final destination."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FMutualAidRouteEvaluation Route = EvaluateMutualAidRoute(
		State, Config, Convoy->SourceBaseId, Command.DestinationBaseId,
		Convoy->RoutePolicy, Convoy->bSignalEscort);
	if (!Route.bValid)
	{
		Evaluation.Diagnostics = Route.Diagnostics;
		return Evaluation;
	}
	Evaluation.ProjectedRoutePressure = Route.RoutePressure;
	Evaluation.ProjectedTransitSeconds = Route.TransitSeconds;
	Evaluation.bInterdictionExpected = Route.bInterdictionExpected;
	Evaluation.ForecastInterdictionDelaySeconds = Route.InterdictionDelaySeconds;

	FCampaignState Projection = State;
	FMutualAidConvoyState* ProjectedConvoy = Projection.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(ProjectedConvoy != nullptr);
	ProjectedConvoy->DestinationBaseId = Command.DestinationBaseId;
	ProjectedConvoy->TotalTransitSeconds = Route.TransitSeconds;
	ProjectedConvoy->RemainingTransitSeconds = Route.TransitSeconds;
	ProjectedConvoy->RoutePressure = Route.RoutePressure;
	ProjectedConvoy->bInterdictionResolved = !Route.bInterdictionExpected;
	ProjectedConvoy->ForecastInterdictionDelaySeconds = Route.InterdictionDelaySeconds;
	ProjectedConvoy->InterdictionDelaySeconds = 0;
	FStrategicCommandResult ProjectedValidation;
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}

	const FStrategicBaseState* ProjectedCurrentDestination =
		FindBase(Projection, Convoy->DestinationBaseId);
	const FStrategicBaseState* ProjectedRequestedDestination =
		FindBase(Projection, Command.DestinationBaseId);
	check(ProjectedCurrentDestination != nullptr
		&& ProjectedRequestedDestination != nullptr);
	FBaseStorageEvaluation CurrentDestinationStorage;
	FBaseStorageEvaluation ProjectedCurrentDestinationStorage;
	FBaseStorageEvaluation RequestedDestinationStorage;
	FBaseStorageEvaluation ProjectedRequestedDestinationStorage;
	if (!ComputeBaseStorage(State, Rules, *CurrentDestination,
			CurrentDestinationStorage, ProjectedValidation)
		|| !ComputeBaseStorage(Projection, Rules, *ProjectedCurrentDestination,
			ProjectedCurrentDestinationStorage, ProjectedValidation)
		|| !ComputeBaseStorage(State, Rules, *RequestedDestination,
			RequestedDestinationStorage, ProjectedValidation)
		|| !ComputeBaseStorage(Projection, Rules, *ProjectedRequestedDestination,
			ProjectedRequestedDestinationStorage, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentDestinationReservedStorage =
		CurrentDestinationStorage.MutualAidReserved;
	Evaluation.ProjectedCurrentDestinationReservedStorage =
		ProjectedCurrentDestinationStorage.MutualAidReserved;
	Evaluation.RequestedDestinationReservedStorage =
		RequestedDestinationStorage.MutualAidReserved;
	Evaluation.ProjectedRequestedDestinationReservedStorage =
		ProjectedRequestedDestinationStorage.MutualAidReserved;
	int64 ExpectedRequestedReservation = 0;
	if (CurrentDestinationStorage.MutualAidReserved < DivertedStorage
		|| ProjectedCurrentDestinationStorage.MutualAidReserved
			!= CurrentDestinationStorage.MutualAidReserved - DivertedStorage
		|| !TryAdd(RequestedDestinationStorage.MutualAidReserved,
			DivertedStorage, ExpectedRequestedReservation)
		|| ProjectedRequestedDestinationStorage.MutualAidReserved
			!= ExpectedRequestedReservation)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("Relief Diversion could not move the exact destination reservation."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (ProjectedRequestedDestinationStorage.bEnforced
		&& ProjectedRequestedDestinationStorage.Overflow
			> RequestedDestinationStorage.Overflow)
	{
		AddError(Validation, TEXT("mutual_aid_relief_diversion_destination_storage"),
			TEXT("The requested destination has no free storage for this convoy's cargo."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	const FMutualAidRelayQueueView* ProjectedQueue =
		ProjectedRelay.FindConvoy(Command.ConvoyId);
	if (ProjectedQueue == nullptr || ProjectedQueue->bInTransit
		|| ProjectedQueue->DispatchSequence != CurrentQueue->DispatchSequence
		|| ProjectedQueue->QueuePosition != CurrentQueue->QueuePosition
		|| ProjectedQueue->WaitingPosition != CurrentQueue->WaitingPosition
		|| ProjectedQueue->RelayChannelNumber != CurrentQueue->RelayChannelNumber
		|| ProjectedQueue->EstimatedWaitSeconds != CurrentQueue->EstimatedWaitSeconds)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The diverted convoy could not retain its Relay Weave position."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentQueue->EstimatedArrivalSeconds == MAX_int64
		|| ProjectedQueue->EstimatedArrivalSeconds == MAX_int64)
	{
		AddError(Validation, TEXT("mutual_aid_relief_diversion_projection_overflow"),
			TEXT("Relief Diversion queue projection exceeds the supported time range."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.ProjectedRelayQueue = *ProjectedQueue;
	const auto SignedDelta = [](const int64 Before, const int64 After)
	{
		return After >= Before ? After - Before : -(Before - After);
	};
	Evaluation.TargetArrivalShiftSeconds = SignedDelta(
		CurrentQueue->EstimatedArrivalSeconds,
		ProjectedQueue->EstimatedArrivalSeconds);
	Evaluation.TotalArrivalShiftSeconds = Evaluation.TargetArrivalShiftSeconds;

	for (const FMutualAidRelayQueueView& CurrentView : CurrentRelay.Convoys)
	{
		if (CurrentView.SourceBaseId != Convoy->SourceBaseId
			|| CurrentView.ConvoyId == Convoy->ConvoyId)
		{
			continue;
		}
		const FMutualAidRelayQueueView* ProjectedView =
			ProjectedRelay.FindConvoy(CurrentView.ConvoyId);
		if (ProjectedView == nullptr
			|| ProjectedView->DispatchSequence != CurrentView.DispatchSequence
			|| ProjectedView->QueuePosition != CurrentView.QueuePosition)
		{
			AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
				TEXT("Relief Diversion would alter stable Relay Weave order."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (CurrentView.bInTransit
			|| CurrentView.QueuePosition < CurrentQueue->QueuePosition)
		{
			if (ProjectedView->bInTransit != CurrentView.bInTransit
				|| ProjectedView->WaitingPosition != CurrentView.WaitingPosition
				|| ProjectedView->RelayChannelNumber != CurrentView.RelayChannelNumber
				|| ProjectedView->EstimatedWaitSeconds
					!= CurrentView.EstimatedWaitSeconds
				|| ProjectedView->EstimatedArrivalSeconds
					!= CurrentView.EstimatedArrivalSeconds)
			{
				AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
					TEXT("Relief Diversion would alter active or earlier Relay Weave work."));
				Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
				return Evaluation;
			}
			continue;
		}
		if (ProjectedView->bInTransit
			|| CurrentView.EstimatedWaitSeconds == MAX_int64
			|| CurrentView.EstimatedArrivalSeconds == MAX_int64
			|| ProjectedView->EstimatedWaitSeconds == MAX_int64
			|| ProjectedView->EstimatedArrivalSeconds == MAX_int64)
		{
			AddError(Validation, TEXT("mutual_aid_relief_diversion_projection_overflow"),
				TEXT("Relief Diversion follow-on projection exceeds the supported time range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (ProjectedView->RelayChannelNumber == CurrentView.RelayChannelNumber
			&& ProjectedView->EstimatedWaitSeconds == CurrentView.EstimatedWaitSeconds
			&& ProjectedView->EstimatedArrivalSeconds
				== CurrentView.EstimatedArrivalSeconds)
		{
			continue;
		}
		Evaluation.AffectedConvoyIds.Add(CurrentView.ConvoyId);
		Evaluation.CurrentAffectedRelayQueues.Add(CurrentView);
		Evaluation.ProjectedAffectedRelayQueues.Add(*ProjectedView);
		const int64 ArrivalShift = SignedDelta(
			CurrentView.EstimatedArrivalSeconds,
			ProjectedView->EstimatedArrivalSeconds);
		if (!TryAdd(Evaluation.TotalArrivalShiftSeconds, ArrivalShift,
			Evaluation.TotalArrivalShiftSeconds))
		{
			AddError(Validation, TEXT("mutual_aid_relief_diversion_projection_overflow"),
				TEXT("Relief Diversion total arrival shift exceeds the supported time range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}
	Evaluation.AffectedConvoyCount = Evaluation.AffectedConvoyIds.Num();
	Evaluation.bAllowed = true;
	return Evaluation;
}

FMutualAidRelayWaypointEvaluation FStrategicCommandService::EvaluateMutualAidRelayWaypoint(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FConfigureMutualAidRelayWaypointCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FMutualAidRelayWaypointEvaluation Evaluation;
	Evaluation.PolicyId = TEXT("logistics.relay-waypoint");
	Evaluation.ConvoyId = Command.ConvoyId;
	Evaluation.RequestedWaypointBaseId = Command.WaypointBaseId;
	Evaluation.bDirectRouteRequested = !Command.WaypointBaseId.IsValid();
	Evaluation.OnwardRoutePolicy = Command.OnwardRoutePolicy;
	Evaluation.OnwardRoutePolicyId = MutualAidRoutePolicyId(Command.OnwardRoutePolicy);
	FStrategicCommandResult Validation;
	bool bSamePlanRequested = false;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Mutual Aid Relay Waypoints cannot change after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.SourceBaseId = Convoy->SourceBaseId;
	Evaluation.DestinationBaseId = Convoy->DestinationBaseId;
	Evaluation.CurrentWaypointBaseId = Convoy->RelayWaypointBaseId;
	Evaluation.FirstLegRoutePolicy = Convoy->RoutePolicy;
	Evaluation.FirstLegRoutePolicyId = MutualAidRoutePolicyId(Convoy->RoutePolicy);
	Evaluation.bSignalEscort = Convoy->bSignalEscort;
	Evaluation.RetainedSignalEscortCost = Convoy->SignalEscortCost;
	Evaluation.CurrentJourneySeconds =
		FMutualAidRelayQueue::ProjectedJourneySeconds(*Convoy);

	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue =
		CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.CurrentRelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0
		|| Convoy->CurrentLegOriginBaseId.IsValid())
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_departed"),
			TEXT("Only a held convoy that has never progressed can change its Relay Waypoint."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Evaluation.bDirectRouteRequested)
	{
		if (!Convoy->RelayWaypointBaseId.IsValid())
		{
			bSamePlanRequested = true;
		}
	}
	else
	{
		const FStrategicBaseState* Waypoint = FindBase(State, Command.WaypointBaseId);
		if (Waypoint == nullptr)
		{
			AddError(Validation, TEXT("unknown_base"),
				TEXT("Relay Waypoint configuration requires an established intermediate base."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (Command.WaypointBaseId == Convoy->SourceBaseId
			|| Command.WaypointBaseId == Convoy->DestinationBaseId)
		{
			AddError(Validation, TEXT("mutual_aid_relay_waypoint_base"),
				TEXT("A Relay Waypoint must differ from both the source and final destination."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (!IsValidMutualAidRoutePolicy(Command.OnwardRoutePolicy))
		{
			AddError(Validation, TEXT("invalid_mutual_aid_route_policy"),
				TEXT("The selected onward Relay Waypoint route policy is unsupported."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (Convoy->RelayWaypointBaseId == Command.WaypointBaseId
			&& Convoy->OnwardRoutePolicy == Command.OnwardRoutePolicy)
		{
			bSamePlanRequested = true;
		}
	}

	const FGuid FirstLegDestinationBaseId = Evaluation.bDirectRouteRequested
		? Convoy->DestinationBaseId
		: Command.WaypointBaseId;
	const FMutualAidRouteEvaluation FirstLeg = EvaluateMutualAidRoute(
		State, Config, Convoy->SourceBaseId, FirstLegDestinationBaseId,
		Convoy->RoutePolicy, Convoy->bSignalEscort);
	if (!FirstLeg.bValid)
	{
		Evaluation.Diagnostics = FirstLeg.Diagnostics;
		return Evaluation;
	}
	Evaluation.FirstLegRoutePolicyId = FirstLeg.PolicyId;
	Evaluation.FirstLegTransitSeconds = FirstLeg.TransitSeconds;
	Evaluation.FirstLegRoutePressure = FirstLeg.RoutePressure;
	Evaluation.bFirstLegInterdictionExpected = FirstLeg.bInterdictionExpected;

	FMutualAidRouteEvaluation Onward;
	if (!Evaluation.bDirectRouteRequested)
	{
		Onward = EvaluateMutualAidRoute(
			State, Config, Command.WaypointBaseId, Convoy->DestinationBaseId,
			Command.OnwardRoutePolicy, Convoy->bSignalEscort);
		if (!Onward.bValid)
		{
			Evaluation.Diagnostics = Onward.Diagnostics;
			return Evaluation;
		}
		Evaluation.OnwardRoutePolicyId = Onward.PolicyId;
		Evaluation.OnwardTransitSeconds = Onward.TransitSeconds;
		Evaluation.OnwardRoutePressure = Onward.RoutePressure;
		Evaluation.bOnwardInterdictionExpected = Onward.bInterdictionExpected;
	}

	FCampaignState Projection = State;
	FMutualAidConvoyState* ProjectedConvoy = Projection.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(ProjectedConvoy != nullptr);
	ProjectedConvoy->CurrentLegOriginBaseId.Invalidate();
	ProjectedConvoy->RoutePolicy = FirstLeg.Policy;
	ProjectedConvoy->TotalTransitSeconds = FirstLeg.TransitSeconds;
	ProjectedConvoy->RemainingTransitSeconds = FirstLeg.TransitSeconds;
	ProjectedConvoy->RoutePressure = FirstLeg.RoutePressure;
	ProjectedConvoy->bInterdictionResolved = !FirstLeg.bInterdictionExpected;
	ProjectedConvoy->ForecastInterdictionDelaySeconds =
		FirstLeg.InterdictionDelaySeconds;
	ProjectedConvoy->InterdictionDelaySeconds = 0;
	if (Evaluation.bDirectRouteRequested)
	{
		ClearMutualAidOnwardRoute(*ProjectedConvoy);
	}
	else
	{
		ProjectedConvoy->RelayWaypointBaseId = Command.WaypointBaseId;
		ProjectedConvoy->OnwardRoutePolicy = Onward.Policy;
		ProjectedConvoy->OnwardTotalTransitSeconds = Onward.TransitSeconds;
		ProjectedConvoy->OnwardRoutePressure = Onward.RoutePressure;
		ProjectedConvoy->bOnwardInterdictionResolved =
			!Onward.bInterdictionExpected;
		ProjectedConvoy->OnwardForecastInterdictionDelaySeconds =
			Onward.InterdictionDelaySeconds;
	}
	Evaluation.ProjectedJourneySeconds =
		FMutualAidRelayQueue::ProjectedJourneySeconds(*ProjectedConvoy);
	if (Evaluation.CurrentJourneySeconds == MAX_int64
		|| Evaluation.ProjectedJourneySeconds == MAX_int64)
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_projection_overflow"),
			TEXT("Relay Waypoint journey projection exceeds the supported time range."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	FStrategicCommandResult ProjectedValidation;
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	const FBaseStorageEvaluation ProjectedDestinationStorage = EvaluateBaseStorage(
		Projection, Rules, ProjectedConvoy->DestinationBaseId);
	if (!ProjectedDestinationStorage.bValid
		|| (ProjectedDestinationStorage.bEnforced
			&& ProjectedDestinationStorage.Overflow > 0))
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_destination_storage"),
			TEXT("The final destination has no free storage for the projected waypoint journey."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (ProjectedConvoy->BalancedHandoffQuantity > 0)
	{
		const FBaseStorageEvaluation ProjectedWaypointStorage = EvaluateBaseStorage(
			Projection, Rules, ProjectedConvoy->RelayWaypointBaseId);
		if (!ProjectedWaypointStorage.bValid
			|| (ProjectedWaypointStorage.bEnforced
				&& ProjectedWaypointStorage.Overflow > 0))
		{
			AddError(Validation, TEXT("mutual_aid_relay_waypoint_handoff_storage"),
				TEXT("The requested Relay Waypoint has no free storage for the retained Balanced Handoff."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}

	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	const FMutualAidRelayQueueView* ProjectedQueue =
		ProjectedRelay.FindConvoy(Command.ConvoyId);
	if (ProjectedQueue == nullptr || ProjectedQueue->bInTransit
		|| ProjectedQueue->DispatchSequence != CurrentQueue->DispatchSequence
		|| ProjectedQueue->QueuePosition != CurrentQueue->QueuePosition
		|| ProjectedQueue->WaitingPosition != CurrentQueue->WaitingPosition
		|| ProjectedQueue->RelayChannelNumber != CurrentQueue->RelayChannelNumber
		|| ProjectedQueue->EstimatedWaitSeconds != CurrentQueue->EstimatedWaitSeconds)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The waypoint convoy could not retain its Relay Weave position."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentQueue->EstimatedArrivalSeconds == MAX_int64
		|| ProjectedQueue->EstimatedArrivalSeconds == MAX_int64)
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_projection_overflow"),
			TEXT("Relay Waypoint queue projection exceeds the supported time range."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.ProjectedRelayQueue = *ProjectedQueue;
	if (ProjectedQueue->bRelayAvailable)
	{
		const int64 FirstLegDelay = FirstLeg.bInterdictionExpected
			&& !Convoy->bSignalEscort ? FirstLeg.InterdictionDelaySeconds : 0;
		int64 FirstLegJourneySeconds = 0;
		if (!TryAdd(FirstLeg.TransitSeconds, FirstLegDelay, FirstLegJourneySeconds)
			|| !TryAdd(ProjectedQueue->EstimatedWaitSeconds, FirstLegJourneySeconds,
				Evaluation.ProjectedWaypointArrivalSeconds))
		{
			AddError(Validation, TEXT("mutual_aid_relay_waypoint_projection_overflow"),
				TEXT("Relay Waypoint arrival projection exceeds the supported time range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}

	const auto SignedDelta = [](const int64 Before, const int64 After)
	{
		return After >= Before ? After - Before : -(Before - After);
	};
	Evaluation.TargetArrivalShiftSeconds = SignedDelta(
		CurrentQueue->EstimatedArrivalSeconds,
		ProjectedQueue->EstimatedArrivalSeconds);
	Evaluation.TotalArrivalShiftSeconds = Evaluation.TargetArrivalShiftSeconds;
	for (const FMutualAidRelayQueueView& CurrentView : CurrentRelay.Convoys)
	{
		if (CurrentView.SourceBaseId != Convoy->SourceBaseId
			|| CurrentView.ConvoyId == Convoy->ConvoyId)
		{
			continue;
		}
		const FMutualAidRelayQueueView* ProjectedView =
			ProjectedRelay.FindConvoy(CurrentView.ConvoyId);
		if (ProjectedView == nullptr
			|| ProjectedView->DispatchSequence != CurrentView.DispatchSequence
			|| ProjectedView->QueuePosition != CurrentView.QueuePosition)
		{
			AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
				TEXT("Relay Waypoint configuration would alter stable Relay Weave order."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (CurrentView.bInTransit
			|| CurrentView.QueuePosition < CurrentQueue->QueuePosition)
		{
			if (ProjectedView->bInTransit != CurrentView.bInTransit
				|| ProjectedView->WaitingPosition != CurrentView.WaitingPosition
				|| ProjectedView->RelayChannelNumber != CurrentView.RelayChannelNumber
				|| ProjectedView->EstimatedWaitSeconds != CurrentView.EstimatedWaitSeconds
				|| ProjectedView->EstimatedArrivalSeconds
					!= CurrentView.EstimatedArrivalSeconds)
			{
				AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
					TEXT("Relay Waypoint configuration would alter active or earlier relay work."));
				Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
				return Evaluation;
			}
			continue;
		}
		if (ProjectedView->bInTransit
			|| CurrentView.EstimatedWaitSeconds == MAX_int64
			|| CurrentView.EstimatedArrivalSeconds == MAX_int64
			|| ProjectedView->EstimatedWaitSeconds == MAX_int64
			|| ProjectedView->EstimatedArrivalSeconds == MAX_int64)
		{
			AddError(Validation, TEXT("mutual_aid_relay_waypoint_projection_overflow"),
				TEXT("Relay Waypoint follow-on projection exceeds the supported time range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
		if (ProjectedView->RelayChannelNumber == CurrentView.RelayChannelNumber
			&& ProjectedView->EstimatedWaitSeconds == CurrentView.EstimatedWaitSeconds
			&& ProjectedView->EstimatedArrivalSeconds
				== CurrentView.EstimatedArrivalSeconds)
		{
			continue;
		}
		Evaluation.AffectedConvoyIds.Add(CurrentView.ConvoyId);
		Evaluation.CurrentAffectedRelayQueues.Add(CurrentView);
		Evaluation.ProjectedAffectedRelayQueues.Add(*ProjectedView);
		const int64 ArrivalShift = SignedDelta(
			CurrentView.EstimatedArrivalSeconds,
			ProjectedView->EstimatedArrivalSeconds);
		if (!TryAdd(Evaluation.TotalArrivalShiftSeconds, ArrivalShift,
			Evaluation.TotalArrivalShiftSeconds))
		{
			AddError(Validation, TEXT("mutual_aid_relay_waypoint_projection_overflow"),
				TEXT("Relay Waypoint total arrival shift exceeds the supported time range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}
	Evaluation.AffectedConvoyCount = Evaluation.AffectedConvoyIds.Num();
	if (bSamePlanRequested)
	{
		AddError(Validation, TEXT("mutual_aid_relay_waypoint_same_plan"),
			Evaluation.bDirectRouteRequested
				? TEXT("This convoy already has a direct route with no Relay Waypoint.")
				: TEXT("Select a different waypoint or onward route before changing this convoy."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.bAllowed = true;
	return Evaluation;
}

FMutualAidBalancedHandoffEvaluation
FStrategicCommandService::EvaluateMutualAidBalancedHandoff(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FConfigureMutualAidBalancedHandoffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FMutualAidBalancedHandoffEvaluation Evaluation;
	Evaluation.PolicyId = Command.bEnabled
		? TEXT("logistics.mutual-aid-balanced-handoff")
		: TEXT("logistics.mutual-aid-through-cargo");
	Evaluation.ConvoyId = Command.ConvoyId;
	Evaluation.bEnabled = Command.bEnabled;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Validation, TEXT("campaign_concluded"),
			TEXT("Balanced Handoffs cannot change after the campaign has concluded."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FMutualAidConvoyState* Convoy = State.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	if (Convoy == nullptr)
	{
		AddError(Validation, TEXT("unknown_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy is no longer active."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.WaypointBaseId = Convoy->RelayWaypointBaseId;
	Evaluation.DestinationBaseId = Convoy->DestinationBaseId;
	Evaluation.TotalQuantity = Convoy->Quantity;
	Evaluation.CurrentHandoffQuantity = Convoy->BalancedHandoffQuantity;
	Evaluation.ProjectedHandoffQuantity = Command.bEnabled ? Convoy->Quantity / 2 : 0;
	Evaluation.ProjectedFinalQuantity =
		Convoy->Quantity - Evaluation.ProjectedHandoffQuantity;
	const FMutualAidRelayQueueSnapshot CurrentRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	const FMutualAidRelayQueueView* CurrentQueue =
		CurrentRelay.FindConvoy(Convoy->ConvoyId);
	if (CurrentQueue == nullptr)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The selected Mutual Aid Convoy has no Relay Weave assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	Evaluation.RelayQueue = *CurrentQueue;
	Evaluation.bValid = true;
	if (!Convoy->RelayWaypointBaseId.IsValid())
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_waypoint"),
			TEXT("Balanced Handoff requires a pending Relay Waypoint."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (CurrentQueue->bInTransit
		|| Convoy->RemainingTransitSeconds != Convoy->TotalTransitSeconds
		|| Convoy->InterdictionDelaySeconds != 0
		|| Convoy->CurrentLegOriginBaseId.IsValid())
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_departed"),
			TEXT("Only a held waypoint convoy that has never progressed can change its Balanced Handoff."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Command.bEnabled && Convoy->Quantity < 2)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_quantity"),
			TEXT("Balanced Handoff requires at least two cargo units."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FItemRule* Item = Rules.Items.Find(Convoy->ItemId);
	if (Item == nullptr || Item->Mass < 0
		|| !TryMultiplyNonNegative(
			Item->Mass, Evaluation.ProjectedHandoffQuantity, Evaluation.HandoffStorage))
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("Balanced Handoff cargo has invalid or overflowing storage data."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.CurrentWaypointStorage = EvaluateBaseStorage(
		State, Rules, Convoy->RelayWaypointBaseId);
	Evaluation.CurrentDestinationStorage = EvaluateBaseStorage(
		State, Rules, Convoy->DestinationBaseId);
	if (!Evaluation.CurrentWaypointStorage.bValid
		|| !Evaluation.CurrentDestinationStorage.bValid)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("Balanced Handoff could not evaluate current storage commitments."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	FCampaignState Projection = State;
	FMutualAidConvoyState* ProjectedConvoy = Projection.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(ProjectedConvoy != nullptr);
	ProjectedConvoy->BalancedHandoffQuantity = Evaluation.ProjectedHandoffQuantity;
	FStrategicCommandResult ProjectedValidation;
	if (!ValidateMutualAidConvoyState(Projection, Rules, ProjectedValidation))
	{
		Evaluation.Diagnostics = MoveTemp(ProjectedValidation.Diagnostics);
		return Evaluation;
	}
	Evaluation.ProjectedWaypointStorage = EvaluateBaseStorage(
		Projection, Rules, Convoy->RelayWaypointBaseId);
	Evaluation.ProjectedDestinationStorage = EvaluateBaseStorage(
		Projection, Rules, Convoy->DestinationBaseId);
	if (!Evaluation.ProjectedWaypointStorage.bValid
		|| !Evaluation.ProjectedDestinationStorage.bValid)
	{
		AddError(Validation, TEXT("invalid_mutual_aid_convoy"),
			TEXT("Balanced Handoff could not project storage commitments."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Evaluation.ProjectedWaypointStorage.bEnforced
		&& Evaluation.ProjectedWaypointStorage.Overflow > 0)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_waypoint_storage"),
			TEXT("The Relay Waypoint has no free storage for its Balanced Handoff share."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Evaluation.ProjectedDestinationStorage.bEnforced
		&& Evaluation.ProjectedDestinationStorage.Overflow > 0)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_destination_storage"),
			TEXT("The final destination has no free storage for the projected onward share."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	int64 CurrentHandoffStorage = 0;
	int64 CurrentFinalStorage = 0;
	int64 ProjectedFinalStorage = 0;
	int64 OtherWaypointReservations = 0;
	int64 ExpectedWaypointReservations = 0;
	int64 OtherDestinationReservations = 0;
	int64 ExpectedDestinationReservations = 0;
	if (!TryMultiplyNonNegative(
			Item->Mass, Evaluation.CurrentHandoffQuantity, CurrentHandoffStorage)
		|| !TryMultiplyNonNegative(
			Item->Mass,
			Convoy->Quantity - Evaluation.CurrentHandoffQuantity,
			CurrentFinalStorage)
		|| !TryMultiplyNonNegative(
			Item->Mass, Evaluation.ProjectedFinalQuantity, ProjectedFinalStorage)
		|| Evaluation.CurrentWaypointStorage.MutualAidReserved < CurrentHandoffStorage
		|| Evaluation.CurrentDestinationStorage.MutualAidReserved < CurrentFinalStorage)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_projection"),
			TEXT("Balanced Handoff projection could not preserve exact storage and relay invariants."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	OtherWaypointReservations =
		Evaluation.CurrentWaypointStorage.MutualAidReserved - CurrentHandoffStorage;
	OtherDestinationReservations =
		Evaluation.CurrentDestinationStorage.MutualAidReserved - CurrentFinalStorage;
	if (!TryAdd(OtherWaypointReservations, Evaluation.HandoffStorage,
			ExpectedWaypointReservations)
		|| !TryAdd(OtherDestinationReservations, ProjectedFinalStorage,
			ExpectedDestinationReservations)
		|| Evaluation.ProjectedWaypointStorage.MutualAidReserved
			!= ExpectedWaypointReservations
		|| Evaluation.ProjectedDestinationStorage.MutualAidReserved
			!= ExpectedDestinationReservations)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_projection"),
			TEXT("Balanced Handoff projection could not preserve exact storage and relay invariants."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	const FMutualAidRelayQueueSnapshot ProjectedRelay =
		FMutualAidRelayQueue::Evaluate(Projection, Rules);
	const FMutualAidRelayQueueView* ProjectedQueue =
		ProjectedRelay.FindConvoy(Convoy->ConvoyId);
	if (ProjectedQueue == nullptr
		|| ProjectedQueue->DispatchSequence != CurrentQueue->DispatchSequence
		|| ProjectedQueue->QueuePosition != CurrentQueue->QueuePosition
		|| ProjectedQueue->WaitingPosition != CurrentQueue->WaitingPosition
		|| ProjectedQueue->RelayChannelNumber != CurrentQueue->RelayChannelNumber
		|| ProjectedQueue->bInTransit != CurrentQueue->bInTransit
		|| ProjectedQueue->EstimatedWaitSeconds != CurrentQueue->EstimatedWaitSeconds
		|| ProjectedQueue->EstimatedArrivalSeconds != CurrentQueue->EstimatedArrivalSeconds)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_projection"),
			TEXT("Balanced Handoff projection could not preserve exact storage and relay invariants."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Evaluation.CurrentHandoffQuantity == Evaluation.ProjectedHandoffQuantity)
	{
		AddError(Validation, TEXT("mutual_aid_balanced_handoff_same_plan"),
			TEXT("Select a different cargo plan before changing this convoy."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.bAllowed = true;
	return Evaluation;
}

FSignalWatchStaffEvaluation FStrategicCommandService::EvaluateSignalWatchStaff(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetSignalWatchStaffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FSignalWatchStaffEvaluation Evaluation;
	Evaluation.PolicyId = FMutualAidRelayQueue::SignalWatchPolicyId();
	Evaluation.BaseId = Command.BaseId;
	Evaluation.RequestedScientists = Command.AssignedScientists;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Command.AssignedScientists < 0)
	{
		AddError(Validation, TEXT("invalid_staff_assignment"),
			TEXT("Signal Watch scientist assignment cannot be negative."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"),
			TEXT("Signal Watch staffing requires an established base."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Base->SignalWatchScientists < 0)
	{
		AddError(Validation, TEXT("invalid_staff_assignment"),
			TEXT("The base has an invalid negative Signal Watch assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	FBasePersonnelCapacityProfile PersonnelCapacity;
	if (!ComputeBasePersonnelCapacities(*Base, Rules, PersonnelCapacity, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	int64 ResearchAssigned = 0;
	for (const FResearchProjectState& Project : State.ResearchProjects)
	{
		if (Project.BaseId == Base->BaseId
			&& (!TryAdd(ResearchAssigned, Project.AssignedScientists, ResearchAssigned)
				|| ResearchAssigned > MAX_int32))
		{
			AddError(Validation, TEXT("scientist_capacity_exceeded"),
				TEXT("Scientist assignments exceed the campaign numeric range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}

	Evaluation.CurrentScientists = Base->SignalWatchScientists;
	Evaluation.ResearchAssignedScientists = static_cast<int32>(ResearchAssigned);
	Evaluation.ScientistCapacity = PersonnelCapacity.ScientistCapacity;
	Evaluation.FacilityRelayChannelCount =
		FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(*Base, Rules);
	const int64 UncommittedCapacity = FMath::Max<int64>(
		0, static_cast<int64>(Evaluation.ScientistCapacity) - ResearchAssigned);
	Evaluation.MaximumScientists = static_cast<int32>(FMath::Min<int64>(
		Evaluation.FacilityRelayChannelCount, UncommittedCapacity));
	Evaluation.EffectiveWatchScientists = FMath::Min(
		Command.AssignedScientists, Evaluation.FacilityRelayChannelCount);
	Evaluation.BonusRelayChannelCount = Evaluation.EffectiveWatchScientists;
	Evaluation.TotalRelayChannelCount = static_cast<int32>(FMath::Min<int64>(
		MAX_int32, static_cast<int64>(Evaluation.FacilityRelayChannelCount)
			+ Evaluation.BonusRelayChannelCount));
	Evaluation.bValid = true;

	if (Command.AssignedScientists > Evaluation.CurrentScientists
		&& Evaluation.FacilityRelayChannelCount <= 0)
	{
		AddError(Validation, TEXT("mutual_aid_relay_unavailable"),
			TEXT("The source base has no operational signal capacity for a Mutual Aid relay channel."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Command.AssignedScientists > Evaluation.CurrentScientists
		&& Command.AssignedScientists > Evaluation.FacilityRelayChannelCount)
	{
		AddError(Validation, TEXT("signal_watch_channel_capacity_exceeded"),
			TEXT("Signal Watch needs one operational facility channel per scientist; this base currently supports fewer."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	int64 CurrentTotal = 0;
	int64 ProposedTotal = 0;
	if (!TryAdd(ResearchAssigned, Evaluation.CurrentScientists, CurrentTotal)
		|| !TryAdd(ResearchAssigned, Command.AssignedScientists, ProposedTotal))
	{
		AddError(Validation, TEXT("scientist_capacity_exceeded"),
			TEXT("Scientist assignments exceed the campaign numeric range."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (ProposedTotal > Evaluation.ScientistCapacity && ProposedTotal > CurrentTotal)
	{
		AddError(Validation, TEXT("scientist_capacity_exceeded"), FString::Printf(
			TEXT("Signal Watch and research need %lld scientists at a base with effective capacity %d; reduce existing overcapacity before increasing staff."),
			ProposedTotal, Evaluation.ScientistCapacity));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.bAllowed = true;
	return Evaluation;
}

FWorksCadreStaffEvaluation FStrategicCommandService::EvaluateWorksCadreStaff(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetWorksCadreStaffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FWorksCadreStaffEvaluation Evaluation;
	Evaluation.PolicyId = WorksCadrePolicyId();
	Evaluation.BaseId = Command.BaseId;
	Evaluation.RequestedEngineers = Command.AssignedEngineers;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Command.AssignedEngineers < 0)
	{
		AddError(Validation, TEXT("invalid_staff_assignment"),
			TEXT("Works Cadre engineer assignment cannot be negative."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"),
			TEXT("Works Cadre staffing requires an established base."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Base->WorksCadreEngineers < 0
		|| Base->WorksCadreEngineers > WorksCadreMaximumEngineerCount)
	{
		AddError(Validation, TEXT("invalid_staff_assignment"),
			TEXT("The base has an invalid Works Cadre assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!IsKnownWorksCadreCharter(Base->WorksCadreCharter))
	{
		AddError(Validation, TEXT("invalid_works_cadre_charter"),
			TEXT("The base has an unknown Works Charter."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FWorksCadreCharterPolicy CharterPolicy =
		GetWorksCadreCharterPolicy(Base->WorksCadreCharter);
	Evaluation.Charter = Base->WorksCadreCharter;
	Evaluation.ConstructionFrontloadPercentPerEngineer =
		CharterPolicy.ConstructionFrontloadPercentPerEngineer;
	Evaluation.RepairFrontloadPercentPerEngineer =
		CharterPolicy.RepairFrontloadPercentPerEngineer;

	FBasePersonnelCapacityProfile PersonnelCapacity;
	if (!ComputeBasePersonnelCapacities(*Base, Rules, PersonnelCapacity, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	int64 ManufacturingAssigned = 0;
	for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
	{
		if (Project.BaseId == Base->BaseId
			&& (!TryAdd(ManufacturingAssigned, Project.AssignedEngineers, ManufacturingAssigned)
				|| ManufacturingAssigned > MAX_int32))
		{
			AddError(Validation, TEXT("engineer_capacity_exceeded"),
				TEXT("Engineer assignments exceed the campaign numeric range."));
			Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
			return Evaluation;
		}
	}

	Evaluation.CurrentEngineers = Base->WorksCadreEngineers;
	Evaluation.ManufacturingAssignedEngineers = static_cast<int32>(ManufacturingAssigned);
	Evaluation.EngineerCapacity = PersonnelCapacity.EngineerCapacity;
	const int64 UncommittedCapacity = FMath::Max<int64>(
		0, static_cast<int64>(Evaluation.EngineerCapacity) - ManufacturingAssigned);
	Evaluation.MaximumEngineers = static_cast<int32>(FMath::Min<int64>(
		WorksCadreMaximumEngineerCount, UncommittedCapacity));
	Evaluation.ConstructionFrontloadPercent = static_cast<int32>(FMath::Min<int64>(
		WorksCadreMaximumEngineerCount
			* CharterPolicy.ConstructionFrontloadPercentPerEngineer,
		static_cast<int64>(Command.AssignedEngineers)
			* CharterPolicy.ConstructionFrontloadPercentPerEngineer));
	Evaluation.RepairFrontloadPercent = static_cast<int32>(FMath::Min<int64>(
		WorksCadreMaximumEngineerCount
			* CharterPolicy.RepairFrontloadPercentPerEngineer,
		static_cast<int64>(Command.AssignedEngineers)
			* CharterPolicy.RepairFrontloadPercentPerEngineer));
	Evaluation.bValid = true;

	if (Command.AssignedEngineers > WorksCadreMaximumEngineerCount)
	{
		AddError(Validation, TEXT("works_cadre_limit_exceeded"), FString::Printf(
			TEXT("Works Cadre supports at most %d engineers at one base."),
			WorksCadreMaximumEngineerCount));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	int64 CurrentTotal = 0;
	int64 ProposedTotal = 0;
	if (!TryAdd(ManufacturingAssigned, Evaluation.CurrentEngineers, CurrentTotal)
		|| !TryAdd(ManufacturingAssigned, Command.AssignedEngineers, ProposedTotal))
	{
		AddError(Validation, TEXT("engineer_capacity_exceeded"),
			TEXT("Engineer assignments exceed the campaign numeric range."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (ProposedTotal > Evaluation.EngineerCapacity && ProposedTotal > CurrentTotal)
	{
		AddError(Validation, TEXT("engineer_capacity_exceeded"), FString::Printf(
			TEXT("Works Cadre and manufacturing need %lld engineers at a base with effective capacity %d; reduce existing overcapacity before increasing staff."),
			ProposedTotal, Evaluation.EngineerCapacity));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.bAllowed = true;
	return Evaluation;
}

FWorksCadreCharterEvaluation FStrategicCommandService::EvaluateWorksCadreCharter(
	const FCampaignState& State,
	const FSetWorksCadreCharterCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FWorksCadreCharterEvaluation Evaluation;
	Evaluation.BaseId = Command.BaseId;
	Evaluation.RequestedCharter = Command.Charter;
	FStrategicCommandResult Validation;
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FWorksCadreCharterPolicy RequestedPolicy =
		GetWorksCadreCharterPolicy(Command.Charter);
	if (!IsKnownWorksCadreCharter(Command.Charter) || RequestedPolicy.PolicyId.IsNone())
	{
		AddError(Validation, TEXT("invalid_works_cadre_charter"),
			TEXT("The selected Works Charter is not supported."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"),
			TEXT("Works Charter selection requires an established base."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (!IsKnownWorksCadreCharter(Base->WorksCadreCharter))
	{
		AddError(Validation, TEXT("invalid_works_cadre_charter"),
			TEXT("The base has an unknown Works Charter."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}
	if (Base->WorksCadreEngineers < 0
		|| Base->WorksCadreEngineers > WorksCadreMaximumEngineerCount)
	{
		AddError(Validation, TEXT("invalid_staff_assignment"),
			TEXT("The base has an invalid Works Cadre assignment."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.CurrentCharter = Base->WorksCadreCharter;
	Evaluation.PolicyId = RequestedPolicy.PolicyId;
	Evaluation.AssignedEngineers = Base->WorksCadreEngineers;
	Evaluation.ConstructionFrontloadPercentPerEngineer =
		RequestedPolicy.ConstructionFrontloadPercentPerEngineer;
	Evaluation.RepairFrontloadPercentPerEngineer =
		RequestedPolicy.RepairFrontloadPercentPerEngineer;
	Evaluation.ConstructionFrontloadPercent = Base->WorksCadreEngineers
		* RequestedPolicy.ConstructionFrontloadPercentPerEngineer;
	Evaluation.RepairFrontloadPercent = Base->WorksCadreEngineers
		* RequestedPolicy.RepairFrontloadPercentPerEngineer;
	Evaluation.bValid = true;
	if (Command.Charter == Base->WorksCadreCharter)
	{
		AddError(Validation, TEXT("works_cadre_charter_already_active"),
			TEXT("The selected Works Charter is already active at this base."));
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	}

	Evaluation.bAllowed = true;
	return Evaluation;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FEstablishBaseCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (!Command.BaseId.IsValid() || FindBase(State, Command.BaseId) != nullptr)
	{
		AddError(Result, TEXT("invalid_base_id"), TEXT("Base id must be valid and unique."));
		return Result;
	}
	const FString TrimmedName = Command.Name.TrimStartAndEnd();
	if (TrimmedName.IsEmpty() || TrimmedName.Len() > 64)
	{
		AddError(Result, TEXT("invalid_base_name"), TEXT("Base name must contain 1-64 non-whitespace characters."));
		return Result;
	}
	if (!FContentPackageResolver::IsValidPackageId(Command.RegionId))
	{
		AddError(Result, TEXT("invalid_region_id"), TEXT("Base region must be a valid namespaced id."));
		return Result;
	}
	if (Command.LongitudeMilliDegrees < -180000 || Command.LongitudeMilliDegrees > 180000
		|| Command.LatitudeMilliDegrees < -90000 || Command.LatitudeMilliDegrees > 90000)
	{
		AddError(Result, TEXT("invalid_coordinates"), TEXT("Base coordinates are outside longitude/latitude bounds."));
		return Result;
	}
	if (Config.BaseEstablishmentCost < 0 || Config.DefaultScientistCapacity < 0 || Config.DefaultEngineerCapacity < 0
		|| Config.BaseGridWidth <= 0 || Config.BaseGridHeight <= 0)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("Base cost and staff capacities cannot be negative."));
		return Result;
	}
	if (!ValidateFacilities(Command.StartingFacilities, Rules, TEXT("duplicate_starting_facility"), Result))
	{
		return Result;
	}

	int64 TotalCost = Config.BaseEstablishmentCost;
	for (const FName FacilityId : Command.StartingFacilities)
	{
		if (!TryAdd(TotalCost, Rules.Facilities.FindChecked(FacilityId).BuildCost, TotalCost))
		{
			AddError(Result, TEXT("financial_overflow"), TEXT("Base establishment cost exceeds the campaign numeric range."));
			return Result;
		}
	}
	if (State.Funds < TotalCost)
	{
		AddError(Result, TEXT("insufficient_funds"), FString::Printf(TEXT("Base requires %lld funds, but only %lld are available."), TotalCost, State.Funds));
		return Result;
	}

	FCampaignState Transaction = State;
	Transaction.Funds -= TotalCost;
	FStrategicBaseState& Base = Transaction.Bases.AddDefaulted_GetRef();
	Base.BaseId = Command.BaseId;
	Base.Name = TrimmedName;
	Base.RegionId = Command.RegionId;
	Base.LongitudeMilliDegrees = Command.LongitudeMilliDegrees;
	Base.LatitudeMilliDegrees = Command.LatitudeMilliDegrees;
	Base.ScientistCapacity = Config.DefaultScientistCapacity;
	Base.EngineerCapacity = Config.DefaultEngineerCapacity;
	for (int32 Index = 0; Index < Command.StartingFacilities.Num(); ++Index)
	{
		const FName FacilityId = Command.StartingFacilities[Index];
		if (!TryPlaceFacilityFirstFit(Base, FacilityId, MakeDeterministicFacilityId(Base.BaseId, FacilityId, Index), Rules, Config))
		{
			AddError(Result, TEXT("starting_facility_layout_failed"), FString::Printf(TEXT("Starting facility '%s' does not fit the configured base grid."), *FacilityId.ToString()));
			return Result;
		}
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;

	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::BaseEstablished, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.Amount = -TotalCost;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetWorksCadreStaffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FWorksCadreStaffEvaluation Evaluation =
		EvaluateWorksCadreStaff(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FStrategicBaseState* Base = FindBase(Transaction, Command.BaseId);
	check(Base != nullptr);
	Base->WorksCadreEngineers = Command.AssignedEngineers;
	++Transaction.CommandSequence;
	FStrategicEvent& Changed = AddEvent(
		Result, EStrategicEventType::WorksCadreStaffChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Changed.BaseId = Command.BaseId;
	Changed.PolicyId = Evaluation.PolicyId;
	Changed.Quantity = Command.AssignedEngineers;
	Changed.WorksCadreAssignedEngineers = Command.AssignedEngineers;
	Changed.WorksCadreCharter = Evaluation.Charter;
	Changed.WorksCadreConstructionFrontloadPercent =
		Evaluation.ConstructionFrontloadPercent;
	Changed.WorksCadreRepairFrontloadPercent =
		Evaluation.RepairFrontloadPercent;
	Changed.WorksCadreFrontloadPercent = FMath::Max(
		Evaluation.ConstructionFrontloadPercent,
		Evaluation.RepairFrontloadPercent);
	Changed.bSuccessful = true;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FSetWorksCadreCharterCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FWorksCadreCharterEvaluation Evaluation =
		EvaluateWorksCadreCharter(State, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FStrategicBaseState* Base = FindBase(Transaction, Command.BaseId);
	check(Base != nullptr);
	const EWorksCadreCharter PreviousCharter = Base->WorksCadreCharter;
	Base->WorksCadreCharter = Command.Charter;
	++Transaction.CommandSequence;
	FStrategicEvent& Changed = AddEvent(
		Result, EStrategicEventType::WorksCadreCharterChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Changed.BaseId = Command.BaseId;
	Changed.PolicyId = Evaluation.PolicyId;
	Changed.PreviousWorksCadreCharter = PreviousCharter;
	Changed.WorksCadreCharter = Command.Charter;
	Changed.WorksCadreAssignedEngineers = Evaluation.AssignedEngineers;
	Changed.WorksCadreConstructionFrontloadPercent =
		Evaluation.ConstructionFrontloadPercent;
	Changed.WorksCadreRepairFrontloadPercent =
		Evaluation.RepairFrontloadPercent;
	Changed.bSuccessful = true;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FRetuneMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FThreadlineRetuneEvaluation Evaluation =
		EvaluateThreadlineRetune(State, Rules, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Convoy != nullptr);
	Convoy->RoutePolicy = Evaluation.RequestedPolicy;
	Convoy->TotalTransitSeconds = Evaluation.RequestedTransitSeconds;
	Convoy->RemainingTransitSeconds = Evaluation.RequestedTransitSeconds;
	Convoy->RoutePressure = Evaluation.RequestedRoutePressure;
	Convoy->bInterdictionResolved = !Evaluation.bInterdictionExpected;
	Convoy->ForecastInterdictionDelaySeconds =
		Evaluation.ForecastInterdictionDelaySeconds;
	Convoy->InterdictionDelaySeconds = 0;
	++Transaction.CommandSequence;

	FStrategicEvent& Retuned = AddEvent(
		Result, EStrategicEventType::MutualAidConvoyThreadlineRetuned,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Retuned.BaseId = Convoy->SourceBaseId;
	Retuned.RelatedBaseId = Convoy->DestinationBaseId;
	Retuned.ConvoyId = Convoy->ConvoyId;
	Retuned.RuleId = Convoy->ItemId;
	Retuned.PolicyId = Evaluation.RequestedPolicyId;
	Retuned.PreviousPolicyId = Evaluation.CurrentPolicyId;
	Retuned.Quantity = Convoy->Quantity;
	Retuned.ConvoyPreviousRoutePressure = Evaluation.CurrentRoutePressure;
	Retuned.ConvoyRoutePressure = Evaluation.RequestedRoutePressure;
	Retuned.ConvoyPreviousTransitSeconds = Evaluation.CurrentTransitSeconds;
	Retuned.ConvoyTransitSeconds = Evaluation.RequestedTransitSeconds;
	Retuned.ConvoyEscortCost = Convoy->SignalEscortCost;
	Retuned.ConvoyDelaySeconds = Evaluation.ForecastInterdictionDelaySeconds;
	Retuned.bConvoySignalEscort = Convoy->bSignalEscort;
	Retuned.ConvoyRelayChannelCount = Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Retuned.SignalWatchFacilityChannelCount =
		Evaluation.ProjectedRelayQueue.FacilityRelayChannelCount;
	Retuned.SignalWatchAssignedScientists =
		Evaluation.ProjectedRelayQueue.SignalWatchScientistCount;
	Retuned.SignalWatchBonusChannelCount =
		Evaluation.ProjectedRelayQueue.SignalWatchBonusChannelCount;
	Retuned.SignalWatchTotalChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Retuned.ConvoyRelayQueuePosition = Evaluation.ProjectedRelayQueue.QueuePosition;
	Retuned.ConvoyRelayChannelNumber = Evaluation.ProjectedRelayQueue.RelayChannelNumber;
	Retuned.bConvoyRelayActive = Evaluation.ProjectedRelayQueue.bInTransit;
	Retuned.ConvoyRelayWaitSeconds = Evaluation.ProjectedRelayQueue.EstimatedWaitSeconds;
	Retuned.ConvoyEstimatedArrivalSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedArrivalSeconds;
	Retuned.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FCommissionMutualAidSignalEscortCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FSignalEscortCommissionEvaluation Evaluation =
		EvaluateSignalEscortCommission(State, Rules, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Convoy != nullptr);
	Transaction.Funds = Evaluation.ProjectedFunds;
	Convoy->bSignalEscort = true;
	Convoy->SignalEscortCost = Evaluation.FundingCost;
	++Transaction.CommandSequence;

	FStrategicEvent& Commissioned = AddEvent(
		Result, EStrategicEventType::MutualAidConvoySignalEscortCommissioned,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Commissioned.BaseId = Convoy->SourceBaseId;
	Commissioned.RelatedBaseId = Convoy->DestinationBaseId;
	Commissioned.ConvoyId = Convoy->ConvoyId;
	Commissioned.RuleId = Convoy->ItemId;
	Commissioned.PolicyId = Evaluation.RoutePolicyId;
	Commissioned.Quantity = Convoy->Quantity;
	Commissioned.Amount = Evaluation.FundingCost;
	Commissioned.ConvoyRoutePressure = Evaluation.RoutePressure;
	Commissioned.ConvoyTransitSeconds = Convoy->TotalTransitSeconds;
	Commissioned.ConvoyEscortCost = Evaluation.FundingCost;
	Commissioned.ConvoyDelaySeconds = Evaluation.PreventedDelaySeconds;
	Commissioned.bConvoySignalEscort = true;
	Commissioned.ConvoyRelayChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Commissioned.SignalWatchFacilityChannelCount =
		Evaluation.ProjectedRelayQueue.FacilityRelayChannelCount;
	Commissioned.SignalWatchAssignedScientists =
		Evaluation.ProjectedRelayQueue.SignalWatchScientistCount;
	Commissioned.SignalWatchBonusChannelCount =
		Evaluation.ProjectedRelayQueue.SignalWatchBonusChannelCount;
	Commissioned.SignalWatchTotalChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Commissioned.ConvoyRelayQueuePosition =
		Evaluation.ProjectedRelayQueue.QueuePosition;
	Commissioned.ConvoyRelayChannelNumber =
		Evaluation.ProjectedRelayQueue.RelayChannelNumber;
	Commissioned.bConvoyRelayActive = Evaluation.ProjectedRelayQueue.bInTransit;
	Commissioned.ConvoyRelayWaitSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedWaitSeconds;
	Commissioned.ConvoyPreviousEstimatedArrivalSeconds =
		Evaluation.CurrentRelayQueue.EstimatedArrivalSeconds;
	Commissioned.ConvoyEstimatedArrivalSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedArrivalSeconds;
	Commissioned.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FPrioritizeMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FMutualAidReliefPriorityEvaluation Evaluation =
		EvaluateMutualAidReliefPriority(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	if (!ApplyMutualAidReliefPriority(
		Transaction, Command.ConvoyId, Evaluation.BypassedConvoyIds))
	{
		AddError(Result, TEXT("invalid_mutual_aid_convoy"),
			TEXT("The held Relay Weave order changed before Relief Priority could commit."));
		return Result;
	}
	FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Convoy != nullptr);
	++Transaction.CommandSequence;

	FStrategicEvent& Prioritized = AddEvent(
		Result, EStrategicEventType::MutualAidConvoyReliefPrioritized,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Prioritized.BaseId = Convoy->SourceBaseId;
	Prioritized.RelatedBaseId = Convoy->DestinationBaseId;
	Prioritized.ConvoyId = Convoy->ConvoyId;
	Prioritized.RelatedConvoyId = Evaluation.BypassedConvoyIds[0];
	Prioritized.RuleId = Convoy->ItemId;
	Prioritized.PolicyId = Evaluation.PolicyId;
	Prioritized.Quantity = Convoy->Quantity;
	Prioritized.ConvoyRoutePressure = Convoy->RoutePressure;
	Prioritized.ConvoyTransitSeconds = Convoy->TotalTransitSeconds;
	Prioritized.ConvoyEscortCost = Convoy->SignalEscortCost;
	Prioritized.ConvoyDelaySeconds = Convoy->InterdictionDelaySeconds;
	Prioritized.bConvoySignalEscort = Convoy->bSignalEscort;
	Prioritized.ConvoyPreviousDispatchSequence = Evaluation.CurrentDispatchSequence;
	Prioritized.ConvoyDispatchSequence = Evaluation.ProjectedDispatchSequence;
	Prioritized.ConvoyPriorityBypassedCount = Evaluation.BypassedConvoyCount;
	Prioritized.ConvoyRelayChannelCount = Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Prioritized.SignalWatchFacilityChannelCount =
		Evaluation.ProjectedRelayQueue.FacilityRelayChannelCount;
	Prioritized.SignalWatchAssignedScientists =
		Evaluation.ProjectedRelayQueue.SignalWatchScientistCount;
	Prioritized.SignalWatchBonusChannelCount =
		Evaluation.ProjectedRelayQueue.SignalWatchBonusChannelCount;
	Prioritized.SignalWatchTotalChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Prioritized.ConvoyRelayQueuePosition = Evaluation.ProjectedRelayQueue.QueuePosition;
	Prioritized.ConvoyRelayChannelNumber = Evaluation.ProjectedRelayQueue.RelayChannelNumber;
	Prioritized.bConvoyRelayActive = Evaluation.ProjectedRelayQueue.bInTransit;
	Prioritized.ConvoyRelayWaitSeconds = Evaluation.ProjectedRelayQueue.EstimatedWaitSeconds;
	Prioritized.ConvoyPreviousEstimatedArrivalSeconds =
		Evaluation.CurrentRelayQueue.EstimatedArrivalSeconds;
	Prioritized.ConvoyEstimatedArrivalSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedArrivalSeconds;
	Prioritized.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStandDownMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FMutualAidReliefStandDownEvaluation Evaluation =
		EvaluateMutualAidReliefStandDown(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	const FMutualAidConvoyState* Existing = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Existing != nullptr);
	const FMutualAidConvoyState StoodDownConvoy = *Existing;
	FStrategicBaseState* Source = FindBase(Transaction, StoodDownConvoy.SourceBaseId);
	check(Source != nullptr);
	if (!TryAdjustInventory(*Source, StoodDownConvoy.ItemId, StoodDownConvoy.Quantity)
		|| Transaction.MutualAidConvoys.RemoveAll(
			[&Command](const FMutualAidConvoyState& Entry)
			{
				return Entry.ConvoyId == Command.ConvoyId;
			}) != 1)
	{
		AddError(Result, TEXT("mutual_aid_relief_stand_down_inventory_overflow"),
			TEXT("The convoy cargo changed before Relief Stand-Down could commit."));
		return Result;
	}
	++Transaction.CommandSequence;

	FStrategicEvent& StoodDown = AddEvent(
		Result, EStrategicEventType::MutualAidConvoyReliefStoodDown,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	StoodDown.BaseId = StoodDownConvoy.SourceBaseId;
	StoodDown.RelatedBaseId = StoodDownConvoy.DestinationBaseId;
	StoodDown.ConvoyId = StoodDownConvoy.ConvoyId;
	StoodDown.RelatedConvoyId = Evaluation.AdvancedConvoyIds.IsEmpty()
		? FGuid()
		: Evaluation.AdvancedConvoyIds[0];
	StoodDown.RuleId = StoodDownConvoy.ItemId;
	StoodDown.PolicyId = Evaluation.PolicyId;
	StoodDown.Quantity = StoodDownConvoy.Quantity;
	StoodDown.Amount = Evaluation.ReleasedStorage;
	StoodDown.ConvoyRoutePressure = StoodDownConvoy.RoutePressure;
	StoodDown.ConvoyTransitSeconds = StoodDownConvoy.TotalTransitSeconds;
	StoodDown.ConvoyEscortCost = StoodDownConvoy.SignalEscortCost;
	StoodDown.ConvoyDelaySeconds = StoodDownConvoy.InterdictionDelaySeconds;
	StoodDown.bConvoySignalEscort = StoodDownConvoy.bSignalEscort;
	StoodDown.ConvoyPreviousDispatchSequence = StoodDownConvoy.DispatchSequence;
	StoodDown.ConvoyDispatchSequence = 0;
	StoodDown.ConvoyReleasedStorage = Evaluation.ReleasedStorage;
	StoodDown.ConvoyStandDownAdvancedCount = Evaluation.AdvancedConvoyCount;
	StoodDown.ConvoyStandDownRecoveredWaitSeconds =
		Evaluation.TotalRecoveredWaitSeconds;
	StoodDown.ConvoyRelayChannelCount =
		Evaluation.CurrentRelayQueue.RelayChannelCount;
	StoodDown.SignalWatchFacilityChannelCount =
		Evaluation.CurrentRelayQueue.FacilityRelayChannelCount;
	StoodDown.SignalWatchAssignedScientists =
		Evaluation.CurrentRelayQueue.SignalWatchScientistCount;
	StoodDown.SignalWatchBonusChannelCount =
		Evaluation.CurrentRelayQueue.SignalWatchBonusChannelCount;
	StoodDown.SignalWatchTotalChannelCount =
		Evaluation.CurrentRelayQueue.RelayChannelCount;
	StoodDown.ConvoyRelayQueuePosition = Evaluation.CurrentRelayQueue.QueuePosition;
	StoodDown.ConvoyRelayChannelNumber =
		Evaluation.CurrentRelayQueue.RelayChannelNumber;
	StoodDown.bConvoyRelayActive = false;
	StoodDown.ConvoyRelayWaitSeconds =
		Evaluation.CurrentRelayQueue.EstimatedWaitSeconds;
	StoodDown.ConvoyPreviousEstimatedArrivalSeconds =
		Evaluation.CurrentRelayQueue.EstimatedArrivalSeconds;
	StoodDown.ConvoyEstimatedArrivalSeconds = 0;
	StoodDown.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FDivertMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FMutualAidReliefDiversionEvaluation Evaluation =
		EvaluateMutualAidReliefDiversion(State, Rules, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Convoy != nullptr);
	const FGuid PreviousDestinationBaseId = Convoy->DestinationBaseId;
	Convoy->DestinationBaseId = Command.DestinationBaseId;
	Convoy->TotalTransitSeconds = Evaluation.ProjectedTransitSeconds;
	Convoy->RemainingTransitSeconds = Convoy->TotalTransitSeconds;
	Convoy->RoutePressure = Evaluation.ProjectedRoutePressure;
	Convoy->bInterdictionResolved = !Evaluation.bInterdictionExpected;
	Convoy->ForecastInterdictionDelaySeconds =
		Evaluation.ForecastInterdictionDelaySeconds;
	Convoy->InterdictionDelaySeconds = 0;
	++Transaction.CommandSequence;

	FStrategicEvent& Diverted = AddEvent(
		Result, EStrategicEventType::MutualAidConvoyReliefDiverted,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Diverted.BaseId = Convoy->SourceBaseId;
	Diverted.RelatedBaseId = Convoy->DestinationBaseId;
	Diverted.ConvoyId = Convoy->ConvoyId;
	Diverted.RelatedConvoyId = Evaluation.AffectedConvoyIds.IsEmpty()
		? FGuid()
		: Evaluation.AffectedConvoyIds[0];
	Diverted.RuleId = Convoy->ItemId;
	Diverted.PolicyId = Evaluation.PolicyId;
	Diverted.Quantity = Convoy->Quantity;
	Diverted.Amount = Evaluation.DivertedStorage;
	Diverted.ConvoyPreviousRoutePressure = Evaluation.CurrentRoutePressure;
	Diverted.ConvoyRoutePressure = Evaluation.ProjectedRoutePressure;
	Diverted.ConvoyTransitSeconds = Convoy->TotalTransitSeconds;
	Diverted.ConvoyEscortCost = Convoy->SignalEscortCost;
	Diverted.ConvoyDelaySeconds = Evaluation.ForecastInterdictionDelaySeconds;
	Diverted.bConvoySignalEscort = Convoy->bSignalEscort;
	Diverted.ConvoyPreviousDispatchSequence = Convoy->DispatchSequence;
	Diverted.ConvoyDispatchSequence = Convoy->DispatchSequence;
	Diverted.ConvoyPreviousDestinationBaseId = PreviousDestinationBaseId;
	Diverted.ConvoyDivertedStorage = Evaluation.DivertedStorage;
	Diverted.ConvoyDiversionAffectedCount = Evaluation.AffectedConvoyCount;
	Diverted.ConvoyDiversionTotalArrivalShiftSeconds =
		Evaluation.TotalArrivalShiftSeconds;
	Diverted.ConvoyRelayChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Diverted.SignalWatchFacilityChannelCount =
		Evaluation.ProjectedRelayQueue.FacilityRelayChannelCount;
	Diverted.SignalWatchAssignedScientists =
		Evaluation.ProjectedRelayQueue.SignalWatchScientistCount;
	Diverted.SignalWatchBonusChannelCount =
		Evaluation.ProjectedRelayQueue.SignalWatchBonusChannelCount;
	Diverted.SignalWatchTotalChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Diverted.ConvoyRelayQueuePosition =
		Evaluation.ProjectedRelayQueue.QueuePosition;
	Diverted.ConvoyRelayChannelNumber =
		Evaluation.ProjectedRelayQueue.RelayChannelNumber;
	Diverted.bConvoyRelayActive = false;
	Diverted.ConvoyRelayWaitSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedWaitSeconds;
	Diverted.ConvoyPreviousEstimatedArrivalSeconds =
		Evaluation.CurrentRelayQueue.EstimatedArrivalSeconds;
	Diverted.ConvoyEstimatedArrivalSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedArrivalSeconds;
	Diverted.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FConfigureMutualAidRelayWaypointCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FMutualAidRelayWaypointEvaluation Evaluation =
		EvaluateMutualAidRelayWaypoint(State, Rules, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Convoy != nullptr);
	const FGuid PreviousWaypointBaseId = Convoy->RelayWaypointBaseId;
	const FName PreviousOnwardPolicyId = Convoy->RelayWaypointBaseId.IsValid()
		? MutualAidRoutePolicyId(Convoy->OnwardRoutePolicy)
		: NAME_None;
	Convoy->CurrentLegOriginBaseId.Invalidate();
	Convoy->RoutePolicy = Evaluation.FirstLegRoutePolicy;
	Convoy->TotalTransitSeconds = Evaluation.FirstLegTransitSeconds;
	Convoy->RemainingTransitSeconds = Evaluation.FirstLegTransitSeconds;
	Convoy->RoutePressure = Evaluation.FirstLegRoutePressure;
	Convoy->bInterdictionResolved = !Evaluation.bFirstLegInterdictionExpected;
	Convoy->ForecastInterdictionDelaySeconds =
		static_cast<int64>(Config.MutualAidInterdictionDelayHours) * 3600LL;
	Convoy->InterdictionDelaySeconds = 0;
	if (Evaluation.bDirectRouteRequested)
	{
		ClearMutualAidOnwardRoute(*Convoy);
	}
	else
	{
		Convoy->RelayWaypointBaseId = Evaluation.RequestedWaypointBaseId;
		Convoy->OnwardRoutePolicy = Evaluation.OnwardRoutePolicy;
		Convoy->OnwardTotalTransitSeconds = Evaluation.OnwardTransitSeconds;
		Convoy->OnwardRoutePressure = Evaluation.OnwardRoutePressure;
		Convoy->bOnwardInterdictionResolved =
			!Evaluation.bOnwardInterdictionExpected;
		Convoy->OnwardForecastInterdictionDelaySeconds =
			static_cast<int64>(Config.MutualAidInterdictionDelayHours) * 3600LL;
	}
	++Transaction.CommandSequence;

	FStrategicEvent& Configured = AddEvent(
		Result, EStrategicEventType::MutualAidConvoyRelayWaypointConfigured,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Configured.BaseId = Convoy->SourceBaseId;
	Configured.RelatedBaseId = Convoy->DestinationBaseId;
	Configured.ConvoyId = Convoy->ConvoyId;
	Configured.RelatedConvoyId = Evaluation.AffectedConvoyIds.IsEmpty()
		? FGuid()
		: Evaluation.AffectedConvoyIds[0];
	Configured.RuleId = Convoy->ItemId;
	Configured.PolicyId = Evaluation.PolicyId;
	Configured.PreviousPolicyId = PreviousOnwardPolicyId;
	Configured.Quantity = Convoy->Quantity;
	Configured.Amount = Evaluation.TargetArrivalShiftSeconds;
	Configured.ConvoyRoutePressure = Evaluation.FirstLegRoutePressure;
	Configured.ConvoyTransitSeconds = Evaluation.FirstLegTransitSeconds;
	Configured.ConvoyEscortCost = Convoy->SignalEscortCost;
	Configured.ConvoyDelaySeconds = Convoy->ForecastInterdictionDelaySeconds;
	Configured.bConvoySignalEscort = Convoy->bSignalEscort;
	Configured.ConvoyPreviousDispatchSequence = Convoy->DispatchSequence;
	Configured.ConvoyDispatchSequence = Convoy->DispatchSequence;
	Configured.ConvoyRelayWaypointBaseId = Convoy->RelayWaypointBaseId;
	Configured.ConvoyPreviousRelayWaypointBaseId = PreviousWaypointBaseId;
	Configured.ConvoyOnwardPolicyId = Evaluation.bDirectRouteRequested
		? NAME_None
		: Evaluation.OnwardRoutePolicyId;
	Configured.ConvoyOnwardRoutePressure = Evaluation.OnwardRoutePressure;
	Configured.ConvoyOnwardTransitSeconds = Evaluation.OnwardTransitSeconds;
	Configured.ConvoyWaypointArrivalSeconds =
		Evaluation.ProjectedWaypointArrivalSeconds;
	Configured.ConvoyWaypointAffectedCount = Evaluation.AffectedConvoyCount;
	Configured.ConvoyWaypointTotalArrivalShiftSeconds =
		Evaluation.TotalArrivalShiftSeconds;
	Configured.ConvoyHandoffQuantity = Convoy->BalancedHandoffQuantity;
	Configured.ConvoyFinalDeliveryQuantity =
		Convoy->Quantity - Convoy->BalancedHandoffQuantity;
	if (const FItemRule* Item = Rules.Items.Find(Convoy->ItemId))
	{
		const bool bStorageValid = TryMultiplyNonNegative(
			Item->Mass, Convoy->BalancedHandoffQuantity,
			Configured.ConvoyHandoffStorage);
		check(bStorageValid);
	}
	if (Convoy->RelayWaypointBaseId.IsValid())
	{
		const FBaseStorageEvaluation WaypointStorage = EvaluateBaseStorage(
			Transaction, Rules, Convoy->RelayWaypointBaseId);
		Configured.ConvoyWaypointReservedStorage =
			WaypointStorage.MutualAidReserved;
	}
	const FBaseStorageEvaluation DestinationStorage = EvaluateBaseStorage(
		Transaction, Rules, Convoy->DestinationBaseId);
	Configured.ConvoyDestinationReservedStorage =
		DestinationStorage.MutualAidReserved;
	Configured.ConvoyRelayChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Configured.SignalWatchFacilityChannelCount =
		Evaluation.ProjectedRelayQueue.FacilityRelayChannelCount;
	Configured.SignalWatchAssignedScientists =
		Evaluation.ProjectedRelayQueue.SignalWatchScientistCount;
	Configured.SignalWatchBonusChannelCount =
		Evaluation.ProjectedRelayQueue.SignalWatchBonusChannelCount;
	Configured.SignalWatchTotalChannelCount =
		Evaluation.ProjectedRelayQueue.RelayChannelCount;
	Configured.ConvoyRelayQueuePosition =
		Evaluation.ProjectedRelayQueue.QueuePosition;
	Configured.ConvoyRelayChannelNumber =
		Evaluation.ProjectedRelayQueue.RelayChannelNumber;
	Configured.bConvoyRelayActive = false;
	Configured.ConvoyRelayWaitSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedWaitSeconds;
	Configured.ConvoyPreviousEstimatedArrivalSeconds =
		Evaluation.CurrentRelayQueue.EstimatedArrivalSeconds;
	Configured.ConvoyEstimatedArrivalSeconds =
		Evaluation.ProjectedRelayQueue.EstimatedArrivalSeconds;
	Configured.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FConfigureMutualAidBalancedHandoffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FMutualAidBalancedHandoffEvaluation Evaluation =
		EvaluateMutualAidBalancedHandoff(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
		[&Command](const FMutualAidConvoyState& Entry)
		{
			return Entry.ConvoyId == Command.ConvoyId;
		});
	check(Convoy != nullptr);
	const FName PreviousPolicyId = Convoy->BalancedHandoffQuantity > 0
		? TEXT("logistics.mutual-aid-balanced-handoff")
		: TEXT("logistics.mutual-aid-through-cargo");
	Convoy->BalancedHandoffQuantity = Evaluation.ProjectedHandoffQuantity;
	++Transaction.CommandSequence;

	FStrategicEvent& Configured = AddEvent(
		Result, EStrategicEventType::MutualAidConvoyBalancedHandoffConfigured,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Configured.BaseId = Convoy->SourceBaseId;
	Configured.RelatedBaseId = Convoy->DestinationBaseId;
	Configured.ConvoyId = Convoy->ConvoyId;
	Configured.RuleId = Convoy->ItemId;
	Configured.PolicyId = Evaluation.PolicyId;
	Configured.PreviousPolicyId = PreviousPolicyId;
	Configured.Quantity = Convoy->Quantity;
	Configured.Amount = Evaluation.HandoffStorage;
	Configured.ConvoyDispatchSequence = Convoy->DispatchSequence;
	Configured.ConvoyRelayWaypointBaseId = Convoy->RelayWaypointBaseId;
	Configured.ConvoyHandoffQuantity = Evaluation.ProjectedHandoffQuantity;
	Configured.ConvoyFinalDeliveryQuantity = Evaluation.ProjectedFinalQuantity;
	Configured.ConvoyHandoffStorage = Evaluation.HandoffStorage;
	Configured.ConvoyWaypointReservedStorage =
		Evaluation.ProjectedWaypointStorage.MutualAidReserved;
	Configured.ConvoyDestinationReservedStorage =
		Evaluation.ProjectedDestinationStorage.MutualAidReserved;
	Configured.ConvoyRelayChannelCount = Evaluation.RelayQueue.RelayChannelCount;
	Configured.SignalWatchFacilityChannelCount =
		Evaluation.RelayQueue.FacilityRelayChannelCount;
	Configured.SignalWatchAssignedScientists =
		Evaluation.RelayQueue.SignalWatchScientistCount;
	Configured.SignalWatchBonusChannelCount =
		Evaluation.RelayQueue.SignalWatchBonusChannelCount;
	Configured.SignalWatchTotalChannelCount =
		Evaluation.RelayQueue.RelayChannelCount;
	Configured.ConvoyRelayQueuePosition = Evaluation.RelayQueue.QueuePosition;
	Configured.ConvoyRelayChannelNumber = Evaluation.RelayQueue.RelayChannelNumber;
	Configured.bConvoyRelayActive = Evaluation.RelayQueue.bInTransit;
	Configured.ConvoyRelayWaitSeconds = Evaluation.RelayQueue.EstimatedWaitSeconds;
	Configured.ConvoyEstimatedArrivalSeconds =
		Evaluation.RelayQueue.EstimatedArrivalSeconds;
	Configured.bSuccessful = true;

	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FRestoreHorizonCompactMemberCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FHorizonCompactRestorationEvaluation Evaluation =
		EvaluateHorizonCompactRestoration(State, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, Command.RegionId);
	if (Mandate == nullptr || !Mandate->bHorizonCompactMemberWithdrawn)
	{
		AddError(Result, TEXT("invalid_coalition_compact_state"),
			TEXT("Compact membership changed before restoration could be committed."));
		return Result;
	}
	const int64 OldContribution = Mandate->CurrentMonthlyFunding;
	Transaction.Funds -= Evaluation.Cost;
	Mandate->bHorizonCompactMemberWithdrawn = false;
	if (!CalculateRegionalFundingContribution(
			*Mandate, Config, true, Mandate->CurrentMonthlyFunding)
		|| !TryAdd(
			Transaction.MonthlyFunding,
			Mandate->CurrentMonthlyFunding - OldContribution,
			Transaction.MonthlyFunding))
	{
		AddError(Result, TEXT("financial_overflow"),
			TEXT("Compact restoration funding exceeds the campaign numeric range."));
		return Result;
	}
	if (!ValidateAdversaryState(Transaction, Rules, Config, Result))
	{
		return Result;
	}

	const int64 NewContribution = Mandate->CurrentMonthlyFunding;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	const FName RestorationId(TEXT("coalition.compact-restoration"));
	FStrategicEvent& Restored = AddEvent(
		Result, EStrategicEventType::HorizonCompactMemberRestored,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Restored.RuleId = RestorationId;
	Restored.RegionId = Command.RegionId;
	Restored.Amount = -Evaluation.Cost;
	Restored.Quantity = Evaluation.CurrentSupport;
	Restored.bSuccessful = true;
	if (NewContribution != OldContribution)
	{
		FStrategicEvent& FundingChanged = AddEvent(
			Result, EStrategicEventType::RegionalFundingChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		FundingChanged.RuleId = RestorationId;
		FundingChanged.RegionId = Command.RegionId;
		FundingChanged.Amount = NewContribution - OldContribution;
		FundingChanged.Quantity = Config.HorizonCompactFundingPercent;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FCallHorizonCompactEmergencyVoteCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FHorizonCompactEmergencyVoteEvaluation Evaluation =
		EvaluateHorizonCompactEmergencyVote(State, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FRegionalMandateState* TargetMandate =
		FindRegionalMandate(Transaction, Evaluation.TargetRegionId);
	FRegionalPressureState* TargetPressure =
		FindRegionalPressure(Transaction, Evaluation.TargetRegionId);
	if (TargetMandate == nullptr || TargetPressure == nullptr
		|| !TargetMandate->bHorizonCompactMemberWithdrawn)
	{
		AddError(Result, TEXT("invalid_coalition_emergency_vote_state"),
			TEXT("Compact membership changed before the emergency motion could be committed."));
		return Result;
	}

	TMap<FName, int32> OldSupports;
	TMap<FName, int64> OldContributions;
	TArray<FName> AffectedRegionIds = Evaluation.SupportingMemberRegionIds;
	AffectedRegionIds.Add(Evaluation.TargetRegionId);
	AffectedRegionIds.Sort(FNameLexicalLess());
	for (const FName RegionId : AffectedRegionIds)
	{
		const FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, RegionId);
		if (Mandate == nullptr)
		{
			AddError(Result, TEXT("invalid_coalition_emergency_vote_state"),
				TEXT("Emergency-vote membership changed before recovery could be committed."));
			return Result;
		}
		OldSupports.Add(RegionId, Mandate->Support);
		OldContributions.Add(RegionId, Mandate->CurrentMonthlyFunding);
	}
	const int32 OldTargetPressure = TargetPressure->Pressure;
	Transaction.Funds -= Evaluation.Cost;
	Transaction.LastCoalitionEmergencyVoteMonth =
		GetDiplomaticMonthSerial(Transaction.StrategicTime.Utc);
	TargetMandate->Support = Evaluation.TargetProjectedSupport;
	TargetPressure->Pressure = Evaluation.TargetProjectedPressure;
	for (const FName RegionId : Evaluation.SupportingMemberRegionIds)
	{
		FRegionalMandateState* Voter = FindRegionalMandate(Transaction, RegionId);
		if (Voter == nullptr
			|| Voter->Support < Evaluation.VoterSupportCost
			|| Voter->Support - Evaluation.VoterSupportCost
				< Config.HorizonCompactWithdrawalSupportThreshold)
		{
			AddError(Result, TEXT("invalid_coalition_emergency_vote_state"),
				TEXT("A supporting member can no longer make its emergency-vote commitment."));
			return Result;
		}
		Voter->Support -= Evaluation.VoterSupportCost;
	}

	Transaction.MonthlyFunding = State.MonthlyFunding;
	for (const FName RegionId : AffectedRegionIds)
	{
		FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, RegionId);
		if (Mandate == nullptr
			|| !CalculateRegionalFundingContribution(
				*Mandate, Config, true, Mandate->CurrentMonthlyFunding)
			|| !TryAdd(
				Transaction.MonthlyFunding,
				Mandate->CurrentMonthlyFunding - OldContributions.FindChecked(RegionId),
				Transaction.MonthlyFunding))
		{
			AddError(Result, TEXT("financial_overflow"),
				TEXT("Emergency solidarity funding exceeds the campaign numeric range."));
			return Result;
		}
	}
	if (Transaction.MonthlyFunding != Evaluation.ProjectedMonthlyFunding)
	{
		AddError(Result, TEXT("invalid_coalition_emergency_vote_state"),
			TEXT("Emergency solidarity funding changed after the vote preview."));
		return Result;
	}
	if (!ValidateAdversaryState(Transaction, Rules, Config, Result))
	{
		return Result;
	}

	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	const FName VoteId(TEXT("coalition.emergency-solidarity-vote"));
	FStrategicEvent& Resolved = AddEvent(
		Result, EStrategicEventType::CoalitionEmergencyVoteResolved,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Resolved.RuleId = VoteId;
	Resolved.RegionId = Evaluation.TargetRegionId;
	Resolved.Amount = -Evaluation.Cost;
	Resolved.Quantity = Evaluation.SupportingMemberRegionIds.Num();
	Resolved.bSuccessful = true;

	TArray<FName> VoterRegionIds = Evaluation.SupportingMemberRegionIds;
	VoterRegionIds.Append(Evaluation.OpposingMemberRegionIds);
	VoterRegionIds.Sort(FNameLexicalLess());
	for (const FName RegionId : VoterRegionIds)
	{
		const bool bSupported = Evaluation.SupportingMemberRegionIds.Contains(RegionId);
		const FRegionalMandateState* Voter = FindRegionalMandate(Transaction, RegionId);
		if (Voter == nullptr)
		{
			checkNoEntry();
			return Result;
		}
		FStrategicEvent& Ballot = AddEvent(
			Result, EStrategicEventType::CoalitionEmergencyBallotCast,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Ballot.RuleId = VoteId;
		Ballot.RegionId = RegionId;
		Ballot.Amount = bSupported ? -Evaluation.VoterSupportCost : 0;
		Ballot.Quantity = Voter->Support;
		Ballot.bSuccessful = bSupported;
	}

	if (Evaluation.TargetProjectedPressure != OldTargetPressure)
	{
		FStrategicEvent& PressureChanged = AddEvent(
			Result, EStrategicEventType::RegionalPressureChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		PressureChanged.RuleId = VoteId;
		PressureChanged.RegionId = Evaluation.TargetRegionId;
		PressureChanged.Amount =
			Evaluation.TargetProjectedPressure - OldTargetPressure;
		PressureChanged.Quantity = Evaluation.TargetProjectedPressure;
	}
	for (const FName RegionId : AffectedRegionIds)
	{
		const FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, RegionId);
		if (Mandate == nullptr)
		{
			checkNoEntry();
			return Result;
		}
		const int32 SupportDelta = Mandate->Support - OldSupports.FindChecked(RegionId);
		if (SupportDelta != 0)
		{
			FStrategicEvent& SupportChanged = AddEvent(
				Result, EStrategicEventType::RegionalSupportChanged,
				Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			SupportChanged.RuleId = VoteId;
			SupportChanged.RegionId = RegionId;
			SupportChanged.Amount = SupportDelta;
			SupportChanged.Quantity = Mandate->Support;
		}
		const int64 FundingDelta =
			Mandate->CurrentMonthlyFunding - OldContributions.FindChecked(RegionId);
		if (FundingDelta != 0)
		{
			FStrategicEvent& FundingChanged = AddEvent(
				Result, EStrategicEventType::RegionalFundingChanged,
				Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			FundingChanged.RuleId = VoteId;
			FundingChanged.RegionId = RegionId;
			FundingChanged.Amount = FundingDelta;
			FundingChanged.Quantity = Mandate->bHorizonCompactMemberWithdrawn
				? Config.ResilienceCharterFundingPercent
				: Config.HorizonCompactFundingPercent;
		}
	}

	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FDeployTacticalDeviceCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical devices cannot deploy after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical device deployment references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical device deployment requires confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot deploy devices."));
		return Result;
	}
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical device deployment references an unknown unit."));
		return Result;
	}
	if (ExistingUnit->Team != ETacticalTeam::Player || ExistingUnit->Team != ExistingBattle->ActiveTeam
		|| ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Only a living player unit on the active team can deploy a tactical device."));
		return Result;
	}
	const FItemRule* Device = Rules.Items.Find(Command.DeviceItemId);
	if (Device == nullptr || !Device->IsTacticalDevice())
	{
		AddError(Result, TEXT("invalid_tactical_device"), TEXT("Tactical device item has no valid area-effect profile."));
		return Result;
	}
	const FInventoryStack* ExistingDeviceStack = ExistingUnit->CarriedItems.FindByPredicate(
		[&Command](const FInventoryStack& Stack)
		{
			return Stack.ItemId == Command.DeviceItemId && Stack.Quantity > 0;
		});
	if (ExistingDeviceStack == nullptr)
	{
		AddError(Result, TEXT("tactical_device_unavailable"), TEXT("Unit carries no matching tactical device."));
		return Result;
	}
	if (!ExistingBattle->IsWithinGrid(Command.TargetX, Command.TargetY, Command.TargetZ))
	{
		AddError(Result, TEXT("invalid_tactical_target"), TEXT("Tactical device target is outside the battlefield."));
		return Result;
	}
	const int64 DeltaX = static_cast<int64>(Command.TargetX) - ExistingUnit->X;
	const int64 DeltaY = static_cast<int64>(Command.TargetY) - ExistingUnit->Y;
	const int64 DeltaZ = (static_cast<int64>(Command.TargetZ) - ExistingUnit->Z) * 2;
	if (DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ
		> static_cast<int64>(Device->TacticalRange) * Device->TacticalRange)
	{
		AddError(Result, TEXT("tactical_target_out_of_range"), TEXT("Tactical device target exceeds the device deployment range."));
		return Result;
	}
	int32 LandingX = Command.TargetX;
	int32 LandingY = Command.TargetY;
	int32 LandingZ = Command.TargetZ;
	FTacticalThrowTrajectoryResult ThrowTrajectory;
	if (Device->HasTacticalThrowArc())
	{
		ThrowTrajectory = FTacticalNavigationService::PreviewThrowTrajectory(
			*ExistingBattle,
			Rules,
			ExistingUnit->X,
			ExistingUnit->Y,
			Command.TargetX,
			Command.TargetY,
			Device->TacticalThrowArcHeight,
			ExistingUnit->Z,
			Command.TargetZ);
		if (!ThrowTrajectory.bSucceeded)
		{
			AddError(Result, TEXT("invalid_tactical_throw_trajectory"), TEXT("Tactical device throw trajectory could not be resolved."));
			return Result;
		}
		LandingX = ThrowTrajectory.LandingX;
		LandingY = ThrowTrajectory.LandingY;
		LandingZ = ThrowTrajectory.LandingZ;
	}
	else if (!FTacticalNavigationService::HasLineOfSight(
		*ExistingBattle, Rules, ExistingUnit->X, ExistingUnit->Y, Command.TargetX, Command.TargetY, ExistingUnit->Z, Command.TargetZ))
	{
		AddError(Result, TEXT("no_tactical_line_of_sight"), TEXT("Intact terrain blocks tactical device deployment."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < Device->TacticalActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Unit lacks the action points required to deploy this tactical device."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	FInventoryStack* DeviceStack = Unit->CarriedItems.FindByPredicate(
		[&Command](const FInventoryStack& Stack) { return Stack.ItemId == Command.DeviceItemId; });
	check(DeviceStack != nullptr && DeviceStack->Quantity > 0);
	Unit->RemainingActionPoints -= Device->TacticalActionPointCost;
	--DeviceStack->Quantity;
	Unit->CarriedItems.RemoveAll([](const FInventoryStack& Stack) { return Stack.Quantity == 0; });

	const int64 RadiusSquared = static_cast<int64>(Device->TacticalRadius) * Device->TacticalRadius;
	int32 AffectedCellCount = 0;
	int32 RemovedSmoke = 0;
	int32 RemovedFire = 0;
	for (FTacticalCellState& Cell : Battle->Cells)
	{
		const int64 CellDeltaX = static_cast<int64>(Cell.X) - LandingX;
		const int64 CellDeltaY = static_cast<int64>(Cell.Y) - LandingY;
		const int64 CellDeltaZ = (static_cast<int64>(Cell.Z) - LandingZ) * 2;
		if (CellDeltaX * CellDeltaX + CellDeltaY * CellDeltaY + CellDeltaZ * CellDeltaZ > RadiusSquared)
		{
			continue;
		}
		const int32 PreviousSmoke = Cell.Smoke;
		const int32 PreviousFire = Cell.Fire;
		Cell.Smoke = FMath::Clamp(
			Cell.Smoke + Device->TacticalSmoke - Device->TacticalSmokeReduction,
			0,
			100);
		Cell.Fire = FMath::Clamp(
			Cell.Fire + Device->TacticalFire - Device->TacticalFireReduction,
			0,
			100);
		RemovedSmoke += FMath::Max(0, PreviousSmoke - Cell.Smoke);
		RemovedFire += FMath::Max(0, PreviousFire - Cell.Fire);
		++AffectedCellCount;
	}
	TArray<FTacticalEnvironmentUnitOutcome> UnitOutcomes;
	if (Device->TacticalSuppression > 0 || Device->TacticalSuppressionReduction > 0
		|| Device->TacticalMoraleRecovery > 0)
	{
		for (FTacticalUnitState& AffectedUnit : Battle->Units)
		{
			if (AffectedUnit.CurrentHealth <= 0 || AffectedUnit.bExtracted)
			{
				continue;
			}
			const int64 UnitDeltaX = static_cast<int64>(AffectedUnit.X) - LandingX;
			const int64 UnitDeltaY = static_cast<int64>(AffectedUnit.Y) - LandingY;
			const int64 UnitDeltaZ = (static_cast<int64>(AffectedUnit.Z) - LandingZ) * 2;
			if (UnitDeltaX * UnitDeltaX + UnitDeltaY * UnitDeltaY + UnitDeltaZ * UnitDeltaZ > RadiusSquared)
			{
				continue;
			}
			const int32 PreviousSuppression = AffectedUnit.Suppression;
			const int32 PreviousMorale = AffectedUnit.CurrentMorale;
			AffectedUnit.Suppression = FMath::Clamp(
				AffectedUnit.Suppression + Device->TacticalSuppression - Device->TacticalSuppressionReduction,
				0,
				100);
			if (Device->TacticalSuppression > 0)
			{
				const int32 MoraleLoss = FMath::Max(1, Device->TacticalSuppression / 2 - AffectedUnit.Resolve / 10);
				AffectedUnit.CurrentMorale = FMath::Max(0, AffectedUnit.CurrentMorale - MoraleLoss);
			}
			AffectedUnit.CurrentMorale = FMath::Min(
				AffectedUnit.MaxMorale,
				AffectedUnit.CurrentMorale + Device->TacticalMoraleRecovery);
			FTacticalEnvironmentUnitOutcome& Outcome = UnitOutcomes.AddDefaulted_GetRef();
			Outcome.UnitId = AffectedUnit.UnitId;
			Outcome.UnitRuleId = AffectedUnit.SourceRuleId;
			Outcome.SuppressionDelta = AffectedUnit.Suppression - PreviousSuppression;
			Outcome.Suppression = AffectedUnit.Suppression;
			Outcome.MoraleDelta = AffectedUnit.CurrentMorale - PreviousMorale;
			Outcome.Morale = AffectedUnit.CurrentMorale;
		}
	}
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const int32 ThrowOriginX = Unit->X;
	const int32 ThrowOriginY = Unit->Y;
	const int32 ThrowOriginZ = Unit->Z;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Deployed = AddEvent(Result, EStrategicEventType::TacticalDeviceDeployed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Deployed.BattleId = BattleId;
	Deployed.OperationId = OperationId;
	Deployed.SiteId = SiteId;
	Deployed.TacticalUnitId = UnitId;
	Deployed.RuleId = Command.DeviceItemId;
	Deployed.FromX = ThrowOriginX;
	Deployed.FromY = ThrowOriginY;
	Deployed.FromZ = ThrowOriginZ;
	Deployed.ToX = LandingX;
	Deployed.ToY = LandingY;
	Deployed.ToZ = LandingZ;
	Deployed.bSuccessful = true;
	Deployed.Amount = -Device->TacticalActionPointCost;
	Deployed.Quantity = AffectedCellCount;
	if (ThrowTrajectory.bIntercepted)
	{
		FStrategicEvent& Intercepted = AddEvent(Result, EStrategicEventType::TacticalDeviceIntercepted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Intercepted.BattleId = BattleId;
		Intercepted.OperationId = OperationId;
		Intercepted.SiteId = SiteId;
		Intercepted.TacticalUnitId = UnitId;
		Intercepted.RuleId = Command.DeviceItemId;
		Intercepted.FromX = Command.TargetX;
		Intercepted.FromY = Command.TargetY;
		Intercepted.FromZ = Command.TargetZ;
		Intercepted.ToX = LandingX;
		Intercepted.ToY = LandingY;
		Intercepted.ToZ = LandingZ;
		Intercepted.Amount = Device->TacticalThrowArcHeight;
		Intercepted.Quantity = ThrowTrajectory.InterceptedObstacleHeight;
	}
	if (RemovedSmoke > 0 || RemovedFire > 0)
	{
		FStrategicEvent& Suppressed = AddEvent(Result, EStrategicEventType::TacticalEnvironmentSuppressed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Suppressed.BattleId = BattleId;
		Suppressed.OperationId = OperationId;
		Suppressed.SiteId = SiteId;
		Suppressed.TacticalUnitId = UnitId;
		Suppressed.RuleId = Command.DeviceItemId;
		Suppressed.ToX = LandingX;
		Suppressed.ToY = LandingY;
		Suppressed.ToZ = LandingZ;
		Suppressed.Amount = RemovedSmoke;
		Suppressed.Quantity = RemovedFire;
	}
	for (const FTacticalEnvironmentUnitOutcome& Outcome : UnitOutcomes)
	{
		if (Outcome.SuppressionDelta != 0)
		{
			FStrategicEvent& Suppression = AddEvent(Result, EStrategicEventType::TacticalUnitSuppressed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Suppression.BattleId = BattleId;
			Suppression.OperationId = OperationId;
			Suppression.SiteId = SiteId;
			Suppression.TacticalUnitId = UnitId;
			Suppression.TargetTacticalUnitId = Outcome.UnitId;
			Suppression.RuleId = Command.DeviceItemId;
			Suppression.Amount = Outcome.SuppressionDelta;
			Suppression.Quantity = Outcome.Suppression;
		}
		if (Outcome.MoraleDelta != 0)
		{
			FStrategicEvent& Morale = AddEvent(Result, EStrategicEventType::TacticalMoraleChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Morale.BattleId = BattleId;
			Morale.OperationId = OperationId;
			Morale.SiteId = SiteId;
			Morale.TacticalUnitId = Outcome.UnitId;
			Morale.RuleId = Outcome.UnitRuleId;
			Morale.Amount = Outcome.MoraleDelta;
			Morale.Quantity = Outcome.Morale;
		}
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = UnitOutcomes.ContainsByPredicate(
		[](const FTacticalEnvironmentUnitOutcome& Outcome) { return Outcome.Morale == 0; });
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStartResearchCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Research project base does not exist."));
		return Result;
	}
	const FResearchRule* Research = Rules.Research.Find(Command.ResearchId);
	if (Research == nullptr)
	{
		AddError(Result, TEXT("unknown_research"), FString::Printf(TEXT("Research rule '%s' is not loaded."), *Command.ResearchId.ToString()));
		return Result;
	}
	if (State.CompletedResearch.Contains(Command.ResearchId))
	{
		AddError(Result, TEXT("research_already_completed"), TEXT("Research topic is already complete."));
		return Result;
	}
	if (State.ResearchProjects.ContainsByPredicate([&Command](const FResearchProjectState& Project) { return Project.ResearchId == Command.ResearchId; }))
	{
		AddError(Result, TEXT("research_already_active"), TEXT("Research topic already has an active project."));
		return Result;
	}
	for (const FName Prerequisite : Research->Prerequisites)
	{
		if (!State.CompletedResearch.Contains(Prerequisite))
		{
			AddError(Result, TEXT("research_prerequisite_missing"), FString::Printf(TEXT("Research prerequisite '%s' is incomplete."), *Prerequisite.ToString()));
			return Result;
		}
	}
	TArray<FName> MissingFacilityIds;
	if (!HasOperationalResearchFacilities(*Base, Rules, *Research, &MissingFacilityIds))
	{
		TArray<FString> MissingFacilityNames;
		MissingFacilityNames.Reserve(MissingFacilityIds.Num());
		for (const FName FacilityId : MissingFacilityIds)
		{
			const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
			MissingFacilityNames.Add(Facility != nullptr && !Facility->DisplayName.TrimStartAndEnd().IsEmpty()
				? Facility->DisplayName : FacilityId.ToString());
		}
		AddError(Result, TEXT("research_facility_missing"), FString::Printf(
			TEXT("Research requires operational facilities at this base: %s."),
			*FString::Join(MissingFacilityNames, TEXT(", "))));
		return Result;
	}

	FCampaignState Transaction = State;
	FResearchProjectState& Project = Transaction.ResearchProjects.AddDefaulted_GetRef();
	Project.ResearchId = Command.ResearchId;
	Project.BaseId = Command.BaseId;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;

	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ResearchStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.RuleId = Command.ResearchId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetResearchStaffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.AssignedScientists < 0)
	{
		AddError(Result, TEXT("invalid_staff_assignment"), TEXT("Assigned scientists cannot be negative."));
		return Result;
	}

	FCampaignState Transaction = State;
	FResearchProjectState* Project = FindResearchProject(Transaction, Command.ResearchId);
	if (Project == nullptr)
	{
		AddError(Result, TEXT("unknown_research_project"), TEXT("Research project is not active."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(Transaction, Project->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Research project references a missing base."));
		return Result;
	}

	FBasePersonnelCapacityProfile PersonnelCapacity;
	if (!ComputeBasePersonnelCapacities(*Base, Rules, PersonnelCapacity, Result))
	{
		return Result;
	}
	int64 CurrentAssignedAtBase = Base->SignalWatchScientists;
	int64 ProposedAssignedAtBase = Base->SignalWatchScientists;
	for (const FResearchProjectState& Other : Transaction.ResearchProjects)
	{
		if (Other.BaseId == Project->BaseId)
		{
			const int32 ProposedAssignment = Other.ResearchId == Project->ResearchId
				? Command.AssignedScientists
				: Other.AssignedScientists;
			if (!TryAdd(CurrentAssignedAtBase, Other.AssignedScientists, CurrentAssignedAtBase)
				|| !TryAdd(ProposedAssignedAtBase, ProposedAssignment, ProposedAssignedAtBase))
			{
				AddError(Result, TEXT("scientist_capacity_exceeded"),
					TEXT("Scientist assignments exceed the campaign numeric range."));
				return Result;
			}
		}
	}
	if (ProposedAssignedAtBase > PersonnelCapacity.ScientistCapacity
		&& ProposedAssignedAtBase > CurrentAssignedAtBase)
	{
		AddError(Result, TEXT("scientist_capacity_exceeded"), FString::Printf(
			TEXT("Assignment needs %lld scientists at a base with effective capacity %d; reduce existing overcapacity before increasing staff."),
			ProposedAssignedAtBase, PersonnelCapacity.ScientistCapacity));
		return Result;
	}

	Project->AssignedScientists = Command.AssignedScientists;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ResearchStaffChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Project->BaseId;
	Event.RuleId = Project->ResearchId;
	Event.Quantity = Project->AssignedScientists;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FCancelResearchCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	const FResearchProjectState* Project = FindResearchProject(Transaction, Command.ResearchId);
	if (Project == nullptr)
	{
		AddError(Result, TEXT("unknown_research_project"), TEXT("Research project is not active."));
		return Result;
	}
	const FGuid BaseId = Project->BaseId;
	Transaction.ResearchProjects.RemoveAll(
		[&Command](const FResearchProjectState& Entry) { return Entry.ResearchId == Command.ResearchId; });
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ResearchCancelled,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.RuleId = Command.ResearchId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FAdvanceStrategicTimeCommand& Command)
{
	return Execute(State, Rules, FStrategicSimulationConfig(), Command);
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FAdvanceStrategicTimeCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Strategic time cannot advance after the campaign has concluded."));
		return Result;
	}
	if (!State.TacticalOperations.IsEmpty())
	{
		AddError(Result, TEXT("tactical_operation_pending"), TEXT("Resolve the pending tactical operation before advancing strategic time."));
		return Result;
	}
	if (!State.BaseAssaults.IsEmpty())
	{
		AddError(Result, TEXT("base_assault_pending"), TEXT("Resolve the pending base assault before advancing strategic time."));
		return Result;
	}
	if (HasPendingRecoveryPlan(State))
	{
		AddError(Result, TEXT("personnel_recovery_plan_required"),
			TEXT("Choose a Return Path for every newly injured person before advancing strategic time."));
		return Result;
	}
	const FTimespan RequestedAdvance = FStrategicClock::GetAdvanceForRate(Command.Rate);
	if (Command.Rate == EStrategicTimeRate::Paused || RequestedAdvance <= FTimespan::Zero() || !State.StrategicTime.IsUsable())
	{
		AddError(Result, TEXT("invalid_time_advance"), TEXT("Time command must request a positive supported rate from a usable timestamp."));
		return Result;
	}
	const int64 RequestedSeconds = RequestedAdvance.GetTicks() / ETimespan::TicksPerSecond;
	if (Config.RecoveryHoursPerHealth <= 0
		|| Config.RecoverySurgeCostPerMissingHealth <= 0
		|| Config.RecoverySurgeDurationPercent <= 0 || Config.RecoverySurgeDurationPercent > 100
		|| Config.RecoveryReflectionDurationPercent < 100 || Config.RecoveryReflectionDurationPercent > 1000
		|| Config.RecoveryReflectionResolveBonus <= 0 || Config.RecoveryReflectionResolveBonus > 100
		|| Config.TrainingHours <= 0 || Config.MaxGeneralPersonnelPerBase <= 0
		|| !IsMutualAidRoutingConfigValid(Config)
		|| !FPersonnelStewardship::IsConfigValid(Config))
	{
		AddError(Result, TEXT("invalid_simulation_config"),
			TEXT("Personnel recovery plans, training, stewardship, logistics, and general base-capacity settings must remain within supported positive bounds."));
		return Result;
	}
	if (!ValidateAdversaryConfig(Config, Result))
	{
		return Result;
	}
	if (!State.ManufacturingProjects.IsEmpty()
		&& !FContentPackageResolver::IsValidPackageId(Config.ManufacturingFacilityId))
	{
		AddError(Result, TEXT("invalid_simulation_config"),
			TEXT("Active manufacturing requires a valid fabrication facility id."));
		return Result;
	}

	if (!ValidateFacilityState(State, Rules, Result))
	{
		return Result;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Result))
	{
		return Result;
	}
	int64 MonthlyMaintenance = 0;
	if (!ComputeMonthlyMaintenance(State, Rules, MonthlyMaintenance, Result))
	{
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result))
	{
		return Result;
	}
	if (!ValidateCraftState(State, Rules, Result))
	{
		return Result;
	}
	if (!ValidateStrategicContacts(State, Rules, Result))
	{
		return Result;
	}
	if (!ValidateStrategicSites(State, Rules, Result))
	{
		return Result;
	}
	if (!ValidateTacticalOperations(State, Result))
	{
		return Result;
	}
	if (!ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	if (!ValidateAdversaryState(State, Rules, Config, Result))
	{
		return Result;
	}
	int64 MonthlySalaries = 0;
	if (!ComputeMonthlyPersonnelSalaries(State, Rules, MonthlySalaries, Result))
	{
		return Result;
	}
	int64 MonthlyCraftMaintenance = 0;
	if (!ComputeMonthlyCraftMaintenance(State, Rules, MonthlyCraftMaintenance, Result))
	{
		return Result;
	}
	int64 ValidatedMonthlyOutgoings = 0;
	int64 FacilityAndSalaryCosts = 0;
	if (!TryAdd(MonthlyMaintenance, MonthlySalaries, FacilityAndSalaryCosts)
		|| !TryAdd(FacilityAndSalaryCosts, MonthlyCraftMaintenance, ValidatedMonthlyOutgoings))
	{
		AddError(Result, TEXT("financial_overflow"), TEXT("Combined monthly operating costs exceed the campaign numeric range."));
		return Result;
	}
	static_cast<void>(ValidatedMonthlyOutgoings);
	constexpr int32 MaximumResearchRatePercent = 300;
	constexpr int32 MaximumManufacturingRatePercent = 300;
	for (const FResearchProjectState& Project : State.ResearchProjects)
	{
		const FResearchRule* Research = Rules.Research.Find(Project.ResearchId);
		const FStrategicBaseState* ResearchBase = FindBase(State, Project.BaseId);
		if (Research == nullptr || ResearchBase == nullptr
			|| Project.AssignedScientists < 0 || Project.AccumulatedWorkSeconds < 0)
		{
			AddError(Result, TEXT("invalid_research_project"), FString::Printf(TEXT("Research project '%s' has invalid persisted state."), *Project.ResearchId.ToString()));
			return Result;
		}
		const int32 EffectiveScientists = HasOperationalResearchFacilities(*ResearchBase, Rules, *Research)
			? Project.AssignedScientists : 0;
		int64 MaximumAdditionalWork = 0;
		int64 ScaledMaximumAdditionalWork = 0;
		if (!TryMultiplyNonNegative(EffectiveScientists, RequestedSeconds, MaximumAdditionalWork)
			|| !TryMultiplyNonNegative(MaximumAdditionalWork, MaximumResearchRatePercent,
				ScaledMaximumAdditionalWork)
			|| (ScaledMaximumAdditionalWork / 100) > MAX_int64 - Project.AccumulatedWorkSeconds)
		{
			AddError(Result, TEXT("research_progress_overflow"), TEXT("Research progress would exceed the campaign numeric range."));
			return Result;
		}
	}
	for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
	{
		const FItemRule* Item = Rules.Items.Find(Project.ItemId);
		if (Item == nullptr || !Item->IsManufacturable() || FindBase(State, Project.BaseId) == nullptr
			|| !Project.ProjectId.IsValid() || Project.AssignedEngineers < 0 || Project.UnitsRemaining <= 0 || Project.AccumulatedWorkSeconds < 0)
		{
			AddError(Result, TEXT("invalid_manufacturing_project"), FString::Printf(TEXT("Manufacturing project '%s' has invalid persisted state."), *Project.ProjectId.ToString()));
			return Result;
		}
		int64 MaximumAdditionalWork = 0;
		int64 ScaledMaximumAdditionalWork = 0;
		if (!TryMultiplyNonNegative(Project.AssignedEngineers, RequestedSeconds, MaximumAdditionalWork)
			|| !TryMultiplyNonNegative(MaximumAdditionalWork, MaximumManufacturingRatePercent,
				ScaledMaximumAdditionalWork)
			|| (ScaledMaximumAdditionalWork / 100) > MAX_int64 - Project.AccumulatedWorkSeconds)
		{
			AddError(Result, TEXT("manufacturing_progress_overflow"), TEXT("Manufacturing progress would exceed the campaign numeric range."));
			return Result;
		}
	}
	for (const FFacilityConstructionProjectState& Project : State.FacilityConstructionProjects)
	{
		if (!Project.ProjectId.IsValid() || !Project.FacilityInstanceId.IsValid()
			|| Rules.Facilities.Find(Project.FacilityId) == nullptr
			|| FindBase(State, Project.BaseId) == nullptr
			|| Project.RemainingBuildSeconds <= 0)
		{
			AddError(Result, TEXT("invalid_construction_project"), FString::Printf(TEXT("Facility construction project '%s' has invalid persisted state."), *Project.ProjectId.ToString()));
			return Result;
		}
	}
	int64 PotentialArrivals = 0;
	for (const FRecruitmentOrderState& Order : State.RecruitmentOrders)
	{
		if (Order.RemainingTransitSeconds <= RequestedSeconds)
		{
			++PotentialArrivals;
		}
	}
	int64 MaximumArrivalDraws = 0;
	if (!TryMultiplyNonNegative(PotentialArrivals, 5, MaximumArrivalDraws)
		|| WouldExhaustDeterministicRandomStream(State.SimulationRandom, MaximumArrivalDraws))
	{
		AddError(Result, TEXT("random_draw_overflow"), TEXT("Personnel arrivals would exceed the deterministic random-stream draw range."));
		return Result;
	}
	int64 HiddenContactCount = 0;
	for (const FStrategicContactState& Contact : State.StrategicContacts)
	{
		HiddenContactCount += Contact.Status == EStrategicContactStatus::Hidden ? 1 : 0;
	}
	const int64 MaximumSensorPasses = RequestedSeconds / 3600LL + 1;
	const int64 MaximumAdditionalMissions = Rules.AdversaryMissions.IsEmpty()
		? 0
		: FMath::Max<int64>(0, static_cast<int64>(Config.MaxActiveAdversaryMissions) - State.AdversaryMissions.Num());
	int64 MaximumHiddenContacts = 0;
	if (!TryAdd(HiddenContactCount, MaximumAdditionalMissions, MaximumHiddenContacts))
	{
		AddError(Result, TEXT("random_draw_overflow"), TEXT("Adversary contact count exceeds the deterministic random-stream range."));
		return Result;
	}
	int64 MaximumSensorDraws = 0;
	const int64 MaximumMissionSelectionDraws = Rules.AdversaryMissions.IsEmpty() ? 0 : MaximumSensorPasses + 1;
	int64 MaximumTimeAdvanceDraws = 0;
	if (!TryMultiplyNonNegative(MaximumHiddenContacts, MaximumSensorPasses, MaximumSensorDraws)
		|| !TryAdd(MaximumArrivalDraws, MaximumSensorDraws, MaximumTimeAdvanceDraws)
		|| !TryAdd(MaximumTimeAdvanceDraws, MaximumMissionSelectionDraws, MaximumTimeAdvanceDraws)
		|| WouldExhaustDeterministicRandomStream(State.SimulationRandom, MaximumTimeAdvanceDraws))
	{
		AddError(Result, TEXT("random_draw_overflow"), TEXT("Contact detection or adversary scheduling would exceed the deterministic random-stream draw range."));
		return Result;
	}

	FCampaignState Transaction = State;
	SortStateCollections(Transaction);
	const FCraftServiceQueueSnapshot InitialServiceQueue =
		FCraftServiceQueue::Evaluate(Transaction, Rules);
	TSet<FGuid> ActiveServiceCraftIds;
	ActiveServiceCraftIds.Reserve(InitialServiceQueue.Craft.Num());
	for (const FCraftServiceQueueView& Queue : InitialServiceQueue.Craft)
	{
		if (Queue.bInServiceLane)
		{
			ActiveServiceCraftIds.Add(Queue.CraftId);
		}
	}
	const FMutualAidRelayQueueSnapshot InitialRelayQueue =
		FMutualAidRelayQueue::Evaluate(Transaction, Rules);
	TSet<FGuid> ActiveRelayConvoyIds;
	ActiveRelayConvoyIds.Reserve(InitialRelayQueue.Convoys.Num());
	for (const FMutualAidRelayQueueView& Queue : InitialRelayQueue.Convoys)
	{
		if (Queue.bInTransit)
		{
			ActiveRelayConvoyIds.Add(Queue.ConvoyId);
		}
	}
	const FDateTime PreviousTime = Transaction.StrategicTime.Utc;
	const int64 NextSequence = Transaction.CommandSequence + 1;
	bool bStopRequested = false;
	bool bSimulationFailed = false;
	Result.ExecutedSlices = FStrategicClock::AdvanceRate(
		Transaction.StrategicTime,
		Command.Rate,
		[&](const FStrategicTimeSlice& Slice)
		{
			const int64 SliceSeconds = (Slice.CurrentUtc - Slice.PreviousUtc).GetTicks() / ETimespan::TicksPerSecond;
			const int64 AdversaryMissionSerialAtSliceStart = Transaction.NextAdversaryMissionSerial;
			TSet<FGuid> SiteIdsAtSliceStart;
			SiteIdsAtSliceStart.Reserve(Transaction.StrategicSites.Num());
			for (const FStrategicSiteState& Site : Transaction.StrategicSites)
			{
				SiteIdsAtSliceStart.Add(Site.SiteId);
			}
			for (FStrategicBaseState& Base : Transaction.Bases)
			{
				for (FBaseFacilityState& Facility : Base.Facilities)
				{
					if (Facility.RemainingRepairSeconds <= 0)
					{
						continue;
					}
					Facility.RemainingRepairSeconds -= FMath::Min(Facility.RemainingRepairSeconds, SliceSeconds);
					if (Facility.RemainingRepairSeconds == 0)
					{
						const int32 RepairedDamage = Facility.ReservedRepairDamage;
						Facility.Damage = FMath::Max(0, Facility.Damage - RepairedDamage);
						Facility.ReservedRepairDamage = 0;
						FStrategicEvent& Repaired = AddEvent(Result, EStrategicEventType::FacilityRepaired,
							NextSequence, Slice.CurrentUtc);
						Repaired.BaseId = Base.BaseId;
						Repaired.FacilityInstanceId = Facility.InstanceId;
						Repaired.RuleId = Facility.FacilityId;
						Repaired.Quantity = RepairedDamage;
						bStopRequested = true;
					}
				}
			}
			for (int32 Index = Transaction.ResearchProjects.Num() - 1; Index >= 0; --Index)
			{
				FResearchProjectState& Project = Transaction.ResearchProjects[Index];
				const FResearchRule& Research = Rules.Research.FindChecked(Project.ResearchId);
				const FStrategicBaseState* ResearchBase = FindBase(Transaction, Project.BaseId);
				if (ResearchBase == nullptr
					|| !HasOperationalResearchFacilities(*ResearchBase, Rules, Research))
				{
					continue;
				}
				int64 WorkThisSlice = 0;
				int64 ScaledWorkThisSlice = 0;
				int64 NewProgress = 0;
				const int32 ResearchRatePercent =
					FStrategicCommandService::EvaluateBaseResearchRatePercent(*ResearchBase, Rules);
				if (!TryMultiplyNonNegative(Project.AssignedScientists, SliceSeconds, WorkThisSlice)
					|| !TryMultiplyNonNegative(WorkThisSlice, ResearchRatePercent, ScaledWorkThisSlice)
					|| !TryAdd(Project.AccumulatedWorkSeconds, ScaledWorkThisSlice / 100, NewProgress))
				{
					bSimulationFailed = true;
					bStopRequested = true;
					return;
				}
				Project.AccumulatedWorkSeconds = NewProgress;
				const int64 RequiredWork = static_cast<int64>(Research.Effort) * 3600LL;
				if (Project.AccumulatedWorkSeconds >= RequiredWork)
				{
					if (!Transaction.CompletedResearch.Contains(Project.ResearchId))
					{
						Transaction.CompletedResearch.Add(Project.ResearchId);
					}
					FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ResearchCompleted, NextSequence, Slice.CurrentUtc);
					Event.BaseId = Project.BaseId;
					Event.RuleId = Project.ResearchId;
					Transaction.ResearchProjects.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = true;
				}
			}

			for (int32 Index = Transaction.ManufacturingProjects.Num() - 1; Index >= 0; --Index)
			{
				FManufacturingProjectState& Project = Transaction.ManufacturingProjects[Index];
				const FItemRule& Item = Rules.Items.FindChecked(Project.ItemId);
				const FStrategicBaseState* ManufacturingBase = FindBase(Transaction, Project.BaseId);
				if (ManufacturingBase == nullptr
					|| !HasOperationalFacility(*ManufacturingBase, Rules, Config.ManufacturingFacilityId))
				{
					continue;
				}
				int64 WorkThisSlice = 0;
				int64 ScaledWorkThisSlice = 0;
				int64 NewProgress = 0;
				const int32 ManufacturingRatePercent =
					FStrategicCommandService::EvaluateBaseManufacturingRatePercent(*ManufacturingBase, Rules);
				if (!TryMultiplyNonNegative(Project.AssignedEngineers, SliceSeconds, WorkThisSlice)
					|| !TryMultiplyNonNegative(WorkThisSlice, ManufacturingRatePercent, ScaledWorkThisSlice)
					|| !TryAdd(Project.AccumulatedWorkSeconds, ScaledWorkThisSlice / 100, NewProgress))
				{
					bSimulationFailed = true;
					bStopRequested = true;
					return;
				}
				Project.AccumulatedWorkSeconds = NewProgress;
				const int64 RequiredWork = static_cast<int64>(Item.ManufactureHours) * 3600LL;
				while (Project.UnitsRemaining > 0 && Project.AccumulatedWorkSeconds >= RequiredWork)
				{
					FStrategicBaseState* Base = FindBase(Transaction, Project.BaseId);
					check(Base != nullptr);
					FInventoryStack* Stack = Base->Inventory.FindByPredicate([&Project](const FInventoryStack& Entry) { return Entry.ItemId == Project.ItemId; });
					if (Stack == nullptr)
					{
						Stack = &Base->Inventory.AddDefaulted_GetRef();
						Stack->ItemId = Project.ItemId;
					}
					if (Stack->Quantity == MAX_int32)
					{
						bSimulationFailed = true;
						bStopRequested = true;
						return;
					}
					++Stack->Quantity;
					--Project.UnitsRemaining;
					Project.AccumulatedWorkSeconds -= RequiredWork;
					FStrategicEvent& Produced = AddEvent(Result, EStrategicEventType::ItemManufactured, NextSequence, Slice.CurrentUtc);
					Produced.BaseId = Project.BaseId;
					Produced.ProjectId = Project.ProjectId;
					Produced.RuleId = Project.ItemId;
					Produced.Quantity = 1;
				}
				if (Project.UnitsRemaining == 0)
				{
					FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::ManufacturingCompleted, NextSequence, Slice.CurrentUtc);
					Completed.BaseId = Project.BaseId;
					Completed.ProjectId = Project.ProjectId;
					Completed.RuleId = Project.ItemId;
					Transaction.ManufacturingProjects.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = true;
				}
			}

			for (int32 Index = Transaction.FacilityConstructionProjects.Num() - 1; Index >= 0; --Index)
			{
				FFacilityConstructionProjectState& Project = Transaction.FacilityConstructionProjects[Index];
				Project.RemainingBuildSeconds -= FMath::Min(Project.RemainingBuildSeconds, SliceSeconds);
				if (Project.RemainingBuildSeconds == 0)
				{
					FStrategicBaseState* Base = FindBase(Transaction, Project.BaseId);
					check(Base != nullptr);
					FBaseFacilityState& Facility = Base->Facilities.AddDefaulted_GetRef();
					Facility.InstanceId = Project.FacilityInstanceId;
					Facility.FacilityId = Project.FacilityId;
					Facility.GridX = Project.GridX;
					Facility.GridY = Project.GridY;
					FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::FacilityConstructionCompleted, NextSequence, Slice.CurrentUtc);
					Completed.BaseId = Project.BaseId;
					Completed.ProjectId = Project.ProjectId;
					Completed.RuleId = Project.FacilityId;
					Transaction.FacilityConstructionProjects.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = true;
				}
			}

			TArray<int32> ArrivedConvoyIndices;
			for (int32 Index = 0; Index < Transaction.MutualAidConvoys.Num(); ++Index)
			{
				FMutualAidConvoyState& Convoy = Transaction.MutualAidConvoys[Index];
				if (!ActiveRelayConvoyIds.Contains(Convoy.ConvoyId))
				{
					continue;
				}
				const int64 PreviousRemaining = Convoy.RemainingTransitSeconds;
				Convoy.RemainingTransitSeconds -= FMath::Min(
					Convoy.RemainingTransitSeconds, SliceSeconds);
				const int64 InterdictionCheckpoint = Convoy.TotalTransitSeconds / 2;
				if (!Convoy.bInterdictionResolved
					&& PreviousRemaining > InterdictionCheckpoint
					&& Convoy.RemainingTransitSeconds <= InterdictionCheckpoint)
				{
					Convoy.bInterdictionResolved = true;
					const int64 DelaySeconds = Convoy.ForecastInterdictionDelaySeconds;
					if (!Convoy.bSignalEscort)
					{
						int64 DelayedRemaining = 0;
						if (!TryAdd(Convoy.RemainingTransitSeconds, DelaySeconds, DelayedRemaining)
							|| DelayedRemaining > Convoy.TotalTransitSeconds)
						{
							AddError(Result, TEXT("mutual_aid_interdiction_overflow"),
								TEXT("A Mutual Aid interdiction delay exceeds the supported route clock."));
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
						Convoy.RemainingTransitSeconds = DelayedRemaining;
						Convoy.InterdictionDelaySeconds = DelaySeconds;
					}
					FStrategicEvent& Interdiction = AddEvent(
						Result,
						Convoy.bSignalEscort
							? EStrategicEventType::MutualAidConvoyEscorted
							: EStrategicEventType::MutualAidConvoyInterdicted,
						NextSequence, Slice.CurrentUtc);
					Interdiction.BaseId = MutualAidCurrentLegOriginBaseId(Convoy);
					Interdiction.RelatedBaseId = Convoy.RelayWaypointBaseId.IsValid()
						? Convoy.RelayWaypointBaseId
						: Convoy.DestinationBaseId;
					Interdiction.ConvoyId = Convoy.ConvoyId;
					Interdiction.RuleId = Convoy.ItemId;
					Interdiction.PolicyId = MutualAidRoutePolicyId(Convoy.RoutePolicy);
					Interdiction.Quantity = Convoy.Quantity;
					Interdiction.Amount = Convoy.bSignalEscort ? 0 : DelaySeconds;
					Interdiction.ConvoyRoutePressure = Convoy.RoutePressure;
					Interdiction.ConvoyTransitSeconds = Convoy.TotalTransitSeconds;
					Interdiction.ConvoyEscortCost = Convoy.SignalEscortCost;
					Interdiction.ConvoyDelaySeconds =
						Convoy.bSignalEscort ? 0 : DelaySeconds;
					Interdiction.bConvoySignalEscort = Convoy.bSignalEscort;
					if (const FMutualAidRelayQueueView* Relay =
						InitialRelayQueue.FindConvoy(Convoy.ConvoyId))
					{
						Interdiction.ConvoyRelayChannelCount = Relay->RelayChannelCount;
						Interdiction.SignalWatchFacilityChannelCount = Relay->FacilityRelayChannelCount;
						Interdiction.SignalWatchAssignedScientists = Relay->SignalWatchScientistCount;
						Interdiction.SignalWatchBonusChannelCount = Relay->SignalWatchBonusChannelCount;
						Interdiction.SignalWatchTotalChannelCount = Relay->RelayChannelCount;
						Interdiction.ConvoyRelayQueuePosition = Relay->QueuePosition;
						Interdiction.ConvoyRelayChannelNumber = Relay->RelayChannelNumber;
						Interdiction.bConvoyRelayActive = Relay->bInTransit;
						Interdiction.ConvoyRelayWaitSeconds = Relay->EstimatedWaitSeconds;
						Interdiction.ConvoyEstimatedArrivalSeconds =
							Relay->EstimatedArrivalSeconds;
					}
					bStopRequested = true;
				}
				if (Convoy.RemainingTransitSeconds != 0)
				{
					continue;
				}
				if (Convoy.RelayWaypointBaseId.IsValid())
				{
					const FGuid PreviousLegOriginBaseId =
						MutualAidCurrentLegOriginBaseId(Convoy);
					const FGuid WaypointBaseId = Convoy.RelayWaypointBaseId;
					const EMutualAidRoutePolicy PreviousRoutePolicy = Convoy.RoutePolicy;
					const int64 PreviousTransitSeconds = Convoy.TotalTransitSeconds;
					const int32 PreviousRoutePressure = Convoy.RoutePressure;
					const int64 PreviousDelaySeconds = Convoy.InterdictionDelaySeconds;
					const EMutualAidRoutePolicy OnwardRoutePolicy =
						Convoy.OnwardRoutePolicy;
					const int64 OnwardTransitSeconds = Convoy.OnwardTotalTransitSeconds;
					const int32 OnwardRoutePressure = Convoy.OnwardRoutePressure;
					const bool bOnwardInterdictionResolved =
						Convoy.bOnwardInterdictionResolved;
					const int64 OnwardForecastDelaySeconds =
						Convoy.OnwardForecastInterdictionDelaySeconds;
					const int32 HandoffQuantity = Convoy.BalancedHandoffQuantity;
					const int32 FinalQuantity = Convoy.Quantity - HandoffQuantity;
					int64 HandoffStorage = 0;
					if (HandoffQuantity > 0)
					{
						FStrategicBaseState* Waypoint = FindBase(Transaction, WaypointBaseId);
						const FItemRule* Item = Rules.Items.Find(Convoy.ItemId);
						if (Waypoint == nullptr || Item == nullptr || Item->Mass < 0
							|| !TryMultiplyNonNegative(
								Item->Mass, HandoffQuantity, HandoffStorage)
							|| !TryAdjustInventory(
								*Waypoint, Convoy.ItemId, HandoffQuantity))
						{
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
					}

					FStrategicEvent& Reached = AddEvent(
						Result, EStrategicEventType::MutualAidConvoyRelayWaypointReached,
						NextSequence, Slice.CurrentUtc);
					Reached.BaseId = PreviousLegOriginBaseId;
					Reached.RelatedBaseId = Convoy.DestinationBaseId;
					Reached.ConvoyId = Convoy.ConvoyId;
					Reached.RuleId = Convoy.ItemId;
					Reached.PolicyId = TEXT("logistics.relay-waypoint");
					Reached.PreviousPolicyId =
						MutualAidRoutePolicyId(PreviousRoutePolicy);
					Reached.Quantity = Convoy.Quantity;
					Reached.Amount = PreviousDelaySeconds;
					Reached.ConvoyPreviousRoutePressure = PreviousRoutePressure;
					Reached.ConvoyRoutePressure = OnwardRoutePressure;
					Reached.ConvoyPreviousTransitSeconds = PreviousTransitSeconds;
					Reached.ConvoyTransitSeconds = OnwardTransitSeconds;
					Reached.ConvoyEscortCost = Convoy.SignalEscortCost;
					Reached.ConvoyDelaySeconds = PreviousDelaySeconds;
					Reached.bConvoySignalEscort = Convoy.bSignalEscort;
					Reached.ConvoyDispatchSequence = Convoy.DispatchSequence;
					Reached.ConvoyRelayWaypointBaseId = WaypointBaseId;
					Reached.ConvoyOnwardPolicyId =
						MutualAidRoutePolicyId(OnwardRoutePolicy);
					Reached.ConvoyOnwardRoutePressure = OnwardRoutePressure;
					Reached.ConvoyOnwardTransitSeconds = OnwardTransitSeconds;
					Reached.ConvoyHandoffQuantity = HandoffQuantity;
					Reached.ConvoyFinalDeliveryQuantity = FinalQuantity;
					Reached.ConvoyHandoffStorage = HandoffStorage;
					if (const FMutualAidRelayQueueView* Relay =
						InitialRelayQueue.FindConvoy(Convoy.ConvoyId))
					{
						Reached.ConvoyRelayChannelCount = Relay->RelayChannelCount;
						Reached.SignalWatchFacilityChannelCount =
							Relay->FacilityRelayChannelCount;
						Reached.SignalWatchAssignedScientists =
							Relay->SignalWatchScientistCount;
						Reached.SignalWatchBonusChannelCount =
							Relay->SignalWatchBonusChannelCount;
						Reached.SignalWatchTotalChannelCount =
							Relay->RelayChannelCount;
						Reached.ConvoyRelayQueuePosition = Relay->QueuePosition;
						Reached.ConvoyRelayChannelNumber = Relay->RelayChannelNumber;
						Reached.bConvoyRelayActive = Relay->bInTransit;
						Reached.ConvoyRelayWaitSeconds = Relay->EstimatedWaitSeconds;
						Reached.ConvoyEstimatedArrivalSeconds =
							Relay->EstimatedArrivalSeconds;
					}
					Reached.bSuccessful = true;

					Convoy.Quantity = FinalQuantity;
					Convoy.CurrentLegOriginBaseId = WaypointBaseId;
					Convoy.RoutePolicy = OnwardRoutePolicy;
					Convoy.TotalTransitSeconds = OnwardTransitSeconds;
					Convoy.RemainingTransitSeconds = OnwardTransitSeconds;
					Convoy.RoutePressure = OnwardRoutePressure;
					Convoy.bInterdictionResolved = bOnwardInterdictionResolved;
					Convoy.ForecastInterdictionDelaySeconds =
						OnwardForecastDelaySeconds;
					Convoy.InterdictionDelaySeconds = 0;
					ClearMutualAidOnwardRoute(Convoy);
					if (HandoffQuantity > 0)
					{
						FStrategicEvent& Delivered = AddEvent(
							Result,
							EStrategicEventType::MutualAidConvoyBalancedHandoffDelivered,
							NextSequence, Slice.CurrentUtc);
						Delivered.BaseId = WaypointBaseId;
						Delivered.RelatedBaseId = Convoy.DestinationBaseId;
						Delivered.ConvoyId = Convoy.ConvoyId;
						Delivered.RuleId = Convoy.ItemId;
						Delivered.PolicyId =
							TEXT("logistics.mutual-aid-balanced-handoff");
						Delivered.Quantity = HandoffQuantity;
						Delivered.Amount = HandoffStorage;
						Delivered.ConvoyDispatchSequence = Convoy.DispatchSequence;
						Delivered.ConvoyRelayWaypointBaseId = WaypointBaseId;
						Delivered.ConvoyHandoffQuantity = HandoffQuantity;
						Delivered.ConvoyFinalDeliveryQuantity = FinalQuantity;
						Delivered.ConvoyHandoffStorage = HandoffStorage;
						const FBaseStorageEvaluation WaypointStorage = EvaluateBaseStorage(
							Transaction, Rules, WaypointBaseId);
						const FBaseStorageEvaluation DestinationStorage = EvaluateBaseStorage(
							Transaction, Rules, Convoy.DestinationBaseId);
						Delivered.ConvoyWaypointReservedStorage =
							WaypointStorage.MutualAidReserved;
						Delivered.ConvoyDestinationReservedStorage =
							DestinationStorage.MutualAidReserved;
						if (const FMutualAidRelayQueueView* Relay =
							InitialRelayQueue.FindConvoy(Convoy.ConvoyId))
						{
							Delivered.ConvoyRelayChannelCount = Relay->RelayChannelCount;
							Delivered.SignalWatchFacilityChannelCount =
								Relay->FacilityRelayChannelCount;
							Delivered.SignalWatchAssignedScientists =
								Relay->SignalWatchScientistCount;
							Delivered.SignalWatchBonusChannelCount =
								Relay->SignalWatchBonusChannelCount;
							Delivered.SignalWatchTotalChannelCount =
								Relay->RelayChannelCount;
							Delivered.ConvoyRelayQueuePosition = Relay->QueuePosition;
							Delivered.ConvoyRelayChannelNumber = Relay->RelayChannelNumber;
							Delivered.bConvoyRelayActive = Relay->bInTransit;
							Delivered.ConvoyRelayWaitSeconds = Relay->EstimatedWaitSeconds;
							Delivered.ConvoyEstimatedArrivalSeconds =
								Relay->EstimatedArrivalSeconds;
						}
						Delivered.bSuccessful = true;
					}
					bStopRequested = true;
					continue;
				}
				FStrategicBaseState* Destination = FindBase(Transaction, Convoy.DestinationBaseId);
				if (Destination == nullptr
					|| !TryAdjustInventory(*Destination, Convoy.ItemId, Convoy.Quantity))
				{
					bSimulationFailed = true;
					bStopRequested = true;
					return;
				}
				FStrategicEvent& Arrived = AddEvent(
					Result, EStrategicEventType::MutualAidConvoyArrived,
					NextSequence, Slice.CurrentUtc);
				Arrived.BaseId = MutualAidCurrentLegOriginBaseId(Convoy);
				Arrived.RelatedBaseId = Convoy.DestinationBaseId;
				Arrived.ConvoyId = Convoy.ConvoyId;
				Arrived.RuleId = Convoy.ItemId;
				Arrived.PolicyId = MutualAidRoutePolicyId(Convoy.RoutePolicy);
				Arrived.Quantity = Convoy.Quantity;
				Arrived.ConvoyRoutePressure = Convoy.RoutePressure;
				Arrived.ConvoyTransitSeconds = Convoy.TotalTransitSeconds;
				Arrived.ConvoyDelaySeconds = Convoy.InterdictionDelaySeconds;
				Arrived.ConvoyEscortCost = Convoy.SignalEscortCost;
				Arrived.bConvoySignalEscort = Convoy.bSignalEscort;
				if (const FMutualAidRelayQueueView* Relay =
					InitialRelayQueue.FindConvoy(Convoy.ConvoyId))
				{
					Arrived.ConvoyRelayChannelCount = Relay->RelayChannelCount;
					Arrived.SignalWatchFacilityChannelCount = Relay->FacilityRelayChannelCount;
					Arrived.SignalWatchAssignedScientists = Relay->SignalWatchScientistCount;
					Arrived.SignalWatchBonusChannelCount = Relay->SignalWatchBonusChannelCount;
					Arrived.SignalWatchTotalChannelCount = Relay->RelayChannelCount;
					Arrived.ConvoyRelayQueuePosition = Relay->QueuePosition;
					Arrived.ConvoyRelayChannelNumber = Relay->RelayChannelNumber;
					Arrived.bConvoyRelayActive = Relay->bInTransit;
					Arrived.ConvoyRelayWaitSeconds = Relay->EstimatedWaitSeconds;
					Arrived.ConvoyEstimatedArrivalSeconds = 0;
				}
				ArrivedConvoyIndices.Add(Index);
				bStopRequested = true;
			}
			for (int32 Index = ArrivedConvoyIndices.Num() - 1; Index >= 0; --Index)
			{
				Transaction.MutualAidConvoys.RemoveAt(
					ArrivedConvoyIndices[Index], EAllowShrinking::No);
			}

			for (int32 Index = Transaction.RecruitmentOrders.Num() - 1; Index >= 0; --Index)
			{
				FRecruitmentOrderState& Order = Transaction.RecruitmentOrders[Index];
				Order.RemainingTransitSeconds -= FMath::Min(Order.RemainingTransitSeconds, SliceSeconds);
				if (Order.RemainingTransitSeconds == 0)
				{
					const FPersonnelRoleRule& Role = Rules.PersonnelRoles.FindChecked(Order.RoleId);
					FPersonnelState& Person = Transaction.Personnel.AddDefaulted_GetRef();
					Person.PersonnelId = Order.PersonnelId;
					Person.DisplayName = Order.DisplayName;
					Person.RoleId = Order.RoleId;
					Person.BaseId = Order.BaseId;
					Person.MaxHealth = FMath::Clamp(Role.BaseHealth + Transaction.SimulationRandom.NextIntInclusive(-5, 5), 1, 200);
					Person.CurrentHealth = Person.MaxHealth;
					Person.Accuracy = FMath::Clamp(Role.BaseAccuracy + Transaction.SimulationRandom.NextIntInclusive(-5, 5), 1, 100);
					Person.Resolve = FMath::Clamp(Role.BaseResolve + Transaction.SimulationRandom.NextIntInclusive(-5, 5), 1, 100);
					Person.Mobility = FMath::Clamp(Role.BaseMobility + Transaction.SimulationRandom.NextIntInclusive(-5, 5), 1, 100);
					Person.Strength = FMath::Clamp(Role.BaseStrength + Transaction.SimulationRandom.NextIntInclusive(-5, 5), 1, 100);
					FStrategicEvent& Arrived = AddEvent(Result, EStrategicEventType::PersonnelArrived, NextSequence, Slice.CurrentUtc);
					Arrived.BaseId = Order.BaseId;
					Arrived.ProjectId = Order.OrderId;
					Arrived.PersonnelId = Order.PersonnelId;
					Arrived.RuleId = Order.RoleId;
					Transaction.RecruitmentOrders.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = true;
				}
			}

			for (FPersonnelState& Person : Transaction.Personnel)
			{
				if (Person.Status == EPersonnelStatus::Recovering)
				{
					Person.RemainingRecoverySeconds -= FMath::Min(Person.RemainingRecoverySeconds, SliceSeconds);
					if (Person.RemainingRecoverySeconds == 0)
					{
						const EPersonnelRecoveryPlan CompletedPlan = Person.RecoveryPlan;
						const int32 ResolveBonus = CompletedPlan == EPersonnelRecoveryPlan::ReflectionCycle
							? FMath::Min(Config.RecoveryReflectionResolveBonus, 100 - Person.Resolve)
							: 0;
						Person.Resolve += ResolveBonus;
						Person.CurrentHealth = Person.MaxHealth;
						Person.Status = EPersonnelStatus::Available;
						Person.RecoveryPlan = EPersonnelRecoveryPlan::None;
						FStrategicEvent& Recovered = AddEvent(Result, EStrategicEventType::PersonnelRecovered, NextSequence, Slice.CurrentUtc);
						Recovered.BaseId = Person.BaseId;
						Recovered.PersonnelId = Person.PersonnelId;
						Recovered.RuleId = Person.RoleId;
						Recovered.PolicyId = FPersonnelRecoveryPlan::PolicyId(CompletedPlan);
						Recovered.PersonnelRecoveryResolveBonus = ResolveBonus;
						bStopRequested = true;
					}
				}
				else if (Person.Status == EPersonnelStatus::Training)
				{
					Person.RemainingTrainingSeconds -= FMath::Min(Person.RemainingTrainingSeconds, SliceSeconds);
					if (Person.RemainingTrainingSeconds == 0)
					{
						switch (Person.TrainingFocus)
						{
						case EPersonnelTrainingFocus::Accuracy:
							Person.Accuracy = FMath::Min(Person.Accuracy + 1, 100);
							break;
						case EPersonnelTrainingFocus::Resolve:
							Person.Resolve = FMath::Min(Person.Resolve + 1, 100);
							break;
						case EPersonnelTrainingFocus::Mobility:
							Person.Mobility = FMath::Min(Person.Mobility + 1, 100);
							break;
						case EPersonnelTrainingFocus::Strength:
							Person.Strength = FMath::Min(Person.Strength + 1, 100);
							break;
						default:
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
						Person.Status = EPersonnelStatus::Available;
						FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::PersonnelTrainingCompleted, NextSequence, Slice.CurrentUtc);
						Completed.BaseId = Person.BaseId;
						Completed.PersonnelId = Person.PersonnelId;
						Completed.RuleId = Person.RoleId;
						Completed.Quantity = static_cast<int32>(Person.TrainingFocus);
						bStopRequested = true;
					}
				}
				else if (Person.Status == EPersonnelStatus::Stewarding)
				{
					Person.RemainingStewardshipSeconds -= FMath::Min(
						Person.RemainingStewardshipSeconds, SliceSeconds);
					if (Person.RemainingStewardshipSeconds == 0)
					{
						const EPersonnelStewardshipFocus CompletedFocus = Person.StewardshipFocus;
						const int32 ResolveBonus = Person.StewardshipToursCompleted < Config.StewardshipResolveAwardTourCap
							? FMath::Min(Config.StewardshipResolveBonus, 100 - Person.Resolve)
							: 0;
						if (Person.StewardshipToursCompleted < MAX_int32)
						{
							++Person.StewardshipToursCompleted;
						}
						Person.Resolve += ResolveBonus;
						Person.Status = EPersonnelStatus::Available;
						Person.StewardshipFocus = EPersonnelStewardshipFocus::None;
						FStrategicEvent& Completed = AddEvent(Result,
							EStrategicEventType::PersonnelStewardshipCompleted, NextSequence, Slice.CurrentUtc);
						Completed.BaseId = Person.BaseId;
						Completed.PersonnelId = Person.PersonnelId;
						Completed.RuleId = Person.RoleId;
						Completed.PolicyId = FPersonnelStewardship::PolicyId(CompletedFocus);
						Completed.PersonnelResolveBonus = ResolveBonus;
						Completed.PersonnelStewardshipFocus = static_cast<int32>(CompletedFocus);
						Completed.PersonnelStewardshipToursCompleted = Person.StewardshipToursCompleted;
						Completed.PersonnelStewardshipDurationSeconds =
							static_cast<int64>(Config.StewardshipDurationDays) * 86400LL;
						bStopRequested = true;
					}
				}
			}

			for (int32 Index = Transaction.CraftAcquisitionOrders.Num() - 1; Index >= 0; --Index)
			{
				FCraftAcquisitionOrderState& Order = Transaction.CraftAcquisitionOrders[Index];
				Order.RemainingTransitSeconds -= FMath::Min(Order.RemainingTransitSeconds, SliceSeconds);
				if (Order.RemainingTransitSeconds == 0)
				{
					const FCraftRule& Rule = Rules.Craft.FindChecked(Order.CraftRuleId);
					FCraftState& Craft = Transaction.Craft.AddDefaulted_GetRef();
					Craft.CraftId = Order.CraftId;
					Craft.DisplayName = Order.DisplayName;
					Craft.CraftRuleId = Order.CraftRuleId;
					Craft.BaseId = Order.BaseId;
					Craft.CurrentHull = Rule.MaxHull;
					Craft.CurrentFuel = Rule.FuelCapacity;
					FStrategicEvent& Arrived = AddEvent(Result, EStrategicEventType::CraftArrived, NextSequence, Slice.CurrentUtc);
					Arrived.BaseId = Order.BaseId;
					Arrived.ProjectId = Order.OrderId;
					Arrived.CraftId = Order.CraftId;
					Arrived.RuleId = Order.CraftRuleId;
					Transaction.CraftAcquisitionOrders.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = true;
				}
			}

			for (FCraftState& Craft : Transaction.Craft)
			{
				if (Craft.Status != ECraftStatus::Servicing
					|| !ActiveServiceCraftIds.Contains(Craft.CraftId))
				{
					continue;
				}
				const FCraftRule& Rule = Rules.Craft.FindChecked(Craft.CraftRuleId);
				if (Craft.RemainingRepairSeconds > 0)
				{
					Craft.RemainingRepairSeconds -= FMath::Min(Craft.RemainingRepairSeconds, SliceSeconds);
					if (Craft.RemainingRepairSeconds == 0)
					{
						Craft.CurrentHull = Rule.MaxHull;
						FStrategicEvent& Repaired = AddEvent(Result, EStrategicEventType::CraftRepaired, NextSequence, Slice.CurrentUtc);
						Repaired.BaseId = Craft.BaseId;
						Repaired.CraftId = Craft.CraftId;
						Repaired.RuleId = Craft.CraftRuleId;
					}
				}
				if (Craft.RemainingRefuelSeconds > 0)
				{
					Craft.RemainingRefuelSeconds -= FMath::Min(Craft.RemainingRefuelSeconds, SliceSeconds);
					if (Craft.RemainingRefuelSeconds == 0)
					{
						Craft.CurrentFuel = Rule.FuelCapacity;
						FStrategicEvent& Refueled = AddEvent(Result, EStrategicEventType::CraftRefueled, NextSequence, Slice.CurrentUtc);
						Refueled.BaseId = Craft.BaseId;
						Refueled.CraftId = Craft.CraftId;
						Refueled.RuleId = Craft.CraftRuleId;
					}
				}
				if (Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
				{
					Craft.Status = ECraftStatus::Grounded;
					FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::CraftServiceCompleted, NextSequence, Slice.CurrentUtc);
					Completed.BaseId = Craft.BaseId;
					Completed.CraftId = Craft.CraftId;
					Completed.RuleId = Craft.CraftRuleId;
					bStopRequested = true;
				}
			}

			for (FCraftState& Craft : Transaction.Craft)
			{
				if (Craft.Status == ECraftStatus::Intercepting)
				{
					Craft.RemainingRouteSeconds -= FMath::Min(Craft.RemainingRouteSeconds, SliceSeconds);
					if (Craft.RemainingRouteSeconds == 0)
					{
						FStrategicContactState* Contact = FindContact(Transaction, Craft.TargetContactId);
						if (Contact == nullptr || Contact->Status == EStrategicContactStatus::Hidden)
						{
							const FGuid LostContactId = Craft.TargetContactId;
							Craft.Status = ECraftStatus::Returning;
							Craft.TargetContactId.Invalidate();
							Craft.RemainingRouteSeconds = Craft.ReservedReturnSeconds;
							FStrategicEvent& Lost = AddEvent(Result, EStrategicEventType::InterceptionLost, NextSequence, Slice.CurrentUtc);
							Lost.BaseId = Craft.BaseId;
							Lost.CraftId = Craft.CraftId;
							Lost.ContactId = LostContactId;
							Lost.RuleId = Craft.CraftRuleId;
							FStrategicEvent& Returning = AddEvent(Result, EStrategicEventType::CraftReturnStarted, NextSequence, Slice.CurrentUtc);
							Returning.BaseId = Craft.BaseId;
							Returning.CraftId = Craft.CraftId;
							Returning.ContactId = LostContactId;
							Returning.RuleId = Craft.CraftRuleId;
						}
						else
						{
							Craft.Status = ECraftStatus::Airborne;
							Contact->Status = EStrategicContactStatus::Engaged;
							FStrategicEvent& Ready = AddEvent(Result, EStrategicEventType::InterceptionReady, NextSequence, Slice.CurrentUtc);
							Ready.BaseId = Craft.BaseId;
							Ready.CraftId = Craft.CraftId;
							Ready.ContactId = Contact->ContactId;
							Ready.RuleId = Contact->ContactRuleId;
							bStopRequested = true;
						}
					}
				}
				else if (Craft.Status == ECraftStatus::Deploying)
				{
					Craft.RemainingRouteSeconds -= FMath::Min(Craft.RemainingRouteSeconds, SliceSeconds);
					if (Craft.RemainingRouteSeconds == 0)
					{
						const FStrategicSiteState* Site = FindSite(Transaction, Craft.TargetSiteId);
						const FGuid OperationId = MakeDeterministicTacticalOperationId(Craft.CraftId, Craft.TargetSiteId, NextSequence);
						if (Site == nullptr || !OperationId.IsValid() || FindTacticalOperation(Transaction, OperationId) != nullptr)
						{
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
						const uint64 TacticalSalt = (static_cast<uint64>(OperationId.A) << 32)
							^ OperationId.B ^ (static_cast<uint64>(OperationId.C) << 16) ^ OperationId.D;
						const FDeterministicRandomStream TacticalStream = Transaction.SimulationRandom.Fork(TacticalSalt);
						Craft.Status = ECraftStatus::OnSite;
						FTacticalOperationState& Operation = Transaction.TacticalOperations.AddDefaulted_GetRef();
						Operation.OperationId = OperationId;
						Operation.SiteId = Craft.TargetSiteId;
						Operation.CraftId = Craft.CraftId;
						Operation.TacticalSeed = TacticalStream.InitialSeed;
						Operation.CreatedUtc = Slice.CurrentUtc;
						Operation.AgentIds = Craft.AssignedAgentIds;
						Operation.Cargo = Craft.Cargo;
						FStrategicEvent& Ready = AddEvent(Result, EStrategicEventType::TacticalOperationReady, NextSequence, Slice.CurrentUtc);
						Ready.BaseId = Craft.BaseId;
						Ready.CraftId = Craft.CraftId;
						Ready.SiteId = Craft.TargetSiteId;
						Ready.OperationId = OperationId;
						Ready.RuleId = Site->SourceContactRuleId;
						Ready.Quantity = Craft.AssignedAgentIds.Num();
						bStopRequested = true;
					}
				}
				else if (Craft.Status == ECraftStatus::Returning)
				{
					Craft.RemainingRouteSeconds -= FMath::Min(Craft.RemainingRouteSeconds, SliceSeconds);
					if (Craft.RemainingRouteSeconds == 0)
					{
						FPersonnelState* Pilot = FindPersonnel(Transaction, Craft.AssignedPilotId);
						if (Pilot == nullptr || Pilot->Status != EPersonnelStatus::Deployed || Craft.CompletedSorties == MAX_int32)
						{
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
						Craft.Status = ECraftStatus::Grounded;
						Craft.ReservedReturnSeconds = 0;
						++Craft.CompletedSorties;
						Pilot->Status = EPersonnelStatus::Available;
						for (const FGuid& AgentId : Craft.AssignedAgentIds)
						{
							FPersonnelState* Agent = FindPersonnel(Transaction, AgentId);
							if (Agent == nullptr || Agent->Status != EPersonnelStatus::Deployed)
							{
								bSimulationFailed = true;
								bStopRequested = true;
								return;
							}
							if (Agent->CurrentHealth < Agent->MaxHealth)
							{
								int64 RecoveryHours = 0;
								int64 RecoverySeconds = 0;
								const int64 MissingHealth = static_cast<int64>(Agent->MaxHealth) - static_cast<int64>(Agent->CurrentHealth);
								if (!TryMultiplyNonNegative(MissingHealth, Config.RecoveryHoursPerHealth, RecoveryHours)
									|| !TryMultiplyNonNegative(RecoveryHours, 3600, RecoverySeconds)
									|| RecoverySeconds <= 0)
								{
									bSimulationFailed = true;
									bStopRequested = true;
									return;
								}
								Agent->Status = EPersonnelStatus::Recovering;
								Agent->RemainingRecoverySeconds = RecoverySeconds;
								Agent->RecoveryPlan = EPersonnelRecoveryPlan::DecisionRequired;
							}
							else
							{
								Agent->Status = EPersonnelStatus::Available;
								Agent->RecoveryPlan = EPersonnelRecoveryPlan::None;
							}
						}
						FStrategicEvent& Recovered = AddEvent(Result, EStrategicEventType::CraftRecovered, NextSequence, Slice.CurrentUtc);
						Recovered.BaseId = Craft.BaseId;
						Recovered.CraftId = Craft.CraftId;
						Recovered.PersonnelId = Pilot->PersonnelId;
						Recovered.RuleId = Craft.CraftRuleId;
						Recovered.Quantity = Craft.CompletedSorties;
						bStopRequested = true;
					}
				}
			}

			for (int32 Index = Transaction.StrategicContacts.Num() - 1; Index >= 0; --Index)
			{
				FStrategicContactState& Contact = Transaction.StrategicContacts[Index];
				Contact.ElapsedRouteSeconds += FMath::Min(Contact.TotalRouteSeconds - Contact.ElapsedRouteSeconds, SliceSeconds);
				ComputeContactPosition(Contact, Contact.ElapsedRouteSeconds, Contact.LongitudeMilliDegrees, Contact.LatitudeMilliDegrees);
				if (Contact.ElapsedRouteSeconds == Contact.TotalRouteSeconds)
				{
					const FGuid EscapedContactId = Contact.ContactId;
					const FName ContactRuleId = Contact.ContactRuleId;
					const bool bWasVisible = Contact.Status != EStrategicContactStatus::Hidden;
					const FAdversaryMissionState* AdversaryMission = FindAdversaryMission(Transaction, EscapedContactId);
					const bool bWasAdversaryMission = AdversaryMission != nullptr;
					const FGuid AdversaryMissionId = AdversaryMission != nullptr ? AdversaryMission->MissionId : FGuid();
					const FAdversaryMissionRule* AdversaryRule = AdversaryMission != nullptr
						? Rules.AdversaryMissions.Find(AdversaryMission->MissionRuleId)
						: nullptr;
					if (AdversaryMission != nullptr && AdversaryRule != nullptr && AdversaryRule->bTargetsPlayerBase)
					{
						const FStrategicBaseState* TargetBase = FindBase(Transaction, AdversaryMission->TargetBaseId);
						const FGuid AssaultId = MakeDeterministicBaseAssaultId(AdversaryMission->MissionId, AdversaryMission->TargetBaseId);
						if (TargetBase == nullptr || !AssaultId.IsValid()
							|| FindBaseAssault(Transaction, AssaultId) != nullptr
							|| Transaction.BaseAssaults.ContainsByPredicate(
								[&Contact](const FBaseAssaultState& Assault) { return Assault.ContactId == Contact.ContactId; }))
						{
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
						Contact.Status = EStrategicContactStatus::Detected;
						for (FCraftState& Craft : Transaction.Craft)
						{
							if (Craft.TargetContactId != Contact.ContactId
								|| (Craft.Status != ECraftStatus::Intercepting && Craft.Status != ECraftStatus::Airborne))
							{
								continue;
							}
							Craft.Status = ECraftStatus::Returning;
							Craft.TargetContactId.Invalidate();
							Craft.RemainingRouteSeconds = Craft.ReservedReturnSeconds;
							FStrategicEvent& Lost = AddEvent(Result, EStrategicEventType::InterceptionLost, NextSequence, Slice.CurrentUtc);
							Lost.BaseId = Craft.BaseId;
							Lost.CraftId = Craft.CraftId;
							Lost.ContactId = Contact.ContactId;
							Lost.RuleId = ContactRuleId;
							FStrategicEvent& Returning = AddEvent(Result, EStrategicEventType::CraftReturnStarted, NextSequence, Slice.CurrentUtc);
							Returning.BaseId = Craft.BaseId;
							Returning.CraftId = Craft.CraftId;
							Returning.ContactId = Contact.ContactId;
							Returning.RuleId = Craft.CraftRuleId;
						}
						FBaseAssaultState& Assault = Transaction.BaseAssaults.AddDefaulted_GetRef();
						Assault.AssaultId = AssaultId;
						Assault.MissionId = AdversaryMission->MissionId;
						Assault.ContactId = Contact.ContactId;
						Assault.BaseId = TargetBase->BaseId;
						Assault.ArrivedUtc = Slice.CurrentUtc;
						if (!bWasVisible)
						{
							const FContactRule& ContactRule = Rules.Contacts.FindChecked(ContactRuleId);
							FStrategicEvent& Detected = AddEvent(Result, EStrategicEventType::StrategicContactDetected, NextSequence, Slice.CurrentUtc);
							Detected.BaseId = TargetBase->BaseId;
							Detected.ContactId = Contact.ContactId;
							Detected.MissionId = AdversaryMission->MissionId;
							Detected.AssaultId = AssaultId;
							Detected.RuleId = ContactRuleId;
							Detected.Quantity = ContactRule.ThreatRating;
						}
						FStrategicEvent& Started = AddEvent(Result, EStrategicEventType::BaseAssaultStarted, NextSequence, Slice.CurrentUtc);
						Started.BaseId = TargetBase->BaseId;
						Started.ContactId = Contact.ContactId;
						Started.MissionId = AdversaryMission->MissionId;
						Started.AssaultId = AssaultId;
						Started.RuleId = AdversaryMission->MissionRuleId;
						Started.RegionId = TargetBase->RegionId;
						Started.Quantity = AdversaryRule->BaseFacilitiesHit;
						bStopRequested = true;
						continue;
					}
					const FContactRule* ArrivalContactRule = Rules.Contacts.Find(ContactRuleId);
					const bool bCreatesLandingSite = bWasVisible && AdversaryRule != nullptr
						&& AdversaryRule->bCreatesLandingSiteOnArrival && ArrivalContactRule != nullptr;
					const int32 ArrivalLongitude = Contact.LongitudeMilliDegrees;
					const int32 ArrivalLatitude = Contact.LatitudeMilliDegrees;
					if (bCreatesLandingSite)
					{
						FStrategicEvent& Landed = AddEvent(Result, EStrategicEventType::StrategicContactLanded, NextSequence, Slice.CurrentUtc);
						Landed.ContactId = EscapedContactId;
						Landed.MissionId = AdversaryMissionId;
						Landed.RuleId = ContactRuleId;
						Landed.RegionId = AdversaryRule->TargetRegionId;
						Landed.Quantity = ArrivalContactRule->ThreatRating + AdversaryRule->LandingSiteThreatBonus;
					}
					else
					{
						FStrategicEvent& Escaped = AddEvent(Result, EStrategicEventType::StrategicContactEscaped, NextSequence, Slice.CurrentUtc);
						Escaped.ContactId = EscapedContactId;
						Escaped.RuleId = ContactRuleId;
					}
					for (FCraftState& Craft : Transaction.Craft)
					{
						if (Craft.TargetContactId != EscapedContactId
							|| (Craft.Status != ECraftStatus::Intercepting && Craft.Status != ECraftStatus::Airborne))
						{
							continue;
						}
						Craft.Status = ECraftStatus::Returning;
						Craft.TargetContactId.Invalidate();
						Craft.RemainingRouteSeconds = Craft.ReservedReturnSeconds;
						FStrategicEvent& Lost = AddEvent(Result, EStrategicEventType::InterceptionLost, NextSequence, Slice.CurrentUtc);
						Lost.BaseId = Craft.BaseId;
						Lost.CraftId = Craft.CraftId;
						Lost.ContactId = EscapedContactId;
						Lost.RuleId = ContactRuleId;
						FStrategicEvent& Returning = AddEvent(Result, EStrategicEventType::CraftReturnStarted, NextSequence, Slice.CurrentUtc);
						Returning.BaseId = Craft.BaseId;
						Returning.CraftId = Craft.CraftId;
						Returning.ContactId = EscapedContactId;
						Returning.RuleId = Craft.CraftRuleId;
					}
					if (!ApplyAdversaryMissionEscape(Transaction, Rules, Config, EscapedContactId, Result, NextSequence, Slice.CurrentUtc))
					{
						bSimulationFailed = true;
						bStopRequested = true;
						return;
					}
					if (bCreatesLandingSite)
					{
						FStrategicSiteState& Site = Transaction.StrategicSites.AddDefaulted_GetRef();
						Site.SiteId = EscapedContactId;
						Site.Type = EStrategicSiteType::Landing;
						Site.SourceContactRuleId = ContactRuleId;
						Site.LongitudeMilliDegrees = ArrivalLongitude;
						Site.LatitudeMilliDegrees = ArrivalLatitude;
						Site.ThreatRating = ArrivalContactRule->ThreatRating + AdversaryRule->LandingSiteThreatBonus;
						Site.RemainingLifetimeSeconds = static_cast<int64>(AdversaryRule->LandingSiteLifetimeHours) * 3600LL;
						FStrategicEvent& SiteCreated = AddEvent(Result, EStrategicEventType::StrategicSiteCreated, NextSequence, Slice.CurrentUtc);
						SiteCreated.SiteId = Site.SiteId;
						SiteCreated.ContactId = EscapedContactId;
						SiteCreated.MissionId = AdversaryMissionId;
						SiteCreated.RuleId = ContactRuleId;
						SiteCreated.RegionId = AdversaryRule->TargetRegionId;
						SiteCreated.Quantity = Site.ThreatRating;
					}
					Transaction.StrategicContacts.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = bStopRequested || bWasVisible || bWasAdversaryMission;
				}
			}

			if (!Rules.AdversaryMissions.IsEmpty()
				&& Transaction.Outcome == ECampaignOutcome::Ongoing
				&& Transaction.NextAdversaryMissionSerial == AdversaryMissionSerialAtSliceStart)
			{
				Transaction.NextAdversaryMissionSeconds -= FMath::Min(Transaction.NextAdversaryMissionSeconds, SliceSeconds);
				if (Transaction.NextAdversaryMissionSeconds == 0
					&& !LaunchAdversaryMission(Transaction, Rules, Config, Result, NextSequence, Slice.CurrentUtc))
				{
					bSimulationFailed = true;
					bStopRequested = true;
					return;
				}
			}

			if (Slice.Contains(EStrategicTimeMarker::Hour))
			{
				for (FStrategicContactState& Contact : Transaction.StrategicContacts)
				{
					if (Contact.Status != EStrategicContactStatus::Hidden)
					{
						continue;
					}
					const FContactRule& ContactRule = Rules.Contacts.FindChecked(Contact.ContactRuleId);
					int32 BestDetectionChance = 0;
					for (const FStrategicBaseState& Base : Transaction.Bases)
					{
						int32 SensorRange = 0;
						int32 SensorStrength = 0;
						if (!ComputeBaseSensorProfile(Base, Rules, SensorRange, SensorStrength, Result))
						{
							bSimulationFailed = true;
							bStopRequested = true;
							return;
						}
						if (SensorRange <= 0 || SensorStrength <= 0)
						{
							continue;
						}
						const int64 Distance = ApproximateSurfaceDistanceKilometers(
							Base.LongitudeMilliDegrees,
							Base.LatitudeMilliDegrees,
							Contact.LongitudeMilliDegrees,
							Contact.LatitudeMilliDegrees);
						if (Distance > SensorRange)
						{
							continue;
						}
						const int32 DistancePenalty = static_cast<int32>(Distance * 100LL / SensorRange);
						BestDetectionChance = FMath::Max(
							BestDetectionChance,
							FMath::Clamp(SensorStrength + ContactRule.Signature - DistancePenalty, 1, 100));
					}
					if (BestDetectionChance > 0
						&& Transaction.SimulationRandom.NextIntInclusive(1, 100) <= BestDetectionChance)
					{
						Contact.Status = EStrategicContactStatus::Detected;
						FStrategicEvent& Detected = AddEvent(Result, EStrategicEventType::StrategicContactDetected, NextSequence, Slice.CurrentUtc);
						Detected.ContactId = Contact.ContactId;
						Detected.RuleId = Contact.ContactRuleId;
						Detected.Quantity = ContactRule.ThreatRating;
						bStopRequested = true;
					}
				}
			}

			for (int32 Index = Transaction.StrategicSites.Num() - 1; Index >= 0; --Index)
			{
				FStrategicSiteState& Site = Transaction.StrategicSites[Index];
				if (!SiteIdsAtSliceStart.Contains(Site.SiteId))
				{
					continue;
				}
				const bool bDeploymentReserved = Transaction.Craft.ContainsByPredicate(
					[&Site](const FCraftState& Craft)
					{
						return Craft.TargetSiteId == Site.SiteId
							&& (Craft.Status == ECraftStatus::Deploying || Craft.Status == ECraftStatus::OnSite);
					});
				if (bDeploymentReserved)
				{
					continue;
				}
				Site.RemainingLifetimeSeconds -= FMath::Min(Site.RemainingLifetimeSeconds, SliceSeconds);
				if (Site.RemainingLifetimeSeconds == 0)
				{
					FStrategicEvent& Expired = AddEvent(Result, EStrategicEventType::StrategicSiteExpired, NextSequence, Slice.CurrentUtc);
					Expired.SiteId = Site.SiteId;
					Expired.RuleId = Site.SourceContactRuleId;
					Transaction.StrategicSites.RemoveAt(Index, EAllowShrinking::No);
					bStopRequested = true;
				}
			}

			if (Slice.Contains(EStrategicTimeMarker::Month))
			{
				int64 CurrentMaintenance = 0;
				int64 CurrentSalaries = 0;
				int64 CurrentCraftMaintenance = 0;
				int64 CurrentFacilityAndSalaryCosts = 0;
				int64 CurrentOutgoings = 0;
				int64 NetFunding = 0;
				int64 NewFunds = 0;
				if (!ReviewRegionalMandates(Transaction, Rules, Config, Result, NextSequence, Slice.CurrentUtc)
					|| !ComputeMonthlyMaintenance(Transaction, Rules, CurrentMaintenance, Result)
					|| !ComputeMonthlyPersonnelSalaries(Transaction, Rules, CurrentSalaries, Result)
					|| !ComputeMonthlyCraftMaintenance(Transaction, Rules, CurrentCraftMaintenance, Result)
					|| !TryAdd(CurrentMaintenance, CurrentSalaries, CurrentFacilityAndSalaryCosts)
					|| !TryAdd(CurrentFacilityAndSalaryCosts, CurrentCraftMaintenance, CurrentOutgoings)
					|| !TryAdd(Transaction.MonthlyFunding, -CurrentOutgoings, NetFunding)
					|| !TryAdd(Transaction.Funds, NetFunding, NewFunds))
				{
					bSimulationFailed = true;
					bStopRequested = true;
					return;
				}
				Transaction.Funds = NewFunds;
				FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::MonthlyFinancesProcessed, NextSequence, Slice.CurrentUtc);
				Event.Amount = NetFunding;
				bStopRequested = true;
			}
		},
		[&]() { return bStopRequested; });

	if (bSimulationFailed)
	{
		AddError(Result, TEXT("simulation_overflow"), TEXT("Strategic simulation exceeded a persisted numeric range."));
		Result.Events.Reset();
		Result.ExecutedSlices = 0;
		return Result;
	}
	if (Result.ExecutedSlices <= 0)
	{
		AddError(Result, TEXT("time_advance_failed"), TEXT("Strategic clock could not execute a simulation slice."));
		return Result;
	}

	SortStateCollections(Transaction);
	const FCraftServiceQueueSnapshot UpdatedServiceQueue =
		FCraftServiceQueue::Evaluate(Transaction, Rules);
	for (const FCraftServiceQueueView& Queue : UpdatedServiceQueue.Craft)
	{
		if (!Queue.bInServiceLane || ActiveServiceCraftIds.Contains(Queue.CraftId))
		{
			continue;
		}
		FStrategicEvent& Scheduled = AddCraftServiceRotationEvent(
			Result, Queue, NextSequence, Transaction.StrategicTime.Utc);
		if (const FCraftState* Craft = FindCraft(Transaction, Queue.CraftId))
		{
			Scheduled.RuleId = Craft->CraftRuleId;
		}
	}
	const FMutualAidRelayQueueSnapshot UpdatedRelayQueue =
		FMutualAidRelayQueue::Evaluate(Transaction, Rules);
	for (const FMutualAidRelayQueueView& Queue : UpdatedRelayQueue.Convoys)
	{
		if (!Queue.bInTransit || ActiveRelayConvoyIds.Contains(Queue.ConvoyId))
		{
			continue;
		}
		FStrategicEvent& Scheduled = AddMutualAidRelayScheduledEvent(
			Result, Queue, NextSequence, Transaction.StrategicTime.Utc);
		if (const FMutualAidConvoyState* Convoy = Transaction.MutualAidConvoys.FindByPredicate(
			[&Queue](const FMutualAidConvoyState& Candidate)
			{
				return Candidate.ConvoyId == Queue.ConvoyId;
			}))
		{
			Scheduled.RelatedBaseId = Convoy->DestinationBaseId;
			Scheduled.RuleId = Convoy->ItemId;
		}
	}
	Transaction.CommandSequence = NextSequence;
	FStrategicEvent& TimeEvent = AddEvent(Result, EStrategicEventType::TimeAdvanced, NextSequence, Transaction.StrategicTime.Utc);
	TimeEvent.Amount = (Transaction.StrategicTime.Utc - PreviousTime).GetTicks() / ETimespan::TicksPerSecond;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bStopRequested;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FRegionalDiplomacyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FRegionalDiplomacyEvaluation Evaluation = EvaluateRegionalDiplomacy(State, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	FCampaignState Transaction = State;
	FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, Command.RegionId);
	FRegionalPressureState* Pressure = FindRegionalPressure(Transaction, Command.RegionId);
	if (Mandate == nullptr || Pressure == nullptr)
	{
		AddError(Result, TEXT("unknown_regional_mandate"), TEXT("Regional mandate changed before outreach could be committed."));
		return Result;
	}
	const int32 OldSupport = Mandate->Support;
	const int32 OldPressure = Pressure->Pressure;
	const int64 OldContribution = Mandate->CurrentMonthlyFunding;
	const int64 OldMonthlyFunding = Transaction.MonthlyFunding;
	Transaction.Funds -= Evaluation.Cost;
	Mandate->Support = FMath::Clamp(Mandate->Support + Evaluation.SupportDelta, 0, 100);
	Pressure->Pressure = FMath::Max(0, Pressure->Pressure - Evaluation.PressureReduction);
	Mandate->LastDiplomaticActionMonth = GetDiplomaticMonthSerial(Transaction.StrategicTime.Utc);
	const bool bWithdrewFromCompact = Evaluation.SupportDelta < 0
		&& WithdrawHorizonCompactMemberIfRequired(Transaction, *Mandate, Config);
	if (bWithdrewFromCompact)
	{
		if (!CalculateRegionalFundingContribution(
				*Mandate, Config, true, Mandate->CurrentMonthlyFunding)
			|| !TryAdd(
				OldMonthlyFunding,
				Mandate->CurrentMonthlyFunding - OldContribution,
				Transaction.MonthlyFunding))
		{
			AddError(Result, TEXT("financial_overflow"),
				TEXT("Compact withdrawal funding exceeds the campaign numeric range."));
			return Result;
		}
	}
	if (!ValidateAdversaryState(Transaction, Rules, Config, Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FName ActionId;
	switch (Command.ActionType)
	{
	case ERegionalDiplomacyActionType::CivicRelief:
		ActionId = TEXT("diplomacy.civic-relief");
		break;
	case ERegionalDiplomacyActionType::SecurityAccord:
		ActionId = TEXT("diplomacy.security-accord");
		break;
	case ERegionalDiplomacyActionType::CrisisMobilization:
		ActionId = TEXT("diplomacy.crisis-mobilization");
		break;
	default:
		checkNoEntry();
		return Result;
	}
	FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::RegionalDiplomacyActionCompleted,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Completed.RuleId = ActionId;
	Completed.RegionId = Command.RegionId;
	Completed.Amount = -Evaluation.Cost;
	Completed.Quantity = Mandate->Support;
	if (Mandate->Support != OldSupport)
	{
		FStrategicEvent& SupportChanged = AddEvent(Result, EStrategicEventType::RegionalSupportChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		SupportChanged.RuleId = ActionId;
		SupportChanged.RegionId = Command.RegionId;
		SupportChanged.Amount = Mandate->Support - OldSupport;
		SupportChanged.Quantity = Mandate->Support;
	}
	if (bWithdrewFromCompact)
	{
		AddHorizonCompactWithdrawalEvent(
			Result, *Mandate, Config,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		if (Mandate->CurrentMonthlyFunding != OldContribution)
		{
			FStrategicEvent& FundingChanged = AddEvent(
				Result, EStrategicEventType::RegionalFundingChanged,
				Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			FundingChanged.RuleId = ActionId;
			FundingChanged.RegionId = Command.RegionId;
			FundingChanged.Amount = Mandate->CurrentMonthlyFunding - OldContribution;
			FundingChanged.Quantity = Config.ResilienceCharterFundingPercent;
		}
	}
	if (Pressure->Pressure != OldPressure)
	{
		FStrategicEvent& PressureChanged = AddEvent(Result, EStrategicEventType::RegionalPressureChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		PressureChanged.RuleId = ActionId;
		PressureChanged.RegionId = Command.RegionId;
		PressureChanged.Amount = Pressure->Pressure - OldPressure;
		PressureChanged.Quantity = Pressure->Pressure;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FSignRegionalCharterCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FRegionalCharterEvaluation Evaluation = EvaluateRegionalCharter(State, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	FCampaignState Transaction = State;
	FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, Command.RegionId);
	if (Mandate == nullptr)
	{
		AddError(Result, TEXT("unknown_regional_mandate"),
			TEXT("Regional mandate changed before the Resilience Charter could be signed."));
		return Result;
	}
	const int32 OldSupport = Mandate->Support;
	const int64 OldContribution = Mandate->CurrentMonthlyFunding;
	const int64 OldMonthlyFunding = Transaction.MonthlyFunding;
	Transaction.Funds -= Evaluation.Cost;
	Mandate->Support -= Evaluation.SupportCost;
	Mandate->bResilienceCharterSigned = true;
	if (!CalculateRegionalFundingContribution(
			*Mandate, Config, Transaction.bHorizonCompactRatified,
			Mandate->CurrentMonthlyFunding)
		|| !TryAdd(
			OldMonthlyFunding,
			Mandate->CurrentMonthlyFunding - OldContribution,
			Transaction.MonthlyFunding)
		|| !ValidateAdversaryState(Transaction, Rules, Config, Result))
	{
		return Result;
	}
	const int32 NewSupport = Mandate->Support;
	const int64 NewContribution = Mandate->CurrentMonthlyFunding;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	const FName CharterId(TEXT("treaty.resilience-charter"));
	FStrategicEvent& Signed = AddEvent(Result, EStrategicEventType::RegionalCharterSigned,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Signed.RuleId = CharterId;
	Signed.RegionId = Command.RegionId;
	Signed.Amount = -Evaluation.Cost;
	Signed.Quantity = NewSupport;
	Signed.bSuccessful = true;
	FStrategicEvent& SupportChanged = AddEvent(Result, EStrategicEventType::RegionalSupportChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	SupportChanged.RuleId = CharterId;
	SupportChanged.RegionId = Command.RegionId;
	SupportChanged.Amount = NewSupport - OldSupport;
	SupportChanged.Quantity = NewSupport;
	if (NewContribution != OldContribution)
	{
		FStrategicEvent& FundingChanged = AddEvent(Result, EStrategicEventType::RegionalFundingChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		FundingChanged.RuleId = CharterId;
		FundingChanged.RegionId = Command.RegionId;
		FundingChanged.Amount = NewContribution - OldContribution;
		FundingChanged.Quantity = Transaction.bHorizonCompactRatified
			? Config.HorizonCompactFundingPercent
			: Config.ResilienceCharterFundingPercent;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FRatifyHorizonCompactCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FHorizonCompactEvaluation Evaluation = EvaluateHorizonCompact(State, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	TMap<FName, int32> OldSupports;
	TMap<FName, int64> OldContributions;
	for (const FName RegionId : Evaluation.MemberRegionIds)
	{
		FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, RegionId);
		if (Mandate == nullptr)
		{
			AddError(Result, TEXT("invalid_coalition_compact_state"),
				TEXT("A Horizon Compact member changed before ratification could be committed."));
			return Result;
		}
		OldSupports.Add(RegionId, Mandate->Support);
		OldContributions.Add(RegionId, Mandate->CurrentMonthlyFunding);
		Mandate->Support -= Evaluation.MemberSupportCost;
	}
	Transaction.Funds -= Evaluation.Cost;
	Transaction.bHorizonCompactRatified = true;
	Transaction.MonthlyFunding = 0;
	for (FRegionalMandateState& Mandate : Transaction.RegionalMandates)
	{
		int64 UpdatedMonthlyFunding = 0;
		if (!CalculateRegionalFundingContribution(
				Mandate, Config, true, Mandate.CurrentMonthlyFunding)
			|| !TryAdd(
				Transaction.MonthlyFunding, Mandate.CurrentMonthlyFunding,
				UpdatedMonthlyFunding))
		{
			AddError(Result, TEXT("financial_overflow"),
				TEXT("The Horizon Compact funding commitment exceeds the campaign numeric range."));
			return Result;
		}
		Transaction.MonthlyFunding = UpdatedMonthlyFunding;
	}
	if (!ValidateAdversaryState(Transaction, Rules, Config, Result))
	{
		return Result;
	}

	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	const FName CompactId(TEXT("coalition.horizon-compact"));
	FStrategicEvent& Ratified = AddEvent(Result, EStrategicEventType::HorizonCompactRatified,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Ratified.RuleId = CompactId;
	Ratified.Amount = -Evaluation.Cost;
	Ratified.Quantity = Evaluation.MemberRegionIds.Num();
	Ratified.bSuccessful = true;
	for (const FName RegionId : Evaluation.MemberRegionIds)
	{
		const FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, RegionId);
		if (Mandate == nullptr)
		{
			checkNoEntry();
			return Result;
		}
		FStrategicEvent& SupportChanged = AddEvent(Result, EStrategicEventType::RegionalSupportChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		SupportChanged.RuleId = CompactId;
		SupportChanged.RegionId = RegionId;
		SupportChanged.Amount = Mandate->Support - OldSupports.FindChecked(RegionId);
		SupportChanged.Quantity = Mandate->Support;
	}
	for (const FName RegionId : Evaluation.MemberRegionIds)
	{
		const FRegionalMandateState* Mandate = FindRegionalMandate(Transaction, RegionId);
		if (Mandate == nullptr)
		{
			checkNoEntry();
			return Result;
		}
		const int64 FundingDelta =
			Mandate->CurrentMonthlyFunding - OldContributions.FindChecked(RegionId);
		if (FundingDelta != 0)
		{
			FStrategicEvent& FundingChanged = AddEvent(Result, EStrategicEventType::RegionalFundingChanged,
				Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			FundingChanged.RuleId = CompactId;
			FundingChanged.RegionId = RegionId;
			FundingChanged.Amount = FundingDelta;
			FundingChanged.Quantity = Evaluation.FundingPercent;
		}
	}

	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FDeployReciprocalAidCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FReciprocalAidEvaluation Evaluation = EvaluateReciprocalAid(State, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	FCampaignState Transaction = State;
	FRegionalMandateState* TargetMandate =
		FindRegionalMandate(Transaction, Evaluation.TargetRegionId);
	FRegionalMandateState* DonorMandate =
		FindRegionalMandate(Transaction, Evaluation.DonorRegionId);
	FRegionalPressureState* TargetPressure =
		FindRegionalPressure(Transaction, Evaluation.TargetRegionId);
	FRegionalPressureState* DonorPressure =
		FindRegionalPressure(Transaction, Evaluation.DonorRegionId);
	if (TargetMandate == nullptr || DonorMandate == nullptr
		|| TargetPressure == nullptr || DonorPressure == nullptr)
	{
		AddError(Result, TEXT("invalid_coalition_aid_state"),
			TEXT("Compact membership changed before Reciprocal Aid could be deployed."));
		return Result;
	}

	const int32 OldTargetPressure = TargetPressure->Pressure;
	const int32 OldDonorPressure = DonorPressure->Pressure;
	const int32 OldTargetSupport = TargetMandate->Support;
	const int32 OldDonorSupport = DonorMandate->Support;
	const int64 OldTargetFunding = TargetMandate->CurrentMonthlyFunding;
	const int64 OldDonorFunding = DonorMandate->CurrentMonthlyFunding;
	const int64 OldMonthlyFunding = Transaction.MonthlyFunding;
	Transaction.Funds -= Evaluation.Cost;
	TargetPressure->Pressure = Evaluation.TargetProjectedPressure;
	DonorPressure->Pressure = Evaluation.DonorProjectedPressure;
	TargetMandate->Support = Evaluation.TargetProjectedSupport;
	DonorMandate->Support = Evaluation.DonorProjectedSupport;
	const bool bDonorWithdrewFromCompact =
		WithdrawHorizonCompactMemberIfRequired(Transaction, *DonorMandate, Config);
	if (bDonorWithdrewFromCompact != Evaluation.bDonorWouldWithdraw)
	{
		AddError(Result, TEXT("invalid_coalition_aid_state"),
			TEXT("Compact cohesion changed before Reciprocal Aid could be deployed."));
		return Result;
	}
	Transaction.LastCoalitionAidMonth = GetDiplomaticMonthSerial(Transaction.StrategicTime.Utc);
	int64 FundingAfterTarget = 0;
	if (!CalculateRegionalFundingContribution(
			*TargetMandate, Config, true, TargetMandate->CurrentMonthlyFunding)
		|| !CalculateRegionalFundingContribution(
			*DonorMandate, Config, true, DonorMandate->CurrentMonthlyFunding)
		|| !TryAdd(
			OldMonthlyFunding,
			TargetMandate->CurrentMonthlyFunding - OldTargetFunding,
			FundingAfterTarget)
		|| !TryAdd(
			FundingAfterTarget,
			DonorMandate->CurrentMonthlyFunding - OldDonorFunding,
			Transaction.MonthlyFunding))
	{
		AddError(Result, TEXT("financial_overflow"),
			TEXT("The Reciprocal Aid funding commitment exceeds the campaign numeric range."));
		return Result;
	}
	if (!ValidateAdversaryState(Transaction, Rules, Config, Result))
	{
		return Result;
	}

	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	const FName AidId(TEXT("coalition.reciprocal-aid"));
	FStrategicEvent& Deployed = AddEvent(Result, EStrategicEventType::CoalitionAidDeployed,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Deployed.RuleId = AidId;
	Deployed.RegionId = Evaluation.TargetRegionId;
	Deployed.Amount = -Evaluation.Cost;
	Deployed.Quantity = Evaluation.PressureTransfer;
	Deployed.bSuccessful = true;

	FStrategicEvent& TargetPressureChanged = AddEvent(
		Result, EStrategicEventType::RegionalPressureChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	TargetPressureChanged.RuleId = AidId;
	TargetPressureChanged.RegionId = Evaluation.TargetRegionId;
	TargetPressureChanged.Amount = Evaluation.TargetProjectedPressure - OldTargetPressure;
	TargetPressureChanged.Quantity = Evaluation.TargetProjectedPressure;
	FStrategicEvent& DonorPressureChanged = AddEvent(
		Result, EStrategicEventType::RegionalPressureChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	DonorPressureChanged.RuleId = AidId;
	DonorPressureChanged.RegionId = Evaluation.DonorRegionId;
	DonorPressureChanged.Amount = Evaluation.DonorProjectedPressure - OldDonorPressure;
	DonorPressureChanged.Quantity = Evaluation.DonorProjectedPressure;

	if (Evaluation.TargetProjectedSupport != OldTargetSupport)
	{
		FStrategicEvent& TargetSupportChanged = AddEvent(
			Result, EStrategicEventType::RegionalSupportChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		TargetSupportChanged.RuleId = AidId;
		TargetSupportChanged.RegionId = Evaluation.TargetRegionId;
		TargetSupportChanged.Amount = Evaluation.TargetProjectedSupport - OldTargetSupport;
		TargetSupportChanged.Quantity = Evaluation.TargetProjectedSupport;
	}
	FStrategicEvent& DonorSupportChanged = AddEvent(
		Result, EStrategicEventType::RegionalSupportChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	DonorSupportChanged.RuleId = AidId;
	DonorSupportChanged.RegionId = Evaluation.DonorRegionId;
	DonorSupportChanged.Amount = Evaluation.DonorProjectedSupport - OldDonorSupport;
	DonorSupportChanged.Quantity = Evaluation.DonorProjectedSupport;
	if (bDonorWithdrewFromCompact)
	{
		AddHorizonCompactWithdrawalEvent(
			Result, *DonorMandate, Config,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	}

	const FRegionalMandateState* UpdatedTarget =
		FindRegionalMandate(Transaction, Evaluation.TargetRegionId);
	const FRegionalMandateState* UpdatedDonor =
		FindRegionalMandate(Transaction, Evaluation.DonorRegionId);
	if (UpdatedTarget == nullptr || UpdatedDonor == nullptr)
	{
		checkNoEntry();
		return Result;
	}
	if (UpdatedTarget->CurrentMonthlyFunding != OldTargetFunding)
	{
		FStrategicEvent& FundingChanged = AddEvent(
			Result, EStrategicEventType::RegionalFundingChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		FundingChanged.RuleId = AidId;
		FundingChanged.RegionId = Evaluation.TargetRegionId;
		FundingChanged.Amount = UpdatedTarget->CurrentMonthlyFunding - OldTargetFunding;
		FundingChanged.Quantity = Config.HorizonCompactFundingPercent;
	}
	if (UpdatedDonor->CurrentMonthlyFunding != OldDonorFunding)
	{
		FStrategicEvent& FundingChanged = AddEvent(
			Result, EStrategicEventType::RegionalFundingChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		FundingChanged.RuleId = AidId;
		FundingChanged.RegionId = Evaluation.DonorRegionId;
		FundingChanged.Amount = UpdatedDonor->CurrentMonthlyFunding - OldDonorFunding;
		FundingChanged.Quantity = UpdatedDonor->bHorizonCompactMemberWithdrawn
			? Config.ResilienceCharterFundingPercent
			: Config.HorizonCompactFundingPercent;
	}

	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FStartManufacturingCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (!Command.ProjectId.IsValid()
		|| State.ManufacturingProjects.ContainsByPredicate([&Command](const FManufacturingProjectState& Project) { return Project.ProjectId == Command.ProjectId; }))
	{
		AddError(Result, TEXT("invalid_manufacturing_project_id"), TEXT("Manufacturing project id must be valid and unique."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Manufacturing project base does not exist."));
		return Result;
	}
	const FItemRule* Item = Rules.Items.Find(Command.ItemId);
	if (Item == nullptr)
	{
		AddError(Result, TEXT("unknown_item"), FString::Printf(TEXT("Item rule '%s' is not loaded."), *Command.ItemId.ToString()));
		return Result;
	}
	if (!Item->IsManufacturable())
	{
		AddError(Result, TEXT("item_not_manufacturable"), TEXT("Item has no manufacturing-time definition."));
		return Result;
	}
	if (Command.Units <= 0)
	{
		AddError(Result, TEXT("invalid_manufacturing_quantity"), TEXT("Manufacturing quantity must be positive."));
		return Result;
	}
	if (!FContentPackageResolver::IsValidPackageId(Config.ManufacturingFacilityId)
		|| !HasOperationalFacility(*Base, Rules, Config.ManufacturingFacilityId))
	{
		AddError(Result, TEXT("manufacturing_facility_missing"), FString::Printf(TEXT("Base requires operational facility '%s' for manufacturing."), *Config.ManufacturingFacilityId.ToString()));
		return Result;
	}
	for (const FName Requirement : Item->RequiredResearch)
	{
		if (!State.CompletedResearch.Contains(Requirement))
		{
			AddError(Result, TEXT("manufacturing_research_missing"), FString::Printf(TEXT("Manufacturing requires completed research '%s'."), *Requirement.ToString()));
			return Result;
		}
	}

	int64 TotalCost = 0;
	if (!TryMultiplyNonNegative(Item->ManufactureCost, Command.Units, TotalCost))
	{
		AddError(Result, TEXT("financial_overflow"), TEXT("Manufacturing order cost exceeds the campaign numeric range."));
		return Result;
	}
	if (State.Funds < TotalCost)
	{
		AddError(Result, TEXT("insufficient_funds"), FString::Printf(TEXT("Manufacturing order requires %lld funds, but only %lld are available."), TotalCost, State.Funds));
		return Result;
	}

	FCampaignState Transaction = State;
	Transaction.Funds -= TotalCost;
	FStrategicBaseState* TransactionBase = FindBase(Transaction, Command.BaseId);
	TArray<FInventoryStack> MaterialChanges;
	if (TransactionBase == nullptr
		|| !AdjustManufacturingInputs(*TransactionBase, Rules, *Item, Command.Units, true, MaterialChanges, Result))
	{
		if (TransactionBase == nullptr)
		{
			AddError(Result, TEXT("unknown_base"), TEXT("Manufacturing project base does not exist."));
		}
		return Result;
	}
	FManufacturingProjectState& Project = Transaction.ManufacturingProjects.AddDefaulted_GetRef();
	Project.ProjectId = Command.ProjectId;
	Project.ItemId = Command.ItemId;
	Project.BaseId = Command.BaseId;
	Project.UnitsRemaining = Command.Units;
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, Command.BaseId,
		TEXT("Starting this production run"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ManufacturingStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.ProjectId = Command.ProjectId;
	Event.RuleId = Command.ItemId;
	Event.Amount = -TotalCost;
	Event.Quantity = Command.Units;
	for (const FInventoryStack& Change : MaterialChanges)
	{
		FStrategicEvent& Materials = AddEvent(Result, EStrategicEventType::ManufacturingMaterialsReserved,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Materials.BaseId = Command.BaseId;
		Materials.ProjectId = Command.ProjectId;
		Materials.RuleId = Change.ItemId;
		Materials.Quantity = Change.Quantity;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetManufacturingStaffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.AssignedEngineers < 0)
	{
		AddError(Result, TEXT("invalid_staff_assignment"), TEXT("Assigned engineers cannot be negative."));
		return Result;
	}

	FCampaignState Transaction = State;
	FManufacturingProjectState* Project = FindManufacturingProject(Transaction, Command.ProjectId);
	if (Project == nullptr)
	{
		AddError(Result, TEXT("unknown_manufacturing_project"), TEXT("Manufacturing project is not active."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(Transaction, Project->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Manufacturing project references a missing base."));
		return Result;
	}

	FBasePersonnelCapacityProfile PersonnelCapacity;
	if (!ComputeBasePersonnelCapacities(*Base, Rules, PersonnelCapacity, Result))
	{
		return Result;
	}
	int64 CurrentAssignedAtBase = Base->WorksCadreEngineers;
	int64 ProposedAssignedAtBase = Base->WorksCadreEngineers;
	for (const FManufacturingProjectState& Other : Transaction.ManufacturingProjects)
	{
		if (Other.BaseId == Project->BaseId)
		{
			const int32 ProposedAssignment = Other.ProjectId == Project->ProjectId
				? Command.AssignedEngineers
				: Other.AssignedEngineers;
			if (!TryAdd(CurrentAssignedAtBase, Other.AssignedEngineers, CurrentAssignedAtBase)
				|| !TryAdd(ProposedAssignedAtBase, ProposedAssignment, ProposedAssignedAtBase))
			{
				AddError(Result, TEXT("engineer_capacity_exceeded"), TEXT("Engineer assignments exceed the campaign numeric range."));
				return Result;
			}
		}
	}
	if (ProposedAssignedAtBase > PersonnelCapacity.EngineerCapacity
		&& ProposedAssignedAtBase > CurrentAssignedAtBase)
	{
		AddError(Result, TEXT("engineer_capacity_exceeded"), FString::Printf(
			TEXT("Assignment needs %lld engineers at a base with effective capacity %d; reduce existing overcapacity before increasing staff."),
			ProposedAssignedAtBase, PersonnelCapacity.EngineerCapacity));
		return Result;
	}

	Project->AssignedEngineers = Command.AssignedEngineers;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ManufacturingStaffChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Project->BaseId;
	Event.ProjectId = Project->ProjectId;
	Event.RuleId = Project->ItemId;
	Event.Quantity = Project->AssignedEngineers;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FAdjustManufacturingUnitsCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.DeltaUnits == 0)
	{
		AddError(Result, TEXT("invalid_manufacturing_adjustment"), TEXT("Manufacturing quantity adjustment cannot be zero."));
		return Result;
	}
	FCampaignState Transaction = State;
	FManufacturingProjectState* Project = FindManufacturingProject(Transaction, Command.ProjectId);
	if (Project == nullptr)
	{
		AddError(Result, TEXT("unknown_manufacturing_project"), TEXT("Manufacturing project is not active."));
		return Result;
	}
	const FItemRule* Item = Rules.Items.Find(Project->ItemId);
	if (Item == nullptr || !Item->IsManufacturable() || Item->ManufactureCost < 0)
	{
		AddError(Result, TEXT("unknown_item"), TEXT("Manufacturing project references an unavailable item rule."));
		return Result;
	}
	FStrategicBaseState* Base = FindBase(Transaction, Project->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Manufacturing project references a missing base."));
		return Result;
	}
	const int64 NewUnits = static_cast<int64>(Project->UnitsRemaining) + static_cast<int64>(Command.DeltaUnits);
	if (NewUnits < 1)
	{
		AddError(Result, TEXT("manufacturing_quantity_below_minimum"), TEXT("Keep at least one unit in the run or cancel the production project."));
		return Result;
	}
	if (NewUnits > MAX_int32)
	{
		AddError(Result, TEXT("manufacturing_quantity_overflow"), TEXT("Manufacturing run exceeds the supported unit count."));
		return Result;
	}
	const int64 UnitDelta = Command.DeltaUnits;
	const int64 ChangedUnits = UnitDelta > 0 ? UnitDelta : -UnitDelta;
	int64 FundsDelta = 0;
	if (!TryMultiplyNonNegative(Item->ManufactureCost, ChangedUnits, FundsDelta))
	{
		AddError(Result, TEXT("economy_overflow"), TEXT("Manufacturing quantity adjustment exceeds the campaign numeric range."));
		return Result;
	}
	if (UnitDelta > 0)
	{
		if (Transaction.Funds < FundsDelta)
		{
			AddError(Result, TEXT("insufficient_funds"), FString::Printf(
				TEXT("Adding %lld production units requires %lld funds, but only %lld are available."),
				ChangedUnits, FundsDelta, Transaction.Funds));
			return Result;
		}
		Transaction.Funds -= FundsDelta;
	}
	else
	{
		int64 NewFunds = 0;
		if (!TryAdd(Transaction.Funds, FundsDelta, NewFunds))
		{
			AddError(Result, TEXT("economy_overflow"), TEXT("Manufacturing quantity refund exceeds the campaign numeric range."));
			return Result;
		}
		Transaction.Funds = NewFunds;
	}
	TArray<FInventoryStack> MaterialChanges;
	if (!AdjustManufacturingInputs(*Base, Rules, *Item, ChangedUnits,
		UnitDelta > 0, MaterialChanges, Result))
	{
		return Result;
	}
	Project->UnitsRemaining = static_cast<int32>(NewUnits);
	const FGuid BaseId = Project->BaseId;
	const FName ItemId = Project->ItemId;
	const int32 UpdatedUnits = Project->UnitsRemaining;
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, BaseId,
		TEXT("Changing this production run"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ManufacturingQuantityChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.ProjectId = Command.ProjectId;
	Event.RuleId = ItemId;
	Event.Amount = UnitDelta > 0 ? -FundsDelta : FundsDelta;
	Event.Quantity = UpdatedUnits;
	for (const FInventoryStack& Change : MaterialChanges)
	{
		FStrategicEvent& Materials = AddEvent(Result,
			UnitDelta > 0 ? EStrategicEventType::ManufacturingMaterialsReserved
				: EStrategicEventType::ManufacturingMaterialsRefunded,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Materials.BaseId = BaseId;
		Materials.ProjectId = Command.ProjectId;
		Materials.RuleId = Change.ItemId;
		Materials.Quantity = Change.Quantity;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FCancelManufacturingCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	const FManufacturingProjectState* Project = FindManufacturingProject(Transaction, Command.ProjectId);
	if (Project == nullptr)
	{
		AddError(Result, TEXT("unknown_manufacturing_project"), TEXT("Manufacturing project is not active."));
		return Result;
	}
	const FItemRule* Item = Rules.Items.Find(Project->ItemId);
	if (Item == nullptr || Item->ManufactureCost < 0)
	{
		AddError(Result, TEXT("unknown_item"), TEXT("Manufacturing project references an unavailable item rule."));
		return Result;
	}
	const int32 RefundableUnits = FMath::Max(0,
		Project->UnitsRemaining - (Project->AccumulatedWorkSeconds > 0 ? 1 : 0));
	int64 Refund = 0;
	int64 NewFunds = 0;
	if (!TryMultiplyNonNegative(Item->ManufactureCost, RefundableUnits, Refund)
		|| !TryAdd(Transaction.Funds, Refund, NewFunds))
	{
		AddError(Result, TEXT("economy_overflow"), TEXT("Manufacturing cancellation refund exceeds the campaign numeric range."));
		return Result;
	}
	const FGuid BaseId = Project->BaseId;
	const FName ItemId = Project->ItemId;
	FStrategicBaseState* Base = FindBase(Transaction, BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Manufacturing project references a missing base."));
		return Result;
	}
	TArray<FInventoryStack> MaterialChanges;
	if (!AdjustManufacturingInputs(*Base, Rules, *Item, RefundableUnits, false, MaterialChanges, Result))
	{
		return Result;
	}
	Transaction.Funds = NewFunds;
	Transaction.ManufacturingProjects.RemoveAll(
		[&Command](const FManufacturingProjectState& Entry) { return Entry.ProjectId == Command.ProjectId; });
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, BaseId,
		TEXT("Cancelling this production run"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::ManufacturingCancelled,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.ProjectId = Command.ProjectId;
	Event.RuleId = ItemId;
	Event.Amount = Refund;
	Event.Quantity = RefundableUnits;
	for (const FInventoryStack& Change : MaterialChanges)
	{
		FStrategicEvent& Materials = AddEvent(Result, EStrategicEventType::ManufacturingMaterialsRefunded,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Materials.BaseId = BaseId;
		Materials.ProjectId = Command.ProjectId;
		Materials.RuleId = Change.ItemId;
		Materials.Quantity = Change.Quantity;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSellInventoryCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.Quantity <= 0)
	{
		AddError(Result, TEXT("invalid_sale_quantity"), TEXT("Inventory sale quantity must be positive."));
		return Result;
	}
	const FItemRule* Item = Rules.Items.Find(Command.ItemId);
	if (Item == nullptr)
	{
		AddError(Result, TEXT("unknown_item"), TEXT("Inventory sale item is not present in the active rules."));
		return Result;
	}
	if (Item->SellValue <= 0)
	{
		AddError(Result, TEXT("item_not_sellable"), TEXT("This inventory item has no positive disposition value."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	const FInventoryStack* Stack = Base != nullptr
		? Base->Inventory.FindByPredicate(
			[&Command](const FInventoryStack& Entry) { return Entry.ItemId == Command.ItemId; })
		: nullptr;
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Inventory sale base does not exist."));
		return Result;
	}
	if (Stack == nullptr || Stack->Quantity < Command.Quantity)
	{
		AddError(Result, TEXT("insufficient_inventory"), TEXT("Inventory sale exceeds the unassigned stock at this base."));
		return Result;
	}
	int64 Proceeds = 0;
	int64 NewFunds = 0;
	if (!TryMultiplyNonNegative(Item->SellValue, Command.Quantity, Proceeds)
		|| !TryAdd(State.Funds, Proceeds, NewFunds))
	{
		AddError(Result, TEXT("economy_overflow"), TEXT("Inventory sale proceeds exceed the campaign numeric range."));
		return Result;
	}
	FCampaignState Transaction = State;
	FStrategicBaseState* TransactionBase = FindBase(Transaction, Command.BaseId);
	check(TransactionBase != nullptr);
	if (!TryAdjustInventory(*TransactionBase, Command.ItemId, -Command.Quantity))
	{
		AddError(Result, TEXT("inventory_transaction_failed"), TEXT("Inventory sale could not be applied atomically."));
		return Result;
	}
	Transaction.Funds = NewFunds;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::InventorySold,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.RuleId = Command.ItemId;
	Event.Amount = Proceeds;
	Event.Quantity = Command.Quantity;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FDispatchMutualAidConvoyCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"),
			TEXT("Mutual Aid Convoys cannot be dispatched after the campaign has concluded."));
		return Result;
	}
	if (Command.SourceBaseId == Command.DestinationBaseId)
	{
		AddError(Result, TEXT("mutual_aid_same_base"),
			TEXT("A Mutual Aid Convoy must connect two different established bases."));
		return Result;
	}
	if (Command.Quantity <= 0)
	{
		AddError(Result, TEXT("invalid_mutual_aid_quantity"),
			TEXT("Mutual Aid Convoy quantity must be positive."));
		return Result;
	}
	const FItemRule* Item = Rules.Items.Find(Command.ItemId);
	if (Item == nullptr || Item->Mass < 0)
	{
		AddError(Result, TEXT("unknown_item"),
			TEXT("Mutual Aid Convoy cargo is not present in the active rules."));
		return Result;
	}
	const FStrategicBaseState* Source = FindBase(State, Command.SourceBaseId);
	const FStrategicBaseState* Destination = FindBase(State, Command.DestinationBaseId);
	if (Source == nullptr || Destination == nullptr)
	{
		AddError(Result, TEXT("unknown_base"),
			TEXT("Mutual Aid Convoy source and destination must both be established bases."));
		return Result;
	}
	const FInventoryStack* Stock = Source->Inventory.FindByPredicate(
		[&Command](const FInventoryStack& Stack) { return Stack.ItemId == Command.ItemId; });
	if (Stock == nullptr || Stock->Quantity < Command.Quantity)
	{
		AddError(Result, TEXT("insufficient_inventory"),
			TEXT("Mutual Aid Convoy cargo exceeds the unassigned stock at its source base."));
		return Result;
	}
	const FMutualAidRouteEvaluation Route = EvaluateMutualAidRoute(
		State, Config, Command.SourceBaseId, Command.DestinationBaseId,
		Command.RoutePolicy, Command.bSignalEscort);
	if (!Route.bValid)
	{
		Result.Diagnostics = Route.Diagnostics;
		return Result;
	}
	if (Command.bSignalEscort && !Route.bSignalEscortAffordable)
	{
		AddError(Result, TEXT("mutual_aid_signal_escort_funds"),
			TEXT("Campaign funds cannot cover this convoy's Signal Escort."));
		return Result;
	}
	const int64 ProjectedJourneySeconds = Route.bInterdictionExpected && !Command.bSignalEscort
		? Route.TransitSeconds + Route.InterdictionDelaySeconds
		: Route.TransitSeconds;
	const FMutualAidRelayQueueView RelayProjection = FMutualAidRelayQueue::ProjectNext(
		State, Rules, Command.SourceBaseId, ProjectedJourneySeconds);
	if (!RelayProjection.bValid || !RelayProjection.bRelayAvailable)
	{
		AddError(Result, TEXT("mutual_aid_relay_unavailable"),
			TEXT("The source base has no operational signal capacity for a Mutual Aid relay channel."));
		return Result;
	}
	if (!ValidateMutualAidConvoyState(State, Rules, Result))
	{
		return Result;
	}
	if (State.MutualAidConvoys.Num() >= 10000)
	{
		AddError(Result, TEXT("mutual_aid_convoy_limit"),
			TEXT("The Mutual Aid Convoy ledger has reached its supported record limit."));
		return Result;
	}

	FCampaignState Transaction = State;
	FStrategicBaseState* TransactionSource = FindBase(Transaction, Command.SourceBaseId);
	check(TransactionSource != nullptr);
	if (!TryAdjustInventory(*TransactionSource, Command.ItemId, -Command.Quantity))
	{
		AddError(Result, TEXT("inventory_transaction_failed"),
			TEXT("Mutual Aid Convoy cargo could not be committed atomically."));
		return Result;
	}
	if (Command.bSignalEscort)
	{
		Transaction.Funds -= Route.SignalEscortCost;
	}
	FMutualAidConvoyState& Convoy = Transaction.MutualAidConvoys.AddDefaulted_GetRef();
	Convoy.ConvoyId = MakeDeterministicMutualAidConvoyId(
		Command.SourceBaseId, Command.DestinationBaseId,
		Command.ItemId, Command.Quantity, Command.RoutePolicy,
		Command.bSignalEscort, State.CommandSequence + 1);
	Convoy.SourceBaseId = Command.SourceBaseId;
	Convoy.DestinationBaseId = Command.DestinationBaseId;
	Convoy.ItemId = Command.ItemId;
	Convoy.Quantity = Command.Quantity;
	Convoy.DispatchSequence = State.CommandSequence + 1;
	Convoy.RoutePolicy = Command.RoutePolicy;
	Convoy.TotalTransitSeconds = Route.TransitSeconds;
	Convoy.RemainingTransitSeconds = Route.TransitSeconds;
	Convoy.RoutePressure = Route.RoutePressure;
	Convoy.bSignalEscort = Command.bSignalEscort;
	Convoy.SignalEscortCost = Command.bSignalEscort ? Route.SignalEscortCost : 0;
	Convoy.bInterdictionResolved = !Route.bInterdictionExpected;
	Convoy.ForecastInterdictionDelaySeconds = Route.InterdictionDelaySeconds;
	Convoy.InterdictionDelaySeconds = 0;
	if (State.MutualAidConvoys.ContainsByPredicate(
		[&Convoy](const FMutualAidConvoyState& Existing)
		{
			return Existing.ConvoyId == Convoy.ConvoyId;
		}))
	{
		AddError(Result, TEXT("mutual_aid_convoy_id_collision"),
			TEXT("Mutual Aid Convoy identity collided with an active commitment."));
		return Result;
	}
	++Transaction.CommandSequence;
	if (!ValidatePlayerStorageTransition(
		State, Transaction, Rules, Command.DestinationBaseId,
		TEXT("Mutual Aid Convoy dispatch"), Result))
	{
		return Result;
	}
	const FGuid ConvoyId = Convoy.ConvoyId;
	const int64 TransitSeconds = Convoy.RemainingTransitSeconds;
	SortStateCollections(Transaction);
	const FMutualAidRelayQueueSnapshot RelayQueue =
		FMutualAidRelayQueue::Evaluate(Transaction, Rules);
	const FMutualAidRelayQueueView* ScheduledRelay = RelayQueue.FindConvoy(ConvoyId);
	if (ScheduledRelay == nullptr)
	{
		AddError(Result, TEXT("mutual_aid_relay_unavailable"),
			TEXT("The committed convoy could not enter the source base Relay Weave."));
		return Result;
	}
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::MutualAidConvoyDispatched,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.SourceBaseId;
	Event.RelatedBaseId = Command.DestinationBaseId;
	Event.ConvoyId = ConvoyId;
	Event.RuleId = Command.ItemId;
	Event.PolicyId = Route.PolicyId;
	Event.Quantity = Command.Quantity;
	Event.Amount = TransitSeconds;
	Event.ConvoyRoutePressure = Route.RoutePressure;
	Event.ConvoyTransitSeconds = TransitSeconds;
	Event.ConvoyEscortCost = Command.bSignalEscort ? Route.SignalEscortCost : 0;
	Event.ConvoyDelaySeconds = Route.InterdictionDelaySeconds;
	Event.bConvoySignalEscort = Command.bSignalEscort;
	Event.ConvoyRelayChannelCount = ScheduledRelay->RelayChannelCount;
	Event.SignalWatchFacilityChannelCount = ScheduledRelay->FacilityRelayChannelCount;
	Event.SignalWatchAssignedScientists = ScheduledRelay->SignalWatchScientistCount;
	Event.SignalWatchBonusChannelCount = ScheduledRelay->SignalWatchBonusChannelCount;
	Event.SignalWatchTotalChannelCount = ScheduledRelay->RelayChannelCount;
	Event.ConvoyRelayQueuePosition = ScheduledRelay->QueuePosition;
	Event.ConvoyRelayChannelNumber = ScheduledRelay->RelayChannelNumber;
	Event.bConvoyRelayActive = ScheduledRelay->bInTransit;
	Event.ConvoyRelayWaitSeconds = ScheduledRelay->EstimatedWaitSeconds;
	Event.ConvoyEstimatedArrivalSeconds = ScheduledRelay->EstimatedArrivalSeconds;
	FStrategicEvent& RelayScheduled = AddMutualAidRelayScheduledEvent(
		Result, *ScheduledRelay, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	RelayScheduled.RelatedBaseId = Command.DestinationBaseId;
	RelayScheduled.RuleId = Command.ItemId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetSignalWatchStaffCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FSignalWatchStaffEvaluation Evaluation =
		EvaluateSignalWatchStaff(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}

	const FMutualAidRelayQueueSnapshot BeforeRelay =
		FMutualAidRelayQueue::Evaluate(State, Rules);
	FCampaignState Transaction = State;
	FStrategicBaseState* Base = FindBase(Transaction, Command.BaseId);
	check(Base != nullptr);
	Base->SignalWatchScientists = Command.AssignedScientists;
	++Transaction.CommandSequence;

	FStrategicEvent& Changed = AddEvent(
		Result, EStrategicEventType::MutualAidSignalWatchStaffChanged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Changed.BaseId = Command.BaseId;
	Changed.PolicyId = Evaluation.PolicyId;
	Changed.Quantity = Command.AssignedScientists;
	Changed.SignalWatchFacilityChannelCount = Evaluation.FacilityRelayChannelCount;
	Changed.SignalWatchAssignedScientists = Command.AssignedScientists;
	Changed.SignalWatchBonusChannelCount = Evaluation.BonusRelayChannelCount;
	Changed.SignalWatchTotalChannelCount = Evaluation.TotalRelayChannelCount;
	Changed.ConvoyRelayChannelCount = Evaluation.TotalRelayChannelCount;
	Changed.bSuccessful = true;

	const FMutualAidRelayQueueSnapshot AfterRelay =
		FMutualAidRelayQueue::Evaluate(Transaction, Rules);
	for (const FMutualAidRelayQueueView& Queue : AfterRelay.Convoys)
	{
		if (Queue.SourceBaseId != Command.BaseId || !Queue.bInTransit)
		{
			continue;
		}
		const FMutualAidRelayQueueView* Previous = BeforeRelay.FindConvoy(Queue.ConvoyId);
		if (Previous == nullptr || !Previous->bInTransit)
		{
			AddMutualAidRelayScheduledEvent(
				Result, Queue, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		}
	}

	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FCancelFacilityConstructionCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	const FFacilityConstructionProjectState* Project = Transaction.FacilityConstructionProjects.FindByPredicate(
		[&Command](const FFacilityConstructionProjectState& Entry) { return Entry.ProjectId == Command.ProjectId; });
	if (Project == nullptr)
	{
		AddError(Result, TEXT("unknown_construction_project"), TEXT("Facility construction project is not active."));
		return Result;
	}
	if (FindBase(Transaction, Project->BaseId) == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Facility construction project references a missing base."));
		return Result;
	}
	const FFacilityRule* Facility = Rules.Facilities.Find(Project->FacilityId);
	if (Facility == nullptr || Facility->BuildCost < 0 || Facility->BuildHours <= 0)
	{
		AddError(Result, TEXT("unknown_facility"), TEXT("Facility construction project references an unavailable facility rule."));
		return Result;
	}
	const int64 TotalBuildSeconds = static_cast<int64>(Facility->BuildHours) * 3600LL;
	if (Project->RemainingBuildSeconds <= 0 || Project->RemainingBuildSeconds > TotalBuildSeconds)
	{
		AddError(Result, TEXT("invalid_construction_state"), TEXT("Facility construction progress is outside the supported range."));
		return Result;
	}
	int64 RefundNumerator = 0;
	int64 Refund = 0;
	int64 NewFunds = 0;
	if (!TryMultiplyNonNegative(Facility->BuildCost, Project->RemainingBuildSeconds, RefundNumerator)
		|| !TryAdd(Transaction.Funds, RefundNumerator / TotalBuildSeconds, NewFunds))
	{
		AddError(Result, TEXT("economy_overflow"), TEXT("Facility construction cancellation refund exceeds the campaign numeric range."));
		return Result;
	}
	Refund = RefundNumerator / TotalBuildSeconds;
	const FGuid BaseId = Project->BaseId;
	const FName FacilityId = Project->FacilityId;
	Transaction.Funds = NewFunds;
	Transaction.FacilityConstructionProjects.RemoveAll(
		[&Command](const FFacilityConstructionProjectState& Entry) { return Entry.ProjectId == Command.ProjectId; });
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::FacilityConstructionCancelled,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.ProjectId = Command.ProjectId;
	Event.RuleId = FacilityId;
	Event.Amount = Refund;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FFacilityDismantleEvaluation FStrategicCommandService::EvaluateFacilityDismantle(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FDismantleFacilityCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FFacilityDismantleEvaluation Evaluation;
	FStrategicCommandResult Validation;
	const auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		return Reject();
	}
	if (!FContentPackageResolver::IsValidPackageId(Config.OperationsFacilityId)
		|| Config.FacilityDismantleRefundPercent < 0
		|| Config.FacilityDismantleRefundPercent > 100)
	{
		AddError(Validation, TEXT("invalid_simulation_config"),
			TEXT("Facility dismantling requires a valid operations facility id and a salvage percentage from 0 to 100."));
		return Reject();
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"), TEXT("Facility dismantling base does not exist."));
		return Reject();
	}
	if (!Base->BuiltFacilities.IsEmpty())
	{
		AddError(Validation, TEXT("legacy_layout_upgrade_required"),
			TEXT("Legacy abstract facilities must be positioned before operational facilities can be dismantled."));
		return Reject();
	}
	if (!Command.FacilityInstanceId.IsValid())
	{
		AddError(Validation, TEXT("unknown_facility_instance"), TEXT("Facility instance id must be valid."));
		return Reject();
	}
	const FBaseFacilityState* Installed = Base->Facilities.FindByPredicate(
		[&Command](const FBaseFacilityState& Facility)
		{
			return Facility.InstanceId == Command.FacilityInstanceId;
		});
	if (Installed == nullptr)
	{
		AddError(Validation, TEXT("unknown_facility_instance"),
			TEXT("Operational facility does not exist at the selected base."));
		return Reject();
	}
	const FFacilityRule* Facility = Rules.Facilities.Find(Installed->FacilityId);
	if (Facility == nullptr || Facility->BuildCost < 0 || Facility->GridWidth <= 0 || Facility->GridHeight <= 0)
	{
		AddError(Validation, TEXT("unknown_facility"),
			TEXT("Operational facility references an unavailable or invalid facility rule."));
		return Reject();
	}
	if (Installed->RemainingRepairSeconds > 0 || Installed->ReservedRepairDamage > 0)
	{
		AddError(Validation, TEXT("facility_repair_active"),
			TEXT("Cancel or complete this facility's active repair before dismantling it."));
		return Reject();
	}

	const bool bRetainsManufacturingFacility = Base->Facilities.ContainsByPredicate(
		[Installed, &Config](const FBaseFacilityState& Other)
		{
			return Other.InstanceId != Installed->InstanceId
				&& Other.FacilityId == Config.ManufacturingFacilityId;
		});
	if (Installed->FacilityId == Config.ManufacturingFacilityId
		&& !bRetainsManufacturingFacility
		&& State.ManufacturingProjects.ContainsByPredicate(
			[Base](const FManufacturingProjectState& Project) { return Project.BaseId == Base->BaseId; }))
	{
		AddError(Validation, TEXT("facility_supports_active_production"),
			TEXT("Complete or cancel this base's active production before removing its last fabrication facility."));
		return Reject();
	}

	FBasePersonnelCapacityProfile CurrentPersonnelCapacity;
	if (!ComputeBasePersonnelCapacities(*Base, Rules, CurrentPersonnelCapacity, Validation))
	{
		return Reject();
	}
	const int32 TargetScientistCapacity = Facility->ScaleEffectByIntegrity(
		Facility->ScientistCapacity, Installed->Damage);
	const int32 TargetEngineerCapacity = Facility->ScaleEffectByIntegrity(
		Facility->EngineerCapacity, Installed->Damage);
	const int32 RemainingScientistCapacity =
		CurrentPersonnelCapacity.ScientistCapacity - TargetScientistCapacity;
	const int32 RemainingEngineerCapacity =
		CurrentPersonnelCapacity.EngineerCapacity - TargetEngineerCapacity;
	int32 ScientistPersonnel = 0;
	int32 EngineerPersonnel = 0;
	if (!CountPersonnelForCategory(State, Rules, Base->BaseId,
			EPersonnelRoleCategory::Scientist, ScientistPersonnel)
		|| !CountPersonnelForCategory(State, Rules, Base->BaseId,
			EPersonnelRoleCategory::Engineer, EngineerPersonnel))
	{
		AddError(Validation, TEXT("invalid_personnel_state"),
			TEXT("Existing personnel reference an unloaded role."));
		return Reject();
	}
	int64 AssignedScientists = Base->SignalWatchScientists;
	for (const FResearchProjectState& Project : State.ResearchProjects)
	{
		if (Project.BaseId == Base->BaseId
			&& !TryAdd(AssignedScientists, Project.AssignedScientists, AssignedScientists))
		{
			AddError(Validation, TEXT("scientist_capacity_exceeded"),
				TEXT("Scientist assignments exceed the campaign numeric range."));
			return Reject();
		}
	}
	int64 AssignedEngineers = Base->WorksCadreEngineers;
	for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
	{
		if (Project.BaseId == Base->BaseId
			&& !TryAdd(AssignedEngineers, Project.AssignedEngineers, AssignedEngineers))
		{
			AddError(Validation, TEXT("engineer_capacity_exceeded"),
				TEXT("Engineer assignments exceed the campaign numeric range."));
			return Reject();
		}
	}
	const int64 ScientistDemand = FMath::Max<int64>(ScientistPersonnel, AssignedScientists);
	const int64 EngineerDemand = FMath::Max<int64>(EngineerPersonnel, AssignedEngineers);
	if (TargetScientistCapacity > 0 && ScientistDemand > RemainingScientistCapacity)
	{
		AddError(Validation, TEXT("facility_scientist_capacity_required"), FString::Printf(
			TEXT("This base needs %lld scientist capacity for its roster or assignments, but dismantling would leave %d."),
			ScientistDemand, RemainingScientistCapacity));
		return Reject();
	}
	if (TargetEngineerCapacity > 0 && EngineerDemand > RemainingEngineerCapacity)
	{
		AddError(Validation, TEXT("facility_engineer_capacity_required"), FString::Printf(
			TEXT("This base needs %lld engineer capacity for its roster or assignments, but dismantling would leave %d."),
			EngineerDemand, RemainingEngineerCapacity));
		return Reject();
	}

	int32 CurrentCraftCapacity = 0;
	if (!ComputeBaseCraftCapacity(*Base, Rules, CurrentCraftCapacity, Validation))
	{
		return Reject();
	}
	const int32 TargetCraftCapacity = Facility->ScaleEffectByIntegrity(Facility->CraftCapacity, Installed->Damage);
	const int32 RemainingCraftCapacity = CurrentCraftCapacity - TargetCraftCapacity;
	int64 OccupiedCraftBerths = 0;
	for (const FCraftState& Craft : State.Craft)
	{
		OccupiedCraftBerths += Craft.BaseId == Base->BaseId ? 1 : 0;
	}
	for (const FCraftAcquisitionOrderState& Order : State.CraftAcquisitionOrders)
	{
		OccupiedCraftBerths += Order.BaseId == Base->BaseId ? 1 : 0;
	}
	if (OccupiedCraftBerths > RemainingCraftCapacity)
	{
		AddError(Validation, TEXT("facility_craft_capacity_required"), FString::Printf(
			TEXT("This base needs %lld occupied or reserved craft berths, but dismantling would leave %d."),
			OccupiedCraftBerths, RemainingCraftCapacity));
		return Reject();
	}

	if (!ValidateFacilityConnectivityAfterRemoval(
		State, *Base, Rules, Config, Command.FacilityInstanceId, Validation))
	{
		return Reject();
	}

	FCampaignState StorageTransaction = State;
	FStrategicBaseState* StorageBase = FindBase(StorageTransaction, Base->BaseId);
	check(StorageBase != nullptr);
	StorageBase->Facilities.RemoveAll(
		[&Command](const FBaseFacilityState& Entry)
		{
			return Entry.InstanceId == Command.FacilityInstanceId;
		});
	if (!ValidatePlayerStorageTransition(State, StorageTransaction, Rules, Base->BaseId,
		TEXT("Dismantling this facility"), Validation))
	{
		return Reject();
	}

	int64 RefundNumerator = 0;
	int64 IntegrityAdjustedRefundNumerator = 0;
	int64 NewFunds = 0;
	if (!TryMultiplyNonNegative(
		Facility->BuildCost, Config.FacilityDismantleRefundPercent, RefundNumerator)
		|| Facility->MaxIntegrity <= 0
		|| Installed->Damage < 0 || Installed->Damage > Facility->MaxIntegrity
		|| !TryMultiplyNonNegative(RefundNumerator,
			Facility->MaxIntegrity - Installed->Damage, IntegrityAdjustedRefundNumerator)
		|| !TryAdd(State.Funds,
			IntegrityAdjustedRefundNumerator / (100LL * Facility->MaxIntegrity), NewFunds))
	{
		AddError(Validation, TEXT("economy_overflow"),
			TEXT("Facility salvage exceeds the campaign numeric range."));
		return Reject();
	}
	Evaluation.bAllowed = true;
	Evaluation.FacilityId = Installed->FacilityId;
	Evaluation.Refund = IntegrityAdjustedRefundNumerator / (100LL * Facility->MaxIntegrity);
	return Evaluation;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FDismantleFacilityCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FFacilityDismantleEvaluation Evaluation = EvaluateFacilityDismantle(State, Rules, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	int64 NewFunds = 0;
	if (!TryAdd(State.Funds, Evaluation.Refund, NewFunds))
	{
		AddError(Result, TEXT("economy_overflow"),
			TEXT("Facility salvage exceeds the campaign numeric range."));
		return Result;
	}

	FCampaignState Transaction = State;
	FStrategicBaseState* TransactionBase = FindBase(Transaction, Command.BaseId);
	check(TransactionBase != nullptr);
	TransactionBase->Facilities.RemoveAll(
		[&Command](const FBaseFacilityState& Entry)
		{
			return Entry.InstanceId == Command.FacilityInstanceId;
		});
	Transaction.Funds = NewFunds;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::FacilityDismantled,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.FacilityInstanceId = Command.FacilityInstanceId;
	Event.RuleId = Evaluation.FacilityId;
	Event.Amount = Evaluation.Refund;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FFacilityRepairEvaluation FStrategicCommandService::EvaluateFacilityRepair(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStartFacilityRepairCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FFacilityRepairEvaluation Evaluation;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		return Reject();
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Validation, TEXT("unknown_base"), TEXT("Facility repair base does not exist."));
		return Reject();
	}
	if (!Command.FacilityInstanceId.IsValid())
	{
		AddError(Validation, TEXT("unknown_facility_instance"), TEXT("Facility instance id must be valid."));
		return Reject();
	}
	const FBaseFacilityState* FacilityState = FindFacility(*Base, Command.FacilityInstanceId);
	if (FacilityState == nullptr)
	{
		AddError(Validation, TEXT("unknown_facility_instance"),
			TEXT("Operational facility does not exist at the selected base."));
		return Reject();
	}
	const FFacilityRule* Rule = Rules.Facilities.Find(FacilityState->FacilityId);
	if (Rule == nullptr || Rule->MaxIntegrity <= 0 || Rule->RepairCostPerIntegrity < 0
		|| Rule->RepairHoursPerIntegrity <= 0
		|| FacilityState->Damage < 0 || FacilityState->Damage > (Rule != nullptr ? Rule->MaxIntegrity : 0))
	{
		AddError(Validation, TEXT("invalid_facility_state"),
			TEXT("Facility repair references invalid rule or durability data."));
		return Reject();
	}
	if (FacilityState->RemainingRepairSeconds > 0 || FacilityState->ReservedRepairDamage > 0)
	{
		AddError(Validation, TEXT("facility_repair_active"), TEXT("This facility already has an active repair."));
		return Reject();
	}
	if (FacilityState->Damage == 0)
	{
		AddError(Validation, TEXT("facility_undamaged"), TEXT("This facility is already at full integrity."));
		return Reject();
	}

	int64 Cost = 0;
	int64 RepairHours = 0;
	int64 BaselineDurationSeconds = 0;
	int64 DurationSeconds = 0;
	int32 FrontloadPercent = 0;
	const FWorksCadreCharterPolicy CharterPolicy =
		GetWorksCadreCharterPolicy(Base->WorksCadreCharter);
	if (CharterPolicy.PolicyId.IsNone())
	{
		AddError(Validation, TEXT("invalid_works_cadre_charter"),
			TEXT("The base has an unknown Works Charter."));
		return Reject();
	}
	if (!TryMultiplyNonNegative(FacilityState->Damage, Rule->RepairCostPerIntegrity, Cost)
		|| !TryMultiplyNonNegative(FacilityState->Damage, Rule->RepairHoursPerIntegrity, RepairHours)
		|| !TryMultiplyNonNegative(RepairHours, 3600, BaselineDurationSeconds)
		|| !TryApplyWorksCadreFrontload(
			BaselineDurationSeconds, Base->WorksCadreEngineers,
			CharterPolicy.RepairFrontloadPercentPerEngineer,
			DurationSeconds, FrontloadPercent))
	{
		AddError(Validation, TEXT("facility_repair_overflow"),
			TEXT("Facility repair cost or duration exceeds the supported range."));
		return Reject();
	}
	Evaluation.FacilityId = FacilityState->FacilityId;
	Evaluation.Damage = FacilityState->Damage;
	Evaluation.Cost = Cost;
	Evaluation.WorksCadreEngineers = Base->WorksCadreEngineers;
	Evaluation.WorksCadreFrontloadPercent = FrontloadPercent;
	Evaluation.WorksCadreCharter = Base->WorksCadreCharter;
	Evaluation.BaselineDurationSeconds = BaselineDurationSeconds;
	Evaluation.DurationSeconds = DurationSeconds;
	if (State.Funds < Cost)
	{
		AddError(Validation, TEXT("insufficient_funds"), FString::Printf(
			TEXT("Facility repair requires %lld funds, but only %lld are available."), Cost, State.Funds));
		return Reject();
	}

	Evaluation.bAllowed = true;
	return Evaluation;
}

FBaseAssaultEvaluation FStrategicCommandService::EvaluateBaseAssault(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FResolveBaseAssaultCommand& Command)
{
	return EvaluateBaseAssault(State, Rules, FStrategicSimulationConfig(), Command);
}

FBaseAssaultEvaluation FStrategicCommandService::EvaluateBaseAssault(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FResolveBaseAssaultCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FBaseAssaultEvaluation Evaluation;
	FStrategicCommandResult Validation;
	auto Reject = [&Evaluation, &Validation]()
	{
		Evaluation.Diagnostics = MoveTemp(Validation.Diagnostics);
		return Evaluation;
	};
	if (!ValidateSequence(State, Command.ExpectedSequence, Validation))
	{
		return Reject();
	}
	const FBaseDefenseFireDoctrinePolicy DoctrinePolicy =
		GetBaseDefenseFireDoctrinePolicy(Command.FireDoctrine);
	if (!DoctrinePolicy.bValid)
	{
		AddError(Validation, TEXT("invalid_base_defense_fire_doctrine"),
			TEXT("Base assault selected an unsupported automatic-defense fire doctrine."));
		return Reject();
	}
	if (Command.FireDoctrine == EBaseDefenseFireDoctrine::GridOvercharge
		&& !HasValidBaseDefenseGridOverchargeConfig(Config))
	{
		AddError(Validation, TEXT("invalid_base_defense_overcharge_config"),
			TEXT("Grid Overcharge cost, accuracy, and damage settings must remain within supported limits."));
		return Reject();
	}
	if (!Command.AssaultId.IsValid())
	{
		AddError(Validation, TEXT("unknown_base_assault"), TEXT("Base assault id must be valid."));
		return Reject();
	}
	const FBaseAssaultState* Assault = FindBaseAssault(State, Command.AssaultId);
	if (Assault == nullptr)
	{
		AddError(Validation, TEXT("unknown_base_assault"), TEXT("Base assault is no longer pending."));
		return Reject();
	}
	const FAdversaryMissionState* Mission = FindAdversaryMissionById(State, Assault->MissionId);
	const FStrategicContactState* Contact = FindContact(State, Assault->ContactId);
	const FStrategicBaseState* Base = FindBase(State, Assault->BaseId);
	const FAdversaryMissionRule* MissionRule = Mission != nullptr
		? Rules.AdversaryMissions.Find(Mission->MissionRuleId)
		: nullptr;
	const FContactRule* ContactRule = Contact != nullptr
		? Rules.Contacts.Find(Contact->ContactRuleId)
		: nullptr;
	if (Mission == nullptr || Contact == nullptr || Base == nullptr || MissionRule == nullptr
		|| ContactRule == nullptr || ContactRule->ThreatRating <= 0 || ContactRule->ThreatRating > 10
		|| !MissionRule->bTargetsPlayerBase || MissionRule->BaseFacilityDamage <= 0
		|| MissionRule->BaseFacilitiesHit <= 0 || Mission->ContactId != Assault->ContactId
		|| Mission->TargetBaseId != Assault->BaseId || Contact->Status != EStrategicContactStatus::Detected
		|| Contact->ElapsedRouteSeconds != Contact->TotalRouteSeconds)
	{
		AddError(Validation, TEXT("invalid_base_assault"), TEXT("Base assault has inconsistent mission, contact, target, or breach rules."));
		return Reject();
	}
	const FBaseInfrastructureEvaluation Infrastructure =
		EvaluateBaseInfrastructure(State, Rules, Assault->BaseId);
	if (!Infrastructure.bValid)
	{
		Validation.Diagnostics = Infrastructure.Diagnostics;
		return Reject();
	}

	Evaluation.AssaultId = Assault->AssaultId;
	Evaluation.BaseId = Assault->BaseId;
	Evaluation.ContactId = Assault->ContactId;
	Evaluation.FireDoctrine = Command.FireDoctrine;
	Evaluation.PolicyId = DoctrinePolicy.PolicyId;
	Evaluation.AccuracyBonus = Command.FireDoctrine == EBaseDefenseFireDoctrine::GridOvercharge
		? Config.BaseDefenseGridOverchargeAccuracyBonus
		: 0;
	Evaluation.DamagePercent = Command.FireDoctrine == EBaseDefenseFireDoctrine::GridOvercharge
		? Config.BaseDefenseGridOverchargeDamagePercent
		: 100;
	if (Base->Facilities.IsEmpty())
	{
		// Legacy layouts are migrated before live play; retain their historical unlimited-fire preview.
		Evaluation.DefenseBatteryCount = Infrastructure.DefenseBatteryCount;
		Evaluation.ReadyDefenseBatteryCount = Infrastructure.DefenseBatteryCount;
		Evaluation.MaximumDefenseDamage = Infrastructure.MaximumDefenseDamage;
		Evaluation.ExpectedDefenseDamage = Infrastructure.ExpectedDefenseDamage;
	}
	else
	{
		FBaseDefenseVolleyPlan Volley;
		if (!BuildBaseDefenseVolleyPlan(*Base, Rules, Config, Command.FireDoctrine, Volley, Validation))
		{
			return Reject();
		}
		Evaluation.DefenseBatteryCount = Volley.OperationalBatteryCount;
		Evaluation.ReadyDefenseBatteryCount = Volley.ReadyShots.Num();
		Evaluation.MaximumDefenseDamage = Volley.MaximumDamage;
		Evaluation.ExpectedDefenseDamage = Volley.ExpectedDamage;
		Evaluation.DefenseSupplies = MoveTemp(Volley.Supplies);
	}
	Evaluation.ContactHull = Contact->CurrentHull;
	Evaluation.BreachDamagePerFacility = MissionRule->BaseFacilityDamage;
	Evaluation.MaximumFacilitiesHit = MissionRule->BaseFacilitiesHit;
	if (Command.FireDoctrine == EBaseDefenseFireDoctrine::GridOvercharge
		&& Evaluation.ReadyDefenseBatteryCount > 0
		&& !TryMultiplyNonNegative(
			Config.BaseDefenseGridOverchargeCostPerThreat,
			ContactRule->ThreatRating,
			Evaluation.FundingCost))
	{
		AddError(Validation, TEXT("invalid_base_defense_overcharge_config"),
			TEXT("Grid Overcharge threat pricing exceeds the supported campaign-fund range."));
		return Reject();
	}
	Evaluation.bAffordable = State.Funds >= Evaluation.FundingCost;
	if (State.TacticalOperations.ContainsByPredicate(
		[&Command](const FTacticalOperationState& Operation)
		{
			return Operation.Type == ETacticalOperationType::BaseDefense && Operation.AssaultId == Command.AssaultId;
		}))
	{
		AddError(Validation, TEXT("base_assault_in_tactical_operation"), TEXT("This assault is already being resolved on the tactical battlefield."));
		return Reject();
	}
	if (!Evaluation.bAffordable)
	{
		AddError(Validation, TEXT("insufficient_base_defense_overcharge_funds"), FString::Printf(
			TEXT("Grid Overcharge requires %lld funds for threat %d, but only %lld are available."),
			Evaluation.FundingCost, ContactRule->ThreatRating, State.Funds));
		return Reject();
	}
	Evaluation.bAllowed = true;
	return Evaluation;
}

FBaseDefenseDeploymentEvaluation FStrategicCommandService::EvaluateBaseDefenseDeployment(
	const FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FDeployBaseDefenseOperationCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FBaseDefenseDeploymentEvaluation Evaluation;
	FResolveBaseAssaultCommand ResolveCommand;
	ResolveCommand.ExpectedSequence = Command.ExpectedSequence;
	ResolveCommand.AssaultId = Command.AssaultId;
	const FBaseAssaultEvaluation AssaultEvaluation = EvaluateBaseAssault(State, Rules, ResolveCommand);
	if (!AssaultEvaluation.bAllowed)
	{
		Evaluation.Diagnostics = AssaultEvaluation.Diagnostics;
		return Evaluation;
	}

	const FBaseAssaultState* Assault = FindBaseAssault(State, Command.AssaultId);
	const FStrategicContactState* Contact = Assault != nullptr ? FindContact(State, Assault->ContactId) : nullptr;
	const FTacticalMissionRule* TacticalMission = nullptr;
	if (Contact != nullptr)
	{
		for (const TPair<FName, FTacticalMissionRule>& Pair : Rules.TacticalMissions)
		{
			if (Pair.Value.Context == ETacticalMissionContext::BaseDefense
				&& Pair.Value.SourceContactRuleId == Contact->ContactRuleId)
			{
				if (TacticalMission != nullptr)
				{
					FStrategicCommandDiagnostic& Diagnostic = Evaluation.Diagnostics.AddDefaulted_GetRef();
					Diagnostic.Code = TEXT("ambiguous_base_defense_mission");
					Diagnostic.Message = TEXT("More than one tactical base-defense recipe maps this contact.");
					return Evaluation;
				}
				TacticalMission = &Pair.Value;
			}
		}
	}
	if (TacticalMission == nullptr)
	{
		FStrategicCommandDiagnostic& Diagnostic = Evaluation.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("missing_base_defense_mission");
		Diagnostic.Message = TEXT("No tactical base-defense recipe maps the assaulting contact.");
		return Evaluation;
	}

	TSet<FGuid> CraftAssignedAgents;
	for (const FCraftState& Craft : State.Craft)
	{
		for (const FGuid& AgentId : Craft.AssignedAgentIds)
		{
			CraftAssignedAgents.Add(AgentId);
		}
	}
	for (const FPersonnelState& Person : State.Personnel)
	{
		const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId);
		if (Person.BaseId == AssaultEvaluation.BaseId
			&& Person.Status == EPersonnelStatus::Available
			&& Role != nullptr && Role->Category == EPersonnelRoleCategory::FieldAgent
			&& !CraftAssignedAgents.Contains(Person.PersonnelId))
		{
			Evaluation.AgentIds.Add(Person.PersonnelId);
		}
	}
	Evaluation.AgentIds.Sort(
		[](const FGuid& Left, const FGuid& Right)
		{
			return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
		});
	if (Evaluation.AgentIds.Num() > TacticalMission->MapWidth)
	{
		Evaluation.AgentIds.SetNum(TacticalMission->MapWidth);
	}
	if (Evaluation.AgentIds.IsEmpty())
	{
		FStrategicCommandDiagnostic& Diagnostic = Evaluation.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("no_base_defenders");
		Diagnostic.Message = TEXT("No available unassigned field agents are stationed at the threatened base.");
		return Evaluation;
	}

	Evaluation.AssaultId = AssaultEvaluation.AssaultId;
	Evaluation.BaseId = AssaultEvaluation.BaseId;
	Evaluation.ContactId = AssaultEvaluation.ContactId;
	Evaluation.MissionRuleId = TacticalMission->Identity.RuleId;
	Evaluation.bAllowed = true;
	return Evaluation;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FApplyFacilityDamageCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.Damage <= 0)
	{
		AddError(Result, TEXT("invalid_facility_damage"), TEXT("Facility damage must be positive."));
		return Result;
	}
	FCampaignState Transaction = State;
	FStrategicBaseState* Base = FindBase(Transaction, Command.BaseId);
	FBaseFacilityState* Facility = Base != nullptr ? FindFacility(*Base, Command.FacilityInstanceId) : nullptr;
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Facility damage base does not exist."));
		return Result;
	}
	if (Facility == nullptr)
	{
		AddError(Result, TEXT("unknown_facility_instance"), TEXT("Facility damage target does not exist."));
		return Result;
	}
	const FFacilityRule* Rule = Rules.Facilities.Find(Facility->FacilityId);
	if (Rule == nullptr || Rule->MaxIntegrity <= 0 || Facility->Damage < 0 || Facility->Damage > Rule->MaxIntegrity)
	{
		AddError(Result, TEXT("invalid_facility_state"), TEXT("Facility damage target has invalid durability data."));
		return Result;
	}
	if (Facility->Damage == Rule->MaxIntegrity)
	{
		AddError(Result, TEXT("facility_already_disabled"), TEXT("This facility is already fully disabled."));
		return Result;
	}
	const int32 AppliedDamage = FMath::Min(Command.Damage, Rule->MaxIntegrity - Facility->Damage);
	Facility->Damage += AppliedDamage;
	const bool bDisabled = Facility->Damage == Rule->MaxIntegrity;
	++Transaction.CommandSequence;
	FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::FacilityDamaged,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Damaged.BaseId = Command.BaseId;
	Damaged.FacilityInstanceId = Command.FacilityInstanceId;
	Damaged.RuleId = Facility->FacilityId;
	Damaged.Quantity = AppliedDamage;
	if (bDisabled)
	{
		FStrategicEvent& Disabled = AddEvent(Result, EStrategicEventType::FacilityDisabled,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Disabled.BaseId = Command.BaseId;
		Disabled.FacilityInstanceId = Command.FacilityInstanceId;
		Disabled.RuleId = Facility->FacilityId;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bDisabled;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStartFacilityRepairCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FFacilityRepairEvaluation Evaluation = EvaluateFacilityRepair(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	FCampaignState Transaction = State;
	FStrategicBaseState* Base = FindBase(Transaction, Command.BaseId);
	check(Base != nullptr);
	FBaseFacilityState* Facility = FindFacility(*Base, Command.FacilityInstanceId);
	check(Facility != nullptr);
	Transaction.Funds -= Evaluation.Cost;
	Facility->ReservedRepairDamage = Evaluation.Damage;
	Facility->RemainingRepairSeconds = Evaluation.DurationSeconds;
	++Transaction.CommandSequence;
	FStrategicEvent& Started = AddEvent(Result, EStrategicEventType::FacilityRepairStarted,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Started.BaseId = Command.BaseId;
	Started.FacilityInstanceId = Command.FacilityInstanceId;
	Started.RuleId = Evaluation.FacilityId;
	Started.Amount = -Evaluation.Cost;
	Started.Quantity = Evaluation.Damage;
	const FWorksCadreCharterPolicy CharterPolicy =
		GetWorksCadreCharterPolicy(Evaluation.WorksCadreCharter);
	Started.PolicyId = CharterPolicy.PolicyId;
	Started.WorksCadreAssignedEngineers = Evaluation.WorksCadreEngineers;
	Started.WorksCadreFrontloadPercent = Evaluation.WorksCadreFrontloadPercent;
	Started.WorksCadreCharter = Evaluation.WorksCadreCharter;
	Started.WorksCadreConstructionFrontloadPercent =
		Evaluation.WorksCadreEngineers
			* CharterPolicy.ConstructionFrontloadPercentPerEngineer;
	Started.WorksCadreRepairFrontloadPercent =
		Evaluation.WorksCadreFrontloadPercent;
	Started.FacilityBaselineDurationSeconds = Evaluation.BaselineDurationSeconds;
	Started.FacilityCommittedDurationSeconds = Evaluation.DurationSeconds;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FCancelFacilityRepairCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	FStrategicBaseState* Base = FindBase(Transaction, Command.BaseId);
	FBaseFacilityState* Facility = Base != nullptr ? FindFacility(*Base, Command.FacilityInstanceId) : nullptr;
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Facility repair base does not exist."));
		return Result;
	}
	if (Facility == nullptr)
	{
		AddError(Result, TEXT("unknown_facility_instance"), TEXT("Facility repair target does not exist."));
		return Result;
	}
	const FFacilityRule* Rule = Rules.Facilities.Find(Facility->FacilityId);
	if (Rule == nullptr || Rule->RepairCostPerIntegrity < 0
		|| Facility->ReservedRepairDamage <= 0 || Facility->RemainingRepairSeconds <= 0
		|| Facility->ReservedRepairDamage > Facility->Damage)
	{
		AddError(Result, TEXT("facility_repair_not_active"), TEXT("This facility has no valid active repair to cancel."));
		return Result;
	}
	int64 Refund = 0;
	int64 NewFunds = 0;
	if (!TryMultiplyNonNegative(Facility->ReservedRepairDamage, Rule->RepairCostPerIntegrity, Refund)
		|| !TryAdd(Transaction.Funds, Refund, NewFunds))
	{
		AddError(Result, TEXT("economy_overflow"), TEXT("Facility repair refund exceeds the campaign numeric range."));
		return Result;
	}
	const int32 CancelledDamage = Facility->ReservedRepairDamage;
	const FName FacilityId = Facility->FacilityId;
	Transaction.Funds = NewFunds;
	Facility->ReservedRepairDamage = 0;
	Facility->RemainingRepairSeconds = 0;
	++Transaction.CommandSequence;
	FStrategicEvent& Cancelled = AddEvent(Result, EStrategicEventType::FacilityRepairCancelled,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Cancelled.BaseId = Command.BaseId;
	Cancelled.FacilityInstanceId = Command.FacilityInstanceId;
	Cancelled.RuleId = FacilityId;
	Cancelled.Amount = Refund;
	Cancelled.Quantity = CancelledDamage;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FStartFacilityConstructionCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Config.BaseGridWidth <= 0 || Config.BaseGridHeight <= 0)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("Base grid dimensions must be positive."));
		return Result;
	}
	if (!Command.ProjectId.IsValid() || !Command.FacilityInstanceId.IsValid())
	{
		AddError(Result, TEXT("invalid_construction_id"), TEXT("Construction project and facility instance ids must be valid."));
		return Result;
	}
	if (State.FacilityConstructionProjects.ContainsByPredicate(
			[&Command](const FFacilityConstructionProjectState& Project)
			{
				return Project.ProjectId == Command.ProjectId || Project.FacilityInstanceId == Command.FacilityInstanceId;
			}))
	{
		AddError(Result, TEXT("duplicate_construction_id"), TEXT("Construction project or facility instance id is already active."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Facility construction base does not exist."));
		return Result;
	}
	if (Base->Facilities.ContainsByPredicate(
			[&Command](const FBaseFacilityState& Facility) { return Facility.InstanceId == Command.FacilityInstanceId; }))
	{
		AddError(Result, TEXT("duplicate_facility_instance"), TEXT("Facility instance id already exists at the base."));
		return Result;
	}
	if (!Base->BuiltFacilities.IsEmpty())
	{
		AddError(Result, TEXT("legacy_layout_upgrade_required"), TEXT("Legacy abstract facilities must be positioned before new construction."));
		return Result;
	}
	const FFacilityRule* Facility = Rules.Facilities.Find(Command.FacilityId);
	if (Facility == nullptr)
	{
		AddError(Result, TEXT("unknown_facility"), FString::Printf(TEXT("Facility rule '%s' is not loaded."), *Command.FacilityId.ToString()));
		return Result;
	}
	for (const FName Requirement : Facility->RequiredResearch)
	{
		if (!State.CompletedResearch.Contains(Requirement))
		{
			AddError(Result, TEXT("facility_research_missing"), FString::Printf(TEXT("Facility construction requires completed research '%s'."), *Requirement.ToString()));
			return Result;
		}
	}
	FName PlacementFailure;
	if (!CanPlaceFacility(*Base, State.FacilityConstructionProjects, Rules, Config, *Facility, Command.GridX, Command.GridY, !Base->Facilities.IsEmpty(), PlacementFailure))
	{
		AddError(Result, PlacementFailure, FString::Printf(TEXT("Facility '%s' cannot be placed at grid (%d,%d)."), *Command.FacilityId.ToString(), Command.GridX, Command.GridY));
		return Result;
	}
	if (State.Funds < Facility->BuildCost)
	{
		AddError(Result, TEXT("insufficient_funds"), FString::Printf(TEXT("Facility requires %d funds, but only %lld are available."), Facility->BuildCost, State.Funds));
		return Result;
	}
	int64 BaselineBuildSeconds = 0;
	int64 CommittedBuildSeconds = 0;
	int32 WorksCadreFrontloadPercent = 0;
	const FWorksCadreCharterPolicy CharterPolicy =
		GetWorksCadreCharterPolicy(Base->WorksCadreCharter);
	if (CharterPolicy.PolicyId.IsNone())
	{
		AddError(Result, TEXT("invalid_works_cadre_charter"),
			TEXT("The base has an unknown Works Charter."));
		return Result;
	}
	if (!TryMultiplyNonNegative(Facility->BuildHours, 3600, BaselineBuildSeconds)
		|| !TryApplyWorksCadreFrontload(
			BaselineBuildSeconds, Base->WorksCadreEngineers,
			CharterPolicy.ConstructionFrontloadPercentPerEngineer,
			CommittedBuildSeconds, WorksCadreFrontloadPercent))
	{
		AddError(Result, TEXT("facility_construction_overflow"),
			TEXT("Facility construction duration or Works Cadre mobilization exceeds the supported range."));
		return Result;
	}

	FCampaignState Transaction = State;
	Transaction.Funds -= Facility->BuildCost;
	FFacilityConstructionProjectState& Project = Transaction.FacilityConstructionProjects.AddDefaulted_GetRef();
	Project.ProjectId = Command.ProjectId;
	Project.FacilityInstanceId = Command.FacilityInstanceId;
	Project.BaseId = Command.BaseId;
	Project.FacilityId = Command.FacilityId;
	Project.GridX = Command.GridX;
	Project.GridY = Command.GridY;
	Project.RemainingBuildSeconds = CommittedBuildSeconds;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::FacilityConstructionStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.ProjectId = Command.ProjectId;
	Event.RuleId = Command.FacilityId;
	Event.Amount = -Facility->BuildCost;
	Event.PolicyId = CharterPolicy.PolicyId;
	Event.WorksCadreAssignedEngineers = Base->WorksCadreEngineers;
	Event.WorksCadreFrontloadPercent = WorksCadreFrontloadPercent;
	Event.WorksCadreCharter = Base->WorksCadreCharter;
	Event.WorksCadreConstructionFrontloadPercent = WorksCadreFrontloadPercent;
	Event.WorksCadreRepairFrontloadPercent = Base->WorksCadreEngineers
		* CharterPolicy.RepairFrontloadPercentPerEngineer;
	Event.FacilityBaselineDurationSeconds = BaselineBuildSeconds;
	Event.FacilityCommittedDurationSeconds = CommittedBuildSeconds;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FRecruitPersonnelCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Config.MaxGeneralPersonnelPerBase <= 0)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("General personnel capacity must be positive."));
		return Result;
	}
	if (!Command.OrderId.IsValid()
		|| State.RecruitmentOrders.ContainsByPredicate(
			[&Command](const FRecruitmentOrderState& Order) { return Order.OrderId == Command.OrderId; }))
	{
		AddError(Result, TEXT("invalid_recruitment_order_id"), TEXT("Recruitment order id must be valid and unique."));
		return Result;
	}
	if (!Command.PersonnelId.IsValid() || IsPersonnelIdentityInUse(State, Command.PersonnelId))
	{
		AddError(Result, TEXT("invalid_personnel_id"), TEXT("Personnel id must be valid and unused by the roster, recruitment queue, and memorial."));
		return Result;
	}
	const FString NormalizedName = Command.DisplayName.TrimStartAndEnd();
	if (!IsUsablePersonnelName(NormalizedName))
	{
		AddError(Result, TEXT("invalid_personnel_name"), TEXT("Personnel names must contain 1-64 non-whitespace characters."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Recruitment destination base does not exist."));
		return Result;
	}
	const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Command.RoleId);
	if (Role == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel_role"), FString::Printf(TEXT("Personnel role '%s' is not loaded."), *Command.RoleId.ToString()));
		return Result;
	}
	if (Role->RecruitmentCost < 0 || Role->RecruitmentHours <= 0 || Role->MonthlySalary < 0
		|| Role->BaseHealth <= 0 || Role->BaseHealth > 200
		|| Role->BaseAccuracy <= 0 || Role->BaseAccuracy > 100
		|| Role->BaseResolve <= 0 || Role->BaseResolve > 100
		|| Role->BaseMobility <= 0 || Role->BaseMobility > 100
		|| Role->BaseStrength <= 0 || Role->BaseStrength > 100)
	{
		AddError(Result, TEXT("invalid_personnel_role"), TEXT("Personnel role contains invalid recruitment, salary, or base-attribute values."));
		return Result;
	}
	for (const FName Requirement : Role->RequiredResearch)
	{
		if (!State.CompletedResearch.Contains(Requirement))
		{
			AddError(Result, TEXT("personnel_research_missing"), FString::Printf(TEXT("Recruitment requires completed research '%s'."), *Requirement.ToString()));
			return Result;
		}
	}

	FBasePersonnelCapacityProfile PersonnelCapacity;
	if ((Role->Category == EPersonnelRoleCategory::Scientist
			|| Role->Category == EPersonnelRoleCategory::Engineer)
		&& !ComputeBasePersonnelCapacities(*Base, Rules, PersonnelCapacity, Result))
	{
		return Result;
	}
	int32 OccupiedCapacity = 0;
	int32 Capacity = 0;
	if (Role->Category == EPersonnelRoleCategory::Scientist)
	{
		Capacity = PersonnelCapacity.ScientistCapacity;
		if (!CountPersonnelForCategory(State, Rules, Command.BaseId, EPersonnelRoleCategory::Scientist, OccupiedCapacity))
		{
			AddError(Result, TEXT("invalid_personnel_state"), TEXT("Existing personnel reference an unloaded role."));
			return Result;
		}
	}
	else if (Role->Category == EPersonnelRoleCategory::Engineer)
	{
		Capacity = PersonnelCapacity.EngineerCapacity;
		if (!CountPersonnelForCategory(State, Rules, Command.BaseId, EPersonnelRoleCategory::Engineer, OccupiedCapacity))
		{
			AddError(Result, TEXT("invalid_personnel_state"), TEXT("Existing personnel reference an unloaded role."));
			return Result;
		}
	}
	else if (Role->Category == EPersonnelRoleCategory::FieldAgent || Role->Category == EPersonnelRoleCategory::Pilot)
	{
		int32 FieldAgents = 0;
		int32 Pilots = 0;
		Capacity = Config.MaxGeneralPersonnelPerBase;
		if (!CountPersonnelForCategory(State, Rules, Command.BaseId, EPersonnelRoleCategory::FieldAgent, FieldAgents)
			|| !CountPersonnelForCategory(State, Rules, Command.BaseId, EPersonnelRoleCategory::Pilot, Pilots)
			|| FieldAgents > MAX_int32 - Pilots)
		{
			AddError(Result, TEXT("invalid_personnel_state"), TEXT("Existing general personnel count is invalid."));
			return Result;
		}
		OccupiedCapacity = FieldAgents + Pilots;
	}
	else
	{
		AddError(Result, TEXT("invalid_personnel_role"), TEXT("Personnel role category is outside the supported schema."));
		return Result;
	}
	if (OccupiedCapacity >= Capacity)
	{
		AddError(Result, TEXT("personnel_capacity_exceeded"), FString::Printf(TEXT("Base personnel capacity %d for role '%s' is already occupied."), Capacity, *Command.RoleId.ToString()));
		return Result;
	}
	if (State.Funds < Role->RecruitmentCost)
	{
		AddError(Result, TEXT("insufficient_funds"), FString::Printf(TEXT("Recruitment requires %d funds, but only %lld are available."), Role->RecruitmentCost, State.Funds));
		return Result;
	}
	const int64 BaselineTransitSeconds = static_cast<int64>(Role->RecruitmentHours) * 3600LL;
	const FPersonnelState* RecruitmentSteward = FPersonnelStewardship::FindActiveSteward(State, Command.BaseId);
	const bool bLiaisonBenefit = RecruitmentSteward != nullptr
		&& RecruitmentSteward->StewardshipFocus == EPersonnelStewardshipFocus::RecruitmentLiaison;
	if (bLiaisonBenefit && !FPersonnelStewardship::IsConfigValid(Config))
	{
		AddError(Result, TEXT("invalid_personnel_stewardship_config"),
			TEXT("Recruitment Liaison settings cannot produce a bounded transit reduction."));
		return Result;
	}
	int64 TransitSeconds = BaselineTransitSeconds;
	if (bLiaisonBenefit
		&& !FPersonnelStewardship::TryApplyReductionCeil(
			BaselineTransitSeconds, Config.StewardshipReductionPercent, TransitSeconds))
	{
		AddError(Result, TEXT("personnel_stewardship_overflow"),
			TEXT("Recruitment Liaison transit reduction exceeds the supported numeric range."));
		return Result;
	}

	FCampaignState Transaction = State;
	Transaction.Funds -= Role->RecruitmentCost;
	FRecruitmentOrderState& Order = Transaction.RecruitmentOrders.AddDefaulted_GetRef();
	Order.OrderId = Command.OrderId;
	Order.PersonnelId = Command.PersonnelId;
	Order.DisplayName = NormalizedName;
	Order.RoleId = Command.RoleId;
	Order.BaseId = Command.BaseId;
	Order.RemainingTransitSeconds = TransitSeconds;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::RecruitmentStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.ProjectId = Command.OrderId;
	Event.PersonnelId = Command.PersonnelId;
	Event.RuleId = Command.RoleId;
	Event.Amount = -Role->RecruitmentCost;
	if (bLiaisonBenefit)
	{
		Event.RelatedPersonnelId = RecruitmentSteward->PersonnelId;
		Event.PolicyId = FPersonnelStewardship::PolicyId(EPersonnelStewardshipFocus::RecruitmentLiaison);
		Event.PersonnelStewardshipFocus = static_cast<int32>(EPersonnelStewardshipFocus::RecruitmentLiaison);
		Event.PersonnelStewardshipBenefitAmount = BaselineTransitSeconds - TransitSeconds;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FTransferPersonnelCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FPersonnelState* Person = FindPersonnel(State, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel"), TEXT("Personnel member does not exist in the active roster."));
		return Result;
	}
	const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person->RoleId);
	if (Role == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel_role"), TEXT("Personnel member references an unloaded role."));
		return Result;
	}
	if (Person->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("personnel_unavailable"), TEXT("Only available personnel can transfer between bases."));
		return Result;
	}
	if (IsAgentAssignedToCraft(State, Person->PersonnelId))
	{
		AddError(Result, TEXT("personnel_assigned_to_craft"), TEXT("Personnel assigned to a craft must be removed from its roster before transfer."));
		return Result;
	}
	const FStrategicBaseState* SourceBase = FindBase(State, Person->BaseId);
	const FStrategicBaseState* DestinationBase = FindBase(State, Command.DestinationBaseId);
	if (SourceBase == nullptr || DestinationBase == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Personnel transfer requires valid source and destination bases."));
		return Result;
	}
	if (SourceBase->BaseId == DestinationBase->BaseId)
	{
		AddError(Result, TEXT("personnel_already_at_base"), TEXT("Personnel member is already stationed at the selected base."));
		return Result;
	}
	if (WouldViolateStaffingCommitmentAfterRelease(State, Rules, SourceBase->BaseId, Role->Category))
	{
		AddError(Result, TEXT("personnel_staffing_committed"), TEXT("Release one project or Signal Watch staffing commitment before transferring this personnel member."));
		return Result;
	}

	FBasePersonnelCapacityProfile PersonnelCapacity;
	if ((Role->Category == EPersonnelRoleCategory::Scientist
			|| Role->Category == EPersonnelRoleCategory::Engineer)
		&& !ComputeBasePersonnelCapacities(*DestinationBase, Rules, PersonnelCapacity, Result))
	{
		return Result;
	}
	int32 Capacity = 0;
	int32 OccupiedCapacity = 0;
	if (Role->Category == EPersonnelRoleCategory::Scientist)
	{
		Capacity = PersonnelCapacity.ScientistCapacity;
		if (!CountPersonnelForCategory(State, Rules, DestinationBase->BaseId, Role->Category, OccupiedCapacity))
		{
			AddError(Result, TEXT("invalid_personnel_state"), TEXT("Existing personnel reference an unloaded role."));
			return Result;
		}
	}
	else if (Role->Category == EPersonnelRoleCategory::Engineer)
	{
		Capacity = PersonnelCapacity.EngineerCapacity;
		if (!CountPersonnelForCategory(State, Rules, DestinationBase->BaseId, Role->Category, OccupiedCapacity))
		{
			AddError(Result, TEXT("invalid_personnel_state"), TEXT("Existing personnel reference an unloaded role."));
			return Result;
		}
	}
	else if (Role->Category == EPersonnelRoleCategory::FieldAgent || Role->Category == EPersonnelRoleCategory::Pilot)
	{
		if (Config.MaxGeneralPersonnelPerBase <= 0)
		{
			AddError(Result, TEXT("invalid_simulation_config"), TEXT("General personnel capacity must be positive."));
			return Result;
		}
		int32 FieldAgents = 0;
		int32 Pilots = 0;
		Capacity = Config.MaxGeneralPersonnelPerBase;
		if (!CountPersonnelForCategory(State, Rules, DestinationBase->BaseId, EPersonnelRoleCategory::FieldAgent, FieldAgents)
			|| !CountPersonnelForCategory(State, Rules, DestinationBase->BaseId, EPersonnelRoleCategory::Pilot, Pilots)
			|| FieldAgents > MAX_int32 - Pilots)
		{
			AddError(Result, TEXT("invalid_personnel_state"), TEXT("Existing general personnel count is invalid."));
			return Result;
		}
		OccupiedCapacity = FieldAgents + Pilots;
	}
	else
	{
		AddError(Result, TEXT("invalid_personnel_role"), TEXT("Personnel role category is outside the supported schema."));
		return Result;
	}
	if (OccupiedCapacity >= Capacity)
	{
		AddError(Result, TEXT("personnel_capacity_exceeded"), FString::Printf(
			TEXT("Destination personnel capacity %d for role '%s' is already occupied."), Capacity, *Person->RoleId.ToString()));
		return Result;
	}

	FCampaignState Transaction = State;
	FPersonnelState* TransferredPerson = FindPersonnel(Transaction, Command.PersonnelId);
	check(TransferredPerson != nullptr);
	TransferredPerson->BaseId = Command.DestinationBaseId;
	const FName TransferredRoleId = TransferredPerson->RoleId;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelTransferred,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.DestinationBaseId;
	Event.PersonnelId = Command.PersonnelId;
	Event.RuleId = TransferredRoleId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FDismissPersonnelCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FPersonnelState* Person = FindPersonnel(State, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel"), TEXT("Personnel member does not exist in the active roster."));
		return Result;
	}
	const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person->RoleId);
	if (Role == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel_role"), TEXT("Personnel member references an unloaded role."));
		return Result;
	}
	if (Person->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("personnel_unavailable"), TEXT("Only available personnel can be dismissed."));
		return Result;
	}
	if (IsAgentAssignedToCraft(State, Person->PersonnelId))
	{
		AddError(Result, TEXT("personnel_assigned_to_craft"), TEXT("Personnel assigned to a craft must be removed from its roster before dismissal."));
		return Result;
	}
	if (WouldViolateStaffingCommitmentAfterRelease(State, Rules, Person->BaseId, Role->Category))
	{
		AddError(Result, TEXT("personnel_staffing_committed"), TEXT("Release one project or Signal Watch staffing commitment before dismissing this personnel member."));
		return Result;
	}

	FCampaignState Transaction = State;
	FPersonnelState* DismissedPerson = FindPersonnel(Transaction, Command.PersonnelId);
	check(DismissedPerson != nullptr);
	FStrategicBaseState* Base = FindBase(Transaction, DismissedPerson->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Personnel member references a missing base."));
		return Result;
	}
	const FGuid BaseId = DismissedPerson->BaseId;
	const FName RoleId = DismissedPerson->RoleId;
	const int32 ReturnedEquipment = DismissedPerson->EquippedItems.Num();
	for (const FName ItemId : DismissedPerson->EquippedItems)
	{
		if (!TryAdjustInventory(*Base, ItemId, 1))
		{
			AddError(Result, TEXT("inventory_overflow"), FString::Printf(
				TEXT("Returning dismissed equipment '%s' would overflow base inventory."), *ItemId.ToString()));
			return Result;
		}
	}
	Transaction.Personnel.RemoveAll(
		[&Command](const FPersonnelState& Entry) { return Entry.PersonnelId == Command.PersonnelId; });
	Transaction.PersonnelSquadBonds.RemoveAll(
		[&Command](const FPersonnelSquadBondState& Bond)
		{
			return Bond.FirstPersonnelId == Command.PersonnelId
				|| Bond.SecondPersonnelId == Command.PersonnelId;
		});
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, BaseId,
		TEXT("Dismissing this personnel member"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelDismissed,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.PersonnelId = Command.PersonnelId;
	Event.RuleId = RoleId;
	Event.Quantity = ReturnedEquipment;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FApplyPersonnelDamageCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.TacticalBattles.ContainsByPredicate(
		[&Command](const FTacticalBattleState& Battle)
		{
			return Battle.Phase != ETacticalBattlePhase::Resolved
				&& Battle.Units.ContainsByPredicate(
					[&Command](const FTacticalUnitState& Unit) { return Unit.PersonnelId == Command.PersonnelId; });
		}))
	{
		AddError(Result, TEXT("personnel_in_tactical_battle"), TEXT("Strategic personnel damage cannot mutate an active tactical combatant."));
		return Result;
	}
	if (Config.RecoveryHoursPerHealth <= 0)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("Recovery hours per health point must be positive."));
		return Result;
	}
	if (Command.Damage <= 0)
	{
		AddError(Result, TEXT("invalid_personnel_damage"), TEXT("Personnel damage must be positive."));
		return Result;
	}
	if (!FContentPackageResolver::IsValidPackageId(Command.CauseId))
	{
		AddError(Result, TEXT("invalid_personnel_cause"), TEXT("Personnel injury or death cause must be a namespaced id."));
		return Result;
	}

	FCampaignState Transaction = State;
	FPersonnelState* Person = FindPersonnel(Transaction, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel"), TEXT("Personnel member does not exist in the active roster."));
		return Result;
	}
	FStrategicBaseState* Base = FindBase(Transaction, Person->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Personnel member references a missing base."));
		return Result;
	}
	const bool bStewardshipInterrupted = Person->Status == EPersonnelStatus::Stewarding;
	const EPersonnelStewardshipFocus InterruptedFocus = Person->StewardshipFocus;
	const int64 InterruptedRemainingSeconds = Person->RemainingStewardshipSeconds;
	const int32 InterruptedTours = Person->StewardshipToursCompleted;
	const int32 AppliedDamage = FMath::Min(Command.Damage, Person->CurrentHealth);
	if (Command.Damage >= Person->CurrentHealth)
	{
		for (const FName ItemId : Person->EquippedItems)
		{
			if (!TryAdjustInventory(*Base, ItemId, 1))
			{
				AddError(Result, TEXT("inventory_overflow"), FString::Printf(TEXT("Returning equipped item '%s' would overflow base inventory."), *ItemId.ToString()));
				return Result;
			}
		}

		FMemorialRecord& Record = Transaction.Memorial.AddDefaulted_GetRef();
		Record.PersonnelId = Person->PersonnelId;
		Record.DisplayName = Person->DisplayName;
		Record.RoleId = Person->RoleId;
		Record.Rank = Person->Rank;
		Record.Missions = Person->Missions;
		Record.Kills = Person->Kills;
		Record.DoctrineSelections = Person->DoctrineSelections;
		Record.Commendations = Person->Commendations;
		Record.StewardshipToursCompleted = Person->StewardshipToursCompleted;
		Record.DeathUtc = Transaction.StrategicTime.Utc;
		Record.CauseId = Command.CauseId;
		const FGuid DeadPersonnelId = Person->PersonnelId;
		const FGuid BaseId = Person->BaseId;
		for (FCraftState& Craft : Transaction.Craft)
		{
			Craft.AssignedAgentIds.Remove(DeadPersonnelId);
			if (Craft.AssignedPilotId == DeadPersonnelId)
			{
				Craft.AssignedPilotId.Invalidate();
				if (IsFlightStatus(Craft.Status))
				{
					for (const FGuid& AgentId : Craft.AssignedAgentIds)
					{
						if (FPersonnelState* Agent = FindPersonnel(Transaction, AgentId))
						{
							Agent->Status = EPersonnelStatus::Available;
						}
					}
					Craft.Status = ECraftStatus::Grounded;
					Craft.TargetContactId.Invalidate();
					Craft.TargetSiteId.Invalidate();
					Craft.RemainingRouteSeconds = 0;
					Craft.ReservedReturnSeconds = 0;
					Transaction.TacticalOperations.RemoveAll(
						[&Craft](const FTacticalOperationState& Operation) { return Operation.CraftId == Craft.CraftId; });
					Transaction.TacticalBattles.RemoveAll(
						[&Transaction](const FTacticalBattleState& Battle) { return Battle.OperationId.IsValid()
							&& !Transaction.TacticalOperations.ContainsByPredicate(
								[&Battle](const FTacticalOperationState& Operation) { return Operation.OperationId == Battle.OperationId; }); });
				}
			}
		}
		for (FTacticalOperationState& Operation : Transaction.TacticalOperations)
		{
			Operation.AgentIds.Remove(DeadPersonnelId);
		}
		for (FStrategicContactState& Contact : Transaction.StrategicContacts)
		{
			if (Contact.Status == EStrategicContactStatus::Engaged
				&& !Transaction.Craft.ContainsByPredicate(
					[&Contact](const FCraftState& Craft)
					{
						return Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Contact.ContactId;
					}))
			{
				Contact.Status = EStrategicContactStatus::Detected;
			}
		}
		Transaction.Personnel.RemoveAll(
			[&DeadPersonnelId](const FPersonnelState& Entry) { return Entry.PersonnelId == DeadPersonnelId; });
		SortStateCollections(Transaction);
		++Transaction.CommandSequence;
		FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelDied, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Event.BaseId = BaseId;
		Event.PersonnelId = DeadPersonnelId;
		Event.RuleId = Command.CauseId;
		Event.Amount = -AppliedDamage;
		if (bStewardshipInterrupted)
		{
			FStrategicEvent& Interrupted = AddEvent(Result,
				EStrategicEventType::PersonnelStewardshipInterrupted,
				Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Interrupted.BaseId = BaseId;
			Interrupted.PersonnelId = DeadPersonnelId;
			Interrupted.RuleId = Command.CauseId;
			Interrupted.PolicyId = FPersonnelStewardship::PolicyId(InterruptedFocus);
			Interrupted.PersonnelStewardshipFocus = static_cast<int32>(InterruptedFocus);
			Interrupted.PersonnelStewardshipToursCompleted = InterruptedTours;
			Interrupted.PersonnelStewardshipDurationSeconds = InterruptedRemainingSeconds;
		}
		State = MoveTemp(Transaction);
		Result.bAccepted = true;
		Result.bDecisionPause = true;
		return Result;
	}

	Person->CurrentHealth -= Command.Damage;
	const int64 MissingHealth = static_cast<int64>(Person->MaxHealth) - static_cast<int64>(Person->CurrentHealth);
	int64 RecoveryHours = 0;
	int64 RecoverySeconds = 0;
	if (!TryMultiplyNonNegative(MissingHealth, Config.RecoveryHoursPerHealth, RecoveryHours)
		|| !TryMultiplyNonNegative(RecoveryHours, 3600, RecoverySeconds))
	{
		AddError(Result, TEXT("recovery_time_overflow"), TEXT("Personnel recovery time exceeds the campaign numeric range."));
		return Result;
	}
	Person->Status = EPersonnelStatus::Recovering;
	Person->RemainingRecoverySeconds = RecoverySeconds;
	Person->RecoveryPlan = EPersonnelRecoveryPlan::DecisionRequired;
	Person->RemainingTrainingSeconds = 0;
	Person->StewardshipFocus = EPersonnelStewardshipFocus::None;
	Person->RemainingStewardshipSeconds = 0;
	TArray<FGuid> EmergencyRecoveredCraftIds;
	for (FCraftState& Craft : Transaction.Craft)
	{
		Craft.AssignedAgentIds.Remove(Person->PersonnelId);
		if (Craft.AssignedPilotId == Person->PersonnelId && IsFlightStatus(Craft.Status))
		{
			for (const FGuid& AgentId : Craft.AssignedAgentIds)
			{
				if (FPersonnelState* Agent = FindPersonnel(Transaction, AgentId))
				{
					Agent->Status = EPersonnelStatus::Available;
				}
			}
			Craft.Status = ECraftStatus::Grounded;
			Craft.TargetContactId.Invalidate();
			Craft.TargetSiteId.Invalidate();
			Craft.RemainingRouteSeconds = 0;
			Craft.ReservedReturnSeconds = 0;
			Transaction.TacticalOperations.RemoveAll(
				[&Craft](const FTacticalOperationState& Operation) { return Operation.CraftId == Craft.CraftId; });
			Transaction.TacticalBattles.RemoveAll(
				[&Transaction](const FTacticalBattleState& Battle) { return !Transaction.TacticalOperations.ContainsByPredicate(
					[&Battle](const FTacticalOperationState& Operation) { return Operation.OperationId == Battle.OperationId; }); });
			EmergencyRecoveredCraftIds.Add(Craft.CraftId);
		}
	}
	for (FTacticalOperationState& Operation : Transaction.TacticalOperations)
	{
		Operation.AgentIds.Remove(Person->PersonnelId);
	}
	for (FStrategicContactState& Contact : Transaction.StrategicContacts)
	{
		if (Contact.Status == EStrategicContactStatus::Engaged
			&& !Transaction.Craft.ContainsByPredicate(
				[&Contact](const FCraftState& Craft)
				{
					return Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Contact.ContactId;
				}))
		{
			Contact.Status = EStrategicContactStatus::Detected;
		}
	}
	const FGuid InjuredBaseId = Person->BaseId;
	const FGuid InjuredPersonnelId = Person->PersonnelId;
	const int32 RemainingHealth = Person->CurrentHealth;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelInjured, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = InjuredBaseId;
	Event.PersonnelId = InjuredPersonnelId;
	Event.RuleId = Command.CauseId;
	Event.Amount = -AppliedDamage;
	Event.Quantity = RemainingHealth;
	if (bStewardshipInterrupted)
	{
		FStrategicEvent& Interrupted = AddEvent(Result,
			EStrategicEventType::PersonnelStewardshipInterrupted,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Interrupted.BaseId = InjuredBaseId;
		Interrupted.PersonnelId = InjuredPersonnelId;
		Interrupted.RuleId = Command.CauseId;
		Interrupted.PolicyId = FPersonnelStewardship::PolicyId(InterruptedFocus);
		Interrupted.PersonnelStewardshipFocus = static_cast<int32>(InterruptedFocus);
		Interrupted.PersonnelStewardshipToursCompleted = InterruptedTours;
		Interrupted.PersonnelStewardshipDurationSeconds = InterruptedRemainingSeconds;
	}
	for (const FGuid& CraftId : EmergencyRecoveredCraftIds)
	{
		const FCraftState* EmergencyCraft = FindCraft(Transaction, CraftId);
		check(EmergencyCraft != nullptr);
		FStrategicEvent& EmergencyRecovery = AddEvent(Result, EStrategicEventType::CraftRecovered, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		EmergencyRecovery.BaseId = EmergencyCraft->BaseId;
		EmergencyRecovery.CraftId = EmergencyCraft->CraftId;
		EmergencyRecovery.PersonnelId = InjuredPersonnelId;
		EmergencyRecovery.RuleId = EmergencyCraft->CraftRuleId;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FSelectPersonnelRecoveryPlanCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (!FPersonnelRecoveryPlan::IsSelected(Command.Plan))
	{
		AddError(Result, TEXT("invalid_recovery_plan"),
			TEXT("Return Path selection is outside the supported schema."));
		return Result;
	}
	if (Config.RecoverySurgeCostPerMissingHealth <= 0
		|| Config.RecoverySurgeDurationPercent <= 0 || Config.RecoverySurgeDurationPercent > 100
		|| Config.RecoveryReflectionDurationPercent < 100 || Config.RecoveryReflectionDurationPercent > 1000
		|| Config.RecoveryReflectionResolveBonus <= 0 || Config.RecoveryReflectionResolveBonus > 100)
	{
		AddError(Result, TEXT("invalid_recovery_plan_config"),
			TEXT("Return Path settings must remain within supported funding, duration, and Resolve bounds."));
		return Result;
	}

	const FPersonnelState* Person = FindPersonnel(State, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel"),
			TEXT("Personnel member does not exist in the active roster."));
		return Result;
	}
	if (Person->Status != EPersonnelStatus::Recovering
		|| Person->RecoveryPlan != EPersonnelRecoveryPlan::DecisionRequired)
	{
		AddError(Result, TEXT("recovery_plan_unavailable"),
			TEXT("This person does not have a pending Return Path decision."));
		return Result;
	}

	const FPersonnelRecoveryPlanView Evaluation =
		FPersonnelRecoveryPlan::Evaluate(State, Config, Command.PersonnelId);
	const FPersonnelRecoveryPlanOptionView* Option = Evaluation.Options.FindByPredicate(
		[&Command](const FPersonnelRecoveryPlanOptionView& Candidate)
		{
			return Candidate.Plan == Command.Plan;
		});
	if (Option == nullptr)
	{
		AddError(Result, TEXT("invalid_recovery_plan"),
			TEXT("The requested Return Path is not available in the current recovery policy."));
		return Result;
	}
	if (!Option->bAvailable)
	{
		AddError(Result,
			Option->UnavailableReasonCode.IsNone()
				? FName(TEXT("recovery_plan_unavailable"))
				: Option->UnavailableReasonCode,
			Option->UnavailableReason.IsEmpty()
				? FString(TEXT("The requested Return Path cannot currently be selected."))
				: Option->UnavailableReason);
		return Result;
	}
	if (Option->DurationSeconds <= 0 || Option->FundingCost < 0
		|| Option->FundingCost > State.Funds)
	{
		AddError(Result, TEXT("recovery_plan_unavailable"),
			TEXT("The requested Return Path changed before it could be committed."));
		return Result;
	}

	FCampaignState Transaction = State;
	FPersonnelState* SelectedPerson = FindPersonnel(Transaction, Command.PersonnelId);
	check(SelectedPerson != nullptr);
	Transaction.Funds -= Option->FundingCost;
	SelectedPerson->RemainingRecoverySeconds = Option->DurationSeconds;
	SelectedPerson->RecoveryPlan = Command.Plan;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelRecoveryPlanSelected,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = SelectedPerson->BaseId;
	Event.PersonnelId = SelectedPerson->PersonnelId;
	Event.RuleId = SelectedPerson->RoleId;
	Event.PolicyId = Option->PolicyId;
	Event.Amount = -Option->FundingCost;
	Event.Quantity = static_cast<int32>(Command.Plan);
	Event.PersonnelRecoverySeconds = Option->DurationSeconds;
	Event.PersonnelRecoveryFundingCost = Option->FundingCost;
	Event.PersonnelRecoveryResolveBonus = Option->ResolveBonus;
	if (Option->bStewardshipBenefitApplied)
	{
		const FPersonnelState* Steward = FPersonnelStewardship::FindActiveSteward(Transaction, SelectedPerson->BaseId);
		if (Steward != nullptr)
		{
			Event.RelatedPersonnelId = Steward->PersonnelId;
		}
		Event.PersonnelStewardshipFocus = static_cast<int32>(EPersonnelStewardshipFocus::RecoveryAdvocacy);
		Event.PersonnelStewardshipBenefitAmount = Option->StewardshipFundingDiscount;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = HasPendingRecoveryPlan(State);
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FBeginPersonnelStewardshipCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (!FPersonnelStewardship::IsSelected(Command.Focus))
	{
		AddError(Result, TEXT("invalid_personnel_stewardship_focus"),
			TEXT("Stewardship focus is outside the supported schema."));
		return Result;
	}
	const FPersonnelStewardshipView Evaluation = FPersonnelStewardship::Evaluate(
		State, Rules, Config, Command.PersonnelId);
	const FPersonnelStewardshipOptionView* Option = Evaluation.Options.FindByPredicate(
		[&Command](const FPersonnelStewardshipOptionView& Candidate)
		{
			return Candidate.Focus == Command.Focus;
		});
	if (Option == nullptr || !Option->bAvailable)
	{
		const FName Code = Option != nullptr
			? Option->UnavailableReasonCode
			: Evaluation.UnavailableReasonCode;
		const FString Message = Option != nullptr
			? Option->UnavailableReason
			: Evaluation.UnavailableReason;
		AddError(Result,
			Code.IsNone() ? FName(TEXT("personnel_stewardship_unavailable")) : Code,
			Message.IsEmpty() ? FString(TEXT("This Stewardship Rotation cannot begin.")) : Message);
		return Result;
	}

	FCampaignState Transaction = State;
	FPersonnelState* Person = FindPersonnel(Transaction, Command.PersonnelId);
	check(Person != nullptr);
	Person->Status = EPersonnelStatus::Stewarding;
	Person->StewardshipFocus = Command.Focus;
	Person->RemainingStewardshipSeconds = Option->DurationSeconds;
	Person->RemainingRecoverySeconds = 0;
	Person->RecoveryPlan = EPersonnelRecoveryPlan::None;
	Person->RemainingTrainingSeconds = 0;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelStewardshipStarted,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Person->BaseId;
	Event.PersonnelId = Person->PersonnelId;
	Event.RuleId = Person->RoleId;
	Event.PolicyId = Option->PolicyId;
	Event.PersonnelStewardshipFocus = static_cast<int32>(Command.Focus);
	Event.PersonnelStewardshipToursCompleted = Person->StewardshipToursCompleted;
	Event.PersonnelStewardshipDurationSeconds = Option->DurationSeconds;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FStrategicSimulationConfig& Config,
	const FBeginPersonnelTrainingCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Config.TrainingHours <= 0)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("Personnel training duration must be positive."));
		return Result;
	}
	if (!IsValidTrainingFocus(Command.Focus))
	{
		AddError(Result, TEXT("invalid_training_focus"), TEXT("Personnel training focus is outside the supported schema."));
		return Result;
	}

	FCampaignState Transaction = State;
	FPersonnelState* Person = FindPersonnel(Transaction, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel"), TEXT("Personnel member does not exist in the active roster."));
		return Result;
	}
	if (Person->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("personnel_unavailable"), TEXT("Only available personnel can begin training."));
		return Result;
	}
	if (IsAgentAssignedToCraft(Transaction, Person->PersonnelId))
	{
		AddError(Result, TEXT("personnel_assigned_to_craft"), TEXT("Personnel assigned to a craft must be removed from its roster before training."));
		return Result;
	}
	const int32 Attribute = Command.Focus == EPersonnelTrainingFocus::Accuracy ? Person->Accuracy
		: Command.Focus == EPersonnelTrainingFocus::Resolve ? Person->Resolve
		: Command.Focus == EPersonnelTrainingFocus::Mobility ? Person->Mobility
		: Person->Strength;
	if (Attribute >= 100)
	{
		AddError(Result, TEXT("attribute_at_maximum"), TEXT("Selected personnel attribute is already at its maximum."));
		return Result;
	}
	const int64 BaselineTrainingSeconds = static_cast<int64>(Config.TrainingHours) * 3600LL;
	const FPersonnelState* TrainingSteward = FPersonnelStewardship::FindActiveSteward(Transaction, Person->BaseId);
	const bool bCadreBenefit = TrainingSteward != nullptr
		&& TrainingSteward->StewardshipFocus == EPersonnelStewardshipFocus::TrainingCadre;
	if (bCadreBenefit && !FPersonnelStewardship::IsConfigValid(Config))
	{
		AddError(Result, TEXT("invalid_personnel_stewardship_config"),
			TEXT("Training Cadre settings cannot produce a bounded training reduction."));
		return Result;
	}
	int64 TrainingSeconds = BaselineTrainingSeconds;
	if (bCadreBenefit
		&& !FPersonnelStewardship::TryApplyReductionCeil(
			BaselineTrainingSeconds, Config.StewardshipReductionPercent, TrainingSeconds))
	{
		AddError(Result, TEXT("personnel_stewardship_overflow"),
			TEXT("Training Cadre duration reduction exceeds the supported numeric range."));
		return Result;
	}
	Person->Status = EPersonnelStatus::Training;
	Person->TrainingFocus = Command.Focus;
	Person->RemainingTrainingSeconds = TrainingSeconds;
	Person->RemainingRecoverySeconds = 0;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelTrainingStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Person->BaseId;
	Event.PersonnelId = Person->PersonnelId;
	Event.RuleId = Person->RoleId;
	Event.Quantity = static_cast<int32>(Command.Focus);
	if (bCadreBenefit)
	{
		Event.RelatedPersonnelId = TrainingSteward->PersonnelId;
		Event.PolicyId = FPersonnelStewardship::PolicyId(EPersonnelStewardshipFocus::TrainingCadre);
		Event.PersonnelStewardshipFocus = static_cast<int32>(EPersonnelStewardshipFocus::TrainingCadre);
		Event.PersonnelStewardshipBenefitAmount = BaselineTrainingSeconds - TrainingSeconds;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSelectPersonnelDoctrineCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FPersonnelDoctrineEvaluation Evaluation = EvaluatePersonnelDoctrine(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	const FPersonnelDoctrineRule* Doctrine = Rules.PersonnelDoctrines.Find(Command.DoctrineId);
	if (Doctrine == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel_doctrine"),
			TEXT("Personnel doctrine changed before the choice could be committed."));
		return Result;
	}
	FCampaignState Transaction = State;
	FPersonnelState* Person = FindPersonnel(Transaction, Command.PersonnelId);
	if (Person == nullptr || Person->PendingDoctrineChoices <= 0)
	{
		AddError(Result, TEXT("personnel_doctrine_choice_unavailable"),
			TEXT("Personnel promotion state changed before the doctrine choice could be committed."));
		return Result;
	}
	const int32 PreviousMaxHealth = Person->MaxHealth;
	Person->MaxHealth = FMath::Min(200, Person->MaxHealth + Doctrine->MaxHealthBonus);
	Person->CurrentHealth += Person->MaxHealth - PreviousMaxHealth;
	Person->Accuracy = FMath::Min(100, Person->Accuracy + Doctrine->AccuracyBonus);
	Person->Resolve = FMath::Min(100, Person->Resolve + Doctrine->ResolveBonus);
	Person->Mobility = FMath::Min(100, Person->Mobility + Doctrine->MobilityBonus);
	Person->Strength = FMath::Min(100, Person->Strength + Doctrine->StrengthBonus);
	Person->DoctrineSelections.Add(Command.DoctrineId);
	--Person->PendingDoctrineChoices;
	if (!ValidatePersonnelState(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BaseId = Person->BaseId;
	const int32 NewSelectionCount = static_cast<int32>(Algo::CountIf(Person->DoctrineSelections,
		[&Command](const FName DoctrineId) { return DoctrineId == Command.DoctrineId; }));
	const int32 RemainingChoices = Person->PendingDoctrineChoices;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelDoctrineSelected,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.PersonnelId = Command.PersonnelId;
	Event.RuleId = Command.DoctrineId;
	Event.Amount = NewSelectionCount;
	Event.Quantity = RemainingChoices;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetPersonnelEquipmentCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.ItemIds.Num() > 16)
	{
		AddError(Result, TEXT("equipment_capacity_exceeded"), TEXT("A personnel loadout can contain at most 16 item units."));
		return Result;
	}
	for (const FName ItemId : Command.ItemIds)
	{
		const FItemRule* Item = Rules.Items.Find(ItemId);
		if (Item == nullptr)
		{
			AddError(Result, TEXT("unknown_item"), FString::Printf(TEXT("Equipment item '%s' is not loaded."), *ItemId.ToString()));
			return Result;
		}
		if (!IsEquippablePersonnelItem(*Item))
		{
			AddError(Result, TEXT("invalid_personnel_equipment"), FString::Printf(
				TEXT("Item '%s' cannot be carried in a personnel loadout."), *ItemId.ToString()));
			return Result;
		}
		for (const FName Requirement : Item->RequiredResearch)
		{
			if (!State.CompletedResearch.Contains(Requirement))
			{
				AddError(Result, TEXT("equipment_research_missing"), FString::Printf(TEXT("Equipping '%s' requires completed research '%s'."), *ItemId.ToString(), *Requirement.ToString()));
				return Result;
			}
		}
	}

	FCampaignState Transaction = State;
	FPersonnelState* Person = FindPersonnel(Transaction, Command.PersonnelId);
	if (Person == nullptr)
	{
		AddError(Result, TEXT("unknown_personnel"), TEXT("Personnel member does not exist in the active roster."));
		return Result;
	}
	if (Person->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("personnel_unavailable"), TEXT("Only available personnel can change equipment."));
		return Result;
	}
	FStrategicBaseState* Base = FindBase(Transaction, Person->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Personnel member references a missing base."));
		return Result;
	}

	TMap<FName, int64> InventoryDeltas;
	for (const FName ItemId : Person->EquippedItems)
	{
		++InventoryDeltas.FindOrAdd(ItemId);
	}
	for (const FName ItemId : Command.ItemIds)
	{
		--InventoryDeltas.FindOrAdd(ItemId);
	}
	TArray<FName> ChangedItems;
	InventoryDeltas.GetKeys(ChangedItems);
	ChangedItems.Sort(FNameLexicalLess());
	for (const FName ItemId : ChangedItems)
	{
		const int64 Delta = InventoryDeltas.FindChecked(ItemId);
		if (Delta == 0)
		{
			continue;
		}
		if (Delta < MIN_int32 || Delta > MAX_int32 || !TryAdjustInventory(*Base, ItemId, static_cast<int32>(Delta)))
		{
			AddError(Result, Delta < 0 ? TEXT("insufficient_inventory") : TEXT("inventory_overflow"), FString::Printf(TEXT("Base inventory cannot satisfy the equipment change for '%s'."), *ItemId.ToString()));
			return Result;
		}
	}
	Person->EquippedItems = Command.ItemIds;
	const FGuid EquippedBaseId = Person->BaseId;
	const FGuid EquippedPersonnelId = Person->PersonnelId;
	const FName EquippedRoleId = Person->RoleId;
	const int32 EquippedCount = Person->EquippedItems.Num();
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, EquippedBaseId,
		TEXT("Changing this personnel loadout"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::PersonnelEquipmentChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = EquippedBaseId;
	Event.PersonnelId = EquippedPersonnelId;
	Event.RuleId = EquippedRoleId;
	Event.Quantity = EquippedCount;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FAcquireCraftCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (!Command.OrderId.IsValid()
		|| State.CraftAcquisitionOrders.ContainsByPredicate(
			[&Command](const FCraftAcquisitionOrderState& Order) { return Order.OrderId == Command.OrderId; }))
	{
		AddError(Result, TEXT("invalid_craft_order_id"), TEXT("Craft acquisition order id must be valid and unique."));
		return Result;
	}
	if (!Command.CraftId.IsValid() || IsCraftIdentityInUse(State, Command.CraftId))
	{
		AddError(Result, TEXT("invalid_craft_id"), TEXT("Craft id must be valid and unused by the active fleet and acquisition queue."));
		return Result;
	}
	const FString NormalizedName = Command.DisplayName.TrimStartAndEnd();
	if (!IsUsableCraftName(NormalizedName))
	{
		AddError(Result, TEXT("invalid_craft_name"), TEXT("Craft names must contain 1-64 non-whitespace characters."));
		return Result;
	}
	const FStrategicBaseState* Base = FindBase(State, Command.BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Craft acquisition destination base does not exist."));
		return Result;
	}
	const FCraftRule* Rule = Rules.Craft.Find(Command.CraftRuleId);
	if (Rule == nullptr)
	{
		AddError(Result, TEXT("unknown_craft_rule"), FString::Printf(TEXT("Craft rule '%s' is not loaded."), *Command.CraftRuleId.ToString()));
		return Result;
	}
	if (Rule->PurchaseCost < 0 || Rule->MonthlyMaintenance < 0 || Rule->AcquisitionHours <= 0
		|| Rule->MaxHull <= 0 || Rule->FuelCapacity <= 0 || Rule->EquipmentSlots < 0 || Rule->EquipmentSlots > 16
		|| Rule->RepairCostPerHull < 0 || Rule->RepairHoursPerHull <= 0
		|| Rule->RefuelCostPerUnit < 0 || Rule->RefuelUnitsPerHour <= 0)
	{
		AddError(Result, TEXT("invalid_craft_rule"), TEXT("Craft rule contains invalid acquisition, capacity, or service values."));
		return Result;
	}
	for (const FName Requirement : Rule->RequiredResearch)
	{
		if (!State.CompletedResearch.Contains(Requirement))
		{
			AddError(Result, TEXT("craft_research_missing"), FString::Printf(TEXT("Craft acquisition requires completed research '%s'."), *Requirement.ToString()));
			return Result;
		}
	}
	int32 Capacity = 0;
	if (!ComputeBaseCraftCapacity(*Base, Rules, Capacity, Result))
	{
		return Result;
	}
	int64 Occupied = 0;
	for (const FCraftState& Craft : State.Craft)
	{
		Occupied += Craft.BaseId == Command.BaseId ? 1 : 0;
	}
	for (const FCraftAcquisitionOrderState& Order : State.CraftAcquisitionOrders)
	{
		Occupied += Order.BaseId == Command.BaseId ? 1 : 0;
	}
	if (Occupied >= Capacity)
	{
		AddError(Result, TEXT("craft_capacity_exceeded"), FString::Printf(TEXT("Base has %d operational craft berths and all are occupied or reserved."), Capacity));
		return Result;
	}
	if (State.Funds < Rule->PurchaseCost)
	{
		AddError(Result, TEXT("insufficient_funds"), FString::Printf(TEXT("Craft acquisition requires %d funds, but only %lld are available."), Rule->PurchaseCost, State.Funds));
		return Result;
	}

	FCampaignState Transaction = State;
	Transaction.Funds -= Rule->PurchaseCost;
	FCraftAcquisitionOrderState& Order = Transaction.CraftAcquisitionOrders.AddDefaulted_GetRef();
	Order.OrderId = Command.OrderId;
	Order.CraftId = Command.CraftId;
	Order.DisplayName = NormalizedName;
	Order.CraftRuleId = Command.CraftRuleId;
	Order.BaseId = Command.BaseId;
	Order.RemainingTransitSeconds = static_cast<int64>(Rule->AcquisitionHours) * 3600LL;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftAcquisitionStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.BaseId;
	Event.ProjectId = Command.OrderId;
	Event.CraftId = Command.CraftId;
	Event.RuleId = Command.CraftRuleId;
	Event.Amount = -Rule->PurchaseCost;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FTransferCraftCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FCraftState* Craft = FindCraft(State, Command.CraftId);
	if (Craft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (Craft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Craft can rebase only while grounded and not servicing."));
		return Result;
	}
	if (!Craft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before rebasing this craft."));
		return Result;
	}
	if (Craft->AssignedPilotId.IsValid() || !Craft->AssignedAgentIds.IsEmpty())
	{
		AddError(Result, TEXT("craft_roster_assigned"), TEXT("Release the craft's pilot and field team before rebasing."));
		return Result;
	}
	const FStrategicBaseState* SourceBase = FindBase(State, Craft->BaseId);
	const FStrategicBaseState* DestinationBase = FindBase(State, Command.DestinationBaseId);
	if (SourceBase == nullptr || DestinationBase == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Craft source and destination bases must both exist."));
		return Result;
	}
	if (Craft->BaseId == Command.DestinationBaseId)
	{
		AddError(Result, TEXT("craft_already_at_base"), TEXT("Craft is already stationed at the destination base."));
		return Result;
	}
	int32 Capacity = 0;
	if (!ComputeBaseCraftCapacity(*DestinationBase, Rules, Capacity, Result))
	{
		return Result;
	}
	int64 Occupied = 0;
	for (const FCraftState& Other : State.Craft)
	{
		Occupied += Other.BaseId == Command.DestinationBaseId ? 1 : 0;
	}
	for (const FCraftAcquisitionOrderState& Order : State.CraftAcquisitionOrders)
	{
		Occupied += Order.BaseId == Command.DestinationBaseId ? 1 : 0;
	}
	if (Occupied >= Capacity)
	{
		AddError(Result, TEXT("craft_capacity_exceeded"), FString::Printf(
			TEXT("Destination base has %d operational craft berths and all are occupied or reserved."), Capacity));
		return Result;
	}

	FCampaignState Transaction = State;
	FCraftState* TransferredCraft = FindCraft(Transaction, Command.CraftId);
	check(TransferredCraft != nullptr);
	const FName CraftRuleId = TransferredCraft->CraftRuleId;
	TransferredCraft->BaseId = Command.DestinationBaseId;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftTransferred,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Command.DestinationBaseId;
	Event.CraftId = Command.CraftId;
	Event.RuleId = CraftRuleId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FAssignCraftPilotCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	if (Craft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (Craft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Pilot assignments can change only while a craft is grounded and not servicing."));
		return Result;
	}
	if (Command.PersonnelId.IsValid())
	{
		FPersonnelState* Pilot = FindPersonnel(Transaction, Command.PersonnelId);
		const FPersonnelRoleRule* Role = Pilot != nullptr ? Rules.PersonnelRoles.Find(Pilot->RoleId) : nullptr;
		if (Pilot == nullptr)
		{
			AddError(Result, TEXT("unknown_personnel"), TEXT("Assigned pilot does not exist in the active roster."));
			return Result;
		}
		if (Role == nullptr || Role->Category != EPersonnelRoleCategory::Pilot)
		{
			AddError(Result, TEXT("personnel_not_pilot"), TEXT("Only personnel with a pilot role can be assigned to craft."));
			return Result;
		}
		if (Pilot->BaseId != Craft->BaseId || Pilot->Status != EPersonnelStatus::Available)
		{
			AddError(Result, TEXT("pilot_unavailable"), TEXT("Pilot must be available at the craft's base."));
			return Result;
		}
		if (Transaction.Craft.ContainsByPredicate(
			[&Command](const FCraftState& Other) { return Other.CraftId != Command.CraftId && Other.AssignedPilotId == Command.PersonnelId; }))
		{
			AddError(Result, TEXT("pilot_already_assigned"), TEXT("Pilot is already assigned to another craft."));
			return Result;
		}
	}
	Craft->AssignedPilotId = Command.PersonnelId;
	const FGuid BaseId = Craft->BaseId;
	const FName CraftRuleId = Craft->CraftRuleId;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftPilotAssigned, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.CraftId = Command.CraftId;
	Event.PersonnelId = Command.PersonnelId;
	Event.RuleId = CraftRuleId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetCraftEquipmentCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	const FCraftRule* Rule = Rules.Craft.Find(ExistingCraft->CraftRuleId);
	if (Rule == nullptr)
	{
		AddError(Result, TEXT("unknown_craft_rule"), TEXT("Craft references an unloaded rule."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Craft equipment can change only while grounded and not servicing."));
		return Result;
	}
	if (Command.ItemIds.Num() > Rule->EquipmentSlots)
	{
		AddError(Result, TEXT("craft_equipment_capacity_exceeded"), FString::Printf(TEXT("Craft has %d equipment slots."), Rule->EquipmentSlots));
		return Result;
	}
	for (const FName ItemId : Command.ItemIds)
	{
		const FItemRule* Item = Rules.Items.Find(ItemId);
		if (Item == nullptr || !IsEquippableCraftItem(*Item)
			|| (Item->IsCraftWeapon() && !IsValidCraftWeaponRule(*Item, Rules)))
		{
			AddError(Result, TEXT("invalid_craft_equipment"), FString::Printf(TEXT("Item '%s' is not loaded craft equipment."), *ItemId.ToString()));
			return Result;
		}
		for (const FName Requirement : Item->RequiredResearch)
		{
			if (!State.CompletedResearch.Contains(Requirement))
			{
				AddError(Result, TEXT("equipment_research_missing"), FString::Printf(TEXT("Equipping '%s' requires completed research '%s'."), *ItemId.ToString(), *Requirement.ToString()));
				return Result;
			}
		}
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	check(Craft != nullptr);
	FStrategicBaseState* Base = FindBase(Transaction, Craft->BaseId);
	if (Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Craft references a missing base."));
		return Result;
	}
	TMap<FName, int64> InventoryDeltas;
	for (const FName ItemId : Craft->EquipmentItems)
	{
		++InventoryDeltas.FindOrAdd(ItemId);
	}
	for (const FCraftWeaponState& WeaponState : Craft->WeaponStates)
	{
		const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
		if (Weapon == nullptr || !IsValidCraftWeaponRule(*Weapon, Rules) || WeaponState.Ammunition < 0)
		{
			AddError(Result, TEXT("invalid_craft_weapon_state"), TEXT("Craft has invalid ammunition state that cannot be unloaded."));
			return Result;
		}
		InventoryDeltas.FindOrAdd(Weapon->AmmunitionItemId) += WeaponState.Ammunition;
	}
	for (const FName ItemId : Command.ItemIds)
	{
		--InventoryDeltas.FindOrAdd(ItemId);
	}
	TArray<FName> ChangedItems;
	InventoryDeltas.GetKeys(ChangedItems);
	ChangedItems.Sort(FNameLexicalLess());
	for (const FName ItemId : ChangedItems)
	{
		const int64 Delta = InventoryDeltas.FindChecked(ItemId);
		if (Delta == 0)
		{
			continue;
		}
		if (Delta < MIN_int32 || Delta > MAX_int32 || !TryAdjustInventory(*Base, ItemId, static_cast<int32>(Delta)))
		{
			AddError(Result, Delta < 0 ? TEXT("insufficient_inventory") : TEXT("inventory_overflow"), FString::Printf(TEXT("Base inventory cannot satisfy the craft equipment change for '%s'."), *ItemId.ToString()));
			return Result;
		}
	}
	Craft->EquipmentItems = Command.ItemIds;
	Craft->WeaponStates.Reset();
	TSet<FName> AddedWeaponIds;
	for (const FName ItemId : Craft->EquipmentItems)
	{
		const FItemRule& Item = Rules.Items.FindChecked(ItemId);
		if (Item.IsCraftWeapon() && !AddedWeaponIds.Contains(ItemId))
		{
			FCraftWeaponState& WeaponState = Craft->WeaponStates.AddDefaulted_GetRef();
			WeaponState.WeaponItemId = ItemId;
			AddedWeaponIds.Add(ItemId);
		}
	}
	const FGuid BaseId = Craft->BaseId;
	const FName CraftRuleId = Craft->CraftRuleId;
	const int32 EquippedCount = Craft->EquipmentItems.Num();
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, BaseId,
		TEXT("Changing this craft loadout"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftEquipmentChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.CraftId = Command.CraftId;
	Event.RuleId = CraftRuleId;
	Event.Quantity = EquippedCount;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FRearmCraftCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Command.Policy != ECraftRearmPolicy::FullLoad
		&& Command.Policy != ECraftRearmPolicy::LoadAvailable)
	{
		AddError(Result, TEXT("invalid_craft_rearm_policy"), TEXT("Craft rearm policy is not supported."));
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Only a grounded craft can rearm."));
		return Result;
	}
	if (!ExistingCraft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before rearming this craft."));
		return Result;
	}
	if (FindBase(State, ExistingCraft->BaseId) == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Craft references a missing home base."));
		return Result;
	}

	TMap<FName, int32> MountCounts;
	for (const FName ItemId : ExistingCraft->EquipmentItems)
	{
		const FItemRule* Item = Rules.Items.Find(ItemId);
		if (Item == nullptr || !IsEquippableCraftItem(*Item)
			|| (Item->IsCraftWeapon() && !IsValidCraftWeaponRule(*Item, Rules)))
		{
			AddError(Result, TEXT("invalid_craft_equipment"), FString::Printf(TEXT("Craft references invalid equipment item '%s'."), *ItemId.ToString()));
			return Result;
		}
		if (Item->IsCraftWeapon())
		{
			++MountCounts.FindOrAdd(ItemId);
		}
	}
	if (MountCounts.IsEmpty())
	{
		AddError(Result, TEXT("craft_has_no_weapons"), TEXT("Craft has no equipped weapons to rearm."));
		return Result;
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	FStrategicBaseState* Base = Craft != nullptr ? FindBase(Transaction, Craft->BaseId) : nullptr;
	check(Craft != nullptr && Base != nullptr);
	TArray<FName> WeaponIds;
	MountCounts.GetKeys(WeaponIds);
	WeaponIds.Sort(FNameLexicalLess());
	int64 TotalLoaded = 0;
	int64 TotalMissing = 0;
	for (const FName WeaponId : WeaponIds)
	{
		const FItemRule& Weapon = Rules.Items.FindChecked(WeaponId);
		const int64 Capacity = static_cast<int64>(Weapon.MagazineCapacity) * MountCounts.FindChecked(WeaponId);
		if (!IsValidCraftWeaponRule(Weapon, Rules) || Capacity <= 0 || Capacity > MAX_int32)
		{
			AddError(Result, TEXT("invalid_craft_weapon_rule"), FString::Printf(TEXT("Weapon '%s' has invalid ammunition capacity or item."), *WeaponId.ToString()));
			return Result;
		}
		FCraftWeaponState* WeaponState = Craft->WeaponStates.FindByPredicate(
			[WeaponId](const FCraftWeaponState& Entry) { return Entry.WeaponItemId == WeaponId; });
		if (WeaponState == nullptr)
		{
			WeaponState = &Craft->WeaponStates.AddDefaulted_GetRef();
			WeaponState->WeaponItemId = WeaponId;
		}
		if (WeaponState->Ammunition < 0 || WeaponState->Ammunition > Capacity)
		{
			AddError(Result, TEXT("invalid_craft_weapon_state"), FString::Printf(TEXT("Weapon '%s' has invalid loaded ammunition."), *WeaponId.ToString()));
			return Result;
		}
		const int64 Missing = Capacity - WeaponState->Ammunition;
		int64 LoadAmount = Missing;
		if (Command.Policy == ECraftRearmPolicy::LoadAvailable && Missing > 0)
		{
			const FInventoryStack* Ammunition = Base->Inventory.FindByPredicate(
				[&Weapon](const FInventoryStack& Stack) { return Stack.ItemId == Weapon.AmmunitionItemId; });
			const int64 Available = Ammunition != nullptr ? FMath::Max(0, Ammunition->Quantity) : 0;
			LoadAmount = FMath::Min(Missing, Available);
		}
		if (LoadAmount > 0 && (LoadAmount > MAX_int32
			|| !TryAdjustInventory(*Base, Weapon.AmmunitionItemId, -static_cast<int32>(LoadAmount))))
		{
			AddError(Result, TEXT("insufficient_ammunition"), FString::Printf(TEXT("Rearming '%s' requires %lld units of '%s'."), *WeaponId.ToString(), Missing, *Weapon.AmmunitionItemId.ToString()));
			return Result;
		}
		WeaponState->Ammunition += static_cast<int32>(LoadAmount);
		WeaponState->RemainingCooldownSeconds = 0;
		TotalLoaded += LoadAmount;
		TotalMissing += Capacity - WeaponState->Ammunition;
	}
	if (TotalLoaded == 0)
	{
		if (TotalMissing == 0)
		{
			AddError(Result, TEXT("craft_rearm_not_needed"), TEXT("Every equipped weapon is already fully armed."));
		}
		else
		{
			AddError(Result, TEXT("insufficient_ammunition"), TEXT("No compatible ammunition is available at this craft's base."));
		}
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftRearmed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Craft->BaseId;
	Event.CraftId = Craft->CraftId;
	Event.RuleId = Craft->CraftRuleId;
	Event.Quantity = TotalLoaded > MAX_int32 ? MAX_int32 : static_cast<int32>(TotalLoaded);
	Event.Amount = TotalMissing;
	Event.bSuccessful = TotalMissing == 0;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FBeginCraftServiceCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	if (Craft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	const FCraftRule* Rule = Rules.Craft.Find(Craft->CraftRuleId);
	if (Rule == nullptr || Rule->RepairCostPerHull < 0 || Rule->RepairHoursPerHull <= 0 || Rule->RefuelCostPerUnit < 0 || Rule->RefuelUnitsPerHour <= 0)
	{
		AddError(Result, TEXT("invalid_craft_rule"), TEXT("Craft service rule is missing or invalid."));
		return Result;
	}
	if (Craft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Only grounded craft can begin servicing."));
		return Result;
	}
	if (!Craft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before servicing this craft."));
		return Result;
	}
	if (Craft->CurrentHull <= 0 || Craft->CurrentHull > Rule->MaxHull || Craft->CurrentFuel < 0 || Craft->CurrentFuel > Rule->FuelCapacity)
	{
		AddError(Result, TEXT("invalid_craft_state"), TEXT("Craft hull or fuel state is outside its rule limits."));
		return Result;
	}
	const int64 MissingHull = Rule->MaxHull - Craft->CurrentHull;
	const int64 MissingFuel = Rule->FuelCapacity - Craft->CurrentFuel;
	if (MissingHull == 0 && MissingFuel == 0)
	{
		AddError(Result, TEXT("craft_service_not_needed"), TEXT("Craft is already fully repaired and fueled."));
		return Result;
	}
	int64 RepairCost = 0;
	int64 RefuelCost = 0;
	int64 TotalCost = 0;
	if (!TryMultiplyNonNegative(MissingHull, Rule->RepairCostPerHull, RepairCost)
		|| !TryMultiplyNonNegative(MissingFuel, Rule->RefuelCostPerUnit, RefuelCost)
		|| !TryAdd(RepairCost, RefuelCost, TotalCost))
	{
		AddError(Result, TEXT("financial_overflow"), TEXT("Craft service cost exceeds the campaign numeric range."));
		return Result;
	}
	if (Transaction.Funds < TotalCost)
	{
		AddError(Result, TEXT("insufficient_funds"), FString::Printf(TEXT("Craft service requires %lld funds, but only %lld are available."), TotalCost, Transaction.Funds));
		return Result;
	}
	int64 RepairHours = 0;
	int64 RepairSeconds = 0;
	int64 RefuelSeconds = 0;
	if (!TryMultiplyNonNegative(MissingHull, Rule->RepairHoursPerHull, RepairHours)
		|| !TryMultiplyNonNegative(RepairHours, 3600, RepairSeconds))
	{
		AddError(Result, TEXT("craft_service_time_overflow"), TEXT("Craft repair time exceeds the campaign numeric range."));
		return Result;
	}
	if (MissingFuel > 0)
	{
		const int64 RefuelHours = (MissingFuel + Rule->RefuelUnitsPerHour - 1) / Rule->RefuelUnitsPerHour;
		if (!TryMultiplyNonNegative(RefuelHours, 3600, RefuelSeconds))
		{
			AddError(Result, TEXT("craft_service_time_overflow"), TEXT("Craft refuel time exceeds the campaign numeric range."));
			return Result;
		}
	}
	Transaction.Funds -= TotalCost;
	Craft->Status = ECraftStatus::Servicing;
	Craft->RemainingRepairSeconds = RepairSeconds;
	Craft->RemainingRefuelSeconds = RefuelSeconds;
	const FGuid BaseId = Craft->BaseId;
	const FName CraftRuleId = Craft->CraftRuleId;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftServiceStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.CraftId = Command.CraftId;
	Event.RuleId = CraftRuleId;
	Event.Amount = -TotalCost;
	Event.Quantity = MissingHull + MissingFuel > MAX_int32
		? MAX_int32
		: static_cast<int32>(MissingHull + MissingFuel);
	const FCraftServiceQueueSnapshot ServiceQueue =
		FCraftServiceQueue::Evaluate(Transaction, Rules);
	if (const FCraftServiceQueueView* Queue = ServiceQueue.FindCraft(Command.CraftId))
	{
		FStrategicEvent& Scheduled = AddCraftServiceRotationEvent(
			Result, *Queue, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Scheduled.RuleId = CraftRuleId;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FCancelCraftServiceCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	if (Craft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (Craft->Status != ECraftStatus::Servicing
		|| (Craft->RemainingRepairSeconds == 0 && Craft->RemainingRefuelSeconds == 0))
	{
		AddError(Result, TEXT("craft_service_not_active"), TEXT("This craft has no active service to cancel."));
		return Result;
	}
	const FCraftRule* Rule = Rules.Craft.Find(Craft->CraftRuleId);
	if (Rule == nullptr || Rule->MaxHull <= 0 || Rule->FuelCapacity <= 0
		|| Rule->RepairCostPerHull < 0 || Rule->RefuelCostPerUnit < 0)
	{
		AddError(Result, TEXT("invalid_craft_rule"), TEXT("Craft service rule is missing or invalid."));
		return Result;
	}
	const bool bRepairActive = Craft->RemainingRepairSeconds > 0;
	const bool bRefuelActive = Craft->RemainingRefuelSeconds > 0;
	if (Craft->RemainingRepairSeconds < 0 || Craft->RemainingRefuelSeconds < 0
		|| Craft->CurrentHull <= 0 || Craft->CurrentHull > Rule->MaxHull
		|| Craft->CurrentFuel < 0 || Craft->CurrentFuel > Rule->FuelCapacity
		|| (bRepairActive ? Craft->CurrentHull >= Rule->MaxHull : Craft->CurrentHull != Rule->MaxHull)
		|| (bRefuelActive ? Craft->CurrentFuel >= Rule->FuelCapacity : Craft->CurrentFuel != Rule->FuelCapacity))
	{
		AddError(Result, TEXT("invalid_craft_state"), TEXT("Craft service state is inconsistent with its remaining work."));
		return Result;
	}

	const int64 CancelledHull = bRepairActive ? Rule->MaxHull - Craft->CurrentHull : 0;
	const int64 CancelledFuel = bRefuelActive ? Rule->FuelCapacity - Craft->CurrentFuel : 0;
	int64 RepairRefund = 0;
	int64 RefuelRefund = 0;
	int64 Refund = 0;
	int64 NewFunds = 0;
	if (!TryMultiplyNonNegative(CancelledHull, Rule->RepairCostPerHull, RepairRefund)
		|| !TryMultiplyNonNegative(CancelledFuel, Rule->RefuelCostPerUnit, RefuelRefund)
		|| !TryAdd(RepairRefund, RefuelRefund, Refund)
		|| !TryAdd(Transaction.Funds, Refund, NewFunds))
	{
		AddError(Result, TEXT("economy_overflow"), TEXT("Craft service cancellation refund exceeds the campaign numeric range."));
		return Result;
	}
	const int64 CancelledUnits = CancelledHull + CancelledFuel;
	const FCraftServiceQueueSnapshot InitialServiceQueue =
		FCraftServiceQueue::Evaluate(Transaction, Rules);
	TSet<FGuid> InitiallyActiveServiceCraftIds;
	InitiallyActiveServiceCraftIds.Reserve(InitialServiceQueue.Craft.Num());
	for (const FCraftServiceQueueView& Queue : InitialServiceQueue.Craft)
	{
		if (Queue.bInServiceLane)
		{
			InitiallyActiveServiceCraftIds.Add(Queue.CraftId);
		}
	}

	const FGuid BaseId = Craft->BaseId;
	const FName CraftRuleId = Craft->CraftRuleId;
	Transaction.Funds = NewFunds;
	Craft->Status = ECraftStatus::Grounded;
	Craft->RemainingRepairSeconds = 0;
	Craft->RemainingRefuelSeconds = 0;
	++Transaction.CommandSequence;
	FStrategicEvent& Cancelled = AddEvent(Result, EStrategicEventType::CraftServiceCancelled,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Cancelled.BaseId = BaseId;
	Cancelled.CraftId = Command.CraftId;
	Cancelled.RuleId = CraftRuleId;
	Cancelled.Amount = Refund;
	Cancelled.Quantity = CancelledUnits > MAX_int32 ? MAX_int32 : static_cast<int32>(CancelledUnits);
	Cancelled.bSuccessful = true;
	const FCraftServiceQueueSnapshot UpdatedServiceQueue =
		FCraftServiceQueue::Evaluate(Transaction, Rules);
	for (const FCraftServiceQueueView& Queue : UpdatedServiceQueue.Craft)
	{
		if (!Queue.bInServiceLane || InitiallyActiveServiceCraftIds.Contains(Queue.CraftId))
		{
			continue;
		}
		FStrategicEvent& Scheduled = AddCraftServiceRotationEvent(
			Result, Queue, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		if (const FCraftState* PromotedCraft = FindCraft(Transaction, Queue.CraftId))
		{
			Scheduled.RuleId = PromotedCraft->CraftRuleId;
		}
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FLaunchCraftCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	if (Craft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	const FCraftRule* Rule = Rules.Craft.Find(Craft->CraftRuleId);
	if (Rule == nullptr || Rule->FuelBurnPerHour <= 0)
	{
		AddError(Result, TEXT("invalid_craft_rule"), TEXT("Craft performance rule is missing or invalid."));
		return Result;
	}
	if (Craft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Only a grounded craft can launch."));
		return Result;
	}
	if (!Craft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before launching this craft."));
		return Result;
	}
	if (Command.FuelUnits < Rule->FuelBurnPerHour || Command.FuelUnits > Craft->CurrentFuel)
	{
		AddError(Result, TEXT("invalid_sortie_fuel"), FString::Printf(TEXT("Sortie must reserve at least %d fuel units without exceeding current fuel %d."), Rule->FuelBurnPerHour, Craft->CurrentFuel));
		return Result;
	}
	if (!Craft->AssignedPilotId.IsValid())
	{
		AddError(Result, TEXT("craft_pilot_missing"), TEXT("Craft requires an assigned available pilot before launch."));
		return Result;
	}
	FPersonnelState* Pilot = FindPersonnel(Transaction, Craft->AssignedPilotId);
	const FPersonnelRoleRule* PilotRole = Pilot != nullptr ? Rules.PersonnelRoles.Find(Pilot->RoleId) : nullptr;
	if (Pilot == nullptr || PilotRole == nullptr || PilotRole->Category != EPersonnelRoleCategory::Pilot
		|| Pilot->BaseId != Craft->BaseId || Pilot->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("pilot_unavailable"), TEXT("Assigned pilot is not available at the craft's base."));
		return Result;
	}
	for (const FGuid& AgentId : Craft->AssignedAgentIds)
	{
		const FPersonnelState* Agent = FindPersonnel(Transaction, AgentId);
		const FPersonnelRoleRule* AgentRole = Agent != nullptr ? Rules.PersonnelRoles.Find(Agent->RoleId) : nullptr;
		if (Agent == nullptr || AgentRole == nullptr || AgentRole->Category != EPersonnelRoleCategory::FieldAgent
			|| Agent->BaseId != Craft->BaseId || Agent->Status != EPersonnelStatus::Available)
		{
			AddError(Result, TEXT("agent_unavailable"), TEXT("Every assigned field agent must be available at the craft's base before launch."));
			return Result;
		}
	}
	Craft->CurrentFuel -= Command.FuelUnits;
	Craft->Status = ECraftStatus::Airborne;
	Craft->TargetContactId.Invalidate();
	Craft->TargetSiteId.Invalidate();
	Craft->RemainingRouteSeconds = 0;
	Craft->ReservedReturnSeconds = 0;
	Pilot->Status = EPersonnelStatus::Deployed;
	for (const FGuid& AgentId : Craft->AssignedAgentIds)
	{
		FindPersonnel(Transaction, AgentId)->Status = EPersonnelStatus::Deployed;
	}
	const FGuid BaseId = Craft->BaseId;
	const FGuid PilotId = Pilot->PersonnelId;
	const FName CraftRuleId = Craft->CraftRuleId;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftLaunched, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.CraftId = Command.CraftId;
	Event.PersonnelId = PilotId;
	Event.RuleId = CraftRuleId;
	Event.Quantity = Command.FuelUnits;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FRecoverCraftCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	if (Craft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (Rules.Craft.Find(Craft->CraftRuleId) == nullptr)
	{
		AddError(Result, TEXT("unknown_craft_rule"), TEXT("Craft references an unloaded rule."));
		return Result;
	}
	if (Craft->Status != ECraftStatus::Airborne || !Craft->AssignedPilotId.IsValid())
	{
		AddError(Result, TEXT("craft_not_airborne"), TEXT("Only an airborne craft with an assigned pilot can recover."));
		return Result;
	}
	FPersonnelState* Pilot = FindPersonnel(Transaction, Craft->AssignedPilotId);
	if (Pilot == nullptr || Pilot->Status != EPersonnelStatus::Deployed)
	{
		AddError(Result, TEXT("invalid_craft_pilot"), TEXT("Airborne craft pilot state is invalid."));
		return Result;
	}
	if (Craft->CompletedSorties == MAX_int32)
	{
		AddError(Result, TEXT("craft_sortie_overflow"), TEXT("Craft sortie count cannot accept another completed mission."));
		return Result;
	}
	const FGuid BaseId = Craft->BaseId;
	const FGuid PilotId = Pilot->PersonnelId;
	const FName CraftRuleId = Craft->CraftRuleId;
	const FGuid ContactId = Craft->TargetContactId;
	if (ContactId.IsValid())
	{
		if (Craft->ReservedReturnSeconds <= 0)
		{
			AddError(Result, TEXT("invalid_craft_route"), TEXT("On-station craft has no valid return route."));
			return Result;
		}
		Craft->Status = ECraftStatus::Returning;
		Craft->TargetContactId.Invalidate();
		Craft->RemainingRouteSeconds = Craft->ReservedReturnSeconds;
		if (FStrategicContactState* Contact = FindContact(Transaction, ContactId))
		{
			if (Contact->Status == EStrategicContactStatus::Engaged
				&& !Transaction.Craft.ContainsByPredicate(
					[&ContactId](const FCraftState& Other)
					{
						return Other.Status == ECraftStatus::Airborne && Other.TargetContactId == ContactId;
					}))
			{
				Contact->Status = EStrategicContactStatus::Detected;
			}
		}
		++Transaction.CommandSequence;
		FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftReturnStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Event.BaseId = BaseId;
		Event.CraftId = Command.CraftId;
		Event.PersonnelId = PilotId;
		Event.ContactId = ContactId;
		Event.RuleId = CraftRuleId;
		State = MoveTemp(Transaction);
		Result.bAccepted = true;
		return Result;
	}
	Craft->Status = ECraftStatus::Grounded;
	Craft->TargetSiteId.Invalidate();
	Craft->RemainingRouteSeconds = 0;
	Craft->ReservedReturnSeconds = 0;
	++Craft->CompletedSorties;
	Pilot->Status = EPersonnelStatus::Available;
	for (const FGuid& AgentId : Craft->AssignedAgentIds)
	{
		FPersonnelState* Agent = FindPersonnel(Transaction, AgentId);
		if (Agent == nullptr || Agent->Status != EPersonnelStatus::Deployed)
		{
			AddError(Result, TEXT("invalid_craft_agent"), TEXT("Airborne craft field-agent state is invalid."));
			return Result;
		}
		Agent->Status = EPersonnelStatus::Available;
	}
	const int32 Sorties = Craft->CompletedSorties;
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftRecovered, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.CraftId = Command.CraftId;
	Event.PersonnelId = PilotId;
	Event.RuleId = CraftRuleId;
	Event.Quantity = Sorties;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetCraftAgentsCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	const FCraftRule* CraftRule = ExistingCraft != nullptr ? Rules.Craft.Find(ExistingCraft->CraftRuleId) : nullptr;
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (CraftRule == nullptr)
	{
		AddError(Result, TEXT("unknown_craft_rule"), TEXT("Craft references an unloaded rule."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Craft rosters can change only while grounded and not servicing."));
		return Result;
	}
	if (Command.PersonnelIds.Num() > CraftRule->AgentCapacity)
	{
		AddError(Result, TEXT("craft_agent_capacity_exceeded"), FString::Printf(TEXT("Craft supports at most %d field agents."), CraftRule->AgentCapacity));
		return Result;
	}
	TSet<FGuid> SeenAgentIds;
	for (const FGuid& AgentId : Command.PersonnelIds)
	{
		const FPersonnelState* Agent = FindPersonnel(State, AgentId);
		const FPersonnelRoleRule* AgentRole = Agent != nullptr ? Rules.PersonnelRoles.Find(Agent->RoleId) : nullptr;
		if (!AgentId.IsValid() || SeenAgentIds.Contains(AgentId))
		{
			AddError(Result, TEXT("invalid_craft_agent"), TEXT("Craft roster contains an invalid or duplicate personnel id."));
			return Result;
		}
		if (Agent == nullptr)
		{
			AddError(Result, TEXT("unknown_personnel"), TEXT("Craft roster references personnel outside the active roster."));
			return Result;
		}
		if (AgentRole == nullptr || AgentRole->Category != EPersonnelRoleCategory::FieldAgent)
		{
			AddError(Result, TEXT("personnel_not_field_agent"), TEXT("Only field-agent personnel can join a craft roster."));
			return Result;
		}
		if (Agent->BaseId != ExistingCraft->BaseId || Agent->Status != EPersonnelStatus::Available)
		{
			AddError(Result, TEXT("agent_unavailable"), TEXT("Field agents must be available at the craft's home base."));
			return Result;
		}
		if (State.Craft.ContainsByPredicate(
			[&Command, &AgentId](const FCraftState& Other)
			{
				return Other.CraftId != Command.CraftId && Other.AssignedAgentIds.Contains(AgentId);
			}))
		{
			AddError(Result, TEXT("agent_already_assigned"), TEXT("A field agent is already assigned to another craft."));
			return Result;
		}
		SeenAgentIds.Add(AgentId);
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	check(Craft != nullptr);
	Craft->AssignedAgentIds = Command.PersonnelIds;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftAgentsChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Craft->BaseId;
	Event.CraftId = Craft->CraftId;
	Event.RuleId = Craft->CraftRuleId;
	Event.Quantity = Craft->AssignedAgentIds.Num();
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetCraftCargoCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	const FCraftRule* CraftRule = ExistingCraft != nullptr ? Rules.Craft.Find(ExistingCraft->CraftRuleId) : nullptr;
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (CraftRule == nullptr)
	{
		AddError(Result, TEXT("unknown_craft_rule"), TEXT("Craft references an unloaded rule."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Craft cargo can change only while grounded and not servicing."));
		return Result;
	}
	if (!ExistingCraft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before editing this craft's cargo manifest."));
		return Result;
	}
	if (Command.Cargo.Num() > 64)
	{
		AddError(Result, TEXT("craft_cargo_stack_limit"), TEXT("Craft cargo can contain at most 64 distinct item stacks."));
		return Result;
	}
	int64 CargoMass = 0;
	if (!TryComputeCargoMass(Command.Cargo, Rules, CargoMass))
	{
		AddError(Result, TEXT("invalid_craft_cargo"), TEXT("Craft cargo contains an unknown, duplicate, non-positive, or numerically invalid stack."));
		return Result;
	}
	if (CargoMass > CraftRule->CargoCapacity)
	{
		AddError(Result, TEXT("craft_cargo_capacity_exceeded"), FString::Printf(TEXT("Cargo mass %lld exceeds craft capacity %d."), CargoMass, CraftRule->CargoCapacity));
		return Result;
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	FStrategicBaseState* Base = Craft != nullptr ? FindBase(Transaction, Craft->BaseId) : nullptr;
	if (Craft == nullptr || Base == nullptr)
	{
		AddError(Result, TEXT("unknown_base"), TEXT("Craft references a missing home base."));
		return Result;
	}
	TMap<FName, int64> InventoryDeltas;
	for (const FInventoryStack& Stack : Craft->Cargo)
	{
		InventoryDeltas.FindOrAdd(Stack.ItemId) += Stack.Quantity;
	}
	for (const FInventoryStack& Stack : Command.Cargo)
	{
		InventoryDeltas.FindOrAdd(Stack.ItemId) -= Stack.Quantity;
	}
	TArray<FName> ChangedItems;
	InventoryDeltas.GetKeys(ChangedItems);
	ChangedItems.Sort(FNameLexicalLess());
	for (const FName ItemId : ChangedItems)
	{
		const int64 Delta = InventoryDeltas.FindChecked(ItemId);
		if (Delta == 0)
		{
			continue;
		}
		if (Delta < MIN_int32 || Delta > MAX_int32 || !TryAdjustInventory(*Base, ItemId, static_cast<int32>(Delta)))
		{
			AddError(Result, Delta < 0 ? TEXT("insufficient_inventory") : TEXT("inventory_overflow"), FString::Printf(TEXT("Base inventory cannot satisfy the craft cargo change for '%s'."), *ItemId.ToString()));
			return Result;
		}
	}
	Craft->Cargo = Command.Cargo;
	int64 TotalUnits = 0;
	for (const FInventoryStack& Stack : Craft->Cargo)
	{
		TotalUnits += Stack.Quantity;
	}
	if (!ValidatePlayerStorageTransition(State, Transaction, Rules, Craft->BaseId,
		TEXT("Changing this craft manifest"), Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftCargoChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Craft->BaseId;
	Event.CraftId = Craft->CraftId;
	Event.RuleId = Craft->CraftRuleId;
	Event.Amount = CargoMass;
	Event.Quantity = TotalUnits > MAX_int32 ? MAX_int32 : static_cast<int32>(TotalUnits);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FResolveCraftSalvageCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Recovered salvage can be retained or sold only after its craft has landed."));
		return Result;
	}
	if (Command.Quantity <= 0)
	{
		AddError(Result, TEXT("invalid_sale_quantity"), TEXT("Salvage disposition quantity must be positive."));
		return Result;
	}
	if (Command.Disposition != ECraftSalvageDisposition::RetainAtBase
		&& Command.Disposition != ECraftSalvageDisposition::Sell)
	{
		AddError(Result, TEXT("invalid_salvage_disposition"), TEXT("Recovered salvage must be retained at the base or sold."));
		return Result;
	}
	const FInventoryStack* PendingStack = ExistingCraft->PendingSalvage.FindByPredicate(
		[&Command](const FInventoryStack& Stack) { return Stack.ItemId == Command.ItemId; });
	const FInventoryStack* CargoStack = ExistingCraft->Cargo.FindByPredicate(
		[&Command](const FInventoryStack& Stack) { return Stack.ItemId == Command.ItemId; });
	if (PendingStack == nullptr || CargoStack == nullptr
		|| PendingStack->Quantity < Command.Quantity || CargoStack->Quantity < Command.Quantity)
	{
		AddError(Result, TEXT("salvage_unavailable"), TEXT("The requested recovered salvage is no longer awaiting disposition on this craft."));
		return Result;
	}
	const FItemRule* Item = Rules.Items.Find(Command.ItemId);
	if (Item == nullptr)
	{
		AddError(Result, TEXT("unknown_item"), TEXT("Recovered salvage item is not present in the active rules."));
		return Result;
	}

	int64 Proceeds = 0;
	int64 NewFunds = State.Funds;
	if (Command.Disposition == ECraftSalvageDisposition::Sell)
	{
		if (Item->SellValue <= 0)
		{
			AddError(Result, TEXT("item_not_sellable"), TEXT("This recovered item has no positive disposition value."));
			return Result;
		}
		if (!TryMultiplyNonNegative(Item->SellValue, Command.Quantity, Proceeds)
			|| !TryAdd(State.Funds, Proceeds, NewFunds))
		{
			AddError(Result, TEXT("economy_overflow"), TEXT("Salvage sale proceeds exceed the campaign numeric range."));
			return Result;
		}
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	FStrategicBaseState* Base = Craft != nullptr ? FindBase(Transaction, Craft->BaseId) : nullptr;
	if (Craft == nullptr || Base == nullptr
		|| !TryAdjustInventoryStacks(Craft->Cargo, Command.ItemId, -Command.Quantity)
		|| !TryAdjustInventoryStacks(Craft->PendingSalvage, Command.ItemId, -Command.Quantity))
	{
		AddError(Result, TEXT("inventory_transaction_failed"), TEXT("Recovered salvage disposition could not be applied atomically."));
		return Result;
	}
	if (Command.Disposition == ECraftSalvageDisposition::RetainAtBase)
	{
		if (!TryAdjustInventory(*Base, Command.ItemId, Command.Quantity))
		{
			AddError(Result, TEXT("inventory_overflow"), TEXT("Retaining recovered salvage would overflow base inventory."));
			return Result;
		}
		if (!ValidatePlayerStorageTransition(State, Transaction, Rules, Craft->BaseId,
			TEXT("Retaining this recovered salvage"), Result))
		{
			return Result;
		}
	}
	else
	{
		Transaction.Funds = NewFunds;
	}
	if (!ValidateCraftState(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BaseId = Craft->BaseId;
	const FGuid CraftId = Craft->CraftId;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result,
		Command.Disposition == ECraftSalvageDisposition::RetainAtBase
			? EStrategicEventType::CraftSalvageRetained
			: EStrategicEventType::CraftSalvageSold,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = BaseId;
	Event.CraftId = CraftId;
	Event.RuleId = Command.ItemId;
	Event.Amount = Proceeds;
	Event.Quantity = Command.Quantity;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FDeployCraftToSiteCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Craft cannot deploy after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	const FStrategicSiteState* ExistingSite = FindSite(State, Command.SiteId);
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	if (ExistingSite == nullptr)
	{
		AddError(Result, TEXT("unknown_site"), TEXT("Strategic site does not exist or has expired."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Only a grounded craft can deploy to a strategic site."));
		return Result;
	}
	if (!ExistingCraft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before deploying this craft."));
		return Result;
	}
	if (ExistingCraft->AssignedAgentIds.IsEmpty())
	{
		AddError(Result, TEXT("craft_agents_missing"), TEXT("Site deployment requires at least one assigned field agent."));
		return Result;
	}
	if (State.Craft.ContainsByPredicate(
		[&Command](const FCraftState& Craft) { return Craft.CraftId != Command.CraftId && Craft.TargetSiteId == Command.SiteId; })
		|| State.TacticalOperations.ContainsByPredicate(
			[&Command](const FTacticalOperationState& Operation) { return Operation.SiteId == Command.SiteId; }))
	{
		AddError(Result, TEXT("site_already_targeted"), TEXT("Another craft or tactical operation already reserves this strategic site."));
		return Result;
	}
	const FCraftRule* CraftRule = Rules.Craft.Find(ExistingCraft->CraftRuleId);
	const FStrategicBaseState* Base = FindBase(State, ExistingCraft->BaseId);
	const FPersonnelState* Pilot = FindPersonnel(State, ExistingCraft->AssignedPilotId);
	const FPersonnelRoleRule* PilotRole = Pilot != nullptr ? Rules.PersonnelRoles.Find(Pilot->RoleId) : nullptr;
	if (CraftRule == nullptr || Base == nullptr)
	{
		AddError(Result, TEXT("invalid_deployment_rules"), TEXT("Craft or home-base data required for site deployment is missing."));
		return Result;
	}
	if (Pilot == nullptr || PilotRole == nullptr || PilotRole->Category != EPersonnelRoleCategory::Pilot
		|| Pilot->BaseId != ExistingCraft->BaseId || Pilot->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("pilot_unavailable"), TEXT("Site deployment requires an available assigned pilot at the craft's base."));
		return Result;
	}
	const int64 Distance = ApproximateSurfaceDistanceKilometers(
		Base->LongitudeMilliDegrees,
		Base->LatitudeMilliDegrees,
		ExistingSite->LongitudeMilliDegrees,
		ExistingSite->LatitudeMilliDegrees);
	int64 OneWaySeconds = 0;
	int64 RoundTripSeconds = 0;
	int64 FuelNumerator = 0;
	if (!ComputeTravelSeconds(Distance, CraftRule->CruiseSpeedKilometersPerHour, OneWaySeconds)
		|| !TryMultiplyNonNegative(OneWaySeconds, 2, RoundTripSeconds)
		|| !TryMultiplyNonNegative(RoundTripSeconds, CraftRule->FuelBurnPerHour, FuelNumerator)
		|| FuelNumerator > MAX_int64 - 3599LL)
	{
		AddError(Result, TEXT("deployment_range_overflow"), TEXT("Site deployment route exceeds the supported numeric range."));
		return Result;
	}
	const int64 RequiredFuel = (FuelNumerator + 3599LL) / 3600LL;
	if (RequiredFuel <= 0 || RequiredFuel > ExistingCraft->CurrentFuel)
	{
		AddError(Result, TEXT("insufficient_deployment_fuel"), FString::Printf(TEXT("Round-trip site deployment requires %lld fuel units, but craft has %d."), RequiredFuel, ExistingCraft->CurrentFuel));
		return Result;
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	FPersonnelState* TransactionPilot = FindPersonnel(Transaction, ExistingCraft->AssignedPilotId);
	check(Craft != nullptr && TransactionPilot != nullptr);
	Craft->CurrentFuel -= static_cast<int32>(RequiredFuel);
	Craft->Status = ECraftStatus::Deploying;
	Craft->TargetContactId.Invalidate();
	Craft->TargetSiteId = Command.SiteId;
	Craft->RemainingRouteSeconds = OneWaySeconds;
	Craft->ReservedReturnSeconds = OneWaySeconds;
	TransactionPilot->Status = EPersonnelStatus::Deployed;
	for (const FGuid& AgentId : Craft->AssignedAgentIds)
	{
		FindPersonnel(Transaction, AgentId)->Status = EPersonnelStatus::Deployed;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftDeployedToSite, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Craft->BaseId;
	Event.CraftId = Craft->CraftId;
	Event.PersonnelId = TransactionPilot->PersonnelId;
	Event.SiteId = Command.SiteId;
	Event.RuleId = ExistingSite->SourceContactRuleId;
	Event.Amount = -RequiredFuel;
	Event.Quantity = OneWaySeconds > MAX_int32 ? MAX_int32 : static_cast<int32>(OneWaySeconds);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FDeployBaseDefenseOperationCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FBaseDefenseDeploymentEvaluation Evaluation = EvaluateBaseDefenseDeployment(State, Rules, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Base defenders cannot deploy after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}

	const int64 NextSequence = State.CommandSequence + 1;
	const FGuid OperationId = MakeDeterministicTacticalOperationId(Evaluation.BaseId, Evaluation.AssaultId, NextSequence);
	if (!OperationId.IsValid() || FindTacticalOperation(State, OperationId) != nullptr)
	{
		AddError(Result, TEXT("tactical_operation_identity_conflict"), TEXT("Base-defense tactical operation identity is already in use."));
		return Result;
	}
	const uint64 TacticalSalt = (static_cast<uint64>(OperationId.A) << 32)
		^ OperationId.B ^ (static_cast<uint64>(OperationId.C) << 16) ^ OperationId.D;
	const FDeterministicRandomStream TacticalStream = State.SimulationRandom.Fork(TacticalSalt);

	FCampaignState Transaction = State;
	for (const FGuid& AgentId : Evaluation.AgentIds)
	{
		FPersonnelState* Agent = FindPersonnel(Transaction, AgentId);
		if (Agent == nullptr || Agent->Status != EPersonnelStatus::Available)
		{
			AddError(Result, TEXT("base_defender_unavailable"), TEXT("A selected base defender became unavailable before deployment."));
			return Result;
		}
		Agent->Status = EPersonnelStatus::Deployed;
	}
	FTacticalOperationState& Operation = Transaction.TacticalOperations.AddDefaulted_GetRef();
	Operation.OperationId = OperationId;
	Operation.Type = ETacticalOperationType::BaseDefense;
	Operation.BaseId = Evaluation.BaseId;
	Operation.AssaultId = Evaluation.AssaultId;
	Operation.TacticalSeed = TacticalStream.InitialSeed;
	Operation.CreatedUtc = Transaction.StrategicTime.Utc;
	Operation.AgentIds = Evaluation.AgentIds;
	SortStateCollections(Transaction);
	Transaction.CommandSequence = NextSequence;

	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::BaseDefenseTacticalOperationReady,
		NextSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Evaluation.BaseId;
	Event.ContactId = Evaluation.ContactId;
	Event.AssaultId = Evaluation.AssaultId;
	Event.OperationId = OperationId;
	Event.RuleId = Evaluation.MissionRuleId;
	Event.Quantity = Evaluation.AgentIds.Num();
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FGenerateTacticalBattleCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("A tactical battlefield cannot be generated after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalOperationState* Operation = FindTacticalOperation(State, Command.OperationId);
	if (Operation == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_operation"), TEXT("Pending tactical operation does not exist."));
		return Result;
	}
	if (State.TacticalBattles.ContainsByPredicate(
		[&Command](const FTacticalBattleState& Battle) { return Battle.OperationId == Command.OperationId; }))
	{
		AddError(Result, TEXT("tactical_battle_exists"), TEXT("The tactical operation already has a materialized battlefield."));
		return Result;
	}
	FTacticalGenerationResult Generated = FTacticalMissionGenerator::Generate(State, Rules, Command.OperationId);
	if (!Generated.bSucceeded)
	{
		for (const FTacticalGenerationDiagnostic& Diagnostic : Generated.Diagnostics)
		{
			AddError(Result, Diagnostic.Code, Diagnostic.Message);
		}
		return Result;
	}
	if (FindTacticalBattle(State, Generated.Battle.BattleId) != nullptr)
	{
		AddError(Result, TEXT("tactical_battle_identity_conflict"), TEXT("Generated tactical battlefield identity is already in use."));
		return Result;
	}
	const FPersonnelMentorshipView Mentorship = FPersonnelMentorship::Evaluate(State, Operation->AgentIds);
	const FPersonnelLegacyRelayView LegacyRelay =
		FPersonnelLegacyRelay::Evaluate(State, Rules, Operation->AgentIds);
	const FPersonnelSquadBondView SquadBonds = FPersonnelSquadBond::Evaluate(State, Operation->AgentIds);

	FCampaignState Transaction = State;
	const FGuid BattleId = Generated.Battle.BattleId;
	const FGuid SiteId = Generated.Battle.SiteId;
	const FName MissionRuleId = Generated.Battle.MissionRuleId;
	const int32 CellCount = Generated.Battle.Cells.Num();
	const ETacticalWindDirection WindDirection = Generated.Battle.WindDirection;
	const int32 WindStrength = Generated.Battle.WindStrength;
	Transaction.TacticalBattles.Add(MoveTemp(Generated.Battle));
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::TacticalBattleGenerated, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.OperationId = Command.OperationId;
	Event.BattleId = BattleId;
	Event.SiteId = SiteId;
	Event.RuleId = MissionRuleId;
	Event.Quantity = CellCount;
	Event.WindDirection = WindDirection;
	Event.WindStrength = WindStrength;
	if (Mentorship.bActive)
	{
		FStrategicEvent& MentorshipEvent = AddEvent(
			Result,
			EStrategicEventType::PersonnelMentorshipApplied,
			Transaction.CommandSequence,
			Transaction.StrategicTime.Utc);
		MentorshipEvent.PersonnelId = Mentorship.MentorId;
		MentorshipEvent.OperationId = Command.OperationId;
		MentorshipEvent.BattleId = BattleId;
		MentorshipEvent.SiteId = SiteId;
		MentorshipEvent.PolicyId = Mentorship.PolicyId;
		MentorshipEvent.Amount = Mentorship.MoraleBonus;
		MentorshipEvent.Quantity = Mentorship.RecipientCount;
	}
	if (LegacyRelay.bActive)
	{
		FStrategicEvent& RelayEvent = AddEvent(
			Result,
			EStrategicEventType::PersonnelLegacyRelayApplied,
			Transaction.CommandSequence,
			Transaction.StrategicTime.Utc);
		RelayEvent.PersonnelId = LegacyRelay.SpecialistId;
		RelayEvent.OperationId = Command.OperationId;
		RelayEvent.BattleId = BattleId;
		RelayEvent.SiteId = SiteId;
		RelayEvent.RuleId = LegacyRelay.DoctrineId;
		RelayEvent.PolicyId = LegacyRelay.PolicyId;
		RelayEvent.PersonnelAccuracyBonus = LegacyRelay.AccuracyBonus;
		RelayEvent.PersonnelResolveBonus = LegacyRelay.ResolveBonus;
		RelayEvent.PersonnelMobilityBonus = LegacyRelay.MobilityBonus;
		RelayEvent.PersonnelStrengthBonus = LegacyRelay.StrengthBonus;
		RelayEvent.Quantity = LegacyRelay.RecipientCount;
	}
	for (const FPersonnelSquadBondPairView& Pair : SquadBonds.ActivePairs)
	{
		FStrategicEvent& BondEvent = AddEvent(
			Result,
			EStrategicEventType::PersonnelSquadBondApplied,
			Transaction.CommandSequence,
			Transaction.StrategicTime.Utc);
		BondEvent.PersonnelId = Pair.FirstPersonnelId;
		BondEvent.RelatedPersonnelId = Pair.SecondPersonnelId;
		BondEvent.OperationId = Command.OperationId;
		BondEvent.BattleId = BattleId;
		BondEvent.SiteId = SiteId;
		BondEvent.PolicyId = SquadBonds.PolicyId;
		BondEvent.PersonnelSquadBondTier = static_cast<int32>(Pair.Tier);
		BondEvent.PersonnelSharedVictories = Pair.SharedVictories;
		BondEvent.PersonnelActionPointBonus = Pair.ActionPointBonus;
		BondEvent.PersonnelMoraleBonus = Pair.MoraleBonus;
		BondEvent.bSuccessful = true;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FConfirmTacticalDeploymentCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical deployment cannot begin after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical deployment references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase != ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_already_confirmed"), TEXT("Tactical deployment has already been confirmed."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	Battle->Phase = ETacticalBattlePhase::PlayerTurn;
	Battle->ActiveTeam = ETacticalTeam::Player;
	for (FTacticalUnitState& Unit : Battle->Units)
	{
		if (Unit.Team == ETacticalTeam::Player && Unit.CurrentHealth > 0 && !Unit.bExtracted)
		{
			Unit.RemainingActionPoints = Unit.MaxActionPoints;
		}
	}
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FName MissionRuleId = Battle->MissionRuleId;
	const int32 TurnNumber = Battle->TurnNumber;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::TacticalDeploymentConfirmed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BattleId = BattleId;
	Event.OperationId = OperationId;
	Event.SiteId = SiteId;
	Event.RuleId = MissionRuleId;
	Event.Quantity = TurnNumber;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FChangeTacticalStanceCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	static constexpr int32 StanceActionPointCost = 1;
	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical units cannot change stance after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical stance change references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical stance changes require confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot accept stance changes."));
		return Result;
	}
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical stance change references an unknown unit."));
		return Result;
	}
	if (ExistingUnit->Team != ExistingBattle->ActiveTeam || ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Only a living unit on the active tactical team can change stance."));
		return Result;
	}
	if (Command.Stance != ETacticalStance::Standing && Command.Stance != ETacticalStance::Crouched)
	{
		AddError(Result, TEXT("invalid_tactical_stance"), TEXT("Tactical stance is unknown."));
		return Result;
	}
	if (ExistingUnit->Stance == Command.Stance)
	{
		AddError(Result, TEXT("tactical_stance_unchanged"), TEXT("Unit is already using the requested tactical stance."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < StanceActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Unit lacks the action point required to change stance."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	Unit->Stance = Command.Stance;
	Unit->RemainingActionPoints -= StanceActionPointCost;
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const FName UnitRuleId = Unit->SourceRuleId;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::TacticalStanceChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BattleId = BattleId;
	Event.OperationId = OperationId;
	Event.SiteId = SiteId;
	Event.TacticalUnitId = UnitId;
	Event.RuleId = UnitRuleId;
	Event.bSuccessful = true;
	Event.Amount = -StanceActionPointCost;
	Event.Quantity = static_cast<int32>(Command.Stance);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FSetTacticalDoorCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical doors cannot be operated after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical door action references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical door actions require confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot accept door actions."));
		return Result;
	}
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical door action references an unknown unit."));
		return Result;
	}
	if (ExistingUnit->Team != ExistingBattle->ActiveTeam || ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Only a living unit on the active tactical team can operate a door."));
		return Result;
	}
	if (!ExistingBattle->IsWithinGrid(Command.TargetX, Command.TargetY, Command.TargetZ))
	{
		AddError(Result, TEXT("invalid_tactical_door"), TEXT("Tactical door coordinates are outside the battlefield."));
		return Result;
	}
	if (FMath::Abs(Command.TargetX - ExistingUnit->X)
		+ FMath::Abs(Command.TargetY - ExistingUnit->Y)
		+ FMath::Abs(Command.TargetZ - ExistingUnit->Z) != 1)
	{
		AddError(Result, TEXT("tactical_door_out_of_reach"), TEXT("A tactical unit must be orthogonally adjacent to operate a door."));
		return Result;
	}
	const FTacticalCellState& ExistingCell = ExistingBattle->Cells[ExistingBattle->GetCellIndex(Command.TargetX, Command.TargetY, Command.TargetZ)];
	const FTacticalTerrainRule* DoorRule = Rules.TacticalTerrains.Find(ExistingCell.TerrainRuleId);
	if (DoorRule == nullptr || !DoorRule->IsDoor() || ExistingCell.CurrentIntegrity <= 0)
	{
		AddError(Result, TEXT("invalid_tactical_door"), TEXT("Target cell has no intact openable terrain."));
		return Result;
	}
	if (ExistingCell.bDoorOpen == Command.bOpen)
	{
		AddError(Result, TEXT("tactical_door_state_unchanged"), TEXT("Door is already in the requested state."));
		return Result;
	}
	if (!Command.bOpen && ExistingBattle->Units.ContainsByPredicate(
		[&Command](const FTacticalUnitState& Unit)
		{
			return Unit.CurrentHealth > 0 && !Unit.bExtracted
				&& Unit.X == Command.TargetX && Unit.Y == Command.TargetY && Unit.Z == Command.TargetZ;
		}))
	{
		AddError(Result, TEXT("occupied_tactical_door"), TEXT("An occupied tactical doorway cannot be closed."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < DoorRule->DoorActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Unit lacks the action points required to operate this door."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	FTacticalCellState& Cell = Battle->Cells[Battle->GetCellIndex(Command.TargetX, Command.TargetY, Command.TargetZ)];
	Cell.bDoorOpen = Command.bOpen;
	Unit->RemainingActionPoints -= DoorRule->DoorActionPointCost;
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const FName TerrainRuleId = Cell.TerrainRuleId;
	const int32 ActionPointCost = DoorRule->DoorActionPointCost;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::TacticalDoorStateChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BattleId = BattleId;
	Event.OperationId = OperationId;
	Event.SiteId = SiteId;
	Event.TacticalUnitId = UnitId;
	Event.ToX = Command.TargetX;
	Event.ToY = Command.TargetY;
	Event.ToZ = Command.TargetZ;
	Event.RuleId = TerrainRuleId;
	Event.bSuccessful = true;
	Event.Amount = -ActionPointCost;
	Event.Quantity = Command.bOpen ? 1 : 0;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FMoveTacticalUnitCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical units cannot move after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical movement references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical movement requires confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot accept movement."));
		return Result;
	}
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical movement references an unknown unit."));
		return Result;
	}
	if (ExistingUnit->Team != ExistingBattle->ActiveTeam)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Only a unit on the active tactical team can move."));
		return Result;
	}
	const FTacticalPathResult Path = FTacticalNavigationService::FindPath(
		*ExistingBattle,
		Rules,
		Command.UnitId,
		Command.DestinationX,
		Command.DestinationY,
		Command.DestinationZ);
	if (!Path.bSucceeded)
	{
		for (const FTacticalNavigationDiagnostic& Diagnostic : Path.Diagnostics)
		{
			AddError(Result, Diagnostic.Code, Diagnostic.Message);
		}
		return Result;
	}
	if (Path.Steps.IsEmpty())
	{
		AddError(Result, TEXT("already_at_tactical_destination"), TEXT("Tactical unit is already at the requested destination."));
		return Result;
	}
	if (Path.TotalCost > ExistingUnit->RemainingActionPoints)
	{
		AddError(Result, TEXT("insufficient_action_points"), FString::Printf(
			TEXT("Tactical path costs %d action points, but unit '%s' has %d remaining."),
			Path.TotalCost,
			*ExistingUnit->DisplayName,
			ExistingUnit->RemainingActionPoints));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	const int32 FromX = Unit->X;
	const int32 FromY = Unit->Y;
	const int32 FromZ = Unit->Z;
	if (Unit->Team == ETacticalTeam::Player)
	{
		for (const FTacticalPathStep& Step : Path.Steps)
		{
			Unit->X = Step.X;
			Unit->Y = Step.Y;
			Unit->Z = Step.Z;
			const FTacticalVisibilityResult Discovery = FTacticalNavigationService::RefreshPlayerDiscovery(*Battle, Rules);
			if (!Discovery.bSucceeded)
			{
				for (const FTacticalNavigationDiagnostic& Diagnostic : Discovery.Diagnostics)
				{
					AddError(Result, Diagnostic.Code, Diagnostic.Message);
				}
				return Result;
			}
		}
	}
	Unit->X = Command.DestinationX;
	Unit->Y = Command.DestinationY;
	Unit->Z = Command.DestinationZ;
	Unit->RemainingActionPoints -= Path.TotalCost;
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const FName UnitRuleId = Unit->SourceRuleId;
	const int32 ToX = Unit->X;
	const int32 ToY = Unit->Y;
	const int32 ToZ = Unit->Z;
	const int32 RemainingActionPoints = Unit->RemainingActionPoints;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::TacticalUnitMoved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BattleId = BattleId;
	Event.OperationId = OperationId;
	Event.SiteId = SiteId;
	Event.TacticalUnitId = UnitId;
	Event.RuleId = UnitRuleId;
	Event.FromX = FromX;
	Event.FromY = FromY;
	Event.FromZ = FromZ;
	Event.ToX = ToX;
	Event.ToY = ToY;
	Event.ToZ = ToZ;
	Event.Amount = RemainingActionPoints;
	Event.Quantity = Path.TotalCost;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FAttackTacticalUnitCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical units cannot attack after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical attack references an unknown battlefield."));
		return Result;
	}
	const FTacticalAttackPreview Preview = FTacticalCombatService::PreviewUnitAttack(
		*ExistingBattle,
		State,
		Rules,
		Command.AttackerUnitId,
		Command.TargetUnitId,
		Command.WeaponItemId,
		Command.FireMode);
	if (!Preview.bSucceeded)
	{
		AddCombatDiagnostics(Result, Preview);
		return Result;
	}
	const int64 MaximumRandomDraws = static_cast<int64>(Preview.ProjectileCount) * 2;
	if (WouldExhaustDeterministicRandomStream(ExistingBattle->TacticalRandom, MaximumRandomDraws))
	{
		AddError(Result, TEXT("tactical_random_exhausted"), TEXT("Tactical random stream cannot accept another attack."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Attacker = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.AttackerUnitId; });
	FTacticalUnitState* Target = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.TargetUnitId; });
	check(Attacker != nullptr && Target != nullptr);
	Attacker->RemainingActionPoints -= Preview.ActionPointCost;
	if (Preview.AmmunitionCost > 0)
	{
		FTacticalWeaponState* WeaponState = Attacker->WeaponStates.FindByPredicate(
			[&Preview](const FTacticalWeaponState& State) { return State.WeaponItemId == Preview.AttackRuleId; });
		check(WeaponState != nullptr && WeaponState->LoadedAmmunition >= Preview.AmmunitionCost);
		WeaponState->LoadedAmmunition -= Preview.AmmunitionCost;
	}
	struct FProjectileOutcome
	{
		int32 HitRoll = 0;
		int32 AppliedDamage = 0;
		int32 RemainingHealth = 0;
		bool bHit = false;
	};
	TArray<FProjectileOutcome> ProjectileOutcomes;
	ProjectileOutcomes.Reserve(Preview.ProjectileCount);
	int32 DefenderArmor = Target->KineticArmor;
	if (Preview.DamageType == ETacticalDamageType::Thermal)
	{
		DefenderArmor = Target->ThermalArmor;
	}
	else if (Preview.DamageType == ETacticalDamageType::Arc)
	{
		DefenderArmor = Target->ArcArmor;
	}
	bool bAnyHit = false;
	bool bIncapacitated = false;
	for (int32 ProjectileIndex = 0; ProjectileIndex < Preview.ProjectileCount; ++ProjectileIndex)
	{
		FProjectileOutcome& Projectile = ProjectileOutcomes.AddDefaulted_GetRef();
		Projectile.HitRoll = static_cast<int32>(Battle->TacticalRandom.NextUInt64() % 100ULL) + 1;
		Projectile.bHit = Projectile.HitRoll <= Preview.HitChance;
		bAnyHit |= Projectile.bHit;
		if (Projectile.bHit)
		{
			const int32 DamageVariance = static_cast<int32>(Battle->TacticalRandom.NextUInt64() % 41ULL) + 80;
			const int32 Damage = FTacticalCombatService::ComputeUnitDamage(
				Preview.AttackPower,
				Attacker->Strength,
				Target->Strength,
				DefenderArmor,
				DamageVariance);
			Projectile.AppliedDamage = FMath::Min(Target->CurrentHealth, Damage);
			Target->CurrentHealth -= Projectile.AppliedDamage;
		}
		Projectile.RemainingHealth = Target->CurrentHealth;
	}
	bIncapacitated = Target->CurrentHealth == 0;
	if (bIncapacitated && Attacker->Team == ETacticalTeam::Player
		&& Target->Team == ETacticalTeam::Adversary)
	{
		FPersonnelState* AttackingPerson = FindPersonnel(Transaction, Attacker->PersonnelId);
		check(AttackingPerson != nullptr);
		if (AttackingPerson->Kills == MAX_int32)
		{
			AddError(Result, TEXT("tactical_kill_record_overflow"), TEXT("Attacking personnel cannot record another tactical incapacitation."));
			return Result;
		}
		++AttackingPerson->Kills;
	}
	const int32 PressurePower = static_cast<int32>(FMath::Min<int64>(
		MAX_int32,
		static_cast<int64>(Preview.AttackPower) * Preview.ProjectileCount));
	const FTacticalPressureChange PressureChange = ApplyAttackPressure(*Target, PressurePower, bAnyHit, bIncapacitated);
	const FTacticalResolutionEvaluation Evaluation = EvaluateTacticalBattleResolution(*Battle);
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FName AttackRuleId = Preview.AttackRuleId;
	const FGuid AttackerId = Attacker->UnitId;
	const FGuid TargetId = Target->UnitId;
	const FName TargetRuleId = Target->SourceRuleId;
	const int32 TargetX = Target->X;
	const int32 TargetY = Target->Y;
	const int32 TargetZ = Target->Z;
	const int32 RemainingTargetSuppression = Target->Suppression;
	const int32 RemainingTargetMorale = Target->CurrentMorale;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	for (int32 ProjectileIndex = 0; ProjectileIndex < ProjectileOutcomes.Num(); ++ProjectileIndex)
	{
		const FProjectileOutcome& Projectile = ProjectileOutcomes[ProjectileIndex];
		FStrategicEvent& Attack = AddEvent(Result, EStrategicEventType::TacticalAttackResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Attack.BattleId = BattleId;
		Attack.OperationId = OperationId;
		Attack.SiteId = SiteId;
		Attack.TacticalUnitId = AttackerId;
		Attack.TargetTacticalUnitId = TargetId;
		Attack.ToX = TargetX;
		Attack.ToY = TargetY;
		Attack.ToZ = TargetZ;
		Attack.RuleId = AttackRuleId;
		Attack.HitChance = Preview.HitChance;
		Attack.Roll = Projectile.HitRoll;
		Attack.bSuccessful = Projectile.bHit;
		Attack.Amount = ProjectileIndex == 0 ? -Preview.ActionPointCost : 0;
		Attack.Quantity = Projectile.AppliedDamage;
		if (Projectile.AppliedDamage > 0)
		{
			FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::TacticalUnitDamaged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Damaged.BattleId = BattleId;
			Damaged.OperationId = OperationId;
			Damaged.SiteId = SiteId;
			Damaged.TacticalUnitId = AttackerId;
			Damaged.TargetTacticalUnitId = TargetId;
			Damaged.RuleId = AttackRuleId;
			Damaged.Amount = -Projectile.AppliedDamage;
			Damaged.Quantity = Projectile.RemainingHealth;
		}
	}
	if (bIncapacitated)
	{
		FStrategicEvent& Incapacitated = AddEvent(Result, EStrategicEventType::TacticalUnitIncapacitated, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Incapacitated.BattleId = BattleId;
		Incapacitated.OperationId = OperationId;
		Incapacitated.SiteId = SiteId;
		Incapacitated.TacticalUnitId = AttackerId;
		Incapacitated.TargetTacticalUnitId = TargetId;
		Incapacitated.RuleId = TargetRuleId;
	}
	if (PressureChange.SuppressionDelta != 0)
	{
		FStrategicEvent& Suppressed = AddEvent(Result, EStrategicEventType::TacticalUnitSuppressed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Suppressed.BattleId = BattleId;
		Suppressed.OperationId = OperationId;
		Suppressed.SiteId = SiteId;
		Suppressed.TacticalUnitId = AttackerId;
		Suppressed.TargetTacticalUnitId = TargetId;
		Suppressed.RuleId = AttackRuleId;
		Suppressed.Amount = PressureChange.SuppressionDelta;
		Suppressed.Quantity = RemainingTargetSuppression;
	}
	if (PressureChange.MoraleDelta != 0)
	{
		FStrategicEvent& Morale = AddEvent(Result, EStrategicEventType::TacticalMoraleChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Morale.BattleId = BattleId;
		Morale.OperationId = OperationId;
		Morale.SiteId = SiteId;
		Morale.TacticalUnitId = TargetId;
		Morale.RuleId = TargetRuleId;
		Morale.Amount = PressureChange.MoraleDelta;
		Morale.Quantity = RemainingTargetMorale;
	}
	if (Evaluation.bResolved)
	{
		FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Resolved.BattleId = BattleId;
		Resolved.OperationId = OperationId;
		Resolved.SiteId = SiteId;
		Resolved.bSuccessful = Evaluation.bObjectiveCompleted;
		Resolved.Quantity = Evaluation.bObjectiveCompleted ? 1 : 0;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bIncapacitated || Evaluation.bResolved;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FProjectTacticalSignalCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical signals cannot be projected after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Signal projection references an unknown battlefield."));
		return Result;
	}
	const FTacticalSignalPreview Preview = FTacticalCombatService::PreviewSignalProjection(
		*ExistingBattle,
		State,
		Rules,
		Command.AttackerUnitId,
		Command.TargetUnitId,
		Command.ProjectorItemId);
	if (!Preview.bSucceeded)
	{
		AddCombatDiagnostics(Result, Preview);
		return Result;
	}
	if (WouldExhaustDeterministicRandomStream(ExistingBattle->TacticalRandom, 1))
	{
		AddError(Result, TEXT("tactical_random_exhausted"), TEXT("Tactical random stream cannot accept another signal projection."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Attacker = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.AttackerUnitId; });
	FTacticalUnitState* Target = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.TargetUnitId; });
	check(Attacker != nullptr && Target != nullptr);
	Attacker->RemainingActionPoints -= Preview.ActionPointCost;
	const int32 Roll = static_cast<int32>(Battle->TacticalRandom.NextUInt64() % 100ULL) + 1;
	const bool bSuccessful = Roll <= Preview.HitChance;
	int32 AppliedSuppression = 0;
	int32 AppliedMoraleDamage = 0;
	if (bSuccessful)
	{
		AppliedSuppression = Preview.SuppressionGain;
		AppliedMoraleDamage = Preview.MoraleDamage;
		Target->Suppression += AppliedSuppression;
		Target->CurrentMorale -= AppliedMoraleDamage;
	}
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid AttackerId = Attacker->UnitId;
	const FGuid TargetId = Target->UnitId;
	const FName TargetRuleId = Target->SourceRuleId;
	const int32 TargetX = Target->X;
	const int32 TargetY = Target->Y;
	const int32 TargetZ = Target->Z;
	const int32 RemainingSuppression = Target->Suppression;
	const int32 RemainingMorale = Target->CurrentMorale;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Projected = AddEvent(Result, EStrategicEventType::TacticalSignalProjected,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Projected.BattleId = BattleId;
	Projected.OperationId = OperationId;
	Projected.SiteId = SiteId;
	Projected.TacticalUnitId = AttackerId;
	Projected.TargetTacticalUnitId = TargetId;
	Projected.ToX = TargetX;
	Projected.ToY = TargetY;
	Projected.ToZ = TargetZ;
	Projected.RuleId = Preview.SignalRuleId;
	Projected.HitChance = Preview.HitChance;
	Projected.Roll = Roll;
	Projected.bSuccessful = bSuccessful;
	Projected.Amount = -Preview.ActionPointCost;
	Projected.Quantity = AppliedMoraleDamage;
	if (AppliedSuppression > 0)
	{
		FStrategicEvent& Suppressed = AddEvent(Result, EStrategicEventType::TacticalUnitSuppressed,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Suppressed.BattleId = BattleId;
		Suppressed.OperationId = OperationId;
		Suppressed.SiteId = SiteId;
		Suppressed.TacticalUnitId = AttackerId;
		Suppressed.TargetTacticalUnitId = TargetId;
		Suppressed.RuleId = Preview.SignalRuleId;
		Suppressed.Amount = AppliedSuppression;
		Suppressed.Quantity = RemainingSuppression;
	}
	if (AppliedMoraleDamage > 0)
	{
		FStrategicEvent& Morale = AddEvent(Result, EStrategicEventType::TacticalMoraleChanged,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Morale.BattleId = BattleId;
		Morale.OperationId = OperationId;
		Morale.SiteId = SiteId;
		Morale.TacticalUnitId = TargetId;
		Morale.RuleId = TargetRuleId;
		Morale.Amount = -AppliedMoraleDamage;
		Morale.Quantity = RemainingMorale;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bSuccessful && RemainingMorale == 0;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FAttackTacticalTerrainCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical terrain cannot be attacked after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical terrain attack references an unknown battlefield."));
		return Result;
	}
	const FTacticalAttackPreview Preview = FTacticalCombatService::PreviewTerrainAttack(
		*ExistingBattle,
		State,
		Rules,
		Command.AttackerUnitId,
		Command.TargetX,
		Command.TargetY,
		Command.WeaponItemId,
		Command.FireMode,
		Command.TargetZ);
	if (!Preview.bSucceeded)
	{
		AddCombatDiagnostics(Result, Preview);
		return Result;
	}
	const int64 RequiredRandomDraws = 1
		+ (Preview.BlastRadius > 0 && Preview.ScatterRadius > 0 ? 1 : 0);
	if (WouldExhaustDeterministicRandomStream(ExistingBattle->TacticalRandom, RequiredRandomDraws))
	{
		AddError(Result, TEXT("tactical_random_exhausted"), TEXT("Tactical random stream cannot accept another terrain attack."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Attacker = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.AttackerUnitId; });
	check(Attacker != nullptr);
	FTacticalCellState& Cell = Battle->Cells[Battle->GetCellIndex(Command.TargetX, Command.TargetY, Command.TargetZ)];
	Attacker->RemainingActionPoints -= Preview.ActionPointCost;
	if (Preview.AmmunitionCost > 0)
	{
		FTacticalWeaponState* WeaponState = Attacker->WeaponStates.FindByPredicate(
			[&Preview](const FTacticalWeaponState& State) { return State.WeaponItemId == Preview.AttackRuleId; });
		check(WeaponState != nullptr && WeaponState->LoadedAmmunition >= Preview.AmmunitionCost);
		WeaponState->LoadedAmmunition -= Preview.AmmunitionCost;
	}
	if (Preview.BlastRadius > 0)
	{
		struct FBlastCellOutcome
		{
			FName TerrainRuleId;
			int32 X = 0;
			int32 Y = 0;
			int32 Z = 0;
			int32 Damage = 0;
			int32 RemainingIntegrity = 0;
			bool bDestroyed = false;
		};
		struct FBlastUnitOutcome
		{
			FGuid UnitId;
			FName UnitRuleId;
			int32 Damage = 0;
			int32 RemainingHealth = 0;
			int32 SuppressionDelta = 0;
			int32 Suppression = 0;
			int32 MoraleDelta = 0;
			int32 Morale = 0;
			bool bIncapacitated = false;
		};

		int32 ImpactX = Command.TargetX;
		int32 ImpactY = Command.TargetY;
		const int32 ImpactZ = Command.TargetZ;
		if (Preview.ScatterRadius > 0)
		{
			const int32 ScatterDiameter = Preview.ScatterRadius * 2 + 1;
			const int32 ScatterCellCount = ScatterDiameter * ScatterDiameter;
			const int32 ScatterIndex = static_cast<int32>(Battle->TacticalRandom.NextUInt64()
				% static_cast<uint64>(ScatterCellCount));
			ImpactX = FMath::Clamp(
				Command.TargetX + ScatterIndex % ScatterDiameter - Preview.ScatterRadius,
				0,
				Battle->Width - 1);
			ImpactY = FMath::Clamp(
				Command.TargetY + ScatterIndex / ScatterDiameter - Preview.ScatterRadius,
				0,
				Battle->Height - 1);
		}
		const FTacticalBattleState PreBlastBattle = *Battle;
		const int32 DamageVariance = static_cast<int32>(Battle->TacticalRandom.NextUInt64() % 41ULL) + 80;
		const int32 BaseTerrainDamage = ScaleTacticalValue(
			FTacticalCombatService::ComputeTerrainDamage(Preview.AttackPower, Attacker->Strength, DamageVariance),
			Preview.TerrainDamagePercent);
		TArray<FBlastCellOutcome> CellOutcomes;
		TArray<FBlastUnitOutcome> UnitOutcomes;
		int32 AffectedCellCount = 0;
		int64 TotalAppliedDamage = 0;
		bool bAnyTerrainDestroyed = false;
		bool bAnyUnitIncapacitated = false;

		for (FTacticalCellState& BlastCell : Battle->Cells)
		{
			const int32 Distance = CeilTacticalDistance(
				BlastCell.X - ImpactX,
				BlastCell.Y - ImpactY,
				BlastCell.Z - ImpactZ);
			if (Distance > Preview.BlastRadius)
			{
				continue;
			}
			const int32 FalloffPercent = FTacticalCombatService::ComputeBlastEffectPercent(
				Distance,
				Preview.BlastFalloffPercent);
			const int32 TransmissionPercent = FTacticalCombatService::ComputeBlastTransmissionPercent(
				PreBlastBattle,
				Rules,
				ImpactX,
				ImpactY,
				BlastCell.X,
				BlastCell.Y,
				ImpactZ,
				BlastCell.Z);
			const int32 EffectPercent = ScaleTacticalValue(FalloffPercent, TransmissionPercent);
			if (EffectPercent <= 0)
			{
				continue;
			}
			++AffectedCellCount;
			BlastCell.Smoke = FMath::Min(100, BlastCell.Smoke + ScaleTacticalValue(Preview.BlastSmoke, EffectPercent));
			BlastCell.Fire = FMath::Min(100, BlastCell.Fire + ScaleTacticalValue(Preview.BlastFire, EffectPercent));
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(BlastCell.TerrainRuleId);
			if (Terrain == nullptr || Terrain->MaxIntegrity <= 0 || BlastCell.CurrentIntegrity <= 0)
			{
				continue;
			}
			const int32 Damage = ScaleTacticalValue(BaseTerrainDamage, EffectPercent);
			if (Damage <= 0)
			{
				continue;
			}
			FBlastCellOutcome& Outcome = CellOutcomes.AddDefaulted_GetRef();
			Outcome.TerrainRuleId = BlastCell.TerrainRuleId;
			Outcome.X = BlastCell.X;
			Outcome.Y = BlastCell.Y;
			Outcome.Z = BlastCell.Z;
			Outcome.Damage = FMath::Min(BlastCell.CurrentIntegrity, Damage);
			BlastCell.CurrentIntegrity -= Outcome.Damage;
			if (BlastCell.CurrentIntegrity == 0)
			{
				BlastCell.bDoorOpen = false;
			}
			Outcome.RemainingIntegrity = BlastCell.CurrentIntegrity;
			Outcome.bDestroyed = BlastCell.CurrentIntegrity == 0;
			bAnyTerrainDestroyed |= Outcome.bDestroyed;
			TotalAppliedDamage += Outcome.Damage;
		}

		for (FTacticalUnitState& Unit : Battle->Units)
		{
			if (Unit.CurrentHealth <= 0 || Unit.bExtracted)
			{
				continue;
			}
			const int32 Distance = CeilTacticalDistance(Unit.X - ImpactX, Unit.Y - ImpactY, Unit.Z - ImpactZ);
			if (Distance > Preview.BlastRadius)
			{
				continue;
			}
			const int32 FalloffPercent = FTacticalCombatService::ComputeBlastEffectPercent(
				Distance,
				Preview.BlastFalloffPercent);
			const int32 TransmissionPercent = FTacticalCombatService::ComputeBlastTransmissionPercent(
				PreBlastBattle,
				Rules,
				ImpactX,
				ImpactY,
				Unit.X,
				Unit.Y,
				ImpactZ,
				Unit.Z);
			const int32 EffectPercent = ScaleTacticalValue(FalloffPercent, TransmissionPercent);
			if (EffectPercent <= 0)
			{
				continue;
			}
			const int32 PreviousHealth = Unit.CurrentHealth;
			const int32 PreviousSuppression = Unit.Suppression;
			const int32 PreviousMorale = Unit.CurrentMorale;
			const int32 BaseUnitDamage = FTacticalCombatService::ComputeUnitDamage(
				Preview.AttackPower,
				Attacker->Strength,
				Unit.Strength,
				GetTacticalArmor(Unit, Preview.DamageType),
				DamageVariance);
			const int32 Damage = ScaleTacticalValue(BaseUnitDamage, EffectPercent);
			const int32 AppliedDamage = FMath::Min(Unit.CurrentHealth, Damage);
			Unit.CurrentHealth -= AppliedDamage;
			if (Unit.CurrentHealth == 0)
			{
				Unit.Suppression = 100;
				Unit.CurrentMorale = 0;
				Unit.RemainingActionPoints = 0;
			}
			else
			{
				const int32 BlastPressure = ScaleTacticalValue(Preview.BlastSuppression, EffectPercent);
				const int32 DamagePressure = AppliedDamage > 0 ? FMath::Max(1, AppliedDamage / 4) : 0;
				Unit.Suppression = FMath::Min(100, Unit.Suppression + BlastPressure + DamagePressure);
				const int32 SuppressionDelta = Unit.Suppression - PreviousSuppression;
				if (SuppressionDelta > 0 || AppliedDamage > 0)
				{
					const int32 MoraleLoss = FMath::Max(
						1,
						SuppressionDelta / 2 + AppliedDamage / 5 - Unit.Resolve / 20);
					Unit.CurrentMorale = FMath::Max(0, Unit.CurrentMorale - MoraleLoss);
				}
			}

			FBlastUnitOutcome& Outcome = UnitOutcomes.AddDefaulted_GetRef();
			Outcome.UnitId = Unit.UnitId;
			Outcome.UnitRuleId = Unit.SourceRuleId;
			Outcome.Damage = PreviousHealth - Unit.CurrentHealth;
			Outcome.RemainingHealth = Unit.CurrentHealth;
			Outcome.SuppressionDelta = Unit.Suppression - PreviousSuppression;
			Outcome.Suppression = Unit.Suppression;
			Outcome.MoraleDelta = Unit.CurrentMorale - PreviousMorale;
			Outcome.Morale = Unit.CurrentMorale;
			Outcome.bIncapacitated = PreviousHealth > 0 && Unit.CurrentHealth == 0;
			bAnyUnitIncapacitated |= Outcome.bIncapacitated;
			TotalAppliedDamage += Outcome.Damage;
			if (Outcome.bIncapacitated && Attacker->Team == ETacticalTeam::Player
				&& Unit.Team == ETacticalTeam::Adversary)
			{
				FPersonnelState* AttackingPerson = FindPersonnel(Transaction, Attacker->PersonnelId);
				check(AttackingPerson != nullptr);
				if (AttackingPerson->Kills == MAX_int32)
				{
					AddError(Result, TEXT("tactical_kill_record_overflow"), TEXT("Attacking personnel cannot record another blast incapacitation."));
					return Result;
				}
				++AttackingPerson->Kills;
			}
		}

		const FTacticalResolutionEvaluation Evaluation = EvaluateTacticalBattleResolution(*Battle);
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
		{
			return Result;
		}
		const FGuid BattleId = Battle->BattleId;
		const FGuid OperationId = Battle->OperationId;
		const FGuid SiteId = Battle->SiteId;
		const FGuid AttackerId = Attacker->UnitId;
		const FName AttackRuleId = Preview.AttackRuleId;
		SortStateCollections(Transaction);
		++Transaction.CommandSequence;
		FStrategicEvent& Attack = AddEvent(Result, EStrategicEventType::TacticalAttackResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Attack.BattleId = BattleId;
		Attack.OperationId = OperationId;
		Attack.SiteId = SiteId;
		Attack.TacticalUnitId = AttackerId;
		Attack.FromX = Command.TargetX;
		Attack.FromY = Command.TargetY;
		Attack.FromZ = Command.TargetZ;
		Attack.ToX = ImpactX;
		Attack.ToY = ImpactY;
		Attack.ToZ = ImpactZ;
		Attack.RuleId = AttackRuleId;
		Attack.HitChance = 100;
		Attack.Roll = DamageVariance;
		Attack.bSuccessful = true;
		Attack.Amount = -Preview.ActionPointCost;
		Attack.Quantity = static_cast<int32>(FMath::Min<int64>(TotalAppliedDamage, MAX_int32));
		FStrategicEvent& Blast = AddEvent(Result, EStrategicEventType::TacticalBlastResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Blast.BattleId = BattleId;
		Blast.OperationId = OperationId;
		Blast.SiteId = SiteId;
		Blast.TacticalUnitId = AttackerId;
		Blast.FromX = Command.TargetX;
		Blast.FromY = Command.TargetY;
		Blast.FromZ = Command.TargetZ;
		Blast.ToX = ImpactX;
		Blast.ToY = ImpactY;
		Blast.ToZ = ImpactZ;
		Blast.RuleId = AttackRuleId;
		Blast.Roll = DamageVariance;
		Blast.Amount = Preview.BlastRadius;
		Blast.Quantity = AffectedCellCount + UnitOutcomes.Num();
		for (const FBlastCellOutcome& Outcome : CellOutcomes)
		{
			FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::TacticalTerrainDamaged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Damaged.BattleId = BattleId;
			Damaged.OperationId = OperationId;
			Damaged.SiteId = SiteId;
			Damaged.TacticalUnitId = AttackerId;
			Damaged.ToX = Outcome.X;
			Damaged.ToY = Outcome.Y;
			Damaged.ToZ = Outcome.Z;
			Damaged.RuleId = Outcome.TerrainRuleId;
			Damaged.Amount = -Outcome.Damage;
			Damaged.Quantity = Outcome.RemainingIntegrity;
			if (Outcome.bDestroyed)
			{
				FStrategicEvent& Destroyed = AddEvent(Result, EStrategicEventType::TacticalTerrainDestroyed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Destroyed.BattleId = BattleId;
				Destroyed.OperationId = OperationId;
				Destroyed.SiteId = SiteId;
				Destroyed.TacticalUnitId = AttackerId;
				Destroyed.ToX = Outcome.X;
				Destroyed.ToY = Outcome.Y;
				Destroyed.ToZ = Outcome.Z;
				Destroyed.RuleId = Outcome.TerrainRuleId;
			}
		}
		for (const FBlastUnitOutcome& Outcome : UnitOutcomes)
		{
			if (Outcome.Damage > 0)
			{
				FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::TacticalUnitDamaged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Damaged.BattleId = BattleId;
				Damaged.OperationId = OperationId;
				Damaged.SiteId = SiteId;
				Damaged.TacticalUnitId = AttackerId;
				Damaged.TargetTacticalUnitId = Outcome.UnitId;
				Damaged.RuleId = AttackRuleId;
				Damaged.Amount = -Outcome.Damage;
				Damaged.Quantity = Outcome.RemainingHealth;
			}
			if (Outcome.bIncapacitated)
			{
				FStrategicEvent& Incapacitated = AddEvent(Result, EStrategicEventType::TacticalUnitIncapacitated, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Incapacitated.BattleId = BattleId;
				Incapacitated.OperationId = OperationId;
				Incapacitated.SiteId = SiteId;
				Incapacitated.TacticalUnitId = AttackerId;
				Incapacitated.TargetTacticalUnitId = Outcome.UnitId;
				Incapacitated.RuleId = Outcome.UnitRuleId;
			}
			if (Outcome.SuppressionDelta != 0)
			{
				FStrategicEvent& Suppressed = AddEvent(Result, EStrategicEventType::TacticalUnitSuppressed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Suppressed.BattleId = BattleId;
				Suppressed.OperationId = OperationId;
				Suppressed.SiteId = SiteId;
				Suppressed.TacticalUnitId = AttackerId;
				Suppressed.TargetTacticalUnitId = Outcome.UnitId;
				Suppressed.RuleId = AttackRuleId;
				Suppressed.Amount = Outcome.SuppressionDelta;
				Suppressed.Quantity = Outcome.Suppression;
			}
			if (Outcome.MoraleDelta != 0)
			{
				FStrategicEvent& Morale = AddEvent(Result, EStrategicEventType::TacticalMoraleChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Morale.BattleId = BattleId;
				Morale.OperationId = OperationId;
				Morale.SiteId = SiteId;
				Morale.TacticalUnitId = Outcome.UnitId;
				Morale.RuleId = Outcome.UnitRuleId;
				Morale.Amount = Outcome.MoraleDelta;
				Morale.Quantity = Outcome.Morale;
			}
		}
		if (Evaluation.bResolved)
		{
			FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Resolved.BattleId = BattleId;
			Resolved.OperationId = OperationId;
			Resolved.SiteId = SiteId;
			Resolved.bSuccessful = Evaluation.bObjectiveCompleted;
			Resolved.Quantity = Evaluation.bObjectiveCompleted ? 1 : 0;
		}
		State = MoveTemp(Transaction);
		Result.bAccepted = true;
		Result.bDecisionPause = bAnyTerrainDestroyed || bAnyUnitIncapacitated || Evaluation.bResolved;
		return Result;
	}
	const int32 DamageVariance = static_cast<int32>(Battle->TacticalRandom.NextUInt64() % 41ULL) + 80;
	const int32 Damage = FTacticalCombatService::ComputeTerrainDamage(
		Preview.AttackPower,
		Attacker->Strength,
		DamageVariance);
	const int32 AppliedDamage = FMath::Min(Cell.CurrentIntegrity, Damage);
	Cell.CurrentIntegrity -= AppliedDamage;
	if (Cell.CurrentIntegrity == 0)
	{
		Cell.bDoorOpen = false;
	}
	const bool bDestroyed = Cell.CurrentIntegrity == 0;
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid AttackerId = Attacker->UnitId;
	const FName AttackRuleId = Preview.AttackRuleId;
	const FName TerrainRuleId = Cell.TerrainRuleId;
	const int32 RemainingIntegrity = Cell.CurrentIntegrity;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Attack = AddEvent(Result, EStrategicEventType::TacticalAttackResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Attack.BattleId = BattleId;
	Attack.OperationId = OperationId;
	Attack.SiteId = SiteId;
	Attack.TacticalUnitId = AttackerId;
	Attack.ToX = Command.TargetX;
	Attack.ToY = Command.TargetY;
	Attack.ToZ = Command.TargetZ;
	Attack.RuleId = AttackRuleId;
	Attack.HitChance = 100;
	Attack.Roll = DamageVariance;
	Attack.bSuccessful = true;
	Attack.Amount = -Preview.ActionPointCost;
	Attack.Quantity = AppliedDamage;
	FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::TacticalTerrainDamaged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Damaged.BattleId = BattleId;
	Damaged.OperationId = OperationId;
	Damaged.SiteId = SiteId;
	Damaged.TacticalUnitId = AttackerId;
	Damaged.ToX = Command.TargetX;
	Damaged.ToY = Command.TargetY;
	Damaged.ToZ = Command.TargetZ;
	Damaged.RuleId = TerrainRuleId;
	Damaged.Amount = -AppliedDamage;
	Damaged.Quantity = RemainingIntegrity;
	if (bDestroyed)
	{
		FStrategicEvent& Destroyed = AddEvent(Result, EStrategicEventType::TacticalTerrainDestroyed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Destroyed.BattleId = BattleId;
		Destroyed.OperationId = OperationId;
		Destroyed.SiteId = SiteId;
		Destroyed.TacticalUnitId = AttackerId;
		Destroyed.ToX = Command.TargetX;
		Destroyed.ToY = Command.TargetY;
		Destroyed.ToZ = Command.TargetZ;
		Destroyed.RuleId = TerrainRuleId;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bDestroyed;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FReloadTacticalWeaponCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical weapons cannot reload after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical reload references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical reload requires confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot reload weapons."));
		return Result;
	}
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical reload references an unknown unit."));
		return Result;
	}
	if (ExistingUnit->Team != ETacticalTeam::Player || ExistingUnit->Team != ExistingBattle->ActiveTeam
		|| ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Only a living player unit on the active team can reload."));
		return Result;
	}
	const FItemRule* Weapon = Rules.Items.Find(Command.WeaponItemId);
	const FTacticalWeaponState* ExistingWeaponState = ExistingUnit->WeaponStates.FindByPredicate(
		[&Command](const FTacticalWeaponState& State) { return State.WeaponItemId == Command.WeaponItemId; });
	if (Weapon == nullptr || !Weapon->IsTacticalWeapon() || Weapon->TacticalAmmunitionItemId.IsNone()
		|| Weapon->TacticalMagazineCapacity <= 0 || Weapon->TacticalReloadActionPointCost <= 0
		|| ExistingWeaponState == nullptr)
	{
		AddError(Result, TEXT("invalid_tactical_reload"), TEXT("Tactical reload requires a carried magazine-fed weapon with a valid reload profile."));
		return Result;
	}
	if (ExistingWeaponState->LoadedAmmunition >= Weapon->TacticalMagazineCapacity)
	{
		AddError(Result, TEXT("tactical_weapon_full"), TEXT("Tactical weapon magazine is already full."));
		return Result;
	}
	const FInventoryStack* ExistingMagazine = ExistingUnit->CarriedItems.FindByPredicate(
		[Weapon](const FInventoryStack& Stack)
		{
			return Stack.ItemId == Weapon->TacticalAmmunitionItemId && Stack.Quantity > 0;
		});
	const int32 ExistingEjectedIndex = FindBestEjectedMagazineIndex(
		*ExistingUnit, Command.WeaponItemId, Weapon->TacticalAmmunitionItemId);
	const int32 BestReserveAmmunition = ExistingMagazine != nullptr
		? Weapon->TacticalMagazineCapacity
		: (ExistingUnit->EjectedMagazines.IsValidIndex(ExistingEjectedIndex)
			? ExistingUnit->EjectedMagazines[ExistingEjectedIndex].LoadedAmmunition
			: 0);
	if (BestReserveAmmunition <= 0)
	{
		AddError(Result, TEXT("tactical_ammunition_unavailable"), TEXT("Unit carries no compatible reserve magazine."));
		return Result;
	}
	if (BestReserveAmmunition <= ExistingWeaponState->LoadedAmmunition)
	{
		AddError(Result, TEXT("tactical_reload_no_improvement"), TEXT("No reserve magazine contains more rounds than the loaded magazine."));
		return Result;
	}
	if (ExistingMagazine != nullptr && ExistingWeaponState->LoadedAmmunition > 0
		&& ExistingUnit->EjectedMagazines.Num() >= 16)
	{
		AddError(Result, TEXT("tactical_magazine_inventory_full"), TEXT("This unit cannot retain the currently loaded magazine."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < Weapon->TacticalReloadActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Unit lacks the action points required to reload."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	FTacticalWeaponState* WeaponState = Unit->WeaponStates.FindByPredicate(
		[&Command](const FTacticalWeaponState& State) { return State.WeaponItemId == Command.WeaponItemId; });
	FInventoryStack* Magazine = Unit->CarriedItems.FindByPredicate(
		[Weapon](const FInventoryStack& Stack) { return Stack.ItemId == Weapon->TacticalAmmunitionItemId; });
	check(WeaponState != nullptr);
	const int32 PreviousAmmunition = WeaponState->LoadedAmmunition;
	int32 LoadedAmmunition = 0;
	if (Magazine != nullptr && Magazine->Quantity > 0)
	{
		LoadedAmmunition = Weapon->TacticalMagazineCapacity;
		--Magazine->Quantity;
	}
	else
	{
		const int32 EjectedIndex = FindBestEjectedMagazineIndex(
			*Unit, Command.WeaponItemId, Weapon->TacticalAmmunitionItemId);
		check(Unit->EjectedMagazines.IsValidIndex(EjectedIndex));
		LoadedAmmunition = Unit->EjectedMagazines[EjectedIndex].LoadedAmmunition;
		Unit->EjectedMagazines.RemoveAt(EjectedIndex);
	}
	Unit->RemainingActionPoints -= Weapon->TacticalReloadActionPointCost;
	WeaponState->LoadedAmmunition = LoadedAmmunition;
	if (PreviousAmmunition > 0)
	{
		FTacticalMagazineState& Ejected = Unit->EjectedMagazines.AddDefaulted_GetRef();
		Ejected.WeaponItemId = Command.WeaponItemId;
		Ejected.AmmunitionItemId = Weapon->TacticalAmmunitionItemId;
		Ejected.LoadedAmmunition = PreviousAmmunition;
	}
	Unit->CarriedItems.RemoveAll([](const FInventoryStack& Stack) { return Stack.Quantity == 0; });
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const int32 EventLoadedAmmunition = WeaponState->LoadedAmmunition;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Reloaded = AddEvent(Result, EStrategicEventType::TacticalWeaponReloaded, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Reloaded.BattleId = BattleId;
	Reloaded.OperationId = OperationId;
	Reloaded.SiteId = SiteId;
	Reloaded.TacticalUnitId = UnitId;
	Reloaded.RuleId = Command.WeaponItemId;
	Reloaded.Amount = -Weapon->TacticalReloadActionPointCost;
	Reloaded.Quantity = EventLoadedAmmunition;
	Reloaded.bSuccessful = true;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FEjectTacticalMagazineCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical magazines cannot be ejected after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Magazine ejection references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Magazine ejection requires confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot eject magazines."));
		return Result;
	}
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Magazine ejection references an unknown unit."));
		return Result;
	}
	if (ExistingUnit->Team != ETacticalTeam::Player || ExistingUnit->Team != ExistingBattle->ActiveTeam
		|| ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Only a living player unit on the active team can eject a magazine."));
		return Result;
	}
	const FItemRule* Weapon = Rules.Items.Find(Command.WeaponItemId);
	const FTacticalWeaponState* ExistingWeaponState = ExistingUnit->WeaponStates.FindByPredicate(
		[&Command](const FTacticalWeaponState& Entry) { return Entry.WeaponItemId == Command.WeaponItemId; });
	if (Weapon == nullptr || !Weapon->IsTacticalWeapon() || Weapon->TacticalAmmunitionItemId.IsNone()
		|| Weapon->TacticalMagazineCapacity <= 0 || Weapon->TacticalReloadActionPointCost <= 0
		|| ExistingWeaponState == nullptr)
	{
		AddError(Result, TEXT("invalid_tactical_ejection"), TEXT("Magazine ejection requires a carried magazine-fed weapon with a valid reload profile."));
		return Result;
	}
	if (ExistingWeaponState->LoadedAmmunition <= 0)
	{
		AddError(Result, TEXT("tactical_weapon_empty"), TEXT("The selected weapon has no loaded magazine to eject."));
		return Result;
	}
	if (ExistingUnit->EjectedMagazines.Num() >= 16)
	{
		AddError(Result, TEXT("tactical_magazine_inventory_full"), TEXT("This unit cannot retain another individually tracked magazine."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < Weapon->TacticalReloadActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Unit lacks the action points required to eject this magazine."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	FTacticalWeaponState* WeaponState = Unit->WeaponStates.FindByPredicate(
		[&Command](const FTacticalWeaponState& Entry) { return Entry.WeaponItemId == Command.WeaponItemId; });
	check(WeaponState != nullptr && WeaponState->LoadedAmmunition > 0);
	const int32 EjectedAmmunition = WeaponState->LoadedAmmunition;
	FTacticalMagazineState& Ejected = Unit->EjectedMagazines.AddDefaulted_GetRef();
	Ejected.WeaponItemId = Command.WeaponItemId;
	Ejected.AmmunitionItemId = Weapon->TacticalAmmunitionItemId;
	Ejected.LoadedAmmunition = EjectedAmmunition;
	WeaponState->LoadedAmmunition = 0;
	Unit->RemainingActionPoints -= Weapon->TacticalReloadActionPointCost;
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& EjectedEvent = AddEvent(Result, EStrategicEventType::TacticalMagazineEjected,
		Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	EjectedEvent.BattleId = BattleId;
	EjectedEvent.OperationId = OperationId;
	EjectedEvent.SiteId = SiteId;
	EjectedEvent.TacticalUnitId = UnitId;
	EjectedEvent.RuleId = Command.WeaponItemId;
	EjectedEvent.Amount = -Weapon->TacticalReloadActionPointCost;
	EjectedEvent.Quantity = EjectedAmmunition;
	EjectedEvent.bSuccessful = true;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FInteractTacticalObjectiveCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical objectives cannot be operated after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical objective command references an unknown battlefield."));
		return Result;
	}
	const FTacticalMissionRule* Mission = Rules.TacticalMissions.Find(ExistingBattle->MissionRuleId);
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	const FTacticalObjectiveState* ExistingObjective = ExistingBattle->Objectives.FindByPredicate(
		[&Command](const FTacticalObjectiveState& Objective) { return Objective.ObjectiveId == Command.ObjectiveId; });
	if (Mission == nullptr || ExistingUnit == nullptr || ExistingObjective == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_objective"), TEXT("Tactical objective command references an unknown unit, objective, or mission rule."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Tactical objective cannot be operated after the battle resolves."));
		return Result;
	}
	const ETacticalBattlePhase RequiredPhase = ExistingUnit->Team == ETacticalTeam::Player
		? ETacticalBattlePhase::PlayerTurn
		: ETacticalBattlePhase::AdversaryTurn;
	if (ExistingBattle->Phase != RequiredPhase || ExistingBattle->ActiveTeam != ExistingUnit->Team)
	{
		AddError(Result, TEXT("inactive_tactical_unit"), TEXT("Tactical objective unit does not belong to the active team."));
		return Result;
	}
	if ((ExistingUnit->Team != ETacticalTeam::Player && ExistingUnit->Team != ETacticalTeam::Adversary)
		|| ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("invalid_tactical_unit"), TEXT("Only a living, deployed unit can operate a tactical objective."));
		return Result;
	}
	if (ExistingUnit->Team == ETacticalTeam::Adversary
		&& ExistingObjective->Type != ETacticalObjectiveType::Control)
	{
		AddError(Result, TEXT("tactical_objective_player_only"), TEXT("Adversary units can operate only opposed control objectives."));
		return Result;
	}
	if (ExistingObjective->Status != ETacticalObjectiveStatus::Active)
	{
		AddError(Result, TEXT("tactical_objective_inactive"), TEXT("Tactical objective is no longer active."));
		return Result;
	}
	if (FMath::Abs(ExistingUnit->X - ExistingObjective->X)
		+ FMath::Abs(ExistingUnit->Y - ExistingObjective->Y)
		+ FMath::Abs(ExistingUnit->Z - ExistingObjective->Z) > 1)
	{
		AddError(Result, TEXT("tactical_objective_out_of_reach"), TEXT("Tactical unit must occupy or stand adjacent to the objective."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < Mission->ObjectiveActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Tactical unit lacks the action points required to operate the objective."));
		return Result;
	}
	const bool bWouldCompleteRecovery = ExistingUnit->Team == ETacticalTeam::Player
		&& ExistingObjective->Type == ETacticalObjectiveType::Recover
		&& ExistingObjective->CompletedInteractions + 1 == ExistingObjective->RequiredInteractions;
	if (bWouldCompleteRecovery)
	{
		const FTacticalOperationState* Operation = FindTacticalOperation(State, ExistingBattle->OperationId);
		const FCraftState* Craft = Operation != nullptr ? FindCraft(State, Operation->CraftId) : nullptr;
		const FCraftRule* CraftRule = Craft != nullptr ? Rules.Craft.Find(Craft->CraftRuleId) : nullptr;
		TArray<FInventoryStack> ProposedCargo = Craft != nullptr ? Craft->Cargo : TArray<FInventoryStack>();
		TArray<FInventoryStack> ProposedSalvage = Craft != nullptr ? Craft->PendingSalvage : TArray<FInventoryStack>();
		int64 ProposedMass = 0;
		if (Operation == nullptr || Craft == nullptr || CraftRule == nullptr
			|| !TryAdjustInventoryStacks(ProposedCargo, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
			|| !TryAdjustInventoryStacks(ProposedSalvage, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
			|| !TryComputeCargoMass(ProposedCargo, Rules, ProposedMass)
			|| ProposedMass > CraftRule->CargoCapacity)
		{
			AddError(Result, TEXT("tactical_recovery_capacity_exceeded"), TEXT("The transport lacks manifest capacity for the objective recovery reward."));
			return Result;
		}
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	FTacticalObjectiveState* Objective = Battle->Objectives.FindByPredicate(
		[&Command](const FTacticalObjectiveState& Entry) { return Entry.ObjectiveId == Command.ObjectiveId; });
	check(Unit != nullptr && Objective != nullptr);
	Unit->RemainingActionPoints -= Mission->ObjectiveActionPointCost;
	bool bContested = false;
	if (Objective->Type == ETacticalObjectiveType::Control)
	{
		if (Unit->Team == ETacticalTeam::Player)
		{
			if (Objective->AdversaryInteractions > 0)
			{
				--Objective->AdversaryInteractions;
				bContested = true;
			}
			else
			{
				++Objective->CompletedInteractions;
			}
		}
		else if (Objective->CompletedInteractions > 0)
		{
			--Objective->CompletedInteractions;
			bContested = true;
		}
		else
		{
			++Objective->AdversaryInteractions;
		}
	}
	else
	{
		++Objective->CompletedInteractions;
	}
	const bool bCompleted = Objective->CompletedInteractions == Objective->RequiredInteractions;
	const bool bFailed = Objective->AdversaryInteractions == Objective->RequiredInteractions;
	if (bCompleted)
	{
		Objective->Status = ETacticalObjectiveStatus::Completed;
		if (!Battle->bRequiresExtraction)
		{
			Battle->Phase = ETacticalBattlePhase::Resolved;
		}
	}
	else if (bFailed)
	{
		Objective->Status = ETacticalObjectiveStatus::Failed;
		Battle->Phase = ETacticalBattlePhase::Resolved;
	}
	const bool bRecoveredLoot = bCompleted && Objective->Type == ETacticalObjectiveType::Recover;
	if (bRecoveredLoot)
	{
		FTacticalOperationState* Operation = FindTacticalOperation(Transaction, Battle->OperationId);
		FCraftState* Craft = Operation != nullptr ? FindCraft(Transaction, Operation->CraftId) : nullptr;
		if (Operation == nullptr || Craft == nullptr
			|| !TryAdjustInventoryStacks(Battle->Cargo, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
			|| !TryAdjustInventoryStacks(Operation->Cargo, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
			|| !TryAdjustInventoryStacks(Craft->Cargo, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
			|| !TryAdjustInventoryStacks(Craft->PendingSalvage, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity))
		{
			AddError(Result, TEXT("tactical_recovery_manifest_failed"), TEXT("Objective reward could not be committed to the tactical and transport manifests."));
			return Result;
		}
	}
	if (!ValidateCraftState(Transaction, Rules, Result)
		|| !ValidateTacticalOperations(Transaction, Result)
		|| !RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const FName ObjectiveId = Objective->ObjectiveId;
	const ETacticalTeam ActingTeam = Unit->Team;
	const int32 CompletedInteractions = Objective->CompletedInteractions;
	const int32 AdversaryInteractions = Objective->AdversaryInteractions;
	const int32 RequiredInteractions = Objective->RequiredInteractions;
	const int32 ObjectiveX = Objective->X;
	const int32 ObjectiveY = Objective->Y;
	const int32 ObjectiveZ = Objective->Z;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Progressed = AddEvent(Result, EStrategicEventType::TacticalObjectiveProgressed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Progressed.BattleId = BattleId;
	Progressed.OperationId = OperationId;
	Progressed.SiteId = SiteId;
	Progressed.TacticalUnitId = UnitId;
	Progressed.RuleId = ObjectiveId;
	Progressed.ToX = ObjectiveX;
	Progressed.ToY = ObjectiveY;
	Progressed.ToZ = ObjectiveZ;
	Progressed.Amount = -Mission->ObjectiveActionPointCost;
	Progressed.Quantity = CompletedInteractions;
	Progressed.Roll = AdversaryInteractions;
	Progressed.bSuccessful = ActingTeam == ETacticalTeam::Player;
	if (bContested)
	{
		FStrategicEvent& Contested = AddEvent(Result, EStrategicEventType::TacticalObjectiveContested, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Contested.BattleId = BattleId;
		Contested.OperationId = OperationId;
		Contested.SiteId = SiteId;
		Contested.TacticalUnitId = UnitId;
		Contested.RuleId = ObjectiveId;
		Contested.ToX = ObjectiveX;
		Contested.ToY = ObjectiveY;
		Contested.ToZ = ObjectiveZ;
		Contested.Quantity = CompletedInteractions;
		Contested.Roll = AdversaryInteractions;
		Contested.bSuccessful = ActingTeam == ETacticalTeam::Player;
	}
	if (bCompleted)
	{
		FStrategicEvent& Completed = AddEvent(Result, EStrategicEventType::TacticalObjectiveCompleted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Completed.BattleId = BattleId;
		Completed.OperationId = OperationId;
		Completed.SiteId = SiteId;
		Completed.TacticalUnitId = UnitId;
		Completed.RuleId = ObjectiveId;
		Completed.ToX = ObjectiveX;
		Completed.ToY = ObjectiveY;
		Completed.ToZ = ObjectiveZ;
		Completed.Quantity = RequiredInteractions;
		Completed.bSuccessful = true;
	}
	if (bCompleted && !Battle->bRequiresExtraction)
	{
		FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Resolved.BattleId = BattleId;
		Resolved.OperationId = OperationId;
		Resolved.SiteId = SiteId;
		Resolved.RuleId = ObjectiveId;
		Resolved.Quantity = 1;
		Resolved.bSuccessful = true;
	}
	if (bFailed)
	{
		FStrategicEvent& Failed = AddEvent(Result, EStrategicEventType::TacticalObjectiveFailed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Failed.BattleId = BattleId;
		Failed.OperationId = OperationId;
		Failed.SiteId = SiteId;
		Failed.TacticalUnitId = UnitId;
		Failed.RuleId = ObjectiveId;
		Failed.ToX = ObjectiveX;
		Failed.ToY = ObjectiveY;
		Failed.ToZ = ObjectiveZ;
		Failed.Quantity = AdversaryInteractions;
		FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Resolved.BattleId = BattleId;
		Resolved.OperationId = OperationId;
		Resolved.SiteId = SiteId;
		Resolved.RuleId = ObjectiveId;
		Resolved.bSuccessful = false;
	}
	if (bRecoveredLoot)
	{
		FStrategicEvent& Recovered = AddEvent(Result, EStrategicEventType::TacticalLootRecovered, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Recovered.BattleId = BattleId;
		Recovered.OperationId = OperationId;
		Recovered.SiteId = SiteId;
		Recovered.TacticalUnitId = UnitId;
		Recovered.RuleId = Mission->ObjectiveRewardItemId;
		Recovered.Quantity = Mission->ObjectiveRewardQuantity;
		Recovered.bSuccessful = true;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bCompleted || bFailed;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FExtractTacticalUnitCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical units cannot extract after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical extraction references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase != ETacticalBattlePhase::PlayerTurn
		|| ExistingBattle->ActiveTeam != ETacticalTeam::Player)
	{
		AddError(Result, ExistingBattle->Phase == ETacticalBattlePhase::Resolved
			? FName(TEXT("tactical_battle_resolved"))
			: FName(TEXT("inactive_tactical_unit")), TEXT("Tactical extraction is only available during the player turn."));
		return Result;
	}
	const FTacticalMissionRule* Mission = Rules.TacticalMissions.Find(ExistingBattle->MissionRuleId);
	const FTacticalUnitState* ExistingUnit = ExistingBattle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Unit) { return Unit.UnitId == Command.UnitId; });
	if (Mission == nullptr || ExistingUnit == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical extraction references an unknown unit or mission rule."));
		return Result;
	}
	if (ExistingUnit->Team != ETacticalTeam::Player || ExistingUnit->CurrentHealth <= 0 || ExistingUnit->bExtracted)
	{
		AddError(Result, TEXT("invalid_tactical_unit"), TEXT("Only a living, deployed player unit can extract."));
		return Result;
	}
	const FTacticalCellState& ExistingCell = ExistingBattle->Cells[ExistingBattle->GetCellIndex(ExistingUnit->X, ExistingUnit->Y, ExistingUnit->Z)];
	if (!ExistingCell.bExtraction)
	{
		AddError(Result, TEXT("tactical_extraction_unavailable"), TEXT("Player unit must occupy an extraction cell."));
		return Result;
	}
	if (ExistingUnit->RemainingActionPoints < Mission->ExtractionActionPointCost)
	{
		AddError(Result, TEXT("insufficient_action_points"), TEXT("Player unit lacks the action points required to extract."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
		[&Command](const FTacticalUnitState& Entry) { return Entry.UnitId == Command.UnitId; });
	check(Unit != nullptr);
	Unit->RemainingActionPoints -= Mission->ExtractionActionPointCost;
	Unit->bExtracted = true;
	const FTacticalResolutionEvaluation Evaluation = EvaluateTacticalBattleResolution(*Battle);
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FGuid UnitId = Unit->UnitId;
	const FName UnitRuleId = Unit->SourceRuleId;
	const int32 UnitX = Unit->X;
	const int32 UnitY = Unit->Y;
	const int32 UnitZ = Unit->Z;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Extracted = AddEvent(Result, EStrategicEventType::TacticalUnitExtracted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Extracted.BattleId = BattleId;
	Extracted.OperationId = OperationId;
	Extracted.SiteId = SiteId;
	Extracted.TacticalUnitId = UnitId;
	Extracted.RuleId = UnitRuleId;
	Extracted.ToX = UnitX;
	Extracted.ToY = UnitY;
	Extracted.ToZ = UnitZ;
	Extracted.Amount = -Mission->ExtractionActionPointCost;
	if (Evaluation.bResolved)
	{
		FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Resolved.BattleId = BattleId;
		Resolved.OperationId = OperationId;
		Resolved.SiteId = SiteId;
		Resolved.bSuccessful = Evaluation.bObjectiveCompleted;
		Resolved.Quantity = Evaluation.bObjectiveCompleted ? 1 : 0;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = Evaluation.bResolved;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FEndTacticalTurnCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical turns cannot advance after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical turn command references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Deployment)
	{
		AddError(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical turn flow requires confirmed deployment."));
		return Result;
	}
	if (ExistingBattle->Phase == ETacticalBattlePhase::Resolved)
	{
		AddError(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot advance turns."));
		return Result;
	}

	FCampaignState Transaction = State;
	FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
	check(Battle != nullptr);
	const ETacticalTeam EndingTeam = Battle->ActiveTeam;
	bool bTurnLimitReached = false;
	FTacticalEnvironmentOutcome EnvironmentOutcome;
	FTacticalResolutionEvaluation EnvironmentResolution;
	if (EndingTeam == ETacticalTeam::Player)
	{
		Battle->Phase = ETacticalBattlePhase::AdversaryTurn;
		Battle->ActiveTeam = ETacticalTeam::Adversary;
	}
	else if (Battle->TurnNumber >= Battle->TurnLimit)
	{
		Battle->Phase = ETacticalBattlePhase::Resolved;
		for (FTacticalObjectiveState& Objective : Battle->Objectives)
		{
			Objective.Status = ETacticalObjectiveStatus::Failed;
		}
		bTurnLimitReached = true;
	}
	else
	{
		++Battle->TurnNumber;
		Battle->Phase = ETacticalBattlePhase::PlayerTurn;
		Battle->ActiveTeam = ETacticalTeam::Player;
	}
	if (!bTurnLimitReached)
	{
		EnvironmentOutcome = AdvanceTacticalEnvironment(*Battle, Rules);
		EnvironmentResolution = EvaluateTacticalBattleResolution(*Battle);
	}
	if (!RefreshAndValidateTacticalBattles(Transaction, Rules, Result))
	{
		return Result;
	}
	const FGuid BattleId = Battle->BattleId;
	const FGuid OperationId = Battle->OperationId;
	const FGuid SiteId = Battle->SiteId;
	const FName MissionRuleId = Battle->MissionRuleId;
	const int32 TurnNumber = Battle->TurnNumber;
	const ETacticalWindDirection WindDirection = Battle->WindDirection;
	const int32 WindStrength = Battle->WindStrength;
	const int32 TacticalLevels = Battle->Levels;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Ended = AddEvent(Result, EStrategicEventType::TacticalTurnEnded, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Ended.BattleId = BattleId;
	Ended.OperationId = OperationId;
	Ended.SiteId = SiteId;
	Ended.RuleId = MissionRuleId;
	Ended.Amount = static_cast<int64>(EndingTeam);
	Ended.Quantity = TurnNumber;
	if (!bTurnLimitReached)
	{
		FStrategicEvent& Environment = AddEvent(Result, EStrategicEventType::TacticalEnvironmentAdvanced, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Environment.BattleId = BattleId;
		Environment.OperationId = OperationId;
		Environment.SiteId = SiteId;
		Environment.RuleId = FName(TEXT("effect.tactical-environment"));
		Environment.Amount = EnvironmentOutcome.SmokeCellCount;
		Environment.Quantity = EnvironmentOutcome.FireCellCount;
		Environment.WindDirection = WindDirection;
		Environment.WindStrength = WindStrength;
		if (WindStrength > 0
			&& (EnvironmentOutcome.WindTransportedSmokeAmount > 0 || EnvironmentOutcome.WindAssistedFireCellCount > 0))
		{
			FStrategicEvent& Wind = AddEvent(Result, EStrategicEventType::TacticalWindApplied, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Wind.BattleId = BattleId;
			Wind.OperationId = OperationId;
			Wind.SiteId = SiteId;
			Wind.RuleId = FName(TEXT("effect.tactical-wind"));
			Wind.WindDirection = WindDirection;
			Wind.WindStrength = WindStrength;
			Wind.Amount = EnvironmentOutcome.WindTransportedSmokeAmount;
			Wind.Quantity = EnvironmentOutcome.WindAssistedFireCellCount;
		}
		if (EnvironmentOutcome.VentilatedSmokeAmount > 0)
		{
			FStrategicEvent& Ventilated = AddEvent(Result, EStrategicEventType::TacticalSmokeVentilated, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Ventilated.BattleId = BattleId;
			Ventilated.OperationId = OperationId;
			Ventilated.SiteId = SiteId;
			Ventilated.RuleId = FName(TEXT("effect.tactical-ventilation"));
			Ventilated.WindDirection = WindDirection;
			Ventilated.WindStrength = WindStrength;
			Ventilated.Amount = EnvironmentOutcome.VentilatedSmokeAmount;
			Ventilated.Quantity = EnvironmentOutcome.VentilatedSmokeCellCount;
		}
		if (EnvironmentOutcome.VerticalPropagatedSmokeAmount > 0
			|| EnvironmentOutcome.VerticalSpreadFireCellCount > 0)
		{
			FStrategicEvent& Vertical = AddEvent(Result, EStrategicEventType::TacticalEnvironmentPropagatedVertically, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Vertical.BattleId = BattleId;
			Vertical.OperationId = OperationId;
			Vertical.SiteId = SiteId;
			Vertical.RuleId = FName(TEXT("effect.tactical-vertical-propagation"));
			Vertical.FromZ = 0;
			Vertical.ToZ = TacticalLevels - 1;
			Vertical.Amount = EnvironmentOutcome.VerticalPropagatedSmokeAmount;
			Vertical.Quantity = EnvironmentOutcome.VerticalSpreadFireCellCount;
		}
		if (EnvironmentOutcome.DiffusedSmokeCellCount > 0)
		{
			FStrategicEvent& Diffused = AddEvent(Result, EStrategicEventType::TacticalSmokeDiffused, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Diffused.BattleId = BattleId;
			Diffused.OperationId = OperationId;
			Diffused.SiteId = SiteId;
			Diffused.RuleId = FName(TEXT("effect.tactical-smoke-diffusion"));
			Diffused.Amount = EnvironmentOutcome.DiffusedSmokeCellCount;
			Diffused.Quantity = EnvironmentOutcome.SmokeCellCount;
		}
		if (EnvironmentOutcome.SpreadFireCellCount > 0)
		{
			FStrategicEvent& Spread = AddEvent(Result, EStrategicEventType::TacticalFireSpread, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Spread.BattleId = BattleId;
			Spread.OperationId = OperationId;
			Spread.SiteId = SiteId;
			Spread.RuleId = FName(TEXT("effect.tactical-fire-spread"));
			Spread.Amount = EnvironmentOutcome.SpreadFireCellCount;
			Spread.Quantity = EnvironmentOutcome.FireCellCount;
		}
		for (const FTacticalEnvironmentUnitOutcome& UnitOutcome : EnvironmentOutcome.Units)
		{
			if (UnitOutcome.Damage > 0)
			{
				FStrategicEvent& Burned = AddEvent(Result, EStrategicEventType::TacticalUnitBurned, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Burned.BattleId = BattleId;
				Burned.OperationId = OperationId;
				Burned.SiteId = SiteId;
				Burned.TargetTacticalUnitId = UnitOutcome.UnitId;
				Burned.RuleId = FName(TEXT("effect.tactical-fire"));
				Burned.Amount = -UnitOutcome.Damage;
				Burned.Quantity = UnitOutcome.RemainingHealth;
				FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::TacticalUnitDamaged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Damaged.BattleId = BattleId;
				Damaged.OperationId = OperationId;
				Damaged.SiteId = SiteId;
				Damaged.TargetTacticalUnitId = UnitOutcome.UnitId;
				Damaged.RuleId = FName(TEXT("effect.tactical-fire"));
				Damaged.Amount = -UnitOutcome.Damage;
				Damaged.Quantity = UnitOutcome.RemainingHealth;
			}
			if (UnitOutcome.SuppressionDelta != 0)
			{
				FStrategicEvent& Suppression = AddEvent(Result, EStrategicEventType::TacticalUnitSuppressed, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Suppression.BattleId = BattleId;
				Suppression.OperationId = OperationId;
				Suppression.SiteId = SiteId;
				Suppression.TargetTacticalUnitId = UnitOutcome.UnitId;
				Suppression.RuleId = FName(TEXT("effect.tactical-environment"));
				Suppression.Amount = UnitOutcome.SuppressionDelta;
				Suppression.Quantity = UnitOutcome.Suppression;
			}
			if (UnitOutcome.MoraleDelta != 0)
			{
				FStrategicEvent& Morale = AddEvent(Result, EStrategicEventType::TacticalMoraleChanged, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Morale.BattleId = BattleId;
				Morale.OperationId = OperationId;
				Morale.SiteId = SiteId;
				Morale.TacticalUnitId = UnitOutcome.UnitId;
				Morale.RuleId = UnitOutcome.UnitRuleId;
				Morale.Amount = UnitOutcome.MoraleDelta;
				Morale.Quantity = UnitOutcome.Morale;
			}
			if (UnitOutcome.bPanicked)
			{
				FStrategicEvent& Panicked = AddEvent(Result, EStrategicEventType::TacticalUnitPanicked, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Panicked.BattleId = BattleId;
				Panicked.OperationId = OperationId;
				Panicked.SiteId = SiteId;
				Panicked.TacticalUnitId = UnitOutcome.UnitId;
				Panicked.RuleId = UnitOutcome.UnitRuleId;
			}
			if (UnitOutcome.bIncapacitated)
			{
				FStrategicEvent& Incapacitated = AddEvent(Result, EStrategicEventType::TacticalUnitIncapacitated, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
				Incapacitated.BattleId = BattleId;
				Incapacitated.OperationId = OperationId;
				Incapacitated.SiteId = SiteId;
				Incapacitated.TargetTacticalUnitId = UnitOutcome.UnitId;
				Incapacitated.RuleId = UnitOutcome.UnitRuleId;
			}
		}
		if (EnvironmentResolution.bResolved)
		{
			FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Resolved.BattleId = BattleId;
			Resolved.OperationId = OperationId;
			Resolved.SiteId = SiteId;
			Resolved.RuleId = MissionRuleId;
			Resolved.bSuccessful = EnvironmentResolution.bObjectiveCompleted;
			Resolved.Quantity = EnvironmentResolution.bObjectiveCompleted ? 1 : 0;
		}
	}
	if (bTurnLimitReached)
	{
		FStrategicEvent& Limit = AddEvent(Result, EStrategicEventType::TacticalTurnLimitReached, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Limit.BattleId = BattleId;
		Limit.OperationId = OperationId;
		Limit.SiteId = SiteId;
		Limit.RuleId = MissionRuleId;
		Limit.Quantity = TurnNumber;
		FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalBattleResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Resolved.BattleId = BattleId;
		Resolved.OperationId = OperationId;
		Resolved.SiteId = SiteId;
		Resolved.RuleId = MissionRuleId;
		Resolved.bSuccessful = false;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = bTurnLimitReached || EnvironmentResolution.bResolved
		|| EndingTeam == ETacticalTeam::Adversary
		|| EnvironmentOutcome.Units.ContainsByPredicate(
			[](const FTacticalEnvironmentUnitOutcome& Unit) { return Unit.bIncapacitated || Unit.bPanicked; });
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FRunTacticalAiTurnCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Tactical AI cannot act after the campaign has concluded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalBattleState* ExistingBattle = FindTacticalBattle(State, Command.BattleId);
	if (ExistingBattle == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_battle"), TEXT("Tactical AI turn references an unknown battlefield."));
		return Result;
	}
	if (ExistingBattle->Phase != ETacticalBattlePhase::AdversaryTurn
		|| ExistingBattle->ActiveTeam != ETacticalTeam::Adversary)
	{
		AddError(Result, TEXT("inactive_tactical_ai"), TEXT("Tactical AI turns require active adversary control."));
		return Result;
	}

	FCampaignState Transaction = State;
	TArray<FGuid> UnitIds;
	for (const FTacticalUnitState& Unit : ExistingBattle->Units)
	{
		if (Unit.Team == ETacticalTeam::Adversary && Unit.CurrentHealth > 0 && !Unit.bExtracted)
		{
			UnitIds.Add(Unit.UnitId);
		}
	}
	UnitIds.Sort(
		[](const FGuid& Left, const FGuid& Right)
		{
			return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
		});

	static constexpr int32 MaximumActionsPerUnit = 8;
	static constexpr int32 MaximumTurnActions = 128;
	int32 ActionCount = 0;
	TSet<FGuid> UnitsWithActions;
	for (const FGuid UnitId : UnitIds)
	{
		for (int32 UnitAction = 0;
			UnitAction < MaximumActionsPerUnit && ActionCount < MaximumTurnActions;
			++UnitAction)
		{
			const FTacticalBattleState* Battle = FindTacticalBattle(Transaction, Command.BattleId);
			if (Battle == nullptr || Battle->Phase == ETacticalBattlePhase::Resolved)
			{
				break;
			}
			const FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
				[&UnitId](const FTacticalUnitState& Entry) { return Entry.UnitId == UnitId; });
			if (Unit == nullptr || Unit->CurrentHealth <= 0 || Unit->bExtracted || Unit->RemainingActionPoints <= 0)
			{
				break;
			}

			const FTacticalAiDecision Decision = FTacticalAiService::ChooseAction(
				*Battle, Transaction, Rules, UnitId);
			if (!Decision.bSucceeded)
			{
				Result.Events.Reset();
				for (const FTacticalAiDiagnostic& Diagnostic : Decision.Diagnostics)
				{
					AddError(Result, Diagnostic.Code, Diagnostic.Message);
				}
				AddError(Result, TEXT("tactical_ai_planning_failed"), TEXT("Tactical AI could not produce a valid action plan."));
				return Result;
			}
			if (Decision.ActionType == ETacticalAiActionType::None)
			{
				break;
			}

			const FGuid BattleId = Battle->BattleId;
			const FGuid OperationId = Battle->OperationId;
			const FGuid SiteId = Battle->SiteId;
			const int32 FromX = Unit->X;
			const int32 FromY = Unit->Y;
			const int32 FromZ = Unit->Z;
			FStrategicCommandResult ActionResult;
			switch (Decision.ActionType)
			{
			case ETacticalAiActionType::AttackUnit:
			{
				FAttackTacticalUnitCommand Attack;
				Attack.ExpectedSequence = Transaction.CommandSequence;
				Attack.BattleId = BattleId;
				Attack.AttackerUnitId = UnitId;
				Attack.TargetUnitId = Decision.TargetUnitId;
				ActionResult = Execute(Transaction, Rules, Attack);
				break;
			}
			case ETacticalAiActionType::ProjectSignal:
			{
				FProjectTacticalSignalCommand Signal;
				Signal.ExpectedSequence = Transaction.CommandSequence;
				Signal.BattleId = BattleId;
				Signal.AttackerUnitId = UnitId;
				Signal.TargetUnitId = Decision.TargetUnitId;
				ActionResult = Execute(Transaction, Rules, Signal);
				break;
			}
			case ETacticalAiActionType::Move:
			{
				FMoveTacticalUnitCommand Move;
				Move.ExpectedSequence = Transaction.CommandSequence;
				Move.BattleId = BattleId;
				Move.UnitId = UnitId;
				Move.DestinationX = Decision.DestinationX;
				Move.DestinationY = Decision.DestinationY;
				Move.DestinationZ = Decision.DestinationZ;
				ActionResult = Execute(Transaction, Rules, Move);
				break;
			}
			case ETacticalAiActionType::OpenDoor:
			{
				FSetTacticalDoorCommand Door;
				Door.ExpectedSequence = Transaction.CommandSequence;
				Door.BattleId = BattleId;
				Door.UnitId = UnitId;
				Door.TargetX = Decision.DestinationX;
				Door.TargetY = Decision.DestinationY;
				Door.TargetZ = Decision.DestinationZ;
				Door.bOpen = true;
				ActionResult = Execute(Transaction, Rules, Door);
				break;
			}
			case ETacticalAiActionType::ChangeStance:
			{
				FChangeTacticalStanceCommand Stance;
				Stance.ExpectedSequence = Transaction.CommandSequence;
				Stance.BattleId = BattleId;
				Stance.UnitId = UnitId;
				Stance.Stance = Decision.DesiredStance;
				ActionResult = Execute(Transaction, Rules, Stance);
				break;
			}
			case ETacticalAiActionType::InteractObjective:
			{
				FInteractTacticalObjectiveCommand Interact;
				Interact.ExpectedSequence = Transaction.CommandSequence;
				Interact.BattleId = BattleId;
				Interact.UnitId = UnitId;
				Interact.ObjectiveId = Decision.ObjectiveId;
				ActionResult = Execute(Transaction, Rules, Interact);
				break;
			}
			default:
				AddError(Result, TEXT("invalid_tactical_ai_action"), TEXT("Tactical AI selected an unsupported action."));
				return Result;
			}
			if (!ActionResult.bAccepted)
			{
				Result.Events.Reset();
				Result.Diagnostics.Append(ActionResult.Diagnostics);
				AddError(Result, TEXT("tactical_ai_action_failed"), TEXT("Tactical AI action was rejected; the turn was rolled back."));
				return Result;
			}

			FName GoalRuleId = TEXT("ai.guard");
			if (Decision.Goal == ETacticalAiGoal::Engage)
			{
				GoalRuleId = TEXT("ai.engage");
			}
			else if (Decision.Goal == ETacticalAiGoal::Advance)
			{
				GoalRuleId = TEXT("ai.advance");
			}
			else if (Decision.Goal == ETacticalAiGoal::Withdraw)
			{
				GoalRuleId = TEXT("ai.withdraw");
			}
			else if (Decision.Goal == ETacticalAiGoal::ControlObjective)
			{
				GoalRuleId = TEXT("ai.control-objective");
			}
			FStrategicEvent& DecisionEvent = AddEvent(
				Result,
				EStrategicEventType::TacticalAiDecisionMade,
				Transaction.CommandSequence,
				Transaction.StrategicTime.Utc);
			DecisionEvent.BattleId = BattleId;
			DecisionEvent.OperationId = OperationId;
			DecisionEvent.SiteId = SiteId;
			DecisionEvent.TacticalUnitId = UnitId;
			DecisionEvent.TargetTacticalUnitId = Decision.TargetUnitId;
			DecisionEvent.RuleId = GoalRuleId;
			DecisionEvent.PolicyId = FTacticalAiService::GetPosturePolicyId(Decision.Posture);
			DecisionEvent.FromX = FromX;
			DecisionEvent.FromY = FromY;
			DecisionEvent.FromZ = FromZ;
			DecisionEvent.ToX = Decision.DestinationX;
			DecisionEvent.ToY = Decision.DestinationY;
			DecisionEvent.ToZ = Decision.DestinationZ;
			DecisionEvent.HitChance = Decision.HitChance;
			DecisionEvent.Amount = static_cast<int64>(Decision.ActionType);
			DecisionEvent.Quantity = Decision.UtilityScore;
			DecisionEvent.bSuccessful = true;
			Result.Events.Append(MoveTemp(ActionResult.Events));
			Result.Diagnostics.Append(MoveTemp(ActionResult.Diagnostics));
			Result.bDecisionPause |= ActionResult.bDecisionPause;
			++ActionCount;
			UnitsWithActions.Add(UnitId);
		}
		if (ActionCount >= MaximumTurnActions)
		{
			break;
		}
	}

	const FTacticalBattleState* BattleBeforeEnd = FindTacticalBattle(Transaction, Command.BattleId);
	if (BattleBeforeEnd != nullptr && BattleBeforeEnd->Phase == ETacticalBattlePhase::AdversaryTurn)
	{
		FEndTacticalTurnCommand EndTurn;
		EndTurn.ExpectedSequence = Transaction.CommandSequence;
		EndTurn.BattleId = Command.BattleId;
		FStrategicCommandResult EndResult = Execute(Transaction, Rules, EndTurn);
		if (!EndResult.bAccepted)
		{
			Result.Events.Reset();
			Result.Diagnostics.Append(EndResult.Diagnostics);
			AddError(Result, TEXT("tactical_ai_end_turn_failed"), TEXT("Tactical AI could not complete its turn; all actions were rolled back."));
			return Result;
		}
		Result.Events.Append(MoveTemp(EndResult.Events));
		Result.Diagnostics.Append(MoveTemp(EndResult.Diagnostics));
		Result.bDecisionPause |= EndResult.bDecisionPause;
	}

	const FTacticalBattleState* CompletedBattle = FindTacticalBattle(Transaction, Command.BattleId);
	check(CompletedBattle != nullptr);
	FStrategicEvent& Completed = AddEvent(
		Result,
		EStrategicEventType::TacticalAiTurnCompleted,
		Transaction.CommandSequence,
		Transaction.StrategicTime.Utc);
	Completed.BattleId = CompletedBattle->BattleId;
	Completed.OperationId = CompletedBattle->OperationId;
	Completed.SiteId = CompletedBattle->SiteId;
	Completed.RuleId = FName(TEXT("ai.turn"));
	Completed.Amount = ActionCount;
	Completed.Quantity = UnitsWithActions.Num();
	Completed.bSuccessful = true;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FResolveTacticalOperationCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Config.TacticalSiteScorePerThreat <= 0 || Config.RecoveryHoursPerHealth <= 0
		|| Config.PersonnelExperiencePerRank <= 0
		|| Config.MaxPersonnelRank <= 0 || Config.MaxPersonnelRank > 100)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("Tactical score, recovery, experience, and rank settings must be positive and bounded."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}
	const FTacticalOperationState* ExistingOperation = FindTacticalOperation(State, Command.OperationId);
	if (ExistingOperation == nullptr)
	{
		AddError(Result, TEXT("unknown_tactical_operation"), TEXT("Pending tactical operation does not exist."));
		return Result;
	}
	if (ExistingOperation->Type == ETacticalOperationType::BaseDefense)
	{
		if (State.Outcome != ECampaignOutcome::Ongoing)
		{
			AddError(Result, TEXT("campaign_concluded"), TEXT("A tactical base defense cannot resolve after the campaign has concluded."));
			return Result;
		}
		if (!ValidateAdversaryConfig(Config, Result)
			|| !ValidateFacilityState(State, Rules, Result)
			|| !ValidateStrategicContacts(State, Rules, Result)
			|| !ValidateAdversaryState(State, Rules, Config, Result))
		{
			return Result;
		}

		const FTacticalBattleState* ExistingBattle = State.TacticalBattles.FindByPredicate(
			[&Command](const FTacticalBattleState& Battle) { return Battle.OperationId == Command.OperationId; });
		const FBaseAssaultState* ExistingAssault = FindBaseAssault(State, ExistingOperation->AssaultId);
		const FStrategicBaseState* ExistingBase = FindBase(State, ExistingOperation->BaseId);
		const FAdversaryMissionState* ExistingMission = ExistingAssault != nullptr
			? FindAdversaryMissionById(State, ExistingAssault->MissionId)
			: nullptr;
		const FStrategicContactState* ExistingContact = ExistingAssault != nullptr
			? FindContact(State, ExistingAssault->ContactId)
			: nullptr;
		const FAdversaryMissionRule* MissionRule = ExistingMission != nullptr
			? Rules.AdversaryMissions.Find(ExistingMission->MissionRuleId)
			: nullptr;
		const FContactRule* ContactRule = ExistingContact != nullptr
			? Rules.Contacts.Find(ExistingContact->ContactRuleId)
			: nullptr;
		const FTacticalMissionRule* DebriefMission = ExistingBattle != nullptr
			? Rules.TacticalMissions.Find(ExistingBattle->MissionRuleId)
			: nullptr;
		if (ExistingBattle == nullptr || ExistingBattle->Phase != ETacticalBattlePhase::Resolved
			|| ExistingBattle->bRequiresExtraction || ExistingBattle->SiteId.IsValid()
			|| ExistingAssault == nullptr || ExistingBase == nullptr || ExistingMission == nullptr
			|| ExistingContact == nullptr || MissionRule == nullptr || ContactRule == nullptr || DebriefMission == nullptr
			|| DebriefMission->Context != ETacticalMissionContext::BaseDefense
			|| ExistingAssault->BaseId != ExistingOperation->BaseId
			|| ExistingAssault->AssaultId != ExistingOperation->AssaultId
			|| ExistingMission->ContactId != ExistingAssault->ContactId
			|| ExistingMission->TargetBaseId != ExistingOperation->BaseId)
		{
			AddError(Result, TEXT("invalid_base_defense_operation"), TEXT("Resolved tactical defense has inconsistent battle, assault, mission, contact, or base links."));
			return Result;
		}
		const bool bBattleObjectiveCompleted = !ExistingBattle->Objectives.IsEmpty()
			&& !ExistingBattle->Objectives.ContainsByPredicate(
				[](const FTacticalObjectiveState& Objective)
				{
					return Objective.Status != ETacticalObjectiveStatus::Completed;
				});
		if (Command.bObjectiveCompleted != bBattleObjectiveCompleted)
		{
			AddError(Result, TEXT("tactical_resolution_mismatch"), TEXT("Strategic base-defense result does not match the resolved battlefield objectives."));
			return Result;
		}

		int64 ExperienceAward64 = DebriefMission->MissionExperienceReward;
		if (Command.bObjectiveCompleted)
		{
			ExperienceAward64 += DebriefMission->ObjectiveExperienceReward;
		}
		if (ExperienceAward64 < 0 || ExperienceAward64 > MAX_int32)
		{
			AddError(Result, TEXT("personnel_experience_overflow"), TEXT("Base-defense experience reward exceeds the personnel numeric range."));
			return Result;
		}
		const int32 ExperienceAward = static_cast<int32>(ExperienceAward64);
		int64 PotentialScore = State.CampaignScore;
		if (Command.bObjectiveCompleted && !TryAdd(State.CampaignScore, ContactRule->ScoreValue, PotentialScore))
		{
			AddError(Result, TEXT("campaign_score_overflow"), TEXT("Tactical base-defense score would exceed the campaign numeric range."));
			return Result;
		}
		for (const FGuid& AgentId : ExistingOperation->AgentIds)
		{
			const FPersonnelState* Agent = FindPersonnel(State, AgentId);
			const FTacticalUnitState* Unit = ExistingBattle->Units.FindByPredicate(
				[&AgentId](const FTacticalUnitState& Entry) { return Entry.PersonnelId == AgentId; });
			if (Agent == nullptr || Agent->Status != EPersonnelStatus::Deployed || Agent->Missions == MAX_int32
				|| static_cast<int64>(Agent->Experience) + ExperienceAward > MAX_int32
				|| Unit == nullptr || Unit->CurrentHealth > Agent->CurrentHealth)
			{
				AddError(Result, TEXT("invalid_tactical_agent"), TEXT("Base defender health or service record cannot be updated."));
				return Result;
			}
			const int32 Damage = Agent->CurrentHealth - Unit->CurrentHealth;
			if (Unit->CurrentHealth > 0 && Damage > 0
				&& static_cast<int64>(Damage) > MAX_int64 / Config.RecoveryHoursPerHealth / 3600LL)
			{
				AddError(Result, TEXT("personnel_recovery_overflow"), TEXT("Base defender recovery duration exceeds the supported range."));
				return Result;
			}
		}

		TArray<FGuid> DamageCandidateIds;
		for (const FBaseFacilityState& Facility : ExistingBase->Facilities)
		{
			const FFacilityRule* FacilityRule = Rules.Facilities.Find(Facility.FacilityId);
			if (FacilityRule == nullptr)
			{
				AddError(Result, TEXT("unknown_facility"), TEXT("Base-defense breach references an unloaded facility rule."));
				return Result;
			}
			if (Facility.Damage < FacilityRule->MaxIntegrity)
			{
				DamageCandidateIds.Add(Facility.InstanceId);
			}
		}
		DamageCandidateIds.Sort(
			[](const FGuid& Left, const FGuid& Right)
			{
				return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
			});
		const int32 MaximumFacilityTargets = FMath::Min(DamageCandidateIds.Num(), MissionRule->BaseFacilitiesHit);
		if (!Command.bObjectiveCompleted
			&& WouldExhaustDeterministicRandomStream(State.SimulationRandom, static_cast<int64>(MaximumFacilityTargets)))
		{
			AddError(Result, TEXT("random_draw_overflow"), TEXT("Tactical base-defense breach exceeds the deterministic random-stream draw range."));
			return Result;
		}

		struct FBaseDefensePersonnelOutcome
		{
			FGuid PersonnelId;
			int32 AppliedDamage = 0;
			int32 RemainingHealth = 0;
			bool bDied = false;
		};
		struct FBaseDefenseProgressionOutcome
		{
			FGuid PersonnelId;
			int32 ExperienceGained = 0;
			int32 TotalExperience = 0;
			int32 PreviousRank = 1;
			int32 NewRank = 1;
		};
		struct FBaseDefenseCommendationOutcome
		{
			FGuid PersonnelId;
			TArray<FName> CommendationIds;
		};

		FCampaignState Transaction = State;
		SortStateCollections(Transaction);
		const FTacticalOperationState Operation = *FindTacticalOperation(Transaction, Command.OperationId);
		const FTacticalBattleState* Battle = Transaction.TacticalBattles.FindByPredicate(
			[&Operation](const FTacticalBattleState& Entry) { return Entry.OperationId == Operation.OperationId; });
		check(Battle != nullptr);
		const FGuid BattleId = Battle->BattleId;
		const FGuid BaseId = Operation.BaseId;
		const FGuid AssaultId = Operation.AssaultId;
		const FGuid MissionId = ExistingAssault->MissionId;
		const FGuid ContactId = ExistingAssault->ContactId;
		const FName MissionRuleId = ExistingMission->MissionRuleId;
		const FName ContactRuleId = ExistingContact->ContactRuleId;
		const FName RegionId = ExistingBase->RegionId;
		const int64 NextSequence = Transaction.CommandSequence + 1;
		TArray<FBaseDefensePersonnelOutcome> PersonnelOutcomes;
		TArray<FBaseDefenseProgressionOutcome> ProgressionOutcomes;
		TArray<FBaseDefenseCommendationOutcome> CommendationOutcomes;
		TArray<FGuid> CasualtyIds;
		TArray<FGuid> SurvivorIds;
		TArray<FPersonnelSquadBondAdvance> SquadBondAdvances;
		const FName CasualtyCause(TEXT("cause.base-defense-casualty"));
		for (const FGuid& AgentId : Operation.AgentIds)
		{
			FPersonnelState* Agent = FindPersonnel(Transaction, AgentId);
			const FTacticalUnitState* Unit = Battle->Units.FindByPredicate(
				[&AgentId](const FTacticalUnitState& Entry) { return Entry.PersonnelId == AgentId; });
			check(Agent != nullptr && Unit != nullptr);
			if (Unit->CurrentHealth > 0)
			{
				SurvivorIds.Add(AgentId);
			}
			++Agent->Missions;
			if (ExperienceAward > 0)
			{
				FBaseDefenseProgressionOutcome& Progression = ProgressionOutcomes.AddDefaulted_GetRef();
				Progression.PersonnelId = AgentId;
				Progression.ExperienceGained = ExperienceAward;
				Progression.PreviousRank = Agent->Rank;
				Agent->Experience += ExperienceAward;
				Agent->Rank = FMath::Max(Agent->Rank,
					FMath::Min(Config.MaxPersonnelRank, 1 + Agent->Experience / Config.PersonnelExperiencePerRank));
				Progression.TotalExperience = Agent->Experience;
				Progression.NewRank = Agent->Rank;
				Agent->PendingDoctrineChoices += Progression.NewRank - Progression.PreviousRank;
			}
			FBaseDefenseCommendationOutcome CommendationOutcome;
			CommendationOutcome.PersonnelId = AgentId;
			AwardEligiblePersonnelCommendations(
				*Agent, Command.bObjectiveCompleted, Rules, CommendationOutcome.CommendationIds);
			if (!CommendationOutcome.CommendationIds.IsEmpty())
			{
				CommendationOutcomes.Add(MoveTemp(CommendationOutcome));
			}
			SyncTacticalConsumablesToPersonnel(*Agent, *Unit, Rules);
			const int32 Damage = Agent->CurrentHealth - Unit->CurrentHealth;
			if (Damage <= 0)
			{
				Agent->Status = EPersonnelStatus::Available;
				Agent->RecoveryPlan = EPersonnelRecoveryPlan::None;
				continue;
			}

			FBaseDefensePersonnelOutcome& Outcome = PersonnelOutcomes.AddDefaulted_GetRef();
			Outcome.PersonnelId = AgentId;
			Outcome.AppliedDamage = Damage;
			Outcome.RemainingHealth = Unit->CurrentHealth;
			Outcome.bDied = Unit->CurrentHealth == 0;
			if (!Outcome.bDied)
			{
				Agent->CurrentHealth = Unit->CurrentHealth;
				Agent->Status = EPersonnelStatus::Recovering;
				Agent->RemainingRecoverySeconds = static_cast<int64>(Damage)
					* Config.RecoveryHoursPerHealth * 3600LL;
				Agent->RecoveryPlan = EPersonnelRecoveryPlan::DecisionRequired;
				continue;
			}

			FStrategicBaseState* HomeBase = FindBase(Transaction, Agent->BaseId);
			if (HomeBase == nullptr)
			{
				AddError(Result, TEXT("invalid_tactical_casualty_state"), TEXT("Base-defense casualty references an unavailable home base."));
				return Result;
			}
			for (const FName ItemId : Agent->EquippedItems)
			{
				if (!TryAdjustInventory(*HomeBase, ItemId, 1))
				{
					AddError(Result, TEXT("inventory_overflow"), FString::Printf(TEXT("Recovering base-defense equipment '%s' would overflow inventory."), *ItemId.ToString()));
					return Result;
				}
			}
			FMemorialRecord& Record = Transaction.Memorial.AddDefaulted_GetRef();
			Record.PersonnelId = Agent->PersonnelId;
			Record.DisplayName = Agent->DisplayName;
			Record.RoleId = Agent->RoleId;
			Record.Rank = Agent->Rank;
			Record.Missions = Agent->Missions;
			Record.Kills = Agent->Kills;
			Record.DoctrineSelections = Agent->DoctrineSelections;
			Record.Commendations = Agent->Commendations;
			Record.StewardshipToursCompleted = Agent->StewardshipToursCompleted;
			Record.DeathUtc = Transaction.StrategicTime.Utc;
			Record.CauseId = CasualtyCause;
			CasualtyIds.Add(AgentId);
		}
		if (!CasualtyIds.IsEmpty())
		{
			Transaction.Personnel.RemoveAll(
				[&CasualtyIds](const FPersonnelState& Person) { return CasualtyIds.Contains(Person.PersonnelId); });
		}
		if (Command.bObjectiveCompleted
			&& !AdvancePersonnelSquadBonds(Transaction, SurvivorIds, SquadBondAdvances, Result))
		{
			return Result;
		}

		if (Command.bObjectiveCompleted)
		{
			Transaction.CampaignScore = PotentialScore;
			FStrategicEvent& Destroyed = AddEvent(Result, EStrategicEventType::StrategicContactDestroyed,
				NextSequence, Transaction.StrategicTime.Utc);
			Destroyed.BaseId = BaseId;
			Destroyed.ContactId = ContactId;
			Destroyed.MissionId = MissionId;
			Destroyed.AssaultId = AssaultId;
			Destroyed.OperationId = Operation.OperationId;
			Destroyed.BattleId = BattleId;
			Destroyed.RuleId = ContactRuleId;
			Destroyed.Amount = ContactRule->ScoreValue;
			FStrategicEvent& Repelled = AddEvent(Result, EStrategicEventType::BaseAssaultRepelled,
				NextSequence, Transaction.StrategicTime.Utc);
			Repelled.BaseId = BaseId;
			Repelled.ContactId = ContactId;
			Repelled.MissionId = MissionId;
			Repelled.AssaultId = AssaultId;
			Repelled.OperationId = Operation.OperationId;
			Repelled.BattleId = BattleId;
			Repelled.RuleId = MissionRuleId;
			Repelled.RegionId = RegionId;
			Repelled.Amount = ContactRule->ScoreValue;
			Repelled.Quantity = Operation.AgentIds.Num();
			if (!ApplyAdversaryMissionThwarted(Transaction, Rules, Config, ContactId,
				Result, NextSequence, Transaction.StrategicTime.Utc))
			{
				Result.Events.Reset();
				AddError(Result, TEXT("adversary_state_overflow"), TEXT("Tactical base-defense victory exceeded the campaign numeric range."));
				return Result;
			}
		}
		else
		{
			FStrategicBaseState* Base = FindBase(Transaction, BaseId);
			check(Base != nullptr);
			int32 TotalFacilityDamage = 0;
			int32 FacilitiesHit = 0;
			for (int32 Target = 0; Target < MaximumFacilityTargets; ++Target)
			{
				const int32 SelectedIndex = Transaction.SimulationRandom.NextIntInclusive(0, DamageCandidateIds.Num() - 1);
				const FGuid FacilityInstanceId = DamageCandidateIds[SelectedIndex];
				DamageCandidateIds.RemoveAt(SelectedIndex, EAllowShrinking::No);
				FBaseFacilityState* Facility = FindFacility(*Base, FacilityInstanceId);
				check(Facility != nullptr);
				const FFacilityRule& FacilityRule = Rules.Facilities.FindChecked(Facility->FacilityId);
				const bool bWasOperational = Facility->Damage < FacilityRule.MaxIntegrity;
				const int32 AppliedDamage = FMath::Min(MissionRule->BaseFacilityDamage,
					FacilityRule.MaxIntegrity - Facility->Damage);
				Facility->Damage += AppliedDamage;
				TotalFacilityDamage += AppliedDamage;
				++FacilitiesHit;
				FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::FacilityDamaged,
					NextSequence, Transaction.StrategicTime.Utc);
				Damaged.BaseId = BaseId;
				Damaged.FacilityInstanceId = FacilityInstanceId;
				Damaged.ContactId = ContactId;
				Damaged.MissionId = MissionId;
				Damaged.AssaultId = AssaultId;
				Damaged.OperationId = Operation.OperationId;
				Damaged.BattleId = BattleId;
				Damaged.RuleId = Facility->FacilityId;
				Damaged.Amount = -AppliedDamage;
				Damaged.Quantity = AppliedDamage;
				if (bWasOperational && Facility->Damage == FacilityRule.MaxIntegrity)
				{
					FStrategicEvent& Disabled = AddEvent(Result, EStrategicEventType::FacilityDisabled,
						NextSequence, Transaction.StrategicTime.Utc);
					Disabled.BaseId = BaseId;
					Disabled.FacilityInstanceId = FacilityInstanceId;
					Disabled.ContactId = ContactId;
					Disabled.MissionId = MissionId;
					Disabled.AssaultId = AssaultId;
					Disabled.OperationId = Operation.OperationId;
					Disabled.BattleId = BattleId;
					Disabled.RuleId = Facility->FacilityId;
				}
			}
			FStrategicEvent& Breached = AddEvent(Result, EStrategicEventType::BaseAssaultBreached,
				NextSequence, Transaction.StrategicTime.Utc);
			Breached.BaseId = BaseId;
			Breached.ContactId = ContactId;
			Breached.MissionId = MissionId;
			Breached.AssaultId = AssaultId;
			Breached.OperationId = Operation.OperationId;
			Breached.BattleId = BattleId;
			Breached.RuleId = MissionRuleId;
			Breached.RegionId = RegionId;
			Breached.Amount = -TotalFacilityDamage;
			Breached.Quantity = FacilitiesHit;
			FStrategicEvent& Escaped = AddEvent(Result, EStrategicEventType::StrategicContactEscaped,
				NextSequence, Transaction.StrategicTime.Utc);
			Escaped.BaseId = BaseId;
			Escaped.ContactId = ContactId;
			Escaped.MissionId = MissionId;
			Escaped.AssaultId = AssaultId;
			Escaped.OperationId = Operation.OperationId;
			Escaped.BattleId = BattleId;
			Escaped.RuleId = ContactRuleId;
			Escaped.RegionId = RegionId;
			if (!ApplyAdversaryMissionEscape(Transaction, Rules, Config, ContactId,
				Result, NextSequence, Transaction.StrategicTime.Utc))
			{
				Result.Events.Reset();
				AddError(Result, TEXT("adversary_state_overflow"), TEXT("Tactical base-defense breach exceeded the campaign numeric range."));
				return Result;
			}
		}

		Transaction.BaseAssaults.RemoveAll(
			[&AssaultId](const FBaseAssaultState& Entry) { return Entry.AssaultId == AssaultId; });
		Transaction.StrategicContacts.RemoveAll(
			[&ContactId](const FStrategicContactState& Entry) { return Entry.ContactId == ContactId; });
		Transaction.TacticalOperations.RemoveAll(
			[&Operation](const FTacticalOperationState& Entry) { return Entry.OperationId == Operation.OperationId; });
		Transaction.TacticalBattles.RemoveAll(
			[&Operation](const FTacticalBattleState& Entry) { return Entry.OperationId == Operation.OperationId; });
		if (!ValidatePersonnelState(Transaction, Rules, Result)
			|| !ValidateFacilityState(Transaction, Rules, Result)
			|| !ValidateStrategicContacts(Transaction, Rules, Result)
			|| !ValidateAdversaryState(Transaction, Rules, Config, Result))
		{
			Result.Events.Reset();
			return Result;
		}
		SortStateCollections(Transaction);
		Transaction.CommandSequence = NextSequence;

		FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalOperationResolved,
			NextSequence, Transaction.StrategicTime.Utc);
		Resolved.BaseId = BaseId;
		Resolved.ContactId = ContactId;
		Resolved.MissionId = MissionId;
		Resolved.AssaultId = AssaultId;
		Resolved.OperationId = Operation.OperationId;
		Resolved.BattleId = BattleId;
		Resolved.RuleId = DebriefMission->Identity.RuleId;
		Resolved.Amount = Command.bObjectiveCompleted ? ContactRule->ScoreValue : 0;
		Resolved.Quantity = Command.bObjectiveCompleted ? 1 : 0;
		Resolved.bSuccessful = Command.bObjectiveCompleted;
		for (const FPersonnelSquadBondAdvance& Advance : SquadBondAdvances)
		{
			FStrategicEvent& BondEvent = AddEvent(Result,
				EStrategicEventType::PersonnelSquadBondAdvanced,
				NextSequence, Transaction.StrategicTime.Utc);
			BondEvent.BaseId = BaseId;
			BondEvent.PersonnelId = Advance.FirstPersonnelId;
			BondEvent.RelatedPersonnelId = Advance.SecondPersonnelId;
			BondEvent.ContactId = ContactId;
			BondEvent.AssaultId = AssaultId;
			BondEvent.OperationId = Operation.OperationId;
			BondEvent.BattleId = BattleId;
			BondEvent.PolicyId = FName(TEXT("personnel.squad-bond-field-cadence"));
			BondEvent.PersonnelSquadBondTier = static_cast<int32>(Advance.Tier);
			BondEvent.PersonnelSharedVictories = Advance.SharedVictories;
			BondEvent.PersonnelActionPointBonus = FPersonnelSquadBond::GetActionPointBonus(Advance.Tier);
			BondEvent.PersonnelMoraleBonus = FPersonnelSquadBond::GetMoraleBonus(Advance.Tier);
			BondEvent.Amount = 1;
			BondEvent.Quantity = Advance.SharedVictories;
			BondEvent.bSuccessful = true;
		}
		for (const FBaseDefenseProgressionOutcome& Progression : ProgressionOutcomes)
		{
			FStrategicEvent& Experience = AddEvent(Result, EStrategicEventType::PersonnelExperienceGained,
				NextSequence, Transaction.StrategicTime.Utc);
			Experience.BaseId = BaseId;
			Experience.PersonnelId = Progression.PersonnelId;
			Experience.ContactId = ContactId;
			Experience.AssaultId = AssaultId;
			Experience.OperationId = Operation.OperationId;
			Experience.BattleId = BattleId;
			Experience.RuleId = DebriefMission->Identity.RuleId;
			Experience.Amount = Progression.ExperienceGained;
			Experience.Quantity = Progression.TotalExperience;
			Experience.bSuccessful = Command.bObjectiveCompleted;
			if (Progression.NewRank > Progression.PreviousRank)
			{
				FStrategicEvent& Promoted = AddEvent(Result, EStrategicEventType::PersonnelPromoted,
					NextSequence, Transaction.StrategicTime.Utc);
				Promoted.BaseId = BaseId;
				Promoted.PersonnelId = Progression.PersonnelId;
				Promoted.ContactId = ContactId;
				Promoted.AssaultId = AssaultId;
				Promoted.OperationId = Operation.OperationId;
				Promoted.BattleId = BattleId;
				Promoted.RuleId = DebriefMission->Identity.RuleId;
				Promoted.Amount = Progression.PreviousRank;
				Promoted.Quantity = Progression.NewRank;
				Promoted.bSuccessful = true;
			}
		}
		for (const FBaseDefenseCommendationOutcome& Outcome : CommendationOutcomes)
		{
			for (const FName CommendationId : Outcome.CommendationIds)
			{
				FStrategicEvent& Awarded = AddEvent(Result, EStrategicEventType::PersonnelCommendationAwarded,
					NextSequence, Transaction.StrategicTime.Utc);
				Awarded.BaseId = BaseId;
				Awarded.PersonnelId = Outcome.PersonnelId;
				Awarded.ContactId = ContactId;
				Awarded.AssaultId = AssaultId;
				Awarded.OperationId = Operation.OperationId;
				Awarded.BattleId = BattleId;
				Awarded.RuleId = CommendationId;
				Awarded.bSuccessful = Command.bObjectiveCompleted;
			}
		}
		for (const FBaseDefensePersonnelOutcome& Outcome : PersonnelOutcomes)
		{
			FStrategicEvent& PersonnelEvent = AddEvent(Result,
				Outcome.bDied ? EStrategicEventType::PersonnelDied : EStrategicEventType::PersonnelInjured,
				NextSequence, Transaction.StrategicTime.Utc);
			PersonnelEvent.BaseId = BaseId;
			PersonnelEvent.PersonnelId = Outcome.PersonnelId;
			PersonnelEvent.ContactId = ContactId;
			PersonnelEvent.AssaultId = AssaultId;
			PersonnelEvent.OperationId = Operation.OperationId;
			PersonnelEvent.BattleId = BattleId;
			PersonnelEvent.RuleId = CasualtyCause;
			PersonnelEvent.Amount = -Outcome.AppliedDamage;
			PersonnelEvent.Quantity = Outcome.RemainingHealth;
		}
		State = MoveTemp(Transaction);
		Result.bAccepted = true;
		Result.bDecisionPause = true;
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, ExistingOperation->CraftId);
	const FStrategicSiteState* ExistingSite = FindSite(State, ExistingOperation->SiteId);
	if (ExistingCraft == nullptr || ExistingSite == nullptr
		|| ExistingCraft->Status != ECraftStatus::OnSite
		|| ExistingCraft->TargetSiteId != ExistingOperation->SiteId)
	{
		AddError(Result, TEXT("invalid_tactical_operation"), TEXT("Pending tactical operation is not linked to an on-site craft and active site."));
		return Result;
	}
	const FTacticalBattleState* ExistingBattleState = State.TacticalBattles.FindByPredicate(
		[&Command](const FTacticalBattleState& Battle) { return Battle.OperationId == Command.OperationId; });
	if (ExistingBattleState != nullptr && ExistingBattleState->Phase == ETacticalBattlePhase::Resolved)
	{
		const bool bBattleObjectiveCompleted = !ExistingBattleState->Objectives.IsEmpty()
			&& ExistingBattleState->Objectives.ContainsByPredicate(
				[](const FTacticalObjectiveState& Objective)
				{
					return Objective.Status != ETacticalObjectiveStatus::Completed;
				}) == false;
		if (Command.bObjectiveCompleted != bBattleObjectiveCompleted)
		{
			AddError(Result, TEXT("tactical_resolution_mismatch"), TEXT("Strategic tactical result does not match the resolved battlefield objectives."));
			return Result;
		}
	}
	const FTacticalMissionRule* DebriefMission = ExistingBattleState != nullptr
		? Rules.TacticalMissions.Find(ExistingBattleState->MissionRuleId)
		: nullptr;
	int64 ExperienceAward64 = 0;
	if (ExistingBattleState != nullptr && ExistingBattleState->Phase == ETacticalBattlePhase::Resolved
		&& DebriefMission != nullptr)
	{
		ExperienceAward64 = DebriefMission->MissionExperienceReward;
		if (Command.bObjectiveCompleted)
		{
			ExperienceAward64 += DebriefMission->ObjectiveExperienceReward;
		}
	}
	if (ExperienceAward64 < 0 || ExperienceAward64 > MAX_int32)
	{
		AddError(Result, TEXT("personnel_experience_overflow"), TEXT("Tactical mission experience reward exceeds the personnel numeric range."));
		return Result;
	}
	const int32 ExperienceAward = static_cast<int32>(ExperienceAward64);
	int64 ScoreAward = 0;
	int64 NewScore = State.CampaignScore;
	if (Command.bObjectiveCompleted
		&& (!TryMultiplyNonNegative(ExistingSite->ThreatRating, Config.TacticalSiteScorePerThreat, ScoreAward)
			|| !TryAdd(State.CampaignScore, ScoreAward, NewScore)))
	{
		AddError(Result, TEXT("campaign_score_overflow"), TEXT("Tactical operation score exceeds the campaign numeric range."));
		return Result;
	}
	for (const FGuid& AgentId : ExistingOperation->AgentIds)
	{
		const FPersonnelState* Agent = FindPersonnel(State, AgentId);
		if (Agent == nullptr || Agent->Status != EPersonnelStatus::Deployed || Agent->Missions == MAX_int32
			|| static_cast<int64>(Agent->Experience) + ExperienceAward > MAX_int32)
		{
			AddError(Result, TEXT("invalid_tactical_agent"), TEXT("Tactical operation agent service record cannot be updated."));
			return Result;
		}
		if (ExistingBattleState != nullptr && ExistingBattleState->Phase == ETacticalBattlePhase::Resolved)
		{
			const FTacticalUnitState* Unit = ExistingBattleState->Units.FindByPredicate(
				[&AgentId](const FTacticalUnitState& Entry) { return Entry.PersonnelId == AgentId; });
			if (Unit == nullptr || Unit->CurrentHealth > Agent->CurrentHealth)
			{
				AddError(Result, TEXT("invalid_tactical_casualty_state"), TEXT("Resolved tactical health cannot exceed its strategic personnel snapshot."));
				return Result;
			}
		}
	}

	struct FTacticalPersonnelOutcome
	{
		FGuid PersonnelId;
		FGuid BaseId;
		int32 AppliedDamage = 0;
		int32 RemainingHealth = 0;
		bool bDied = false;
	};
	struct FTacticalProgressionOutcome
	{
		FGuid PersonnelId;
		FGuid BaseId;
		int32 ExperienceGained = 0;
		int32 TotalExperience = 0;
		int32 PreviousRank = 1;
		int32 NewRank = 1;
	};
	struct FTacticalCommendationOutcome
	{
		FGuid PersonnelId;
		FGuid BaseId;
		TArray<FName> CommendationIds;
	};

	FCampaignState Transaction = State;
	const FTacticalOperationState Operation = *FindTacticalOperation(Transaction, Command.OperationId);
	FCraftState* Craft = FindCraft(Transaction, Operation.CraftId);
	check(Craft != nullptr);
	const FTacticalBattleState* TransactionBattle = Transaction.TacticalBattles.FindByPredicate(
		[&Operation](const FTacticalBattleState& Battle) { return Battle.OperationId == Operation.OperationId; });
	const FGuid BattleId = TransactionBattle != nullptr ? TransactionBattle->BattleId : FGuid();
	TArray<FTacticalPersonnelOutcome> PersonnelOutcomes;
	TArray<FTacticalProgressionOutcome> ProgressionOutcomes;
	TArray<FTacticalCommendationOutcome> CommendationOutcomes;
	TArray<FGuid> CasualtyIds;
	TArray<FGuid> SurvivorIds;
	TArray<FPersonnelSquadBondAdvance> SquadBondAdvances;
	const FName TacticalCasualtyCause(TEXT("cause.tactical-casualty"));
	for (const FGuid& AgentId : Operation.AgentIds)
	{
		FPersonnelState* Agent = FindPersonnel(Transaction, AgentId);
		check(Agent != nullptr);
		++Agent->Missions;
		if (ExperienceAward > 0)
		{
			FTacticalProgressionOutcome& Progression = ProgressionOutcomes.AddDefaulted_GetRef();
			Progression.PersonnelId = Agent->PersonnelId;
			Progression.BaseId = Agent->BaseId;
			Progression.ExperienceGained = ExperienceAward;
			Progression.PreviousRank = Agent->Rank;
			Agent->Experience += ExperienceAward;
			Agent->Rank = FMath::Max(
				Agent->Rank,
				FMath::Min(Config.MaxPersonnelRank, 1 + Agent->Experience / Config.PersonnelExperiencePerRank));
			Progression.TotalExperience = Agent->Experience;
			Progression.NewRank = Agent->Rank;
			Agent->PendingDoctrineChoices += Progression.NewRank - Progression.PreviousRank;
		}
		FTacticalCommendationOutcome CommendationOutcome;
		CommendationOutcome.PersonnelId = Agent->PersonnelId;
		CommendationOutcome.BaseId = Agent->BaseId;
		AwardEligiblePersonnelCommendations(
			*Agent, Command.bObjectiveCompleted, Rules, CommendationOutcome.CommendationIds);
		if (!CommendationOutcome.CommendationIds.IsEmpty())
		{
			CommendationOutcomes.Add(MoveTemp(CommendationOutcome));
		}
		if (TransactionBattle == nullptr || TransactionBattle->Phase != ETacticalBattlePhase::Resolved)
		{
			SurvivorIds.Add(AgentId);
			continue;
		}
		const FTacticalUnitState* Unit = TransactionBattle->Units.FindByPredicate(
			[&AgentId](const FTacticalUnitState& Entry) { return Entry.PersonnelId == AgentId; });
		check(Unit != nullptr);
		if (Unit->CurrentHealth > 0)
		{
			SurvivorIds.Add(AgentId);
		}
		SyncTacticalConsumablesToPersonnel(*Agent, *Unit, Rules);
		if (Unit->CurrentHealth == Agent->CurrentHealth)
		{
			continue;
		}

		FTacticalPersonnelOutcome& Outcome = PersonnelOutcomes.AddDefaulted_GetRef();
		Outcome.PersonnelId = Agent->PersonnelId;
		Outcome.BaseId = Agent->BaseId;
		Outcome.AppliedDamage = Agent->CurrentHealth - Unit->CurrentHealth;
		Outcome.RemainingHealth = Unit->CurrentHealth;
		Outcome.bDied = Unit->CurrentHealth == 0;
		if (!Outcome.bDied)
		{
			Agent->CurrentHealth = Unit->CurrentHealth;
			continue;
		}

		FStrategicBaseState* Base = FindBase(Transaction, Agent->BaseId);
		if (Base == nullptr)
		{
			AddError(Result, TEXT("invalid_tactical_casualty_state"), TEXT("Tactical casualty references an unavailable home base."));
			return Result;
		}
		for (const FName ItemId : Agent->EquippedItems)
		{
			if (!TryAdjustInventory(*Base, ItemId, 1))
			{
				AddError(Result, TEXT("inventory_overflow"), FString::Printf(TEXT("Recovering casualty equipment '%s' would overflow base inventory."), *ItemId.ToString()));
				return Result;
			}
		}
		FMemorialRecord& Record = Transaction.Memorial.AddDefaulted_GetRef();
		Record.PersonnelId = Agent->PersonnelId;
		Record.DisplayName = Agent->DisplayName;
		Record.RoleId = Agent->RoleId;
		Record.Rank = Agent->Rank;
		Record.Missions = Agent->Missions;
		Record.Kills = Agent->Kills;
		Record.DoctrineSelections = Agent->DoctrineSelections;
		Record.Commendations = Agent->Commendations;
		Record.StewardshipToursCompleted = Agent->StewardshipToursCompleted;
		Record.DeathUtc = Transaction.StrategicTime.Utc;
		Record.CauseId = TacticalCasualtyCause;
		CasualtyIds.Add(Agent->PersonnelId);
		Craft->AssignedAgentIds.Remove(Agent->PersonnelId);
	}
	if (!CasualtyIds.IsEmpty())
	{
		Transaction.Personnel.RemoveAll(
			[&CasualtyIds](const FPersonnelState& Person) { return CasualtyIds.Contains(Person.PersonnelId); });
	}
	if (Command.bObjectiveCompleted
		&& !AdvancePersonnelSquadBonds(Transaction, SurvivorIds, SquadBondAdvances, Result))
	{
		return Result;
	}
	Transaction.CampaignScore = NewScore;
	if (Command.bObjectiveCompleted)
	{
		Transaction.StrategicSites.RemoveAll(
			[&Operation](const FStrategicSiteState& Site) { return Site.SiteId == Operation.SiteId; });
	}
	Craft->Status = ECraftStatus::Returning;
	Craft->TargetSiteId.Invalidate();
	Craft->RemainingRouteSeconds = Craft->ReservedReturnSeconds;
	const FGuid ReturnBaseId = Craft->BaseId;
	const FGuid ReturnCraftId = Craft->CraftId;
	const FName ReturnCraftRuleId = Craft->CraftRuleId;
	Transaction.TacticalOperations.RemoveAll(
		[&Operation](const FTacticalOperationState& Entry) { return Entry.OperationId == Operation.OperationId; });
	Transaction.TacticalBattles.RemoveAll(
		[&Operation](const FTacticalBattleState& Entry) { return Entry.OperationId == Operation.OperationId; });
	if (!ValidatePersonnelState(Transaction, Rules, Result)
		|| !ValidateCraftState(Transaction, Rules, Result)
		|| !ValidateStrategicSites(Transaction, Rules, Result))
	{
		return Result;
	}
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Resolved = AddEvent(Result, EStrategicEventType::TacticalOperationResolved, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Resolved.BaseId = ReturnBaseId;
	Resolved.CraftId = ReturnCraftId;
	Resolved.SiteId = Operation.SiteId;
	Resolved.OperationId = Operation.OperationId;
	Resolved.BattleId = BattleId;
	Resolved.Amount = ScoreAward;
	Resolved.Quantity = Command.bObjectiveCompleted ? 1 : 0;
	Resolved.bSuccessful = Command.bObjectiveCompleted;
	for (const FPersonnelSquadBondAdvance& Advance : SquadBondAdvances)
	{
		FStrategicEvent& BondEvent = AddEvent(Result,
			EStrategicEventType::PersonnelSquadBondAdvanced,
			Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		BondEvent.BaseId = ReturnBaseId;
		BondEvent.CraftId = ReturnCraftId;
		BondEvent.SiteId = Operation.SiteId;
		BondEvent.PersonnelId = Advance.FirstPersonnelId;
		BondEvent.RelatedPersonnelId = Advance.SecondPersonnelId;
		BondEvent.OperationId = Operation.OperationId;
		BondEvent.BattleId = BattleId;
		BondEvent.PolicyId = FName(TEXT("personnel.squad-bond-field-cadence"));
		BondEvent.PersonnelSquadBondTier = static_cast<int32>(Advance.Tier);
		BondEvent.PersonnelSharedVictories = Advance.SharedVictories;
		BondEvent.PersonnelActionPointBonus = FPersonnelSquadBond::GetActionPointBonus(Advance.Tier);
		BondEvent.PersonnelMoraleBonus = FPersonnelSquadBond::GetMoraleBonus(Advance.Tier);
		BondEvent.Amount = 1;
		BondEvent.Quantity = Advance.SharedVictories;
		BondEvent.bSuccessful = true;
	}
	for (const FTacticalProgressionOutcome& Progression : ProgressionOutcomes)
	{
		FStrategicEvent& Experience = AddEvent(Result, EStrategicEventType::PersonnelExperienceGained, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Experience.BaseId = Progression.BaseId;
		Experience.PersonnelId = Progression.PersonnelId;
		Experience.CraftId = ReturnCraftId;
		Experience.SiteId = Operation.SiteId;
		Experience.OperationId = Operation.OperationId;
		Experience.BattleId = BattleId;
		Experience.RuleId = DebriefMission != nullptr ? DebriefMission->Identity.RuleId : NAME_None;
		Experience.Amount = Progression.ExperienceGained;
		Experience.Quantity = Progression.TotalExperience;
		Experience.bSuccessful = Command.bObjectiveCompleted;
		if (Progression.NewRank > Progression.PreviousRank)
		{
			FStrategicEvent& Promoted = AddEvent(Result, EStrategicEventType::PersonnelPromoted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Promoted.BaseId = Progression.BaseId;
			Promoted.PersonnelId = Progression.PersonnelId;
			Promoted.CraftId = ReturnCraftId;
			Promoted.SiteId = Operation.SiteId;
			Promoted.OperationId = Operation.OperationId;
			Promoted.BattleId = BattleId;
			Promoted.RuleId = DebriefMission != nullptr ? DebriefMission->Identity.RuleId : NAME_None;
			Promoted.Amount = Progression.PreviousRank;
			Promoted.Quantity = Progression.NewRank;
			Promoted.bSuccessful = true;
		}
	}
	for (const FTacticalCommendationOutcome& Outcome : CommendationOutcomes)
	{
		for (const FName CommendationId : Outcome.CommendationIds)
		{
			FStrategicEvent& Awarded = AddEvent(Result, EStrategicEventType::PersonnelCommendationAwarded,
				Transaction.CommandSequence, Transaction.StrategicTime.Utc);
			Awarded.BaseId = Outcome.BaseId;
			Awarded.PersonnelId = Outcome.PersonnelId;
			Awarded.CraftId = ReturnCraftId;
			Awarded.SiteId = Operation.SiteId;
			Awarded.OperationId = Operation.OperationId;
			Awarded.BattleId = BattleId;
			Awarded.RuleId = CommendationId;
			Awarded.bSuccessful = Command.bObjectiveCompleted;
		}
	}
	for (const FTacticalPersonnelOutcome& Outcome : PersonnelOutcomes)
	{
		FStrategicEvent& PersonnelEvent = AddEvent(Result,
			Outcome.bDied ? EStrategicEventType::PersonnelDied : EStrategicEventType::PersonnelInjured,
			Transaction.CommandSequence,
			Transaction.StrategicTime.Utc);
		PersonnelEvent.BaseId = Outcome.BaseId;
		PersonnelEvent.PersonnelId = Outcome.PersonnelId;
		PersonnelEvent.CraftId = ReturnCraftId;
		PersonnelEvent.SiteId = Operation.SiteId;
		PersonnelEvent.OperationId = Operation.OperationId;
		PersonnelEvent.BattleId = BattleId;
		PersonnelEvent.RuleId = TacticalCasualtyCause;
		PersonnelEvent.Amount = -Outcome.AppliedDamage;
		PersonnelEvent.Quantity = Outcome.RemainingHealth;
	}
	if (Command.bObjectiveCompleted)
	{
		FStrategicEvent& Secured = AddEvent(Result, EStrategicEventType::StrategicSiteSecured, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
		Secured.BaseId = ReturnBaseId;
		Secured.CraftId = ReturnCraftId;
		Secured.SiteId = Operation.SiteId;
		Secured.OperationId = Operation.OperationId;
		Secured.Amount = ScoreAward;
	}
	FStrategicEvent& Returning = AddEvent(Result, EStrategicEventType::CraftReturnStarted, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Returning.BaseId = ReturnBaseId;
	Returning.CraftId = ReturnCraftId;
	Returning.SiteId = Operation.SiteId;
	Returning.OperationId = Operation.OperationId;
	Returning.RuleId = ReturnCraftRuleId;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FCreateStrategicContactCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (!Command.ContactId.IsValid() || FindContact(State, Command.ContactId) != nullptr || FindSite(State, Command.ContactId) != nullptr)
	{
		AddError(Result, TEXT("invalid_contact_id"), TEXT("Strategic contact id must be valid and unique across active contacts and sites."));
		return Result;
	}
	const FContactRule* Rule = Rules.Contacts.Find(Command.ContactRuleId);
	if (Rule == nullptr)
	{
		AddError(Result, TEXT("unknown_contact_rule"), FString::Printf(TEXT("Contact rule '%s' is not loaded."), *Command.ContactRuleId.ToString()));
		return Result;
	}
	if (!AreValidCoordinates(Command.OriginLongitudeMilliDegrees, Command.OriginLatitudeMilliDegrees)
		|| !AreValidCoordinates(Command.DestinationLongitudeMilliDegrees, Command.DestinationLatitudeMilliDegrees)
		|| (Command.OriginLongitudeMilliDegrees == Command.DestinationLongitudeMilliDegrees
			&& Command.OriginLatitudeMilliDegrees == Command.DestinationLatitudeMilliDegrees))
	{
		AddError(Result, TEXT("invalid_contact_route"), TEXT("Contact route requires distinct origin and destination coordinates within longitude/latitude bounds."));
		return Result;
	}
	const int64 Distance = ApproximateSurfaceDistanceKilometers(
		Command.OriginLongitudeMilliDegrees,
		Command.OriginLatitudeMilliDegrees,
		Command.DestinationLongitudeMilliDegrees,
		Command.DestinationLatitudeMilliDegrees);
	int64 TotalRouteSeconds = 0;
	if (Distance <= 0 || !ComputeTravelSeconds(Distance, Rule->CruiseSpeedKilometersPerHour, TotalRouteSeconds))
	{
		AddError(Result, TEXT("invalid_contact_route"), TEXT("Contact route cannot be represented by the loaded movement rule."));
		return Result;
	}

	FCampaignState Transaction = State;
	FStrategicContactState& Contact = Transaction.StrategicContacts.AddDefaulted_GetRef();
	Contact.ContactId = Command.ContactId;
	Contact.ContactRuleId = Command.ContactRuleId;
	Contact.Status = EStrategicContactStatus::Hidden;
	Contact.OriginLongitudeMilliDegrees = Command.OriginLongitudeMilliDegrees;
	Contact.OriginLatitudeMilliDegrees = Command.OriginLatitudeMilliDegrees;
	Contact.LongitudeMilliDegrees = Command.OriginLongitudeMilliDegrees;
	Contact.LatitudeMilliDegrees = Command.OriginLatitudeMilliDegrees;
	Contact.DestinationLongitudeMilliDegrees = Command.DestinationLongitudeMilliDegrees;
	Contact.DestinationLatitudeMilliDegrees = Command.DestinationLatitudeMilliDegrees;
	Contact.TotalRouteSeconds = TotalRouteSeconds;
	Contact.CurrentHull = Rule->MaxHull;
	SortStateCollections(Transaction);
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::StrategicContactCreated, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.ContactId = Command.ContactId;
	Event.RuleId = Command.ContactRuleId;
	Event.Quantity = Rule->ThreatRating;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FDispatchCraftCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FCraftState* ExistingCraft = FindCraft(State, Command.CraftId);
	if (ExistingCraft == nullptr)
	{
		AddError(Result, TEXT("unknown_craft"), TEXT("Craft does not exist in the active fleet."));
		return Result;
	}
	const FStrategicContactState* ExistingContact = FindContact(State, Command.ContactId);
	if (ExistingContact == nullptr || ExistingContact->Status == EStrategicContactStatus::Hidden)
	{
		AddError(Result, TEXT("contact_unavailable"), TEXT("Craft can dispatch only to an active detected contact."));
		return Result;
	}
	if (State.BaseAssaults.ContainsByPredicate(
		[&Command](const FBaseAssaultState& Assault) { return Assault.ContactId == Command.ContactId; }))
	{
		AddError(Result, TEXT("base_assault_arrived"), TEXT("This contact has reached its target; resolve the base defense instead."));
		return Result;
	}
	const FCraftRule* CraftRule = Rules.Craft.Find(ExistingCraft->CraftRuleId);
	const FContactRule* ContactRule = Rules.Contacts.Find(ExistingContact->ContactRuleId);
	const FStrategicBaseState* ExistingBase = FindBase(State, ExistingCraft->BaseId);
	if (CraftRule == nullptr || ContactRule == nullptr || ExistingBase == nullptr)
	{
		AddError(Result, TEXT("invalid_interception_rules"), TEXT("Craft, contact, or home-base data required for interception is missing."));
		return Result;
	}
	if (ExistingCraft->Status != ECraftStatus::Grounded)
	{
		AddError(Result, TEXT("craft_unavailable"), TEXT("Only a grounded craft can dispatch to a contact."));
		return Result;
	}
	if (!ExistingCraft->PendingSalvage.IsEmpty())
	{
		AddError(Result, TEXT("salvage_disposition_required"), TEXT("Retain or sell all recovered salvage before dispatching this craft."));
		return Result;
	}
	if (!ExistingCraft->AssignedPilotId.IsValid())
	{
		AddError(Result, TEXT("craft_pilot_missing"), TEXT("Craft requires an assigned available pilot before dispatch."));
		return Result;
	}
	const FPersonnelState* ExistingPilot = FindPersonnel(State, ExistingCraft->AssignedPilotId);
	const FPersonnelRoleRule* PilotRole = ExistingPilot != nullptr ? Rules.PersonnelRoles.Find(ExistingPilot->RoleId) : nullptr;
	if (ExistingPilot == nullptr || PilotRole == nullptr || PilotRole->Category != EPersonnelRoleCategory::Pilot
		|| ExistingPilot->BaseId != ExistingCraft->BaseId || ExistingPilot->Status != EPersonnelStatus::Available)
	{
		AddError(Result, TEXT("pilot_unavailable"), TEXT("Assigned pilot is not available at the craft's base."));
		return Result;
	}
	for (const FGuid& AgentId : ExistingCraft->AssignedAgentIds)
	{
		const FPersonnelState* Agent = FindPersonnel(State, AgentId);
		const FPersonnelRoleRule* AgentRole = Agent != nullptr ? Rules.PersonnelRoles.Find(Agent->RoleId) : nullptr;
		if (Agent == nullptr || AgentRole == nullptr || AgentRole->Category != EPersonnelRoleCategory::FieldAgent
			|| Agent->BaseId != ExistingCraft->BaseId || Agent->Status != EPersonnelStatus::Available)
		{
			AddError(Result, TEXT("agent_unavailable"), TEXT("Every assigned field agent must be available at the craft's base before dispatch."));
			return Result;
		}
	}
	const int64 Distance = ApproximateSurfaceDistanceKilometers(
		ExistingBase->LongitudeMilliDegrees,
		ExistingBase->LatitudeMilliDegrees,
		ExistingContact->LongitudeMilliDegrees,
		ExistingContact->LatitudeMilliDegrees);
	int64 OneWaySeconds = 0;
	int64 RoundTripSeconds = 0;
	int64 FuelNumerator = 0;
	if (!ComputeTravelSeconds(Distance, CraftRule->CruiseSpeedKilometersPerHour, OneWaySeconds)
		|| !TryMultiplyNonNegative(OneWaySeconds, 2, RoundTripSeconds)
		|| !TryMultiplyNonNegative(RoundTripSeconds, CraftRule->FuelBurnPerHour, FuelNumerator)
		|| FuelNumerator > MAX_int64 - 3599LL)
	{
		AddError(Result, TEXT("interception_range_overflow"), TEXT("Interception route exceeds the supported numeric range."));
		return Result;
	}
	const int64 RequiredFuel = (FuelNumerator + 3599LL) / 3600LL;
	if (RequiredFuel <= 0 || RequiredFuel > ExistingCraft->CurrentFuel)
	{
		AddError(Result, TEXT("insufficient_interception_fuel"), FString::Printf(TEXT("Round-trip interception requires %lld fuel units, but craft has %d."), RequiredFuel, ExistingCraft->CurrentFuel));
		return Result;
	}

	FCampaignState Transaction = State;
	FCraftState* Craft = FindCraft(Transaction, Command.CraftId);
	FPersonnelState* Pilot = FindPersonnel(Transaction, ExistingCraft->AssignedPilotId);
	check(Craft != nullptr && Pilot != nullptr);
	Craft->CurrentFuel -= static_cast<int32>(RequiredFuel);
	Craft->Status = ECraftStatus::Intercepting;
	Craft->TargetContactId = Command.ContactId;
	Craft->TargetSiteId.Invalidate();
	Craft->RemainingRouteSeconds = OneWaySeconds;
	Craft->ReservedReturnSeconds = OneWaySeconds;
	Pilot->Status = EPersonnelStatus::Deployed;
	for (const FGuid& AgentId : Craft->AssignedAgentIds)
	{
		FindPersonnel(Transaction, AgentId)->Status = EPersonnelStatus::Deployed;
	}
	++Transaction.CommandSequence;
	FStrategicEvent& Event = AddEvent(Result, EStrategicEventType::CraftDispatched, Transaction.CommandSequence, Transaction.StrategicTime.Utc);
	Event.BaseId = Craft->BaseId;
	Event.CraftId = Craft->CraftId;
	Event.PersonnelId = Pilot->PersonnelId;
	Event.ContactId = Command.ContactId;
	Event.RuleId = ContactRule->Identity.RuleId;
	Event.Amount = -RequiredFuel;
	Event.Quantity = OneWaySeconds > MAX_int32 ? MAX_int32 : static_cast<int32>(OneWaySeconds);
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FWithdrawInterceptionCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	const FInterceptionWithdrawalEvaluation Evaluation =
		EvaluateInterceptionWithdrawal(State, Rules, Command);
	if (!Evaluation.bCanExecute)
	{
		AddError(Result, Evaluation.UnavailableReasonCode, Evaluation.UnavailableReason);
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicContacts(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result))
	{
		return Result;
	}

	FCampaignState Transaction = State;
	SortStateCollections(Transaction);
	FStrategicContactState* Contact = FindContact(Transaction, Command.ContactId);
	check(Contact != nullptr);
	const TArray<FGuid>& ParticipantIds = Evaluation.WithdrawingCraftIds;

	++Transaction.CommandSequence;
	if (Evaluation.ContactRouteDelaySeconds > 0)
	{
		check(Evaluation.ContactRouteDelaySeconds <= Contact->ElapsedRouteSeconds);
		Contact->ElapsedRouteSeconds -= Evaluation.ContactRouteDelaySeconds;
		ComputeContactPosition(
			*Contact,
			Contact->ElapsedRouteSeconds,
			Contact->LongitudeMilliDegrees,
			Contact->LatitudeMilliDegrees);
	}
	Contact->Status = Evaluation.RemainingCraftCount == 0
		? EStrategicContactStatus::Detected
		: EStrategicContactStatus::Engaged;
	FStrategicEvent& Withdrawn = AddEvent(
		Result,
		EStrategicEventType::InterceptionWithdrawn,
		Transaction.CommandSequence,
		Transaction.StrategicTime.Utc);
	Withdrawn.ContactId = Command.ContactId;
	Withdrawn.RuleId = Contact->ContactRuleId;
	Withdrawn.PolicyId = Evaluation.PolicyId;
	Withdrawn.CraftId = Evaluation.PriorityCraftId;
	Withdrawn.Quantity = ParticipantIds.Num();
	Withdrawn.Amount = Evaluation.RemainingCraftCount;
	Withdrawn.ContactRouteDelaySeconds = Evaluation.ContactRouteDelaySeconds;
	Withdrawn.bSuccessful = Evaluation.RemainingCraftCount == 0;
	for (const FGuid& CraftId : ParticipantIds)
	{
		FCraftState* Craft = FindCraft(Transaction, CraftId);
		check(Craft != nullptr);
		const FGuid BaseId = Craft->BaseId;
		const FGuid PilotId = Craft->AssignedPilotId;
		const FName CraftRuleId = Craft->CraftRuleId;
		Craft->Status = ECraftStatus::Returning;
		Craft->TargetContactId.Invalidate();
		Craft->RemainingRouteSeconds = Craft->ReservedReturnSeconds;

		FStrategicEvent& Returning = AddEvent(
			Result,
			EStrategicEventType::CraftReturnStarted,
			Transaction.CommandSequence,
			Transaction.StrategicTime.Utc);
		Returning.BaseId = BaseId;
		Returning.CraftId = CraftId;
		Returning.PersonnelId = PilotId;
		Returning.ContactId = Command.ContactId;
		Returning.RuleId = CraftRuleId;
		Returning.PolicyId = Evaluation.PolicyId;
		Returning.ContactRouteDelaySeconds = Evaluation.ContactRouteDelaySeconds;
	}
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FResolveInterceptionRoundCommand& Command)
{
	return Execute(State, Rules, FStrategicSimulationConfig(), Command);
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FResolveInterceptionRoundCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	if (!ValidateSequence(State, Command.ExpectedSequence, Result))
	{
		return Result;
	}
	if (Config.InterceptionRoundSeconds <= 0 || Config.InterceptionRoundSeconds > 3600
		|| Config.WreckageSiteLifetimeHours <= 0
		|| Config.WreckageSiteLifetimeHours > MAX_int64 / 3600LL)
	{
		AddError(Result, TEXT("invalid_simulation_config"), TEXT("Interception round and wreckage lifetime settings are outside supported limits."));
		return Result;
	}
	if (!ValidateAdversaryConfig(Config, Result))
	{
		return Result;
	}
	const FInterceptionPosturePolicy PosturePolicy = GetInterceptionPosturePolicy(Command.Posture);
	if (!PosturePolicy.bValid)
	{
		AddError(Result, TEXT("invalid_interception_posture"),
			TEXT("Interception round selected an unsupported engagement posture."));
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Interception combat cannot continue after the campaign has concluded."));
		return Result;
	}
	const FStrategicContactState* ExistingContact = FindContact(State, Command.ContactId);
	if (ExistingContact == nullptr)
	{
		AddError(Result, TEXT("unknown_contact"), TEXT("Strategic contact does not exist."));
		return Result;
	}
	if (ExistingContact->Status != EStrategicContactStatus::Engaged)
	{
		AddError(Result, TEXT("contact_not_engaged"), TEXT("Interception rounds require at least one craft on station at an engaged contact."));
		return Result;
	}
	const FContactRule* ContactRule = Rules.Contacts.Find(ExistingContact->ContactRuleId);
	if (ContactRule == nullptr || ContactRule->AttackAccuracy <= 0 || ContactRule->AttackAccuracy > 100
		|| ContactRule->AttackDamage <= 0 || ContactRule->AttackIntervalSeconds <= 0)
	{
		AddError(Result, TEXT("invalid_contact_rule"), TEXT("Contact combat profile is missing or invalid."));
		return Result;
	}
	if (!ValidatePersonnelState(State, Rules, Result)
		|| !ValidateCraftState(State, Rules, Result)
		|| !ValidateStrategicContacts(State, Rules, Result)
		|| !ValidateStrategicSites(State, Rules, Result)
		|| !ValidateTacticalOperations(State, Result)
		|| !ValidateTacticalBattles(State, Rules, Result)
		|| !ValidateAdversaryState(State, Rules, Config, Result))
	{
		return Result;
	}
	if (FindSite(State, Command.ContactId) != nullptr)
	{
		AddError(Result, TEXT("site_identity_conflict"), TEXT("Contact identity conflicts with an active strategic site."));
		return Result;
	}
	if (ExistingContact->CompletedCombatRounds == MAX_int32)
	{
		AddError(Result, TEXT("interception_round_overflow"), TEXT("Interception cannot accept another combat round."));
		return Result;
	}
	const FInterceptionCoordinationPolicy CoordinationPolicy =
		EvaluateInterceptionCoordination(State, Command.ContactId);
	if (!CoordinationPolicy.bValid)
	{
		AddError(Result, TEXT("contact_not_engaged"), TEXT("Engaged contact has no on-station craft."));
		return Result;
	}
	const FInterceptionContactManeuverPolicy ContactManeuver =
		EvaluateInterceptionContactManeuver(State, Rules, Command.ContactId);
	if (!ContactManeuver.bValid)
	{
		AddError(Result, TEXT("invalid_contact_rule"),
			TEXT("Contact maneuver cannot be derived from the current combat profile."));
		return Result;
	}
	int64 PotentialScore = 0;
	if (!TryAdd(State.CampaignScore, ContactRule->ScoreValue, PotentialScore))
	{
		AddError(Result, TEXT("campaign_score_overflow"), TEXT("Contact score would exceed the campaign numeric range."));
		return Result;
	}

	int64 MaximumDraws = 2;
	for (const FCraftState& Craft : State.Craft)
	{
		if (Craft.Status != ECraftStatus::Airborne || Craft.TargetContactId != Command.ContactId)
		{
			continue;
		}
		for (const FCraftWeaponState& WeaponState : Craft.WeaponStates)
		{
			const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
			if (Weapon == nullptr || !IsValidCraftWeaponRule(*Weapon, Rules))
			{
				AddError(Result, TEXT("invalid_craft_weapon_rule"), TEXT("Interception craft references an invalid weapon rule."));
				return Result;
			}
			if (WeaponState.RemainingCooldownSeconds <= Config.InterceptionRoundSeconds && WeaponState.Ammunition > 0)
			{
				const int64 MountCount = CountEquippedItem(Craft.EquipmentItems, WeaponState.WeaponItemId);
				int64 SalvoCapacity = 0;
				if (!TryMultiplyNonNegative(MountCount, Weapon->SalvoSize, SalvoCapacity)
					|| !TryAdd(MaximumDraws, FMath::Min<int64>(WeaponState.Ammunition, SalvoCapacity), MaximumDraws))
				{
					AddError(Result, TEXT("random_draw_overflow"), TEXT("Interception weapon fire exceeds the deterministic random-stream range."));
					return Result;
				}
			}
		}
	}
	if (WouldExhaustDeterministicRandomStream(State.SimulationRandom, MaximumDraws))
	{
		AddError(Result, TEXT("random_draw_overflow"), TEXT("Interception round exceeds the deterministic random-stream draw range."));
		return Result;
	}

	FCampaignState Transaction = State;
	SortStateCollections(Transaction);
	FStrategicContactState* Contact = FindContact(Transaction, Command.ContactId);
	check(Contact != nullptr);
	TArray<FGuid> ParticipantIds;
	for (const FCraftState& Craft : Transaction.Craft)
	{
		if (Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Command.ContactId)
		{
			ParticipantIds.Add(Craft.CraftId);
		}
	}
	ParticipantIds.Sort(
		[](const FGuid& Left, const FGuid& Right)
		{
			return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
		});
	if (ParticipantIds.IsEmpty())
	{
		AddError(Result, TEXT("contact_not_engaged"), TEXT("Engaged contact has no on-station craft."));
		return Result;
	}
	for (const FGuid& CraftId : ParticipantIds)
	{
		FCraftState* Craft = FindCraft(Transaction, CraftId);
		check(Craft != nullptr);
		for (FCraftWeaponState& WeaponState : Craft->WeaponStates)
		{
			WeaponState.RemainingCooldownSeconds -= FMath::Min<int64>(
				WeaponState.RemainingCooldownSeconds,
				Config.InterceptionRoundSeconds);
		}
	}
	Contact->RemainingAttackCooldownSeconds -= FMath::Min<int64>(
		Contact->RemainingAttackCooldownSeconds,
		Config.InterceptionRoundSeconds);

	const int64 NextSequence = Transaction.CommandSequence + 1;
	for (const FGuid& CraftId : ParticipantIds)
	{
		FCraftState* Craft = FindCraft(Transaction, CraftId);
		check(Craft != nullptr);
		const FPersonnelState* Pilot = FindPersonnel(Transaction, Craft->AssignedPilotId);
		check(Pilot != nullptr);
		for (FCraftWeaponState& WeaponState : Craft->WeaponStates)
		{
			if (Contact->CurrentHull <= 0 || WeaponState.RemainingCooldownSeconds > 0 || WeaponState.Ammunition <= 0)
			{
				continue;
			}
			const FItemRule& Weapon = Rules.Items.FindChecked(WeaponState.WeaponItemId);
			const int32 MountCount = CountEquippedItem(Craft->EquipmentItems, WeaponState.WeaponItemId);
			const int64 SalvoCapacity = static_cast<int64>(Weapon.SalvoSize) * MountCount;
			const int32 Shots = static_cast<int32>(FMath::Min<int64>(WeaponState.Ammunition, SalvoCapacity));
			if (Shots <= 0)
			{
				continue;
			}
			const int32 HitChance = FMath::Clamp(
				Weapon.InterceptionAccuracy + (Pilot->Accuracy - 50) / 2
					- (ContactRule->ThreatRating - 1) * 3
					+ PosturePolicy.OutgoingAccuracyModifier
					+ CoordinationPolicy.OutgoingAccuracyModifier
					+ ContactManeuver.OutgoingAccuracyModifier,
				5,
				100);
			int32 Hits = 0;
			for (int32 Shot = 0; Shot < Shots; ++Shot)
			{
				Hits += Transaction.SimulationRandom.NextIntInclusive(1, 100) <= HitChance ? 1 : 0;
			}
			WeaponState.Ammunition -= Shots;
			WeaponState.RemainingCooldownSeconds = Weapon.FireIntervalSeconds;
			const int64 PotentialDamage = static_cast<int64>(Hits) * Weapon.InterceptionDamage;
			const int32 AppliedDamage = static_cast<int32>(FMath::Min<int64>(Contact->CurrentHull, PotentialDamage));
			Contact->CurrentHull -= AppliedDamage;
			FStrategicEvent& Fired = AddEvent(Result, EStrategicEventType::CraftWeaponFired, NextSequence, Transaction.StrategicTime.Utc);
			Fired.BaseId = Craft->BaseId;
			Fired.CraftId = Craft->CraftId;
			Fired.PersonnelId = Pilot->PersonnelId;
			Fired.ContactId = Contact->ContactId;
			Fired.RuleId = Weapon.Identity.RuleId;
			Fired.PolicyId = CoordinationPolicy.PolicyId;
			Fired.OutgoingAccuracyModifier = CoordinationPolicy.OutgoingAccuracyModifier;
			Fired.IncomingAccuracyModifier = CoordinationPolicy.IncomingAccuracyModifier;
			Fired.ContactManeuverPolicyId = ContactManeuver.PolicyId;
			Fired.ContactManeuverOutgoingAccuracyModifier = ContactManeuver.OutgoingAccuracyModifier;
			Fired.ContactManeuverIncomingAccuracyModifier = ContactManeuver.IncomingAccuracyModifier;
			Fired.Amount = AppliedDamage;
			Fired.Quantity = Shots;
			Fired.HitChance = HitChance;
			Fired.Roll = Hits;
			Fired.bSuccessful = Hits > 0;
			if (AppliedDamage > 0)
			{
				FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::StrategicContactDamaged, NextSequence, Transaction.StrategicTime.Utc);
				Damaged.CraftId = Craft->CraftId;
				Damaged.ContactId = Contact->ContactId;
				Damaged.RuleId = Contact->ContactRuleId;
				Damaged.Amount = -AppliedDamage;
				Damaged.Quantity = Contact->CurrentHull;
			}
		}
		if (Contact->CurrentHull <= 0)
		{
			break;
		}
	}

	++Contact->CompletedCombatRounds;
	const int32 CompletedRounds = Contact->CompletedCombatRounds;
	if (Contact->CurrentHull <= 0)
	{
		const FGuid ContactId = Contact->ContactId;
		const FName ContactRuleId = Contact->ContactRuleId;
		const int32 ContactLongitude = Contact->LongitudeMilliDegrees;
		const int32 ContactLatitude = Contact->LatitudeMilliDegrees;
		const FAdversaryMissionState* ExistingMission = FindAdversaryMission(Transaction, ContactId);
		const bool bHasAdversaryMission = ExistingMission != nullptr;
		const FGuid MissionId = ExistingMission != nullptr ? ExistingMission->MissionId : FGuid();
		const FName MissionRuleId = ExistingMission != nullptr
			? ExistingMission->MissionRuleId
			: NAME_None;
		Transaction.CampaignScore = PotentialScore;
		FStrategicSiteState& Site = Transaction.StrategicSites.AddDefaulted_GetRef();
		Site.SiteId = ContactId;
		Site.Type = EStrategicSiteType::Wreckage;
		Site.SourceContactRuleId = ContactRuleId;
		Site.LongitudeMilliDegrees = ContactLongitude;
		Site.LatitudeMilliDegrees = ContactLatitude;
		Site.ThreatRating = ContactRule->ThreatRating;
		Site.RemainingLifetimeSeconds = static_cast<int64>(Config.WreckageSiteLifetimeHours) * 3600LL;

		FStrategicEvent& Destroyed = AddEvent(Result, EStrategicEventType::StrategicContactDestroyed, NextSequence, Transaction.StrategicTime.Utc);
		Destroyed.ContactId = ContactId;
		Destroyed.RuleId = ContactRuleId;
		Destroyed.Amount = ContactRule->ScoreValue;
		FStrategicEvent& Won = AddEvent(Result, EStrategicEventType::InterceptionWon, NextSequence, Transaction.StrategicTime.Utc);
		Won.ContactId = ContactId;
		Won.RuleId = ContactRuleId;
		Won.Amount = ContactRule->ScoreValue;
		FStrategicEvent& SiteCreated = AddEvent(Result, EStrategicEventType::StrategicSiteCreated, NextSequence, Transaction.StrategicTime.Utc);
		SiteCreated.SiteId = Site.SiteId;
		SiteCreated.ContactId = ContactId;
		SiteCreated.RuleId = ContactRuleId;
		SiteCreated.Quantity = Site.ThreatRating;
		if (!ApplyAdversaryMissionThwarted(Transaction, Rules, Config, ContactId, Result, NextSequence, Transaction.StrategicTime.Utc))
		{
			Result.Events.Reset();
			AddError(Result, TEXT("adversary_state_overflow"), TEXT("Adversary mission resolution exceeded the campaign numeric range."));
			return Result;
		}
		if (bHasAdversaryMission && Transaction.Outcome == ECampaignOutcome::Ongoing
			&& Transaction.NextAdversaryMissionSeconds > 0)
		{
			int64 AftershockSeconds = 0;
			if (!FStrategicCommandService::CalculateInterceptionAftershockSeconds(
				ContactRule->ThreatRating, Config, AftershockSeconds))
			{
				Result.Events.Reset();
				AddError(Result, TEXT("adversary_state_overflow"),
					TEXT("Interception aftershock exceeded the campaign numeric range."));
				return Result;
			}
			if (AftershockSeconds > 0)
			{
				const int64 PreviousAdversaryMissionSeconds = Transaction.NextAdversaryMissionSeconds;
				if (!TryAdd(
					PreviousAdversaryMissionSeconds,
					AftershockSeconds,
					Transaction.NextAdversaryMissionSeconds))
				{
					Result.Events.Reset();
					AddError(Result, TEXT("adversary_state_overflow"),
						TEXT("Interception aftershock exceeded the campaign numeric range."));
					return Result;
				}
				FStrategicEvent& Aftershock = AddEvent(
					Result,
					EStrategicEventType::InterceptionAftershockApplied,
					NextSequence,
					Transaction.StrategicTime.Utc);
				Aftershock.MissionId = MissionId;
				Aftershock.ContactId = ContactId;
				Aftershock.RuleId = MissionRuleId;
				Aftershock.Amount = AftershockSeconds;
				Aftershock.Quantity = ContactRule->ThreatRating;
				Aftershock.PreviousAdversaryMissionSeconds = PreviousAdversaryMissionSeconds;
				Aftershock.AdversaryMissionDelaySeconds = AftershockSeconds;
				Aftershock.NextAdversaryMissionSeconds = Transaction.NextAdversaryMissionSeconds;
			}
		}
		for (FCraftState& Craft : Transaction.Craft)
		{
			if (Craft.TargetContactId != ContactId
				|| (Craft.Status != ECraftStatus::Airborne && Craft.Status != ECraftStatus::Intercepting))
			{
				continue;
			}
			Craft.Status = ECraftStatus::Returning;
			Craft.TargetContactId.Invalidate();
			Craft.RemainingRouteSeconds = Craft.ReservedReturnSeconds;
			FStrategicEvent& Returning = AddEvent(Result, EStrategicEventType::CraftReturnStarted, NextSequence, Transaction.StrategicTime.Utc);
			Returning.BaseId = Craft.BaseId;
			Returning.CraftId = Craft.CraftId;
			Returning.ContactId = ContactId;
			Returning.RuleId = Craft.CraftRuleId;
		}
		Transaction.StrategicContacts.RemoveAll(
			[&ContactId](const FStrategicContactState& Entry) { return Entry.ContactId == ContactId; });
	}
	else if (Contact->RemainingAttackCooldownSeconds == 0)
	{
		const int32 TargetIndex = Transaction.SimulationRandom.NextIntInclusive(0, ParticipantIds.Num() - 1);
		const FGuid TargetCraftId = ParticipantIds[TargetIndex];
		FCraftState* TargetCraft = FindCraft(Transaction, TargetCraftId);
		check(TargetCraft != nullptr);
		int64 DefensivePower = 0;
		for (const FName ItemId : TargetCraft->EquipmentItems)
		{
			const FItemRule& Item = Rules.Items.FindChecked(ItemId);
			if (Item.Category == FName(TEXT("craft-defense")))
			{
				DefensivePower = FMath::Min<int64>(100, DefensivePower + Item.Power);
			}
		}
		const int32 HitChance = FMath::Clamp(
			ContactRule->AttackAccuracy - static_cast<int32>(DefensivePower / 2)
				+ PosturePolicy.IncomingAccuracyModifier
				+ CoordinationPolicy.IncomingAccuracyModifier
				+ ContactManeuver.IncomingAccuracyModifier,
			5,
			100);
		const int32 AttackRoll = Transaction.SimulationRandom.NextIntInclusive(1, 100);
		const bool bHit = AttackRoll <= HitChance;
		Contact->RemainingAttackCooldownSeconds = ContactRule->AttackIntervalSeconds;
		const int32 AppliedDamage = bHit
			? FMath::Min(TargetCraft->CurrentHull, ContactRule->AttackDamage)
			: 0;
		FStrategicEvent& ContactFired = AddEvent(
			Result,
			EStrategicEventType::StrategicContactWeaponFired,
			NextSequence,
			Transaction.StrategicTime.Utc);
		ContactFired.CraftId = TargetCraft->CraftId;
		ContactFired.ContactId = Contact->ContactId;
		ContactFired.RuleId = Contact->ContactRuleId;
		ContactFired.PolicyId = CoordinationPolicy.PolicyId;
		ContactFired.OutgoingAccuracyModifier = CoordinationPolicy.OutgoingAccuracyModifier;
		ContactFired.IncomingAccuracyModifier = CoordinationPolicy.IncomingAccuracyModifier;
		ContactFired.ContactManeuverPolicyId = ContactManeuver.PolicyId;
		ContactFired.ContactManeuverOutgoingAccuracyModifier = ContactManeuver.OutgoingAccuracyModifier;
		ContactFired.ContactManeuverIncomingAccuracyModifier = ContactManeuver.IncomingAccuracyModifier;
		ContactFired.HitChance = HitChance;
		ContactFired.Roll = AttackRoll;
		ContactFired.bSuccessful = bHit;
		ContactFired.Amount = -AppliedDamage;
		ContactFired.Quantity = 1;
		if (bHit)
		{
			TargetCraft->CurrentHull -= AppliedDamage;
			FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::CraftDamaged, NextSequence, Transaction.StrategicTime.Utc);
			Damaged.BaseId = TargetCraft->BaseId;
			Damaged.CraftId = TargetCraft->CraftId;
			Damaged.ContactId = Contact->ContactId;
			Damaged.RuleId = Contact->ContactRuleId;
			Damaged.Amount = -AppliedDamage;
			Damaged.Quantity = TargetCraft->CurrentHull;
			Damaged.HitChance = HitChance;
			Damaged.Roll = AttackRoll;
			Damaged.bSuccessful = true;
			if (TargetCraft->CurrentHull == 0)
			{
				const FGuid DestroyedCraftId = TargetCraft->CraftId;
				const FGuid PilotId = TargetCraft->AssignedPilotId;
				TArray<FGuid> CrewIds = TargetCraft->AssignedAgentIds;
				CrewIds.Insert(PilotId, 0);
				const FGuid BaseId = TargetCraft->BaseId;
				const FName CraftRuleId = TargetCraft->CraftRuleId;
				const FName LossCauseId(TEXT("cause.interception-loss"));
				if (CrewIds.ContainsByPredicate(
					[&Transaction](const FGuid& PersonnelId) { return FindPersonnel(Transaction, PersonnelId) == nullptr; }))
				{
					AddError(Result, TEXT("invalid_craft_crew"), TEXT("Destroyed craft has an incomplete persisted crew roster."));
					return Result;
				}
				for (const FGuid& CrewId : CrewIds)
				{
					const FPersonnelState& Crew = *FindPersonnel(Transaction, CrewId);
					FMemorialRecord& Record = Transaction.Memorial.AddDefaulted_GetRef();
					Record.PersonnelId = Crew.PersonnelId;
					Record.DisplayName = Crew.DisplayName;
					Record.RoleId = Crew.RoleId;
					Record.Rank = Crew.Rank;
					Record.Missions = Crew.Missions;
					Record.Kills = Crew.Kills;
					Record.DoctrineSelections = Crew.DoctrineSelections;
					Record.Commendations = Crew.Commendations;
					Record.StewardshipToursCompleted = Crew.StewardshipToursCompleted;
					Record.DeathUtc = Transaction.StrategicTime.Utc;
					Record.CauseId = LossCauseId;
				}
				Transaction.Personnel.RemoveAll(
					[&CrewIds](const FPersonnelState& Entry) { return CrewIds.Contains(Entry.PersonnelId); });
				Transaction.Craft.RemoveAll(
					[&DestroyedCraftId](const FCraftState& Entry) { return Entry.CraftId == DestroyedCraftId; });
				Transaction.TacticalOperations.RemoveAll(
					[&DestroyedCraftId](const FTacticalOperationState& Entry) { return Entry.CraftId == DestroyedCraftId; });
				Transaction.TacticalBattles.RemoveAll(
					[&Transaction](const FTacticalBattleState& Battle) { return !Transaction.TacticalOperations.ContainsByPredicate(
						[&Battle](const FTacticalOperationState& Operation) { return Operation.OperationId == Battle.OperationId; }); });
				FStrategicEvent& CraftDestroyed = AddEvent(Result, EStrategicEventType::CraftDestroyed, NextSequence, Transaction.StrategicTime.Utc);
				CraftDestroyed.BaseId = BaseId;
				CraftDestroyed.CraftId = DestroyedCraftId;
				CraftDestroyed.PersonnelId = PilotId;
				CraftDestroyed.ContactId = Contact->ContactId;
				CraftDestroyed.RuleId = CraftRuleId;
				CraftDestroyed.Quantity = CrewIds.Num();
				for (const FGuid& CrewId : CrewIds)
				{
					FStrategicEvent& PersonnelDied = AddEvent(Result, EStrategicEventType::PersonnelDied, NextSequence, Transaction.StrategicTime.Utc);
					PersonnelDied.BaseId = BaseId;
					PersonnelDied.PersonnelId = CrewId;
					PersonnelDied.CraftId = DestroyedCraftId;
					PersonnelDied.ContactId = Contact->ContactId;
					PersonnelDied.RuleId = LossCauseId;
				}
				const bool bAnyOnStation = Transaction.Craft.ContainsByPredicate(
					[&Command](const FCraftState& Craft)
					{
						return Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Command.ContactId;
					});
				if (!bAnyOnStation)
				{
					Contact->Status = EStrategicContactStatus::Detected;
					FStrategicEvent& Defeated = AddEvent(Result, EStrategicEventType::InterceptionDefeated, NextSequence, Transaction.StrategicTime.Utc);
					Defeated.ContactId = Contact->ContactId;
					Defeated.RuleId = Contact->ContactRuleId;
					Result.bDecisionPause = true;
				}
			}
		}
	}

	SortStateCollections(Transaction);
	Transaction.CommandSequence = NextSequence;
	FStrategicEvent& Round = AddEvent(Result, EStrategicEventType::InterceptionRoundResolved, NextSequence, Transaction.StrategicTime.Utc);
	Round.ContactId = Command.ContactId;
	Round.RuleId = PosturePolicy.PolicyId;
	Round.PolicyId = CoordinationPolicy.PolicyId;
	Round.OutgoingAccuracyModifier = CoordinationPolicy.OutgoingAccuracyModifier;
	Round.IncomingAccuracyModifier = CoordinationPolicy.IncomingAccuracyModifier;
	Round.ContactManeuverPolicyId = ContactManeuver.PolicyId;
	Round.ContactManeuverOutgoingAccuracyModifier = ContactManeuver.OutgoingAccuracyModifier;
	Round.ContactManeuverIncomingAccuracyModifier = ContactManeuver.IncomingAccuracyModifier;
	Round.Quantity = CompletedRounds;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = Result.bDecisionPause || Result.HasEvent(EStrategicEventType::InterceptionWon);
	return Result;
}

FStrategicCommandResult FStrategicCommandService::Execute(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FResolveBaseAssaultCommand& Command)
{
	using namespace StrategicCommandServicePrivate;

	FStrategicCommandResult Result;
	const FBaseAssaultEvaluation Evaluation = EvaluateBaseAssault(State, Rules, Config, Command);
	if (!Evaluation.bAllowed)
	{
		Result.Diagnostics = Evaluation.Diagnostics;
		return Result;
	}
	if (State.Outcome != ECampaignOutcome::Ongoing)
	{
		AddError(Result, TEXT("campaign_concluded"), TEXT("Base defense cannot resolve after the campaign has concluded."));
		return Result;
	}
	if (!ValidateAdversaryConfig(Config, Result)
		|| !ValidateFacilityState(State, Rules, Result)
		|| !ValidateStrategicContacts(State, Rules, Result)
		|| !ValidateAdversaryState(State, Rules, Config, Result))
	{
		return Result;
	}

	const FBaseAssaultState* ExistingAssault = FindBaseAssault(State, Command.AssaultId);
	check(ExistingAssault != nullptr);
	const FAdversaryMissionState* ExistingMission = FindAdversaryMissionById(State, ExistingAssault->MissionId);
	const FStrategicContactState* ExistingContact = FindContact(State, ExistingAssault->ContactId);
	const FStrategicBaseState* ExistingBase = FindBase(State, ExistingAssault->BaseId);
	check(ExistingMission != nullptr && ExistingContact != nullptr && ExistingBase != nullptr);
	const FAdversaryMissionRule* MissionRule = Rules.AdversaryMissions.Find(ExistingMission->MissionRuleId);
	const FContactRule* ContactRule = Rules.Contacts.Find(ExistingContact->ContactRuleId);
	if (MissionRule == nullptr || ContactRule == nullptr)
	{
		AddError(Result, TEXT("invalid_base_assault"), TEXT("Base assault references an unloaded mission or contact rule."));
		return Result;
	}
	int64 PotentialScore = 0;
	if (!TryAdd(State.CampaignScore, ContactRule->ScoreValue, PotentialScore))
	{
		AddError(Result, TEXT("campaign_score_overflow"), TEXT("Base-defense score would exceed the campaign numeric range."));
		return Result;
	}

	FBaseDefenseVolleyPlan Volley;
	if (!BuildBaseDefenseVolleyPlan(*ExistingBase, Rules, Config, Command.FireDoctrine, Volley, Result))
	{
		return Result;
	}
	TArray<FGuid> DamageCandidateIds;
	for (const FBaseFacilityState& Facility : ExistingBase->Facilities)
	{
		const FFacilityRule* FacilityRule = Rules.Facilities.Find(Facility.FacilityId);
		check(FacilityRule != nullptr);
		if (Facility.Damage < FacilityRule->MaxIntegrity)
		{
			DamageCandidateIds.Add(Facility.InstanceId);
		}
	}
	auto SortGuids = [](TArray<FGuid>& Values)
	{
		Values.Sort([](const FGuid& Left, const FGuid& Right)
		{
			return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
		});
	};
	SortGuids(DamageCandidateIds);
	const int64 MaximumDraws = static_cast<int64>(Volley.ReadyShots.Num())
		+ FMath::Min<int64>(DamageCandidateIds.Num(), MissionRule->BaseFacilitiesHit);
	if (WouldExhaustDeterministicRandomStream(State.SimulationRandom, MaximumDraws))
	{
		AddError(Result, TEXT("random_draw_overflow"), TEXT("Base defense exceeds the deterministic random-stream draw range."));
		return Result;
	}

	FCampaignState Transaction = State;
	SortStateCollections(Transaction);
	FBaseAssaultState* Assault = FindBaseAssault(Transaction, Command.AssaultId);
	check(Assault != nullptr);
	const FGuid AssaultId = Assault->AssaultId;
	const FGuid MissionId = Assault->MissionId;
	const FGuid ContactId = Assault->ContactId;
	const FGuid BaseId = Assault->BaseId;
	const FName MissionRuleId = ExistingMission->MissionRuleId;
	const FName ContactRuleId = ExistingContact->ContactRuleId;
	const FName RegionId = ExistingBase->RegionId;
	const FName DoctrinePolicyId = Evaluation.PolicyId;
	FStrategicContactState* Contact = FindContact(Transaction, ContactId);
	FStrategicBaseState* Base = FindBase(Transaction, BaseId);
	check(Contact != nullptr && Base != nullptr);
	const int64 NextSequence = Transaction.CommandSequence + 1;
	if (Evaluation.FundingCost > 0)
	{
		check(Command.FireDoctrine == EBaseDefenseFireDoctrine::GridOvercharge);
		check(Transaction.Funds >= Evaluation.FundingCost);
		Transaction.Funds -= Evaluation.FundingCost;
		FStrategicEvent& Overcharged = AddEvent(Result,
			EStrategicEventType::BaseDefenseGridOvercharged,
			NextSequence, Transaction.StrategicTime.Utc);
		Overcharged.BaseId = BaseId;
		Overcharged.ContactId = ContactId;
		Overcharged.MissionId = MissionId;
		Overcharged.AssaultId = AssaultId;
		Overcharged.RuleId = ContactRuleId;
		Overcharged.PolicyId = DoctrinePolicyId;
		Overcharged.Amount = -Evaluation.FundingCost;
		Overcharged.Quantity = ContactRule->ThreatRating;
	}

	int32 FiredBatteryCount = 0;
	for (const FBaseDefenseShotPlan& Shot : Volley.ReadyShots)
	{
		if (Contact->CurrentHull <= 0)
		{
			break;
		}
		const FBaseFacilityState* Facility = FindFacility(*Base, Shot.FacilityInstanceId);
		check(Facility != nullptr);
		if (!Shot.SupplyItemId.IsNone())
		{
			if (!TryAdjustInventory(*Base, Shot.SupplyItemId, -Shot.SupplyQuantity))
			{
				Result.Events.Reset();
				AddError(Result, TEXT("base_defense_supply_transaction_failed"), FString::Printf(
					TEXT("Defense supply '%s' could not be consumed atomically."),
					*Shot.SupplyItemId.ToString()));
				return Result;
			}
			FStrategicEvent& Consumed = AddEvent(Result,
				EStrategicEventType::BaseDefenseSupplyConsumed,
				NextSequence, Transaction.StrategicTime.Utc);
			Consumed.BaseId = BaseId;
			Consumed.FacilityInstanceId = Shot.FacilityInstanceId;
			Consumed.ContactId = ContactId;
			Consumed.MissionId = MissionId;
			Consumed.AssaultId = AssaultId;
			Consumed.RuleId = Shot.SupplyItemId;
			Consumed.PolicyId = DoctrinePolicyId;
			Consumed.Amount = -Shot.SupplyQuantity;
			Consumed.Quantity = Shot.SupplyQuantity;
		}
		const int32 Roll = Transaction.SimulationRandom.NextIntInclusive(1, 100);
		const bool bHit = Roll <= Shot.Accuracy;
		const int32 AppliedDamage = bHit ? FMath::Min(Contact->CurrentHull, Shot.Damage) : 0;
		Contact->CurrentHull -= AppliedDamage;
		++FiredBatteryCount;
		FStrategicEvent& Fired = AddEvent(Result, EStrategicEventType::BaseDefenseWeaponFired,
			NextSequence, Transaction.StrategicTime.Utc);
		Fired.BaseId = BaseId;
		Fired.FacilityInstanceId = Shot.FacilityInstanceId;
		Fired.ContactId = ContactId;
		Fired.MissionId = MissionId;
		Fired.AssaultId = AssaultId;
		Fired.RuleId = Shot.FacilityId;
		Fired.PolicyId = DoctrinePolicyId;
		Fired.HitChance = Shot.Accuracy;
		Fired.Roll = Roll;
		Fired.bSuccessful = bHit;
		Fired.Amount = AppliedDamage;
		Fired.Quantity = Contact->CurrentHull;
		if (AppliedDamage > 0)
		{
			FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::StrategicContactDamaged,
				NextSequence, Transaction.StrategicTime.Utc);
			Damaged.BaseId = BaseId;
			Damaged.FacilityInstanceId = Shot.FacilityInstanceId;
			Damaged.ContactId = ContactId;
			Damaged.MissionId = MissionId;
			Damaged.AssaultId = AssaultId;
			Damaged.RuleId = ContactRuleId;
			Damaged.PolicyId = DoctrinePolicyId;
			Damaged.Amount = -AppliedDamage;
			Damaged.Quantity = Contact->CurrentHull;
		}
	}

	if (Contact->CurrentHull <= 0)
	{
		Transaction.CampaignScore = PotentialScore;
		FStrategicEvent& Destroyed = AddEvent(Result, EStrategicEventType::StrategicContactDestroyed,
			NextSequence, Transaction.StrategicTime.Utc);
		Destroyed.BaseId = BaseId;
		Destroyed.ContactId = ContactId;
		Destroyed.MissionId = MissionId;
		Destroyed.AssaultId = AssaultId;
		Destroyed.RuleId = ContactRuleId;
		Destroyed.PolicyId = DoctrinePolicyId;
		Destroyed.Amount = ContactRule->ScoreValue;
		FStrategicEvent& Repelled = AddEvent(Result, EStrategicEventType::BaseAssaultRepelled,
			NextSequence, Transaction.StrategicTime.Utc);
		Repelled.BaseId = BaseId;
		Repelled.ContactId = ContactId;
		Repelled.MissionId = MissionId;
		Repelled.AssaultId = AssaultId;
		Repelled.RuleId = MissionRuleId;
		Repelled.PolicyId = DoctrinePolicyId;
		Repelled.RegionId = RegionId;
		Repelled.Amount = ContactRule->ScoreValue;
		Repelled.Quantity = FiredBatteryCount;
		if (!ApplyAdversaryMissionThwarted(Transaction, Rules, Config, ContactId,
			Result, NextSequence, Transaction.StrategicTime.Utc))
		{
			Result.Events.Reset();
			AddError(Result, TEXT("adversary_state_overflow"), TEXT("Repelled base assault exceeded the campaign numeric range."));
			return Result;
		}
	}
	else
	{
		int32 TotalFacilityDamage = 0;
		int32 FacilitiesHit = 0;
		const int32 TargetCount = FMath::Min(DamageCandidateIds.Num(), MissionRule->BaseFacilitiesHit);
		for (int32 Target = 0; Target < TargetCount; ++Target)
		{
			const int32 SelectedIndex = Transaction.SimulationRandom.NextIntInclusive(0, DamageCandidateIds.Num() - 1);
			const FGuid FacilityInstanceId = DamageCandidateIds[SelectedIndex];
			DamageCandidateIds.RemoveAt(SelectedIndex, EAllowShrinking::No);
			FBaseFacilityState* Facility = FindFacility(*Base, FacilityInstanceId);
			check(Facility != nullptr);
			const FFacilityRule& FacilityRule = Rules.Facilities.FindChecked(Facility->FacilityId);
			const bool bWasOperational = Facility->Damage < FacilityRule.MaxIntegrity;
			const int32 AppliedDamage = FMath::Min(
				MissionRule->BaseFacilityDamage,
				FacilityRule.MaxIntegrity - Facility->Damage);
			Facility->Damage += AppliedDamage;
			TotalFacilityDamage += AppliedDamage;
			++FacilitiesHit;
			FStrategicEvent& Damaged = AddEvent(Result, EStrategicEventType::FacilityDamaged,
				NextSequence, Transaction.StrategicTime.Utc);
			Damaged.BaseId = BaseId;
			Damaged.FacilityInstanceId = FacilityInstanceId;
			Damaged.ContactId = ContactId;
			Damaged.MissionId = MissionId;
			Damaged.AssaultId = AssaultId;
			Damaged.RuleId = Facility->FacilityId;
			Damaged.PolicyId = DoctrinePolicyId;
			Damaged.Amount = -AppliedDamage;
			Damaged.Quantity = AppliedDamage;
			if (bWasOperational && Facility->Damage == FacilityRule.MaxIntegrity)
			{
				FStrategicEvent& Disabled = AddEvent(Result, EStrategicEventType::FacilityDisabled,
					NextSequence, Transaction.StrategicTime.Utc);
				Disabled.BaseId = BaseId;
				Disabled.FacilityInstanceId = FacilityInstanceId;
				Disabled.ContactId = ContactId;
				Disabled.MissionId = MissionId;
				Disabled.AssaultId = AssaultId;
				Disabled.RuleId = Facility->FacilityId;
				Disabled.PolicyId = DoctrinePolicyId;
			}
		}
		FStrategicEvent& Breached = AddEvent(Result, EStrategicEventType::BaseAssaultBreached,
			NextSequence, Transaction.StrategicTime.Utc);
		Breached.BaseId = BaseId;
		Breached.ContactId = ContactId;
		Breached.MissionId = MissionId;
		Breached.AssaultId = AssaultId;
		Breached.RuleId = MissionRuleId;
		Breached.PolicyId = DoctrinePolicyId;
		Breached.RegionId = RegionId;
		Breached.Amount = -TotalFacilityDamage;
		Breached.Quantity = FacilitiesHit;
		FStrategicEvent& Escaped = AddEvent(Result, EStrategicEventType::StrategicContactEscaped,
			NextSequence, Transaction.StrategicTime.Utc);
		Escaped.BaseId = BaseId;
		Escaped.ContactId = ContactId;
		Escaped.MissionId = MissionId;
		Escaped.AssaultId = AssaultId;
		Escaped.RuleId = ContactRuleId;
		Escaped.PolicyId = DoctrinePolicyId;
		Escaped.RegionId = RegionId;
		if (!ApplyAdversaryMissionEscape(Transaction, Rules, Config, ContactId,
			Result, NextSequence, Transaction.StrategicTime.Utc))
		{
			Result.Events.Reset();
			AddError(Result, TEXT("adversary_state_overflow"), TEXT("Breached base assault exceeded the campaign numeric range."));
			return Result;
		}
	}

	Transaction.BaseAssaults.RemoveAll(
		[&AssaultId](const FBaseAssaultState& Entry) { return Entry.AssaultId == AssaultId; });
	Transaction.StrategicContacts.RemoveAll(
		[&ContactId](const FStrategicContactState& Entry) { return Entry.ContactId == ContactId; });
	SortStateCollections(Transaction);
	Transaction.CommandSequence = NextSequence;
	State = MoveTemp(Transaction);
	Result.bAccepted = true;
	Result.bDecisionPause = true;
	return Result;
}

bool FStrategicCommandService::UpgradeLegacyFacilityLayouts(
	FCampaignState& State,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	TArray<FStrategicCommandDiagnostic>& OutDiagnostics)
{
	using namespace StrategicCommandServicePrivate;

	if (Config.BaseGridWidth <= 0 || Config.BaseGridHeight <= 0)
	{
		FStrategicCommandDiagnostic& Diagnostic = OutDiagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("invalid_simulation_config");
		Diagnostic.Message = TEXT("Base grid dimensions must be positive for layout migration.");
		return false;
	}

	FCampaignState Transaction = State;
	for (FStrategicBaseState& Base : Transaction.Bases)
	{
		for (int32 Index = 0; Index < Base.BuiltFacilities.Num(); ++Index)
		{
			const FName FacilityId = Base.BuiltFacilities[Index];
			if (Base.Facilities.ContainsByPredicate([FacilityId](const FBaseFacilityState& Facility) { return Facility.FacilityId == FacilityId; }))
			{
				continue;
			}
			if (!TryPlaceFacilityFirstFit(Base, FacilityId, MakeDeterministicFacilityId(Base.BaseId, FacilityId, Index), Rules, Config))
			{
				FStrategicCommandDiagnostic& Diagnostic = OutDiagnostics.AddDefaulted_GetRef();
				Diagnostic.Code = TEXT("legacy_layout_upgrade_failed");
				Diagnostic.Message = FString::Printf(TEXT("Legacy facility '%s' could not be positioned at base '%s'."), *FacilityId.ToString(), *Base.Name);
				return false;
			}
		}
		Base.BuiltFacilities.Reset();
	}
	SortStateCollections(Transaction);
	State = MoveTemp(Transaction);
	return true;
}
