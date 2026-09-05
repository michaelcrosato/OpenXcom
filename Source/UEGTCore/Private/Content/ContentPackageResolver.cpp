// Copyright 2026 UEGT contributors. MIT License.

#include "Content/ContentPackageResolver.h"

namespace ContentPackageResolverPrivate
{
	struct FNode
	{
		FContentPackageDescriptor Descriptor;
		TSet<FName> Prerequisites;
		TArray<FName> Dependents;
		int32 RemainingPrerequisites = 0;
	};

	void AddError(FContentResolution& Resolution, const FName Code, const FName PackageId, FString Message)
	{
		FContentDiagnostic& Diagnostic = Resolution.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EContentDiagnosticSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.PackageId = PackageId;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool HasErrors(const FContentResolution& Resolution)
	{
		return Resolution.Diagnostics.ContainsByPredicate(
			[](const FContentDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == EContentDiagnosticSeverity::Error;
			});
	}
}

bool FContentResolution::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FContentDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == Code;
		});
}

bool FContentPackageResolver::IsValidPackageId(const FName PackageId)
{
	return !PackageId.IsNone() && IsValidPackageIdText(PackageId.ToString());
}

bool FContentPackageResolver::IsValidPackageIdText(const FString& Value)
{
	if (Value.Len() < 3 || Value.Len() > 64 || Value[0] < TEXT('a') || Value[0] > TEXT('z')
		|| Value.Equals(TEXT("none"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	for (const TCHAR Character : Value)
	{
		const bool bLowercaseLetter = Character >= TEXT('a') && Character <= TEXT('z');
		const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
		const bool bSeparator = Character == TEXT('.') || Character == TEXT('_') || Character == TEXT('-');
		if (!bLowercaseLetter && !bDigit && !bSeparator)
		{
			return false;
		}
	}

	return true;
}

FContentResolution FContentPackageResolver::Resolve(const TArray<FContentPackageDescriptor>& Packages)
{
	using namespace ContentPackageResolverPrivate;

	FContentResolution Resolution;
	TArray<FContentPackageDescriptor> SortedPackages = Packages;
	SortedPackages.Sort(
		[](const FContentPackageDescriptor& Left, const FContentPackageDescriptor& Right)
		{
			return Left.PackageId.LexicalLess(Right.PackageId);
		});

	TMap<FName, FNode> Nodes;
	for (const FContentPackageDescriptor& Package : SortedPackages)
	{
		if (!IsValidPackageId(Package.PackageId))
		{
			AddError(Resolution, TEXT("invalid_package_id"), Package.PackageId, FString::Printf(TEXT("Package id '%s' must be 3-64 lowercase ASCII characters and start with a letter."), *Package.PackageId.ToString()));
			continue;
		}
		if (Package.Version.TrimStartAndEnd().IsEmpty())
		{
			AddError(Resolution, TEXT("invalid_package_version"), Package.PackageId, TEXT("Package version cannot be empty."));
		}
		if (Package.SchemaVersion != CurrentSchemaVersion)
		{
			AddError(Resolution, TEXT("unsupported_schema_version"), Package.PackageId, FString::Printf(TEXT("Schema version %d is unsupported; expected %d."), Package.SchemaVersion, CurrentSchemaVersion));
		}
		if (Nodes.Contains(Package.PackageId))
		{
			AddError(Resolution, TEXT("duplicate_package_id"), Package.PackageId, FString::Printf(TEXT("Package id '%s' is declared more than once."), *Package.PackageId.ToString()));
			continue;
		}

		FNode& Node = Nodes.Add(Package.PackageId);
		Node.Descriptor = Package;
	}

	if (HasErrors(Resolution))
	{
		return Resolution;
	}

	for (const FContentPackageDescriptor& Package : SortedPackages)
	{
		FNode& Node = Nodes.FindChecked(Package.PackageId);
		for (const FName Dependency : Package.Dependencies)
		{
			if (!Nodes.Contains(Dependency))
			{
				AddError(Resolution, TEXT("missing_dependency"), Package.PackageId, FString::Printf(TEXT("Required package '%s' was not provided."), *Dependency.ToString()));
				continue;
			}
			Node.Prerequisites.Add(Dependency);
		}
		for (const FName OptionalPredecessor : Package.LoadAfter)
		{
			if (Nodes.Contains(OptionalPredecessor))
			{
				Node.Prerequisites.Add(OptionalPredecessor);
			}
		}
	}

	if (HasErrors(Resolution))
	{
		return Resolution;
	}

	for (TPair<FName, FNode>& Pair : Nodes)
	{
		Pair.Value.RemainingPrerequisites = Pair.Value.Prerequisites.Num();
		for (const FName Prerequisite : Pair.Value.Prerequisites)
		{
			Nodes.FindChecked(Prerequisite).Dependents.Add(Pair.Key);
		}
	}

	auto SortReady = [&Nodes](TArray<FName>& Ready)
	{
		Ready.Sort(
			[&Nodes](const FName Left, const FName Right)
			{
				const int32 LeftPriority = Nodes.FindChecked(Left).Descriptor.Priority;
				const int32 RightPriority = Nodes.FindChecked(Right).Descriptor.Priority;
				return LeftPriority != RightPriority ? LeftPriority < RightPriority : Left.LexicalLess(Right);
			});
	};

	TArray<FName> Ready;
	for (const TPair<FName, FNode>& Pair : Nodes)
	{
		if (Pair.Value.RemainingPrerequisites == 0)
		{
			Ready.Add(Pair.Key);
		}
	}
	SortReady(Ready);

	while (!Ready.IsEmpty())
	{
		const FName PackageId = Ready[0];
		Ready.RemoveAt(0, EAllowShrinking::No);
		Resolution.LoadOrder.Add(PackageId);

		TArray<FName> Dependents = Nodes.FindChecked(PackageId).Dependents;
		Dependents.Sort(FNameLexicalLess());
		for (const FName DependentId : Dependents)
		{
			FNode& Dependent = Nodes.FindChecked(DependentId);
			--Dependent.RemainingPrerequisites;
			if (Dependent.RemainingPrerequisites == 0)
			{
				Ready.Add(DependentId);
			}
		}
		SortReady(Ready);
	}

	if (Resolution.LoadOrder.Num() != Nodes.Num())
	{
		TArray<FString> CyclePackages;
		for (const TPair<FName, FNode>& Pair : Nodes)
		{
			if (!Resolution.LoadOrder.Contains(Pair.Key))
			{
				CyclePackages.Add(Pair.Key.ToString());
			}
		}
		CyclePackages.Sort();
		AddError(Resolution, TEXT("dependency_cycle"), NAME_None, FString::Printf(TEXT("Dependency cycle involves: %s."), *FString::Join(CyclePackages, TEXT(", "))));
		Resolution.LoadOrder.Reset();
		return Resolution;
	}

	Resolution.bSucceeded = true;
	return Resolution;
}
