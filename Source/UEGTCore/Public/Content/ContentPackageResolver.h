#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"

#include "ContentPackageResolver.generated.h"

UENUM(BlueprintType)
enum class EContentDiagnosticSeverity : uint8
{
	Warning,
	Error
};

/** Versioned manifest for one original UEGT content or mod package. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FContentPackageDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	FName PackageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	FString Version = TEXT("0.1.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	int32 SchemaVersion = 1;

	/** Lower priorities load first; later packages can override earlier data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	int32 Priority = 0;

	/** Required packages. A missing entry is a hard error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FName> Dependencies;

	/** Optional ordering edges. Missing entries are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Content")
	TArray<FName> LoadAfter;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FContentDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	EContentDiagnosticSeverity Severity = EContentDiagnosticSeverity::Error;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	FName PackageId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	FString Message;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FContentResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	TArray<FName> LoadOrder;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	TArray<FContentDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Validates package manifests and produces a stable topological load order. */
class UEGTCORE_API FContentPackageResolver final
{
public:
	static constexpr int32 CurrentSchemaVersion = 1;

	static FContentResolution Resolve(const TArray<FContentPackageDescriptor>& Packages);
	static bool IsValidPackageId(FName PackageId);
	/** Validate serialized spelling before FName conversion can discard casing. */
	static bool IsValidPackageIdText(const FString& Value);
};
