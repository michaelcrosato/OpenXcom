#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"

#include "ContentPackageJson.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FContentPackageParseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	FContentPackage Package;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content")
	TArray<FContentDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Strict parser for the original UEGT JSON package schema. */
class UEGTCORE_API FContentPackageJson final
{
public:
	static FContentPackageParseResult ParseString(const FString& Json, const FString& SourceLabel = TEXT("<memory>"));
	static FContentPackageParseResult ParseFile(const FString& FilePath);
};
