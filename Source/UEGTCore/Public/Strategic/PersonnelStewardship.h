// Copyright 2026 UEGT contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "PersonnelStewardship.generated.h"

struct FStrategicSimulationConfig;

USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelStewardshipOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelStewardshipFocus Focus = EPersonnelStewardshipFocus::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 DurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ReductionPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString UnavailableReason;
};

/** Immutable projection for one base's active or available Stewardship Rotation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelStewardshipView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bBaseHasActiveSteward = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bSelectedPersonnelIsSteward = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bEligible = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FGuid StewardId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString StewardDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelStewardshipFocus ActiveFocus = EPersonnelStewardshipFocus::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName ActivePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 RemainingSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int64 DurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 MinimumMissions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ReductionPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ToursCompleted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ResolveAwardTourCap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ResolveBonusOnCompletion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	TArray<FPersonnelStewardshipOptionView> Options;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString UnavailableReason;
};

/** Pure deterministic base-leadership policy shared by commands and presentation. */
class UEGTCORE_API FPersonnelStewardship final
{
public:
	static FName PolicyId(EPersonnelStewardshipFocus Focus);
	static bool IsKnown(EPersonnelStewardshipFocus Focus);
	static bool IsSelected(EPersonnelStewardshipFocus Focus);
	static bool IsConfigValid(const FStrategicSimulationConfig& Config);

	/** Returns the lexical-first active steward at a base; valid state permits at most one. */
	static const FPersonnelState* FindActiveSteward(const FCampaignState& Campaign, const FGuid& BaseId);
	static bool HasActiveFocus(
		const FCampaignState& Campaign,
		const FGuid& BaseId,
		EPersonnelStewardshipFocus Focus);

	/** Applies a positive percentage reduction with deterministic ceiling division. */
	static bool TryApplyReductionCeil(int64 Baseline, int32 ReductionPercent, int64& OutReduced);

	static FPersonnelStewardshipView Evaluate(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FGuid& PersonnelId);
};
