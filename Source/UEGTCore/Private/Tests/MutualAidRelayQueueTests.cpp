// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/MutualAidRelayQueue.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"
#include "Strategic/StrategicCommandService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMutualAidRelayQueueEvaluationTest,
	"UEGT.Core.Strategic.MutualAidRelayQueue.DeterministicScheduling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMutualAidRelayQueueEvaluationTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FFacilityRule Operations;
	Operations.Identity.RuleId = TEXT("facility.operations-hub");
	Operations.DisplayName = TEXT("Operations Hub");
	Operations.DetectionStrength = 20;
	Operations.MaxIntegrity = 100;
	Rules.Facilities.Add(Operations.Identity.RuleId, Operations);
	FFacilityRule LongRangeArray;
	LongRangeArray.Identity.RuleId = TEXT("facility.long-range-array");
	LongRangeArray.DisplayName = TEXT("Long-Range Array");
	LongRangeArray.DetectionStrength = 70;
	LongRangeArray.MaxIntegrity = 100;
	Rules.Facilities.Add(LongRangeArray.Identity.RuleId, LongRangeArray);

	FCampaignState Campaign;
	Campaign.CommandSequence = 20;
	FStrategicBaseState& Source = Campaign.Bases.AddDefaulted_GetRef();
	Source.BaseId = FGuid(1, 2, 3, 4);
	Source.Name = TEXT("Relay Source");
	FBaseFacilityState& Hub = Source.Facilities.AddDefaulted_GetRef();
	Hub.InstanceId = FGuid(10, 2, 3, 4);
	Hub.FacilityId = Operations.Identity.RuleId;
	FBaseFacilityState& Array = Source.Facilities.AddDefaulted_GetRef();
	Array.InstanceId = FGuid(11, 2, 3, 4);
	Array.FacilityId = LongRangeArray.Identity.RuleId;
	Array.GridX = 1;

	const FGuid DestinationBaseId(5, 6, 7, 8);
	const auto AddConvoy = [&](const FGuid ConvoyId, const int64 DispatchSequence,
		const int64 RemainingSeconds) -> FMutualAidConvoyState&
	{
		FMutualAidConvoyState& Convoy = Campaign.MutualAidConvoys.AddDefaulted_GetRef();
		Convoy.ConvoyId = ConvoyId;
		Convoy.SourceBaseId = Source.BaseId;
		Convoy.DestinationBaseId = DestinationBaseId;
		Convoy.ItemId = TEXT("item.relief-kit");
		Convoy.Quantity = 1;
		Convoy.DispatchSequence = DispatchSequence;
		Convoy.TotalTransitSeconds = RemainingSeconds;
		Convoy.RemainingTransitSeconds = RemainingSeconds;
		Convoy.bInterdictionResolved = true;
		Convoy.ForecastInterdictionDelaySeconds = 24 * 3600;
		return Convoy;
	};
	const FGuid FirstId(101, 2, 3, 4);
	const FGuid SecondId(102, 2, 3, 4);
	const FGuid ThirdId(103, 2, 3, 4);
	AddConvoy(FirstId, 10, 72 * 3600);
	AddConvoy(SecondId, 11, 24 * 3600);
	AddConvoy(ThirdId, 12, 48 * 3600);

	const FMutualAidRelayQueueSnapshot ThreeChannels =
		FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	const FMutualAidRelayQueueView* First = ThreeChannels.FindConvoy(FirstId);
	const FMutualAidRelayQueueView* Second = ThreeChannels.FindConvoy(SecondId);
	const FMutualAidRelayQueueView* Third = ThreeChannels.FindConvoy(ThirdId);
	const FMutualAidRelayQueueBaseView* SourceTelemetry =
		ThreeChannels.FindBase(Source.BaseId);
	TestTrue(TEXT("A specialized Signal Relay base adds one active Relay Weave channel"),
		ThreeChannels.PolicyId == FName(TEXT("logistics.mutual-aid-relay-weave"))
		&& First != nullptr && Second != nullptr && Third != nullptr
		&& SourceTelemetry != nullptr && ThreeChannels.Bases.Num() == 1
		&& First->FacilityRelayChannelCount == 3
		&& First->SpecializationRelayChannelBonus == 1
		&& First->RelayChannelCount == 4 && First->bInTransit
		&& First->QueuePosition == 1 && First->RelayChannelNumber == 1
		&& Second->bInTransit && Second->QueuePosition == 2
		&& Second->RelayChannelNumber == 2 && Second->WaitingConvoyCount == 0
		&& Third->bInTransit && Third->QueuePosition == 3
		&& Third->RelayChannelNumber == 3 && Third->QueuePressurePercent == 0
		&& SourceTelemetry->ActiveConvoyCount == 3
		&& SourceTelemetry->TotalConvoyCount == 3
		&& SourceTelemetry->FacilityRelayChannelCount == 3
		&& SourceTelemetry->SpecializationRelayChannelBonus == 1
		&& SourceTelemetry->WaitingConvoyCount == 0
		&& SourceTelemetry->QueuePressurePercent == 0
		&& SourceTelemetry->QueueTailArrivalSeconds == int64(72) * 3600);

	const FMutualAidRelayQueueView Prospective = FMutualAidRelayQueue::ProjectNext(
		Campaign, Rules, Source.BaseId, 36 * 3600);
	TestTrue(TEXT("The next convoy uses the specialized channel without a FIFO hold"),
		Prospective.bValid && Prospective.bRelayAvailable && Prospective.bInTransit
		&& Prospective.QueuePosition == 4 && Prospective.WaitingPosition == 0
		&& Prospective.WaitingConvoyCount == 0 && Prospective.QueuePressurePercent == 0
		&& Prospective.RelayChannelNumber == 4
		&& Prospective.EstimatedWaitSeconds == 0
		&& Prospective.EstimatedArrivalSeconds == 36 * 3600);

	FCampaignState TerminalSequenceCampaign = Campaign;
	TerminalSequenceCampaign.CommandSequence = MAX_int64 - 3;
	const FMutualAidRelayQueueView TerminalProjection = FMutualAidRelayQueue::ProjectNext(
		TerminalSequenceCampaign, Rules, Source.BaseId, 36 * 3600);
	TestFalse(TEXT("Relay projection does not synthesize an unavailable terminal command sequence"),
		TerminalProjection.bValid);

	Algo::Reverse(Campaign.MutualAidConvoys);
	const FMutualAidRelayQueueView Reordered = FMutualAidRelayQueue::ProjectNext(
		Campaign, Rules, Source.BaseId, 36 * 3600);
	TestTrue(TEXT("Persisted array order cannot change FIFO scheduling"),
		Reordered.QueuePosition == Prospective.QueuePosition
		&& Reordered.RelayChannelNumber == Prospective.RelayChannelNumber
		&& Reordered.EstimatedWaitSeconds == Prospective.EstimatedWaitSeconds
		&& Reordered.EstimatedArrivalSeconds == Prospective.EstimatedArrivalSeconds);

	Array.Damage = 50;
	const FMutualAidRelayQueueView Degraded = FMutualAidRelayQueue::ProjectNext(
		Campaign, Rules, Source.BaseId, 36 * 3600);
	TestTrue(TEXT("Progressive array damage retains the specialized channel while reducing facility output"),
		Degraded.FacilityRelayChannelCount == 2
		&& Degraded.SpecializationRelayChannelBonus == 1
		&& Degraded.RelayChannelCount == 3 && !Degraded.bInTransit
		&& Degraded.WaitingConvoyCount == 1 && Degraded.QueuePressurePercent == 25
		&& Degraded.WaitingPosition == 1
		&& Degraded.EstimatedWaitSeconds == 24 * 3600
		&& Degraded.EstimatedArrivalSeconds == 60 * 3600);

	Hub.Damage = Operations.MaxIntegrity;
	Array.Damage = LongRangeArray.MaxIntegrity;
	const FMutualAidRelayQueueSnapshot Offline =
		FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	const FMutualAidRelayQueueView OfflineProspective = FMutualAidRelayQueue::ProjectNext(
		Campaign, Rules, Source.BaseId, 36 * 3600);
	TestTrue(TEXT("A total signal outage holds every existing convoy without inventing an ETA"),
		Offline.Convoys.Num() == 3
		&& Offline.Bases.Num() == 1
		&& Offline.Bases[0].WaitingConvoyCount == 3
		&& Offline.Bases[0].QueuePressurePercent == 100
		&& Offline.Convoys.ContainsByPredicate(
			[](const FMutualAidRelayQueueView& View)
			{
				return View.bValid && !View.bRelayAvailable && !View.bInTransit
					&& View.EstimatedWaitSeconds == 0 && View.EstimatedArrivalSeconds == 0;
			})
		&& OfflineProspective.bValid && !OfflineProspective.bRelayAvailable);

	FCampaignState Legacy = Campaign;
	Legacy.Bases[0].Facilities.Reset();
	Legacy.Bases[0].BuiltFacilities = {
		Operations.Identity.RuleId, LongRangeArray.Identity.RuleId };
	TestEqual(TEXT("Legacy facility lists retain full-strength relay capacity and specialization"),
		FMutualAidRelayQueue::EvaluateRelayChannelCount(Legacy.Bases[0], Rules), 4);

	FMutualAidConvoyState Pending = Campaign.MutualAidConvoys[0];
	Pending.RemainingTransitSeconds = 40 * 3600;
	Pending.TotalTransitSeconds = 48 * 3600;
	Pending.bInterdictionResolved = false;
	Pending.bSignalEscort = false;
	Pending.ForecastInterdictionDelaySeconds = 24 * 3600;
	TestEqual(TEXT("A pending deterministic delay participates in exact queue readiness"),
		FMutualAidRelayQueue::ProjectedJourneySeconds(Pending), int64(64) * 3600);
	Pending.bSignalEscort = true;
	TestEqual(TEXT("Signal Escort removes the pending delay from queue readiness"),
		FMutualAidRelayQueue::ProjectedJourneySeconds(Pending), int64(40) * 3600);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMutualAidRelayQueueSpecializationBenefitTest,
	"UEGT.Core.Strategic.MutualAidRelayQueue.SpecializationBenefit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMutualAidRelayQueueSpecializationBenefitTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FFacilityRule Relay;
	Relay.Identity.RuleId = TEXT("facility.specialization-relay");
	Relay.DisplayName = TEXT("Specialization Relay");
	Relay.DetectionStrength = 70;
	Relay.StorageCapacity = 0;
	Relay.MaxIntegrity = 100;
	Rules.Facilities.Add(Relay.Identity.RuleId, Relay);

	FCampaignState Campaign;
	Campaign.CommandSequence = 41;
	FStrategicBaseState& Base = Campaign.Bases.AddDefaulted_GetRef();
	Base.BaseId = FGuid(201, 202, 203, 204);
	Base.Name = TEXT("Specialization Relay Base");
	FBaseFacilityState& Facility = Base.Facilities.AddDefaulted_GetRef();
	Facility.InstanceId = FGuid(205, 206, 207, 208);
	Facility.FacilityId = Relay.Identity.RuleId;

	const uint64 InitialRandomState = Campaign.SimulationRandom.GetStateForSave();
	const int64 InitialRandomDraws = Campaign.SimulationRandom.DrawCount;
	const FGuid InitialBaseId = Base.BaseId;
	const int32 InitialFacilityCount = Base.Facilities.Num();
	const FStrategicBaseSpecializationView Specialization =
		FStrategicCommandService::EvaluateBaseSpecialization(Base, Rules);
	const int32 FacilityChannelCount =
		FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Base, Rules);
	const int32 TotalChannelCount =
		FMutualAidRelayQueue::EvaluateRelayChannelCount(Base, Rules);
	const FMutualAidRelayQueueSnapshot Snapshot =
		FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	const FMutualAidRelayQueueBaseView* BaseView = Snapshot.FindBase(Base.BaseId);

	TestTrue(TEXT("Signal Relay specialization supplies one authoritative Relay Weave channel"),
		Specialization.bSpecialized
		&& Specialization.SpecializationId == FName(TEXT("base.specialization.signal-relay"))
		&& Specialization.OperationalBenefitMetricId
			== FName(TEXT("base.specialization.relay-channels"))
		&& Specialization.OperationalBenefitValue == 1
		&& FacilityChannelCount == 2
		&& TotalChannelCount == FacilityChannelCount + 1
		&& BaseView != nullptr
		&& BaseView->FacilityRelayChannelCount == FacilityChannelCount
		&& BaseView->SpecializationRelayChannelBonus == 1
		&& BaseView->RelayChannelCount == TotalChannelCount
		&& BaseView->SignalWatchBonusChannelCount == 0
		&& Campaign.Bases.Num() == 1
		&& Base.BaseId == InitialBaseId
		&& Base.Facilities.Num() == InitialFacilityCount
		&& Campaign.SimulationRandom.GetStateForSave() == InitialRandomState
		&& Campaign.SimulationRandom.DrawCount == InitialRandomDraws);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMutualAidRelayQueueBalanceCorpusTest,
	"UEGT.Core.Strategic.MutualAidRelayQueue.SeededPressureCorpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMutualAidRelayQueueBalanceCorpusTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	const auto AddFacilityRule = [&Rules](
		const FName RuleId,
		const int32 DetectionStrength,
		const int32 MaxIntegrity)
	{
		FFacilityRule Rule;
		Rule.Identity.RuleId = RuleId;
		Rule.DisplayName = RuleId.ToString();
		Rule.DetectionStrength = DetectionStrength;
		Rule.MaxIntegrity = MaxIntegrity;
		Rules.Facilities.Add(Rule.Identity.RuleId, Rule);
	};
	AddFacilityRule(TEXT("facility.corpus-relay-hub"), 20, 100);
	AddFacilityRule(TEXT("facility.corpus-relay-array"), 50, 200);
	AddFacilityRule(TEXT("facility.corpus-relay-lattice"), 100, 300);

	constexpr int32 ScenarioCount = 1024;
	constexpr int32 MaximumConvoysPerScenario = 16;
	int32 ScenariosWithPressure = 0;
	int32 ScenariosWithoutCapacity = 0;
	int32 ScenariosWithMultipleChannels = 0;
	int32 MaximumPressure = 0;
	int64 TotalWaitingConvoys = 0;
	bool bCorpusValid = true;
	bool bReportedFailure = false;
	for (int32 Seed = 0; Seed < ScenarioCount; ++Seed)
	{
		bool bScenarioValid = true;
		int32 FirstFailureKind = 0;
		FCampaignState Campaign;
		Campaign.CommandSequence = 10000 + Seed;
		const uint64 InitialSimulationState = Campaign.SimulationRandom.GetStateForSave();
		const int64 InitialSimulationDraws = Campaign.SimulationRandom.DrawCount;
		FStrategicBaseState& Source = Campaign.Bases.AddDefaulted_GetRef();
		Source.BaseId = FGuid(
			static_cast<uint32>(0x6c000000 + Seed), 0x6c000001, 0x6c000002, 0x6c000003);
		Source.Name = TEXT("Corpus Relay Source");

		const int32 FacilityCount = 1 + (Seed % 4);
		for (int32 FacilityIndex = 0; FacilityIndex < FacilityCount; ++FacilityIndex)
		{
			const int32 RuleIndex = (Seed + FacilityIndex) % 3;
			const FName RuleId = RuleIndex == 0
				? FName(TEXT("facility.corpus-relay-hub"))
				: RuleIndex == 1
					? FName(TEXT("facility.corpus-relay-array"))
					: FName(TEXT("facility.corpus-relay-lattice"));
			const FFacilityRule& Rule = Rules.Facilities.FindChecked(RuleId);
			FBaseFacilityState& Facility = Source.Facilities.AddDefaulted_GetRef();
			Facility.InstanceId = FGuid(
				static_cast<uint32>(0x6c100000 + Seed),
				static_cast<uint32>(0x6c200000 + FacilityIndex), 0x6c300000, 0x6c400000);
			Facility.FacilityId = RuleId;
			Facility.Damage = Seed % 11 == 0
				? Rule.MaxIntegrity
				: ((Seed + 3) * (FacilityIndex + 5) * 17) % (Rule.MaxIntegrity + 1);
		}

		const int32 ConvoyCount = 1 + (Seed % MaximumConvoysPerScenario);
		for (int32 ConvoyIndex = 0; ConvoyIndex < ConvoyCount; ++ConvoyIndex)
		{
			FMutualAidConvoyState& Convoy = Campaign.MutualAidConvoys.AddDefaulted_GetRef();
			Convoy.ConvoyId = FGuid(
				static_cast<uint32>(0x6c500000 + Seed),
				static_cast<uint32>(0x6c600000 + ConvoyIndex), 0x6c700000, 0x6c800000);
			Convoy.SourceBaseId = Source.BaseId;
			Convoy.DestinationBaseId = FGuid(0x6c900000, 0x6c900001, 0x6c900002, 0x6c900003);
			Convoy.DispatchSequence = 100 + ConvoyIndex;
			Convoy.TotalTransitSeconds = static_cast<int64>(
				12 + ((Seed * 11 + ConvoyIndex * 19) % 325)) * 3600;
			Convoy.RemainingTransitSeconds = Convoy.TotalTransitSeconds;
			Convoy.bInterdictionResolved = (Seed + ConvoyIndex) % 3 != 0;
			Convoy.ForecastInterdictionDelaySeconds = static_cast<int64>(
				6 + ((Seed + ConvoyIndex * 5) % 57)) * 3600;
		}
		Algo::Reverse(Campaign.MutualAidConvoys);

		const FMutualAidRelayQueueSnapshot Snapshot =
			FMutualAidRelayQueue::Evaluate(Campaign, Rules);
		const FMutualAidRelayQueueBaseView* BaseView = Snapshot.FindBase(Source.BaseId);
		const int32 RelayChannelCount = FMutualAidRelayQueue::EvaluateRelayChannelCount(
			Source, Rules);
		const int32 ExpectedActiveCount = FMath::Min(RelayChannelCount, ConvoyCount);
		const int32 ExpectedWaitingCount = FMath::Max(0, ConvoyCount - ExpectedActiveCount);
		const int32 ExpectedPressure = ConvoyCount <= 0
			? 0
			: FMath::Clamp<int32>(static_cast<int32>(
				(static_cast<int64>(ExpectedWaitingCount) * 100 + ConvoyCount - 1) / ConvoyCount), 0, 100);
		ScenariosWithPressure += ExpectedWaitingCount > 0 ? 1 : 0;
		ScenariosWithoutCapacity += RelayChannelCount <= 0 ? 1 : 0;
		ScenariosWithMultipleChannels += RelayChannelCount >= 2 ? 1 : 0;
		MaximumPressure = FMath::Max(MaximumPressure, ExpectedPressure);
		TotalWaitingConvoys += ExpectedWaitingCount;

		TArray<int32> OrderedIndices;
		OrderedIndices.Reserve(ConvoyCount);
		for (int32 Index = 0; Index < ConvoyCount; ++Index)
		{
			OrderedIndices.Add(Index);
		}
		OrderedIndices.Sort(
			[&Campaign](const int32 Left, const int32 Right)
			{
				return Campaign.MutualAidConvoys[Left].DispatchSequence
					< Campaign.MutualAidConvoys[Right].DispatchSequence;
			});

		TArray<int64> ChannelAvailableSeconds;
		ChannelAvailableSeconds.Init(0, ExpectedActiveCount);
		int64 ExpectedTailArrivalSeconds = 0;
		for (int32 JobIndex = 0; JobIndex < OrderedIndices.Num(); ++JobIndex)
		{
			const FMutualAidConvoyState& Convoy =
				Campaign.MutualAidConvoys[OrderedIndices[JobIndex]];
			const FMutualAidRelayQueueView* View = Snapshot.FindConvoy(Convoy.ConvoyId);
			const int64 JourneySeconds = FMutualAidRelayQueue::ProjectedJourneySeconds(Convoy);
			const bool bAvailable = ExpectedActiveCount > 0;
			int32 ExpectedChannelIndex = INDEX_NONE;
			int64 ExpectedWaitSeconds = 0;
			int64 ExpectedArrivalSeconds = 0;
			if (bAvailable)
			{
				ExpectedChannelIndex = 0;
				for (int32 Candidate = 1; Candidate < ChannelAvailableSeconds.Num(); ++Candidate)
				{
					if (ChannelAvailableSeconds[Candidate] < ChannelAvailableSeconds[ExpectedChannelIndex])
					{
						ExpectedChannelIndex = Candidate;
					}
				}
				ExpectedWaitSeconds = ChannelAvailableSeconds[ExpectedChannelIndex];
				ExpectedArrivalSeconds = ExpectedWaitSeconds + JourneySeconds;
				ChannelAvailableSeconds[ExpectedChannelIndex] = ExpectedArrivalSeconds;
				ExpectedTailArrivalSeconds = FMath::Max(
					ExpectedTailArrivalSeconds, ExpectedArrivalSeconds);
			}
			const bool bExpectedInTransit = bAvailable && ExpectedWaitSeconds == 0;
			const int32 ExpectedWaitingPosition = bAvailable
				? bExpectedInTransit ? 0 : JobIndex - ExpectedActiveCount + 1
				: JobIndex + 1;
			const bool bViewValid = View != nullptr
				&& View->QueuePosition == JobIndex + 1
				&& View->WaitingPosition == ExpectedWaitingPosition
				&& View->WaitingConvoyCount == ExpectedWaitingCount
				&& View->QueuePressurePercent == ExpectedPressure
				&& View->bRelayAvailable == bAvailable
				&& View->bInTransit == bExpectedInTransit
				&& View->RelayChannelNumber == (bAvailable ? ExpectedChannelIndex + 1 : 0)
				&& View->EstimatedWaitSeconds == ExpectedWaitSeconds
				&& View->EstimatedArrivalSeconds == (bAvailable ? ExpectedArrivalSeconds : 0);
			bScenarioValid &= bViewValid;
			if (!bViewValid && FirstFailureKind == 0)
			{
				FirstFailureKind = 1;
			}
		}
		const bool bBaseValid = BaseView != nullptr
			&& BaseView->RelayChannelCount == RelayChannelCount
			&& BaseView->ActiveConvoyCount == ExpectedActiveCount
			&& BaseView->TotalConvoyCount == ConvoyCount
			&& BaseView->WaitingConvoyCount == ExpectedWaitingCount
			&& BaseView->QueuePressurePercent == ExpectedPressure
			&& BaseView->QueueTailArrivalSeconds == ExpectedTailArrivalSeconds;
		bScenarioValid &= bBaseValid;
		if (!bBaseValid && FirstFailureKind == 0)
		{
			FirstFailureKind = 2;
		}

		const FMutualAidRelayQueueView Prospective = FMutualAidRelayQueue::ProjectNext(
			Campaign, Rules, Source.BaseId, static_cast<int64>(24 + Seed % 72) * 3600);
		const int32 ExpectedNextTotal = ConvoyCount + 1;
		const int32 ExpectedNextActiveCount = FMath::Min(
			RelayChannelCount, ExpectedNextTotal);
		TArray<int64> ExpectedNextChannelAvailableSeconds;
		ExpectedNextChannelAvailableSeconds.Init(0, ExpectedNextActiveCount);
		for (const int32 OrderedIndex : OrderedIndices)
		{
			if (ExpectedNextActiveCount <= 0)
			{
				break;
			}
			int32 ChannelIndex = 0;
			for (int32 Candidate = 1;
				Candidate < ExpectedNextChannelAvailableSeconds.Num(); ++Candidate)
			{
				if (ExpectedNextChannelAvailableSeconds[Candidate]
					< ExpectedNextChannelAvailableSeconds[ChannelIndex])
				{
					ChannelIndex = Candidate;
				}
			}
			ExpectedNextChannelAvailableSeconds[ChannelIndex] =
				ExpectedNextChannelAvailableSeconds[ChannelIndex]
				+ FMutualAidRelayQueue::ProjectedJourneySeconds(
					Campaign.MutualAidConvoys[OrderedIndex]);
		}
		int32 ExpectedNextChannelIndex = INDEX_NONE;
		int64 ExpectedNextWaitSeconds = 0;
		int64 ExpectedNextArrivalSeconds = 0;
		if (ExpectedNextActiveCount > 0)
		{
			ExpectedNextChannelIndex = 0;
			for (int32 Candidate = 1;
				Candidate < ExpectedNextChannelAvailableSeconds.Num(); ++Candidate)
			{
				if (ExpectedNextChannelAvailableSeconds[Candidate]
					< ExpectedNextChannelAvailableSeconds[ExpectedNextChannelIndex])
				{
					ExpectedNextChannelIndex = Candidate;
				}
			}
			ExpectedNextWaitSeconds = ExpectedNextChannelAvailableSeconds[ExpectedNextChannelIndex];
			ExpectedNextArrivalSeconds = ExpectedNextWaitSeconds
				+ static_cast<int64>(24 + Seed % 72) * 3600;
		}
		const int32 ExpectedNextWaiting = FMath::Max(
			0, ExpectedNextTotal - ExpectedNextActiveCount);
		const int32 ExpectedNextPressure = FMath::Clamp<int32>(static_cast<int32>(
			(static_cast<int64>(ExpectedNextWaiting) * 100 + ExpectedNextTotal - 1)
			/ ExpectedNextTotal), 0, 100);
		const bool bNextAvailable = ExpectedNextActiveCount > 0;
		const bool bProspectiveValid = Prospective.bValid
			&& Prospective.QueuePosition == ExpectedNextTotal
			&& Prospective.WaitingPosition == (bNextAvailable
				&& ExpectedNextTotal <= ExpectedNextActiveCount
				? 0 : bNextAvailable
					? ExpectedNextTotal - ExpectedNextActiveCount : ExpectedNextTotal)
			&& Prospective.WaitingConvoyCount == ExpectedNextWaiting
			&& Prospective.QueuePressurePercent == ExpectedNextPressure
			&& Prospective.bRelayAvailable == bNextAvailable
			&& Prospective.bInTransit == (bNextAvailable && ExpectedNextWaitSeconds == 0)
			&& Prospective.RelayChannelNumber == (bNextAvailable ? ExpectedNextChannelIndex + 1 : 0)
			&& Prospective.EstimatedWaitSeconds == ExpectedNextWaitSeconds
			&& Prospective.EstimatedArrivalSeconds == (bNextAvailable ? ExpectedNextArrivalSeconds : 0);
		bScenarioValid &= bProspectiveValid;
		if (!bProspectiveValid && FirstFailureKind == 0)
		{
			FirstFailureKind = 3;
		}

		FCampaignState ReorderedCampaign = Campaign;
		Algo::Reverse(ReorderedCampaign.MutualAidConvoys);
		const FMutualAidRelayQueueSnapshot Reordered = FMutualAidRelayQueue::Evaluate(
			ReorderedCampaign, Rules);
		for (const FMutualAidConvoyState& Convoy : Campaign.MutualAidConvoys)
		{
			const FMutualAidRelayQueueView* Before = Snapshot.FindConvoy(Convoy.ConvoyId);
			const FMutualAidRelayQueueView* After = Reordered.FindConvoy(Convoy.ConvoyId);
			const bool bReorderedValid = Before != nullptr && After != nullptr
				&& Before->QueuePosition == After->QueuePosition
				&& Before->RelayChannelNumber == After->RelayChannelNumber
				&& Before->EstimatedWaitSeconds == After->EstimatedWaitSeconds
				&& Before->EstimatedArrivalSeconds == After->EstimatedArrivalSeconds;
			bScenarioValid &= bReorderedValid;
			if (!bReorderedValid && FirstFailureKind == 0)
			{
				FirstFailureKind = 4;
			}
		}
		const bool bRandomStateValid = Campaign.SimulationRandom.DrawCount == InitialSimulationDraws
			&& Campaign.SimulationRandom.GetStateForSave() == InitialSimulationState;
		bScenarioValid &= bRandomStateValid;
		if (!bRandomStateValid && FirstFailureKind == 0)
		{
			FirstFailureKind = 5;
		}
		if (!bScenarioValid && !bReportedFailure)
		{
			AddError(FString::Printf(
				TEXT("Seed %d violated Relay Weave invariant group %d."), Seed, FirstFailureKind));
			bReportedFailure = true;
		}
		bCorpusValid &= bScenarioValid;
	}

	TestTrue(TEXT("1024 seeded long-horizon Relay Weave schedules preserve exact lanes, pressure, and prospective readiness"),
		bCorpusValid
		&& ScenariosWithPressure > 0
		&& ScenariosWithoutCapacity > 0
		&& ScenariosWithMultipleChannels > 0
		&& MaximumPressure == 100
		&& TotalWaitingConvoys > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMutualAidSignalWatchEvaluationTest,
	"UEGT.Core.Strategic.MutualAidRelayQueue.SignalWatchSurge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMutualAidSignalWatchEvaluationTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FFacilityRule RelayArray;
	RelayArray.Identity.RuleId = TEXT("facility.signal-watch-array");
	RelayArray.DisplayName = TEXT("Signal Watch Array");
	RelayArray.DetectionStrength = 100;
	RelayArray.MaxIntegrity = 100;
	Rules.Facilities.Add(RelayArray.Identity.RuleId, RelayArray);

	FCampaignState Campaign;
	Campaign.CommandSequence = 9;
	FStrategicBaseState& Source = Campaign.Bases.AddDefaulted_GetRef();
	Source.BaseId = FGuid(20, 21, 22, 23);
	Source.Name = TEXT("Watch Source");
	Source.SignalWatchScientists = 2;
	FBaseFacilityState& Array = Source.Facilities.AddDefaulted_GetRef();
	Array.InstanceId = FGuid(24, 25, 26, 27);
	Array.FacilityId = RelayArray.Identity.RuleId;

	TestTrue(TEXT("Signal Watch layers staffed surge on the Signal Relay specialization"),
		FMutualAidRelayQueue::SignalWatchPolicyId() == FName(TEXT("logistics.signal-watch"))
		&& FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Source, Rules) == 2
		&& FMutualAidRelayQueue::EvaluateRelayChannelCount(Source, Rules) == 5);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		FMutualAidConvoyState& Convoy = Campaign.MutualAidConvoys.AddDefaulted_GetRef();
		Convoy.ConvoyId = FGuid(30 + Index, 31, 32, 33);
		Convoy.SourceBaseId = Source.BaseId;
		Convoy.DestinationBaseId = FGuid(40, 41, 42, 43);
		Convoy.ItemId = TEXT("item.relief-kit");
		Convoy.Quantity = 1;
		Convoy.DispatchSequence = Index + 1;
		Convoy.TotalTransitSeconds = int64(Index + 1) * 3600;
		Convoy.RemainingTransitSeconds = Convoy.TotalTransitSeconds;
		Convoy.bInterdictionResolved = true;
	}

	const FMutualAidRelayQueueSnapshot Intact = FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	TestTrue(TEXT("Queue telemetry distinguishes facility channels, assigned scientists, and staffed surge"),
		Intact.Convoys.Num() == 4
		&& Intact.Convoys.ContainsByPredicate(
			[](const FMutualAidRelayQueueView& View)
			{
				return View.FacilityRelayChannelCount == 2
					&& View.SpecializationRelayChannelBonus == 1
					&& View.SignalWatchScientistCount == 2
					&& View.SignalWatchBonusChannelCount == 2
					&& View.RelayChannelCount == 5
					&& View.ActiveConvoyCount == 4 && View.WaitingConvoyCount == 0
					&& View.QueuePressurePercent == 0 && View.bInTransit;
			}));

	Array.Damage = 50;
	const FMutualAidRelayQueueSnapshot Damaged = FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	TestTrue(TEXT("Damage suppresses effective surge without deleting the authored staffing commitment"),
		FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Source, Rules) == 1
		&& FMutualAidRelayQueue::EvaluateRelayChannelCount(Source, Rules) == 3
		&& Damaged.Convoys.Num() == 4
		&& Damaged.Convoys.ContainsByPredicate(
			[](const FMutualAidRelayQueueView& View)
			{
				return View.FacilityRelayChannelCount == 1
					&& View.SpecializationRelayChannelBonus == 1
					&& View.SignalWatchScientistCount == 2
					&& View.SignalWatchBonusChannelCount == 1
					&& View.RelayChannelCount == 3
					&& View.ActiveConvoyCount == 3 && View.WaitingConvoyCount == 1
					&& View.QueuePressurePercent == 25 && !View.bInTransit;
			}));

	Array.Damage = RelayArray.MaxIntegrity;
	TestTrue(TEXT("A total outage suppresses both facility and staffed channels while retaining assignment"),
		Source.SignalWatchScientists == 2
		&& FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Source, Rules) == 0
		&& FMutualAidRelayQueue::EvaluateRelayChannelCount(Source, Rules) == 0);
	return true;
}

#endif
