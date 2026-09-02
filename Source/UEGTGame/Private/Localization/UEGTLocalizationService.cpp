// Copyright 2026 UEGT contributors. MIT License.

#include "Localization/UEGTLocalizationService.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UEGTLocalizationPrivate
{
	TOptional<FUEGTLocalizationCatalog> ActiveCatalog;
	bool bDefaultLoadAttempted = false;

	FString NormalizeCulture(FString CultureName)
	{
		CultureName.TrimStartAndEndInline();
		CultureName.ToLowerInline();
		CultureName.ReplaceInline(TEXT("_"), TEXT("-"));
		if (CultureName.Len() > 2)
		{
			CultureName = CultureName.Left(2);
		}
		return CultureName;
	}

	bool IsValidCatalogId(const FString& Value)
	{
		if (Value.IsEmpty() || Value[0] < TEXT('a') || Value[0] > TEXT('z'))
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!((Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9'))
				|| Character == TEXT('.') || Character == TEXT('-')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsValidTextKey(const FString& Value)
	{
		if (!IsValidCatalogId(Value) || !Value.Contains(TEXT(".")))
		{
			return false;
		}
		return !Value.EndsWith(TEXT(".")) && !Value.Contains(TEXT(".."));
	}

	TArray<int32> ExtractIndexedPlaceholders(const FString& Value)
	{
		TArray<int32> Placeholders;
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			if (Value[Index] != TEXT('{') || Index + 2 >= Value.Len()
				|| !FChar::IsDigit(Value[Index + 1]))
			{
				continue;
			}
			int32 Cursor = Index + 1;
			int32 PlaceholderIndex = 0;
			while (Cursor < Value.Len() && FChar::IsDigit(Value[Cursor]))
			{
				PlaceholderIndex = PlaceholderIndex * 10 + (Value[Cursor] - TEXT('0'));
				++Cursor;
			}
			if (Cursor < Value.Len() && Value[Cursor] == TEXT('}'))
			{
				Placeholders.Add(PlaceholderIndex);
				Index = Cursor;
			}
		}
		Placeholders.Sort();
		return Placeholders;
	}

	void AddDiagnostic(FUEGTLocalizationLoadResult& Result, const FString& Message)
	{
		Result.Diagnostics.Add(Message);
	}

	void EnsureDefaultCatalog()
	{
		if (!bDefaultLoadAttempted)
		{
			FUEGTLocalizationService::ReloadDefaultCatalog();
		}
	}
}

FString FUEGTLocalizationCatalog::Resolve(
	const FString& Key,
	const FString& EnglishFallback,
	const FString& CultureName) const
{
	const FUEGTLocalizedTextEntry* Entry = Entries.Find(Key);
	if (Entry == nullptr)
	{
		return EnglishFallback;
	}

	FString NormalizedCulture = UEGTLocalizationPrivate::NormalizeCulture(CultureName);
	if (!Cultures.Contains(NormalizedCulture))
	{
		NormalizedCulture = SourceCulture;
	}
	if (const FString* Translation = Entry->Translations.Find(NormalizedCulture))
	{
		return *Translation;
	}
	if (const FString* SourceTranslation = Entry->Translations.Find(SourceCulture))
	{
		return *SourceTranslation;
	}
	return Entry->Source.IsEmpty() ? EnglishFallback : Entry->Source;
}

FUEGTLocalizationLoadResult FUEGTLocalizationService::ParseCatalog(const FString& Json)
{
	using namespace UEGTLocalizationPrivate;

	FUEGTLocalizationLoadResult Result;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AddDiagnostic(Result, TEXT("Localization catalog is not valid JSON."));
		return Result;
	}

	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion)
		|| SchemaVersion != 1.0 || FMath::FloorToDouble(SchemaVersion) != SchemaVersion)
	{
		AddDiagnostic(Result, TEXT("Localization catalog requires schemaVersion 1."));
		return Result;
	}
	Result.Catalog.SchemaVersion = 1;

	FString CatalogId;
	if (!Root->TryGetStringField(TEXT("catalogId"), CatalogId) || !IsValidCatalogId(CatalogId))
	{
		AddDiagnostic(Result, TEXT("Localization catalogId must be a lowercase namespaced id."));
		return Result;
	}
	Result.Catalog.CatalogId = FName(*CatalogId);

	if (!Root->TryGetStringField(TEXT("sourceCulture"), Result.Catalog.SourceCulture))
	{
		AddDiagnostic(Result, TEXT("Localization catalog requires sourceCulture."));
		return Result;
	}
	Result.Catalog.SourceCulture = NormalizeCulture(Result.Catalog.SourceCulture);
	if (Result.Catalog.SourceCulture != TEXT("en"))
	{
		AddDiagnostic(Result, TEXT("Localization sourceCulture must be English (en)."));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* CultureValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("cultures"), CultureValues) || CultureValues == nullptr)
	{
		AddDiagnostic(Result, TEXT("Localization catalog requires a cultures array."));
		return Result;
	}
	TSet<FString> SeenCultures;
	for (const TSharedPtr<FJsonValue>& Value : *CultureValues)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			AddDiagnostic(Result, TEXT("Localization cultures must be strings."));
			return Result;
		}
		const FString Culture = NormalizeCulture(Value->AsString());
		if (Culture.Len() != 2 || SeenCultures.Contains(Culture))
		{
			AddDiagnostic(Result, TEXT("Localization cultures must be unique two-letter codes."));
			return Result;
		}
		SeenCultures.Add(Culture);
		Result.Catalog.Cultures.Add(Culture);
	}
	const TArray<FString> RequiredCultures = GetSupportedCultures();
	const bool bMissingRequiredCulture = RequiredCultures.ContainsByPredicate(
		[&SeenCultures](const FString& Culture) { return !SeenCultures.Contains(Culture); });
	if (Result.Catalog.Cultures.Num() != RequiredCultures.Num()
		|| bMissingRequiredCulture)
	{
		AddDiagnostic(Result, TEXT("Localization catalog must contain en, fr, de, es, and ja exactly once."));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* EntryValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("entries"), EntryValues) || EntryValues == nullptr || EntryValues->IsEmpty())
	{
		AddDiagnostic(Result, TEXT("Localization catalog requires at least one entry."));
		return Result;
	}
	for (const TSharedPtr<FJsonValue>& Value : *EntryValues)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			AddDiagnostic(Result, TEXT("Localization entries must be objects."));
			return Result;
		}
		const TSharedPtr<FJsonObject> EntryObject = Value->AsObject();
		FString Key;
		FUEGTLocalizedTextEntry Entry;
		if (!EntryObject->TryGetStringField(TEXT("key"), Key) || !IsValidTextKey(Key))
		{
			AddDiagnostic(Result, TEXT("Localization entry keys must be lowercase dotted ids."));
			return Result;
		}
		if (Result.Catalog.Entries.Contains(Key))
		{
			AddDiagnostic(Result, FString::Printf(TEXT("Localization entry '%s' is duplicated."), *Key));
			return Result;
		}
		if (!EntryObject->TryGetStringField(TEXT("source"), Entry.Source)
			|| Entry.Source.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(Result, FString::Printf(TEXT("Localization entry '%s' requires source text."), *Key));
			return Result;
		}
		if (!EntryObject->HasTypedField<EJson::Object>(TEXT("translations")))
		{
			AddDiagnostic(Result, FString::Printf(TEXT("Localization entry '%s' requires translations."), *Key));
			return Result;
		}
		const TSharedPtr<FJsonObject> Translations = EntryObject->GetObjectField(TEXT("translations"));
		if (Translations->Values.Num() != RequiredCultures.Num())
		{
			AddDiagnostic(Result, FString::Printf(
				TEXT("Localization entry '%s' must contain exactly the five supported cultures."), *Key));
			return Result;
		}
		for (const FString& Culture : RequiredCultures)
		{
			FString Translation;
			if (!Translations->TryGetStringField(Culture, Translation)
				|| Translation.TrimStartAndEnd().IsEmpty())
			{
				AddDiagnostic(Result, FString::Printf(
					TEXT("Localization entry '%s' requires a non-empty '%s' translation."),
					*Key, *Culture));
				return Result;
			}
			Entry.Translations.Add(Culture, MoveTemp(Translation));
		}
		const TArray<int32> SourcePlaceholders = ExtractIndexedPlaceholders(Entry.Source);
		for (const FString& Culture : RequiredCultures)
		{
			if (ExtractIndexedPlaceholders(Entry.Translations.FindChecked(Culture))
				!= SourcePlaceholders)
			{
				AddDiagnostic(Result, FString::Printf(
					TEXT("Localization entry '%s' translation '%s' must preserve every indexed format placeholder."),
					*Key, *Culture));
				return Result;
			}
		}
		if (Entry.Translations.FindChecked(TEXT("en")) != Entry.Source)
		{
			AddDiagnostic(Result, FString::Printf(
				TEXT("Localization entry '%s' English translation must equal its source."), *Key));
			return Result;
		}
		Result.Catalog.Entries.Add(MoveTemp(Key), MoveTemp(Entry));
	}

	Result.bSucceeded = true;
	return Result;
}

FUEGTLocalizationLoadResult FUEGTLocalizationService::LoadCatalogFile(const FString& Filename)
{
	FUEGTLocalizationLoadResult Result;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Filename))
	{
		Result.Diagnostics.Add(FString::Printf(
			TEXT("Localization catalog '%s' could not be read."), *Filename));
		return Result;
	}
	return ParseCatalog(Json);
}

FUEGTLocalizationLoadResult FUEGTLocalizationService::ReloadDefaultCatalog()
{
	using namespace UEGTLocalizationPrivate;

	bDefaultLoadAttempted = true;
	FUEGTLocalizationLoadResult Result = LoadCatalogFile(GetDefaultCatalogFilename());
	if (Result.bSucceeded)
	{
		ActiveCatalog = Result.Catalog;
	}
	else
	{
		ActiveCatalog.Reset();
	}
	return Result;
}

FString FUEGTLocalizationService::Text(const FString& Key, const FString& EnglishFallback)
{
	const FString Culture = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName();
	return TextForCulture(Key, EnglishFallback, Culture);
}

FString FUEGTLocalizationService::TextForCulture(
	const FString& Key,
	const FString& EnglishFallback,
	const FString& CultureName)
{
	using namespace UEGTLocalizationPrivate;

	EnsureDefaultCatalog();
	return ActiveCatalog.IsSet()
		? ActiveCatalog.GetValue().Resolve(Key, EnglishFallback, CultureName)
		: EnglishFallback;
}

FString FUEGTLocalizationService::ContentName(
	const FName RuleId,
	const FString& EnglishFallback)
{
	return ContentField(RuleId, TEXT("name"), EnglishFallback);
}

FString FUEGTLocalizationService::ContentNameForCulture(
	const FName RuleId,
	const FString& EnglishFallback,
	const FString& CultureName)
{
	return ContentFieldForCulture(RuleId, TEXT("name"), EnglishFallback, CultureName);
}

FString FUEGTLocalizationService::ContentField(
	const FName RuleId,
	const FName FieldId,
	const FString& EnglishFallback)
{
	const FString Culture = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName();
	return ContentFieldForCulture(RuleId, FieldId, EnglishFallback, Culture);
}

FString FUEGTLocalizationService::ContentFieldForCulture(
	const FName RuleId,
	const FName FieldId,
	const FString& EnglishFallback,
	const FString& CultureName)
{
	if (RuleId.IsNone() || FieldId.IsNone())
	{
		return EnglishFallback;
	}
	FString NormalizedRuleId = RuleId.ToString().ToLower();
	NormalizedRuleId.ReplaceInline(TEXT("_"), TEXT("-"));
	FString NormalizedFieldId = FieldId.ToString().ToLower();
	NormalizedFieldId.ReplaceInline(TEXT("_"), TEXT("-"));
	return TextForCulture(
		FString::Printf(TEXT("content.%s.%s"), *NormalizedRuleId, *NormalizedFieldId),
		EnglishFallback,
		CultureName);
}

FString FUEGTLocalizationService::DiagnosticText(
	const FName Code,
	const FString& EnglishFallback)
{
	const FString Culture = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName();
	return DiagnosticTextForCulture(Code, EnglishFallback, Culture);
}

FString FUEGTLocalizationService::DiagnosticTextForCulture(
	const FName Code,
	const FString& EnglishFallback,
	const FString& CultureName)
{
	if (Code.IsNone())
	{
		return EnglishFallback;
	}
	FString NormalizedCode = Code.ToString().ToLower();
	NormalizedCode.ReplaceInline(TEXT("_"), TEXT("-"));
	const FString MissingSentinel = FString::Printf(TEXT("\x1fmissing-diagnostic:%s"), *NormalizedCode);
	const FString Exact = TextForCulture(
		FString::Printf(TEXT("diagnostic.%s"), *NormalizedCode),
		MissingSentinel,
		CultureName);
	if (Exact != MissingSentinel)
	{
		return Exact;
	}

	struct FDiagnosticFamily
	{
		const TCHAR* Key;
		const TCHAR* English;
		bool (*Matches)(const FString&);
	};
	const FDiagnosticFamily Families[] = {
		{ TEXT("diagnostic.generic-capacity"), TEXT("The requested capacity is unavailable."),
			[](const FString& Value) { return Value.Contains(TEXT("capacity-exceeded")); } },
		{ TEXT("diagnostic.generic-unknown"), TEXT("The requested record is no longer available."),
			[](const FString& Value) { return Value.StartsWith(TEXT("unknown-")); } },
		{ TEXT("diagnostic.generic-invalid"), TEXT("The request contains invalid data."),
			[](const FString& Value) { return Value.StartsWith(TEXT("invalid-")); } },
		{ TEXT("diagnostic.generic-insufficient"), TEXT("The request lacks a required resource."),
			[](const FString& Value) { return Value.StartsWith(TEXT("insufficient-")); } },
		{ TEXT("diagnostic.generic-unavailable"), TEXT("The requested action is no longer available."),
			[](const FString& Value) { return Value.EndsWith(TEXT("-unavailable")); } },
		{ TEXT("diagnostic.generic-overflow"), TEXT("The request would exceed a supported limit."),
			[](const FString& Value) { return Value.EndsWith(TEXT("-overflow")); } },
		{ TEXT("diagnostic.generic-missing"), TEXT("A required record or resource is missing."),
			[](const FString& Value) { return Value.EndsWith(TEXT("-missing")); } },
		{ TEXT("diagnostic.generic-already"), TEXT("The requested state is already active."),
			[](const FString& Value)
			{
				return Value.StartsWith(TEXT("already-")) || Value.Contains(TEXT("-already-"));
			} },
		{ TEXT("diagnostic.generic-pending"), TEXT("Resolve the pending decision before continuing."),
			[](const FString& Value) { return Value.EndsWith(TEXT("-pending")); } },
		{ TEXT("diagnostic.generic-failed"), TEXT("The requested operation could not be completed."),
			[](const FString& Value) { return Value.EndsWith(TEXT("-failed")); } },
		{ TEXT("diagnostic.generic-rejected"), TEXT("The requested operation was rejected."),
			[](const FString& Value) { return Value.EndsWith(TEXT("-rejected")); } }
	};
	for (const FDiagnosticFamily& Family : Families)
	{
		if (Family.Matches(NormalizedCode))
		{
			return TextForCulture(Family.Key, Family.English, CultureName);
		}
	}
	return EnglishFallback;
}

FString FUEGTLocalizationService::GetDefaultCatalogFilename()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization"), TEXT("uegt.ui.json"));
}

TArray<FString> FUEGTLocalizationService::GetSupportedCultures()
{
	return { TEXT("en"), TEXT("fr"), TEXT("de"), TEXT("es"), TEXT("ja") };
}

bool FUEGTLocalizationService::IsCatalogReady()
{
	UEGTLocalizationPrivate::EnsureDefaultCatalog();
	return UEGTLocalizationPrivate::ActiveCatalog.IsSet();
}

int32 FUEGTLocalizationService::GetActiveEntryCount()
{
	UEGTLocalizationPrivate::EnsureDefaultCatalog();
	return UEGTLocalizationPrivate::ActiveCatalog.IsSet()
		? UEGTLocalizationPrivate::ActiveCatalog.GetValue().Entries.Num()
		: 0;
}
