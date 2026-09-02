// Copyright 2026 UEGT contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Strategic/StrategicCampaignState.h"

#include "PersonnelRecoveryPlan.generated.h"

struct FStrategicSimulationConfig;

USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelRecoveryPlanOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelRecoveryPlan Plan = EPersonnelRecoveryPlan::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 DurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 FundingCost = 0;

	/** Exact funding saved by an active Recovery Advocacy rotation at this person's base. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 StewardshipFundingDiscount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bStewardshipBenefitApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ResolveBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString UnavailableReason;
};

/** Immutable projection for one recovering person's mandatory Return Path decision. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelRecoveryPlanView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bRecovering = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bDecisionRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelRecoveryPlan SelectedPlan = EPersonnelRecoveryPlan::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName SelectedPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 BaselineRemainingSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	TArray<FPersonnelRecoveryPlanOptionView> Options;
};

/** Pure deterministic Return Path evaluation shared by commands and strategic presentation. */
class UEGTCORE_API FPersonnelRecoveryPlan final
{
public:
	static FName PolicyId(EPersonnelRecoveryPlan Plan);
	static bool IsKnown(EPersonnelRecoveryPlan Plan);
	static bool IsSelected(EPersonnelRecoveryPlan Plan);

	static FPersonnelRecoveryPlanView Evaluate(
		const FCampaignState& Campaign,
		const FStrategicSimulationConfig& Config,
		const FGuid& PersonnelId);
};
