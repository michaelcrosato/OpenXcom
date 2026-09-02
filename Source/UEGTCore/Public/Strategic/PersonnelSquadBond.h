// Copyright 2026 UEGT contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Strategic/StrategicCampaignState.h"
#include "PersonnelSquadBond.generated.h"

UENUM(BlueprintType)
enum class EPersonnelSquadBondTier : uint8
{
	None,
	Aligned,
	Interlocked,
	Unbroken
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelSquadBondPairView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FGuid FirstPersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FGuid SecondPersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString FirstDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FString SecondDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 SharedVictories = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	EPersonnelSquadBondTier Tier = EPersonnelSquadBondTier::None;

	/** Zero at the maximum tier; otherwise the exact cumulative threshold for the next tier. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 NextTierVictories = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ActionPointBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 MoraleBonus = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelSquadBondView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 ResolvedPersonnelCount = 0;

	/** Eligible edges before the non-overlapping selection policy is applied. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	int32 EligiblePairCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	TArray<FPersonnelSquadBondPairView> ActivePairs;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Personnel")
	TArray<FPersonnelSquadBondPairView> DevelopingPairs;
};

/** Mutation-free projection for durable shared-victory records and active non-overlapping pairs. */
class UEGTCORE_API FPersonnelSquadBond final
{
public:
	static constexpr int32 AlignedVictories = 3;
	static constexpr int32 InterlockedVictories = 8;
	static constexpr int32 UnbrokenVictories = 15;

	static EPersonnelSquadBondTier GetTier(int32 SharedVictories);
	static int32 GetNextTierVictories(EPersonnelSquadBondTier Tier);
	static int32 GetActionPointBonus(EPersonnelSquadBondTier Tier);
	static int32 GetMoraleBonus(EPersonnelSquadBondTier Tier);

	static FPersonnelSquadBondView Evaluate(
		const FCampaignState& Campaign,
		const TArray<FGuid>& PersonnelIds);
};
