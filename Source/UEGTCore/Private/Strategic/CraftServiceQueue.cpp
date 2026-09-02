// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/CraftServiceQueue.h"

namespace CraftServiceQueuePrivate
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

	int32 CountOperationalServiceLanes(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules)
	{
		int32 LaneCount = 0;
		if (!Base.Facilities.IsEmpty())
		{
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
				if (Rule != nullptr && Rule->CraftCapacity > 0 && Rule->MaxIntegrity > 0
					&& Facility.Damage >= 0 && Facility.Damage <= Rule->MaxIntegrity
					&& Rule->ScaleEffectByIntegrity(Rule->CraftCapacity, Facility.Damage) > 0
					&& LaneCount < MAX_int32)
				{
					++LaneCount;
				}
			}
			return LaneCount;
		}

		for (const FName FacilityId : Base.BuiltFacilities)
		{
			const FFacilityRule* Rule = Rules.Facilities.Find(FacilityId);
			if (Rule != nullptr && Rule->CraftCapacity > 0 && LaneCount < MAX_int32)
			{
				++LaneCount;
			}
		}
		return LaneCount;
	}

	int64 SaturatingAdd(const int64 Left, const int64 Right)
	{
		return Left > MAX_int64 - Right ? MAX_int64 : Left + Right;
	}

	struct FServiceJob
	{
		FGuid CraftId;
		int64 DurationSeconds = 0;
	};
}

const FCraftServiceQueueView* FCraftServiceQueueSnapshot::FindCraft(const FGuid& CraftId) const
{
	return Craft.FindByPredicate(
		[&CraftId](const FCraftServiceQueueView& View)
		{
			return View.CraftId == CraftId;
		});
}

FName FCraftServiceQueue::PolicyId()
{
	return TEXT("craft.service-rapid-turnaround");
}

FCraftServiceQueueSnapshot FCraftServiceQueue::Evaluate(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules)
{
	using namespace CraftServiceQueuePrivate;

	FCraftServiceQueueSnapshot Snapshot;
	Snapshot.PolicyId = PolicyId();

	TArray<const FStrategicBaseState*> StableBases;
	StableBases.Reserve(Campaign.Bases.Num());
	for (const FStrategicBaseState& Base : Campaign.Bases)
	{
		if (Base.BaseId.IsValid())
		{
			StableBases.Add(&Base);
		}
	}
	StableBases.Sort(
		[](const FStrategicBaseState& Left, const FStrategicBaseState& Right)
		{
			return GuidLess(Left.BaseId, Right.BaseId);
		});

	for (const FStrategicBaseState* Base : StableBases)
	{
		const int32 LaneCount = CountOperationalServiceLanes(*Base, Rules);
		if (LaneCount <= 0)
		{
			continue;
		}

		TArray<FServiceJob> Jobs;
		for (const FCraftState& Craft : Campaign.Craft)
		{
			if (Craft.BaseId != Base->BaseId || !Craft.CraftId.IsValid()
				|| Craft.Status != ECraftStatus::Servicing)
			{
				continue;
			}
			const int64 DurationSeconds = FMath::Max(
				FMath::Max<int64>(0, Craft.RemainingRepairSeconds),
				FMath::Max<int64>(0, Craft.RemainingRefuelSeconds));
			if (DurationSeconds > 0)
			{
				Jobs.Add({ Craft.CraftId, DurationSeconds });
			}
		}
		Jobs.Sort(
			[](const FServiceJob& Left, const FServiceJob& Right)
			{
				return Left.DurationSeconds == Right.DurationSeconds
					? GuidLess(Left.CraftId, Right.CraftId)
					: Left.DurationSeconds < Right.DurationSeconds;
			});
		if (Jobs.IsEmpty())
		{
			continue;
		}

		const int32 ActiveCount = FMath::Min(LaneCount, Jobs.Num());
		TArray<int64> LaneAvailableSeconds;
		LaneAvailableSeconds.Init(0, ActiveCount);
		for (int32 JobIndex = 0; JobIndex < Jobs.Num(); ++JobIndex)
		{
			int32 LaneIndex = 0;
			for (int32 CandidateLane = 1; CandidateLane < LaneAvailableSeconds.Num(); ++CandidateLane)
			{
				if (LaneAvailableSeconds[CandidateLane] < LaneAvailableSeconds[LaneIndex])
				{
					LaneIndex = CandidateLane;
				}
			}

			const int64 WaitSeconds = LaneAvailableSeconds[LaneIndex];
			const int64 ReadySeconds = SaturatingAdd(WaitSeconds, Jobs[JobIndex].DurationSeconds);
			LaneAvailableSeconds[LaneIndex] = ReadySeconds;

			FCraftServiceQueueView& View = Snapshot.Craft.AddDefaulted_GetRef();
			View.bValid = true;
			View.PolicyId = Snapshot.PolicyId;
			View.BaseId = Base->BaseId;
			View.CraftId = Jobs[JobIndex].CraftId;
			View.ServiceLaneCount = LaneCount;
			View.ActiveServiceCraftCount = ActiveCount;
			View.TotalServiceCraftCount = Jobs.Num();
			View.QueuePosition = JobIndex + 1;
			View.bInServiceLane = WaitSeconds == 0;
			View.WaitingPosition = View.bInServiceLane ? 0 : JobIndex - ActiveCount + 1;
			View.ServiceLaneNumber = LaneIndex + 1;
			View.EstimatedWaitSeconds = WaitSeconds;
			View.EstimatedReadySeconds = ReadySeconds;
		}
	}

	return Snapshot;
}
