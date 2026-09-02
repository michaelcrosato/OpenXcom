#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/ContentPackageResolver.h"
#include "Tactical/TacticalObjectiveTypes.h"

#include "RuleTypes.generated.h"

/** Shared identity and explicit package-override policy for a typed rule. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRuleIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName RuleId;

	/** Must be true when intentionally replacing a rule from an earlier package. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	bool bReplaceExisting = false;
};

UENUM(BlueprintType)
enum class ETacticalDamageType : uint8
{
	Kinetic,
	Thermal,
	Arc
};

UENUM(BlueprintType)
enum class ETacticalFireMode : uint8
{
	Single,
	Burst
};

/** One deterministic per-unit inventory input for a manufacturing recipe. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FManufacturingInputRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FItemRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 PurchaseCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 SellValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 Mass = 0;

	/** Generic effectiveness used by the first combat/equipment schema revision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 Power = 0;

	/** Maximum Euclidean grid range when this item is used as a tactical weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "64"))
	int32 TacticalRange = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "-50", ClampMax = "50"))
	int32 TacticalAccuracyModifier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "20"))
	int32 TacticalActionPointCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	ETacticalDamageType TacticalDamageType = ETacticalDamageType::Kinetic;

	/** Optional magazine item; unset weapons use an unlimited abstract feed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName TacticalAmmunitionItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "200"))
	int32 TacticalMagazineCapacity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 TacticalAmmunitionPerAttack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "20"))
	int32 TacticalReloadActionPointCost = 0;

	/** Optional alternate fire mode. Zero shot count means this weapon is single-shot only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "8"))
	int32 TacticalBurstShotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "20"))
	int32 TacticalBurstActionPointCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "-50", ClampMax = "0"))
	int32 TacticalBurstAccuracyModifier = 0;

	/** Ground-target blast profile. Blast weapons use single fire and may affect either team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "8"))
	int32 TacticalBlastRadius = 0;

	/** Maximum square-grid deviation from the aimed cell; zero means exact impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "4"))
	int32 TacticalScatterRadius = 0;

	/** Percentage points of damage/effect strength lost per grid cell from the impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalBlastFalloffPercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "300"))
	int32 TacticalTerrainDamagePercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalBlastSmoke = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalBlastFire = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalBlastSuppression = 0;

	/** Area radius for a consumable tactical device; zero for ordinary items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "8"))
	int32 TacticalRadius = 0;

	/** Peak clearance of a deterministic parabolic throw; zero retains direct line-of-sight deployment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "8"))
	int32 TacticalThrowArcHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalSmoke = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalFire = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalSuppression = 0;

	/** Flat smoke intensity removed from each affected cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalSmokeReduction = 0;

	/** Flat fire intensity removed from each affected cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalFireReduction = 0;

	/** Flat suppression removed from each living unit in the affected area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalSuppressionReduction = 0;

	/** Flat morale restored to each living unit in the affected area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalMoraleRecovery = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalKineticArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalThermalArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalArcArmor = 0;

	/** Per-unit funds consumed when a manufacturing order is accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 ManufactureCost = 0;

	/** Engineer-hours per unit; zero means this item cannot be manufactured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 ManufactureHours = 0;

	/** Inventory reserved per manufactured unit and returned only for untouched cancelled units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FManufacturingInputRule> ManufactureInputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RequiredResearch;

	/** Required inventory item for craft-weapon ammunition; unset for non-weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName AmmunitionItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 MagazineCapacity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "16"))
	int32 SalvoSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 InterceptionAccuracy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 InterceptionDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 FireIntervalSeconds = 0;

	bool IsManufacturable() const { return ManufactureHours > 0; }
	bool IsCraftWeapon() const { return Category == FName(TEXT("craft-weapon")); }
	bool IsTacticalWeapon() const { return Category == FName(TEXT("weapon")) && Power > 0 && TacticalRange > 0 && TacticalActionPointCost > 0; }
	bool HasTacticalBurstMode() const { return TacticalBurstShotCount >= 2 && TacticalBurstActionPointCost > 0; }
	bool HasTacticalBlastProfile() const { return TacticalBlastRadius > 0 && TacticalTerrainDamagePercent > 0; }
	bool HasTacticalThrowArc() const { return IsTacticalDevice() && TacticalThrowArcHeight > 0; }
	/** Non-consumable field sensor that can project resolve pressure against a visible hostile. */
	bool IsTacticalSignalProjector() const
	{
		return Category == FName(TEXT("sensor")) && Power > 0
			&& TacticalRange > 0 && TacticalActionPointCost > 0;
	}
	bool IsTacticalDevice() const
	{
		return Category == FName(TEXT("device")) && TacticalRange > 0 && TacticalActionPointCost > 0
			&& TacticalRadius > 0 && (TacticalSmoke > 0 || TacticalFire > 0 || TacticalSuppression > 0
				|| TacticalSmokeReduction > 0 || TacticalFireReduction > 0
				|| TacticalSuppressionReduction > 0 || TacticalMoraleRecovery > 0);
	}
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FResearchRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	/** Abstract scientist-hours required to complete the topic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 Effort = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> Prerequisites;

	/** Every listed facility must be operational at the project's base to start or advance this topic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RequiredFacilityIds;

	/** Rule ids made available when this topic completes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> UnlockRuleIds;
};

/** One original, research-gated record in the player-facing knowledge archive. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FKnowledgeArchiveEntryRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString Summary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 SortOrder = 0;

	/** Every listed topic must be complete before this record is exposed to presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RequiredResearch;

	/** Optional links to other archive records; links do not bypass those records' research gates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RelatedEntryIds;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FFacilityRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 BuildCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 BuildHours = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 MonthlyMaintenance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 GridWidth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 GridHeight = 1;

	/** Number of operational craft berths supplied by this facility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 CraftCapacity = 0;

	/** Mass-weighted inventory capacity supplied while this facility is operational. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 StorageCapacity = 0;

	/** Scientist capacity added to the base-local compatibility allowance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 ScientistCapacity = 0;

	/** Engineer capacity added to the base-local compatibility allowance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 EngineerCapacity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 SensorRangeKilometers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DetectionStrength = 0;

	/** Damage capacity before this facility goes offline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 MaxIntegrity = 100;

	/** Funds reserved for each point of damage repaired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 RepairCostPerIntegrity = 0;

	/** Elapsed strategic hours required for each point repaired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 RepairHoursPerIntegrity = 1;

	/** Accuracy of one automatic base-defense shot when this facility is operational; zero disables defense fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BaseDefenseAccuracy = 0;

	/** Hull damage dealt by a successful automatic base-defense shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 BaseDefenseDamage = 0;

	/** Optional inventory item consumed when this battery fires; unset preserves legacy unlimited fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName BaseDefenseSupplyItemId;

	/** Units of BaseDefenseSupplyItemId consumed by one accepted automatic-defense shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 BaseDefenseSupplyPerShot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RequiredResearch;

	/** Integrity-scaled contribution using deterministic ceiling division; legacy/full-integrity facilities return FullValue. */
	int32 ScaleEffectByIntegrity(int32 FullValue, int32 Damage) const;
};

UENUM(BlueprintType)
enum class EPersonnelRoleCategory : uint8
{
	FieldAgent,
	Scientist,
	Engineer,
	Pilot
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelRoleRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	EPersonnelRoleCategory Category = EPersonnelRoleCategory::FieldAgent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 RecruitmentCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 MonthlySalary = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 RecruitmentHours = 72;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "200"))
	int32 BaseHealth = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 BaseAccuracy = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 BaseResolve = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 BaseMobility = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 BaseStrength = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RequiredResearch;
};

/** One repeatable, player-selected field doctrine gained through rank progression. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelDoctrineRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString Summary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxSelections = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "50"))
	int32 MaxHealthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "25"))
	int32 AccuracyBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "25"))
	int32 ResolveBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "25"))
	int32 MobilityBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "25"))
	int32 StrengthBonus = 0;
};

/** One deterministic service citation awarded when its cumulative thresholds are met. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelCommendationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString Summary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 RequiredMissions = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 RequiredKills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 RequiredRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	bool bRequiresSuccessfulMission = true;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCraftRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 PurchaseCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 MonthlyMaintenance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 AcquisitionHours = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 MaxHull = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 FuelCapacity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 CruiseSpeedKilometersPerHour = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 FuelBurnPerHour = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 AgentCapacity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 CargoCapacity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "16"))
	int32 EquipmentSlots = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 RepairCostPerHull = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 RepairHoursPerHull = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 RefuelCostPerUnit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 RefuelUnitsPerHour = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	TArray<FName> RequiredResearch;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicRegionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	/** Authored globe label/mandate center, independent of current hostile routes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "-180000", ClampMax = "180000"))
	int32 CenterLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "-90000", ClampMax = "90000"))
	int32 CenterLatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 InitialSupport = 50;

	/** Relative share of the selected campaign mandate's recurring funding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 FundingWeight = 1;

	/** Pressure above this value erodes support during the monthly mandate review. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "99"))
	int32 PressureTolerance = 50;

	/** Monthly support restored while pressure is at or below half the tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "20"))
	int32 LowPressureSupportRecovery = 2;

	/** Support lost per started ten pressure above tolerance at monthly review. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 HighPressureSupportLossPerTen = 1;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FContactRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Signature = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 CruiseSpeedKilometersPerHour = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 MaxHull = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "10"))
	int32 ThreatRating = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 ScoreValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 AttackAccuracy = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 AttackDamage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 AttackIntervalSeconds = 5;
};

/** Named deterministic strategy graph whose opening mission may enter weighted scheduling. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FAdversaryPlanRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName OpeningMissionRuleId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAdversaryMissionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	/** Optional named strategy graph. Only its authored opening mission enters weighted scheduling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName PlanId;

	/** One-based stage within a plan; zero for standalone missions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "16"))
	int32 PlanStage = 0;

	/** Immediate successor launched without a weighted-selection draw when this mission escapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName EscapeBranchMissionRuleId;

	/** Immediate successor launched without a weighted-selection draw when this mission is thwarted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName ThwartBranchMissionRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName ContactRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName TargetRegionId;

	/** Dynamically routes this mission to a deterministic player base instead of the authored destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	bool bTargetsPlayerBase = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	int32 OriginLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	int32 OriginLatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	int32 DestinationLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	int32 DestinationLatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 IntervalHours = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MinimumEscalation = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1"))
	int32 SelectionWeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 PressureOnEscape = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 PressureReductionOnDestroyed = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 ScorePenaltyOnEscape = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 FundingPenaltyOnEscape = 0;

	/** Regional mandate support lost when this operation succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 SupportLossOnEscape = 0;

	/** Regional mandate support restored when this operation is thwarted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 SupportGainOnThwarted = 0;

	/** Support lost by every other active Horizon Compact member when this operation escapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CompactPeerSupportLossOnEscape = 0;

	/** Support restored to every withdrawn Horizon Compact member when this operation is thwarted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 WithdrawnCompactSupportGainOnThwarted = 0;

	/** A detected contact that reaches its destination creates an intact tactical site after escape consequences resolve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	bool bCreatesLandingSiteOnArrival = false;

	/** Strategic decision window for an authored intact landing site. Zero when no site is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 LandingSiteLifetimeHours = 0;

	/** Additional intact-site threat above the source contact. Zero when no site is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "9"))
	int32 LandingSiteThreatBonus = 0;

	/** Integrity damage applied to each selected facility after base defenses are breached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 BaseFacilityDamage = 0;

	/** Maximum number of distinct installed facilities struck after a breach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 BaseFacilitiesHit = 0;
};

/** One material used by deterministic tactical battlefields. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalTerrainRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MoveCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CoverPercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0"))
	int32 MaxIntegrity = 0;

	/** Multiplicative blast energy removed when an intact cell lies between impact and target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlastResistancePercent = 0;

	/** Controls deterministic ignition from orthogonally adjacent burning cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 Flammability = 0;

	/** Percentage of remaining smoke exhausted per environment step and exposure to directional wind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 VentilationPercent = 0;

	/** AP spent to move one level through this cell; zero means it is not a vertical connector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "20"))
	int32 VerticalMoveCost = 0;

	/** Abstract height checked against deterministic device throw trajectories while this terrain is intact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "8"))
	int32 ThrowObstacleHeight = 0;

	/** AP spent by an adjacent unit to open or close this terrain; zero means it is not a door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DoorActionPointCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	bool bBlocksMovement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	bool bBlocksVision = false;

	bool IsDoor() const { return DoorActionPointCost > 0; }
	bool IsVerticalConnector() const { return VerticalMoveCost > 0; }
};

/** Original non-player tactical combatant archetype. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalUnitRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "200"))
	int32 MaxHealth = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Accuracy = 45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Resolve = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Mobility = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Strength = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ActionPoints = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "64"))
	int32 AttackRange = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "200"))
	int32 AttackPower = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 AttackActionPointCost = 4;

	/** Optional intrinsic resolve-pressure projection. All three signal values must be zero or positive together. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 SignalPower = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "64"))
	int32 SignalRange = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "20"))
	int32 SignalActionPointCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	ETacticalDamageType AttackDamageType = ETacticalDamageType::Kinetic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 KineticArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 ThermalArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 ArcArmor = 0;

	bool HasSignalProjection() const
	{
		return SignalPower > 0 && SignalRange > 0 && SignalActionPointCost > 0;
	}
};

/** Strategic situation in which a procedural tactical mission recipe can be selected. */
UENUM(BlueprintType)
enum class ETacticalMissionContext : uint8
{
	StrategicSite,
	BaseDefense
};

/** Strategic-site condition that selects a distinct procedural tactical recipe. */
UENUM(BlueprintType)
enum class ETacticalSiteType : uint8
{
	Wreckage,
	Landing
};

/** Procedural battlefield recipe selected from a strategic contact and operation context. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalMissionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FRuleIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FString DisplayName;

	/** Defaults to the legacy crash-site flow when omitted from content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	ETacticalMissionContext Context = ETacticalMissionContext::StrategicSite;

	/** Used only for StrategicSite missions; omitted legacy recipes remain wreckage missions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	ETacticalSiteType SiteType = ETacticalSiteType::Wreckage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName SourceContactRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName FloorTerrainRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName ObstacleTerrainRuleId;

	/** Optional openable terrain used as the pass-through point in a generated barrier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName DoorTerrainRuleId;

	/** Required connector terrain for missions with more than one tactical level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName VerticalConnectorTerrainRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName AdversaryUnitRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	ETacticalObjectiveType ObjectiveType = ETacticalObjectiveType::Disrupt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ObjectiveRequiredInteractions = 1;

	/** Item secured into the transport cargo when a recovery objective completes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules")
	FName ObjectiveRewardItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "100"))
	int32 ObjectiveRewardQuantity = 0;

	/** Experience awarded to every deployed agent who reaches debrief. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 MissionExperienceReward = 25;

	/** Additional experience awarded when the primary objective is completed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "10000"))
	int32 ObjectiveExperienceReward = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "8", ClampMax = "64"))
	int32 MapWidth = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "12", ClampMax = "96"))
	int32 MapHeight = 28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MapLevels = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "2", ClampMax = "8"))
	int32 DeploymentDepth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "60"))
	int32 ObstaclePercent = 22;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "32"))
	int32 BaseEnemyCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "0", ClampMax = "8"))
	int32 EnemiesPerThreat = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "500"))
	int32 TurnLimit = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ObjectiveActionPointCost = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Rules", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ExtractionActionPointCost = 1;
};

/** Parsed package manifest plus its typed original rule contributions. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FContentPackage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	FContentPackageDescriptor Descriptor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FItemRule> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FResearchRule> Research;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FKnowledgeArchiveEntryRule> ArchiveEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FFacilityRule> Facilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FPersonnelRoleRule> PersonnelRoles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FPersonnelDoctrineRule> PersonnelDoctrines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FPersonnelCommendationRule> PersonnelCommendations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FCraftRule> Craft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FStrategicRegionRule> Regions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FContactRule> Contacts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FAdversaryPlanRule> AdversaryPlans;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FAdversaryMissionRule> AdversaryMissions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FTacticalTerrainRule> TacticalTerrains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FTacticalUnitRule> TacticalUnits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FTacticalMissionRule> TacticalMissions;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FResolvedRuleSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FItemRule> Items;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FResearchRule> Research;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FKnowledgeArchiveEntryRule> ArchiveEntries;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FFacilityRule> Facilities;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FPersonnelRoleRule> PersonnelRoles;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FPersonnelDoctrineRule> PersonnelDoctrines;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FPersonnelCommendationRule> PersonnelCommendations;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FCraftRule> Craft;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FStrategicRegionRule> Regions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FContactRule> Contacts;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FAdversaryPlanRule> AdversaryPlans;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FAdversaryMissionRule> AdversaryMissions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FTacticalTerrainRule> TacticalTerrains;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FTacticalUnitRule> TacticalUnits;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FTacticalMissionRule> TacticalMissions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> ItemOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> ResearchOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> ArchiveEntryOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> FacilityOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> PersonnelRoleOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> PersonnelDoctrineOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> PersonnelCommendationOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> CraftOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> RegionOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> ContactOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> AdversaryPlanOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> AdversaryMissionOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> TacticalTerrainOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> TacticalUnitOrigins;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TMap<FName, FName> TacticalMissionOrigins;

	bool ContainsAnyRule(FName RuleId) const;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRuleSetBuildResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	FResolvedRuleSet RuleSet;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TArray<FName> PackageLoadOrder;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Rules")
	TArray<FContentDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

class UEGTCORE_API FRuleSetBuilder final
{
public:
	static FRuleSetBuildResult Build(const TArray<FContentPackage>& Packages);
};
