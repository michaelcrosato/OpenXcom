#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/CraftServiceQueue.h"
#include "Strategic/MutualAidRelayQueue.h"
#include "Strategic/PersonnelLegacyRelay.h"
#include "Strategic/PersonnelMentorship.h"
#include "Strategic/PersonnelRecoveryPlan.h"
#include "Strategic/PersonnelSquadBond.h"
#include "Strategic/PersonnelStewardship.h"
#include "Strategic/StrategicCampaignState.h"
#include "Strategic/StrategicCommandService.h"
#include "Strategic/PersonnelServiceHistory.h"

#include "StrategicPresentationService.generated.h"

UENUM(BlueprintType)
enum class EStrategicGlobeMarkerType : uint8
{
	Base,
	Craft,
	Contact,
	Site
};

UENUM(BlueprintType)
enum class EStrategicProjectType : uint8
{
	Research,
	Manufacturing,
	Construction,
	Recruitment,
	CraftAcquisition
};

UENUM(BlueprintType)
enum class EStrategicActionOptionType : uint8
{
	Research,
	Facility,
	Personnel,
	Craft,
	Manufacturing
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMutualAidRouteOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EMutualAidRoutePolicy Policy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaselinePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ExposureModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 InterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 SignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSignalEscortAffordable = false;

	/** Unescorted Relay Weave projection for this route choice. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FMutualAidRelayQueueView RelayQueue;

	/** Exact queued arrival if Signal Escort removes a forecast delay. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 EscortedEstimatedArrivalSeconds = 0;

	/** True when this route can be committed by the containing command surface. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMutualAidDispatchOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid DestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DestinationBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TransitSeconds = 0;

	/** Three fixed route doctrines in policy order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidRouteOptionView> Routes;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMutualAidDiversionOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid DestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DestinationBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 DivertedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ProjectedRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RetainedSignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	/** Signed target-arrival change; negative values arrive sooner. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AffectedConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMutualAidWaypointOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bDirectRoute = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid WaypointBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString WaypointBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EMutualAidRoutePolicy OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName OnwardRoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FirstLegRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bFirstLegInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OnwardRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bOnwardInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 JourneySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 WaypointArrivalSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	/** Signed target-arrival change; positive values arrive later. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AffectedConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMutualAidBalancedHandoffOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabledChoice = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WaypointQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FinalQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 HandoffStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 WaypointReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 DestinationReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicInventoryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 UnitSellValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 UnitStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bPersonnelEquippable = false;

	/** Stable destination choices and exact safe quantities for this unassigned stack. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidDispatchOptionView> MutualAidOptions;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMutualAidConvoyView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid SourceBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString SourceBaseName;

	/** Physical origin of the currently active leg; source base before any waypoint. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid CurrentLegOriginBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString CurrentLegOriginBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid DestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DestinationBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString ItemDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 DispatchSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EMutualAidRoutePolicy RoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 SignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bInterdictionResolved = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ForecastInterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 InterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid RelayWaypointBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RelayWaypointBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EMutualAidRoutePolicy OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName OnwardRoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 OnwardTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OnwardRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bOnwardInterdictionResolved = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 OnwardForecastInterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BalancedHandoffQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FinalDeliveryQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 BalancedHandoffStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FMutualAidRelayQueueView RelayQueue;

	/** Fixed route-policy order with exact post-retune projections for a held convoy. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidRouteOptionView> RetuneRoutes;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanRetune = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RetuneUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RetuneUnavailableReason;

	/** True when this held convoy can still commission its deterministic Signal Escort. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanCommissionSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 SignalEscortCommissionCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 SignalEscortPreventedDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FMutualAidRelayQueueView SignalEscortProjectedRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName SignalEscortCommissionUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString SignalEscortCommissionUnavailableReason;

	/** True when Relief Priority can move this convoy to the front of the held relay line. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanPrioritizeRelief = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ReliefPriorityPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ReliefPriorityBypassedConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ReliefPriorityRecoveredWaitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FMutualAidRelayQueueView ReliefPriorityProjectedRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ReliefPriorityUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString ReliefPriorityUnavailableReason;

	/** True when this never-departed held convoy can be withdrawn and returned to its source. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanStandDownRelief = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ReliefStandDownPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ReliefStandDownReleasedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ReliefStandDownSunkSignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ReliefStandDownAdvancedConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ReliefStandDownRecoveredWaitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ReliefStandDownUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString ReliefStandDownUnavailableReason;

	/** Stable established-base choices for redirecting a held convoy before departure. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidDiversionOptionView> ReliefDiversionOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanDivertRelief = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ReliefDiversionUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString ReliefDiversionUnavailableReason;

	/** Direct restoration plus stable base/policy choices for a two-leg route. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidWaypointOptionView> RelayWaypointOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanConfigureRelayWaypoint = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RelayWaypointUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RelayWaypointUnavailableReason;

	/** Stable Through Cargo and Balanced Handoff choices for a pending waypoint. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidBalancedHandoffOptionView> BalancedHandoffOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanConfigureBalancedHandoff = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName BalancedHandoffUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString BalancedHandoffUnavailableReason;
};

UENUM(BlueprintType)
enum class ERegionalSupportTier : uint8
{
	Suspended,
	Strained,
	Committed,
	Allied
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicRegionalActionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	ERegionalDiplomacyActionType ActionType = ERegionalDiplomacyActionType::CivicRelief;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SupportDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PressureReduction = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MinimumPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bWouldWithdrawCompactMember = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicRegionalCharterView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSigned = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MinimumSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FundingPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MissionWeightPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 EscapePressurePercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCompactRestorationView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bWithdrawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MinimumSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCompactEmergencyVoteView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bTargetWithdrawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetCurrentSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetProjectedSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetSupportGain = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetCurrentPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetProjectedPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetPressureReduction = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 VoterSupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumVoterPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RequiredVotes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> SupportingMemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> OpposingMemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCoalitionAidView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName TargetRegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName DonorRegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MinimumTargetPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumPressureTransfer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PressureTransfer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetCurrentPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetProjectedPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DonorCurrentPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DonorProjectedPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 TargetSupportGain = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DonorSupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bDonorWouldWithdraw = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicHorizonCompactView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bRatified = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RequiredCharters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SignedCharters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MinimumMemberSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MemberSupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FundingPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SharedEscapePressurePercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WithdrawalSupportThreshold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RestorationMinimumSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> MemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> ActiveMemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> WithdrawnMemberRegionIds;

	/** Canonically sorted player-directed aid options, one for each current member. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicCoalitionAidView> AidOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicRegionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Pressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bHasMandate = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Support = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	ERegionalSupportTier SupportTier = ERegionalSupportTier::Suspended;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 BaselineMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicRegionalActionView> ActionOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicRegionalCharterView ResilienceCharter;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicCompactRestorationView HorizonCompactRestoration;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicCompactEmergencyVoteView HorizonCompactEmergencyVote;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicFacilityView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid FacilityInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid ProjectId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName FacilityId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 GridX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 GridY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 GridWidth = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 GridHeight = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bOperational = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bConstructing = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingBuildSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentIntegrity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaxIntegrity = 0;

	/** Rounded remaining output percentage used by all integrity-scaled facility contributions. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 EffectivenessPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 StorageCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumStorageCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 EngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumEngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CraftCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumCraftCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SensorRangeKilometers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumSensorRangeKilometers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DetectionStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumDetectionStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaseDefenseAccuracy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumBaseDefenseAccuracy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaseDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumBaseDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName BaseDefenseSupplyItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString BaseDefenseSupplyDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaseDefenseSupplyPerShot = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Damage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bRepairing = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingRepairSeconds = 0;

	/** Full reserved-cost refund available while an all-or-nothing repair is active. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RepairCancellationRefund = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanRepair = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RepairCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RepairDurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RepairBaselineDurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RepairWorksCadreFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RepairUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RepairUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanDismantle = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 DismantleRefund = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName DismantleUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DismantleUnavailableReason;
};

/** One command-ready future-work specialization exposed on a base card. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicWorksCadreCharterOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EWorksCadreCharter Charter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ConstructionFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RepairFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ConstructionFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RepairFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

/** Read-only service profile derived from the current integrity-scaled base outputs. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseSpecializationView
{
	GENERATED_BODY()

	/** Stable presentation policy selected from the strongest operational capability. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName SpecializationId = TEXT("base.specialization.integrated-command");

	/** Existing capability value normalized to a 0-100 specialization index. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Score = 0;

	/** Second-highest normalized capability, exposed to keep close calls explainable. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SecondaryScore = 0;

	/** Existing output represented by the selected specialization, not a new bonus. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName BenefitMetricId = TEXT("base.specialization.balanced-capabilities");

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 BenefitValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSpecialized = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RegionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaseScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FacilityScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AssignedScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName SignalWatchPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SignalWatchScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SignalWatchMaximumScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FacilityRelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SignalWatchBonusChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RelayChannelCount = 0;

	/** Current Relay Weave load across all commitments sourced by this base. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RelayQueueActiveConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RelayQueueTotalConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RelayQueueWaitingConvoyCount = 0;

	/** Share of this source line held by queue congestion, clamped to [0, 100]. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RelayQueuePressurePercent = 0;

	/** Exact arrival horizon of the last currently committed convoy. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RelayQueueTailArrivalSeconds = 0;

	/** Derived read-only service focus; it changes only when current facility output changes. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicBaseSpecializationView Specialization;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanIncreaseSignalWatch = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName SignalWatchIncreaseUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString SignalWatchIncreaseUnavailableReason;

	/** Active roster plus inbound recruitment orders. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ScientistPersonnel = 0;

	/** Maximum of roster and assignments above current effective capacity. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ScientistOverCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 EngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaseEngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FacilityEngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AssignedEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName WorksCadrePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WorksCadreEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WorksCadreMaximumEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EWorksCadreCharter WorksCadreCharter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName WorksCadreCharterPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WorksCadreConstructionFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WorksCadreRepairFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicWorksCadreCharterOptionView> WorksCadreCharterOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanIncreaseWorksCadre = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName WorksCadreIncreaseUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString WorksCadreIncreaseUnavailableReason;

	/** Active roster plus inbound recruitment orders. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 EngineerPersonnel = 0;

	/** Maximum of roster and assignments above current effective capacity. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 EngineerOverCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CraftCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CraftOccupied = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SensorRangeKilometers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DetectionStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumDefenseDamage = 0;

	/** Rounded uncapped expected damage from one complete operational battery volley. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ExpectedDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bStorageEnforced = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageReserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageProductionReserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageMutualAidReserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageCommitted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageAvailable = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageOverflow = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 GridWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 GridHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> Facilities;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicFacilityView> FacilityLayout;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicInventoryView> Inventory;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicPersonnelDoctrineView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName DoctrineId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentSelections = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumSelections = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaxHealthBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AccuracyBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ResolveBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MobilityBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 StrengthBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicPersonnelCommendationView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName CommendationId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicPersonnelView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid PersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	/** Stable role rule identity retained so presentation adapters can localize the authored role name. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RoleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RoleDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EPersonnelRoleCategory RoleCategory = EPersonnelRoleCategory::FieldAgent;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Status;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EPersonnelStatus StatusType = EPersonnelStatus::Available;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Rank = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Missions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelServiceHistoryView ServiceHistory;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentHealth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaxHealth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Accuracy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Resolve = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Mobility = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Strength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingRecoverySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelRecoveryPlanView RecoveryPlan;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingTrainingSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EPersonnelTrainingFocus TrainingFocus = EPersonnelTrainingFocus::Accuracy;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelStewardshipView Stewardship;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 StewardshipToursCompleted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PendingDoctrineChoices = 0;

	/** All authored options while a choice is pending; otherwise, only doctrines already selected. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicPersonnelDoctrineView> DoctrineOptions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicPersonnelCommendationView> Commendations;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bAssignedToCraft = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> EquippedItemIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> EquippedItemNames;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMemorialView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid PersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RoleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RoleDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Rank = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Missions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelServiceHistoryView ServiceHistory;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 StewardshipToursCompleted = 0;

	/** Stable doctrine ids retain repeated selections as earned doctrine levels. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> DoctrineSelections;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicPersonnelCommendationView> Commendations;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FDateTime DeathUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName CauseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString CauseDisplayName;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCraftSalvageView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 UnitStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 UnitSellValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalSellValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanRetainAtBase = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanSell = false;
};

/** Exact per-weapon ammunition readiness and the deterministic share loadable from base stores. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCraftWeaponView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName WeaponItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString WeaponDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName AmmunitionItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString AmmunitionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MountCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 LoadedAmmunition = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MissingAmmunition = 0;

	/** Total matching rounds currently at base before allocating them across weapon types. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 BaseAvailableAmmunition = 0;

	/** Rounds this weapon receives under LoadAvailable after earlier lexical weapon ids. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 LoadableAmmunition = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCraftView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid CraftId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid BaseId;

	/** Stable craft rule identity retained so adapters can localize the type name. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName CraftRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString TypeDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Status;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	ECraftStatus StatusType = ECraftStatus::Grounded;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaxHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentFuel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FuelCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AssignedAgents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AgentCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bHasPilot = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid AssignedPilotId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FGuid> AssignedAgentIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelMentorshipView Mentorship;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelLegacyRelayView LegacyRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FPersonnelSquadBondView SquadBonds;

	/** Derived Flight-Deck Rotation lane, queue position, and exact wait/ready estimate. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FCraftServiceQueueView ServiceQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingRouteSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingRepairSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingRefuelSeconds = 0;

	/** Time until every active service component has completed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingServiceSeconds = 0;

	/** Reservation returned if every still-unapplied service component is cancelled now. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ServiceCancellationRefund = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanCancelService = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicCraftWeaponView> Weapons;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalAmmunitionLoaded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalAmmunitionCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalAmmunitionMissing = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 TotalAmmunitionLoadable = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanRearmFully = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanLoadAvailableAmmunition = false;

	/** Recovered stacks remain aboard until each is explicitly retained or sold after landing. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicCraftSalvageView> PendingSalvage;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSalvageDispositionAvailable = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicInterceptionPostureView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EInterceptionPosture Posture = EInterceptionPosture::BalancedVector;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OutgoingAccuracyModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 IncomingAccuracyModifier = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicInterceptionCoordinationView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OnStationCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SupportingCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OutgoingAccuracyModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 IncomingAccuracyModifier = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicInterceptionContactManeuverView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EInterceptionContactManeuver Maneuver = EInterceptionContactManeuver::VectorSurvey;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CompletedCombatRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OutgoingAccuracyModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 IncomingAccuracyModifier = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicInterceptionWithdrawalView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EInterceptionWithdrawalDoctrine Doctrine = EInterceptionWithdrawalDoctrine::FormationBreak;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OnStationCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 WithdrawingCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RemainingCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CompletedCombatRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RequiredCombatRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ContactRouteDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid PriorityCraftId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString PriorityCraftDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PriorityCraftCurrentHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PriorityCraftMaximumHull = 0;
};

/** Exact member-level support projection for one detected coalition-counterplay outcome. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCoalitionCounterplayMemberView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ProjectedSupport = 0;

	/** Escape-only warning: the projected support crosses the compact withdrawal threshold. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bWouldWithdraw = false;

	/** Thwart-only reminder: support recovery never restores membership automatically. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bRemainsWithdrawn = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicContactView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid ContactId;

	/** Stable contact rule identity retained so adapters can localize the contact class. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ContactRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Status;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EStrategicContactStatus StatusType = EStrategicContactStatus::Detected;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CurrentHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaxHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ThreatRating = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	float RouteProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bTargetsBase = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid TargetBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString TargetBaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bAssaultPending = false;

	/** Fog-safe plan intelligence is projected only after the owning contact is detected. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PlanId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString PlanDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PlanStage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName EscapeBranchMissionRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString EscapeBranchMissionName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ThwartBranchMissionRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString ThwartBranchMissionName;

	/** True when the detected operation carries authored Horizon Compact counterplay. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bHasCoalitionCounterplay = false;

	/** Other currently active members projected to lose support if this operation escapes. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicCoalitionCounterplayMemberView> EscapeStrainMembers;

	/** Currently withdrawn members projected to recover support if this operation is thwarted. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicCoalitionCounterplayMemberView> ThwartRecoveryMembers;

	/** Fog-safe decision intelligence for detected contacts with an authored intact landing outcome. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanShadowToLanding = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LandingSiteThreatRating = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 LandingSiteLifetimeSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 WreckageSiteLifetimeSeconds = 0;

	/** Fixed-order, command-ready engagement geometries exposed only for an active interception. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicInterceptionPostureView> InterceptionPostures;

	/** Automatic formation benefit derived from the exact craft currently on station. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicInterceptionCoordinationView InterceptionCoordination;

	/** Automatic adversary response derived from pursuit rounds and contact integrity. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicInterceptionContactManeuverView InterceptionContactManeuver;

	/** Fixed-order, command-ready withdrawal doctrines exposed only for an active interception. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicInterceptionWithdrawalView> InterceptionWithdrawals;

	/** Number of craft currently on station at this engaged contact. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 InterceptionCraftCount = 0;

	/** True when the entire on-station formation has a valid deterministic return route. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanWithdrawInterception = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseDefenseSupplyView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RequiredQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AvailableQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AllocatedQuantity = 0;
};

/** One command-ready automatic-defense policy with its exact scarce-supply projection. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseDefenseDoctrineView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EBaseDefenseFireDoctrine Doctrine = EBaseDefenseFireDoctrine::CoordinatedLine;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName PolicyId;

	/** Immediate campaign-fund commitment for this volley. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 FundingCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bAffordable = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AccuracyBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DamagePercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanResolve = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ReadyDefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ExpectedDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicBaseDefenseSupplyView> DefenseSupplies;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseAssaultView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid AssaultId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid MissionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName MissionRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid ContactId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ContactRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString BaseName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString MissionName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString ContactName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FDateTime ArrivedUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ThreatRating = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ContactHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ReadyDefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumDefenseDamage = 0;

	/** Rounded uncapped expected damage from one complete operational battery volley. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ExpectedDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicBaseDefenseSupplyView> DefenseSupplies;

	/** Fixed-order, command-ready fire doctrines with doctrine-specific supply and damage previews. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicBaseDefenseDoctrineView> FireDoctrines;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BreachDamagePerFacility = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 MaximumFacilitiesHit = 0;

	/** Available, unassigned field agents that would defend the base. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DefenderCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName TacticalMissionRuleId;

	/** Existing base-defense operation, when the ground team has already been committed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid TacticalOperationId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanResolve = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanDeployTacticalDefense = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bTacticalDefensePrepared = false;

	/** Stable machine-readable reason for automatic-defense unavailability. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;

	/** Stable machine-readable reason for ground-defense unavailability. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName TacticalUnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString TacticalUnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicSiteView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid SiteId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EStrategicSiteType Type = EStrategicSiteType::Wreckage;

	/** Contact class that produced this wreckage or intact landing site. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName SourceContactRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 ThreatRating = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingLifetimeSeconds = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicMaterialRequirementView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 PerUnitQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AvailableQuantity = 0;

	/** Quantity returned if the active production run is cancelled now. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RefundableQuantity = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicProjectView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EStrategicProjectType Type = EStrategicProjectType::Research;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid ProjectId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Detail;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 RemainingSeconds = 0;

	/** Scientists for research, engineers for manufacturing, otherwise zero. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AssignedStaff = 0;

	/** Facility requirements are populated for research projects in authored stable order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> RequiredFacilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> RequiredFacilityNames;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> MissingFacilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> MissingFacilityNames;

	/** True when an active research project cannot advance because a required facility is absent or offline. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bPaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString PauseReason;

	/** Remaining units and per-unit reservation cost for editable manufacturing runs. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 UnitsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 UnitCost = 0;

	/** Refund for cancelling this project now, following its domain-specific progress policy. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 CancellationRefund = 0;

	/** Per-unit inputs, current unreserved stock, and cancellation-return quantities. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMaterialRequirementView> MaterialRequirements;

	/** Net committed storage change for adding one untouched production unit. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageDeltaPerUnit = 0;

	/** Net storage change if the active production run is cancelled now. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 CancellationStorageDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanRemoveManufacturingUnit = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString RemoveManufacturingUnitUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanCancel = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString CancellationUnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicActionOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EStrategicActionOptionType Type = EStrategicActionOptionType::Research;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName RuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Detail;

	/** Research-only facility contracts retained as ids so adapters can localize names safely. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> RequiredFacilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> RequiredFacilityNames;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FName> MissingFacilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> MissingFacilityNames;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DurationHours = 0;

	/** Craft-option values used to reconstruct localized acquisition details. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CraftMaxHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 CraftAgentCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bAffordable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString UnavailableReason;

	/** Per-unit inventory inputs and current unreserved stock for manufacturing options. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMaterialRequirementView> MaterialRequirements;

	/** Net committed storage change after one output reservation and its input consumption. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 StorageDeltaPerUnit = 0;

	/** First valid command placement for a facility option; -1 for other option types or no fit. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SuggestedGridX = -1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SuggestedGridY = -1;

	/** Footprint and all domain-valid anchor cells at the primary base. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FacilityGridWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 FacilityGridHeight = 0;

	/** Facility-only automatic-defense supply contract retained for localized adapters. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName BaseDefenseSupplyItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString BaseDefenseSupplyDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 BaseDefenseSupplyPerShot = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FIntPoint> ValidFacilityPlacements;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicGlobeMarkerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	EStrategicGlobeMarkerType Type = EStrategicGlobeMarkerType::Base;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FString Detail;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bUrgent = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicGlobeRouteView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OriginLongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 OriginLatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DestinationLongitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 DestinationLatitudeMilliDegrees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bPlayerControlled = false;
};

/** Research-authorized archive record; locked content is never copied into the player-facing snapshot. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicArchiveEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	FName EntryId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	FName CategoryId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	FString CategoryDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	FString Summary;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	int32 SortOrder = 0;

	/** Only links to other records unlocked in the same immutable snapshot are exposed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	TArray<FName> RelatedEntryIds;
};

/** Immutable, presentation-ready view of the strategic simulation and its safe action surface. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicDashboardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FString> Diagnostics;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FDateTime CampaignTimeUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	ECampaignDifficulty Difficulty = ECampaignDifficulty::Standard;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	ECampaignOutcome Outcome = ECampaignOutcome::Ongoing;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName OutcomeReasonId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ExpectedCommandSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 Funds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 MonthlyOutgoings = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 NetMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 CampaignScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AdversaryEscalationLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 NextAdversaryMissionSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AdversaryMissionsLaunched = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AdversaryMissionsEscaped = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 AdversaryMissionsThwarted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 AdversaryResolvedMissions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 VictoryThwartedMissionTarget = 12;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 VictoryEscalationTarget = 5;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 RegionalCollapsePressureThreshold = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 HighestRegionalPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 ResolvedMissionsUntilNextEscalation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bAtMaximumAdversaryEscalation = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FGuid PrimaryBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bRequiresBase = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bCanAdvanceTime = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bDecisionRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicHorizonCompactView HorizonCompact;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicRegionView> Regions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicBaseView> Bases;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMutualAidConvoyView> MutualAidConvoys;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicPersonnelView> Personnel;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicMemorialView> Memorial;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicCraftView> Craft;

	/** Hidden contacts are intentionally absent. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicContactView> Contacts;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicBaseAssaultView> BaseAssaults;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicSiteView> Sites;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicProjectView> Projects;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicActionOptionView> ActionOptions;

	/** Research-authorized archive records only; bodies for classified records never enter presentation. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	TArray<FStrategicArchiveEntryView> ArchiveEntries;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	int32 ArchiveTotalCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Archive")
	int32 ArchiveLockedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicGlobeMarkerView> GlobeMarkers;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FStrategicGlobeRouteView> GlobeRoutes;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FGuid> PendingOperationIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	TArray<FGuid> TacticalBattleIds;
};

class UEGTCORE_API FStrategicPresentationService final
{
public:
	static FStrategicDashboardSnapshot BuildDashboard(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config);
};
