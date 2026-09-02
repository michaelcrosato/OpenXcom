#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Campaign/CampaignSave.h"

#include "CampaignSaveStore.generated.h"

UENUM(BlueprintType)
enum class ECampaignSaveSource : uint8
{
	None,
	Primary,
	Temporary,
	Backup
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveStoreResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bSucceeded = false;

	/** True when load selected a verified temporary or backup file. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bRecovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	ECampaignSaveSource Source = ECampaignSaveSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FCampaignSaveEnvelope Envelope;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bLoadable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bRecovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	ECampaignSaveSource Source = ECampaignSaveSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FDateTime LastSavedUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FDateTime CampaignTimeUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	ECampaignDifficulty Difficulty = ECampaignDifficulty::Standard;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	int64 Funds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	int64 CampaignScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FString BuildVersion;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveDiagnostic> Diagnostics;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveSlotListResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveSlotSummary> Slots;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/**
 * Slot storage with verified temporary writes and recoverable primary/backup rotation.
 * The caller supplies the platform-appropriate save directory.
 */
class UEGTCORE_API FCampaignSaveStore final
{
public:
	static bool IsValidSlotName(const FString& SlotName);

	static FString GetPrimaryPath(const FString& SaveDirectory, const FString& SlotName);
	static FString GetTemporaryPath(const FString& SaveDirectory, const FString& SlotName);
	static FString GetBackupPath(const FString& SaveDirectory, const FString& SlotName);

	static FCampaignSaveStoreResult Save(
		const FString& SaveDirectory,
		const FString& SlotName,
		const FCampaignSaveEnvelope& Envelope,
		const FDateTime& WallClockUtc);

	static FCampaignSaveStoreResult Load(const FString& SaveDirectory, const FString& SlotName);
	static FCampaignSaveStoreResult Load(
		const FString& SaveDirectory,
		const FString& SlotName,
		const TArray<FCampaignContentVersion>& ExpectedContentPackages);

	static FCampaignSaveSlotListResult List(const FString& SaveDirectory);
	static FCampaignSaveSlotListResult List(
		const FString& SaveDirectory,
		const TArray<FCampaignContentVersion>& ExpectedContentPackages);
};
