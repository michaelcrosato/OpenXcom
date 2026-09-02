// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/PersonnelLegacyRelay.h"

namespace PersonnelLegacyRelayPrivate
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

	int32 SharedBonus(const int32 AuthoredBonus)
	{
		return FMath::DivideAndRoundUp(FMath::Max(0, AuthoredBonus), 2);
	}

	struct FDoctrineProjection
	{
		const FPersonnelDoctrineRule* Rule = nullptr;
		int32 AccuracyBonus = 0;
		int32 ResolveBonus = 0;
		int32 MobilityBonus = 0;
		int32 StrengthBonus = 0;
		int32 TotalBonus = 0;
	};

	FDoctrineProjection SelectDoctrine(
		const FPersonnelState& Person,
		const FResolvedRuleSet& Rules,
		const TArray<FName>& StableDoctrineIds)
	{
		TMap<FName, int32> SelectionCounts;
		for (const FName DoctrineId : Person.DoctrineSelections)
		{
			++SelectionCounts.FindOrAdd(DoctrineId);
		}

		FDoctrineProjection Best;
		for (const FName DoctrineId : StableDoctrineIds)
		{
			const FPersonnelDoctrineRule& Rule = Rules.PersonnelDoctrines.FindChecked(DoctrineId);
			const int32 SelectionCount = SelectionCounts.FindRef(DoctrineId);
			if (Rule.MaxSelections <= 0 || SelectionCount < Rule.MaxSelections)
			{
				continue;
			}

			FDoctrineProjection Candidate;
			Candidate.Rule = &Rule;
			Candidate.AccuracyBonus = SharedBonus(Rule.AccuracyBonus);
			Candidate.ResolveBonus = SharedBonus(Rule.ResolveBonus);
			Candidate.MobilityBonus = SharedBonus(Rule.MobilityBonus);
			Candidate.StrengthBonus = SharedBonus(Rule.StrengthBonus);
			Candidate.TotalBonus = Candidate.AccuracyBonus + Candidate.ResolveBonus
				+ Candidate.MobilityBonus + Candidate.StrengthBonus;
			if (Candidate.TotalBonus <= 0)
			{
				continue;
			}

			if (Best.Rule == nullptr || Candidate.TotalBonus > Best.TotalBonus)
			{
				Best = Candidate;
			}
		}
		return Best;
	}
}

FName FPersonnelLegacyRelay::PolicyId()
{
	return TEXT("personnel.specialization-legacy-relay");
}

FPersonnelLegacyRelayView FPersonnelLegacyRelay::Evaluate(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const TArray<FGuid>& PersonnelIds)
{
	using namespace PersonnelLegacyRelayPrivate;

	FPersonnelLegacyRelayView View;
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

	TArray<FName> StableDoctrineIds;
	Rules.PersonnelDoctrines.GenerateKeyArray(StableDoctrineIds);
	StableDoctrineIds.Sort(FNameLexicalLess());

	const FPersonnelState* Specialist = nullptr;
	FPersonnelServiceHistoryView SpecialistHistory;
	FDoctrineProjection SpecialistDoctrine;
	int32 SpecialistMissions = 0;
	for (const FGuid& PersonnelId : StablePersonnelIds)
	{
		const FPersonnelState* Person = FindPersonnel(Campaign, PersonnelId);
		if (Person == nullptr)
		{
			continue;
		}
		const int32 SafeMissions = FMath::Max(0, Person->Missions);
		const FPersonnelServiceHistoryView ServiceHistory = FPersonnelServiceHistory::Project(SafeMissions);
		if (static_cast<int32>(ServiceHistory.Band)
			< static_cast<int32>(EPersonnelServiceBand::LegacyAnchor))
		{
			continue;
		}
		const FDoctrineProjection Doctrine = SelectDoctrine(*Person, Rules, StableDoctrineIds);
		if (Doctrine.Rule == nullptr)
		{
			continue;
		}

		const bool bMoreMissions = Specialist == nullptr || SafeMissions > SpecialistMissions;
		const bool bLowerStableId = Specialist != nullptr && SafeMissions == SpecialistMissions
			&& GuidLess(Person->PersonnelId, Specialist->PersonnelId);
		if (bMoreMissions || bLowerStableId)
		{
			Specialist = Person;
			SpecialistHistory = ServiceHistory;
			SpecialistDoctrine = Doctrine;
			SpecialistMissions = SafeMissions;
		}
	}

	if (Specialist == nullptr || SpecialistDoctrine.Rule == nullptr)
	{
		return View;
	}

	View.bHasSpecialist = true;
	View.SpecialistId = Specialist->PersonnelId;
	View.SpecialistDisplayName = Specialist->DisplayName;
	View.SpecialistServiceHistory = SpecialistHistory;
	View.DoctrineId = SpecialistDoctrine.Rule->Identity.RuleId;
	View.DoctrineDisplayName = SpecialistDoctrine.Rule->DisplayName.IsEmpty()
		? View.DoctrineId.ToString()
		: SpecialistDoctrine.Rule->DisplayName;
	View.AccuracyBonus = SpecialistDoctrine.AccuracyBonus;
	View.ResolveBonus = SpecialistDoctrine.ResolveBonus;
	View.MobilityBonus = SpecialistDoctrine.MobilityBonus;
	View.StrengthBonus = SpecialistDoctrine.StrengthBonus;
	for (const FGuid& PersonnelId : StablePersonnelIds)
	{
		if (PersonnelId != Specialist->PersonnelId && FindPersonnel(Campaign, PersonnelId) != nullptr)
		{
			View.RecipientIds.Add(PersonnelId);
		}
	}
	View.RecipientCount = View.RecipientIds.Num();
	View.bActive = View.RecipientCount > 0;
	return View;
}
