// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Content/ContentPackageJson.h"
#include "Content/RuleTypes.h"

#include "Misc/AutomationTest.h"

namespace ContentRulePipelineTests
{
	const FString BasePackageJson = TEXT(R"JSON(
{
  "schemaVersion": 1,
  "packageId": "uegt.base",
  "displayName": "UEGT Base Rules",
  "version": "1.0.0",
  "priority": 0,
  "rules": {
    "research": [
      {
        "id": "research.directed-energy",
        "displayName": "Directed Energy",
        "effort": 240,
        "prerequisites": [],
		"requiredFacilities": ["facility.energy-lab"],
        "unlocks": ["item.pulse-carbine", "facility.energy-lab"]
      }
    ],
    "archiveEntries": [
      {
        "id": "archive.directed-energy-notes",
        "displayName": "Directed Energy Notes",
        "category": "category.engineering",
        "summary": "Validated archive summary.",
        "body": "Validated archive body with a deterministic research gate.",
        "sortOrder": 20,
        "requires": ["research.directed-energy"],
        "relatedEntries": []
      }
    ],
    "items": [
      {
        "id": "item.pulse-carbine",
        "displayName": "Pulse Carbine",
        "category": "weapon",
        "purchaseCost": 48000,
        "sellValue": 21000,
        "mass": 7,
        "power": 42,
		"requires": ["research.directed-energy"]
      }
    ],
    "facilities": [
      {
        "id": "facility.energy-lab",
        "displayName": "Energy Laboratory",
        "buildCost": 750000,
        "buildHours": 480,
        "monthlyMaintenance": 40000,
		"gridWidth": 1,
		"gridHeight": 1,
		"storageCapacity": 800,
		"scientistCapacity": 12,
		"engineerCapacity": 7,
		"maxIntegrity": 320,
		"repairCostPerIntegrity": 1400,
		"repairHoursPerIntegrity": 2,
		"baseDefenseAccuracy": 68,
		"baseDefenseDamage": 90,
        "requires": []
      }
    ],
    "personnelDoctrines": [
      {
        "id": "doctrine.test-focus",
        "displayName": "Test Focus",
        "summary": "A deterministic field doctrine used by content tests.",
        "maxSelections": 3,
        "maxHealthBonus": 0,
        "accuracyBonus": 4,
        "resolveBonus": 0,
        "mobilityBonus": 0,
        "strengthBonus": 0
      }
    ],
    "personnelCommendations": [
      {
        "id": "commendation.test-service",
        "displayName": "Test Service Citation",
        "summary": "A deterministic service threshold used by content tests.",
        "requiredMissions": 2,
        "requiredKills": 1,
        "requiredRank": 1,
        "requiresSuccessfulMission": true
      }
    ]
  }
}
)JSON");

	const FString BalancePackageJson = TEXT(R"JSON(
{
  "schemaVersion": 1,
  "packageId": "mod.balance",
  "displayName": "Balance Pass",
  "version": "1.1.0",
  "priority": -100,
  "dependencies": ["uegt.base"],
  "rules": {
    "items": [
      {
        "id": "item.pulse-carbine",
        "replace": true,
        "displayName": "Pulse Carbine",
        "category": "weapon",
        "purchaseCost": 50000,
        "sellValue": 22000,
        "mass": 7,
        "power": 47,
        "requires": ["research.directed-energy"]
      }
    ]
  }
}
)JSON");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentJsonValidTest,
	"UEGT.Core.Content.Json.ValidTypedPackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentJsonValidTest::RunTest(const FString& Parameters)
{
	using namespace ContentRulePipelineTests;

	const FContentPackageParseResult Result = FContentPackageJson::ParseString(BasePackageJson, TEXT("base-test.json"));
	TestTrue(TEXT("Valid typed package parses"), Result.bSucceeded);
	TestEqual(TEXT("Package id parses"), Result.Package.Descriptor.PackageId, FName(TEXT("uegt.base")));
	TestEqual(TEXT("One item parses"), Result.Package.Items.Num(), 1);
	TestEqual(TEXT("One research topic parses"), Result.Package.Research.Num(), 1);
	TestEqual(TEXT("One knowledge-archive record parses"), Result.Package.ArchiveEntries.Num(), 1);
	TestEqual(TEXT("One facility parses"), Result.Package.Facilities.Num(), 1);
	TestEqual(TEXT("One personnel doctrine parses"), Result.Package.PersonnelDoctrines.Num(), 1);
	TestEqual(TEXT("One personnel commendation parses"), Result.Package.PersonnelCommendations.Num(), 1);
	TestTrue(TEXT("Research parses its operational facility requirement"),
		Result.Package.Research.Num() == 1
		&& Result.Package.Research[0].RequiredFacilityIds.Contains(TEXT("facility.energy-lab")));
	if (Result.Package.ArchiveEntries.Num() == 1)
	{
		TestTrue(TEXT("Knowledge-archive category, order, gate, and body parse exactly"),
			Result.Package.ArchiveEntries[0].CategoryId == FName(TEXT("category.engineering"))
			&& Result.Package.ArchiveEntries[0].SortOrder == 20
			&& Result.Package.ArchiveEntries[0].RequiredResearch.Contains(TEXT("research.directed-energy"))
			&& Result.Package.ArchiveEntries[0].Body.Contains(TEXT("deterministic research gate")));
	}
	if (Result.Package.Items.Num() == 1)
	{
		TestEqual(TEXT("Typed item power parses"), Result.Package.Items[0].Power, 42);
		TestEqual(TEXT("Item research requirement parses"), Result.Package.Items[0].RequiredResearch.Num(), 1);
	}
	if (Result.Package.Facilities.Num() == 1)
	{
		TestTrue(TEXT("Typed facility durability and repair fields parse exactly"),
			Result.Package.Facilities[0].StorageCapacity == 800
			&& Result.Package.Facilities[0].ScientistCapacity == 12
			&& Result.Package.Facilities[0].EngineerCapacity == 7
			&& Result.Package.Facilities[0].MaxIntegrity == 320
			&& Result.Package.Facilities[0].RepairCostPerIntegrity == 1400
			&& Result.Package.Facilities[0].RepairHoursPerIntegrity == 2
			&& Result.Package.Facilities[0].BaseDefenseAccuracy == 68
			&& Result.Package.Facilities[0].BaseDefenseDamage == 90);
	}
	if (Result.Package.PersonnelDoctrines.Num() == 1)
	{
		TestTrue(TEXT("Doctrine text, cap, and bonuses parse exactly"),
			Result.Package.PersonnelDoctrines[0].Summary.Contains(TEXT("deterministic"))
			&& Result.Package.PersonnelDoctrines[0].MaxSelections == 3
			&& Result.Package.PersonnelDoctrines[0].AccuracyBonus == 4);
	}
	if (Result.Package.PersonnelCommendations.Num() == 1)
	{
		TestTrue(TEXT("Commendation thresholds and success policy parse exactly"),
			Result.Package.PersonnelCommendations[0].RequiredMissions == 2
			&& Result.Package.PersonnelCommendations[0].RequiredKills == 1
			&& Result.Package.PersonnelCommendations[0].RequiredRank == 1
			&& Result.Package.PersonnelCommendations[0].bRequiresSuccessfulMission);
	}
	const FString SignalPackageJson = TEXT(R"JSON(
{
  "schemaVersion": 1,
  "packageId": "example.signal-profiles",
  "displayName": "Signal Profile Test",
  "version": "1.0.0",
  "rules": {
    "items": [
      {
        "id": "item.example-projector",
        "displayName": "Example Projector",
        "category": "sensor",
        "purchaseCost": 100,
        "sellValue": 50,
        "mass": 2,
        "power": 24,
        "tacticalRange": 8,
        "tacticalActionPointCost": 4
      }
    ],
    "tacticalUnits": [
      {
        "id": "unit.example-projector",
        "displayName": "Example Projector Unit",
        "maxHealth": 40,
        "accuracy": 50,
        "resolve": 60,
        "mobility": 50,
        "strength": 40,
        "actionPoints": 8,
        "attackRange": 8,
        "attackPower": 12,
        "attackActionPointCost": 4,
        "signalPower": 28,
        "signalRange": 9,
        "signalActionPointCost": 4,
        "attackDamageType": "arc",
        "kineticArmor": 2,
        "thermalArmor": 2,
        "arcArmor": 4
      }
    ]
  }
}
)JSON");
	const FContentPackageParseResult SignalProfiles = FContentPackageJson::ParseString(
		SignalPackageJson, TEXT("signal-profiles-test.json"));
	TestTrue(TEXT("Complete item and intrinsic signal profiles parse"),
		SignalProfiles.bSucceeded && SignalProfiles.Package.Items.Num() == 1
			&& SignalProfiles.Package.Items[0].IsTacticalSignalProjector()
			&& SignalProfiles.Package.TacticalUnits.Num() == 1
			&& SignalProfiles.Package.TacticalUnits[0].HasSignalProjection());
	const FContentPackageParseResult IncompleteItemSignal = FContentPackageJson::ParseString(
		SignalPackageJson.Replace(TEXT("\"tacticalActionPointCost\": 4"), TEXT("\"tacticalActionPointCost\": 0")),
		TEXT("incomplete-item-signal-test.json"));
	TestTrue(TEXT("Partial item signal profiles reject with a stable diagnostic"),
		!IncompleteItemSignal.bSucceeded && IncompleteItemSignal.HasDiagnostic(TEXT("invalid_field_value")));
	const FContentPackageParseResult IncompleteIntrinsicSignal = FContentPackageJson::ParseString(
		SignalPackageJson.Replace(TEXT("\"signalActionPointCost\": 4"), TEXT("\"signalActionPointCost\": 0")),
		TEXT("incomplete-intrinsic-signal-test.json"));
	TestTrue(TEXT("Partial intrinsic signal profiles reject with a stable diagnostic"),
		!IncompleteIntrinsicSignal.bSucceeded && IncompleteIntrinsicSignal.HasDiagnostic(TEXT("invalid_field_value")));
	const FString InvalidDoctrineCapJson = BasePackageJson.Replace(
		TEXT("\"maxSelections\": 3"), TEXT("\"maxSelections\": 0"));
	const FContentPackageParseResult InvalidDoctrineCap = FContentPackageJson::ParseString(
		InvalidDoctrineCapJson, TEXT("invalid-doctrine-cap-test.json"));
	TestTrue(TEXT("Non-positive doctrine selection caps are rejected with a stable diagnostic"),
		!InvalidDoctrineCap.bSucceeded && InvalidDoctrineCap.HasDiagnostic(TEXT("invalid_field_value")));
	const FString InvalidCommendationThresholdJson = BasePackageJson.Replace(
		TEXT("\"requiredMissions\": 2"), TEXT("\"requiredMissions\": 0"));
	const FContentPackageParseResult InvalidCommendationThreshold = FContentPackageJson::ParseString(
		InvalidCommendationThresholdJson, TEXT("invalid-commendation-threshold-test.json"));
	TestTrue(TEXT("Non-positive commendation mission thresholds are rejected with a stable diagnostic"),
		!InvalidCommendationThreshold.bSucceeded && InvalidCommendationThreshold.HasDiagnostic(TEXT("invalid_field_value")));
	FContentPackage ZeroBonusDoctrine = Result.Package;
	ZeroBonusDoctrine.PersonnelDoctrines[0].AccuracyBonus = 0;
	const FRuleSetBuildResult ZeroBonusDoctrineBuild = FRuleSetBuilder::Build({ ZeroBonusDoctrine });
	TestTrue(TEXT("Doctrine records must provide at least one effective attribute bonus"),
		!ZeroBonusDoctrineBuild.bSucceeded && ZeroBonusDoctrineBuild.HasDiagnostic(TEXT("invalid_rule_value")));
	const FString ZeroFacilityIntegrityJson = BasePackageJson.Replace(
		TEXT("\"maxIntegrity\": 320"), TEXT("\"maxIntegrity\": 0"));
	const FContentPackageParseResult ZeroFacilityIntegrity = FContentPackageJson::ParseString(
		ZeroFacilityIntegrityJson, TEXT("zero-facility-integrity-test.json"));
	TestFalse(TEXT("Non-positive facility integrity is rejected"), ZeroFacilityIntegrity.bSucceeded);
	TestTrue(TEXT("Invalid facility integrity has a stable diagnostic"),
		ZeroFacilityIntegrity.HasDiagnostic(TEXT("invalid_field_value")));
	const FString InvalidFacilityRepairJson = BasePackageJson.Replace(
		TEXT("\"repairHoursPerIntegrity\": 2"), TEXT("\"repairHoursPerIntegrity\": 0"));
	const FContentPackageParseResult InvalidFacilityRepair = FContentPackageJson::ParseString(
		InvalidFacilityRepairJson, TEXT("invalid-facility-repair-test.json"));
	TestFalse(TEXT("Non-positive facility repair duration is rejected"), InvalidFacilityRepair.bSucceeded);
	TestTrue(TEXT("Invalid facility repair duration has a stable diagnostic"),
		InvalidFacilityRepair.HasDiagnostic(TEXT("invalid_field_value")));
	const FString NegativeFacilityRepairCostJson = BasePackageJson.Replace(
		TEXT("\"repairCostPerIntegrity\": 1400"), TEXT("\"repairCostPerIntegrity\": -1"));
	const FContentPackageParseResult NegativeFacilityRepairCost = FContentPackageJson::ParseString(
		NegativeFacilityRepairCostJson, TEXT("negative-facility-repair-cost-test.json"));
	TestFalse(TEXT("Negative facility repair cost is rejected"), NegativeFacilityRepairCost.bSucceeded);
	TestTrue(TEXT("Invalid facility repair cost has a stable diagnostic"),
		NegativeFacilityRepairCost.HasDiagnostic(TEXT("invalid_field_value")));
	const FString NegativeStorageCapacityJson = BasePackageJson.Replace(
		TEXT("\"storageCapacity\": 800"), TEXT("\"storageCapacity\": -1"));
	const FContentPackageParseResult NegativeStorageCapacity = FContentPackageJson::ParseString(
		NegativeStorageCapacityJson, TEXT("negative-storage-capacity-test.json"));
	TestFalse(TEXT("Negative facility storage capacity is rejected"), NegativeStorageCapacity.bSucceeded);
	TestTrue(TEXT("Invalid storage capacity has a stable diagnostic"),
		NegativeStorageCapacity.HasDiagnostic(TEXT("invalid_field_value")));
	const FString ExcessiveStorageCapacityJson = BasePackageJson.Replace(
		TEXT("\"storageCapacity\": 800"), TEXT("\"storageCapacity\": 1000001"));
	const FContentPackageParseResult ExcessiveStorageCapacity = FContentPackageJson::ParseString(
		ExcessiveStorageCapacityJson, TEXT("excessive-storage-capacity-test.json"));
	TestFalse(TEXT("Excessive facility storage capacity is rejected"), ExcessiveStorageCapacity.bSucceeded);
	TestTrue(TEXT("Excessive storage capacity has a stable diagnostic"),
		ExcessiveStorageCapacity.HasDiagnostic(TEXT("invalid_field_value")));
	const FString NegativeScientistCapacityJson = BasePackageJson.Replace(
		TEXT("\"scientistCapacity\": 12"), TEXT("\"scientistCapacity\": -1"));
	const FContentPackageParseResult NegativeScientistCapacity = FContentPackageJson::ParseString(
		NegativeScientistCapacityJson, TEXT("negative-scientist-capacity-test.json"));
	TestFalse(TEXT("Negative facility scientist capacity is rejected"), NegativeScientistCapacity.bSucceeded);
	TestTrue(TEXT("Invalid scientist capacity has a stable diagnostic"),
		NegativeScientistCapacity.HasDiagnostic(TEXT("invalid_field_value")));
	const FString ExcessiveEngineerCapacityJson = BasePackageJson.Replace(
		TEXT("\"engineerCapacity\": 7"), TEXT("\"engineerCapacity\": 1000001"));
	const FContentPackageParseResult ExcessiveEngineerCapacity = FContentPackageJson::ParseString(
		ExcessiveEngineerCapacityJson, TEXT("excessive-engineer-capacity-test.json"));
	TestFalse(TEXT("Excessive facility engineer capacity is rejected"), ExcessiveEngineerCapacity.bSucceeded);
	TestTrue(TEXT("Excessive engineer capacity has a stable diagnostic"),
		ExcessiveEngineerCapacity.HasDiagnostic(TEXT("invalid_field_value")));
	const FString IncompleteBaseDefenseJson = BasePackageJson.Replace(
		TEXT("\"baseDefenseDamage\": 90"), TEXT("\"baseDefenseDamage\": 0"));
	const FContentPackageParseResult IncompleteBaseDefense = FContentPackageJson::ParseString(
		IncompleteBaseDefenseJson, TEXT("incomplete-base-defense-test.json"));
	TestFalse(TEXT("Incomplete facility base-defense profile is rejected"), IncompleteBaseDefense.bSucceeded);
	TestTrue(TEXT("Incomplete base-defense profile has a stable diagnostic"),
		IncompleteBaseDefense.HasDiagnostic(TEXT("invalid_field_value")));
	const FString SuppliedBaseDefenseJson = BasePackageJson.Replace(
		TEXT("\"baseDefenseDamage\": 90,"),
		TEXT("\"baseDefenseDamage\": 90, \"baseDefenseSupplyItemId\": \"item.perimeter-capacitor\", \"baseDefenseSupplyPerShot\": 2,"));
	const FContentPackageParseResult SuppliedBaseDefense = FContentPackageJson::ParseString(
		SuppliedBaseDefenseJson, TEXT("supplied-base-defense-test.json"));
	TestTrue(TEXT("Complete base-defense supply profiles parse"),
		SuppliedBaseDefense.bSucceeded && SuppliedBaseDefense.Package.Facilities.Num() == 1
		&& SuppliedBaseDefense.Package.Facilities[0].BaseDefenseSupplyItemId
			== FName(TEXT("item.perimeter-capacitor"))
		&& SuppliedBaseDefense.Package.Facilities[0].BaseDefenseSupplyPerShot == 2);
	const FContentPackageParseResult IncompleteDefenseSupply = FContentPackageJson::ParseString(
		SuppliedBaseDefenseJson.Replace(TEXT("\"baseDefenseSupplyPerShot\": 2"),
			TEXT("\"baseDefenseSupplyPerShot\": 0")),
		TEXT("incomplete-base-defense-supply-test.json"));
	TestTrue(TEXT("Incomplete base-defense supply profiles reject with a stable diagnostic"),
		!IncompleteDefenseSupply.bSucceeded
		&& IncompleteDefenseSupply.HasDiagnostic(TEXT("invalid_field_value")));
	if (SuppliedBaseDefense.bSucceeded)
	{
		FContentPackage SupplyPackage = SuppliedBaseDefense.Package;
		FItemRule Supply;
		Supply.Identity.RuleId = TEXT("item.perimeter-capacitor");
		Supply.DisplayName = TEXT("Perimeter Capacitor Bank");
		Supply.Category = TEXT("base-defense-supply");
		Supply.Mass = 4;
		Supply.ManufactureCost = 9000;
		Supply.ManufactureHours = 18;
		SupplyPackage.Items.Add(Supply);
		const FRuleSetBuildResult ValidSupplyReference = FRuleSetBuilder::Build({ SupplyPackage });
		TestTrue(TEXT("Defense supply references require and accept the typed supply category"),
			ValidSupplyReference.bSucceeded);
		SupplyPackage.Items.Last().Category = TEXT("ammunition");
		const FRuleSetBuildResult InvalidSupplyReference = FRuleSetBuilder::Build({ SupplyPackage });
		TestTrue(TEXT("Incompatible defense supply references have a stable diagnostic"),
			!InvalidSupplyReference.bSucceeded
			&& InvalidSupplyReference.HasDiagnostic(TEXT("missing_base_defense_supply_reference")));
	}
	const FString BaseAssaultMissionJson = TEXT(R"JSON({
		"schemaVersion": 1,
		"packageId": "mission.test",
		"displayName": "Mission Test",
		"version": "1.0.0",
		"rules": {
		"regions": [{
			"id": "region.test", "displayName": "Test Assembly",
			"centerLongitudeMilliDegrees": -115000, "centerLatitudeMilliDegrees": 43000,
			"initialSupport": 61, "fundingWeight": 40, "pressureTolerance": 47,
			"lowPressureSupportRecovery": 3, "highPressureSupportLossPerTen": 2
		}],
		"adversaryPlans": [{
			"id": "plan.perimeter-test", "displayName": "Perimeter Pattern",
			"openingMissionRuleId": "mission.perimeter-test"
		}],
		"adversaryMissions": [{
			"id": "mission.perimeter-test", "displayName": "Perimeter Test",
			"planId": "plan.perimeter-test", "planStage": 2,
			"escapeBranchMissionRuleId": "mission.escape-test",
			"thwartBranchMissionRuleId": "mission.thwart-test",
			"contactRuleId": "contact.test", "targetRegionId": "region.test",
			"targetsPlayerBase": true,
			"originLongitudeMilliDegrees": -120000, "originLatitudeMilliDegrees": 40000,
			"destinationLongitudeMilliDegrees": -110000, "destinationLatitudeMilliDegrees": 45000,
			"intervalHours": 24, "minimumEscalation": 2, "selectionWeight": 3,
			"pressureOnEscape": 10, "pressureReductionOnDestroyed": 4,
			"scorePenaltyOnEscape": 50, "fundingPenaltyOnEscape": 10000,
			"supportLossOnEscape": 7, "supportGainOnThwarted": 4,
			"compactPeerSupportLossOnEscape": 5,
			"withdrawnCompactSupportGainOnThwarted": 9,
			"baseFacilityDamage": 45, "baseFacilitiesHit": 2
		}] }
	})JSON");
	const FContentPackageParseResult BaseAssaultMission = FContentPackageJson::ParseString(
		BaseAssaultMissionJson, TEXT("base-assault-mission-test.json"));
	TestTrue(TEXT("Complete base-targeting mission profile parses"), BaseAssaultMission.bSucceeded);
	TestTrue(TEXT("Base-targeting mission fields remain typed and exact"),
		BaseAssaultMission.Package.AdversaryPlans.Num() == 1
		&& BaseAssaultMission.Package.AdversaryPlans[0].OpeningMissionRuleId == FName(TEXT("mission.perimeter-test"))
		&& BaseAssaultMission.Package.Regions.Num() == 1
		&& BaseAssaultMission.Package.Regions[0].Identity.RuleId == FName(TEXT("region.test"))
		&& BaseAssaultMission.Package.Regions[0].InitialSupport == 61
		&& BaseAssaultMission.Package.Regions[0].FundingWeight == 40
		&& BaseAssaultMission.Package.Regions[0].PressureTolerance == 47
		&& BaseAssaultMission.Package.AdversaryMissions.Num() == 1
		&& BaseAssaultMission.Package.AdversaryMissions[0].bTargetsPlayerBase
		&& BaseAssaultMission.Package.AdversaryMissions[0].PlanId == FName(TEXT("plan.perimeter-test"))
		&& BaseAssaultMission.Package.AdversaryMissions[0].PlanStage == 2
		&& BaseAssaultMission.Package.AdversaryMissions[0].EscapeBranchMissionRuleId == FName(TEXT("mission.escape-test"))
		&& BaseAssaultMission.Package.AdversaryMissions[0].ThwartBranchMissionRuleId == FName(TEXT("mission.thwart-test"))
		&& BaseAssaultMission.Package.AdversaryMissions[0].SupportLossOnEscape == 7
		&& BaseAssaultMission.Package.AdversaryMissions[0].SupportGainOnThwarted == 4
		&& BaseAssaultMission.Package.AdversaryMissions[0].CompactPeerSupportLossOnEscape == 5
		&& BaseAssaultMission.Package.AdversaryMissions[0].WithdrawnCompactSupportGainOnThwarted == 9
		&& BaseAssaultMission.Package.AdversaryMissions[0].BaseFacilityDamage == 45
		&& BaseAssaultMission.Package.AdversaryMissions[0].BaseFacilitiesHit == 2);
	const FContentPackageParseResult InvalidRegionSupport = FContentPackageJson::ParseString(
		BaseAssaultMissionJson.Replace(TEXT("\"initialSupport\": 61"), TEXT("\"initialSupport\": 101")),
		TEXT("invalid-region-support-test.json"));
	TestFalse(TEXT("Out-of-range initial mandate support is rejected"), InvalidRegionSupport.bSucceeded);
	TestTrue(TEXT("Invalid region support has a stable parser diagnostic"),
		InvalidRegionSupport.HasDiagnostic(TEXT("invalid_field_value")));
	const FContentPackageParseResult InvalidMissionSupport = FContentPackageJson::ParseString(
		BaseAssaultMissionJson.Replace(TEXT("\"supportLossOnEscape\": 7"), TEXT("\"supportLossOnEscape\": 101")),
		TEXT("invalid-mission-support-test.json"));
	TestFalse(TEXT("Out-of-range mission support effects are rejected"), InvalidMissionSupport.bSucceeded);
	TestTrue(TEXT("Invalid mission support effect has a stable parser diagnostic"),
		InvalidMissionSupport.HasDiagnostic(TEXT("invalid_field_value")));
	const FContentPackageParseResult InvalidCompactCounterplay = FContentPackageJson::ParseString(
		BaseAssaultMissionJson.Replace(
			TEXT("\"compactPeerSupportLossOnEscape\": 5"),
			TEXT("\"compactPeerSupportLossOnEscape\": 101")),
		TEXT("invalid-compact-counterplay-test.json"));
	TestFalse(TEXT("Out-of-range compact counterplay is rejected"),
		InvalidCompactCounterplay.bSucceeded);
	TestTrue(TEXT("Invalid compact counterplay has a stable parser diagnostic"),
		InvalidCompactCounterplay.HasDiagnostic(TEXT("invalid_field_value")));
	const FContentPackageParseResult InvalidPlanStage = FContentPackageJson::ParseString(
		BaseAssaultMissionJson.Replace(TEXT("\"planStage\": 2"), TEXT("\"planStage\": 0")),
		TEXT("invalid-adversary-plan-stage-test.json"));
	TestFalse(TEXT("A named plan cannot use standalone stage zero"), InvalidPlanStage.bSucceeded);
	TestTrue(TEXT("Invalid plan metadata has a stable diagnostic"),
		InvalidPlanStage.HasDiagnostic(TEXT("invalid_field_value")));
	const FContentPackageParseResult IncompleteBaseAssaultMission = FContentPackageJson::ParseString(
		BaseAssaultMissionJson.Replace(TEXT("\"baseFacilitiesHit\": 2"), TEXT("\"baseFacilitiesHit\": 0")),
		TEXT("incomplete-base-assault-mission-test.json"));
	TestFalse(TEXT("Incomplete base-assault breach profile is rejected"), IncompleteBaseAssaultMission.bSucceeded);
	TestTrue(TEXT("Incomplete breach profile has a stable diagnostic"),
		IncompleteBaseAssaultMission.HasDiagnostic(TEXT("invalid_field_value")));

	const FString LandingMissionJson = TEXT(R"JSON({
		"schemaVersion": 1,
		"packageId": "landing.test",
		"displayName": "Landing Test",
		"version": "1.0.0",
		"rules": { "adversaryMissions": [{
			"id": "mission.landing-test", "displayName": "Landing Test",
			"contactRuleId": "contact.test", "targetRegionId": "region.test",
			"originLongitudeMilliDegrees": -120000, "originLatitudeMilliDegrees": 40000,
			"destinationLongitudeMilliDegrees": -110000, "destinationLatitudeMilliDegrees": 45000,
			"intervalHours": 24, "minimumEscalation": 1, "selectionWeight": 3,
			"pressureOnEscape": 10, "pressureReductionOnDestroyed": 4,
			"scorePenaltyOnEscape": 50, "fundingPenaltyOnEscape": 10000,
			"createsLandingSiteOnArrival": true,
			"landingSiteLifetimeHours": 36, "landingSiteThreatBonus": 2
		}] }
	})JSON");
	const FContentPackageParseResult LandingMission = FContentPackageJson::ParseString(
		LandingMissionJson, TEXT("landing-mission-test.json"));
	TestTrue(TEXT("Complete intact-landing mission profile parses"), LandingMission.bSucceeded);
	TestTrue(TEXT("Landing outcome fields remain typed and exact"),
		LandingMission.Package.AdversaryMissions.Num() == 1
		&& LandingMission.Package.AdversaryMissions[0].bCreatesLandingSiteOnArrival
		&& LandingMission.Package.AdversaryMissions[0].LandingSiteLifetimeHours == 36
		&& LandingMission.Package.AdversaryMissions[0].LandingSiteThreatBonus == 2);
	const FContentPackageParseResult IncompleteLandingMission = FContentPackageJson::ParseString(
		LandingMissionJson.Replace(TEXT("\"landingSiteThreatBonus\": 2"), TEXT("\"landingSiteThreatBonus\": 0")),
		TEXT("incomplete-landing-mission-test.json"));
	TestFalse(TEXT("Landing missions require a positive intact-site threat premium"), IncompleteLandingMission.bSucceeded);
	TestTrue(TEXT("Incomplete landing profile has a stable diagnostic"),
		IncompleteLandingMission.HasDiagnostic(TEXT("invalid_field_value")));

	const FString ManufacturingRecipeJson = BasePackageJson.Replace(
		TEXT("\"power\": 42,"),
		TEXT("\"power\": 42, \"manufactureCost\": 1000, \"manufactureHours\": 10, \"manufactureInputs\": [{\"itemId\": \"item.recovered-alloy\", \"quantity\": 2}],"));
	const FContentPackageParseResult ManufacturingRecipe = FContentPackageJson::ParseString(
		ManufacturingRecipeJson, TEXT("manufacturing-recipe-test.json"));
	TestTrue(TEXT("Typed manufacturing inputs parse"), ManufacturingRecipe.bSucceeded);
	if (ManufacturingRecipe.Package.Items.Num() == 1)
	{
		TestTrue(TEXT("Manufacturing recipe retains exact item and quantity"),
			ManufacturingRecipe.Package.Items[0].ManufactureInputs.Num() == 1
			&& ManufacturingRecipe.Package.Items[0].ManufactureInputs[0].ItemId == FName(TEXT("item.recovered-alloy"))
			&& ManufacturingRecipe.Package.Items[0].ManufactureInputs[0].Quantity == 2);
	}
	const FString DuplicateManufacturingInputJson = ManufacturingRecipeJson.Replace(
		TEXT("{\"itemId\": \"item.recovered-alloy\", \"quantity\": 2}"),
		TEXT("{\"itemId\": \"item.recovered-alloy\", \"quantity\": 2}, {\"itemId\": \"item.recovered-alloy\", \"quantity\": 1}"));
	const FContentPackageParseResult DuplicateManufacturingInput = FContentPackageJson::ParseString(
		DuplicateManufacturingInputJson, TEXT("duplicate-manufacturing-input-test.json"));
	TestFalse(TEXT("Duplicate manufacturing inputs are rejected"), DuplicateManufacturingInput.bSucceeded);
	TestTrue(TEXT("Duplicate manufacturing input has a stable diagnostic"),
		DuplicateManufacturingInput.HasDiagnostic(TEXT("duplicate_manufacturing_input")));
	const FString ZeroManufacturingInputJson = ManufacturingRecipeJson.Replace(
		TEXT("\"quantity\": 2"), TEXT("\"quantity\": 0"));
	const FContentPackageParseResult ZeroManufacturingInput = FContentPackageJson::ParseString(
		ZeroManufacturingInputJson, TEXT("zero-manufacturing-input-test.json"));
	TestFalse(TEXT("Non-positive manufacturing input quantities are rejected"), ZeroManufacturingInput.bSucceeded);
	TestTrue(TEXT("Non-positive manufacturing input quantity has a stable diagnostic"),
		ZeroManufacturingInput.HasDiagnostic(TEXT("invalid_field_value")));

	const FString FireModeJson = BasePackageJson.Replace(
		TEXT("\"power\": 42,"),
		TEXT("\"power\": 42, \"tacticalRange\": 12, \"tacticalActionPointCost\": 4, \"tacticalBurstShotCount\": 3, \"tacticalBurstActionPointCost\": 6, \"tacticalBurstAccuracyModifier\": -12,"));
	const FContentPackageParseResult FireModeResult = FContentPackageJson::ParseString(FireModeJson, TEXT("fire-mode-test.json"));
	TestTrue(TEXT("Complete tactical burst profiles parse"), FireModeResult.bSucceeded);
	if (FireModeResult.Package.Items.Num() == 1)
	{
		TestTrue(TEXT("Parsed tactical burst profile is typed"), FireModeResult.Package.Items[0].HasTacticalBurstMode());
		TestEqual(TEXT("Parsed tactical burst shot count is exact"), FireModeResult.Package.Items[0].TacticalBurstShotCount, 3);
	}
	const FString IncompatibleBlastBurst = FireModeJson.Replace(
		TEXT("\"tacticalBurstAccuracyModifier\": -12,"),
		TEXT("\"tacticalBurstAccuracyModifier\": -12, \"tacticalBlastRadius\": 2, \"tacticalBlastFalloffPercent\": 30, \"tacticalTerrainDamagePercent\": 150,"));
	const FContentPackageParseResult IncompatibleResult = FContentPackageJson::ParseString(
		IncompatibleBlastBurst,
		TEXT("incompatible-fire-mode-test.json"));
	TestFalse(TEXT("Weapons cannot combine burst and ground-blast profiles"), IncompatibleResult.bSucceeded);
	TestTrue(TEXT("Incompatible tactical profiles have a stable diagnostic"), IncompatibleResult.HasDiagnostic(TEXT("invalid_field_value")));

	const FString SupportDeviceJson = BasePackageJson
		.Replace(TEXT("\"category\": \"weapon\""), TEXT("\"category\": \"device\""))
		.Replace(
			TEXT("\"power\": 42,"),
			TEXT("\"power\": 0, \"tacticalRange\": 10, \"tacticalActionPointCost\": 3, \"tacticalRadius\": 2, \"tacticalThrowArcHeight\": 4, \"tacticalSmokeReduction\": 55, \"tacticalFireReduction\": 75, \"tacticalSuppressionReduction\": 25, \"tacticalMoraleRecovery\": 15,"));
	const FContentPackageParseResult SupportDeviceResult = FContentPackageJson::ParseString(
		SupportDeviceJson,
		TEXT("support-device-test.json"));
	TestTrue(TEXT("Complete tactical support-device profiles parse"), SupportDeviceResult.bSucceeded);
	if (SupportDeviceResult.Package.Items.Num() == 1)
	{
		TestTrue(TEXT("Parsed support-device profile is typed"), SupportDeviceResult.Package.Items[0].IsTacticalDevice());
		TestTrue(TEXT("Parsed support device exposes a throw arc"), SupportDeviceResult.Package.Items[0].HasTacticalThrowArc());
		TestEqual(TEXT("Parsed throw arc height is exact"), SupportDeviceResult.Package.Items[0].TacticalThrowArcHeight, 4);
		TestEqual(TEXT("Parsed support fire reduction is exact"), SupportDeviceResult.Package.Items[0].TacticalFireReduction, 75);
		TestEqual(TEXT("Parsed support morale recovery is exact"), SupportDeviceResult.Package.Items[0].TacticalMoraleRecovery, 15);
	}
	const FString ContradictorySupportDeviceJson = SupportDeviceJson.Replace(
		TEXT("\"tacticalSmokeReduction\": 55"),
		TEXT("\"tacticalSmoke\": 10, \"tacticalSmokeReduction\": 55"));
	const FContentPackageParseResult ContradictorySupportDeviceResult = FContentPackageJson::ParseString(
		ContradictorySupportDeviceJson,
		TEXT("contradictory-support-device-test.json"));
	TestFalse(TEXT("Devices cannot add and remove the same environmental field"), ContradictorySupportDeviceResult.bSucceeded);
	TestTrue(TEXT("Contradictory support profiles have a stable diagnostic"), ContradictorySupportDeviceResult.HasDiagnostic(TEXT("invalid_field_value")));

	const FString UnknownFieldJson = BasePackageJson.Replace(TEXT("\"priority\": 0,"), TEXT("\"priority\": 0, \"authorNote\": \"ignored metadata\","));
	const FContentPackageParseResult WarningResult = FContentPackageJson::ParseString(UnknownFieldJson, TEXT("warning-test.json"));
	TestTrue(TEXT("Unknown field is non-fatal"), WarningResult.bSucceeded);
	TestTrue(TEXT("Unknown field emits a diagnostic"), WarningResult.HasDiagnostic(TEXT("unknown_field")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentJsonInvalidTest,
	"UEGT.Core.Content.Json.StrictDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentJsonInvalidTest::RunTest(const FString& Parameters)
{
	const FContentPackageParseResult Malformed = FContentPackageJson::ParseString(TEXT("{ not-json"), TEXT("malformed.json"));
	TestFalse(TEXT("Malformed JSON fails"), Malformed.bSucceeded);
	TestTrue(TEXT("Malformed JSON has a diagnostic"), Malformed.HasDiagnostic(TEXT("invalid_json")));

	const FString WrongTypes = TEXT(R"JSON(
{
  "schemaVersion": 1.5,
  "packageId": "Bad.Id",
  "displayName": "",
  "version": "1.0.0",
  "rules": { "items": "not-an-array" }
}
)JSON");
	const FContentPackageParseResult Invalid = FContentPackageJson::ParseString(WrongTypes, TEXT("invalid.json"));
	TestFalse(TEXT("Invalid manifest fails"), Invalid.bSucceeded);
	TestTrue(TEXT("Fractional schema is rejected"), Invalid.HasDiagnostic(TEXT("invalid_field_type")));
	TestTrue(TEXT("Invalid package id is rejected"), Invalid.HasDiagnostic(TEXT("invalid_package_id")));
	TestTrue(TEXT("Empty required string is rejected"), Invalid.HasDiagnostic(TEXT("invalid_field_value")));

	const FString FutureSchema = TEXT(R"JSON({"schemaVersion":2,"packageId":"mod.future","displayName":"Future","version":"1.0.0"})JSON");
	const FContentPackageParseResult Future = FContentPackageJson::ParseString(FutureSchema, TEXT("future.json"));
	TestFalse(TEXT("Future schema fails safely"), Future.bSucceeded);
	TestTrue(TEXT("Future schema has a diagnostic"), Future.HasDiagnostic(TEXT("unsupported_schema_version")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentJsonScalarTypesTest,
	"UEGT.Core.Content.Json.ScalarTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentJsonScalarTypesTest::RunTest(const FString& Parameters)
{
	using namespace ContentRulePipelineTests;
	struct FInvalidCase
	{
		const TCHAR* Name;
		const TCHAR* Original;
		const TCHAR* Replacement;
	};
	const FInvalidCase Cases[] = {
		{ TEXT("Quoted schema version"), TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": \"1\"") },
		{ TEXT("Boolean schema version"), TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": true") },
		{ TEXT("Quoted priority"), TEXT("\"priority\": 0"), TEXT("\"priority\": \"0\"") },
		{ TEXT("Boolean priority"), TEXT("\"priority\": 0"), TEXT("\"priority\": false") },
		{ TEXT("Numeric display name"), TEXT("\"displayName\": \"UEGT Base Rules\""), TEXT("\"displayName\": 42") },
		{ TEXT("Boolean display name"), TEXT("\"displayName\": \"UEGT Base Rules\""), TEXT("\"displayName\": true") },
		{ TEXT("Numeric compatibility version"), TEXT("\"version\": \"1.0.0\""), TEXT("\"version\": 42") },
		{ TEXT("Boolean package id"), TEXT("\"packageId\": \"uegt.base\""), TEXT("\"packageId\": true") },
		{ TEXT("Boolean id array entry"), TEXT("\"prerequisites\": []"), TEXT("\"prerequisites\": [true]") },
		{ TEXT("Numeric id array entry"), TEXT("\"prerequisites\": []"), TEXT("\"prerequisites\": [42]") },
		{ TEXT("Quoted rule integer"), TEXT("\"power\": 42"), TEXT("\"power\": \"42\"") },
		{ TEXT("Boolean rule integer"), TEXT("\"power\": 42"), TEXT("\"power\": true") }
	};
	for (const FInvalidCase& Case : Cases)
	{
		const FString Json = BasePackageJson.Replace(Case.Original, Case.Replacement);
		TestNotEqual(FString::Printf(TEXT("%s changes the valid fixture"), Case.Name), Json, BasePackageJson);
		const FContentPackageParseResult Invalid = FContentPackageJson::ParseString(Json, Case.Name);
		TestTrue(FString::Printf(TEXT("%s is rejected as an invalid field type"), Case.Name),
			!Invalid.bSucceeded && Invalid.HasDiagnostic(TEXT("invalid_field_type")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContentJsonTacticalRulesTest,
	"UEGT.Core.Content.Json.TacticalRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentJsonTacticalRulesTest::RunTest(const FString& Parameters)
{
	const FString TacticalJson = TEXT(R"JSON(
{
  "schemaVersion": 1,
  "packageId": "test.tactical",
  "displayName": "Tactical Test Rules",
  "version": "1.0.0",
  "rules": {
    "contacts": [
      {
        "id": "contact.test-skimmer",
        "displayName": "Test Skimmer",
        "signature": 60,
        "cruiseSpeedKilometersPerHour": 800,
        "maxHull": 70,
        "threatRating": 2,
        "scoreValue": 100
      }
    ],
    "tacticalTerrains": [
      {
        "id": "terrain.test-floor",
        "displayName": "Test Floor",
        "moveCost": 1,
        "coverPercent": 0,
        "maxIntegrity": 0,
        "blastResistancePercent": 0,
        "flammability": 80,
        "ventilationPercent": 50,
        "blocksMovement": false,
        "blocksVision": false
      },
      {
        "id": "terrain.test-wall",
        "displayName": "Test Wall",
        "moveCost": 8,
        "coverPercent": 70,
        "maxIntegrity": 100,
        "blastResistancePercent": 50,
        "flammability": 0,
		"ventilationPercent": 0,
		"throwObstacleHeight": 6,
        "blocksMovement": true,
        "blocksVision": true
      },
      {
        "id": "terrain.test-door",
        "displayName": "Test Door",
        "moveCost": 1,
        "coverPercent": 50,
        "maxIntegrity": 60,
        "blastResistancePercent": 40,
        "flammability": 10,
        "ventilationPercent": 100,
        "throwObstacleHeight": 4,
        "doorActionPointCost": 2,
        "blocksMovement": true,
        "blocksVision": true
      },
      {
        "id": "terrain.test-lift",
        "displayName": "Test Lift",
        "moveCost": 1,
        "coverPercent": 0,
        "maxIntegrity": 0,
        "blastResistancePercent": 0,
        "flammability": 40,
        "ventilationPercent": 100,
        "verticalMoveCost": 2,
        "blocksMovement": false,
        "blocksVision": false
      }
    ],
    "tacticalUnits": [
      {
        "id": "unit.test-raider",
        "displayName": "Test Raider",
        "maxHealth": 40,
        "accuracy": 48,
        "resolve": 52,
        "mobility": 54,
        "strength": 46,
        "actionPoints": 8,
        "attackRange": 9,
        "attackPower": 15,
        "attackActionPointCost": 4
      }
    ],
    "tacticalMissions": [
      {
        "id": "tactical.test-recovery",
        "displayName": "Test Recovery",
        "sourceContactRuleId": "contact.test-skimmer",
        "floorTerrainRuleId": "terrain.test-floor",
        "obstacleTerrainRuleId": "terrain.test-wall",
        "doorTerrainRuleId": "terrain.test-door",
        "verticalConnectorTerrainRuleId": "terrain.test-lift",
        "adversaryUnitRuleId": "unit.test-raider",
        "objectiveId": "objective.test-core",
        "objectiveType": "control",
        "objectiveRequiredInteractions": 3,
        "missionExperienceReward": 30,
        "objectiveExperienceReward": 70,
        "mapWidth": 12,
        "mapHeight": 18,
        "mapLevels": 2,
        "deploymentDepth": 2,
        "obstaclePercent": 20,
        "baseEnemyCount": 2,
        "enemiesPerThreat": 1,
        "turnLimit": 24,
        "objectiveActionPointCost": 2,
        "extractionActionPointCost": 1
      }
    ]
  }
}
)JSON");

	const FContentPackageParseResult Parsed = FContentPackageJson::ParseString(TacticalJson, TEXT("tactical.json"));
	TestTrue(TEXT("Typed tactical terrain, unit, and mission rules parse"), Parsed.bSucceeded);
	TestEqual(TEXT("Four tactical terrains parse"), Parsed.Package.TacticalTerrains.Num(), 4);
	TestEqual(TEXT("One tactical unit parses"), Parsed.Package.TacticalUnits.Num(), 1);
	TestEqual(TEXT("One tactical mission parses"), Parsed.Package.TacticalMissions.Num(), 1);
	if (Parsed.bSucceeded)
	{
		TestTrue(TEXT("Tactical terrain blast, fire, ventilation, throw, door, and vertical properties parse"), Parsed.Package.TacticalTerrains.Num() == 4
			&& Parsed.Package.TacticalTerrains[0].Flammability == 80
			&& Parsed.Package.TacticalTerrains[0].VentilationPercent == 50
			&& Parsed.Package.TacticalTerrains[1].BlastResistancePercent == 50
			&& Parsed.Package.TacticalTerrains[1].ThrowObstacleHeight == 6
			&& Parsed.Package.TacticalTerrains[2].IsDoor()
			&& Parsed.Package.TacticalTerrains[2].DoorActionPointCost == 2
			&& Parsed.Package.TacticalTerrains[3].IsVerticalConnector()
			&& Parsed.Package.TacticalTerrains[3].VerticalMoveCost == 2
			&& Parsed.Package.TacticalMissions[0].MapLevels == 2
			&& Parsed.Package.TacticalMissions[0].VerticalConnectorTerrainRuleId == FName(TEXT("terrain.test-lift"))
			&& Parsed.Package.TacticalMissions[0].ObjectiveType == ETacticalObjectiveType::Control
			&& Parsed.Package.TacticalMissions[0].ObjectiveRequiredInteractions == 3
			&& Parsed.Package.TacticalMissions[0].MissionExperienceReward == 30
			&& Parsed.Package.TacticalMissions[0].ObjectiveExperienceReward == 70);
		const FRuleSetBuildResult Built = FRuleSetBuilder::Build({ Parsed.Package });
		TestTrue(TEXT("Tactical rule cross-references resolve"), Built.bSucceeded);
		if (Parsed.Package.TacticalMissions.Num() == 1)
		{
			FContentPackage ExtremeDimensions = Parsed.Package;
			ExtremeDimensions.TacticalMissions[0].MapWidth = MAX_int32;
			ExtremeDimensions.TacticalMissions[0].MapHeight = MAX_int32;
			ExtremeDimensions.TacticalMissions[0].MapLevels = MAX_int32;
			const FRuleSetBuildResult ExtremeBuild = FRuleSetBuilder::Build({ ExtremeDimensions });
			TestFalse(TEXT("Rule building rejects extreme tactical map dimensions without overflowing"), ExtremeBuild.bSucceeded);
			TestTrue(TEXT("Extreme tactical map dimensions have the shared rule-value diagnostic"),
				ExtremeBuild.HasDiagnostic(TEXT("invalid_rule_value")));
		}
	}
	const FString LandingTacticalJson = TacticalJson.Replace(
		TEXT("\"displayName\": \"Test Recovery\","),
		TEXT("\"displayName\": \"Test Recovery\",\n        \"siteType\": \"landing\","));
	const FContentPackageParseResult LandingTactical = FContentPackageJson::ParseString(
		LandingTacticalJson, TEXT("tactical-landing.json"));
	TestTrue(TEXT("Typed intact-landing tactical recipe parses"), LandingTactical.bSucceeded
		&& LandingTactical.Package.TacticalMissions.Num() == 1
		&& LandingTactical.Package.TacticalMissions[0].SiteType == ETacticalSiteType::Landing);
	const FContentPackageParseResult UnknownSiteType = FContentPackageJson::ParseString(
		LandingTacticalJson.Replace(TEXT("\"siteType\": \"landing\""), TEXT("\"siteType\": \"orbit\"")),
		TEXT("tactical-unknown-site-type.json"));
	TestFalse(TEXT("Unknown tactical site types are rejected"), UnknownSiteType.bSucceeded);
	TestTrue(TEXT("Unknown tactical site type is diagnosed"),
		UnknownSiteType.HasDiagnostic(TEXT("invalid_field_value")));

	const FString WrongBoolean = TacticalJson.Replace(TEXT("\"blocksMovement\": false"), TEXT("\"blocksMovement\": \"false\""));
	const FContentPackageParseResult WrongBooleanResult = FContentPackageJson::ParseString(WrongBoolean, TEXT("tactical-wrong-bool.json"));
	TestFalse(TEXT("String tactical booleans are rejected"), WrongBooleanResult.bSucceeded);
	TestTrue(TEXT("Wrong tactical boolean type is diagnosed"), WrongBooleanResult.HasDiagnostic(TEXT("invalid_field_type")));

	const FString TooNarrow = TacticalJson.Replace(TEXT("\"mapWidth\": 12"), TEXT("\"mapWidth\": 7"));
	const FContentPackageParseResult TooNarrowResult = FContentPackageJson::ParseString(TooNarrow, TEXT("tactical-too-narrow.json"));
	TestFalse(TEXT("Unsupported tactical map dimensions are rejected"), TooNarrowResult.bSucceeded);
	TestTrue(TEXT("Unsupported tactical map dimensions are diagnosed"), TooNarrowResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString ExtremeDimensions = TacticalJson
		.Replace(TEXT("\"mapWidth\": 12"), TEXT("\"mapWidth\": 2147483647"))
		.Replace(TEXT("\"mapHeight\": 18"), TEXT("\"mapHeight\": 2147483647"))
		.Replace(TEXT("\"mapLevels\": 2"), TEXT("\"mapLevels\": 2147483647"));
	const FContentPackageParseResult ExtremeDimensionsResult = FContentPackageJson::ParseString(
		ExtremeDimensions, TEXT("tactical-extreme-dimensions.json"));
	TestFalse(TEXT("JSON parsing rejects extreme tactical map dimensions without overflowing"), ExtremeDimensionsResult.bSucceeded);
	TestTrue(TEXT("Extreme JSON tactical map dimensions have a field-value diagnostic"),
		ExtremeDimensionsResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString ExtremeDeploymentDepth = TacticalJson.Replace(
		TEXT("\"deploymentDepth\": 2"), TEXT("\"deploymentDepth\": 2147483647"));
	const FContentPackageParseResult ExtremeDeploymentDepthResult = FContentPackageJson::ParseString(
		ExtremeDeploymentDepth, TEXT("tactical-extreme-deployment-depth.json"));
	TestFalse(TEXT("JSON parsing rejects extreme tactical deployment depth without overflowing"),
		ExtremeDeploymentDepthResult.bSucceeded);
	TestTrue(TEXT("Extreme tactical deployment depth has a field-value diagnostic"),
		ExtremeDeploymentDepthResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString UnknownObjectiveType = TacticalJson.Replace(TEXT("\"objectiveType\": \"control\""), TEXT("\"objectiveType\": \"capture\""));
	const FContentPackageParseResult UnknownObjectiveTypeResult = FContentPackageJson::ParseString(UnknownObjectiveType, TEXT("tactical-unknown-objective.json"));
	TestFalse(TEXT("Unknown tactical objective types are rejected"), UnknownObjectiveTypeResult.bSucceeded);
	TestTrue(TEXT("Unknown tactical objective type is diagnosed"), UnknownObjectiveTypeResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString OverFlammable = TacticalJson.Replace(TEXT("\"flammability\": 80"), TEXT("\"flammability\": 180"));
	const FContentPackageParseResult OverFlammableResult = FContentPackageJson::ParseString(OverFlammable, TEXT("tactical-over-flammable.json"));
	TestFalse(TEXT("Unsupported terrain flammability is rejected"), OverFlammableResult.bSucceeded);
	TestTrue(TEXT("Unsupported terrain flammability is diagnosed"), OverFlammableResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString OverVentilated = TacticalJson.Replace(TEXT("\"ventilationPercent\": 50"), TEXT("\"ventilationPercent\": 150"));
	const FContentPackageParseResult OverVentilatedResult = FContentPackageJson::ParseString(OverVentilated, TEXT("tactical-over-ventilated.json"));
	TestFalse(TEXT("Unsupported terrain ventilation is rejected"), OverVentilatedResult.bSucceeded);
	TestTrue(TEXT("Unsupported terrain ventilation is diagnosed"), OverVentilatedResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString OverheightTerrain = TacticalJson.Replace(TEXT("\"throwObstacleHeight\": 6"), TEXT("\"throwObstacleHeight\": 9"));
	const FContentPackageParseResult OverheightTerrainResult = FContentPackageJson::ParseString(OverheightTerrain, TEXT("tactical-overheight-terrain.json"));
	TestFalse(TEXT("Unsupported throw-obstacle height is rejected"), OverheightTerrainResult.bSucceeded);
	TestTrue(TEXT("Unsupported throw-obstacle height is diagnosed"), OverheightTerrainResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString OvercostDoor = TacticalJson.Replace(TEXT("\"doorActionPointCost\": 2"), TEXT("\"doorActionPointCost\": 5"));
	const FContentPackageParseResult OvercostDoorResult = FContentPackageJson::ParseString(OvercostDoor, TEXT("tactical-overcost-door.json"));
	TestFalse(TEXT("Unsupported door action-point cost is rejected"), OvercostDoorResult.bSucceeded);
	TestTrue(TEXT("Unsupported door action-point cost is diagnosed"), OvercostDoorResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString NonblockingDoor = TacticalJson.Replace(
		TEXT("\"doorActionPointCost\": 2,\n        \"blocksMovement\": true"),
		TEXT("\"doorActionPointCost\": 2,\n        \"blocksMovement\": false"));
	const FContentPackageParseResult NonblockingDoorResult = FContentPackageJson::ParseString(NonblockingDoor, TEXT("tactical-nonblocking-door.json"));
	TestFalse(TEXT("Door terrain must block movement while closed"), NonblockingDoorResult.bSucceeded);
	TestTrue(TEXT("Nonblocking door terrain is diagnosed"), NonblockingDoorResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString OvercostLift = TacticalJson.Replace(TEXT("\"verticalMoveCost\": 2"), TEXT("\"verticalMoveCost\": 21"));
	const FContentPackageParseResult OvercostLiftResult = FContentPackageJson::ParseString(OvercostLift, TEXT("tactical-overcost-lift.json"));
	TestFalse(TEXT("Unsupported vertical action-point cost is rejected"), OvercostLiftResult.bSucceeded);
	TestTrue(TEXT("Unsupported vertical action-point cost is diagnosed"), OvercostLiftResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString TooManyLevels = TacticalJson.Replace(TEXT("\"mapLevels\": 2"), TEXT("\"mapLevels\": 5"));
	const FContentPackageParseResult TooManyLevelsResult = FContentPackageJson::ParseString(TooManyLevels, TEXT("tactical-too-many-levels.json"));
	TestFalse(TEXT("Unsupported tactical level counts are rejected"), TooManyLevelsResult.bSucceeded);
	TestTrue(TEXT("Unsupported tactical level counts are diagnosed"), TooManyLevelsResult.HasDiagnostic(TEXT("invalid_field_value")));
	const FString BlockingLift = TacticalJson.Replace(
		TEXT("\"verticalMoveCost\": 2,\n        \"blocksMovement\": false"),
		TEXT("\"verticalMoveCost\": 2,\n        \"blocksMovement\": true"));
	const FContentPackageParseResult BlockingLiftResult = FContentPackageJson::ParseString(BlockingLift, TEXT("tactical-blocking-lift.json"));
	TestFalse(TEXT("Vertical connectors must remain traversable"), BlockingLiftResult.bSucceeded);
	TestTrue(TEXT("Blocking vertical connector is diagnosed"), BlockingLiftResult.HasDiagnostic(TEXT("invalid_field_value")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRuleSetOverrideTest,
	"UEGT.Core.Content.Rules.ExplicitOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuleSetOverrideTest::RunTest(const FString& Parameters)
{
	using namespace ContentRulePipelineTests;

	const FContentPackageParseResult Base = FContentPackageJson::ParseString(BasePackageJson, TEXT("base.json"));
	const FContentPackageParseResult Balance = FContentPackageJson::ParseString(BalancePackageJson, TEXT("balance.json"));
	TestTrue(TEXT("Base fixture parses"), Base.bSucceeded);
	TestTrue(TEXT("Override fixture parses"), Balance.bSucceeded);

	const FRuleSetBuildResult Result = FRuleSetBuilder::Build({ Balance.Package, Base.Package });
	TestTrue(TEXT("Explicit override builds"), Result.bSucceeded);
	TestEqual(TEXT("Both packages are resolved"), Result.PackageLoadOrder.Num(), 2);
	if (Result.PackageLoadOrder.Num() == 2)
	{
		TestEqual(TEXT("Dependency controls package order"), Result.PackageLoadOrder[0], FName(TEXT("uegt.base")));
	}
	const FItemRule* Item = Result.RuleSet.Items.Find(TEXT("item.pulse-carbine"));
	TestNotNull(TEXT("Resolved item exists"), Item);
	if (Item != nullptr)
	{
		TestEqual(TEXT("Later explicit replacement wins"), Item->Power, 47);
	}
	TestEqual(TEXT("Origin tracks replacing package"), Result.RuleSet.ItemOrigins.FindRef(TEXT("item.pulse-carbine")), FName(TEXT("mod.balance")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRuleSetOverrideSafetyTest,
	"UEGT.Core.Content.Rules.OverrideSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuleSetOverrideSafetyTest::RunTest(const FString& Parameters)
{
	using namespace ContentRulePipelineTests;

	const FContentPackageParseResult Base = FContentPackageJson::ParseString(BasePackageJson, TEXT("base.json"));
	FContentPackage Accidental = FContentPackageJson::ParseString(BalancePackageJson, TEXT("balance.json")).Package;
	Accidental.Items[0].Identity.bReplaceExisting = false;
	const FRuleSetBuildResult AccidentalResult = FRuleSetBuilder::Build({ Base.Package, Accidental });
	TestFalse(TEXT("Implicit override fails"), AccidentalResult.bSucceeded);
	TestTrue(TEXT("Implicit override is diagnosed"), AccidentalResult.HasDiagnostic(TEXT("unexpected_rule_override")));

	FContentPackage MissingTarget = Accidental;
	MissingTarget.Descriptor.PackageId = TEXT("mod.missing-target");
	MissingTarget.Descriptor.Dependencies.Reset();
	MissingTarget.Items[0].Identity.RuleId = TEXT("item.not-defined");
	MissingTarget.Items[0].Identity.bReplaceExisting = true;
	const FRuleSetBuildResult MissingTargetResult = FRuleSetBuilder::Build({ MissingTarget });
	TestFalse(TEXT("Replacement without target fails"), MissingTargetResult.bSucceeded);
	TestTrue(TEXT("Missing replacement target is diagnosed"), MissingTargetResult.HasDiagnostic(TEXT("missing_rule_override_target")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRuleSetReferenceTest,
	"UEGT.Core.Content.Rules.ReferenceAndCycleValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuleSetReferenceTest::RunTest(const FString& Parameters)
{
	FContentPackage Package;
	Package.Descriptor.PackageId = TEXT("test.references");
	Package.Descriptor.DisplayName = TEXT("Reference Tests");
	Package.Descriptor.Version = TEXT("1.0.0");

	FResearchRule Alpha;
	Alpha.Identity.RuleId = TEXT("research.alpha");
	Alpha.DisplayName = TEXT("Alpha");
	Alpha.Effort = 10;
	Alpha.Prerequisites.Add(TEXT("research.beta"));
	Alpha.RequiredFacilityIds.Add(TEXT("facility.missing-lab"));
	FResearchRule Beta;
	Beta.Identity.RuleId = TEXT("research.beta");
	Beta.DisplayName = TEXT("Beta");
	Beta.Effort = 10;
	Beta.Prerequisites.Add(TEXT("research.alpha"));
	Beta.RequiredFacilityIds.Add(TEXT("facility.cyclic-lab"));
	Package.Research = { Alpha, Beta };

	FFacilityRule CyclicLab;
	CyclicLab.Identity.RuleId = TEXT("facility.cyclic-lab");
	CyclicLab.DisplayName = TEXT("Cyclic Lab");
	CyclicLab.BuildHours = 1;
	CyclicLab.RequiredResearch.Add(Beta.Identity.RuleId);
	Package.Facilities.Add(CyclicLab);

	FItemRule Item;
	Item.Identity.RuleId = TEXT("item.unresolved");
	Item.DisplayName = TEXT("Unresolved Item");
	Item.Category = TEXT("equipment");
	Item.RequiredResearch.Add(TEXT("research.missing"));
	Item.ManufactureCost = 5;
	Item.ManufactureHours = 1;
	FManufacturingInputRule& MissingInput = Item.ManufactureInputs.AddDefaulted_GetRef();
	MissingInput.ItemId = TEXT("item.missing-material");
	MissingInput.Quantity = 2;
	Package.Items.Add(Item);

	FKnowledgeArchiveEntryRule ArchiveEntry;
	ArchiveEntry.Identity.RuleId = TEXT("archive.unresolved-link");
	ArchiveEntry.DisplayName = TEXT("Unresolved Link");
	ArchiveEntry.CategoryId = TEXT("category.test");
	ArchiveEntry.Summary = TEXT("Reference test summary.");
	ArchiveEntry.Body = TEXT("Reference test body.");
	ArchiveEntry.RelatedEntryIds.Add(TEXT("archive.missing"));
	Package.ArchiveEntries.Add(ArchiveEntry);

	FAdversaryMissionRule Mission;
	Mission.Identity.RuleId = TEXT("mission.unresolved-contact");
	Mission.DisplayName = TEXT("Unresolved Contact Mission");
	Mission.ContactRuleId = TEXT("contact.missing");
	Mission.TargetRegionId = TEXT("region.test");
	Mission.OriginLongitudeMilliDegrees = -1000;
	Mission.DestinationLongitudeMilliDegrees = 1000;
	Package.AdversaryMissions.Add(Mission);

	FAdversaryPlanRule MissingOpeningPlan;
	MissingOpeningPlan.Identity.RuleId = TEXT("plan.missing-opening");
	MissingOpeningPlan.DisplayName = TEXT("Missing Opening");
	MissingOpeningPlan.OpeningMissionRuleId = TEXT("mission.missing-opening");
	Package.AdversaryPlans.Add(MissingOpeningPlan);
	FAdversaryPlanRule BranchingPlan;
	BranchingPlan.Identity.RuleId = TEXT("plan.branching-test");
	BranchingPlan.DisplayName = TEXT("Branching Test");
	BranchingPlan.OpeningMissionRuleId = TEXT("mission.plan-opening");
	Package.AdversaryPlans.Add(BranchingPlan);
	FAdversaryPlanRule InvalidOpeningPlan;
	InvalidOpeningPlan.Identity.RuleId = TEXT("plan.invalid-opening");
	InvalidOpeningPlan.DisplayName = TEXT("Invalid Opening");
	InvalidOpeningPlan.OpeningMissionRuleId = TEXT("mission.plan-wrong-stage");
	Package.AdversaryPlans.Add(InvalidOpeningPlan);

	FAdversaryMissionRule PlanOpening = Mission;
	PlanOpening.Identity.RuleId = TEXT("mission.plan-opening");
	PlanOpening.DisplayName = TEXT("Plan Opening");
	PlanOpening.PlanId = BranchingPlan.Identity.RuleId;
	PlanOpening.PlanStage = 1;
	PlanOpening.EscapeBranchMissionRuleId = TEXT("mission.missing-branch");
	PlanOpening.ThwartBranchMissionRuleId = TEXT("mission.plan-wrong-stage");
	Package.AdversaryMissions.Add(PlanOpening);
	FAdversaryMissionRule WrongStage = Mission;
	WrongStage.Identity.RuleId = TEXT("mission.plan-wrong-stage");
	WrongStage.DisplayName = TEXT("Wrong Stage");
	WrongStage.PlanId = BranchingPlan.Identity.RuleId;
	WrongStage.PlanStage = 3;
	Package.AdversaryMissions.Add(WrongStage);
	FAdversaryMissionRule MissingPlan = Mission;
	MissingPlan.Identity.RuleId = TEXT("mission.missing-plan");
	MissingPlan.DisplayName = TEXT("Missing Plan");
	MissingPlan.PlanId = TEXT("plan.undefined");
	MissingPlan.PlanStage = 1;
	Package.AdversaryMissions.Add(MissingPlan);
	FAdversaryMissionRule ExtremeStage = Mission;
	ExtremeStage.Identity.RuleId = TEXT("mission.plan-max-stage");
	ExtremeStage.DisplayName = TEXT("Maximum Stage");
	ExtremeStage.PlanId = BranchingPlan.Identity.RuleId;
	ExtremeStage.PlanStage = MAX_int32;
	ExtremeStage.ThwartBranchMissionRuleId = TEXT("mission.plan-min-stage");
	Package.AdversaryMissions.Add(ExtremeStage);
	FAdversaryMissionRule ExtremeBranch = Mission;
	ExtremeBranch.Identity.RuleId = TEXT("mission.plan-min-stage");
	ExtremeBranch.DisplayName = TEXT("Minimum Stage");
	ExtremeBranch.PlanId = BranchingPlan.Identity.RuleId;
	ExtremeBranch.PlanStage = MIN_int32;
	Package.AdversaryMissions.Add(ExtremeBranch);
	FContactRule ExtremeLandingContact;
	ExtremeLandingContact.Identity.RuleId = TEXT("contact.extreme-landing");
	ExtremeLandingContact.DisplayName = TEXT("Extreme Landing Contact");
	ExtremeLandingContact.Signature = 1;
	ExtremeLandingContact.CruiseSpeedKilometersPerHour = 1;
	ExtremeLandingContact.MaxHull = 1;
	ExtremeLandingContact.ThreatRating = MAX_int32;
	ExtremeLandingContact.AttackAccuracy = 1;
	ExtremeLandingContact.AttackDamage = 1;
	ExtremeLandingContact.AttackIntervalSeconds = 1;
	Package.Contacts.Add(ExtremeLandingContact);
	FAdversaryMissionRule ExtremeLandingMission;
	ExtremeLandingMission.Identity.RuleId = TEXT("mission.extreme-landing");
	ExtremeLandingMission.DisplayName = TEXT("Extreme Landing Mission");
	ExtremeLandingMission.ContactRuleId = ExtremeLandingContact.Identity.RuleId;
	ExtremeLandingMission.TargetRegionId = TEXT("region.test");
	ExtremeLandingMission.OriginLongitudeMilliDegrees = -1000;
	ExtremeLandingMission.DestinationLongitudeMilliDegrees = 1000;
	ExtremeLandingMission.IntervalHours = 1;
	ExtremeLandingMission.MinimumEscalation = 1;
	ExtremeLandingMission.SelectionWeight = 1;
	ExtremeLandingMission.PressureOnEscape = 1;
	ExtremeLandingMission.bCreatesLandingSiteOnArrival = true;
	ExtremeLandingMission.LandingSiteLifetimeHours = 1;
	ExtremeLandingMission.LandingSiteThreatBonus = MIN_int32;
	Package.AdversaryMissions.Add(ExtremeLandingMission);

	FTacticalMissionRule TacticalMission;
	TacticalMission.Identity.RuleId = TEXT("tactical.unresolved-recovery");
	TacticalMission.DisplayName = TEXT("Unresolved Recovery");
	TacticalMission.SourceContactRuleId = TEXT("contact.missing");
	TacticalMission.FloorTerrainRuleId = TEXT("terrain.missing-floor");
	TacticalMission.ObstacleTerrainRuleId = TEXT("terrain.missing-wall");
	TacticalMission.AdversaryUnitRuleId = TEXT("unit.missing-raider");
	TacticalMission.ObjectiveId = TEXT("objective.test-core");
	TacticalMission.MapWidth = 12;
	TacticalMission.MapHeight = 18;
	TacticalMission.DeploymentDepth = 2;
	TacticalMission.ObstaclePercent = 20;
	TacticalMission.BaseEnemyCount = 2;
	TacticalMission.EnemiesPerThreat = 1;
	TacticalMission.TurnLimit = 24;
	FTacticalMissionRule DuplicateTacticalMission = TacticalMission;
	DuplicateTacticalMission.Identity.RuleId = TEXT("tactical.duplicate-contact-map");
	Package.TacticalMissions = { TacticalMission, DuplicateTacticalMission };

	const FRuleSetBuildResult Result = FRuleSetBuilder::Build({ Package });
	TestFalse(TEXT("Invalid cross references fail build"), Result.bSucceeded);
	TestTrue(TEXT("Missing research is diagnosed"), Result.HasDiagnostic(TEXT("missing_research_reference")));
	TestTrue(TEXT("Missing research facility is diagnosed"),
		Result.HasDiagnostic(TEXT("missing_research_facility_reference")));
	TestTrue(TEXT("Direct research/facility dependency cycles are diagnosed"),
		Result.HasDiagnostic(TEXT("cyclic_research_facility_requirement")));
	TestTrue(TEXT("Missing manufacturing input is diagnosed"), Result.HasDiagnostic(TEXT("missing_manufacturing_input_reference")));
	TestTrue(TEXT("Missing related archive record is diagnosed"), Result.HasDiagnostic(TEXT("missing_archive_reference")));
	TestTrue(TEXT("Missing adversary contact is diagnosed"), Result.HasDiagnostic(TEXT("missing_contact_reference")));
	TestTrue(TEXT("Missing plan opening is diagnosed"), Result.HasDiagnostic(TEXT("missing_adversary_plan_opening")));
	TestTrue(TEXT("Invalid plan opening ownership is diagnosed"), Result.HasDiagnostic(TEXT("invalid_adversary_plan_opening")));
	TestTrue(TEXT("Missing mission plan is diagnosed"), Result.HasDiagnostic(TEXT("missing_adversary_plan_reference")));
	TestTrue(TEXT("Missing plan branch is diagnosed"), Result.HasDiagnostic(TEXT("missing_adversary_plan_branch")));
	TestTrue(TEXT("Non-sequential plan branch is diagnosed"), Result.HasDiagnostic(TEXT("invalid_adversary_plan_branch")));
	bool bExtremeStageBranchDiagnosed = false;
	for (const FContentDiagnostic& Diagnostic : Result.Diagnostics)
	{
		bExtremeStageBranchDiagnosed |= Diagnostic.Code == FName(TEXT("invalid_adversary_plan_branch"))
			&& Diagnostic.Message.Contains(TEXT("mission.plan-max-stage"));
	}
	TestTrue(TEXT("Extreme plan stages are compared without signed wraparound"), bExtremeStageBranchDiagnosed);
	bool bExtremeLandingThreatOverflowDiagnosed = false;
	for (const FContentDiagnostic& Diagnostic : Result.Diagnostics)
	{
		bExtremeLandingThreatOverflowDiagnosed |= Diagnostic.Code == FName(TEXT("landing_site_threat_overflow"))
			&& Diagnostic.Message.Contains(TEXT("mission.extreme-landing"));
	}
	TestFalse(TEXT("Extreme landing threat validation does not wrap a minimum bonus into a false overflow"),
		bExtremeLandingThreatOverflowDiagnosed);
	TestTrue(TEXT("Unreachable plan mission is diagnosed"), Result.HasDiagnostic(TEXT("orphaned_adversary_plan_mission")));
	TestTrue(TEXT("Missing tactical floor is diagnosed"), Result.HasDiagnostic(TEXT("invalid_tactical_floor_reference")));
	TestTrue(TEXT("Missing tactical obstacle is diagnosed"), Result.HasDiagnostic(TEXT("invalid_tactical_obstacle_reference")));
	TestTrue(TEXT("Missing tactical unit is diagnosed"), Result.HasDiagnostic(TEXT("missing_tactical_unit_reference")));
	TestTrue(TEXT("Duplicate tactical contact mapping is diagnosed"), Result.HasDiagnostic(TEXT("duplicate_tactical_contact_mapping")));
	TestTrue(TEXT("Research cycle is diagnosed"), Result.HasDiagnostic(TEXT("research_cycle")));
	TestEqual(TEXT("Failed build exposes no partial rules"), Result.RuleSet.Items.Num()
		+ Result.RuleSet.Research.Num() + Result.RuleSet.ArchiveEntries.Num()
		+ Result.RuleSet.Facilities.Num() + Result.RuleSet.PersonnelRoles.Num()
		+ Result.RuleSet.PersonnelDoctrines.Num() + Result.RuleSet.PersonnelCommendations.Num()
		+ Result.RuleSet.Craft.Num() + Result.RuleSet.Contacts.Num()
		+ Result.RuleSet.AdversaryPlans.Num() + Result.RuleSet.AdversaryMissions.Num() + Result.RuleSet.TacticalTerrains.Num()
		+ Result.RuleSet.TacticalUnits.Num() + Result.RuleSet.TacticalMissions.Num(), 0);

	return true;
}

#endif
