// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/PersonnelSquadBond.h"

namespace PersonnelSquadBondPrivate
{
	FString GuidKey(const FGuid& Id)
	{
		return Id.ToString(EGuidFormats::Digits);
	}

	const FPersonnelState* FindPersonnel(const FCampaignState& Campaign, const FGuid& PersonnelId)
	{
		return Campaign.Personnel.FindByPredicate(
			[&PersonnelId](const FPersonnelState& Person)
			{
				return Person.PersonnelId == PersonnelId;
			});
	}

	FPersonnelSquadBondPairView BuildPairView(
		const FCampaignState& Campaign,
		const FPersonnelSquadBondState& Bond)
	{
		FPersonnelSquadBondPairView View;
		View.FirstPersonnelId = Bond.FirstPersonnelId;
		View.SecondPersonnelId = Bond.SecondPersonnelId;
		View.SharedVictories = Bond.SharedVictories;
		View.Tier = FPersonnelSquadBond::GetTier(Bond.SharedVictories);
		View.NextTierVictories = FPersonnelSquadBond::GetNextTierVictories(View.Tier);
		View.ActionPointBonus = FPersonnelSquadBond::GetActionPointBonus(View.Tier);
		View.MoraleBonus = FPersonnelSquadBond::GetMoraleBonus(View.Tier);
		if (const FPersonnelState* First = FindPersonnel(Campaign, Bond.FirstPersonnelId))
		{
			View.FirstDisplayName = First->DisplayName;
		}
		if (const FPersonnelState* Second = FindPersonnel(Campaign, Bond.SecondPersonnelId))
		{
			View.SecondDisplayName = Second->DisplayName;
		}
		return View;
	}

	bool PairPriorityLess(
		const FPersonnelSquadBondPairView& Left,
		const FPersonnelSquadBondPairView& Right)
	{
		if (Left.Tier != Right.Tier)
		{
			return static_cast<uint8>(Left.Tier) > static_cast<uint8>(Right.Tier);
		}
		if (Left.SharedVictories != Right.SharedVictories)
		{
			return Left.SharedVictories > Right.SharedVictories;
		}
		const FString LeftFirst = GuidKey(Left.FirstPersonnelId);
		const FString RightFirst = GuidKey(Right.FirstPersonnelId);
		if (LeftFirst != RightFirst)
		{
			return LeftFirst < RightFirst;
		}
		return GuidKey(Left.SecondPersonnelId) < GuidKey(Right.SecondPersonnelId);
	}
}

EPersonnelSquadBondTier FPersonnelSquadBond::GetTier(const int32 SharedVictories)
{
	if (SharedVictories >= UnbrokenVictories)
	{
		return EPersonnelSquadBondTier::Unbroken;
	}
	if (SharedVictories >= InterlockedVictories)
	{
		return EPersonnelSquadBondTier::Interlocked;
	}
	if (SharedVictories >= AlignedVictories)
	{
		return EPersonnelSquadBondTier::Aligned;
	}
	return EPersonnelSquadBondTier::None;
}

int32 FPersonnelSquadBond::GetNextTierVictories(const EPersonnelSquadBondTier Tier)
{
	switch (Tier)
	{
	case EPersonnelSquadBondTier::None:
		return AlignedVictories;
	case EPersonnelSquadBondTier::Aligned:
		return InterlockedVictories;
	case EPersonnelSquadBondTier::Interlocked:
		return UnbrokenVictories;
	case EPersonnelSquadBondTier::Unbroken:
	default:
		return 0;
	}
}

int32 FPersonnelSquadBond::GetActionPointBonus(const EPersonnelSquadBondTier Tier)
{
	return Tier == EPersonnelSquadBondTier::Unbroken
		? 2
		: (Tier == EPersonnelSquadBondTier::Aligned || Tier == EPersonnelSquadBondTier::Interlocked ? 1 : 0);
}

int32 FPersonnelSquadBond::GetMoraleBonus(const EPersonnelSquadBondTier Tier)
{
	return Tier == EPersonnelSquadBondTier::Interlocked || Tier == EPersonnelSquadBondTier::Unbroken ? 5 : 0;
}

FPersonnelSquadBondView FPersonnelSquadBond::Evaluate(
	const FCampaignState& Campaign,
	const TArray<FGuid>& PersonnelIds)
{
	using namespace PersonnelSquadBondPrivate;

	FPersonnelSquadBondView View;
	View.PolicyId = FName(TEXT("personnel.squad-bond-field-cadence"));

	TArray<FGuid> ResolvedIds;
	TSet<FGuid> SeenIds;
	for (const FGuid& PersonnelId : PersonnelIds)
	{
		if (!PersonnelId.IsValid() || SeenIds.Contains(PersonnelId)
			|| FindPersonnel(Campaign, PersonnelId) == nullptr)
		{
			continue;
		}
		SeenIds.Add(PersonnelId);
		ResolvedIds.Add(PersonnelId);
	}
	ResolvedIds.Sort(
		[](const FGuid& Left, const FGuid& Right)
		{
			return GuidKey(Left) < GuidKey(Right);
		});
	View.ResolvedPersonnelCount = ResolvedIds.Num();
	if (ResolvedIds.Num() < 2)
	{
		return View;
	}

	TSet<FGuid> ResolvedSet;
	ResolvedSet.Reserve(ResolvedIds.Num());
	for (const FGuid& PersonnelId : ResolvedIds)
	{
		ResolvedSet.Add(PersonnelId);
	}

	TArray<FPersonnelSquadBondPairView> EligiblePairs;
	for (const FPersonnelSquadBondState& Bond : Campaign.PersonnelSquadBonds)
	{
		if (!Bond.FirstPersonnelId.IsValid() || !Bond.SecondPersonnelId.IsValid()
			|| Bond.FirstPersonnelId == Bond.SecondPersonnelId || Bond.SharedVictories <= 0
			|| !ResolvedSet.Contains(Bond.FirstPersonnelId)
			|| !ResolvedSet.Contains(Bond.SecondPersonnelId)
			|| GuidKey(Bond.SecondPersonnelId) <= GuidKey(Bond.FirstPersonnelId))
		{
			continue;
		}
		FPersonnelSquadBondPairView Pair = BuildPairView(Campaign, Bond);
		if (Pair.FirstDisplayName.IsEmpty() || Pair.SecondDisplayName.IsEmpty())
		{
			continue;
		}
		if (Pair.Tier == EPersonnelSquadBondTier::None)
		{
			View.DevelopingPairs.Add(MoveTemp(Pair));
		}
		else
		{
			EligiblePairs.Add(MoveTemp(Pair));
		}
	}

	EligiblePairs.Sort(PairPriorityLess);
	View.DevelopingPairs.Sort(PairPriorityLess);
	View.EligiblePairCount = EligiblePairs.Num();
	TSet<FGuid> PairedPersonnelIds;
	for (FPersonnelSquadBondPairView& Pair : EligiblePairs)
	{
		if (PairedPersonnelIds.Contains(Pair.FirstPersonnelId)
			|| PairedPersonnelIds.Contains(Pair.SecondPersonnelId))
		{
			continue;
		}
		PairedPersonnelIds.Add(Pair.FirstPersonnelId);
		PairedPersonnelIds.Add(Pair.SecondPersonnelId);
		View.ActivePairs.Add(MoveTemp(Pair));
	}
	View.bActive = !View.ActivePairs.IsEmpty();
	return View;
}
