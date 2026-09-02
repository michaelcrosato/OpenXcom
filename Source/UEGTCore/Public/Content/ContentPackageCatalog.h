#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"

#include "ContentPackageCatalog.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FContentCatalogLoadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	bool bSucceeded = false;

	/** Packages normalized into their resolved load order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	TArray<FContentPackage> Packages;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	FResolvedRuleSet RuleSet;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	TArray<FString> LoadedFiles;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	TArray<FContentDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Discovers, strictly parses, and resolves JSON packages from deterministic directory roots. */
class UEGTCORE_API FContentPackageCatalog final
{
public:
	/** Compatibility helper for one required directory tree. */
	static FContentCatalogLoadResult LoadDirectory(const FString& Directory);

	/**
	 * Loads every required directory as one catalog. Roots and files are normalized, sorted, and
	 * de-duplicated before parsing so base content and user mods resolve identically across runs.
	 */
	static FContentCatalogLoadResult LoadDirectories(const TArray<FString>& Directories);
};
