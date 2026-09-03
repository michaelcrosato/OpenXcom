// Copyright 2026 UEGT contributors. MIT License.

#include "Content/RuleTypes.h"

int32 FFacilityRule::ScaleEffectByIntegrity(const int32 FullValue, const int32 Damage) const
{
	if (FullValue <= 0 || MaxIntegrity <= 0 || Damage >= MaxIntegrity)
	{
		return 0;
	}
	if (Damage <= 0)
	{
		return FullValue;
	}
	const int64 RemainingIntegrity = static_cast<int64>(MaxIntegrity) - Damage;
	const int64 Numerator = static_cast<int64>(FullValue) * RemainingIntegrity;
	return static_cast<int32>(FMath::Min<int64>(
		FullValue,
		(Numerator + MaxIntegrity - 1) / MaxIntegrity));
}

namespace RuleSetBuilderPrivate
{
	void AddError(FRuleSetBuildResult& Result, const FName Code, const FName PackageId, FString Message)
	{
		FContentDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EContentDiagnosticSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.PackageId = PackageId;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool HasErrors(const FRuleSetBuildResult& Result)
	{
		return Result.Diagnostics.ContainsByPredicate(
			[](const FContentDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == EContentDiagnosticSeverity::Error;
			});
	}

	int64 ComputeTacticalMapCellCount(const int32 Width, const int32 Height, const int32 Levels)
	{
		if (Width < 8 || Width > 64 || Height < 12 || Height > 96 || Levels < 1 || Levels > 4)
		{
			return MAX_int64;
		}
		return static_cast<int64>(Width) * static_cast<int64>(Height) * static_cast<int64>(Levels);
	}

	template <typename RuleType>
	void ApplyRules(
		const FName PackageId,
		const TCHAR* RuleKind,
		const TArray<RuleType>& Contributions,
		TMap<FName, RuleType>& Rules,
		TMap<FName, FName>& Origins,
		FRuleSetBuildResult& Result)
	{
		TSet<FName> SeenInPackage;
		for (const RuleType& Contribution : Contributions)
		{
			const FName RuleId = Contribution.Identity.RuleId;
			if (!FContentPackageResolver::IsValidPackageId(RuleId))
			{
				AddError(Result, TEXT("invalid_rule_id"), PackageId, FString::Printf(TEXT("%s rule id '%s' is invalid."), RuleKind, *RuleId.ToString()));
				continue;
			}
			if (SeenInPackage.Contains(RuleId))
			{
				AddError(Result, TEXT("duplicate_rule_in_package"), PackageId, FString::Printf(TEXT("%s rule '%s' appears more than once in one package."), RuleKind, *RuleId.ToString()));
				continue;
			}
			SeenInPackage.Add(RuleId);

			if (Rules.Contains(RuleId))
			{
				if (!Contribution.Identity.bReplaceExisting)
				{
					AddError(Result, TEXT("unexpected_rule_override"), PackageId, FString::Printf(TEXT("%s rule '%s' already exists; set replace=true to override it explicitly."), RuleKind, *RuleId.ToString()));
					continue;
				}
				Rules[RuleId] = Contribution;
				Origins[RuleId] = PackageId;
			}
			else
			{
				if (Contribution.Identity.bReplaceExisting)
				{
					AddError(Result, TEXT("missing_rule_override_target"), PackageId, FString::Printf(TEXT("%s rule '%s' requests replacement, but no earlier package defines it."), RuleKind, *RuleId.ToString()));
					continue;
				}
				Rules.Add(RuleId, Contribution);
				Origins.Add(RuleId, PackageId);
			}
		}
	}

	void ValidateRuleValues(const FName PackageId, const FContentPackage& Package, FRuleSetBuildResult& Result)
	{
		auto IsValidDamageType = [](const ETacticalDamageType DamageType)
		{
			return DamageType == ETacticalDamageType::Kinetic
				|| DamageType == ETacticalDamageType::Thermal
				|| DamageType == ETacticalDamageType::Arc;
		};
		for (const FItemRule& Rule : Package.Items)
		{
			bool bManufacturingInputsValid = Rule.ManufactureInputs.Num() <= 16
				&& (Rule.IsManufacturable() || Rule.ManufactureInputs.IsEmpty());
			TSet<FName> SeenManufacturingInputs;
			for (const FManufacturingInputRule& Input : Rule.ManufactureInputs)
			{
				bManufacturingInputsValid &= FContentPackageResolver::IsValidPackageId(Input.ItemId)
					&& Input.ItemId != Rule.Identity.RuleId
					&& Input.Quantity > 0
					&& !SeenManufacturingInputs.Contains(Input.ItemId);
				SeenManufacturingInputs.Add(Input.ItemId);
			}
			const bool bHasAnyTacticalProfile = Rule.TacticalRange != 0
				|| Rule.TacticalAccuracyModifier != 0
				|| Rule.TacticalActionPointCost != 0
				|| Rule.TacticalBurstShotCount != 0 || Rule.TacticalBurstActionPointCost != 0
				|| Rule.TacticalBurstAccuracyModifier != 0 || Rule.TacticalBlastRadius != 0
				|| Rule.TacticalScatterRadius != 0
				|| Rule.TacticalBlastFalloffPercent != 0 || Rule.TacticalTerrainDamagePercent != 0
				|| Rule.TacticalBlastSmoke != 0 || Rule.TacticalBlastFire != 0
				|| Rule.TacticalBlastSuppression != 0
				|| Rule.TacticalRadius != 0 || Rule.TacticalThrowArcHeight != 0 || Rule.TacticalSmoke != 0
				|| Rule.TacticalFire != 0 || Rule.TacticalSuppression != 0
				|| Rule.TacticalSmokeReduction != 0 || Rule.TacticalFireReduction != 0
				|| Rule.TacticalSuppressionReduction != 0 || Rule.TacticalMoraleRecovery != 0;
			const bool bTacticalAmmunitionValid = Rule.TacticalAmmunitionItemId.IsNone()
				? Rule.TacticalMagazineCapacity == 0 && Rule.TacticalReloadActionPointCost == 0
				: Rule.IsTacticalWeapon() && FContentPackageResolver::IsValidPackageId(Rule.TacticalAmmunitionItemId)
					&& Rule.TacticalMagazineCapacity > 0 && Rule.TacticalMagazineCapacity <= 200
					&& Rule.TacticalAmmunitionPerAttack <= Rule.TacticalMagazineCapacity
					&& Rule.TacticalReloadActionPointCost > 0 && Rule.TacticalReloadActionPointCost <= 20;
			const bool bTacticalArmorValid = Rule.TacticalKineticArmor >= 0 && Rule.TacticalKineticArmor <= 100
				&& Rule.TacticalThermalArmor >= 0 && Rule.TacticalThermalArmor <= 100
				&& Rule.TacticalArcArmor >= 0 && Rule.TacticalArcArmor <= 100
				&& (Rule.Category == FName(TEXT("armor"))
					|| (Rule.TacticalKineticArmor == 0 && Rule.TacticalThermalArmor == 0 && Rule.TacticalArcArmor == 0));
			const int64 BurstAmmunitionCost = static_cast<int64>(Rule.TacticalAmmunitionPerAttack)
				* Rule.TacticalBurstShotCount;
			const bool bTacticalBurstValid = Rule.HasTacticalBurstMode()
				? Rule.IsTacticalWeapon() && Rule.TacticalBurstShotCount <= 8
					&& Rule.TacticalBurstActionPointCost <= 20
					&& Rule.TacticalBurstAccuracyModifier >= -50 && Rule.TacticalBurstAccuracyModifier <= 0
					&& (Rule.TacticalAmmunitionItemId.IsNone()
						|| BurstAmmunitionCost <= Rule.TacticalMagazineCapacity)
					&& !Rule.HasTacticalBlastProfile()
				: Rule.TacticalBurstShotCount == 0 && Rule.TacticalBurstActionPointCost == 0
					&& Rule.TacticalBurstAccuracyModifier == 0;
			const bool bTacticalBlastValid = Rule.HasTacticalBlastProfile()
				? Rule.IsTacticalWeapon() && Rule.TacticalBlastRadius <= 8
					&& Rule.TacticalScatterRadius <= 4
					&& Rule.TacticalBlastFalloffPercent >= 0 && Rule.TacticalBlastFalloffPercent <= 100
					&& Rule.TacticalTerrainDamagePercent <= 300
					&& Rule.TacticalBlastSmoke >= 0 && Rule.TacticalBlastSmoke <= 100
					&& Rule.TacticalBlastFire >= 0 && Rule.TacticalBlastFire <= 100
					&& Rule.TacticalBlastSuppression >= 0 && Rule.TacticalBlastSuppression <= 100
					&& !Rule.HasTacticalBurstMode()
				: Rule.TacticalBlastRadius == 0 && Rule.TacticalScatterRadius == 0
					&& Rule.TacticalBlastFalloffPercent == 0
					&& Rule.TacticalTerrainDamagePercent == 0 && Rule.TacticalBlastSmoke == 0
					&& Rule.TacticalBlastFire == 0 && Rule.TacticalBlastSuppression == 0;
			const bool bTacticalDeviceValid = Rule.IsTacticalDevice()
				? Rule.TacticalRadius <= 8
					&& Rule.TacticalThrowArcHeight >= 0 && Rule.TacticalThrowArcHeight <= 8
					&& Rule.TacticalSmoke >= 0 && Rule.TacticalSmoke <= 100
					&& Rule.TacticalFire >= 0 && Rule.TacticalFire <= 100
					&& Rule.TacticalSuppression >= 0 && Rule.TacticalSuppression <= 100
					&& Rule.TacticalSmokeReduction >= 0 && Rule.TacticalSmokeReduction <= 100
					&& Rule.TacticalFireReduction >= 0 && Rule.TacticalFireReduction <= 100
					&& Rule.TacticalSuppressionReduction >= 0 && Rule.TacticalSuppressionReduction <= 100
					&& Rule.TacticalMoraleRecovery >= 0 && Rule.TacticalMoraleRecovery <= 100
					&& (Rule.TacticalSmoke == 0 || Rule.TacticalSmokeReduction == 0)
					&& (Rule.TacticalFire == 0 || Rule.TacticalFireReduction == 0)
					&& (Rule.TacticalSuppression == 0 || Rule.TacticalSuppressionReduction == 0)
					&& Rule.Power == 0 && Rule.TacticalAccuracyModifier == 0 && Rule.TacticalAmmunitionItemId.IsNone()
				: Rule.TacticalRadius == 0 && Rule.TacticalThrowArcHeight == 0 && Rule.TacticalSmoke == 0
					&& Rule.TacticalFire == 0 && Rule.TacticalSuppression == 0
					&& Rule.TacticalSmokeReduction == 0 && Rule.TacticalFireReduction == 0
					&& Rule.TacticalSuppressionReduction == 0 && Rule.TacticalMoraleRecovery == 0;
			const bool bTacticalProfileValid = Rule.TacticalRange >= 0 && Rule.TacticalRange <= 64
				&& Rule.TacticalAccuracyModifier >= -50 && Rule.TacticalAccuracyModifier <= 50
				&& Rule.TacticalActionPointCost >= 0 && Rule.TacticalActionPointCost <= 20
				&& Rule.TacticalAmmunitionPerAttack > 0 && Rule.TacticalAmmunitionPerAttack <= 20
				&& IsValidDamageType(Rule.TacticalDamageType)
				&& bTacticalAmmunitionValid && bTacticalArmorValid && bTacticalBurstValid
				&& bTacticalBlastValid && bTacticalDeviceValid
				&& (!Rule.IsTacticalSignalProjector() || Rule.Power <= 100)
				&& (Rule.Category != FName(TEXT("device")) || Rule.IsTacticalDevice())
				&& (!bHasAnyTacticalProfile || Rule.IsTacticalWeapon() || Rule.IsTacticalDevice()
					|| Rule.IsTacticalSignalProjector());
			const bool bWeaponProfileValid = Rule.IsCraftWeapon()
				? FContentPackageResolver::IsValidPackageId(Rule.AmmunitionItemId)
					&& Rule.MagazineCapacity > 0 && Rule.SalvoSize > 0 && Rule.SalvoSize <= 16
					&& Rule.InterceptionAccuracy > 0 && Rule.InterceptionAccuracy <= 100
					&& Rule.InterceptionDamage > 0 && Rule.FireIntervalSeconds > 0
				: Rule.AmmunitionItemId.IsNone() && Rule.MagazineCapacity == 0 && Rule.SalvoSize == 0
					&& Rule.InterceptionAccuracy == 0 && Rule.InterceptionDamage == 0 && Rule.FireIntervalSeconds == 0;
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || Rule.Category.IsNone() || Rule.PurchaseCost < 0 || Rule.SellValue < 0 || Rule.Mass < 0 || Rule.Power < 0 || Rule.ManufactureCost < 0 || Rule.ManufactureHours < 0 || !bManufacturingInputsValid || !bTacticalProfileValid || !bWeaponProfileValid)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Item rule '%s' contains invalid identity, economy, or craft-weapon values."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FResearchRule& Rule : Package.Research)
		{
			TSet<FName> RequiredFacilities;
			bool bRequiredFacilitiesValid = Rule.RequiredFacilityIds.Num() <= 4;
			for (const FName FacilityId : Rule.RequiredFacilityIds)
			{
				bRequiredFacilitiesValid &= FContentPackageResolver::IsValidPackageId(FacilityId)
					&& !RequiredFacilities.Contains(FacilityId);
				RequiredFacilities.Add(FacilityId);
			}
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || Rule.Effort <= 0
				|| !bRequiredFacilitiesValid)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(
					TEXT("Research rule '%s' requires a name, positive effort, and up to four distinct facility ids."),
					*Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FKnowledgeArchiveEntryRule& Rule : Package.ArchiveEntries)
		{
			TSet<FName> RelatedEntries;
			bool bRelatedEntriesValid = Rule.RelatedEntryIds.Num() <= 12;
			for (const FName RelatedEntryId : Rule.RelatedEntryIds)
			{
				bRelatedEntriesValid &= FContentPackageResolver::IsValidPackageId(RelatedEntryId)
					&& RelatedEntryId != Rule.Identity.RuleId
					&& !RelatedEntries.Contains(RelatedEntryId);
				RelatedEntries.Add(RelatedEntryId);
			}
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || Rule.DisplayName.Len() > 96
				|| !FContentPackageResolver::IsValidPackageId(Rule.CategoryId)
				|| Rule.Summary.TrimStartAndEnd().IsEmpty() || Rule.Summary.Len() > 280
				|| Rule.Body.TrimStartAndEnd().IsEmpty() || Rule.Body.Len() > 4000
				|| Rule.SortOrder < 0 || Rule.SortOrder > 100000
				|| Rule.RequiredResearch.Num() > 8 || !bRelatedEntriesValid)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId,
					FString::Printf(TEXT("Archive entry '%s' contains invalid category, text, order, research, or related-record values."),
						*Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FFacilityRule& Rule : Package.Facilities)
		{
			const bool bSupplyProfileValid = Rule.BaseDefenseSupplyItemId.IsNone()
				? Rule.BaseDefenseSupplyPerShot == 0
				: FContentPackageResolver::IsValidPackageId(Rule.BaseDefenseSupplyItemId)
					&& Rule.BaseDefenseSupplyPerShot > 0 && Rule.BaseDefenseSupplyPerShot <= 100000
					&& Rule.BaseDefenseAccuracy > 0 && Rule.BaseDefenseDamage > 0;
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || Rule.BuildCost < 0 || Rule.BuildHours <= 0
				|| Rule.MonthlyMaintenance < 0 || Rule.GridWidth <= 0 || Rule.GridHeight <= 0
				|| Rule.CraftCapacity < 0 || Rule.StorageCapacity < 0 || Rule.StorageCapacity > 1000000
				|| Rule.SensorRangeKilometers < 0
				|| Rule.DetectionStrength < 0 || Rule.DetectionStrength > 100
				|| Rule.MaxIntegrity <= 0 || Rule.MaxIntegrity > 100000
				|| Rule.RepairCostPerIntegrity < 0 || Rule.RepairHoursPerIntegrity <= 0
				|| Rule.BaseDefenseAccuracy < 0 || Rule.BaseDefenseAccuracy > 100
				|| Rule.BaseDefenseDamage < 0 || Rule.BaseDefenseDamage > 100000
				|| ((Rule.BaseDefenseAccuracy == 0) != (Rule.BaseDefenseDamage == 0))
				|| !bSupplyProfileValid)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Facility rule '%s' contains invalid construction or grid values."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FPersonnelRoleRule& Rule : Package.PersonnelRoles)
		{
			const bool bValidCategory = Rule.Category == EPersonnelRoleCategory::FieldAgent
				|| Rule.Category == EPersonnelRoleCategory::Scientist
				|| Rule.Category == EPersonnelRoleCategory::Engineer
				|| Rule.Category == EPersonnelRoleCategory::Pilot;
			if (!bValidCategory || Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| Rule.RecruitmentCost < 0 || Rule.MonthlySalary < 0 || Rule.RecruitmentHours <= 0
				|| Rule.BaseHealth <= 0 || Rule.BaseHealth > 200
				|| Rule.BaseAccuracy <= 0 || Rule.BaseAccuracy > 100
				|| Rule.BaseResolve <= 0 || Rule.BaseResolve > 100
				|| Rule.BaseMobility <= 0 || Rule.BaseMobility > 100
				|| Rule.BaseStrength <= 0 || Rule.BaseStrength > 100)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Personnel role '%s' contains invalid cost, timing, or base attributes."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FPersonnelDoctrineRule& Rule : Package.PersonnelDoctrines)
		{
			const int64 TotalBonus = static_cast<int64>(Rule.MaxHealthBonus) + Rule.AccuracyBonus
				+ Rule.ResolveBonus + Rule.MobilityBonus + Rule.StrengthBonus;
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || Rule.Summary.TrimStartAndEnd().IsEmpty()
				|| Rule.MaxSelections <= 0 || Rule.MaxSelections > 10
				|| Rule.MaxHealthBonus < 0 || Rule.MaxHealthBonus > 50
				|| Rule.AccuracyBonus < 0 || Rule.AccuracyBonus > 25
				|| Rule.ResolveBonus < 0 || Rule.ResolveBonus > 25
				|| Rule.MobilityBonus < 0 || Rule.MobilityBonus > 25
				|| Rule.StrengthBonus < 0 || Rule.StrengthBonus > 25
				|| TotalBonus <= 0)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId,
					FString::Printf(TEXT("Personnel doctrine '%s' contains invalid text, selection limits, or bonuses."),
						*Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FPersonnelCommendationRule& Rule : Package.PersonnelCommendations)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || Rule.Summary.TrimStartAndEnd().IsEmpty()
				|| Rule.RequiredMissions <= 0 || Rule.RequiredMissions > 10000
				|| Rule.RequiredKills < 0 || Rule.RequiredKills > 10000
				|| Rule.RequiredRank <= 0 || Rule.RequiredRank > 100)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId,
					FString::Printf(TEXT("Personnel commendation '%s' contains invalid text or service thresholds."),
						*Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FCraftRule& Rule : Package.Craft)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| Rule.PurchaseCost < 0 || Rule.MonthlyMaintenance < 0 || Rule.AcquisitionHours <= 0
				|| Rule.MaxHull <= 0 || Rule.FuelCapacity <= 0
				|| Rule.CruiseSpeedKilometersPerHour <= 0 || Rule.FuelBurnPerHour <= 0
				|| Rule.AgentCapacity < 0 || Rule.CargoCapacity < 0
				|| Rule.EquipmentSlots < 0 || Rule.EquipmentSlots > 16
				|| Rule.RepairCostPerHull < 0 || Rule.RepairHoursPerHull <= 0
				|| Rule.RefuelCostPerUnit < 0 || Rule.RefuelUnitsPerHour <= 0)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Craft rule '%s' contains invalid acquisition, capacity, performance, or service values."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FStrategicRegionRule& Rule : Package.Regions)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| Rule.CenterLongitudeMilliDegrees < -180000 || Rule.CenterLongitudeMilliDegrees > 180000
				|| Rule.CenterLatitudeMilliDegrees < -90000 || Rule.CenterLatitudeMilliDegrees > 90000
				|| Rule.InitialSupport < 0 || Rule.InitialSupport > 100
				|| Rule.FundingWeight <= 0 || Rule.FundingWeight > 1000
				|| Rule.PressureTolerance <= 0 || Rule.PressureTolerance >= 100
				|| Rule.LowPressureSupportRecovery < 0 || Rule.LowPressureSupportRecovery > 20
				|| Rule.HighPressureSupportLossPerTen <= 0 || Rule.HighPressureSupportLossPerTen > 20)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId,
					FString::Printf(TEXT("Region rule '%s' contains invalid center, support, funding, tolerance, or monthly-review values."),
						*Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FContactRule& Rule : Package.Contacts)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| Rule.Signature <= 0 || Rule.Signature > 100
				|| Rule.CruiseSpeedKilometersPerHour <= 0 || Rule.MaxHull <= 0
				|| Rule.ThreatRating <= 0 || Rule.ThreatRating > 10 || Rule.ScoreValue < 0
				|| Rule.AttackAccuracy <= 0 || Rule.AttackAccuracy > 100
				|| Rule.AttackDamage <= 0 || Rule.AttackIntervalSeconds <= 0)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Contact rule '%s' contains invalid signature, speed, hull, threat, or score values."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FAdversaryPlanRule& Rule : Package.AdversaryPlans)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| !FContentPackageResolver::IsValidPackageId(Rule.OpeningMissionRuleId))
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId,
					FString::Printf(TEXT("Adversary plan rule '%s' requires a display name and valid opening mission id."),
						*Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FAdversaryMissionRule& Rule : Package.AdversaryMissions)
		{
			const bool bOriginValid = Rule.OriginLongitudeMilliDegrees >= -180000 && Rule.OriginLongitudeMilliDegrees <= 180000
				&& Rule.OriginLatitudeMilliDegrees >= -90000 && Rule.OriginLatitudeMilliDegrees <= 90000;
			const bool bDestinationValid = Rule.DestinationLongitudeMilliDegrees >= -180000 && Rule.DestinationLongitudeMilliDegrees <= 180000
				&& Rule.DestinationLatitudeMilliDegrees >= -90000 && Rule.DestinationLatitudeMilliDegrees <= 90000;
			const bool bPlanMetadataValid = Rule.PlanId.IsNone()
				? Rule.PlanStage == 0
					&& Rule.EscapeBranchMissionRuleId.IsNone()
					&& Rule.ThwartBranchMissionRuleId.IsNone()
				: FContentPackageResolver::IsValidPackageId(Rule.PlanId)
					&& Rule.PlanStage >= 1 && Rule.PlanStage <= 16
					&& (Rule.EscapeBranchMissionRuleId.IsNone()
						|| FContentPackageResolver::IsValidPackageId(Rule.EscapeBranchMissionRuleId))
					&& (Rule.ThwartBranchMissionRuleId.IsNone()
						|| FContentPackageResolver::IsValidPackageId(Rule.ThwartBranchMissionRuleId));
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| !bPlanMetadataValid
				|| !FContentPackageResolver::IsValidPackageId(Rule.ContactRuleId)
				|| !FContentPackageResolver::IsValidPackageId(Rule.TargetRegionId)
				|| !bOriginValid || !bDestinationValid
				|| (Rule.OriginLongitudeMilliDegrees == Rule.DestinationLongitudeMilliDegrees
					&& Rule.OriginLatitudeMilliDegrees == Rule.DestinationLatitudeMilliDegrees)
				|| Rule.IntervalHours <= 0
				|| Rule.MinimumEscalation <= 0 || Rule.MinimumEscalation > 10
				|| Rule.SelectionWeight <= 0 || Rule.SelectionWeight > 1000000
				|| Rule.PressureOnEscape <= 0 || Rule.PressureOnEscape > 100
				|| Rule.PressureReductionOnDestroyed < 0 || Rule.PressureReductionOnDestroyed > 100
				|| Rule.ScorePenaltyOnEscape < 0 || Rule.FundingPenaltyOnEscape < 0
				|| Rule.SupportLossOnEscape < 0 || Rule.SupportLossOnEscape > 100
				|| Rule.SupportGainOnThwarted < 0 || Rule.SupportGainOnThwarted > 100
				|| Rule.CompactPeerSupportLossOnEscape < 0
				|| Rule.CompactPeerSupportLossOnEscape > 100
				|| Rule.WithdrawnCompactSupportGainOnThwarted < 0
				|| Rule.WithdrawnCompactSupportGainOnThwarted > 100
				|| (Rule.bCreatesLandingSiteOnArrival
					? (Rule.bTargetsPlayerBase || Rule.LandingSiteLifetimeHours <= 0
						|| Rule.LandingSiteLifetimeHours > 720 || Rule.LandingSiteThreatBonus <= 0
						|| Rule.LandingSiteThreatBonus > 9)
					: (Rule.LandingSiteLifetimeHours != 0 || Rule.LandingSiteThreatBonus != 0))
				|| Rule.BaseFacilityDamage < 0 || Rule.BaseFacilityDamage > 100000
				|| Rule.BaseFacilitiesHit < 0 || Rule.BaseFacilitiesHit > 64
				|| ((Rule.BaseFacilityDamage == 0) != (Rule.BaseFacilitiesHit == 0))
				|| (Rule.bTargetsPlayerBase != (Rule.BaseFacilityDamage > 0)))
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Adversary mission rule '%s' contains invalid plan, route, cadence, escalation, pressure, or economy values."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FTacticalTerrainRule& Rule : Package.TacticalTerrains)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| Rule.MoveCost <= 0 || Rule.MoveCost > 20
				|| Rule.CoverPercent < 0 || Rule.CoverPercent > 100
				|| Rule.MaxIntegrity < 0
				|| Rule.BlastResistancePercent < 0 || Rule.BlastResistancePercent > 100
				|| Rule.Flammability < 0 || Rule.Flammability > 100
				|| Rule.VentilationPercent < 0 || Rule.VentilationPercent > 100
				|| Rule.VerticalMoveCost < 0 || Rule.VerticalMoveCost > 20
				|| Rule.ThrowObstacleHeight < 0 || Rule.ThrowObstacleHeight > 8
				|| Rule.DoorActionPointCost < 0 || Rule.DoorActionPointCost > 4
				|| (Rule.ThrowObstacleHeight > 0 && Rule.MaxIntegrity <= 0)
				|| (Rule.DoorActionPointCost > 0 && (Rule.MaxIntegrity <= 0 || !Rule.bBlocksMovement))
				|| (Rule.VerticalMoveCost > 0 && Rule.bBlocksMovement)
				|| (Rule.bBlocksMovement && Rule.MaxIntegrity <= 0))
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Tactical terrain rule '%s' contains invalid movement, cover, integrity, environment, or blocking values."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FTacticalUnitRule& Rule : Package.TacticalUnits)
		{
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty()
				|| Rule.MaxHealth <= 0 || Rule.MaxHealth > 200
				|| Rule.Accuracy <= 0 || Rule.Accuracy > 100
				|| Rule.Resolve <= 0 || Rule.Resolve > 100
				|| Rule.Mobility <= 0 || Rule.Mobility > 100
				|| Rule.Strength <= 0 || Rule.Strength > 100
				|| Rule.ActionPoints <= 0 || Rule.ActionPoints > 20
				|| Rule.AttackRange <= 0 || Rule.AttackRange > 64
				|| Rule.AttackPower <= 0 || Rule.AttackPower > 200
				|| Rule.AttackActionPointCost <= 0 || Rule.AttackActionPointCost > 20
				|| Rule.SignalPower < 0 || Rule.SignalPower > 100
				|| Rule.SignalRange < 0 || Rule.SignalRange > 64
				|| Rule.SignalActionPointCost < 0 || Rule.SignalActionPointCost > 20
				|| ((Rule.SignalPower == 0 || Rule.SignalRange == 0 || Rule.SignalActionPointCost == 0)
					&& (Rule.SignalPower != 0 || Rule.SignalRange != 0 || Rule.SignalActionPointCost != 0))
				|| !IsValidDamageType(Rule.AttackDamageType)
				|| Rule.KineticArmor < 0 || Rule.KineticArmor > 100
				|| Rule.ThermalArmor < 0 || Rule.ThermalArmor > 100
				|| Rule.ArcArmor < 0 || Rule.ArcArmor > 100)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Tactical unit rule '%s' contains invalid combat attributes."), *Rule.Identity.RuleId.ToString()));
			}
		}
		for (const FTacticalMissionRule& Rule : Package.TacticalMissions)
		{
			const int64 MapCells = ComputeTacticalMapCellCount(Rule.MapWidth, Rule.MapHeight, Rule.MapLevels);
			const int64 MaximumEnemies = static_cast<int64>(Rule.BaseEnemyCount) + static_cast<int64>(Rule.EnemiesPerThreat) * 10;
			const bool bKnownAiPosture = Rule.AiPosture == ETacticalAiPosture::Assault
				|| Rule.AiPosture == ETacticalAiPosture::SignalPressure
				|| Rule.AiPosture == ETacticalAiPosture::ObjectivePush
				|| Rule.AiPosture == ETacticalAiPosture::Sentinel;
			const bool bKnownObjectiveType = Rule.ObjectiveType == ETacticalObjectiveType::Disrupt
				|| Rule.ObjectiveType == ETacticalObjectiveType::Recover
				|| Rule.ObjectiveType == ETacticalObjectiveType::Control;
			const bool bKnownContext = Rule.Context == ETacticalMissionContext::StrategicSite
				|| Rule.Context == ETacticalMissionContext::BaseDefense;
			const bool bKnownSiteType = Rule.SiteType == ETacticalSiteType::Wreckage
				|| Rule.SiteType == ETacticalSiteType::Landing;
			const bool bObjectiveRewardValid = Rule.ObjectiveType == ETacticalObjectiveType::Recover
				? FContentPackageResolver::IsValidPackageId(Rule.ObjectiveRewardItemId)
					&& Rule.ObjectiveRewardQuantity > 0 && Rule.ObjectiveRewardQuantity <= 100
				: Rule.ObjectiveRewardItemId.IsNone() && Rule.ObjectiveRewardQuantity == 0;
			if (Rule.DisplayName.TrimStartAndEnd().IsEmpty() || !bKnownContext || !bKnownSiteType || !bKnownAiPosture
				|| (Rule.Context == ETacticalMissionContext::BaseDefense && Rule.MapLevels != 1)
				|| (Rule.Context == ETacticalMissionContext::BaseDefense && Rule.SiteType != ETacticalSiteType::Wreckage)
				|| !FContentPackageResolver::IsValidPackageId(Rule.SourceContactRuleId)
				|| !FContentPackageResolver::IsValidPackageId(Rule.FloorTerrainRuleId)
				|| !FContentPackageResolver::IsValidPackageId(Rule.ObstacleTerrainRuleId)
				|| (!Rule.DoorTerrainRuleId.IsNone() && !FContentPackageResolver::IsValidPackageId(Rule.DoorTerrainRuleId))
				|| (!Rule.VerticalConnectorTerrainRuleId.IsNone() && !FContentPackageResolver::IsValidPackageId(Rule.VerticalConnectorTerrainRuleId))
				|| (Rule.MapLevels > 1 && Rule.VerticalConnectorTerrainRuleId.IsNone())
				|| !FContentPackageResolver::IsValidPackageId(Rule.AdversaryUnitRuleId)
				|| !FContentPackageResolver::IsValidPackageId(Rule.ObjectiveId)
				|| !bKnownObjectiveType || !bObjectiveRewardValid
				|| Rule.ObjectiveRequiredInteractions <= 0 || Rule.ObjectiveRequiredInteractions > 20
				|| Rule.MissionExperienceReward < 0 || Rule.MissionExperienceReward > 10000
				|| Rule.ObjectiveExperienceReward < 0 || Rule.ObjectiveExperienceReward > 10000
				|| Rule.MapWidth < 8 || Rule.MapWidth > 64
				|| Rule.MapHeight < 12 || Rule.MapHeight > 96 || Rule.MapLevels < 1 || Rule.MapLevels > 4 || MapCells > 8192
				|| Rule.DeploymentDepth < 2 || Rule.DeploymentDepth > 8
				|| Rule.MapHeight <= Rule.DeploymentDepth * 2 + 4
				|| Rule.ObstaclePercent < 0 || Rule.ObstaclePercent > 60
				|| Rule.BaseEnemyCount <= 0 || Rule.BaseEnemyCount > 32
				|| Rule.EnemiesPerThreat < 0 || Rule.EnemiesPerThreat > 8
				|| MaximumEnemies >= static_cast<int64>(Rule.MapWidth) * Rule.DeploymentDepth
				|| Rule.TurnLimit <= 0 || Rule.TurnLimit > 500
				|| Rule.ObjectiveActionPointCost <= 0 || Rule.ObjectiveActionPointCost > 20
				|| Rule.ExtractionActionPointCost <= 0 || Rule.ExtractionActionPointCost > 20)
			{
				AddError(Result, TEXT("invalid_rule_value"), PackageId, FString::Printf(TEXT("Tactical mission rule '%s' contains invalid map, deployment, population, objective, AI-posture, or turn-limit values."), *Rule.Identity.RuleId.ToString()));
			}
		}
	}

	void ValidateResearchReference(
		const FName SourceRuleId,
		const FName RequiredResearchId,
		const FName Origin,
		const FResolvedRuleSet& RuleSet,
		FRuleSetBuildResult& Result)
	{
		if (!RuleSet.Research.Contains(RequiredResearchId))
		{
			AddError(Result, TEXT("missing_research_reference"), Origin, FString::Printf(TEXT("Rule '%s' requires undefined research '%s'."), *SourceRuleId.ToString(), *RequiredResearchId.ToString()));
		}
	}

	void ValidateReferencesAndCycles(FRuleSetBuildResult& Result)
	{
		const FResolvedRuleSet& RuleSet = Result.RuleSet;
		int64 TotalRegionFundingWeight = 0;
		for (const TPair<FName, FStrategicRegionRule>& Pair : RuleSet.Regions)
		{
			TotalRegionFundingWeight += Pair.Value.FundingWeight;
		}
		if (TotalRegionFundingWeight > MAX_int32)
		{
			AddError(Result, TEXT("region_funding_weight_overflow"), NAME_None,
				TEXT("Combined regional funding weights exceed the deterministic allocation range."));
		}
		for (const TPair<FName, FItemRule>& Pair : RuleSet.Items)
		{
			for (const FName Requirement : Pair.Value.RequiredResearch)
			{
				ValidateResearchReference(Pair.Key, Requirement, RuleSet.ItemOrigins.FindRef(Pair.Key), RuleSet, Result);
			}
			if (Pair.Value.IsCraftWeapon())
			{
				const FItemRule* Ammunition = RuleSet.Items.Find(Pair.Value.AmmunitionItemId);
				if (Ammunition == nullptr || Ammunition->Category != FName(TEXT("craft-ammunition")))
				{
					AddError(Result, TEXT("missing_ammunition_reference"), RuleSet.ItemOrigins.FindRef(Pair.Key), FString::Printf(TEXT("Craft weapon '%s' requires undefined or non-ammunition item '%s'."), *Pair.Key.ToString(), *Pair.Value.AmmunitionItemId.ToString()));
				}
			}
			if (Pair.Value.IsTacticalWeapon() && !Pair.Value.TacticalAmmunitionItemId.IsNone())
			{
				const FItemRule* Ammunition = RuleSet.Items.Find(Pair.Value.TacticalAmmunitionItemId);
				if (Ammunition == nullptr || Ammunition->Category != FName(TEXT("ammunition")))
				{
					AddError(Result, TEXT("missing_tactical_ammunition_reference"), RuleSet.ItemOrigins.FindRef(Pair.Key), FString::Printf(TEXT("Tactical weapon '%s' requires undefined or non-ammunition item '%s'."), *Pair.Key.ToString(), *Pair.Value.TacticalAmmunitionItemId.ToString()));
				}
			}
			for (const FManufacturingInputRule& Input : Pair.Value.ManufactureInputs)
			{
				if (!RuleSet.Items.Contains(Input.ItemId))
				{
					AddError(Result, TEXT("missing_manufacturing_input_reference"),
						RuleSet.ItemOrigins.FindRef(Pair.Key), FString::Printf(
							TEXT("Manufacturing recipe '%s' requires undefined item '%s'."),
							*Pair.Key.ToString(), *Input.ItemId.ToString()));
				}
			}
		}
		for (const TPair<FName, FKnowledgeArchiveEntryRule>& Pair : RuleSet.ArchiveEntries)
		{
			const FName Origin = RuleSet.ArchiveEntryOrigins.FindRef(Pair.Key);
			for (const FName Requirement : Pair.Value.RequiredResearch)
			{
				ValidateResearchReference(Pair.Key, Requirement, Origin, RuleSet, Result);
			}
			for (const FName RelatedEntryId : Pair.Value.RelatedEntryIds)
			{
				if (RelatedEntryId == Pair.Key)
				{
					AddError(Result, TEXT("self_archive_reference"), Origin,
						FString::Printf(TEXT("Archive entry '%s' cannot link to itself."), *Pair.Key.ToString()));
				}
				else if (!RuleSet.ArchiveEntries.Contains(RelatedEntryId))
				{
					AddError(Result, TEXT("missing_archive_reference"), Origin,
						FString::Printf(TEXT("Archive entry '%s' links to undefined record '%s'."),
							*Pair.Key.ToString(), *RelatedEntryId.ToString()));
				}
			}
		}
		for (const TPair<FName, FFacilityRule>& Pair : RuleSet.Facilities)
		{
			if (!Pair.Value.BaseDefenseSupplyItemId.IsNone())
			{
				const FItemRule* Supply = RuleSet.Items.Find(Pair.Value.BaseDefenseSupplyItemId);
				if (Supply == nullptr || Supply->Category != FName(TEXT("base-defense-supply")))
				{
					AddError(Result, TEXT("missing_base_defense_supply_reference"),
						RuleSet.FacilityOrigins.FindRef(Pair.Key), FString::Printf(
							TEXT("Base-defense facility '%s' requires undefined or non-defense-supply item '%s'."),
							*Pair.Key.ToString(), *Pair.Value.BaseDefenseSupplyItemId.ToString()));
				}
			}
			for (const FName Requirement : Pair.Value.RequiredResearch)
			{
				ValidateResearchReference(Pair.Key, Requirement, RuleSet.FacilityOrigins.FindRef(Pair.Key), RuleSet, Result);
			}
		}
		for (const TPair<FName, FPersonnelRoleRule>& Pair : RuleSet.PersonnelRoles)
		{
			for (const FName Requirement : Pair.Value.RequiredResearch)
			{
				ValidateResearchReference(Pair.Key, Requirement, RuleSet.PersonnelRoleOrigins.FindRef(Pair.Key), RuleSet, Result);
			}
		}
		for (const TPair<FName, FCraftRule>& Pair : RuleSet.Craft)
		{
			for (const FName Requirement : Pair.Value.RequiredResearch)
			{
				ValidateResearchReference(Pair.Key, Requirement, RuleSet.CraftOrigins.FindRef(Pair.Key), RuleSet, Result);
			}
		}
		for (const TPair<FName, FAdversaryPlanRule>& Pair : RuleSet.AdversaryPlans)
		{
			const FAdversaryMissionRule* OpeningMission = RuleSet.AdversaryMissions.Find(Pair.Value.OpeningMissionRuleId);
			if (OpeningMission == nullptr)
			{
				AddError(Result, TEXT("missing_adversary_plan_opening"), RuleSet.AdversaryPlanOrigins.FindRef(Pair.Key),
					FString::Printf(TEXT("Adversary plan '%s' references undefined opening mission '%s'."),
						*Pair.Key.ToString(), *Pair.Value.OpeningMissionRuleId.ToString()));
			}
			else if (OpeningMission->PlanId != Pair.Key || OpeningMission->PlanStage != 1)
			{
				AddError(Result, TEXT("invalid_adversary_plan_opening"), RuleSet.AdversaryPlanOrigins.FindRef(Pair.Key),
					FString::Printf(TEXT("Adversary plan '%s' opening mission '%s' must belong to that plan at stage one."),
						*Pair.Key.ToString(), *Pair.Value.OpeningMissionRuleId.ToString()));
			}
		}

		int64 TotalMissionWeight = 0;
		bool bHasOpeningMission = RuleSet.AdversaryMissions.IsEmpty();
		for (const TPair<FName, FAdversaryMissionRule>& Pair : RuleSet.AdversaryMissions)
		{
			const FName Origin = RuleSet.AdversaryMissionOrigins.FindRef(Pair.Key);
			const FAdversaryPlanRule* Plan = Pair.Value.PlanId.IsNone()
				? nullptr
				: RuleSet.AdversaryPlans.Find(Pair.Value.PlanId);
			if (!Pair.Value.PlanId.IsNone() && Plan == nullptr)
			{
				AddError(Result, TEXT("missing_adversary_plan_reference"), Origin,
					FString::Printf(TEXT("Adversary mission '%s' references undefined plan '%s'."),
						*Pair.Key.ToString(), *Pair.Value.PlanId.ToString()));
			}
			if (!RuleSet.Regions.IsEmpty() && !RuleSet.Regions.Contains(Pair.Value.TargetRegionId))
			{
				AddError(Result, TEXT("missing_region_reference"), Origin,
					FString::Printf(TEXT("Adversary mission '%s' references undefined region '%s'."),
						*Pair.Key.ToString(), *Pair.Value.TargetRegionId.ToString()));
			}

			auto ValidateBranch = [&](const FName BranchMissionRuleId, const TCHAR* Outcome)
			{
				if (BranchMissionRuleId.IsNone())
				{
					return;
				}
				const FAdversaryMissionRule* BranchMission = RuleSet.AdversaryMissions.Find(BranchMissionRuleId);
				if (BranchMission == nullptr)
				{
					AddError(Result, TEXT("missing_adversary_plan_branch"), Origin,
						FString::Printf(TEXT("Adversary mission '%s' %s branch references undefined mission '%s'."),
							*Pair.Key.ToString(), Outcome, *BranchMissionRuleId.ToString()));
				}
				else if (Pair.Value.PlanId.IsNone()
					|| BranchMission->PlanId != Pair.Value.PlanId
					|| static_cast<int64>(BranchMission->PlanStage)
						!= static_cast<int64>(Pair.Value.PlanStage) + 1)
				{
					AddError(Result, TEXT("invalid_adversary_plan_branch"), Origin,
						FString::Printf(TEXT("Adversary mission '%s' %s branch '%s' must remain in the same plan at the next stage."),
							*Pair.Key.ToString(), Outcome, *BranchMissionRuleId.ToString()));
				}
			};
			ValidateBranch(Pair.Value.EscapeBranchMissionRuleId, TEXT("escape"));
			ValidateBranch(Pair.Value.ThwartBranchMissionRuleId, TEXT("thwart"));

			if (!RuleSet.Contacts.Contains(Pair.Value.ContactRuleId))
			{
				AddError(Result, TEXT("missing_contact_reference"), Origin, FString::Printf(TEXT("Adversary mission '%s' references undefined contact '%s'."), *Pair.Key.ToString(), *Pair.Value.ContactRuleId.ToString()));
			}
			else if (Pair.Value.bCreatesLandingSiteOnArrival)
			{
				const FContactRule& Contact = RuleSet.Contacts.FindChecked(Pair.Value.ContactRuleId);
				if (static_cast<int64>(Contact.ThreatRating)
					+ static_cast<int64>(Pair.Value.LandingSiteThreatBonus) > 10)
				{
					AddError(Result, TEXT("landing_site_threat_overflow"), Origin,
						FString::Printf(TEXT("Adversary mission '%s' landing-site threat exceeds the supported maximum."), *Pair.Key.ToString()));
				}
				bool bHasLandingRecipe = false;
				for (const TPair<FName, FTacticalMissionRule>& TacticalPair : RuleSet.TacticalMissions)
				{
					bHasLandingRecipe |= TacticalPair.Value.Context == ETacticalMissionContext::StrategicSite
						&& TacticalPair.Value.SiteType == ETacticalSiteType::Landing
						&& TacticalPair.Value.SourceContactRuleId == Pair.Value.ContactRuleId;
				}
				if (!bHasLandingRecipe)
				{
					AddError(Result, TEXT("missing_landing_tactical_mapping"), Origin,
						FString::Printf(TEXT("Adversary mission '%s' creates a landing site without a matching tactical recipe."), *Pair.Key.ToString()));
				}
			}

			const bool bEligibleForWeightedScheduling = Pair.Value.PlanId.IsNone()
				|| (Plan != nullptr && Plan->OpeningMissionRuleId == Pair.Key);
			if (bEligibleForWeightedScheduling)
			{
				bHasOpeningMission |= Pair.Value.MinimumEscalation == 1;
				TotalMissionWeight += Pair.Value.SelectionWeight;
			}
		}

		TSet<FName> ReachablePlanMissions;
		TArray<FName> PendingPlanMissions;
		for (const TPair<FName, FAdversaryPlanRule>& Pair : RuleSet.AdversaryPlans)
		{
			const FAdversaryMissionRule* OpeningMission = RuleSet.AdversaryMissions.Find(Pair.Value.OpeningMissionRuleId);
			if (OpeningMission != nullptr && OpeningMission->PlanId == Pair.Key && OpeningMission->PlanStage == 1)
			{
				PendingPlanMissions.Add(Pair.Value.OpeningMissionRuleId);
			}
		}
		while (!PendingPlanMissions.IsEmpty())
		{
			const FName MissionRuleId = PendingPlanMissions.Pop(EAllowShrinking::No);
			if (ReachablePlanMissions.Contains(MissionRuleId))
			{
				continue;
			}
			ReachablePlanMissions.Add(MissionRuleId);
			const FAdversaryMissionRule& Mission = RuleSet.AdversaryMissions.FindChecked(MissionRuleId);
			for (const FName BranchRuleId : { Mission.EscapeBranchMissionRuleId, Mission.ThwartBranchMissionRuleId })
			{
				const FAdversaryMissionRule* Branch = RuleSet.AdversaryMissions.Find(BranchRuleId);
				if (Branch != nullptr && Branch->PlanId == Mission.PlanId
					&& static_cast<int64>(Branch->PlanStage)
						== static_cast<int64>(Mission.PlanStage) + 1)
				{
					PendingPlanMissions.Add(BranchRuleId);
				}
			}
		}
		for (const TPair<FName, FAdversaryMissionRule>& Pair : RuleSet.AdversaryMissions)
		{
			if (!Pair.Value.PlanId.IsNone() && !ReachablePlanMissions.Contains(Pair.Key))
			{
				AddError(Result, TEXT("orphaned_adversary_plan_mission"), RuleSet.AdversaryMissionOrigins.FindRef(Pair.Key),
					FString::Printf(TEXT("Adversary plan mission '%s' is unreachable from its plan opening."), *Pair.Key.ToString()));
			}
		}
		if (TotalMissionWeight > MAX_int32)
		{
			AddError(Result, TEXT("adversary_weight_overflow"), NAME_None, TEXT("Combined adversary mission selection weights exceed the deterministic selection range."));
		}
		if (!bHasOpeningMission)
		{
			AddError(Result, TEXT("missing_opening_adversary_mission"), NAME_None, TEXT("At least one adversary mission must be eligible at escalation level one."));
		}

		TSet<FString> TacticalSourceContacts;
		for (const TPair<FName, FTacticalMissionRule>& Pair : RuleSet.TacticalMissions)
		{
			const FName Origin = RuleSet.TacticalMissionOrigins.FindRef(Pair.Key);
			const FTacticalTerrainRule* Floor = RuleSet.TacticalTerrains.Find(Pair.Value.FloorTerrainRuleId);
			const FTacticalTerrainRule* Obstacle = RuleSet.TacticalTerrains.Find(Pair.Value.ObstacleTerrainRuleId);
			const FTacticalTerrainRule* Door = Pair.Value.DoorTerrainRuleId.IsNone()
				? nullptr
				: RuleSet.TacticalTerrains.Find(Pair.Value.DoorTerrainRuleId);
			const FTacticalTerrainRule* VerticalConnector = Pair.Value.VerticalConnectorTerrainRuleId.IsNone()
				? nullptr
				: RuleSet.TacticalTerrains.Find(Pair.Value.VerticalConnectorTerrainRuleId);
			if (!RuleSet.Contacts.Contains(Pair.Value.SourceContactRuleId))
			{
				AddError(Result, TEXT("missing_contact_reference"), Origin, FString::Printf(TEXT("Tactical mission '%s' references undefined contact '%s'."), *Pair.Key.ToString(), *Pair.Value.SourceContactRuleId.ToString()));
			}
			if (Floor == nullptr || Floor->bBlocksMovement)
			{
				AddError(Result, TEXT("invalid_tactical_floor_reference"), Origin, FString::Printf(TEXT("Tactical mission '%s' requires a non-blocking floor terrain '%s'."), *Pair.Key.ToString(), *Pair.Value.FloorTerrainRuleId.ToString()));
			}
			if (Obstacle == nullptr || !Obstacle->bBlocksMovement)
			{
				AddError(Result, TEXT("invalid_tactical_obstacle_reference"), Origin, FString::Printf(TEXT("Tactical mission '%s' requires a blocking obstacle terrain '%s'."), *Pair.Key.ToString(), *Pair.Value.ObstacleTerrainRuleId.ToString()));
			}
			if (!Pair.Value.DoorTerrainRuleId.IsNone() && (Door == nullptr || !Door->IsDoor()))
			{
				AddError(Result, TEXT("invalid_tactical_door_reference"), Origin, FString::Printf(TEXT("Tactical mission '%s' requires an openable door terrain '%s'."), *Pair.Key.ToString(), *Pair.Value.DoorTerrainRuleId.ToString()));
			}
			if ((!Pair.Value.VerticalConnectorTerrainRuleId.IsNone() && (VerticalConnector == nullptr || !VerticalConnector->IsVerticalConnector()))
				|| (Pair.Value.MapLevels > 1 && VerticalConnector == nullptr))
			{
				AddError(Result, TEXT("invalid_tactical_vertical_connector_reference"), Origin, FString::Printf(TEXT("Tactical mission '%s' requires traversable vertical connector terrain '%s'."), *Pair.Key.ToString(), *Pair.Value.VerticalConnectorTerrainRuleId.ToString()));
			}
			if (!RuleSet.TacticalUnits.Contains(Pair.Value.AdversaryUnitRuleId))
			{
				AddError(Result, TEXT("missing_tactical_unit_reference"), Origin, FString::Printf(TEXT("Tactical mission '%s' references undefined unit '%s'."), *Pair.Key.ToString(), *Pair.Value.AdversaryUnitRuleId.ToString()));
			}
			if (Pair.Value.ObjectiveType == ETacticalObjectiveType::Recover
				&& !RuleSet.Items.Contains(Pair.Value.ObjectiveRewardItemId))
			{
				AddError(Result, TEXT("missing_tactical_reward_reference"), Origin, FString::Printf(TEXT("Tactical recovery mission '%s' references undefined reward item '%s'."), *Pair.Key.ToString(), *Pair.Value.ObjectiveRewardItemId.ToString()));
			}
			const FString TacticalSourceKey = FString::Printf(TEXT("%d:%d:%s"),
				static_cast<int32>(Pair.Value.Context),
				Pair.Value.Context == ETacticalMissionContext::StrategicSite
					? static_cast<int32>(Pair.Value.SiteType)
					: 0,
				*Pair.Value.SourceContactRuleId.ToString());
			if (TacticalSourceContacts.Contains(TacticalSourceKey))
			{
				AddError(Result, TEXT("duplicate_tactical_contact_mapping"), Origin, FString::Printf(TEXT("More than one tactical mission maps contact '%s' in context %d and site type %d."), *Pair.Value.SourceContactRuleId.ToString(), static_cast<int32>(Pair.Value.Context), static_cast<int32>(Pair.Value.SiteType)));
			}
			TacticalSourceContacts.Add(TacticalSourceKey);
			bool bHasLandingSource = false;
			for (const TPair<FName, FAdversaryMissionRule>& MissionPair : RuleSet.AdversaryMissions)
			{
				bHasLandingSource |= MissionPair.Value.bCreatesLandingSiteOnArrival
					&& MissionPair.Value.ContactRuleId == Pair.Value.SourceContactRuleId;
			}
			if (Pair.Value.Context == ETacticalMissionContext::StrategicSite
				&& Pair.Value.SiteType == ETacticalSiteType::Landing
				&& !bHasLandingSource)
			{
				AddError(Result, TEXT("orphaned_landing_tactical_mapping"), Origin,
					FString::Printf(TEXT("Landing tactical mission '%s' has no landing-capable adversary mission."), *Pair.Key.ToString()));
			}
		}

		TMap<FName, int32> RemainingPrerequisites;
		TMap<FName, TArray<FName>> Dependents;
		for (const TPair<FName, FResearchRule>& Pair : RuleSet.Research)
		{
			const FName Origin = RuleSet.ResearchOrigins.FindRef(Pair.Key);
			for (const FName FacilityId : Pair.Value.RequiredFacilityIds)
			{
				const FFacilityRule* Facility = RuleSet.Facilities.Find(FacilityId);
				if (Facility == nullptr)
				{
					AddError(Result, TEXT("missing_research_facility_reference"), Origin,
						FString::Printf(TEXT("Research '%s' requires undefined facility '%s'."),
							*Pair.Key.ToString(), *FacilityId.ToString()));
				}
				else if (Facility->RequiredResearch.Contains(Pair.Key))
				{
					AddError(Result, TEXT("cyclic_research_facility_requirement"), Origin,
						FString::Printf(TEXT("Research '%s' requires facility '%s', but that facility requires the same research."),
							*Pair.Key.ToString(), *FacilityId.ToString()));
				}
			}
			int32 ValidPrerequisites = 0;
			for (const FName Prerequisite : Pair.Value.Prerequisites)
			{
				if (RuleSet.Research.Contains(Prerequisite))
				{
					++ValidPrerequisites;
					Dependents.FindOrAdd(Prerequisite).Add(Pair.Key);
				}
				else
				{
					ValidateResearchReference(Pair.Key, Prerequisite, Origin, RuleSet, Result);
				}
			}
			RemainingPrerequisites.Add(Pair.Key, ValidPrerequisites);

			for (const FName UnlockId : Pair.Value.UnlockRuleIds)
			{
				if (!RuleSet.ContainsAnyRule(UnlockId))
				{
					AddError(Result, TEXT("missing_unlock_reference"), Origin, FString::Printf(TEXT("Research '%s' unlocks undefined rule '%s'."), *Pair.Key.ToString(), *UnlockId.ToString()));
				}
			}
		}

		TArray<FName> Ready;
		for (const TPair<FName, int32>& Pair : RemainingPrerequisites)
		{
			if (Pair.Value == 0)
			{
				Ready.Add(Pair.Key);
			}
		}
		Ready.Sort(FNameLexicalLess());

		int32 Visited = 0;
		while (!Ready.IsEmpty())
		{
			const FName ResearchId = Ready[0];
			Ready.RemoveAt(0, EAllowShrinking::No);
			++Visited;

			TArray<FName> Children = Dependents.FindRef(ResearchId);
			Children.Sort(FNameLexicalLess());
			for (const FName Child : Children)
			{
				int32& Remaining = RemainingPrerequisites.FindChecked(Child);
				--Remaining;
				if (Remaining == 0)
				{
					Ready.Add(Child);
				}
			}
			Ready.Sort(FNameLexicalLess());
		}

		if (Visited != RuleSet.Research.Num())
		{
			TArray<FString> CyclicIds;
			for (const TPair<FName, int32>& Pair : RemainingPrerequisites)
			{
				if (Pair.Value > 0)
				{
					CyclicIds.Add(Pair.Key.ToString());
				}
			}
			CyclicIds.Sort();
			AddError(Result, TEXT("research_cycle"), NAME_None, FString::Printf(TEXT("Research dependency cycle involves: %s."), *FString::Join(CyclicIds, TEXT(", "))));
		}
	}
}

bool FResolvedRuleSet::ContainsAnyRule(const FName RuleId) const
{
	return Items.Contains(RuleId) || Research.Contains(RuleId) || ArchiveEntries.Contains(RuleId)
		|| Facilities.Contains(RuleId) || PersonnelRoles.Contains(RuleId)
		|| PersonnelDoctrines.Contains(RuleId) || PersonnelCommendations.Contains(RuleId)
		|| Craft.Contains(RuleId) || Regions.Contains(RuleId) || Contacts.Contains(RuleId) || AdversaryPlans.Contains(RuleId) || AdversaryMissions.Contains(RuleId)
		|| TacticalTerrains.Contains(RuleId) || TacticalUnits.Contains(RuleId) || TacticalMissions.Contains(RuleId);
}

bool FRuleSetBuildResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FContentDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == Code;
		});
}

FRuleSetBuildResult FRuleSetBuilder::Build(const TArray<FContentPackage>& Packages)
{
	using namespace RuleSetBuilderPrivate;

	FRuleSetBuildResult Result;
	TArray<FContentPackageDescriptor> Descriptors;
	Descriptors.Reserve(Packages.Num());
	for (const FContentPackage& Package : Packages)
	{
		Descriptors.Add(Package.Descriptor);
	}

	const FContentResolution Resolution = FContentPackageResolver::Resolve(Descriptors);
	Result.Diagnostics = Resolution.Diagnostics;
	if (!Resolution.bSucceeded)
	{
		return Result;
	}
	Result.PackageLoadOrder = Resolution.LoadOrder;

	TMap<FName, const FContentPackage*> PackagesById;
	for (const FContentPackage& Package : Packages)
	{
		PackagesById.Add(Package.Descriptor.PackageId, &Package);
	}

	for (const FName PackageId : Resolution.LoadOrder)
	{
		const FContentPackage& Package = *PackagesById.FindChecked(PackageId);
		ValidateRuleValues(PackageId, Package, Result);
		ApplyRules(PackageId, TEXT("Item"), Package.Items, Result.RuleSet.Items, Result.RuleSet.ItemOrigins, Result);
		ApplyRules(PackageId, TEXT("Research"), Package.Research, Result.RuleSet.Research, Result.RuleSet.ResearchOrigins, Result);
		ApplyRules(PackageId, TEXT("Archive entry"), Package.ArchiveEntries, Result.RuleSet.ArchiveEntries, Result.RuleSet.ArchiveEntryOrigins, Result);
		ApplyRules(PackageId, TEXT("Facility"), Package.Facilities, Result.RuleSet.Facilities, Result.RuleSet.FacilityOrigins, Result);
		ApplyRules(PackageId, TEXT("Personnel role"), Package.PersonnelRoles, Result.RuleSet.PersonnelRoles, Result.RuleSet.PersonnelRoleOrigins, Result);
		ApplyRules(PackageId, TEXT("Personnel doctrine"), Package.PersonnelDoctrines, Result.RuleSet.PersonnelDoctrines, Result.RuleSet.PersonnelDoctrineOrigins, Result);
		ApplyRules(PackageId, TEXT("Personnel commendation"), Package.PersonnelCommendations, Result.RuleSet.PersonnelCommendations, Result.RuleSet.PersonnelCommendationOrigins, Result);
		ApplyRules(PackageId, TEXT("Craft"), Package.Craft, Result.RuleSet.Craft, Result.RuleSet.CraftOrigins, Result);
		ApplyRules(PackageId, TEXT("Region"), Package.Regions, Result.RuleSet.Regions, Result.RuleSet.RegionOrigins, Result);
		ApplyRules(PackageId, TEXT("Contact"), Package.Contacts, Result.RuleSet.Contacts, Result.RuleSet.ContactOrigins, Result);
		ApplyRules(PackageId, TEXT("Adversary plan"), Package.AdversaryPlans, Result.RuleSet.AdversaryPlans, Result.RuleSet.AdversaryPlanOrigins, Result);
		ApplyRules(PackageId, TEXT("Adversary mission"), Package.AdversaryMissions, Result.RuleSet.AdversaryMissions, Result.RuleSet.AdversaryMissionOrigins, Result);
		ApplyRules(PackageId, TEXT("Tactical terrain"), Package.TacticalTerrains, Result.RuleSet.TacticalTerrains, Result.RuleSet.TacticalTerrainOrigins, Result);
		ApplyRules(PackageId, TEXT("Tactical unit"), Package.TacticalUnits, Result.RuleSet.TacticalUnits, Result.RuleSet.TacticalUnitOrigins, Result);
		ApplyRules(PackageId, TEXT("Tactical mission"), Package.TacticalMissions, Result.RuleSet.TacticalMissions, Result.RuleSet.TacticalMissionOrigins, Result);
	}

	ValidateReferencesAndCycles(Result);
	Result.bSucceeded = !HasErrors(Result);
	if (!Result.bSucceeded)
	{
		Result.RuleSet = FResolvedRuleSet();
	}
	return Result;
}
