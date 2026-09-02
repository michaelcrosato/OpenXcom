#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Determinism/DeterministicRandomStream.h"
#include "Strategic/StrategicClock.h"
#include "Tactical/TacticalObjectiveTypes.h"

#include "StrategicCampaignState.generated.h"

UENUM(BlueprintType)
enum class ECampaignDifficulty : uint8
{
	Cadet,
	Standard,
	Veteran,
	Apex
};

/** Persistent future-work emphasis selected for one base's Works Cadre. */
UENUM(BlueprintType)
enum class EWorksCadreCharter : uint8
{
	/** Even ten-percent construction and repair front-load per assigned engineer. */
	CommonCadence UMETA(DisplayName = "Common Cadence"),
	/** Faster construction in exchange for reduced repair mobilization. */
	AssemblyCadence UMETA(DisplayName = "Assembly Cadence"),
	/** Faster repairs in exchange for reduced construction mobilization. */
	RestorationCadence UMETA(DisplayName = "Restoration Cadence")
};

/** One player organization site on the strategic map. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseFacilityState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	FName FacilityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 GridX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 GridY = 0;

	/** Accumulated structural damage; zero preserves legacy facilities at full integrity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 Damage = 0;

	/** Damage covered by the currently paid repair order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 ReservedRepairDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int64 RemainingRepairSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FInventoryStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Inventory")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Inventory")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	FName RegionId;

	/** Signed thousandths of a degree in [-180000, 180000]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 LongitudeMilliDegrees = 0;

	/** Signed thousandths of a degree in [-90000, 90000]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 LatitudeMilliDegrees = 0;

	/** Saved base-local allowance; integrity-scaled facility bonuses are derived from active rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 ScientistCapacity = 10;

	/** Saved base-local allowance; integrity-scaled facility bonuses are derived from active rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 EngineerCapacity = 10;

	/** Scientists committed to the original Signal Watch relay-throughput policy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 SignalWatchScientists = 0;

	/** Engineers reserved for the original Works Cadre facility-mobilization policy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	int32 WorksCadreEngineers = 0;

	/** Specializes only facility-work clocks committed after the charter is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	EWorksCadreCharter WorksCadreCharter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	TArray<FName> BuiltFacilities;

	/** Positioned operational facilities used by save format v5 and newer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	TArray<FBaseFacilityState> Facilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Base")
	TArray<FInventoryStack> Inventory;
};

/** Player-selected routing doctrine for one inter-base aid commitment. */
UENUM(BlueprintType)
enum class EMutualAidRoutePolicy : uint8
{
	/** Baseline seventy-two-hour relay with no exposure adjustment. */
	OpenRelay UMETA(DisplayName = "Open Relay"),
	/** Faster routing that accepts a higher transparent interdiction exposure. */
	RapidThread UMETA(DisplayName = "Rapid Thread"),
	/** Slower routing that reduces transparent interdiction exposure. */
	VeiledChain UMETA(DisplayName = "Veiled Chain")
};

/** One lossless, deterministic inventory commitment moving between established bases. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidConvoyState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	FGuid ConvoyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	FGuid SourceBaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	FGuid DestinationBaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int32 Quantity = 0;

	/** Unique relay-order token initialized from dispatch; Relief Priority may rotate existing tokens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 DispatchSequence = 0;

	/** Stable route doctrine committed at dispatch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	EMutualAidRoutePolicy RoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	/** Original route duration before a possible one-time interdiction delay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 TotalTransitSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 RemainingTransitSeconds = 0;

	/** Exact pressure-derived exposure committed at dispatch in [0, 100]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int32 RoutePressure = 0;

	/** A paid Signal Escort prevents the forecast deterministic delay without altering cargo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	bool bSignalEscort = false;

	/** Exact dispatch funding already paid for the optional Signal Escort. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 SignalEscortCost = 0;

	/** False only while a forecast midpoint interdiction remains unresolved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	bool bInterdictionResolved = true;

	/** Exact delay forecast committed at dispatch, independently of later simulation config changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 ForecastInterdictionDelaySeconds = 0;

	/** One-time deterministic delay already added to the remaining route clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 InterdictionDelaySeconds = 0;

	/**
	 * Optional physical origin of the current leg after a completed Relay Waypoint.
	 * An invalid identity means the persisted source base is also the current leg origin.
	 * The source base retains its end-to-end Relay Weave channel for the complete journey.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	FGuid CurrentLegOriginBaseId;

	/** Optional established base reached before the final destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	FGuid RelayWaypointBaseId;

	/** Route doctrine committed for the second leg after RelayWaypointBaseId is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	EMutualAidRoutePolicy OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 OnwardTotalTransitSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int32 OnwardRoutePressure = 0;

	/** False only while the committed second leg still has a forecast checkpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	bool bOnwardInterdictionResolved = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int64 OnwardForecastInterdictionDelaySeconds = 0;

	/**
	 * Cargo reserved for delivery at the pending Relay Waypoint.
	 * Zero keeps every unit bound for the final destination. A positive value is
	 * delivered on waypoint arrival and must always leave at least one onward unit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Logistics")
	int32 BalancedHandoffQuantity = 0;
};

/** Deterministic scientist-seconds accumulated toward one research rule. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FResearchProjectState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Research")
	FName ResearchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Research")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Research")
	int32 AssignedScientists = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Research")
	int64 AccumulatedWorkSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FManufacturingProjectState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Manufacturing")
	FGuid ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Manufacturing")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Manufacturing")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Manufacturing")
	int32 AssignedEngineers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Manufacturing")
	int32 UnitsRemaining = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Manufacturing")
	int64 AccumulatedWorkSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FFacilityConstructionProjectState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	FGuid ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	FGuid FacilityInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	FName FacilityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	int32 GridX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	int32 GridY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Construction")
	int64 RemainingBuildSeconds = 0;
};

UENUM(BlueprintType)
enum class EPersonnelStatus : uint8
{
	Available,
	Recovering,
	Training,
	Deployed,
	/** Temporarily committed to a fixed-term base leadership rotation. */
	Stewarding
};

UENUM(BlueprintType)
enum class EPersonnelTrainingFocus : uint8
{
	Accuracy,
	Resolve,
	Mobility,
	Strength
};

/** Player-selected base benefit supplied by one experienced field agent during a fixed-term rotation. */
UENUM(BlueprintType)
enum class EPersonnelStewardshipFocus : uint8
{
	None,
	/** Reduces the funding cost of Surge Care started at the steward's base. */
	RecoveryAdvocacy,
	/** Reduces the duration of personnel training started at the steward's base. */
	TrainingCadre,
	/** Reduces personnel recruitment transit started for the steward's base. */
	RecruitmentLiaison
};

/** Player-directed treatment selected for one active recovery episode. */
UENUM(BlueprintType)
enum class EPersonnelRecoveryPlan : uint8
{
	/** No active recovery episode. Also accepted as the legacy baseline for in-memory fixtures. */
	None,
	/** A new injury is waiting for an explicit player choice. */
	DecisionRequired,
	/** Baseline recovery duration with no funding cost or lasting attribute change. */
	MeasuredReturn,
	/** Funded treatment that halves the remaining recovery duration. */
	SurgeCare,
	/** A longer recovery that grants one Resolve when completed. */
	ReflectionCycle
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FName RoleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	EPersonnelStatus Status = EPersonnelStatus::Available;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Rank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Missions = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Kills = 0;

	/** Lifetime debrief experience used for deterministic rank progression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Experience = 0;

	/** Unspent, player-directed doctrine choices earned one-for-one with rank increases. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 PendingDoctrineChoices = 0;

	/** Stable doctrine ids; repeated ids represent successive selections of the same doctrine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	TArray<FName> DoctrineSelections;

	/** Unique, stable service-citation ids awarded by deterministic debrief thresholds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	TArray<FName> Commendations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 MaxHealth = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 CurrentHealth = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Accuracy = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Resolve = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Mobility = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Strength = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int64 RemainingRecoverySeconds = 0;

	/** Mandatory Return Path choice and its completion effect for the current injury only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	EPersonnelRecoveryPlan RecoveryPlan = EPersonnelRecoveryPlan::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int64 RemainingTrainingSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	EPersonnelTrainingFocus TrainingFocus = EPersonnelTrainingFocus::Accuracy;

	/** Selected benefit while Status is Stewarding; None for every other status. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	EPersonnelStewardshipFocus StewardshipFocus = EPersonnelStewardshipFocus::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int64 RemainingStewardshipSeconds = 0;

	/** Completed fixed-term rotations retained as bounded late-career service history. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 StewardshipToursCompleted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	TArray<FName> EquippedItems;
};

/** Canonical shared-victory history for two field agents. The lower lexical id is always first. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelSquadBondState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid FirstPersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid SecondPersonnelId;

	/** Successful tactical operations completed by both agents while both survived. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 SharedVictories = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRecruitmentOrderState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid OrderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FName RoleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int64 RemainingTransitSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FMemorialRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FName RoleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Rank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Missions = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 Kills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	TArray<FName> DoctrineSelections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	TArray<FName> Commendations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	int32 StewardshipToursCompleted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FDateTime DeathUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Personnel")
	FName CauseId;
};

UENUM(BlueprintType)
enum class ECraftStatus : uint8
{
	Grounded,
	Servicing,
	Airborne,
	Intercepting,
	Returning,
	Deploying,
	OnSite
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCraftWeaponState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FName WeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int32 Ammunition = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int64 RemainingCooldownSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCraftState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FName CraftRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid BaseId;

	/** Invalid means no pilot is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid AssignedPilotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	ECraftStatus Status = ECraftStatus::Grounded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int32 CurrentHull = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int32 CurrentFuel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int64 RemainingRepairSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int64 RemainingRefuelSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int32 CompletedSorties = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	TArray<FName> EquipmentItems;

	/** One aggregate magazine/cooldown record per equipped craft-weapon rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	TArray<FCraftWeaponState> WeaponStates;

	/** Field agents reserved for this craft; grounded agents remain available until launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	TArray<FGuid> AssignedAgentIds;

	/** Inventory-backed cargo stacks carried separately from mounted craft equipment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	TArray<FInventoryStack> Cargo;

	/** Recovered cargo awaiting an explicit retain-or-sell decision after the craft lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	TArray<FInventoryStack> PendingSalvage;

	/** Active detected contact target; invalid for ordinary sorties and grounded craft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid TargetContactId;

	/** Strategic site destination while deploying or awaiting a tactical operation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid TargetSiteId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int64 RemainingRouteSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int64 ReservedReturnSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCraftAcquisitionOrderState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid OrderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FName CraftRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Craft")
	int64 RemainingTransitSeconds = 0;
};

UENUM(BlueprintType)
enum class EStrategicContactStatus : uint8
{
	Hidden,
	Detected,
	Engaged
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicContactState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	FGuid ContactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	FName ContactRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	EStrategicContactStatus Status = EStrategicContactStatus::Hidden;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 OriginLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 OriginLatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 DestinationLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 DestinationLatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int64 TotalRouteSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int64 ElapsedRouteSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 CurrentHull = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int32 CompletedCombatRounds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Contacts")
	int64 RemainingAttackCooldownSeconds = 0;
};

UENUM(BlueprintType)
enum class EStrategicSiteType : uint8
{
	Wreckage,
	Landing
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicSiteState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	FGuid SiteId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	EStrategicSiteType Type = EStrategicSiteType::Wreckage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	FName SourceContactRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	int32 ThreatRating = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Sites")
	int64 RemainingLifetimeSeconds = 0;
};

/** Strategic source that handed personnel and equipment to a tactical battle. */
UENUM(BlueprintType)
enum class ETacticalOperationType : uint8
{
	SiteRecovery,
	BaseDefense
};

/** Immutable handoff from strategic deployment to the tactical simulation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalOperationState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid OperationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalOperationType Type = ETacticalOperationType::SiteRecovery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid SiteId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid CraftId;

	/** Defended base for a base-defense operation; invalid for a site recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid BaseId;

	/** Pending perimeter assault resolved by a base-defense operation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid AssaultId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int64 TacticalSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FDateTime CreatedUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FGuid> AgentIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FInventoryStack> Cargo;
};

UENUM(BlueprintType)
enum class ETacticalTeam : uint8
{
	Player,
	Adversary
};

UENUM(BlueprintType)
enum class ETacticalStance : uint8
{
	Standing,
	Crouched
};

/** Cardinal airflow for a battlefield. Direction describes where the wind carries hazards. */
UENUM(BlueprintType)
enum class ETacticalWindDirection : uint8
{
	Calm,
	North,
	East,
	South,
	West
};

UENUM(BlueprintType)
enum class ETacticalBattlePhase : uint8
{
	Deployment,
	PlayerTurn,
	AdversaryTurn,
	Resolved
};

UENUM(BlueprintType)
enum class ETacticalObjectiveStatus : uint8
{
	Active,
	Completed,
	Failed
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalCellState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Z = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName TerrainRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 CurrentIntegrity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	bool bPlayerDeployment = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	bool bExtraction = false;

	/** Persisted open state for intact terrain whose rule exposes a door action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	bool bDoorOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Smoke = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Fire = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalWeaponState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName WeaponItemId;

	/** Zero for unlimited-feed weapons or an empty magazine, disambiguated by the item rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 LoadedAmmunition = 0;
};

/** A nonempty magazine removed from a tactical weapon, retaining its exact remaining rounds. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalMagazineState
{
	GENERATED_BODY()

	/** Weapon profile that owns this magazine; partial magazines cannot silently change capacity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName WeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName AmmunitionItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 LoadedAmmunition = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalUnitState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid UnitId;

	/** Valid only for player field agents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName SourceRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalTeam Team = ETacticalTeam::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalStance Stance = ETacticalStance::Standing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Z = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 MaxHealth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 CurrentHealth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Accuracy = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Resolve = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Mobility = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Strength = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 MaxActionPoints = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 RemainingActionPoints = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	bool bExtracted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 KineticArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 ThermalArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 ArcArmor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 MaxMorale = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 CurrentMorale = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Suppression = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FTacticalWeaponState> WeaponStates;

	/** Reserve magazines and non-weapon carried items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FInventoryStack> CarriedItems;

	/** Individually tracked nonempty magazines created by ejection or retained during reload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FTacticalMagazineState> EjectedMagazines;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Z = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalObjectiveStatus Status = ETacticalObjectiveStatus::Active;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalObjectiveType Type = ETacticalObjectiveType::Disrupt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 RequiredInteractions = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 CompletedInteractions = 0;

	/** Opposed progress used only by control objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 AdversaryInteractions = 0;
};

/** Fully materialized deterministic battlefield; presentation reads this state but never owns it. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalBattleState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid OperationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FGuid SiteId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FName MissionRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FDateTime CreatedUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Height = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 Levels = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 TurnLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	int32 TurnNumber = 1;

	/** Site recoveries require evacuation; defenders instead hold their own command relay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	bool bRequiresExtraction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalBattlePhase Phase = ETacticalBattlePhase::Deployment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalTeam ActiveTeam = ETacticalTeam::Player;

	/** Persisted weather generated from the operation seed without consuming tactical random draws. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	ETacticalWindDirection WindDirection = ETacticalWindDirection::Calm;

	/** Bounded airflow strength from zero (calm) through three (strong). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical", meta = (ClampMin = "0", ClampMax = "3"))
	int32 WindStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	FDeterministicRandomStream TacticalRandom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FTacticalCellState> Cells;

	/** Sorted cell indices observed by the player at least once. Historical views expose only coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<int32> PlayerDiscoveredCellIndices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FTacticalUnitState> Units;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FTacticalObjectiveState> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Tactical")
	TArray<FInventoryStack> Cargo;

	bool IsWithinGrid(const int32 X, const int32 Y, const int32 Z) const
	{
		return X >= 0 && X < Width && Y >= 0 && Y < Height && Z >= 0 && Z < Levels;
	}

	int32 GetCellIndex(const int32 X, const int32 Y, const int32 Z) const
	{
		return (Z * Height + Y) * Width + X;
	}
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRegionalPressureState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FName RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	int32 Pressure = 0;
};

/** Persisted relationship and recurring-funding contract for one regional mandate partner. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRegionalMandateState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	FName RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	int32 Support = 50;

	/** Persistent contribution before the current support-tier multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	int64 BaselineMonthlyFunding = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	int64 CurrentMonthlyFunding = 0;

	/** Gregorian year * 12 + zero-based month; zero means no outreach has occurred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	int32 LastDiplomaticActionMonth = 0;

	/** Durable mutual-aid compact that trades recurring contribution for lower hostile exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	bool bResilienceCharterSigned = false;

	/** A ratified member that withdrew under low support until the player restores compact confidence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Diplomacy")
	bool bHorizonCompactMemberWithdrawn = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAdversaryMissionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid MissionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid ContactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FName MissionRuleId;

	/** Dynamically selected target for base-assault mission rules; invalid for regional missions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid TargetBaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FDateTime StartedUtc;
};

/** A hostile contact that reached its selected base and now requires a defense decision. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseAssaultState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid AssaultId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid MissionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid ContactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign|Adversary")
	FDateTime ArrivedUtc;
};

UENUM(BlueprintType)
enum class ECampaignOutcome : uint8
{
	Ongoing,
	Victory,
	Failure
};

/** Persisted campaign root shared by strategic simulation and save codecs. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	FStrategicTimestamp StrategicTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	FDeterministicRandomStream SimulationRandom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int64 Funds = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int64 CampaignScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	ECampaignDifficulty Difficulty = ECampaignDifficulty::Standard;

	/** Monotonic id for deterministic command/event ordering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int64 CommandSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FName> CompletedResearch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int64 MonthlyFunding = 500000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FStrategicBaseState> Bases;

	/** Inbound storage remains reserved until each lossless Mutual Aid Convoy arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FMutualAidConvoyState> MutualAidConvoys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FResearchProjectState> ResearchProjects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FManufacturingProjectState> ManufacturingProjects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FFacilityConstructionProjectState> FacilityConstructionProjects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FPersonnelState> Personnel;

	/** Durable pair history used to derive non-stacking Field Cadence deployment bonuses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FPersonnelSquadBondState> PersonnelSquadBonds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FRecruitmentOrderState> RecruitmentOrders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FMemorialRecord> Memorial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FCraftState> Craft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FCraftAcquisitionOrderState> CraftAcquisitionOrders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FStrategicContactState> StrategicContacts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FStrategicSiteState> StrategicSites;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FTacticalOperationState> TacticalOperations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FTacticalBattleState> TacticalBattles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int32 AdversaryEscalationLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int64 NextAdversaryMissionSeconds = 86400;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int64 NextAdversaryMissionSerial = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int32 AdversaryMissionsLaunched = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int32 AdversaryMissionsEscaped = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int32 AdversaryMissionsThwarted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FRegionalPressureState> RegionalPressure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FRegionalMandateState> RegionalMandates;

	/** Durable multi-partner network ratified after regional Resilience Charters establish its membership. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	bool bHorizonCompactRatified = false;

	/** Gregorian year * 12 + zero-based month; zero means Reciprocal Aid has never been deployed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int32 LastCoalitionAidMonth = 0;

	/** Gregorian year * 12 + zero-based month; zero means no emergency solidarity vote has passed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	int32 LastCoalitionEmergencyVoteMonth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FAdversaryMissionState> AdversaryMissions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	TArray<FBaseAssaultState> BaseAssaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	ECampaignOutcome Outcome = ECampaignOutcome::Ongoing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign")
	FName OutcomeReasonId;
};
