// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/TacticalPresentationService.h"

namespace TacticalPresentationPrivate
{
	void AddDiagnostic(TArray<FTacticalPresentationDiagnostic>& Diagnostics, const FName Code, FString Message)
	{
		FTacticalPresentationDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A)
		{
			return Left.A < Right.A;
		}
		if (Left.B != Right.B)
		{
			return Left.B < Right.B;
		}
		if (Left.C != Right.C)
		{
			return Left.C < Right.C;
		}
		return Left.D < Right.D;
	}

	const FTacticalUnitState* FindUnit(const FTacticalBattleState& Battle, const FGuid UnitId)
	{
		return Battle.Units.FindByPredicate(
			[&UnitId](const FTacticalUnitState& Unit) { return Unit.UnitId == UnitId; });
	}

	const FPersonnelState* FindPersonnel(const FCampaignState& Campaign, const FGuid PersonnelId)
	{
		return Campaign.Personnel.FindByPredicate(
			[&PersonnelId](const FPersonnelState& Person) { return Person.PersonnelId == PersonnelId; });
	}

	const FCraftState* FindCraft(const FCampaignState& Campaign, const FGuid CraftId)
	{
		return Campaign.Craft.FindByPredicate(
			[&CraftId](const FCraftState& Craft) { return Craft.CraftId == CraftId; });
	}

	const FStrategicBaseState* FindBase(const FCampaignState& Campaign, const FGuid BaseId)
	{
		return Campaign.Bases.FindByPredicate(
			[&BaseId](const FStrategicBaseState& Base) { return Base.BaseId == BaseId; });
	}

	const FTacticalOperationState* FindOperation(const FCampaignState& Campaign, const FGuid OperationId)
	{
		return Campaign.TacticalOperations.FindByPredicate(
			[&OperationId](const FTacticalOperationState& Operation) { return Operation.OperationId == OperationId; });
	}

	int32 ManhattanDistance(
		const int32 FromX,
		const int32 FromY,
		const int32 FromZ,
		const int32 ToX,
		const int32 ToY,
		const int32 ToZ)
	{
		const int64 DeltaX = static_cast<int64>(FromX) - ToX;
		const int64 DeltaY = static_cast<int64>(FromY) - ToY;
		const int64 DeltaZ = static_cast<int64>(FromZ) - ToZ;
		const int64 AbsX = DeltaX < 0 ? -DeltaX : DeltaX;
		const int64 AbsY = DeltaY < 0 ? -DeltaY : DeltaY;
		const int64 AbsZ = DeltaZ < 0 ? -DeltaZ : DeltaZ;
		return static_cast<int32>(FMath::Min<int64>(MAX_int32, AbsX + AbsY + AbsZ));
	}

	int32 FindItemQuantity(const TArray<FInventoryStack>& Inventory, const FName ItemId)
	{
		const FInventoryStack* Stack = Inventory.FindByPredicate(
			[ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		return Stack != nullptr ? FMath::Max(0, Stack->Quantity) : 0;
	}

	int32 GetEffectiveTacticalMagazineCapacity(const FItemRule& Weapon)
	{
		return Weapon.TacticalMagazineCapacity > 0
			? FMath::Clamp(Weapon.TacticalMagazineCapacity, 1, 200)
			: 0;
	}

	int32 GetEffectiveTacticalReloadActionPointCost(const FItemRule& Weapon)
	{
		return Weapon.TacticalReloadActionPointCost > 0
			? FMath::Clamp(Weapon.TacticalReloadActionPointCost, 1, 20)
			: 0;
	}

	int32 GetEffectiveTacticalMissionActionPointCost(const int32 ActionPointCost)
	{
		return ActionPointCost > 0
			? FMath::Clamp(ActionPointCost, 1, 20)
			: 0;
	}

	int32 GetEffectiveTacticalDoorActionPointCost(const int32 ActionPointCost)
	{
		return ActionPointCost > 0
			? FMath::Clamp(ActionPointCost, 1, 4)
			: 0;
	}

	int32 SaturatingNonNegativeAdd(const int32 Current, const int64 Contribution)
	{
		const int64 ClampedCurrent = FMath::Clamp<int64>(Current, 0, MAX_int32);
		if (Contribution <= 0 || Contribution > MAX_int32 - ClampedCurrent)
		{
			return Contribution <= 0 ? static_cast<int32>(ClampedCurrent) : MAX_int32;
		}
		return static_cast<int32>(ClampedCurrent + Contribution);
	}

	bool CanAddInventoryStack(
		const TArray<FInventoryStack>& Inventory,
		const FName ItemId,
		const int32 Quantity)
	{
		if (Quantity <= 0)
		{
			return false;
		}
		const FInventoryStack* Stack = Inventory.FindByPredicate(
			[ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		if (Stack == nullptr)
		{
			return Inventory.Num() < 64;
		}
		const int64 NewQuantity = static_cast<int64>(Stack->Quantity) + Quantity;
		return NewQuantity <= MAX_int32;
	}

	int32 FindBestReserveAmmunition(
		const FTacticalUnitState& Unit,
		const FName WeaponItemId,
		const FItemRule& Weapon)
	{
		const int32 EffectiveMagazineCapacity = GetEffectiveTacticalMagazineCapacity(Weapon);
		if (FindItemQuantity(Unit.CarriedItems, Weapon.TacticalAmmunitionItemId) > 0)
		{
			return EffectiveMagazineCapacity;
		}
		int32 Best = 0;
		for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
		{
			if (Magazine.WeaponItemId == WeaponItemId
				&& Magazine.AmmunitionItemId == Weapon.TacticalAmmunitionItemId)
			{
				Best = FMath::Max(Best, FMath::Clamp(Magazine.LoadedAmmunition, 0, EffectiveMagazineCapacity));
			}
		}
		return Best;
	}

	FTacticalHudItemView MakeItemView(const FInventoryStack& Stack, const FResolvedRuleSet& Rules)
	{
		FTacticalHudItemView View;
		View.ItemId = Stack.ItemId;
		View.Quantity = Stack.Quantity;
		if (const FItemRule* Item = Rules.Items.Find(Stack.ItemId))
		{
			View.DisplayName = Item->DisplayName;
			View.Category = Item->Category;
			View.UnitMass = Item->Mass;
			View.UnitSellValue = Item->SellValue;
		}
		else
		{
			View.DisplayName = Stack.ItemId.ToString();
		}
		return View;
	}

	void BuildItemViews(
		const TArray<FInventoryStack>& Inventory,
		const FResolvedRuleSet& Rules,
		TArray<FTacticalHudItemView>& OutViews)
	{
		OutViews.Reset();
		for (const FInventoryStack& Stack : Inventory)
		{
			if (!Stack.ItemId.IsNone() && Stack.Quantity > 0)
			{
				OutViews.Add(MakeItemView(Stack, Rules));
			}
		}
		OutViews.Sort(
			[](const FTacticalHudItemView& Left, const FTacticalHudItemView& Right)
			{
				return Left.ItemId.LexicalLess(Right.ItemId);
			});
	}

	int64 ComputeManifestMass(const TArray<FInventoryStack>& Inventory, const FResolvedRuleSet& Rules)
	{
		int64 Total = 0;
		for (const FInventoryStack& Stack : Inventory)
		{
			const FItemRule* Item = Rules.Items.Find(Stack.ItemId);
			if (Item == nullptr || Stack.Quantity <= 0 || Item->Mass < 0)
			{
				continue;
			}
			const int64 Amount = static_cast<int64>(Stack.Quantity) * Item->Mass;
			Total = Amount > MAX_int64 - Total ? MAX_int64 : Total + Amount;
		}
		return Total;
	}

	void SetUnavailable(
		FTacticalHudActionAvailability& Action,
		const FName Code,
		FString Message)
	{
		Action.bAvailable = false;
		Action.UnavailableReasonCode = Code;
		Action.UnavailableReason = MoveTemp(Message);
	}

	void SetAvailable(FTacticalHudActionAvailability& Action)
	{
		Action.bAvailable = true;
		Action.UnavailableReasonCode = NAME_None;
		Action.UnavailableReason.Reset();
	}

	FTacticalHudActionAvailability& AddAction(
		FTacticalHudSnapshot& Snapshot,
		const ETacticalHudActionType Type,
		const FTacticalHudQuery& Query)
	{
		FTacticalHudActionAvailability& Action = Snapshot.Actions.AddDefaulted_GetRef();
		Action.ActionType = Type;
		Action.TargetX = Query.HoveredX;
		Action.TargetY = Query.HoveredY;
		Action.TargetZ = Query.HoveredZ;
		Action.FireMode = Query.FireMode;
		SetUnavailable(Action, TEXT("unavailable"), TEXT("This tactical action is unavailable."));
		return Action;
	}

	FName FirstPathDiagnosticCode(const FTacticalPathResult& Path)
	{
		return Path.Diagnostics.IsEmpty() ? FName(TEXT("path_unavailable")) : Path.Diagnostics[0].Code;
	}

	FString FirstPathDiagnosticMessage(const FTacticalPathResult& Path)
	{
		return Path.Diagnostics.IsEmpty()
			? FString(TEXT("No tactical path reaches the requested cell."))
			: Path.Diagnostics[0].Message;
	}

	FName FirstCombatDiagnosticCode(const FTacticalAttackPreview& Preview)
	{
		return Preview.Diagnostics.IsEmpty() ? FName(TEXT("attack_unavailable")) : Preview.Diagnostics[0].Code;
	}

	FString FirstCombatDiagnosticMessage(const FTacticalAttackPreview& Preview)
	{
		return Preview.Diagnostics.IsEmpty()
			? FString(TEXT("The selected tactical attack is unavailable."))
			: Preview.Diagnostics[0].Message;
	}

	FName FirstCombatDiagnosticCode(const FTacticalSignalPreview& Preview)
	{
		return Preview.Diagnostics.IsEmpty() ? FName(TEXT("signal_projection_unavailable")) : Preview.Diagnostics[0].Code;
	}

	FString FirstCombatDiagnosticMessage(const FTacticalSignalPreview& Preview)
	{
		return Preview.Diagnostics.IsEmpty()
			? FString(TEXT("The selected signal projection is unavailable."))
			: Preview.Diagnostics[0].Message;
	}

	bool RequireControllablePlayerUnit(
		const FTacticalBattleState& Battle,
		const FTacticalUnitState* Unit,
		FTacticalHudActionAvailability& Action)
	{
		if (Unit == nullptr)
		{
			SetUnavailable(Action, TEXT("player_unit_selection_required"), TEXT("Select a player unit to use this action."));
			return false;
		}
		if (Unit->Team != ETacticalTeam::Player)
		{
			SetUnavailable(Action, TEXT("player_unit_required"), TEXT("Only a player unit can use this HUD action."));
			return false;
		}
		if (Unit->CurrentHealth <= 0 || Unit->bExtracted)
		{
			SetUnavailable(Action, TEXT("invalid_tactical_unit"), TEXT("The selected unit is incapacitated or extracted."));
			return false;
		}
		if (!Battle.IsWithinGrid(Unit->X, Unit->Y, Unit->Z))
		{
			SetUnavailable(Action, TEXT("invalid_tactical_unit"), TEXT("The selected unit is outside the battlefield."));
			return false;
		}
		if (Battle.Phase == ETacticalBattlePhase::Deployment)
		{
			SetUnavailable(Action, TEXT("tactical_deployment_unconfirmed"), TEXT("Confirm deployment before issuing this action."));
			return false;
		}
		if (Battle.Phase == ETacticalBattlePhase::Resolved)
		{
			SetUnavailable(Action, TEXT("tactical_battle_resolved"), TEXT("The tactical battle is already resolved."));
			return false;
		}
		if (Battle.Phase != ETacticalBattlePhase::PlayerTurn || Battle.ActiveTeam != ETacticalTeam::Player)
		{
			SetUnavailable(Action, TEXT("player_turn_required"), TEXT("This action is available only during the player turn."));
			return false;
		}
		return true;
	}

	TArray<FName> FindPlayerWeaponIds(
		const FTacticalUnitState& Unit,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules)
	{
		TArray<FName> WeaponIds;
		if (const FPersonnelState* Person = FindPersonnel(Campaign, Unit.PersonnelId))
		{
			for (const FName ItemId : Person->EquippedItems)
			{
				const FItemRule* Item = Rules.Items.Find(ItemId);
				if (Item != nullptr && Item->IsTacticalWeapon())
				{
					WeaponIds.AddUnique(ItemId);
				}
			}
		}
		for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
		{
			const FItemRule* Item = Rules.Items.Find(WeaponState.WeaponItemId);
			if (Item != nullptr && Item->IsTacticalWeapon())
			{
				WeaponIds.AddUnique(WeaponState.WeaponItemId);
			}
		}
		WeaponIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
		return WeaponIds;
	}

	TArray<FName> FindPlayerDeviceIds(const FTacticalUnitState& Unit, const FResolvedRuleSet& Rules)
	{
		TArray<FName> DeviceIds;
		for (const FInventoryStack& Stack : Unit.CarriedItems)
		{
			const FItemRule* Item = Rules.Items.Find(Stack.ItemId);
			if (Stack.Quantity > 0 && Item != nullptr && Item->IsTacticalDevice())
			{
				DeviceIds.AddUnique(Stack.ItemId);
			}
		}
		DeviceIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
		return DeviceIds;
	}

	TArray<FName> FindPlayerSignalProjectorIds(const FTacticalUnitState& Unit, const FResolvedRuleSet& Rules)
	{
		TArray<FName> ProjectorIds;
		for (const FInventoryStack& Stack : Unit.CarriedItems)
		{
			const FItemRule* Item = Rules.Items.Find(Stack.ItemId);
			if (Stack.Quantity > 0 && Item != nullptr && Item->IsTacticalSignalProjector())
			{
				ProjectorIds.AddUnique(Stack.ItemId);
			}
		}
		ProjectorIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
		return ProjectorIds;
	}

	FTacticalHudUnitView MakeUnitView(
		const FTacticalUnitState& Unit,
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FGuid SelectedUnitId)
	{
		FTacticalHudUnitView View;
		View.UnitId = Unit.UnitId;
		View.PersonnelId = Unit.PersonnelId;
		View.SourceRuleId = Unit.SourceRuleId;
		View.DisplayName = Unit.DisplayName;
		View.Team = Unit.Team;
		View.Stance = Unit.Stance;
		View.X = Unit.X;
		View.Y = Unit.Y;
		View.Z = Unit.Z;
		View.CurrentHealth = Unit.CurrentHealth;
		View.MaxHealth = Unit.MaxHealth;
		View.RemainingActionPoints = Unit.RemainingActionPoints;
		View.MaxActionPoints = Unit.MaxActionPoints;
		View.CurrentMorale = Unit.CurrentMorale;
		View.MaxMorale = Unit.MaxMorale;
		View.Suppression = Unit.Suppression;
		View.bSelected = Unit.UnitId == SelectedUnitId;
		View.bCurrentlyVisible = true;
		View.bControllable = Unit.Team == ETacticalTeam::Player
			&& Unit.CurrentHealth > 0 && !Unit.bExtracted
			&& Battle.Phase == ETacticalBattlePhase::PlayerTurn
			&& Battle.ActiveTeam == ETacticalTeam::Player;
		View.bIncapacitated = Unit.CurrentHealth <= 0;
		View.bExtracted = Unit.bExtracted;
		if (Unit.Team != ETacticalTeam::Player)
		{
			return View;
		}

		BuildItemViews(Unit.CarriedItems, Rules, View.CarriedItems);
		for (const FName WeaponId : FindPlayerWeaponIds(Unit, Campaign, Rules))
		{
			const FItemRule* Weapon = Rules.Items.Find(WeaponId);
			if (Weapon == nullptr)
			{
				continue;
			}
			const FTacticalWeaponState* WeaponState = Unit.WeaponStates.FindByPredicate(
				[WeaponId](const FTacticalWeaponState& State) { return State.WeaponItemId == WeaponId; });
			const int32 EffectiveMagazineCapacity = GetEffectiveTacticalMagazineCapacity(*Weapon);
			const int32 EffectiveReloadActionPointCost = GetEffectiveTacticalReloadActionPointCost(*Weapon);
			const int32 EffectiveWeaponRange = FMath::Clamp(Weapon->TacticalRange, 1, 64);
			const int32 EffectiveWeaponActionPointCost = FMath::Clamp(Weapon->TacticalActionPointCost, 1, 20);
			const int32 EffectiveBurstActionPointCost = Weapon->HasTacticalBurstMode()
				? FMath::Clamp(Weapon->TacticalBurstActionPointCost, 1, 20)
				: 0;
			FTacticalHudWeaponView& WeaponView = View.Weapons.AddDefaulted_GetRef();
			WeaponView.ItemId = WeaponId;
			WeaponView.DisplayName = Weapon->DisplayName;
			WeaponView.Range = EffectiveWeaponRange;
			WeaponView.SingleActionPointCost = EffectiveWeaponActionPointCost;
			WeaponView.bSupportsBurst = Weapon->HasTacticalBurstMode();
			WeaponView.BurstActionPointCost = EffectiveBurstActionPointCost;
			WeaponView.LoadedAmmunition = WeaponState != nullptr && EffectiveMagazineCapacity > 0
				? FMath::Clamp(WeaponState->LoadedAmmunition, 0, EffectiveMagazineCapacity)
				: 0;
			WeaponView.MagazineCapacity = EffectiveMagazineCapacity;
			WeaponView.AmmunitionItemId = Weapon->TacticalAmmunitionItemId;
			const int32 CarriedFullMagazines = FindItemQuantity(Unit.CarriedItems, Weapon->TacticalAmmunitionItemId);
			WeaponView.FullReserveMagazines = CarriedFullMagazines;
			WeaponView.ReserveAmmunition = SaturatingNonNegativeAdd(
				0,
				static_cast<int64>(CarriedFullMagazines) * EffectiveMagazineCapacity);
			for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
			{
				if (Magazine.WeaponItemId != WeaponId
					|| Magazine.AmmunitionItemId != Weapon->TacticalAmmunitionItemId)
				{
					continue;
				}
				const int32 EffectiveLoadedAmmunition = FMath::Clamp(
					Magazine.LoadedAmmunition, 0, EffectiveMagazineCapacity);
				WeaponView.ReserveAmmunition = SaturatingNonNegativeAdd(
					WeaponView.ReserveAmmunition, EffectiveLoadedAmmunition);
				if (EffectiveLoadedAmmunition >= EffectiveMagazineCapacity)
				{
					WeaponView.FullReserveMagazines = SaturatingNonNegativeAdd(
						WeaponView.FullReserveMagazines, 1);
				}
				else
				{
					WeaponView.PartialReserveMagazines = SaturatingNonNegativeAdd(
						WeaponView.PartialReserveMagazines, 1);
				}
			}
			WeaponView.ReserveMagazines = SaturatingNonNegativeAdd(
				WeaponView.FullReserveMagazines, WeaponView.PartialReserveMagazines);
			WeaponView.NextReloadAmmunition = FindBestReserveAmmunition(Unit, WeaponId, *Weapon);
			WeaponView.ReloadActionPointCost = EffectiveReloadActionPointCost;
		}
		return View;
	}

	FTacticalHudUnitView MakeLastKnownUnitView(const FTacticalUnitMemoryState& Memory)
	{
		FTacticalHudUnitView View;
		View.UnitId = Memory.UnitId;
		View.SourceRuleId = Memory.SourceRuleId;
		View.DisplayName = Memory.DisplayName;
		View.Team = ETacticalTeam::Adversary;
		View.Stance = Memory.Stance;
		View.X = Memory.X;
		View.Y = Memory.Y;
		View.Z = Memory.Z;
		View.CurrentHealth = Memory.CurrentHealth;
		View.MaxHealth = Memory.MaxHealth;
		View.CurrentMorale = Memory.CurrentMorale;
		View.MaxMorale = Memory.MaxMorale;
		View.Suppression = Memory.Suppression;
		View.bCurrentlyVisible = false;
		View.bLastKnown = true;
		View.LastSeenTurnNumber = Memory.LastSeenTurnNumber;
		View.bControllable = false;
		View.bIncapacitated = false;
		View.bExtracted = false;
		return View;
	}

	const FStrategicEvent* FindResolutionEvent(
		const FStrategicCommandResult& Resolution,
		const FGuid OperationId)
	{
		return Resolution.Events.FindByPredicate(
			[&OperationId](const FStrategicEvent& Event)
			{
				return Event.Type == EStrategicEventType::TacticalOperationResolved
					&& Event.OperationId == OperationId;
			});
	}

	const FStrategicEvent* FindPersonnelEvent(
		const FStrategicCommandResult& Resolution,
		const EStrategicEventType Type,
		const FGuid PersonnelId)
	{
		return Resolution.Events.FindByPredicate(
			[Type, &PersonnelId](const FStrategicEvent& Event)
			{
				return Event.Type == Type && Event.PersonnelId == PersonnelId;
			});
	}
}

bool FTacticalHudSnapshot::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalPresentationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

const FTacticalHudActionAvailability* FTacticalHudSnapshot::FindAction(const ETacticalHudActionType ActionType) const
{
	return Actions.FindByPredicate(
		[ActionType](const FTacticalHudActionAvailability& Action) { return Action.ActionType == ActionType; });
}

bool FTacticalDebriefView::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalPresentationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FTacticalHudSnapshot FTacticalPresentationService::BuildHudSnapshot(
	const FTacticalBattleState& Battle,
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FTacticalHudQuery& Query)
{
	using namespace TacticalPresentationPrivate;

	FTacticalHudSnapshot Snapshot;
	Snapshot.BattleId = Battle.BattleId;
	Snapshot.OperationId = Battle.OperationId;
	Snapshot.bRequiresExtraction = Battle.bRequiresExtraction;
	Snapshot.ExpectedCommandSequence = Campaign.CommandSequence;
	Snapshot.MissionRuleId = Battle.MissionRuleId;
	Snapshot.Width = Battle.Width;
	Snapshot.Height = Battle.Height;
	Snapshot.Levels = Battle.Levels;
	Snapshot.ViewedLevel = Query.ViewedLevel;
	Snapshot.TurnNumber = Battle.TurnNumber;
	Snapshot.TurnLimit = Battle.TurnLimit;
	Snapshot.Phase = Battle.Phase;
	Snapshot.ActiveTeam = Battle.ActiveTeam;
	Snapshot.WindDirection = Battle.WindDirection;
	Snapshot.WindStrength = Battle.WindStrength;
	const FTacticalOperationState* Operation = FindOperation(Campaign, Battle.OperationId);
	if (Operation != nullptr)
	{
		Snapshot.OperationType = Operation->Type;
		Snapshot.BaseId = Operation->BaseId;
		Snapshot.AssaultId = Operation->AssaultId;
		Snapshot.Mentorship = FPersonnelMentorship::Evaluate(Campaign, Operation->AgentIds);
		Snapshot.LegacyRelay = FPersonnelLegacyRelay::Evaluate(Campaign, Rules, Operation->AgentIds);
		Snapshot.SquadBonds = FPersonnelSquadBond::Evaluate(Campaign, Operation->AgentIds);
		if (const FStrategicBaseState* Base = FindBase(Campaign, Operation->BaseId))
		{
			Snapshot.BaseDisplayName = Base->Name;
		}
	}

	const FTacticalMissionRule* Mission = Rules.TacticalMissions.Find(Battle.MissionRuleId);
	if (Mission == nullptr)
	{
		AddDiagnostic(Snapshot.Diagnostics, TEXT("unknown_tactical_mission"), TEXT("The battlefield references a tactical mission rule that is not loaded."));
		return Snapshot;
	}
	Snapshot.MissionDisplayName = Mission->DisplayName;
	if (Query.ViewedLevel < 0 || Query.ViewedLevel >= Battle.Levels)
	{
		AddDiagnostic(Snapshot.Diagnostics, TEXT("invalid_tactical_level"), TEXT("The requested HUD level is outside the battlefield."));
		return Snapshot;
	}

	const FTacticalVisibilityResult Visibility = FTacticalNavigationService::ComputePlayerVisibility(Battle, Rules);
	if (!Visibility.bSucceeded)
	{
		for (const FTacticalNavigationDiagnostic& Diagnostic : Visibility.Diagnostics)
		{
			AddDiagnostic(Snapshot.Diagnostics, Diagnostic.Code, Diagnostic.Message);
		}
		return Snapshot;
	}
	Snapshot.VisibleCellCount = Visibility.VisibleCellIndices.Num();
	TSet<int32> KnownCellIndexSet;
	TSet<int32> CurrentVisibleCellIndexSet;
	KnownCellIndexSet.Reserve(Battle.PlayerDiscoveredCellIndices.Num() + Visibility.VisibleCellIndices.Num());
	CurrentVisibleCellIndexSet.Reserve(Visibility.VisibleCellIndices.Num());
	int32 PreviousDiscoveredCellIndex = INDEX_NONE;
	for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
	{
		if (!Battle.Cells.IsValidIndex(CellIndex) || CellIndex <= PreviousDiscoveredCellIndex)
		{
			AddDiagnostic(Snapshot.Diagnostics, TEXT("invalid_tactical_discovery"), TEXT("Tactical discovery indices must be unique, sorted, and inside the grid."));
			return Snapshot;
		}
		KnownCellIndexSet.Add(CellIndex);
		PreviousDiscoveredCellIndex = CellIndex;
	}
	for (const int32 CellIndex : Visibility.VisibleCellIndices)
	{
		KnownCellIndexSet.Add(CellIndex);
		CurrentVisibleCellIndexSet.Add(CellIndex);
	}
	TArray<int32> KnownCellIndices = KnownCellIndexSet.Array();
	KnownCellIndices.Sort();
	Snapshot.KnownCellCount = KnownCellIndices.Num();
	for (const int32 CellIndex : KnownCellIndices)
	{
		if (!Battle.Cells.IsValidIndex(CellIndex) || Battle.Cells[CellIndex].Z != Query.ViewedLevel)
		{
			continue;
		}
		const FTacticalCellState& Cell = Battle.Cells[CellIndex];
		FTacticalHudCellView& View = Snapshot.KnownCells.AddDefaulted_GetRef();
		View.CellIndex = CellIndex;
		View.X = Cell.X;
		View.Y = Cell.Y;
		View.Z = Cell.Z;
		View.bCurrentlyVisible = CurrentVisibleCellIndexSet.Contains(CellIndex);
		if (!View.bCurrentlyVisible)
		{
			continue;
		}
		const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
		if (Terrain == nullptr)
		{
			AddDiagnostic(Snapshot.Diagnostics, TEXT("unknown_tactical_terrain"), TEXT("A visible battlefield cell references an unloaded terrain rule."));
			return Snapshot;
		}
		View.TerrainRuleId = Cell.TerrainRuleId;
		View.TerrainDisplayName = Terrain->DisplayName;
		View.CurrentIntegrity = Cell.CurrentIntegrity;
		View.MaxIntegrity = Terrain->MaxIntegrity;
		View.MoveCost = FMath::Clamp(Terrain->MoveCost, 1, 20);
		View.CoverPercent = Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen
			? FMath::Clamp(Terrain->CoverPercent, 0, 100)
			: 0;
		View.Smoke = Cell.Smoke;
		View.Fire = Cell.Fire;
		View.bBlocksMovement = Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen && Terrain->bBlocksMovement;
		View.bBlocksVision = Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen && Terrain->bBlocksVision;
		View.bPlayerDeployment = Cell.bPlayerDeployment;
		View.bExtraction = Cell.bExtraction;
		View.bIsDoor = Terrain->IsDoor();
		View.bDoorOpen = Cell.bDoorOpen;
		View.bIsVerticalConnector = Terrain->IsVerticalConnector();
		Snapshot.VisibleCells.Add(View);
	}

	const FTacticalUnitState* SelectedUnit = nullptr;
	for (const FTacticalUnitState& Unit : Battle.Units)
	{
		const bool bPlayer = Unit.Team == ETacticalTeam::Player;
		const bool bVisibleHostile = Unit.Team == ETacticalTeam::Adversary && Visibility.IsUnitVisible(Unit.UnitId);
		if (!bPlayer && !bVisibleHostile)
		{
			continue;
		}
		if (bPlayer && Unit.CurrentHealth > 0 && !Unit.bExtracted)
		{
			++Snapshot.LivingPlayerUnitCount;
		}
		if (bVisibleHostile)
		{
			++Snapshot.VisibleAdversaryUnitCount;
		}
		Snapshot.Units.Add(MakeUnitView(Unit, Battle, Campaign, Rules, Query.SelectedUnitId));
		if (Unit.UnitId == Query.SelectedUnitId)
		{
			SelectedUnit = &Unit;
		}
	}
	const bool bSelectedUnitOnGrid = SelectedUnit != nullptr
		&& Battle.IsWithinGrid(SelectedUnit->X, SelectedUnit->Y, SelectedUnit->Z);
	FGuid PreviousLastKnownUnitId;
	bool bHasPreviousLastKnownUnitId = false;
	for (const FTacticalUnitMemoryState& Memory : Battle.PlayerLastKnownAdversaries)
	{
		const FTacticalUnitState* Unit = FindUnit(Battle, Memory.UnitId);
		const bool bKnownStance = Memory.Stance == ETacticalStance::Standing
			|| Memory.Stance == ETacticalStance::Crouched;
		const bool bSorted = !bHasPreviousLastKnownUnitId
			|| GuidLess(PreviousLastKnownUnitId, Memory.UnitId);
		if (!Memory.UnitId.IsValid() || !bSorted || Unit == nullptr || Unit->Team != ETacticalTeam::Adversary
			|| Unit->CurrentHealth <= 0 || Unit->bExtracted
			|| !Battle.IsWithinGrid(Memory.X, Memory.Y, Memory.Z)
			|| Memory.SourceRuleId != Unit->SourceRuleId || Memory.DisplayName != Unit->DisplayName
			|| !bKnownStance || Memory.MaxHealth <= 0 || Memory.MaxHealth > 200 || Memory.CurrentHealth <= 0 || Memory.CurrentHealth > Memory.MaxHealth
			|| Memory.MaxMorale <= 0 || Memory.MaxMorale > 100 || Memory.CurrentMorale < 0 || Memory.CurrentMorale > Memory.MaxMorale
			|| Memory.Suppression < 0 || Memory.Suppression > 100
			|| Memory.LastSeenTurnNumber <= 0 || Memory.LastSeenTurnNumber > Battle.TurnNumber)
		{
			AddDiagnostic(Snapshot.Diagnostics, TEXT("invalid_tactical_memory"), TEXT("Tactical last-known adversary memory is invalid or exposes an unknown unit."));
			return Snapshot;
		}
		PreviousLastKnownUnitId = Memory.UnitId;
		bHasPreviousLastKnownUnitId = true;
		if (Visibility.IsUnitVisible(Memory.UnitId))
		{
			continue;
		}
		Snapshot.Units.Add(MakeLastKnownUnitView(Memory));
		++Snapshot.LastKnownAdversaryUnitCount;
	}
	Snapshot.Units.Sort(
		[](const FTacticalHudUnitView& Left, const FTacticalHudUnitView& Right)
		{
			if (Left.Team != Right.Team)
			{
				return Left.Team == ETacticalTeam::Player;
			}
			return GuidLess(Left.UnitId, Right.UnitId);
		});

	for (const FTacticalObjectiveState& Objective : Battle.Objectives)
	{
		FTacticalHudObjectiveView& View = Snapshot.Objectives.AddDefaulted_GetRef();
		View.ObjectiveId = Objective.ObjectiveId;
		View.Type = Objective.Type;
		View.Status = Objective.Status;
		View.X = Objective.X;
		View.Y = Objective.Y;
		View.Z = Objective.Z;
		View.PlayerInteractions = Objective.CompletedInteractions;
		View.AdversaryInteractions = Objective.AdversaryInteractions;
		View.RequiredInteractions = Objective.RequiredInteractions;
		if (Objective.Type == ETacticalObjectiveType::Recover)
		{
			View.RewardItemId = Mission->ObjectiveRewardItemId;
			View.RewardQuantity = Mission->ObjectiveRewardQuantity;
			if (const FItemRule* Reward = Rules.Items.Find(View.RewardItemId))
			{
				View.RewardDisplayName = Reward->DisplayName;
			}
		}
	}
	Snapshot.Objectives.Sort(
		[](const FTacticalHudObjectiveView& Left, const FTacticalHudObjectiveView& Right)
		{
			return Left.ObjectiveId.LexicalLess(Right.ObjectiveId);
		});

	const FCraftState* Craft = Operation != nullptr ? FindCraft(Campaign, Operation->CraftId) : nullptr;
	const TArray<FInventoryStack>& Manifest = Craft != nullptr ? Craft->Cargo : Battle.Cargo;
	BuildItemViews(Manifest, Rules, Snapshot.Cargo);
	Snapshot.CargoMass = ComputeManifestMass(Manifest, Rules);
	if (Craft != nullptr)
	{
		if (const FCraftRule* CraftRule = Rules.Craft.Find(Craft->CraftRuleId))
		{
			Snapshot.CargoCapacity = CraftRule->CargoCapacity;
		}
	}

	TArray<FName> WeaponIds;
	TArray<FName> DeviceIds;
	TArray<FName> SignalProjectorIds;
	if (SelectedUnit != nullptr && SelectedUnit->Team == ETacticalTeam::Player)
	{
		WeaponIds = FindPlayerWeaponIds(*SelectedUnit, Campaign, Rules);
		DeviceIds = FindPlayerDeviceIds(*SelectedUnit, Rules);
		SignalProjectorIds = FindPlayerSignalProjectorIds(*SelectedUnit, Rules);
	}
	Snapshot.EffectiveWeaponItemId = !Query.SelectedWeaponItemId.IsNone()
		? Query.SelectedWeaponItemId
		: (WeaponIds.IsEmpty() ? NAME_None : WeaponIds[0]);
	Snapshot.EffectiveDeviceItemId = !Query.SelectedDeviceItemId.IsNone()
		? Query.SelectedDeviceItemId
		: (DeviceIds.IsEmpty() ? NAME_None : DeviceIds[0]);
	Snapshot.EffectiveSignalProjectorItemId = SignalProjectorIds.IsEmpty() ? NAME_None : SignalProjectorIds[0];

	Snapshot.Hover.bHasCell = Query.bHasHoveredCell;
	Snapshot.Hover.X = Query.HoveredX;
	Snapshot.Hover.Y = Query.HoveredY;
	Snapshot.Hover.Z = Query.HoveredZ;
	Snapshot.Hover.bCellVisible = Query.bHasHoveredCell
		&& Battle.IsWithinGrid(Query.HoveredX, Query.HoveredY, Query.HoveredZ)
		&& Visibility.IsCellVisible(Query.HoveredX, Query.HoveredY, Query.HoveredZ);
	if (SelectedUnit != nullptr && SelectedUnit->Team == ETacticalTeam::Player && Snapshot.Hover.bCellVisible)
	{
		Snapshot.Hover.bHasPathPreview = true;
		Snapshot.Hover.Path = FTacticalNavigationService::FindPath(
			Battle, Rules, SelectedUnit->UnitId, Query.HoveredX, Query.HoveredY, Query.HoveredZ);
	}

	const FTacticalUnitState* HoveredUnit = Visibility.IsUnitVisible(Query.HoveredUnitId)
		? FindUnit(Battle, Query.HoveredUnitId)
		: nullptr;
	const bool bVisibleHostileTarget = SelectedUnit != nullptr && HoveredUnit != nullptr
		&& HoveredUnit->Team != SelectedUnit->Team;
	if (bVisibleHostileTarget && !Snapshot.EffectiveWeaponItemId.IsNone())
	{
		Snapshot.Hover.bHasUnitAttackPreview = true;
		Snapshot.Hover.UnitAttack = FTacticalCombatService::PreviewUnitAttack(
			Battle,
			Campaign,
			Rules,
			SelectedUnit->UnitId,
			HoveredUnit->UnitId,
			Snapshot.EffectiveWeaponItemId,
			Query.FireMode);
	}
	if (bVisibleHostileTarget && !Snapshot.EffectiveSignalProjectorItemId.IsNone())
	{
		Snapshot.Hover.bHasSignalPreview = true;
		Snapshot.Hover.Signal = FTacticalCombatService::PreviewSignalProjection(
			Battle,
			Campaign,
			Rules,
			SelectedUnit->UnitId,
			HoveredUnit->UnitId,
			Snapshot.EffectiveSignalProjectorItemId);
	}
	if (SelectedUnit != nullptr && Snapshot.Hover.bCellVisible && !Snapshot.EffectiveWeaponItemId.IsNone())
	{
		Snapshot.Hover.bHasTerrainAttackPreview = true;
		Snapshot.Hover.TerrainAttack = FTacticalCombatService::PreviewTerrainAttack(
			Battle,
			Campaign,
			Rules,
			SelectedUnit->UnitId,
			Query.HoveredX,
			Query.HoveredY,
			Snapshot.EffectiveWeaponItemId,
			Query.FireMode,
			Query.HoveredZ);
	}
	const FItemRule* SelectedDevice = Rules.Items.Find(Snapshot.EffectiveDeviceItemId);
	if (SelectedUnit != nullptr && bSelectedUnitOnGrid && Snapshot.Hover.bCellVisible
		&& SelectedDevice != nullptr && SelectedDevice->HasTacticalThrowArc())
	{
		Snapshot.Hover.bHasDeviceTrajectory = true;
		Snapshot.Hover.DeviceTrajectory = FTacticalNavigationService::PreviewThrowTrajectory(
			Battle,
			Rules,
			SelectedUnit->X,
			SelectedUnit->Y,
			Query.HoveredX,
			Query.HoveredY,
			FMath::Clamp(SelectedDevice->TacticalThrowArcHeight, 1, 8),
			SelectedUnit->Z,
			Query.HoveredZ);
	}

	FTacticalHudActionAvailability& Confirm = AddAction(Snapshot, ETacticalHudActionType::ConfirmDeployment, Query);
	if (Battle.Phase == ETacticalBattlePhase::Deployment)
	{
		SetAvailable(Confirm);
	}
	else
	{
		SetUnavailable(Confirm, TEXT("tactical_deployment_already_confirmed"), TEXT("Deployment has already been confirmed."));
	}

	FTacticalHudActionAvailability& Move = AddAction(Snapshot, ETacticalHudActionType::Move, Query);
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Move))
	{
		if (!Query.bHasHoveredCell)
		{
			SetUnavailable(Move, TEXT("tactical_target_required"), TEXT("Point to a visible destination cell."));
		}
		else if (!Snapshot.Hover.bCellVisible)
		{
			SetUnavailable(Move, TEXT("tactical_target_not_visible"), TEXT("Movement previews do not expose unseen battlefield cells."));
		}
		else if (!Snapshot.Hover.bHasPathPreview || !Snapshot.Hover.Path.bSucceeded)
		{
			SetUnavailable(Move, FirstPathDiagnosticCode(Snapshot.Hover.Path), FirstPathDiagnosticMessage(Snapshot.Hover.Path));
		}
		else if (Snapshot.Hover.Path.Steps.IsEmpty())
		{
			SetUnavailable(Move, TEXT("already_at_tactical_destination"), TEXT("The selected unit already occupies this cell."));
		}
		else
		{
			Move.ActionPointCost = Snapshot.Hover.Path.TotalCost;
			if (Move.ActionPointCost > SelectedUnit->RemainingActionPoints)
			{
				SetUnavailable(Move, TEXT("insufficient_action_points"), TEXT("The selected path costs more action points than the unit has remaining."));
			}
			else
			{
				SetAvailable(Move);
			}
		}
	}

	FTacticalHudActionAvailability& AttackUnit = AddAction(Snapshot, ETacticalHudActionType::AttackUnit, Query);
	AttackUnit.ItemId = Snapshot.EffectiveWeaponItemId;
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, AttackUnit))
	{
		if (!bVisibleHostileTarget)
		{
			SetUnavailable(AttackUnit, TEXT("visible_hostile_target_required"), TEXT("Point to a currently visible hostile unit."));
		}
		else if (Snapshot.EffectiveWeaponItemId.IsNone())
		{
			SetUnavailable(AttackUnit, TEXT("tactical_weapon_required"), TEXT("Select a carried tactical weapon."));
		}
		else if (!Snapshot.Hover.bHasUnitAttackPreview || !Snapshot.Hover.UnitAttack.bSucceeded)
		{
			SetUnavailable(AttackUnit, FirstCombatDiagnosticCode(Snapshot.Hover.UnitAttack), FirstCombatDiagnosticMessage(Snapshot.Hover.UnitAttack));
		}
		else
		{
			AttackUnit.ActionPointCost = Snapshot.Hover.UnitAttack.ActionPointCost;
			SetAvailable(AttackUnit);
		}
	}

	FTacticalHudActionAvailability& Signal = AddAction(Snapshot, ETacticalHudActionType::ProjectSignal, Query);
	Signal.ItemId = Snapshot.EffectiveSignalProjectorItemId;
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Signal))
	{
		if (!bVisibleHostileTarget)
		{
			SetUnavailable(Signal, TEXT("visible_hostile_target_required"), TEXT("Point to a currently visible hostile unit."));
		}
		else if (Snapshot.EffectiveSignalProjectorItemId.IsNone())
		{
			SetUnavailable(Signal, TEXT("tactical_signal_projector_required"), TEXT("Equip and carry a tactical field projector."));
		}
		else if (!Snapshot.Hover.bHasSignalPreview || !Snapshot.Hover.Signal.bSucceeded)
		{
			SetUnavailable(Signal, FirstCombatDiagnosticCode(Snapshot.Hover.Signal), FirstCombatDiagnosticMessage(Snapshot.Hover.Signal));
		}
		else
		{
			Signal.ActionPointCost = Snapshot.Hover.Signal.ActionPointCost;
			SetAvailable(Signal);
		}
	}

	FTacticalHudActionAvailability& AttackTerrain = AddAction(Snapshot, ETacticalHudActionType::AttackTerrain, Query);
	AttackTerrain.ItemId = Snapshot.EffectiveWeaponItemId;
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, AttackTerrain))
	{
		if (!Snapshot.Hover.bCellVisible)
		{
			SetUnavailable(AttackTerrain, TEXT("visible_tactical_target_required"), TEXT("Point to a currently visible battlefield cell."));
		}
		else if (Snapshot.EffectiveWeaponItemId.IsNone())
		{
			SetUnavailable(AttackTerrain, TEXT("tactical_weapon_required"), TEXT("Select a carried tactical weapon."));
		}
		else if (!Snapshot.Hover.bHasTerrainAttackPreview || !Snapshot.Hover.TerrainAttack.bSucceeded)
		{
			SetUnavailable(AttackTerrain, FirstCombatDiagnosticCode(Snapshot.Hover.TerrainAttack), FirstCombatDiagnosticMessage(Snapshot.Hover.TerrainAttack));
		}
		else
		{
			AttackTerrain.ActionPointCost = Snapshot.Hover.TerrainAttack.ActionPointCost;
			SetAvailable(AttackTerrain);
		}
	}

	FTacticalHudActionAvailability& Reload = AddAction(Snapshot, ETacticalHudActionType::Reload, Query);
	Reload.ItemId = Snapshot.EffectiveWeaponItemId;
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Reload))
	{
		const FItemRule* Weapon = Rules.Items.Find(Snapshot.EffectiveWeaponItemId);
		const FTacticalWeaponState* WeaponState = SelectedUnit != nullptr
			? SelectedUnit->WeaponStates.FindByPredicate(
				[&Snapshot](const FTacticalWeaponState& State)
				{
					return State.WeaponItemId == Snapshot.EffectiveWeaponItemId;
				})
			: nullptr;
		if (Weapon == nullptr || !Weapon->IsTacticalWeapon() || Weapon->TacticalAmmunitionItemId.IsNone()
			|| Weapon->TacticalMagazineCapacity <= 0 || Weapon->TacticalReloadActionPointCost <= 0 || WeaponState == nullptr)
		{
			SetUnavailable(Reload, TEXT("invalid_tactical_reload"), TEXT("Select a carried magazine-fed weapon with a reload profile."));
		}
		else
		{
			const int32 EffectiveMagazineCapacity = GetEffectiveTacticalMagazineCapacity(*Weapon);
			Reload.ActionPointCost = GetEffectiveTacticalReloadActionPointCost(*Weapon);
			const int32 BestReserveAmmunition = FindBestReserveAmmunition(
				*SelectedUnit, Snapshot.EffectiveWeaponItemId, *Weapon);
			if (WeaponState->LoadedAmmunition >= EffectiveMagazineCapacity)
			{
				SetUnavailable(Reload, TEXT("tactical_weapon_full"), TEXT("The selected weapon magazine is already full."));
			}
			else if (BestReserveAmmunition <= 0)
			{
				SetUnavailable(Reload, TEXT("tactical_ammunition_unavailable"), TEXT("The selected unit carries no compatible reserve magazine."));
			}
			else if (BestReserveAmmunition <= WeaponState->LoadedAmmunition)
			{
				SetUnavailable(Reload, TEXT("tactical_reload_no_improvement"), TEXT("No reserve magazine contains more rounds than the loaded magazine."));
			}
			else if (FindItemQuantity(SelectedUnit->CarriedItems, Weapon->TacticalAmmunitionItemId) > 0
				&& WeaponState->LoadedAmmunition > 0 && SelectedUnit->EjectedMagazines.Num() >= 16)
			{
				SetUnavailable(Reload, TEXT("tactical_magazine_inventory_full"), TEXT("This unit cannot retain the currently loaded magazine."));
			}
			else if (SelectedUnit->RemainingActionPoints < Reload.ActionPointCost)
			{
				SetUnavailable(Reload, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action points required to reload."));
			}
			else
			{
				SetAvailable(Reload);
			}
		}
	}

	FTacticalHudActionAvailability& Eject = AddAction(Snapshot, ETacticalHudActionType::EjectMagazine, Query);
	Eject.ItemId = Snapshot.EffectiveWeaponItemId;
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Eject))
	{
		const FItemRule* Weapon = Rules.Items.Find(Snapshot.EffectiveWeaponItemId);
		const FTacticalWeaponState* WeaponState = SelectedUnit != nullptr
			? SelectedUnit->WeaponStates.FindByPredicate(
				[&Snapshot](const FTacticalWeaponState& State)
				{
					return State.WeaponItemId == Snapshot.EffectiveWeaponItemId;
				})
			: nullptr;
		if (Weapon == nullptr || !Weapon->IsTacticalWeapon() || Weapon->TacticalAmmunitionItemId.IsNone()
			|| Weapon->TacticalMagazineCapacity <= 0 || Weapon->TacticalReloadActionPointCost <= 0 || WeaponState == nullptr)
		{
			SetUnavailable(Eject, TEXT("invalid_tactical_ejection"), TEXT("Select a carried magazine-fed weapon with a valid reload profile."));
		}
		else
		{
			Eject.ActionPointCost = GetEffectiveTacticalReloadActionPointCost(*Weapon);
			if (WeaponState->LoadedAmmunition <= 0)
			{
				SetUnavailable(Eject, TEXT("tactical_weapon_empty"), TEXT("The selected weapon has no loaded magazine to eject."));
			}
			else if (SelectedUnit->EjectedMagazines.Num() >= 16)
			{
				SetUnavailable(Eject, TEXT("tactical_magazine_inventory_full"), TEXT("This unit cannot retain another individually tracked magazine."));
			}
			else if (SelectedUnit->RemainingActionPoints < Eject.ActionPointCost)
			{
				SetUnavailable(Eject, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action points required to eject this magazine."));
			}
			else
			{
				SetAvailable(Eject);
			}
		}
	}

	FTacticalHudActionAvailability& Stance = AddAction(Snapshot, ETacticalHudActionType::ChangeStance, Query);
	Stance.ActionPointCost = 1;
	if (SelectedUnit != nullptr)
	{
		Stance.RequestedStance = SelectedUnit->Stance == ETacticalStance::Standing
			? ETacticalStance::Crouched
			: ETacticalStance::Standing;
	}
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Stance))
	{
		if (SelectedUnit->RemainingActionPoints < Stance.ActionPointCost)
		{
			SetUnavailable(Stance, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action point required to change stance."));
		}
		else
		{
			SetAvailable(Stance);
		}
	}

	FTacticalHudActionAvailability& Door = AddAction(Snapshot, ETacticalHudActionType::OperateDoor, Query);
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Door))
	{
		if (!Snapshot.Hover.bCellVisible)
		{
			SetUnavailable(Door, TEXT("visible_tactical_door_required"), TEXT("Point to a visible adjacent door."));
		}
		else
		{
			const FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(Query.HoveredX, Query.HoveredY, Query.HoveredZ)];
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			Door.bRequestedDoorOpen = !Cell.bDoorOpen;
			Door.ActionPointCost = Terrain != nullptr
				? GetEffectiveTacticalDoorActionPointCost(Terrain->DoorActionPointCost)
				: 0;
			const int32 Distance = ManhattanDistance(
				Query.HoveredX, Query.HoveredY, Query.HoveredZ,
				SelectedUnit->X, SelectedUnit->Y, SelectedUnit->Z);
			const bool bOccupied = Battle.Units.ContainsByPredicate(
				[&Query](const FTacticalUnitState& Unit)
				{
					return Unit.CurrentHealth > 0 && !Unit.bExtracted
						&& Unit.X == Query.HoveredX && Unit.Y == Query.HoveredY && Unit.Z == Query.HoveredZ;
				});
			if (Distance != 1)
			{
				SetUnavailable(Door, TEXT("tactical_door_out_of_reach"), TEXT("The selected unit must stand orthogonally adjacent to the door."));
			}
			else if (Terrain == nullptr || !Terrain->IsDoor() || Cell.CurrentIntegrity <= 0)
			{
				SetUnavailable(Door, TEXT("invalid_tactical_door"), TEXT("The target cell has no intact openable terrain."));
			}
			else if (!Door.bRequestedDoorOpen && bOccupied)
			{
				SetUnavailable(Door, TEXT("occupied_tactical_door"), TEXT("An occupied tactical doorway cannot be closed."));
			}
			else if (SelectedUnit->RemainingActionPoints < Door.ActionPointCost)
			{
				SetUnavailable(Door, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action points required to operate this door."));
			}
			else
			{
				SetAvailable(Door);
			}
		}
	}

	FTacticalHudActionAvailability& Device = AddAction(Snapshot, ETacticalHudActionType::DeployDevice, Query);
	Device.ItemId = Snapshot.EffectiveDeviceItemId;
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, Device))
	{
		const FItemRule* DeviceRule = Rules.Items.Find(Snapshot.EffectiveDeviceItemId);
		if (!Snapshot.Hover.bCellVisible)
		{
			SetUnavailable(Device, TEXT("visible_tactical_target_required"), TEXT("Point to a currently visible battlefield cell."));
		}
		else if (DeviceRule == nullptr || !DeviceRule->IsTacticalDevice())
		{
			SetUnavailable(Device, TEXT("invalid_tactical_device"), TEXT("Select a carried tactical device."));
		}
		else if (FindItemQuantity(SelectedUnit->CarriedItems, Snapshot.EffectiveDeviceItemId) <= 0)
		{
			SetUnavailable(Device, TEXT("tactical_device_unavailable"), TEXT("The selected unit carries no matching tactical device."));
		}
		else
		{
			const int32 EffectiveDeviceActionPointCost = FMath::Clamp(DeviceRule->TacticalActionPointCost, 1, 20);
			const int32 EffectiveDeviceRange = FMath::Clamp(DeviceRule->TacticalRange, 1, 64);
			const bool bHasThrowArc = DeviceRule->HasTacticalThrowArc();
			Device.ActionPointCost = EffectiveDeviceActionPointCost;
			const int64 DeltaX = static_cast<int64>(Query.HoveredX) - SelectedUnit->X;
			const int64 DeltaY = static_cast<int64>(Query.HoveredY) - SelectedUnit->Y;
			const int64 DeltaZ = (static_cast<int64>(Query.HoveredZ) - SelectedUnit->Z) * 2;
			const bool bInRange = DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ
				<= static_cast<int64>(EffectiveDeviceRange) * EffectiveDeviceRange;
			const bool bTrajectoryValid = bHasThrowArc
				? Snapshot.Hover.bHasDeviceTrajectory && Snapshot.Hover.DeviceTrajectory.bSucceeded
				: FTacticalNavigationService::HasLineOfSight(
					Battle,
					Rules,
					SelectedUnit->X,
					SelectedUnit->Y,
					Query.HoveredX,
					Query.HoveredY,
					SelectedUnit->Z,
					Query.HoveredZ);
			if (!bInRange)
			{
				SetUnavailable(Device, TEXT("tactical_target_out_of_range"), TEXT("The target exceeds this device's deployment range."));
			}
			else if (!bTrajectoryValid)
			{
				SetUnavailable(Device, bHasThrowArc
					? FName(TEXT("invalid_tactical_throw_trajectory"))
					: FName(TEXT("no_tactical_line_of_sight")), TEXT("The device cannot reach the selected cell."));
			}
			else if (SelectedUnit->RemainingActionPoints < Device.ActionPointCost)
			{
				SetUnavailable(Device, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action points required to deploy this device."));
			}
			else
			{
				SetAvailable(Device);
			}
		}
	}

	const FTacticalObjectiveState* SelectedObjective = nullptr;
	if (!Query.HoveredObjectiveId.IsNone())
	{
		SelectedObjective = Battle.Objectives.FindByPredicate(
			[&Query](const FTacticalObjectiveState& Objective)
			{
				return Objective.ObjectiveId == Query.HoveredObjectiveId;
			});
	}
	else if (bSelectedUnitOnGrid)
	{
		for (const FTacticalObjectiveState& Objective : Battle.Objectives)
		{
			const int32 Distance = ManhattanDistance(
				Objective.X, Objective.Y, Objective.Z,
				SelectedUnit->X, SelectedUnit->Y, SelectedUnit->Z);
			if (Objective.Status == ETacticalObjectiveStatus::Active && Distance <= 1
				&& (SelectedObjective == nullptr || Objective.ObjectiveId.LexicalLess(SelectedObjective->ObjectiveId)))
			{
				SelectedObjective = &Objective;
			}
		}
	}
	FTacticalHudActionAvailability& ObjectiveAction = AddAction(Snapshot, ETacticalHudActionType::InteractObjective, Query);
	ObjectiveAction.ActionPointCost = GetEffectiveTacticalMissionActionPointCost(
		Mission->ObjectiveActionPointCost);
	if (SelectedObjective != nullptr)
	{
		ObjectiveAction.ObjectiveId = SelectedObjective->ObjectiveId;
		ObjectiveAction.TargetX = SelectedObjective->X;
		ObjectiveAction.TargetY = SelectedObjective->Y;
		ObjectiveAction.TargetZ = SelectedObjective->Z;
	}
	if (RequireControllablePlayerUnit(Battle, SelectedUnit, ObjectiveAction))
	{
		if (SelectedObjective == nullptr)
		{
			SetUnavailable(ObjectiveAction, TEXT("tactical_objective_required"), TEXT("Select or stand next to an active tactical objective."));
		}
		else
		{
			const int32 Distance = ManhattanDistance(
				SelectedObjective->X, SelectedObjective->Y, SelectedObjective->Z,
				SelectedUnit->X, SelectedUnit->Y, SelectedUnit->Z);
			if (SelectedObjective->Status != ETacticalObjectiveStatus::Active)
			{
				SetUnavailable(ObjectiveAction, TEXT("tactical_objective_inactive"), TEXT("The selected objective is no longer active."));
			}
			else if (Distance > 1)
			{
				SetUnavailable(ObjectiveAction, TEXT("tactical_objective_out_of_reach"), TEXT("The selected unit must occupy or stand adjacent to the objective."));
			}
			else if (SelectedUnit->RemainingActionPoints < ObjectiveAction.ActionPointCost)
			{
				SetUnavailable(ObjectiveAction, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action points required to operate this objective."));
			}
			else
			{
				bool bCapacityAvailable = true;
				if (SelectedObjective->Type == ETacticalObjectiveType::Recover
					&& SelectedObjective->CompletedInteractions + 1 == SelectedObjective->RequiredInteractions)
				{
					const FItemRule* Reward = Rules.Items.Find(Mission->ObjectiveRewardItemId);
					const FCraftRule* CraftRule = Craft != nullptr ? Rules.Craft.Find(Craft->CraftRuleId) : nullptr;
					const int64 RewardMass = Reward != nullptr
						? static_cast<int64>(Mission->ObjectiveRewardQuantity) * Reward->Mass
						: MAX_int64;
					bCapacityAvailable = Reward != nullptr && Craft != nullptr && CraftRule != nullptr
						&& RewardMass >= 0 && Snapshot.CargoMass <= MAX_int64 - RewardMass
						&& CanAddInventoryStack(Craft->Cargo, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
						&& CanAddInventoryStack(Craft->PendingSalvage, Mission->ObjectiveRewardItemId, Mission->ObjectiveRewardQuantity)
						&& Snapshot.CargoMass + RewardMass <= CraftRule->CargoCapacity;
				}
				if (!bCapacityAvailable)
				{
					SetUnavailable(ObjectiveAction, TEXT("tactical_recovery_capacity_exceeded"), TEXT("The transport lacks manifest capacity for the recovery reward."));
				}
				else
				{
					SetAvailable(ObjectiveAction);
				}
			}
		}
	}

	FTacticalHudActionAvailability& Extract = AddAction(Snapshot, ETacticalHudActionType::Extract, Query);
	Extract.ActionPointCost = GetEffectiveTacticalMissionActionPointCost(
		Mission->ExtractionActionPointCost);
	if (!Battle.bRequiresExtraction)
	{
		SetUnavailable(Extract, TEXT("tactical_extraction_not_required"),
			TEXT("This defense is won or lost at the base; extraction is not part of the mission."));
	}
	else if (RequireControllablePlayerUnit(Battle, SelectedUnit, Extract))
	{
		const bool bOnExtraction = Battle.IsWithinGrid(SelectedUnit->X, SelectedUnit->Y, SelectedUnit->Z)
			&& Battle.Cells[Battle.GetCellIndex(SelectedUnit->X, SelectedUnit->Y, SelectedUnit->Z)].bExtraction;
		if (!bOnExtraction)
		{
			SetUnavailable(Extract, TEXT("tactical_extraction_unavailable"), TEXT("The selected unit must occupy an extraction cell."));
		}
		else if (SelectedUnit->RemainingActionPoints < Extract.ActionPointCost)
		{
			SetUnavailable(Extract, TEXT("insufficient_action_points"), TEXT("The selected unit lacks the action points required to extract."));
		}
		else
		{
			SetAvailable(Extract);
		}
	}

	FTacticalHudActionAvailability& EndTurn = AddAction(Snapshot, ETacticalHudActionType::EndTurn, Query);
	if (Battle.Phase == ETacticalBattlePhase::PlayerTurn && Battle.ActiveTeam == ETacticalTeam::Player)
	{
		SetAvailable(EndTurn);
	}
	else if (Battle.Phase == ETacticalBattlePhase::Resolved)
	{
		SetUnavailable(EndTurn, TEXT("tactical_battle_resolved"), TEXT("The tactical battle is already resolved."));
	}
	else
	{
		SetUnavailable(EndTurn, TEXT("player_turn_required"), TEXT("Only the active player turn can be ended from the HUD."));
	}
	for (FTacticalHudActionAvailability& Action : Snapshot.Actions)
	{
		Action.UnitId = SelectedUnit != nullptr ? SelectedUnit->UnitId : FGuid();
		Action.TargetUnitId = (Action.ActionType == ETacticalHudActionType::AttackUnit
			|| Action.ActionType == ETacticalHudActionType::ProjectSignal) && bVisibleHostileTarget
			? HoveredUnit->UnitId
			: FGuid();
	}

	Snapshot.bSucceeded = true;
	return Snapshot;
}

FTacticalDebriefView FTacticalPresentationService::BuildDebrief(
	const FCampaignState& Before,
	const FCampaignState& After,
	const FResolvedRuleSet& Rules,
	const FStrategicCommandResult& Resolution,
	const FGuid OperationId)
{
	using namespace TacticalPresentationPrivate;

	FTacticalDebriefView View;
	View.OperationId = OperationId;
	View.CampaignScore = After.CampaignScore;
	if (!Resolution.bAccepted)
	{
		AddDiagnostic(View.Diagnostics, TEXT("tactical_resolution_rejected"), TEXT("A debrief is available only after an accepted tactical resolution."));
		return View;
	}
	const FTacticalOperationState* Operation = FindOperation(Before, OperationId);
	if (Operation == nullptr)
	{
		AddDiagnostic(View.Diagnostics, TEXT("unknown_tactical_operation"), TEXT("The pre-resolution campaign does not contain the requested tactical operation."));
		return View;
	}
	const FStrategicEvent* Resolved = FindResolutionEvent(Resolution, OperationId);
	if (Resolved == nullptr)
	{
		AddDiagnostic(View.Diagnostics, TEXT("tactical_resolution_event_missing"), TEXT("The accepted command did not emit a matching tactical resolution event."));
		return View;
	}

	const FTacticalBattleState* Battle = Before.TacticalBattles.FindByPredicate(
		[&OperationId](const FTacticalBattleState& Entry) { return Entry.OperationId == OperationId; });
	const FCraftState* CraftBefore = FindCraft(Before, Operation->CraftId);
	const FCraftState* CraftAfter = FindCraft(After, Operation->CraftId);
	View.BattleId = Resolved->BattleId;
	View.OperationType = Operation->Type;
	View.SiteId = Operation->SiteId;
	View.CraftId = Operation->CraftId;
	View.BaseId = Operation->BaseId;
	View.AssaultId = Operation->AssaultId;
	View.bMissionSucceeded = Resolved->Quantity > 0;
	View.ScoreAwarded = Resolved->Amount;
	View.CraftDisplayName = CraftAfter != nullptr
		? CraftAfter->DisplayName
		: (CraftBefore != nullptr ? CraftBefore->DisplayName : FString());
	if (const FStrategicBaseState* Base = FindBase(Before, Operation->BaseId))
	{
		View.BaseDisplayName = Base->Name;
	}
	if (Battle != nullptr)
	{
		View.BattleId = Battle->BattleId;
		View.MissionRuleId = Battle->MissionRuleId;
		if (const FTacticalMissionRule* Mission = Rules.TacticalMissions.Find(Battle->MissionRuleId))
		{
			View.MissionDisplayName = Mission->DisplayName;
		}
		else
		{
			View.MissionDisplayName = Battle->MissionRuleId.ToString();
		}
	}
	if (Operation->Type == ETacticalOperationType::SiteRecovery && CraftBefore != nullptr)
	{
		BuildItemViews(CraftBefore->PendingSalvage, Rules, View.RecoveredCargo);
	}

	TArray<FGuid> AgentIds = Operation->AgentIds;
	AgentIds.Sort([](const FGuid& Left, const FGuid& Right) { return GuidLess(Left, Right); });
	for (const FGuid AgentId : AgentIds)
	{
		const FPersonnelState* PersonBefore = FindPersonnel(Before, AgentId);
		if (PersonBefore == nullptr)
		{
			AddDiagnostic(View.Diagnostics, TEXT("unknown_tactical_personnel"), TEXT("A deployed agent is missing from the pre-resolution campaign."));
			continue;
		}
		const FPersonnelState* PersonAfter = FindPersonnel(After, AgentId);
		const FMemorialRecord* Memorial = After.Memorial.FindByPredicate(
			[&AgentId](const FMemorialRecord& Record) { return Record.PersonnelId == AgentId; });
		const FTacticalUnitState* Unit = Battle != nullptr
			? Battle->Units.FindByPredicate(
				[&AgentId](const FTacticalUnitState& Entry) { return Entry.PersonnelId == AgentId; })
			: nullptr;
		const FStrategicEvent* Experience = FindPersonnelEvent(
			Resolution, EStrategicEventType::PersonnelExperienceGained, AgentId);
		const FStrategicEvent* Promotion = FindPersonnelEvent(
			Resolution, EStrategicEventType::PersonnelPromoted, AgentId);
		const FStrategicEvent* Injury = FindPersonnelEvent(
			Resolution, EStrategicEventType::PersonnelInjured, AgentId);
		const FStrategicEvent* Death = FindPersonnelEvent(
			Resolution, EStrategicEventType::PersonnelDied, AgentId);

		FTacticalDebriefPersonnelView& PersonView = View.Personnel.AddDefaulted_GetRef();
		PersonView.PersonnelId = AgentId;
		PersonView.DisplayName = PersonBefore->DisplayName;
		PersonView.StartingHealth = PersonBefore->CurrentHealth;
		PersonView.EndingHealth = Unit != nullptr
			? Unit->CurrentHealth
			: (PersonAfter != nullptr ? PersonAfter->CurrentHealth : 0);
		PersonView.DamageTaken = FMath::Max(0, PersonView.StartingHealth - PersonView.EndingHealth);
		PersonView.bKilled = Death != nullptr || (PersonAfter == nullptr && Memorial != nullptr);
		PersonView.bInjured = !PersonView.bKilled && (Injury != nullptr || PersonView.DamageTaken > 0);
		PersonView.ExperienceGained = Experience != nullptr
			? static_cast<int32>(FMath::Clamp<int64>(Experience->Amount, 0, MAX_int32))
			: 0;
		PersonView.TotalExperience = Experience != nullptr
			? FMath::Max(0, Experience->Quantity)
			: (PersonAfter != nullptr ? PersonAfter->Experience : PersonBefore->Experience);
		PersonView.PreviousRank = Promotion != nullptr
			? static_cast<int32>(FMath::Clamp<int64>(Promotion->Amount, 1, 100))
			: PersonBefore->Rank;
		PersonView.NewRank = Promotion != nullptr
			? FMath::Clamp(Promotion->Quantity, 1, 100)
			: (PersonAfter != nullptr ? PersonAfter->Rank : (Memorial != nullptr ? Memorial->Rank : PersonBefore->Rank));
		PersonView.bPromoted = PersonView.NewRank > PersonView.PreviousRank;
		PersonView.Missions = PersonAfter != nullptr
			? PersonAfter->Missions
			: (Memorial != nullptr ? Memorial->Missions : PersonBefore->Missions);
		PersonView.ServiceHistory = FPersonnelServiceHistory::Project(PersonView.Missions);
		PersonView.PreviousServiceBand = FPersonnelServiceHistory::Project(PersonBefore->Missions).Band;
		PersonView.bServiceBandAdvanced = static_cast<uint8>(PersonView.ServiceHistory.Band)
			> static_cast<uint8>(PersonView.PreviousServiceBand);
		PersonView.RecoverySeconds = PersonAfter != nullptr ? PersonAfter->RemainingRecoverySeconds : 0;
		for (const FStrategicEvent& Event : Resolution.Events)
		{
			if (Event.Type == EStrategicEventType::PersonnelCommendationAwarded
				&& Event.PersonnelId == AgentId && Event.OperationId == OperationId
				&& !Event.RuleId.IsNone() && !PersonView.AwardedCommendationIds.Contains(Event.RuleId))
			{
				PersonView.AwardedCommendationIds.Add(Event.RuleId);
			}
		}
	}

	View.bAvailable = true;
	return View;
}
