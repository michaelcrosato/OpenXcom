// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/PersonnelMentorship.h"

namespace PersonnelMentorshipPrivate
{
	bool GuidLess(const FGuid& Left, const FGuid& Right)
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
	}

	const FPersonnelState* FindPersonnel(const FCampaignState& Campaign, const FGuid& PersonnelId)
	{
		return Campaign.Personnel.FindByPredicate(
			[&PersonnelId](const FPersonnelState& Person)
			{
				return Person.PersonnelId == PersonnelId;
			});
	}

	int32 BandOrdinal(const EPersonnelServiceBand Band)
	{
		return static_cast<int32>(Band);
	}
}

FName FPersonnelMentorship::PolicyId()
{
	return TEXT("personnel.mentorship-watchkeeper");
}

FPersonnelMentorshipView FPersonnelMentorship::Evaluate(
	const FCampaignState& Campaign,
	const TArray<FGuid>& PersonnelIds)
{
	using namespace PersonnelMentorshipPrivate;

	FPersonnelMentorshipView View;
	View.PolicyId = PolicyId();

	TArray<FGuid> StablePersonnelIds;
	TSet<FGuid> SeenPersonnelIds;
	StablePersonnelIds.Reserve(PersonnelIds.Num());
	SeenPersonnelIds.Reserve(PersonnelIds.Num());
	for (const FGuid& PersonnelId : PersonnelIds)
	{
		if (PersonnelId.IsValid() && !SeenPersonnelIds.Contains(PersonnelId))
		{
			SeenPersonnelIds.Add(PersonnelId);
			StablePersonnelIds.Add(PersonnelId);
		}
	}
	StablePersonnelIds.Sort(
		[](const FGuid& Left, const FGuid& Right)
		{
			return GuidLess(Left, Right);
		});

	const FPersonnelState* Mentor = nullptr;
	FPersonnelServiceHistoryView MentorHistory;
	int32 MentorMissions = 0;
	for (const FGuid& PersonnelId : StablePersonnelIds)
	{
		const FPersonnelState* Person = FindPersonnel(Campaign, PersonnelId);
		if (Person == nullptr)
		{
			continue;
		}
		const int32 SafeMissions = FMath::Max(0, Person->Missions);
		const FPersonnelServiceHistoryView ServiceHistory = FPersonnelServiceHistory::Project(SafeMissions);
		if (BandOrdinal(ServiceHistory.Band) < BandOrdinal(EPersonnelServiceBand::LongWatch))
		{
			continue;
		}
		const bool bHigherBand = Mentor == nullptr
			|| BandOrdinal(ServiceHistory.Band) > BandOrdinal(MentorHistory.Band);
		const bool bMoreMissions = Mentor != nullptr
			&& ServiceHistory.Band == MentorHistory.Band
			&& SafeMissions > MentorMissions;
		const bool bLowerStableId = Mentor != nullptr
			&& ServiceHistory.Band == MentorHistory.Band
			&& SafeMissions == MentorMissions
			&& GuidLess(Person->PersonnelId, Mentor->PersonnelId);
		if (bHigherBand || bMoreMissions || bLowerStableId)
		{
			Mentor = Person;
			MentorHistory = ServiceHistory;
			MentorMissions = SafeMissions;
		}
	}

	if (Mentor == nullptr)
	{
		return View;
	}

	View.bHasMentor = true;
	View.MentorId = Mentor->PersonnelId;
	View.MentorDisplayName = Mentor->DisplayName;
	View.MentorServiceHistory = MentorHistory;
	View.MoraleBonus = MentorHistory.Band == EPersonnelServiceBand::LegacyAnchor ? 10 : 5;
	for (const FGuid& PersonnelId : StablePersonnelIds)
	{
		if (PersonnelId == Mentor->PersonnelId)
		{
			continue;
		}
		const FPersonnelState* Person = FindPersonnel(Campaign, PersonnelId);
		if (Person != nullptr
			&& BandOrdinal(FPersonnelServiceHistory::Project(Person->Missions).Band)
				< BandOrdinal(MentorHistory.Band))
		{
			View.RecipientIds.Add(PersonnelId);
		}
	}
	View.RecipientCount = View.RecipientIds.Num();
	View.bActive = View.RecipientCount > 0;
	return View;
}
