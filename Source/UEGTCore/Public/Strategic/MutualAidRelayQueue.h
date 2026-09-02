#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "MutualAidRelayQueue.generated.h"

/** Immutable Relay Weave assignment and estimate for one lossless convoy. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidRelayQueueView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid SourceBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 DispatchSequence = 0;

	/** Integrity-scaled signal channels available at the source base. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RelayChannelCount = 0;

	/** Channels supplied directly by operational signal facilities before staffing. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 FacilityRelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 SignalWatchScientistCount = 0;

	/** Effective staffed surge channels after damage suppression. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 SignalWatchBonusChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ActiveConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 TotalConvoyCount = 0;

	/** One-based position in FIFO dispatch order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 QueuePosition = 0;

	/** One-based position among held convoys; zero while occupying a relay channel. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 WaitingPosition = 0;

	/** Number of commitments waiting behind the active relay channels. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 WaitingConvoyCount = 0;

	/** Share of this source line held by queue congestion, clamped to [0, 100]. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 QueuePressurePercent = 0;

	/** One-based active or projected relay channel assignment. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RelayChannelNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bRelayAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bInTransit = false;

	/** Exact projected hold before this convoy can advance; zero when relay is offline. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 EstimatedWaitSeconds = 0;

	/** Hold plus remaining route and any still-pending deterministic delay. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 EstimatedArrivalSeconds = 0;
};

/** Immutable aggregate Relay Weave load projection for one established source base. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidRelayQueueBaseView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 FacilityRelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 SignalWatchScientistCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 SignalWatchBonusChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ActiveConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 TotalConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 WaitingConvoyCount = 0;

	/** Share of this source line held by queue congestion, clamped to [0, 100]. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 QueuePressurePercent = 0;

	/** Exact arrival horizon of the last currently committed convoy, or zero when clear. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 QueueTailArrivalSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bRelayAvailable = false;
};

/** Stable per-base Relay Weave projection for every active convoy. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidRelayQueueSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> Convoys;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueBaseView> Bases;

	const FMutualAidRelayQueueView* FindConvoy(const FGuid& ConvoyId) const;
	const FMutualAidRelayQueueBaseView* FindBase(const FGuid& BaseId) const;
};

/** Pure deterministic signal-channel scheduling shared by simulation and presentation. */
class UEGTCORE_API FMutualAidRelayQueue final
{
public:
	static FName PolicyId();
	static FName SignalWatchPolicyId();

	/** Facility-only baseline: one channel per started 50 integrity-scaled detection points. */
	static int32 EvaluateFacilityRelayChannelCount(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	/** Facility baseline plus one staffed surge channel per effective Signal Watch scientist. */
	static int32 EvaluateRelayChannelCount(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	/** Projects all active and held convoys in stable FIFO order. */
	static FMutualAidRelayQueueSnapshot Evaluate(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules);

	/** Projects the next dispatch without mutating campaign state. */
	static FMutualAidRelayQueueView ProjectNext(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FGuid& SourceBaseId,
		int64 JourneySeconds);

	/** Remaining route plus a still-pending deterministic unescorted delay. */
	static int64 ProjectedJourneySeconds(const FMutualAidConvoyState& Convoy);
};
