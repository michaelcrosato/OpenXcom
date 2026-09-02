#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "CraftServiceQueue.generated.h"

/** Immutable service-lane assignment and estimate for one craft. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FCraftServiceQueueView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	FGuid CraftId;

	/** One lane is supplied by each operational facility with positive craft capacity. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int32 ServiceLaneCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int32 ActiveServiceCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int32 TotalServiceCraftCount = 0;

	/** One-based position in shortest-turnaround-first order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int32 QueuePosition = 0;

	/** One-based position among waiting craft; zero while occupying a lane. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int32 WaitingPosition = 0;

	/** One-based active or projected lane assignment. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int32 ServiceLaneNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	bool bInServiceLane = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int64 EstimatedWaitSeconds = 0;

	/** Exact wait plus the craft's concurrent repair/refuel duration, saturated at MAX_int64. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	int64 EstimatedReadySeconds = 0;
};

/** Stable per-base Rapid Turnaround projection for every valid active service job. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FCraftServiceQueueSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Craft")
	TArray<FCraftServiceQueueView> Craft;

	const FCraftServiceQueueView* FindCraft(const FGuid& CraftId) const;
};

/** Pure deterministic Flight-Deck Rotation evaluation shared by simulation and presentation. */
class UEGTCORE_API FCraftServiceQueue final
{
public:
	static FName PolicyId();

	/** Projects operational lanes, active jobs, queue positions, and exact ready estimates. */
	static FCraftServiceQueueSnapshot Evaluate(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules);
};
