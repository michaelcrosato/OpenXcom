#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/PersonnelServiceHistory.h"
#include "Strategic/StrategicCampaignState.h"

#include "PersonnelLegacyRelay.generated.h"

/** Immutable late-career doctrine relay derived from an assigned field team. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelLegacyRelayView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bHasSpecialist = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FGuid SpecialistId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString SpecialistDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FPersonnelServiceHistoryView SpecialistServiceHistory;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName DoctrineId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString DoctrineDisplayName;

	/** Half of one authored doctrine selection, rounded up; temporary tactical bonuses only. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 AccuracyBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ResolveBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 MobilityBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 StrengthBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 RecipientCount = 0;

	/** Stable, deduplicated teammates other than the selected specialist. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	TArray<FGuid> RecipientIds;
};

/** Pure deterministic Legacy Relay evaluation shared by strategy and tactics. */
class UEGTCORE_API FPersonnelLegacyRelay final
{
public:
	static FName PolicyId();

	/** Selects one 20+-mission, maxed-doctrine specialist without mutating state or rules. */
	static FPersonnelLegacyRelayView Evaluate(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const TArray<FGuid>& PersonnelIds);
};
