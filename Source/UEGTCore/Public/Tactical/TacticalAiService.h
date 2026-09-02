#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "TacticalAiService.generated.h"

UENUM(BlueprintType)
enum class ETacticalAiGoal : uint8
{
	Guard,
	Engage,
	Advance,
	Withdraw,
	ControlObjective
};

UENUM(BlueprintType)
enum class ETacticalAiActionType : uint8
{
	None,
	AttackUnit,
	ProjectSignal,
	Move,
	OpenDoor,
	ChangeStance,
	InteractObjective
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalAiDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	FString Message;
};

/** One deterministic, immediately executable adversary decision over the current battle state. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalAiDecision
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	ETacticalAiGoal Goal = ETacticalAiGoal::Guard;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	ETacticalAiActionType ActionType = ETacticalAiActionType::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	FGuid UnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	FGuid TargetUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	FName ObjectiveId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 DestinationX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 DestinationY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 DestinationZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	ETacticalStance DesiredStance = ETacticalStance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 PerceivedHostileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 MovementCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 HitChance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	int32 UtilityScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|AI")
	TArray<FTacticalAiDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Pure deterministic adversary perception and next-action selection. */
class UEGTCORE_API FTacticalAiService final
{
public:
	static FTacticalAiDecision ChooseAction(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		FGuid UnitId);
};
