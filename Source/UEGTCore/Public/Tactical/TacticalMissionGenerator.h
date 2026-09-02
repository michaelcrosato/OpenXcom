#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "TacticalMissionGenerator.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalGenerationDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FString Message;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FTacticalBattleState Battle;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalGenerationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Pure deterministic battlefield materialization and invariant validation. */
class UEGTCORE_API FTacticalMissionGenerator final
{
public:
	static FTacticalGenerationResult Generate(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		FGuid OperationId);

	static bool ValidateBattle(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		TArray<FTacticalGenerationDiagnostic>& OutDiagnostics);
};
