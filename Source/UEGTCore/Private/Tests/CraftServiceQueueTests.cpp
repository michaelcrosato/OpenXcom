// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/CraftServiceQueue.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftServiceQueueEvaluationTest,
	"UEGT.Core.Strategic.Craft.FlightDeckRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftServiceQueueEvaluationTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FFacilityRule FlightDeck;
	FlightDeck.Identity.RuleId = TEXT("facility.flight-deck");
	FlightDeck.CraftCapacity = 2;
	FlightDeck.MaxIntegrity = 100;
	Rules.Facilities.Add(FlightDeck.Identity.RuleId, FlightDeck);

	const FGuid BaseId(1, 1, 1, 1);
	const FGuid TieWinnerId(10, 1, 1, 1);
	const FGuid TieLoserId(20, 1, 1, 1);
	const FGuid LongJobId(30, 1, 1, 1);
	FCampaignState Campaign;
	FStrategicBaseState& Base = Campaign.Bases.AddDefaulted_GetRef();
	Base.BaseId = BaseId;
	FBaseFacilityState& FirstDeck = Base.Facilities.AddDefaulted_GetRef();
	FirstDeck.InstanceId = FGuid(2, 1, 1, 1);
	FirstDeck.FacilityId = FlightDeck.Identity.RuleId;

	auto AddServiceJob = [&Campaign, &BaseId](
		const FGuid CraftId, const int64 RepairSeconds, const int64 RefuelSeconds)
	{
		FCraftState& Craft = Campaign.Craft.AddDefaulted_GetRef();
		Craft.CraftId = CraftId;
		Craft.BaseId = BaseId;
		Craft.Status = ECraftStatus::Servicing;
		Craft.RemainingRepairSeconds = RepairSeconds;
		Craft.RemainingRefuelSeconds = RefuelSeconds;
	};
	AddServiceJob(LongJobId, 4 * 3600, 2 * 3600);
	AddServiceJob(TieLoserId, 3600, 0);
	AddServiceJob(TieWinnerId, 0, 3600);
	FCraftState& Grounded = Campaign.Craft.AddDefaulted_GetRef();
	Grounded.CraftId = FGuid(40, 1, 1, 1);
	Grounded.BaseId = BaseId;
	Grounded.Status = ECraftStatus::Grounded;

	const FCraftServiceQueueSnapshot OneLane = FCraftServiceQueue::Evaluate(Campaign, Rules);
	const FCraftServiceQueueView* TieWinner = OneLane.FindCraft(TieWinnerId);
	const FCraftServiceQueueView* TieLoser = OneLane.FindCraft(TieLoserId);
	const FCraftServiceQueueView* LongJob = OneLane.FindCraft(LongJobId);
	TestTrue(TEXT("Flight Operations adds a derived maintenance lane to stable shortest-job ordering"),
		OneLane.PolicyId == FName(TEXT("craft.service-rapid-turnaround"))
		&& OneLane.Craft.Num() == 3
		&& TieWinner != nullptr && TieWinner->bValid && TieWinner->bInServiceLane
		&& TieWinner->ServiceLaneCount == 2 && TieWinner->QueuePosition == 1
		&& TieWinner->ServiceLaneNumber == 1 && TieWinner->EstimatedWaitSeconds == 0
		&& TieWinner->EstimatedReadySeconds == 3600
		&& TieLoser != nullptr && TieLoser->bInServiceLane
		&& TieLoser->QueuePosition == 2 && TieLoser->WaitingPosition == 0
		&& TieLoser->ServiceLaneNumber == 2
		&& TieLoser->EstimatedWaitSeconds == 0 && TieLoser->EstimatedReadySeconds == 3600
		&& LongJob != nullptr && !LongJob->bInServiceLane
		&& LongJob->QueuePosition == 3 && LongJob->WaitingPosition == 1
		&& LongJob->EstimatedWaitSeconds == 3600 && LongJob->EstimatedReadySeconds == 18000
		&& OneLane.FindCraft(Grounded.CraftId) == nullptr);

	Algo::Reverse(Campaign.Craft);
	const FCraftServiceQueueSnapshot Reordered = FCraftServiceQueue::Evaluate(Campaign, Rules);
	const FCraftServiceQueueView* ReorderedLongJob = Reordered.FindCraft(LongJobId);
	TestTrue(TEXT("Campaign craft order does not change lane assignment or estimates"),
		ReorderedLongJob != nullptr
		&& ReorderedLongJob->QueuePosition == LongJob->QueuePosition
		&& ReorderedLongJob->ServiceLaneNumber == LongJob->ServiceLaneNumber
		&& ReorderedLongJob->EstimatedWaitSeconds == LongJob->EstimatedWaitSeconds
		&& ReorderedLongJob->EstimatedReadySeconds == LongJob->EstimatedReadySeconds);

	FBaseFacilityState& SecondDeck = Base.Facilities.AddDefaulted_GetRef();
	SecondDeck.InstanceId = FGuid(3, 1, 1, 1);
	SecondDeck.FacilityId = FlightDeck.Identity.RuleId;
	const FCraftServiceQueueSnapshot TwoLanes = FCraftServiceQueue::Evaluate(Campaign, Rules);
	const FCraftServiceQueueView* TwoLaneWinner = TwoLanes.FindCraft(TieWinnerId);
	const FCraftServiceQueueView* TwoLaneLoser = TwoLanes.FindCraft(TieLoserId);
	const FCraftServiceQueueView* TwoLaneLongJob = TwoLanes.FindCraft(LongJobId);
	TestTrue(TEXT("A second operational deck combines with the derived lane for exact earliest-lane estimates"),
		TwoLaneWinner != nullptr && TwoLaneWinner->bInServiceLane
		&& TwoLaneWinner->ServiceLaneCount == 3 && TwoLaneWinner->ServiceLaneNumber == 1
		&& TwoLaneLoser != nullptr && TwoLaneLoser->bInServiceLane
		&& TwoLaneLoser->ServiceLaneNumber == 2
		&& TwoLaneLongJob != nullptr && TwoLaneLongJob->bInServiceLane
		&& TwoLaneLongJob->WaitingPosition == 0 && TwoLaneLongJob->ServiceLaneNumber == 3
		&& TwoLaneLongJob->EstimatedWaitSeconds == 0
		&& TwoLaneLongJob->EstimatedReadySeconds == 14400);

	SecondDeck.Damage = FlightDeck.MaxIntegrity;
	const FCraftServiceQueueSnapshot DamagedDeck = FCraftServiceQueue::Evaluate(Campaign, Rules);
	TestTrue(TEXT("A fully damaged deck stops supplying a service lane"),
		DamagedDeck.FindCraft(TieWinnerId) != nullptr
		&& DamagedDeck.FindCraft(TieWinnerId)->ServiceLaneCount == 2);

	FCampaignState LegacyCampaign = Campaign;
	LegacyCampaign.Bases[0].Facilities.Reset();
	LegacyCampaign.Bases[0].BuiltFacilities = {
		FlightDeck.Identity.RuleId, FlightDeck.Identity.RuleId
	};
	const FCraftServiceQueueSnapshot Legacy = FCraftServiceQueue::Evaluate(LegacyCampaign, Rules);
	TestTrue(TEXT("Legacy built-facility saves retain physical lanes plus the derived Flight Operations lane"),
		Legacy.FindCraft(TieWinnerId) != nullptr
		&& Legacy.FindCraft(TieWinnerId)->ServiceLaneCount == 3);
	return true;
}

#endif
