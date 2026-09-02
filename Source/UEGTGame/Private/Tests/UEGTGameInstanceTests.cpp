// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "UEGTGameInstance.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTGameInstanceModCatalogTest,
	"UEGT.Core.GameInstance.ModCatalogLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTGameInstanceModCatalogTest::RunTest(const FString& Parameters)
{
	UUEGTGameInstance* Instance = NewObject<UUEGTGameInstance>();
	TestNotNull(TEXT("Mod-aware game instance can be constructed"), Instance);
	if (Instance == nullptr)
	{
		return false;
	}

	const FString RulesDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
	const FString SampleModsDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples"), TEXT("Mods"));
	const TArray<FString> ContentDirectories = { RulesDirectory, SampleModsDirectory };
	TestTrue(TEXT("Game lifecycle loads base content and user mods transactionally"),
		Instance->ReloadContentFromDirectories(ContentDirectories));
	TestTrue(TEXT("Successful mod load exposes a ready catalog and both source roots"),
		Instance->IsContentReady()
		&& Instance->GetLoadedContentDirectories().Num() == 2
		&& Instance->GetLoadedContentFiles().Num() == 2
		&& Instance->GetLoadedContentVersions().Num() == 2);
	const FItemRule* AuroraRelay = Instance->GetLoadedRules().Items.Find(TEXT("item.aurora-relay"));
	TestTrue(TEXT("Game lifecycle exposes the sample mod rule and provenance"), AuroraRelay != nullptr
		&& Instance->GetLoadedRules().ItemOrigins.FindRef(TEXT("item.aurora-relay"))
			== FName(TEXT("sample.aurora-relay")));

	const FString MissingDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("missing-live-mod-root"));
	TestFalse(TEXT("A failed pre-campaign reload is rejected"),
		Instance->ReloadContentFromDirectories({ RulesDirectory, MissingDirectory }));
	TestTrue(TEXT("Failed reload disables campaign start but retains the last verified catalog atomically"),
		!Instance->IsContentReady()
		&& Instance->GetContentDiagnostics().ContainsByPredicate(
			[](const FContentDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == FName(TEXT("content_directory_missing"));
			})
		&& Instance->GetLoadedRules().Items.Contains(TEXT("item.aurora-relay"))
		&& Instance->GetLoadedContentVersions().Num() == 2);
	TestTrue(TEXT("Corrected roots restore the verified mod catalog"),
		Instance->ReloadContentFromDirectories(ContentDirectories));

	const TArray<FCampaignContentVersion> ModVersions = Instance->GetLoadedContentVersions();
	TestTrue(TEXT("A campaign can begin against the verified mod set"), Instance->StartNewCampaign(
		ECampaignDifficulty::Cadet, 424242, EUEGTFundingModel::BalancedMandate));
	TestFalse(TEXT("Content and mods cannot reload beneath an active campaign"),
		Instance->ReloadContentFromDirectories(ContentDirectories));
	const FCampaignSaveEnvelope ModEnvelope = FCampaignSaveCodec::CreateNew(
		Instance->GetCampaignState(), ModVersions, TEXT("mod-lifecycle"),
		FDateTime(2042, 3, 4, 5, 6, 7), FGuid(401, 402, 403, 404));
	const FCampaignSaveWriteResult Written = FCampaignSaveCodec::Serialize(ModEnvelope);
	TestTrue(TEXT("Mod package identities and versions participate in verified saves"), Written.bSucceeded);
	UUEGTGameInstance* BaseOnly = NewObject<UUEGTGameInstance>();
	TestTrue(TEXT("A base-only comparison catalog loads"), BaseOnly != nullptr
		&& BaseOnly->ReloadContentFromDirectories({ RulesDirectory }));
	const FCampaignSaveReadResult Incompatible = BaseOnly != nullptr
		? FCampaignSaveCodec::Deserialize(Written.Json, BaseOnly->GetLoadedContentVersions())
		: FCampaignSaveReadResult();
	TestTrue(TEXT("A modded campaign cannot be loaded under a different active package set"),
		!Incompatible.bSucceeded && Incompatible.HasDiagnostic(TEXT("incompatible_content")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTFundingModelPolicyTest,
	"UEGT.Core.GameInstance.FundingModelPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTFundingModelPolicyTest::RunTest(const FString& Parameters)
{
	struct FBaseline
	{
		ECampaignDifficulty Difficulty;
		int64 StartingFunds;
		int64 MonthlyFunding;
	};
	const FBaseline Baselines[] = {
		{ ECampaignDifficulty::Cadet, 2000000, 650000 },
		{ ECampaignDifficulty::Standard, 1500000, 500000 },
		{ ECampaignDifficulty::Veteran, 1250000, 425000 },
		{ ECampaignDifficulty::Apex, 1000000, 350000 }
	};
	for (const FBaseline& Baseline : Baselines)
	{
		FUEGTFundingProjection Balanced;
		FUEGTFundingProjection Mobilization;
		FUEGTFundingProjection Sustained;
		TestTrue(TEXT("Balanced projection accepts every authored difficulty"),
			UUEGTGameInstance::CalculateFundingProjection(
				Baseline.Difficulty, EUEGTFundingModel::BalancedMandate, Balanced));
		TestTrue(TEXT("Mobilization projection accepts every authored difficulty"),
			UUEGTGameInstance::CalculateFundingProjection(
				Baseline.Difficulty, EUEGTFundingModel::RapidMobilization, Mobilization));
		TestTrue(TEXT("Sustained projection accepts every authored difficulty"),
			UUEGTGameInstance::CalculateFundingProjection(
				Baseline.Difficulty, EUEGTFundingModel::SustainedCharter, Sustained));
		TestEqual(TEXT("Balanced preserves difficulty starting reserves"),
			Balanced.StartingFunds, Baseline.StartingFunds);
		TestEqual(TEXT("Balanced preserves difficulty monthly support"),
			Balanced.MonthlyFunding, Baseline.MonthlyFunding);
		TestEqual(TEXT("Mobilization raises starting reserves by exactly twenty percent"),
			Mobilization.StartingFunds, Baseline.StartingFunds * 120 / 100);
		TestEqual(TEXT("Mobilization lowers monthly support by exactly twenty percent"),
			Mobilization.MonthlyFunding, Baseline.MonthlyFunding * 80 / 100);
		TestEqual(TEXT("Sustained lowers starting reserves by exactly twenty percent"),
			Sustained.StartingFunds, Baseline.StartingFunds * 80 / 100);
		TestEqual(TEXT("Sustained raises monthly support by exactly twenty percent"),
			Sustained.MonthlyFunding, Baseline.MonthlyFunding * 120 / 100);
	}
	FUEGTFundingProjection InvalidProjection;
	InvalidProjection.StartingFunds = 1;
	InvalidProjection.MonthlyFunding = 1;
	TestFalse(TEXT("Unknown funding models are rejected"),
		UUEGTGameInstance::CalculateFundingProjection(
			ECampaignDifficulty::Standard,
			static_cast<EUEGTFundingModel>(255),
			InvalidProjection));
	TestTrue(TEXT("Rejected funding projections reset every output"),
		InvalidProjection.StartingFunds == 0 && InvalidProjection.MonthlyFunding == 0);

	const auto StartFixture = [this](const EUEGTFundingModel FundingModel)
	{
		UUEGTGameInstance* Instance = NewObject<UUEGTGameInstance>();
		const bool bStarted = Instance != nullptr
			&& Instance->ReloadContent()
			&& Instance->StartNewCampaign(ECampaignDifficulty::Standard, 707070, FundingModel);
		TestTrue(TEXT("Funding fixture starts through the packaged-content lifecycle"), bStarted);
		return bStarted ? Instance : nullptr;
	};
	UUEGTGameInstance* BalancedCampaign = StartFixture(EUEGTFundingModel::BalancedMandate);
	UUEGTGameInstance* MobilizationCampaign = StartFixture(EUEGTFundingModel::RapidMobilization);
	UUEGTGameInstance* SustainedCampaign = StartFixture(EUEGTFundingModel::SustainedCharter);
	UUEGTGameInstance* SustainedReplay = StartFixture(EUEGTFundingModel::SustainedCharter);
	const auto StartDifficultyFixture = [this](const ECampaignDifficulty Difficulty)
	{
		UUEGTGameInstance* Instance = NewObject<UUEGTGameInstance>();
		const bool bStarted = Instance != nullptr
			&& Instance->ReloadContent()
			&& Instance->StartNewCampaign(
				Difficulty, 707070, EUEGTFundingModel::BalancedMandate);
		TestTrue(TEXT("Difficulty fixture starts through the packaged-content lifecycle"), bStarted);
		return bStarted ? Instance : nullptr;
	};
	UUEGTGameInstance* CadetCampaign = StartDifficultyFixture(ECampaignDifficulty::Cadet);
	UUEGTGameInstance* VeteranCampaign = StartDifficultyFixture(ECampaignDifficulty::Veteran);
	UUEGTGameInstance* ApexCampaign = StartDifficultyFixture(ECampaignDifficulty::Apex);
	if (BalancedCampaign == nullptr || MobilizationCampaign == nullptr
		|| SustainedCampaign == nullptr || SustainedReplay == nullptr
		|| CadetCampaign == nullptr || VeteranCampaign == nullptr || ApexCampaign == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Balanced realizes its projected economy in campaign state"),
		BalancedCampaign->GetCampaignState().Funds == 1500000
		&& BalancedCampaign->GetCampaignState().MonthlyFunding == 500000);
	TestTrue(TEXT("Mobilization realizes its projected economy in campaign state"),
		MobilizationCampaign->GetCampaignState().Funds == 1800000
		&& MobilizationCampaign->GetCampaignState().MonthlyFunding == 400000);
	TestTrue(TEXT("Sustained realizes its projected economy in campaign state"),
		SustainedCampaign->GetCampaignState().Funds == 1200000
		&& SustainedCampaign->GetCampaignState().MonthlyFunding == 600000);
	TestTrue(TEXT("Balanced mandate allocates exact authored regional funding shares"),
		BalancedCampaign->GetCampaignState().RegionalMandates.Num() == 3
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].RegionId == FName(TEXT("region.cascadia"))
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].Support == 55
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].CurrentMonthlyFunding == 200000
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].RegionId == FName(TEXT("region.north-atlantic"))
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].Support == 60
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].CurrentMonthlyFunding == 175000
		&& BalancedCampaign->GetCampaignState().RegionalMandates[2].RegionId == FName(TEXT("region.western-pacific"))
		&& BalancedCampaign->GetCampaignState().RegionalMandates[2].Support == 50
		&& BalancedCampaign->GetCampaignState().RegionalMandates[2].CurrentMonthlyFunding == 125000);
	TestTrue(TEXT("Sustained charter preserves the same exact regional weights"),
		SustainedCampaign->GetCampaignState().RegionalMandates.Num() == 3
		&& SustainedCampaign->GetCampaignState().RegionalMandates[0].CurrentMonthlyFunding == 240000
		&& SustainedCampaign->GetCampaignState().RegionalMandates[1].CurrentMonthlyFunding == 210000
		&& SustainedCampaign->GetCampaignState().RegionalMandates[2].CurrentMonthlyFunding == 150000);
	TestTrue(TEXT("Funding selection consumes no campaign random draw"),
		BalancedCampaign->GetCampaignState().SimulationRandom.DrawCount
			== MobilizationCampaign->GetCampaignState().SimulationRandom.DrawCount
		&& BalancedCampaign->GetCampaignState().SimulationRandom.GetStateForSave()
			== MobilizationCampaign->GetCampaignState().SimulationRandom.GetStateForSave()
		&& BalancedCampaign->GetCampaignState().SimulationRandom.GetStateForSave()
			== SustainedCampaign->GetCampaignState().SimulationRandom.GetStateForSave());
	TestTrue(TEXT("Difficulty realizes transparent opening mission gaps"),
		CadetCampaign->GetCampaignState().NextAdversaryMissionSeconds == 108000
		&& BalancedCampaign->GetCampaignState().NextAdversaryMissionSeconds == 86400
		&& VeteranCampaign->GetCampaignState().NextAdversaryMissionSeconds == 73440
		&& ApexCampaign->GetCampaignState().NextAdversaryMissionSeconds == 60480);
	TestTrue(TEXT("Difficulty setup consumes no campaign random draw"),
		CadetCampaign->GetCampaignState().SimulationRandom.DrawCount == 0
		&& VeteranCampaign->GetCampaignState().SimulationRandom.DrawCount == 0
		&& ApexCampaign->GetCampaignState().SimulationRandom.DrawCount == 0
		&& CadetCampaign->GetCampaignState().SimulationRandom.GetStateForSave()
			== ApexCampaign->GetCampaignState().SimulationRandom.GetStateForSave());
	FAdversaryDifficultyTuning ApexTuning;
	TestTrue(TEXT("Game-instance UI projection shares the active simulation configuration"),
		ApexCampaign->GetAdversaryDifficultyTuning(ECampaignDifficulty::Apex, ApexTuning));
	TestTrue(TEXT("Game-instance UI projection exposes the exact Apex profile"),
		ApexTuning.MissionIntervalPercent == 70
		&& ApexTuning.EscapeConsequencePercent == 150);

	const FDateTime FixedWallClock(2041, 2, 3, 4, 5, 6);
	const FGuid FixedCampaignId(81, 82, 83, 84);
	const auto MakeEnvelope = [&](const UUEGTGameInstance* Instance)
	{
		return FCampaignSaveCodec::CreateNew(
			Instance->GetCampaignState(),
			Instance->GetLoadedContentVersions(),
			TEXT("funding-policy"),
			FixedWallClock,
			FixedCampaignId);
	};
	const FCampaignSaveEnvelope SustainedEnvelope = MakeEnvelope(SustainedCampaign);
	const FCampaignSaveWriteResult Written = FCampaignSaveCodec::Serialize(SustainedEnvelope);
	TestTrue(TEXT("Realized funding state serializes through the existing save format"), Written.bSucceeded);
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Written.Json, SustainedCampaign->GetLoadedContentVersions());
	TestTrue(TEXT("Realized funding state reloads against the current content set"), Read.bSucceeded);
	TestTrue(TEXT("Reloaded funding values retain the selected long-term tradeoff"),
		Read.Envelope.State.Funds == 1200000 && Read.Envelope.State.MonthlyFunding == 600000);
	TestEqual(TEXT("Same seed and funding mandate reproduce the exact canonical campaign"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(SustainedEnvelope),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(MakeEnvelope(SustainedReplay)));
	TestNotEqual(TEXT("Different funding mandates produce distinct persisted economy state"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(MakeEnvelope(BalancedCampaign)),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(SustainedEnvelope));
	const int64 DrawsBeforeOutreach = BalancedCampaign->GetCampaignState().SimulationRandom.DrawCount;
	FRegionalDiplomacyCommand Outreach;
	Outreach.ExpectedSequence = BalancedCampaign->GetCampaignState().CommandSequence;
	Outreach.RegionId = TEXT("region.cascadia");
	Outreach.ActionType = ERegionalDiplomacyActionType::CivicRelief;
	const FStrategicCommandResult OutreachResult = BalancedCampaign->ExecuteRegionalDiplomacy(Outreach);
	TestTrue(TEXT("Game-instance adapter routes regional outreach transactionally"), OutreachResult.bAccepted
		&& OutreachResult.HasEvent(EStrategicEventType::RegionalDiplomacyActionCompleted)
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].Support == 67
		&& BalancedCampaign->GetCampaignState().Funds == 1380000
		&& BalancedCampaign->GetCampaignState().SimulationRandom.DrawCount == DrawsBeforeOutreach);
	FSignRegionalCharterCommand Charter;
	Charter.ExpectedSequence = BalancedCampaign->GetCampaignState().CommandSequence;
	Charter.RegionId = TEXT("region.north-atlantic");
	const FStrategicCommandResult CharterResult = BalancedCampaign->SignRegionalCharter(Charter);
	TestTrue(TEXT("Game-instance adapter routes the durable regional charter transactionally"),
		CharterResult.bAccepted
		&& CharterResult.HasEvent(EStrategicEventType::RegionalCharterSigned)
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].bResilienceCharterSigned
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].Support == 50
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].CurrentMonthlyFunding == 157500
		&& BalancedCampaign->GetCampaignState().MonthlyFunding == 482500
		&& BalancedCampaign->GetCampaignState().Funds == 1130000
		&& BalancedCampaign->GetCampaignState().SimulationRandom.DrawCount == DrawsBeforeOutreach);
	const FStrategicDashboardSnapshot CharteredDashboard = BalancedCampaign->BuildStrategicDashboard();
	TestTrue(TEXT("Signed charter reaches immutable dashboard projection with exact effects"),
		CharteredDashboard.bSucceeded && CharteredDashboard.Regions.Num() == 3
		&& CharteredDashboard.Regions[1].ResilienceCharter.bSigned
		&& !CharteredDashboard.Regions[1].ResilienceCharter.bEnabled
		&& CharteredDashboard.Regions[1].ResilienceCharter.FundingPercent == 90
		&& CharteredDashboard.Regions[1].ResilienceCharter.MissionWeightPercent == 50
		&& CharteredDashboard.Regions[1].ResilienceCharter.EscapePressurePercent == 75
		&& CharteredDashboard.Regions[1].ProjectedMonthlyFunding == 157500);
	FSignRegionalCharterCommand CascadiaCharter;
	CascadiaCharter.ExpectedSequence = BalancedCampaign->GetCampaignState().CommandSequence;
	CascadiaCharter.RegionId = TEXT("region.cascadia");
	const FStrategicCommandResult CascadiaCharterResult =
		BalancedCampaign->SignRegionalCharter(CascadiaCharter);
	TestTrue(TEXT("A second charter establishes the adapter's compact prerequisite"),
		CascadiaCharterResult.bAccepted
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].bResilienceCharterSigned
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].Support == 57
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].CurrentMonthlyFunding == 180000
		&& BalancedCampaign->GetCampaignState().MonthlyFunding == 462500
		&& BalancedCampaign->GetCampaignState().Funds == 880000);
	FRatifyHorizonCompactCommand Compact;
	Compact.ExpectedSequence = BalancedCampaign->GetCampaignState().CommandSequence;
	const FStrategicCommandResult CompactResult = BalancedCampaign->RatifyHorizonCompact(Compact);
	TestTrue(TEXT("Game-instance adapter ratifies the global compact transactionally"),
		CompactResult.bAccepted
		&& CompactResult.HasEvent(EStrategicEventType::HorizonCompactRatified)
		&& BalancedCampaign->GetCampaignState().bHorizonCompactRatified
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].Support == 52
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].Support == 45
		&& BalancedCampaign->GetCampaignState().RegionalMandates[0].CurrentMonthlyFunding == 190000
		&& BalancedCampaign->GetCampaignState().RegionalMandates[1].CurrentMonthlyFunding == 166250
		&& BalancedCampaign->GetCampaignState().MonthlyFunding == 481250
		&& BalancedCampaign->GetCampaignState().Funds == 480000
		&& BalancedCampaign->GetCampaignState().SimulationRandom.DrawCount == DrawsBeforeOutreach);
	const FStrategicDashboardSnapshot CompactDashboard = BalancedCampaign->BuildStrategicDashboard();
	TestTrue(TEXT("Ratified compact reaches the immutable dashboard projection"),
		CompactDashboard.bSucceeded
		&& CompactDashboard.HorizonCompact.bRatified
		&& !CompactDashboard.HorizonCompact.bEnabled
		&& CompactDashboard.HorizonCompact.SignedCharters == 2
		&& CompactDashboard.HorizonCompact.FundingPercent == 95
		&& CompactDashboard.HorizonCompact.SharedEscapePressurePercent == 33
		&& CompactDashboard.HorizonCompact.WithdrawalSupportThreshold == 25
		&& CompactDashboard.HorizonCompact.RestorationMinimumSupport == 40
		&& CompactDashboard.HorizonCompact.ActiveMemberRegionIds.Num() == 2
		&& CompactDashboard.HorizonCompact.WithdrawnMemberRegionIds.IsEmpty()
		&& CompactDashboard.HorizonCompact.CurrentMonthlyFunding == 481250
		&& CompactDashboard.HorizonCompact.AidOptions.Num() == 2
		&& !CompactDashboard.HorizonCompact.AidOptions[0].bEnabled
		&& CompactDashboard.HorizonCompact.AidOptions[0].UnavailableReasonCode
			== FName(TEXT("coalition_aid_crisis_required"))
		&& CompactDashboard.HorizonCompact.UnavailableReasonCode
			== FName(TEXT("coalition_compact_already_ratified")));
	FRestoreHorizonCompactMemberCommand RestoreActiveMember;
	RestoreActiveMember.ExpectedSequence = BalancedCampaign->GetCampaignState().CommandSequence;
	RestoreActiveMember.RegionId = TEXT("region.north-atlantic");
	const FStrategicCommandResult RestoreActiveResult =
		BalancedCampaign->RestoreHorizonCompactMember(RestoreActiveMember);
	TestTrue(TEXT("Game-instance adapter routes compact restoration to its authoritative membership gate"),
		!RestoreActiveResult.bAccepted
		&& RestoreActiveResult.HasDiagnostic(TEXT("coalition_restoration_target_not_withdrawn"))
		&& BalancedCampaign->GetCampaignState().CommandSequence
			== RestoreActiveMember.ExpectedSequence
		&& !BalancedCampaign->GetCampaignState().RegionalMandates[1]
			.bHorizonCompactMemberWithdrawn);
	FCallHorizonCompactEmergencyVoteCommand VoteForActiveMember;
	VoteForActiveMember.ExpectedSequence =
		BalancedCampaign->GetCampaignState().CommandSequence;
	VoteForActiveMember.TargetRegionId = TEXT("region.north-atlantic");
	const FStrategicCommandResult VoteForActiveResult =
		BalancedCampaign->CallHorizonCompactEmergencyVote(VoteForActiveMember);
	TestTrue(TEXT("Game-instance adapter routes emergency votes to the authoritative withdrawal gate"),
		!VoteForActiveResult.bAccepted
		&& VoteForActiveResult.HasDiagnostic(
			TEXT("coalition_emergency_vote_target_not_withdrawn"))
		&& BalancedCampaign->GetCampaignState().CommandSequence
			== VoteForActiveMember.ExpectedSequence
		&& BalancedCampaign->GetCampaignState().LastCoalitionEmergencyVoteMonth == 0);
	FDeployReciprocalAidCommand Aid;
	Aid.ExpectedSequence = BalancedCampaign->GetCampaignState().CommandSequence;
	Aid.TargetRegionId = TEXT("region.cascadia");
	const FStrategicCommandResult AidResult = BalancedCampaign->DeployReciprocalAid(Aid);
	TestTrue(TEXT("Game-instance adapter routes Reciprocal Aid to its authoritative crisis gate"),
		!AidResult.bAccepted
		&& AidResult.HasDiagnostic(TEXT("coalition_aid_crisis_required"))
		&& BalancedCampaign->GetCampaignState().CommandSequence == Aid.ExpectedSequence
		&& BalancedCampaign->GetCampaignState().LastCoalitionAidMonth == 0);
#if !UE_BUILD_SHIPPING
	const bool bVoteDemoReady = BalancedCampaign->PrepareHorizonCompactEmergencyVoteDemo(
		TEXT("region.north-atlantic"));
	const FStrategicDashboardSnapshot VoteDemoSnapshot =
		BalancedCampaign->BuildStrategicDashboard();
	const FStrategicRegionView* VoteDemoTarget = VoteDemoSnapshot.Regions.FindByPredicate(
		[](const FStrategicRegionView& Region)
		{
			return Region.RegionId == FName(TEXT("region.north-atlantic"));
		});
	TestTrue(TEXT("Development runtime fixture produces a coherent command-ready emergency ballot"),
		bVoteDemoReady && VoteDemoSnapshot.bSucceeded && VoteDemoTarget != nullptr
		&& VoteDemoSnapshot.HorizonCompact.ActiveMemberRegionIds.Num() == 1
		&& VoteDemoSnapshot.HorizonCompact.WithdrawnMemberRegionIds.Num() == 1
		&& VoteDemoTarget->HorizonCompactEmergencyVote.bTargetWithdrawn
		&& VoteDemoTarget->HorizonCompactEmergencyVote.bEnabled
		&& VoteDemoTarget->HorizonCompactEmergencyVote.SupportingMemberRegionIds.Num() == 1
		&& VoteDemoTarget->HorizonCompactEmergencyVote.RequiredVotes == 1
		&& VoteDemoTarget->HorizonCompactEmergencyVote.TargetCurrentSupport == 30
		&& VoteDemoTarget->HorizonCompactEmergencyVote.TargetProjectedSupport == 42
		&& VoteDemoTarget->HorizonCompactEmergencyVote.TargetCurrentPressure == 45
		&& VoteDemoTarget->HorizonCompactEmergencyVote.TargetProjectedPressure == 30);
	const bool bCounterplayDemoReady = BalancedCampaign->PrepareCoalitionCounterplayDemo(
		TEXT("mission.ashen-accord-severance"));
	const FStrategicDashboardSnapshot CounterplayDemoSnapshot =
		BalancedCampaign->BuildStrategicDashboard();
	const FStrategicContactView* CounterplayContact =
		CounterplayDemoSnapshot.Contacts.IsEmpty()
			? nullptr : &CounterplayDemoSnapshot.Contacts[0];
	TestTrue(TEXT("Development runtime fixture produces one exact detected coalition-counterplay card"),
		bCounterplayDemoReady && CounterplayDemoSnapshot.bSucceeded
		&& CounterplayContact != nullptr
		&& CounterplayContact->PlanId == FName(TEXT("plan.cinder-lattice"))
		&& CounterplayContact->PlanStage == 4
		&& CounterplayContact->bHasCoalitionCounterplay
		&& CounterplayContact->EscapeStrainMembers.Num() == 1
		&& CounterplayContact->EscapeStrainMembers[0].RegionId
			== FName(TEXT("region.cascadia"))
		&& CounterplayContact->EscapeStrainMembers[0].CurrentSupport == 27
		&& CounterplayContact->EscapeStrainMembers[0].ProjectedSupport == 19
		&& CounterplayContact->EscapeStrainMembers[0].bWouldWithdraw
		&& CounterplayContact->ThwartRecoveryMembers.Num() == 1
		&& CounterplayContact->ThwartRecoveryMembers[0].RegionId
			== FName(TEXT("region.north-atlantic"))
		&& CounterplayContact->ThwartRecoveryMembers[0].CurrentSupport == 30
		&& CounterplayContact->ThwartRecoveryMembers[0].ProjectedSupport == 40
		&& CounterplayContact->ThwartRecoveryMembers[0].bRemainsWithdrawn);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTGameInstanceCampaignLifecycleTest,
	"UEGT.Core.GameInstance.CampaignLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTGameInstanceCampaignLifecycleTest::RunTest(const FString& Parameters)
{
	UUEGTGameInstance* Instance = NewObject<UUEGTGameInstance>();
	TestNotNull(TEXT("Game instance adapter can be constructed"), Instance);
	if (Instance == nullptr)
	{
		return false;
	}

	FAdvanceStrategicTimeCommand UnavailableTime;
	const FStrategicCommandResult Unavailable = Instance->AdvanceStrategicTime(UnavailableTime);
	TestFalse(TEXT("Commands require an active campaign"), Unavailable.bAccepted);
	TestTrue(TEXT("Unavailable campaign has a stable diagnostic"), Unavailable.HasDiagnostic(TEXT("campaign_unavailable")));
	FRetuneMutualAidConvoyCommand UnavailableRetune;
	const FStrategicCommandResult UnavailableRetuneResult =
		Instance->RetuneMutualAidConvoy(UnavailableRetune);
	TestFalse(TEXT("Threadline Retune requires an active campaign"),
		UnavailableRetuneResult.bAccepted);
	TestTrue(TEXT("Unavailable Threadline Retune preserves the adapter diagnostic"),
		UnavailableRetuneResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FCommissionMutualAidSignalEscortCommand UnavailableSignalSurety;
	const FStrategicCommandResult UnavailableSignalSuretyResult =
		Instance->CommissionMutualAidSignalEscort(UnavailableSignalSurety);
	TestFalse(TEXT("Signal Surety requires an active campaign"),
		UnavailableSignalSuretyResult.bAccepted);
	TestTrue(TEXT("Unavailable Signal Surety preserves the adapter diagnostic"),
		UnavailableSignalSuretyResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FPrioritizeMutualAidConvoyCommand UnavailableReliefPriority;
	const FStrategicCommandResult UnavailableReliefPriorityResult =
		Instance->PrioritizeMutualAidConvoy(UnavailableReliefPriority);
	TestFalse(TEXT("Relief Priority requires an active campaign"),
		UnavailableReliefPriorityResult.bAccepted);
	TestTrue(TEXT("Unavailable Relief Priority preserves the adapter diagnostic"),
		UnavailableReliefPriorityResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FStandDownMutualAidConvoyCommand UnavailableReliefStandDown;
	const FStrategicCommandResult UnavailableReliefStandDownResult =
		Instance->StandDownMutualAidConvoy(UnavailableReliefStandDown);
	TestFalse(TEXT("Relief Stand-Down requires an active campaign"),
		UnavailableReliefStandDownResult.bAccepted);
	TestTrue(TEXT("Unavailable Relief Stand-Down preserves the adapter diagnostic"),
		UnavailableReliefStandDownResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FDivertMutualAidConvoyCommand UnavailableReliefDiversion;
	const FStrategicCommandResult UnavailableReliefDiversionResult =
		Instance->DivertMutualAidConvoy(UnavailableReliefDiversion);
	TestFalse(TEXT("Relief Diversion requires an active campaign"),
		UnavailableReliefDiversionResult.bAccepted);
	TestTrue(TEXT("Unavailable Relief Diversion preserves the adapter diagnostic"),
		UnavailableReliefDiversionResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FConfigureMutualAidRelayWaypointCommand UnavailableRelayWaypoint;
	const FStrategicCommandResult UnavailableRelayWaypointResult =
		Instance->ConfigureMutualAidRelayWaypoint(UnavailableRelayWaypoint);
	TestFalse(TEXT("Relay Waypoint configuration requires an active campaign"),
		UnavailableRelayWaypointResult.bAccepted);
	TestTrue(TEXT("Unavailable Relay Waypoint preserves the adapter diagnostic"),
		UnavailableRelayWaypointResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FConfigureMutualAidBalancedHandoffCommand UnavailableBalancedHandoff;
	const FStrategicCommandResult UnavailableBalancedHandoffResult =
		Instance->ConfigureMutualAidBalancedHandoff(UnavailableBalancedHandoff);
	TestFalse(TEXT("Balanced Handoff configuration requires an active campaign"),
		UnavailableBalancedHandoffResult.bAccepted);
	TestTrue(TEXT("Unavailable Balanced Handoff preserves the adapter diagnostic"),
		UnavailableBalancedHandoffResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FWithdrawInterceptionCommand UnavailableWithdrawal;
	const FStrategicCommandResult UnavailableWithdrawalResult =
		Instance->WithdrawInterception(UnavailableWithdrawal);
	TestFalse(TEXT("Interception withdrawal requires an active campaign"),
		UnavailableWithdrawalResult.bAccepted);
	TestTrue(TEXT("Unavailable interception withdrawal has a stable diagnostic"),
		UnavailableWithdrawalResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FResolveBaseAssaultCommand UnavailableBaseAssault;
	const FStrategicCommandResult UnavailableBaseAssaultResult = Instance->ResolveBaseAssault(UnavailableBaseAssault);
	TestFalse(TEXT("Base-defense commands require an active campaign"), UnavailableBaseAssaultResult.bAccepted);
	TestTrue(TEXT("Unavailable base defense has a stable diagnostic"),
		UnavailableBaseAssaultResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FDeployBaseDefenseOperationCommand UnavailableGroundDefense;
	const FStrategicCommandResult UnavailableGroundDefenseResult =
		Instance->DeployBaseDefenseOperation(UnavailableGroundDefense);
	TestFalse(TEXT("Ground-defense deployment requires an active campaign"), UnavailableGroundDefenseResult.bAccepted);
	TestTrue(TEXT("Unavailable ground defense has a stable diagnostic"),
		UnavailableGroundDefenseResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FSelectPersonnelDoctrineCommand UnavailableDoctrine;
	const FStrategicCommandResult UnavailableDoctrineResult =
		Instance->SelectPersonnelDoctrine(UnavailableDoctrine);
	TestFalse(TEXT("Personnel-doctrine choices require an active campaign"),
		UnavailableDoctrineResult.bAccepted);
	TestTrue(TEXT("Unavailable doctrine choice has a stable diagnostic"),
		UnavailableDoctrineResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FResolveCraftSalvageCommand UnavailableSalvage;
	const FStrategicCommandResult UnavailableSalvageResult =
		Instance->ResolveCraftSalvage(UnavailableSalvage);
	TestFalse(TEXT("Salvage disposition requires an active campaign"),
		UnavailableSalvageResult.bAccepted);
	TestTrue(TEXT("Unavailable salvage disposition has a stable diagnostic"),
		UnavailableSalvageResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FRearmCraftCommand UnavailableRearm;
	UnavailableRearm.Policy = ECraftRearmPolicy::LoadAvailable;
	const FStrategicCommandResult UnavailableRearmResult = Instance->RearmCraft(UnavailableRearm);
	TestFalse(TEXT("Load-available craft rearming requires an active campaign"),
		UnavailableRearmResult.bAccepted);
	TestTrue(TEXT("Unavailable craft rearming has a stable diagnostic"),
		UnavailableRearmResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FCancelCraftServiceCommand UnavailableServiceCancellation;
	const FStrategicCommandResult UnavailableServiceCancellationResult =
		Instance->CancelCraftService(UnavailableServiceCancellation);
	TestFalse(TEXT("Craft service cancellation requires an active campaign"),
		UnavailableServiceCancellationResult.bAccepted);
	TestTrue(TEXT("Unavailable craft service cancellation has a stable diagnostic"),
		UnavailableServiceCancellationResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FEjectTacticalMagazineCommand UnavailableMagazineEjection;
	const FStrategicCommandResult UnavailableMagazineEjectionResult =
		Instance->EjectTacticalMagazine(UnavailableMagazineEjection);
	TestFalse(TEXT("Tactical magazine ejection requires an active campaign"),
		UnavailableMagazineEjectionResult.bAccepted);
	TestTrue(TEXT("Unavailable magazine ejection has a stable diagnostic"),
		UnavailableMagazineEjectionResult.HasDiagnostic(TEXT("campaign_unavailable")));
	const FTacticalPathResult UnavailablePath = Instance->FindTacticalPath(FGuid(), FGuid(), 0, 0);
	TestFalse(TEXT("Path queries require an active campaign"), UnavailablePath.bSucceeded);
	TestTrue(TEXT("Unavailable path query has a stable diagnostic"), UnavailablePath.HasDiagnostic(TEXT("campaign_unavailable")));
	const FTacticalVisibilityResult UnavailableVisibility = Instance->ComputeTacticalVisibility(FGuid());
	TestFalse(TEXT("Visibility queries require an active campaign"), UnavailableVisibility.bSucceeded);
	TestTrue(TEXT("Unavailable visibility query has a stable diagnostic"), UnavailableVisibility.HasDiagnostic(TEXT("campaign_unavailable")));
	const FTacticalHudSnapshot UnavailableHud = Instance->BuildTacticalHudSnapshot(FGuid(), FTacticalHudQuery());
	TestFalse(TEXT("HUD snapshots require an active campaign"), UnavailableHud.bSucceeded);
	TestTrue(TEXT("Unavailable HUD snapshot has a stable diagnostic"), UnavailableHud.HasDiagnostic(TEXT("campaign_unavailable")));
	TestFalse(TEXT("A fresh game instance has no tactical debrief"), Instance->GetLastTacticalDebrief().bAvailable);
	const FTacticalAiDecision UnavailableAiDecision = Instance->ChooseTacticalAiAction(FGuid(), FGuid());
	TestFalse(TEXT("AI queries require an active campaign"), UnavailableAiDecision.bSucceeded);
	TestTrue(TEXT("Unavailable AI query has a stable diagnostic"), UnavailableAiDecision.HasDiagnostic(TEXT("campaign_unavailable")));
	const FTacticalAttackPreview UnavailableAttack = Instance->PreviewTacticalUnitAttack(
		FGuid(), FGuid(), FGuid(), NAME_None);
	TestFalse(TEXT("Attack previews require an active campaign"), UnavailableAttack.bSucceeded);
	TestTrue(TEXT("Unavailable attack preview has a stable diagnostic"), UnavailableAttack.HasDiagnostic(TEXT("campaign_unavailable")));
	const FTacticalAttackPreview UnavailableTerrainAttack = Instance->PreviewTacticalTerrainAttack(
		FGuid(), FGuid(), 0, 0, NAME_None);
	TestFalse(TEXT("Terrain attack previews require an active campaign"), UnavailableTerrainAttack.bSucceeded);
	TestTrue(TEXT("Unavailable terrain preview has a stable diagnostic"), UnavailableTerrainAttack.HasDiagnostic(TEXT("campaign_unavailable")));
	const FTacticalSignalPreview UnavailableSignal = Instance->PreviewTacticalSignal(
		FGuid(), FGuid(), FGuid(), NAME_None);
	TestFalse(TEXT("Signal previews require an active campaign"), UnavailableSignal.bSucceeded);
	TestTrue(TEXT("Unavailable signal preview has a stable diagnostic"),
		UnavailableSignal.HasDiagnostic(TEXT("campaign_unavailable")));
	FAttackTacticalUnitCommand UnavailableAttackCommand;
	const FStrategicCommandResult UnavailableAttackResult = Instance->AttackTacticalUnit(UnavailableAttackCommand);
	TestFalse(TEXT("Combat commands require an active campaign"), UnavailableAttackResult.bAccepted);
	TestTrue(TEXT("Unavailable combat command has a stable diagnostic"), UnavailableAttackResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FProjectTacticalSignalCommand UnavailableSignalCommand;
	const FStrategicCommandResult UnavailableSignalResult = Instance->ProjectTacticalSignal(UnavailableSignalCommand);
	TestFalse(TEXT("Signal commands require an active campaign"), UnavailableSignalResult.bAccepted);
	TestTrue(TEXT("Unavailable signal command has a stable diagnostic"),
		UnavailableSignalResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FChangeTacticalStanceCommand UnavailableStanceCommand;
	const FStrategicCommandResult UnavailableStanceResult = Instance->ChangeTacticalStance(UnavailableStanceCommand);
	TestFalse(TEXT("Stance commands require an active campaign"), UnavailableStanceResult.bAccepted);
	TestTrue(TEXT("Unavailable stance command has a stable diagnostic"), UnavailableStanceResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FSetTacticalDoorCommand UnavailableDoorCommand;
	const FStrategicCommandResult UnavailableDoorResult = Instance->SetTacticalDoor(UnavailableDoorCommand);
	TestFalse(TEXT("Door commands require an active campaign"), UnavailableDoorResult.bAccepted);
	TestTrue(TEXT("Unavailable door command has a stable diagnostic"), UnavailableDoorResult.HasDiagnostic(TEXT("campaign_unavailable")));
	FRunTacticalAiTurnCommand UnavailableAiTurnCommand;
	const FStrategicCommandResult UnavailableAiTurnResult = Instance->RunTacticalAiTurn(UnavailableAiTurnCommand);
	TestFalse(TEXT("AI turn commands require an active campaign"), UnavailableAiTurnResult.bAccepted);
	TestTrue(TEXT("Unavailable AI turn has a stable diagnostic"), UnavailableAiTurnResult.HasDiagnostic(TEXT("campaign_unavailable")));
	const FStrategicDashboardSnapshot UnavailableDashboard = Instance->BuildStrategicDashboard();
	TestFalse(TEXT("Strategic dashboards require ready content and an active campaign"), UnavailableDashboard.bSucceeded);
	TestTrue(TEXT("Unavailable strategic dashboard explains its state"), !UnavailableDashboard.Diagnostics.IsEmpty());

	TestTrue(TEXT("Packaged project rules load through lifecycle adapter"), Instance->ReloadContent());
	TestTrue(TEXT("Content-ready state is exposed"), Instance->IsContentReady());
	TestEqual(TEXT("Loaded package versions are exposed"), Instance->GetLoadedContentVersions().Num(), 1);
	TestFalse(TEXT("No campaign is active before setup"), Instance->HasActiveCampaign());
	TestFalse(TEXT("Content-ready instance still requires a campaign for its dashboard"), Instance->BuildStrategicDashboard().bSucceeded);
	TestTrue(TEXT("New standard campaign starts"), Instance->StartNewCampaign(
		ECampaignDifficulty::Standard, 12345, EUEGTFundingModel::BalancedMandate));
	TestTrue(TEXT("Campaign-active state is exposed"), Instance->HasActiveCampaign());
	TestEqual(TEXT("Standard campaign receives deterministic starting funds"), Instance->GetCampaignState().Funds, int64(1500000));
	TestEqual(TEXT("Campaign seed reaches deterministic stream"), Instance->GetCampaignState().SimulationRandom.InitialSeed, int64(12345));
	TestNotNull(TEXT("Loaded rules expose opening adversary mission"), Instance->GetLoadedRules().AdversaryMissions.Find(TEXT("mission.glass-tide-survey")));
	TestNotNull(TEXT("Loaded rules expose authored regional mandate partners"), Instance->GetLoadedRules().Regions.Find(TEXT("region.cascadia")));
	TestNotNull(TEXT("Loaded rules expose the original base-defense facility"), Instance->GetLoadedRules().Facilities.Find(TEXT("facility.aegis-battery")));
	TestNotNull(TEXT("Loaded rules expose the original base-targeting raid"), Instance->GetLoadedRules().AdversaryMissions.Find(TEXT("mission.nightglass-raid")));
	TestNotNull(TEXT("Loaded rules expose an original tactical mission"), Instance->GetLoadedRules().TacticalMissions.Find(TEXT("tactical.glass-wreck-recovery")));
	TestEqual(TEXT("New campaign initializes all mission regions"), Instance->GetCampaignState().RegionalPressure.Num(), 3);
	TestEqual(TEXT("New campaign initializes matching regional mandates"), Instance->GetCampaignState().RegionalMandates.Num(), 3);
	TestEqual(TEXT("New campaign uses configured adversary delay"), Instance->GetCampaignState().NextAdversaryMissionSeconds, int64(24 * 3600));
	TestTrue(TEXT("New campaign outcome begins ongoing"), Instance->GetCampaignState().Outcome == ECampaignOutcome::Ongoing);
	const FStrategicDashboardSnapshot FoundingDashboard = Instance->BuildStrategicDashboard();
	TestTrue(TEXT("New campaign exposes a strategic founding dashboard"), FoundingDashboard.bSucceeded && FoundingDashboard.bRequiresBase);
	TestEqual(TEXT("Founding dashboard exposes all loaded mission regions"), FoundingDashboard.Regions.Num(), 3);
	TestTrue(TEXT("Founding dashboard exposes mandate support, all three outreach choices, and charter eligibility"),
		FoundingDashboard.Regions[0].bHasMandate
		&& FoundingDashboard.Regions[0].ActionOptions.Num() == 3
		&& FoundingDashboard.Regions[0].ActionOptions[2].ActionType
			== ERegionalDiplomacyActionType::CrisisMobilization
		&& !FoundingDashboard.Regions[0].ActionOptions[2].bEnabled
		&& FoundingDashboard.Regions[0].ActionOptions[2].UnavailableReasonCode
			== FName(TEXT("regional_crisis_not_severe"))
		&& !FoundingDashboard.Regions[0].ResilienceCharter.bEnabled
		&& FoundingDashboard.Regions[0].ResilienceCharter.UnavailableReasonCode
			== FName(TEXT("regional_charter_support_required"))
		&& FoundingDashboard.Regions[1].ResilienceCharter.bEnabled
		&& FoundingDashboard.Regions[1].ResilienceCharter.ProjectedMonthlyFunding == 157500);

	FEstablishBaseCommand Base;
	Base.ExpectedSequence = 0;
	Base.BaseId = FGuid(101, 102, 103, 104);
	Base.Name = TEXT("Cascadia Watch");
	Base.RegionId = TEXT("region.cascadia");
	Base.LongitudeMilliDegrees = -123120;
	Base.LatitudeMilliDegrees = 49280;
	Base.StartingFacilities.Add(TEXT("facility.operations-hub"));
	const FStrategicCommandResult Established = Instance->EstablishBase(Base);
	TestTrue(TEXT("Lifecycle adapter executes base command"), Established.bAccepted);
	TestEqual(TEXT("Loaded rule cost participates in command"), Instance->GetCampaignState().Funds, int64(820000));
	const FStrategicDashboardSnapshot BaseDashboard = Instance->BuildStrategicDashboard();
	TestTrue(TEXT("Established base reaches the strategic presentation adapter"),
		BaseDashboard.bSucceeded && !BaseDashboard.bRequiresBase && BaseDashboard.Bases.Num() == 1);
	TestEqual(TEXT("Dashboard command sequence follows accepted base command"), BaseDashboard.ExpectedCommandSequence, int64(1));
	FDismantleFacilityCommand ProtectedHub;
	ProtectedHub.ExpectedSequence = BaseDashboard.ExpectedCommandSequence;
	ProtectedHub.BaseId = Base.BaseId;
	ProtectedHub.FacilityInstanceId = BaseDashboard.Bases[0].FacilityLayout[0].FacilityInstanceId;
	const FStrategicCommandResult ProtectedHubResult = Instance->DismantleFacility(ProtectedHub);
	TestFalse(TEXT("Lifecycle adapter preserves operational command-hub protection"), ProtectedHubResult.bAccepted);
	TestTrue(TEXT("Lifecycle dismantling rejection preserves the domain diagnostic"),
		ProtectedHubResult.HasDiagnostic(TEXT("operations_facility_required")));

	FStartResearchCommand Start;
	Start.ExpectedSequence = 1;
	Start.BaseId = Base.BaseId;
	Start.ResearchId = TEXT("research.signal-analysis");
	TestTrue(TEXT("Lifecycle adapter starts loaded research"), Instance->StartResearch(Start).bAccepted);
	FSetResearchStaffCommand Staff;
	Staff.ExpectedSequence = 2;
	Staff.ResearchId = Start.ResearchId;
	Staff.AssignedScientists = 3;
	TestTrue(TEXT("Lifecycle adapter assigns research staff"), Instance->SetResearchStaff(Staff).bAccepted);
	FSetSignalWatchStaffCommand SignalWatch;
	SignalWatch.ExpectedSequence = 3;
	SignalWatch.BaseId = Base.BaseId;
	SignalWatch.AssignedScientists = 1;
	const FStrategicCommandResult SignalWatchResult = Instance->SetSignalWatchStaff(SignalWatch);
	TestTrue(TEXT("Lifecycle adapter routes Signal Watch staffing with policy telemetry"),
		SignalWatchResult.bAccepted
		&& SignalWatchResult.HasEvent(EStrategicEventType::MutualAidSignalWatchStaffChanged)
		&& Instance->GetCampaignState().Bases[0].SignalWatchScientists == 1);
	FSetWorksCadreStaffCommand WorksCadre;
	WorksCadre.ExpectedSequence = 4;
	WorksCadre.BaseId = Base.BaseId;
	WorksCadre.AssignedEngineers = 1;
	const FStrategicCommandResult WorksCadreResult = Instance->SetWorksCadreStaff(WorksCadre);
	TestTrue(TEXT("Lifecycle adapter routes Works Cadre staffing with policy telemetry"),
		WorksCadreResult.bAccepted
		&& WorksCadreResult.HasEvent(EStrategicEventType::WorksCadreStaffChanged)
		&& Instance->GetCampaignState().Bases[0].WorksCadreEngineers == 1);
	FSetWorksCadreCharterCommand WorksCharter;
	WorksCharter.ExpectedSequence = 5;
	WorksCharter.BaseId = Base.BaseId;
	WorksCharter.Charter = EWorksCadreCharter::AssemblyCadence;
	const FStrategicCommandResult WorksCharterResult =
		Instance->SetWorksCadreCharter(WorksCharter);
	TestTrue(TEXT("Lifecycle adapter routes Works Charter selection with policy telemetry"),
		WorksCharterResult.bAccepted
		&& WorksCharterResult.HasEvent(
			EStrategicEventType::WorksCadreCharterChanged)
		&& Instance->GetCampaignState().Bases[0].WorksCadreCharter
			== EWorksCadreCharter::AssemblyCadence);
	FAdvanceStrategicTimeCommand Advance;
	Advance.ExpectedSequence = 6;
	Advance.Rate = EStrategicTimeRate::FiveMinutes;
	TestTrue(TEXT("Lifecycle adapter advances deterministic simulation"), Instance->AdvanceStrategicTime(Advance).bAccepted);
	FStartFacilityConstructionCommand Construction;
	Construction.ExpectedSequence = 7;
	Construction.ProjectId = FGuid(201, 202, 203, 204);
	Construction.FacilityInstanceId = FGuid(205, 206, 207, 208);
	Construction.BaseId = Base.BaseId;
	Construction.FacilityId = TEXT("facility.secure-storage");
	Construction.GridX = 2;
	Construction.GridY = 0;
	TestTrue(TEXT("Lifecycle adapter starts grid-validated construction"), Instance->StartFacilityConstruction(Construction).bAccepted);
	FRecruitPersonnelCommand Recruit;
	Recruit.ExpectedSequence = 8;
	Recruit.OrderId = FGuid(209, 210, 211, 212);
	Recruit.PersonnelId = FGuid(213, 214, 215, 216);
	Recruit.BaseId = Base.BaseId;
	Recruit.RoleId = TEXT("role.field-agent");
	Recruit.DisplayName = TEXT("Imani Cross");
	TestTrue(TEXT("Lifecycle adapter starts data-driven recruitment"), Instance->RecruitPersonnel(Recruit).bAccepted);
	FApplyFacilityDamageCommand FacilityDamage;
	FacilityDamage.ExpectedSequence = 9;
	FacilityDamage.BaseId = Base.BaseId;
	FacilityDamage.FacilityInstanceId = ProtectedHub.FacilityInstanceId;
	FacilityDamage.Damage = 1;
	TestTrue(TEXT("Lifecycle adapter routes facility damage"),
		Instance->ApplyFacilityDamage(FacilityDamage).bAccepted);
	FStartFacilityRepairCommand FacilityRepair;
	FacilityRepair.ExpectedSequence = 10;
	FacilityRepair.BaseId = Base.BaseId;
	FacilityRepair.FacilityInstanceId = ProtectedHub.FacilityInstanceId;
	TestTrue(TEXT("Lifecycle adapter routes facility repair start"),
		Instance->StartFacilityRepair(FacilityRepair).bAccepted);
	FCancelFacilityRepairCommand CancelFacilityRepair;
	CancelFacilityRepair.ExpectedSequence = 11;
	CancelFacilityRepair.BaseId = Base.BaseId;
	CancelFacilityRepair.FacilityInstanceId = ProtectedHub.FacilityInstanceId;
	TestTrue(TEXT("Lifecycle adapter routes facility repair cancellation"),
		Instance->CancelFacilityRepair(CancelFacilityRepair).bAccepted);
	const FCampaignState State = Instance->GetCampaignState();
	TestEqual(TEXT("Command sequence reflects all accepted commands"), State.CommandSequence, int64(12));
	TestEqual(TEXT("Research project remains active"), State.ResearchProjects.Num(), 1);
	if (State.ResearchProjects.Num() == 1)
	{
		TestEqual(TEXT("Research work is exact through Unreal adapter"), State.ResearchProjects[0].AccumulatedWorkSeconds, int64(900));
	}
	TestEqual(TEXT("Construction project is exposed through Unreal adapter"), State.FacilityConstructionProjects.Num(), 1);
	TestEqual(TEXT("Recruitment order is exposed through Unreal adapter"), State.RecruitmentOrders.Num(), 1);
	TestEqual(TEXT("Signal Watch staffing remains exposed through Unreal adapter"),
		State.Bases[0].SignalWatchScientists, 1);
	TestEqual(TEXT("Works Cadre staffing remains exposed through Unreal adapter"),
		State.Bases[0].WorksCadreEngineers, 1);
	TestEqual(TEXT("Works Charter remains exposed through Unreal adapter"),
		State.Bases[0].WorksCadreCharter,
		EWorksCadreCharter::AssemblyCadence);
	TestTrue(TEXT("Cancelled facility repair leaves damage but clears the reservation"),
		State.Bases[0].Facilities[0].Damage == 1
		&& State.Bases[0].Facilities[0].ReservedRepairDamage == 0
		&& State.Bases[0].Facilities[0].RemainingRepairSeconds == 0);
	TestFalse(TEXT("Content cannot hot-reload under an active campaign"), Instance->ReloadContent());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTGameInstanceCraftLifecycleTest,
	"UEGT.Core.GameInstance.CraftLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTGameInstanceCraftLifecycleTest::RunTest(const FString& Parameters)
{
	UUEGTGameInstance* Instance = NewObject<UUEGTGameInstance>();
	TestNotNull(TEXT("Craft lifecycle adapter can be constructed"), Instance);
	if (Instance == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Packaged craft rules load"), Instance->ReloadContent());
	TestNotNull(TEXT("Loaded rules expose the Sparrow craft"), Instance->GetLoadedRules().Craft.Find(TEXT("craft.sparrow-interceptor")));
	TestNotNull(TEXT("Loaded rules expose the Skimmer contact"), Instance->GetLoadedRules().Contacts.Find(TEXT("contact.skimmer")));
	TestTrue(TEXT("Cadet campaign starts for craft lifecycle"), Instance->StartNewCampaign(
		ECampaignDifficulty::Cadet, 67890, EUEGTFundingModel::BalancedMandate));

	FEstablishBaseCommand Base;
	Base.ExpectedSequence = 0;
	Base.BaseId = FGuid(301, 302, 303, 304);
	Base.Name = TEXT("Pacific Flight Operations");
	Base.RegionId = TEXT("region.cascadia");
	Base.LongitudeMilliDegrees = -123120;
	Base.LatitudeMilliDegrees = 49280;
	Base.StartingFacilities = { TEXT("facility.operations-hub"), TEXT("facility.flight-deck") };
	TestTrue(TEXT("Lifecycle establishes a base with flight capacity"), Instance->EstablishBase(Base).bAccepted);

	FAcquireCraftCommand Acquire;
	Acquire.ExpectedSequence = 1;
	Acquire.OrderId = FGuid(305, 306, 307, 308);
	Acquire.CraftId = FGuid(309, 310, 311, 312);
	Acquire.BaseId = Base.BaseId;
	Acquire.CraftRuleId = TEXT("craft.sparrow-interceptor");
	Acquire.DisplayName = TEXT("Pacific Kestrel");
	const FStrategicCommandResult Acquired = Instance->AcquireCraft(Acquire);
	TestTrue(TEXT("Lifecycle adapter starts craft acquisition"), Acquired.bAccepted);
	TestTrue(TEXT("Lifecycle craft acquisition emits an event"), Acquired.HasEvent(EStrategicEventType::CraftAcquisitionStarted));
	TestEqual(TEXT("Lifecycle exposes incoming craft"), Instance->GetCampaignState().CraftAcquisitionOrders.Num(), 1);
	TestEqual(TEXT("Loaded craft purchase cost is applied"), Instance->GetCampaignState().Funds, int64(220000));

	FCreateStrategicContactCommand CreateContact;
	CreateContact.ExpectedSequence = 2;
	CreateContact.ContactId = FGuid(313, 314, 315, 316);
	CreateContact.ContactRuleId = TEXT("contact.skimmer");
	CreateContact.OriginLongitudeMilliDegrees = -123120;
	CreateContact.OriginLatitudeMilliDegrees = 49280;
	CreateContact.DestinationLongitudeMilliDegrees = -100000;
	CreateContact.DestinationLatitudeMilliDegrees = 49280;
	const FStrategicCommandResult ContactCreated = Instance->CreateStrategicContact(CreateContact);
	TestTrue(TEXT("Lifecycle adapter creates a strategic contact"), ContactCreated.bAccepted);
	TestTrue(TEXT("Lifecycle exposes created strategic contact"), Instance->GetCampaignState().StrategicContacts.Num() == 1);

	return true;
}

#endif
