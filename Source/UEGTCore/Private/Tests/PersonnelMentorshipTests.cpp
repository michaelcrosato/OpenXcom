// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/PersonnelMentorship.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelMentorshipEvaluationTest,
	"UEGT.Core.Strategic.Personnel.WatchkeeperGuidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelMentorshipEvaluationTest::RunTest(const FString& Parameters)
{
	auto AddPerson = [](FCampaignState& Campaign, const FGuid PersonnelId, const TCHAR* DisplayName, const int32 Missions)
	{
		FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
		Person.PersonnelId = PersonnelId;
		Person.DisplayName = DisplayName;
		Person.Missions = Missions;
		return &Person;
	};

	FCampaignState Campaign;
	const FGuid PreVeteranId(90, 1, 1, 1);
	AddPerson(Campaign, PreVeteranId, TEXT("Pre-veteran"), 9);
	const FPersonnelMentorshipView NoMentor = FPersonnelMentorship::Evaluate(
		Campaign,
		{ FGuid(), FGuid(999, 1, 1, 1), PreVeteranId, PreVeteranId });
	TestTrue(TEXT("Teams below Long Watch expose no mentor or recipients"),
		NoMentor.PolicyId == FName(TEXT("personnel.mentorship-watchkeeper"))
		&& !NoMentor.bHasMentor
		&& !NoMentor.bActive
		&& !NoMentor.MentorId.IsValid()
		&& NoMentor.MoraleBonus == 0
		&& NoMentor.RecipientIds.IsEmpty());

	const FGuid FirstWatchId(2, 1, 1, 1);
	const FGuid FieldProvenId(3, 1, 1, 1);
	const FGuid LongWatchTieWinnerId(10, 1, 1, 1);
	const FGuid LongWatchTieLoserId(20, 1, 1, 1);
	const FGuid LowerMissionLongWatchId(30, 1, 1, 1);
	AddPerson(Campaign, FirstWatchId, TEXT("Negative history"), -4);
	AddPerson(Campaign, FieldProvenId, TEXT("Field proven"), 7);
	AddPerson(Campaign, LongWatchTieWinnerId, TEXT("Lexi Vale"), 12);
	AddPerson(Campaign, LongWatchTieLoserId, TEXT("Tomas Reed"), 12);
	AddPerson(Campaign, LowerMissionLongWatchId, TEXT("Earlier veteran"), 10);
	TArray<FGuid> LongWatchRoster = {
		LongWatchTieLoserId,
		FieldProvenId,
		FGuid(998, 1, 1, 1),
		LongWatchTieWinnerId,
		FirstWatchId,
		LowerMissionLongWatchId,
		FieldProvenId
	};
	const FPersonnelMentorshipView LongWatch = FPersonnelMentorship::Evaluate(Campaign, LongWatchRoster);
	TestTrue(TEXT("Long Watch mentor uses mission and lexical-id tie breaks and affects only lower bands"),
		LongWatch.bHasMentor
		&& LongWatch.bActive
		&& LongWatch.MentorId == LongWatchTieWinnerId
		&& LongWatch.MentorDisplayName == TEXT("Lexi Vale")
		&& LongWatch.MentorServiceHistory.Band == EPersonnelServiceBand::LongWatch
		&& LongWatch.MoraleBonus == 5
		&& LongWatch.RecipientCount == 2
		&& LongWatch.RecipientIds == TArray<FGuid>{ FirstWatchId, FieldProvenId });

	TArray<FGuid> ReversedLongWatchRoster;
	for (int32 Index = LongWatchRoster.Num() - 1; Index >= 0; --Index)
	{
		ReversedLongWatchRoster.Add(LongWatchRoster[Index]);
	}
	const FPersonnelMentorshipView ReorderedLongWatch =
		FPersonnelMentorship::Evaluate(Campaign, ReversedLongWatchRoster);
	TestTrue(TEXT("Input order, duplicate ids, and unresolved ids do not change guidance"),
		ReorderedLongWatch.MentorId == LongWatch.MentorId
		&& ReorderedLongWatch.MoraleBonus == LongWatch.MoraleBonus
		&& ReorderedLongWatch.RecipientIds == LongWatch.RecipientIds);

	const FGuid LegacyLowerMissionId(1, 1, 1, 1);
	const FGuid LegacyTieWinnerId(40, 1, 1, 1);
	const FGuid LegacyTieLoserId(50, 1, 1, 1);
	AddPerson(Campaign, LegacyLowerMissionId, TEXT("Legacy twenty"), 20);
	AddPerson(Campaign, LegacyTieWinnerId, TEXT("Mara Sol"), 25);
	AddPerson(Campaign, LegacyTieLoserId, TEXT("Iris Quill"), 25);
	TArray<FGuid> LegacyRoster = LongWatchRoster;
	LegacyRoster.Append({ LegacyTieLoserId, LegacyLowerMissionId, LegacyTieWinnerId });
	const FPersonnelMentorshipView Legacy = FPersonnelMentorship::Evaluate(Campaign, LegacyRoster);
	TestTrue(TEXT("Legacy Anchor supersedes Long Watch, grants ten, and never buffs equal-band peers"),
		Legacy.bHasMentor
		&& Legacy.bActive
		&& Legacy.MentorId == LegacyTieWinnerId
		&& Legacy.MentorServiceHistory.Band == EPersonnelServiceBand::LegacyAnchor
		&& Legacy.MoraleBonus == 10
		&& Legacy.RecipientCount == 5
		&& Legacy.RecipientIds.Contains(FirstWatchId)
		&& Legacy.RecipientIds.Contains(FieldProvenId)
		&& Legacy.RecipientIds.Contains(LongWatchTieWinnerId)
		&& Legacy.RecipientIds.Contains(LongWatchTieLoserId)
		&& Legacy.RecipientIds.Contains(LowerMissionLongWatchId)
		&& !Legacy.RecipientIds.Contains(LegacyLowerMissionId)
		&& !Legacy.RecipientIds.Contains(LegacyTieLoserId));

	const FGuid EnduringTieWinnerId(60, 1, 1, 1);
	const FGuid EnduringTieLoserId(70, 1, 1, 1);
	AddPerson(Campaign, EnduringTieWinnerId, TEXT("Asha North"), 45);
	AddPerson(Campaign, EnduringTieLoserId, TEXT("Bram Sato"), 45);
	TArray<FGuid> EnduringRoster = LegacyRoster;
	EnduringRoster.Append({ EnduringTieLoserId, EnduringTieWinnerId });
	const FPersonnelMentorshipView Enduring = FPersonnelMentorship::Evaluate(Campaign, EnduringRoster);
	TestTrue(TEXT("Enduring Beacon supersedes Legacy Anchor, grants fifteen, and never buffs equal-band peers"),
		Enduring.bHasMentor
		&& Enduring.bActive
		&& Enduring.MentorId == EnduringTieWinnerId
		&& Enduring.MentorServiceHistory.Band == EPersonnelServiceBand::EnduringBeacon
		&& Enduring.MoraleBonus == 15
		&& Enduring.RecipientCount == 8
		&& Enduring.RecipientIds.Contains(LegacyLowerMissionId)
		&& Enduring.RecipientIds.Contains(LegacyTieWinnerId)
		&& Enduring.RecipientIds.Contains(LegacyTieLoserId)
		&& !Enduring.RecipientIds.Contains(EnduringTieLoserId));

	const FPersonnelMentorshipView Dormant = FPersonnelMentorship::Evaluate(
		Campaign,
		{ LegacyTieLoserId, LegacyTieWinnerId, LegacyLowerMissionId });
	TestTrue(TEXT("A selected veteran remains visible when no lower-band teammate can receive guidance"),
		Dormant.bHasMentor
		&& !Dormant.bActive
		&& Dormant.MentorId == LegacyTieWinnerId
		&& Dormant.MoraleBonus == 10
		&& Dormant.RecipientCount == 0
		&& Dormant.RecipientIds.IsEmpty());
	return true;
}

#endif
