#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"

#include "PersonnelServiceHistory.generated.h"

/** Derived career bands used by roster, memorial, and debrief presentation. */
UENUM(BlueprintType)
enum class EPersonnelServiceBand : uint8
{
	FirstWatch,
	FieldProven,
	LongWatch,
	LegacyAnchor
};

/** Immutable milestone projection; no value in this view is persisted to a campaign save. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelServiceHistoryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelServiceBand Band = EPersonnelServiceBand::FirstWatch;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelServiceBand NextBand = EPersonnelServiceBand::FieldProven;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 NextBandMissions = 5;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 MissionsUntilNextBand = 5;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bMaximumBand = false;
};

class UEGTCORE_API FPersonnelServiceHistory final
{
public:
	/** Projects exact 0/5/10/20-mission bands without mutating campaign state. */
	static FPersonnelServiceHistoryView Project(int32 Missions);
};
