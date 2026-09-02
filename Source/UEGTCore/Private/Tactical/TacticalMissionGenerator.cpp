// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/TacticalMissionGenerator.h"

#include "Misc/Crc.h"
#include "Strategic/PersonnelLegacyRelay.h"
#include "Strategic/PersonnelMentorship.h"
#include "Strategic/PersonnelSquadBond.h"
#include "Tactical/TacticalNavigationService.h"

namespace TacticalMissionGeneratorPrivate
{
	int64 ComputeTacticalMapCellCount(const int32 Width, const int32 Height, const int32 Levels)
	{
		if (Width < 8 || Width > 64 || Height < 12 || Height > 96 || Levels < 1 || Levels > 4)
		{
			return MAX_int64;
		}
		return static_cast<int64>(Width) * Height * Levels;
	}

	void AddError(TArray<FTacticalGenerationDiagnostic>& Diagnostics, const FName Code, FString Message)
	{
		FTacticalGenerationDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	int32 ClampPersonnelStatWithBonus(const int32 BaseValue, const int32 Bonus)
	{
		return static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(BaseValue) + static_cast<int64>(Bonus),
			1,
			100));
	}

	const FTacticalOperationState* FindOperation(const FCampaignState& Campaign, const FGuid& OperationId)
	{
		return Campaign.TacticalOperations.FindByPredicate(
			[&OperationId](const FTacticalOperationState& Operation) { return Operation.OperationId == OperationId; });
	}

	const FStrategicSiteState* FindSite(const FCampaignState& Campaign, const FGuid& SiteId)
	{
		return Campaign.StrategicSites.FindByPredicate(
			[&SiteId](const FStrategicSiteState& Site) { return Site.SiteId == SiteId; });
	}

	const FStrategicBaseState* FindBase(const FCampaignState& Campaign, const FGuid& BaseId)
	{
		return Campaign.Bases.FindByPredicate(
			[&BaseId](const FStrategicBaseState& Base) { return Base.BaseId == BaseId; });
	}

	const FBaseAssaultState* FindBaseAssault(const FCampaignState& Campaign, const FGuid& AssaultId)
	{
		return Campaign.BaseAssaults.FindByPredicate(
			[&AssaultId](const FBaseAssaultState& Assault) { return Assault.AssaultId == AssaultId; });
	}

	const FAdversaryMissionState* FindAdversaryMission(const FCampaignState& Campaign, const FGuid& MissionId)
	{
		return Campaign.AdversaryMissions.FindByPredicate(
			[&MissionId](const FAdversaryMissionState& Mission) { return Mission.MissionId == MissionId; });
	}

	const FStrategicContactState* FindContact(const FCampaignState& Campaign, const FGuid& ContactId)
	{
		return Campaign.StrategicContacts.FindByPredicate(
			[&ContactId](const FStrategicContactState& Contact) { return Contact.ContactId == ContactId; });
	}

	const FPersonnelState* FindPersonnel(const FCampaignState& Campaign, const FGuid& PersonnelId)
	{
		return Campaign.Personnel.FindByPredicate(
			[&PersonnelId](const FPersonnelState& Person) { return Person.PersonnelId == PersonnelId; });
	}

	FGuid MakeDeterministicId(const FGuid& OperationId, const TCHAR* Kind, const int32 Index)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%s|%d"),
			*OperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
			Kind,
			Index);
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

	void DeriveBattleWeather(
		const int64 TacticalSeed,
		ETacticalWindDirection& OutDirection,
		int32& OutStrength)
	{
		// SplitMix64-style finalization creates a stable weather channel without advancing combat RNG.
		uint64 Weather = static_cast<uint64>(TacticalSeed) + 0x9e3779b97f4a7c15ULL;
		Weather = (Weather ^ (Weather >> 30U)) * 0xbf58476d1ce4e5b9ULL;
		Weather = (Weather ^ (Weather >> 27U)) * 0x94d049bb133111ebULL;
		Weather ^= Weather >> 31U;
		OutStrength = static_cast<int32>(Weather % 4ULL);
		OutDirection = OutStrength == 0
			? ETacticalWindDirection::Calm
			: static_cast<ETacticalWindDirection>(1 + static_cast<int32>((Weather >> 2U) % 4ULL));
	}

	bool CargoMatches(const TArray<FInventoryStack>& Left, const TArray<FInventoryStack>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		TSet<FName> SeenItemIds;
		for (const FInventoryStack& Stack : Left)
		{
			if (SeenItemIds.Contains(Stack.ItemId))
			{
				return false;
			}
			const FInventoryStack* Other = Right.FindByPredicate(
				[&Stack](const FInventoryStack& Entry) { return Entry.ItemId == Stack.ItemId; });
			if (Other == nullptr || Other->Quantity != Stack.Quantity)
			{
				return false;
			}
			SeenItemIds.Add(Stack.ItemId);
		}
		return true;
	}

	void AddCarriedStack(TArray<FInventoryStack>& Inventory, const FName ItemId, const int32 Quantity)
	{
		if (Quantity <= 0)
		{
			return;
		}
		FInventoryStack& Stack = Inventory.AddDefaulted_GetRef();
		Stack.ItemId = ItemId;
		Stack.Quantity = Quantity;
	}

	int32 GetEffectiveTacticalMagazineCapacity(const FItemRule& Weapon)
	{
		return Weapon.TacticalMagazineCapacity > 0
			? FMath::Clamp(Weapon.TacticalMagazineCapacity, 1, 200)
			: 0;
	}

	void BuildPlayerLoadout(
		const FPersonnelState& Person,
		const FResolvedRuleSet& Rules,
		FTacticalUnitState& Unit)
	{
		TMap<FName, int32> RemainingItems;
		for (const FName ItemId : Person.EquippedItems)
		{
			++RemainingItems.FindOrAdd(ItemId);
		}

		TArray<FName> ItemIds;
		RemainingItems.GetKeys(ItemIds);
		ItemIds.Sort(FNameLexicalLess());
		for (const FName ItemId : ItemIds)
		{
			const FItemRule* Item = Rules.Items.Find(ItemId);
			if (Item == nullptr)
			{
				continue;
			}
			if (Item->IsTacticalWeapon())
			{
				FTacticalWeaponState& Weapon = Unit.WeaponStates.AddDefaulted_GetRef();
				Weapon.WeaponItemId = ItemId;
				if (!Item->TacticalAmmunitionItemId.IsNone())
				{
					int32& AvailableMagazines = RemainingItems.FindOrAdd(Item->TacticalAmmunitionItemId);
					if (AvailableMagazines > 0)
					{
						Weapon.LoadedAmmunition = GetEffectiveTacticalMagazineCapacity(*Item);
						--AvailableMagazines;
					}
				}
				--RemainingItems.FindChecked(ItemId);
			}
			if (Item->Category == FName(TEXT("armor")))
			{
				Unit.KineticArmor = FMath::Max(Unit.KineticArmor, Item->TacticalKineticArmor);
				Unit.ThermalArmor = FMath::Max(Unit.ThermalArmor, Item->TacticalThermalArmor);
				Unit.ArcArmor = FMath::Max(Unit.ArcArmor, Item->TacticalArcArmor);
				RemainingItems.FindChecked(ItemId) = 0;
			}
		}
		for (const FName ItemId : ItemIds)
		{
			AddCarriedStack(Unit.CarriedItems, ItemId, RemainingItems.FindRef(ItemId));
		}
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
	}

	bool IsReachable(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const int32 StartX,
		const int32 StartY,
		const int32 StartZ,
		const int32 GoalX,
		const int32 GoalY,
		const int32 GoalZ)
	{
		if (!Battle.IsWithinGrid(StartX, StartY, StartZ)
			|| !Battle.IsWithinGrid(GoalX, GoalY, GoalZ))
		{
			return false;
		}
		TArray<int32> Queue;
		Queue.Reserve(Battle.Cells.Num());
		TBitArray<> Visited(false, Battle.Cells.Num());
		const int32 StartIndex = Battle.GetCellIndex(StartX, StartY, StartZ);
		Queue.Add(StartIndex);
		Visited[StartIndex] = true;
		for (int32 ReadIndex = 0; ReadIndex < Queue.Num(); ++ReadIndex)
		{
			const int32 Current = Queue[ReadIndex];
			const int32 LayerArea = Battle.Width * Battle.Height;
			const int32 Z = Current / LayerArea;
			const int32 WithinLayer = Current % LayerArea;
			const int32 X = WithinLayer % Battle.Width;
			const int32 Y = WithinLayer / Battle.Width;
			if (X == GoalX && Y == GoalY && Z == GoalZ)
			{
				return true;
			}
			static constexpr int32 OffsetX[] = { 1, -1, 0, 0, 0, 0 };
			static constexpr int32 OffsetY[] = { 0, 0, 1, -1, 0, 0 };
			static constexpr int32 OffsetZ[] = { 0, 0, 0, 0, 1, -1 };
			for (int32 Direction = 0; Direction < 6; ++Direction)
			{
				const int32 NextX = X + OffsetX[Direction];
				const int32 NextY = Y + OffsetY[Direction];
				const int32 NextZ = Z + OffsetZ[Direction];
				if (!Battle.IsWithinGrid(NextX, NextY, NextZ))
				{
					continue;
				}
				const int32 NextIndex = Battle.GetCellIndex(NextX, NextY, NextZ);
				const FTacticalCellState& Cell = Battle.Cells[NextIndex];
				const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
				const FTacticalCellState& CurrentCell = Battle.Cells[Current];
				const FTacticalTerrainRule* CurrentTerrain = Rules.TacticalTerrains.Find(CurrentCell.TerrainRuleId);
				const bool bVertical = OffsetZ[Direction] != 0;
				if (Visited[NextIndex] || Terrain == nullptr
					|| (Terrain->bBlocksMovement && Cell.CurrentIntegrity > 0 && !Terrain->IsDoor())
					|| (bVertical && (CurrentTerrain == nullptr || !CurrentTerrain->IsVerticalConnector() || !Terrain->IsVerticalConnector())))
				{
					continue;
				}
				Visited[NextIndex] = true;
				Queue.Add(NextIndex);
			}
		}
		return false;
	}

	bool SiteTypesMatch(const ETacticalSiteType RuleType, const EStrategicSiteType SiteType)
	{
		return (RuleType == ETacticalSiteType::Wreckage && SiteType == EStrategicSiteType::Wreckage)
			|| (RuleType == ETacticalSiteType::Landing && SiteType == EStrategicSiteType::Landing);
	}
}

bool FTacticalGenerationResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalGenerationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FTacticalGenerationResult FTacticalMissionGenerator::Generate(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FGuid OperationId)
{
	using namespace TacticalMissionGeneratorPrivate;

	FTacticalGenerationResult Result;
	const FTacticalOperationState* Operation = FindOperation(Campaign, OperationId);
	if (Operation == nullptr)
	{
		AddError(Result.Diagnostics, TEXT("unknown_tactical_operation"), TEXT("Tactical battlefield generation requires an active operation."));
		return Result;
	}
	const bool bBaseDefense = Operation->Type == ETacticalOperationType::BaseDefense;
	const bool bSiteRecovery = Operation->Type == ETacticalOperationType::SiteRecovery;
	const FStrategicSiteState* Site = bSiteRecovery ? FindSite(Campaign, Operation->SiteId) : nullptr;
	const FStrategicBaseState* Base = bBaseDefense ? FindBase(Campaign, Operation->BaseId) : nullptr;
	const FBaseAssaultState* Assault = bBaseDefense ? FindBaseAssault(Campaign, Operation->AssaultId) : nullptr;
	const FAdversaryMissionState* AdversaryMission = Assault != nullptr ? FindAdversaryMission(Campaign, Assault->MissionId) : nullptr;
	const FStrategicContactState* Contact = Assault != nullptr ? FindContact(Campaign, Assault->ContactId) : nullptr;
	FName SourceContactRuleId;
	int32 ThreatRating = 0;
	ETacticalMissionContext MissionContext = ETacticalMissionContext::StrategicSite;
	EStrategicSiteType StrategicSiteType = EStrategicSiteType::Wreckage;
	if (bSiteRecovery && Site != nullptr && !Operation->BaseId.IsValid() && !Operation->AssaultId.IsValid())
	{
		SourceContactRuleId = Site->SourceContactRuleId;
		ThreatRating = Site->ThreatRating;
		StrategicSiteType = Site->Type;
	}
	else if (bBaseDefense && Base != nullptr && Assault != nullptr && AdversaryMission != nullptr && Contact != nullptr
		&& !Operation->SiteId.IsValid() && !Operation->CraftId.IsValid()
		&& Assault->BaseId == Operation->BaseId && AdversaryMission->MissionId == Assault->MissionId
		&& AdversaryMission->ContactId == Assault->ContactId && AdversaryMission->TargetBaseId == Operation->BaseId)
	{
		const FContactRule* ContactRule = Rules.Contacts.Find(Contact->ContactRuleId);
		if (ContactRule == nullptr)
		{
			AddError(Result.Diagnostics, TEXT("unknown_tactical_contact"), TEXT("Base-defense operation references an unloaded contact rule."));
			return Result;
		}
		SourceContactRuleId = Contact->ContactRuleId;
		ThreatRating = ContactRule->ThreatRating;
		MissionContext = ETacticalMissionContext::BaseDefense;
	}
	else
	{
		AddError(Result.Diagnostics, TEXT("invalid_tactical_context"), TEXT("Tactical operation has inconsistent strategic source links."));
		return Result;
	}

	const FTacticalMissionRule* Mission = nullptr;
	for (const TPair<FName, FTacticalMissionRule>& Pair : Rules.TacticalMissions)
	{
		if (Pair.Value.Context != MissionContext || Pair.Value.SourceContactRuleId != SourceContactRuleId
			|| (MissionContext == ETacticalMissionContext::StrategicSite
				&& !SiteTypesMatch(Pair.Value.SiteType, StrategicSiteType)))
		{
			continue;
		}
		if (Mission != nullptr)
		{
			AddError(Result.Diagnostics, TEXT("ambiguous_tactical_mission"), TEXT("More than one tactical mission recipe matches the operation context and source contact."));
			return Result;
		}
		Mission = &Pair.Value;
	}
	if (Mission == nullptr)
	{
		AddError(Result.Diagnostics, TEXT("missing_tactical_mission"), FString::Printf(TEXT("No tactical mission recipe maps source contact '%s' in this operation context."), *SourceContactRuleId.ToString()));
		return Result;
	}
	const FTacticalTerrainRule* Floor = Rules.TacticalTerrains.Find(Mission->FloorTerrainRuleId);
	const FTacticalTerrainRule* Obstacle = Rules.TacticalTerrains.Find(Mission->ObstacleTerrainRuleId);
	const FTacticalTerrainRule* Door = Mission->DoorTerrainRuleId.IsNone()
		? nullptr
		: Rules.TacticalTerrains.Find(Mission->DoorTerrainRuleId);
	const FTacticalTerrainRule* VerticalConnector = Mission->VerticalConnectorTerrainRuleId.IsNone()
		? nullptr
		: Rules.TacticalTerrains.Find(Mission->VerticalConnectorTerrainRuleId);
	const FTacticalUnitRule* Adversary = Rules.TacticalUnits.Find(Mission->AdversaryUnitRuleId);
	if (Floor == nullptr || Floor->bBlocksMovement || Obstacle == nullptr || !Obstacle->bBlocksMovement
		|| (!Mission->DoorTerrainRuleId.IsNone() && (Door == nullptr || !Door->IsDoor()))
		|| (!Mission->VerticalConnectorTerrainRuleId.IsNone() && (VerticalConnector == nullptr || !VerticalConnector->IsVerticalConnector()))
		|| (Mission->MapLevels > 1 && VerticalConnector == nullptr) || Adversary == nullptr)
	{
		AddError(Result.Diagnostics, TEXT("invalid_tactical_rules"), TEXT("Tactical mission references an invalid floor, obstacle, connector, or adversary unit rule."));
		return Result;
	}
	const int64 MapCellCount = ComputeTacticalMapCellCount(
		Mission->MapWidth, Mission->MapHeight, Mission->MapLevels);
	const int64 EnemyDeploymentCapacity = static_cast<int64>(Mission->MapWidth) * Mission->DeploymentDepth;
	const int64 EnemyCount64 = static_cast<int64>(Mission->BaseEnemyCount)
		+ static_cast<int64>(Mission->EnemiesPerThreat) * ThreatRating;
	if (Mission->MapWidth < 8 || Mission->MapWidth > 64
		|| Mission->MapHeight < 12 || Mission->MapHeight > 96
		|| Mission->MapLevels < 1 || Mission->MapLevels > 4 || MapCellCount > 8192
		|| Mission->DeploymentDepth < 2 || Mission->DeploymentDepth > 8
		|| Mission->MapHeight <= Mission->DeploymentDepth * 2 + 4
		|| Mission->ObstaclePercent < 0 || Mission->ObstaclePercent > 60
		|| Mission->TurnLimit <= 0 || Mission->TurnLimit > 500
		|| ThreatRating <= 0 || ThreatRating > 10
		|| EnemyCount64 <= 0 || EnemyCount64 >= EnemyDeploymentCapacity)
	{
		AddError(Result.Diagnostics, TEXT("invalid_tactical_rules"), TEXT("Tactical mission dimensions, deployment capacity, threat, or population are outside supported limits."));
		return Result;
	}
	if (Operation->AgentIds.IsEmpty() || Operation->AgentIds.Num() > Mission->MapWidth)
	{
		AddError(Result.Diagnostics, TEXT("invalid_tactical_roster"), TEXT("Tactical roster does not fit the mission deployment line."));
		return Result;
	}

	FTacticalBattleState& Battle = Result.Battle;
	Battle.BattleId = MakeDeterministicId(Operation->OperationId, TEXT("battle"), 0);
	Battle.OperationId = Operation->OperationId;
	Battle.SiteId = Operation->SiteId;
	Battle.MissionRuleId = Mission->Identity.RuleId;
	Battle.CreatedUtc = Operation->CreatedUtc;
	Battle.Width = Mission->MapWidth;
	Battle.Height = Mission->MapHeight;
	Battle.Levels = Mission->MapLevels;
	Battle.TurnLimit = Mission->TurnLimit;
	Battle.TurnNumber = 1;
	Battle.bRequiresExtraction = bSiteRecovery;
	Battle.Phase = ETacticalBattlePhase::Deployment;
	Battle.ActiveTeam = ETacticalTeam::Player;
	DeriveBattleWeather(Operation->TacticalSeed, Battle.WindDirection, Battle.WindStrength);
	Battle.TacticalRandom.Initialize(Operation->TacticalSeed);
	Battle.Cargo = Operation->Cargo;
	Battle.Cargo.Sort(
		[](const FInventoryStack& Left, const FInventoryStack& Right) { return Left.ItemId.LexicalLess(Right.ItemId); });

	Battle.Cells.Reserve(static_cast<int32>(MapCellCount));
	for (int32 Z = 0; Z < Battle.Levels; ++Z)
	{
		for (int32 Y = 0; Y < Battle.Height; ++Y)
		{
			for (int32 X = 0; X < Battle.Width; ++X)
			{
				FTacticalCellState& Cell = Battle.Cells.AddDefaulted_GetRef();
				Cell.X = X;
				Cell.Y = Y;
				Cell.Z = Z;
				Cell.TerrainRuleId = Floor->Identity.RuleId;
				Cell.CurrentIntegrity = Floor->MaxIntegrity;
				Cell.bPlayerDeployment = Z == 0 && Y < Mission->DeploymentDepth;
				Cell.bExtraction = Battle.bRequiresExtraction && Z == 0 && Y == 0;
			}
		}
	}

	const int32 ObjectiveX = Battle.Width / 2;
	const int32 ObjectiveY = bBaseDefense
		? Mission->DeploymentDepth + 1
		: Battle.Height - Mission->DeploymentDepth - 2;
	const int32 ObjectiveZ = bBaseDefense ? 0 : Battle.Levels - 1;
	TArray<int32> ObstacleCandidates;
	for (int32 Z = 0; Z < Battle.Levels; ++Z)
	{
		for (int32 Y = Mission->DeploymentDepth; Y < Battle.Height - Mission->DeploymentDepth; ++Y)
		{
			for (int32 X = 0; X < Battle.Width; ++X)
			{
				if (FMath::Abs(X - ObjectiveX) <= 1
					|| (Z == ObjectiveZ && FMath::Abs(X - ObjectiveX) <= 2 && FMath::Abs(Y - ObjectiveY) <= 2))
				{
					continue;
				}
				ObstacleCandidates.Add(Battle.GetCellIndex(X, Y, Z));
			}
		}
	}
	const int32 ObstacleCount = static_cast<int32>(
		(static_cast<int64>(ObstacleCandidates.Num()) * Mission->ObstaclePercent) / 100);
	for (int32 Index = 0; Index < ObstacleCount; ++Index)
	{
		const int32 SwapIndex = Battle.TacticalRandom.NextIntInclusive(Index, ObstacleCandidates.Num() - 1);
		ObstacleCandidates.Swap(Index, SwapIndex);
		FTacticalCellState& Cell = Battle.Cells[ObstacleCandidates[Index]];
		Cell.TerrainRuleId = Obstacle->Identity.RuleId;
		Cell.CurrentIntegrity = Obstacle->MaxIntegrity;
	}
	if (bBaseDefense && Base != nullptr)
	{
		TArray<const FBaseFacilityState*> OperationalFacilities;
		for (const FBaseFacilityState& Facility : Base->Facilities)
		{
			const FFacilityRule* FacilityRule = Rules.Facilities.Find(Facility.FacilityId);
			if (FacilityRule != nullptr && FacilityRule->MaxIntegrity > 0 && Facility.Damage < FacilityRule->MaxIntegrity)
			{
				OperationalFacilities.Add(&Facility);
			}
		}
		OperationalFacilities.Sort(
			[](const FBaseFacilityState& Left, const FBaseFacilityState& Right)
			{
				return Left.InstanceId.ToString(EGuidFormats::Digits) < Right.InstanceId.ToString(EGuidFormats::Digits);
			});
		const int32 FriendlyMinY = Mission->DeploymentDepth;
		const int32 FriendlyMaxY = FMath::Max(FriendlyMinY, Battle.Height / 2 - 2);
		const int32 FriendlyRows = FriendlyMaxY - FriendlyMinY + 1;
		TSet<int32> FacilityCoverCells;
		for (const FBaseFacilityState* Facility : OperationalFacilities)
		{
			const uint32 FacilityHash = Facility->InstanceId.A ^ Facility->InstanceId.B
				^ Facility->InstanceId.C ^ Facility->InstanceId.D
				^ static_cast<uint32>(Facility->GridX * 73856093) ^ static_cast<uint32>(Facility->GridY * 19349663);
			const int32 CandidateCount = Battle.Width * FriendlyRows;
			for (int32 Offset = 0; Offset < CandidateCount; ++Offset)
			{
				const int32 Candidate = (static_cast<int32>(FacilityHash % static_cast<uint32>(CandidateCount)) + Offset) % CandidateCount;
				const int32 X = Candidate % Battle.Width;
				const int32 Y = FriendlyMinY + Candidate / Battle.Width;
				const int32 CellIndex = Battle.GetCellIndex(X, Y, 0);
				const FTacticalCellState& CandidateCell = Battle.Cells[CellIndex];
				if (FMath::Abs(X - ObjectiveX) <= 1
					|| (FMath::Abs(X - ObjectiveX) <= 2 && FMath::Abs(Y - ObjectiveY) <= 2)
					|| FacilityCoverCells.Contains(CellIndex)
					|| CandidateCell.TerrainRuleId != Floor->Identity.RuleId)
				{
					continue;
				}
				FTacticalCellState& Cell = Battle.Cells[CellIndex];
				Cell.TerrainRuleId = Obstacle->Identity.RuleId;
				Cell.CurrentIntegrity = Obstacle->MaxIntegrity;
				FacilityCoverCells.Add(CellIndex);
				break;
			}
		}
	}
	if (Door != nullptr)
	{
		const int32 BarrierY = Battle.Height / 2;
		for (int32 Z = 0; Z < Battle.Levels; ++Z)
		{
			for (int32 X = 0; X < Battle.Width; ++X)
			{
				FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(X, BarrierY, Z)];
				const FTacticalTerrainRule* BarrierTerrain = X == ObjectiveX ? Door : Obstacle;
				Cell.TerrainRuleId = BarrierTerrain->Identity.RuleId;
				Cell.CurrentIntegrity = BarrierTerrain->MaxIntegrity;
				Cell.bDoorOpen = false;
			}
		}
	}
	if (VerticalConnector != nullptr && Battle.Levels > 1)
	{
		const int32 ConnectorY = Battle.Height / 2 - 2;
		for (int32 Z = 0; Z < Battle.Levels; ++Z)
		{
			FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(ObjectiveX, ConnectorY, Z)];
			Cell.TerrainRuleId = VerticalConnector->Identity.RuleId;
			Cell.CurrentIntegrity = VerticalConnector->MaxIntegrity;
			Cell.bDoorOpen = false;
		}
	}

	TArray<FGuid> AgentIds = Operation->AgentIds;
	AgentIds.Sort(
		[](const FGuid& Left, const FGuid& Right) { return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits); });
	const FPersonnelMentorshipView Mentorship = FPersonnelMentorship::Evaluate(Campaign, AgentIds);
	TSet<FGuid> MentorshipRecipientIds;
	MentorshipRecipientIds.Reserve(Mentorship.RecipientIds.Num());
	for (const FGuid& RecipientId : Mentorship.RecipientIds)
	{
		MentorshipRecipientIds.Add(RecipientId);
	}
	const FPersonnelLegacyRelayView LegacyRelay =
		FPersonnelLegacyRelay::Evaluate(Campaign, Rules, AgentIds);
	TSet<FGuid> LegacyRelayRecipientIds;
	LegacyRelayRecipientIds.Reserve(LegacyRelay.RecipientIds.Num());
	for (const FGuid& RecipientId : LegacyRelay.RecipientIds)
	{
		LegacyRelayRecipientIds.Add(RecipientId);
	}
	const FPersonnelSquadBondView SquadBonds = FPersonnelSquadBond::Evaluate(Campaign, AgentIds);
	TMap<FGuid, int32> SquadBondActionPointBonuses;
	TMap<FGuid, int32> SquadBondMoraleBonuses;
	for (const FPersonnelSquadBondPairView& Pair : SquadBonds.ActivePairs)
	{
		SquadBondActionPointBonuses.Add(Pair.FirstPersonnelId, Pair.ActionPointBonus);
		SquadBondActionPointBonuses.Add(Pair.SecondPersonnelId, Pair.ActionPointBonus);
		SquadBondMoraleBonuses.Add(Pair.FirstPersonnelId, Pair.MoraleBonus);
		SquadBondMoraleBonuses.Add(Pair.SecondPersonnelId, Pair.MoraleBonus);
	}
	for (int32 Index = 0; Index < AgentIds.Num(); ++Index)
	{
		const FPersonnelState* Person = FindPersonnel(Campaign, AgentIds[Index]);
		if (Person == nullptr || Person->Status != EPersonnelStatus::Deployed)
		{
			AddError(Result.Diagnostics, TEXT("invalid_tactical_agent"), TEXT("Tactical operation references a missing or non-deployed field agent."));
			Result.Battle = FTacticalBattleState();
			return Result;
		}
		FTacticalUnitState& Unit = Battle.Units.AddDefaulted_GetRef();
		Unit.UnitId = MakeDeterministicId(Operation->OperationId, TEXT("agent"), Index);
		Unit.PersonnelId = Person->PersonnelId;
		Unit.SourceRuleId = Person->RoleId;
		Unit.DisplayName = Person->DisplayName;
		Unit.Team = ETacticalTeam::Player;
		Unit.X = ((Index + 1) * Battle.Width) / (AgentIds.Num() + 1);
		Unit.Y = 1;
		Unit.Z = 0;
		Unit.MaxHealth = Person->MaxHealth;
		Unit.CurrentHealth = Person->CurrentHealth;
		const bool bReceivesLegacyRelay = LegacyRelayRecipientIds.Contains(Person->PersonnelId);
		Unit.Accuracy = ClampPersonnelStatWithBonus(
			Person->Accuracy, bReceivesLegacyRelay ? LegacyRelay.AccuracyBonus : 0);
		Unit.Resolve = ClampPersonnelStatWithBonus(
			Person->Resolve, bReceivesLegacyRelay ? LegacyRelay.ResolveBonus : 0);
		Unit.Mobility = ClampPersonnelStatWithBonus(
			Person->Mobility, bReceivesLegacyRelay ? LegacyRelay.MobilityBonus : 0);
		Unit.Strength = ClampPersonnelStatWithBonus(
			Person->Strength, bReceivesLegacyRelay ? LegacyRelay.StrengthBonus : 0);
		Unit.MaxActionPoints = FMath::Clamp(
			6 + Unit.Mobility / 20 + SquadBondActionPointBonuses.FindRef(Person->PersonnelId), 6, 20);
		Unit.RemainingActionPoints = Unit.MaxActionPoints;
		const int32 MentorshipBonus = MentorshipRecipientIds.Contains(Person->PersonnelId)
			? Mentorship.MoraleBonus
			: 0;
		Unit.MaxMorale = FMath::Clamp(
			Unit.Resolve + MentorshipBonus + SquadBondMoraleBonuses.FindRef(Person->PersonnelId), 0, 100);
		Unit.CurrentMorale = Unit.MaxMorale;
		BuildPlayerLoadout(*Person, Rules, Unit);
	}

	const int32 EnemyCount = static_cast<int32>(EnemyCount64);
	TArray<int32> EnemyCells;
	for (int32 Y = Battle.Height - Mission->DeploymentDepth; Y < Battle.Height; ++Y)
	{
		for (int32 X = 0; X < Battle.Width; ++X)
		{
			EnemyCells.Add(Battle.GetCellIndex(X, Y, ObjectiveZ));
		}
	}
	for (int32 Index = 0; Index < EnemyCount; ++Index)
	{
		const int32 SwapIndex = Battle.TacticalRandom.NextIntInclusive(Index, EnemyCells.Num() - 1);
		EnemyCells.Swap(Index, SwapIndex);
		const int32 Position = EnemyCells[Index];
		FTacticalUnitState& Unit = Battle.Units.AddDefaulted_GetRef();
		Unit.UnitId = MakeDeterministicId(Operation->OperationId, TEXT("adversary"), Index);
		Unit.SourceRuleId = Adversary->Identity.RuleId;
		Unit.DisplayName = Adversary->DisplayName;
		Unit.Team = ETacticalTeam::Adversary;
		const int32 LayerArea = Battle.Width * Battle.Height;
		const int32 WithinLayer = Position % LayerArea;
		Unit.X = WithinLayer % Battle.Width;
		Unit.Y = WithinLayer / Battle.Width;
		Unit.Z = Position / LayerArea;
		Unit.MaxHealth = Adversary->MaxHealth;
		Unit.CurrentHealth = Adversary->MaxHealth;
		Unit.Accuracy = Adversary->Accuracy;
		Unit.Resolve = Adversary->Resolve;
		Unit.Mobility = Adversary->Mobility;
		Unit.Strength = Adversary->Strength;
		Unit.MaxActionPoints = Adversary->ActionPoints;
		Unit.RemainingActionPoints = Adversary->ActionPoints;
		Unit.KineticArmor = Adversary->KineticArmor;
		Unit.ThermalArmor = Adversary->ThermalArmor;
		Unit.ArcArmor = Adversary->ArcArmor;
		Unit.MaxMorale = Adversary->Resolve;
		Unit.CurrentMorale = Unit.MaxMorale;
	}

	FTacticalObjectiveState& Objective = Battle.Objectives.AddDefaulted_GetRef();
	Objective.ObjectiveId = Mission->ObjectiveId;
	Objective.X = ObjectiveX;
	Objective.Y = ObjectiveY;
	Objective.Z = ObjectiveZ;
	Objective.Status = ETacticalObjectiveStatus::Active;
	Objective.Type = Mission->ObjectiveType;
	Objective.RequiredInteractions = Mission->ObjectiveRequiredInteractions;

	const FTacticalVisibilityResult InitialDiscovery = FTacticalNavigationService::RefreshPlayerDiscovery(Battle, Rules);
	if (!InitialDiscovery.bSucceeded)
	{
		for (const FTacticalNavigationDiagnostic& Diagnostic : InitialDiscovery.Diagnostics)
		{
			AddError(Result.Diagnostics, Diagnostic.Code, Diagnostic.Message);
		}
		Result.Battle = FTacticalBattleState();
		return Result;
	}

	if (!ValidateBattle(Battle, Campaign, Rules, Result.Diagnostics))
	{
		Result.Battle = FTacticalBattleState();
		return Result;
	}
	Result.bSucceeded = true;
	return Result;
}

bool FTacticalMissionGenerator::ValidateBattle(
	const FTacticalBattleState& Battle,
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	TArray<FTacticalGenerationDiagnostic>& OutDiagnostics)
{
	using namespace TacticalMissionGeneratorPrivate;

	const FTacticalOperationState* Operation = FindOperation(Campaign, Battle.OperationId);
	const bool bSiteRecovery = Operation != nullptr && Operation->Type == ETacticalOperationType::SiteRecovery;
	const bool bBaseDefense = Operation != nullptr && Operation->Type == ETacticalOperationType::BaseDefense;
	const FStrategicSiteState* Site = bSiteRecovery ? FindSite(Campaign, Operation->SiteId) : nullptr;
	const FStrategicBaseState* Base = bBaseDefense ? FindBase(Campaign, Operation->BaseId) : nullptr;
	const FBaseAssaultState* Assault = bBaseDefense ? FindBaseAssault(Campaign, Operation->AssaultId) : nullptr;
	const FAdversaryMissionState* AdversaryMission = Assault != nullptr ? FindAdversaryMission(Campaign, Assault->MissionId) : nullptr;
	const FStrategicContactState* Contact = Assault != nullptr ? FindContact(Campaign, Assault->ContactId) : nullptr;
	const FContactRule* ContactRule = Contact != nullptr ? Rules.Contacts.Find(Contact->ContactRuleId) : nullptr;
	const FTacticalMissionRule* Mission = Rules.TacticalMissions.Find(Battle.MissionRuleId);
	const int32 ThreatRating = bSiteRecovery && Site != nullptr
		? Site->ThreatRating
		: (bBaseDefense && ContactRule != nullptr ? ContactRule->ThreatRating : 0);
	const bool bContextLinksValid =
		(bSiteRecovery && Site != nullptr
			&& !Operation->BaseId.IsValid() && !Operation->AssaultId.IsValid()
			&& Battle.SiteId == Operation->SiteId && Battle.bRequiresExtraction
			&& Mission != nullptr && Mission->Context == ETacticalMissionContext::StrategicSite
			&& SiteTypesMatch(Mission->SiteType, Site->Type)
			&& Mission->SourceContactRuleId == Site->SourceContactRuleId)
		|| (bBaseDefense && Base != nullptr && Assault != nullptr && AdversaryMission != nullptr && Contact != nullptr && ContactRule != nullptr
			&& !Operation->SiteId.IsValid() && !Operation->CraftId.IsValid() && !Battle.SiteId.IsValid() && !Battle.bRequiresExtraction
			&& Assault->BaseId == Operation->BaseId && AdversaryMission->MissionId == Assault->MissionId
			&& AdversaryMission->ContactId == Assault->ContactId && AdversaryMission->TargetBaseId == Operation->BaseId
			&& Mission != nullptr && Mission->Context == ETacticalMissionContext::BaseDefense
			&& Mission->SourceContactRuleId == Contact->ContactRuleId);
	const bool bKnownPhase = Battle.Phase == ETacticalBattlePhase::Deployment
		|| Battle.Phase == ETacticalBattlePhase::PlayerTurn
		|| Battle.Phase == ETacticalBattlePhase::AdversaryTurn
		|| Battle.Phase == ETacticalBattlePhase::Resolved;
	const bool bKnownActiveTeam = Battle.ActiveTeam == ETacticalTeam::Player
		|| Battle.ActiveTeam == ETacticalTeam::Adversary;
	const bool bKnownWindDirection = Battle.WindDirection == ETacticalWindDirection::Calm
		|| Battle.WindDirection == ETacticalWindDirection::North
		|| Battle.WindDirection == ETacticalWindDirection::East
		|| Battle.WindDirection == ETacticalWindDirection::South
		|| Battle.WindDirection == ETacticalWindDirection::West;
	const bool bWindValid = bKnownWindDirection && Battle.WindStrength >= 0 && Battle.WindStrength <= 3
		&& ((Battle.WindStrength == 0) == (Battle.WindDirection == ETacticalWindDirection::Calm));
	const bool bPhaseTeamValid = (Battle.Phase == ETacticalBattlePhase::Deployment && Battle.ActiveTeam == ETacticalTeam::Player)
		|| (Battle.Phase == ETacticalBattlePhase::PlayerTurn && Battle.ActiveTeam == ETacticalTeam::Player)
		|| (Battle.Phase == ETacticalBattlePhase::AdversaryTurn && Battle.ActiveTeam == ETacticalTeam::Adversary)
		|| Battle.Phase == ETacticalBattlePhase::Resolved;
	if (!Battle.BattleId.IsValid() || Operation == nullptr || Mission == nullptr || !bContextLinksValid
		|| Battle.CreatedUtc != Operation->CreatedUtc
		|| Battle.Width <= 0 || Battle.Height <= 0 || Battle.Levels <= 0
		|| Battle.Width > 64 || Battle.Height > 96 || Battle.Levels > 4
		|| Battle.Width != (Mission != nullptr ? Mission->MapWidth : 0)
		|| Battle.Height != (Mission != nullptr ? Mission->MapHeight : 0)
		|| Battle.Levels != (Mission != nullptr ? Mission->MapLevels : 0)
		|| Battle.TurnLimit != (Mission != nullptr ? Mission->TurnLimit : 0)
		|| Battle.TurnNumber <= 0 || Battle.TurnNumber > Battle.TurnLimit
		|| !bKnownPhase || !bKnownActiveTeam || !bPhaseTeamValid || !bWindValid
		|| !Battle.TacticalRandom.IsValid()
		|| Battle.TacticalRandom.InitialSeed != Operation->TacticalSeed
		|| static_cast<int64>(Battle.Cells.Num()) != static_cast<int64>(Battle.Width) * Battle.Height * Battle.Levels
		|| !CargoMatches(Battle.Cargo, Operation->Cargo))
	{
		AddError(OutDiagnostics, TEXT("invalid_tactical_battle"), TEXT("Tactical battle identity, operation link, dimensions, phase, weather, random state, cells, or cargo are invalid."));
		return false;
	}

	TSet<int32> SeenCellIndices;
	int32 DeploymentCells = 0;
	int32 ExtractionCells = 0;
	for (int32 CellArrayIndex = 0; CellArrayIndex < Battle.Cells.Num(); ++CellArrayIndex)
	{
		const FTacticalCellState& Cell = Battle.Cells[CellArrayIndex];
		const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
		const int32 Index = Battle.IsWithinGrid(Cell.X, Cell.Y, Cell.Z)
			? Battle.GetCellIndex(Cell.X, Cell.Y, Cell.Z)
			: -1;
		if (Index < 0 || Index != CellArrayIndex || SeenCellIndices.Contains(Index) || Terrain == nullptr
			|| Cell.CurrentIntegrity < 0 || (Terrain != nullptr && Cell.CurrentIntegrity > Terrain->MaxIntegrity)
			|| (Cell.bDoorOpen && (Terrain == nullptr || !Terrain->IsDoor() || Cell.CurrentIntegrity <= 0))
			|| Cell.Smoke < 0 || Cell.Smoke > 100 || Cell.Fire < 0 || Cell.Fire > 100
			|| (Cell.bExtraction && !Cell.bPlayerDeployment))
		{
			AddError(OutDiagnostics, TEXT("invalid_tactical_cell"), TEXT("Tactical battlefield contains an invalid coordinate, terrain, integrity, or zone marker."));
			return false;
		}
		SeenCellIndices.Add(Index);
		DeploymentCells += Cell.bPlayerDeployment ? 1 : 0;
		ExtractionCells += Cell.bExtraction ? 1 : 0;
	}
	if (DeploymentCells < Operation->AgentIds.Num()
		|| (Battle.bRequiresExtraction ? ExtractionCells <= 0 : ExtractionCells != 0))
	{
		AddError(OutDiagnostics, TEXT("invalid_tactical_zones"), TEXT("Tactical battlefield lacks sufficient deployment or extraction cells."));
		return false;
	}

	int32 PreviousDiscoveredCellIndex = INDEX_NONE;
	for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
	{
		if (!Battle.Cells.IsValidIndex(CellIndex) || CellIndex <= PreviousDiscoveredCellIndex)
		{
			AddError(OutDiagnostics, TEXT("invalid_tactical_discovery"), TEXT("Tactical battlefield discovery indices must be unique, sorted, and inside the grid."));
			return false;
		}
		PreviousDiscoveredCellIndex = CellIndex;
	}

	TSet<FGuid> SeenUnitIds;
	TSet<int32> OccupiedCells;
	TSet<FGuid> PlayerPersonnelIds;
	const FPersonnelLegacyRelayView LegacyRelay = Operation != nullptr
		? FPersonnelLegacyRelay::Evaluate(Campaign, Rules, Operation->AgentIds)
		: FPersonnelLegacyRelayView();
	TSet<FGuid> LegacyRelayRecipientIds;
	LegacyRelayRecipientIds.Reserve(LegacyRelay.RecipientIds.Num());
	for (const FGuid& RecipientId : LegacyRelay.RecipientIds)
	{
		LegacyRelayRecipientIds.Add(RecipientId);
	}
	const FPersonnelMentorshipView Mentorship = Operation != nullptr
		? FPersonnelMentorship::Evaluate(Campaign, Operation->AgentIds)
		: FPersonnelMentorshipView();
	TSet<FGuid> MentorshipRecipientIds;
	for (const FGuid& RecipientId : Mentorship.RecipientIds)
	{
		MentorshipRecipientIds.Add(RecipientId);
	}
	const FPersonnelSquadBondView SquadBonds = Operation != nullptr
		? FPersonnelSquadBond::Evaluate(Campaign, Operation->AgentIds)
		: FPersonnelSquadBondView();
	TMap<FGuid, int32> SquadBondActionPointBonuses;
	TMap<FGuid, int32> SquadBondMoraleBonuses;
	for (const FPersonnelSquadBondPairView& Pair : SquadBonds.ActivePairs)
	{
		SquadBondActionPointBonuses.Add(Pair.FirstPersonnelId, Pair.ActionPointBonus);
		SquadBondActionPointBonuses.Add(Pair.SecondPersonnelId, Pair.ActionPointBonus);
		SquadBondMoraleBonuses.Add(Pair.FirstPersonnelId, Pair.MoraleBonus);
		SquadBondMoraleBonuses.Add(Pair.SecondPersonnelId, Pair.MoraleBonus);
	}
	int32 AdversaryCount = 0;
	for (const FTacticalUnitState& Unit : Battle.Units)
	{
		const int32 Index = Battle.IsWithinGrid(Unit.X, Unit.Y, Unit.Z)
			? Battle.GetCellIndex(Unit.X, Unit.Y, Unit.Z)
			: -1;
		const FTacticalCellState* Cell = Index >= 0 ? &Battle.Cells[Index] : nullptr;
		const FTacticalTerrainRule* Terrain = Cell != nullptr ? Rules.TacticalTerrains.Find(Cell->TerrainRuleId) : nullptr;
		const bool bKnownStance = Unit.Stance == ETacticalStance::Standing || Unit.Stance == ETacticalStance::Crouched;
		const bool bAttributesValid = Unit.MaxHealth > 0 && Unit.MaxHealth <= 200
			&& Unit.CurrentHealth >= 0 && Unit.CurrentHealth <= Unit.MaxHealth
			&& Unit.Accuracy > 0 && Unit.Accuracy <= 100
			&& Unit.Resolve > 0 && Unit.Resolve <= 100
			&& Unit.Mobility > 0 && Unit.Mobility <= 100
			&& Unit.Strength > 0 && Unit.Strength <= 100
			&& Unit.MaxActionPoints > 0 && Unit.MaxActionPoints <= 20
			&& Unit.RemainingActionPoints >= 0 && Unit.RemainingActionPoints <= Unit.MaxActionPoints
			&& Unit.KineticArmor >= 0 && Unit.KineticArmor <= 100
			&& Unit.ThermalArmor >= 0 && Unit.ThermalArmor <= 100
			&& Unit.ArcArmor >= 0 && Unit.ArcArmor <= 100
			&& Unit.MaxMorale > 0 && Unit.MaxMorale <= 100
			&& Unit.CurrentMorale >= 0 && Unit.CurrentMorale <= Unit.MaxMorale
			&& Unit.Suppression >= 0 && Unit.Suppression <= 100
			&& bKnownStance;
		const bool bOccupiesCell = Unit.CurrentHealth > 0 && !Unit.bExtracted;
		if (!Unit.UnitId.IsValid() || SeenUnitIds.Contains(Unit.UnitId) || Index < 0
			|| (bOccupiesCell && OccupiedCells.Contains(Index))
			|| Terrain == nullptr || (Terrain != nullptr && Terrain->bBlocksMovement && Cell->CurrentIntegrity > 0 && !Cell->bDoorOpen)
			|| Unit.DisplayName.TrimStartAndEnd().IsEmpty() || !FContentPackageResolver::IsValidPackageId(Unit.SourceRuleId)
			|| !bAttributesValid)
		{
			AddError(OutDiagnostics, TEXT("invalid_tactical_unit"), TEXT("Tactical battlefield contains an invalid unit identity, position, rule, or attribute snapshot."));
			return false;
		}
		SeenUnitIds.Add(Unit.UnitId);
		if (bOccupiesCell)
		{
			OccupiedCells.Add(Index);
		}
		if (Unit.Team == ETacticalTeam::Player)
		{
			const FPersonnelState* Person = FindPersonnel(Campaign, Unit.PersonnelId);
			const bool bReceivesLegacyRelay = Person != nullptr
				&& LegacyRelayRecipientIds.Contains(Person->PersonnelId);
			const int32 ExpectedAccuracy = Person != nullptr
				? ClampPersonnelStatWithBonus(Person->Accuracy, bReceivesLegacyRelay ? LegacyRelay.AccuracyBonus : 0)
				: 0;
			const int32 ExpectedResolve = Person != nullptr
				? ClampPersonnelStatWithBonus(Person->Resolve, bReceivesLegacyRelay ? LegacyRelay.ResolveBonus : 0)
				: 0;
			const int32 ExpectedMobility = Person != nullptr
				? ClampPersonnelStatWithBonus(Person->Mobility, bReceivesLegacyRelay ? LegacyRelay.MobilityBonus : 0)
				: 0;
			const int32 ExpectedStrength = Person != nullptr
				? ClampPersonnelStatWithBonus(Person->Strength, bReceivesLegacyRelay ? LegacyRelay.StrengthBonus : 0)
				: 0;
			const int32 BondActionPointBonus = SquadBondActionPointBonuses.FindRef(Unit.PersonnelId);
			const int32 BondMoraleBonus = SquadBondMoraleBonuses.FindRef(Unit.PersonnelId);
			const int32 ExpectedBondedActionPoints = FMath::Clamp(
				6 + ExpectedMobility / 20 + BondActionPointBonus, 6, 20);
			const int32 MentorshipBonus = MentorshipRecipientIds.Contains(Unit.PersonnelId)
				? Mentorship.MoraleBonus
				: 0;
			const int32 ExpectedBondedMorale = FMath::Clamp(
				ExpectedResolve + MentorshipBonus + BondMoraleBonus, 0, 100);
			TSet<FName> SeenWeaponIds;
			int64 TacticalLoadoutCount = static_cast<int64>(Unit.WeaponStates.Num())
				+ static_cast<int64>(Unit.EjectedMagazines.Num());
			bool bLoadoutValid = Unit.WeaponStates.Num() <= 16 && Unit.CarriedItems.Num() <= 16
				&& Unit.EjectedMagazines.Num() <= 16;
			TMap<FName, int64> PersonnelEquipmentCounts;
			if (Person != nullptr)
			{
				for (const FName ItemId : Person->EquippedItems)
				{
					++PersonnelEquipmentCounts.FindOrAdd(ItemId);
				}
			}
			TMap<FName, int64> TacticalMagazineCounts;
			TSet<FName> TacticalAmmunitionItemIds;
			for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
			{
				const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
				const int32 EffectiveMagazineCapacity = Weapon != nullptr
					? GetEffectiveTacticalMagazineCapacity(*Weapon)
					: 0;
				bLoadoutValid &= Weapon != nullptr && Weapon->IsTacticalWeapon()
					&& !SeenWeaponIds.Contains(WeaponState.WeaponItemId)
					&& Person != nullptr && Person->EquippedItems.Contains(WeaponState.WeaponItemId)
					&& WeaponState.LoadedAmmunition >= 0
					&& (Weapon != nullptr && (Weapon->TacticalAmmunitionItemId.IsNone()
						? WeaponState.LoadedAmmunition == 0
						: EffectiveMagazineCapacity > 0
							&& WeaponState.LoadedAmmunition <= EffectiveMagazineCapacity));
				if (Weapon != nullptr && !Weapon->TacticalAmmunitionItemId.IsNone()
					&& WeaponState.LoadedAmmunition >= 0)
				{
					TacticalAmmunitionItemIds.Add(Weapon->TacticalAmmunitionItemId);
					if (WeaponState.LoadedAmmunition > 0)
					{
						++TacticalMagazineCounts.FindOrAdd(Weapon->TacticalAmmunitionItemId);
					}
				}
				SeenWeaponIds.Add(WeaponState.WeaponItemId);
			}
			for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
			{
				const FItemRule* Weapon = Rules.Items.Find(Magazine.WeaponItemId);
				const int32 EffectiveMagazineCapacity = Weapon != nullptr
					? GetEffectiveTacticalMagazineCapacity(*Weapon)
					: 0;
				bLoadoutValid &= Weapon != nullptr && Weapon->IsTacticalWeapon()
					&& SeenWeaponIds.Contains(Magazine.WeaponItemId)
					&& Weapon->TacticalAmmunitionItemId == Magazine.AmmunitionItemId
					&& Magazine.LoadedAmmunition > 0
					&& EffectiveMagazineCapacity > 0
					&& Magazine.LoadedAmmunition <= EffectiveMagazineCapacity;
				if (Magazine.LoadedAmmunition > 0)
				{
					++TacticalMagazineCounts.FindOrAdd(Magazine.AmmunitionItemId);
				}
			}
			TSet<FName> SeenCarriedItemIds;
			for (const FInventoryStack& Stack : Unit.CarriedItems)
			{
				bLoadoutValid &= Rules.Items.Contains(Stack.ItemId) && Stack.Quantity > 0
					&& !SeenCarriedItemIds.Contains(Stack.ItemId)
					&& (Person == nullptr || Stack.Quantity <= PersonnelEquipmentCounts.FindRef(Stack.ItemId));
				if (Stack.Quantity > 0)
				{
					TacticalLoadoutCount += Stack.Quantity;
					if (TacticalAmmunitionItemIds.Contains(Stack.ItemId))
					{
						TacticalMagazineCounts.FindOrAdd(Stack.ItemId) += Stack.Quantity;
					}
				}
				SeenCarriedItemIds.Add(Stack.ItemId);
			}
			for (const TPair<FName, int64>& MagazineCount : TacticalMagazineCounts)
			{
				bLoadoutValid &= Person != nullptr
					&& MagazineCount.Value <= PersonnelEquipmentCounts.FindRef(MagazineCount.Key);
			}
			bLoadoutValid &= TacticalLoadoutCount <= 16;
			if (Person == nullptr || !Operation->AgentIds.Contains(Unit.PersonnelId)
				|| PlayerPersonnelIds.Contains(Unit.PersonnelId)
				|| !bLoadoutValid
				|| (Person != nullptr && (Unit.SourceRuleId != Person->RoleId
					|| Unit.DisplayName != Person->DisplayName
					|| Unit.MaxHealth != Person->MaxHealth || Unit.CurrentHealth > Person->CurrentHealth
					|| Unit.Accuracy != ExpectedAccuracy || Unit.Resolve != ExpectedResolve
					|| Unit.Mobility != ExpectedMobility || Unit.Strength != ExpectedStrength
					|| (BondActionPointBonus > 0 && Unit.MaxActionPoints != ExpectedBondedActionPoints)
					|| (BondMoraleBonus > 0 && Unit.MaxMorale != ExpectedBondedMorale)))
				|| (Battle.Phase == ETacticalBattlePhase::Deployment && (!Cell->bPlayerDeployment || Unit.bExtracted))
				|| (Unit.bExtracted && (!Battle.bRequiresExtraction || Unit.CurrentHealth <= 0 || !Cell->bExtraction)))
			{
				AddError(OutDiagnostics, TEXT("invalid_tactical_player_unit"), TEXT("Player tactical unit does not match the deployed strategic roster or deployment zone."));
				return false;
			}
			PlayerPersonnelIds.Add(Unit.PersonnelId);
		}
		else if (Unit.Team == ETacticalTeam::Adversary)
		{
			const FTacticalUnitRule* UnitRule = Rules.TacticalUnits.Find(Unit.SourceRuleId);
			if (Unit.PersonnelId.IsValid() || Unit.bExtracted || UnitRule == nullptr
				|| Unit.SourceRuleId != Mission->AdversaryUnitRuleId
				|| Unit.DisplayName != UnitRule->DisplayName
				|| !Unit.WeaponStates.IsEmpty() || !Unit.CarriedItems.IsEmpty() || !Unit.EjectedMagazines.IsEmpty()
				|| (UnitRule != nullptr && (Unit.KineticArmor != UnitRule->KineticArmor
					|| Unit.ThermalArmor != UnitRule->ThermalArmor || Unit.ArcArmor != UnitRule->ArcArmor)))
			{
				AddError(OutDiagnostics, TEXT("invalid_tactical_adversary_unit"), TEXT("Adversary tactical unit has an invalid strategic identity or unit rule."));
				return false;
			}
			++AdversaryCount;
		}
		else
		{
			AddError(OutDiagnostics, TEXT("invalid_tactical_team"), TEXT("Tactical unit has an unknown team."));
			return false;
		}
	}
	FGuid PreviousLastKnownUnitId;
	bool bHasPreviousLastKnownUnitId = false;
	for (const FTacticalUnitMemoryState& Memory : Battle.PlayerLastKnownAdversaries)
	{
		const FTacticalUnitState* Unit = Battle.Units.FindByPredicate(
			[&Memory](const FTacticalUnitState& Entry) { return Entry.UnitId == Memory.UnitId; });
		const bool bKnownStance = Memory.Stance == ETacticalStance::Standing
			|| Memory.Stance == ETacticalStance::Crouched;
		const bool bSorted = !bHasPreviousLastKnownUnitId
			|| PreviousLastKnownUnitId.ToString(EGuidFormats::Digits) < Memory.UnitId.ToString(EGuidFormats::Digits);
		if (!Memory.UnitId.IsValid() || !bSorted || Unit == nullptr || Unit->Team != ETacticalTeam::Adversary
			|| Unit->CurrentHealth <= 0 || Unit->bExtracted
			|| !Battle.IsWithinGrid(Memory.X, Memory.Y, Memory.Z)
			|| Memory.SourceRuleId != Unit->SourceRuleId || Memory.DisplayName != Unit->DisplayName
			|| !bKnownStance || Memory.MaxHealth <= 0 || Memory.MaxHealth > 200
			|| Memory.CurrentHealth <= 0 || Memory.CurrentHealth > Memory.MaxHealth
			|| Memory.MaxMorale <= 0 || Memory.MaxMorale > 100
			|| Memory.CurrentMorale < 0 || Memory.CurrentMorale > Memory.MaxMorale
			|| Memory.Suppression < 0 || Memory.Suppression > 100
			|| Memory.LastSeenTurnNumber <= 0 || Memory.LastSeenTurnNumber > Battle.TurnNumber)
		{
			AddError(OutDiagnostics, TEXT("invalid_tactical_memory"), TEXT("Tactical last-known adversary memory is invalid or exposes an unknown unit."));
			return false;
		}
		PreviousLastKnownUnitId = Memory.UnitId;
		bHasPreviousLastKnownUnitId = true;
	}
	const int64 ExpectedAdversaries = static_cast<int64>(Mission->BaseEnemyCount)
		+ static_cast<int64>(Mission->EnemiesPerThreat) * ThreatRating;
	if (PlayerPersonnelIds.Num() != Operation->AgentIds.Num()
		|| static_cast<int64>(AdversaryCount) != ExpectedAdversaries)
	{
		AddError(OutDiagnostics, TEXT("invalid_tactical_population"), TEXT("Tactical player or adversary population does not match its operation and mission recipe."));
		return false;
	}

	if (Battle.Objectives.Num() != 1)
	{
		AddError(OutDiagnostics, TEXT("invalid_tactical_objective"), TEXT("Generated tactical battle requires exactly one primary objective."));
		return false;
	}
	const FTacticalObjectiveState& Objective = Battle.Objectives[0];
	const int32 ObjectiveIndex = Battle.IsWithinGrid(Objective.X, Objective.Y, Objective.Z)
		? Battle.GetCellIndex(Objective.X, Objective.Y, Objective.Z)
		: -1;
	const FTacticalTerrainRule* ObjectiveTerrain = ObjectiveIndex >= 0
		? Rules.TacticalTerrains.Find(Battle.Cells[ObjectiveIndex].TerrainRuleId)
		: nullptr;
	const bool bKnownObjectiveStatus = Objective.Status == ETacticalObjectiveStatus::Active
		|| Objective.Status == ETacticalObjectiveStatus::Completed
		|| Objective.Status == ETacticalObjectiveStatus::Failed;
	const bool bKnownObjectiveType = Objective.Type == ETacticalObjectiveType::Disrupt
		|| Objective.Type == ETacticalObjectiveType::Recover
		|| Objective.Type == ETacticalObjectiveType::Control;
	const bool bObjectiveProgressValid = Objective.RequiredInteractions > 0
		&& Objective.CompletedInteractions >= 0
		&& Objective.CompletedInteractions <= Objective.RequiredInteractions
		&& Objective.AdversaryInteractions >= 0
		&& Objective.AdversaryInteractions <= Objective.RequiredInteractions
		&& (Objective.Type == ETacticalObjectiveType::Control || Objective.AdversaryInteractions == 0)
		&& (Objective.CompletedInteractions == 0 || Objective.AdversaryInteractions == 0)
		&& (Objective.Status != ETacticalObjectiveStatus::Active
			|| (Objective.CompletedInteractions < Objective.RequiredInteractions
				&& Objective.AdversaryInteractions < Objective.RequiredInteractions))
		&& (Objective.Status != ETacticalObjectiveStatus::Completed
			|| (Objective.CompletedInteractions == Objective.RequiredInteractions
				&& Objective.AdversaryInteractions == 0));
	const bool bObjectivePhaseValid = Battle.bRequiresExtraction
		? (Battle.Phase == ETacticalBattlePhase::Resolved
			? Objective.Status != ETacticalObjectiveStatus::Active
			: Objective.Status != ETacticalObjectiveStatus::Failed)
		: (Battle.Phase == ETacticalBattlePhase::Resolved
			? Objective.Status != ETacticalObjectiveStatus::Active
			: Objective.Status == ETacticalObjectiveStatus::Active);
	if (Objective.ObjectiveId != Mission->ObjectiveId || Objective.Type != Mission->ObjectiveType
		|| Objective.RequiredInteractions != Mission->ObjectiveRequiredInteractions
		|| !bKnownObjectiveStatus || !bKnownObjectiveType
		|| !bObjectiveProgressValid || !bObjectivePhaseValid
		|| ObjectiveTerrain == nullptr || ObjectiveTerrain->bBlocksMovement)
	{
		AddError(OutDiagnostics, TEXT("invalid_tactical_objective"), TEXT("Generated tactical objective identity, position, status, or interaction state is invalid."));
		return false;
	}
	for (const FTacticalUnitState& Unit : Battle.Units)
	{
		if (Objective.Status == ETacticalObjectiveStatus::Active
			&& Unit.Team == ETacticalTeam::Player
			&& Unit.CurrentHealth > 0 && !Unit.bExtracted
			&& !IsReachable(Battle, Rules, Unit.X, Unit.Y, Unit.Z, Objective.X, Objective.Y, Objective.Z))
		{
			AddError(OutDiagnostics, TEXT("unreachable_tactical_objective"), FString::Printf(TEXT("Player unit '%s' cannot reach the primary objective."), *Unit.DisplayName));
			return false;
		}
	}
	return true;
}
