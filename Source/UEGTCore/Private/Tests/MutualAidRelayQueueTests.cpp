// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/MutualAidRelayQueue.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

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
	TestTrue(TEXT("Integrity-scaled authored signals supply three active Relay Weave channels"),
		ThreeChannels.PolicyId == FName(TEXT("logistics.mutual-aid-relay-weave"))
		&& First != nullptr && Second != nullptr && Third != nullptr
		&& First->RelayChannelCount == 3 && First->bInTransit
		&& First->QueuePosition == 1 && First->RelayChannelNumber == 1
		&& Second->bInTransit && Second->QueuePosition == 2
		&& Second->RelayChannelNumber == 2
		&& Third->bInTransit && Third->QueuePosition == 3
		&& Third->RelayChannelNumber == 3);

	const FMutualAidRelayQueueView Prospective = FMutualAidRelayQueue::ProjectNext(
		Campaign, Rules, Source.BaseId, 36 * 3600);
	TestTrue(TEXT("The next FIFO convoy chooses the earliest available channel exactly"),
		Prospective.bValid && Prospective.bRelayAvailable && !Prospective.bInTransit
		&& Prospective.QueuePosition == 4 && Prospective.WaitingPosition == 1
		&& Prospective.RelayChannelNumber == 2
		&& Prospective.EstimatedWaitSeconds == 24 * 3600
		&& Prospective.EstimatedArrivalSeconds == 60 * 3600);

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
	TestTrue(TEXT("Progressive array damage reduces two authored channels to one"),
		Degraded.RelayChannelCount == 2 && !Degraded.bInTransit
		&& Degraded.WaitingPosition == 2
		&& Degraded.EstimatedWaitSeconds == 72 * 3600
		&& Degraded.EstimatedArrivalSeconds == 108 * 3600);

	Hub.Damage = Operations.MaxIntegrity;
	Array.Damage = LongRangeArray.MaxIntegrity;
	const FMutualAidRelayQueueSnapshot Offline =
		FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	const FMutualAidRelayQueueView OfflineProspective = FMutualAidRelayQueue::ProjectNext(
		Campaign, Rules, Source.BaseId, 36 * 3600);
	TestTrue(TEXT("A total signal outage holds every existing convoy without inventing an ETA"),
		Offline.Convoys.Num() == 3
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
	TestEqual(TEXT("Legacy facility lists retain full-strength relay capacity"),
		FMutualAidRelayQueue::EvaluateRelayChannelCount(Legacy.Bases[0], Rules), 3);

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

	TestTrue(TEXT("Signal Watch uses the original policy identity and doubles the intact facility baseline"),
		FMutualAidRelayQueue::SignalWatchPolicyId() == FName(TEXT("logistics.signal-watch"))
		&& FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Source, Rules) == 2
		&& FMutualAidRelayQueue::EvaluateRelayChannelCount(Source, Rules) == 4);

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
					&& View.SignalWatchScientistCount == 2
					&& View.SignalWatchBonusChannelCount == 2
					&& View.RelayChannelCount == 4
					&& View.ActiveConvoyCount == 4 && View.bInTransit;
			}));

	Array.Damage = 50;
	const FMutualAidRelayQueueSnapshot Damaged = FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	TestTrue(TEXT("Damage suppresses effective surge without deleting the authored staffing commitment"),
		FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Source, Rules) == 1
		&& FMutualAidRelayQueue::EvaluateRelayChannelCount(Source, Rules) == 2
		&& Damaged.Convoys.Num() == 4
		&& Damaged.Convoys.ContainsByPredicate(
			[](const FMutualAidRelayQueueView& View)
			{
				return View.FacilityRelayChannelCount == 1
					&& View.SignalWatchScientistCount == 2
					&& View.SignalWatchBonusChannelCount == 1
					&& View.RelayChannelCount == 2
					&& View.ActiveConvoyCount == 2 && !View.bInTransit;
			}));

	Array.Damage = RelayArray.MaxIntegrity;
	TestTrue(TEXT("A total outage suppresses both facility and staffed channels while retaining assignment"),
		Source.SignalWatchScientists == 2
		&& FMutualAidRelayQueue::EvaluateFacilityRelayChannelCount(Source, Rules) == 0
		&& FMutualAidRelayQueue::EvaluateRelayChannelCount(Source, Rules) == 0);
	return true;
}

#endif
