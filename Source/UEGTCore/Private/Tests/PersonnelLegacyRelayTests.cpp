// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/PersonnelLegacyRelay.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelLegacyRelayEvaluationTest,
	"UEGT.Core.Strategic.Personnel.LegacyRelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelLegacyRelayEvaluationTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FPersonnelDoctrineRule ClearSight;
	ClearSight.Identity.RuleId = TEXT("doctrine.clear-sight");
	ClearSight.DisplayName = TEXT("Clear Sight");
	ClearSight.MaxSelections = 3;
	ClearSight.AccuracyBonus = 4;
	Rules.PersonnelDoctrines.Add(ClearSight.Identity.RuleId, ClearSight);
	FPersonnelDoctrineRule Steadfast;
	Steadfast.Identity.RuleId = TEXT("doctrine.steadfast");
	Steadfast.DisplayName = TEXT("Steadfast");
	Steadfast.MaxSelections = 3;
	Steadfast.MaxHealthBonus = 5;
	Steadfast.ResolveBonus = 5;
	Rules.PersonnelDoctrines.Add(Steadfast.Identity.RuleId, Steadfast);
	FPersonnelDoctrineRule Anchor;
	Anchor.Identity.RuleId = TEXT("doctrine.anchor");
	Anchor.DisplayName = TEXT("Anchor");
	Anchor.MaxSelections = 3;
	Anchor.MaxHealthBonus = 3;
	Anchor.StrengthBonus = 4;
	Rules.PersonnelDoctrines.Add(Anchor.Identity.RuleId, Anchor);

	auto AddPerson = [](FCampaignState& Campaign, const FGuid PersonnelId,
		const TCHAR* DisplayName, const int32 Missions, const TArray<FName>& Doctrines)
	{
		FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
		Person.PersonnelId = PersonnelId;
		Person.DisplayName = DisplayName;
		Person.Missions = Missions;
		Person.DoctrineSelections = Doctrines;
		return &Person;
	};

	FCampaignState Campaign;
	const FGuid PreLegacyId(1, 1, 1, 1);
	const FGuid IncompleteLegacyId(2, 1, 1, 1);
	AddPerson(Campaign, PreLegacyId, TEXT("Nineteen"), 19,
		{ ClearSight.Identity.RuleId, ClearSight.Identity.RuleId, ClearSight.Identity.RuleId });
	AddPerson(Campaign, IncompleteLegacyId, TEXT("Incomplete"), 20,
		{ ClearSight.Identity.RuleId, ClearSight.Identity.RuleId });
	const FPersonnelLegacyRelayView NoRelay = FPersonnelLegacyRelay::Evaluate(
		Campaign, Rules, { FGuid(), PreLegacyId, IncompleteLegacyId, PreLegacyId });
	TestTrue(TEXT("Legacy Relay requires both the terminal service band and a maxed doctrine"),
		NoRelay.PolicyId == FName(TEXT("personnel.specialization-legacy-relay"))
		&& !NoRelay.bHasSpecialist
		&& !NoRelay.bActive
		&& !NoRelay.SpecialistId.IsValid()
		&& NoRelay.DoctrineId.IsNone()
		&& NoRelay.RecipientIds.IsEmpty());

	const FGuid RecipientLowId(10, 1, 1, 1);
	const FGuid RecipientHighId(20, 1, 1, 1);
	const FGuid TieWinnerId(40, 1, 1, 1);
	const FGuid TieLoserId(50, 1, 1, 1);
	AddPerson(Campaign, RecipientHighId, TEXT("Recipient high"), 8, {});
	AddPerson(Campaign, RecipientLowId, TEXT("Recipient low"), 3, {});
	AddPerson(Campaign, TieWinnerId, TEXT("Mara Sol"), 25,
		{
			ClearSight.Identity.RuleId, ClearSight.Identity.RuleId, ClearSight.Identity.RuleId,
			Steadfast.Identity.RuleId, Steadfast.Identity.RuleId, Steadfast.Identity.RuleId
		});
	AddPerson(Campaign, TieLoserId, TEXT("Iris Quill"), 25,
		{ Anchor.Identity.RuleId, Anchor.Identity.RuleId, Anchor.Identity.RuleId });
	TArray<FGuid> Roster = {
		TieLoserId,
		RecipientHighId,
		FGuid(999, 1, 1, 1),
		TieWinnerId,
		RecipientLowId,
		RecipientHighId
	};
	const FPersonnelLegacyRelayView Relay = FPersonnelLegacyRelay::Evaluate(Campaign, Rules, Roster);
	TestTrue(TEXT("The stable specialist and strongest maxed doctrine project half authored bonuses"),
		Relay.bHasSpecialist
		&& Relay.bActive
		&& Relay.SpecialistId == TieWinnerId
		&& Relay.SpecialistDisplayName == TEXT("Mara Sol")
		&& Relay.SpecialistServiceHistory.Band == EPersonnelServiceBand::LegacyAnchor
		&& Relay.DoctrineId == Steadfast.Identity.RuleId
		&& Relay.DoctrineDisplayName == Steadfast.DisplayName
		&& Relay.AccuracyBonus == 0
		&& Relay.ResolveBonus == 3
		&& Relay.MobilityBonus == 0
		&& Relay.StrengthBonus == 0
		&& Relay.RecipientCount == 3
		&& Relay.RecipientIds == TArray<FGuid>{ RecipientLowId, RecipientHighId, TieLoserId });

	Algo::Reverse(Roster);
	const FPersonnelLegacyRelayView Reordered = FPersonnelLegacyRelay::Evaluate(Campaign, Rules, Roster);
	TestTrue(TEXT("Input order, duplicate ids, and unresolved ids cannot change the relay"),
		Reordered.SpecialistId == Relay.SpecialistId
		&& Reordered.DoctrineId == Relay.DoctrineId
		&& Reordered.ResolveBonus == Relay.ResolveBonus
		&& Reordered.RecipientIds == Relay.RecipientIds);

	const FGuid HigherMissionId(60, 1, 1, 1);
	AddPerson(Campaign, HigherMissionId, TEXT("Tao Neris"), 26,
		{ Anchor.Identity.RuleId, Anchor.Identity.RuleId, Anchor.Identity.RuleId });
	const FPersonnelLegacyRelayView HigherMission = FPersonnelLegacyRelay::Evaluate(
		Campaign, Rules, { TieWinnerId, RecipientLowId, HigherMissionId });
	TestTrue(TEXT("Mission count precedes lexical identity and doctrine strength for specialist selection"),
		HigherMission.SpecialistId == HigherMissionId
		&& HigherMission.DoctrineId == Anchor.Identity.RuleId
		&& HigherMission.StrengthBonus == 2
		&& HigherMission.RecipientIds == TArray<FGuid>{ RecipientLowId, TieWinnerId });

	const FGuid DoctrineTieId(70, 1, 1, 1);
	AddPerson(Campaign, DoctrineTieId, TEXT("Edda Vale"), 27,
		{
			ClearSight.Identity.RuleId, ClearSight.Identity.RuleId, ClearSight.Identity.RuleId,
			Anchor.Identity.RuleId, Anchor.Identity.RuleId, Anchor.Identity.RuleId
		});
	const FPersonnelLegacyRelayView DoctrineTie = FPersonnelLegacyRelay::Evaluate(
		Campaign, Rules, { DoctrineTieId, RecipientLowId });
	TestTrue(TEXT("Lexical doctrine identity resolves an equal projected non-health bonus"),
		DoctrineTie.SpecialistId == DoctrineTieId
		&& DoctrineTie.DoctrineId == Anchor.Identity.RuleId
		&& DoctrineTie.StrengthBonus == 2
		&& DoctrineTie.AccuracyBonus == 0);

	const FPersonnelLegacyRelayView Dormant = FPersonnelLegacyRelay::Evaluate(
		Campaign, Rules, { HigherMissionId, HigherMissionId });
	TestTrue(TEXT("A qualified solo specialist remains visible as a dormant relay"),
		Dormant.bHasSpecialist
		&& !Dormant.bActive
		&& Dormant.SpecialistId == HigherMissionId
		&& Dormant.StrengthBonus == 2
		&& Dormant.RecipientCount == 0
		&& Dormant.RecipientIds.IsEmpty());
	return true;
}

#endif
