// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/TacticalCombatService.h"

#include "Tactical/TacticalNavigationService.h"

namespace TacticalCombatPrivate
{
	static constexpr int64 MaxTacticalCellCount = 8192;

	struct FAttackProfile
	{
		FName RuleId;
		int32 MaximumRange = 0;
		int32 AccuracyModifier = 0;
		int32 AttackPower = 0;
		int32 ActionPointCost = 0;
		ETacticalDamageType DamageType = ETacticalDamageType::Kinetic;
		int32 AmmunitionCost = 0;
		ETacticalFireMode FireMode = ETacticalFireMode::Single;
		int32 ProjectileCount = 1;
		int32 BlastRadius = 0;
		int32 ScatterRadius = 0;
		int32 BlastFalloffPercent = 0;
		int32 TerrainDamagePercent = 0;
		int32 BlastSmoke = 0;
		int32 BlastFire = 0;
		int32 BlastSuppression = 0;
	};

	void AddDiagnostic(FTacticalAttackPreview& Result, const FName Code, FString Message)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	void AddDiagnostic(FTacticalSignalPreview& Result, const FName Code, FString Message)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	int32 SaturatingInt32Add(const int32 Left, const int32 Right)
	{
		return static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(Left) + static_cast<int64>(Right),
			MIN_int32,
			MAX_int32));
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

	bool IsWithin(const int32 X, const int32 Y, const int32 Z, const FTacticalBattleState& Battle)
	{
		return Battle.IsWithinGrid(X, Y, Z);
	}

	bool TryGetCellCount(const FTacticalBattleState& Battle, int64& OutCellCount)
	{
		if (Battle.Width <= 0 || Battle.Height <= 0 || Battle.Levels <= 0 || Battle.Levels > 4)
		{
			return false;
		}

		const int64 LayerArea = static_cast<int64>(Battle.Width) * static_cast<int64>(Battle.Height);
		if (LayerArea > MaxTacticalCellCount / static_cast<int64>(Battle.Levels))
		{
			return false;
		}

		OutCellCount = LayerArea * static_cast<int64>(Battle.Levels);
		return true;
	}

	int32 CeilDistance(const int64 DeltaX, const int64 DeltaY, const int64 DeltaZ)
	{
		const int64 AbsX = DeltaX < 0 ? -static_cast<int64>(DeltaX) : static_cast<int64>(DeltaX);
		const int64 AbsY = DeltaY < 0 ? -static_cast<int64>(DeltaY) : static_cast<int64>(DeltaY);
		const int64 AbsZ = DeltaZ < 0 ? -static_cast<int64>(DeltaZ) : static_cast<int64>(DeltaZ);
		if (AbsX > 128 || AbsY > 128 || AbsZ > 64)
		{
			return MAX_int32;
		}
		const int64 ScaledZ = AbsZ * 2;
		const int64 DistanceSquared = AbsX * AbsX + AbsY * AbsY + ScaledZ * ScaledZ;
		for (int32 Distance = 0; Distance <= 128; ++Distance)
		{
			if (static_cast<int64>(Distance) * Distance >= DistanceSquared)
			{
				return Distance;
			}
		}
		return MAX_int32;
	}

	bool BuildAttackProfile(
		const FTacticalUnitState& Attacker,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FName WeaponItemId,
		const ETacticalFireMode FireMode,
		FAttackProfile& OutProfile,
		FTacticalAttackPreview& Result)
	{
		if (Attacker.Team == ETacticalTeam::Player)
		{
			const FPersonnelState* Person = FindPersonnel(Campaign, Attacker.PersonnelId);
			if (Person == nullptr)
			{
				AddDiagnostic(Result, TEXT("unknown_tactical_personnel"), TEXT("Player combatant does not reference active strategic personnel."));
				return false;
			}
			if (WeaponItemId.IsNone() || !Person->EquippedItems.Contains(WeaponItemId))
			{
				AddDiagnostic(Result, TEXT("tactical_weapon_not_equipped"), TEXT("Player combatants must attack with an explicitly equipped weapon."));
				return false;
			}
			const FItemRule* Weapon = Rules.Items.Find(WeaponItemId);
			if (Weapon == nullptr || !Weapon->IsTacticalWeapon())
			{
				AddDiagnostic(Result, TEXT("invalid_tactical_weapon"), TEXT("The equipped item has no valid tactical weapon profile."));
				return false;
			}
			const bool bHasBlastProfile = Weapon->HasTacticalBlastProfile();
			OutProfile.RuleId = WeaponItemId;
			OutProfile.MaximumRange = FMath::Clamp(Weapon->TacticalRange, 1, 64);
			OutProfile.AccuracyModifier = FMath::Clamp(Weapon->TacticalAccuracyModifier, -50, 50);
			OutProfile.AttackPower = FMath::Max(1, Weapon->Power);
			OutProfile.ActionPointCost = FMath::Clamp(Weapon->TacticalActionPointCost, 1, 20);
			OutProfile.DamageType = Weapon->TacticalDamageType;
			OutProfile.FireMode = FireMode;
			OutProfile.BlastRadius = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalBlastRadius, 1, 8)
				: 0;
			OutProfile.ScatterRadius = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalScatterRadius, 0, 4)
				: 0;
			OutProfile.BlastFalloffPercent = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalBlastFalloffPercent, 0, 100)
				: 0;
			OutProfile.TerrainDamagePercent = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalTerrainDamagePercent, 1, 300)
				: 0;
			OutProfile.BlastSmoke = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalBlastSmoke, 0, 100)
				: 0;
			OutProfile.BlastFire = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalBlastFire, 0, 100)
				: 0;
			OutProfile.BlastSuppression = bHasBlastProfile
				? FMath::Clamp(Weapon->TacticalBlastSuppression, 0, 100)
				: 0;
			if (FireMode == ETacticalFireMode::Burst)
			{
				if (!Weapon->HasTacticalBurstMode())
				{
					AddDiagnostic(Result, TEXT("tactical_fire_mode_unavailable"), TEXT("The selected weapon has no valid burst-fire profile."));
					return false;
				}
				if (Weapon->HasTacticalBlastProfile())
				{
					AddDiagnostic(Result, TEXT("tactical_fire_mode_incompatible"), TEXT("Ground-target blast weapons cannot use burst fire."));
					return false;
				}
				OutProfile.AccuracyModifier = SaturatingInt32Add(
					OutProfile.AccuracyModifier,
					FMath::Clamp(Weapon->TacticalBurstAccuracyModifier, -50, 0));
				OutProfile.ActionPointCost = FMath::Clamp(Weapon->TacticalBurstActionPointCost, 1, 20);
				OutProfile.ProjectileCount = FMath::Clamp(Weapon->TacticalBurstShotCount, 2, 8);
			}
			else if (FireMode != ETacticalFireMode::Single)
			{
				AddDiagnostic(Result, TEXT("invalid_tactical_fire_mode"), TEXT("Tactical attack selected an unknown fire mode."));
				return false;
			}
			if (!Weapon->TacticalAmmunitionItemId.IsNone())
			{
				const FTacticalWeaponState* WeaponState = Attacker.WeaponStates.FindByPredicate(
					[WeaponItemId](const FTacticalWeaponState& State) { return State.WeaponItemId == WeaponItemId; });
				if (WeaponState == nullptr)
				{
					AddDiagnostic(Result, TEXT("tactical_weapon_not_carried"), TEXT("Player weapon has no tactical magazine state in this battle."));
					return false;
				}
				const int64 RequiredAmmunition = static_cast<int64>(Weapon->TacticalAmmunitionPerAttack)
					* static_cast<int64>(OutProfile.ProjectileCount);
				if (RequiredAmmunition <= 0 || RequiredAmmunition > MAX_int32)
				{
					AddDiagnostic(Result, TEXT("invalid_tactical_ammunition_profile"), TEXT("The weapon's ammunition cost cannot be represented safely for the selected fire mode."));
					return false;
				}
				if (static_cast<int64>(WeaponState->LoadedAmmunition) < RequiredAmmunition)
				{
					AddDiagnostic(Result, TEXT("tactical_weapon_empty"), TEXT("Player weapon lacks enough loaded ammunition for the selected fire mode."));
					return false;
				}
				OutProfile.AmmunitionCost = static_cast<int32>(RequiredAmmunition);
			}
			return true;
		}

		if (Attacker.Team == ETacticalTeam::Adversary)
		{
			if (FireMode != ETacticalFireMode::Single)
			{
				AddDiagnostic(Result, TEXT("tactical_fire_mode_unavailable"), TEXT("This adversary profile exposes only its intrinsic single attack."));
				return false;
			}
			if (!WeaponItemId.IsNone())
			{
				AddDiagnostic(Result, TEXT("unexpected_tactical_weapon"), TEXT("Adversary combatants use their intrinsic attack profile."));
				return false;
			}
			const FTacticalUnitRule* UnitRule = Rules.TacticalUnits.Find(Attacker.SourceRuleId);
			if (UnitRule == nullptr || UnitRule->AttackRange <= 0 || UnitRule->AttackPower <= 0
				|| UnitRule->AttackActionPointCost <= 0)
			{
				AddDiagnostic(Result, TEXT("invalid_tactical_attack_profile"), TEXT("Adversary combatant has no valid intrinsic attack profile."));
				return false;
			}
			OutProfile.RuleId = Attacker.SourceRuleId;
			OutProfile.MaximumRange = FMath::Clamp(UnitRule->AttackRange, 1, 64);
			OutProfile.AttackPower = FMath::Clamp(UnitRule->AttackPower, 1, 200);
			OutProfile.ActionPointCost = FMath::Clamp(UnitRule->AttackActionPointCost, 1, 20);
			OutProfile.DamageType = UnitRule->AttackDamageType;
			OutProfile.FireMode = ETacticalFireMode::Single;
			return true;
		}

		AddDiagnostic(Result, TEXT("invalid_tactical_team"), TEXT("Attacking unit has an unknown tactical team."));
		return false;
	}

	bool BeginPreview(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FGuid AttackerUnitId,
		const FName WeaponItemId,
		const ETacticalFireMode FireMode,
		const FTacticalUnitState*& OutAttacker,
		FAttackProfile& OutProfile,
		FTacticalAttackPreview& Result)
	{
		Result.AttackerUnitId = AttackerUnitId;
		if (Battle.Phase == ETacticalBattlePhase::Deployment)
		{
			AddDiagnostic(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Tactical attacks require confirmed deployment."));
			return false;
		}
		if (Battle.Phase == ETacticalBattlePhase::Resolved)
		{
			AddDiagnostic(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot accept attacks."));
			return false;
		}
		OutAttacker = FindUnit(Battle, AttackerUnitId);
		if (OutAttacker == nullptr)
		{
			AddDiagnostic(Result, TEXT("unknown_tactical_unit"), TEXT("Tactical attack references an unknown attacker."));
			return false;
		}
		if (OutAttacker->CurrentHealth <= 0 || OutAttacker->bExtracted)
		{
			AddDiagnostic(Result, TEXT("incapacitated_tactical_unit"), TEXT("Only a living, deployed combatant can attack."));
			return false;
		}
		if (!IsWithin(OutAttacker->X, OutAttacker->Y, OutAttacker->Z, Battle))
		{
			AddDiagnostic(Result, TEXT("invalid_tactical_unit"), TEXT("Tactical attacks require a living, deployed combatant on the grid."));
			return false;
		}
		if (OutAttacker->Team != Battle.ActiveTeam)
		{
			AddDiagnostic(Result, TEXT("inactive_tactical_unit"), TEXT("Only a unit on the active tactical team can attack."));
			return false;
		}
		if (!BuildAttackProfile(*OutAttacker, Campaign, Rules, WeaponItemId, FireMode, OutProfile, Result))
		{
			return false;
		}
		Result.AttackRuleId = OutProfile.RuleId;
		Result.MaximumRange = OutProfile.MaximumRange;
		Result.ActionPointCost = OutProfile.ActionPointCost;
		Result.AttackPower = OutProfile.AttackPower;
		Result.DamageType = OutProfile.DamageType;
		Result.AmmunitionCost = OutProfile.AmmunitionCost;
		Result.FireMode = OutProfile.FireMode;
		Result.ProjectileCount = OutProfile.ProjectileCount;
		Result.BlastRadius = OutProfile.BlastRadius;
		Result.ScatterRadius = OutProfile.ScatterRadius;
		Result.BlastFalloffPercent = OutProfile.BlastFalloffPercent;
		Result.TerrainDamagePercent = OutProfile.TerrainDamagePercent;
		Result.BlastSmoke = OutProfile.BlastSmoke;
		Result.BlastFire = OutProfile.BlastFire;
		Result.BlastSuppression = OutProfile.BlastSuppression;
		if (OutAttacker->RemainingActionPoints < OutProfile.ActionPointCost)
		{
			AddDiagnostic(Result, TEXT("insufficient_action_points"), TEXT("Attacking unit lacks the action points required by its attack profile."));
			return false;
		}
		return true;
	}

	bool FinishSpatialPreview(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const FTacticalUnitState& Attacker,
		const int32 TargetX,
		const int32 TargetY,
		const int32 TargetZ,
		const FAttackProfile& Profile,
		FTacticalAttackPreview& Result)
	{
		Result.TargetX = TargetX;
		Result.TargetY = TargetY;
		Result.TargetZ = TargetZ;
		if (!IsWithin(TargetX, TargetY, TargetZ, Battle))
		{
			AddDiagnostic(Result, TEXT("invalid_tactical_target"), TEXT("Tactical attack target is outside the battlefield."));
			return false;
		}
		const int64 DeltaX = static_cast<int64>(TargetX) - Attacker.X;
		const int64 DeltaY = static_cast<int64>(TargetY) - Attacker.Y;
		const int64 DeltaZ = static_cast<int64>(TargetZ) - Attacker.Z;
		Result.Distance = CeilDistance(DeltaX, DeltaY, DeltaZ);
		if (Result.Distance <= 0 || Result.Distance > Profile.MaximumRange)
		{
			AddDiagnostic(Result, TEXT("tactical_target_out_of_range"), TEXT("Tactical attack target is outside the attack profile's range."));
			return false;
		}
		if (!FTacticalNavigationService::HasLineOfSight(
			Battle, Rules, Attacker.X, Attacker.Y, TargetX, TargetY, Attacker.Z, TargetZ))
		{
			AddDiagnostic(Result, TEXT("no_tactical_line_of_sight"), TEXT("Intact terrain blocks line of sight to the tactical target."));
			return false;
		}
		Result.SmokePenalty = FMath::Min(60, FTacticalNavigationService::ComputeSmokeObscuration(
			Battle, Attacker.X, Attacker.Y, TargetX, TargetY, Attacker.Z, TargetZ));
		return true;
	}

	int32 CoverAroundTarget(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const int32 TargetX,
		const int32 TargetY,
		const int32 TargetZ)
	{
		int32 Cover = 0;
		static constexpr int32 OffsetX[] = { -1, 1, 0, 0 };
		static constexpr int32 OffsetY[] = { 0, 0, -1, 1 };
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			const int32 X = TargetX + OffsetX[Direction];
			const int32 Y = TargetY + OffsetY[Direction];
			if (!IsWithin(X, Y, TargetZ, Battle))
			{
				continue;
			}
			const FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(X, Y, TargetZ)];
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			if (Terrain != nullptr && Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen)
			{
				Cover = FMath::Max(Cover, Terrain->CoverPercent);
			}
		}
		return Cover;
	}
}

bool FTacticalAttackPreview::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalCombatDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FTacticalSignalPreview::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalCombatDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FTacticalAttackPreview FTacticalCombatService::PreviewUnitAttack(
	const FTacticalBattleState& Battle,
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FGuid AttackerUnitId,
	const FGuid TargetUnitId,
	const FName WeaponItemId,
	const ETacticalFireMode FireMode)
{
	using namespace TacticalCombatPrivate;

	FTacticalAttackPreview Result;
	Result.TargetUnitId = TargetUnitId;
	const FTacticalUnitState* Attacker = nullptr;
	FAttackProfile Profile;
	if (!BeginPreview(Battle, Campaign, Rules, AttackerUnitId, WeaponItemId, FireMode, Attacker, Profile, Result))
	{
		return Result;
	}
	if (Profile.BlastRadius > 0)
	{
		AddDiagnostic(Result, TEXT("blast_weapon_requires_ground_target"), TEXT("Blast weapons must target a battlefield cell so all affected units and terrain are explicit."));
		return Result;
	}
	const FTacticalUnitState* Target = FindUnit(Battle, TargetUnitId);
	if (Target == nullptr)
	{
		AddDiagnostic(Result, TEXT("unknown_tactical_target"), TEXT("Tactical attack references an unknown target unit."));
		return Result;
	}
	if (Target->CurrentHealth <= 0 || Target->bExtracted)
	{
		AddDiagnostic(Result, TEXT("invalid_tactical_target"), TEXT("Tactical attacks require a living, deployed target unit."));
		return Result;
	}
	if (Target->Team == Attacker->Team)
	{
		AddDiagnostic(Result, TEXT("friendly_tactical_target"), TEXT("Tactical combatants cannot attack their own team."));
		return Result;
	}
	if (!FinishSpatialPreview(Battle, Rules, *Attacker, Target->X, Target->Y, Target->Z, Profile, Result))
	{
		return Result;
	}
	Result.StanceAccuracyModifier = Attacker->Stance == ETacticalStance::Crouched ? 10 : 0;
	Result.StanceCoverModifier = Target->Stance == ETacticalStance::Crouched ? 15 : 0;
	Result.ElevationAccuracyModifier = FMath::Clamp(Attacker->Z - Target->Z, -2, 2) * 10;
	Result.CoverPercent = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(CoverAroundTarget(Battle, Rules, Target->X, Target->Y, Target->Z))
			+ static_cast<int64>(Result.StanceCoverModifier),
		0,
		100));
	const int64 DistancePenalty = FMath::Max<int64>(
		0,
		static_cast<int64>(Result.Distance) - 3) * 3;
	const int64 RawHitChance = static_cast<int64>(Attacker->Accuracy)
		+ static_cast<int64>(Profile.AccuracyModifier)
		+ static_cast<int64>(Result.StanceAccuracyModifier)
		+ static_cast<int64>(Result.ElevationAccuracyModifier)
		- DistancePenalty
		- static_cast<int64>(Result.CoverPercent)
		- static_cast<int64>(Result.SmokePenalty);
	Result.HitChance = static_cast<int32>(FMath::Clamp<int64>(RawHitChance, 5, 95));
	Result.bSucceeded = true;
	return Result;
}

FTacticalAttackPreview FTacticalCombatService::PreviewTerrainAttack(
	const FTacticalBattleState& Battle,
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FGuid AttackerUnitId,
	const int32 TargetX,
	const int32 TargetY,
	const FName WeaponItemId,
	const ETacticalFireMode FireMode,
	const int32 TargetZ)
{
	using namespace TacticalCombatPrivate;

	FTacticalAttackPreview Result;
	const FTacticalUnitState* Attacker = nullptr;
	FAttackProfile Profile;
	if (!BeginPreview(Battle, Campaign, Rules, AttackerUnitId, WeaponItemId, FireMode, Attacker, Profile, Result))
	{
		return Result;
	}
	if (Profile.FireMode != ETacticalFireMode::Single)
	{
		AddDiagnostic(Result, TEXT("tactical_fire_mode_requires_unit_target"), TEXT("Burst fire requires a unit target; ground-target attacks resolve one impact."));
		return Result;
	}
	if (!FinishSpatialPreview(Battle, Rules, *Attacker, TargetX, TargetY, TargetZ, Profile, Result))
	{
		return Result;
	}
	const FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(TargetX, TargetY, TargetZ)];
	const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
	if (Profile.BlastRadius == 0 && (Terrain == nullptr || Terrain->MaxIntegrity <= 0 || Cell.CurrentIntegrity <= 0))
	{
		AddDiagnostic(Result, TEXT("indestructible_tactical_target"), TEXT("Target cell has no intact destructible terrain."));
		return Result;
	}
	Result.HitChance = 100;
	Result.bSucceeded = true;
	return Result;
}

FTacticalSignalPreview FTacticalCombatService::PreviewSignalProjection(
	const FTacticalBattleState& Battle,
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FGuid AttackerUnitId,
	const FGuid TargetUnitId,
	const FName ProjectorItemId)
{
	using namespace TacticalCombatPrivate;

	FTacticalSignalPreview Result;
	Result.AttackerUnitId = AttackerUnitId;
	Result.TargetUnitId = TargetUnitId;
	if (Battle.Phase == ETacticalBattlePhase::Deployment)
	{
		AddDiagnostic(Result, TEXT("tactical_deployment_unconfirmed"), TEXT("Signal projection requires confirmed deployment."));
		return Result;
	}
	if (Battle.Phase == ETacticalBattlePhase::Resolved)
	{
		AddDiagnostic(Result, TEXT("tactical_battle_resolved"), TEXT("Resolved tactical battles cannot accept signal projections."));
		return Result;
	}
	const FTacticalUnitState* Attacker = FindUnit(Battle, AttackerUnitId);
	if (Attacker == nullptr)
	{
		AddDiagnostic(Result, TEXT("unknown_tactical_unit"), TEXT("Signal projection references an unknown projecting unit."));
		return Result;
	}
	if (Attacker->CurrentHealth <= 0 || Attacker->bExtracted)
	{
		AddDiagnostic(Result, TEXT("incapacitated_tactical_unit"), TEXT("Only a living, deployed combatant can project a signal."));
		return Result;
	}
	if (!IsWithin(Attacker->X, Attacker->Y, Attacker->Z, Battle))
	{
		AddDiagnostic(Result, TEXT("invalid_tactical_unit"), TEXT("Signal projection requires a living, deployed projecting unit on the grid."));
		return Result;
	}
	if (Attacker->Team != Battle.ActiveTeam)
	{
		AddDiagnostic(Result, TEXT("inactive_tactical_unit"), TEXT("Only a unit on the active tactical team can project a signal."));
		return Result;
	}

	if (Attacker->Team == ETacticalTeam::Player)
	{
		const FPersonnelState* Person = FindPersonnel(Campaign, Attacker->PersonnelId);
		const FItemRule* Projector = Rules.Items.Find(ProjectorItemId);
		const FInventoryStack* CarriedProjector = Attacker->CarriedItems.FindByPredicate(
			[ProjectorItemId](const FInventoryStack& Stack)
			{
				return Stack.ItemId == ProjectorItemId && Stack.Quantity > 0;
			});
		if (Person == nullptr)
		{
			AddDiagnostic(Result, TEXT("unknown_tactical_personnel"), TEXT("Player signal operator does not reference active strategic personnel."));
			return Result;
		}
		if (ProjectorItemId.IsNone() || !Person->EquippedItems.Contains(ProjectorItemId)
			|| Projector == nullptr || !Projector->IsTacticalSignalProjector() || CarriedProjector == nullptr)
		{
			AddDiagnostic(Result, TEXT("tactical_signal_projector_required"), TEXT("Player signal projection requires a carried and equipped field projector."));
			return Result;
		}
		Result.SignalRuleId = ProjectorItemId;
		Result.SignalPower = FMath::Clamp(Projector->Power, 1, 100);
		Result.MaximumRange = FMath::Clamp(Projector->TacticalRange, 1, 64);
		Result.ActionPointCost = FMath::Clamp(Projector->TacticalActionPointCost, 1, 20);
	}
	else if (Attacker->Team == ETacticalTeam::Adversary)
	{
		if (!ProjectorItemId.IsNone())
		{
			AddDiagnostic(Result, TEXT("unexpected_tactical_signal_projector"), TEXT("Adversary signal operators use their intrinsic projection profile."));
			return Result;
		}
		const FTacticalUnitRule* UnitRule = Rules.TacticalUnits.Find(Attacker->SourceRuleId);
		if (UnitRule == nullptr || !UnitRule->HasSignalProjection())
		{
			AddDiagnostic(Result, TEXT("tactical_signal_unavailable"), TEXT("This adversary has no intrinsic signal projection profile."));
			return Result;
		}
		Result.SignalRuleId = Attacker->SourceRuleId;
		Result.SignalPower = FMath::Clamp(UnitRule->SignalPower, 1, 100);
		Result.MaximumRange = FMath::Clamp(UnitRule->SignalRange, 1, 64);
		Result.ActionPointCost = FMath::Clamp(UnitRule->SignalActionPointCost, 1, 20);
	}
	else
	{
		AddDiagnostic(Result, TEXT("invalid_tactical_team"), TEXT("Signal operator has an unknown tactical team."));
		return Result;
	}
	if (Attacker->RemainingActionPoints < Result.ActionPointCost)
	{
		AddDiagnostic(Result, TEXT("insufficient_action_points"), TEXT("Signal operator lacks the action points required to project pressure."));
		return Result;
	}

	const FTacticalUnitState* Target = FindUnit(Battle, TargetUnitId);
	if (Target == nullptr)
	{
		AddDiagnostic(Result, TEXT("unknown_tactical_target"), TEXT("Signal projection references an unknown target unit."));
		return Result;
	}
	if (Target->CurrentHealth <= 0 || Target->bExtracted)
	{
		AddDiagnostic(Result, TEXT("invalid_tactical_target"), TEXT("Signal projection requires a living, deployed target unit."));
		return Result;
	}
	if (!IsWithin(Target->X, Target->Y, Target->Z, Battle))
	{
		AddDiagnostic(Result, TEXT("invalid_tactical_target"), TEXT("Signal projection requires a target unit on the battlefield."));
		return Result;
	}
	if (Target->Team == Attacker->Team)
	{
		AddDiagnostic(Result, TEXT("friendly_tactical_target"), TEXT("Signal pressure cannot be projected against the operator's own team."));
		return Result;
	}
	if (Target->Suppression >= 100 && Target->CurrentMorale <= 0)
	{
		AddDiagnostic(Result, TEXT("tactical_signal_target_saturated"), TEXT("The target is already at maximum signal pressure."));
		return Result;
	}
	const FTacticalVisibilityResult Visibility = FTacticalNavigationService::ComputeTeamVisibility(
		Battle, Rules, Attacker->Team);
	if (!Visibility.bSucceeded || !Visibility.IsUnitVisible(TargetUnitId))
	{
		AddDiagnostic(Result, TEXT("tactical_signal_target_not_visible"), TEXT("Signal projection requires a target currently visible to the operator's team."));
		return Result;
	}
	Result.Distance = CeilDistance(
		static_cast<int64>(Target->X) - Attacker->X,
		static_cast<int64>(Target->Y) - Attacker->Y,
		static_cast<int64>(Target->Z) - Attacker->Z);
	if (Result.Distance <= 0 || Result.Distance > Result.MaximumRange)
	{
		AddDiagnostic(Result, TEXT("tactical_signal_target_out_of_range"), TEXT("Target is outside the signal projector's range."));
		return Result;
	}
	if (!FTacticalNavigationService::HasLineOfSight(
		Battle, Rules, Attacker->X, Attacker->Y, Target->X, Target->Y, Attacker->Z, Target->Z))
	{
		AddDiagnostic(Result, TEXT("no_tactical_line_of_sight"), TEXT("Intact terrain blocks the signal projection lock."));
		return Result;
	}
	Result.SmokePenalty = FMath::Min(40, FTacticalNavigationService::ComputeSmokeObscuration(
		Battle, Attacker->X, Attacker->Y, Target->X, Target->Y, Attacker->Z, Target->Z) / 2);
	const int64 DistancePenalty = FMath::Max<int64>(
		0,
		static_cast<int64>(Result.Distance) - 3) * 3;
	const int64 ResolveDelta = static_cast<int64>(Attacker->Resolve) - static_cast<int64>(Target->Resolve);
	const int64 RawHitChance = 45LL
		+ ResolveDelta
		+ static_cast<int64>(Result.SignalPower) / 2
		- DistancePenalty
		- static_cast<int64>(Result.SmokePenalty);
	Result.HitChance = static_cast<int32>(FMath::Clamp<int64>(RawHitChance, 5, 95));
	const int32 RawSuppression = static_cast<int32>(FMath::Clamp<int64>(
		4LL + static_cast<int64>(Result.SignalPower) / 3,
		4,
		30));
	const int32 RawMoraleDamage = static_cast<int32>(FMath::Clamp<int64>(
		5LL + static_cast<int64>(Result.SignalPower) / 2 + FMath::Max<int64>(0, ResolveDelta) / 5,
		5,
		35));
	const int32 AvailableSuppression = static_cast<int32>(FMath::Clamp<int64>(
		100LL - static_cast<int64>(Target->Suppression),
		0,
		100));
	const int32 AvailableMorale = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(Target->CurrentMorale),
		0,
		100));
	Result.SuppressionGain = FMath::Min(AvailableSuppression, RawSuppression);
	Result.MoraleDamage = FMath::Min(AvailableMorale, RawMoraleDamage);
	Result.bSucceeded = true;
	return Result;
}

int32 FTacticalCombatService::ComputeUnitDamage(
	const int32 AttackPower,
	const int32 AttackerStrength,
	const int32 DefenderStrength,
	const int32 DefenderArmor,
	const int32 VariancePercent)
{
	const int64 ScaledPower = FMath::Max<int64>(
		1,
		static_cast<int64>(AttackPower) + FMath::Max<int64>(0, static_cast<int64>(AttackerStrength)) / 10);
	const int64 VariedPower = FMath::Max<int64>(
		1,
		ScaledPower * FMath::Clamp<int64>(static_cast<int64>(VariancePercent), 80, 120) / 100);
	return static_cast<int32>(FMath::Clamp<int64>(
		VariedPower
			- FMath::Max<int64>(0, static_cast<int64>(DefenderStrength)) / 12
			- FMath::Max<int64>(0, static_cast<int64>(DefenderArmor)),
		1,
		MAX_int32));
}

int32 FTacticalCombatService::ComputeTerrainDamage(
	const int32 AttackPower,
	const int32 AttackerStrength,
	const int32 VariancePercent)
{
	const int64 ScaledPower = FMath::Max<int64>(
		1,
		static_cast<int64>(AttackPower) + FMath::Max<int64>(0, static_cast<int64>(AttackerStrength)) / 10);
	return static_cast<int32>(FMath::Clamp<int64>(
		ScaledPower * FMath::Clamp<int64>(static_cast<int64>(VariancePercent), 80, 120) / 100,
		1,
		MAX_int32));
}

int32 FTacticalCombatService::ComputeBlastEffectPercent(
	const int32 Distance,
	const int32 FalloffPercentPerCell)
{
	const int64 Falloff = FMath::Max<int64>(0, static_cast<int64>(Distance))
		* FMath::Clamp<int64>(static_cast<int64>(FalloffPercentPerCell), 0, 100);
	return static_cast<int32>(FMath::Clamp<int64>(100 - Falloff, 0, 100));
}

int32 FTacticalCombatService::ComputeBlastTransmissionPercent(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules,
	const int32 ImpactX,
	const int32 ImpactY,
	const int32 TargetX,
	const int32 TargetY,
	const int32 ImpactZ,
	const int32 TargetZ)
{
	int64 ExpectedCells = 0;
	if (!TacticalCombatPrivate::TryGetCellCount(Battle, ExpectedCells)
		|| !TacticalCombatPrivate::IsWithin(ImpactX, ImpactY, ImpactZ, Battle)
		|| !TacticalCombatPrivate::IsWithin(TargetX, TargetY, TargetZ, Battle)
		|| Battle.Cells.Num() != ExpectedCells)
	{
		return 0;
	}

	int32 TransmissionPercent = 100;
	auto ApplyIntermediateCell = [&Battle, &Rules, &TransmissionPercent](const int32 X, const int32 Y, const int32 Z)
	{
		const FTacticalCellState& Cell = Battle.Cells[Battle.GetCellIndex(X, Y, Z)];
		const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
		if (Terrain == nullptr)
		{
			TransmissionPercent = 0;
			return;
		}
		if (Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen && Terrain->BlastResistancePercent > 0)
		{
			const int64 ResistancePercent = FMath::Clamp<int64>(
				static_cast<int64>(Terrain->BlastResistancePercent),
				0,
				100);
			TransmissionPercent = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(TransmissionPercent) * (100 - ResistancePercent) / 100,
				0,
				100));
		}
	};

	if (ImpactZ != TargetZ)
	{
		const int32 DeltaX = TargetX - ImpactX;
		const int32 DeltaY = TargetY - ImpactY;
		const int32 DeltaZ = TargetZ - ImpactZ;
		const int32 StepCount = FMath::Max3(FMath::Abs(DeltaX), FMath::Abs(DeltaY), FMath::Abs(DeltaZ));
		int32 PreviousIndex = Battle.GetCellIndex(ImpactX, ImpactY, ImpactZ);
		for (int32 Step = 1; Step < StepCount; ++Step)
		{
			auto RoundRatio = [StepCount](const int64 Numerator)
			{
				return Numerator >= 0
					? static_cast<int32>((Numerator + StepCount / 2) / StepCount)
					: -static_cast<int32>((-Numerator + StepCount / 2) / StepCount);
			};
			const int32 X = ImpactX + RoundRatio(static_cast<int64>(DeltaX) * Step);
			const int32 Y = ImpactY + RoundRatio(static_cast<int64>(DeltaY) * Step);
			const int32 Z = ImpactZ + RoundRatio(static_cast<int64>(DeltaZ) * Step);
			const int32 Index = Battle.GetCellIndex(X, Y, Z);
			if (Index != PreviousIndex)
			{
				ApplyIntermediateCell(X, Y, Z);
				PreviousIndex = Index;
				if (TransmissionPercent <= 0)
				{
					return 0;
				}
			}
		}
		return TransmissionPercent;
	}

	int32 X = ImpactX;
	int32 Y = ImpactY;
	const int32 DeltaX = FMath::Abs(TargetX - ImpactX);
	const int32 StepX = ImpactX < TargetX ? 1 : -1;
	const int32 DeltaY = FMath::Abs(TargetY - ImpactY);
	const int32 StepY = ImpactY < TargetY ? 1 : -1;
	int32 Error = DeltaX - DeltaY;
	while (X != TargetX || Y != TargetY)
	{
		const int32 TwiceError = 2 * Error;
		if (TwiceError > -DeltaY)
		{
			Error -= DeltaY;
			X += StepX;
		}
		if (TwiceError < DeltaX)
		{
			Error += DeltaX;
			Y += StepY;
		}
		if (X == TargetX && Y == TargetY)
		{
			break;
		}
		ApplyIntermediateCell(X, Y, ImpactZ);
		if (TransmissionPercent <= 0)
		{
			return 0;
		}
	}
	return TransmissionPercent;
}
