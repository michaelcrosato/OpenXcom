// Copyright 2026 UEGT contributors. MIT License.

#include "Content/ContentPackageCatalog.h"

#include "Content/ContentPackageJson.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace ContentPackageCatalogPrivate
{
	void AddError(FContentCatalogLoadResult& Result, const FName Code, FString Message)
	{
		FContentDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EContentDiagnosticSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool HasErrors(const FContentCatalogLoadResult& Result)
	{
		return Result.Diagnostics.ContainsByPredicate(
			[](const FContentDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == EContentDiagnosticSeverity::Error;
			});
	}

	bool ContainsSamePath(const TArray<FString>& Paths, const FString& Candidate)
	{
		return Paths.ContainsByPredicate(
			[&Candidate](const FString& Existing)
			{
				return FPaths::IsSamePath(Existing, Candidate);
			});
	}
}

bool FContentCatalogLoadResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FContentDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FContentCatalogLoadResult FContentPackageCatalog::LoadDirectory(const FString& Directory)
{
	return LoadDirectories({ Directory });
}

FContentCatalogLoadResult FContentPackageCatalog::LoadDirectories(const TArray<FString>& Directories)
{
	using namespace ContentPackageCatalogPrivate;

	FContentCatalogLoadResult Result;
	IFileManager& FileManager = IFileManager::Get();
	if (Directories.IsEmpty())
	{
		AddError(Result, TEXT("no_content_packages"), TEXT("No content package directories were provided."));
		return Result;
	}

	TArray<FString> Files;
	TArray<FString> NormalizedDirectories;
	for (const FString& Directory : Directories)
	{
		const FString TrimmedDirectory = Directory.TrimStartAndEnd();
		if (TrimmedDirectory.IsEmpty())
		{
			AddError(Result, TEXT("content_directory_missing"), TEXT("Content package directory path is empty."));
			continue;
		}
		FString NormalizedDirectory = FPaths::ConvertRelativePathToFull(TrimmedDirectory);
		FPaths::NormalizeDirectoryName(NormalizedDirectory);
		FPaths::CollapseRelativeDirectories(NormalizedDirectory);
		if (!FileManager.DirectoryExists(*NormalizedDirectory))
		{
			AddError(Result, TEXT("content_directory_missing"), FString::Printf(
				TEXT("Content package directory '%s' does not exist."), *NormalizedDirectory));
			continue;
		}
		if (ContainsSamePath(NormalizedDirectories, NormalizedDirectory))
		{
			continue;
		}
		NormalizedDirectories.Add(NormalizedDirectory);

		TArray<FString> DirectoryFiles;
		FileManager.FindFilesRecursive(DirectoryFiles, *NormalizedDirectory, TEXT("*.json"), true, false);
		for (FString& File : DirectoryFiles)
		{
			File = FPaths::ConvertRelativePathToFull(File);
			FPaths::NormalizeFilename(File);
			FPaths::CollapseRelativeDirectories(File);
			if (!ContainsSamePath(Files, File))
			{
				Files.Add(MoveTemp(File));
			}
		}
	}
	if (HasErrors(Result))
	{
		return Result;
	}
	Files.Sort();
	if (Files.IsEmpty())
	{
		AddError(Result, TEXT("no_content_packages"), TEXT("Content package directories contain no JSON packages."));
		return Result;
	}

	TArray<FContentPackage> ParsedPackages;
	for (const FString& File : Files)
	{
		const FContentPackageParseResult Parse = FContentPackageJson::ParseFile(File);
		Result.Diagnostics.Append(Parse.Diagnostics);
		if (Parse.bSucceeded)
		{
			ParsedPackages.Add(Parse.Package);
			Result.LoadedFiles.Add(File);
		}
	}
	if (HasErrors(Result))
	{
		Result.LoadedFiles.Reset();
		return Result;
	}

	const FRuleSetBuildResult Build = FRuleSetBuilder::Build(ParsedPackages);
	Result.Diagnostics.Append(Build.Diagnostics);
	if (!Build.bSucceeded)
	{
		Result.LoadedFiles.Reset();
		return Result;
	}

	TMap<FName, FContentPackage> PackagesById;
	for (FContentPackage& Package : ParsedPackages)
	{
		PackagesById.Add(Package.Descriptor.PackageId, MoveTemp(Package));
	}
	for (const FName PackageId : Build.PackageLoadOrder)
	{
		Result.Packages.Add(MoveTemp(PackagesById.FindChecked(PackageId)));
	}
	Result.RuleSet = Build.RuleSet;
	Result.bSucceeded = true;
	return Result;
}
