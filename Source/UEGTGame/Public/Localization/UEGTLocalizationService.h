#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"

struct FUEGTLocalizedTextEntry
{
	FString Source;
	TMap<FString, FString> Translations;
};

struct FUEGTLocalizationCatalog
{
	int32 SchemaVersion = 0;
	FName CatalogId;
	FString SourceCulture;
	TArray<FString> Cultures;
	TMap<FString, FUEGTLocalizedTextEntry> Entries;

	FString Resolve(const FString& Key, const FString& EnglishFallback, const FString& CultureName) const;
};

struct FUEGTLocalizationLoadResult
{
	bool bSucceeded = false;
	FUEGTLocalizationCatalog Catalog;
	TArray<FString> Diagnostics;
};

/** Strict, staged localization catalog for the native UEGT shell. */
class UEGTGAME_API FUEGTLocalizationService
{
public:
	static FUEGTLocalizationLoadResult ParseCatalog(const FString& Json);
	static FUEGTLocalizationLoadResult LoadCatalogFile(const FString& Filename);
	static FUEGTLocalizationLoadResult ReloadDefaultCatalog();

	/** Resolves through the active Unreal language, with an explicit English call-site fallback. */
	static FString Text(const FString& Key, const FString& EnglishFallback);

	/** Explicit-culture overload used by locale snapshots and culture-change feedback. */
	static FString TextForCulture(
		const FString& Key,
		const FString& EnglishFallback,
		const FString& CultureName);

	/** Resolves a rule's staged content.<rule-id>.name entry without coupling UEGTCore to locale state. */
	static FString ContentName(FName RuleId, const FString& EnglishFallback);

	/** Explicit-culture content-name overload used by locale and adapter automation. */
	static FString ContentNameForCulture(
		FName RuleId,
		const FString& EnglishFallback,
		const FString& CultureName);

	/** Resolves a staged content.<rule-id>.<field-id> entry such as an archive summary or body. */
	static FString ContentField(
		FName RuleId,
		FName FieldId,
		const FString& EnglishFallback);

	/** Explicit-culture content-field overload used by complete content locale snapshots. */
	static FString ContentFieldForCulture(
		FName RuleId,
		FName FieldId,
		const FString& EnglishFallback,
		const FString& CultureName);

	/** Resolves a stable backend diagnostic code, falling back through a localized diagnostic family. */
	static FString DiagnosticText(FName Code, const FString& EnglishFallback);

	/** Explicit-culture diagnostic overload used by rejection-path locale automation. */
	static FString DiagnosticTextForCulture(
		FName Code,
		const FString& EnglishFallback,
		const FString& CultureName);

	static FString GetDefaultCatalogFilename();
	static TArray<FString> GetSupportedCultures();
	static bool IsCatalogReady();
	static int32 GetActiveEntryCount();
};
