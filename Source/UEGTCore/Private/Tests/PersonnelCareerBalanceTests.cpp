// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Determinism/DeterministicRandomStream.h"
#include "Strategic/PersonnelLegacyRelay.h"
#include "Strategic/PersonnelMentorship.h"
#include "Strategic/PersonnelServiceHistory.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelCareerBalanceCorpusTest,
	"UEGT.Core.Strategic.Personnel.SeededCareerBalanceCorpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelCareerBalanceCorpusTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FPersonnelDoctrineRule ClearSight;
	ClearSight.Identity.RuleId = TEXT("doctrine.clear-sight");
	ClearSight.DisplayName = TEXT("Clear Sight");
	ClearSight.MaxSelections = 3;
	ClearSight.AccuracyBonus = 4;
	Rules.PersonnelDoctrines.Add(ClearSight.Identity.RuleId, ClearSight);
	FPersonnelDoctrineRule Resolve;
	Resolve.Identity.RuleId = TEXT("doctrine.resolve");
	Resolve.DisplayName = TEXT("Resolve");
	Resolve.MaxSelections = 3;
	Resolve.ResolveBonus = 5;
	Rules.PersonnelDoctrines.Add(Resolve.Identity.RuleId, Resolve);
	FPersonnelDoctrineRule Anchor;
	Anchor.Identity.RuleId = TEXT("doctrine.anchor");
	Anchor.DisplayName = TEXT("Anchor");
	Anchor.MaxSelections = 3;
	Anchor.StrengthBonus = 6;
	Rules.PersonnelDoctrines.Add(Anchor.Identity.RuleId, Anchor);
	const FName DoctrineIds[] = {
		ClearSight.Identity.RuleId,
		Resolve.Identity.RuleId,
		Anchor.Identity.RuleId
	};
	const int32 FixedMissions[] = { 0, 4, 5, 9, 10, 19, 20, 39, 40 };

	auto GuidLess = [](const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A)
		{
			return Left.A < Right.A;
		}
		if (Left.B != Right.B)
		{
			return Left.B < Right.B;
		}
		if (Left.C != Right.C)
		{
			return Left.C < Right.C;
		}
		return Left.D < Right.D;
	};
	auto FindPerson = [](const FCampaignState& Campaign, const FGuid& PersonnelId)
		-> const FPersonnelState*
	{
		return Campaign.Personnel.FindByPredicate(
			[&PersonnelId](const FPersonnelState& Person)
			{
				return Person.PersonnelId == PersonnelId;
			});
	};
	auto MaxedDoctrine = [&DoctrineIds](const FPersonnelState& Person) -> FName
	{
		for (const FName DoctrineId : DoctrineIds)
		{
			int32 SelectionCount = 0;
			for (const FName Selection : Person.DoctrineSelections)
			{
				SelectionCount += Selection == DoctrineId ? 1 : 0;
			}
			if (SelectionCount >= 3)
			{
				return DoctrineId;
			}
		}
		return NAME_None;
	};
	auto ExpectedSharedBonus = [&Rules](const FName DoctrineId)
	{
		const FPersonnelDoctrineRule* Rule = Rules.PersonnelDoctrines.Find(DoctrineId);
		if (Rule == nullptr)
		{
			return TArray<int32>{ 0, 0, 0, 0 };
		}
		return TArray<int32>{
			FMath::DivideAndRoundUp(FMath::Max(0, Rule->AccuracyBonus), 2),
			FMath::DivideAndRoundUp(FMath::Max(0, Rule->ResolveBonus), 2),
			FMath::DivideAndRoundUp(FMath::Max(0, Rule->MobilityBonus), 2),
			FMath::DivideAndRoundUp(FMath::Max(0, Rule->StrengthBonus), 2)
		};
	};

	bool bAllSeedsValid = true;
	bool bReportedFailure = false;
	for (int64 Seed = 1; Seed <= 256; ++Seed)
	{
		FDeterministicRandomStream Random(0xCAFE0000LL + Seed);
		FCampaignState Campaign;
		TArray<FGuid> PersonnelIds;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			const FGuid PersonnelId(
				0xCA000000u + static_cast<uint32>(Seed),
				0x00010000u + static_cast<uint32>(Index),
				0xC0DE0000u + static_cast<uint32>(Seed),
				0xBEEF0000u + static_cast<uint32>(Index));
			FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
			Person.PersonnelId = PersonnelId;
			Person.DisplayName = FString::Printf(TEXT("Seed %lld Person %d"), Seed, Index);
			Person.Missions = Index < UE_ARRAY_COUNT(FixedMissions)
				? FixedMissions[Index]
				: Random.NextIntInclusive(0, 80);
			if (Index >= 7 || (Index == 6 && Seed % 4 != 0))
			{
				const FName DoctrineId = DoctrineIds[
					(Index + static_cast<int32>(Seed)) % UE_ARRAY_COUNT(DoctrineIds)];
				Person.DoctrineSelections = { DoctrineId, DoctrineId, DoctrineId };
			}
			PersonnelIds.Add(PersonnelId);
		}

		TArray<FGuid> Roster = PersonnelIds;
		Roster.Add(PersonnelIds[Seed % PersonnelIds.Num()]);
		Roster.Add(FGuid(
			0xDEAD0000u + static_cast<uint32>(Seed),
			0x00020000u + static_cast<uint32>(Seed),
			0xFACE0000u + static_cast<uint32>(Seed),
			0x00030000u + static_cast<uint32>(Seed)));
		TArray<FGuid> ReversedRoster;
		for (int32 Index = Roster.Num() - 1; Index >= 0; --Index)
		{
			ReversedRoster.Add(Roster[Index]);
		}

		const FPersonnelMentorshipView Mentorship =
			FPersonnelMentorship::Evaluate(Campaign, Roster);
		const FPersonnelMentorshipView ReorderedMentorship =
			FPersonnelMentorship::Evaluate(Campaign, ReversedRoster);
		const FPersonnelLegacyRelayView Relay =
			FPersonnelLegacyRelay::Evaluate(Campaign, Rules, Roster);
		const FPersonnelLegacyRelayView ReorderedRelay =
			FPersonnelLegacyRelay::Evaluate(Campaign, Rules, ReversedRoster);

		bool bSeedValid = Mentorship.MentorId == ReorderedMentorship.MentorId
			&& Mentorship.MoraleBonus == ReorderedMentorship.MoraleBonus
			&& Mentorship.RecipientIds == ReorderedMentorship.RecipientIds
			&& Mentorship.bActive == (Mentorship.RecipientCount > 0)
			&& Relay.SpecialistId == ReorderedRelay.SpecialistId
			&& Relay.DoctrineId == ReorderedRelay.DoctrineId
			&& Relay.AccuracyBonus == ReorderedRelay.AccuracyBonus
			&& Relay.ResolveBonus == ReorderedRelay.ResolveBonus
			&& Relay.MobilityBonus == ReorderedRelay.MobilityBonus
			&& Relay.StrengthBonus == ReorderedRelay.StrengthBonus
			&& Relay.RecipientIds == ReorderedRelay.RecipientIds
			&& Relay.bActive == (Relay.RecipientCount > 0);

		const FPersonnelState* Mentor = FindPerson(Campaign, Mentorship.MentorId);
		if (Mentorship.bHasMentor)
		{
			const FPersonnelServiceHistoryView MentorHistory =
				Mentor == nullptr
					? FPersonnelServiceHistoryView{}
					: FPersonnelServiceHistory::Project(Mentor->Missions);
			const int32 MentorBand = static_cast<int32>(MentorHistory.Band);
			const int32 ExpectedMoraleBonus = MentorBand
				== static_cast<int32>(EPersonnelServiceBand::EnduringBeacon)
				? 15
				: (MentorBand == static_cast<int32>(EPersonnelServiceBand::LegacyAnchor) ? 10 : 5);
			bSeedValid &= Mentor != nullptr
				&& MentorBand >= static_cast<int32>(EPersonnelServiceBand::LongWatch)
				&& Mentorship.MoraleBonus == ExpectedMoraleBonus
				&& Mentorship.MentorServiceHistory.Band == MentorHistory.Band;
		}
		else
		{
			bSeedValid &= !Mentorship.bActive
				&& Mentorship.MoraleBonus == 0
				&& Mentorship.RecipientIds.IsEmpty();
		}

		TSet<FGuid> MentorshipRecipientIds;
		for (int32 Index = 0; Index < Mentorship.RecipientIds.Num(); ++Index)
		{
			const FGuid RecipientId = Mentorship.RecipientIds[Index];
			const FPersonnelState* Recipient = FindPerson(Campaign, RecipientId);
			const FPersonnelServiceHistoryView RecipientHistory = Recipient == nullptr
				? FPersonnelServiceHistoryView{}
				: FPersonnelServiceHistory::Project(Recipient->Missions);
			bSeedValid &= Recipient != nullptr
				&& RecipientId != Mentorship.MentorId
				&& Mentor != nullptr
				&& static_cast<int32>(RecipientHistory.Band)
					< static_cast<int32>(Mentorship.MentorServiceHistory.Band)
				&& !MentorshipRecipientIds.Contains(RecipientId);
			if (Index > 0)
			{
				bSeedValid &= GuidLess(Mentorship.RecipientIds[Index - 1], RecipientId);
			}
			MentorshipRecipientIds.Add(RecipientId);
		}
		bSeedValid &= Mentorship.RecipientCount == MentorshipRecipientIds.Num();

		const FPersonnelState* ExpectedSpecialist = nullptr;
		for (const FPersonnelState& Person : Campaign.Personnel)
		{
			const FPersonnelServiceHistoryView History =
				FPersonnelServiceHistory::Project(Person.Missions);
			if (static_cast<int32>(History.Band)
				< static_cast<int32>(EPersonnelServiceBand::LegacyAnchor)
				|| MaxedDoctrine(Person).IsNone())
			{
				continue;
			}
			if (ExpectedSpecialist == nullptr
				|| Person.Missions > ExpectedSpecialist->Missions
				|| (Person.Missions == ExpectedSpecialist->Missions
					&& GuidLess(Person.PersonnelId, ExpectedSpecialist->PersonnelId)))
			{
				ExpectedSpecialist = &Person;
			}
		}
		if (ExpectedSpecialist == nullptr)
		{
			bSeedValid &= !Relay.bHasSpecialist
				&& Relay.RecipientIds.IsEmpty();
		}
		else
		{
			const FName ExpectedDoctrine = MaxedDoctrine(*ExpectedSpecialist);
			const TArray<int32> ExpectedBonus = ExpectedSharedBonus(ExpectedDoctrine);
			bSeedValid &= Relay.bHasSpecialist
				&& Relay.SpecialistId == ExpectedSpecialist->PersonnelId
				&& Relay.SpecialistServiceHistory.Band
					== FPersonnelServiceHistory::Project(ExpectedSpecialist->Missions).Band
				&& Relay.DoctrineId == ExpectedDoctrine
				&& Relay.AccuracyBonus == ExpectedBonus[0]
				&& Relay.ResolveBonus == ExpectedBonus[1]
				&& Relay.MobilityBonus == ExpectedBonus[2]
				&& Relay.StrengthBonus == ExpectedBonus[3]
				&& Relay.RecipientCount == Campaign.Personnel.Num() - 1;
		}

		TSet<FGuid> RelayRecipientIds;
		for (const FGuid& RecipientId : Relay.RecipientIds)
		{
			bSeedValid &= FindPerson(Campaign, RecipientId) != nullptr
				&& RecipientId != Relay.SpecialistId
				&& !RelayRecipientIds.Contains(RecipientId);
			RelayRecipientIds.Add(RecipientId);
		}
		bSeedValid &= Relay.RecipientCount == RelayRecipientIds.Num();

		int32 CareerMissions = 0;
		int32 PreviousBand = -1;
		bool SeenBands[5] = {};
		for (int32 OperationIndex = 0; OperationIndex < 96; ++OperationIndex)
		{
			CareerMissions += Random.NextIntInclusive(1, 3);
			const FPersonnelServiceHistoryView History =
				FPersonnelServiceHistory::Project(CareerMissions);
			const int32 Band = static_cast<int32>(History.Band);
			bSeedValid &= Band >= PreviousBand && Band >= 0 && Band < 5;
			PreviousBand = Band;
			SeenBands[Band] = true;
			if (History.bMaximumBand)
			{
				bSeedValid &= History.Band == EPersonnelServiceBand::EnduringBeacon
					&& History.NextBand == EPersonnelServiceBand::EnduringBeacon
					&& History.NextBandMissions == 40
					&& History.MissionsUntilNextBand == 0;
			}
			else
			{
				bSeedValid &= History.NextBandMissions > CareerMissions
					&& History.MissionsUntilNextBand
					== History.NextBandMissions - CareerMissions;
			}
		}
		for (const bool bSeen : SeenBands)
		{
			bSeedValid &= bSeen;
		}

		if (!bSeedValid && !bReportedFailure)
		{
			AddError(FString::Printf(
				TEXT("Seed %lld violated the seeded career projection invariants."), Seed));
			bReportedFailure = true;
		}
		bAllSeedsValid &= bSeedValid;
	}

	TestTrue(TEXT("256 seeded careers preserve monotonic bands, stable mentor selection, and relay invariants"),
		bAllSeedsValid);
	return true;
}

#endif
