// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Content/ContentPackageResolver.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

namespace ContentPackageResolverTests
{
	FContentPackageDescriptor MakePackage(const TCHAR* PackageId, const int32 Priority = 0)
	{
		FContentPackageDescriptor Package;
		Package.PackageId = PackageId;
		Package.DisplayName = PackageId;
		Package.Version = TEXT("1.0.0");
		Package.SchemaVersion = FContentPackageResolver::CurrentSchemaVersion;
		Package.Priority = Priority;
		return Package;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentPackageOrderTest,
	"UEGT.Core.Content.StableLoadOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentPackageOrderTest::RunTest(const FString& Parameters)
{
	using namespace ContentPackageResolverTests;

	FContentPackageDescriptor Core = MakePackage(TEXT("uegt.core"), 100);
	FContentPackageDescriptor Visuals = MakePackage(TEXT("mod.visuals"), -50);
	Visuals.LoadAfter.Add(Core.PackageId);
	FContentPackageDescriptor Balance = MakePackage(TEXT("mod.balance"), 10);
	Balance.Dependencies.Add(Core.PackageId);
	FContentPackageDescriptor Campaign = MakePackage(TEXT("campaign.first-contact"), -100);
	Campaign.Dependencies = { Visuals.PackageId, Balance.PackageId };

	const TArray<FContentPackageDescriptor> Scrambled = { Campaign, Balance, Core, Visuals };
	const FContentResolution First = FContentPackageResolver::Resolve(Scrambled);
	TestTrue(TEXT("Valid package graph resolves"), First.bSucceeded);

	const TArray<FName> Expected = {
		Core.PackageId,
		Visuals.PackageId,
		Balance.PackageId,
		Campaign.PackageId
	};
	TestTrue(TEXT("Dependency edges override raw priority"), First.LoadOrder == Expected);

	TArray<FContentPackageDescriptor> Reversed = Scrambled;
	Algo::Reverse(Reversed);
	const FContentResolution Second = FContentPackageResolver::Resolve(Reversed);
	TestTrue(TEXT("Input enumeration does not affect load order"), Second.LoadOrder == First.LoadOrder);

	FContentPackageDescriptor Optional = MakePackage(TEXT("mod.optional"));
	Optional.LoadAfter.Add(TEXT("mod.not-installed"));
	const FContentResolution OptionalResolution = FContentPackageResolver::Resolve({ Optional });
	TestTrue(TEXT("Absent optional ordering target is ignored"), OptionalResolution.bSucceeded);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentPackageMissingDependencyTest,
	"UEGT.Core.Content.MissingDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentPackageMissingDependencyTest::RunTest(const FString& Parameters)
{
	using namespace ContentPackageResolverTests;

	FContentPackageDescriptor Package = MakePackage(TEXT("campaign.first-contact"));
	Package.Dependencies.Add(TEXT("rules.not-installed"));
	const FContentResolution Resolution = FContentPackageResolver::Resolve({ Package });

	TestFalse(TEXT("Missing required dependency fails resolution"), Resolution.bSucceeded);
	TestTrue(TEXT("Missing dependency has a machine-readable diagnostic"), Resolution.HasDiagnostic(TEXT("missing_dependency")));
	TestEqual(TEXT("Failed resolution exposes no partial order"), Resolution.LoadOrder.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentPackageCycleTest,
	"UEGT.Core.Content.DependencyCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentPackageCycleTest::RunTest(const FString& Parameters)
{
	using namespace ContentPackageResolverTests;

	FContentPackageDescriptor Alpha = MakePackage(TEXT("mod.alpha"));
	FContentPackageDescriptor Beta = MakePackage(TEXT("mod.beta"));
	FContentPackageDescriptor Gamma = MakePackage(TEXT("mod.gamma"));
	Alpha.Dependencies.Add(Gamma.PackageId);
	Beta.Dependencies.Add(Alpha.PackageId);
	Gamma.Dependencies.Add(Beta.PackageId);

	const FContentResolution Resolution = FContentPackageResolver::Resolve({ Gamma, Alpha, Beta });
	TestFalse(TEXT("Dependency cycle fails resolution"), Resolution.bSucceeded);
	TestTrue(TEXT("Cycle has a machine-readable diagnostic"), Resolution.HasDiagnostic(TEXT("dependency_cycle")));
	TestEqual(TEXT("Cyclic graph exposes no partial order"), Resolution.LoadOrder.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentPackageValidationTest,
	"UEGT.Core.Content.ManifestValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentPackageValidationTest::RunTest(const FString& Parameters)
{
	using namespace ContentPackageResolverTests;

	FContentPackageDescriptor Valid = MakePackage(TEXT("mod.valid-id_2"));
	TestTrue(TEXT("Namespaced lowercase package id is valid"), FContentPackageResolver::IsValidPackageId(Valid.PackageId));
	TestFalse(TEXT("Uppercase package id is rejected"), FContentPackageResolver::IsValidPackageId(TEXT("Mod.Invalid")));
	TestFalse(TEXT("Package id must begin with a letter"), FContentPackageResolver::IsValidPackageId(TEXT("1mod.invalid")));

	FContentPackageDescriptor Duplicate = Valid;
	const FContentResolution DuplicateResolution = FContentPackageResolver::Resolve({ Valid, Duplicate });
	TestFalse(TEXT("Duplicate package id fails resolution"), DuplicateResolution.bSucceeded);
	TestTrue(TEXT("Duplicate package id has a diagnostic"), DuplicateResolution.HasDiagnostic(TEXT("duplicate_package_id")));

	FContentPackageDescriptor Unsupported = MakePackage(TEXT("mod.future"));
	Unsupported.SchemaVersion = FContentPackageResolver::CurrentSchemaVersion + 1;
	const FContentResolution UnsupportedResolution = FContentPackageResolver::Resolve({ Unsupported });
	TestFalse(TEXT("Unknown schema version fails resolution"), UnsupportedResolution.bSucceeded);
	TestTrue(TEXT("Unknown schema version has a diagnostic"), UnsupportedResolution.HasDiagnostic(TEXT("unsupported_schema_version")));

	return true;
}

#endif
