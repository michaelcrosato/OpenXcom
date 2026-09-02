#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Strategic/StrategicCampaignState.h"

#include "CampaignSave.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignContentVersion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign Save")
	FName PackageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Campaign Save")
	FString Version;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveHeader
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	int32 FormatVersion = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FGuid CampaignId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FDateTime CreatedUtc;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FDateTime LastSavedUtc;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FString BuildVersion;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	TArray<FCampaignContentVersion> ContentPackages;

	/** SHA-256 of the normalized package-id/version list. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FString ContentFingerprint;

	/** SHA-256 over all persisted fields other than this checksum. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FString SaveChecksum;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveEnvelope
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FCampaignSaveHeader Header;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Campaign Save")
	FCampaignState State;
};

UENUM(BlueprintType)
enum class ECampaignSaveDiagnosticSeverity : uint8
{
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	ECampaignSaveDiagnosticSeverity Severity = ECampaignSaveDiagnosticSeverity::Error;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FString Message;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveWriteResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bSucceeded = false;

	/** Normalized and sealed copy corresponding exactly to Json. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FCampaignSaveEnvelope Envelope;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FString Json;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCampaignSaveReadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	bool bMigrated = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	FCampaignSaveEnvelope Envelope;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign Save")
	TArray<FCampaignSaveDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Strict, deterministic JSON codec for the original UEGT campaign-save format. */
class UEGTCORE_API FCampaignSaveCodec final
{
public:
	static constexpr int32 CurrentFormatVersion = 44;
	static constexpr int32 OldestSupportedFormatVersion = 1;

	static FCampaignSaveEnvelope CreateNew(
		const FCampaignState& State,
		const TArray<FCampaignContentVersion>& ContentPackages,
		const FString& BuildVersion,
		const FDateTime& WallClockUtc,
		FGuid CampaignId = FGuid());

	/** Normalizes ordering, validates, seals, and serializes a current-version save. */
	static FCampaignSaveWriteResult Serialize(const FCampaignSaveEnvelope& Envelope);

	static FCampaignSaveReadResult Deserialize(const FString& Json);
	static FCampaignSaveReadResult Deserialize(
		const FString& Json,
		const TArray<FCampaignContentVersion>& ExpectedContentPackages);

	static FCampaignSaveValidationResult Validate(const FCampaignSaveEnvelope& Envelope);
	static FCampaignSaveValidationResult Validate(
		const FCampaignSaveEnvelope& Envelope,
		const TArray<FCampaignContentVersion>& ExpectedContentPackages);

	static FString ComputeContentFingerprint(const TArray<FCampaignContentVersion>& ContentPackages);

	/** Stable checksum used by migration tooling and save-integrity diagnostics. */
	static FString ComputeEnvelopeChecksum(const FCampaignSaveEnvelope& Envelope);
};
