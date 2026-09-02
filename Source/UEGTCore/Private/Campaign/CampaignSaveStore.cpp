// Copyright 2026 UEGT contributors. MIT License.

#include "Campaign/CampaignSaveStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CampaignSaveStorePrivate
{
	void AddDiagnostic(
		TArray<FCampaignSaveDiagnostic>& Diagnostics,
		const ECampaignSaveDiagnosticSeverity Severity,
		const FName Code,
		FString Message)
	{
		FCampaignSaveDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool ValidateLocation(
		const FString& SaveDirectory,
		const FString& SlotName,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		if (SaveDirectory.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_save_directory"), TEXT("Save directory cannot be empty."));
			return false;
		}
		if (!FCampaignSaveStore::IsValidSlotName(SlotName))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_slot_name"), TEXT("Slot name must contain 1-48 ASCII letters, digits, underscores, or hyphens."));
			return false;
		}
		return true;
	}

	struct FCandidate
	{
		ECampaignSaveSource Source = ECampaignSaveSource::None;
		FString Path;
		int32 TieBreakPriority = 0;
		FCampaignSaveReadResult Read;
	};

	const TCHAR* SourceLabel(const ECampaignSaveSource Source)
	{
		switch (Source)
		{
		case ECampaignSaveSource::Primary:
			return TEXT("primary");
		case ECampaignSaveSource::Temporary:
			return TEXT("temporary");
		case ECampaignSaveSource::Backup:
			return TEXT("backup");
		default:
			return TEXT("unknown");
		}
	}

	FCampaignSaveStoreResult LoadInternal(
		const FString& SaveDirectory,
		const FString& SlotName,
		const TArray<FCampaignContentVersion>* ExpectedContentPackages)
	{
		FCampaignSaveStoreResult Result;
		if (!ValidateLocation(SaveDirectory, SlotName, Result.Diagnostics))
		{
			return Result;
		}

		TArray<FCandidate> Candidates;
		Candidates.Add({ ECampaignSaveSource::Primary, FCampaignSaveStore::GetPrimaryPath(SaveDirectory, SlotName), 3 });
		Candidates.Add({ ECampaignSaveSource::Temporary, FCampaignSaveStore::GetTemporaryPath(SaveDirectory, SlotName), 2 });
		Candidates.Add({ ECampaignSaveSource::Backup, FCampaignSaveStore::GetBackupPath(SaveDirectory, SlotName), 1 });

		IFileManager& FileManager = IFileManager::Get();
		int32 ExistingFiles = 0;
		FCandidate* Best = nullptr;
		for (FCandidate& Candidate : Candidates)
		{
			if (!FileManager.FileExists(*Candidate.Path))
			{
				continue;
			}
			++ExistingFiles;

			FString Json;
			if (!FFileHelper::LoadFileToString(Json, *Candidate.Path))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Warning, TEXT("save_candidate_unreadable"), FString::Printf(TEXT("The %s save candidate could not be read."), SourceLabel(Candidate.Source)));
				continue;
			}

			Candidate.Read = ExpectedContentPackages == nullptr
				? FCampaignSaveCodec::Deserialize(Json)
				: FCampaignSaveCodec::Deserialize(Json, *ExpectedContentPackages);
			if (!Candidate.Read.bSucceeded)
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Warning, TEXT("save_candidate_invalid"), FString::Printf(TEXT("The %s save candidate failed validation."), SourceLabel(Candidate.Source)));
				continue;
			}

			if (Best == nullptr
				|| Candidate.Read.Envelope.Header.LastSavedUtc > Best->Read.Envelope.Header.LastSavedUtc
				|| (Candidate.Read.Envelope.Header.LastSavedUtc == Best->Read.Envelope.Header.LastSavedUtc && Candidate.TieBreakPriority > Best->TieBreakPriority))
			{
				Best = &Candidate;
			}
		}

		if (Best == nullptr)
		{
			if (ExistingFiles == 0)
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("save_not_found"), TEXT("No file exists for the requested campaign slot."));
			}
			else
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("no_valid_save_candidate"), TEXT("Primary, temporary, and backup campaign saves are all unreadable or invalid."));
			}
			return Result;
		}

		Result.bSucceeded = true;
		Result.Source = Best->Source;
		Result.bRecovered = Best->Source != ECampaignSaveSource::Primary;
		Result.Envelope = Best->Read.Envelope;
		Result.Diagnostics.Append(Best->Read.Diagnostics);
		if (Result.bRecovered)
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Warning, TEXT("save_recovered"), FString::Printf(TEXT("Loaded the verified %s campaign save candidate."), SourceLabel(Best->Source)));
		}
		return Result;
	}

	bool TryExtractSlotName(const FString& Filename, FString& OutSlotName)
	{
		static const FString TemporarySuffix = TEXT(".uegtsave.tmp");
		static const FString BackupSuffix = TEXT(".uegtsave.bak");
		static const FString PrimarySuffix = TEXT(".uegtsave");
		int32 SuffixLength = 0;
		if (Filename.EndsWith(TemporarySuffix, ESearchCase::IgnoreCase))
		{
			SuffixLength = TemporarySuffix.Len();
		}
		else if (Filename.EndsWith(BackupSuffix, ESearchCase::IgnoreCase))
		{
			SuffixLength = BackupSuffix.Len();
		}
		else if (Filename.EndsWith(PrimarySuffix, ESearchCase::IgnoreCase))
		{
			SuffixLength = PrimarySuffix.Len();
		}
		if (SuffixLength <= 0)
		{
			return false;
		}
		OutSlotName = Filename.LeftChop(SuffixLength);
		return FCampaignSaveStore::IsValidSlotName(OutSlotName);
	}

	FCampaignSaveSlotListResult ListInternal(
		const FString& SaveDirectory,
		const TArray<FCampaignContentVersion>* ExpectedContentPackages)
	{
		FCampaignSaveSlotListResult Result;
		if (SaveDirectory.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
				TEXT("invalid_save_directory"), TEXT("Save directory cannot be empty."));
			return Result;
		}

		IFileManager& FileManager = IFileManager::Get();
		if (!FileManager.DirectoryExists(*SaveDirectory))
		{
			Result.bSucceeded = true;
			return Result;
		}
		TArray<FString> Filenames;
		FileManager.FindFiles(Filenames, *FPaths::Combine(SaveDirectory, TEXT("*.uegtsave*")), true, false);
		TMap<FString, FString> SlotByLowerName;
		for (const FString& Filename : Filenames)
		{
			FString SlotName;
			if (!TryExtractSlotName(Filename, SlotName))
			{
				continue;
			}
			const FString LowerName = SlotName.ToLower();
			FString* Existing = SlotByLowerName.Find(LowerName);
			if (Existing == nullptr || SlotName < *Existing)
			{
				SlotByLowerName.Add(LowerName, SlotName);
			}
		}

		TArray<FString> SlotNames;
		SlotByLowerName.GenerateValueArray(SlotNames);
		SlotNames.Sort();
		for (const FString& SlotName : SlotNames)
		{
			const FCampaignSaveStoreResult Load = LoadInternal(SaveDirectory, SlotName, ExpectedContentPackages);
			FCampaignSaveSlotSummary& Summary = Result.Slots.AddDefaulted_GetRef();
			Summary.SlotName = SlotName;
			Summary.bLoadable = Load.bSucceeded;
			Summary.bRecovered = Load.bRecovered;
			Summary.Source = Load.Source;
			Summary.Diagnostics = Load.Diagnostics;
			if (Load.bSucceeded)
			{
				Summary.LastSavedUtc = Load.Envelope.Header.LastSavedUtc;
				Summary.CampaignTimeUtc = Load.Envelope.State.StrategicTime.Utc;
				Summary.Difficulty = Load.Envelope.State.Difficulty;
				Summary.Funds = Load.Envelope.State.Funds;
				Summary.CampaignScore = Load.Envelope.State.CampaignScore;
				Summary.BuildVersion = Load.Envelope.Header.BuildVersion;
			}
		}
		Result.Slots.Sort([](const FCampaignSaveSlotSummary& Left, const FCampaignSaveSlotSummary& Right)
		{
			if (Left.bLoadable != Right.bLoadable)
			{
				return Left.bLoadable;
			}
			if (Left.LastSavedUtc != Right.LastSavedUtc)
			{
				return Left.LastSavedUtc > Right.LastSavedUtc;
			}
			return Left.SlotName < Right.SlotName;
		});
		Result.bSucceeded = true;
		return Result;
	}
}

bool FCampaignSaveStoreResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FCampaignSaveDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FCampaignSaveSlotListResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FCampaignSaveDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FCampaignSaveStore::IsValidSlotName(const FString& SlotName)
{
	if (SlotName.IsEmpty() || SlotName.Len() > 48)
	{
		return false;
	}
	for (const TCHAR Character : SlotName)
	{
		const bool bLetter = (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('A') && Character <= TEXT('Z'));
		const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
		if (!bLetter && !bDigit && Character != TEXT('_') && Character != TEXT('-'))
		{
			return false;
		}
	}
	return true;
}

FString FCampaignSaveStore::GetPrimaryPath(const FString& SaveDirectory, const FString& SlotName)
{
	return FPaths::Combine(SaveDirectory, SlotName + TEXT(".uegtsave"));
}

FString FCampaignSaveStore::GetTemporaryPath(const FString& SaveDirectory, const FString& SlotName)
{
	return GetPrimaryPath(SaveDirectory, SlotName) + TEXT(".tmp");
}

FString FCampaignSaveStore::GetBackupPath(const FString& SaveDirectory, const FString& SlotName)
{
	return GetPrimaryPath(SaveDirectory, SlotName) + TEXT(".bak");
}

FCampaignSaveStoreResult FCampaignSaveStore::Save(
	const FString& SaveDirectory,
	const FString& SlotName,
	const FCampaignSaveEnvelope& InputEnvelope,
	const FDateTime& WallClockUtc)
{
	using namespace CampaignSaveStorePrivate;

	FCampaignSaveStoreResult Result;
	if (!ValidateLocation(SaveDirectory, SlotName, Result.Diagnostics))
	{
		return Result;
	}

	FCampaignSaveEnvelope Envelope = InputEnvelope;
	Envelope.Header.LastSavedUtc = WallClockUtc;
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	Result.Diagnostics.Append(Write.Diagnostics);
	if (!Write.bSucceeded)
	{
		return Result;
	}

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.MakeDirectory(*SaveDirectory, true) && !FileManager.DirectoryExists(*SaveDirectory))
	{
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("save_directory_create_failed"), TEXT("Could not create the campaign save directory."));
		return Result;
	}

	const FString PrimaryPath = GetPrimaryPath(SaveDirectory, SlotName);
	const FString TemporaryPath = GetTemporaryPath(SaveDirectory, SlotName);
	const FString BackupPath = GetBackupPath(SaveDirectory, SlotName);
	if (!FFileHelper::SaveStringToFile(Write.Json, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("temporary_write_failed"), TEXT("Could not write the temporary campaign save."));
		return Result;
	}

	FString VerificationJson;
	const bool bTemporaryReadable = FFileHelper::LoadFileToString(VerificationJson, *TemporaryPath);
	const FCampaignSaveReadResult Verification = bTemporaryReadable
		? FCampaignSaveCodec::Deserialize(VerificationJson, Write.Envelope.Header.ContentPackages)
		: FCampaignSaveReadResult();
	if (!bTemporaryReadable || !Verification.bSucceeded || Verification.Envelope.Header.SaveChecksum != Write.Envelope.Header.SaveChecksum)
	{
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("temporary_verification_failed"), TEXT("Temporary campaign save did not pass read-back verification."));
		return Result;
	}

	if (FileManager.FileExists(*PrimaryPath)
		&& !FileManager.Move(*BackupPath, *PrimaryPath, true, true, false, true))
	{
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("backup_rotation_failed"), TEXT("Could not rotate the current campaign save to its backup."));
		return Result;
	}
	if (!FileManager.Move(*PrimaryPath, *TemporaryPath, true, true, false, true))
	{
		if (!FileManager.FileExists(*PrimaryPath) && FileManager.FileExists(*BackupPath))
		{
			FileManager.Move(*PrimaryPath, *BackupPath, true, true, false, true);
		}
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("save_commit_failed"), TEXT("Could not atomically promote the verified temporary campaign save."));
		return Result;
	}

	Result.bSucceeded = true;
	Result.Source = ECampaignSaveSource::Primary;
	Result.Envelope = Write.Envelope;
	return Result;
}

FCampaignSaveStoreResult FCampaignSaveStore::Load(const FString& SaveDirectory, const FString& SlotName)
{
	return CampaignSaveStorePrivate::LoadInternal(SaveDirectory, SlotName, nullptr);
}

FCampaignSaveStoreResult FCampaignSaveStore::Load(
	const FString& SaveDirectory,
	const FString& SlotName,
	const TArray<FCampaignContentVersion>& ExpectedContentPackages)
{
	return CampaignSaveStorePrivate::LoadInternal(SaveDirectory, SlotName, &ExpectedContentPackages);
}

FCampaignSaveSlotListResult FCampaignSaveStore::List(const FString& SaveDirectory)
{
	return CampaignSaveStorePrivate::ListInternal(SaveDirectory, nullptr);
}

FCampaignSaveSlotListResult FCampaignSaveStore::List(
	const FString& SaveDirectory,
	const TArray<FCampaignContentVersion>& ExpectedContentPackages)
{
	return CampaignSaveStorePrivate::ListInternal(SaveDirectory, &ExpectedContentPackages);
}
