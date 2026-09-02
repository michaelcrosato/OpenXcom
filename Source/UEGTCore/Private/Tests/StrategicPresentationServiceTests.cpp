// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/StrategicPresentationService.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelServiceHistoryProjectionTest,
	"UEGT.Core.Strategic.Presentation.PersonnelServiceHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelServiceHistoryProjectionTest::RunTest(const FString& Parameters)
{
	struct FExpectation
	{
		int32 Missions;
		EPersonnelServiceBand Band;
		EPersonnelServiceBand NextBand;
		int32 NextMissions;
		int32 Remaining;
		bool bMaximum;
	};
	const FExpectation Expectations[] = {
		{ -3, EPersonnelServiceBand::FirstWatch, EPersonnelServiceBand::FieldProven, 5, 5, false },
		{ 0, EPersonnelServiceBand::FirstWatch, EPersonnelServiceBand::FieldProven, 5, 5, false },
		{ 4, EPersonnelServiceBand::FirstWatch, EPersonnelServiceBand::FieldProven, 5, 1, false },
		{ 5, EPersonnelServiceBand::FieldProven, EPersonnelServiceBand::LongWatch, 10, 5, false },
		{ 9, EPersonnelServiceBand::FieldProven, EPersonnelServiceBand::LongWatch, 10, 1, false },
		{ 10, EPersonnelServiceBand::LongWatch, EPersonnelServiceBand::LegacyAnchor, 20, 10, false },
		{ 19, EPersonnelServiceBand::LongWatch, EPersonnelServiceBand::LegacyAnchor, 20, 1, false },
		{ 20, EPersonnelServiceBand::LegacyAnchor, EPersonnelServiceBand::EnduringBeacon, 40, 20, false },
		{ 39, EPersonnelServiceBand::LegacyAnchor, EPersonnelServiceBand::EnduringBeacon, 40, 1, false },
		{ 40, EPersonnelServiceBand::EnduringBeacon, EPersonnelServiceBand::EnduringBeacon, 40, 0, true },
		{ 73, EPersonnelServiceBand::EnduringBeacon, EPersonnelServiceBand::EnduringBeacon, 40, 0, true }
	};
	for (const FExpectation& Expected : Expectations)
	{
		const FPersonnelServiceHistoryView View = FPersonnelServiceHistory::Project(Expected.Missions);
		TestTrue(*FString::Printf(TEXT("Mission boundary %d projects the exact service band"), Expected.Missions),
			View.Band == Expected.Band && View.NextBand == Expected.NextBand
			&& View.NextBandMissions == Expected.NextMissions
			&& View.MissionsUntilNextBand == Expected.Remaining
			&& View.bMaximumBand == Expected.bMaximum);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelRecoveryPlanProjectionTest,
	"UEGT.Core.Strategic.Presentation.PersonnelReturnPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelRecoveryPlanProjectionTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FStrategicSimulationConfig Config;
	Config.RecoverySurgeCostPerMissingHealth = 2000;
	Config.RecoverySurgeDurationPercent = 50;
	Config.RecoveryReflectionDurationPercent = 150;
	Config.RecoveryReflectionResolveBonus = 1;
	FCampaignState Campaign;
	Campaign.Funds = 19999;
	FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
	Person.PersonnelId = FGuid(0x7a510001, 0x7a510002, 0x7a510003, 0x7a510004);
	Person.DisplayName = TEXT("Maelle Venn");
	Person.RoleId = TEXT("role.field-agent");
	Person.Status = EPersonnelStatus::Recovering;
	Person.MaxHealth = 55;
	Person.CurrentHealth = 45;
	Person.Resolve = 51;
	Person.RemainingRecoverySeconds = int64(10) * 3600;
	Person.RecoveryPlan = EPersonnelRecoveryPlan::DecisionRequired;

	const FStrategicDashboardSnapshot Unfunded =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, Config);
	TestTrue(TEXT("Pending Return Path controls strategic dashboard availability"),
		Unfunded.bSucceeded && Unfunded.bDecisionRequired && !Unfunded.bCanAdvanceTime
		&& Unfunded.Personnel.Num() == 1
		&& Unfunded.Personnel[0].RecoveryPlan.bRecovering
		&& Unfunded.Personnel[0].RecoveryPlan.bDecisionRequired
		&& Unfunded.Personnel[0].RecoveryPlan.Options.Num() == 3);
	if (Unfunded.Personnel.Num() == 1 && Unfunded.Personnel[0].RecoveryPlan.Options.Num() == 3)
	{
		const TArray<FPersonnelRecoveryPlanOptionView>& Options =
			Unfunded.Personnel[0].RecoveryPlan.Options;
		TestTrue(TEXT("Immutable Return Path projection preserves stable option order and exact values"),
			Options[0].Plan == EPersonnelRecoveryPlan::MeasuredReturn
			&& Options[0].DurationSeconds == int64(10) * 3600 && Options[0].bAvailable
			&& Options[1].Plan == EPersonnelRecoveryPlan::SurgeCare
			&& Options[1].DurationSeconds == int64(5) * 3600
			&& Options[1].FundingCost == 20000 && !Options[1].bAvailable
			&& Options[1].UnavailableReasonCode == FName(TEXT("recovery_surge_unaffordable"))
			&& Options[2].Plan == EPersonnelRecoveryPlan::ReflectionCycle
			&& Options[2].DurationSeconds == int64(15) * 3600
			&& Options[2].ResolveBonus == 1 && Options[2].bAvailable);
	}

	Campaign.Funds = 100000;
	const FStrategicDashboardSnapshot Funded =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, Config);
	TestTrue(TEXT("Surge Care availability updates from campaign funds without mutation"),
		Funded.bSucceeded && Funded.Personnel.Num() == 1
		&& Funded.Personnel[0].RecoveryPlan.Options.Num() == 3
		&& Funded.Personnel[0].RecoveryPlan.Options[1].bAvailable
		&& Campaign.Personnel[0].RecoveryPlan == EPersonnelRecoveryPlan::DecisionRequired
		&& Campaign.Personnel[0].RemainingRecoverySeconds == int64(10) * 3600);

	Campaign.Personnel[0].RecoveryPlan = EPersonnelRecoveryPlan::ReflectionCycle;
	Campaign.Personnel[0].RemainingRecoverySeconds = int64(15) * 3600;
	const FStrategicDashboardSnapshot Active =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, Config);
	TestTrue(TEXT("Committed Return Path becomes a non-blocking active recovery projection"),
		Active.bSucceeded && !Active.bDecisionRequired && Active.bCanAdvanceTime
		&& Active.Personnel.Num() == 1
		&& Active.Personnel[0].RecoveryPlan.bRecovering
		&& !Active.Personnel[0].RecoveryPlan.bDecisionRequired
		&& Active.Personnel[0].RecoveryPlan.SelectedPlan == EPersonnelRecoveryPlan::ReflectionCycle
		&& Active.Personnel[0].RecoveryPlan.SelectedPolicyId
			== FName(TEXT("personnel.recovery-reflection-cycle"))
		&& Active.Personnel[0].RecoveryPlan.Options.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelStewardshipProjectionTest,
	"UEGT.Core.Strategic.Presentation.PersonnelStewardship",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelStewardshipProjectionTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FPersonnelRoleRule FieldAgent;
	FieldAgent.Identity.RuleId = TEXT("role.field-agent");
	FieldAgent.DisplayName = TEXT("Field Agent");
	FieldAgent.Category = EPersonnelRoleCategory::FieldAgent;
	FieldAgent.MonthlySalary = 500;
	Rules.PersonnelRoles.Add(FieldAgent.Identity.RuleId, FieldAgent);
	FStrategicSimulationConfig Config;
	Config.StewardshipDurationDays = 30;
	Config.StewardshipMinimumMissions = 10;
	Config.StewardshipReductionPercent = 25;
	Config.StewardshipResolveBonus = 1;
	Config.StewardshipResolveAwardTourCap = 3;
	FCampaignState Campaign;
	FStrategicBaseState& Base = Campaign.Bases.AddDefaulted_GetRef();
	Base.BaseId = FGuid(0x57e00001, 0x57e00002, 0x57e00003, 0x57e00004);
	Base.Name = TEXT("Northwatch");
	Base.RegionId = TEXT("region.cascadia");
	const FGuid PersonnelId(0x57e00011, 0x57e00012, 0x57e00013, 0x57e00014);
	FPersonnelState& Veteran = Campaign.Personnel.AddDefaulted_GetRef();
	Veteran.PersonnelId = PersonnelId;
	Veteran.DisplayName = TEXT("Aster Vale");
	Veteran.RoleId = FieldAgent.Identity.RuleId;
	Veteran.BaseId = Base.BaseId;
	Veteran.Missions = 12;
	Veteran.MaxHealth = 55;
	Veteran.CurrentHealth = 55;
	Veteran.Accuracy = 60;
	Veteran.Resolve = 70;
	Veteran.Mobility = 58;
	Veteran.Strength = 57;
	Veteran.StewardshipToursCompleted = 1;
	FMemorialRecord& Memorial = Campaign.Memorial.AddDefaulted_GetRef();
	Memorial.PersonnelId = FGuid(0x57e00021, 0x57e00022, 0x57e00023, 0x57e00024);
	Memorial.DisplayName = TEXT("Bram Sato");
	Memorial.RoleId = FieldAgent.Identity.RuleId;
	Memorial.Rank = 3;
	Memorial.Missions = 16;
	Memorial.StewardshipToursCompleted = 2;
	Memorial.DeathUtc = Campaign.StrategicTime.Utc;
	Memorial.CauseId = TEXT("cause.field-injury");

	const FStrategicDashboardSnapshot Eligible =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, Config);
	TestTrue(TEXT("Dashboard projects an eligible veteran's stable leadership choices and history"),
		Eligible.bSucceeded && Eligible.Personnel.Num() == 1
		&& Eligible.Personnel[0].Stewardship.bEligible
		&& Eligible.Personnel[0].Stewardship.Options.Num() == 3
		&& Eligible.Personnel[0].Stewardship.DurationSeconds == int64(30) * 86400
		&& Eligible.Personnel[0].Stewardship.ResolveBonusOnCompletion == 1
		&& Eligible.Personnel[0].StewardshipToursCompleted == 1
		&& Eligible.Memorial.Num() == 1
		&& Eligible.Memorial[0].StewardshipToursCompleted == 2);

	Campaign.Personnel[0].Status = EPersonnelStatus::Stewarding;
	Campaign.Personnel[0].StewardshipFocus = EPersonnelStewardshipFocus::TrainingCadre;
	Campaign.Personnel[0].RemainingStewardshipSeconds = int64(12) * 86400;
	const FStrategicDashboardSnapshot Active =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, Config);
	TestTrue(TEXT("Active Stewardship dashboard exposes exact focus, clock, benefit, and projected reward"),
		Active.bSucceeded && Active.Personnel.Num() == 1
		&& Active.Personnel[0].Status == TEXT("Stewarding")
		&& Active.Personnel[0].Stewardship.bBaseHasActiveSteward
		&& Active.Personnel[0].Stewardship.bSelectedPersonnelIsSteward
		&& Active.Personnel[0].Stewardship.ActiveFocus == EPersonnelStewardshipFocus::TrainingCadre
		&& Active.Personnel[0].Stewardship.ActivePolicyId
			== FName(TEXT("personnel.stewardship-training-cadre"))
		&& Active.Personnel[0].Stewardship.RemainingSeconds == int64(12) * 86400
		&& Active.Personnel[0].Stewardship.ReductionPercent == 25
		&& Active.Personnel[0].Stewardship.ToursCompleted == 1
		&& Active.Personnel[0].Stewardship.ResolveBonusOnCompletion == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicBaseSpecializationProjectionTest,
	"UEGT.Core.Strategic.Presentation.BaseSpecializationProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicBaseSpecializationProjectionTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	const auto AddFacilityRule = [&Rules](
		const FName RuleId,
		const int32 DetectionStrength,
		const int32 ScientistCapacity,
		const int32 EngineerCapacity,
		const int32 CraftCapacity,
		const int32 StorageCapacity)
	{
		FFacilityRule Rule;
		Rule.Identity.RuleId = RuleId;
		Rule.DisplayName = RuleId.ToString();
		Rule.DetectionStrength = DetectionStrength;
		Rule.ScientistCapacity = ScientistCapacity;
		Rule.EngineerCapacity = EngineerCapacity;
		Rule.CraftCapacity = CraftCapacity;
		Rule.StorageCapacity = StorageCapacity;
		Rule.MaxIntegrity = 100;
		Rules.Facilities.Add(Rule.Identity.RuleId, Rule);
	};
	AddFacilityRule(TEXT("facility.test-specialization-signal"), 70, 0, 0, 0, 0);
	AddFacilityRule(TEXT("facility.test-specialization-research"), 0, 6, 0, 0, 0);
	AddFacilityRule(TEXT("facility.test-specialization-fabrication"), 0, 0, 6, 0, 0);
	AddFacilityRule(TEXT("facility.test-specialization-flight"), 0, 0, 0, 2, 0);
	AddFacilityRule(TEXT("facility.test-specialization-logistics"), 0, 0, 0, 0, 1200);

	const auto Project = [&Rules](const FName FacilityId, const bool bLegacy, const int32 Damage)
	{
		FCampaignState Campaign;
		Campaign.CommandSequence = 1;
		FStrategicBaseState& Base = Campaign.Bases.AddDefaulted_GetRef();
		Base.BaseId = FGuid(0x5a510001, 0x5a510002, 0x5a510003, 0x5a510004);
		Base.Name = TEXT("Profile Station");
		if (bLegacy)
		{
			Base.BuiltFacilities.Add(FacilityId);
		}
		else
		{
			FBaseFacilityState& Facility = Base.Facilities.AddDefaulted_GetRef();
			Facility.InstanceId = FGuid(0x5a510011, 0x5a510012, 0x5a510013, 0x5a510014);
			Facility.FacilityId = FacilityId;
			Facility.Damage = Damage;
		}
		const FStrategicDashboardSnapshot Snapshot =
			FStrategicPresentationService::BuildDashboard(Campaign, Rules, {});
		return Snapshot.Bases.Num() == 1
			? Snapshot.Bases[0].Specialization
			: FStrategicBaseSpecializationView();
	};

	const FStrategicBaseSpecializationView Signal =
		Project(TEXT("facility.test-specialization-signal"), false, 0);
	const FStrategicBaseSpecializationView Research =
		Project(TEXT("facility.test-specialization-research"), false, 0);
	const FStrategicBaseSpecializationView Fabrication =
		Project(TEXT("facility.test-specialization-fabrication"), false, 0);
	const FStrategicBaseSpecializationView Flight =
		Project(TEXT("facility.test-specialization-flight"), false, 0);
	const FStrategicBaseSpecializationView Logistics =
		Project(TEXT("facility.test-specialization-logistics"), false, 0);
	const FStrategicBaseSpecializationView LegacySignal =
		Project(TEXT("facility.test-specialization-signal"), true, 0);
	const FStrategicBaseSpecializationView DamagedSignal =
		Project(TEXT("facility.test-specialization-signal"), false, 50);
	TestTrue(TEXT("Every capability axis gets a stable specialized projection with its existing output"),
		Signal.bSpecialized
		&& Signal.SpecializationId == FName(TEXT("base.specialization.signal-relay"))
		&& Signal.Score == 70
		&& Signal.BenefitMetricId == FName(TEXT("base.specialization.detection-strength"))
		&& Signal.BenefitValue == 70
		&& Signal.OperationalBenefitMetricId
			== FName(TEXT("base.specialization.relay-channels"))
		&& Signal.OperationalBenefitValue == 1
		&& Research.bSpecialized
		&& Research.SpecializationId == FName(TEXT("base.specialization.research-enclave"))
		&& Research.Score == 60
		&& Research.BenefitMetricId == FName(TEXT("base.specialization.scientist-capacity"))
		&& Research.BenefitValue == 6
		&& Research.OperationalBenefitMetricId
			== FName(TEXT("base.specialization.research-rate"))
		&& Research.OperationalBenefitValue == 20
		&& Fabrication.bSpecialized
		&& Fabrication.SpecializationId == FName(TEXT("base.specialization.fabrication-works"))
		&& Fabrication.Score == 60
		&& Fabrication.BenefitMetricId == FName(TEXT("base.specialization.engineer-capacity"))
		&& Fabrication.BenefitValue == 6
		&& Flight.bSpecialized
		&& Flight.SpecializationId == FName(TEXT("base.specialization.flight-operations"))
		&& Flight.Score == 100
		&& Flight.BenefitMetricId == FName(TEXT("base.specialization.craft-berths"))
		&& Flight.BenefitValue == 2
		&& Logistics.bSpecialized
		&& Logistics.SpecializationId == FName(TEXT("base.specialization.logistics-depot"))
		&& Logistics.Score == 100
		&& Logistics.BenefitMetricId == FName(TEXT("base.specialization.storage-capacity"))
		&& Logistics.BenefitValue == 1200);
	TestTrue(TEXT("Legacy abstract facilities retain the same specialization and damaged output falls back safely"),
		LegacySignal.bSpecialized
		&& LegacySignal.SpecializationId == Signal.SpecializationId
		&& LegacySignal.Score == Signal.Score
		&& LegacySignal.BenefitValue == Signal.BenefitValue
		&& !DamagedSignal.bSpecialized
		&& DamagedSignal.SpecializationId
			== FName(TEXT("base.specialization.integrated-command"))
		&& DamagedSignal.Score == 35
		&& DamagedSignal.SecondaryScore == 0);
	FCampaignState ResearchCampaign;
	FStrategicBaseState& ResearchBase = ResearchCampaign.Bases.AddDefaulted_GetRef();
	ResearchBase.BaseId = FGuid(0x5a520001, 0x5a520002, 0x5a520003, 0x5a520004);
	ResearchBase.Name = TEXT("Research Station");
	FBaseFacilityState& ResearchFacility = ResearchBase.Facilities.AddDefaulted_GetRef();
	ResearchFacility.InstanceId = FGuid(0x5a520011, 0x5a520012, 0x5a520013, 0x5a520014);
	ResearchFacility.FacilityId = TEXT("facility.test-specialization-research");
	FResearchRule ResearchRule;
	ResearchRule.Identity.RuleId = TEXT("research.test-specialization-rate");
	ResearchRule.DisplayName = TEXT("Specialization Rate Study");
	ResearchRule.Effort = 2;
	ResearchRule.RequiredFacilityIds.Add(ResearchFacility.FacilityId);
	Rules.Research.Add(ResearchRule.Identity.RuleId, ResearchRule);
	FResearchProjectState& ResearchProject = ResearchCampaign.ResearchProjects.AddDefaulted_GetRef();
	ResearchProject.ResearchId = ResearchRule.Identity.RuleId;
	ResearchProject.BaseId = ResearchBase.BaseId;
	ResearchProject.AssignedScientists = 1;
	const FStrategicDashboardSnapshot ResearchSnapshot =
		FStrategicPresentationService::BuildDashboard(ResearchCampaign, Rules, {});
	const FStrategicProjectView* ResearchProjectView = ResearchSnapshot.Projects.FindByPredicate(
		[](const FStrategicProjectView& Project)
		{
			return Project.Type == EStrategicProjectType::Research;
		});
	TestTrue(TEXT("Research Enclave exposes its derived throughput in the read model and ETA"),
		ResearchProjectView != nullptr
		&& ResearchSnapshot.Bases.Num() == 1
		&& ResearchSnapshot.Bases[0].Specialization.bSpecialized
		&& ResearchSnapshot.Bases[0].Specialization.SpecializationId
			== FName(TEXT("base.specialization.research-enclave"))
		&& ResearchSnapshot.Bases[0].Specialization.OperationalBenefitValue == 20
		&& ResearchProjectView->ResearchRatePercent == 120
		&& !ResearchProjectView->bPaused
		&& ResearchProjectView->RemainingSeconds == 6000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicPresentationDashboardTest,
	"UEGT.Core.Strategic.Presentation.DashboardActionsAndContactSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicPresentationDashboardTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;

	FFacilityRule Operations;
	Operations.Identity.RuleId = TEXT("facility.operations");
	Operations.DisplayName = TEXT("Operations Node");
	Operations.BuildCost = 100;
	Operations.BuildHours = 24;
	Operations.MonthlyMaintenance = 10;
	Operations.GridWidth = 2;
	Operations.GridHeight = 2;
	Operations.StorageCapacity = 60;
	Operations.ScientistCapacity = 4;
	Operations.SensorRangeKilometers = 900;
	Operations.DetectionStrength = 35;
	Rules.Facilities.Add(Operations.Identity.RuleId, Operations);

	FFacilityRule FlightDeck;
	FlightDeck.Identity.RuleId = TEXT("facility.flight-deck");
	FlightDeck.DisplayName = TEXT("Flight Deck");
	FlightDeck.BuildCost = 150;
	FlightDeck.BuildHours = 48;
	FlightDeck.MonthlyMaintenance = 15;
	FlightDeck.GridWidth = 2;
	FlightDeck.GridHeight = 2;
	FlightDeck.CraftCapacity = 2;
	FlightDeck.EngineerCapacity = 4;
	FlightDeck.MaxIntegrity = 200;
	FlightDeck.RepairCostPerIntegrity = 50;
	FlightDeck.RepairHoursPerIntegrity = 2;
	Rules.Facilities.Add(FlightDeck.Identity.RuleId, FlightDeck);
	FFacilityRule Fabrication;
	Fabrication.Identity.RuleId = TEXT("facility.fabrication");
	Fabrication.DisplayName = TEXT("Fabrication Bay");
	Fabrication.BuildCost = 125;
	Fabrication.BuildHours = 36;
	Fabrication.MonthlyMaintenance = 12;
	Fabrication.EngineerCapacity = 6;
	Rules.Facilities.Add(Fabrication.Identity.RuleId, Fabrication);
	FFacilityRule DefenseBattery;
	DefenseBattery.Identity.RuleId = TEXT("facility.aegis-test");
	DefenseBattery.DisplayName = TEXT("Aegis Test Battery");
	DefenseBattery.BuildCost = 200;
	DefenseBattery.BuildHours = 48;
	DefenseBattery.MonthlyMaintenance = 20;
	DefenseBattery.MaxIntegrity = 150;
	DefenseBattery.RepairCostPerIntegrity = 10;
	DefenseBattery.RepairHoursPerIntegrity = 1;
	DefenseBattery.BaseDefenseAccuracy = 75;
	DefenseBattery.BaseDefenseDamage = 90;
	DefenseBattery.BaseDefenseSupplyItemId = TEXT("item.test-perimeter-capacitor");
	DefenseBattery.BaseDefenseSupplyPerShot = 2;
	Rules.Facilities.Add(DefenseBattery.Identity.RuleId, DefenseBattery);

	FResearchRule ActiveResearch;
	ActiveResearch.Identity.RuleId = TEXT("research.active");
	ActiveResearch.DisplayName = TEXT("Active Study");
	ActiveResearch.Effort = 100;
	ActiveResearch.RequiredFacilityIds.Add(Operations.Identity.RuleId);
	Rules.Research.Add(ActiveResearch.Identity.RuleId, ActiveResearch);
	FResearchRule AvailableResearch;
	AvailableResearch.Identity.RuleId = TEXT("research.available");
	AvailableResearch.DisplayName = TEXT("Available Study");
	AvailableResearch.Effort = 50;
	AvailableResearch.RequiredFacilityIds.Add(Fabrication.Identity.RuleId);
	Rules.Research.Add(AvailableResearch.Identity.RuleId, AvailableResearch);

	FKnowledgeArchiveEntryRule PublicArchiveEntry;
	PublicArchiveEntry.Identity.RuleId = TEXT("archive.public");
	PublicArchiveEntry.DisplayName = TEXT("Public Record");
	PublicArchiveEntry.CategoryId = TEXT("category.command");
	PublicArchiveEntry.Summary = TEXT("An operational record cleared for immediate command review.");
	PublicArchiveEntry.Body = TEXT("Public archive body.");
	PublicArchiveEntry.SortOrder = 20;
	PublicArchiveEntry.RelatedEntryIds.Add(TEXT("archive.gated"));
	Rules.ArchiveEntries.Add(PublicArchiveEntry.Identity.RuleId, PublicArchiveEntry);
	FKnowledgeArchiveEntryRule GatedArchiveEntry;
	GatedArchiveEntry.Identity.RuleId = TEXT("archive.gated");
	GatedArchiveEntry.DisplayName = TEXT("Gated Record");
	GatedArchiveEntry.CategoryId = TEXT("category.science");
	GatedArchiveEntry.Summary = TEXT("A technical record released after the associated study.");
	GatedArchiveEntry.Body = TEXT("Gated archive body.");
	GatedArchiveEntry.SortOrder = 10;
	GatedArchiveEntry.RequiredResearch.Add(AvailableResearch.Identity.RuleId);
	GatedArchiveEntry.RelatedEntryIds.Add(PublicArchiveEntry.Identity.RuleId);
	Rules.ArchiveEntries.Add(GatedArchiveEntry.Identity.RuleId, GatedArchiveEntry);

	FPersonnelRoleRule Scientist;
	Scientist.Identity.RuleId = TEXT("role.scientist");
	Scientist.DisplayName = TEXT("Researcher");
	Scientist.Category = EPersonnelRoleCategory::Scientist;
	Scientist.RecruitmentCost = 80;
	Scientist.MonthlySalary = 20;
	Scientist.RecruitmentHours = 24;
	Rules.PersonnelRoles.Add(Scientist.Identity.RuleId, Scientist);
	FPersonnelRoleRule FieldAgent;
	FieldAgent.Identity.RuleId = TEXT("role.field-agent");
	FieldAgent.DisplayName = TEXT("Field Agent");
	FieldAgent.Category = EPersonnelRoleCategory::FieldAgent;
	Rules.PersonnelRoles.Add(FieldAgent.Identity.RuleId, FieldAgent);
	FPersonnelDoctrineRule AnchorDoctrine;
	AnchorDoctrine.Identity.RuleId = TEXT("doctrine.anchor-test");
	AnchorDoctrine.DisplayName = TEXT("Anchor Test");
	AnchorDoctrine.Summary = TEXT("Improves test health and strength.");
	AnchorDoctrine.MaxSelections = 3;
	AnchorDoctrine.MaxHealthBonus = 2;
	AnchorDoctrine.StrengthBonus = 4;
	Rules.PersonnelDoctrines.Add(AnchorDoctrine.Identity.RuleId, AnchorDoctrine);
	FPersonnelDoctrineRule FocusDoctrine;
	FocusDoctrine.Identity.RuleId = TEXT("doctrine.focus-test");
	FocusDoctrine.DisplayName = TEXT("Focus Test");
	FocusDoctrine.Summary = TEXT("Improves test accuracy.");
	FocusDoctrine.MaxSelections = 3;
	FocusDoctrine.AccuracyBonus = 4;
	Rules.PersonnelDoctrines.Add(FocusDoctrine.Identity.RuleId, FocusDoctrine);
	FPersonnelCommendationRule ServiceCommendation;
	ServiceCommendation.Identity.RuleId = TEXT("commendation.service-test");
	ServiceCommendation.DisplayName = TEXT("Service Test Citation");
	ServiceCommendation.Summary = TEXT("Completed a test service threshold.");
	ServiceCommendation.RequiredMissions = 1;
	Rules.PersonnelCommendations.Add(ServiceCommendation.Identity.RuleId, ServiceCommendation);

	FCraftRule CraftRule;
	CraftRule.Identity.RuleId = TEXT("craft.test");
	CraftRule.DisplayName = TEXT("Test Skiff");
	CraftRule.PurchaseCost = 200;
	CraftRule.MonthlyMaintenance = 5;
	CraftRule.AcquisitionHours = 72;
	CraftRule.MaxHull = 100;
	CraftRule.FuelCapacity = 500;
	CraftRule.AgentCapacity = 4;
	CraftRule.EquipmentSlots = 2;
	CraftRule.RepairCostPerHull = 10;
	CraftRule.RepairHoursPerHull = 1;
	CraftRule.RefuelCostPerUnit = 2;
	CraftRule.RefuelUnitsPerHour = 100;
	Rules.Craft.Add(CraftRule.Identity.RuleId, CraftRule);

	FContactRule ContactRule;
	ContactRule.Identity.RuleId = TEXT("contact.test");
	ContactRule.DisplayName = TEXT("Veil Contact");
	ContactRule.MaxHull = 80;
	ContactRule.ThreatRating = 3;
	Rules.Contacts.Add(ContactRule.Identity.RuleId, ContactRule);
	FStrategicRegionRule RegionRule;
	RegionRule.Identity.RuleId = TEXT("region.test-zone");
	RegionRule.DisplayName = TEXT("Test Assembly");
	RegionRule.CenterLongitudeMilliDegrees = 13000;
	RegionRule.CenterLatitudeMilliDegrees = 35000;
	RegionRule.InitialSupport = 55;
	RegionRule.FundingWeight = 100;
	RegionRule.PressureTolerance = 45;
	Rules.Regions.Add(RegionRule.Identity.RuleId, RegionRule);

	FAdversaryMissionRule Mission;
	Mission.Identity.RuleId = TEXT("mission.test");
	Mission.DisplayName = TEXT("Test Incursion");
	Mission.PlanId = TEXT("plan.mirror-rain");
	Mission.PlanStage = 1;
	Mission.EscapeBranchMissionRuleId = TEXT("mission.nightglass-raid");
	Mission.ThwartBranchMissionRuleId = TEXT("mission.saffron-incursion");
	Mission.CompactPeerSupportLossOnEscape = 8;
	Mission.WithdrawnCompactSupportGainOnThwarted = 10;
	Mission.ContactRuleId = ContactRule.Identity.RuleId;
	Mission.TargetRegionId = TEXT("region.test-zone");
	Mission.DestinationLongitudeMilliDegrees = 12000;
	Mission.DestinationLatitudeMilliDegrees = 34000;
	Mission.bCreatesLandingSiteOnArrival = true;
	Mission.LandingSiteLifetimeHours = 36;
	Mission.LandingSiteThreatBonus = 2;
	Rules.AdversaryMissions.Add(Mission.Identity.RuleId, Mission);
	FAdversaryPlanRule Plan;
	Plan.Identity.RuleId = Mission.PlanId;
	Plan.DisplayName = TEXT("Mirror Rain Pattern");
	Plan.OpeningMissionRuleId = Mission.Identity.RuleId;
	Rules.AdversaryPlans.Add(Plan.Identity.RuleId, Plan);
	FAdversaryMissionRule EscapeBranch = Mission;
	EscapeBranch.Identity.RuleId = Mission.EscapeBranchMissionRuleId;
	EscapeBranch.DisplayName = TEXT("Nightglass Raid");
	EscapeBranch.PlanStage = 2;
	EscapeBranch.EscapeBranchMissionRuleId = NAME_None;
	EscapeBranch.ThwartBranchMissionRuleId = NAME_None;
	Rules.AdversaryMissions.Add(EscapeBranch.Identity.RuleId, EscapeBranch);
	FAdversaryMissionRule ThwartBranch = EscapeBranch;
	ThwartBranch.Identity.RuleId = Mission.ThwartBranchMissionRuleId;
	ThwartBranch.DisplayName = TEXT("Saffron Incursion");
	Rules.AdversaryMissions.Add(ThwartBranch.Identity.RuleId, ThwartBranch);
	FAdversaryMissionRule BaseRaid = Mission;
	BaseRaid.Identity.RuleId = TEXT("mission.test-base-raid");
	BaseRaid.DisplayName = TEXT("Test Perimeter Raid");
	BaseRaid.PlanId = NAME_None;
	BaseRaid.PlanStage = 0;
	BaseRaid.EscapeBranchMissionRuleId = NAME_None;
	BaseRaid.ThwartBranchMissionRuleId = NAME_None;
	BaseRaid.bTargetsPlayerBase = true;
	BaseRaid.BaseFacilityDamage = 35;
	BaseRaid.BaseFacilitiesHit = 2;
	Rules.AdversaryMissions.Add(BaseRaid.Identity.RuleId, BaseRaid);
	FTacticalMissionRule BaseDefenseMission;
	BaseDefenseMission.Identity.RuleId = TEXT("tactical.test-base-defense");
	BaseDefenseMission.Context = ETacticalMissionContext::BaseDefense;
	BaseDefenseMission.SourceContactRuleId = ContactRule.Identity.RuleId;
	BaseDefenseMission.MapWidth = 12;
	BaseDefenseMission.MapHeight = 16;
	BaseDefenseMission.MapLevels = 1;
	Rules.TacticalMissions.Add(BaseDefenseMission.Identity.RuleId, BaseDefenseMission);

	FItemRule DefenseSupply;
	DefenseSupply.Identity.RuleId = DefenseBattery.BaseDefenseSupplyItemId;
	DefenseSupply.DisplayName = TEXT("Test Perimeter Capacitor");
	DefenseSupply.Category = TEXT("base-defense-supply");
	DefenseSupply.Mass = 4;
	Rules.Items.Add(DefenseSupply.Identity.RuleId, DefenseSupply);

	FItemRule RecoveredMaterial;
	RecoveredMaterial.Identity.RuleId = TEXT("item.recovered-alloy");
	RecoveredMaterial.DisplayName = TEXT("Recovered Alloy");
	RecoveredMaterial.Category = TEXT("recovered-material");
	RecoveredMaterial.Mass = 2;
	RecoveredMaterial.SellValue = 7;
	Rules.Items.Add(RecoveredMaterial.Identity.RuleId, RecoveredMaterial);

	FItemRule Manufactured;
	Manufactured.Identity.RuleId = TEXT("item.manufactured");
	Manufactured.DisplayName = TEXT("Signal Relay");
	Manufactured.Category = TEXT("weapon");
	Manufactured.Mass = 5;
	Manufactured.ManufactureCost = 25;
	Manufactured.ManufactureHours = 12;
	Manufactured.SellValue = 12;
	Manufactured.ManufactureInputs.Add({ RecoveredMaterial.Identity.RuleId, 2 });
	Rules.Items.Add(Manufactured.Identity.RuleId, Manufactured);
	FItemRule CraftAmmunition;
	CraftAmmunition.Identity.RuleId = TEXT("item.test-craft-rounds");
	CraftAmmunition.DisplayName = TEXT("Test Craft Rounds");
	CraftAmmunition.Category = TEXT("craft-ammunition");
	CraftAmmunition.Mass = 1;
	Rules.Items.Add(CraftAmmunition.Identity.RuleId, CraftAmmunition);
	FItemRule CraftWeapon;
	CraftWeapon.Identity.RuleId = TEXT("item.test-craft-cannon");
	CraftWeapon.DisplayName = TEXT("Test Craft Cannon");
	CraftWeapon.Category = TEXT("craft-weapon");
	CraftWeapon.Mass = 4;
	CraftWeapon.AmmunitionItemId = CraftAmmunition.Identity.RuleId;
	CraftWeapon.MagazineCapacity = 6;
	CraftWeapon.SalvoSize = 1;
	CraftWeapon.InterceptionAccuracy = 60;
	CraftWeapon.InterceptionDamage = 20;
	CraftWeapon.FireIntervalSeconds = 5;
	Rules.Items.Add(CraftWeapon.Identity.RuleId, CraftWeapon);

	FCampaignState Campaign;
	Campaign.Funds = 1000;
	Campaign.MonthlyFunding = 100;
	Campaign.CommandSequence = 42;
	Campaign.RegionalPressure.Add({ Mission.TargetRegionId, 90 });
	Campaign.AdversaryEscalationLevel = 4;
	Campaign.AdversaryMissionsEscaped = 4;
	Campaign.AdversaryMissionsThwarted = 3;
	FRegionalMandateState& Mandate = Campaign.RegionalMandates.AddDefaulted_GetRef();
	Mandate.RegionId = Mission.TargetRegionId;
	Mandate.Support = 55;
	Mandate.BaselineMonthlyFunding = 100;
	Mandate.CurrentMonthlyFunding = 100;

	const FGuid BaseId(1, 2, 3, 4);
	FStrategicBaseState& Base = Campaign.Bases.AddDefaulted_GetRef();
	Base.BaseId = BaseId;
	Base.Name = TEXT("Test Station");
	Base.RegionId = Mission.TargetRegionId;
	Base.LongitudeMilliDegrees = 11000;
	Base.LatitudeMilliDegrees = 33000;
	Base.ScientistCapacity = 10;
	Base.EngineerCapacity = 10;
	Base.Facilities.Add({ FGuid(11, 12, 13, 14), Operations.Identity.RuleId, 0, 0 });
	Base.Facilities.Add({ FGuid(21, 22, 23, 24), FlightDeck.Identity.RuleId, 2, 0 });
	Base.Facilities[1].Damage = 50;
	Base.Inventory.Add({ Manufactured.Identity.RuleId, 4 });
	Base.Inventory.Add({ RecoveredMaterial.Identity.RuleId, 3 });
	Base.Inventory.Add({ CraftAmmunition.Identity.RuleId, 5 });

	FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
	Person.PersonnelId = FGuid(31, 32, 33, 34);
	Person.BaseId = BaseId;
	Person.DisplayName = TEXT("Rin Vale");
	Person.RoleId = Scientist.Identity.RuleId;
	Person.MaxHealth = 45;
	Person.CurrentHealth = 45;
	Person.Status = EPersonnelStatus::Training;
	Person.Rank = 3;
	Person.Missions = 7;
	Person.Kills = 4;
	Person.Experience = 190;
	Person.Accuracy = 68;
	Person.Resolve = 59;
	Person.Mobility = 63;
	Person.Strength = 55;
	Person.TrainingFocus = EPersonnelTrainingFocus::Resolve;
	Person.RemainingTrainingSeconds = 12 * 3600;
	Person.PendingDoctrineChoices = 1;
	Person.DoctrineSelections.Add(FocusDoctrine.Identity.RuleId);
	Person.Commendations.Add(ServiceCommendation.Identity.RuleId);
	Person.EquippedItems.Add(Manufactured.Identity.RuleId);
	const FGuid RecentMemorialId(35, 36, 37, 38);
	FMemorialRecord& RecentMemorial = Campaign.Memorial.AddDefaulted_GetRef();
	RecentMemorial.PersonnelId = RecentMemorialId;
	RecentMemorial.DisplayName = TEXT("Tao Neris");
	RecentMemorial.RoleId = Scientist.Identity.RuleId;
	RecentMemorial.Rank = 3;
	RecentMemorial.Missions = 10;
	RecentMemorial.Kills = 8;
	RecentMemorial.DoctrineSelections.Add(FocusDoctrine.Identity.RuleId);
	RecentMemorial.Commendations.Add(ServiceCommendation.Identity.RuleId);
	RecentMemorial.DeathUtc = Campaign.StrategicTime.Utc;
	RecentMemorial.CauseId = TEXT("cause.tactical-casualty");
	const FGuid EarlierMemorialId(39, 40, 41, 42);
	FMemorialRecord& EarlierMemorial = Campaign.Memorial.AddDefaulted_GetRef();
	EarlierMemorial.PersonnelId = EarlierMemorialId;
	EarlierMemorial.DisplayName = TEXT("Mara Sol");
	EarlierMemorial.RoleId = Scientist.Identity.RuleId;
	EarlierMemorial.Rank = 5;
	EarlierMemorial.Missions = 40;
	EarlierMemorial.Kills = 13;
	EarlierMemorial.DeathUtc = Campaign.StrategicTime.Utc - FTimespan::FromDays(1);
	EarlierMemorial.CauseId = TEXT("cause.interception-loss");

	FCraftState& Craft = Campaign.Craft.AddDefaulted_GetRef();
	Craft.CraftId = FGuid(41, 42, 43, 44);
	Craft.BaseId = BaseId;
	Craft.DisplayName = TEXT("Relay One");
	Craft.CraftRuleId = CraftRule.Identity.RuleId;
	Craft.CurrentHull = CraftRule.MaxHull;
	Craft.CurrentFuel = CraftRule.FuelCapacity;
	Craft.AssignedPilotId = Person.PersonnelId;
	Craft.AssignedAgentIds.Add(Person.PersonnelId);
	Craft.EquipmentItems.Add(CraftWeapon.Identity.RuleId);
	Craft.EquipmentItems.Add(CraftWeapon.Identity.RuleId);
	Craft.WeaponStates.Add({ CraftWeapon.Identity.RuleId, 4, 0 });
	Craft.Cargo.Add({ RecoveredMaterial.Identity.RuleId, 4 });
	Craft.PendingSalvage.Add({ RecoveredMaterial.Identity.RuleId, 4 });

	FResearchProjectState& Research = Campaign.ResearchProjects.AddDefaulted_GetRef();
	Research.ResearchId = ActiveResearch.Identity.RuleId;
	Research.BaseId = BaseId;
	Research.AssignedScientists = 5;
	Research.AccumulatedWorkSeconds = 90 * 3600;

	FFacilityConstructionProjectState& Construction = Campaign.FacilityConstructionProjects.AddDefaulted_GetRef();
	Construction.ProjectId = FGuid(81, 82, 83, 84);
	Construction.FacilityInstanceId = FGuid(91, 92, 93, 94);
	Construction.BaseId = BaseId;
	Construction.FacilityId = FlightDeck.Identity.RuleId;
	Construction.GridX = 4;
	Construction.GridY = 0;
	Construction.RemainingBuildSeconds = 18 * 3600;

	FManufacturingProjectState& Production = Campaign.ManufacturingProjects.AddDefaulted_GetRef();
	Production.ProjectId = FGuid(95, 96, 97, 98);
	Production.BaseId = BaseId;
	Production.ItemId = Manufactured.Identity.RuleId;
	Production.UnitsRemaining = 3;
	Production.AssignedEngineers = 2;
	Production.AccumulatedWorkSeconds = 1;

	FStrategicContactState& Visible = Campaign.StrategicContacts.AddDefaulted_GetRef();
	Visible.ContactId = FGuid(51, 52, 53, 54);
	Visible.ContactRuleId = ContactRule.Identity.RuleId;
	Visible.Status = EStrategicContactStatus::Detected;
	Visible.OriginLongitudeMilliDegrees = -20000;
	Visible.OriginLatitudeMilliDegrees = 10000;
	Visible.LongitudeMilliDegrees = -10000;
	Visible.LatitudeMilliDegrees = 20000;
	Visible.DestinationLongitudeMilliDegrees = 12000;
	Visible.DestinationLatitudeMilliDegrees = 34000;
	Visible.TotalRouteSeconds = 100;
	Visible.ElapsedRouteSeconds = 25;
	Visible.CurrentHull = 70;
	const FGuid VisibleContactId = Visible.ContactId;
	FStrategicContactState Hidden = Visible;
	Hidden.ContactId = FGuid(61, 62, 63, 64);
	Hidden.Status = EStrategicContactStatus::Hidden;
	Campaign.StrategicContacts.Add(Hidden);
	FAdversaryMissionState& VisibleMission = Campaign.AdversaryMissions.AddDefaulted_GetRef();
	VisibleMission.MissionId = FGuid(55, 56, 57, 58);
	VisibleMission.ContactId = VisibleContactId;
	VisibleMission.MissionRuleId = Mission.Identity.RuleId;
	VisibleMission.StartedUtc = Campaign.StrategicTime.Utc;
	FAdversaryMissionState& HiddenMission = Campaign.AdversaryMissions.AddDefaulted_GetRef();
	HiddenMission.MissionId = FGuid(65, 66, 67, 68);
	HiddenMission.ContactId = Hidden.ContactId;
	HiddenMission.MissionRuleId = Mission.Identity.RuleId;
	HiddenMission.StartedUtc = Campaign.StrategicTime.Utc;
	Campaign.AdversaryMissionsLaunched = 9;

	FStrategicSiteState& Site = Campaign.StrategicSites.AddDefaulted_GetRef();
	Site.SiteId = FGuid(71, 72, 73, 74);
	Site.SourceContactRuleId = ContactRule.Identity.RuleId;
	Site.LongitudeMilliDegrees = 13000;
	Site.LatitudeMilliDegrees = 35000;
	Site.ThreatRating = 3;
	Site.RemainingLifetimeSeconds = 36 * 3600;
	FStrategicSiteState LandingSite = Site;
	LandingSite.SiteId = FGuid(75, 76, 77, 78);
	LandingSite.Type = EStrategicSiteType::Landing;
	LandingSite.ThreatRating = 5;
	LandingSite.RemainingLifetimeSeconds = 18 * 3600;
	Campaign.StrategicSites.Add(LandingSite);

	FStrategicSimulationConfig Config;
	Config.ManufacturingFacilityId = TEXT("facility.fabrication");
	Config.OperationsFacilityId = Operations.Identity.RuleId;
	Config.CivicReliefCost = 100;
	Config.SecurityAccordCost = 200;
	Config.ResilienceCharterMinimumSupport = 55;
	Config.ResilienceCharterCost = 250;
	const FStrategicDashboardSnapshot Snapshot = FStrategicPresentationService::BuildDashboard(Campaign, Rules, Config);
	TestTrue(TEXT("Valid strategic state produces a dashboard"), Snapshot.bSucceeded);
	TestTrue(TEXT("Dashboard exposes the global compact gate before enough charters are signed"),
		!Snapshot.HorizonCompact.bRatified
		&& !Snapshot.HorizonCompact.bEnabled
		&& Snapshot.HorizonCompact.Cost == 400000
		&& Snapshot.HorizonCompact.RequiredCharters == 2
		&& Snapshot.HorizonCompact.SignedCharters == 0
		&& Snapshot.HorizonCompact.MinimumMemberSupport == 50
		&& Snapshot.HorizonCompact.MemberSupportCost == 5
		&& Snapshot.HorizonCompact.FundingPercent == 95
		&& Snapshot.HorizonCompact.SharedEscapePressurePercent == 33
		&& Snapshot.HorizonCompact.WithdrawalSupportThreshold == 25
		&& Snapshot.HorizonCompact.RestorationMinimumSupport == 40
		&& Snapshot.HorizonCompact.ActiveMemberRegionIds.IsEmpty()
		&& Snapshot.HorizonCompact.WithdrawnMemberRegionIds.IsEmpty()
		&& Snapshot.HorizonCompact.UnavailableReasonCode
			== FName(TEXT("coalition_compact_charters_required")));
	TestTrue(TEXT("Archive presentation redacts locked records and links without leaking their bodies"),
		Snapshot.ArchiveTotalCount == 2
		&& Snapshot.ArchiveLockedCount == 1
		&& Snapshot.ArchiveEntries.Num() == 1
		&& Snapshot.ArchiveEntries[0].EntryId == PublicArchiveEntry.Identity.RuleId
		&& Snapshot.ArchiveEntries[0].Body == PublicArchiveEntry.Body
		&& Snapshot.ArchiveEntries[0].RelatedEntryIds.IsEmpty());
	FCampaignState ArchiveUnlockedCampaign = Campaign;
	ArchiveUnlockedCampaign.CompletedResearch.Add(AvailableResearch.Identity.RuleId);
	const FStrategicDashboardSnapshot ArchiveUnlockedSnapshot = FStrategicPresentationService::BuildDashboard(
		ArchiveUnlockedCampaign, Rules, Config);
	TestTrue(TEXT("Completed research deterministically releases archive records and reciprocal links"),
		ArchiveUnlockedSnapshot.bSucceeded
		&& ArchiveUnlockedSnapshot.ArchiveTotalCount == 2
		&& ArchiveUnlockedSnapshot.ArchiveLockedCount == 0
		&& ArchiveUnlockedSnapshot.ArchiveEntries.Num() == 2
		&& ArchiveUnlockedSnapshot.ArchiveEntries[0].EntryId == PublicArchiveEntry.Identity.RuleId
		&& ArchiveUnlockedSnapshot.ArchiveEntries[1].EntryId == GatedArchiveEntry.Identity.RuleId
		&& ArchiveUnlockedSnapshot.ArchiveEntries[0].RelatedEntryIds.Num() == 1
		&& ArchiveUnlockedSnapshot.ArchiveEntries[0].RelatedEntryIds[0] == GatedArchiveEntry.Identity.RuleId
		&& ArchiveUnlockedSnapshot.ArchiveEntries[1].RelatedEntryIds.Num() == 1
		&& ArchiveUnlockedSnapshot.ArchiveEntries[1].RelatedEntryIds[0] == PublicArchiveEntry.Identity.RuleId
		&& ArchiveUnlockedSnapshot.ArchiveEntries[1].Body == GatedArchiveEntry.Body);
	TestEqual(TEXT("Command sequence is retained for UI commands"), Snapshot.ExpectedCommandSequence, int64(42));
	TestTrue(TEXT("Campaign readiness exposes both victory gates, regional failure risk, and deterministic adaptation progress"),
		Snapshot.AdversaryMissionsThwarted == 3
		&& Snapshot.AdversaryResolvedMissions == 7
		&& Snapshot.AdversaryEscalationLevel == 4
		&& Snapshot.VictoryThwartedMissionTarget == 12
		&& Snapshot.VictoryEscalationTarget == 5
		&& Snapshot.HighestRegionalPressure == 90
		&& Snapshot.RegionalCollapsePressureThreshold == 100
		&& Snapshot.ResolvedMissionsUntilNextEscalation == 1
		&& !Snapshot.bAtMaximumAdversaryEscalation);
	FCampaignState MaximumAdaptationCampaign = Campaign;
	MaximumAdaptationCampaign.AdversaryEscalationLevel = Config.MaxAdversaryEscalation;
	MaximumAdaptationCampaign.AdversaryMissionsEscaped = 15;
	MaximumAdaptationCampaign.AdversaryMissionsThwarted = 3;
	const FStrategicDashboardSnapshot MaximumAdaptationSnapshot =
		FStrategicPresentationService::BuildDashboard(MaximumAdaptationCampaign, Rules, Config);
	TestTrue(TEXT("Maximum adaptation is presented as a terminal state without a countdown"),
		MaximumAdaptationSnapshot.bSucceeded
		&& MaximumAdaptationSnapshot.bAtMaximumAdversaryEscalation
		&& MaximumAdaptationSnapshot.ResolvedMissionsUntilNextEscalation == 0);
	FStrategicSimulationConfig InvalidAdaptationConfig = Config;
	InvalidAdaptationConfig.ResolvedMissionsPerEscalationLevel = 0;
	const FStrategicDashboardSnapshot InvalidAdaptationSnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidAdaptationConfig);
	TestTrue(TEXT("Presentation rejects invalid adaptation settings without inventing readiness values"),
		!InvalidAdaptationSnapshot.bSucceeded
		&& InvalidAdaptationSnapshot.Diagnostics.Contains(
			TEXT("Strategic presentation requires valid adversary adaptation, campaign-outcome, regional-policy, and base-defense economy settings.")));
	FStrategicSimulationConfig InvalidDefenseEconomyConfig = Config;
	InvalidDefenseEconomyConfig.BaseDefenseGridOverchargeCostPerThreat = 0;
	const FStrategicDashboardSnapshot InvalidDefenseEconomySnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidDefenseEconomyConfig);
	TestTrue(TEXT("Presentation rejects invalid base-defense economy settings"),
		!InvalidDefenseEconomySnapshot.bSucceeded
		&& !InvalidDefenseEconomySnapshot.Diagnostics.IsEmpty());
	FStrategicSimulationConfig InvalidCrisisConfig = Config;
	InvalidCrisisConfig.CrisisMobilizationPressureReduction =
		InvalidCrisisConfig.CrisisMobilizationMinimumPressure + 1;
	const FStrategicDashboardSnapshot InvalidCrisisSnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidCrisisConfig);
	TestTrue(TEXT("Presentation rejects invalid regional-crisis settings"),
		!InvalidCrisisSnapshot.bSucceeded
		&& !InvalidCrisisSnapshot.Diagnostics.IsEmpty());
	FStrategicSimulationConfig InvalidCharterConfig = Config;
	InvalidCharterConfig.ResilienceCharterEscapePressurePercent = 0;
	const FStrategicDashboardSnapshot InvalidCharterSnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidCharterConfig);
	TestTrue(TEXT("Presentation rejects invalid durable regional-charter settings"),
		!InvalidCharterSnapshot.bSucceeded
		&& !InvalidCharterSnapshot.Diagnostics.IsEmpty());
	FStrategicSimulationConfig InvalidCompactConfig = Config;
	InvalidCompactConfig.HorizonCompactRestorationMinimumSupport =
		InvalidCompactConfig.HorizonCompactWithdrawalSupportThreshold;
	const FStrategicDashboardSnapshot InvalidCompactSnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidCompactConfig);
	TestTrue(TEXT("Presentation rejects compact cohesion without a restoration hysteresis gap"),
		!InvalidCompactSnapshot.bSucceeded
		&& !InvalidCompactSnapshot.Diagnostics.IsEmpty());
	FStrategicSimulationConfig InvalidAidConfig = Config;
	InvalidAidConfig.ReciprocalAidPressureTransfer = 0;
	const FStrategicDashboardSnapshot InvalidAidSnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidAidConfig);
	TestTrue(TEXT("Presentation rejects invalid Reciprocal Aid settings"),
		!InvalidAidSnapshot.bSucceeded
		&& !InvalidAidSnapshot.Diagnostics.IsEmpty());
	FStrategicSimulationConfig InvalidVoteConfig = Config;
	InvalidVoteConfig.HorizonCompactEmergencyVoterSupportCost = 0;
	const FStrategicDashboardSnapshot InvalidVoteSnapshot =
		FStrategicPresentationService::BuildDashboard(Campaign, Rules, InvalidVoteConfig);
	TestTrue(TEXT("Presentation rejects invalid emergency solidarity vote settings"),
		!InvalidVoteSnapshot.bSucceeded
		&& !InvalidVoteSnapshot.Diagnostics.IsEmpty());
	FCampaignState CompactReadyCampaign = Campaign;
	CompactReadyCampaign.Funds = 1000000;
	CompactReadyCampaign.MonthlyFunding = 180;
	CompactReadyCampaign.RegionalMandates[0].Support = 60;
	CompactReadyCampaign.RegionalMandates[0].CurrentMonthlyFunding = 90;
	CompactReadyCampaign.RegionalMandates[0].bResilienceCharterSigned = true;
	CompactReadyCampaign.RegionalPressure.Add({ TEXT("region.partner-zone"), 20 });
	FRegionalMandateState& PartnerMandate =
		CompactReadyCampaign.RegionalMandates.AddDefaulted_GetRef();
	PartnerMandate.RegionId = TEXT("region.partner-zone");
	PartnerMandate.Support = 60;
	PartnerMandate.BaselineMonthlyFunding = 100;
	PartnerMandate.CurrentMonthlyFunding = 90;
	PartnerMandate.bResilienceCharterSigned = true;
	const FStrategicDashboardSnapshot CompactReadySnapshot =
		FStrategicPresentationService::BuildDashboard(CompactReadyCampaign, Rules, Config);
	TestTrue(TEXT("Dashboard projects an affordable two-charter compact exactly"),
		CompactReadySnapshot.bSucceeded
		&& CompactReadySnapshot.HorizonCompact.bEnabled
		&& !CompactReadySnapshot.HorizonCompact.bRatified
		&& CompactReadySnapshot.HorizonCompact.SignedCharters == 2
		&& CompactReadySnapshot.HorizonCompact.MemberRegionIds.Num() == 2
		&& CompactReadySnapshot.HorizonCompact.ActiveMemberRegionIds.Num() == 2
		&& CompactReadySnapshot.HorizonCompact.WithdrawnMemberRegionIds.IsEmpty()
		&& CompactReadySnapshot.HorizonCompact.WithdrawalSupportThreshold == 25
		&& CompactReadySnapshot.HorizonCompact.RestorationMinimumSupport == 40
		&& CompactReadySnapshot.HorizonCompact.CurrentMonthlyFunding == 180
		&& CompactReadySnapshot.HorizonCompact.ProjectedMonthlyFunding == 190
		&& CompactReadySnapshot.HorizonCompact.MonthlyFundingDelta == 10
		&& CompactReadySnapshot.HorizonCompact.AidOptions.Num() == 2
		&& !CompactReadySnapshot.HorizonCompact.AidOptions[0].bEnabled
		&& CompactReadySnapshot.HorizonCompact.AidOptions[0].UnavailableReasonCode
			== FName(TEXT("coalition_aid_compact_required")));
	FCampaignState AidReadyCampaign = CompactReadyCampaign;
	AidReadyCampaign.bHorizonCompactRatified = true;
	AidReadyCampaign.Funds = 600000;
	AidReadyCampaign.MonthlyFunding = 190;
	for (FRegionalMandateState& AidMandate : AidReadyCampaign.RegionalMandates)
	{
		AidMandate.Support = 55;
		AidMandate.CurrentMonthlyFunding = 95;
	}
	const FStrategicDashboardSnapshot AidReadySnapshot =
		FStrategicPresentationService::BuildDashboard(AidReadyCampaign, Rules, Config);
	const FStrategicCoalitionAidView* AidOption =
		AidReadySnapshot.HorizonCompact.AidOptions.FindByPredicate(
			[&Mission](const FStrategicCoalitionAidView& Option)
			{
				return Option.TargetRegionId == Mission.TargetRegionId;
			});
	TestTrue(TEXT("Ratified compact presents an exact command-ready Reciprocal Aid option"),
		AidReadySnapshot.bSucceeded && AidReadySnapshot.HorizonCompact.bRatified
		&& AidReadySnapshot.HorizonCompact.AidOptions.Num() == 2
		&& AidReadySnapshot.HorizonCompact.ActiveMemberRegionIds.Num() == 2
		&& AidReadySnapshot.HorizonCompact.WithdrawnMemberRegionIds.IsEmpty()
		&& AidOption != nullptr && AidOption->bEnabled
		&& AidOption->TargetRegionId == Mission.TargetRegionId
		&& AidOption->DonorRegionId == FName(TEXT("region.partner-zone"))
		&& AidOption->Cost == 150000
		&& AidOption->MinimumTargetPressure == 60
		&& AidOption->MaximumPressureTransfer == 20
		&& AidOption->PressureTransfer == 20
		&& AidOption->TargetCurrentPressure == 90
		&& AidOption->TargetProjectedPressure == 70
		&& AidOption->DonorCurrentPressure == 20
		&& AidOption->DonorProjectedPressure == 40
		&& AidOption->TargetSupportGain == 5
		&& AidOption->DonorSupportCost == 5
		&& !AidOption->bDonorWouldWithdraw
		&& AidOption->MonthlyFundingDelta == 0);
	FCampaignState WithdrawnCampaign = AidReadyCampaign;
	WithdrawnCampaign.MonthlyFunding = 185;
	FRegionalMandateState* WithdrawnMandate = WithdrawnCampaign.RegionalMandates.FindByPredicate(
		[](const FRegionalMandateState& Entry)
		{
			return Entry.RegionId == FName(TEXT("region.partner-zone"));
		});
	if (WithdrawnMandate != nullptr)
	{
		WithdrawnMandate->Support = 40;
		WithdrawnMandate->CurrentMonthlyFunding = 90;
		WithdrawnMandate->bHorizonCompactMemberWithdrawn = true;
	}
	const FStrategicDashboardSnapshot WithdrawnSnapshot =
		FStrategicPresentationService::BuildDashboard(WithdrawnCampaign, Rules, Config);
	const FStrategicRegionView* WithdrawnRegion = WithdrawnSnapshot.Regions.FindByPredicate(
		[](const FStrategicRegionView& Region)
		{
			return Region.RegionId == FName(TEXT("region.partner-zone"));
		});
	TestTrue(TEXT("Dashboard separates withdrawn members, excludes them from aid, and exposes exact restoration economics"),
		WithdrawnSnapshot.bSucceeded
		&& WithdrawnMandate != nullptr
		&& WithdrawnSnapshot.HorizonCompact.MemberRegionIds.Num() == 2
		&& WithdrawnSnapshot.HorizonCompact.ActiveMemberRegionIds.Num() == 1
		&& WithdrawnSnapshot.HorizonCompact.ActiveMemberRegionIds[0]
			== Mission.TargetRegionId
		&& WithdrawnSnapshot.HorizonCompact.WithdrawnMemberRegionIds.Num() == 1
		&& WithdrawnSnapshot.HorizonCompact.WithdrawnMemberRegionIds[0]
			== FName(TEXT("region.partner-zone"))
		&& WithdrawnSnapshot.HorizonCompact.AidOptions.Num() == 1
		&& WithdrawnSnapshot.HorizonCompact.AidOptions[0].TargetRegionId
			== Mission.TargetRegionId
		&& WithdrawnRegion != nullptr
		&& WithdrawnRegion->HorizonCompactRestoration.bWithdrawn
		&& WithdrawnRegion->HorizonCompactRestoration.bEnabled
		&& WithdrawnRegion->HorizonCompactRestoration.Cost == 100000
		&& WithdrawnRegion->HorizonCompactRestoration.CurrentSupport == 40
		&& WithdrawnRegion->HorizonCompactRestoration.MinimumSupport == 40
		&& WithdrawnRegion->HorizonCompactRestoration.CurrentMonthlyFunding == 185
		&& WithdrawnRegion->HorizonCompactRestoration.ProjectedMonthlyFunding == 190
		&& WithdrawnRegion->HorizonCompactRestoration.MonthlyFundingDelta == 5
		&& WithdrawnRegion->HorizonCompactEmergencyVote.bTargetWithdrawn
		&& !WithdrawnRegion->HorizonCompactEmergencyVote.bEnabled
		&& WithdrawnRegion->HorizonCompactEmergencyVote.Cost == 200000
		&& WithdrawnRegion->HorizonCompactEmergencyVote.RequiredVotes == 1
		&& WithdrawnRegion->HorizonCompactEmergencyVote.SupportingMemberRegionIds.IsEmpty()
		&& WithdrawnRegion->HorizonCompactEmergencyVote.OpposingMemberRegionIds.Num() == 1
		&& WithdrawnRegion->HorizonCompactEmergencyVote.UnavailableReasonCode
			== FName(TEXT("coalition_emergency_vote_rejected")));
	FCampaignState VoteReadyCampaign = WithdrawnCampaign;
	VoteReadyCampaign.MonthlyFunding = 163;
	FRegionalMandateState* VoteTarget = VoteReadyCampaign.RegionalMandates.FindByPredicate(
		[](const FRegionalMandateState& Entry)
		{
			return Entry.RegionId == FName(TEXT("region.partner-zone"));
		});
	FRegionalPressureState* VoteTargetPressure =
		VoteReadyCampaign.RegionalPressure.FindByPredicate(
			[](const FRegionalPressureState& Entry)
			{
				return Entry.RegionId == FName(TEXT("region.partner-zone"));
			});
	FRegionalPressureState* VoteMemberPressure =
		VoteReadyCampaign.RegionalPressure.FindByPredicate(
			[&Mission](const FRegionalPressureState& Entry)
			{
				return Entry.RegionId == Mission.TargetRegionId;
			});
	if (VoteTarget != nullptr)
	{
		VoteTarget->Support = 30;
		VoteTarget->CurrentMonthlyFunding = 68;
	}
	if (VoteTargetPressure != nullptr)
	{
		VoteTargetPressure->Pressure = 20;
	}
	if (VoteMemberPressure != nullptr)
	{
		VoteMemberPressure->Pressure = 70;
	}
	const FStrategicDashboardSnapshot VoteReadySnapshot =
		FStrategicPresentationService::BuildDashboard(VoteReadyCampaign, Rules, Config);
	const FStrategicRegionView* VoteReadyRegion = VoteReadySnapshot.Regions.FindByPredicate(
		[](const FStrategicRegionView& Region)
		{
			return Region.RegionId == FName(TEXT("region.partner-zone"));
		});
	TestTrue(TEXT("Dashboard exposes a command-ready majority ballot and exact recovery projection"),
		VoteReadySnapshot.bSucceeded && VoteTarget != nullptr
		&& VoteTargetPressure != nullptr && VoteMemberPressure != nullptr
		&& VoteReadyRegion != nullptr
		&& VoteReadyRegion->HorizonCompactEmergencyVote.bTargetWithdrawn
		&& VoteReadyRegion->HorizonCompactEmergencyVote.bEnabled
		&& VoteReadyRegion->HorizonCompactEmergencyVote.RequiredVotes == 1
		&& VoteReadyRegion->HorizonCompactEmergencyVote.SupportingMemberRegionIds.Num() == 1
		&& VoteReadyRegion->HorizonCompactEmergencyVote.SupportingMemberRegionIds[0]
			== Mission.TargetRegionId
		&& VoteReadyRegion->HorizonCompactEmergencyVote.OpposingMemberRegionIds.IsEmpty()
		&& VoteReadyRegion->HorizonCompactEmergencyVote.TargetCurrentSupport == 30
		&& VoteReadyRegion->HorizonCompactEmergencyVote.TargetProjectedSupport == 42
		&& VoteReadyRegion->HorizonCompactEmergencyVote.TargetSupportGain == 12
		&& VoteReadyRegion->HorizonCompactEmergencyVote.TargetCurrentPressure == 20
		&& VoteReadyRegion->HorizonCompactEmergencyVote.TargetProjectedPressure == 5
		&& VoteReadyRegion->HorizonCompactEmergencyVote.TargetPressureReduction == 15
		&& VoteReadyRegion->HorizonCompactEmergencyVote.VoterSupportCost == 2
		&& VoteReadyRegion->HorizonCompactEmergencyVote.MaximumVoterPressure == 70
		&& VoteReadyRegion->HorizonCompactEmergencyVote.MonthlyFundingDelta == 22
		&& !VoteReadyRegion->HorizonCompactRestoration.bEnabled);
	FCampaignState AidFractureCampaign = AidReadyCampaign;
	AidFractureCampaign.MonthlyFunding = 167;
	FRegionalMandateState* FragileDonor = AidFractureCampaign.RegionalMandates.FindByPredicate(
		[](const FRegionalMandateState& Entry)
		{
			return Entry.RegionId == FName(TEXT("region.partner-zone"));
		});
	if (FragileDonor != nullptr)
	{
		FragileDonor->Support = 29;
		FragileDonor->CurrentMonthlyFunding = 72;
	}
	const FStrategicDashboardSnapshot AidFractureSnapshot =
		FStrategicPresentationService::BuildDashboard(AidFractureCampaign, Rules, Config);
	const FStrategicCoalitionAidView* AidFractureOption =
		AidFractureSnapshot.HorizonCompact.AidOptions.FindByPredicate(
			[&Mission](const FStrategicCoalitionAidView& Option)
			{
				return Option.TargetRegionId == Mission.TargetRegionId;
			});
	TestTrue(TEXT("Dashboard warns when Reciprocal Aid would withdraw its deterministic donor"),
		AidFractureSnapshot.bSucceeded && FragileDonor != nullptr
		&& AidFractureOption != nullptr && AidFractureOption->bEnabled
		&& AidFractureOption->DonorRegionId == FName(TEXT("region.partner-zone"))
		&& AidFractureOption->bDonorWouldWithdraw
		&& AidFractureOption->MonthlyFundingDelta == -4);
	FCampaignState CrisisFractureCampaign = AidFractureCampaign;
	FRegionalMandateState* FragileCrisisMember =
		CrisisFractureCampaign.RegionalMandates.FindByPredicate(
			[&Mission](const FRegionalMandateState& Entry)
			{
				return Entry.RegionId == Mission.TargetRegionId;
			});
	if (FragileCrisisMember != nullptr)
	{
		FragileCrisisMember->Support = 30;
		FragileCrisisMember->CurrentMonthlyFunding = 72;
		CrisisFractureCampaign.MonthlyFunding = 144;
	}
	const FStrategicDashboardSnapshot CrisisFractureSnapshot =
		FStrategicPresentationService::BuildDashboard(CrisisFractureCampaign, Rules, Config);
	const FStrategicRegionView* CrisisRegion = CrisisFractureSnapshot.Regions.FindByPredicate(
		[&Mission](const FStrategicRegionView& Region)
		{
			return Region.RegionId == Mission.TargetRegionId;
		});
	const FStrategicRegionalActionView* CrisisOption = CrisisRegion != nullptr
		? CrisisRegion->ActionOptions.FindByPredicate(
			[](const FStrategicRegionalActionView& Option)
			{
				return Option.ActionType == ERegionalDiplomacyActionType::CrisisMobilization;
			})
		: nullptr;
	TestTrue(TEXT("Dashboard warns before Crisis Mobilization crosses compact cohesion"),
		CrisisFractureSnapshot.bSucceeded && FragileCrisisMember != nullptr
		&& CrisisOption != nullptr && CrisisOption->bEnabled
		&& CrisisOption->bWouldWithdrawCompactMember);
	TestEqual(TEXT("Region pressure becomes one named region"), Snapshot.Regions.Num(), 1);
	TestEqual(TEXT("Region longitude comes from the authored mandate center"), Snapshot.Regions[0].LongitudeMilliDegrees, 13000);
	TestTrue(TEXT("Region presentation joins support, funding tier, all outreach previews, and durable charter"),
		Snapshot.Regions[0].DisplayName == FString(TEXT("Test Assembly"))
		&& Snapshot.Regions[0].bHasMandate
		&& Snapshot.Regions[0].Support == 55
		&& Snapshot.Regions[0].SupportTier == ERegionalSupportTier::Committed
		&& Snapshot.Regions[0].BaselineMonthlyFunding == 100
		&& Snapshot.Regions[0].CurrentMonthlyFunding == 100
		&& Snapshot.Regions[0].ProjectedMonthlyFunding == 100
		&& Snapshot.Regions[0].ActionOptions.Num() == 3
		&& Snapshot.Regions[0].ActionOptions[0].ActionType == ERegionalDiplomacyActionType::CivicRelief
		&& Snapshot.Regions[0].ActionOptions[0].bEnabled
		&& Snapshot.Regions[0].ActionOptions[0].Cost == 100
		&& Snapshot.Regions[0].ActionOptions[0].SupportDelta == 12
		&& Snapshot.Regions[0].ActionOptions[1].ActionType == ERegionalDiplomacyActionType::SecurityAccord
		&& Snapshot.Regions[0].ActionOptions[1].bEnabled
		&& Snapshot.Regions[0].ActionOptions[1].Cost == 200
		&& Snapshot.Regions[0].ActionOptions[1].SupportDelta == 5
		&& Snapshot.Regions[0].ActionOptions[2].ActionType == ERegionalDiplomacyActionType::CrisisMobilization
		&& Snapshot.Regions[0].ActionOptions[2].bEnabled
		&& Snapshot.Regions[0].ActionOptions[2].Cost == 0
		&& Snapshot.Regions[0].ActionOptions[2].SupportDelta == -15
		&& Snapshot.Regions[0].ActionOptions[2].PressureReduction == 25
		&& Snapshot.Regions[0].ActionOptions[2].MinimumPressure == 60
		&& Snapshot.Regions[0].ResilienceCharter.bEnabled
		&& !Snapshot.Regions[0].ResilienceCharter.bSigned
		&& Snapshot.Regions[0].ResilienceCharter.Cost == 250
		&& Snapshot.Regions[0].ResilienceCharter.SupportCost == 10
		&& Snapshot.Regions[0].ResilienceCharter.MinimumSupport == 55
		&& Snapshot.Regions[0].ResilienceCharter.ProjectedMonthlyFunding == 90
		&& Snapshot.Regions[0].ResilienceCharter.MonthlyFundingDelta == -10
		&& Snapshot.Regions[0].ResilienceCharter.MissionWeightPercent == 50
		&& Snapshot.Regions[0].ResilienceCharter.EscapePressurePercent == 75);
	TestEqual(TEXT("Primary base is stable"), Snapshot.PrimaryBaseId, BaseId);
	TestEqual(TEXT("Base facility capacity reaches the read model"), Snapshot.Bases[0].CraftCapacity, 2);
	TestTrue(TEXT("Base personnel capacity combines saved allowances and integrity-scaled facility bonuses"),
		Snapshot.Bases[0].BaseScientistCapacity == 10
		&& Snapshot.Bases[0].FacilityScientistCapacity == 4
		&& Snapshot.Bases[0].ScientistCapacity == 14
		&& Snapshot.Bases[0].ScientistPersonnel == 1
		&& Snapshot.Bases[0].ScientistOverCapacity == 0
		&& Snapshot.Bases[0].BaseEngineerCapacity == 10
		&& Snapshot.Bases[0].FacilityEngineerCapacity == 3
		&& Snapshot.Bases[0].EngineerCapacity == 13
		&& Snapshot.Bases[0].EngineerPersonnel == 0
		&& Snapshot.Bases[0].EngineerOverCapacity == 0);
	TestTrue(TEXT("Base presentation exposes a clear Relay Weave horizon when no convoy is committed"),
		Snapshot.Bases[0].RelayChannelCount == 1
		&& Snapshot.Bases[0].RelayQueueActiveConvoyCount == 0
		&& Snapshot.Bases[0].RelayQueueTotalConvoyCount == 0
		&& Snapshot.Bases[0].RelayQueueWaitingConvoyCount == 0
		&& Snapshot.Bases[0].RelayQueuePressurePercent == 0
		&& Snapshot.Bases[0].RelayQueueTailArrivalSeconds == 0);
	TestTrue(TEXT("Base presentation derives a flight specialization from the strongest operational output"),
		Snapshot.Bases[0].Specialization.bSpecialized
		&& Snapshot.Bases[0].Specialization.SpecializationId
			== FName(TEXT("base.specialization.flight-operations"))
		&& Snapshot.Bases[0].Specialization.Score == 100
		&& Snapshot.Bases[0].Specialization.SecondaryScore == 40
		&& Snapshot.Bases[0].Specialization.BenefitMetricId
			== FName(TEXT("base.specialization.craft-berths"))
		&& Snapshot.Bases[0].Specialization.BenefitValue == 2);
	FCampaignState DegradedSpecializationCampaign = Campaign;
	DegradedSpecializationCampaign.Bases[0].Facilities[1].Damage = FlightDeck.MaxIntegrity;
	const FStrategicDashboardSnapshot DegradedSpecializationSnapshot =
		FStrategicPresentationService::BuildDashboard(
			DegradedSpecializationCampaign, Rules, Config);
	TestTrue(TEXT("A facility outage falls back to an integrated profile without changing campaign state"),
		DegradedSpecializationSnapshot.bSucceeded
		&& DegradedSpecializationSnapshot.Bases.Num() == 1
		&& !DegradedSpecializationSnapshot.Bases[0].Specialization.bSpecialized
		&& DegradedSpecializationSnapshot.Bases[0].Specialization.SpecializationId
			== FName(TEXT("base.specialization.integrated-command"))
		&& DegradedSpecializationSnapshot.Bases[0].Specialization.Score == 40
		&& DegradedSpecializationSnapshot.Bases[0].Specialization.SecondaryScore == 35
		&& Campaign.Bases[0].Facilities[1].Damage == 50);
	FCampaignState OvercapacityCampaign = Campaign;
	OvercapacityCampaign.Bases[0].ScientistCapacity = 0;
	OvercapacityCampaign.Bases[0].EngineerCapacity = 0;
	OvercapacityCampaign.Bases[0].Facilities[0].Damage = Operations.MaxIntegrity;
	OvercapacityCampaign.Bases[0].Facilities[1].Damage = FlightDeck.MaxIntegrity;
	const FStrategicDashboardSnapshot OvercapacitySnapshot = FStrategicPresentationService::BuildDashboard(
		OvercapacityCampaign, Rules, Config);
	TestTrue(TEXT("Forced facility outages expose exact lossless personnel overcapacity"),
		OvercapacitySnapshot.bSucceeded && OvercapacitySnapshot.Bases.Num() == 1
		&& OvercapacitySnapshot.Bases[0].ScientistCapacity == 0
		&& OvercapacitySnapshot.Bases[0].ScientistOverCapacity == 5
		&& OvercapacitySnapshot.Bases[0].EngineerCapacity == 0
		&& OvercapacitySnapshot.Bases[0].EngineerOverCapacity == 2);
	TestTrue(TEXT("Required-facility outages expose an exact paused research presentation"),
		OvercapacitySnapshot.Projects.Num() == 3
		&& OvercapacitySnapshot.Projects[0].Type == EStrategicProjectType::Research
		&& OvercapacitySnapshot.Projects[0].bPaused
		&& OvercapacitySnapshot.Projects[0].RemainingSeconds == 0
		&& OvercapacitySnapshot.Projects[0].RequiredFacilityIds == TArray<FName>{ FName(TEXT("facility.operations")) }
		&& OvercapacitySnapshot.Projects[0].RequiredFacilityNames == TArray<FString>{ FString(TEXT("Operations Node")) }
		&& OvercapacitySnapshot.Projects[0].MissingFacilityIds == TArray<FName>{ FName(TEXT("facility.operations")) }
		&& OvercapacitySnapshot.Projects[0].MissingFacilityNames == TArray<FString>{ FString(TEXT("Operations Node")) }
		&& OvercapacitySnapshot.Projects[0].PauseReason.Contains(TEXT("Operations Node"))
		&& OvercapacitySnapshot.Projects[0].Detail.Contains(TEXT("LAB OFFLINE")));
	TestTrue(TEXT("Mass-weighted storage usage and production reservations reach the base read model"),
		Snapshot.Bases[0].bStorageEnforced
		&& Snapshot.Bases[0].StorageCapacity == 60
		&& Snapshot.Bases[0].StorageUsed == 31
		&& Snapshot.Bases[0].StorageReserved == 15
		&& Snapshot.Bases[0].StorageCommitted == 46
		&& Snapshot.Bases[0].StorageAvailable == 14
		&& Snapshot.Bases[0].StorageOverflow == 0);
	const FStrategicInventoryView* ManufacturedInventory = Snapshot.Bases[0].Inventory.FindByPredicate(
		[&Manufactured](const FStrategicInventoryView& Item) { return Item.ItemId == Manufactured.Identity.RuleId; });
	TestTrue(TEXT("Inventory disposition value reaches the base read model"),
		Snapshot.Bases[0].Inventory.Num() == 3
		&& ManufacturedInventory != nullptr
		&& ManufacturedInventory->Quantity == 4
		&& ManufacturedInventory->UnitSellValue == 12
		&& ManufacturedInventory->UnitStorage == 5
		&& ManufacturedInventory->TotalStorage == 20
		&& ManufacturedInventory->bPersonnelEquippable);
	TestEqual(TEXT("Base grid width reaches the read model"), Snapshot.Bases[0].GridWidth, 8);
	TestEqual(TEXT("Base grid height reaches the read model"), Snapshot.Bases[0].GridHeight, 8);
	TestEqual(TEXT("Operational and constructing facilities share one positioned layout"),
		Snapshot.Bases[0].FacilityLayout.Num(), 3);
	TestTrue(TEXT("Facility layout retains stable positions, dimensions, and construction state"),
		Snapshot.Bases[0].FacilityLayout[0].GridX == 0
		&& Snapshot.Bases[0].FacilityLayout[0].GridWidth == 2
		&& Snapshot.Bases[0].FacilityLayout[0].StorageCapacity == 60
		&& Snapshot.Bases[0].FacilityLayout[0].bOperational
		&& Snapshot.Bases[0].FacilityLayout[2].GridX == 4
		&& !Snapshot.Bases[0].FacilityLayout[2].bOperational
		&& Snapshot.Bases[0].FacilityLayout[2].bConstructing
		&& Snapshot.Bases[0].FacilityLayout[2].RemainingBuildSeconds == 18 * 3600);
	TestTrue(TEXT("Facility layout exposes exact degraded integrity and repair requirements"),
		Snapshot.Bases[0].FacilityLayout[1].bOperational
		&& !Snapshot.Bases[0].FacilityLayout[1].bConstructing
		&& Snapshot.Bases[0].FacilityLayout[1].Damage == 50
		&& Snapshot.Bases[0].FacilityLayout[1].CurrentIntegrity == 150
		&& Snapshot.Bases[0].FacilityLayout[1].MaxIntegrity == 200
		&& Snapshot.Bases[0].FacilityLayout[1].EffectivenessPercent == 75
		&& Snapshot.Bases[0].FacilityLayout[1].EngineerCapacity == 3
		&& Snapshot.Bases[0].FacilityLayout[1].MaximumEngineerCapacity == 4
		&& Snapshot.Bases[0].FacilityLayout[1].CraftCapacity == 2
		&& Snapshot.Bases[0].FacilityLayout[1].MaximumCraftCapacity == 2
		&& !Snapshot.Bases[0].FacilityLayout[1].bCanRepair
		&& Snapshot.Bases[0].FacilityLayout[1].RepairCost == 2500
		&& Snapshot.Bases[0].FacilityLayout[1].RepairDurationSeconds == 100 * 3600
		&& Snapshot.Bases[0].FacilityLayout[1].RepairUnavailableReasonCode == FName(TEXT("insufficient_funds"))
		&& Snapshot.Bases[0].FacilityLayout[1].RepairUnavailableReason.Contains(TEXT("2500")));
	TestTrue(TEXT("Base facility summary marks degraded infrastructure"),
		Snapshot.Bases[0].Facilities.ContainsByPredicate(
			[](const FString& Name)
			{
				return Name.Contains(TEXT("Flight Deck")) && Name.Contains(TEXT("DEGRADED 150/200"));
			}));
	FCampaignState RepairingCampaign = Campaign;
	RepairingCampaign.Bases[0].Facilities[1].ReservedRepairDamage = 50;
	RepairingCampaign.Bases[0].Facilities[1].RemainingRepairSeconds = 90 * 3600;
	const FStrategicDashboardSnapshot RepairingSnapshot = FStrategicPresentationService::BuildDashboard(
		RepairingCampaign, Rules, Config);
	TestTrue(TEXT("Active repair presentation exposes remaining time and full cancellation refund"),
		RepairingSnapshot.Bases[0].FacilityLayout[1].bRepairing
		&& RepairingSnapshot.Bases[0].FacilityLayout[1].RemainingRepairSeconds == 90 * 3600
		&& RepairingSnapshot.Bases[0].FacilityLayout[1].RepairCancellationRefund == 2500
		&& !RepairingSnapshot.Bases[0].FacilityLayout[1].bCanRepair
		&& RepairingSnapshot.Bases[0].FacilityLayout[1].RepairUnavailableReasonCode
			== FName(TEXT("facility_repair_active"))
		&& RepairingSnapshot.Bases[0].FacilityLayout[1].RepairUnavailableReason.Contains(TEXT("active repair")));
	TestTrue(TEXT("Base facility summary marks active repairs"),
		RepairingSnapshot.Bases[0].Facilities.ContainsByPredicate(
			[](const FString& Name)
			{
				return Name.Contains(TEXT("Flight Deck")) && Name.Contains(TEXT("REPAIR 90 h"));
			}));
	TestTrue(TEXT("Facility layout publishes exact dismantling guards for command presentation"),
		!Snapshot.Bases[0].FacilityLayout[0].bCanDismantle
		&& !Snapshot.Bases[0].FacilityLayout[0].DismantleUnavailableReasonCode.IsNone()
		&& Snapshot.Bases[0].FacilityLayout[0].DismantleUnavailableReason.Contains(TEXT("must retain"))
		&& !Snapshot.Bases[0].FacilityLayout[1].bCanDismantle
		&& !Snapshot.Bases[0].FacilityLayout[1].DismantleUnavailableReasonCode.IsNone()
		&& Snapshot.Bases[0].FacilityLayout[1].DismantleUnavailableReason.Contains(TEXT("craft berths")));
	TestEqual(TEXT("Monthly outgoings include facilities, salaries, and craft"), Snapshot.MonthlyOutgoings, int64(50));
	TestEqual(TEXT("Net monthly funding is transparent"), Snapshot.NetMonthlyFunding, int64(50));
	TestEqual(TEXT("Only detected contacts are exposed"), Snapshot.Contacts.Num(), 1);
	TestEqual(TEXT("Visible contact identity is retained"), Snapshot.Contacts[0].ContactId, VisibleContactId);
	TestEqual(TEXT("Visible contact rule identity reaches localization adapters"),
		Snapshot.Contacts[0].ContactRuleId, ContactRule.Identity.RuleId);
	TestEqual(TEXT("Visible contact route progress is computed"), Snapshot.Contacts[0].RouteProgress, 0.25f);
	TestTrue(TEXT("Detected contact exposes localized plan and branch identities"),
		Snapshot.Contacts[0].PlanId == Plan.Identity.RuleId
		&& Snapshot.Contacts[0].PlanDisplayName == Plan.DisplayName
		&& Snapshot.Contacts[0].PlanStage == 1
		&& Snapshot.Contacts[0].EscapeBranchMissionRuleId == EscapeBranch.Identity.RuleId
		&& Snapshot.Contacts[0].EscapeBranchMissionName == EscapeBranch.DisplayName
		&& Snapshot.Contacts[0].ThwartBranchMissionRuleId == ThwartBranch.Identity.RuleId
		&& Snapshot.Contacts[0].ThwartBranchMissionName == ThwartBranch.DisplayName);
	FCampaignState CounterplayCampaign = Campaign;
	CounterplayCampaign.Difficulty = ECampaignDifficulty::Standard;
	CounterplayCampaign.bHorizonCompactRatified = true;
	CounterplayCampaign.RegionalMandates[0].Support = 38;
	CounterplayCampaign.RegionalMandates[0].bResilienceCharterSigned = true;
	CounterplayCampaign.RegionalMandates[0].bHorizonCompactMemberWithdrawn = true;
	FRegionalMandateState& ActivePeer =
		CounterplayCampaign.RegionalMandates.AddDefaulted_GetRef();
	ActivePeer.RegionId = TEXT("region.active-peer");
	ActivePeer.Support = 27;
	ActivePeer.BaselineMonthlyFunding = 100;
	ActivePeer.CurrentMonthlyFunding = 72;
	ActivePeer.bResilienceCharterSigned = true;
	FRegionalMandateState& WithdrawnPeer =
		CounterplayCampaign.RegionalMandates.AddDefaulted_GetRef();
	WithdrawnPeer.RegionId = TEXT("region.withdrawn-peer");
	WithdrawnPeer.Support = 35;
	WithdrawnPeer.BaselineMonthlyFunding = 100;
	WithdrawnPeer.CurrentMonthlyFunding = 68;
	WithdrawnPeer.bResilienceCharterSigned = true;
	WithdrawnPeer.bHorizonCompactMemberWithdrawn = true;
	const FStrategicDashboardSnapshot CounterplaySnapshot =
		FStrategicPresentationService::BuildDashboard(
			CounterplayCampaign, Rules, Config);
	TestTrue(TEXT("Detected authored counterplay exposes exact current-member projections without restoring state"),
		CounterplaySnapshot.bSucceeded && CounterplaySnapshot.Contacts.Num() == 1
		&& CounterplaySnapshot.Contacts[0].bHasCoalitionCounterplay
		&& CounterplaySnapshot.Contacts[0].EscapeStrainMembers.Num() == 1
		&& CounterplaySnapshot.Contacts[0].EscapeStrainMembers[0].RegionId
			== FName(TEXT("region.active-peer"))
		&& CounterplaySnapshot.Contacts[0].EscapeStrainMembers[0].CurrentSupport == 27
		&& CounterplaySnapshot.Contacts[0].EscapeStrainMembers[0].ProjectedSupport == 19
		&& CounterplaySnapshot.Contacts[0].EscapeStrainMembers[0].bWouldWithdraw
		&& CounterplaySnapshot.Contacts[0].ThwartRecoveryMembers.Num() == 2
		&& CounterplaySnapshot.Contacts[0].ThwartRecoveryMembers[0].RegionId
			== Mission.TargetRegionId
		&& CounterplaySnapshot.Contacts[0].ThwartRecoveryMembers[0].CurrentSupport == 38
		&& CounterplaySnapshot.Contacts[0].ThwartRecoveryMembers[0].ProjectedSupport == 48
		&& CounterplaySnapshot.Contacts[0].ThwartRecoveryMembers[0].bRemainsWithdrawn
		&& CounterplayCampaign.RegionalMandates[0].Support == 38
		&& CounterplayCampaign.RegionalMandates[0].bHorizonCompactMemberWithdrawn);
	TestTrue(TEXT("Detected contact exposes both bounded site outcomes without simulation mutation"),
		Snapshot.Contacts[0].bCanShadowToLanding
		&& Snapshot.Contacts[0].WreckageSiteLifetimeSeconds == 72 * 3600LL
		&& Snapshot.Contacts[0].LandingSiteLifetimeSeconds == 36 * 3600LL
		&& Snapshot.Contacts[0].LandingSiteThreatRating == 5);
	FCampaignState EngagedCampaign = Campaign;
	FStrategicContactState* EngagedContact = EngagedCampaign.StrategicContacts.FindByPredicate(
		[&VisibleContactId](const FStrategicContactState& Contact)
		{
			return Contact.ContactId == VisibleContactId;
		});
	if (EngagedContact != nullptr)
	{
		EngagedContact->Status = EStrategicContactStatus::Engaged;
	}
	EngagedCampaign.Craft[0].Status = ECraftStatus::Airborne;
	EngagedCampaign.Craft[0].TargetContactId = VisibleContactId;
	EngagedCampaign.Craft[0].RemainingRouteSeconds = 0;
	EngagedCampaign.Craft[0].ReservedReturnSeconds = 270;
	EngagedCampaign.Craft[0].CurrentHull = 80;
	FCraftState RelayCraft = EngagedCampaign.Craft[0];
	RelayCraft.CraftId = FGuid(0x52454c41, 0x59435241, 1, 2);
	RelayCraft.DisplayName = TEXT("Kestrel Relay");
	RelayCraft.AssignedPilotId.Invalidate();
	RelayCraft.CurrentHull = 25;
	RelayCraft.ReservedReturnSeconds = 420;
	EngagedCampaign.Craft.Add(MoveTemp(RelayCraft));
	const FStrategicDashboardSnapshot EngagedSnapshot =
		FStrategicPresentationService::BuildDashboard(EngagedCampaign, Rules, Config);
	TestTrue(TEXT("Engaged contacts expose three fixed-order command-ready pursuit geometries"),
		EngagedSnapshot.bSucceeded && EngagedSnapshot.Contacts.Num() == 1
		&& EngagedSnapshot.Contacts[0].InterceptionPostures.Num() == 3
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[0].Posture
			== EInterceptionPosture::StandOffScreen
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[0].PolicyId
			== FName(TEXT("interception.stand-off-screen"))
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[0].OutgoingAccuracyModifier == -20
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[0].IncomingAccuracyModifier == -25
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[1].Posture
			== EInterceptionPosture::BalancedVector
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[1].OutgoingAccuracyModifier == 0
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[1].IncomingAccuracyModifier == 0
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[2].Posture
			== EInterceptionPosture::CloseAssault
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[2].PolicyId
			== FName(TEXT("interception.close-assault"))
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[2].OutgoingAccuracyModifier == 20
		&& EngagedSnapshot.Contacts[0].InterceptionPostures[2].IncomingAccuracyModifier == 25
		&& EngagedSnapshot.Contacts[0].InterceptionCraftCount == 2
		&& EngagedSnapshot.Contacts[0].bCanWithdrawInterception);
	TestTrue(TEXT("Engaged multi-craft contacts expose the exact automatic linked-wing benefit"),
		EngagedSnapshot.Contacts[0].InterceptionCoordination.bValid
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.bActive
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.PolicyId
			== FName(TEXT("interception.coordination-linked-wing"))
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.DisplayName == TEXT("Linked Wing")
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.OnStationCraftCount == 2
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.SupportingCraftCount == 1
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.OutgoingAccuracyModifier == 5
		&& EngagedSnapshot.Contacts[0].InterceptionCoordination.IncomingAccuracyModifier == -5);
	TestTrue(TEXT("Engaged opening contacts expose the exact automatic Vector Survey maneuver"),
		EngagedSnapshot.Contacts[0].InterceptionContactManeuver.bValid
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.Maneuver
			== EInterceptionContactManeuver::VectorSurvey
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.PolicyId
			== FName(TEXT("interception.contact-vector-survey"))
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.DisplayName
			== TEXT("Vector Survey")
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.CompletedCombatRounds == 0
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.CurrentHull == 70
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.MaximumHull == 80
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.OutgoingAccuracyModifier == 0
		&& EngagedSnapshot.Contacts[0].InterceptionContactManeuver.IncomingAccuracyModifier == 0);
	FCampaignState SignalShearCampaign = EngagedCampaign;
	FStrategicContactState* SignalShearContact = SignalShearCampaign.StrategicContacts.FindByPredicate(
		[&VisibleContactId](const FStrategicContactState& Contact)
		{
			return Contact.ContactId == VisibleContactId;
		});
	if (SignalShearContact != nullptr)
	{
		SignalShearContact->CompletedCombatRounds = 2;
	}
	const FStrategicDashboardSnapshot SignalShearSnapshot =
		FStrategicPresentationService::BuildDashboard(SignalShearCampaign, Rules, Config);
	TestTrue(TEXT("Two completed rounds above 35 percent expose exact Signal Shear counterplay"),
		SignalShearSnapshot.bSucceeded && SignalShearSnapshot.Contacts.Num() == 1
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.bValid
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.Maneuver
			== EInterceptionContactManeuver::SignalShear
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.PolicyId
			== FName(TEXT("interception.contact-signal-shear"))
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.DisplayName
			== TEXT("Signal Shear")
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.CompletedCombatRounds == 2
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.CurrentHull == 70
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.MaximumHull == 80
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.OutgoingAccuracyModifier == -10
		&& SignalShearSnapshot.Contacts[0].InterceptionContactManeuver.IncomingAccuracyModifier == -15
		&& SignalShearSnapshot.Contacts[0].InterceptionWithdrawals.Num() == 3
		&& SignalShearSnapshot.Contacts[0].InterceptionWithdrawals[2].Doctrine
			== EInterceptionWithdrawalDoctrine::WakeSnare
		&& SignalShearSnapshot.Contacts[0].InterceptionWithdrawals[2].bEnabled
		&& SignalShearSnapshot.Contacts[0].InterceptionWithdrawals[2].CompletedCombatRounds == 2
		&& SignalShearSnapshot.Contacts[0].InterceptionWithdrawals[2].RequiredCombatRounds == 2
		&& SignalShearSnapshot.Contacts[0].InterceptionWithdrawals[2].ContactRouteDelaySeconds == 25);
	FCampaignState BreaklineCampaign = SignalShearCampaign;
	FStrategicContactState* BreaklineContact = BreaklineCampaign.StrategicContacts.FindByPredicate(
		[&VisibleContactId](const FStrategicContactState& Contact)
		{
			return Contact.ContactId == VisibleContactId;
		});
	if (BreaklineContact != nullptr)
	{
		BreaklineContact->CurrentHull = 28;
	}
	const FStrategicDashboardSnapshot BreaklineSnapshot =
		FStrategicPresentationService::BuildDashboard(BreaklineCampaign, Rules, Config);
	TestTrue(TEXT("The inclusive 35-percent boundary overrides round history with Breakline Counter"),
		BreaklineSnapshot.bSucceeded && BreaklineSnapshot.Contacts.Num() == 1
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.bValid
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.Maneuver
			== EInterceptionContactManeuver::BreaklineCounter
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.PolicyId
			== FName(TEXT("interception.contact-breakline-counter"))
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.DisplayName
			== TEXT("Breakline Counter")
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.CompletedCombatRounds == 2
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.CurrentHull == 28
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.MaximumHull == 80
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.OutgoingAccuracyModifier == 10
		&& BreaklineSnapshot.Contacts[0].InterceptionContactManeuver.IncomingAccuracyModifier == 20);
	TestTrue(TEXT("Engaged contacts expose exact fixed-order formation, relay, and Wake Snare projections"),
		EngagedSnapshot.Contacts[0].InterceptionWithdrawals.Num() == 3
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[0].Doctrine
			== EInterceptionWithdrawalDoctrine::FormationBreak
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[0].PolicyId
			== FName(TEXT("interception.withdrawal-formation-break"))
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[0].bEnabled
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[0].OnStationCraftCount == 2
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[0].WithdrawingCraftCount == 2
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[0].RemainingCraftCount == 0
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].Doctrine
			== EInterceptionWithdrawalDoctrine::EvasiveRelay
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].PolicyId
			== FName(TEXT("interception.withdrawal-evasive-relay"))
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].bEnabled
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].WithdrawingCraftCount == 1
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].RemainingCraftCount == 1
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].PriorityCraftId
			== FGuid(0x52454c41, 0x59435241, 1, 2)
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].PriorityCraftDisplayName
			== TEXT("Kestrel Relay")
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].PriorityCraftCurrentHull == 25
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[1].PriorityCraftMaximumHull == 100
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].Doctrine
			== EInterceptionWithdrawalDoctrine::WakeSnare
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].PolicyId
			== FName(TEXT("interception.withdrawal-wake-snare"))
		&& !EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].bEnabled
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].OnStationCraftCount == 2
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].WithdrawingCraftCount == 2
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].RemainingCraftCount == 0
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].CompletedCombatRounds == 0
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].RequiredCombatRounds == 2
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].ContactRouteDelaySeconds == 25
		&& EngagedSnapshot.Contacts[0].InterceptionWithdrawals[2].UnavailableReasonCode
			== FName(TEXT("interception_wake_snare_rounds_required")));
	TestTrue(TEXT("Detected contacts expose no interception withdrawal command"),
		Snapshot.Contacts[0].InterceptionCraftCount == 0
		&& !Snapshot.Contacts[0].bCanWithdrawInterception
		&& !Snapshot.Contacts[0].InterceptionCoordination.bValid
		&& !Snapshot.Contacts[0].InterceptionContactManeuver.bValid
		&& Snapshot.Contacts[0].InterceptionWithdrawals.IsEmpty());
	FCampaignState InvalidWithdrawalCampaign = EngagedCampaign;
	InvalidWithdrawalCampaign.Craft[1].ReservedReturnSeconds = 0;
	const FStrategicDashboardSnapshot InvalidWithdrawalSnapshot =
		FStrategicPresentationService::BuildDashboard(InvalidWithdrawalCampaign, Rules, Config);
	TestTrue(TEXT("Presentation preserves doctrine-specific rejection priority when a craft route is invalid"),
		InvalidWithdrawalSnapshot.Contacts.Num() == 1
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionCraftCount == 2
		&& !InvalidWithdrawalSnapshot.Contacts[0].bCanWithdrawInterception
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals.Num() == 3
		&& !InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals[0].bEnabled
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals[0].UnavailableReasonCode
			== FName(TEXT("invalid_craft_route"))
		&& !InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals[1].bEnabled
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals[1].UnavailableReasonCode
			== FName(TEXT("invalid_craft_route"))
		&& !InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals[2].bEnabled
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionWithdrawals[2].UnavailableReasonCode
			== FName(TEXT("interception_wake_snare_rounds_required"))
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionCoordination.bActive
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionCoordination.OutgoingAccuracyModifier == 5
		&& InvalidWithdrawalSnapshot.Contacts[0].InterceptionCoordination.IncomingAccuracyModifier == -5);
	TestTrue(TEXT("Hidden plan contact contributes no player-visible intelligence"),
		!Snapshot.Contacts.ContainsByPredicate(
			[&Hidden](const FStrategicContactView& Contact)
			{
				return Contact.ContactId == Hidden.ContactId;
			}));
	TestTrue(TEXT("Tactical sites retain typed condition and source identity for localized naming"),
		Snapshot.Sites.Num() == 2
		&& Snapshot.Sites[0].Type == EStrategicSiteType::Wreckage
		&& Snapshot.Sites[0].SourceContactRuleId == ContactRule.Identity.RuleId
		&& Snapshot.Sites[1].Type == EStrategicSiteType::Landing
		&& Snapshot.Sites[1].DisplayName == TEXT("Veil Contact Landing Site"));
	TestEqual(TEXT("Base, craft, visible contact, and both sites receive markers"), Snapshot.GlobeMarkers.Num(), 5);
	TestEqual(TEXT("Only the visible contact contributes an adversary route"), Snapshot.GlobeRoutes.Num(), 1);
	TestTrue(TEXT("Active research reports near-complete progress"),
		Snapshot.Projects.Num() == 3 && Snapshot.Projects[0].Type == EStrategicProjectType::Research
		&& Snapshot.Projects[0].Progress > 0.89f);
	TestTrue(TEXT("Personnel statistics and training state reach the immutable roster view"),
		Snapshot.Personnel.Num() == 1
		&& Snapshot.Personnel[0].RoleId == Person.RoleId
		&& Snapshot.Personnel[0].RoleCategory == EPersonnelRoleCategory::Scientist
		&& Snapshot.Personnel[0].Rank == 3
		&& Snapshot.Personnel[0].Accuracy == 68
		&& Snapshot.Personnel[0].ServiceHistory.Band == EPersonnelServiceBand::FieldProven
		&& Snapshot.Personnel[0].ServiceHistory.NextBand == EPersonnelServiceBand::LongWatch
		&& Snapshot.Personnel[0].ServiceHistory.NextBandMissions == 10
		&& Snapshot.Personnel[0].ServiceHistory.MissionsUntilNextBand == 3
		&& !Snapshot.Personnel[0].ServiceHistory.bMaximumBand
		&& Snapshot.Personnel[0].TrainingFocus == EPersonnelTrainingFocus::Resolve
		&& Snapshot.Personnel[0].RemainingTrainingSeconds == 12 * 3600
		&& Snapshot.Personnel[0].EquippedItemIds == Person.EquippedItems
		&& Snapshot.Personnel[0].EquippedItemNames.Num() == 1
		&& Snapshot.Personnel[0].EquippedItemNames[0] == TEXT("Signal Relay"));
	TestTrue(TEXT("Personnel doctrine choices expose stable rules, levels, bonuses, and authoritative disabled reasons"),
		Snapshot.Personnel[0].PendingDoctrineChoices == 1
		&& Snapshot.Personnel[0].DoctrineOptions.Num() == 2
		&& Snapshot.Personnel[0].DoctrineOptions[0].DoctrineId == AnchorDoctrine.Identity.RuleId
		&& Snapshot.Personnel[0].DoctrineOptions[0].MaxHealthBonus == 2
		&& Snapshot.Personnel[0].DoctrineOptions[0].StrengthBonus == 4
		&& Snapshot.Personnel[0].DoctrineOptions[0].CurrentSelections == 0
		&& Snapshot.Personnel[0].DoctrineOptions[0].MaximumSelections == 3
		&& !Snapshot.Personnel[0].DoctrineOptions[0].bEnabled
		&& Snapshot.Personnel[0].DoctrineOptions[0].UnavailableReasonCode == FName(TEXT("personnel_unavailable"))
		&& Snapshot.Personnel[0].DoctrineOptions[1].DoctrineId == FocusDoctrine.Identity.RuleId
		&& Snapshot.Personnel[0].DoctrineOptions[1].CurrentSelections == 1);
	TestTrue(TEXT("Personnel commendation history exposes authored identity, name, and summary"),
		Snapshot.Personnel[0].Commendations.Num() == 1
		&& Snapshot.Personnel[0].Commendations[0].CommendationId == ServiceCommendation.Identity.RuleId
		&& Snapshot.Personnel[0].Commendations[0].DisplayName == ServiceCommendation.DisplayName
		&& Snapshot.Personnel[0].Commendations[0].Summary == ServiceCommendation.Summary);
	TestTrue(TEXT("Memorial history is newest-first and retains role, cause, milestones, doctrines, and citations"),
		Snapshot.Memorial.Num() == 2
		&& Snapshot.Memorial[0].PersonnelId == RecentMemorialId
		&& Snapshot.Memorial[0].RoleId == Scientist.Identity.RuleId
		&& Snapshot.Memorial[0].RoleDisplayName == Scientist.DisplayName
		&& Snapshot.Memorial[0].CauseId == FName(TEXT("cause.tactical-casualty"))
		&& Snapshot.Memorial[0].CauseDisplayName == TEXT("Tactical Casualty")
		&& Snapshot.Memorial[0].ServiceHistory.Band == EPersonnelServiceBand::LongWatch
		&& Snapshot.Memorial[0].ServiceHistory.NextBandMissions == 20
		&& Snapshot.Memorial[0].ServiceHistory.MissionsUntilNextBand == 10
		&& Snapshot.Memorial[0].DoctrineSelections == TArray<FName>{ FocusDoctrine.Identity.RuleId }
		&& Snapshot.Memorial[0].Commendations.Num() == 1
		&& Snapshot.Memorial[1].PersonnelId == EarlierMemorialId
		&& Snapshot.Memorial[1].ServiceHistory.Band == EPersonnelServiceBand::EnduringBeacon
		&& Snapshot.Memorial[1].ServiceHistory.bMaximumBand);
	FCampaignState DoctrineReadyCampaign = Campaign;
	DoctrineReadyCampaign.Personnel[0].Status = EPersonnelStatus::Available;
	DoctrineReadyCampaign.Personnel[0].RemainingTrainingSeconds = 0;
	const FStrategicDashboardSnapshot DoctrineReadySnapshot = FStrategicPresentationService::BuildDashboard(
		DoctrineReadyCampaign, Rules, Config);
	TestTrue(TEXT("Available promoted personnel receive enabled deterministic doctrine options"),
		DoctrineReadySnapshot.bSucceeded
		&& DoctrineReadySnapshot.Personnel[0].DoctrineOptions.Num() == 2
		&& DoctrineReadySnapshot.Personnel[0].DoctrineOptions[0].bEnabled
		&& DoctrineReadySnapshot.Personnel[0].DoctrineOptions[1].bEnabled);
	TestTrue(TEXT("Explicit craft roster identities reach the immutable fleet view"),
		Snapshot.Craft.Num() == 1
		&& Snapshot.Craft[0].CraftRuleId == CraftRule.Identity.RuleId
		&& Snapshot.Craft[0].AssignedPilotId == Person.PersonnelId
		&& Snapshot.Craft[0].AssignedAgentIds.Num() == 1
		&& Snapshot.Craft[0].AssignedAgentIds[0] == Person.PersonnelId);
	FCampaignState MentorshipCampaign = Campaign;
	FPersonnelState* MentorshipMentor = MentorshipCampaign.Personnel.FindByPredicate(
		[&Person](const FPersonnelState& Entry)
		{
			return Entry.PersonnelId == Person.PersonnelId;
		});
	TestNotNull(TEXT("Mentorship presentation fixture retains its veteran"), MentorshipMentor);
	if (MentorshipMentor != nullptr)
	{
		MentorshipMentor->Missions = 20;
		MentorshipMentor->Rank = 4;
		MentorshipMentor->PendingDoctrineChoices = 0;
		MentorshipMentor->DoctrineSelections = {
			AnchorDoctrine.Identity.RuleId,
			AnchorDoctrine.Identity.RuleId,
			AnchorDoctrine.Identity.RuleId };
		const FGuid MentorshipRecipientId(43, 44, 45, 46);
		FPersonnelState& MentorshipRecipient = MentorshipCampaign.Personnel.AddDefaulted_GetRef();
		MentorshipRecipient.PersonnelId = MentorshipRecipientId;
		MentorshipRecipient.DisplayName = TEXT("Pavel Orin");
		MentorshipRecipient.RoleId = Person.RoleId;
		MentorshipRecipient.BaseId = Person.BaseId;
		MentorshipRecipient.Missions = 8;
		MentorshipCampaign.Craft[0].AssignedAgentIds = { Person.PersonnelId, MentorshipRecipientId };
		FPersonnelSquadBondState& SquadBond = MentorshipCampaign.PersonnelSquadBonds.AddDefaulted_GetRef();
		SquadBond.FirstPersonnelId = Person.PersonnelId;
		SquadBond.SecondPersonnelId = MentorshipRecipientId;
		SquadBond.SharedVictories = 8;
		const FStrategicDashboardSnapshot MentorshipSnapshot =
			FStrategicPresentationService::BuildDashboard(MentorshipCampaign, Rules, Config);
		TestTrue(TEXT("Fleet preview exposes the shared active Legacy Anchor guidance projection"),
			MentorshipSnapshot.bSucceeded
			&& MentorshipSnapshot.Craft.Num() == 1
			&& MentorshipSnapshot.Craft[0].Mentorship.bActive
			&& MentorshipSnapshot.Craft[0].Mentorship.MentorId == Person.PersonnelId
			&& MentorshipSnapshot.Craft[0].Mentorship.MentorServiceHistory.Band
				== EPersonnelServiceBand::LegacyAnchor
			&& MentorshipSnapshot.Craft[0].Mentorship.MoraleBonus == 10
			&& MentorshipSnapshot.Craft[0].Mentorship.RecipientIds
				== TArray<FGuid>{ MentorshipRecipientId });
		TestTrue(TEXT("Fleet preview exposes the same deterministic active Legacy Relay projection"),
			MentorshipSnapshot.Craft[0].LegacyRelay.bActive
			&& MentorshipSnapshot.Craft[0].LegacyRelay.SpecialistId == Person.PersonnelId
			&& MentorshipSnapshot.Craft[0].LegacyRelay.SpecialistServiceHistory.Band
				== EPersonnelServiceBand::LegacyAnchor
			&& MentorshipSnapshot.Craft[0].LegacyRelay.DoctrineId == AnchorDoctrine.Identity.RuleId
			&& MentorshipSnapshot.Craft[0].LegacyRelay.AccuracyBonus == 0
			&& MentorshipSnapshot.Craft[0].LegacyRelay.ResolveBonus == 0
			&& MentorshipSnapshot.Craft[0].LegacyRelay.MobilityBonus == 0
			&& MentorshipSnapshot.Craft[0].LegacyRelay.StrengthBonus == 2
			&& MentorshipSnapshot.Craft[0].LegacyRelay.RecipientIds
				== TArray<FGuid>{ MentorshipRecipientId });
		TestTrue(TEXT("Fleet preview exposes the same deterministic active Field Cadence projection"),
			MentorshipSnapshot.Craft[0].SquadBonds.bActive
			&& MentorshipSnapshot.Craft[0].SquadBonds.ActivePairs.Num() == 1
			&& MentorshipSnapshot.Craft[0].SquadBonds.ActivePairs[0].FirstPersonnelId == Person.PersonnelId
			&& MentorshipSnapshot.Craft[0].SquadBonds.ActivePairs[0].SecondPersonnelId == MentorshipRecipientId
			&& MentorshipSnapshot.Craft[0].SquadBonds.ActivePairs[0].Tier == EPersonnelSquadBondTier::Interlocked
			&& MentorshipSnapshot.Craft[0].SquadBonds.ActivePairs[0].ActionPointBonus == 1
			&& MentorshipSnapshot.Craft[0].SquadBonds.ActivePairs[0].MoraleBonus == 5);
	}
	TestTrue(TEXT("Fleet view exposes exact mounted-weapon and scarce-ammunition readiness"),
		Snapshot.Craft[0].Weapons.Num() == 1
		&& Snapshot.Craft[0].Weapons[0].WeaponItemId == CraftWeapon.Identity.RuleId
		&& Snapshot.Craft[0].Weapons[0].AmmunitionItemId == CraftAmmunition.Identity.RuleId
		&& Snapshot.Craft[0].Weapons[0].MountCount == 2
		&& Snapshot.Craft[0].Weapons[0].LoadedAmmunition == 4
		&& Snapshot.Craft[0].Weapons[0].Capacity == 12
		&& Snapshot.Craft[0].Weapons[0].MissingAmmunition == 8
		&& Snapshot.Craft[0].Weapons[0].BaseAvailableAmmunition == 5
		&& Snapshot.Craft[0].Weapons[0].LoadableAmmunition == 5
		&& Snapshot.Craft[0].TotalAmmunitionLoaded == 4
		&& Snapshot.Craft[0].TotalAmmunitionCapacity == 12
		&& Snapshot.Craft[0].TotalAmmunitionMissing == 8
		&& Snapshot.Craft[0].TotalAmmunitionLoadable == 5);
	TestTrue(TEXT("Pending salvage disables both rearm policies without hiding ammunition state"),
		!Snapshot.Craft[0].bCanRearmFully
		&& !Snapshot.Craft[0].bCanLoadAvailableAmmunition);
	TestTrue(TEXT("Grounded recovery exposes exact retain and sale projections"),
		Snapshot.Craft[0].bSalvageDispositionAvailable
		&& Snapshot.Craft[0].PendingSalvage.Num() == 1
		&& Snapshot.Craft[0].PendingSalvage[0].ItemId == RecoveredMaterial.Identity.RuleId
		&& Snapshot.Craft[0].PendingSalvage[0].DisplayName == RecoveredMaterial.DisplayName
		&& Snapshot.Craft[0].PendingSalvage[0].Quantity == 4
		&& Snapshot.Craft[0].PendingSalvage[0].TotalStorage == 8
		&& Snapshot.Craft[0].PendingSalvage[0].TotalSellValue == 28
		&& Snapshot.Craft[0].PendingSalvage[0].bCanRetainAtBase
		&& Snapshot.Craft[0].PendingSalvage[0].bCanSell);
	FCampaignState ReturningSalvageCampaign = Campaign;
	ReturningSalvageCampaign.Craft[0].Status = ECraftStatus::Returning;
	ReturningSalvageCampaign.Craft[0].RemainingRouteSeconds = 600;
	ReturningSalvageCampaign.Craft[0].ReservedReturnSeconds = 600;
	const FStrategicDashboardSnapshot ReturningSalvageSnapshot =
		FStrategicPresentationService::BuildDashboard(ReturningSalvageCampaign, Rules, Config);
	TestTrue(TEXT("Returning salvage remains visible but its controls wait for landing"),
		ReturningSalvageSnapshot.Craft.Num() == 1
		&& ReturningSalvageSnapshot.Craft[0].PendingSalvage.Num() == 1
		&& !ReturningSalvageSnapshot.Craft[0].bSalvageDispositionAvailable
		&& !ReturningSalvageSnapshot.Craft[0].PendingSalvage[0].bCanRetainAtBase
		&& !ReturningSalvageSnapshot.Craft[0].PendingSalvage[0].bCanSell);
	FCampaignState RearmReadyCampaign = Campaign;
	RearmReadyCampaign.Craft[0].PendingSalvage.Reset();
	const FStrategicDashboardSnapshot RearmReadySnapshot =
		FStrategicPresentationService::BuildDashboard(RearmReadyCampaign, Rules, Config);
	TestTrue(TEXT("Scarce stores enable load-available while keeping full rearm disabled"),
		RearmReadySnapshot.bSucceeded && RearmReadySnapshot.Craft.Num() == 1
		&& !RearmReadySnapshot.Craft[0].bCanRearmFully
		&& RearmReadySnapshot.Craft[0].bCanLoadAvailableAmmunition
		&& RearmReadySnapshot.Craft[0].TotalAmmunitionLoadable == 5
		&& RearmReadySnapshot.Craft[0].TotalAmmunitionMissing == 8);
	FCampaignState ServicingCampaign = RearmReadyCampaign;
	ServicingCampaign.Craft[0].Status = ECraftStatus::Servicing;
	ServicingCampaign.Craft[0].CurrentHull = 98;
	ServicingCampaign.Craft[0].CurrentFuel = 350;
	ServicingCampaign.Craft[0].RemainingRepairSeconds = 2 * 3600;
	ServicingCampaign.Craft[0].RemainingRefuelSeconds = 2 * 3600;
	const FStrategicDashboardSnapshot ServicingSnapshot =
		FStrategicPresentationService::BuildDashboard(ServicingCampaign, Rules, Config);
	TestTrue(TEXT("Active craft service exposes exact component clocks, readiness, and refundable reservation"),
		ServicingSnapshot.bSucceeded && ServicingSnapshot.Craft.Num() == 1
		&& ServicingSnapshot.Craft[0].RemainingRepairSeconds == 2 * 3600
		&& ServicingSnapshot.Craft[0].RemainingRefuelSeconds == 2 * 3600
		&& ServicingSnapshot.Craft[0].RemainingServiceSeconds == 2 * 3600
		&& ServicingSnapshot.Craft[0].ServiceQueue.bValid
		&& ServicingSnapshot.Craft[0].ServiceQueue.PolicyId
			== FName(TEXT("craft.service-rapid-turnaround"))
		&& ServicingSnapshot.Craft[0].ServiceQueue.ServiceLaneCount == 1
		&& ServicingSnapshot.Craft[0].ServiceQueue.QueuePosition == 1
		&& ServicingSnapshot.Craft[0].ServiceQueue.ServiceLaneNumber == 1
		&& ServicingSnapshot.Craft[0].ServiceQueue.bInServiceLane
		&& ServicingSnapshot.Craft[0].ServiceQueue.EstimatedWaitSeconds == 0
		&& ServicingSnapshot.Craft[0].ServiceQueue.EstimatedReadySeconds == 2 * 3600
		&& ServicingSnapshot.Craft[0].ServiceCancellationRefund == 320
		&& ServicingSnapshot.Craft[0].bCanCancelService
		&& !ServicingSnapshot.Craft[0].bCanRearmFully
		&& !ServicingSnapshot.Craft[0].bCanLoadAvailableAmmunition);
	FCampaignState QueuedServiceCampaign = ServicingCampaign;
	FCraftState& ShortServiceCraft = QueuedServiceCampaign.Craft.AddDefaulted_GetRef();
	ShortServiceCraft.CraftId = FGuid(5, 6, 7, 8);
	ShortServiceCraft.BaseId = BaseId;
	ShortServiceCraft.DisplayName = TEXT("Relay Express");
	ShortServiceCraft.CraftRuleId = CraftRule.Identity.RuleId;
	ShortServiceCraft.Status = ECraftStatus::Servicing;
	ShortServiceCraft.CurrentHull = CraftRule.MaxHull - 1;
	ShortServiceCraft.CurrentFuel = CraftRule.FuelCapacity;
	ShortServiceCraft.RemainingRepairSeconds = 3600;
	const FStrategicDashboardSnapshot QueuedServiceSnapshot =
		FStrategicPresentationService::BuildDashboard(QueuedServiceCampaign, Rules, Config);
	const FStrategicCraftView* QueuedCraftView = QueuedServiceSnapshot.Craft.FindByPredicate(
		[&Craft](const FStrategicCraftView& View) { return View.CraftId == Craft.CraftId; });
	const FStrategicCraftView* ActiveCraftView = QueuedServiceSnapshot.Craft.FindByPredicate(
		[&ShortServiceCraft](const FStrategicCraftView& View)
		{
			return View.CraftId == ShortServiceCraft.CraftId;
		});
	TestTrue(TEXT("Presentation distinguishes the active shortest turnaround from an exact queued wait"),
		QueuedCraftView != nullptr && ActiveCraftView != nullptr
		&& !QueuedCraftView->ServiceQueue.bInServiceLane
		&& QueuedCraftView->ServiceQueue.QueuePosition == 2
		&& QueuedCraftView->ServiceQueue.WaitingPosition == 1
		&& QueuedCraftView->ServiceQueue.EstimatedWaitSeconds == 3600
		&& QueuedCraftView->ServiceQueue.EstimatedReadySeconds == 3 * 3600
		&& ActiveCraftView->ServiceQueue.bInServiceLane
		&& ActiveCraftView->ServiceQueue.QueuePosition == 1
		&& ActiveCraftView->ServiceQueue.EstimatedReadySeconds == 3600);
	TestTrue(TEXT("Active research exposes staffing and its operational facility contract"),
		Snapshot.Projects[0].AssignedStaff == 5
		&& !Snapshot.Projects[0].bPaused
		&& Snapshot.Projects[0].RequiredFacilityIds == TArray<FName>{ FName(TEXT("facility.operations")) }
		&& Snapshot.Projects[0].RequiredFacilityNames == TArray<FString>{ FString(TEXT("Operations Node")) }
		&& Snapshot.Projects[0].MissingFacilityNames.IsEmpty()
		&& Snapshot.Projects[0].Detail.Contains(TEXT("lab Operations Node")));
	TestTrue(TEXT("Production view exposes assigned staff and its current cancellation refund"),
		Snapshot.Projects[1].Type == EStrategicProjectType::Manufacturing
		&& Snapshot.Projects[1].AssignedStaff == 2
		&& Snapshot.Projects[1].UnitsRemaining == 3
		&& Snapshot.Projects[1].UnitCost == Manufactured.ManufactureCost
		&& Snapshot.Projects[1].CancellationRefund == 50
		&& Snapshot.Projects[1].MaterialRequirements.Num() == 1
		&& Snapshot.Projects[1].MaterialRequirements[0].ItemId == RecoveredMaterial.Identity.RuleId
		&& Snapshot.Projects[1].MaterialRequirements[0].PerUnitQuantity == 2
		&& Snapshot.Projects[1].MaterialRequirements[0].AvailableQuantity == 3
		&& Snapshot.Projects[1].MaterialRequirements[0].RefundableQuantity == 4
		&& Snapshot.Projects[1].StorageDeltaPerUnit == 1
		&& Snapshot.Projects[1].CancellationStorageDelta == -7
		&& Snapshot.Projects[1].bCanRemoveManufacturingUnit
		&& Snapshot.Projects[1].bCanCancel);
	TestTrue(TEXT("Construction view exposes its progress-based cancellation refund"),
		Snapshot.Projects[2].Type == EStrategicProjectType::Construction
		&& Snapshot.Projects[2].CancellationRefund == 56);

	const FStrategicActionOptionView* ActiveOption = Snapshot.ActionOptions.FindByPredicate(
		[&ActiveResearch](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Research && Option.RuleId == ActiveResearch.Identity.RuleId;
		});
	const FStrategicActionOptionView* AvailableOption = Snapshot.ActionOptions.FindByPredicate(
		[&AvailableResearch](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Research && Option.RuleId == AvailableResearch.Identity.RuleId;
		});
	const FStrategicActionOptionView* FacilityOption = Snapshot.ActionOptions.FindByPredicate(
		[&Operations](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Facility && Option.RuleId == Operations.Identity.RuleId;
		});
	const FStrategicActionOptionView* DefenseOption = Snapshot.ActionOptions.FindByPredicate(
		[&DefenseBattery](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Facility
				&& Option.RuleId == DefenseBattery.Identity.RuleId;
		});
	const FStrategicActionOptionView* ManufacturingOption = Snapshot.ActionOptions.FindByPredicate(
		[&Manufactured](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Manufacturing && Option.RuleId == Manufactured.Identity.RuleId;
		});
	const FStrategicActionOptionView* CraftOption = Snapshot.ActionOptions.FindByPredicate(
		[&CraftRule](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Craft && Option.RuleId == CraftRule.Identity.RuleId;
		});
	TestTrue(TEXT("Active research cannot be started twice"), ActiveOption != nullptr && !ActiveOption->bAvailable
		&& ActiveOption->UnavailableReasonCode == FName(TEXT("research_already_known")));
	TestTrue(TEXT("Unlocked research without its specialist facility is visible but unavailable"),
		AvailableOption != nullptr && !AvailableOption->bAvailable
		&& AvailableOption->RequiredFacilityIds == TArray<FName>{ FName(TEXT("facility.fabrication")) }
		&& AvailableOption->RequiredFacilityNames == TArray<FString>{ FString(TEXT("Fabrication Bay")) }
		&& AvailableOption->MissingFacilityIds == TArray<FName>{ FName(TEXT("facility.fabrication")) }
		&& AvailableOption->MissingFacilityNames == TArray<FString>{ FString(TEXT("Fabrication Bay")) }
		&& AvailableOption->UnavailableReasonCode == FName(TEXT("research_facility_missing"))
		&& AvailableOption->Detail.Contains(TEXT("lab Fabrication Bay"))
		&& AvailableOption->UnavailableReason.Contains(TEXT("Fabrication Bay")));
	FCampaignState ResearchFacilityCampaign = Campaign;
	FBaseFacilityState& ResearchFacility = ResearchFacilityCampaign.Bases[0].Facilities.AddDefaulted_GetRef();
	ResearchFacility.InstanceId = FGuid(109, 110, 111, 112);
	ResearchFacility.FacilityId = Fabrication.Identity.RuleId;
	ResearchFacility.GridX = 6;
	ResearchFacility.GridY = 0;
	const FStrategicDashboardSnapshot ResearchFacilitySnapshot = FStrategicPresentationService::BuildDashboard(
		ResearchFacilityCampaign, Rules, Config);
	const FStrategicActionOptionView* ReadyResearchOption = ResearchFacilitySnapshot.ActionOptions.FindByPredicate(
		[&AvailableResearch](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Research
				&& Option.RuleId == AvailableResearch.Identity.RuleId;
		});
	TestTrue(TEXT("Operating the required specialist facility makes unlocked research command-ready"),
		ReadyResearchOption != nullptr && ReadyResearchOption->bAvailable);
	TestTrue(TEXT("Facility option exposes its footprint and all valid manual anchor cells"), FacilityOption != nullptr
		&& FacilityOption->bAvailable
		&& FacilityOption->Detail.Contains(TEXT("+4 scientist cap"))
		&& FacilityOption->FacilityGridWidth == Operations.GridWidth
		&& FacilityOption->FacilityGridHeight == Operations.GridHeight
		&& !FacilityOption->ValidFacilityPlacements.IsEmpty()
		&& FacilityOption->ValidFacilityPlacements[0] == FIntPoint(
			FacilityOption->SuggestedGridX, FacilityOption->SuggestedGridY));
	TestTrue(TEXT("Defense construction option exposes accuracy, damage, and rounded expected output"),
		DefenseOption != nullptr
		&& DefenseOption->Detail.Contains(TEXT("battery 75% / 90 dmg / ~68 expected")));
	TestTrue(TEXT("Manufacturing explains the missing fabrication facility"), ManufacturingOption != nullptr
		&& !ManufacturingOption->bAvailable
		&& ManufacturingOption->UnavailableReasonCode == FName(TEXT("manufacturing_facility_missing"))
		&& ManufacturingOption->StorageDeltaPerUnit == 1
		&& ManufacturingOption->MaterialRequirements.Num() == 1
		&& ManufacturingOption->MaterialRequirements[0].AvailableQuantity == 3);
	TestTrue(TEXT("Craft procurement exposes locale-neutral hull and crew values"), CraftOption != nullptr
		&& CraftOption->CraftMaxHull == CraftRule.MaxHull
		&& CraftOption->CraftAgentCapacity == CraftRule.AgentCapacity);

	FCampaignState MaterialBlocked = Campaign;
	FBaseFacilityState& FabricationState = MaterialBlocked.Bases[0].Facilities.AddDefaulted_GetRef();
	FabricationState.InstanceId = FGuid(101, 102, 103, 104);
	FabricationState.FacilityId = Fabrication.Identity.RuleId;
	FabricationState.GridX = 6;
	FabricationState.GridY = 0;
	FInventoryStack* MaterialStock = MaterialBlocked.Bases[0].Inventory.FindByPredicate(
		[&RecoveredMaterial](const FInventoryStack& Stack) { return Stack.ItemId == RecoveredMaterial.Identity.RuleId; });
	if (MaterialStock != nullptr)
	{
		MaterialStock->Quantity = 1;
	}
	const FStrategicDashboardSnapshot BlockedSnapshot = FStrategicPresentationService::BuildDashboard(
		MaterialBlocked, Rules, Config);
	const FStrategicActionOptionView* BlockedManufacturing = BlockedSnapshot.ActionOptions.FindByPredicate(
		[&Manufactured](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Manufacturing
				&& Option.RuleId == Manufactured.Identity.RuleId;
		});
	TestTrue(TEXT("Manufacturing read model explains exact material shortages"), BlockedManufacturing != nullptr
		&& !BlockedManufacturing->bAvailable
		&& BlockedManufacturing->UnavailableReasonCode == FName(TEXT("manufacturing_materials_missing"))
		&& BlockedManufacturing->UnavailableReason.Contains(TEXT("2 Recovered Alloy (1 stock)")));

	FResolvedRuleSet StorageConstrainedRules = Rules;
	StorageConstrainedRules.Facilities.FindChecked(Operations.Identity.RuleId).StorageCapacity = 20;
	FCampaignState StorageBlocked = Campaign;
	FBaseFacilityState& StorageBlockedFabrication = StorageBlocked.Bases[0].Facilities.AddDefaulted_GetRef();
	StorageBlockedFabrication.InstanceId = FGuid(105, 106, 107, 108);
	StorageBlockedFabrication.FacilityId = Fabrication.Identity.RuleId;
	StorageBlockedFabrication.GridX = 6;
	StorageBlockedFabrication.GridY = 0;
	const FStrategicDashboardSnapshot StorageBlockedSnapshot = FStrategicPresentationService::BuildDashboard(
		StorageBlocked, StorageConstrainedRules, Config);
	const FStrategicActionOptionView* StorageBlockedManufacturing =
		StorageBlockedSnapshot.ActionOptions.FindByPredicate(
			[&Manufactured](const FStrategicActionOptionView& Option)
			{
				return Option.Type == EStrategicActionOptionType::Manufacturing
					&& Option.RuleId == Manufactured.Identity.RuleId;
			});
	TestTrue(TEXT("Over-capacity presentation remains lossless and blocks only added overflow"),
		StorageBlockedSnapshot.Bases[0].StorageCommitted == 46
		&& StorageBlockedSnapshot.Bases[0].StorageOverflow == 26
		&& StorageBlockedManufacturing != nullptr
		&& !StorageBlockedManufacturing->bAvailable
		&& StorageBlockedManufacturing->UnavailableReasonCode == FName(TEXT("storage_capacity_exceeded"))
		&& StorageBlockedSnapshot.Projects[1].bCanRemoveManufacturingUnit
		&& StorageBlockedSnapshot.Projects[1].bCanCancel);

	FCampaignState OutageCampaign = Campaign;
	OutageCampaign.Funds = 20000;
	OutageCampaign.Bases[0].Facilities[1].Damage = FlightDeck.MaxIntegrity;
	const FStrategicDashboardSnapshot OutageSnapshot = FStrategicPresentationService::BuildDashboard(
		OutageCampaign, Rules, Config);
	const FStrategicFacilityView* OfflineDeck = OutageSnapshot.Bases[0].FacilityLayout.FindByPredicate(
		[&FlightDeck](const FStrategicFacilityView& Facility)
		{
			return Facility.FacilityId == FlightDeck.Identity.RuleId && !Facility.bConstructing;
		});
	TestTrue(TEXT("A fully disabled flight deck stops supplying craft capacity"),
		OutageSnapshot.Bases[0].CraftCapacity == 0
		&& OfflineDeck != nullptr
		&& !OfflineDeck->bOperational
		&& OfflineDeck->CurrentIntegrity == 0
		&& OfflineDeck->bCanRepair
		&& OfflineDeck->RepairCost == 10000
		&& OfflineDeck->RepairDurationSeconds == 400 * 3600);
	TestTrue(TEXT("Base facility summary marks disabled infrastructure offline"),
		OutageSnapshot.Bases[0].Facilities.ContainsByPredicate(
			[](const FString& Name)
			{
				return Name.Contains(TEXT("Flight Deck")) && Name.Contains(TEXT("OFFLINE 0/200"));
			}));

	FCampaignState DisabledFabricationCampaign = MaterialBlocked;
	DisabledFabricationCampaign.Funds = 20000;
	DisabledFabricationCampaign.Bases[0].Facilities.Last().Damage = Fabrication.MaxIntegrity;
	const FStrategicDashboardSnapshot DisabledFabricationSnapshot =
		FStrategicPresentationService::BuildDashboard(DisabledFabricationCampaign, Rules, Config);
	const FStrategicActionOptionView* DisabledFabricationOption =
		DisabledFabricationSnapshot.ActionOptions.FindByPredicate(
			[&Manufactured](const FStrategicActionOptionView& Option)
			{
				return Option.Type == EStrategicActionOptionType::Manufacturing
					&& Option.RuleId == Manufactured.Identity.RuleId;
			});
	TestTrue(TEXT("A physically present but disabled fabrication bay cannot start production"),
		DisabledFabricationOption != nullptr
		&& !DisabledFabricationOption->bAvailable
		&& DisabledFabricationOption->UnavailableReasonCode == FName(TEXT("manufacturing_facility_missing")));

	FCampaignState AssaultCampaign = Campaign;
	AssaultCampaign.Funds = 200000;
	AssaultCampaign.StrategicContacts.Reset();
	AssaultCampaign.AdversaryMissions.Reset();
	AssaultCampaign.BaseAssaults.Reset();
	AssaultCampaign.AdversaryMissionsLaunched = 1;
	FBaseFacilityState& BatteryState = AssaultCampaign.Bases[0].Facilities.AddDefaulted_GetRef();
	BatteryState.InstanceId = FGuid(111, 112, 113, 114);
	BatteryState.FacilityId = DefenseBattery.Identity.RuleId;
	BatteryState.GridX = 6;
	BatteryState.GridY = 2;
	AssaultCampaign.Bases[0].Inventory.Add({ DefenseSupply.Identity.RuleId, 2 });
	const FGuid AssaultContactId(121, 122, 123, 124);
	const FGuid AssaultMissionId(131, 132, 133, 134);
	const FGuid AssaultId(141, 142, 143, 144);
	FStrategicContactState& AssaultContact = AssaultCampaign.StrategicContacts.AddDefaulted_GetRef();
	AssaultContact.ContactId = AssaultContactId;
	AssaultContact.ContactRuleId = ContactRule.Identity.RuleId;
	AssaultContact.Status = EStrategicContactStatus::Detected;
	AssaultContact.OriginLongitudeMilliDegrees = -20000;
	AssaultContact.OriginLatitudeMilliDegrees = 10000;
	AssaultContact.LongitudeMilliDegrees = Base.LongitudeMilliDegrees;
	AssaultContact.LatitudeMilliDegrees = Base.LatitudeMilliDegrees;
	AssaultContact.DestinationLongitudeMilliDegrees = Base.LongitudeMilliDegrees;
	AssaultContact.DestinationLatitudeMilliDegrees = Base.LatitudeMilliDegrees;
	AssaultContact.TotalRouteSeconds = 100;
	AssaultContact.ElapsedRouteSeconds = 100;
	AssaultContact.CurrentHull = 70;
	FAdversaryMissionState& AssaultMission = AssaultCampaign.AdversaryMissions.AddDefaulted_GetRef();
	AssaultMission.MissionId = AssaultMissionId;
	AssaultMission.ContactId = AssaultContactId;
	AssaultMission.MissionRuleId = BaseRaid.Identity.RuleId;
	AssaultMission.TargetBaseId = BaseId;
	AssaultMission.StartedUtc = AssaultCampaign.StrategicTime.Utc;
	FBaseAssaultState& Assault = AssaultCampaign.BaseAssaults.AddDefaulted_GetRef();
	Assault.AssaultId = AssaultId;
	Assault.MissionId = AssaultMissionId;
	Assault.ContactId = AssaultContactId;
	Assault.BaseId = BaseId;
	Assault.ArrivedUtc = AssaultCampaign.StrategicTime.Utc;
	const FGuid DefenderId(151, 152, 153, 154);
	FPersonnelState& Defender = AssaultCampaign.Personnel.AddDefaulted_GetRef();
	Defender.PersonnelId = DefenderId;
	Defender.BaseId = BaseId;
	Defender.DisplayName = TEXT("Ari Venn");
	Defender.RoleId = FieldAgent.Identity.RuleId;
	Defender.Status = EPersonnelStatus::Available;
	Defender.MaxHealth = 40;
	Defender.CurrentHealth = 40;
	const FStrategicDashboardSnapshot AssaultSnapshot = FStrategicPresentationService::BuildDashboard(
		AssaultCampaign, Rules, Config);
	TestTrue(TEXT("Pending perimeter raid becomes a blocking strategic decision"),
		AssaultSnapshot.bSucceeded && AssaultSnapshot.bDecisionRequired && !AssaultSnapshot.bCanAdvanceTime
		&& AssaultSnapshot.BaseAssaults.Num() == 1);
	TestTrue(TEXT("Base defense alert exposes exact combat readiness and breach risk"),
		AssaultSnapshot.BaseAssaults[0].AssaultId == AssaultId
		&& AssaultSnapshot.BaseAssaults[0].MissionRuleId == BaseRaid.Identity.RuleId
		&& AssaultSnapshot.BaseAssaults[0].ContactRuleId == ContactRule.Identity.RuleId
		&& AssaultSnapshot.BaseAssaults[0].BaseName == Base.Name
		&& AssaultSnapshot.BaseAssaults[0].MissionName == BaseRaid.DisplayName
		&& AssaultSnapshot.BaseAssaults[0].ContactHull == 70
		&& AssaultSnapshot.BaseAssaults[0].DefenseBatteryCount == 1
		&& AssaultSnapshot.BaseAssaults[0].ReadyDefenseBatteryCount == 1
		&& AssaultSnapshot.BaseAssaults[0].MaximumDefenseDamage == 90
		&& AssaultSnapshot.BaseAssaults[0].ExpectedDefenseDamage == 68
		&& AssaultSnapshot.BaseAssaults[0].DefenseSupplies.Num() == 1
		&& AssaultSnapshot.BaseAssaults[0].DefenseSupplies[0].ItemId == DefenseSupply.Identity.RuleId
		&& AssaultSnapshot.BaseAssaults[0].DefenseSupplies[0].DisplayName == DefenseSupply.DisplayName
		&& AssaultSnapshot.BaseAssaults[0].DefenseSupplies[0].RequiredQuantity == 2
		&& AssaultSnapshot.BaseAssaults[0].DefenseSupplies[0].AvailableQuantity == 2
		&& AssaultSnapshot.BaseAssaults[0].DefenseSupplies[0].AllocatedQuantity == 2
		&& AssaultSnapshot.BaseAssaults[0].BreachDamagePerFacility == 35
		&& AssaultSnapshot.BaseAssaults[0].MaximumFacilitiesHit == 2
		&& AssaultSnapshot.BaseAssaults[0].bCanResolve);
	const TArray<FStrategicBaseDefenseDoctrineView>& DoctrineOptions =
		AssaultSnapshot.BaseAssaults[0].FireDoctrines;
	TestTrue(TEXT("Base defense alert publishes four fixed-order command-ready doctrine previews"),
		DoctrineOptions.Num() == 4
		&& DoctrineOptions[0].Doctrine == EBaseDefenseFireDoctrine::CoordinatedLine
		&& DoctrineOptions[0].PolicyId == FName(TEXT("base-defense.coordinated-line"))
		&& DoctrineOptions[0].DisplayName == TEXT("Coordinated Line")
		&& DoctrineOptions[0].bCanResolve
		&& DoctrineOptions[0].ReadyDefenseBatteryCount == 1
		&& DoctrineOptions[0].MaximumDefenseDamage == 90
		&& DoctrineOptions[0].ExpectedDefenseDamage == 68
		&& DoctrineOptions[0].DefenseSupplies.Num() == 1
		&& DoctrineOptions[0].DefenseSupplies[0].AllocatedQuantity == 2
		&& DoctrineOptions[1].Doctrine == EBaseDefenseFireDoctrine::PrecisionScreen
		&& DoctrineOptions[1].PolicyId == FName(TEXT("base-defense.precision-screen"))
		&& DoctrineOptions[1].DisplayName == TEXT("Precision Screen")
		&& DoctrineOptions[1].bCanResolve
		&& DoctrineOptions[2].Doctrine == EBaseDefenseFireDoctrine::BreachBreaker
		&& DoctrineOptions[2].PolicyId == FName(TEXT("base-defense.breach-breaker"))
		&& DoctrineOptions[2].DisplayName == TEXT("Breach Breaker")
		&& DoctrineOptions[2].bCanResolve
		&& DoctrineOptions[3].Doctrine == EBaseDefenseFireDoctrine::GridOvercharge
		&& DoctrineOptions[3].PolicyId == FName(TEXT("base-defense.grid-overcharge"))
		&& DoctrineOptions[3].DisplayName == TEXT("Grid Overcharge")
		&& DoctrineOptions[3].bCanResolve
		&& DoctrineOptions[3].bAffordable
		&& DoctrineOptions[3].FundingCost == 75000
		&& DoctrineOptions[3].AccuracyBonus == 15
		&& DoctrineOptions[3].DamagePercent == 125
		&& DoctrineOptions[3].ReadyDefenseBatteryCount == 1
		&& DoctrineOptions[3].MaximumDefenseDamage == 113
		&& DoctrineOptions[3].ExpectedDefenseDamage == 102);
	FCampaignState UnaffordableGridCampaign = AssaultCampaign;
	UnaffordableGridCampaign.Funds = 74999;
	const FStrategicDashboardSnapshot UnaffordableGridSnapshot =
		FStrategicPresentationService::BuildDashboard(UnaffordableGridCampaign, Rules, Config);
	const TArray<FStrategicBaseDefenseDoctrineView>& UnaffordableGridOptions =
		UnaffordableGridSnapshot.BaseAssaults[0].FireDoctrines;
	TestTrue(TEXT("Only the threat-priced Grid Overcharge option locks when campaign funds are short"),
		UnaffordableGridSnapshot.bSucceeded && UnaffordableGridOptions.Num() == 4
		&& UnaffordableGridOptions[0].bCanResolve
		&& UnaffordableGridOptions[1].bCanResolve
		&& UnaffordableGridOptions[2].bCanResolve
		&& !UnaffordableGridOptions[3].bCanResolve
		&& !UnaffordableGridOptions[3].bAffordable
		&& UnaffordableGridOptions[3].FundingCost == 75000
		&& UnaffordableGridOptions[3].UnavailableReasonCode
			== FName(TEXT("insufficient_base_defense_overcharge_funds")));
	TestTrue(TEXT("Base defense alert exposes a playable local ground-team option"),
		AssaultSnapshot.BaseAssaults[0].bCanDeployTacticalDefense
		&& !AssaultSnapshot.BaseAssaults[0].bTacticalDefensePrepared
		&& AssaultSnapshot.BaseAssaults[0].DefenderCount == 1
		&& AssaultSnapshot.BaseAssaults[0].TacticalMissionRuleId == BaseDefenseMission.Identity.RuleId
		&& !AssaultSnapshot.BaseAssaults[0].TacticalOperationId.IsValid());
	TestTrue(TEXT("Targeted contact and base summary expose the same pending defense state"),
		AssaultSnapshot.Contacts.Num() == 1
		&& AssaultSnapshot.Contacts[0].bTargetsBase
		&& AssaultSnapshot.Contacts[0].TargetBaseId == BaseId
		&& AssaultSnapshot.Contacts[0].TargetBaseName == Base.Name
		&& AssaultSnapshot.Contacts[0].bAssaultPending
		&& AssaultSnapshot.Bases[0].DefenseBatteryCount == 1
		&& AssaultSnapshot.Bases[0].MaximumDefenseDamage == 90
		&& AssaultSnapshot.Bases[0].ExpectedDefenseDamage == 68);
	const FStrategicFacilityView* PresentedBattery =
		AssaultSnapshot.Bases[0].FacilityLayout.FindByPredicate(
			[&DefenseBattery](const FStrategicFacilityView& Facility)
			{
				return Facility.FacilityId == DefenseBattery.Identity.RuleId;
			});
	TestTrue(TEXT("Facility inspection retains the localized defense supply contract"),
		PresentedBattery != nullptr
		&& PresentedBattery->BaseDefenseSupplyItemId == DefenseSupply.Identity.RuleId
		&& PresentedBattery->BaseDefenseSupplyDisplayName == DefenseSupply.DisplayName
		&& PresentedBattery->BaseDefenseSupplyPerShot == 2);

	FCampaignState UnsuppliedCampaign = AssaultCampaign;
	UnsuppliedCampaign.Bases[0].Inventory.RemoveAll(
		[&DefenseSupply](const FInventoryStack& Stack)
		{
			return Stack.ItemId == DefenseSupply.Identity.RuleId;
		});
	const FStrategicDashboardSnapshot UnsuppliedSnapshot =
		FStrategicPresentationService::BuildDashboard(UnsuppliedCampaign, Rules, Config);
	TestTrue(TEXT("An empty capacitor stock remains resolvable but exposes an unready zero-damage volley"),
		UnsuppliedSnapshot.bSucceeded && UnsuppliedSnapshot.BaseAssaults.Num() == 1
		&& UnsuppliedSnapshot.BaseAssaults[0].bCanResolve
		&& UnsuppliedSnapshot.BaseAssaults[0].DefenseBatteryCount == 1
		&& UnsuppliedSnapshot.BaseAssaults[0].ReadyDefenseBatteryCount == 0
		&& UnsuppliedSnapshot.BaseAssaults[0].MaximumDefenseDamage == 0
		&& UnsuppliedSnapshot.BaseAssaults[0].ExpectedDefenseDamage == 0
		&& UnsuppliedSnapshot.BaseAssaults[0].DefenseSupplies.Num() == 1
		&& UnsuppliedSnapshot.BaseAssaults[0].DefenseSupplies[0].RequiredQuantity == 2
		&& UnsuppliedSnapshot.BaseAssaults[0].DefenseSupplies[0].AvailableQuantity == 0
		&& UnsuppliedSnapshot.BaseAssaults[0].DefenseSupplies[0].AllocatedQuantity == 0);

	FCampaignState PreparedCampaign = AssaultCampaign;
	PreparedCampaign.Personnel.Last().Status = EPersonnelStatus::Deployed;
	const FGuid TacticalOperationId(161, 162, 163, 164);
	FTacticalOperationState& TacticalOperation = PreparedCampaign.TacticalOperations.AddDefaulted_GetRef();
	TacticalOperation.OperationId = TacticalOperationId;
	TacticalOperation.Type = ETacticalOperationType::BaseDefense;
	TacticalOperation.BaseId = BaseId;
	TacticalOperation.AssaultId = AssaultId;
	TacticalOperation.AgentIds.Add(DefenderId);
	const FStrategicDashboardSnapshot PreparedSnapshot = FStrategicPresentationService::BuildDashboard(
		PreparedCampaign, Rules, Config);
	TestTrue(TEXT("Prepared ground defense is resumable and locks the battery alternative"),
		PreparedSnapshot.bSucceeded && PreparedSnapshot.BaseAssaults.Num() == 1
		&& PreparedSnapshot.BaseAssaults[0].bTacticalDefensePrepared
		&& !PreparedSnapshot.BaseAssaults[0].bCanDeployTacticalDefense
		&& !PreparedSnapshot.BaseAssaults[0].bCanResolve
		&& PreparedSnapshot.BaseAssaults[0].TacticalOperationId == TacticalOperationId
		&& PreparedSnapshot.BaseAssaults[0].DefenderCount == 1
		&& PreparedSnapshot.BaseAssaults[0].DefenseBatteryCount == 1
		&& PreparedSnapshot.BaseAssaults[0].MaximumDefenseDamage == 90
		&& PreparedSnapshot.BaseAssaults[0].ExpectedDefenseDamage == 68
		&& PreparedSnapshot.BaseAssaults[0].UnavailableReasonCode
			== FName(TEXT("base_assault_in_tactical_operation"))
		&& PreparedSnapshot.BaseAssaults[0].FireDoctrines.Num() == 4
		&& !PreparedSnapshot.BaseAssaults[0].FireDoctrines.ContainsByPredicate(
			[](const FStrategicBaseDefenseDoctrineView& Option)
			{
				return Option.bCanResolve
					|| Option.UnavailableReasonCode
						!= FName(TEXT("base_assault_in_tactical_operation"));
			})
		&& PreparedSnapshot.BaseAssaults[0].TacticalUnavailableReasonCode
			== FName(TEXT("base_defenders_committed"))
		&& PreparedSnapshot.PendingOperationIds.Contains(TacticalOperationId));

	FCampaignState NoGroundDefendersCampaign = AssaultCampaign;
	for (FPersonnelState& PersonnelEntry : NoGroundDefendersCampaign.Personnel)
	{
		if (PersonnelEntry.RoleId == FieldAgent.Identity.RuleId)
		{
			PersonnelEntry.Status = EPersonnelStatus::Deployed;
		}
	}
	const FStrategicDashboardSnapshot NoGroundDefendersSnapshot =
		FStrategicPresentationService::BuildDashboard(NoGroundDefendersCampaign, Rules, Config);
	TestTrue(TEXT("Base-defense presentation retains the exact ground-team rejection code"),
		NoGroundDefendersSnapshot.bSucceeded && NoGroundDefendersSnapshot.BaseAssaults.Num() == 1
		&& !NoGroundDefendersSnapshot.BaseAssaults[0].bCanDeployTacticalDefense
		&& NoGroundDefendersSnapshot.BaseAssaults[0].TacticalUnavailableReasonCode
			== FName(TEXT("no_base_defenders"))
		&& NoGroundDefendersSnapshot.BaseAssaults[0].TacticalUnavailableReason.Contains(
			TEXT("No available unassigned field agents")));
	return true;
}

#endif
