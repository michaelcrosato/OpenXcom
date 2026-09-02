// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Content/ContentPackageCatalog.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectBaseContentCatalogTest,
	"UEGT.Core.Content.Catalog.ProjectBasePackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectBaseContentCatalogTest::RunTest(const FString& Parameters)
{
	const FString RulesDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
	const FContentCatalogLoadResult Result = FContentPackageCatalog::LoadDirectory(RulesDirectory);
	TestTrue(TEXT("Project base content catalog loads"), Result.bSucceeded);
	TestEqual(TEXT("One original package is loaded"), Result.Packages.Num(), 1);
	TestEqual(TEXT("One source file is tracked"), Result.LoadedFiles.Num(), 1);
	TestEqual(TEXT("Base catalog exposes seventeen item rules"), Result.RuleSet.Items.Num(), 17);
	TestEqual(TEXT("Base catalog exposes five research rules"), Result.RuleSet.Research.Num(), 5);
	TestEqual(TEXT("Base catalog exposes nine original knowledge-archive records"),
		Result.RuleSet.ArchiveEntries.Num(), 9);
	TestEqual(TEXT("Base catalog exposes eleven facility rules"), Result.RuleSet.Facilities.Num(), 11);
	TestEqual(TEXT("Base catalog exposes four personnel role rules"), Result.RuleSet.PersonnelRoles.Num(), 4);
	TestEqual(TEXT("Base catalog exposes four field doctrine rules"), Result.RuleSet.PersonnelDoctrines.Num(), 4);
	TestEqual(TEXT("Base catalog exposes four personnel commendation rules"), Result.RuleSet.PersonnelCommendations.Num(), 4);
	TestEqual(TEXT("Base catalog exposes two craft rules"), Result.RuleSet.Craft.Num(), 2);
	TestEqual(TEXT("Base catalog exposes three contact rules"), Result.RuleSet.Contacts.Num(), 3);
	TestEqual(TEXT("Base catalog exposes three regional mandate rules"), Result.RuleSet.Regions.Num(), 3);
	TestEqual(TEXT("Base catalog exposes two adversary plan rules"), Result.RuleSet.AdversaryPlans.Num(), 2);
	TestEqual(TEXT("Base catalog exposes fourteen adversary mission rules"), Result.RuleSet.AdversaryMissions.Num(), 14);
	TestEqual(TEXT("Base catalog exposes nine tactical terrain rules"), Result.RuleSet.TacticalTerrains.Num(), 9);
	TestEqual(TEXT("Base catalog exposes three tactical unit rules"), Result.RuleSet.TacticalUnits.Num(), 3);
	TestEqual(TEXT("Base catalog exposes seven tactical mission rules"), Result.RuleSet.TacticalMissions.Num(), 7);
	if (Result.Packages.Num() == 1)
	{
		TestEqual(TEXT("Base package id is original UEGT namespace"), Result.Packages[0].Descriptor.PackageId, FName(TEXT("uegt.base")));
		TestEqual(TEXT("Base package version is explicit"), Result.Packages[0].Descriptor.Version, FString(TEXT("0.42.0")));
		TestEqual(TEXT("Base package exposes knowledge-archive records"),
			Result.Packages[0].ArchiveEntries.Num(), 9);
		TestEqual(TEXT("Base package exposes strategic contact rules"), Result.Packages[0].Contacts.Num(), 3);
		TestEqual(TEXT("Base package exposes regional mandate rules"), Result.Packages[0].Regions.Num(), 3);
		TestEqual(TEXT("Base package exposes field doctrine rules"), Result.Packages[0].PersonnelDoctrines.Num(), 4);
		TestEqual(TEXT("Base package exposes commendation rules"), Result.Packages[0].PersonnelCommendations.Num(), 4);
		TestEqual(TEXT("Base package exposes adversary plans"), Result.Packages[0].AdversaryPlans.Num(), 2);
		TestEqual(TEXT("Base package exposes adversary mission rules"), Result.Packages[0].AdversaryMissions.Num(), 14);
		TestEqual(TEXT("Base package exposes tactical mission rules"), Result.Packages[0].TacticalMissions.Num(), 7);

		FContentPackage MissingLandingRecipe = Result.Packages[0];
		MissingLandingRecipe.TacticalMissions.RemoveAll(
			[](const FTacticalMissionRule& Mission) { return Mission.SiteType == ETacticalSiteType::Landing; });
		const FRuleSetBuildResult MissingLandingBuild = FRuleSetBuilder::Build({ MissingLandingRecipe });
		TestTrue(TEXT("Landing-capable missions require an intact-site tactical mapping"),
			!MissingLandingBuild.bSucceeded
			&& MissingLandingBuild.HasDiagnostic(TEXT("missing_landing_tactical_mapping")));

		FContentPackage OrphanedLandingRecipe = Result.Packages[0];
		FAdversaryMissionRule* LandingSource = OrphanedLandingRecipe.AdversaryMissions.FindByPredicate(
			[](const FAdversaryMissionRule& Mission) { return Mission.bCreatesLandingSiteOnArrival; });
		TestNotNull(TEXT("Landing-capable source is present for policy probes"), LandingSource);
		if (LandingSource != nullptr)
		{
			LandingSource->bCreatesLandingSiteOnArrival = false;
			LandingSource->LandingSiteLifetimeHours = 0;
			LandingSource->LandingSiteThreatBonus = 0;
		}
		const FRuleSetBuildResult OrphanedLandingBuild = FRuleSetBuilder::Build({ OrphanedLandingRecipe });
		TestTrue(TEXT("Intact-site tactical mappings cannot be orphaned from strategic content"),
			!OrphanedLandingBuild.bSucceeded
			&& OrphanedLandingBuild.HasDiagnostic(TEXT("orphaned_landing_tactical_mapping")));

		FContentPackage ExcessiveLandingThreat = Result.Packages[0];
		FAdversaryMissionRule* ExcessiveLandingSource = ExcessiveLandingThreat.AdversaryMissions.FindByPredicate(
			[](const FAdversaryMissionRule& Mission) { return Mission.bCreatesLandingSiteOnArrival; });
		if (ExcessiveLandingSource != nullptr)
		{
			ExcessiveLandingSource->LandingSiteThreatBonus = 9;
		}
		const FRuleSetBuildResult ExcessiveLandingBuild = FRuleSetBuilder::Build({ ExcessiveLandingThreat });
		TestTrue(TEXT("Contact and intact-site threat cannot exceed the tactical simulation bound"),
			!ExcessiveLandingBuild.bSucceeded
			&& ExcessiveLandingBuild.HasDiagnostic(TEXT("landing_site_threat_overflow")));

		FContentPackage DuplicateLandingRecipe = Result.Packages[0];
		const FTacticalMissionRule* LandingRecipe = DuplicateLandingRecipe.TacticalMissions.FindByPredicate(
			[](const FTacticalMissionRule& Mission) { return Mission.SiteType == ETacticalSiteType::Landing; });
		TestNotNull(TEXT("Landing tactical recipe is present for duplicate-policy probes"), LandingRecipe);
		if (LandingRecipe != nullptr)
		{
			FTacticalMissionRule DuplicateLanding = *LandingRecipe;
			DuplicateLanding.Identity.RuleId = TEXT("tactical.duplicate-landing-denial");
			DuplicateLandingRecipe.TacticalMissions.Add(DuplicateLanding);
		}
		const FRuleSetBuildResult DuplicateLandingBuild = FRuleSetBuilder::Build({ DuplicateLandingRecipe });
		TestTrue(TEXT("The same contact and site condition cannot map to two tactical recipes"),
			!DuplicateLandingBuild.bSucceeded
			&& DuplicateLandingBuild.HasDiagnostic(TEXT("duplicate_tactical_contact_mapping")));

		FContentPackage MissingRegion = Result.Packages[0];
		MissingRegion.Regions.RemoveAll(
			[](const FStrategicRegionRule& Region) { return Region.Identity.RuleId == FName(TEXT("region.cascadia")); });
		const FRuleSetBuildResult MissingRegionBuild = FRuleSetBuilder::Build({ MissingRegion });
		TestTrue(TEXT("Adversary missions cannot target a missing authored mandate region"),
			!MissingRegionBuild.bSucceeded
			&& MissingRegionBuild.HasDiagnostic(TEXT("missing_region_reference")));

		FContentPackage InvalidRegion = Result.Packages[0];
		InvalidRegion.Regions[0].FundingWeight = 0;
		const FRuleSetBuildResult InvalidRegionBuild = FRuleSetBuilder::Build({ InvalidRegion });
		TestTrue(TEXT("Regional funding weights must remain positive and bounded"),
			!InvalidRegionBuild.bSucceeded
			&& InvalidRegionBuild.HasDiagnostic(TEXT("invalid_rule_value")));
	}
	const FFacilityRule* DefenseBattery = Result.RuleSet.Facilities.Find(TEXT("facility.aegis-battery"));
	TestNotNull(TEXT("Original base-defense battery resolves"), DefenseBattery);
	if (DefenseBattery != nullptr)
	{
		TestTrue(TEXT("Base-defense battery exposes one typed automatic shot"),
			DefenseBattery->BaseDefenseAccuracy == 72 && DefenseBattery->BaseDefenseDamage == 95);
	}
	const FFacilityRule* PrecisionBattery = Result.RuleSet.Facilities.Find(TEXT("facility.parallax-interceptor"));
	const FFacilityRule* HeavyBattery = Result.RuleSet.Facilities.Find(TEXT("facility.resonance-lance"));
	const FItemRule* PerimeterSupply = Result.RuleSet.Items.Find(TEXT("item.perimeter-capacitor"));
	TestTrue(TEXT("Perimeter capacitor is a manufacturable typed defense supply"),
		PerimeterSupply != nullptr && PerimeterSupply->Category == FName(TEXT("base-defense-supply"))
		&& PerimeterSupply->ManufactureCost == 9000 && PerimeterSupply->ManufactureHours == 18);
	TestTrue(TEXT("Original defense recipes expose distinct precision, balanced, and heavy profiles"),
		PrecisionBattery != nullptr && HeavyBattery != nullptr && DefenseBattery != nullptr
		&& PrecisionBattery->BaseDefenseAccuracy == 92 && PrecisionBattery->BaseDefenseDamage == 60
		&& DefenseBattery->BaseDefenseAccuracy == 72 && DefenseBattery->BaseDefenseDamage == 95
		&& HeavyBattery->BaseDefenseAccuracy == 46 && HeavyBattery->BaseDefenseDamage == 180
		&& PrecisionBattery->BaseDefenseAccuracy > DefenseBattery->BaseDefenseAccuracy
		&& DefenseBattery->BaseDefenseAccuracy > HeavyBattery->BaseDefenseAccuracy
		&& PrecisionBattery->BaseDefenseDamage < DefenseBattery->BaseDefenseDamage
		&& DefenseBattery->BaseDefenseDamage < HeavyBattery->BaseDefenseDamage
		&& PrecisionBattery->BaseDefenseSupplyItemId == FName(TEXT("item.perimeter-capacitor"))
		&& DefenseBattery->BaseDefenseSupplyItemId == PrecisionBattery->BaseDefenseSupplyItemId
		&& HeavyBattery->BaseDefenseSupplyItemId == PrecisionBattery->BaseDefenseSupplyItemId
		&& PrecisionBattery->BaseDefenseSupplyPerShot == 1
		&& DefenseBattery->BaseDefenseSupplyPerShot == 2
		&& HeavyBattery->BaseDefenseSupplyPerShot == 4
		&& PrecisionBattery->RequiredResearch.Contains(TEXT("research.signal-analysis"))
		&& HeavyBattery->RequiredResearch.Contains(TEXT("research.directed-energy")));
	const FAdversaryMissionRule* BaseRaid = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.nightglass-raid"));
	TestTrue(TEXT("Base raid exposes dynamic targeting and breach damage"), BaseRaid != nullptr
		&& BaseRaid->bTargetsPlayerBase && BaseRaid->BaseFacilityDamage == 110 && BaseRaid->BaseFacilitiesHit == 2);
	const FFacilityRule* OperationsHub = Result.RuleSet.Facilities.Find(TEXT("facility.operations-hub"));
	TestNotNull(TEXT("Core operations facility resolves"), OperationsHub);
	if (OperationsHub != nullptr)
	{
		TestTrue(TEXT("Operations hub exposes typed durability and repair economics"),
			OperationsHub->MaxIntegrity == 400
			&& OperationsHub->RepairCostPerIntegrity == 600
			&& OperationsHub->RepairHoursPerIntegrity == 1
			&& OperationsHub->StorageCapacity == 240);
	}
	const FFacilityRule* SecureStorage = Result.RuleSet.Facilities.Find(TEXT("facility.secure-storage"));
	TestTrue(TEXT("Secure storage exposes mass-weighted inventory capacity"),
		SecureStorage != nullptr && SecureStorage->StorageCapacity == 1200);
	const FFacilityRule* ResearchLab = Result.RuleSet.Facilities.Find(TEXT("facility.research-lab"));
	const FFacilityRule* FabricationBay = Result.RuleSet.Facilities.Find(TEXT("facility.fabrication-bay"));
	const FFacilityRule* EnergyLab = Result.RuleSet.Facilities.Find(TEXT("facility.energy-lab"));
	const FFacilityRule* Biocontainment = Result.RuleSet.Facilities.Find(TEXT("facility.biocontainment"));
	TestTrue(TEXT("Original laboratories and fabrication expose typed personnel capacity bonuses"),
		ResearchLab != nullptr && ResearchLab->ScientistCapacity == 10
		&& FabricationBay != nullptr && FabricationBay->EngineerCapacity == 10
		&& EnergyLab != nullptr && EnergyLab->ScientistCapacity == 6
		&& Biocontainment != nullptr && Biocontainment->ScientistCapacity == 6);
	const FResearchRule* SignalAnalysis = Result.RuleSet.Research.Find(TEXT("research.signal-analysis"));
	const FResearchRule* DirectedEnergy = Result.RuleSet.Research.Find(TEXT("research.directed-energy"));
	TestTrue(TEXT("Defense recipes are explicitly surfaced by their research unlocks"),
		SignalAnalysis != nullptr && DirectedEnergy != nullptr
		&& SignalAnalysis->UnlockRuleIds.Contains(TEXT("facility.aegis-battery"))
		&& SignalAnalysis->UnlockRuleIds.Contains(TEXT("facility.parallax-interceptor"))
		&& DirectedEnergy->UnlockRuleIds.Contains(TEXT("facility.resonance-lance")));
	TestTrue(TEXT("Research topics require explicit operational specialist facilities"),
		SignalAnalysis != nullptr && DirectedEnergy != nullptr
		&& SignalAnalysis->RequiredFacilityIds == TArray<FName>{ FName(TEXT("facility.operations-hub")) }
		&& DirectedEnergy->RequiredFacilityIds == TArray<FName>{ FName(TEXT("facility.energy-lab")) });
	TestTrue(TEXT("Specialist laboratories become constructible before the topics they enable"),
		EnergyLab != nullptr && Biocontainment != nullptr
		&& EnergyLab->RequiredResearch == TArray<FName>{ FName(TEXT("research.signal-analysis")) }
		&& Biocontainment->RequiredResearch == TArray<FName>{ FName(TEXT("research.signal-analysis")) });
	const FKnowledgeArchiveEntryRule* Charter =
		Result.RuleSet.ArchiveEntries.Find(TEXT("archive.signal-front-charter"));
	const FKnowledgeArchiveEntryRule* CarrierSignatures =
		Result.RuleSet.ArchiveEntries.Find(TEXT("archive.anomalous-carriers"));
	const FKnowledgeArchiveEntryRule* CinderAssessment =
		Result.RuleSet.ArchiveEntries.Find(TEXT("archive.cinder-lattice-assessment"));
	TestTrue(TEXT("Knowledge archive includes an initially available command record"),
		Charter != nullptr && Charter->CategoryId == FName(TEXT("category.command"))
		&& Charter->RequiredResearch.IsEmpty()
		&& Charter->RelatedEntryIds.Contains(TEXT("archive.perimeter-doctrine")));
	TestTrue(TEXT("Knowledge archive records expose typed research gates and related links"),
		CarrierSignatures != nullptr
		&& CarrierSignatures->RequiredResearch.Contains(TEXT("research.signal-analysis"))
		&& CarrierSignatures->RelatedEntryIds.Contains(TEXT("archive.liminal-ecology"))
		&& !CarrierSignatures->Body.IsEmpty());
	TestTrue(TEXT("Late-stage Cinder intelligence is research-gated and cross-linked"),
		CinderAssessment != nullptr
		&& CinderAssessment->CategoryId == FName(TEXT("category.science"))
		&& CinderAssessment->RequiredResearch == TArray<FName>{ FName(TEXT("research.directed-energy")) }
		&& CinderAssessment->RelatedEntryIds.Contains(TEXT("archive.anomalous-carriers"))
		&& CarrierSignatures != nullptr
		&& CarrierSignatures->RelatedEntryIds.Contains(TEXT("archive.cinder-lattice-assessment")));
	const FItemRule* ServiceRifle = Result.RuleSet.Items.Find(TEXT("item.service-rifle"));
	TestNotNull(TEXT("Service rifle item resolves"), ServiceRifle);
	if (ServiceRifle != nullptr)
	{
		TestTrue(TEXT("Service rifle exposes a tactical attack profile"), ServiceRifle->IsTacticalWeapon());
		TestEqual(TEXT("Service rifle tactical range is data-driven"), ServiceRifle->TacticalRange, 12);
		TestEqual(TEXT("Service rifle tactical AP cost is data-driven"), ServiceRifle->TacticalActionPointCost, 4);
		TestEqual(TEXT("Service rifle tactical magazine is data-driven"), ServiceRifle->TacticalMagazineCapacity, 8);
		TestEqual(TEXT("Service rifle tactical ammunition link is typed"), ServiceRifle->TacticalAmmunitionItemId, FName(TEXT("item.service-magazine")));
		TestTrue(TEXT("Service rifle exposes a validated burst mode"), ServiceRifle->HasTacticalBurstMode());
		TestEqual(TEXT("Service rifle burst shot count is data-driven"), ServiceRifle->TacticalBurstShotCount, 3);
	}
	const FItemRule* BreachLauncher = Result.RuleSet.Items.Find(TEXT("item.breach-launcher"));
	TestNotNull(TEXT("Resonance breach launcher resolves"), BreachLauncher);
	if (BreachLauncher != nullptr)
	{
		TestTrue(TEXT("Breach launcher exposes a ground-target blast profile"), BreachLauncher->HasTacticalBlastProfile());
		TestEqual(TEXT("Breach launcher blast radius is data-driven"), BreachLauncher->TacticalBlastRadius, 2);
		TestEqual(TEXT("Breach launcher scatter is data-driven"), BreachLauncher->TacticalScatterRadius, 1);
		TestEqual(TEXT("Breach launcher terrain multiplier is data-driven"), BreachLauncher->TacticalTerrainDamagePercent, 160);
		TestEqual(TEXT("Breach launcher ammunition link is typed"), BreachLauncher->TacticalAmmunitionItemId, FName(TEXT("item.breach-shells")));
		TestTrue(TEXT("Advanced manufacturing exposes a typed recovered-material recipe"),
			BreachLauncher->ManufactureInputs.Num() == 1
			&& BreachLauncher->ManufactureInputs[0].ItemId == FName(TEXT("item.resonance-shard"))
			&& BreachLauncher->ManufactureInputs[0].Quantity == 1);
	}
	const FTacticalTerrainRule* Bulkhead = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.prismatic-bulkhead"));
	const FTacticalTerrainRule* Deck = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.slate-mesh-deck"));
	const FTacticalTerrainRule* Hatch = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.resonance-hatch"));
	const FTacticalTerrainRule* Lift = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.resonance-lift"));
	TestTrue(TEXT("Terrain exposes data-driven blast resistance, flammability, and ventilation"), Bulkhead != nullptr && Deck != nullptr
		&& Bulkhead->BlastResistancePercent == 70 && Bulkhead->Flammability == 5
		&& Bulkhead->VentilationPercent == 0
		&& Bulkhead->ThrowObstacleHeight == 2
		&& Deck->BlastResistancePercent == 0 && Deck->Flammability == 35
		&& Deck->VentilationPercent == 45
		&& Deck->ThrowObstacleHeight == 0);
	TestTrue(TEXT("Original hatch exposes a bounded door action profile"), Hatch != nullptr && Hatch->IsDoor()
		&& Hatch->DoorActionPointCost == 1 && Hatch->ThrowObstacleHeight == 4
		&& Hatch->VentilationPercent == 100
		&& Hatch->bBlocksMovement && Hatch->bBlocksVision);
	TestTrue(TEXT("Original lift exposes a traversable vertical movement profile"), Lift != nullptr
		&& Lift->IsVerticalConnector() && Lift->VerticalMoveCost == 2
		&& !Lift->bBlocksMovement && Lift->VentilationPercent == 100);
	const FItemRule* AerosolCharge = Result.RuleSet.Items.Find(TEXT("item.thermal-aerosol-charge"));
	TestNotNull(TEXT("Thermal aerosol charge resolves"), AerosolCharge);
	if (AerosolCharge != nullptr)
	{
		TestTrue(TEXT("Thermal aerosol charge exposes a tactical device profile"), AerosolCharge->IsTacticalDevice());
		TestEqual(TEXT("Tactical device radius is data-driven"), AerosolCharge->TacticalRadius, 2);
		TestEqual(TEXT("Tactical device throw arc is data-driven"), AerosolCharge->TacticalThrowArcHeight, 4);
		TestEqual(TEXT("Tactical device smoke is data-driven"), AerosolCharge->TacticalSmoke, 70);
		TestEqual(TEXT("Tactical device fire is data-driven"), AerosolCharge->TacticalFire, 45);
	}
	const FItemRule* NullFoam = Result.RuleSet.Items.Find(TEXT("item.null-foam-canister"));
	TestNotNull(TEXT("Null-foam support device resolves"), NullFoam);
	if (NullFoam != nullptr)
	{
		TestTrue(TEXT("Null-foam exposes a tactical support profile"), NullFoam->IsTacticalDevice());
		TestTrue(TEXT("Null-foam exposes a tactical throw arc"), NullFoam->HasTacticalThrowArc());
		TestEqual(TEXT("Support smoke reduction is data-driven"), NullFoam->TacticalSmokeReduction, 55);
		TestEqual(TEXT("Support fire reduction is data-driven"), NullFoam->TacticalFireReduction, 75);
		TestEqual(TEXT("Support suppression reduction is data-driven"), NullFoam->TacticalSuppressionReduction, 25);
		TestEqual(TEXT("Support morale recovery is data-driven"), NullFoam->TacticalMoraleRecovery, 15);
	}
	const FItemRule* FieldScanner = Result.RuleSet.Items.Find(TEXT("item.field-scanner"));
	TestTrue(TEXT("Field scanner resolves as an original tactical signal projector"),
		FieldScanner != nullptr && FieldScanner->IsTacticalSignalProjector()
			&& FieldScanner->Power == 16 && FieldScanner->TacticalRange == 8
			&& FieldScanner->TacticalActionPointCost == 4);
	const FPersonnelRoleRule* FieldAgent = Result.RuleSet.PersonnelRoles.Find(TEXT("role.field-agent"));
	TestNotNull(TEXT("Field agent role resolves"), FieldAgent);
	if (FieldAgent != nullptr)
	{
		TestTrue(TEXT("Field agent category is typed"), FieldAgent->Category == EPersonnelRoleCategory::FieldAgent);
		TestEqual(TEXT("Field agent recruitment transit is data-driven"), FieldAgent->RecruitmentHours, 72);
	}
	const FPersonnelDoctrineRule* ClearSight = Result.RuleSet.PersonnelDoctrines.Find(TEXT("doctrine.clear-sight"));
	TestNotNull(TEXT("Clear Sight field doctrine resolves"), ClearSight);
	if (ClearSight != nullptr)
	{
		TestEqual(TEXT("Doctrine selection cap is data-driven"), ClearSight->MaxSelections, 3);
		TestEqual(TEXT("Doctrine accuracy bonus is data-driven"), ClearSight->AccuracyBonus, 4);
	}
	const FPersonnelCommendationRule* LongWatch = Result.RuleSet.PersonnelCommendations.Find(TEXT("commendation.long-watch"));
	TestNotNull(TEXT("Long Watch commendation resolves"), LongWatch);
	if (LongWatch != nullptr)
	{
		TestEqual(TEXT("Commendation mission threshold is data-driven"), LongWatch->RequiredMissions, 10);
		TestEqual(TEXT("Commendation kill threshold is data-driven"), LongWatch->RequiredKills, 8);
		TestEqual(TEXT("Commendation rank threshold is data-driven"), LongWatch->RequiredRank, 3);
		TestTrue(TEXT("Commendation requires a successful operation"), LongWatch->bRequiresSuccessfulMission);
	}
	const FCraftRule* Sparrow = Result.RuleSet.Craft.Find(TEXT("craft.sparrow-interceptor"));
	TestNotNull(TEXT("Sparrow interceptor craft resolves"), Sparrow);
	if (Sparrow != nullptr)
	{
		TestEqual(TEXT("Craft fuel capacity is data-driven"), Sparrow->FuelCapacity, 900);
		TestEqual(TEXT("Craft equipment slots are data-driven"), Sparrow->EquipmentSlots, 2);
	}
	const FItemRule* SkyLance = Result.RuleSet.Items.Find(TEXT("item.sky-lance"));
	TestNotNull(TEXT("Sky-Lance craft weapon resolves"), SkyLance);
	if (SkyLance != nullptr)
	{
		TestEqual(TEXT("Craft weapon ammunition link is typed"), SkyLance->AmmunitionItemId, FName(TEXT("item.sky-lance-rounds")));
		TestEqual(TEXT("Craft weapon magazine capacity is data-driven"), SkyLance->MagazineCapacity, 12);
		TestEqual(TEXT("Craft weapon cadence is data-driven"), SkyLance->FireIntervalSeconds, 5);
	}
	const FFacilityRule* FlightDeck = Result.RuleSet.Facilities.Find(TEXT("facility.flight-deck"));
	TestNotNull(TEXT("Flight deck resolves"), FlightDeck);
	if (FlightDeck != nullptr)
	{
		TestEqual(TEXT("Flight deck provides typed craft capacity"), FlightDeck->CraftCapacity, 2);
	}
	const FContactRule* Skimmer = Result.RuleSet.Contacts.Find(TEXT("contact.skimmer"));
	const FContactRule* CinderLoom = Result.RuleSet.Contacts.Find(TEXT("contact.cinder-loom"));
	TestNotNull(TEXT("Skimmer strategic contact resolves"), Skimmer);
	if (Skimmer != nullptr)
	{
		TestEqual(TEXT("Contact signature is data-driven"), Skimmer->Signature, 65);
		TestEqual(TEXT("Contact movement speed is data-driven"), Skimmer->CruiseSpeedKilometersPerHour, 850);
	}
	TestTrue(TEXT("Cinder Loom is a distinct late-game interception profile"), CinderLoom != nullptr
		&& CinderLoom->Signature == 34 && CinderLoom->CruiseSpeedKilometersPerHour == 610
		&& CinderLoom->MaxHull == 320 && CinderLoom->ThreatRating == 6
		&& CinderLoom->AttackAccuracy == 70 && CinderLoom->AttackDamage == 34
		&& CinderLoom->AttackIntervalSeconds == 4);
	const FAdversaryMissionRule* OpeningMission = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.glass-tide-survey"));
	const FAdversaryPlanRule* MirrorRain = Result.RuleSet.AdversaryPlans.Find(TEXT("plan.mirror-rain"));
	TestTrue(TEXT("Mirror Rain plan resolves to its authored opening"), MirrorRain != nullptr
		&& MirrorRain->OpeningMissionRuleId == FName(TEXT("mission.glass-tide-survey")));
	TestNotNull(TEXT("Opening adversary mission resolves"), OpeningMission);
	if (OpeningMission != nullptr)
	{
		TestEqual(TEXT("Mission contact link is data-driven"), OpeningMission->ContactRuleId, FName(TEXT("contact.skimmer")));
		TestEqual(TEXT("Mission target region is typed"), OpeningMission->TargetRegionId, FName(TEXT("region.cascadia")));
		TestEqual(TEXT("Mission escape pressure is data-driven"), OpeningMission->PressureOnEscape, 12);
		TestTrue(TEXT("Mission outcomes expose authored mandate support effects"),
			OpeningMission->SupportLossOnEscape == 6 && OpeningMission->SupportGainOnThwarted == 3);
		TestTrue(TEXT("Opening mission exposes deterministic outcome branches"),
			OpeningMission->PlanId == FName(TEXT("plan.mirror-rain"))
			&& OpeningMission->PlanStage == 1
			&& OpeningMission->EscapeBranchMissionRuleId == FName(TEXT("mission.nightglass-raid"))
			&& OpeningMission->ThwartBranchMissionRuleId == FName(TEXT("mission.saffron-incursion")));
		TestTrue(TEXT("Opening mission exposes a shorter, higher-threat intact landing outcome"),
			OpeningMission->bCreatesLandingSiteOnArrival
			&& OpeningMission->LandingSiteLifetimeHours == 36
			&& OpeningMission->LandingSiteThreatBonus == 2);
	}
	const FAdversaryMissionRule* ThwartBranch = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.saffron-incursion"));
	const FAdversaryMissionRule* MirrorEscapeTerminal =
		Result.RuleSet.AdversaryMissions.Find(TEXT("mission.prism-schism-broadcast"));
	const FAdversaryMissionRule* MirrorThwartTerminal =
		Result.RuleSet.AdversaryMissions.Find(TEXT("mission.concordance-relay-hunt"));
	TestTrue(TEXT("Mirror Rain second-stage outcomes now converge on distinct coalition-counterplay terminals"),
		BaseRaid != nullptr && ThwartBranch != nullptr
		&& ThwartBranch->PlanId == FName(TEXT("plan.mirror-rain")) && ThwartBranch->PlanStage == 2
		&& BaseRaid->EscapeBranchMissionRuleId == FName(TEXT("mission.prism-schism-broadcast"))
		&& BaseRaid->ThwartBranchMissionRuleId == FName(TEXT("mission.concordance-relay-hunt"))
		&& ThwartBranch->EscapeBranchMissionRuleId == BaseRaid->EscapeBranchMissionRuleId
		&& ThwartBranch->ThwartBranchMissionRuleId == BaseRaid->ThwartBranchMissionRuleId
		&& MirrorEscapeTerminal != nullptr && MirrorThwartTerminal != nullptr
		&& MirrorEscapeTerminal->PlanStage == 3 && MirrorThwartTerminal->PlanStage == 3
		&& MirrorEscapeTerminal->EscapeBranchMissionRuleId.IsNone()
		&& MirrorEscapeTerminal->ThwartBranchMissionRuleId.IsNone()
		&& MirrorThwartTerminal->EscapeBranchMissionRuleId.IsNone()
		&& MirrorThwartTerminal->ThwartBranchMissionRuleId.IsNone()
		&& MirrorEscapeTerminal->CompactPeerSupportLossOnEscape == 4
		&& MirrorEscapeTerminal->WithdrawnCompactSupportGainOnThwarted == 6
		&& MirrorThwartTerminal->CompactPeerSupportLossOnEscape == 3
		&& MirrorThwartTerminal->WithdrawnCompactSupportGainOnThwarted == 8);
	const FAdversaryPlanRule* CinderLattice = Result.RuleSet.AdversaryPlans.Find(TEXT("plan.cinder-lattice"));
	const FAdversaryMissionRule* CinderOpening = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.ember-meridian-probe"));
	const FAdversaryMissionRule* Hearthline = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.hearthline-seeding"));
	const FAdversaryMissionRule* QuietEmber = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.quiet-ember-feint"));
	const FAdversaryMissionRule* AshenCrown = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.ashen-crown-raid"));
	const FAdversaryMissionRule* QuenchedVault = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.quenched-vault-run"));
	const FAdversaryMissionRule* LatticeUnraveling = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.lattice-unraveling"));
	const FAdversaryMissionRule* AshenAccord = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.ashen-accord-severance"));
	const FAdversaryMissionRule* DawnlineBlackout = Result.RuleSet.AdversaryMissions.Find(TEXT("mission.dawnline-blackout"));
	TestTrue(TEXT("Cinder Lattice opens only as a weighted escalation-five plan"),
		CinderLattice != nullptr && CinderOpening != nullptr
		&& CinderLattice->OpeningMissionRuleId == FName(TEXT("mission.ember-meridian-probe"))
		&& CinderOpening->PlanId == FName(TEXT("plan.cinder-lattice"))
		&& CinderOpening->PlanStage == 1 && CinderOpening->MinimumEscalation == 5
		&& CinderOpening->SelectionWeight == 2
		&& CinderOpening->EscapeBranchMissionRuleId == FName(TEXT("mission.hearthline-seeding"))
		&& CinderOpening->ThwartBranchMissionRuleId == FName(TEXT("mission.quiet-ember-feint")));
	TestTrue(TEXT("Cinder Lattice second stage distinguishes landing pressure from a diversion"),
		Hearthline != nullptr && QuietEmber != nullptr
		&& Hearthline->PlanStage == 2 && QuietEmber->PlanStage == 2
		&& Hearthline->bCreatesLandingSiteOnArrival
		&& Hearthline->LandingSiteLifetimeHours == 24 && Hearthline->LandingSiteThreatBonus == 2
		&& Hearthline->EscapeBranchMissionRuleId == FName(TEXT("mission.ashen-crown-raid"))
		&& Hearthline->ThwartBranchMissionRuleId == FName(TEXT("mission.quenched-vault-run"))
		&& QuietEmber->EscapeBranchMissionRuleId == FName(TEXT("mission.ashen-crown-raid"))
		&& QuietEmber->ThwartBranchMissionRuleId == FName(TEXT("mission.lattice-unraveling")));
	TestTrue(TEXT("Cinder Lattice stage three retains three outcomes and routes each into a fourth-stage coalition arc"),
		AshenCrown != nullptr && QuenchedVault != nullptr && LatticeUnraveling != nullptr
		&& AshenCrown->PlanStage == 3 && QuenchedVault->PlanStage == 3 && LatticeUnraveling->PlanStage == 3
		&& AshenCrown->bTargetsPlayerBase && AshenCrown->BaseFacilityDamage == 170
		&& AshenCrown->BaseFacilitiesHit == 3
		&& AshenCrown->EscapeBranchMissionRuleId == FName(TEXT("mission.ashen-accord-severance"))
		&& AshenCrown->ThwartBranchMissionRuleId == FName(TEXT("mission.dawnline-blackout"))
		&& QuenchedVault->EscapeBranchMissionRuleId == AshenCrown->EscapeBranchMissionRuleId
		&& QuenchedVault->ThwartBranchMissionRuleId == AshenCrown->ThwartBranchMissionRuleId
		&& LatticeUnraveling->EscapeBranchMissionRuleId == AshenCrown->EscapeBranchMissionRuleId
		&& LatticeUnraveling->ThwartBranchMissionRuleId == AshenCrown->ThwartBranchMissionRuleId);
	TestTrue(TEXT("Cinder Lattice has two reachable fourth-stage coalition-counterplay terminals"),
		AshenAccord != nullptr && DawnlineBlackout != nullptr
		&& AshenAccord->PlanStage == 4 && DawnlineBlackout->PlanStage == 4
		&& AshenAccord->EscapeBranchMissionRuleId.IsNone()
		&& AshenAccord->ThwartBranchMissionRuleId.IsNone()
		&& DawnlineBlackout->EscapeBranchMissionRuleId.IsNone()
		&& DawnlineBlackout->ThwartBranchMissionRuleId.IsNone()
		&& AshenAccord->CompactPeerSupportLossOnEscape == 8
		&& AshenAccord->WithdrawnCompactSupportGainOnThwarted == 10
		&& DawnlineBlackout->CompactPeerSupportLossOnEscape == 5
		&& DawnlineBlackout->WithdrawnCompactSupportGainOnThwarted == 12);
	const FStrategicRegionRule* Cascadia = Result.RuleSet.Regions.Find(TEXT("region.cascadia"));
	const FStrategicRegionRule* NorthAtlantic = Result.RuleSet.Regions.Find(TEXT("region.north-atlantic"));
	const FStrategicRegionRule* WesternPacific = Result.RuleSet.Regions.Find(TEXT("region.western-pacific"));
	TestTrue(TEXT("Regional mandates expose distinct support, funding, and pressure policies"),
		Cascadia != nullptr && NorthAtlantic != nullptr && WesternPacific != nullptr
		&& Cascadia->InitialSupport == 55 && Cascadia->FundingWeight == 40 && Cascadia->PressureTolerance == 45
		&& NorthAtlantic->InitialSupport == 60 && NorthAtlantic->FundingWeight == 35 && NorthAtlantic->PressureTolerance == 50
		&& WesternPacific->InitialSupport == 50 && WesternPacific->FundingWeight == 25 && WesternPacific->PressureTolerance == 40
		&& Cascadia->FundingWeight + NorthAtlantic->FundingWeight + WesternPacific->FundingWeight == 100);
	const FTacticalMissionRule* TacticalMission = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.glass-wreck-recovery"));
	TestNotNull(TEXT("Glass wreck tactical mission resolves"), TacticalMission);
	if (TacticalMission != nullptr)
	{
		TestTrue(TEXT("Wreck recovery is scoped to strategic sites"), TacticalMission->Context == ETacticalMissionContext::StrategicSite);
		TestTrue(TEXT("Wreck recovery is scoped to destroyed contacts"), TacticalMission->SiteType == ETacticalSiteType::Wreckage);
		TestEqual(TEXT("Tactical mission contact mapping is typed"), TacticalMission->SourceContactRuleId, FName(TEXT("contact.skimmer")));
		TestEqual(TEXT("Tactical mission width is data-driven"), TacticalMission->MapWidth, 20);
		TestEqual(TEXT("Tactical mission level count is data-driven"), TacticalMission->MapLevels, 2);
		TestEqual(TEXT("Tactical mission connector is typed"), TacticalMission->VerticalConnectorTerrainRuleId, FName(TEXT("terrain.resonance-lift")));
		TestEqual(TEXT("Tactical mission objective is original"), TacticalMission->ObjectiveId, FName(TEXT("objective.secure-resonance-core")));
		TestEqual(TEXT("Tactical objective AP cost is data-driven"), TacticalMission->ObjectiveActionPointCost, 2);
		TestTrue(TEXT("Recovery objective behavior and progress are data-driven"), TacticalMission->ObjectiveType == ETacticalObjectiveType::Recover
			&& TacticalMission->ObjectiveRequiredInteractions == 2);
		TestTrue(TEXT("Recovery objective reward is a typed item manifest"), TacticalMission->ObjectiveRewardItemId == FName(TEXT("item.resonance-shard"))
			&& TacticalMission->ObjectiveRewardQuantity == 2
			&& Result.RuleSet.Items.Contains(TacticalMission->ObjectiveRewardItemId));
		TestTrue(TEXT("Tactical debrief experience is data-driven"), TacticalMission->MissionExperienceReward == 35
			&& TacticalMission->ObjectiveExperienceReward == 85);
	}
	const FTacticalMissionRule* LandingMission = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.glass-landing-denial"));
	TestTrue(TEXT("Intact Glass landing resolves to a distinct higher-pressure tactical recipe"), LandingMission != nullptr
		&& LandingMission->Context == ETacticalMissionContext::StrategicSite
		&& LandingMission->SiteType == ETacticalSiteType::Landing
		&& LandingMission->SourceContactRuleId == FName(TEXT("contact.skimmer"))
		&& LandingMission->ObjectiveId == FName(TEXT("objective.break-shore-anchor"))
		&& LandingMission->ObjectiveType == ETacticalObjectiveType::Disrupt
		&& LandingMission->BaseEnemyCount == 4);
	const FTacticalMissionRule* ControlMission = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.aurora-vault-breach"));
	TestTrue(TEXT("Aurora mission exposes opposed objective control"), ControlMission != nullptr
		&& ControlMission->ObjectiveType == ETacticalObjectiveType::Control
		&& ControlMission->ObjectiveRequiredInteractions == 3);
	const FTacticalMissionRule* BaseDefenseMission = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.aegis-perimeter-stand"));
	TestTrue(TEXT("Aegis perimeter mission is a typed one-level non-extraction defense recipe"), BaseDefenseMission != nullptr
		&& BaseDefenseMission->Context == ETacticalMissionContext::BaseDefense
		&& BaseDefenseMission->SourceContactRuleId == FName(TEXT("contact.skimmer"))
		&& BaseDefenseMission->ObjectiveId == FName(TEXT("objective.hold-command-relay"))
		&& BaseDefenseMission->ObjectiveType == ETacticalObjectiveType::Control
		&& BaseDefenseMission->MapLevels == 1);
	const FTacticalMissionRule* CinderRecovery = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.cinder-loom-recovery"));
	const FTacticalMissionRule* HearthlineDenial = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.hearthline-seed-denial"));
	const FTacticalMissionRule* CinderDefense = Result.RuleSet.TacticalMissions.Find(TEXT("tactical.cinder-perimeter-lock"));
	TestTrue(TEXT("Cinder wreckage has a three-level high-threat recovery recipe"),
		CinderRecovery != nullptr
		&& CinderRecovery->Context == ETacticalMissionContext::StrategicSite
		&& CinderRecovery->SiteType == ETacticalSiteType::Wreckage
		&& CinderRecovery->SourceContactRuleId == FName(TEXT("contact.cinder-loom"))
		&& CinderRecovery->AdversaryUnitRuleId == FName(TEXT("unit.cinder-weaver"))
		&& CinderRecovery->ObjectiveType == ETacticalObjectiveType::Recover
		&& CinderRecovery->ObjectiveRewardItemId == FName(TEXT("item.resonance-shard"))
		&& CinderRecovery->ObjectiveRewardQuantity == 4 && CinderRecovery->MapLevels == 3);
	TestTrue(TEXT("Cinder landing and base assault select distinct tactical contexts"),
		HearthlineDenial != nullptr && CinderDefense != nullptr
		&& HearthlineDenial->Context == ETacticalMissionContext::StrategicSite
		&& HearthlineDenial->SiteType == ETacticalSiteType::Landing
		&& HearthlineDenial->ObjectiveType == ETacticalObjectiveType::Disrupt
		&& HearthlineDenial->ObjectiveRequiredInteractions == 4
		&& CinderDefense->Context == ETacticalMissionContext::BaseDefense
		&& CinderDefense->SourceContactRuleId == FName(TEXT("contact.cinder-loom"))
		&& CinderDefense->ObjectiveType == ETacticalObjectiveType::Control
		&& CinderDefense->MapLevels == 1
		&& CinderDefense->VerticalConnectorTerrainRuleId.IsNone());
	const FTacticalUnitRule* Scout = Result.RuleSet.TacticalUnits.Find(TEXT("unit.glass-tide-scout"));
	TestNotNull(TEXT("Glass Tide Scout tactical unit resolves"), Scout);
	if (Scout != nullptr)
	{
		TestEqual(TEXT("Adversary intrinsic attack range is data-driven"), Scout->AttackRange, 10);
		TestEqual(TEXT("Adversary intrinsic attack power is data-driven"), Scout->AttackPower, 14);
	}
	const FTacticalUnitRule* Warden = Result.RuleSet.TacticalUnits.Find(TEXT("unit.aurora-warden"));
	TestTrue(TEXT("Aurora Warden exposes an intrinsic signal-pressure profile"),
		Warden != nullptr && Warden->HasSignalProjection()
			&& Warden->SignalPower == 26 && Warden->SignalRange == 9
			&& Warden->SignalActionPointCost == 4);
	const FTacticalUnitRule* CinderWeaver = Result.RuleSet.TacticalUnits.Find(TEXT("unit.cinder-weaver"));
	const FTacticalTerrainRule* CinderDeck = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.cinder-ceramic-deck"));
	const FTacticalTerrainRule* CinderPier = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.ember-lattice-pier"));
	const FTacticalTerrainRule* ThermalGate = Result.RuleSet.TacticalTerrains.Find(TEXT("terrain.thermal-shunt-gate"));
	TestTrue(TEXT("Cinder Weaver combines thermal pressure with intrinsic signal projection"),
		CinderWeaver != nullptr && CinderWeaver->AttackDamageType == ETacticalDamageType::Thermal
		&& CinderWeaver->AttackPower == 30 && CinderWeaver->ThermalArmor == 30
		&& CinderWeaver->HasSignalProjection() && CinderWeaver->SignalPower == 32
		&& CinderWeaver->SignalRange == 10 && CinderWeaver->SignalActionPointCost == 4);
	TestTrue(TEXT("Cinder battlefields use a distinct traversable deck, destructible pier, and openable gate"),
		CinderDeck != nullptr && !CinderDeck->bBlocksMovement && CinderDeck->VentilationPercent == 55
		&& CinderPier != nullptr && CinderPier->bBlocksMovement && CinderPier->MaxIntegrity == 150
		&& CinderPier->BlastResistancePercent == 60
		&& ThermalGate != nullptr && ThermalGate->IsDoor() && ThermalGate->DoorActionPointCost == 2
		&& ThermalGate->VentilationPercent == 80);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMultiRootContentCatalogTest,
	"UEGT.Core.Content.Catalog.MultiRootSampleMod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMultiRootContentCatalogTest::RunTest(const FString& Parameters)
{
	const FString RulesDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
	const FString SampleModsDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples"), TEXT("Mods"));
	const FString NormalizedRulesAlias = FPaths::Combine(
		RulesDirectory, TEXT(".."), TEXT("Rules"));
	const FContentCatalogLoadResult Result = FContentPackageCatalog::LoadDirectories(
		{ SampleModsDirectory, RulesDirectory, NormalizedRulesAlias });
	TestTrue(TEXT("Base content and the original sample mod load as one strict catalog"), Result.bSucceeded);
	TestEqual(TEXT("Repeated source roots do not load package files twice"), Result.LoadedFiles.Num(), 2);
	TestEqual(TEXT("Resolved catalog contains the base package and one user package"), Result.Packages.Num(), 2);
	if (Result.Packages.Num() == 2)
	{
		TestEqual(TEXT("Dependency resolution places the base package first"),
			Result.Packages[0].Descriptor.PackageId, FName(TEXT("uegt.base")));
		TestEqual(TEXT("The sample package follows its required base dependency"),
			Result.Packages[1].Descriptor.PackageId, FName(TEXT("sample.aurora-relay")));
	}
	const FItemRule* AuroraRelay = Result.RuleSet.Items.Find(TEXT("item.aurora-relay"));
	TestTrue(TEXT("The sample mod contributes a usable original item rule"), AuroraRelay != nullptr
		&& AuroraRelay->DisplayName == TEXT("Aurora Relay")
		&& AuroraRelay->PurchaseCost == 24000
		&& AuroraRelay->ManufactureHours == 20
		&& AuroraRelay->RequiredResearch == TArray<FName>{ FName(TEXT("research.signal-analysis")) });
	const FName* AuroraOrigin = Result.RuleSet.ItemOrigins.Find(TEXT("item.aurora-relay"));
	TestTrue(TEXT("Resolved provenance identifies the user package exactly"),
		AuroraOrigin != nullptr && *AuroraOrigin == FName(TEXT("sample.aurora-relay")));

	const FString MissingDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("missing-user-mod-root"));
	const FContentCatalogLoadResult MissingRoot = FContentPackageCatalog::LoadDirectories(
		{ RulesDirectory, MissingDirectory });
	TestTrue(TEXT("An explicitly requested missing user root fails the entire load atomically"),
		!MissingRoot.bSucceeded && MissingRoot.HasDiagnostic(TEXT("content_directory_missing"))
		&& MissingRoot.LoadedFiles.IsEmpty() && MissingRoot.Packages.IsEmpty()
		&& MissingRoot.RuleSet.Items.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMissingContentCatalogTest,
	"UEGT.Core.Content.Catalog.MissingDirectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMissingContentCatalogTest::RunTest(const FString& Parameters)
{
	const FString MissingDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("definitely-not-a-content-catalog"));
	const FContentCatalogLoadResult Result = FContentPackageCatalog::LoadDirectory(MissingDirectory);
	TestFalse(TEXT("Missing catalog fails cleanly"), Result.bSucceeded);
	TestTrue(TEXT("Missing catalog has a stable diagnostic"), Result.HasDiagnostic(TEXT("content_directory_missing")));
	TestTrue(TEXT("Failed catalog exposes no packages"), Result.Packages.IsEmpty());
	TestTrue(TEXT("Failed catalog exposes no partial rules"), Result.RuleSet.Items.IsEmpty()
		&& Result.RuleSet.Research.IsEmpty() && Result.RuleSet.ArchiveEntries.IsEmpty()
		&& Result.RuleSet.Facilities.IsEmpty() && Result.RuleSet.PersonnelRoles.IsEmpty()
		&& Result.RuleSet.PersonnelDoctrines.IsEmpty() && Result.RuleSet.PersonnelCommendations.IsEmpty()
		&& Result.RuleSet.Craft.IsEmpty() && Result.RuleSet.Contacts.IsEmpty() && Result.RuleSet.Regions.IsEmpty()
		&& Result.RuleSet.AdversaryMissions.IsEmpty() && Result.RuleSet.TacticalTerrains.IsEmpty()
		&& Result.RuleSet.TacticalUnits.IsEmpty() && Result.RuleSet.TacticalMissions.IsEmpty());
	return true;
}

#endif
