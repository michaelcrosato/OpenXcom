#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Strategic/PersonnelServiceHistory.h"
#include "Strategic/StrategicCampaignState.h"

#include "PersonnelMentorship.generated.h"

/** Immutable late-career guidance projection derived from an assigned field team. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelMentorshipView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bHasMentor = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FGuid MentorId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString MentorDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FPersonnelServiceHistoryView MentorServiceHistory;

	/** Nominal starting-morale bonus before the existing 100-point cap. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 MoraleBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 RecipientCount = 0;

	/** Stable, deduplicated lower-band recipients in lexical personnel-id order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	TArray<FGuid> RecipientIds;
};

/** Pure deterministic Watchkeeper Guidance evaluation shared by strategy and tactics. */
class UEGTCORE_API FPersonnelMentorship final
{
public:
	static FName PolicyId();

	/** Selects one 10+-mission mentor and lower-service-band recipients without mutating state. */
	static FPersonnelMentorshipView Evaluate(
		const FCampaignState& Campaign,
		const TArray<FGuid>& PersonnelIds);
};
